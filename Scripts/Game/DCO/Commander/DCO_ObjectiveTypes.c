// AI commander objective types + record. A DCO_Objective is one thing the commander
// wants serviced. GM-placed objectives are backed by a real DCO_ObjectiveZone editable
// entity (draggable); auto objectives are lightweight records the registry derives each
// tick (undermanned bases, hot fights). Both feed the same scoring + order pipeline.
enum DCO_EObjectiveType
{
	DEFEND,		// hold a friendly area/base
	HOLD,		// hold current ground
	ATTACK,		// take an enemy position
	PATROL,		// roam within radius
	SUPPORT,	// assist a friendly group in contact
	RESERVE,	// stage, available for the next main effort
}

class DCO_Objective
{
	DCO_EObjectiveType	m_eType				= DCO_EObjectiveType.DEFEND;
	vector				m_vPos;
	float				m_fRadius			= 100.0;
	Faction				m_Faction;
	int					m_iPriorityBias		= 0;
	bool				m_bGMPlaced			= false;
	float				m_fThreatScore		= 0;
	int					m_iAssignedCount	= 0;

	void DCO_Objective(DCO_EObjectiveType type, vector pos, float radius, Faction faction, int priorityBias, bool gmPlaced)
	{
		m_eType			= type;
		m_vPos			= pos;
		m_fRadius		= radius;
		m_Faction		= faction;
		m_iPriorityBias	= priorityBias;
		m_bGMPlaced		= gmPlaced;
	}
}
