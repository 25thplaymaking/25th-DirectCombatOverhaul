// Fratricide avoidance / friendly-fire LOS hold. Movement-heavy AI tend to shoot each
// other when a friendly crosses their firing lane. This briefly holds a member's fire
// when a friendly character is on the line between it and the enemy it would shoot.
//
// Per-group tick: for each member, raycast the lane to every perceived enemy with
// QueryEntitiesByLine; if a same-faction character is in any of those lanes, apply a
// short SetWeaponNoFireTime so it won't shoot through a friendly whichever target it's
// engaging. Conservative by design. Additive to - never disables - the engine's own
// PerceptionComponent.GetFriendlyInLineOfFire avoidance.
//
// modded SCR_AIGroupUtilityComponent fragment. "FriendlyFire" sorts before "QRF" so
// DCO_UpdateFriendlyFire is defined before the tick that calls it. Server-only,
// default ON. One throttled line-query per engaging member, only while it perceives an
// enemy; widen the lane with a small offset if grazing shots slip through.
modded class SCR_AIGroupUtilityComponent
{
	protected float				m_fDCO_LastFFTime	= -1;
	protected ref array<IEntity>	m_aDCO_FFLineHits;
	protected IEntity			m_eDCO_FFShooter;
	protected Faction			m_DCO_FFSelfFaction;

	void DCO_UpdateFriendlyFire()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableFriendlyFire || !m_Owner || !m_Perception)
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastFFTime >= 0 && (now - m_fDCO_LastFFTime) < cfg.m_fFriendlyLaneCheckSec * 1000.0)
			return;
		m_fDCO_LastFFTime = now;

		m_DCO_FFSelfFaction = m_Owner.GetFaction();

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity ent = agent.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never hold a player's fire

			// Hold this member's fire if a friendly sits on its lane to any perceived enemy
			// (not just the nearest), so it never shoots through a friendly.
			vector shooterPos = ent.GetOrigin();
			bool friendlyInAnyLane = false;
			foreach (IEntity t : targets)
			{
				if (!t)
					continue;
				if (DCO_FriendlyInLane(world, shooterPos, t.GetOrigin(), ent))
				{
					friendlyInAnyLane = true;
					break;	// one blocked lane is enough to hold fire
				}
			}

			if (friendlyInAnyLane)
			{
				SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
				if (cc)
					cc.SetWeaponNoFireTime(cfg.m_fFriendlyFireHold);
			}
		}
	}

	// True if a same-faction character sits on the firing lane from->to, excluding the shooter.
	protected bool DCO_FriendlyInLane(BaseWorld world, vector from, vector to, IEntity shooter)
	{
		if (!m_aDCO_FFLineHits)
			m_aDCO_FFLineHits = {};
		m_aDCO_FFLineHits.Clear();
		m_eDCO_FFShooter = shooter;

		vector eye = Vector(0, 1.4, 0);	// rough muzzle/eye height
		world.QueryEntitiesByLine(from + eye, to + eye, DCO_FFLineCollect);

		foreach (IEntity e : m_aDCO_FFLineHits)
		{
			if (!e || e == shooter)
				continue;

			FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(e.FindComponent(FactionAffiliationComponent));
			if (fac && fac.GetAffiliatedFaction() == m_DCO_FFSelfFaction)
				return true;	// a friendly is in the lane
		}
		return false;
	}

	// QueryEntitiesByLine callback: collect entities on the lane, excluding the shooter.
	protected bool DCO_FFLineCollect(IEntity e)
	{
		if (e && e != m_eDCO_FFShooter)
			m_aDCO_FFLineHits.Insert(e);
		return true;
	}
}
