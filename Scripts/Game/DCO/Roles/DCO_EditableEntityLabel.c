// Editor content-browser filter for the placeable Task Zones.
// GM placeable entities are bucketed by entity type into the top-level tabs (Characters / Vehicles /
// Systems / ...). Our Task Zones are SYSTEM entities, so they live in the Systems tab. To make them
// easy to find instead of mixed in with every other system, we add our own filter category there,
// exactly how 25thGMERemix adds its "Module Type" filter.
//
// This needs three pieces (mirroring GME): these modded enums - a new label group (the filter
// category) plus one label per zone role; Configs/Core/EditableEntityCore.conf which describes the
// group + labels and (via m_ConditionalLabel ENTITYTYPE_SYSTEM) shows the filter only under Systems;
// each Task-Zone prefab carries its label in m_aAuthoredLabels alongside ENTITYTYPE_SYSTEM.
//
// Label values are in the 72xx range to avoid the base game's values and GME's 70xx range so the two
// mods coexist. The group enum value is auto-assigned (merged), same as GME does.
modded enum EEditableEntityLabel
{
	DCO_TASKZONE_QRF		= 7210,
	DCO_TASKZONE_AMBUSH		= 7211,
	DCO_TASKZONE_TRIGGER	= 7212,
	DCO_TASKZONE_DEFEND		= 7213,
	DCO_TASKZONE_REINFORCE	= 7214,
	DCO_TASKZONE_OBJECTIVE	= 7215,
}

modded enum EEditableEntityLabelGroup
{
	DCO_TASKZONE
}
