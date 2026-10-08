# Challenge natives: camera, cut scenes and fades

The challenge scripts' functions for the camera: setting and gliding it, following objects, cut-scene control, the cinema bars, fades, lens, shakes and the challenge log's pictures. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 43/58 done, 3 partial — 77%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Puts the camera's eye at a position at once: *set camera position to ‹position›* (called 307 times in 85 scripts) | done | `SetCameraPosition` in `src/CHLApi.cpp`: `script_camera::SetPosition` (`src/Camera/ScriptCamera.cpp`), only in the script camera mode, as the original; `test/test_script_camera.cpp` |
| Points the camera at a position at once: *set camera focus to ‹position›* (called 290 times in 85 scripts) | done | `SetCameraFocus`: `script_camera::SetFocus`, only in the script camera mode |
| Glides the camera's eye to a position over a number of seconds: *move camera position to ‹position› time ‹time›* (called 769 times in 162 scripts) | done | `MoveCameraPosition`: `script_camera::MovePosition`, in seconds of the wall clock; `test/test_script_camera.cpp` |
| Glides the camera's point of view to a position over a number of seconds: *move camera focus to ‹position› time ‹time›* (called 737 times in 161 scripts) | done | `MoveCameraFocus`: `script_camera::MoveFocus` |
| Gives the camera's current eye position: *camera position* (called 561 times in 171 scripts) | done | `GetCameraPosition`: the drawn camera's position |
| Gives the position the camera is looking at: *camera focus* (called 263 times in 111 scripts) | done | `GetCameraFocus`: the drawn camera's focus |
| Whether an object is in the camera's view: *‹object› viewed* (called 53 times in 36 scripts) | done | `GameThingFieldOfView`: `field_of_view::ThingInView` (`src/Camera/FieldOfView.cpp`), the screen test of the thing's bounding sphere or point |
| Whether a position is in the camera's view: *‹position› viewed* (called 54 times in 30 scripts) | done | `PosFieldOfView`: `field_of_view::PosInView`, false inside the temple |
| Takes the camera from the player at the start of a cut scene (part of the language's cinema and camera blocks): *begin cinema / begin camera (opening the block)* (called 328 times in 181 scripts) | done | `StartCameraControl`: `help::script_control::StartCameraControl` and `script_camera::BeginFrom`, which takes over the player's zoomers; no camera mode inside the temple |
| Gives the camera back to the player at the end of a cut scene: *end cinema / end camera (closing the block)* (called 450 times in 238 scripts) | done | `EndCameraControl`: `help::script_control::EndCameraControl` then `script_camera::End` (the player's mode from where the camera is, the lens back to 70 degrees); a stopped task gives it back too |
| Brings the black cinema bars in or out: *the cinema block's bars (opening and closing it)* (called 754 times in 235 scripts) | done | `SetWidescreen`: `help::script_control::SetWideScreen` for the holding task; the bars slide in the help system's wide-screen time (`src/3D/ScreenFade.cpp`) |
| Whether the camera has finished a glide: *camera ready* (called 493 times in 153 scripts) | done | `HasCameraArrived`: `script_camera::ScriptArrived` in the script mode, else the player's zoomers against their destinations |
| Keeps the camera's point of view on a moving object: *move camera focus follow ‹target›* (called 4 times in 3 scripts) | done | `FocusFollow`: `script_camera::FocusFollow`; `ScriptCameraFollow` tests in `test/test_script_camera.cpp` |
| Takes a picture of the camera's view for the challenge log in the temple: *snapshot (a challenge log picture)* (called 137 times in 76 scripts) | todo | `Snapshot` logs "not implemented" (no challenge log) |
| Gives one of the camera positions placed in the landscape by name: *camera position named in the landscape* (called 13 times in 11 scripts) | done | `ConvertCameraPosition`: the "Cam" entry of the land's `camera.edt` |
| Gives one of the camera focus positions placed in the landscape by name: *camera ‹camera enum›* (called 19 times in 13 scripts) | done | `ConvertCameraFocus`: the "Cam" entry's focus |
| Starts a camera that keeps two objects in view: *begin dual camera to [object] and [object]* (called 6 times in 5 scripts) | done | `StartDualCamera`: `script_camera::StartDual` (`src/Camera/ScriptCamera.cpp`); `test/test_script_camera_dual.cpp`; there is no mode stack, so the dual always goes on top of the script mode (approximate) |
| Stops the two-object camera: *end dual camera* (called 6 times in 5 scripts) | done | `ReleaseDualCamera`: `script_camera::ReleaseDual` |
| Glides the camera in front of an object, at a distance, over a time: *move camera to face ‹target› distance ‹distance› time ‹time›* (called 14 times in 7 scripts) | done | `MoveCameraToFaceObject`: `script_camera::FaceObject` then the moves; creatures have no facing angle yet |
| Flies the camera along one of the camera paths made for the land: *camera path ‹camera enum›* (called 12 times in 11 scripts) | done | `RunCameraPath`: `script_camera::RunPath`, the land's `camera.edt` tracks (`src/3D/CameraTracks.cpp`) |
| Whether the cinema bars have finished moving: *widescreen ready* (called 17 times in 14 scripts) | done | `WidescreenTransistionFinished`: `ScreenFade::IsWideScreenTransitionFinished` |
| Loads a set of camera zones that keep the camera away from places: *set camera zones to ‹filename›* (called 10 times in 3 scripts) | partial | `SetCameraZone`: `player_camera::SetCameraZone` (`src/Camera/PlayerCameraScript.cpp`) loads the zone file; the player's camera does not read the zones yet |
| Sets the following camera's distance, speed and angle and whether it stays behind: *set camera properties distance ‹distance› speed ‹speed› angle ‹angle› enable/disable behind* (called once in 1 script) | done | `CameraProperties`: `script_camera::SetFollowProperties` (distance, speed, angle, behind) |
| Shakes the camera, strongest near a position, for a time: *shake camera at ‹position› radius ‹radius› amplitude ‹amplitude› [time ‹duration›]* (called 31 times in 20 scripts) | done | `ShakeCamera`: `camera_shake::StartCameraShake` (`src/Camera/CameraShake.cpp`), applied to the drawn camera in any mode outside the temple; `test/test_script_camera_dual.cpp` |
| Turns a recorded sequence on an object on or off (unconfirmed): *enable/disable ‹avi sequence› avi sequence* (called 2 times in 2 scripts) | done | `SetAviSequence`: sequence 1 plays the intro film `intro.bik` with its pause and fade, sequence 2 the falling-spell film (`src/Video/`). Our wiki differs: it starts one of the game's full-screen films (1 the intro, 2 the falling spell), not a sequence on an object ([video](../../bw1-notes/video.md#the-five-videos)) |
| Updates the challenge log picture with what is going on: *update snapshot (the challenge log picture)* (called 110 times in 60 scripts) | todo | `UpdateSnapshot` logs "not implemented" |
| Fades the screen to a colour over a time: *set fade red ‹red› green ‹green› blue ‹blue› time ‹time›* (called 116 times in 48 scripts) | done | `SetFade`: `ScreenFade::FadeTo`, every argument truncated |
| Fades the screen back in over a time: *set fade in time ‹duration›* (called 110 times in 47 scripts) | done | `SetFadeIn`: `ScreenFade::FadeBackToNormal` |
| Whether the fade has finished: *fade ready* (called 171 times in 38 scripts) | done | `FadeFinished`: `ScreenFade::IsFinished` |
| Keeps the camera looking at an object as it moves: *set camera focus follow ‹target›* (called 32 times in 27 scripts) | done | `SetFocusFollow`: the same code as focus follow |
| Keeps the camera's eye moving with an object: *set camera position follow ‹target›* (called 5 times in 5 scripts) | done | `SetPositionFollow`: `script_camera::PositionFollow` and `PlaceFollowNow` |
| Keeps the camera following an object at a distance: *set camera follow ‹target› distance ‹distance›* (called 2 times in 2 scripts) | done | `SetFocusAndPositionFollow`: `script_camera::FocusAndPositionFollow` and `PlaceFollowNow` |
| Puts the camera's lens back to normal: *reset camera lens [time ‹lens›]* (called once in 1 script) | done | `SetCameraLens`: the lens back to 70 degrees over the given time, as the original reads its argument |
| Changes the camera's lens (field of view) over a time: *set camera lens ‹lens› [time ‹time›]* (called 25 times in 7 scripts) | done | `MoveCameraLens`: `script_camera::SetFov` over the time, in game seconds |
| Whether an object can see the camera within an angle: *‹object› can view camera in ‹degrees› degrees* (called 4 times in 3 scripts) | todo | `GameThingCanViewCamera` logs "not implemented" and pushes false |
| Updates the picture of a challenge log entry: *update snapshot picture* (called 13 times in 4 scripts) | todo | `UpdateSnapshotPicture` logs "not implemented" |
| Gives a position a distance in front of the camera: *facing camera position distance ‹distance›* (called 7 times in 4 scripts) | done | `GetFacingCameraPosition`: the drawn camera's position plus the distance along the view (the direction is inferred) |
| Whether the camera is within a turn (unconfirmed): *within rotation* (called once in 1 script) | todo | `WithinRotation` logs "not implemented" and pushes false |
| Narrows the screen to a window over a time: *set clipping window across ‹across› down ‹down› width ‹width› height ‹height› time ‹time›* (called 2 times in 1 script) | todo | `SetClippingWindow` logs "not implemented" |
| Widens the screen back from the window over a time: *clear clipping window time ‹time›* (called once in 1 script) | todo | `ClearClippingWindow` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Keeps the camera's eye on a moving object: *move camera position follow ‹target›* (not called by the shipped scripts) | done | `PositionFollow`: `script_camera::PositionFollow`, at the thing's viewing distance, with "behind" |
| Changes the two objects the two-object camera keeps in view: *set dual camera to ‹obj1› ‹obj2›* (not called by the shipped scripts) | done | `UpdateDualCamera`: `script_camera::UpdateDual` |
| Starts the two-object camera on an object and a fixed point: *dual camera with a point (no statement in the language)* (not called by the shipped scripts) | done | `CreateDualCameraWithPoint`: `script_camera::StartDualWithPoint` |
| Puts the camera in front of an object, at a distance, at once: *set camera to face ‹value› distance ‹value›* (not called by the shipped scripts) | done | `SetCameraToFaceObject`: `script_camera::FaceObject` then position and focus set at once |
| Gives how far away things are still drawn (unconfirmed): *get inclusion distance* (not called by the shipped scripts) | partial | `GetInclusionDistance`: the player camera's inclusion distance, but nothing updates it, so it stays at its start value |
| Keeps the camera following an object at a distance: *move camera follow ‹value› distance ‹value›* (not called by the shipped scripts) | done | `FocusAndPositionFollow`: `script_camera::FocusAndPositionFollow` |
| Locks the camera's turning about a point: *enable/disable fixed camera rotation at ‹value›* (not called by the shipped scripts) | partial | `SetFixedCamRotation`: `player_camera::ForceRotateAboutPoint` records the point; the player's camera model does not read it yet |
| Remembers where the camera is: *store camera details* (not called by the shipped scripts) | done | `StoreCameraDetails`: the drawn camera's position and focus kept in the script camera's state |
| Puts the camera back where it was remembered: *restore camera details* (not called by the shipped scripts) | done | `RestoreCameraDetails`: `script_camera::SetPositionAndFocus` to the stored ones (also the player's camera outside the script mode, inferred) |
| Sets the camera's eye, view and lens at once: *set camera position ‹value› focus ‹value› lens ‹value›* (not called by the shipped scripts) | done | `SetCameraPosFocLens`: position, focus and lens at once, the lens not turned into radians as the original |
| Glides the camera's eye, view and lens over a time: *move camera position ‹value› focus ‹value› lens ‹value› time ‹value›* (not called by the shipped scripts) | done | `MoveCameraPosFocLens`: position, focus and lens moved over the time |
| Lets the camera clip close to things, or puts it back: *enable/disable clipping distance [‹value›]* (not called by the shipped scripts) | todo | `SetGraphicsClipping` logs "not implemented" |
| Gives the remembered camera eye: *stored camera position* (not called by the shipped scripts) | done | `GetStoredCameraPosition`: the stored position |
| Gives the remembered camera view: *stored camera focus* (not called by the shipped scripts) | done | `GetStoredCameraFocus`: the stored focus |
| Turns drawing the sun on or off: *draw the sun on/off (no statement in the language)* (not called by the shipped scripts) | todo | `SetSunDraw` logs "not implemented" |
| Sets one of the player's camera bookmarks to a position: *set bookmark ‹value› to ‹value›* (not called by the shipped scripts) | todo | `SetBookmarkPosition` logs "not implemented" |
| Keeps the camera looking at a rival god's hand: *set camera focus follow computer player ‹player›* (not called by the shipped scripts) | todo | `SetFocusFollowComputerPlayer` logs "not implemented": there are no computer players |
| Keeps the camera's eye moving with a rival god's hand: *set camera position follow computer player ‹player›* (not called by the shipped scripts) | todo | `SetPositionFollowComputerPlayer` logs "not implemented": there are no computer players |
