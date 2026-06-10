// Shared stance control (anti-snap / anti-stance-war).
// Several DCO systems want to put a member into a specific stance: adjust-to-cover (DCO_CoverStance),
// suppression dig-in (DCO_Suppression), machine-gunner emplacement (DCO_MachineGunner) and the
// member-morale cower (DCO_MemberMorale). Each used to call CharacterControllerComponent.ForceStance()
// unconditionally on its own interval. Two problems came out of that: ForceStance is an instant snap, and
// re-issuing it every interval (even when the member is already in that stance) produces the jarring
// "insanely fast prone" the playtest saw; and the engine/Skarris combat AI keeps standing the member back
// up to aim/move, so the DCO call and the engine fight every tick in a prone/up war.
//
// This is the single choke point for all DCO stance forcing. It never re-issues a stance the member
// already holds (kills the snap spam) and won't re-force the same member's stance again within
// minIntervalMs (bounds the war to at most one transition per interval instead of one per tick). It does
// not try to win the war outright (that is the engine's call); it just stops DCO from making it jarring.
//
// Standalone static class (like DCO_VehicleUtil) so it is callable from any fragment regardless of folder
// order. Server-side callers only. CharacterControllerComponent.GetStance() returns int:
// 0 STAND / 1 CROUCH / 2 PRONE; ForceStance(ECharacterStance) sets it.
class DCO_StanceUtil
{
	// Per-member world time (ms) DCO last forced a stance. Stale entries for dead members are harmless and the
	// map is cleared if it ever grows large (a throttle reset is cosmetically invisible).
	protected static ref map<IEntity, float> s_LastForceMs = new map<IEntity, float>();

	// Force a member's stance only if it is not already in that stance and DCO has not forced this member's
	// stance within minIntervalMs. Returns true if a transition was actually issued.
	static bool TrySetStance(IEntity ent, ECharacterStance desired, float minIntervalMs)
	{
		if (!ent)
			return false;

		if (DCO_PlayerUtil.IsPlayer(ent))
			return false;	// never force a player's stance

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return false;

		// Already in the desired stance: never re-snap. This alone removes the redundant instant-prone.
		int desiredInt = desired;
		if (cc.GetStance() == desiredInt)
			return false;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;
		float now = world.GetWorldTime();
		float last;
		if (s_LastForceMs.Find(ent, last) && (now - last) < minIntervalMs)
			return false;	// throttled: don't yank this member's stance again yet

		if (s_LastForceMs.Count() > 2000)
			s_LastForceMs.Clear();	// bound memory over a long session

		cc.ForceStance(desired);
		s_LastForceMs.Set(ent, now);
		return true;
	}
}
