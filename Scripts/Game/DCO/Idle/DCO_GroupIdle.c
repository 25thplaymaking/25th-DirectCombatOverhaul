// Idle behaviour. A group left with no orders eventually spreads out and patrols
// locally rather than freezing on its spawn marker. Static-gun manning already lives
// in Assets/DCO_AssetUse.c; this adds the rest:
//   - Detect idle: no perceived enemy, no active waypoint, and the leader has stayed
//     put for m_fIdleDelaySec.
//   - Spread (once on entering idle): disperse the members to navmesh points around the
//     leader so they don't bunch.
//   - Patrol: every m_fIdlePatrolIntervalSec, with chance m_fIdlePatrolChance, walk the
//     group to a random reachable point within m_fIdlePatrolRadius of the anchor, then settle.
//
// Deferential: it only acts with no perceived target and no current waypoint, so a group
// with a real task (combat, an IPC/Scenario-Framework waypoint, a DCO role move) is never
// idle. The patrol move itself just pauses the idle logic until the group settles again;
// the anchor is kept so patrols stay local.
//
// modded SCR_AIGroupUtilityComponent fragment in Idle/ (sorts before the QRF tick that
// calls DCO_UpdateIdle). On-foot, server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected bool		m_bDCO_IdleActive			= false;
	protected bool		m_bDCO_IdleSpread			= false;	// did the one-time spread for this idle spell
	protected vector	m_vDCO_IdleAnchor;						// position the group settled at (patrol origin)
	protected vector	m_vDCO_IdleLastLeaderPos;				// leader pos last check (movement detection)
	protected float		m_fDCO_IdleLeaderMoveTime	= -1;		// world time (ms) the leader last moved significantly
	protected float		m_fDCO_IdleLastPatrolTime	= -1;		// world time (ms) of the last patrol roll

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Detects idle and
	// drives spread + patrol.
	void DCO_UpdateIdle()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableIdleBehaviour || !m_Owner || !m_Mailbox)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		// A real task cancels idle: perceived enemy or an active waypoint.
		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			if (targets && !targets.IsEmpty())
			{
				m_bDCO_IdleActive = false;
				return;
			}
		}
		if (m_Owner.GetCurrentWaypoint())
		{
			m_bDCO_IdleActive = false;
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector pos = leader.GetOrigin();

		// Movement tracking: reset the stationary timer whenever the leader has moved beyond the epsilon.
		if (m_fDCO_IdleLeaderMoveTime < 0 || vector.DistanceSq(pos, m_vDCO_IdleLastLeaderPos) > cfg.m_fIdleMoveEpsilon * cfg.m_fIdleMoveEpsilon)
		{
			m_vDCO_IdleLastLeaderPos = pos;
			m_fDCO_IdleLeaderMoveTime = now;
		}

		float stationarySec = (now - m_fDCO_IdleLeaderMoveTime) / 1000.0;
		if (stationarySec < cfg.m_fIdleDelaySec)
			return;	// not idle yet (or moving on a patrol leg)

		// Idle.
		if (!m_bDCO_IdleActive)
		{
			m_bDCO_IdleActive		= true;
			m_bDCO_IdleSpread		= false;
			m_vDCO_IdleAnchor		= pos;
			m_fDCO_IdleLastPatrolTime = now;	// wait one interval before the first patrol
			if (cfg.m_bDebug)
				DCO_Debug.LogGroup("IDLE", leader, string.Format("entered idle at %1", pos));
		}

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));

		// One-time spread to cover/positions around the leader.
		if (!m_bDCO_IdleSpread)
		{
			DCO_IdleSpread(pf, pos, cfg);
			m_bDCO_IdleSpread = true;
		}

		// Periodic local patrol.
		if ((now - m_fDCO_IdleLastPatrolTime) >= cfg.m_fIdlePatrolIntervalSec * 1000.0)
		{
			m_fDCO_IdleLastPatrolTime = now;
			if (Math.RandomFloat(0, 1) <= cfg.m_fIdlePatrolChance)
				DCO_IdlePatrol(pf, cfg);
		}
	}

	// Disperse each member to a navmesh point on a ring around the leader (anti-bunch).
	protected void DCO_IdleSpread(AIPathfindingComponent pf, vector center, DCO_MoraleSettings cfg)
	{
		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		int n = agents.Count();
		if (n <= 1)
			return;

		for (int i = 0; i < n; i++)
		{
			AIAgent a = agents[i];
			if (!a)
				continue;
			// Even ring spacing + a little jitter.
			float ang = (i / (float)n) * 6.2831853 + Math.RandomFloat(-0.3, 0.3);
			vector cand = center + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fIdleSpreadRadius;
			DCO_IdleSnapNav(pf, cand, cand);

			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, cand, EMovementType.WALK, false, null);
			if (msg)
			{
				msg.SetReceiver(a);
				m_Mailbox.RequestBroadcast(msg, a);
			}
		}
	}

	// Walk the whole group to a random reachable point within patrol radius of the anchor.
	protected void DCO_IdlePatrol(AIPathfindingComponent pf, DCO_MoraleSettings cfg)
	{
		float ang = Math.RandomFloat(0, 6.2831853);
		float dist = Math.RandomFloat(cfg.m_fIdlePatrolRadius * 0.3, cfg.m_fIdlePatrolRadius);
		vector cand = m_vDCO_IdleAnchor + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * dist;
		DCO_IdleSnapNav(pf, cand, cand);
		DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, cand, m_Mailbox);
		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("IDLE", m_Owner.GetLeaderEntity(), string.Format("patrol to %1", cand));
	}

	// Snap a candidate onto the navmesh; unchanged if no pathfinding/no hit.
	protected void DCO_IdleSnapNav(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;
		vector corrected;
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(8, 2, 8), corrected))
			outPos = corrected;
	}
}
