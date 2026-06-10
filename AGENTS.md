# Agent guide — 25thCRX (DCO Expansion)

## MANDATORY: verify every engine touchpoint with the Reforger MCP

This mod is built against the Arma Reforger / Enfusion script API. **Do not hand-wave engine APIs from
memory or training data.** Before writing or editing any script/config that calls an engine class, method,
enum, component, or attribute, you MUST confirm it exists and has the signature you expect using the
`enfusion-mcp` ("Reforger MCP") tools:

- `mcp__enfusion-mcp__api_search` — confirm class/method/enum/property names and signatures (e.g.
  `SCR_AICombatComponent.SetAISkill(EAISkill)`, `EAISkill` members, `SCR_EAIGroupFormation`,
  `FireModeManagerComponent`, `PerceptionComponent.SetPerceptionFactor`).
- `mcp__enfusion-mcp__game_browse` / `game_read` — read the base-game scripts/prefabs/configs to copy proven
  patterns (e.g. how stock editor attributes, categories, and `EditorModeEdit.et` registration work).
- `mcp__enfusion-mcp__wiki_search` — Enfusion/BIKI concepts and modding patterns.

**Rule of thumb:** if a feature depends on an engine hook, line it up with the MCP *first*, cite the verified
signature in the plan/spec/code comment, and only then implement. If the MCP cannot confirm a hook, treat the
feature as a research spike with an explicit cut criterion (see the VOIP-detection lever as an example).

This requirement applies to all DCO work, and explicitly to the **25th DCO BASE SETTINGS** feature
(`docs/superpowers/specs/2026-06-08-base-settings-tab-design.md`).

## Project invariants (do not violate)
- **Never** rebuild or repackage the mod (`mod_build` / `mod_create`). The GUID must stay fixed; the user
  publishes manually.
- Hand-authored `.conf`/`.et` files need a `.meta`. **Never** hand-author file/resource GUIDs — those are
  Workbench-only. A unique *embedded* `.conf` array id may be hand-added.
- Never delete a class that a `.conf` references.
- Any DCO Game-module compile error drops the **entire** "25th DCO" GM tab — keep every change compiling clean.
- Script + config edits compile-verify only on a **user Workbench reload**; treat that as an explicit
  checkpoint, not something the agent can self-verify.
