# Sea, coast and rivers

## Summary

Before it draws the sea, the original paints what lies under it: the mirrored sky and moon, the land reflected at half
light without writing depth, the hand, what it holds, the physics objects, the boats and the creature mirrored in
y = 0, the sharks and swimmers cut by the water plane, and the fish-farm shoals. The sea itself is `sky.raw` in rows
2 px high from the edge of a large quad, its period set by the detail level, blended over all that. The coast is the
alpha of each block's texture (cone-weighted altitude plus the land's noise); rivers have no renderer of their own:
two footprints per segment let the sea show through a channel.

openblack draws the same layers into a reflection target the size of the main view and blends its sea over it per
pixel; the block texture (colour and coastal alpha) is identical to the reference.

## The original

### The under-sea pass

**Faithful** except what is marked. A single API for everything the original draws "below" the sea before it
(`GLandscape::Draw` 0x5E48AE..0x5E4E8C): `src/Graphics/SeaPass.h` (CPU, `namespace graphics::sea_pass`) and its twin
`assets/shaders/sea_plane.sh` (GPU, `u_objectClip`, `SeaPlaneDiscard`, `SeaUnmirror`); test `test/test_sea_pass.cpp`.

The original has three mechanisms and a single plane:

| Mechanism | What it does | Plane | Light |
|---|---|---|---|
| A, mirrored land (`fn_007FF4F0`) | height unit [0xC3720C] = 0.67 × [0x8AB678] = −1.0 (0x7FF515..0x7FF52F), [0xFA92DC] = 1 (the blocks reverse their indices, 0x7FF535) | — | table >> 1 (0x7FF53F..0x7FF564), no small bump ([0xC37210] = 0, 0x7FF566..0x7FF577), no Z write (0x5E48C5..0x5E4900) |
| B, `DrawUnderWater` (vt+0x118: `fn_00811010` static, `fn_00810E20` animated, `fn_00813300` complex → `fn_00850FC0`) | mirrors the object at y = 0 (fsubp 0x851094 / 0x8510BD / 0x8510E5) | tests the **mirrored** point: d > 0 out (0x85111C..0x851149) | constant colour obj+0x4C / +0x50 (0x811033..0x81103F → [0xC37D8C] / [0xE9FE2C], read at 0x851082 / 0x85102F), unlit |
| C, `DrawCutByPlane` (vt+0x11C: `fn_0080C050` static / complex, `fn_00811C70` animated → `fn_00858BA0`) | does **not** mirror (0x858C5D..0x858CAE) | tests the point: d < 0 out (0x858D49..0x858D80) | 90 + 165·I >> 8 on obj+0x4C, + obj+0x50 (see the next section) |

- B and C share the CPU clipper `fn_0081D2C0` (its only callers: 0x8515E7 / 0x8516AD and 0x85930E /
  0x8593C5) and the user plane of `fn_00822560` (world [0xF03128], view [0xF03118]). The default plane
  (0, 1, 0, 0) is set by the initialisers `fn_0084A380` (0x84A39A: [0xF0312C] = 0x3F800000) and `fn_0084A3C0`; the
  swimmers set (0, −1, 0, 0) (0x5E4C4A..0x5E4C5A, dwords, 0xBF800000) and restore it at 0x5E4D76. The shark
  (`fn_00774E30` 0x774FF5..0x77501A, restored at 0x7750E6..0x775106) and the net (`fn_00829BC0` 0x829C91..0x829CB7,
  restored at 0x829D25..0x829D45) set **their own** (0, −1, 0, 0), before the swimmers' loop
  (`sea_pass::k_SharkPlane`, `k_NetPlane`: the same values as `k_SwimPlane`). With
  (0, b, 0, 0) both mechanisms keep the same side of the **real** y: b > 0 → y ≥ 0, b < 0 → y ≤ 0 (the mirror and the
  opposite test cancel out). `sea_pass::Kept(Mechanism, plane)` → `SeaPlane {None, KeepAbove, KeepBelow}`.
  - The original's mirrored land swaps its indices with [0xFA92DC] = 1, read in `fn_00875C60` (0x875D7B / 0x8766B7)
    and `fn_00876910` (0x876A2A / 0x87707B).
- **Face**: the material decides in all three places (the Draw 0x84C34A, B 0x851798..0x8517D1, C 0x8594CF..0x8594ED:
  `((~mat+5) & 1)·2 + 1`, `push 0x16` = CULLMODE). `SeaPassState::FaceCull(Surface, twoSided, unmirror)` brings together the
  four places in openblack: models (C1), moon (C2), sky (C3) and land (C4, which in openblack has the opposite vertex
  order: it is an openblack fact, with no address).
- `sea_pass::ForPass(pass)`: `mirrored`, `landLightScale` 0.5, `landWriteZ` and `smallBump` false in Reflection
  (it replaces `DrawSceneDesc::cullBack` and the `mirrored` of `DrawMoon`, which no longer exist).
- `L3DMeshSubmitDesc::sea` (`sea_pass::SeaDraw`: light, plane, unmirror, argb, specular, per-instance colour) replaces
  the six previous fields (`unlitColour`, `landColourOnly`, `clipBelowSea`, `cutByPlane`, `cutColour`,
  `mirrorInSea`). `u_objectLight.x` comes from `SeaDraw::light`: `Normal` 1, `Constant` 2 (B, z = rgb, w = specular),
  `LastDraw` 3 (B with what the last Draw left), `Cut` 4 (C, y = alpha, z = rgb or −1 = that of each instance,
  w = specular). `u_objectClip` = `PackClip` (x plane, y unmirror); the sky writes 0 (bgfx keeps the last value of
  each uniform).
- `Renderer` entry points (`RendererCut.cpp`): `DrawUnderWater(view, mesh, …, SeaDraw)` and `DrawUnderWater(view,
  entity, SeaDraw)` (the hand with `UnderWater(k_HandColour, k_HandSpecular)`, what it holds and the physics objects with
  `UnderWaterLastDraw()`, the boat with `UnderWater(0xFF303070, 0)`), `DrawCutByPlane(view, entity, SeaPlane, argb,
  specular)` (sharks: specular 0, `push 0` 0x775027) and `DrawFishPlots(view, SeaPlane)` (the net: +0x50 = 0 from the
  ctor 0x8164FE). The order is the binary's: sharks (`fn_00775120` 0x5E4B26), fish and nets (`fn_00824B90`
  0x5E4B2B), the swimmers' slot (0x5E4B4C..0x5E4D76) and the hand's glow (0x5E4D89). The boat reflection
  keeps using `ObjectInstanced`: the hull has no `MorphWithTerrain`. An instance that sticks to the ground is not drawn under the sea: the `DrawUnderWater` of the morphable
  vtables (vt+0x118 of 0x9A2E34 / 0x9A2BFC) is a `ret` (0x80BA40), like their `DrawCutByPlane` (0x80BA50).
- Pass colours (`SetColorSpecular` vt+0x2C before the call): hand 0x65A0A0A0 / 0 (0x5E496E / 0x5E496C),
  creature 0x65A0A0D0 / 0x30 (0x5E4ACF / 0x5E4ACD), swimmers 0xFF303070 / 0 (0x5E4C69 / 0x5E4C68), boat 0xFF303070
  (`mov [eax+0x4C]` 0x5E016C, without touching +0x50: **🔍 inferred** openblack sets 0).
  - The boat's DrawUnderWater (static 0x811010 → `fn_00850FC0`) is called at 0x5E0178. The held object's reflection is
    at 0x5E49A2 and the physics objects' at 0x6470C6 / 0x64717C.
- **The hand's 0x65 alpha is not visible** (**🔍 inferred**): `fn_00811010` only sets the table 0xC387C8 (0x8110CF)
  if vt+0x4C = `fn_007F9D80` ((+4 >> 7) & 1) returns 1 (0x8110BF, `test eax, eax / je 0x81114C` 0x8110C2..0x8110C4).
  The hand's LH3D object is `LH3DObject::Create(3)` (`Morphable::MorphInit` 0x61731A → 0x80B5B2, complex vtable
  0x9A3068, whose vt+0x118 is `fn_00813300`: [0xC37D9C] = obj+0x80 and then `fn_00811010`), its flags start at
  0x10009 (0x816537) and a byte search found no call to its vt+0x48 (`fn_007F9D60`, Flags1 | 0x80) in
  the 0x30 bytes after a read of [x+0x482C] (it does not rule out a setter via another path): its `DrawUnderWater` uses the
  normal table and its `AlphaTextured` material takes the texture's alpha.
- Around the hand's `DrawUnderWater`: vt+0x58(0) 0x5E497E (`fn_008168C0`: clears Flags1 0x20; only sets it if
  [0xC38224] ≠ 0) and vt+0x58(edi) 0x5E4991 restores it. **(not ported)**: it is not known what Flags1 0x20 does in this
  draw (`fn_00811010` does not test it).
  - vt+0x58 (`fn_008168C0`) is UseDynamicLighting: it only sets bit 0x20 of +4. The creature's slot also calls
    vt+0x58(0) (0x5E4ADF) after SetColorSpecular (vt+0x2C 0x5E4AD6) and before vt+0x118 (0x5E4AE6).
- The **moon** does use the table 0xC387C8: vt+0x48(1) before its Draw (0x86AC05) and before its `DrawUnderWater`
  (0x86AC3B). Mode 4 → 5 (0x82DD90: same blend and Z, ALPHAOP MODULATE(TEXTURE, DIFFUSE)); `fs_celestial` always
  modulates alpha with `u_colour`, so `DrawMoon` already draws like mode 5 in both passes.
- **PSys mesh atoms with `DrawCutByPlane`**: the bit is the atom's `+0x24 & 4`, not `& 0x10` (`fn_00679F20` `test al, 4`
  0x679F29; set by `CreateParticle` from +0x5F, 0x6A8B94..0x6A8B9A). With the bit, vt+0xF8 (0x679F2F, returns the mesh),
  then `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80 (direct call at 0x679F3C; if it gives 0 it is skipped, 0x679F43)
  and vt+0x11C (0x679F4A) instead of the Draw vt+0x104 (0x679F52); also on the sorted path (the return of `fn_00679F60`
  is `fn_00679F20`, 0x679FBC). `particles::mesh_atoms::Instance` carries the bit (`cutByPlane`, from
  `MeshCreator::drawCutByPlane`) and the specular DrawData+0xC (`specular`), which `RenderingSystem` reads; the alpha of
  DrawData+8 (`alpha`) reaches the cut as the instance's fade (`particles::mesh_atoms::Fade`), which `vs_object` multiplies
  by the cut draw's alpha 0xFF (test `SeaPass.CutAtomsTakeTheirOwnAlpha`). An atom with `DrawCutByPlane` and `DrawWithLandscapeColor` is registered
  once and drawn uncut **(inferred: no effect with both is known)**. The on-screen box test 0x679F3C is provided by
  bgfx's clipping **⚠️ approximate**.
- **Slots with no user** (constants and a TODO with an address in `Renderer.cpp`): the creature (`DrawUnderWater` after the
  physics objects, 0x5E4A84..0x5E4AE6, if its block is visible (+0x920 & 1), the block's +0x9BC ≤ [0xC37200] = 100000,
  and < [0x8AB35C] = 6 and +0xA0 < [0x8AB244] = 0.2) and swimming SuperVillagers (list [0xEB9A08], animation
  "M_P_Swim2" 0xBF3598, the swimmers' plane, vt+0x11C 0x5E4C77, after `fn_00824B90` (0x5E4B2B: fish and nets) and
  before the hand's glow 0x5E4D89).
  - The swimmers' loop: each SuperVillager's Draw vt+0x610 (0x5E4BC2) and shadow `fn_00874850` (0x5E4BFB) come first;
    the animation name is compared at 0x5E4C07; the swim rings every 1000 ms are made at 0x5E4C7D..0x5E4D4B.
- The view plane of the initialiser `fn_0084A3C0` is [0xF0311C] = 1.0f (0x84A3D0); every user puts the default plane back at 0x5E4D76. The swimmers are the list `SuperVillager::g_first` [0xEB9A08].

### Fish-farm fish shoals

**Faithful** (done), except what is said to be missing. The fish puzzle and the rest of the water are in
[water](water.md); creating the farms from the script, in
[fish farms](../scripts/map-script-reader.md#fish-farms).

- **Fish farms** (`FishFarm::CallVirtualFunctionsForCreation` 0x52CC10): rings of radius 2, 4... < 50 × 32
  directions, with the sea flattening **disabled** (`[0xC37BF4]` = 0); the first direction with altitude 0 in two
  consecutive radii gives the centre (x', y of the farm, z'). 15 fish (`fn_00824740`): horizontal `misc0.raw` sprite (flag
  0x40: quad rotated in Y with its local x along the heading), half-width 0.8–1.2, position centre + (±5, −1..0, ±5), heading
  ±π, speed 0.5–1.5, turn speed·(1 ± 0.1)·0.6283; cells 8–23 (frame += dt·speed·25; the cell is taken before the wrap −15·ftol(f/15), so cell 23 shows up;
  `frame_anim::FishFrame`).
  Movement `fn_008248E0` (dt ≤ 0.1 s; the fright +0x20, when not 0, first runs down by the **whole** frame's
  time, clamped at 0, and the speed factor comes from what is left: 4 from 1 s up, else 1 + 3·(1 − (1 − left)),
  0x8248FD..0x82495A; after the turn the heading loses ftol(h·0.15915494)·2π and then 2π more only when above π,
  so a heading turning right past −π stays below −π until a whole turn comes off, 0x824A5B..0x824A99), shoal target `fn_00824DA0` (centre ± 7, timer 0.5·distance; the timer runs down by the frame's whole
  game time, `g_game_time_inc`·0.001 not capped, 0x824E73; the shoals are stepped on every drawn frame,
  `GLandscape::Draw` 0x5E4B2B → `fn_00824B90`, with no test of the time: a paused frame's step of 0 s still takes
  each fish's cell from its wrapped frame and counts the bait; the fish puzzle's net `fn_00829BC0` (0x829BC0..0x829BCB)
  and the bait's hold time (+0x24 += `g_game_time_inc`, 0x824C0A..0x824C1B) take the frame's whole time too);
  beyond 300 it is not drawn, alpha from 200 (with the original's byte overflow). Drawn in mode 6 before the sea.
  - **Scare**: the global splash point 0xEA9F40 / flag 0xEB99F0 are set by
    the **start of the terrain grip over water** (`StartLandscapeGrip` fn_005D1AB0, grip button: growth ring
    7 and the sounds `G_HANDINWATER_01..10` per turn), the creature's footsteps with the foot below y 1
    (fn_00483290) and a physics object falling into the water (fn_0074F2D0). Each shoal within 300 of the camera: target =
    centre + 2·(cos r, 0, sin r) (0x824F70..0x824FA9: x = 2·cos r, z = 2·sin r), timer 2 s; visible fish within 8 (3D distance) flee for 2 s with heading
    atan2(fz − sz, fx − sx). The flag is cleared at the end of the frame.
  - **Fishing**: with the action button over the water with no other object below, a visible fish at < 2 (x, z; fn_00824B10) makes
    the farm the object of the action; `NetworkFriendlyStartLockedSelect` 0x52D770 puts in the hand a HandFood of
    amountPickedUpInitially (25) **without removing it from the stock**, with the particles `SF_MultiPickUpFoodFish`
    (`S_Spangle_A` cells 48–63, 40 fps). `ProcessInInteract` 0x52D950 per turn: n = (int)(8 + 62·t²), t = turns/60,
    ≤ 1400 and ≤ 20000 − what is in the hand; `RemoveFood` 0x52CED0 removes n (or whatever is left) and the hand receives **n**
    (quirk); 0 → it ends. They cannot be returned.
  - **Stock**: +0x94, 1400 on creation (GFishFarmInfo 0: foodValue 1400, food type 1, 16 turns per unit, 4 fishermen).
    The full stock is `GetFoodValue(3)` 0x52D1E0 (the constructor at 0x52C436, `Process` for the cap and the fullness):
    the info's foodValue (+0x68) when the type asked is the info's food type (+0x70) or 3, and the farm's own food type
    has a bit in 3; else 0.0 ([0x8AA398]). the growth period is the info's `numGameTurnsAfterWhichFoodIsIncreased`;
    `Process` 0x52D130 adds 1 every 16 turns, then on **every** turn keeps the stock within 0..1400 (0x52D160..0x52D1A4)
    and stores the shoal's fullness +0x64 = stock / 1400 (0x52D1C0; 1.0 from the shoal's creation, 0x52CE77). Visible
    fish = ftol(15·fullness) (read at 0x824FC7), so a scoop shows from the farm's next process: the hand's scoop runs in
    the turn's inputs, before the global lists' processes, so the drawn count follows it within the same turn ([The
    engine loop](../engine/loop.md)). Those with a high index disappear first and the hidden ones neither move nor are
    drawn. The last argument of `CREATE_TOWN_FISH_FARM` is the GFishFarmInfo index.
- **Fish puzzle** (Land 4, the `FishPlot` net and its baited shoals): in [fish puzzle](sea-life-and-scenery.md#fish-puzzle).

### Sea

From the disassembly:
- `GLandscape::Open` 0x5E5432 loads `.\Data\Textures\sky.raw` (alpha flag; `skya.raw` is the alpha, 4 bits) with
  material mode 5 (= `fn_0082DD90`, the same as 0xE: `SRCALPHA/INVSRCALPHA`, `TEXTURE*DIFFUSE`).
- `GLandscape::Draw`: sky (with the reflected moon) → reflected land (`LandRef`, heights × −1, light table × 0.5,
  `fn_007FF4F0`) → parts under the water (hand, objects, fish…) → hand glow (0x5E4D89) → sea
  (`fn_00879930`) → land. The sea is skipped only if `[0xECA670]` (wireframe, debug) or `[0xECA664]` ≠ 0; the latter is
  set by `PSysLightMaps` (0x6CA57E) to `clamp(level, 0, 1)·190` and the level only goes up/down when a cutscene ends
  (`CameraModePath::Cleanup`): **in normal play the sea is drawn**.
- Strips of 2 px rows on screen, `ZFUNC ALWAYS`, without writing Z. UV = (world + offset) / P,
  P = 2000 − 1800·WaterTiling (560 at the default detail, 200 at the maximum). Offset with the ambient wind
  (−1/330 per ms, applied twice per frame), which is 0 in a game.
  Ripple: each row moves 0.9·sin(i·π/8) along the camera's horizontal front,
  i = (frame + 2·(row+1)) & 15, full beyond 70 depth, nothing below 30.
- Vertex colour = entry 255 of the terrain light table (full light for the time of day); alpha = 255 up to
  depth 7000, dropping linearly to 80 at 14000 (0xC39908). A single colour for the whole sea: **the sea receives no
  cloud shadows** (only the reflected land seen through it).
- WaterTiling is the global 0xC38228 (read by `fn_00879930`).
- The ripple direction is the camera's forward x and z ([0xEA1DD4] / [0xEA1DDC]) scaled to 0.9, unless both are 0.
- The sea material's mode 5 is set by `GLandscape::Open` at 0x5E546C.
- The sea's CPU routines are `fn_008792E0`, `fn_00879500`, `fn_00879930` and `fn_0087A090` (the role of `fn_008792E0`
  is in [Pending](../rendering/frame.md#pending)).
- Texture offsets: [0xFA9370] / [0xFA9374] for detail levels 1..6, [0xFA9390] / [0xFA9394] for level 0.
- Drift constants: −1/330 at 0x9A3C24 (used at 0x879963), −1/330000 at 0x9A3C2C (0x87A130).
- The ambient wind is `g_ambient_wind_direction` (0xEA9E70 / 0xEA9E78) = FastNormalize(int8 `LH3DAtmos::ambient[4]` /
  8, 0, ambient[5] / 8), set every turn by `GGame::ProcessTurn` 0x54E5DC. ambient[4..5] are only written by
  `InitStaticsValues` / `fn_00835AD0` (0), `GGame::Load` and the Internet weather: that is why it is 0 in a game.

- **Sea area** (`fn_00879500`, `sea::ComputeScreenRange`): 30000×30000 quad at y = 0 centred on (2560, 2560)
  (corners −12440 / 17560), clipped with the near plane and the four sides (no far plane). `top` = minimum and
  `bot` = maximum screen y, with their 1/z; `top` is raised to 0 and `bot` lowered to height − 1 without correcting
  its 1/z. Oddity copied: the first vertex can only set `bot` (`if y > bot … else if y < top`). The clipping is the
  original's: vertices 0..3 = (−12440, −12440), (17560, −12440), (17560, 17560), (−12440, 17560) and triangles
  (0, 2, 1), (0, 3, 2) (0x879537); each one goes through the recursive clipper `fn_0081A760` (x87) / `fn_007A3A50`
  (SSE, P4), one plane per bit from 0x20 (near) to 0x02, two new vertices per cut at the end of the table and, if two
  triangles remain, the first one clipped by a recursive call; the new vertices carry only the codes of the following
  planes and their screen y is clamped to 0..height−1 (`g_MaxScreen`, 0x81E130). `bot`/`top` are read in the order of
  that list. Checked with a Unicorn emulation of `fn_00879500`: both paths
  give the same list and the same y (±0.05 px of rounding) in 400 random cameras; `test_sea_rows` carries two cases
  from the emulation. In 30000 cameras over the island the "only `bot`" oddity never moved `top` by more than 1 px.
  **Bug in the original, copied**: to decide whether clipping is needed it adds up the codes of 0, 1, 2 and of entry
  **4** of the table (`[0xE3B5F0]`, instead of 3 at 0xE3B5EC), which is a leftover from the last draw by LH3DP3. If
  that entry and codes 0..2 are 0 with vertex 3 outside, it draws without clipping with the camera x', y', z' of
  vertex 3 (a garbage `top`). It requires seeing corners 0, 1 and 2 at the same time: it does not happen in any of
  200000 cameras over the island's disc (radius 5120, height 3..4000), so entry 4 = 0 is assumed. Near plane:
  `[0xE839E0]`, which `GCamera::Update` 0x4424AF sets every frame with `LandFeature::GetNearClipping` 0x5E2F30
  (see [Camera](../camera/game-camera.md#camera)): it is the `cameraNearClip` that openblack recalculates in `Game` and the one the sea uses.
  - Clip codes of LH3DP3 (`fn_008797F0` 0x87985A..0x8798C3): 0x20 in front of the near plane, 0x10 / 0x08 right /
    left, 0x04 / 0x02 top / bottom of the 90° frustum of the pre-scaled camera matrix (|x'| ≤ z, |y'| ≤ z); no far
    plane.
  - The sea is clipped with `g_NoBackfaceCull` = 1 (0x8795AC): no triangle is culled.
  - New edge vertex (`fn_0081DD90` / SSE `fn_007A1480`): t = d(in) / (d(in) − d(out)), p = in·(1 − t) + out·t; the
    distance to the plane comes from the jump table 0x81E17C; its clip code only tests the planes after the one being
    cut (jump table 0x81E1B4, 0x81E063), never the near plane again; its screen position is clamped at 0x81E0F1 to
    `g_MaxScreen` 0xC2AB00 (width − 1, height − 1).
  - Triangles left after each cut in `fn_0081A760`: (c, c−a, c−b) 0x81A7C1; (b−a, b, b−c) 0x81A835; (b−a, b, c) then
    (b−a, c, c−a) 0x81A856; (a, a−b, a−c) 0x81A8C8; (a, a−b, c) then (a−b, c−b, c) 0x81A8E9; (a−c, a, b) then (a−c, b,
    b−c) 0x81A928; nothing to clip 0x81A971.
  - `fn_00879500` returns no sea when the whole quad is outside one plane (0x8795CA, 0x879649) or when bottom < 0 or
    top > height − 1 (0x879768..0x8797E2).
  - top starts at the screen height and bottom at −1 (0x879651..0x879762); screen y = (1 − y'/z)·height/2
    (`fn_008797F0` 0x8798E6). The two 1/z are stored as rhw = near/z at [0xFA9384] / [0xFA9388] and divided by near in
    `fn_00879930`.
- **Rows** (`fn_00879930`): rows of vertices r = 0..n at y = ftol(top) + 2r, n = (ftol(bot) − ftol(top) + 2)/2;
  1/z of the row = 1/z(top) + r·(1/z(bot) − 1/z(top))/(n − 1). `vs_water` draws a full-screen quad and `fs_water`
  redoes each row per pixel: point on the plane under the row, ripple on the even rows (full if 1/z < 1/70,
  (1/z − 30)·0.025 if < 1/30), the odd rows repeat the UV of the previous one (2 identical px and 2 interpolated px,
  without perspective correction), alpha per row rounded and **row 0 with alpha 0x20** if ftol(top) ≥ 1.
  It is counted from `top` and with `u_viewRect`, so it does not depend on the API.
  - The rows are set up at 0x879AD7..0x879B3E; the alpha 0x20 of row 0 is decided by "y of row 0 > 0.5" (0x879D5A).
- Outside the rows and below the horizon (between the far edge of the quad and the horizon, or with no sea on screen)
  what is under the sea is seen unblended: the **strip of mirrored sky** above the edge of the sea (about 25 px at 864
  high with the camera at a height of 300). Above the horizon, the sky of the main view.
- **Frame** `[0xFA938C]`: goes up by 1 (& 15) per drawn frame only if the sea is on screen and the game is running
  (`g_game_time_inc` ≠ 0): **while paused the sea lines do not move** (identical captures while paused). At modern fps
  it shimmers faster than at ~30 fps, as the original would at that speed.
- **Drift with the wind** (`sea::Drift`): off += wind·ms·(−1/330), twice (0x879963 always, 0x879A69 if there is sea on
  screen), wrapped to P; at level 0, off0 += wind·ms·(−1/330000) wrapped to (−1, 1). The two steps are the floats
  0xBB46980D ([0x9A3C24]) and 0xB64B5C35 ([0x9A3C2C]), one bit away from −1/330 and −1/330000 rounded; openblack
  uses those bits (oracle run of `fn_0087A090`: one second at wind (1, 0.5) gives off0 = 0xBB46980C / 0xBAC6980C, test
  `SeaRows.AtTheLowestDetailTheSeaIsAStillSquareOfFiftyRepeats`). The ambient wind is 0
  (`sea::k_AmbientWind`), so it does not move: the code is there for the day it is not 0.
- **Detail level 0** (`fn_0087A090`, WaterTiling = 0): world quad of ±70000, UV (x + 70000)/2800 and
  (70000 − z)/2800 (V grows towards −z) plus its drift, vertex alpha 255, without ripple or fade.
- **Texture in 4 bits** (`fn_00837400`): at load time, `sky.raw` and `skya.raw` keep `v >> 4` (n/15, like ARGB4444 in
  D3D). The colour of `sky.raw` has a deviation of 5-8 out of 255, so 4-6 levels per channel remain and close up flat
  blotches are seen.
- **What is under the sea** is the reflection target, now **the size of the main view** (it is recreated when the size
  changes; before it was 1024²) and painted **in order** (sequential view, like `GLandscape::Draw` 0x5E48AE–0x5E4E6B):
  mirrored sky, reflected moon, reflected land (`fn_007FF4F0`: half light, **no small bump** because `[0xC37210]` = 0,
  no dynamic shadows; the cloud ones yes; **without writing Z**: `GLandscape::Draw` 0x5E48C5–0x5E4900 sets ZWRITEENABLE
  (14) to 0 before `fn_007FF4F0` and to 1 afterwards, so it does not cover anything that comes after and, between
  blocks, the last one drawn wins: the farthest, see [Coast](#coast)), hand and objects under the water, schools of fish
  and the **hand glow**. it no longer does. No models or sprites. `fs_water` blends `light·sky` over it with alpha
  `skya·row alpha`, the same as SRCALPHA/INVSRCALPHA over the frame.
- Sea with ZFUNC ALWAYS, without writing Z and without culling; the main view is sequential and the land that comes
  afterwards covers it.
- The sea level of the rows' quad is [0xFA936C] = 0 (`fn_00879500`); its corners are the floats 0xC6426000 / 0x46893000 (−12440 / 17560), and the detail-level-0 quad is ±70000 (0x4788B800) with 50 texture repeats.
### Coast

**Faithful** (done). The normal and SSE paths do the same. The earlier attempt (flattening and skipping
cells, nothing more) gave a "stepped sand floor" because it lacked the piece that gives the shore its shape: the
**alpha of the block texture**.
- **Coastal alpha per texel** (`fn_008732C0`, SSE `fn_007AB4B0`, 16×16 texels per cell, 256 per block): altitude
  weighted with cones `h = Σ w_k·alt_k` (`fn_00871560`: `w_k = max(0, 14 − d_k)` to the 4 corners, truncated to Σ = 255
  and +1 to the largest), `h < 0x100` → 0, `h ≥ 0x400` → 15, in between `e = h + 4·(int8)noise` (LND noise,
  `[x·256 + z]`, the same in all blocks) and `idx = 15 + trunc((15e − 14250)/438)` on the table 0xC39814 =
  0xC2AAC0 (16 dwords already shifted 12 bits: nibbles 0,0,0,1,2,4,6,8,9,10,10,11,12,13,14,15). Transparent below an
  altitude of ~1.2–2, opaque towards ~2.9–3.7; the noise draws the shoreline.
- **Full-water cells** (bit 0x02 of the `flags` byte, `LNDCell::k_FullWaterFlag`, `word(cell+6) & 0x200`, the 2015
  `lnd.h`'s MASK_FULLWATER): their triangles are not emitted (0x875DDC, 0x876A8A **and** SSE 0x7A9B14, 0x7AAB14),
  only while the block's meshBlending (+0x934) is 0 (tests at 0x875D98 / 0x876A47), and the builder zeroes their
  texels (0x87382B, 0x8739FC, 0x873B24, 0x873CEE). No code writes the bit: it only comes from the file. 3297 cells in
  Land1. Bit 0x02 is **not** part of the sound code: bits 2..5 of the byte are the ATMOS type.
- **Mesh flattening**: `y = alt ≤ 3 ? 0 : alt·0.67` at each vertex (0x874B95, SSE 0x7A1EE7, `cmpleps` against 3.0
  at 0xFC01F0). The game (`GetAltitude` 0x803121) only flattens next to a base corner ≤ 4; in Land1 there is no cell
  where the two rules give different heights (base > 4 with a corner ≤ 3: 0 of 25600).
- **Specular**: `cell dword | 0xFF000000`; its alpha is 0 if UseSmallBump and alt ≤ 1 (0x874BA9, SSE 0x7A1F16).
  The small bump pass copies it to its diffuse (alpha 0 at those vertices, see
  [Terrain detail](terrain.md#terrain-detail-small-bump)) and skips the triangles with all 3 vertices like that (SSE
  0x7A31A0). The small bump is **not** modulated by the coastal alpha, but it fades to 0 towards the vertices with
  altitude ≤ 1: over the shallow water its specks only appear next to higher vertices, without an edge.
- **No hard edge next to the open-sea cells**: on the 6 lands, the 0x02 cells have all 4 corners at altitude 0 and
  the coastal alpha of the neighbouring drawn cells is 0 in every texel of the shared edge
  (Land1 871 edges, Land2 1722, Land3 900, Land4 1283, Land5 1808, LandT 556,
  all with maximum nibble 0). A straight edge in openblack cannot come from the coastal alpha.
- **Dynamic shadows** (`fn_00878350`): vertex colour 0 where the altitude byte ≤ 1 → they fade towards the water
  (interpolated), without a hard cut.
- The land is drawn in mode 14 (SRCALPHA/INVSRCALPHA) over the already painted sea and **writes Z even if the alpha is
  0** (`fn_0082DD90` sets ALPHATESTENABLE = 0 at 0x82DE2A and does not touch ZWRITEENABLE). Consequence, in the original
  and in openblack: whatever is **below y = 0** and is drawn afterwards with its normal Draw (a tree or other physical
  object that sinks before being deleted at −4R, see [sinking, drowning and being deleted](water.md#sinking-drowning-and-being-deleted)) is covered in
  all the drawn cells and seen whole, as if floating, over the 0x02 cells, which do not write Z: it comes out **cut into
  rectangles** along the edge of the open-sea cells.
- **Block order** (land and reflected land): the list from `LH3DIsland::PreDraw` (0x7FF45F–0x7FF4DD), ascending by
  `block+0x9BC` = distance from the camera to the centre (x + 80, 0, z + 80) of the block with LandRef (`fn_00877210`
  0x87722C–0x877296, 0x877C8A–0x877CCD): **the closest first**; `fn_007FF610` and `fn_007FF4F0` walk the same list
  (+0x9B8). Without Z on the reflected land, that order decides which mirrored hill covers which.
- **Cone weights, tie-break**: the +1 goes to the largest and, among equals, to the **last** (w3 before w0): the table
  that `fn_00871560` builds at 0xE3A3E0, read from a Unicorn run, gives e.g. at texel (8, 8) 63, 63, 63, 66.
  31 of the 256 texels (the lines i = 8 and j = 8) change with respect to the previous rule (the coastal alpha barely).
- **Colour of the block texture (done)**: `fn_00873790` per cell and `fn_008732C0` per texel, with the block's
  `(x·256 + z)` index for the material, the noise and the bump (`[0xFA7698]`, the LND's bump):
  - `h < 0x100` → texel 0. Otherwise, entry `mat = min((h >> 8) + noise, 255)` of the country's table (`[0xFA75C0 +
    4·country]`, country = low nibble of byte 6 of the cell): {index 0, index 1, coef}. 5-bit colour of the material
    textures (`[0xFA74F8 + 4·i]`, B5G5R5 after the type u16): `c0·coef + c1·(256 − coef)` (the **first** one carries
    the coefficient; only c0 if they are equal), × bump and to 4 bits: R = (Σ(c & 0x7C00)·bump) >> 18, G >> 17, B >> 16,
    each one clamped to 15 (0x873438..0x8734EA); alpha the coastal nibble.
  - If the cell's 4 corners are not of the same country, it is built once per corner country and `fn_00871850`
    blends per 4-bit channel (alpha included) with the cone weights: `floor(Σ channel_k·w_k / 255)`.
  - Checked: the Unicorn emulation of `fn_00873790` matches the
    reference in the 163840 texels of 40 rows of Land1 cells along the x87 path; the SSE one (P4, `fn_007AB4B0` with
    `pmulhuw`) lowers the blue nibble (sometimes the green) by 1 in ~1.8 % of the texels. The x87 one is followed. The
    texture openblack generates (`OPENBLACK_DUMP_BLOCK_TEXTURE`) is identical to the reference in 524288 compared
    texels.
  - `h < 0x100` gives texel 0, black and transparent (0x87339C). The country's table entry is 12 bytes: {index 0,
    index 1, coefficient} at country + 4 + 12·mat (0x8733BF).
  - One build when the 4 corners share the country, else one per corner country (0x8737EA..0x873821); `fn_00871850`
    blends at 0x8718EC..0x871A36 with the 0x80808081 multiply per channel mask (the result is the same for equal
    countries: the weights add up to 255).
  - The LND noise map is [0xFA7694] (the bump map [0xFA7698]); both 256×256, index x·256 + z, the same for every
    block.
  - This settles the question in [Research tools](../project/research-tools.md): the material entry is `min((h >> 8) + noise, 255)`, neither the
    editor's `min(alt + noise/4)` nor openblack's former `(alt + noise) % 256`.

### Rivers

**Faithful** (done).

- **There is no river renderer.** `GStream` (GameThing 0x47, Stream.cpp) stores its points in script order
  (`CREATE_STREAM_POINT` 0x717550 sets y = ground height and appends at the end); the segments are p[i] → p[i+1].
- `GStream::CreateAll` 0x733FF0 (after the script) → `CreateRiver` 0x7341E0: per segment, angle θ = atan2(dz, dx),
  position p[i], scale only in local X = 3D length / 30, and two terrain footprints (fn_0081E9E0):
  `data\river2.l3d` (brown riverbed, blended into the block's colour like a building's footprint, fn_008728A0) and
  `data\river.l3d` (flag +0x38 = 1: only its alpha, which **lowers** the block's alpha nibble: min(dst, src),
  fn_00872AB0; banks 15/15, channel 10-14/15). 32×64 ARGB4444 footprints, 10.6 × 30 units.
- The visible water is **the sea**: it is drawn before the land (ZFUNC ALWAYS) and the land (mode 14, SRCALPHA) lets
  it show through in the channel. No texture, scrolling, particles or LOD of its own; the "flow" is the sea's movement.
- **In the sea's reflection** the channels are cut the same way: the mirrored land (`fn_007FF4F0`) draws every block through the same block preparation and block draw as the land (`fn_00877D20`, `fn_00874AA0`, also called by the land's `fn_007FF610` at 0x7FF66C / 0x7FF678), so with the same block texture, whose alpha already carries the channel (baked by `fn_00872AB0`), and the same material mode. The reflection flag [0xFA92DC] is read only by the poly list builders (`LandBlock::CreateListPoly*`, to swap the indices); the other differences are the height unit × −1, the light table >> 1 and no small bump. So in the reflection the mirrored sky drawn before shows through the channel, as the sea does on the land.
- Not analysed: the sound `ATMOS_TYPE_RUNNING_WATER` (`audio/sfx/atmos/stream.sad`). The waterfalls: see
  [fixed scenery per land: Land 3 waterfall, Land 4 ark and dinosaur](sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur) (`GWaterfall` draws nothing; the one in `waterfall3.l3d` is `DesignedWaterFall`, only in Land 3).

## openblack

Status: **done** for the under-sea pass (`graphics::sea_pass`), the sea, the coast and its block texture, the rivers
and the fish shoals; what is missing is in [Pending](#pending).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **The under-sea pass**
  - openblack: `RenderContext::cutAtomInstances` (with `particles::manager::k_DrawByPath` all of them, opaque or not, in
    `particleAtoms`: with their Z-object in a `Sorted`, at their place inside the effect in a `Queued`),
    `sea_pass::CutAtoms` (default plane, instance colour, specular and alpha: `fn_00858BA0` takes the vertex alpha from
    obj+0x4C & 0xFF000000, 0x858C42 → [ebp−0x24], OR at 0x858D60, that is the alpha of DrawData+8, which the
    instance's fade carries, `particles::mesh_atoms::Fade`).
  - **openblack**: the `RenderPass::Reflection` pass draws with the mirrored camera (`ReflectionXZCamera`), which
    already gives B's mirror. What C draws inside that pass is **un-mirrored** (`SeaDraw::unmirror`, `SeaUnmirror`
    in `vs_object`, in both its branches), and the plane is a per-fragment discard on the real y (`fs_object`,
    **⚠️ approximate**: strict per pixel, y = 0 stays on both sides; the original clips triangles). The same with the
    fish sprites (`Unmirror` on the CPU) and the moon view (`UnmirrorView`).
- **Fish-farm fish shoals**
  - openblack: `fish_farms::FoodValue` / `FullStock`
- **Sea**
  - In openblack (`fs_water`/`vs_water`, `Graphics/RendererSea.cpp`, `Graphics/SeaRows`): **done like the
    original**.
  - openblack drew it writing Z (its flat cells at y = 0, even if they were transparent, and the mirrored hills
    covered the reflections that came after)
- **Coast**
  - openblack sorts them the same way in `Renderer::DrawPass`.
- **Rivers**
  - openblack linked each point with the closest previous one: corrected, `Stream::points`

### The coast in openblack

- `3D/CoastAlpha` (port of the reference script, identical texel by texel in Land1),
  today the alpha channel of `LandIsland`'s RGBA8 `BlockTexture` (see below; linear filter, rows along +z; it is
  a changed block is painted again through the block texture queue, or at once before its first draw,
  [terrain.md](terrain.md#the-block-texture-queue-in-openblack));
  `LandBlock` flattens, collapses the 0x02 cells to a point and passes the "shore
  fade" (alt > 1) per vertex; `fs_terrain`: `a = min(costa, LandAlpha)` (the rivers still use `min`), without
  `discard`, the two passes as a premultiplied colour (blend ONE/INV_SRC_ALPHA: `(land+spec)·a·(1−b) +
  (bump+spec)·b`) and the shadows also multiply what is seen behind. `GetDrawnHeightAt` = height of the drawn mesh.
  Differences: a single texture for the island (the bilinear filter crosses the block edges; the original had one
  texture per block).
- `3D/BlockTexture` (`CountryTexel`, `BlendCorners`, `BuildIslandBlockTexture`); `LandIsland` keeps the
  material textures and the bump in CPU and uploads an RGBA8 `BlockTexture` (nibble × 17, colour and coastal alpha)
  that replaces the R8 `CoastAlpha`; `fs_terrain` takes the colour (it already includes the bump) and the alpha from
  it; on top go the footprints, the static shadows and the light as before.

### The rivers in openblack

- `ECS/Rivers` creates, when loading the map, two `StreamFootprint` + `Transform` entities per segment; the
  riverbed and the channel are baked into the blocks' textures at their prints step, the newest first
  (`ecs::land_prints`): the bed blends its colour, the channel lowers the block's alpha to the lower of the two
  (`block_texture::PrintFootprint`'s alpha mode), which `fs_terrain` takes as the land's alpha.

### The fish shoals in openblack

- `FishFarmArchetype`, `ecs::UpdateFishShoals` (frame game time), `Renderer::DrawFishShoals`.
  Since `fs_water` composes the opaque sea with the reflection texture, "what is behind the sea" is the reflection pass:
  the fish are drawn there **mirrored** (y → −y) and without a Z test, over the reflected land (which in the original does not
  write Z). The same would apply to the underwater cuts.
- `ecs::SplashWater` / `ProcessFishFarmsTurn` / `FindFishFarmAt` / `RemoveFishFarmFood` (FishShoals.cpp),
  `HandFish.cpp` (`SplashHand`, `TryPickUpFish`, `ProcessInInteractFish`), splash when thrown objects land in
  `AttemptCollisionSound` (0x6465B7). The scoop's looping pick-up sound rises in pitch from 60 to 240 % with t² as the
  original's (`ProcessInInteractFish`, `HandSystem::UpdatePickupSound`); over the water in the influence a shown fish within 2 units gives the hand the "Pick up"
  tooltip (`hand_tooltips::LandToolTip`, test `HandToolTips.AHintComesBeforeAFishAndAFishBeforeTheMove`); the fishermen
  are the villagers' fish-farm job (`VillagerFishFarms.cpp`, tests `FishFarm.*` and `FishFarmFinder.*` in
  `test_fish_farm.cpp`).
- The creature's step scares the fish too: a foot set down below a height of 1 sets the same splash point as the hand's
  (`creature_footprints::Splashes`, `FootprintSystem::Step`, `ecs::PendingSplash`; tests
  `FootprintSystemTest.AFootSetDownInTheWaterSplashesTheFish` and `FootprintSystemTest.AFootSetDownAtAHeightOfOneMakesNoSplash`).

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/CoastAlpha.h`: `BuildIslandCoastAlpha`, `CoastAlphaNibble`.
- `src/3D/Implementations/Ocean.cpp`: `Ocean::CreateMesh`.
- `src/3D/LandBlock.cpp`: `LandBlock::BuildVertexList`.
- `src/Graphics/Renderer.cpp`: `SuperVillagerFrame::swimmers`.
- `src/Graphics/RendererSea.cpp`: `Renderer::DrawSea`.
- `src/Graphics/SeaPass.h`: `FaceCull`, `ReflectionXZCamera::GetViewMatrix`, `UnderWaterLastDraw`.
- `src/Graphics/SeaRows.cpp`: `ClipCode`.

### Test hooks

- Hook `OPENBLACK_SEA_TRACE=1` (every 500 frames: first row, n, 1/z, step, smooth row, frame, drift).
  Test: `test_sea_rows`.
- `OPENBLACK_DUMP_BLOCK_TEXTURE` (coast block texture); the cut and fish scenes are in
  [Test hooks](../rendering/model-lighting.md#test-hooks). The full list:
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- The coast's block texture is one texture for the whole island, filtered linearly across the block edges
  (`LandIsland::CreateBlockTexture`, `assets/shaders/fs_terrain.sc`); the original has one texture per block
  ([The coast in openblack](#the-coast-in-openblack)).

### From the model rendering notes

- Under-sea pass, the boned bodies' cut (0x811C70): with no clip (+0x80 = 0) it calls `fn_00839980`
  (0x811CA4..0x811CC1) and has no `SetTransform` branch, which is not read further; openblack's cut takes the rest bones
  there, as the reflection does. The creature has no `SkeletalAnimation`; its reflection is its own draw
  (`DrawCreatureReflection`), in its drawn pose.
- Under-sea pass, not yet seen or read: a capture of the shield dome further into the sea (e.g.
  `PHYSICAL_SHIELD,1800.3120`; the one at 1825.3140 stays almost entirely on the beach); the +0x50 specular of the
  boat hull (what its last Draw left, **🔍 inferred** 0).
- Under-sea pass, captures: that the migration to `sea_pass` does not change any pixel **is not demonstrated** with
  captures (the code does give it: hand 0xA0/255, specular 0 of mode 4, same culling, `UnmirrorView` = view·diag(1,
  −1, 1, 1)); two runs of the same exe differed because the sea and the clouds followed the real clock, and some views
  (coast, moon, cut rock, Land 4 net) came out somewhat above that noise. Also missing: the reflection of the boat hull
  (the view shows the ark on land, with no sea in front), a shark before/after pair, and why the first capture of the
  resting physics rock (y = 0.75) came out without a reflection and the others did (an intermittent failure of
  `DrawObjectReflections`; and whether the reflection should be that light, at half light).
- Question for the user (implemented "as in the original" as read from the binary; they depend only on how the game
  looked):
  - **Under-sea pass** (`sea_pass`): (1) were the shield dome and the PSys mesh effects next to the sea cut flush with
    the water and without a reflection? (0x679F4A → fn_00858BA0; done that way); (2) the hand's reflection has colour
    0x65A0A0A0: grey and semi-transparent?; (3) the original blends the sea over the sky without mirroring it and
    openblack reflects the sky: was it noticeable in the distant water?; (4) morphable objects (houses, fields, ark,
    physical shield) are not reflected (their DrawUnderWater is a `ret`, 0x80BA40; done that way): do you remember it
    the same way?
