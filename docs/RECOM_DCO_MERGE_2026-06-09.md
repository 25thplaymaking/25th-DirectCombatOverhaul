# RECOM → DCO Map-Intelligence Merge (2026-06-09)

Merges ReforgerCommander's map-area intelligence into DCO's AI commander, **fully retagged
under DCO** (no `RCM_`/`RECOM_` symbols). The commander now fights over the map's **key
locations** — towns and military sites — not only the bases the GM places.

Branch: `feature/dco-map-intel`. New code under `Scripts/Game/DCO/Commander/Area/`.
**Default OFF.** Needs a Workbench compile + a GM-attribute registration step (below).

---

## The idea

Turn **AI Commander** on and it acts for **both factions**, continuously, reacting to what
players do: units in contact get supported, held key locations get reinforced, contested
towns get fought over. The GM (player) places the compositions; the commander generates the
moves when conflicts start. For PVE this means every problem the players create gets an
automatic, sensible reaction — no scripted triggers.

This was already half-built: `DCO_Commander` scores objectives and vectors reserves; it
derived objectives from **compositions** (defend your bases) and **hot fights** (support
contact). What it lacked was awareness of the **map's own key locations**. RECOM computed
exactly that (village/military/forest areas) but **never fed it back into gameplay** (its
client is send-only; the area-consumer was never built). This merge supplies the missing
layer and the consumer.

---

## Architecture

```
DCO_MapAreaRegistry (built once, cached)
   ├── EXTERNAL tier  -> DCO_AreaSourceExternal  -> $profile:DCO_MapIntel.json (baked, incl. forests)
   └── NATIVE tier    -> in-engine grid-DBSCAN    -> villages + military sites (no file, no backend)
            |
            v
DCO_ObjectiveRegistry.DCO_DeriveAreaObjectives()
   - significant area, no composition owner, units present:
       1 faction present  -> HOLD  (dig in, reinforce the key location)
       2+ factions present -> ATTACK per faction (commit reserves to take it)
            |
            v
DCO_Commander  -> scores + vectors nearest reserves (existing pipeline)
```

`m_iCommanderAreaMode`: **0 AUTO** (baked file if present, else native scan), **1 EXTERNAL-only**,
**2 NATIVE-only**, **3 OFF**. Areas are static terrain, so the registry builds **once per
session** and caches. The native scan is spread across frames (a slice of grid cells per
tick) so it never hitches the server.

---

## What was ported, retagged, and finished

| RECOM feature | State in RECOM | In DCO |
|---|---|---|
| Village clustering | working (convex hull) | `DCO_MapAreaRegistry` native grid-DBSCAN of `SCR_DestructibleBuildingComponent` density. Retagged. |
| **Military areas** | **disabled** | **Finished**: tagged from the map's `SCR_MilitaryBaseMapDescriptorComponent`. |
| Cluster hull | convex working / concave disabled | `DCO_AreaHull` (monotone-chain convex hull) finished for viz/bake. Commander consumes the simpler centroid+radius, so concave is fidelity-only. |
| Forest areas | disabled (needs concave hull, heavy scan) | **External-tier only.** Vegetation can't be scanned cheaply at runtime, so forests are produced offline and read from the baked file (`FOREST` type). |
| Area → behaviour | **never built (either side)** | **Net-new** in `DCO_DeriveAreaObjectives` — the whole point. |

The MariaDB question is moot for the shipped mod: the **native tier needs no database, no
backend, no keys**. The external tier reads a flat JSON file. (RECOM's backend even defaults
to embedded H2, not MariaDB — MariaDB was only its dev profile.)

---

## Setup

### Native tier (recommended, zero setup)
1. Compile-verify (below).
2. Enable **AI Commander** (`m_bEnableCommander` — GM tab or JSON) and, in
   `$profile:DCO_Settings.json` only, **Commander Areas** (`m_bEnableCommanderAreas`). Leave
   `m_iCommanderAreaMode = 0`.
3. Place faction compositions as usual. The commander does the rest.

Tuning keys (JSON): `m_fAreaCellSize` (scan grid, default 150 m), `m_iAreaMinBuildings`
(built-up cell threshold, 6), `m_iAreaSignificantWeight` (min buildings for a village to earn
an objective, 15), `m_fAreaMaxRadius` (400), `m_iAreaCellsPerStep` (scan cells/frame, 64 —
lower if the one-time scan ever hitches).

### External tier (adds forests / hand-tuned areas)
1. Produce `DCO_MapIntel.json` (next section) and copy it to your server's `$profile/`
   directory (next to `DCO_Settings.json`). Schema reference: `Configs/DCO_MapIntel.example.json`.
2. Set `m_iCommanderAreaMode = 0` (AUTO uses the file when present) or `1` (file required).

### Producing the baked file (Tier-A data)
The file is **static per map** — bake once, ship it. Three ways:
- **Hand-author** for a few key locations (fastest; copy the example and edit the parallel
  arrays). Good enough to drive the commander.
- **From RECOM's backend** (if you want full village/forest coverage): run it headless on
  **H2** (no MariaDB — its default `application.properties` already uses H2), let the RECOM
  client scan the map once, then `POST /api/v1/map/clusters` and convert the
  `ClusterResponseDto` (cluster centroids + hull → centroid+radius) into the DCO parallel-array
  schema. To include **forests/military**, enable them in `ClusteringService` (RECOM ships
  them commented out) and point the military resource list at military prefabs.
- **DCO native dump** (optional future tool): run the native scan and serialise its areas to
  the same schema for later EXTERNAL reuse.

---

## You must do, in Workbench
1. **Compile-verify.** Reload scripts; confirm **0 DCO errors** and note the Game CRC.
   Highest-attention new code: `DCO_MapAreaRegistry` (world query + async scan),
   `DCO_AreaSourceExternal` (JsonApiStruct file parse). All engine calls were MCP-verified
   (`QueryEntitiesByAABB`, `GetWorldBounds`, `SCR_DestructibleBuildingComponent`,
   `SCR_MilitaryBaseMapDescriptorComponent`, `SCR_FileIOHelper.GetFileStringContent`,
   `JsonApiStruct.ExpandFromRAW`) but only a real compile confirms.
2. **Nothing to register in GM.** The map-intelligence + provider settings are **JSON-only by
   design** — they apply at scan/startup, so exposing them live in GM would do nothing (areas are
   already cached) or mislead. The only commander control in GM is the existing **AI Commander**
   switch. Everything else is set in `$profile:DCO_Settings.json`.

## Validate in playtest (all default-OFF, not bugs)
- Native scan: turn on `m_bDebug`, watch `[DCO:AREA]` for the cell grid + area count. If 0
  areas on a built-up map, lower `m_iAreaMinBuildings` / `m_iAreaSignificantWeight`, or the
  building component differs on that map (tune the detector in `DCO_OnScanEntity`).
- Military tagging depends on the map carrying `SCR_MilitaryBaseMapDescriptorComponent`
  (Conflict/Campaign worlds do; blank GM worlds may not — those areas fall back to VILLAGE).
- Objective spam: if the commander over-commits to towns, raise `m_iAreaSignificantWeight` or
  lower per-objective reserves (`m_iCmdGroupsPerObjective`).

## External AI Commander — bring-your-own model (multi-provider)

The external commander now talks **directly** to the common LLM APIs (not only a custom
proxy), via `DCO_ExternalProvider`. `external_provider`: **0 RAW** (proxy / RECOM backend),
**1 OPENAI-compatible**, **2 ANTHROPIC**. All of OpenAI/DeepSeek/OpenRouter/Groq/local share
the OpenAI shape; OpenRouter routes to *every* model (Claude, GPT, DeepSeek, Gemini, Llama)
through one endpoint, so it's the simplest "all models" option. Keys live in
`$profile:DCO_Settings.json` only.

| Provider | `external_provider` | `external_endpoint` | `external_eval_path` | `external_model` (example) | auth |
|---|---|---|---|---|---|
| OpenAI (ChatGPT) | 1 | `https://api.openai.com` | `/v1/chat/completions` | `gpt-4o` | Bearer |
| **OpenRouter** (any model) | 1 | `https://openrouter.ai` | `/api/v1/chat/completions` | `anthropic/claude-3.5-sonnet`, `openai/gpt-4o`, `deepseek/deepseek-chat`, `google/gemini-flash-1.5`, … | Bearer |
| DeepSeek | 1 | `https://api.deepseek.com` | `/v1/chat/completions` | `deepseek-chat` | Bearer |
| Groq | 1 | `https://api.groq.com/openai` | `/v1/chat/completions` | `llama-3.3-70b-versatile` | Bearer |
| Local (LM Studio / Ollama) | 1 | `http://127.0.0.1:1234` (Ollama `:11434`) | `/v1/chat/completions` | your local model | none/Bearer |
| Anthropic (Claude, direct) | 2 | `https://api.anthropic.com` | `/v1/messages` | `claude-opus-4-8` (cheaper: `claude-sonnet-4-6`) | x-api-key |
| Custom proxy / RECOM backend | 0 | your URL | your path | — | Bearer (optional) |

JSON keys: `external_provider`, `external_model`, `external_max_tokens` (default 1024),
`external_system_prompt` (empty = baked commander instruction), plus the existing
`external_endpoint` / `external_eval_path` / `external_api_key` / `external_faction` /
`external_interval_sec`. The model is told to return **only** the order JSON
`{objectiveType,x,z,radius,priority,faction,reasoning}`; the reply is unwrapped per provider
(`choices[0].message.content` / `content[0].text`) and parsed into the objective. A bad/empty
reply is dropped — the commander keeps running on its native objectives. **Tip:** for a tick
every ~20 s, a cheaper/faster model (sonnet, gpt-4o-mini, deepseek-chat) is the economical
choice; raise `external_interval_sec` to cut spend further.

Requests are built and responses parsed with `JsonApiStruct` (no fragile string handling) —
the same nested-DTO mechanism RECOM's own DTOs use. Anthropic raw-HTTP shape verified against
the Claude API reference (`x-api-key` + `anthropic-version: 2023-06-01`, `/v1/messages`).

## Completed RECOM leftovers (the five gaps)

1. **Native forests** — optional prefab-name forest pass, clustered exactly like villages.
   `m_bAreaIncludeForest` (default OFF — heavier, and only works if trees are prefab entities),
   `m_sAreaForestPrefabs` (substring, default `Vegetation/Trees`), `m_iAreaMinForest`. Forests
   also still come from the baked file. Tune the substring per map with `m_bDebug`.
2. **Concave hull** — `DCO_ConcaveHull` (k-NN, hard-bounded, convex fallback) finishes RECOM's
   disabled concave/alpha-shape. Visualization-only; the commander uses centre+radius, so it
   can't affect gameplay.
3. **Topography / high ground** — `DCO_TerrainUtil` samples `GetSurfaceY` for an area's
   *prominence* (height above its surroundings); area objectives on high ground get a priority
   bonus (`m_bAreaHighGroundBias`, `m_fAreaHighGroundWeight`). RECOM scanned terrain but never
   wired it to decisions — this does.
4. **Prefab-name classification** — supplements the component/descriptor detection. A single
   substring per setting (`m_sAreaMilitaryPrefabs`, `m_sAreaForestPrefabs`) via
   `ResourceName.Contains` (no unproven `string.Split`). Closes the "military site without a
   descriptor" gap.
5. **Offline bake from native** — set `m_bDumpMapIntelOnLoad: true` (JSON) and the native scan
   writes its areas to `$profile:DCO_MapIntel.json` when it finishes. Turn it back off, hand-tune
   the file, and switch to the EXTERNAL tier — no RECOM/Java, and no GM button (kept out of GM on
   purpose).

Not done (out of scope): the **TacView replay/visualization** tool (`recom-commander`) — a
separate spectator product, not AI behaviour.

## MariaDB — what it actually is (investigated 2026-06-09)

MariaDB is **not required** by RECOM or by this integration. Evidence from the repo:
- The **only** MariaDB reference is one stale line in `README.md`, plus a `docker/trash/`
  folder (named *trash*) holding the old `mariadb-docker-setup-or-start` scripts and a
  `0_CREATE_DATABASE.sql`. Abandoned tooling.
- **No `mariadb-java-client` dependency in any `pom.xml`** — the backend can't connect to
  MariaDB without a driver that isn't present.
- Both `application.properties` and `application-local.properties` point at **embedded H2**
  (`jdbc:h2:file:…;AUTO_SERVER=TRUE`), so the backend + the TacView client share one H2 file.
- `recom-commander` is **"RECOM TacView"** (a desktop replay/visualization client,
  `web-application-type=none`), not a DB service or an LLM brain.

So MariaDB was RECOM's *original* backing store for the scanned-map DB, which they migrated
off to embedded H2 and moved to `trash/`. A relational DB exists at all only to cache the
expensive map scan for clustering — which the DCO **native tier replaces entirely** (no DB).
If you ever wanted production-grade concurrent persistence for an offline bake farm, swapping
H2 → MariaDB/Postgres is possible, but it's optional and not wired anywhere today.

## Status
- Native + external tiers, objective derivation, settings, JSON keys, GM toggle: **built**,
  retagged, committed on `feature/dco-map-intel`. **Pending a Workbench compile + the GM
  attribute conf entry.** No GUIDs/.meta hand-authored; new `.c` files get their `.meta` on
  the next Workbench load. Nothing rebuilt/repackaged.
