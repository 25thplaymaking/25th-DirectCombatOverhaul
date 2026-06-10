// Standoff: minimum engagement distance, no suicide-charge. AI close to knife range in open-ground
// fights (even with melee off) because the engine's fire-position behaviour is fed a tiny minimum
// distance. SCR_AIFindFirePositionBehavior is "a behavior which makes AI walk around a position
// while keeping beyond some minimal distance", so we hook its InitParameters and raise that minimum
// to a weapon-aware standoff floor: riflemen hold at a medium range, MGs / marksmen / launcher
// troops stay farther back, sidearm-only troops may still close. The engine's own fire-position
// search then finds a covered spot at that range, so "cover at distance" comes largely free.
//
// Yield rules: the floor is suppressed while the agent's group is committed to an assault
// (EDCO_COA.ASSAULT_FLANK, from the tactical brain) or is actively clearing a building (a CQB order
// within the last few seconds), so a real flanking assault still presses home and rooms still get
// cleared. The floor only ever raises the minimum (never shoves an already-distant unit farther),
// and the engagement band is widened to keep max > min so AI never stand uselessly beyond their
// weapon's reach.
//
// This is a modded behaviour class (not an SCR_AIGroupUtilityComponent fragment), in folder
// "Standoff" which sorts after "Brain" and "CQB" so the cross-class getters it calls (DCO_GetCOA /
// DCO_GetLastCqbOrderTime) are already defined on the group-utility class. Server-authoritative;
// players never run AI behaviours; default OFF (DCO_TacticalMoveSettings.m_bEnableStandoff).
modded class SCR_AIFindFirePositionBehavior
{
	protected static const float DCO_STANDOFF_CQB_WINDOW_MS = 12000.0;	// yield the floor for this long after a CQB assault order
	protected static const float DCO_STANDOFF_POINTBLANK    = 15.0;		// metres: at/under this the floor yields at point-blank

	override void InitParameters(vector targetPos, float minDistance, float maxDistance, float duration)
	{
		float newMin = minDistance;
		float newMax = maxDistance;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (cfg && cfg.m_bEnableStandoff && Replication.IsServer() && !DCO_StandoffYields() && !DCO_StandoffOwnerMounted()
			&& !DCO_StandoffPointBlank(targetPos))
		{
			float floor = DCO_StandoffFloor(cfg);
			if (newMin < floor)
				newMin = floor;
			// Keep a valid engagement band so the AI don't sit uselessly beyond their weapon's reach.
			if (newMax < newMin + cfg.m_fStandoffBand)
				newMax = newMin + cfg.m_fStandoffBand;
		}

		super.InitParameters(targetPos, newMin, newMax, duration);
	}

	// True when the target is already at point-blank range (a surprise close contact the agent walked
	// into). The standoff floor is an anti-suicide-charge lever, not a "retreat from a close enemy"
	// lever: at point-blank we yield it entirely so the agent fights where it stands instead of turning
	// its back to reach standoff distance and getting cut down doing it. Above this range the floor
	// governs normally, so the anti-charge behaviour from range is unaffected (a unit at range is
	// beyond point-blank, so the floor still stops it charging in).
	protected bool DCO_StandoffPointBlank(vector targetPos)
	{
		if (!m_Utility || !m_Utility.m_OwnerEntity)
			return false;
		return vector.DistanceSq(m_Utility.m_OwnerEntity.GetOrigin(), targetPos) < (DCO_STANDOFF_POINTBLANK * DCO_STANDOFF_POINTBLANK);
	}

	// Weapon-aware minimum engagement distance for this agent.
	protected float DCO_StandoffFloor(DCO_TacticalMoveSettings cfg)
	{
		if (!m_Utility || !m_Utility.m_CombatComponent)
			return cfg.m_fStandoffRifle;

		switch (m_Utility.m_CombatComponent.GetCurrentWeaponType())
		{
			case EWeaponType.WT_MACHINEGUN:		return cfg.m_fStandoffMG;
			case EWeaponType.WT_SNIPERRIFLE:	return cfg.m_fStandoffSniper;
			case EWeaponType.WT_ROCKETLAUNCHER:	return cfg.m_fStandoffLauncher;
			case EWeaponType.WT_HANDGUN:		return cfg.m_fStandoffPistol;
		}
		return cfg.m_fStandoffRifle;	// rifle / grenade-launcher / default
	}

	// True if this agent is crewing a vehicle. The standoff floor is an infantry engagement-distance
	// lever; applying it to a vehicle gunner makes the crew hold a rifleman/MG standoff and reposition
	// (drive) to keep that distance instead of fighting, the "vehicles drive stupidly" bug. Mounted
	// crews are passed straight through to the vanilla fire-position behaviour (mirrors the crew guard
	// in DCO_TacticalMove).
	protected bool DCO_StandoffOwnerMounted()
	{
		if (!m_Utility || !m_Utility.m_OwnerEntity)
			return false;
		return CompartmentAccessComponent.GetVehicleIn(m_Utility.m_OwnerEntity) != null;
	}

	// True while the agent's group is committed to an assault or actively clearing a building; the
	// standoff floor is suppressed so they close (no passive behaviour, rooms still get cleared).
	protected bool DCO_StandoffYields()
	{
		SCR_AIGroupUtilityComponent gu = DCO_StandoffGroupUtil();
		if (!gu)
			return false;

		if (gu.DCO_GetCOA() == EDCO_COA.ASSAULT_FLANK)
			return true;

		float cqbOrderTime = gu.DCO_GetLastCqbOrderTime();
		if (cqbOrderTime >= 0)
		{
			BaseWorld world = GetGame().GetWorld();
			if (world && (world.GetWorldTime() - cqbOrderTime) < DCO_STANDOFF_CQB_WINDOW_MS)
				return true;
		}
		return false;
	}

	// Resolve this agent's group utility component (owner entity: AI control, agent, group, util).
	protected SCR_AIGroupUtilityComponent DCO_StandoffGroupUtil()
	{
		if (!m_Utility || !m_Utility.m_OwnerEntity)
			return null;

		AIControlComponent aic = AIControlComponent.Cast(m_Utility.m_OwnerEntity.FindComponent(AIControlComponent));
		if (!aic)
			return null;

		AIAgent agent = aic.GetControlAIAgent();
		if (!agent)
			return null;

		SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
		if (!grp)
			return null;

		return grp.GetGroupUtilityComponent();
	}
}
