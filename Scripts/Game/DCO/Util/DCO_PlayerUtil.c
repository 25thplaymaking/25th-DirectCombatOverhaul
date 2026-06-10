// Player guard. DCO is server-side AI behaviour and must NEVER drive a player-controlled
// character. In PVE a player commonly leads (or sits in) an AI squad, so the player's character
// appears in that group's agent list - and without this check DCO's per-member combat holds
// (weapon no-fire, fire-rate, forced stance, weapon-lowered, surrender freeze) would land on the
// player, who then "can't pull the trigger". Every per-member combat-state mutation calls this
// first. Standalone static (like DCO_VehicleUtil / DCO_StanceUtil) so any fragment can use it.
class DCO_PlayerUtil
{
	// True if the entity is currently controlled by a player.
	static bool IsPlayer(IEntity ent)
	{
		if (!ent)
			return false;
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;
		return pm.GetPlayerIdFromControlledEntity(ent) != 0;
	}
}
