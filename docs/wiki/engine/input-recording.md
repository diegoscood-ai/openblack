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
  `src/Hand/HandDemo.h` (`hand_demo::Record`), and the playback rules in
  [the hand demos](../project/debugging.md#hand-demos).
- The interface also has a developer recorder and player of its own: it records to and plays `InterfaceRecord.hnd`
  in the game's folder (`GInterface::StartPlayBack` and the recorder that opens the same name for writing). Nothing in
  the shipped game starts them. Its records are the same interface messages as the hand demos.
- Neither keeps the keyboard (beyond the one trigger key) or the wheel, and both replay at the interface's level, so
  they cannot drive menus, the keys, gestures drawn with the keyboard's help or anything outside the hand.

## openblack

- **Format** (`src/Input/InputRecording.{h,cpp}`, pure; tests `InputRecording.*`): a 16-byte header, the magic `OBIN`,
  a version (1) and the window's size when recorded, then 32-byte events: the frame counted from the recording's
  start, the kind (1 mouse motion, 2 mouse button, 3 wheel, 4 key, 5 the logo, 6 text) and six values (the position, the relative motion
  and the buttons held; the button, up or down, the clicks and the position; the wheel's x, y and direction; the
  scancode, key code, modifiers, repeat and up or down; the wheel's precise x and y since 2026-11-09, see "Live as
  replayed"). A file of another magic or version is refused.
  Kind 6 (2026-11-11) is the text a key typed, SDL's text input, which the name boxes read (the first start's new
  player box, the save room's name box, the Players page): up to 24 bytes of its UTF-8 in the six values, four to a
  value from the first byte, a longer text in several of them at the same frame, each cut between two characters
  (`recording::TextFromSdl`; tests `InputRecording.TypedTextIsKeptAndGoesBackToTheSameText`,
  `InputRecording.ALongTextIsCutBetweenCharacters`). Before, a recording kept the keys but not the text they typed, so
  a play that began at the first start's name box replayed with an empty name and waited at the box for ever (the
  user's play of joint589). Older files read as before; replay.py's `text:<frame>:<text>` step writes one.
- **Where it sits** (`src/Input/InputReplay.{h,cpp}`, kept in `Locator::inputState`): at the one point where the
  window's events enter the game (`Game::Update`'s dispatch), beside `OPENBLACK_FIXED_MOUSE`,
  `OPENBLACK_IGNORE_REAL_INPUT` and `OPENBLACK_MOUSE_AT`. Nothing in the game's logic knows of it. The date a run
  reads is pinned apart, by `OPENBLACK_CLOCK_AT` ([testing](../project/testing.md#deterministic-conditions)).
  - Recording keeps every real mouse and key event dispatched, at the frame it came in (the game's frame count,
    `Locator::screenshotRequest`). The file's header is written as the recording starts, and its events are appended
    every 300 of the recording's frames (`replay::k_FlushFrames`, `replay::FlushIfDue` after the frame's dispatch in
    `Game::Update`), at the run's end and from the debug bar, so a game that stops without ending (a crash) keeps them
    up to the last time; the format has no event count, and a file cut inside an event reads as the events before it.
    Before (2026-11-08) the file was written whole only as the run ended, and a crash left none. Tests
    `InputRecording.AHeaderAndItsEventsInPiecesAreTheWholeFile`,
    `InputRecording.AFileCutInTheMiddleOfAnEventKeepsTheEventsBefore`,
    `InputReplayTest.ARecordingGoesIntoItsFileEverySoManyFrames`.
  - Playing gives each recorded event at its frame, after the window's own, through the same dispatch, so the input
    map, the hand and the menus take them as real ones. Meanwhile the real mouse and keys are held back, the recorded
    mouse moves the game's cursor (not the real pointer), and the input map reads no real button or key state, as in
    the other test runs.
  - The playback ends only once its last events are dispatched (`replay::EndIfPlayedOut`, right after the frame's
    recorded events in `Game::Update`; `DueEvents` ends one with nothing left too), so a recording whose last event is
    a mouse move moves the game's cursor there, as its other moves do. Before (2026-10-30), it ended as its last events
    were handed over, and a last move set the cursor to the real pointer, about (0, 0). Test
    `InputReplayTest.ALastMouseMoveIsGivenWhileThePlaybackStillPlays`.
  - A playback plays on through a game started again (the skirmish's start or leave, the Players page's Restart,
    `Game::StartAgain`): its frames count on from the frame after the last game's last (`replay::CountAgainFrom`,
    `recording::CountAgainFrom`), it is carried into the next game's input state (`replay::TakePlayback`,
    `ResumePlayback`), and the run's start does not start it again. The skirmish box between the two games takes the
    recording's events at its own frames, which count too. So a recording's frames are the frames of the whole run,
    the box's included. Until 2026-11-03 a restart played the recording again from its first event. Tests
    `InputRecording.ARecordingPlaysOnWhenTheFrameCountStartsAgain`,
    `InputRecording.APlaybackNotYetStartedStartsSoonerOnTheNewCount`,
    `InputReplayTest.APlaybackTakenOutOfAGamePlaysOnInTheNext`.
  - A recording being made records on through a game started again in the same way (`replay::TakeRecording`,
    `ResumeRecording`; its clock counts on with `CountAgainFrom`), so it keeps the earlier game's events and its
    frames are the frames a playback of it counts. Before (2026-11-08) the next game's start made the file anew, and
    only the last game's events, from frame 0 again, were kept. Tests
    `InputRecording.TheRecordingsFrameCountsOnThroughANewCount`,
    `InputReplayTest.ARecordingTakenOutOfAGameRecordsOnInTheNext`.
- **Resuming a game** (developer tool, 2026-11-08): `--resume-input <old> --record-input <new>` plays `<old>` from
  the run's first frame with the real mouse and keys held back, as `--replay-input` does, then gives them back to the
  player once it has played out, so a game a crash stopped goes on from its recording's last flush
  (`replay::StartResume`). The new recording starts with `<old>`'s events, written at once, and goes on with the
  player's, so it can be resumed again; the same file for both is refused (one log line, nothing recorded).
  - The hand-over leaves nothing the next resume would not replay: one frame after its last events the playback lets
    go of the mouse buttons and keys they left held (`recording::ReleasesOfHeld`: a button's up at the pointer's last
    place, a key's with no modifiers), given as its own events and recorded too; else the input map would make up
    those ups from the real mouse and keyboard, which a recording never sees. Then the playback ends, the log says
    "Input resume: live play from frame N", and the Game puts the real pointer where the game's cursor is
    (`replay::TakeHandOver`, `SDL_WarpMouseInWindow`), so the real mouse's first reads agree with the recording's last
    place; the move it makes comes in as the player's and is recorded.
  - The debug bar's Debug > Input Recording shows a resume playing and the frame it handed over at.
  - The game is the same up to the hand-over only if the run is: the same exe, fixed frame time, `OPENBLACK_CLOCK_AT`,
    window size and arguments, and the profiles as they were when `<old>` started recording (the game writes them
    while it plays). No `-n`, which makes the run a check run without profiles. Tests
    `InputRecording.TheButtonsAndKeysLeftHeldAreLetGo`,
    `InputReplayTest.AResumedRecordingLetsGoOfWhatItHeldThenTheRecordingGoesOnLive`,
    `InputReplayTest.AResumedRecordingWithNothingHeldHandsOverAtItsEnd`.
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

- **Live as replayed** (2026-11-09): the game reads only what the dispatched events carry, so a recording gives a
  live run's input back whole. Found by a live Land 1 recording that parted from its replay at the first middle-button
  drag (turn 5435 of 10550).
  - The cursor: a motion event puts the game's cursor at the event's place, live as in a playback
    (`input::CursorAfterMotion`), and the action map reads that cursor (`GameActionMap::PointerPosition`), as does the
    in-game interface's draw. Before, live play read SDL's pointer state (`SDL_GetMouseState`), which runs ahead of
    the events: SDL drops the motion queued behind a press when it turns relative mode on or off
    (`SDL_SetRelativeMouseMode` flushes `SDL_MOUSEMOTION`) and a warp in relative mode moves its state with no event,
    so the cursor held during the drag, and the hand after it, stood where no recorded event had put them. A warp of
    the cursor (`GameActionMap::WarpCursor`, the camera's edge drag) moves the game's cursor too, where no
    `OPENBLACK_MOUSE_AT` or fixed mouse stands in for it. The middle button's relative mode and warp back touch the real
    pointer only when real input is read (`input::UsesRealInput`). Test
    `MouseAt.TheRealMouseMovesNothingWhileAHandDemoPlays`.
  - Every motion event moves the game's cursor, whatever takes the event (2026-11-11): the cursor is moved as the event
    is dispatched, before the debug windows, the escape menu, the boxes or the game take it. Found in the user's play
    of joint589 (Bugs.txt #4: the pointer stayed where it was once Escape opened the menu): the move had been in the
    game's own event handling, which the open menu (it takes every mouse event) and a debug window under the mouse
    never let the event reach, so their pointer, drawn at the game's cursor, stood still.
  - The wheel: its event keeps the notches as turned (SDL's `preciseX`/`preciseY`, a fraction of one from a fine wheel),
    which the action map's wheel delta and so the camera's zoom read, in the event's fourth and fifth values as float
    bits. Before, a replayed wheel turn gave a delta of 0. A wheel event with both at 0 (recorded before, or crafted
    by `tools/saves/replay.py`) takes its whole notches for them. Test `InputRecording.TheWheelsNotchesAsTurnedComeBack`.
  - The keys and buttons let go elsewhere: an up the menu, a box or a debug window took never reaches the action map,
    which made up the letting go from SDL's keyboard and mouse state, live only; a replay, which feeds no real state,
    kept the key or button held. Now every dispatched event, whoever takes it, tells the action map what the window
    holds (`GameActionMap::SeeDispatchedEvent`), and the letting go is made up from that, live and under a playback
    alike; `IsMouseButtonHeld` (the temple's rooms) reads it too. Test
    `GameActionMapCursor.ALettingGoTakenElsewhereIsMadeUpFromTheWindowsEvents`.
  - The modifiers: a mouse button's binding with a modifier reads Shift, Ctrl and Alt as the window's last key event
    had them, and an edit box's Shift (the new player's name, a save's name) comes with its key's own event; before,
    both read `SDL_GetModState`, the real keyboard's. Only Shift, Ctrl and Alt count, as the original's own modifiers
    (`LHKEYMOD_SHIFT`, `LHKEYMOD_CTRL`, `LHKEYMOD_ALT`, bw1-decomp `src/Lionhead/LHLib/ver5.0/LHKey.h`), so Num Lock
    and Caps Lock, which the system turns on or off with no key event (SDL's `KMOD_NUM`, `KMOD_CAPS` on focus), never
    change what a key or button does. Tests
    `GameActionMapCursor.ADragTheWheelAndTheModifiersReadTheSameFromTheirRecording`,
    `GameActionMap.TheLockKeysChangeNoKeysActions`.

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
- A player of the original's `.hnd` interface records at the input level (turning each record into input events)
  is not made: the hand demos already play through `hand_demo`.
