# Camera help

What the world's camera lets the player do, as the land's scripts allow it. The tutorials take most camera features away
and give them back one by one, can make the camera tilt itself, and show the player which key or button to use.

**Progress: 9/18 done, 6 partial — 67%**

## Features the scripts allow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each tutorial interface level sets which camera features (zoom, turn, tilt, move, turn round the mouse) the player has | done | `help::interface_interaction::Set` (`src/Help/InterfaceInteraction.cpp`) writes `camera_help::EnableCameraFeatures` (`src/Camera/CameraHelp.cpp`) for every `SET_INTERFACE_INTERACTION` level; `InterfaceInteractionTest.EveryLevelAsTheJumpTable` (`test/gui/test_interface_interaction.cpp`) |
| Without a feature its controls do nothing | done | `DefaultWorldCameraModel::Update` zeroes the zoom, turn and tilt input without their bits, and `HandleActions` needs the double-click and land-grab bits for the flight and the drag |
| Turning round the mouse with the middle or both buttons needs its own feature | done | `camera_help::Feature::JustZoom` (0x20) in `DefaultWorldCameraModel::HandleActions` and `src/Game.cpp`'s cursor freeze; `WorldCameraMouse.WithoutTheirFeatureTheMiddleButtonAndBothButtonsDoNothing` |
| Interface levels can take the camera keys away, or just the keys that fly to the temple, the creature and the realm | partial | the levels store the camera-moves and realm-zoom switches (`interface_interaction::IsActionBlocked`, `InterfaceInteractionTest.ControlMapActionGate`), but only the bookmark number keys read them (`KeyShortcutsEnabled` in `src/Game.cpp`); `IsActionBlocked` has no caller in the game |
| The first tutorial level shortens how far from the camera the hand reaches | done | `interface_interaction::LevelHandReach` hands the reach to `HandSystem::SetHandReach`, which clamps the hand's distance in `HandPlacement.cpp`; `InterfaceInteractionTest.InvalidLevelsLeaveTheReach` |
| Unknown levels change nothing | done | `InterfaceInteractionTest.InvalidLevelsOnlyStoreTheLevel` |
| A new land gives every feature back | done | the map clear and script reboot call `interface_interaction::Set(0)` (`src/Game.cpp`), which gives back `camera_help::k_NormalFeatures` |
| The game starts with every feature but the self-tilting camera | done | `camera_help::k_NormalFeatures` (every bit but auto-pitch) is the start value in `src/Camera/CameraHelp.cpp` |

## The self-tilting camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera tilts itself towards a set pitch, a fifth of the way at most each frame, while the land isn't gripped, and the player can't tilt it meanwhile | done | `camera_help::AutoPitchInput`, `DefaultWorldCameraModel::HandleActions`; `CameraHelp.TheSelfTiltingCameraTiltsAFifthOfTheWayAtMostTheFramesSeconds`, `WorldCameraMouse.TheSelfTilting*` |
| While self-tilting, or with nothing done, the camera keeps to a set height over the land | done | `DefaultWorldCameraModel::Update`; `WorldCameraMouse.TheSelfTiltingCameraTiltsAFifthOfTheWayAndKeepsToItsHeight` |
| Scripts set the pitch and height the camera tilts to | partial | only the tutorial levels 1, 2 and 10 store fixed values (`k_TutorialAutoPitchAngle`, `k_TutorialAutoPitchDistance` in `src/Help/InterfaceInteraction.cpp`), which the camera uses; no script sets its own values |

## Fixed rotation

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A script can make the camera turn only about a fixed point | partial | `SET_FIXED_CAM_ROTATION` stores the point (`player_camera::ForceRotateAboutPoint`, `src/Camera/PlayerCameraScript.cpp`); `DefaultWorldCameraModel` does not turn about it yet |
| Scripts can ask whether the camera has been turned to within the wanted rotation | todo | `WITHIN_ROTATION` is a stub in `src/CHLApi.cpp` (always false) |

## Showing the player the controls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial shows glowing pictures of the key or mouse button to use, next to its text | partial | the key and mouse pictures are `help::input_prompt` (`src/Help/InputPromptIcon.cpp`, drawn by the renderer) for the help text's control icon and the click cue; not compared with the game |
| The game counts how often and how recently each camera control was used, to remind the player of ones they don't use | partial | `help_profile::OnPlayerCameraMove` and `CameraHelpCallback` (`src/Help/HelpProfile.cpp`) count the player's zoom, turn, tilt and double-click flights for `GET_TOTAL_EVENTS`, `GET_TIME_SINCE`; the input mask (keys, buttons, wheel) is not passed yet |
| Reminders such as the zoom reminder play once a control has gone unused | partial | the tutorial scripts read those counts through `GET_TIME_SINCE` and `GET_TOTAL_EVENTS`; whether every reminder plays as in the game is not checked (see `../story/`) |

## During fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Watching the player's creature fight, the camera can't be sent to another fight | todo | not in our tree: no camera rule for fights watched by the player's creature |
| Watching a fight, the edges of the screen offer no tilting | todo | the edge drags are `camera_drag::` (`src/Camera/CameraDrag.cpp`), not wired into the game, and have no fight rule |
