// Per-group QRF GM attributes. Shown when a GM selects an AI group (the "25th DCO QRF" category):
// toggle the group as a Quick Reaction Force responder and set its response range. These are server
// attributes: the AI group is server-authoritative and SCR_EditableGroupComponent.GetAIGroupComponent()
// is server-only. ReadVariable returns null for any non-group selection, so they never appear on other
// entities. State + behaviour live on the group's SCR_AIGroupUtilityComponent (DCO_GroupQRF.c).

// Resolve the AI group utility component from a selected editable entity, or null if not a group.
class DCO_QRFAttributeHelper
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
class DCO_QRFResponderEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;

		return SCR_BaseEditorAttributeVar.CreateBool(util.DCO_IsQRFResponder());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetQRFResponder(var.GetBool());
	}
}

[BaseContainerProps()]
class DCO_QRFRangeEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(util.DCO_GetQRFRange());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetQRFRange(var.GetFloat());
	}

	// While the GM adjusts the range, draw a radius circle at the selected group's position so the
	// QRF coverage area is visible as it is configured (cleared when editing ends).
	override void PreviewVariable(bool setPreview, SCR_AttributesManagerEditorComponent manager)
	{
		if (!setPreview || !manager)
		{
			DCO_QRFRangeVisual.Hide();
			return;
		}

		float radius = 0;
		SCR_BaseEditorAttributeVar var = GetVariable();
		if (var)
			radius = var.GetFloat();

		array<Managed> items = {};
		manager.GetEditedItems(items);
		foreach (Managed item : items)
		{
			SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(item);
			if (!editable)
				continue;

			IEntity owner = editable.GetOwner();
			if (owner)
			{
				DCO_QRFRangeVisual.Show(owner.GetOrigin(), radius);
				return;
			}
		}
	}
	// No StopEditing() override - that base method is sealed. The circle is cleared via
	// PreviewVariable(setPreview = false), which the base StopEditing invokes when editing ends.
}

// Per-group: mark this group as a CQB Clearer (used when CQB Marked-Only is on).
[BaseContainerProps()]
class DCO_CqbClearerEditorAttribute : SCR_BaseEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (!util)
			return null;
		return SCR_BaseEditorAttributeVar.CreateBool(util.DCO_IsCqbClearer());
	}
	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;
		SCR_AIGroupUtilityComponent util = DCO_QRFAttributeHelper.GetGroupUtility(item);
		if (util)
			util.DCO_SetCqbClearer(var.GetBool());
	}
}
