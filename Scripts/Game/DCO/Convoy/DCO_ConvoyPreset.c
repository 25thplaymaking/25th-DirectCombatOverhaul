// Convoy preset (spawn-time tag, no Game Master required). Bakes a convoy id onto a vehicle's
// AI group prefab so every group spawned from it is locked into the same section regardless of
// distance (the tag-override path of the section controller). Untagged groups still auto-section
// by proximity. Mirrors DCO_GroupRolePreset: applied one frame after init, server-side.
//
// To use in the Workbench:
//   1. Open the AI group prefab the vehicle's crew uses (carries SCR_AIGroup /
//      SCR_AIGroupUtilityComponent).
//   2. Add component DCO_ConvoyPreset.
//   3. Set Convoy Id (a positive integer; the same number on multiple vehicle group prefabs binds
//      them). Leave it at -1 to rely on proximity sectioning only.
class DCO_ConvoyPresetClass : ScriptComponentClass
{
}

class DCO_ConvoyPreset : ScriptComponent
{
	[Attribute("-1", UIWidgets.Slider, "Convoy id: vehicles sharing this id are locked into one section (proximity ignored for them). -1 = untagged (proximity only).", "-1 64 1", category: "25th DCO")]
	int m_iConvoyId;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_iConvoyId < 0)
			return;

		// Only the authoritative server runs DCO behaviour; skip in the World Editor (not in play mode).
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		// Defer one frame: the SCR_AIGroupUtilityComponent is reliably present by then.
		GetGame().GetCallqueue().CallLater(DCO_Apply, 0, false);
	}

	protected void DCO_Apply()
	{
		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_AIGroup grp = SCR_AIGroup.Cast(owner);
		if (!grp)
			return;

		SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
		if (!util)
		{
			// Group utility not up yet (rare) - try once more shortly, then give up.
			GetGame().GetCallqueue().CallLater(DCO_Apply, 500, false);
			return;
		}

		util.DCO_SetConvoyId(m_iConvoyId);
	}
}
