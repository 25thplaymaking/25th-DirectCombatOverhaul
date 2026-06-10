// AI stance-transition cooldown (anti-jitter / anti-"matrix-dodge"). Stop AI from rapidly cycling
// prone/crouch/stand on the spot ("twitch" stance spam) and the "matrix dodge" (up one instant, prone
// the next) when reacting to incoming fire - without making them sluggish about genuinely taking cover.
//
// Mechanism (read before changing): the character's native stance call
// (CharacterControllerComponent.SetStanceChange / ForceStance) is `proto external` and cannot be
// overridden in script (doing so fails to compile with "Multiple declaration" and kills the whole Game
// module). Instead we hook the scripted layer that decides stance: the AI combat-move request queue.
// The behaviour tree pushes typed requests into SCR_AICombatMoveState.ApplyNewRequest(...), a scripted
// (overridable) method, and only that path leads to the native stance change.
//
// The "matrix dodge": the snap-to-prone when shot from an unseen source (behind a hill / cover) is the
// danger reaction in SCR_AICombatMoveLogic_HideFromUnknownFire, which calls
// PushRequesChangeStanceInCover()/PushRequestChangeStanceOutsideCover() - a ChangeStance or
// ChangeStanceInCover request through this method. It predates DCO (it's CRX/Skarris behaviour; those
// are packed and unreadable here, so they can't be patched directly). The lever we have is to throttle
// how often these stance flips are allowed, which is what this cooldown does. For the unseen-fire snap
// specifically, the request is usually the in-cover variant, so enable m_bStanceCooldownInCover.
//
// By default we throttle only the free-standing stance flip (SCR_AICombatMoveRequest_ChangeStance).
// SCR_AICombatMoveRequest_ChangeStanceInCover (rapid cover peek/duck) is added to the throttle only
// when m_bStanceCooldownInCover is ON - it was excluded by default because interrupting the cover-pose
// transition can shudder torso gear physics. SCR_AICombatMoveRequest_Move (carries its own stance) is
// never touched, so moving / repositioning / getting up to run to cover is never penalised.
//
// Players are never affected: only AI agents own a SCR_AICombatMoveState.
//
// Opt-in and live-server-safe: gated on DCO_MoraleSettings.m_bEnableStanceCooldown (default OFF).
// m_bStanceCooldownDebug (default OFF) logs every stance request that reaches this method (type /
// reason / target stance / moving / throttled) so the exact matrix-dodge path can be identified.
modded class SCR_AICombatMoveState
{
	// World time (ms) of the last stance flip we let through, per combat-move state (per AI).
	protected float m_fDCO_LastStanceChangeMs;
	// Last free-standing target stance (ECharacterStance as int) we let through; -1 = none yet. Used to damp
	// the up/prone metronome: reversing a recent change costs extra settle time instead of flipping forever.
	protected int m_iDCO_LastStance = -1;

	override void ApplyNewRequest(notnull SCR_AICombatMoveRequestBase request)
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();

		if (cfg && cfg.m_bStanceCooldownDebug && Replication.IsServer())
			DCO_LogStanceRequest(request);

		BaseWorld world = GetGame().GetWorld();
		if (world && Replication.IsServer() && cfg && cfg.m_bEnableStanceCooldown && !IsMoving() && DCO_IsThrottledStanceRequest(request))
		{
			float now = world.GetWorldTime(); // ms
			float cdMs = cfg.m_fStanceCooldownSec * 1000.0;

			SCR_AICombatMoveRequest_ChangeStance cs = SCR_AICombatMoveRequest_ChangeStance.Cast(request);
			if (cs)
			{
				int target = cs.m_eStance;

				// Re-requesting the stance we last let through is a no-op flip: pass it without restarting the
				// clock, so a steady desired stance can't feed the metronome.
				if (target == m_iDCO_LastStance)
				{
					super.ApplyNewRequest(request);
					return;
				}

				// A genuine change. A reversal (different target than the one just accepted) must wait double the
				// cooldown - that extra settle is what breaks the endless prone/up oscillation when two
				// behaviours disagree. The first change of a settled member only waits the normal cooldown.
				float required = cdMs;
				if (m_iDCO_LastStance >= 0)
					required = cdMs * 2.0;

				if (now - m_fDCO_LastStanceChangeMs < required)
					return; // within the settle window: drop this flip

				m_fDCO_LastStanceChangeMs = now;
				m_iDCO_LastStance = target;
				super.ApplyNewRequest(request);
				return;
			}

			// In-cover variant (only throttled when m_bStanceCooldownInCover): no explicit target stance, so keep
			// the simple time throttle.
			if (now - m_fDCO_LastStanceChangeMs < cdMs)
				return;
			m_fDCO_LastStanceChangeMs = now;
		}

		super.ApplyNewRequest(request);
	}

	// True for the stance flips this feature throttles. By default only the free-standing
	// SCR_AICombatMoveRequest_ChangeStance (prone/crouch/stand cycling). When m_bStanceCooldownInCover
	// is ON, the in-cover peek/duck (SCR_AICombatMoveRequest_ChangeStanceInCover) is also throttled -
	// that's the lever for the unseen-fire snap-to-prone. SCR_AICombatMoveRequest_Move is never throttled.
	protected bool DCO_IsThrottledStanceRequest(SCR_AICombatMoveRequestBase request)
	{
		if (SCR_AICombatMoveRequest_ChangeStance.Cast(request) != null)
			return true;

		if (DCO_MoraleSettings.Get().m_bStanceCooldownInCover && SCR_AICombatMoveRequest_ChangeStanceInCover.Cast(request) != null)
			return true;

		return false;
	}

	// Diagnostic: print the kind of stance request arriving + whether it would be throttled. Reads the
	// target stance for a free-standing ChangeStance (it carries m_eStance); the in-cover variant has no
	// explicit target stance. m_eReason is the combat-move reason (e.g. the hide-from-fire reaction).
	protected void DCO_LogStanceRequest(SCR_AICombatMoveRequestBase request)
	{
		string kind = "Other";
		string stance = "-";

		SCR_AICombatMoveRequest_ChangeStance cs = SCR_AICombatMoveRequest_ChangeStance.Cast(request);
		if (cs)
		{
			kind = "ChangeStance";
			stance = cs.m_eStance.ToString();
		}
		else if (SCR_AICombatMoveRequest_ChangeStanceInCover.Cast(request))
		{
			kind = "ChangeStanceInCover";
		}

		Print(string.Format("[DCO Stance] kind=%1 stance=%2 reason=%3 moving=%4 throttledType=%5",
			kind, stance, request.m_eReason, IsMoving(), DCO_IsThrottledStanceRequest(request)), LogLevel.NORMAL);
	}
}
