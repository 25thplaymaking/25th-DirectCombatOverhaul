// Composition-aware defense (faction "base" behaviour). An uncommitted group that finds an own-faction
// composition (checkpoint / bunker / works the GM placed for its side) within range latches onto the
// nearest one: it repositions onto it and becomes a defender, so the existing layers take over -
// DCO_DefendComponent holds + orients, DCO_Garrison occupies a building in it, DCO_AssetUse mans its
// MG / mortar. Detection + counting is done by DCO_CompositionRegistry (one shared scan). The
// density-scaled "base morale" resolve is applied separately in DCO_GroupMorale.
//
// Order deference (hard rule): a GM / scenario / external waypoint always wins. The order check runs
// first and, if the group has an active waypoint, this system releases its latch and does nothing - it
// never tug-of-wars with an order. It also only ever clears the Defender flag it set itself (a GM- or
// task-zone-set defender is left alone).
//
// Another modded SCR_AIGroupUtilityComponent fragment. Folder "Outpost" sorts after "Comms" (for
// DCO_GroupHasActiveWaypoint), "Defend" (DCO_SetDefender / DCO_IsDefender) and "Garrison"/"Assets", and
// before "QRF" (so DCO_UpdateComposition is defined before the shared EvaluateActivity in DCO_GroupQRF.c
// that calls it) - Enforce resolves cross-fragment calls in file order. Server-authoritative. Default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float		m_fDCO_LastCompositionTime	= -1;
	protected bool		m_bDCO_CompositionLatched	= false;
	protected bool		m_bDCO_CompositionSetDefender = false;	// did we turn the Defender flag on? only then do we clear it
	protected vector	m_vDCO_CompositionAnchor	= vector.Zero;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity).
	void DCO_UpdateComposition()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg || !cfg.m_bEnableCompositionDefense || !m_Owner || !m_Mailbox)
		{
			DCO_ReleaseCompositionLatch();
			return;
		}

		// Order deference first: a GM / scenario / external waypoint always wins. (DCO_GroupHasActiveWaypoint
		// is defined in DCO_GroupReinforcement.c - Comms sorts before Outpost.)
		if (DCO_GroupHasActiveWaypoint(m_Owner))
		{
			DCO_ReleaseCompositionLatch();
			return;
		}

		// Mounted groups skip (consistent with Defend / Garrison / AssetUse).
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
		{
			DCO_ReleaseCompositionLatch();
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastCompositionTime >= 0 && (now - m_fDCO_LastCompositionTime) < cfg.m_fCompositionCheckSec * 1000.0)
			return;
		m_fDCO_LastCompositionTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector selfPos = leader.GetOrigin();

		Faction fac = m_Owner.GetFaction();
		if (!fac)
		{
			DCO_ReleaseCompositionLatch();
			return;
		}

		vector anchor;
		if (!DCO_CompositionRegistry.Get().GetNearest(selfPos, fac, cfg.m_fCompositionRadius, anchor))
		{
			// Out of range / the composition is gone: release and resume normal behaviour.
			DCO_ReleaseCompositionLatch();
			return;
		}

		// Latch onto the nearest composition: become its defender (the reused fragments hold / occupy / man
		// the gun) and, if enabled, reposition onto it, bounded to within radius (no map-wide march).
		if (!m_bDCO_CompositionLatched)
		{
			m_bDCO_CompositionLatched = true;
			m_vDCO_CompositionAnchor = anchor;

			if (!DCO_IsDefender())
			{
				DCO_SetDefender(true);
				m_bDCO_CompositionSetDefender = true;
			}

			if (cfg.m_bCompositionReposition
				&& vector.DistanceSq(selfPos, anchor) > cfg.m_fCompositionHoldLeash * cfg.m_fCompositionHoldLeash)
			{
				DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, anchor, m_Mailbox);
			}
		}
	}

	// Drop the latch and clear the Defender flag only if this system set it (never disturbs a GM- or
	// task-zone-assigned defender).
	protected void DCO_ReleaseCompositionLatch()
	{
		if (!m_bDCO_CompositionLatched)
			return;

		if (m_bDCO_CompositionSetDefender)
		{
			DCO_SetDefender(false);
			m_bDCO_CompositionSetDefender = false;
		}
		m_bDCO_CompositionLatched = false;
	}
}
