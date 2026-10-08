# Testbed

The creature testbed: a flat land with a lake, made by openblack on raffclar's branch and not yet in our tree, and the
scenario runner that sets it up and plays a scripted timeline on it, from the debug window, the editor or the command
line. What the scenarios cover is in [testbed_scenarios.md](testbed_scenarios.md).

**Progress: 0/23 done, 1 partial — 2%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## The testbed land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A flat land with a lake, loaded from the menu or `--testbed` instead of a story land | todo | no testbed in our tree: no flat land, no `--testbed` |
| No land script runs on it, so its clock and weather stay as set | todo | no testbed in our tree |
| The player has a ring of influence and plenty of prayer power on it | todo | no testbed in our tree |
| A grid of every miracle's dispenser laid out in front of the camera, kept or cleared per scenario | todo | no testbed in our tree |
| Emptying the testbed of everything put on it | todo | no testbed in our tree |

## Scenarios as data

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each scenario is plain data: a name, a description, the facet it shows and an id | todo | no testbed in our tree |
| The weather, the hour and whether the clock runs, the body's game speed, fainting and fights by themselves | todo | no testbed in our tree |
| The player's alignment and prayer power as it starts | todo | no testbed in our tree |
| Where the camera starts: the testbed's view, an overview of everything, behind or in front of one creature, or a placed shot | todo | no testbed in our tree |
| The creatures with their species, body, age, needs, desires, wounds, mind file and leash | todo | no testbed in our tree |
| The objects, trees, features, villagers, food and wood, buildings and fields of a town, and animals about them | todo | no testbed in our tree |
| Crowds of hundreds to thousands, laid out the same way every run | todo | no testbed in our tree |
| A timeline of commands at set seconds: moving, acting, needs, the hand, the leash, fights, learning, the camera keys, Creature Mode and the cave, seeds, gestures, miracles, key presses, the pointer | todo | no testbed in our tree |
| The pointer moved and pressed by the scenario in place of the mouse, through the game's own input | todo | no testbed in our tree; a queued press goes through the game's input (`GameActionMap::QueuePress`; test `GameActionMap.QueuedPressGoesThroughTheKeyPath`), and `OPENBLACK_MOUSE_AT` places the game's cursor |

## Running them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A list of scenarios by facet, searched, with Run, Restart and Stop | todo | no testbed in our tree |
| Overview and testbed camera views, following one of the creatures, and letting go of the camera | todo | no testbed in our tree |
| A readout of each creature's needs and what it is doing as the scenario runs | todo | no testbed in our tree |
| Saving a run's results to a file | todo | no testbed in our tree |
| `--scenario ID` starts the game on the testbed running a scenario | todo | no `--scenario` in our tree |
| Benchmarks: a crowd warmed up for some frames, then measured, its results written as JSON and CSV, and the game quits | partial | the recorder that sums up and writes a run is there (`src/Debug/BenchmarkRecorder.cpp`; tests `BenchmarkRecorder.*`), but no crowds, no switches to start it and no quitting after it |
| A scenario checks its own outcome and reports pass or fail | todo | checks live in the unit tests |
| Scenarios on the story lands rather than the flat testbed | todo | no testbed in our tree |
| Scenarios recorded from play rather than written by hand | todo | no testbed in our tree |
