# Positional audio

Where sounds are heard from: sounds placed in the world are heard from the camera, louder when near and panned to their
side, and fall silent beyond their distance.

**Progress: 8/8 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../../audio/engine.md).

Music placed in the world is a row of [music.md](music.md) ("Music attached to an object"), and the two main volumes
are the volume rows of [sound_effects.md](sound_effects.md) and [music.md](music.md); this page's own rows for them are
merged there.

## 3D sound

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sounds placed in the world are heard from the camera's position and facing | done | `src/Audio/Engine/SamplePlay.cpp` sets the listener from the camera each turn; the volume and pan laws in `src/Audio/Engine/QMixerLaws.cpp`; tests `AudioLaws.*` |
| The original has no Doppler: its listener has no motion, so moving the camera changes no sound's pitch | done | openblack has none either: `sample_play::UpdateChannels` passes a zero listener velocity (`src/Audio/Device/Device.cpp`), gives no source a velocity and moves only the listener's and the sounds' points as the camera moves ([engine.md](../../../audio/engine.md)); test `SamplePlayTest.MovingTheCameraPastASoundChangesNoPitch` |
| Each sound fades from full at its near distance to nothing at its far one | done | `qmixer` distance law (near, far, scale) in `src/Audio/Engine/QMixerLaws.cpp`; test `AudioLaws.QMixerDistance` |
| A sound beyond its far distance isn't started at all | done | 3D cut-off in `src/Audio/Game/GameSfx.cpp`; tests `ScriptSoundTest.Play3DFartherThanItsMaxDistanceIsCulled`, `GameSfx.GuardSoundPoint` |
| Loops move with their owner each turn | done | tracked 3D channels follow their owner (`sample_play::UpdateChannels`, `src/Audio/Services/SoundTags.cpp`); tests `SoundTagTest.*` |
| Interface sounds are heard centred, not placed | done | 2D channels (no position, no distance law) in `src/Audio/Engine/SamplePlay.cpp`; tests `UiSfxTest.*`, `ScriptSoundTest.PlayWithoutPositionIs2D` |
| Too many sounds at once: the quietest or furthest are dropped | done | 16 shared channels; a new sound takes the lowest-priority channel if its own priority is higher, else it does not play (`src/Audio/Engine/SamplePlay.cpp`); tests `SamplePlayTest.SeventeenthDoesNotPlay`, `SamplePlayTest.PriorityStealsTheLowest` |
| The game runs without a sound device | done | without an OpenAL device the channels answer as an empty engine (`src/Audio/AudioManagerNoOp.h`, `device::IsOpen`); test `AudioManager.TheNoOpBaseAnswersAsAnEmptyEngine` |
