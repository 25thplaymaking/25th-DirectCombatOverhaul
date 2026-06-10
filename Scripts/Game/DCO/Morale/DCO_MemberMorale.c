// Per-member morale modifier. Our morale model is group-level by design (a squad breaks,
// flees, or surrenders as one). This adds a thin per-member modifier on top: each soldier
// has a personal morale = the group baseline minus their own current stress (how suppressed
// they are right now). The most personally-shaken members hesitate to fire and keep their
// heads down a beat longer, so within one squad the pinned man cowers while his less
// pressured buddy keeps shooting - individual variance without throwing away cohesion.
//
// Non-contending by design: it only re-applies a short weapon-hold (SetWeaponNoFireTime) to
// currently shaken members. It never writes a 0 hold (which would cancel an ambush's
// hold-fire on a steady member) and never touches fire-rate or stance (owned by suppression
// / react-to-contact / cover-stance). When a member recovers we stop re-applying and the
// hold decays on its own. Reads group morale (m_fDCO_Morale) and the engine per-AI
// suppression measure.
//
// modded SCR_AIGroupUtilityComponent fragment in Morale/. Sorts after DCO_GroupMorale.c so
// m_fDCO_Morale is declared, and before the QRF tick that calls DCO_UpdateMemberMorale().
// On-foot, server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastMemberMoraleTime	= -1;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity, after DCO_UpdateMorale so
	// m_fDCO_Morale is current). Makes the most personally-shaken members hesitate / hide.
	void DCO_UpdateMemberMorale()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableMemberMorale || !m_Owner)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastMemberMoraleTime >= 0 && (now - m_fDCO_LastMemberMoraleTime) < cfg.m_fMemberMoraleCheckSec * 1000.0)
			return;
		m_fDCO_LastMemberMoraleTime = now;

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
				continue;	// never hold a player's fire

			SCR_ChimeraAIAgent chim = SCR_ChimeraAIAgent.Cast(a);
			if (!chim || !chim.m_UtilityComponent || !chim.m_UtilityComponent.m_ThreatSystem)
				continue;

			float supp = chim.m_UtilityComponent.m_ThreatSystem.GetSuppressionMeasure();
			// Personal morale = group baseline minus this soldier's own current stress.
			float personal = m_fDCO_Morale - supp * cfg.m_fMemberMoraleSuppressionWeight;

			if (personal > cfg.m_fMemberMoraleHesitateThreshold)
				continue;	// steady enough - leave to the normal combat AI

			SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
			if (!cc)
				continue;

			// Shaken: hesitate to fire / keep head down. Re-applied while shaken; decays on recovery (we never
			// write 0, so we don't cancel an ambush hold on a steady member).
			cc.SetWeaponNoFireTime(cfg.m_fMemberMoraleHesitateSec);

			// The very worst-shaken additionally drop prone to hide (optional - shares the ForceStance lever
			// with cover-stance / cower, but here everyone agrees the man should be down).
			if (cfg.m_bMemberMoraleCower && personal <= cfg.m_fMemberMoraleCowerThreshold)
				DCO_StanceUtil.TrySetStance(ent, ECharacterStance.PRONE, cfg.m_fStanceCooldownSec * 1000.0);

			if (cfg.m_bDebug)
				DCO_Debug.LogGroup("MBRMORALE", ent, string.Format("shaken: personal=%1 (group=%2 supp=%3)", personal, m_fDCO_Morale, supp));
		}
	}
}
