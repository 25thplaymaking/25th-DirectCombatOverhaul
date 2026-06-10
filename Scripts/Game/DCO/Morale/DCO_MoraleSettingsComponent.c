// Optional designer settings override (Scenario Settings). Optional. The DCO systems work in every
// Game Master mode with no setup (defaults live in the DCO_MoraleSettings / DCO_TacticalMoveSettings
// singletons, and are tunable live via the "25th DCO" Game Master attributes, or globally via
// $profile:DCO_Settings.json).
//
// This component exists only as a convenience for scenario designers who want to bake specific values
// into a world/game-mode prefab: on init it copies its attribute values into the two singletons.
// It is not required for the mod to function.
//
// Scope: this surface exposes every system's master enable toggle plus its most important tunables.
// Fine-grained per-system numbers (delays, sub-thresholds, sample counts) stay in the JSON config
// (Configs/DCO_Settings.example.json) so this designer panel stays readable.
//
// It overrides, for the fields it exposes:
//   - Every default below is matched to the singleton's baked default, so adding this component with
//     nothing changed leaves behaviour identical to not having it. If you change a default here, make
//     sure it still matches the store, or merely placing the component will silently alter that system.
//   - OnPostInit runs after the singleton first loads $profile:DCO_Settings.json, so for the fields
//     this component exposes, the component value wins over the server JSON. Use one surface or the
//     other for a given system; don't expect JSON to override a value baked here.
class DCO_MoraleSettingsComponentClass : ScriptComponentClass
{
}

class DCO_MoraleSettingsComponent : ScriptComponent
{
	// ============================================================ CORE MORALE ============================================================
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Master switch for the DCO morale / break / surrender system.", category: "25th DCO")]
	bool m_bEnabled;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Unified DCO debug logging (tagged [DCO:...] lines to console). Verbose - tuning/testing only.", category: "25th DCO")]
	bool m_bDebug;

	[Attribute(defvalue: "2.0", uiwidget: UIWidgets.Slider, params: "0.25 10 0.25", desc: "How often (seconds) group morale is evaluated.", category: "25th DCO")]
	float m_fUpdateIntervalSec;

	[Attribute(defvalue: "25", uiwidget: UIWidgets.Slider, params: "0 100 1", desc: "Morale at/below which a group breaks and flees.", category: "25th DCO")]
	float m_fFleeThreshold;

	[Attribute(defvalue: "8", uiwidget: UIWidgets.Slider, params: "0 100 1", desc: "Morale at/below which a group surrenders.", category: "25th DCO")]
	float m_fSurrenderThreshold;

	[Attribute(defvalue: "55", uiwidget: UIWidgets.Slider, params: "0 100 1", desc: "Morale at/above which a broken group rallies.", category: "25th DCO")]
	float m_fRallyThreshold;

	[Attribute(defvalue: "7", uiwidget: UIWidgets.Slider, params: "0 50 0.5", desc: "Morale regained per evaluation when safe.", category: "25th DCO")]
	float m_fRecoveryPerTick;

	[Attribute(defvalue: "9", uiwidget: UIWidgets.Slider, params: "0 50 0.5", desc: "Base morale lost per evaluation under heavy threat.", category: "25th DCO")]
	float m_fDrainPerTick;

	[Attribute(defvalue: "45", uiwidget: UIWidgets.Slider, params: "0 100 1", desc: "Extra morale drain at 100% casualties.", category: "25th DCO")]
	float m_fCasualtyWeight;

	[Attribute(defvalue: "0.5", uiwidget: UIWidgets.Slider, params: "0 2 0.05", desc: "Threat measure at/above which a group is 'under fire'.", category: "25th DCO")]
	float m_fHeavyThreat;

	[Attribute(defvalue: "120", uiwidget: UIWidgets.Slider, params: "20 400 10", desc: "Distance (m) a broken group flees.", category: "25th DCO")]
	float m_fFleeDistance;

	// ============================================================ SURRENDER ============================================================
	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Allow groups to surrender (otherwise they only flee).", category: "25th DCO - Surrender")]
	bool m_bEnableSurrender;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Surrendering units drop weapon + gear (keep jacket/pants/boots).", category: "25th DCO - Surrender")]
	bool m_bDropGearOnSurrender;

	[Attribute(defvalue: "1500", uiwidget: UIWidgets.Slider, params: "0 8000 100", desc: "Delay (ms) before surrendered units drop gear.", category: "25th DCO - Surrender")]
	int m_iSurrenderDropDelayMs;

	[Attribute(defvalue: "3000", uiwidget: UIWidgets.Slider, params: "0 10000 100", desc: "Delay (ms) before surrendered units lie prone.", category: "25th DCO - Surrender")]
	int m_iSurrenderProneDelayMs;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Play an authored surrender sound event when a unit surrenders (set the event name below).", category: "25th DCO - Surrender")]
	bool m_bEnableSurrenderVoice;

	[Attribute(defvalue: "", uiwidget: UIWidgets.EditBox, desc: "Sound EVENT name to play on surrender (authored in a SoundProject .acp, e.g. SOUND_DCO_SURRENDER). Empty = no voiceline.", category: "25th DCO - Surrender")]
	string m_sSurrenderSoundEvent;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Last stand: groups never surrender (they still break/flee). For elite/fanatical forces.", category: "25th DCO - Surrender")]
	bool m_bEnableLastStand;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Fake surrender: some 'surrendering' units keep a grenade and prime it when an enemy closes (ambush).", category: "25th DCO - Surrender")]
	bool m_bEnableFakeSurrender;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Surrender recovery: a surrendered group can re-arm and rejoin the fight if morale recovers or it is left alone.", category: "25th DCO - Surrender")]
	bool m_bEnableSurrenderRecovery;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Shared morale pool: surrender is gated on combined casualties of nearby same-faction groups (small teams don't fold early).", category: "25th DCO - Surrender")]
	bool m_bEnableSharedMorale;

	// ============================================================ INFANTRY COMBAT ============================================================
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Panic: a group just above the flee threshold may briefly cower + hold fire before breaking.", category: "25th DCO - Infantry")]
	bool m_bEnablePanic;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Morale affects accuracy: AI combat skill + fire rate degrade as group morale falls, recover as it rises.", category: "25th DCO - Infantry")]
	bool m_bEnableMoraleAccuracy;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Progressive aim zeroing: a fresh target starts cold and aim warms up to the unit's trained skill over dwell time.", category: "25th DCO - Infantry")]
	bool m_bEnableAimZeroing;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vision / night limiting: AI take longer to spot you in the dark (penalty lifts on contact).", category: "25th DCO - Infantry")]
	bool m_bEnableVisionLimit;

	[Attribute(defvalue: "0.5", uiwidget: UIWidgets.Slider, params: "0.1 1 0.05", desc: "Perception multiplier at full dark (1.0 = no night penalty).", category: "25th DCO - Infantry")]
	float m_fNightPerceptionFactor;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Per-member morale: individual soldiers react to their own local pressure on top of squad morale.", category: "25th DCO - Infantry")]
	bool m_bEnableMemberMorale;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Adjust-to-cover stance: in contact, AI fit their stance to the cover in front of them.", category: "25th DCO - Infantry")]
	bool m_bEnableCoverStance;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Real suppression: pinned members degrade fire and seek cover; group pops smoke when heavily pinned.", category: "25th DCO - Infantry")]
	bool m_bEnableSuppression;

	[Attribute(defvalue: "0.5", uiwidget: UIWidgets.Slider, params: "0 2 0.05", desc: "Suppression measure at/above which a member is 'pinned' (range unknown - tune via debug log).", category: "25th DCO - Infantry")]
	float m_fSuppressionThreshold;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Pinned members sprint to the nearest cover / break line of sight.", category: "25th DCO - Infantry")]
	bool m_bSuppressionSeekCover;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Dig in: only dash to CLOSE cover that doesn't lose the high ground; otherwise go prone in place and fight.", category: "25th DCO - Infantry")]
	bool m_bSuppressionDigIn;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Machine-gunner emplacement: an MG carrier deploys prone and holds a firing position instead of maneuvering.", category: "25th DCO - Infantry")]
	bool m_bEnableMachineGunner;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "If a machine gunner has no line of sight, reposition a short distance to a spot that does (else deploy in place).", category: "25th DCO - Infantry")]
	bool m_bMGReposition;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Emergency rearm: a soldier who runs dry tops up his magazines (and optionally scavenges a dropped weapon).", category: "25th DCO - Infantry")]
	bool m_bEnableEmergencyRearm;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Emergency rearm scavenge: also walk to and pick up the nearest dropped ground weapon.", category: "25th DCO - Infantry")]
	bool m_bRearmScavenge;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Close-quarters melee (experimental): AI strike with melee when an enemy is right on top of them.", category: "25th DCO - Infantry")]
	bool m_bEnableMelee;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Only melee when the member's weapon is out of ammo.", category: "25th DCO - Infantry")]
	bool m_bMeleeOnlyWhenDry;



	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Squad-leader asset use: spare members man a nearby empty static weapon / turret, then recall when clear.", category: "25th DCO - Infantry")]
	bool m_bEnableAssetUse;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Proactive asset use: man a nearby static weapon even when NOT in contact (defenders set guns up early).", category: "25th DCO - Infantry")]
	bool m_bAssetProactive;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Stance cooldown (anti-jitter): throttle rapid prone/crouch/stand cycling on the spot.", category: "25th DCO - Infantry")]
	bool m_bEnableStanceCooldown;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Hit reaction flinch: an AI that takes a hit briefly holds fire (disorientation when shot). AI only.", category: "25th DCO - Infantry")]
	bool m_bEnableHitFlinch;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Fratricide avoidance: a member holds fire when a friendly is in its firing lane.", category: "25th DCO - Infantry")]
	bool m_bEnableFriendlyFire;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Morale contagion: a group loses morale when nearby same-faction friendlies break or surrender (routs cascade).", category: "25th DCO - Infantry")]
	bool m_bEnableMoraleContagion;

	// ============================================================ COORDINATION ============================================================
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vocal info sharing: a squad that spots an enemy calls it out, sharpening nearby friendly squads (shouting range).", category: "25th DCO - Coordination")]
	bool m_bEnableVocalShare;

	[Attribute(defvalue: "60", uiwidget: UIWidgets.Slider, params: "10 200 5", desc: "Vocal shouting range (m) to nearby friendly groups.", category: "25th DCO - Coordination")]
	float m_fVocalRadius;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Radio info sharing: leaders relay contacts to friendly squads across a much larger radius.", category: "25th DCO - Coordination")]
	bool m_bEnableRadioShare;

	[Attribute(defvalue: "400", uiwidget: UIWidgets.Slider, params: "50 1000 25", desc: "Radio range (m) to friendly groups.", category: "25th DCO - Coordination")]
	float m_fRadioRadius;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Radio flags listeners reinforcement-eligible so they move up in support (needs reinforcement enabled).", category: "25th DCO - Coordination")]
	bool m_bRadioFlagsReinforcement;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Reinforcement: friendly groups within radius share contact; idle responders move toward it.", category: "25th DCO - Coordination")]
	bool m_bEnableReinforcement;

	[Attribute(defvalue: "300", uiwidget: UIWidgets.Slider, params: "50 1000 25", desc: "Reinforcement radius (m) - friendly groups within this share contact.", category: "25th DCO - Coordination")]
	float m_fReinforcementRadius;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Respect external waypoints: a responder still on its own waypoint (IPC/Scenario Framework) is left to finish first.", category: "25th DCO - Coordination")]
	bool m_bReinforcementRespectWaypoints;

	[Attribute(defvalue: "40", uiwidget: UIWidgets.Slider, params: "0 100 1", desc: "QRF critical morale: a QRF commits to a friendly in contact only once that friendly's morale falls to/below this.", category: "25th DCO - Coordination")]
	float m_fQRFCriticalMorale;

	[Attribute(defvalue: "30", uiwidget: UIWidgets.Slider, params: "0 200 5", desc: "QRF hold leash (m): a QRF returns to its hold/rally point when it has drifted further than this and nobody needs help. 0 = don't return.", category: "25th DCO - Coordination")]
	float m_fQRFHoldLeash;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "AI commander (toggleable): a battlefield-wide coordinator that vectors the nearest reserves to the hottest fight.", category: "25th DCO - Coordination")]
	bool m_bEnableCommander;

	[Attribute(defvalue: "2", uiwidget: UIWidgets.Slider, params: "1 8 1", desc: "Reserves the commander commits to the hot fight per assessment.", category: "25th DCO - Coordination")]
	int m_iCommanderMaxDispatch;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Building garrison (experimental): a defending squad near a building occupies it, splitting across the perimeter.", category: "25th DCO - Coordination")]
	bool m_bEnableGarrison;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Composition defense + base morale: own-faction groups near a placed composition (checkpoint/bunker) reposition onto it, man its assets, and gain morale that scales with nearby composition count. GM/scenario orders always win. Experimental.", category: "25th DCO - Coordination")]
	bool m_bEnableCompositionDefense;

	[Attribute(defvalue: "120", uiwidget: UIWidgets.Slider, params: "20 400 10", desc: "Composition awareness / reposition radius (m). Local only - no map-wide seeking.", category: "25th DCO - Coordination")]
	float m_fCompositionRadius;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Composition: move onto the nearest composition (off = adopt the defensive behaviour in place).", category: "25th DCO - Coordination")]
	bool m_bCompositionReposition;

	[Attribute(defvalue: "5", uiwidget: UIWidgets.Slider, params: "0 50 1", desc: "Composition base-morale resolve added per nearby friendly composition (0-100 scale).", category: "25th DCO - Coordination")]
	float m_fCompositionMoralePerUnit;

	[Attribute(defvalue: "20", uiwidget: UIWidgets.Slider, params: "0 100 5", desc: "Composition base-morale resolve cap (0-100 scale).", category: "25th DCO - Coordination")]
	float m_fCompositionMoraleCap;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "CQB / building assault: AI identify buildings and push inside to clear an enemy sheltering within.", category: "25th DCO - Coordination")]
	bool m_bEnableCQB;

	[Attribute(defvalue: "60", uiwidget: UIWidgets.Slider, params: "10 150 5", desc: "CQB engage range (m): only assault a building when the enemy is within this.", category: "25th DCO - Coordination")]
	float m_fCqbEngageRange;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Artillery fire missions: AI mortar crews call fire on enemy clusters, gated by a cooldown.", category: "25th DCO - Coordination")]
	bool m_bEnableArtillery;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.Slider, params: "1 12 1", desc: "Minimum perceived enemies in a cluster to justify a fire mission.", category: "25th DCO - Coordination")]
	int m_iArtyMinTargets;

	[Attribute(defvalue: "60", uiwidget: UIWidgets.Slider, params: "10 300 5", desc: "Minimum seconds between fire missions per battery (approval throttle).", category: "25th DCO - Coordination")]
	float m_fArtyCooldownSec;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Straggler merge: a reduced AI group in contact folds into the nearest larger same-faction group.", category: "25th DCO - Coordination")]
	bool m_bEnableStragglerMerge;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Night illumination: groups in contact in low light pop illum flares over the enemy (native behaviour).", category: "25th DCO - Coordination")]
	bool m_bEnableNightIllum;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Smoke on flee: a breaking/fleeing group pops a smoke screen toward the enemy to cover its withdrawal.", category: "25th DCO - Coordination")]
	bool m_bEnableFleeSmoke;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Adaptive formation: groups use column when travelling and line when in contact.", category: "25th DCO - Coordination")]
	bool m_bEnableAdaptiveFormation;

	// ============================================================ VEHICLES ============================================================
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vehicle armour angling: AI vehicle crews in contact turn their front (strongest armour) toward the nearest enemy.", category: "25th DCO - Vehicles")]
	bool m_bEnableVehicleArmor;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vehicle combat positioning: a crewed vehicle with no shot repositions for line of sight; pulls back when threatened.", category: "25th DCO - Vehicles")]
	bool m_bEnableVehicleCombat;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vehicle doctrine (master): out of combat vehicles drive on vanilla navmesh; on contact a controller runs convoy/section tactics.", category: "25th DCO - Vehicles")]
	bool m_bEnableVehicleDoctrine;

	[Attribute(defvalue: "120", uiwidget: UIWidgets.Slider, params: "30 500 10", desc: "Section radius (m): friendly vehicles within this of each other form a section.", category: "25th DCO - Vehicles")]
	float m_fVehDoctrineSectionRadius;

	[Attribute(defvalue: "12", uiwidget: UIWidgets.Slider, params: "0 60 1", desc: "Handback (s): no contact in the section for this long returns full control to vanilla navmesh.", category: "25th DCO - Vehicles")]
	float m_fVehDoctrineHandbackSec;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Allow bounding overwatch (one vehicle moves while another covers).", category: "25th DCO - Vehicles")]
	bool m_bVehDoctrineBounding;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Dismount transport passengers to a base of fire when the section is stuck (road blocked / disabled).", category: "25th DCO - Vehicles")]
	bool m_bVehDoctrineDismountStuck;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Handle disabled friendly vehicles (crew abandons / joins base of fire).", category: "25th DCO - Vehicles")]
	bool m_bVehDoctrineRecovery;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Debug-draw section links, roles, and target points.", category: "25th DCO - Vehicles")]
	bool m_bDebugVehDoctrine;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "CQB town-clearing (master): on-foot groups methodically clear buildings in their detection radius as they move to their target, chaining house-to-house.", category: "25th DCO - CQB")]
	bool m_bEnableCqbClear;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Marked-only: if Yes, only groups flagged 'CQB Clearer' (CLEAR zone / GM attribute / role preset) clear. If No, every on-foot group clears.", category: "25th DCO - CQB")]
	bool m_bCqbClearMarkedOnly;

	[Attribute(defvalue: "80", uiwidget: UIWidgets.Slider, params: "20 300 5", desc: "Sweep radius (m): buildings within this of the group leader are cleared.", category: "25th DCO - CQB")]
	float m_fCqbClearRadius;

	[Attribute(defvalue: "3", uiwidget: UIWidgets.Slider, params: "0.5 15 0.5", desc: "Dwell (s) a member holds + scans each interior node before advancing.", category: "25th DCO - CQB")]
	float m_fCqbNodeDwellSec;

	[Attribute(defvalue: "6", uiwidget: UIWidgets.Slider, params: "1 20 1", desc: "Max buildings attempted per sweep cycle.", category: "25th DCO - CQB")]
	int m_iCqbMaxBuildings;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Debug-draw building states (claimed/clearing/cleared) + stack/node markers.", category: "25th DCO - CQB")]
	bool m_bDebugCqbClear;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Vehicle back-off: when heavily threatened, pull back to break line of sight (hull-down).", category: "25th DCO - Vehicles")]
	bool m_bVehicleBackoff;

	[Attribute(defvalue: "6.0", uiwidget: UIWidgets.Slider, params: "0 30 0.5", desc: "Max seconds a vehicle hides in cover before peeking back out to fight (anti-freeze).", category: "25th DCO - Vehicles")]
	float m_fVehicleBackoffHoldSec;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vehicle hijacking: an on-foot group in contact commandeers the nearest empty drivable vehicle in range.", category: "25th DCO - Vehicles")]
	bool m_bEnableVehicleHijack;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "HVT priority targeting: a group focuses fire on a perceived enemy vehicle instead of infantry.", category: "25th DCO - Vehicles")]
	bool m_bEnableHVTTargeting;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Vehicle-borne reinforcement: distant converging reinforcements mount up to close the gap faster.", category: "25th DCO - Vehicles")]
	bool m_bEnableVehicleReinforcement;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Hide from armour: an on-foot group that perceives an enemy vehicle in range flees instead of fighting it.", category: "25th DCO - Vehicles")]
	bool m_bEnableFleeFromArmor;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Safe-eject gate: block AI from voluntarily dismounting a vehicle that is moving fast or high off the ground.", category: "25th DCO - Vehicles")]
	bool m_bEnableSafeEjectGate;

	// ============================================================ MOVEMENT (DCO_TacticalMoveSettings) ============================================================
	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Tactical movement: break long move orders into cover-weighted check-in legs instead of pathing straight.", category: "25th DCO - Movement")]
	bool m_bEnableTacticalMove;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Avoid threat funnel: side-step the approach off the enemy axis so the group doesn't path up the kill-zone.", category: "25th DCO - Movement")]
	bool m_bAvoidThreatFunnel;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Flanking maneuver: approach a known enemy from its flank instead of straight down its front.", category: "25th DCO - Movement")]
	bool m_bEnableFlanking;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Exposure scoring: pick the least line-of-sight-exposed of several candidate move destinations.", category: "25th DCO - Movement")]
	bool m_bEnableExposureScoring;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Procedural tactical path: re-decide the route as the group moves, bounding leg-by-leg through cover.", category: "25th DCO - Movement")]
	bool m_bEnableProceduralPath;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Traveling overwatch: a lead element advances while a trailing element follows back, ready to support.", category: "25th DCO - Movement")]
	bool m_bEnableTravelingOverwatch;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Bounding overwatch: under heavy danger, peel a base-of-fire element while the maneuver element bounds (leapfrog).", category: "25th DCO - Movement")]
	bool m_bEnableBaseOfFireSplit;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Halt on contact: a perceived enemy within range halts the tactical advance - fight from here, don't bound into fire.", category: "25th DCO - Movement")]
	bool m_bPathHaltOnContact;

	[Attribute(defvalue: "200", uiwidget: UIWidgets.Slider, params: "20 500 10", desc: "Perceived enemy within this (m) = decisive contact -> halt the advance.", category: "25th DCO - Movement")]
	float m_fPathContactHaltRange;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Leader in base: keep the leader's element as the static base of fire so the leader never bounds into the kill zone.", category: "25th DCO - Movement")]
	bool m_bPathLeaderInBase;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Base maintains line of sight: a base-of-fire member with no LOS to the threat repositions to regain it.", category: "25th DCO - Movement")]
	bool m_bPathBaseMaintainLos;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Cover-aware placement: in contact, nudge exposed members to the nearest concealed, walkable spot.", category: "25th DCO - Movement")]
	bool m_bEnableCoverSeek;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "React to contact: execute a course of action (support by fire / assault flank / break contact) when in contact.", category: "25th DCO - Movement")]
	bool m_bEnableReactToContact;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "Tactical brain: choose the course of action by relative combat power + morale (feeds React to Contact).", category: "25th DCO - Movement")]
	bool m_bEnableTacticalBrain;

	[Attribute(defvalue: "1", uiwidget: UIWidgets.CheckBox, desc: "Standoff (no suicide-charge): AI keep beyond a weapon-aware minimum distance in open fights instead of closing to knife range; yields for committed assaults and building clears. Per-weapon distances are tuned via JSON. ON by default.", category: "25th DCO - Movement")]
	bool m_bEnableStandoff;

	[Attribute(defvalue: "0", uiwidget: UIWidgets.CheckBox, desc: "DCO formation shapes (experimental): impose echelon / vee / dispersion via per-member offsets.", category: "25th DCO - Movement")]
	bool m_bEnableDcoFormations;

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		// --- Morale / surrender / combat / coordination / vehicle store ---
		DCO_MoraleSettings s = DCO_MoraleSettings.Get();
		s.m_bEnabled					= m_bEnabled;
		s.m_bDebug						= m_bDebug;
		s.m_fUpdateIntervalSec			= m_fUpdateIntervalSec;
		s.m_fFleeThreshold				= m_fFleeThreshold;
		s.m_fSurrenderThreshold			= m_fSurrenderThreshold;
		s.m_fRallyThreshold				= m_fRallyThreshold;
		s.m_fRecoveryPerTick			= m_fRecoveryPerTick;
		s.m_fDrainPerTick				= m_fDrainPerTick;
		s.m_fCasualtyWeight				= m_fCasualtyWeight;
		s.m_fHeavyThreat				= m_fHeavyThreat;
		s.m_fFleeDistance				= m_fFleeDistance;

		s.m_bEnableSurrender			= m_bEnableSurrender;
		s.m_bDropGearOnSurrender		= m_bDropGearOnSurrender;
		s.m_iSurrenderDropDelayMs		= m_iSurrenderDropDelayMs;
		s.m_iSurrenderProneDelayMs		= m_iSurrenderProneDelayMs;
		s.m_bEnableSurrenderVoice		= m_bEnableSurrenderVoice;
		s.m_sSurrenderSoundEvent		= m_sSurrenderSoundEvent;
		s.m_bEnableLastStand			= m_bEnableLastStand;
		s.m_bEnableFakeSurrender		= m_bEnableFakeSurrender;
		s.m_bEnableSurrenderRecovery	= m_bEnableSurrenderRecovery;
		s.m_bEnableSharedMorale			= m_bEnableSharedMorale;

		s.m_bEnablePanic				= m_bEnablePanic;
		s.m_bEnableMoraleAccuracy		= m_bEnableMoraleAccuracy;
		s.m_bEnableAimZeroing			= m_bEnableAimZeroing;
		s.m_bEnableVisionLimit			= m_bEnableVisionLimit;
		s.m_fNightPerceptionFactor		= m_fNightPerceptionFactor;
		s.m_bEnableMemberMorale			= m_bEnableMemberMorale;
		s.m_bEnableCoverStance			= m_bEnableCoverStance;
		s.m_bEnableSuppression			= m_bEnableSuppression;
		s.m_fSuppressionThreshold		= m_fSuppressionThreshold;
		s.m_bSuppressionSeekCover		= m_bSuppressionSeekCover;
		s.m_bSuppressionDigIn			= m_bSuppressionDigIn;
		s.m_bEnableMachineGunner		= m_bEnableMachineGunner;
		s.m_bMGReposition				= m_bMGReposition;
		s.m_bEnableEmergencyRearm		= m_bEnableEmergencyRearm;
		s.m_bRearmScavenge				= m_bRearmScavenge;
		s.m_bEnableMelee				= m_bEnableMelee;
		s.m_bMeleeOnlyWhenDry			= m_bMeleeOnlyWhenDry;
		s.m_bEnableAssetUse				= m_bEnableAssetUse;
		s.m_bAssetProactive				= m_bAssetProactive;
		s.m_bEnableStanceCooldown		= m_bEnableStanceCooldown;
		s.m_bEnableHitFlinch			= m_bEnableHitFlinch;
		s.m_bEnableFriendlyFire			= m_bEnableFriendlyFire;
		s.m_bEnableMoraleContagion		= m_bEnableMoraleContagion;

		s.m_bEnableVocalShare			= m_bEnableVocalShare;
		s.m_fVocalRadius				= m_fVocalRadius;
		s.m_bEnableRadioShare			= m_bEnableRadioShare;
		s.m_fRadioRadius				= m_fRadioRadius;
		s.m_bRadioFlagsReinforcement	= m_bRadioFlagsReinforcement;
		s.m_bEnableReinforcement		= m_bEnableReinforcement;
		s.m_fReinforcementRadius		= m_fReinforcementRadius;
		s.m_bReinforcementRespectWaypoints	= m_bReinforcementRespectWaypoints;
		s.m_fQRFCriticalMorale			= m_fQRFCriticalMorale;
		s.m_fQRFHoldLeash				= m_fQRFHoldLeash;
		s.m_bEnableCommander			= m_bEnableCommander;
		s.m_iCommanderMaxDispatch		= m_iCommanderMaxDispatch;
		s.m_bEnableGarrison				= m_bEnableGarrison;
		s.m_bEnableCompositionDefense	= m_bEnableCompositionDefense;
		s.m_fCompositionRadius			= m_fCompositionRadius;
		s.m_bCompositionReposition		= m_bCompositionReposition;
		s.m_fCompositionMoralePerUnit	= m_fCompositionMoralePerUnit;
		s.m_fCompositionMoraleCap		= m_fCompositionMoraleCap;
		s.m_bEnableCQB					= m_bEnableCQB;
		s.m_fCqbEngageRange				= m_fCqbEngageRange;
		s.m_bEnableArtillery			= m_bEnableArtillery;
		s.m_iArtyMinTargets				= m_iArtyMinTargets;
		s.m_fArtyCooldownSec			= m_fArtyCooldownSec;
		s.m_bEnableStragglerMerge		= m_bEnableStragglerMerge;
		s.m_bEnableNightIllum			= m_bEnableNightIllum;
		s.m_bEnableFleeSmoke			= m_bEnableFleeSmoke;
		s.m_bEnableAdaptiveFormation	= m_bEnableAdaptiveFormation;

		s.m_bEnableVehicleArmor			= m_bEnableVehicleArmor;
		s.m_bEnableVehicleCombat		= m_bEnableVehicleCombat;
		s.m_bEnableVehicleDoctrine		= m_bEnableVehicleDoctrine;
		s.m_fVehDoctrineSectionRadius	= m_fVehDoctrineSectionRadius;
		s.m_fVehDoctrineHandbackSec		= m_fVehDoctrineHandbackSec;
		s.m_bVehDoctrineBounding		= m_bVehDoctrineBounding;
		s.m_bVehDoctrineDismountStuck	= m_bVehDoctrineDismountStuck;
		s.m_bVehDoctrineRecovery		= m_bVehDoctrineRecovery;
		s.m_bDebugVehDoctrine			= m_bDebugVehDoctrine;
		s.m_bEnableCqbClear		= m_bEnableCqbClear;
		s.m_bCqbClearMarkedOnly	= m_bCqbClearMarkedOnly;
		s.m_fCqbClearRadius		= m_fCqbClearRadius;
		s.m_fCqbNodeDwellSec	= m_fCqbNodeDwellSec;
		s.m_iCqbMaxBuildings	= m_iCqbMaxBuildings;
		s.m_bDebugCqbClear		= m_bDebugCqbClear;
		s.m_bVehicleBackoff				= m_bVehicleBackoff;
		s.m_fVehicleBackoffHoldSec		= m_fVehicleBackoffHoldSec;
		s.m_bEnableVehicleHijack		= m_bEnableVehicleHijack;
		s.m_bEnableHVTTargeting			= m_bEnableHVTTargeting;
		s.m_bEnableVehicleReinforcement	= m_bEnableVehicleReinforcement;
		s.m_bEnableFleeFromArmor		= m_bEnableFleeFromArmor;
		s.m_bEnableSafeEjectGate		= m_bEnableSafeEjectGate;

		// --- Tactical-movement store ---
		DCO_TacticalMoveSettings t = DCO_TacticalMoveSettings.Get();
		t.m_bEnableTacticalMove			= m_bEnableTacticalMove;
		t.m_bAvoidThreatFunnel			= m_bAvoidThreatFunnel;
		t.m_bEnableFlanking				= m_bEnableFlanking;
		t.m_bEnableExposureScoring		= m_bEnableExposureScoring;
		t.m_bEnableProceduralPath		= m_bEnableProceduralPath;
		t.m_bEnableTravelingOverwatch	= m_bEnableTravelingOverwatch;
		t.m_bEnableBaseOfFireSplit		= m_bEnableBaseOfFireSplit;
		t.m_bPathHaltOnContact			= m_bPathHaltOnContact;
		t.m_fPathContactHaltRange		= m_fPathContactHaltRange;
		t.m_bPathLeaderInBase			= m_bPathLeaderInBase;
		t.m_bPathBaseMaintainLos		= m_bPathBaseMaintainLos;
		t.m_bEnableCoverSeek			= m_bEnableCoverSeek;
		t.m_bEnableReactToContact		= m_bEnableReactToContact;
		t.m_bEnableTacticalBrain		= m_bEnableTacticalBrain;
		t.m_bEnableStandoff				= m_bEnableStandoff;
		t.m_bEnableDcoFormations		= m_bEnableDcoFormations;
	}
}
