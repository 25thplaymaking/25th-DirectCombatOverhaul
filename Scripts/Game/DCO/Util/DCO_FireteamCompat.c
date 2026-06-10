// Fireteam accessor compatibility shim.
// CRX used to add SCR_AIGroupFireteamManager.GetFireteams() (return the group's fireteams). The base 1.7
// API has no such method, and after the CRX update it is gone, so DCO callers broke ("Undefined function
// 'SCR_AIGroupFireteamManager.GetFireteams'"). This decouples DCO from that CRX-specific method by
// reconstructing the fireteam list from the base API only: iterate the group's agents and collect the unique
// fireteams via SCR_AIGroupFireteamManager.FindFireteam(agent). Naturally yields only non-empty fireteams
// (every returned fireteam has >= 1 member), which is what the DCO callers want (they skip empties).
// Plain static (order-independent) so any DCO file can call it regardless of folder load order.
class DCO_FireteamCompat
{
	// Fill outFireteams with the group's unique non-empty fireteams (base-API replacement for the removed
	// CRX GetFireteams()). Caller passes an empty array.
	static void GetAllFireteams(SCR_AIGroupFireteamManager mgr, SCR_AIGroup group, out array<ref SCR_AIGroupFireteam> outFireteams)
	{
		if (!mgr || !group)
			return;

		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			SCR_AIGroupFireteam ft = mgr.FindFireteam(a);
			if (ft && outFireteams.Find(ft) < 0)
				outFireteams.Insert(ft);
		}
	}
}
