// Vehicle doctrine shared types: the role a section controller assigns to one vehicle crew,
// and the intent payload the per-crew executor (DCO_VehicleDoctrine.c) reads. Plain types
// only - no logic, no engine calls - so they're safe to reference from any DCO file
// regardless of folder load order.
enum EDCO_VehicleRole
{
	NONE,		// out of combat - no DCO order; vanilla navmesh owns movement
	LEAD,		// front of the bound / order of march; drives the section's tactical movement
	OVERWATCH,	// holds a position with LOS to the danger area and covers the bounding element
	ESCORT,		// gun truck: move to a covered spot between convoy and enemy and suppress (never halt on the road)
	TRAIL,		// rear security
	DISMOUNT,	// stuck transport: debus passengers to a base of fire off-road
	RECOVER,	// assist a disabled friendly vehicle (push aside / cross-load)
	DESTROY,	// own vehicle disabled & unrecoverable: crew dismounts and joins base of fire
}

// Per-crew order written by DCO_VehicleSectionController and consumed by the executor.
class DCO_VehicleIntent
{
	EDCO_VehicleRole	m_eRole			= EDCO_VehicleRole.NONE;
	vector				m_vTargetPos	= vector.Zero;	// navmesh-validated destination (set by the executor for debug)
	bool				m_bHasTarget	= false;
	vector				m_vEnemyPos		= vector.Zero;	// section's reference enemy position (for orientation / LOS)
	bool				m_bHasEnemy		= false;
	int					m_iSectionId	= -1;			// which section this crew belongs to (debug / dedupe)
	bool				m_bRoadBlocked	= false;		// section-level: the route ahead is blocked (drive-through impossible)
}
