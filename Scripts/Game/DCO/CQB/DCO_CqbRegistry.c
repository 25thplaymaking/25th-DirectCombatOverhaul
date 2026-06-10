// CQB building clear-state registry. Server-side singleton so multiple clearing
// groups split the work: a building is claimed by one group while it clears, then
// marked cleared (which persists) so nobody re-sweeps it. Mirrors DCO_TaskZoneRegistry.
enum EDCO_CqbBuildingState
{
	UNCLEARED,
	CLAIMED,
	CLEARED,
}

class DCO_CqbRegistry
{
	protected static ref DCO_CqbRegistry s_Instance;
	protected ref map<IEntity, EDCO_CqbBuildingState>	m_mState;
	protected ref map<IEntity, SCR_AIGroup>				m_mOwner;

	static DCO_CqbRegistry Get()
	{
		if (!s_Instance)
		{
			s_Instance = new DCO_CqbRegistry();
			s_Instance.m_mState = new map<IEntity, EDCO_CqbBuildingState>();
			s_Instance.m_mOwner = new map<IEntity, SCR_AIGroup>();
		}
		return s_Instance;
	}

	// Claimable by grp if uncleared, or already claimed by this same group. Cleared
	// buildings are never re-claimed.
	bool IsClaimable(IEntity building, SCR_AIGroup grp)
	{
		if (!building)
			return false;
		EDCO_CqbBuildingState st;
		if (!m_mState.Find(building, st))
			return true;	// unknown = unclaimed
		if (st == EDCO_CqbBuildingState.CLEARED)
			return false;
		if (st == EDCO_CqbBuildingState.CLAIMED)
		{
			SCR_AIGroup owner;
			m_mOwner.Find(building, owner);
			return owner == grp;
		}
		return true;
	}

	void Claim(IEntity building, SCR_AIGroup grp)
	{
		if (!building || !grp)
			return;
		m_mState.Set(building, EDCO_CqbBuildingState.CLAIMED);
		m_mOwner.Set(building, grp);
	}

	// Release a claimed building back to uncleared (e.g. the clearer aborted). Never
	// downgrades a cleared building.
	void Release(IEntity building)
	{
		if (!building)
			return;
		EDCO_CqbBuildingState st;
		if (m_mState.Find(building, st) && st == EDCO_CqbBuildingState.CLEARED)
			return;
		m_mState.Set(building, EDCO_CqbBuildingState.UNCLEARED);
		m_mOwner.Remove(building);
	}

	void MarkCleared(IEntity building)
	{
		if (!building)
			return;
		m_mState.Set(building, EDCO_CqbBuildingState.CLEARED);
		m_mOwner.Remove(building);
	}

	EDCO_CqbBuildingState GetState(IEntity building)
	{
		EDCO_CqbBuildingState st;
		if (building && m_mState.Find(building, st))
			return st;
		return EDCO_CqbBuildingState.UNCLEARED;
	}
}
