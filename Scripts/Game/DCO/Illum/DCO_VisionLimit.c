// Vision / night limiting. Slower detection at night via the real detection lever
// (SetPerceptionFactor): a group's members get a reduced perception factor that scales
// with how dark it is (a time-of-day twilight ramp), so AI take longer to spot the enemy
// in the dark. The moment a group is engaged (already perceives a target) the penalty is
// lifted - muzzle flashes give the contact away, and we hand perception back to the
// combat/zeroing layer. "Harder to find, but once you're shooting you're seen" reads more
// naturally than a flat night nerf, and it dovetails with DCO_MarksmanZeroing.
//
// Time of day via ChimeraWorld.GetTimeAndWeatherManager().GetTimeOfTheDay() (hours 0..24);
// darkness is a configurable twilight ramp between sunrise/sunset so dawn/dusk are smooth.
//
// Turn-off safety: a perception penalty is a persistent value on the AI, so simply ceasing
// to tick would leave dimmed members half-blind forever. When the feature is off we restore
// full perception once (DCO_VisionResetIfPenalized) and then idle.
//
// modded SCR_AIGroupUtilityComponent fragment in Illum/ (sorts before the QRF tick that
// calls DCO_UpdateVisionLimit). Server-only, throttled, applies the factor only when it
// changes (minimal contention with zeroing). Default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastVisionTime		= -1;	// world time (ms) vision limiting was last evaluated
	protected float	m_fDCO_VisionApplied		= -1;	// last perception factor we pushed onto the members (-1 = none)

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Scales member
	// perception by darkness while searching; restores full perception once in contact.
	void DCO_UpdateVisionLimit()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVisionLimit)
		{
			// Feature turned OFF: undo any lingering night penalty so the AI don't stay dimmed.
			DCO_VisionResetIfPenalized();
			return;
		}
		if (!m_Owner || !m_Perception)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastVisionTime >= 0 && (now - m_fDCO_LastVisionTime) < cfg.m_fVisionCheckSec * 1000.0)
			return;
		m_fDCO_LastVisionTime = now;

		float factor;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		bool engaged = targets && !targets.IsEmpty();
		if (engaged)
		{
			// In contact: full perception (hand off to the combat/zeroing layer).
			factor = 1.0;
		}
		else
		{
			float tod = DCO_GetTimeOfDay();
			if (tod < 0)
				return;	// no time manager - leave perception alone
			float darkness = DCO_NightDarkness(tod, cfg.m_fVisionSunriseHour, cfg.m_fVisionSunsetHour, cfg.m_fVisionTwilightHours);
			factor = 1.0 + (cfg.m_fNightPerceptionFactor - 1.0) * darkness;
		}

		// Only re-push when the factor meaningfully changed.
		if (m_fDCO_VisionApplied >= 0 && Math.AbsFloat(factor - m_fDCO_VisionApplied) < 0.01)
			return;
		m_fDCO_VisionApplied = factor;

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
				continue;	// never touch a player's perception
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (combat)
				combat.SetPerceptionFactor(factor);
		}

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("VISION", m_Owner.GetLeaderEntity(), string.Format("perception=%1 (engaged=%2)", factor, engaged));
	}

	// Restore full perception once when vision limiting is turned off, so members dimmed by
	// the night penalty don't stay half-blind. No-op if nothing (or only full) was applied.
	protected void DCO_VisionResetIfPenalized()
	{
		if (!m_Owner || m_fDCO_VisionApplied < 0 || m_fDCO_VisionApplied >= 0.999)
			return;	// nothing applied, or the last value we pushed was already full perception

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
				continue;	// never touch a player's perception
			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (combat)
				combat.SetPerceptionFactor(1.0);
		}
		m_fDCO_VisionApplied = -1;
	}

	// Current time of day in hours [0..24), or -1 if the time/weather manager is absent.
	protected float DCO_GetTimeOfDay()
	{
		ChimeraWorld cw = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!cw)
			return -1;
		TimeAndWeatherManagerEntity twm = cw.GetTimeAndWeatherManager();
		if (!twm)
			return -1;
		return twm.GetTimeOfTheDay();
	}

	// 0 = full daylight, 1 = full dark, with a linear twilight ramp of width tw (hours) on
	// each side of sunrise/sunset. Deep night before sunrise reads as full dark.
	protected float DCO_NightDarkness(float tod, float sunrise, float sunset, float tw)
	{
		if (tw <= 0)
			tw = 0.5;

		// Full daylight.
		if (tod >= sunrise && tod <= sunset)
			return 0;

		// Dusk ramp (just after sunset).
		if (tod > sunset)
		{
			if (tod < sunset + tw)
				return (tod - sunset) / tw;
			return 1;
		}

		// Early morning (tod < sunrise): dawn ramp just before sunrise, else full dark.
		if (tod > sunrise - tw)
			return (sunrise - tod) / tw;
		return 1;
	}
}
