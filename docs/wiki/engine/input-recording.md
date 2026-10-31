# Recording and replaying the input

## Summary

To check long stretches of play automatically (the quests of a land, the tutorial past the points where it waits for
the player), openblack can record the raw input of a run (the mouse's motion, buttons and wheel, and the keys) frame by
frame, and play it back in a later run. It is openblack's own test tool (user, 2026-10-21): off unless asked for, and
the game is the same without it. The original has a playback of its own, for the tutorial's hand demos, which
openblack already ports; it keeps the interface's messages and the camera, not the input.

## The original

- The tutorial's hand demos, `Data\HandDemo\<name>.hnd`, are played by the scripts' `PLAY_HAND_DEMO` (the string
  `.\Data\HandDemo\%s.hnd`). Each file is a list of 124-byte records with no header: the interface message (0 move,
  1 grab down, 2 grab up, 3 action down, 4 action up), the hand's throw block, the normalised mouse, the camera's eye
  and focus, the camera hints, a trigger key state, the time in ms and the thing under the hand. The full layout is in
  `src/Input/HandDemo.h` (`hand_demo::Record`), and the playback rules in
  [the hand demos](../project/debugging.md#hand-demos).
- The interface also has a developer recorder and player of its own: it records to and plays `InterfaceRecord.hnd`
  in the game's folder (`GInterface::StartPlayBack` and the recorder that opens the same name for writing). Nothing in
  the shipped game starts them. Its records are the same interface messages as the hand demos.
- Neither keeps the keyboard (beyond the one trigger key) or the wheel, and both replay at the interface's level, so
  they cannot drive menus, the keys, gestures drawn with the keyboard's help or anything outside the hand.

## openblack

- **Format** (`src/Input/InputRecording.{h,cpp}`, pure; tests `InputRecording.*`): a 16-byte header, the magic `OBIN`,
  a version (1) and the window's size when recorded, then 32-byte events: the frame counted from the recording's
  start, the kind (1 mouse motion, 2 mouse button, 3 wheel, 4 key) and six values (the position, the relative motion
  and the buttons held; the button, up or down, the clicks and the position; the wheel's x, y and direction; the
  scancode, key code, modifiers, repeat and up or down). A file of another magic or version is refused.
- **Where it sits** (`src/Input/InputReplay.{h,cpp}`, kept in `Locator::inputState`): at the one point where the
  window's events enter the game (`Game::Update`'s dispatch), beside `OPENBLACK_FIXED_MOUSE`,
  `OPENBLACK_IGNORE_REAL_INPUT` and `OPENBLACK_MOUSE_AT`. Nothing in the game's logic knows of it. The date a run
  reads is pinned apart, by `OPENBLACK_CLOCK_AT` ([testing](../project/testing.md#deterministic-conditions)).
  - Recording keeps every real mouse and key event dispatched, at the frame it came in (the game's frame count,
    `Locator::screenshotRequest`), and writes the file as the run ends (or from the debug bar).
  - Playing gives each recorded event at its frame, after the window's own, through the same dispatch, so the input
    map, the hand and the menus take them as real ones. Meanwhile the real mouse and keys are held back, the recorded
    mouse moves the game's cursor (not the real pointer), and the input map reads no real button or key state, as in
    the other test runs.
- **Switches:** `--record-input <file>` and `--replay-input <file>` from the run's first frame; the debug bar's
  Debug > Input Recording records to and plays `input_recording.obin` in the working folder from the current frame,
  stops either, and shows the frame and the events played or kept.
- **Same run every time:** a replay matches its recording when both run with a fixed frame time
  (`OPENBLACK_FIXED_FRAME_MS`) and the same seed, so that frame N of one is frame N of the other. The replay run may
  add `OPENBLACK_IGNORE_REAL_INPUT=1`; the recording run cannot, since that switch drops the real events before the
  recorder sees them (`input::DropRealEvent` in `Game::PlayFrameStep`'s event dispatch).
- **With the testbed's pointer:** a testbed command that drives the pointer (`PointerTo`, `PointerPress`,
  `PointerRelease`, `PointerSweep`, `PointerDoubleClick`, `WheelTurn`) takes it through the fixed cursor
  (`Runner::GivePointerCommand`, `src/Debug/TestbedScenarioRunner.cpp`) and keeps it until the scenario's
  timeline is done and its buttons are let go (`Runner::UpdatePointer`). Meanwhile the game reads no real
  mouse, so the player's own mouse cannot move the hand; the key commands do not take the pointer. A recording made
  beside such a timeline keeps only what the player did before the first pointer command, and the keys.

- **The debug bar follows a replay:** the vendored ImGui SDL backend reads the real pointer's place every frame the
  window has the focus and no button is down (`ImGui_ImplSDL2_UpdateMouseData`, `SDL_GetGlobalMouseState`), over the
  motions a recording gives, so a replayed click on the debug bar landed where the real pointer was. While a recording
  plays, `Gui` keeps the last motion it was given and gives it to ImGui again after the backend's frame
  (`io.AddMousePosEvent`); the vendored code is not touched. Verified (2026-10-24) by a crafted recording on the
  testbed (`tools/saves/replay.py`): Escape opens the pause menu and its debug bar, and the replayed clicks open Load
  Island > Saved Games and load a slot.

## Pending

- It has been used once to record (2026-10-22): a recording run (`--record-input`, without
  `OPENBLACK_IGNORE_REAL_INPUT`) of Land 1's tutorial, in which the player's own mouse took the hand into the temple.
  A replay of that recording, and above all one on a later build, is not verified yet: the recorded events are screen
  pixels at frame numbers, so any change that moves a frame or the camera puts them out of step. The first check left
  is to replay it twice on the same build with identical state hashes.
- The window size stored in the header is not checked on replay.
- Text typed into a box (the new player's name) comes through SDL's text events, which a recording does not keep yet.
- A mouse button pressed with a modifier key reads the modifier from the real keyboard's state, not the recorded one.
- A player of the original's `.hnd` interface records at the input level (turning each record into input events)
  is not made: the hand demos already play through `hand_demo`.
