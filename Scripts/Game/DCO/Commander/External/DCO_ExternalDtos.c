// External AI commander - JSON DTOs. Request/response payloads exchanged with the
// external provider (the RECOM backend or a thin OpenAI proxy). Follows the
// JsonApiStruct pattern from ReforgerCommanderClient: extend JsonApiStruct, declare
// fields, RegV("field") each in the ctor, and the engine handles serialise/parse.
//
// Protocol (the backend talks to OpenAI/etc, same as RECOM):
//   POST <endpoint>  body = DCO_ExternalStateDto  { mapName, faction, summary }
//   200 response     body = DCO_ExternalOrdersDto { objectiveType, x, z, radius, priority, faction, reasoning }
// The response is the AI's main-effort recommendation for this tick, which DCO
// services with reserves. A full multi-order array is the next step.

// Battlefield state sent to the external AI. `summary` is a compact text digest of
// the tactical picture (factions, groups, contacts, objectives), built by DCO_ExternalCommander.
class DCO_ExternalStateDto : JsonApiStruct
{
	string	mapName;
	string	faction;
	string	summary;

	void DCO_ExternalStateDto()
	{
		RegV("mapName");
		RegV("faction");
		RegV("summary");
	}
}

// The AI's recommended main-effort order for this tick.
class DCO_ExternalOrdersDto : JsonApiStruct
{
	string	objectiveType;	// "DEFEND" | "HOLD" | "ATTACK" | "PATROL" | "SUPPORT" | "RESERVE"
	float	x;				// world position (east)
	float	z;				// world position (north)
	float	radius;
	int		priority;		// 0..100 GM-style bias
	string	faction;		// faction key the order serves (empty = the requesting faction)
	string	reasoning;		// optional AI rationale (logged when debug is on)

	void DCO_ExternalOrdersDto()
	{
		RegV("objectiveType");
		RegV("x");
		RegV("z");
		RegV("radius");
		RegV("priority");
		RegV("faction");
		RegV("reasoning");
	}
}
