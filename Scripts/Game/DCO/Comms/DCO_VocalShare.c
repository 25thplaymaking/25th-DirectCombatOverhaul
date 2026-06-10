// Vocal info sharing - the short-range sibling of radio sharing. A group that
// perceives an enemy "shouts" the contact to friendly groups within shouting range,
// seeding their perception so they react far sooner. It doesn't move anyone, so it
// never fights a waypoint or an active engagement - pure shared awareness.
//
// Built on the same primitive the reinforcement system uses for shared SA
// (SCR_AIGroupPerception.AddOrUpdateGunshot). The difference is scope: this is
// short-range, high-frequency, awareness-only; radio sharing (DCO_RadioShare.c) is
// the long-range leader-to-leader version. (Audio voicelines would be a cosmetic
// follow-up - the shared awareness is the mechanic.)
//
// modded SCR_AIGroupUtilityComponent fragment (sorts before the QRF tick that calls
// DCO_UpdateVocalShare). Server-only, throttled, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastVocalTime	= -1;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Shares this group's
	// nearest contact with friendly groups within shouting range.
	void DCO_UpdateVocalShare()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVocalShare || !m_Owner || !m_Perception)
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastVocalTime >= 0 && (now - m_fDCO_LastVocalTime) < cfg.m_fVocalCheckSec * 1000.0)
			return;
		m_fDCO_LastVocalTime = now;

		IEntity selfLeader = m_Owner.GetLeaderEntity();
		if (!selfLeader)
			return;
		vector selfPos = selfLeader.GetOrigin();

		// Nearest perceived enemy = the contact we call out.
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

		// DCO_ShareContactWithNeighbours is defined in DCO_RadioShare.c (sorts before this file).
		DCO_ShareContactWithNeighbours(enemy, selfPos, cfg.m_fVocalRadius, cfg.m_iVocalMaxListeners, now);

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("VOCAL", selfLeader, string.Format("called out contact at %1 (r=%2)", enemy.GetOrigin(), cfg.m_fVocalRadius));
	}
}
