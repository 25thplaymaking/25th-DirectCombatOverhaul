// Machine-gunner emplacement. An MG is a static base of fire, not a rifleman, but by
// default CRX maneuvers MG troops around like everyone else and wastes the weapon. When
// the group is in contact, a member carrying a machine gun sets up properly:
//   - Has LOS to the enemy: deploy - go prone and hold this firing position (the hold
//     order at his own spot stops CRX walking him off the gun) for sustained suppression.
//   - No LOS: reposition a short distance to the nearest spot that does see the enemy,
//     preferring slightly higher ground, then deploy there next tick.
//
// MG detection is GetCurrentWeaponType() == EWeaponType.WT_MACHINEGUN. Per-member, reuses
// the combat-component access path from suppression/AT.
//
// modded SCR_AIGroupUtilityComponent fragment ("MachineGun" sorts before the QRF tick).
// On-foot, server-only, throttled, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float	m_fDCO_LastMGTime	= -1;

	// Per-gunner emplacement state, keyed by the gunner's controlled entity. Both are
	// map<IEntity,float> where presence is what matters; the float is just a timestamp.
	//   m_mDCO_MGDeployed  : gunner is emplaced (prone, holding).
	//   m_mDCO_MGMoveSince : gunner is en route to a firing position - value = move time.
	// Together they give the carrier a small state machine so it doesn't re-issue
	// prone+hold every tick (the "repeatedly lies down" metronome) and paths to a firing
	// position first, only emplacing once it actually has LOS there.
	protected ref map<IEntity, float>	m_mDCO_MGDeployed;
	protected ref map<IEntity, float>	m_mDCO_MGMoveSince;

	// While en route, don't re-pick a firing position within this long of the last move
	// order, so he actually walks to the spot instead of being re-ordered every tick.
	protected static const float DCO_MG_REPATH_MS = 4000.0;

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity). Deploys MG carriers
	// onto firing positions.
	void DCO_UpdateMachineGunner()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableMachineGunner || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastMGTime >= 0 && (now - m_fDCO_LastMGTime) < cfg.m_fMGCheckSec * 1000.0)
			return;
		m_fDCO_LastMGTime = now;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
		{
			// Contact over: forget the emplacements so the gunners un-stick and rejoin
			// (CRX resumes control); they re-evaluate from scratch on the next contact.
			DCO_MGClearAll();
			return;
		}

		if (!m_mDCO_MGDeployed)
			m_mDCO_MGDeployed = new map<IEntity, float>();
		if (!m_mDCO_MGMoveSince)
			m_mDCO_MGMoveSince = new map<IEntity, float>();

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			SCR_ChimeraAIAgent chim = SCR_ChimeraAIAgent.Cast(a);
			if (!chim || !chim.m_UtilityComponent || !chim.m_UtilityComponent.m_CombatComponent)
				continue;

			// MG carriers only.
			if (chim.m_UtilityComponent.m_CombatComponent.GetCurrentWeaponType() != EWeaponType.WT_MACHINEGUN)
				continue;

			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			vector pos = ent.GetOrigin();

			// Nearest perceived enemy to this gunner.
			vector enemy;
			bool hasEnemy = false;
			float bestSq = 1000000000.0;
			foreach (IEntity t : targets)
			{
				if (!t)
					continue;
				float d = vector.DistanceSq(t.GetOrigin(), pos);
				if (d < bestSq)
				{
					bestSq = d;
					enemy = t.GetOrigin();
					hasEnemy = true;
				}
			}
			if (!hasEnemy)
				continue;

			AIPathfindingComponent pf = AIPathfindingComponent.Cast(ent.FindComponent(AIPathfindingComponent));
			float eye = cfg.m_fMGSightHeight;
			bool hasLos = !DCO_MGLosBlocked(pf, pos, enemy, eye);

			// Already emplaced: just hold. Keep him prone (the stance util is throttled, so
			// no spam) and don't re-issue the hold-move every tick. Only break the
			// emplacement if the enemy moves out of sight, then drop through to re-seek.
			if (m_mDCO_MGDeployed.Contains(ent))
			{
				if (hasLos)
				{
					DCO_StanceUtil.TrySetStance(ent, ECharacterStance.PRONE, cfg.m_fStanceCooldownSec * 1000.0);
					continue;
				}
				m_mDCO_MGDeployed.Remove(ent);	// lost LOS - tear down and find a new firing position
			}

			// Has LOS (he was already on a good spot, or just arrived at the ordered firing
			// position): emplace now (one hold + prone) and latch it so we stop re-issuing.
			if (hasLos)
			{
				DCO_MGDeploy(a, ent, pos);
				m_mDCO_MGDeployed.Set(ent, now);
				m_mDCO_MGMoveSince.Remove(ent);
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("MG", ent, "emplaced - holding firing position");
				continue;
			}

			// No LOS: path to a firing position first, then emplace next tick on arrival.
			if (!cfg.m_bMGReposition)
				continue;

			// Recently ordered to a firing position? Let him walk there instead of re-picking it every tick.
			float lastMove;
			if (m_mDCO_MGMoveSince.Find(ent, lastMove) && (now - lastMove) < DCO_MG_REPATH_MS)
				continue;

			vector firePos;
			if (DCO_MGFindFiringPos(pf, pos, enemy, eye, cfg, firePos))
			{
				SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, firePos, EMovementType.RUN, false, null);
				if (msg)
				{
					msg.SetReceiver(a);
					m_Mailbox.RequestBroadcast(msg, a);
				}
				m_mDCO_MGMoveSince.Set(ent, now);
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("MG", ent, "pathing to firing position");
			}
		}
	}

	// Drop all emplacement state (contact ended). Gunners go back under CRX control.
	protected void DCO_MGClearAll()
	{
		if (m_mDCO_MGDeployed)
			m_mDCO_MGDeployed.Clear();
		if (m_mDCO_MGMoveSince)
			m_mDCO_MGMoveSince.Clear();
	}

	// Deploy a gunner: hold his current position (so CRX doesn't maneuver him off the gun)
	// and go prone. Issued once on the deploy transition, not every tick.
	protected void DCO_MGDeploy(AIAgent a, IEntity ent, vector pos)
	{
		SCR_AIMessage_Move hold = SCR_AIMessage_Move.Create(null, pos, EMovementType.WALK, false, null);
		if (hold)
		{
			hold.SetReceiver(a);
			m_Mailbox.RequestBroadcast(hold, a);
		}

		DCO_StanceUtil.TrySetStance(ent, ECharacterStance.PRONE, DCO_MoraleSettings.Get().m_fStanceCooldownSec * 1000.0);
	}

	// Ring-sample for the nearest walkable spot with LOS to the enemy, preferring higher
	// ground. Returns false if none found within the reposition radius.
	protected bool DCO_MGFindFiringPos(AIPathfindingComponent pf, vector pos, vector enemy, float eye, DCO_MoraleSettings cfg, out vector best)
	{
		float maxClimb = DCO_TacticalMoveSettings.Get().m_fCoverMaxClimb;
		float bestScore = -99999.0;
		bool found = false;

		for (int i = 0; i < 8; i++)
		{
			float ang = (i / 8.0) * 6.2831853;
			vector cand = pos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fMGRepositionRadius;
			DCO_MGSnapNav(pf, cand, cand);
			if (cand[1] > pos[1] + maxClimb)
				continue;	// don't climb to an attic
			if (DCO_MGLosBlocked(pf, cand, enemy, eye))
				continue;	// must actually see the enemy from here

			// Prefer higher ground (sight lines), nearer is a mild tiebreak.
			float score = (cand[1] - pos[1]) * 5.0 - vector.Distance(cand, pos) * 0.1;
			if (score > bestScore)
			{
				bestScore = score;
				best = cand;
				found = true;
			}
		}
		return found;
	}

	// True if world geometry blocks the line from a firing position (at MG sight height) to the enemy.
	protected bool DCO_MGLosBlocked(AIPathfindingComponent pf, vector fromPos, vector enemyPos, float eye)
	{
		if (!pf)
			return false;
		vector hit;
		return pf.RayTrace(fromPos + Vector(0, eye, 0), enemyPos + Vector(0, eye, 0), hit);
	}

	// Snap a candidate onto the navmesh; unchanged if no pathfinding/no hit.
	protected void DCO_MGSnapNav(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;
		vector corrected;
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(8, 2, 8), corrected))
			outPos = corrected;
	}
}
