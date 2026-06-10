// Safe-eject gate: don't let AI bail from a fast / high vehicle. Defensive net so an AI passenger
// won't voluntarily dismount a vehicle that is moving fast or is well off the ground (e.g. a
// helicopter in flight, where bailing means falling to death). Hooks the scripted request seam
// SCR_CompartmentAccessComponent.AskOwnerToGetOutFromVehicle and blocks it when it is not a forced
// eject (ejectOnTheSpot, i.e. death/destruction still ejects), the occupant is AI (players are
// never blocked), and the vehicle's speed > m_fSafeEjectMaxSpeedKmh or its height-above-ground >
// m_fSafeEjectMaxHeightM. A slow/low/stationary/landed vehicle still lets AI out normally so legit
// LZ dismounts work.
//
// The engine-native CompartmentAccessComponent.GetOutVehicle(...) is `proto external` and cannot be
// overridden, so this gates the scripted "ask the owner to leave" path AI normally uses.
// Server-side; default OFF.
modded class SCR_CompartmentAccessComponent
{
	override void AskOwnerToGetOutFromVehicle(EGetOutType type, int doorInfoIndex, ECloseDoorAfterActions closeDoor, bool performWhenPaused, bool ejectOnTheSpot = false)
	{
		// Forced ejects (death/destruction) and any non-server path are never blocked.
		if (Replication.IsServer() && !ejectOnTheSpot && DCO_SafeEjectShouldBlock())
			return;

		super.AskOwnerToGetOutFromVehicle(type, doorInfoIndex, closeDoor, performWhenPaused, ejectOnTheSpot);
	}

	// True if this occupant is AI and the vehicle is currently unsafe to dismount (too fast or too high).
	protected bool DCO_SafeEjectShouldBlock()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableSafeEjectGate)
			return false;

		IEntity occupant = GetOwner();
		if (!occupant)
			return false;

		// Never block a player.
		PlayerManager pm = GetGame().GetPlayerManager();
		if (pm && pm.GetPlayerIdFromControlledEntity(occupant) != 0)
			return false;

		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(occupant);
		if (!vehicle)
			return false;

		// Speed (km/h) from the vehicle's physics body.
		float speedKmh = 0;
		Physics ph = vehicle.GetPhysics();
		if (ph)
			speedKmh = ph.GetVelocity().Length() * Physics.MS2KMH;

		// Height above ground (m) = vehicle Y minus terrain surface Y beneath it.
		float agl = 0;
		BaseWorld world = GetGame().GetWorld();
		if (world)
		{
			vector vp = vehicle.GetOrigin();
			agl = vp[1] - world.GetSurfaceY(vp[0], vp[2]);
		}

		bool unsafe = (speedKmh > cfg.m_fSafeEjectMaxSpeedKmh) || (agl > cfg.m_fSafeEjectMaxHeightM);
		if (unsafe)
			DCO_Debug.Log("SAFEEJECT", string.Format("blocked AI dismount: speed=%1 km/h agl=%2 m", speedKmh, agl));
		return unsafe;
	}
}
