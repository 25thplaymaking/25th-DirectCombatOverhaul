// Base-settings applier. modded SCR_AIGroupUtilityComponent fragment (same seam as
// DCO_VisionLimit), throttled per group, that pushes the global base-settings
// baseline onto each AI member - never onto a player-controlled one.
//
// Ownership split: when morale (DCO_GroupMorale) is enabled it owns skill +
// fire-rate, so the applier only pushes perception + formation + stance; when
// morale is off, the applier also pushes skill + fire-rate. Perception is deferred
// while the group is engaged, handing off to vision/zeroing.
//
// Formation: AIFormationComponent lives on the group entity, and SetFormation takes
// the SCR_EAIGroupFormation enum *name* via SCR_Enum.GetEnumName. m_eDefaultSpawnFormation
// is the enum ordinal. (Verified against SCR_ScenarioFrameworkAIActionSetFormation.)
modded class SCR_AIGroupUtilityComponent
{
	protected float m_fDCO_LastBaseTime         = -1;
	protected int   m_iDCO_BaseFormationApplied = -1;
	protected bool  m_bDCO_ReactWasInContact    = false;   // reaction-time edge state: was this group in contact last check

	// Reaction time (shared-tick entry, called from DCO_GroupQRF.EvaluateActivity).
	// The moment a group first acquires a target (the no-contact -> contact edge),
	// each AI member gets a brief weapon-hold: the delay before it opens fire on the
	// new threat. Higher "Reaction Speed" means a shorter delay (score 0 ~2.0s,
	// 100 ~0.2s). One-shot per acquisition - not re-applied while in contact, or the
	// AI would never fire. AI-only; no-op unless base settings is enabled.
	void DCO_UpdateReaction()
	{
		if (!Replication.IsServer())
			return;

		DCO_BaseSettings bs = DCO_BaseSettings.Get();
		if (!bs.m_bEnableBaseSettings || !m_Owner || !m_Perception)
		{
			m_bDCO_ReactWasInContact = false;	// reset edge so re-enabling re-arms cleanly
			return;
		}

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		bool inContact = targets && !targets.IsEmpty();

		// Rising edge only: just acquired contact this check.
		if (inContact && !m_bDCO_ReactWasInContact)
		{
			float delay = DCO_BaseSettingsUtil.ReactionDelayFromScore(bs.m_iReactionTime);
			if (delay > 0)
			{
				array<AIAgent> agents = {};
				m_Owner.GetAgents(agents);
				foreach (AIAgent a : agents)
				{
					if (!a)
						continue;
					IEntity ent = a.GetControlledEntity();
					if (!ent)
						continue;
					if (DCO_BaseSettingsApplierUtil.IsPlayerControlled(ent))
						continue;	// AI only

					SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
					if (cc)
						cc.SetWeaponNoFireTime(delay);
				}

				if (bs.m_bDebugBaseSettings)
					DCO_Debug.LogGroup("BASE", m_Owner.GetLeaderEntity(), string.Format("reaction hold %1s on contact", delay));
			}
		}

		m_bDCO_ReactWasInContact = inContact;
	}

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Pushes the global
	// base-settings baseline onto each member.
	void DCO_UpdateBaseSettings()
	{
		if (!Replication.IsServer())
			return;

		DCO_BaseSettings cfg = DCO_BaseSettings.Get();
		if (!cfg.m_bEnableBaseSettings || !m_Owner)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastBaseTime >= 0 && (now - m_fDCO_LastBaseTime) < cfg.m_fApplyIntervalSec * 1000.0)
			return;
		m_fDCO_LastBaseTime = now;

		bool moraleOwnsSkill = DCO_MoraleSettings.Get().m_bEnabled;
		bool engaged = m_Perception && m_Perception.m_aTargetEntities && !m_Perception.m_aTargetEntities.IsEmpty();

		// Group formation. AIFormationComponent is on the group entity; SetFormation
		// takes the SCR_EAIGroupFormation enum name. Only re-push on change; an
		// out-of-range ordinal yields an empty name and is skipped (fails safe).
		if (cfg.m_eDefaultSpawnFormation != m_iDCO_BaseFormationApplied)
		{
			// Same path as DCO_FormationComponent.DCO_UpdateFormation. Formation members:
			// {Wedge=0, Line=1, Column=2, StaggeredColumn=3}.
			IEntity groupEnt = GetOwner();
			AIFormationComponent formComp;
			if (groupEnt)
				formComp = AIFormationComponent.Cast(groupEnt.FindComponent(AIFormationComponent));
			if (formComp)
			{
				string formName = SCR_Enum.GetEnumName(SCR_EAIGroupFormation, cfg.m_eDefaultSpawnFormation);
				if (formName != string.Empty)
				{
					formComp.SetFormation(formName);
					m_iDCO_BaseFormationApplied = cfg.m_eDefaultSpawnFormation;
				}
			}
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

			// AI only: never touch a player-controlled character.
			if (DCO_BaseSettingsApplierUtil.IsPlayerControlled(ent))
				continue;

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (combat)
			{
				if (!moraleOwnsSkill)
				{
					combat.SetAISkill(DCO_BaseSettingsUtil.BaselineSkill());
					combat.SetFireRateCoef(DCO_BaseSettingsUtil.BaselineFireRateCoef(), false);
				}
				if (!engaged)
					combat.SetPerceptionFactor(DCO_BaseSettingsUtil.BaselinePerception());
			}

			if (cfg.m_eGlobalStance != DCO_EBaseStance.NONE)
				DCO_BaseSettingsApplierUtil.ForceStance(ent, cfg.m_eGlobalStance);
		}

		if (cfg.m_bDebugBaseSettings)
			DCO_Debug.LogGroup("BASE", m_Owner.GetLeaderEntity(),
				string.Format("skill=%1 perc=%2 form=%3 stance=%4 moraleOwnsSkill=%5",
					cfg.m_iAiSkill, cfg.m_iPerception, cfg.m_eDefaultSpawnFormation, cfg.m_eGlobalStance, moraleOwnsSkill));
	}
}

// Helper holder: AI-only check + stance bridge in one place.
class DCO_BaseSettingsApplierUtil
{
	static bool IsPlayerControlled(IEntity ent)
	{
		PlayerManager pm = GetGame().GetPlayerManager();
		if (!pm)
			return false;
		return pm.GetPlayerIdFromControlledEntity(ent) != 0;
	}

	static void ForceStance(IEntity ent, DCO_EBaseStance stance)
	{
		ECharacterStance s;
		switch (stance)
		{
			case DCO_EBaseStance.STAND:  s = ECharacterStance.STAND;  break;
			case DCO_EBaseStance.CROUCH: s = ECharacterStance.CROUCH; break;
			case DCO_EBaseStance.PRONE:  s = ECharacterStance.PRONE;  break;
			default: return;
		}
		DCO_StanceUtil.TrySetStance(ent, s, 1500);
	}
}
