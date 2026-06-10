// Map-intelligence registry. Builds the set of map areas (villages / military sites /
// forests) ONCE per session and caches them - areas are static terrain, so there's no reason
// to rescan. The AI commander reads the cache to derive defend/attack objectives at key
// locations.
//
// Two tiers, auto-selected (DCO_MoraleSettings.m_iCommanderAreaMode):
//   - EXTERNAL: load a pre-baked file (DCO_AreaSourceExternal) - the full RECOM-style
//     intelligence including forests. Zero runtime cost; needs the file to exist.
//   - NATIVE: scan the world in-engine (this class) and cluster density into areas. Villages
//     come from building density (SCR_DestructibleBuildingComponent); military sites from the
//     map's military descriptors or a configurable prefab list; forests (optional, heavier)
//     from a configurable tree-prefab list. No backend, no file.
//   AUTO prefers the baked file when present and falls back to the native scan.
//
// Each native area also records its terrain prominence (DCO_TerrainUtil - high ground) and an
// outline polygon (DCO_ConcaveHull, falling back to convex) for the GM view. The scan is
// spread across frames so it never hitches the server. Server-only; plain singleton, no folder
// load-order constraints. Default OFF (m_bEnableCommanderAreas).
class DCO_MapAreaRegistry
{
	protected static ref DCO_MapAreaRegistry	s_Instance;

	protected ref array<ref DCO_MapArea>	m_aAreas;
	protected bool							m_bBuilt;
	protected bool							m_bBuilding;

	// Native-scan state (valid only while m_bBuilding).
	protected BaseWorld						m_ScanWorld;
	protected vector						m_vScanMin;
	protected float							m_fScanCell;
	protected int							m_iScanCols;
	protected int							m_iScanRows;
	protected int							m_iScanIndex;
	protected bool							m_bForest;				// forest pass enabled this scan
	protected string						m_sForestTok;			// forest prefab-name substring ("" = off)
	protected string						m_sMilTok;				// military prefab-name substring ("" = off)
	protected ref array<int>				m_aCellBuild;			// buildings per cell
	protected ref array<int>				m_aCellForest;			// forest entities per cell
	protected ref array<bool>				m_aCellMilitary;		// cell touched a military marker
	// Per-cell query accumulators (reset before each cell's query).
	protected int							m_iCellBuild;
	protected int							m_iCellForest;
	protected bool							m_bCellMilitary;

	static DCO_MapAreaRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_MapAreaRegistry();
		return s_Instance;
	}

	void DCO_MapAreaRegistry()
	{
		m_aAreas = {};
	}

	void GetAll(out array<ref DCO_MapArea> outAreas)
	{
		outAreas = m_aAreas;
	}

	bool IsBuilt()
	{
		return m_bBuilt;
	}

	// Start building the area set once (server + enabled). No-ops while built or building.
	void EnsureBuilt()
	{
		if (m_bBuilt || m_bBuilding)
			return;
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg || !cfg.m_bEnableCommanderAreas || cfg.m_iCommanderAreaMode == 3)	// 3 = OFF
			return;

		// EXTERNAL (1) or AUTO (0): try the baked file first.
		if (cfg.m_iCommanderAreaMode != 2)	// 2 = NATIVE-only
		{
			array<ref DCO_MapArea> loaded = {};
			if (DCO_AreaSourceExternal.Load(loaded) && !loaded.IsEmpty())
			{
				m_aAreas = loaded;
				m_bBuilt = true;
				if (cfg.m_bDebug)
					Print(string.Format("[DCO:AREA] loaded %1 baked areas (external tier)", m_aAreas.Count()), LogLevel.NORMAL);
				return;
			}
		}

		if (cfg.m_iCommanderAreaMode == 1)	// EXTERNAL-only with no file
			return;

		DCO_BeginNativeScan(cfg);
	}

	void Invalidate()
	{
		m_bBuilt = false;
		m_bBuilding = false;
		m_aAreas = {};
	}

	// ---- Native scan ----

	protected void DCO_BeginNativeScan(DCO_MoraleSettings cfg)
	{
		IEntity worldEnt = GetGame().GetWorldEntity();
		m_ScanWorld = GetGame().GetWorld();
		if (!worldEnt || !m_ScanWorld)
			return;

		vector mins, maxs;
		worldEnt.GetWorldBounds(mins, maxs);
		if (maxs[0] <= mins[0] || maxs[2] <= mins[2])
			return;

		m_fScanCell = cfg.m_fAreaCellSize;
		if (m_fScanCell < 50.0)
			m_fScanCell = 50.0;
		m_vScanMin = mins;
		m_iScanCols = (int)Math.Ceil((maxs[0] - mins[0]) / m_fScanCell);
		m_iScanRows = (int)Math.Ceil((maxs[2] - mins[2]) / m_fScanCell);
		int cells = m_iScanCols * m_iScanRows;
		if (cells <= 0 || cells > 1000000)
			return;

		m_bForest = cfg.m_bAreaIncludeForest;
		m_sForestTok = cfg.m_sAreaForestPrefabs;
		m_sMilTok = cfg.m_sAreaMilitaryPrefabs;

		m_aCellBuild	= {};
		m_aCellForest	= {};
		m_aCellMilitary	= {};
		for (int i = 0; i < cells; i++)
		{
			m_aCellBuild.Insert(0);
			m_aCellForest.Insert(0);
			m_aCellMilitary.Insert(false);
		}

		m_iScanIndex = 0;
		m_bBuilding = true;
		if (cfg.m_bDebug)
			Print(string.Format("[DCO:AREA] native scan start: %1x%2 cells @ %3m (forest=%4)", m_iScanCols, m_iScanRows, m_fScanCell, m_bForest), LogLevel.NORMAL);
		GetGame().GetCallqueue().CallLater(DCO_ScanStep, 1, false);
	}

	protected void DCO_ScanStep()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!m_bBuilding || !cfg)
			return;

		int total = m_iScanCols * m_iScanRows;
		int budget = cfg.m_iAreaCellsPerStep;
		if (budget < 1)
			budget = 64;

		int processed = 0;
		while (m_iScanIndex < total && processed < budget)
		{
			DCO_ScanCell(m_iScanIndex % m_iScanCols, m_iScanIndex / m_iScanCols);
			m_iScanIndex++;
			processed++;
		}

		if (m_iScanIndex < total)
		{
			GetGame().GetCallqueue().CallLater(DCO_ScanStep, 1, false);
			return;
		}

		DCO_FinishNativeScan(cfg);
	}

	protected void DCO_ScanCell(int col, int row)
	{
		float x0 = m_vScanMin[0] + col * m_fScanCell;
		float z0 = m_vScanMin[2] + row * m_fScanCell;
		vector mins = Vector(x0, -2000.0, z0);
		vector maxs = Vector(x0 + m_fScanCell, 5000.0, z0 + m_fScanCell);

		m_iCellBuild = 0;
		m_iCellForest = 0;
		m_bCellMilitary = false;
		if (m_ScanWorld)
			m_ScanWorld.QueryEntitiesByAABB(mins, maxs, DCO_OnScanEntity);

		int idx = row * m_iScanCols + col;
		m_aCellBuild[idx]	= m_iCellBuild;
		m_aCellForest[idx]	= m_iCellForest;
		m_aCellMilitary[idx]= m_bCellMilitary;
	}

	// Query callback: classify each entity by component then by prefab path.
	protected bool DCO_OnScanEntity(IEntity ent)
	{
		if (!ent)
			return true;

		if (ent.FindComponent(SCR_DestructibleBuildingComponent))
			m_iCellBuild++;

		if (ent.FindComponent(SCR_MilitaryBaseMapDescriptorComponent))
			m_bCellMilitary = true;

		// Prefab-name classification (military supplement + optional forests). Resolve the
		// prefab name once and test it against the configured substrings.
		bool wantMil = !m_bCellMilitary && m_sMilTok != string.Empty;
		bool wantForest = m_bForest && m_sForestTok != string.Empty;
		if (wantMil || wantForest)
		{
			EntityPrefabData pd = ent.GetPrefabData();
			if (pd)
			{
				ResourceName rn = pd.GetPrefabName();
				if (wantMil && rn.Contains(m_sMilTok))
					m_bCellMilitary = true;
				if (wantForest && rn.Contains(m_sForestTok))
					m_iCellForest++;
			}
		}

		return true;
	}

	protected void DCO_FinishNativeScan(DCO_MoraleSettings cfg)
	{
		m_aAreas = {};

		int minB = cfg.m_iAreaMinBuildings;
		if (minB < 1)
			minB = 1;
		DCO_ClusterPass(m_aCellBuild, minB, DCO_EMapAreaType.VILLAGE, true, cfg);

		if (m_bForest)
		{
			int minF = cfg.m_iAreaMinForest;
			if (minF < 1)
				minF = 1;
			DCO_ClusterPass(m_aCellForest, minF, DCO_EMapAreaType.FOREST, false, cfg);
		}

		m_aCellBuild	= null;
		m_aCellForest	= null;
		m_aCellMilitary	= null;
		m_ScanWorld		= null;
		m_bBuilding		= false;
		m_bBuilt		= true;
		if (cfg.m_bDebug)
			Print(string.Format("[DCO:AREA] native scan done: %1 areas", m_aAreas.Count()), LogLevel.NORMAL);

		// Offline bake (JSON-only m_bDumpMapIntelOnLoad): write the freshly-scanned areas to the
		// external-tier file so they can be hand-tuned and reused.
		if (cfg.m_bDumpMapIntelOnLoad)
			DumpToFile();
	}

	// Flood-fill 8-connected cells whose density >= minCount into clusters; emit one area each.
	protected void DCO_ClusterPass(array<int> density, int minCount, DCO_EMapAreaType baseType, bool useMilitary, DCO_MoraleSettings cfg)
	{
		int total = m_iScanCols * m_iScanRows;
		array<bool> visited = {};
		for (int i = 0; i < total; i++)
			visited.Insert(false);

		for (int start = 0; start < total; start++)
		{
			if (visited[start] || density[start] < minCount)
				continue;

			array<int> stack = {};
			array<int> members = {};
			stack.Insert(start);
			visited[start] = true;
			while (!stack.IsEmpty())
			{
				int c = stack[stack.Count() - 1];
				stack.Remove(stack.Count() - 1);
				members.Insert(c);

				int cc = c % m_iScanCols;
				int cr = c / m_iScanCols;
				for (int dz = -1; dz <= 1; dz++)
				{
					for (int dx = -1; dx <= 1; dx++)
					{
						if (dx == 0 && dz == 0)
							continue;
						int nc = cc + dx;
						int nr = cr + dz;
						if (nc < 0 || nc >= m_iScanCols || nr < 0 || nr >= m_iScanRows)
							continue;
						int ni = nr * m_iScanCols + nc;
						if (visited[ni] || density[ni] < minCount)
							continue;
						visited[ni] = true;
						stack.Insert(ni);
					}
				}
			}

			DCO_EmitArea(members, density, baseType, useMilitary, cfg);
		}
	}

	// Build one area from a set of cells: density-weighted centroid (from cell centres),
	// enclosing radius, terrain prominence, outline hull. Promote to MILITARY if any member
	// cell carried a military marker (buildings pass only).
	protected void DCO_EmitArea(array<int> members, array<int> density, DCO_EMapAreaType baseType, bool useMilitary, DCO_MoraleSettings cfg)
	{
		int totalW = 0;
		vector sum = vector.Zero;
		bool military = false;
		float half = m_fScanCell * 0.5;
		array<vector> cellPts = {};

		foreach (int c : members)
		{
			int cc = c % m_iScanCols;
			int cr = c / m_iScanCols;
			vector centre = Vector(m_vScanMin[0] + cc * m_fScanCell + half, 0, m_vScanMin[2] + cr * m_fScanCell + half);
			cellPts.Insert(centre);
			int w = density[c];
			totalW += w;
			sum = sum + centre * (float)w;
			if (useMilitary && m_aCellMilitary[c])
				military = true;
		}
		if (totalW <= 0)
			return;

		vector center = sum / (float)totalW;

		float radiusSq = 0;
		foreach (vector p : cellPts)
		{
			float d = vector.DistanceSq(p, center);
			if (d > radiusSq)
				radiusSq = d;
		}
		float radius = Math.Sqrt(radiusSq) + half;
		if (radius < m_fScanCell)
			radius = m_fScanCell;
		if (radius > cfg.m_fAreaMaxRadius)
			radius = cfg.m_fAreaMaxRadius;

		DCO_EMapAreaType type = baseType;
		if (military)
			type = DCO_EMapAreaType.MILITARY;

		// Place the centre on the surface and record how high it sits vs. its surroundings.
		center[1] = DCO_TerrainUtil.SurfaceHeight(center);
		float prominence = DCO_TerrainUtil.LocalProminence(center, radius, 8);

		DCO_MapArea area = new DCO_MapArea(type, center, radius, totalW);
		area.m_fProminence = prominence;
		array<vector> hull = {};
		DCO_ConcaveHull.Build(cellPts, hull);
		area.m_aHull = hull;
		m_aAreas.Insert(area);
	}

	// ---- offline bake (dump native areas to the external-tier file) ----

	// Serialise the current areas to $profile:DCO_MapIntel.json so they can be hand-tuned and
	// reused as the EXTERNAL tier (or copied to other servers). Returns false if nothing to dump.
	bool DumpToFile()
	{
		if (!m_aAreas || m_aAreas.IsEmpty())
			return false;

		DCO_MapIntelFileDto dto = new DCO_MapIntelFileDto();
		dto.mapName = GetGame().GetWorldFile();
		dto.types = {};
		dto.xs = {};
		dto.zs = {};
		dto.radii = {};
		dto.weights = {};
		foreach (DCO_MapArea a : m_aAreas)
		{
			if (!a)
				continue;
			dto.types.Insert(DCO_AreaTypeToString(a.m_eType));
			dto.xs.Insert(a.m_vCenter[0]);
			dto.zs.Insert(a.m_vCenter[2]);
			dto.radii.Insert(a.m_fRadius);
			dto.weights.Insert(a.m_iWeight);
		}
		dto.Pack();

		array<string> lines = {};
		lines.Insert(dto.AsString());
		bool ok = SCR_FileIOHelper.WriteFileContent(DCO_AreaSourceExternal.FILE_PATH, lines);
		Print(string.Format("[DCO:AREA] dumped %1 areas to %2 (ok=%3)", m_aAreas.Count(), DCO_AreaSourceExternal.FILE_PATH, ok), LogLevel.NORMAL);
		return ok;
	}

	protected string DCO_AreaTypeToString(DCO_EMapAreaType t)
	{
		switch (t)
		{
			case DCO_EMapAreaType.MILITARY:	return "MILITARY";
			case DCO_EMapAreaType.FOREST:	return "FOREST";
			case DCO_EMapAreaType.KEYPOINT:	return "KEYPOINT";
		}
		return "VILLAGE";
	}
}
