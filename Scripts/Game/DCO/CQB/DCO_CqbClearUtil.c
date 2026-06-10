// CQB town-clearing geometry/query helpers. Plain statics (order-independent).
// Every returned position is navmesh-validated; a node that won't snap is dropped,
// never forced. Interior structure comes from the building's baked AISmartActionComponent
// points (window/corner/cover firing positions), with a navmesh-sampled ring fallback
// for buildings that have none.
class DCO_CqbClearUtil
{
	// True if this group is running the clearing state machine (so the reactive DCO_CQB
	// push yields). Routed through a static because DCO_CQB.c sorts before DCO_CqbClear.c
	// and can't otherwise reach the later fragment's flag.
	static bool IsClearingActive(SCR_AIGroupUtilityComponent util)
	{
		return util && util.DCO_CqbIsClearingActive();
	}

	// Snap to navmesh near cand; false if nothing reachable is close.
	static bool SnapToNavmesh(AIPathfindingComponent pf, vector cand, out vector snapped)
	{
		if (!pf)
			return false;
		return pf.GetClosestPositionOnNavmesh(cand, Vector(6, 3, 6), snapped);
	}

	// True if the line between two points (at height h) is blocked.
	static bool LosBlocked(AIPathfindingComponent pf, vector a, vector b, float h)
	{
		if (!pf)
			return true;
		vector hit;
		return pf.RayTrace(a + Vector(0, h, 0), b + Vector(0, h, 0), hit);
	}

	// Collect a building's interior nodes: accessible smart-action points within the
	// footprint, navmesh-snapped. If none, fall back to a sampled ring + center.
	static void CollectInteriorNodes(AIPathfindingComponent pf, IEntity building, float footprint, out array<vector> nodes)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || !building)
			return;
		vector center = building.GetOrigin();

		DCO_CqbSACollector col = new DCO_CqbSACollector();
		world.QueryEntitiesBySphere(center, footprint, col.Collect);

		foreach (IEntity e : col.m_aPoints)
		{
			if (!e)
				continue;
			vector snapped;
			if (SnapToNavmesh(pf, e.GetOrigin(), snapped))
				nodes.Insert(snapped);
		}

		if (!nodes.IsEmpty())
			return;

		// Fallback: sample a small ring + center inside the footprint (navmesh-snapped).
		vector c;
		if (SnapToNavmesh(pf, center, c))
			nodes.Insert(c);
		int samples = 6;
		float r = footprint * 0.5;
		for (int i = 0; i < samples; i++)
		{
			float ang = (i / (float)samples) * 6.2831853;
			vector cand = center + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * r;
			vector s;
			if (SnapToNavmesh(pf, cand, s))
				nodes.Insert(s);
		}
	}

	// Choose the entry: the interior node nearest the approaching group (natural breach side).
	static bool PickEntry(array<vector> nodes, vector fromPos, out vector entry)
	{
		bool found = false;
		float bestSq = 1000000000.0;
		foreach (vector n : nodes)
		{
			float d = vector.DistanceSq(n, fromPos);
			if (!found || d < bestSq)
			{
				bestSq = d;
				entry = n;
				found = true;
			}
		}
		return found;
	}

	// Order nodes into a sweep path from the entry (greedy nearest-neighbour).
	static void OrderNodesFrom(array<vector> nodes, vector entry, out array<vector> ordered)
	{
		array<vector> remaining = {};
		foreach (vector n : nodes)
			remaining.Insert(n);

		vector cur = entry;
		while (!remaining.IsEmpty())
		{
			int bestIdx = -1;
			float bestSq = 1000000000.0;
			for (int i = 0; i < remaining.Count(); i++)
			{
				float d = vector.DistanceSq(remaining[i], cur);
				if (bestIdx < 0 || d < bestSq)
				{
					bestSq = d;
					bestIdx = i;
				}
			}
			cur = remaining[bestIdx];
			ordered.Insert(cur);
			remaining.Remove(bestIdx);
		}
	}

	// Single-file stack positions behind the entry along the approach vector, navmesh-snapped.
	static void StackOffsets(AIPathfindingComponent pf, vector entry, vector fromPos, int count, float spacing, out array<vector> offsets)
	{
		vector dir = fromPos - entry;	// points back toward the group (behind the breach)
		dir[1] = 0;
		if (dir.LengthSq() < 0.01)
			dir = Vector(0, 0, 1);
		dir.Normalize();
		for (int i = 0; i < count; i++)
		{
			float dist = spacing * (i + 1);
			vector cand = entry + dir * dist;
			vector s;
			if (SnapToNavmesh(pf, cand, s))
				offsets.Insert(s);
			else
				offsets.Insert(entry);	// fall back to the breach point if no snap
		}
	}

	// True if an enemy this group perceives is inside/near the building footprint.
	static bool EnemyInsideBuilding(SCR_AIGroupPerception perception, IEntity building, float footprint)
	{
		if (!perception || !building)
			return false;
		array<IEntity> targets = perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return false;
		vector center = building.GetOrigin();
		float rSq = footprint * footprint;
		foreach (IEntity t : targets)
		{
			if (t && vector.DistanceSq(t.GetOrigin(), center) <= rSq)
				return true;
		}
		return false;
	}
}

// Tiny collector object for QueryEntitiesBySphere, so DCO_CqbClearUtil's statics stay
// reentrant (no shared scratch). Keeps entities with an accessible AI smart action
// (a free firing/cover point).
class DCO_CqbSACollector
{
	ref array<IEntity> m_aPoints = {};

	bool Collect(IEntity e)
	{
		if (!e)
			return true;
		AISmartActionComponent sa = AISmartActionComponent.Cast(e.FindComponent(AISmartActionComponent));
		if (sa && sa.IsActionAccessible())
			m_aPoints.Insert(e);
		return true;
	}
}
