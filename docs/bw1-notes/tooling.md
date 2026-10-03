# Tools and formats

How the original is investigated (disassembly scripts over `runblack.exe`), openblack's command-line
tools and the game's data formats: G3D/L3D packs, effects, textures, scripts, `info.dat` and the LND with
the extensions of the BWLandEditor map editor.

- [Disassembly of runblack.exe](#disassembly-of-runblackexe)
- [openblack tools](#openblack-tools)
- [Data formats](#data-formats)
- [LND and BWLandEditor maps](#lnd-and-bwlandeditor-maps)
  - [How it reads the LND](#how-it-reads-the-lnd)
  - [Cell bytes](#cell-bytes)
  - [Low-resolution textures](#low-resolution-textures)
  - [What is adapted in openblack](#what-is-adapted-in-openblack)
  - [Test maps and analysis scripts](#test-maps-and-analysis-scripts)
  - [Differences not checked in the original](#differences-not-checked-in-the-original)

## Disassembly of runblack.exe

Scripts in `C:\Users\diewgarc\dev\documentacion`:

- `python bwdis.py ADDR:SIZE [ADDR:SIZE...]` disassembles `runblack.exe` with capstone. It annotates symbols, floats from the
  data section (`; =0.67`) and strings.
  - Virtual calls `call [reg + off]` are annotated with the vtable of class `VTC` (environment variable, by
    default `Tree`). **The annotated name is only valid if `VTC` is the object's real class**; otherwise, ignore it.
- `python callers.py ADDR` lists who calls a function (searches for `call rel32`).
- `python refs.py ADDR...` lists instructions that reference an address (data or functions).
- `multi\vt.py Clase [regex]` prints a class's vtable; `multi\potinfo.py` reads the pot table from `info.dat`.

Symbols and tricks:

- The symbols come from `bw1-decomp\config\BW1W120\symbols.txt`. Some names are wrong, for example:
  `0x5B3C70` is `HandStateHolding::Update` and `0x5B5E70` is `ObtainRequiredHandPosition`.
- To read tables that are filled in at startup (in `.data` they are zero), look for the `crt_xc_fn_*` initialiser that
  writes to them (e.g. the interface action state table, 0x5D7960).

## openblack tools

Built in `cmake-build-presets\ninja-multi-vcpkg\bin\Release`:

- `packtool -M pack.g3d` lists meshes; `packtool -m N -e out.l3d pack.g3d` extracts mesh N;
  `packtool -T pack.g3d` lists textures (**ids in hexadecimal**); `packtool -t IDX -e out.dds` extracts the texture by
  index (the id is a different field).
- `l3dtool read -H|-m|-P|-V|-I|-s file.l3d`: header, submeshes, primitives (material, skinID), vertices (positions
  with **one decimal place**), indices, embedded skins.
- `lndtool write ... --points "x y z"`: generates the test terrain (see tests in openblack-internals.md).
- **Shaders with #include**: `bgfx_compile_shaders` only follows the top-level file. The `vs_object_*.sc` variants
  (and `vs_static_shadow_instanced_static.sc`) include `vs_object.sc`/another variant: after changing the included file you have to
  `touch` the variants, or the executable keeps the old version for static and instanced meshes.

## Data formats

- **Mesh pack** `Data\AllMeshes.g3d`: Lionhead pack with ~626 L3D meshes and their DDS textures. The mesh
  number is the index in the pack, which matches the enum in `Data\AllMeshes.h` **only for that pack** (Creature
  Isle and other packs have other indices). The installed one is modified: [mods.md](mods.md).
- **L3D**: header of 19 u32 (magic, flags, size, submeshCount, submeshOffsets, bbox[8], another, skinCount,
  skinOffsets, extraCount, extraOffset, footprintOffset). Embedded skins start with a u32 id followed by
  256×256 16-bit pixels.
- **Effects**: `Data\Spells\ZSpellFiles\*.zzz` = zlib from byte 4 onwards; inside, a text properties file
  (`BEGINCLASS` / `PROPERTY`). Examples: `SF_GripLandscape`, `SF_MultiPickUpWood/Food/FoodFish`, `SF_MultiPutDown*`.
- **Loose textures** `Data\Textures\X.raw` (256×256 RGB) + `Xa.raw` (R8 alpha). Sprite sheets of 8×8 cells.
- **Map scripts** `Scripts\LandN.txt`; the executable's command table (0xC21190…) gives the name and parameter
  types (`A` position, `N` integer, `F` float). E.g.: `CREATE_MOBILE_STATIC` = `ANFFFFF` =
  (pos, type, altitude, X angle, Y angle, Z angle, scale).
- **`Scripts\info.dat`**: object tables (pots, trees, mobile statics...). openblack loads it into `InfoConstants`.

## LND and BWLandEditor maps

`B&W\BWLandEditor-main` is Daniels118's map editor (Java, GPL-3). Part of its code is ported from
openblack: InfoConstants, L3D/G3D and an old version of `fs_terrain`. What follows comes from the editor's code
(not from the original executable) except where stated otherwise.

### How it reads the LND

The same as openblack, plus three extensions that openblack already supports (`LNDFile`, `LandIsland`):

- At the end of the file there may be the blocks `EXT0` (u32 size of the whole block = 10, u8 version, u8 altitude
  bits 8-16) and `META` (u32 data size, editor data). With more than 8 bits, the high bits of the altitude
  go in the low bits of `saveColor` (`LNDCell::Altitude`, `LandIslandInterface::GetCellAltitude`).
- The grid can have up to 128×128 blocks and more than 255 blocks. The header table only covers 32×32 and
  indices < 256, so `LandIsland` builds its table from each block's `blockX`/`blockZ`. In the 21 original `.lnd`
  files the table matches those fields (`dev\herramientas\lnd\lnd_check.py`).
- The editor corrects a `mapX`/`mapZ` that does not agree with `blockX`/`blockZ`, and `LandIsland` does the same.

### Cell bytes

- **`flags`, according to the editor.** Bit 0 = "transparent"; bits 1-7 = ambient sound: 0 nothing, 2 splashing, 3 ocean,
  4 slow waves, 5 lake, 6 coast, 7 fast waves, 8 jungle, 10 wind, 12 desert, 14 birds, 16 forest, 18 river.
  The odd ones above 8 are variants of the preceding even one. openblack uses it in the `zone`/`not_zone` filters of
  `world.foliage` (**mod**, 1579a51c, see [mod-library.md](mod-library.md)).
  The original reads the zone as `(flags >> 2) & 0xF` (`Terrain::GetAtmosType` 0x7352B0): the editor's codes are
  `tipo << 1`; table and usage in [objects-and-resources.md](objects-and-resources.md) («Ambience (atmos)»).
- **`properties` (+6).** Bits 0-3 = country, bit 4 (0x10) = hasWater, bit 5 (0x20) = coastLine, bit 6 (0x40) =
  fullWater, bit 7 (0x80) = split (diagonal, see [engine-math.md](engine-math.md#terrain-height)). To separate
  the open sea from inland water, `lnd_water.py` groups the connected cells with water or without a block: those touching
  the map edge or the void are sea; the rest, lakes or ponds.

### Low-resolution textures

Atlas of 4×4 subtextures of 64×64, one per block, 4 texels per cell, with X and Y swapped. The "unknown" in its
header is the number of blocks in the atlas. `iu_lrs`/`iv_lrs` are integers (0/64/128/192). openblack does not use them (it only
reads them in `LNDFile`).

### What is adapted in openblack

**Our own** (the original maps do not change):

- With more than 8 bits the heightmap goes from R8 to R32F on the same scale (1 = altitude 255). Above 255 the
  country's last material is used, as the editor does.
- The per-island textures (footprints, static shadows, alpha) drop below 256 texels per block as soon as they would exceed 8192.
- The disc that limits the camera (centre 2560, radius 5120) grows with the size of the map.

### Test maps and analysis scripts

`dev\herramientas\lnd\lnd_make_tests.py` generates three maps and their scripts in `dev\lnd_test` (start with `-s` and the absolute
path of the `.txt`):

- `Land1_ext`: Land1 with the editor blocks at the end; identical to Land1, height at (1788.4, 2710) = 28.9173050.
- `Land1_hi`: 10 bits and doubled altitudes; height 57.8346100.
- `Land5_x2`: Land5 twice, grid of 60, 374 blocks.

`.lnd` analysis scripts (`dev\herramientas\lnd\`): `lnd_check` (block table versus blockX/blockZ),
`lnd_make_tests` (test maps), `lnd_beaches` (sand next to water, materials 6 and 11), `lnd_zones` /
`lnd_countries` / `lnd_find_country <lnd> <n>` (sound zones, countries and median position of a country),
`lnd_materials` / `lnd_colours` / `lnd_tile_check` / `lnd_decal_metric` (256×256 RGB555 materials),
`lnd_water` (bodies of water), `lnd_hash` (FNV-1a of the materials).

### Differences not checked in the original

- The editor chooses the material with `min(altitud + ruido/4, 255)`; the original does it **per texel** with
  `min((h >> 8) + ruido, 255)` and the altitude weighted with cones (resolved, [rendering.md](rendering.md#coast)), and openblack
  already follows it (`3D/BlockTexture`).
- openblack's L3D reader takes the footprint width and height from the header; the editor reads them per entry.
- The editor reads Creature Isle's info.dat (627250 bytes; longer tables in InfoConstants.java L23-33); openblack
  does not yet.
