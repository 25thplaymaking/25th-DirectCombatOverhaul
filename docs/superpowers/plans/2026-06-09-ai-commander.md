# AI Commander — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A battlefield director that maintains objectives (auto-derived + GM-placed draggable entities), ranks them by threat, and issues orders to groups (defend undermanned bases, assist fights under attack, hold/patrol, commit reserves to the main effort), with an always-on GM visualization.

**Architecture:** Reuse-heavy. A `DCO_Objective` record + `DCO_ObjectiveRegistry` singleton (modelled on `DCO_CompositionRegistry`) hold auto objectives (lightweight records) and GM-placed objectives (real `DCO_ObjectiveZone` editable placeables, modelled on `DCO_TaskZone`). The extended `DCO_Commander` singleton scores/ranks objectives (METT-TC-lite), assigns groups, and issues orders via the existing reinforcement/defend/move primitives. A Shape draw shows the plan in GM.

**Tech Stack:** Enfusion Script (Arma Reforger 1.7). Reforger MCP for all symbol verification. Verification = MCP-confirm + **Workbench reload** (no test harness; no git). Placeable GUIDs are **Workbench-assigned** (never hand-authored).

---

## Project execution rules (READ FIRST)
- **No unit tests / no git.** Each task ends with a **Reload Checkpoint** (user reloads Workbench; expect 0 DCO errors). MCP-verify every engine symbol *before* writing it (AGENTS.md).
- **Placeable GUIDs are Workbench-only.** The `DCO_ObjectiveZone` prefab + placeable-registry entry + editor-attribute category are created by duplicating the existing `DCO_TaskZone` placeable in Workbench (assigns GUIDs); scripts/.conf bodies are authored here.
- **Compile-gate:** any DCO compile error drops the whole "25th DCO" tab — keep every task clean. Plain static/singleton classes are order-independent; modded-class methods are file-order resolved.
- **Default OFF / additive:** gated on `DCO_MoraleSettings.m_bEnableCommander`; never alters behaviour when off.

**Verified API to rely on (MCP-confirmed earlier this session + this plan's Task 0):**
- `SCR_AIWorld.GetAIAgents`; `AIAgent.GetParentGroup/GetControlledEntity`; `SCR_AIGroup.GetFaction/GetLeaderEntity/GetGroupUtilityComponent/GetAgents`.
- `SCR_AIGroupUtilityComponent.GetThreatMeasure/m_Perception/m_Mailbox/DCO_SetReinforcementEligible`; `SCR_AIGroupPerception.m_aTargetEntities`.
- `DCO_VehicleUtil.OrderGroupMoveToPosition/OrderGroupMoveToEntity`; `DCO_CompositionRegistry.Get().CountNear/GetNearest`.
- `SCR_EditableEntityCore.GetAllEntities`; `SCR_EditableEntityComponent.GetEntityType/GetOwner`; `FactionAffiliationComponent.GetAffiliatedFaction`.
- `Shape.CreateSphere/CreateArrow`, `ShapeFlags`; `GetGame().GetCallqueue().CallLater`.

---

## File Structure

| File | Responsibility | New/Modify |
|---|---|---|
| `Scripts/Game/DCO/Commander/DCO_ObjectiveTypes.c` | `DCO_EObjectiveType` enum + `DCO_Objective` record | Create |
| `Scripts/Game/DCO/Commander/DCO_ObjectiveRegistry.c` | Singleton: GM + auto objectives, auto-derivation, dedupe | Create |
| `Scripts/Game/DCO/Commander/DCO_ObjectiveZone.c` | GM-placed objective: editable placeable component (registers into registry) | Create |
| `Scripts/Game/DCO/Commander/DCO_ObjectiveEditorAttributes.c` | Entity attributes (type/priority/radius) for the placeable | Create |
| `Scripts/Game/DCO/Commander/DCO_Commander.c` | Brain rework: score → rank → assign → order; GM viz | Modify |
| `Scripts/Game/DCO/Morale/DCO_MoraleSettings.c` | Commander objective settings (intervals, weights, radii, viz toggle) | Modify |
| `Scripts/Game/DCO/Morale/DCO_MoraleEditorAttributes.c` | GM attribute for the viz toggle | Modify |
| `Configs/Editor/AttributeLists/DCO_Attributes.conf` | Register the viz-toggle attribute | Modify |
| `Prefabs/.../DCO_ObjectiveZone.et` + placeable `.conf` | The draggable placeable (Workbench-created GUIDs) | Create (Workbench) |
| `<auto-memory>/ai-commander-system.md` | Memory | Create |

---

## Task 0: MCP verification of new engine touchpoints

- [ ] **Step 1:** `api_search` confirm each, record signatures in a scratch note:
  - `Shape.CreateSphere` / `Shape.CreateArrow` signatures + `ShapeFlags` members (WIREFRAME/NOZBUFFER).
  - `SCR_AIGroup` has a method to read group strength / member count (e.g. `GetAgentsCount`/`GetPlayerAndAgentCount`) — pick the verified one for "combat power".
  - `SCR_AIGroupUtilityComponent.GetThreatMeasure` return scale (used as enemy-threat proxy).
  - Confirm `DCO_DefendComponent` public entry to put a group into a defend posture at a position (read `Scripts/Game/DCO/Defend/DCO_DefendComponent.c`); if none is callable externally, orders use the move+reinforcement-eligible primitive only and DEFEND = hold-at-position.
- [ ] **Step 2: Reload Checkpoint** — none (no code yet). Just record findings to drive the code below.

---

## Task 1: Objective types + record

**Files:** Create `Scripts/Game/DCO/Commander/DCO_ObjectiveTypes.c`

- [ ] **Step 1: Write the file**
```cpp
/*
 * 25thCRX - DCO Expansion : AI Commander objective types + record
 * A DCO_Objective is one thing the commander wants serviced. GM-placed objectives are backed by a real
 * DCO_ObjectiveZone editable entity (draggable); auto objectives are lightweight records the registry derives
 * each tick (undermanned bases, hot fights). Both feed the same scoring + order pipeline.
 */
enum DCO_EObjectiveType
{
	DEFEND,		// hold a friendly area/base
	HOLD,		// hold current ground
	ATTACK,		// take an enemy position
	PATROL,		// roam within radius
	SUPPORT,	// assist a friendly group in contact
	RESERVE,	// stage, available for the next main effort
}

class DCO_Objective
{
	DCO_EObjectiveType	m_eType			= DCO_EObjectiveType.DEFEND;
	vector				m_vPos;
	float				m_fRadius		= 100.0;
	Faction				m_Faction;
	int					m_iPriorityBias	= 0;		// GM 0..100 added on top of the computed threat score
	bool				m_bGMPlaced		= false;	// true = real draggable entity; false = auto record
	float				m_fThreatScore	= 0;		// computed each commander tick
	int					m_iAssignedCount = 0;		// groups currently assigned (for distribution)

	void DCO_Objective(DCO_EObjectiveType type, vector pos, float radius, Faction faction, int priorityBias, bool gmPlaced)
	{
		m_eType = type;
		m_vPos = pos;
		m_fRadius = radius;
		m_Faction = faction;
		m_iPriorityBias = priorityBias;
		m_bGMPlaced = gmPlaced;
	}
}
```
- [ ] **Step 2: Reload Checkpoint** — reload, expect clean (pure data).

---

## Task 2: Objective registry (GM + auto, derivation, dedupe)

**Files:** Create `Scripts/Game/DCO/Commander/DCO_ObjectiveRegistry.c`

- [ ] **Step 1: MCP-verify** `DCO_CompositionRegistry` API used (`Get()`, and add a `GetAll(out array<ref DCO_CompositionAnchor>)` accessor in Task 2b if needed — see below). Confirm `SCR_AIWorld.GetAIAgents` + group iteration (already used in DCO_Commander).

- [ ] **Step 2: Add a composition accessor** — Modify `Scripts/Game/DCO/Outpost/DCO_CompositionRegistry.c`: add a public method so the objective registry can enumerate bases:
```cpp
	//! Snapshot all cached composition anchors (refreshes if stale). For the AI commander's DEFEND objectives.
	void GetAll(out array<ref DCO_CompositionAnchor> outAnchors)
	{
		RefreshIfStale();
		outAnchors = m_aAnchors;
	}
```

- [ ] **Step 3: Write the registry**
```cpp
/*
 * 25thCRX - DCO Expansion : AI Commander objective registry
 * Singleton (order-independent). Holds GM-placed objectives (registered by DCO_ObjectiveZone) plus auto
 * objectives re-derived each commander tick (undermanned/own bases via DCO_CompositionRegistry; hot fights via
 * the AI world). Dedupes auto objectives against nearby GM-placed ones (the GM marker wins).
 */
class DCO_ObjectiveRegistry
{
	protected static ref DCO_ObjectiveRegistry s_Instance;

	protected ref array<ref DCO_Objective>	m_aGMObjectives;	// from DCO_ObjectiveZone entities
	protected ref array<ref DCO_Objective>	m_aAutoObjectives;	// derived each tick

	static DCO_ObjectiveRegistry Get()
	{
		if (!s_Instance)
			s_Instance = new DCO_ObjectiveRegistry();
		return s_Instance;
	}

	void DCO_ObjectiveRegistry()
	{
		m_aGMObjectives = {};
		m_aAutoObjectives = {};
	}

	//! DCO_ObjectiveZone registers/deregisters its objective here on spawn/delete.
	void RegisterGM(DCO_Objective obj)
	{
		if (obj && m_aGMObjectives.Find(obj) == -1)
			m_aGMObjectives.Insert(obj);
	}
	void UnregisterGM(DCO_Objective obj)
	{
		if (obj)
			m_aGMObjectives.RemoveItem(obj);
	}

	//! Rebuild the AUTO objectives (undermanned own bases + hot friendly fights), then return GM + auto combined.
	//! dedupe: an auto objective within mergeDist of a GM objective of the same faction is dropped (GM wins).
	void BuildActive(out array<ref DCO_Objective> outAll, float baseRadius, int undermannedBelow, float hotThreat, float mergeDist)
	{
		m_aAutoObjectives.Clear();
		DCO_DeriveBaseDefends(baseRadius, undermannedBelow);
		DCO_DeriveHotFights(hotThreat);

		outAll = {};
		foreach (DCO_Objective g : m_aGMObjectives)
			if (g) outAll.Insert(g);

		float mergeSq = mergeDist * mergeDist;
		foreach (DCO_Objective a : m_aAutoObjectives)
		{
			if (!a)
				continue;
			bool superseded = false;
			foreach (DCO_Objective g : m_aGMObjectives)
			{
				if (g && g.m_Faction == a.m_Faction && vector.DistanceSq(g.m_vPos, a.m_vPos) <= mergeSq)
				{
					superseded = true;
					break;
				}
			}
			if (!superseded)
				outAll.Insert(a);
		}
	}

	//! One DEFEND objective per own-faction composition; priority bias scaled up when undermanned nearby.
	protected void DCO_DeriveBaseDefends(float baseRadius, int undermannedBelow)
	{
		array<ref DCO_CompositionAnchor> anchors = {};
		DCO_CompositionRegistry.Get().GetAll(anchors);
		foreach (DCO_CompositionAnchor a : anchors)
		{
			if (!a || !a.m_Faction)
				continue;
			DCO_Objective obj = new DCO_Objective(DCO_EObjectiveType.DEFEND, a.m_vPos, baseRadius, a.m_Faction, 0, false);
			m_aAutoObjectives.Insert(obj);
		}
	}

	//! One SUPPORT objective at each own-faction group that is in contact under heavy threat (the hot fights).
	protected void DCO_DeriveHotFights(float hotThreat)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (!grp || seen.Contains(grp))
				continue;
			seen.Insert(grp);

			SCR_AIGroupUtilityComponent util = grp.GetGroupUtilityComponent();
			if (!util || !util.m_Perception)
				continue;
			array<IEntity> tgts = util.m_Perception.m_aTargetEntities;
			if (!tgts || tgts.IsEmpty())
				continue;	// not in contact
			if (util.GetThreatMeasure() < hotThreat)
				continue;	// not hot enough

			IEntity ldr = grp.GetLeaderEntity();
			if (!ldr)
				continue;
			DCO_Objective obj = new DCO_Objective(DCO_EObjectiveType.SUPPORT, ldr.GetOrigin(), 150.0, grp.GetFaction(), 0, false);
			m_aAutoObjectives.Insert(obj);
		}
	}
}
```
- [ ] **Step 4: Reload Checkpoint** — reload, expect clean. (Registry not yet called; the GetAll accessor must compile against DCO_CompositionRegistry.)

---

## Task 3: Commander brain rework (score → rank → assign → order + viz)

**Files:** Modify `Scripts/Game/DCO/Commander/DCO_Commander.c` (replace the `Update()` body; keep `Get()`/`EnsureRunning()`)

- [ ] **Step 1: MCP-verify** the strength/threat methods chosen in Task 0; confirm `Shape` draw signatures.

- [ ] **Step 2: Replace `Update()`** with the objective-driven loop. (Full code; reuses `DCO_ObjectiveRegistry`, the move/reinforcement primitives, and `m_mLastDispatch` re-issue guard already on the class.)
```cpp
	void Update()
	{
		DCO_MoraleSettings cfg = DCO_MoraleSettings.Get();
		if (!cfg.m_bEnableCommander)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float now = world.GetWorldTime();

		// 1) Build the active objective set (GM-placed + auto-derived, deduped).
		array<ref DCO_Objective> objectives = {};
		DCO_ObjectiveRegistry.Get().BuildActive(objectives, cfg.m_fCmdBaseRadius, cfg.m_iCmdUndermannedBelow, cfg.m_fCommanderThreatTrigger, cfg.m_fCmdMergeDist);
		if (objectives.IsEmpty())
			return;

		// 2) Gather groups once (deduped) with their faction, leader pos, contact state, strength.
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return;
		array<AIAgent> agents = {};
		aiWorld.GetAIAgents(agents);
		set<SCR_AIGroup> seen = new set<SCR_AIGroup>();
		array<SCR_AIGroup> groups = {};
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			SCR_AIGroup grp = SCR_AIGroup.Cast(agent.GetParentGroup());
			if (grp && !seen.Contains(grp))
			{
				seen.Insert(grp);
				groups.Insert(grp);
			}
		}

		// 3) Score each objective (METT-TC-lite): threat near it + GM bias; type weight.
		foreach (DCO_Objective obj : objectives)
			obj.m_fThreatScore = DCO_ScoreObjective(obj, groups, cfg);

		// 4) Assign idle/uncommitted groups to the highest-scoring objectives that need force (main effort first),
		//    issuing orders via the proven primitives; throttle re-issue per group.
		DCO_AssignAndOrder(objectives, groups, cfg, now);

		// 5) GM visualization.
		if (cfg.m_bCmdVisualize)
			DCO_DrawPlan(objectives);
	}

	//! METT-TC-lite: enemy strength near the objective + under-attack/undermanned spikes + GM bias − distance is
	//! applied at assignment time (per reserve). Higher = more urgent.
	protected float DCO_ScoreObjective(DCO_Objective obj, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg)
	{
		float score = obj.m_iPriorityBias;	// GM bias
		// Type base weight.
		switch (obj.m_eType)
		{
			case DCO_EObjectiveType.SUPPORT: score += cfg.m_fCmdWeightContact; break;	// a fight in progress
			case DCO_EObjectiveType.DEFEND:  score += cfg.m_fCmdWeightDefend; break;
			case DCO_EObjectiveType.ATTACK:  score += cfg.m_fCmdWeightAttack; break;
			default: break;
		}
		// Enemy strength near the objective (count enemy-faction groups within radius).
		float rSq = obj.m_fRadius * obj.m_fRadius;
		int enemyNear = 0;
		int friendlyNear = 0;
		foreach (SCR_AIGroup g : groups)
		{
			IEntity ldr = g.GetLeaderEntity();
			if (!ldr)
				continue;
			if (vector.DistanceSq(ldr.GetOrigin(), obj.m_vPos) > rSq)
				continue;
			if (g.GetFaction() == obj.m_Faction)
				friendlyNear++;
			else
				enemyNear++;
		}
		score += enemyNear * cfg.m_fCmdWeightEnemy;
		// Friendly deficit (fewer friendlies than enemies near the objective) spikes the need.
		if (enemyNear > friendlyNear)
			score += (enemyNear - friendlyNear) * cfg.m_fCmdWeightDeficit;
		return score;
	}

	//! Assign the nearest uncommitted reserves to the most-urgent objectives until each is covered or reserves
	//! run out. Reserves = own-faction groups NOT in contact and off cooldown. Main effort (top score) first.
	protected void DCO_AssignAndOrder(array<ref DCO_Objective> objectives, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg, float now)
	{
		// Sort objectives by score desc (simple selection; counts are small).
		array<ref DCO_Objective> ordered = {};
		array<ref DCO_Objective> pool = {};
		foreach (DCO_Objective o : objectives) pool.Insert(o);
		while (!pool.IsEmpty())
		{
			int bi = 0;
			for (int i = 1; i < pool.Count(); i++)
				if (pool[i].m_fThreatScore > pool[bi].m_fThreatScore) bi = i;
			ordered.Insert(pool[bi]);
			pool.Remove(bi);
		}

		foreach (DCO_Objective obj : ordered)
		{
			if (obj.m_fThreatScore < cfg.m_fCmdActThreshold)
				continue;	// not urgent enough to commit reserves
			int need = cfg.m_iCmdGroupsPerObjective;
			int assigned = 0;
			while (assigned < need)
			{
				SCR_AIGroup best = DCO_NearestReserve(obj, groups, cfg, now);
				if (!best)
					break;
				SCR_AIGroupUtilityComponent util = best.GetGroupUtilityComponent();
				if (!util)
					break;
				util.DCO_SetReinforcementEligible(true);
				DCO_VehicleUtil.OrderGroupMoveToPosition(best, obj.m_vPos, util.m_Mailbox);
				m_mLastDispatch.Set(best, now);
				assigned++;
				if (cfg.m_bDebug)
					DCO_Debug.LogGroup("COMMANDER", best.GetLeaderEntity(), string.Format("-> objective %1 (score %2)", typename.EnumToString(DCO_EObjectiveType, obj.m_eType), obj.m_fThreatScore));
			}
		}
	}

	//! Nearest own-faction reserve to the objective that is not in contact and off cooldown. Marks none.
	protected SCR_AIGroup DCO_NearestReserve(DCO_Objective obj, array<SCR_AIGroup> groups, DCO_MoraleSettings cfg, float now)
	{
		SCR_AIGroup best;
		float bestSq = cfg.m_fCommanderDispatchRange * cfg.m_fCommanderDispatchRange;
		foreach (SCR_AIGroup g : groups)
		{
			if (g.GetFaction() != obj.m_Faction)
				continue;
			SCR_AIGroupUtilityComponent util = g.GetGroupUtilityComponent();
			if (!util || !util.m_Perception)
				continue;
			array<IEntity> tgts = util.m_Perception.m_aTargetEntities;
			if (tgts && !tgts.IsEmpty())
				continue;	// busy fighting
			float last;
			if (m_mLastDispatch.Find(g, last) && (now - last) < cfg.m_fCommanderReissueSec * 1000.0)
				continue;	// recently vectored
			IEntity ldr = g.GetLeaderEntity();
			if (!ldr)
				continue;
			float dSq = vector.DistanceSq(ldr.GetOrigin(), obj.m_vPos);
			if (dSq < bestSq)
			{
				bestSq = dSq;
				best = g;
			}
		}
		return best;
	}

	//! GM plan view: a wireframe sphere per objective (colour by type), HELD in m_aVizShapes so it persists
	//! between commander ticks; cleared + rebuilt each draw (releasing a held Shape ref frees it). Mirrors
	//! DCO_QRFRangeVisual. VERIFIED: Shape.CreateSphere(int color, ShapeFlags, vector origin, float radius);
	//! colours are HEX ARGB ints (there is NO ARGB() helper). NOTE: Shapes render client-side, so on a dedicated
	//! server a remote GM won't see them (visible on listen-host / SP GM) - a replicated marker is a later step.
	protected void DCO_DrawPlan(array<ref DCO_Objective> objectives)
	{
		m_aVizShapes = {};	// release last tick's shapes
		foreach (DCO_Objective obj : objectives)
		{
			Shape s = Shape.CreateSphere(DCO_ObjectiveColour(obj.m_eType), ShapeFlags.WIREFRAME | ShapeFlags.NOZBUFFER, obj.m_vPos, obj.m_fRadius);
			if (s)
				m_aVizShapes.Insert(s);
		}
	}

	protected int DCO_ObjectiveColour(DCO_EObjectiveType t)
	{
		switch (t)
		{
			case DCO_EObjectiveType.SUPPORT: return 0xFFFF3C3C;	// red - a fight in progress
			case DCO_EObjectiveType.DEFEND:  return 0xFF3C78FF;	// blue - hold/defend
			case DCO_EObjectiveType.ATTACK:  return 0xFFFFA000;	// orange - attack
		}
		return 0xFFA0A0A0;	// grey - other
	}
```
- [ ] **Step 3: MCP-verify** `Shape.CreateSphere` arg order + `ShapeFlags.ONCE` (drawn-this-frame) exists; if `ONCE` differs, use the persistent-shape-held pattern from `DCO_QRFRangeVisual.c` instead. Confirm `ARGB` global helper exists (used widely) and `typename.EnumToString`.
- [ ] **Step 4: Reload Checkpoint** — reload, expect clean. In GM enable Commander + the viz toggle; confirm objective spheres draw over bases/fights and reserves vector toward them.

---

## Task 4: Settings + GM viz toggle

**Files:** Modify `DCO_MoraleSettings.c`, `DCO_MoraleEditorAttributes.c`, `DCO_Attributes.conf`, `DCO_JsonConfig.c`

- [ ] **Step 1:** Add to `DCO_MoraleSettings.c` (near the existing commander settings `m_fCommanderIntervalSec` etc.):
```cpp
	// AI COMMANDER - objective system (extends the lean commander).
	bool	m_bCmdVisualize			= false;	// draw the objective plan in GM
	float	m_fCmdBaseRadius		= 150.0;	// DEFEND objective radius around an own composition
	int		m_iCmdUndermannedBelow	= 2;		// (reserved) base considered undermanned below this many friendly groups
	float	m_fCmdMergeDist			= 120.0;	// an auto objective within this of a GM objective is superseded
	int		m_iCmdGroupsPerObjective = 1;		// reserves committed per urgent objective per tick
	float	m_fCmdActThreshold		= 1.0;		// minimum score to commit reserves to an objective
	// Scoring weights (METT-TC-lite).
	float	m_fCmdWeightContact		= 100.0;	// a SUPPORT/fight objective base urgency
	float	m_fCmdWeightDefend		= 30.0;
	float	m_fCmdWeightAttack		= 20.0;
	float	m_fCmdWeightEnemy		= 25.0;		// per enemy group near the objective
	float	m_fCmdWeightDeficit		= 40.0;		// per friendly-deficit unit near the objective
```
- [ ] **Step 2:** Add a `DCO_CmdVisualizeEditorAttribute` (checkbox) in `DCO_MoraleEditorAttributes.c` mirroring an existing bool attribute, reading/writing `m_bCmdVisualize`.
- [ ] **Step 3:** Register that attribute in `DCO_Attributes.conf` under the existing `DCO.conf` category (checkbox layout, a new unique embedded id e.g. `{5DC0DC4000000045}`).
- [ ] **Step 4:** Add `m_bCmdVisualize` to `DCO_JsonConfig.c` load + write (one key `cmd_visualize`), matching the file's existing pattern.
- [ ] **Step 5: Reload Checkpoint** — reload, expect clean; the toggle appears in the "25th DCO" tab.

---

## Task 5: GM-placed objective placeable (`DCO_ObjectiveZone`) + attributes

**Files:** Create `DCO_ObjectiveZone.c`, `DCO_ObjectiveEditorAttributes.c`; create the prefab + placeable `.conf` (Workbench).

- [ ] **Step 1: MCP/read** `Scripts/Game/DCO/Roles/DCO_TaskZone.c` + `DCO_TaskZoneEditorAttributes.c` fully — copy the placeable component + entity-scoped attribute pattern (and heed memory [[taskzone-placeable-registration]]: the prefab must have NO `m_EntityInteraction`).
- [ ] **Step 2: Write `DCO_ObjectiveZone.c`** — a `ScriptComponent` on the placeable that builds a `DCO_Objective` (with `m_bGMPlaced = true`), keeps its `m_vPos` synced to the entity origin each tick (so dragging moves the objective), reads its faction, and registers/unregisters with `DCO_ObjectiveRegistry`. (Full code mirrors `DCO_TaskZone` lifecycle: `OnPostInit` register via `CallLater(...,0)`; `OnDelete` unregister; a throttled tick to refresh `m_vPos`/attributes from the entity.)
- [ ] **Step 3: Write `DCO_ObjectiveEditorAttributes.c`** — entity-scoped attributes (gated to the `DCO_ObjectiveZone`/editable entity, NOT the game mode): Objective Type (ButtonBox_Selection dropdown DEFEND/HOLD/ATTACK/PATROL/SUPPORT/RESERVE), Priority (slider 0–100), Radius (slider 25–500). Mirror `DCO_TaskZoneEditorAttributes.c` for the read/write-against-the-component pattern + the dropdown `m_aValues` pattern from the base-settings dropdowns.
- [ ] **Step 4: Workbench — create the placeable** (assigns GUIDs): duplicate the `DCO_TaskZone` prefab → `DCO_ObjectiveZone.et`, swap its component to `DCO_ObjectiveZone`, ensure NO `m_EntityInteraction`; duplicate the Task Zone placeable-registry entry so it appears in the GM browser under Systems; create an entity-attribute category `.conf` for the three attributes. Record the assigned GUIDs.
- [ ] **Step 5: Reload Checkpoint + in-GM** — reload clean; place a `DCO_ObjectiveZone` in GM, drag it, set its type/priority/radius; confirm the commander services it (a reserve vectors to it) and it draws.

---

## Task 6: Memory

- [ ] **Step 1:** Create `<auto-memory>/ai-commander-system.md` (type project) recording the architecture (objective record/registry/zone/commander), the auto-vs-GM split, the scoring weights, reuse of reinforcement/composition primitives, and the Workbench-created placeable GUIDs + clean-compile CRC. Add the `MEMORY.md` index line.

---

## Self-Review

**Spec coverage:** objectives auto + GM-placed (Tasks 1,2,5 ✓); draggable placeable (Task 5 ✓); threat ranking/METT-TC-lite (Task 3 `DCO_ScoreObjective` ✓); order generation via existing primitives (Task 3 `DCO_AssignAndOrder` ✓); auto base-defense + assist-fight (Task 2 ✓); main-effort first (Task 3 score-sort ✓); GM visualization + toggle (Tasks 3,4 ✓); dedupe GM-vs-auto (Task 2 `BuildActive` mergeDist ✓); default-OFF/additive (gated on `m_bEnableCommander` ✓).

**Placeholder scan:** Task 5 Steps 2–3 describe the placeable/attribute code by reference to `DCO_TaskZone` rather than inlining it — this is intentional (the implementer copies the proven sibling and adapts the 3 fields); the structure, gating, and the dropdown `m_aValues` source are named explicitly. No vague "handle edge cases".

**Type consistency:** `DCO_Objective` fields (`m_eType/m_vPos/m_fRadius/m_Faction/m_iPriorityBias/m_bGMPlaced/m_fThreatScore`), `DCO_ObjectiveRegistry.BuildActive/RegisterGM/UnregisterGM`, `DCO_CompositionRegistry.GetAll`, and the `m_fCmd*` settings names are used consistently across Tasks 1–5.

**Open risks carried to execution:** `Shape.CreateSphere`/`ShapeFlags.ONCE` (Task 3 Step 3 — fall back to the held-Shape pattern of `DCO_QRFRangeVisual` if `ONCE` is absent); the group-strength method (Task 0); `DCO_DefendComponent` external entry (Task 0 — if none, DEFEND = hold-at-position via the move primitive). All are MCP-verified before coding.
