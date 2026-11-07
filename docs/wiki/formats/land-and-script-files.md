# Land and script files

## Summary

The files a land is made of: the landscape (`.lnd`), the map script (`Scripts\LandN.txt`) and the lines the game
writes back into it, the camera editor's file `Data\camera.edt` (fixed camera places and the tracks a script camera
or a walking thing follows), and the dance files (`.dnc`). This page owns the camera track file; following a track
in play is in [The script camera](../camera/script-camera.md) and
[WALK_PATH](../beings/villager-speed-and-scripts.md#walk_path-177-of-a-mobileobject).

openblack reads all of them. The camera tracks, the dance files and the script lines are ✅ faithful to the
original; the landscape reader also takes the extensions of the BWLandEditor maps, which leave the original maps
unchanged.

## The original

### The file

The camera editor's file, `.\Data\camera.edt`.

- `GCameraEditor::CreateSegFile` 0x445530 opens it once (the global `LHFile` 0xC59CF4). `LHFile::GetSegment` 0x7BDDD0
  finds a segment by name, and `LHFile::GetSegmentData(ptr, size, -1)` 0x7BE040 reads its data in order
  (the camera zone files use the same reader, see [The script camera](../camera/script-camera.md)).
- Format: `"LiOnHeAd"`, then segments of a 32-byte name (zero-padded), a u32 size and the data.
- On the original disc (150536 bytes): `EDITOR` (58084 bytes of editor names; the game does not read it),
  `Cam0`..`Cam555` (32 bytes each) and `Track0`..`Track52`. Creature Isle ships its own file.
- **`Cam%d`**: `GCameraEditor::LoadCameraFromHD` 0x446FE0 copies the 32 bytes as they are: the position (3 floats),
  the focus (3 floats), and two floats that are 0 and −1 in all 556. `CONVERT_CAMERA_POSITION` 0x6ED200 pushes floats
  0..2, `CONVERT_CAMERA_FOCUS` 0x6ED270 floats 3..5. When the segment does not exist the original leaves its stack
  buffer uninitialised.
- **`Track%d`**: `ScriptedCamera::Create` 0x447060 makes a 12-byte `ScriptedCamera`: a u32 (0 in every track; it goes
  to +0) and two `LH3DWay`, each preceded by its u16 size. They are copied to new memory, `LH3DWay::AdjustPtr` 0x844570
  rebuilds their pointers and each gets an `LH3DWay::Running` (0x843ED0): +4 the one for the camera
  **position**, +8 the one for the **focus**. A missing track logs `"Cannot load track No %d"` (0x4470E1) and returns
  0.

<a id="lh3dway-0x24--44n-bytes"></a>
#### `LH3DWay`

A way is 0x24 + 44·n bytes:

| Offset | Contents |
|---|---|
| +0x00 | u16 size; +0x02 u16 = 0x63 in all of them |
| +0x04 | n points |
| +0x08 | float 0.26 in all of them (nothing below reads it) |
| +0x0C | recomputed by `AdjustPtr`: Σ, over the segments with distinct times, dt·(v0 − a·t0 + a/2) with t0 = time[i]·0.001, dt = time[i+1]·0.001 − t0, a = (v[i+1] − v0)/dt; 0.1 if it comes out 0; summed and stored as a float (0x8445AF..0x8445F8). It is not the chord lengths. The `WALK_PATH` of a Living uses it ([Living, not ported](../beings/villager-speed-and-scripts.md#living-not-ported)); that of a MobileObject does not |
| +0x10 | duration in ms = ftol(last time) (12212 in Track20, 12884 in Track21) |
| +0x14..+0x20 | pointers (from the editor; `AdjustPtr` rebuilds them from +0x24) |
| +0x24 | n points (vec3), then n pairs of handles (vec3 ×2: the two inner handles of segment i→i+1), n times in ms, n speeds (units/s) |

Each segment i is a cubic Bezier (P[i], handle[i].a, handle[i].b, P[i+1]).

<a id="lh3dwayrunning-0x20c-bytes"></a>
#### `LH3DWay::Running`

0x20C bytes: +0 the current segment, +0x04..+0x200 a table of 128 floats, +0x204 the last parameter t, +0x208 the
way.

- `fn_00843F00` (the table of segment s): the total length of the Bezier in 127 steps (t = i·(1/127) [0x8D8690],
  i = 0..127, from P[s]); then table[i] = 127·accumulated length(i)/total for i = 1..126, table[0] = 0,
  table[127] = **1.0** (+0x200).
- `fn_00844280(sample, &out)`:
  - The segment is the first i with time[i+1] ≥ sample (a new one rebuilds the table). The search comes first
    (0x844298..0x8442C6), so the segment is updated even at the ends. It reads past the array for a time after the
    last one (0x844298).
  - sample ≥ duration → the last point (t not written); sample ≤ 0 → the first point and t = 0.
  - Otherwise, the chord fraction (0x844335..0x844385): L = |P[s+1] − P[s]| (the chord, `fn_008433E0`:
    sqrt((z·z + y·y) + x·x), as `fn_004A1BA0`), v0 = speed[s], dt = (time[s+1] − time[s])·0.001,
    τ = (sample − time[s])·0.001, a = 2(L − dt·v0)/dt², **u = τ(v0 + aτ/2)/L**: a constant acceleration that covers
    the chord exactly at time[s+1].
  - Then the table lookup (0x844387..0x844418; with k = 127, hi is the last t, +0x204): k = ftol(127u),
    lo = table[k], hi = table[k+1]; if u == lo, t = lo. If not, the loops that search for the entry move a pointer
    but do **not** change lo or hi, and t = w·lo + (1 − w)·hi with w = (hi − u)/(hi − lo), which is u again (with
    the rounding of the table's scale, up to ~4·10⁻⁴ in t with the FPU at 24 bits). So the table does not
    reparametrise anything.
  - Outside the table (u < 0 or u > 1, which only happens if v0 > 2L/dt; in the shark tracks every speed is 10) the
    original reads other fields of the object.
  - out = the segment's Bezier at t; t is left at +0x204.
- `fn_008439C0(segment, t, &out)` is the bare Bezier (the one `WALK_PATH` uses). It sums x in a different order from
  y and z: (((B2·3ut² + B1·3u²t) + B3·t³) + B0·u³).

<a id="the-dnc-format-groupbehaviourloaddance-0x5978e0"></a>
### The .dnc format

`GroupBehaviour::LoadDance` 0x5978E0 reads a dance file little-endian, through an `Archive` over `LHOSFile` (opened
at 0x59791B; 0x400 bytes read at 0x59795A). What a dance does with it is in
[Dances](../beings/dances.md#the-dance-table-and-the-files).

```
u32 version            -> the global 0xD06AF8 (save loading reads it too)
u32 keyFrameCount
keyFrameCount x {
  f32 time             -> DanceKeyFrame +0x14
  u32 editFlags        -> +0x20 (1 on the first key frame of the game's files)
  u32 actionCount
  actionCount x {
    u32 groupCount; groupCount x u32 group   (each pushed on the action's list head, 0x597AF9..0x597B1E)
    u32 type           -> DanceKeyAction +0x1C
    56 bytes argument, read byte by byte (0x597B3F..0x597B4B)
  }                    -> appended to the key frame's actions (0x597B58..0x597B83)
}                      -> each key frame inserted before the first one whose time is not less than its own
                          (`fcomp; test ah, 0x41`, 0x597BC9..0x597BFD): of two at one time the later read comes first
f32 time (+0x28)       -> the clock it starts at
u32 numGroups (+0x40), u32 editorValue (+0x44), u32 editorValue2 (+0x48), u32 length (+0x74)
if version >= 2: f32 groupAngle (+0xAC)
groups 0..numGroups-1: a group past the list's end is made (DanceGroup 0x17C bytes, its Index = the count, appended,
  0x597C93..0x597D11); each one Initialise()d (0x597D32)
every key frame with time <= the clock done in order (0x597D4A..0x597D66), then UpdateNumeratorsAndDenominator
  (0x597D6A)
if version >= 1: numGroups x { u32 length; length bytes } -> each group's Name (+0xA8), a null after it (0x597E12)
```

- Before reading, the key frames it had are deleted (0x5979C2..0x597A0F).
- A file that does not open leaves everything as it was (exit 0x597E66): a new dance keeps the one group of
  `Dance::Initialise`, no key frames and numGroups 0.
- Nothing checks a short file or bytes after the end. The `Archive` reads from a 0x400-byte window (`operator>>`
  0x712330 / 0x712230 / 0x712430: the value at +0x10C + offset +0x50C; `Archive::Flush` 0x711F00 moves the rest to
  the front and reads on). Past the end of the file it returns whatever the window still held (on the first window,
  uninitialised stack), which can't be known.
- An action's groups are a stack: the action is done to the last listed first.
- The `DanceGroup` constructor 0x50CE40 names a group "Untitled %d" from the global counter 0xBE8290; version 0 files
  keep those names.
- The shipped files (`Scripts\Dance`): the 20 files of the table read whole, with no trailing bytes, their times
  ascending and distinct, no two key frames in one half second; every time a multiple of 5. Action types used: 0, 1,
  2, 3, 4, 6, 11, 12, 14, 15, 16, 17, 18, 20, 21, 22, 23. Shapes set: 0, 1, 4, 5, 6, 7, 10, 19. The highest group
  number used is below each file's numGroups.

### The line format

How the game writes a map-script line (when a land is left, see
[The writers](../scripts/land-change-and-saving.md#the-writers)).

- `GSetup::GetCommandAsText(cmd)` 0x715130 → `LHScriptCommandX::GetCommandAsText` 0x7E7FB0 builds a **printf
  format** from the command table 0xC20F00: 105 entries of 16 bytes, the name pointer and then 12 inline type
  letters (spaces are padding). The order: strcpy of the name and strcat "(" (0x7E7FBE..0x7E7FD5); the 12 letters,
  each through (letter − 'A') in the byte table 0x7E8078 and the jump table 0x7E8064 (0x7E7FE1..0x7E802A); the last
  two characters cut if anything was added (0x7E8035..0x7E8042); then ")\n" (0x7E8047, literal 0xC22098).
- Per letter:

  | Letter | Appended | Meaning |
  |---|---|---|
  | `A` | `%s, ` | a text without quotes: a position, or a name such as a villager's type (`CELTIC_FISHERMAN`) |
  | `L` | `"%s", ` | a quoted string |
  | `N` | `%d, ` | an int |
  | `F` | `%f, ` | a float, printed with 6 decimals |
  | any other | nothing | padding |

  The per-letter literals: A `%s, ` 0xC34DDC, L `"%s", ` 0xC34DE4, N `%d, ` 0xBE83AC, F `%f, ` 0xC34DD4.
- The last `, ` is cut if anything was added, then `)\n` closes it. A command with no parameters is `NAME()\n`.
- **A position** is `MapCoords::ConvertToText` 0x602880: `"%0.2f,%0.2f"` of x then z, with the quotes in the
  literal (0xBF4198). Each value is `fixed × 10.0f × 2^-16` on the x87 at 24 bits (`fild`, `fmul [0x930050]` (10.0),
  `fmul [0x8AC41C]` (2^-16)), so the first product is rounded to a float and the second is exact.
- The exe's CRT (`sprintf` 0x7C57D2) rounds the digits **half up**, on the number's 17 significant digits (the steps
  are in [Hand animation and placement](../hand/animation-and-placement.md), "how such a number is written"): 0.625 m
  is written `0.63`, and 2^-7 is written `0.007813`. Today's C runtimes round half to even here.
- Example (Villager, CREATE_TOWN_VILLAGER `NAAN`): `CREATE_TOWN_VILLAGER(1, "3079.69,3117.83", CELTIC_FISHERMAN, 26)`.
  The shipped Land2 and Land3 use this form.

### Data formats

- **Mesh pack** `Data\AllMeshes.g3d`: a Lionhead pack of ~626 L3D meshes and their DDS textures. The mesh number is
  the index in the pack, which matches the enum in `Data\AllMeshes.h` **only for that pack** (Creature Isle and
  other packs have other indices; see [Media files](media-files.md#the-installations-pack)).
  - ✅ **Segment order** (the original pack `AllMeshesOriginal.g3d`, 13 311 150 bytes, walked from `"LiOnHeAd"` to
    its last byte; the order was first given by the openblack project wiki's Mesh Pack page): every segment is a
    32-byte zero-padded name, a u32 size and the data. First the 110 textures, named by their id in lower-case hex
    from `6e` down to `1`; then `INFO` (4100 bytes: a u32 count, 110, then id/type pairs, 55 of type 1 and 55 of
    type 2); then `MESHES` (`"MKJC"`, a u32 count, 626, then the offsets of the L3D meshes from the segment's data
    start); then the 110 low-resolution textures, `LOW6e` down to `LOW1`.
  - Their page's `26, 53, 106, 0` before `MKJC` is this file's `MESHES` size field (6 960 410 bytes), not a
    fixed marker.
- **L3D**: a header of 19 u32 (magic, flags, size, submeshCount, submeshOffsets, bbox[8], another, skinCount,
  skinOffsets, extraCount, extraOffset, footprintOffset). Embedded skins start with a u32 id followed by 256×256
  16-bit pixels. The second u32 is the flags, not a model size as the openblack project wiki's Mesh File page says:
  in the pack it holds bit sets such as 0x23000 and 0x23107, and the third u32 is the mesh's size (equal to the gap
  to the next mesh's offset, checked on meshes 0, 1 and 100).
- **Effects**: `Data\Spells\ZSpellFiles\*.zzz` = zlib from byte 4 onwards; inside, a text properties file
  (`BEGINCLASS` / `PROPERTY`). Examples: `SF_GripLandscape`, `SF_MultiPickUpWood/Food/FoodFish`, `SF_MultiPutDown*`.
- **Loose textures** `Data\Textures\X.raw` (256×256 RGB) + `Xa.raw` (R8 alpha). Sprite sheets of 8×8 cells.
- **Map scripts** `Scripts\LandN.txt`: the executable's command table (0xC20F00, above) gives the name and parameter
  types (`A` position, `N` integer, `F` float). For example `CREATE_MOBILE_STATIC` (the entry at 0xC211A0, after
  CREATE_MOBILESTATIC's at 0xC21190) = `ANFFFFF` = (pos, type, altitude, X angle, Y angle, Z angle, scale).
- **`Scripts\info.dat`**: the object tables (pots, trees, mobile statics...), see [info.dat](info-dat.md).

### LND and BWLandEditor maps

BWLandEditor is Daniels118's map editor (Java, GPL-3). Part of its code is ported from openblack: InfoConstants,
L3D/G3D and an old version of `fs_terrain`. What follows comes from the editor's code (not from the original
executable) except where stated otherwise.

#### How it reads the LND

The same as openblack, plus three extensions:

- At the end of the file there may be the blocks `EXT0` (u32 size of the whole block = 10, u8 version, u8 altitude
  bits 8-16) and `META` (u32 data size, editor data). With more than 8 bits, the high bits of the altitude go in the
  low bits of `saveColor`.
- The grid can have up to 128×128 blocks and more than 255 blocks. The header table only covers 32×32 and indices
  < 256, so the block's own `blockX`/`blockZ` must place it. In the 21 original `.lnd` files the table matches those
  fields (`lnd_check.py`).
- `LNDCountry` +0 (`unused`, the 2015 `lnd.h`'s mTerrainType): no reader in either lookup of the country (block
  texture 0x8733BF, `GetTerrainMaterial` 0x804CF0). In every country of every shipped land it is 0xCCCCCCCC (the
  compiler's uninitialised fill), except the added countries of 9African and 9DEMON, which hold their own index.
- The block record: index at 2312 (0x908), mapX 2316 (0x90C), mapZ 2320 (0x910), blockX 2324 (0x914), blockZ 2328
  (0x918); the exe reads 0x90C with the view line's x and 0x914 as the `g_index_block[x]` index.
- The editor corrects a `mapX`/`mapZ` that does not agree with `blockX`/`blockZ`.
- The openblack project wiki's Land Island page has two errors here: its block fields `mapY, mapX, blockY, blockX`
  are mapX, mapZ, blockX, blockZ in that order (the exe's reads above), and its country `TerrainType` at +0 is the
  unused fill described above, not an enum.

#### Cell bytes

- **`flags`, according to the editor.** Bit 0 = "transparent"; bits 1-7 = ambient sound: 0 nothing, 2 splashing,
  3 ocean, 4 slow waves, 5 lake, 6 coast, 7 fast waves, 8 jungle, 10 wind, 12 desert, 14 birds, 16 forest, 18 river.
  The odd ones above 8 are variants of the preceding even one. The original reads the zone as `(flags >> 2) & 0xF`
  (`Terrain::GetAtmosType` 0x7352B0): the editor's codes are `type << 1`. The table and its use are in
  [Objects](../world/objects.md) ("Ambience (atmos)").
- **`properties` (+6).** Bits 0-3 = country, bit 4 (0x10) = hasWater, bit 5 (0x20) = coastLine, bit 6 (0x40) =
  `unused6`, bit 7 (0x80) = split (diagonal, see [Terrain height](../world/terrain.md#terrain-height)). Bit 6 has no
  reader and no writer in runblack.exe (no test of 0x40 after any load of `[cell+6]`, no shift that pulls it out, no
  dword test of 0x400000 on a cell); bw1-decomp has no name for it. It is set only in 4Japanese (13 cells), 5Egyptian
  (6106), 8Aztec (60), 9African (28) and Ultimate.lnd (1280), mostly on water cells with the full-water flag at
  altitude 0: an editor marker, not "deep water". To separate the open sea from inland water, `lnd_water.py` groups
  the connected cells with water or without a block: those touching the map edge or the void are sea; the rest, lakes
  or ponds.

#### Low-resolution textures

An atlas of 4×4 subtextures of 64×64, one per block, 4 texels per cell, with X and Y swapped. The "unknown" in its
header is the number of tiles handed out on the page (`tileCount`). `iu_lrs`/`iv_lrs` are integers (0/64/128/192).
The record and its DXT3 decode as the game reads it back: [The low-resolution
pages](../world/terrain.md#the-low-resolution-pages).

#### Test maps and analysis scripts

`lnd_make_tests.py` generates three maps and their scripts (start with `-s` and the absolute path of the `.txt`):

- `Land1_ext`: Land1 with the editor blocks at the end; identical to Land1, height at (1788.4, 2710) = 28.9173050.
- `Land1_hi`: 10 bits and doubled altitudes; height 57.8346100.
- `Land5_x2`: Land5 twice, a grid of 60, 374 blocks.

`.lnd` analysis scripts: `lnd_check` (block table against blockX/blockZ), `lnd_make_tests` (test maps),
`lnd_beaches` (sand next to water, materials 6 and 11), `lnd_zones` / `lnd_countries` / `lnd_find_country <lnd> <n>`
(sound zones, countries and the median position of a country), `lnd_materials` / `lnd_colours` / `lnd_tile_check` /
`lnd_decal_metric` (256×256 RGB555 materials), `lnd_water` (bodies of water), `lnd_hash` (FNV-1a of the materials).

#### Differences not checked in the original

- The editor chooses the material with `min(altitude + noise/4, 255)`; the original does it **per texel** with
  `min((h >> 8) + noise, 255)` and the altitude weighted with cones (resolved, see
  [Coast](../world/sea-coast-and-rivers.md#coast)), and openblack follows the original (`3D/BlockTexture`).
- openblack's L3D reader takes the footprint width and height from the header; the editor reads them per entry.
- The editor reads Creature Isle's info.dat (627250 bytes; longer tables in InfoConstants.java L23-33); openblack
  does not yet.

### Sources

- bw1-decomp and the xref index for the camera file, the dance reader and the line writer; the shipped
  `Data\camera.edt`, `Scripts\Dance` and `.lnd` files; the BWLandEditor sources for the LND extensions.

## openblack

- **The camera file**: `components/edt` (`EDTFile`) reads `camera.edt`; `src/3D/CameraEdits.{h,cpp}` gives the
  `Cam%d` places (a missing segment gives (0, 0, 0), where the original leaves its buffer uninitialised), and
  `src/3D/CameraTrack.{h,cpp}` (`camera_track::Bezier`, `WayRunner`) follows a way: ✅ faithful, the Bezier summed in
  the original's order for x; `camera_track::WayLength` is `AdjustPtr`'s +0x0C sum (0.1 when it comes out 0). Outside the table openblack uses t = u (the original reads other fields there). Tests:
  `test/test_camera_track.cpp`.
- **A thing walking a track** (`WALK_PATH`): the component `src/ECS/Components/WalkPath.h` (`WalkPath`: the track,
  its `camera_track::Walk`, and whether a living walks it in its own state) and `WalkPathSystem`
  (`src/ECS/Systems/WalkPathSystemInterface.h`, `Implementations/WalkPathSystem`) move it each turn. The script side
  is in `src/CHLApi.cpp`: `WALK_PATH` (177), `GET_WALK_PATH_PERCENTAGE` (179) and `CONVERT_CAMERA_POSITION` /
  `CONVERT_CAMERA_FOCUS` (81 / 82). The row by row status is in
  [the camera paths progress](../project/progress/camera/camera_paths.md).
- **The dance files**: `components/dance` reads the `.dnc` format; a short file keeps what was read before the end
  and leaves the rest. Tests: `test/test_dance_file.cpp`.
- **The script lines**: `src/LHScriptX/ScriptWriter.{h,cpp}` builds a line from the command's letters, with the
  position written as the original's CRT rounds it (half up); `src/ECS/LandScriptSave.{h,cpp}` writes the lines.
  openblack's reader takes the bare name (`CELTIC_FISHERMAN`) as a string. Tests: `test/test_script_writer.cpp`.
- **The landscape**: `components/lnd` (`LNDFile`) and `LandIsland` read the original maps and the editor's three
  extensions (`EXT0` and `META`; `LNDCell::Altitude`, `LandIslandInterface::GetCellAltitude` for altitudes over 8 bits;
  `LandIsland` builds its block table from each block's `blockX`/`blockZ` and corrects a `mapX`/`mapZ` that does not
  agree). The low-resolution textures are read in `LNDFile`, decoded by `low_res::DecodePage` and drawn beyond the texture
  detail line ([the terrain](../world/terrain.md)).
- **`Scripts\info.dat`** is loaded into `InfoConstants` ([info.dat](info-dat.md)).

#### What is adapted in openblack

**Our own** (the original maps do not change):

- With more than 8 bits the heightmap goes from R8 to R32F on the same scale (1 = altitude 255). Above 255 the
  country's last material is used, as the editor does.
- The per-island textures (footprints, static shadows, alpha) drop below 256 texels per block as soon as they would
  exceed 8192.
- The disc that limits the camera (centre 2560, radius 5120) grows with the size of the map.

### Test hooks

None of their own. A test map is started with `-s` and the absolute path of its script (above).

## Pending

- **A way past its table** (u < 0 or u > 1): the original reads the object's other fields there; what it gets is
  not read, and openblack uses t = u ([`LH3DWay::Running`](#lh3dwayrunning)). Verify with xref
  (`xref.py show 0x844387`).

- **Creature Isle's info.dat** (627250 bytes, longer tables): not read by openblack yet.
- **The L3D footprint size**: openblack reads it from the header, the editor per entry; which the original uses is
  still open. Known: the footprint block is `LH3DMesh` +0x48 when flag 0x8000 is set (`GetLandscapeFeatureData`
  0x403730); `GetSizeFootprintData` 0x403B90 returns its dword at +8 (the block size), which the other getters
  (`GetSizeTnLData` 0x403AA7, fn_0080D9B0 0x80DA7A / 0x80DAC7) use only to skip the block. The reader of the
  footprint's width and height is not among the 19 callers of 0x403730 read so far; next, the footprint blit in the
  LH3DLandscape code (+0xC / +0x10 reads of the block).
