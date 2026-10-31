# Model lighting and colour

## Summary

The original lights every normal model on the CPU, per vertex, with one integer rule: the land's light table under
the object (bilinear over four cells) gives the base colour, one point light (a fixed sun by day, next to the hand in
full night) gives `I = fistp(255·N·L)`, and the diffuse is `(c·f) >> 8` with an ambient of 90. The animated class
(the advisors, the creature's and the hand's bodies) has a float rule of its own; trees are unlit. All colours go
through the LH3DColor byte arithmetic (multiply and shift, saturated add).

openblack reproduces the rules in `model_light` (CPU and shader twins) and the colour arithmetic in `argb_colour`,
carrying each object's colours per instance to the shaders. This page also gathers the test hooks of the model
rendering pages (meshes, textures, transparency, shadows).

## The original

### Model lighting

**Faithful**.

- **A single rule, integer and on the CPU** (`fn_0084BA90`, D3DTLVERTEX vertices), the one for all normal models
  (buildings, villagers, rocks; **not** the trees, dead trees and big forests, which are unlit: see "Unlit objects"
  below): `I = fistp(255 · N·L)` (0x84BBAF..0x84BBBE, round to nearest with
  ties to even, the FPU's normal mode), `f = I < 0 ? amb : amb + ((255 − amb)·I >> 8)` (0x84BBC3..0x84BBE5) with
  `amb` = [0xC39264] = 90, and the per-channel diffuse `(c·f) >> 8` truncated, with alpha untouched (0x84BBEA..0x84BC1D). So
  `f` reaches at most 254/256, never 1. Variant with `__ftol` truncation instead of to-even: 0x859649
  (`fn_00859530`, `fn_00859D90`, `fn_00878C70`).
- **The animated objects have a rule of their own.** The boned draw of the animated class (vtable 0x9A3068, made by
  `LH3DObject::Create(3)`: the advisors, the creature's and the hand's bodies), vt+0x108 `fn_00813BA0`, lights each
  primitive in `fn_0084C3C0` (0x813E90 / 0x813F22): the light brought into each bone's space (0x84C418) and normalised
  (0x84C4C7), d = N·L (0x84C626..0x84C63D), `lit = min((d < 0 ? 0 : d × 1.3) + 0.4, 1)` ([0x8AA398] 0, [0x8C7A18] 1.3,
  [0x8C7A44] 0.4, [0x8AA390] 1; 0x84C63F..0x84C671), `I = ftol(lit × 255)` ([0x8AB270], 0x84C677..0x84C67D), each
  channel of the object colour [0xC37D8C] times I >> 8 with its alpha kept (0x84C682..0x84C6B6), the specular [0xE9FE2C]
  as the vertex's (0x84C623). No ambient term. The constants are the floats 0x3FA66666 (1.3), 0x3ECCCCCD (0.4) and
  0x437F0000 (255), every step rounded to float (control word 0x007F); `fcom` sets C0 for a NaN, so a NaN N·L counts as
  facing away (102). Oracle run of `fn_0084C3C0` (one identity bone, light (0,0,1), `InverseSquareRoot` stubbed to 1,
  normals (0,0,d), colours 0x80FF4020 / 0x01818181 / 0xFFFFFFFF; diffuse at vertex +0x10 of 0xE437E0): d ≤ 0 → 102, 0.01
  → 105, 0.1 → 135, 0x3E7D4C11 → 183 / 0x3E7D4C12 → 184, 0.25 → 184, 0.4 → 234, 0.46 → 254, 0x3EEC4EC3 → 254, 0x3EEC4EC4
  (the clamp) → 255, ±inf → 255 / 102; pinned in test_model_light. The GPU twin `AnimatedLightIntensity` follows the
  same steps but its dot product's order and fused multiply-adds are the driver's, so a value within one float step of a
  truncation edge may land one level apart there. the creature's and the hand's bodies still take the integer rule
  above.
- **The float formula from 166 is never executed** in this build: it is the D3D path `fn_0082C680`, which is only reached by
  `LH3DObject::DrawTnL`, and `DrawTnL` requires the hardware T&L flag [0xECA60C] (0x80DC7A), which `OpenD3D` only sets to 1
  if [0xC386E4] is 0 (0x82D0F5); `start_system` writes 1 to it unconditionally (0x642EA7). That is why openblack no longer has it.
- Object base colour (`fn_00801C90`), one per object and frame: `table[brightness]` of the 4 cells around
  its origin, bilinear; specular = RGB of those cells read as D3DCOLOR (R and B swapped; almost always 0).
- **The light is a point and it moves with the time of day.** LH3DTech stores a single light, [0xEA9E90] (`SetLight`
  `fn_0081E1F0`; saving and restoring is up to the caller, 0x8254A3/0x82551F). `fn_005E5830`, called by
  `GLandscape::Draw` (0x5E488E) once per frame before the models, leaves it at the default sun [0xEA1C88] = (−500000,
  500000, −500000) (`__xc_a` initialiser `fn_00818920` 0x818930) except in **full night**: if the sky type is > 1.5 (the
  double at [0x8C5838]; `LH3DSky::Time2SkyType` 0x86A1B0 of the visual time computed there, 0x5E58D1..0x5E58DF, 2 =
  night.
  - At start-up `fn_00818950` (0x818960) copies the default sun into the light [0xEA9E90].
  - `fn_0081E1F0` writes the position at 0x81E1F6..0x81E20D and returns it through the hidden pointer of its first
    argument (0x81E2E6).
  - `fn_005E5830`: the focus is lifted at 0x5E58AC..0x5E58C6 (the 10 is used at 0x5E58B1); the sky-type test
    0x5E5A6C..0x5E5A77 (`test ah, 0x41` / `jne`) sends "not greater" **and the unordered case** to the day branch;
    only an exactly null focus→camera vector is left unnormalised (0x5E5AA9..0x5E5AD6); the 3 is used at 0x5E5B12. The
    focus is the hand model's position even while the hand is hidden.
  - A SuperVillager's HD body and its eye meshes are drawn with the light at the default sun (`fn_008254A0`
    0x8254C3..0x8254D1, restored 0x82551F).
  - With the cursor off the terrain (in the sky) the original keeps moving the hand along the mouse ray at its
    distance from the view (`ObtainRequiredHandPosition` 0x5B5E70; `CHand::fn_0046DF60` keeps
    |camera − position| if the ray does not touch land). In openblack `HandSystem::Place` leaves the hand where it was last
    placed (or at its initial spot), so at night the light stays there and the faces of distant models can
    end up with almost only the ambient: **known difference** (pending in the hand system). To test night
    lighting, put the cursor over the terrain (`OPENBLACK_MOUSE_AT`). Without a hand or without a camera, `Renderer::DrawScene`
    leaves the default sun (the daytime branch, 0x5E5B70) **🔍 inferred**.
- **N·L is done in mesh space, not with the rotated normal**: `fn_00855340` brings the light into the object's space with
  the general inverse of its matrix (`LHMatrix::SetInverse` 0x7FB290) and normalises it (0xF03140); the boned branch does
  `SetInverse` of each bone matrix (0x84BD9E) on the light already transformed to camera space (0x84BDA3), which is the same per
  bone if those matrices go from the bone to the camera and the camera cancels out **🔍 inferred**. The vertex normal goes in
  raw, neither rotated nor normalised, and the direction comes from the **origin** of the bone or the object. With uniform scale it gives the
  same as rotating the normal; with per-axis scale (swaying a tree, the shear of a field) it does not.
- The plane cut (`DrawCutByPlane`, `fn_00858BA0`) uses the light [0xF03140] in **object** space: the two
  callers set it once with `fn_00855340` on obj+0x14 (static 0x80C0EE, animated 0x811D2F) and `fn_00858BA0`
  reads it in both its branches (rigid 0x858CB1, boned 0x859049); the bone matrices (0x858F77) only move the
  positions. `vs_object` mode 4 uses only the instance matrix.
  - LightInMeshSpace: SetInverse at 0x855349, the light as a point through it (0x85534E..0x8553D3) and
    InverseSquareRoot at 0x85540A; the boned branch 0x84BD82..0x84BDFE.
- Mists and clouds raise the ambient to 210 while they are drawn (`fn_007FA300` 0x7FA56D, confirmed: xref immediates; back to 90 at
  0x7FA586; the light is saved and raised at 0x7FA53C..0x7FA563 and restored at 0x7FA590).
- **Unlit objects** (✅ faithful). "Unlit" objects go through `fn_00856D40`/`fn_0085BA30`, with the base colour
  [0xC37D8C] as is (0x856D89, 0x85BA68): the selector is the edx argument of `fn_0080D910` (0x80D926 `test edx, edx`:
  `fn_0084BA90` if non-zero, `fn_00856D40` if 0). That edx comes from the LH3DObject's slot vt+0x5C, `IsDynamicLighting`
  (`fn_007F97D0`, bit 5 of Flags1): the object draw `fn_0080DB30` calls it at 0x80DF17; true brings the light into mesh
  space (`fn_00855340`) and calls `fn_0080D910` with edx = 1, false (0x80E065) calls it with edx = 0
  (0x80E0E0..0x80E0E4). Slot vt+0x58, `SetDynamicLighting` (`fn_008168C0`), sets that bit only if [0xC38224] is set and
  its argument is non-zero, and clears it otherwise. `Object::Create3DObject` passes 1 (0x636611, 0x636B44);
  `Tree::CallVirtualFunctionsForCreation` (0x749F70) passes 0 (0x749F87..0x749F89), and so do BigForest (0x43906E),
  DeadTree (0x510B0B), Creed (0x50B464) and ParticleVolBlendMesh (0x6AADF7), found by the calls through vt+0x58. So a
  tree's vertices take its colour as `Tree::Draw` leaves it (the cell's light, the haze, then the brightness), with no
  N·L and no ambient 90. `vs_object` then leaves out `ModelLightDiffuse`. Primitives with bit 0x1000 (with [0xE9FE44])
  do not go that way: they take the alternative colour [0xC37D98] (`fn_0080AD90` 0x80ADBC, `fn_0080AF80` 0x80AFAC) and
  are lit in `fn_00859530` (the rule with `__ftol`); what that bit is remains **(not verified)**.
- The hand: base × 1.5 (`CHand::AddDrawing` 0x46D135). Untextured primitives: material colour × base. Chroma:
  `ALPHAREF` = the material's threshold, `GREATEREQUAL`; only modes 9 / 15 of an object with its own alpha use
  `threshold · object alpha / 255 − 5` ([Render modes](transparency-and-render-modes.md#render-modes-and-materials-render_modes)).
- Inside the temple `Temple::Draw` sets the light to (−1000, 1000, −500) for the whole of its draw, the hand's
  included, and puts the old one back after it ([the temple's light](../interface/temple-interior.md#the-depth-fog-and-the-temples-light));
  openblack: `model_light::InsideTemple` under a `ScopedLight` in `Renderer::DrawTemplePass`.
- Static shadows do **not** follow this light: `fn_008721A0` (0x8721E1) and `fn_0080ECB0` (0x80EDA8) read [0xEA1C88], the
  fixed sun, so at night they keep the daytime one (`block_texture::k_ShadowSun`).
- `vs_object` does the bilinear filtering and the lighting. A single system with a single API:
  `src/Graphics/ModelLight.h` (`model_light::Light/SetLight/ScopedLight`, `Ambient/ScopedAmbient`, `UpdateFrameLight` =
  `fn_005E5830`, `LightInMeshSpace` = `fn_00855340`, `Apply` for the CPU paths) and its GPU twin
  `assets/shaders/model_light.sh` (`ModelLightI`, `ModelLightFactor`, `ModelLightDiffuse`, `ModelLightLocal` and the
  uniform `u_modelLight`: xyz the light, w the ambient). It is used by `vs_object` (objects, PSys mesh atoms and the cut
  mode) and `vs_cloud` (clouds and mists). `Renderer::DrawScene` calls `UpdateFrameLight` once per frame and
  `ECS/Trees.cpp` uses `model_light::Light()`.
- `model_light::Intensity/Factor/Apply` on the CPU: the pieces of the explosions (`mesh_pieces::AppendPiece`) and the
  FragMesh (below, with `TwoSided`); the path of the primitives of `fn_00859530` (the `__ftol` variant) is still
  missing. `Renderer::DrawCloud` uses `ScopedLight` + `ScopedAmbient(k_MistAmbient)` and passes `Ambient()` to
  `u_cloud.z`.
- **Broken buildings and their fragments: per-face light, two-sided, on the CPU** (✅ faithful). `FragMesh::Draw`
  (`fn_007F7960`) calls `fn_007F7ED0(matrix, position, pass)` for each primitive; three sites call it and there are only
  two object classes: the damaged house (`Abode::Draw` 0x5160E9: the DestructionMesh +0x90, **no matrix** because its
  triangles are already in the world, and the position of its LH3DObject +0x40 + 0x38) and the fragment (`Fragment`
  +0x94: `Fragment::Draw` 0x76EC2A with `GetWorldMatrix` and its translation, and `PhysicsObject::DrawAll` 0x646E77 in
  flight with the LH3DObject's matrix +0x14 and translation +0x38). What `fn_007F7ED0` does, pass 0:
  - **Light**: `L = (light [0xEA9E90] − position)` axis by axis, times `InverseSquareRoot((y·y + z·z) + x·x)`
    (0x7F7ED9..0x7F7F5A), in the **world**. `InverseSquareRoot` 0x841170 is a 128-byte table
    (`MakeInverseSqrtLookupTable` 0x8411D0) and one Newton step `((3 − (x·y)·y)·y)·0.5`, a little below the exact
    value: `gutils::ModelInverseSquareRoot`.
  - **Base colour, one per draw**: `fn_00801C90` at the position (0x7F7F5F, the bilinear land light), times the tint
    +0x10 `(c·t) >> 8` in the 4 channels and the specular + the +0x14 saturated in the 4, except with the pair
    0xFFFFFFFF / 0 (0x7F7F64..0x7F806E; the flag [0xC371B4] is 1 in .data and nothing writes it); then the haze
    `fn_007FEB30` at the position (0x7F807D). Abode::Draw sets the pair every time: with a fire the charring grey
    `fn_00730570` and the glow 0x730480 (both with alpha 0xFF), without a fire 0xFFFFFFFF / 0 (0x5160A6..0x5160D8); the
    fragment keeps the one of the constructor `fn_007F6EE0` (0xFFFFFFFF / 0).
  - **Per triangle** (0x7F809B..0x7F869B): with a matrix, each vertex `((z·r2 + y·r1) + r0·x) + t`; the **face** normal
    `n = (v1 − v0) × (v2 − v0)` normalised with `InverseSquareRoot((x·x + y·y) + z·z)`; **one single**
    `I = fistp(255·((l.z·n.z + l.y·n.y) + l.x·n.x))` (0x7F82A8); the front gets the factor of `I`, the back that of `−I`
    (`neg` 0x7F82AF, the two ambient branches 0x7F82B1..0x7F82EC: the rule of `Factor`) and each channel `(c·f) >> 8`
    with the colour's alpha (0x7F82EF..0x7F8363): `model_light::TwoSided`. So a lit face has its back at the bare
    ambient.
  - **Geometry**: the face in front (v0, v1, v2); the copy 0.45 ([0x8C7C78]) behind along −n, drawn (b2, b1, b0)
    ([0xC371AC] = 1); and with [0xC371A8] = 1, on each edge k with no neighbour (+0x40 + 4k = −1) the wall (fk, bk, fk+1),
    (fk+1, bk, bk+1) **on those same vertices**, so the wall carries the front colour on top and the back colour below.
    Both copies carry the triangle's uv and the draw's specular. Everything goes to `fn_0081C780` every 256 vertices or
    triangles: the indexed sibling of `Draw3DWorldTriangle`, with `g_world_to_clipping` (vertices in the world), the
    material's cull (+5 bit 0; without it, a backface test on the screen 0x81CBC7..0x81CC11 and CULLMODE CCW) and which
    **does copy the specular** of each vertex (0x81C9B9..0x81C9D0).
  - Pass 1 (0x7F7A0F..0x7F7CC6) is the snow on top (texture [0xEDD394], map [0xEDC344]): not ported (see
    [pending](../physics/physics.md#pending)).
  - `BuildingDamageSystem::AppendDamageMeshes` gathers the broken houses and the fragments every frame (a flying
    fragment at its `PhysicsDrawPose`, the pose between its last two turns that fn_00646FE0 puts in its Game3DObject
    matrix, as the instances) and `Renderer::DrawPass` sends them to the main view with the `WorldTriangles` program
    (vs_world_triangles + fs_object). The generated L3D mesh (`damage::Mesh::BuildMesh`) stays for the other views
    (reflections), the picking and the bounds: its FragMesh sub-meshes are marked `cpuDrawn` and `Renderer::DrawSubMesh`
    skips them in Main and MainBlended, also in the projected shadows over the object (`DrawShadowsOnObject`):
    `fn_007F7ED0` is no LH3DObject draw and the shadow loop of `fn_0080DB30` does not go through it **🔍 inferred**. The
    rebuilt part of a damaged house (that of `DrawBuilding`) still goes through the object program.
    `test_damage_mesh_light` compares `TwoSided` with the instructions 0x7F82AA..0x7F8363 emulated (200 000 cases), the
    root with values of the table and the vertices of one triangle with and without a matrix.
- **Trap**: `vs_object` is also used by the sky (`fs_sky`); adding a new varying to it leaves the sky white. The
  specular travels in `v_texcoord0.zw` and `v_position.w` (after computing `gl_Position`).
- The point light's focus is the player hand's position (`CHand::position`, the interface hand +0x78, 0x5E5848), lifted to at least 10 over the land under it (0x5E58AC..0x5E58C6).

### LH3DColor arithmetic

**Faithful**. An LH3DColor is a D3DCOLOR, 0xAARRGGBB. The engine combines two colours with two
families of operations, and **they all truncate, none rounds**:

- `(c·t) >> 8` per channel: `imul` on the masked byte and `shr 8`. With t = 0xFF each channel loses 1 (0xFF → 0xFE).
- `c·l / 255` per channel: the 0x80808081 trick (`sar 7` plus the sign bit, or `mul` and `shr 7`), which gives exactly
  trunc(x/255) for x from 0 to 65025.

Each routine has its own rule for alpha. When alpha is **preserved**, it is always that of the **first**
argument:

| Routine | What it does | Alpha | openblack |
|---|---|---|---|
| `fn_0080BF10` diffuse (0x80BFA3..0x80C00B) | `(c·t)>>8` → +0x4C | multiplied (0x80BFC5..0x80BFD3) | `MultiplyArgbShift8` |
| `fn_0080BF10` specular (0x80BF1B..0x80BFB9) | `min(a+b, 255)` → +0x50 | added and saturated (`cmp 0xFF`/`jb`) | `AddArgbSaturated` |
| `fn_00809D80` (only 0x80A290: object colour × part) | `(a·b)>>8` | that of a (0x809DCF) | `MultiplyRgbShift8KeepAlpha` |
| `fn_00809DE0` (0x80A2A6: object specular + part) | `min(a+b, 255)` | that of a (0x809E46) | `AddRgbSaturatedKeepAlpha` |
| `fn_0084BA90` 0x84BBEA, `Tree::Draw` 0x74B077, `fn_0074B3A0` | `(c·k)>>8` by a scalar | that of the object (0x74B0BD, 0x74B4C9) | `ScaleRgbShift8KeepAlpha` |
| `fn_007ACF70` (0x7ACF79..0x7AD03C) | `trunc(a·b/255)` | multiplied | `MultiplyArgb` |
| `LH3DMist` 0x7FA6C8..0x7FA75F | colour × light `/255` | that of the colour, not the light (0x7FA753) | `MultiplyRgbKeepAlpha` |
| `LH3DCreature::DrawNow` 0x48EF00..0x48EF98 | body × object `/255` | 0xFF (0x48EF8F) | `MultiplyRgbOpaque` |
| `fn_005E25C0` 0x5E2729..0x5E273B | cloud edge × alpha `/255` | — (scalar) | `MultiplyBytes` |

Details read from the binary:

- A field: `Field::Draw` calls `fn_0080BEC0` with the field's colour (BlendColor, alpha 0xFF at 0x528510), so its
  final alpha is (0xFF·0xFF)>>8 = **254**. With fire (0x528809..0x528862) the tint is first that colour times the
  charred grey of `fn_00730570`, on all 4 channels (= `MultiplyArgbShift8`).
- The one-shot ball (0x518DDA and 0x519002): tint `([0xBE8E8C] & 0xFF) << 24 | 0xFFFFFF` with [0xBE8E8C] = 0x00010196
  (no writer), so the alpha is (0xFF·0x96)>>8 = 0x95.
- `SpellWolf::Draw`: +0x4C = `fistp(alpha) << 24 | 0xFFFFFF` (0x51C701..0x51C714) and translucency is decided with that
  raw alpha (0x51C71E..0x51C727); when burning, +0x4C × charred on all 4 channels (0x51C751..0x51C7B7) and
  `fn_0080BEC0` with the glow `fn_00730480`.
- **`Tree::Draw` does not call `fn_0080BEC0`**: its +0x4C is `fn_00802120` (0x74AB1B) plus the fog `fn_007FEB30`
  (0x74AB60), and then the brightness [0xC22FA0] by `(c·k)>>8` with alpha preserved (0x74B077..0x74B0C4). The fog goes
  **before** the brightness, the other way round from `vs_object`.
- The burning tree (`fn_0074B3A0`): grey 50, or `ftol(255 − (1 − life)·2550)` with a minimum of 50 if life > 0.9, and
  then unsigned `min(gris, [0xC22FA0])` (`jb`, 0x74B47B..0x74B484); alpha **is preserved** (0x74B4C9, confirmed).
- The charred grey `fn_00730570` is `255 − ceil(175k/256)` (0x730585..0x7305D7): k = 255 gives 80.
- `fn_0080BF10`'s specular compares each of the 4 channels with `cmp 0xFF` / `jb` at 0x80BF5B..0x80BF98; `fn_00809DE0`
  saturates R, G, B with `jle` at 0x809E01, 0x809E19, 0x809E35.
- LH3DMist 0x7FA6C8..0x7FA75F multiplies the mist's colour (+0x4C) by the land light of `fn_00801C90` **after** the
  haze `fn_007FEB30` (channels at 0x7FA6DF, 0x7FA711, 0x7FA740).
- `fn_0080BF10` with t = 0xFFFFFFFF is called by `TownCentre::Draw` (0x5164AD), by the villagers with a specular
  (0x51B424) and by `PhysicalShield` (0x72D0D4): the land light loses 1 in each channel.
- The one-shot ball's support PSys takes the orb's 0x95 alpha (`DrawSpellGraphic` 0x51A252 → SetAlpha 0x55ED50 →
  [0xC0215C], applied in `fn_00679920` 0x679BC2), and the player's seed is lit with the land cell (0x803340); the
  bubble is the cyan texture × the grass light.

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x70911A, fn_007F98E0, 0x7F9900, LH3DObject::AddDrawing 0x815B95..0x815BEC` | feature 19: LH3DObject (+0x40) SetDisappear(0) (vt +0x98 clears obj+4 bit 0x8; IsDisappear vt +0x9C); AddDrawing fades a Disappear object with the distance (obj+0x4C alpha = 255 x part left before g_last_distance [0xEA1AF4], then SetDrawWithGlobalAlpha(1)) | `ThingJcSpecial case 19` |
| `0x84BAA3` | fn_0084BA90's untextured-primitive branch: material colour x object colour (with fn_007ACF70) | `fs_object.sc (u_materialColour.w > 0)` |
| `0x82E181` | where render modes 9 / 15 take ALPHAREF from the table 0xC387C8 (material alpha ref scaled by the object's alpha, - 5) | `fs_object.sc alpha test, render_modes::AlphaRef` |
| `0x48EF10` | LH3DCreature::DrawNow: trunc(c l / 255) per channel via the 0x80808081 multiply | `argb_colour.sh MultiplyDiv255` |
| `0x84BB90..0x84BC1D` | fn_0084BA90's per-vertex model light loop (on the CPU over D3DTLVERTEX) | `model_light.sh ModelLightI / Factor / Diffuse` |
| `0x84BBB5` | fn_0084BA90: fstp of 255 (n . l) to a float32 before the fistp (not copied) | `model_light.sh ModelLightI` |
| `[0xE9FE48]` | bone matrices used by the boned model-light path and by fn_00858BA0 (0x858F77) for positions only | `model_light.sh ModelLightLocal, vs_object.sc cut branch` |
| `0x7F80AE..0x7F8116` | FragMesh fn_007F7ED0: transforms its world-triangle vertices itself | `vs_world_triangles.sc` |
| `0x81C9BE..0x81C9C4` | fn_0081C780 (indexed Draw3DWorldTriangle): copies each vertex's specular (the plain one writes 0, 0x81C2BF) | `vs_world_triangles.sc a_color1` |

## openblack

Status: **done** for the model light (`src/Graphics/ModelLight.h` and `assets/shaders/model_light.sh`), the colour
arithmetic and the per-instance colours; the copies listed in [Pending](#pending) are still to be unified. Progress:
[Models and lighting](../project/progress/rendering/models_and_lighting.md),
[Shadows](../project/progress/rendering/shadows.md).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Model lighting**
  - openblack: `model_light::AnimatedIntensity` and `AnimatedLightIntensity` (model_light.sh) for the advisors'
    bodies (`USE_ANIMATED_OBJECT_LIGHT`)
  - openblack: `sky_type::At(visual hour)`) it puts it 3 units ([0x8C2C50]) from the **hand** towards the camera,
    with the hand raised to at least 10 ([0x8AB414]) above the terrain beneath it.
  - openblack: `components::Unlit` on the Tree, DeadTree, BigForest and Flowers archetypes (a dead tree made from a
    tree keeps it) and on the SuperVillagers' eyes (`ECS/SuperVillagerEyes.cpp`), carried per instance in the w flags (`argb_colour::PackInstanceUnlit`,
    [below](#the-object-colour-fields-in-the-instance))
  - openblack: `LandIsland::CreateCellMap` (RGBA texture per cell: rgb = colour read as D3DCOLOR, a = brightness)
  - openblack: `damage::Mesh::LightDirection`, `ObjectLight` (base colour), `AppendTriangle` (one triangle) and
    `AppendDraw` (one primitive per batch, in its source material `Primitive::source`), in `world_triangles::Vertex`
    with `specular` (Color1, which `vs_world_triangles` passes to `fs_object`)
- **Model lighting**
  - **⚠️ approximate** openblack draws them after all the models and not in the order of the object loop; the blended
    primitives go in the Main view (as the pieces of the explosions), not in MainBlended. The FragMesh reflections
    keep the generated mesh and the object program's light (vertex light in mesh space, the same rule; the
    original's reflection of it is not checked).

### The colour arithmetic in openblack

`src/Graphics/ArgbColour.h` (`argb_colour::`; stateless, all
`constexpr`), with the alpha rule in the name (`Argb`, `Rgb…KeepAlpha`, `RgbOpaque`), plus `Argb`, `Red/Green/Blue/Alpha` and the bgfx conversions with no original
(`ToAbgr(argb)`, `ToAbgr(argb, alpha)`, `ToAbgr(vec4)` rounding and `ToVec4/ToVec3` = byte/255). The GPU twin is
`assets/shaders/argb_colour.sh` (`MultiplyShift8`, `ScaleShift8`, `AddSaturated`, `MultiplyDiv255`, `UnpackRgb24`), included by
`vs_object.sc` (the instance's colour column, the cut colour and the unlit colour from `u_objectLight.z`).
`MultiplyDiv255` is floor((c·l + 0.5)/255): division in a shader does not round well (often x·rcp(255)) and a plain
floor(x/255) can give k − 1 when x = 255k; with the + 0.5 the fraction stays within [0.002, 0.998]
(approximate until tested on a GPU). `ScaleRgbShift8KeepAlpha` works for any k: the masks come after each `imul`
(0x74B099 / 0x74B09F / 0x74B0B2), so the channels do not overwrite each other; the callers pass 0..255. `fn_0080BEC0` is
"draw with the terrain colour" for the PSys property `DrawWithLandscapeColor` (`Particle3DObj::DrawAt`
0x67A00C); symbols.txt calls it `GetPoisonColor@Pot`, and the name is ours 🔍 inferred. Model
lighting (`model_light::Apply`), the cloud colour (`Clouds::Colour`, 0x5E1ECE..0x5E1F24), the mists
(`RendererMists`), the one-shot ball and the conversions in `Renderer`, `RendererBoat`, `RendererSea`,
`RendererSmoke`, `Dust`, `GameFont` and `ScriptFade` already use it, with no visible change. `test_argb_colour` compares
`MultiplyArgbShift8` with an instruction-by-instruction emulation of 0x80BFA3..0x80C00B and `MultiplyBytes` with the two forms of
0x80808081 for all products of two bytes.

### The object colour fields in the instance

**openblack's own transport** (no original): the engine stores the colours in the LH3DObject (obj+0x4C diffuse,
+0x50 specular, +0x54 windows) and the CPU uses them when lighting; openblack carries them per instance to `vs_object`. Each
instance has **five columns** (80 bytes, `i_data0..i_data4`): the matrix and a fifth one with one float per field, each
an integer of at most 2^24 that the float holds exactly. It is written by `argb_colour::PackInstance*`
(`src/Graphics/ArgbColour.h`) into `RenderContext::instanceColours`, and `RenderingSystemCommon::UploadInstances` interleaves
the two lists in the buffer.

| Field | Value | What it is |
|---|---|---|
| x (+0x4C) | 0 | the land light alone (`fn_00801C90` without `fn_0080BF10`) |
| | −1 − rgb (`PackInstanceTint`) | a tint t that multiplies the land light (`(c·t)>>8`): that of `fn_0080BF10`, or `Tree::Draw`'s own (see w) |
| | 1 + rgb (`PackInstanceColour`) | the colour from `SetColorSpecular` 0x7F9770 (vt 0x2C), instead of the land light |
| y (+0x50) | rgb (`PackInstanceSpecular`) | the specular, 8 bits per channel, added with saturation to the land one (0x80BF1B..0x80BFB9) |
| z (+0x54) | 0 or 1 + rgb (`PackInstanceWindow`) | the window colour from `Abode::Draw` (vt 0x30, 0x516068); 0 = off |
| w | flags, 0 to 3: + 1 (`PackInstanceTreeTint`), + 2 (`PackInstanceUnlit`) | + 1 = the tint goes after the fog: `Tree::Draw` does not call `fn_0080BF10`, it lights with `fn_00802120` (0x74AB1B), applies the fog (0x74AB60) and then multiplies the +0x4C (0x74B077..0x74B0C4; when burning, `fn_0074B3A0` 0x74B48F..0x74B4D3). + 2 = not lit by the model light (`IsDynamicLighting` false, [Unlit objects](#model-lighting)): the trees, dead trees, big forests, flowers and the SuperVillagers' eyes (`components::Unlit`) |

This way colour and specular go together (before, they shared the w of the fourth column and the last one won). Who passes what
(`DrawColoursOf` in `RenderingSystem.cpp`, read in each Draw):

| Object | Tint | Specular | Address |
|---|---|---|---|
| Villager | burning: charred grey; if +0xD0 ≠ 0: white 0xFFFFFFFF; poisoned: 0xFFE8FFDD; otherwise, nothing | fire glow / +0xD0 / 0xFF001000 | `fn_0051B3D0` 0x51B402..0x51B488 |
| Animal | burning: charred; +0xD0 ≠ 0: white; otherwise, nothing (does not check poison) | glow / +0xD0 | `Animal::Draw` 0x51C4C6..0x51C51C |
| Miracle wolf | always white (+0x4C = alpha << 24 \| 0xFFFFFF); burning, × charred on all 4 channels | glow / +0xD0 | 0x51C709..0x51C7E1 |
| Poisoned pot or pile without fire | 0xFFE8FFDD | 0xFF001000 | `Pot::Draw` 0x51BB8F..0x51BBA3, `PileFood::Draw` 0x51C191..0x51C1B8 |
| Town centre miracle icon (`TownCentreSpellIcon`) | white, always (with centre life > 0) | +0x10C (`SetSpecularColor` 0x55D380; not ported: 0) | `TownCentre::Draw` 0x5164A6..0x5164B2 |
| Worship site miracle icon | white only if +0x10C ≠ 0 (0x519672..0x51967C, 0x5198A8); since +0x10C is not ported, nothing | 0 | `SpellIcon::Draw` 0x519650 |
| Physical shield | white | 0 | `PhysicalShield::DrawShield` 0x72D0D4 |
| One-shot ball | white (its alpha goes in `components::Alpha`) | 0 | 0x519002..0x51901E |
| Field | its colour; burning, × charred (`MultiplyArgbShift8`) | 0 / glow | `Field::Draw` 0x528809..0x52888A |
| Tree | brightness [0xC22FA0]; burning, `TreeDrawColour`; both after the fog (w = 1) | 0 | `Tree::Draw` 0x74B077..0x74B0C4, `fn_0074B3A0` 0x74B48F..0x74B4D3 |
| Anything else with fire (buildings, rocks, dead trees, totems...) | charred grey `fn_00730570` | glow `fn_00730480` | `fn_00518050` (11 callers) and `DrawBuilding` 0x517FD4 |
| Power bands | player colour (`SetColorSpecular`) | 0x141414 ([0xBE8EA0] = 20) | `DrawSpellGraphic` 0x51A370..0x51A3BE; the hand's one writes the fields directly (`PHandFX` Band::Draw +0x4C 0x68D87D / 0x68D8AB, +0x50 0x68D8B1; 🔍 inferred without `fn_00801C90` behind it) |
| PSys mesh atom | DrawData+8 (tint with `DrawWithLandscapeColor`, otherwise `SetColorSpecular`) | DrawData+0xC, the atom's specular, given with the colour on both paths (read at 0x67A012 and 0x67A023) | `Particle3DObj::DrawAt` 0x67A00C..0x67A02F |

The white tint subtracts 1 from each channel (`(c·255)>>8`): before, it was not applied. The tint's alpha is not carried. The
LH3DObject draw copies the whole +0x4C to [0xC37D8C] (0x80DEF8; the +0x50 to [0xE9FE2C], 0x80DEFE) for all objects,
and several routines read it (`fn_007A4170` 0x7A6A2C, 0x7A7E85, `fn_00805CD0` 0x805EAA, `fn_00809E50`); that its alpha only
counts for the ones that fade out is **🔍 inferred** (those reads were not followed through to the vertex colour). For
those openblack uses `components::Alpha` (approximate: the shield should end up at 0xFE and the wolf at (A·0xFF)>>8).
`test_argb_colour` checks the extremes (−2^24, 2^24, black, the black window switched on) and 200 000 random colours
decoded as in the shader.

- Draws that leave the land light alone in +0x4C (`fn_00801C90` without `fn_0080BF10`): e.g. `MultiMapFixed::Draw`
  0x5180CF, `Animal::Draw` 0x51C4F0.
- +0x54 is written by vt 0x30 = `fn_007F9780` (`mov [ecx + 0x54], edx`), from `Abode::Draw` 0x516068; 0 when the
  windows are not lit (0x51606D..0x516073).
- The window colour (`Abode::Draw` 0x515F70): a grey 0xFFgggggg (0x516054..0x516064), scaled below 256 by the
  intensity, (g·k) >> 8 (0x516044..0x51604F); it flickers with the table 0x8D86D0 = {0, 7, 3, 5, 4, 2, 6, 1} and has a
  per-house time offset from the position. The window sub-meshes are drawn unlit in that flat colour.

**⚠️ approximate** The +0xD0 specular of living beings (`Living::GetSpecularColor` 0x417490): the original checks the whole dword with its alpha (0x51B416 /
0x51C4D6 `test eax,eax`), and the healing chakra writes alpha 0xFF (`fn_006A0E30` 0x6A0EF5), so its frames
with rgb 0 keep the white tint; openblack removes `SpecularColour` with rgb 0 (`Particles/Rules/Heal.cpp`) and those
frames go with the land light alone.

**🔍 inferred** Every instance class that can burn goes through `fn_00518050` or `DrawBuilding`, or carries the same
pair inline (the wolf 0x51C751; that of the house's FragMesh, 0x5160AF, now goes in `BuildingDamageSystem::AppendDamageMeshes`);
still to be ported are the inline pairs of `Object::DrawOutOfMap` (0x51C839) and of the physics prediction object
(0x646F8C) (see Pending).

### Code map

The source files of this page and the symbols their notes describe:

- `src/ECS/Systems/Implementations/RenderingSystem.cpp`:
  - `DrawColoursOf` picks each class's tint and specular as the table above says: the `SpecularColour` component
    (Living +0xD0), the town centre's icons (`TownCentreSpellIcon`, always white), the spell wolf
    (`AnimalInfo::SpellWolf`: alpha << 24 | 0xFFFFFF, times the charring grey when it burns);
  - `RenderingSystem::WriteEntityRow` writes an entity's instance row; a tree's roots (`TreeRoots::factor`, 0.15 × the
    tree mesh's extent, in `ECS/Components/TreeRoots.h`) scale the tree's drawn rotation and keep its translation,
    as `fn_00511270` does ([trees](../world/trees.md)); a zero matrix still writes its row;
  - `RenderingSystem::PrepareDrawUploadUniforms` gathers the PSys mesh atoms with their sort key (the object's
    translation, `fn_00679F60`) and their colour (DrawData +8, the land tint with `DrawWithLandscapeColor`, else
    `SetColorSpecular`'s colour) and specular (DrawData +0xC, `particles::mesh_atoms::Instance::specular`);
  - `AtomDrawnCut`: a cut atom that also asks for the land colour is drawn as a plain cut atom (the land tint under
    the cut's own light is not ported; no effect is known to set both);
  - `ReceivesDynamicShadow` keeps the original's receivers of the projected shadows, leaving out what turns the bit
    off, among them the one-shot orb and the dispensers' bubble (`OneOffSpellSeed::CallVirtualFunctionsForCreation`
    0x72A4B4; [the shadows on objects](shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects)).
- `src/Graphics/ArgbColour.h`: `PackRgb24`.
- `src/Physics/DamageMesh.h`: `damage::Mesh::AppendDraw`.
- `test/test_argb_colour.cpp`: `EmulateDiffuse`, `EmulateSignedDiv255`, `EmulateUnsignedDiv255` (instruction-by-instruction emulations).
- `test/test_fire.cpp`: `OriginalRed`, `OriginalGreen` and `OriginalBlue` compute the charring grey's bytes as the
  exe does (red 0x7305BB..0x7305D7, green 0x730585..0x7305A2, blue 0x7305A4..0x7305B9), against which the port is
  pinned.
- `test/test_damage_mesh_light.cpp`: `EmulateFace`, against `DamageMesh::AppendTriangle` (front, back 0.45 behind, the walls of the open edges).

### Test hooks
The model rendering pages' own variables; the full list is in [Debug environment variables](../project/debugging.md#debug-environment-variables).

- `OPENBLACK_MOUSE_AT="fx,fy"`: cursor fixed at a fraction of the window (the hand shows up in captures); use interior
  points, on the edge it moves the camera.
- `OPENBLACK_TEST_PHYSICS="x,z,height,vx,vy,vz[,scale[,n]]"`: falling rocks, with their physics shadows.
- `OPENBLACK_TEST_CUT=1` with `OPENBLACK_TEST_SEA`: cut by the water plane.
- `OPENBLACK_HAND_TEST_FISH=1` and `OPENBLACK_TEST_SPLASH="x,z"`: fishing and scaring the fish.
- `OPENBLACK_TEST_CHIMNEY=all`: all chimneys smoke.
- Billboards: `OPENBLACK_TEST_SEED=FIREBALL` / `LIGHTNING_BOLT` with `OPENBLACK_MOUSE_AT` (PSys sprites in the hand:
  origin, CentreAtBase and rotation); `OPENBLACK_TEST_FIRE` (flames); `OPENBLACK_TIME_OF_DAY=22` with `OPENBLACK_CAMERA_LOCK`
  (the moon, centred and at an edge); `OPENBLACK_TEST_ONESHOT` (the bubble).
- Animated textures: `OPENBLACK_TEST_ONESHOT="<seed>,x,z[,pu]"` (the bubble; with pu, the bands that face the
  camera; with a creature vial, its 8×4 sheet), `OPENBLACK_TEST_DISPENSER`, `OPENBLACK_TEST_FIRE` (flames),
  `OPENBLACK_HAND_TEST_FISH=1` and `OPENBLACK_TEST_SPLASH` (fish and rings), `OPENBLACK_TEST_SEED=LIGHTNING_BOLT`
  (chains), `OPENBLACK_TIME_OF_DAY=22` (lanterns), `OPENBLACK_TEST_WEATHER` (storm puffs),
  `OPENBLACK_TEST_CHIMNEY=all` (smoke).
- Transparent ordering and bubble: `OPENBLACK_ORB_TRACE=1` writes per frame the place and key of each
  `ZR_SurfRevol` disc and of each one-shot ball bubble in the sorted list, with the phase, the frame, the packed
  `[1][3]`, the alpha and the clipping of each bubble (see
  [debug environment variables](../project/debugging.md#debug-environment-variables)). Scene: a dispenser with
  `OPENBLACK_TEST_DISPENSER` and the camera fixed with `OPENBLACK_CAMERA_LOCK`.
- Transparent queue: `OPENBLACK_ZSORTER_TRACE=1` writes once per second how many entries the queue has of
  each class (models, clouds, rain tiles, boat sprites, effects, surfaces, ribbons, mists, smoke,
  sprites, hand), those lost to the cap and the extreme keys. Scenes: the Land 1 sky with
  `OPENBLACK_CLOUD_SEED=7`, a storm with `OPENBLACK_TEST_WEATHER="x,z,60"`, the boat with `OPENBLACK_BOAT_TRACE=1`
  and the hand with `OPENBLACK_MOUSE_AT`.
- Meshes stuck to the ground: `OPENBLACK_TEST_SPELL="PHYSICAL_SHIELD,x,z,..."` with `OPENBLACK_TEST_SHIELD_SHOT` (the physical
  shield melts into the land), `OPENBLACK_TEST_DISPENSER` and `OPENBLACK_TEST_TELEPORT` (the cut discs),
  `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` and `OPENBLACK_TEST_EXPLOSION_SHOT` (the ground mark; `UR_Explosion` is only
  in `SF_BeamExplosion*`), `OPENBLACK_TEST_TUG`
  (the crater); `OPENBLACK_SPELL_TRACE=1` writes `Explosion: ground mark`.
- Render modes: the test `test_render_modes` (the tables, the states of each site before and after and the fixes);
  scenes with `OPENBLACK_HAND_TEST_TREE` and `OPENBLACK_CAMERA_LOCK` (chroma edges), `OPENBLACK_TEST_ONESHOT` and
  `OPENBLACK_TEST_DISPENSER` (fades and additives), `OPENBLACK_MOUSE_AT` (hand shadow on objects).

## Pending

- The model level of detail is settled in the v1.42 exe; see
  [The model level of detail in v1.42](visible-blocks-and-lod.md#the-model-level-of-detail-in-v142).
- Unlit objects (`components::Unlit`): (not verified) how the original draws the snow lying on an unlit object
  (`DrawSnow`, slots 0x124/0x128); `vs_object` still lights the snow layer with the model light. (not verified)
  whether the plane cut `DrawCutByPlane` (`fn_00858BA0`), which lights with the light in object space, also tests
  `IsDynamicLighting`; an unlit tree cut by the water is still lit there. Whether [0xC38224] can be 0, which would
  make every object unlit, from a table indexed by a detail level (`fn_00823AD0` 0x823C56). The creed: the original
  makes it unlit and casting no dynamic shadow (`Creed::Create3DObject`, `SetDynamicLighting(0)` 0x50B464);
  openblack makes creeds by their row's kind (`ecs::physics::script_create`) with no `components::Unlit`, so they are
  lit (to fix in code).
- FragMesh draw: `Abode::Draw`'s on-screen gate `CheckRegionOnScreen` 0x51609D is not ported (it only saves time);
  the fire tint of a burning broken building is taken from `Burning()` (not verified against 0x5160A6); (not
  verified) whether leaving the CPU-drawn sub-meshes out of RenderPass::Main / MainBlended also drops a broken
  building from the Main view's special draws (the sea cut `DrawCutByPlane` and the under-water pass of a held or
  flying object), which the original draws through the object's own LH3DObject (to check with captures).
- Model lighting, copies still to be unified with `model_light`:
  - `RendererRevolvedSurface.cpp`: the GJ mesh goes unlit (`UseLighting` not ported; that it is active is **🔍 inferred**).
- LH3DColor arithmetic, what is still to be moved to `argb_colour`:
  - after the instance repacking (the `u_objectLight` transport already goes through `sea_pass::SeaDraw` and
    `UnpackRgb24` in modes 2 and 4): that `DrawBuilding` never applies fog, burning or not (0x517F90..0x518046 does
    not call `fn_007FEB30`: Abode 0x516129, MultiMapFixed 0x5180A6, WorshipSite 0x5193E9, SpellIcon 0x519668, Totem
    0x51ABC3; the "fix 7" of `LandLightOf`); and the colours that still differ:
    - the tint's alpha: the original's tint carries (0xFF·a)>>8 for the spell wolf, its translucency decided with the
      raw alpha (0x51C724), and 0xFE for the shield; openblack's tint carries no alpha, the wolf's goes as alpha / 255
      in float (`SpellFlock.cpp`) (queued fix);
    - the icons' +0x10C specular: the original gives the icons their specular, and with it the white tint of the
      worship site icons (0x519672); openblack's icons have none, so no white tint (queued fix);
    - 🔍 inferred the town centre's white tint is the write the frame keeps: `TownCentreSpellIcon::Draw` 0x519640 is a
      jump to `SpellIcon::Draw`, and whether it also runs for the slot icons in the same frame (and after
      `TownCentre::Draw`) was not read;
    - `LandLightOf` (through `land_light::ObjectLightOf`, `src/3D/LandLight.cpp`) gives every `SpellIcon` {Cell, no fog}, but the original lights those of a town centre
      (`TownCentre::Draw` 0x5164B2 → `fn_0080BEC0`) with the bilinear light and fog (queued fix);
    - the inline colours: the original draws the burning physics prediction object white with a glow
      (`PhysicsObject::DrawAll` 0x646F81..0x646F8C), `Object::DrawOutOfMap` with its own colour (0x51C837..0x51C84F)
      and `CitadelHeart::DrawNow` with its tint +0xA4 and specular vt 0x5A4 (0x4670DD..0x4670EE); openblack applies
      none of them, each its own fix (queued fix);
    - the +0xD0 specular: the original tests it with its alpha, so the heal chakra's fade frames with rgb 0 still
      take the white tint; openblack drops the specular at rgb 0 and draws them with the land light alone (see above,
      `Heal.cpp`) (queued fix);
  - elsewhere in the code: the copies in `src/Particles` (Mist 0x67A6C1, `TintWithPlayerColour` 0x6A865C, Storm
    0x6D2C21, SurfRevol, Heal), `NightLights`, `LandLightTable` and `RendererChain` / `RendererParticles` (`ToAbgr`);
  - `fn_00809D80` / `fn_00809DE0` (colour per mesh part, 0x80A290 / 0x80A2A6) and `LH3DCreature::DrawNow` have no
    user in openblack yet; `fn_007ACF70` exists only on the GPU (`fs_object.sc`, in float without truncation).
- Questions for the user (implemented "as in the original" as read from the binary; they depend only on how the game
  looked):
  - **Model lighting at night** (`model_light`): with sky type > 1.5 (double at 0x8C5838) the light is placed 3
    units from the hand, on the camera side (fn_005E5830 0x5E5A7D..0x5E5B64). Does it look like what you remember at
    night?
