# Terrain

## Summary

The land is 512×512 cells in 32×32 blocks of 17×17 shared corners. Its height at a point is the original's integer
blend over the cell's triangle (the split bit picks the diagonal, heights near the sea edge flatten to 0), its normal
comes from two lookup tables, each land vertex is lit by a 256-entry light table built every frame from `palette.raw`
by time of day, alignment, overcast and lightning, and a second "small bump" pass adds light detail that fades with
distance in front of the camera. Only the vortex and the temple change heights at run time, and most objects keep the
height they were drawn at. Each block's texture is painted again through a queue worked every frame: requests made as
out-of-date blocks are drawn, sorted nearest first, six worked a frame (or a whole block at the highest detail), one
finished a frame, the block keeping its old texture until then.

openblack ports the height, the normal, the light table and the small bump exactly, and the block texture queue for
the land's changes, with each block's footprints and static shadows drawn again at its prints step (the land's first
paint is still all at once), and the texture detail line: beyond it a block shows its low-resolution tile, across
its band the tile over the block's texture.

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
- physics (`src/Physics/Body.cpp` (was `PhysOb.cpp`) `Normal` through `physics::Ground::NormalAt`: `Body::SettleOnLand`
  for `AdjustToGroundLevel` 0x7FCBE2, `Body::TouchGround` for `GroundAndWater` 0x7FD93F);
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
- The moon colour (palette row 5 at the alignment column; confirmed: xref immediates) is stored at [0xFA26DC]. Its one reader is the sky draw
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
  The test, after the Zoomer's step: `call [vt+0x42C]` (GetHeight), `fchs`, `fcomp` with the sink, `test ah, 1` /
  `je` past the whole draw (PileWood 0x51BD27..0x51BD38, PileFood 0x51C068..0x51C079): the pile is drawn only while
  the sink is above −GetHeight; at or below it nothing of the pile is drawn (an unordered compare draws). At the game
  thread's 24-bit precision an empty pile's target, (0 − 1) × GetHeight (`PileResource::SetSize` 0x66E918..0x66E957),
  is exactly −GetHeight: an empty pile at rest is not drawn. PuzzleGrain's Draw (0x561A20) is a plain jump to
  `MobileObject::Draw`, with no such test.
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
height after a land change (🔍 inferred: that +0x7C is the drawn y). Footpaths are not
drawn (read 2026-10-25): the classes `GFootpath`, `GFootpathNode` and `GFootpathLink` have no draw of their own (their
only text is `GetDebugText`, which nothing calls), and the 20 places that read the game's footpath list
(g_game +0x205C64) are the footpath code itself (0x534E50..0x538970: the constructor, `ToBeDeleted`, `SaveAll`,
`GFootpathLink::LoadAll`, `SendFootpathsAroundObsticle`, `StopGoingRoundObsticle` and their helpers), the `GGame`
constructor (0x54B43D) and the map script's footpath commands (0x71766D, 0x7177D6); none is in the drawing.

#### The vortex and the objects under it

The In takes in, once Active (after the 7 s FadeIn during which it flattens), what `CanBeSuckedIntoVortex` vt+0x640
allows: `Object` 0x639B60 = `CanBecomeAPhysicsObject` vt+0x7B0, which is 1 for Tree 0x74B630, Living 0x5ED4F0,
Villager (via vt+0x530), MobileStatic 0x609320, MobileObject 0x607AA0, DeadTree 0x511A10, FieldCrop 0x6082D0, Pot by its
info (0x66E8F0), Scaffold by state; and 0 for Object 0x6377E0 (Feature, BigForest, AnimatedStatic, CitadelHeart/Part,
WorshipSite, Flowers, GStreetLantern, GBaseOnly), Abode 0x406800 (and every abode class), Field 0x528030, Bonfire
0x4397A0. The flattening reaches 56 m and the take 50 m (25 m two turns in three): what stays under the edge, and every
building, field, feature and forest, keeps its drawn height.

### The block texture queue

Every claim here is from the x86 of the frozen v1.42 runblack.exe; the frame numbers below were run natively through
the oracle (`tools/re/oracle`) with fake blocks and requests, and the unit tests are pinned to them.

#### Who asks for a block texture, and when

- Each land block has its own texture object (block +0x944, made with type 0x48 and the block's index as its user
  data +0x18; its material +0x948) and a place in a shared low-resolution page (block +0x9C0, its place
  +0x9CC/+0x9D0). `LH3DTexture::Create` 0x8379E0 makes a type-8 texture with +0x138 = 1 (0x837C52): **out of date**,
  so a block's first paint takes the same path as a repaint. Texture +0x134 is the texture's paint callback.
- `LandBlock::SetNeedUpdate` (fn_008786D0) only sets +0x138 = 1 on the block's texture (type 8 or 4, and only while
  the land has textures, [0xE9CD90] = 0). Its callers: the altitude writer fn_00800DA0, once for each block that keeps
  the corner (see [What changes the land at run time](#what-changes-the-land-at-run-time)); `RequestChangeTexture`
  0x5E2FF0 → fn_005E3260 (an object put on or taken off the map's cells, a field or a multi-map building built: the
  blocks under it); fn_0081E9E0 (adds a footprint record to [0xEB9968], unless one with the same mesh and the same
  12-float matrix is there, then marks the blocks of its box: from `CreateRiver` 0x734337 / 0x734365 at the map's
  load, before any land draw; `DesignedWaterFall`; `GBaseOnly::CallVirtualFunctionsForCreation` 0x60945A; the
  creature sub-action sub_5062C0, 0x4FBEE5); the snow cover's fn_0086C5D0. **Dead** in this exe (no call, no jump and
  no pointer to them anywhere in the image, searched 2026-10-22): sub_8012C0 (nine calls, a cell writer of the same
  shape), sub_8015A0, sub_871AE0 and sub_601AE0 (every block: "update all the land's textures").
- The land file's loader (fn_008015D0 for a `.lnd`, 0x8016E9; `LH3DIsland::Create` 0x804417 for the other format)
  gives each block texture the **synchronous** painter sub_873700 (the 16 strips, then the prints, at once). The
  first `LH3DIsland::PreDraw` 0x7FF2D0 after an island opens ([0xC3722C] = 1, set by fn_00803890) replaces it on
  every block with the **queueing** callback sub_872050 (0x7FF43A). For a `.lnd` land (fn_00804CA0: the file name
  ends in "lnd") that PreDraw binds no block material first, so the synchronous painter never runs and a land's first
  paint goes through the queue too.
- The callback runs when the texture is bound for drawing while it is out of date (the type-8 case of the bind
  fn_00837DF0, 0x837EC6..0x837F74), which clears +0x138. A block binds its own texture only from its draw
  (fn_00874AA0, 0x8753AF), with texture level 0 or 2 (below), and only when it has triangles to draw; the reflected
  land (fn_007FF4F0) is drawn first and binds too. Both go through the visible list [0xFA92D8], nearest first (stages
  4c and 4i of [the frame](../rendering/frame.md#draw-stages-in-order)), so a frame's requests are made in that order.
  A block out of view, or wholly beyond the texture detail line, is not asked for until it is drawn with its own
  texture again.
- sub_872050(block), the request: a block already in the queue [0xFA76BC] goes back to strip 0 in its place
  (0x87206A..0x87207D). Otherwise a new 0x24-byte request goes **in front**: +0x10 the block, +0x14 strip 0, +4..+0xC
  the centre (block +0x90C + 80, 0, block +0x910 + 80; 80.0f at [0x8D060C]), +0x18 a surface of its own, 256 × 256
  (128 × 128 with the low-resolution option [0xEDD470]) from `IDirectDraw::CreateSurface`, +0x20 the next. Either way,
  when the block has no card texture yet ([+0x944]+4 = 0) its **pending** mark +0x92C = 1. It returns 0.

#### One frame of the queue

`LH3DLandscape::TextureUpdateThread` 0x871F00 is not a thread: `GGame::Process3dEngine` calls it once a frame
(0x54DEB2), in the preparation stage, after the land's PreDraw and before the land is drawn.

1. Every request's distance +0 = sqrt((dz dz + dy dy) + dx dx) from the camera position `g_camera` +0..+8 to its
   centre, each operation rounded to a float at the game's 24-bit precision (0x871F31..0x871F75).
2. Passes of adjacent swaps until a pass makes none, a request swapped back only when strictly farther (`fcomp`,
   `test ah, 0x41`, 0x871F7A..0x871FC5): a stable sort, nearest first; equal distances keep their order.
3. The nearest request is worked once (0x871FD1), then the requests from the nearest on are worked once each, at most
   **6** ([esp] = 6 at 0x871F1F; the nearest counts once though it is worked twice). After each, a request whose strip
   is past 16 is **finished**: uploaded (fn_00871DF0: the card texture made if missing, the surface copied into it by
   fn_0085DC00, the block's pending mark cleared), unlinked, freed (fn_00871D50 releases the surface, then `delete`),
   and **the frame's work stops there** (0x872010..0x872046): at most one block finishes a frame.

Working a request (fn_00871E50, Mac name `UpdateListElement::Update`): the surface is locked (fn_00871D70; a request
without a surface does nothing). While the strip is below 16 it paints **strip** `strip` (fn_00873790, Mac
`ProcessTextureInBackground`) and the strip counts up; at the detail index 4 (fn_00804E60: [0xC381EC] = 4, the level
`LH3DLevelOfDetail::UpdateFromRegistry` 0x823B32 stores) it paints 16 strips from there at once instead. When the strip
is then exactly 16 it puts in the **prints** (below), and the strip counts up once more; the surface is unlocked
(fn_00871DD0). So:

| Detail index | A lone request | Several requests |
|---|---|---|
| 4 | painted whole and finished in its first frame | one block a frame, the nearest; the others are not touched |
| 0..3 | 2 strips a frame: finished on the 9th frame (8 frames of strips, then the prints) | the nearest 2 strips a frame, the next five 1 each, the rest wait |

A request that changes from a lower index to 4 part way paints its strips from its own on, 16 of them (the ones past
15 into memory after its surface, a bug of the original), skips the prints and finishes.

#### A strip

fn_00873790(strip, buffer, pitch, block) paints the cells (strip, 0..15), `cell[strip * 17 + column]`, 16 × 16
texels each into rows `strip * 16 ..` of the surface (8 × 8 at low resolution, fn_00873AB0). A cell whose four corners
have one country is painted by it (fn_008732C0) or, with bit 0x200 (open sea), left clear (0x873A01: zero texels, the
material offsets moved on); otherwise each corner's country paints into a side buffer (0xFA7230, 0xFA7030, 0xFA6E30)
and fn_00871850 blends them. The material and noise offsets [0xFA768C] / [0xFA7688] start at strip << 12 and move 16
a cell. A strip reads only its own cells' corners, so the 16 strips of a request painted from unchanged cells give the
whole block's texture.

#### The prints (the 16th step)

At strip 16 (0x871EBF..0x871EE8; the same in the synchronous painter, 0x87372B):

- fn_008721A0 (Mac `DrawShadowOnTexture`; the block's x and z indices +0x914 / +0x918): for each object the map lists
  over the block (the enumerators [0xFA76A0] / [0xFA76A4]) whose vt+0x84 or vt+0x8C answers and vt+0x64 does not: its
  **static shadow** (an object with vt+0x94 draws its own, vt+0x164(x, z); the others' mesh vt+0xF8 is rasterised
  along `DEFAULT_SUN` 0xEA1C88 by fn_00806DA0 into the byte map [0xFA76C0], 257 × 257, cleared before the first), and
  its mesh's **footprint** when the mesh has flag 0x8000 and footprint data (+0x48): fn_00872FA0(footprint, surface,
  x, z, 0). Then the list [0xEB9968] (next +4): each footprint whose box (+0x3C..+0x48) meets the block, its own
  strength +0x4C into [0xC39804] and +0x38 passed on, through fn_00872FA0. Then the **snow cover** (`g_snowcover`
  [0xFA2918], fn_0086CBA0). Last, when any shadow was drawn, each texel with shadow s ≠ 0 has its three colour
  nibbles multiplied by 255 − (table[0xFA94C4][s] >> 1) and shifted down by 8, alpha kept (0x87256A..0x8725E4).
- fn_008002C0 (Mac `LowResTexture::UpdateSmallTexture`; not a decal pass): the finished texels are reduced into the
  block's place in the low-resolution page (block +0x9C0, +0x9CC / +0x9D0) with its neighbours' borders
  (`g_index_block`); the page is marked out of date (+0x138 = 1, 0x800BEF) and block +0x9D4 = 1.

So a building's print goes when its block is painted again: its removal asks for the blocks under it
(`RequestChangeTexture`), and the print is gone once that block's request finishes, one block a frame at the highest
detail.

#### The blocks an object marks

`RequestChangeTexture` 0x5E2FF0 (an `Object`): nothing without its 3D object (+0x40). It saves the 3D object's matrix
(+0x14..+0x43), makes it again from the object (scale `GetScale` vt+0x120, angle `GetYAngle` vt+0x508, the place from
`Pos` × 10 / 65536 with the land's height plus `Pos.altitude`): the scale on the diagonal, then, when the angle is not
0, `RotateY` (r0' = c r0 + s r2, r2' = c r2 − s r0, 0x5E30EB..0x5E3154 and 0x5E3166..0x5E31E1); +0x44 / +0x48 take the
scale and the angle; it calls fn_005E3260(3D object) and puts the saved matrix back.

fn_005E3260 (0x5E3260..0x5E3744), run natively through the oracle (`tools/re/oracle`) on fake objects, also through
`RequestChangeTexture` with fake game objects (the unit tests are pinned to it):

- Nothing until the land has been drawn once since the game started ([0xE9CD78], set at the end of every land draw
  fn_007FF610, never cleared), nor for a null object, nor unless `IsShadowOnTexture` (vt+0x84, 3D object +4 bit 12)
  or `IsFootPrintOnTexture` (vt+0x8C, +4 bit 14 for the meshed objects, 0 for some classes) answers.
- The rectangle starts at the matrix's place (tx, tz). With the shadow: the mesh's box (`GetMesh` vt+0xF8, box centre
  +0x18, half size +0x24; its +0x14 is zeroed) gives its eight corners (± half size), each through the matrix, x =
  ((m6 pz + m3 py) + m0 px) + tx, y = (m1 px + m7 pz) + m4 py (its own height cancels out), z = ((m5 py + m2 px) +
  m8 pz) + tz, then moved along the sun: (x + kx y, kz y + z) with kx = [0xC398E4], kz = [0xC398E8]. Those are
  1.0 and 1.0 in the image and the static shadow's draw fn_0080ECB0 writes −(sun.x / sun.y) and −(sun.z / sun.y) of
  the normalised `DEFAULT_SUN` (−500000, 500000, −500000), so they stay 1.0. With the footprint, when the mesh has
  flag 0x8000 and footprint data (+0x48: a count, then its first entry with its triangle count at +0x14 and its
  triangles of 0x30 bytes from +0x18, the three (x, z) first): each corner (fz m6 + fx m0) + tx, (fx m2 + fz m8) + tz.
  A point lower than the lowest side moves it; otherwise a point higher than the highest side moves that one. Every
  operation is a float one (the game's 24-bit FPU).
- The sides × 0.00625 (1 / 160), truncated toward zero (`__ftol`). A lower side below 0 becomes 0; an upper side of
  0x200 or more read unsigned (so also a negative one) becomes 0x1FF. Then every block of the rectangle within
  `g_index_block`'s 32 × 32 that exists gets `SetNeedUpdate`. An object wholly at a negative x so marks its whole row
  of blocks from x = 0.

Its callers, by class (the 3D object's `Flags2` +8 bit 0x100 gates the `Object` ones; no writer of that bit was found
in the exe, so they never mark):

| Where | When |
|---|---|
| `Fixed::InsertMapObjectToCell` 0x52DEC2, its AssumeFixed form 0x52DF02 | a SingleMapFixed, a MultiMapFixed (each cell) or a FishFarm (each cell) put in a cell |
| `MultiMapFixed::InsertMapObject` 0x52E719 | once more after its cells (also with none) |
| `MultiMapFixed::RemoveMapObject` 0x52E7E9 | after it left its cells; a SingleMapFixed or a FishFarm leaves its cells through `Object::RemoveMapObjectFromCell`, so it marks nothing |
| `Object::InsertMapObjectToCell` 0x6368B6, `RemoveMapObjectFromCell` 0x636A1A | only with `Flags2` bit 0x100 |
| `MultiMapFixed::Built` 0x52EC33 | not a field, with its 3D object: after `SetShadowOnTexture(1)` |
| `Field::Built` 0x52A0BC | after `Abode::Built`, always |

A turn or a scale goes through them too: `Object::SetScale` 0x639200 (a tree growing) and the angle setters call
`SetXYZAnglesAndScale` 0x638F80, which takes an object in the map out (vt+0x548), sets them, makes the 3D matrix again
and puts it back (vt+0x544), so a Fixed class marks its blocks with its new size.

fn_005E3260 has one more caller, sub_5062C0 (an object at +0x11F4 with fn_0081E9E0 first, not read further).

#### Who asks for its footprint

Both the prints step (fn_008721A0, 0x8722F2) and the marks (fn_005E3260, 0x5E3292 and 0x5E3484) ask the 3D object's
`IsFootPrintOnTexture` (vt+0x8C): in every meshed 3D class (`LH3DStaticObject` 0x9A2974, the boned type 0x9A32A0,
0x9A2748, `LH3DCitadel` 0x9A2BFC, 0x9A2E34) fn_007FA2E0, bit 14 (0x4000) of +4; the sprite-like vtable 0x9A22E8
answers 0 (fn_007FA2F0). The marks read the footprint's corners only with that bit (0x5E3484..0x5E348C), after the
box with the shadow bit (vt+0x84, bit 12). Read from the exe (checked 2026-10-26):

- `LH3DObject::Create` 0x80B4D0 makes a type 0 (static) or 2 (boned) object for a game object (`Object::Get3DType`
  0x6364F0: the mesh's header bit 0x100 >> 7); `LH3DMeshedObject`'s constructor 0x8164B0 sets +4 = 0x10009, so the
  bit starts **off**. `Fixed::Create3DObject` 0x52DE30 then sets the **shadow** bit on (vt+0x80 with 1) for every Fixed
  class that makes its 3D object through it (abodes, animated statics, features, the citadel's heart, worship sites,
  fields not: `Field::CallVirtualFunctionsForCreation` makes its own with `Game3DObject::Create`).
- The only writer of the bit is `SetFootPrintOnTexture` fn_007F9DC0 (vt+0x88 of all the meshed classes; no direct
  call; a sweep of the code for an `or` or `mov` of 0x4000 at +4 finds only the mesh header's own bit, set on the
  mesh at 0x42543C, 0x60719E, 0x726178, 0x7374A8, 0x77BB65). Its calls on a 3D object:

| Where | Value | When |
|---|---|---|
| `AnimatedStatic::CallVirtualFunctionsForCreation` 0x4223B3 | 1 | always, before its insert in the map (0x4223EE); the shadow then off (0x4223CD) |
| the same, 0x42240C | 1 | again when not built (`IsBuilt` vt+0x890), the shadow off (0x422419) |
| `Field::CallVirtualFunctionsForCreation` 0x528A9B | 1 | always, after its mesh |
| `MultiMapFixed::CallVirtualFunctionsForCreation` 0x52EA33 | 1 | when `IsBuilt` is not TRUE, after `InsertMapObject`; the shadow then off |
| `WorshipSite::CallVirtualFunctionsForCreation` 0x77BB76 | 1 | always, after its insert (0x77BB49); its shadow on too (0x77BB86) |
| `LH3DCitadel::InitTemple` 0x882777 (from `CitadelHeart::CallVirtualFunctionsForCreation` 0x46767F) | 1 | always |
| `LandscapeVortex::Create3DObject` 0x5FEDFF | 0 | always |
| `MagicShield::CallVirtualFunctionsForCreation` 0x72C2CB, `PhysicalShield`'s 0x72CD0B | 0 | always |

  The other vt+0x88 calls (`RemoveFootpath` of `Dance` 0x50CB29, `MultiMapFixed` 0x52EE96, `Forest` 0x53ACD6;
  sub_6F6A30) are on other objects' tables. Nothing clears the bit later: `MultiMapFixed::Built` 0x52EC33 only turns
  the shadow on (not for a field) and asks for the blocks under it.
- A **building site** has no 3D object and no bit: `BuildingSite` is a `GameThing` (its constructors 0x43B700 and
  0x43B7E0 call `GameThing`'s 0x56FA10 and set the table 0x8C6B6C), whose table has 75 entries (to +0x128), so it has
  no `CallVirtualFunctionsForCreation`, `Create3DObject` or `IsBuilt` slot (none of 0x52E890, 0x52DE30, 0x6365F0 or
  0x636BE0 is in it); read past its end, +0x890 is another table's 0x401D90 (`ret 4`). The bit 0x52EA33 sets when not
  built is the building's own (the `Abode` made under construction that the site builds).
- `MultiMapFixed::CallVirtualFunctionsForCreation` 0x52E890 is the creation of Abode (0x40320C), BigForest, CitadelPart,
  Flowers, MobileStatic, SpellIcon, TotemStatue and WorshipTotem, and the table entry of Feature, PrayerSite and
  WorshipSiteUpgrade. `IsBuilt` is 1 for `MultiMapFixed` 0x438D80 (BigForest, MobileStatic, PrayerSite, Rock, the
  vortices…), and for the others (`Abode` 0x4016C0, `Feature` 0x422110, `CitadelPart` 0x464AD0, `SpellIcon` 0x55D3D0,
  `WorshipSite` 0x77BDD0, `TotemStatue` 0x5610B0, `Football` 0x5312B0, `PFootball` 0x643910) 1 when the
  under-construction bit (+0x58 bit 1) is clear and the percent built (`GetPercentBuilt` 0x4014F0, +0x5C) is not below
  1.0 ([0x8AA390]). The map's and the scripts' features and animated statics are made with percent 1.0 and not under
  construction (`Feature::Create` 0x527350 from fn_006F11A0, 1.0 at 0x6F127B and ebp, zeroed at 0x6F11B0; the land
  script's set-up, 1.0 at 0x716A57 and its zero register ebx), so they are built.

So the footprint goes into the land's textures for a **field**, an **animated static** (of the 16 records only the
Norse gate's mesh 212 and the Piper cave entrance's mesh 155 have the landscape feature bit 0x8000 and a footprint), a
**worship site**, the **citadel's heart**, and a **building made not built** (a plan put up by its builders), which
keeps it once built. A building made whole, a feature, a tree, a forest and the others do not have it, whatever their
meshes hold (in the original `AllMeshes.g3d` 176 meshes have bit 0x8000 and a footprint: the buildings, the forests,
the trees, some features and objects). Their prints still come from the shadow bit, as the bake prints a mesh's
footprint for either bit. A building made not built marks its blocks at its insert with its **box** (its shadow is still
on and its footprint off then), and with both once built.

#### What a block shows meanwhile, and the far blocks

The block's texture level +0x938 (fn_00877210, 0x87751B..0x877704): 1 while the block is pending (+0x92C, no card
texture yet); otherwise from its four ground corners against the **texture detail line** `g_texture_lod` 0xE9C508
(rebuilt every frame like the mesh lines, fn_007FEE60 in PreDraw; v = (x − point.x) along.z − (z − point.z) along.x,
near above +half width, far below −half width): any corner in the band, or corners on both sides → 2; all far → 1;
all near → 0. The draw (fn_00874AA0): level 1 draws only the low-resolution tile (0x875580); level 0 binds the block's
own texture (so asking for it when it is out of date) and draws it, or, while the block is pending, the
low-resolution tile instead (0x87543B..0x875574); level 2 draws both. A block repainted while it has its card texture
keeps showing its **old** texture until its request finishes. The line's place: 300 ahead with a band 40 wide at
start-up (fn_007FE830, 0x7FE89A); at a land's creation at the detail index 4 (`LH3DIsland::Create` 0x803C25..0x803DBB)
400 / 70, then by the texture manager's surface count [0xF05200]: above 600 → 800 / 170, above 500 → 700 / 170,
above 400 → 600 / 170. At any other index the creation leaves it as it is. [0xF05200] counts the live surfaces of
the texture manager's video-memory pool (+1 per surface made, fn_0085DEB0 0x85DF00; −1 per surface freed,
fn_0085DB10 0x85DB41; 0 at its start, fn_0085D7F0 0x85D825). The pool is filled at the device's start (and again
after a mode change or an Alt-Tab): 16 surfaces of 32 × 32, 3 of 256 × 256 (more with the low-resolution option),
then 256 × 256 16-bit surfaces until one fails or the local bytes reach `MaxVRAM` [0xC392C4]. `MaxVRAM` and `MaxAGP`
[0xC392C0] are read in MB by `start_system` 0x642ECC..0x642EEF from `HKCU\Software\Lionhead Studios Ltd\Black &
White\BWSetup` (values "MaxVRAM" / "MaxAGP"); with the values absent the .data defaults stay: 256 MB and 8 MB. The
test machine's registry has neither (2026-10-22), so the original there uses the 256 MB default: about 2064 surfaces
(16 + 3 + 2045) on any card with room for them, so 800 / 170 at the detail index 4. The count is a function of the
detail index and of `MaxVRAM` (read the same way when present), not of a benchmark; only a card that fails to make
about 600 surfaces (≈ 75 MB) gives a smaller one.

#### The low-resolution pages

A `.lnd` land ships its blocks' low-resolution tiles: `lowResolutionCount` pages (Land1: 7) of 256 × 256 texels, each
4 × 4 tiles of 64 × 64, one per block. Each record: a 20-byte header {texture, material (the editor's pointers), the
tiles handed out on the page (16, the last page fewer), the page's number, the size of the rest + 4}, then a
DirectDraw surface description (DDSURFACEDESC2, 124 bytes: 256 × 256, `DXT3`) and the 65 536 bytes of DXT3 texels.

- The loader fn_008015D0 hands the records to fn_00804A70 (0x801643), which makes one page texture per record
  (fn_007FEDB0; page +8 the tile count, +0xC the number, 0x804ABA..0x804ACD), a DXT3 surface from the record's own
  description (fn_0087F6D0: flags CAPS | HEIGHT | WIDTH | PIXELFORMAT, caps TEXTURE | SYSTEMMEMORY, 0x87F718 /
  0x87F720), and **Blts it into the 16-bit work surface** [0xEDD474] (vt+0x14, DDBLT_WAIT, 0x804AF5), whose texels it
  then copies into the page row by row (fn_00871170 lock 0x804B43, fn_00870910). The work surface (fn_008396A0) is
  256 × 256 (128 × 128 with the low-resolution option) in the texture manager's 16-bit alpha format [0xF05190],
  which is ARGB 4444 when the device lists it (sub_85DC20 keeps the first 4444 format, 0x85DCA1..0x85DCC2); it is
  asked for in AGP memory first (caps 0x20405008) and in system memory if that fails (0x401808).
- **The decode is DirectDraw's**, not the game's. Measured on the test machine (2026-10-22) by a small program that
  makes the same two surfaces through `ddraw.dll` and the same Blt, over Land1's 7 pages and 9 synthetic surfaces
  (every pair of channel values in both colour orders, random blocks, equal colours): every word matches this rule,
  with no exception. The alpha is the texel's own 4 bits. The two colours are widened from 5:6:5 to 8 bits by
  repeating their top bits; when the first colour (as a 16-bit number) is larger, the other two are
  (2 c0 + c1 + 1) / 3 and (c0 + 2 c1 + 1) / 3; otherwise the third is (c0 + c1) / 2 and the fourth black, as in a
  DXT1 three-colour block (not the four colours that Direct3D 10's BC2 always gives). Each channel then keeps its top
  4 bits. On this machine DirectDraw has no hardware (Intel Arc Pro: `DDCAPS_NOHARDWARE`, no video memory), so the AGP
  work surface fails and the original takes its system-memory one, and the Blt is DirectDraw's own; a machine whose
  driver does the Blt may round otherwise.
- Each block keeps from the file: +0x9C0 its page (fn_00804C60 at 0x80171F: the page whose number is the file's
  value, the first page when none has it), +0x9C4 / +0x9C8 the tile's UV origin (floats, the texel origin / 256) and
  +0x9CC / +0x9D0 its texel origin (**ints**, 0 / 64 / 128 / 192, read as ints by fn_008002C0 0x800326..0x800335).
  The file's +0x9D4 (the tile rewritten, 1 in the story lands) is not kept: the loader clears it (0x8016AC, 0x80172B).
  In every shipped land each block's tile is its own 64 × 64 place inside its page.
- **The tile's rewrite** (fn_008002C0, at the prints step of the queue 0x871EE8 and of the synchronous painter
  0x873750; its arguments are the block, the block texture's locked texels and their pitch): block +0x9D4 = 1 at
  once (0x8002CF), the page locked (fn_00838AF0; if that fails, unlocked and nothing more). The tile is a quarter of
  the page's locked row (`pitch / 4`, 0x8004D1..0x8004DB: 64, or 32 with the low-resolution option) at the block's
  texel origin (row +0x9D0, column +0x9CC; both halved with the option, 0x8003F4..0x800415). Its inside, rows and
  columns 1..62 (1..30), takes the texture's texel at row 4 × ⌊r × s / 65536⌋ and column 4 × ⌊k × s / 65536⌋ for
  r, k = 0..61 (0..29), with s = 67650 (`fistp` of the float 0x47842108, 64 / 62; 69905 from 0x47888889, 16 / 15),
  and column 0 the same texel as column 1 (0x800380..0x8003ED); column 63 (31) repeats column 62 (30), then row 0
  repeats row 1 and the last row the one before it (0x8004F1..0x800523). Then the four neighbours, in this order:
  the blocks at z − 1, z + 1, x − 1 and x + 1 (+0x914 / +0x918, `g_index_block`, inside 0..31), each only when it
  exists and its +0x9D4 is set: along z the tile's first (last) column against the neighbour's last (first), along x
  its first (last) row against the neighbour's last (first); each pair of edge texels becomes their mean nibble by
  nibble, ⌊(a & m + b & m) / 2⌋ & m for the four nibble masks, written into both edge lines and the line next to each
  (0x800607..0x8006A2, 0x80079B..0x800836, 0x80093F..0x8009EA, 0x800AFA..0x800BA5). A neighbour on another page has
  its page locked for this (its place halved when the tile is 32 wide; if the lock fails the block's own page is
  written at the neighbour's place); its page is not marked out of date. Last the block's page is unlocked and marked
  out of date (+0x138 = 1 when its type is 8 or 4, 0x800BEF). Run natively through the oracle (2026-10-22: one block
  alone, four neighbours on its page, neighbours on another page and one not rewritten, a block at the map's edge,
  the low-resolution option), every page word as described.

#### The block's highest point

Block +0x924, worked out as the land opens by fn_007FEA60 (from the loader fn_008015D0, 0x801735, and
`LH3DIsland::Create`, 0x804553; the loader first clears +0x91C..+0x940, so the file's value is not kept): the largest
altitude byte of the block's 289 cells (0x7FEA82..0x7FEA93). Nothing works it out again after a land change (the
altitude writer fn_00800DA0 does not touch it). The five story lands store the same value (all 668 blocks). Read by
fn_00877210 for the block's box (its height, highest × [0xC3720C] × 0.5 without the land's
reflection, 0x87722C..0x877292) and so for its cull and its distance +0x9BC, which orders the visible list and the
land's draw ([stage A](../rendering/visible-blocks-and-lod.md#stage-a-the-visible-blocks)). The texture queue does
not read it: its centre is at height 0.

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
- **Where each class takes its drawn height: the piles**
  - A wood or food pile (an entity with `Pot` and `PileSink`) whose sink offset is at or below minus its height is
    left out of the frame's draw, as the original's pile draws skip it: `draw_gate::PileBuried` and `FillFrame`
    (`src/ECS/DrawList/DrawGate.cpp`, reason `ObjectListHidden::Buried`), test
    `DrawGate.AWoodOrFoodPileIsDrawnOnlyWhileSomeOfItIsAboveTheLand` (`test/test_draw_list_gate.cpp`). The height is
    the one `PotArchetype::SetSize` last read (a pile's mesh and scale do not change in between). A field's sink has
    no `Pot` and keeps its own draw. The Land Island > Object draw list readout counts and lists the buried piles.

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

### The block texture queue in openblack

- `block_texture::TextureQueue` (`src/3D/BlockTextureQueue.{h,cpp}`) is the queue of
  [The block texture queue](#the-block-texture-queue): a new request in front and an asked one back to strip 0 in its
  place, the distance with the original's float steps, the stable sort, then the frame's work (the nearest worked
  twice, six worked, all the strips at once at the detail index 4, the prints step after the 16th strip, the one
  finish that ends the frame). Test `test/test_block_texture_queue.cpp`, pinned to the original's queue run natively.
- `LandIsland` keeps each block's marks (`block_texture::BlockMarks`: out of date, drawn since the land opened).
  `SetCellAltitude` marks every block that keeps the corner out of date, as the altitude writer does. After the frame's object draw list (`UpdateObjectDrawList` in `src/Game.cpp`, in the
  same frames the land is drawn), `UpdateBlockTextures` works the queue from the drawn eye at the detail index, then
  `RequestBlockTextures` asks for the out-of-date blocks among the visible ones, nearest first. Each strip is painted
  by `block_texture::BuildBlock` over the cells (strip, 0..15) into the request's own texels, and the finish replaces
  the block's rectangle of the island texture.
- Once the queue is done the picture is the same as painting the changed blocks at once. Before that:
  - At a land's opening openblack still paints every block at once (`CreateBlockTexture`); the original paints each
    one through the queue after its first draw, the nearest first, one a frame at the detail index 4, and shows the
    block's low-resolution tile until then. openblack keeps and draws the low-resolution tiles (below), but only by
    the texture detail line, never for a block waiting for its first paint. So a block
    changed before its first draw (the temple's flattening as the land opens) is painted again at once in openblack,
    as the original's first paint of it already holds the change.
  - A changed block that has been drawn (the vortex, the temple built in play) keeps its old texture until its
    request finishes, as in the original: the next frame at the detail index 4 when it is the nearest waiting, one
    more frame for each nearer block waiting. Before the queue openblack showed the new texels in the frame of the
    change.
  - The queue runs where the land is drawn, in both: the original's temple branch of `GGame::Process3dEngine`
    (0x54DE07..0x54DE55: the sky, `Temple::Draw`, `Temple::Update`) runs neither the land's PreDraw nor
    `TextureUpdateThread` (called only at 0x54DEB2, in the land branch), and openblack's temple view does not run the
    draw list nor the queue. Nothing shows the land there, so nothing waits on it.
  - As in the original, a visible block wholly beyond the texture detail line is not asked for: the draw list's
    `TextureRequestBlocks` (the visible blocks that kept a land triangle and whose level is not "its tile only") is
    what the frame asks the queue for.
  - The prints step bakes the prints into the block's own texels, as the original (#222): at a request's prints step,
    at a block's paint at once (`PaintBlockTextureNow`), at the land's opening (each block in order) and at a block's
    first draw (painted again at once, its prints in), `LandIsland::BakeBlock` takes the block's texels as the game
    keeps them (ARGB 4444, a row along x), bakes them (`block_texture::BakePrints` from
    `ecs::land_prints::SourcesFor`) and puts them back before the upload and the tile's rewrite, so the tile carries
    them too. The sources: the objects over the block as the game's map walk reaches them (`ObjectsOverBlock`: the
    cells from 4 before the block to 4 past it on each side, x outer and z inner; the fixed list, cut at an unavailable
    object, then the mobile list; what the map's readers skip, held or flying, left out; each object once), each with
    its 3D object's matrix (`DrawnBodyModel` of the game's pose), its static shadow (the classes the original sets it
    for, not for a building not built yet: every sub-mesh but the physics ones and those with a status, through the
    sun; a tree's and a big forest's from its first level of detail's texture alpha) and its mesh's first footprint
    (printed when it casts a shadow or has the footprint bit, `land_print_marks::HasFootprintBit`: a field, an animated
    static, a worship site, the citadel's heart, a building made not built, as
    [Who asks for its footprint](#who-asks-for-its-footprint)); then the land's own prints, the newest first: the
    prints of the things that show only as prints (below), then the river beds and their channels (each segment's
    channel made before its bed), the channels lowering the block's alpha.
    `fs_terrain` reads none of the old live targets. So a building removed from a block keeps its print until the queue
    paints that block again, and a MobileObject's static shadow stays where it was baked, as in the original. The snow
    lying on the land is baked last, before the shadows' darkening, whitening each texel by the depth under it and the
    snow's noise image (`snowmap.raw`), and the blocks the melt lists are painted again. Debug: the
    Block textures node's "Show bake layers" leaves the static shadows, the footprints or the river beds and ground
    prints out of the next paints ("Paint every block again" shows them on every block), and counts the ground prints.
  - The holder of a singing stone (MOBILE_STATIC row 6) shows only as a print of its model on the land, as the
    original's: `GBaseOnly::CallVirtualFunctionsForCreation` 0x6093A0 puts no 3D object in the map's cells (it skips
    `Object`'s creation, so nothing draws it and the prints step's map walk never reaches it, no static shadow); unless
    the object is unavailable (+0xA bit 0), it places the shared 3D object [0xD3EE58] at (Pos.x × 10 / 65536,
    `GetAltitude` + Pos.altitude, Pos.z × 10 / 65536) with no turn and no scale (`Translation`, +0x44 = 1, +0x48 = 0;
    the mesh its info's, vt+0x2C, 0 out of the pack's range) and adds its footprint to the land's list
    (`LH3DJustFootPrint::Create` 0x81E9E0, bw1-decomp LH3DDivers.cpp:524): none when a print of the same mesh with the
    same 12-float matrix is already in the list (that object then keeps none, also once the other goes), otherwise a
    record in front of the list ([0xEB9968], so painted before the older rivers), mode 0 (colour) at full strength,
    its box the footprint's triangle corners through the matrix, and `SetNeedUpdate` on each block from the box's
    sides × 1/160 truncated. `ToBeDeleted` 0x6095C0 and `GGame::ClearMap` (`ReleaseAll` 0x6094E0) take the record out
    (fn_006094A0, `LH3DJustFootPrint::Release`) without marking any block: the print stays until the block is painted
    again for another reason. openblack: `MobileStaticArchetype::Create` calls `land_prints::MakeGroundPrint` for row 6
    (`components::GroundPrint` with its placement, its order and whether the land took it, and `NotDrawn`; the blocks
    of its box marked through `MarkBlocksChanged`); `SourcesFor` leaves such a thing out of the objects
    (`CastsShadow`) and puts its print in the land's prints, the newest first, before the rivers; the rules
    (`land_footprints::GroundPrintPlacement`, `IsNewGroundPrint`, `GroundPrintOrder`, `GroundPrintBox`,
    `GroundPrintBlocks`, `src/ECS/LandFootprintRules.h`, after raffclar's #862) are tested in
    `test/test_land_footprint_rules.cpp`. As the original's, the holder never enters the map's cells
    (`map_cells::KindOf` answers none for a `GroundPrint`, so neither the creation's hook nor `Sync` files it, and no
    search of the map, collision or footpath meets it) and CREATE keeps it at the height the script gave
    (`CreateScriptObject`). Its model, `MeshId::SingingStoneCenter` (409) in `AllMeshes.g3d` (read 2026-10-30 with
    the loader's layout), has no sub-meshes and the NoDraw bit (0x2000): it is only its footprint, one entry of two
    triangles over 11.2 x 11.2 m (corners ±5.61) with a 64 x 64 picture, 2945 of its texels with some alpha.
  - The prints step's own arithmetic, the original's, for the bake into the block's texture (#222; run by `BakePrints`, below):
    `block_texture::PrintFootprint` (`src/3D/BlockPrints.h`; tests `BlockPrints.*`, pinned to the original run
    natively) prints a footprint's picture as the game does there: each triangle on its own, its edges walked texel
    by texel (rounding to the nearest), each texel row filled between the nearest and the farthest z an edge
    reached, the picture taken at its nearest texel; colour: each nibble (s (255 − a) + f a) / 255 rounded down,
    a = 16 × the picture's alpha nibble, the land's alpha kept; the river beds: the alpha only, the lower of the
    land's and the picture's (raised first by the strength's high nibble).
    `ShadowTable` and `ApplyShadow`: the darkening by coverage, 255 × the samples covered / 8 (halved with the
    low-resolution textures; the table read back from the original), each nibble × (255 − t / 2) / 256; and the
    static shadows' raster in `src/3D/BlockShadows.h`: `ProjectShadow` (the mesh along the sun from (−500000,
    500000, −500000) onto y = 0), `ShadowMaskPoint` and `DrawShadowTriangles` (8 samples a texel, 4 across z and
    2 across x; tests `BlockShadows.*`, the original's masks byte for byte).
    `PrintSnow`: the snow lying on the block, cell by cell from its corners' depths in 16.16, against the noise,
    with the blend table the snow cover builds, T[w][c] = (w c) >> 4.
    `DrawTextureShadow`: the shadow of a tree or a big forest (the objects whose chroma bit, vt+0x94, is set), its
    texture's 64 × 64 alpha ORed into the mask by its own rasteriser (`LH3DStaticObject::DrawTextureShadow` 0x80ECB0;
    the original masks byte for byte); `ProjectSkinnedShadow` a skinned part's. `BakePrints` (`src/3D/BlockBake.h`;
    tests `BlockBake.*` with fake objects) runs the step in the original's order: each object once, its shadow (the
    mask cleared at the first) and then its mesh's footprint (printed when it casts or prints), the land's footprints
    whose box meets the block, the snow, and last the darkening. The footprints' pictures and triangles stay on the CPU
    (`L3DMesh::Footprint`).
- The blocks an object marks ([The blocks an object marks](#the-blocks-an-object-marks)) are
  `block_texture::BlocksMarkedBy` (`src/3D/BlockTextureQueue.cpp`; test `BlockTextureMarkedBlocks.*` in
  `test/test_block_texture_queue.cpp`, pinned to the original run natively), with the object's place, its Y angle, its
  scale, its mesh's box and the corners of its mesh's first footprint (`L3DMesh::Footprint::corners`), from
  `ecs::land_print_marks::MarkBlocksUnder` (`src/ECS/LandPrintMarks.cpp`); `LandIsland::MarkBlocksChanged` marks the
  blocks that exist out of date. Called where the original calls `RequestChangeTexture`: `map_cells::InsertMapObject`
  for the Fixed classes (a MultiMapFixed once for all its cells; `map_cells::OnAnglesOrScaleChanged` goes through it),
  `map_cells::RemoveMapObject` for a MultiMapFixed, `abodes::Built` (a field's and any other abode's) and a tree's
  growth (`ecs::GrowTree`, which changes its scale without taking it out of the cells). Not audited: other code of
  openblack that turns or scales a Fixed object in the map without `OnAnglesOrScaleChanged`. What the marks read of the
  object is `land_print_marks::TextureBitsOf`, as [Who asks for its footprint](#who-asks-for-its-footprint): the shadow
  for the renderer's static shadow classes (`land_print_marks::IsShadowClass`) when not a building not built, and for
  one being made not built until its first insert is done; the footprint bit (`land_print_marks::HasFootprintBit`, the
  same the prints step asks), and the footprint's corners only with it. A building made not built keeps the bit
  (`Abode::footprintOnTexture`, set by `AbodeArchetype::Create` right after its `InsertMapObject`, also for a saved
  game's plan). Tests `LandPrintMarks.*` (`test/test_land_print_marks.cpp`), on a registry of their own, pinned to the
  classes and the order read from the exe. The first-draw gate [0xE9CD78] is not kept: before a block is first drawn
  its marks change nothing that shows.
- A block's highest point is worked out from its cells as the land opens (`LandBlock::HighestAltitude` from
  `LandIsland::Build`; test `test/test_land_block_highest.cpp`), with the whole altitude on maps with more altitude
  bits, and kept through land changes.
- Debug: the Land Island window's "Block textures" node (closed): the requests waiting, and "Paint every block
  again", which marks every block out of date as the original's request to update all the land's textures does.
- The low-resolution pages ([The low-resolution pages](#the-low-resolution-pages)): `LNDFile` reads each record's
  tile count, number, surface description and DXT3 texels (`LNDLowResolutionTexture::Width`, `Height`, `FourCC`,
  `CompressedTexels`) and each block's page and tile origin, the texel origin as ints (`LNDBlock::iuLrs`, `ivLrs`).
  `low_res::DecodePage` (`src/3D/LowResTile.h`) decodes a page to ARGB 4444 by the measured rule; test
  `test/test_low_res_tile.cpp`, pinned to the measured words (synthetic blocks, and Land1's 7 pages by their hash).
  `low_res::UpdateTile` rewrites a block's tile on its page from its finished texture, with the neighbours' borders,
  as the original's own rewrite does (test pinned to it, run natively on the same pages and textures). The pages are
  kept at the land's opening (`LandIsland::PlaceTiles`, 256 × 256 pages only), every block's paint rewrites its tile
  (`RewriteTile`, at the open paint in block order and at each queue request's end) and the changed pages go to one
  texture array, a layer a page (`SendLowResPages`).
- The texture detail line (`src/ECS/DrawList/TextureDetail.h`, test `test/test_texture_detail.cpp` pinned to the
  original run natively): `TextureLineAtLandCreation` sets it at the land's creation (at detail index 4, 800 ahead with
  a band of 170 from the texture pool's count, `k_TexturePoolCount`, 2064 on the test machine), `RebuildTextureLine`
  rebuilds it every frame in the plane at min(eye y, 110.55) (`ObjectDrawListSystem::TextureLine`), and
  `BlockTextureLevel` gives each block its level from its four corners. The land's draw (`Renderer`, terrain loop;
  `vs_terrain` / `fs_terrain`) draws a level 0 block as before, a level 1 block as its tile only (its alpha the tile's,
  no small bump) and a level 2 block with its tile over its texture by the tile's alpha times the vertices' alpha
  (`TileAlpha`, worked per vertex in `vs_terrain` with every step a float, ties to even), between the land and the
  small bump passes, each pass with its specular once, as the original's three blended draws (until 2026-10-29 the level 2 blend took the land already blended with its small bump and specular, so across the band both were counted twice: a pale, see-through strip with the blocks' straight edges); the tile UV is the block's times 62 / 256 from the tile's origin plus 1/255. openblack's first
  paint is all at once, so no block waits for its first texture and none is drawn as its tile for that. Debug, in
  the Block textures node: "Texture detail line" tints each block by its level (red its tile only, blue both) and
  "Low-resolution tiles only" draws every block as its tile; both off.

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/Implementations/LandIsland.cpp`: `CreateSmallBumpTexture`, `LandIsland::GetCellCorners`, `LandIslandInterface::RayCastCells` (a thin wrapper over the land line).
- `src/3D/LandLine.h`: `HitInCell`, `FirstHit`, `LandAlong`, `LandOrSeaAlong`, `UnderPixel` (the land line, under raffclar's names).
- `src/3D/LandBlock.h`: `HighestAltitude` (a block's highest point).
- `src/3D/LowResTile.h`: `DecodePage`, `UpdateTile` (the low-resolution pages).
- `src/ECS/DrawList/LandClip.h`: `BlockTriangles` (a block's triangles at its level of detail, a pure function the land clip
  reads; test `test/test_land_clip.cpp`, [object-draw-list.md](../rendering/object-draw-list.md)); the land's GPU draw does
  not take it yet.
- `src/3D/BlockTextureQueue.h`: `TextureQueue`, `RequestDistance` (the block texture queue).
- `src/3D/LandIslandInterface.h`: `GetCellCorners`, `RequestBlockTextures`, `UpdateBlockTextures`, `RepaintAllBlockTextures`.
- `src/3D/LandLightTable.h`: `GetMoonColour`.
- `src/3D/LandNormal.cpp`: `EdgeFormula`, `LengthTable`.
- `src/3D/LandNormal.h`: `OfCell`.
- `test/test_land_light.cpp`: `OriginalHazeDistances` (the haze distances with the exe's floats).
- `test/test_land_raycast.cpp`: since the walk moves the end to the map's edge, a ray to a point one metre ahead
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
- The block texture queue's prints step (issues #71, #184 and #222): the footprints, the static shadows and the river
  channels are baked into the block's texels at it ([The block texture queue in openblack](#the-block-texture-queue-in-openblack)).
  Still open:
  the live callers of `LandBlock::SetNeedUpdate` through fn_0081E9E0 (`DesignedWaterFall`,
  the creature sub-action sub_5062C0) are not ported (the `GBaseOnly` creation's is: the singing stone holders' prints;
  the rivers' call is at the map's load,
  before any land draw, and the four dead ones need nothing). These are block marks, not prints: everything the prints
  step draws, and the darkening after it, is the original's.
- The low-resolution pages: the file's pages are read and decoded (`low_res::DecodePage`) and a tile's rewrite
  (fn_008002C0) is a pure function (`low_res::UpdateTile`), both in `src/3D/LowResTile.h` with
  `test/test_low_res_tile.cpp`, kept, rewritten and drawn by the texture detail line (#71 B1), the prints and the snow
  baked into the tile as into the texture (#222). The texture pool's
  count is a constant (`k_TexturePoolCount`), as openblack has no pool to count.
- The land's first paint: openblack paints every block at once as the land opens (`LandIsland::CreateBlockTexture`,
  `src/3D/Implementations/LandIsland.cpp`); the original paints each through the queue after its first draw and shows
  its low-resolution tile until then
  ([The block texture queue in openblack](#the-block-texture-queue-in-openblack)).
