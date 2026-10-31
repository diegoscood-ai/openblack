# The audio engine

## Summary

The original plays every sound through GAudio, the game's audio object in `runblack.exe`, on top of Lionhead's audio
DLL (LHaudio) and QMixer: 16 sample channels and 6 music tracks at 22050 Hz, pumped every 20 ms, with no Doppler,
EAX or reverb. Every effect goes through one entry point with a fixed set of filters (distance, the citadel, the
script's wide screen, the game-sound switch, the creature fight), and the audio's turn runs the music, the ambience,
the channels and the listener after the game turn.

openblack has one audio engine of its own, in layers under `src/Audio`: an OpenAL device, the bank registry, the
channels, the music engine, the ambience and the game's services (voices, Guidance, music, the world's callers), with
our own decoders for every format the game uses. It reproduces the original's filters and order; what is not ported
yet is in Pending and on each topic's page.

## The original

The addresses are the v1.42 exe's (with the 1.20 symbols; the patch did not change the audio) and, for the audio
DLL, `LHaudiodllR.dll`'s (0x1000xxxx). The pages of the audio section:

- this page: the engine (GAudio, the LHaudio DLL, QMixer), the filters every sound passes, and openblack's engine;
- [Banks and channels](banks-and-channels.md): the banks, the waves, the decoders, the 16 channels, the volumes and
  the ambience;
- [Music](music.md): LHMusic's six tracks and what plays when;
- [Voices and texts](voices.md): the voice table, the narration, the advisors, Guidance and the night voices;
- [Script audio](script-audio.md): the CHL audio functions and the world's callers;
- [World sounds](world-sounds.md), [Magic and weather sounds](magic-and-weather-sounds.md),
  [Creature and interface sounds](creature-and-interface-sounds.md): what sounds, and when;
- the file formats (.sad, .sas, the music segments): [Media files](../formats/media-files.md); the 24-bit FPU and the
  owners' arithmetic: [Floating point](../engine/floating-point.md#owners-the-24-bit-fpu-and-the-double-constants).

Progress: [audio](../project/progress/audio/) (what our tree does of it, row by row).

### Audio engine

#### Original: GAudio, LHaudio and QMixer

**Faithful.**

- **Creation.** `pc_main` 0x641D79 calls fn_00590FD0, which creates `GGlobal::Global.audio = new GAudio`
  (0x3D4 bytes, ctor 0x426D40). Before it come `PlayLogoScreens` and `Report3D("InitialiseAudio")`, and after it
  `PlayPreIntroVideo`. The ctor does the following:
  - Creates `LH_AudioSystem` (0x1EC bytes) with GAudio's values: 16 sample channels (sys+0xCC), 22050 Hz
    (0x5622, sys+0xD8), a wave cache of RAM/8 (`dwTotalPhys >> 3`, 0x426DDC) and no hardware.
  - With `LHWaveIsInstalled() == 1` it registers `fn_00427200` as the 3D position function, with 800.0f as the maximum
    distance of the anim-effects (0x426E6B; it is a distance, not a number of sounds).
  - Reads the master volumes (fn_00428250) and registers the 85 music banks of 0x9C9748 and the 11 effect banks of
    0x9CB3F8 (fn_00429CB0 → fn_0042A350 → fn_0042A390).
- **LH_AudioSystem.**
  - Ctor 0x10015290 and `Create` 0x100153F0. `Create` can only be called once ("NO, NO, NO!! - one-time audio
    object only").
  - Reads the registry key `...\Audio\Override` (0x100103E0). Its values are NoAudio, Wave, Midi, Music, Redbook,
    Atmos, Sfx, 3DSpatialisation, UseHardware, SpeakerConfig, HeapSize, MaxSamp, HWRate and Latency. On the user's
    machine the key does not exist.
- **QMixer** (0x10015820).
  - Opens 22 software channels: 16 for samples and 6 for music. It works at 22050 Hz.
  - `QSWaveMixSetPanRate(16, 1, 100)`.
  - The pump runs via `timeSetEvent(20, …, 0x10015800)`, every **20 ms**.
  - The listener velocity is 0 (0x10015C1A): **there is no Doppler, no EAX and no reverb**.
- **All triggers go through a single path**: `GAudio::PlaySoundEffect` 0x429E30 (with its 5 variants) or
  `SamplePlayAnimEffect` 0x42A4B0. The only exceptions are:
  - `HelpDude::PlaySample` 0x5BB530, which calls `LHSamplePlay` directly, without GAudio's filters;
  - `LHAtmosProcess`;
  - 8 calls to `LHMusicPlay`.
- **Stopping and querying** are also done outside GAudio:
  - `LHSampleStop` in FallingSpell (0x526FD6, 0x527181, 0x52719D) and in `HelpDude::StopSentence` 0x5BB875;
  - `LHMusicStop` in FallingSpell 0x5271B0 and in `PlayPreIntroVideo` 0x642907;
  - `LHSampleIsPlaying` and `LHSampleSetVolume` in PSysSound fn_006D1110 (0x6D120A, 0x6D1239);
  - the HelpDude queries (0x5BB797, 0x5BB80D, 0x5BCD66).
- **`PlaySoundEffect` filters** (0x429E37..0x429FD9, 0x42A56E):
  - Nothing sounds if `g_game` or HelpSystem are null.
  - 3D cutoff if dist²(camera, pos + offset) > max². The maximum is the one from the .sad (+0x26C) or, if it is 0, the one
    from opts+0x58 = 9999. Inside the citadel, the distance is measured from `LH3DTech::g_camera` 0xEA1DB8.
  - userParam 1 (script widescreen) does not sound with the black bars on. Inside the citadel only userParam 2
    sounds.
  - With `SET_GAME_SOUND` only banks 6 and 7 sound (GAudio+0x3C0/+0x3C4).
  - userParam 4 does not sound in the interface action states (GInterface+0x44) 0x10, 0x16 and 0x17
    (0x429FA5..0x429FB8). Their names, from the exe's action state table (built by the initialiser 0x5D7970, names at
    0xD18290 + 0x20·state): **0x10 CREATURE FIGHT, 0x16 FIGHT_APPLY, 0x17 FIGHT_SELECT** (the same table gives 0xC IN
    THROW, 0x11 BUBBLE GRIP and 7 WAIT FOR PLACE IN HAND, as in hand-and-interface.md). openblack's
    `SoundEffectConditions::creatureFightControl` (`src/Audio/ScriptSoundEffect.h`) stands for these three states.
  - With is3D and track, nothing sounds if the owner is a GameThing with `IsAvailable() == 0`. The symbol
    0x429D20 (named `GAudio::IsOwnerUnavailable` in the names layer; bw1-decomp leaves it unnamed) answers whether the owner is not available; bw1-decomp's `GAudio::IsInsideCitadel` is 0x4282F0.
- **Inside the citadel** = `g_game+0x205A28 == 1` (0x4282F0; the symbol `GetWideScreenControl` is misnamed).
  It is set to 1 by `GoInsideCitadel` 0x554004 and to 0 by `LeaveInsideCitadel` 0x553B1F.
- **Per-turn order**, `ProcessAudioGameTurn` 0x427080. It is called by `GGame::EndTurn` after turn 5 (0x54E997):
  1. Only if `LHWaveIsActive` (0x427086): the music (0x427DF0), the ambience targets (0x429100), the ambience
     banks (0x428FE0), `UpdateChannels` + the listener (0x4270D0) and `LHAtmosProcess(1)` if `g_game+0x250188 == 0`
     (no video playing: +0x250188 is the LHVideoPlayer).
  2. **Always**: fn_00429700, which purges the ThingMusicInfo list.
- **Game fields the audio reads.**
  - g_game+0x250188 is the LHVideoPlayer: written by fn_0054AB20 (0x54AC23), cleared by `DeleteVideo` 0x54A969 and
    `ClearVariables` 0x54BF28. Only the intro and fall films set it, not the tips or the pre-intro video. It is tested by
    ProcessMusic (0x427DF8) and by ProcessAudioGameTurn (0x4270B1; LHAtmosProcess(1) is skipped, 0x4270AC..0x4270C0).
  - g_game+0x205A08 is the land number (SET_LAND_NUMBER 0x7177A4); ProcessMusic plays nothing on land 6 (0x427E1D).
  - g_game+0x205A0C is the playground-game flag: `GGame::Init` sets it at 0x54F75E, after `ResetAndStartPlaygroundGame`.
  - The turn test of ProcessAlignmentMusic (turn > 20) is unsigned (0x427A2E).
  - The interface state is GInterface+0x44 (`GGame::MyInterface` 0x555850); the user-parameter-4 filter reads it at
    0x429FA5..0x429FB8.
  - The local player's number is `GPlayer::GetPlayerNumber` 0x64A790 (+0xB5); the local player is g_game+0x205A59. A new
    game without a profile starts with alignment 0 (`GGame::Init` 0x54FEA0).
  - The position of a script thing is built from its MapCoords as (x, GetAltitude + height above the land, z)
    (0x42943F..0x429470). The thing is unavailable when `GameThing::IsAvailable` (vt +0x2C) is 0.
  - `GAudio::IsInstalled` 0x426D30 → `LHWaveIsInstalled` 0x10015D20; `LHWaveIsActive` is 0x10015D30.
  - `GAudio::ToBeDeleted` 0x426FE0 → `~LH_AudioSystem` (StopAll first, 0x10015C70): the channels stop before the device
    is closed.
  - Call sites inside ProcessAudioGameTurn: ProcessMusic 0x427092, fn_00429100 0x427099, ProcessAtmosBanks 0x4270A0,
    LHAtmosProcess(1) 0x4270C0, fn_00429700 0x4270C8.
- **`PlaySoundEffect` variants.**
  - In every variant sample 0 plays nothing (0x429DA9, 0x42A04C, 0x42A109).
  - 0x429DA0: a 3D sound with no owner, or with an unavailable owner, plays nothing (0x429DB5..0x429DD7). Otherwise the
    point is the owner's Get3DSoundPos, or (0, 0, 0) for a 2D sound. Then it calls 0x42A040.
  - 0x42A040 fills GAudio's working options +0x240 (ctor call at 0x426D63): bank +0x04, owner +0x20, sample +0x24,
    +0x10 = the 6th argument (0x42A06E), is3D +0x08, track +0x0C = is3D (0x42A087), point +0x30, offset +0x3C = 0
    (0x42A0B5..0x42A0C1), loops +0x4C, mode +0x50. A 3D sound of an unavailable owner plays nothing
    (0x42A0CE..0x42A0DB). Then it calls 0x429E30.
  - 0x42A100 takes the offset into +0x3C (0x42A15C..0x42A174) and track +0x0C from the low byte of its 5th argument
    (0x42A18E..0x42A19C). It tests the owner only when both is3D and track are set (0x42A19A..0x42A1B4).
  - The user parameter the filters compare is the low 16 bits of LHSampleGetUserParam (`cmp di, 1` at 0x429F65; read
    at 0x42A520 in SamplePlayAnimEffect). SET_GAME_SOUND's flag GScript+0x90 is read at 0x429F88 and at 0x42A56E.
  - Inside SamplePlayAnimEffect: the filters 0x42A51B..0x42A5B8, the play `LHSamplePlayAnimEffect` 0x42A5D4. A stop or
    release (action ≠ 0) passes no min and no max (0x42A4C6 / 0x42A4C8).
- **Cyclic sample counters**: the sample is base + counter, then the counter goes up by one and wraps to 0 at its count
  (Abode::InterfaceTap 0x4068F4..0x40690F; the tree mulch adds first and masks with 3, 0x63AA39):
  - [0xC5E3E4] 0..4: G_CitadelSpark 206 + c (fn_004686B0 0x468815);
  - [0xC5E3E8] 0..4: G_CitadelSpark 206 + c (`CitadelHeart::ProcessTransferedDamageEffect` 0x468B32);
  - [0xC6421C] 0..3: G_RockTap 139 + c (fn_0048BCC0 0x48C433);
  - [0xC64220] 0..2: G_SquashAnimal 143 + c (fn_0048BCC0 0x48C510);
  - [0xD95AF8] 0..3: G_ScaffoldCombine 201 + c (`Scaffold::ApplyThisToObject` 0x6E9AC9);
  - [0xD95AFC] 0..3: G_ScaffoldTap 151 + c (`Scaffold::InterfaceTap` 0x6E9E7E).
- **Samples picked with GetTickCount()**: Tree::DropSfx 83 + t % 3 (0x74BCB6, confirmed: xref immediates), Tree::ApplyWaterSpell **120** + t % 9
  (0x74C4B3), PhysicsObject::GameTurnUpdate 69 + t % 5 (0x645BF8), CameraModeNew3::FlyToPosFoc 46 + (t & 3) (0x458986),
  and the pile fn_0066D1A0.

- GetTickCount() of the original (milliseconds of the process's clock, wrapping at 2^32): the callers that pick a sample with it instead of a random generator (Tree::DropSfx 0x74BCB6: 83 + t % 3, Tree::ApplyWaterSpell 0x74C4B3: 120 + t % 9, PhysicsObject::GameTurnUpdate 0x645BF8: 69 + t % 5, CameraModeNew3::FlyToPosFoc 0x458986: 46 + (t & 3), fn_0066D1A0) (in the code: `TickCount`)
- the mode LHSamplePlay starts with: the .sad's with its flag 0x400 unless the caller set it (0x10011B5C) (in the code: `TraceSoundEffect`)
- 0x42A4BC: an action goes to the key variant 0x100146F0 with no filter and no min / max (0x42A4C6 / 0x42A4C8) (at `if (action != AnimAction::Play)`) (in the code: `audio::PlayAnimationEffect`)

### What sounds and when

Each topic page says what sounds and when. Here there is only the engine:
- Picking up and dropping, piles, pots: [Objects](../world/objects.md) ("Sounds").
- Sounds of the animation clips and the banter: [Clip sounds](world-sounds.md#clip-sounds).
- Hits, collisions and throws: [Sounds, dust and the look of impacts](../physics/physics-impacts.md#sounds-dust-and-the-look-of-impacts).
- Street lanterns (SoundTag, at night): [Day and night](../world/day-and-night.md) ("Night lights").
- Trees: leaves in [Drawing](../world/trees.md#drawing), falling in [Dropping and replanting](../hand/grab-hold-and-throw.md#dropping-and-replanting).
- Miracle particles (SOUND_ACTION, PSysSound, spells.sad): [Particles: the PSys core](../physics/particles.md) ("Sound of the particles").
- Water (the hand in the water, hits, drowning, boat, waterfall and ark, sea, coast and lake ambience):
  [Water audio](world-sounds.md#water-audio). Spells: [Particles: the PSys core](../physics/particles.md) and [The spell core](../magic/spell-core.md).

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `LHBankRegister 0x10002383..0x100023BD` | the audio DLL's reading of the first three u32 of a .sad's bank info block | `pack::AudioBankInfo` |
| `LHBankRegister (LHaudiodllR 0x10002240)` | registers a .sad bank; fails without LHFileSegmentBankInfo (0x10002368); with inMemory = 0 it keeps the file open (b+0 = 1) | `pack::PackFile (audio bank)` |
| `LHIsMusicBank 0x10002EE0` | returns the bank's 3rd u32 (bank+4) | `pack::PackFile::IsAudioMusicBank` |
| `LHMusicPlay 0x1000DF60` | tests LHIsMusicBank for non-zero | `pack::PackFile::IsAudioMusicBank` |
| `LHaudiodllR 0x10011420 -> fn_100032D0` | reads a wave at its first play | `pack::PackFile (wave data)` |

## openblack

### openblack audio architecture

One folder per layer of the original, the facade at the root. The game only includes `Audio/Audio.h` (and, for what is
not effects, its service's header: `GameMusic.h`, `Guidance.h`, `Voices.h`...).

`Game::~Game` closes the audio in the original's order: the sample channels, then GAudio's music before LHMusic, then
the music thread and its OpenAL sources before the audio context (LHMusicClose 0x1000E7A0). `audio::guidance::Close` is
GInterfaceStatus::UnInit 0x5DD226 → GGuidance::Close, and fn_0054EB40 0x54EC24 → fn_0072E280 closes GSpookyVoices'
options.

> **Code rules.** New state lives in ECS components or Locator services, not in new globals; banks and other files load
> through the resource caches; resources are owned with RAII and standard types (`std::unique_ptr`, `std::optional`,
> `std::span`, `std::array`); the pure logic (the QMixer laws, the music engine, GameMusic, Guidance) is unit tested in
> `test/` with fakes, never through the locator; comments describe behaviour in plain English, with no decompiled
> names or addresses (those belong here). See [the conventions](../project/refactor-conventions.md#coding-conventions-summary).

```
          game (ECS, hand, camera, Magic, PSys, Worship, CHLApi, Help, menus, Debug)
              │ #include "Audio/Audio.h"                    ▲ GameQueries (std::functions registered by
              ▼                                             │ Game.cpp and ecs::audio_queries)
 4. Audio.h        facade: GAudio's signatures (PlaySoundEffect, StopSoundEffect, PlayAnimationEffect, tags,
                   RegisterObject, Counter, TickCount, Init/ProcessTurn/ClearMap/OnFocus...) · GameQueries.h
 3. Services/      SoundTags · SoundMap + AtmosBanks · GameMusic + ThingMusic · Voices · Advisor · Guidance ·
                   SpookyVoices · ScriptSound · ScriptAudioState · AnimationSounds · LanternSounds · SpellSounds
 2. Game/          AudioSystem (filters, owners, masters, ProcessTurn) · GameSfx (the 5 variants of
                   PlaySoundEffect, counters) · Banks (LHBankRegister of every .sad) · BankTables (0x9CB3F8,
                   0x9C9748)
 1. Engine/        LHaudio/QMixer emulated: SamplePlay (16 channels, the DLL's generator) · QMixerLaws · AnimEffects
                   (+ AnimEffectBank) · MusicBank · MusicEngine (LHMusic) · MusicStream (its 6 tracks and the thread)
 0. Device/        Device (the only one that calls OpenAL; GetTickCount) · AlSampleOutput · SampleOutput · WaveBuffers ·
                   Sound (the record of a sample) · decoders (audio::codec: WaveFile, MpegAudio)
```

**Who calls whom.** Downwards, except for what the original also does upwards:
- `Game/AudioSystem` distributes the turn (GGame::EndTurn 0x54E960 → GAudio::ProcessAudioGameTurn 0x427080) among the
  services: `sound_map::Update`, `lantern_sounds::ProcessTurn`, `tags::ProcessSoundTags`, ProcessMusic, the ambience
  banks. This is what GAudio does in the original.
- The services use the GAudio functions declared in `Audio.h` (PlaySoundEffect and its family) and `AudioSystem.h`
  (`Queries`, `ListenerPoint`, `SurfaceType`, `IslandAltitude`, `OwnerSoundPosition`).
- `Engine/` uses the types of `Game/` (`BankId`, `SfxBank`, `MusicType`) and GAudio's `OwnerSoundPoint` (was `Get3DSoundPos`; fn_00427200, the
  3D function that the game registers in the DLL, 0x426E6B). The older names in `SamplePlay` (`SetGameSound`,
  `IsInsideCitadel`...) forward to GAudio.
- `Device/` knows nobody above it, except the bank registry that `WaveBuffers` reads (`banks::ReadWave`).

**What the audio asks the game**: `audio::GameQueries` (`src/Audio/GameQueries.h`). No file in `src/Audio`
includes an ECS component or `ECS/*`:
- `Game.cpp` (`MakeMusicQueries`) registers those for the game state: land, turn, camera, position of a thing
  (`thingPosition`, the owner `Owner::Thing`), hand, widescreen, HelpSystem, night...
- `src/ECS/AudioQueries.{h,cpp}` (`ecs::audio_queries::Fill`, called from `MakeMusicQueries`) registers those that read the
  ECS registry and its systems: `surfaceType` (`ecs::sea_cells::GetSurfaceType`, the single GSoundMap::GetSurfaceType
  0x71D8E0), `weatherSmooth` (`weather::atmos::GetWeatherSmooth` 0x835180), `cameraAlignment` (GAudio+0x190 as
  fn_005E2240 writes it from `ecs::effects::alignment::GetInterfaceAlignment`), `animatedThing` (`AnimatedThing`, what fn_00516510 reads:
  position, TurnsSinceStateChange and, for a villager, alive/child/woman/house; no answer without `Get3DSoundPos`,
  0x516548), the voices' towns (`NearestTownAt`, the town of a resource drop through `MapCoords::GetNearestTown`
  0x6020E0; `DesireTowns`, each town with its storage pit's point, `GetStoragePit` 0x73B5B0; `WorshipSites`, nothing
  without a citadel as at 0x71B2AB), `animationClipName` (LoadAllAnimations
  0x550180) and `streetLanterns` (the list g_game+0x205C34 with Object::GetHeight 0x638120). Also the test hooks
  that move the camera (`OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` / `_LANTERN`, `ecs::audio_queries::RunTestHooks`).
- Objects that are not things (ParticleSound, the fire, the hand...) give their point with `RegisterObject` (`Owner::Object`).
- An unregistered query gives the value of a game without that system (each one says so in its comment).

**A single place for each thing**:

| what | where |
|---|---|
| OpenAL (device, sources, buffers, axes) | `Device/Device.{h,cpp}`, `audio::device` |
| GetTickCount (real ms) | `device::TickCount()` → `game_clock::TickCount()`; the public API is `audio::TickCount()` |
| the DLL's generator (rand from its CRT, srand(time(0)), LH_AudioSystem::Rand) | `sample_play::Rand`, `SeedRand`, `AlternatingRandom` (was `AudioSystemRand`), `Random` (`Engine/SamplePlay.h`) |
| GRand::LocalRand / LocalFloatRand (the game's, g_game+0x205A3C) | `guidance::LocalRand` / `LocalFloatRand` (`Services/Guidance.h`); `tags::RandomSample` uses it |
| reading a .sad | `Game/Banks.cpp`, `audio::banks` |
| the surface at a point | `ecs::sea_cells::GetSurfaceType` (game); inside the audio `audio::SurfaceType` (query) |

**Where each new thing goes**:
- a sound from a game system: in the system's file, calling `Audio.h` with the original caller's arguments
  (no default arguments); no `PlaySound` or `CreateEmitter` (they do not exist);
- a new GAudio service (GConfirmation, the creature...): `Services/`, with its header, called per turn
  from `AudioSystem` if the original does it from ProcessAudioGameTurn;
- a piece of game data that the audio needs: a new `GameQueries` field with its neutral value, registered in
  `Game.cpp` or, if it reads the ECS, in `src/ECS/AudioQueries.cpp`;
- a DLL function (LHSample*, LHMusic*, LHAtmos*): `Engine/`; something for OpenAL: `Device/`.

**File map**:

| file | layer | original | what it is |
|---|---|---|---|
| `Audio.h` | 4 | GAudio | the public API |
| `AudioApi.cpp`, `AudioManager.h`, `AudioManagerInterface.h`, `AudioManagerNoOp.h` | 4 | — | the audio service behind `Locator::audio`: the functions of `Audio.h` handing their calls to it, its interface, this engine as its implementation, and the base of a test's fake |
| `GameQueries.h` | 4 | — | what the audio asks the game |
| `Services/SoundTags.{h,cpp}` | 3 | SoundTag 0x71E300..0x71ED90 | sounds tied to a thing or a point (`audio::tags`) |
| `SoundMap.{h,cpp}` | 3 | GSoundMap 0x71D6F0 | ambience zones around the camera |
| `Services/AtmosBanks.{h,cpp}` | 3 | InitAtmos 0x428EF0, LHAtmos | the 14 ambience banks and their mixer |
| `GameMusic.{h,cpp}` | 3 | GAudio (music) 0x427DF0 | ProcessMusic and its sources, script music |
| `Services/ThingMusic.{h,cpp}` | 3 | ThingMusicInfo | ATTACH_MUSIC |
| `Services/Voices.{h,cpp}` | 3 | voice table 0x915D40 | voices for the texts and the script |
| `LipSync.{h,cpp}` | 3 | HelpDude | the advisors' lip-sync analysis, `audio::lip_sync` (their voices: `src/Help/AdvisorVoices.{h,cpp}`, held by the help texts slot) |
| `WaveCueLabels.{h,cpp}` | 3 | `AudioTag::BuildAudioTags` 0x42AE70 (audiotag.cpp) | the labels of a wave's cue points, `audio::ReadWaveCueLabels`, which the advisors' gestures are read from |
| `Services/Guidance.{h,cpp}` | 3 | GGuidance 0x71AB10..0x71D490 | villager and advisor comments; GRand::LocalRand |
| `Services/SpookyVoices.{h,cpp}` | 3 | GSpookyVoices 0x72E130 | night voices |
| `Services/ScriptSound.{h,cpp}` | 3 | effects CHL | PLAY/STOP_SOUND_EFFECT, script tags |
| `Services/ScriptAudioState.{h,cpp}` | 3 | GScript +0x84..+0x9C | script audio switches |
| `Services/AnimationSounds.{h,cpp}` | 3 | fn_00516510, Tree::Draw | sound events of the clips (`Fire`, `PlayFromTable`) |
| `Services/LanternSounds.{h,cpp}` | 3 | GStreetLantern +0x60 | the tag of each street lantern (`SetOn`, `ProcessTurn`, `Clear`) |
| `Services/SpellSounds.{h,cpp}` | 3 | PSysSound fn_006D11A0 | particle sounds (miracles) |
| `Game/AudioSystem.{h,cpp}` | 2 | GAudio 0x426D40 | state, filters, owners, masters, turn |
| `Game/GameSfx.cpp` | 2 | GAudio::PlaySoundEffect 0x429D60..0x42A330 | the PlaySoundEffect family, counters |
| `Game/Banks.{h,cpp}` | 2 | LHBankRegister from fn_00429CB0 | registering and loading banks |
| `Game/BankTables.h` | 2 | 0x9CB3F8, 0x9C9748 | effect and music bank tables |
| `Engine/SamplePlay.{h,cpp}` | 1 | LHSample* | the 16 channels; the DLL's generator |
| `Engine/QMixerLaws.{h,cpp}` | 1 | QMixer | volume, distance, polar, pitch |
| `Engine/AnimEffects.{h,cpp}`, `Engine/AnimEffectBank.h` | 1 | LHSamplePlayAnimEffect 0x10014A20 | anim-effect tables |
| `Engine/MusicBank.{h,cpp}` | 1 | LHBankRegister (music) | MP2 segments of a music bank |
| `Engine/MusicEngine.{h,cpp}` | 1 | LHMusic | 6 tracks, fades, queue |
| `Engine/MusicStream.{h,cpp}` | 1 | LHMusic + QMixer | the 6 tracks on the device and the 120 ms thread |
| `Device/Device.{h,cpp}` | 0 | LH_AudioSystem::Create, QMixer, GetTickCount | OpenAL |
| `Device/AlSampleOutput.{h,cpp}`, `Device/SampleOutput.h` | 0 | QSWaveMix* | the channels in OpenAL (and their interface) |
| `Device/WaveBuffers.{h,cpp}` | 0 | fn_100032D0, ACM | decoded waves and their buffer |
| `Device/Sound.h` | 0 | LH_SampleInfo (+.sad) | the record of a sample |
| `Device/*AudioDecoder*`, `Codec/` | 0 | ACM / MPEG decoder | audio::codec (WaveFile, MpegAudio) |
| outside: `src/ECS/AudioQueries.{h,cpp}` | — | — | the queries that read the ECS, test hooks |

Rules:
1. **A single entry point.** Outside `src/Audio` nobody calls OpenAL or `CreateEmitter`/`PlaySound`.
2. **Channels, not entities.** Each channel has its own AL source and there is no `AudioEmitter` in the ECS registry.
3. **A sample is (bank, number).** The key `"<file>.sad/<n>"` is case-insensitive.
4. **Everything runs per turn**, except `HelpDude::UpdateSaySentence` (per frame, 0x5BDE48, with a delay in real ms
   0x5BB554), the camera woosh (`GetTickCount & 3`) and the music thread (120 real ms).
5. **The audio does not include ECS components** (from `ECS/*` only the pure maths `MapCoords.h` and
   `Common/GUtilsDistance.h`, without registry or components). What it needs from the game it asks through
   `audio::GameQueries` (`src/Audio/GameQueries.h`), some `std::function`s registered by `Game.cpp` and
   `ecs::audio_queries` (`src/ECS/AudioQueries.cpp`). A query without an owner returns the value
   of a game without that system: no video, land 0, no camera, no widescreen, alignment 0, no towns, outside the citadel and the
   fight and dance music branches as false (the chant, without a worship site: `chantSite` empty).
6. **The logic is pure and is tested without AL**, with fake sinks.
7. **A single engine.** A single OpenAL device (`src/Audio/Device/Device.{h,cpp}`, `audio::device`): the channels
   (`AlSampleOutput`), the buffers (`WaveBuffers`) and the music (`MusicStream`) request sources and buffers from it; nobody else
   includes `AL/al.h`. A single bank load (`src/Audio/Game/Banks.{h,cpp}`, `audio::banks`), called from `audio::Init`.
   There is no `AudioPlayer`, `AlCheck` or `SoundGroup`; `Locator::audio` holds this engine itself
   (`audio::AudioManager`, [below](#a-single-engine)).
8. **Cyclic counters go in an `enum class Counter`** with the address in the comment, not with addresses
   as keys. The API has no invented default arguments.
9. **One generator and one clock.** The DLL's draws go through `sample_play::Rand` / `Random` and GRand's through
   `guidance::LocalRand`; real milliseconds (GetTickCount), through `device::TickCount` (`audio::TickCount` in the
   API). Nobody in `src/Audio` reads `Locator::rng` for the DLL or `std::chrono` for GetTickCount.

- `AlSampleOutput::StopRamped`: LHSampleStop 0x10012CCA..0x10012CE5: QSWaveMixSetPanRate(mixer, channel, 0, 20) and QSWaveMixSetVolume(.., 0): the volume ramps to 0 in 20 ms while the DLL sleeps 20 ms; then SetPanRate(100) and the flush. (approximated) OpenAL has no pan rate: four steps of 5 ms down to 0 (OpenAL Soft smooths each gain change), on a fader thread (`RampedStops`) while the game goes on, where the original's caller waits.
- `AlSampleOutput::StopRamped`: push 0x14 (0x10012CCA, 0x10012CE3)
- `final`: (openblack, test runs; desvios D12) With OPENBLACK_FIXED_FRAME_MS the game's clocks repeat, but the device plays its samples in real time, and the help / advisor logic waits on them: HelpSystem's isPlaying (fn_0042A280 via 0x5C63CA) and advisorsTalking (HelpSystem+0x10 -> TalkingOrJustStopped 0x5BB749, refreshed by PercentageDone). This output forwards every call to the real one (the sound is still heard) but answers Playing and PlayPositionMs from a timeline of its own on the fixed clock (device::TickCount): a sample sounds for its decoded length divided by its pitch, times its passes. device::Open wraps the real output in it only while fixed_clock::Enabled(); off, nothing changes. ⚠️ approximate a wave's loop section (Sound::loopStart / loopEnd) counts as the whole wave for each pass, and QMixer's end callback lag after the last buffer is not modelled: the tests only need the runs to repeat
- `StopRamped`: LHSampleStop's stop (0x10012CC1..0x10012D09, 0x10012D79..0x10012DCB, 0x10012E6E..): QSWaveMixSetPanRate(20 ms), QSWaveMixSetVolume(0) (a 20 ms ramp to silence), Sleep(20), SetPanRate(100), QSWaveMixFlushChannel. The original's caller waits those 20 ms; openblack's does not (the source fades on the fader thread). Default: Stop.
- `audio::Init`: fn_00428250 (0x426E70): BWSetup AudioSampleMasterVolume -> LHSampleSetMasterVolume (at `if (Locator::config::has_value())`)
- `audio::SetSampleMainVolume`: LHSampleSetMasterVolume ignores more than 127 (0x100150FD)
- `Get3DSoundPos`: fn_00427200, the game's 3D function of LHaudio (LHSampleRegister3DObjectFunction 0x426E6B) as the anim effects ask it: no owner = the camera (0x4272F9); the atmos owner gives 0 (0x42726D); a GameThing gives 0 when not available (0x4272AD), else its Get3DSoundPos; any other Base its own Get3DSoundPos (a SoundTag's: SoundTag::Get3DSoundPos 0x71EC90). nullopt = 0 (nothing plays). The point is guarded (GuardSoundPoint, 0x427349..0x42738E).
- `Variant`: GAudio's working options +0x240 (LH_SamplePlayOptions::ctor at 0x426D63) as the variants fill them: the fields they do not write keep the ctor's defaults (inferred: no other GAudio function writes +0x240 between two calls)
- `audio::PlaySoundEffect`: 0x429DBD..0x429DD7: an unavailable owner or none plays nothing; else its Get3DSoundPos (at `if (OwnerUnavailable(owner) || owner.kind == Owner::Kind::None)`)
- `turnsSinceStateChange`: Living::TurnsSinceStateChange (P_THROWN 0x5166B1 / P_THROWN_VORTEX 0x5166F8); 0 for a thing without one
- The thrown clips' sounds (`Object::CheckSounds` 0x516510, v1.42 exe, 2026-10-26): P_THROWN (399) plays only for a villager (vt +0x2C8, 0x5166BE) whose word +0x90 (the turns since its state changed) is below 15 (0x5166CC), P_THROWN_VORTEX (401) only for a villager (0x516705) below 10 (0x516713); both through `SamplePlayAnimEffect` 0x42A4B0 with the bank at GGlobal +0x3B0. Tests `AnimationSoundsSyntheticTest.AThrownVillagerCriesOnlyEarlyInItsFlightSynthetic` and `AnimationSoundsSyntheticTest.AVillagerThrownOutOfAVortexCriesOnlyInItsFirstTenTurnsSynthetic`

### State of the effects engine in openblack

**Done.** There is a single channel engine: the 16 channels of
`audio::sample_play` (LHSamplePlay), each with its own OpenAL source and **outside the ECS registry**, behind
GAudio's filters (`AudioSystem`) and the public API `src/Audio/Audio.h`. The whole world (hand, trees, rocks,
camera, physics, buildings, boat, piles), the effects CHL, the miracles
(`SpellSounds`, `FireSound`, `HandSpellSeed`, `Gesture`, `HandMagicFX`, `SpellSeed`, `WorshipSpellIcon`, `MagicTeleport`,
`Fireball`, `OneOffSpellSeed`, `FireGraphic`) and the debug panel go through there. **There are no emitters** nor a second
engine ([below](#a-single-engine)): the old `AudioManager`'s emitters and `AudioPlayer` were removed, and
`Locator::audio` holds this engine; the
device is `audio::device` and the banks are `audio::banks`. `src/Audio` ([below](#the-structure-layers-one-generator-one-clock))
is layered, does not include the ECS and has a single DLL generator and a single clock.

### A single engine

Nobody outside `src/Audio/Device` calls OpenAL (`grep` of `AL/al.h|alGen|alSource` in `src`, `test` and `apps`: only
`Device.cpp`).

**The audio service.** `Locator::audio` (upstream's slot) holds an `audio::AudioManagerInterface`, which has the
functions `Audio.h` declares. Its implementation, `audio::AudioManager`, is this engine: its members are defined in
`Game/AudioSystem.cpp` and `Game/GameSfx.cpp`. The free functions of `Audio.h` (`AudioApi.cpp`) each hand their call
to the slot, and an empty slot fails with a clear message. `AudioManagerNoOp` answers as an empty engine, as the base
of a test's fake; it is not a fallback for a missing device (without one the engine runs on `NullSampleOutput`).

**Removed**: the old `AudioManager` (its emitters and its own OpenAL work), `AudioPlayer.{h,cpp}`,
`AudioPlayerInterface.h`, `AlCheck.{h,cpp}` and `SoundGroup.h`. What they still did moves to:

| before | now |
|---|---|
| `AudioPlayer::Initialize` (device, context, OpenAL Soft logging, `AL_INVERSE_DISTANCE_CLAMPED`), `AudioManagerNoOp` if it fails | `audio::device::Open()` from `InitializeEngine` (`LH_AudioSystem::Create`); without a device, `NullSampleOutput` |
| `~AudioManager` (channel sources, buffers, context) | `audio::device::Close()` from `ShutDownServices`, after `audio::Shutdown` and `music::Shutdown` |
| `AudioManager::Update` (finite loops of the channels) | `sample_play::UpdateFrame()` at the start of `audio::UpdateFrame()` (same place in the frame) |
| `AudioManager::UpdateListener` → `AudioPlayer::UpdateListener` | `device::SetListener(camera, 0, forward, up)` in `sample_play::UpdateChannels` (LHListenerUpdate 0x10003850 from fn_004270D0 0x4271EF: QSWaveMixSetListenerPosition 0x1000398E / Orientation 0x100039A7; velocity 0 once, 0x10015C1A) |
| `AudioManager::GetSampleOutput` | `device::Output()` |
| `AudioManager::GetSound`, `CreateSoundGroup`/`AddToSoundGroup`/`GetSoundGroups` (bank list of the panel and of LHAtmos) | `banks::Count/Path/Samples(BankId)`, `BankGroup`, `FindBank` |
| `SoundExists` checking whether the output was `NullSampleOutput` | `device::IsOpen()` |

**Device** (`src/Audio/Device/Device.{h,cpp}`, `audio::device`, layer 0): `Open`, `Close`, `IsOpen`, `Output`,
`SetListener`, `ListenerPosition`; sources (`CreateSource`, `DeleteSource`, `SetSourceBuffer/Pitch/Gain/Looping/Relative/
Position/Distance/Rolloff`, `Play/Stop/PauseSource`, `SourceStatus`, `SourceSampleOffset`, `SourceSecondOffset`,
`SourceBuffersProcessed`, `QueueSourceBuffer`, `UnqueueSourceBuffer`); buffers (`CreateBuffer`, `SetBufferLoopPoints`
with `AL_SOFT_loop_points`, `DeleteBuffer(s)`). It is the only file that includes OpenAL and the only `alCheckCall`. The axis
change (x ↔ z, openblack's world left-handed, OpenAL right-handed) is done only here: before it was repeated in `AudioPlayer`,
`AlSampleOutput` and `MusicStream`. Users: `AlSampleOutput` (16 channels; voices and advisors go through them),
`WaveBuffers` and `MusicStream` (6 tracks).

**Banks** (`src/Audio/Game/Banks.{h,cpp}`, `audio::banks`, layer 1, LHBankRegister 0x10002240): the registry
(`RegisterBank`, `SetBankSampleCount`, `BankSampleCount`, `Bank(SfxBank)` = GAudio+0x3A8 + 4·type from 0x9CB3F8,
`FindBank`, `BankGroup`, `SampleId`, which were in `AudioSystem`) and the loading:
- `banks::LoadAll()`, at the end of `audio::Init` (GAudio ctor 0x426D40 → fn_00429CB0; ⚠️ approximate the 14 ambience ones also here, although the original registers them later, InitAtmos 0x428EF0 → fn_00428F30 from GGame::FinishInitialisation, with nothing playing in between): each .sad
  of `Audio\` in file system order, like the loop there was in `Game.cpp`. Same contents: anim-effect
  tables (`anim_effects::RegisterTables`, 0x10002778..0x100029AB), wave names for the voice table (banks 6, 7,
  10), empty samples skipped (`continue`), music banks (.mpg waves) out.
- The dialogue ones (types 6..10) remain **lazy**: only headers, and `banks::ReadWave` reads the wave on first use
  (0x10011420 → fn_100032D0; previously `wave_buffers::ReadWave`).
- `banks::MusicBankOf(MusicType)` (0x9C9748, GAudio+0x2C + 4·type) registers each music bank on first use (previously in
  `MusicStream.cpp`); `music::GetBank` tells the engine about it; `banks::ReleaseMusicBanks()` in `music::Shutdown`.
- `LHAtmos`: the first `atmos_banks::UpdateBanks` looks up its 14 banks with `FindBank("/<file>.sad")`, reads their
  samples with `banks::Samples` and hands each bank's records to the mixer's `atmos_banks::RegisterBank` with the wall
  clock (fn_10001610; [Banks and channels](banks-and-channels.md#ambience)).
- `AnimEffectTable::Load(path)` only reads the file for an unregistered bank (tools and tests).

**Debug panel** (`src/Debug/Audio.cpp`): the bank list comes from `audio::banks`; the "Channels" tab shows
the device (LHWaveIsInstalled), the 16 channels, the advisors (`AdvisorVoices::GetSpeaker/GetSentence/IsTalking/SentenceTime`,
owner 0x270C) and the 6 LHMusic tracks (state +0x24, bank +0x58, chunk +0x48/+0x4C, volume +0x34 → +0x30).

**Verification** (Land 1, 1800 frames, the five traces `OPENBLACK_AUDIO/SFX/MUSIC/ANIM/TEXT_TRACE`, before and
after): same startup lines (Atmos 15
loops and 400 one-shots, voice table 6974/1922/1328/227, two empty samples, WELCOME_DANCE without a file), the same music
(`intro.sad`, MUSIC_TYPE_SCRIPT_INTRO) and the same event types; the differences are in numbers (anim-effect
draws and a few fewer turns in the second pass, which depends on real time). No OpenAL errors.

- `AlSampleOutput::Play`: 2D (0x100125AB): no position and no distance mapping
- `audio::UpdateFrame`: the options dialog's slider 0x64 applies the sample master at once (fn_00428600, DialogBoxOptions 0x5145A3); LHSampleSetMasterVolume ignores the same value (0x100150F9)

### The structure: layers, one generator, one clock

Thin wrappers without the ECS, a single DLL generator and a single clock, `src/Audio` in layers. The
architecture and the file map are [above](#openblack-audio-architecture).

**1. No ECS in `src/Audio`.** `grep '#include "ECS/' src/Audio` gives nothing. The public signatures do not change
(`AnimationSounds::Fire/PlayFromTable/Update`, `lantern_sounds::SetOn/ProcessTurn/Clear`, `spell_sounds::*`); what
they read from the registry arrives through new `GameQueries` queries, registered by `src/ECS/AudioQueries.cpp`:

| wrapper | read | now |
|---|---|---|
| `AnimationSounds::Fire` (fn_00516510) | `Transform`, `Villager` (life, stage, sex, house), `LivingAction`, `sea_cells` | `animatedThing(entity)` and `audio::SurfaceType` (`surfaceType`) |
| `AnimationSounds` (loading of SmallSounds.SAS, LoadAllAnimations 0x550180) | the clips from the resources (`ecs::ClipId`) | `animationClipName(index)` |
| `lantern_sounds::ProcessTurn` | `StreetLantern`, `Transform`, `Rocks::Height` | `streetLanterns()` (same registry order) |
| `SoundMap` (CameraWeather, Dump) | `weather::atmos`, the hand (`HandSystem` + `Transform`), `sea_cells` | `weatherSmooth(point)`, `handPosition()` (it was already there), `SurfaceType` |
| `SpellSounds` (USESURFACE 0x674661) | `ecs::sea_cells::GetSurfaceType` | `audio::SurfaceType` |

`audio::GetSurfaceType` (SoundMap.h) no longer exists: it duplicated `ecs::sea_cells::GetSurfaceType`. Its only outside
caller, `Particles/Rules/Fireball.cpp` (fn_006A1F90), now calls `ecs::sea_cells::GetSurfaceType`. `CameraWeatherInfo`
moves from `SoundMap.h` to `GameQueries.h`. The hooks that move the camera (`OPENBLACK_AUDIO_TEST_VIEW` / `_ANIM` /
`_LANTERN`) move out of `src/Audio` to `ecs::audio_queries::RunTestHooks(turn)`, which `Game.cpp` calls after
`audio::ProcessTurn` (previously `AnimationSounds::RunTestHooks`; the lantern one counts its calls as it used to count those
of `lantern_sounds::ProcessTurn`).

**2. One DLL generator and one clock.** `LHaudiodllR.dll` has its own CRT linked in: `rand` 0x1001E7EB (seed ·
0x343FD + 0x269EC3, `(seed >> 16) & 0x7FFF`, per-thread seed in `_getptd` 0x100206F1 +0x14) and `srand` 0x1001E7DE,
separate from runblack.exe's. It is used, on the game thread, by: the pitch of LHSamplePlay (0x100127DF, directly), LHAtmos
(0x100016F4, 0x100017DC, 0x10001C2A, 0x10001C5A, 0x10001E74, directly) and `LH_AudioSystem::Rand()` 0x10015740 (the anim-effect
lists, 0x100146CF / 0x100147D8 via `Rand(n)` 0x10015710). Before there were three generators (`SamplePlay`,
`AtmosBanks` and `tags::RandomSample`, all three on top of `Locator::rng`); now:
- `sample_play::Rand()` is that `rand` (or `Backend::rand` in the tests), `SeedRand()` its `srand(time(0))`: on the first
  LHSamplePlay (0x10011497, flag [0x1005645C]), on the first `LH_AudioSystem::Rand()` (0x10015749, [0x10056464]) and when
  registering each bank with ambience records (LHBankRegister 0x10002765 -> fn_10001610 0x10001635). The power-on
  fn_10001840 0x10001843 seeds once at DLL startup (0x10015BDD, with +0x90) and the bank seedings
  overwrite it before LHAtmos draws: it is not emulated. `AtmosBanks` uses it.
- `sample_play::AlternatingRandom()` (was `AudioSystemRand`) = `LH_AudioSystem::Rand()`: `rand() / 2`, plus 0x3FFF every other time (the flag
  [0x1003C124] starts at 1 and toggles on each call, 0x10015768..0x10015783). **Audible change, because of the original**:
  the draws from an anim-effect list alternate between its upper half and its lower half (footsteps, saw, leaf
  rustle...). `Random(n)` = `AlternatingRandom() · n / 32767` (0x10015717..0x1001572B) can no longer give n (the maximum is
  0x7FFE): the ⚠️ approximate of the upper bound is removed.
- `tags::RandomSample` (GRand::LocalRand 0x71ED40) uses `guidance::LocalRand`, the only GRand::LocalRand in `src/Audio`;
  `guidance::LocalRand` / `LocalFloatRand` are `game_random::LocalRand` / `LocalFloatRand` (the single local stream,
  0x6DE570 / 0x6DE590).
- `audio::TickCount()` (Audio.h) and the music thread (`MusicStream`, which had its own copy) read
  `device::TickCount()`, which reads `game_clock::TickCount()` (the GetTickCount of all of openblack, `src/GameClock.h`).

**3. Layers.** `git mv` of the 60 files to `Device/`, `LH/`, `GAudio/` and `Services/`; `Audio.h` and `GameQueries.h`
stay at the root (the path and API of `Audio.h` do not change). The
`#include`s of the whole repository (src, test) use the new path (`"Audio/LH/SamplePlay.h"`); CMake does not change
(`file(GLOB_RECURSE ... *.cpp / *.h)` in `src/CMakeLists.txt` picks up the subfolders; a reconfigure is needed). Later renamed: `GAudio/` → `Game/` and `LH/` → `Engine/`.
`Banks` moves to `GAudio/` (it is the registry that GAudio does, fn_00429CB0, with the tables 0x9CB3F8 / 0x9C9748).

**Verification**: build and the 57 tests. Land 1, 1800 and 5000 frames with the audio, ambience, lantern,
tag and anim-effect traces, and `OPENBLACK_AUDIO_TEST_VIEW="90,0"` + `OPENBLACK_AUDIO_TEST_ANIM=354` (saw), before and after: same startup lines (115 clips with
sound, 201 + 3 rows, 15 loops and 400 ambience one-shots, 12 lanterns with their tag and their height), the same
GSoundMap dump (surface of the camera and of the hand), the camera hook on turn 90 and the same anim-effect
keys (animal footsteps group 18/32/36, villagers group 1 with voices 1/2/3 and the saw 30/31, tree
leaves); the distribution of the samples in each list changes because of the alternation in `LH_AudioSystem::Rand`. No new
errors.

**Audit.** checked in the disassembly 0x10015710, 0x10015740 ([0x10056464],
[0x1003C124]), 0x1001E7EB / 0x1001E7DE, 0x10011483..0x100114A6 ([0x1005645C]), 0x100127DF (direct rand, inside the
"NONE" name branch, which in openblack is always taken), 0x10001635 / 0x10002765, 0x10001843 / 0x10015BDD, 0x100146CF /
0x100147D8 (with 1 sample Rand is not called: the flag does not change, the same in openblack), 0x71D950 (6 outside the map),
0x71ED40 / 0x6DE570, 0x5165BC, 0x5166B1 / 0x5166CC, 0x51675D and 0x73494E. Changes: the comment on the power-on
fn_10001840 (it is from DLL startup, not "the same second"); `streetLanterns` uses `ecs::object::GetHeight` (0x638120)
instead of `Rocks::Height`, which only forwarded it; new test `LibraryRandomState.CrtSequenceAndAlternation` (the CRT sequence
from seed 1, the alternation of 0x10015740 and that `Random(n)` never gives n). In game:
115 clips, 201 + 3 rows, 15 loops / 400 one-shots, the saw with voices 1/2/3 in the view hook, 12 lanterns
with the same heights (4 x 1.29 and 8 x 4.95) and the lantern hook.

### The shared APIs

`src/Audio` uses the shared MapCoords conversions, GUtils distances, turn clock and sky type instead of copies of its own: `map_coords` (`src/3D/MapCoords.h`), `gutils`
(`src/Common/GUtilsDistance.h`), `game_clock` (`src/GameClock.h`), `sky_type` (`src/3D/SkyType.h`) and `ecs::object`
(`src/ECS/ObjectMetrics.h`).

| site | before | now |
|---|---|---|
| `SoundMap` CalculateRadiusPointAndDistance 0x71D834..0x71D855 | `ToMapCoord` (x · 6553.6, its own) | `map_coords::ToFixed` |
| `SoundMap` AtmosMapTypeInfo::Add 0x71D514..0x71D5A4 | `float(x) · 10 / 65536` (two roundings above 2^24) | `map_coords::ToMetres` (fild; fmul 10; fmul 2^-16: one rounding) |
| `SoundMap` UpdateFromMap 0x71D76B / 0x71D790 | `r / 10 · 65536` and `>> 16` | `gutils::ConvertMetersToWholeDistance` (fn_0074DC80, twin of 0x74DCE0) and `map_coords::SignedCellOf` (movsx) |
| `SoundMap` Dump 0x71D990 | `(x >> 16) & 0xFFFF` | `map_coords::CellOf` |
| `ThingMusic` SetPlayPosition 0x4298D9 | `MapCoordsRoundTrip` (in double) | `map_coords::Quantise` (product in float, like the 24-bit FPU of fn_007DEE00); `MapCoordsRoundTrip` is removed |
| `tags::CreateAtMapCoords` 0x71EB60 | only (x, z, height) in metres | additionally `CreateAtMapCoords(const map_coords::MapCoords&, ...)`: x, z via `ToMetres` (0x71EB8A / 0x71EBA6); the metres one remains for points that are already in metres from a MapCoords (`magic::ToMap`), without quantising again |
| `Guidance` Distance (GetDistanceInMetres 0x74CD70 / GetInfo 0x74CD50 / fn_00605CD0) | `std::hypot` of the points **⚠️ approximate** | `gutils::GetDistanceInMetres` (16.16 MapCoords and the 1/√ table): 10 m gives 9.9975 m |
| `SoundTags::CheckDelay` 0x71E766..0x71E79B, `Guidance` heartbeat 0x71C4C7 | `k_MsPerTurn = 100` **🔍 inferred** | `game_clock::MsPerTurn()` ([0xD01A38], GGame::Init 0x54F4A5) |
| `audio::ProcessTurn` | `(skyType, turn)` from `Game.cpp` (`DayNightClock::SkyType()` of the turn) | `ProcessTurn()`: the turn is `game_clock::Turn()` (g_game+0x205A40, 0x54E997) and the sky type `sky_type::Frame()` ([0xFA26BC]: CalculateVolumes reads it at 0x71DDF1 and only DrawSky writes it with fn_0086A2C0, so it is the one from the last drawn frame) |
| `lantern_sounds` (query `streetLanterns`) | — | it already uses `ecs::object::GetHeight` 0x638120: nothing to change |

The real clocks in `src/Audio` were already one (`device::TickCount` = `game_clock::TickCount`); the music
thread's `steady_clock` is its wait (Sleep of 120 ms) and that of the `OPENBLACK_TEST_MUSIC` hook. `GameMusic` has no conversions
of its own: the distance in `ThingMusicInRange` (0x429479..0x4294C1) is 3D on LHPoint, not GUtils,
and `nearestTown` / `town` use the camera's MapCoords and `gutils::GetDistanceInMetres`. The position of a thing (`thingPosition`, `Game.cpp`) is still its float point,
without the rounding of its MapCoords **⚠️ approximate**: it is the owner of all 3D channels and it is not changed here.

**Fidelity change (with its address).** The cell that AddAtmosType compares is its **centre**, not its corner:
fn_00601F40 (UpdateFromMap 0x71D7C2) puts the cell in the high words and in the low ones (GMap+0x28 >> 1) · GMap+0x2C
(g_game+0x59E0 / +0x59E4 / +0x59E8; GMap::Init 0x6014C0, at g_game+0x59B8, sets +0x28 = 8 and +0x2C = +0x30 = 0x2000;
no other code reads or writes them through g_game) = 0x8000, half a cell. The distance to the nearest type and its point
(`nearestX/Z`, the one HeightFade looks at) are those of the centre (x · 10 + 5). In Land 1 the dump does not change (JUNGLE 0.950 with
120 cells: the camera is less than 20 m from the jungle).

**The alignment calls the advisors.** `alignment::ProcessForPlayer` (`src/ECS/Effects/Alignment.cpp`)
does what GAlignment::ProcessForPlayer 0x4141A0 does: for the MyInterfaceStatus player (IsMemberOfThisPlayer 0x64D750;
PLAYER_ONE **🔍 inferred**, like `localPlayerNumber`), every turn and before Process 0x414140, also with nothing pending,
`audio::guidance::UpdateAlignmentRemarks(maximum · pending, alignment, maximum)` (0x4141CD..0x4141D9: vt +0x40 ×
+0xC, unclamped; the alignment from before the change, GetAlignmentValue 0x64D6A0; the maximum, GPlayer+0x64 +0x10). In
Land 1 the advisors are muted by land (PlayNow 0x71AF6F), so nothing new sounds.

**Verification**: build and the 57 tests (new `ThingMusic.PlayPositionQuantised` in `test_game_music` and
`SoundTagTest.MapCoordsTagIsTheMapPoint` in `test_sound_tag_channels`; `GuidanceTest.TownDesireEveryTenTurns` now expects the
GUtils distance). Land 1, 1800 and 5000 frames (`OPENBLACK_ATMOS_TRACE=50`, lanterns, guidance, alignment, tags and
`OPENBLACK_AUDIO_TEST_VIEW="90,0"`, before and after): the same
GSoundMap dump (cells 159/224, 158/222 and 157/221, GRAVEL, 121 cells, JUNGLE 0.950, NIGHT 0.050), the 12 lanterns with their
heights, the view hook on turn 90 and the same startup errors. No new errors.

**Audit.** Checked in the disassembly: UpdateFromMap 0x71D76B (fn_0074DC80: fdiv 10; fmul 65536;
__ftol), 0x71D77A / 0x71D784 (fn_00605490 / fn_00605400 subtract / add r to x and z), 0x71D790 (movsx) and 0x71D7C2;
fn_00601F40 (low words (g_game+0x59E0 >> 1) · g_game+0x59E4 / +0x59E8); GMap::Init 0x6014C0 (+0x28 = 8, +0x2C =
+0x30 = 0x2000), called from GGame::Init 0x54F650 with `lea ecx, [ebx + 0x59B8]`; neither the 7 methods of GMap nor any
other code write those words, and only fn_00601F40 reads them through g_game. AtmosMapTypeInfo::Add 0x71D514..0x71D5A4
(receiver − cell, strict `fcom`, `__ftol` of the cell's metres to +0x8 / +0xA); CalculateRadiusPointAndDistance
0x71D834..0x71D855; CalculateVolumes 0x71DDF1 (`fld [0xFA26BC]`); CheckDelay 0x71E766 and the heartbeat 0x71C4C7 (`fimul`
of [0xD01A38], which GGame::Init 0x54F4A5 sets to 0x64); EndTurn 0x54E997 (`cmp [+0x205A40], 5; jbe`); ProcessForPlayer
0x4141A0..0x4141E1; HelpSpritesAlignmentProcess 0x71CEDA; GPlayer::GetMaxAlignmentChangePerGameTurn 0x64B670 = `mov
eax, [ecx+0x64]; fld [eax+0x10]` (so the maximum that the guidance reads is the same as that of vt +0x40: it is no longer 🔍 inferred);
GetInfo 0x74CD50 / 0x74CD70 / fn_00605CD0 (used by the towns' desire 0x71B19D and 0x71B2D8 after MapCoords(LHPoint)
of the camera 0x71B14A / 0x71B289); SoundTag::Create(MapCoords) 0x71EB71..0x71EBB2; SetPlayPosition 0x4298D9. Everything
adds up. Fixed: the CheckDelay comment still said "100 ms 🔍 inferred". Added the trace line
`(openblack) Sound map nearest:` (with `OPENBLACK_ATMOS_TRACE`): distance and point of the nearest cell of each type
present. In Land 1 (with `OPENBLACK_AUDIO_TEST_VIEW="90,0"` and
`"90,0,80"`) the points end in 5 (JUNGLE 3.905 @ (1585, 2225), COUNTRYSIDE 74.224 @ (1635, 2175)): they are cell
centres; the volumes and the startup errors do not change.

- `nearestTown`: fn_00602160(camera, maxDistance): the nearest town of every player and the neutral one (GetNextPlayerAndNeutral 0x550980, towns from player +0xA50 by +0x75C) closer than maxDistance (strictly, 0x60219C; the distance is fn_00605CD0 = GetDistanceInMetres 0x74CD70, the same as MusicTown::distance) that has +0x9A4 set or fn_00741020 (a town centre among its buildings +0x754, or an entry of +0x9A8 whose GetComputerSeen is 0xC). Game: ecs::audio_queries (ecs::map_cells::GetNearestTownWithCentre from the camera's MapCoords, the town's Tribe; (aproximado) without fn_00741020's test, which map_cells does not port). Unset: nullopt (the generic music of the alignment, fn_00427460 0x427579).

### State in openblack

Whoever touches `src/Audio` reads this page first.

**Done**
- A single engine (`src/Audio`, API `Audio.h`, layers Device / Engine / Game / Services) that replaces the old AudioManager:
  QMixer's 16 channels, volume laws, anim-effects, 6-track music on its thread, banks, SoundTags, ambience,
  SoundMap, voices and advisors, Guidance, night voices, audio and script CHL (dialogue, widescreen, camera
  locks), miracles, interface and hand; the audio reads the game only through `GameQueries` (`src/ECS/AudioQueries.cpp`).
- Weather and alignment in ambience and music, the citadel interior, videos (`videoPlaying`,
  ambience silenced and music continuing underneath; the 11 sounds of fall.bik and `MusicStop(1)` at 43.9 s, checked).
- Tribe music per town (with fn_00741020), Guidance town search, town desires
  (`desireTowns`, `townResourceNeeds` on top of `ecs::town_desire`) and temple desires (`worshipSites` on top of `worship::citadel`
  / `worship::site`), single local randomness (`game_random`), the 24-bit FPU rule.
- The two main volumes, effects and music, kept for the computer in `Profiles/Setup.txt` (`machine_setup`,
  `audio::main_volumes`) as the original keeps them in its setup's registry key; tests `MachineSetup.*`
  ([Banks and channels](banks-and-channels.md#master-volumes-focus-and-reset-1)). The original reads the volumes each
  time an options box shows and sets its sliders from them, and openblack does the same (`GameMenu::ShowOptionsBox`).
- The volcano's G_Volcano_02 tag of `LandscapeVortex::CallVirtualFunctionsForCreation` 0x5FEE30 (the vortex's side is in
  [Creation](../world/vortex.md#creation); nothing removes the
  tag, it goes with its thing): `ecs::vortex::Create`
  makes it through `audio::vortex_sound::CreationTag` since the audio leftovers work (formerly issue 22), after the vortex's transform and component
  (tests `VortexSound.OnlyTheVolcanoTagsItself`, `VortexDeletion.OnlyTheVolcanoTagsItselfWhenMade`); the tribal power of `DoPostCastThings` 0x72930A (27 + tribe, mode 2, 2D,
  SpellDialogue, when `GetTribalPowerTribe` 0x5FB710 finds one; `SpellSeed.cpp`, test in test_magic_tables.cpp) and the
  storms' thunder (fn_0071E920 through the callback GAudio::InitSFX stores at 0x429CC9, read by GWeather::Update
  0x83FC62; `tags::Thunder`, tests in test_sound_tag_channels.cpp).
- drlibs is out of the build: its vcpkg.json entry and the CMake `find_path` are gone, since nothing used it after
  our own decoders replaced it.

What is left is in [Pending](#pending).

**How to test** (see also [Test hooks](#test-hooks))
- The whole log goes to `openblack.log`.
- Desires: in Land 1 the guide spirit is silent on purpose (PlayNow 0x71AF6F); `-s Land2.txt OPENBLACK_GUIDANCE_TRACE=1
  OPENBLACK_CAMERA_FLY="2187,120,2200,2187,20,2260"` (town 94). Temple: `OPENBLACK_TEST_WORSHIP_PLAYER=1
  OPENBLACK_TEST_WORSHIP="1,0.5" OPENBLACK_CAMERA_FLY="2540,150,1740,2540,100,1800"`.
- Video: `OPENBLACK_TEST_VIDEO=fall OPENBLACK_TEST_MUSIC=54@5 OPENBLACK_SFX_TRACE=1 OPENBLACK_MUSIC_TRACE=1`; the test
  music must start after the map is loaded (ClearMap cuts it with LHMusicStop(0) 0x426CD3).

- `int`: HelpSystem+0x45F8 ? +0x45F4 : 0 (GGuidance::PlayNow 0x71AF99..0x71AFB1): the help switch (SET_HELP_SYSTEM 0x6FC03D; HelpSystem::Reset 0x5C55FC sets 1) and the profile's HELP_LEVEL (fn_005C6CF0, 3 when the profile has none: 0x5C6DB6). Unset: 3.

### Test hooks

| Hook | What it does |
|---|---|
| `OPENBLACK_MUSIC_TRACE=1` | One `music:` line per pass of the thread with each busy channel: bank, state, cur/target/sad, chunk, loops, queue, QMixer volume, gain and decoded samples. It also writes `game music: Music Playing=…` each time it changes |
| `OPENBLACK_TEST_MUSIC="<type>[,<type>@<s>][,stop@<s>][,cut@<s>]"` | The first track is played like the trailer (vol 127, no sync or fade, 2D). Each of the following ones, at `s` seconds, with sync and fade, like `ProcessCitadelMusic` (to hear a synchronised change). `stop` = `LHMusicStop(1)`, `cut` = `LHMusicStop(0)`. It is a hook, not a behaviour of the original |
| `OPENBLACK_TEST_MUSIC_VOLUME=<0..127>` | The music master at startup |
| `OPENBLACK_TEST_ALIGNMENT_MUSIC=<turn>` | From that turn, every turn, `ENABLE_DISABLE_ALIGNMENT_MUSIC(1)` and without the script widescreen filter (which Land 1 leaves on in openblack), to hear the alignment music. Not from the original |
| `OPENBLACK_TEST_SCRIPT_MUSIC="<type>[@<turn>]"` | A script START_MUSIC on that turn (30 by default) |
| `OPENBLACK_AUDIO_TRACE=1` | Each channel start, steal, stop and cutoff (`Sample play:`), each buffer created (`Wave buffer … N made`), the changes of the script widescreen, the anim-effects rejected by distance (`Anim effect: bank/n (wave) too far`) |
| `OPENBLACK_ANIM_TRACE=1` | The sounds of the clips and of the trees (`Animation sound: clip … -> editor.sad/n`, `key … -> editor.sad/n`, `no row`, `banter n too far`), with the same format as the old trace |
| `OPENBLACK_SOUND_TAG_TRACE=1` | Each tag created, deleted, released or with a delay, and every 50 turns the channel of each thing tag |
| `OPENBLACK_SFX_TRACE=1` | One `SFX:` line per call to `GAudio::PlaySoundEffect` (and to the tags), to `PlayAnimationEffect` and to `StopSoundEffect`: bank/sample (wave), 2D/3D, track, point, mode and loops it starts with, pitch, owner and what happened (channel, `culled`, `filtered (reason)`); and the script's `PLAY_SOUND_EFFECT(...)` |
| `OPENBLACK_AUDIO_TEST_VIEW="turn,n[,distance]"` / `OPENBLACK_AUDIO_TEST_ANIM=<clip>` | On that turn the camera looks at villager n from that distance (4), and all villagers play that clip in a loop (437 yawn, 354 saw, 369 sitting). In `src/ECS/AudioQueries.cpp` (`ecs::audio_queries::RunTestHooks`) |
| `OPENBLACK_AUDIO_TEST_LANTERN="turn[,distance]"` | On that turn (counted by the calls to `RunTestHooks`) the camera looks at the tip of the first street lantern from that distance (3) |
| `OPENBLACK_AUDIO_TEST_CITADEL="<enter>[,<leave>]"` | On those hook turns it enters / leaves the temple interior, like `ENTER_EXIT_CITADEL(1)` / `(0)`: the citadel music and filters. The hook turns are the world's (`RunTestHooks`, one per turn) and, while the world is paused inside the temple, the temple's own 100 ms turns (`RunCitadelTestHook` from `Game::ProcessTempleTurn`). Not from the original |
| `OPENBLACK_AUDIO_TEST_NO_WIDESCREEN=1` | The audio does not see the script widescreen (the Land 1 intro has it until a click), to compare without that filter. Not from the original |
| `OPENBLACK_TEST_SAMPLE_VOLUME=<0..127>` | The effects master at startup |
| "Channels" tab of the audio panel (`SampleChannels`, `src/Debug/Audio.cpp`) | Effects master (slider: master × (1/127) when it opens, `ftol(slider·127)` back, as `DialogBoxOptions::ControlCallback` 0x5145A3), LHWaveIsActive, live/created buffers and the 16 channels (sample, bank, owner, priority, volume, pitch, 3D/track/ambience, playing) |
| `OPENBLACK_TEXT_TRACE=1` | Each RUN_TEXT/TEMP_TEXT text in the log (`|` for each line break) |
| `OPENBLACK_AUDIO_TRACE=1` (voices) | `Advisor: dude n says HelpSprites m (s)` when each advisor sentence starts; `Wave of … read from <bank>` when a wave is read from a dialogue bank |
| `OPENBLACK_TEST_TEXT_CLICK=1` | Every turn, if a text is waiting for the click, it does the left click (`HelpSystem::ProcessInterface(true)`); see map-loading.md |
| `OPENBLACK_TEST_BW_ROOT=<install>` | For the tests with data: `test_audio_tables`, `test_music_bank`, `test_music_stream`, `test_game_music`, `test_voice_table`, `test_help_system`, `test_sample_play`, `test_anim_effects`, `test_sound_tag_channels`, `test_script_sound`, `test_voices`, `test_spooky_voices` |
| `OPENBLACK_GUIDANCE_TRACE=1` | `Guidance:` for each Init, PlaySample (type, text or sample, 2D/3D, bank/sample, channel) and HelpSpiritSay (text, type, script, started or not); `SpookyVoices:` on Init and PlaySpooky |
| `OPENBLACK_TEST_GUIDANCE_SAY=<turn>:<HELP_TEXT>` | On that turn, `SpiritSay(text, 32)`: tests the `MultiHelpJustTalkWithText` script (not from the original) |
| "Music" debug window | Master, 6 channels, MUSIC_TYPE player, GameMusic and GScript state, list of objects with music |

Example: `OPENBLACK_TEST_MUSIC="3,1@20" OPENBLACK_MUSIC_TRACE=1` starts good.sad and after 20 s switches to evil.sad
synchronised (same chunk).

## Pending

Items about one topic are on that topic's page: [World sounds](world-sounds.md#pending), [Banks and channels](banks-and-channels.md#pending), [Voices and texts](voices.md#pending), [Creature and interface sounds](creature-and-interface-sounds.md#pending), [Music](music.md#pending), [The temple interior](../interface/temple-interior.md#pending), [Magic and weather sounds](magic-and-weather-sounds.md#pending).

**Not yet in openblack**
- Save and load: `GAudio::Save` 0x428310 / `Load` 0x428480, `ThingMusicInfo::Save` 0x429950 / `Load` 0x429AE0,
  `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0.
- The `OPENBLACK_SFX_TRACE` trace does not say the address of the original site.

**Checks in the game**
- The real 10 s minimise next to a town, to confirm with the user: since issue #9 the game loop waits while
  minimised, so the ambience should fall silent with the rest and nothing should move until the restore.
- `LHSampleIsPlaying(info)` with the audio off: openblack still looks at the handle's start, a choice made while its
  game kept running minimised. With the loop now waiting, the original's 0 (0x1001407A) can be taken; to measure.

**Known approximations** (all with their comment in the code)
- `audio::TickCount` = milliseconds of the process clock like `GetTickCount`. The pile (fn_0066D1A0) reads it twice
  as the original (0x66D1D7 for the food sample, then 0x66D1E7 for the wood one; `PlayPileSound` through
  `pot_resource::PileSoundSample(type, amount, foodTicks, woodTicks)` since the audio leftovers work (formerly issue 22)). Under the checks' fixed clock
  both readings are the same.
- `SOUND_EXISTS` = OpenAL device **⚠️ approximate**; banks outside 1..10 and texts outside the voice table neither sound
  nor stop **⚠️ approximate**.
- GAudio's working options (+0x240) with the ctor's values in each variant (nobody else writes them: 🔍 inferred).
- `OwnerUnavailable` = `GameQueries::thingPosition` empty (the entity is not valid or has no Transform).
- Null g_game / HelpSystem (0x429E37..0x429E4F) are not modelled: openblack always has them.
- `LHSampleIsPlaying(info)` with the audio off (0x1001407A) and the fixed axes of `LHSampleSet3DPosition` (0x10013BCC):
  see the audit of the core.
- Stereo in 3D: OpenAL does not spatialise stereo buffers.
- In the music and text code: `MusicThing` (the script's object table) and `CHAR2WCHAR`; the script camera always taken
  and the citadel as the temple interior; `GameThing::IsAvailable` and the MapCoords in float; `LHWaveIsActive`;
  openblack's turn as `g_game+0x205A40`; the render camera as GGame's; out-of-range types and alignments; group 0;
  silence on a decoding failure; the empty queue in OpenAL; the unit of `lStart` and of the play position; the
  notification inside `FlushChannel`; the marker clock in double; the marker without a label; the scaled clock in real
  ms; the narrator number as `_wtoi`.

**Open questions**
- `qmixer::PolarRelative` azimuth: "azimuth = y < 0.0f ? 180.0f : 0.0f (0x1001239C / 0x1001238F)", "azimuth = x > 0.0f ?
  90.0f : 270.0f (0x100123C9 / 0x100123E9, x is not 0 here)", "90 − atan(y/x)" (0x10012414..0x1001242E), "90 +
  atan(|y|/x)" (0x10012444..0x10012470), "270 − atan(|y|/|x|)" (0x1001249B..), "270 + atan(y/|x|)" (0x100124EE..). Also
  "elevation = z < 0.0f ? -90.0f : 90.0f (0x1001233F / 0x10012349)", otherwise atan(z / √(x² + y²)), and range = √(z² +
  x² + y²) (0x100122D5..0x100122EB). The branch conditions are not in the notes. QMIX_POLAR: azimuth +0x20, range +0x24,
  elevation +0x28.
- `.github/workflows/packaging.yml` (the AUR job) still builds `dr_libs-git`, because the external AUR
  `openblack-git` PKGBUILD lists it in makedepends; it goes when that PKGBUILD drops it.
