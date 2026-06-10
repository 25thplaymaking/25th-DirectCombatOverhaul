# 25thCRX — MODPLAN

> ℹ️ **CRX 1.7 shims REMOVED (2026-06-07).** The two former compatibility patches under
> `Scripts/Game/CRX_EAI_1-6-0-119_1-3-71/Modded/` (`SCR_EditableCharacterComponent.SetTransform` `void`→`bool`,
> and `SCR_FactionManager` faction-limit map key `string`→`FactionKey`) are GONE — CRX updated to be natively
> 1.7-compatible, so the shims are no longer needed and were deleted from the project. The Game module compiles
> clean without them (CRC 0229de2e, 2026-06-07) and no 25thCRX code references them.
> **Do NOT re-add them** — against the updated CRX they would re-introduce the very 1.6 signature mismatch they
> once fixed (which on 2026-05-29, in the opposite direction, took the server down). If CRX/the game updates
> again, re-verify the live signatures before adding any new override.


## Vision
`25thCRX` is the 25th unit's combined A.I. mod. It (1) bundles and 1.7-patches **CRX Enfusion A.I.** + **SkarrisCRXAIRealism**, and (2) layers a **DCO-style tactical/immersion expansion** ("Dynamic Combat Operations") on top of CRX's systems — inspired by the Arma 3 mods *DCO Soldier FSM* (YipMan) and *Enhanced Tactical AI & TCL* (Niceman). The goal is human-like, morale-driven AI that breaks, calls for help, maneuvers under smoke, and prioritizes high-value targets — without rebuilding what CRX already does.

## Identity
- **Addon:** `25thCRX`  |  **GUID:** `697A1949E09F26D2`
- **Dependencies (load after):** CRX Enfusion A.I. `5F268647F8A1A1F4`, SkarrisCRXAIRealism `68E33AC5D41A7D7B`, Arma Reforger `58D0FB3206B6F859`
- **Target game version:** 1.7.0.41
- **New-code class prefix:** `DCO_`  |  **New-code folder:** `Scripts/Game/DCO/`
- **CRX override copies** (1.7 fixes) live under their original path: `Scripts/Game/CRX_EAI_1-6-0-119_1-3-71/Modded/...` (file-path overrides that replace CRX's packed versions)

## Architecture notes
- New combat actions are added as **`SCR_AIBehaviorBase` subclasses** (priority-evaluated, behavior-tree backed) — the engine's supported extension point.
- Cross-cutting state (morale) lives in a **`DCO_` component** hooked into CRX's `SCR_AIUtilityComponent` / `SCR_AIInfoComponent` / threat & suppression state via `modded` — reuse CRX data, don't recompute.
- Group coordination uses the native **`AIMessage` / `SCR_AIMessageGoal`** system + `SCR_AIMessageHandling` + CRX `SCR_AICommsHandler`.
- All tunables exposed through a **Game Master settings component + editor attributes**, mirroring CRX's `SCR_AISettingsComponent` pattern, so 25th admins tune in-game. Every phase ships behind a toggle.
- Always call `super.` in `modded` overrides unless intentionally replacing. Verify every API signature against the 1.7 DB before use.

## Decisions
- **POW / capture / restrain:** AI side done (surrender); player capture is anim-gated → out of scope without an animation dependency (Bryce 2026-05-30).
- **Indirect fire / artillery:** DROPPED — base game already provides orderable artillery (Bryce 2026-05-30).
- **Casualty drag (R7):** DROPPED for now — needs an animation dependency not in scope (Bryce 2026-05-30).
- **Dynamic-simulation / distant-AI freezing (Ref 2 perf layer):** SKIPPED — Reforger already provides native AI LOD/limiting; no duplication.

## 🔧 LATEST — Playtest tuning + Scenario Settings parity (2026-06-03/04)

Post-playtest refinement of the v1.1.5 systems (in-engine feedback), one new system, and a Scenario
Settings overhaul. All on disk and integrity-checked after a mid-session power loss (no work lost).

**New system**
- **Machine-Gunner Emplacement** (`MachineGun/DCO_MachineGunner.c`, default OFF): an MG carrier
  (`SCR_AICombatComponent.GetCurrentWeaponType() == EWeaponType.WT_MACHINEGUN`) deploys prone + holds a
  firing position with LOS instead of maneuvering; repositions to a higher LOS spot if blocked.

**Tuning (data + behaviour, all default-preserving)**
- Reaction delay `m_fAttackReactionDelayModifier` `-1.9 → -0.5` in `Prefabs/AI/SCR_AIWorld.et` (kills the
  spot-to-aim freeze without the matrix-dodge).
- Suppression **dig-in** (`Morale/DCO_Suppression.c`): dash only to close cover that holds the high ground
  (`m_fSuppressionDashMax` 8 m / `m_fSuppressionMaxDescend` 4 m), else go prone in place. `m_bSuppressionDigIn` ON.
- Vehicle **peek-and-shoot** (`Armor/DCO_VehicleCombat.c`): `m_fVehicleBackoffHoldSec` (6 s) pops a
  hull-down vehicle back out to fight instead of hiding forever.
- **AT-vs-infantry** more aggressive (`ATInfantry/DCO_ATAntiInfantry.c`): `m_fATInfantryCheckSec` 3→1,
  `m_iATInfantryMinCluster` 2→1.
- **Proactive asset use** (`Assets/DCO_AssetUse.c`): `m_bAssetProactive` (ON) — defenders man a static
  weapon out of contact with a spare member.
- **Tactical-movement life preservation** (`Movement/DCO_TacticalPath.c` + `DCO_TacticalMoveSettings`):
  `m_bPathHaltOnContact` (halt the advance on contact within `m_fPathContactHaltRange` 200 m),
  `m_bPathLeaderInBase` (leader's element = static base of fire), `m_bPathBaseMaintainLos` (+
  `m_fPathBaseLosRadius` 15 m, base member with no LOS repositions). All ON.

**Scenario Settings parity** (`Morale/DCO_MoraleSettingsComponent.c`): the optional designer component now
exposes the master toggle + key tunables for **every** DCO system across BOTH stores (`DCO_MoraleSettings`
and `DCO_TacticalMoveSettings`), grouped into editor categories (25th DCO / - Surrender / - Infantry /
- Coordination / - Vehicles / - Movement). 95 attributes, defaults matched 1:1 to the baked store defaults
(so placing the component is behaviour-neutral until a switch is flipped). Fixed a latent default bug:
`m_bEnableSharedMorale` and `m_bEnableFriendlyFire` were `"0"` while the store defaults them `true` — they
now match, so adding the component no longer silently disables shared-morale / fratricide-avoidance. Note:
for the fields it exposes, the component OVERRIDES the JSON config (it applies after the JSON load).

**All settings remain wired across 4 surfaces** (store default → JSON read → JSON write → example.json),
verified consistent.

## ✅ CONSOLIDATED CURRENT STATE (2026-05-30) — read this first
**Compile:** Bryce's reload log validated 15/16 of the first batch (incl. `EAISkill.ROOKIE/REGULAR`, the
vehicle-hijack `QueryEntitiesBySphere` callback + `EAICompartmentType.Pilot`, HVT `SCR_AITargetInfo`, night
illum). The ONLY error was `SCR_AIActivitySmokeCoverFeature.Execute` — root-caused 2026-05-31 from the real source
Bryce pasted: arg 3 `SCR_AIActivitySmokeCoverFeatureProperties` is an ENUM and I'd passed `null` (Enforce
args are 0-indexed, hence "argument 2"); FIXED by passing `.NONE`. `DCO_DeploySmoke` is now LIVE (drops a
screen between the group and the threat on break). ADDED SINCE that log (code-complete, verified vs
source, pending the next reload to confirm): R5 vehicle-borne reinforcement, Defensive Hold, hit-flinch (built
from Bryce's pasted `SCR_DamageManagerComponent` source), hide-from-armour. The six "no need to Cast for
up-casting" warnings were cleaned; only the harmless `CreateMessage` deprecation remains.

**Implemented features (all server-side, GM-toggled in the "25th DCO" tab; morale fixes are ON by default):**
- MORALE CORE: group morale meter; **surrender hold** (DeactivateAI freezes the prone pose — fixes "won't lie
  down") + immersive surrender (disarm/neutral/prone) + grenade-ambush variant; **flee** (caches last-known
  enemy so it flees AWAY, re-issues to commit — fixes "aimless flee"); **panic** band (R1); **last-stand** no-surrender (R3);
  **morale→accuracy** (R2, `SCR_AICombatComponent.SetAISkill`); **hide-from-armour** (on-foot group flees a
  perceived enemy vehicle in range).  *(The "overall morale issue" Bryce raised = the surrender-hold +
  flee-direction fixes; both ON by default.)*
- COORDINATION: reinforcement / shared SA (P2) + **vehicle-borne reinforcement** (R5, distant responders mount up); per-group **QRF**; per-group **ambush** hold-fire (R4); per-group **defensive hold** (garrison-lite); **shared morale pool** (F2 — small teams don't surrender early); **fratricide avoidance** (F1 — hold fire when a friendly is in the lane); **straggler merge** (F3 — fold lone survivors into a larger group).
- MOVEMENT: tactical-move M1 (cautious WALK + anti-funnel side-step) + **M4 exposure scoring** (route to least-LOS-exposed spot); **adaptive formation** (column travelling / line in contact); stance cooldown.
- VEHICLES: **armour angling** (front-to-threat); **hijacking** (commandeer empty vehicle, P4).
- TARGETING/SUPPORT: **HVT** priority on enemy vehicles (P3); **night illumination** flares (P3.6).
- IMMERSION/REACTIVE: surrender **voiceline** framework; **hit-reaction flinch** (Phase 7 - AI holds fire briefly when shot); **smoke-on-flee** (breaking group screens its withdrawal); GM tuning sliders.

**GM tab icon:** `m_sImageName` set to a base `.edds` (Bryce wired `icon_excl_mark.edds`).

## Phase 7 — Reactive combat (VCOM-grounded)  [NEXT, defined 2026-05-30]
Cross-referenced VCOM AI (Genesis92x, the linked mod) against 25thCRX: cover-to-cover, reinforcements, hijack,
medical, suppressed-response, static weapons, scavenging are ALL already covered (CRX or DCO). The genuinely
NEW + engine-grounded gaps VCOM has that we don't:
- **M1 — Adaptive formation ✅ IMPLEMENTED** (`Scripts/Game/DCO/Formation/DCO_FormationComponent.c`). Switches
  formation by situation: `m_sFormationTravel` ("Column") when no contact, `m_sFormationContact` ("Line") in
  contact, applied via `AIFormationComponent.SetFormation(string)` on the group entity
  (`GetOwner().FindComponent(AIFormationComponent)`), re-applied only on change. Enum members resolved =
  `Wedge/Line/Column/StaggeredColumn` (used as the strings). GM toggle "Adaptive Formation" (`{5DC0DC1C…}`, default OFF).
  RUNTIME-TUNE (NOT compile risks - the call is sound either way): whether the component is on the group entity
  and whether "Column"/"Line" are the exact `SetFormation` names; both adjustable via the string settings.
- **M2 — Hit-reaction / flinch ("disorientation when shot") ✅ IMPLEMENTED** (`Scripts/Game/DCO/Reaction/DCO_HitFlinch.c`).
  Bryce supplied the base source: `OnDamage(notnull BaseDamageContext)` is an overridable protected method on
  `SCR_CharacterDamageManagerComponent`. So we `modded`-override it: call super, then (AI-only via
  `IsAIActivated()`, non-healing damage, throttled) `SetWeaponNoFireTime(m_fHitFlinchDuration)` for a brief
  held-fire. GM toggle "Hit Reaction Flinch" (`{5DC0DC1A…}`, default OFF).
M1 (formation) is still gated on the `SCR_EAIGroupFormation` members + the group's formation setter (Script
Editor). I will NOT guess enum/signature values and risk re-breaking the compile.

## Viability matrix (validated via MCP api_search on 1.7)
| Feature | Status | API basis |
|---|---|---|
| Morale + break/flee/surrender | Custom (on CRX) | `SCR_AIMessage_Flee`, CRX flee/retreat + threat/suppression state, surrender via `SCR_AIMessage_Animate` |
| Reinforcement / call-for-help | Extend | `AIMessage`/`SCR_AIMessageGoal`, `SCR_AIMessageHandling`, CRX `SCR_AICommsHandler`, High Command |
| Smoke-covered retreat / breach | Native wire-in | `SCR_AIDeploySmokeCover`, `SCR_DeploySmokeCoverWaypoint`, `SCR_AIActivitySmokeCoverFeature` |
| Marksman / HVT targeting | Extend | CRX target clusters, `SCR_AIGroupPerception`, `PerceptionComponent.GetTargetsList(ETargetCategory)` |
| Vehicle hijacking (empty vehicles) | Extend | `SCR_AIMessageHandling.SendGetInMessage`, CRX `SCR_AIGroupVehicleManager` |
| Garrison + coordinated room clearing | Custom | CRX CQB mode + cover system + custom garrison slots |
| POW / capture / restrain | Stretch | No native restraint; custom interaction + anim |
| AI explosives on fortifications | Stretch | No clean AI charge-placement API |

## Already complete
### 1.7 compatibility port — RETIRED (2026-06-07)
CRX was built for 1.6.0.119 and originally failed to compile on 1.7.0.41, fixed via two file-path override shims in 25thCRX. **CRX has since updated to be natively 1.7-compatible, so both shims were removed** (`Scripts/Game/CRX_EAI_1-6-0-119_1-3-71/Modded/` no longer exists). The Game module compiles clean without them (CRC 0229de2e); no 25thCRX code references the old shim symbols. Do NOT re-add the shims (see the banner at the top of this file). Historical note — the shims were:
- `SCR_AIHelpers/SCR_EditableCharacterComponent.c` — `SetTransform` return type `void`→`bool`.
- `SCR_FactionManager.c` — `map<string,int>`→`map<FactionKey,int>` (`SCR_MissionHeader.GetFactionLimitMap()` changed).

## GM registration FIXED + morale rebalance (2026-05-29 PM) — compiles clean (CRC bbcc1275)
- GM TAB ROOT CAUSE: the runtime `modded SCR_AttributesManagerEditorComponentClass` ctor did NOT surface attributes in-GM (compiled but no tab). Correct mechanism (from open-source GME mod, `github.com/zen-mod/GME_AR`): OVERRIDE the editor-mode prefab. Implemented `Prefabs/Editor/Modes/EditorModeEdit.et` (base GUID `59EF8ECAE1DCD417`, inherits EditorModeBase `E56F54E533ACE527`, AttributesManager component `54C8DACDCD4AE14E`) adding `DCO_Attributes.conf` to `m_AttributeLists` (additive → CRX tabs preserved). Deleted the modded-ctor registrar. Global morale attrs now gate `ReadVariable` on `SCR_BaseGameMode.Cast(item)` (the Scenario-properties panel's item) — required for the global tab. See memory `gm-attribute-registration.md`.
- SERVER TUNING: all 10 DCO attributes are now `m_bIsServer 1` so GM changes apply on the dedicated server.
- MORALE REBALANCE (B): surrender now requires `lossFrac >= m_fMinCasualtyFractionForSurrender` (0.30) AND `Math.RandomFloat01() <= m_fSurrenderChancePerTick` (0.25); flee gated by `m_fFleeChancePerTick` (0.40). Stops full-strength units surrendering from mere proximity; gradual, not instant.
- SURRENDER EMPTY-HANDS (C): `DCO_DropHeldItem` CallLater(400ms,1000ms) clears the grenade/pistol the weapon manager auto-equips after the rifle drop.
- D IMPLEMENTED (fake-surrender grenade ambush) — compiles clean (CRC c96cc1cf). Mechanism uses the REAL grenade system (no spawn/GUID guessing): `BaseWeaponManagerComponent.GetCurrentGrenade()` → `SelectWeapon(grenade)` → `Throw(forwardAxis, m_fFakeSurrenderThrowSpeed=0.05)` (near-zero speed = drops the primed grenade right in front). In `DCO_GroupMorale.c`: `DCO_BeginAgentSurrender` rolls `m_fFakeSurrenderChance` (0.25); fakers go `DCO_BeginFakeSurrender` (weapon lowered + prone, KEEP grenade/faction, no gear/hand clear) and are added to `m_aDCO_FakeSurrenderWatch`; a 0.5s `DCO_FakeSurrenderTick` drops the grenade when a perceived enemy is within `m_fFakeSurrenderTriggerRange` (6 m). All fake-surrender methods live in DCO_GroupMorale.c (same fragment as the surrender code → no cross-fragment ordering issue).
- D RUNTIME UNKNOWNS to watch in-GM: (1) the select→Throw sequence on a prone/lowered AI may need a brief weapon-raise or different timing (currently SelectWeapon + 350ms + Throw); (2) `DCO_MaintainSurrender` re-lowers weapons every 2s for the whole group and may fight the throw — exclude fakers if needed; (3) enemy detection uses the group's perception targets (could be empty if the group fully lost perception). All tunable in `DCO_MoraleSettings`.

## Slider fix + grenade toggle + stance cooldown (2026-05-29 PM)
- SLIDER FIX: slider attributes (`m_baseValues`) MUST extend `SCR_BaseValueListEditorAttribute`, not `SCR_BaseEditorAttribute`, or the widget shows `MISSING m_SliderData!`. Switched Flee/Surrender thresholds, Reinforcement Radius/Max Responders, QRF Range. Checkboxes stay on `SCR_BaseEditorAttribute`. (Compiled clean, CRC 7b25482e.)
- GRENADE-DROP REWORK (per Bryce): now a GM toggle "Surrender Grenade Drop" (`DCO_EnableGrenadeDropEditorAttribute` → `m_bEnableFakeSurrender`). OFF = genuine empty-handed surrender; ON = surrendering unit drops only its rifle (keeps grenade) and is watched. The 25% roll is DEFERRED to the moment a PLAYER comes within `m_fFakeSurrenderTriggerRange` (6 m) - `DCO_FakeSurrenderTick` detects players via `PlayerManager.GetPlayers`/`GetPlayerControlledEntity`, rolls ONCE per unit (one-shot), drops primed grenade on success. (Compiled clean, CRC dee0adea.)
- STANCE COOLDOWN (anti-jitter): first attempt (`modded SCR_CharacterControllerComponent.SetStanceChange`) FAILED — `SetStanceChange`/`ForceStance` are `proto external` and can't be overridden (`Multiple declaration`, module down). IMPLEMENTED PROPERLY via the SCRIPTED combat-move request queue: `modded SCR_AICombatMoveState.ApplyNewRequest(...)` in `Scripts/Game/DCO/Movement/DCO_StanceCooldown.c`. Throttles only pure in-place stance requests (`SCR_AICombatMoveRequest_ChangeStance` / `_ChangeStanceInCover`); MOVE requests pass through untouched. Per-instance `m_fDCO_LastStanceChangeMs`; drops a request if within `m_fStanceCooldownSec*1000`. AI-only (players have no combat-move state); opt-in via "AI Stance Cooldown" toggle (`m_bEnableStanceCooldown`, default OFF), conf GUID `{5DC0DC0F...}`. Compiled clean CRC `eacc4b98`. See memory `stance-cooldown-mechanism.md` + `enforce-gotchas.md`.
- STANCE BUGFIX (instant-prone-in-movement): added `!IsMoving()` guard to the throttle — a pure stance request can arrive mid-move (e.g. prone at end of a bound); dropping it made the movement system apply the stance via its snappier path ("instant" prone). Now stance is never throttled while moving (also honours "moving to cover not penalised"). Removed the now-dead `m_fStanceMoveSpeedThreshold`.

## Live-issue pass (2026-05-29 eve, "workshop at the forefront")
- GEAR-SHUDDER FIX (stance cooldown): enemy armor/chest rig shuddered & "flew up and down" with the cooldown enabled. Cause: we were throttling `SCR_AICombatMoveRequest_ChangeStanceInCover` — the engine's rapid cover peek/duck (rise-to-fire / drop-back). Dropping those interrupted the cover-pose transition, jittering the torso-attached gear physics. FIX: `DCO_IsThrottledStanceRequest` now matches ONLY `SCR_AICombatMoveRequest_ChangeStance` (free-standing prone/crouch/stand cycling); in-cover peek/duck always passes through. Cooldown now shapes the stance DECISION only, never the cover animation/gear. ✅ COMPILED CLEAN (CRC `77e587f0`). RUNTIME CONFIRM PENDING: enable "AI Stance Cooldown" in-GM and verify the armor/chest-rig shudder is gone while the prone/crouch twitch is still calmed.
- SURRENDER — ARMED-WHILE-WHITE FIX (root cause): `DCO_BeginAgentSurrender` cleared the faction IMMEDIATELY but disarmed on a delay (and only if `m_bDropGearOnSurrender`), so a still-armed unit went neutral/"white" mid-fight and was discounted by friendly AI. FIX: faction is now neutralized via new `DCO_NeutralizeFaction`, scheduled ONLY after the weapon drop. Weapon-drop is now UNCONDITIONAL on genuine surrender (a surrendered unit must be unarmed); gear-drop stays the `m_bDropGearOnSurrender` toggle (moved inside `DCO_AgentDropGear`). Fakers (grenade ambush) drop the rifle then go white after 700 ms — kept grenade is the intended player-facing trap. No armed-while-white window.
- GM TAB: category `DCO.conf` priority 50→1 to push "25th DCO" toward the END (DIRECTION UNCONFIRMED — verify in-GM; flip high if it lands at the top). Name already "25th DCO".
- GM TAB "no properties" — ROOT CAUSE FOUND (console.log, 17:14): the category config `DCO.conf` FAILS TO PARSE — `DEFAULT (E): Unknown keyword/data 'm_Info' at offset 38`. So `SCR_EditorAttributeCategory`'s UI-info member is NOT `m_Info`; the category never loaded → tab empty/unnamed all along. FIX: `m_Info` → `m_UIInfo` in `DCO.conf` AND `DCO_QRF.conf` (matches the working `m_UIInfo` member used by the attributes; getter is `GetInfo()` but backing member is `m_UIInfo`). VERIFY in-GM: reopen Scenario Properties → tab should now populate + be named "25th DCO"; if log still shows `Unknown keyword 'm_UIInfo'`, get the real member from Script Editor autocomplete on `SCR_EditorAttributeCategory`. (The MCP `wb_resources rebuild` "Cannot find metafile configuration / Build failed" is a separate harmless quirk — the parse error only surfaces on actual Config load.) Confirmed NO unpacked CRX/Skarris override of `EditorModeEdit.et` / attrs-manager `54C8DACDCD4AE14E` / `m_AttributeLists` (grep of all siblings → only 25thCRX), so not a prefab conflict. Temp `Print` added in `DCO_EnableMoraleEditorAttribute.ReadVariable` as backup diagnostic.
- TACTICAL MOVE diagnosis: added `Print` in `SCR_AIMoveActivity.InitParameters` (logs inType/hasEntity/threat/dist/outType) + validation flags in `DCO_TacticalMoveSettings` (`m_bDebugLog=true`, `m_bForceIgnoreThreat=true`) so the WALK is visible without needing a live threat. Likely the user tested peaceful (option-a threat gate) OR `InitParameters` isn't the engine's move entry — the log decides. SET BOTH FLAGS FALSE for the clean release.

## ✅ Pre-publish production review (2026-05-29, FINAL) — DCO GM layer COMPLETE & compiling clean
Full review done before Bryce publishes to live end-users. Final clean compile at 12:07:57 (no `SCRIPT (E)`, no module failure, no `Unknown class`/`Wrong GUID`). Production-safety verdict:
- All gameplay logic (morale/reinforcement/QRF + every editor-attribute write) is `Replication.IsServer()`-gated; server-authoritative.
- The `modded SCR_AttributesManagerEditorComponentClass` ctor runs for EVERY editor mode at init for ALL users — it is null-guarded, GM-gated (only the "Edit" set via `SCR_DaytimeEditorAttribute`), and the resource load is hardened (explicit `GetResource()` null-check) so it cannot deref.
- QRF defaults to OFF per group (`m_bDCO_IsQRFResponder=false`) — no surprise behavior on publish; GM must flag a group.
- QRF range circle is client-side only (GM screen) — no server/crash impact; worst case it doesn't render.
- IP-clean: zero CRX/base code shipped (own list + modded manager; CRX content NOT copied).
- PRE-PUBLISH CHECKLIST: (1) Workbench → Resource Manager → **Rebuild Resource Database** to purge orphaned rdb entries for the deleted QRF zone/placeable files (deleted via filesystem; source has no refs; low risk but do it for a clean pack). (2) Publish. (3) In-GM verify: "25th DCO" world-settings tab; select group → "25th DCO QRF" Responder/Range + range circle; confirm circle renders for the GM (if not, pivot to map-circle — visual only).

## ⚠️ Dedicated-server test result (2026-05-28) — DCO GM layer NOT surfacing
First dedicated-server test. Mod loads fine and CRX works (see above), BUT **none of the "25th DCO" Game Master settings tabs or DCO systems were present or usable by the Game Master** on the server. All DCO phases below were previously verified only in single-player Workbench, never on dedi — so every "DONE / in-GM verification pending" item must be treated as UNVERIFIED on a dedicated server until re-tested there.
Likely causes to investigate (next session): addon actually enabled in the server mod list / load order; server-side vs client config replication; the `Edit.conf`/`DCO.conf` GM attribute-tab override not taking on dedi; modded `SCR_AIGroupUtilityComponent` systems not running server-side; placeables possibly needing an `RplComponent`.

### Investigation findings (2026-05-29) — two distinct problem layers
Load order ruled out by Bryce: 25thCRX IS loaded (else the CRX 1.7 fix wouldn't apply). So the DCO scripts compile and run on the server. That narrows it.

**Layer A — Deployment. RULED OUT (2026-05-29):** Bryce confirmed the dedicated server loaded a freshly-republished build that included all the latest DCO configs. So the missing GM layer is NOT a stale-build problem — it is a genuine config-wiring bug (Layer B).

**Layer B — Config wiring (real latent bugs, independent of deployment).** Even with a correct build, the GM-facing layer is likely misconfigured:
1. The `DCO_*EditorAttribute` classes ignore the `item` arg and read/write the global `DCO_MoraleSettings` singleton — i.e. they are *global/session* attributes. But they are registered in `Configs/Editor/AttributeLists/Edit.conf`, whose existing contents are all per-entity `CRX_AIGroup*`/`CRX_AICharacter*` attributes (the entity-selection edit list). Global attributes belong in the editor's GLOBAL attribute list, referenced by the GM editor mode's `SCR_AttributesManagerEditorComponent` — not the entity edit list.
2. `Configs/Editor/AttributeCategories/DCO.conf` has `m_bIsGlobalAttributeCategory 0` — inconsistent with a global settings tab; MODPLAN earlier described it as a "global category."
3. GUID mismatch: MODPLAN claimed the `Edit.conf` override sits at GUID `A4DEDD9410978E68`, but the actual `Edit.conf.meta` GUID is `F3D6C6D25642352C`. For a file-path/GUID override to actually replace the base list, our `.meta` GUID must equal the base-game `Edit.conf` GUID — must be confirmed against base game (needs live Workbench; offline game-data index/`game_read` could not resolve it).
4. Same class of risk for `DCO_QRFPlaceables.conf`: a standalone `SCR_PlaceableEntitiesRegistry` only appears in the GM content browser if something the editor loads references/aggregates it. Needs verification of how base game collects placeable registries.
5. `Prefabs/DCO_QRFZone.et` has no `RplComponent` (MeshObject + DCO_QRFZoneComponent + SCR_EditableEntityComponent only) — server logic runs fine, but MP client representation of a GM-placed instance may need it.

Verifying Layer B's correct target list/GUID and placeable-registry wiring requires the live Enfusion Workbench (Net API) or a built asset index; both were unavailable during this investigation.

### Deeper findings (2026-05-29, Workbench live in edit mode)
- Tooling blocker: base game lives in `P:\SteamLibrary\...\Arma Reforger\data*.pak`; the MCP `game_read`/`asset_search`/`game_browse` cannot descend into the paks here (return 0 files / not found), so base-game config CONTENT is unreadable. CRX dependency is likewise shipped as a single packed `data.pak` (addons/CRXEnfusionA.I._5F268647F8A1A1F4) — it ships NO loose `Edit.conf` and neither candidate GUID appears in it. So base/CRX attribute-list reference cannot be read via tools in this environment; only `api_search` (class signatures) works.
- LEADING hypothesis for "DCO attributes never appear": our `Edit.conf` override may be an ORPHAN — if its `.meta` GUID `F3D6C6D25642352C` is NOT the base-game `Edit.conf` GUID, the engine never treats it as the override the GM editor loads, so the DCO attribute entries are simply never registered (while base/CRX attributes keep working from the real list). The MODPLAN earlier recorded the intended override GUID as `A4DEDD9410978E68` — a mismatch that fits this theory. (Unconfirmed: whether Enfusion override resolution is GUID-based or path-based; if purely path-based, the override would take and the cause is instead the global-vs-entity list + non-global category.)
- DECISIVE NEXT TEST (bisects it): enter a local Game Master session in the live Workbench and observe — (a) does a "25th DCO" category appear in the attributes panel? (b) does "25th DCO QRF Zone" appear in the GM place menu? NEITHER appearing locally ⇒ registration is fundamentally broken (matches dedi) ⇒ fix config wiring. BOTH appearing locally ⇒ problem is MP/replication- or build-specific ⇒ re-examine. Requires eyes on the GM UI (MCP can't read the editor UI panels).

### ROOT CAUSE CONFIRMED + FIX IMPLEMENTED (2026-05-29, via reference files Bryce supplied then deleted)
Confirmed against the real base configs: the GM attribute list is `Configs/Editor/AttributeLists/Edit.conf` with GUID **`95F3570D0368C716`** (also holds the global Time/Date/Weather attributes — proof that global + per-entity attributes share one list; the global ones show via the world-settings panel). Our override carried GUID `F3D6C6D25642352C` ⇒ ORPHAN ⇒ DCO attributes never registered ⇒ no tab. CRX's own packed Edit.conf stayed active ⇒ CRX controls kept working. Mechanism (from base `SCR_AttributesManagerEditorComponentClass`): each editor-mode prefab's manager flattens its `m_AttributeLists` into one pool in the class constructor; an attribute is shown for a selection only if its `ReadVariable(item)` returns non-null.

FIX (IP-clean — ships zero CRX/base content; does NOT override Edit.conf):
- NEW `Configs/Editor/AttributeLists/DCO_Attributes.conf` (GUID `5DC0DC0A77E1B5E0`) — our own list, ONLY the 8 DCO_* attribute entries.
- NEW `Scripts/Game/DCO/Editor/DCO_RegisterAttributes.c` — `modded SCR_AttributesManagerEditorComponentClass`: its constructor redoes the base flatten of `m_AttributeLists`, then appends our DCO list via `BaseContainerTools.LoadContainer`/`CreateInstanceFromContainer` → `InsertAllAttributes`. Compiles clean (script recompile 10:21:48, warnings only, NO errors, NO "already an attribute" duplicates ⇒ modded ctor replaced the base body as intended).
- `Configs/Editor/AttributeCategories/DCO.conf` → `m_bIsGlobalAttributeCategory 1` (so the tab shows in GM world/session settings).
- DELETED our orphan `Edit.conf` + `.meta` (removes the dead override AND the copied CRX/base attribute entries — the IP problem).

REMAINING STEP / CURRENT BLOCKER: hand-created resources in this project are NOT auto-registered (console.log shows `DCO_QRFPlaceables.conf` and `DCO_QRFZone.et` as `resource not registered → null GUID`). `DCO_Attributes.conf` likewise needs registering or the modded loader silently no-ops. MCP `wb_resources register` (even with buildRuntime) kept timing out at 10s and the Workbench went unresponsive. Register these 3 resources (Resource Browser → right-click → Register, or focus the Workbench window to auto-scan): `Configs/Editor/AttributeLists/DCO_Attributes.conf`, `Configs/Editor/DCO_QRFPlaceables.conf`, `Prefabs/DCO_QRFZone.et`. Then reload + GM test (a)+(b) above.

### Registration resolved (2026-05-29) — use `rebuild`, not `register`
The MCP `wb_resources register` action ALWAYS times out (10s) and never writes the DB in this setup; `wb_resources rebuild <path>` WORKS instantly. Hand-created resources need a `.meta` (with the GUID) first, then a rebuild registers them with that GUID. The QRF `.conf`/`.et` had NO `.meta` at all (root of their null-GUID); created them:
- `Prefabs/DCO_QRFZone.et.meta` → GUID `496D16AE6F697063` (matches the placeables-config prefab ref); `.et` metas use `EntityTemplateResourceClass` (not `CONFResourceClass`).
- `Configs/Editor/DCO_QRFPlaceables.conf.meta` → GUID `5DC0DC0B9F1ACE01`.
Rebuilt all four (DCO_Attributes, DCO category, QRF placeables, QRF prefab). Log now shows the QRF prefab + placeables config loading by GUID, and the `DCO_Attributes.conf` "Wrong GUID {0000…}" loader error stopped recurring after the rebuild. Final confirmation = in-GM observation (Workbench backgrounded, not flushing logs live).
- OPEN: `Prefabs/DCO_QRFZone.et` runtime build fails — placeholder mesh `{5F4C4181F065B447}…/BarrelGreen_01.xob` is a wrong GUID on 1.7. Registration is fine; swap the placeholder mesh for a valid 1.7 asset (planned anyway).

### QRF REDESIGNED to per-group attribute (2026-05-29, Bryce's direction) + GM-only scoping
Bryce rejected the placed-mesh/zone placeable (and the fabricated BarrelGreen mesh GUID — do NOT invent GUIDs for existing engine assets; minting GUIDs for our OWN new .conf/.et resources is fine). New QRF model = **per-group Game Master attribute** (chosen over a placed area marker): the GM selects a group and toggles it as a QRF responder; that group then moves to support same-faction groups in distress (in contact, or broken from low morale) within its range. A radius "circle/UI" visual (like the base-game GM mortar/artillery effect) is still WANTED but DEFERRED — it needs the real base effect as reference (don't guess the rendering); functional binding implemented first.
- NEW `Scripts/Game/DCO/QRF/DCO_GroupQRF.c` — third `modded SCR_AIGroupUtilityComponent`: per-group state (`m_bDCO_IsQRFResponder`, `m_fDCO_QRFRange`), public accessors, `DCO_NeedsQRFSupport()`, and `DCO_UpdateQRF()` (hooked into the shared EvaluateActivity tick in DCO_GroupMorale.c). Reuses the reinforcement/zone enumeration patterns.
- NEW `Scripts/Game/DCO/QRF/DCO_QRFEditorAttributes.c` — two PER-GROUP, SERVER attributes (`DCO_QRFResponderEditorAttribute` toggle, `DCO_QRFRangeEditorAttribute` slider) that read/write the group state via `SCR_EditableGroupComponent.GetAIGroupComponent()` (server-only). ReadVariable returns null for non-group selections so they only show on groups.
- NEW `Configs/Editor/AttributeCategories/DCO_QRF.conf` (GUID `5DC0DC0C0F1ACE02`, `m_bIsGlobalAttributeCategory 0`) — per-entity category "25th DCO QRF"; the two entries added to `DCO_Attributes.conf` with `m_bIsServer 1`.
- DELETED the obsolete placed-zone files: `DCO_QRFZoneComponent.c`, `Prefabs/DCO_QRFZone.et`(+meta), `Configs/Editor/DCO_QRFPlaceables.conf`(+meta). (Stale rdb entries for these may remain until a full DB rebuild — harmless.)
- GM-ONLY SCOPING (also done): `DCO_RegisterAttributes.c` now only appends the DCO list when the flattened pool contains `SCR_DaytimeEditorAttribute` (a GM/"Edit"-only base attribute), so DCO settings no longer leak into the Admin/Photo editor modes.
- VERIFIED COMPILING CLEAN (2026-05-29, 11:09:42 recompile): module compiles with no `SCRIPT (E)` and no "Can't compile Game script module"; the modded attribute manager loaded `DCO_Attributes.conf` (all 10 attributes incl. the 2 QRF classes) with no `Unknown class` / `Wrong GUID`. 
  - GOTCHA fixed: Enforce resolves cross-fragment `modded class` method calls in FILE-PROCESSING (folder-alphabetical) order. The shared `EvaluateActivity` override must live in the alphabetically-LAST `SCR_AIGroupUtilityComponent` fragment (Comms `DCO_GroupReinforcement` → Morale `DCO_GroupMorale` → QRF `DCO_GroupQRF`). It was in Morale and called `DCO_UpdateQRF` (QRF, later) → "Undefined function" + full-module cascade (spurious base-game Tuple1/Tuple2/etc errors). Moved the override into `DCO_GroupQRF.c`; if a new group-utility fragment is ever added that sorts after "QRF", move the override there.
  - The 11:03 `Unknown class` errors were the earlier sequencing artifact (config rebuilt before scripts compiled) and are gone. Resource registration: `rebuild` works (`register` times out).

### QRF range circle visual (2026-05-29) — implemented as a Shape preview
The "circle/UI like the GM mortar effect" is done as a CLIENT-SIDE range circle drawn with the engine `Shape` visualizer (`Shape.CreateCylinder`) - NO asset/mesh GUIDs (researched: asset index is unavailable here and Bryce forbade fabricated asset GUIDs, so a procedural Shape is the correct no-asset route).
- NEW `Scripts/Game/DCO/QRF/DCO_QRFRangeVisual.c` — `DCO_QRFRangeVisual.Show(origin, radius)` / `Hide()`; holds a `ref Shape` (wireframe cylinder, radius = QRF range) and releases it to clear.
- `DCO_QRFRangeEditorAttribute` (DCO_QRFEditorAttributes.c) now overrides `PreviewVariable()` to draw the circle at the selected group's position (`SCR_EditableEntityComponent.GetOwner().GetOrigin()`) while the GM adjusts the range, and `StopEditing()` to clear it - the same "show a radius while configuring" UX as GM area effects.
- DESIGN NOTE: this is a preview-while-editing visual (no replicated custom state, no assets). A PERSISTENT always-on circle for every flagged QRF group would need either replicating the QRF flag/range to clients + a client draw loop, or a replicated visual entity with a ground-ring material (real asset GUID via the resource browser). Deferred.
- COMPILES CLEAN (2026-05-29, 11:54:07 recompile, no errors). Confirmed valid: `Shape.CreateCylinder`, `ShapeFlags.WIREFRAME|NOZBUFFER`, `PreviewVariable` override, `SCR_EditableEntityComponent.GetOwner()`, `manager.GetEditedItems()`, `GetVariable()`. GOTCHA: `SCR_BaseEditorAttribute.StopEditing()` is SEALED (cannot override); clear the preview via `PreviewVariable(setPreview=false)` instead (base StopEditing invokes it).
- ONE RUNTIME UNKNOWN left, needs an in-GM look: whether `Shape` visualizers render for a Game Master on a release client. If not, pivot to a map-circle (GM map UI) or an asset-based ground ring (real circle asset GUID via the resource browser). In-GM check: select a group → "25th DCO QRF" → drag QRF Range → a blue range cylinder should appear at the group.

### Code review (2026-05-29) — attribute tab READY; QRF needs the content-browser hookup
Reviewed all DCO GM-layer work. Clean: no tempreview/reference leftovers; GUIDs consistent (DCO_Attributes→DCO category, QRF placeables→QRF prefab all match their .meta); all 8 `DCO_*EditorAttribute` classes exist and match `DCO_Attributes.conf`; `DCO_RegisterAttributes.c` compiled at 10:37 with no script errors and the modded ctor demonstrably ran.
- READY to server-test: the "25th DCO" **attribute tab** (morale + reinforcement). IP-clean (own list + modded manager; zero CRX code).
- MINOR (attribute tab): the modded `SCR_AttributesManagerEditorComponentClass` ctor runs for EVERY editor mode (Edit/Admin/Photo each have this component + their own list), so the "25th DCO" category will also appear in the Admin/Photo attribute panels, not only Game Master. Harmless clutter; refine to GM-only later if wanted (hard to target cleanly from the class ctor).
- NOT YET WIRED: the **QRF placeable** will likely still NOT appear in the GM place menu. `SCR_ContentBrowserEditorComponent` ("Management of placeable entities") sources its placeables from entity catalogs + `SCR_PlaceableEntitiesRegistry` lists configured on the editor-mode prefab; a standalone `DCO_QRFPlaceables.conf` is not auto-discovered (same class of problem the attribute list had). To finish it cleanly, mirror the attribute fix: a `modded SCR_ContentBrowserEditorComponentClass` that appends our registry — needs the base component source (its registry-list member name) duplicated for reference, same as Bryce did for the attributes manager. Until then, QRF registration is correct but the browser hookup is missing.

### Also found: QRF placeable never registered
`DCO_QRFPlaceables.conf` (placeable registry) and `Prefabs/DCO_QRFZone.et` log as unregistered (null GUID) at world load ⇒ that's why the QRF zone never appeared in the GM place menu. Same registration gap as the attribute list. NOTE: still to verify how a `SCR_PlaceableEntitiesRegistry` is aggregated by the editor content browser (a standalone registry may also need referencing), but registration is the first required step.

## Live-session refinement pass (2026-05-30) — surrender hold, flee, anti-funnel, voicelines, last-stand
After an 8-member live session, Bryce flagged: (1) surrendering units often don't fully lay down / stay
down; (2) flee looks aimless (run around, not away); (3) groups maneuver to the nearest road and funnel
straight into the enemy; (4) wants a surrender-voiceline framework (author a sound in Workbench); (5)
scout more A3 AI features to port. Implemented — ✅ COMPILES CLEAN on 1.7 (Game module CRC32 `82295fb9`,
5862 files / 11276 classes, no `SCRIPT (E)`, "Game successfully initialized"). One real error was caught
in Bryce's startup log and fixed: two `vector.Length` calls missing parens in `DCO_TacticalMove.c`
(Enforce: it's a method, `.Length()`); the accompanying `Tuple2`/`SCR_SpinningWidgetAnimation`/
`SCR_ScenarioFrameworkParam` "(E)" lines were the known spurious full-module cascade and vanished once
that was fixed. Remaining DCO warnings are harmless (`CreateMessage` obsolete; "No need to use Cast for
up-casting" in `DCO_TacticalMove.c,67` / Reinforcement / QRF — style only). ⚠ The EnfusionMCP handler
addon is NOT loaded in this project (log: `Failed to call not existing Net API function 'EMCP_WB_Reload'`),
so MCP `wb_reload`/`wb_state` can't drive compiles — load it if you want me to compile/test directly;
otherwise pasting the Workbench log works. Implemented:

- **Fix 1 — Surrender hold (`DCO_GroupMorale.c`).** Root cause: after the one-shot crouch→prone beats,
  CRX's AI keeps driving the agent (stands it up / wanders); `DCO_MaintainSurrender` only re-lowered the
  weapon, never re-asserted the pose or stopped the brain. FIX: new `DCO_FreezeSurrenderedAgent` calls
  `SCR_CharacterControllerComponent.GetAIControlComponent().DeactivateAI()` (verified 1.7 API) a beat
  after the prone pose, FREEZING genuine surrenderers in pose. `DCO_MaintainSurrender` now re-asserts
  weapon-lower + PRONE every tick for still-active surrenderers, and SKIPS both frozen agents
  (`!IsAIActivated()`) and fake-surrender ambushers (re-posing would interrupt their grenade throw).
  Gated by `m_bFreezeSurrenderedAI` (default ON, internal knob) + `m_iSurrenderFreezeDelayMs` (1000).
- **Fix 2 — Flee coherence (`DCO_GroupMorale.c`).** Two causes: (a) `DCO_GetThreatPosition` returned
  false when perception was empty/stale at the moment of breaking → `fleePos` collapsed to the leader's
  own position (= mill around); (b) flee was sent once, so CRX combat behaviours kept competing. FIX:
  cache the last-known enemy centroid (`m_vDCO_LastThreatPos`, refreshed every tick under fire and on
  every `DCO_GetThreatPosition` success); `DCO_BreakAndFlee` falls back to it so a flee never degenerates;
  and while `m_bDCO_Broken` and below the flee threshold the flee is RE-ISSUED each tick so the group
  commits to one away-direction. New public `DCO_GetThreatOrLastPosition(out vector)` accessor (also used
  by Fix 3).
- **Fix 3 — Anti-funnel maneuver (`DCO_TacticalMove.c` / M2-lite).** `modded SCR_AIMoveActivity.InitParameters`
  now, when a known threat sits in the corridor between the group and a DISTANT destination, side-steps
  the destination laterally OFF the enemy axis (`DCO_SidestepFunnel`: 2D projection + perpendicular
  offset, `m_fFunnelSidestep`=45 m, `m_fFunnelCorridorHalfWidth`=30 m) so the group flanks instead of
  charging the funnel. Approximation of true cover-routing (full M2); CRX re-paths on contact. New GM
  toggle "Avoid Threat Funnel" (`m_bAvoidThreatFunnel`, default OFF, requires Enable Tactical Movement).
  Also flipped the leftover validation flags `m_bDebugLog`/`m_bForceIgnoreThreat` → FALSE for release.
- **Fix 4 — Surrender voiceline framework (`Scripts/Game/DCO/Audio/DCO_SurrenderVoice.c`).** `DCO_SurrenderVoice.Play()`
  fires an authored sound EVENT on the surrendering character via `SoundComponent.SoundEvent(eventName)`
  (verified API). Hooked in `DCO_Surrender` with per-member stagger (`m_iSurrenderVoiceStaggerMs`=350).
  GM toggle "Surrender Voicelines" (`m_bEnableSurrenderVoice`, default OFF) + event-name EditBox on
  `DCO_MoraleSettingsComponent` (`m_sSurrenderSoundEvent`). Authoring steps documented in the file header.
  ⚠ MP CAVEAT: `SoundEvent` plays on the calling machine; audible in Workbench/SP/listen host but on a
  DEDICATED server it has no audio sink — to have all clients hear it the call must be BROADCAST (RPC /
  client-side reaction to replicated surrender state). That broadcast layer is the documented next step.
- **Fix 5 — Last stand / no-surrender (R3) + roadmap.** New global GM toggle "Last Stand (No Surrender)"
  (`m_bEnableLastStand`, default OFF) gates the surrender branch (groups still break/flee). Implemented
  the easy high-synergy R3 now; R1 (panic), R2 (morale→accuracy), R4 (ambush hold-fire) re-prioritised in
  `DCO_ROADMAP.md` as the next batch.

NEW GM ATTRIBUTES (added to `Configs/Editor/AttributeLists/DCO_Attributes.conf`, all `m_bIsServer 1`):
`DCO_EnableLastStandEditorAttribute` `{5DC0DC1200000003}`, `DCO_EnableSurrenderVoiceEditorAttribute`
`{5DC0DC1200000001}` (DCO category), `DCO_AvoidThreatFunnelEditorAttribute` `{5DC0DC1200000005}` (DCO
category). Also removed the leftover diagnostic `Print` from `DCO_EnableMoraleEditorAttribute.ReadVariable`.

NEXT STEP (Bryce): load the EnfusionMCP handler addon in the open 25thCRX Workbench project, then
`wb_reload scripts` (or Script Editor recompile) to confirm a clean build + capture the CRC; then in-GM /
dedi test each toggle. Watch items: does `DeactivateAI()` hold the prone pose cleanly on a dedi client
(if the frozen agent T-poses or floats, re-activate + rely on the MaintainSurrender prone re-assert
instead); does the re-issued flee read as committed (tune flee distance if they still loop); does the
funnel side-step send AI sensibly off-axis without stranding them off-objective (tune sidestep/corridor).

## GM tab icon (2026-05-30) — how scenario-settings tab icons work
The "25th DCO" Scenario-Properties tab is iconless by default. Mechanism (verified via API + `Constants.c`
source): each tab = an `SCR_EditorAttributeCategory` config; its `m_UIInfo` (`SCR_UIInfo`) sets the icon
via two fields — **`Icon`** (ResourceName: a texture OR an imageset) and **`IconSetName`** (the quad/sub-image
name inside that imageset). `SCR_AttributesEditorUIComponent` renders each tab text+image and applies it
with `SetIconTo()` (image-only when many tabs). Base game's main icon atlas is
`{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset` (from `SCR_Constants.ICONS_IMAGE_SET`).
WIRED: `Configs/Editor/AttributeCategories/DCO.conf` now sets `Icon` to that imageset with `IconSetName ""`.
REMAINING (1 click, needs Workbench — the quad list is inside the packed imageset, not readable offline):
open `DCO.conf` in the config editor → `m_UIInfo` → pick `IconSetName` from the dropdown (real quads with
previews; look for an AI/group/soldier/combat/target glyph), save, reload. Until a quad is chosen the tab
stays iconless. Same optional treatment for `DCO_QRF.conf`. (No fabricated GUIDs — imageset GUID is from
source; quad chosen visually.)

## Roadmap port batch R1/R2/R4 (2026-05-30) — panic, morale→accuracy, ambush hold-fire
Implemented the next A3-feature batch from `DCO_ROADMAP.md` (R3 last-stand already done). All server-side,
default OFF, GM-toggled. ⚠ LIVE COMPILE PENDING (paste the next Workbench reload log to confirm; watch the
two new enum literals — see below).
- **R1 — Panic / shell-shock band (`DCO_GroupMorale.c`).** A morale band JUST above the flee threshold
  (`m_fFleeThreshold < morale <= m_fFleeThreshold + m_fPanicBand`): on a throttled chance the group briefly
  cowers — `SetWeaponNoFireTime(m_fPanicDurationSec)` + `ForceStance(CROUCH)` per member — then a further
  drop tips it into flee. State `m_bDCO_Panicking` + `m_fDCO_LastPanicTime` (cooldown `m_fPanicCooldownSec`),
  self-clears via `DCO_EndPanic`. Global GM toggle **"Enable Panic"** (`m_bEnablePanic`, default OFF).
- **R2 — Morale → accuracy (`DCO_GroupMorale.c`).** `DCO_ApplyMoraleAccuracy` maps group morale to an AI
  skill band and (only on change) pushes it to every member's `SCR_AICombatComponent`: `<= m_fAccuracyRookieMorale`
  → `SetAISkill(EAISkill.ROOKIE)` + `SetFireRateCoef(m_fLowMoraleFireRateCoef)`; `<= m_fAccuracyRegularMorale`
  → `EAISkill.REGULAR`; above → `ResetAISkill()` (default restored) + fire-rate 1.0. Verified levers on
  `SCR_AICombatComponent`: `SetAISkill(EAISkill)`, `ResetAISkill()`, `GetAISkillDefault()`, `SetFireRateCoef(coef,persistent)`,
  `SetPerceptionFactor()`. `EAISkill` members NONE/ROOKIE/REGULAR/VETERAN/EXPERT (ROOKIE+REGULAR used;
  names attested via workshop changelog, not the API dump — if either is wrong the compile names it).
  Global GM toggle **"Morale Affects Accuracy"** (`m_bEnableMoraleAccuracy`, default OFF).
- **R4 — Ambush / hold-fire, per-group (`Scripts/Game/DCO/Ambush/`).** New `modded SCR_AIGroupUtilityComponent`
  fragment `DCO_AmbushComponent.c` (folder "Ambush" sorts before "QRF", so `DCO_UpdateAmbush` is defined
  before the shared `EvaluateActivity` in `DCO_GroupQRF.c` that now calls it; the file is named
  "AmbushComponent" not "GroupAmbush" so it ALSO sorts before `DCO_AmbushEditorAttributes.c`, the external
  caller of its accessors — Enforce resolves modded-class method calls in file order, callers too). A flagged group keeps a re-applied
  `SetWeaponNoFireTime` hold until a perceived enemy is within `m_fDCO_AmbushRange` of the leader, then
  releases (`SetWeaponNoFireTime(0)`) and latches `m_bDCO_AmbushSprung`. Per-group GM attributes (mirror
  QRF, reuse `DCO_QRFAttributeHelper`): **"Ambusher"** toggle + **"Ambush Range"** slider, new per-entity
  category `DCO_Ambush.conf` (GUID `{5DC0DC0D0F1ACE03}`, +`.meta`).
- NEW GM ATTRIBUTES in `DCO_Attributes.conf` (all `m_bIsServer 1`): `DCO_EnablePanicEditorAttribute`
  `{5DC0DC1300000001}`, `DCO_EnableMoraleAccuracyEditorAttribute` `{5DC0DC1300000003}` (DCO category);
  `DCO_AmbushResponderEditorAttribute` `{5DC0DC0D00000001}`, `DCO_AmbushRangeEditorAttribute` `{5DC0DC0D00000003}`
  (Ambush category). Toggles also added to `DCO_MoraleSettingsComponent`.
- TUNING SLIDERS (added 2026-05-30, DCO category, `{5DC0DC13000000xx}`): "Panic Chance", "Panic Duration",
  "Accuracy: Rookie Below", "Accuracy: Regular Below" — so R1/R2 are tunable live in-GM during testing.
- REGISTER: `Configs/Editor/AttributeCategories/DCO_Ambush.conf` is new — the Workbench full-scan on
  focus/restart registers it via its `.meta` (or `wb_resources rebuild` once the EnfusionMCP handler is loaded).

## Viability audit + precedence + vehicle armour angling (2026-05-30)

### Precedence — DCO runs FIRST, before Skarris & CRX (point: "our behaviours first")
- **Load order (authoritative):** `addon.gproj` Dependencies = `5F268647F8A1A1F4` (CRX) → `68E33AC5D41A7D7B` (Skarris) → `58D0FB3206B6F859` (ArmaReforger). 25thCRX declares all three as deps ⇒ it LOADS LAST ⇒ its `modded class` bodies are the MOST-DERIVED ⇒ DCO executes first, then `super` chains down to Skarris → CRX → base. So DCO is the outermost layer on every modded class it touches.
- **Pre-emptive hooks decide DCO-FIRST (DCO logic runs before `super`):** `SCR_AIMoveActivity.InitParameters` (movement-type downgrade + anti-funnel sidestep — modifies the args, THEN calls super), and `SCR_AICombatMoveState.ApplyNewRequest` (drops the throttled stance request and only calls super if accepted). These genuinely override CRX/Skarris inputs first.
- **Augmentation tick (`SCR_AIGroupUtilityComponent.EvaluateActivity`):** calls `super` first (CRX/Skarris pick the action), then runs the DCO updates which inject AUTHORITATIVE state — `SetWeaponNoFireTime`/`SetWeaponRaised`/`ForceStance`, `AIControlComponent.DeactivateAI`, `FactionAffiliationComponent.SetAffiliatedFaction`, `SetAISkill`, and queued `SCR_AIMessage_Flee`/`SendMoveMessage`. These override the result regardless of intra-tick order (state writes win; messages are queued for the activity system), so DCO is authoritative here too.
- The two files under `CRX_EAI.../Modded/` are 1.7 compat shims (SetTransform, FactionManager), not behaviour — load-bearing, do not touch.

### API viability — methods verified (existence by clean compile and/or `api_search` on 1.7)
- Character: `SCR_CharacterControllerComponent` `SetWeaponRaised`/`SetWeaponNoFireTime`/`ForceStance(ECharacterStance)`/`GetAIControlComponent`; `AIControlComponent.DeactivateAI/ActivateAI/IsAIActivated`. ✓
- Group/perception/utility: `SCR_AIGroup` `GetAgents/GetLeaderEntity/GetLeaderAgent/GetFaction/GetGroupUtilityComponent`; `SCR_AIGroupPerception.m_aTargetEntities`/`AddOrUpdateGunshot`; `GetThreatMeasure`. ✓
- Messaging: `SCR_AIMessageHandling.SendMoveMessage/SendCancelMessage/SendGetInMessage`; `SCR_AIMessage_Flee.Set/GetPosition`; `AICommunicationComponent`. ✓
- Combat tuning: `SCR_AICombatComponent.SetAISkill(EAISkill)/ResetAISkill/SetFireRateCoef/SetPerceptionFactor`. ✓ (EAISkill.ROOKIE/REGULAR names attested, confirm on compile.)
- Inventory/weapon/faction/sound/vehicle: `SCR_CharacterInventoryStorageComponent`/`InventoryStorageManagerComponent` drop paths; `BaseWeaponManagerComponent.GetCurrentGrenade/SelectWeapon/Throw`; `FactionAffiliationComponent.SetAffiliatedFaction`; `SoundComponent.SoundEvent`; `CompartmentAccessComponent.GetVehicleIn`. ✓
- **SEMANTIC runtime-unknowns (existence is proven, BEHAVIOUR needs in-engine test):** DeactivateAI holds the prone pose on a dedi client; `AddOrUpdateGunshot`/`SendMoveMessage` actually make receivers react/converge; `SetAISkill` perceptibly changes aim; surrender `SoundEvent` reaches MP clients (needs broadcast); anti-funnel sidestep doesn't strand AI off-objective; **vehicle angling** (below) reorients without over-charging or oscillating vs CRX. These are the test-pass items, not code defects.

### NEW FEATURE — Vehicle armour angling (`Scripts/Game/DCO/Armor/DCO_VehicleArmor.c`)
When a crewed AI vehicle is in contact and presenting a weak side/rear to the nearest enemy, turn its FRONT toward that enemy for survivability (like a player angling). New `modded SCR_AIGroupUtilityComponent` fragment (a crew IS a group); folder "Armor" sorts before "QRF" so `DCO_UpdateVehicleArmor` is defined before the shared `EvaluateActivity` that calls it; self-contained.
- DETECTION: `CompartmentAccessComponent.GetVehicleIn(leader)` (is the crew in a vehicle) + group perception for the nearest enemy. DECISION: compare the hull forward axis (`vehicle.GetTransformAxis(2)`) to the enemy bearing; if the enemy is OUTSIDE the frontal arc (`m_fVehicleFrontalArcDeg`, default 50°) and beyond `m_fVehicleMinEngageRange` (30 m), reorient. ACTION: `SendMoveMessage(leaderAgent, enemy)` to drive the front toward the threat, then `SendCancelMessage` once the front is within the arc.
- ⚠ VIABILITY LIMIT (researched): Reforger exposes NO scriptable in-place AI hull-rotate (only the GUNNER has `PushRequestRotateToTarget`; `BaseVehicleControllerComponent` has no AI heading setter; the only order primitive is move-to-ENTITY). So this v1 angles by MOVING toward the enemy and cancelling when oriented — it also noses/advances the vehicle a little toward the threat. Precise in-place "front-side" sloping needs an engine driver-combat-move seam not in the public API. GATED OFF (`m_bEnableVehicleArmor`), GM toggle "Vehicle Armour Angling" + "Vehicle Frontal Arc" slider (`{5DC0DC14000000xx}`), also on `DCO_MoraleSettingsComponent`. TEST: enable, put an AI tank in contact from its flank — expect it to swing its front toward the enemy (and creep toward it); tune arc/min-range; watch for oscillation vs CRX vehicle behaviour.

## Goal pass — soundness review + outstanding-feature implementation (2026-05-30)

### Soundness review of implemented features (code-sound = APIs exist + used correctly)
Reviewed every DCO file against the verified 1.7 API. Findings:
- Fragment wiring is correct: the 5 `modded SCR_AIGroupUtilityComponent` fragments process in folder order
  Ambush → Armor → Comms → Illum → Morale → QRF; the shared `EvaluateActivity` (caller of all `DCO_Update*`)
  lives in QRF (alphabetically last) so every update method is defined first. The one within-folder caller
  risk (Ambush attributes) was fixed by naming the fragment `DCO_AmbushComponent.c`.
- Precedence: 25thCRX loads after CRX+Skarris ⇒ DCO is the outermost modded layer (runs first); pre-emptive
  hooks decide before `super`; the EvaluateActivity tick injects authoritative state after `super`.
- All engine methods used are real (existence proven by the Phase 1/2 clean compiles + `api_search`). Two
  proactive soundness fixes this pass: removed `Math.DEG2RAD` (used a `0.0174533` literal) in the vehicle
  code; the only remaining compile-risk literals are `EAISkill.ROOKIE/REGULAR` (attested, not in the API
  dump — the compiler will name them if wrong). Remaining risks are SEMANTIC (runtime behaviour), captured
  per-feature as test items — not code defects.
- ⚠ The whole 2026-05-30 batch (surrender-hold, flee, anti-funnel, voicelines, last-stand, R1 panic, R2
  accuracy, R4 ambush, vehicle angling, night illum, smoke-on-flee, vehicle hijack, HVT targeting, sliders)
  is CODE-COMPLETE but NOT yet compile-confirmed as one set. "Code sound" is only fully established after a
  clean reload — paste the next Workbench log. COMPILE-RISK SPOTS TO WATCH: `EAISkill.ROOKIE/REGULAR`;
  `new SCR_AIActivitySmokeCoverFeature()` + null properties; passing instance method `DCO_HijackCollect` as
  the `QueryEntitiesBySphere` callback; `EAICompartmentType.Pilot`; `SCR_BaseCompartmentManagerComponent.CREW_COMPARTMENT_TYPES`;
  `new SCR_AITargetInfo()` + `msg.m_TargetInfo =`.

### Phase 3.6 — Night illumination ✅ IMPLEMENTED (`Scripts/Game/DCO/Illum/DCO_NightIllum.c`)
A group in contact in low light pops an illum flare over the nearest enemy. Fully native: broadcast
`SCR_AIMessage_FireIllumFlareAt.Create(enemyPos)` via the group mailbox; gated by the engine's own
`SCR_AIGroupInfoComponent.IsIllumFlareAllowed()` (reached via `GetOwner().FindComponent(...)`) which already
checks ambient light + a per-group cooldown, so it only fires at night and never spams. GM toggle "Night
Illumination" (`{5DC0DC15…}`, default OFF) + component field. New `SCR_AIGroupUtilityComponent` fragment
(folder "Illum" < "QRF"). VERIFIED APIs: `AICommunicationComponent.CreateMessage/RequestBroadcast`,
`SCR_AIMessage_FireIllumFlareAt.Create`, `SCR_AIGroupInfoComponent.IsIllumFlareAllowed`.

### Outstanding features — status after API research (what's left, and the SPECIFIC blocker)
These cannot be soundly implemented OFFLINE without guessing GUIDs/enums (forbidden) — each needs a live
Workbench session or further in-engine validation. API basis researched + recorded so they're ready to build:
- **Phase 3 smoke-on-flee** — ✅ IMPLEMENTED & FIXED (2026-05-31). `new SCR_AIActivitySmokeCoverFeature().Execute(this, screenPos, SCR_AIActivitySmokeCoverFeatureProperties.NONE, {}, {}, 1, null)` — the earlier compile break was passing `null` for the ENUM properties arg (now `.NONE`). `DCO_DeploySmoke` drops a screen ~12 m toward the threat (between group and enemy), once per break (`wasBroken` guard). The feature itself only picks agents with `EUnitRole.HAS_SMOKE_GRENADE` within ~40 m, so it no-ops cleanly if the group has no smoke. GM toggle "Smoke On Flee" (`{5DC0DC16…}`, default OFF).
- **Phase 3.5 indirect fire** — `SCR_AIMessage_ArtillerySupport` exists but has NO `Create()` and needs a real `m_ArtilleryEntity` + `SCR_EAIArtilleryAmmoType`; the supported path is the `SCR_AIWaypointArtillerySupport` waypoint. BLOCKER: needs a friendly artillery asset + waypoint prefab GUID + ammo enum (in-engine).
- **Phase 3 HVT/marksman** — ✅ IMPLEMENTED (`Scripts/Game/DCO/HVT/DCO_HVTTargeting.c`). A group that perceives an enemy VEHICLE (`FindComponent(BaseVehicleControllerComponent)`) builds a `SCR_AITargetInfo` (`Init(entity, perceivable, pos, ts)` - later args default) and broadcasts a `SCR_AIMessage_Attack` to focus fire on it. GM toggle "HVT Priority Targeting" (`{5DC0DC18…}`, default OFF). RUNTIME-UNKNOWN: whether a script-built target info re-prioritises fire vs the engine's own selection.
- **Phase 4 vehicle hijack** — ✅ IMPLEMENTED (`Scripts/Game/DCO/Hijack/DCO_VehicleHijack.c`). On-foot group in contact finds the nearest EMPTY drivable vehicle via `BaseWorld.QueryEntitiesBySphere` + callback `bool DCO_HijackCollect(IEntity)` (`SCR_BaseCompartmentManagerComponent.GetOccupantCount()==0` + `HasFreeCompartmentOfTypes(CREW_COMPARTMENT_TYPES)`), then `SendGetInMessage(leaderAgent, vehicle, EAICompartmentType.Pilot, null, comms)`. GM toggle "Vehicle Hijacking" (`{5DC0DC17…}`, default OFF). RUNTIME-UNKNOWN: boarding takes for an on-foot AI group; filter excludes static turrets.
- **Phase 4 garrison** — partial: **Defensive Hold IMPLEMENTED** (`Scripts/Game/DCO/Defend/DCO_DefendComponent.c`) — a per-group GM flag ("25th DCO Defend") that makes a group hold position and orient defence toward the nearest threat via `SCR_AIMessage_Defend.Create(dir, arc, false, prio, null, null)`. That's the achievable engine-grounded defensive piece. FULL building-interior garrison (window firing positions + room clearing) still outstanding — needs nav/building-position queries (in-engine design). RUNTIME-UNKNOWN: the Defend arc-units (assumed radians) + priority value (tunable floats; call is sound).
- **Phase 6 M2–M4 tactical** — ✅ DONE (2026-06-02). Realized by the procedural cover-pathing + bounding system (`DCO/Movement/DCO_TacticalPath.c`) + cover placement (`DCO/Formation/DCO_FormationComponent.c`) — re-decides the covered route as the group moves instead of inserting fixed waypoints.
- **R5 vehicle-borne reinforcement** — ✅ DONE (2026-06-02). The `useVehicles=true` move IS the supported board-then-converge (engine boards the nearest suitable vehicle + drives); there is no separate get-in order to conflict. Guarded to fire only for on-foot, far responders (`DCO_GroupReinforcement.DCO_OrderConverge`).
- **R6 bounding overwatch** — ✅ DONE (2026-06-02). `DCO_TacticalPath` Phase 2 (base-of-fire + leapfrogging maneuver element) + Phase A traveling-overwatch. The old `DCO_FlankSplit.c` experiment was DELETED as superseded.
- **Echelon / Vee / dispersion formations** — ✅ DONE (2026-06-02). `DCO_FormationComponent.DCO_UpdateFormationShape` imposes per-member offsets (column/wedge/line/echelon-L/echelon-R/vee) scaled by spacing, oriented to heading/threat — the native `SetFormation` set couldn't do echelon/vee/dispersion. EXPERIMENTAL (fights CRX formation; default OFF).
- **Safe-eject gate** — ✅ DONE (2026-06-02). `modded SCR_CompartmentAccessComponent.AskOwnerToGetOutFromVehicle` blocks a voluntary AI dismount when the vehicle is too fast (`Physics.GetVelocity`) or too high (`BaseWorld.GetSurfaceY`); forced ejects + players never blocked. Default OFF.

DROPPED (Bryce 2026-06-02 — removed from scope, not gaps):
- **Phase 5 POW / capture** — DROPPED. No animations available and out of current scope/knowledge.
- **Phase 3.5 indirect fire / artillery** — DROPPED. Base game already provides orderable artillery.
- **R7 casualty drag** — DROPPED. Needs an animation dependency not in scope.
- **Dynamic-simulation / distant-AI freezing** — SKIPPED. Engine provides native AI LOD/limiting.

EVERYTHING IMPLEMENTABLE ON VERIFIED API IS NOW IMPLEMENTED. The only remaining work is **in-engine validation/tuning** (no test world in the build environment) — the recurring open checks are per-member `RequestBroadcast` isolation, `SetDisarmed` disengage/re-engage, spawned-grenade throwability, and behavioural tuning.

## ✅ PRODUCTION-SAFETY REVIEW (2026-05-31) — PASS, with noted items
Full review of all ~23 DCO features for live-server safety:
- **Server authority — PASS.** Every group-tick update (11 `DCO_Update*`), the flinch `OnDamage` override, and
  the tactical-move `InitParameters` gate on `Replication.IsServer()`. Hardened the one gap this pass:
  `DCO_StanceCooldown.ApplyNewRequest` now also guards `Replication.IsServer()`. All GM attributes are
  `m_bIsServer 1` (writes apply server-side). No client-side gameplay writes.
- **Default-OFF — PASS (2 intended ON).** Every feature defaults OFF or is per-group-flagged, EXCEPT the morale
  system (surrender/flee + the surrender-hold & flee-direction FIXES) and reinforcement/shared-SA, which are ON by
  design. ⚠ CONFIRM those two are wanted on the live server (they change AI behaviour); the F2 shared-morale-pool
  is the planned fix for the early-surrender complaint.
- **Null-safety — PASS.** Updates guard `m_Owner`/`m_Perception`/`world`/leader; the flinch override calls
  `super.OnDamage` FIRST, so a DCO bug can never break base damage handling; all surrender `CallLater` beats null-check the character.
- **Performance — PASS.** Every heavy update (QRF, reinforcement, hijack `QueryEntitiesBySphere`, defend, formation,
  night-illum) is throttled per-group (1.5–10 s). M4 raycasts fire only on a move order (not per-tick) and only when
  exposure is enabled (OFF). Per-eval cost is ~11 cheap gate checks.
- **Fragment wiring / singleton — PASS.** Fragments resolve in folder order before the QRF `EvaluateActivity` caller;
  settings singletons are server-authoritative (no client desync).
- RUNTIME-VERIFY (not code defects): surrender `DeactivateAI`/faction-clear pose replicates to dedi clients; M4
  `m_iExposureLayerMask` default 0 = safe no-op until tuned. COMPILE: the 2026-05-31 batch (smoke fix, M4, formation,
  defend, flinch, hide-from-armour, R5) needs one reload to confirm — all patterns are verified/source-derived.

## Phase 9 — Battlefield cohesion & friendly-fire (✅ IMPLEMENTED 2026-05-31, Bryce's direction)
Three new systems, all built + wired (GM toggles "Shared Morale Pool" `{5DC0DC1E…}`, "Fratricide Avoidance"
`{5DC0DC1F…}`, "Straggler Merge" `{5DC0DC20…}`, all default OFF; component fields added; F1 fragment
`DCO_FriendlyFire.c`, F3 fragment `DCO_StragglerMerge.c`, F2 in `DCO_GroupMorale.DCO_GetPooledLossFrac`).
All grounded in verified/resolved 1.7 API. Design (as built) below.

### F1 — Fratricide avoidance (friendly-fire LOS hold)
- PROBLEM: movement-heavy AI shoot each other when a friendly crosses their firing lane.
- APPROACH: a per-group tick (`DCO_UpdateFriendlyFire`, new fragment). For each member that has a perceived
  enemy in front of it, raycast the firing lane shooter→target with `BaseWorld.QueryEntitiesByLine(from, to, addCb)`
  (callback collects entities on the line); if any collected entity is a FRIENDLY character (same faction via
  `FactionAffiliationComponent` / `m_Owner.GetFaction()`), apply a brief `SetWeaponNoFireTime` to that member so
  it holds until the lane clears. Throttled (~0.5 s). Reuses the M4 raycast + the held-fire lever; all verified.
- TUNABLES: enable toggle, lane half-width (use a short beveled/sphere line or a small offset check), hold time.
- RISK: cost (one line-query per engaging member) → throttle + only when the member actually has a target.

### F2 — Shared morale pool (fixes early surrender of small groups)
- PROBLEM: morale is per-GROUP; a 3-man team that loses a man early surrenders far too soon.
- APPROACH: in `DCO_UpdateMorale`, when pooling is on, compute COMBINED max-strength + current-strength across
  same-faction groups within `m_fMoralePoolRadius` (enumerate via `SCR_AIWorld.GetAIAgents`, same pattern as QRF/
  reinforcement), and gate surrender/flee on the POOL's loss fraction instead of the lone group's. So a small
  group fighting alongside a strong friendly force inherits the force's resilience and won't fold on one casualty.
  Falls back to single-group maths when pooling off. All verified API.
- TUNABLES: enable toggle, pool radius, optional min-pool-size.

### F3 — Straggler / one-man-team merge
- PROBLEM: lone survivors / 1-man teams are ineffective; should fold into nearby friendlies as the fight goes on.
- APPROACH: a per-group tick (`DCO_UpdateMerge`). When a group's `GetPlayerAndAgentCount()` <= `m_iStragglerSize`
  AND it's in contact (perceived enemy), find the nearest LARGER same-faction group within `m_fMergeRadius`
  (`SCR_AIWorld.GetAIAgents`), and transfer the survivors into it with `targetGroup.AddAIEntityToGroup(memberEntity)`
  (resolved from source; pairs with `RemoveAIEntityFromGroup`). One-shot per straggler; throttled. Verified.
- TUNABLES: enable toggle, straggler size threshold, merge radius. RISK: don't merge players' groups / locked groups
  — guard with player-count check; only merge pure-AI stragglers.

IMPLEMENTATION ORDER (after the production review): F2 (highest impact, lowest risk — pure maths on the morale
path) → F1 (fratricide, reuses M4 raycast) → F3 (group surgery, most care needed re: replication + player groups).

### Surrender gear-drop — ✅ ROOT-CAUSED + FIXED (2026-05-31, from Bryce's EquipedLoadoutStorageComponent source)
ROOT CAUSE: worn clothing/armour lives in a **LOADOUT slot** (`LoadoutSlotInfo`, owned by the
`EquipedLoadoutStorageComponent` hierarchy), NOT in a regular storage slot. `DCO_DropArea` did
`charInv.FindItemSlot(item)` — `FindItemSlot` searches a storage's OWN regular slots, so for an equipped cloth
it returned **null** and the method bailed at `if (!slot) return;` BEFORE dropping anything. That's why the
rifle dropped (`DropCurrentItem`, a different path) but gear never did.
FIX: get the item's own parent slot from its `InventoryItemComponent.GetParentSlot()` → `GetStorage()` →
`invMgr.TryRemoveItemFromStorage(item, storage)`. This is EXACTLY what vanilla
`SCR_InventoryStorageManagerComponent.TryRemoveItemFromInventory(item)` does internally (GetParentSlot →
GetStorage → TryRemoveItemFromStorage) — the engine's real drop-to-ground for an equipped item. All verified
1.7 API (`InventoryItemComponent.GetParentSlot`, `InventoryStorageSlot.GetStorage`, `TryRemoveItemFromStorage`).
- ⚠ FALSE START earlier (reverted, do NOT reintroduce): `invMgr.CanDropItem`/`TryDropItem`/`DropItem` — none
  exist in 1.7. Also note `SCR_InventoryStorageManagerComponent.MoveItemToVicinity` is an EMPTY STUB (no body) —
  not a usable drop call.
- ⚠ COMPILE PENDING + RUNTIME-VERIFY: confirm gear visibly hits the ground on a dedicated server.

## Phase 16 — Heli-crew / mounted-group exclusion hardened (2026-05-31, Bryce) — COMPILED CRC 9f1c47a2
BUG: helicopter pilots/crews (and transport passengers) were disembarking mid-flight because Tactical Movement
reshaped their move order. ROOT CAUSE: `DCO_VehicleUtil.IsGroupInVehicle` checked ONLY the group leader via
`CompartmentAccessComponent.GetVehicleIn` - it missed cases where the mounted member given the move order is not
the leader, or the leader is briefly unmounted in a transition, so heli groups slipped past the guard.
FIX: `IsGroupInVehicle` now returns true if the leader OR ANY member is mounted (leader checked first as the
cheap fast path; member scan via `SCR_AIGroup.GetAgents` -> `AIAgent.GetControlledEntity` -> `GetVehicleIn` only
if leader isn't mounted). This robustly excludes mounted groups from ALL three gated systems (tactical move,
straggler merge, adaptive formation) since they all call this one helper. Compiled clean via wb_reload, CRC32
9f1c47a2, 0 errors, "Game successfully initialized". (Earlier reads showed a stale identical CRC; a forced
re-reload confirmed the new bytecode.)

## Phase 15 — Unified debug logging (2026-05-31, Bryce)
NEW central logger `Scripts/Game/DCO/Debug/DCO_Debug.c` (plain static, order-independent): `DCO_Debug.Log(category, msg)`,
`DCO_Debug.LogGroup(category, leaderEntity, msg)`, `DCO_Debug.Enabled()`. One master switch
`DCO_MoraleSettings.m_bDebug` (GM toggle "DCO Debug Logging" `{5DC0DC2400000001}` + JSON key `m_bDebug`, default
OFF; near-zero cost when off - one bool read). Output to console.log tagged per category: `[DCO:SURRENDER] (grp <id>) ...`.
Instrumented decision points (more can be added trivially):
- SURRENDER: per-eligible-tick line showing morale vs threshold, squadDepleted (str vs keep-N), lullOk (secs since
  combat), lossFrac vs min, and the resulting surrender bool - so you can SEE which gate blocked/passed it.
- SURRENDER: a line when a group actually surrenders (staggered disarm).
- FLEE: break&flee destination + hasThreat + morale.
- COMBAT: when a hit resets the surrender lull (`DCO_NoteCombatActivity`).
- RECOVERY: when a surrendered group recovers/re-arms.
MCP `mod_validate` = PASS (all 6). Replaces ad-hoc flags conceptually (the old `m_bDebugLog`/`m_bStanceCooldownDebug`/
`FLANK_SPLIT_DEBUG` still exist for their niches but new logging should use DCO_Debug). ⚠ COMPILE PENDING (reload).
GOTCHA re-confirmed: Enforce has NO ternary `?:` - a `?:` in the first debug line broke it; precompute into a var.

## Phase 14 — Surrender context gates: anti-mid-fight + anti-synchronized-drop (2026-05-31, Bryce + Opus design)
Reworked the surrender DECISION (not the morale value) so low morale is NECESSARY BUT NOT SUFFICIENT. Morale
stays the continuous, gradual 0-100 group value; surrender now also requires being OUT of combat and depleted,
and members surrender staggered. MCP `mod_validate` = PASS (all 6 checks). Per Bryce: combat activity is keyed on
PERCEIVED ENEMY (+ being shot), not an unverified weapon-fired event.
- **Combat-lull gate (the key fix for mid-firefight surrender):** new `m_fDCO_LastCombatActivityMs`, refreshed in
  `DCO_UpdateMorale` whenever the group perceives an enemy (`m_Perception.m_aTargetEntities`) or `threat >= heavy`,
  AND from the damage hook via new public `DCO_NoteCombatActivity()` (so being shot counts too). Surrender is only
  allowed once `(now - lastCombat) >= m_fSurrenderLullSec`. Verified victim->group path in `DCO_HitFlinch.OnDamage`:
  `AIControlComponent.GetGroup()` -> `SCR_AIGroup.GetGroupUtilityComponent()` -> `DCO_NoteCombatActivity()`. The combat-note
  now runs independent of the hit-flinch toggle (refactored OnDamage: note first, then the flinch is its own opt-in).
- **Squad-strength gate:** group keeps fighting while `strength >= m_iSurrenderMinSquadToFight` (0 = ignore).
- **Per-member disarm stagger (anti-synchronized-drop):** `DCO_Surrender` now schedules each member's
  `DCO_BeginAgentSurrender` via `CallLater` at a random delay up to `m_fSurrenderStaggerMaxSec` (0 = simultaneous).
  Previously only the voiceline staggered; the disarm was instant for the whole squad.
- **Per-casualty morale hit:** `m_fMoraleLostPerCasualty` subtracted the instant living-count drops (tracked via
  new `m_iDCO_LastStrength`), so losses are felt immediately on top of the gradual drain. 0 = disable.
- **0-disables convention honoured:** `m_fSurrenderThreshold 0` => never surrender (explicit guard added);
  lull/squad/stagger/per-casualty all treat 0 as "off".
- NEW GM attrs (DCO category, `m_bIsServer 1`, plain-language designer descriptions): "Surrender: Combat Lull (s)"
  `{5DC0DC2300000001}`, "Surrender: Keep Fighting Above N" `{…04}`, "Surrender: Stagger (s)" `{…07}`,
  "Morale Lost Per Casualty" `{…0A}`. All four also in the server JSON (read+write) + example template.
- DEFAULTS (ON, tuned reluctant): lull 12 s, keep-fighting-above 3, stagger 1.5 s, morale-lost-per-casualty 12.
- ⚠ COMPILE PENDING (reload to confirm). The cross-fragment call (`DCO_NoteCombatActivity` defined in Morale/,
  called from Reaction/) resolves because Morale < Reaction in folder order.

## MCP production pass + R6 fireteam-split DRAFT (2026-05-31)
- **MCP `mod_validate` = PASS** (all checks: structure, gproj, scripts, prefabs, configs, references, naming).
  Only warning: `Configs/DCO_Settings.example.json` "unconventional location" — false positive (it's a JSON
  template intentionally in Configs/, not a `.conf`). No action.
- **R6 two-element flank — DRAFT, NOT WIRED:** `Scripts/Game/DCO/Bounding/DCO_FlankSplit.c`. Adds
  `DCO_UpdateFlankSplit()` to the modded `SCR_AIGroupUtilityComponent` but NOTHING calls it (verified: zero
  callers) so it's inert — a review artifact. Splits fireteams into base-of-fire (leader's FT) + maneuver FT,
  sends the maneuver members a per-agent targeted `SCR_AIMessage_Move` via `RequestBroadcast(msg, agent)` to a
  flank position. Default OFF (`ENABLE_FLANK_SPLIT_EXPERIMENT=false`) + heavy debug logging. PURPOSE: the
  in-engine experiment to answer whether per-agent move targeting relocates ONLY that element or fights group
  cohesion (the R6 blocker). To activate: flip the const + add `DCO_UpdateFlankSplit();` to EvaluateActivity in
  DCO_GroupQRF.c (folder "Bounding" < "QRF" so order is satisfied). API all verified: `m_FireteamMgr` (public
  field), `GetFireteams`/`GetMembers`, `RequestBroadcast(msg,receiver)`, `SCR_AIMessage_Move.Create`, `SetReceiver`.

## Reload log result #2 (2026-05-31) — CLEAN, CRC32 `0575f391` (Skarris-dropped + flanking)
Second reload after dropping Skarris + adding DCO-native flanking + the JSON-context switch: **Module: Game,
5876 files / 11318 classes, CRC32 `0575f391`, ZERO `SCRIPT (E)`, "Game successfully initialized."** Class count
+2 vs the prior clean build (`38b5573e`) = the two new flanking editor-attribute classes. The `SCR_JsonLoadContext`/
`SCR_JsonSaveContext` obsolete (W) are GONE (base JsonLoadContext/JsonSaveContext switch worked). Only harmless
warnings remain (our 2 `CreateMessage`; base/CRX obsolete notices). Skarris removal + flanking are compile-confirmed.

## Phase 13 — Skarris dropped + DCO-native flanking (2026-05-31, Bryce)
**Skarris removed as a dependency.** `addon.gproj` Dependencies now = CRX `5F268647F8A1A1F4` + ArmaReforger
`58D0FB3206B6F859` only (Skarris `68E33AC5D41A7D7B` gone). Verified: zero Skarris GUID refs anywhere; DCO has
NO code coupling to Skarris (its `modded` classes extend engine/CRX, super-chain is now DCO→CRX→base). Flanking
was always CRX's behaviour, not Skarris's (Skarris = the AI-settings tuning, already absorbed into our own
`Prefabs/AI/SCR_AIWorld.et`). NOTE: keep that prefab — it's the matrix-dodge fix, independent of Skarris.
- **KEY API FINDING (from the SCR_AIGroupUtilityComponent source Bryce supplied):** the fireteam manager IS
  reachable — public field `SCR_AIGroupUtilityComponent.m_FireteamMgr` (no getter, direct field). `SCR_AIGroupFireteam`
  has `GetMembers/GetMemberCount/GetFirstMemberEntity`. This unblocks a true base-of-fire + maneuver-element split
  LATER (R6); for now flanking is whole-group (reliable, no cohesion risk).

### DCO-native flanking maneuver (`DCO_TacticalMove.DCO_FlankApproach`)
When a group moves to engage a KNOWN enemy, the approach is rotated around the enemy to come in from a FLANK
instead of straight down its front. Built on the move seam we already own (`SCR_AIMoveActivity.InitParameters`),
runs after anti-funnel, before exposure scoring. Pure 2D geometry: rotate the enemy→group bearing by
`m_fFlankAngleDeg` both ways (`Math.Sin`/`Math.Cos`, verified), keep the same standoff range (circle, not charge),
pick the flank that is LESS LOS-exposed to the threat (reuses M4 `DCO_IsCovered`), else the shorter swing. Only
engages when the destination is within `m_fFlankMaxEnemyDist` (250 m) of the known enemy and standoff >
`m_fFlankMinEnemyDist` (25 m). Crews excluded (the existing vehicle-crew guard at the top of InitParameters).
CRX still does in-contact cover-to-cover on arrival.
- Settings on `DCO_TacticalMoveSettings`: `m_bEnableFlanking` (default OFF), `m_fFlankAngleDeg` (55),
  `m_fFlankMaxEnemyDist` (250), `m_fFlankMinEnemyDist` (25).
- GM attrs (DCO category): "Enable Flanking" `{5DC0DC2200000001}`, "Flank Angle" slider `{5DC0DC2200000003}`.
- JSON: the whole `DCO_TacticalMoveSettings` singleton is NOW covered by `DCO_JsonConfig` (it was previously
  only `DCO_MoraleSettings` — this closes that gap: tactical-move + funnel + flanking + exposure all in the server JSON).
- ⚠ COMPILE PENDING (reload to confirm). RUNTIME-TUNE: flank angle / distances; whether the circle reads as a
  natural flank vs over-swing. Two-element bounding flank (R6) is the future heavier step now that m_FireteamMgr is known reachable.

## Reload log result (2026-05-31) — CLEAN COMPILE, CRC32 `38b5573e`
Bryce's Workbench reload: **Module: Game, 5876 files / 11316 classes, CRC32 `38b5573e`, ZERO `SCRIPT (E)`.**
Every multi-session change (Phase 9 F1/F2/F3 + hardening, Phase 10 recovery + JSON, Phase 11 vehicle-crew
exclusion + armour hysteresis + stance in-cover, Phase 12 AI-settings, gear-drop fix) compiles. "Game
successfully initialized." Remaining items from the log + their disposition:
- ✅ FIXED: `SCR_JsonLoadContext`/`SCR_JsonSaveContext` "obsolete" (W) → switched to base `JsonLoadContext`/
  `JsonSaveContext` in `DCO_JsonConfig.c` (same proto API: LoadFromFile/ReadValue, SaveToFile/WriteValue; verified).
- HARMLESS (W): two `CreateMessage` obsolete (DCO_HVTTargeting.c:89, DCO_GroupMorale.c:207) — still functional
  in 1.7; the only replacement path is engine-internal. Left as-is (documented since Phase 1).
- NOT OURS: `WORLD (E): Unknown keyword 'm_bScenarioPropertiesConfigFiles' at offset 1398` during load of the
  prefab GUID `{E0A05C76552E7F58}` (the base `Prefabs/AI/SCR_AIWorld.et` our override inherits from). `grep`
  CONFIRMS our on-disk file (1412 bytes) does NOT contain that keyword — it's in the inheritance source (the
  packed base/Skarris prefab authored against a pre-1.7 version where that field still existed). NON-FATAL: the
  log shows "Resource file successfully registered" + "Duplicate successfully created" + the SCR_AIWorld entity
  created + "Game successfully initialized" — the engine skips the one unknown line. Our explicit values
  (`m_fAttackReactionDelayModifier 0` etc.) are set in our DERIVED prefab, so they override the inherited ones
  regardless. Nothing to fix in our file; can't edit a packed dependency prefab.
- HARMLESS (E): `PATHFINDING: no terrain` + `Thumbnail request timeout` — Workbench prefab-edit context with no
  world loaded; not a runtime/dedi error.

## Phase 12 — "Matrix-dodge" / instant-prone ROOT CAUSE = Skarris AI settings data (2026-05-31, Bryce)
Bryce copied the Skarris tuning into `Prefabs/AI/SCR_AIWorld.et` (an `SCR_AISettingsComponent` override of base
`{E0A05C76552E7F58}Prefabs/AI/SCR_AIWorld.et`; `m_bUseConfigFiles 0` so the inline values are authoritative).
The instant-prone "matrix dodge" is NOT a code bug and NOT `ForceStance` - it's reaction-DELAY compression in
that data. The base game plays a normal animated stance transition; these modifiers slam the reaction lead-time
to ~0 so the drop-to-cover/prone fires with no animation runway. CONFIRMED culprits + the fix applied:
- `m_fAttackReactionDelayModifier -1.9 -> 0` (THE main lever: restored base reaction timing; -1.9 s shaved the
  entire animated lead-in off the danger/drop reaction = the snap).
- `m_iCombatMoveToNextPositionDelayModifier -3 -> 0` (stop the sped-up reposition cadence that churns stance).
- `m_iCombatInCoverDynamicCoverSearchChance 91 -> 45` and `m_iGlobalCombatInCoverDynamicCoverSearchChance 91 -> 45`
  (91% = near-constant cover re-seek every eval = up/down churn; 45 calms it without disabling cover use).
LEFT INTACT (Skarris realism Bryce presumably wants): accuracy (`m_fAimAccuracyErrorModifier -0.47`), combat mode
RED/OFFENSIVE, perception multipliers, suppression, burst fire, `m_iDangerReactionChance 88`, formations.
- ⚠ API NOTE: these tuning fields are CONFIG attributes, NOT in the public script API dump (only the 9 bool
  enables are). Values verified by reading the prefab Bryce supplied; base defaults for the modifiers are 0
  (they are "modifiers"). No code/GUID guessing - pure data edit on the supplied file.
- ⚠ WIRING TO VERIFY in-engine: this prefab's `.meta` GUID is `B7665F0AC486B8F1` and it INHERITS from base
  `E0A05C76552E7F58` - confirm the running world actually instantiates THIS prefab (i.e. Skarris/the scenario
  references it) so the edited values take effect. If the live AISettings still read 91/-1.9, the active settings
  come from elsewhere (Skarris's own packed prefab or a scenario override) and the same 3 values must be changed there.
- The DCO stance-cooldown seam (`m_bEnableStanceCooldown` + `m_bStanceCooldownInCover`) remains as a secondary,
  runtime rate-limiter; with the reaction-delay fix it should no longer be needed for the dodge, but it's there.

## Phase 11 — Live-issue batch: vehicle-crew exclusion, armour jitter, stance, prone (2026-05-31, Bryce)
Goal list from Bryce's live session, all addressed below. ⚠ COMPILE PENDING (paste next reload log).
1. **Tactical Movement was affecting crewed vehicles** (downgraded a driving crew to WALK / side-stepped its
   destination → wrong path + jitter). FIXED.
2. **Unit Merging (F3) was affecting crewed vehicles** (a depleted crew got folded into a foot element and
   abandoned its objective; "commander gets out / vehicle joins a larger element"). FIXED.
3. **Adaptive Formation could poke a crew** (commander dismounts to reposition / "use binos") — same class of
   bug; pre-emptively FIXED even though only 1+2 were reported.
4. **Vehicle armour angling jitter while driving.** FIXED (was the root jitter source).
5. **Stance cooldown "not sure it's working" + enemies go prone too fast.** Lever added + root-cause documented.

### Shared fix — vehicle-crew detector (`Scripts/Game/DCO/Util/DCO_VehicleUtil.c`)
NEW plain (non-modded) static helper `DCO_VehicleUtil.IsGroupInVehicle(SCR_AIGroup)` = leader occupies a vehicle
(`CompartmentAccessComponent.GetVehicleIn(leader) != null`). PLAIN class ⇒ calls are ORDER-INDEPENDENT (the
Enforce cross-fragment ordering trap only applies to `modded class` methods), so every system can gate on it
regardless of folder order. Guards added:
- `DCO_TacticalMove.InitParameters` — if the move group is a vehicle crew, just call super (no WALK downgrade / sidestep).
- `DCO_StragglerMerge.DCO_UpdateMerge` — skip if the straggler is a crew; also skip crew groups as a merge TARGET.
- `DCO_FormationComponent.DCO_UpdateFormation` — skip crews entirely.

### Vehicle armour-angling jitter (`DCO_VehicleArmor.c`)
ROOT CAUSE: `DCO_OrderVehicleFaceEnemy` re-broadcast a move-to-enemy order EVERY check tick (was 1.5 s) while the
hull was outside the frontal arc, constantly fighting the driver = the "weird jitter while driving." FIX:
- HYSTERESIS: a reorient only STARTS once the threat is beyond `m_fVehicleFrontalArcDeg + m_fVehicleReorientDeadbandDeg`
  (25°), but "presented" stays the plain arc - so it no longer oscillates around the boundary.
- ISSUE-ONCE: the face order is sent only when not already reorienting, OR when the enemy has moved past
  `m_fVehicleReorderDist` (20 m) from the last ordered position (`m_vDCO_LastVehicleOrderPos`) - no more
  every-tick re-broadcast.
- Check interval relaxed 1.5 s → 2.5 s. New settings on `DCO_MoraleSettings` + JSON + (no new GM slider; tune via JSON).
  Still default OFF and still fundamentally limited (no in-place AI hull-rotate exists - it noses forward; documented).

### Stance cooldown + "enemies prone too fast"
- The fast-prone behaviour most likely originates in **CRX or SkarrisCRXAIRealism** (the realism mod's whole
  point is aggressive suppression/cover/prone reactions). Those are shipped PACKED (`.pak`) and are unreadable
  by the tooling here (`game_browse`/`game_read` return 0 files), so they can't be patched offline - the DCO
  lever is the stance-cooldown seam (`modded SCR_AICombatMoveState.ApplyNewRequest`).
- NEW toggle `m_bStanceCooldownInCover` (default OFF): by default the cooldown throttles only free-standing
  ChangeStance (the in-cover peek/duck was excluded to stop a gear-physics shudder - see the 2026-05-29 gear-shudder
  fix). If AI still drop prone too eagerly, turning this ON ALSO throttles `SCR_AICombatMoveRequest_ChangeStanceInCover`.
  `DCO_IsThrottledStanceRequest` now adds that branch behind the toggle.
- DIAGNOSING "is the cooldown working": it rate-limits the FREQUENCY of in-place stance flips, not the speed of a
  single drop, and only when `!IsMoving()`. To see it: enable "AI Stance Cooldown", set `m_fStanceCooldownSec` high
  (e.g. 6), and watch whether the prone/stand cycling slows. If only the FIRST eager prone is the problem (not
  cycling), that's CRX/Skarris choosing prone - capping that needs the stance-request target field confirmed
  in-engine (the `SCR_AICombatMoveRequest_*` classes aren't in the public API dump).

## Phase 10 — Surrender recovery / re-arm + server JSON config (2026-05-31, Bryce's direction)
A surrendered AI group can now RECOVER: reactivate, stand up, restore its faction, re-arm and rejoin the fight.
Plus a server-owner JSON to set every DCO variable in advance. ⚠ COMPILE PENDING (EnfusionMCP handler not
loaded here; paste the next reload log to confirm). All APIs verified via api_search + the vanilla
SCR_InventoryStorageManagerComponent source Bryce pasted.
- **Recovery (`DCO_GroupMorale.c`, same fragment as surrender to avoid cross-file ordering):**
  - At genuine surrender, `DCO_CaptureSurrenderRecord` snapshots each member's faction + held weapon entity +
    weapon prefab (`GetCurrentSlot().GetWeaponEntity()` / `GetPrefabData().GetPrefabName()`) BEFORE the drop/neutralize.
    New `DCO_SurrenderRecord` class; records held in `m_aDCO_SurrenderRecords`. Fakers are NOT captured.
  - `DCO_Surrender` stamps `m_fDCO_SurrenderStartTime`. `DCO_UpdateSurrenderRecovery(now)` runs from the
    surrendered branch of `DCO_UpdateMorale` (throttled `m_fRecoveryCheckSec`): regains morale while safe;
    recovers when morale >= `m_fRecoveryMoraleThreshold` OR no contact for `m_fRecoveryNoContactSec`, after a
    `m_fRecoveryMinSurrenderSec` floor, by `m_fRecoveryChancePerTick`. NEVER recovers while an enemy is perceived.
  - `DCO_RecoverAgent`: `AIControlComponent.ActivateAI()` (undo Fix-1 freeze) + clear no-fire + STAND + restore
    faction. `DCO_RearmAgent`: re-pickup the dropped weapon via `TryInsertItem(weapon, PURPOSE_WEAPON_PROXY)`;
    if the entity is gone (`IsDeleted()`) and `m_bRecoverySpawnWeaponIfLost`, spawn from the captured prefab via
    `TrySpawnPrefabToStorage`. `DCO_ResupplyAfterRearm` (delayed 600 ms) calls `SCR_InventoryStorageManagerComponent.ResupplyMagazines(count)`
    for ammo, plus optional throwables from a server-set prefab (no fabricated GUIDs).
  - Master GM toggle "Surrender Recovery" (`{5DC0DC21…}`, `m_bIsServer 1`, default OFF) + 11 tunables on `DCO_MoraleSettings`.
- **Server JSON (`Scripts/Game/DCO/Config/DCO_JsonConfig.c`):** `DCO_MoraleSettings.Get()` lazily calls
  `DCO_JsonConfig.LoadInto` ONCE on singleton creation; reads `$profile:DCO_Settings.json` via
  `FileIO.FileExists` + `SCR_JsonLoadContext.LoadFromFile`/`ReadValue` (missing file/key = baked default kept,
  safe no-op). Covers every `DCO_MoraleSettings` field. Template: `Configs/DCO_Settings.example.json`. JSON =
  startup baseline; the GM tab still overrides live. (Tactical-move settings live in a separate singleton and
  are not in this JSON yet.)
- ⚠ RUNTIME-VERIFY on dedi: `ActivateAI()` cleanly un-freezes a surrender-frozen agent; `TryInsertItem` re-picks
  a ground weapon into the slot; `ResupplyMagazines` refills the re-armed weapon; JSON path resolves on the
  dedicated server profile dir.

### Phase 9 review + hardening + default-ON (2026-05-31, Bryce's direction) — F1/F2 LIVE by default
Re-reviewed all three Phase 9 systems against the 1.7 API (confirmed via api_search: `QueryEntitiesByLine`,
`SCR_AIGroup.AddAIEntityToGroup`/`RemoveAIEntityFromGroup`/`GetPlayerAndAgentCount`/`GetPlayerCount`,
`FactionAffiliationComponent.GetAffiliatedFaction`, `AIAgent.GetParentGroup`, `SCR_AIWorld.GetAIAgents`) — all
F1/F2/F3 calls are API-sound, no compile defects expected. Verified all 3 GM toggles exist in
`DCO_MoraleEditorAttributes.c` and are registered in `DCO_Attributes.conf` (`{5DC0DC1E/1F/20…}`, all `m_bIsServer 1`).
- **F1 fratricide + F2 shared morale pool → DEFAULT ON** (Bryce: both address active live-session problems).
  Flipped `m_bEnableFriendlyFire` and `m_bEnableSharedMorale` to `true` in `DCO_MoraleSettings.c`; updated the two
  GM attribute descriptions to "Default Yes". Both remain GM-toggleable (opt-out).
- **F3 straggler merge → stays DEFAULT OFF** (riskiest — live group-membership mutation; test in a controlled
  session before enabling on the live server).
- **HARDENING (applied):**
  - F2 perf: the shared-pool `O(agents)` scan is now computed ONLY when a group's morale has actually collapsed
    to `m_fSurrenderThreshold` (gated inside the surrender branch in `DCO_UpdateMorale`), not every 2 s tick for
    every healthy group — important now that it's ON by default. Behaviour unchanged (the else-if flee/panic
    chain is preserved).
  - F2 robustness: `DCO_GetPooledLossFrac` now floors each pooled group's max-strength at its current count, so a
    group that hasn't yet recorded its peak can't inflate the pool's loss fraction.
  - F3 robustness: a merge candidate that is itself queued to merge away this frame is now skipped (`candGu.m_DCO_PendingMergeTarget`),
    avoiding chained-merge thrash. (Deferred-transfer + player-group guards were already present.)
- ⚠ COMPILE PENDING: paste the next Workbench reload log to confirm the clean build + CRC (EnfusionMCP handler
  still not loaded here, so I can't drive the compile). RUNTIME-VERIFY on dedi: F1 reads as fewer friendly-fire
  incidents without over-suppressing return fire (tune `m_fFriendlyFireHold`/`m_fFriendlyLaneCheckSec`); F2 small
  teams near a strong force stop folding early (tune `m_fMoralePoolRadius`); F3 (when enabled) membership transfer
  replicates and merged agents adopt the target group's activity.

## Phase 8 — Remaining multi-session systems: design + viable approach (2026-05-31)
These three MODPLAN items have NO ready-to-wire 1.7 API (confirmed via api_search + reading the real source
for fireteams/smoke). Each needs a custom subsystem. Scoped here so they're plannable, not hand-wavy:

### R6 — Bounding overwatch
- WHAT'S AVAILABLE: `SCR_AIGroupFireteamManager` (GetFreeFireteams / FindFireteam) + `SCR_AIGroupFireteam.GetMembers(array<AIAgent>)`
  give the element split for free. Messages can target a SPECIFIC agent (smoke source: `msg.SetReceiver(agent); comms.RequestBroadcast(msg, agent)`).
- WHAT'S MISSING / THE RISK: `SCR_AIGroupFireteam` is membership-only (no move/command/waypoint). Movement is
  whole-group via the leader. The open question is whether `SendMoveMessage` targeted at a NON-leader agent
  moves just that agent or fights group cohesion. STEP 1 (cheap experiment, gated OFF): send a move to one
  fireteam's agents and observe — if a subset relocates without the group snapping it back, bounding is
  buildable (split into 2 elements, alternate move/hold, track which bounds). If cohesion overrides it, R6
  needs a deeper hook (modded move behaviour) and is a larger lift.

### M2–M4 — Cover-biased movement / path exposure
- ALREADY DONE: M1 (cautious WALK) + anti-funnel side-step (M2-lite). CRX already does IN-CONTACT cover-to-cover.
- **M4 exposure scoring ✅ IMPLEMENTED** (`DCO_TacticalMove.DCO_PickLeastExposed` / `DCO_IsCovered`). On a tactical
  move it raycasts (`BaseWorld.TraceMove`, frac < 1 = blocked = covered) from the threat to the chosen destination
  + two lateral alternatives, and routes to the first the threat can't see. Avoided the `TraceFlags` enum entirely
  by configuring the trace with the **int `LayerMask`** field (a wrong/zero layer degrades to a safe no-op — keeps
  the chosen destination — never a worse path). GM toggle "Exposure Scoring" (`{5DC0DC1D…}`, default OFF; requires
  Enable Tactical Movement). RUNTIME-TUNE: `m_iExposureLayerMask` (the world-geometry layer bitmask) — the feature
  is inert until that's a value that catches geometry; set it once a base-game trace layer is known. M2/M3
  (cover-routing / true bounding) still need the unexposed cover-query / sub-element primitives.

### Garrison — building-interior firing positions
- ALREADY DONE: Defensive Hold (`DCO_DefendComponent` — hold + orient defence toward threat) is the achievable
  defensive piece.
- WHAT'S MISSING: enumerating window/firing positions inside a structure (no API; `SCR_DestructibleBuildingComponent`
  is destruction, not nav). VIABLE APPROACH: a GM-placed "garrison post" marker system (the GM drops firing-position
  markers in/around a building; flagged groups occupy the nearest free markers via `SendMoveMessage` + Defend) —
  sidesteps the missing auto-detection by making positions designer-authored. That's a real placeable+component
  build (like the original QRF-zone idea), not a one-liner.

## Phases (DCO expansion) — all PENDING
Each phase: new behavior/component scripts under `Scripts/Game/DCO/`, a Game Master toggle, then compile + play-test in Workbench before the next.

### Phase 1 — Morale & Break / Surrender  (IN PROGRESS — core done, GM settings pending)
A morale meter per group fed by engine threat measure (suppression/shots/injury) + casualties (lost strength); recovers when safe. Below FLEE threshold the group breaks and is sent a native `SCR_AIMessage_Flee` away from perceived enemies; below SURRENDER threshold it gives up (weapons lowered + held-fire).
- `Scripts/Game/DCO/Morale/DCO_GroupMorale.c` — DONE. `modded SCR_AIGroupUtilityComponent`: morale state + decay/recovery in `EvaluateActivity` (server-only, throttled 2s), `DCO_BreakAndFlee` (native flee msg via `m_Mailbox`), `DCO_Surrender` (`SetWeaponRaised(false)` + `SetWeaponNoFireTime` per member via `m_Owner.GetAgents`). Compiles clean on 1.7 (verified via live wb_reload; CRC 3255a8f5, 0 errors). Implemented on engine API only — no CRX coupling beyond stacking with super.
- KNOWN: `m_Mailbox.CreateMessage` is deprecated in 1.7 (warning only, still works).
- DONE (compiles clean on 1.7; runtime to verify on dedicated server) — Enhanced immersive surrender in `DCO_GroupMorale.c`, per member:
  (a) non-threat: `FactionAffiliationComponent.SetAffiliatedFaction(null)`;
  (b) lower weapon + hold fire (`SetWeaponRaised(false)`, `SetWeaponNoFireTime`);
  (c) immediate `ForceStance(ECharacterStance.CROUCH)`;
  (d) +1.5s `DCO_AgentDropGear`: `DropCurrentItem()` for the weapon, then drop vest/armor/backpack/headcover/goggles/binoculars/cover/handwear via `GetClothFromArea` → `FindItemSlot().GetStorage()` → `TryRemoveItemFromStorage` (to ground); keeps jacket/pants/boots;
  (e) +3s `ForceStance(ECharacterStance.PRONE)`.
  Timed beats via component `GetGame().GetCallqueue().CallLater`. SERVER TEST TODO: confirm faction-clear de-aggros enemies, stance holds vs AI, and inventory drops replicate to clients.
- DONE — Plug-and-play settings: `DCO_MoraleSettings.c` is a lazy global SINGLETON (defaults baked in) read by the morale system → works in EVERY GM mode with NO scenario setup. `DCO_MoraleSettingsComponent.c` is now OPTIONAL (copies its `[Attribute]` values, "25th DCO" category, into the singleton on init for designers who want to bake values into a prefab). NOTE: Enforce has no ternary `?:` and rejects compact one-line `if/return` bodies.
- DONE (code) — `DCO_MoraleEditorAttributes.c`: `DCO_*EditorAttribute : SCR_BaseEditorAttribute` (enable, flee/surrender thresholds, enable-surrender, drop-gear) with `ReadVariable`/`WriteVariable` against the singleton. Compiles clean. These are the live "25th DCO" Game Master tuning controls.
- DONE — "25th DCO" Game Master tab. `Configs/Editor/AttributeLists/Edit.conf` (overridden in addon, GUID `A4DEDD9410978E68`) now lists the 5 DCO attributes under category `Configs/Editor/AttributeCategories/DCO.conf` (`{6FD4B67122DABD68}`, Name "25th DCO", global category). Loads with no config errors. In-GM visual appearance to be confirmed on Bryce's test. NOTE: the Edit.conf override necessarily contains CRX's existing attribute entries (required so CRX's own GM controls keep working) — that's a list config, not CRX script logic.
- POLISH (later): hands-up surrender animation, flee-direction tuning, per-agent morale modifiers (rank/skill).

### Phase 2 — Reinforcement / Shared Situational Awareness  (IN PROGRESS — shared-SA core done)
Groups in contact share the enemy position with nearby friendlies so contact spreads across the battlefield.
- DONE — `Scripts/Game/DCO/Comms/DCO_GroupReinforcement.c` — second `modded SCR_AIGroupUtilityComponent` (merges with morale); `DCO_UpdateReinforcement()` called from the shared `EvaluateActivity` tick. While a group has a perceived enemy, every `m_fReinforcementCooldownSec` (8s) it enumerates `SCR_AIWorld.GetAIAgents`, finds same-faction groups within `m_fReinforcementRadius` (300m) of its leader, and injects the enemy contact into their perception via `SCR_AIGroupPerception.AddOrUpdateGunshot` → they become aware/react. Compiles clean (CRC b82dfa5e). Settings in `DCO_MoraleSettings` (enable / radius / cooldown).
- DONE — active converge: idle responders (no contact of their own) are ordered toward the enemy via `SCR_AIMessageHandling.SendMoveMessage(leaderAgent, enemy, null, AICommunicationComponent.Cast(m_Mailbox))`; engaged groups left to fight. Max-responders cap (`m_iReinforcementMaxResponders`, default 3) prevents map-wide cascade. GM "25th DCO" attributes added for enable / radius / max-responders (8 DCO attributes total in the tab). Compiles clean (CRC c7cf1886).
- SERVER TEST TODO: confirm `AddOrUpdateGunshot` makes receivers react & `SendMoveMessage` converge works (timestamp units / not disrupting engaged groups); tune radius/cooldown/cap.

### Phase 3 — Smoke Maneuver & HVT Targeting  (BLOCKED on API verification — deferred)
- Smoke-on-flee: attempted via `SCR_AIActivitySmokeCoverFeature.Execute(...)` but the 1.7 signature differs from the scraped docs (arg 2 is an `int`, not a vector) and `SCR_AIActivitySmokeCoverFeatureProperties` isn't a resolvable class. Reverted.
  - **API re-verified 2026-05-30:** the clean route is NOT a broadcast message (there is none) — it's `SCR_DeploySmokeCoverWaypoint` (an `AIWaypoint` subclass: `SetMaxGrenadeCount`, `Set/GetSmokeCoverProperties(SCR_AIActivitySmokeCoverFeatureProperties)`, `CreateWaypointState`). `SCR_AIDeploySmokeCover` is the underlying BT *task node* (`AITaskScripted`, holds `MAX_SMOKE_POSITION_COUNT`), not callable directly. PLAN: on break/flee, spawn a `SCR_DeploySmokeCoverWaypoint` at the group and add it to the group's waypoint list (like the flee msg but a waypoint). BLOCKER (needs in-engine, not offline): creating the waypoint at runtime needs its **prefab GUID to spawn** (or the engine's waypoint-spawn helper), and the offline asset index is empty so the GUID can't be resolved here — and `SCR_AIActivitySmokeCoverFeatureProperties` defaults need confirming. Do this in a live-Workbench session (grab the smoke-cover waypoint prefab GUID from the Resource Browser), gated OFF + tied to `DCO_BreakAndFlee`.
- HVT targeting: injecting target-selection priority is non-obvious (engine-internal). Needs research.

### QRF Zone — Game Master placeable  (DONE, code-complete; in-GM verification pending)
GM-placeable reserve/QRF system, per Bryce's spec.
- `Scripts/Game/DCO/QRF/DCO_QRFZoneComponent.c` — radius (default 50 m) defines QRF membership (groups whose leader is inside). On a timed check it auto-detects the zone faction from groups inside, finds a same-faction group in contact within `m_fResponseRange` (default 1500 m), and orders all QRF groups to move/support via `SendMoveMessage`. Re-arms when the contact clears. Server-authoritative. Compiles clean (CRC 33b08e57).
- `Prefabs/DCO_QRFZone.et` — GenericEntity with MeshObject (placeholder BarrelGreen mesh — swap for a fitting marker), `SCR_EditableEntityComponent` (display "25th DCO QRF Zone"), `DCO_QRFZoneComponent`.
- `Configs/Editor/DCO_QRFPlaceables.conf` — `SCR_PlaceableEntitiesRegistry` (category "25th DCO") registering the prefab for Game Master. Loads with no config errors.
- IN-GM TODO: confirm it appears in the GM asset browser under "25th DCO" and places/selects correctly; QRF logic fires when a friendly is in contact. POLISH: a real 50 m ground-circle visual (decal/spline ring) instead of the placeholder mesh; QRF settings in the "25th DCO" tab; possible RplComponent if MP placement needs it.

### Phase 4 — Vehicle Hijacking & Garrison  (PENDING)
AI commandeer nearby empty vehicles (esp. with turrets) when advantageous; assign units to building garrison positions with coordinated room clearing.
- `Scripts/Game/DCO/Behaviors/DCO_CommandeerVehicleBehavior.c` (uses `SendGetInMessage`)
- `Scripts/Game/DCO/Garrison/DCO_GarrisonManager.c`
- Settings: hijack toggle, garrison density.

### Phase 5 — POW / Capture (stretch)  (PENDING)
Surrendered units become capturable: restrain interaction, escort/detain, optional execution. Requires custom interaction + animation; revisit feasibility at start of phase.

### Phase 6 — Tactical Maneuver / "Smarter Movement"  (NEXT BIG TASK — Bryce 2026-05-29)
**Goal:** when a group is given a move order, it should NOT blindly path straight to the destination. It should weigh each leg for cover/concealment/exposure, insert non-visible "check-in" sub-objectives along the route, and at each one observe → halt → take cover → bound to the next covered position.

**Key research finding — build ON CRX, don't reinvent.** CRX Enfusion A.I. already implements (workshop-confirmed): Observe-Threat System w/ flanking, cover-to-cover movement, break-out/desperation movement, "Move and Investigate" (range/pos from enemy knowledge + threat), a Combat-Range system, and a GM "Combat move and cover duration modifier." Most *in-contact* maneuvering is already CRX's job. DCO's value-add is **pre-contact / on-order tactical movement** — the deliberate approach, which CRX does less of.

**Engine seams (validated via api_search 1.7):**
- `SCR_AIMoveActivity` (subclass of `SCR_AIActivityBase`) is the group's move-to-position activity. Ctor `(utility, relatedWaypoint, pos, ent, EMovementType, useVehicles, priority, priorityLevel)`; public ports `m_vPosition`, `m_eMovementType`, behavior tree `m_sBehaviorTree`; overridable `CustomEvaluate()`, `OnActionCompleted/Failed/Deselected`. **`modded SCR_AIMoveActivity` is the primary hook.**
- **Activity Features** (`SCR_AIActivityFeatureBase`, e.g. existing `SCR_AIActivitySmokeCoverFeature`/`SCR_AIActivityIllumFlareFeature`) = the engine's supported way to bolt extra behavior onto an activity via `GetActivityFeatures()`/`FindActivityFeature()`. Candidate clean extension point for a DCO maneuver feature.
- **Waypoints:** `AIWaypoint`/`SCR_AIWaypoint` — `SetCompletionRadius/Type`, `CreateWaypointState(groupUtility)`, `AddSetting`; `AIWaypointCycle.PerformOn` adds waypoints to a group's list. Basis for inserting invisible intermediate check-in waypoints.
- **Cover:** `SCR_AICoverLock` (cover point: pos, dir, tallest pos, `CosAngleToThreat`), `SCR_AICombatMoveState.AssignCover()`, cover-query nodes `SCR_AICalculateCoverQueryProps_CombatMove` / `SCR_AICalculateNextCombatMovePos`. Basis for cover/concealment-biased leg targets.
- ⚠️ `EMovementType` enum members not yet confirmed via DB (search returned none) — DO NOT reference members until verified in-editor (no guessing per Bryce's GUID/enum rule).

**Phased delivery (each behind a GM toggle, default OFF, server-side, players never affected):**
- **M1 — Foundation + cautious approach.** ✅ SCAFFOLD LANDED + compiles clean (CRC `2492cb04`, +3 files/+6 classes): `DCO_TacticalMoveSettings` store + four "25th DCO" GM attributes (Enable Tactical Movement + Min Distance/Check-in Interval/Observe Pause sliders, GUIDs `{5DC0DC10…}`, all `m_bIsServer 1`, default OFF) in `Scripts/Game/DCO/Movement/`, registered in `DCO_Attributes.conf`. `modded SCR_AIMoveActivity` is a safe pass-through (`CustomEvaluate` → super; gated `DCO_TacticalMoveEngaged()`), NO behavior change yet. DESIGN DECISION (made): option (a) — deliberate approach engages ONLY when the group has a threat/contact, so routine patrols stay responsive. ✅ IMPLEMENTED + compiles clean (CRC `09fb2705`). `EMovementType` confirmed by Bryce = `IDLE/WALK/RUN/SPRINT`. Behavior in `DCO_TacticalMove.c`: `modded SCR_AIMoveActivity.InitParameters` downgrades RUN→WALK when `m_bEnableTacticalMove` + server + no follow-entity + `m_Utility.GetThreatMeasure() >= m_fThreatActivation` (0.10) + `distance(group,target) >= m_fMinMoveDistance`. SPRINT/WALK/IDLE + escort moves untouched. Confirmed in-compile: `GetThreatMeasure()` IS callable externally (no visibility error). Chosen lever = downgrade the activity's movement type at InitParameters (simpler/safer than the AI speed-setting system). REMAINING: GM-test the feel (new task) before M2.
> ✅ **M2–M4 REALIZED (2026-06-02)** by the procedural cover-pathing + bounding system (`DCO/Movement/DCO_TacticalPath.c`) + cover placement (`DCO/Formation/DCO_FormationComponent.c`) — see the 2026-06-02 entry below. The per-leg-waypoint design described next was superseded by a procedural-reassessment approach (re-decide the covered route every few seconds as the group moves) which avoids waypoint-prefab GUIDs. Original M2–M4 text kept for reference:

- **M2 — Cover/concealment-biased check-in legs.** For each leg, query nearby cover (`SCR_AICoverLock`/cover-query) and route the intermediate waypoint through the most-covered position toward the goal; brief observe-halt on arrival.
- **M3 — Bounding & overwatch.** Split group into elements (reuse CRX `SCR_AIFireteamsActivity` where possible); alternate move/overwatch between covered positions when threat is elevated.
- **M4 — Path exposure scoring.** Score candidate routes by exposure (cover density + LOS to known threat clusters via `SCR_AIGroupPerception`) and pick the lowest-exposure path.

**Risk note:** AI movement internals are partly compiled (not browsable as .c), and this touches live AI behavior with real end-users. Every increment ships OFF by default and must be GM-tested before recommending enablement. If a maneuver hook destabilizes pathing, fall back to the previous CRC.

## 2026-06-02 — Aircraft/disembark fixes, surrender refinements, #5 tactics suite (Phases A–D)
Driven by an end-user report (a modded `SCR_AIGroup` spawn-pipeline callstack — diagnosed as CRX/base + likely load-order/prefab on that machine, NOT our code) plus Bryce's priorities. Everything new is **default-OFF, server-authoritative, JSON-wired** in `DCO_Settings.json` (example updated). Compiles clean (latest Game module CRC `b7d7ec2d`, warnings only). **No test world available — all of this is code-correct + compiling, NOT behaviour-verified.**

### Random vehicle disembark / helicopter crews falling from the sky — FIXED
Root cause: the on-foot move primitive (`SCR_AIMessageHandling.SendMoveMessage`) issued to a **mounted** group makes the AI dismount and walk — fatal for aircraft. Centralized on new vehicle-aware helpers `DCO_VehicleUtil.OrderGroupMoveToEntity` / `OrderGroupMoveToPosition` (`useVehicles=true` when mounted). Gated every move/relocation vector: QRF, Reinforcement fallback, VehicleArmor, Defend; and made morale **flee** (`DCO_BreakAndFlee`), **surrender** (`DCO_Surrender`), and **panic** (`DCO_Panic`) skip/vehicle-aware for mounted crews (heli crews fly/drive away or stay mounted instead of bailing). `DCO_UpdateArmorAvoid` + `DCO_VehicleHijack` were already gated.

### Surrender → "go white" reliability — FIXED
Root cause (`api_search`): AI targeting reads the **perceived** faction (outfit-derived) via `PerceivableComponent`, not the affiliated faction — a uniformed surrenderer kept getting shot. `DCO_NeutralizeFaction` now also calls `PerceivableComponent.SetDisarmed(true)` and re-asserts (`DCO_ReassertNeutralized`).

### Fake-surrender grenade — REWORKED
Decoupled the faker-SELECTION chance (`m_fFakeSurrenderChance`) from the in-range TRIGGER chance (`m_fFakeSurrenderDropChance`, 6 m / 25%). Spawn-a-grenade-if-none via `TrySpawnPrefabToStorage` (`m_sFakeSurrenderGrenadePrefab`, empty default; no-loop retry). Waiting fakers posed prone (no more "stands holding a grenade"); fake surrender is **terminal** (`m_bDCO_HasCommittedFaker` blocks recovery).

### Morale cohesion — leadership model (Bryce-corrected)
First attempt (leader-resilience / leaderless-drain multiplier) was wrong — a group is never leaderless (the engine promotes a replacement). Replaced with a **leader-loss shock**: one-time morale hit (`m_fMoraleLostOnLeaderLoss`, default 15) when the leader entity changes coincident with a fresh casualty (decapitation). Surrender stagger default tightened 1.5→0.8 s (squad surrenders together, not raggedly).

### #5 TACTICS SUITE — Phases A–D (doctrine: FM 3-90 *Tactics* + MCA *Mastering Tactics*)
All gated default-OFF, server-side, debug-instrumented, JSON-wired in `DCO_TacticalMoveSettings`.
- **Procedural cover-pathing + bounding overwatch** (`DCO/Movement/DCO_TacticalPath.c`): under danger, re-decides the route through cover as the group moves — forward-cone sampling scored by concealment (`AIPathfindingComponent.RayTrace`) + progress, snapped to navmesh (`GetClosestPositionOnNavmesh`), bounding leg-by-leg, with a **live debug path** (Shape arrows). **This realizes Phase 6 M2/M3/M4 as a unified system.**
- **Phase A — Movement techniques:** danger-band selector → traveling / **traveling-overwatch** (lead bounds, trail follows) / **bounding overwatch** (base-of-fire + leapfrogging maneuver element).
- **Phase B — Cover placement** (`DCO_FormationComponent.DCO_UpdateCoverPlacement`): in contact, nudge exposed members to the nearest concealed navmesh spot. Formation *selection* stays the configurable `SetFormation`. Echelon/Vee + true dispersion need custom `AIFormationDefinition` (Workbench) — not done.
- **Phase C — Reaction to contact** (`DCO/Contact/DCO_ReactToContact.c`): FM 3-90 actions-on-contact — SUPPORT_BY_FIRE (`SetFireRateCoef` boost + hold), ASSAULT_FLANK (envelopment via `DCO_FlankOf`), BREAK_CONTACT (withdraw).
- **Phase D — Tactical brain** (`DCO/Brain/DCO_TacticalBrain.c`): own:enemy combat-power ratio → chooses the COA (`m_eDCO_COA`) Phase C executes (assault flank when stronger, support when even, break when overmatched).
- **Orchestration:** the `EvaluateActivity` tick runs Brain → ReactToContact → … → Formation → CoverPlacement → TacticalPath; inter-phase comms via the shared `m_eDCO_COA` field declared in the earliest ("Brain") fragment.

### Critical Enforce lessons (added to memory)
- **modded-class FIELDS resolve in file-processing order too** (not just methods) — a fragment can only reference a field whose declaring fragment sorts earlier. (Took the module down once; fixed by declaring shared `m_eDCO_COA` in `DCO/Brain`.)
- **Never delete a script class still referenced by a `.conf`** — orphaned entries make the whole attribute list fail to load (recovered via temporary no-op stub classes, then Bryce removed the entries in the Workbench).
- **Hand-adding a UNIQUE embedded GUID to a `.conf` array works** — the real prohibition is file/`.meta`/resource-DB GUIDs. Added the "Morale: Leader-Loss Shock" GM slider by direct text edit (`{7C3A9E1F5B2D4A60}`), verified it loads via the live `console.log`.

### GUID registration
Fixed the pre-existing `0E` duplicate (grenade-drop vs defender) via Workbench re-add (Bryce). Self-verify compiles by reading `logs/logs_<newest>/console.log` for `Can't compile` vs a clean `Module: Game; … CRC32:`.

### In-engine validation still owed (no test world here)
Per-member `RequestBroadcast` isolation (make-or-break for traveling-overwatch / bounding / cover-placement element control); `SetDisarmed` actually disengaging enemies; spawned-grenade selectability; bound smoothness; leader-loss-shock + COA threshold tuning.

## 2026-06-02 (cont.) — outstanding-feature closeout
Cleared the "genuinely outstanding" list except POW/Capture (dropped). Compiles clean (Game module CRC `16990ce9`). All default-OFF, server-authoritative, JSON-wired (example updated). No test world → code-verified, not behaviour-verified.
- **R5 vehicle-borne reinforcement — finalized.** The `useVehicles=true` converge move is the engine's board-then-converge; added a guard so it only fires for on-foot, far responders (already-mounted just drive). No conflicting get-in order.
- **Echelon / Vee / dispersion formations — added.** `DCO_FormationComponent.DCO_UpdateFormationShape` + `DCO_FormationOffset` (enum `EDCO_FormShape`: column/wedge/line/echelon-L/echelon-R/vee), per-member offsets from the leader, oriented to heading or threat, scaled by `m_fFormationSpacing`. Settings `m_bEnableDcoFormations` + shape ints. EXPERIMENTAL: per-member moves fight CRX's formation; don't run with cover-seek on the same group.
- **Safe-eject gate — added.** `modded SCR_CompartmentAccessComponent.AskOwnerToGetOutFromVehicle` blocks a voluntary AI dismount when speed > `m_fSafeEjectMaxSpeedKmh` (15) or AGL > `m_fSafeEjectMaxHeightM` (3); forced ejects (`ejectOnTheSpot`) + players never blocked. Sensors: `Physics.GetVelocity()*MS2KMH`, `BaseWorld.GetSurfaceY`. (`GetOutVehicle` is `proto external`/native — not overridable; this gates the scripted "ask" path AI uses.)
- **`DCO_FlankSplit.c` — DELETED** (superseded by the Phase A/2 bounding overwatch).
- **Surrender recovery fix + fake-surrender go-loud + morale contagion** (earlier same day) — recovery now clears `SetDisarmed`; sprung fakers restore faction/un-disarm; nearby broken/surrendered friendlies cascade morale loss.
- **Fake-surrender grenade prefab set** to the RGD5 (`{6D1AF5BE65D92CF6}…Grenade_RGD5.et`).
- **DROPPED from scope:** POW/Capture (no anims), indirect fire (base game has it), R7 casualty drag (anims), dynamic-sim (engine handles).

## 2026-06-02 — Combat realism: suppression (done), AT-vs-infantry + asset-use (planned)
New direction (Bryce): make AI human-like under fire and use ALL assets. Research found the engine ALREADY has the threat/suppression brain — see [[ai-threat-suppression-assets]] in memory.
- **Real suppression — ✅ DONE** (`DCO/Morale/DCO_Suppression.c`, `DCO_UpdateSuppression`). Reads each member's `SCR_AIThreatSystem.GetSuppressionMeasure()` (via `SCR_ChimeraAIAgent.m_UtilityComponent`); when pinned ≥ `m_fSuppressionThreshold`: degrades fire (`SetFireRateCoef`), sprints to cover/breaks LOS (RayTrace+navmesh), and the group pops smoke (`DCO_DeploySmoke`) when a fraction is pinned. Default OFF; compiles (CRC `f0302297`). ⚠ Tune `m_fSuppressionThreshold` in-engine — the measure's numeric range is unknown (watch `[DCO:SUPPRESS]`).
- **AT weapons vs infantry — ✅ DONE** (`DCO/ATInfantry/DCO_ATAntiInfantry.c`, CRC `a99d8004`). When a group sees ≥ `m_iATInfantryMinCluster` infantry and no vehicle (unless `m_bATInfantryEvenWithVehicles`), each member's `AIWeaponTargetSelector.SelectWeaponAndTarget(infantry, …, whitelist=[EWeaponType.WT_ROCKETLAUNCHER])` forces a launcher engagement; the call returns false for non-launcher units so it self-filters to AT/LAT troops. Enums confirmed by Bryce: `EWeaponType.WT_ROCKETLAUNCHER`, `EAIUnitType.UnitType_Infantry`. ⚠ Selector is engine-tree-driven (re-asserts each tick, a shot isn't guaranteed) and the launcher may be config-gated to vehicles — validate in-engine.
- **Squad leader splits a team to use an asset (MG/mortar/static), then leaves it — ✅ DONE** (`DCO/Assets/DCO_AssetUse.c`, CRC `16ccd912`). In contact + spare fireteam → a maneuver member mans a nearby empty static weapon (found via `QueryEntitiesBySphere` + `SCR_BaseCompartmentManagerComponent`: 0 occupants + NO free crew seat = static, not a vehicle) via `SendGetInMessage(…, EAICompartmentType.Turret, …)`; recalled (dismount + rejoin) when the contact clears. (The `ECompartmentType.Turret` member does NOT exist — confirmed by a failed compile; used the `CREW_COMPARTMENT_TYPES` heuristic instead.)

## 2026-06-02 — Attic-climbing / garrison fix
Bryce: "AI consistently climb buildings until they all end up in the attic. Ensure tactical movement is garrisoning, not bunching." Diagnosed three compounding causes in our own movement code: (1) concealment/LOS scoring rewards hidden upper floors, (2) the navmesh snap used a ±6 m vertical extent (`GetClosestPositionOnNavmesh(pos, Vector(6,6,6), …)`) so a ground candidate could snap up onto a floor/roof above, and (3) per-tick ratcheting kept nudging the group higher each pass.
- **Fix — height-cap every cover/leg candidate to the current floor.** New setting `m_fCoverMaxClimb = 2.0` (in `DCO_TacticalMoveSettings`, JSON-wired in `DCO_JsonConfig` ApplyKeys+WriteDefaults + `DCO_Settings.example.json`). After snapping a candidate to navmesh, reject it if `cand[1] > origin[1] + m_fCoverMaxClimb`. Applied in `DCO_FormationComponent` (cover-seek loop), `DCO_TacticalPath.DCO_PickBestLeg`, and `DCO_Suppression.DCO_SuppSeekCover` (fetches the cap via `DCO_TacticalMoveSettings.Get()` since its own cfg is `DCO_MoraleSettings`).
- **Fix — tightened the navmesh snap vertical extent** from `Vector(6,6,6)` to `Vector(6,2,6)` in `DCO_FormSnapNav`, `DCO_SnapToNavmesh`, and `DCO_SuppSnapNav` so a lateral cover dash stays on the same floor instead of being pulled to the floor above. Horizontal ±6 m preserved.
- Compiles clean (Game module `CRC32: be211082`). Default behaviour unchanged when the relevant features are OFF; cap only constrains where the existing cover/path logic may send a unit. ⚠ Not behaviour-verified (no test world) — `m_fCoverMaxClimb` is the live tuning lever if AI still need to take legitimate stairs/upper floors. See [[procedural-tactical-path]].

## 2026-06-02 — Spawn-time role presets + order deconfliction with external waypoints (IPC / Scenario Framework)
End-user (24/7 public server, no GM around) reported: (1) no way to make groups spawn already configured as QRF/Ambush; (2) DCO reinforcement was yanking freshly-spawned IPC patrol/capture groups off their waypoints, spamming the log with duplicate move orders, leaving groups spread ~100 m apart then stopped. Root cause: DCO reinforcement treated "no perceived enemy" as **idle** — but a group walking to its waypoint has no enemy yet, so it got converge-ordered (and re-ordered every scan). Verified APIs: `SCR_AIGroup.GetCurrentWaypoint()/GetWaypoints(out)/RemoveWaypoint()`.
- **Deconfliction (`DCO/Comms/DCO_GroupReinforcement.c`).** A responder that still holds its OWN current waypoint is now treated as BUSY and left to finish (only converges once the waypoint clears = "finish IPC task, THEN act on DCO"), **unless** it's flagged reinforcement-eligible (then it OVERRIDES the waypoint). New per-responder re-issue guard (`DCO_TryMarkConverge`) stops re-ordering the same group every scan unless the contact moved past a retarget distance — kills the duplicate-order spam + wander. Engaged groups still left fighting; shared SA (awareness) still broadcast to everyone (it doesn't move them).
- **New settings** (`DCO_MoraleSettings`, JSON-wired + example): `m_bReinforcementRespectWaypoints` (default ON), `m_fReinforcementReissueSec` (20), `m_fReinforcementRetargetDist` (50), `m_bClearWaypointOnOverride` (default OFF — clearing a framework/IPC waypoint can desync that system; when ON, an overriding QRF/reinforcement group has its waypoints removed so the DCO move holds).
- **Spawn-time role preset (`DCO/Roles/DCO_GroupRolePreset.c`).** New ScriptComponent the user adds to an AI GROUP prefab: pick a Role (NONE/QRF/AMBUSH/DEFEND/REINFORCEMENT) + range; on spawn it calls the same accessors the GM attributes use (`DCO_SetQRFResponder/Range`, `DCO_SetAmbusher/Range`, `DCO_SetDefender`, `DCO_SetReinforcementEligible`). Server-only, deferred one frame (group utility ready), retry once. Lets them designate SF/armored prefabs as QRF and infantry as reinforcements with no GM. Make a prefab variant per role and point spawners at it.
- **QRF** got the same optional `m_bClearWaypointOnOverride` hook (it already overrode by issuing a move; this makes the override hold).
- Compiles clean (Game module `CRC32: f605f991`, file/class count +1/+3). ⚠ Not behaviour-verified (no test world): the waypoint-respect skip + throttle are low-risk; `m_bClearWaypointOnOverride` removes framework waypoints so validate with IPC before enabling. New `DCO_ReinforcementEligible` flag is currently set only via the preset (no GM attribute registered — that's a Workbench/GUID job if wanted). See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-02 — "Reinforcement Eligible" GM attribute (per-group)
Registered the per-group GM toggle for the new reinforcement-eligible flag (approved: hand-adding a UNIQUE embedded `.conf` GUID is allowed — see [[never-hand-author-guids]]). New class `DCO_ReinforcementEligibleEditorAttribute` (`DCO/Comms/DCO_ReinforcementEditorAttributes.c`, same resolve pattern as `DCO_QRFEditorAttributes.c`); registered in `DCO_Attributes.conf` under embedded GUID block `5DC0DC25` in the DCO_QRF category so it sits next to the QRF/role toggles on a selected group. Compiles clean (Game module `CRC32: fd6df331`, +1 file/+2 classes). NOTE: a conf reload BEFORE the script module recompiles logs a transient `Unknown class` — recompile then reload, don't panic.

## 2026-06-02 — GM placeable Task Zones (FOUNDATION) + behaviour redefinition (in progress)
Direction (Bryce): GM-placeable CIRCLES that highlight a group and assign a task (QRF/Ambush/Defend/etc), referencing 25thGMERemix's placeable "Modules" pattern. Decisions: GME-style custom placeable (not native waypoint mesh); I build prefabs via MCP; FOUNDATION first, then deepen each behaviour.
- **Foundation built + compiles clean (`CRC32: fb98cdad`)** in `DCO/Roles/DCO_TaskZone.c`:
  - `DCO_TaskZoneRegistry` (static list of live zones; trigger<->ambush pairing/queries).
  - `enum EDCO_ZoneRole {NONE,QRF,DEFEND,REINFORCE,AMBUSH,AMBUSH_TRIGGER}`.
  - `DCO_TaskZoneComponent` (ScriptComponent): attributes role/radius/checkSec/pairTag. Ticks on a CallLater loop; each tick links groups whose LEADER is inside the circle (flat XZ dist) and applies the matching DCO role flag (reuses QRF/Defend/Reinforce/Ambush we already built); clears the flag when a group leaves or the zone is deleted (destructor unregisters + clears). AMBUSH_TRIGGER = detached kill zone: when an enemy (AI or player, via `IsFactionEnemy`) enters, springs every paired AMBUSH zone (matched by Pair Tag).
  - Supporting accessors added: `DCO_SpringAmbush()` (Ambush) and QRF hold-point storage `DCO_SetQRFHoldPosition/Clear/Has/Get` (QRF) — hold/return behaviour comes in the QRF deepening pass.
  - API notes: NO `vector.DistanceSqXZ` (compute flat dist manually); confirmed `PlayerManager.GetPlayerControlledEntity`, `Faction.IsFactionEnemy`; used `array` not `set` for indexed manage-list; cast `CallLater` delay to int.
- **DONE earlier same session:** "Reinforcement Eligible" per-group GM attribute (`CRC32: fd6df331`).
- **STILL TODO (next passes):** (1) the GM-PLACEABLE PREFAB + editor-catalog registration via MCP (so a GM can actually drop the circle) + a client-visible circle marker (Shape is host-only; need an area mesh / decal model for remote GMs); (2) DEEPEN behaviours — QRF garrison hold + "things critical" trigger (morale+numbers low nearby) + return-to-hold; ambush all-guns volume; Defend/Patrol zones; (3) optional click-to-link context action (GME pattern) as a convenience over area-linking. See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-02 — GM placeable Task Zone: real placeable prefab + registry (self-contained, NO GME dependency)
Bryce: don't reinvent — reuse GME's placeable approach; but keep the mods SEPARATE (no cross-dependency; 25thCRX deps stay CRX-AI + `58D0FB32...`). So COPIED GME's placeable *pattern* into DCO with DCO-owned resources (not GME's). Key realisation: `DCO_TaskZoneComponent` self-runs on spawn, so DCO needs NONE of GME's module/placing/InitAction framework — just a placeable editable-SYSTEM prefab + a `SCR_PlaceableEntitiesRegistry` conf (which the editor auto-discovers; GME's Systems.conf is referenced nowhere).
- **Prefab** `Prefabs/E_DCO_TaskZone.et` (GUID `{841779BF689F6FB5}`, via MCP `prefab_create` then hand-authored body): `SCR_EditableSystemComponent` (m_EntityType SYSTEM, UIInfo "DCO QRF Zone" + `ENTITYTYPE_SYSTEM` label + SYSTEMS budget, m_Flags 67) + `RplComponent` + `SCR_EditableEntityVisibilityChildComponent` + `DCO_TaskZoneComponent {m_eRole QRF; m_fRadius 50}` + Hierarchy. `wb_resources getInfo` = **status OK** (parses correctly). Mirrors GME's `E_GME_Modules_Base.et`. Icons referenced are BASE-GAME GUIDs (not GME's) to keep mods independent.
- **Registry** `Configs/Editor/PlaceableEntities/Systems/DCO_TaskZones.conf` (GUID `{D1D2A960...}`): `SCR_PlaceableEntitiesRegistry { m_Prefabs +{ "{841779BF...}..." } }` (GME's format; the MCP `config_create` emitted an older `m_aEntries`/`SCR_PlaceableEntity` schema, replaced).
- **Resource-DB lesson (reconfirms [[resource-registration]]):** `wb_resources register` returns false / only writes the `.meta`; a `rebuild` is required to get the GUID into the `.rdb`. After rebuild the PREFAB GUID resolved (null-GUID errors cleared). The registry CONF still logged one `Wrong GUID {0000...}` self-ref (DB index not fully propagated) — needs a full resource rebuild / **Workbench restart** to finalise (then "DCO QRF Zone" appears under the GM Systems placement category). Could not verify in-GM appearance headless.
- Scripts compile clean (`CRC32: 54a929ac`).
- **STILL TODO:** finalise the conf in the DB (restart/full rebuild); then add role variants (Ambush / Ambush-Trigger / Defend / Reinforce) + GM radius/tag attributes; DEEPEN behaviours (QRF garrison hold + "critical" trigger + return; ambush all-guns). See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-02 — DCO content-browser FILTER (own category in the GM Systems tab)
Bryce: are they sortable / accessible, or just dumped in the Systems tab? Answer: GM top-level tabs = entity TYPE (can't add a new top tab cleanly); the GME way is a custom FILTER category inside Systems. Mirrored it for DCO:
- **Modded enums** `Scripts/Game/DCO/Roles/DCO_EditableEntityLabel.c`: `modded enum EEditableEntityLabel { DCO_TASKZONE_QRF=7210, _AMBUSH, _TRIGGER, _DEFEND, _REINFORCE }` (72xx avoids base + GME's 70xx) + `modded enum EEditableEntityLabelGroup { DCO_TASKZONE }`.
- **Core config** `Configs/Core/EditableEntityCore.conf` (GUID `{DE17AC501F0CD5ED}`): a `SCR_EditableEntityCore` defining the group "DCO Task Zones" (`m_ConditionalLabel ENTITYTYPE_SYSTEM` → only shows under Systems) + a label per role (literal display names, no localization file needed). Mirrors GME's `Configs/Core/EditableEntityCore.conf` (which is AUTO-MERGED across addons — referenced nowhere). Embedded GUIDs in DCO's `5DC0DC30` block.
- **Prefab** `E_DCO_TaskZone.et` now carries `DCO_TASKZONE_QRF` in `m_aAuthoredLabels` alongside `ENTITYTYPE_SYSTEM`.
- Compiles clean (`CRC32: d26cf520`, +1 file). Core conf registered; no enum/GUID/class errors after rebuild (only benign runtime "Build failed" lines, same as the prefab which still `getInfo`=OK). Full DB propagation + the filter actually appearing still needs a Workbench restart to confirm (can't verify the GM browser headless). See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-02 — All DCO Task Zone placeables (full set) + placement-model rationale
Expanded the placeable system to every per-group DCO ROLE (the only behaviours that suit a placeable; suppression/morale/flanking/etc are automatic or global). All are PERSISTENT + MULTI-PLACEMENT (standing assignments, live until deleted) — genuine one-time command modules are a separate future category (flagged, not built).
- 5 placeable prefabs (each editable SYSTEM, RplComponent, DCO_TaskZoneComponent baked role + radius, tagged with its `DCO_TASKZONE_*` filter label; all `getInfo`=OK):
  - `E_DCO_TaskZone.et` QRF Zone (`{841779BF689F6FB5}`, r50)
  - `E_DCO_TaskZone_Defend.et` Defend Zone (`{1D28FFA184C2D8EC}`, r50)
  - `E_DCO_TaskZone_Reinforce.et` Reinforce Pool (`{46124C5C81890FC2}`, r50)
  - `E_DCO_TaskZone_Ambush.et` Ambush Position (`{A524C110BDE25731}`, r30)
  - `E_DCO_TaskZone_AmbushKillZone.et` Ambush Kill-Zone / trigger (`{43C3E316165CD677}`, r30)
- All 5 in the registry `Configs/Editor/PlaceableEntities/Systems/DCO_TaskZones.conf`; after the final registry rebuild NO Wrong-GUID/null errors (only benign runtime "Build failed"). All show under the **Systems tab → "DCO Task Zones" filter** with a per-role label checkbox.
- **Resource-DB ordering lesson:** when rebuilding a registry that references freshly-rebuilt prefabs, rebuild the registry LAST/AGAIN — a registry rebuilt <1s after a prefab sees that prefab's GUID as null (`{0000...}`); a second registry rebuild clears it.
- Scripts unchanged this step (compile `d26cf520`). Full DB propagation + in-GM appearance still confirmable only after a Workbench restart (can't see the GM browser headless).
- **NEXT (behaviour DEPTH, not placement):** QRF garrison hold + "critical" trigger (morale+numbers) + return-to-hold (hold pos already stored via DCO_SetQRFHoldPosition); ambush is already whole-group "all guns" on spring. See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-02 — Ambush pairing logic (numeric Pair ID) + GM field; one-position/many-kill-zones
Per Bryce's spec for foolproof ambush pairing. Compiles clean (`CRC32: edb99a39`).
- **Pair link = numeric `m_iPairId`** on `DCO_TaskZoneComponent` (NOT a string — base `SCR_BaseEditorAttributeVar` has no string type; GME had to mod it with `GME_CreateString`. A number is natively supported AND typo-proof, which is the "no bugs" goal). Set the same non-zero ID on a Position + its Kill-Zone(s).
- **Kill-zone logic rewritten** (`DCO_TickTrigger`/`DCO_GetPairedPositions`/`DCO_DeleteSiblingTriggers`): ID!=0 → springs every AMBUSH position with the SAME id (one position, many kill-zones); on trip, deletes the OTHER same-id kill-zones via `SCR_EntityHelper.DeleteEntityAndChildren` (collect-then-delete to not mutate the registry mid-iterate) and KEEPS the tripped one. ID==0 → springs the NEAREST ambush position and deletes nothing. `m_bDCO_Tripped` latch = fires once.
- **GM field:** `DCO_TaskZonePairIdEditorAttribute` (slider 0–50, DCO_Ambush category, GUID block `5DC0DC31`) reads/writes the component on the selected zone via CreateFloat/round-to-int. (First tried a string editbox → `CreateString`/`GetString` don't exist in base; switched to int.)
- **Radius = visual circle** — IN PROGRESS. Bryce supplied `SCR_CustomAreaMeshComponent` source: radius attr is `m_fRadius`, self-generates in EOnInit. Added it to the Kill-Zone prefab (`m_fRadius 30, m_fHeight 6, m_bFollowTerrain 1`); `getInfo`=OK. BLOCKER: `m_Material` defaults to "" (empty = renders nothing) and I can't get a base-game `.emat` GUID headless (asset_search index empty in this env; no sibling mod references an area material; must NOT fabricate a GUID). Need Bryce to either confirm a circle shows on placement (default material) OR provide an area-mesh material resource path/GUID; then set `m_Material` on all 5 prefabs. Area-mesh base props confirmed via getInfo: m_eShape, m_fHeight, m_vResolution, m_bFollowTerrain, m_bStretchMaterial, m_Material, m_bActiveEveryFrameOnInit, m_bHideInWorkbench, m_fRadius. See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-03 — Task Zone visual circle: reuse base waypoint disc (material dead-end resolved)
The white GM circle IS an area mesh. `SCR_CustomAreaMeshComponent` needs `m_Material` and HARD-CRASHES when empty (`GenerateAreaMesh` null `res`, confirmed by Bryce's VM exception). Could NOT obtain a base area-material GUID: asset_search index empty, game_read/game_browse can't read paks in this env, the material is baked in the base waypoint prefab (in-pak, not exposed in a duplicate), no loose `.emat` in project/GME. So switched approach (Bryce's "just use this"):
- **Removed** the crashing `SCR_CustomAreaMeshComponent` from the Kill-Zone prefab.
- **`DCO_TaskZoneComponent` now spawns the BASE-GAME defend-waypoint** (`{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et` — Bohemia's asset, NOT GME's, so no cross-dep) as a child visual via `DCO_SpawnVisual` (deferred 500 ms so placement transform is set), `SetCompletionRadius(m_fRadius)` + `GenerateAreaMesh()`. The waypoint's own `SCR_WaypointAreaMeshComponent` supplies the material → disc renders, no material needed from us, no crash. Visual follows the zone (origin synced in tick) and is deleted with the zone. Compiles clean (`CRC32: 17835adf`).
- ⚠ NOT runtime-verified (can't see GM render headless): need Bryce to place a zone and confirm the disc shows sized to radius. Known risks to watch: a standalone (group-less) defend-waypoint may log/behave oddly; the disc is the waypoint's colour (same for all roles); the spawned waypoint may be separately selectable in GM (may want to hide/flag it non-editable next). See [[group-role-presets-and-waypoint-deconfliction]].

## 2026-06-03 — Pre-publish review fixes (#2 visual hardening, #3 orphan, #4 QRF depth)
Compiles clean (`CRC32: f718844b`); `mod_validate` passes structure/gproj/prefabs/configs/references.
- **#2 spawn-visual hardened** (`DCO_TaskZone.c`): the circle (spawned base defend-waypoint) is now PARENTED to the zone via `GenericEntity.AddChild(child, -1)`. So it follows the zone, is auto-deleted with the zone (removed the manual `DeleteEntityAndChildren`), and isn't independently selectable/deletable in GM — eliminating the dangling-reference crash (also removed the per-tick `SetOrigin`; m_VisualEntity is never poked after spawn).
- **#3 orphan removed:** deleted `Prefabs/AI/Waypoints/AIWaypoint_Defend_Large_CO.et` (+meta). ⚠ Filesystem-delete left a STALE .rdb entry (`Wrong GUID … inherited-name` on load) — harmless (nothing references it; `references` check passes) and clears on the next Workbench rescan/restart. CLEAN BEFORE PUBLISH: restart Workbench so the DB drops the missing-file entry.
- **#4 QRF depth** (`DCO_GroupQRF.c`): (a) `DCO_NeedsQRFSupport` now morale-gated — a QRF reacts to a friendly in CONTACT only once that friendly's `m_fDCO_Morale <= m_fQRFCriticalMorale` (broken/routing always triggers); so QRFs hold during routine contact and commit only when critical. (b) Garrison return: when nobody needs help and the QRF has a hold point (set by a QRF Task Zone) and has drifted > `m_fQRFHoldLeash`, it walks back to the meeting point (`DCO_VehicleUtil.OrderGroupMoveToPosition`). New settings `m_fQRFCriticalMorale` (40) + `m_fQRFHoldLeash` (30), JSON-wired + example.
- **Review notes (not blockers):** EnfusionMCP `Scripts/WorkbenchGame/EnfusionMCP/EMCP_*.c` (19) ship but are editor-only (won't run for users) — optional to remove. The whole Task Zone placeable system (placement, circle render + GM-only, behaviours, the new parenting) is STILL UNVERIFIED IN A LIVE GM — that's the one pre-publish smoke test only Bryce can do (needs the Workbench restart anyway). See [[group-role-presets-and-waypoint-deconfliction]].

## Changes outside plan
- (none yet)
