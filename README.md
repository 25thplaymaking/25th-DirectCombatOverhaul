# 25th Direct Combat Overhaul (DCO)

**A server-side AI behaviour overhaul for Arma Reforger, built for Game Master and PVE.**

DCO makes AI fight like people instead of bullet sponges: they break, surrender, suppress,
dig in, clear buildings room by room, move in bounds, crew vehicles with doctrine, and : when
you switch it on : take coordinated commander-level action across the map. It runs entirely on
the server, layers on top of the vanilla AI, and is driven by a live Game Master tab plus an
optional server JSON file. Nearly everything ships **off by default**, so you opt in to exactly
the behaviour you want.

> Addon: `25thCRX` · GUID `697A1949E09F26D2` · License: MIT · Requires: **Arma Reforger**

---

## What it does

DCO is organised as ~40 self-contained behaviour modules (one folder per system under
`Scripts/Game/DCO/`). Highlights:

### Morale & psychology
- **Group morale** model with casualty/leader-loss shock, recovery and rally thresholds.
- **Surrender** : outnumbered, broken groups lay down arms, drop gear and freeze in pose; optional **last stand** and **recovery/re-arm**.
- **Fake surrender** : a chance for a "surrendering" enemy to pull a grenade.
- **Suppression** : reads the engine threat system, drops heads down, seeks cover or digs in, pops smoke when pinned.

### Tactical movement
- **Movement techniques**, **bounding overwatch**, **formations** and cover-aware repositioning.
- **Reaction to contact** + **hit flinch** so being shot at actually disrupts AI.
- **Standoff** : weapon-aware engagement ranges; AI stop suicide-charging across open ground (but still fight at point-blank).
- **Melee** : commit / break / return-to-shoot logic.

### Urban & area combat
- **CQB town clearing** : methodical building-by-building, house-to-house sweeps.
- **Garrison** and **composition defense** : own-faction compositions trigger defend/utilise behaviour with a morale floor.
- **Ambush** roles.

### Vehicles & heavy weapons
- **Convoy / vehicle doctrine** : combat-only takeover with drive-through, herringbone, bounding, dismount and disabled-vehicle drills (navmesh-safe).
- **Armor** combat, **AT infantry** engagement, **vehicle hijack**, **safe eject**.

### Fire support & assets
- **Artillery** support, **machine-gun emplacement** use, smart **asset / turret use**, **illumination** & vision limits.

### Command layer
- **AI Commander** : when enabled, both factions react to the battlefield: assist units in contact, defend or take key locations.
- **Objectives** system with reactive HOLD/ATTACK orders.
- **RECOM map intelligence** : village/military/forest area clustering (in-engine grid scan or a baked `DCO_MapIntel.json`), convex/concave hulls and topography, biasing objectives toward key terrain.
- **External (bring-your-own-model) AI Commander** : post the battlefield to an LLM and apply its orders. Multi-provider: **OpenAI-compatible**, **Anthropic**, or **OpenRouter** (which routes virtually any model). See `docs/RECOM_INTEGRATION_NOTES_2026-06-09.md`.
- **QRF / reinforcement**, **radio & vocal sharing**, and **group role presets** (QRF / Ambush / Defend / Reinforcement baked at spawn via placeables).

### Targeting & global tuning
- **HVT tiered targeting** (heli > armour > motorised > infantry) with cross-group target distribution.
- **Friendly-fire** avoidance, **emergency rearm**, **idle** behaviours.
- **Base settings tab** : global skill / perception / reaction / fire-rate / formation / stance, plus conscript → fanatic grade presets.

---

## Requirements

- **Arma Reforger** (the addon depends on the base game data project).
- For building from source: **Arma Reforger Tools / Workbench** (this repo is addon *source* : Workbench regenerates the per-resource `.meta` on first load).

## Installation (servers)

1. Subscribe to / load the published mod, **or** clone this repo and publish yourself.
2. Add your settings file (optional) : see below. Without one, baked defaults apply.

## Configuration

There are two surfaces, by design:

| Surface | Scope | How |
|---------|-------|-----|
| **`25th DCO` Game Master tab** | Live, in-session overrides | In-game, as Game Master |
| **`$profile:DCO_Settings.json`** | Server startup baseline | Copy `Configs/DCO_Settings.example.json` to your server profile dir as `DCO_Settings.json` |

Every JSON key is **optional** : delete any you don't want to override and the baked default is
kept. The file is loaded **once at startup**; the GM tab still overrides live during a session.

The map-intelligence / external-commander settings are **JSON-only** (only the AI Commander
on/off switch is exposed to the GM) to avoid unexpected mid-session side effects. See
`Configs/DCO_MapIntel.example.json`.

---

## For modders

DCO is a clean reference for non-destructive Enfusion AI work:

- **One folder per behaviour** under `Scripts/Game/DCO/` (89 scripts). Each is independent and individually gated.
- Built almost entirely from **`modded class` fragments** over engine AI components (`SCR_AIGroupUtilityComponent`, `SCR_AICombatComponent`, `SCR_CharacterControllerComponent`, the threat system, etc.) : no forked vanilla files.
- **Shared utilities** live in `Util/` (e.g. `DCO_PlayerUtil`, `DCO_StanceUtil`, `DCO_VehicleUtil`) so any fragment can call them regardless of folder load order.
- **Server-side only** : every behaviour guards on `Replication.IsServer()` and never drives player-controlled characters (`DCO_PlayerUtil.IsPlayer`).
- Settings flow through `Morale/DCO_MoraleSettings` + `Config/DCO_JsonConfig`, mirrored to GM editor attributes in `Configs/Editor/AttributeLists/DCO_Attributes.conf`.

To extend: drop a new folder under `DCO/`, add a `modded class` fragment, gate it behind a
settings flag, and (optionally) surface that flag in the GM attribute list. Open `addon.gproj`
in Workbench to compile.

---

## Credits & license

- Created by **BISHOP / 25thplaymaking**, with substantial pair-programming by **Claude**.
- Incorporates and finishes ideas from the **RECOM** AI-commander work, retagged and completed under DCO; shoutout to the original owner in the ALIVE Discord.
- Released under the **MIT License** : see [`LICENSE`](LICENSE).
