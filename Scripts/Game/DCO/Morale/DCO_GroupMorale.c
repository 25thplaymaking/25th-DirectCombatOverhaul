// Group morale on the AI group brain. Morale drains under heavy threat (suppression /
// shots / injury via the engine threat measure) scaled by casualties (lost strength), and
// recovers when safe.
//   - Below the flee threshold the group breaks and is sent a native SCR_AIMessage_Flee.
//   - Below the surrender threshold the group performs an immersive surrender, per member:
//     clear faction, crouch, drop weapon + gear (keep jacket/pants/boots), go prone.
//
// Tunables come from the DCO_MoraleSettings singleton (present in every GM mode, no scenario
// setup; tuned live via the "25th DCO" Game Master attributes or an optional
// DCO_MoraleSettingsComponent). Engine API only; stacks on CRX via super. Server-side.

// Per-member snapshot taken at genuine surrender so the unit can be recovered / re-armed later.
class DCO_SurrenderRecord
{
	IEntity			m_Character;
	Faction			m_OriginalFaction;
	IEntity			m_WeaponEntity;		// the dropped primary weapon (may become invalid if cleaned up)
	ResourceName	m_WeaponResource;	// captured prefab, for the spawn fallback when the entity is gone
}

modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_Morale			= 100.0;
	protected int	m_iDCO_MaxStrength		= 0;
	protected float	m_fDCO_LastUpdateTime	= -1;
	protected bool	m_bDCO_Broken			= false;
	protected bool	m_bDCO_Surrendered		= false;

	// Fake-surrender ambush watch (entities that kept a grenade and will drop it when an enemy closes).
	protected ref set<IEntity>	m_aDCO_FakeSurrenderWatch;
	protected bool				m_bDCO_FakeWatchActive	= false;
	// Fakers that have already triggered (springing / sprung) - left alone by surrender maintenance so the
	// throw sequence isn't re-posed mid-animation.
	protected ref set<IEntity>	m_aDCO_FakeSprung;
	// Terminal flag: a group that produced a fake-surrenderer never recovers morale or
	// un-surrenders - fake surrender is a one-way commitment.
	protected bool				m_bDCO_HasCommittedFaker	= false;
	// Faker's original faction, so a sprung ambusher can go loud (become hostile again) after the throw.
	protected ref map<IEntity, Faction>	m_mDCO_FakerFaction;
	// Morale contagion (cascade) scan throttle.
	protected float				m_fDCO_LastContagionCheck	= -1;

	// Last-known perceived-enemy centroid (Fix 2): cached whenever perception has targets so a flee
	// always has a coherent "away" direction even if perception goes quiet at the moment of breaking
	// (which is exactly when units used to flee to their own position = mill around aimlessly).
	protected vector	m_vDCO_LastThreatPos;
	protected bool		m_bDCO_HasLastThreat	= false;

	// Panic (R1) + morale-to-accuracy (R2) runtime state.
	protected bool	m_bDCO_Panicking			= false;
	protected float	m_fDCO_LastPanicTime		= -1;
	protected int	m_iDCO_LastAccuracyBand		= -1;	// -1 unset, 0 default skill, 1 regular, 2 rookie
	protected float	m_fDCO_LastArmorAvoidTime	= -1;	// hide-from-armour throttle

	// Surrender context-gate state (Phase 14).
	protected float	m_fDCO_LastCombatActivityMs	= -1;	// last tick the group was engaged (perceived enemy / threat); drives the surrender lull
	protected int	m_iDCO_LastStrength			= -1;	// living member count last tick, for the per-casualty morale hit

	// Leader-loss (decapitation) cohesion state. A group is never truly leaderless - the engine promotes a
	// new leader on death - so the meaningful signal is the change of leader coinciding with a casualty.
	protected IEntity	m_DCO_LastLeader;
	protected bool		m_bDCO_HadLeader		= false;
	protected float		m_fDCO_LastCasualtyMs	= -1;	// last time the group took a casualty (leader-loss window)

	// Surrender recovery (Phase 10) state.
	protected float	m_fDCO_SurrenderStartTime	= -1;	// when this group surrendered (ms)
	protected float	m_fDCO_LastSurrenderContact	= -1;	// last time an enemy was perceived while surrendered (ms)
	protected float	m_fDCO_LastRecoveryCheck	= -1;	// recovery-evaluation throttle (ms)
	protected ref array<ref DCO_SurrenderRecord>	m_aDCO_SurrenderRecords;

	// Surrender flee/lay-down (prisoner behaviour): per genuinely-surrendered member, the world time its current
	// flee order was issued. Presence = it has an active run order; the timestamp throttles re-issue. map<*,float>
	// is the proven container shape (cf. DCO_StanceUtil) - avoids an unverified vector-valued map.
	protected ref map<IEntity, float>	m_mDCO_SurrenderFleeSince;
	protected static const float		DCO_SURRENDER_REFLEE_MS = 4000.0;	// don't re-issue a prisoner's run within this

	protected static const float DCO_MORALE_MAX				= 100.0;
	protected static const float DCO_SURRENDER_NOFIRE_TIME	= 3.0;

	// The shared EvaluateActivity override that drives morale + reinforcement + QRF lives in
	// DCO_GroupQRF.c - it must sit in the alphabetically-last SCR_AIGroupUtilityComponent fragment
	// (Comms, Morale, QRF in that order) so every DCO_Update* method is already defined when it
	// compiles (Enforce resolves cross-fragment method calls in file-processing order).

	protected void DCO_UpdateMorale()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnabled)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastUpdateTime >= 0 && (now - m_fDCO_LastUpdateTime) < cfg.UpdateIntervalMs())
			return;
		m_fDCO_LastUpdateTime = now;

		if (m_bDCO_Surrendered)
		{
			DCO_MaintainSurrender();
			// Fake surrender is terminal: a group that sprang (or is holding) a grenade ambush never
			// recovers or un-surrenders. Only genuinely-surrendered groups may recover.
			if (cfg.m_bEnableSurrenderRecovery && !m_bDCO_HasCommittedFaker)
				DCO_UpdateSurrenderRecovery(now);
			return;
		}

		int strength;
		if (m_aInfoComponents)
			strength = m_aInfoComponents.Count();

		if (strength > m_iDCO_MaxStrength)
			m_iDCO_MaxStrength = strength;

		if (m_iDCO_MaxStrength <= 0 || strength <= 0)
			return;

		float strengthRatio	= strength / (float)m_iDCO_MaxStrength;
		float lossFrac		= 1.0 - strengthRatio;
		float threat		= GetThreatMeasure();

		// Per-casualty morale hit (Phase 14): each friendly lost since last tick lands an immediate morale
		// blow on top of the gradual drain, so losses are felt. m_fMoraleLostPerCasualty = 0 disables it.
		bool casualtyThisTick = (m_iDCO_LastStrength >= 0 && strength < m_iDCO_LastStrength);
		if (casualtyThisTick && cfg.m_fMoraleLostPerCasualty > 0)
		{
			int lost = m_iDCO_LastStrength - strength;
			m_fDCO_Morale -= lost * cfg.m_fMoraleLostPerCasualty;
		}
		if (casualtyThisTick)
			m_fDCO_LastCasualtyMs = now;
		m_iDCO_LastStrength = strength;

		// Combat-activity timestamp (Phase 14): the group counts as "in combat" while it perceives an enemy
		// or is under threat. This drives the surrender lull gate (don't surrender mid-firefight). Being hit
		// also refreshes it via DCO_NoteCombatActivity (called from the damage hook).
		bool inContact = false;
		if (m_Perception)
		{
			array<IEntity> ct = m_Perception.m_aTargetEntities;
			inContact = ct && !ct.IsEmpty();
		}
		if (inContact || threat >= cfg.m_fHeavyThreat)
			m_fDCO_LastCombatActivityMs = now;

		if (threat >= cfg.m_fHeavyThreat)
		{
			m_fDCO_Morale -= cfg.m_fDrainPerTick + (lossFrac * cfg.m_fCasualtyWeight);
			// Refresh the last-known enemy centroid while under fire, so that when morale finally
			// collapses the flee already has a valid "away" direction (Fix 2).
			vector tp;
			DCO_GetThreatPosition(tp);
		}
		else
			m_fDCO_Morale += cfg.m_fRecoveryPerTick;

		// Leader-loss shock (unit cohesion): losing the group's leader under fire is a cohesion blow as
		// the squad reorganizes. A group is never truly leaderless (the engine promotes a replacement),
		// so the real signal is the leader changing while a casualty was just taken (i.e. the old leader
		// was killed), not the absence of a leader. One-time hit; m_fMoraleLostOnLeaderLoss = 0 disables it.
		IEntity curLeader = m_Owner.GetLeaderEntity();
		bool recentCasualty = (m_fDCO_LastCasualtyMs >= 0 && (now - m_fDCO_LastCasualtyMs) <= 4000.0);
		if (m_bDCO_HadLeader && curLeader != m_DCO_LastLeader && recentCasualty && cfg.m_fMoraleLostOnLeaderLoss > 0)
		{
			m_fDCO_Morale -= cfg.m_fMoraleLostOnLeaderLoss;
			DCO_Debug.LogGroup("MORALE", curLeader, string.Format("leader-loss shock -%1 -> morale %2", cfg.m_fMoraleLostOnLeaderLoss, m_fDCO_Morale));
		}
		m_DCO_LastLeader = curLeader;
		m_bDCO_HadLeader = (curLeader != null);

		// Morale contagion: nearby broken/surrendered friendlies sap this group's nerve (panic spreads / routs
		// cascade). Throttled internally. Adjusts m_fDCO_Morale; the clamp below bounds it.
		DCO_UpdateMoraleContagion(now);

		// Composition "base morale": while among its own faction's compositions the group's morale floor is
		// raised, scaled by how many are nearby (capped) - defenders on home ground dig in and resist routing.
		// Proximity-based, and decays off the moment they leave (pull model; DCO_CompositionRegistry caches
		// the scan). 0 when the system is off or no compositions are near. See DCO_GroupComposition (Outpost).
		float compFloor = DCO_CompositionMoraleFloor(cfg);
		m_fDCO_Morale = Math.Clamp(m_fDCO_Morale, compFloor, DCO_MORALE_MAX);

		// Surrender decision (Phase 14): low morale is necessary but not sufficient. Beyond morale and
		// attrition, the group must also be out of combat (lull) and depleted (squad strength) before it may
		// surrender, and then only by chance. This is what stops mid-firefight surrenders; the per-member
		// disarm stagger (in DCO_Surrender) stops the whole team dropping weapons on the same instant.
		//  - m_fSurrenderThreshold 0      => never surrender (master off).
		//  - m_fSurrenderLullSec 0        => no lull requirement (legacy: may surrender mid-fight).
		//  - m_iSurrenderMinSquadToFight 0 => ignore squad strength.
		// Last-stand (Fix 5 / R3): when enabled, groups never surrender. Shared morale pool (F2): the casualty
		// gate uses the combined losses of nearby same-faction groups. The expensive pool scan only runs once
		// morale has actually collapsed to the threshold.
		bool doSurrender = false;
		if (cfg.m_bEnableSurrender && !cfg.m_bEnableLastStand
			&& cfg.m_fSurrenderThreshold > 0 && m_fDCO_Morale <= cfg.m_fSurrenderThreshold)
		{
			// Squad-strength gate: a group still fielding enough men keeps fighting.
			bool squadDepleted = cfg.m_iSurrenderMinSquadToFight <= 0 || strength < cfg.m_iSurrenderMinSquadToFight;

			// Combat-lull gate: must have been out of contact for the lull window. (LastCombatActivity is
			// seeded on the first contacted tick; if it was never set, treat as "in combat" = not eligible.)
			bool lullOk = cfg.m_fSurrenderLullSec <= 0
				|| (m_fDCO_LastCombatActivityMs >= 0 && (now - m_fDCO_LastCombatActivityMs) >= cfg.m_fSurrenderLullSec * 1000.0);

			float surrenderLossFrac = lossFrac;
			if (squadDepleted && lullOk)
			{
				if (cfg.m_bEnableSharedMorale)
					surrenderLossFrac = DCO_GetPooledLossFrac(lossFrac);
				if (surrenderLossFrac >= cfg.m_fMinCasualtyFractionForSurrender && Math.RandomFloat01() <= cfg.m_fSurrenderChancePerTick)
					doSurrender = true;
			}

			// DEBUG: eligible by morale - show exactly which gate is blocking (or passing) the surrender.
			// (string.Format caps at %1..%9, so this is split into two lines.)
			if (DCO_Debug.Enabled() && m_Owner)
			{
				// (Enforce has no ternary - precompute the seconds-since-combat; -1 means never been in combat.)
				float secsSinceCombat = -1.0;
				if (m_fDCO_LastCombatActivityMs >= 0)
					secsSinceCombat = (now - m_fDCO_LastCombatActivityMs) / 1000.0;

				IEntity dbgLeader = m_Owner.GetLeaderEntity();
				DCO_Debug.LogGroup("SURRENDER", dbgLeader, string.Format(
					"eligible morale=%1<=%2 | squadDepleted=%3 (str %4 vs keep %5) | lullOk=%6 (%7s since combat)",
					m_fDCO_Morale, cfg.m_fSurrenderThreshold, squadDepleted, strength, cfg.m_iSurrenderMinSquadToFight,
					lullOk, secsSinceCombat));
				DCO_Debug.LogGroup("SURRENDER", dbgLeader, string.Format(
					"  lossFrac=%1>=%2 | -> surrender=%3",
					surrenderLossFrac, cfg.m_fMinCasualtyFractionForSurrender, doSurrender));
			}
		}
		if (doSurrender)
		{
			DCO_Surrender();
		}
		else if (m_fDCO_Morale <= cfg.m_fFleeThreshold)
		{
			// Fanatic troop grade: hold ground - never break/flee.
			if (DCO_BaseSettings.Get().m_bEnableBaseSettings && DCO_BaseSettings.Get().m_bFanaticHoldsGround)
			{
				// stay and fight; skip the flee logic entirely
			}
			else if (!m_bDCO_Broken)
			{
				// First break: roll the flee chance so it doesn't trigger the instant morale dips.
				if (Math.RandomFloat01() <= cfg.m_fFleeChancePerTick)
					DCO_BreakAndFlee();
			}
			else
			{
				// Already broken and still below the flee threshold: re-issue the flee each tick so the
				// group keeps committing to a single "away" direction instead of milling / reverting to
				// CRX's combat behaviour between flee orders (Fix 2 - aimless flee).
				DCO_BreakAndFlee();
			}
		}
		else if (cfg.m_bEnablePanic && !m_bDCO_Broken && m_fDCO_Morale <= cfg.m_fFleeThreshold + cfg.m_fPanicBand)
		{
			// Panic band (R1): morale is just above the flee threshold - the group may briefly lose its
			// nerve (cower + hold fire) before a further drop tips it into the flee branch above.
			DCO_MaybePanic();
		}
		else if (m_bDCO_Broken && m_fDCO_Morale >= cfg.m_fRallyThreshold)
		{
			m_bDCO_Broken = false;
		}

		// Morale-to-accuracy (R2): step AI combat skill with the group's morale (re-applied only on change).
		if (cfg.m_bEnableMoraleAccuracy)
			DCO_ApplyMoraleAccuracy();
		else if (m_Owner && m_iDCO_LastAccuracyBand > 0)
		{
			// Turned off while degraded: restore default skill + fire rate once.
			array<AIAgent> ar = {};
			m_Owner.GetAgents(ar);
			foreach (AIAgent aa : ar)
			{
				if (!aa) continue;
				IEntity ae = aa.GetControlledEntity();
				if (!ae) continue;
				if (DCO_PlayerUtil.IsPlayer(ae)) continue;	// never touch a player's skill/fire-rate
				SCR_AICombatComponent ac = SCR_AICombatComponent.Cast(ae.FindComponent(SCR_AICombatComponent));
				if (!ac) continue;
				ac.ResetAISkill();
				ac.SetFireRateCoef(1.0, false);
			}
			m_iDCO_LastAccuracyBand = -1;
		}
	}

	// Base-morale floor from nearby own-faction compositions (see DCO_GroupComposition / Outpost). Returns 0
	// if the system is off, the group has no leader/faction, or no compositions are within range; otherwise
	// min(cap, count * perUnit) on the 0-100 scale. Pull model - reads DCO_CompositionRegistry directly.
	protected float DCO_CompositionMoraleFloor(DCO_MoraleSettings cfg)
	{
		if (!cfg || !cfg.m_bEnableCompositionDefense || !m_Owner)
			return 0.0;

		IEntity ldr = m_Owner.GetLeaderEntity();
		if (!ldr)
			return 0.0;

		Faction fac = m_Owner.GetFaction();
		if (!fac)
			return 0.0;

		int n = DCO_CompositionRegistry.Get().CountNear(ldr.GetOrigin(), fac, cfg.m_fCompositionRadius);
		if (n <= 0)
			return 0.0;

		return Math.Min(cfg.m_fCompositionMoraleCap, n * cfg.m_fCompositionMoralePerUnit);
	}

	protected void DCO_BreakAndFlee()
	{
		if (!m_Mailbox || !m_Owner)
			return;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		bool wasBroken = m_bDCO_Broken;	// so smoke deploys once per break, not on every flee re-issue

		vector leaderPos = leader.GetOrigin();
		vector fleePos = leaderPos;

		vector threatPos;
		bool hasThreat = DCO_GetThreatPosition(threatPos);
		if (!hasThreat && m_bDCO_HasLastThreat)
		{
			// Perception has gone quiet (common right after a break) - fall back to the last-known enemy
			// centroid so we still flee directly away from where the enemy was, not to our own position.
			threatPos = m_vDCO_LastThreatPos;
			hasThreat = true;
		}
		if (hasThreat)
		{
			vector dir = leaderPos - threatPos;
			dir[1] = 0;
			if (dir.LengthSq() > 0.01)
			{
				dir.Normalize();
				fleePos = leaderPos + dir * DCO_MoraleSettings.Get().m_fFleeDistance;
			}
		}

		// Mounted crew (especially aircraft): the on-foot SCR_AIMessage_Flee below makes the crew disembark
		// and walk away - for a helicopter/aircraft that means bailing out and falling to their death.
		// Instead, order a vehicle-aware withdrawal so the crew drives/flies away from the threat, still
		// flagging the group as broken so the rest of the morale state machine behaves consistently.
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
		{
			DCO_VehicleUtil.OrderGroupMoveToPosition(m_Owner, fleePos, m_Mailbox);
			m_bDCO_Broken = true;

			if (DCO_Debug.Enabled())
				DCO_Debug.LogGroup("FLEE", leader, string.Format("mounted break -> vehicle withdraw to %1 (hasThreat=%2, morale=%3)", fleePos, hasThreat, m_fDCO_Morale));
			return;
		}

		// No static Create() factory exists for SCR_AIMessage_Flee; mailbox.CreateMessage(TypeName) is obsolete in
		// 1.7, so construct directly (the same path the Create() wrappers use for the message types that have one).
		SCR_AIMessage_Flee msg = new SCR_AIMessage_Flee();
		if (!msg)
			return;

		msg.SetPosition(fleePos);
		m_Mailbox.RequestBroadcast(msg);

		if (DCO_Debug.Enabled())
			DCO_Debug.LogGroup("FLEE", leader, string.Format("break&flee -> %1 (hasThreat=%2, morale=%3)", fleePos, hasThreat, m_fDCO_Morale));

		m_bDCO_Broken = true;

		// Smoke-covered retreat (Phase 3): screen the withdrawal toward the enemy, once per break.
		if (!wasBroken && hasThreat && DCO_MoraleSettings.Get().m_bEnableFleeSmoke)
			DCO_DeploySmoke(threatPos);
	}

	// Deploy a smoke screen between the fleeing group and the threat, via the native smoke-cover feature.
	// Execute's 3rd param SCR_AIActivitySmokeCoverFeatureProperties is an enum, not a class - passing null
	// for it gives "Cannot convert 'Class' to 'int' for argument 2", so pass .NONE. The feature only picks
	// agents that hold smoke grenades (EUnitRole.HAS_SMOKE_GRENADE) and are within ~40 m of the screen
	// position, so it fails silently if the group has no smoke.
	protected void DCO_DeploySmoke(vector threatPos)
	{
		if (!m_Owner)
			return;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		// Screen a point a short way toward the enemy from the group, so the smoke drops between them
		// (and stays within the feature's ~40 m thrower range of the group).
		vector leaderPos = leader.GetOrigin();
		vector dir = threatPos - leaderPos;
		dir[1] = 0;
		if (dir.LengthSq() > 0.01)
			dir.Normalize();
		vector screenPos = leaderPos + dir * 12.0;

		SCR_AIActivitySmokeCoverFeature feat = new SCR_AIActivitySmokeCoverFeature();
		if (!feat)
			return;

		array<AIAgent> avoid = {};
		array<AIAgent> exclude = {};
		// arg3 (properties) must be an enum value, not null; NONE = drop smoke directly at the screen point.
		feat.Execute(this, screenPos, SCR_AIActivitySmokeCoverFeatureProperties.NONE, avoid, exclude, 1, null);
	}

	// Hide from armour: an on-foot group that perceives an enemy vehicle within range breaks and flees
	// (for infantry that can't fight armour). Lives in the Morale fragment so it can call DCO_BreakAndFlee.
	void DCO_UpdateArmorAvoid()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableFleeFromArmor || !m_Owner || !m_Perception)
			return;

		if (m_bDCO_Surrendered || m_bDCO_Broken)
			return;	// already out of the fight

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastArmorAvoidTime >= 0 && (now - m_fDCO_LastArmorAvoidTime) < cfg.m_fFleeFromArmorCheckSec * 1000.0)
			return;
		m_fDCO_LastArmorAvoidTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;

		// If we're crewing a vehicle ourselves, don't flee from armour.
		if (CompartmentAccessComponent.GetVehicleIn(leader))
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		vector leaderPos = leader.GetOrigin();
		float rangeSq = cfg.m_fFleeFromArmorRange * cfg.m_fFleeFromArmorRange;

		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			if (!t.FindComponent(BaseVehicleControllerComponent))
				continue;	// only enemy vehicles trigger this
			if (vector.DistanceSq(t.GetOrigin(), leaderPos) <= rangeSq)
			{
				DCO_BreakAndFlee();	// run from the armour
				return;
			}
		}
	}

	// Morale contagion / cascade: a group loses heart when nearby same-faction friendlies have broken or
	// surrendered (panic spreads, routs cascade). Throttled scan of same-faction groups within
	// m_fContagionRadius; each broken/surrendered neighbour subtracts m_fContagionMoralePerBrokenGroup (capped
	// per check). Reads neighbours' m_bDCO_Broken/m_bDCO_Surrendered (same modded class, protected access on
	// other instances is fine, as in DCO_GetPooledLossFrac). Server-side; default OFF; caller clamps morale.
	protected void DCO_UpdateMoraleContagion(float now)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableMoraleContagion || !m_Owner)
			return;

		if (m_fDCO_LastContagionCheck >= 0 && (now - m_fDCO_LastContagionCheck) < cfg.m_fContagionCheckSec * 1000.0)
			return;
		m_fDCO_LastContagionCheck = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector selfPos = leader.GetOrigin();
		Faction selfFaction = m_Owner.GetFaction();

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		float radiusSq = cfg.m_fContagionRadius * cfg.m_fContagionRadius;
		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		seen.Insert(m_Owner);
		int brokenNearby = 0;

		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(a.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			if (grp.GetFaction() != selfFaction)
				continue;
			IEntity gl = grp.GetLeaderEntity();
			if (!gl || vector.DistanceSq(gl.GetOrigin(), selfPos) > radiusSq)
				continue;

			SCR_AIGroupUtilityComponent gu = grp.GetGroupUtilityComponent();
			if (gu && (gu.m_bDCO_Broken || gu.m_bDCO_Surrendered))
				brokenNearby++;
		}

		if (brokenNearby <= 0)
			return;

		float loss = brokenNearby * cfg.m_fContagionMoralePerBrokenGroup;
		if (loss > cfg.m_fContagionMaxLossPerTick)
			loss = cfg.m_fContagionMaxLossPerTick;
		m_fDCO_Morale -= loss;

		DCO_Debug.LogGroup("MORALE", leader, string.Format("contagion: %1 broken/surrendered nearby -> -%2", brokenNearby, loss));
	}

	// Shared morale pool (F2): combined loss fraction (1 - curStrength/maxStrength) of this group plus all
	// same-faction groups within m_fMoralePoolRadius. Used only to gate surrender, so a small group near a
	// strong friendly force inherits the force's casualty resilience. Returns ownLossFrac if pooling fails.
	protected float DCO_GetPooledLossFrac(float ownLossFrac)
	{
		if (!m_Owner)
			return ownLossFrac;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return ownLossFrac;

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return ownLossFrac;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		vector selfPos = leader.GetOrigin();
		Faction selfFaction = m_Owner.GetFaction();
		float radiusSq = cfg.m_fMoralePoolRadius * cfg.m_fMoralePoolRadius;

		int totalMax = m_iDCO_MaxStrength;
		int totalCur = 0;
		if (m_aInfoComponents)
			totalCur = m_aInfoComponents.Count();

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		seen.Insert(m_Owner);

		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;

			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			if (grp.GetFaction() != selfFaction)
				continue;

			IEntity grpLeader = grp.GetLeaderEntity();
			if (!grpLeader || vector.DistanceSq(grpLeader.GetOrigin(), selfPos) > radiusSq)
				continue;

			SCR_AIGroupUtilityComponent gu = grp.GetGroupUtilityComponent();
			if (!gu)
				continue;

			int gcur = 0;
			if (gu.m_aInfoComponents)
				gcur = gu.m_aInfoComponents.Count();
			int gmax = gu.m_iDCO_MaxStrength;
			if (gcur > gmax)
				gmax = gcur;	// group hasn't recorded its peak yet - don't undercount the pool's max
			totalMax += gmax;
			totalCur += gcur;
		}

		if (totalMax <= 0)
			return ownLossFrac;

		return 1.0 - (totalCur / (float)totalMax);
	}

	// Panic (R1): roll the chance (respecting the per-group cooldown) and, on success, briefly cower the
	// whole group. A short loss of nerve that precedes the actual break/flee.
	protected void DCO_MaybePanic()
	{
		if (m_bDCO_Panicking)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;

		float now = world.GetWorldTime();
		if (m_fDCO_LastPanicTime >= 0 && (now - m_fDCO_LastPanicTime) < cfg.m_fPanicCooldownSec * 1000.0)
			return;

		if (Math.RandomFloat01() > cfg.m_fPanicChancePerTick)
			return;

		m_fDCO_LastPanicTime = now;
		DCO_Panic();
	}

	// Per member: hold fire and drop to a crouch for the panic duration. Self-clears via DCO_EndPanic;
	// the no-fire window expires on its own and CRX resumes control afterwards.
	protected void DCO_Panic()
	{
		if (!m_Owner)
			return;

		// Mounted crew can't meaningfully "cower" - ForceStance on a seated character is pointless and can
		// glitch a pilot/crew; skip panic while crewing a vehicle.
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		m_bDCO_Panicking = true;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		int panicMs = (int)(cfg.m_fPanicDurationSec * 1000.0);

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
				continue;	// never panic-freeze a player

			SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
			if (cc)
			{
				cc.SetWeaponNoFireTime(cfg.m_fPanicDurationSec);
				cc.ForceStance(ECharacterStance.CROUCH);
			}
		}

		GetGame().GetCallqueue().CallLater(DCO_EndPanic, panicMs, false);
	}

	protected void DCO_EndPanic()
	{
		m_bDCO_Panicking = false;
	}

	// Morale-to-accuracy (R2): pick a skill band from the current morale and, only when it changes, push
	// that AI skill (+ a fire-rate coefficient at the lowest band) onto every member's combat component.
	// ROOKIE/REGULAR degrade aim; the default skill is restored via ResetAISkill when morale recovers.
	protected void DCO_ApplyMoraleAccuracy()
	{
		if (!m_Owner)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		int band;
		if (m_fDCO_Morale <= cfg.m_fAccuracyRookieMorale)
			band = 2;
		else if (m_fDCO_Morale <= cfg.m_fAccuracyRegularMorale)
			band = 1;
		else
			band = 0;

		if (band == m_iDCO_LastAccuracyBand)
			return;
		m_iDCO_LastAccuracyBand = band;

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
				continue;	// never touch a player's fire-rate/skill

			SCR_AICombatComponent combat = SCR_AICombatComponent.Cast(ent.FindComponent(SCR_AICombatComponent));
			if (!combat)
				continue;

			if (band == 2)
			{
				combat.SetAISkill(EAISkill.ROOKIE);
				combat.SetFireRateCoef(cfg.m_fLowMoraleFireRateCoef, false);
			}
			else if (band == 1)
			{
				combat.SetAISkill(EAISkill.REGULAR);
				combat.SetFireRateCoef(1.0, false);
			}
			else
			{
				// High morale: settle to the DCO base settings baseline (not the prefab default), so the
				// global skill/fire-rate levers take effect when morale is healthy. Falls back to the engine
				// default when base settings is disabled.
				if (DCO_BaseSettings.Get().m_bEnableBaseSettings)
				{
					combat.SetAISkill(DCO_BaseSettingsUtil.BaselineSkill());
					combat.SetFireRateCoef(DCO_BaseSettingsUtil.BaselineFireRateCoef(), false);
				}
				else
				{
					combat.ResetAISkill();
					combat.SetFireRateCoef(1.0, false);
				}
			}
		}
	}

	protected void DCO_Surrender()
	{
		// Mounted crew (especially aircraft): surrendering forces each member to crouch/prone/disarm, which
		// ejects a seated pilot/crew - a flying crew would fall to their death and a driver would abandon a moving
		// vehicle. Don't surrender while crewing a vehicle; the group stays un-flagged so it can still
		// surrender later once on foot (e.g. after the vehicle is destroyed or they dismount).
		if (m_Owner && DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		m_bDCO_Surrendered = true;

		if (m_Owner)
			DCO_Debug.LogGroup("SURRENDER", m_Owner.GetLeaderEntity(), "group is surrendering now (staggered disarm)");

		// Stamp the surrender time so recovery can enforce a minimum hold and a no-contact timer (Phase 10).
		BaseWorld surrWorld = GetGame().GetWorld();
		if (surrWorld)
		{
			m_fDCO_SurrenderStartTime = surrWorld.GetWorldTime();
			m_fDCO_LastSurrenderContact = m_fDCO_SurrenderStartTime;
		}

		if (!m_Owner)
			return;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		int idx = 0;
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity ent = agent.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never surrender/disarm a player

			// Per-member disarm stagger (Phase 14): give each member a random delay up to
			// m_fSurrenderStaggerMaxSec before it actually begins surrendering, so the squad doesn't drop
			// weapons on the exact same instant. 0 = simultaneous (legacy). Even at 0 we go through CallLater(0)
			// which fires the same frame, so behaviour is unchanged when disabled.
			int staggerMs = 0;
			if (cfg.m_fSurrenderStaggerMaxSec > 0)
				staggerMs = (int)(Math.RandomFloat01() * cfg.m_fSurrenderStaggerMaxSec * 1000.0);
			GetGame().GetCallqueue().CallLater(DCO_BeginAgentSurrender, staggerMs, false, ent);

			// Surrender voiceline (Fix 4): stagger across members so a whole squad doesn't shout at once.
			if (cfg.m_bEnableSurrenderVoice && cfg.m_sSurrenderSoundEvent != "")
				GetGame().GetCallqueue().CallLater(DCO_PlaySurrenderVoice, idx * cfg.m_iSurrenderVoiceStaggerMs, false, ent);

			idx++;
		}
	}

	// Public: refresh the group's "last combat activity" stamp. Called from the damage hook so being hit
	// (not just perceiving an enemy) keeps the surrender lull open. Safe to call on any machine; only the
	// server-side morale tick reads it.
	void DCO_NoteCombatActivity()
	{
		BaseWorld world = GetGame().GetWorld();
		if (world)
			m_fDCO_LastCombatActivityMs = world.GetWorldTime();

		if (DCO_Debug.Enabled() && m_Owner)
			DCO_Debug.LogGroup("COMBAT", m_Owner.GetLeaderEntity(), "combat activity noted (hit) - surrender lull reset");
	}

	// Instance wrapper so the staggered CallLater can fire the (static) surrender-voice helper.
	protected void DCO_PlaySurrenderVoice(IEntity character)
	{
		DCO_SurrenderVoice.Play(character);
	}

	protected void DCO_BeginAgentSurrender(IEntity character)
	{
		if (!character)
			return;
		if (DCO_PlayerUtil.IsPlayer(character))
			return;	// a player is never surrendered/disarmed/frozen

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		// Ordering: we do not clear the faction here anymore. Clearing it makes friendly AI
		// stop engaging the unit, so it must only happen after the unit is actually disarmed - otherwise
		// a still-armed unit becomes "white"/neutral mid-firefight and is wrongly discounted. Faction is
		// neutralized via DCO_NeutralizeFaction, scheduled only once the weapon has been dropped below.

		// Stop fighting + adopt the surrender pose immediately.
		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (cc)
		{
			cc.SetWeaponRaised(false);
			cc.SetWeaponNoFireTime(99999);
			cc.ForceStance(ECharacterStance.CROUCH);
		}

		GetGame().GetCallqueue().CallLater(DCO_AgentGoProne, cfg.m_iSurrenderProneDelayMs, false, character);

		// The faker path must be a per-unit chance, not the master toggle. Previously, with
		// m_bEnableFakeSurrender on (default), every surrenderer kept a grenade and was left un-frozen,
		// so the whole squad stood holding a smoke/grenade and never lay down. Now only m_fFakeSurrenderChance
		// of them become ambushers; the rest take the genuine path (drop everything, go prone, freeze).
		bool fakeThisOne = cfg.m_bEnableFakeSurrender && Math.RandomFloat01() <= cfg.m_fFakeSurrenderChance;
		if (fakeThisOne)
		{
			// Grenade-drop feature on: drop only the primary weapon (still looks surrendered) but keep
			// grenades, and watch for an approaching player. The 25% roll is deferred to when a player
			// closes within range (DCO_FakeSurrenderTick), not now, so the drop can actually catch them.
			SCR_CharacterInventoryStorageComponent charInv = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
			if (charInv)
				charInv.DropCurrentItem();

			// Remember the faker's faction before it's neutralized, so it can be restored when the ambusher
			// "goes loud" after springing the grenade (DCO_FakerGoLoud).
			FactionAffiliationComponent fakerFac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
			if (fakerFac)
			{
				if (!m_mDCO_FakerFaction)
					m_mDCO_FakerFaction = new map<IEntity, Faction>();
				m_mDCO_FakerFaction.Set(character, fakerFac.GetAffiliatedFaction());
			}

			// Neutralize only after the rifle is down (so they never read as an armed combatant while white).
			// The kept grenade is the deliberate ambush aimed at the approaching player.
			GetGame().GetCallqueue().CallLater(DCO_NeutralizeFaction, 700, false, character);
			DCO_AddFakeSurrenderWatch(character);

			// Mark the whole group terminal: with a committed faker, the group can never recover (see
			// DCO_UpdateMorale). The faker's choice to ambush is irreversible.
			m_bDCO_HasCommittedFaker = true;
		}
		else
		{
			// Recovery (Phase 10): snapshot faction + held weapon before we drop/neutralize, so the unit can
			// be re-armed and put back on its side later. Captured only when recovery is enabled.
			if (cfg.m_bEnableSurrenderRecovery)
				DCO_CaptureSurrenderRecord(character);

			// Genuine surrender: always drop the primary weapon (gear-drop stays a toggle inside
			// DCO_AgentDropGear), then neutralize the faction once unarmed - never armed-while-white.
			GetGame().GetCallqueue().CallLater(DCO_AgentDropGear, cfg.m_iSurrenderDropDelayMs, false, character);
			GetGame().GetCallqueue().CallLater(DCO_NeutralizeFaction, cfg.m_iSurrenderDropDelayMs + 700, false, character);

			// Surrender hold (Fix 1): once the unit is posed prone, freeze its AI brain so CRX can't drive
			// it back up / wander - the #1 cause of units "not laying down". Scheduled after the prone beat.
			// Fakers are not frozen (they need to stay active to spring the grenade), so this is genuine-only.
			// Skipped when flee/lay-down is on: a fleeing prisoner must stay AI-active so it can run and stop
			// on approach - DCO_MaintainSurrender drives its pose/movement instead of a hard freeze.
			if (cfg.m_bFreezeSurrenderedAI && !cfg.m_bEnableSurrenderFlee)
				GetGame().GetCallqueue().CallLater(DCO_FreezeSurrenderedAgent, cfg.m_iSurrenderProneDelayMs + cfg.m_iSurrenderFreezeDelayMs, false, character);
		}
	}

	// Add a surrendered unit to the grenade-drop watch list (starts the 0.5s watch tick if needed).
	protected void DCO_AddFakeSurrenderWatch(IEntity character)
	{
		if (!m_aDCO_FakeSurrenderWatch)
			m_aDCO_FakeSurrenderWatch = new set<IEntity>();
		m_aDCO_FakeSurrenderWatch.Insert(character);

		if (!m_bDCO_FakeWatchActive)
		{
			m_bDCO_FakeWatchActive = true;
			GetGame().GetCallqueue().CallLater(DCO_FakeSurrenderTick, 500, true);
		}
	}

	// Per-0.5s watch (only while the grenade-drop feature is enabled): the first time a player comes
	// within trigger range of a surrendered unit, roll the drop chance once. On success the unit drops
	// a primed grenade; either way it is removed from the watch (one-shot - no early or repeated rolls).
	protected void DCO_FakeSurrenderTick()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableFakeSurrender || !m_aDCO_FakeSurrenderWatch || m_aDCO_FakeSurrenderWatch.IsEmpty())
		{
			DCO_StopFakeWatch();
			return;
		}

		PlayerManager playerMgr = GetGame().GetPlayerManager();
		if (!playerMgr)
			return;

		array<int> players = {};
		playerMgr.GetPlayers(players);
		if (players.IsEmpty())
			return;

		float rangeSq = cfg.m_fFakeSurrenderTriggerRange * cfg.m_fFakeSurrenderTriggerRange;

		for (int i = m_aDCO_FakeSurrenderWatch.Count() - 1; i >= 0; i--)
		{
			IEntity faker = m_aDCO_FakeSurrenderWatch.Get(i);
			if (!faker)
			{
				m_aDCO_FakeSurrenderWatch.Remove(i);
				continue;
			}

			vector fakerPos = faker.GetOrigin();
			bool playerClose = false;
			foreach (int playerId : players)
			{
				IEntity p = playerMgr.GetPlayerControlledEntity(playerId);
				if (p && vector.DistanceSq(p.GetOrigin(), fakerPos) <= rangeSq)
				{
					playerClose = true;
					break;
				}
			}

			if (playerClose)
			{
				// One roll the moment a player enters trigger range (separate drop chance, not the selection
				// chance), then stop watching this unit either way (one-shot - no repeated rolls). On success
				// the faker is marked "sprung" so surrender maintenance leaves the throw sequence alone.
				m_aDCO_FakeSurrenderWatch.Remove(i);
				if (Math.RandomFloat01() <= cfg.m_fFakeSurrenderDropChance)
				{
					DCO_MarkFakeSprung(faker);
					DCO_DropPrimedGrenade(faker);
				}
			}
		}

		if (m_aDCO_FakeSurrenderWatch.IsEmpty())
			DCO_StopFakeWatch();
	}

	// Equip the grenade, then (after the switch) throw it with near-zero speed so it drops primed
	// right in front of the prone unit - the engine primes/fuzes/detonates the real grenade.
	protected void DCO_DropPrimedGrenade(IEntity character)
	{
		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!wm)
			return;

		BaseWeaponComponent grenade = wm.GetCurrentGrenade();
		if (!grenade)
		{
			// No grenade to spring the ambush with. If a grenade prefab is configured, spawn one into the
			// faker's inventory and retry once on the next beat (the spawn needs a tick to settle before the
			// weapon manager can surface it). The retry goes through the no-spawn path so a grenade that never
			// becomes selectable can't cause an endless spawn loop. Empty prefab = leave as-is (no ambush).
			if (DCO_TrySpawnAmbushGrenade(character))
				GetGame().GetCallqueue().CallLater(DCO_DropPrimedGrenadeNoSpawn, 600, false, character);
			return;
		}

		wm.SelectWeapon(grenade);
		GetGame().GetCallqueue().CallLater(DCO_ExecuteGrenadeDrop, 350, false, character);
	}

	// Retry the grenade drop without spawning (used after a spawn-in-hand). If the grenade still isn't the
	// current grenade it gives up quietly - never re-spawns, so there is no spawn loop.
	protected void DCO_DropPrimedGrenadeNoSpawn(IEntity character)
	{
		if (!character)
			return;

		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!wm)
			return;

		BaseWeaponComponent grenade = wm.GetCurrentGrenade();
		if (!grenade)
			return;

		wm.SelectWeapon(grenade);
		GetGame().GetCallqueue().CallLater(DCO_ExecuteGrenadeDrop, 350, false, character);
	}

	// Give a faker a grenade when it has none, so the ambush can still spring. Mirrors the recovery re-arm
	// spawn path (DCO_ResupplyAfterRearm): TrySpawnPrefabToStorage drops the configured prefab straight into
	// the character's inventory. Returns true if a spawn was attempted. No-op when no prefab is configured
	// (no fabricated GUIDs - the server owner sets m_sFakeSurrenderGrenadePrefab).
	protected bool DCO_TrySpawnAmbushGrenade(IEntity character)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (cfg.m_sFakeSurrenderGrenadePrefab == string.Empty)
			return false;

		SCR_InventoryStorageManagerComponent scrMgr = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!scrMgr)
			return false;

		return scrMgr.TrySpawnPrefabToStorage(cfg.m_sFakeSurrenderGrenadePrefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, 1);
	}

	// Mark a faker as having triggered, so surrender maintenance leaves its throw sequence alone.
	protected void DCO_MarkFakeSprung(IEntity character)
	{
		if (!m_aDCO_FakeSprung)
			m_aDCO_FakeSprung = new set<IEntity>();
		m_aDCO_FakeSprung.Insert(character);
	}

	protected void DCO_ExecuteGrenadeDrop(IEntity character)
	{
		if (!character)
			return;

		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (!wm)
			return;

		vector forward = character.GetTransformAxis(2);	// drop it out in front of their face
		wm.Throw(forward, DCO_MoraleSettings.Get().m_fFakeSurrenderThrowSpeed);

		// Follow-up: a beat after the grenade is away, the ambusher reveals as hostile again so the player and
		// friendly AI engage it (the deception is spent).
		GetGame().GetCallqueue().CallLater(DCO_FakerGoLoud, 800, false, character);
	}

	// A sprung faker "goes loud": restore its original faction, clear the disarmed-perception flag and the
	// no-fire hold so it is treated and fought as a combatant. (Its primary was dropped on surrender, so it
	// typically fights with whatever it has left or is cut down - the intended end of a grenade ambush.)
	protected void DCO_FakerGoLoud(IEntity character)
	{
		if (!character)
			return;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (fac && m_mDCO_FakerFaction && m_mDCO_FakerFaction.Contains(character))
			fac.SetAffiliatedFaction(m_mDCO_FakerFaction.Get(character));

		PerceivableComponent perc = PerceivableComponent.Cast(character.FindComponent(PerceivableComponent));
		if (perc)
			perc.SetDisarmed(false);

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (cc)
			cc.SetWeaponNoFireTime(0);
	}

	protected void DCO_StopFakeWatch()
	{
		m_bDCO_FakeWatchActive = false;
		if (GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(DCO_FakeSurrenderTick);
	}

	// Disarm a genuinely-surrendering unit. Always drops the primary weapon (a surrendered unit must be
	// unarmed - this is what gates the faction neutralization). Dropping the rest of the gear (vest,
	// backpack, etc - keeping jacket / pants / boots) is optional via m_bDropGearOnSurrender.
	protected void DCO_AgentDropGear(IEntity character)
	{
		if (!character)
			return;

		SCR_CharacterInventoryStorageComponent charInv = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!charInv)
			return;

		// Always disarm - the primary weapon must come out of their hands on surrender.
		charInv.DropCurrentItem();

		if (DCO_MoraleSettings.Get().m_bDropGearOnSurrender && invMgr)
		{
			DCO_DropArea(charInv, invMgr, LoadoutVestArea);
			DCO_DropArea(charInv, invMgr, LoadoutArmoredVestSlotArea);
			DCO_DropArea(charInv, invMgr, LoadoutBackpackArea);
			DCO_DropArea(charInv, invMgr, LoadoutHeadCoverArea);
			DCO_DropArea(charInv, invMgr, LoadoutGooglesArea);
			DCO_DropArea(charInv, invMgr, LoadoutBinocularsArea);
			DCO_DropArea(charInv, invMgr, LoadoutCoverArea);
			DCO_DropArea(charInv, invMgr, LoadoutHandwearSlotArea);
		}

		// Some characters auto-equip a grenade/pistol once the primary weapon is dropped, leaving a
		// surrendering unit visibly "holding a grenade" (a perceived threat). Clear the hand again a
		// moment later so they end up empty-handed. (The 25% fake-surrender variant that keeps a
		// grenade for an ambush is handled on a separate path and skips this.)
		GetGame().GetCallqueue().CallLater(DCO_DropHeldItem, 400, false, character);
		GetGame().GetCallqueue().CallLater(DCO_DropHeldItem, 1000, false, character);
	}

	// Clear the unit's faction so former enemies stop engaging it. Called only after the unit has been
	// disarmed, so a still-armed unit is never wrongly turned neutral/"white" mid-fight.
	protected void DCO_NeutralizeFaction(IEntity character)
	{
		if (!character)
			return;

		// (1) Clear the affiliated faction (GM/map "white" rendering + faction-keyed systems).
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (fac)
			fac.SetAffiliatedFaction(null);

		// (2) Root-cause fix for "surrendered AI don't go white / keep getting shot": AI target selection
		// reads the perceived faction from the PerceivableComponent, which for a character is derived from its
		// outfit, not the affiliated faction. So clearing affiliation alone leaves a still-uniformed
		// surrenderer perceived as hostile and under fire. SetDisarmed(true) is the engine's intended "no
		// longer a combat threat" flag the perception system honours, so former enemies disengage. (It also
		// sells the fake-surrender ambush: the approaching player reads the unit as harmless.)
		PerceivableComponent perc = PerceivableComponent.Cast(character.FindComponent(PerceivableComponent));
		if (perc)
			perc.SetDisarmed(true);

		// (3) Re-assert once shortly after, in case inventory/AI churn re-derived the faction or re-armed the
		// perceivable between the disarm and the unit settling into its pose.
		GetGame().GetCallqueue().CallLater(DCO_ReassertNeutralized, 1500, false, character);
	}

	// Belt-and-suspenders second pass of the surrender neutralization (see DCO_NeutralizeFaction).
	protected void DCO_ReassertNeutralized(IEntity character)
	{
		if (!character)
			return;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (fac && fac.GetAffiliatedFaction())
			fac.SetAffiliatedFaction(null);

		PerceivableComponent perc = PerceivableComponent.Cast(character.FindComponent(PerceivableComponent));
		if (perc && !perc.IsDisarmed())
			perc.SetDisarmed(true);
	}

	// Drop whatever the character is currently holding (used to clear an auto-equipped grenade after surrender).
	protected void DCO_DropHeldItem(IEntity character)
	{
		if (!character)
			return;

		SCR_CharacterInventoryStorageComponent charInv = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (charInv)
			charInv.DropCurrentItem();
	}

	protected void DCO_DropArea(SCR_CharacterInventoryStorageComponent charInv, InventoryStorageManagerComponent invMgr, typename area)
	{
		IEntity item = charInv.GetClothFromArea(area);
		if (!item)
			return;

		// Gear-drop did nothing: worn clothing lives in a loadout slot, not a regular storage slot,
		// so the previous charInv.FindItemSlot(item) returned null and we bailed before dropping anything.
		// Use the item's own parent slot (the vanilla drop path: SCR_InventoryStorageManagerComponent.
		// TryRemoveItemFromInventory does exactly GetParentSlot, GetStorage, then TryRemoveItemFromStorage,
		// which is the engine's real drop-to-ground for an equipped item).
		InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
		if (!itemComp)
			return;

		InventoryStorageSlot parentSlot = itemComp.GetParentSlot();
		if (!parentSlot)
			return;

		BaseInventoryStorageComponent storage = parentSlot.GetStorage();
		if (!storage)
			return;

		invMgr.TryRemoveItemFromStorage(item, storage);
	}

	protected void DCO_AgentGoProne(IEntity character)
	{
		if (!character || DCO_PlayerUtil.IsPlayer(character))
			return;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (cc)
			cc.ForceStance(ECharacterStance.PRONE);
	}

	// Freeze a genuinely-surrendered unit in its prone pose by deactivating its AI brain, so CRX's AI
	// can no longer issue stance/move orders that stand it back up (Fix 1). Re-asserts the final pose
	// first. Never called for fake-surrender ambushers (they stay AI-active to spring the grenade).
	protected void DCO_FreezeSurrenderedAgent(IEntity character)
	{
		if (!character || DCO_PlayerUtil.IsPlayer(character))
			return;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (!cc)
			return;

		cc.SetWeaponRaised(false);
		cc.SetWeaponNoFireTime(99999);
		cc.ForceStance(ECharacterStance.PRONE);

		AIControlComponent ai = cc.GetAIControlComponent();
		if (ai && ai.IsAIActivated())
			ai.DeactivateAI();
	}

	protected void DCO_MaintainSurrender()
	{
		if (!m_Owner)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		// Prisoner flee/lay-down: gather the captor positions once for the whole group (players + still-armed
		// enemies of this group's faction), so each surrendered member can find its nearest captor cheaply.
		array<vector> captors;
		if (cfg.m_bEnableSurrenderFlee)
		{
			captors = {};
			DCO_CollectCaptors(captors);
			if (!m_mDCO_SurrenderFleeSince)
				m_mDCO_SurrenderFleeSince = new map<IEntity, float>();
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
				continue;	// never pose/freeze a player

			// Sprung fakers: left entirely alone (mid / post grenade-throw) - re-posing would interrupt it.
			if (m_aDCO_FakeSprung && m_aDCO_FakeSprung.Contains(ent))
				continue;

			SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
			if (!cc)
				continue;

			// Waiting fakers (armed, not yet triggered): keep them posed as surrendered (prone, weapon
			// lowered) so they don't stand around visibly holding a grenade - but do not deactivate their AI
			// (they must stay active to spring the ambush). This fixes the "stands there with a grenade" look.
			if (m_aDCO_FakeSurrenderWatch && m_aDCO_FakeSurrenderWatch.Contains(ent))
			{
				cc.SetWeaponRaised(false);
				cc.SetWeaponNoFireTime(DCO_SURRENDER_NOFIRE_TIME);
				cc.ForceStance(ECharacterStance.PRONE);
				continue;
			}

			// Already frozen (AI deactivated) - it holds its pose on its own; don't poke it.
			AIControlComponent ai = cc.GetAIControlComponent();
			if (ai && !ai.IsAIActivated())
				continue;

			// Prisoner flee/lay-down (default): the member stays AI-active and runs from its nearest captor,
			// stopping + lying down empty-handed when one closes within m_fSurrenderStopRange. Drives its own
			// pose/movement each tick (no freeze).
			if (cfg.m_bEnableSurrenderFlee)
			{
				DCO_MaintainSurrenderFlee(agent, ent, cc, captors);
				continue;
			}

			// Legacy hold (flee disabled): keep it disarmed and prone so it can't pop back up between morale
			// ticks (Fix 1 belt-and-suspenders).
			cc.SetWeaponRaised(false);
			cc.SetWeaponNoFireTime(DCO_SURRENDER_NOFIRE_TIME);
			cc.ForceStance(ECharacterStance.PRONE);
		}
	}

	// One genuinely-surrendered member's prisoner behaviour. Always empty-handed + holding fire. If a captor
	// is within m_fSurrenderStopRange it stops and lies down (the surrender pose); if a captor is in the
	// awareness band beyond that it runs directly away; with no captor near it simply lies low (so it never
	// sprints off after nobody). Keeps the member AI-active - this replaces the freeze when flee mode is on.
	protected void DCO_MaintainSurrenderFlee(AIAgent agent, IEntity ent, SCR_CharacterControllerComponent cc, array<vector> captors)
	{
		cc.SetWeaponRaised(false);
		cc.SetWeaponNoFireTime(DCO_SURRENDER_NOFIRE_TIME);

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		vector pos = ent.GetOrigin();

		// Nearest captor to this member.
		bool hasCaptor = false;
		float nearestSq = 1e12;
		vector nearestPos;
		if (captors)
		{
			foreach (vector c : captors)
			{
				float d = vector.DistanceSq(c, pos);
				if (d < nearestSq)
				{
					nearestSq = d;
					nearestPos = c;
					hasCaptor = true;
				}
			}
		}

		float stop = cfg.m_fSurrenderStopRange;
		float aware = cfg.m_fSurrenderFleeAwareness;

		// Captor on top of them (<= stop), or nobody near enough to run from (> awareness / none): stop and lie
		// down. Cancels any active run order so the prone pose actually sticks.
		if ((hasCaptor && nearestSq <= stop * stop) || !hasCaptor || nearestSq > aware * aware)
		{
			DCO_SurrenderStopAndDrop(agent, ent);
			return;
		}

		// A captor is in the awareness band but not yet on top of them: run directly away. Re-issue the run
		// order only every DCO_SURRENDER_REFLEE_MS (timestamp throttle) so we don't spam moves every tick.
		BaseWorld world = GetGame().GetWorld();
		float lastFlee;
		if (world && m_mDCO_SurrenderFleeSince && m_mDCO_SurrenderFleeSince.Find(ent, lastFlee))
		{
			float nowMs = world.GetWorldTime();
			if ((nowMs - lastFlee) < DCO_SURRENDER_REFLEE_MS)
				return;	// still running from the last order - let it run
		}

		vector away = pos - nearestPos;
		away[1] = 0;
		if (away.LengthSq() < 0.01)
			away = ent.GetTransformAxis(2) * -1.0;	// captor exactly on top - just pick a direction
		away.Normalize();
		vector fleePos = pos + away * cfg.m_fFleeDistance;

		if (m_Mailbox)
		{
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, fleePos, EMovementType.RUN, false, null);
			if (msg)
			{
				msg.SetReceiver(agent);
				m_Mailbox.RequestBroadcast(msg, agent);
				if (world && m_mDCO_SurrenderFleeSince)
					m_mDCO_SurrenderFleeSince.Set(ent, world.GetWorldTime());
			}
		}
	}

	// Stop a fleeing prisoner and put it prone. If it was running, cancels the run with a single hold-at-self
	// order (issued once, on the run-to-stop transition) so the prone actually holds; then prone via the shared
	// stance util (which won't re-snap if it is already down).
	protected void DCO_SurrenderStopAndDrop(AIAgent agent, IEntity ent)
	{
		if (m_mDCO_SurrenderFleeSince && m_mDCO_SurrenderFleeSince.Contains(ent))
		{
			if (m_Mailbox)
			{
				SCR_AIMessage_Move hold = SCR_AIMessage_Move.Create(null, ent.GetOrigin(), EMovementType.WALK, false, null);
				if (hold)
				{
					hold.SetReceiver(agent);
					m_Mailbox.RequestBroadcast(hold, agent);
				}
			}
			m_mDCO_SurrenderFleeSince.Remove(ent);
		}

		DCO_StanceUtil.TrySetStance(ent, ECharacterStance.PRONE, DCO_MoraleSettings.Get().m_fStanceCooldownSec * 1000.0);
	}

	// Positions of everyone a surrendered member should treat as a captor: players and still-armed AI that are
	// enemies of this group's faction. Surrendered members (faction cleared to null) are naturally excluded.
	protected void DCO_CollectCaptors(array<vector> outPositions)
	{
		if (!m_Owner)
			return;

		SCR_Faction ownScr = SCR_Faction.Cast(m_Owner.GetFaction());

		// Players - filtered to hostiles when factions resolve; otherwise any player is a potential captor.
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm)
		{
			array<int> ids = {};
			pm.GetPlayers(ids);
			foreach (int id : ids)
			{
				IEntity p = pm.GetPlayerControlledEntity(id);
				if (!p)
					continue;
				if (ownScr && !DCO_IsCaptorHostile(ownScr, p))
					continue;
				outPositions.Insert(p.GetOrigin());
			}
		}

		// Enemy AI - needs a resolvable own-faction to judge hostility.
		if (!ownScr)
			return;
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		array<AIAgent> aiAgents = {};
		aiWorld.GetAIAgents(aiAgents);
		foreach (AIAgent a : aiAgents)
		{
			if (!a)
				continue;
			IEntity e = a.GetControlledEntity();
			if (!e)
				continue;
			if (DCO_IsCaptorHostile(ownScr, e))
				outPositions.Insert(e.GetOrigin());
		}
	}

	protected bool DCO_IsCaptorHostile(SCR_Faction ownScr, IEntity ent)
	{
		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(ent.FindComponent(FactionAffiliationComponent));
		if (!fac)
			return false;
		Faction other = fac.GetAffiliatedFaction();
		if (!other)
			return false;
		return ownScr.IsFactionEnemy(other);
	}

	protected bool DCO_GetThreatPosition(out vector outPos)
	{
		if (!m_Perception)
			return false;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return false;

		vector sum = vector.Zero;
		int n = 0;
		foreach (IEntity e : targets)
		{
			if (e)
			{
				sum += e.GetOrigin();
				n++;
			}
		}

		if (n == 0)
			return false;

		outPos = sum / n;
		m_vDCO_LastThreatPos = outPos;	// cache for a coherent flee / anti-funnel after perception goes quiet
		m_bDCO_HasLastThreat = true;
		return true;
	}

	// Public: current perceived-enemy centroid, or the last-known one if perception has gone quiet.
	// Used by flee (Fix 2) and the tactical-move anti-funnel logic (Fix 3, via the group utility).
	bool DCO_GetThreatOrLastPosition(out vector outPos)
	{
		if (DCO_GetThreatPosition(outPos))
			return true;

		if (m_bDCO_HasLastThreat)
		{
			outPos = m_vDCO_LastThreatPos;
			return true;
		}
		return false;
	}

	// Snapshot a genuinely-surrendering member's faction + held weapon so it can be recovered later.
	protected void DCO_CaptureSurrenderRecord(IEntity character)
	{
		if (!character)
			return;

		if (!m_aDCO_SurrenderRecords)
			m_aDCO_SurrenderRecords = {};

		DCO_SurrenderRecord rec = new DCO_SurrenderRecord();
		rec.m_Character = character;

		FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
		if (fac)
			rec.m_OriginalFaction = fac.GetAffiliatedFaction();

		BaseWeaponManagerComponent wm = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		if (wm)
		{
			WeaponSlotComponent slot = wm.GetCurrentSlot();
			if (slot)
			{
				IEntity weapon = slot.GetWeaponEntity();
				if (weapon)
				{
					rec.m_WeaponEntity = weapon;
					EntityPrefabData pd = weapon.GetPrefabData();
					if (pd)
						rec.m_WeaponResource = pd.GetPrefabName();
				}
			}
		}

		m_aDCO_SurrenderRecords.Insert(rec);
	}

	// Per-tick (throttled) recovery evaluation for a surrendered group: regain morale while safe, then
	// recover when morale has climbed back or the group has been left alone long enough. Never recovers
	// while an enemy is actively perceived. Called only when m_bEnableSurrenderRecovery + surrendered.
	protected void DCO_UpdateSurrenderRecovery(float now)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		if (m_fDCO_LastRecoveryCheck >= 0 && (now - m_fDCO_LastRecoveryCheck) < cfg.m_fRecoveryCheckSec * 1000.0)
			return;
		m_fDCO_LastRecoveryCheck = now;

		// Is an enemy currently perceived?
		bool hasContact = false;
		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			if (targets && !targets.IsEmpty())
				hasContact = true;
		}

		if (hasContact)
		{
			m_fDCO_LastSurrenderContact = now;
			return;	// never un-surrender with an enemy right there
		}

		// Safe: rebuild morale toward the recovery threshold.
		m_fDCO_Morale = Math.Clamp(m_fDCO_Morale + cfg.m_fRecoveryMoralePerTick, 0.0, DCO_MORALE_MAX);

		// Enforce the minimum surrender hold before any recovery is allowed.
		if (m_fDCO_SurrenderStartTime < 0 || (now - m_fDCO_SurrenderStartTime) < cfg.m_fRecoveryMinSurrenderSec * 1000.0)
			return;

		// Eligible if morale recovered or the group has been left unmolested long enough.
		bool moraleOk = m_fDCO_Morale >= cfg.m_fRecoveryMoraleThreshold;
		bool quietOk = (now - m_fDCO_LastSurrenderContact) >= cfg.m_fRecoveryNoContactSec * 1000.0;
		if (!moraleOk && !quietOk)
			return;

		// Stagger so a whole battlefield of surrendered groups doesn't pop up on the same tick.
		if (Math.RandomFloat01() > cfg.m_fRecoveryChancePerTick)
			return;

		DCO_RecoverFromSurrender();
	}

	// Recover the whole group: restore every captured member, then clear the group surrender state.
	protected void DCO_RecoverFromSurrender()
	{
		if (!m_Owner)
			return;

		DCO_Debug.LogGroup("RECOVERY", m_Owner.GetLeaderEntity(), "recovering from surrender (re-arm + rejoin)");

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		if (m_aDCO_SurrenderRecords)
		{
			foreach (DCO_SurrenderRecord rec : m_aDCO_SurrenderRecords)
			{
				if (rec && rec.m_Character)
					DCO_RecoverAgent(rec);
			}
			m_aDCO_SurrenderRecords.Clear();
		}

		m_bDCO_Surrendered = false;
		m_bDCO_Broken = false;
		m_fDCO_SurrenderStartTime = -1;
		if (m_mDCO_SurrenderFleeSince)
			m_mDCO_SurrenderFleeSince.Clear();
		// Float morale up so the group doesn't immediately collapse and re-surrender on the next tick.
		if (m_fDCO_Morale < cfg.m_fRecoveryMoraleThreshold)
			m_fDCO_Morale = cfg.m_fRecoveryMoraleThreshold;
	}

	// Restore one member: reactivate its AI, clear the held-fire, stand up, restore faction, re-arm.
	protected void DCO_RecoverAgent(DCO_SurrenderRecord rec)
	{
		IEntity character = rec.m_Character;
		if (!character)
			return;

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (cc)
		{
			AIControlComponent ai = cc.GetAIControlComponent();
			if (ai && !ai.IsAIActivated())
				ai.ActivateAI();	// undo the surrender freeze (Fix 1 inverse)
			cc.SetWeaponNoFireTime(0);
			cc.ForceStance(ECharacterStance.STAND);
		}

		if (rec.m_OriginalFaction)
		{
			FactionAffiliationComponent fac = FactionAffiliationComponent.Cast(character.FindComponent(FactionAffiliationComponent));
			if (fac)
				fac.SetAffiliatedFaction(rec.m_OriginalFaction);
		}

		// Undo the surrender perception flag (mirrors the SetDisarmed(true) in DCO_NeutralizeFaction) so the
		// re-armed unit reads as a combatant again - otherwise it recovers + re-arms but enemies keep ignoring it.
		PerceivableComponent perc = PerceivableComponent.Cast(character.FindComponent(PerceivableComponent));
		if (perc)
			perc.SetDisarmed(false);

		DCO_RearmAgent(character, rec);
	}

	// Re-arm a recovering member: re-pickup the dropped weapon if it still exists, else spawn a
	// replacement from its captured prefab; then resupply magazines (and optional throwables) once equipped.
	protected void DCO_RearmAgent(IEntity character, DCO_SurrenderRecord rec)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		InventoryStorageManagerComponent invMgr = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!invMgr)
			return;

		bool rearmed = false;

		// (a) re-pickup the originally dropped weapon if the entity is still alive.
		if (rec.m_WeaponEntity && !rec.m_WeaponEntity.IsDeleted())
			rearmed = invMgr.TryInsertItem(rec.m_WeaponEntity, EStoragePurpose.PURPOSE_WEAPON_PROXY);

		// (b) fallback: spawn a fresh weapon from the captured prefab into the weapon storage.
		if (!rearmed && cfg.m_bRecoverySpawnWeaponIfLost && rec.m_WeaponResource != string.Empty)
		{
			SCR_CharacterInventoryStorageComponent charInv = SCR_CharacterInventoryStorageComponent.Cast(character.FindComponent(SCR_CharacterInventoryStorageComponent));
			BaseInventoryStorageComponent wpnStorage;
			if (charInv)
				wpnStorage = charInv.GetWeaponStorage();
			rearmed = invMgr.TrySpawnPrefabToStorage(rec.m_WeaponResource, wpnStorage);
		}

		// (c) resupply ammo + optional throwables once the weapon is (re)equipped (slightly delayed so the
		//     insert/spawn callback has completed first).
		if (rearmed)
			GetGame().GetCallqueue().CallLater(DCO_ResupplyAfterRearm, 600, false, character);
	}

	// Resupply the recovered member's magazines (ammo) and any configured throwables.
	protected void DCO_ResupplyAfterRearm(IEntity character)
	{
		if (!character)
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		SCR_InventoryStorageManagerComponent scrMgr = SCR_InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!scrMgr)
			return;

		if (cfg.m_iRecoveryMagazineCount > 0)
			scrMgr.ResupplyMagazines(cfg.m_iRecoveryMagazineCount);

		// Optional throwables (grenades) - only if the server owner configured a prefab; no fabricated GUIDs.
		if (cfg.m_sRecoveryThrowablePrefab != string.Empty && cfg.m_iRecoveryThrowableCount > 0)
			scrMgr.TrySpawnPrefabToStorage(cfg.m_sRecoveryThrowablePrefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, cfg.m_iRecoveryThrowableCount);
	}
}
