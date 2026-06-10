// Night illumination. When a group is in contact in low light, it pops an illumination
// flare over the enemy so it can see and fight at night.
//
// Native wire-in: broadcast SCR_AIMessage_FireIllumFlareAt.Create(enemyPos) via the group
// mailbox; the engine's illum-flare behaviour selects an illum-capable weapon, aims and
// fires. Gated by SCR_AIGroupInfoComponent.IsIllumFlareAllowed() - the engine's own check
// that already accounts for ambient light and a per-group flare cooldown, so this only
// fires at night and never spams. With no illum-capable weapon the broadcast just no-ops.
//
// modded SCR_AIGroupUtilityComponent fragment. "Illum" sorts before "QRF" so
// DCO_UpdateNightIllum is defined before the tick that calls it. Server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastIllumTime	= -1;

	void DCO_UpdateNightIllum()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableNightIllum || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastIllumTime >= 0 && (now - m_fDCO_LastIllumTime) < cfg.m_fIllumCheckIntervalSec * 1000.0)
			return;
		m_fDCO_LastIllumTime = now;

		// Engine gate: only at night, respects the per-group flare cooldown. The info
		// component is a sibling on the group entity (this component's owner).
		IEntity groupEnt = GetOwner();
		if (!groupEnt)
			return;
		SCR_AIGroupInfoComponent info = SCR_AIGroupInfoComponent.Cast(groupEnt.FindComponent(SCR_AIGroupInfoComponent));
		if (!info || !info.IsIllumFlareAllowed())
			return;

		// Pop the flare over the nearest perceived enemy.
		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		vector fromPos = leader.GetOrigin();
		vector enemyPos;
		bool found = false;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float dSq = vector.DistanceSq(t.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				enemyPos = t.GetOrigin();
				found = true;
			}
		}
		if (!found)
			return;

		SCR_AIMessage_FireIllumFlareAt msg = SCR_AIMessage_FireIllumFlareAt.Create(enemyPos);
		if (msg)
			m_Mailbox.RequestBroadcast(msg);
	}
}
