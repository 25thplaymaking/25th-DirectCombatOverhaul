// QRF (Quick Reaction Force) - per-group responder. A group flagged as a QRF responder (via the
// per-group "25th DCO QRF" GM attribute on the selected group) watches for same-faction friendly
// groups in distress within its QRF range and moves to support them. Distress = in contact with an
// enemy, or broken/fleeing from low morale.
// Replaces the old placed-zone/mesh approach: the GM now selects a group and toggles it as a
// responder - no placeable, no mesh, no content-browser registry.
// Third `modded SCR_AIGroupUtilityComponent` (merges with morale + reinforcement); the shared
// EvaluateActivity tick (DCO_GroupMorale.c) calls DCO_UpdateQRF(). Server-authoritative. Engine API only.
modded class SCR_AIGroupUtilityComponent
{
	protected bool	m_bDCO_IsQRFResponder	= false;
	protected float	m_fDCO_QRFRange			= 1500.0;
	protected float	m_fDCO_LastQRFTime		= -1;
	protected bool	m_bDCO_QRFDeployed		= false;

	// Optional hold / rally point (set by a QRF task zone). When set, this is where the QRF waits and can
	// return to once the contact it responded to is over (full return behaviour comes in the QRF pass).
	protected vector	m_vDCO_QRFHoldPos		= vector.Zero;
	protected bool		m_bDCO_HasQRFHold		= false;

	protected static const float DCO_QRF_CHECK_INTERVAL_MS = 5000.0;

	// Shared DCO tick. Defined here (the alphabetically-last SCR_AIGroupUtilityComponent fragment:
	// Comms, Morale, QRF) so the morale, reinforcement and QRF update methods are all already
	// defined when this override compiles - Enforce resolves cross-fragment calls in file order.
	override SCR_AIActionBase EvaluateActivity(out bool restartActivity)
	{
		SCR_AIActionBase action = super.EvaluateActivity(restartActivity);
		DCO_UpdateMorale();			// morale / break / surrender / panic / accuracy / shared-pool (DCO_GroupMorale.c)
		DCO_UpdateAimZeroing();		// progressive aim zeroing on a held target, morale-gated (DCO_MarksmanZeroing.c)
		DCO_UpdateMemberMorale();	// per-soldier morale variance: shaken members hesitate (DCO_MemberMorale.c)
		DCO_UpdateEmergencyRearm();	// out-of-ammo members resupply / scavenge a weapon (DCO_EmergencyRearm.c)
		DCO_UpdateSuppression();	// real suppression: pinned members take cover + smoke + heads down (DCO_Suppression.c)
		DCO_UpdateArmorAvoid();		// hide-from-armour flee (DCO_GroupMorale.c)
		DCO_UpdateFriendlyFire();	// fratricide LOS hold (DCO_FriendlyFire.c)
		DCO_UpdateMerge();			// straggler / one-man-team merge (DCO_StragglerMerge.c)
		DCO_UpdateReinforcement();	// shared situational awareness (DCO_GroupReinforcement.c)
		DCO_UpdateAmbush();			// per-group hold-fire ambush (DCO_AmbushComponent.c)
		DCO_UpdateDefend();			// per-group defensive hold / orient (DCO_DefendComponent.c)
		DCO_UpdateTacticalBrain();	// choose course of action from combat power + morale (DCO_TacticalBrain.c)
		DCO_UpdateReactToContact();	// execute the COA: suppress / assault-flank / break contact (DCO_ReactToContact.c)
		DCO_UpdateFormation();		// adaptive group formation (DCO_FormationComponent.c)
		DCO_UpdateCoverPlacement();	// place exposed members in cover while in contact (DCO_FormationComponent.c)
		DCO_UpdateFormationShape();	// DCO echelon/vee/dispersion per-member shaping (DCO_FormationComponent.c)
		DCO_UpdateTacticalPath();	// procedural cover-routing as the group moves (DCO_TacticalPath.c)
		DCO_UpdateVehicleArmor();	// AI vehicle armour angling (DCO_VehicleArmor.c)
		DCO_UpdateVehicleCombat();	// vehicle LOS firing-position seeking + hull-down back-off (DCO_VehicleCombat.c)
		DCO_UpdateVehicleDoctrine();	// convoy/section vehicle doctrine executor (DCO_VehicleDoctrine.c)
		DCO_UpdateVehicleHijack();	// commandeer nearby empty vehicles (DCO_VehicleHijack.c)
		DCO_UpdateAssetUse();		// man a nearby static weapon/turret, then release it (DCO_AssetUse.c)
		DCO_UpdateHVT();			// high-value (vehicle) priority targeting (DCO_HVTTargeting.c)
		DCO_UpdateNightIllum();		// night illumination flares (DCO_NightIllum.c)
		DCO_UpdateVisionLimit();	// reduced detection in darkness while searching (DCO_VisionLimit.c)
		DCO_UpdateBaseSettings();	// global AI base-settings baseline (DCO_BaseSettingsApplier.c)
		DCO_UpdateReaction();		// reaction-time fire delay on first contact (DCO_BaseSettingsApplier.c)
		DCO_UpdateIdle();			// spread + local patrol when taskless and stationary (DCO_GroupIdle.c)
		DCO_UpdateCoverStance();	// fit member stance to cover geometry while in contact (DCO_CoverStance.c)
		DCO_UpdateCQB();			// push into a building to clear an enemy sheltering inside (DCO_CQB.c)
		DCO_UpdateCqbClear();	// methodical building-clearing + town sweep (DCO/CQB/DCO_CqbClear.c)
		DCO_UpdateGarrison();		// defender splits across a building's perimeter to hold it (DCO_Garrison.c)
		DCO_UpdateMelee();			// strike enemies in grappling range with melee (DCO_Melee.c)
		DCO_UpdateMachineGunner();	// MG carriers deploy prone on a firing position to suppress (DCO_MachineGunner.c)
		DCO_UpdateArtillery();		// AI mortar crew calls fire missions on enemy clusters (DCO_Artillery.c)
		DCO_Commander.EnsureRunning();	// lazily start the global AI-commander coordinator (DCO_Commander.c)
		DCO_ExternalCommander.EnsureRunning();	// lazily start the external (LLM/RECOM) commander bridge (External/DCO_ExternalCommander.c)
		DCO_VehicleSectionController.EnsureRunning();	// lazily start the vehicle section controller (DCO_VehicleSectionController.c)
		DCO_UpdateVocalShare();		// shout contacts to nearby friendly groups (DCO_VocalShare.c)
		DCO_UpdateRadioShare();		// radio contacts to distant friendly groups (DCO_RadioShare.c)
		DCO_UpdateComposition();		// reposition onto / defend own-faction compositions nearby (DCO_GroupComposition.c)
		DCO_UpdateQRF();			// per-group quick reaction force (this file)
		return action;
	}

	// Game Master attribute accessors (DCO_QRFEditorAttributes.c).
	bool DCO_IsQRFResponder()
	{
		return m_bDCO_IsQRFResponder;
	}

	void DCO_SetQRFResponder(bool enable)
	{
		m_bDCO_IsQRFResponder = enable;
		if (!enable)
			m_bDCO_QRFDeployed = false;
	}

	float DCO_GetQRFRange()
	{
		return m_fDCO_QRFRange;
	}

	// Hold/rally point accessors (set by a DCO_TaskZone with role QRF).
	void DCO_SetQRFHoldPosition(vector pos)
	{
		m_vDCO_QRFHoldPos = pos;
		m_bDCO_HasQRFHold = true;
	}

	void DCO_ClearQRFHoldPosition()
	{
		m_bDCO_HasQRFHold = false;
	}

	bool DCO_HasQRFHoldPosition()
	{
		return m_bDCO_HasQRFHold;
	}

	vector DCO_GetQRFHoldPosition()
	{
		return m_vDCO_QRFHoldPos;
	}

	void DCO_SetQRFRange(float range)
	{
		m_fDCO_QRFRange = range;
	}

	// True when this group is in a critical situation worth a QRF response: it is broken/routing (morale +
	// numbers have collapsed), or it is in contact and its morale has fallen to/below the critical line. The
	// morale gate means a QRF holds at its meeting point during routine contact and only commits when a
	// friendly is actually in trouble (move when things are critical).
	bool DCO_NeedsQRFSupport()
	{
		if (m_bDCO_Broken)
			return true;

		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			if (targets && !targets.IsEmpty() && m_fDCO_Morale <= DCO_MoraleSettings.Get().m_fQRFCriticalMorale)
				return true;
		}
		return false;
	}

	void DCO_UpdateQRF()
	{
		if (!Replication.IsServer())
			return;

		if (!m_bDCO_IsQRFResponder || !m_Owner)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastQRFTime >= 0 && (now - m_fDCO_LastQRFTime) < DCO_QRF_CHECK_INTERVAL_MS)
			return;
		m_fDCO_LastQRFTime = now;

		IEntity selfLeader = m_Owner.GetLeaderEntity();
		if (!selfLeader)
			return;

		vector selfPos = selfLeader.GetOrigin();
		Faction selfFaction = m_Owner.GetFaction();
		float rangeSq = m_fDCO_QRFRange * m_fDCO_QRFRange;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		IEntity supportTarget;
		float bestDistSq = rangeSq;
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || grp == m_Owner || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			if (grp.GetFaction() != selfFaction)
				continue;

			IEntity grpLeader = grp.GetLeaderEntity();
			if (!grpLeader)
				continue;

			float dSq = vector.DistanceSq(grpLeader.GetOrigin(), selfPos);
			if (dSq > bestDistSq)
				continue;

			SCR_AIGroupUtilityComponent grpUtil = grp.GetGroupUtilityComponent();
			if (!grpUtil || !grpUtil.DCO_NeedsQRFSupport())
				continue;

			bestDistSq = dSq;
			supportTarget = DCO_PickQRFMoveTarget(grpUtil, grpLeader);
		}

		if (!supportTarget)
		{
			m_bDCO_QRFDeployed = false;	// nobody needs help - re-arm

			// Garrison behaviour: if this QRF has a hold/rally point (set by a QRF Task Zone) and has drifted
			// from it - e.g. it just finished a response, or wandered - walk it back so it waits at the meeting
			// point until the next call. Leash-gated so it isn't re-ordered every check once it's home.
			if (m_bDCO_HasQRFHold)
			{
				float leash = DCO_MoraleSettings.Get().m_fQRFHoldLeash;
				if (vector.DistanceSq(selfPos, m_vDCO_QRFHoldPos) > leash * leash)
				{
					AICommunicationComponent rcomms = m_Mailbox;
					if (rcomms)
						DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, m_vDCO_QRFHoldPos, rcomms);
				}
			}
			return;
		}

		if (m_bDCO_QRFDeployed)
			return;						// already responding to the current call

		AIAgent selfLeaderAgent = m_Owner.GetLeaderAgent();
		AICommunicationComponent comms = m_Mailbox;
		if (!selfLeaderAgent || !comms)
			return;

		// QRF intentionally overrides the responder's patrol/waypoint to go support. When the override-clear
		// option is on, drop its external waypoint so the support move holds instead of fighting the waypoint
		// (default OFF - clearing a framework/IPC waypoint can desync it; DCO_ClearExternalWaypoints is defined
		// in DCO_GroupReinforcement.c, which sorts before this file).
		if (DCO_MoraleSettings.Get().m_bClearWaypointOnOverride)
			DCO_ClearExternalWaypoints(m_Owner);

		// Vehicle-aware: a mounted QRF responder drives to support instead of dismounting to walk
		// (random-disembark fix). Foot QRFs keep the original on-foot order.
		DCO_VehicleUtil.OrderGroupMoveToEntity(m_Owner, supportTarget, comms);
		m_bDCO_QRFDeployed = true;
	}

	// Prefer moving onto the distressed group's enemy; fall back to the friendly group's position.
	protected IEntity DCO_PickQRFMoveTarget(SCR_AIGroupUtilityComponent grpUtil, IEntity grpLeader)
	{
		if (grpUtil.m_Perception)
		{
			array<IEntity> tgts = grpUtil.m_Perception.m_aTargetEntities;
			if (tgts)
			{
				foreach (IEntity t : tgts)
				{
					if (t)
						return t;
				}
			}
		}
		return grpLeader;
	}
}
