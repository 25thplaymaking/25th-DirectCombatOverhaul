// Vehicle armour angling. When a crewed AI vehicle is in contact and showing a
// weak side/rear to the nearest enemy, turn its front (thickest armour) at that
// enemy so AI tanks stop dying to broadsides they never reorient against.
//
// Reforger gives us no scriptable in-place hull rotate for AI - the driver's
// heading is engine-internal, and the only verified order primitive is a move.
// So we angle the hull by ordering the crew to drive toward the nearest enemy
// (the path swings the front around) and cancel once the front is inside the
// frontal arc. Side effect: the vehicle also noses forward a little each time.
// Best available lever until a driver combat-move node is exposed.
//
// modded SCR_AIGroupUtilityComponent fragment (a crew is an SCR_AIGroup). Armor/
// sorts before QRF/ so DCO_UpdateVehicleArmor is defined before the shared
// EvaluateActivity tick that calls it. Runs as the outermost modded layer (25thCRX
// loads after CRX + Skarris), so the DCO order wins. Server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastVehicleArmorTime	= -1;
	protected bool	m_bDCO_VehicleReorienting	= false;
	protected vector	m_vDCO_LastVehicleOrderPos;		// last enemy pos the hull was ordered toward
	protected bool		m_bDCO_HasVehicleOrderPos	= false;

	void DCO_UpdateVehicleArmor()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleArmor || !m_Owner)
			return;

		// Yield to the section controller: while a doctrine intent is active, the section owns this crew's
		// movement (the armour-angling move would fight the doctrinal order). Checked via the plain static
		// helper so it resolves regardless of folder load order (this fragment loads before DCO/Convoy/).
		if (cfg.m_bEnableVehicleDoctrine && DCO_VehicleDoctrineUtil.IsUnderDoctrineControl(this))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastVehicleArmorTime >= 0 && (now - m_fDCO_LastVehicleArmorTime) < cfg.m_fVehicleCheckIntervalSec * 1000.0)
			return;
		m_fDCO_LastVehicleArmorTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		// Only acts if the group leader is actually crewing a vehicle.
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(leader);
		if (!vehicle)
			return;

		vector vehPos = vehicle.GetOrigin();

		IEntity enemy;
		vector enemyPos;
		if (!DCO_VehicleNearestEnemy(vehPos, enemy, enemyPos))
		{
			// No contact: drop any reorient order we issued so CRX/base resumes normal control.
			if (m_bDCO_VehicleReorienting)
				DCO_VehicleCancelReorient();
			return;
		}

		// Too close to meaningfully angle (the enemy is basically on top of us) - leave it to CRX.
		if (vector.Distance(vehPos, enemyPos) < cfg.m_fVehicleMinEngageRange)
			return;

		// Is the threat already within the hull's frontal arc? (front presented = strongest armour out)
		vector fwd = vehicle.GetTransformAxis(2);
		fwd[1] = 0;
		vector toEnemy = enemyPos - vehPos;
		toEnemy[1] = 0;
		if (fwd.LengthSq() < 0.01 || toEnemy.LengthSq() < 0.01)
			return;
		fwd.Normalize();
		toEnemy.Normalize();

		float dot = vector.Dot(fwd, toEnemy);
		float cosArc = Math.Cos(cfg.m_fVehicleFrontalArcDeg * 0.0174533);	// deg -> rad (pi/180), literal = no API dependency
		// Hysteresis boundary: reorienting only STARTS once the threat is beyond arc+deadband.
		float cosStart = Math.Cos((cfg.m_fVehicleFrontalArcDeg + cfg.m_fVehicleReorientDeadbandDeg) * 0.0174533);

		if (dot >= cosArc)
		{
			// Front is already presented to the threat - stop driving toward it and hold (once).
			if (m_bDCO_VehicleReorienting)
				DCO_VehicleCancelReorient();
			return;
		}

		// Inside the deadband (between the arc and the start boundary) and not already turning: leave it be.
		// This stops the hull oscillating around the arc edge and re-ordering every tick (the driving jitter).
		if (dot >= cosStart && !m_bDCO_VehicleReorienting)
			return;

		// Showing a weak side/rear. Re-issue the face order ONLY when we aren't already turning, OR the enemy
		// has moved far enough that the prior order is stale - never re-broadcast the same move every tick.
		bool needOrder = !m_bDCO_VehicleReorienting;
		if (!needOrder && m_bDCO_HasVehicleOrderPos)
			needOrder = vector.DistanceSq(enemyPos, m_vDCO_LastVehicleOrderPos) > (cfg.m_fVehicleReorderDist * cfg.m_fVehicleReorderDist);
		if (!needOrder)
			return;

		AIAgent leaderAgent = m_Owner.GetLeaderAgent();
		AICommunicationComponent comms = m_Mailbox;
		if (!leaderAgent || !comms)
			return;

		// Vehicle-aware move: the crew DRIVES to present its front (useVehicles) instead of an on-foot move
		// order, which would make the crew DISMOUNT and walk toward the enemy (random-disembark fix). We are
		// guaranteed mounted here (leader is crewing `vehicle`), so this always takes the drive path.
		DCO_VehicleUtil.OrderGroupMoveToEntity(m_Owner, enemy, comms);
		m_bDCO_VehicleReorienting = true;
		m_vDCO_LastVehicleOrderPos = enemyPos;
		m_bDCO_HasVehicleOrderPos = true;
	}

	protected void DCO_VehicleCancelReorient()
	{
		m_bDCO_VehicleReorienting = false;

		if (!m_Owner)
			return;

		AIAgent leaderAgent = m_Owner.GetLeaderAgent();
		AICommunicationComponent comms = m_Mailbox;
		if (leaderAgent && comms)
			SCR_AIMessageHandling.SendCancelMessage(leaderAgent, null, comms);
	}

	// Nearest perceived enemy entity + position from the vehicle. False if none perceived.
	protected bool DCO_VehicleNearestEnemy(vector fromPos, out IEntity outEnemy, out vector outPos)
	{
		if (!m_Perception)
			return false;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return false;

		float bestSq = 1000000000.0;	// ~31 km^2 cap; perceived targets are well within this
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float dSq = vector.DistanceSq(t.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				outEnemy = t;
				outPos = t.GetOrigin();
			}
		}
		return outEnemy != null;
	}
}
