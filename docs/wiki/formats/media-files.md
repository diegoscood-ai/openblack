# Media files and data packs

## Summary

The game's media files: the animation clips of `Data\AllAnims.anm`, the sound banks (`.sad`, with their sample
tables and anim-effect tables), the `.sas` text of the clip sounds, the music banks of MPEG layer II segments, and the
texture files as the loader cuts them. The second half of the page is about the data packs of this installation: the
user's own `AllMeshes.g3d` mod and how it differs from the original pack, which the checks read (the mod is only a
mod).

openblack reads every one of these formats with its own readers (`components/pack`, `components/anm`, the audio
engine's `MusicBank`); the bank and music formats are ✅ faithful.

## The original

<a id="clips-dataallanimsanm"></a>
### Clips

`Data\AllAnims.anm`:

- Clip index = enum `ANM_*` from the game's `Data\AllMeshes.h` (441 clips; the `AllMeshes.h` in bw1-decomp is the
  Creature Isle one and has other numbers).
- Header (0x54): 0x20 **duration in ms** of game time; 0x28 **stride** (metres of one cycle; walk man 1.16);
  0x2C..0x34 displacement vector; 0x38 frames; 0x40 clip size in bytes (openblack once called it
  `animationDuration`); 0x50 flags: low byte = bones, **0x100 loop**, 0x200 (at runtime) no distance sync
  (stride < 0.05). `LoadAllAnimations` (0x550180) removes the loop from 323 `P_OUT_OF_PRAY`.
- Frame: pointers and count + bones × 12 floats. **There are no per-frame times** (what openblack once read as time is
  a pointer): the keys are evenly spaced. Loop: period = duration and the last key returns to the first; no loop:
  period = duration·n/(n−1) (integers) and it stays on the last one.
- Matrices: rows (x, y, z axes, translation), row vectors; each bone **relative to its parent** (the same convention as
  the mesh bones). No root displacement (the advance is only in the header).
- How the clips are played: [Animation](../beings/animation.md).

#### .sad format

**Faithful.** A .sad is `"LiOnHeAd"` followed by blocks `{char name[32]; u32 size; data}`.

| block | contents |
|---|---|
| `LHFileSegmentBankInfo` (532 B) | u32 a, u32 b, u32 **music** and a 520 B title. Music is 1 in the 58 banks of `Music\`, in `Intro\trailer.sad` and in `Dialogue\MissionariesVerse1..3.sad`; 0 in the rest. a/b are 0 except in `ocean.sad` (7, 6), **🔍 inferred**: editor version |
| `LHAudioWaveData` | the concatenated waves |
| `LHAudioBankSampleTable` | u32 `n | (nAtmos << 16)` + n records of 0x280 B (nAtmos = n in the 13 of `Atmos\`) |
| `LHAudioAnimArrayTable` | u32 rows, u32 width (6), rows × 6 s32: anim-effect tables |
| `LHAudioWaveNumTable` | the {count, samples…} lists that column 6 points to |
| `LHAudioListViewText` | bank editor text, not used |

Sample record (0x280 B):

| off | field |
|---|---|
| +0x000 | path of the original .wav (char[256]). In the dialogues it is `K:\4frosty\Spanish\…\HELP_TEXT_….wav`: it is the basis of the voice table |
| +0x104 | id, 1-based: the sample number the game uses |
| +0x108 | sample whose wave is used (clones: editor 1..6 → 1); the cache compares this |
| +0x10C / +0x110 | size and offset in WaveData. Size 0 = empty sample (InGame 165, spells 31) |
| +0x118 | u16 clone group (mode 3) or, in music, the sync group (only in segment 0) |
| +0x11A | u16 ambience group |
| +0x124 | WAVEFORMATEX (+0x128 = Hz) |
| +0x138 / +0x13C | loop start and end in frames (−1 = none), taken from the .wav's `cue` chunks |
| +0x140 | description (in music, the `!n=label` markers) |
| +0x240 | priority |
| +0x244 | override flags (0x20 volume, 0x40 loops, 0x80 min, 0x100 max, 0x200 scale, 0x400 mode…) |
| +0x248..+0x27C | loops, pan, volume/userParam (+0x25C; userParam = >> 16), pitch and deviation, min/max/scale (+0x268/+0x26C/+0x270), mode (+0x274), +0x278 (if it is 0, `LHSamplePlay` gives an error at 0x100118FD), ambience frequency (+0x27C) |

The override flags at +0x244 (the old `unknown10/11`) are read by the audio DLL at 0x10011420: 0x1 pitch (+0x260),
0x20 volume (+0x25C), 0x80 minDist (+0x268), 0x100 maxDist (+0x26C), 0x200 scale (+0x270) for
QSWaveMixSetDistanceMapping, 0x400 play mode (+0x274). The defaults come from `LH_SamplePlayOptions` (0x10010E90):
1, 9999, 0.3, play mode 3.

Wave formats in the data:

| format | where |
|---|---|
| RIFF 16-bit PCM, 22050 Hz (11025 in 1 of InGame; 44100 in 210 of VillagersBanter and 1 of Scriptsfx) | almost all effects |
| RIFF MS-ADPCM (0x0002) | high, rain, wind, stream, 3 of InGame, 27 of spells, 2 of Scriptsfx, 23 of VillagersBanter |
| RIFF **MPEG-2 layer II (0x0050)**, 22050 Hz, 64 kbps mono | all of HelpSprites (1923) and villagers (1328); Guidance 129/227, SpellDialogue 9/35, Scriptsfx 21, country 17, InGame 2 |
| raw MPEG layer II frames | the 58 banks of `Music\` and MissionariesVerse1..3 ([below](#music-banks-mp2-segments)) |

- `LHBankGetNumberOfSamples` 0x10002E90 is the low u16 of the sample table's first u32 (bank+8). The high u16
  (bank+0xC) counts the atmos samples and is 0 in music banks. `HelpDude::SaySentence` uses it at 0x5BB389.
- `LHBankRegister` opens the file with `LHReleasedFile::Open(path, 2)` (0x1000228A..0x100022FE). It returns 0 if the
  file is missing or has no `LiOnHeAd`, no `LHFileSegmentBankInfo` (0x10002368), no `LHAudioWaveData` or no
  `LHAudioBankSampleTable`. It looks blocks up by name (fn_100183A0). With inMemory 0, bank+0 = 1 and the file stays
  open at bank+0x18 (0x10002792).
- It reads the sample table as n × 0x280 bytes without checking the block size (0x100026AA..0x10002733).
- bank+4 is the 3rd u32 of `LHFileSegmentBankInfo` (the music mark, 0x10002357..0x100023BD).
- Anim tables: u32 rows (+0x130), u32 width (+0x12C), then rows × width s32 (+0x124), read at 0x100027DD /
  0x100027F8. The wave-number lists are read by `LHFileSegmentAnimArrayWaveNumbers` 0x100028E8. Every bank of the
  game has width 6.
- `LHFindAttribRow` 0x10014420: a row matches when each of its first width − 1 columns is the wildcard or equal to
  the key's (0x100145A1..0x100145B6).
- Music-bank getters: `LHGetFirstBankSample` 0x10002EB0 gives segment 0 (bank+0x14). `LHBankGetMusicGroupId`
  0x10002F30 gives −1 for a bank that is not music (0x10002F4A). The distance getters' "no distance" value is −1.0f
  ([0x10030304], 0x10002F05 / 0x10002F25).
- In an ambience bank, a sample whose ambience frequency (+0x27C) is 0 is a loop sample (fn_100011B0).

#### .sas and animation tables

`Data\SmallSounds.SAS` is the text of the sound events per animation clip (`LoadAllAnimations` 0x550180). Together
with the `LHAudioAnimArrayTable` tables of each .sad, it feeds `SamplePlayAnimEffect` 0x42A4B0 → 0x10014A20. The key
has 5 columns and there are 3 actions (0 play, 1 stop, 2 release). The clips and the banter are in
[Clip sounds](../audio/world-sounds.md#clip-sounds). The original has no other audio format.

- Tree::Draw keys: the bend {c, *, *, 10, 75} (0x74B009, track 0, `push ebp = 0` at 0x74AFE1) and the ambient
  rustle {*, *, 20, *, 70} (0x74B25C, track 1, `push 1` at 0x74B1FA). Both measure |tree − camera| as the distance
  (0x74AFFD / 0x74B250).

#### Music banks: MP2 segments

**Faithful.**

- There is no LHStream and no loose files. Each music .sad is a normal `LiOnHeAd` bank with the music mark.
- Its sample table consists of **segments** `c:\windows\temp\sectNNNN.mpg` of MPEG-1/2 Audio Layer II, at 22050 Hz:
  128 kbps stereo, 96/64 kbps mono, and 160 kbps in the Outro.
- Each segment has **21 frames = 24192 samples (0x5E80) = 1.0971 s**. They are contiguous and frame-aligned: 13457
  segments were checked without a single failure. A segment's 0x5E80 samples (21 Layer II frames of 1152) is the
  constant the marker clock uses (0x1000DBF7).
- From **segment 0** come:
  - the sync group (u16 +0x118);
  - the flags (+0x244) that `LHMusicPlay` applies: 0x20 volume (u16 +0x25C), 0x40 loops (+0x248, −1 = forever),
    0x80/0x100/0x200 3D min/max/scale (+0x268/+0x26C/+0x270);
  - the Hz (+0x128).
- **Markers**: fn_1000D9E0 reads the description (+0x140) of each segment, of the form `"!<sample>=<label>!…"`, and
  builds nodes {chunk i+1, sample, label}. It walks the segments from last to first and inserts each node at the
  head. Only `MissionariesVerse1..3.sad` use them (L1..L12 are the verses, P the hits).
- Marker parsing: after the digits (isdigit, atoi) exactly one character is skipped, the '=' (0x1000DA87). The label
  runs to '\0' or '!' (0x1000DA92..0x1000DAA2), and parsing repeats while the label ended at a '!' (0x1000DAF0). A
  node is malloc(0x30) with the label in 0x20 bytes at +0x10.
- `LHMusicPlay` takes these from segment 0: volume 127 (0x1000E0C5), or u16 +0x25C with flag 0x20
  (0x1000E1D6..0x1000E1ED); loops = the requested ones (opts+0x18, 0x1000E13E), or i32 +0x248 with flag 0x40
  (0x1000E1F1..0x1000E206); the distances, in the order 0x100 max, 0x80 min, 0x200 scale (0x1000E2D9..0x1000E338),
  otherwise the options' +0x30 / +0x2C / +0x34; the Hz (+0x128), copied to channel +0x50 (0x1000E210).
- With the retail banks `LHMusicGetTotalGroups` (sys+0x40, read at 0x100027B5) is 13.
- Banks:

  | banks | group | other data |
  |---|---|---|
  | the 24 of `align` | **1** | 430 segments (471.1 s, 7 814 232 B), parallel versions of the same melody |
  | chants | 6..13 | 0x3C0, loop −1, 3D 30/120/2 |
  | citadel | 4 | |
  | CreatureFight | 5 | |
  | the rest | 0 | |

  .sad volumes: Script01 60, Script02..04 65, Epic04 80, Sleg 65, Khazar 65, Gregorian 40, Circus 60, CreatureGuide
  70, Gregorian3D 50, Circus3D 60; the others 127. 3D distances: PiperTune_M 15/100/4, Pipercave_M 25/80/4,
  SingingStonesA 100/200/4, MissionariesBackground 30/100/4, Whistle* 30/80/2, Circus3D 30/120/3, Gregorian3D
  60/120/4.

#### Texture files: 16-bit colour and ARGB4444

- A `.raw` colour file created without the alpha flag 0x40 loads through fn_00837400, which cuts it to 16 bits
  (`[tex+0x10] & 0x40` = 0 at 0x838079 / 0x838D37, `je` 0x8374D2) on the switch [0xEDD46C] (0 on the target
  hardware): the 555 branch 0x837765..0x83779F (Pack555; D3D samples Unpack555). Only a 0x30000-byte colour file gets
  there (fn_00837300 0x837318); another goes through fn_0087F980 (0x8380A0).
- Of the 69 calls to Create 0x8379E0, the ones with a file and no 0x40 are Sun.raw (flags 1, 0x81E844..0x81E851) and
  the saved games' pictures screenshots_lores_%i_map_0.raw / screenshots_hires_%i_map_0.raw (flags 1, fn_00784070
  0x78408F, fn_00784640 0x78466A). The landscape converter fn_008227A0 0x822874 is a command-line mode, not the game.
- ARGB4444: fn_00837400 takes a 0x30000-byte colour file (0x837318) and reads min(length, 0x10000) bytes of its
  alpha (0x837600, 0x7BCEC9). fn_0081FAA0 cuts human_shadow (0x81FCDD). The colour must be read before xa.raw.
- The halving of [0xEDD470] (step 6, 0x8374BB) is a further step of the same loader, taken only in low-resolution
  mode. [0xEDD470] is `LH3DTexture::g_b_use_low_res` (0 after start-up): when it is set and the format argument is not
  3 (0x837497..0x8374B9), the source step becomes 6 bytes (every other RGB pixel) and 0x300 bytes are skipped per row
  (every other 256-pixel row; 0x8374BB / 0x8374C3); the alpha pass does the same with step 2 (0x83761E..0x83763F).
  Its writers: fn_008237B0 sets it at start-up (0x823A94) when the registry's "UseHighTexture" is 0 (0xC3851C, key
  0x9CAFF8); fn_00823AD0 (0x823C69, from `start_system` 0x643026) sets it from the detail-level table [0x9A38E0 +
  4·level] = 1, 1, 0, 0, 0, 0, 0, so only levels 0 and 1; the save-game room 0x7923D0 and `ScriptedScreenShot`
  0x82A8E0 clear and restore it around their captures [V].

#### 16-bit bitmaps (.16b, .555)

✅ Checked on all 111 such files under `_gamedata\frozen` (92 `.16b`, one `.16B`, 18 `.555`; the layout was first
given by the openblack project wiki's 16b Bitmaps page): the header is four u32 (0, width, height, and a value that
varies between files, 0x03080D20 in 83 of them; that wiki says the game overwrites it when loading, which was not
checked here), then width × height 16-bit texels; every file is exactly 16 + 2 × width × height bytes. openblack reads them with
`src/Common/Bitmap16B.h` (the sky's `.555` layers and the temple's outside `.16b` pictures).

<a id="modified-data-packs"></a>
### The installation's pack

The user's `AllMeshes.g3d` (49 MB) is a mod, not the original pack. What follows are its differences from the
original (embedded textures, rock origins, villager textures) and what was found out about the villager meshes.

- The user's pack is a mod (626 meshes, textures 0x01–0x70). The original pack (13 MB) is the frozen copy's
  `Data\AllMeshes.g3d`, which every check reads; the user's pack is played only as a mod.
- References for original meshes: the Creature Isle pack (`...\CreatureIsle\Data\AllMeshes.g3d`, **different
  indices**: check its `AllMeshes.h`).
- The `Ultimate\Data\AllMeshes.g3d` pack is another mod (704 meshes).

### Embedded textures

- The palm trees (meshes 586–589) carry their texture embedded with id **0x1001**, but their material asks for
  0x85–0x88, which do not exist in the pack. Ultimate repeats the pattern. In the original game they look fine.
- In `LH3DMesh::Create` (0x806460) the embedded skins are registered with `fn_008379E0`, as
  `LH3DTexture::Create(pixels, 4, id, ...)` (pixels = skin + 4, 0x806571..0x806590): the skin's u32 (0x1001) is the
  texture's whole id (stored at +0x18 at 0x837A16 / 0x837A39, the key `GetThisTexture` 0x838480 compares at
  0x838490), and the 4 is the kind (`& 0x80` → +0x13C at 0x837A63, `& 0x3F` the format case, 0x837A8B..0x837AB1). A
  mesh with flag 0x1000 instead looks its textures up by id with `GetThisTexture` (0x80653C..0x806553) [V].

### Mesh origin (floating rocks)

- The mod's rock meshes have their origin shifted relative to the original ones: `MSH_Z_SPELLROCK01` lowest vertex
  +0.8 (original −0.6), `MSH_BOULDER3_LIME` +0.2 (original −0.2).
- The script altitudes are designed for the original meshes, so with the mod they end up in the air (in Land1, 74 of
  243 static objects float more than 5 cm). The original does not correct it.
- In addition, many of the mod's rocks end up resting on a tip because of their rotation: even though the lowest
  vertex touches the ground, they seem to float.

### Villagers: meshes and textures

**Meshes**

- `MSH_P_*` meshes 413–524 of the base pack (geometry identical to Creature Isle's, so the user's mod does not touch
  them). Each tribe and sex has **its own mesh**, in 3 levels: `_1` ~260–300 vertices, `_2` ~110–150, `_3` 26–40.
  There is one texture per tribe, shared by man and woman (0x4E–0x5C). Some meshes repeat geometry with another
  texture (TIBETAN=TIBT, JAPANESE=JAPN, SHAOLIN_MONK=JAPN_M_A_1, TAN/WHITE girls, INTRO_M=CULT_PRIEST).
- `MSH_P_INTRO_M`/`_F` (483/484) **do not have more polygons**: 258 v / 348 t, like a normal villager. INTRO_M is the
  CULT_PRIEST geometry with texture 0x59; INTRO_F uses the female villagers' skeleton. Their textures are 256².
  `INTRO.bik` is a pre-rendered video: its models are not in the files.
- Skeleton: 110 of the 112 meshes have the same 22-bone hierarchy (EGPT_M_B_2 has 21), and all the `M_P_*`
  animations of AllAnims.anm (232) are 22-bone. The rest poses vary a little (groups: 85 meshes, 12 female, 10,
  CULT_PRIEST+INTRO_M).
- Which mesh is drawn: the original always draws LOD 1 (the LevelOfDetail loads are disabled), and LOD 1 is the
  **high** mesh (the boned draw takes its slot +0x98): `highDetail` / `ChildMeshHigh` for villagers (`_1`, about
  12 KB of mesh against 5.7 KB for the standard one) and `high` for animals. The skeleton, bounds and picking stay on
  the standard mesh ([the model level of detail in v1.42](../rendering/visible-blocks-and-lod.md#the-model-level-of-detail-in-v142)).

**Textures**

- All are **native** 256² atlases. The ones the user's pack has at 512 or 1024 are upscales with duplicated pixels
  (the error against doubling their half is < 1.5 levels), except 0x5A and 0x47 (native 512).
- INTRO_M/F look finer because their 256² texture is of **a single** character (the villagers: 4 per atlas). No pack
  (base, Creature Isle, Ultimate) comes with better ones.
- **The Norse atlas of the user's pack (0x5A, 1024 px) is not the original**: it has the intro characters painted on
  it (the face of the blonde woman of INTRO_F, the bearded man, one with a red shirt and jeans). The Creature Isle
  Norse atlas (skin 0x74 of its pack, `MSH_P_NORS_F_A_1` = 580 there) has the original clothing (dark dress, men in
  black with a belt). That is why the female villagers of Land1 (a Norse village) look like "the ones from the
  intro": the mesh (`NORS_F_A_1`, 498) and the clip (`M_P_Walk_Woman`) are the correct ones.

### Future ideas (need the SDK)

These ideas from the user's task list wait for a real mod SDK, built once the game is reconstructed
([The mod loader](../project/mod-loader.md)). They are **not** pending engine work: the engine stays faithful to the
original, and each idea would be off by default.

- **Better physics** (see [Physics](../physics/physics.md#pending)): building pieces with a collision mesh, so that
  they hit other objects; building pieces that can be grabbed with the hand; more improvements as they come up.

### Sources

- The shipped `.sad`, `.sas`, `.anm` and `.g3d` files (scanned whole), the audio DLL and runblack.exe read with the
  xref index.

## openblack

- **Clips**: `components/anm` reads `AllAnims.anm`; a clip is `ecs::ClipId(index)` (the manager stores hashed ids).
  The keys are evenly spaced, as in the original.
- **Banks**: `components/pack` (`PackFile`) reads the `.sad` blocks; `AudioBankInfo` (the 3 u32) and
  `PackFile::IsAudioMusicBank()` are ✅ faithful (0x10002240, `LHIsMusicBank` 0x10002EE0). `Audio/Game/Banks.cpp` still
  recognises music by the ".mpg" extension of the first name. The anim-effect tables of each bank are read once when
  it is registered ([The anim effects in the core](../audio/world-sounds.md#the-anim-effects-in-the-core)), and the
  `.sas` file once into the resources (`ClipSoundsManager`, `audio::clip_sounds::Parse`). Our own decoders play the
  PCM, ADPCM and layer II waves, bit-exact against the output they replaced.
- **Music**: `MusicBank` (`src/Audio/Engine/MusicBank.{h,cpp}`, ✅ faithful) covers `Register` (0x10002240), the
  segments, the group (0x10002F30), the distances (0x10002EF0/0x10002F10), the flags, the Hz, `GetVolume`,
  `GetLoops`, `GetDistanceMapping` (0x1000E30A..0x1000E338), `ParseMarkers` (0x1000D9E0) and `ReadSegment`. The file
  stays open and each segment is read when used. A marker without a label gives an empty label **⚠️ approximate**: it
  does not occur in the data.
- **Textures** (`src/Resources/Loaders.cpp`): the 16-bit 555 cut and the ARGB4444 cut of the files above; the colour
  is read before its alpha file. The low-resolution halving of [0xEDD470] is not ported: openblack has no low-detail
  texture mode (the flag is 0 at detail level 2 and above).
- **Embedded textures**: openblack uses the mesh's embedded skin when the material does not find its texture.
- **Villager meshes**: openblack draws LOD 1, the high mesh, at detail level 6, as the original does at every level
  (✅ faithful there). The villagers' and animals' high meshes are drawn only at detail level 6, the top setting, and
  their standard meshes below it: a 🎯 chosen difference chosen by the user (2026-10-15). How the clips are played:
  [Animation](../beings/animation.md).

### Test hooks

- `OPENBLACK_TEST_SPREAD_REACTIONS=<turn>` (owned by [Animals](../beings/animals.md#test-hooks)).

## Pending

- **Floating rocks resting on a tip**: planned solution: physics (**not checked** whether it has already been done).
  Only with the user's mod pack; the original's rocks rest on their base.
- **The palm trees' embedded skin**: 0x1001 is the whole texture id (above). Still open: how the palm trees'
  material reaches the embedded skin in the original (by skin index in the mesh's own texture array [mesh+0x3C], not
  by id) is not traced.
- **The a/b fields of `LHFileSegmentBankInfo`** (inferred: editor version): no reader read yet; 🧪 verify with xref
  (`xref.py refs` on bank+0 / bank+4 readers in the audio DLL).
- Not ported: the low-resolution textures (`g_b_use_low_res`, detail levels 0 and 1), a port row.
