// Defensive hold. A group flagged as a defender (per-group "25th DCO Defend" GM
// attribute) holds its position and orients its defence toward the nearest perceived
// enemy instead of advancing into the open - the engine-grounded "defensive AI" piece
// of the garrison goal: hold a point (building, chokepoint, objective) and face the
// threat. Full building-interior garrison still needs the nav work; this is the first cut.
//
// Wire-in: build a SCR_AIMessage_Defend.Create(dir, angularRange, ...) and broadcast it
// via the group mailbox (same pattern as flee / illum). Issued once per contact, re-armed
// when the threat clears.
//
// modded SCR_AIGroupUtilityComponent fragment. "Defend" sorts before "QRF" so DCO_UpdateDefend
// is defined before the tick that calls it; the filename sorts before
// DCO_DefendEditorAttributes.c (an external caller of these accessors). Server-only,
// per-group opt-in.
//
// Worth tuning in-engine: the angular-range units (assumed radians, PI = 180-deg front)
// and the priority value - both plain floats, so the call is sound; the values may not be ideal.
modded class SCR_AIGroupUtilityComponent
{
	protected bool	m_bDCO_IsDefender		= false;
	protected float	m_fDCO_LastDefendTime	= -1;
	protected bool	m_bDCO_DefendIssued		= false;

	protected static const float DCO_DEFEND_CHECK_INTERVAL_MS	= 5000.0;
	protected static const float DCO_DEFEND_ARC_RAD				= 3.14159;	// ~180 deg defensive front (radians)
	protected static const float DCO_DEFEND_PRIORITY			= 1.0;

	// GM attribute accessors (set from DCO_DefendEditorAttributes.c).
	bool DCO_IsDefender()
	{
		return m_bDCO_IsDefender;
	}

	void DCO_SetDefender(bool enable)
	{
		m_bDCO_IsDefender = enable;
		if (!enable)
			m_bDCO_DefendIssued = false;	// disarming clears the latch so it can re-arm later
	}

	void DCO_UpdateDefend()
	{
		if (!Replication.IsServer())
			return;

		if (!m_bDCO_IsDefender || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		// A mounted crew defends by holding in its vehicle; a hold/orient order would make
		// it disembark to take foot positions. Leave vehicle defenders alone.
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastDefendTime >= 0 && (now - m_fDCO_LastDefendTime) < DCO_DEFEND_CHECK_INTERVAL_MS)
			return;
		m_fDCO_LastDefendTime = now;

		// Nearest perceived enemy = the direction to defend toward. No contact: re-arm and wait.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
		{
			m_bDCO_DefendIssued = false;
			return;
		}

		if (m_bDCO_DefendIssued)
			return;	// already holding/facing the current contact

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector leaderPos = leader.GetOrigin();

		IEntity nearest;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float dSq = vector.DistanceSq(t.GetOrigin(), leaderPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				nearest = t;
			}
		}
		if (!nearest)
			return;

		vector dir = nearest.GetOrigin() - leaderPos;
		dir[1] = 0;
		if (dir.LengthSq() < 0.01)
			return;
		dir.Normalize();

		SCR_AIMessage_Defend msg = SCR_AIMessage_Defend.Create(dir, DCO_DEFEND_ARC_RAD, false, DCO_DEFEND_PRIORITY, null, null);
		if (!msg)
			return;

		m_Mailbox.RequestBroadcast(msg);
		m_bDCO_DefendIssued = true;
	}
}
