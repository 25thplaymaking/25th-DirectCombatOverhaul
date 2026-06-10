// Tactical-movement hook on SCR_AIMoveActivity (the group's "move to a position" activity, subclass
// of SCR_AIActivityBase) - the supported seam for making on-order movement smarter.
//
// Deliberate approach: when the tactical layer is enabled and a group is ordered to move to a distant
// fixed position while under threat/contact, it advances at a careful WALK instead of jogging (RUN)
// blindly into the open. Foundation for the later milestones (cover-biased check-in legs,
// bounding/overwatch, path scoring).
//
// Only engages when the group already has a threat/contact (GetThreatMeasure >= m_fThreatActivation),
// so routine patrols stay at normal pace. Also gated on a minimum distance (m_fMinMoveDistance) and
// only downgrades RUN to WALK (desperation SPRINT and existing WALK/IDLE are left untouched;
// entity-follow/escort moves are skipped).
//
// Players are never affected (only AI run move activities). Server-authoritative. Gated on
// DCO_TacticalMoveSettings.m_bEnableTacticalMove (default OFF) - no effect on a live server until a
// Game Master enables it.
modded class SCR_AIMoveActivity
{
	override void InitParameters(vector position, IEntity entity, EMovementType movementType, bool useVehicles, float priorityLevel)
	{
		EMovementType chosen = movementType;
		vector chosenPos = position;

		// Never re-shape a crewed vehicle's move (downgrading a driving crew to WALK or side-stepping its
		// destination fights the driver: wrong path plus jitter). Pass crews straight through to super.
		if (m_Utility && DCO_VehicleUtil.IsGroupInVehicle(SCR_AIGroup.Cast(m_Utility.GetOwner())))
		{
			super.InitParameters(position, entity, movementType, useVehicles, priorityLevel);
			return;
		}

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();

		// Only consider re-shaping a plain RUN-to-a-fixed-position order on the server when the layer is
		// on. Follow/escort moves (entity set), SPRINT (desperation) and slower types are left as-is.
		if (cfg && cfg.m_bEnableTacticalMove && Replication.IsServer())
		{
			// Gather inputs (also used by the diagnostic log) so we can see why a move did/didn't change.
			float threat = -1;
			float dist = -1;
			vector groupPos = vector.Zero;
			bool haveGroup = false;
			if (m_Utility)
			{
				threat = m_Utility.GetThreatMeasure();
				IEntity groupOwner = m_Utility.GetOwner();
				if (groupOwner)
				{
					groupPos = groupOwner.GetOrigin();
					haveGroup = true;
					dist = vector.Distance(groupPos, position);
				}
			}

			// Only when the group has a threat/contact (unless the validation override is on).
			bool threatOk = cfg.m_bForceIgnoreThreat || threat >= cfg.m_fThreatActivation;
			bool longMove = dist >= cfg.m_fMinMoveDistance;

			if (movementType == EMovementType.RUN && !entity && m_Utility && threatOk && longMove)
				chosen = EMovementType.WALK;

			// Anti-funnel: if a known threat sits in the corridor toward a distant destination,
			// sidestep the approach off the enemy axis so the group flanks instead of charging the funnel.
			if (cfg.m_bAvoidThreatFunnel && !entity && haveGroup && threatOk && longMove)
			{
				vector threatPos;
				SCR_AIGroupUtilityComponent groupUtil = m_Utility;
				if (groupUtil && groupUtil.DCO_GetThreatOrLastPosition(threatPos))
					chosenPos = DCO_SidestepFunnel(groupPos, position, threatPos, cfg);
			}

			// Flanking: when the destination is near a known enemy, swing the approach around to
			// the enemy's flank instead of pushing straight down its front. DCO-native replacement for the
			// flanking we used to lean on Skarris for. Runs before exposure scoring so the flank target is
			// then also LOS-checked. Whole-group (no fireteam split) - reliable, no cohesion risk.
			if (cfg.m_bEnableFlanking && !entity && haveGroup && threatOk)
			{
				vector flThreat;
				SCR_AIGroupUtilityComponent flUtil = m_Utility;
				if (flUtil && flUtil.DCO_GetThreatOrLastPosition(flThreat))
					chosenPos = DCO_FlankApproach(groupPos, chosenPos, flThreat, cfg);
			}

			// Exposure scoring: route to the least line-of-sight-exposed of the chosen destination +
			// lateral alternatives, so the group ends up where the threat can't see it when possible.
			if (cfg.m_bEnableExposureScoring && !entity && haveGroup && threatOk && longMove)
			{
				vector exThreat;
				SCR_AIGroupUtilityComponent exUtil = m_Utility;
				if (exUtil && exUtil.DCO_GetThreatOrLastPosition(exThreat))
					chosenPos = DCO_PickLeastExposed(groupPos, chosenPos, exThreat, cfg);
			}

			if (cfg.m_bDebugLog)
				Print(string.Format("[DCO TacMove] init: inType=%1 hasEntity=%2 threat=%3 dist=%4 -> outType=%5 posMoved=%6 (0=IDLE 1=WALK 2=RUN 3=SPRINT)",
					movementType, entity != null, threat, dist, chosen, vector.Distance(chosenPos, position) > 0.1), LogLevel.NORMAL);
		}

		// Procedural pathing: hand the final destination to the per-group pather, which then
		// re-routes to it through cover, leg by leg, while under a danger state. Whole-group positional moves
		// only (no entity-follow); server-side. Default OFF so it changes nothing until a GM enables it.
		if (cfg && cfg.m_bEnableProceduralPath && !entity && m_Utility && Replication.IsServer())
			m_Utility.DCO_SetPathDestination(chosenPos);

		super.InitParameters(chosenPos, entity, chosen, useVehicles, priorityLevel);
	}

	// Flanking approach: rotate the approach around the known enemy so the group comes in from a flank
	// rather than straight down the enemy's front. Returns a destination on the enemy's side, at the same
	// standoff range as the original destination, rotated by m_fFlankAngleDeg toward whichever side is less
	// LOS-exposed. Only engages when the destination is near the enemy (an actual assault, not a long
	// march). Pure 2D ground-plane geometry; the engine re-projects onto navmesh. Returns dest unchanged
	// if the geometry doesn't warrant a flank.
	protected vector DCO_FlankApproach(vector groupPos, vector dest, vector threatPos, DCO_TacticalMoveSettings cfg)
	{
		// Only flank when the destination is actually near the known enemy (i.e. we're moving to engage it),
		// not on a long march to somewhere the enemy merely happens to be vaguely toward.
		vector destToEnemy = threatPos - dest;
		destToEnemy[1] = 0;
		float destEnemyDist = destToEnemy.Length();
		if (destEnemyDist > cfg.m_fFlankMaxEnemyDist)
			return dest;

		// Vector from enemy back out to the group's current side, flattened. This is the bearing we rotate.
		vector enemyToGroup = groupPos - threatPos;
		enemyToGroup[1] = 0;
		float standoff = enemyToGroup.Length();
		if (standoff < cfg.m_fFlankMinEnemyDist)
			return dest;	// already on top of the enemy - no room/benefit to flank
		vector dir = enemyToGroup / standoff;	// unit, enemy to group

		float angleRad = cfg.m_fFlankAngleDeg * 0.0174533;	// deg to rad (no Math.DEG2RAD dependency)
		float s = Math.Sin(angleRad);
		float c = Math.Cos(angleRad);

		// Rotate `dir` about the vertical axis both ways (2D rotation in the ground plane), giving two
		// candidate approach bearings off the enemy's front. Keep the standoff range so we circle, not charge.
		vector left  = Vector(dir[0] * c - dir[2] * s, 0, dir[0] * s + dir[2] * c);
		vector right = Vector(dir[0] * c + dir[2] * s, 0, -dir[0] * s + dir[2] * c);

		vector leftDest  = threatPos + left  * standoff;
		vector rightDest = threatPos + right * standoff;

		// Choose the flank that the enemy is less likely to be watching: the one less line-of-sight exposed
		// to the threat. Reuses the cover trace (safe no-op if the trace layer is unset - then we just
		// keep the left candidate, still a valid flank).
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			bool leftCovered  = DCO_IsCovered(world, threatPos, leftDest, cfg);
			bool rightCovered = DCO_IsCovered(world, threatPos, rightDest, cfg);
			if (rightCovered && !leftCovered)
				return rightDest;
			if (leftCovered && !rightCovered)
				return leftDest;
		}

		// Otherwise pick the flank closer to the group's current side (shorter swing = more natural path).
		if (vector.DistanceSq(leftDest, groupPos) <= vector.DistanceSq(rightDest, groupPos))
			return leftDest;
		return rightDest;
	}

	// If the threat lies within the corridor between the group and the destination (and is closer than
	// the destination), return a destination nudged laterally away from the enemy axis by m_fFunnelSidestep
	// so the group approaches from a flank rather than straight up the funnel. Otherwise returns dest
	// unchanged. Pure 2D (ground-plane) geometry; the engine re-projects onto navmesh.
	protected vector DCO_SidestepFunnel(vector groupPos, vector dest, vector threatPos, DCO_TacticalMoveSettings cfg)
	{
		vector toDest = dest - groupPos;
		toDest[1] = 0;
		float legLen = toDest.Length();
		if (legLen < 1.0)
			return dest;

		vector dir = toDest / legLen;	// unit vector group to dest (flat)

		vector toThreat = threatPos - groupPos;
		toThreat[1] = 0;

		// How far along the leg the threat projects. Only a funnel if the threat is between us and the
		// destination (not behind us, not past the objective).
		float proj = vector.Dot(toThreat, dir);
		if (proj <= 0 || proj >= legLen)
			return dest;

		// Perpendicular offset of the threat from the path. If it's wide of the corridor, no funnel.
		vector perpVec = toThreat - dir * proj;
		float perpDist = perpVec.Length();
		if (perpDist > cfg.m_fFunnelCorridorHalfWidth)
			return dest;

		// Sidestep direction = perpendicular to the leg, pointing away from the threat. If the threat is
		// dead on the line (perpDist ~ 0), pick a stable perpendicular (rotate dir 90 deg in the plane).
		vector side;
		if (perpDist > 0.01)
			side = perpVec / perpDist * -1.0;		// away from where the threat offsets the path
		else
			side = Vector(-dir[2], 0, dir[0]);		// arbitrary but consistent perpendicular

		return dest + side * cfg.m_fFunnelSidestep;
	}

	// Return the least line-of-sight-exposed of: the chosen destination + two lateral alternatives.
	// "Exposed" = the threat has clear LOS to the spot. If the chosen spot is already covered (or none of
	// the alternatives are), the chosen one is kept. Pure geometry + world raycasts; safe no-op if the
	// trace layer is unset (everything then reads "exposed" and the chosen destination is kept).
	protected vector DCO_PickLeastExposed(vector groupPos, vector baseDest, vector threatPos, DCO_TacticalMoveSettings cfg)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return baseDest;

		// If the base destination is already hidden from the threat, keep it.
		if (DCO_IsCovered(world, threatPos, baseDest, cfg))
			return baseDest;

		vector toDest = baseDest - groupPos;
		toDest[1] = 0;
		float len = toDest.Length();
		if (len < 1.0)
			return baseDest;

		vector dir = toDest / len;
		vector perp = Vector(-dir[2], 0, dir[0]);
		float off = cfg.m_fExposureSidestep;

		// Take the first lateral alternative the threat can't see.
		vector left = baseDest + perp * off;
		if (DCO_IsCovered(world, threatPos, left, cfg))
			return left;

		vector right = baseDest - perp * off;
		if (DCO_IsCovered(world, threatPos, right, cfg))
			return right;

		return baseDest;	// nothing better - keep the chosen destination
	}

	// True if world geometry blocks the line from the threat to pos (pos is hidden from the threat).
	// BaseWorld.TraceMove returns the fraction travelled before a hit (< 1 = blocked). LayerMask is a
	// tunable int (no enum), so this always compiles; a zero/unset layer reads everything as not-covered
	// and the feature degrades to a safe no-op until the layer is tuned in-engine.
	protected bool DCO_IsCovered(BaseWorld world, vector threatPos, vector pos, DCO_TacticalMoveSettings cfg)
	{
		vector eye = Vector(0, 1.5, 0);	// rough eye height for both endpoints
		TraceParam param = new TraceParam();
		param.Start = threatPos + eye;
		param.End = pos + eye;
		param.LayerMask = cfg.m_iExposureLayerMask;
		float frac = world.TraceMove(param, null);
		return frac < 0.99;	// hit geometry before reaching pos = covered from the threat
	}
}
