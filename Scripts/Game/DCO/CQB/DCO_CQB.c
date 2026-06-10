// CQB / building assault. The building-ID is the part most mods can't do; here an
// entity that casts to Building (or carries a SCR_DestructibleBuildingComponent)
// counts as a building, which unblocks the rest:
//   - Building ID: DCO_CqbFindBuildingNear(pos) returns the nearest building.
//   - Assault: when a group is in contact and the nearest enemy is co-located with a
//     building, push to the enemy's position instead of trading shots from outside -
//     the engine paths the group in through the entry, so they actually clear rooms.
// Full room-by-room sequencing builds on this later; this delivers the detection plus
// "enter and close on the enemy inside".
//
// modded SCR_AIGroupUtilityComponent fragment in CQB/ (sorts before the QRF tick that
// calls DCO_UpdateCQB). On-foot, server-only, throttled, re-orders only as the enemy
// moves. Default OFF and still experimental - check the assault push reads well.
modded class SCR_AIGroupUtilityComponent
{
	protected float			m_fDCO_LastCqbTime		= -1;
	protected vector		m_vDCO_LastCqbOrder;
	protected bool			m_bDCO_HasCqbOrder		= false;
	protected float			m_fDCO_CqbOrderTime		= -1;	// world time of the LAST building-assault order (transient; for the standoff yield)
	protected ref array<IEntity>	m_aDCO_CqbBuildings;	// scratch for the building query

	// World time of the most recent building-assault order, or -1. The standoff layer
	// reads this: a group that just pushed into a building yields the engagement floor
	// for a short window so it closes and clears the room instead of holding off.
	float DCO_GetLastCqbOrderTime()
	{
		return m_fDCO_CqbOrderTime;
	}

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Pushes a contacted
	// group into a building to close on an enemy sheltering inside.
	void DCO_UpdateCQB()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableCQB || !m_Owner || !m_Perception)
			return;

		// Yield to the methodical clearer: while the CQB-clear state machine owns this
		// group, its orders win (the reactive push would fight them). Plain-static gate
		// so it resolves regardless of folder order.
		if (cfg.m_bEnableCqbClear && DCO_CqbClearUtil.IsClearingActive(this))
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastCqbTime >= 0 && (now - m_fDCO_LastCqbTime) < cfg.m_fCqbCheckSec * 1000.0)
			return;
		m_fDCO_LastCqbTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector lead = leader.GetOrigin();

		// Nearest perceived enemy.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;
		vector enemyPos;
		bool hasEnemy = false;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float d = vector.DistanceSq(t.GetOrigin(), lead);
			if (d < bestSq)
			{
				bestSq = d;
				enemyPos = t.GetOrigin();
				hasEnemy = true;
			}
		}
		if (!hasEnemy)
			return;

		// Only assault when reasonably close; long-range contacts aren't a building assault.
		if (vector.DistanceSq(enemyPos, lead) > cfg.m_fCqbEngageRange * cfg.m_fCqbEngageRange)
			return;

		// Is the enemy co-located with a building?
		IEntity building = DCO_CqbFindBuildingNear(world, enemyPos, cfg.m_fCqbBuildingScan);
		if (!building)
			return;

		// Already near the enemy? Then we're inside/clearing - don't re-order.
		if (vector.DistanceSq(enemyPos, lead) < cfg.m_fCqbArriveDist * cfg.m_fCqbArriveDist)
			return;

		// Re-order only as the enemy moves meaningfully.
		if (m_bDCO_HasCqbOrder && vector.DistanceSq(enemyPos, m_vDCO_LastCqbOrder) < cfg.m_fCqbReorderDist * cfg.m_fCqbReorderDist)
			return;

		m_vDCO_LastCqbOrder = enemyPos;
		m_bDCO_HasCqbOrder = true;
		m_fDCO_CqbOrderTime = now;	// stamp so the standoff layer yields while we close
		// Push to the enemy; the engine paths the group in through the building's entry.
		DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, enemyPos, m_Mailbox);

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("CQB", leader, string.Format("assaulting building at %1 (enemy inside)", enemyPos));
	}

	// Nearest building within radius of pos, or null. A building casts to Building or
	// carries a SCR_DestructibleBuildingComponent.
	protected IEntity DCO_CqbFindBuildingNear(BaseWorld world, vector pos, float radius)
	{
		m_aDCO_CqbBuildings = {};
		world.QueryEntitiesBySphere(pos, radius, DCO_CqbCollect);

		IEntity best;
		float bestSq = radius * radius + 1;
		foreach (IEntity b : m_aDCO_CqbBuildings)
		{
			if (!b)
				continue;
			float d = vector.DistanceSq(b.GetOrigin(), pos);
			if (d < bestSq)
			{
				bestSq = d;
				best = b;
			}
		}
		return best;
	}

	// QueryEntitiesBySphere callback: collect building entities. Returns true to continue.
	protected bool DCO_CqbCollect(IEntity e)
	{
		if (!e)
			return true;
		if (Building.Cast(e) || e.FindComponent(SCR_DestructibleBuildingComponent))
			m_aDCO_CqbBuildings.Insert(e);
		return true;
	}
}
