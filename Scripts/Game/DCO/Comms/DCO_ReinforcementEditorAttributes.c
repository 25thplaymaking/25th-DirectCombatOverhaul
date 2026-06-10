// Per-group "Reinforcement Eligible" GM attribute. A checkbox shown when a GM selects
// an AI group. An eligible group overrides the "respect waypoints" deconfliction guard,
// so DCO may pull it off its own patrol/capture waypoint to converge on a nearby
// contact. Left off, the group finishes its waypoint first and only reinforces when
// genuinely idle (the default deconflicted behaviour).
//
// Same pattern as DCO_QRFEditorAttributes.c: server attribute; ReadVariable returns null
// for any non-group selection. State + behaviour live on the group's
// SCR_AIGroupUtilityComponent (DCO_GroupReinforcement.c). Registered in DCO_Attributes.conf
// under the DCO_QRF category so it sits next to the QRF/role toggles.
class DCO_ReinforceAttributeHelper
{
	static SCR_AIGroupUtilityComponent GetGroupUtility(Managed item)
	{
		SCR_EditableGroupComponent editableGroup = SCR_EditableGroupComponent.Cast(item);
		if (!editableGroup)
			return null;

		SCR_AIGroup aiGroup = editableGroup.GetAIGroupComponent();
		if (!aiGroup)
			return null;

		return aiGroup.GetGroupUtilityComponent();
	}
}

[BaseContainerProps()]
class DCO_ReinforcementEligibleEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_ReinforceAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(util.DCO_IsReinforcementEligible());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		SCR_AIGroupUtilityComponent util = DCO_ReinforceAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetReinforcementEligible(var.GetBool());
	}
}
