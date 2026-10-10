# Tutorial island (the Gods' Playground)

A separate small island, the Gods' Playground, where the Island Keeper walks the player through the controls in six
lessons: dragging and turning, tilting, all of them together, zooming, picking things up and double clicking. It is not
part of the story: the player goes there from the first land with F2, which the advisors suggest, and comes back with
Escape. The story's own teaching, in the opening of the first land, is in tutorial.md.

The game over cannot happen here, as the player has no temple: [losing_and_game_over.md](losing_and_game_over.md).

**Progress: 0/53 done, 39 partial — 37%**

## Going there and back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The advisors tell the player about the Gods' Playground and that F2 takes them there | todo | the lines are in the land control scripts, which never get there; the advisors themselves work (see advisors.md) |
| F2 asks "Are you sure you want to go to the Gods' Playground?" | todo | openblack has no F2 binding for it |
| F2 does nothing in a multiplayer game, while a script holds the cinema bars, or when already there | todo | openblack has no F2 binding for it |
| Yes: the game is quick-saved to a reserved slot, every script stops and the playground island loads | todo | no F2 trip and no saved games; see ../engine/saving_and_loading.md |
| The island's own script starts and the interface is put back to normal | todo | never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT`; see ../scripts/landT_script.md |
| No closes the question and play goes on | todo | no F2 question |
| Escape stops the lesson running; Escape again asks "Are you sure you want to leave the Gods' Playground?" | todo | no F2 trip; `KeyDown` is a stub |
| Leaving reloads the quick-saved game, putting the player back where they were ("back to Eden") | todo | no saved games |
| The island can be loaded on its own | partial | the land loads from the debug menu's lands (`Game.cpp`), without its lessons |

## The island

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A small land of its own, numbered 6, with a mountain, a valley path, a ditch and an offshore islet | partial | the land loads (`Data/Landscape/LandT.lnd`); see ../scripts/landT_script.md |
| Two of the player's towns (Japanese and Celtic) and a neutral Indian one, with houses, fish farms and fields | partial | loaded by the land script; see ../town/ |
| Hundreds of animals, trees, big forests, fireflies and lanterns | partial | see ../animal/, ../nature/ |
| The player's influence covers the island (a ring of radius 1000 at its middle) | partial | `InfluencePosition` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The Island Keeper, a monk who can't be picked up, moved or hurt | partial | `CREATE` makes the monk (a villager) and the three flags are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| A field of miracle dispensers, one for every miracle: the 25 player miracles in four rows and the 16 creature spells in two | partial | `CREATE` of spell dispensers is real (`magic::script::CreateSpellDispenser`); see ../miracles/dispensers_and_seeds.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |

## Running the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lessons run in order, each starting when the last is done | partial | plain script order in `LandControlT`; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Space restarts the lesson running | todo | `KeyDown` is a stub |
| Each lesson limits the interface to what it teaches | partial | `SetInterfaceInteraction` is real; see ../hand/; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Each lesson's lines are said by the Island Keeper, and wait to be read | partial | `RunText` and `TextRead` are real; see advisors.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| A lesson that the player strays from has the Keeper call them back ("You are straying! Try and keep me in view!") | partial | `GameThingFieldOfView` and the distances are real; the view test `GameThingCanViewCamera` is a stub; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Each line names the mouse button or key to press, as the player has bound it | todo | `SetHandDemoKeys` is empty; see ../interface/key_bindings.md |

## Lesson 1: moving and turning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper welcomes the player and sets off round the mountain, beckoning | partial | villager walks and animations are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Only grabbing and dragging the land works | partial | `SetInterfaceInteraction` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The player keeps up by grabbing the land and pulling it | partial | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The Keeper waits while he is out of view or more than 100 away | todo | the view test `GameThingCanViewCamera` is a stub |
| Then turning: the hand at the screen's left edge shows the rotation arrows; holding the button and moving the mouse turns the view | partial | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Only turning and dragging work until the player has turned the view the way asked | todo | the rotation check `WithinRotation` is a stub |
| The evil advisor reminds the player how to turn if they forget | partial | advisors and texts are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| A timed round: follow the Keeper round the mountain in under five minutes; he stops if the player is more than 50 away or ahead of him | partial | timers and distances are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Too slow: "Oh. Bad luck" and the round starts again | partial | `RunText` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |

## Lesson 2: tilting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper climbs a path and the player follows by tilting the view | partial | villager walks are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Only tilting works | partial | `SetInterfaceInteraction` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The hand at the screen's top edge shows the arrows; holding the button and moving the mouse tilts the view | partial | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Then the Keeper walks down again and the player keeps him in view | partial | villager walks and field-of-view are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Too slow or straying: the step starts again | partial | the same script; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |

## Lesson 3: everything together

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper walks round a valley path and the player follows with every control | partial | villager walks are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| He waits when out of view or too far | todo | the view test `GameThingCanViewCamera` is a stub |

## Lesson 4: zooming

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding both mouse buttons and moving the mouse zooms | partial | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The player zooms out to see the whole island, then back in | partial | the camera distance reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Two rocks are placed as marks to zoom between | partial | `CREATE` makes rocks; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The keyboard and the mouse wheel zoom too | todo | `HasMouseWheel` is a stub (answers no); the player camera control itself: see ../camera/world_camera_controls.md |

## Lesson 5: picking up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five lost teddy bears to pick up and drop in a ditch | partial | `CREATE` makes mobile objects; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Picking up: the hand over a bear, the Action button held and the mouse moved | partial | see ../hand/picking_up.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Dropping: the Action button again | partial | see ../hand/holding.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| All five in the ditch ends the lesson | partial | `CallNear` and the counts are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |

## Lesson 6: double clicking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Double clicking the Move button somewhere flies the camera there | partial | the player camera control itself: see ../camera/world_camera_controls.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The player double clicks the islet offshore to fly there | partial | the camera position reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Getting there any other way: "Hey! You didn't double-click to get there!" and try again | partial | the same script; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Then double clicks the mountain top to fly back | partial | the camera position reads are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |

## After the lessons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Keeper says that was all and the player may stay and practise | partial | `RunText` is real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| Six gold scrolls stand by the start, one per lesson; tapping one runs that lesson again | partial | `CreateHighlight` and `GameThingClicked` are real; see ../interface/scrolls_and_signs.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The Keeper goes; each replay brings its own Keeper | partial | villager `CREATE` and `ObjectDelete` are real; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
| The miracle dispensers stay for practising miracles | partial | see ../miracles/dispensers_and_seeds.md; never started: our tree starts only `LandControlAll` (`Game.cpp`), never the island's `LandControlT` |
