// Vehicle doctrine geometry helpers. Plain static helpers (order-independent) for the
// controller + executor. Everything here returns a navmesh-validated position or fails - the
// safety contract is "never order a move to an unreachable / unsnapped point". Concealment is
// never decided here; callers use these only for safe geometry.
class DCO_VehicleDoctrineUtil
{
	// Plain-static gate for the Armor/Combat primitives: true if this crew is currently under an active
	// section-controller intent (so those primitives should yield). Routing the check through this static
	// helper keeps it order-independent - the Armor/Combat fragments load before the Convoy fragment, so a
	// direct modded-method call would not resolve (cross-fragment calls are file-order sensitive), but a
	// plain-static call resolves globally (same reason DCO_Commander can call DCO_VehicleUtil).
	static bool IsUnderDoctrineControl(SCR_AIGroupUtilityComponent util)
	{
		return util && util.DCO_HasActiveVehicleIntent();
	}

	// Snap a candidate to the navmesh near `cand`. Returns false if nothing reachable is close.
	static bool SnapToNavmesh(AIPathfindingComponent pf, vector cand, out vector snapped)
	{
		if (!pf)
			return false;
		return pf.GetClosestPositionOnNavmesh(cand, Vector(12, 5, 12), snapped);
	}

	// True if the line between two points (at height h) is blocked - reused for LOS to a danger area.
	static bool LosBlocked(AIPathfindingComponent pf, vector fromPos, vector toPos, float h)
	{
		if (!pf)
			return true;
		vector hit;
		return pf.RayTrace(fromPos + Vector(0, h, 0), toPos + Vector(0, h, 0), hit);
	}

	// Find the road tangent (unit direction along the road) nearest `pos`, plus the road width. Returns
	// false if there is no road close enough (so callers fall back to open-terrain behaviour).
	static bool RoadTangentAt(vector pos, float maxDist, out vector tangent, out float width)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;
		RoadNetworkManager rnm = aiWorld.GetRoadNetworkManager();
		if (!rnm)
			return false;

		BaseRoad road;
		float dist;
		rnm.GetClosestRoad(pos, road, dist);
		if (!road || dist > maxDist)
			return false;

		array<vector> pts = {};
		road.GetPoints(pts);
		if (pts.Count() < 2)
			return false;

		// Nearest segment; its direction is the road tangent at pos.
		int bestI = 0;
		float bestSq = 1000000000.0;
		for (int i = 0; i < pts.Count() - 1; i++)
		{
			vector mid = (pts[i] + pts[i + 1]) * 0.5;
			float dSq = vector.DistanceSq(mid, pos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				bestI = i;
			}
		}
		tangent = pts[bestI + 1] - pts[bestI];
		tangent[1] = 0;
		if (tangent.LengthSq() < 0.01)
			return false;
		tangent.Normalize();
		width = road.GetWidth();
		if (width < 2.0)
			width = 4.0;	// sane floor for narrow / zero-width roads
		return true;
	}

	// Herringbone shoulder point: offset `vehPos` to the given side (sideSign +1/-1) of the road by
	// (width/2 + margin), snapped to navmesh. Falls back to a perpendicular offset from the bearing to
	// the enemy if no road is found. Returns false if nothing safe is reachable (caller then holds).
	static bool HerringboneShoulder(AIPathfindingComponent pf, vector vehPos, vector enemyPos, int sideSign, float maxRoadDist, out vector outPos)
	{
		vector tangent;
		float width;
		vector lateral;
		if (RoadTangentAt(vehPos, maxRoadDist, tangent, width))
		{
			// Perpendicular to the road tangent, in the horizontal plane.
			lateral = Vector(-tangent[2], 0, tangent[0]);
		}
		else
		{
			// No road: push to the side relative to the enemy bearing so we clear the lane of fire.
			vector toEnemy = enemyPos - vehPos;
			toEnemy[1] = 0;
			if (toEnemy.LengthSq() < 0.01)
				return false;
			toEnemy.Normalize();
			lateral = Vector(-toEnemy[2], 0, toEnemy[0]);
			width = 6.0;
		}
		float offset = sideSign * (width * 0.5 + 2.0);	// int * float yields float (keep vector * float, never vector * int)
		vector cand = vehPos + lateral * offset;
		return SnapToNavmesh(pf, cand, outPos);
	}

	// Drive-through exit: a reachable road point `dist` metres further along the road, on the side away
	// from the enemy (continue past the kill zone). Falls back to "directly away from the enemy" off-road.
	static bool DriveThroughExit(AIPathfindingComponent pf, vector vehPos, vector enemyPos, float dist, float maxRoadDist, out vector outPos)
	{
		vector tangent;
		float width;
		if (RoadTangentAt(vehPos, maxRoadDist, tangent, width))
		{
			// Choose the tangent direction that points away from the enemy (don't drive into the ambush).
			vector toEnemy = enemyPos - vehPos;
			toEnemy[1] = 0;
			if (vector.Dot(tangent, toEnemy) > 0)
				tangent = -tangent;
			vector cand = vehPos + tangent * dist;
			if (SnapToNavmesh(pf, cand, outPos))
				return true;
		}
		// Off-road fallback: straight away from the enemy.
		vector away = vehPos - enemyPos;
		away[1] = 0;
		if (away.LengthSq() < 0.01)
			return false;
		away.Normalize();
		return SnapToNavmesh(pf, vehPos + away * dist, outPos);
	}

	// Overwatch / firing position: ring-sample around `vehPos` for the nearest navmesh-reachable spot with
	// clear LOS to `enemyPos` at sight height. When wantCover=true it returns a spot that breaks LOS
	// instead (hull-down). Returns false if none found.
	static bool FindLosPosition(AIPathfindingComponent pf, vector vehPos, vector enemyPos, float radius, int samples, float sightH, bool wantCover, out vector outPos)
	{
		bool found = false;
		float bestSq = 1000000000.0;
		for (int i = 0; i < samples; i++)
		{
			float ang = (i / (float)samples) * 6.2831853;
			vector cand = vehPos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * radius;
			vector snapped;
			if (!SnapToNavmesh(pf, cand, snapped))
				continue;
			bool blocked = LosBlocked(pf, snapped, enemyPos, sightH);
			bool ok;
			if (wantCover)
				ok = blocked;		// hide
			else
				ok = !blocked;		// engage
			if (!ok)
				continue;
			float dSq = vector.DistanceSq(snapped, vehPos);
			if (!found || dSq < bestSq)
			{
				bestSq = dSq;
				outPos = snapped;
				found = true;
			}
		}
		return found;
	}
}
