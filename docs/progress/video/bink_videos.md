# Bink videos

The game ships five Bink videos: two start-up pictures, a pre-intro, the story's intro film, a strip of tip pictures for
the loading screen and the film behind the creature's fall at the end of the game. This file is what each is for and
when the game shows it; how they are decoded and drawn is in [bink_playback.md](bink_playback.md). openblack plays the
intro and the falling-spell film; the other three are only decoded, for the debug Video window.

**Progress: 10/29 done, 8 partial — 48%**

How the original does it, in our wiki: [Bink videos (.bik)](../../bw1-notes/video.md).

## The files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| `Data/logo.bik`: the publisher's and studio's pictures, two still frames | partial | decoded by our Bink decoder and playable in the debug Video window (test `BinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); the game never shows it (no start-up sequence); see [bink_playback.md](bink_playback.md) |
| `Data/pre_intro.bik`: the pre-intro film, about 100 seconds | partial | decoded by our Bink decoder and playable in the debug Video window (test `BinkDecoder.PreIntroMatchesTheOriginal`); the game never plays it (no front end or profiles); see [bink_playback.md](bink_playback.md) |
| `Data/INTRO.bik`: the story's intro film, about 67 seconds | done | played by SET_AVI_SEQUENCE sequence 1 (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`); test `BinkDecoder.IntroMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |
| `Data/tips.bik`: 35 still pictures for the loading screen's tips | partial | decoded by our Bink decoder (test `BinkDecoder.LogoAndTipsMatchTheOriginalInAnyOrder`); no loading screen shows it; see [bink_playback.md](bink_playback.md) |
| `Data/Spells/fall/fall.bik`: the film behind the creature's fall, 50 seconds | done | `src/Video/FallingSpellVideo.cpp`; test `BinkDecoder.FallMatchesTheOriginal`; see [bink_playback.md](bink_playback.md) |
| No other videos ship with the game; Creature Isle is not supported | n/a | searched `C:\projects\black`; no expansion videos |
| The videos have no sound of their own: every sound with them comes from the game's music and sound banks | done | the films' sounds and music come from the game's banks (`src/Video/FallingSpellVideo.cpp`, `src/Audio`); see [bink_playback.md](bink_playback.md) |

## Start-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The logo pictures show at every start of the game, before the front end | todo | openblack goes straight to the land; see [bink_playback.md](bink_playback.md) |
| The pre-intro plays on a first run, before any player profile exists, with the trailer music | todo | openblack keeps no profiles (../interface/profiles.md) |
| The pre-intro ends at its last frame or on a key press, and the trailer music stops with it | todo | the pre-intro is not played; see [bink_playback.md](bink_playback.md) |
| The cursor is hidden while the pre-intro plays | todo | the pre-intro is not played; see [bink_playback.md](bink_playback.md) |

## The loading screen

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a land loads, one of the tip pictures shows with a tip of the day under it | todo | openblack has no loading screen |
| The tip is picked at random by the time; with player profiles already made, two first-run tips are skipped | todo | (unconfirmed which two tips those are) |
| The tip's text comes from the game's help texts, with its control codes blanked out | todo | the texts load (`src/Gui/TextDatabase.h`); no loading screen |
| The picture fades in, with the game's version shown in a corner and a progress bar | todo | no loading screen |
| The tip picture is cleared when a full-screen film starts | todo | no loading screen or tip picture; see [bink_playback.md](bink_playback.md) |

## The story's intro film

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the start of a new game, in the first land's opening scene: after the boy is saved from the sharks the screen fades to black and the film plays | done | the opening scene's script plays `INTRO.bik` through SET_AVI_SEQUENCE (`src/CHLApi.cpp`, `src/Video/VideoPlayer.cpp`), checked in Land 1 |
| When it ends the opening carries on: a fireball streaks across the sky, the camera follows it and the family is left on the beach | partial | the script carries on after the film; the falling light and the hand that lifts the boy are wired but still drafted; see ../story/land_1.md and ../story/cutscenes.md |
| It fades out two seconds before its end and the screen fades back from the script's fade | done | `VideoPlayer::ScheduleIntro` (fades from 58 s, ends at 60 s) and the script fade removed by SET_AVI_SEQUENCE; test `VideoPlayerTest.IntroSchedule`; see [bink_playback.md](bink_playback.md) |
| Skipping the intro film on a new player's first game is not allowed | partial | the no-skip flag exists (test `VideoPlayerTest.NoSkipClearedByTheEnd`), but nothing sets it without profiles; see [bink_playback.md](bink_playback.md) |
| The game is paused and the cinema bars are on while it plays | done | tests `VideoPlayerTest.PlayKeepsThePauseAndTheWideScreen`, `VideoPlayerMaths.BarsFullAtOnceDuringTheFilm`; see [bink_playback.md](bink_playback.md) |

## The creature's fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It plays at the end of the game, when the creature has climbed the volcano and leapt in | partial | SET_AVI_SEQUENCE sequence 2 plays it (`src/CHLApi.cpp`, `FallingSpellVideo::KickOff`; tests `FallingSpellVideoTest.*`), but the ending's script is never reached; see ../story/ending.md; the scene round it: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#into-the-volcano) |
| The player's creature is drawn falling in front of the film, with glows on its hands, sparks and a burst of light | partial | the sparks and light burst are drawn (`src/Magic/Objects/FallingSpell.cpp`, `src/Graphics/RendererFallingSpell.cpp`); the falling creature and its hand glows are not ported |
| The camera follows the fall along its recorded path | partial | the path file is read and followed (`falling_spell::CameraPath`, `FallingSpell::UpdateCamera`; tests `FallingSpell.CameraFollowsTheFilm`, `CameraPathDriftsLikeTheOriginal`), but the camera hook is not connected in the game |
| Sounds and a white fade are timed to the film | done | `src/Video/FallingSpellVideo.cpp`; tests `FallingSpellVideoTest.StatesAndSounds`, `TheWhiteFadeEndsIt`; see [bink_playback.md](bink_playback.md) |
| It only plays when the player has a creature | done | `FallingSpellVideo::KickOff`; test `FallingSpellVideoTest.NoCreatureNoFilm`; see [bink_playback.md](bink_playback.md) |
| Afterwards the creature is put back on the land and the game speed and music are restored | todo | the ending's script is never reached; see ../story/ending.md |

## Options and keys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Escape skips a film (fading it out) | done | `VideoPlayer::EscapeKey`; tests `VideoPlayerTest.SkipFadesOver48Frames`, `FallingSpellVideoTest.EscapeSkips`; see [bink_playback.md](bink_playback.md) |
| The video option in the game's options | todo | (unconfirmed what it changes; see ../interface/options.md) |
| Films run while the world is paused; the land's ambience is hushed | done | the game is paused while a film plays and the ambience mix is skipped while `video::IsPlaying` (`src/Video/VideoPlayer.cpp`, `src/Audio/Game/AudioSystem.cpp`) |
