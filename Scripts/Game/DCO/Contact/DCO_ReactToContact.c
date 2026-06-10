// Reaction to contact - the squad's actions-on-contact drill (react, develop, choose,
// execute). When a group is in contact it runs the course of action chosen by the
// tactical brain (m_eDCO_COA); with no brain running it defaults to SUPPORT_BY_FIRE:
//   - SUPPORT_BY_FIRE / DEFEND : boost volume of fire and hold position (fix/suppress).
//   - ASSAULT_FLANK            : drive toward an assailable flank of the enemy
//                                (envelopment) while keeping fire up.
//   - BREAK_CONTACT            : move directly away from the threat and disengage.
//
// Phases talk through the shared m_eDCO_COA field, declared in DCO_TacticalBrain.c
// ("Brain" sorts before "Contact", and Enforce resolves modded-class fields in file
// order, so the declarer must come first). The assault drives a direct move to the
// computed flank rather than touching the later-processed path fields.
//
// "Contact" sorts before "QRF" so DCO_UpdateReactToContact is defined before the tick
// that calls it. Server-only, default OFF.

modded class SCR_AIGroupUtilityComponent
{
	// EDCO_COA + m_eDCO_COA are declared in DCO_TacticalBrain.c ("Brain"), processed
	// before this fragment, so this reader can see them.
	protected float	m_fDCO_LastReactTime	= -1;
	protected bool	m_bDCO_SupportFireApplied = false;

	protected static const float DCO_DEG2RAD_C = 0.0174533;

	void DCO_UpdateReactToContact()
	{
		if (!Replication.IsServer())
			return;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (!cfg || !cfg.m_bEnableReactToContact || !m_Owner || !m_Mailbox || !m_Perception)
		{
			if (cfg && m_Owner && m_bDCO_SupportFireApplied)
				DCO_ApplySupportFire(false, cfg);	// turned OFF: restore fire rate
			return;
		}

		// Out of contact: stand down, clearing any support-fire boost.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
		{
			if (m_bDCO_SupportFireApplied)
				DCO_ApplySupportFire(false, cfg);
			return;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastReactTime >= 0 && (now - m_fDCO_LastReactTime) < cfg.m_fReactCheckSec * 1000.0)
			return;
		m_fDCO_LastReactTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector selfPos = leader.GetOrigin();

		// Threat centroid + nearest enemy.
		vector sum = vector.Zero;
		int n = 0;
		IEntity nearest;
		float bestSq = 1e12;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			vector tp = t.GetOrigin();
			sum += tp;
			n++;
			float d = vector.DistanceSq(tp, selfPos);
			if (d < bestSq)
			{
				bestSq = d;
				nearest = t;
			}
		}
		if (n == 0 || !nearest)
			return;
		vector threatPos = sum / n;

		// COA: the brain's decision, or the safe default (gain fire superiority and hold).
		int coa = m_eDCO_COA;
		if (coa == EDCO_COA.AUTO)
			coa = EDCO_COA.SUPPORT_BY_FIRE;

		if (coa == EDCO_COA.BREAK_CONTACT)
		{
			DCO_ApplySupportFire(false, cfg);
			vector dir = selfPos - threatPos;
			dir[1] = 0;
			if (dir.LengthSq() > 0.01)
			{
				dir.Normalize();
				vector away = selfPos + dir * cfg.m_fBreakContactDistance;
				DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, away, m_Mailbox);
			}
			DCO_Debug.LogGroup("CONTACT", leader, "COA=BREAK_CONTACT");
			return;
		}

		if (coa == EDCO_COA.ASSAULT_FLANK)
		{
			// Envelopment: aim at an assailable flank of the enemy (rotate the enemy->self
			// bearing around the enemy, keeping standoff). The movement layer
			// (DCO_TacticalPath, if on) routes there via cover + bounding, and we keep fire
			// volume up during the assault.
			vector flank = DCO_FlankOf(threatPos, selfPos, cfg);
			DCO_ApplySupportFire(true, cfg);
			DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, flank, m_Mailbox);	// drive toward the flank
			DCO_Debug.LogGroup("CONTACT", leader, string.Format("COA=ASSAULT_FLANK -> %1", flank));
			return;
		}

		// SUPPORT_BY_FIRE / DEFEND (and anything else): gain fire superiority and hold.
		DCO_ApplySupportFire(true, cfg);
		DCO_Debug.LogGroup("CONTACT", leader, "COA=SUPPORT_BY_FIRE (hold + suppress)");
	}

	// Toggle the base-of-fire volume boost on every member (fire-rate coefficient).
	// Off restores 1.0.
	protected void DCO_ApplySupportFire(bool on, DCO_TacticalMoveSettings cfg)
	{
		if (on == m_bDCO_SupportFireApplied)
			return;	// only act on change (mirrors the morale-accuracy pattern)
		m_bDCO_SupportFireApplied = on;

		if (!m_Owner)
			return;

		float coef = 1.0;
		if (on)
			coef = cfg.m_fSupportFireRateCoef;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never touch a player's fire-rate
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (combat)
				combat.SetFireRateCoef(coef, false);
		}
	}

	// A point on an assailable flank of the enemy: rotate the enemy->self bearing by the
	// flank angle, keeping standoff, so the group swings onto the enemy's side not its front.
	protected vector DCO_FlankOf(vector threatPos, vector selfPos, DCO_TacticalMoveSettings cfg)
	{
		vector enemyToSelf = selfPos - threatPos;
		enemyToSelf[1] = 0;
		float standoff = enemyToSelf.Length();
		if (standoff < 1.0)
			return selfPos;
		vector dir = enemyToSelf / standoff;

		float ang = cfg.m_fAssaultFlankAngleDeg * DCO_DEG2RAD_C;
		float s = Math.Sin(ang);
		float c = Math.Cos(ang);
		vector rot = Vector(dir[0] * c - dir[2] * s, 0, dir[0] * s + dir[2] * c);
		return threatPos + rot * standoff;
	}
}
