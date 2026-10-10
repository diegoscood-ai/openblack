# Camera paths

Recorded camera flights: the paths some miracles take their caster's camera along, the land's intro flight, the camera
tracks of the camera library, and how the game's camera hands over from one kind of camera to another.

**Progress: 4/18 done, 3 partial — 31%**

How the original does it, in our wiki: [Camera tracks (`Data\camera.edt`) and `WALK_PATH`](../../bw1-notes/camera-tracks.md).

## Miracle camera paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle's effect takes its caster's camera along a path placed at the miracle | todo | no miracle takes the camera along a path in our tree; `CameraPathSystem` (`src/ECS/Systems/Implementations/CameraPathSystem.cpp`) only plays `.cam` paths from the debug camera window |
| The camera glides from where it is onto the path's start, then follows the path a little behind, sped up | todo | `CameraPathSystem::Update` sets the camera on the path's samples, with no glide onto it and no lag |
| The path lasts one play of its animation, or ends with its miracle | todo | no miracle path |
| A movement key that would move the camera this frame, or gripping the land, takes the camera back at once | partial | the rule is `camera_path::TakesCameraBack` (`src/Camera/CameraPathControl.h`; `CameraPathControl.AMovementKeyTakesTheCameraBackOnlyWhenItWouldMoveIt`, `GrippingTheLandAlwaysTakesTheCameraBack` in `test/test_near_clipping.cpp`), not wired yet |
| Turning, tilting and zooming don't take it back; meanwhile the player's other camera controls do nothing | todo | no miracle path holds the camera |
| The hand stays free and drawn while a path has the camera | todo | no miracle path |
| The near plane comes in close while a path has the camera | todo | no miracle path; the script close clipping is not wired either |
| Other spells' camera paths (the tree goddess and teacher paths in the spell animations) | todo | not wired |
| The falling spell's film takes the camera along its own recorded path | done | (added) `FallingSpell::UpdateCamera` (`src/Magic/Objects/FallingSpell.cpp`) reads `fall.cm2` and sets the camera from it while `SET_AVI_SEQUENCE` 2 plays ([video](../../bw1-notes/video.md#the-falling-spell-fallbik)) |

## Path playback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A path is sampled smoothly between its points and holds the last | done | `CameraPath::SampleAt` (`src/3D/CameraPath.cpp`); `CameraPath.SamplesBetweenItsPointsAndHoldsTheLast` (`test/camera/test_temple_camera.cpp`) |
| The camera glides to where it is sent on a smooth curve arriving at a set speed after a set time | done | `Zoomer3` (`src/Common/Zoomer.h`), the game's zoomer; `TestCameraZoomers.ZoomerMatchesRecording` (`test/camera/test_camera.cpp`), `test/test_zoomer.cpp` |
| The land's intro flight over the island as a land begins | todo | not in our tree; `.cam` paths are only played from the debug camera window (`src/Debug/Camera.cpp`) |
| The camera library's tracks: recorded flights with smooth curved segments that scripts play | done | `Data/camera.edt` is read by `src/3D/CameraTracks.cpp` (resource loader in `src/Resources/Loaders.cpp`); `RUN_CAMERA_PATH` runs a track in the script camera (`script_camera::RunPath`); `test/camera/test_camera_segments.cpp` |
| Scripts can slow the camera down for slow motion | todo | not in our tree |

## Handing the camera over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Other cameras (creature, temple, fight, path) take over from the player's and hand it back where they found it | partial | `Camera::SetModel` swaps models (creature mode, temple, editor); the script camera starts from the player's zoomers and hands them back (`script_camera::BeginFrom`, `End`); no general stack of camera modes (approximate, as our wiki says) |
| A camera that is no longer valid (its thing gone) gives way to the one before it | partial | the script's dual camera is dropped once a turn when its things are gone (`script_camera::Validate`, `CheckDualModes`) and Creature Mode lets go when the creature goes; nothing general |
| Some cameras stop the player drawing gestures while they move | todo | no gesture drawing in the game yet (only the debug gesture window) |
| The camera's state is kept in saved games | todo | no saved games; see `../engine/` |
