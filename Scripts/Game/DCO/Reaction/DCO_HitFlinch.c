// Hit-reaction flinch. When an AI character takes a hit, it briefly holds fire - a short flinch so
// being shot at disrupts the AI's return fire (VCOM AI's "disorientation when shot", done as a fire
// interruption instead of physics). Makes firefights feel reactive.
//
// Hooked by overriding SCR_CharacterDamageManagerComponent.OnDamage(notnull BaseDamageContext), the
// engine's per-character damage entry point (an overridable protected method invoked on every applied
// hit, carrying a single BaseDamageContext). We call super first, then apply the flinch.
//
// AI only: players have an AIControlComponent that is not activated, so IsAIActivated() gates this to
// AI characters. Healing/regeneration damage is ignored. A short cooldown dedupes damage-over-time
// ticks (fire/bleeding) so they don't permanently pin the AI. Server-authoritative. Gated on
// DCO_MoraleSettings.m_bEnableHitFlinch (default OFF). Reuses the SetWeaponNoFireTime lever.
modded class SCR_CharacterDamageManagerComponent
{
	protected float m_fDCO_LastFlinchTime = -1;

	override protected void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);

		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		// Ignore healing/regeneration "damage" - it is neither a flinch nor combat activity.
		if (damageContext.damageType == EDamageType.HEALING || damageContext.damageType == EDamageType.REGENERATION)
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;
		if (DCO_PlayerUtil.IsPlayer(owner))
			return;	// the damage hook fires on players too - never flinch a player

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(owner.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return;

		// AI only - a player's AI control component is present but not activated.
		AIControlComponent ai = cc.GetAIControlComponent();
		if (!ai || !ai.IsAIActivated())
			return;

		// Surrender lull: being shot is combat activity, so refresh the victim's group "last combat
		// activity" stamp so it cannot surrender mid-firefight. Done independently of the hit-flinch toggle
		// (it's part of the morale gate, not the flinch). Path: AIControlComponent.GetControlAIAgent() then
		// AIAgent.GetParentGroup() then SCR_AIGroup.GetGroupUtilityComponent() then DCO_NoteCombatActivity().
		AIAgent selfAgent = ai.GetControlAIAgent();
		if (selfAgent)
		{
			SCR_AIGroup scrGrp = SCR_AIGroup.Cast(selfAgent.GetParentGroup());
			if (scrGrp)
			{
				SCR_AIGroupUtilityComponent gu = scrGrp.GetGroupUtilityComponent();
				if (gu)
					gu.DCO_NoteCombatActivity();
			}
		}

		if (!cfg.m_bEnableHitFlinch)
			return;	// combat-note above always runs; the flinch below is its own opt-in feature

		// Throttle so damage-over-time ticks don't continuously re-trigger the flinch.
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastFlinchTime >= 0 && (now - m_fDCO_LastFlinchTime) < cfg.m_fHitFlinchCooldownSec * 1000.0)
			return;
		m_fDCO_LastFlinchTime = now;

		// The flinch: a brief held-fire that disrupts the AI's return fire.
		cc.SetWeaponNoFireTime(cfg.m_fHitFlinchDuration);
	}
}
