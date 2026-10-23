# Temple camera

The camera inside the temple: each room's camera comes in along the room's path, then the player looks around the room
and walks between rooms by clicking doors. The rooms themselves are in `../temple/`.

**Progress: 3/28 done, 0 chosen, 24 partial, 1 inferred — 51%**

## Entering and leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Entering the temple, the camera flies up to the entrance and in | inferred | our tree cuts straight inside (`TempleInterior::Activate`, `src/3D/Implementations/TempleInterior.cpp`). Whether the original flies up to the entrance first is a guess: the room cameras' intro (`TriggerIntro`, which takes the outside camera's zoomers) has not been read |
| Each room's camera comes in along the room's path | partial 60% | `src/Camera/TempleCameraModel.cpp` (raffclar's port), paths from `Data/Citadel/engine/<room>.cam` (loaded in `src/Game.cpp`); `TempleCamera.PictureRoomsStartLookingAlongTheEndOfTheirPaths` (`test/camera/test_temple_camera.cpp`). Missing: the room cameras' intro in the exe has not been read, so the path's timing is unverified |
| Coming in, the camera eases from where it was onto the path | partial 40% | `TempleCameraModel::StartIntro` blends from the previous camera over its first seconds. Missing: no test, and the blend's time and curve are not read in the exe |
| The entrance doors close behind the camera as it comes in | partial 40% | `TempleCameraModel` closes the doors 0.4 s into a room's path at its own rate (`k_IntroDoorCloseTime`, `k_IntroDoorCloseRate`) through `TempleDoors` (`TempleDoors.*`, `test/temple/test_temple_doors.cpp`, which covers the doors, not the camera's timing). Missing: the 0.4 s and the rate are not read in the exe |
| Sent straight to a room by its key, the camera cuts past the room's path | partial 40% | `Game::ProcessTempleRoomKeys` and `TempleInterior::GoToRoom`; keys in `src/Input/KeyBindings.h`. Missing: no test; inside the temple the original sends the room keys to the temple's own key handler, which has not been read ([one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)) |
| Leaving, the main room fades to white and the camera is put 50 above the place and 70 along from it, looking at it | partial 80% | `TempleInterior::LeaveForMapPoint` (`src/3D/Implementations/TempleInterior.cpp`): only for a point within 10 of the map's middle or on land above height 1, fade to white, then the camera 50 above and 70 along z from the place, looking at it. Read in the exe on 2026-10-21 (the main room camera's update): the same 10, 1, 50 and 70 and the fade. Missing: a test; the force feedback the original starts there belongs to `../pc_integration/` |
| A double click on the pool's island map leaves the temple for that place | partial 50% | `TempleCameraModel::TakeMapDoubleClick`, read by `TempleInterior`; the leave rules match the exe (row above). Missing: a test, and the double click's own timing on the map is not read |
| A closed room's door does not let the camera through, and the advisor says the room is not open | done | (added) `TempleCameraModel::UpdateOrbit` asks `temple.IsRoomAvailable` and calls `TellRoomNotAvailable`; `TempleRoomAccess.AClosedRoomsDoorStopsTheHelpAndSaysSo` (`test/temple/test_temple_room_access.cpp`); the exe's rule is in [which rooms are open](../../../interface/temple-interior.md#which-rooms-are-open) |

## Main room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dragging the walls turns the room round the player and leans the view between the pool and the roof | partial 60% | `TempleCameraModel` orbit (raffclar's port); `TempleCamera.LooksAtTheMainRoomFromItsOrbit`. Missing: the main room camera's update in the exe has not been read for the orbit's poses and rates |
| The arrow keys turn and lean | partial 30% | `TempleCameraModel.cpp` orbit keys. Missing: no test; the speeds are not read in the exe |
| With the mouse at the screen's top or bottom edge the view leans, speeding up | partial 30% | `TempleCameraModel.cpp` (`_edgeRamp`). Missing: no test; the ramp is not read in the exe |
| The pool slowly draws the camera down towards it | partial 30% | `TempleCameraModel.cpp`, the pool's lean. Missing: no test; not read in the exe |
| Clicking a door walks the camera through it to that room | partial 60% | `TempleCamera.ClickingADoorOnScreenFindsIt`, `HitsTheFloorBelowAndLeadsThroughTheDoorsToTheRooms`. Missing: the walk's poses and times are not read in the exe |
| The doors are found round the walls; the way out and the wall of scrolls aren't doors the camera goes through | partial 60% | `TempleCamera.FindsTheMainRoomsDoorsAroundItsWalls`; the rooms behind each door match the exe's room list ([which rooms are open](../../../interface/temple-interior.md#which-rooms-are-open)). Missing: the door shapes (heights, widths) are raffclar's, not read in the exe |
| Dragging the pool's map turns and tilts about the pressed point | partial 30% | `TempleCameraModel.cpp`, holding the press on the pool's map. Missing: no test; not read in the exe |
| The main room starts facing the scroll, leaning a little up | partial 60% | `TempleCameraModel.cpp`; `TempleCamera.LooksAtTheMainRoomFromItsOrbit` pins the start pose, and the joints' temple shot keeps it. Missing: the start lean (0.7 in our tree) is not read in the exe, whose main room camera starts a lean-like field at 0.5 |
| The cursor is cast against each room's own walls and floor, which the hand follows | partial 60% | `TempleCamera.CastsAgainstEachRoomsOwnWalls`. Missing: the original's room ray cast and the hand's placement inside are not read ([pending](../../../interface/temple-interior.md#pending)) |

## Creature's room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player looks around from where the room's path ends, by dragging and with the arrow keys | partial 60% | `TempleCamera.TheCreaturesRoomLooksOnFromTheEndOfItsPath`. Missing: the creature room camera's look speeds and limits are not read in the exe |
| Clicking a target zooms to it over a second; a press anywhere or the arrow keys zoom back in half that | partial 60% | `TempleCameraModel::UpdateCaveTargets` with `CreatureCaveTargets` (`src/3D/CreatureCaveTargets.cpp`; `test/temple/test_creature_cave_targets.cpp`, `TempleCamera.APressLeavesTheCreatureZoomedTo`). Missing: the one second and the half are not read in the exe |
| Zoomed to the exit, the player leaves the temple | partial 30% | `TempleCameraModel.cpp`, the exit target. Missing: no test of the leave; not read in the exe |

## Rooms of pictures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge, save game, options, multiplayer and credits rooms turn round the player and raise and lower the camera, by dragging the walls and with the arrow keys | partial 40% | `TempleCameraModel.cpp` picture rooms; only their start pose is tested (`TempleCamera.PictureRoomsStartLookingAlongTheEndOfTheirPaths`). Missing: tests of the turn and the rise, and the picture rooms' camera in the exe is not read |
| The credits room turns about where its path ends rather than its centre | partial 30% | `TempleCameraModel.cpp` (`k_CreditsRoom`). Missing: no test; not read in the exe |
| Clicking the door walks back to the main room | partial 30% | `TempleCameraModel.cpp`. Missing: no test; not read in the exe |
| Under a dialog the room turns slowly by itself, speeding up over a second | done | `TempleCameraModel::PictureTurnByItself` (raffclar's; `k_PictureDialogTurnSpeed`): a quarter turn a second times the time turned so far, held to one, the time going on only while the room turns by itself, the library kept still, as `ChallengeRoomCamera::UpdateMain` 0x7859D0 (0x786174; read 2026-10-22, [the future room](../../../interface/temple-interior.md#the-future-room)); tests `TempleCamera.APictureRoomTurnsByItselfUnderADialogSpeedingUpOverASecond`, `TheLibraryKeepsStillUnderADialog` |
| Each room's turn, lean and height are kept while the player stays in the temple | partial 30% | `TempleCameraModel.h`. Missing: no test; not read in the exe |

## Scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking a scroll brings the camera close to look at it along a smooth curve, with a whoosh | partial 80% | `TempleCameraModel::LookAtSubMesh`; the whoosh `TempleSounds.TheCameraWooshesOneOfFourByTheClock` (`test/temple/test_temple_scroll.cpp`) and the close look's points (the submesh's middle and 25 in front, the scrolls' own offset) are the exe's ([the temple's sounds](../../../interface/temple-interior.md#the-temple-interiors-sounds), [the challenge room's camera](../../../interface/temple-interior.md#the-challenge-rooms-controls-and-its-cameras-blend)). Missing: no test of the camera's curve between the room view and the close look |
| The close look grows by 0.75 a second and falls back by 0.7 a second, held between none and full | done | (added) `temple_challenge_room::BlendStep`, stepped by `TempleCameraModel`; `TempleChallengeRoom.TheCameraBlendStepsAsTheOriginals` (`test/temple/test_temple_challenge_room.cpp`), pinned to the oracle ([the camera's blend](../../../interface/temple-interior.md#the-challenge-rooms-controls-and-its-cameras-blend)) |
| A press elsewhere or the arrow keys send the camera back | partial 40% | `TempleCameraModel.cpp`; the way back's rate is the exe's (row above). Missing: no test, and which presses and keys end the look in the exe is not read |
