// CQB town-clearing shared types. Plain types only (no engine calls) so any DCO
// file can reference them regardless of folder order.
enum EDCO_CqbState
{
	IDLE,		// not clearing (no assignment, or busy fleeing/mounted)
	SELECT,		// pick + claim the nearest claimable building in radius
	APPROACH,	// move to the entry point and form the stack
	ENTER,		// send members single-file through the breach
	CLEAR,		// sweep interior nodes one by one, scanning each
	DONE,		// nothing left to clear; cancel and resume the ordered move
}

// One building-clearing job for one group.
class DCO_CqbJob
{
	IEntity				m_Building;
	ref array<vector>	m_aNodes	= {};	// interior nodes, nearest-first sweep path, navmesh-snapped
	int					m_iNodeIdx	= 0;	// node currently being cleared
	vector				m_vEntry;			// chosen entry/breach point
	bool				m_bHasEntry	= false;
	float				m_fNodeEnterTime = -1;	// world time (ms) the current node scan began (dwell)
}
