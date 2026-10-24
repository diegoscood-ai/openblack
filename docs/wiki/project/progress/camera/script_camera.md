# Script camera

The camera commands of the challenge scripts: taking the camera from the player, placing and gliding it, following
things, the named camera positions of the camera library, the lens, and shaking. The cut scenes that use them are in
`../story/`; the cinema bars and fades are in [cinematics.md](cinematics.md).

**Progress: 20/30 done, 0 chosen, 7 partial, 1 inferred — 87%**

How the original does it, in our wiki: [Script camera (GCamera and CameraModeScript)](../../../camera/script-camera.md).

## Taking the camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script starts camera control, taking the camera from the player (only one script at a time) | done | `START_CAMERA_CONTROL` (`src/CHLApi.cpp`): `script_camera::BeginFrom` (`src/Camera/ScriptCamera.cpp`) and `help::script_control::StartCameraControl`; `ScriptCamera.OneScriptModeAtATime`, `BeginStartsFromTheDrawnCamera` (`test/test_script_camera_mode.cpp`) |
| Ending camera control gives the camera back to the player | done | `END_CAMERA_CONTROL` and the task stop call `script_camera::End`: the player's camera goes on from the script's zoomers, the lens back to 70 degrees in 0.5 s; `ScriptCameraDual.EndReleasesOneDualThenTheScriptMode` |
| A script stores the camera's position and focus and restores them later | partial 90% | `STORE_CAMERA_DETAILS`, `RESTORE_CAMERA_DETAILS` in `src/CHLApi.cpp`: the store keeps the drawn camera, shake included, as the original's reads the drawn camera; the restore sets the camera whatever its mode, as the original (read: it sets the game camera's zoomers with no mode check) and asks for the draw list again (`ScriptCameraRebuilds.RestoreAndPosFocLensAlwaysAsk`); `ScriptCameraDetails.StoringKeepsTheDrawnPositionAndFocusUntilTheNextStore`, `RestoringPutsThePlayersCameraWhereItWasStored` (`test/test_script_camera_details.cpp`). Missing: the original's setter does nothing when the camera is already there, not ported ([openblack](../../../camera/game-camera.md#openblack)) |
| Scripts read the stored position and focus | done | `GET_STORED_CAMERA_POSITION`, `GET_STORED_CAMERA_FOCUS` push `script_camera::Get().storedPosition` and `storedFocus`, x then y then z, as the original; `ScriptCameraDetails.StoringKeepsTheDrawnPositionAndFocusUntilTheNextStore` (`test/test_script_camera_details.cpp`) |
| Scripts set how a following camera behaves: distance, speed, angle and whether it stays behind | done | `CAMERA_PROPERTIES` to `script_camera::SetFollowProperties`; `ScriptCameraFollow.CameraPropertiesWithoutSpeedPlaceAtOnce` |

## Placing and gliding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set the camera's position at once | done | `SET_CAMERA_POSITION` to `script_camera::SetPosition`, only with the script mode current (`ScriptCameraMode` in `src/CHLApi.cpp`); `ScriptCamera.SetDropsTheMove` |
| Set the camera's focus at once | done | `SET_CAMERA_FOCUS` to `script_camera::SetFocus`, the same check |
| Read the camera's position and focus | done | `GET_CAMERA_POSITION`, `GET_CAMERA_FOCUS` read the drawn camera, shake included, as the original outside the temple; inside the citadel both give the original's point out of the world, 50000 on every axis (`script_camera::ReadCameraPoint`, 2026-10-23); tests `ScriptCameraDetails.TheCameraIsReadAsItIsDrawnShakeIncluded` (`test/test_script_camera_details.cpp`), `ScriptCameraMode.InsideTheCitadelTheCameraIsReadAsAPointOutsideTheWorld` |
| Glide the camera's position to a point over a time | done | `MOVE_CAMERA_POSITION` to `script_camera::MovePosition`, seconds of the wall clock; `ScriptCamera.MoveArrivesAfterItsTime`, `ShortTimeSetsAtOnce` |
| Glide the camera's focus to a point over a time | done | `MOVE_CAMERA_FOCUS` to `script_camera::MoveFocus` |
| Set or glide position, focus and lens together | partial 90% | `SET_CAMERA_POS_FOC_LENS`, `MOVE_CAMERA_POS_FOC_LENS`, with the lens not turned into radians as in the game; `ScriptCameraRebuilds.RestoreAndPosFocLensAlwaysAsk`. Missing: the setter's early way out when the camera is already there ([openblack](../../../camera/game-camera.md#openblack)) |
| "camera ready": whether the camera has arrived | done | `HAS_CAMERA_ARRIVED`: `script_camera::ScriptArrived` (a track's time, or both points within 0.001), or the player's zoomers with the same rule; `ScriptCameraDual.ArrivedIgnoresThePathUnderneath` |
| Set or glide the camera to face a thing from a distance | done | `SET_CAMERA_TO_FACE_OBJECT`, `MOVE_CAMERA_TO_FACE_OBJECT` through `script_camera::FaceObject`; a creature faces by its body's yaw plus a full turn less 2.5 (`script_camera::CreatureFacing`); `ScriptCameraFollow.FacePosition`, `FaceObject`, `ACreatureFacesByItsBodysYawPlusAFullTurnLessTwoAndAHalf` |
| A point in front of the camera at a distance | inferred | `GET_FACING_CAMERA_POSITION`: openblack adds the distance along the unit vector from the drawn position to the drawn focus; the original adds it along the drawn camera's own direction, taken to be the same but not checked; no test |
| Whether a thing or a point is on the screen | partial 85% | `GAME_THING_FIELD_OF_VIEW`, `POS_FIELD_OF_VIEW` through `field_of_view::` (`src/Camera/FieldOfView.cpp`) on the drawn camera; `TimersEventsTest.PointOnScreen`, `SphereOnScreen` (`test/test_timers_events.cpp`). Missing: an object's sphere comes from openblack's mesh box, approximate ([field-of-view tests](../../../camera/script-camera.md#the-field-of-view-tests)) |
| Whether a thing can see the camera within an angle | pending | `GAME_THING_CAN_VIEW_CAMERA` is a stub (always false); the original has it |

## Named camera positions and tracks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set or glide the camera to a named position from the game's camera library | done | `CONVERT_CAMERA_POSITION`, `CONVERT_CAMERA_FOCUS` read camera Cam%d of `Data/camera.edt` (`src/3D/CameraEdits.cpp`); `CameraTrack.TheFileHoldsItsNumberedCamerasAndTracks`; the scripts then set or move the camera there |
| Create a marker at a named camera position | done | a marker is made at any point (`ScriptMarkerArchetype` through `CREATE` in `src/CHLApi.cpp`), the point taken from `CONVERT_CAMERA_POSITION` |
| Run a recorded camera track by name | done | `RUN_CAMERA_PATH` to `script_camera::RunPath` on track Track%d of `Data/camera.edt` (`src/3D/CameraEdits.cpp`, `src/3D/CameraTrack.cpp`); `CameraTrack.*`; see [camera_paths.md](camera_paths.md) |

## Following

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Make the camera's focus follow a thing, at once or gliding | done | `FOCUS_FOLLOW`, `SET_FOCUS_FOLLOW` to `script_camera::FocusFollow`; `ScriptCameraFollow.TimeRule`, `FollowStartsSlower` |
| Make the camera's position follow a thing, at once or gliding | done | `POSITION_FOLLOW`, `SET_POSITION_FOLLOW` (placed at once with `PlaceFollowNow`); `ScriptCameraFollow.PositionFollowTakesTheCurrentAngles`, `PlaceNowThenFollow` |
| Follow a thing with both focus and position at a distance | done | `FOCUS_AND_POSITION_FOLLOW`, `SET_FOCUS_AND_POSITION_FOLLOW`; `ScriptCameraFollow.DistanceAndPitchLimits`, `AThingThatGoesIsDropped` |
| Follow a computer player's hand | pending | `SET_FOCUS_FOLLOW_COMPUTER_PLAYER`, `SET_POSITION_FOLLOW_COMPUTER_PLAYER` are stubs: no computer players |
| A dual camera keeps two things in view | partial 90% | `START_DUAL_CAMERA`, `UPDATE_DUAL_CAMERA`, `RELEASE_DUAL_CAMERA`, `CREATE_DUAL_CAMERA_WITH_POINT` (`script_camera::StartDual` and the rest); `ScriptCameraDual.*` in `test/test_script_camera_dual.cpp`. Missing: no camera mode stack, the dual always sits on the script mode, and a dual over the player's own camera is approximated ([dual camera](../../../camera/script-camera.md#dual-camera)) |
| Focus a thing on another and release it | done | `SET_FOCUS_ON_OBJECT`, `RELEASE_OBJECT_FOCUS`: a creature turns to the thing, its head on it until released or the thing is gone (`CreatureMindSystem::FocusOnThingByScript`, `ReleaseFocusByScript`); another object faces it as SET_FOCUS does; `CreatureMindSystemTest.AScriptsFocusOnAThingHoldsItsHeadWhereTheThingStandsUntilReleasedOrGone`. Natives checked against v1.42 |

## Lens and effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Set, glide or reset the camera's lens | done | `SET_CAMERA_LENS` (70 degrees in the given time, as the game does), `MOVE_CAMERA_LENS` (degrees in seconds of game time) through `script_camera::SetFov`; `ScriptCamera.FovFollowsGameTime` |
| Shake the camera from a point with a radius, amplitude and time | done | `SHAKE_CAMERA` to `camera_shake::StartCameraShake` (`src/Camera/CameraShake.cpp`), drawn only, by `script_camera::ApplyShake`; `CameraShake.OnlyWithinTheRadiusAndLessAndLess`, `TheNearestDecides`, `YOnlyMovesY` |
| A particle effect's sound (the beam explosion's) shakes the camera up and down while it is within the radius | done | `AddSoundToAtom` (`src/Particles/Rules/Sound.cpp`) through `ExplosionSystem::AddShake` to `camera_shake::StartEffectShake`; `CameraShake.AnEffectsShakeCutsItsMillisecondsAndMayBeUpAndDownOnly`. As every shake, felt in full anywhere within its radius and fading with its time only ([shake](../../../camera/script-camera.md#shake)) |
| A reward chest landing shakes the camera within 200 of the world's middle | done | `RewardSystem` (`src/ECS/Systems/Implementations/RewardSystem.cpp`, `reward::k_ShakeRadius`): radius 200 about the world's origin, strength 1, 400 ms, along every axis, once, with the thump (sample 174), as the original's chest landing (read in the exe); `RewardLanding.AChestFromTheSkyThumpsAndShakesTheCameraAboutTheWorldsOriginOnceAsItLands` (`test/test_reward_landing.cpp`) |
| Turn close clipping on and off for close shots | done | `SET_GRAPHICS_CLIPPING` (only its first argument) to `CinematicDirectorSystem::SetCloseClipping`, read by the near plane in `src/Game.cpp` (0.1), cleared when the scripts restart and kept by a land change (`Game::ResetScripts`); `NearClipping.ScriptsCanClipClose` ([camera](../../../camera/game-camera.md#camera)) |
