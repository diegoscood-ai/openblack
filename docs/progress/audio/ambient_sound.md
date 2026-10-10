# Ambient sound

The sound of the land itself: sea, shore, rivers, wind, rain, birds and insects, mixed from a map of the land's sound
types around the camera and changing with height, weather, time of day and the player's alignment.

**Progress: 11/11 done, 0 partial — 100%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../bw1-notes/audio.md).

## The ambience

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each kind of place has its bank: sea, still water, coast, jungle, arctic, desert, countryside, swamp, running water, high up, night, rain and wind | done | `src/Audio/Services/SoundMap.cpp` (the 14 ambience types), `src/Audio/Services/AtmosBanks.cpp` (their banks) |
| Each turn the sound map around the camera sets how loud each bank should be | done | `sound_map::Update` scans the cells around the camera each audio turn (`src/Audio/Services/SoundMap.cpp`); test `SoundMap.StratosphereVolumeRisesAndFalls` |
| Banks fade towards their volume by a set step a turn | done | `src/Audio/Services/AtmosBanks.cpp` (`UpdateBanks`: a step of 0.02 or 0.04 a turn towards the target) |
| The time of day (day, dusk, night) chooses the banks' samples, night sounds taking over | done | `src/Audio/Services/SoundMap.cpp` (sky type from `src/3D/SkyType.cpp`: the night bank fades in, the day banks out) |
| The camera's height above the land fades the ground's banks and brings in the high-up one | done | `HeightFade` in `src/Audio/Services/SoundMap.cpp`; test `SoundMap.StratosphereVolumeRisesAndFalls` |
| Rain and wind follow the weather at the camera | done | `src/Audio/Services/SoundMap.cpp` reads the smoothed weather at the camera (`GameQueries::weatherSmooth`, `src/ECS/AudioQueries.cpp`) |
| Over land of a god at or below -0.6 alignment the banks switch to their evil samples | done | `atmos_banks::GroupFor` in `src/Audio/Services/AtmosBanks.cpp`; test `AudioLaws.AtmosGroupByAlignment` |
| Each bank schedules its own samples at random times and places | done | `src/Audio/Services/AtmosBanks.cpp` (the mixer's queue of one-shots at random times and points, the audio library's random generator) |
| Inside the temple the ambience fades to silence | done | `SetTargets(insideCitadel)` in `src/Audio/Services/AtmosBanks.cpp`; the temple interior answers `GameQueries::insideCitadel` |
| The ambience is quiet while a film plays | done | `ProcessAudioGameTurn` in `src/Audio/Game/AudioSystem.cpp` skips the ambience mix while `video::IsPlaying` (our Bink player, connected to the game's films) |
| The sound map is made from the land when it loads | done | the sound map reads each cell's surface type from the loaded land (`ecs::sea_cells::GetSurfaceType`, `src/ECS/SeaCells.cpp`) |
