// Close-quarters melee. When an enemy gets point-blank (inside m_fMeleeRange) a member
// commits to meleeing that one enemy and follows through, instead of the old blunt
// behaviour where every member in range swung every tick and lowered all their weapons
// into melee mode at once (the "everyone stops shooting and shuffles at each other"
// showdown).
//
// Per-member state machine:
//   - Not committed: only start a melee if an enemy is genuinely point-blank and it's
//     worth it (with m_bMeleeOnlyWhenDry, only when out of ammo - otherwise shooting is
//     better). Until then the member fights normally.
//   - Committed: keep striking the same target until it dies / is lost / breaks past
//     m_fMeleeRange*2 (hysteresis), or the commit times out (DCO_MELEE_COMMIT_MAX_MS, a
//     melee that won't resolve while the member is being shot). On break, DCO_MeleeBreak
//     drops the commit and raises the weapon so it returns cleanly to shooting rather
//     than lingering in a lowered-weapon melee posture.
//
// Actuation is the engine's own melee path (SCR_MeleeComponent.PerformAttack). The
// commit/break logic above is what was missing; the swing itself is the engine's, so
// it's worth confirming in-engine that a server-triggered swing lands like player input.
//
// modded SCR_AIGroupUtilityComponent fragment in Melee/ (sorts after Ammo/ to reuse
// DCO_RearmIsEmpty, before QRF/). On-foot, server-only, default OFF.
modded class SCR_AIGroupUtilityComponent
{
	protected float							m_fDCO_LastMeleeTime	= -1;
	protected ref map<IEntity, IEntity>		m_mDCO_MeleeCommit;			// member -> the enemy it has committed to melee
	protected ref map<IEntity, float>		m_mDCO_MeleeCommitStart;	// member -> world time (ms) it committed (commit timeout)
	protected ref map<IEntity, float>		m_mDCO_MeleeBreakTime;		// member -> world time (ms) it last broke off a melee (re-commit cooldown)
	protected static const float			DCO_MELEE_COMMIT_MAX_MS	= 3500.0;	// give up a melee that hasn't resolved in this long -> return to shooting
	protected static const float			DCO_MELEE_RECOMMIT_COOLDOWN_MS = 4000.0;	// after giving up a melee, stay shooting at least this long before meleeing again

	// Shared-tick entry (called from DCO_GroupQRF.EvaluateActivity).
	void DCO_UpdateMelee()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableMelee || !m_Owner || !m_Perception)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastMeleeTime >= 0 && (now - m_fDCO_LastMeleeTime) < cfg.m_fMeleeCheckSec * 1000.0)
			return;
		m_fDCO_LastMeleeTime = now;

		if (!m_mDCO_MeleeCommit)
			m_mDCO_MeleeCommit = new map<IEntity, IEntity>();

		array<IEntity> targets = m_Perception.m_aTargetEntities;

		float engageSq = cfg.m_fMeleeRange * cfg.m_fMeleeRange;
		float breakRange = cfg.m_fMeleeRange * 2.0;	// hysteresis: commit at range, only break past 2x range
		float breakSq = breakRange * breakRange;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;
			if (DCO_PlayerUtil.IsPlayer(ent))
				continue;	// never drive a player into/out of melee
			vector pos = ent.GetOrigin();

			// Already committed? Follow through until the target dies / is lost / breaks
			// range, or the commit times out (a melee that won't resolve while being shot).
			IEntity committed;
			if (m_mDCO_MeleeCommit.Find(ent, committed))
			{
				bool stillValid = committed && targets && targets.Find(committed) != -1
					&& vector.DistanceSq(committed.GetOrigin(), pos) <= breakSq;
				if (!stillValid || DCO_MeleeCommitTimedOut(ent, now))
				{
					DCO_MeleeBreak(ent, now);		// target gone/dead/escaped/stalemate: back to shooting
				}
				else
				{
					DCO_MeleeStrike(ent);			// keep meleeing the committed target
					continue;
				}
			}

			// Not committed: only start a melee if an enemy is point-blank and it's worth it.
			if (!targets || targets.IsEmpty())
				continue;

			// Recently broke off a melee (e.g. it timed out): keep shooting for a cooldown
			// before re-committing, so it doesn't instantly re-commit to a melee it just abandoned.
			float lastBreak;
			if (m_mDCO_MeleeBreakTime && m_mDCO_MeleeBreakTime.Find(ent, lastBreak) && (now - lastBreak) < DCO_MELEE_RECOMMIT_COOLDOWN_MS)
				continue;

			if (cfg.m_bMeleeOnlyWhenDry && !DCO_RearmIsEmpty(ent))
				continue;	// still has ammo: shooting beats meleeing, don't commit

			IEntity nearest;
			float bestSq = engageSq;
			foreach (IEntity t : targets)
			{
				if (!t)
					continue;
				float d = vector.DistanceSq(t.GetOrigin(), pos);
				if (d <= bestSq)
				{
					bestSq = d;
					nearest = t;
				}
			}
			if (!nearest)
				continue;	// nobody in grappling range: keep shooting

			m_mDCO_MeleeCommit.Set(ent, nearest);
			if (!m_mDCO_MeleeCommitStart)
				m_mDCO_MeleeCommitStart = new map<IEntity, float>();
			m_mDCO_MeleeCommitStart.Set(ent, now);
			DCO_MeleeStrike(ent);

			if (cfg.m_bDebug)
				DCO_Debug.LogGroup("MELEE", ent, "committing to melee a point-blank enemy");
		}
	}

	// One melee swing via the engine's own melee path.
	protected void DCO_MeleeStrike(IEntity ent)
	{
		SCR_MeleeComponent melee = SCR_MeleeComponent.Cast(ent.FindComponent(SCR_MeleeComponent));
		if (melee)
			melee.PerformAttack();
	}

	// True once a member has been committed to a melee longer than the timeout without
	// resolving it. Stops it swinging indefinitely while being shot. Never times out if
	// the commit start time is unknown.
	protected bool DCO_MeleeCommitTimedOut(IEntity ent, float now)
	{
		if (!m_mDCO_MeleeCommitStart)
			return false;
		float start;
		if (!m_mDCO_MeleeCommitStart.Find(ent, start))
			return false;
		return (now - start) >= DCO_MELEE_COMMIT_MAX_MS;
	}

	// Drop a member's melee commitment and transition it cleanly back to shooting: clear
	// the commit state and raise the weapon so it resumes the firefight rather than
	// lingering in a lowered-weapon melee posture.
	protected void DCO_MeleeBreak(IEntity ent, float now)
	{
		m_mDCO_MeleeCommit.Remove(ent);
		if (m_mDCO_MeleeCommitStart)
			m_mDCO_MeleeCommitStart.Remove(ent);

		// Record the break so the member stays shooting for a cooldown before it is allowed to re-commit.
		if (!m_mDCO_MeleeBreakTime)
			m_mDCO_MeleeBreakTime = new map<IEntity, float>();
		m_mDCO_MeleeBreakTime.Set(ent, now);

		SCR_CharacterControllerComponent cc = SCR_CharacterControllerComponent.Cast(ent.FindComponent(SCR_CharacterControllerComponent));
		if (cc)
			cc.SetWeaponRaised(true);	// return to a firing posture (clean transition back to shooting)
	}
}
