// Lean AI commander - a toggleable global coordination layer built on systems we
// already ship, not a new game mode. A single server-side coordinator wakes on a
// timer, builds the active objective set (GM-placed + auto-derived), scores each by
// urgency, and vectors the nearest uncommitted reserves to the top ones by reusing
// the existing reinforcement primitives. It's the battlefield-wide director that the
// per-group reinforcement layer can't be.
//
// Plain singleton (not a modded fragment, so no file-order constraints). Started once,
// lazily, from the group tick (DCO_Commander.EnsureRunning) and then self-paced via a
// repeating CallLater. Server-only, default OFF: when off it never starts, and toggling
// it off at runtime makes the tick no-op.
class DCO_Commander
{
	protected static ref DCO_Commander		s_Instance;
	protected bool							m_bRunning;
	protected ref map<SCR_AIGroup, float>	m_mLastDispatch;	// group -> world time (ms) it was last vectored
	protected ref array<ref Shape>			m_aVizShapes;	// held GM plan spheres (released + rebuilt each tick)

	static DCO_Commander Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_Commander();
		return s_Instance;
	}

	// Start the coordinator's repeating tick once (server + enabled only). Cheap to
	// call every group tick.
	static void EnsureRunning()
	{
		if (!Replication.IsServer())
			return;
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableCommander)
			return;

		DCO_Commander c = Get();
		if (c.m_bRunning)
			return;
		c.m_bRunning = true;
		c.m_mLastDispatch = new map<SCR_AIGroup, float>();
		GetGame().GetCallqueue().CallLater(c.Update, (int)(cfg.m_fCommanderIntervalSec * 1000.0), true);
	}

	// Repeating coordinator tick: assess all active objectives and vector reserves.
	void Update()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableCommander)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();

		// 1) Build the active objective set (GM-placed + auto-derived, deduped).
		array<ref DCO_Objective> objectives = {};
		DCO_ObjectiveRegistry.Get().BuildActive(objectives, cfg.m_fCmdBaseRadius, cfg.m_iCmdUndermannedBelow, cfg.m_fCommanderThreatTrigger, cfg.m_fCmdMergeDist);
		if (objectives.IsEmpty())
			return;

		// 2) Gather groups once (deduped).
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		array<SCR_AIGroup> groups = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (grp && !seen.Contains(grp))
			{
				seen.Insert(grp);
				groups.Insert(grp);
			}
		}

		// 3) Score each objective.
		foreach (DCO_Objective obj : objectives)
			obj.m_fThreatScore = DCO_ScoreObjective(obj, groups, cfg);

		// 4) Assign reserves to the most-urgent objectives (main effort first) and issue orders.
		DCO_AssignAndOrder(objectives, groups, cfg, now);

		// 5) GM visualization.
		if (cfg.m_bCmdVisualize)
			DCO_DrawPlan(objectives);
	}

	// Urgency score: GM bias + type weight + enemy strength near it + friendly deficit.
	// Higher means act sooner.
	protected float DCO_ScoreObjective(DCO_Objective obj, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg)
	{
		float score = obj.m_iPriorityBias;
		switch (obj.m_eType)
		{
			case DCO_EObjectiveType.SUPPORT: score += cfg.m_fCmdWeightContact; break;
			case DCO_EObjectiveType.DEFEND:  score += cfg.m_fCmdWeightDefend; break;
			case DCO_EObjectiveType.ATTACK:  score += cfg.m_fCmdWeightAttack; break;
		}
		float rSq = obj.m_fRadius * obj.m_fRadius;
		int enemyNear = 0;
		int friendlyNear = 0;
		foreach (SCR_AIGroup g : groups)
		{
			IEntity ldr = g.GetLeaderEntity();
			if (!ldr)
				continue;
			if (vector.DistanceSq(ldr.GetOrigin(), obj.m_vPos) > rSq)
				continue;
			if (g.GetFaction() == obj.m_Faction)
				friendlyNear++;
			else
				enemyNear++;
		}
		score += enemyNear * cfg.m_fCmdWeightEnemy;
		if (enemyNear > friendlyNear)
			score += (enemyNear - friendlyNear) * cfg.m_fCmdWeightDeficit;
		return score;
	}

	// Assign nearest uncommitted reserves to objectives in descending score (main effort
	// first). DEFEND/HOLD get move + defender posture; SUPPORT/ATTACK get
	// reinforcement-eligible + move.
	protected void DCO_AssignAndOrder(array<ref DCO_Objective> objectives, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg, float now)
	{
		array<ref DCO_Objective> ordered = {};
		array<ref DCO_Objective> pool = {};
		foreach (DCO_Objective o : objectives)
			pool.Insert(o);
		while (!pool.IsEmpty())
		{
			int bi = 0;
			for (int i = 1; i < pool.Count(); i++)
				if (pool[i].m_fThreatScore > pool[bi].m_fThreatScore)
					bi = i;
			ordered.Insert(pool[bi]);
			pool.Remove(bi);
		}

		foreach (DCO_Objective obj : ordered)
		{
			if (obj.m_fThreatScore < cfg.m_fCmdActThreshold)
				continue;
			int assigned = 0;
			while (assigned < cfg.m_iCmdGroupsPerObjective)
			{
				SCR_AIGroup best = DCO_NearestReserve(obj, groups, cfg, now);
				if (!best)
					break;
				SCR_AIGroupUtilityComponent util = best.GetGroupUtilityComponent();
				if (!util)
					break;

				if (obj.m_eType == DCO_EObjectiveType.DEFEND || obj.m_eType == DCO_EObjectiveType.HOLD)
					util.DCO_SetDefender(true);		// hold and face the threat on arrival
				else
					util.DCO_SetReinforcementEligible(true);
				DCO_VehicleUtil.OrderGroupMoveToPosition(best, obj.m_vPos, util.m_Mailbox);

				m_mLastDispatch.Set(best, now);
				assigned++;
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("COMMANDER", best.GetLeaderEntity(), string.Format("-> %1 (score %2)", typename.EnumToString(DCO_EObjectiveType, obj.m_eType), obj.m_fThreatScore));
			}
		}
	}

	// Nearest own-faction reserve to the objective that's not in contact and off cooldown.
	protected SCR_AIGroup DCO_NearestReserve(DCO_Objective obj, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg, float now)
	{
		SCR_AIGroup best;
		float bestSq = cfg.m_fCommanderDispatchRange * cfg.m_fCommanderDispatchRange;
		foreach (SCR_AIGroup g : groups)
		{
			if (g.GetFaction() != obj.m_Faction)
				continue;
			SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
			if (!util || !util.m_Perception)
				continue;
			array<IEntity> tgts = util.m_Perception.m_aTargetEntities;
			if (tgts && !tgts.IsEmpty())
				continue;
			float last;
			if (m_mLastDispatch.Find(g, last) && (now - last) < cfg.m_fCommanderReissueSec * 1000.0)
				continue;
			IEntity ldr = g.GetLeaderEntity();
			if (!ldr)
				continue;
			float dSq = vector.DistanceSq(ldr.GetOrigin(), obj.m_vPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = g;
			}
		}
		return best;
	}

	// GM plan view: a wireframe sphere per objective, coloured by type, rebuilt each
	// draw. Renders client-side, so a listen-host/SP GM sees it but a remote dedi GM won't.
	protected void DCO_DrawPlan(array<ref DCO_Objective> objectives)
	{
		m_aVizShapes = {};
		foreach (DCO_Objective obj : objectives)
		{
			Shape s = Shape.CreateSphere(DCO_ObjectiveColour(obj.m_eType), ShapeFlags.WIREFRAME | ShapeFlags.NOZBUFFER, obj.m_vPos, obj.m_fRadius);
			if (s)
				m_aVizShapes.Insert(s);
		}
	}

	protected int DCO_ObjectiveColour(DCO_EObjectiveType t)
	{
		switch (t)
		{
			case DCO_EObjectiveType.SUPPORT: return 0xFFFF3C3C;	// red - a fight
			case DCO_EObjectiveType.DEFEND:  return 0xFF3C78FF;	// blue - defend
			case DCO_EObjectiveType.ATTACK:  return 0xFFFFA000;	// orange - attack
		}
		return 0xFFA0A0A0;	// grey
	}
}
