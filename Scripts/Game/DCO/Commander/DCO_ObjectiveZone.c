// GM-draggable objective-zone component. A ScriptComponent on the Objective
// placeable prefab. When the GM drops one, it registers a GM-placed DCO_Objective
// with DCO_ObjectiveRegistry so the commander picks it up next tick. The server tick
// keeps position/radius/priority synced while the GM drags or edits the marker, so
// changes are live without re-placing it. Lifecycle mirrors DCO_TaskZoneComponent
// (deferred register on init, unregister on destroy).
//
// Don't duplicate the EditorAttributes declarations here; those GM-panel attributes
// just call the public setters below.
//
// Workbench steps to do after scripting (not scriptable):
//   1. Duplicate the Task Zone placeable prefab and name it DCO_ObjectiveZone.
//   2. Remove its m_EntityInteraction (Attachable) entry - the wrong interaction
//      type breaks the GM browser listing.
//   3. Swap DCO_TaskZoneComponent for DCO_ObjectiveZoneComponent on the prefab.
//   4. Register the 3 attributes in DCO_Attributes.conf (see DCO_ObjectiveEditorAttributes.c).
//   5. Add a placeable-registry entry (same .conf as the TaskZone placeables).

class DCO_ObjectiveZoneComponentClass : ScriptComponentClass
{
}

class DCO_ObjectiveZoneComponent : ScriptComponent
{
	// ---- Workbench-editable defaults (also writable at runtime via public setters) ----

	[Attribute("0", UIWidgets.ComboBox, "Commander objective type assigned to this marker.",
		"", ParamEnumArray.FromEnum(DCO_EObjectiveType), category: "25th DCO Objective")]
	protected DCO_EObjectiveType m_eType;

	[Attribute("50", UIWidgets.Slider, "Priority bias fed to the commander's scoring (0 = neutral).",
		"0 100 1", category: "25th DCO Objective")]
	protected int m_iPriority;

	[Attribute("100", UIWidgets.Slider, "Radius (m) of the objective area.",
		"25 500 5", category: "25th DCO Objective")]
	protected float m_fRadius;

	// ---- Runtime state ----

	// The live DCO_Objective registered with the registry. Server-only after init;
	// null on clients and before the deferred register fires.
	protected ref DCO_Objective m_Objective;

	// Sync interval. The origin only changes when the GM drags the entity, so 1s is fine.
	protected static const float TICK_SEC = 1.0;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame() || !GetGame().InPlayMode())
			return;

		if (!Replication.IsServer())
			return;

		// Defer registration so the GM's final placement transform is committed before
		// we read the origin (same reason as DCO_TaskZone).
		GetGame().GetCallqueue().CallLater(DCO_RegisterObjective, 500, false);
	}

	void ~DCO_ObjectiveZoneComponent()
	{
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(DCO_RegisterObjective);
			GetGame().GetCallqueue().Remove(DCO_Tick);
		}

		if (m_Objective)
		{
			DCO_ObjectiveRegistry.Get().UnregisterGM(m_Objective);
			m_Objective = null;
		}
	}

	// ---- Public accessors (called by editor attribute Write methods) ----

	DCO_EObjectiveType GetObjType()				{ return m_eType; }
	int GetPriority()							{ return m_iPriority; }
	float GetRadius()							{ return m_fRadius; }

	void SetObjType(int typeOrdinal)
	{
		m_eType = typeOrdinal;
		// The tick pushes this into m_Objective next cycle, so nothing to do here.
	}

	void SetPriority(int priority)
	{
		m_iPriority = Math.ClampInt(priority, 0, 100);
	}

	void SetRadius(float radius)
	{
		m_fRadius = Math.Clamp(radius, 25.0, 500.0);
	}

	// ---- Internal ----

	// Deferred server-side registration. Reads the owner's origin, resolves the faction
	// from an optional FactionAffiliationComponent (most markers are faction-neutral),
	// builds a DCO_Objective, registers it, then starts the sync tick.
	protected void DCO_RegisterObjective()
	{
		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Resolve faction from the owner entity (optional; most GM markers are faction-neutral).
		Faction faction = null;
		FactionAffiliationComponent facComp = FactionAffiliationComponent.Cast(
			owner.FindComponent(FactionAffiliationComponent));
		if (facComp)
			faction = facComp.GetAffiliatedFaction();

		m_Objective = new DCO_Objective(
			m_eType,
			owner.GetOrigin(),
			m_fRadius,
			faction,
			m_iPriority,
			true	// m_bGMPlaced
		);

		DCO_ObjectiveRegistry.Get().RegisterGM(m_Objective);

		// Start the periodic sync that keeps pos/radius/priority live while the GM drags or edits.
		GetGame().GetCallqueue().CallLater(DCO_Tick, (int)(TICK_SEC * 1000.0), true);
	}

	// Server tick: push the entity origin + component fields into the live DCO_Objective
	// so dragging or editing the marker takes effect without re-placing it.
	protected void DCO_Tick()
	{
		if (!Replication.IsServer())
			return;

		if (!m_Objective)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		m_Objective.m_vPos			= owner.GetOrigin();
		m_Objective.m_eType			= m_eType;
		m_Objective.m_fRadius		= m_fRadius;
		m_Objective.m_iPriorityBias	= m_iPriority;
	}
}
