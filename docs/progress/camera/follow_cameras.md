# Following cameras

Cameras that lock onto something and follow it: the creature, the creature's fights, and the other following cameras
the game has. Script-driven following is in [script_camera.md](script_camera.md).

**Progress: 9/18 done, 3 partial — 58%**

## Following the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A key locks the camera onto the player's creature and lets go again | done | `CreatureModeSystem::PressCreatureKey` (`src/ECS/Systems/Implementations/CreatureModeSystem.cpp`) with `CreatureCameraModel` (`src/Camera/CreatureCameraModel.cpp`); `CreatureMode.CreatureKeyLocksOntoYourCreatureAndLetsGo` (`test/creature/test_creature_follow.cpp`). Whether the game's key follows or only flies there is unconfirmed |
| A double click on a creature locks onto it, another god's too | done | `CreatureModeSystem::ReadKeys` enters on a double click on any creature under the hand, when the double-click flight is allowed |
| The camera starts the creature's viewing distance away, or as close as it already is, looking at its middle | done | `src/Camera/CreatureFollow.cpp`; `CreatureFollow.StartsAtTheViewingDistanceFromAfar`, `StaysAboutAsCloseWhenAlreadyClose`, `LooksAtTheMiddleOfTheCreature`; `CreatureCameraModel.StartsOnTheCreaturesMiddleWithTheFollowsView` (`test/camera/test_creature_camera_model.cpp`) |
| It eases after the creature, arriving two seconds later at first and one second once settled | done | `CreatureFollow.EasesInTwoSecondsAtFirstAndOneLater`, `CreatureCameraModel.TheEaseSettlesToASecondAfterTwo` |
| Shift and the arrow keys turn it round the creature and tilt it | done | `CreatureFollow.ShiftAndCursorKeysTurnAndTilt`, `CreatureCameraModelTest.ShiftAndTheCursorKeysTurnAndTiltIt` |
| Ctrl and the arrow keys turn it and draw it in and out; the wheel zooms | done | `CreatureFollow.CtrlAndCursorKeysTurnAndZoom`, `TheWheelZoomsTwice`; `CreatureCameraModelTest.TheWheelDrawsItInAndOut` |
| Its distance and pitch stay within bounds | done | `CreatureFollow.KeepsWithinItsBounds` |
| Ctrl and Shift together swing it to where the land falls away, for a clear view | done | `CreatureFollow.ClearViewSwingsAwayFromAHill`, `ClearViewTiltsTowardsTwentyTwoDegrees`; `CreatureCameraModelTest.CtrlAndShiftTogetherSwingItClearOfTheLand` |
| The arrow keys alone, gripping the land, the temple, a script's cinema bars or the creature leaving give the camera back | done | `CreatureModeSystem::Update` (a fresh grip, the temple, a script's camera or bars, the creature gone); `CreatureFollow.CursorKeysAloneGiveTheCameraBack`, `CreatureCameraModelTest.TheCursorKeysAloneGiveTheCameraBack` |
| The camera keeps a thing in view smoothly unless it turns too fast to follow | todo | not in our tree (unconfirmed rule) |

## Fights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the player's creature starts a fight the camera flies to watch it from the side of the arena | partial | `CreatureFightSystem::Watch` (`src/ECS/Systems/Implementations/CreatureFightSystem.cpp`) flies the camera with `creature_fight::CameraOrigin`, `CameraSide`; but it returns while the interface is active, so in normal play it does not fly (looks like an inverted check) |
| The camera stays on the arena during the fight | partial | `CreatureFightSystem::FollowDuel` follows the fighters once they move 0.3 of the arena's radius away (`fight::k_CameraFollowShare`, `src/Creature/CreatureFight.h`); the game stays on the arena |
| Clicking near another creature fight takes the camera to watch it | todo | not in our tree |
| The fight's camera shows text and can be left, ending the fight soon after | todo | not in our tree |
| During a fight moving the mouse always turns the camera | todo | `TODO(#710)` in `DefaultWorldCameraModel.cpp` |

## Other following cameras

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A camera that follows behind a thing, keeping to its heading | partial | only the script camera's follow does it (`CAMERA_PROPERTIES` with behind, `script_camera::FollowHeading`; `ScriptCameraFollow.HeadingBehindAWallHug` in `test/test_script_camera.cpp`); no other camera uses it |
| A camera that watches a worship dance | todo | not in our tree (unconfirmed when the game uses it) |
| The camera that zooms in to the temple from outside as the player interacts with it | todo | entering the temple is a cut in our tree (`TempleInterior::Activate`, `src/3D/Implementations/TempleInterior.cpp`); see [temple_camera.md](temple_camera.md) |
| The editor's orbit and follow cameras | n/a | openblack-only, `src/Camera/EditorCameraModel.cpp`; see `../debug/` |
