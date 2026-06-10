// Game Master attribute for a placed Task Zone's Pair Tag.
// Shown when a GM selects a placed DCO Task Zone (Ambush Position / Kill-Zone). A text field to set the
// Pair Tag that links a position to its kill-zone(s). ReadVariable returns null for any selection that
// is not a Task Zone, so it never appears on other entities.
//
// Resolves the DCO_TaskZoneComponent from the selected editable entity (the zone is a placeable SYSTEM
// entity). State + behaviour live on DCO_TaskZoneComponent (DCO_TaskZone.c). Registered in
// DCO_Attributes.conf (embedded GUID block 5DC0DC31, DCO_Ambush category, editbox layout).
[BaseContainerProps()]
class DCO_TaskZonePairIdEditorAttribute : SCR_BaseValueListEditorAttribute
{
	protected DCO_TaskZoneComponent GetZone(Managed item)
	{
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(item);
		if (!editable)
			return null;

		IEntity owner = editable.GetOwner();
		if (!owner)
			return null;

		return DCO_TaskZoneComponent.Cast(owner.FindComponent(DCO_TaskZoneComponent));
	}

	override SCR_BaseEditorAttributeVar ReadVariable(Managed item, SCR_AttributesManagerEditorComponent manager)
	{
		DCO_TaskZoneComponent zone = GetZone(item);
		if (!zone)
			return null;

		return SCR_BaseEditorAttributeVar.CreateFloat(zone.DCO_GetPairId());
	}

	override void WriteVariable(Managed item, SCR_BaseEditorAttributeVar var, SCR_AttributesManagerEditorComponent manager, int playerID)
	{
		if (!var)
			return;

		DCO_TaskZoneComponent zone = GetZone(item);
		if (zone)
			zone.DCO_SetPairId((int)Math.Round(var.GetFloat()));
	}
}
