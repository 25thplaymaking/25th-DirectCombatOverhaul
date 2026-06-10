// External AI commander (RECOM integration) - the "let an AI drive the orders" layer.
// On a timer it builds a compact battlefield summary, POSTs it to the configured
// external provider (the RECOM backend or a thin OpenAI proxy) with the server's API
// key, and applies the AI's recommended order as the main-effort objective in
// DCO_ObjectiveRegistry. The local DCO_Commander then services that with reserves
// using the existing move/defend/reinforce primitives.
//
// This ports ReforgerCommander's idea (an external brain drives in-game units) into
// DCO, updated to the 1.7 RestApi and made GM-compatible: a plain server-side
// singleton started from the shared group tick, no game mode required.
//
// Security: endpoint + API key come from the server profile JSON, never hardcoded.
// Server-only, default OFF; if off, it never starts.
class DCO_ExternalCommander
{
	protected static ref DCO_ExternalCommander		s_Instance;
	protected bool									m_bRunning;
	protected ref DCO_RestClient					m_Client;
	protected ref DCO_ExternalResponseHandler		m_Handler;
	protected ref DCO_Objective						m_ExternalObjective;	// current AI main-effort objective (held GM record)

	static DCO_ExternalCommander Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_ExternalCommander();
		return s_Instance;
	}

	// Start the external-commander loop once (server + enabled + configured). Cheap to
	// call every group tick.
	static void EnsureRunning()
	{
		if (!Replication.IsServer())
			return;
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableExternalCommander || cfg.m_sExternalEndpoint == string.Empty)
			return;

		DCO_ExternalCommander c = Get();
		if (c.m_bRunning)
			return;
		c.m_bRunning = true;
		c.m_Client = new DCO_RestClient(cfg.m_sExternalEndpoint);
		c.m_Handler = new DCO_ExternalResponseHandler();
		GetGame().GetCallqueue().CallLater(c.Tick, (int)(cfg.m_fExternalIntervalSec * 1000.0), true);
	}

	void Tick()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableExternalCommander)
			return;	// toggled off at runtime; the timer keeps ticking harmlessly
		if (!m_Client || !m_Client.IsReady())
			return;

		int prov = cfg.m_iExternalProvider;
		string summary = DCO_BuildSummary(cfg.m_sExternalFaction);
		string headers = DCO_ExternalProvider.BuildHeaders(prov, cfg.m_sExternalApiKey);
		string body;

		if (prov == 0)
		{
			// RAW: post DCO's battlefield state; the endpoint returns the order JSON directly.
			DCO_ExternalStateDto state = new DCO_ExternalStateDto();
			state.mapName = "";	// optional; the backend can derive context from summary
			state.faction = cfg.m_sExternalFaction;
			state.summary = summary;
			state.Pack();
			body = state.AsString();
		}
		else
		{
			// OPENAI / ANTHROPIC: wrap the summary in a chat request to the model.
			string sys = cfg.m_sExternalSystemPrompt;
			if (sys == string.Empty)
				sys = DCO_ExternalProvider.DefaultSystemPrompt(cfg.m_sExternalFaction);
			body = DCO_ExternalProvider.BuildBody(prov, cfg.m_sExternalModel, sys, summary, cfg.m_iExternalMaxTokens);
		}

		m_Client.Post(cfg.m_sExternalEvalPath, body, headers, m_Handler);
	}

	// Called by the response handler with the AI's recommended order JSON. Replaces the
	// prior external main-effort objective in the registry; DCO_Commander services it next tick.
	void ApplyOrderJson(string json)
	{
		if (json == string.Empty)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		// Pull the order JSON out of the provider's response envelope (RAW returns it as-is;
		// OPENAI/ANTHROPIC unwrap the chat message the model returned).
		string orderJson = DCO_ExternalProvider.ExtractContent(cfg.m_iExternalProvider, json);
		if (orderJson == string.Empty)
			return;

		DCO_ExternalOrdersDto dto = new DCO_ExternalOrdersDto();
		dto.ExpandFromRAW(orderJson);

		Faction f = DCO_ResolveFaction(dto.faction);
		if (!f)
			f = DCO_ResolveFaction(cfg.m_sExternalFaction);
		if (!f)
			return;	// can't resolve a faction to command, so ignore

		if (m_ExternalObjective)
			DCO_ObjectiveRegistry.Get().UnregisterGM(m_ExternalObjective);

		float radius = dto.radius;
		if (radius < 25.0)
			radius = 100.0;
		int priority = dto.priority;
		if (priority < 0) priority = 0;
		if (priority > 100) priority = 100;

		m_ExternalObjective = new DCO_Objective(DCO_ParseObjectiveType(dto.objectiveType), Vector(dto.x, 0, dto.z), radius, f, priority, true);
		DCO_ObjectiveRegistry.Get().RegisterGM(m_ExternalObjective);

		if (cfg.m_bDebug)
			Print(string.Format("[DCO:EXTCMD] order %1 @ (%2,%3) r%4 prio%5 | %6", dto.objectiveType, dto.x, dto.z, radius, priority, dto.reasoning), LogLevel.NORMAL);
	}

	// Compact text digest of the tactical picture for the AI: own groups (pos/contact)
	// and current objectives. Kept terse - the external brain reasons over it.
	protected string DCO_BuildSummary(string factionKey)
	{
		string s = "faction=" + factionKey + "\n";
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return s;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		int gi = 0;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			IEntity ldr = grp.GetLeaderEntity();
			if (!ldr)
				continue;
			Faction gf = grp.GetFaction();
			string fkey = "";
			if (gf)
				fkey = gf.GetFactionKey();

			SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
			bool inContact = false;
			float threat = 0;
			if (util)
			{
				if (util.m_Perception && util.m_Perception.m_aTargetEntities)
					inContact = !util.m_Perception.m_aTargetEntities.IsEmpty();
				threat = util.GetThreatMeasure();
			}
			vector p = ldr.GetOrigin();
			s = s + string.Format("group#%1 f=%2 pos=%3,%4 contact=%5 threat=%6\n", gi, fkey, (int)p[0], (int)p[2], inContact, threat);
			gi++;
			if (gi >= 60)
				break;	// cap the payload size
		}

		// Current objectives (GM + auto) for context.
		array<ref DCO_Objective> objs = {};
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		DCO_ObjectiveRegistry.Get().BuildActive(objs, cfg.m_fCmdBaseRadius, cfg.m_iCmdUndermannedBelow, cfg.m_fCommanderThreatTrigger, cfg.m_fCmdMergeDist);
		int oi = 0;
		foreach (DCO_Objective o : objs)
		{
			if (!o)
				continue;
			s = s + string.Format("obj#%1 type=%2 pos=%3,%4 r=%5\n", oi, typename.EnumToString(DCO_EObjectiveType, o.m_eType), (int)o.m_vPos[0], (int)o.m_vPos[2], (int)o.m_fRadius);
			oi++;
			if (oi >= 40)
				break;
		}
		return s;
	}

	// Resolve a faction by key, or null.
	protected Faction DCO_ResolveFaction(string key)
	{
		if (key == string.Empty)
			return null;
		SCR_FactionManager fm = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!fm)
			return null;
		return fm.GetFactionByKey(key);
	}

	// Map the order's type string to the enum (defaults to DEFEND).
	protected DCO_EObjectiveType DCO_ParseObjectiveType(string t)
	{
		if (t == "SUPPORT")	return DCO_EObjectiveType.SUPPORT;
		if (t == "ATTACK")	return DCO_EObjectiveType.ATTACK;
		if (t == "HOLD")	return DCO_EObjectiveType.HOLD;
		if (t == "PATROL")	return DCO_EObjectiveType.PATROL;
		if (t == "RESERVE")	return DCO_EObjectiveType.RESERVE;
		return DCO_EObjectiveType.DEFEND;	// default
	}
}

// REST response sink: routes the external AI's reply back into the commander.
class DCO_ExternalResponseHandler : DCO_RestResponseHandler
{
	override void OnResponse(string data)
	{
		DCO_ExternalCommander.Get().ApplyOrderJson(data);
	}
	override void OnFailure(int httpCode)
	{
		if (DCO_MoraleSettings.Get().m_bDebug)
			Print("[DCO:EXTCMD] external request failed, http=" + httpCode, LogLevel.WARNING);
	}
}
