# Positional audio

Where sounds are heard from: sounds placed in the world are heard from the camera, louder when near and panned to their
side, and fall silent beyond their distance.

**Progress: 10/10 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../../audio/engine.md).

## 3D sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds placed in the world are heard from the camera's position and facing | done | `src/Audio/Engine/SamplePlay.cpp` sets the listener from the camera each turn; the volume and pan laws in `src/Audio/Engine/QMixerLaws.cpp`; tests `AudioLaws.*` |
| The listener has no motion, so moving the camera makes no Doppler | done | `sample_play::UpdateChannels` passes a zero listener velocity (`src/Audio/Device/Device.cpp`) |
| Each sound fades from full at its near distance to nothing at its far one | done | `qmixer` distance law (near, far, scale) in `src/Audio/Engine/QMixerLaws.cpp`; test `AudioLaws.QMixerDistance` |
| A sound beyond its far distance isn't started at all | done | 3D cut-off in `src/Audio/Game/GameSfx.cpp`; tests `ScriptSoundTest.Play3DFartherThanItsMaxDistanceIsCulled`, `GameSfx.GuardSoundPoint` |
| Loops move with their owner each turn | done | tracked 3D channels follow their owner (`sample_play::UpdateChannels`, `src/Audio/Services/SoundTags.cpp`); tests `SoundTagTest.*` |
| Interface sounds are heard centred, not placed | done | 2D channels (no position, no distance law) in `src/Audio/Engine/SamplePlay.cpp`; tests `UiSfxTest.*`, `ScriptSoundTest.PlayWithoutPositionIs2D` |
| Music placed in the world, heard by distance | done | `src/Audio/Services/ThingMusic.cpp`, `GameMusic` (ATTACH_MUSIC, DETACH_MUSIC, MOVE_MUSIC in `src/CHLApi.cpp`); test `GameMusicTest.ThingMusicPlays3DEveryTurnAndBlocksTheAlignment` |
| A master volume over everything | done | sample and music main volumes (0..127): `audio::SetSampleMainVolume`, `EngineConfig::audioMusicMainVolume`; the options box sliders in `src/Game.cpp`; both kept for the computer in `Setup.txt` beside the players' profiles, read and kept when the original reads and keeps them in its setup's registry key (`src/Audio/MainVolumesSetup.cpp`; tests `MachineSetup.*`). Read again 2026-10-22: the note "not saved to disk yet" was stale |
| Too many sounds at once: the quietest or furthest are dropped | done | 16 shared channels; a new sound takes the lowest-priority channel if its own priority is higher, else it does not play (`src/Audio/Engine/SamplePlay.cpp`); tests `SamplePlayTest.SeventeenthDoesNotPlay`, `SamplePlayTest.PriorityStealsTheLowest` |
| The game runs without a sound device | done | without an OpenAL device the channels answer as an empty engine (`src/Audio/AudioManagerNoOp.h`, `device::IsOpen`); test `AudioManager.TheNoOpBaseAnswersAsAnEmptyEngine` |
