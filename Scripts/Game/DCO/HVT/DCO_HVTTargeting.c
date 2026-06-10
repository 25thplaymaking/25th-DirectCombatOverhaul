// HVT / priority targeting. When a group perceives a high-value target (an enemy
// vehicle) it focuses fire on the biggest threat instead of trading shots with
// riflemen. Targets are ranked by threat tier:
//   helicopter > armed/armoured vehicle (has a turret) > unarmed transport.
// Infantry are not HVTs (left to normal combat).
//
// Cross-group distribution: friendly groups share a claim registry (DCO_HVTUtil), so
// when several see the same vehicles they spread across the different targets rather
// than all focus-firing one and leaving the rest free to hammer our other elements.
// Per target, score = tier + a penalty for how many friendly groups already claim it +
// a small distance tiebreak; lowest score wins. So a few groups peel onto a heli, then
// the rest spread to cover the BTRs/trucks.
//
// modded SCR_AIGroupUtilityComponent fragment. "HVT" sorts before "QRF" so DCO_UpdateHVT
// is defined before the tick that calls it. Server-only. Gated on
// m_bEnableHVTTargeting AND the base-settings AT/Asset utilization roll.
modded class SCR_AIGroupUtilityComponent
{
	protected float		m_fDCO_LastHVTTime		= -1;
	protected IEntity	m_eDCO_CurrentHVT;

	void DCO_UpdateHVT()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableHVTTargeting || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		// Base-settings AT/asset gate: only a rolled-in fraction of groups prioritise armour
		// as high-value targets (the "AT" half of the AT/Assets lever).
		if (!DCO_BaseSettingsUtil.GroupUsesAssets(this))
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
		{
			if (m_eDCO_CurrentHVT)
				DCO_HVTUtil.SetGroupTarget(this, null);	// release our claim so others re-distribute
			m_eDCO_CurrentHVT = null;
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastHVTTime >= 0 && (now - m_fDCO_LastHVTTime) < cfg.m_fHVTCheckIntervalSec * 1000.0)
			return;
		m_fDCO_LastHVTTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector fromPos = leader.GetOrigin();

		// Pick the highest-priority HVT, spread across friendly groups. Lowest score wins.
		IEntity hvt;
		float bestScore = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			int tier = DCO_HVTUtil.Classify(t);
			if (tier == DCO_HVTUtil.HVT_NONE)
				continue;	// infantry / non-vehicle: leave it to normal combat

			int claims = DCO_HVTUtil.ClaimCount(t);
			if (t == m_eDCO_CurrentHVT && claims > 0)
				claims--;	// don't count our own existing claim (don't abandon a target we hold)

			float dist = vector.Distance(t.GetOrigin(), fromPos);
			float score = tier * DCO_HVTUtil.TIER_WEIGHT + claims * DCO_HVTUtil.CLAIM_WEIGHT + Math.Min(dist, 99.0) * DCO_HVTUtil.DIST_WEIGHT;
			if (score < bestScore)
			{
				bestScore = score;
				hvt = t;
			}
		}

		if (!hvt)
		{
			if (m_eDCO_CurrentHVT)
				DCO_HVTUtil.SetGroupTarget(this, null);
			m_eDCO_CurrentHVT = null;
			return;
		}

		// Maintain the cross-group claim registry (no-op if our target is unchanged).
		DCO_HVTUtil.SetGroupTarget(this, hvt);

		// Only (re)issue the attack order when the HVT changes, to avoid spamming the same order every tick.
		if (hvt == m_eDCO_CurrentHVT)
			return;
		m_eDCO_CurrentHVT = hvt;

		SCR_AITargetInfo ti = new SCR_AITargetInfo();
		PerceivableComponent perc = PerceivableComponent.Cast(hvt.FindComponent(PerceivableComponent));
		ti.Init(hvt, perc, hvt.GetOrigin(), now);

		// No static Create() factory for this message type, and CreateMessage(TypeName) is
		// obsolete in 1.7, so construct directly (same path the Create() wrappers use).
		SCR_AIMessage_Attack msg = new SCR_AIMessage_Attack();
		if (!msg)
			return;

		msg.m_TargetInfo = ti;
		m_Mailbox.RequestBroadcast(msg);
	}
}

// HVT classification + cross-group claim registry. Plain static class (like
// DCO_VehicleUtil) so it's callable regardless of folder order, and the registry is
// shared across every group instance.
class DCO_HVTUtil
{
	// Threat tiers (lower = higher priority).
	static const int HVT_HELI      = 0;		// helicopter (top threat)
	static const int HVT_MECH      = 1;		// armed/armoured: APC/BTR/tank/technical (has a turret seat)
	static const int HVT_MOTORIZED = 2;		// unarmed transport: trucks/cars (GAZ etc.)
	static const int HVT_NONE      = -1;	// not a vehicle HVT (infantry / non-vehicle)

	// Scoring weights (see DCO_UpdateHVT). Tier dominates; claims then spread groups across targets; distance
	// only breaks ties. With these values a heli draws ~3 groups before a fresh armour target is preferred.
	static const float TIER_WEIGHT  = 100.0;
	static const float CLAIM_WEIGHT = 30.0;
	static const float DIST_WEIGHT  = 0.5;

	// group util -> the HVT it currently targets; target -> how many groups are claiming it.
	protected static ref map<SCR_AIGroupUtilityComponent, IEntity>	s_GroupTarget	= new map<SCR_AIGroupUtilityComponent, IEntity>();
	protected static ref map<IEntity, int>							s_TargetClaims	= new map<IEntity, int>();

	// Classify a perceived entity into a threat tier. Helicopter > armed vehicle > unarmed
	// vehicle; anything else is HVT_NONE and left to normal combat.
	static int Classify(IEntity e)
	{
		if (!e)
			return HVT_NONE;

		if (e.FindComponent(HelicopterControllerComponent))
			return HVT_HELI;

		if (!e.FindComponent(BaseVehicleControllerComponent))
			return HVT_NONE;	// not a vehicle -> infantry

		// Armed (mechanised) vs motorised: a TURRET seat means an armed fighting vehicle.
		// ECompartmentType is all-caps {PILOT, TURRET, CARGO} - '.Turret' failed earlier.
		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(e.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (mgr)
		{
			array<BaseCompartmentSlot> turrets = {};
			mgr.GetCompartmentsOfType(turrets, ECompartmentType.TURRET);
			if (turrets && !turrets.IsEmpty())
				return HVT_MECH;
		}
		return HVT_MOTORIZED;
	}

	// How many friendly groups currently claim this target (for fire distribution).
	static int ClaimCount(IEntity target)
	{
		if (!target)
			return 0;
		int n;
		if (s_TargetClaims.Find(target, n))
			return n;
		return 0;
	}

	// Record the HVT a group is engaging (reference-counted) so nearby groups deconflict.
	// Decrements the previous target's claim. Pass null to release. No-op if unchanged.
	static void SetGroupTarget(SCR_AIGroupUtilityComponent gu, IEntity target)
	{
		if (!gu)
			return;

		IEntity prev;
		bool hadPrev = s_GroupTarget.Find(gu, prev);
		if (hadPrev && prev == target)
			return;	// unchanged

		// Release the previous claim.
		if (hadPrev && prev)
		{
			int pn;
			if (s_TargetClaims.Find(prev, pn))
			{
				pn--;
				if (pn <= 0)
					s_TargetClaims.Remove(prev);
				else
					s_TargetClaims.Set(prev, pn);
			}
		}

		// Add the new claim (or clear the group's entry).
		if (target)
		{
			int tn;
			s_TargetClaims.Find(target, tn);
			s_TargetClaims.Set(target, tn + 1);
			s_GroupTarget.Set(gu, target);
		}
		else
		{
			s_GroupTarget.Remove(gu);
		}

		// Bound memory over a long session (a reset just re-distributes next tick).
		if (s_GroupTarget.Count() > 4000)
		{
			s_GroupTarget.Clear();
			s_TargetClaims.Clear();
		}
	}
}
