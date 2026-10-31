# Challenge natives: sound and music

The challenge scripts' functions for music, sound effects, spoken sounds and sound tags on objects. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 26/26 done, 0 chosen, 0 partial, 0 inferred — 100%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Plays a sound effect from one of the sound banks, at a position or on an object: *start sound ‹sound› [‹soundbank›] ‹position› ‹with position›* (called 186 times in 54 scripts) | done | `PlaySoundEffect` in `src/CHLApi.cpp`: `audio::script_sound::PlaySoundEffect` (`src/Audio/Services/ScriptSound.cpp`), at the position or not; tests `ScriptSoundTest.PlayWithoutPositionIs2D`, `ScriptSoundTest.GameSoundPlayingUntilStopped` |
| Starts a piece of music: *start music ‹music›* (called 123 times in 84 scripts) | done | `StartMusic`: `GameMusic::ScriptStartMusic` (`src/Audio/GameMusic.cpp`); test `GameMusicTest.ScriptMusicPlaysOnce2DFromChunk1` |
| Stops the script's music: *stop music* (called 108 times in 77 scripts) | done | `StopMusic`: `GameMusic::ScriptStopMusic`; test `GameMusicTest.StopMusicFadesOut` |
| Fixes a piece of music to an object so it is heard near it: *attach music ‹music› to ‹target›* (called 22 times in 13 scripts) | done | `AttachMusic`: `GameMusic::ScriptAttachMusic`, heard near the thing; test `GameMusicTest.ThingMusicPlays3DEveryTurnAndBlocksTheAlignment` |
| Takes the music off an object: *detach music from ‹object›* (called 12 times in 7 scripts) | done | `DetachMusic`: `GameMusic::RemoveThingMusic`; test `GameMusicTest.ThingMusicList` |
| Turns the angle sound (heard as the camera turns) on or off: *enable/disable angle sound* (called 2 times in 1 script) | done | `StartAngleSound285`: `audio::confirmation::StartAngleSound`, the confirmation sound as the camera turns (`src/Audio/Services/Confirmation.cpp`), stopped by the scripts' restart and kept by a land change (`Game::ResetScripts`, [what a land change keeps](../../../scripts/land-change-and-saving.md#what-a-land-change-keeps)); `test/test_confirmation.cpp`; the Land Island window shows it |
| Turns the creature's sounds on or off: *enable/disable creature sound* (called 42 times in 6 scripts) | done | `SetCreatureSound`: the script audio state's creature-sound setting, read by the creature's voices (`src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`); test `ScriptSoundEffect.TheCreatureSoundSettingKeepsTheScriptsValueAsItIs` |
| Gives the line the music has got to: *music line ‹line›* (called 36 times in 3 scripts) | done | `LastMusicLine`: `GameMusic::ScriptLastMusicLine`; without audio it answers as text read, as the original; test `GameMusicTest.MarkersSetTheLineAndTheBeats` |
| Plays a spoken sound effect: *start say [extra] sound ‹sound› ‹position› ‹with position›* (called 74 times in 24 scripts) | done | `GamePlaySaySoundEffect`: `audio::voices::Say`, at the position or not; test `VoicesTest.SayOwnersPositionAndPlaying` |
| Turns the angle sound on or off (second form): *enable/disable angle sound* (called 2 times in 1 script) | done | `StartAngleSound348`: `audio::confirmation::StartPitchSound`, the sound as the camera tilts; test `ScriptSoundEffect.TheTwoAngleSoundsWatchTheTurnAndTheTilt` |
| Turns sound effects on or off: *enable/disable sound effects* (called 8 times in 5 scripts) | done | `SetGameSound`: `audio::SetGameSound`; off stops every sample and only the dialogue banks play; test `ScriptSoundTest.SetGameSoundOffLeavesOnlyTheDialogueBanks` |
| Stops a sound effect: *stop [say] sound ‹sound› [‹soundbank›]* (called 19 times in 7 scripts) | done | `StopSoundEffect`: `audio::script_sound::StopSoundEffect`; test `ScriptSoundTest.GameSoundPlayingUntilStopped` |
| Turns the music that follows the player's alignment on or off: *enable/disable alignment music* (called 2 times in 1 script) | done | `EnableDisableAlignmentMusic`: the script audio state's alignment-music setting, read by `GameMusic`; test `GameMusicTest.AlignmentMusicConditions` |
| Fixes a sound to an object, optionally heard in 3D: *attach [3d] sound tag ‹sound› [‹soundbank›] to ‹target›* (called 5 times in 2 scripts) | done | `AttachSoundTag`: `audio::script_sound::AttachSoundTag`, in 3D or not; tests `ScriptSoundTest.AttachSoundTagReplaysInMode2AndDetachReleasesTheLoop`, `ScriptSoundTest.AttachSoundTag2DDoesNotTrack` |
| Takes a sound off an object: *detach sound tag ‹sound› [‹soundbank›] from ‹target›* (called 6 times in 3 scripts) | done | `DetachSoundTag`: `audio::script_sound::DetachSoundTag`; test `ScriptSoundTest.AttachSoundTagReplaysInMode2AndDetachReleasesTheLoop` |
| Whether a sound effect is playing: *sound ‹sound› [‹soundbank›] playing* (called 17 times in 3 scripts) | done | `GameSoundPlaying`: `audio::script_sound::GameSoundPlaying`; test `ScriptSoundTest.GameSoundPlayingUntilStopped` |
| Whether a spoken sound effect is playing: *say [extra] sound ‹sound› playing* (called 9 times in 1 script) | done | `SaySoundEffectPlaying`: `audio::voices::IsSaying`; test `VoicesTest.SayOwnersPositionAndPlaying` |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moves a piece of music from one object to another: *move music from ‹value› to ‹value›* (not called by the shipped scripts) | done | `MoveMusic`: `GameMusic::MoveThingMusic`; test `GameMusicTest.ThingMusicList` |
| Turns the music on an object on or off: *enable/disable music on ‹value›* (not called by the shipped scripts) | done | `EnableDisableMusic`: `GameMusic::EnableThingMusic`; test `GameMusicTest.ThingMusicPlays3DEveryTurnAndBlocksTheAlignment` |
| Gives the distance from the camera to an object's music (unconfirmed): *get ‹source› music distance* (not called by the shipped scripts) | done | `GetMusicObjDistance`: `GameMusic::ThingMusicDistance`, nought for a missing thing; test `GameMusicTest.PlayDistance` |
| Gives the distance from the camera to a piece of music (unconfirmed): *get music ‹type› distance* (not called by the shipped scripts) | done | `GetMusicEnumDistance`: `GameMusic::ScriptMusicTypeDistances`, two values for a wrong type as the original; test `GameMusicTest.PlayDistance` |
| Moves the music on an object to a point in it: *set ‹value› music position to ‹value›* (not called by the shipped scripts) | done | `SetMusicPlayPosition`: `GameMusic::SetPlayPosition`; test `ThingMusic.PlayPositionQuantised` |
| Starts the music on an object again: *restart music on ‹value›* (not called by the shipped scripts) | done | `RestartMusic`: `GameMusic::RestartThingMusic`; test `GameMusicTest.ThingMusicList` |
| Whether the music on an object has played: *‹value› music played* (not called by the shipped scripts) | done | `MusicPlayed191`: `GameMusic::IsThingMusicFinished`, true for a missing thing; test `GameMusicTest.ThingMusicList` |
| Whether the music on an object has played (second form): *‹value› music played* (not called by the shipped scripts) | done | `MusicPlayed350`: `GameMusic::ScriptMusicPlayed`; without audio it answers as text read; test `GameMusicTest.ScriptMusicPlaysOnce2DFromChunk1` |
| Whether a sound exists: *sound exists* (not called by the shipped scripts) | done | `SoundExists`: `audio::SoundExists`; test `ScriptSoundTest.SoundExistsNeedsTheDevice` |
