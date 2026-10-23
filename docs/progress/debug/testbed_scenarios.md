# Testbed scenarios

The ready-made scenarios of the testbed (about 200, in `src/Debug/TestbedScenarioRegistry.cpp` and the facet files
`src/Debug/TestbedScenario*.cpp`), grouped by what they show, and the parts of the game that have none yet. A row is
done when there are scenarios for that part and this tree's runner plays everything they ask for; partial when some of
what they ask for logs "not in this tree yet" and is skipped. Since 2026-10-15 no timeline command logs "not in this tree yet" (`Runner::Give*`, `src/Debug/TestbedScenarioRunner.cpp`); only a set-up the runner cannot make does (:1514-1535), and only `miracles.hand_tribal_power`, `miracles.hand_extreme_lightning`, `miracles.hand_press_too_soon`, `miracles.hand_worship_and_creature`, `miracles.shield_runs_out` and `miracles.teleport_worship` ask for one. No scenario has a reference run yet. The framework is in
[testbed.md](testbed.md); how to write one is in [SCENARIOS.md](../../scenarios.md).

**Progress: 23/36 done, 0 chosen, 5 partial, 0 inferred — 72%**

How the original does it, in our wiki: [openblack internals](../../internals.md).

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
| The hand on it: stroking each part, slapping, the status panel, the hand's look by alignment | done | `hand.stroke`, `hand.slap`, `hand.status_panel`, `hand.look_*`; the hand held to a creature strokes, slaps and lets go through `HandStroke`, `HandSlap` and `HandLetGo` (`Runner`, `src/Debug/TestbedScenarioRunner.cpp:2360-2370`); read in the code 2026-10-22, not run |
| The leashes: leading, tying, keeping home, the leash keys and picker, other players' creatures | done | `leash.*`, `examples.leashed_creature`; shaking a leash off is the game's scribble drawn with the empty hand (`LeashShake`, `src/Debug/TestbedScenarioRunner.cpp:2403`); read in the code 2026-10-22, not run |
| Fights: by themselves, charged and quick blows, blocks, knock-outs, slapping other gods' creatures | done | `combat.*` play in full; slapping another god's creature goes through the hand held to it (`HandSlap`); read in the code 2026-10-22, not run |
| Learning: rewards and punishments, by watching, from mind files, copying the player | done | `mind.*`; a "chosen:" mind file is the game's file of that name |
| Creature Mode, following it with C, and the Creature Cave with its tattoos | done | `creature_mode.*`; the double click and the held camera keys go through the game's own input (`DoubleClick`, `CameraKeys`, since 2026-10-15, closes #105) |
| Its voices by species and size, and the sounds of the land | done | `audio.*` |
| Its shadow, its reflection, light on the land through the day | done | `light.*` |

## Miracles and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every miracle cast, held, powered up and its effect, each in its own scenarios | partial 70% | `miracles.*` are listed, set up and cast their miracles (`Runner::CastMiracle`, `src/Debug/TestbedScenarioRunner.cpp`); what a scenario asks that this tree cannot set up yet (tribal power, prayer power, the villagers' walks to worship, seeds summoned from worship into the hand) is logged as "not in this tree yet"; the debug Magic window casts any miracle (`src/Debug/Magic.cpp`, `MiraclesCaster.cpp`); re-measured 2026-10-21: the miracles are set up and cast; what this tree cannot set up yet logs so |
| Dispensers, globes and seeds from worship | partial 50% | the dispenser and hand-seed fixtures work (`src/Debug/TestbedFixtures.cpp`); the globe scenarios' dispensers (`Runner::PlaceDispensers`) and seeds held (`HoldSeed`) are set up; their tribal power and seeds summoned from worship into the hand still log "not in this tree yet" (`miracles.hand_tribal_power`, `miracles.hand_extreme_lightning`); re-measured 2026-10-21: the dispenser and seed fixtures work; the globe scenarios' own parts do not |
| The creature casting its spells | partial 70% | `miracles.*` of the creature casting: the creature is taught and told to cast; a seed held in the hand is skipped; the hand is held still where a scenario asks; re-measured 2026-10-21: the creature is taught and told to cast; a seed in the hand is skipped |
| Fire spreading, blasts, water putting fire out | done | the fire fixture lights objects or a bonfire; the blast and water scenarios cast their miracles through `Runner::CastMiracle`; read in the code 2026-10-22, not run |
| Particles and sprites: sparkles, smoke, steam, mist, sorted sprites | done | `particles.*`; an effect takes its player's colour (`ParticleSetup::player`, given by `Runner::StartParticle` through `SetPlayer`, `src/Debug/TestbedScenarioRunner.cpp:4417-4438`) |
| Gestures drawn through the recogniser | done | the gesture scenarios draw each gesture's stroke with the scenario's pointer for the game's recogniser (`DrawGesture`, `Runner::DrawGesture`, `src/Debug/TestbedScenarioRunner.cpp:2632`); the debug Gestures window draws and sends gestures too (`src/Debug/Gestures.cpp`); read in the code 2026-10-22, not run |
| Hand navigation: dragging, edge turning, tilting, both buttons, middle button | partial 70% | `hand.click`, `hand.drag`, `hand.edge_*` and the rest, driven by the scenario's pointer (`src/Debug/TestbedPointer.cpp`), whose buttons reach the hand only; the wheel and the camera's drags are dropped when the real input is ignored; re-measured 2026-10-21: driven by the scenario's pointer; the wheel and the camera keys do not reach the camera |
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
| Villagers' lives and jobs | pending | no scenario; the village fixture sets out huts, villagers and a storage pit |
| Towns growing, their desires and building | pending | no scenario |
| Buildings: construction, the workshop's scaffolds, damage and repair | pending | no scenario |
| Wild animals and livestock on their own | pending | no scenario |
| Worship and prayer power from worshippers | pending | no scenario; worship walks log "not in this tree yet" |
| Weather on its own: climates, rain and snow, wind | partial 40% | the island's weather is a scenario setting, and the storm fixture makes a storm (`examples.storm_lightning`); the climates and the wind have no scenario. The debug Weather window forces climates, rain and storms (`src/Debug/Weather.cpp`); re-measured 2026-10-21: the weather setting and the storm fixture work; climates and wind have no scenario |
| The temple and its rooms | pending | no scenario; the debug Temple window goes to each room (`src/Debug/Temple.cpp`) |
| Land scripts and challenges | pending | no scenario; no land script runs on the testbed |
| Physics of thrown objects other than the creature's and the miracles' | pending | no scenario |
