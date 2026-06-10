// GM attributes for the "25th DCO BASE SETTINGS" tab. Each one reads/writes the
// global DCO_BaseSettings singleton, gated to SCR_BaseGameMode so they only show in
// the GM "Scenario properties" panel. AI-only levers.

[BaseContainerProps()]
class DCO_EnableBaseSettingsEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_BaseSettings.Get().m_bEnableBaseSettings);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_bEnableBaseSettings = var.GetBool();
	}
}

[BaseContainerProps()]
class DCO_AiSkillEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateInt(DCO_BaseSettings.Get().m_iAiSkill);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_iAiSkill = var.GetInt();
	}
}

[BaseContainerProps()]
class DCO_PerceptionEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateInt(DCO_BaseSettings.Get().m_iPerception);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_iPerception = var.GetInt();
	}
}

[BaseContainerProps()]
class DCO_ReactionTimeEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateInt(DCO_BaseSettings.Get().m_iReactionTime);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_iReactionTime = var.GetInt();
	}
}

[BaseContainerProps()]
class DCO_FireRateEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateInt(DCO_BaseSettings.Get().m_iFireRate);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_iFireRate = var.GetInt();
	}
}

// AT/asset utilization percentage (0..100). Each group rolls once against this to
// decide whether it joins AT/asset behaviour (manning statics + armour HVT priority).
[BaseContainerProps()]
class DCO_AssetUtilizationEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateInt(DCO_BaseSettings.Get().m_iAssetUtilizationPct);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_iAssetUtilizationPct = var.GetInt();
	}
}

// Stance dropdown. Options come from m_aValues in DCO_Attributes.conf; the engine
// works in index space, mapped to the stored value via the base Convert*Index helpers.
[BaseContainerProps()]
class DCO_GlobalStanceEditorAttribute : SCR_BaseFloatValueHolderEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		int index = ConvertValueToIndex(DCO_BaseSettings.Get().m_eGlobalStance);
		if (index != -1)
			return SCR_BaseEditorAttributeVar.CreateInt(index);
		return null;
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var) return;
		float val;
		if (!ConvertIndexToValue(var.GetInt(), val)) return;
		DCO_BaseSettings.Get().m_eGlobalStance = Math.Round(val);
	}
}

// Default-formation dropdown. Ordinal maps to SCR_EAIGroupFormation
// {Wedge=0, Line=1, Column=2, StaggeredColumn=3}.
[BaseContainerProps()]
class DCO_DefaultFormationEditorAttribute : SCR_BaseFloatValueHolderEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		int index = ConvertValueToIndex(DCO_BaseSettings.Get().m_eDefaultSpawnFormation);
		if (index != -1)
			return SCR_BaseEditorAttributeVar.CreateInt(index);
		return null;
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var) return;
		float val;
		if (!ConvertIndexToValue(var.GetInt(), val)) return;
		DCO_BaseSettings.Get().m_eDefaultSpawnFormation = Math.Round(val);
	}
}

// Troop-grade dropdown. Writing a non-NONE grade snaps all levers to its quarter
// value via ApplyGrade. Ordinal maps through the base Convert*Index helpers.
[BaseContainerProps()]
class DCO_GlobalGradeEditorAttribute : SCR_BaseFloatValueHolderEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		int index = ConvertValueToIndex(DCO_BaseSettings.Get().m_eGlobalGrade);
		if (index != -1)
			return SCR_BaseEditorAttributeVar.CreateInt(index);
		return null;
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var) return;
		float val;
		if (!ConvertIndexToValue(var.GetInt(), val)) return;
		DCO_BaseSettings cfg = DCO_BaseSettings.Get();
		int g = Math.Round(val);
		cfg.m_eGlobalGrade = g;
		DCO_BaseSettingsUtil.ApplyGrade(cfg, g);
	}
}

[BaseContainerProps()]
class DCO_DebugBaseSettingsEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		if (!SCR_BaseGameMode.Cast(item)) return null;
		return SCR_BaseEditorAttributeVar.CreateBool(DCO_BaseSettings.Get().m_bDebugBaseSettings);
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (var) DCO_BaseSettings.Get().m_bDebugBaseSettings = var.GetBool();
	}
}
