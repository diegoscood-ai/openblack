# Testing and verifying a change

## Summary

A refactor or a port must not change what the game does, and a fix must change only what it says it changes. Every
change passes four kinds of check: the unit and integration tests, the **fidelity run** (two runs of the game under
fixed conditions with an identical per-turn state hash and random trace), the **visual checks** (screenshots that match
pixel for pixel, and the scripted hand demos), and a **performance** comparison. Logic that only real input can reach
is first moved into a pure function and covered by characterisation tests. To check one part of the game on its own,
set up and compare a testbed scenario: see [Testbed scenarios](scenarios.md#summary).

## The original

The checks on this page compare openblack with itself, before and after a change. What openblack must reproduce is the
frozen v1.42 `runblack.exe` (see [How to read the pages](reading-the-wiki.md#how-to-read-the-pages)): its numbers reach
the tests through the wiki's pages and the exe oracle ([Research tools](research-tools.md#openblack-tools)), and a test
that pins a number of the original names the page that holds it.

## openblack

### Unit and integration tests

- Tests live in the top-level `test/` folder and are grouped into one gtest executable per area: `test_core`,
  `test_villagers`, `test_town`, `test_magic`, `test_audio`, `test_creature`, `test_ui_script`, `test_bgfx`,
  `test_game` (with the video tests) and `test_camera`, plus the standalone `test_mod_loader`.
- Run them both ways, because they catch different problems:
  - `ctest` runs each test in its own process;
  - running each `test_*.exe` whole catches state that leaks from one test to the next.
- **Test services.** A gtest listener emplaces fresh state services before every test and resets them after it: the
  random streams, the clock, the particle system, the weather, the physics objects, the players and the map script. A
  service is added to that list only when all three of these hold:
  1. it replaces a global that always existed;
  2. it starts empty, as in the game;
  3. it has no behaviour a test should fake.
- **Fakes and mocks.** Everything else is injected by the test itself, with fakes from `test/support` and
  `test/mock`. Tests never check a system through `Locator::X::value()`; the locator is only used to inject.
- **Game data.** Tests that need the original game's data are marked as integration tests and skip themselves when
  `OPENBLACK_GAME_PATH` / `OPENBLACK_TEST_GAME_PATH` is not set. Where possible a synthetic twin with in-memory data
  covers the same logic.
- Every commit builds and passes the tests on its own, so the history can be bisected.

### Behaviour is identical: the fidelity run

Run the game twice, once with the build before the change and once with the build after it, under the same conditions,
then compare.

#### Deterministic conditions

| Variable | Purpose |
|---|---|
| `OPENBLACK_FIXED_FRAME_MS=16` | Fixed time step, so the run does not depend on the frame rate (33 for the screenshots) |
| `OPENBLACK_MOUSE_AT=0.5,0.5` | The hand stays at the screen centre |
| `OPENBLACK_IGNORE_REAL_INPUT=1` | Real mouse events are ignored, so a person at the machine cannot disturb the run |
| `OPENBLACK_TEST_SKIP_ANSWER=3,<frame>` | Answers the start-up requester with «keep my creature» at that frame |
| `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` | Fixed camera for screenshots |
| `OPENBLACK_CLOUD_SEED=1` | Fixed cloud seed |
| `OPENBLACK_TIME_OF_DAY=<hour>` | Fixed time of day, e.g. 22 for a night screenshot |

Use an **empty `Mods` folder**. A check run reads a read-only copy of the game data; a run that writes game data (a
land change saves the creature) runs on a copy of its own, so it cannot change what the next run reads.

#### What is compared

| Variable | Records |
|---|---|
| `OPENBLACK_STATE_HASH=<file>` | One line per turn, with a hash of each part of the world state (transforms, villagers, towns, alignment…). The `pools` part counts component storages and changes when a refactor adds a component type, so it is the only part allowed to differ. |
| `OPENBLACK_STATE_HASH_ENTITIES=all` | Beside the state hash, a per-entity file: each entity's parts hashed every turn. A behaviour commit runs with it, so that the first entity and turn that moved can be named. |
| `OPENBLACK_TRACE_GAME_RAND=1` | Every random draw, with its stream and game turn. The traces must be identical line by line. |

Two tools read these files ([Research tools](research-tools.md#openblack-tools)):

- `compare_run.py <ref> <run> [--ignore <parts>]` compares the per-turn hash part by part (`pools` always ignored) and
  the random-draw lines of the log. "first differing part: none … identical" is a pass. Rerun on a run's own files, it
  is the authority for any batch verdict.
- `hashdiff.py <base>_hash_entities.txt <run>_hash_entities.txt [--part transform] [--max 20]` names the first entity
  and turn that moved, and how.

When two hash files are compared by hand, remove the `pools=` field from every line; never drop whole lines.

#### Runs

- **Land 1** for 900 frames.
- **The land cycle:** Land 1 → Land 2 → Land 1. This catches state that should, or should not, survive a land load.
  Both can run in one launch; the first land is then split off as the fidelity run.

### Visual checks

- **Land view.** Take a screenshot of the land view by day and by night, with the camera fixed, before and after the
  change, and compare the pixels: the result must be identical. For night shots, take one throw-away run first, because
  the first run after a build can differ (cold caches).
  The camera lock places the camera once a turn; between placements the world camera re-aims its focus every frame
  at the land under the screen centre. So a change that only moves that ray's last bits (the eye ray, 2026-10-09) moves
  the shot by about 1e5 pixels of ±1 noise (about 550 over 8 on foliage and edges, max about 206, the sky unchanged, no
  coherent shift): the same signature as moving the lock by one float step. Check against that control before reading
  such a difference as a real change. A new reference picture is taken only when a second run matches it.
- **Start menu.** A screenshot of the start menu is informational only: tree foliage sway is not stable between runs.
- **Hand demos.** The hand demos (`OPENBLACK_TEST_HAND_DEMO`) and spell hooks (`OPENBLACK_TEST_SPELL`,
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_STORM_SHOT`) exercise input, gestures and spells. With
  `OPENBLACK_IGNORE_REAL_INPUT=1` they are deterministic, and their state hash and random trace must match too.
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
  are in [Running one and comparing](scenarios.md#running-one-and-comparing).

### Logic only reached by input

Some code is only reached through real input, such as mouse button edges. Move the logic into a pure function without
changing it, write characterisation tests with fake events from a reading of the original code, then move the state
into its owner. The tests must pass before and after the move. No new test hook is added to the game for this.

### Performance

A refactor must not make the game slower. Compare the mean frame time of the old and new exe on Land 1, without any
tracing, without a fixed time step, and with no build running. The difference must stay within run-to-run noise
(about 5%).

### Which checks a change runs

A single change runs only the checks its list of changed files can reach; a series tip and every joint build run the
full set.

| The change touches only | Runs |
|---|---|
| documentation | nothing (the joint rebuilds the wiki) |
| test files of some groups | a build of those test groups, and their tests |
| testbed files | the full build, the tests and its scenarios (one batch launch) |
| debug interface only | the full build, the tests and the day screenshots |
| other game code, behaviour identical | the full build, the tests, the fidelity run and the cycle |
| other game code, behaviour changed | the above, plus the hand demos, the screenshots and the area's scenarios |
| a series tip | the full set, every scenario included |

In a stack of commits, each behaviour commit is measured on its own against the same base, with
`OPENBLACK_STATE_HASH_ENTITIES`. An intermediate commit runs the light checks (build, tests, the fidelity run and the
hand demos); the tip runs the full set, and every part the tip moves must be explained by one of the single commits.

### Test hooks

The variables above are the hooks the checks use; the cross-page list is in
[Debug environment variables](debugging.md#debug-environment-variables). Each fix or feature reachable in play also
gets a debug control, closed by default so the checks stay identical.

## Pending

- Nothing open in the method itself. The scenario runner's limits are in [Limits](scenarios.md#limits).
