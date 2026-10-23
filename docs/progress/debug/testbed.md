# Testbed

The creature testbed: a flat land with a lake, made by openblack rather than loaded from the game, and the scenario
runner that sets it up and plays a scripted timeline on it, from the debug window, the editor or the command line. What
the scenarios cover is in [testbed_scenarios.md](testbed_scenarios.md); how to write and run one is in
[SCENARIOS.md](../../scenarios.md).

**Progress: 19/29 done, 0 chosen, 7 partial, 0 inferred — 81%**

How the original does it, in our wiki: [openblack internals](../../internals.md).

## The testbed land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flat land with a lake, loaded from the menu or `--testbed` instead of a story land | done | `src/3D/FlatLand.cpp`, `Game::LoadTestbed` (`src/Game.cpp`), "Creature Testbed" in the debug bar's menu (`src/Debug/Gui.cpp`); tests `FlatLand.*`, `LandData.*` |
| No land script runs on it, so its clock and weather stay as set | done | `Game::LoadTestbed` stops every script task, and the game's start skips the story's scripts and the start-up question on the testbed (`src/Game.cpp`) |
| The player has a ring of influence and plenty of prayer power on it | partial 60% | a ring of influence over the middle of the map (`Game::LoadTestbed`); no extra prayer power; re-measured 2026-10-21: the ring is laid; the prayer power is not |
| A grid of every miracle's dispenser laid out in front of the camera, kept or cleared per scenario | partial 40% | the layout and the rule for keeping it are pure data (`src/Debug/TestbedDispenserGrid.cpp`, `KeepsDispenserGrid`; tests `TestbedDispenserGrid.*`), but nothing lays the grid out yet; re-measured 2026-10-21: the layout and its rule are data with tests; nothing lays the grid out |
| Emptying the testbed of everything put on it | done | the window's "Empty testbed" loads it afresh (`src/Debug/TestbedScenarios.cpp`) |

## Scenarios as data

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each scenario is plain data: a name, a description, the facet it shows and an id | done | `src/Debug/TestbedScenarioRegistry.*` and the facet files `src/Debug/TestbedScenario*.cpp`, about 200 scenarios; tests `TestbedScenarios.*` |
| The weather, the hour and whether the clock runs, the body's game speed, fainting and fights by themselves | done | `Runner::SetUpEnvironment`, the island's weather by `Runner::SetIslandWeather` (`src/Debug/TestbedScenarioRunner.cpp`) |
| The player's alignment and prayer power as it starts | partial 50% | the alignment is set; a scenario's prayer power logs "not in this tree yet"; re-measured 2026-10-21: the alignment is set; the prayer power is not |
| Where the camera starts: the testbed's view, an overview of everything, behind or in front of one creature, or a placed shot | done | `Runner::Frame` (`src/Debug/TestbedScenarioRunner.cpp`); tests `TestbedScenarios.OverviewSeesTheWholeBox`, `FollowAndHeadShotsScaleWithTheCreature` |
| The creatures with their species, body, age, needs, desires, wounds, mind file and leash | done | `Runner::PlaceCreatures`; a "chosen:" mind file is always the game's file of that name, as no file picked in the spawner is kept |
| The objects, trees, features, villagers, food and wood, buildings and fields of a town, and animals about them | partial 80% | `Runner::PlaceObjects`; the villagers' walks to worship through the stones log "not in this tree yet" (`miracles.teleport_worship`); re-measured 2026-10-21: the objects are placed; villagers' walks, worship walks and teleport stones are not |
| Crowds of hundreds to thousands, laid out the same way every run | done | `src/Debug/TestbedCrowd.cpp`, spawned a batch a frame by the runner; tests `TestbedScenarios.BenchmarksMeasureCrowdsOfEachSize`, `CrowdsAreChecked` |
| A timeline of commands at set seconds: moving, acting, needs, the hand, the leash, fights, learning, the camera keys, Creature Mode and the cave, seeds, gestures, miracles, key presses, the pointer | done | `testbed_scenarios::Advance` (pure; tests `TestbedScenarios.Timeline*`) and the runner, which handles every command kind (`Runner`, `src/Debug/TestbedScenarioRunner.cpp`); since 2026-10-15 no command logs "not in this tree yet". What still does is the set-up of a few miracle scenarios (tribal power, prayer power, worship walks through the stones, seeds summoned from worship into the hand; `src/Debug/TestbedScenarioRunner.cpp:1514-1535`), covered by the rows above; re-read in the code 2026-10-22 |
| The pointer moved and pressed by the scenario in place of the mouse, through the game's own input | partial 90% | `src/Debug/TestbedPointer.cpp` (tests `TestbedPointer.*`), with no hook of its own: it moves the game's fixed cursor and writes the buttons the hand reads, so the buttons reach the hand only. The wheel and the camera's drags go as mouse events, which are dropped when the real input is ignored or the mouse is fixed; re-measured 2026-10-21: the pointer goes through the game's own input; see its notes for the buttons it cannot reach |
| Fixtures: a dispenser, a storm, the creature free, leashed or penned, a village, a fire, trees, a pile and a seed in the hand, each in a line or two | done | `src/Debug/TestbedFixtures.*`, checked by `Problems()` without the game and set out through the game's own archetypes and systems; tests `TestbedFixtures.*`. A dispenser needs a village fixture for its town |
| A recorded hand demo replayed on the testbed, with fixtures at the points its presses meet the land | done | `src/Debug/TestbedDemoPoints.cpp` (pure; tests `TestbedDemoPoints.*`, `HandDemoRecords.*`), the plane lowered under a demo recorded with a low camera; the game's playback unchanged |
| A scenario that writes into the game's folder runs only on a copy of the game's data | done | `writesGameData`, checked against the marker `openblack_game_data_copy.txt` by `Runner::PlaceFixtures` |

## Running them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A list of scenarios by facet, searched, with Run, Restart and Stop | partial 80% | `src/Debug/TestbedScenarios.cpp`, `TestbedScenariosModel.cpp` (tests `TestbedScenariosModel.*`): picked by facet, with no search; re-measured 2026-10-21: picked by facet with Run, Restart and Stop; the search is missing |
| Overview and testbed camera views, following one of the creatures, and letting go of the camera | done | the window's Camera section (`src/Debug/TestbedScenarios.cpp`) |
| A readout of each creature's needs and what it is doing as the scenario runs | done | the window's "Its creatures" table, and the run's log |
| Setting one fixture out from the window | done | the window's Fixtures section (`TestbedScenarios::DrawFixtures`) |
| The scenarios as a tab of the editor | done | `src/Editor/ScenariosTab.h`, `src/Editor/EditorWindow.cpp`; test `EditorScenariosTab.*` |
| Saving a run's results to a file | done | "Save results" with `--benchmark-out` only (`Runner::SaveResults`) |
| `--scenario ID` starts the game on the testbed running a scenario | done | `src/main.cpp`, `src/Debug/TestbedOptions.cpp` (tests `TestbedOptions.*`); an unknown id logs the ids and quits |
| Benchmarks: a crowd warmed up for some frames, then measured, its results written as JSON and CSV, and the game quits | done | `--benchmark-warmup`, `--benchmark-frames`, `--benchmark-out` (`src/main.cpp`), the runner over `src/Debug/BenchmarkRecorder.cpp` (tests `BenchmarkRecorder.*`) |
| A scenario checks its own outcome and reports pass or fail | pending | checks live in the unit tests, and runs are compared with a reference run |
| Scenarios on the story lands rather than the flat testbed | pending | the testbed only; a batch's land entries start a story land with a hand demo, the start-up answer, the menu or the citadel's turns (`demo=`, `skip=`, `menu=`, `citadel=`, `src/Debug/TestbedOptions.cpp:101-106`) but run no timeline; land timelines are coming |
| Scenarios recorded from play rather than written by hand | pending | written by hand; the game's own hand demos can be replayed on them |
| The raw input of a run recorded frame by frame and played back in a later one (the mouse, its wheel and the keys) | partial 50% | `--record-input`, `--replay-input` and Debug > Input Recording (`src/Input/InputReplay.cpp`, the format in `src/Input/InputRecording.cpp`, tests `InputRecording.*`); used once to record a Land 1 run (2026-10-22); its replay is not verified yet; see [input recording](../../../engine/input-recording.md); re-measured 2026-10-21: recorded and played (joint172); not yet run in the game |
