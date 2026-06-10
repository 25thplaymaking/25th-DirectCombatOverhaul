// Procedural tactical pathing - "smart maneuver" pathing re-decided as the group moves, instead of
// pathing straight to the destination. While a group is moving under a danger state (perceived threat),
// every few seconds it samples a forward cone of candidate next-legs toward the goal, scores each by
// concealment (is it hidden from the threat - raycast), cover, and progress toward the destination
// (path of least resistance), snaps the winner onto the navmesh (walkable ground) and bounds to it,
// then re-assesses at the next tick and adjusts - so the path procedurally bends through cover. The end
// goal is always the original destination; the route to it is what adapts.
//
// With debug draw on, the live path is drawn as arrows - the current leg (where they're moving now)
// plus the intended remaining legs - refreshed each reassessment so you can watch the path alter. Debug
// shapes are host-side (a dedicated server draws nothing to clients); this is a tuning aid.
//
// Another `modded SCR_AIGroupUtilityComponent` fragment. Folder "Movement" sorts before "QRF", so
// DCO_UpdateTacticalPath is defined before the shared EvaluateActivity (DCO_GroupQRF.c) that calls it
// (Enforce resolves cross-fragment calls in file order). Server-authoritative. Default OFF
// (DCO_TacticalMoveSettings.m_bEnableProceduralPath) so nothing changes on a live server until enabled.
// The base-of-fire split is separate.
modded class SCR_AIGroupUtilityComponent
{
	// Procedural path state.
	protected vector			m_vDCO_PathDest;					// the fixed end goal
	protected bool				m_bDCO_HasPathDest		= false;
	protected float				m_fDCO_LastPathReassess	= -1;
	protected vector			m_vDCO_PathNextLeg;					// the leg we are currently bounding to
	protected bool				m_bDCO_HasNextLeg		= false;
	protected ref array<vector>	m_aDCO_PathPreview;					// intended remaining legs (for debug draw)
	protected ref array<ref Shape>	m_aDCO_PathShapes;				// held debug visuals (freed when cleared)
	protected bool				m_bDCO_BoundFlip		= false;	// leapfrog toggle: alternates which element bounds vs overwatches
	protected ref array<vector>	m_aDCO_BaseElemPositions;			// base-of-fire / overwatch member positions (debug)
	protected int				m_iDCO_MoveTechnique	= 0;		// 0 traveling, 1 traveling overwatch, 2 bounding overwatch (debug/telemetry)

	protected static const float DCO_DEG2RAD = 0.0174533;

	// Set by the tactical-move hook when a group is given a long move order and procedural pathing is on.
	// Records the end goal; the per-tick reassessment then routes to it leg by leg.
	void DCO_SetPathDestination(vector dest)
	{
		m_vDCO_PathDest		= dest;
		m_bDCO_HasPathDest	= true;
		m_fDCO_LastPathReassess = -1;	// force an immediate reassessment on the next tick
		m_bDCO_HasNextLeg	= false;
	}

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Re-decides the route through cover
	// while the group is moving under a danger state, and refreshes the debug path.
	void DCO_UpdateTacticalPath()
	{
		if (!Replication.IsServer())
			return;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (!cfg || !cfg.m_bEnableProceduralPath || !m_bDCO_HasPathDest || !m_Owner)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastPathReassess >= 0 && (now - m_fDCO_LastPathReassess) < cfg.m_fPathReassessSec * 1000.0)
			return;
		m_fDCO_LastPathReassess = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
		{
			DCO_ClearPath();
			return;
		}
		vector pos = leader.GetOrigin();

		// Arrived at the end goal: done.
		if (vector.DistanceSq(pos, m_vDCO_PathDest) < cfg.m_fPathArriveDist * cfg.m_fPathArriveDist)
		{
			DCO_ClearPath();
			return;
		}

		// Contact takes precedence over the move. If the group perceives an enemy within decisive range, halt the
		// tactical advance and fight from here - pushing on under fire gets the element (and especially the
		// leader) killed on an unwinnable path and saps morale. The contact drill / suppression / cover systems
		// run the fight; the route resumes once contact is broken. Overrides the move no matter how far the goal.
		if (cfg.m_bPathHaltOnContact && m_Perception)
		{
			array<IEntity> seen = m_Perception.m_aTargetEntities;
			if (seen && !seen.IsEmpty())
			{
				float haltSq = cfg.m_fPathContactHaltRange * cfg.m_fPathContactHaltRange;
				foreach (IEntity t : seen)
				{
					if (!t)
						continue;
					if (vector.DistanceSq(t.GetOrigin(), pos) <= haltSq)
					{
						m_bDCO_HasNextLeg = false;	// stop issuing advance bounds
						DCO_ClearPathVisualsOnly();
						DCO_Debug.LogGroup("PATH", leader, "in contact - halting tactical advance (fight from here, preserve the element)");
						return;
					}
				}
			}
		}

		// Danger gate: only procedurally re-route through cover when the group is actually under threat;
		// otherwise let the normal path run (routine movement stays direct/fast). Tethered to danger state.
		float threat = GetThreatMeasure();
		vector threatPos;
		bool hasThreat = DCO_GetThreatOrLastPosition(threatPos);
		if (threat < cfg.m_fPathThreatActivation || !hasThreat)
		{
			// No danger: clear any cover-detour visuals; the group proceeds to the goal normally.
			DCO_ClearPathVisualsOnly();
			return;
		}

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));

		// Build the intended path: greedily pick the best next leg through cover, repeating toward the goal.
		DCO_BuildProceduralPath(pos, threatPos, cfg, pf);

		// Movement technique by danger band (FM 3-90 2-18/19/20): traveling (whole unit) when contact is light,
		// traveling overwatch (lead + trailing element) when contact is possible, bounding overwatch (base of
		// fire + leapfrogging maneuver element) when contact is heavy/expected. On-foot only for the split forms.
		int tech = 0;	// traveling
		if (threat >= cfg.m_fPathSplitThreatActivation)
			tech = 2;	// bounding overwatch
		else if (threat >= cfg.m_fPathTravelOverwatchThreat)
			tech = 1;	// traveling overwatch
		m_iDCO_MoveTechnique = tech;

		bool mounted = DCO_VehicleUtil.IsGroupInVehicle(m_Owner);
		bool handled = false;
		if (tech == 2 && cfg.m_bEnableBaseOfFireSplit && !mounted)
			handled = DCO_TryBoundingOverwatch(cfg, threatPos, pf);
		else if (tech == 1 && cfg.m_bEnableTravelingOverwatch && !mounted)
			handled = DCO_TryTravelingOverwatch(cfg);

		if (!handled && m_bDCO_HasNextLeg)
		{
			if (m_aDCO_BaseElemPositions)
				m_aDCO_BaseElemPositions.Clear();	// no separate element this tick
			// Traveling: whole unit moves together. Vehicle-aware helper keeps a mounted crew driving.
			DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, m_vDCO_PathNextLeg, m_Mailbox);
		}

		// Debug: redraw the live path so the adjustment is visible.
		if (cfg.m_bPathDebugDraw || DCO_MoraleSettings.Get().m_bDebug)
			DCO_DrawPath(pos);
		else
			DCO_ClearPathVisualsOnly();

		int legCount = 0;	// Enforce has no ternary operator - precompute for the log.
		if (m_aDCO_PathPreview)
			legCount = m_aDCO_PathPreview.Count();
		DCO_Debug.LogGroup("PATH", leader, string.Format("reassess: threat=%1 legs=%2 next=%3", threat, legCount, m_vDCO_PathNextLeg));
	}

	// Greedy cover-biased planner: from `pos`, repeatedly choose the best next leg toward the goal that is
	// most concealed from the threat while still making progress, snapping each onto the navmesh. Fills
	// m_vDCO_PathNextLeg (first leg, what we move to) and m_aDCO_PathPreview (the full intended route).
	protected void DCO_BuildProceduralPath(vector startPos, vector threatPos, DCO_TacticalMoveSettings cfg, AIPathfindingComponent pf)
	{
		if (!m_aDCO_PathPreview)
			m_aDCO_PathPreview = {};
		m_aDCO_PathPreview.Clear();

		vector cur = startPos;
		m_bDCO_HasNextLeg = false;

		for (int leg = 0; leg < cfg.m_iPathMaxLegs; leg++)
		{
			// Close enough to commit straight to the goal.
			if (vector.DistanceSq(cur, m_vDCO_PathDest) <= cfg.m_fPathLegLength * cfg.m_fPathLegLength)
			{
				m_aDCO_PathPreview.Insert(m_vDCO_PathDest);
				break;
			}

			vector best;
			if (!DCO_PickBestLeg(cur, threatPos, cfg, pf, best))
			{
				// No good cover candidate - step straight toward the goal so we don't stall.
				vector dir = m_vDCO_PathDest - cur;
				dir[1] = 0;
				dir.Normalize();
				best = cur + dir * cfg.m_fPathLegLength;
				DCO_SnapToNavmesh(pf, best, best);
			}

			m_aDCO_PathPreview.Insert(best);
			cur = best;
		}

		if (!m_aDCO_PathPreview.IsEmpty())
		{
			m_vDCO_PathNextLeg = m_aDCO_PathPreview[0];
			m_bDCO_HasNextLeg = true;
		}
	}

	// Sample a forward cone toward the goal and return the leg endpoint that best balances concealment
	// from the threat with progress toward the goal. False if nothing usable was found.
	protected bool DCO_PickBestLeg(vector cur, vector threatPos, DCO_TacticalMoveSettings cfg, AIPathfindingComponent pf, out vector best)
	{
		vector toGoal = m_vDCO_PathDest - cur;
		toGoal[1] = 0;
		float goalDist = toGoal.Length();
		if (goalDist < 1.0)
			return false;
		vector fwd = toGoal / goalDist;

		float bestScore = -99999.0;
		bool found = false;

		int samples = cfg.m_iPathConeSamples;
		if (samples < 1)
			samples = 1;

		for (int i = 0; i < samples; i++)
		{
			// Spread samples evenly across [-cone, +cone] around the bearing to the goal.
			float t = 0.0;
			if (samples > 1)
				t = (i / (float)(samples - 1)) * 2.0 - 1.0;	// -1..+1
			float ang = t * cfg.m_fPathConeHalfAngleDeg * DCO_DEG2RAD;
			float s = Math.Sin(ang);
			float c = Math.Cos(ang);

			vector dir = Vector(fwd[0] * c - fwd[2] * s, 0, fwd[0] * s + fwd[2] * c);
			vector cand = cur + dir * cfg.m_fPathLegLength;
			DCO_SnapToNavmesh(pf, cand, cand);

			// Don't route a leg up onto an upper floor / attic - keep the bound on the current level.
			if (cand[1] > cur[1] + cfg.m_fCoverMaxClimb)
				continue;

			// Concealment: is the candidate hidden from the threat? (raycast threat to candidate blocked = good)
			bool concealed = DCO_IsConcealedFromThreat(pf, threatPos, cand, cfg);

			// Progress: how much closer to the goal this candidate gets us (path of least resistance bias).
			float progress = goalDist - vector.Distance(cand, m_vDCO_PathDest);

			// Score: progress, with a strong concealment bonus and a mild penalty for veering off-axis.
			float score = progress;
			if (concealed)
				score += cfg.m_fPathConcealmentBonus;
			score -= Math.AbsFloat(t) * cfg.m_fPathStraightBias;

			if (score > bestScore)
			{
				bestScore = score;
				best = cand;
				found = true;
			}
		}

		return found;
	}

	// True if world geometry blocks the line from the threat to pos (pos is concealed). Uses the AI
	// pathfinding ray trace when available; degrades to "not concealed" (safe) if there's no component.
	protected bool DCO_IsConcealedFromThreat(AIPathfindingComponent pf, vector threatPos, vector pos, DCO_TacticalMoveSettings cfg)
	{
		if (!pf)
			return false;

		vector eye = Vector(0, 1.5, 0);
		vector hit;
		// RayTrace returns true when the segment hits geometry before reaching the end (line is blocked).
		return pf.RayTrace(threatPos + eye, pos + eye, hit);
	}

	// Snap a candidate point onto the navmesh so legs land on walkable ground. Leaves the point unchanged
	// if there's no pathfinding component or no navmesh hit (still a usable approximate target).
	protected void DCO_SnapToNavmesh(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;

		vector corrected;
		// Vertically tight (±2 m) so a leg snaps to the current level, not an upper storey/attic.
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(6, 2, 6), corrected))
			outPos = corrected;
	}

	// Traveling overwatch: a lead element advances to the next leg while a trailing element follows a short
	// distance back (toward the leader's current position) at a walk, ready to overwatch/support. On-foot.
	// Returns true if issued. Per-member targeting reuses the verified DCO_FlankSplit message path.
	protected bool DCO_TryTravelingOverwatch(DCO_TacticalMoveSettings cfg)
	{
		if (!m_FireteamMgr || !m_Mailbox || !m_bDCO_HasNextLeg)
			return false;

		array<ref SCR_AIGroupFireteam> fireteams = {};
		DCO_FireteamCompat.GetAllFireteams(m_FireteamMgr, m_Owner, fireteams);
		if (fireteams.Count() < 2)
			return false;

		SCR_AIGroupFireteam ftLead;
		SCR_AIGroupFireteam ftTrail;
		foreach (SCR_AIGroupFireteam ft : fireteams)
		{
			if (!ft || ft.GetMemberCount() <= 0)
				continue;
			if (!ftLead)
				ftLead = ft;
			else
			{
				ftTrail = ft;
				break;
			}
		}
		if (!ftLead || !ftTrail)
			return false;

		AICommunicationComponent comms = m_Mailbox;

		// Lead element advances to the next path leg.
		array<AIAgent> leadMembers = {};
		ftLead.GetMembers(leadMembers);
		foreach (AIAgent lm : leadMembers)
		{
			if (!lm)
				continue;
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, m_vDCO_PathNextLeg, EMovementType.RUN, false, null);
			if (!msg)
				continue;
			msg.SetReceiver(lm);
			comms.RequestBroadcast(msg, lm);
		}

		// Trailing element follows toward the leader's current position (a short bound behind), at a walk.
		IEntity leader = m_Owner.GetLeaderEntity();
		vector trailPos = m_vDCO_PathNextLeg;	// no ternary in Enforce
		if (leader)
			trailPos = leader.GetOrigin();

		array<AIAgent> trailMembers = {};
		ftTrail.GetMembers(trailMembers);
		foreach (AIAgent tm : trailMembers)
		{
			if (!tm)
				continue;
			SCR_AIMessage_Move msg2 = SCR_AIMessage_Move.Create(null, trailPos, EMovementType.WALK, false, null);
			if (!msg2)
				continue;
			msg2.SetReceiver(tm);
			comms.RequestBroadcast(msg2, tm);
		}

		if (m_aDCO_BaseElemPositions)
			m_aDCO_BaseElemPositions.Clear();	// trailing element is moving, not a static base

		return true;
	}

	// Bounding overwatch: split the group into a base-of-fire element (holds + overwatches) and a maneuver
	// element (bounds to the next leg), alternating roles each call (leapfrog). On-foot infantry only.
	// Returns true if a split was issued. Per-member targeting reuses the verified DCO_FlankSplit message
	// path. Validate in-engine: whether RequestBroadcast(msg, member) isolates a single member.
	protected bool DCO_TryBoundingOverwatch(DCO_TacticalMoveSettings cfg, vector threatPos, AIPathfindingComponent pf)
	{
		if (!m_FireteamMgr || !m_Mailbox || !m_bDCO_HasNextLeg)
			return false;

		array<ref SCR_AIGroupFireteam> fireteams = {};
		DCO_FireteamCompat.GetAllFireteams(m_FireteamMgr, m_Owner, fireteams);
		if (fireteams.Count() < 2)
			return false;	// need at least two elements to split

		// First two non-empty fireteams.
		SCR_AIGroupFireteam ftA;
		SCR_AIGroupFireteam ftB;
		foreach (SCR_AIGroupFireteam ft : fireteams)
		{
			if (!ft || ft.GetMemberCount() <= 0)
				continue;
			if (!ftA)
				ftA = ft;
			else
			{
				ftB = ft;
				break;
			}
		}
		if (!ftA || !ftB)
			return false;

		// Assign maneuver vs base. By default the leader's element stays as the static base of fire so the
		// leader is never the one bounding into the kill zone (leader losses sap morale hardest). Only leapfrog
		// when leader-protection is off.
		SCR_AIGroupFireteam leaderFt;
		AIAgent leaderAgent = m_Owner.GetLeaderAgent();
		if (leaderAgent)
			leaderFt = m_FireteamMgr.FindFireteam(leaderAgent);

		SCR_AIGroupFireteam maneuverFt;
		SCR_AIGroupFireteam baseFt;
		if (cfg.m_bPathLeaderInBase && leaderFt && (leaderFt == ftA || leaderFt == ftB))
		{
			baseFt = leaderFt;
			if (leaderFt == ftA)
				maneuverFt = ftB;
			else
				maneuverFt = ftA;
		}
		else
		{
			// Leapfrog: alternate which element bounds vs. overwatches each reassessment.
			maneuverFt = ftA;
			baseFt = ftB;
			if (m_bDCO_BoundFlip)
			{
				maneuverFt = ftB;
				baseFt = ftA;
			}
			m_bDCO_BoundFlip = !m_bDCO_BoundFlip;
		}

		AICommunicationComponent comms = m_Mailbox;

		// Maneuver element bounds to the next path leg (on foot).
		array<AIAgent> maneuverMembers = {};
		maneuverFt.GetMembers(maneuverMembers);
		foreach (AIAgent mv : maneuverMembers)
		{
			if (!mv)
				continue;
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, m_vDCO_PathNextLeg, EMovementType.RUN, false, null);
			if (!msg)
				continue;
			msg.SetReceiver(mv);
			comms.RequestBroadcast(msg, mv);
		}

		// Base-of-fire element overwatches - but it must stay useful. A base member with no line of sight to the
		// threat is a wasted gun, so it repositions a short distance to regain LOS; otherwise it holds its spot.
		if (!m_aDCO_BaseElemPositions)
			m_aDCO_BaseElemPositions = {};
		m_aDCO_BaseElemPositions.Clear();

		array<AIAgent> baseMembers = {};
		baseFt.GetMembers(baseMembers);
		foreach (AIAgent bm : baseMembers)
		{
			if (!bm)
				continue;
			IEntity be = bm.GetControlledEntity();
			if (!be)
				continue;
			vector bp = be.GetOrigin();

			// DCO_IsConcealedFromThreat returns true when the threat-to-member line is blocked (no LOS). The base
			// of fire wants LOS, so if it's blocked, reposition to a nearby spot that can see the threat.
			vector holdPos = bp;
			if (cfg.m_bPathBaseMaintainLos && DCO_IsConcealedFromThreat(pf, threatPos, bp, cfg))
			{
				vector losPos;
				if (DCO_FindOverwatchPos(pf, bp, threatPos, cfg, losPos))
					holdPos = losPos;
			}
			m_aDCO_BaseElemPositions.Insert(holdPos);

			SCR_AIMessage_Move hold = SCR_AIMessage_Move.Create(null, holdPos, EMovementType.WALK, false, null);
			if (!hold)
				continue;
			hold.SetReceiver(bm);
			comms.RequestBroadcast(hold, bm);
		}

		return true;
	}

	// Find the nearest walkable spot within m_fPathBaseLosRadius that has line of sight to the threat (so the
	// base of fire can keep shooting), preferring slightly higher ground for better fields of fire. False if
	// none found. Used to re-seat a base-of-fire member that has gone blind.
	protected bool DCO_FindOverwatchPos(AIPathfindingComponent pf, vector pos, vector threatPos, DCO_TacticalMoveSettings cfg, out vector best)
	{
		float maxClimb = cfg.m_fCoverMaxClimb;
		float bestScore = -99999.0;
		bool found = false;

		for (int i = 0; i < 8; i++)
		{
			float ang = (i / 8.0) * 6.2831853;
			vector cand = pos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fPathBaseLosRadius;
			DCO_SnapToNavmesh(pf, cand, cand);
			if (cand[1] > pos[1] + maxClimb)
				continue;	// don't climb to an attic
			if (DCO_IsConcealedFromThreat(pf, threatPos, cand, cfg))
				continue;	// must be able to see the threat from here

			float score = (cand[1] - pos[1]) * 5.0 - vector.Distance(cand, pos) * 0.1;	// prefer higher, nearer
			if (score > bestScore)
			{
				bestScore = score;
				best = cand;
				found = true;
			}
		}
		return found;
	}

	// Draw the live path: a bright arrow for the current leg (where they're moving now) and dimmer arrows
	// for the intended remaining legs. Rebuilt each reassessment so the path visibly alters over time.
	protected void DCO_DrawPath(vector startPos)
	{
		DCO_ClearPathVisualsOnly();
		if (!m_aDCO_PathPreview || m_aDCO_PathPreview.IsEmpty())
			return;

		m_aDCO_PathShapes = {};

		int COL_CURRENT		= 0xFF39FF14;	// bright green = current leg
		int COL_INTENDED	= 0x80FFC400;	// translucent amber = intended legs
		ShapeFlags flags = ShapeFlags.NOZBUFFER;
		vector lift = Vector(0, 0.5, 0);	// lift slightly off the ground so the line is visible

		vector prev = startPos;
		for (int i = 0; i < m_aDCO_PathPreview.Count(); i++)
		{
			vector leg = m_aDCO_PathPreview[i];
			int col = COL_INTENDED;
			if (i == 0)
				col = COL_CURRENT;

			Shape arrow = Shape.CreateArrow(prev + lift, leg + lift, 0.6, col, flags);
			if (arrow)
				m_aDCO_PathShapes.Insert(arrow);

			prev = leg;
		}

		// Base-of-fire element markers (cyan spheres) while bounding overwatch is active this tick.
		if (m_aDCO_BaseElemPositions && !m_aDCO_BaseElemPositions.IsEmpty())
		{
			int COL_BASE = 0xFF00E5FF;	// cyan = base of fire / overwatch
			foreach (vector bp : m_aDCO_BaseElemPositions)
			{
				Shape s = Shape.CreateSphere(COL_BASE, flags, bp + lift, 0.6);
				if (s)
					m_aDCO_PathShapes.Insert(s);
			}
		}
	}

	// Release only the debug visuals (keeps the planned path).
	protected void DCO_ClearPathVisualsOnly()
	{
		if (m_aDCO_PathShapes)
			m_aDCO_PathShapes.Clear();	// releasing the held Shape refs frees the visualizers
	}

	// Full reset: drop the goal, the planned legs and the debug visuals.
	protected void DCO_ClearPath()
	{
		m_bDCO_HasPathDest	= false;
		m_bDCO_HasNextLeg	= false;
		if (m_aDCO_PathPreview)
			m_aDCO_PathPreview.Clear();
		if (m_aDCO_BaseElemPositions)
			m_aDCO_BaseElemPositions.Clear();
		DCO_ClearPathVisualsOnly();
	}
}
