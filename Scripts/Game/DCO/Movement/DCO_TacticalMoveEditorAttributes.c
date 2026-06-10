// Game Master attributes for the tactical-movement layer. Global "25th DCO" attributes that read/write
// the DCO_TacticalMoveSettings singleton, so admins can tune the maneuver layer live in any Game Master
// scenario. Same pattern as DCO_MoraleEditorAttributes: global attrs return null unless the edited item
// is the game mode; checkboxes extend SCR_BaseEditorAttribute, sliders extend
// SCR_BaseValueListEditorAttribute (or the widget shows "MISSING m_SliderData!").

[BaseContainerProps()]
class DCO_EnableTacticalMoveEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableTacticalMove);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableTacticalMove = var.GetBool();
	}
}

// Standoff (minimum engagement distance / no suicide-charge).
[BaseContainerProps()]
class DCO_EnableStandoffEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableStandoff);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableStandoff = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_EnableExposureScoringEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableExposureScoring);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableExposureScoring = var.GetBool();
	}
}

// DCO-native flanking maneuver toggle.
[BaseContainerProps()]
class DCO_EnableFlankingEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableFlanking);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableFlanking = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_FlankAngleEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_TacticalMoveSettings.Get().m_fFlankAngleDeg);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_fFlankAngleDeg = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_AvoidThreatFunnelEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bAvoidThreatFunnel);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bAvoidThreatFunnel = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_MinMoveDistanceEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_TacticalMoveSettings.Get().m_fMinMoveDistance);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_fMinMoveDistance = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_CheckInIntervalEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_TacticalMoveSettings.Get().m_fCheckInInterval);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_fCheckInInterval = var.GetFloat();
	}
}

[BaseContainerProps()]
class DCO_ObservePauseEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(DCO_TacticalMoveSettings.Get().m_fObservePause);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_fObservePause = var.GetFloat();
	}
}

// Procedural tactical path.
[BaseContainerProps()]
class DCO_EnableProceduralPathEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableProceduralPath);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableProceduralPath = var.GetBool();
	}
}

// Traveling overwatch.
[BaseContainerProps()]
class DCO_EnableTravelingOverwatchEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableTravelingOverwatch);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableTravelingOverwatch = var.GetBool();
	}
}

// Bounding overwatch (base-of-fire split).
[BaseContainerProps()]
class DCO_EnableBoundingOverwatchEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableBaseOfFireSplit);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableBaseOfFireSplit = var.GetBool();
	}
}

// Cover-aware placement.
[BaseContainerProps()]
class DCO_EnableCoverSeekEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableCoverSeek);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableCoverSeek = var.GetBool();
	}
}

// React to contact.
[BaseContainerProps()]
class DCO_EnableReactToContactEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableReactToContact);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableReactToContact = var.GetBool();
	}
}

// Tactical brain / COA selection.
[BaseContainerProps()]
class DCO_EnableTacticalBrainEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableTacticalBrain);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableTacticalBrain = var.GetBool();
	}
}

// DCO formation shapes.
[BaseContainerProps()]
class DCO_EnableDcoFormationsEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item))
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_TacticalMoveSettings.Get().m_bEnableDcoFormations);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var)
			DCO_TacticalMoveSettings.Get().m_bEnableDcoFormations = var.GetBool();
	}
}
