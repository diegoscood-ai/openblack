# Tutorial

How the game teaches itself: the opening of the first land, where the advisors and short hand demonstrations show the
camera, the hand and the first miracles. The separate practice island is in tutorial_island.md.

A cut early draft of the opening at the temple (rotating the camera, carrying wood and food to the builders, the kinds
of scroll) is in [silver_scrolls/see_the_citadel.md](silver_scrolls/see_the_citadel.md).

**Progress: 5/17 done, 9 partial — 56%**

How the original does it, in our wiki: [The Land 1 intro and the tutorial's script side](../../bw1-notes/intro.md).

## The opening scene

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Norse family (father, mother and son) is made, in high detail, at early morning | partial | `SetHighGraphicsDetail` makes them high-detail villagers (`ecs::super_villager`, `src/Graphics/SuperVillagerFrame`, eyes in `src/ECS/SuperVillager*`), wired into `Game.cpp`; the wiki still calls it drafted, not checked against the original ([intro.md](../../bw1-notes/intro.md#the-family-in-high-detail-supervillager)) |
| The scene takes the camera, the dialogue and the game speed, brings the cinema bars in and starts its music | done | `StartCameraControl`, `StartDialogue`, `StartGameSpeed`, `SetWidescreen`, `StartMusic` in `src/CHLApi.cpp`; checked in game up to the hand-over ([intro.md](../../bw1-notes/intro.md#flow)) |
| The parents kiss, the son swims out and sharks come for him | partial | the family's walks, the son and the sharks run (`CREATE` of the shark, `MoveGameThing`, script states); the villagers' focus and override animations are not all done ([intro.md](../../bw1-notes/intro.md#state-in-openblack)) |
| The screen fades to black and the intro film plays | done | `SetFade` and `SetAviSequence` play `INTRO.bik` through our Bink decoder (`src/Video`); see ../video/bink_videos.md |
| A light falls from the sky, a ghostly hand lifts the boy from the sea and sets him down on the beach | partial | the light and the intro hand are in `ECS/IntroSpecial` and the JC specials (`PlayJcSpecial`, `ThingJcSpecial`), wired but still drafted ([intro.md](../../bw1-notes/intro.md#jc-specials-and-the-confirmation-sounds)) |
| The village crowds round to welcome their new god; the advisors appear and introduce themselves | partial | the advisors appear, move and speak (`src/Help`, see advisors.md); the welcome dance (`DanceCreate`) is a stub ([intro.md](../../bw1-notes/intro.md#state-in-openblack)) |
| A demonstration of dragging the land, then the scene hands control to the player | done | `PlayHandDemo` plays `Data/HandDemo/drag.hnd` (`src/Input/HandDemo.h`), `HandDemoTrigger`, `IsPlayingHandDemo`, then the hand-over ([intro.md](../../bw1-notes/intro.md#flow)) |
| About four minutes from the start to the hand-over | done | checked in game: about 4 min of game time ([intro.md](../../bw1-notes/intro.md#state-in-openblack)) |

## The opening of the first land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player follows the family to the village, learning to move over the land | partial | the rest of `FollowUs` calls real natives (walk paths, `MoveGameThing`, the camera distance checks) but is not checked in game past the hand-over; it also starts the gate-stone guards: [gold_scrolls/choose_your_creature.md](gold_scrolls/choose_your_creature.md#the-stones-are-guarded) |
| Hand demonstrations: a ghost hand shows a movement (rotating, tilting, dragging the land, zooming, throwing, giving food) | partial | `PlayHandDemo` reads the `Data/HandDemo` recordings (`src/Input/HandDemo.h`); only the drag demo is checked in game |
| A demonstration waits for the player to copy it, and reminds them if they don't | partial | the dialogue, advisors, highlights and timers they use are real in `src/CHLApi.cpp`; not checked in game past the hand-over |
| Lessons on worship, influence and casting a miracle by gesture | todo | Khazar's lessons are on Land 2, never reached (`LOAD_MAP` is empty); see land_2.md |
| Each lesson's key or mouse button is shown as the player has bound it | todo | `SetHandDemoKeys` is an empty native; see ../interface/help_system.md |
| The camera's features are allowed one by one as each is taught | partial | `SetInterfaceInteraction` sets the camera feature mask; what the player camera does with each feature is pending ([../../bw1-notes/script-camera.md](../../bw1-notes/script-camera.md#player-camera-features-camerahelp)) |
| The interface is limited during the lessons (only grabbing, only rotating …) | partial | `SetInterfaceInteraction` (`help::interface_interaction`) works; the levels' effects on the interface are partly done ([intro.md](../../bw1-notes/intro.md#interface-interaction-levels)) |
| An Immersion force-feedback mouse is detected and welcomed | n/a | hardware long gone |
| A returning player can skip the tutorial and the creature's training (patch 1.1) | done | the SkipBox (`src/Gui/SkipBox`) and `CanSkipTutorial`, `CanSkipCreatureTraining`, `IsKeepingOldCreature` are real; see [../../bw1-notes/map-loading.md](../../bw1-notes/map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) and land_1.md |

## The tutorial island

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A separate practice island, the Gods' Playground, reached with F2 | todo | see tutorial_island.md |
