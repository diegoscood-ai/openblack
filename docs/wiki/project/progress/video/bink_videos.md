# Bink videos

The game ships five Bink videos: two start-up pictures, a pre-intro, the story's intro film, a strip of tip pictures for
the loading screen and the film behind the creature's fall at the end of the game. This file is what each is for and
when the game shows it; how they are decoded and drawn is in [bink_playback.md](bink_playback.md). openblack shows all five
where the original does: the logos and the pre-intro as it starts, the tips while a land loads, the intro and the
falling-spell film from the scripts; the debug Video window plays any of them.

**Progress: 9/12 done, 0 chosen, 2 partial, 0 inferred — 85%**

How the original does it, in our wiki: [Bink videos (.bik)](../../../video/bink.md).

## The files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Data/logo.bik`: the publisher's and studio's pictures, two still frames | done | decoded by our Bink decoder and playable in the debug Video window (test `VideoBinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: shown as the first start-up screen (GameStartup.cpp, startup::LogoStill) |
| `Data/pre_intro.bik`: the pre-intro film, about 100 seconds | done | decoded by our Bink decoder and playable in the debug Video window (test `VideoBinkDecoder.PreIntroMatchesTheOriginal`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: played on a first start (startup::PlaysPreIntro) and with --pre-intro |
| `Data/INTRO.bik`: the story's intro film, about 67 seconds | done | played by SET_AVI_SEQUENCE sequence 1 (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`); test `VideoBinkDecoder.IntroMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |
| `Data/tips.bik`: 35 still pictures for the loading screen's tips | done | decoded by our Bink decoder (test `VideoBinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); see [bink_playback.md](bink_playback.md); re-measured 2026-10-21: the loading screen shows its pictures (src/Gui/LoadingScreen.cpp) |
| `Data/Spells/fall/fall.bik`: the film behind the creature's fall, 50 seconds | done | `src/Video/FallingSpellVideo.cpp`; test `VideoBinkDecoder.FallMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |

The videos having no sound of their own, the start-up logos and the pre-intro (its first run, its end, its music and
its hidden cursor), the tip picture shown while a land loads and cleared as a film starts, the intro's fade, pause,
bars and no-skip rule, the falling creature with its camera, sounds, white fade and creature check, and Escape and the
paused world during a film are each one row in [bink_playback.md](bink_playback.md), where this page's own rows for
them are merged.

## The loading screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tip is picked at random by the time; with player profiles already made, two first-run tips are skipped | done | as the v1.42 exe (`StartTipOfTheDayText`): tip (milliseconds >> 4) % 34 + 1, tips 29 and 32 never shown (the next one is), and the first tip, which explains the tips, when no profile is kept (`loading::PickTip`, test `LoadingScreen.TipsArePickedByTheSixteenthsOfAMillisecondCount`); a picture still there is kept |
| The tip's text comes from the game's help texts, with its control codes blanked out | done | the tip's HELP_TEXT_TOTD entry, its `$` and `\` codes blanked up to a space, the hidden-word mark or the end (`loading::TipTextName`, `loading::BlankControlCodes`; tests `LoadingScreen.TipTextsAreTheTipsOfTheDay`, `LoadingScreen.ControlCodesAreBlanked`) |
| The picture fades in, with the game's version shown in a corner and a progress bar | done | `gui::LoadingScreen::DrawTips` with raffclar's `loading::` rules (tests `LoadingScreen.FadeBringsInTheBlurThenTheSharpPicture`, `LoadingScreen.Layout`, `LoadingScreen.BarSweeps`, `LoadingScreen.Version`, `LoadingScreen.VersionFromTheSettings`): the first time it fades in over a second (`startup::NextTipFade`); the version from the player's `GameVersion` / `IsDeveloperPatch` settings, V1.00 without them; the bar's body as the bevel box draws ([start-up](../../../video/playback.md#startup-and-loading-screen)) |

## The story's intro film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the start of a new game, in the first land's opening scene: after the boy is saved from the sharks the screen fades to black and the film plays | done | the opening scene's script plays `INTRO.bik` through SET_AVI_SEQUENCE (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`), checked in Land 1; test `VideoPlayerMaths.TheOpeningScenesScriptPlaysTheIntroFilm` |
| When it ends the opening carries on: a fireball streaks across the sky, the camera follows it and the family is left on the beach | partial 50% | the script carries on after the film; the falling light and the hand that lifts the boy are wired but still drafted; see ../story/land_1.md and ../story/cutscenes.md |

## The creature's fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the end of the game, when the creature has climbed the volcano and leapt in | partial 70% | SET_AVI_SEQUENCE sequence 2 plays it (`src/CHLApi.cpp`, `FallingSpellVideo::KickOff`; tests `FallingSpellVideoTest.*`), but the ending's script is never reached; see ../story/ending.md; the scene round it: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano); re-measured 2026-10-21: the film plays from SET_AVI_SEQUENCE 2 as the original; the ending's script that calls it is not reached yet |
| Afterwards the creature is put back on the land and the game speed and music are restored | pending | the ending's script is never reached; see ../story/ending.md |
