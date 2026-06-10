// Terrain / elevation helper for map intelligence. Samples the engine surface height
// (GetSurfaceY) to answer "is this spot high ground?". This is the useful half of RECOM's
// topography scan - RECOM sampled terrain heights but only ever fed them to its map renderer;
// DCO wires elevation into commander priority (hold/take the high ground). Pure read-only
// sampling; cheap when called once per area at build time.
class DCO_TerrainUtil
{
	// Surface height at a world XZ (0 if the world isn't ready).
	static float SurfaceHeight(vector pos)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;
		return world.GetSurfaceY((int)pos[0], (int)pos[2]);
	}

	// Prominence: how far the centre sits above the average of a ring at outerRadius.
	// Positive = high ground (hilltop / ridge), negative = a hollow / valley. Returns 0 if
	// the world isn't ready. samples is clamped to [4, 16].
	static float LocalProminence(vector center, float outerRadius, int samples)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world || outerRadius < 1.0)
			return 0;

		int n = samples;
		if (n < 4) n = 4;
		if (n > 16) n = 16;

		float centerH = world.GetSurfaceY((int)center[0], (int)center[2]);
		float ringSum = 0;
		for (int i = 0; i < n; i++)
		{
			float ang = (i / (float)n) * 6.2831853;
			float sx = center[0] + Math.Cos(ang) * outerRadius;
			float sz = center[2] + Math.Sin(ang) * outerRadius;
			ringSum += world.GetSurfaceY((int)sx, (int)sz);
		}
		return centerH - (ringSum / n);
	}
}
