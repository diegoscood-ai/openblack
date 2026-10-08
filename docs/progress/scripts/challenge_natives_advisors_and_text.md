# Challenge natives: advisors, text and help

The challenge scripts' functions for the good and evil advisors, the dialogue box and its spoken text, text drawn on screen, the help system, challenge markers (scrolls) and the tutorial's hand demonstrations and interface levels. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 35/44 done, 1 partial — 81%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Brings the good or evil advisor out onto the screen: *eject good spirit/evil spirit* (called 460 times in 167 scripts) | done | `SpiritEject` in `src/CHLApi.cpp`: the advisor controller (`src/Help/SpiritsRuntime.h`) ejects the spirit, a help script's one appears; drawn and moving in game; `test/test_spirits.cpp` |
| Sends the good or evil advisor back off the screen: *send good spirit/evil spirit home* (called 266 times in 101 scripts) | done | `SpiritHome`: `HelpSystem::SpiritHome`, a help script's spirit vanishes and any other flies home |
| Makes an advisor point at a place, on screen or in the world: *make good spirit/evil spirit point at ‹position› [in world]* (called 131 times in 61 scripts) | done | `SpiritPointPos`: the advisor controller's `SpiritPointPosition`, on screen or in the world |
| Makes an advisor point at an object, on screen or in the world: *make good spirit/evil spirit point to ‹target› [in world]* (called 19 times in 12 scripts) | done | `SpiritPointGameThing`: the advisor controller's `SpiritPointObject`, re-sent every turn |
| Shows a line of text (and plays its speech) in the dialogue box, optionally waiting for the player to click on: *say [single line] ‹text id› with interaction/without interaction* (called 1767 times in 264 scripts) | done | `RunText`: `HelpSystem::RunText` (`src/Help/HelpSystem.cpp`), the dialogue text drawn and its speech played; `test/test_help_text_display.cpp` |
| Shows a line of text for a moment without the dialogue box: *say [single line] ‹string› with interaction/without interaction* (called 2 times in 2 scripts) | done | `TempText`: `HelpSystem::TempText` |
| Whether the text on screen has been read (its speech finished or clicked past): *read* (called 1727 times in 267 scripts) | done | `TextRead`: `HelpSystem::IsTextRead` |
| Sets how much of the interface the player may use (the tutorial opens it up step by step): *set interaction ‹level›* (called 25 times in 8 scripts) | partial | `SetInterfaceInteraction`: `help::interface_interaction::Set` (`src/Help/InterfaceInteraction.cpp`) sets the interface flags, camera features, auto-pitch and hand reach; the control map does not ask it yet which actions are blocked |
| Makes an advisor look at an object: *make good spirit/evil spirit look at ‹target›* (called 4 times in 4 scripts) | done | `LookGameThing`: the advisor controller's `SpiritLookObject` |
| Takes the dialogue box for the script (start of the language's dialogue block); waits while another script has it: *begin dialogue (opening the block)* (called 626 times in 265 scripts) | done | `StartDialogue`: `help::script_control::StartDialogue` (`src/Help/ScriptControl.cpp`); the advisors going home on it are not ported |
| Lets go of the dialogue box (end of the dialogue block): *end dialogue (closing the block)* (called 748 times in 296 scripts) | done | `EndDialogue`: `help::script_control::EndDialogue`, only for the task that holds the dialogue |
| Whether the dialogue box is free for this script: *dialogue ready* (called 15 times in 13 scripts) | done | `IsDialogueReady`: `help::script_control::IsSpiritReady` |
| Stops an advisor pointing: *stop good spirit/evil spirit pointing* (called 68 times in 38 scripts) | done | `StopPointing`: the advisor controller's `SpiritStopPointing` |
| Stops an advisor looking: *stop good spirit/evil spirit looking* (called once in 1 script) | done | `StopLooking`: the advisor controller's `SpiritStopLooking` |
| Makes an advisor look at a position: *make good spirit/evil spirit look at ‹position›* (called 10 times in 5 scripts) | done | `LookAtPosition`: the advisor controller's `SpiritLookAtPosition` |
| Makes an advisor cling to the edge of the screen at a place: *make good spirit/evil spirit cling across ‹x percent› down ‹y percent›* (called 21 times in 12 scripts) | done | `ClingSpirit`: the advisor controller's `SpiritCling`, with the screen range checks |
| Makes an advisor fly to a place on the screen: *make good spirit/evil spirit fly across ‹x percent› down ‹y percent›* (called 2 times in 1 script) | done | `FlySpirit`: the advisor controller's `SpiritFly` |
| Shows text with a number in it in the dialogue box: *say [single line] ‹string› with number ‹number› with interaction/without interaction* (called once in 1 script) | done | `RunTextWithNumber`: `HelpSystem::RunTextWithNumber` |
| Whether an advisor is speaking: *good spirit/evil spirit speaks ‹text id›* (called 13 times in 6 scripts) | done | `SpiritSpeaks`: whether `help::SpiritWhoTalks` of the text's narrator is this spirit |
| Gives a help text by number: *get ‹object› help* (called 2 times in 2 scripts) | todo | `GetHelp` logs "not implemented" and pushes nought |
| Plays a recorded demonstration of the hand (moving, grabbing, casting) for the tutorial: *start hand demonstration ‹string› [with pause on trigger] [without hand modify]* (called 18 times in 17 scripts) | done | `PlayHandDemo`: `hand_demo::Play` (`src/Input/HandDemo.cpp`) for the running task; the scripted hand demos are part of the verification runs |
| Whether a hand demonstration has finished: *hand demonstration played* (called 17 times in 16 scripts) | done | `IsPlayingHandDemo`: the negation of `hand_demo::IsPlaying`, as the scripts' wait loops expect |
| Creates a challenge marker (the bronze, silver or gold scroll and its signpost) at a position: *create highlight ‹type› at ‹position› ‹challenge id›* (called 82 times in 78 scripts) | done | `CreateHighlight`: `ecs::script_highlight::Create` (`src/ECS/ScriptHighlight.cpp`), drawn and tapped in game; `test/test_script_highlight.cpp`; the creating script's debug name is not kept |
| Makes an advisor appear: *make good spirit/evil spirit appear* (called 20 times in 9 scripts) | done | `SpiritAppear`: ejected as a help script's spirit |
| Makes an advisor disappear: *make good spirit/evil spirit disappear* (called 20 times in 9 scripts) | done | `SpiritDisappear`: sent home as a help script's spirit |
| Whether challenge markers are drawn: *enable/disable highlight draw* (called 3 times in 3 scripts) | done | `SetDrawHighlight`: the camera control's draw-highlight setting, read by the highlights' draw |
| Sets the text and category of a challenge marker: *set ‹object› text properties text ‹text› category ‹category›* (called 2 times in 2 scripts) | done | `HighlightProperties`: `ecs::script_highlight::SetScriptId` (text and category) |
| Whether a hand demonstration has reached its trigger: *hand demonstration trigger* (called 33 times in 12 scripts) | done | `HandDemoTrigger`: `hand_demo::ConsumeTrigger`, cleared as it is read |
| Gives the first help text of a group: *get ‹object› first help* (called once in 1 script) | todo | `GetFirstHelp` logs "not implemented" and pushes nought |
| Gives the last help text of a group: *get ‹object› last help* (called once in 1 script) | todo | `GetLastHelp` logs "not implemented" and pushes nought |
| Clears the dialogue box: *clear dialogue* (called 18 times in 10 scripts) | done | `GameClearDialogue`: `HelpSystem::ClearDialogue` (the voices go on) |
| Closes the dialogue box: *close dialogue* (called 165 times in 70 scripts) | done | `GameCloseDialogue`: `HelpSystem::CloseDialogue` |
| Draws text on the screen at a place, a size and fading in: *draw text ‹text id› across ‹across› down ‹down› width ‹width› height ‹height› size ‹size› fade in time ‹fade› seconds* (called 32 times in 2 scripts) | todo | `GameDrawText` logs "not implemented" |
| Draws text on the screen for a moment: *draw text ‹string› across ‹across› down ‹down› width ‹width› height ‹height› size ‹size› fade in time ‹fade› seconds* (called 266 times in 1 script) | todo | `GameDrawTempText` logs "not implemented" |
| Fades all drawn text out: *fade all draw text time ‹time› seconds* (called 26 times in 3 scripts) | todo | `FadeAllDrawText` logs "not implemented" |
| Sets the colour of drawn text: *set draw text color red ‹red› green ‹green› blue ‹blue›* (called 61 times in 2 scripts) | todo | `SetDrawTextColour` logs "not implemented" |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Makes an advisor play an animation across the screen: *make good spirit/evil spirit play across ‹value› down ‹value› ‹value› [speed ‹value›]* (not called by the shipped scripts) | done | `PlaySpiritAnim`: the advisor controller's `SpiritPlayAnim`, with the enum and screen range messages |
| Whether an advisor has finished an animation: *good spirit/evil spirit played* (not called by the shipped scripts) | done | `SpiritPlayed`: whether the advisor is not playing an animation |
| Whether the help system is on: *help system on* (not called by the shipped scripts) | done | `HelpSystemOn`: `HelpSystem::IsHelpSystemOn` |
| Shows the shape of a gesture at a position: *animate gesture ‹value› at ‹value› radius ‹value›* (not called by the shipped scripts) | todo | `PlayGesture` logs "not implemented" |
| Shows text with a number in it for a moment: *say [single line] ‹format› with number ‹value› with interaction/without interaction* (not called by the shipped scripts) | done | `TempTextWithNumber`: `HelpSystem::TempTextWithNumber` |
| Turns the help system on or off: *enable/disable help system* (not called by the shipped scripts) | done | `SetHelpSystem`: `HelpSystem::SetHelpOn` |
| Makes an advisor point at a place on the screen: *make good spirit/evil spirit point across ‹value› down ‹value›* (not called by the shipped scripts) | done | `SpiritScreenPoint`: the screen fraction turned into a pixel and the advisor controller's `SpiritScreenPoint` |
| Lets the hand demonstrations be stepped with keys: *enable/disable hand demonstration keys* (not called by the shipped scripts) | done | `SetHandDemoKeys` does nothing, as the original's empty handler |
