// Radio info sharing - the long-range sibling of vocal sharing (DCO_VocalShare.c).
// When a group is in contact it radios the contact to friendly groups out to a large
// radius, seeding their perception. Optionally it also flags those listeners
// reinforcement-eligible so the reinforcement layer (if enabled) lets them converge -
// "the SL heard the call and can move to support". Awareness-only by default; never
// forces a move on its own.
//
// Hosts the shared helper DCO_ShareContactWithNeighbours. This file sorts before
// DCO_VocalShare.c in Comms/, so the helper is defined before both callers.
//
// modded SCR_AIGroupUtilityComponent fragment (sorts before the QRF tick that calls
// DCO_UpdateRadioShare). Server-only, throttled, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastRadioTime	= -1;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Radios this group's
	// nearest contact to friendly groups out to the radio radius.
	void DCO_UpdateRadioShare()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableRadioShare || !m_Owner || !m_Perception)
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastRadioTime >= 0 && (now - m_fDCO_LastRadioTime) < cfg.m_fRadioCheckSec * 1000.0)
			return;
		m_fDCO_LastRadioTime = now;

		IEntity selfLeader = m_Owner.GetLeaderEntity();
		if (!selfLeader)
			return;
		vector selfPos = selfLeader.GetOrigin();

		IEntity enemy;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float d = vector.DistanceSq(t.GetOrigin(), selfPos);
			if (d < bestSq)
			{
				bestSq = d;
				enemy = t;
			}
		}
		if (!enemy)
			return;

		DCO_ShareContactWithNeighbours(enemy, selfPos, cfg.m_fRadioRadius, cfg.m_iRadioMaxListeners, now);

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("RADIO", selfLeader, string.Format("radioed contact at %1 (r=%2)", enemy.GetOrigin(), cfg.m_fRadioRadius));
	}

	// Seed the perception of up to maxListeners friendly groups whose leader is within
	// radius of fromPos with this enemy contact (awareness only, no movement). Shared by
	// vocal + radio sharing. When m_bRadioFlagsReinforcement is on (radio only), listeners
	// are also flagged reinforcement-eligible so they can converge to support.
	protected void DCO_ShareContactWithNeighbours(IEntity enemy, vector fromPos, float radius, int maxListeners, float now)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		vector enemyPos = enemy.GetOrigin();
		Faction enemyFaction;
		FactionAffiliationComponent efac = FactionAffiliationComponent.Cast(enemy.FindComponent(FactionAffiliationComponent));
		if (efac)
			enemyFaction = efac.GetAffiliatedFaction();

		Faction selfFaction = m_Owner.GetFaction();
		float radiusSq = radius * radius;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		set<SCR_AIGroup> notified = new set<SCR_AIGroup>();
		int reached = 0;

		// Is this a radio share? Only radio flags reinforcement eligibility. Inferring it
		// from the radius keeps the helper signature the same for both callers.
		bool isRadio = radius >= cfg.m_fRadioRadius - 0.01;

		foreach (AIAgent agent : agents)
		{
			if (reached >= maxListeners)
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
			if (vector.DistanceSq(grpLeader.GetOrigin(), fromPos) > radiusSq)
				continue;

			SCR_AIGroupUtilityComponent grpUtil = grp.GetGroupUtilityComponent();
			if (!grpUtil || !grpUtil.m_Perception)
				continue;

			notified.Insert(grp);
			reached++;

			// Pure shared awareness - doesn't move them, so it never fights a waypoint or active engagement.
			grpUtil.m_Perception.AddOrUpdateGunshot(enemy, enemyPos, enemyFaction, now, true);

			// Radio only: optionally let the reinforcement layer move them to support.
			if (isRadio && cfg.m_bRadioFlagsReinforcement)
				grpUtil.DCO_SetReinforcementEligible(true);
		}
	}
}
