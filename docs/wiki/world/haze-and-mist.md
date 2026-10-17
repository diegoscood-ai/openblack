# Haze and mist

## Summary

The original's distance haze is done in software, per land vertex and once per model at its origin, with a near, a
far, a colour and a strength computed every frame from the light table's base (storms pull it in to 15 / 350,
lightning whitens it); it is on from detail level 3 (the "Fog" key) and not applied to the sea, the sky, the particles
or the hand. Models take their light from the land under them in one of four ways, and light and shadow stamps
(clouds, storms, light maps, the flash) are written straight into the land's cells. Map mists (`CREATE_MIST`) are
domes of `mist.l3d` turned to face the camera, queued with the transparent objects.

openblack has one haze API (`graphics::haze`, `haze.sh`) and one land light API (`land_light`) with the original's
rounding, and draws the mists in the shared transparent queue.

## The original

### Map mist (CREATE_MIST)

- `CREATE_MIST` "AFNFF" (0x7155C9) → `Mist::Create` 0x6063D0(pos with relY = F1, size F3, colour N2, k F4) →
  `CallVirtualFunctionsForCreation` 0x606420: `LH3DObject::Create(7)` (LH3DMist) at `GetAltitude(x, z) + F1`,
  +0x88 = F3, +0x90 = N2 >> 24 (alpha), flag +0x80 bit 1; only if F4 ≠ 1, +0x8C = F4 and bit 2. The constructor of
  LH3DMist 0x7F9560 sets +0x88 = 1, +0x8C = 3, +0x90 = 0x80 and the counter +0x84 = Random(0, 16) & 15.
- `Mist::SetFade` 0x606800(initial size, final size, initial alpha, final alpha, seconds): sets the initial size and
  alpha right away and `fn_00606880` adds one step per 0.1 s turn for seconds × 10 turns (alpha limited to
  0..255); `Get2DRadius` 0x606660 = scale × the largest x/z half-extent of the mesh. It is used by `CREATE_MIST` 263 and
  `SET_MIST_FADE` 264 of CHL (one call of each in challenge.chl; not done yet).
- Land1 has 17 (swamp, the piper's cave...).

#### Drawing

- Same mesh `mist.l3d` (dome of radius 20, base at the origin), smoke material 0xEA1ABC (`fn_0080BBD0`, mode 6:
  SRCALPHA/INVSRCALPHA blending, colour and alpha = texture × diffuse, no Z write, two-sided) and 8×8 atlas as the
  clouds. Creation (`CallVirtualFunctionsForCreation` 0x606420): +0x80 |= 1 always; if F4 ≠ 1, +0x8C = F4 and
  +0x80 |= 2 ("effect" branch). Colour N2 at +0x4C (ARGB), +0x50 (specular) = 0.
- **Rotation** (key): 0x7FA38F copies into the object's matrix the one at 0xEA1C98, which `UpdateCamera` 0x819A62 builds in two
  steps. First it permutes the columns of the world→camera A = 0xEA1D28: row i = (A[3i], −A[3i+2], A[3i+1]),
  translation 0. And **then** (0x819AC5 `mov ecx, 0xEA1C98`, 0x819AF3 `call fn_007FB3F0`; the other copy of
  `UpdateCamera`, 0x81A1AD, does the same at 0x81A265) it **inverts it in place**: `fn_007FB3F0` is the inverse of
  the 4×3 matrix (cofactors / determinant, translation = −t·M⁻¹). Being orthonormal, the inverse is the transpose, so
  the final rows are right, −forward and up. A uses row vectors and the object's matrix is applied the same way
  (x' = m0 x + m3 y + m6 z, `fn_0084BA90`), so row k is the image of local axis k: in glm
  **mat3(right, −forward, up)**, that is, a **billboard**. Local X = screen right, local Y (the axis of
  the dome) towards the camera, local Z = up, so the dome is always seen face-on, like a smoke disc, and
  never edge-on (checked by emulating 0x819690 + the permutation + 0x7FB3F0 with several cameras). Its centre is on the ground, so the Z test cuts off the lower half of the disc (also
  in the original).
- **Effect branch** (bit 2; in Land1 they all have k = 1, in Land4/Land5 k = 3.78 / 2.64): s = size/(1 + (k − 1)
  (1 − |dy|/|d|)); 0x7FA4DC..0x7FA539 scale row 0 (local X) by the size and rows 1 and 2 (Y, Z) by s: **non-uniform
  scale**. Light at (0, 500000, 0), ambient 0xD2, no land light, atlas V + 0.25 (0x7FA44D: rows 2-3).
- **Normal branch** (0x7FA5B0): the 9 cells × size. `fn_00801C90` gives the light (table[lum] bilinear over the 4 cells)
  and leaves at +0x50 the bilinear RGB of those cells (the first dword read as D3DCOLOR: red = blue byte). `fn_007FEB30`
  applies the haze: light × (256 − trunc((256 − k) t)) >> 8 and specular += round(haze colour × t). Then each
  channel = floor(N2 × light / 255), alpha = alpha of N2, and the model lighting (light at (−500000, 500000, −500000),
  ambient 90). **Without** the + 0.25 of the atlas (0x7FA675: rows 0-1 of `smokea.raw`, peaks 171-197; rows 2-3 reach
  228-248).
- Per-vertex light (`fn_0084BA90`): I = round(255 · n_local · L_local), L_local = normalize(M⁻¹ (Lpos − pos)); with
  non-uniform scale it is not the light of the rotated normal.
- Counter +0x84 += ftol(g_game_time_inc · 0.255), modulo 900 only if it goes past 900; frame (counter/20) & 15. It only
  advances inside Draw, that is, with the mist on screen.
- Order: `LH3DMist::AddDrawing` 0x7FA7F0 discards with `CheckRegionOnScreen` (radius = mesh radius × size ×
  0.55) and sends the mist to the `LH3DZSorter` (key |pos − camera|², callback 0x7FA980), together with the transparent
  models and the sprites.
### Distance haze (original, "Fog" detail, levels 3–6)

- Per-frame parameters in `fn_00869850` (0x869CB8..0x869F78 → `fn_007FEAA0` / `fn_007FEAD0`), from the base of the
  light table (with the cloudy weather cap): `k = min(255, (R + 4G + 3B)/8 + 8)`, colour = (R/3, G/3, B/3); with v' = 0
  by day and by night and 1 at sunset: `near = 1/(0.0025 + 0.0075·v'²)`, `far = 1/(0.00111111 +
  0.000138889·v'²)` (400→900 / 100→800). Storm: colour → (c>>3)+32, k → 48, near/far → 15/350 by w;
  lightning: colour and k → 255. The 15/400/64 of the static initialisers are not used.
- `t = clamp((z − near)/(far − near))`, z = view depth along the camera axis. Diffuse ×
  `(256 − trunc((256 − k)·t))/256`; specular += `round(color·t)` with saturation.
- Land: per vertex (`fn_00874AA0` 0x874C5B, SSE `fn_007A1800`), block classes 0/1/2 (block+0x940); the land's
  specular = RGB of the cell (its first dword as a D3DCOLOR: bytes 0..2 blue, green, red) + haze, added after the small bump.
  Models: once per object at its origin (`fn_007FEB30`), no haze closer than near. It is not applied to the sea, the
  sky (only storm tint), the sun and the moon (alpha ÷ (1 + 8w)), nor to PSys particles, shadows or the hand.
- Values (neutral, clear): noon near 400 far 900 k 211 colour (63, 70, 65); sunset 100/800 k 120 (56, 37,
  31); midnight 400/900 k 81 (16, 24, 28).
- The haze globals: the "Fog" key [0xC37204] (written by `fn_008237B0` 0x8239E2 and `fn_00823AD0` 0x823C1F); near
  [0xC37220] (static 15), far [0xC37224] (static 400), range = far − near [0xE9B6DC] (`fn_007FEAA0`
  0x7FEAB0..0x7FEABB); k [0xC37228] in 1/256 (static 256, an int argument of `fn_007FEAD0`, 0x7FEAE0..0x7FEAF5);
  colour [0xC37214] / [0xC37218] / [0xC3721C] as floats (static 64).
- The packed colour [0xE9B6D8] is `__ftol` of each float: b in byte 0, g in byte 1, r in byte 2, alpha 0
  (0x7FEAFA..0x7FEB26).
- The depth z is the z column of `g_world_to_clipping` 0xEA9E40 (+0x08, +0x14, +0x20, translation +0x2C)
  (0x7FEB4A..0x7FEB75, also 0x874C14 and 0x8773BC).
- Objects (`fn_007FEB30`): with Fog off the specular comes back untouched (0x7FEB36..0x7FEB49); strictly closer than
  near nothing changes (0x7FEB7D..0x7FEB94); t uses fdiv (0x7FEBAD..0x7FEBBE); the diffuse is scaled only when f < 256
  (unsigned, 0x7FEBEB); the haze colour gets alpha 0xFF (0x7FEC80..0x7FEC99); a zero specular becomes that colour as
  it is (0x7FEC9B), any other gets it added with saturation, alpha 0xFF either way (0x7FECA2..0x7FED0D).
- Land vertices (`fn_00874AA0`): with no haze the loop at 0x874B5E jumps to 0x875024; class 1 is 0x874C5B..0x874D1E;
  the specular keeps its **own** alpha (0x874D83), or becomes the haze colour with alpha 0 when it was 0 (0x874D9E);
  then the diffuse (0x874DA1..0x874DF0), only when f < 256 (0x874DA7).
- The haze distances' constants, as floats: [0x9A3B18] 0x3B23D70A = 0.0025, [0x9A3BDC] 0x3BF5C28F = 0.0075, [0x9A3BE0] 0x3A91A2B4 = 1/900, [0x9A3BD8] 0x3911A2B0 = 0.00013888883, [0x9A3BD4] 0x3B3B3EE7 = 1/350.
### Haze and land light: the common API

One haze formula with three implementations (objects `fn_007FEB30`, x87 land `fn_00874AA0`,
SSE land `fn_007A1800`) and four ways of taking the land light under an object, which are not merged:

| Routine | What it does | Users |
|---|---|---|
| `fn_00801C90` (SSE `fn_007A3EC0`) | **integer** bilinear over 4 cells: weights `ftol(frac·256)` [0x8D45CC], first in z (+0x08) and then in x (+0x88), `a + ((b − a)·w >> 8)` per byte; outside the map table[255] and specular 0xFF000000 (0x8020F8) | almost all the models (48 calls) |
| `fn_00802120` (SSE `fn_007A4170`) | the same, with weights `CellX >> 8` and `CellZ >> 8` (0x802206, 0x802237): **almost no interpolation** | `Tree::Draw` 0x74AB1B (and the haze at 0x74AB60), `Scaffold::Draw` 0x6EA6CA, `TownArtifact::Draw` 0x51CB14 |
| `GetAltitudeAndSetColorSpecular` 0x803340 | the cell alone (0x8033FA..0x803413), no haze | `WorshipSite::Draw` 0x519460 (except if it is burning: with `Object +0x44`, the FireEffect, it goes through `fn_00518050` → `fn_0080BEC0`, bilinear and haze; 0x5193FF..0x51940A), `SpellIcon::Draw` 0x5196CC, `Totem::Draw` 0x51ACD1 |
| `fn_00801C90` without `fn_007FEB30` | the bilinear, no haze | `MultiMapFixed::DrawBuilding` 0x517F90 (0x517FB2; with fire only the tint `fn_0080BF10` 0x517FD4): the half-built building. It is called by `MultiMapFixed::Draw` 0x5180A6 if `IsDrawBuilding` (vt +0x8A4: the building plot +0x74, 0x52F0C0; for a Feature, 0x527790: the unfinished ArkDryDock), `Abode::Draw` 0x516129 (with DestructionMesh +0x90: the repaired part) and, with `IsBuilt` (vt +0x890) = 0, `WorshipSite::Draw` 0x5193E9, `SpellIcon::Draw` 0x519668 and `Totem::Draw` 0x51ABC3. `PetitNavire::PreDraw` 0x5DFF20 (hull +0x28, 0x5E018D and 0x5E03DF) and `PostDraw` 0x5E03F0 (sailors 0x5E073B; the deck copies the hull's +0x4C, 0x5E099C..0x5E09A2) never call `fn_007FEB30`. `Scaffold::Draw` 0x6EA5C0: the scaffold through `MobileObject::Draw` (with haze) and the ghost building +0x74 with `fn_00802120` (0x6EA6CA), no haze |
| [0xEDDD08] = table[255] | fixed light | `Dove::Draw` 0x41F75B, `CitadelHeart` 0x466958, clouds `fn_005E1DE0`, sea `fn_00879930` |

Haze class of each block (`fn_00877210` 0x87743D..0x87749B): the depth of the 8 corners of its box
(0x877232..0x877370, one per case of the jump table 0x877D04: centre +0x90C / +0x910 + 80 [0x8D060C] ± 80; in y,
from 0 to `fild(+0x924)` × 0.67 [0xC3720C], or ± that height with [0xE9CD8C]). 🔍 inferred +0x924 is
`LNDBlock::highestAltitude` and [0xE9CD8C] the LandRef detail key.
Bit 2 if a corner goes past far, bit 1 if it is in (near, far]. Class 0 with Fog off or without bits, 1 if there is any
bit 1, otherwise 2.
- Cell layout: every LandBlock holds 17×17 cells of 8 bytes, index (x & 15)·17 + (z & 15) (0x83AED6..0x83AEEC); the
  first dword is the cell colour read as a D3DCOLOR (bytes 0..2 blue, green, red, named so in `LNDCell`), byte 3 the
  luminosity. The specular store at 0x874B7A (`mov eax,[cell]; or eax,0xFF000000`) and the haze sum at
  0x874D08..0x874D99 (byte 2 + R, byte 1 + G, byte 0 + B) read the bytes in this order, with no swap. bw1-decomp's
  `LandCell` and the 2015 `lnd.h` call them r, g, b (byte 0 first), which does not match. The 4 corners are at +4, +0xC, +0x8C, +0x94 (`fn_0083AE80` 0x83AEF7..0x83AF17).
- Cells are global, 0..511 along x and z (world / 10); `g_index_block` 0xE9C964 is 32×32 blocks of 16 cells, index (cx
  >> 4)·32 + (cz >> 4) (0x874650..0x87466F).
- `fn_00801C90`: cell = `__ftol(x·0.1)`, `__ftol(z·0.1)` (0x801CB8..0x801CDC, clipped to 0..0x1FF), weight ftol((f −
  ftol(f))·256) (0x801DCB..0x801E06); the byte lerp a + ((b − a)·w >> 8) with the masks is a + floor((b − a)·w/256),
  alpha 0 (0x801E0B..0x8020F0); off the map 0x8020F8..0x802113.
- `fn_00802120` reads CellX = word [mc + 2] and CellZ = word [mc + 6]; SSE weights 0x7A42AC / 0x7A42BC; off the map
  0x802525.
- `GetAltitudeAndSetColorSpecular` 0x803340: off the map or without a block it exits at 0x803365 / 0x8033DA.

**Light and shadow stamps on the land.** There is no separate texture: they are written into the LandBlock's cells.
- `fn_0086CFF0(pos, texels, pitch, centre, alpha, mode, maximum)` → `fn_0086CF50`: up to 200 stamps (0x86CFF5) in the
  list 0xFA2920 (0x34 bytes, count [0xFA51C0]). The alpha × 255 is clipped and goes through `ftol`. With `centre`, the
  position moves back (pitch − 1)·5 [0x8AB6E4] in x and z.
- `fn_005E5830` → `fn_0086D360` (0x5E592F) applies the list with `fn_0086D060`: cell `ftol(x·0.1)`, weight
  `fistp(255 − frac·255)` & 0xFF, clipping to 0x200 cells and only blocks with +0x920 & 4. Modes 1..8 of the table
  0x86D338 only set a bpp (3, 1, 1, 1, 1, 1, 1, 4); only 1 and 2 stamp.
  - Mode 2, shadow (`fn_00878C70`): bilinear of the map (z with wz, then x with wx), `v = 255 − (255 − r)·alpha/255`
    (0x80808081), **floor 0x30** (0x878DAD) and brightness = **min**(brightness, v) (0x878DBD..0x878DC6).
  - Mode 1, light (`fn_00878780`): per channel, `v = r·alpha/255` added to the cell's colour capped at 0xFF (0x878B09),
    or the maximum if the last argument is not 0 (0x87890D). The map's R goes to the D3DCOLOR's red.
- `ClearLight` 0x5E57B0 → `fn_0086D460`: `fn_00878700` leaves colour 0 and brightness = byte +5 (which in the .lnd files
  has the same value as +3: checked in Land1, Land2 and Norse) and empties the list (0x86D487).
- Callers:
  - PSysLightMaps `fn_006CA280`: + (10, 0, 10) [0x8AB414], centred, mode 1 with bpp 3 and 2 with bpp 1. It is fed by
    `ParticleLightMap::DrawAt` 0x67B220, the shadow of the PSys clouds (0x67A7BE, `GetBitmap` 0x6AA540) and
    `FireGraphic` (0x731633).
  - The map's clouds, `fn_005E25C0` 0x5E2800: sclouds.raw of 40, not centred, mode 2. It covers pitch − 1 = 39
    cells. The map's rows go along x (`fn_0086D060` 0x86D1EC advances `ix·pitch·bpp`) and the bytes along z
    (`fn_00878C70` 0x878DD1): the shadow ends up **transposed** with respect to openblack's earlier one, which read
    `image[z·40 + x]` and covered 41 cells.
  - The storm's shadow, `GWeather::DrawClouds` 0x8400C6: sstorm.raw of 40, centred, mode 2, s = (blackness +
    0.7)·fade.
  - The flash, `fn_00837200` 0x837278: the radial map 0xED92F0 of 64, mode 1.
  - `DanceLight` 0x50F919: not ported.
- `fn_0086D060`'s weight below 0: fistp((i − f)·255) and the cell one less (0x86D110..0x86D135); above 0
  0x86D0E7..0x86D106; & 0xFF at 0x86D19A..0x86D1A7; clipping 0x86D1B5..0x86D241; only modes 1 and 2 stamp (0x86D2E8).
- Light stamp (`fn_00878780`): texel byte c goes into colour byte 2 − c (texel R → D3DCOLOR red), capped at 0xFF
  (0x878B09..0x878B19).
- `ClearLight` goes `fn_0086D460` → `fn_0086D390` → `fn_00878700`.
- The "drawn this frame" bit 4 of block +0x920 is set by `fn_00877210` (0x8774E8).
- The map clouds' stamp: only clouds with an alpha (0x5E27CB), at the cloud's world x, z with y 0
  (0x5E2769..0x5E27B5), `fn_0086CFF0(pos, [0xD1A25C], 40, 0, alpha·(1/255) [0x900058], 2, 0)` (0x5E27DE..0x5E2800,
  `push 0x28` 0x5E27FE). [0xD1A25C] holds `sclouds.raw`; the detail key is "CloudShadows".
- The routines' full names: `LH3DIsland::GetAltitudeAndSetColorSpecular` 0x803340, and the block index `LH3DIsland::g_index_block` 0xE9C964.

### Map mist

The drawing of the map's mist (`fn_007FA300`, billboards, order, fade and the other mists of `mists::Submit`)
is in [drawing](#drawing), together with its creation (CREATE_MIST).

## openblack

Status: **done** for the distance haze, the land light and its stamps, and the map mists; the differences still open
are in [Pending](#pending).

### The haze and land light in openblack

- A single API, `graphics::haze` (`src/Graphics/Haze.{h,cpp}`) and `assets/shaders/haze.sh` (see the
  next section). Faithful in the rounding: `f = 256 − ftol((256 − k)·t)` (0x7FEBC3), diffuse `(c·f) >> 8` per byte
  with the alpha kept (0x7FEBED), colour with `fistp` (round-to-even, 0x7FEC4A), class 2 with the truncated colour
  [0xE9B6D8] (0x874C48, packed at 0x7FEB26) and saturated add.
- `graphics::haze`: `Params` (the 7 globals), `Frame()` (the Fog key in a single place), `ApplyObject` =
  `fn_007FEB30`, `BlockClass` = `fn_00877210`, `ApplyVertex` = `fn_00874AA0` and `Uniforms`. `haze.sh`: `HazeT`,
  `HazeFactor`, `ApplyHazeDiffuse`, `HazeColour` (round-to-even), `HazeColourFull` and `HazeAddSaturated`.
- `land_light` (`src/3D/LandLight.{h,cpp}`, `assets/shaders/land_light.sh`): the frame's cells (`BeginFrame`,
  `ApplyStamps`), `At` / `AtCellShift` / `AtCell` / `FullLight`, `AddStamp` / `ApplyStamp` and `LoadBitmapFile` (it
  reads the file and passes it to `psys::LoadStackedBitmap` = `GJBitmap::LoadBitmapFromFile` 0x57CA90,
  with the frame layout of `fn_0057CB40`; `psys::StackedBitmap::Frame` gives one frame, 0x6CA2E3). `Renderer::UpdateClouds`
  uploads the cells as an RGBA texture (colour and brightness) read by `vs_terrain` and `vs_object`.
- The haze class of each block: `haze::BlockCorners` (the 8 corners) and `haze::BlockClassOf`.
- `vs_object`: `u_objectLight.w` = no haze + 2 × `land_light::ObjectMode` per mesh
  (`RenderContext::meshLandLight`). The trees go with `CellShift` and haze; the worship sites (if they are not burning)
  and the icons, with `Cell` and no haze; the Dove class, with `Full` and no haze (`Dove::Draw` only writes +0x4C;
  🔍 inferred +0x50 stays at 0). The half-built ArkDryDock (`ecs/FeatureBuild.h`) and the boat's hull
  (`missionary_boat::GetHull`, was `petit_navire`) go bilinear and without haze (`DrawBuilding` 0x517F90, `PetitNavire` 0x5E03DF).
- `vs_terrain`: the light and colour of the frame's cell, and the haze according to the block's class (`u_hazeBlock`).
- The shadow of the map's clouds is no longer a separate cap: they are mode 2 stamps (`Clouds::StampShadows`).
- The night lights (`night_lights`, `fn_008229B0`) write into the frame's brightness, after the stamps. `fn_008229B0`
  reads byte +3 of the cell and writes it directly (0x822D9D, 0x822DC3). Before, openblack clipped them with
  `min(loaded brightness, cap)`, so a light did not raise a cell with brightness < 48; now it does.
- `Particles/Creators/LightMap` is no longer a flat sprite: it stamps on the land (`light_map_atoms::SubmitFrame`, once per
  frame: one record per atom, like `DrawAt` 0x67B220). Each atom runs through the map's frames
  (`LightMapCreator::InitAtom` = `CreateParticleLightMap` 0x6A9DEF..0x6A9E16: FrameRate at +0x110, NumFramesInUse at
  +0x114, PlayAnim at +0x118), so the light fades out; stuck on frame 0, the lightning left the ground white. The `UseRandJitter` jitter comes from
  `game_random::LocalFloatRand` (GRand's local seed, the same one `FireGraphic` uses): the 1st draw goes to z, the 2nd
  to y and the 3rd to x (0x67B264..0x67B2A3).

### The mists in openblack

- The map mists: `MistArchetype`, `components::Mist`, `Renderer::CollectMists` / `DrawMist`.
- `Renderer::CollectMists` / `DrawMist` (RendererMists.cpp) enter the back-to-front list of the
  main pass (`DrawPass`, `ZObject::mist` of the `graphics::zsort` queue); without entities the mists go into the
  same queue as the clouds (there is no `DrawMists` any more). `vs_cloud`
  receives `u_cloudLight` (L_local) and `fs_cloud` adds `u_cloudSpecular` (0 in the clouds and in the effect branch).
  Deviation: the counter keeps the fraction (like `Clouds.cpp`), because without vsync openblack goes over 250 fps and
  the original's truncated step would be 0. The alpha texture `smokea.raw` is cut to 4 bits on load, as in the
  original (ARGB4444, `a.raw` 0x8375C1: 228 → 238/255; see
  [ARGB4444 textures](../rendering/textures.md#argb4444-textures)).
- **Other mists** (API `mists::Submit(const MistDesc&)`, `src/Graphics/Mists.h`): whoever has their own
  LH3DMist objects (the storm puffs of `GWeather::DrawClouds` 0x83FC90) sends them every frame with position,
  size, ARGB colour, effect/normal branch, k and its counter; `CollectMists` clips them with the same sphere and they go into the
  same back-to-front list as the map ones and the blended models (`_frameMists`, `DrawMist(índice)`).

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/LandLight.cpp`: `CellAndWeight`.
- `src/3D/LandLight.h`: `Clouds::GetLandscapeGeneration`, `LerpBytes`.
- `src/3D/LandLightTable.cpp`: `LandLightTable::Load`.
- `src/Graphics/Haze.h`: `BlockCorners`, `DetailLevel::landReflection`, `FromTable`, `LandLightTable::Haze`, `ScaleDiffuse`.
- `src/Graphics/Mists.h`: `InView`.
- `src/Graphics/RendererMists.cpp`: `Camera::Interpolation::Current`, `MistSphereRadius`, `Renderer::DrawEffectMist`.

### Test hooks

- `test_haze_land_light` (haze, land light and stamps); `OPENBLACK_TEST_STORM_CLOUDS`, `OPENBLACK_TEST_SPELL` and
  `OPENBLACK_TEST_WEATHER` show the stamps (storm shadow, lightning light and fireball)
  ([Test hooks](../rendering/frame.md#test-hooks)). The full list:
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

### Haze and land light: approximate and still to do

- ⚠️ approximate All blocks are stamped, not only those drawn in the frame (+0x920 bit 4).
- ⚠️ approximate Outside the map, or with c00 in a missing block, the shader's bilinear treats each cell as having no
  block (brightness 255, colour 0), instead of giving table[255] and 0xFF000000 to the whole sample (0x801D17..0x801D43 →
  0x8020F8).
- 🔍 inferred In `FireGraphic`, `Object +0x24 & 2` = map object and +0x98 = the object's position.
- ⚠️ approximate In `FireGraphic`, the two-sine noise (`VLNoise` 0x590C30 not ported), without the turn fraction, and
  the fire's id instead of `this & 0xFFFF`.
- ⚠️ approximate One light mode per mesh, not per instance: if two classes share a mesh (a `DeadTree` or `FelledTree`,
  which use `fn_00801C90`, with that of a living tree), the special mode wins and a warning is logged once.
- `grand_local::LocalRand` / `LocalFloatRand` (3D/LH3DRandom) have been removed: `game_random::LocalRand` /
  `LocalFloatRand` (LHRand over GData +0xC) are called directly instead, and `game_random::crt::Random` for the CRT's
  `Random`. There is no longer a separate generator.
- PLAUSIBLE, not done: the reflection under the water (`vs_object` x = 3) goes without haze, but the original stores in
  +0x4C / +0x50 the light already with haze (`MobileObject::Draw` 0x51818E, `PhysicsObject::DrawAll` 0x646FB1), which
  `DrawUnderWater` reuses.
- Pending: the `vt+0x890 == 0` branches of `WorshipSite::Draw` and `SpellIcon::Draw` (0x5193D9, 0x519658 →
  `DrawBuilding`) and the +0x10C branch of `SpellIcon::Draw` (0x519672, not read).
- Pending: `RendererSea.cpp:164` reads table[255] with `GetColour(255)` and not with `land_light::FullLight`.
- 🔍 inferred The "cell >> 8" and "one cell" modes are only given to `Tree`, `WorshipSite` and `SpellIcon`: the
  scaffolds, the artifacts and the totems do not have their own component yet.
- Pending: the "light without haze" of `DrawBuilding` for the building plots (MultiMapFixed +0x74; openblack does not
  build on plots) and for unbuilt WorshipSite / SpellIcon / Totem; ⚠️ approximate the repaired part of a damaged Abode
  is merged with its FragMesh, whose pieces do carry haze (`fn_007F7ED0` 0x7F7F5F, 0x7F807D), and keeps the haze;
  ⚠️ approximate the sailors and the boat's deck share a mesh with villagers and cows (one mode per mesh) and keep the
  haze; the scaffold's ghost building is not drawn; the UseLandscapeColor sprites (0x67AFD9) and
  `RenderParticleGJMesh` (0x67C184).
