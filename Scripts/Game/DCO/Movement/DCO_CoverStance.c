// Adjust-to-cover stance fitting. Probe the actual cover in front of a member: while in contact,
// raytrace from the threat's eye to the member at three body heights (standing / crouched / prone). The
// member then adopts the lowest stance that is covered (line of sight blocked) - so tall cover lets
// them fight standing, a low wall drops them to a crouch, and a mound/log puts them prone. If nothing
// blocks even the prone line, there is no cover to fit and we leave the member to the normal combat AI.
//
// Cooperates with DCO_StanceCooldown (which de-jitters stance-change requests): this is throttled
// (m_fCoverStanceCheckSec) and only fires while the group is in contact, so it nudges a settled firing
// stance rather than fighting per-frame movement. Uses the same ForceStance lever as the
// morale/surrender code.
//
// Another `modded SCR_AIGroupUtilityComponent` fragment in Movement/ (sorts before the QRF
// EvaluateActivity tick that calls DCO_UpdateCoverStance). On-foot infantry; server-authoritative;
// default OFF.
//
// Validate in-engine: forcing a stance on a member that is actively repositioning could briefly root
// it; the throttle + contact gate limit this, but tune m_fCoverStanceCheckSec if AI look sticky.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastCoverStanceTime	= -1;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Fits each in-contact member's stance to
	// the cover between it and the nearest threat.
	void DCO_UpdateCoverStance()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableCoverStance || !m_Owner || !m_Perception)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastCoverStanceTime >= 0 && (now - m_fDCO_LastCoverStanceTime) < cfg.m_fCoverStanceCheckSec * 1000.0)
			return;
		m_fDCO_LastCoverStanceTime = now;

		// Nearest perceived enemy = the threat we fit cover against.
		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector lead = leader.GetOrigin();
		vector threat;
		bool hasThreat = false;
		float bestSq = 1000000000.0;
		foreach (IEntity t : targets)
		{
			if (!t)
				continue;
			float d = vector.DistanceSq(t.GetOrigin(), lead);
			if (d < bestSq)
			{
				bestSq = d;
				threat = t.GetOrigin();
				hasThreat = true;
			}
		}
		if (!hasThreat)
			return;

		vector threatEye = threat + Vector(0, cfg.m_fCoverStandEye, 0);

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			vector pos = ent.GetOrigin();

			// Only fit stance for members in the close fight where cover height matters.
			if (vector.DistanceSq(pos, threat) > cfg.m_fCoverStanceMaxRange * cfg.m_fCoverStanceMaxRange)
				continue;

			AIPathfindingComponent pf = AIPathfindingComponent.Cast(ent.FindComponent(AIPathfindingComponent));
			if (!pf)
				continue;

			bool standCovered = DCO_CoverLosBlocked(pf, threatEye, pos, cfg.m_fCoverStandEye);
			bool crouchCovered = DCO_CoverLosBlocked(pf, threatEye, pos, cfg.m_fCoverCrouchEye);
			bool proneCovered = DCO_CoverLosBlocked(pf, threatEye, pos, cfg.m_fCoverProneEye);

			int desired = -1;	// -1 = no cover, leave alone
			if (standCovered)
				desired = 0;		// STAND
			else if (crouchCovered)
				desired = 1;		// CROUCH
			else if (proneCovered)
				desired = 2;		// PRONE

			if (desired < 0)
				continue;

			// Route through the shared throttle: no-op if already in this stance, and at most one transition
			// per member per m_fStanceCooldownSec - kills the instant re-snap + the prone/up war with CRX.
			ECharacterStance want = ECharacterStance.PRONE;
			if (desired == 0)
				want = ECharacterStance.STAND;
			else if (desired == 1)
				want = ECharacterStance.CROUCH;

			DCO_StanceUtil.TrySetStance(ent, want, cfg.m_fStanceCooldownSec * 1000.0);
		}

		if (cfg.m_bDebug)
			DCO_Debug.LogGroup("COVERSTANCE", leader, "fitted member stances to cover");
	}

	// True if the line from the threat's eye to the member's body AT height h is blocked (member is covered
	// at that stance height).
	protected bool DCO_CoverLosBlocked(AIPathfindingComponent pf, vector threatEye, vector memberPos, float h)
	{
		vector hit;
		return pf.RayTrace(threatEye, memberPos + Vector(0, h, 0), hit);
	}
}
