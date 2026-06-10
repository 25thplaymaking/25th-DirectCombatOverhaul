// CQB town-clearing per-group state machine. An on-foot group methodically clears
// buildings in its detection radius as it advances to its ordered target, chaining
// nearest-first via DCO_CqbRegistry (so squads split the work), then cancels so the
// standing order resumes. modded SCR_AIGroupUtilityComponent fragment in CQB/ (sorts
// before the QRF tick; DCO_CQB.c sorts before this so we can reuse its helpers).
// Server-only, throttled, default OFF.
//
// State flow: SELECT (claim the nearest building, build the node sweep path) ->
// APPROACH (move members to stack offsets at the breach) -> ENTER (send the stack
// single-file to the first interior node) -> CLEAR (hold and scan each node
// nearest-first; if an enemy's inside, pause and let the combat layers fight, then
// resume; when all nodes are swept, mark cleared and pick the next building) -> DONE.
// Movement is per-member on-foot (the DCO_Garrison pattern); every node is
// navmesh-validated by DCO_CqbClearUtil.
modded class SCR_AIGroupUtilityComponent
{
	protected bool				m_bDCO_IsCqbClearer;
	protected EDCO_CqbState		m_eDCO_CqbState	= EDCO_CqbState.IDLE;
	protected ref DCO_CqbJob	m_DCO_CqbJob;
	protected float				m_fDCO_LastCqbClearTime	= -1;
	protected float				m_fDCO_CqbBuildingStart	= -1;	// world time (ms) APPROACH on the current building began (anti-hang watchdog)
	protected int				m_iDCO_CqbClearedCount	= 0;	// buildings finished in the current area (caps via m_iCqbMaxBuildings)
	protected vector			m_vDCO_CqbRunStart;				// leader pos where the current clearing run began (cap-reset reference)
	protected bool				m_bDCO_CqbHasRunStart	= false;
	protected vector			m_vDCO_CqbLastStackPos;			// leader pos at the last APPROACH stack issue (reorder gate)
	protected bool				m_bDCO_CqbStackIssued	= false;
	protected ref array<ref Shape>	m_aDCO_CqbDebugShapes;
	protected ref array<IEntity>	m_aDCO_CqbScan;

	bool DCO_IsCqbClearer()				{ return m_bDCO_IsCqbClearer; }
	void DCO_SetCqbClearer(bool enable)	{ m_bDCO_IsCqbClearer = enable; }

	// True while the state machine owns this group's movement (so DCO_CQB's push yields).
	bool DCO_CqbIsClearingActive()
	{
		return m_eDCO_CqbState == EDCO_CqbState.APPROACH
			|| m_eDCO_CqbState == EDCO_CqbState.ENTER
			|| m_eDCO_CqbState == EDCO_CqbState.CLEAR;
	}

	// Does the CQB-clear feature apply to this group right now?
	protected bool DCO_CqbApplies(DCO_MoraleSettings cfg)
	{
		if (!cfg.m_bEnableCqbClear)
			return false;
		if (cfg.m_bCqbClearMarkedOnly && !m_bDCO_IsCqbClearer)
			return false;
		if (!m_Owner || !m_Mailbox)
			return false;
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return false;	// on-foot only
		return true;
	}

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity).
	void DCO_UpdateCqbClear()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!DCO_CqbApplies(cfg))
		{
			DCO_CqbAbort();	// off / mounted / unmarked: drop any claim and go idle
			if (cfg.m_bDebugCqbClear)
				DCO_CqbClearDebug();
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastCqbClearTime >= 0 && (now - m_fDCO_LastCqbClearTime) < cfg.m_fCqbClearCheckSec * 1000.0)
			return;
		m_fDCO_LastCqbClearTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector lead = leader.GetOrigin();

		// Approach watchdog: if the squad can't reach the breach within the timeout,
		// give up on this building (mark it done so we don't re-claim it, drop the job)
		// and move on - never hang outside an unreachable building. Only APPROACH can
		// stall; CLEAR self-advances on a time-based dwell.
		if (m_eDCO_CqbState == EDCO_CqbState.APPROACH && m_DCO_CqbJob && m_fDCO_CqbBuildingStart >= 0
			&& (now - m_fDCO_CqbBuildingStart) >= cfg.m_fCqbApproachTimeoutSec * 1000.0)
		{
			if (m_DCO_CqbJob.m_Building)
				DCO_CqbRegistry.Get().MarkCleared(m_DCO_CqbJob.m_Building);
			m_iDCO_CqbClearedCount++;	// consumed an attempt (counts toward the per-area cap)
			m_DCO_CqbJob = null;
			m_fDCO_CqbBuildingStart = -1;
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			if (cfg.m_bDebug)
				DCO_Debug.LogGroup("CQBCLEAR", leader, "approach timed out - skipping unreachable building");
		}

		switch (m_eDCO_CqbState)
		{
			case EDCO_CqbState.IDLE:
			case EDCO_CqbState.SELECT:
			case EDCO_CqbState.DONE:
			{
				DCO_CqbSelect(world, leader, lead, cfg, now);
				break;
			}
			case EDCO_CqbState.APPROACH:
			{
				DCO_CqbApproach(leader, lead, cfg);
				break;
			}
			case EDCO_CqbState.ENTER:
			{
				DCO_CqbEnter(leader, lead, cfg);
				break;
			}
			case EDCO_CqbState.CLEAR:
			{
				DCO_CqbClearStep(leader, lead, cfg, now);
				break;
			}
		}

		if (cfg.m_bDebugCqbClear)
			DCO_CqbClearDebug();
	}

	// SELECT: claim the nearest claimable building, build its node sweep path + entry,
	// then go APPROACH.
	protected void DCO_CqbSelect(BaseWorld world, IEntity leader, vector lead, DCO_MoraleSettings cfg, float now)
	{
		// Per-area cap reset: once the squad advances a full sweep-radius beyond where
		// this run began, treat it as a new area and reset the count.
		if (m_bDCO_CqbHasRunStart && vector.DistanceSq(lead, m_vDCO_CqbRunStart) > cfg.m_fCqbClearRadius * cfg.m_fCqbClearRadius)
		{
			m_iDCO_CqbClearedCount = 0;
			m_bDCO_CqbHasRunStart = false;
		}

		// Quota reached for this area: stand down (cancel once) so the squad resumes its
		// ordered move. It only clears again after moving on to a new area (reset above).
		if (cfg.m_iCqbMaxBuildings > 0 && m_iDCO_CqbClearedCount >= cfg.m_iCqbMaxBuildings)
		{
			if (m_eDCO_CqbState != EDCO_CqbState.IDLE)
			{
				AIAgent lq = m_Owner.GetLeaderAgent();
				if (lq)
					SCR_AIMessageHandling.SendCancelMessage(lq, null, m_Mailbox);
			}
			m_eDCO_CqbState = EDCO_CqbState.IDLE;
			return;
		}

		IEntity building = DCO_CqbFindNearestClaimable(world, lead, cfg.m_fCqbClearRadius);
		if (!building)
		{
			// Nothing left in radius: area clear. Reset the cap, cancel so the standing
			// order resumes, then idle.
			m_iDCO_CqbClearedCount = 0;
			m_bDCO_CqbHasRunStart = false;
			if (m_eDCO_CqbState != EDCO_CqbState.IDLE)
			{
				AIAgent la = m_Owner.GetLeaderAgent();
				if (la)
					SCR_AIMessageHandling.SendCancelMessage(la, null, m_Mailbox);
			}
			m_eDCO_CqbState = EDCO_CqbState.IDLE;
			return;
		}

		DCO_CqbRegistry.Get().Claim(building, m_Owner);
		m_DCO_CqbJob = new DCO_CqbJob();
		m_DCO_CqbJob.m_Building = building;
		if (!m_bDCO_CqbHasRunStart)
		{
			m_vDCO_CqbRunStart = lead;	// anchor the area for the cap reset
			m_bDCO_CqbHasRunStart = true;
		}

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
		array<vector> nodes = {};
		DCO_CqbClearUtil.CollectInteriorNodes(pf, building, cfg.m_fCqbBuildingScan, nodes);
		if (nodes.IsEmpty())
		{
			// No reachable interior: can't clear, so mark cleared (don't re-select it), count it, move on.
			DCO_CqbRegistry.Get().MarkCleared(building);
			m_iDCO_CqbClearedCount++;
			m_DCO_CqbJob = null;
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			return;
		}
		vector entry;
		DCO_CqbClearUtil.PickEntry(nodes, lead, entry);
		DCO_CqbClearUtil.OrderNodesFrom(nodes, entry, m_DCO_CqbJob.m_aNodes);
		m_DCO_CqbJob.m_vEntry = entry;
		m_DCO_CqbJob.m_bHasEntry = true;
		m_DCO_CqbJob.m_iNodeIdx = 0;
		m_eDCO_CqbState = EDCO_CqbState.APPROACH;
		m_fDCO_CqbBuildingStart = now;	// start the approach watchdog for this building
		m_bDCO_CqbStackIssued = false;	// re-issue the stack fresh for this building (reorder gate)

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("CQBCLEAR", leader, string.Format("clearing building at %1 (%2 nodes)", building.GetOrigin(), m_DCO_CqbJob.m_aNodes.Count()));
	}

	// APPROACH: move members to stack offsets behind the breach; when the leader reaches
	// the breach area, transition to ENTER.
	protected void DCO_CqbApproach(IEntity leader, vector lead, DCO_MoraleSettings cfg)
	{
		if (!m_DCO_CqbJob || !m_DCO_CqbJob.m_bHasEntry)
		{
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			return;
		}
		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		int n = agents.Count();
		if (n <= 0)
		{
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			return;
		}

		// Reorder gate: only (re)issue the stack on first entry to APPROACH, or when the
		// leader has shifted more than the reorder distance since the last issue - avoids
		// re-pathing the stack every tick.
		bool issueStack = !m_bDCO_CqbStackIssued
			|| vector.DistanceSq(lead, m_vDCO_CqbLastStackPos) > cfg.m_fCqbReorderDistClear * cfg.m_fCqbReorderDistClear;
		if (issueStack)
		{
			array<vector> offsets = {};
			DCO_CqbClearUtil.StackOffsets(pf, m_DCO_CqbJob.m_vEntry, lead, n, cfg.m_fCqbStackSpacing, offsets);
			for (int i = 0; i < n; i++)
			{
				AIAgent a = agents[i];
				if (!a || i >= offsets.Count())
					continue;
				DCO_CqbMoveMember(a, offsets[i]);
			}
			m_vDCO_CqbLastStackPos = lead;
			m_bDCO_CqbStackIssued = true;
		}

		// Arrived at the breach? (leader within two stack spacings of the entry)
		if (vector.DistanceSq(lead, m_DCO_CqbJob.m_vEntry) <= (cfg.m_fCqbStackSpacing * 2.0) * (cfg.m_fCqbStackSpacing * 2.0))
		{
			m_DCO_CqbJob.m_iNodeIdx = 0;
			m_DCO_CqbJob.m_fNodeEnterTime = -1;
			m_eDCO_CqbState = EDCO_CqbState.ENTER;
		}
	}

	// ENTER: send the whole stack to the first interior node single-file (the engine
	// queues them through the breach), then go to CLEAR.
	protected void DCO_CqbEnter(IEntity leader, vector lead, DCO_MoraleSettings cfg)
	{
		if (!m_DCO_CqbJob || m_DCO_CqbJob.m_aNodes.IsEmpty())
		{
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			return;
		}
		vector first = m_DCO_CqbJob.m_aNodes[0];
		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (a)
				DCO_CqbMoveMember(a, first);
		}
		m_eDCO_CqbState = EDCO_CqbState.CLEAR;
		m_DCO_CqbJob.m_fNodeEnterTime = -1;
	}

	// CLEAR: hold and scan the current node for the dwell, then advance. If an enemy is
	// perceived inside, pause (combat layers engage) and reset the dwell so we only
	// advance once it's quiet. When all nodes are swept and quiet: mark cleared, next building.
	protected void DCO_CqbClearStep(IEntity leader, vector lead, DCO_MoraleSettings cfg, float now)
	{
		if (!m_DCO_CqbJob || !m_DCO_CqbJob.m_Building || m_DCO_CqbJob.m_aNodes.IsEmpty())
		{
			m_eDCO_CqbState = EDCO_CqbState.SELECT;
			return;
		}

		bool enemyInside = DCO_CqbClearUtil.EnemyInsideBuilding(m_Perception, m_DCO_CqbJob.m_Building, cfg.m_fCqbBuildingScan);
		if (enemyInside)
		{
			m_DCO_CqbJob.m_fNodeEnterTime = now;	// keep resetting so the dwell never elapses while an enemy's present
			return;
		}

		int idx = m_DCO_CqbJob.m_iNodeIdx;
		vector node = m_DCO_CqbJob.m_aNodes[idx];

		// Start the dwell on this node + send the squad to it.
		if (m_DCO_CqbJob.m_fNodeEnterTime < 0)
		{
			m_DCO_CqbJob.m_fNodeEnterTime = now;
			array<AIAgent> agents = {};
			m_Owner.GetAgents(agents);
			foreach (AIAgent a : agents)
			{
				if (a)
					DCO_CqbMoveMember(a, node);
			}
			return;
		}

		// Dwell elapsed on this node: advance.
		if ((now - m_DCO_CqbJob.m_fNodeEnterTime) >= cfg.m_fCqbNodeDwellSec * 1000.0)
		{
			m_DCO_CqbJob.m_iNodeIdx = idx + 1;
			m_DCO_CqbJob.m_fNodeEnterTime = -1;
			if (m_DCO_CqbJob.m_iNodeIdx >= m_DCO_CqbJob.m_aNodes.Count())
			{
				// Every node swept and quiet: mark cleared and move to the next building.
				DCO_CqbRegistry.Get().MarkCleared(m_DCO_CqbJob.m_Building);
				m_iDCO_CqbClearedCount++;	// counts toward the per-area cap (m_iCqbMaxBuildings)
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("CQBCLEAR", leader, string.Format("cleared building at %1", m_DCO_CqbJob.m_Building.GetOrigin()));
				m_DCO_CqbJob = null;
				m_eDCO_CqbState = EDCO_CqbState.SELECT;
			}
		}
	}

	// Per-member on-foot move order (the DCO_Garrison pattern).
	protected void DCO_CqbMoveMember(AIAgent agent, vector pos)
	{
		SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, pos, EMovementType.WALK, false, null);
		if (msg)
		{
			msg.SetReceiver(agent);
			m_Mailbox.RequestBroadcast(msg, agent);
		}
	}

	// Nearest building within radius that the registry says this group may claim.
	protected IEntity DCO_CqbFindNearestClaimable(BaseWorld world, vector fromPos, float radius)
	{
		if (!m_aDCO_CqbScan)
			m_aDCO_CqbScan = {};
		m_aDCO_CqbScan.Clear();
		world.QueryEntitiesBySphere(fromPos, radius, DCO_CqbBuildingCollect);

		IEntity best;
		float bestSq = radius * radius + 1.0;
		DCO_CqbRegistry reg = DCO_CqbRegistry.Get();
		foreach (IEntity b : m_aDCO_CqbScan)
		{
			if (!b || !reg.IsClaimable(b, m_Owner))
				continue;
			float d = vector.DistanceSq(b.GetOrigin(), fromPos);
			if (d < bestSq)
			{
				bestSq = d;
				best = b;
			}
		}
		m_aDCO_CqbScan.Clear();
		return best;
	}

	// QueryEntitiesBySphere callback: collect building entities.
	protected bool DCO_CqbBuildingCollect(IEntity e)
	{
		if (!e)
			return true;
		if (Building.Cast(e) || e.FindComponent(SCR_DestructibleBuildingComponent))
			m_aDCO_CqbScan.Insert(e);
		return true;
	}

	// Drop any active job/claim and go idle (feature disabled, group mounted, or unmarked).
	protected void DCO_CqbAbort()
	{
		if (m_DCO_CqbJob && m_DCO_CqbJob.m_Building)
			DCO_CqbRegistry.Get().Release(m_DCO_CqbJob.m_Building);
		m_DCO_CqbJob = null;
		m_eDCO_CqbState = EDCO_CqbState.IDLE;
		m_iDCO_CqbClearedCount = 0;
		m_bDCO_CqbHasRunStart = false;
		m_bDCO_CqbStackIssued = false;
		m_fDCO_CqbBuildingStart = -1;
	}

	// Debug draw: target building (amber=approaching, cyan=clearing), entry, and the
	// ordered interior nodes (green=swept, red=current, grey=pending) with sweep arrows.
	protected void DCO_CqbClearDebug()
	{
		if (m_aDCO_CqbDebugShapes)
			m_aDCO_CqbDebugShapes.Clear();
		else
			m_aDCO_CqbDebugShapes = {};
		if (!m_DCO_CqbJob || !m_DCO_CqbJob.m_Building)
			return;
		ShapeFlags flags = ShapeFlags.NOZBUFFER;

		int col = 0xffffaa00;	// amber = claimed/approaching
		if (m_eDCO_CqbState == EDCO_CqbState.CLEAR)
			col = 0xff00ffff;	// cyan = clearing
		vector bp = m_DCO_CqbJob.m_Building.GetOrigin() + Vector(0, 4, 0);
		Shape bs = Shape.CreateSphere(col, flags, bp, 2.0);
		if (bs)
			m_aDCO_CqbDebugShapes.Insert(bs);

		if (m_DCO_CqbJob.m_bHasEntry)
		{
			Shape es = Shape.CreateSphere(0xffffffff, flags, m_DCO_CqbJob.m_vEntry + Vector(0, 1, 0), 0.5);
			if (es)
				m_aDCO_CqbDebugShapes.Insert(es);
		}

		for (int i = 0; i < m_DCO_CqbJob.m_aNodes.Count(); i++)
		{
			int nc = 0xff808080;	// pending grey
			if (i < m_DCO_CqbJob.m_iNodeIdx)
				nc = 0xff00ff00;	// done green
			else if (i == m_DCO_CqbJob.m_iNodeIdx)
				nc = 0xffff0000;	// current red
			vector np = m_DCO_CqbJob.m_aNodes[i] + Vector(0, 1, 0);
			Shape ns = Shape.CreateSphere(nc, flags, np, 0.4);
			if (ns)
				m_aDCO_CqbDebugShapes.Insert(ns);
			if (i > 0)
			{
				Shape ar = Shape.CreateArrow(m_DCO_CqbJob.m_aNodes[i - 1] + Vector(0, 1, 0), np, 0.2, 0xff8888ff, flags);
				if (ar)
					m_aDCO_CqbDebugShapes.Insert(ar);
			}
		}
	}
}
