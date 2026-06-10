// AI commander objective registry. Singleton holding GM-placed objectives
// (registered by DCO_ObjectiveZone) plus auto objectives re-derived each commander
// tick: own bases via DCO_CompositionRegistry, hot fights via the AI world. Auto
// objectives near a same-faction GM one are dropped (the GM marker wins).
class DCO_ObjectiveRegistry
{
	protected static ref DCO_ObjectiveRegistry	s_Instance;

	protected ref array<ref DCO_Objective>	m_aGMObjectives;
	protected ref array<ref DCO_Objective>	m_aAutoObjectives;

	static DCO_ObjectiveRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_ObjectiveRegistry();
		return s_Instance;
	}

	void DCO_ObjectiveRegistry()
	{
		m_aGMObjectives		= {};
		m_aAutoObjectives	= {};
	}

	void RegisterGM(DCO_Objective obj)
	{
		if (obj && m_aGMObjectives.Find(obj) == -1)
			m_aGMObjectives.Insert(obj);
	}

	void UnregisterGM(DCO_Objective obj)
	{
		if (obj)
			m_aGMObjectives.RemoveItem(obj);
	}

	// Rebuild auto objectives (own bases + hot friendly fights), then return GM + auto
	// combined. An auto objective within mergeDist of a same-faction GM objective is
	// dropped. (undermannedBelow is reserved for a future undermanned-base gate.)
	void BuildActive(out array<ref DCO_Objective> outAll, float baseRadius, int undermannedBelow, float hotThreat, float mergeDist)
	{
		m_aAutoObjectives.Clear();
		DCO_DeriveBaseDefends(baseRadius);
		DCO_DeriveHotFights(hotThreat);
		DCO_DeriveAreaObjectives();

		outAll = {};
		foreach (DCO_Objective g : m_aGMObjectives)
		{
			if (g)
				outAll.Insert(g);
		}

		float mergeSq = mergeDist * mergeDist;
		foreach (DCO_Objective a : m_aAutoObjectives)
		{
			if (!a)
				continue;
			bool superseded = false;
			foreach (DCO_Objective g : m_aGMObjectives)
			{
				if (g && g.m_Faction == a.m_Faction && vector.DistanceSq(g.m_vPos, a.m_vPos) <= mergeSq)
				{
					superseded = true;
					break;
				}
			}
			if (!superseded)
				outAll.Insert(a);
		}
	}

	// One DEFEND objective per own-faction composition.
	protected void DCO_DeriveBaseDefends(float baseRadius)
	{
		array<ref DCO_CompositionAnchor> anchors = {};
		DCO_CompositionRegistry.Get().GetAll(anchors);
		foreach (DCO_CompositionAnchor a : anchors)
		{
			if (!a || !a.m_Faction)
				continue;
			DCO_Objective obj = new DCO_Objective(DCO_EObjectiveType.DEFEND, a.m_vPos, baseRadius, a.m_Faction, 0, false);
			m_aAutoObjectives.Insert(obj);
		}
	}

	// Map-intelligence objectives (RECOM merge): turn key locations - villages and military
	// sites from DCO_MapAreaRegistry - into commander objectives, but only reactively, when
	// units are actually there. Composition-owned areas are skipped (their base-defend already
	// covers them); the value here is the key locations the GM did NOT place a composition on.
	//   - one faction present at a significant area -> that faction HOLDs it (dig in, reinforce).
	//   - two+ factions present (contested) -> each gets an ATTACK (commit reserves to take it).
	// This is what makes the commander fight over the map's towns instead of only its bases.
	protected void DCO_DeriveAreaObjectives()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg || !cfg.m_bEnableCommanderAreas)
			return;

		DCO_MapAreaRegistry.Get().EnsureBuilt();	// lazy one-time build; no-op once ready
		array<ref DCO_MapArea> areas = {};
		DCO_MapAreaRegistry.Get().GetAll(areas);
		if (!areas || areas.IsEmpty())
			return;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		// Unique groups + their leader positions, gathered once.
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		array<SCR_AIGroup> groups = {};
		array<vector> groupPos = {};
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
			groups.Insert(grp);
			groupPos.Insert(ldr.GetOrigin());
		}

		foreach (DCO_MapArea area : areas)
		{
			if (!area)
				continue;

			bool significant = area.m_eType == DCO_EMapAreaType.MILITARY || area.m_iWeight >= cfg.m_iAreaSignificantWeight;
			if (!significant)
				continue;

			// Skip if a composition already owns this spot (base-defend covers it).
			if (DCO_AreaHasComposition(area, cfg))
				continue;

			// Which factions have a group inside the area?
			float presenceSq = (area.m_fRadius * 1.5) * (area.m_fRadius * 1.5);
			array<Faction> present = {};
			for (int i = 0; i < groups.Count(); i++)
			{
				if (vector.DistanceSq(groupPos[i], area.m_vCenter) > presenceSq)
					continue;
				Faction f = groups[i].GetFaction();
				if (f && present.Find(f) == -1)
					present.Insert(f);
			}
			if (present.IsEmpty())
				continue;

			int bias = 10;
			if (area.m_eType == DCO_EMapAreaType.MILITARY)
				bias = 25;

			// High-ground bonus (RECOM topography): prefer elevated key locations.
			if (cfg.m_bAreaHighGroundBias && area.m_fProminence > 0)
			{
				int hg = (int)(area.m_fProminence * cfg.m_fAreaHighGroundWeight);
				if (hg > 25)
					hg = 25;
				bias += hg;
			}

			if (present.Count() == 1)
			{
				// Single occupant: hold the key location.
				m_aAutoObjectives.Insert(new DCO_Objective(DCO_EObjectiveType.HOLD, area.m_vCenter, area.m_fRadius, present[0], bias, false));
			}
			else
			{
				// Contested: every present faction commits to taking it.
				foreach (Faction f : present)
					m_aAutoObjectives.Insert(new DCO_Objective(DCO_EObjectiveType.ATTACK, area.m_vCenter, area.m_fRadius, f, bias, false));
			}
		}
	}

	// True if any faction's composition sits inside the area (so base-defend already owns it).
	protected bool DCO_AreaHasComposition(DCO_MapArea area, DCO_MoraleSettings cfg)
	{
		array<ref DCO_CompositionAnchor> anchors = {};
		DCO_CompositionRegistry.Get().GetAll(anchors);
		float rSq = area.m_fRadius * area.m_fRadius;
		foreach (DCO_CompositionAnchor a : anchors)
		{
			if (a && vector.DistanceSq(a.m_vPos, area.m_vCenter) <= rSq)
				return true;
		}
		return false;
	}

	// One SUPPORT objective at each own-faction group in contact under heavy threat.
	protected void DCO_DeriveHotFights(float hotThreat)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
			if (!util || !util.m_Perception)
				continue;
			array<IEntity> tgts = util.m_Perception.m_aTargetEntities;
			if (!tgts || tgts.IsEmpty())
				continue;
			if (util.GetThreatMeasure() < hotThreat)
				continue;

			IEntity ldr = grp.GetLeaderEntity();
			if (!ldr)
				continue;
			DCO_Objective obj = new DCO_Objective(DCO_EObjectiveType.SUPPORT, ldr.GetOrigin(), 150.0, grp.GetFaction(), 0, false);
			m_aAutoObjectives.Insert(obj);
		}
	}
}
