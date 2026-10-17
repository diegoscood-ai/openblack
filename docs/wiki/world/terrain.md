# Terrain

## Summary

The land is 512×512 cells in 32×32 blocks of 17×17 shared corners. Its height at a point is the original's integer
blend over the cell's triangle (the split bit picks the diagonal, heights near the sea edge flatten to 0), its normal
comes from two lookup tables, each land vertex is lit by a 256-entry light table built every frame from `palette.raw`
by time of day, alignment, overcast and lightning, and a second "small bump" pass adds light detail that fades with
distance in front of the camera. Only the vortex and the temple change heights at run time, and most objects keep the
height they were drawn at.

openblack ports the height, the normal, the light table and the small bump exactly.

## The original

### Terrain height

`LH3DIsland::GetAltitude` (0x803090.

1. Cell `(x>>16, z>>16)`, 16-bit fractions `fx, fz` (`>>8` is used, 0..255).
2. Blocks of 17×17 cells (shared row): neighbours `+1` = z+1, `+17` = x+1. Cell height in `cell.altitude`.
3. If the base vertex ≤ 4, heights ≤ 3 count as 0 (sea edge; global 0xC37BF4 active).
4. The cell's `split` bit (`properties +6 & 0x80`) chooses the diagonal. The 4th corner is extrapolated from the other three
   so that the bilinear blend is flat over the triangle:
   - split and `fz > 0xFFFF - fx`: `c00 = v10 + v01 - v11`; split and not: `c11 = v10 + v01 - v00`
   - no split and `fx > fz`: `c01 = v00 + v11 - v10`; no split and not: `c10 = v00 + v11 - v01`
5. `atX1 = (c11 - c10)*fz + (c10<<8)`, `atX0 = (c01 - c00)*fz + (c00<<8)`,
   `h = (((atX1 - atX0)*fx) >> 8) + atX0`, result `h * 0.67 / 256`.

Verified: Land1 at (1788.4, 2710) = **28.9173050**, the recorded value from the original.

- The high words of the coords are read signed, and the height is 0 when one is negative or past 0x200
  (0x803095..0x8030B5). fn_00800DA0 changes a cell's altitude in every block that stores it, the shared border row and
  column too.
- `LH3DIsland::RayCast` fn_00802550 → `RayCastInternal` fn_00802680, cell test fn_0083AE80: described in
  [the spell core](../magic/spell-core.md) (the lightning's land cut). Also: the Cohen-Sutherland codes are 1 x < 0.1, 2 x > 511.9
  [0x9A2BD8], 4 z < 0.1, 8 z > 511.9 (0x80276B..0x80281C); the cells are walked by column then row
  (0x802B4D..0x802C7B); without a hit, a downward ray (|dy| ≥ 0.0001) gets its crossing of y = 0, true within 7500 m
  of g_camera (0x8025D9..0x802672). Callers: fn_00691F30 0x69221C (lightning), GCamera::Update 0x442406, fn_0044EF60
  0x44F046, CameraModeNew3::Update 0x45DA4A, GLandscape::Draw 0x5E4848, fn_005E5620 0x5E5660, fn_00800C30 0x800D79,
  fn_0086BD00 0x86BF1C.
- The ray walk: "no map edge in that direction" is [0x8FFED8] (the float 1e20, 0x60AD78EC); the guard compares it with the double 1e20 (qword [0x9A2BE0]), which is not equal, so the end is always moved on to the edge plus one step (0x802726..0x802765); the cells are 512 [0x8A99F0]; a hit goes to [0xE9CD80] / [0xE9CD84] (0x83B24B / 0x83B252).
### Terrain normal

**Faithful.** `LH3DIsland::GetNormal(const LH3DMapCoords&, LHPoint*)` 0x803630 (fastcall: ecx = coords, edx = out), ported
in `land_normal::OfCell` (`src/3D/LandNormal.{h,cpp}`) and called from `LandIsland::GetNormalAt`, next to
`HeightAt`. All of the original's callers build the MapCoords with `ftol(x·65536 [0x8AC408]·0.1 [0x8AC404])`
(fn_004427B0, 0x8126C0, 0x459105, 0x7FCBA2): `65536·0.1f` is exactly `6553.6f`, so it is `map_coords::ToFixed`.

1. Cell `(int16)(x >> 16)`, `(int16)(z >> 16)`; outside [0, 0x200) → (0, 1, 0) (0x80363B..0x80366B).
2. `GetCell` 0x516AA0: NULL if there is no block (`g_index_block` = 0, 0x516ADC) → (0, 1, 0).
3. **Raw** heights (without flattening next to the sea): h00 = [+4], h01 = [+0xC] (z+1), h10 = [+0x8C] (x+1),
   h11 = [+0x94], within the 17×17 block (the edge row belongs to the block: cell 511 is read).
4. Triangle (`split` bit = [+6] & 0x80, 0x803698..0x803752):
   - with split: B = h11 at (10, 10) if `fz > 0xFFFF − fx`, otherwise h00 at (0, 0); P = h10 at (10, 0); Q = h01 at (0, 10);
   - without split: B = h10 at (10, 0) if `fx > fz`, otherwise h01 at (0, 10); P = h11 at (10, 10); Q = h00 at (0, 0).
5. `dP = hP − hB`, `dQ = hQ − hB` in integer; `s = T1[|dP|]·T1[|dQ|]`; `P' = (Px − Bx, dP·0.67 [0xC3720C], Pz − Bz)`,
   `Q'` likewise (Q'x = `−Bx`, an `fchs`).
6. `n = ((P'z·Q'y − P'y·Q'z)·s, (Q'z·P'x − P'z·Q'x)·s, (P'y·Q'x − Q'y·P'x)·s)` (0x8037BA..0x8037F5).
7. `k = fistp(((nz·nz + nx·nx) + ny·ny)·1023 [0x9A2BE8])` (to nearest), `n *= T2[k]`; if `n.y < 0`, `n = −n`.

Tables from the island initialisation fn_00803890 (each step at 24 bits):
- T1 at 0xE9B2D8: `T1[i] = 1/√((0.67·i)² + 100)`, i = 0..255 (0x8038E3..0x803934). The edges P' and Q' are
  perpendicular in xz and measure `√(100 + (0.67·d)²)`, so `s = 1/(|P'||Q'|)`.
- T2 at 0xE9A2D8: `T2[0] = 1`, `T2[j] = 1/√(j·0.000977517 [0x9A2BEC = 1/1023])`, j = 1..1023 (0x803936..0x80396F).

The result is **almost unit length**: the quantisation of T2 leaves a relative error of up to ~0.5/k (in a very
steep cell with k = 7, the length is 0.998). It is never zero: the `dot(n, n) <= 0` check PhysOb had was dead
code. **(port)** With BWLandEditor's 16-bit altitudes, `|d|` can exceed 255: it is computed with the same
formula; and the T2 index is clamped to 1023 (in the original, `|n|² ≤ 1` guarantees it).

**Who uses it:**
- physics (`PhysicsBody.cpp` (was `PhysOb.cpp`) `Normal`/`LandscapeNormal`: `AdjustToGroundLevel` 0x7FCBE2, `GroundAndWater` 0x7FD93F);
- the hand (`HandHolding.cpp`, `InitialisePhysicsFromHand` 0x63729E);
- the fireball (`Fireball.cpp`, `GravityWithFloor` 0x6A1C7F);
- the shadows of villagers and animals (`Renderer.cpp`, fn_00812170 0x8126FD / 0x812859);
- the player camera (`DefaultWorldCameraModel.cpp`, `CameraModeNew3::FindBestAngle` 0x459144).

Before, `GetNormalAt` was central differences of ±0.1 m over `GetHeightAt` (which flattens next to the sea) and physics
had its own copy (`glm::normalize` instead of T2, up in cell 511 and without checking whether there is a block). This is not
`GetNormal`, and is not touched: `LandBlock.cpp` (smooth per-vertex normal of the renderer).
- T2's step [0x9A2BEC] is 0x3A802008 (1/1023).

### Terrain detail ("small bump")

From the disassembly:
- `fn_00804830` loads `.\data\Textures\smallbump.raw` with the alpha flag (0x41). On first use `fn_00837400` packs it
  into ARGB4444 and puts `smallbumpa.raw` into the alpha nibble (`"a.raw"`, 0x8375C1). Material mode 0xE.
- Mode 0xE = `fn_0082DD90` (mode table 0xC38728, 8-byte {fn, flag} entries): `ALPHABLENDENABLE`,
  `SRCALPHA/INVSRCALPHA`, stage 0 `TEXTURE*DIFFUSE` in colour and alpha.
- `fn_007A1800` (block drawing): second pass per block, unlit, UV × 12 per block (one repeat every
  13.33 units). It is not done on blocks with an overlay material.
- **Vertex diffuse of that pass** (x87 `fn_00874AA0` 0x8758E7–0x87598B, SSE `fn_007A1800` 0x7A2FD0–0x7A3136; both
  paths the same), with d the signed distance to the fade line and e = 20:
  - d ≥ e → `specular & 0xFF000000 | 0xFFFFFF`: white with alpha 255, or **alpha 0 if the vertex altitude is ≤ 1**
    (the specular alpha, see [Coast](sea-coast-and-rivers.md#coast));
  - −e < d < e → `fistp(255 − (e − d)·255/40) << 24 | 0xFFFFFF` if the specular alpha is not 0, otherwise 0 (black,
    alpha 0);
  - d ≤ −e → 0.
  Masks of the SSE path: 0xFC01B0 = 0xFF000000, 0xFC01C0 = 0x00FFFFFF, compared with 0 (`[esp+0x460]`, 0x7A1C3B).
  That way the detail **fades (Gouraud) towards each vertex with altitude ≤ 1**: it does not end in a straight edge
  where the shallow water touches the open-sea cells. Skipping the triangles with all 3 vertices at 0 (0x7A31A0)
  changes nothing.
- Fade (`fn_007FEE60`; constants from `fn_007FE7B0`: 50 = 0x42480000, ramp 40 = 0x42200000, e = 20 = 0x41A00000):
  line 50 units in front of the camera on the plane y = min(cam.y, 0.67·165); full up to 20 units before, nothing 20
  after. It depends on the camera's height and angle (not a fixed 200): **when zooming the camera in or out the
  area with detail grows or shrinks, also over the shore, just as in the original**.
- Result: `col = mix(col_lit, smallbump.rgb · c, smallbumpa · a)` with (c, a) the diffuse above → **light** specks, also
  at night.
- `SetUseSmallBump` (0x87FD30) only changes `[0xC37210]` (registry "UseSmallBump", detail levels).
- `S_TileLandscape.raw` is not for the terrain: it is entry 6 of the global textures table 0xBEF484, used by the
  citadel heart (`fn_00465C70`) **🔍 inferred**. `L_Smallbump_01.raw` is not used.
- In this installation `smallbump.raw` (2017), `smallbumpa.raw`, `Sky.raw` and `S_TileLandscape*.raw` (2021) come from
  a texture pack, they are not from 2001. The user remembers (user, 2026-10-01) that the mottling over the shallow water may
  come from that `smallbump.raw`: with the 2001 original it would look different; the drawing rule is the same.
### Terrain light table

- `fn_00869850`, every frame from `GLandAlignement::DrawSky`: 256 colours indexed by the cell's brightness, from
  `Data\WeatherSystem\palette.raw` (32×32 RGBA, one for all the islands). Rows 0..2: colour of the good/neutral/evil
  land according to the time (column = (2 − Time2SkyType)·15: 0 midnight, 15 sunset, 30 noon); rows 3..7 by alignment
  (column = X·15, X = 1 − alignment: 0 good, 2 evil).
- base = lerp of rows 0/1/2 by X (with t = −1 exactly at neutral, a bug in the original that is kept), limited to
  255 − 96·cloudy weather. Entries 48..255 = `(c3·(255−i) + base·i) / 200` (`fn_00869790`; LightBoost from the
  registry, 0 by default) capped at 255; 0..47 another ramp (Land1's land uses 48..255). Lightning: lerp to white.
- Use: land vertex diffuse = table[brightness]; reflected land × 0.5; sea = table[255].
- Values at neutral noon: [48] 686d66, [128] 9eab9f, [218] daf0e0, [255] f3fffb (almost white); midnight
  [255] 3d5b6c; sunset d68e79; evil at noon dbc8ff.
- `palette.raw` is read as R, G, B, A bytes and swapped into D3DCOLOR by the loader (0x835CEC).
- The moon colour (palette row 5 at the alignment column) is stored at [0xFA26DC]. Its one reader is the sky draw
  `fn_0086B010` (0x86B625), for the moon: the table, its haze and the land and warm colours never read row 5, so the
  night's land light comes from the land rows at the night column (`test_land_light`,
  `TheNightIsLitByTheLandRowsNotTheMoonsColour`).
- table[255] ([0xEDDD08] = [0xEDD90C + 0x3FC]) is what the sea reads at 0x87993F, and the light of the missing cells
  (0x8020F8, 0x802525, 0x803365).
- Storm: the overcast amount is clamped to 1 and the clamp written back to [0xFA2754] (0x869DDB); near and far move
  towards 15 / 350 at 0x869E71..0x869E96; the flash also whitens the haze (0x869E98).
- Overcast at the camera: `GCamera::Update` 0x4426BA..0x4426E5 stores (float)(int8)byte 3 (movsx of +0x83) × 0.01
  (0x8C5840) in [0xD1A26C], not clamped; `GLandAlignement::DrawSky` copies it to [0xFA2754] (0x5E2215).
- The copy of the last table is read by the PSys mists (`RenderParticleMist::DrawAt` 0x67A6C7) and the storm puffs
  (`GWeather::DrawClouds` 0x83FF56). The ring creators take table[255] for the hand's rings and table[200] for the
  rain's.
### Heights after a land change

Every claim here is from the x86 of runblack.exe.

#### What changes the land at run time

`fn_00800DA0(x, z, altitude)` is the only writer of a cell altitude (the byte at cell +4, in every block that stores
the corner, 0x800DA0..0x800FE9; each write calls `fn_008786D0`, which only marks the block's texture dirty,
[+0x944]+0x138 = 1, 0x8786D0..0x8786F9). It has three callers (`callers.py 800DA0`):

| Caller | What | What follows the writes |
|---|---|---|
| 0x5FF8F0 in `fn_005FF4F0` (vortex turn, from `fn_005FF330` ← `PSysGlobal::GameLoopEnd` 0x68F5B9) | the In / Out flattening, 11 × 11 cells | nothing: the loop ends and the function returns (0x5FF907..0x5FF91F) |
| 0x5FF2C5 in 0x5FF230 (vortex: writes back the saved altitudes +0xBC) | the land restored | nothing. 0x5FF230 has **no caller** (no call/jmp rel32 and no pointer to it in the image): dead code, so the land is never restored |
| 0x88293C in `fn_00882730` (the temple's InitTemple, slot +0x234 of the citadel 3D vtable 0x9A2BFC, from `CitadelHeart::CallVirtualFunctionsForCreation` 0x4675A0) | the temple flattening, 17 × 17 cells | only the temple's own 3D object: vt+0x1E8 = `UpdateMelting` 0x8168F0 (0x88296C, its melting deltas retaken on the new land), the entrance mesh, `ComputeBoundingBox`, `fn_008829C0`, vt+0x208 = `fn_00882B10` (0x8829AF, the outside baked to the land) |

No script command writes altitudes (the CHL `GetLandHeight` 0x6FB1F0 only reads, `refs.py e9c564`); none of the other
readers of `g_ptr_blocks` stores into byte +4 (`fnw.py`: the only byte stores are +3 in `fn_008229B0` / `fn_00822F90`).
None of the three paths walks the map cells, sends a notification or touches another object. No random draw
in any of them (the calls of 0x5FF4F0, 0x5FF350, 0x882730, 0x8826C0, 0x882AC0, 0x8829C0 listed: none is a
GRand / GameRand / FloatRand).

#### Where each class takes its drawn height

An object's position is `MapCoords` (x, z, altitude above the ground). Its 3D object (Game3DObject, +0x40) has its own
matrix +0x14..+0x40, whose translation y is written only when something sets the position:
`Game3DObject::SetPosition(MapCoords)` 0x63B680 = `GetAltitude(pos) + altitude` into +0x3C (0x63B69E..0x63B6B3),
and the same in `SetPositionAndXZYScale` 0x63B330, `Object::SetXYZAngles` 0x638D00, `SetXYZAnglesAndScale` 0x638F80.
`GetWorldMatrix` (vt+0x63C; `Object` 0x638200, `MobileObject` 0x607560, `MobileStatic` 0x608DE0) is computed fresh
from the MapCoords, but nothing calls it to draw except `Fragment::Draw` 0x76EC0F and the hand hover (below).

**A. Drawn from the 3D object's stored y (does not follow a land change).**
- `MultiMapFixed::Draw` 0x518090 (Feature, BigForest, MobileStatic → Rock, Bonfire, DeadTree, FelledTree…; `Abode::Draw`
  0x515F70 ends in it or `DrawBuilding`): land colour `fn_00801C90` and fade `fn_007FEB30` at +0x38, then `AddForDrawing`
  0x63B5D0. No position write.
- `MobileObject::Draw` 0x518150 (pots and other mobile objects at rest): the same.
- `Tree::Draw` 0x74AB00: reads +0x3C (0x74ABC2), and the sway saves the 12 floats +0x14..+0x40, draws with a
  temporary matrix whose translation is +0x38.. plus the sway offset, and restores them (0x74AED9, 0x74AF32, 0x74B0F9).
- `Field::Draw` 0x528570: no GetAltitude; the field's mesh is morphable.
- The morphable objects (3D types 1 / 8: fields, morphable buildings, BigForest, piles' crater…): `UpdateMelting`
  0x8168F0 stores per vertex `−(H(origin) − H(vertex)) / scale` (0x81695B, 0x816A59..0x816A77). It is called only at
  creation (`CallVirtualFunctionsForCreation` of BigForest 0x439061, CitadelHeart 0x467759, CitadelPart 0x469772,
  Creche 0x50AB2A, Flowers 0x527A46, Field 0x528AE9, Football 0x53113F, Graveyard 0x595DE1, PileFood 0x66E1B2,
  StoragePit 0x732E91, TownCentre 0x743D05, Wonder 0x779180, Workshop 0x779445, WorshipSite 0x77BB54; the scaffold
  0x6E8B03, the waterfall 0x5E3C55 / 0x5E3DBE, the roots pile 0x8252C3), on every draw only by `PhysicalShield`
  (0x72D01E), and by the temple after its own flattening (0x88296C). The deltas are a snapshot.
- Collide data made once from `GetWorldMatrix` (`Tree::CreateCollideData` 0x74C604, `CitadelHeart` 0x46903B,
  `WorshipSite` 0x77E66B) is not remade either.

**B. Drawn at `GetAltitude(MapCoords) + altitude` on every draw (follows the land at once).**
- Living: `Living::Draw` 0x51AEC0 / `Villager::Draw` 0x51B940 (via `fn_0051B3D0`), `Animal::Draw` 0x51C310, `Dove::Draw`
  0x41F777, `SpellWolf::Draw` 0x51C579 all call `fn_0051AF00`: while moving for an animation, the position lerped from
  the previous point (+0x2C, `GetAltitude` + its +0x34, 0x51AFBF) to the current one (+0x14, `GetAltitude` + +0x1C,
  0x51AFF1); otherwise (x, `GetAltitude(+0x14) + [+0x1C]`, z) (0x51B0D9..0x51B0E2), written as the 3D object's
  translation (`Translation` 0x403530 / `PostTranslation` 0x403570, 0x51B17C..0x51B1BD).
- `PileWood::Draw` 0x51BC40 / `PileFood::Draw` 0x51BF80: while not fully buried (−GetHeight < the sink +0x84,
  0x51BD27..0x51BD38) the translation is (x, `GetAltitude(pos) + sink`, z) (0x51BD8F, 0x51C153). Otherwise not drawn.
- `SpellIcon::Draw` 0x5197DA, `GBaseOnly::Draw` 0x609562 (`Translation` at 0x60958F), `MagicTeleport::Draw` 0x5FCCD3,
  `Totem::Draw` 0x51AC3C (its MapCoords +0xC4), `TotemStatue::Draw` 0x738B63.., `SpellDispenser::Draw` (GetLHPoint
  0x722A21), `TownArtifact::Draw` (0x51C9C3, SetPosition 0x51CAE8), `Scaffold::Draw` (SetPosition 0x6EA69A),
  `Fragment::Draw` (GetWorldMatrix 0x76EC0F).
- Physics objects in flight: `PhysicsObject::GameTurnUpdate` reads `GetAltitude` every turn (0x645CE7, 0x645DED,
  0x645E7D, 0x64611F).

**C. Re-seated only as a side effect.**
- A **tree that grows** (`Tree::Grow` 0x74A3F0, from `Tree::Process` 0x74A33E, the forests 0x53A548 / 0x53A580,
  `ApplyWaterSpell` 0x74C443, 0x6D78CB): after `SetJustScale` it rebuilds the whole matrix with the translation
  (x, `GetAltitude(pos) + [+0x1C]`, z) (0x74A4CA..0x74A561). Only forest trees grow: none in Land 1 and Land 2.
- A **tree under the cursor**: `AddForDrawing` 0x63B5D0 → `GInterface::SendObjectDrawCollision(obj, 0, NULL)` (0x63B66E,
  when `g_b_last_on_screen` and `g_last_selected_box`); for an object whose info type (+0x28 → +0x10) is 6
  (`OBJECT_TYPE_FOREST_TREE`) and in the map, `GetWorldMatrix` is written straight into the 3D object's matrix
  (0x5D58DC..0x5D58FB), which seats it on the current land.
- Any later move of an object through the setters above (script `SET_POSITION` 0x6F8B78, angles, scale).

The creature's body: `Creature::Draw` 0x517910's GetAltitude calls are its debug lines and bubble (0x517A08, 0x517B5B,
0x517BD4, 0x517E32; objects at mind +0x222C and +0xF58); `LH3DCreature::UpdateTime` 0x4826BF drops its result. The
body's height (LH3DCreature +0x7C) is rewritten from GetAltitude only by its moving, move-straight, fight and discard
actions (`UpdateMotion` 0x490D6B, `DoMovingAction` 0x48D42C, `ProcessMoveStraight` 0x491440 / 0x491508,
`DoFightActionAction` 0x48B3F0 / 0x48B431, `DoDiscardAction` 0x48C8DB), so a creature standing still keeps its old
height after a land change (🔍 inferred: that +0x7C is the drawn y). Footpaths: not
looked at (no drawing found).

#### The vortex and the objects under it

The In takes in, once Active (after the 7 s FadeIn during which it flattens), what `CanBeSuckedIntoVortex` vt+0x640
allows: `Object` 0x639B60 = `CanBecomeAPhysicsObject` vt+0x7B0, which is 1 for Tree 0x74B630, Living 0x5ED4F0,
Villager (via vt+0x530), MobileStatic 0x609320, MobileObject 0x607AA0, DeadTree 0x511A10, FieldCrop 0x6082D0, Pot by its
info (0x66E8F0), Scaffold by state; and 0 for Object 0x6377E0 (Feature, BigForest, AnimatedStatic, CitadelHeart/Part,
WorshipSite, Flowers, GStreetLantern, GBaseOnly), Abode 0x406800 (and every abode class), Field 0x528030, Bonfire
0x4397A0. The flattening reaches 56 m and the take 50 m (25 m two turns in three): what stays under the edge, and every
building, field, feature and forest, keeps its drawn height.

## openblack

Status: **done**. The height (`LandIsland::GetHeightAt`), the normal (`land_normal::OfCell`), the small bump and the
light table (`3D/LandLightTable`) are exact ports; the land line (`land_line`) keeps the original's float steps.
Progress: [Landscape rendering](../project/progress/terrain/landscape_rendering.md).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Terrain height**
  - openblack: `LandIsland::GetHeightAt`, exact)
  - openblack's terrain rendering uses the same triangulation.
- **Terrain normal**
  - openblack compares with `GetCellsPerSide()` (512 in the game's maps).

### The land line

- **The land line's float steps (read from the exe).** openblack's `land_line` (`3D/LandLine.{h,cpp}`, under
  raffclar's names; `LandIslandInterface::RayCast` and `RayCastCells` are thin wrappers over it):
  - Metres to cells: fn_00802550 multiplies x and z by a **float** tenth, `fmul dword ptr [0x8AC404]`
    (0x802566, 0x802573, 0x80258C, 0x802599; bytes `cd cc cc 3d`, 0.1f = 0.100000001490116) and divides y by the
    float 0.67 (`fdiv dword ptr [0xC3720C]`, 0x802559 and 0x802580; `1f 85 2b 3f`), each stored as a float. A float
    times a float is exact in the FPU before the store rounds it once, so `x * 0.1f` in float gives the same bits;
    the division rounds the same way too. The hit goes back to metres by `fmul dword ptr [0x8AB414]` (10.0f).
  - The flat-triangle threshold: fn_0083AE80 compares `|denominator|` with `fcomp qword ptr [0x8C79D8]` for both
    triangles (0x83B026 and 0x83B161), a **double** whose bytes `00 00 00 e0 e2 36 1a 3f` are
    9.9999997473787516e-05, the float 0.0001f widened, not the double 0.0001.
  - So openblack keeps `0.1f` and `(double)0.0001f`. raffclar's `land_line` multiplies by the double 0.1 and compares
    with the double 0.0001: a metre value whose double tenth rounds to another float, or a denominator between
    9.99999975e-05 and 1e-04, comes out differently there (not measured yet; a change of behaviour of its own).

### The small bump

- openblack used to do `col *= 1 − smallbumpa` (dark blotches) with only the alpha and UV × 10.
- `LandIsland::CreateSmallBumpTexture` (RGBA with 4-bit quantisation), per-vertex fade in `vs_terrain`
  (`u_smallBumpLine`, `u_skyAndBump.w`), blend in `fs_terrain`.
- Before the shore audit openblack gave alpha = fade on every triangle with any vertex of altitude > 1, also
  on its shore vertices: a semi-transparent greenish film over the shallow water near the camera, cut along the
  triangle edges (straight and diagonal) and changing with the zoom. Now `vs_terrain` computes (c, a) per vertex like
  the original (`v_smallBumpFade`, vec2).

### The light table

- `3D/LandLightTable` (exact port, checked against a Python reference), 256×1 texture that
  `vs_terrain` samples per vertex; `u_seaColour` for the sea. `Build(skyType, alignment, overcast, flash)`:
  cap `min(c, ftol(255 − 96·overcast))` without clipping the cloudiness (0x869ADB), flash `c += ((0xFF − c)·f) >> 8` with
  alpha 0xFF across the whole table (0x869C25) and the storm/lightning haze; `LandLightTable::Current().GetRaw(i)` (copy
  of the last `Build`) is the global table 0xEDD90C read by the ring creators (`WaterRingSystem::Add`); the renderer uses
  `GetRaw` of its own table; `Current()` also keeps the base [0xFA26A4] (`GetRawBase`) and the haze, which are read by
  the PSys mists and the storm clouds. Cloudy = `[0xFA2754]` (`GCamera::Update` 0x4426BA: byte 3 of
  `GetWeatherSmooth` at the camera × 0.01) and flash = `[0xFA2768]` (`Update3D` 0x83587C, the closest storm that
  contains the camera). The cloudiness comes from a single source, `Clouds::WeatherOvercastAtCamera()`; `3D/SkyWeather`
  only gives the flash (`weather::LightningFlashAtCamera(camera)` from `ECS/Weather/LightningFlash`). Test:
  `test_land_light` against a Python generator (exact inputs in binary: with 1.3 or 0.6 the
  float columns fall on the other side of an integer from Python's doubles). The alignment is the sky's smoothed one (`Renderer::_skyAlignment`, [0xBF3378]).
  The sky type of the column is the frame's sample [0xFA26BC] (`sky_type::Frame()`; column
  `sky_type::LightColumn` = (2 − T)·15, haze `sky_type::HazeFactor`, see
  [sky type](sky.md#sky-type)); `Build(T, …)` receives it as is. The
  original computes the column with `Time2SkyType([0xFA26C4])` (0x869859) and the haze with [0xFA26BC] (0x869D5F): it is
  the same value, because `fn_00869850` runs in `fn_0086A330` right after `fn_0086A2C0` (DrawSky 0x5E2226..0x5E222B).
  With the old convention (2 − T by the forwarder) the table came out identical at every hour (same float column);
  only the last bits of near/far change: v'² = v·v and then `v'²·c + base` (0x869D7A..0x869DAB) instead of
  `base + c·v·v` with v = 1 − |S − 1|, and the exe's exact constants [0x9A3BE0] = 1/900, [0x9A3BD8] =
  0.00013888883, [0x9A3B70] = 1/15, [0x9A3BD4] = 1/350 instead of 0.00111111 / 0.000138889 / 0.0666667 /
  0.00285714 (`test_land_light`, `LandLightTable.MatchesTheOldConventionEveryHour`).

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/Implementations/LandIsland.cpp`: `CreateSmallBumpTexture`, `LandIsland::GetCellCorners`, `LandIslandInterface::RayCastCells`, `RayCellTest`, `RayOutCode`.
- `src/3D/LandIslandInterface.h`: `GetCellCorners`.
- `src/3D/LandLightTable.h`: `GetMoonColour`.
- `src/3D/LandNormal.cpp`: `EdgeFormula`, `LengthTable`.
- `src/3D/LandNormal.h`: `OfCell`.
- `src/Tests/test_land_light.cpp`: `OriginalHazeDistances` (the haze distances with the exe's floats).
- `src/Tests/test_land_raycast.cpp`: since the walk moves the end to the map's edge, a ray to a point one metre ahead
  still finds the ridge 65 m on; a ray going up gets no fallback (0x8025D9..0x8025EA).

### Test hooks

- `OPENBLACK_DUMP_BLOCK_TEXTURE` and the tests `test_land_light` and `test_haze_land_light`
  ([Test hooks](../rendering/frame.md#test-hooks)); the full list is in
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- Heights after a land change, the creature's body: known, its height (LH3DCreature +0x7C) follows the land only
  while a moving, move-straight, fight or discard action runs (see
  [Where each class takes its drawn height](#where-each-class-takes-its-drawn-height)). Still open: that +0x7C is the
  drawn y, and openblack's creature body draw is not checked against it (`xref.py callers 0x803090`).
- Whether footpaths are drawn in the retail game: not looked at (no drawing found).
