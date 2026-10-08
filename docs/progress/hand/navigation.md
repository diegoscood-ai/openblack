# Navigation

The player moves around the island with the hand: gripping the land and dragging it, dragging by the edges of the screen to
turn and tilt the camera, and turning with the middle button or both buttons. This file covers what the hand does while
it moves the camera; the camera's own keys, zoom, limits, paths and focusing are in [../camera/](../camera/).

**Progress: 17/40 done, 16 partial — 62%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## Gripping and dragging the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Holding the Move button with land under the cursor grips the land at the exact point under the cursor | done | The hand keeps the grabbed land point (`HandSystem::Place`, camera state); the camera drags in `DefaultWorldCameraModel` (`Mode::DraggingLandscape`, the MOVE action) |
| Dragging keeps the gripped land under the cursor, sliding the camera across the island | partial | `DefaultWorldCameraModel::UpdateModeDragging` drags along a level plane through the grabbed point; raffclar's `camera_pan` (`src/Camera/CameraPan.cpp`, test `CameraPan.TheLandUnderTheCursorStaysUnderIt`) is in our tree but not wired |
| On a slope the plane the land is dragged along leans back towards the camera | partial | Only in the dormant `camera_pan` (test `CameraPan.OnASlopeThePlaneLeansBackTowardsTheCamera`); the wired drag uses a level plane |
| How far the camera moves is limited by how far the cursor went | partial | Only in the dormant `camera_pan` (test `CameraPan.TheMoveIsLimitedByHowFarTheCursorWent`); the wired drag's limit is commented out in `UpdateModeDragging` |
| Land gripped too far ahead isn't dragged until the button is let go | partial | Only in the dormant `camera_pan` (`k_MaxGripDepth`); the wired drag has no grip depth limit |
| Pressing with only sky under the cursor drags nothing | done | No land under the cursor means no hand position, so `DefaultWorldCameraModel::HandleActions` never enters the drag mode; dormant test `CameraPan.LookingAboveTheLandDragsNothing` |
| The camera stops short of land, or of the sea, in its way while dragging | partial | The wired camera only keeps its altitude above the land (`DefaultWorldCameraModel::ConstrainCamera`); the stop short of land in the way is only in the dormant `camera_pan` (tests `CameraPan.StopsThreeShortOfLandInItsWay`, `CameraPan.TheSeaIsMetGoingDownNearTheCamera`) |
| Gripping the land plays one of six grab sounds | done | `HandSystem::GripLandSound` (`HandFish.cpp`): a random one of 6 G_HandGrabLand samples, from `HandSystem::Place` |
| Gripping the land throws up a puff of dust where the hand takes hold | done | The grip packet makes spot visual 2 (grip dust) at the next turn (`HandSystem::Place`, `HandTurn.cpp`) |
| Gripping the sea splashes a ring on the water and plays the ten water sounds in turn | done | `HandSystem::SplashHand` (`HandFish.cpp`): a water ring and the next of the ten G_HandInWater samples in turn |
| Gripping the sea scares the fish near the hand | done | `HandSystem::SplashHand` calls `ecs::SplashWater`, which the fish shoals flee (`src/ECS/FishShoals.cpp`) |
| Gripping is silent while a script holds the cinema bars | done | `HandSystem::Place` makes no dust nor sound while a script's widescreen is on |
| How far away the hand can reach and grip is set by the land's interface level | done | SET_INTERFACE_INTERACTION sets the reach through `HandSystemInterface::SetHandReach` (`src/Help/InterfaceInteraction.cpp`); tests `InterfaceInteractionTest.EveryLevelAsTheJumpTable`, `InterfaceInteractionTest.InvalidLevelsLeaveTheReach` (`test/gui/test_interface_interaction.cpp`) |

## Dragging by the edges of the screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cursor is measured from the middle of the screen, with the cinema bars measured by a 16:9 picture | partial | Only in raffclar's `camera_drag` (`src/Camera/CameraDrag.cpp`, tests `CameraDrag.CursorIsMeasuredFromTheMiddle`, `CameraDrag.TheCinemaBarsMeasureByA16To9Picture`), in our tree but not wired |
| Pressing away from the edges grips the land at once | partial | Dormant `camera_drag` only (test `CameraDrag.PressingAwayFromTheEdgesGripsAtOnce`); the wired camera grips anywhere |
| Near the sides and bottom a press is undecided until the mouse has moved far enough, then becomes a pan or a turn | partial | Dormant `camera_drag` only (tests `CameraDrag.NothingIsDecidedUntilTheMouseMovesFarEnough`, `CameraDrag.TheSidesAndBottomOfferTurning`) |
| A quick drag from the side towards the middle pans instead of turning | partial | Dormant `camera_drag` only (test `CameraDrag.AQuickDragFromTheSideTowardsTheMiddlePans`) |
| Turning by the edge holds the cursor on a ring around the middle and turns the camera by the angle swept round it | partial | Dormant `camera_drag` only (tests `CameraDrag.EdgeRotateHoldsTheCursorOnTheRing`, `CameraDrag.EdgeRotateTurnsByTheAngleSweptRoundTheMiddle`) |
| At the top, dragging up and down tilts the camera and across turns it | partial | Dormant `camera_drag` only (test `CameraDrag.AtTheTopUpAndDownTiltsAndAcrossTurns`) |
| A drag down the whole screen tilts by seven thirds of the field of view | partial | Dormant `camera_drag` only (test `CameraDrag.ADragDownTheWholeScreenTiltsBySevenThirdsOfTheFieldOfView`) |
| A quick tilt downwards over land pans instead | partial | Dormant `camera_drag` only (test `CameraDrag.AQuickTiltDownOverLandPans`) |
| Watching a fight, the camera offers no tilting | todo | No fight camera rule (`DefaultWorldCameraModel.cpp` has a TODO for it) |
| Scripts choose which of pan, turn, tilt and zoom the drags offer | done | `camera_help::GetEnabledFeatures` gates zoom, rotate, pitch, the land grab and the double-click flight in `DefaultWorldCameraModel` (`src/Camera/CameraHelp.cpp`); test `InterfaceInteractionTest.EnableCameraFeaturesMask` |
| With a joystick the edge hints start further in | todo | No joystick input in our tree |

## Turning with the middle button or both buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The middle button turns and tilts the camera at the game's rates while the cursor stays frozen | done | The middle button (ROTATE_AROUND_MOUSE_ON) turns the camera in `DefaultWorldCameraModel` (arc ball) with the cursor in relative mode, so it stays put (`Game::ProcessEvents`) |
| Letting go puts the pointer back where it was frozen, so the hand doesn't jump to a new place | done | `Game::ProcessEvents` warps the pointer back to where the middle button went down (`MouseButtonsState::middlePressPosition`) |
| While turning, the hand keeps its normal hover on the line of sight through the frozen cursor | partial | The middle button counts as a grip for the hand (`input::HandGripping`), so the hand takes the grip, not its hover |
| Both buttons zoom with up and down, and turn only once the mouse has moved far enough across | partial | Both buttons zoom with the mouse's up and down (TWO_BUTTON_CLICK, with the JustZoom feature, `DefaultWorldCameraModel::HandleActions`); the turn is only in the dormant `camera_drag::TwoButtonTurn` (test `CameraDrag.BothButtonsTurnOnlyOnceMovedFarEnoughAcross`) |
| Both buttons never grip the land | done | `GameActionMap::Frame` clears the MOVE binding while both buttons are held, so the camera does not drag |
| Holding both the zoom and rotate modifiers eases in the clear view, during which the hand grips | todo | Not in our tree (only a `clearViewGrip` field in `CameraModel.h`) |

## The hand's hints while navigating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hovering by the sides or bottom with nothing under it, the hand takes its turning pose; at the top, its tilting pose | todo | Not in our tree: the Crotate and Cpitch hint clips are not ported (`HandSystem::Update`) |
| The hint poses stand the hand up towards where the camera looks | todo | Not in our tree |
| Dragging, the hand grips while panning, and shows turning or tilting while it does those | partial | Gripping plays Cgrip (`HandSystem::Update`); the turning and tilting poses are not ported |
| Showing that it turns the camera, the hand is drawn a third of its height higher | todo | Not in our tree |
| Every change of state or pose fades the hand from where it was drawn over 0.13 seconds | done | The 0.13 s state blend at each change of the hand's state (`HandCrossFade` (`src/3D/HandCrossFade.h`, the hand system's `_stateBlend`), started in `HandSystem::Update`); test `HandCrossFade.FadesAtAnEvenPaceOverATenthAndAThirdOfASecond`. Our wiki differs: the original blends only at a change of the hand's state, not at a change of clip inside a state ([page](../../bw1-notes/hand-and-interface.md#the-hands-clip-handstatenormal)) |

## Recorded hand demos

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial plays recorded hand movements that drive the real hand and camera (dragging, casting, giving wood …) | done | `src/Input/HandDemo.cpp` (`hand_demo::Play`, `Update`) replays the records through the real hand and sets the camera; the CHL opcode is in `src/CHLApi.cpp`; no unit test. The recorded throw block is not applied (hand-and-interface.md, Pending) |
| Scripts ask whether a hand demo is playing and wait for its triggers | done | IS_PLAYING_HAND_DEMO and HAND_DEMO_TRIGGER in `src/CHLApi.cpp` (`hand_demo::IsPlaying`, `ConsumeTrigger`) |
| Scripts set which keys a hand demo shows being pressed | done | SET_HAND_DEMO_KEYS is an empty handler, as the original's (`src/CHLApi.cpp`) |
| A demo can pause the game and leave the player's hand alone while it plays | done | `hand_demo::Play` takes the wait-for-trigger flag (the time stands still while waiting) and the keep-hand flag (without it the held object is dropped) |

## Network play

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's and camera's movements are sent to the other players, throttled, so they see each other's hands | todo | No multiplayer in our tree |
