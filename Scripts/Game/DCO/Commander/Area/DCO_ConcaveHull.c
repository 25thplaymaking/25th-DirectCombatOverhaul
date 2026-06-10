// Concave hull (k-nearest-neighbours, Moreira & Santos). This is the finished version of the
// concave/alpha-shape hull ReforgerCommander shipped disabled ("forests will not work with
// convex hulls; we need concave hulls"). It hugs an irregular point set far tighter than a
// convex hull. Used only for area visualization - the commander itself consumes centre+radius,
// so this never affects gameplay. Hard-bounded (k escalation capped, step count capped) so it
// can never hang, and it falls back to DCO_AreaHull's convex result on any failure.
class DCO_ConcaveHull
{
	// Concave hull of pts (XZ). Fewer than 3 points returns a copy; on failure returns the
	// convex hull. Output is an ordered loop.
	static void Build(array<vector> pts, out array<vector> outHull)
	{
		int n = pts.Count();
		outHull = {};
		if (n < 3)
		{
			foreach (vector p : pts)
				outHull.Insert(p);
			return;
		}

		int kMax = n - 1;
		if (kMax > 12)
			kMax = 12;
		for (int k = 3; k <= kMax; k++)
		{
			array<vector> hull = {};
			if (DCO_TryKnn(pts, k, hull))
			{
				outHull = hull;
				return;
			}
		}

		DCO_AreaHull.Build(pts, outHull);	// fallback: convex
	}

	// One concave attempt with neighbourhood size k. Returns false (caller escalates k) if it
	// gets stuck or exceeds the step cap.
	protected static bool DCO_TryKnn(array<vector> pts, int k, out array<vector> hull)
	{
		int n = pts.Count();
		hull = {};

		// Start at the lowest-Z point (tie: lowest X).
		int startIdx = 0;
		for (int i = 1; i < n; i++)
		{
			if (pts[i][2] < pts[startIdx][2] || (pts[i][2] == pts[startIdx][2] && pts[i][0] < pts[startIdx][0]))
				startIdx = i;
		}

		array<bool> used = {};
		for (int i = 0; i < n; i++)
			used.Insert(false);

		vector start = pts[startIdx];
		hull.Insert(start);
		used[startIdx] = true;
		vector current = start;
		float prevAngle = 0;	// previous travel direction (radians)
		int steps = 0;
		int maxSteps = n * 2;

		while (steps < maxSteps)
		{
			steps++;

			// After three vertices, allow closing back to the start.
			if (hull.Count() > 3)
				used[startIdx] = false;

			// k nearest unused points to current.
			array<int> cand = DCO_NearestUnused(pts, used, current, k);
			if (cand.IsEmpty())
				return false;

			// Sort candidates by descending right-hand turn from the previous direction so the
			// walk hugs the boundary clockwise.
			DCO_SortByTurn(pts, cand, current, prevAngle);

			// Take the first candidate whose new edge doesn't cross the hull so far.
			int picked = -1;
			foreach (int ci : cand)
			{
				if (!DCO_EdgeCrossesHull(hull, current, pts[ci]))
				{
					picked = ci;
					break;
				}
			}
			if (picked < 0)
				return false;

			if (picked == startIdx)
				return true;	// closed the loop cleanly

			vector next = pts[picked];
			prevAngle = Math.Atan2(next[2] - current[2], next[0] - current[0]);
			hull.Insert(next);
			used[picked] = true;
			current = next;
		}

		return false;	// step cap hit
	}

	protected static array<int> DCO_NearestUnused(array<vector> pts, array<bool> used, vector from, int k)
	{
		array<int> idx = {};
		array<float> dist = {};
		for (int i = 0; i < pts.Count(); i++)
		{
			if (used[i])
				continue;
			idx.Insert(i);
			dist.Insert(vector.DistanceSq(pts[i], from));
		}
		// Partial selection sort for the k smallest.
		int take = k;
		if (take > idx.Count())
			take = idx.Count();
		for (int a = 0; a < take; a++)
		{
			int best = a;
			for (int b = a + 1; b < idx.Count(); b++)
				if (dist[b] < dist[best])
					best = b;
			if (best != a)
			{
				float td = dist[a]; dist[a] = dist[best]; dist[best] = td;
				int ti = idx[a]; idx[a] = idx[best]; idx[best] = ti;
			}
		}
		array<int> outIdx = {};
		for (int a = 0; a < take; a++)
			outIdx.Insert(idx[a]);
		return outIdx;
	}

	// Sort candidate indices by descending clockwise turn angle relative to prevAngle.
	protected static void DCO_SortByTurn(array<vector> pts, array<int> cand, vector from, float prevAngle)
	{
		array<float> turn = {};
		foreach (int ci : cand)
		{
			float a = Math.Atan2(pts[ci][2] - from[2], pts[ci][0] - from[0]);
			float delta = prevAngle - a;
			while (delta <= 0)
				delta += 6.2831853;
			while (delta > 6.2831853)
				delta -= 6.2831853;
			turn.Insert(delta);
		}
		for (int i = 1; i < cand.Count(); i++)
		{
			float kt = turn[i];
			int ki = cand[i];
			int j = i - 1;
			while (j >= 0 && turn[j] < kt)
			{
				turn[j + 1] = turn[j];
				cand[j + 1] = cand[j];
				j--;
			}
			turn[j + 1] = kt;
			cand[j + 1] = ki;
		}
	}

	// True if segment from->to crosses any non-adjacent existing hull edge.
	protected static bool DCO_EdgeCrossesHull(array<vector> hull, vector from, vector to)
	{
		int c = hull.Count();
		// Skip the last edge (shares 'from'); test the rest.
		for (int i = 0; i < c - 2; i++)
		{
			if (DCO_SegIntersect(from, to, hull[i], hull[i + 1]))
				return true;
		}
		return false;
	}

	protected static bool DCO_SegIntersect(vector a, vector b, vector c, vector d)
	{
		float d1 = DCO_Cross(c, d, a);
		float d2 = DCO_Cross(c, d, b);
		float d3 = DCO_Cross(a, b, c);
		float d4 = DCO_Cross(a, b, d);
		if (((d1 > 0 && d2 < 0) || (d1 < 0 && d2 > 0)) && ((d3 > 0 && d4 < 0) || (d3 < 0 && d4 > 0)))
			return true;
		return false;
	}

	protected static float DCO_Cross(vector o, vector a, vector b)
	{
		return (a[0] - o[0]) * (b[2] - o[2]) - (a[2] - o[2]) * (b[0] - o[0]);
	}
}
