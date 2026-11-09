# Future room

The temple room kept for the game's future: in the released game it shows only "The future is still uncertain...".
Network play itself is reached from the front end (../multiplayer/).

**Progress: 4/4 done, 0 chosen, 0 partial, 0 inferred — 100%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`; test `TempleCamera.PictureRoomsStartLookingAlongTheEndOfTheirPaths`) |
| "The future is still uncertain..." fades in to half over two seconds, each time the camera comes in | done | `temple::future_room::Step` (raffclar's, `src/Temple/FutureRoomRules.h`) from `TempleInterior::UpdateOptionsAndFutureRooms`: the words start once the camera has come in with no dialog up and then show at every coming in of the visit; their alpha is the truncated 64 × the seconds of the frames' whole milliseconds, held to 128, from the start each time the camera comes in, as `UniverseRoom::Update` 0x79D000 and `DrawAdditional` 0x79CED0 ([the future room](../../../interface/temple-interior.md#the-future-room), read 2026-10-22); tests `FutureRoom.*`; debug: the temple panel's "Future room" line |
| Once its words show, the room turns slowly by itself, speeding up over a second, and the arrow keys do not turn it | done | `TempleCameraModel::PictureTurnByItself` (raffclar's), as `ChallengeRoomCamera::UpdateMain` 0x7859D0 (0x78600A, 0x786174; read 2026-10-22, [the future room](../../../interface/temple-interior.md#the-future-room)); test `TempleCamera.TheFutureRoomTurnsByItselfWhileItShowsItsWords` |
| Whether the room is open to the player | done | `temple::IsRoomAvailable` (`src/Temple/TempleRoomAccess.h`): open once the main room has been drawn in the game; a closed door starts the temple's "room not available" help; read in the exe ([which rooms are open](../../../interface/temple-interior.md#which-rooms-are-open)); tests `TempleRoomAccess.*` |
