// Convex-hull helper for map areas (Andrew's monotone chain, on the XZ plane). Returns the
// hull as an ordered vector loop. Used for area visualization and as the polygon stored on a
// DCO_MapArea; the commander itself consumes the simpler centroid+radius. This is the
// finished version of ReforgerCommander's cluster-hull step (it shipped a convex hull for
// villages and had concave/alpha-shape disabled). Pure math, no engine calls.
class DCO_AreaHull
{
	// Build the convex hull of pts (XZ used; Y carried from the lowest input for placement).
	// Fewer than 3 points returns a copy of the input. Output winds counter-clockwise.
	static void Build(array<vector> pts, out array<vector> outHull)
	{
		outHull = {};
		int n = pts.Count();
		if (n < 3)
		{
			foreach (vector p : pts)
				outHull.Insert(p);
			return;
		}

		// Sort by X then Z (insertion sort - cluster point counts are small).
		array<vector> s = {};
		foreach (vector p : pts)
			s.Insert(p);
		for (int i = 1; i < s.Count(); i++)
		{
			vector key = s[i];
			int j = i - 1;
			while (j >= 0 && (s[j][0] > key[0] || (s[j][0] == key[0] && s[j][2] > key[2])))
			{
				s[j + 1] = s[j];
				j--;
			}
			s[j + 1] = key;
		}

		array<vector> hull = {};

		// Lower hull.
		for (int i = 0; i < s.Count(); i++)
		{
			while (hull.Count() >= 2 && DCO_Cross(hull[hull.Count() - 2], hull[hull.Count() - 1], s[i]) <= 0)
				hull.Remove(hull.Count() - 1);
			hull.Insert(s[i]);
		}

		// Upper hull.
		int lower = hull.Count() + 1;
		for (int i = s.Count() - 2; i >= 0; i--)
		{
			while (hull.Count() >= lower && DCO_Cross(hull[hull.Count() - 2], hull[hull.Count() - 1], s[i]) <= 0)
				hull.Remove(hull.Count() - 1);
			hull.Insert(s[i]);
		}

		// Drop the duplicated start point that closes the loop.
		if (hull.Count() > 1)
			hull.Remove(hull.Count() - 1);

		outHull = hull;
	}

	// 2D cross product of OA x OB on the XZ plane (> 0 = counter-clockwise turn).
	protected static float DCO_Cross(vector o, vector a, vector b)
	{
		return (a[0] - o[0]) * (b[2] - o[2]) - (a[2] - o[2]) * (b[0] - o[0]);
	}
}
