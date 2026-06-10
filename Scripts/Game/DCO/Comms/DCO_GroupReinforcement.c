// Reinforcement / shared situational awareness. When a group is in contact it
// periodically broadcasts that contact to nearby friendly groups within a radius:
//   - All nearby friendlies get the enemy position injected into their perception
//     (shared SA) so they become aware and react.
//   - Idle nearby friendlies (no contact of their own) additionally converge: ordered
//     to move toward the enemy. Engaged groups are left to keep fighting.
// A max-responders cap prevents a map-wide cascade.
//
// Deconfliction: "idle" used to mean only "no perceived enemy", but a freshly-spawned
// patrol / Scenario-Framework / IPC capture group has no enemy yet while it walks to
// its waypoint - the old code grabbed it, yanked it off the waypoint, and re-issued the
// move every scan (order spam + groups wandering and stopping). Now a responder still on
// its own waypoint is treated as busy and left to finish, unless it's flagged
// reinforcement-eligible (role preset / GM toggle), which overrides. A per-responder
// re-issue guard stops the same group being re-ordered every scan. Settings live on
// DCO_MoraleSettings (m_bReinforcementRespectWaypoints, m_fReinforcementReissueSec,
// m_fReinforcementRetargetDist, m_bClearWaypointOnOverride).
//
// Second modded SCR_AIGroupUtilityComponent fragment; the shared EvaluateActivity tick
// calls DCO_UpdateReinforcement(). "Comms" sorts before "QRF" so the waypoint/eligibility
// helpers here are visible to DCO_GroupQRF.c. Server-only.
modded class SCR_AIGroupUtilityComponent
{
	protected float m_fDCO_LastReinforceTime = -1;

	// Per-RESPONDER converge throttle (lives on the group that GETS ordered, not the broadcaster).
	protected float		m_fDCO_LastConvergeTime		= -1;
	protected vector	m_vDCO_LastConvergeTarget	= vector.Zero;

	// Per-group opt-in: a reinforcement-eligible group OVERRIDES the "respect waypoints" guard and will
	// converge even while it still holds its own waypoint (set via DCO_GroupRolePreset or a GM toggle).
	protected bool		m_bDCO_ReinforcementEligible	= false;

	// Accessors (set by DCO_GroupRolePreset spawn presets or a GM attribute).
	bool DCO_IsReinforcementEligible()
	{
		return m_bDCO_ReinforcementEligible;
	}

	void DCO_SetReinforcementEligible(bool enable)
	{
		m_bDCO_ReinforcementEligible = enable;
	}

	// True if this group is on its own active waypoint (a patrol/capture/etc from the
	// scenario, Scenario Framework, IPC or a designer). DCO never adds waypoints, so any
	// current waypoint is external.
	bool DCO_GroupHasActiveWaypoint(SCR_AIGroup grp)
	{
		if (!grp)
			return false;
		return grp.GetCurrentWaypoint() != null;
	}

	// Remove this group's current waypoints so an override move (QRF / eligible
	// reinforcement) holds instead of tug-of-warring with the waypoint. Only used when
	// m_bClearWaypointOnOverride is on (default OFF, since clearing a framework/IPC
	// waypoint can desync that system).
	void DCO_ClearExternalWaypoints(SCR_AIGroup grp)
	{
		if (!grp)
			return;
		array<AIWaypoint> wps = {};
		grp.GetWaypoints(wps);
		foreach (AIWaypoint wp : wps)
		{
			if (wp)
				grp.RemoveWaypoint(wp);
		}
	}

	// Per-responder re-issue guard. Returns true (and records) if this group may get a
	// fresh converge order now: it hasn't been ordered within m_fReinforcementReissueSec,
	// or the contact has moved more than m_fReinforcementRetargetDist. False suppresses a
	// duplicate order.
	bool DCO_TryMarkConverge(float now, vector targetPos, DCO_MoraleSettings cfg)
	{
		if (m_fDCO_LastConvergeTime >= 0 && cfg.m_fReinforcementReissueSec > 0)
		{
			float elapsed = now - m_fDCO_LastConvergeTime;
			bool movedFar = vector.Distance(targetPos, m_vDCO_LastConvergeTarget) > cfg.m_fReinforcementRetargetDist;
			if (elapsed < cfg.m_fReinforcementReissueSec * 1000.0 && !movedFar)
				return false;
		}
		m_fDCO_LastConvergeTime = now;
		m_vDCO_LastConvergeTarget = targetPos;
		return true;
	}

	void DCO_UpdateReinforcement()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableReinforcement)
			return;

		if (!m_Owner || !m_Perception)
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastReinforceTime >= 0 && (now - m_fDCO_LastReinforceTime) < cfg.m_fReinforcementCooldownSec * 1000.0)
			return;

		IEntity enemy;
		foreach (IEntity t : targets)
		{
			if (t)
			{
				enemy = t;
				break;
			}
		}
		if (!enemy)
			return;

		m_fDCO_LastReinforceTime = now;

		vector enemyPos = enemy.GetOrigin();
		Faction enemyFaction;
		FactionAffiliationComponent efac = FactionAffiliationComponent.Cast(enemy.FindComponent(FactionAffiliationComponent));
		if (efac)
			enemyFaction = efac.GetAffiliatedFaction();

		IEntity selfLeader = m_Owner.GetLeaderEntity();
		if (!selfLeader)
			return;
		vector selfPos = selfLeader.GetOrigin();
		Faction selfFaction = m_Owner.GetFaction();
		float radiusSq = cfg.m_fReinforcementRadius * cfg.m_fReinforcementRadius;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		set<SCR_AIGroup> notified = new set<SCR_AIGroup>();
		int responders = 0;

		foreach (AIAgent agent : agents)
		{
			if (responders >= cfg.m_iReinforcementMaxResponders)
				break;

			if (!agent)
				continue;

			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || grp == m_Owner || notified.Contains(grp))
				continue;

			if (grp.GetFaction() != selfFaction)
				continue;

			IEntity grpLeader = grp.GetLeaderEntity();
			if (!grpLeader)
				continue;

			if (vector.DistanceSq(grpLeader.GetOrigin(), selfPos) > radiusSq)
				continue;

			SCR_AIGroupUtilityComponent grpUtil = grp.GetGroupUtilityComponent();
			if (!grpUtil || !grpUtil.m_Perception)
				continue;

			notified.Insert(grp);
			responders++;

			// Shared SA: everyone nearby becomes aware of the contact. Doesn't move them,
			// so it never conflicts with a waypoint.
			grpUtil.m_Perception.AddOrUpdateGunshot(enemy, enemyPos, enemyFaction, now, true);

			// Active reinforcement: only groups not already engaged converge.
			if (!cfg.m_bReinforcementConverge)
				continue;

			array<IEntity> theirTargets = grpUtil.m_Perception.m_aTargetEntities;
			bool engaged = theirTargets && !theirTargets.IsEmpty();
			if (engaged)
				continue;	// fighting its own contact - leave it

			// A responder still on its own waypoint is busy (patrol / IPC capture / designer
			// task). Leave it to finish unless it's flagged reinforcement-eligible.
			bool eligibleOverride = grpUtil.DCO_IsReinforcementEligible();
			if (cfg.m_bReinforcementRespectWaypoints && !eligibleOverride && DCO_GroupHasActiveWaypoint(grp))
				continue;

			// Re-issue guard: don't re-order the same group every scan (the duplicate-order / wander fix).
			if (!grpUtil.DCO_TryMarkConverge(now, enemyPos, cfg))
				continue;

			// Optional hard override: clear the external waypoint so the converge move actually holds.
			if (eligibleOverride && cfg.m_bClearWaypointOnOverride)
				DCO_ClearExternalWaypoints(grp);

			DCO_OrderConverge(grp, grpUtil, enemy);
		}
	}

	// Order an idle friendly group to move toward the contact. If vehicle-borne
	// reinforcement is enabled and the responder is far away, the move is issued with
	// useVehicles=true so the group mounts up; otherwise it's a normal on-foot converge.
	protected void DCO_OrderConverge(SCR_AIGroup grp, SCR_AIGroupUtilityComponent grpUtil, IEntity enemy)
	{
		AIAgent leaderAgent = grp.GetLeaderAgent();
		if (!leaderAgent)
			return;

		AICommunicationComponent comms = grpUtil.m_Mailbox;
		if (!comms)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		// Vehicle-borne reinforcement: only for an on-foot responder far from the contact.
		// useVehicles=true lets the engine board the nearest suitable vehicle and drive
		// there (board-then-converge, no separate get-in order to conflict with).
		// Already-mounted responders fall through to the vehicle-aware converge below.
		if (cfg.m_bEnableVehicleReinforcement && !DCO_VehicleUtil.IsGroupInVehicle(grp))
		{
			IEntity grpLeader = grp.GetLeaderEntity();
			if (grpLeader && vector.Distance(grpLeader.GetOrigin(), enemy.GetOrigin()) >= cfg.m_fVehicleReinforceMinDist)
			{
				SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(enemy, enemy.GetOrigin(), EMovementType.RUN, true, null);
				if (msg)
				{
					comms.RequestBroadcast(msg);
					return;
				}
			}
		}

		// Vehicle-aware fallback: a mounted responder drives to converge instead of
		// dismounting to walk, even when vehicle reinforcement above is disabled.
		DCO_VehicleUtil.OrderGroupMoveToEntity(grp, enemy, comms);
	}
}
