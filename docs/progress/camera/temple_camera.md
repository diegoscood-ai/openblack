# Temple camera

The camera inside the temple: each room's camera comes in along the room's path, then the player looks around the room
and walks between rooms by clicking doors. The rooms themselves are in `../temple/`.

**Progress: 24/26 done, 1 partial — 94%**

## Entering and leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Entering the temple, the camera flies up to the entrance and in | todo | our tree cuts straight inside (`TempleInterior::Activate`, `src/3D/Implementations/TempleInterior.cpp`) (unconfirmed exact flight) |
| Each room's camera comes in along the room's path | done | `src/Camera/TempleCameraModel.cpp`, paths from `Data/Citadel/engine/<room>.cam` (loaded in `src/Game.cpp`); `TempleCamera.PictureRoomsStartLookingAlongTheEndOfTheirPaths` (`test/camera/test_temple_camera.cpp`) |
| Coming in, the camera eases from where it was onto the path | done | `TempleCameraModel::StartIntro` blends from the previous camera over its first seconds |
| The entrance doors close behind the camera as it comes in | partial | `TempleCameraModel` closes the doors 0.4 s into a room's path at its own rate (`k_IntroDoorCloseTime`, `k_IntroDoorCloseRate`) through `TempleDoors` (`TempleDoors.*`, `test/temple/test_temple_doors.cpp`); not compared with the game |
| Sent straight to a room by its key, the camera cuts past the room's path | done | `Game::ProcessTempleRoomKeys` and `TempleInterior::GoToRoom`; keys in `src/Input/KeyBindings.h` |
| Leaving, the main room fades to white and the camera is put 50 above the place and 70 along from it, looking at it | done | `TempleInterior` (`src/3D/Implementations/TempleInterior.cpp`): fade to white, then the camera 50 above and 70 along from the place |
| A double click on the pool's island map leaves the temple for that place | done | `TempleCameraModel::TakeMapDoubleClick`, read by `TempleInterior` |

## Main room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dragging the walls turns the room round the player and leans the view between the pool and the roof | done | `TempleCamera.LooksAtTheMainRoomFromItsOrbit` |
| The arrow keys turn and lean | done | `TempleCameraModel.cpp` orbit keys |
| With the mouse at the screen's top or bottom edge the view leans, speeding up | done | `TempleCameraModel.cpp` (`_edgeRamp`) |
| The pool slowly draws the camera down towards it | done | `TempleCameraModel.cpp`, the pool's lean |
| Clicking a door walks the camera through it to that room | done | `TempleCamera.ClickingADoorOnScreenFindsIt`, `HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms` |
| The doors are found round the walls; the way out and the wall of scrolls aren't doors the camera goes through | done | `TempleCamera.FindsTheMainRoomsDoorsAroundItsWalls` |
| Dragging the pool's map turns and tilts about the pressed point | done | `TempleCameraModel.cpp`, holding the press on the pool's map |
| The main room starts facing the scroll, leaning a little up | done | `TempleCameraModel.cpp` |
| The cursor is cast against each room's own walls and floor, which the hand follows | done | `TempleCamera.CastsAgainstEachRoomsOwnWalls` |

## Creature's room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player looks around from where the room's path ends, by dragging and with the arrow keys | done | `TempleCamera.TheCreaturesRoomLooksOnFromTheEndOfItsPath` |
| Clicking a target zooms to it over a second; a press anywhere or the arrow keys zoom back in half that | done | `TempleCameraModel::UpdateCaveTargets` with `CreatureCaveTargets` (`src/3D/CreatureCaveTargets.cpp`; `test/temple/test_creature_cave_targets.cpp`) |
| Zoomed to the exit, the player leaves the temple | done | `TempleCameraModel.cpp`, the exit target |

## Rooms of pictures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge, save game, options, multiplayer and credits rooms turn round the player and raise and lower the camera, by dragging the walls and with the arrow keys | done | `TempleCameraModel.cpp` picture rooms |
| The credits room turns about where its path ends rather than its centre | done | `TempleCameraModel.cpp` (`k_CreditsRoom`) |
| Clicking the door walks back to the main room | done | `TempleCameraModel.cpp` |
| Under a dialog the room turns slowly by itself, speeding up over a second | done | `TempleCameraModel.cpp` (`k_PictureDialogTurnSpeed`) |
| Each room's turn, lean and height are kept while the player stays in the temple | done | `TempleCameraModel.h` |

## Scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking a scroll brings the camera close to look at it along a smooth curve, with a whoosh | done | `TempleCameraModel::LookAtSubMesh`; the whoosh `TempleSounds.TheCameraWooshesOneOfFourByTheClock` (`test/temple/test_temple_scroll.cpp`) |
| A press elsewhere or the arrow keys send the camera back | done | `TempleCameraModel.cpp` |
