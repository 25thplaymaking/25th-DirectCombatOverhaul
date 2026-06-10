// Map-intelligence area types. A DCO_MapArea is one clustered region of interest on the
// map - a village, a military site, a forest, or a hand-tagged key point. The AI commander
// turns areas into objectives (defend the towns you hold, push the ones you don't).
//
// Ported and retagged from ReforgerCommander's village/forest/military clustering, which
// shipped only village clustering (convex hull) and left forest + military disabled. The
// DCO version finishes both: military is detected from the map's military descriptors, and
// the area shape is a centroid+radius the commander can consume directly (the optional
// convex hull is kept for visualization). Plain types only - no engine calls - so they're
// safe to reference from any DCO file regardless of folder load order.
enum DCO_EMapAreaType
{
	VILLAGE,	// built-up area (house/structure cluster)
	MILITARY,	// military site (base, camp, fortification)
	FOREST,		// wooded area (offline-baked only; see DCO_AreaSourceExternal)
	KEYPOINT,	// hand-tagged point of interest (from the external file)
}

class DCO_MapArea
{
	DCO_EMapAreaType	m_eType		= DCO_EMapAreaType.VILLAGE;
	vector				m_vCenter;					// area centroid (world)
	float				m_fRadius	= 100.0;		// enclosing radius (m)
	int					m_iWeight	= 1;			// member count (buildings/points); drives commander priority
	float				m_fProminence;				// metres above the surrounding terrain (high-ground signal); 0 if unknown
	ref array<vector>	m_aHull;					// optional area-outline polygon (XZ); may be null/empty

	void DCO_MapArea(DCO_EMapAreaType type, vector center, float radius, int weight)
	{
		m_eType		= type;
		m_vCenter	= center;
		m_fRadius	= radius;
		m_iWeight	= weight;
	}
}
