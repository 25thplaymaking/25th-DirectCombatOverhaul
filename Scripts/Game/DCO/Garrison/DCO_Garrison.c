// Building garrison. Turns a defender group near a building into a garrison: it spreads
// its members around the building's footprint (so they hold windows/corners rather than
// bunching), then the Defend layer (DCO_DefendComponent) orients them at the threat -
// together, a defendable occupied building.
//
// Tied to the defender flag (DCO_IsDefender, set by a Defend task zone / role preset / GM
// attribute) so it's intentional and never hijacks a moving group. A defender within
// m_fGarrisonRange of a building distributes its members once (latched on that building),
// re-arming if the building changes or the group leaves. Static-gun manning (DCO_AssetUse)
// still applies inside.
//
// modded SCR_AIGroupUtilityComponent fragment. "Garrison" sorts after "CQB" and "Defend"
// (so it can reuse DCO_CqbFindBuildingNear and read DCO_IsDefender) and before "QRF" (so
// DCO_UpdateGarrison is defined before the tick that calls it). On-foot, server-only, default OFF.
//
// Members prefer the building's baked AI firing/cover positions (window/corner smart-action
// points, via QueryEntitiesBySphere + AISmartActionComponent) and fall back to a navmesh
// ring when none are found. The smart-action route uses the same query path as DCO_AssetUse,
// not a GameSystem retrieval (AISmartActionSystem is a GameSystem, and World.FindSystem only
// returns WorldSystems). An engine PerformAction "occupy" message is a possible later refinement.
modded class SCR_AIGroupUtilityComponent
{
	protected float		m_fDCO_LastGarrisonTime	= -1;
	protected IEntity	m_eDCO_GarrisonBuilding;	// the building we have distributed onto (latch)

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). A defender near a
	// building occupies it.
	void DCO_UpdateGarrison()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableGarrison || !m_Owner || !m_Mailbox)
			return;

		// Only defenders garrison (intentional; never grabs a moving/attacking group).
		if (!DCO_IsDefender())
		{
			m_eDCO_GarrisonBuilding = null;
			return;
		}

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastGarrisonTime >= 0 && (now - m_fDCO_LastGarrisonTime) < cfg.m_fGarrisonCheckSec * 1000.0)
			return;
		m_fDCO_LastGarrisonTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector pos = leader.GetOrigin();

		// Nearest building to occupy (reuses the CQB building-ID helper).
		IEntity building = DCO_CqbFindBuildingNear(world, pos, cfg.m_fGarrisonRange);
		if (!building)
		{
			m_eDCO_GarrisonBuilding = null;	// nothing in range - hold in the open (Defend handles it)
			return;
		}

		// Already distributed onto this building: leave them (Defend keeps them facing out).
		if (building == m_eDCO_GarrisonBuilding)
			return;
		m_eDCO_GarrisonBuilding = building;

		DCO_GarrisonDistribute(leader, building, cfg);

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("GARRISON", leader, string.Format("occupying building at %1", building.GetOrigin()));
	}

	// Reused query buffer for the building's baked AI firing/cover (smart-action) point entities.
	protected ref array<IEntity> m_aDCO_GarrisonSAPoints;

	// Spread the members across the building. Prefers the building's baked AI firing/cover
	// positions (window/corner smart-action points) so defenders hold real firing spots,
	// and falls back to a navmesh ring when none are found (or the option is off).
	protected void DCO_GarrisonDistribute(IEntity leader, IEntity building, DCO_MoraleSettings cfg)
	{
		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
		vector center = building.GetOrigin();

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		int n = agents.Count();
		if (n <= 0)
			return;

		// Baked firing points first (if enabled). An empty result just means the ring fallback.
		array<vector> firingPts = {};
		if (cfg.m_bGarrisonUseSmartActions)
			DCO_GarrisonCollectSmartActions(center, cfg.m_fGarrisonRange, firingPts);

		for (int i = 0; i < n; i++)
		{
			AIAgent a = agents[i];
			if (!a)
				continue;

			vector cand;
			if (i < firingPts.Count())
			{
				cand = firingPts[i];	// a real window/cover firing position
			}
			else
			{
				float ang = (i / (float)n) * 6.2831853;	// fall back to the geometric ring
				cand = center + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fGarrisonRingRadius;
				DCO_GarrisonSnapNav(pf, cand, cand);
			}

			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, cand, EMovementType.WALK, false, null);
			if (msg)
			{
				msg.SetReceiver(a);
				m_Mailbox.RequestBroadcast(msg, a);
			}
		}
	}

	// Collect the world positions of accessible AI firing/cover (smart-action) points around
	// the building, via QueryEntitiesBySphere + AISmartActionComponent (same path as DCO_AssetUse).
	protected void DCO_GarrisonCollectSmartActions(vector center, float range, out array<vector> outPts)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		if (!m_aDCO_GarrisonSAPoints)
			m_aDCO_GarrisonSAPoints = {};
		m_aDCO_GarrisonSAPoints.Clear();

		world.QueryEntitiesBySphere(center, range, DCO_GarrisonSACollect);

		foreach (IEntity e : m_aDCO_GarrisonSAPoints)
		{
			if (e)
				outPts.Insert(e.GetOrigin());
		}
		m_aDCO_GarrisonSAPoints.Clear();
	}

	// QueryEntitiesBySphere callback: collect entities with an accessible smart action (an
	// unoccupied firing/cover point). Returns true to continue.
	protected bool DCO_GarrisonSACollect(IEntity e)
	{
		if (!e)
			return true;
		AISmartActionComponent sa = AISmartActionComponent.Cast(e.FindComponent(AISmartActionComponent));
		if (sa && sa.IsActionAccessible())
			m_aDCO_GarrisonSAPoints.Insert(e);
		return true;
	}

	// Snap a candidate onto the nearest walkable point, vertically tight so members stay on
	// the building's floor rather than being pulled onto a roof.
	protected void DCO_GarrisonSnapNav(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;
		vector corrected;
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(6, 2, 6), corrected))
			outPos = corrected;
	}
}
