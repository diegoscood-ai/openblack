# Music

The game's music: each tribe's music in good, neutral and evil versions that follow the player's alignment over the
land, the worship chants, the temple's music, fight music, and the pieces the story's scripts start.

**Progress: 22/26 done, 2 partial — 88%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../bw1-notes/audio.md).

## Choosing what plays

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a game turn the music is picked: the temple's music inside the temple, else a script's music, else the land's | done | `game_music::ProcessTurn` in `src/Audio/Services/GameMusic.cpp` (citadel, chant, thing, script, alignment, in that order); tests `GameMusicTest.*` |
| Over land, the music of the nearest town's tribe within 300 of the camera, or the last heard while still within 400 | done | `GameMusic::AlignmentMusicType` with the town from `src/ECS/AudioQueries.cpp`; tests `GameMusicTest.TribeMusicNearTowns`, `AlignmentMusic.TownTriggerOfTheGame` |
| Away from towns, the generic music of the player's alignment | done | `src/Audio/Services/GameMusic.cpp`; test `GameMusicTest.AlignmentMusicGenericAt80WithFade` |
| Too high above the land no town is heard | done | the height test (under 400) in `GameMusic::AlignmentMusicType`; test `GameMusicTest.AlignmentMusicConditions` |
| The alignment is taken in seven steps and grouped into evil, neutral and good | done | `src/Audio/Services/GameMusic.cpp` (discrete alignment and its table); test `AlignmentMusic.DiscreteAlignmentAndTables` |
| Changing between versions of the same music carries on in time | done | sync groups in `src/Audio/Engine/MusicEngine.cpp`; test `MusicEngineTest.SyncStartsOnTheSameChunkAndSample` |
| A piece that played to its end isn't picked again for a while | done | `src/Audio/Services/GameMusic.cpp`; tests `GameMusicTest.SilenceAfterTheAlignmentTrackEnds`, `GameMusicTest.SilenceEndsWhenTheTypeChanges` |
| Each music group remembers where it got to and picks up from there | done | the saved positions per group in `src/Audio/Services/GameMusic.cpp`; test `GameMusicTest.AlignmentMusicFromTheSecondHalfLoopsOnce` |
| Scripts turn the alignment music off and on | done | ENABLE_DISABLE_ALIGNMENT_MUSIC in `src/CHLApi.cpp`, `src/Audio/Services/ScriptAudioState.cpp` |
| The music forgets what played when a new land loads | done | `audio::ClearMap` from `Game::LoadMap`; test `GameMusicTest.ResetValues` |
| A music volume setting | done | `EngineConfig::audioMusicMainVolume`, the options box slider (`src/Game.cpp`) and the Music debug window; not saved to disk yet |
| The music fades out when stopped | done | `src/Audio/Engine/MusicEngine.cpp`; tests `GameMusicTest.StopMusicFadesOut`, `MusicEngineTest.FadeOutTakes43And27Passes` |

## Tribes' music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Celtic, Aztec, Japanese, Indian, Egyptian, Greek and Tibetan town music, each good, neutral and evil | done | `src/Audio/Services/GameMusic.cpp`, banks in `src/Audio/Game/BankTables.h`; test `GameMusicTest.TribeMusicNearTowns` |
| Generic good, neutral and evil music | done | `src/Audio/Services/GameMusic.cpp`, `src/Audio/Game/BankTables.h` |
| Norse towns have no music of their own and play the Celtic music | done | `src/Audio/Game/BankTables.h` (the Norse types point at the Celtic banks) |

## Situations

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's music, of the player's alignment, inside the temple | done | `GameMusic::ProcessCitadelMusic`; the temple interior answers `GameQueries::insideCitadel`; tests `GameMusicTest.CitadelMusicInsideTheCitadel`, `GameMusicTest.CitadelMusicWithoutItsBank` |
| Worship chants: each tribe's chant and its sung version while its people worship | done | `GameMusic::ProcessChantMusic` with the nearest dancing worship site (`ChantSite` in `src/ECS/AudioQueries.cpp`); tests `ChantMusic.Table`, `GameMusicTest.ChantMusicAtTheDance` |
| Creature fight music, and the big fight's own music | todo | the fight branch is in `GameMusic::ProcessTurn`, but nothing registers `GameQueries::creatureFightMusic`; see ../creature/fighting.md |
| Music while the creature dances | todo | the dance branch is in `GameMusic::ProcessTurn`, but nothing registers `GameQueries::creatureDanceMusic` |
| The creature's mood shapes the music | n/a | Our wiki differs: the original has no mood-driven music; what looks like a music mood controller is network packets ([audio](../../bw1-notes/audio.md#intro-trailer-outro-videos-and-menu)) |
| Music attached to an object, heard by distance (the pied piper's tune) | done | `src/Audio/Services/ThingMusic.cpp`, ATTACH_MUSIC, DETACH_MUSIC, MOVE_MUSIC in `src/CHLApi.cpp`; tests `GameMusicTest.PiperOfTheGame`, `GameMusicTest.ThingMusicPlays3DEveryTurnAndBlocksTheAlignment`, `ThingMusic.PlayPositionQuantised` |
| The intro and outro music | partial | the intro and outro pieces are started by the story scripts (START_MUSIC); the trailer music that plays with the pre-intro film is not played; see ../story/ending.md; the outro plays over the credits: [../story/gold_scrolls/so_this_is_a_fight_to_the_death.md](../story/gold_scrolls/so_this_is_a_fight_to_the_death.md#the-credits) |
| Music from the CD's audio tracks | n/a | the game's own banks are used |

## Script music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts start and stop a piece of music | done | START_MUSIC and STOP_MUSIC in `src/CHLApi.cpp`, `GameMusic::ScriptStartMusic`; test `GameMusicTest.ScriptMusicOfTheGame` |
| A script piece plays from its start until stopped | done | `src/Audio/Services/GameMusic.cpp`; test `GameMusicTest.ScriptMusicPlaysOnce2DFromChunk1` |
| The story's pieces: the piper's tunes, the hermit, the missionaries, the intro, the singing stones, the welcome dance, creature chosen, funeral, creature guide, Khazar, Nemesis, twinkle, the whistles, Sleg the ogre, guardian stone, failure, gregorian, Christmas, circus, the epics and the creature's end sequence | done | every script music type in `src/Audio/Game/BankTables.h`; test `AudioTables.MusicBanks` |
| Scripts ask whether a piece has played, how far from it they are, and set where it plays from | done | MUSIC_PLAYED, LAST_MUSIC_LINE, GET_MUSIC_OBJ_DISTANCE, GET_MUSIC_ENUM_DISTANCE, SET_MUSIC_PLAY_POSITION, RESTART_MUSIC in `src/CHLApi.cpp`; tests `GameMusicTest.PlayDistance`, `GameMusicTest.MarkersSetTheLineAndTheBeats` |
| The missionaries' sing-along with a bouncing ball over the words | partial | the music's line markers are read (LAST_MUSIC_LINE, test `GameMusicTest.MarkersSetTheLineAndTheBeats`); no bouncing ball over the words; see voices_and_speech.md; the quest: [the_explorers.md](../story/silver_scrolls/the_explorers.md) |
