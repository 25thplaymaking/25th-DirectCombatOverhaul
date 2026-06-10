// Vehicle combat positioning. The only verified AI vehicle lever is a move order
// (no scriptable hull rotate - see DCO_VehicleArmor.c), so both behaviours are
// expressed as repositioning:
//   - Seek a firing position: a crew that perceives an enemy but has no line of
//     sight ray-samples a ring around itself for the nearest reachable spot with
//     clear LOS, and drives there to engage.
//   - Back off to cover: when threat is high and back-off is enabled, it instead
//     moves to the nearest spot that breaks LOS (hull-down / behind terrain).
// Complements DCO_VehicleArmor (which angles the front at the threat).
//
// modded SCR_AIGroupUtilityComponent fragment in Armor/ (sorts before the QRF
// EvaluateActivity tick that calls DCO_UpdateVehicleCombat). Server-only,
// throttled, re-orders only when the firing solution changes. Default OFF.
// Tune ring radius / sample count if hull-down positioning looks coarse.
modded class SCR_AIGroupUtilityComponent
{
	protected float		m_fDCO_LastVehCombatTime	= -1;
	protected vector	m_vDCO_LastVehCombatOrder;
	protected bool		m_bDCO_HasVehCombatOrder	= false;
	protected float		m_fDCO_VehBackoffSince		= -1;	// world time (ms) the vehicle entered hull-down (peek timer)

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Repositions a
	// vehicle crew for LOS, or to cover when heavily threatened.
	void DCO_UpdateVehicleCombat()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleCombat || !m_Owner || !m_Perception)
			return;

		// Yield to the section controller: while a doctrine intent is active, the section owns this crew's
		// movement (the firing-position move would fight the doctrinal order). Checked via the plain static
		// helper so it resolves regardless of folder load order (this fragment loads before DCO/Convoy/).
		if (cfg.m_bEnableVehicleDoctrine && DCO_VehicleDoctrineUtil.IsUnderDoctrineControl(this))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastVehCombatTime >= 0 && (now - m_fDCO_LastVehCombatTime) < cfg.m_fVehicleCombatCheckSec * 1000.0)
			return;
		m_fDCO_LastVehCombatTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		// Only for a crew actually in a vehicle.
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(leader);
		if (!vehicle)
			return;
		vector vehPos = vehicle.GetOrigin();

		// Nearest perceived enemy.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;
		vector enemy;
		bool hasEnemy = false;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float d = vector.DistanceSq(t.GetOrigin(), vehPos);
			if (d < bestSq)
			{
				bestSq = d;
				enemy = t.GetOrigin();
				hasEnemy = true;
			}
		}
		if (!hasEnemy)
			return;

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
		if (!pf)
			return;

		float h = cfg.m_fVehicleSightHeight;
		bool heavyThreat = GetThreatMeasure() >= cfg.m_fVehicleBackoffThreat;
		bool wantCover = heavyThreat && cfg.m_bVehicleBackoff;

		bool losNow = !DCO_VehLosBlocked(pf, vehPos, enemy, h);

		// Peek-and-shoot: don't let it hull-down and sit blind forever. After holding
		// in cover for m_fVehicleBackoffHoldSec it pops back to a firing position to
		// keep fighting, then can break again next cycle.
		if (wantCover && !losNow)
		{
			if (m_fDCO_VehBackoffSince < 0)
				m_fDCO_VehBackoffSince = now;
			if ((now - m_fDCO_VehBackoffSince) >= cfg.m_fVehicleBackoffHoldSec * 1000.0)
			{
				wantCover = false;				// hold's over -> return to a shooting position
				m_fDCO_VehBackoffSince = -1;
			}
		}
		else
		{
			m_fDCO_VehBackoffSince = -1;			// not currently hiding
		}

		// If we already have what we want, do nothing (anti-spam).
		if (!wantCover && losNow)
			return;	// can see/shoot - hold and fight
		if (wantCover && !losNow)
			return;	// hiding within the hold window - hold

		// Sample a ring for the nearest spot satisfying the goal (LOS to engage, or no-LOS to hide).
		vector best;
		bool found = false;
		float bestDist = 0;
		int n = cfg.m_iVehicleSampleCount;
		for (int i = 0; i < n; i++)
		{
			float ang = (i / (float)n) * 6.2831853;
			vector cand = vehPos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fVehicleRepositionRadius;
			vector snapped;
			if (!pf.GetClosestPositionOnNavmesh(cand, Vector(10, 4, 10), snapped))
				continue;
			bool candBlocked = DCO_VehLosBlocked(pf, snapped, enemy, h);
			bool ok;
			if (wantCover)
				ok = candBlocked;		// hide: want LOS broken
			else
				ok = !candBlocked;		// engage: want LOS clear
			if (!ok)
				continue;
			float d = vector.DistanceSq(snapped, vehPos);
			if (!found || d < bestDist)
			{
				bestDist = d;
				best = snapped;
				found = true;
			}
		}

		if (!found)
			return;

		// Re-order only if the new spot differs from the last order (avoid re-pathing every tick).
		if (m_bDCO_HasVehCombatOrder && vector.DistanceSq(best, m_vDCO_LastVehCombatOrder) < cfg.m_fVehicleReorderDist * cfg.m_fVehicleReorderDist)
			return;

		m_vDCO_LastVehCombatOrder = best;
		m_bDCO_HasVehCombatOrder = true;
		DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, best, m_Mailbox);

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("VEHCOMBAT", leader, string.Format("reposition to %1 (cover=%2)", best, wantCover));
	}

	// True if the line from the vehicle to the enemy (both at sight height) is blocked.
	protected bool DCO_VehLosBlocked(AIPathfindingComponent pf, vector fromPos, vector enemyPos, float h)
	{
		vector hit;
		return pf.RayTrace(fromPos + Vector(0, h, 0), enemyPos + Vector(0, h, 0), hit);
	}
}
