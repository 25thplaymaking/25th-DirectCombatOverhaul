// Ambush / hold-fire, per group.
//
// A group flagged with the "25th DCO Ambush" GM attribute holds fire until an
// enemy enters its trigger range, then the whole element opens up at once.
// Reuses the QRF/fake-surrender perception + range-check pattern. Server-only.
//
// This is a modded SCR_AIGroupUtilityComponent fragment, sharing the class with
// morale/reinforcement/QRF. Folder order matters: "Ambush" sorts before "QRF",
// so DCO_UpdateAmbush is defined before the shared EvaluateActivity override (in
// DCO_GroupQRF.c) that calls it - Enforce resolves cross-fragment calls in file
// order. Likewise the filename sorts before DCO_AmbushEditorAttributes.c so the
// accessors below compile before that caller. Don't rename it to start after 'E'.
modded class SCR_AIGroupUtilityComponent
{
	protected bool	m_bDCO_IsAmbusher		= false;
	protected float	m_fDCO_AmbushRange		= 50.0;
	protected bool	m_bDCO_AmbushSprung		= false;
	protected float	m_fDCO_LastAmbushTime	= -1;

	protected static const float DCO_AMBUSH_CHECK_INTERVAL_MS	= 1000.0;
	protected static const float DCO_AMBUSH_HOLD_NOFIRE_SEC		= 5.0;	// re-applied so the hold never lapses

	// GM attribute accessors (set from DCO_AmbushEditorAttributes.c).
	bool DCO_IsAmbusher()
	{
		return m_bDCO_IsAmbusher;
	}

	void DCO_SetAmbusher(bool enable)
	{
		m_bDCO_IsAmbusher = enable;
		if (!enable)
			m_bDCO_AmbushSprung = false;	// disarming clears the sprung latch so it can be re-armed
	}

	float DCO_GetAmbushRange()
	{
		return m_fDCO_AmbushRange;
	}

	void DCO_SetAmbushRange(float range)
	{
		m_fDCO_AmbushRange = range;
	}

	// Spring the ambush immediately - called by a paired AMBUSH_TRIGGER task zone
	// when an enemy walks into a detached kill zone. Releases the hold and latches
	// as sprung so it won't be re-held.
	void DCO_SpringAmbush()
	{
		if (!m_Owner || m_bDCO_AmbushSprung)
			return;
		DCO_SetGroupNoFireTime(0);
		m_bDCO_AmbushSprung = true;
	}

	// Per-tick check (throttled): spring if an enemy is in range, otherwise keep
	// the no-fire hold topped up.
	void DCO_UpdateAmbush()
	{
		if (!Replication.IsServer())
			return;

		if (!m_bDCO_IsAmbusher || m_bDCO_AmbushSprung || !m_Owner)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastAmbushTime >= 0 && (now - m_fDCO_LastAmbushTime) < DCO_AMBUSH_CHECK_INTERVAL_MS)
			return;
		m_fDCO_LastAmbushTime = now;

		bool enemyInRange = DCO_AmbushEnemyInRange();

		if (enemyInRange)
		{
			// Spring the ambush: release the hold so the group can fire, then stop managing it.
			DCO_SetGroupNoFireTime(0);
			m_bDCO_AmbushSprung = true;
		}
		else
		{
			// Still waiting: keep the hold topped up so it never lapses between checks.
			DCO_SetGroupNoFireTime(DCO_AMBUSH_HOLD_NOFIRE_SEC);
		}
	}

	// True if any perceived enemy is within trigger range of the group leader.
	protected bool DCO_AmbushEnemyInRange()
	{
		if (!m_Perception)
			return false;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return false;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return false;

		vector leaderPos = leader.GetOrigin();
		float rangeSq = m_fDCO_AmbushRange * m_fDCO_AmbushRange;

		foreach (IEntity t : targets)
		{
			if (t && vector.DistanceSq(t.GetOrigin(), leaderPos) <= rangeSq)
				return true;
		}
		return false;
	}

	// Apply a weapon no-fire time to every member (positive = held, 0 = released).
	protected void DCO_SetGroupNoFireTime(float seconds)
	{
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

			SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
			if (cc)
				cc.SetWeaponNoFireTime(seconds);
		}
	}
}
