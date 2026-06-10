// Tactical brain / course-of-action selection - the group's decision layer (the
// "choose an action" step of actions-on-contact). While in contact it weighs
// relative combat power and writes a COA (m_eDCO_COA) that the reaction-to-contact
// drill executes:
//   - stronger (own:enemy >= assault ratio): ASSAULT_FLANK  (hit the open flank)
//   - overmatched (<= break ratio):          BREAK_CONTACT  (disengage, save the force)
//   - roughly even:                          SUPPORT_BY_FIRE (fix/suppress first)
//
// Talks only through shared SCR_AIGroupUtilityComponent fields, so it calls no other
// DCO fragment and folder order is irrelevant to correctness. "Brain" still sorts
// before "QRF" so DCO_UpdateTacticalBrain is defined before the tick that calls it
// (the brain runs before the contact drill). Server-only, default OFF.

// Course of action for a group in contact. AUTO = no decision yet (use the default).
// Declared in this earliest-processed fragment so later fragments (Contact) can read
// m_eDCO_COA - Enforce resolves modded-class fields in file order too, so the
// declarer must sort before any reader.
enum EDCO_COA
{
	AUTO,
	DEFEND,
	SUPPORT_BY_FIRE,
	ASSAULT_FLANK,
	BREAK_CONTACT,
	ADVANCE
}

modded class SCR_AIGroupUtilityComponent
{
	protected int	m_eDCO_COA				= EDCO_COA.AUTO;	// chosen COA; read by the contact drill (Phase C)
	protected float	m_fDCO_LastBrainTime	= -1;

	// Current course of action. Read by the standoff layer so it can drop the
	// minimum-engagement-distance floor while the group is committed to an assault.
	int DCO_GetCOA()
	{
		return m_eDCO_COA;
	}

	void DCO_UpdateTacticalBrain()
	{
		if (!Replication.IsServer())
			return;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (!cfg || !cfg.m_bEnableTacticalBrain || !m_Owner || !m_Perception)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastBrainTime >= 0 && (now - m_fDCO_LastBrainTime) < cfg.m_fBrainCheckSec * 1000.0)
			return;
		m_fDCO_LastBrainTime = now;

		// No contact: clear the COA and let default/idle behaviour run.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		bool inContact = targets && !targets.IsEmpty();
		if (!inContact)
		{
			m_eDCO_COA = EDCO_COA.AUTO;
			return;
		}

		// Own strength = living members.
		int own = 0;
		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (a && a.GetControlledEntity())
				own++;
		}

		// Enemy strength = perceived targets. Rough but cheap; could refine later with
		// an AIWorld scan inside m_fBrainEnemyScanRadius.
		int enemy = 0;
		foreach (IEntity t : targets)
		{
			if (t)
				enemy++;
		}
		if (enemy < 1)
			enemy = 1;

		float ratio = own / (float)enemy;

		int coa;
		if (ratio <= cfg.m_fBrainBreakPowerRatio)
			coa = EDCO_COA.BREAK_CONTACT;
		else if (ratio >= cfg.m_fBrainAssaultPowerRatio)
			coa = EDCO_COA.ASSAULT_FLANK;
		else
			coa = EDCO_COA.SUPPORT_BY_FIRE;

		m_eDCO_COA = coa;

		DCO_Debug.LogGroup("BRAIN", m_Owner.GetLeaderEntity(), string.Format("own=%1 enemy=%2 ratio=%3 -> COA=%4", own, enemy, ratio, coa));
	}
}
