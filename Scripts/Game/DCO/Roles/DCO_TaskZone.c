// GM placeable Task Zone (the "circle") - foundation.
// A GM-placeable circle that assigns a DCO task to the groups inside it. This is the foundation the
// individual behaviours layer on. Pattern referenced from 25thGMERemix's placeable "Modules": a
// placeable editable entity carries a role + radius; at runtime it links the groups whose leader sits
// inside the circle and applies the matching DCO role flag (the same flags the GM attributes /
// DCO_GroupRolePreset set). Moving/resizing/deleting the zone updates the assignment.
//
// "Highlight a group" in practice means drop the circle over the group you want (its leader inside the
// radius). No custom selection-capture framework needed, so it works in vanilla Game Master on a
// dedicated 24/7 server with no extra setup. Explicit click-to-link via a context action can be added
// later as a convenience on top of this.
//
// Role: what it does to groups inside (reuses existing DCO behaviour, see [[group-role-presets...]]):
//   QRF        : DCO_SetQRFResponder(true) + remembers this circle as the hold/rally point
//   DEFEND     : DCO_SetDefender(true)
//   REINFORCE  : DCO_SetReinforcementEligible(true)
//   AMBUSH     : DCO_SetAmbusher(true) (holds fire); springs from its own range or a paired trigger zone
//   AMBUSH_TRIGGER : kill zone: when an enemy (AI or player) enters, springs every AMBUSH zone sharing
//                    the same Pair Tag (the detached kill-zone the user asked for)
//
// Server-authoritative (all DCO logic is server-side). Ticks on a CallLater loop; unregisters on delete.
// A standalone component (not a SCR_AIGroupUtilityComponent fragment), so it can call the role accessors
// on the merged modded class freely.
enum EDCO_ZoneRole
{
	NONE,
	QRF,			// groups inside become QRF responders; this circle is their hold/rally point
	DEFEND,			// groups inside hold + orient (garrison-lite)
	REINFORCE,		// groups inside are reinforcement-eligible (DCO may pull them to a contact)
	AMBUSH,			// groups inside hold fire until sprung (own range or a paired trigger zone)
	AMBUSH_TRIGGER,	// kill zone: an enemy entering here springs paired AMBUSH zones
	CONVOY,			// sticky: vehicle crews inside are branded with this zone's Convoy ID, locked into one section (vehicle doctrine). Retained after they drive out / the zone is deleted.
	CLEAR,			// sticky: groups inside are flagged CQB Clearer (methodical building clearing)
}

// Global registry of live task zones so zones can find each other (trigger/ambush pairing) and so
// behaviours can query them. Pruned of dead entries on access.
class DCO_TaskZoneRegistry
{
	protected static ref array<DCO_TaskZoneComponent> s_aZones;

	static void Register(DCO_TaskZoneComponent z)
	{
		if (!s_aZones)
			s_aZones = {};
		if (s_aZones.Find(z) < 0)
			s_aZones.Insert(z);
	}

	static void Unregister(DCO_TaskZoneComponent z)
	{
		if (s_aZones)
		{
			int i = s_aZones.Find(z);
			if (i >= 0)
				s_aZones.Remove(i);
		}
	}

	static array<DCO_TaskZoneComponent> GetZones()
	{
		if (!s_aZones)
			s_aZones = {};
		return s_aZones;
	}
}

class DCO_TaskZoneComponentClass : ScriptComponentClass
{
}

class DCO_TaskZoneComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "What task this circle assigns to the groups inside it.", "", ParamEnumArray.FromEnum(EDCO_ZoneRole), category: "25th DCO")]
	EDCO_ZoneRole m_eRole;

	[Attribute("50", UIWidgets.Slider, "Circle radius (m). Groups whose leader is inside become assigned; for a kill zone, an enemy inside springs the ambush.", "5 500 5", category: "25th DCO")]
	float m_fRadius;

	[Attribute("3", UIWidgets.Slider, "How often (s) the zone re-evaluates which groups/enemies are inside.", "0.5 15 0.5", category: "25th DCO")]
	float m_fCheckSec;

	[Attribute("0", UIWidgets.Slider, "Pair ID linking an Ambush Position to its Kill-Zone(s). Set the SAME non-zero number on a position and each of its kill-zones. 0 = unlinked: a kill-zone then springs the NEAREST ambush position and deletes nothing.", "0 50 1", category: "25th DCO")]
	int m_iPairId;

	[Attribute("1", UIWidgets.Slider, "Convoy ID (only used when Role = CONVOY): every vehicle crew inside this circle is branded with this id and locked into ONE section under it, regardless of distance. The tag is STICKY - kept after the vehicles drive out and after the zone is deleted. Use different ids for separate convoys.", "1 64 1", category: "25th DCO")]
	int m_iConvoyId;

	// Groups this zone is currently managing, so we can clear the role when a group leaves the circle or
	// the zone is deleted (don't strand a flag on a group forever).
	protected ref array<SCR_AIGroup> m_aManaged = {};

	// Kill-zone latch: once an AMBUSH_TRIGGER has fired it never fires again, and for tagged ambushes it is
	// the survivor while its sibling kill-zones are deleted.
	protected bool m_bDCO_Tripped = false;

	// GM-visible ground circle. Drawn with the engine Shape visualizer (no asset dependency, guaranteed
	// circle) and role-coloured, replacing the old base-game AIWaypoint_Defend prefab which rendered every
	// zone as a red defensive/danger marker (so a Defend zone "read as a kill zone") and whose area-mesh disc
	// didn't generate reliably ("no circle"). The same asset-free approach as DCO_QRFRangeVisual.
	// Drawn on every machine (not server-gated) so the Game Master - a client on a dedicated server -
	// actually sees it; a Shape created server-side would never reach the GM. Refreshed on the tick so it
	// follows a moved/resized zone. The held ref keeps it alive; clearing the ref (destructor) frees it.
	protected ref Shape m_DCO_VisualShape;
	protected static const float DCO_VISUAL_HEIGHT = 3.0;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame() || !GetGame().InPlayMode())
			return;

		// Visual runs on every machine (incl. the GM client on a dedicated server). Deferred once so the GM's
		// final placement transform is set, then refreshed on the tick so it tracks a moved/resized zone.
		GetGame().GetCallqueue().CallLater(DCO_DrawVisual, 500, false);
		GetGame().GetCallqueue().CallLater(DCO_DrawVisual, (int)(m_fCheckSec * 1000.0), true);

		if (!Replication.IsServer())
			return;

		DCO_TaskZoneRegistry.Register(this);
		GetGame().GetCallqueue().CallLater(DCO_Tick, (int)(m_fCheckSec * 1000.0), true);
	}

	void ~DCO_TaskZoneComponent()
	{
		DCO_TaskZoneRegistry.Unregister(this);
		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(DCO_Tick);
			GetGame().GetCallqueue().Remove(DCO_DrawVisual);
		}
		DCO_ClearAllManaged();
		// Releasing the held Shape reference frees the visualizer (no entity to delete).
		m_DCO_VisualShape = null;
	}

	vector DCO_GetCenter()
	{
		IEntity owner = GetOwner();
		if (owner)
			return owner.GetOrigin();
		return vector.Zero;
	}

	float DCO_GetRadius()		{ return m_fRadius; }
	EDCO_ZoneRole DCO_GetRole()	{ return m_eRole; }
	int DCO_GetPairId()			{ return m_iPairId; }
	void DCO_SetPairId(int id)	{ m_iPairId = id; }

	// Flat (ground-plane, XZ) squared distance - ignore height so a unit on a different floor/slope still
	// counts as inside the circle. The engine has no vector.DistanceSqXZ.
	protected float DCO_FlatDistSq(vector a, vector b)
	{
		float dx = a[0] - b[0];
		float dz = a[2] - b[2];
		return dx * dx + dz * dz;
	}

	protected void DCO_Tick()
	{
		if (!Replication.IsServer() || m_eRole == EDCO_ZoneRole.NONE)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		// Visual circle is parented to this zone, so it follows automatically - no per-tick reposition.

		if (m_eRole == EDCO_ZoneRole.AMBUSH_TRIGGER)
		{
			DCO_TickTrigger();
			return;
		}

		DCO_TickGroupAssignment();
	}

	// ARGB ground-circle colour per role, so a Defend zone no longer reads as a red kill marker. Only the
	// actual kill zone (AMBUSH_TRIGGER) is red.
	protected int DCO_VisualColor()
	{
		switch (m_eRole)
		{
			case EDCO_ZoneRole.QRF:				return 0xFF3FA9F5;	// blue
			case EDCO_ZoneRole.DEFEND:			return 0xFF40C040;	// green
			case EDCO_ZoneRole.REINFORCE:		return 0xFFFFC000;	// amber
			case EDCO_ZoneRole.AMBUSH:			return 0xFFB050FF;	// purple
			case EDCO_ZoneRole.AMBUSH_TRIGGER:	return 0xFFFF3030;	// red = the actual kill zone
			case EDCO_ZoneRole.CONVOY:			return 0xFF00E5FF;	// cyan = convoy branding zone
			case EDCO_ZoneRole.CLEAR:			return 0xFFFF8C00;	// dark orange = clear zone
		}
		return 0xFFFFFFFF;
	}

	// Draw (or refresh) the GM-visible ground circle as a role-coloured wireframe cylinder at the zone origin,
	// sized to m_fRadius. Reassigning the held ref releases the previous shape (so a moved/resized zone tracks
	// cleanly). Asset-free; runs on every machine so the GM client sees it.
	protected void DCO_DrawVisual()
	{
		if (m_eRole == EDCO_ZoneRole.NONE || m_fRadius < 1.0)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		m_DCO_VisualShape = Shape.CreateCylinder(DCO_VisualColor(), ShapeFlags.WIREFRAME | ShapeFlags.NOZBUFFER, owner.GetOrigin(), m_fRadius, DCO_VISUAL_HEIGHT);
	}

	// Apply this zone's role to every group whose leader is inside the circle, and clear it from any
	// group that has since left.
	protected void DCO_TickGroupAssignment()
	{
		vector center = DCO_GetCenter();
		float radiusSq = m_fRadius * m_fRadius;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		array<SCR_AIGroup> inside = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || inside.Find(grp) >= 0)
				continue;
			IEntity leader = grp.GetLeaderEntity();
			if (!leader)
				continue;
			if (DCO_FlatDistSq(leader.GetOrigin(), center) <= radiusSq)
				inside.Insert(grp);
		}

		// Apply to groups now inside that we weren't already managing.
		for (int i = 0, c = inside.Count(); i < c; i++)
		{
			SCR_AIGroup grp = inside[i];
			if (m_aManaged.Find(grp) < 0)
			{
				DCO_ApplyRoleTo(grp, true);
				m_aManaged.Insert(grp);
			}
		}

		// Clear groups we were managing that have left the circle (or died).
		for (int i = m_aManaged.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup grp = m_aManaged[i];
			if (!grp || inside.Find(grp) < 0)
			{
				if (grp)
					DCO_ApplyRoleTo(grp, false);
				m_aManaged.Remove(i);
			}
		}
	}

	// Kill zone tick. Resolve the paired AMBUSH position(s); if a hostile is inside this circle, spring them
	// once. A tagged kill-zone then deletes its sibling kill-zones (same tag), keeping this one - so after an
	// enemy trips one of several same-name kill-zones, only the tripped circle survives. A blank kill-zone
	// springs the nearest ambush and deletes nothing. Latched so it only ever fires once.
	protected void DCO_TickTrigger()
	{
		if (m_bDCO_Tripped)
			return;

		array<DCO_TaskZoneComponent> positions = {};
		DCO_GetPairedPositions(positions);
		if (positions.IsEmpty())
			return;	// no armed ambush paired yet

		Faction ambusherFaction;
		foreach (DCO_TaskZoneComponent p : positions)
		{
			ambusherFaction = p.DCO_GetFirstManagedFaction();
			if (ambusherFaction)
				break;
		}
		if (!ambusherFaction)
			return;	// the paired ambush has no group assigned yet

		if (!DCO_EnemyInsideRelativeTo(ambusherFaction))
			return;

		// Tripped - spring every paired ambush position once.
		m_bDCO_Tripped = true;
		foreach (DCO_TaskZoneComponent p : positions)
			p.DCO_SpringManagedAmbushes();

		// Linked (Pair ID != 0): remove the now-redundant sibling kill-zones (same ID), keeping this one.
		// Unlinked (0): keep all.
		if (m_iPairId != 0)
			DCO_DeleteSiblingTriggers();
	}

	// Fill 'result' with the AMBUSH position zones this kill-zone is paired to:
	//   tag set   : every AMBUSH zone with the exact same tag (one position may have many kill-zones).
	//   tag blank : the single nearest AMBUSH zone (and DCO_TickTrigger deletes nothing when blank).
	protected void DCO_GetPairedPositions(out array<DCO_TaskZoneComponent> result)
	{
		array<DCO_TaskZoneComponent> zones = DCO_TaskZoneRegistry.GetZones();

		if (m_iPairId != 0)
		{
			foreach (DCO_TaskZoneComponent z : zones)
			{
				if (!z || z == this)
					continue;
				if (z.DCO_GetRole() == EDCO_ZoneRole.AMBUSH && z.DCO_GetPairId() == m_iPairId)
					result.Insert(z);
			}
			return;
		}

		// Pair ID 0: nearest AMBUSH zone by flat distance.
		vector center = DCO_GetCenter();
		DCO_TaskZoneComponent nearest;
		float bestSq = -1;
		foreach (DCO_TaskZoneComponent z : zones)
		{
			if (!z || z == this || z.DCO_GetRole() != EDCO_ZoneRole.AMBUSH)
				continue;
			float dSq = DCO_FlatDistSq(z.DCO_GetCenter(), center);
			if (bestSq < 0 || dSq < bestSq)
			{
				bestSq = dSq;
				nearest = z;
			}
		}
		if (nearest)
			result.Insert(nearest);
	}

	// Delete every other AMBUSH_TRIGGER that shares this kill-zone's tag (keep this one). Collect first, then
	// delete, so we never mutate the registry while iterating it; each deleted zone's destructor unregisters.
	protected void DCO_DeleteSiblingTriggers()
	{
		array<DCO_TaskZoneComponent> zones = DCO_TaskZoneRegistry.GetZones();
		array<IEntity> toDelete = {};
		foreach (DCO_TaskZoneComponent z : zones)
		{
			if (!z || z == this)
				continue;
			if (z.DCO_GetRole() != EDCO_ZoneRole.AMBUSH_TRIGGER)
				continue;
			if (z.DCO_GetPairId() != m_iPairId)
				continue;
			IEntity e = z.GetOwner();
			if (e)
				toDelete.Insert(e);
		}

		foreach (IEntity e : toDelete)
			SCR_EntityHelper.DeleteEntityAndChildren(e);
	}

	Faction DCO_GetFirstManagedFaction()
	{
		for (int i = 0, c = m_aManaged.Count(); i < c; i++)
		{
			SCR_AIGroup grp = m_aManaged[i];
			if (grp)
			{
				Faction f = grp.GetFaction();
				if (f)
					return f;
			}
		}
		return null;
	}

	// True if a character hostile to refFaction is inside this circle (checks AI + players).
	protected bool DCO_EnemyInsideRelativeTo(Faction refFaction)
	{
		vector center = DCO_GetCenter();
		float radiusSq = m_fRadius * m_fRadius;
		SCR_Faction refScr = SCR_Faction.Cast(refFaction);

		// AI
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (aiWorld)
		{
			array<AIAgent> agents = {};
			aiWorld.GetAIAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity ent = agent.GetControlledEntity();
				if (!ent)
					continue;
				if (DCO_FlatDistSq(ent.GetOrigin(), center) > radiusSq)
					continue;
				if (DCO_IsHostile(refScr, ent))
					return true;
			}
		}

		// Players
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			array<int> ids = {};
			pm.GetPlayers(ids);
			foreach (int id : ids)
			{
				IEntity ent = pm.GetPlayerControlledEntity(id);
				if (!ent)
					continue;
				if (DCO_FlatDistSq(ent.GetOrigin(), center) > radiusSq)
					continue;
				if (DCO_IsHostile(refScr, ent))
					return true;
			}
		}

		return false;
	}

	protected bool DCO_IsHostile(SCR_Faction refScr, IEntity ent)
	{
		if (!refScr)
			return false;
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return false;
		Faction other = fac.GetAffiliatedFaction();
		if (!other || other == refScr)
			return false;
		return refScr.IsFactionEnemy(other);
	}

	// Spring every ambush group this zone manages (called by a paired trigger zone).
	void DCO_SpringManagedAmbushes()
	{
		for (int i = 0, c = m_aManaged.Count(); i < c; i++)
		{
			SCR_AIGroup grp = m_aManaged[i];
			if (!grp)
				continue;
			SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
			if (util)
				util.DCO_SpringAmbush();
		}
	}

	// Apply (enable=true) or clear (enable=false) this zone's role on a group.
	protected void DCO_ApplyRoleTo(SCR_AIGroup grp, bool enable)
	{
		SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
		if (!util)
			return;

		switch (m_eRole)
		{
			case EDCO_ZoneRole.QRF:
			{
				util.DCO_SetQRFResponder(enable);
				if (enable)
					util.DCO_SetQRFHoldPosition(DCO_GetCenter());
				else
					util.DCO_ClearQRFHoldPosition();
				break;
			}
			case EDCO_ZoneRole.DEFEND:
			{
				util.DCO_SetDefender(enable);
				break;
			}
			case EDCO_ZoneRole.REINFORCE:
			{
				util.DCO_SetReinforcementEligible(enable);
				break;
			}
			case EDCO_ZoneRole.AMBUSH:
			{
				util.DCO_SetAmbusher(enable);
				break;
			}
			case EDCO_ZoneRole.CONVOY:
			{
				// Sticky branding: tag vehicle crews on entry; do not clear on leave/delete, so the convoy keeps
				// its id once it drives out from under the marker. Only vehicle crews are branded; the doctrine
				// controller ignores foot groups anyway.
				if (enable && util.DCO_IsVehicleCrew())
					util.DCO_SetConvoyId(m_iConvoyId);
				break;
			}
			case EDCO_ZoneRole.CLEAR:
			{
				// Sticky: flag groups to clear; do not unflag on leave (they sweep as they advance out).
				if (enable)
					util.DCO_SetCqbClearer(true);
				break;
			}
		}
	}

	protected void DCO_ClearAllManaged()
	{
		for (int i = m_aManaged.Count() - 1; i >= 0; i--)
		{
			SCR_AIGroup grp = m_aManaged[i];
			if (grp)
				DCO_ApplyRoleTo(grp, false);
		}
		m_aManaged.Clear();
	}
}
