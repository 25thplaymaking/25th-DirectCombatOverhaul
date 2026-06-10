// Real suppression. The engine already tracks a per-AI suppression measure
// (SCR_AIThreatSystem.GetSuppressionMeasure, fed by bullet-impact / danger events) and has
// suppressed-in-cover combat-move logic, but in practice AI often shrug it off. This
// amplifies the reaction: each tick it reads every member's suppression measure and, for
// those above a threshold, degrades their fire (heads-down fire-rate drop) and seeks cover /
// breaks line of sight. When a large fraction of the group is pinned it pops smoke once to
// screen them. Fire is restored when the measure falls. Reads the threat system via
// SCR_ChimeraAIAgent.m_UtilityComponent.
//
// modded SCR_AIGroupUtilityComponent fragment in Morale/. Sorts after DCO_GroupMorale.c so it
// can reuse DCO_GetThreatOrLastPosition + DCO_DeploySmoke, and before the QRF tick that calls
// DCO_UpdateSuppression(). On-foot, server-only, default OFF.
//
// The numeric range of GetSuppressionMeasure() is runtime-unknown: turn on m_bDebug, watch
// [DCO:SUPPRESS], and tune m_fSuppressionThreshold accordingly.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastSuppressionTime	= -1;
	protected bool	m_bDCO_SuppressionSmokeUp	= false;

	void DCO_UpdateSuppression()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableSuppression || !m_Owner || !m_Mailbox)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;	// vehicle crews handled elsewhere

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastSuppressionTime >= 0 && (now - m_fDCO_LastSuppressionTime) < cfg.m_fSuppressionCheckSec * 1000.0)
			return;
		m_fDCO_LastSuppressionTime = now;

		vector threatPos;
		bool hasThreat = DCO_GetThreatOrLastPosition(threatPos);	// defined in DCO_GroupMorale.c (sorts first)

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		AICommunicationComponent comms = m_Mailbox;

		int total = 0;
		int suppressed = 0;

		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never suppress/seek-cover a player
			total++;

			SCR_ChimeraAIAgent chim = SCR_ChimeraAIAgent.Cast(a);
			if (!chim || !chim.m_UtilityComponent || !chim.m_UtilityComponent.m_ThreatSystem)
				continue;

			float supp = chim.m_UtilityComponent.m_ThreatSystem.GetSuppressionMeasure();
			bool isSupp = supp >= cfg.m_fSuppressionThreshold;

			// Degrade / restore fire effectiveness (heads down while pinned).
			SCR_AICombatComponent combat = chim.m_UtilityComponent.m_CombatComponent;
			if (combat)
			{
				if (isSupp)
					combat.SetFireRateCoef(cfg.m_fSuppressedFireRateCoef, false);
				else
					combat.SetFireRateCoef(1.0, false);
			}

			if (isSupp)
			{
				suppressed++;
				if (cfg.m_bSuppressionSeekCover && hasThreat)
					DCO_SuppSeekCover(a, ent, threatPos, comms, cfg);
			}
		}

		// Group smoke screen when a large fraction is pinned (once until the pin clears). DCO_DeploySmoke
		// self-no-ops if no member carries smoke, so this is safe to call unconditionally.
		if (total > 0 && hasThreat)
		{
			float frac = suppressed / (float)total;
			if (frac >= cfg.m_fSuppressionSmokeFraction)
			{
				if (!m_bDCO_SuppressionSmokeUp)
				{
					DCO_DeploySmoke(threatPos);	// defined in DCO_GroupMorale.c (sorts first)
					m_bDCO_SuppressionSmokeUp = true;
				}
			}
			else
			{
				m_bDCO_SuppressionSmokeUp = false;
			}
		}

		if (suppressed > 0)
			DCO_Debug.LogGroup("SUPPRESS", m_Owner.GetLeaderEntity(), string.Format("%1/%2 pinned (threshold %3)", suppressed, total, cfg.m_fSuppressionThreshold));
	}

	// Pinned-and-exposed member: prefer to dig in. Only dash to cover that is close and doesn't
	// give up the high ground; if no such cover is a short dash away, go prone in place and
	// return fire instead of sprinting across the open / downhill. Skips if already in cover.
	// Per-member order.
	protected void DCO_SuppSeekCover(AIAgent a, IEntity ent, vector threatPos, AICommunicationComponent comms, DCO_MoraleSettings cfg)
	{
		AIPathfindingComponent pf = AIPathfindingComponent.Cast(ent.FindComponent(AIPathfindingComponent));
		vector pos = ent.GetOrigin();

		if (DCO_SuppRayBlocked(pf, threatPos, pos))
			return;	// already in cover/concealment - hold here

		// In dig-in mode, only look for cover within a SHORT dash so AI don't sprint across open ground.
		float searchRadius = cfg.m_fSuppressionCoverRadius;
		if (cfg.m_bSuppressionDigIn && cfg.m_fSuppressionDashMax < searchRadius)
			searchRadius = cfg.m_fSuppressionDashMax;

		vector best;
		bool found = false;
		float bestSq = 0;
		float maxClimb = DCO_TacticalMoveSettings.Get().m_fCoverMaxClimb;
		for (int i = 0; i < 8; i++)
		{
			float ang = (i / 8.0) * 6.2831853;
			vector cand = pos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * searchRadius;
			DCO_SuppSnapNav(pf, cand, cand);
			if (cand[1] > pos[1] + maxClimb)
				continue;	// stay on this floor - don't dash up to the attic
			if (cfg.m_bSuppressionDigIn && cand[1] < pos[1] - cfg.m_fSuppressionMaxDescend)
				continue;	// don't abandon the high ground / drop into a hole below the position
			if (!DCO_SuppRayBlocked(pf, threatPos, cand))
				continue;	// candidate must actually break LOS to the threat
			float d = vector.DistanceSq(cand, pos);
			if (!found || d < bestSq)
			{
				bestSq = d;
				best = cand;
				found = true;
			}
		}

		if (found)
		{
			// Close, position-holding cover exists: short bound to it.
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, best, EMovementType.SPRINT, false, null);
			if (msg)
			{
				msg.SetReceiver(a);
				comms.RequestBroadcast(msg, a);
			}
		}
		else if (cfg.m_bSuppressionDigIn)
		{
			// No close cover that holds the position: dig in, hit the dirt and fight from here
			// rather than running into the open to look for cover.
			DCO_StanceUtil.TrySetStance(ent, ECharacterStance.PRONE, cfg.m_fStanceCooldownSec * 1000.0);
		}
	}

	protected bool DCO_SuppRayBlocked(AIPathfindingComponent pf, vector threatPos, vector pos)
	{
		if (!pf)
			return false;
		vector eye = Vector(0, 1.5, 0);
		vector hit;
		return pf.RayTrace(threatPos + eye, pos + eye, hit);
	}

	protected void DCO_SuppSnapNav(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;
		vector corrected;
		// Vertically tight (±2 m) so a cover dash stays on this floor, not up to the attic.
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(6, 2, 6), corrected))
			outPos = corrected;
	}
}
