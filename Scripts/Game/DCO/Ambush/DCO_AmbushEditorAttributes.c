// GM attributes shown under "25th DCO Ambush" when a group is selected: flag the
// group as an ambusher and set its trigger range. Server-side (the AI group is
// server-authoritative); ReadVariable returns null for non-group selections.
// Group lookup reuses DCO_QRFAttributeHelper; state lives on the group's
// SCR_AIGroupUtilityComponent (DCO_AmbushComponent.c).

[BaseContainerProps()]
class DCO_AmbushResponderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(util.DCO_IsAmbusher());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetAmbusher(var.GetBool());
	}
}

[BaseContainerProps()]
class DCO_AmbushRangeEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(util.DCO_GetAmbushRange());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetAmbushRange(var.GetFloat());
	}
}
