# World camera controls

How the player moves the camera over the island: the movement, turn, tilt and zoom keys, the mouse wheel, the middle
button, both buttons together, dragging round the edge of the screen, and double clicking to fly somewhere. Dragging the
land itself with the hand is in [../hand/navigation.md](../hand/navigation.md).

**Progress: 11/31 done, 16 partial — 61%**

## Keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The arrow keys move the camera forwards, backwards, left and right over the land | done | `src/Camera/DefaultWorldCameraModel.cpp` (`HandleActions`, `TiltZoom`); recorded game runs `MoveBackwardForward`, `MoveRightLeft` in `test/camera/test_camera.cpp` |
| A movement key's step grows with the camera's height above what it looks at, within set bounds | partial | the step in `DefaultWorldCameraModel::TiltZoom` scales only with the tilt (a fixed scale of 60, not the height); it matches the recorded runs, all taken at one height |
| The camera moves the same distance per second whatever the frame rate | done | `k_InteractionSpeedMultiplier` times the frame's seconds in `DefaultWorldCameraModel::HandleActions`; `KeyboardMoveSpeed.DefaultLeavesTheGamesDistanceUntouched` (`test/camera/test_keyboard_move_speed.cpp`) |
| Shift with the arrow keys turns the camera round what it looks at and tilts it | done | `ROTATE_ON` in `DefaultWorldCameraModel::HandleActions`; recorded runs `TiltUpDown`, `TiltUpPanLeft` |
| Ctrl with the arrow keys zooms in and out | partial | `ZOOM_ON` handled in `DefaultWorldCameraModel::HandleActions`; no recorded run of the keys themselves |
| The turn keys turn the camera left and right | partial | `ROTATE_LEFT`, `ROTATE_RIGHT` bound in `src/Input/KeyBindings.h`; not compared with a recording of the game |
| The tilt keys tilt the camera up and down | partial | `TILT_UP`, `TILT_DOWN` bound; not compared with a recording of the game |
| Holding Ctrl and Shift together eases in the clear view over half a second, and the hand grips while it does | todo | the world camera has no clear view in our tree; only the creature's camera has one (see [follow_cameras.md](follow_cameras.md)) |
| Moving, turning and zooming keys do nothing while a script's cinema bars are in or the camera is not the player's | partial | the keys are skipped while a script's camera drives or a hand demo plays (`src/Game.cpp`); a script's cinema bars alone do not stop them |
| A setting to speed the movement keys up or down | n/a | openblack-only (editor camera speed), `src/Camera/KeyboardMoveSpeed.h`; see `../debug/` |

## Mouse wheel and buttons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wheel zooms towards and away from what the camera looks at, a fixed step per notch | done | `ZOOM_IN`, `ZOOM_OUT` with `k_WheelZoomFactor` in `DefaultWorldCameraModel.cpp`; recorded run `ZoomOutIn` |
| Zooming speeds up with height, so a notch covers about the same share of the view at any altitude | done | the zoom scale from the height between camera and focus, 60 to 240 or 2000, in `DefaultWorldCameraModel::Update` (as our [script camera](../../bw1-notes/script-camera.md#player-camera-features-camerahelp) page says); recorded runs `ZoomOutIn`, `TiltDownZoomOut` |
| Holding the middle button turns and tilts the camera round the point under the mouse | done | `Mode::ArcBall`; recorded run `MiddleDragRightUp` |
| Holding both buttons and moving up and down zooms | done | `TWO_BUTTON_CLICK` with the second zoom bit; recorded run `TwoButtonZoomOutIn` |
| Holding both buttons and moving across turns the camera, once the mouse has moved far enough across | partial | `camera_drag::TwoButtonTurn` (`CameraDrag.BothButtonsTurnOnlyOnceMovedFarEnoughAcross`, `test/camera/test_camera_drag.cpp`) is not wired yet |
| While the mouse turns the camera the cursor and the hand stay where they were, and the pointer is put back there after | partial | only for the middle button: relative mouse mode while held and the pointer put back where it was pressed (`src/Game.cpp`); not for both buttons |
| Double clicking the land flies the camera there along a curved flight | done | `Mode::FlyingToPoint`, `CharterFlight`; recorded run `DoubleClickFlyTo` |
| The flight picks the heading that gives the clearest look at the place, avoiding hills in the way | done | the flight scores 32 headings in `DefaultWorldCameraModel::UpdateModeFlying`; recorded run `DoubleClickFlyTo` |
| A flight plays one of four whoosh sounds as it starts | done | `DefaultWorldCameraModel::PlayWoosh` (one of four whooshes by the tick count), from the double click's flight and `SetFlight` beyond 150 |

## Dragging round the edge and at the top

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A drag decides what it is from where it was pressed and how the mouse first moves; nothing happens until it has moved far enough | partial | `camera_drag::DragClassifier` (`CameraDrag.NothingIsDecidedUntilTheMouseMovesFarEnough`) is not wired into the camera yet |
| A drag started at the sides or bottom of the screen turns the camera round its focus by the angle the cursor sweeps round the middle | partial | `camera_drag::EdgeRotate` (`CameraDrag.EdgeRotateTurnsByTheAngleSweptRoundTheMiddle`); not wired yet |
| While turning round the edge the cursor is held on a ring around the middle of the screen | partial | `CameraDrag.EdgeRotateHoldsTheCursorOnTheRing`; not wired yet |
| A quick drag from the side in towards the middle pans instead of turning | partial | `CameraDrag.AQuickDragFromTheSideTowardsTheMiddlePans`; not wired yet |
| A drag from the top of the screen tilts the camera when moved up and down and turns it when moved across | partial | `CameraDrag.AtTheTopUpAndDownTiltsAndAcrossTurns`; not wired yet |
| A drag down the whole screen tilts by seven thirds of the field of view across | partial | `CameraDrag.ADragDownTheWholeScreenTiltsBySevenThirdsOfTheFieldOfView`; not wired yet |
| A quick tilt down over land pans instead | partial | `CameraDrag.AQuickTiltDownOverLandPans`; not wired yet |
| Pressing away from the edges grips the land at once | done | in our tree any press grips the land (`Mode::DraggingLandscape`, when the land-grab feature is on); the edge drags are not told apart yet. The drag itself in [../hand/navigation.md](../hand/navigation.md) |
| With the cinema bars in, the mouse controls measure by the 16:9 picture | partial | `camera_drag::ViewHeight` (`CameraDrag.TheCinemaBarsMeasureByA16To9Picture`); not wired yet |

## Camera hints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Where the cursor rests, the camera offers hints of what a drag would do: turning at the sides and bottom, tilting at the very bottom, everything at the top | partial | `camera_drag::IdleTricons` (`CameraDrag.TheSidesAndBottomOfferTurning`, `TheTopOffersEverything`, `TheMiddleOffersNoHints`); not wired yet |
| The hints pick the hand's pose (see the hand's poses) | todo | `CameraModel::GetHandCues` returns nothing and has no reader yet; [../hand/](../hand/) |
| Moving, zooming or tilting with the keys sets the matching hints too | todo | `TODO(#709)` in `DefaultWorldCameraModel.cpp` |
| The game keeps how long the camera has been left alone, which other parts of the game read | todo | `DefaultWorldCameraModel::GetIdleTime` logs "not implemented" |
