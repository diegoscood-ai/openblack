# Cinematics

What the scripts do to the picture around their cut scenes: the cinema bars, fading to a colour and back, putting the
player's interface away, and close clipping. The cut scenes and films themselves are in `../story/`.

**Progress: 11/15 done, 2 partial — 80%**

## Cinema bars

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cinema bars slide in to a 16:9 picture and back out | done | `ScreenFade::SetWideScreen` and `UpdateWideScreen` (`src/3D/ScreenFade.cpp`), bars of `ScreenFade::LetterboxHeight` drawn from `src/Game.cpp`; the slide time is the help system's `wideScreenTime` |
| Only the script that brought the bars in can take them out, or any script while none has | done | `SET_WIDESCREEN` in `src/CHLApi.cpp` through `help::script_control::SetWideScreen` (`src/Help/ScriptControl.cpp`); `ScriptControl.SetWideScreen` (`test/test_help_system.cpp`) |
| A script's bars put the hand and the player's interface away and hide the game's dialogs | done | the help system's wide-screen hook in `src/Game.cpp` makes the interface inactive (`interface_active::SetActive`), which hides the hand (`HandSystem`) and its tooltips; the did-you-know bubble is not drawn under a script's bars |
| The game's own bars leave the interface | done | the same hook keeps the interface active when the bars have no owner (the game's own) |
| Setting the bars the same again changes nothing | done | `HelpSystem::SetWideScreen` returns at once when nothing changes (`src/Help/HelpSystem.cpp`) |
| "widescreen ready": whether the bars have finished sliding | done | `WIDESCREEN_TRANSISTION_FINISHED` reads `ScreenFade::IsWideScreenTransitionFinished` |
| The bars slide with game time and stop while the game is paused | done | `ScreenFade::UpdateWideScreen` takes the frame's game milliseconds, 0 while paused (`src/Game.cpp`) |
| The player's input is blocked while the bars are in | done | with a script's bars the interface is inactive and the key shortcuts and Escape are skipped (`src/Game.cpp`) |
| Escape skips a cut scene or film | partial | Escape goes to the film player (`video::Get().EscapeKey`, `src/Game.cpp`); under a script's bars it does nothing; skipping a cut scene itself is not checked against the game |

## Fades

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fade the picture to a colour over whole seconds, or at once | done | `SET_FADE` in `src/CHLApi.cpp` to `ScreenFade::FadeTo`, moved once a turn (`ScreenFade::ProcessTurn`) |
| Fade back to normal over whole seconds | done | `SET_FADE_IN` to `ScreenFade::FadeBackToNormal` |
| "fade finished": whether the fade is done | done | `FADE_FINISHED` reads `ScreenFade::IsFinished` |
| A new land opens with no fade, no bars and no close clipping | partial | the script reboot clears the bars (`HelpSystem::Reset`) and the script camera (`script_camera::Reset`, `src/Game.cpp`); the screen fade is not reset, and there is no close clipping |

## Close shots

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts bring the near plane right in for close shots | todo | `SET_GRAPHICS_CLIPPING` is a stub in `src/CHLApi.cpp`; `near_clipping::k_Close` (`NearClipping.ScriptsCanClipClose`) is unused |
| Close clipping is cleared when the scripts restart | todo | there is no close clipping to clear (the script reboot itself is in `src/Game.cpp`) |
