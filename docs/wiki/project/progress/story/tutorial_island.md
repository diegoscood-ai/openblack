# Tutorial island (the Gods' Playground)

A separate small island, the Gods' Playground, where the Island Keeper walks the player through the controls in six
lessons: dragging and turning, tilting, all of them together, zooming, picking things up and double clicking. It is not
part of the story: the player goes there from the first land with F2, which the advisors suggest, and comes back with
Escape. The story's own teaching, in the opening of the first land, is in tutorial.md.

The game over cannot happen here, as the player has no temple: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 1/53 done, 0 chosen, 38 partial, 0 inferred — 26%**

## Going there and back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors tell the player about the Gods' Playground and that F2 takes them there | pending | the lines are in the land control scripts, which never get there; the advisors themselves work (see advisors.md) |
| F2 asks "Are you sure you want to go to the Gods' Playground?" | pending | openblack has no F2 binding for it |
| F2 does nothing in a multiplayer game, while a script holds the cinema bars, or when already there | pending | openblack has no F2 binding for it |
| Yes: the game is quick-saved to a reserved slot, every script stops and the playground island loads | pending | no F2 trip and no saved games; see ../engine/saving_and_loading.md |
| The island's own script starts and the interface is put back to normal | pending | never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; see ../scripts/landT_script.md |
| No closes the question and play goes on | pending | no F2 question |
| Escape stops the lesson running; Escape again asks "Are you sure you want to leave the Gods' Playground?" | pending | no F2 trip; the island's scripts ask `KEY_DOWN` only for the space bar, so Escape is the game's own key handling, not read |
| Leaving reloads the quick-saved game, putting the player back where they were ("back to Eden") | pending | no saved games |
| The island can be loaded on its own | partial 60% | the land loads from the debug menu's lands (`Game.cpp`), without its lessons; re-measured 2026-10-21: it loads from the debug menu, without its lessons |

## The island

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small land of its own, numbered 6, with a mountain, a valley path, a ditch and an offshore islet | done | the land loads (`Data/Landscape/LandT.lnd`); see ../scripts/landT_script.md; re-measured 2026-10-21: the land loads |
| Two of the player's towns (Japanese and Celtic) and a neutral Indian one, with houses, fish farms and fields | partial 80% | loaded by the land script; see ../town/; re-measured 2026-10-21: made by the land script; see ../town/ |
| Hundreds of animals, trees, big forests, fireflies and lanterns | partial 50% | see ../animal/, ../nature/; re-measured 2026-10-21: its share follows the pages it points to |
| The player's influence covers the island (a ring of radius 1000 at its middle) | partial 30% | `InfluencePosition` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The Island Keeper, a monk who can't be picked up, moved or hurt | partial 30% | `CREATE` makes the monk (a villager) and the three flags are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| A field of miracle dispensers, one for every miracle: the 25 player miracles in four rows and the 16 creature spells in two | partial 30% | `CREATE` of spell dispensers is real (`magic::script::CreateSpellDispenser`); see ../miracles/dispensers_and_seeds.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |

## Running the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lessons run in order, each starting when the last is done | partial 30% | plain script order in `LandControlT`; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Space restarts the lesson running | pending | `KeyDown` is real (the space bar held); never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Each lesson limits the interface to what it teaches | partial 30% | `SetInterfaceInteraction` is real; see ../hand/; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Each lesson's lines are said by the Island Keeper, and wait to be read | partial 30% | `RunText` and `TextRead` are real; see advisors.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| A lesson that the player strays from has the Keeper call them back ("You are straying! Try and keep me in view!") | partial 40% | `GameThingFieldOfView` and the distances are real; the view test `GameThingCanViewCamera` is a stub; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: part of what it calls is a stub (see the notes) |
| Each line names the mouse button or key to press, as the player has bound it | pending | `SetHandDemoKeys` is empty; see ../interface/key_bindings.md |

## Lesson 1: moving and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper welcomes the player and sets off round the mountain, beckoning | partial 30% | villager walks and animations are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Only grabbing and dragging the land works | partial 30% | `SetInterfaceInteraction` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The player keeps up by grabbing the land and pulling it | partial 30% | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The Keeper waits while he is out of view or more than 100 away | pending | the view test `GameThingCanViewCamera` is a stub |
| Then turning: the hand at the screen's left edge shows the rotation arrows; holding the button and moving the mouse turns the view | partial 30% | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Only turning and dragging work until the player has turned the view the way asked | pending | the rotation check `WithinRotation` is a stub |
| The evil advisor reminds the player how to turn if they forget | partial 30% | advisors and texts are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| A timed round: follow the Keeper round the mountain in under five minutes; he stops if the player is more than 50 away or ahead of him | partial 30% | timers and distances are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Too slow: "Oh. Bad luck" and the round starts again | partial 30% | `RunText` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |

## Lesson 2: tilting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper climbs a path and the player follows by tilting the view | partial 30% | villager walks are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Only tilting works | partial 30% | `SetInterfaceInteraction` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The hand at the screen's top edge shows the arrows; holding the button and moving the mouse tilts the view | partial 30% | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Then the Keeper walks down again and the player keeps him in view | partial 30% | villager walks and field-of-view are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Too slow or straying: the step starts again | partial 50% | the same script; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: as the row above: the same script |

## Lesson 3: everything together

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper walks round a valley path and the player follows with every control | partial 30% | villager walks are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| He waits when out of view or too far | pending | the view test `GameThingCanViewCamera` is a stub |

## Lesson 4: zooming

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding both mouse buttons and moving the mouse zooms | partial 30% | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The player zooms out to see the whole island, then back in | partial 30% | the camera distance reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Two rocks are placed as marks to zoom between | partial 30% | `CREATE` makes rocks; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The keyboard and the mouse wheel zoom too | pending | `HasMouseWheel` is a stub (answers no); the player camera control itself: see ../camera/world_camera_controls.md |

## Lesson 5: picking up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five lost teddy bears to pick up and drop in a ditch | partial 30% | `CREATE` makes mobile objects; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Picking up: the hand over a bear, the Action button held and the mouse moved | partial 30% | see ../hand/picking_up.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Dropping: the Action button again | partial 30% | see ../hand/holding.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| All five in the ditch ends the lesson | partial 30% | `CallNear` and the counts are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |

## Lesson 6: double clicking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Double clicking the Move button somewhere flies the camera there | partial 30% | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The player double clicks the islet offshore to fly there | partial 30% | the camera position reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Getting there any other way: "Hey! You didn't double-click to get there!" and try again | partial 50% | the same script; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: as the row above: the same script |
| Then double clicks the mountain top to fly back | partial 30% | the camera position reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |

## After the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper says that was all and the player may stay and practise | partial 30% | `RunText` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| Six gold scrolls stand by the start, one per lesson; tapping one runs that lesson again | partial 30% | `CreateHighlight` and `GameThingClicked` are real; see ../interface/scrolls_and_signs.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The Keeper goes; each replay brings its own Keeper | partial 30% | villager `CREATE` and `ObjectDelete` are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
| The miracle dispensers stay for practising miracles | partial 30% | see ../miracles/dispensers_and_seeds.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; re-measured 2026-10-21: the natives it needs work; the island's control script is never started |
