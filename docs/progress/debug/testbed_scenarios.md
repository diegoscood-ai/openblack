# Testbed scenarios

The ready-made scenarios of the testbed (about 200, in `src/Debug/TestbedScenarioRegistry.cpp` and the facet files
`src/Debug/TestbedScenario*.cpp`), grouped by what they show, and the parts of the game that have none yet. A row is
done when there are scenarios for that part and this tree's runner plays everything they ask for; partial when some of
what they ask for logs "not in this tree yet" and is skipped. No scenario has a reference run yet. The framework is in
[testbed.md](testbed.md); how to write one is in [SCENARIOS.md](../../refactor/SCENARIOS.md).

**Progress: 16/36 done, 11 partial — 60%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Idle: fidgeting, hanging around, wandering | done | `idle.*` |
| Faces, emotions, gestures and the emotes of its desires; eyes and blinking | done | `expressions.*` |
| What it looks at: watching a walker | done | `senses.*` |
| Needs: thirst, hunger, sleep by night and day, poo, puking, fainting, cold, exhaustion | done | `needs.*` |
| Growing up: a time-lapse, the stages unlocking desires, the morph lineup | done | `growth.*` |
| Looks: skins and hair by alignment, tattoos and wounds | done | `appearance.*` |
| Footprints per species, in snow, grass and shallows, the first of April smileys | done | `footprints.*` |
| Moving: walking, running and turning, routes round obstacles, round the lake and through small trees | done | `movement.*` |
| Objects: picking up, looking over, putting down, throwing, eating, knocking down trees, pointing | done | `objects.*` |
| The hand on it: stroking each part, slapping, the status panel, the hand's look by alignment | partial | `hand.stroke`, `hand.slap`, `hand.status_panel`, `hand.look_*`; the hand held to a creature (stroking, slapping and letting go by hand) logs "not in this tree yet" |
| The leashes: leading, tying, keeping home, the leash keys and picker, other players' creatures | partial | `leash.*`, `examples.leashed_creature`; shaking a leash off and the leash's gestures log "not in this tree yet" |
| Fights: by themselves, charged and quick blows, blocks, knock-outs, slapping other gods' creatures | partial | `combat.*` play in full; slapping another god's creature takes the hand held to it, which logs "not in this tree yet" |
| Learning: rewards and punishments, by watching, from mind files, copying the player | done | `mind.*`; a "chosen:" mind file is the game's file of that name |
| Creature Mode, following it with C, and the Creature Cave with its tattoos | partial | `creature_mode.*`; the double click and the held camera keys log "not in this tree yet" |
| Its voices by species and size, and the sounds of the land | done | `audio.*` |
| Its shadow, its reflection, light on the land through the day | done | `light.*` |

## Miracles and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every miracle cast, held, powered up and its effect, each in its own scenarios | partial | `miracles.*` are listed and set up, but the scenarios' miracle casts log "not in this tree yet", so they cast nothing; the debug Magic window casts any miracle (`src/Debug/Magic.cpp`, `MiraclesCaster.cpp`) |
| Dispensers, globes and seeds from worship | partial | the dispenser and hand-seed fixtures work (`src/Debug/TestbedFixtures.cpp`); the globe scenarios' own dispensers, seeds held or summoned and tribal power log "not in this tree yet" |
| The creature casting its spells | partial | `miracles.*` of the creature casting: the creature is taught and told to cast; a seed held in the hand and the hand held still are skipped |
| Fire spreading, blasts, water putting fire out | partial | the fire fixture lights objects or a bonfire; the blast and water scenarios' casts log "not in this tree yet" |
| Particles and sprites: sparkles, smoke, steam, mist, sorted sprites | partial | `particles.*`; an effect in a player's colour logs "not in this tree yet" |
| Gestures drawn through the recogniser | todo | the gesture scenarios are listed, but drawing a gesture logs "not in this tree yet"; the debug Gestures window draws and sends gestures (`src/Debug/Gestures.cpp`) |
| Hand navigation: dragging, edge turning, tilting, both buttons, middle button | partial | `hand.click`, `hand.drag`, `hand.edge_*` and the rest, driven by the scenario's pointer (`src/Debug/TestbedPointer.cpp`), whose buttons reach the hand only; the wheel and the camera's drags are dropped when the real input is ignored |
| Hand demos replayed with things set out where they press | done | `examples.miracle_near_village`, `examples.hand_throw_target` (`src/Debug/TestbedScenarioExamples.cpp`) |

## Other uses

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set ups for trying the editor on | done | `editor.*` |
| Crowd benchmarks of creatures and villagers | done | `benchmark.creatures_*`, `benchmark.villagers_*` |
| Example scenarios to copy, set out with fixtures | done | `examples.*` (`src/Debug/TestbedScenarioExamples.cpp`); test `TestbedScenarios.ExamplesAreWellFormedFixtures` |

## Parts of the game without scenarios

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers' lives and jobs | todo | no scenario; the village fixture sets out huts, villagers and a storage pit |
| Towns growing, their desires and building | todo | no scenario |
| Buildings: construction, the workshop's scaffolds, damage and repair | todo | no scenario |
| Wild animals and livestock on their own | todo | no scenario |
| Worship and prayer power from worshippers | todo | no scenario; worship walks log "not in this tree yet" |
| Weather on its own: climates, rain and snow, wind | partial | the island's weather is a scenario setting, and the storm fixture makes a storm (`examples.storm_lightning`); the climates and the wind have no scenario. The debug Weather window forces climates, rain and storms (`src/Debug/Weather.cpp`) |
| The temple and its rooms | todo | no scenario; the debug Temple window goes to each room (`src/Debug/Temple.cpp`) |
| Land scripts and challenges | todo | no scenario; no land script runs on the testbed |
| Physics of thrown objects other than the creature's and the miracles' | todo | no scenario |
