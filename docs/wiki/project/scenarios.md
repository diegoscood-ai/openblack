# Testbed scenarios

## Summary

The testbed is a flat land with a lake, made by the game itself instead of a story land. A scenario is a ready-made set
up of it, written as plain data: what stands on the land, the weather and the hour, where the camera looks, and a
timeline of commands. Scenarios show one part of the game on its own, and check that a change leaves it as it was: each
has a reference run (state hash, random trace and screenshot) that a new build is compared with. Many scenarios run in
one launch, each in a new game. A scenario's timeline can also be played on one of the game's own lands, to drive a
chapter of the story ([Playing a timeline on a land](#playing-a-timeline-on-a-land)). This page tells how to start,
write, add, run and compare one.

## The original

The testbed and its scenarios are openblack's own tool; the original has no counterpart. What a scenario shows is the
original's behaviour as the other pages of this wiki describe it, and a scenario that shows a rule names it in its
`expected` text. Where a scenario reproduces something the original does only by a script command, it goes through
that command's native, as the land scripts do ([Script commands](../scripts/script-commands.md#summary)).

## openblack

The runner is in `src/Debug/`: `TestbedScenarioRunner.{h,cpp}` runs the scenarios that
`TestbedScenarioRegistry.{h,cpp}` and the facet files `TestbedScenario<Facet>.cpp` hold, with `TestbedFixtures.{h,cpp}`,
`TestbedOptions.{h,cpp}` (the command line) and `TestbedLandTimeline.{h,cpp}` beside them. Its tests are
`test/test_testbed_scenarios.cpp` and the other `test/test_testbed_*.cpp` files. How a scenario's run fits the other
checks is in [Testing and verifying a change](testing.md#visual-checks).

### Starting it

| Switch | What it does |
|---|---|
| `--testbed` | Starts on the testbed instead of `--start-level`. No land script runs on it, and the player gets a ring of influence over the middle of the map |
| `--scenario <id>` | Starts on the testbed and runs that scenario. An unknown id logs the list of ids and the game quits |
| `--benchmark-warmup N`, `--benchmark-frames N`, `--benchmark-out <base>` | For a `benchmark.*` scenario: the frames its crowd settles for, the frames measured (then the game quits), and where the results go as `<base>.json` and `<base>.csv`. Nothing is written without `--benchmark-out` |
| `--scenarios <id>[:N],...`, `--scenarios-out <prefix>` | Runs these scenarios one after the other in one launch, each in a new game ([Running a list in one launch](#running-a-list-in-one-launch)). An entry runs N frames, or `-n`'s without them |

From the debug bar, the menu's **Creature Testbed** item loads it too. Loading the testbed opens the **Testbed
Scenarios** window ([The Testbed Scenarios window](#the-testbed-scenarios-window)).

### Writing one with fixtures

The fixtures (`src/Debug/TestbedFixtures.h`, namespace `testbed_fixtures`) put the common things on the land in a line
or two each: a miracle dispenser, a storm, the player's creature (free, leashed or penned), a village with huts,
villagers and a storage pit, a player's temple, a fire, trees, a pile, a script highlight (a challenge scroll or a
did-you-know sign), flowers, a football, a seed in the hand, and a recorded hand demo. Points are offsets from the
middle of the map, x east and y north. The testbed's camera looks north from 120 units south of the middle; keep clear
of the lake beyond the middle.

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
- Fixtures are set out in this order: villages, temples, dispensers, storms, creatures, trees, piles, highlights,
  flowers, footballs, fires, the seed in the hand. Each logs one line, placed or not, and why not.
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
  gold), with no challenge, unturned and at full size. No script holds it, and nothing lifts it. With `lit` it starts
  lit, as a tapped scroll is lit: its active effect starts and it takes its active mesh.
  `particles.highlight_glints` sets out a silver and a gold scroll for their glints.
  `particles.highlight_lit` sets out one gold scroll, lit, for its active effect, glow, glints and sparkles.
  `particles.highlight_offscreen` sets out a gold scroll in view and one behind the camera, whose pose is set only on
  the frames the object draw list draws it.
- A did-you-know sign takes a `tip`, its help text and category, as a building's sign is given its own. `TapHighlight`
  needs no creature: the player's hand taps the highlight nearest the point, as the interface's tap does, and the
  runner logs the text the tip bubble shows. `help.tip_bubble` (facet Advisors) taps a sign in the middle of the view,
  and `help.tip_bubble_edge` one whose bubble hangs from just past the left edge of a 1024 x 768 screen.
- `PointerDoubleClick` puts the Action (right) button down as the second click of a double click; `PointerRelease`
  (value 3) lets it go. `leash.double_click_tie` clicks an oak once and then double clicks it.
- A fire's `what` is either the index of one of the scenario's `objects`, or a point, where a bonfire is made and lit.
- The overview frames the scenario's own creatures and objects, not the fixtures: add the fixtures' points to
  `framing.include`.
- The timeline's creature commands act on the scenario's `creatures`, not on a creature fixture. The player's own
  commands, such as `PressKey`, act on the player's creature, which a creature fixture is.
- `Problems()` checks the fixtures without the game, one sentence per problem; the runner logs them before it starts.
- `logHandLight` logs a line each time the land's colour of the frame changes: the colour, the sum and the whole mean
  of its bytes, whether that is dark enough for the lights, and the hand light's strength, worked out by the same
  functions the night lights use, from the table they read that frame. `hand.night_light` uses it. Its sweep of the
  hour is timed for the checks' 16 ms step, so run it for 900 frames, the default: the shot, at frame 880, is at
  17.044 h, a mean of 112. It puts the cursor in the middle of the window (`environment.cursor`) under a placed camera
  looking down at the middle of the map, so the hand and the land it lights are in the middle of the shot.
- Two commands act on the scenario's `objects` rather than on a creature: `VillagerWalkTo` walks a villager to a point,
  going round what stands in its way as the villagers' own walks do, and `StoreTakes` has a storage pit take a thing as
  a thrown thing landing in it is taken (the thing is deleted at once); waiting until free, it waits until a villager
  walks round that thing. `examples.villager_tree_gone` uses both: a tree taken while a villager walks round it.
- `SetGameSpeed` sets the game speed (`gameSpeed`: 2 twice as fast, 0.5 half), which stays as set once the scenario
  ends; after a `ChangeLand` the runner logs the speed the new land runs at. `examples.speed_across_land` sets 2 and
  changes land.
- `SetOpenClose` opens (`value` 1) or closes (0) one of the scenario's `objects`, an animated static such as the Norse
  gate or the gate stone plinth, as a script does, and logs whether it took it. `objects.creature_gate_close` opens the
  gate and the plinth with one stone laid in it, and closes them twelve seconds later.
- `objects.gate_stone_by_hand`: the hand, not a script, lays the tiger stone in the plinth with the pointer: picked up with the right button and given to the plinth with another press.
- `LayGateStone` has the player's hand lay the scenario's `object`, a gate stone, in the plinth that is the object at
  `value`; taken, the stone is used up as the hand uses it up (a fading ghost of it, then the stone deleted) and the
  runner logs what the plinth's stones are now worth. `objects.creature_gate_open` lays the tiger, ape and cow stones,
  then opens the plinth and the gate. The gate and the plinth play their clips on the frames the object draw list draws
  them, and the stones laid in the plinth stand stacked on its top and sink with it as it opens.
- Two more need no creature: `SpiritOut` flies an advisor spirit (value 0 the good one, 1 the evil one) out from its
  corner to a spot on the screen, as the spirits' debug window's Eject does, and `SpiritHome` flies it back. The facet
  Advisors (`src/Debug/TestbedScenarioAdvisors.cpp`) uses them: `advisors.trails` shows the spirits' trails.
- `Advisor` needs no creature either: one of the player's advisors (value 0 the good one, 1 the evil one) is told what a
  script tells it (`advisor`: out, appear, home, vanish, cling, fly, point on the screen or at the land, look, play an
  anim, feel), through the advisors' controller as the scripts' natives go, as raffclar wrote it. His four scenarios
  use it: `advisors.out`, `advisors.cling`, `advisors.point_and_act` and `advisors.appear_and_vanish`.
  `advisors.point` uses it too: the good advisor points at the middle of the map from near the screen, then from
  the world beside it, then flies home.
- `SpiritSay` needs no creature: an advisor spirit (value) says a line of the advisors' voices (`spiritLine`, from 1)
  through the texts' own voice path, as a script's text in that spirit's voice does. `advisors.talk` has the good spirit
  fly out, say the first line (5.2 s) and fly home, for its mouth moving with its voice. `advisors.gesture` has it say
  line 26 (2 s), whose recording's cue labels make it look confused from the start and shrug 1.65 s in.
- Two more need no creature either: `ShowText` runs a text (value, its id) in the dialogue as a script's RUN_TEXT
  does, waiting for a click, and `PlayFilm` starts a full screen film: 0 the intro film as SET_AVI_SEQUENCE(1) does,
  1 the falling spell's film without its test for the player's creature. A film pauses the game, so the scenario's
  clock stands still while it plays: give what must happen under the film on the frame it starts.
  `dialogue.under_falling_film` (facet Advisors) shows two texts, then the falling spell's film over them.
  `film.falling_creature` (facet Appearance) plays that film with a creature, whose copy falls over the film; its shots
  are placed by the run's frames (the film's 5, 15, 19, 23, 30 and 40 s).
- Three more need no creature: `CreateVortex` makes a land vortex at the point as a script's CREATE does (value 0 the
  way out of a land, 1 the way into the next), and `VortexFadeIn` and `VortexFadeOut` fade one the scenario made (value,
  by the order it was made in). Once every vortex the scenario made is gone, or 30 seconds in with the ones still there
  named, the runner logs the heights of the cells the first two vortices' levelled squares share, a line for each
  column. `miracles.vortex_levels_uneven_land` makes the two on the lake's south bank, 60 apart, and fades them out at
  12 and 20 seconds.
- `ScriptFade` needs no creature either: a script's SET_FADE to `fadeColour` (0xRRGGBB) over `fadeSeconds`, or with
  value 1 its SET_FADE_IN back to normal, through the cinematic director as a script's fade goes; the runner puts
  the screen back to normal at once when the scenario stops. `combat.fight_panel_under_fade` fades a fight to black
  over ten seconds, for the fight's panel under the fade.
- `HighDetail` needs no creature: a script's SET_HIGH_GRAPHICS_DETAIL on the scenario's object (`object`), value 1
  on and 0 off, through the `highDetailSystem` slot; it stays on only under a script's wide screen (`WideScreen`).
  `villagers.high_detail_eyes` draws the opening's father, a Norse shepherd, in high detail for twenty seconds, close
  up by day, for his eyes' light.
- `villagers.poisoned_pause`: six villagers at half life, three poisoned, walk four metres to and fro every five
  seconds, arriving each time with nothing to do (deciding afresh never pauses), so that their states change often.
  Each change may pause a villager for a second: a healthy one with a pause clip picked at random, a poisoned one with
  its own clip and no draw, so the random trace counts the poisoned pauses.
- `scriptSpeed` on a villager object sets its whole speed by a script's SET_PROPERTY of SPEED (in metres a turn, the
  whole speed x 10 / 65536) as each of its walks starts; the villager is the script's own from the start, so that it
  keeps the speed. `villagers.gait_limits` walks four men and four women on and just over their walk and run limits.
- `ScriptReward` needs no creature: a script's CREATE_REWARD of a type (`value`, 1 to 60) at the `point`, on the land
  or from the sky (`fromSky`), through the native; the log gives the chest's object. `rewards.on_land` puts three
  chests down, `rewards.from_sky` drops one, which falls for three seconds and thumps down in a cloud of dust.
  raffclar's `story.reward_chests` (six kinds from the sky, a second apart) and `story.reward_chest_opens` (a toy
  ball chest close up) give chests to tap open with the Action button; his `RewardSetup` is not taken, the chests
  come from the same `ScriptReward` commands.
- `birdFlocks` (raffclar's `BirdFlockSetup`) makes flocks of birds as a land's script does, through the animal system's
  `CreateScriptFlock` and `CreateBird`: each flock numbered from 1, its birds spread 8 about its home. The facet Animals
  has his `birds.every_kind`, `birds.close_up`, `birds.seagulls_lake` and `birds.many_flocks`; his `birds.temple_doves` and
  `birds.temple_bats` stand on the temple fixture (`fixtures.temples`); `birds.dead_doves` lays two
  doves killed before a tiger, which picks one up and puts it down.
- A mobile object of the whale's kind (`MobileObjectInfo::Whale`) is made a shark, as a script's CREATE of a whale
  makes it. `ScriptWalkPath` needs no creature: a script's WALK_PATH of the scenario's `object` along a track of the
  game's camera file (`value`, its number), from and to shares of the track's time (`pathFrom`, `pathTo`), backwards
  with `pathBackwards`, through the native; the log says whether the thing is then on the list of things walking a
  track. The tracks are the first land's, so only track 0 crosses the testbed's lake: its focus runs over the shallows
  and into the water at the lake's north western corner, between 39% and about 41% of its time. `sharks.swimming`
  lays a shark in the water there, told nothing: only its part under the water is drawn. `sharks.walks_track` walks
  the same shark from 39% to 40.8% of track 0 every 2.3 seconds: four turns from the shallows back to where it lay,
  its part above the water drawn only while it walks, the checks' shot falling during its sixth walk.
- An animal object of `life` 0 is killed as an effect that takes its last life kills it (the animal's SetDying,
  `animal_ai::DestroyedByEffect`); a bird is first put on the land. Its dying goes as the original's killed bird's: into
  the physics with its flight speed along its heading, then, at rest on the land, dead with its corpse's time.
- `creature.pick_up_by_size`: a small (0.25) and a full-size held tiger each reach for a sheep and then a cow; each
  attempt's result is in the log, as the creature's own pick-up test decides it (reach and weight).
- `creature.pick_up_villagers`: the same test of a villager: a very small (0.1) and a full-size held tiger each reach
  for a Norse shepherd before it and put it down again.
- `creature.pick_up_mobile_objects`: the same test of mobile objects: a very small (0.1) and a full-size held tiger
  each reach for a ball and then a cart.
- `ScriptDance` needs no creature: a script's DANCE_CREATE of a dance (`value`, a row of the game's 25 dances) at the
  `point`, around the scenario's object `danceAround` or none, for `danceTime` (the number the script gives), through the
  native; the log gives the dance's object.

- `hand.stroked_holding_thing` and `hand.stroked_after_fleeing`: the lesson from a stroke, on what the creature's own
  mind just did (its planner, the creature not held): a tiger stroked while it holds a ball it picked up
  to examine, and a tiger stroked a second after it ran from the player (slapped first, so that it thinks less of the
  player). A command such as PickUp or FleeFrom would not do: the runner's own commands leave nothing in the mind to
  learn from.
- `ScriptDanceAttach` needs no creature: a script's FLOCK_ATTACH of the scenario's `object` to the dance the last
  `ScriptDance` made, not as its leader (the object, the dance and 0 pushed, as the original's FLOCK_ATTACH takes
  them), through the native; the log gives what it returned.
- `ScriptInterfaceLevel` needs no creature: a script's SET_INTERFACE_INTERACTION of a level (`value`, the game's 0..15)
  through the native; the log gives the level it is then. The level the scenario found is put back when it stops.
  `creature_mode.c_key_interface_level` presses C (the ZOOM_TO_CREATURE key, `PressKey`) under level 13 and again
  under the normal level 0: the game holds C back under the first.
- `OpenTattooEditor` needs no creature: the Creature Cave's tattoo editor opened on the cave's creature, as the cave's
  button opens it (`gui::OpenTattooEditorOnCreature`). While its dialog is open, the scenario's pointer is handed to it
  as the window's mouse is (`GameInterface::ProcessEvent`): the test runs drop the real mouse's events, so the pointer's
  moves and its button presses are given to the dialog directly. `creature_mode.cave_tattoo_editor` opens the cave on
  the tattoo page, then the editor, and drags the first symbol to the middle of the screen. Its ref shows today's
  behaviour: the dialog opens and the left release rewrites the skin, with no place under the pointer. It moves on
  purpose once the room draws its copy of the creature and the editor is told the places of its skin on the screen.
  Its clock runs on while the game is paused (`clockWhilePaused`, a tool rule): the temple's room pauses the game,
  and a scenario's clock otherwise stands still with it. `creature_mode.cave_tattoo_editor_dragging` is the same drag
  held: its shot comes with the left button still down, the symbol under the pointer.
- `objects.eat_what_is_not_food`: a hungry tiger eats a barrel (food value 0 in the shipped tables) and then a pile of
  food; the barrel is eaten and judged only as the eating ends: it pukes, its hunger is held back 20 s and the mobile
  objects' not-food count goes up; the pile is a meal.
- A fish farm is one of the scenario's objects (`FishFarmInfo`), made as a script makes one, bound to the nearest town
  and with its shoal in the water around it; a building may hold other than the scenario town's food (`storedFood`).
  raffclar's fish scenarios use them: `hand.fish_farm`, `hand.fish_scoop`, `hand.fish_scare` and `needs.fish_and_eat`,
  on the lake's southern shore. His `needs.fish_for_town` waits for his pond on the flat land.
- `CreateVortex` value 2 opens the volcano's mouth. raffclar's vortex scenarios open theirs with it, as he wrote them but
  through the vortex commands rather than his own vortex set-up, which does the same: `story.vortex_levels_the_bank`,
  `story.vortex_opens_a_hole` and `story.volcano_vortex_keeps_the_land`.
- raffclar's `objects.village_totem` and `objects.village_totem_worship` stand a Norse town centre's totem without a
  worship site and with one; his temple set-up is the temple fixture here, which builds the same temple.
- `story.bell_towers` and `story.undead_totem_drag` make the fourth land's puzzle totems as its scripts do, through
  the natives (`ScriptPuzzleTotem`: CREATE of a totem, its HEIGHT, SET_ID_PICKUPABLE), and `PuzzleTotemHeights` logs
  every puzzle totem's HEIGHT and MAX_HEIGHT ([the puzzle totem](../magic/puzzle-totem.md)).
- `mind.hungry_takes_what_it_may_eat`: a hungry tiger left to its mind, with only a barrel and a pot: the food
  choosers take whatever it can eat as the game's eating plans ask it (an object it can pick up), not only what is
  worth food, so it eats the barrel and is sick.
- `KeyDown` and `KeyUp` need no creature: a key of the keyboard (`value`, its SDL scancode) goes down or up through
  the game's input as the window gives a real key's (under `OPENBLACK_IGNORE_REAL_INPUT` it stays down until its
  `KeyUp`); a key still down when the scenario stops is let go then (`testbed_scenarios::HeldKeys`).
  `dialogue.skip_key` holds the keypad's Enter for a second over a text that waits for a click.
- `DoubleClick` is a double click on the command's creature, any player's, through the input Creature Mode reads: the
  cursor is kept over the creature's middle (through the fixed cursor) until the hand sees it under itself, then the
  second press of a double click with the left button goes to the game's actions as the window's own event (button
  down with two clicks, `testbed_scenarios::DoubleClickEvent`), and up again the next frame. Creature Mode takes it in
  that frame, as it takes a player's, when the camera's double-click flight is allowed. The log gives what the camera
  follows then, or that the hand never came over the creature (after 2 s). `creature_mode.other_god` double clicks
  another god's lion.
- `CameraKeys` holds Shift (or Ctrl, with `ctrl`) and a cursor key (`value`: left, right, up or down) for `amount`
  seconds, through the game's input as the window gives real keys, by the keys the game's bindings name (Tilt/Rotate On
  or Zoom On, and Move Left, Right, Forwards or Backwards; `testbed_scenarios::CameraKeysOf`). Creature Mode's camera
  reads them from the game's actions each frame; a new `CameraKeys` lets go of the last one's first, and those still
  held when the scenario stops are let go then. `creature_mode.orbit` turns, tilts and zooms round the player's tiger.

- `ScriptFocus` and `ScriptRelease` act on a creature through the scripts' own natives, SET_FOCUS at the `point` and
  RELEASE_FROM_SCRIPT, their arguments pushed on the scripts' stack as a script's call pushes them, so a run shows the
  native's creature branch and the creature's mind together; the log says whether the script controls it after each.
  `creature.focus_by_script` (900 frames) focuses a content tiger on a spot behind it 2 seconds in and releases it 8
  seconds later: it turns round to the spot, holds still with its head looking there, and once released its mind plans
  again; the shot, after the release, shows it still facing the spot.
- `ScriptPlay` gives a creature a script's play through the natives, SET_SCRIPT_ULONG of the action (`value`) and the
  times (`plays`), then SET_SCRIPT_STATE; `ScriptPlayed` asks PLAYED of it the same way, the log giving the answer.
  `creature.script_play` (900 frames) has a content tiger sneeze twice (62, an individual action) and, half a second
  later, another sit once (38, a static action): PLAYED is false for the first while it sneezes and true after, false
  for the second at every readout (the readouts end at 13.5 seconds, before its ten seconds' loop and its end are over).
- `ScriptMove` sends a creature to the `point` with a script's MOVE_GAME_THING through the native, within `radius`
  (0 for the game's own two legs). `creature.script_move` (900 frames) sends two content tigers 60 ahead, the left one
  with no distance (a first stop within a sixth of the way or its height, then within its height) and the right one
  within 5, their heads on their points' ground; 9 seconds later SET_FOCUS turns the left one back to where it
  started.
- `ScriptDoAction` forces an action of the game's table (`value`, its row) on a creature, done to the scenario's
  `object`, with a script's CREATURE_DO_ACTION through the native. `creature.script_do_action` (900 frames) has a
  content tiger look at a walking villager for good (LookForever) 2 seconds in, look at its feet without going up
  to it (LookButDontApproach) 3 seconds later, and sit at a rock (SitDown, under the script's control) 4 seconds after
  that: it walks there, faces the rock, then down the slope, and sits.
- `ScriptHealth` needs no creature: a script's GET_PROPERTY of HEALTH on the scenario's `object`, through the native,
  the log giving the value and whether it came back as a float. `script.health_of_living` reads three sheep (full
  life, half, a quarter) and two villagers (full, half) once each from 1 second in: each reads back its life as a
  float (1.000, 0.500, 0.250, 1.000, 0.500), so a script that stops at 0 or less, as Land 1's bleating sheep do,
  carries on for a living one.
  Nothing on the land changes, so the shot and the hash are those of the scenery alone.
- `ScriptChallengeSnapshot` needs no creature: a script's SNAPSHOT (`value` 0), UPDATE_SNAPSHOT (1) or
  UPDATE_SNAPSHOT_PICTURE (2) of challenge 90 with the command's success (`amount`) and `alignment`, through the
  native; the log lists every challenge record and the local player's alignment. `script.challenge_snapshots` makes
  the record (alignment unchanged), updates it to done (the alignment moves at once) and takes its picture (it stays
  done, its reminder script and arguments emptied). Nothing on the land changes.
- `creature.walks_past_creature_gate`: a small tiger walks from one side of a shut Norse gate to the other, which takes
  it round one end of the gate; once it has arrived, `SetOpenClose` with `waitUntilFree` opens the gate, and twelve
  seconds later the tiger walks back, through the middle of the gate once it stands open and still.
- `logSunGlare` logs a line each time it changes: how high the sun stands and which of its glare's samples the land
  hides from the camera, by their offsets from the sun, worked out by the same functions the renderer uses, from the
  camera's origin. What is drawn in front of a sample hides it too, but only the drawing knows that, so the log leaves
  it out. The facet Sky (`src/Debug/TestbedScenarioSky.cpp`) uses it: `sky.sun_glare_behind_bank` faces the sun from
  the lake's water at 9.54 h, when the lake's bank hides the two lower samples and not the others, with a pillar of
  rock in the water 40 units out in front of the sun.
- `sky.trees_by_day` and `sky.trees_at_night`: the same row of five trees (an oak, a beech, a birch, a pine and a
  cedar) and a pillar of rock, seen across the row from its south east, at noon and at midnight, the clock standing
  still; at night the hand is held by the east end of the row, where it carries the light the models are lit with.
  They show whether that light shades the trees: the game does not shade them, and does shade the rock.
- `sky.moon_midnight` and `sky.moon_night`: the moon seen from low on the land looking east, past a few trees, with the
  clock standing still at midnight, or running from 19:00 so that the moon rises in the south east, stands due east at
  midnight and sets in the north east. `sky.moon_over_sea` stands the camera 3 above the lake's water, looking level
  across it at midnight, so that the moon and its glow show in the sky and their mirror images in the water. The
  testbed's land is flat, so nothing stands farther off than the moon behind it: whether the moon hides what is farther
  is not shown here.
- `physics.burning_rock_split`: the six rocks of `physics.rock_split`, each set alight by a fire fixture of its object
  (`fires = {{.what = i}}`) before the first throw. With `OPENBLACK_FIRE_TRACE=1` the log tells each fire a half takes
  from its rock and the blaze it joins, and any fire that leaves its blaze as its object starts to move. In the
  original both halves start to fly before they take the fire, so they stay in the rock's blaze.
- `physics.immovable_let_go`: two rocks side by side on the field, the west one made immovable as a script's
  SET_ID_MOVEABLE false makes it (the object set-up's `immovable`); a second in, each is lifted 10 m up and let go of
  with no speed. The movable one falls and comes to rest; the immovable one hangs where it was let go, in the physics
  with no body and out of the map, as in the original (a bug kept: "An immovable thing let go hangs" in [Physics](../physics/physics.md#who-owns-what-class-hooks-and-the-hands-api)).
- `physics.rock_strikes_creature`: the player's tiger stands still in the middle of the field (its mind paused); a
  rock is thrown at it from the west every eight seconds, and a toy ball four seconds after each rock. The creature
  spawner shows its life, its attitude to the player and the fear and anger sources 18 and 9: a rock that strikes it
  hard enough lowers the first two and raises both sources; the toy ball changes none of them. The testbed's own
  table of creatures shows none of these yet.
- `growth.carrying_strength`: a small young tiger (size 0.3, too young to grow) and a big grown one (size 2), both
  with no strength, each pick up a pile of magic food and walk to and fro with it, their bodies' time twenty times as
  fast. A creature weighs its mass, from its size, fatness and strength, so the food is as heavy as the small one or
  more (its strength rises at the most it can) and a small share of the big one (it gains a little). With the
  creature tables' weight both gained alike. The spawner's body panel shows their strength.
- `growth.oversized_brought_back`: two tigers made at size 6, beyond the 4 a body is drawn at, a young one (stage 2,
  too young to grow) and a grown one. Both keep the size they are made with and are drawn at 4; on its first growth
  turn the grown one is put back to 2 and drawn at 2, the young one keeps 6. A scenario may make a creature at any size
  of 0 or more; its body is drawn between 0.05 and 4. The testbed's table of creatures shows their own sizes.
- **Which creature the player leads is a rule of the testbed's, not the game's.** The game makes each new creature
  its owner's creature, the newest winning, and a player who leads creatures leads that one. The scenarios are written
  for the testbed's own rule instead: once all of a scenario's `creatures` are made, the first creature of each owner
  who leads creatures is the one they lead, and the others aren't (`Scenario::firstCreatureLeads`, on unless a
  scenario turns it off). It is applied once, after the creatures are made. A creature's own `leashable` (`Leadable()`
  in the casting scenarios) and the `MakeLeashable` command have the last word over it. A crowd's creatures get the
  same rule once the whole crowd is made, for the owners the scenario has no creature of.
- `leash.newest_takes_the_slot`: two tigers of the player, the left one made first, with `firstCreatureLeads` turned
  off, so the game's rule decides. The hand taps the left one, which is refused, then the right one, made last, which
  goes on the leash; each one's leash state is logged after its tap. It is the only scenario with the rule off.
- `RemoveCreature` takes the creature out of the game for good, through the whole removal a script's DELETE of it
  goes through. The creature keeps its place in the scenario's `creatures`, so the numbers of the others stay; a later
  command to it is logged as "it is gone" and does nothing. A scenario that removes all its creatures stops, as when
  another land is loaded over the testbed.
- `leash.spare_creature_removed`: two tigers of the player, the left one led by the testbed's rule and tapped onto the
  leash. The right one is removed, a leash state command to it is logged as gone, and the left one's leash state is
  logged again before it is tapped again. The game empties a player's creature whichever of theirs goes, so the left
  tiger is no longer the one the player can lead: openblack takes its leash off, and the tap is refused.

### Where to add it

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

### Replaying a hand demo

A scenario can play one of the game's recorded hand demos (`Data/HandDemo/<name>.hnd`), from a time into the scenario:

```cpp
// examples.hand_throw_target: a food pile where the demo first presses, a village where it last presses
.fixtures = {.villages = {{.at = fixtures::DemoPress {.press = 3}}},
             .piles = {{.type = PotInfo::FoodPile, .at = fixtures::DemoPress {.press = 0}}},
             .handDemo = fixtures::HandDemo {.name = "castfood"}},
```

- The demo moves the camera, the cursor and the buttons, as the tutorial plays it. The game's playback is not changed.
- A recorded action click on a thing lands on the nearest thing of the recorded kind within 3 m of the recorded place,
  as the game's does, so a fixture of that kind put near a recorded click takes it. `hand.demo_click_retarget` puts a
  wood pile 2.5 m off where `givewood` takes wood.
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

### Scenarios that write into the game's folder

Some scenarios write into the game's folder: a land change (the `ChangeLand` command, as in
`examples.land_change_creature`) saves the creature's mind and physique there. Such a scenario sets
`writesGameData = true`, and it runs only on a copy of the game's data, which carries the marker file
`openblack_game_data_copy.txt` in its root. On the game itself the runner logs a problem and does not start it. Never put
the marker in the real game's folder; run such scenarios with `--game-copy` ([Running one and comparing](#running-one-and-comparing)).

### Running one and comparing

A scenario is checked like a fidelity run (see [Behaviour is identical](testing.md#behaviour-is-identical-the-fidelity-run)): the same build twice, under the same
conditions, then compare. The local script `scenario.sh` does this:

```sh
sh scenario.sh run <tree> <id> <label> [--frames N] [--game-copy]
sh scenario.sh ref <label> <id>
```

- `run` copies the tree's `openblack.exe` and its DLLs to a private folder with no `Mods` folder, and runs
  `--testbed --scenario <id>` at 1024x768 for N frames (900 by default). It uses a 16 ms fixed step and `--seed 1`,
  ignores the real mouse, records the random trace and the state hash every turn, and takes a shot 20 frames before the
  end. `<tree>` is a worktree name or `deployed`, the exe the last joint deployed (`main` is refused: it is not built at
  the joints).
- The run's files are `<label>_<id>.log`, `<label>_<id>_hash.txt` and `<label>_<id>.png` in the script's audit folder.
  If there is a reference `ref_<id>`, the script compares the state hashes without `pools`, the random traces, and the
  shots byte for byte. Exit 0 means identical, 1 different, 3 that there is no reference yet, and 4 the known water
  draw below. Exit 5 is an incomplete run (no shot: the game stopped early) and 6 a crash on a GPU fault (the driver
  lost the device: run it again); neither is a measurement. Exit 2 is a bad call.
- A reference remembers the frames and the game copy it was made with, so a run takes them unless it gives its own.
- `--game-copy` runs on a copy of the game's data, one per session (`OB_SESSION`), made on first use with its marker.
  The files a run may write are restored from the game before every run.
- `ref` makes a run the reference `ref_<id>` in place of the old one: every reference has one copy. A reference is
  made from two identical runs, and only on the coordinator's order.
- Compare the shot only for scenarios whose camera does not move with something random.
- **Explained: the GPU driver draws the water, and land close to the camera, slightly differently from run to run.**
  A shot can differ from its pair while the state hash and the random trace are identical: twice in about thirty runs
  in the lake's water and shallows (`examples.leashed_creature`, 448 pixels; `examples.storm_lightning`, 476), each
  pixel off by 1 in one channel; once in `miracles.gesture_circle_shield`, water only, up to 5 per channel; and between
  two single launches of the Land 1 `drag` hand demo, 204 isolated land pixels near the camera, off by 1 with a random
  sign (the hand check compares hashes only, so it had not shown). On the same exe with `-b Direct3D12`, three `drag`
  runs and two `examples.storm_lightning` runs gave pixel-identical land and water; they differed only inside the FPS
  overlay's box at the top right, which shows the wall clock and is drawn into Direct3D 12 shots only. So the flakes
  are Vulkan's on this machine's integrated GPU, not the game's. The checks stay on Vulkan. The script reports off-by-1
  water alone as exit 4: the hash and the trace identical, every differing pixel off by at most 1 and within 40 pixels
  of clear water in the reference. Anything else is exit 1; a larger water-only or close-land difference with the hash
  and the trace identical is rerun once, and a match on the rerun is this driver effect. Always compare the hash and
  the trace before the shot. A Direct3D 12 comparison has to mask or hide the FPS box. A joint's run judges the scenes
  on the references' list of known shot flakes by their state hash alone.

### Running a list in one launch

`--scenarios` runs several scenarios in one launch, to save the start-up and the shutdown of a launch for each:

```sh
openblack -g <game> -W 1024 -H 768 -n 900 -l <log> -L all=debug -L game=trace \
    --scenarios examples.storm_lightning,examples.leashed_creature:600 --scenarios-out <folder>/<label>_
```

- Each entry is a new game, as a new launch starts it: the last game's services go and are made again in the start-up's
  order, with a new registry, the clock, the frame count and the random seeds as at a launch, and the configuration as
  the command line made it. The window, the renderer, the file system, the asset caches and info.dat's values stay
  (`Game::EndGame`, then `Game::StartNewGame`).
- Each entry takes its state hash in `<prefix><id>_hash.txt`, whatever `OPENBLACK_STATE_HASH` says, and its shot 20
  frames before its end in `<prefix><id>.png`, as `scenario.sh` names a single run's. An id given again names its
  outputs `<prefix><id>.2`, `.3` and so on. `--scenarios` goes without `--scenario` and `--screenshot-frame`.
- The run has one log. Each entry's part of it starts at the line `Scenario list: entry <n> of <count>, <name> begins`
  and ends at `... <name> ends`; the next game's start-up is in the next entry's part. The first mark comes after the
  launch's start-up, whose lines go to the first entry. Cut there, each part reads as a single run's log.
- A quit ends the list: the window closed, an unknown id, or a benchmark measured (put a benchmark last).
- An entry can be a land instead of a scenario: `land:<file>:<frames>` followed by any number of `:<key>=<value>`
  options, for example `land:Land1.txt:900:demo=Gestures:skip=3/30`. It is a new game on that land, which loads as a
  launch's start level (`-s`) does: the land, the challenge script with LandControlAll, the SkipBox and its pause, the
  test hooks. `<file>` is the land's script under the game's `Scripts` folder; `<frames>` is required. Its outputs are
  named after its demo, else after its land's stem (`<prefix>Gestures_hash.txt`, `<prefix>Land1.png`), so that the
  entry of a list run with `--scenarios-out <folder>/<label>_` has the names of a single run labelled `<label>_<demo>`.
  The keys (one table, `LandOptions` in `src/Debug/TestbedOptions.cpp`):

  | Key | Value | In place of | What it does |
  |---|---|---|---|
  | `demo` | `<name>` | `OPENBLACK_TEST_HAND_DEMO` | plays `Data/HandDemo/<name>.hnd` where and when the test hook plays the variable's, once the land exists |
  | `skip` | `<answer>/<frame>` | `OPENBLACK_TEST_SKIP_ANSWER` | answers the SkipBox (0 to 3) at that frame of the entry's own run, counted from its frame 0 |
  | `menu` | `<frame>` | `OPENBLACK_TEST_MENU_AT` | opens the escape menu at that frame of the entry's own run, counted from its frame 0 |
  | `citadel` | `<in>[/<out>]` | `OPENBLACK_AUDIO_TEST_CITADEL` (`<in>[,<out>]`) | enters the temple at the entry's hook turn `<in>` (above 0), and leaves it at `<out>` (after `<in>`) |
  | `timeline` | `<id>[/<id>...]` | (none: the testbed's own) | plays those scenarios' commands on the loaded land, one after the other ([Playing a timeline on a land](#playing-a-timeline-on-a-land)); the entry's outputs are then named after the first id, unless `demo=` names them |

  A value with several numbers separates them with `/`, because `,` separates the list's entries and `:` an entry's
  options. An unknown key, a key given twice or a malformed value refuses the whole list, and the error lists the known
  keys. A key the entry does not give leaves its variable as it is: without `skip=`, `OPENBLACK_TEST_SKIP_ANSWER` still
  answers the box. A new key is a row of the table, its field in `LandRun` (`src/Debug/TestbedOptions.h`) and the one
  place in the game that reads it.
- Not in a list: the scenarios that write into the game's folder ([Scenarios that write into the game's folder](#scenarios-that-write-into-the-games-folder)). They change what the next entry reads
  from it, which a single run restores before it starts. The game refuses a list with one of them before its first
  entry, with one log line, and runs nothing.
- The local script `scenario_batch.sh` runs a list the way `scenario.sh` runs one, and compares each entry with its
  reference:

```sh
sh scenario_batch.sh run <tree> <label> [--reverse] [--twice] <id>[:N] [<id>[:N]...]
sh scenario_batch.sh prove <tree> <label> [--reverse] [--twice] <id>[:N] [<id>[:N]...]
```

  Its outputs are the single runs' (`<label>_<id>.log`, `_hash.txt`, `.png`), and so is each entry's exit status. It
  takes each entry's frames from the entry (`<id>:N`), else from its reference, else 900, and refuses an entry whose
  reference ran on the game's copy (exit 2). `--reverse` runs the list backwards and `--twice` runs each entry twice in
  a row. `<tree>` may also be `dir:<folder>`, any folder that holds an `openblack.exe` and its DLLs.
- What shows that the new game starts as a launch does: each entry identical to its single run on the same build, with
  `pools` too; the same list in the reverse order; and one scenario twice in a row. A difference that depends on the
  order is something the new game kept from the last one. `prove` does this: it runs each entry alone with
  `scenario.sh` (as `<label>_single`), then the list, on the same exe, and compares each entry with its single run: the
  state hash files whole, the random trace and the shot byte for byte. The comparison with the reference is shown as
  information.

### Playing a timeline on a land

A land entry's `timeline=<id>[/<id>...]` plays testbed scenarios' commands on the land the entry loaded, as the story
runs on it: the Land 1 play-through check drives its chapters this way. The scenarios play one after the other, the
next once the last has given all its commands, and they share one log (`Testbed:` lines, with `Land timeline <n> of
<count>: <name>` as each starts and `the land timeline is done` at the end):

```sh
openblack -g <game copy> -W 1024 -H 768 -n 20000 -l <log> -L game=trace \
    --scenarios land:Land1.txt:20000:skip=0/30:timeline=story.t1_opening --scenarios-out <folder>/<label>_
```

- The land is not loaded afresh and nothing is set out on it: a scenario written for a land (`onLand`) has no
  creatures, objects, fixtures, crowd, town or hand hold of its own, and the testbed refuses to run it. Its points
  (`point`, a pick's point, the camera's `eye` and `look`) are map coordinates, x east and y the map's z.
- Creature 0 is the player's creature, the one they lead, looked for again every frame: none until they have one,
  then whichever they have after a change of land. A command on another creature is refused by the land's rules.
- A command on an object picks a thing of the land instead (`Command::thing`, `ThingNear`): the nearest of a script
  object type and sub-type (the scripts' "any", 5000, for any) to a point, within a radius, by the same type and
  sub-type filter as a script's `CALL_NEAR`, but without the script taking the thing up. `LayGateStone` picks its
  plinth the same way (`Command::other`). So `HandTake`, `TieLeash`, `PickUp`, `PutDown`, `Kick`, `KnockDown`,
  `AbodeBuilt`, `ScriptHealth`, `ScriptDelete` and `HighDetail` act on the land's own things.
- A change of land, by the `ChangeLand` command or by the story's own `LOAD_MAP`, does not end it, and when it stops
  nothing of the land's environment is put back (the clock, the creatures' body time, fainting, fights, footprints):
  it set none of them. The camera, the pointer, the keys and the clicks it took are given back.
- The scenarios a timeline names must exist: an unknown id logs the list of ids and the game quits, playing none.
- A land run writes into the game's folder when its land changes (the creature is saved to the profile), so a land
  timeline that changes land runs on a game copy, as every story run does.

The commands written for it (each is a testbed command, so a testbed scenario may use them too):

| Command | Fields | What it does |
|---|---|---|
| `WaitGlobal` | `global`, `compare`, `amount` | waits until the script global holds up against the value (`==`, `!=`, `<`, `<=`, `>`, `>=`) |
| `WaitTask` | `script`, `value` | waits until a task of the script runs (0), none runs (1), or none runs after one was seen (2) |
| `WaitText` | `value`, `read` | waits until the text (its id) is among the dialogue's last six, and with `read` until it no longer waits for a click |
| `SetGlobal` | `global`, `amount` | sets the global, keeping its type |
| `StartScript` | `script` | starts the script as a new task, as the LHVM viewer's Start; only one that takes no parameters |
| `LogGlobals` | `global`, `script` | logs each global named (separated by spaces) and whether each script named runs: a chapter's end check |
| `ClickTexts` | `value` | from now on (1), or no longer (0), a text waiting for a click is clicked on once the dialogue takes the click, as the player's click; `OPENBLACK_TEST_TEXT_CLICK` does the same for every text of a run, by the game's turn |
| `CameraTo` | `eye`, `look` | puts the camera at the eye, looking at the point; each is x and z, and a height above the ground there |
| `CameraFollow` | `thing` or the creature, `amount`, `value` | keeps the camera behind and above it, `amount` times a follow shot's distance, until value 1 lets it go |
| `SetCreatureSize` | `amount` | the creature's size, as the creature spawner's size slider sets it |
| `MoveThing` | `thing` or the creature, `point` | a script's `SET_POSITION` there, through the native: a thing is put on the ground there, the creature placed |
| `HandOver` | `point`, `value` | keeps the pointer, so the hand, over that point of the land wherever the camera goes, until value 1 lets it go |
| `HandDrop` | | the hand is made to let go of what it holds, where it is |
| `WaitCamera` | `point`, `radius` | waits until the camera is within the radius of the point of the land, at the land's height there, measured in three dimensions as a script's distance from the camera to a marker is (the tutorial's follow ends at a camera distance) |

The waits wait until free (`waitUntilFree`, which the rules require), so the timeline follows the story rather than
the clock, and a delay counts from when the wait is over. What was reused rather than written: `TapHighlight` (the
script highlight nearest a point, in place of the run-wide `OPENBLACK_TEST_TAP_SCROLL` index), `WalkTo` and `RunTo`,
`SetPhase`, `TempleBuilt` (the player's temple built this far), `AbodeBuilt`, `HandTake` and `HandGive`, `TieLeash`,
`PickUp` and `PutDown` (through the creature's own hands), `ScriptCreate` (a pot of food or wood, then `HandTake`,
`HandOver` and `HandDrop` to put it down at a point), `PointerPress` and `PointerRelease` (the Action button is 3),
`ShowText`, `ScriptMove`, `ChangeLand` and `SetGameSpeed`. Not written: putting a thing straight into the creature's
hand, since the game's only way is the creature's own `PickUp`.

The rules a land scenario is checked by (`Problems`, then `LandProblems` in `TestbedScenarioRegistry.cpp`) and the
pure parts (the points' origin, the next scenario, the comparisons, the reading of globals, tasks and texts, the pick)
are in `src/Debug/TestbedLandTimeline.{h,cpp}`, tested in `test/test_testbed_land_timeline.cpp` with fakes.

The chapters written so far (facet Story, `src/Debug/TestbedScenarioStory.cpp`), each ending with a `LogGlobals`
line that is its end check:

| Id | Chapter | Starts from | Ends when |
|---|---|---|---|
| `story.t1_opening` | Land 1, the opening to the hand-over | `skip=0/<frame>`: the tutorial plays | text 5219 is read; logs the setup's globals (`IsSkippingToCreatureSelect=0`, `LandNumber=1`, `GateKey2` set) and the tutorial's later steps still 0 |
| `story.t2_follow` | Land 1, following the family | after the opening | the camera, following the family, is let go within 45 of the walk's end (the camera file's point 219); text 5193 shown |
| `story.t3_tunnel` | Land 1, the second cinematic and the way to the temple | after the follow | the camera put at the lessons' spot ends the tutorial's wait for it; text 5196 read, `ProtectGateKeysFlag=1` |
| `story.t4_lessons` | Land 1, the camera lessons and the pick-up demo | after the way to the temple | the turn key (Z), the tilt key (A) and the zoom key with the forward and back keys held through the lessons; `PickupDemoPlayed=1` |
| `story.t5_temple_built` | Land 1, the temple built | after the lessons | nothing is done: the town's builders finish the temple; `CitadelBuiltL1=1` and the tutorial returns |
| `story.t6_temple_finished` | Land 1, The Temple Is Finished | after the temple is built | the player goes in through the entrance and out with Escape, from a recording of their mouse and keys; text 5232 read, CitadelGuide returns, ChooseYourCreature running |
| `story.c1_tiger_stone` | Land 1, Choose Your Creature's tiger stone | after the temple's chapter, in the same timeline | `GateKey2Present=1` and The Lost Brother running |

The SkipBox's answers 1 to 3 delete the gate stones and go straight to the glade, so the tiger stone is only played
after the chapters before it. The whole chain plays in one launch:

```sh
openblack -g <game copy> -W 1024 -H 768 -n 160000 --seed 1 --replay-input <recording> \
    --scenarios land:Land1.txt:160000:skip=0/30:timeline=story.t1_opening/story.t2_follow/story.t3_tunnel/story.t4_lessons/story.t5_temple_built/story.t6_temple_finished/story.c1_tiger_stone \
    --scenarios-out <folder>/<label>_
```

The recording is made once with `--record-input` in place of `--replay-input` and without
`OPENBLACK_IGNORE_REAL_INPUT`, the player keeping the mouse out of the window until text 2000 ("This is the entrance")
and then tapping the entrance and pressing Escape inside. A command on the mouse (`PointerTo`, `PointerPress`,
`WheelTurn`, ...) keeps the pointer for the rest of the timeline, so the chapters before a recorded moment use keys
only.

Found by the chain: the waits for a task's end (`WaitTask` value 2) of the tunnel and of the tutorial use "not running"
(value 1), since the camera put at the temple, and the check that finds the temple built, end those tasks in the
scripts' same turn, before a wait for their end has seen them run.

With `OPENBLACK_STATE_HASH_SCRIPTS=1`, the state hash has a `scripts` part: every script global and every task's script
and instruction, so two runs of a chapter reaching the same quest state can be told apart from two that do not
([Research tools](research-tools.md#hashdiff-which-entity-moved-first)).

### The Testbed Scenarios window

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

### Limits

- In a list, an entry after the first gets none of the window's own events of a launch (shown, exposed, focus, the
  pointer entering) in its frame 0, as the start of a new game empties the event queue. Nothing in the state hash is
  known to depend on them; the reversed and doubled lists of `scenario_batch.sh prove` are what shows it.
- `hand` (raffclar's `HandHold`) holds the player's hand still for the whole scenario over the point of the land at
  `offset` from the middle of the map, as a player holds the mouse there: each frame the cursor is put where that point
  is on the screen, through the same fixed cursor the hand demos use, wherever the camera has gone (not while a sweep
  or a gesture moves the pointer). The hand is then at the game's own height over the point, as the mouse puts it, so
  `height` is not used: his runner sets the miracles' hand frame instead (`DriveHand`), with the hand `height` above
  the point and looking straight down. Its consumers are game code that only a scenario would drive, so they are not
  ported, and the hand stands where a player's would.
- Parts of a scenario's set-up that this tree cannot do yet are skipped, with one log line `not in this tree yet: ...`:
  tribal power (`miracles.hand_tribal_power`), prayer power (`miracles.shield_runs_out`), the villagers' walks to
  worship through the teleport stones (`miracles.teleport_worship`), and seeds summoned from worship into the hand for a
  miracle cast by hand (`miracles.hand_extreme_lightning`, `miracles.hand_press_too_soon`,
  `miracles.hand_worship_and_creature`). Such a scenario still runs, without that part. No command logs the line any
  more: the commands for catching a fireball or taking one into a fire seed, shaking a leash off and the hand stroking
  or slapping a creature are played, and so are the scenario data's dispensers, its other miracle casts, the miracles'
  position log, the villagers' walks and drops on teleport stones, and particle colours by player.
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
- `miracles.creature_casts_wood`, `creature_casts_wood_by_workshop`, `creature_shield_idles`,
  `creature_casts_forest` and `creature_casts_to_impress` have a script force the action's row on the tiger
  (`CREATURE_DO_ACTION`), as raffclar's scenarios force it by name: the impress row and the workshop's casts no miracle
  of its own in the game's table, so `TellCast`, which goes by the miracle, cannot reach them; a script's plan is about
  nothing, so the forced shield idles. `miracles.creature_casts_shield` is told through `TellCast`, about its target.
  The town's shield (row 286) is reached from the creature section of the debug window ("Casting for a town"), as
  `TellCast` keeps to row 54 for the shield.
- The creature casting scenarios (`miracles.creature_casts_itchy`, `creature_casts_lightning`, `creature_casts_fireball`,
  `creature_casts_explosion`, `creature_casts_heal`, `creature_cast_fizzles`, `creature_too_tired_to_cast`) tell the
  creature to cast through its mind's `TellCast`, and
  it goes about it as it would by itself. `miracles.creature_heals_by_itself` and `creature_heals_town_villager` are
  not told: the first's villager belongs to no town, so its tiger's compassion has nothing to do; the second's tiger
  may choose the heal for the hurt farmer of its village, on a coin's toss each time it plans. The first is run, and
  its reference made, with `--frames 1800`; the second with `--frames 450`, so that its shot catches the heal, cast
  some five seconds in.
- `miracles.creature_casts_food_on_town` tells the tiger to cast food at a Celtic storage pit beside two huts. The
  testbed makes every building of a scenario in the scenario's town, and a whole storage pit becomes its town's
  pit as it is made, so the pit is the town's and the food that lands in it is the town's food.
- `miracles.creature_waters_field` tells the tiger to cast water at a wheat field of the scenario's town, made
  newly sown, so its food is nothing and it is a field that needs watering.
- `miracles.creature_casts_amusing_spell` has a script's do-action (`ScriptDoAction`, row 297, `atCreature`) tell the
  tiger to amuse the cow with one of the two spells it knows, picked at random; the script's plan does not ask whether
  the cow seems friendly.
- `miracles.creature_itchy_on_leashed_creature` is the itchy cast at a tiger that thinks for itself and is on the rope
  leash (its wants are not held, unlike the cow's in `creature_casts_itchy`), so the spell's hold on a creature shows:
  the leash comes off and scratching becomes what it wants most. Its turn to scratch is the planner's, so it is
  measured rather than written down.
- The `FightCast` command, which queues a spell as the fight's next move (`combat.fight_casts`), is this tree's own:
  raffclar's testbed has none, and his fight spells come from the creature spawner's fight window.
- A new command kind is a new value at the end of `Command::Kind` (before `Count`) and one row at the end of the
  command table (`k_CommandKinds` in `TestbedScenarioRegistry.cpp`): its name, and whether it needs no creature or is
  the player's mouse. `Name`, `NeedsNoCreature`, `IsPointerCommand` and the tests read the table, and a compile-time
  check refuses a table that misses a kind or has one out of order. The others keep their numbers, and the logs that
  show them stay as they were. Then its case in the runner.
- A scenario has no pass or fail of its own. Its checks are the unit tests and the comparison with its reference.

### Test hooks

- `--testbed`, `--scenario`, `--scenarios`, `--scenarios-out` and the benchmark switches ([Starting it](#starting-it)).
- A land entry's keys stand in for `OPENBLACK_TEST_HAND_DEMO`, `OPENBLACK_TEST_SKIP_ANSWER`, `OPENBLACK_TEST_MENU_AT`
  and `OPENBLACK_AUDIO_TEST_CITADEL` ([Running a list in one launch](#running-a-list-in-one-launch)); its `timeline=`
  plays scenarios on the land ([Playing a timeline on a land](#playing-a-timeline-on-a-land)).
- `OPENBLACK_STATE_HASH_SCRIPTS=1` adds the state hash's `scripts` part.
- `OPENBLACK_FIRE_TRACE=1` logs the fires of `physics.burning_rock_split`.

## Pending

- The parts of a scenario this tree cannot do yet, listed in [Limits](#limits): the grid of every miracle's dispenser
  (open issue #188), extra prayer power, tribal power, the villagers' walks to worship through the teleport stones, and
  seeds summoned from worship into the hand for a miracle cast by hand.
- `creature_mode.cave_tattoo_editor` moves on purpose once the room draws its copy of the creature and the editor is
  told the places of its skin on the screen.
- `needs.fish_for_town` waits for its pond on the flat land.
- Land timelines: the tutorial's chain (T1 to T6 and C1) has played through on Land 1, `CameraTo` and `CameraFollow`
  holding while the player's camera model runs. Unverified: that `ClickTexts`'s clicks, given in the frame rather than
  the game's turn, end each text's wait at the same turn as `OPENBLACK_TEST_TEXT_CLICK`'s; and the replay of the
  recorded temple entry on a later build.
