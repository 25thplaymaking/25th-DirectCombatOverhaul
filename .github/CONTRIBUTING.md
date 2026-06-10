# Contributing to 25th Direct Combat Overhaul (DCO)

Thanks for your interest in improving DCO, an AI behaviour overhaul for Arma Reforger.

## How to contribute
1. Fork the repository.
2. Create a branch for your change (for example `feature/...` or `fix/...`).
3. Make your change in the Arma Reforger Workbench and confirm it compiles with no script errors.
4. Open a pull request against `main` and fill in the PR template.

All pull requests require review before they can be merged. Direct pushes to `main` are reserved for the maintainer.

## Working with the source
- The repository holds the human-authored source only: scripts (`.c`), configs (`.conf`), prefabs (`.et`), layouts, docs, and the `.gproj`.
- `.meta` files, the resource database, and built textures are intentionally NOT committed. The Workbench regenerates them when you import the addon, so open it once in the Workbench after cloning to rebuild them.
- The GUIDs that matter live inside the `.conf`/`.et` source and are committed. Do not hand-edit resource or file GUIDs; let the Workbench manage them.

## Guidelines
- Keep new systems OFF by default and expose them through the "25th DCO" Game Master settings (and, where useful, the server config JSON).
- Never commit secrets or API keys. The external AI-commander endpoint and key are read from the server profile JSON only.
- Match the existing code style and comment density.

## Reporting bugs and requests
Use the issue templates. Include your DCO version, game mode, other mods loaded, and which DCO settings were enabled.
