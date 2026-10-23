# Camera paths

Recorded camera flights: the paths some miracles take their caster's camera along, the land's intro flight, the camera
tracks of the camera library, and how the game's camera hands over from one kind of camera to another.

**Progress: 8/16 done, 0 chosen, 5 partial, 1 inferred — 71%**

How the original does it, in our wiki: [Camera tracks (`Data\camera.edt`) and `WALK_PATH`](../../../camera/script-camera.md).

## Miracle camera paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A miracle's effect takes its caster's camera along a path placed at the miracle | done | `CameraPathSystem::Begin` (`src/ECS/Systems/Implementations/CameraPathSystem.cpp`), asked by the forest miracle's camera particle (`src/Particles/Rules/Forest.cpp`), the only effect the game gives one; refused while a script holds the camera or the creature interaction's camera has the view, under which a placed path waits and then has the camera again (`SetCovered`, 2026-10-22); `CameraPathSystemTest.APlacedPathHoldsTheCameraUntilStopped`, `AScriptHoldingTheCameraRefusesIt`, `UnderTheCreatureInteractionsCameraThePathWaitsAndHasTheCameraAgainAfter` (`test/camera/test_camera_path_system.cpp`), `ForestCamera.SFForestsCameraTakesTheCameraForItsAnimationsLengthAfterFourSeconds`, `AnotherPlayersEffectNeverAsksForTheCamera` (`test/test_psys_forest_camera.cpp`) ([placed paths](../../../camera/script-camera.md#placed-paths-cameramodepath)) |
| The camera glides from where it is onto the path's start, then follows the path a little behind, sped up | done | `CameraPathSystem::Update`: one glide over the pause and 0.3 s, then each update sends the camera to the path's point to arrive 0.3 s later, the points placed by the effect's matrix in the original's order; `CameraPathSystemTest.TheCameraGlidesOntoThePathOnceOverThePauseAndTheLag`, `WhileItPlaysTheCameraIsSentEveryUpdateToArriveThreeTenthsLater`, `CameraPathRules.*`. The speed-up reaches the camera only through the animation's drawn frame, as in the original |
| The path lasts one play of its animation, or ends with its miracle | done | the effect lets go at its animation's last drawn frame or when it goes (`CameraPathSystem::Release`); `CameraPathRules.ItLetsGoAtTheLastFrameNotBefore`, `ForestCamera.TheLastDrawnFrameLetsTheCameraGoOnceAndTheRemovalDoesNotAgain`, `TheParticleRemovedBeforeItsLastFrameLetsTheCameraGo`, `CameraPathSystemTest.ReleaseLetsGoOfItsOwnersPathAndLeavesTheCameraWhereItIs` |
| A movement key that would move the camera this frame, or gripping the land, takes the camera back at once | done | `camera_path::TakesCameraBack` (`src/Camera/CameraPathControl.h`), wired in `src/Game.cpp` through `CameraPathSystem::HandlePlayerControl` (the four movement keys and the land grip); `CameraPathSystemTest.AMovementKeyThatMovesTheCameraOrALandGripTakesItBack`, `CameraPathControl.*` (`test/test_near_clipping.cpp`) |
| Turning, tilting and zooming don't take it back; meanwhile the player's other camera controls do nothing | partial 90% | turning, tilting and zooming are not read by `TakesCameraBack` (`CameraPathControl.AMovementKeyTakesTheCameraBackOnlyWhenItWouldMoveIt`), and the player's camera controls are skipped while `HoldsCamera` (`src/Game.cpp`). Missing: no test of the skipped controls |
| The hand stays free and drawn while a path has the camera | partial 80% | nothing hides the hand while a path has the camera (`HandSystem` and `HandToolTips` only drop the camera's hints), as in the original. Missing: the path's end shows the hand again and makes one more interface call in the original, not ported (what they do was not read, [pending](../../../camera/script-camera.md#pending)); no test |
| The near plane comes in close while a path has the camera | inferred | openblack brings the near plane in to 0.1 while a path has the camera (`near_clipping::NearPlane` with the path, `src/Game.cpp`). In the original the path writes its close plane once as it starts, and the camera's update writes the height's near plane over it on every frame, so the close plane is read as never reaching a draw ([camera](../../../camera/game-camera.md#camera), not proven). To be checked; if it holds, openblack's close plane for paths is a difference to fix |
| The falling spell's film takes the camera along its own recorded path | done | (added) `FallingSpell::UpdateCamera` (`src/Magic/Objects/FallingSpell.cpp`) reads `fall.cm2` and sets the camera from it while `SET_AVI_SEQUENCE` 2 plays; `FallingSpell.CameraPathMatchesTheOriginalBits`, `CameraAppliedEveryUpdateAndGivenBackAtClose` (`test/video/test_falling_spell.cpp`) ([video](../../../video/playback.md#the-falling-spell-fallbik)) |
| A script or dual camera started while a path has the camera goes on top of it | partial 50% | the script camera's update comes first and the path waits under it (`src/Game.cpp`). Missing: the path's take-back is still read while it waits, and the near plane stays the path's ([placed paths, pending](../../../camera/script-camera.md#pending)) |

## Path playback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A path is sampled smoothly between its points and holds the last | done | `CameraPath::SampleAt` (`src/3D/CameraPath.cpp`), the original's sampler; `CameraPath.SamplesBetweenItsPointsAndHoldsTheLast` (`test/camera/test_temple_camera.cpp`) |
| The camera glides to where it is sent on a smooth curve arriving at a set speed after a set time | done | `Zoomer3` (`src/Common/Zoomer.h`), the game's zoomer; `TestCameraZoomers.ZoomerMatchesRecording` (`test/camera/test_camera.cpp`), `test/test_zoomer.cpp` |
| The camera library's tracks: recorded flights with smooth curved segments that scripts play | done | `Data/camera.edt` is loaded by `src/3D/CameraEdits.cpp` and evaluated by `src/3D/CameraTrack.cpp`; `RUN_CAMERA_PATH` runs a track in the script camera (`script_camera::RunPath`); `CameraTrack.*` (`test/test_camera_track.cpp`), `CameraSegments.*` (`test/camera/test_camera_segments.cpp`). What the original reads past a track's ends is not known ([pending](../../../camera/script-camera.md#pending)) |

## Handing the camera over

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Other cameras (creature, temple, fight, path) take over from the player's and hand it back where they found it | partial 60% | `Camera::SetModel` swaps models (creature mode, temple, editor); the script camera starts from the player's zoomers and hands them back (`script_camera::BeginFrom`, `End`); placed paths stack among themselves (`CameraPathSystem`). Missing: the original's one stack of camera modes, where any mode goes on top of another and the one under it restarts ([GCamera](../../../camera/game-camera.md#gcamera-and-its-zoomers)) |
| A camera that is no longer valid (its thing gone) gives way to the one before it | partial 60% | the script's dual camera is dropped once a turn when its things are gone (`script_camera::Validate`, `CheckDualModes`), Creature Mode lets go when the creature goes, and a placed path goes with its effect. Missing: the original checks every mode of its stack once a turn and restarts the new top ([GCamera](../../../camera/game-camera.md#gcamera-and-its-zoomers)); nothing general |
| Some cameras stop the player drawing gestures while they move | pending | each of the original's camera modes says whether the player may gesture while the camera moves (the follow and dance cameras answer differently from the rest); the hand's casting (`src/Magic/Hand/HandCasting.cpp`) asks no camera |
| The camera's state is kept in saved games | pending | no saved games; see `../engine/` |
