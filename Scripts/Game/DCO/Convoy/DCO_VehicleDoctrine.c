// Vehicle doctrine: per-crew intent storage + executor. One modded SCR_AIGroupUtilityComponent
// fragment that both stores the intent the section controller (DCO_VehicleSectionController, a
// plain singleton) writes for this crew and executes it each tick, using only navmesh-validated
// move orders plus the proven dismount/board levers.
//
// Storage and executor live together because Enforce resolves cross-fragment calls within a
// modded class in file order: keeping the accessors (DCO_GetVehicleIntent, DCO_VehNoteMovement,
// ...) in the same fragment that calls them avoids an "earlier fragment calls later-defined
// method" failure. External callers reach these via plain-class/plain-static paths
// (DCO_VehicleSectionController, DCO_ConvoyPreset, and the
// DCO_VehicleDoctrineUtil.IsUnderDoctrineControl helper used by the Armor/Combat gates), which
// resolve globally and are order-independent.
//
// The "Convoy" folder sorts before "QRF" so DCO_UpdateVehicleDoctrine is defined before
// DCO_GroupQRF.EvaluateActivity calls it.
//
// Safety contract: every move is a vehicle-aware order to a reachable, navmesh-snapped position
// (the geometry helpers fail rather than return an unsafe point); if a drill can't find a safe
// spot the crew holds. Out of combat (role NONE) the executor issues a single cancel and goes
// silent so vanilla navmesh fully owns movement again. Server-side; the drills are throttled to
// the controller's cadence.
modded class SCR_AIGroupUtilityComponent
{
	// --- intent storage (written by the controller, read by the executor below) ---
	protected ref DCO_VehicleIntent	m_DCO_VehIntent;
	protected vector				m_vDCO_VehLastPos;			// disabled watchdog: last sampled vehicle position
	protected float					m_fDCO_VehLastMoveTime	= -1;
	protected bool					m_bDCO_VehDisabled		= false;
	protected int					m_iDCO_ConvoyId			= -1;	// -1 = untagged; set by DCO_ConvoyPreset at spawn

	// --- executor state ---
	protected EDCO_VehicleRole		m_eDCO_VehLastExecRole	= EDCO_VehicleRole.NONE;
	protected vector				m_vDCO_VehLastOrderPos;
	protected bool					m_bDCO_VehHasOrder;
	protected bool					m_bDCO_VehDismounted;
	protected float					m_fDCO_LastDoctrineExec	= -1;
	protected ref array<IEntity>	m_aDCO_ReembarkCandidates;

	// Current intent for this crew (created lazily; never null after first call).
	DCO_VehicleIntent DCO_GetVehicleIntent()
	{
		if (!m_DCO_VehIntent)
			m_DCO_VehIntent = new DCO_VehicleIntent();
		return m_DCO_VehIntent;
	}

	// Controller writes a role + optional target/enemy here.
	void DCO_SetVehicleIntent(EDCO_VehicleRole role, vector targetPos, bool hasTarget, vector enemyPos, bool hasEnemy, int sectionId, bool roadBlocked)
	{
		DCO_VehicleIntent it = DCO_GetVehicleIntent();
		it.m_eRole			= role;
		it.m_vTargetPos		= targetPos;
		it.m_bHasTarget		= hasTarget;
		it.m_vEnemyPos		= enemyPos;
		it.m_bHasEnemy		= hasEnemy;
		it.m_iSectionId		= sectionId;
		it.m_bRoadBlocked	= roadBlocked;
	}

	// True if this crew currently has an active (non-NONE) doctrine intent. The Armor/Combat gates reach
	// this via the plain static DCO_VehicleDoctrineUtil.IsUnderDoctrineControl (order-independent).
	bool DCO_HasActiveVehicleIntent()
	{
		return m_DCO_VehIntent && m_DCO_VehIntent.m_eRole != EDCO_VehicleRole.NONE;
	}

	// True if this group's leader is currently crewing a vehicle (cheap leader-only check).
	bool DCO_IsVehicleCrew()
	{
		if (!m_Owner)
			return false;
		IEntity leader = m_Owner.GetLeaderEntity();
		return leader && CompartmentAccessComponent.GetVehicleIn(leader) != null;
	}

	// Convoy tag (DCO_ConvoyPreset). -1 = untagged (proximity sectioning only).
	int DCO_GetConvoyId()
	{
		return m_iDCO_ConvoyId;
	}

	void DCO_SetConvoyId(int id)
	{
		m_iDCO_ConvoyId = id;
	}

	// Disabled watchdog: if the vehicle hasn't moved more than `epsilon` for `disabledSec` while under an
	// active move order, flag it disabled (stuck / immobilised).
	void DCO_VehNoteMovement(vector pos, float now, float epsilon, float disabledSec)
	{
		if (m_fDCO_VehLastMoveTime < 0)
		{
			m_vDCO_VehLastPos = pos;
			m_fDCO_VehLastMoveTime = now;
			m_bDCO_VehDisabled = false;
			return;
		}
		if (vector.Distance(pos, m_vDCO_VehLastPos) >= epsilon)
		{
			m_vDCO_VehLastPos = pos;
			m_fDCO_VehLastMoveTime = now;
			m_bDCO_VehDisabled = false;
		}
		else if ((now - m_fDCO_VehLastMoveTime) >= disabledSec * 1000.0)
		{
			m_bDCO_VehDisabled = true;
		}
	}

	bool DCO_VehIsDisabled()
	{
		return m_bDCO_VehDisabled;
	}

	void DCO_VehResetDisabled()
	{
		m_fDCO_VehLastMoveTime = -1;
		m_bDCO_VehDisabled = false;
	}

	// Per-crew executor. Called from DCO_GroupQRF.EvaluateActivity.
	void DCO_UpdateVehicleDoctrine()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleDoctrine || !m_Owner || !m_Mailbox)
			return;

		DCO_VehicleIntent it = DCO_GetVehicleIntent();

		// Handback (runs every tick, un-throttled, so a cleared role releases the crew promptly): on role
		// NONE issue one cancel so the engine resumes vanilla navmesh, re-embark if we dismounted, then idle.
		if (it.m_eRole == EDCO_VehicleRole.NONE)
		{
			if (m_eDCO_VehLastExecRole != EDCO_VehicleRole.NONE || m_bDCO_VehHasOrder)
			{
				AIAgent la = m_Owner.GetLeaderAgent();
				if (la)
					SCR_AIMessageHandling.SendCancelMessage(la, null, m_Mailbox);
				m_bDCO_VehHasOrder = false;
				DCO_VehResetDisabled();
				if (m_bDCO_VehDismounted)
				{
					DCO_TryReembark();
					m_bDCO_VehDismounted = false;
				}
			}
			m_eDCO_VehLastExecRole = EDCO_VehicleRole.NONE;
			return;
		}

		// Throttle the (more expensive) in-combat drills to the controller's cadence.
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastDoctrineExec >= 0 && (now - m_fDCO_LastDoctrineExec) < cfg.m_fVehDoctrineCheckSec * 1000.0)
			return;
		m_fDCO_LastDoctrineExec = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(leader);
		if (!vehicle)
		{
			// Crew is on foot (already dismounted) - leave it to the on-foot DCO layers.
			m_eDCO_VehLastExecRole = it.m_eRole;
			return;
		}
		vector vehPos = vehicle.GetOrigin();

		// Disabled watchdog: only meaningful once we've ordered a move.
		if (m_bDCO_VehHasOrder)
			DCO_VehNoteMovement(vehPos, now, cfg.m_fVehDoctrineDisabledEps, cfg.m_fVehDoctrineDisabledSec);

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
		if (!pf || !it.m_bHasEnemy)
		{
			m_eDCO_VehLastExecRole = it.m_eRole;
			return;
		}

		switch (it.m_eRole)
		{
			case EDCO_VehicleRole.LEAD:
			{
				if (it.m_bRoadBlocked)
					DCO_DrillHerringbone(pf, vehPos, it, cfg);
				else if (cfg.m_bVehDoctrineBounding)
					DCO_DrillBound(pf, vehPos, it, cfg);
				else
					DCO_DrillDriveThrough(pf, vehPos, it, cfg);
				break;
			}
			case EDCO_VehicleRole.TRAIL:
			{
				if (it.m_bRoadBlocked)
					DCO_DrillHerringbone(pf, vehPos, it, cfg);
				else
					DCO_DrillDriveThrough(pf, vehPos, it, cfg);
				break;
			}
			case EDCO_VehicleRole.ESCORT:
			{
				DCO_DrillEscort(pf, vehPos, it, cfg);
				break;
			}
			case EDCO_VehicleRole.OVERWATCH:
			{
				DCO_DrillOverwatch(pf, vehPos, it, cfg);
				break;
			}
			case EDCO_VehicleRole.DISMOUNT:
			case EDCO_VehicleRole.DESTROY:
			case EDCO_VehicleRole.RECOVER:
			{
				// Vehicle is out of the fight (stuck transport / dead vehicle): crew debuses to a base of fire.
				DCO_DrillDismount(pf, vehPos, it, cfg);
				break;
			}
		}

		m_eDCO_VehLastExecRole = it.m_eRole;
	}

	// Issue a vehicle-aware move to `pos`, but only if it differs from the last order by the reorder gate
	// (anti-thrash). Records the order for the disabled watchdog / handback and the debug target.
	protected void DCO_VehOrderMove(vector pos, float reorderDist)
	{
		if (m_bDCO_VehHasOrder && vector.DistanceSq(pos, m_vDCO_VehLastOrderPos) < reorderDist * reorderDist)
			return;
		DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, pos, m_Mailbox);
		m_vDCO_VehLastOrderPos = pos;
		m_bDCO_VehHasOrder = true;

		DCO_VehicleIntent it = DCO_GetVehicleIntent();
		it.m_vTargetPos = pos;
		it.m_bHasTarget = true;
	}

	// LEAD/TRAIL, road open, no bounding: accelerate out of the kill zone along the road, away from the enemy.
	protected void DCO_DrillDriveThrough(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		vector dest;
		if (DCO_VehicleDoctrineUtil.DriveThroughExit(pf, vehPos, it.m_vEnemyPos, 80.0, 30.0, dest))
			DCO_VehOrderMove(dest, cfg.m_fVehDoctrineReorderDist);
		// else: no safe exit, hold (do nothing).
	}

	// LEAD bound: move one bound length along the route away from the enemy (covered by the overwatch element).
	protected void DCO_DrillBound(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		vector dest;
		if (DCO_VehicleDoctrineUtil.DriveThroughExit(pf, vehPos, it.m_vEnemyPos, cfg.m_fVehDoctrineBoundDist, 30.0, dest))
			DCO_VehOrderMove(dest, cfg.m_fVehDoctrineReorderDist);
	}

	// LEAD/TRAIL, road blocked: pull to a herringbone shoulder (alternating side by section id parity) off
	// the traveled road, then hold as a base of fire (turret auto-engages).
	protected void DCO_DrillHerringbone(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		int side = -1;
		if (it.m_iSectionId % 2 == 0)
			side = 1;
		vector dest;
		if (DCO_VehicleDoctrineUtil.HerringboneShoulder(pf, vehPos, it.m_vEnemyPos, side, cfg.m_fVehDoctrineRoadBlockDist, dest))
			DCO_VehOrderMove(dest, cfg.m_fVehDoctrineReorderDist);
	}

	// ESCORT (gun truck): take a covered position with LOS to the enemy, pulled toward the road shoulder so
	// it never blocks the convoy (doctrine: armoured/escort vehicles must not halt in the traveled road).
	protected void DCO_DrillEscort(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		vector dest;
		if (DCO_VehicleDoctrineUtil.FindLosPosition(pf, vehPos, it.m_vEnemyPos, 25.0, 8, cfg.m_fVehicleSightHeight, false, dest))
		{
			vector shoulder;
			if (DCO_VehicleDoctrineUtil.HerringboneShoulder(pf, dest, it.m_vEnemyPos, 1, cfg.m_fVehDoctrineRoadBlockDist, shoulder))
				dest = shoulder;
			DCO_VehOrderMove(dest, cfg.m_fVehDoctrineReorderDist);
		}
		// else: hold position (turret still bears on the enemy).
	}

	// OVERWATCH: hold a position with LOS to the danger area so the turret covers the bounding element. If
	// we lack LOS, reposition a short distance to regain it; otherwise hold (no order = vanilla holds us).
	protected void DCO_DrillOverwatch(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		if (!DCO_VehicleDoctrineUtil.LosBlocked(pf, vehPos, it.m_vEnemyPos, cfg.m_fVehicleSightHeight))
			return;	// already have LOS: hold and cover
		vector dest;
		if (DCO_VehicleDoctrineUtil.FindLosPosition(pf, vehPos, it.m_vEnemyPos, 20.0, 8, cfg.m_fVehicleSightHeight, false, dest))
			DCO_VehOrderMove(dest, cfg.m_fVehDoctrineReorderDist);
	}

	// DISMOUNT/DESTROY/RECOVER: send the group to an off-road base-of-fire position on the covered side.
	// Because this is an on-foot move (useVehicles = false), the AI debuses and moves there - the deliberate
	// use of the disembark mechanism documented in DCO_VehicleUtil. Issued once per dismount.
	protected void DCO_DrillDismount(AIPathfindingComponent pf, vector vehPos, DCO_VehicleIntent it, DCO_MoraleSettings cfg)
	{
		if (m_bDCO_VehDismounted)
			return;
		vector dest;
		if (!DCO_VehicleDoctrineUtil.HerringboneShoulder(pf, vehPos, it.m_vEnemyPos, -1, cfg.m_fVehDoctrineRoadBlockDist, dest))
		{
			vector away = vehPos - it.m_vEnemyPos;
			away[1] = 0;
			if (away.LengthSq() < 0.01)
				return;
			away.Normalize();
			if (!DCO_VehicleDoctrineUtil.SnapToNavmesh(pf, vehPos + away * 12.0, dest))
				return;
		}
		// On-foot move (useVehicles = false) => dismount + move. DCO_VehicleUtil only has vehicle-aware
		// helpers, so build the on-foot move message directly (same factory, vehicle flag off).
		SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, dest, EMovementType.RUN, false, null);
		if (msg)
			m_Mailbox.RequestBroadcast(msg);
		m_bDCO_VehDismounted = true;
	}

	// Best-effort re-embark after a dismount once combat ends: order the leader to board the nearest vehicle
	// that has a free passenger seat. If none is found the group simply stays on foot (safe default).
	protected void DCO_TryReembark()
	{
		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		if (CompartmentAccessComponent.GetVehicleIn(leader))
			return;	// already mounted
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		m_aDCO_ReembarkCandidates = {};
		world.QueryEntitiesBySphere(leader.GetOrigin(), 30.0, DCO_ReembarkCollect);
		IEntity best;
		float bestSq = 1000000000.0;
		foreach (IEntity v : m_aDCO_ReembarkCandidates)
		{
			if (!v)
				continue;
			float dSq = vector.DistanceSq(v.GetOrigin(), leader.GetOrigin());
			if (dSq < bestSq) { bestSq = dSq; best = v; }
		}
		m_aDCO_ReembarkCandidates = null;
		if (!best)
			return;
		AIAgent la = m_Owner.GetLeaderAgent();
		if (la)
			SCR_AIMessageHandling.SendGetInMessage(la, best, EAICompartmentType.Cargo, null, m_Mailbox);
	}

	// QueryEntitiesBySphere callback: keep vehicles with a free passenger seat. Returns true to continue.
	protected bool DCO_ReembarkCollect(IEntity e)
	{
		if (!e)
			return true;
		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(e.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (mgr && mgr.HasFreeCompartmentOfTypes(SCR_BaseCompartmentManagerComponent.PASSENGER_COMPARTMENT_TYPES))
			m_aDCO_ReembarkCandidates.Insert(e);
		return true;
	}
}
