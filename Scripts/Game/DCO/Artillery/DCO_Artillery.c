// Artillery fire missions. An AI mortar crew that perceives (or is radioed - see
// DCO_RadioShare.c) a big enough enemy cluster calls its own fire mission on the
// cluster centroid via the engine's artillery-support message. A per-battery
// cooldown + a minimum-cluster gate stand in for "approval" until the AI commander
// layer (DCO_Commander.c) mediates it; it won't shell a target too close to itself.
//
// The piece: a crewed entity carrying a MortarMuzzleComponent, found via the
// members' compartment vehicle. Ammo type is left at the message default.
//
// modded SCR_AIGroupUtilityComponent fragment in Artillery/ (sorts before the QRF
// tick that calls DCO_UpdateArtillery). Server-only, throttled, default OFF. Still
// foundational - validate in-engine that the support message actually fires the tube.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastArtyTime			= -1;	// last evaluation time (throttle)
	protected float	m_fDCO_ArtyCooldownUntil	= -1;	// no new fire mission before this world time (ms)

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). If this group
	// crews a mortar and sees a worthwhile enemy cluster, call a fire mission on it.
	void DCO_UpdateArtillery()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		// Runs when artillery is enabled, or when "utilize static weapons" (asset-use)
		// is on. In the latter case the fire logic is only reached if the group really
		// crews a mortar (DCO_ArtyFindCrewedMortar returns null otherwise), so manning a
		// tube via asset-use makes it fire instead of just sitting there - mortars are
		// indirect and don't auto-engage like an MG.
		bool artilleryActive = cfg.m_bEnableArtillery || cfg.m_bEnableAssetUse;
		if (!artilleryActive || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastArtyTime >= 0 && (now - m_fDCO_LastArtyTime) < cfg.m_fArtyCheckSec * 1000.0)
			return;
		m_fDCO_LastArtyTime = now;

		if (m_fDCO_ArtyCooldownUntil >= 0 && now < m_fDCO_ArtyCooldownUntil)
			return;

		// Find the mortar this group crews.
		IEntity mortar = DCO_ArtyFindCrewedMortar();
		if (!mortar)
			return;

		// Enemy cluster: need at least m_iArtyMinTargets perceived enemies; target = their centroid.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		vector sum = vector.Zero;
		int count = 0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			sum = sum + t.GetOrigin();
			count++;
		}
		if (count < cfg.m_iArtyMinTargets)
			return;

		vector target = sum / (float)count;

		// Don't drop rounds on top of ourselves.
		IEntity leader = m_Owner.GetLeaderEntity();
		if (leader && vector.DistanceSq(target, leader.GetOrigin()) < cfg.m_fArtyMinRange * cfg.m_fArtyMinRange)
			return;

		// Construct directly: this message type has no static Create() factory, and
		// mailbox.CreateMessage(TypeName) is obsolete in 1.7. `new` is the path the
		// Create() wrappers use anyway.
		SCR_AIMessage_ArtillerySupport msg = new SCR_AIMessage_ArtillerySupport();
		if (!msg)
			return;
		msg.m_ArtilleryEntity = mortar;
		msg.m_vTargetPos = target;
		msg.m_fPriorityLevel = cfg.m_fArtyPriority;
		m_Mailbox.RequestBroadcast(msg);

		m_fDCO_ArtyCooldownUntil = now + cfg.m_fArtyCooldownSec * 1000.0;

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("ARTY", leader, string.Format("fire mission on %1 (%2 targets)", target, count));
	}

	// The mortar this group crews (a compartment vehicle with a MortarMuzzleComponent),
	// or null if the group doesn't operate one.
	protected IEntity DCO_ArtyFindCrewedMortar()
	{
		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			IEntity veh = CompartmentAccessComponent.GetVehicleIn(ent);
			if (!veh)
				continue;
			if (veh.FindComponent(MortarMuzzleComponent))
				return veh;
		}
		return null;
	}
}
