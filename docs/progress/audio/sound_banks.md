# Sound banks and formats

How the game stores its sounds: banks of samples with headers saying how each plays, music banks of compressed chunks
streamed back to back, and loose wave files.

**Progress: 8/9 done, 1 partial — 94%**

How the original does it, in our wiki: [Audio: the engine, the banks, the music, the voices and the script](../../bw1-notes/audio.md).

## Banks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sound banks are read with every sample's header | done | `src/Audio/Game/Banks.cpp` (`audio::banks`, every `.sad` under Audio); tests `AudioTables.*`, `AudioGolden.BankWaves` |
| Samples are decoded from the banks' formats | done | our own decoders in `src/Audio/Codec/` (`WaveFile`, `MpegAudio`: PCM, MS-ADPCM, MPEG layer II), `src/Audio/Device/WaveBuffers.cpp`; tests `AudioWave.*`, `AudioMpeg.*`, `AudioGolden.*` |
| Loose wave files | done | `src/Audio/Codec/WaveFile.cpp` (RIFF and the other wave containers), `src/Audio/Device/WavAudioDecoder.cpp`; tests `AudioWave.*` |
| Music banks are compressed chunks, streamed and played back to back | done | `src/Audio/Engine/MusicBank.cpp`, `MusicStream.cpp` (six tracks on their own thread), `MusicEngine.cpp`; tests `MusicBank.*`, `MusicStream.*`, `MusicEngineTest.*` |
| A music bank's group and length | done | `src/Audio/Engine/MusicBank.cpp`; tests `MusicBank.FirstSegmentFields`, `MusicBank.GroupsOfTheTypeTable` |
| Every bank in the audio folder is loaded at start | done | `banks::LoadAll` from `audio::Init` registers every sample bank; the dialogue banks keep only their headers and read each wave at its first play (test `LazyDialogueBank.*`); music banks are registered at their first use. Our wiki differs: the original registers all its banks at start-up, headers only, and loads each wave on first use ([audio](../../bw1-notes/audio.md#wave-loading-and-cache)) |
| Animation effect tables in the banks | done | `src/Audio/Engine/AnimEffects.cpp`, `AnimEffectBank.h` (tables read once per bank); tests `AnimEffectsTest.*` |
| Atmosphere banks load and release with each land | done | `src/Audio/Services/AtmosBanks.cpp`: registered once with the other banks, silenced on a new land (`atmos_banks::Clear`). Our wiki differs: the original registers the ambience banks once and does not reload them per land ([audio](../../bw1-notes/audio.md#table-of-effect-and-dialogue-banks)) |
| Banks are released when no longer needed | partial | the music banks are released when the audio closes (`banks::ReleaseMusicBanks`); a decoded wave keeps its buffer until then (`wave_buffers::Release`), with no memory budget and no eviction of the oldest wave as the original's cache does |
