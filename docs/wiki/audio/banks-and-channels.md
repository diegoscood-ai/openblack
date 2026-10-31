# Banks and channels

## Summary

Every sound effect and spoken line of the game is a wave stored in a bank file. The game opens 11 effect and dialogue
banks at start-up, plus one per creature species, the ambience banks and a few others. It reads a wave only the
first time it is played and keeps it in a cache. All these sounds share 16 mixer channels. Each sound has a priority:
when every channel is busy, a new sound takes the channel of a less important one, so help dialogue is never lost
and ambience gives way first. A sound can play once, loop for ever, or repeat a part of itself a set number of times.
Two main volumes (effects and music) are kept in the registry and set on the options screen. Minimising the window cuts
the sounds and the music. The ambience (birds, sea, wind) follows what lies around the camera.

openblack decodes every wave with its own decoders (PCM, MS-ADPCM, MPEG layer II and III), bit for bit against the
libraries they replaced, and reproduces the channels, volumes and ambience in its effects core: the ambience mixer
replays, event for event, three scenarios recorded from the game's own audio library, and the two main volumes are
kept in `Profiles/Setup.txt` as the original keeps them in the registry. The debug bar's Audio Player and Audio banks
windows play any sample.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Banks and formats

#### Table of effect and dialogue banks

**✅ faithful.** The bank type is `AUDIO_SFX_BANK_TYPE`. Its paths are in table `0x9CB3F8` (11 `const char*`), and each
bank is stored at `GAudio+0x3A8 + 4·type`. They are all registered at start-up with `LHBankRegister(path, 0)`. The
paths have no language folder: the language is decided by whatever is installed.

| type | bank | path | path string | typical priority (data) |
|---|---|---|---|---|
| 0 | — | null | | |
| 1 | InGame | `audio/sfx/game/ingame.sad` | `0x9CB540` | 0..2000 |
| 2 | Editor | `audio/sfx/game/editor.sad` | `0x9CB524` | 0..300 |
| 3 | Spells | `audio/sfx/game/spells.sad` | `0x9CB508` | 0..6000 |
| 4 | Creature | `audio/sfx/creature/creature.sad` | `0x9CB4E8` | 200/400 |
| 5 | ScriptSfx | `audio/sfx/script/scriptsfx.sad` | `0x9CB4C8` | 200..9999 |
| 6 | HelpSprites | `audio/dialogue/HelpSprites.sad` | `0x9CB4A8` | 9999 |
| 7 | Villagers | `audio/dialogue/Villagers.sad` (on disk `villagers.sad`) | `0x9CB488` | 9999 |
| 8 | VillagersBanter | `audio/dialogue/VillagersBanter.sad` | `0x9CB464` | 2 |
| 9 | SpellDialogue | `audio/dialogue/SpellDialogue.sad` | `0x9CB440` | 2000 |
| 10 | Guidance | `audio/dialogue/Guidance.sad` | `0x9CB424` | 20 |

Entry 84 of table `0x9C9748` (`creatureendsequence.sad`) is at `0x9C99E8`.

How the 11 are registered:

1. GAudio's fn_00429CB0 first clears `GAudio+0x3A8[0..10]` (`0x429CB5..0x429CC0`).
2. It calls fn_0042A350 (at `0x429CC4`), which loops over types 0..10 (`0x42A356..0x42A362`).
3. For each type, fn_0042A390 sets `GAudio+0x3A8 + 4 * type = LHBankRegister(path, 0)` only if the path is not null and
   the slot is still empty (`0x42A39D..0x42A3BA`). It opens the path as it is, with no language folder
   (`0x42A3A3..0x42A3B4`).

The game also registers other banks:

- One **per creature species**: `audio\sfx\creature\%s.sad`, in `LH3DCreature::LoadBinary` 0x4EBD81 →
  `GAudio::RegisterBank` fn_00428620 (+0x5288).
- The 13 ambience ones (`Audio\SFX\Atmos`).
- The one for a video (`PlayFullScreenMovie` 0x54D920) and `audio\music\intro\trailer.sad` (`0x6427CE`).
- The HelpDude one (fn_005BB1B0, `0x5BB1E6`, **🔍 inferred**).

#### Wave loading and cache

**✅ faithful.**

- **Registration.** `LHBankRegister(path, inMemory)` `0x10002240`. The game **always** passes `inMemory = 0`
  (`0x426EEE`, `0x428F30`, `0x42A3B1`, `0x4EBDB9`, `0x54D991`, `0x5BB1DE`, `0x6427C1`). On registration only the
  headers are read, and the file stays open. `LHMusicGetTotalGroups` (sys+0x40) is the maximum of the u16 +0x118 of
  the music banks.
- **Loading.** Samples are **not streamed**. They are loaded whole on first use (`0x10011420` → fn_100032D0) into a
  **FIFO cache** (list `0x10042F80`) with a budget of RAM/8 (`[0x100383B8]`). The victim is the oldest wave that no
  channel is using.
  - Only `LHSamplePlay` loads a wave into it (fn_100032D0, called once, at 0x10011DB8); the cache is emptied
    (fn_100031F0) by the audio system's `Create` 0x100155D1, `Shutdown` 0x100156F4 and destructor 0x100153BF, never
    in between. The music banks are played by LHMusic and do not go through it.
  - It counts each wave's data size (+0x10C) in [0x10042F8C] against the budget sys+0xD4 (0x100031E6;
    0x1000331B..0x1000332C): within it, the wave is allocated and read; past it, the oldest waves no channel is playing
    (status +0x8C ≠ 1) are freed one by one until it fits (0x100033ED..0x100034F7), and with none to free the play fails
    and is logged (0x100034FD).
  - The budget is `dwTotalPhys >> 3` of `GlobalMemoryStatus` (GAudio's ctor, 0x426DDC). runblack.exe is not large
    address aware (characteristics 0x10F, no 0x20), so Windows caps what it reports at 2 GB: measured on 2026-10-21 in a
    32-bit process without that flag, `dwTotalPhys` = 0x7FFFFFFF, so the budget is 268,435,455 bytes on any machine with
    2 GB or more. All the waves of the 33 sample banks of `Audio\` (clones once) come to fewer bytes than that
    (test `AudioTables.EveryWaveOfTheSampleBanksFitsTheWaveCache`), so in the shipped game **no wave is ever dropped**.
- **Conversion.** QMixer converts ADPCM and MPEG with ACM. With opts+0x164 (`keepPcm`), the DLL leaves the PCM in
  SampleInfo+0x80/+0x84 (fn_10010910, `0x10011CB3`/`0x10011E25`), and HelpDude copies it for the lip-sync
  (`0x5BB57B..0x5BB5C9`).

### Channels, priorities and loops

**✅ faithful.** There are **16 channels**, shared by 2D, 3D, ambience and dialogue. GAudio's ctor writes the 16
channels at `0x426DF5`. The game never sets the registry override "MaxSamp". A new sample steals the lowest-priority
channel if that priority is lower than its own. Because of the priorities in the data, help dialogue is never lost
and ambience yields first. There is **no fade API**: each caller does its own.

#### The channel record

| offset | field |
|---|---|
| +0x00 | non-zero on an ambience (atmos) channel |
| +0x04 | bank |
| +0x14 | the fields the start applied (see below) |
| +0x18 | owner |
| +0x1C | sample |
| +0x28 | clone group (.sad +0x118) |
| +0x70 | the priority it started with (.sad +0x240) |
| +0x8C | in use; the end callback `0x100108C0` clears it |

#### Play options and the caller mask

`LH_SamplePlayOptions` (ctor `0x10010E90`):

- +0x04 bank, +0x08 is3D, +0x0C track, +0x10 the GAudio variants' 6th argument;
- +0x1C caller mask, +0x20 owner, +0x24 sample, +0x28 volume, +0x2C a 2D sample's pan (default 90, see below);
- +0x30 point, +0x3C offset added to it (`0x10012159`), +0x48 pitch, +0x4C loops, +0x50 mode;
- +0x54 / +0x58 / +0x5C min / max / scale (defaults 1, 9999, 0.3), +0x164 keep the PCM.

A 2D start (+0x08 = 0) is placed by its pan: the channel is configured with no distance law
(`QSWaveMixConfigureChannel` 0x1020000 / 0x1010000 at `0x100125ED` / `0x10012617`, then `QSWaveMixEnableChannel`
with none of the 3D range bits), and the pan p = +0x2C, taken unsigned and kept to 180, becomes the azimuth
p − 90 from 90 up and p + 270 below (`0x10012639`..`0x1001272A`), sent by `QSWaveMixSetPolarPosition` at range 1 and
elevation 0. The .sad's own pan overrides it with flag 0x2 (the dword at the record's +0x24C; openblack reads the whole u32, `Sound::pan`: the four samples that carry the flag, InGame.sad 41 and 42 in both copies, hold 90 in it). ✅ The library sets
QMixer's left-handed coordinates (`QMIX_CONFIG` flags 8, `0x1001590E`), and QMixer's listener frame takes x as up × front
(`0x1802D210` / `0x1802D0E0`), the listener's right: so 0 is heard on the left, 90 ahead and 180 on the right (🔍 the
last step, QSound's renderer sending +x to the right speaker, is not traced; every 3D sound in the game would be
mirrored otherwise). `LHSampleSetPan` `0x100131B0` does the same with range 0, and the exe never calls it. openblack:
`qmixer::PanAzimuth` / `PanDirection`, and `sample_play::Start` places a 2D source on the unit circle around the
listener there (AL's panning law, not QSound's: the side and the direction match, not the exact gains); ahead, the pan
every 2D sample but the spooky voices' has, it stays at the listener as before.

+0x10 makes LHSamplePlay use 3D flags 3 instead of 0x11 (`0x10012050`); its meaning is unknown. The 3D cutoff of
`0x429E6F` measures the distance to point + offset.

Caller mask +0x1C: 0x1 pitch, 0x20 volume, 0x40 loops, 0x80 min, 0x100 max, 0x200 scale, 0x400 mode. A .sad field
applies only where its +0x244 flag is set and the caller's bit is clear (`0x100119D4..0x10011B84`). The mode follows
this rule too (`0x10011B5C`).

#### Choosing a channel

The channel is chosen in LHSamplePlay `0x100113B0` (allocation `0x10011020`, then the start `0x10011420`). It depends
on the play mode: options +0x50, 3 by default, or the .sad's +0x274 when its flag 0x400 is set.

- **Mode 1:** a free channel.
- **Mode 2:** nothing at all, not even a new position, while a channel of the same bank and owner is playing the same
  sample, or one of the same clone group (> 0) is playing. Otherwise a free channel.
- **Mode 3:** the channel of the same bank, owner and sample (restarted); else one of the same clone group > 0
  (restarted); else a free channel.
- **No free channel** (any mode): the channel with the lowest priority, if it is lower than the sample's, is restarted.
  A channel at priority 0 is taken at once (`0x100112CA`). Otherwise the sample does not play.

The "restarted" flag is passed on to the start (`0x10011350`). The channel gets the sample's priority and clone group
at `0x10012AAA`.

#### The fields the start applies

**The channel keeps only the fields the start applied** (the ambience mixer work, formerly issue 21).

- The start's local `[esp+0x10]` is 0 at entry (`0x1001144B`). Each of the eleven tests ORs its bit into it when it
  takes the .sad's field (`0x100119D4..0x10011B84`): 0x4 / 0x8 / 0x10 the position's x / y / z (.sad +0x250 / +0x254 /
  +0x258), 0x20 volume (low u16 of +0x25C), 0x2 pan (+0x24C), 0x1 pitch (+0x260), 0x80 / 0x100 / 0x200 min / max /
  scale, 0x40 loops (+0x248), 0x400 mode (+0x274).
- The whole block is skipped, so nothing is applied, when the options' name (+0xC4) is not "NONE" (strcmp at
  `0x100119BF`). Every game caller leaves the ctor's "NONE".
- That value, not the .sad's flags, is stored in the channel's +0x14 (`0x100129C4` / `0x100129E5`). It is what the
  setters test: `LHSampleSetVolume` (by channel `0x100134CD`, by triple `0x100133BB`) bit 0x20, `LHSampleSetPitch`
  (`0x1001358D`, `0x1001368F`) bit 0x1, `LHSampleSetPan` (`0x10013138`, `0x100132CE`) bit 0x2,
  `LHSampleSet3DPosition` bits 4 / 8 / 0x10 (`0x100137B5`, `0x10013BCC`).
- So a caller that gives its own volume (mask 0x20) can change it later even when the .sad has a volume. The ambience
  mixer starts its loops and loose samples with mask 0x20 (`0x10001A18`, `0x10001BD7`). In the shipped Atmos banks 14
  of the 15 loops and 378 of the 400 loose samples have the .sad's flag 0x20, and their fades are heard.

#### The start pitch and the DLL's random numbers

Start pitch (`0x1001278B..0x1001283B`, unsigned integers):

1. p = the pitch (0 → 100). The pitch is the .sad's when its flag 0x1 is set, otherwise the options' +0x48.
2. d = deviation · p / 100.
3. p = p − d + rand · 2d / 32767 (the DLL's rand, 0..32767); 0 → 100 again.

The DLL's generator:

- `srand` 0x1001E7DE: the thread's seed = time(0) (`0x1001E80D`).
- `LH_AudioSystem::Rand()` (`0x10015740..0x10015759`, `0x10015763..0x10015783`): cdq / sub / sar 1 (rand() is never
  negative: rand() >> 1), + 0x3FFF while the flag is set, then the flag flips.
- `LH_AudioSystem::Rand(n)` `0x10015710`: Rand() * n / 32767 (unsigned: the magic 0x20005, `0x10015717..0x1001572B`).

#### 2D and 3D channels, distance and volume

- **3D**: `QSWaveMixEnableChannel(…, 0x20, f | 0x100)` (`QSWaveMixEnableChannel` 0x20) with the 3D flags (`0x10012050`; flags 0x103 / 0x111,
  `0x10012065`), and `SetDistanceMapping {min, max, scale}`. A **2D** channel (`0x100125AB`) has no position and no
  distance mapping.
- **Distance law**: d ≤ min → 1; d > max → 0; otherwise `min / ((d − min)·scale + min)` (`0x1802CE50`). QMixer's law
  (`0x1800ACDF..0x1800AE1A`) has two more flags, 0x800 "clamp at max" and 0x1000 "linear"; neither LHSamplePlay nor
  LHMusic uses them. Beyond max, QMixer mutes the channel but keeps it playing (`0x1800ADDF`).
- **Volume**: `floor(m·v/127)·258/32767` (`0x100133C1..0x100133E3`, and `0x1001511D..0x10015143` for the master).
  `QSWaveMixSetVolume` takes 0..32766 and QMixer stores vol / 32767 (`0x18007AE5` / `0x18007AE8`). The division by 127
  is the magic 0x2040811 (× 129 × 2 = 258).
- **Tracking**: `LHSampleUpdate3DChannels` 0x10014310 only moves the 3D channels with track. It stops the channel if
  its owner is gone (`0x1001439D`), and then also sets the channel's owner to 0 (`0x100143A2`). It stops it if the
  distance is **≥** the maximum (`0x100143AB`, the stop at `0x100143BC`).

#### The 3D position sent to QMixer

The position relative to the listener (`0x100122BC..0x10012522`) is computed on the game's thread, so with the FPU at
24 bits (fn_007DEE00: fninit and `and cw, 0xFCFF` at `0x7DEE0D`). Every fadd / fsub / fmul / fdiv / fsqrt rounds to a
float's 24-bit mantissa, while the loads of the doubles (and fcomp against them) are exact and fpatan keeps the full
precision. QMixer's half (`0x1800AA85..0x1800AB0B`) runs at 53 bits on the timer thread of `QSWaveMixPump`.

- The doubles of LHaudiodllR: 0 (`0x10030460`), 180 (`0x10030458`), 90 (`0x10030448`), 270 (`0x10030440`) and
  0.31847133757961782 = 1 / 3.14 (`0x10030450`, LHaudio's "1 / pi"; the old float reading of the disassembler showed it
  as 0.318471). Each angle is atan(..) * 180 * that double, in that order.
- Steps read: fld [esp+0x94] (fstp qword [esp+0x18], `0x100122C3`: exact); `0x10012304`; the constants 0xC2B40000 /
  0x42B40000 (`0x1001233F` / `0x10012349`); `0x10012353..0x10012369`: fadd, fsqrt, fdivr (24 bits each), fpatan, the
  two fmul.
- The QMIX_POLAR it sends (azimuth +0x20, range +0x24, elevation +0x28) holds floats.

#### Loops

- `QSWaveMixPlayEx(…, 0x421, wave, loops, &p)` at `0x10012949`, with `lStart..lEnd` if both are ≠ −1 (+0x138/+0x13C).
- Loops: 0 = once, −1 = forever, N = N repetitions of the span and then it continues to the end. Whether that is N or
  N+1 passes remains **🔍 inferred**.
- Examples: G_VillageBell InGame 30 (5 loops, 0..27400 of 57855), G_PickUpFood 44 (−1, 47743..110078), the editor's
  doves 342/343 (5), jungle (3..5), swamp (2..6), country bird15 (6).
- **`ReleaseLoop`** (`LHSampleReleaseLoop` 0x10012F20 = `QSWaveMixStopChannel(0x1000)` → QMixer `0x18007520`): acts
  on the **first** matching channel that is in use. It sets the remaining loops to 0 and the current pass finishes. If
  that first channel is inactive, it returns 0 without searching further. With the audio off it does nothing
  (`0x10012F2A`).

#### Stopping

- **`LHSampleStop`** (`0x10012C50` by sample or owner, `0x10012DF0` by channel, and the cutoff in
  `LHSampleUpdate3DChannels` 0x1001439D/`0x100143BC`): `QSWaveMixSetPanRate(20 ms)`, `SetVolume(0)`, `Sleep(20)`,
  `SetPanRate(100)` and `Flush`. That is a 20 ms fade, and the game waits those 20 ms.
- **By sample** (`LHSampleStop(bank, owner, sample)`, sample ≠ 0, `0x10012C50..0x10012DD1`): only the **first**
  channel of (bank, owner, sample) stops, with the 20 ms ramp to 0 (`0x10012D88..0x10012DCB`). While the audio is off
  it does nothing, unless the channel is an atmos one (`0x10012D5B`).
- **By owner** (sample 0): every channel of the bank and owner, each with the 20 ms ramp (`0x10012C78..0x10012D1A`).
  While switched off, the first channel met that is not an atmos one ends the whole stop (`0x10012CA8..0x10012CB2`).
- **By channel** (`LHSampleStop(LH_SampleInfo*)` `0x10012DF0`): it stops the first channel of that channel's (bank,
  owner, sample). While the audio is off it does nothing, except on an atmos channel (`0x10012E18..0x10012E22`).
- **`LHSampleStopAll`** `0x10012BF0` and resetting a channel cut abruptly: StopAll flushes at once (`0x10012C2E`). It
  does not touch the ambience channels (+0x00 ≠ 0, `0x10012C13`).

#### Queries and setters

| function | behaviour |
|---|---|
| `LHSampleIsPlaying(bank, owner, LH_SampleInfo**)` `0x10014010` | nothing while switched off (+0x14, `0x10014018`); the first channel of the bank and owner, whatever its sample (`0x10014038..0x10014040`); its info when it is in use (+0x8C == 1, `0x10014051`) |
| `LHSampleIsPlaying(bank, owner)` `0x10013FB0` | whether the first channel of the bank and owner (any sample) is in use (`0x10013FF1`: only that first one is looked at); nothing while switched off |
| `LHSampleIsPlaying(LH_SampleInfo*)` `0x10014070` | 0 while switched off (`0x1001407A`); otherwise the first channel of the info's (bank, owner, sample) (`0x100140E6..0x10014100`). The IsPlaying calls do nothing with the audio off (`0x10013F69`) |
| `LHSampleGetPlayPosition` 0x10014C00 (bank, owner, sample) | −1 while off (`0x10014C09`). It looks at the first channel of the bank and owner without comparing the sample (`0x10014C29..0x10014C31`). A channel in use gives `QSWaveMixGetPlayPosition(…, 2)` in ms; otherwise −1 (`0x10014C6E`) |
| `LHSampleGetPercentageDone` 0x10015180 | 1 when no channel matches. It walks every channel of (bank, owner, sample) (`0x100151AB..0x10015207`); a non-atmos channel met while the audio is off ends it with 1 (`0x100151C6..0x100151D0`); the first channel in use gives position_ms / (bytes · 1000 / (channels · rate · 2)) (`0x10015243..0x1001527D`) |
| `LHSampleGetInfo` 0x10013F60 | the first channel of the triple, in use or not; its +0x40 is the loops (GAudio's fn_0042A460) |
| `LHSampleSetVolume` | nothing while switched off, unless the channel is an atmos one (+0x00, `0x10013412..0x10013420`); clamps to 0..127 (`0x10013473..0x10013487`); changes the first channel of its (bank, owner, sample) (`0x10013489..0x100134A9`) only if it is in use (`0x100134BE`) |
| `LHSampleSetPitch` | nothing while switched off, unless an atmos channel (`0x10013572..0x1001357C`); nothing on a channel that is not in use (`0x1001357E`) or when the pitch is unchanged (`0x10013588`); it ignores pitch 0 |
| `LHSampleSet3DPosition` | looks for the first channel of the triple and respects the channel's fixed axes (+0x14 bits 4/8/0x10, `0x10013BCC`) |
| `LHSampleClearInfoList` | clears +0x1C, +0x18 and +0x04 of each channel (`0x100142EB..0x100142F0`); does nothing with the audio off (`0x100142CC`) |

So in `GAudio::Reset`, which calls `LHSampleClearInfoList` between `LHGlobalSwitch(0)` and `(1)`, it clears nothing.

#### GAudio's sound-effect calls

- 0x42A100 has **10** arguments: the 7th is the loops (+0x4C, `0x42A17A`) and the 8th the +0x10 (`0x42A121`).
- `GAudio::PlaySoundEffect` always returns 0 (`0x429FE8`): no caller receives the channel.
- In `SamplePlayAnimEffect` the unavailable-owner filter depends only on track, without is3D (`0x42A5A1`). The
  distance for `SamplePlayAnimEffect` is computed by the caller (`|LH3DTech::g_camera − pos|`).

### Master volumes, focus and reset

**✅ faithful.**

#### Master volumes

- There are two, from 0 to 127: `AudioSampleMasterVolume` and `AudioMusicMasterVolume`, in
  `HKCU\Software\Lionhead Studios Ltd\Black & White\BWSetup`. Without the registry key, they are 127
  (`[0x10056280] = 0x7F`, `0x1000DE08`).
- They are read in `GAudio::RegistrySetup` 0x428250 and saved in `GAudio::RegistryShutdown` 0x4282B0 ✅ (read
  2026-10-21):
  - read: by GAudio's ctor (0x426E7D) and by the options box `DialogBoxOptions::ControlCallback` on
    SETUP_MESSAGE_ACTIVATE (6, jump table 0x5145F8 → 0x51425D, the call at 0x5142A0), so each time the box shows;
  - saved: by `GAudio::ToBeDeleted` (0x426FF3), as the game ends, and by the same callback on SETUP_MESSAGE_DEACTIVATE
    (7 → 0x5142E0, the call at 0x51430C), each time the box goes;
  - each value is taken only when `RegistryRetrieveULong` finds it (returns 0), the effects' through
    `LHSampleSetMasterVolume` (more than 127 ignored) and the music's through `LHMusicSetMasterVolume`; a missing
    value leaves the volume as it is;
  - the third pair of calls, sub_515830 (0x5158B8 / 0x5158EE, on the same two messages), is a callback whose address
    is nowhere in the exe (no reference, no copy of it in the file): never used.
- The options sliders (`DialogBoxOptions::ControlCallback` 0x5145A3/`0x5145AE`):
  - the music slider does `LHMusicSetMasterVolume(ftol(slider·127))` (`0x5145AE` calls fn_00428660 `0x428660`);
  - the effects slider 0x64 applies the effects master at once through fn_00428600 (DialogBoxOptions `0x5145A3`).
- When the dialog opens, each slider is the master × (1/127): fn_00428690 (`0x515704..0x51571A`), and
  `MiniDialogBoxOptions::InitControls` 0x515690 through fn_00428680 / fn_00428690. Both products are exact in double,
  as on the x87.
- `LHSampleSetMasterVolume` ignores the current value and anything above 127 (`0x100150F2..0x10015100`). It does not
  clamp.
- `LHMusicSetMasterVolume` ignores the current value (`0x1000E8C0`) and applies the new one to the channels in status
  1 or 4 (`0x1000E8ED..0x1000E8F5`).

#### Minimising the window (Alt-Tab)

The chain is `0x7DE6D0` → `0x642470` → `0x428720`.

- It is not focus, it is **minimising**. `GameWindowProc` sets state 0x8002 with wParam 1 (`0x7DBFF6..0x7DC009`:
  message 5, WM_SIZE, through the jump table at `0x7DC658`; wParam 1 is SIZE_MINIMIZED), and sub_7DE8D0 (`0x7DE8DC`)
  calls `AltTabDeactivate` 0x7DE6D0.
- On restore (WM_SYSCOMMAND 0x112 at `0x7DC101`/`0x7DC10C`, wParam 0xF120 SC_RESTORE sets the flag `[0xE8C0FB]`,
  `0x7DC23A..0x7DC245`), `ProcessWindowMessages` 0x7DB9DB calls `AltTabReactivate` 0x7DE6F0.
- While it is minimised, `ProcessWindowMessages` does not leave the message loop (`AltTabbedAway`, `0x7DB9E0`): the
  whole game stops. See [The original loop](../engine/loop.md#the-original-loop) for the loop and the clock on the way
  back.
- On minimising, `LHGlobalSwitch(0)`: `LHWaveSwitch(0)` (StopAll) and `LHMusicSwitch(0)` (`LHMusicStop(0)`). On
  returning, `LHGlobalSwitch(1)` only reactivates the flags: **nothing is resumed**. LHGlobalSwitch includes
  LHMusicSwitch: (0) stops the music without a fade and makes it inactive; (1) makes it active again and restarts
  nothing.
- Alt-Tab without minimising does not touch the audio (WM_ACTIVATEAPP `0x7DC073` only clears keyboard and mouse): the
  music continues (confirmed by the user); a real minimise cuts it, as openblack does.

#### GAudio::Reset

`GAudio::Reset` 0x426CA0 (from `GGame::Init`), in order:

1. Zeroes +0x18C (`0x426CA6`), pos[group] (fn_00428190, `0x426CAC`), +0x28 (`0x426CB6`), +0x24 (`0x426CB9`) and
   +0x180 (`0x426CBC`), and +0x190; sets +0x1C = −1 (`0x426CC8`).
2. The audio-system part runs only when the system exists (`0x426CCF`): LHMusicStop(0) `0x426CD3`, LHAtmosProcess(0)
   `0x426CDD` and LHSampleStopAll `0x426CE6` (StopAll).
3. LHGlobalSwitch(0) `0x426CF6`, a wait until LHSampleGetNumberPlaying() == 0 (`0x426D01..0x426D13`), ClearInfoList
   `0x426D18` and LHGlobalSwitch(1) `0x426D23`.
4. `ReleaseAllThingMusicInfo` 0x4291B0 (called at `0x426D28`).

#### Pause

- `PauseGame` 0x54AE20 does not touch the audio (single player only). It toggles the value 4 of g_game+0x14
  (`0x54AE2F`).
- While that value is set, `LocalTimerSaysDoATurn` 0x54C4A0 gives no turn at all (`0x54C528`). Only the debug flag
  0x400000, written at `0x640F10` / `0x640F32` by the "as fast as possible" key, lets turns run.
- `EndTurn` (`GGame::EndTurn` `0x54E960..0x54E9B4`), its only caller, always does `GSoundMap::Update` 0x54E96F, Dump
  and ProcessSoundTags (`0x54E989`). Then it does ProcessAudioGameTurn when not paused (`g_game+0x14 & 4`) and turn > 5
  (unsigned, `0x54E997`), else `AtmosProcess(0)` (`0x54E9B4`, `0x4286C0`: the loops and the atmos channels stopped).
- So a turn ends paused only when the pause was set inside that turn (a key or `GoInsideCitadel` handled in
  ProcessGameInputs, or the script starting a film): that one end of turn cuts the ambience.
- A pause set outside a turn (the front end's callbacks, a key taken by the frame-start key pass) gets no paused end of
  turn. The ambience loops sound on at their last volumes, no new one-shot starts, music plays on, samples already
  playing finish and the 3D positions stay as they were.
- The two other callers of `LHAtmosProcess` (`0x46126A`, `0x55C7CA`) are unreferenced. runblack.exe does not import
  `LHMusicPause` 0x1000EA10, so the game never calls it.

### Ambience: GSoundMap and LHAtmos

#### The sound map

- The only GSoundMap is at g_game+0x250058. `GSoundMap::Update` 0x71D6F0 runs
  `GSoundMap::CalculateRadiusPointAndDistance` 0x71D800, `GSoundMap::UpdateFromMap` 0x71D720 and CalculateVolumes fn_0071DD60 (the 14 volumes), then Dump `0x71D990`.
- **UpdateFromMap** scans the (2r/10 + 1)² cells around the receiver (11 × 11
  with r = 50). It subtracts and adds r to the MapCoords (fn_00605490 / fn_00605400, `0x71D77A` / `0x71D784`). The
  cells are the high words, read signed, both ends included (`0x71D790..0x71D7E9`).
- **AddAtmosType** (`GSoundMap::AddAtmosType` 0x71E260 + `AtmosMapTypeInfo::Add` 0x71D510) keeps the squared
  distance from the receiver to the cell's centre:
  - each MapCoords is fild; fmul 10 `[0x9804D8]`; fmul 2^-16 `[0x8AC41C]`, with one rounding of the exact product
    (`0x71D514..0x71D558`); the receiver minus the cell is squared and summed (`0x71D55A..0x71D562`);
  - a cell replaces the kept one only when strictly nearer (`0x71D568..0x71D570`);
  - the nearest point is stored as words by __ftol (`0x71D575..0x71D5A4`);
  - the starting squared distance is 65535 (0x477FFF00).
- **HeightFade** fn_0071DAC0: 1 while the camera is below normalAtmosFadeStartHeight above the land at p, 0 above the
  end height.
- The sea's share is fn_0071E210(SEA): sea cells over all typed cells.
- The wind volume is clamped after its formula: above 1 → 1, below 0.01f (dword `0x8C5840`) → 0.
- The table of surface names is at `0xC22270` (9 names).

#### The LHAtmos mixer

- **Loop sample** (LHSamplePlay at `0x10001A18`): AtmosInfo 1, is3D 0, owner −1, loops −1, mode 2, caller mask 0x20,
  volume 0.
- **One-shot (loose sample)** (LHSamplePlay at `0x10001E3C`, options from `0x10001BD7`): AtmosInfo = its queue entry,
  is3D 1 (+0x08), +0x0C 0 (`0x10001E38`, not tracked; no effect on an atmos channel, which the 3D update skips),
  relative (+0x14 = 1) at (x, y, 0), owner −1, caller mask 0x20, volume = bank volume × the entry's volume / 127. The
  mode (3), pitch, deviation and distance mapping come from the .sad or the defaults. The channel it returns is not
  kept: the mixer finds its loose samples again by their AtmosInfo, so a start that returns an already playing channel
  (the .sad's play mode 2 meeting the sample or its clone group) leaves that channel what it was, a loop's (AtmosInfo
  1) or another loose sample's.
- **Loose fade** (`0x10001925..0x10001975`): every sample channel with an AtmosInfo other than 0 and 1 whose entry's
  group is neither 0 nor its bank's gets its own volume (+0x38, what its start or the last volume change left) − 5,
  when that is not negative.
- **`LHAtmosSetBankVolume`** (0x10001FC0): the volume clamped to 0..127; nothing more if the bank already has it;
  otherwise it is stored and every channel of the bank with an AtmosInfo other than 0 and 1 whose entry's group is 0
  or the bank's gets the entry's volume × the new bank volume / 127 at once (`0x10001FFE..0x10002051`).
  `LHAtmosSetGroup` 0x10002060 only stores the group (+0x134).
- **The queue step** (`0x10001B84..0x10001BA7`): only while the queue holds an entry does the turn counter go up by 1;
  then the head plays if it is due (next <= counter, unsigned).
- **The entries' volume** (fn_10001610 `0x100016A2..0x100016BD`, and the loop node at `0x10001208`): the .sad's low
  u16 at +0x25C as it is, not clamped to 127, with the flag 0x20; else 127.
- **One-shot queue** (fn_100011B0, `0x10001304..0x100013A9`): a new entry goes before the first one that is due at the
  same time or later. On a reinsertion the walk starts at the head's next, and fn_10001170 pops the head afterwards.
- **Bank registration** (fn_10001610): `srand(time(0))` first (`0x10001635..0x1000163D`), then every record of the
  bank (its +0x08 count) with atmos frequency >= 0 and a sample number other than 0 (`0x10001668..0x1000167E`). A
  loose one is due at counter + 4f + rand * 12f / 32767 and the earliest of the bank's is kept (unsigned,
  `0x1000171C..0x10001727`). If the bank had one, the counter becomes that earliest − 20 (`0x1000176A`, wrapping below
  0), and then, at most once per queued entry, while the head of the whole queue is due before the new counter, its
  time is drawn again from the counter and it is reinserted (`0x10001785..0x1000182E`).
- **`LHAtmosProcess(0)`** stops every kept channel without asking whether it plays (`0x10001ED3..0x10001EE5`). The
  by-channel stop (`0x10012DF0`) stops an atmos channel even while the audio is switched off.

## openblack

### The Audio Player and Audio banks windows (debug bar)

A debug tool of openblack's, not in the original. The **Audio Player** window (*Debug → Windows*,
`src/Debug/Audio.{h,cpp}`) has two switches at the top, on by default, above its Sound, Music, Channels and Atmos
tabs. The **Audio banks** window (`src/Debug/AudioBanks.{h,cpp}`) is listed beside it.

- **Game sounds** and **Game music** (`audio::OutputSwitches`, kept in `Locator::audioState`): off, the 16 sample
  channels (`AlSampleOutput::ApplyGain`) or the 6 music channels (`MusicStream::ApplyGain`) get gain 0 at the output.
  Nothing else changes: the channels keep playing, the music keeps decoding and calling back, and turning a switch on
  gives each channel its own gain again (at the next frame for the samples, the next pump for the music). On, the gains
  are the same as without the switches. They stay as they were set while the window is closed.
- **Audio banks** lists every `.sad` under `Audio\` and `CreatureIsle\Audio\`, and a bank's samples (or music
  segments) with their size and format. Each one decodes as the game decodes it (`wave_buffers::Decode`), showing its
  channels, rate, length and decode time; "Decode all" times the whole bank. Play uses a source of its own, a 2D one at
  full gain outside the 16 channels, so the game's channels and the two switches do not touch it.
- **Atmos** (raffclar's Atmos tab, on our ambient banks) shows a copy of the banks' state, `atmos_banks::GetView()`:
  the mixer's turn counter and group, each bank's target, current value, volume 0..127 and group, the loops (fade,
  volume, playing), the loose samples in the order they are due (turns to go), the loose samples playing, and the
  climates (`weather::climate::ForEach`). Reading it asks no channel and draws no random number. His Emitters tab is
  not taken: the Channels tab shows the same 16 channels. The storms are in the Weather window, and the weather at the
  camera is not sampled here, since sampling it stamps the weather grid.

### The decoders (audio::codec)

**✅ faithful (checked bit by bit).** openblack's own decoders, `src/Audio/Codec/`, give the samples the earlier
libraries gave (dr_wav 0.14 and dr_mp3 0.7, which they replaced) for every sample of every bank of the game, `Audio\`
and `CreatureIsle\Audio\` (81 850 checks: 6208 PCM, 219 MS-ADPCM, 4919 MPEG waves, every music segment in order and
from a fresh decoder).

- `WaveFile` reads what dr_wav read, so that nothing that played stops playing: RIFF, RIFX, RF64, Wave64, AIFF and
  AIFC; integer PCM of 1 to 8 bytes (8-bit unsigned; the top 16 bits kept), IEEE float 32/64, A-law, mu-law, MS-ADPCM,
  IMA ADPCM, WAVE_FORMAT_EXTENSIBLE. Checked against dr_wav with 450 000 random files of every container and format,
  broken ones included: 0 differences.
- `MpegAudio` is MPEG-1/2 layer II (the voices, the music): the frame search, the layer II tables and the
  single-precision synthesis in the order of minimp3's SSE path, with its two roundings (half away from zero for two
  samples of each 64, half to even for the rest). GCC and Clang compile it with `-ffp-contract=off`.
- Kept as before, quirks included: a wave of an unknown format opens as silence of its data's length.
- **Fixed (the user's decision):** an MS-ADPCM wave ends at its "fact" length. The old library ignored the chunk and
  played the padding of the last block: the 219 MS-ADPCM waves of the game lasted 5 to 1009 frames (550 on average)
  too long. Their samples up to that length are unchanged. Windows' own MS-ADPCM codec (msadp32.acm, the codec QMixer
  calls) gives the same samples, but it decodes whole blocks, so it does not tell whether QMixer stopped at the
  "fact" length.
- **Different from before:** a header that claims more than 2^28 samples is refused as broken (dr_wav tried to
  allocate it, which threw); no file of the game comes near it.

### The effects core in openblack

#### The core

Layers:

- **0. Device**: `audio::device` (the OpenAL device and context), `AlSampleOutput` (one source per channel, outside the registry; they
  are deleted in `ClearMap`), `WaveBuffers` (buffers and decoders).
- **1. LHaudio/QMixer**: `QMixerLaws` (`qmixer::Gain` 0x100133C1, `DistanceGain` 0x1802CE50, `PolarRelative`
  0x10012269, `FrequencyRatio` 0x10012820, `StartPitch` 0x1001278B) and `SamplePlay` (16 channels: `Start` 0x100113B0,
  `Stop` 0x10012C50 / 0x10012DF0 with the first-channel rule, `StopAll` 0x10012BF0 without the ambience ones,
  `IsPlaying` 0x10013ED0 / 0x10013FB0, `ReleaseLoop` 0x10012F20 on the first one, `SetPitch` 0x10013520, `SetVolume`
  0x10013400, `SetMainVolume` 0x100150E0, `UpdateChannels` 0x10014310 + listener, `Switch` 0x10015D40,
  `ClearChannelOwners` (was `ClearInfoList`) 0x100142C0).
- **2. GAudio** (`AudioSystem`): banks (`RegisterBank`, `Bank(SfxBank)` = GAudio+0x3A8+4·type, `FindBank`,
  `SampleId`, `CreatureBank`), `PlaySoundEffect` filters 0x429E30 (3D cutoff with +0x58, userParam 1/2/4,
  SET_GAME_SOUND on GScript+0x90 of `ScriptAudioState`, `OwnerUnavailable` 0x429D20), life cycle. (The
  `PlayAnimEffectSample` sample branch of `CollisionSounds` was removed: the physics now passes its key.)
- **3. GameSfx**: the 5 variants 0x429D60 / 0x429DA0 / 0x42A000 / 0x42A040 / 0x42A100, Stop 0x42A210, ReleaseLoop
  0x42A330 / 0x42A310, IsPlaying 0x42A280 / 0x42A2B0 / 0x42A2D0, SetPitch 0x428740 and the cyclic counters
  (`enum class Counter`).
- **4. Public API** `src/Audio/Audio.h`.

#### The public API

No default arguments; each caller passes what the original passes.

| function | original |
|---|---|
| `PlaySoundEffect(PlayOptions)` | 0x429E30 (own options: Guidance, PSysSound, SoundTag) |
| `PlaySoundEffect(owner, sample, mode, loops, extra3DFlag, is3D, SfxBank / BankId)` | 0x429D60 / 0x429DA0 (3D without owner or with an unavailable owner: nothing) |
| `PlaySoundEffectAt(owner, pos, sample, mode, loops, extra3DFlag, is3D, SfxBank / BankId)` | 0x42A000 / 0x42A040 (track = is3D) |
| `PlaySoundEffectAt(owner, pos, offset, sample, track, mode, loops, extra3DFlag, is3D, BankId)` | 0x42A100 (SoundTag fn_0071E680) |
| `StopSoundEffect(sample, owner, bank)` (sample 0 = all of the owner's) | 0x42A210 → LHSampleStop |
| `StopAllSoundEffects()` | fn_004287D0 |
| `LeaveCitadel()` | Temple fn_00793D00 from LeaveInsideCitadel 0x553B25: Stop 2 and 12 of InGame, owner 0 |
| `GuardSoundPoint(p)` (AudioSystem.h) | fn_00427200: \|v\| > 5000 → 0 per coordinate |
| `ReleaseLoop(owner, sample, bank)` | 0x42A330 / 0x42A310 |
| `IsPlaying(owner, sample, bank)`, `IsPlaying(owner, SfxBank)`, `IsPlaying(Channel)` | 0x42A280 / 0x42A2D0, 0x42A2B0, 0x10014070 |
| `SetPitch(bank, owner, sample, percent)`, `SetVolume(Channel, v)` | 0x428740, 0x10013400 |
| `NextCounter(Counter)` | counters 0xC4CC7C, 0xC5E3E4/E8, 0xC6421C, 0xC64220, 0xD18228, 0xD4437C, 0xD559AC, 0xD95AF8/FC |
| `MaxDistance(Sample)`, `CreatureBank(species)` | 0x42A430, 0x4EBD81 |
| `RegisterObject(id, fn)` / `UnregisterObject` | Get3DSoundPos (vt +0x10) of an owner that is not a GameThing |
| `Init(GameQueries)`, `Shutdown()` | ctor 0x426D40 (master fn_00428250), ToBeDeleted 0x426FE0 |
| `ProcessTurn()` | GGame::EndTurn 0x54E960 (the turn from `game_clock::Turn()`, the sky type from `sky_type::Frame()`): GSoundMap::Update, ProcessSoundTags, and after turn 5 ProcessAudioGameTurn 0x427080 (with the LHWaveIsActive gate) or AtmosProcess(0) |
| `ProcessCitadelTurn()` | Temple::ProcessGameTurn 0x794A5A: ProcessAudioGameTurn alone, every 100 ms while the game is paused inside the citadel |
| `UpdateFrame()` | the live master and the finite loops (the paused end of turn, 0x54E9B4, is in `ProcessTurn` since the audio leftovers work, formerly issue 22) |
| `ClearMap()` | GAudio::Reset 0x426CA0 (+ the map's SoundTags and street lanterns) |
| `OnFocus(bool)` | minimise / restore: 0x7DE6D0 / 0x7DE6F0 → 0x642470 → fn_00428720 → LHGlobalSwitch 0x10015790 |
| `SetSampleMainVolume(v)`, `SampleMainVolume()` (were `SetSampleMasterVolume`, `SampleMasterVolume`) | 0x100150E0 / 0x10015170 |

#### Player fixes

| fix | state | where |
|---|---|---|
| Empty sample → `continue` | already done | `Game.cpp` |
| RIFF 0x50 (MPEG layer II) → the MPEG decoder | **done**: HelpSprites 1 = 5.2 s; the 209 of InGame decode | `WaveBuffers.cpp` (`Decode`) |
| One AL buffer per sample, on first use (leak gone) | **done**, also for the old players | `WaveBuffers.cpp` (`Get`) |
| Finite loops N | **done**: the source loops and each wrap of the offset is one pass; after N wraps it stops looping, so N+1 passes of the span sound **🔍 inferred** | `SampleOutput.h` (`LoopCounter`), `AlSampleOutput::Update` |
| Span lStart..lEnd | **done** with `AL_SOFT_loop_points` (G_VillageBell 0..27400 of 57855) | `WaveBuffers.cpp`, `Sound::loopStart/loopEnd` (+0x138/+0x13C, `Loaders.cpp`) |
| "Villagers.sad" case-insensitive | **done**: the filter compares banks (`BankId`), not names; `RegisterBank` recognises the 11 types of 0x9CB3F8 by case-insensitive path | `AudioSystem.cpp` |
| AL_PITCH overwritten / per-frame listener | already done | `Device.cpp`, `SamplePlay` |

### Counterparts in the code

What openblack does at each step of the original described above, by section.

#### Banks

- `audio::SfxBank` and `k_SfxBankPaths` in `src/Audio/Game/BankTables.h`
  (`inline constexpr std::array<std::string_view, static_cast<size_t>(SfxBank::_COUNT)> k_SfxBankPaths = {`). An
  empty view is the null pointer of type 0. The banks are registered by `audio::banks`
  ([the audio engine](engine.md#a-single-engine)).

#### Wave loading

In `src/Audio/Device/WaveBuffers.*`:

- The .sad files are read whole at start-up, except the dialogue ones (types 6..10, `Audio\Dialogue`). Of those only the
  headers are read (`PackFile::ReadAudioHeaders`), and each wave is read from the file when it is decoded
  (`Sound::waveFile`, `banks::ReadWave`), like `LHBankRegister(path, 0)`. That the rest of the banks are read whole is
  **⚠️ approximate**: the original registers them all this way (0x426EEE).
- Each sample is **decoded only once, on first use**, into an AL buffer that is kept (`Sound::bufferId`) until the
  audio is closed. **⚠️ approximate**: one buffer per sample record, not per wave +0x108 (clones are each decoded).
  There is no budget or eviction, which is the original's result: its cache never fills with the shipped banks
  ([Wave loading and cache](#wave-loading-and-cache), `audio::k_WaveCacheBudget`).
- RIFF with wFormatTag 0x50 (or 0x55) → the `data` block to the MPEG decoder (layer II), like ACM; any other RIFF → the
  wave decoder; anything that is not RIFF → raw MPEG, then a wave file ([the decoders](#the-decoders-audiocodec)).
- The +0x138/+0x13C span goes to the buffer as `AL_LOOP_POINTS_SOFT`.

#### Channels

- **Applied fields.** `sample_play::AppliedOverrides`, kept on the channel by `sample_play::Start`. Before, the channel
  kept the .sad's whole flags, so those loops started at volume 0 and stayed silent, and those loose samples ignored
  the fade of other groups. Tests: `SampleOverrides.AppliedAreTheSadsFieldsTheCallerLeftClear`,
  `SamplePlayTest.CallerVolumeCanBeChangedDespiteTheSadsVolume`, `...CallerPitchCanBeChangedDespiteTheSadsPitch`,
  `...AmbienceFadesReachTheChannels` (and `Synthetic`).
- **The DLL's random numbers**: `sample_play::SeedRand` (srand), `sample_play::AlternatingRandom` (Rand()),
  `sample_play::Random` (Rand(n)).
- **Laws**: `qmixer::Gain` is floor(m * v / 127) * 258, / 32767. `qmixer::PolarRelative` reproduces the game-thread
  half with each FPU step a double operation rounded to a float (R); `test_audio_laws.cpp` pins both halves, emulated
  digit for digit.
- **Stop fade** (`ramped`, in `Halt` and `Stop`): openblack fades the same way (`SampleOutput::StopRamped`: four 5 ms
  gain steps, **⚠️ approximate**: OpenAL has no pan rate), but **asynchronously**. The channel is stopped and free for
  the logic as soon as the call returns, as it was when the wait ended, and its source fades and is flushed on a fader
  thread of `AlSampleOutput` (`RampedStops`) while the game goes on. It sounds the same; only the instant of the cut may
  move by a few ms. A new start on that channel takes another source, so it neither fades nor is cut; `StopAll` cuts a
  fade still sounding at once.
- **Stops**: `sample_play::Stop` (by sample, or by channel), `sample_play::StopOwner` (sample 0).
- **Queries**: `sample_play::OwnerChannel` (`LHSampleIsPlaying` 0x10014010; `k_NoChannel` when not in
  use or while switched off), `IsOwnerPlaying` (0x10013FB0), `PlayPosition` (0x10014C00).
- **Setters**: `sample_play::SetVolume`, `sample_play::SetPitch`.
- **⚠️ approximate**: `sample_play::IsPlaying` (`LHSampleIsPlaying(info)`, which gives 0 with the audio off,
  0x1001407A) looks at the handle's own start, also while switched off. openblack's game runs on while minimised (the
  original stops), and a caller asking every turn (PSysSound 0x6D120A) would start its sample again on another channel
  each turn. For `LHSampleSet3DPosition`, openblack moves the handle's channel and has no fixed axes.

#### Master volumes, focus and reset

- **Music volume**: `EngineConfig::audioMusicMainVolume` (was `audioMusicMasterVolume`) (127) in the "Music" debug
  panel (`Music::Draw`, `src/Debug/Music.cpp`: a slider with the original's two products).
- **Effects volume**: `EngineConfig::audioSampleMainVolume` (was `audioSampleMasterVolume`) (127), applied live with
  `LHSampleSetMasterVolume` (reapplies to the channels in use) and a `ftol(slider·127)` slider in the "Channels" tab
  of the audio panel ([The audio engine](engine.md#test-hooks)).
- **Kept for the computer**: both in `Setup.txt` of the profiles' folder (`machine_setup` in `src/Common/MachineSetup`,
  the original's two value names, a tab and the number), through `audio::main_volumes` (`src/Audio/MainVolumesSetup`):
  read after the profiles are found, before the menu and the audio start; read each time the menu's Options or
  Advanced page shows and kept each time either is left (`Game::CreateInterface`'s volume source,
  `Game::HandleInterfaceAction`); kept as the audio shuts down (`Game::ShutDownGame`). The rule of
  taking them is `machine_setup::ApplyVolumes` (the effects' only up to 127). A run of the checks has no profiles'
  folder, so it reads and writes nothing. The audio panel's Channels tab shows the file and can read or keep them.
  ✅ Read 2026-10-23: on either options box shown (event 6, 0x51425D) the original reads the volumes first
  (`GAudio::RegistrySetup`, 0x5142A0), then sets the box's controls (`DialogBoxOptions::InitControls` through
  [vtable+0x20], 0x5142D6), the sliders from the volumes just read; it reads them on the Advanced box's show too, and
  keeps them on either box's leave (event 7, 0x51430C). openblack sets the sliders from the configuration's volumes
  each time the Options or the Advanced page shows (`GameMenu::ShowOptionsBox`, test
  `GameMenu.TheVolumeSlidersAreSetFromTheMainVolumesEachTimeAnOptionsPageShows`), the file read just before (the
  volume source given by `Game::CreateInterface`). A tab between the pages leaves one box and shows the other in
  the same step, so the source is told the box was left and keeps the volumes before it reads them, as the
  original's leave comes before the show; a leave with no page shown after it (Back, Escape, the menu closing) keeps
  them in the menu's leave step (`GameMenu::TakeOptionsLeft`); test
  `GameMenu.TheVolumesAreKeptAsTheBoxIsLeftBeforeTheShownPageReadsThem`.
- **Reset**: the music part of `GAudio::Reset` and `GScript::Reset` goes in `Game::LoadMap`.
- **Alt-Tab**: `SDL_WINDOWEVENT_MINIMIZED/RESTORED` → `audio::OnFocus` → `LHWaveSwitch` + `LHMusicSwitch`. Losing
  focus does not turn anything off, as in the original. The ambience channels are not stopped by `LHSampleStopAll`
  (0x10012C13). Since issue #9 the game loop waits while minimised, as the original (`Game::Update`,
  `Windowing/WindowAway.h`), so no turn or frame runs and the ambience is not driven meanwhile.
- **Pause**: as the original (see [Pause](#pause) above), since the audio leftovers work (formerly issue 22). `audio::ProcessTurn` takes its steps from
  `audio::end_of_turn::For(game_clock::IsPaused(), turn)` (tests `EndOfTurnAudio.*`): the sound map and the tags every
  turn, then the audio game turn, or `AtmosProcess(0)` (`atmos_banks::Silence`) on a turn that ends paused or up to
  turn 5. No turn runs while paused (`game_clock::TurnDue`), so only a turn whose own inputs or scripts paused the game
  (a film started by a script, for one) cuts the ambience; a pause set in the frame's key pass, by the escape menu or
  by the start-up requester leaves it sounding. The per-frame `audio::Paused` call is gone. Which key pass takes the
  player's pause key is openblack's own timing (the frame-start pass), so that key does not cut the ambience.

#### Ambience

- The sound map: `CalculateRadiusPointAndDistance`, `UpdateFromMap` (the (2r/10 + 1)^2 cells around the receiver), `AddAtmosType` (with
  `ecs::map_coords::ToMetres` for the one rounding of each MapCoords), `HeightFade`, `CalculateVolumes`, and
  `SurfaceName` (the table 0xC22270).
- The LHAtmos mixer: `PlayLoop` (the loop sample) and `PlayLoose` (a loose sample). `atmos_banks::StopChannel` stops
  each stored channel without asking whether it is playing, like `LHAtmosProcess(0)`.
- The mixer's entry points are the DLL's: `atmos_banks::RegisterBank(bank, records, clock)` (fn_10001610: a bank's
  sample records, and the clock its `srand(time(0))` reads, passed to `sample_play::SeedRand(clock)`),
  `SetGroup` (LHAtmosSetGroup), `SetBankVolume` (LHAtmosSetBankVolume) and `Process(voices, active)` (LHAtmosProcess).
  The game drives them as GAudio does: `UpdateBanks` registers the 14 banks once with the wall clock, then per bank
  `SetGroup`, the fade and `SetBankVolume` (ProcessAtmosBanks 0x428FE0); `Mix` is `Process(1)` and `Silence`
  `Process(0)`. The voices are an `AtmosVoicesInterface` (`src/Audio/Services/AtmosVoicesInterface.h`): the game's
  `AtmosVoices` are the 16 sample channels (`sample_play`), and the scheduler tests give a fake with the scenarios'
  model of LHSamplePlay.
- The golden ambience scenarios (from raffclar's tree) (`test/audio/test_atmos.cpp`, data in `test/audio/scenarios`, made by
  `test/audio/scripts/emu_*.py` under Unicorn):
  - **The sound map, alignments and sky types (2026-10-22).** The first recording of the sound map volumes, the alignment values
    (fn_005E2240) and the sky types of `volumes_1/2.json` kept double intermediates: Unicorn's x87 ignores the control
    word fn_007DEE00 leaves (24 bits). Run natively on the frozen v1.42 exe at its control word 0x007F through the
    oracle (`test/audio/scripts/oracle_volumes.py`, the same set-up as `emu_soundmap.py`), the exe differs from that
    recording in exactly the queries ours did (244 / 600 and 268 / 600, 33 / 206 and 28 / 206 alignments, 22 / 300 and
    17 / 300 sky types), and gives ours, float step by float step, in every alignment and sky type and in all but two
    sound map queries: there a day-only bank (jungle, swamp) took its weather fade and its night in the other order,
    now fixed (`sound_map::DayOnlyShare`, 0x71DFC8..0x71DFDA). The three results are recorded again from the exe and
    the six tests run (`AtmosAudio.MatchesOriginalSoundMap*`, `MatchesOriginalAlignments*`, `MatchesOriginalSkyTypes*`).
    The bank steps (ProcessAtmosBanks, float in both) are kept as first recorded and run.
  - **The scheduler (2026-10-22).** `scheduler_1..3.json` were recorded from LHAtmosProcess of
    LHaudiodllR.dll, which is the frozen game's own file (sha1 35f149cb…, the same in bw1-decomp's BW1W100..BW1W130);
    recorded again on the frozen DLL (`emu_scheduler.py` with `BW1_AUDIO_DLL`) they come out the same. Our mixer
    replays them through its entry points (`RegisterBank`, `SetGroup`, `SetBankVolume`, `Process`) on a test fake
    with the scenarios' model of LHSamplePlay (`ModelVoices` in `test_atmos.cpp`: a fresh voice for every start, a
    one-shot lasting 1 + (7 · sample + 3 · bank) mod 13 turns, play modes 2 and 3 finding a playing voice by sample or
    clone group, the record's overrides of volume, loops, distances, scale, pitch and mode, the pitch deviation drawn
    from the library's generator, which the first start seeds with the clock). The differences of ours they showed are
    fixed from the DLL (see the audible behaviour changes): the registration's counter and its second draws, a bank's
    new volume reaching its loose samples and their fade from the channel's volume, a play-mode-2 start leaving the
    voice it finds what it was, and the record's volume taken whole. The three tests run
    (`AtmosPlayer.MatchesOriginalScheduler1..3`: 3950, 676 and 1850 events, each the original's).
- `cornerA` / `cornerB` (the DLL's [0x10037050] / [0x10037054], both 1 at the start): the x and y signs of the
  corner (±5, ±5) a loose sample drawn exactly at the listener is moved to; each such sample moves them on, through
  (5, 5), (-5, -5), (5, -5), (-5, 5) and back (`atmos_banks::Process`, `src/Audio/Services/AtmosBanks.cpp`). Read from the code ported from the DLL;
  the scheduler scenarios above replay it only if they draw such a sample (not checked).

#### Audible behaviour changes

All because of the original:

- `SET_GAME_SOUND` no longer cuts the ambience (0x10012C13; `SET_GAME_SOUND false` used to stop them) and its flag goes
  back to 0 on each map (GScript::Reset 0x6EB403; before it was a separate flag that was not reset).
- `Stop`/`ReleaseLoop`/`SetPitch` act on the first matching channel (before on all of them).
- With the bank filter by `BankId`, the villagers are no longer silenced after `SET_GAME_SOUND false`.
- Minimising the window stops the effects and the music (losing focus does not).
- Finite loops end (bell ×5, frogs, birds, doves) and loops with a span repeat only the span on the 16 channels.
- The music and the ambience are processed in the order of `ProcessAudioGameTurn` (before, the music went at the start
  of the turn).
- (2026-10-22) A new bank volume reaches the bank's loose samples that are playing in its group (or in none) at once,
  and a loose sample out of its group fades from the volume its channel has (LHAtmosSetBankVolume 0x10001FC0, the fade
  at 0x1000195C); before, the loose samples kept their start volume and faded from it. A loose start that returns an
  already playing channel (play mode 2: the arctic, high and wind banks' samples) no longer counts that channel as a
  new loose sample (tests `AtmosPlayer.ABanksVolumeReachesItsPlayingLooseSamples`,
  `AtmosPlayer.ALooseStartGivenTheLoopsVoiceLeavesItTheLoops`).
- (2026-10-22, none of them reaches the shipped banks: every ambience record's volume is at most 127, every number is
  above 0 and Land 1's queue is never empty) The mixer takes a record's .sad volume whole, not clamped to 127
  (`Sound::atmosVolume`, `atmos_banks::RecordOf`); a record with a negative sample number registers (only 0 is left
  out, `0x1000167E`); and the turn counter only runs while the queue holds a loose sample (tests
  `AtmosPlayer.ASampleRecordGivesTheMixerItsWholeVolume`, `AtmosPlayer.OnlyTheSampleNumberZeroIsLeftOut`).
- (2026-10-22) A bank's registration puts the mixer's turn counter 20 turns before that bank's earliest loose sample,
  and the queued samples due before the new counter draw their time again (fn_10001610, `0x1000176A..0x1000182E`).
  Before, the counter went 20 turns before the queue's head and nothing was drawn again: which loose samples play
  first after the start, and when, now follows the original (test `AtmosPlayer.RegisteringABankDrawsTheEarlierOnesAgain`).

#### Audit of the core

- Checked in the disassembly: 0x429D20, 0x429D60/0x429DA0, 0x42A040, 0x42A100 (order of the 10 arguments), 0x429E30
  (order of the filters and the `<=` of the 3D cutoff), 0x42A4B0, 0x426E6B (800), 0x54E960, 0x427080, 0x426CA0,
  0x427200, 0x4068F4, 0x63AA39, 0x5D2109 (counters), 0x7DBFF6 / 0x7DE8DC / 0x7DE6D0 / 0x642470 (minimise) and in
  LHaudiodllR 0x10011020 (allocation), 0x10012BF0, 0x10012C50, 0x10012F20, 0x10013400, 0x10013520, 0x10013AC0,
  0x10013ED0 / 0x10013FB0, 0x10014070, 0x100142C0, 0x10014310, 0x100146F0, 0x100150E0, 0x10015D40, 0x10001EBF.
- Fixed: `LHSampleSetVolume` and `LHSampleSetPitch` do nothing with the audio off except on ambience channels
  (0x10013412, 0x10013572), and `SetVolume` acts on the first channel of its (bank, owner, sample) (0x10013489).
  `LHAtmosProcess(0)` stops each stored channel without asking whether it is playing (0x10001ED3);
  `atmos_banks::StopChannel` likewise.
- Fixed (leak): `audio::ClearMap` left sounds playing across a land change. In the original they are channels and
  `LHSampleStopAll` 0x426CE6 stops them: now every sound is a channel, and `ClearMap` stops them all and releases
  the channels' sources (`sample_play::ReleaseSources`). Trace: `Sample play: N channel sources released` with
  `OPENBLACK_AUDIO_TRACE`.
- `audio::GetSurfaceType` called `ecs::sea_cells::GetSurfaceType` (a single source); it no longer exists: the game
  calls `ecs::sea_cells` and the audio calls `audio::SurfaceType` (query `surfaceType`).

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

### From the audio engine's list

- A wave of an unknown format opens as silence of its data's length (kept from the old wave library): it could be
  refused instead. None of the game's banks has one.
- The channel's applied fields (the ambience mixer work, formerly issue 21): the ambience loops' fade-in and the loose samples' fades now reach the
  channel; to confirm on Land 1 with `OPENBLACK_AUDIO_TRACE` ("Sample play: ... volume a -> b" without "ignored").
  Still not modelled: the options' name other than "NONE" (no game caller sets one) and the fixed axes of
  `LHSampleSet3DPosition` (bits 4 / 8 / 0x10, see the approximations above).
- The channel's loops (+0x40 of `LHSampleGetInfo`) are those from the start and 0 after `ReleaseLoop` (the DLL's pass
  counter is not read) **🔍 inferred**.
- One buffer per sample record and not per wave.
- Loops: N or N+1 passes; today N+1 **🔍 inferred**.
- Not dumped: `GSoundMap::Reset` 0x71D6D0.
- MPEG layer III (.mp3) decodes as dr_mp3 did, bit for bit on LAME-made files, Xing/Info and LAME delay and padding
  included; nothing in the game uses it. Layer I frames go through the layer II path and are not checked against
  dr_mp3.
