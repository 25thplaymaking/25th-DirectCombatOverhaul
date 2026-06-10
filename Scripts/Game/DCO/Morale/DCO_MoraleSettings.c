// Global morale settings store (singleton). Runtime store for all DCO morale/surrender tunables. A
// lazily-created singleton, so the settings exist in every Game Master mode with no per-scenario
// setup. Defaults are baked in here.
//
// Tuning sources (all optional, all write into this store):
//   - Game Master "25th DCO" attributes (DCO_*EditorAttribute) - live, in any GM scenario.
//   - DCO_MoraleSettingsComponent - optional designer override placed on a scenario entity.
// If nothing overrides them, these defaults are used.
class DCO_MoraleSettings
{
	protected static ref DCO_MoraleSettings s_Instance;

	bool	m_bEnabled				= true;
	// Unified DCO debug logging master switch (DCO_Debug.Log). When on, DCO systems print tagged diagnostic
	// lines to console.log ([DCO:SURRENDER] ... etc) so you can see morale values + why groups do/don't
	// surrender/flee/recover. Verbose - for tuning/testing only. Default OFF (near-zero cost when off).
	bool	m_bDebug				= false;
	float	m_fUpdateIntervalSec	= 2.0;
	float	m_fFleeThreshold		= 25.0;
	float	m_fSurrenderThreshold	= 8.0;	// morale at/below which a group becomes eligible to surrender. 0 = never surrender.
	float	m_fRallyThreshold		= 55.0;
	float	m_fRecoveryPerTick		= 7.0;
	float	m_fDrainPerTick			= 9.0;
	float	m_fCasualtyWeight		= 45.0;
	float	m_fHeavyThreat			= 0.5;

	// Anti-instant-surrender: a group must have lost this fraction of its strength before it is even
	// allowed to surrender (stops full-strength frontline units giving up just because an enemy
	// approached). Once eligible, surrender/flee happen by chance per tick, not deterministically.
	float	m_fMinCasualtyFractionForSurrender	= 0.30;	// 30% losses required to surrender
	float	m_fSurrenderChancePerTick			= 0.25;	// per-tick chance to surrender once eligible
	float	m_fFleeChancePerTick				= 0.40;	// per-tick chance to break/flee once below flee threshold

	// Surrender context gates: low morale is necessary but not sufficient to surrender. These add
	// the "don't surrender mid-firefight / don't drop in unison" behaviour scenario designers expect.
	float	m_fSurrenderLullSec			= 12.0;	// seconds the group must be out of combat (no perceived enemy and not under threat) before it may surrender. Higher = AI keep fighting longer before giving up; lower = give up sooner after contact eases; 0 = no lull (may surrender mid-fight, old behaviour).
	int		m_iSurrenderMinSquadToFight	= 3;	// while the group still has at least this many living members it keeps fighting and will not surrender. Higher = only badly-depleted squads surrender; lower = even larger squads may surrender; 0 = ignore squad strength entirely.
	float	m_fSurrenderStaggerMaxSec	= 0.8;	// when a group does surrender, each member begins at a random delay up to this many seconds, so they don't all drop weapons on the exact same instant. Kept small (was 1.5) so the squad surrenders together (cohesion) rather than raggedly; 0 = all simultaneous.
	float	m_fMoraleLostPerCasualty	= 12.0;	// morale points lost the instant a friendly in the group is killed (on top of the gradual drain), so each loss is felt. Higher = casualties crush morale faster; 0 = disable the per-casualty hit (gradual drain only).

	// Unit cohesion / leadership: morale is a collective property - a squad holds together and
	// breaks as a whole, not man-by-man. A group is never truly "leaderless" (the engine promotes a new leader
	// on death), so the meaningful leadership signal is losing the leader under fire: the squad takes a
	// one-time cohesion hit as it reorganizes (a decapitation shock). Detected as a leader change coinciding
	// with a fresh casualty. 0 = disable.
	float	m_fMoraleLostOnLeaderLoss	= 15.0;	// one-time morale hit (0-100 scale) when the group's leader is killed under fire. Higher = decapitation hurts cohesion more; 0 = off.

	// Fake surrender (immersive ambush): a fraction of "surrendering" units actually keep a grenade,
	// play surrendered (weapon lowered, prone), and drop it primed at their feet when an enemy closes.
	bool	m_bEnableFakeSurrender			= true;
	float	m_fFakeSurrenderChance			= 0.25;	// chance a surrendering unit is selected to fake it (keeps a grenade) instead of genuinely surrendering
	float	m_fFakeSurrenderTriggerRange	= 6.0;	// metres - an enemy this close arms the trigger
	float	m_fFakeSurrenderDropChance		= 0.25;	// once a player is within trigger range, the per-encounter chance the faker actually primes + throws (separate from the selection chance above). 1.0 = always spring.
	float	m_fFakeSurrenderThrowSpeed		= 0.05;	// throw speed scale; near-zero = drops right in front
	string	m_sFakeSurrenderGrenadePrefab	= "{6D1AF5BE65D92CF6}Prefabs/Weapons/Grenades/Grenade_RGD5.et";	// frag grenade given to a faker that has no grenade of its own, so the ambush can still spring (RGD5 shipped in 25thCRX). Empty = don't spawn one. Spawned into inventory via TrySpawnPrefabToStorage.

	// Composition defense + base morale (DCO_GroupComposition / DCO_CompositionRegistry, folder Outpost).
	// Uncommitted own-faction groups near a GM-placed composition (checkpoint/bunker/works) reposition onto
	// it, hold + man its assets (via the existing Defend/Garrison/AssetUse layers), and gain a morale floor
	// that scales with how many friendly compositions are nearby. GM/scenario orders always take precedence.
	bool	m_bEnableCompositionDefense	= false;	// master toggle (default OFF)
	float	m_fCompositionRadius		= 120.0;	// metres - awareness/reposition radius around the group; also the morale-count radius. Keep "local" (no map-wide seeking).
	float	m_fCompositionScanSec		= 5.0;		// how often the shared registry rescans the world for placed compositions (one scan feeds all groups)
	float	m_fCompositionCheckSec		= 5.0;		// per-group throttle for the reposition/latch evaluation
	bool	m_bCompositionReposition	= true;		// true = move onto the nearest composition; false = only adopt the defensive behaviour where the group already is
	float	m_fCompositionHoldLeash		= 15.0;		// metres - don't re-issue a move if already this close to the anchor (reposition deadzone)
	float	m_fCompositionMoralePerUnit	= 5.0;		// morale-floor resolve (0-100 scale) added per nearby friendly composition
	float	m_fCompositionMoraleCap		= 20.0;		// max total composition morale-floor resolve (0-100 scale)

	// AI stance-transition cooldown (anti-jitter): stops AI rapidly cycling prone/crouch/stand on the
	// spot. Implemented in DCO_StanceCooldown.c by throttling only pure stance-change combat-move
	// requests (SCR_AICombatMoveRequest_ChangeStance / _ChangeStanceInCover); move requests are never
	// throttled, so moving / taking cover is never penalised. Players are never affected (only AI own a
	// SCR_AICombatMoveState). Off by default - opt-in via the "25th DCO" GM settings so it cannot affect
	// a live server until enabled.
	bool	m_bEnableStanceCooldown		= false;
	float	m_fStanceCooldownSec		= 3.0;	// min seconds between AI in-place stance changes
	// By default only free-standing prone/crouch/stand cycling is throttled; the rapid in-cover peek/duck
	// is left alone (throttling it jittered torso gear physics - see MODPLAN gear-shudder fix). If AI still
	// drop prone too eagerly/often, turn this on to also throttle the in-cover stance requests. Default OFF.
	bool	m_bStanceCooldownInCover	= false;
	// Diagnostic: log every AI stance request that reaches the cooldown hook (type / reason / target
	// stance / moving / would-be-throttled), to identify the exact "matrix-dodge" snap-to-prone path in a
	// server log. Default OFF (noisy). Tunable via JSON only (investigation aid, no GM slider needed).
	bool	m_bStanceCooldownDebug		= false;
	float	m_fFleeDistance			= 120.0;
	bool	m_bEnableSurrender		= true;
	bool	m_bDropGearOnSurrender	= true;
	int		m_iSurrenderDropDelayMs	= 1500;
	int		m_iSurrenderProneDelayMs = 3000;

	// Surrender hold: once a unit is genuinely surrendered and posed prone, deactivate its AI
	// brain so it freezes in the surrender pose instead of CRX's AI standing it back up / wandering.
	// Done via AIControlComponent.DeactivateAI() shortly after the prone beat. Fake-surrender ambushers
	// are never frozen (they must stay active to spring the grenade). Internal knob (no GM slider).
	bool	m_bFreezeSurrenderedAI		= true;
	int		m_iSurrenderFreezeDelayMs	= 1000;	// extra delay after the prone beat before freezing

	// Surrender flee / lay-down (prisoner behaviour): instead of freezing in place, a genuinely-surrendered
	// unit runs from its nearest captor while none is close, and stops + lies down (hands up, prone, empty
	// handed) the moment a captor comes within m_fSurrenderStopRange. More lifelike than dropping where they
	// stood. When on it replaces the freeze (the unit stays AI-active so it can move); set it off to restore
	// the old freeze-in-place hold. Fake-surrender ambushers are never affected (they hold for the grenade).
	//   m_fSurrenderStopRange     : captor within this many metres: stop and lay down.
	//   m_fSurrenderFleeAwareness : only bother fleeing from a captor within this many metres; with none that
	//                               near, the prisoner just lies low instead of running off the map.
	bool	m_bEnableSurrenderFlee		= true;
	float	m_fSurrenderStopRange		= 20.0;
	float	m_fSurrenderFleeAwareness	= 150.0;

	// Surrender voiceline framework: when a unit genuinely surrenders, play an authored sound
	// event ("hands up!" / "don't shoot!" etc). Default OFF + empty event so nothing fires until a
	// designer authors a soundproject (.acp) in Workbench and sets the event name here (via the
	// DCO_MoraleSettingsComponent EditBox or the GM toggle). m_iSurrenderVoiceStaggerMs spaces members
	// so a whole squad doesn't shout in unison. See DCO_SurrenderVoice.c for the play path + MP caveat.
	bool	m_bEnableSurrenderVoice		= false;
	string	m_sSurrenderSoundEvent		= "";	// e.g. "SOUND_DCO_SURRENDER" - authored event name
	int		m_iSurrenderVoiceStaggerMs	= 350;	// per-member spacing so calls don't overlap

	// Last stand / no-surrender: when on, groups never surrender - they fight on
	// (and still flee/break). The counterweight to surrender for elite/fanatical forces. Global toggle
	// for now; per-faction/rank gating is a future refinement. Default OFF (surrender allowed).
	bool	m_bEnableLastStand			= false;

	// Panic / shell-shock: a morale band just above the flee threshold where a group briefly
	// loses its nerve before it actually breaks - members cower (crouch) and hold fire for a moment, then
	// the continued morale drop tips them into the flee branch. Brief, throttled, default OFF. Triggers
	// when m_fFleeThreshold < morale <= m_fFleeThreshold + m_fPanicBand (and the chance rolls).
	bool	m_bEnablePanic			= false;
	float	m_fPanicBand			= 15.0;	// morale points above the flee threshold that count as "panic"
	float	m_fPanicChancePerTick	= 0.5;	// per-tick chance to panic while in the band
	float	m_fPanicDurationSec		= 4.0;	// how long the cower/hold-fire lasts
	float	m_fPanicCooldownSec		= 12.0;	// min seconds between panic episodes for a group

	// Morale to accuracy: tie combat power to morale. As group morale falls the members'
	// AI skill is stepped down (worse aim / slower reactions) and recovers when morale does, via
	// SCR_AICombatComponent.SetAISkill(EAISkill.ROOKIE/REGULAR) + ResetAISkill() (default restored). Only
	// re-applied when the band changes. Server-side, AI-only, default OFF.
	bool	m_bEnableMoraleAccuracy	= false;
	float	m_fAccuracyRookieMorale	= 20.0;	// at/below this morale: ROOKIE skill (worst aim)
	float	m_fAccuracyRegularMorale = 45.0;	// at/below this morale: REGULAR skill; above: default
	float	m_fLowMoraleFireRateCoef = 0.7;	// fire-rate coefficient applied in the ROOKIE band (ragged fire)

	// Progressive aim zeroing (parity feature "Aim correction", done better): a freshly-acquired target starts
	// a shooter's aim cold and it warms up to the unit's own trained skill over dwell time (ROOKIE to REGULAR to
	// default via ResetAISkill) - it never pushes above the unit's configured skill, so it can't make
	// super-soldiers that beat the server's chosen difficulty. The zero resets when all targets are lost for
	// m_fZeroResetSec or the primary target switches. Crucially it only warms up while morale is healthy
	// (accuracy band 0): a suppressed/panicking group never settles its aim (our edge over a flat ramp). Defers
	// the skill lever to m_bEnableMoraleAccuracy while the band is degraded. Server-side, AI-only on-foot,
	// default OFF. See DCO_MarksmanZeroing.c. Uses only EAISkill.ROOKIE/REGULAR/ResetAISkill (proven members).
	bool	m_bEnableAimZeroing		= false;
	float	m_fZeroStep1Sec			= 4.0;	// dwell (s) on the same target before aim warms from ROOKIE to REGULAR
	float	m_fZeroStep2Sec			= 9.0;	// dwell (s) before it reaches the unit's trained default (fully zeroed)
	float	m_fZeroResetSec			= 3.0;	// all targets lost (LOS broken) for this long: zero resets to cold
	float	m_fZeroCheckSec			= 1.0;	// how often zeroing is evaluated
	bool	m_bZeroBoostPerception	= true;	// also raise perception (faster reacquire/tracking) while zeroed in
	float	m_fZeroBasePerception	= 1.0;	// perception factor restored on reset (1.0 = engine default baseline)
	float	m_fZeroMaxPerception	= 1.35;	// perception factor at full zero (detection lever, may exceed 1.0)

	// Vehicle armour angling (survivability): when an AI vehicle crew is in contact and the vehicle is
	// presenting a weak side/rear to the nearest enemy (the threat is outside the frontal arc), order the
	// crew's group to turn its front toward that enemy (strongest armour presented), like a player angling.
	// Reforger exposes no in-place hull-rotate for AI, so this drives the front toward the threat via
	// the move-order system and cancels once the front is presented - so the vehicle also noses/advances a
	// little toward the enemy (validate/tune in-engine). Server-side, default OFF.
	bool	m_bEnableVehicleArmor		= false;
	float	m_fVehicleFrontalArcDeg		= 50.0;	// threat within +/- this of the hull front = "front presented" (no reorient)
	float	m_fVehicleReorientDeadbandDeg = 25.0;	// must be this far beyond the arc before a reorient starts (anti-jitter hysteresis)
	float	m_fVehicleReorderDist		= 20.0;	// only re-issue the face order if the enemy moved at least this far (m) since the last
	float	m_fVehicleMinEngageRange	= 30.0;	// don't reorient if the enemy is closer than this (m)
	float	m_fVehicleCheckIntervalSec	= 2.5;	// how often the angling is evaluated (also bounds how far it noses forward)

	// Night illumination: when a group is in contact in low light, pop an illumination flare
	// over the enemy via the native illum-flare behaviour. Gated by the engine's own
	// SCR_AIGroupInfoComponent.IsIllumFlareAllowed() (which checks light level + a per-group cooldown), so
	// it only fires at night and never spams. Server-side, default OFF.
	bool	m_bEnableNightIllum			= false;
	float	m_fIllumCheckIntervalSec	= 15.0;	// how often a contacted group considers popping a flare

	// Vision / night limiting (parity feature "Vision Limiting", done better): AI detect more slowly in the
	// dark. A group that is still searching (no perceived target) gets its members' perception factor scaled
	// down by darkness (time-of-day twilight ramp); the instant it is in contact the penalty lifts (muzzle
	// flashes give the enemy away) and perception is handed back to the combat/zeroing layer. Server-side;
	// throttled; default OFF. See DCO_VisionLimit.c.
	bool	m_bEnableVisionLimit		= false;
	float	m_fNightPerceptionFactor	= 0.5;	// perception multiplier at full dark (1.0 = no penalty)
	float	m_fVisionSunriseHour		= 6.0;	// hour (0..24) full daylight begins
	float	m_fVisionSunsetHour			= 19.0;	// hour (0..24) daylight ends (dusk begins)
	float	m_fVisionTwilightHours		= 1.5;	// twilight ramp width (hours) on each side of sunrise/sunset
	float	m_fVisionCheckSec			= 10.0;	// how often vision limiting is re-evaluated per group

	// Idle behaviour (parity feature "IDLE behaviour"): a group with no enemy, no waypoint, and a stationary
	// leader for m_fIdleDelaySec spreads out (once) and then patrols within m_fIdlePatrolRadius of where it
	// settled. Static-gun manning is handled separately (DCO_AssetUse.c). Deferential - never overrides a real
	// task. Server-side, on-foot, default OFF. See DCO_GroupIdle.c.
	bool	m_bEnableIdleBehaviour		= false;
	float	m_fIdleDelaySec				= 60.0;		// stationary, taskless time before a group is considered idle
	float	m_fIdleMoveEpsilon			= 3.0;		// leader movement (m) that resets the stationary timer
	float	m_fIdleSpreadRadius			= 10.0;		// ring radius (m) members disperse to on entering idle
	float	m_fIdlePatrolRadius			= 150.0;	// patrol points stay within this (m) of the idle anchor
	float	m_fIdlePatrolIntervalSec	= 45.0;		// seconds between patrol rolls
	float	m_fIdlePatrolChance			= 0.5;		// chance (0..1) a patrol roll actually moves the group

	// Adjust-to-cover stance (parity feature "Adjust to cover"): while in contact, fit each member's stance to
	// the cover between it and the nearest threat - it adopts the lowest stance whose body line is blocked
	// (tall cover = stand, low wall = crouch, mound = prone). Throttled; cooperates with DCO_StanceCooldown.
	// Server-side, on-foot, default OFF. See DCO_CoverStance.c. Validate in-engine (may briefly root a mover).
	bool	m_bEnableCoverStance		= false;
	float	m_fCoverStanceCheckSec		= 4.0;		// how often stance fitting runs per group
	float	m_fCoverStanceMaxRange		= 120.0;	// only fit stance for members within this (m) of the threat
	float	m_fCoverStandEye			= 1.5;		// standing eye/body height (m) for the LOS probe (also the threat eye)
	float	m_fCoverCrouchEye			= 0.9;		// crouched body height (m)
	float	m_fCoverProneEye			= 0.4;		// prone body height (m)

	// Vocal info sharing (parity feature "Vocal Info sharing"): a group that perceives an enemy "shouts" the
	// contact to friendly groups within shouting range, seeding their perception (awareness only, no movement).
	// Server-side; throttled; default OFF. See DCO_VocalShare.c.
	bool	m_bEnableVocalShare			= false;
	float	m_fVocalRadius				= 60.0;		// shouting range (m) to nearby friendly groups
	float	m_fVocalCheckSec			= 4.0;		// how often a group calls out its contact
	int		m_iVocalMaxListeners		= 4;		// max friendly groups seeded per call-out

	// Radio info sharing (parity feature "Radio Info sharing"): the long-range sibling - squad leaders radio
	// the contact to friendly groups out to a much larger radius. Optionally flags listeners
	// reinforcement-eligible so the reinforcement layer (if enabled) lets them converge to support. Awareness
	// only by default. Server-side; throttled; default OFF. See DCO_RadioShare.c.
	bool	m_bEnableRadioShare			= false;
	float	m_fRadioRadius				= 400.0;	// radio range (m) to friendly groups
	float	m_fRadioCheckSec			= 6.0;		// how often a group radios its contact
	int		m_iRadioMaxListeners		= 8;		// max friendly groups seeded per radio call
	bool	m_bRadioFlagsReinforcement	= false;	// also flag listeners reinforcement-eligible (needs reinforcement enabled)

	// Per-member morale modifier (parity feature "Individual morale"): a thin per-soldier layer over the
	// group-level morale. Each member's personal morale = group morale - their own suppression stress; the
	// most personally-shaken members hesitate to fire (and optionally drop prone) while steadier squadmates
	// keep fighting - individual variance without discarding the cohesion model. Server-side, on-foot,
	// default OFF. See DCO_MemberMorale.c.
	bool	m_bEnableMemberMorale			= false;
	float	m_fMemberMoraleCheckSec			= 2.0;	// how often per-member morale is evaluated
	float	m_fMemberMoraleSuppressionWeight = 50.0;	// how strongly personal suppression subtracts from morale
	float	m_fMemberMoraleHesitateThreshold = 50.0;	// personal morale at/below this: the soldier hesitates / heads down
	float	m_fMemberMoraleHesitateSec		= 2.5;	// weapon-hold applied to a shaken soldier (must exceed the check interval)
	bool	m_bMemberMoraleCower			= false;	// also drop the worst-shaken prone (shares the stance lever)
	float	m_fMemberMoraleCowerThreshold	= 20.0;	// personal morale at/below this: prone cower (if enabled)

	// Emergency rearm (parity feature "Emergency rearm"): an in-contact AI that runs dry tops up its magazines
	// from any available supply, and (opt-in) scavenges the nearest dropped weapon on the ground. Generalises
	// the surrender-recovery rearm to all AI. Server-side, on-foot, default OFF. See DCO_EmergencyRearm.c.
	bool	m_bEnableEmergencyRearm		= false;
	float	m_fRearmCheckSec			= 5.0;		// how often members are checked for being out of ammo
	int		m_iRearmMagazineCount		= 3;		// magazines to top a dry weapon up to
	bool	m_bRearmScavenge			= false;	// also walk to + pick up the nearest dropped ground weapon
	float	m_fRearmScavengeRange		= 30.0;		// search radius (m) for a dropped weapon
	float	m_fRearmPickupDist			= 2.0;		// within this (m) of the weapon = pick it up (else move to it)

	// Vehicle combat positioning (parity feature "Basic vehicle behaviour"): a crewed AI vehicle with no line
	// of sight to a perceived enemy moves to a ring-sampled spot that has LOS (to engage); under heavy threat
	// (if back-off enabled) it instead moves to a spot that breaks LOS (hull-down). Complements the armour
	// angling. Server-side; throttled; re-orders only on a meaningful change. Default OFF. See DCO_VehicleCombat.c.
	bool	m_bEnableVehicleCombat		= false;
	float	m_fVehicleCombatCheckSec	= 3.0;		// how often a vehicle crew re-evaluates its firing position
	float	m_fVehicleSightHeight		= 2.0;		// height (m) of the LOS ray (turret/optic line)
	float	m_fVehicleRepositionRadius	= 35.0;		// ring radius (m) sampled for a better position
	int		m_iVehicleSampleCount		= 8;		// candidate directions sampled around the vehicle
	bool	m_bVehicleBackoff			= true;		// when heavily threatened, pull back to break LOS (hull-down)
	float	m_fVehicleBackoffThreat		= 0.6;		// group threat measure at/above which back-off engages
	float	m_fVehicleBackoffHoldSec	= 6.0;		// max seconds a vehicle hides in cover before peeking back out to fight (anti-freeze)

	// CQB / building assault (parity features "CQB" + "Building clearance"): we ID buildings (Building cast /
	// SCR_DestructibleBuildingComponent - the competitor's stated blocker) and, when a close enemy is sheltering
	// in one, push the group into the building to close on them instead of trading shots at the wall. Foundation
	// for full room-by-room clearance. Server-side, on-foot, default OFF. See DCO_CQB.c.
	bool	m_bEnableCQB				= false;
	float	m_fCqbCheckSec				= 2.5;		// how often the CQB push is evaluated
	float	m_fCqbEngageRange			= 60.0;		// only assault when the enemy is within this (m) - CQB range
	float	m_fCqbBuildingScan			= 12.0;		// an enemy within this (m) of a building counts as "inside" it
	float	m_fCqbArriveDist			= 6.0;		// within this (m) of the enemy = inside/clearing (stop re-pushing)
	float	m_fCqbReorderDist			= 8.0;		// re-issue the assault move when the enemy moves at least this (m)

	// Artillery fire missions (parity feature "Artillery Request"): an AI mortar crew that perceives a worthwhile
	// enemy cluster calls a fire mission on its centroid (SCR_AIMessage_ArtillerySupport). Cooldown + min-cluster
	// gate stand in for commander approval. Server-side, default OFF. See DCO_Artillery.c.
	bool	m_bEnableArtillery			= false;
	float	m_fArtyCheckSec				= 5.0;		// how often a mortar crew considers a fire mission
	int		m_iArtyMinTargets			= 3;		// minimum perceived enemies in the cluster to justify a mission
	float	m_fArtyMinRange				= 80.0;		// never shell a target closer than this (m) to the crew
	float	m_fArtyCooldownSec			= 60.0;		// minimum seconds between fire missions per battery (approval throttle)
	float	m_fArtyPriority				= 1.0;		// priority level on the artillery-support message

	// Lean AI commander (parity feature "High Command" - toggleable, no game mode): a global server-side
	// coordinator that finds each faction's hottest contact and vectors the nearest uncommitted reserves to it,
	// reusing the reinforcement primitives. Battlefield-wide where per-group reinforcement is local. Default OFF.
	// See DCO_Commander.c.
	bool	m_bEnableCommander			= false;
	float	m_fCommanderIntervalSec		= 15.0;		// how often the commander re-assesses the battlefield
	float	m_fCommanderThreatTrigger	= 0.4;		// a contact must reach this threat measure to pull reserves
	float	m_fCommanderDispatchRange	= 800.0;	// max distance (m) a reserve will be vectored from
	int		m_iCommanderMaxDispatch		= 2;		// reserves committed to the hot fight per assessment
	float	m_fCommanderReissueSec		= 30.0;		// minimum seconds before the same reserve is re-vectored

	// AI commander - objective system (extends the lean commander).
	bool	m_bCmdVisualize				= false;	// draw the objective plan in GM (listen-host/SP only)
	float	m_fCmdBaseRadius			= 150.0;	// DEFEND objective radius around an own composition
	int		m_iCmdUndermannedBelow		= 2;		// (reserved) base undermanned below this many friendly groups
	float	m_fCmdMergeDist				= 120.0;	// an auto objective within this of a GM objective is superseded
	int		m_iCmdGroupsPerObjective	= 1;		// reserves committed per urgent objective per tick
	float	m_fCmdActThreshold			= 1.0;		// minimum score to commit reserves to an objective
	float	m_fCmdWeightContact			= 100.0;	// SUPPORT/fight objective base urgency
	float	m_fCmdWeightDefend			= 30.0;
	float	m_fCmdWeightAttack			= 20.0;
	float	m_fCmdWeightEnemy			= 25.0;		// per enemy group near the objective
	float	m_fCmdWeightDeficit			= 40.0;		// per friendly-deficit unit near the objective

	// Map intelligence (RECOM merge): key-location awareness for the commander - villages and
	// military sites become hold/attack objectives so the AI fights over the map's towns, not
	// just its bases. See DCO/Commander/Area/. Built once per session, then cached.
	bool	m_bEnableCommanderAreas		= false;	// master toggle for map-area objectives
	int		m_iCommanderAreaMode		= 0;		// 0 AUTO (baked file else native), 1 EXTERNAL-only, 2 NATIVE-only, 3 OFF
	float	m_fAreaCellSize				= 150.0;	// native scan grid cell (m); buildings clustered by cell
	int		m_iAreaMinBuildings			= 6;		// a cell needs this many buildings to count as built-up
	int		m_iAreaSignificantWeight	= 15;		// a village needs this many buildings to earn an objective (military always counts)
	float	m_fAreaMaxRadius			= 400.0;	// cap on a derived area's radius (m)
	int		m_iAreaCellsPerStep			= 64;		// native scan: grid cells processed per frame (lower = gentler)
	// Prefab-name classification (supplements the component/descriptor detection). A single
	// substring matched against each entity's prefab name. Military supplement is off by default;
	// forest needs m_bAreaIncludeForest. Tune the substring per map (validate via m_bDebug).
	string	m_sAreaMilitaryPrefabs		= "";				// extra "this prefab = military" substring ("" = off)
	bool	m_bAreaIncludeForest		= false;			// scan + cluster forests natively (heavier; trees must be prefab entities)
	string	m_sAreaForestPrefabs		= "Vegetation/Trees";	// "this prefab = tree" substring for the forest pass
	int		m_iAreaMinForest			= 12;				// forest entities a cell needs to count as wooded
	// High-ground bias (RECOM topography, wired to behaviour): area objectives on terrain that
	// sits above its surroundings get a small priority bonus, so the AI prefers the high ground.
	bool	m_bAreaHighGroundBias		= true;				// add a priority bonus for elevated areas
	float	m_fAreaHighGroundWeight		= 0.4;				// bonus per metre of prominence (capped at +25)
	// Offline bake (JSON-only): when true, the native scan writes its areas to
	// $profile:DCO_MapIntel.json once it finishes, then you can turn this back off and switch to
	// the EXTERNAL tier. Replaces the old GM bake button (kept out of GM on purpose).
	bool	m_bDumpMapIntelOnLoad		= false;			// dump native areas to file after the scan, then set false

	// External AI commander (RECOM/ReforgerCommander bridge): a server-side loop posts the battlefield to an
	// external provider (RECOM backend / OpenAI proxy) and applies the AI's main-effort order into the objective
	// system. Endpoint + API key come from the server JSON only (never hardcoded/committed). Default OFF.
	bool	m_bEnableExternalCommander	= false;	// master toggle for the external AI commander
	string	m_sExternalEndpoint			= "";		// base URL of the provider (e.g. "http://127.0.0.1:8080")
	string	m_sExternalEvalPath			= "/commander/evaluate";	// POST path that returns the order JSON
	string	m_sExternalApiKey			= "";		// bearer/API key - set via $profile JSON only
	string	m_sExternalFaction			= "USSR";	// faction key the external AI commands
	float	m_fExternalIntervalSec		= 20.0;		// how often the battlefield is sent for evaluation

	// Provider style (bring-your-own model): 0 RAW (custom proxy / RECOM backend), 1 OPENAI-compatible
	// (OpenAI, DeepSeek, OpenRouter, Groq, local LM Studio/Ollama), 2 ANTHROPIC (Claude Messages API). For
	// OPENAI/ANTHROPIC set external_endpoint to the provider base URL and external_eval_path to its chat path
	// (/v1/chat/completions; /api/v1/chat/completions for OpenRouter; /v1/messages for Anthropic). See
	// docs/RECOM_DCO_MERGE_2026-06-09.md for the per-provider table.
	int		m_iExternalProvider			= 0;		// 0 RAW, 1 OPENAI-compatible, 2 ANTHROPIC
	string	m_sExternalModel			= "";		// model id (gpt-4o, claude-opus-4-8, deepseek-chat, anthropic/claude-3.5-sonnet, ...)
	int		m_iExternalMaxTokens		= 1024;		// max response tokens for the model
	string	m_sExternalSystemPrompt		= "";		// optional override; empty = the baked commander instruction

	// Vehicle doctrine (convoy / section combat): out of combat, vehicles use vanilla navmesh; on contact a
	// server-side controller clusters nearby friendly vehicle crews into a section and assigns each a doctrinal
	// role (lead / overwatch / escort / trail / dismount / recover / destroy). Movement is always a navmesh move
	// order; concealment is only a tiebreaker among safe spots. Default OFF. See DCO/Convoy/.
	bool	m_bEnableVehicleDoctrine	= false;	// master toggle
	float	m_fVehDoctrineSectionRadius	= 120.0;	// metres - friendly vehicles within this of each other form a section
	float	m_fVehDoctrineCheckSec		= 3.0;		// controller re-assessment interval (s); also the executor drill throttle
	float	m_fVehDoctrineEnterThreat	= 0.4;		// section enters combat when a member's threat measure reaches this and it perceives an enemy
	float	m_fVehDoctrineHandbackSec	= 12.0;		// no contact in the section for this long: full vanilla handback
	float	m_fVehDoctrineReorderDist	= 15.0;		// per-crew: don't re-issue a move unless the target moved at least this far (m) - anti-thrash
	float	m_fVehDoctrineRoadBlockDist	= 25.0;		// max distance (m) to the road used for herringbone shoulder lookup
	bool	m_bVehDoctrineBounding		= true;		// allow bounding overwatch (one vehicle moves while another covers)
	float	m_fVehDoctrineBoundDist		= 60.0;		// max length (m) of a single bound
	bool	m_bVehDoctrineDismountStuck	= true;		// transports debus passengers to a base of fire when stuck (road blocked / disabled)
	bool	m_bVehDoctrineRecovery		= true;		// handle disabled friendly vehicles (crew abandons / joins base of fire)
	float	m_fVehDoctrineDisabledSec	= 6.0;		// a crew ordered to move that hasn't moved > epsilon for this long is treated as DISABLED/stuck
	float	m_fVehDoctrineDisabledEps	= 3.0;		// movement (m) below which a vehicle counts as "not moving" for the disabled watchdog
	bool	m_bDebugVehDoctrine			= false;	// draw section links, roles, and target points (Shape API)

	// CQB town-clearing (methodical building clearing + house-to-house sweep, on-foot). When on, applicable
	// groups clear buildings in their detection radius as they move to their ordered target, chaining nearest-
	// first until the area is clear, then resume. Default OFF. See DCO/CQB/DCO_CqbClear.c.
	bool	m_bEnableCqbClear		= false;	// master toggle
	bool	m_bCqbClearMarkedOnly	= false;	// false = every on-foot group clears; true = only groups flagged CQB Clearer
	float	m_fCqbClearRadius		= 80.0;		// metres - buildings within this of the leader are swept (radius travels with the group)
	float	m_fCqbNodeDwellSec		= 3.0;		// seconds a member holds + scans each interior node before advancing
	float	m_fCqbStackSpacing		= 1.5;		// metres between stacked members at the breach point
	int		m_iCqbMaxBuildings		= 6;		// max buildings attempted per sweep cycle (cost bound)
	float	m_fCqbClearCheckSec		= 2.0;		// per-group state-machine throttle (s)
	float	m_fCqbReorderDistClear	= 4.0;		// per-member: don't re-issue a move unless the target moved at least this far (m)
	float	m_fCqbApproachTimeoutSec	= 30.0;		// anti-hang: give up on a building if the squad can't reach its breach within this (s); mark it done, move on
	bool	m_bDebugCqbClear		= false;	// draw building states + stack/node markers

	// Building garrison (parity feature "Building Occupying"): a defender group (DCO_IsDefender) near a building
	// splits its members onto a navmesh ring around it (holding the perimeter), then the Defend layer faces them
	// at the threat. Server-side, on-foot, default OFF. See DCO_Garrison.c.
	bool	m_bEnableGarrison			= false;
	float	m_fGarrisonCheckSec			= 8.0;		// how often a defender re-evaluates garrisoning
	float	m_fGarrisonRange			= 30.0;		// occupy a building within this (m) of the leader
	float	m_fGarrisonRingRadius		= 5.0;		// spread radius (m) of the member positions around the building
	// When on, the garrison sends members onto the building's baked AI firing/cover positions (window/corner
	// smart-action points) when any are found, falling back to the geometric ring otherwise. Off: always the
	// ring. Found via QueryEntitiesBySphere + AISmartActionComponent (no unverified GameSystem retrieval).
	bool	m_bGarrisonUseSmartActions	= true;

	// Close-quarters melee (parity feature "Close combat behaviour" - melee): a member with an enemy inside
	// m_fMeleeRange strikes via SCR_MeleeComponent.PerformAttack. Optionally only when out of ammo. Server-side,
	// on-foot, default OFF. See DCO_Melee.c. Runtime-unknown: server-triggered AI melee swing/hit - validate.
	bool	m_bEnableMelee				= false;
	float	m_fMeleeCheckSec			= 1.0;		// how often melee is evaluated (also the per-group swing cooldown)
	float	m_fMeleeRange				= 2.0;		// grappling range (m) within which a member melees
	bool	m_bMeleeOnlyWhenDry			= false;	// only melee when the member's weapon is out of ammo

	// Machine-gunner emplacement: a member carrying a machine gun deploys as a static base of fire instead of
	// maneuvering like a rifleman - if he can see the enemy he goes prone and holds his firing position to
	// suppress; if not, he repositions a short distance to the nearest higher spot with line of sight. Server-
	// side, on-foot, default OFF. See DCO_MachineGunner.c. (Uses the EWeaponType.WT_MACHINEGUN weapon type.)
	bool	m_bEnableMachineGunner		= false;
	float	m_fMGCheckSec				= 3.0;	// how often MG emplacement is evaluated
	bool	m_bMGReposition				= true;	// if no line of sight, move to a nearby spot that has it (else just deploy in place)
	float	m_fMGRepositionRadius		= 25.0;	// search radius (m) for a firing position with sight lines
	float	m_fMGSightHeight			= 0.8;	// height (m) of the LOS check (prone/deployed gun line)

	// Smoke-covered retreat: when a group breaks and flees, pop smoke toward the enemy to
	// screen the withdrawal. Native wire-in via SCR_AIActivitySmokeCoverFeature.Execute (no asset GUID).
	// Server-side, default OFF. (If a group has no smoke grenade the Execute simply fails - no effect.)
	bool	m_bEnableFleeSmoke			= false;

	// Vehicle hijacking: an on-foot group in contact commandeers the nearest empty drivable
	// vehicle within range (for mobility / firepower / escape). Native: find the vehicle via
	// BaseWorld.QueryEntitiesBySphere + SCR_BaseCompartmentManagerComponent (occupant/free-crew-seat check),
	// then SCR_AIMessageHandling.SendGetInMessage(..., EAICompartmentType.Pilot, ...). Server-side, default OFF.
	bool	m_bEnableVehicleHijack		= false;
	float	m_fHijackRange				= 100.0;	// metres to search for an empty vehicle
	float	m_fHijackCheckIntervalSec	= 10.0;		// how often an on-foot group in contact looks for a vehicle

	// HVT / priority targeting: when a group perceives a high-value target (a vehicle - tank,
	// APC, technical) it issues a priority Attack on it instead of trading shots with infantry. Built on
	// SCR_AITargetInfo + SCR_AIMessage_Attack. Server-side, default OFF.
	bool	m_bEnableHVTTargeting		= false;
	float	m_fHVTCheckIntervalSec		= 4.0;	// how often a group re-evaluates its highest-value target

	// Shared morale pool: gate surrender on the combined casualties of same-faction groups
	// within a radius, not just this lone group - so a small team fighting alongside a strong friendly force
	// doesn't fold after one early loss. Server-side, default ON (live-server default; opt-out via GM).
	bool	m_bEnableSharedMorale		= true;
	float	m_fMoralePoolRadius			= 200.0;	// metres - friendly groups within this share the casualty pool

	// Safe-eject gate: block an AI passenger from voluntarily dismounting a vehicle that is moving fast or is
	// high off the ground (so heli crews/passengers don't bail mid-flight and fall). Forced ejects
	// (death/destruction) and players are never blocked. Server-side, default OFF.
	bool	m_bEnableSafeEjectGate		= false;
	float	m_fSafeEjectMaxSpeedKmh		= 15.0;	// above this vehicle speed, AI won't voluntarily get out
	float	m_fSafeEjectMaxHeightM		= 3.0;	// above this height-above-ground, AI won't voluntarily get out


	// Squad-leader asset use: when in contact and able to spare an element, the leader sends a maneuver
	// fireteam member to man a nearby empty static weapon / turret (MG, mortar), then recalls it (dismount +
	// rejoin) when the contact clears. Server-side, default OFF.
	bool	m_bEnableAssetUse			= false;
	float	m_fAssetCheckSec			= 8.0;	// how often the leader looks for / re-evaluates an asset
	float	m_fAssetRange				= 80.0;	// metres to search for a usable static weapon / turret
	// Man a nearby static weapon even when not in contact (and with just a spare member, not two fireteams), so
	// a group holding/defending a position sets the gun up autonomously before the enemy arrives. Default ON
	// (asset use is opt-in already). Pairs with the Defend role / garrison.
	bool	m_bAssetProactive			= true;

	// Real suppression: amplify the engine's per-AI suppression measure (SCR_AIThreatSystem) - when a member is
	// pinned it degrades its fire, sprints to cover/breaks LOS, and the group pops smoke when a large fraction
	// is pinned. Makes incoming fire actually matter. Server-side, default OFF.
	bool	m_bEnableSuppression			= false;
	float	m_fSuppressionCheckSec			= 1.5;	// how often suppression is evaluated (responsive)
	float	m_fSuppressionThreshold			= 0.5;	// GetSuppressionMeasure at/above this = pinned (range unknown - tune via m_bDebug [DCO:SUPPRESS] logs)
	float	m_fSuppressedFireRateCoef		= 0.4;	// fire-rate multiplier while pinned (heads down)
	bool	m_bSuppressionSeekCover			= true;	// pinned members sprint to the nearest cover / break LOS
	float	m_fSuppressionCoverRadius		= 12.0;	// search radius (m) for cover when a pinned member breaks LOS
	float	m_fSuppressionSmokeFraction		= 0.5;	// fraction of the group pinned that triggers a smoke screen

	// Dig in (don't abandon tactical positions): when a pinned member is exposed, only dash to cover that is
	// close (within m_fSuppressionDashMax) and does not give up the high ground (no more than
	// m_fSuppressionMaxDescend below the current spot). If no such cover is a short dash away, the member goes
	// prone in place and returns fire instead of sprinting across open ground / downhill - stops AI running into
	// the open and abandoning hilltops under fire. Default ON (only acts while a member is already pinned).
	bool	m_bSuppressionDigIn				= true;
	float	m_fSuppressionDashMax			= 8.0;	// max distance (m) a pinned member dashes to cover before just digging in
	float	m_fSuppressionMaxDescend		= 4.0;	// won't relocate to cover more than this (m) below the current position (holds high ground)

	// Morale contagion / cascade: a group loses morale when nearby same-faction friendlies have broken or
	// surrendered, so panic spreads down a line and routs cascade (the realistic flip side of unit cohesion).
	// Server-side, default OFF.
	bool	m_bEnableMoraleContagion			= false;
	float	m_fContagionRadius					= 150.0;	// metres - nearby friendly groups that can spread panic
	float	m_fContagionCheckSec				= 5.0;		// how often contagion is evaluated
	float	m_fContagionMoralePerBrokenGroup	= 4.0;		// morale lost per nearby broken/surrendered friendly group, per check
	float	m_fContagionMaxLossPerTick			= 12.0;		// cap on contagion morale loss per check

	// Friendly-fire / fratricide avoidance: when a member would fire on an enemy but a
	// friendly is in its firing lane, briefly hold its fire. Server-side, default ON (live-server default).
	bool	m_bEnableFriendlyFire		= true;
	float	m_fFriendlyFireHold			= 1.0;	// seconds of held-fire when a friendly is in the lane
	float	m_fFriendlyLaneCheckSec		= 0.5;	// how often the lanes are checked

	// Straggler merge: fold a reduced AI group (<= size, in contact) into the nearest larger
	// same-faction group so lone survivors keep fighting effectively. Server-side, default OFF.
	bool	m_bEnableStragglerMerge		= false;
	int		m_iStragglerSize			= 2;		// merge when the group is at or below this many members
	float	m_fMergeRadius				= 150.0;	// metres to look for a larger friendly group to join
	float	m_fMergeCheckSec			= 8.0;		// how often a straggler looks to merge

	// Surrender recovery / re-arm: a surrendered group can recover - reactivate, stand up, get its
	// faction back, re-arm (re-pickup the dropped weapon, or spawn a replacement from its prefab), resupply
	// ammo, and rejoin the fight - if its morale climbs back up or it is left unmolested for a while. Never
	// recovers while an enemy is actively perceived. Server-side, default OFF (changes surrender behaviour).
	bool	m_bEnableSurrenderRecovery	= false;
	float	m_fRecoveryMoraleThreshold	= 50.0;		// surrendered morale recovers to >= this: stand back up
	float	m_fRecoveryNoContactSec		= 60.0;		// or no enemy perceived for this long: recover
	float	m_fRecoveryMinSurrenderSec	= 20.0;		// never recover before this many seconds surrendered
	float	m_fRecoveryCheckSec			= 5.0;		// how often recovery is evaluated for a surrendered group
	float	m_fRecoveryChancePerTick	= 0.5;		// per-eligible-tick chance to actually recover (staggers groups)
	float	m_fRecoveryMoralePerTick	= 5.0;		// morale regained per recovery tick while safe & surrendered
	int		m_iRecoveryMagazineCount	= 3;		// magazines resupplied to the re-armed weapon (0 = none)
	bool	m_bRecoverySpawnWeaponIfLost = true;	// if the dropped weapon is gone, spawn a replacement from its prefab
	string	m_sRecoveryThrowablePrefab	= "";		// optional grenade/throwable prefab to re-give (empty = none)
	int		m_iRecoveryThrowableCount	= 0;		// how many of m_sRecoveryThrowablePrefab to re-give on recovery

	// Reinforcement / shared situational awareness
	bool	m_bEnableReinforcement		= true;
	float	m_fReinforcementRadius		= 300.0;	// metres - friendly groups within this share contact
	float	m_fReinforcementCooldownSec	= 8.0;		// per-group broadcast cooldown
	int		m_iReinforcementMaxResponders = 3;		// cap responders per broadcast (prevents map-wide cascade)
	bool	m_bReinforcementConverge	= true;		// idle responders also move toward the contact

	// Deconfliction with external waypoint owners (Scenario Framework / IPC / hand-placed designer waypoints).
	// A freshly-spawned patrol/capture group has no perceived enemy yet, so the old code treated it as "idle"
	// and converge-ordered it - yanking it off its assigned waypoint and flooding the log with move orders.
	// With this on, a responder that still holds its own current waypoint is considered busy (doing its task)
	// and is left alone; it only becomes a converge candidate once that waypoint clears ("finish IPC task,
	// then act on DCO"). A group explicitly flagged reinforcement-eligible (role preset / GM toggle) overrides
	// this and converges anyway ("override the waypoint and move as a reinforcement"). Default ON.
	bool	m_bReinforcementRespectWaypoints		= true;
	// Per-responder re-issue guard: once DCO orders a specific group to converge it will not re-order that same
	// group for this many seconds, unless the contact has moved more than the retarget distance below. This is
	// what kills the duplicate-order spam and the "walk 100 m, get re-pathed, stop, wander" behaviour caused by
	// re-broadcasting a move every scan. 0 = old behaviour (may re-order every cooldown).
	float	m_fReinforcementReissueSec				= 20.0;
	float	m_fReinforcementRetargetDist			= 50.0;	// metres the contact must move to justify a fresh converge order
	// When an overriding group (reinforcement-eligible, or a QRF responder) takes over, also remove its current
	// waypoints so the DCO move actually holds instead of tug-of-warring with the waypoint. Default OFF: clearing
	// a Scenario-Framework / IPC waypoint can desync that system's own state tracking, so leave it off unless you
	// have validated it with your mission framework. Off = the DCO move still takes priority while it is being
	// issued, but the external waypoint may reassert once DCO stops ordering.
	bool	m_bClearWaypointOnOverride				= false;

	// QRF garrison behaviour (used by a group flagged QRF, esp. via a QRF Task Zone with a hold point).
	float	m_fQRFCriticalMorale	= 40.0;	// a QRF reacts to a friendly in contact only once that friendly's morale falls to/below this - so QRFs hold during routine contact and commit only when things get critical. A broken/routing friendly always triggers regardless.
	float	m_fQRFHoldLeash			= 30.0;	// metres - a QRF that has a hold/rally point walks back to it when it has drifted further than this and nobody needs help (garrison-at-the-meeting-point + return-after-responding). 0 = don't return.

	// Vehicle-borne reinforcement: when a converging responder is far from the contact, order
	// it to move with useVehicles = true so it mounts up to cover the distance faster (via SCR_AIMessage_Move
	// with the vehicle flag, instead of the on-foot SendMoveMessage). Server-side, default OFF.
	bool	m_bEnableVehicleReinforcement	= false;
	float	m_fVehicleReinforceMinDist		= 200.0;	// only use vehicles if the responder is at least this far (m)

	// Hit-reaction flinch ("disorientation when shot"): when an AI character takes a hit, it
	// briefly holds fire so being shot at actually disrupts it. Hooked by overriding
	// SCR_CharacterDamageManagerComponent.OnDamage(BaseDamageContext). AI only (players have a deactivated
	// AI control component), server-side, healing damage ignored, throttled. Default OFF.
	bool	m_bEnableHitFlinch			= false;
	float	m_fHitFlinchDuration		= 0.6;	// seconds of held-fire per flinch
	float	m_fHitFlinchCooldownSec		= 0.5;	// min seconds between flinches (dedupes damage-over-time ticks)

	// Hide from armour: an on-foot group that perceives an enemy
	// vehicle (tank/APC) within range breaks and flees rather than suicidally trading rifle fire with armour.
	// For infantry forces that lack AT. Reuses vehicle detection + the flee path. Server-side, default OFF.
	bool	m_bEnableFleeFromArmor		= false;
	float	m_fFleeFromArmorRange		= 150.0;	// metres - an enemy vehicle this close triggers the flee
	float	m_fFleeFromArmorCheckSec	= 4.0;		// how often it is evaluated

	// Adaptive formation: switch the group's formation by situation - a tighter
	// column when travelling (no contact), a spread line when in contact. Applied via
	// AIFormationComponent.SetFormation(name). SCR_EAIGroupFormation members = {Wedge, Line, Column,
	// StaggeredColumn}; the formation name strings + which entity hosts AIFormationComponent are runtime
	// items to validate (the call compiles either way; a wrong name simply no-ops). Server-side, default OFF.
	bool	m_bEnableAdaptiveFormation	= false;
	float	m_fFormationCheckSec		= 5.0;
	string	m_sFormationTravel			= "Column";	// formation when not in contact
	string	m_sFormationContact			= "Line";	// formation when a perceived enemy is present

	static DCO_MoraleSettings Get()
	{
		if (!s_Instance)
		{
			s_Instance = new DCO_MoraleSettings();
			// Apply the optional server JSON override once, over the baked defaults (missing file = no-op).
			DCO_JsonConfig.LoadInto(s_Instance);
		}
		return s_Instance;
	}

	float UpdateIntervalMs()
	{
		return m_fUpdateIntervalSec * 1000.0;
	}
}
