# Testbed scenarios

The testbed is a flat land with a lake, made by the game itself instead of a story land. A scenario is a ready-made set
up of it, written as plain data: what stands on the land, the weather and the hour, where the camera looks, and a
timeline of commands. This page is for anyone who wants to show one part of the game on its own, and to check that a
change leaves it as it was.

## 1. Starting it

| Switch | What it does |
|---|---|
| `--testbed` | Starts on the testbed instead of `--start-level`. No land script runs on it, and the player gets a ring of influence over the middle of the map |
| `--scenario <id>` | Starts on the testbed and runs that scenario. An unknown id logs the list of ids and the game quits |
| `--benchmark-warmup N`, `--benchmark-frames N`, `--benchmark-out <base>` | For a `benchmark.*` scenario: the frames its crowd settles for, the frames measured (then the game quits), and where the results go as `<base>.json` and `<base>.csv`. Nothing is written without `--benchmark-out` |

From the debug bar, the menu's **Creature Testbed** item loads it too. Loading the testbed opens the **Testbed
Scenarios** window (section 6).

## 2. Writing one with fixtures

The fixtures (`src/Debug/TestbedFixtures.h`, namespace `testbed_fixtures`) put the common things on the land in a line
or two each: a miracle dispenser, a storm, the player's creature (free, leashed or penned), a village with huts,
villagers and a storage pit, a player's temple, a fire, trees, a pile, a script highlight (a challenge scroll or a
did-you-know sign), a seed in the hand, and a recorded hand demo. Points are offsets from the middle of the map, x east
and y north. The testbed's camera looks north from 120 units south of the middle; keep clear of the lake beyond the
middle.

The example scenarios in `src/Debug/TestbedScenarioExamples.cpp` (facet Examples) are the models to copy. Two of
them, shortened:

```cpp
namespace fixtures = testbed_fixtures;

// A storm with lightning over a village
all.push_back({
    .id = "examples.storm_lightning",
    .name = "A storm with lightning over a village",
    .facet = Facet::Examples,
    .description = "A storm stands over a village north of the middle for a minute, with fork and sheet lightning.",
    .expected = "Rain falls within the storm's radius; lightning forks down now and then and flashes the sky.",
    .environment = {.dispenserGrid = false},
    .framing = {.shot = Shot::Overview, .include = {{-90.0f, 0.0f}, {90.0f, 140.0f}}},
    .fixtures = {.storms = {{.at = glm::vec2 {0.0f, 60.0f},
                             .innerRadius = 60.0f,
                             .outerRadius = 150.0f,
                             .lifeSeconds = 60.0f,
                             .forkSeconds = glm::vec2 {4.0f, 8.0f},
                             .sheetSeconds = glm::vec2 {3.0f, 6.0f}}},
                 .villages = {{.at = glm::vec2 {0.0f, 60.0f}, .villagers = 6}}},
});

// The creature on a leash, and the player's leash key acting on it every 4 seconds
all.push_back({
    .id = "examples.leashed_creature",
    .name = "Your creature on the leash by a wood pile",
    .facet = Facet::Examples,
    .description = "Your creature on the learning leash north of the middle, by a wood pile; L pressed every 4 seconds.",
    .expected = "The first press takes the leash off and it wanders; the next puts the leash back on.",
    .environment = {.dispenserGrid = false},
    .framing = {.shot = Shot::Overview, .include = {{-60.0f, 20.0f}, {60.0f, 80.0f}}},
    .commands = {Press(input::BindableActionMap::LEASH_UNLEASH_CREATURE, 4.0f),
                 Press(input::BindableActionMap::LEASH_UNLEASH_CREATURE, 4.0f)},
    .repeatFrom = 0,
    .fixtures = {.creatures = {{.at = glm::vec2 {0.0f, 40.0f}, .hold = fixtures::Hold::Leashed}},
                 .piles = {{.type = PotInfo::WoodPile_1, .at = glm::vec2 {30.0f, 45.0f}}}},
});
```

`Press` is a small helper of that file that makes a `PressKey` command.

Good to know:

- Fields go in the order the structs declare them, as designated initialisers require.
- Fixtures are set out in this order: villages, temples, dispensers, storms, creatures, trees, piles, highlights, fires,
  the seed in the hand. Each logs one line, placed or not, and why not.
- A dispenser needs a town, so a scenario with one needs a village fixture.
  `particles.freeze_phial_glints` sets out a Freeze dispenser for the glints of the phial in its bubble.
- A creature with no `file` is the profile's creature, which is always the first player's. With a `file` it is loaded
  from the game's creature folder as a map script loads one.
- A temple is made built, as a land's script makes it, with its three leash posts. A player has one temple. Its posts
  show only the leashes its player's creature knows: give the creature fixture those leashes in `knows`, as
  `examples.temple_leash_posts` does (the evil, learning and compassion leashes: all three posts).
- `Hold::TemplePen` sets the creature out at its owner's temple's pen point, the one the game keeps it at, and leaves it
  free: each turn the game makes that point its home and draws it at the pen's size near it, as
  `creature.walks_in_temple_pen` shows. It needs that player's temple among the fixtures.
- A storm turns the climates' own storms off, so that the run is the same every time.
- A highlight is made as a map script's highlight command makes one, from its info row (bronze, did-you-know, silver or
  gold), with no challenge, unturned and at full size. No script holds it, and nothing lifts or lights it.
  `particles.highlight_glints` sets out a silver and a gold scroll for their glints.
- A fire's `what` is either the index of one of the scenario's `objects`, or a point, where a bonfire is made and lit.
- The overview frames the scenario's own creatures and objects, not the fixtures: add the fixtures' points to
  `framing.include`.
- The timeline's creature commands act on the scenario's `creatures`, not on a creature fixture. The player's own
  commands, such as `PressKey`, act on the player's creature, which a creature fixture is.
- `Problems()` checks the fixtures without the game, one sentence per problem; the runner logs them before it starts.

## 3. Where to add it

1. **The facet file.** Add the scenario to the `Add<Facet>Scenarios` function of its facet file
   (`src/Debug/TestbedScenario<Facet>.cpp`), or to the facet's section of `src/Debug/TestbedScenarioRegistry.cpp`.
   A new file is picked up by the build by itself; declare its `Add...Scenarios(std::vector<Scenario>&)` in
   `TestbedScenarioRegistry.h`.
2. **One `Build()` line.** A new facet file needs its call in `Build()` in `TestbedScenarioRegistry.cpp`, which sets
   the order of the window's list.
3. **A test** in `test/test_testbed_scenarios.cpp` (group `test_creature`): the scenario is there under its id and its
   fixtures have no problems, as `ExamplesAreWellFormedFixtures` checks the examples. `EveryScenarioIsWellFormed`
   already checks the rest of its data.

```cpp
const auto* scenario = Find("examples.storm_lightning");
ASSERT_NE(scenario, nullptr);
EXPECT_TRUE(Problems(*scenario).empty());
// with the number of presses its hand demo has, when it has one
EXPECT_TRUE(testbed_fixtures::Problems(scenario->fixtures, std::nullopt).empty());
```

An id is `facet.what`. It is never reused or renamed, because tests, references and the command line pick scenarios by
it.

## 4. Replaying a hand demo

A scenario can play one of the game's recorded hand demos (`Data/HandDemo/<name>.hnd`), from a time into the scenario:

```cpp
// examples.hand_throw_target: a food pile where the demo first presses, a village where it last presses
.fixtures = {.villages = {{.at = fixtures::DemoPress {.press = 3}}},
             .piles = {{.type = PotInfo::FoodPile, .at = fixtures::DemoPress {.press = 0}}},
             .handDemo = fixtures::HandDemo {.name = "castfood"}},
```

- The demo moves the camera, the cursor and the buttons, as the tutorial plays it. The game's playback is not changed.
- `DemoPress {n, nudge}` stands a fixture where the demo's n-th press (a grab or action button going down, counted from
  0) meets the land, moved by `nudge`. The point is the ray from the recorded camera through the recorded mouse, rounded
  to the window's pixel, onto the testbed's plane. It depends on the window's size, so run at the size the scenario was
  written for.
- A demo recorded with its camera close to the ground could not be replayed over the usual plane. The runner then lowers
  the plane until it lies at least 2 units under the demo's lowest eye. `planeAltitude` sets the plane's altitude by
  hand instead.
- The runner logs each press's point. A press that misses the land ends the count, and a fixture at a press the demo
  does not have is not placed.
- Do not drive the pointer from the timeline while a demo plays: both write the cursor and the buttons.

## 5. Scenarios that write into the game's folder

Some scenarios write into the game's folder: a land change (the `ChangeLand` command, as in
`examples.land_change_creature`) saves the creature's mind and physique there. Such a scenario sets
`writesGameData = true`, and it runs only on a copy of the game's data, which carries the marker file
`openblack_game_data_copy.txt` in its root. On the game itself the runner logs a problem and does not start it. Never put
the marker in the real game's folder; run such scenarios with `--game-copy` (section 6).

## 6. Running one and comparing

A scenario is checked like a fidelity run (see [TESTING.md](TESTING.md)): the same build twice, under the same
conditions, then compare. The local script `scenario.sh` does this:

```sh
sh scenario.sh run <tree> <id> <label> [--frames N] [--game-copy]
sh scenario.sh ref <label> <id>
```

- `run` copies the tree's `openblack.exe` and its DLLs to a private folder with no `Mods` folder, and runs
  `--testbed --scenario <id>` at 1024x768 for N frames (900 by default). It uses a 16 ms fixed step, ignores the real
  mouse, records the random trace and the state hash every turn, and takes a shot 20 frames before the end. `<tree>` is
  a worktree name or `main`.
- The run's files are `<label>_<id>.log`, `<label>_<id>_hash.txt` and `<label>_<id>.png` in the script's audit folder.
  If there is a reference `ref_<id>`, the script compares the state hashes without `pools`, the random traces, and the
  shots byte for byte. Exit 0 means identical, 1 different, 3 that there is no reference yet, and 4 the known water
  draw below.
- A reference remembers the frames and the game copy it was made with, so a run takes them unless it gives its own.
- `--game-copy` runs on a copy of the game's data, one per session (`OB_SESSION`), made on first use with its marker.
  The files a run may write are restored from the game before every run.
- `ref` makes a run the reference `ref_<id>`, and keeps the old one with a date. A reference is made from two identical
  runs, and only on the coordinator's order.
- Compare the shot only for scenarios whose camera does not move with something random.
- **A known bug: the water is drawn non-deterministically.** Twice in about thirty runs, a shot differed from its pair
  while the state hash and the random trace were identical: `examples.leashed_creature` (448 pixels) and
  `examples.storm_lightning` (476 pixels). Each differing pixel was off by 1 in one channel, and all lay in the lake's
  water and its shallows. Later runs were identical again. The script reports this case alone as exit 4: the hash and
  the trace identical, every differing pixel off by at most 1 and within 40 pixels of clear water in the reference.
  Anything else is exit 1. On exit 4, run the scenario once more; a second 4, or a 1, is a difference to report. Always
  compare the hash and the trace before the shot. The cause is not found yet; the candidates are the draw's clock (the
  game milliseconds of the drawn frame) moving the sea's drift and its frame counter, and the draw thread's timing
  against the logic's snapshot (water.md, Pending).

## 7. The Testbed Scenarios window

- Pick a facet, then a scenario. Its description, what to look for, and a summary of its data are shown.
- **Run**, **Restart**, **Stop**, and **Empty testbed**, which loads the testbed afresh with nothing on it. The status
  line and the log show what each command and fixture did.
- **Benchmark**: the crowd's spawning and the frames measured. **Save results** shows only with `--benchmark-out`.
- **Time**: the game's speed and the creatures' body time. **Camera**: follow a creature, its head and eyes, the
  overview, the testbed's view, or let go. **Its creatures**: a readout of each creature's needs, size, speed, what it
  is doing and its strongest desire.
- **Fixtures**: sets one thing out at once at a point from the middle of the map: a dispenser of any miracle or its seed
  in the hand, a storm (rain, snow or both, with or without lightning), a creature (free, leashed or penned), a village,
  a fire, trees or a wood pile. It is the quickest way to try a fixture before writing a scenario.
- The editor shows the same window as its **Scenarios** tab.

## 8. Limits

- Parts of a scenario that this tree cannot do yet are skipped, with one log line `not in this tree yet: ...`: the old
  dispenser and miracle lists of the scenario data (use the dispenser fixture instead), the hand held still, tribal and
  prayer power, the miracles' position log, villagers' walks and teleport stones, and particle colours by player. So are
  these commands: catching a fireball or taking one into a fire seed, shaking a leash off, the hand stroking or slapping
  a creature, the double click and the held camera keys of Creature Mode. Such a scenario still runs, without that part.
- A gesture is drawn as the first template of that gesture, 320 pixels wide across the middle of the screen, through
  the stroke player the Gestures window draws with: one mouse message every 28 ms, none dropped, the real mouse left
  out meanwhile. The pointer follows the stroke through the fixed cursor alone, with no mouse events, so that the hand
  draws it and no drag can move the camera. The circle is drawn with the Action button held, which is let go a quarter
  of a second after the last message. What the recogniser takes is logged, with the circle's middle and size. A seed
  held is given as from a bubble, at its base level; a seed summoned is asked of the player's worship, as the miracle
  selection asks, and needs a worship site that can give it.
- This tree's testbed does not yet lay out the grid of every miracle's dispenser, nor give the player extra prayer
  power.
- The pointer's buttons reach the hand only. They go straight into what the hand reads, not through the mouse's events,
  so the camera and the debug windows do not see them.
- The wheel and the camera's drags do come as mouse events, and these are dropped when the real input is ignored
  (`OPENBLACK_IGNORE_REAL_INPUT=1`, as in every check) or the mouse is fixed. A scenario that needs them logs a problem
  under those runs.
- A creature does not cast miracles yet: its mind's command to cast (`TellCast`) is not ported, so the creature
  casting scenarios `miracles.creature_casts_itchy`, `creature_casts_lightning`, `creature_casts_heal`,
  `creature_cast_fizzles` and `creature_too_tired_to_cast` only show it walking up. They are not tests of casting
  until that command is ported.
- A scenario has no pass or fail of its own. Its checks are the unit tests and the comparison with its reference.
