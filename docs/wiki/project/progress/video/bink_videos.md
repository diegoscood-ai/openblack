# Bink videos

The game ships five Bink videos: two start-up pictures, a pre-intro, the story's intro film, a strip of tip pictures for
the loading screen and the film behind the creature's fall at the end of the game. This file is what each is for and
when the game shows it; how they are decoded and drawn is in [bink_playback.md](bink_playback.md). openblack shows all five
where the original does: the logos and the pre-intro as it starts, the tips while a land loads, the intro and the
falling-spell film from the scripts; the debug Video window plays any of them.

**Progress: 25/28 done, 0 chosen, 2 partial, 0 inferred — 94%**

How the original does it, in our wiki: [Bink videos (.bik)](../../../video/bink.md).

## The files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Data/logo.bik`: the publisher's and studio's pictures, two still frames | done | decoded by our Bink decoder and playable in the debug Video window (test `VideoBinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: shown as the first start-up screen (GameStartup.cpp, startup::LogoStill) |
| `Data/pre_intro.bik`: the pre-intro film, about 100 seconds | done | decoded by our Bink decoder and playable in the debug Video window (test `VideoBinkDecoder.PreIntroMatchesTheOriginal`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: played on a first start (startup::PlaysPreIntro) and with --pre-intro |
| `Data/INTRO.bik`: the story's intro film, about 67 seconds | done | played by SET_AVI_SEQUENCE sequence 1 (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`); test `VideoBinkDecoder.IntroMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |
| `Data/tips.bik`: 35 still pictures for the loading screen's tips | done | decoded by our Bink decoder (test `VideoBinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: the loading screen shows its pictures (src/Gui/LoadingScreen.cpp) |
| `Data/Spells/fall/fall.bik`: the film behind the creature's fall, 50 seconds | done | `src/Video/FallingSpellVideo.cpp`; test `VideoBinkDecoder.FallMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |
| The videos have no sound of their own: every sound with them comes from the game's music and sound banks | done | the films' sounds and music come from the game's banks (`src/Video/FallingSpellVideo.cpp`, `src/Audio`); see [bink_playback.md](bink_playback.md) |

## Start-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The logo pictures show at every start of the game, before the front end | done | `Game::PlayLogoScreens`, at every start before the audio; tests `Startup.*`; see [bink_playback.md](bink_playback.md) |
| The pre-intro plays on a first run, before any player profile exists, with the trailer music | done | as the v1.42 exe (`pc_main`: with no profile, or the command line's `PREINTROVIDEO`, after the audio): `Game::PlayPreIntro` when no profile is kept or with `--pre-intro` (`startup::PlaysPreIntro`, test `Startup.PreIntro`), with the trailer music; profiles: [../interface/profiles.md](../interface/profiles.md); see [bink_playback.md](bink_playback.md) |
| The pre-intro ends at its last frame or when Escape is held, and the trailer music stops with it | done | after its last frame or when Escape is held as a frame ends (tests `Startup.PreIntro`, `Startup.OnlyEscapeHeldStopsThePreIntro`), the music cut with it; see [bink_playback.md](bink_playback.md) |
| The cursor is hidden while the pre-intro plays | done | no pointer is seen: the window's system pointer is always hidden (`Sdl2WindowingSystem`) and the pre-intro's frames draw only the film; see [bink_playback.md](bink_playback.md) |

## The loading screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a land loads, one of the tip pictures shows with a tip of the day under it | done | the tips screen while each land loads (`Game::BeginLoadingScreen`, `gui::LoadingScreen::DrawTips`); tests `LoadingScreen.*`; see [bink_playback.md](bink_playback.md) |
| The tip is picked at random by the time; with player profiles already made, two first-run tips are skipped | done | as the v1.42 exe (`StartTipOfTheDayText`): tip (milliseconds >> 4) % 34 + 1, tips 29 and 32 never shown (the next one is), and the first tip, which explains the tips, when no profile is kept (`loading::PickTip`, test `LoadingScreen.TipsArePickedByTheSixteenthsOfAMillisecondCount`); a picture still there is kept |
| The tip's text comes from the game's help texts, with its control codes blanked out | done | the tip's HELP_TEXT_TOTD entry, its `$` and `\` codes blanked up to a space, the hidden-word mark or the end (`loading::TipTextName`, `loading::BlankControlCodes`; tests `LoadingScreen.TipTextsAreTheTipsOfTheDay`, `LoadingScreen.ControlCodesAreBlanked`) |
| The picture fades in, with the game's version shown in a corner and a progress bar | done | `gui::LoadingScreen::DrawTips` with raffclar's `loading::` rules (tests `LoadingScreen.FadeBringsInTheBlurThenTheSharpPicture`, `LoadingScreen.Layout`, `LoadingScreen.BarSweeps`, `LoadingScreen.Version`, `LoadingScreen.VersionFromTheSettings`): the first time it fades in over a second (`startup::NextTipFade`); the version from the player's `GameVersion` / `IsDeveloperPatch` settings, V1.00 without them; the bar's body as the bevel box draws ([start-up](../../../video/playback.md#startup-and-loading-screen)) |
| The tip picture is cleared when a full-screen film starts | done | no tip picture is ever left when a film starts: the original clears it as each game starts after its set-up, and openblack as the loading screen ends (`Game::EndLoadingScreen`); see [bink_playback.md](bink_playback.md) |

## The story's intro film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the start of a new game, in the first land's opening scene: after the boy is saved from the sharks the screen fades to black and the film plays | done | the opening scene's script plays `INTRO.bik` through SET_AVI_SEQUENCE (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`), checked in Land 1 |
| When it ends the opening carries on: a fireball streaks across the sky, the camera follows it and the family is left on the beach | partial 50% | the script carries on after the film; the falling light and the hand that lifts the boy are wired but still drafted; see ../story/land_1.md and ../story/cutscenes.md; re-measured 2026-10-21: the script carries on after the film; the falling light and the hand that lifts the boy are still drafts |
| It fades out two seconds before its end and the screen fades back from the script's fade | done | `VideoPlayer::ScheduleIntro` (fades from 58 s, ends at 60 s) and the script fade removed by SET_AVI_SEQUENCE; test `VideoPlayerTest.IntroSchedule`; see [bink_playback.md](bink_playback.md) |
| Skipping the intro film on a new player's first game is not allowed | done | each game started, when no player has a profile yet, as the v1.42 exe (`Game::LoadStartLand`, `startup::FilmsUnskippable`; tests `Startup.ANewPlayersFirstGameHasFilmsThatCantBeSkipped`, `VideoPlayerTest.NoSkipClearedByTheEnd`); see [bink_playback.md](bink_playback.md) |
| The game is paused and the cinema bars are on while it plays | done | tests `VideoPlayerTest.PlayKeepsThePauseAndTheWideScreen`, `VideoPlayerMaths.BarsFullAtOnceDuringTheFilm`; see [bink_playback.md](bink_playback.md) |

## The creature's fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the end of the game, when the creature has climbed the volcano and leapt in | partial 70% | SET_AVI_SEQUENCE sequence 2 plays it (`src/CHLApi.cpp`, `FallingSpellVideo::KickOff`; tests `FallingSpellVideoTest.*`), but the ending's script is never reached; see ../story/ending.md; the scene round it: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano); re-measured 2026-10-21: the film plays from SET_AVI_SEQUENCE 2 as the original; the ending's script that calls it is not reached yet |
| The player's creature is drawn falling in front of the film, with glows on its hands, sparks and a burst of light | done | the falling creature in its tint with its hair, its eyes and its two hand glows, the sparks and the light burst are drawn (`src/Magic/Objects/FallingSpell.cpp`, `src/Graphics/RendererFallingSpell.cpp`); silent as the original's copy, whose animations carry no sound moments ([the falling spell](../../../magic/falling-spell.md#the-copy-of-the-creature)); see [bink_playback.md](bink_playback.md) |
| The camera follows the fall along its recorded path | done | the path file is read and followed (`falling_spell::CameraPath`, `FallingSpell::UpdateCamera`; tests `FallingSpell.CameraFollowsTheFilm`, `CameraPathDriftsLikeTheOriginal`); re-measured 2026-10-21: the camera is driven from the film's time every update (FallingSpellVideo: UpdateCamera) |
| Sounds and a white fade are timed to the film | done | `src/Video/FallingSpellVideo.cpp`; tests `FallingSpellVideoTest.StatesAndSounds`, `TheWhiteFadeEndsIt`; see [bink_playback.md](bink_playback.md) |
| It only plays when the player has a creature | done | `FallingSpellVideo::KickOff`; test `FallingSpellVideoTest.NoCreatureNoFilm`; see [bink_playback.md](bink_playback.md) |
| Afterwards the creature is put back on the land and the game speed and music are restored | pending | the ending's script is never reached; see ../story/ending.md |

## Options and keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape skips a film (fading it out) | done | `VideoPlayer::EscapeKey`; tests `VideoPlayerTest.SkipFadesOver48Frames`, `FallingSpellVideoTest.EscapeSkips`; not inside the temple, under an open menu, once the game is over (unless a script holds the wide screen) or in the playground land, as the original (`video::EscapeReachesTheFilm`, test `VideoPlayerMaths.EscapeReachesTheFilm`); see [bink_playback.md](bink_playback.md) |
| Films run while the world is paused; the land's ambience is hushed | done | the game is paused while a film plays and the ambience mix is skipped while `video::IsPlaying` (`src/Video/VideoPlayer.cpp`, `src/Audio/Game/AudioSystem.cpp`) |
