// Vehicle hijacking / commandeering. An on-foot group in contact commandeers the
// nearest empty drivable vehicle within range, for mobility, firepower, or escape.
// A candidate is a vehicle whose compartment manager is empty (GetOccupantCount() == 0)
// and has a free crew seat; boarding is ordered with SendGetInMessage(..., Pilot).
// (EAICompartmentType = {None=-1, Pilot, Turret, Cargo}.)
//
// modded SCR_AIGroupUtilityComponent fragment. "Hijack" sorts before "QRF" so
// DCO_UpdateVehicleHijack is defined before the tick that calls it. Server-only, default OFF.
// Worth validating in-engine: that the boarding order takes cleanly for an on-foot group,
// and that the empty/crew-seat filter excludes static turrets and crewed vehicles.
modded class SCR_AIGroupUtilityComponent
{
	protected float				m_fDCO_LastHijackTime	= -1;
	protected bool				m_bDCO_HijackOrdered	= false;
	protected ref array<IEntity>	m_aDCO_HijackCandidates;

	void DCO_UpdateVehicleHijack()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleHijack || !m_Owner || !m_Mailbox)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastHijackTime >= 0 && (now - m_fDCO_LastHijackTime) < cfg.m_fHijackCheckIntervalSec * 1000.0)
			return;
		m_fDCO_LastHijackTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		// Already crewing a vehicle - nothing to take; clear the latch so it can re-fire.
		if (CompartmentAccessComponent.GetVehicleIn(leader))
		{
			m_bDCO_HijackOrdered = false;
			return;
		}

		// Only act with a reason: a perceived enemy (wants mobility/firepower/escape).
		if (!m_Perception)
			return;
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
		{
			m_bDCO_HijackOrdered = false;
			return;
		}

		if (m_bDCO_HijackOrdered)
			return;	// already moving to board a vehicle

		// Collect empty drivable vehicles in range, pick the nearest.
		vector fromPos = leader.GetOrigin();
		if (!m_aDCO_HijackCandidates)
			m_aDCO_HijackCandidates = {};
		m_aDCO_HijackCandidates.Clear();

		world.QueryEntitiesBySphere(fromPos, cfg.m_fHijackRange, DCO_HijackCollect);

		IEntity best;
		float bestSq = cfg.m_fHijackRange * cfg.m_fHijackRange + 1.0;
		foreach (IEntity v : m_aDCO_HijackCandidates)
		{
			if (!v)
				continue;
			float dSq = vector.DistanceSq(v.GetOrigin(), fromPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = v;
			}
		}
		m_aDCO_HijackCandidates.Clear();

		if (!best)
			return;

		AIAgent leaderAgent = m_Owner.GetLeaderAgent();
		AICommunicationComponent comms = m_Mailbox;
		if (!leaderAgent || !comms)
			return;

		SCR_AIMessageHandling.SendGetInMessage(leaderAgent, best, EAICompartmentType.Pilot, null, comms);
		m_bDCO_HijackOrdered = true;
	}

	// QueryEntitiesBySphere callback: collect empty, drivable vehicles. Returns true to continue.
	protected bool DCO_HijackCollect(IEntity e)
	{
		if (!e)
			return true;

		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(e.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!mgr)
			return true;	// not a vehicle with compartments

		if (mgr.GetOccupantCount() != 0)
			return true;	// occupied (don't try to steal a crewed vehicle)

		if (!mgr.HasFreeCompartmentOfTypes(SCR_BaseCompartmentManagerComponent.CREW_COMPARTMENT_TYPES))
			return true;	// no driver/crew seat: not drivable by us

		m_aDCO_HijackCandidates.Insert(e);
		return true;
	}
}
