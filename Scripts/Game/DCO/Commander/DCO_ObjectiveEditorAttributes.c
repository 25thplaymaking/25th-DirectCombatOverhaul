// GM attributes for a placed Objective Zone. Three entity-scoped attributes shown
// when a GM selects a DCO Objective Zone entity. Each resolves the
// DCO_ObjectiveZoneComponent off the selected editable entity (same chain as
// DCO_TaskZoneEditorAttributes.c) and reads/writes its fields. ReadVariable returns
// null for any non-objective-zone selection, so they never show on unrelated entities.
//
// Registration needed in DCO_Attributes.conf (entity-scoped category, not the game-mode
// one - mirror the DCO_Ambush category block):
//   1. DCO_ObjectiveTypeEditorAttribute     - ButtonBox_Selection dropdown; m_aValues in
//      enum order: DEFEND HOLD ATTACK PATROL SUPPORT RESERVE
//   2. DCO_ObjectivePriorityEditorAttribute - Slider, min=0 max=100 step=1
//   3. DCO_ObjectiveRadiusEditorAttribute   - Slider, min=25 max=500 step=5

// Resolve the DCO_ObjectiveZoneComponent off the Managed item the engine passes to
// Read/WriteVariable. Null for anything that isn't an objective zone entity.
static DCO_ObjectiveZoneComponent DCO_GetObjectiveZone(Managed item)
{
	SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(item);
	if (!editable)
		return null;

	IEntity owner = editable.GetOwner();
	if (!owner)
		return null;

	return DCO_ObjectiveZoneComponent.Cast(owner.FindComponent(DCO_ObjectiveZoneComponent));
}

// Objective type dropdown. Engine works in index space (0..5); ConvertValueToIndex/
// ConvertIndexToValue map the stored DCO_EObjectiveType ordinal to the display index
// and back. The .conf m_aValues list must be in enum order.
[BaseContainerProps()]
class DCO_ObjectiveTypeEditorAttribute : SCR_BaseFloatValueHolderEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (!zone)
			return null;

		int index = ConvertValueToIndex(zone.GetObjType());
		if (index == -1)
			return null;

		return SCR_BaseEditorAttributeVar.CreateInt(index);
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (!zone)
			return;

		float val;
		if (!ConvertIndexToValue(var.GetInt(), val))
			return;

		zone.SetObjType(Math.Round(val));
	}
}

// Objective priority slider (0..100).
[BaseContainerProps()]
class DCO_ObjectivePriorityEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (!zone)
			return null;

		return SCR_BaseEditorAttributeVar.CreateInt(zone.GetPriority());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (zone)
			zone.SetPriority(var.GetInt());
	}
}

// Objective radius slider (25..500 m), float storage.
[BaseContainerProps()]
class DCO_ObjectiveRadiusEditorAttribute : SCR_BaseValueListEditorAttribute
{
	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (!zone)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(zone.GetRadius());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		DCO_ObjectiveZoneComponent zone = DCO_GetObjectiveZone(item);
		if (zone)
			zone.SetRadius(var.GetFloat());
	}
}
