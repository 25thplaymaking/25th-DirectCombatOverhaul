// Composition registry (faction "base" awareness). A lazily-refreshed singleton that caches the
// positions + factions of placed compositions (the checkpoints / bunkers / sandbag works a Game Master
// drops for a side). AI groups read this cache to answer two cheap questions about their immediate
// surroundings: "is there an own-faction composition within R?" and "how many?". One scan feeds every
// group, so groups never query the world themselves.
//
// Detection: the editor's SCR_EditableEntityCore lists every editable entity. We take the top-level
// (onlyDirect) ones, exclude the dynamic unit types (characters / vehicles / groups / waypoints / items /
// comments / factions / tasks) - what's left is the placed structures / compositions / slots / systems -
// and keep the ones that carry an own-faction FactionAffiliationComponent. Excluding the dynamic types is
// deliberately robust: GM compositions report various structural EEditableEntityType values across game
// versions, but they are never a soldier or a vehicle.
//
// Lazy refresh: a rescan happens at most once per m_fCompositionScanSec; otherwise callers read the cache.
// Server-authoritative (the only callers are server-gated DCO group ticks). Returns nothing while the
// master toggle (DCO_MoraleSettings.m_bEnableCompositionDefense) is off.
//
// Runtime-unknown (confirm in-GM, default OFF so it is safe): whether a GM-placed faction composition's
// root entity carries a FactionAffiliationComponent. If a placed composition is not detected, that is the
// spot to extend (read the faction off the editable entity instead). The exact structural
// EEditableEntityType a composition reports is sidestepped by the exclusion approach above.
class DCO_CompositionAnchor
{
	vector	m_vPos;
	Faction	m_Faction;
}

class DCO_CompositionRegistry
{
	protected static ref DCO_CompositionRegistry s_Instance;

	protected ref array<ref DCO_CompositionAnchor>	m_aAnchors;
	protected float									m_fLastScanMs = -1;

	static DCO_CompositionRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_CompositionRegistry();
		return s_Instance;
	}

	void DCO_CompositionRegistry()
	{
		m_aAnchors = {};
	}

	// Count own-faction compositions within radius of pos. Refreshes the cache if it is stale.
	int CountNear(vector pos, Faction faction, float radius)
	{
		RefreshIfStale();
		if (!faction)
			return 0;

		int n = 0;
		float rSq = radius * radius;
		foreach (DCO_CompositionAnchor a : m_aAnchors)
		{
			if (a.m_Faction != faction)
				continue;
			if (vector.DistanceSq(a.m_vPos, pos) <= rSq)
				n++;
		}
		return n;
	}

	// Nearest own-faction composition within radius. Returns false (and leaves anchorPos untouched) if none.
	bool GetNearest(vector pos, Faction faction, float radius, out vector anchorPos)
	{
		RefreshIfStale();
		if (!faction)
			return false;

		bool found = false;
		float bestSq = radius * radius;
		foreach (DCO_CompositionAnchor a : m_aAnchors)
		{
			if (a.m_Faction != faction)
				continue;
			float dSq = vector.DistanceSq(a.m_vPos, pos);
			if (dSq <= bestSq)
			{
				bestSq = dSq;
				anchorPos = a.m_vPos;
				found = true;
			}
		}
		return found;
	}

	// Snapshot all cached composition anchors (refreshes if stale). For the AI commander's defend objectives.
	void GetAll(out array<ref DCO_CompositionAnchor> outAnchors)
	{
		RefreshIfStale();
		outAnchors = m_aAnchors;
	}

	// Rescan at most once per scan interval. Clears the cache when the system is disabled.
	protected void RefreshIfStale()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg || !cfg.m_bEnableCompositionDefense)
		{
			m_aAnchors.Clear();
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fLastScanMs >= 0 && (now - m_fLastScanMs) < cfg.m_fCompositionScanSec * 1000.0)
			return;
		m_fLastScanMs = now;

		Rescan();
	}

	protected void Rescan()
	{
		m_aAnchors.Clear();

		SCR_EditableEntityCore core = SCR_EditableEntityCore.Cast(SCR_GameCoresManager.GetCore(SCR_EditableEntityCore));
		if (!core)
			return;

		set<SCR_EditableEntityComponent> ents = new set<SCR_EditableEntityComponent>();
		core.GetAllEntities(ents, true, true);	// onlyDirect = top-level placed entities; skipIgnored

		foreach (SCR_EditableEntityComponent ec : ents)
		{
			if (!ec || !DCO_IsStructureType(ec.GetEntityType()))
				continue;

			IEntity owner = ec.GetOwner();
			if (!owner)
				continue;

			FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
			if (!fac)
				continue;
			Faction f = fac.GetAffiliatedFaction();
			if (!f)
				continue;

			DCO_CompositionAnchor a = new DCO_CompositionAnchor();
			a.m_vPos = owner.GetOrigin();
			a.m_Faction = f;
			m_aAnchors.Insert(a);
		}
	}

	// A placed composition/structure is an editable entity that is not one of the dynamic unit types.
	// Robust to the exact composition EEditableEntityType (GENERIC / SLOT / SYSTEM all qualify); we never
	// want a soldier, vehicle, group, etc. counted as a "composition".
	protected bool DCO_IsStructureType(EEditableEntityType t)
	{
		switch (t)
		{
			case EEditableEntityType.CHARACTER:
			case EEditableEntityType.VEHICLE:
			case EEditableEntityType.GROUP:
			case EEditableEntityType.WAYPOINT:
			case EEditableEntityType.ITEM:
			case EEditableEntityType.COMMENT:
			case EEditableEntityType.FACTION:
			case EEditableEntityType.TASK:
				return false;
		}
		return true;
	}
}
