// Progressive aim zeroing. AI accuracy improves the longer a shooter holds fire on a
// target, resetting on LOS loss or target switch. Done group-level (matching our
// group-centric morale design) and gated on morale: a group only settles its aim while
// morale is healthy, so a suppressed or panicking group never zeroes in - the same
// fight that makes our morale systems bite also denies the enemy their accuracy ramp.
//
// Model is "zero in, don't exceed": a freshly-acquired target starts the aim cold and
// warms up to the unit's own trained skill over dwell time, never past it - so we never
// manufacture super-soldiers that beat the server's difficulty. Per-member skill steps
// ROOKIE (cold) to REGULAR (warming) to ResetAISkill (the trained default). The edge
// over a flat ramp is the morale gate plus an optional perception ramp (faster reacquire),
// which can exceed 1.0 because it's detection, not aim. The zero resets when all targets
// are lost for m_fZeroResetSec, or the primary target switches.
//
// Morale co-operation (no tug-of-war on SetAISkill): DCO_GroupMorale owns the skill lever
// while morale is degraded (band != 0). This fragment only writes skill while the accuracy
// band is 0 (healthy), read off m_iDCO_LastAccuracyBand. When the band is non-zero it
// leaves skill to morale. (With morale-accuracy disabled the band stays 0 and we own the
// lever; our top state ResetAISkill equals morale's band-0 state, so they never disagree.)
//
// modded SCR_AIGroupUtilityComponent fragment in Morale/. Sorts after DCO_GroupMorale.c so
// its fields are already declared, and before the QRF tick that calls DCO_UpdateAimZeroing().
// On-foot, server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	// Aim states: 0 DEFAULT (idle / not engaged), 1 COLD (just acquired), 2 WARM, 3 ZEROED (= trained default).
	protected static const int DCO_ZERO_DEFAULT	= 0;
	protected static const int DCO_ZERO_COLD	= 1;
	protected static const int DCO_ZERO_WARM	= 2;
	protected static const int DCO_ZERO_ZEROED	= 3;

	protected float		m_fDCO_LastZeroTime		= -1;	// world time (ms) zeroing was last evaluated (throttle)
	protected IEntity	m_eDCO_ZeroTarget;					// primary target we are currently zeroing on
	protected float		m_fDCO_ZeroDwellStart	= -1;		// world time (ms) we began holding the current target
	protected float		m_fDCO_ZeroLostSince	= -1;		// world time (ms) all targets were first lost (reset grace)
	protected int		m_iDCO_LastZeroState	= -1;		// last applied aim state (-1 = none yet)
	protected bool		m_bDCO_LastZeroHealthy;				// morale-healthy flag at last apply (re-apply on change)

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity, after DCO_UpdateMorale
	// so the accuracy band is current). Warms / resets the group's aim and applies it.
	void DCO_UpdateAimZeroing()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableAimZeroing)
		{
			// Turned off: restore perception once (and skill unless morale-accuracy owns it), then idle.
			if (m_Owner && m_iDCO_LastZeroState >= 0)
			{
				array<AIAgent> zr = {};
				m_Owner.GetAgents(zr);
				foreach (AIAgent za : zr)
				{
					if (!za) continue;
					IEntity ze = za.GetControlledEntity();
					if (!ze) continue;
					if (DCO_PlayerUtil.IsPlayer(ze)) continue;	// never touch a player's skill/perception
					SCR_AICombatComponent zc = SCR_AICombatComponent.Cast(ze.FindComponent(SCR_AICombatComponent));
					if (!zc) continue;
					zc.SetPerceptionFactor(cfg.m_fZeroBasePerception);
					if (!cfg.m_bEnableMoraleAccuracy)
						zc.ResetAISkill();
				}
				m_iDCO_LastZeroState = -1;
				m_eDCO_ZeroTarget = null;
				m_fDCO_ZeroDwellStart = -1;
			}
			return;
		}
		if (!m_Owner || !m_Perception)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;	// small-arms zeroing is for on-foot infantry

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastZeroTime >= 0 && (now - m_fDCO_LastZeroTime) < cfg.m_fZeroCheckSec * 1000.0)
			return;
		m_fDCO_LastZeroTime = now;

		bool healthy = !cfg.m_bEnableMoraleAccuracy || m_iDCO_LastAccuracyBand == 0;

		// Resolve the primary (nearest perceived) target and update the dwell/lost timers.
		IEntity primary = DCO_ZeroFindPrimaryTarget();

		if (!primary)
		{
			// No perceived target: start / continue the lost grace; reset the zero once it expires.
			if (m_fDCO_ZeroLostSince < 0)
				m_fDCO_ZeroLostSince = now;
			if ((now - m_fDCO_ZeroLostSince) >= cfg.m_fZeroResetSec * 1000.0)
			{
				m_eDCO_ZeroTarget = null;
				m_fDCO_ZeroDwellStart = -1;
				DCO_ZeroApplyState(DCO_ZERO_DEFAULT, healthy, cfg);	// idle: restore default skill (if healthy)
			}
			return;
		}

		m_fDCO_ZeroLostSince = -1;	// we hold a target again

		if (primary != m_eDCO_ZeroTarget)
		{
			// Target switch: the zero starts over cold on the new target.
			m_eDCO_ZeroTarget = primary;
			m_fDCO_ZeroDwellStart = now;
		}

		// Dwell-based warm-up stage.
		float dwellSec = 0;
		if (m_fDCO_ZeroDwellStart >= 0)
			dwellSec = (now - m_fDCO_ZeroDwellStart) / 1000.0;

		int state = DCO_ZERO_COLD;
		if (dwellSec >= cfg.m_fZeroStep2Sec)
			state = DCO_ZERO_ZEROED;
		else if (dwellSec >= cfg.m_fZeroStep1Sec)
			state = DCO_ZERO_WARM;

		DCO_ZeroApplyState(state, healthy, cfg);
	}

	// Nearest perceived enemy to the leader = the group's primary target (the one being
	// zeroed). Null when the group perceives nothing.
	protected IEntity DCO_ZeroFindPrimaryTarget()
	{
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return null;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return null;
		vector fromPos = leader.GetOrigin();

		IEntity best;
		float bestSq = 1000000000.0;	// ~31 km^2 cap; perceived targets are well within this
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float d = vector.DistanceSq(t.GetOrigin(), fromPos);
			if (d < bestSq)
			{
				bestSq = d;
				best = t;
			}
		}
		return best;
	}

	// Apply an aim state to every member, only when the state (or the morale-healthy flag)
	// changes. Skill is written only while morale is healthy (else morale owns the lever):
	// COLD to ROOKIE, WARM to REGULAR, DEFAULT/ZEROED to ResetAISkill (the trained default, never
	// above it). Perception (if enabled) ramps with the state but is forced to base while
	// not healthy.
	protected void DCO_ZeroApplyState(int state, bool healthy, DCO_MoraleSettings cfg)
	{
		if (state == m_iDCO_LastZeroState && healthy == m_bDCO_LastZeroHealthy)
			return;
		m_iDCO_LastZeroState = state;
		m_bDCO_LastZeroHealthy = healthy;

		// Perception factor for this state (only boosted while healthy; detection lever, may exceed 1.0).
		float perc = cfg.m_fZeroBasePerception;
		if (cfg.m_bZeroBoostPerception && healthy)
		{
			if (state == DCO_ZERO_ZEROED)
				perc = cfg.m_fZeroMaxPerception;
			else if (state == DCO_ZERO_WARM)
				perc = cfg.m_fZeroBasePerception + (cfg.m_fZeroMaxPerception - cfg.m_fZeroBasePerception) * 0.5;
		}

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity ent = agent.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never touch a player's skill/perception

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			if (healthy)
			{
				if (state == DCO_ZERO_COLD)
					combat.SetAISkill(EAISkill.ROOKIE);
				else if (state == DCO_ZERO_WARM)
					combat.SetAISkill(EAISkill.REGULAR);
				else
					combat.ResetAISkill();	// DEFAULT or ZEROED: the unit's trained default skill
			}

			if (cfg.m_bZeroBoostPerception)
				combat.SetPerceptionFactor(perc);
		}

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("ZERO", m_Owner.GetLeaderEntity(), string.Format("aim state=%1 (healthy=%2 perc=%3)", state, healthy, perc));
	}
}
