# Testing and verifying a change

## Summary

A refactor or a port must not change what the game does, and a fix must change only what it says it changes. Every
change passes three kinds of check: the unit and integration tests, the **fidelity run** (two runs of the game under
fixed conditions with an identical per-turn state hash and random trace, on Land 1 and over a land cycle), and the
**visual checks** (screenshots that match pixel for pixel, and the scripted hand demos). A refactor must also not make
the game slower (a **performance** comparison). Logic that only real input can reach is first moved into a pure
function and covered by characterisation tests. To check one part of the game on its own, set up and compare a testbed
scenario: see [Testbed scenarios](scenarios.md#summary).

## The original

The checks on this page compare openblack with itself, before and after a change. What openblack must reproduce is the
frozen v1.42 `runblack.exe` (see [How to read the pages](reading-the-wiki.md#how-to-read-the-pages)): its numbers reach
the tests through the wiki's pages and the exe oracle ([Research tools](research-tools.md#openblack-tools)), and a test
that pins a number of the original names the page that holds it.

raffclar's tests that pin a table of the oracle's numbers read it through `test/oracle/OracleData.h`: the table's
folder is found when the test runs (`OPENBLACK_ORACLE_DATA`, else the first line of `oracle_data_folder.txt` in the
build folder of the tests, which CMake writes), never compiled in. Our own tests pin the numbers inline, so the
folder `test/oracle/data` has no files yet; `test_oracle_data_folder` checks the lookup.

## openblack

### Unit and integration tests

- Tests live in the top-level `test/` folder and are grouped into one gtest executable per area: `test_core`,
  `test_villagers`, `test_town`, `test_magic`, `test_audio`, `test_creature`, `test_ui_script`, `test_bgfx`,
  `test_game` (with the video tests) and `test_camera`, plus the standalone `test_mod_loader`. A new test file is
  added by hand to its area's group in `test/CMakeLists.txt`.
- Run them in all three ways, because they catch different problems:
  - each grouped `test_*.exe` whole, which catches state that leaks from one test to the next;
  - each test suite alone, in its own process;
  - at the full joints, every test alone in its own process, through `ctest` (which lists each test on its own).
- **Test services.** A gtest listener (`test/support/TestServices.cpp`) emplaces fresh state services before every
  test and resets them after it: the random streams and the clock, and the empty state of the game's systems (the
  particles, the weather and the rain, the physics objects, the players, the magic, the towns and the villagers, the
  worship, the audio, the world effects, the route planner, the input, the game statistics and the map script, among
  others). A service is added to that list only when all three of these hold:
  1. it replaces a global that always existed;
  2. it starts empty, as in the game;
  3. it has no behaviour a test should fake.
- **Trace switches.** Every test executable starts with the game's trace switches (`OPENBLACK_*TRACE*`) cleared, so a
  test that wants one sets it itself; the data paths and the other switches are kept.
- **Fakes and mocks.** Everything else is injected by the test itself, with fakes from `test/support` and
  `test/mock`. Tests never check a system through `Locator::X::value()`; the locator is only used to inject.
- **Game data.** Tests that need the original game's data are marked as integration tests and skip themselves when
  their own variable is not set: `OPENBLACK_GAME_PATH`, `OPENBLACK_TEST_GAME_PATH` or `OPENBLACK_TEST_BW_ROOT`,
  depending on the test ([Tests and test data](debugging.md#tests-and-test-data)), so a run that is to cover them all
  sets all three to the game folder. Where possible a synthetic twin with in-memory data covers the same logic.
- **Temporary files.** A test that writes files takes its paths from `test/TestTempFolder.h` (raffclar's):
  `UniqueTempPath(name)` puts the process number in the name, and `TestTempFolder` is an empty folder removed when the
  test ends. Two test runs at once, from two worktrees, then never share a file.
- Every commit builds and passes the tests on its own, so the history can be bisected.

### Behaviour is identical: the fidelity run

Run the game twice, once with the build before the change and once with the build after it, under the same conditions,
then compare.

#### Deterministic conditions

| Variable | Purpose |
|---|---|
| `OPENBLACK_FIXED_FRAME_MS=16` | Fixed time step, so the run does not depend on the frame rate (33 for the screenshots) |
| `OPENBLACK_MOUSE_AT=0.5,0.5` | The hand stays at the screen centre (0.02,0.02 for the land view shots; unset for the hand demos, whose cursor is the demo's) |
| `OPENBLACK_IGNORE_REAL_INPUT=1` | The real mouse (its motion, buttons and wheel) is ignored, so moving or clicking it cannot disturb the run. Keyboard events still reach the game: nobody should type in the game window during a run |
| `OPENBLACK_TEST_SKIP_ANSWER=3,<frame>` | Answers the start-up requester (the SkipBox) at that frame with answer 3: skip the tutorial and the creature training, and keep the old creature ([Skipping the tutorial](../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial)). The checks answer at frame 30 |
| `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` | Fixed camera for screenshots |
| `OPENBLACK_CLOUD_SEED=1` | Fixed cloud seed |
| `OPENBLACK_TIME_OF_DAY=<hour>` | Fixed time of day, e.g. 22 for a night screenshot |

Use an **empty `Mods` folder**: no check runs with a mod on. A check run reads a read-only copy of the game data; a run
that writes game data (a land change saves the creature) runs on a copy of its own, so it cannot change what the next
run reads. That copy holds the original's `Data\AllMeshes.g3d` from joint180 on; until then the checks read the
installation's modified mesh pack, now only a mod, whose bigger meshes stop 25 of Land 1's trees being created. Every
check and every reference runs at detail level 6, where openblack starts (🎯 chosen difference, user, 2026-10-21; the
original starts at 4), from joint172 on.

#### What is compared

| Variable | Records |
|---|---|
| `OPENBLACK_STATE_HASH=<file>` | One line per turn, with a hash of each part of the world state (transforms, villagers, towns, alignment…). The `pools` part counts component storages and changes when a refactor adds a component type, so it is the only part allowed to differ. |
| `OPENBLACK_STATE_HASH_ENTITIES=all` | Beside the state hash (nothing without `OPENBLACK_STATE_HASH`), a per-entity file: each entity's parts hashed every turn (`all`, or a list of parts). A behaviour commit runs with it, so that the first entity and turn that moved can be named. |
| `OPENBLACK_TRACE_GAME_RAND=1` | Every random draw, with its stream and game turn. The traces must be identical line by line. |

Two tools read these files:

- `compare_run.py <ref> <run> [--ignore <parts>]`, one of the check scripts, compares the per-turn hash part by part
  (`pools` always ignored) and the random-draw lines of the log. "first differing part: none … identical" is a pass.
  Rerun on a run's own files, it is the authority for any batch verdict.
- `hashdiff.py <base>_hash_entities.txt <run>_hash_entities.txt [--part transform] [--max 20]` (`tools/re/hashdiff`
  in the repository, [hashdiff](research-tools.md#hashdiff-which-entity-moved-first)) names the first entity and turn
  that moved, and how.

When two hash files are compared by hand, remove the `pools=` field from every line; never drop whole lines.

#### Runs

- **Land 1** for 900 frames.
- **The land cycle:** Land 1 → Land 2 → Land 1. This catches state that should, or should not, survive a land load.
  Both can run in one launch; the first land is then split off as the fidelity run.
- **The story run:** Land 1 → the vortex → Land 2, the story's own way between the two lands (9000 frames, on a copy
  of the game data, since the land change writes it). Every joint runs it.

### Visual checks

- **Land view.** Take a screenshot of the land view by day and by night, with the camera fixed, before and after the
  change, and compare the pixels: the result must be identical. For night shots, take one throw-away run first, because
  the first run after a build can differ (cold caches).
  The camera lock places the camera once a turn; between placements the world camera re-aims its focus every frame
  at the land under the screen centre. So a change that only moves that ray's last bits (the eye ray) moves
  the shot by about 1e5 pixels of ±1 noise (about 550 over 8 on foliage and edges, max about 206, the sky unchanged, no
  coherent shift): the same signature as moving the lock by one float step. Check against that control before reading
  such a difference as a real change. A new reference picture is taken only when a second run matches it.
- **Start menu.** A screenshot of the start menu is informational only: it is not stable between runs (the menu's 3D
  cursor spins from run to run).
- **Hand demos.** The hand demos (`OPENBLACK_TEST_HAND_DEMO`) and spell hooks (`OPENBLACK_TEST_SPELL`,
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_STORM_SHOT`) exercise input, gestures and spells. With
  `OPENBLACK_IGNORE_REAL_INPUT=1` they are deterministic, and their state hash and random trace must match too. The
  checks play six Land 1 demos (Gestures, shield, drag, MiracleCast, castfood and fireball) in one launch, each as a
  new game.
  The miracle demos were recorded at tutorial moments with a miracle seed already in the hand; played from the land's
  start, the hand holds no seed, so no miracle is cast in them. A change to miracles or to reactions to them needs a run
  that casts one, for example with `OPENBLACK_TEST_SPELL`.
- **Temple interior.** `OPENBLACK_AUDIO_TEST_CITADEL=120` enters the temple at hook turn 120 of a fixed-step Land 1 run
  (`OPENBLACK_FIXED_FRAME_MS=33`, cursor fixed); a screenshot at frame 700 must match the reference pixel for pixel, and
  the run, which ends inside the temple, must exit cleanly (also with `120,200`, in and out again).
- **Creature in its pen.** For a change to how the creature is drawn (its body, its shadow): a fixed-step Land 1
  run (`OPENBLACK_FIXED_FRAME_MS=33`, `OPENBLACK_TEST_SKIP_ANSWER=3,30`, cursor fixed) with
  `OPENBLACK_CAMERA_LOCK=1897,38,2510,1897,24,2526` and a screenshot at frame 150, while the creature still stands in
  the temple's pen (the script lets it go at turn 30; at frame 740 the pen is empty). Compare before and after; only
  the changed part may differ. The other shots don't show the creature.
- **Scenarios.** The testbed scenarios are compared the same way, run in batches of one launch each; their verdicts
  are in [Running one and comparing](scenarios.md#running-one-and-comparing). A scenario whose picture is known to
  differ between two runs of the same build is on a list of flaky shots, and is judged by its state hash only: its
  pixels may differ, its state must not.

### Logic only reached by input

Some code is only reached through real input, such as mouse button edges. Move the logic into a pure function without
changing it, write characterisation tests with fake events from a reading of the original code, then move the state
into its owner. The tests must pass before and after the move. No new test hook is added to the game for this.

### Performance

A refactor must not make the game slower. Compare the mean frame time of the old and new exe on Land 1, without any
tracing, without a fixed time step, and with no build running. The difference must stay within run-to-run noise
(about 5%).

### Which checks a change runs

A single change runs only the checks its list of changed files can reach; a series tip runs the full set. A joint
always runs the build, the tests, the fidelity run, the cycle, the hand demos and the story run, plus the scenarios
its commits can reach and, when the renderer, the interface, the sky or the terrain can have moved, the screenshots
(the land view by day and by night, and the temple). Every fifth joint, and any joint that moves every reference, runs
every scenario. A new reference is taken only when a second pass agrees with the first.

| The change touches only | Runs |
|---|---|
| documentation | nothing (the joint rebuilds the wiki) |
| test files of some groups | a build of those test groups, and their tests |
| testbed files | the full build, the tests and its scenarios (one batch launch) |
| debug interface only | the full build, the tests and the day screenshots |
| other game code, behaviour identical | the full build, the tests, the fidelity run and the cycle |
| other game code, behaviour changed | the above, plus the hand demos, the screenshots and the area's scenarios |
| a series tip | the full set, the whole scenario batch included |

In a stack of commits, each behaviour commit is measured on its own against the same base, with
`OPENBLACK_STATE_HASH_ENTITIES`. An intermediate commit runs the light checks (build, tests, the fidelity run and the
hand demos); the tip runs the full set, and every part the tip moves must be explained by one of the single commits. A
part that moves only at the tip is bisected first.

### Test hooks

The variables above are the hooks the checks use; the cross-page list is in
[Debug environment variables](debugging.md#debug-environment-variables). Each fix or feature reachable in play also
gets a debug control, closed by default so the checks stay identical.

## Pending

- Nothing open in the method itself. The scenario runner's limits are in [Limits](scenarios.md#limits).
