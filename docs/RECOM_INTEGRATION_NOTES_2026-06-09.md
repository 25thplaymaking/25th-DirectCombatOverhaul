# ReforgerCommander (RECOM) → DCO Integration — Morning Notes (2026-06-09)

Autonomous overnight work. Read this first.

## TL;DR
- Built an **External AI Commander bridge** in DCO that posts the battlefield to an external provider (the RECOM
  backend or an OpenAI-compatible proxy) and applies the AI's order into DCO's objective/commander system.
- It's **GM-compatible, server-only, default OFF**, and needs **no game mode** (lazy-starts from the group tick
  like `DCO_Commander`). RECOM's own "needs a placed `RECOM_Starter` entity" requirement is sidestepped.
- Updated RECOM's **deprecated 1.6 REST callbacks** (`override OnSuccess/OnError`) to the **1.7** API
  (`RestCallback.SetOnSuccess/SetOnError`).
- **No assets imported** (per your Iron Rule). Asset inventory + GUIDs to review are listed below.
- Two background review subagents ran (RECOM repo review = done; DCO bridge code review = check its result).
- **Reviews pending your compile.** A short CONFIRM-ON-COMPILE list is below (JSON parse method name etc.).

---

## 1. What RECOM actually is (from the repo review)
- **ReforgerCommanderBackend** (662 Java files): an external Spring server — map scan → MariaDB, DBSCAN
  clustering, terrain/forest/structure intelligence, and *planned* OpenAI commander. **Not portable to Enfusion**;
  it's the external brain. Run separately.
- **ReforgerCommanderClient** (159 `.c`): the Reforger mod — scans the world and talks to the backend over REST,
  wrapped in their own DI ("Facets") + ECS + FSM + logger framework. **Most of it we deliberately skip** (DCO has
  its own architecture). The valuable patterns: the `RestContext` wrapper, the `JsonApiStruct` DTOs, auth-with-
  expiry, and the long-poll order FSM.

**PORT / ADAPT / SKIP (summary):** PORT = the HTTP client + JsonApiStruct DTOs. ADAPT = auth, properties→JSON,
order receiver. SKIP = their Facet DI container, ECS, MessageBus framework, MapRenderer (author-flagged
"will_not_be_ported"), topography/road scanners, SLF4R logger. (Full table is in the review transcript.)

**Key correction:** RECOM attaches the bearer token **in the JSON body** (`Authorization` field), NOT an HTTP
header — because it talks to their Java backend. Our `DCO_RestClient` uses the **standard `Authorization: Bearer`
HTTP header** (correct for OpenAI-compatible endpoints). If you point DCO at the *RECOM backend* instead, switch
to body-auth (add an `Authorization` field to `DCO_ExternalStateDto` and set it) — noted in code.

---

## 2. What I built (DCO side) — files
All under `Scripts/Game/DCO/Commander/External/`:
- **`DCO_RestClient.c`** — thin wrapper over `GetGame().GetRestApi().GetContext(baseUrl)` → `RestContext.POST`,
  with `SetHeaders("Content-Type…\nAuthorization: Bearer <key>")`, modern `RestCallback.SetOnSuccess/SetOnError`,
  callback held as `ref` (engine deletes non-ref callbacks). `DCO_RestResponseHandler` is the result sink.
- **`DCO_ExternalDtos.c`** — `DCO_ExternalStateDto {mapName, faction, summary}` (sent) and
  `DCO_ExternalOrdersDto {objectiveType, x, z, radius, priority, faction, reasoning}` (received), both
  `JsonApiStruct` with `RegV(...)` per RECOM's proven pattern.
- **`DCO_ExternalCommander.c`** — singleton; `EnsureRunning()` lazy-starts a `CallLater` loop (server + enabled +
  endpoint set). `Tick()` builds a compact battlefield **summary** (own groups: faction/pos/contact/threat +
  current objectives), POSTs it with the API key, and `ApplyOrderJson()` parses the reply and registers it as the
  **main-effort GM objective** in `DCO_ObjectiveRegistry` — which the local `DCO_Commander` then services with
  reserves via the existing DCO move/defend/reinforce primitives. **This is the tie-together: external AI →
  objective → DCO behaviours.**

Wiring: `DCO_GroupQRF.c` calls `DCO_ExternalCommander.EnsureRunning()` next to `DCO_Commander.EnsureRunning()`.
Settings in `DCO_MoraleSettings.c`; JSON in `DCO_JsonConfig.c`.

---

## 3. How to configure & use it
All via the **server profile JSON** (`$profile:DCO_Settings.json`) — the API key is **never** in the repo:
```json
"m_bEnableExternalCommander": true,
"external_endpoint": "http://127.0.0.1:8080",      // base URL of your provider/proxy
"external_eval_path": "/commander/evaluate",        // POST path returning the order JSON
"external_api_key": "sk-....",                       // bearer/API key — fill this in on YOUR server only
"external_faction": "USSR",                          // faction key the external AI commands
"external_interval_sec": 20.0
```
- **Request** DCO sends (POST): `{"mapName":"","faction":"USSR","summary":"...tactical digest..."}`.
- **Response** your endpoint must return: `{"objectiveType":"SUPPORT","x":1234.5,"z":6789.0,"radius":150,"priority":80,"faction":"USSR","reasoning":"..."}`.
- For **OpenAI**: stand up a thin proxy that takes our state, prompts the model, and returns that JSON (exactly
  RECOM's "Java backend between the game and OpenAI" architecture — keeps fragile LLM-JSON parsing OUT of Enfusion).
- There's a GM checkbox planned for the *enable* toggle (not the key); see Remaining Work.

---

## 4. Verification status — bridge REVIEWED CLEAN (no compile blockers expected)
The background code-review agent MCP-confirmed ALL of these (no fixes needed):
- `JsonApiStruct.RegV/Pack/AsString/`**`ExpandFromRAW`** — `ExpandFromRAW(string)` IS the correct response parse.
- `GetGame().GetRestApi()`→`RestApi`; `RestApi.GetContext`→`RestContext`; `RestContext.POST(RestCallback,string,string)`,
  `SetHeaders(string)`; `RestCallback.SetOnSuccess/SetOnError/GetData/GetHttpCode` (`HttpCode` = int typedef).
- `SCR_FactionManager.GetFactionByKey` + `Faction.GetFactionKey`; all DCO types (`DCO_Objective` ctor, registry,
  settings) match exactly. Callback held-ref lifetime verified safe; API key JSON-only + server-gated. **APPROVED.**
- Only residual (functional, not compile): the external backend's JSON must use field names
  `objectiveType/x/z/radius/priority/faction/reasoning` so `ExpandFromRAW` populates the order. That's your
  endpoint's contract, documented in §3.

---

## 5. ASSETS — DO NOT IMPORT until you review (per your Iron Rule)
No obfuscation or embedded secrets were found in RECOM (only `localhost:8080` placeholders in code + GitHub/Discord
links in README). Non-script assets you'd review before any import (we imported NONE):
- `worlds/Everon/Everon.ent` (SubScene stub → vanilla Eden), `.../RefCom.layer` (places one `RECOM_Starter`).
- `worlds/Everon_CTI/RefCom_CTI_Campaign_Eden.ent` (GUID `B32EB02C5303B6C7`), `.../RECOM.layer`.
- `worlds/Arland_CTI/Arland_CTI.ent` + `default.layer`.
- `Configs/Map/MapFullscreen.conf` (registers `RECOM_MapModule` GUID `{5DA495EAC2FA1487}` — MapRenderer, skip).
- `UI/layouts/Map/CanvasLayer.layout` + `DEMO_CanvasLayer.layout` (MapRenderer canvases — skip).
- `RECOMClient.gproj` (GUID `5D68C6A8E8235705`), `resourceDatabase.rdb`, `logo2.png`/`recom.png`/`thumbnail.png`.
**GUIDs to NOT import (collision risk):** `5D68C6A8E8235705`, `5DA495EAC2FA1487`, `B32EB02C5303B6C7`. We
hand-authored none and copied no `.meta`s.

---

## 6. Remaining work
1. **GM Objective placeable — SCRIPTS DONE** (`Scripts/Game/DCO/Commander/DCO_ObjectiveZone.c` +
   `DCO_ObjectiveEditorAttributes.c`, mirror the proven `DCO_TaskZone` lifecycle; MCP-verified). **Your Workbench
   steps:** (a) duplicate the Task Zone prefab → `DCO_ObjectiveZone.et`, **remove `m_EntityInteraction`**, swap the
   component to `DCO_ObjectiveZoneComponent`; (b) add a placeable-registry entry so it shows in the GM browser
   (Systems); (c) add 3 attribute entries to an attribute list — Type (ButtonBox_Selection, m_aValues DEFEND0..
   RESERVE5), Priority (Slider 0–100), Radius (Slider 25–500). **CAVEAT:** a placed objective is only serviced by
   the commander if it has a FACTION — most markers won't carry one, so either add a `FactionAffiliationComponent`
   to the prefab (set the side in WB) or we add a faction GM-attribute next. Until then the AUTO objectives + the
   external-AI main-effort objective already drive the commander; this placeable adds hand-placed draggable ones.
2. **GM enable toggle** for `m_bEnableExternalCommander` + `m_bEnableCommander` (checkbox attributes) so you can flip
   the commanders on without editing JSON. (The viz toggle `cmd_visualize` is already wired.)
3. **Memory + the AI-Commander plan's Task 6.**
4. **Refactor pass — DONE.** Scanned all RUNTIME-UNKNOWN/experimental/first-cut markers. The codebase is largely
   sound; most flags are *in-engine-validation* behaviors (default-OFF + guarded), not defects. Made **3 safe
   hardening fixes** (additive `if (!world)` guards before chained `GetWorld().GetWorldTime()`):
   `Util/DCO_StanceUtil.c` (TrySetStance — the shared stance choke point), `Movement/DCO_StanceCooldown.c`
   (ApplyNewRequest hot path), `Morale/DCO_GroupMorale.c` (surrender flee timing). No compile risk; no
   scope/default changes. PerceptiveNav:109 reconciled (correct as-is).

   **In-engine-validation list (behaviors to confirm during playtest — all guarded/default-OFF, not bugs):**
   EmergencyRearm resupply source; VehicleCombat/VehicleHijack move-seat/boarding; CQB assault push +
   Garrison "occupy"; Defend `SCR_AIMessage_Defend` arc-units(radians?)/priority; Formation host-entity +
   formation name strings (don't run DCO formation shapes alongside cover-seek); Melee/HitFlinch server-triggered
   actuation; Suppression `GetSuppressionMeasure` range → tune threshold; CoverStance reposition-rooting → tune
   interval; TacticalPath per-member RequestBroadcast isolation; PerceptiveNav off-mesh `SetMovementDirWorld`
   spike; CompositionRegistry root `FactionAffiliationComponent`. Plus the HVT timestamp ms/s + VisionLimit
   hour-units from the earlier audit.

---

## 7. Architecture recap (how it all ties together for GM)
`External provider (OpenAI/RECOM)` ⇄ `DCO_RestClient` ⇄ `DCO_ExternalCommander` → registers a main-effort
`DCO_Objective` → `DCO_ObjectiveRegistry` (also auto-derives base-defense + hot-fight objectives) →
`DCO_Commander` scores/ranks/assigns reserves → orders execute through existing DCO behaviours
(`DCO_SetDefender`, `DCO_SetReinforcementEligible`, vehicle-aware moves). All server-side, all GM-usable, all
gated behind toggles that default OFF. Enable `m_bEnableCommander` (local brain) and/or
`m_bEnableExternalCommander` (LLM brain) — they compose.
