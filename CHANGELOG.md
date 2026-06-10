# 25th Direct Combat Overhaul (DCO) — v1.2.5

A full AI behaviour overhaul for Arma Reforger. AI hold cover, manoeuvre, break under pressure, surrender, reinforce each other, crew vehicles with doctrine, and clear buildings. Every system is controlled live from the "25th DCO" Game Master tab, ships OFF by default so you enable only what you want, and can be preset in a config file for dedicated servers.

## Morale and Surrender
- Groups lose morale under fire and casualties: at low morale they break and flee, at critical morale they surrender (drop weapons, hands up, go prone).
- Surrendered prisoners run from the nearest captor and lie down when cornered, and can recover, re-arm, and rejoin the fight if left alone.
- Optional fake surrender: some units keep a grenade and spring an ambush when a player closes in.
- Panic, leader-loss shock, morale contagion that spreads to nearby friendlies, and a shared morale pool so small teams near a strong force don't fold early.

## Combat
- Morale affects accuracy and fire rate: shaken and suppressed troops shoot worse.
- Aim zeroing: AI start cold on a fresh target and tighten up the longer they hold fire on it.
- Real suppression: pinned troops keep their heads down, seek cover, pop smoke, and dig in to hold ground.
- Hit flinch, friendly-fire avoidance, and weapon-aware standoff so AI stop suicide-charging to knife range.

## Movement and Tactics
- Cover-aware routing, flanking approaches, traveling and bounding overwatch, and react-to-contact actions chosen by a tactical brain (assault a weak flank, support by fire, or break contact).
- Cover-fitting stances, anti-jitter stance control, and adaptive formations.

## Squad Tactics
- CQB: AI push into buildings to clear enemies inside, plus methodical room-by-room town clearing that splits buildings between squads.
- Building garrison, defensive holds, ambushes, machine-gunner emplacement, static weapon and mortar use, and AI artillery fire missions.
- Close-quarters melee when an enemy is right on top of them.

## Coordination
- Reinforcement and Quick Reaction Force responses, plus vocal and radio contact sharing.
- AI Commander: a battlefield coordinator that finds the hottest fights and your placed objectives and commits reserves to them.
- Map intelligence: the Commander recognises towns and key locations and fights over them, favouring high ground.
- Optional external/LLM commander bridge for server owners.

## Vehicles
- Convoy doctrine: vehicles form sections on contact and run drive-through, herringbone, bounding, escort, and dismount drills.
- Armour angling to keep the front toward threats, hull-down repositioning, vehicle hijacking, and a safe-eject gate so crews don't bail from moving or airborne vehicles.

## Game Master Tools
- Placeable zones: QRF, Ambush, Ambush Kill-Zone, Defend, Reinforce, and Objective, each with its own labelled filter under "25th DCO" in the Systems tab.
- AI Commander Objective Zones: drop one and set its Type, Priority, and Radius to tell the Commander what to do there.
- Global AI base settings: skill, perception, reaction, fire rate, formation, and stance, with Conscript, Trained, Elite, and Fanatic presets.
- Per-group role presets so groups spawn pre-configured with no Game Master present.
- Full server config JSON for dedicated servers.

## Notes
- Everything defaults to OFF. Turn systems on from the "25th DCO" Game Master tab.
- Runs in Game Master, Conflict, and scenarios, including Game Master on dedicated servers.
