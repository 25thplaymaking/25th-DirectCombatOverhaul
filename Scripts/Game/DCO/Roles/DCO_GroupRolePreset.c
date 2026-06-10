// Per-group role preset (spawn-time, no Game Master required).
// QRF / Ambush / Defend / Reinforcement-eligibility are per-group flags that normally a Game Master
// toggles on a selected group at runtime. On a 24/7 public server there is often no GM around to do
// that. This ScriptComponent bakes a role into a group prefab so every group spawned from it comes up
// already configured - SF / armored prefabs as QRF, line infantry as reinforcements, a fixed garrison
// as Defend, etc.
//
// How to use (in the Workbench):
//   Open the AI group prefab you spawn (the entity that carries SCR_AIGroup / SCR_AIGroupUtilityComponent,
//   e.g. a variant of an ambient patrol group, or the group an IPC/Scenario-Framework spawner uses).
//   Add component DCO_GroupRolePreset. Pick the Role and (for QRF/Ambush) the range.
//   Make a prefab variant per role and point your spawners at the right variant. No GM, no per-session setup.
//
// It just calls the same accessors the GM attributes use (DCO_SetQRFResponder/Range, DCO_SetAmbusher/Range,
// DCO_SetDefender, DCO_SetReinforcementEligible) on the group's SCR_AIGroupUtilityComponent, so behaviour is
// identical to a GM having flipped the toggle. Server-authoritative (DCO logic is all server-side); on a
// client the call is a harmless no-op. Applied one frame after init so the group utility component exists.
//
// REINFORCEMENT vs the deconfliction guard: a group with role REINFORCEMENT is flagged
// "reinforcement-eligible", which means DCO may pull it off its own waypoint to converge on a contact
// (it overrides m_bReinforcementRespectWaypoints). Leave the role NONE if you want a patrol to finish its
// waypoint first and only reinforce when genuinely idle - that is the default deconflicted behaviour.
enum EDCO_GroupRole
{
	NONE,			// no preset (default DCO behaviour; waypoints respected)
	QRF,			// Quick Reaction Force responder (moves to support distressed friendlies; overrides its waypoint)
	AMBUSH,			// holds fire until an enemy enters ambush range, then springs
	DEFEND,			// holds position and orients its defence toward the threat
	REINFORCEMENT,	// reinforcement-eligible: DCO may override its waypoint to converge on a contact
	CLEAR,			// CQB Clearer: methodically clears buildings as it moves
}

class DCO_GroupRolePresetClass : ScriptComponentClass
{
}

class DCO_GroupRolePreset : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Role baked into this group at spawn (no Game Master needed).", "", ParamEnumArray.FromEnum(EDCO_GroupRole), category: "25th DCO")]
	EDCO_GroupRole m_eRole;

	[Attribute("1500", UIWidgets.Slider, "QRF response range (m) - only used when Role = QRF.", "100 5000 50", category: "25th DCO")]
	float m_fQRFRange;

	[Attribute("50", UIWidgets.Slider, "Ambush trigger range (m) - only used when Role = AMBUSH.", "5 300 5", category: "25th DCO")]
	float m_fAmbushRange;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (m_eRole == EDCO_GroupRole.NONE)
			return;

		// Only the authoritative server runs DCO behaviour; skip in the World Editor (not in play mode).
		if (!GetGame() || !GetGame().InPlayMode())
			return;

		// Defer one frame: the SCR_AIGroupUtilityComponent (and its DCO flags) are reliably present by then.
		GetGame().GetCallqueue().CallLater(DCO_ApplyRole, 0, false);
	}

	protected void DCO_ApplyRole()
	{
		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_AIGroup grp = SCR_AIGroup.Cast(owner);
		if (!grp)
			return;

		SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
		if (!util)
		{
			// Group utility not up yet (rare) - try once more shortly, then give up.
			GetGame().GetCallqueue().CallLater(DCO_ApplyRoleRetry, 500, false);
			return;
		}

		DCO_ApplyTo(util);
	}

	protected void DCO_ApplyRoleRetry()
	{
		if (!Replication.IsServer())
			return;

		IEntity owner = GetOwner();
		if (!owner)
			return;

		SCR_AIGroup grp = SCR_AIGroup.Cast(owner);
		if (!grp)
			return;

		SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
		if (util)
			DCO_ApplyTo(util);
	}

	protected void DCO_ApplyTo(SCR_AIGroupUtilityComponent util)
	{
		switch (m_eRole)
		{
			case EDCO_GroupRole.QRF:
			{
				util.DCO_SetQRFResponder(true);
				util.DCO_SetQRFRange(m_fQRFRange);
				break;
			}
			case EDCO_GroupRole.AMBUSH:
			{
				util.DCO_SetAmbusher(true);
				util.DCO_SetAmbushRange(m_fAmbushRange);
				break;
			}
			case EDCO_GroupRole.DEFEND:
			{
				util.DCO_SetDefender(true);
				break;
			}
			case EDCO_GroupRole.REINFORCEMENT:
			{
				util.DCO_SetReinforcementEligible(true);
				break;
			}
			case EDCO_GroupRole.CLEAR:
			{
				util.DCO_SetCqbClearer(true);
				break;
			}
		}
	}
}
