// Game Master attributes for the "25th DCO" settings tab. Each attribute reads/writes the
// global DCO_MoraleSettings singleton, so admins can tune the DCO morale/surrender system
// live in any Game Master scenario (no per-scenario setup). They are grouped under the
// "25th DCO" category via the attribute-list registration
// (Configs/Editor/AttributeLists/Edit.conf override), which sets the category title.

[BaseContainerProps()]
class DCO_EnableMoraleEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnabled);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnabled = var.GetBool();
	}
}

// Unified DCO debug-logging master toggle.
[BaseContainerProps()]
class DCO_DebugEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bDebug);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bDebug = var.GetBool();
	}
}

// Phase 14 - surrender context gates (lull / squad strength / stagger / per-casualty hit).
[BaseContainerProps()]
class DCO_SurrenderLullEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fSurrenderLullSec);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fSurrenderLullSec = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_SurrenderMinSquadEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateInt(DCO_MoraleSettings.Get().m_iSurrenderMinSquadToFight);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_iSurrenderMinSquadToFight = var.GetInt();
	}
}

[BaseContainerProps()]
class DCO_SurrenderStaggerEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fSurrenderStaggerMaxSec);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fSurrenderStaggerMaxSec = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_MoraleLostPerCasualtyEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fMoraleLostPerCasualty);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fMoraleLostPerCasualty = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_FleeThresholdEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fFleeThreshold);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fFleeThreshold = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_SurrenderThresholdEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fSurrenderThreshold);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fSurrenderThreshold = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_EnableSurrenderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSurrender);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSurrender = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_DropGearOnSurrenderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bDropGearOnSurrender);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bDropGearOnSurrender = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableGrenadeDropEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableFakeSurrender);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableFakeSurrender = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableStanceCooldownEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableStanceCooldown);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableStanceCooldown = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableSurrenderVoiceEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSurrenderVoice);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSurrenderVoice = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableLastStandEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableLastStand);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableLastStand = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnablePanicEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnablePanic);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnablePanic = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableMoraleAccuracyEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableMoraleAccuracy);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableMoraleAccuracy = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_PanicChanceEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fPanicChancePerTick);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fPanicChancePerTick = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_PanicDurationEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fPanicDurationSec);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fPanicDurationSec = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_AccuracyRookieMoraleEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fAccuracyRookieMorale);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fAccuracyRookieMorale = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_AccuracyRegularMoraleEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fAccuracyRegularMorale);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fAccuracyRegularMorale = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_EnableVehicleArmorEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVehicleArmor);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVehicleArmor = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_VehicleFrontalArcEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fVehicleFrontalArcDeg);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fVehicleFrontalArcDeg = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_EnableAdaptiveFormationEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableAdaptiveFormation);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableAdaptiveFormation = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableSharedMoraleEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSharedMorale);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSharedMorale = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableFriendlyFireEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableFriendlyFire);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableFriendlyFire = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableStragglerMergeEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableStragglerMerge);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableStragglerMerge = var.GetBool();
	}
}

// In-cover stance-cooldown toggle (anti "matrix-dodge").
[BaseContainerProps()]
class DCO_StanceCooldownInCoverEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bStanceCooldownInCover);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bStanceCooldownInCover = var.GetBool();
	}
}

// Phase 10 - surrender recovery / re-arm master toggle.
[BaseContainerProps()]
class DCO_EnableSurrenderRecoveryEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSurrenderRecovery);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSurrenderRecovery = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableFleeFromArmorEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableFleeFromArmor);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableFleeFromArmor = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableHitFlinchEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableHitFlinch);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableHitFlinch = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableVehicleReinforcementEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVehicleReinforcement);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVehicleReinforcement = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableHVTTargetingEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableHVTTargeting);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableHVTTargeting = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableVehicleHijackEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVehicleHijack);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVehicleHijack = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableFleeSmokeEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableFleeSmoke);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableFleeSmoke = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableNightIllumEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableNightIllum);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableNightIllum = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableReinforcementEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableReinforcement);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableReinforcement = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_ReinforcementRadiusEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fReinforcementRadius);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fReinforcementRadius = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_ReinforcementMaxRespondersEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		// Global attribute: only valid in the Game Master "Scenario properties" panel, whose edited
		// item is the game mode. Returning null for any other selection keeps it out of entity panels
		// (matches the engine's global-attribute pattern).
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateInt(DCO_MoraleSettings.Get().m_iReinforcementMaxResponders);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_iReinforcementMaxResponders = var.GetInt();
	}
}

// Unit cohesion (realism): one-time morale hit when the group's leader is killed under fire (decapitation).
[BaseContainerProps()]
class DCO_MoraleLostOnLeaderLossEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fMoraleLostOnLeaderLoss);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fMoraleLostOnLeaderLoss = var.GetFloat();
	}
}

// Fake surrender: the in-range chance a faker actually primes + throws (separate from selection chance).
[BaseContainerProps()]
class DCO_FakeSurrenderDropChanceEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_MoraleSettings.Get().m_fFakeSurrenderDropChance);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_fFakeSurrenderDropChance = var.GetFloat();
	}
}

// Morale contagion / cascade master toggle.
[BaseContainerProps()]
class DCO_EnableMoraleContagionEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableMoraleContagion);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableMoraleContagion = var.GetBool();
	}
}

// Progressive aim zeroing.
[BaseContainerProps()]
class DCO_EnableAimZeroingEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableAimZeroing);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableAimZeroing = var.GetBool();
	}
}

// Vision / night limiting.
[BaseContainerProps()]
class DCO_EnableVisionLimitEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVisionLimit);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVisionLimit = var.GetBool();
	}
}

// Per-member morale modifier.
[BaseContainerProps()]
class DCO_EnableMemberMoraleEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableMemberMorale);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableMemberMorale = var.GetBool();
	}
}

// Adjust-to-cover stance.
[BaseContainerProps()]
class DCO_EnableCoverStanceEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableCoverStance);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableCoverStance = var.GetBool();
	}
}

// Real suppression.
[BaseContainerProps()]
class DCO_EnableSuppressionEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSuppression);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSuppression = var.GetBool();
	}
}

// Machine-gunner emplacement.
[BaseContainerProps()]
class DCO_EnableMachineGunnerEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableMachineGunner);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableMachineGunner = var.GetBool();
	}
}

// Emergency rearm.
[BaseContainerProps()]
class DCO_EnableEmergencyRearmEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableEmergencyRearm);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableEmergencyRearm = var.GetBool();
	}
}

// Close-quarters melee.
[BaseContainerProps()]
class DCO_EnableMeleeEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableMelee);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableMelee = var.GetBool();
	}
}


// Static weapon use.
[BaseContainerProps()]
class DCO_EnableAssetUseEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableAssetUse);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableAssetUse = var.GetBool();
	}
}

// Vehicle combat positioning.
[BaseContainerProps()]
class DCO_EnableVehicleCombatEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVehicleCombat);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVehicleCombat = var.GetBool();
	}
}

// CQB / building assault.
[BaseContainerProps()]
class DCO_EnableCQBEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableCQB);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableCQB = var.GetBool();
	}
}

// Artillery fire missions.
[BaseContainerProps()]
class DCO_EnableArtilleryEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableArtillery);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableArtillery = var.GetBool();
	}
}

// AI commander.
[BaseContainerProps()]
class DCO_EnableCommanderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableCommander);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableCommander = var.GetBool();
	}
}

// NOTE: the map-intelligence settings (areas, native scan, forests, high-ground, provider,
// offline bake) are deliberately JSON-ONLY - they apply at scan/startup time, so altering them
// live in GM would either do nothing (areas are already built + cached) or mislead. The only
// commander control exposed to the Game Master is the existing AI Commander switch
// (DCO_EnableCommanderEditorAttribute above). The offline bake is triggered by the JSON flag
// m_bDumpMapIntelOnLoad (write once on the next scan, then turn it back off).

// AI Commander: draw the objective plan in GM (listen-host/SP only).
[BaseContainerProps()]
class DCO_CmdVisualizeEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bCmdVisualize);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bCmdVisualize = var.GetBool();
	}
}

// External AI Commander (RECOM/LLM bridge) master toggle. Endpoint + API key are configured in the server JSON.
[BaseContainerProps()]
class DCO_EnableExternalCommanderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableExternalCommander);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableExternalCommander = var.GetBool();
	}
}

// Vehicle doctrine master toggle.
[BaseContainerProps()]
class DCO_EnableVehicleDoctrineEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVehicleDoctrine);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVehicleDoctrine = var.GetBool();
	}
}

// Vehicle doctrine debug draw.
[BaseContainerProps()]
class DCO_DebugVehicleDoctrineEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bDebugVehDoctrine);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bDebugVehDoctrine = var.GetBool();
	}
}

// Building garrison.
[BaseContainerProps()]
class DCO_EnableGarrisonEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableGarrison);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableGarrison = var.GetBool();
	}
}

// Composition defense + base morale (DCO_GroupComposition / Outpost).
[BaseContainerProps()]
class DCO_EnableCompositionDefenseEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableCompositionDefense);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableCompositionDefense = var.GetBool();
	}
}

// Vocal info sharing.
[BaseContainerProps()]
class DCO_EnableVocalShareEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableVocalShare);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableVocalShare = var.GetBool();
	}
}

// Radio info sharing.
[BaseContainerProps()]
class DCO_EnableRadioShareEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableRadioShare);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableRadioShare = var.GetBool();
	}
}

// Safe-eject gate.
[BaseContainerProps()]
class DCO_EnableSafeEjectEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableSafeEjectGate);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableSafeEjectGate = var.GetBool();
	}
}

// CQB town-clearing master toggle.
[BaseContainerProps()]
class DCO_EnableCqbClearEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bEnableCqbClear);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bEnableCqbClear = var.GetBool();
	}
}

// CQB town-clearing: marked-only scope.
[BaseContainerProps()]
class DCO_CqbClearMarkedOnlyEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bCqbClearMarkedOnly);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bCqbClearMarkedOnly = var.GetBool();
	}
}

// CQB town-clearing: debug draw.
[BaseContainerProps()]
class DCO_DebugCqbClearEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_MoraleSettings.Get().m_bDebugCqbClear);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_MoraleSettings.Get().m_bDebugCqbClear = var.GetBool();
	}
}
