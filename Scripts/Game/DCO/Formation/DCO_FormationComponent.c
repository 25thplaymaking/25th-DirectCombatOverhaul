// Adaptive formation. Switches a group's formation by situation - a tighter column
// when travelling, a spread line when in contact - so its posture matches the moment.
//
// Applied via AIFormationComponent.SetFormation(string); the component lives on the
// group entity. SCR_EAIGroupFormation members {Wedge, Line, Column, StaggeredColumn}
// double as the SetFormation strings. Only re-applied when the desired formation changes.
//
// modded SCR_AIGroupUtilityComponent fragment. "Formation" sorts before "QRF" so
// DCO_UpdateFormation is defined before the tick that calls it. Server-only, default OFF.
// Names are tunable via m_sFormationTravel / m_sFormationContact.

// DCO-computed formation shapes (per-member offsets), used by DCO_UpdateFormationShape
// to impose echelon/vee/dispersion the native SetFormation set can't.
enum EDCO_FormShape
{
	COLUMN,
	WEDGE,
	LINE,
	ECHELON_LEFT,
	ECHELON_RIGHT,
	VEE
}

modded class SCR_AIGroupUtilityComponent
{
	protected float		m_fDCO_LastFormationTime	= -1;
	protected string	m_sDCO_LastFormation;
	protected float		m_fDCO_LastCoverSeek		= -1;
	protected float		m_fDCO_LastShapeTime		= -1;

	void DCO_UpdateFormation()
	{
		if (!Replication.IsServer())
			return;

		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableAdaptiveFormation || !m_Owner)
			return;

		// Formations are an infantry concept - never poke a crewed vehicle (it would make
		// the commander dismount to "reposition").
		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastFormationTime >= 0 && (now - m_fDCO_LastFormationTime) < cfg.m_fFormationCheckSec * 1000.0)
			return;
		m_fDCO_LastFormationTime = now;

		IEntity groupEnt = GetOwner();
		if (!groupEnt)
			return;
		AIFormationComponent formation = AIFormationComponent.Cast(groupEnt.FindComponent(AIFormationComponent));
		if (!formation)
			return;

		// In contact = has a perceived enemy; otherwise travelling.
		bool inContact = false;
		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			inContact = targets && !targets.IsEmpty();
		}

		string desired;
		if (inContact)
			desired = cfg.m_sFormationContact;
		else
			desired = cfg.m_sFormationTravel;

		if (desired == string.Empty || desired == m_sDCO_LastFormation)
			return;	// nothing to change

		m_sDCO_LastFormation = desired;
		formation.SetFormation(desired);
	}

	// Cover-aware placement: while in contact, any member exposed to the threat is nudged
	// to the nearest spot concealed from it (ring sample + LOS raycast + navmesh snap).
	// Moves individuals to local cover instead of leaving them in the open, layered over
	// CRX's formation slots. On-foot, server-side, default OFF.
	void DCO_UpdateCoverPlacement()
	{
		if (!Replication.IsServer())
			return;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (!cfg || !cfg.m_bEnableCoverSeek || !m_Owner || !m_Mailbox || !m_Perception)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		array<IEntity> targets = m_Perception.m_aTargetEntities;
		if (!targets || targets.IsEmpty())
			return;	// only seek cover when there is something to hide from

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastCoverSeek >= 0 && (now - m_fDCO_LastCoverSeek) < cfg.m_fCoverSeekCheckSec * 1000.0)
			return;
		m_fDCO_LastCoverSeek = now;

		// Threat centroid.
		vector sum = vector.Zero;
		int n = 0;
		foreach (IEntity t : targets)
		{
			if (t)
			{
				sum += t.GetOrigin();
				n++;
			}
		}
		if (n == 0)
			return;
		vector threatPos = sum / n;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		AICommunicationComponent comms = m_Mailbox;

		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent)
				continue;

			AIPathfindingComponent pf = AIPathfindingComponent.Cast(ent.FindComponent(AIPathfindingComponent));
			vector pos = ent.GetOrigin();

			// Already concealed from the threat: leave it be.
			if (DCO_FormRayBlocked(pf, threatPos, pos))
				continue;

			// Sample a ring around the member for the nearest concealed, navmesh-valid spot.
			vector best;
			bool found = false;
			float bestDistSq = 0;
			int samples = cfg.m_iCoverSeekSamples;
			if (samples < 1)
				samples = 1;
			for (int i = 0; i < samples; i++)
			{
				float ang = (i / (float)samples) * 6.2831853;
				vector cand = pos + Vector(Math.Cos(ang), 0, Math.Sin(ang)) * cfg.m_fCoverSeekRadius;
				DCO_FormSnapNav(pf, cand, cand);
				if (cand[1] > pos[1] + cfg.m_fCoverMaxClimb)
					continue;	// don't climb to an upper floor/attic - take cover on this floor
				if (!DCO_FormRayBlocked(pf, threatPos, cand))
					continue;	// candidate still exposed
				float d = vector.DistanceSq(cand, pos);
				if (!found || d < bestDistSq)
				{
					bestDistSq = d;
					best = cand;
					found = true;
				}
			}

			if (found)
			{
				SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, best, EMovementType.RUN, false, null);
				if (msg)
				{
					msg.SetReceiver(a);
					comms.RequestBroadcast(msg, a);
				}
			}
		}
	}

	// True if world geometry blocks the line threat->pos (pos is concealed). A local copy,
	// since this fragment can't call the Movement-fragment version (file-order resolution).
	protected bool DCO_FormRayBlocked(AIPathfindingComponent pf, vector threatPos, vector pos)
	{
		if (!pf)
			return false;
		vector eye = Vector(0, 1.5, 0);
		vector hit;
		return pf.RayTrace(threatPos + eye, pos + eye, hit);
	}

	// Snap a candidate onto the navmesh (walkable ground). Unchanged if no pathfinding/no hit.
	protected void DCO_FormSnapNav(AIPathfindingComponent pf, vector inPos, out vector outPos)
	{
		outPos = inPos;
		if (!pf)
			return;
		vector corrected;
		// Tight vertical search (+-2 m) so a ground candidate snaps to this floor, not an upper storey.
		if (pf.GetClosestPositionOnNavmesh(inPos, Vector(6, 2, 6), corrected))
			outPos = corrected;
	}

	// DCO formation shapes: impose echelon / vee / dispersion the native SetFormation set
	// can't, by placing each non-leader member at a computed offset from the leader,
	// oriented along the group's heading (or toward the threat in contact) and scaled by
	// m_fFormationSpacing. Travel uses m_iFormationTravelShape, contact uses
	// m_iFormationContactShape. On-foot, server-side, default OFF. Experimental: it issues
	// per-member moves and will fight CRX's own formation - an alternative to it, not an
	// addition, so don't run it alongside cover-seek on the same group.
	void DCO_UpdateFormationShape()
	{
		if (!Replication.IsServer())
			return;

		DCO_TacticalMoveSettings cfg = DCO_TacticalMoveSettings.Get();
		if (!cfg || !cfg.m_bEnableDcoFormations || !m_Owner || !m_Mailbox)
			return;

		if (DCO_VehicleUtil.IsGroupInVehicle(m_Owner))
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();
		if (m_fDCO_LastShapeTime >= 0 && (now - m_fDCO_LastShapeTime) < cfg.m_fFormationShapeCheckSec * 1000.0)
			return;
		m_fDCO_LastShapeTime = now;

		IEntity leader = m_Owner.GetLeaderEntity();
		if (!leader)
			return;
		vector leaderPos = leader.GetOrigin();

		// Heading: face the way the leader faces while travelling; face the threat while in contact.
		vector fwd = leader.GetTransformAxis(2);
		fwd[1] = 0;
		int shape = cfg.m_iFormationTravelShape;

		bool inContact = false;
		if (m_Perception)
		{
			array<IEntity> targets = m_Perception.m_aTargetEntities;
			inContact = targets && !targets.IsEmpty();
			if (inContact)
			{
				shape = cfg.m_iFormationContactShape;
				vector sum = vector.Zero;
				int n = 0;
				foreach (IEntity t : targets)
				{
					if (t)
					{
						sum += t.GetOrigin();
						n++;
					}
				}
				if (n > 0)
				{
					vector toThreat = (sum / n) - leaderPos;
					toThreat[1] = 0;
					if (toThreat.LengthSq() > 0.01)
						fwd = toThreat;
				}
			}
		}

		if (fwd.LengthSq() < 0.01)
			fwd = Vector(0, 0, 1);
		fwd.Normalize();
		vector right = Vector(fwd[2], 0, -fwd[0]);	// right-hand perpendicular in the ground plane

		AIPathfindingComponent pf = AIPathfindingComponent.Cast(leader.FindComponent(AIPathfindingComponent));
		AICommunicationComponent comms = m_Mailbox;

		array<AIAgent> agents = {};
		m_Owner.GetAgents(agents);
		int idx = 0;
		foreach (AIAgent a : agents)
		{
			if (!a)
				continue;
			IEntity ent = a.GetControlledEntity();
			if (!ent || ent == leader)
				continue;	// the leader anchors the formation
			idx++;

			vector slot = leaderPos + DCO_FormationOffset(shape, idx, cfg.m_fFormationSpacing, fwd, right);
			DCO_FormSnapNav(pf, slot, slot);

			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, slot, EMovementType.RUN, false, null);
			if (msg)
			{
				msg.SetReceiver(a);
				comms.RequestBroadcast(msg, a);
			}
		}
	}

	// Per-member offset for a formation shape. i is the 1-based member index (leader
	// excluded); odd = right side, even = left; rank groups them behind/around the leader.
	protected vector DCO_FormationOffset(int shape, int i, float s, vector fwd, vector right)
	{
		int rank = (i + 1) / 2;		// 1,1,2,2,3,3...
		float side = 1.0;			// odd -> right
		if (i % 2 == 0)
			side = -1.0;			// even -> left

		switch (shape)
		{
			case EDCO_FormShape.COLUMN:
				return fwd * (-i * s);
			case EDCO_FormShape.LINE:
				return right * (side * rank * s) + fwd * (-s * 0.2);
			case EDCO_FormShape.ECHELON_LEFT:
				return (fwd * -1.0 - right) * (i * s * 0.7);
			case EDCO_FormShape.ECHELON_RIGHT:
				return (fwd * -1.0 + right) * (i * s * 0.7);
			case EDCO_FormShape.VEE:
				return fwd * (rank * s * 0.5) + right * (side * rank * s);
		}
		// WEDGE (default): a V trailing behind the leader.
		return fwd * (-rank * s) + right * (side * rank * s * 0.7);
	}
}
