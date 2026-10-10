# Script camera

The camera commands of the challenge scripts: taking the camera from the player, placing and gliding it, following
things, the named camera positions of the camera library, the lens, and shaking. The cut scenes that use them are in
`../story/`; the cinema bars and fades are in [cinematics.md](cinematics.md).

**Progress: 22/28 done, 1 partial — 80%**

How the original does it, in our wiki: [Script camera (GCamera and CameraModeScript)](../../bw1-notes/script-camera.md).

## Taking the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script starts camera control, taking the camera from the player (only one script at a time) | done | `START_CAMERA_CONTROL` (`src/CHLApi.cpp`): `script_camera::BeginFrom` (`src/Camera/ScriptCamera.cpp`) and `help::script_control::StartCameraControl`; `ScriptCamera.OneScriptModeAtATime`, `BeginStartsFromTheDrawnCamera` (`test/test_script_camera.cpp`) |
| Ending camera control gives the camera back to the player | done | `END_CAMERA_CONTROL` and the task stop call `script_camera::End`: the player's camera goes on from the script's zoomers, the lens back to 70 degrees in 0.5 s; `ScriptCameraDual.EndReleasesOneDualThenTheScriptMode` |
| A script stores the camera's position and focus and restores them later | done | `STORE_CAMERA_DETAILS`, `RESTORE_CAMERA_DETAILS` in `src/CHLApi.cpp` (restore also sets the player's camera without a script mode: inferred) |
| Scripts read the stored position and focus | done | `GET_STORED_CAMERA_POSITION`, `GET_STORED_CAMERA_FOCUS` push `script_camera::Get().storedPosition` and `storedFocus` |
| Scripts set how a following camera behaves: distance, speed, angle and whether it stays behind | done | `CAMERA_PROPERTIES` to `script_camera::SetFollowProperties`; `ScriptCameraFollow.CameraPropertiesWithoutSpeedPlaceAtOnce` |

## Placing and gliding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set the camera's position at once | done | `SET_CAMERA_POSITION` to `script_camera::SetPosition`, only with the script mode current (`ScriptCameraMode` in `src/CHLApi.cpp`); `ScriptCamera.SetDropsTheMove` |
| Set the camera's focus at once | done | `SET_CAMERA_FOCUS` to `script_camera::SetFocus`, the same check |
| Read the camera's position and focus | done | `GET_CAMERA_POSITION`, `GET_CAMERA_FOCUS` read the drawn camera |
| Glide the camera's position to a point over a time | done | `MOVE_CAMERA_POSITION` to `script_camera::MovePosition`, seconds of the wall clock; `ScriptCamera.MoveArrivesAfterItsTime`, `ShortTimeSetsAtOnce` |
| Glide the camera's focus to a point over a time | done | `MOVE_CAMERA_FOCUS` to `script_camera::MoveFocus` |
| Set or glide position, focus and lens together | done | `SET_CAMERA_POS_FOC_LENS`, `MOVE_CAMERA_POS_FOC_LENS`, with the lens not turned into radians as in the game |
| "camera ready": whether the camera has arrived | done | `HAS_CAMERA_ARRIVED`: `script_camera::ScriptArrived` (a track's time, or both points within 0.001), or the player's zoomers with the same rule; `ScriptCameraDual.ArrivedIgnoresThePathUnderneath` |
| Set or glide the camera to face a thing from a distance | partial | `SET_CAMERA_TO_FACE_OBJECT`, `MOVE_CAMERA_TO_FACE_OBJECT` through `script_camera::FaceObject`; `ScriptCameraFollow.FacePosition`, `FaceObject`. A creature faces 0 for now: our tree has no creature body angle for it (approximate) |
| A point in front of the camera at a distance | done | `GET_FACING_CAMERA_POSITION`: the drawn position plus the distance towards the drawn focus (inferred unit vector) |
| Whether a thing can see the camera within an angle | todo | `GAME_THING_CAN_VIEW_CAMERA` is a stub (always false) |

## Named camera positions and tracks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set or glide the camera to a named position from the game's camera library | done | `CONVERT_CAMERA_POSITION`, `CONVERT_CAMERA_FOCUS` read segment Cam%d of `Data/camera.edt` (`src/3D/CameraTracks.cpp`); the scripts then set or move the camera there |
| Create a marker at a named camera position | done | a marker is made at any point (`MarkerArchetype` through `CREATE` in `src/CHLApi.cpp`), the point taken from `CONVERT_CAMERA_POSITION` |
| Run a recorded camera track by name | done | `RUN_CAMERA_PATH` to `script_camera::RunPath` on track Track%d of `Data/camera.edt`; see [camera_paths.md](camera_paths.md) |

## Following

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Make the camera's focus follow a thing, at once or gliding | done | `FOCUS_FOLLOW`, `SET_FOCUS_FOLLOW` to `script_camera::FocusFollow`; `ScriptCameraFollow.TimeRule`, `FollowStartsSlower` |
| Make the camera's position follow a thing, at once or gliding | done | `POSITION_FOLLOW`, `SET_POSITION_FOLLOW` (placed at once with `PlaceFollowNow`); `ScriptCameraFollow.PositionFollowTakesTheCurrentAngles`, `PlaceNowThenFollow` |
| Follow a thing with both focus and position at a distance | done | `FOCUS_AND_POSITION_FOLLOW`, `SET_FOCUS_AND_POSITION_FOLLOW`; `ScriptCameraFollow.DistanceAndPitchLimits`, `AThingThatGoesIsDropped` |
| Follow a computer player's hand | todo | `SET_FOCUS_FOLLOW_COMPUTER_PLAYER`, `SET_POSITION_FOLLOW_COMPUTER_PLAYER` are stubs: no computer players |
| A dual camera keeps two things in view | done | `START_DUAL_CAMERA`, `UPDATE_DUAL_CAMERA`, `RELEASE_DUAL_CAMERA`, `CREATE_DUAL_CAMERA_WITH_POINT` (`script_camera::StartDual` and the rest); `ScriptCameraDual.*` in `test/test_script_camera_dual.cpp`. No camera mode stack: the dual always sits on the script mode (approximate) |
| Focus a thing on another and release it | todo | `SET_FOCUS_ON_OBJECT`, `RELEASE_OBJECT_FOCUS` are stubs in `src/CHLApi.cpp` |

## Lens and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set, glide or reset the camera's lens | done | `SET_CAMERA_LENS` (70 degrees in the given time, as the game does), `MOVE_CAMERA_LENS` (degrees in seconds of game time) through `script_camera::SetFov`; `ScriptCamera.FovFollowsGameTime` |
| Shake the camera from a point with a radius, amplitude and time | done | `SHAKE_CAMERA` to `camera_shake::StartCameraShake` (`src/Camera/CameraShake.cpp`), drawn only, by `script_camera::ApplyShake`; `CameraShake.OnlyWithinTheRadiusAndLessAndLess`, `TheNearestDecides`, `YOnlyMovesY` |
| Explosions and particle effects shake the camera, less the further away it is | todo | only the script's shakes exist: the particle sounds' `DoCameraShake` is read but not applied (`src/Particles/Rules/Sound.cpp`). Our wiki differs: a shake is felt in full anywhere within its radius, fading only with its time, not with distance ([script camera](../../bw1-notes/script-camera.md#shake)) |
| Turn close clipping on and off for close shots | todo | `SET_GRAPHICS_CLIPPING` is a stub in `src/CHLApi.cpp`; `near_clipping::k_Close` (`NearClipping.ScriptsCanClipClose`) is unused |
