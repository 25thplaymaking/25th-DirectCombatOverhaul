// Squad-leader asset use: man a static weapon, then leave it. When a group is in
// contact and can spare an element, the leader peels off a maneuver-team member to
// man a nearby empty static weapon / mounted MG / mortar (a turreted entity with a
// free turret seat and no occupants). When the contact clears, the member is sent
// back to the leader, dismounts the asset, and rejoins the group.
//
// Finds assets like DCO_VehicleHijack (QueryEntitiesBySphere + compartment manager,
// but for a free turret seat) and mans via SendGetInMessage(..., Turret). Releases
// with an on-foot move order to the leader - here the dismount we normally suppress
// is wanted, and a static weapon is stationary so the safe-eject gate allows it.
// modded SCR_AIGroupUtilityComponent fragment (Assets/ sorts before the QRF tick).
// Server-only, default OFF.
//
// Known interaction: while a member mans the turret, IsGroupInVehicle() reads true
// for the whole group, so other on-foot DCO systems skip it until the asset is
// released. Acceptable while this is experimental; a per-member refinement is later.
modded class SCR_AIGroupUtilityComponent
{
	protected IEntity				m_DCO_AssetEntity;
	protected AIAgent				m_DCO_AssetGunner;
	protected bool					m_bDCO_AssetManned		= false;
	protected float					m_fDCO_LastAssetCheck	= -1;
	protected ref array<IEntity>	m_aDCO_AssetCandidates;

	void DCO_UpdateAssetUse()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableAssetUse || !m_Owner || !m_Mailbox)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastAssetCheck >= 0 && (now - m_fDCO_LastAssetCheck) < cfg.m_fAssetCheckSec * 1000.0)
			return;
		m_fDCO_LastAssetCheck = now;

		bool inContact = false;
		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			inContact = targets && !targets.IsEmpty();
		}

		// Proactive: man (and keep manning) a nearby static even out of contact, so a
		// group holding a position sets the gun up before the enemy arrives rather than
		// scrambling for it mid-firefight.
		bool proactive = cfg.m_bAssetProactive;

		// Already manning one -> release when no longer useful. Proactive keeps it manned out of contact.
		if (m_bDCO_AssetManned)
		{
			bool gunnerValid = m_DCO_AssetGunner && m_DCO_AssetGunner.GetControlledEntity();
			bool stillUseful = inContact || proactive;
			if (!stillUseful || !gunnerValid || !m_DCO_AssetEntity || m_DCO_AssetEntity.IsDeleted())
				DCO_ReleaseAsset();
			return;
		}

		// Non-proactive: only grab an asset once there's a reason (in contact).
		if (!inContact && !proactive)
			return;

		// Base-settings gate: only a rolled-in fraction of groups acquire assets. Gates
		// acquisition only - a group that never mans an asset never needs to release one.
		if (!DCO_BaseSettingsUtil.GroupUsesAssets(this))
			return;

		// In contact we want two fireteams (spare a maneuver team while the base element
		// fights). Proactive holding just needs any spare member to put on the gun.
		array<ref SCR_AIGroupFireteam> fireteams = {};
		if (m_FireteamMgr)
			DCO_FireteamCompat.GetAllFireteams(m_FireteamMgr, m_Owner, fireteams);
		bool haveTwoTeams = fireteams && fireteams.Count() >= 2;
		if (!proactive && !haveTwoTeams)
			return;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector pos = leader.GetOrigin();

		// Find the nearest empty asset with a free turret seat.
		if (!m_aDCO_AssetCandidates)
			m_aDCO_AssetCandidates = {};
		m_aDCO_AssetCandidates.Clear();
		world.QueryEntitiesBySphere(pos, cfg.m_fAssetRange, DCO_AssetCollect);

		IEntity best;
		float bestSq = cfg.m_fAssetRange * cfg.m_fAssetRange + 1.0;
		foreach (IEntity e : m_aDCO_AssetCandidates)
		{
			if (!e)
				continue;
			float d = vector.DistanceSq(e.GetOrigin(), pos);
			if (d < bestSq)
			{
				bestSq = d;
				best = e;
			}
		}
		m_aDCO_AssetCandidates.Clear();
		if (!best)
			return;

		// Choose a gunner. Prefer a non-leader (maneuver) member so the base keeps fighting.
		AIAgent leaderAgent = m_Owner.GetLeaderAgent();
		AIAgent gunner;
		if (haveTwoTeams && m_FireteamMgr)
		{
			SCR_AIGroupFireteam baseFt;
			if (leaderAgent)
				baseFt = m_FireteamMgr.FindFireteam(leaderAgent);

			foreach (SCR_AIGroupFireteam ft : fireteams)
			{
				if (!ft || ft == baseFt || ft.GetMemberCount() <= 0)
					continue;
				array<AIAgent> members = {};
				ft.GetMembers(members);
				foreach (AIAgent m : members)
				{
					if (m && m.GetControlledEntity())
					{
						gunner = m;
						break;
					}
				}
				if (gunner)
					break;
			}
		}

		// Fallback (proactive holding / single-element group): spare any non-leader member for the gun.
		if (!gunner)
			gunner = DCO_PickSpareMember(leaderAgent);
		if (!gunner)
			return;

		SCR_AIMessageHandling.SendGetInMessage(gunner, best, EAICompartmentType.Turret, null, m_Mailbox);
		m_DCO_AssetEntity = best;
		m_DCO_AssetGunner = gunner;
		m_bDCO_AssetManned = true;
		DCO_Debug.LogGroup("ASSET", leader, "manning a static weapon / turret asset");
	}

	// Send the gunner back to the leader: the on-foot move dismounts it from the
	// stationary asset and rejoins the group. Clears the manned state.
	protected void DCO_ReleaseAsset()
	{
		if (m_DCO_AssetGunner && m_Owner && m_Mailbox)
		{
			IEntity leader = m_Owner.GetLeaderEntity();
			if (leader)
				SCR_AIMessageHandling.SendMoveMessage(m_DCO_AssetGunner, leader, null, m_Mailbox);
			DCO_Debug.LogGroup("ASSET", leader, "releasing asset (no longer useful)");
		}

		m_bDCO_AssetManned = false;
		m_DCO_AssetEntity = null;
		m_DCO_AssetGunner = null;
	}

	// Any non-leader member with a controlled entity - spares a gunner when there
	// aren't two fireteams (e.g. a small group proactively manning the gun).
	protected AIAgent DCO_PickSpareMember(AIAgent leaderAgent)
	{
		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a || a == leaderAgent)
				continue;
			if (a.GetControlledEntity())
				return a;
		}
		return null;
	}

	// QueryEntitiesBySphere callback: collect unoccupied entities with a free turret
	// seat (static weapon / mounted MG / mortar). Returns true to continue the query.
	protected bool DCO_AssetCollect(IEntity e)
	{
		if (!e)
			return true;

		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(e.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!mgr)
			return true;	// not a manned entity

		if (mgr.GetOccupantCount() != 0)
			return true;	// already crewed

		// A mortar is always a valid asset even if its firing seat types as a crew
		// (not turret) compartment - otherwise the crew-seat heuristic below would
		// reject the tube and asset-use would never put a gunner on it. DCO_Artillery
		// runs the fire mission once a member is seated.
		bool isMortar = e.FindComponent(MortarMuzzleComponent) != null;

		// Heuristic using only the confirmed CREW_COMPARTMENT_TYPES constant: a static
		// weapon / mounted MG / mortar has a gunner seat but no crew seat, whereas a
		// drivable vehicle does. Skip anything with a free crew seat (that's a vehicle,
		// hijack's job) unless it's a mortar (accepted above).
		if (!isMortar && mgr.HasFreeCompartmentOfTypes(SCR_BaseCompartmentManagerComponent.CREW_COMPARTMENT_TYPES))
			return true;

		m_aDCO_AssetCandidates.Insert(e);
		return true;
	}
}
