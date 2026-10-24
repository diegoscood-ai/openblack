# Visible blocks and level of detail

## Summary

Each frame the original culls the land's 32×32 blocks against the view, sorts the visible ones front to back and gives
each a level of detail from four lines across the land; that list of blocks drives both the land pass and the object
draw list. The detail index (0..4, from the registry, 4 when missing) is read once at start-up and decides the fog,
the reflected land and the level-of-detail lines. In the v1.42 exe the per-model level of detail is switched off: every
boned model draws its high mesh at any distance.

openblack runs the same cull, sort, level of detail and land-load reset on the logic side for the object draw list,
with `--detail-level` (default 4). By the user's choice (2026-10-15) the high meshes of villagers, animals and birds
are drawn only at the top detail level, 6.

## The original

### Stage A: the visible blocks

```
[0xEA9EA0] = &g_world_to_clipping (0xEA9E40)
LH3DIsland::PreDraw()                 // once per frame: does nothing if [0xE9CD74] != 0, sets it at 0x7FF4E2;
                                      // fn_007FF610 clears it at 0x7FF7AD
  for n in 1 .. [0xE9CD70]-1:         // g_ptr_blocks[0xE9C564 + 4n]; slot 0 is never used
    fn_00877210(block n)              // called at 0x7FF32A
      box: x = +0x90C+80, z = +0x910+80, half extents 80; H = int(+0x924) * [0xC3720C] (0.67)
           LandRef [0xE9CD8C] != 0: y from -H to +H;  otherwise y from 0 to H
      for each of the 8 corners: (cx, cy, d) = corner through g_world_to_clipping (rows 0xEA9E40/4C/58, t 0xEA9E64)
           near  : !(d >= [0xE839E0])                 // fcom/test ah,1: a NaN depth sets near (0x8773E4..0x8773F1)
           right : cx > d,  else left  : -d > cx
           top   : cy > d,  else bottom: -d > cy       // no far plane
      if any of the 5 masks == 0xFF: +0x920 &= ~5; +0x930 = 0; +0x934 = 4; return     // 0x877CDD, old +0x9BC kept
      +0x920 |= 5;  +0x91C = (all masks 0) ? 0 : 1
      +0x9BC = sqrt((oz*oz + oy*oy) + ox*ox), o = centre - cam  // 0x877C8A..0x877CCD, not squared
               centre (x+80, LandRef ? 0 : H/2, z+80), cam = g_camera 0xEA1DB8
  [0xFA92D8] = 0
  for n in 1 .. count-1 with +0x920 & 1: insertion sort ascending by +0x9BC (next = +0x9B8)
    // walk while cur.+0x9BC < new.+0x9BC or the compare is unordered (test ah,1), insert before the first cur
    // that is >=, so on equal distances the later block goes before the earlier one, a NaN new distance walks
    // to the end, and a NaN already in the list is walked past     (0x7FF45F..0x7FF4DD)
[0xC37208] = 0                                        // 0x7FF62B
for each block b in [0xFA92D8]:                       // 0x7FF639..0x7FF791
  if b.+0x934 in {1, 3}: copy b to 0xE9B6E0; fn_00877D20(copy, +0x934 == 1); t = copy   else t = b
  bit0 = fn_00874AA0(t) != 0
  b.+0x920 bit0 = bit0 (the copy too)
  if bit0 != b.+0x920 bit1: b.+0x920 bit1 = bit0; [0xC37208] = 1          // 0x7FF759..0x7FF783
```

fn_00874AA0 returns 1 exactly when the block keeps at least one front-facing land triangle after clipping. It clears
the list size at `0x875314`, sets the result to 0 at `0x875350` and to 1 at `0x87539B` only for a non-empty list, and
returns it at `0x875C49`.

- The vertices are a grid with a step of `1 << +0x930` cells (`0x874AC9`), `16/step + 1` per side (17, 9 or 5),
  stored compactly (index `row*n + col`) with the altitude of record `(R*step, C*step)`; `g_NumClipVerts` is
  `(16/step + 1)^2` (`0x875326`). The level of detail is [below](#the-blocks-level-of-detail):
  - height `y = alt > 3 ? alt * [0xC3720C] : 0` (the compares with 3 are at `0x874B98` and `0x875065`);
  - screen `sx = (x/d + 1) * [0xE839F0]`, clamped to `[0, [0xC2AB00]]`, and `sy = [0xE839F4] - (y/d) * [0xE839F4]`,
    clamped to `[0, [0xC2AB04]]`. `[0xE839F0]` and `[0xE839F4]` are half the viewport's width and height:
    UpdateViewPort `0x819030` multiplies the width `0xE839E4` and the height `0xE839E8` by 0.5 and stores them at
    `0x81905E` and `0x819070`.
- Outcodes are computed only when `+0x91C != 0`, that is, when the block is partly outside (`0x874E04..0x874E72`): 0x20
  near, 0x10 or 8 for x outside ±d, 4 or 2 for y outside ±d.
- A triangle counts when all of these hold:
  - its cell is not skipped: a cell with cell word `+6 & 0x200` is skipped, but only when `+0x934 == 0` (`0x876A8A`,
    `0x875DD8`); the path at `0x876CEF` has no such test. The cell read is the one the walk's pointer reaches, which
    does not follow the level of detail ([the cell-flag pointer](#the-blocks-level-of-detail));
  - the diagonal comes from cell word `+6 & 0x80`, and the seam fans (`+0x93C`, `+0x930`) are included;
  - either its three outcodes are 0, or `(c0 & c1 & c2) == 0` and a piece survives fn_0081A760's clip against the near,
    left, right, top and bottom planes. Clipped vertices come from fn_0081DD90 and are projected and clamped the same
    way (`0x81E0EB..0x81E158`);
  - `A = (P1.x-P0.x)*(P2.y-P0.y) - (P1.y-P0.y)*(P2.x-P0.x) > 0`, strictly (0 and NaN fail), on the clamped screen points
    in emit order (`0x81A99C..0x81AA7C`). g_NoBackfaceCull is forced to 0 (`0x876A3D`, `0x875D8E`), and `[0xFA92DC]`
    is 0 in this pass.
- The frustum-culled blocks are not in the list. fn_00877210 clears their bits 0 and 2 but keeps bit 1, so they never
  set `[0xC37208]`.
- LH3DIsland::Release also sets `[0xC37208] = 1` (`0x804816`).
- `[0xEA9EB4]` picks the code path:
  - its one writer is fn_007AC9E0 (`0x7AC9ED`), called once from LH3DRender::Open `0x82B52C`; it copies IsPentium4;
  - IsPentium4 is set when the CPUID base family is 0xF with SSE2 and OS FXSR (`0x8A2610`, `0x7ACA00`), with no vendor
    test;
  - set (Pentium 4, AMD K8 to Zen), the SSE path fn_007A1800 runs, and CheckCPU also turns on flush-to-zero
    (`0x7ACA83..0x7ACA95`). It builds the same tests (`0x7A2D02..0x7A2D23`, then fn_007A98B0, fn_007AA8F0 and
    fn_007A3A50, `0x7AAB14..0x7AABAD`) and returns 0 for an empty list (`0x7A2D28`), 1 otherwise (`0x7A2D45`);
  - not set (Intel family 6, so every Core and later), the x87 path runs.
- `[0xC3720C]` is 0.67 in every read that matters here. Its only writers are two `fstp` in the reflected land pass
  fn_007FF4F0 (called from GLandscape::Draw `0x5E48DD`, active only with LandRef), which negates it at `0x7FF52F`,
  after its own PreDraw (`0x7FF510`), and restores it exactly at `0x7FF5F3`. That pass calls fn_00874AA0 for every
  listed block and discards the result: it writes no `+0x920` bit and no `[0xC37208]`. With LandRef on, the frame's
  PreDraw therefore runs inside that pass and the main pass's PreDraw does nothing; the camera is the same for both, so
  the block list is the same.

#### The block's level of detail

What follows is the x87 path. The SSE path (fn_007A1800, fn_007ACCE0, fn_007ACD00; it reads the seam table at
`0x7AA499` and `0x7AB12C`) was not read for it.

**The lines.** Four lines across the land, structs of 0x28 bytes: `+0x00` n, a horizontal unit direction along the
line (n.y is 0); `+0x0C` p, a point on it; `+0x18` valid (nothing in the cull or the morph reads it); `+0x1C` dist;
`+0x20` W, the band's full width (the morph's ramp); `+0x24` T, its half width (the cull's test; T = W/2 in every set).

| Line | Built by | Static init (pointer table `0x9C7D24..0x9C7D38`) | LH3DIsland::Create, detail index 4 only |
|---|---|---|---|
| L0 `0xE9C4B8` | fn_007FF0E0 (y_ref 0) | 600, W 50, T 25 (fn_007FE930) | 2500, W 550, T 275 (`0x803E25..0x803E7B`) |
| L1 `0xE9C4E0` | fn_007FF0E0 (y_ref 0) | 300, W 50, T 25 (fn_007FE8B0) | 1600, W 550, T 275 (`0x803DC5..0x803E1B`) |
| L2 `0xE9C508` | fn_007FEE60 (y_ref = min(cam.y, 110.55)) | 300, W 40, T 20 (fn_007FE830) | 400/70/35 (`0x803C2B`), then by the live LH3DVRAMTex count `[0xF05200]`: > 600 gives 800/170/85, > 500 700/170/85, > 400 600/170/85 |
| L3 `0xE9C530` | fn_007FEE60 (y_ref as L2) | 50, W 40, T 20 (fn_007FE7B0) | never changed |

- Create's two tests are `[0xC381EC] == 4` (`0x803C25`) and fn_00804E60, which in the running copy also returns
  `[0xC381EC] == 4` (`0x804E69`). Nothing else writes dist, W or T. `[0xF05200]` counts live LH3DVRAMTex textures (+1
  at `0x85DF00`, −1 at `0x85DB41`, reset at `0x85D825`); it only affects L2. The pool is filled at the device's
  start up to `MaxVRAM` (registry, 256 MB when absent, as on the test machine): about 2064, so 800/170/85 there
  ([terrain.md](../world/terrain.md#what-a-block-shows-meanwhile-and-the-far-blocks)).
- Once per frame, in LH3DIsland::PreDraw (`0x7FF2E1..0x7FF30E`, before any cull), each line is rebuilt from the camera:

```
f   = [0xEA1DD4..0xEA1DDC]      // unit view direction, written by UpdateWorldToCamera 0x819773..0x819782 when its
                                // bool is set (UpdateCamera passes 1 at 0x819A41, CameraModeNew3 passes 0 at 0x459A66);
                                // f = normalize(focus - eye), with dx replaced by +-1e-4 (its sign) first when
                                // |dx| < 1e-4 and |dz| < 1e-4 (0x8196B9..0x8196F8)
P   = g_camera + dist * f       // per component
d   = -(P.z*f.z + P.y*f.y + P.x*f.x)
n   = (f.z, 0, -f.x)            // cross((0,1,0), f), fn_006A3E20
m   = -y_ref                    // y_ref = 0 (fn_007FF0E0); (110.55 < cam.y || NaN) ? 110.55 : cam.y (fn_007FEE60,
                                // 110.55 = 0.67f * 165f, 0x7FEE63..0x7FEE90)
sx = n.x^2, sy = 0, sz = n.z^2
if (sz > sy && sz > sx && sz > 0.0001)  p = (d - m*f.y, m*f.x, 0) * (1/n.z)     // strict compares
elif (sx > 0.0001)                      p = (0, -m*f.z, m*f.y - d) * (1/n.x)    // (the sy branch is unreachable)
else { valid = 0; return }              // n stays (f.z, 0, -f.x) unnormalised, p keeps last frame's value
// the scale by 1/n.z or 1/n.x is done once, by fn_007FF0C0
n *= 1/sqrt(sz + sy + sx); valid = 1
```

  The cull and the morph then use, for a point (x, z), `v = (x - p.x) * n.z - (z - p.z) * n.x` in exactly this order:
  the signed horizontal distance to the line, positive on the camera's side.

**The choice** (fn_00877210 after the frustum masks, `0x87751B..0x877C84`). The block's 4 ground corners (x, z),
(x, z+160), (x+160, z), (x+160, z+160), with x = `+0x90C` and z = `+0x910` (the 160 added before p is subtracted), are
each *near* (v > T), *far* (v < −T, and a NaN v) or *in the band*:

```
L1:  any band            -> +0x934 = 1, +0x930 = 0     // 0x877A6A
     any near and any far -> +0x934 = 1, +0x930 = 0     // 0x877A84
     all near             -> +0x934 = 0, +0x930 = 0     // 0x877A92
     all far              -> +0x934 = 2, +0x930 = 1     // 0x877AA0
if +0x934 == 2, L0:
     any band             -> +0x934 = 3, +0x930 = 1     // 0x877C5A
     all far              -> +0x934 = 4, +0x930 = 2     // 0x877C7A
     all near             -> +0x934 = 2, +0x930 = 1     // 0x877C6C
     near and far         -> +0x934 = 3, +0x930 = 1     // 0x877C5A
```

- `+0x934`: 0 full detail; 1 full detail morphing towards step 2 (straddles L1); 2 step 2; 3 step 2 morphing towards
  step 4 (straddles L0); 4 step 4, also the value a frustum-culled block is given. `+0x930` is the matching step shift.
- L2 and L3 set only `+0x938` and `+0x928`, and the fog band sets `+0x940`. With the detail and bump lines they change
  colours, UVs and passes only: fn_00874AA0's result does not depend on them (it is fixed at `0x87539B`).

**The seams** (fn_00878210, from PreDraw `0x7FF33C..0x7FF359`). It runs for every block after all of the frame's
culls, so it sees this frame's `+0x930` of every neighbour; culled blocks have `+0x930 = 0` and never cause a seam.

```
+0x93C = 0;  nb(i, j) = block g_index_block[i*32 + j] if 0 <= i, j <= 31 and that index is not 0
nb(bx-1, bz).+0x930 > +0x930: |= 1      // bx = +0x914, bz = +0x918, signed compares
nb(bx, bz-1).+0x930 > +0x930: |= 2
nb(bx, bz+1).+0x930 > +0x930: |= 4
nb(bx+1, bz).+0x930 > +0x930: |= 8
```

- The seam table (`0xE9A130`, 24 bytes per entry: has_fans, skipRow0, skipRowLast, skipCol0, skipColLast) is built
  once by fn_00803890 (`0x803983..0x803A15`, guarded by `[0xC37BF8]`). 1 → {1,1,0,0,0}, 2 → {1,0,0,1,0}, 3 →
  {1,1,0,1,0}, 4 → {1,0,0,0,1}, 5 → {1,1,0,0,1}, 8 → {1,0,1,0,0}, 10 → {1,0,1,1,0}, 12 → {1,0,1,0,1}. Every other
  value (0, 6, 7, 9, 11, 13, 14, 15) is all zero: no edge is skipped and no fan is added, so two opposite coarser
  sides are left as they are.
- The quad walk (fn_00876910 `0x876936..0x876A26`, fn_00875C60 `0x875C86..0x875D77`): at steps 0 and 1 the rows run
  from skipRow0 to `16/step - skipRowLast` and the columns likewise; at step 2 all 5×5 vertices, with no seam handling
  (`0x876964`). The first quad is `v00 = (skipRow0 ? n : 0) + (skipCol0 ? 1 : 0)`, and at a row end each index moves by
  2 if a column is skipped, otherwise 1. A quad with the cell's split bit 0x80 emits (v01, v11, v10) and (v01, v10,
  v00), otherwise (v00, v01, v11) and (v00, v11, v10).
- The fans (`0x876F26..0x877200`, outside `0x876562..0x8768FA`) run only when has_fans is set, at steps 0 and 1. Bit 1
  is tested before bit 8 and, within either, bit 4 before bit 2. Step 0 tables: 1+4 `0xC37584`, 1+2 `0xC37470`, 1
  `0xC37230`, 8+4 `0xC377AC`, 8+2 `0xC37698`, 8 `0xC372C0`, 4 `0xC373E0`, 2 `0xC37350` (pairs 46 triangles, single
  sides 24). Step 1, in the same order: `0xC37A64`, `0xC379E0`, `0xC378C0`, `0xC37B6C`, `0xC37AE8`, `0xC37908`,
  `0xC37998`, `0xC37950` (22 and 12). The step 1 tables are stored with 17-wide indices and converted once, in place,
  to 9-wide ones, `v = (9*(v/17) + v%17) / 2` (`0x803A1D..0x803BCC`). Each triangle is emitted in table order if
  `A > 0` (`0x877159..0x8771E8`), or clipped first on the outside path.

**The cell-flag pointer.** The walk's cell pointer does not follow the level of detail or the seams: it starts at
record 0 and moves one record per quad and one more per row (`0x876C8E`/`0x876CB1`, `0x876EEC`/`0x876F0F`,
`0x876141`/`0x87616F`). At step 0 with no seam, quad (r, c) reads record (r, c). At step 1 it reads record `9r + c` of
the 17-wide array, at step 2 record `5r + c`; with a skipped first row the flags are one row behind, and with a skipped
column they drift by one record per row. The flags read this way are the split bit 0x80 and, at `+0x934 == 0` only,
the open-sea bit 0x200.

**The open-sea bit.** Cell word `+6` is `properties | flags << 8`, so 0x200 is bit 1 of the LND flags byte, the open-sea
cell of [coast](../world/sea-coast-and-rivers.md#coast). A quad is skipped for it only when `+0x934 == 0`, and only for the record the
pointer reaches.

**The morph** (fn_00877D20, fastcall: ecx = the copy at `0xE9B6E0`, edx = `+0x934 == 1`). Stage A copies the whole
block first (`rep movsd` of 0x276 dwords, `0x7FF64B`). Only the copy's altitude bytes change: record i at
`block + 8*i`, altitude at +4, i = row*17 + col, the row along x (`+0x90C`) and the column along z (`+0x910`), 10 units
per cell.

```
edx ? (L = L1, s = 2, h = 1) : (L = L0, s = 4, h = 2)          // 0x877D2B..0x877DD0
K = 256 / L.W;  half = L.W * 0.5
blend(r, c, a, b):
    v = (X(r) - L.p.x) * L.n.z - (Z(c) - L.p.z) * L.n.x       // X(r) = +0x90C + 10r, Z(c) = +0x910 + 10c
    if (v >= half) return                                      // on the near side of the band; ordered, NaN goes on
    mid = (alt[a] + alt[b]) >> 1
    if (-half >= v) alt[r,c] = mid                             // beyond the band; ordered, NaN goes on
    else { t = fistp((half - v) * K); alt[r,c] = ((256 - t)*alt[r,c] + t*mid) >> 8 }   // unsigned, byte store
rows 0, s, .., 16;  cols h, h+s, ..:   blend(r, c, (r, c-h), (r, c+h))          // 0x877DD3..0x877F42
rows h, h+s, ..;    cols h, h+s, ..:   blend(r, c, (r-h, c-h), (r+h, c+h))      // 0x877F48..0x8780AF
rows h, h+s, ..;    cols 0, s, .., 16: blend(r, c, (r-h, c), (r+h, c))          // 0x8780B5..0x878203
```

- X and Z are float accumulators: they start from `+0x90C - p.x` (or with 10h added first) and add `10*s` per step, in
  that order.
- a and b always lie on the coarser grid, which no pass writes, so the passes are independent. The centre vertex always
  uses the (−,−)/(+,+) diagonal, whatever the split bit. A NaN v takes the partial branch with fistp's
  integer-indefinite result.

#### The detail index

- The running detail index is `[0xC381EC]` (static 3). Its only writer is fn_00823AD0 at `0x823B32`, called once from
  start_system (`0x643026`), so it is set at start-up and never changes while the game runs.
- start_system (`0x642FCB..0x643004`) reads the DWORD `detailidx` from `HKEY_CURRENT_USER\Software\Lionhead Studios
  Ltd\Black & White\BWSetup` (key `0x9CAFF8`, value `0xBE8D68`, LHLogR RegistryRetrieveULong, import `0x8A9344`; the
  root `[0x1001DEE0]` is HKCU). Missing gives **4**; any value of 5 or more (unsigned, `jae` at `0x642FF2`) also gives 4.
  No ini, profile or command-line option is read.
- fn_00823AD0 then sets, from tables indexed by d (0..4): the fog flag `[0xC37204]` (0x9A3854: 0,0,0,1,1),
  UseSmallBump `[0xC37210]` (1 in all), LandRef `[0xE9CD8C]` (0,0,0,1,1) and `[0xE9CD90]` (0x9A38A8: 0 at every
  index 0..4; it seeds `+0x92C`, texturing only). Index 5, the custom set (fn_008237B0), is never reached from
  start-up.
- At d 0..3 the lines keep their static values (L0/L1 600/300, ±25); at d 4 they are 2500/1600, ±275.
- The Options box (ReadRegistrySettings `0x51482A..0x514875`, saved by `0x5148B0`) writes `detailidx` back to the
  registry but does not call fn_00823AD0: a change applies at the next start. `maxdetailidx` (default 5,
  `[0xC381E8]`) only limits the dialog.
- The test machine's registry has no `detailidx` (nor `maxdetailidx`), so the original runs there with **d = 4**. This
  holds until someone saves the Options box with another level.

#### What the level of detail changes in the list

For one rebuild, the list's membership and order do not depend on the level of detail (`+0x930`, `+0x934`, `+0x93C`,
`+0x940`):

- The block list `[0xFA92D8]` is linked in PreDraw from bit 0 of `+0x920`, which only the cull's frustum test writes,
  every frame, before any level-of-detail code (`0x8774E8`, `0x877CEB`). fn_00878210 writes only `+0x93C`.
- The sort key and the vanish break use `+0x9BC`, made from the box centre only (`0x877C8A..0x877CCD`).
- The land pass and its callees never write `+0x9B8`, `+0x9BC` or `[0xFA92D8]`. A block whose bit 0 the land pass
  clears stays linked.
- Stage B's walk reads per block only `+0x9BC`, `+0x914`, `+0x918` and `+0x9B8`, not `+0x920`. So it walks every
  frustum-accepted block, including those whose land triangles were all clipped or back-facing.

The level of detail reaches the list only through the rebuild trigger `[0xC37208]`: fn_00874AA0's result depends on
`+0x930`, `+0x934` and `+0x93C`, so a block can flip visible or invisible because of its level, and decide which frame
rebuilds. `+0x940` does not reach that result.

#### A land load

Every land load frees the land's blocks and makes them again from the land file, so each block's state starts afresh:

- **The paths.** LOAD_MAP (GScript::LoadMap `0x6FB320`) and the first land (GGame::Init, then
  ResetAndStartPlaygroundGame `0x5559A7`) both reach StartPlaygroundGame `0x552F40`. It runs ClearMap,
  LH3DLandscape::Release and LH3DIsland::Release (`0x552F59`), then LoadMapFeatures `0x7180B0`, whose feature
  dispatcher `0x715180` has the only call to GLandscape::Open (`0x716FE7`; its case number was not resolved). A saved
  game's load (LoadAllGame) runs ClearMap and then GLandscape::Open (`0x558AC9`). The vortex reaches neither: the next
  land comes through the script's LOAD_MAP.
- **GLandscape::Open `0x5E52E0`** has no "same land" test: it always runs both Releases, then LH3DIsland::Create
  (`0x5E533B`), which hands a `.lnd` file to fn_008015D0 (`0x803E98`).
- **LH3DIsland::Release `0x804790`** frees every block (its texture and material, then LH3DMem::Free at `0x878767`),
  zeroes `g_ptr_blocks` and `g_index_block`, sets the block count `[0xE9CD70]` to 0, and sets `[0xC37208] = 1`
  (`0x804816`).
- **The `.lnd` load.** Each block 1..count−1 is allocated again (0x9D8 bytes, zeroed) and copied whole from the file
  (`0x80169B`). Then `+0x91C..+0x943` are zeroed (from `0x8016A4`): the partly-outside flag `+0x91C`, all of `+0x920`
  (bit 1, the last land-clip result, included), `+0x930`, `+0x934`, `+0x93C` and `+0x940`. `+0x9D4` is zeroed too, and
  fn_007FEA60 sets `+0x924` to the block's highest altitude (`0x7FEA93`). The other path (an LH_FILE, not a `.lnd`)
  zeroes the whole block.
- **Not cleared:** `+0x9B8` (next) and `+0x9BC` (the distance) keep the file's bytes. Nothing reads them first: PreDraw
  culls every block (`0x7FF32A`), the cull writes `+0x9BC` for every block it keeps, then PreDraw empties `[0xFA92D8]`
  (`0x7FF468`) and links the visible blocks again, writing `+0x9B8`. Only listed blocks are read after that.
- **What a land load keeps:** the last rebuild's turn `[0xD2019C]` (its only writer is `0x5E5D90`), and `[0xFA92D8]`,
  which points at the freed blocks until the next PreDraw.
- **`[0xC37208]`.** Release's 1 is never seen: fn_007FF610 sets the flag to 0 at its start (`0x7FF62B`), and
  GLandscape::Draw always calls it (`0x5E4E96`) before the rebuild check (`0x5E4EDD`); nothing in Draw jumps over that
  call. The check fn_005E5CD0 only reads the flag (`0x5E5D5E`). Since bit 1 is 0 after a load, the first drawn frame
  sets the flag for every listed block that keeps a triangle; that rebuild is forced anyway by ClearMap's RebuildCount
  of 1.

### The model level of detail in v1.42

- The six-NOP runs after `fmul [0x9A391C]` are in the two detail-level loaders, `fn_008237B0` at 0x823810 and
  `fn_00823AD0` at 0x823B43 (the Mac symbols' `LH3DLevelOfDetail::UpdateFromCustomRegistry` and
  `UpdateFromRegistry`; bw1-decomp has no C for them). Each removed an `fstp dword ptr [0xC3813C]`: with the bytes
  `D9 1D 3C 81 C3 00` poked back the x87 stack is balanced after the call, without them one value is left on it (an
  oracle run of both), and the same function keeps that idiom intact at 0x823C02. **Determined.**
- [0xC3813C] keeps its .data value **100000.0** at every detail level (its only other writers are a debug widget whose
  creator 0x5E76C0 is never called; the oracle run of the loader at levels 0..6 gives 100000 each time; with the
  stores back, 0.2, 0.3, 0.4 then 0.5).
- So the LOD chooser in `AddDrawing` 0x815A70 gives every boned model **LOD 1** at any visible depth (oracle), and the
  boned draw 0x812170 draws slot +0x98, the **high** mesh (`_1`: a villager's highDetail, a child's ChildMeshHigh, an
  animal's high; e.g. NORS_M_A_1 263 vertices / 346 triangles against 112 / 124 for `_2`); the skeleton, bounds and
  picking stay on the std mesh (+0x7C). Static models have one mesh and draw the sub-meshes of LOD bit 1.
- A bird's high mesh is the finer of its two (`_1`; its std and low are both `_2`, the bat's high and std both
  `BAT_1`). `EGPT_M_B_1` and `TIBT_M_B_1` have 22 bones against 21, and their vertices use bones 0 to 20.
- These high meshes are not unused models; the only finer villager models are the intro's `nors_man`, `nors_woman`,
  `nors_boy` (used by SetHighGFXDetail) and `sable.l3d`, and nothing finer exists for animals, the other tribes or the
  creature.
- 📼 1.20 only Whether Lionhead's 1.20 had the stores (its bytes are not on this machine) only matters for Lionhead's game.
- The readme's item 1 (the low-spec detection) more likely is three other NOP runs in `start_system` (0x642FF8,
  0x643009, 0x64302E: a detail index that is missing or 5 and above becomes level 4) **🔍 inferred**.
- The object-side LOD distances and the fade are described under
  [Stage D](object-draw-list.md#stage-d-what-sets-g_b_last_on_screen-inside-draw).

> **Which exe this is.** These bytes come from the community **Infinite Drawing Distance** patch, which the original used here includes: the plain published v1.42 (sha1 `9c9aea96…`) keeps both `fstp`s at 0x823810 and 0x823B43, and holds 700, 5 and 1 at [0xC37200], [0xC37EC8] and [0xC3813C] instead of 100000. The patched exe stays the original (user, 2026-10-17); see [Reading this wiki](../project/reading-the-wiki.md).

## openblack

Status: **done** for the block cull, the level of detail and the land load on the logic side (they feed the
[object draw list](object-draw-list.md#how-openblack-runs-the-list)); the GPU draws the land by its own path.

- **The detail level.** `EngineConfig::detailLevel` (src/EngineConfig.h, default 4), from `--detail-level`
  (src/main.cpp, default 4, 0..6). Its 5 (custom) and 6 (top) have no counterpart in this executable, which never runs
  above 4.
- **A land load.** At the map clear the list is emptied and the blocks start again from their state at the load
  (`InitialBlockState`, [A land load](#a-land-load)): no visibility bit, not partly outside, every level-of-detail field
  0, the distance the file's. The last rebuild's turn is kept, and the map clear's RebuildCount of 1 gives the first
  rebuild. At that first frame the lines are placed as LH3DIsland::Create places them (`LodLinesAtLandCreation`). The
  detail index is `DetailIndexOf(EngineConfig::detailLevel)`: openblack's levels 5 and 6 are taken as 4, because the
  original's start-up turns a stored index of 5 or more into 4.
- **Model level of detail.** Static models draw the sub-meshes of LOD bit 1, as in the original. **Chosen difference**
  (the user's decision, 2026-10-15): openblack draws the high meshes only at the top detail level, 6
  (`graphics::detail_level::DrawsHighMeshes`, `components::DetailMesh` on villagers, animals and birds, chosen by
  `ecs::drawn_mesh::Of`: the one choice the draw layout counts its ranges with and every row is written with, so that
  each row finds its range); below it, the default 4 included, the std mesh as before. The pose stays the std
  mesh's, taken by bone index (`pose_fit::PoseFits`), and the reflection, the shadow caster, the held object's shadow
  and the culling read the object's own std mesh (`EntityInstance::ownMeshId`, `RenderContext::rowOwnMesh`), as the
  v1.42 exe's do; the testbed's `villagers.high_meshes_at_top_detail` shows it.

- **The options box's detail control.** 🎯 **Chosen difference** (the user's decision, 2026-10-20). In the original the
  control's entry is the detail index itself (entries 0 to 4 are the indices 0 to 4, saved as `detailidx`) and a change
  only takes effect at the next start. Here the entries 0 to 3 set the levels 0 to 3, and the top entry, Maximum
  Detail, sets the top level 6 (`detail_level::LevelOfMenuEntry`). The levels 4 (the default) and 5 have no entry of
  their own and show Maximum, as the original shows its top entry at its default 4 (`MenuEntryOf`). Leaving the page
  on Maximum always sets the top level, also when the control already showed it at 4 (the user, 2026-10-21: picking
  Maximum at 4 had changed nothing); another entry sets its level only when the control was stepped to it
  (`LevelAfterOptions`). openblack keeps no settings for a next
  start, so the change is taken as the Options page is left (another page or the menu closed), and the
  VIDEOCHANGE message is still shown.

### Test hooks

- `--detail-level <0..6>` picks the level at start, the debug bar's World > Detail level (original) while playing; the
  high meshes show at 6. In play the options box's Maximum Detail entry sets 6.
- The full list: [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- The level of detail on the SSE path (fn_007A1800, fn_007ACCE0, fn_007ACD00) was not read; everything in
  [The block's level of detail](#the-blocks-level-of-detail) is the x87 path. fn_007A98B0 and fn_007A3A50 were checked
  for structure only, not instruction by instruction.
