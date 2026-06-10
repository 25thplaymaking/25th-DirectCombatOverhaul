// Vehicle-crew detection helper.
// Several DCO group systems (tactical movement, straggler merge, adaptive formation) are meant for
// on-foot infantry and misbehave when applied to a group mounted in a vehicle/aircraft: tactical move
// downgraded a driving crew to walk and side-stepped its destination; tactical move reshaped a
// helicopter crew's/passengers' move order, making them disembark mid-flight to walk to the position;
// straggler-merge folded a depleted vehicle crew into a foot element; adaptive formation poked a crew
// (commander dismounts to reposition).
//
// Plain (non-modded) static helper, so calls to it are order-independent across files, unlike methods
// added to a modded class, which Enforce resolves only if the defining file is processed first. That lets
// every DCO system gate on it regardless of folder order.
//
// Detection: a group counts as "in a vehicle" if its leader or any member is currently mounted. The
// earlier leader-only check missed helicopters/transports where the mounted member being given the move
// order is not the leader, or the leader is briefly unmounted in a transition, so crews/passengers slipped
// through and tactical move made them disembark. Checking any member closes that. Leader is checked first
// (cheap fast path); the member scan runs only if the leader isn't mounted.
class DCO_VehicleUtil
{
	// True if this group is mounted in a vehicle/aircraft (leader or any member inside a vehicle).
	static bool IsGroupInVehicle(SCR_AIGroup group)
	{
		if (!group)
			return false;

		// Fast path: leader mounted (the common crewed-vehicle case).
		IEntity leader = group.GetLeaderEntity();
		if (leader && CompartmentAccessComponent.GetVehicleIn(leader) != null)
			return true;

		// Robust path: any member mounted. Catches helicopter/transport crews + passengers whose mounted
		// member isn't the leader, and transitional states (leader momentarily unmounted). This is what
		// stops heli crews disembarking mid-flight when a tactical move order would otherwise reshape them.
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity member = agent.GetControlledEntity();
			if (member && CompartmentAccessComponent.GetVehicleIn(member) != null)
				return true;
		}

		return false;
	}

	// Order a group's leader to move toward a target entity, choosing a vehicle-aware move when the group
	// is mounted so crews/passengers drive there instead of dismounting to walk (the random mid-mission
	// disembark bug). Falls back to the plain on-foot move when the group is on foot (or if the vehicle
	// message can't be built). Plain static, so order-independent; callable from any DCO fragment.
	// Mirrors the two order primitives already used in this mod: the vehicle-borne move
	// SCR_AIMessage_Move.Create(target, pos, moveType, useVehicles=true, ...) broadcast via the group's
	// mailbox (see DCO_GroupReinforcement), and the on-foot SCR_AIMessageHandling.SendMoveMessage.
	static void OrderGroupMoveToEntity(SCR_AIGroup group, IEntity target, AICommunicationComponent comms, EMovementType moveType = EMovementType.RUN)
	{
		if (!group || !target || !comms)
			return;

		AIAgent leaderAgent = group.GetLeaderAgent();
		if (!leaderAgent)
			return;

		// Mounted: issue a move the AI is allowed to drive (useVehicles = true) so it keeps the vehicle
		// instead of disembarking to walk to the position.
		if (IsGroupInVehicle(group))
		{
			SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(target, target.GetOrigin(), moveType, true, null);
			if (msg)
			{
				comms.RequestBroadcast(msg);
				return;
			}
		}

		// On foot (or vehicle message construction failed): original on-foot move order.
		SCR_AIMessageHandling.SendMoveMessage(leaderAgent, target, null, comms);
	}

	// Order a group to move/withdraw to a world position with a vehicle-aware move (useVehicles = true), so
	// a mounted crew drives/flies there instead of disembarking to walk. Intended for the mounted branch of
	// systems that would otherwise issue an on-foot order to a position (e.g. a morale-break withdrawal of a
	// helicopter/vehicle crew; on foot they would bail out and, for aircraft, fall to their death). Callers
	// gate on IsGroupInVehicle first. Plain static, so order-independent.
	static void OrderGroupMoveToPosition(SCR_AIGroup group, vector pos, AICommunicationComponent comms, EMovementType moveType = EMovementType.RUN)
	{
		if (!group || !comms)
			return;

		AIAgent leaderAgent = group.GetLeaderAgent();
		if (!leaderAgent)
			return;

		SCR_AIMessage_Move msg = SCR_AIMessage_Move.Create(null, pos, moveType, true, null);
		if (msg)
			comms.RequestBroadcast(msg);
	}
}
