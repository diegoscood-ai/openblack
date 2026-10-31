# Camera help

What the world's camera lets the player do, as the land's scripts allow it. The tutorials take most camera features away
and give them back one by one, can make the camera tilt itself, and show the player which key or button to use.

**Progress: 15/20 done, 0 chosen, 3 partial, 0 inferred — 82%**

How the original does it, in our wiki: [player camera features](../../../camera/player-camera.md#player-camera-features-camerahelp).

## Features the scripts allow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tutorial interface level sets which camera features (zoom, turn, tilt, move, turn round the mouse) the player has | done | `help::interface_interaction::Set` (`src/Help/InterfaceInteraction.cpp`) writes `camera_help::EnableCameraFeatures` (`src/Camera/CameraHelp.cpp`) for every `SET_INTERFACE_INTERACTION` level; `InterfaceInteractionTest.EveryLevelAsTheJumpTable` (`test/gui/test_interface_interaction.cpp`), `CameraHelp.EnableReplacesOnlyTheMaskedBits` |
| Without a feature its controls do nothing | done | `DefaultWorldCameraModel::Update` zeroes the zoom, turn and tilt input without their bits, `HandleActions` needs the double-click and land-grab bits for the flight and the drag, the edge drags need turn and tilt, and `TiltZoom` needs the land-grab bit for the movement keys (`WorldCameraMouse.AnEdgeDragDoesNothingOnceTurningIsTakenAway`, `WithoutTheirFeature*`, `TheMovementKeysDoNothingWithoutTheLandGrab`) |
| Turning round the mouse with the middle or both buttons needs its own feature | done | `camera_help::Feature::JustZoom` (0x20) in `DefaultWorldCameraModel::HandleActions` and `src/Game.cpp`'s cursor freeze; `WorldCameraMouse.WithoutTheirFeatureTheMiddleButtonAndBothButtonsDoNothing` |
| Interface levels can take the camera keys away, or just the keys that fly to the temple, the creature and the realm | done | `interface_interaction::BlockedActions` is given to the action map every frame and at each `Set` (`GameActionInterface::SetBlockedActions`, `src/Game.cpp`), so a blocked action reads as not held everywhere; `InterfaceInteractionTest.ControlMapActionGate`, `BlockedActionsAsTheControlMap` ([picking](../../../hand/picking.md)). Differs only after rebinding: openblack masks every key of a blocked action, the original stops at the first |
| The first tutorial level shortens how far from the camera the hand reaches | done | `interface_interaction::LevelHandReach` hands the reach to `HandSystem::SetHandReach`, which clamps the hand's distance in `HandPlacement.cpp`; `InterfaceInteractionTest.InvalidLevelsLeaveTheReach` |
| Unknown levels change nothing | done | `InterfaceInteractionTest.InvalidLevelsOnlyStoreTheLevel` |
| A new land gives every feature back | done | every land load calls `camera_help::Reset` (`src/Game.cpp`): the features back to `camera_help::k_NormalFeatures` and the self-tilt's angle and height to their start, as the original's map clear writes them; the scripts' restart (a new game, a playground's start) also sets the interface interaction back to normal (`interface_interaction::Set(0)` in `Game::ResetScripts`), which a land change keeps ([what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps), 2026-10-22); `CameraHelp.ResetGivesBackEveryStartValue`; the Land Island window shows the level |
| The game starts with every feature but the self-tilting camera | done | `camera_help::k_NormalFeatures` (0x1BF, every bit but auto-pitch) is the start value in `src/Camera/CameraHelp.cpp`, with the self-tilt's start angle the exe's float bit for bit (π/6, 0x3F060A92); `CameraHelp.StartsWithTheOriginalsValues` |
| The camera features a script set are kept in saved games | pending | (added) the original saves and loads them with the game; no saved games in openblack ([pending](../../../camera/player-camera.md#pending)) |

## The self-tilting camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera tilts itself towards a set pitch, a fifth of the way at most each frame, while the land isn't gripped, and the player can't tilt it meanwhile | done | `camera_help::AutoPitchInput`, `DefaultWorldCameraModel::HandleActions`; `CameraHelp.TheSelfTiltingCameraTiltsAFifthOfTheWayAtMostTheFramesSeconds`, `WorldCameraMouse.TheSelfTilting*`, `GrippingTheLandStopsTheSelfTiltingCamera` |
| While self-tilting, or with nothing done, the camera keeps to a set height over the land | done | `DefaultWorldCameraModel::Update`; `WorldCameraMouse.TheSelfTiltingCameraTiltsAFifthOfTheWayAndKeepsToItsHeight`, `TheSelfTiltingCameraDropsThePlayersTiltAndKeepsItsHeightOnlyWhenIdleOrTilting` (the height read with the land's own height function, not the map's fixed point: approximate in the wiki) |
| The tutorial levels that turn the self-tilting camera on set the pitch and height it tilts to | done | levels 1, 2 and 10 store 0.448799 and 15 (`k_TutorialAutoPitchAngle`, `k_TutorialAutoPitchDistance` in `src/Help/InterfaceInteraction.h`), the original's only three callers of that setter; the defaults are π/6 and 75; `InterfaceInteractionTest.AutoPitchParameters`, `CameraHelp.AutoPitchSetsItsBitAndKeepsItsParameters`. (Re-worded 2026-10-21: no script sets its own values in the original either) |

## Fixed rotation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can make the camera turn only about a fixed point | partial 30% | `SET_FIXED_CAM_ROTATION` stores the point as the original does (`player_camera::ForceRotateAboutPoint`, `src/Camera/PlayerCameraScript.cpp`; faithful in the wiki). Missing: `DefaultWorldCameraModel` does not turn about it, and nothing clears it as the original's camera reinitialisation does. No land uses it ([zones and fixed rotation](../../../camera/player-camera.md#zones-and-fixed-rotation)) |
| Scripts can ask whether the camera has been turned to within the wanted rotation | partial 90% | `WithinRotation` in `src/CHLApi.cpp`: whether the player's camera shows any helper icon by the hand (the word the icons' fade keeps on the camera, `Camera::GetShownHandIcons`), as the original; while no icons are faded (a box up, the temple) the last word is kept ([../../../camera/player-camera.md](../../../camera/player-camera.md#pending)); tests `WithinRotationNative.*` |

## Showing the player the controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial shows glowing pictures of the key or mouse button to use, next to its text | partial 70% | the key and mouse pictures are `help::input_prompt` (`src/Help/InputPromptIcon.cpp`), drawn by `Renderer::DrawKeyOrMouse` for the help text's control icon and the click cue, ported from the original's draw ([tooltips](../../../interface/tooltips.md)); tests `InputPromptIcons.*`. Missing: not compared with the game on screen |
| The game counts how often and how recently each camera control was used, to remind the player of ones they don't use | done | `help_profile::OnPlayerCameraMove` and `CameraHelpCallback` (`src/Help/HelpProfile.cpp`) count the player's zoom, turn, tilt, drags and double-click flights for `GET_TOTAL_EVENTS`, `GET_TIME_SINCE`, the edge drag's quirk included. Each frame the movement keys move the camera counts as a drag, as for a pan of the land (`WorldCameraMouse.EachFrameOfAMoveWithTheKeysCountsAsADrag`). It goes with the original's input mask (fork issue #216; `help_profile::InputMask`: the keys, the land gripped, both buttons, the wheel, the middle button) and in every mode of the camera, as the original's zoom in every update and its turn and tilt with any such input (`PlayerCameraReports.*`, `CameraInputMask.*`, `WorldCameraMouse.AKeysTurnCountsWithTheKeysAsItsInput`, `AZoomWhileTheLandIsGrippedCounts`) ([player camera features](../../../camera/player-camera.md#player-camera-features-camerahelp)); the Camera debug window's "The camera help" shows the last update's input and reports |
| Only the player's own camera controls count: not a script's camera, a pull-back, a flight or a hand demo | done | the counts come only from the player's camera model, in the update of the frame whose controls it read: `Camera::SkipActions` (the game's input step while a script's camera, a hand demo or a camera path has the camera) and `Camera::SetModel` (a model handed the camera) leave an update with none, as the original's player camera starts again with no input when the camera comes back to it (2026-10-23, fork issue #216; `WorldCameraMouse.TheCameraHelpHearsNoControlsLeftFromAFrameWithoutAnUpdate`, `AModelHandedTheCameraHearsNoControlsItReadBefore`). Those controls no longer move the camera either (`DefaultWorldCameraModel::SkipActions`, [player camera](../../../camera/player-camera.md#openblack)) |
| Reminders such as the zoom reminder play once a control has gone unused | partial 50% | the tutorial scripts read those counts through `GET_TIME_SINCE` and the total counts (`GetTotalEvents` in `src/CHLApi.cpp`, which is the original's native for every event the scripts ask about: [advisors-and-help.md](../../../interface/advisors-and-help.md#timers-help-events-and-field-of-view)); whether every reminder plays as in the game is not checked (see `../story/`) |

## During fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Watching the player's creature fight, the camera can't be sent to another fight | done | the original takes the double click's flight away while a fight is watched, and only starts watching an arena it lingers over or is double clicked onto when not fighting ([watching a fight](../../../creature/fights.md#player-camera-watching-a-fight)); openblack drops the double-click feature during the watch (`camera_help::DuringFight`; `WorldCameraFight.NoDoubleClickFlightWhileWatching`, `FightOrbit.WatchingTakesOnlyTheDoubleClicksFlightAway`). Being sent to a fight by lingering or a double click is in [follow_cameras.md](follow_cameras.md) |
| Watching a fight, the edges of the screen offer no tilting | done | `DefaultWorldCameraModel::HandleActions` clears the tilt hint while a fight is watched, as the original's hints do; `WorldCameraFight.NoTiltHintFromTheScreensEdgeWhileWatching` |
