# Cinematics

What the scripts do to the picture around their cut scenes: the cinema bars, fading to a colour and back, putting the
player's interface away, and close clipping. The cut scenes and films themselves are in `../story/`.

**Progress: 8/14 done, 0 chosen, 6 partial, 0 inferred — 90%**

## Cinema bars

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cinema bars slide in to a 16:9 picture and back out | done | `CinematicDirectorSystem::SetWideScreen` over `gui::CinemaBars` (`src/Gui/CinemaBars.cpp`), drawn by `Renderer::DrawScreenOverlay`; the slide time is the help system's `wideScreenTime`; `CinemaBars.SlideInAndTurnBackFromWhereTheyAre`, `BarsMakeThePictureSixteenByNine` (`test/gui/test_cinematics.cpp`) ([screen fade and cinema bars](../../../interface/screen-and-readouts.md#screen-fade-and-cinema-bars)) |
| Only the script that brought the bars in can take them out, or any script while none has | done | `SET_WIDESCREEN` in `src/CHLApi.cpp` through `help::script_control::SetWideScreen` (`src/Help/ScriptControl.cpp`); `ScriptControl.SetWideScreen` (`test/test_help_system.cpp`) |
| A script's bars put the hand and the player's interface away and hide the game's dialogs | done | the help system's wide-screen hook in `src/Game.cpp` makes the interface inactive (`interface_active::SetActive`), which hides the hand (`HandSystem`) and its tooltips; the did-you-know bubble is not drawn under a script's bars. As the bars come in, a script's or the game's own, every dialog is put away first (`gui::HideAllDialogs`, as the original's `DialogBoxBase::HideAll`, see [the help system](../../../interface/advisors-and-help.md)): the menu from the temple's options room closes and the room goes back to the hall; the tattoo editor's box and the story history's buttons go and their owners show them again; tests `HideAllDialogs.TheMenuClosesAndTheTempleBoxesGoWithNeitherOkNorCancel`, `HelpSystem.TheBarsComingInTellTheHookOnceWhoeverHoldsThem`; the debug Land Island window turns the bars on and off (Kept by a land change) |
| The game's own bars leave the interface | partial 90% | the same hook keeps the interface active when the bars have no owner, as the original. Missing: no test |
| Setting the bars the same again changes nothing | done | `HelpSystem::SetWideScreen` returns at once when nothing changes (`src/Help/HelpSystem.cpp`), as the original; `CinemaBars.SettingTheSameAgainChangesNothing` |
| "widescreen ready": whether the bars have finished sliding | done | `WIDESCREEN_TRANSISTION_FINISHED` reads `CinematicDirectorSystem::IsWideScreenTransitionFinished`; `CinematicDirector.TheBarsMoveOnlyWithTheFramesGameTime` (`test/gui/test_cinematic_director.cpp`) |
| The bars slide with game time and stop while the game is paused | done | `CinematicDirectorSystem::Update` takes the frame's game milliseconds, 0 while paused (`src/Game.cpp`); `CinematicDirector.TheBarsMoveOnlyWithTheFramesGameTime` |
| The player's input is blocked while the bars are in | partial 90% | with a script's bars the interface is inactive and the key shortcuts and Escape are skipped (`src/Game.cpp`), as the original's action gate ([one-press keys](../../../interface/screen-and-readouts.md#the-one-press-keys-temple-realm-villager-names)). Missing: only Escape's gate is tested (`MenuEscape.FilmWideScreenAndDebounce`) |
| Escape skips a cut scene or film | partial 60% | Escape skips a film (`VideoPlayer::EscapeKey`, `VideoPlayerTest.EscapeKey`) and does nothing under a script's bars (`MenuEscape.FilmWideScreenAndDebounce`). Missing: the original's earlier takers of the key (a box on screen, the tutorial, the temple) are not ported, so openblack skips in every case ([skipping with ESC](../../../video/playback.md#skipping-with-esc)) |

## Fades

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fade the picture to a colour over whole seconds, or at once | done | `SET_FADE` in `src/CHLApi.cpp` to `CinematicDirectorSystem::FadeTo` (`gui::ScriptFade`, `src/Gui/ScriptFade.cpp`), moved once a turn (`ProcessTurn`); `ScriptFade.FadesToAColourOverTurns`, `TheTimeIsTruncatedToWholeSeconds`, `NoTimeIsAtOnce`, `CinematicDirector.TheFadeMovesOnlyWithTheTurns` |
| Fade back to normal over whole seconds | done | `SET_FADE_IN` to `CinematicDirectorSystem::FadeBackToNormal`; `ScriptFade.FadesBackToClear` |
| "fade finished": whether the fade is done | done | `FADE_FINISHED` reads `CinematicDirectorSystem::IsFadeFinished`; `CinematicDirector.TheFadeMovesOnlyWithTheTurns` |
| A new game opens with no fade, no bars and no close clipping; a land change keeps what the scripts set | done | (reworded 2026-10-22: the original resets them only when the scripts restart, at a new game or a playground's start, never at `LOAD_MAP`, [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)) the bars go at once with `HelpSystem::Reset` in `Game::ResetScripts` (its `SetWideScreen(0, 0)`, then `Hooks::snapWideScreen`, `CinemaBars::Snap`, as the original's reset snaps them; `CinemaBars.TakenAwayAndSnappedTheyGoAtOnce`, `HelpSystem.ResetTakesTheWideScreenAwayThenSnapsTheBars`), only when the scripts restart (`land_load::ResetsFor`; `LandLoadScripts.OnlyANewGameOrAPlaygroundsStartRestartsTheScripts`); the Land Island window shows them. The close clipping is cleared in `Game::ResetScripts` too, and kept by a land change. The fade stops and clears there too (`CinematicDirectorSystem::ResetFade`, `ScriptFade.TheScriptsRestartStopsTheFadeAndClearsThePicture`), kept by a land change; the Land Island window shows it. The restart's own 0 in the bars' fraction lasts only until the help system's next frame writes it from its timer; openblack's fraction is read only after that frame's update, so it needs no write ([screen fade and cinema bars](../../../interface/screen-and-readouts.md#screen-fade-and-cinema-bars)) |

## Close shots

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Close clipping is cleared when the scripts restart | partial 90% | the scripts' restart (`Game::ResetScripts`, at a new game or a playground's start) calls `CinematicDirectorSystem::SetCloseClipping(false)`, and a land change keeps it, as the original's ([camera](../../../camera/game-camera.md#camera), [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)); the Land Island window shows it. Missing: no test of the clear itself |
