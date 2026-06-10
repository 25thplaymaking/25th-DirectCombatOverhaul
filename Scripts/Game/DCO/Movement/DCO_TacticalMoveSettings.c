// Tactical-movement settings store (singleton). Tunables for the "smarter maneuver" layer: when a
// group is given a move order it should weigh each leg for cover/concealment rather than blindly
// pathing straight to the destination, inserting non-visible "check-in" sub-objectives to
// observe / halt / take cover / bound.
//
// The store + GM attributes exist and are server-applied; the movement behaviour that consumes them is
// built incrementally (verify, cover bias, bounding, path scoring). m_bEnableTacticalMove defaults OFF
// so nothing changes on a live server until a Game Master enables it. Players are never affected (only
// AI run move activities).
//
// Lazily-created singleton, mirrors DCO_MoraleSettings.
class DCO_TacticalMoveSettings
{
	protected static ref DCO_TacticalMoveSettings s_Instance;

	// Master switch for the whole tactical-movement layer.
	bool	m_bEnableTacticalMove		= false;

	// Only moves at least this far (m) are treated as "tactical" and broken into check-in legs;
	// short repositions path normally so close-quarters movement stays responsive.
	float	m_fMinMoveDistance			= 80.0;

	// Spacing (m) between non-visible check-in sub-objectives along a tactical move.
	float	m_fCheckInInterval			= 60.0;

	// How long (s) the group pauses to observe/scan at each check-in before bounding onward.
	float	m_fObservePause				= 3.0;

	// Threat gate: the group must be under at least this much threat (engine threat measure, ~0..1)
	// for the deliberate approach to engage. Keeps routine/peaceful patrols at normal pace. Internal
	// sensitivity knob (not a GM slider yet); 0.10 = any light contact.
	float	m_fThreatActivation			= 0.10;

	// --- ANTI-FUNNEL ---
	// When a known threat sits in the corridor between the group and a distant move destination, the
	// approach is side-stepped off the enemy axis so the group doesn't path straight up the kill-zone
	// (e.g. a road that runs at the enemy = "fatal funnel"). Approximation of true cover-routing: it
	// nudges the destination laterally, so the group flanks rather than charging the funnel.
	// CRX re-paths on contact. Default OFF - GM opt-in, server-side, players never affected.
	bool	m_bAvoidThreatFunnel		= false;
	float	m_fFunnelSidestep			= 45.0;	// metres to offset the approach away from the enemy axis
	float	m_fFunnelCorridorHalfWidth	= 30.0;	// threat counts as "in the funnel" within this of the path

	// --- FLANKING MANEUVER ---
	// When a group moves toward a known enemy (the destination is near the perceived threat), approach from
	// the enemy's flank instead of straight down its front: the approach vector is rotated around the enemy
	// by m_fFlankAngleDeg, picking the side that is less line-of-sight exposed. This is the DCO-native
	// flanking that replaces relying on Skarris. Group-level (whole group flanks as one) - reliable, no
	// fireteam split / cohesion risk. CRX still does the in-contact cover-to-cover once the group arrives.
	// Default OFF, server-side, GM opt-in, players never affected. A two-element base-of-fire + maneuver
	// split via m_FireteamMgr is a heavier future step.
	bool	m_bEnableFlanking			= false;
	float	m_fFlankAngleDeg			= 55.0;	// how far around the enemy to swing the approach (deg)
	float	m_fFlankMaxEnemyDist		= 250.0;	// only flank when the destination is within this of the known enemy (m)
	float	m_fFlankMinEnemyDist		= 25.0;	// don't bother flanking if already basically on top of the enemy (m)

	// --- EXPOSURE SCORING ---
	// Pick the least line-of-sight-exposed of several candidate move destinations, via world raycasts
	// (BaseWorld.TraceMove): if the threat can't see a candidate (something blocks the line), prefer it.
	// Uses the int LayerMask (no enum), so it always compiles; a wrong/zero layer simply degrades to
	// keeping the original destination (safe no-op). Default OFF.
	bool	m_bEnableExposureScoring	= false;
	int		m_iExposureLayerMask		= 0;	// physics layer bitmask for the LOS trace (0 = engine default; tune in-engine)
	float	m_fExposureSidestep			= 30.0;	// lateral offset (m) of the alternative covered candidates

	// --- PROCEDURAL TACTICAL PATH ---
	// Re-decides the route as the group moves: while under a danger state it samples a forward cone, scores
	// candidates by concealment + cover + progress, snaps onto navmesh, and bounds leg-by-leg through cover
	// to the goal (path of least resistance). Debug draws the live, self-adjusting path. Default OFF.
	bool	m_bEnableProceduralPath		= false;
	bool	m_bPathDebugDraw			= false;	// draw the live path arrows (also shown when m_bDebug is on)
	float	m_fPathReassessSec			= 3.0;		// how often the route is re-decided as the group moves
	float	m_fPathLegLength			= 25.0;		// length (m) of each bound / leg
	float	m_fPathConeHalfAngleDeg		= 50.0;		// half-angle of the forward sample cone toward the goal
	int		m_iPathConeSamples			= 5;		// candidate directions sampled across the cone
	float	m_fPathArriveDist			= 8.0;		// within this of the goal = arrived (path cleared)
	int		m_iPathMaxLegs				= 8;		// max legs planned ahead (also the debug preview length)
	float	m_fPathThreatActivation		= 0.10;		// only procedurally re-route under at least this threat (danger gate)
	float	m_fPathConcealmentBonus		= 40.0;		// score bonus for a leg concealed from the threat
	float	m_fPathStraightBias			= 10.0;		// penalty for veering off the straight line to the goal

	// TRAVELING OVERWATCH (movement technique, FM 3-90 2-19): contact possible - a lead element advances while a
	// trailing element follows a short distance back, ready to overwatch/support. The middle gear between plain
	// traveling (whole unit, light danger) and bounding overwatch (heavy danger). On-foot infantry; default OFF.
	bool	m_bEnableTravelingOverwatch		= false;
	float	m_fPathTravelOverwatchThreat	= 0.30;	// threat at/above which traveling overwatch engages (below it = plain traveling); bounding takes over at m_fPathSplitThreatActivation

	// BOUNDING OVERWATCH / base-of-fire split: under heavy danger the leader peels a base-of-fire
	// element that holds + overwatches while the maneuver element bounds to the next cover leg, alternating
	// roles each reassessment (leapfrog). On-foot infantry only. Below the heavy threshold the whole unit
	// bounds together. Default OFF.
	bool	m_bEnableBaseOfFireSplit	= false;
	float	m_fPathSplitThreatActivation = 0.5;		// threat at/above which the split engages (heavy danger); below it the whole unit bounds

	// --- LIFE PRESERVATION ON CONTACT (keeps AI from pushing unwinnable advances) ---
	// Contact takes precedence over the tactical move: the moment the group perceives an enemy within
	// m_fPathContactHaltRange it halts the advance and fights from where it is, instead of bounding forward into
	// fire (which gets the element - and especially the leader - killed and saps morale). The route resumes once
	// contact is broken. Overrides the move regardless of how far the objective is.
	bool	m_bPathHaltOnContact		= true;
	float	m_fPathContactHaltRange		= 200.0;	// perceived enemy within this (m) = decisive contact, halt the advance
	// In bounding overwatch, keep the leader's element as the static base of fire (overwatch) so the leader is
	// never the one bounding into the kill zone. The other element maneuvers.
	bool	m_bPathLeaderInBase			= true;
	// The base of fire adjusts to stay useful: a base member with no line of sight to the threat repositions a
	// short distance to regain it, so the overwatch element doesn't go blind when the maneuver element moves.
	bool	m_bPathBaseMaintainLos		= true;
	float	m_fPathBaseLosRadius		= 15.0;		// how far (m) a base member repositions to regain line of sight

	// --- COVER-AWARE PLACEMENT ---
	// While in contact, members standing exposed to the threat are nudged to the nearest concealed, walkable
	// spot (ring sample + LOS raycast + navmesh snap) - "place AI in cover" over CRX's formation slots.
	// On-foot infantry; server-side; default OFF.
	bool	m_bEnableCoverSeek		= false;
	float	m_fCoverSeekCheckSec	= 5.0;	// how often cover placement is re-evaluated
	float	m_fCoverSeekRadius		= 12.0;	// search radius (m) for cover near each exposed member
	int		m_iCoverSeekSamples		= 6;	// directions sampled in the ring around each member
	// Anti-attic-funnel: a cover/path candidate may not be more than this far above the unit's current height,
	// and the navmesh snap is kept vertically tight - so AI take cover on their current floor instead of
	// climbing to the most-concealed (highest) spot until they bunch in the attic. ~floor height.
	float	m_fCoverMaxClimb		= 2.0;

	// --- REACTION TO CONTACT (FM 3-90 actions on contact) ---
	// When in contact, execute a course of action (chosen by the brain): support-by-fire (boost
	// volume of fire + hold), assault an assailable flank (envelopment via the path layer), or break contact
	// (move away). Server-side; on-foot drives; default OFF.
	bool	m_bEnableReactToContact		= false;
	float	m_fReactCheckSec			= 2.0;	// how often the contact drill re-evaluates
	float	m_fSupportFireRateCoef		= 1.3;	// fire-rate multiplier for the base-of-fire / support-by-fire volume
	float	m_fAssaultFlankAngleDeg		= 60.0;	// angle the assault swings around the enemy onto its flank (envelopment)

	// --- STANDOFF (minimum engagement distance / no suicide-charge) ---
	// AI keep beyond a weapon-aware minimum distance from the enemy in open-ground fights instead of closing to
	// knife range. Hooks SCR_AIFindFirePositionBehavior.InitParameters (DCO_Standoff): raises its minDistance to
	// the floor for the agent's weapon, keeping a valid engagement band. Yields (no floor) while the group is
	// committed to an assault (EDCO_COA.ASSAULT_FLANK) or actively clearing a building (recent CQB order), so it
	// never makes the AI passive or stops them clearing rooms. Default OFF; server-side; players unaffected.
	bool	m_bEnableStandoff			= true;		// ON by default (Bryce, 2026-06-04) - deliberate exception to the usual OFF-by-default rule
	float	m_fStandoffRifle			= 35.0;		// min engagement distance (m) for riflemen / default weapons
	float	m_fStandoffMG				= 80.0;		// min engagement distance (m) for machine gunners (base of fire, stay back)
	float	m_fStandoffSniper			= 120.0;	// min engagement distance (m) for marksmen / snipers
	float	m_fStandoffLauncher			= 60.0;		// min engagement distance (m) for rocket/AT launcher troops
	float	m_fStandoffPistol			= 12.0;		// min engagement distance (m) for sidearm-only (last-ditch, allowed close)
	float	m_fStandoffBand				= 25.0;		// min valid engagement window (m) kept above the floor - only bumps the engine's max when it was tighter than floor+band; kept small so short-range troops aren't nudged to engage from absurd range
	float	m_fBreakContactDistance		= 100.0;	// distance (m) to withdraw when breaking contact

	// --- TACTICAL BRAIN / COA SELECTION (maneuver warfare) ---
	// Chooses the course of action by relative combat power + morale: assault a flank when stronger, support
	// by fire when even, break contact when overmatched/broken. Feeds the reaction-to-contact drill.
	// Server-side; default OFF.
	bool	m_bEnableTacticalBrain		= false;
	float	m_fBrainCheckSec			= 3.0;	// how often the COA is re-evaluated
	float	m_fBrainAssaultPowerRatio	= 1.3;	// own:enemy strength ratio at/above which the brain chooses ASSAULT_FLANK
	float	m_fBrainBreakPowerRatio		= 0.5;	// own:enemy strength ratio at/below which the brain chooses BREAK_CONTACT
	float	m_fBrainEnemyScanRadius		= 200.0;	// radius (m) to count enemy strength around the contact

	// --- DCO FORMATION SHAPES ---
	// Impose echelon / vee / dispersion the native SetFormation set can't, via per-member offsets from the
	// leader (oriented to heading, or to the threat in contact), scaled by spacing. Experimental: fights CRX's
	// own formation; alternative to the SetFormation selector. On-foot; server-side; default OFF.
	// Shape ints: 0 column, 1 wedge, 2 line, 3 echelon-left, 4 echelon-right, 5 vee.
	bool	m_bEnableDcoFormations		= false;
	float	m_fFormationShapeCheckSec	= 5.0;	// how often the shape is re-imposed
	float	m_fFormationSpacing			= 5.0;	// metres between ranks/files (dispersion)
	int		m_iFormationTravelShape		= 1;	// shape when not in contact (default wedge)
	int		m_iFormationContactShape	= 2;	// shape when in contact (default line)

	// --- VALIDATION-build diagnostics (off for the clean release) ---
	// Logs each tactical-move decision to the console so we can confirm the InitParameters hook fires
	// and see threat/distance inputs.
	bool	m_bDebugLog					= false;
	// Test: ignore the threat gate so the deliberate WALK is unmistakably visible while validating
	// (still requires the layer enabled + a long RUN-to-position). Set false to restore the threat gate.
	bool	m_bForceIgnoreThreat		= false;

	static DCO_TacticalMoveSettings Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_TacticalMoveSettings();
		return s_Instance;
	}
}
