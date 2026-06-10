// Vehicle section controller (the doctrine "brain"). Server-side singleton (mirrors
// DCO_Commander): lazily started from the group tick, self-paced via a repeating CallLater. Each
// tick it enumerates AI vehicle crews, clusters same-faction crews within the section radius into
// sections (proximity, transitive) honouring an optional DCO_ConvoyPreset convoyId, decides each
// section's combat state with strict N-second handback hysteresis, and writes a doctrinal role +
// reference enemy onto each crew's intent store. Out of combat it writes NONE (full vanilla).
//
// Movement itself is performed by the per-crew executor (DCO_VehicleDoctrine.c) using only
// navmesh-validated move orders. This class decides roles; the executor decides safe positions.
class DCO_VehicleSection
{
	ref array<SCR_AIGroup>	m_aMembers			= {};
	int						m_iId;
	bool					m_bInCombat;
	float					m_fLastContactTime	= -1;	// world time (ms) of the most recent member contact
	vector					m_vEnemyPos;
	bool					m_bHasEnemy;
	bool					m_bRoadBlocked;
}

class DCO_VehicleSectionController
{
	protected static ref DCO_VehicleSectionController	s_Instance;
	protected bool										m_bRunning;
	// Persistent per-section state keyed on the section's first (lead) group so it survives across ticks.
	protected ref map<SCR_AIGroup, float>				m_mLastContact;	// section-lead group to last contact time (ms)
	protected ref map<SCR_AIGroup, bool>				m_mBoundToggle;	// section-lead group to which half bounds this cycle
	protected ref array<ref Shape>						m_aDCO_DebugShapes;

	static DCO_VehicleSectionController Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_VehicleSectionController();
		return s_Instance;
	}

	static void EnsureRunning()
	{
		if (!Replication.IsServer())
			return;
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleDoctrine)
			return;

		DCO_VehicleSectionController c = Get();
		if (c.m_bRunning)
			return;
		c.m_bRunning = true;
		c.m_mLastContact = new map<SCR_AIGroup, float>();
		c.m_mBoundToggle = new map<SCR_AIGroup, bool>();
		c.m_aDCO_DebugShapes = {};
		GetGame().GetCallqueue().CallLater(c.Update, (int)(cfg.m_fVehDoctrineCheckSec * 1000.0), true);
	}

	void Update()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableVehicleDoctrine)
		{
			ClearDebug();
			return;	// toggled off at runtime - harmless no-op
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;

		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);

		// 1) Collect unique vehicle-crew groups.
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		array<SCR_AIGroup> crews = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
			if (!util || !util.DCO_IsVehicleCrew())
				continue;
			crews.Insert(grp);
		}

		// 2) Cluster into sections (transitive proximity, same faction, honour convoyId tag).
		array<ref DCO_VehicleSection> sections = BuildSections(crews, cfg);

		// 3) Per section: combat state + role assignment.
		int nextId = 0;
		foreach (DCO_VehicleSection sec : sections)
		{
			sec.m_iId = nextId++;
			ClassifyCombat(sec, cfg, now);
			AssignRoles(sec, cfg, now);
		}

		if (cfg.m_bDebugVehDoctrine)
			DebugDraw(sections);
		else
			ClearDebug();
	}

	// Transitive proximity clustering. Two crews join the same section if within section radius and same
	// faction; an explicit convoyId tag forces membership (same id together, different ids never merge).
	protected array<ref DCO_VehicleSection> BuildSections(array<SCR_AIGroup> crews, DCO_MoraleSettings cfg)
	{
		array<ref DCO_VehicleSection> sections = {};
		float rSq = cfg.m_fVehDoctrineSectionRadius * cfg.m_fVehDoctrineSectionRadius;
		array<bool> used = {};
		for (int i = 0; i < crews.Count(); i++)
			used.Insert(false);

		for (int i = 0; i < crews.Count(); i++)
		{
			if (used[i])
				continue;
			DCO_VehicleSection sec = new DCO_VehicleSection();
			sec.m_aMembers.Insert(crews[i]);
			used[i] = true;

			// Grow the section by repeatedly absorbing any unused crew near any current member (transitive).
			bool grew = true;
			while (grew)
			{
				grew = false;
				for (int j = 0; j < crews.Count(); j++)
				{
					if (used[j])
						continue;
					if (!SameSectionEligible(crews[i], crews[j]))
						continue;
					foreach (SCR_AIGroup m : sec.m_aMembers)
					{
						if (InSectionRange(m, crews[j], rSq))
						{
							sec.m_aMembers.Insert(crews[j]);
							used[j] = true;
							grew = true;
							break;
						}
					}
				}
			}
			sections.Insert(sec);
		}
		return sections;
	}

	// Faction + convoy-tag eligibility (proximity is checked separately in InSectionRange).
	protected bool SameSectionEligible(SCR_AIGroup a, SCR_AIGroup b)
	{
		if (a.GetFaction() != b.GetFaction())
			return false;
		int ca = DCO_ConvoyTag(a);
		int cb = DCO_ConvoyTag(b);
		if (ca >= 0 && cb >= 0 && ca != cb)
			return false;	// explicitly different convoys never merge
		return true;
	}

	// Convoy id baked by DCO_ConvoyPreset. -1 = untagged. Stored on the utility component.
	protected int DCO_ConvoyTag(SCR_AIGroup grp)
	{
		SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
		if (!util)
			return -1;
		return util.DCO_GetConvoyId();
	}

	protected bool InSectionRange(SCR_AIGroup a, SCR_AIGroup b, float rSq)
	{
		IEntity la = a.GetLeaderEntity();
		IEntity lb = b.GetLeaderEntity();
		if (!la || !lb)
			return false;
		// A tagged convoy is never split by distance.
		int ta = DCO_ConvoyTag(a);
		if (ta >= 0 && ta == DCO_ConvoyTag(b))
			return true;
		return vector.DistanceSq(la.GetOrigin(), lb.GetOrigin()) <= rSq;
	}

	// Combat state with strict handback: in combat if any member perceives an enemy with threat >= enter
	// threshold; stays in combat until no member has had contact for handback seconds. Records the section's
	// reference enemy (nearest to any member) for orientation.
	protected void ClassifyCombat(DCO_VehicleSection sec, DCO_MoraleSettings cfg, float now)
	{
		SCR_AIGroup key = sec.m_aMembers[0];
		bool contactNow = false;
		sec.m_bHasEnemy = false;
		float bestSq = 1000000000.0;

		foreach (SCR_AIGroup g : sec.m_aMembers)
		{
			SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
			if (!util || !util.m_Perception)
				continue;
			array<IEntity> tgts = util.m_Perception.m_aTargetEntities;
			if (!tgts || tgts.IsEmpty())
				continue;

			if (util.GetThreatMeasure() >= cfg.m_fVehDoctrineEnterThreat)
				contactNow = true;

			IEntity gl = g.GetLeaderEntity();
			vector from = vector.Zero;
			if (gl)
				from = gl.GetOrigin();
			foreach (IEntity t : tgts)
			{
				if (!t)
					continue;
				float dSq = vector.DistanceSq(t.GetOrigin(), from);
				if (dSq < bestSq)
				{
					bestSq = dSq;
					sec.m_vEnemyPos = t.GetOrigin();
					sec.m_bHasEnemy = true;
				}
			}
		}

		float last = -1;
		m_mLastContact.Find(key, last);
		if (contactNow)
		{
			last = now;
			m_mLastContact.Set(key, now);
		}
		sec.m_fLastContactTime = last;
		sec.m_bInCombat = (last >= 0) && ((now - last) < cfg.m_fVehDoctrineHandbackSec * 1000.0) && sec.m_bHasEnemy;
	}

	// Assign a doctrinal role to each member. Out of combat: NONE (vanilla). In combat: detect a blocked
	// route (any disabled/destroyed member); if advancing (road open, bounding enabled, >=2) run bounding
	// overwatch; otherwise nearest-to-enemy = ESCORT base of fire, farthest = LEAD, rest TRAIL, with stuck
	// transports dismounting and destroyed crews abandoning.
	protected void AssignRoles(DCO_VehicleSection sec, DCO_MoraleSettings cfg, float now)
	{
		if (!sec.m_bInCombat || !sec.m_bHasEnemy)
		{
			foreach (SCR_AIGroup g0 : sec.m_aMembers)
			{
				SCR_AIGroupUtilityComponent u0 = g0.GetGroupUtilityComponent();
				if (u0)
					u0.DCO_SetVehicleIntent(EDCO_VehicleRole.NONE, vector.Zero, false, vector.Zero, false, sec.m_iId, false);
			}
			return;
		}

		// Road blocked if any member is disabled (watchdog) or destroyed.
		sec.m_bRoadBlocked = false;
		foreach (SCR_AIGroup gd : sec.m_aMembers)
		{
			if (DCO_VehDisposition(gd) != 0)
			{
				sec.m_bRoadBlocked = true;
				break;
			}
		}

		// Advancing under contact, road open: bounding overwatch (one half moves while the other covers).
		if (cfg.m_bVehDoctrineBounding && !sec.m_bRoadBlocked && sec.m_aMembers.Count() >= 2)
		{
			AssignBounding(sec, cfg);
			return;
		}

		// Find the member nearest the enemy (the ESCORT) and farthest (the LEAD).
		SCR_AIGroup nearest;
		SCR_AIGroup farthest;
		float nearSq = 1000000000.0;
		float farSq = -1;
		foreach (SCR_AIGroup g : sec.m_aMembers)
		{
			IEntity gl = g.GetLeaderEntity();
			if (!gl)
				continue;
			float dSq = vector.DistanceSq(gl.GetOrigin(), sec.m_vEnemyPos);
			if (dSq < nearSq) { nearSq = dSq; nearest = g; }
			if (dSq > farSq)  { farSq = dSq;  farthest = g; }
		}

		foreach (SCR_AIGroup g : sec.m_aMembers)
		{
			SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
			if (!util)
				continue;

			// Destroyed own vehicle: crew abandons and joins the base of fire.
			if (cfg.m_bVehDoctrineRecovery && DCO_VehDisposition(g) == 2)
			{
				util.DCO_SetVehicleIntent(EDCO_VehicleRole.DESTROY, vector.Zero, false, sec.m_vEnemyPos, true, sec.m_iId, true);
				continue;
			}

			// Stuck transport: debus passengers to a base of fire.
			if (sec.m_bRoadBlocked && ShouldDismount(g, cfg))
			{
				util.DCO_SetVehicleIntent(EDCO_VehicleRole.DISMOUNT, vector.Zero, false, sec.m_vEnemyPos, true, sec.m_iId, true);
				continue;
			}

			EDCO_VehicleRole role;
			if (g == nearest)
				role = EDCO_VehicleRole.ESCORT;
			else if (g == farthest && sec.m_aMembers.Count() > 1)
				role = EDCO_VehicleRole.LEAD;
			else
				role = EDCO_VehicleRole.TRAIL;

			util.DCO_SetVehicleIntent(role, vector.Zero, false, sec.m_vEnemyPos, true, sec.m_iId, sec.m_bRoadBlocked);
		}
	}

	// Bounding overwatch: split the section in two; one half leads (bounds) while the other overwatches.
	// The bounding half alternates each tick so they leapfrog. Doctrine: only one element moves at a time.
	protected void AssignBounding(DCO_VehicleSection sec, DCO_MoraleSettings cfg)
	{
		SCR_AIGroup key = sec.m_aMembers[0];
		bool toggle = false;
		m_mBoundToggle.Find(key, toggle);
		toggle = !toggle;
		m_mBoundToggle.Set(key, toggle);

		int half = sec.m_aMembers.Count() / 2;
		for (int i = 0; i < sec.m_aMembers.Count(); i++)
		{
			SCR_AIGroup g = sec.m_aMembers[i];
			SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
			if (!util)
				continue;
			bool bounds = (i < half) == toggle;	// first half bounds when toggle, else overwatches
			EDCO_VehicleRole role = EDCO_VehicleRole.OVERWATCH;
			if (bounds)
				role = EDCO_VehicleRole.LEAD;
			util.DCO_SetVehicleIntent(role, vector.Zero, false, sec.m_vEnemyPos, true, sec.m_iId, false);
		}
	}

	// A crew should dismount when the section is stuck and its vehicle carries passengers (a transport whose
	// occupants are combat-useless while mounted). Crews of pure fighting vehicles (turret only) stay mounted.
	protected bool ShouldDismount(SCR_AIGroup g, DCO_MoraleSettings cfg)
	{
		if (!cfg.m_bVehDoctrineDismountStuck)
			return false;
		IEntity leader = g.GetLeaderEntity();
		if (!leader)
			return false;
		IEntity veh = CompartmentAccessComponent.GetVehicleIn(leader);
		if (!veh)
			return false;
		SCR_BaseCompartmentManagerComponent mgr = SCR_BaseCompartmentManagerComponent.Cast(veh.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!mgr)
			return false;
		// A vehicle with passenger (cargo) compartments is a transport whose occupants are combat-useless
		// while mounted, so dismount it when stuck. A pure fighting vehicle (tank/APC turret, no cargo seats) has none,
		// so it stays mounted as a base of fire. Uses the verified PASSENGER_COMPARTMENT_TYPES static (no bare
		// ECompartmentType literal).
		array<BaseCompartmentSlot> seats = {};
		mgr.GetCompartmentsOfTypes(seats, SCR_BaseCompartmentManagerComponent.PASSENGER_COMPARTMENT_TYPES);
		return seats.Count() > 0;
	}

	// 0 = operational, 1 = disabled (alive but watchdog says not moving when ordered), 2 = destroyed.
	protected int DCO_VehDisposition(SCR_AIGroup g)
	{
		SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
		if (!util)
			return 0;
		IEntity leader = g.GetLeaderEntity();
		if (!leader)
			return 0;
		IEntity veh = CompartmentAccessComponent.GetVehicleIn(leader);
		if (!veh)
			return 0;
		// Destroyed = default hit zone health at zero. GetHealthScaled() is inherited from DamageManagerComponent
		// (SCR_DamageManagerComponent derives from it) - safer than the unconfirmed GetState()/EDamageState.
		SCR_DamageManagerComponent dmg = SCR_DamageManagerComponent.GetDamageManager(veh);
		if (dmg && dmg.GetHealthScaled() <= 0.0)
			return 2;
		if (util.DCO_VehIsDisabled())
			return 1;
		return 0;
	}

	protected void ClearDebug()
	{
		if (m_aDCO_DebugShapes)
			m_aDCO_DebugShapes.Clear();	// releasing the held Shape refs frees the visualizers
	}

	protected void DebugDraw(array<ref DCO_VehicleSection> sections)
	{
		ClearDebug();
		if (!m_aDCO_DebugShapes)
			m_aDCO_DebugShapes = {};
		ShapeFlags flags = ShapeFlags.NOZBUFFER;

		foreach (DCO_VehicleSection sec : sections)
		{
			IEntity lead = sec.m_aMembers[0].GetLeaderEntity();
			if (!lead)
				continue;
			vector lp = lead.GetOrigin() + Vector(0, 2, 0);

			foreach (SCR_AIGroup g : sec.m_aMembers)
			{
				SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
				IEntity gl = g.GetLeaderEntity();
				if (!util || !gl)
					continue;
				DCO_VehicleIntent it = util.DCO_GetVehicleIntent();
				int col = RoleColour(it.m_eRole);
				vector gp = gl.GetOrigin() + Vector(0, 2, 0);

				Shape sph = Shape.CreateSphere(col, flags, gp, 1.5);
				if (sph)
					m_aDCO_DebugShapes.Insert(sph);
				Shape link = Shape.CreateArrow(lp, gp, 0.3, 0xff888888, flags);
				if (link)
					m_aDCO_DebugShapes.Insert(link);
				if (it.m_bHasTarget)
				{
					Shape tgt = Shape.CreateArrow(gp, it.m_vTargetPos + Vector(0, 1, 0), 0.4, col, flags);
					if (tgt)
						m_aDCO_DebugShapes.Insert(tgt);
				}
			}
		}
	}

	protected int RoleColour(EDCO_VehicleRole role)
	{
		switch (role)
		{
			case EDCO_VehicleRole.LEAD:			return 0xff00ff00;	// green
			case EDCO_VehicleRole.OVERWATCH:	return 0xff00ffff;	// cyan
			case EDCO_VehicleRole.ESCORT:		return 0xffff8800;	// orange
			case EDCO_VehicleRole.TRAIL:		return 0xffffff00;	// yellow
			case EDCO_VehicleRole.DISMOUNT:		return 0xffff00ff;	// magenta
			case EDCO_VehicleRole.DESTROY:		return 0xffff0000;	// red
			case EDCO_VehicleRole.RECOVER:		return 0xff8800ff;	// purple
		}
		return 0xff808080;	// NONE = grey
	}
}
