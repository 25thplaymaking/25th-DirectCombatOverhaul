// Emergency rearm: keep an in-contact AI fighting when it runs dry. Generalised
// from the surrender-recovery rearm into a live behaviour:
//   1. Detect empty: current magazine is null or at 0 rounds.
//   2. Resupply: top up magazines from any available supply (cheap, safe).
//   3. Scavenge (opt-in): if still dry, walk to and pick up the nearest dropped
//      weapon on the ground, then resupply it.
//
// modded SCR_AIGroupUtilityComponent fragment. Ammo/ sorts before QRF/ so
// DCO_UpdateEmergencyRearm is defined before the shared EvaluateActivity tick
// (DCO_GroupQRF.c) that calls it. Server-only, throttled, on-foot, default OFF;
// scavenge is a separate opt-in toggle.
//
// Worth validating in-engine: when ResupplyMagazines actually finds a source,
// and that a *foreign* ground weapon inserts cleanly into a live AI (recovery
// only ever proved re-inserting a weapon the same character dropped). Both
// guarded and default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float			m_fDCO_LastRearmTime	= -1;
	protected ref array<IEntity>	m_aDCO_RearmCandidates;	// scratch for the ground-weapon query

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Rearms any
	// member who has run dry.
	void DCO_UpdateEmergencyRearm()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableEmergencyRearm || !m_Owner)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastRearmTime >= 0 && (now - m_fDCO_LastRearmTime) < cfg.m_fRearmCheckSec * 1000.0)
			return;
		m_fDCO_LastRearmTime = now;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;

			if (!DCO_RearmIsEmpty(ent))
				continue;

			// (1) Cheap, safe: try to top up magazines from any available supply.
			SCR_InventoryStorageManagerComponent scrMgr = SCR_InventoryStorageManagerComponent.Cast(ent.FindComponent(SCR_InventoryStorageManagerComponent));
			if (scrMgr)
			{
				EResupplyUnavailableReason reason;
				if (scrMgr.IsResupplyMagazinesAvailable(cfg.m_iRearmMagazineCount, reason))
				{
					scrMgr.ResupplyMagazines(cfg.m_iRearmMagazineCount);
					if (cfg.m_bDebug)
						DCO_Debug.LogGroup("REARM", ent, "resupplied magazines from available supply");
					continue;
				}
			}

			// (2) Opt-in scavenge: grab the nearest dropped weapon on the ground.
			if (cfg.m_bRearmScavenge)
				DCO_RearmScavenge(a, ent, world, scrMgr, cfg);
		}
	}

	// True if the member's current magazine is empty or absent.
	protected bool DCO_RearmIsEmpty(IEntity ent)
	{
		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return false;
		BaseWeaponManagerComponent wm = cc.GetWeaponManagerComponent();
		if (!wm)
			return false;
		BaseMagazineComponent mag = SCR_AIWeaponHandling.GetCurrentMagazineComponent(wm);
		if (!mag)
			return true;	// no magazine at all
		return mag.GetAmmoCount() <= 0;
	}

	// Move to and pick up the nearest dropped ground weapon, then resupply it.
	protected void DCO_RearmScavenge(AIAgent a, IEntity ent, BaseWorld world, SCR_InventoryStorageManagerComponent scrMgr, DCO_MoraleSettings cfg)
	{
		vector pos = ent.GetOrigin();

		m_aDCO_RearmCandidates = {};
		world.QueryEntitiesBySphere(pos, cfg.m_fRearmScavengeRange, DCO_RearmCollect);

		IEntity best;
		float bestSq = cfg.m_fRearmScavengeRange * cfg.m_fRearmScavengeRange + 1;
		foreach (IEntity w : m_aDCO_RearmCandidates)
		{
			if (!w || w == ent)
				continue;
			// A weapon on the ground is unparented; a held/holstered one is parented to its carrier.
			if (w.GetParent())
				continue;
			float d = vector.DistanceSq(w.GetOrigin(), pos);
			if (d < bestSq)
			{
				bestSq = d;
				best = w;
			}
		}

		if (!best)
			return;

		// Close enough: pick it up and resupply. Otherwise walk to it.
		if (bestSq <= cfg.m_fRearmPickupDist * cfg.m_fRearmPickupDist)
		{
			InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(ent.FindComponent(InventoryStorageManagerComponent));
			if (invMgr && invMgr.TryInsertItem(best, EStoragePurpose.PURPOSE_WEAPON_PROXY))
			{
				if (scrMgr)
					scrMgr.ResupplyMagazines(cfg.m_iRearmMagazineCount);
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("REARM", ent, string.Format("scavenged ground weapon at %1", best.GetOrigin()));
			}
		}
		else
		{
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, best.GetOrigin(), EMovementType.SPRINT, false, null);
			if (msg && m_Mailbox)
			{
				msg.SetReceiver(a);
				m_Mailbox.RequestBroadcast(msg, a);
			}
		}
	}

	// QueryEntitiesBySphere callback: collect weapon entities. Returns true to continue.
	protected bool DCO_RearmCollect(IEntity e)
	{
		if (e && e.FindComponent(WeaponComponent))
			m_aDCO_RearmCandidates.Insert(e);
		return true;
	}
}
