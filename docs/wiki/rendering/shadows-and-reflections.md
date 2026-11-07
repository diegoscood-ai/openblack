# Shadows and reflections

## Summary

The original has three shadow systems: static shadows baked into the land's block textures (sheared by a fixed sun),
projected shadows (a list of 32×32 silhouettes rasterised on the CPU and drawn over every land block they touch and,
for the hand, the creature and the boat, over objects), and the ground blobs of villagers and animals. The sea shows
reflections: the hand, what it holds, the physics objects, the boats and the local creature's body are drawn mirrored
in y = 0, unlit, clipped to what is above the water.

openblack draws all three systems and the reflections as the original does, the hand's shadow compared against the
user's observation of the original; the creature is a complex shadow caster in half rows.

## The original

### Object reflections and hand shadow on objects

**Faithful** (done). The common code is in
[The under-sea pass](../world/sea-coast-and-rivers.md#the-under-sea-pass).
- **DrawUnderWater** (static 0x811010 → `fn_00850FC0` per primitive; animated 0x810E20; complex 0x813300): world =
  object × vertex, clip = W2C·(x, −y, z), index order reversed, plane (0, 1, 0, 0) that removes what had y < 0.
  Diffuse = obj+0x4C and specular = obj+0x50, **unlit**. The alternative mode table 0xC387C8 only with Flags1 & 0x80
  (the hand does not use it: the 0x65 alpha has no effect with its mode 4 material).
  - Hand: skipped if hand+0xAC; 0x65A0A0A0; then what it holds (hand+0x8C) **with its own colour**.
  - Creature (0x5E49E1..0x5E4B26): only the local player's (`players[PlayerIndex].creature`, no loop), its body's
    complex object (physical +0x160 → +0x58 → +0x482C). Cells x, z = ftol(translation × 0.1) inside 0..0x1FF (a fixed
    511, not the land's limit), the block there (`g_index_block`, `g_ptr_blocks`) existing, its bit +0x920 & 1 set (this
    frame's PreDraw view test: the land clip writes it only later) and its distance +0x9BC not past VanishObjectDist
    [0xC37200] (100000), then y (+0x3C) < 6 and `FizzAmount` (+0xA0, the drawn fizz) < 0.2; a NaN passes each compare.
    Then `SetColorSpecular(0x65A0A0D0, 0x30)`, dynamic lighting off, `DrawUnderWater` (vt +0x118, `fn_00813300`: the
    bone palette = its posed bones +0x80, then `fn_00811010`), dynamic lighting on (unconditionally) and the colour back
    to 0xFFFFFFFF / 0. Every failed test still goes on to 0x5E4B04 (the tree-bend slot 2). It draws the body in its
    drawn pose and shape with its own materials (painted skins), and nothing it carries, no eyes, hair, glows, fire,
    fizz or freeze. VanishObjectDist is fixed at 100000, as the shadows' (the script's `SET_VANISH_OBJECT_DIST` is not
    ported). The View menu's "Creature Reflection" switch turns it off.
  - Physics objects (`fn_00646FE0`, array 0xD47814, stride 0x1DC): the awake ones only (the asleep byte +0x19C,
    0x647004..0x647011: no resting proxy, so never a broken house), if y > −r (r = maximum distance from a vertex to the
    centre of mass), with no distance limit. A fragment mirrors its own LH3DObject, the Rock of info 0xD3A930 (0x76E9EC)
    in the ctor colour 0xFFFFFFFF / 0 (0x8164F7), not its FragMesh (🔍 inferred).
  - The "own colour" is what `fn_00801C90` left in obj+0x4C/+0x50 in its last Draw (called by `PhysicsObject::DrawAll`
    0x646F9F, `MobileObject::Draw`, `Rock::Draw`...): the bilinear land light and the cells' specular, without N·L or fog.
  - Boats (`PetitNavire::PreDraw` 0x5DFF20): **one** `DrawUnderWater` per frame of the hull in 0xFF303070 (then
    `fn_00801C90` gives it back the land light). The branches 0x5E0100-0x5E0190 (mode 0, launch: it also corrects y with
    `GetAltitude` and the shadow) and 0x5E0380-0x5E03EE (mode 1, voyage) are mutually exclusive via +0x30, both with the
    mirror diag(−1, 1, 1) on the hull's track (determinant −1) and the `RotateY(π/2)`: there is no second part. See [the
    missionaries' boat (PetitNavire)](../world/sea-life-and-scenery.md#the-missionaries-boat-petitnavire). It is the
    under-sea stage's first draw: `PetitNavire::PreDraw` is called at 0x5E490F, before the hand (0x5E4985), what it
    holds (0x5E49A2), the physics objects (0x5E49DC) and the creature.
  - What the hand holds is mirrored only when the hand's own reflection is drawn, so not while the hand is hidden
    (`NotDrawn`, in INVISIBLE), as the original skips it with the hand while hand+0xAC is set
    (`sea_reflection::ReflectsVillager`'s rule for what is in the hand).
  - **The pose of a boned body** (animated 0x810E20, the vt+0x118 of the boned class 0x9A32A0): it is posed before its
    primitives are mirrored, with the same object fields and branches as its main draw (0x812170 0x8123B4..0x8125DE, the
    SuperVillager draw 0x825530 0x8259B0..0x825A33): blend +0x90 = 0 and no clip (+0x80 = 0) → `SetTransform` 0x810E9D
    (the skeleton at rest); blend 0 with a clip → `fn_00839980(buffer, +0x7C, +0x84, obj+0x14)` 0x810E86 (the clip +0x80
    at time +0x84); a blend → two `fn_00839BC0` (clip +0x80 / +0x84 with 1 − w, clip +0x88 / +0x8C with w, 0x810ECC,
    0x810EEE) and `FinishTransform` 0x810F02. So the reflection has the pose drawn above in the same frame, a fade
    included. Skeleton and primitives are both the object's mesh +0x7C (0x810E7B, 0x810E8F, 0x810EF3; loops 0x810F29 /
    0x810FA3), never the LOD slots +0x98/+0x9C/+0xA0 (std, not the drawn high model, xref note #45). with no pose, its
    rest bones. Before this the reflection of a held or flying villager or animal was drawn at rest.
  - **An animated object is mirrored in its current frame** (a villager held in the hand or flying, an animal): the
    animated `DrawUnderWater` 0x810E20 poses the mesh first from the same fields as its Draw `fn_00812170` (0x8121DA,
    0x812351): with blend weight +0x90 = 0 (`fcomp [0x8AA398]` 0.0, 0x810E5A..0x810E65) it is `fn_00839980` (anim +0x80,
    frame +0x84, mesh +0x7C, 0x810E86), or `LH3DAnim::SetTransform` 0x83A1D0 with no anim (0x810E9D); otherwise
    `fn_00839BC0` twice (+0x80/+0x84 at 1 − w, +0x88/+0x8C at w, 0x810ECC / 0x810EEE) and `LH3DAnim::FinishTransform`
    0x839F10 (0x810F02), then each primitive's `fn_00850FC0`. it was the mesh's bind pose before. The reflection draws
    such an instance alone with uniform bones, through the same mirrored draw (and cull) as the hand's: the bone palette
    is filled for the main view only.
- **Dynamic shadow on objects**: at the end of each Draw (static `fn_0080DB30` 0x80E457..0x80E4D7, animated
  `fn_00812170` 0x81311A..0x81317C, vt+0x15C `fn_00810720` 0x810CD6 and `fn_00817930` 0x8185AB, morphable 0x80E74B...),
  if the object has Flags1 0x40 (vt+0x7C), for each `ShadowInfo` in the list (from newest to oldest) with
  `fn_00881030`, si+0xC = 0 (the hand, the creature and the boat, 0x5E11BE; physics objects and SuperVillagers set 1: land
  only), that is not the caster (si+0x464 ≠ obj) nor the complex object's own si (vt+0x1A8 / vt+0x1B8), and whose box
  si+0x2C {x0, z0, x1, z1} touches the mesh's XZ box (centre ± half + position, without rotation or scale; vt+0x1BC =
  `fn_007F9E80`): `fn_0080B050` (mode 6 from the current table, which is already back to the normal one also on an object that
  fades, 0x80E197; colour white; `fn_0084E200` with u = (Wx − x0)/(x1 − x0), v = (Wz − z0)/(z1 − z0), `fn_00880770`:
  **vertical projection**; all the darkening goes in the texture's alpha, with the fade baked in).
  - Z test: the static one and `fn_00810720` set ZFUNC EQUAL before each shadow (0x80E484 / 0x810C8F) and LESSEQUAL
    when done (0x80E4CE / 0x810CF2); the animated one does not touch ZFUNC: the frame's LESSEQUAL remains.
    - The receiver test of a static Draw: vt+0x7C (Flags1 0x40) at 0x80E457..0x80E460; the whole filter
      0x80E46C..0x80E49F; ContainsThisBoundingBox at 0x80E497; the caster test at 0x80E47C; the complex object's own
      si (vt+0x1A8 / vt+0x1B8, 0x80E4A5..0x80E4BB) is the hand's body.
  - **Where**: inside the receiver's Draw, so immediately with an object drawn immediately (without the 0x200 flag,
    0x815F62) and in its slot in the queue with a queued one (0x7FA980 → vt+0x108); never behind the drain ([the queue](transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter)).
  - Receivers (Flags1 0x40, `Object::Create3DObject` 0x6365F0 if ShadowsOnObjects): all objects except those that call
    vt+0x78(0) (`xor edx, edx; call [eax+0x78]`): trees (0x749FA3), forests (0x439098), flowers (0x527A5D), magic
    food (0x5FAAC8), food in the hand (pot 12, 0x66D180), the creeds (0x50B46E), the town flags
    (`TownDesireFlags`, 0x746DD4), the one-shot balls (`OneOffSpellSeed`, 0x72A4B4), the shields (MagicShield
    0x72C2B4, PhysicalShield 0x72CCF4), the charge of icons and totems (`TChargingData` 0x72675F, 0x780BBB) and crops when being deleted (0x607EC5). When an
    object is picked up it is removed (`SetHeldObject` 0x816842); when it is thrown it is restored.
  - And those that do not go through `Create3DObject` do not receive: a new LH3DObject has the bit at 0 (the
    `LH3DMeshedObject` ctor sets +4 = 0x10009, 0x816537) and vt+0x78 (`fn_008168A0`) only sets it with an argument ≠ 0 and
    [0xC38220] ≠ 0. So the hand's power-up bands (`Band`, `fn_0068CA30` → `LH3DObject::Create` 0x68CA98) and the
    spell icon's one (`CreatePUBand` 0x727080 → `Game3DObject::Create` 0x63ABB0, which jumps to `LH3DObject::Create`)
    do not receive. The icon's mesh does (`fn_00727190`, vt+0x78(1) at 0x727245).
  - Correction: PSys meshes do not call vt+0x78(0), but vt+0x78 with the creator's byte +0x54 (`mov dl, [edi+0x54]`
    at 0x6A8ACE / 0x6A8D65; the same byte goes to vt+0x80). That byte is 0 in both ctors (0x6A8986, 0x6A8BDE) and
    no property writes it (`DefineProperties` 0x6B37A0 / 0x6B38B0 / 0x6B3970; there is no other write in
    0x6A8000..0x6B4000), so they do not receive either. `ParticleAnimCreator::CreateLH3DObject` (0x6A9760) does not call vt+0x78.
  - `CollectShadowReceivers` (when the Main view's objects begin: receivers from `RenderContext::entityInstances` with
    `receivesDynamicShadow`, shadows with `onObjects`), `DrawShadowsOnObject` (behind the `DrawMesh` of each opaque mesh
    in Main, or behind its entry in the drain of `graphics::zsort`, in `MainBlended`; with the matrices it was drawn
    with) and `DrawShadowsOnCutObjects` (the parts of the sharks above the water, right behind `DrawCutAboveWater` and
    not one by one **(inferred: they are opaque and the receiver's Z test already discards what is drawn in front)**). A
    receiver that was not drawn in the frame (outside the view, already fully transparent, or past the 0x800 cap of the
    queue) receives no shadow: `ClearShadowReceivers` empties the list at the end of the objects, like the tail of a
    Draw that was not executed (0x80E457..0x80E4D7). ZFUNC Equal, except boned meshes and morphable ones
    (`ZFunc::LessEqualInclusive`: GEQUAL = LESSEQUAL with inverted Z; **🔍 inferred** that a boned mesh is of the
    animated class; the morphable Draw `fn_0080E550` does not touch ZFUNC around its loop 0x80E768..0x80E874).
    `fs_object_shadow` with `shadow.sh`. `ReceivesDynamicShadow` (RenderingSystem.cpp) also leaves out the one-shot
    balls, the shields and the power-up bands (`HandFxPart` and the `Power_Up_Band` mesh). Detail key `shadowsOnObjects`
    (levels 3–6).
  - **Morphable receivers** (done): the morphable Draw (`fn_0080E550`, vt+0x108 of 0x9A2E34; it is drawn if dx² + dz² <
    R², strict (0x80E84E). It does not check vt+0x1A8 / vt+0x1B8, only si+0x464 (0x80E782).
    `shadow_math::ReachesMorphable`, tested in `test_shadow_math`. **🔍 inferred** that `MorphWithTerrain` is the
    morphable class (vtable 0x9A2E34, Get3DType 1): the CITADEL class (Get3DType 8, `CitadelHeart` 0x464B40; vtable
    0x9A2BFC) draws with `fn_00882A40`, which calls the static Draw `fn_0080DB30` (0x882AB5), with
    `ContainsThisBoundingBox` and ZFUNC EQUAL; today no entity of type 8 carries the component (`CitadelArchetype` does
    not add it; `CitadelPart` is type 1, 0x4694B0). The test reads obj+0x14 from the matrix openblack draws; it holds as
    long as no `MorphWithTerrain` entity receives the tweaks of `RenderingSystem` (swaying of fields and trees, the
    tilting and shrinking of trees), which are the original's draw matrix and not obj+0x14 🔍 inferred.

### Shadows of physics objects

**Faithful** (done).
- `fn_00646FE0` (from `GLandscape::Draw` 0x5E49DC) → `fn_007FCE80` per physics object that is not at rest (byte
  elem+0x19C = PhysOb+0x174) nor with y ≤ −r: if it has no shadow and casts a static shadow (Flags1 0x1000 / 0x2000) or
  is animated, `fn_008745A0` creates a land-only `ShadowInfo` (si+0xC = 1). Pots, magic
  food, flowers, crops, DeadTree, AnimatedStatic and building pieces do not have one (`SetShadowOnTexture(0)`). It is freed in
  `PhysOb::DeInitialise`. No limit on the number and no detail key.
- Light: the object's position + (0, 15000, 0) (0x9A3C10), not the sun: practically vertical projection. Box = the
  minimal one of the projected vertices (no margin). 32×32 silhouette with 4×2 subsamples per texel (`fn_00806F60`), alpha =
  covered subsamples / 15 (max. 8/15) without writing the outer ring (`fn_00880FC0`, table 0xFA95C4); trees
  through the chroma path (texture with alpha and 2×2 filter). Fade at 50–80 radii from the camera to the ground under the
  object (`fn_00874600`, with the 9-block test), **baked** in nibble steps. On the land
  (`fn_00878350`): t' with H = GetAltitude of the caster (one per shadow), nothing on cells with altitude ≤ 1; mode 6 black.
  All the detail, in [projected shadows (ShadowInfo)](#projected-shadows-shadowinfo).
- it is drawn on each block it touches (`Renderer::DrawLandShadows`).
- **Static shadow of what is not on the map**: the bake (`fn_008721A0`) takes the casters from the map cells (`0x5E2A90`
  / `0x5E2C30`); picking up an object (`fn_005DC330`) or giving it physics (`Object::InitialisePhysics*`) removes it
  from them until it lands (`EndPhysics` → `InsertMapObject`). Quirk not reproduced: only Fixed/MultiMapFixed rebake the
  block when they leave; the old shadow of a tree or a MobileObject stays on the ground until another change rebakes
  that block.
- Test: `OPENBLACK_TEST_PHYSICS="1490.2140.760.0,0.0,0.6.3"` with the camera `1450.60.2095.1492.6,2140` and `-n 4000`
  (the rocks fall at ~70 u/s; in the capture they are at a height of ~110 and their three shadows can be seen on the ground).

### Dynamic hand shadow

**Faithful** (done; source: the user's observation of the original, 2026-10-02; see
[projected shadows (ShadowInfo)](#projected-shadows-shadowinfo)).

- Original: `CHand::CHand` 0x46BC0B → `CreateDynamicShadow` 0x80C020 (if [0xC3820C] ≠ 0, 1 in the data), a
  complex `ShadowInfo` (`fn_00814FD0`) with si+0x3C = 1 (0x80C037: the fill skips the even subrows, 0x880141,
  at most **4/15**), the light 200 above the hand (0x8151C4, [0x8C7B34]), the base at the hand's y (0x8152B1), t' = 1
  on the land (there is no si+0x464) and the held object (si+0, `SetHeldObject` vt+0x234 = `fn_00816830`, only if
  `IsG3DObjectDrawnInHand`) inside the same texture at full density (0x807532..0x8075B7). It falls on the land
  and on objects (si+0xC = 0).
- As in the original: light silhouette
  with the fingers, dark round shadow of the held orb, whole orbs on top. With `false` the look from before the list
  returns (64×64, 8/15, on the ground under the hand, without the held object), only for
  comparison.

### Shadows (three systems in the original)

- **Static** (done): `fn_008721A0` (block texture thread, after snow and footprints). All Fixed and MobileObject objects
  cast them (`SetShadowOnTexture` in `Create3DObject` 0x52DE30 / 0x607210), trees and forests with alpha test
  (`DrawTextureShadow`); not AnimatedStatic, DeadTree, flowers, vessels, magic food, crops, shields, seeds, totems. LOD
  0, shear by the sun x' = x + h, z' = z + h (h above the object's base), coverage 4×2 per texel; the texel's RGB × (255
  − I/2)/256, that is ×0.5 with full coverage (×0.75 with 128 px textures).
- **The trees' static shadows in the bake** (`LH3DStaticObject::DrawTextureShadow` 0x80ECB0, for the objects whose
  chroma bit 13 is set: trees and big forests): when `g_b_P3` (`LH3DP3::IsPentium4`, from the Intel compiler's CPU
  dispatch in `CheckCPU` 0x7ACA00) is set, the rows are filled by `DrawTShadowAlpha` 0x7AC4D0 with SSE (`rcpss` and a
  Newton step for 1 / (max − min), the 0.5 added once) instead of the x87 `DrawAlpha` 0x874440; the two differ by up to
  5 mask bytes a shadow, and `rcpss` is not the same bit for bit on every processor. On the test machine (Intel family 6
  model 170) the dispatch leaves the indicator at 0x80, `IsPentiumIII` = 1 and `IsPentium4` = 0, so `g_b_P3` = 0
  (`LH3DP3::Init` 0x7AC9E0 run natively by the exe oracle, 2026-10-23): the original takes the x87 path there, and
  openblack's `block_texture::DrawTextureShadow` is that path. That raster's pitch and last row (0xC398EC / 0xC398F0)
  are 256 and 255 from `LH3DRender::Open` 0x82B4C5 (other writers not looked for): with the
  low-resolution textures (128 a side) the original would draw it 256 a row into a mask 128 wide. openblack draws it
  `size` a row; not reproduced (the checks run at detail level 6, 256 a side).
- **Projected** (the `ShadowInfo` list): its own section, [Projected shadows](#projected-shadows-shadowinfo).
- **Footprints** (done), in the same bake: for each object over the block that asks for a texture shadow or a print (vt +0x84 `IsShadowOnTexture`, vt +0x8C `IsFootPrintOnTexture`; each object once per bake, through its enum flag vt +0x64 / +0x60), after its shadow, the mesh's footprint is printed (`fn_00872FA0`) when the mesh's header has bit 0x8000 (the landscape feature) and the feature's data is there with a count (its first word) other than 0 (0x8723B8..0x8723E7). A field asks for its print as it is made, with no condition (`Field::CallVirtualFunctionsForCreation` 0x528A40, `SetFootPrintOnTexture(1)` at 0x528A9B); a building asks for it while unbuilt (0x52EA33). openblack: `MeshPrintsFootprint` (`src/3D/L3DMesh.h`) is the same mesh rule, read by `Renderer::DrawFootprintPass` for every placed mesh, a field's included; tests `FootprintPrint.*`.
- **Blobs of villagers and animals** (done): `human_shadow.raw` 32×32 between two bones (`fn_0081FFF0`), mode 6;
  see [villager blobs, object reflections and LOD](meshes-and-lod.md#villager-blobs-object-reflections-and-lod).

### Projected shadows (ShadowInfo)

**Faithful** except where marked.

- A linked list of `ShadowInfo` (0x4AC bytes, head [0xFAA7E0]); each new one goes at the front (`fn_0087FD50`
  0x87FEC4..0x87FEF2). There are only two constructors: `CreateDynamicShadow` (0x80C02C, the hand and the creature) and
  the holder `fn_008745A0` (0x8745BA: physical objects in flight `fn_007FCE80`, the boat `PetitNavire::PetitNavire`
  0x5E11AE, the prediction, PSys and SuperVillagers). Each one has its own 32×32 ARGB4444 texture (si+0x45C) and its
  material `CreateMaterial(6)` with +5 = 0 (single-sided, **CLAMP**, 0x87FE12).
- **Fade** `fn_00874600`: 0 if the block under the emitter is at ≥ 100000 ([0xC37200]); 0 if none of the 9
  blocks at (−60, 0, +60) ([0x8C36A8]) exists, is visible (+0x920 & 1, the 5 outcodes of `fn_00877210` over the 8
  corners of `haze::BlockCorners`) and is at < 100000; otherwise, q = |(x, ground, z) − camera| / (scale · radius):
  255 below 50 ([0xC398F4]), linear down to 0 at 80 ([0xC398F8]).
- **Alpha**: generic `ftol(fundido · base / 255)` ([0x900058], 0x874872); complex (hand) the same below 255 and 255
  otherwise (0x815007..0x815051).
- **Light**: vertical pos + (0, 15000, 0) ([0x9A3C10]); the fixed sun [0xEA1C88] if holder+4 (the boat, 0x5E11B6); the
  hand pos + (0, 200, 0) ([0x8C7B34]); the creature at 3 radii minimum and 45° (0x815058..0x815181; the flow is in
  "Creature light" below).
- **Silhouette on the CPU** (`fn_00806F60`): projection `fn_00850900` with the **light's absolute y** (t = −Ly / (h − Ly),
  h = max(0, W.y − base)), box = min/max without margin, grid of 128 × 64 subsamples (4 × 2 per texel), triangles
  with the back face discarded (`fn_00850CC0`), half-open 16.16 edges (`fn_0087FF70`), filling by nibbles
  (`fn_00880050`; with si+0x3C the even subrows are not written, 0x880141). Resolve `fn_00880FC0`: texel = popcount
  → alpha **n/15**, the **outer ring** stays at 0. If si+0x10 ≠ 255, **baked fade** n' = floor(n·a/255)
  (0x80769A, `0x80808081`, `and 0xF000`).
- **Chroma path** (the emitter has vt+0x94: trees, dead trees, forests, the hand's food): instead of the
  rasterisation, `DrawTextureShadow32x32` (vt+0x168 = `fn_0080EE80` → `fn_0084B7D0` → `fn_00881DE0`) paints the
  textured triangles with the 64×64 map of its texture's alpha (`fn_00838F00`: high nibble & 0xF0), each texel
  OR `(a & 0xE0) << 7` (at most 7/15), and then a 2×2 OR filter of the nibbles (0x807635..0x807688). If the
  emitter is chroma the held object is dropped (si+0 = 0, 0x8073E8).
- **Land** (`fn_007FF610` 0x7FF749): right after each block of the main land (not the mirrored one), for every active
  shadow whose box touches the block (bx·160 ≤ x1, (bx + 1)·160 ≥ x0; [0x8D151C]), **all of them** (it does not look at
  si+0xC). `fn_00878350`: H = GetAltitude(emitter) if si+0x464, otherwise si+0x18 (the hand: t' = 1), **one H per
  shadow**; t' = (si+0x18 − Ly)/(H − Ly); u, v in the box; diffuse 0 if the altitude byte ≤ 1; codes 0x400 /
  0x40..0x200 and the triangle with a common code is removed. Drawn **immediately** (`LH3DRender::DrawTriangle` 0x82F810
  → IDirect3DDevice7 vt+0x68, 0x82F916).
- **Objects** (`fn_0080B050` → `fn_0084E200`): see
  [object reflections and hand shadow on objects](#object-reflections-and-hand-shadow-on-objects).
- **Where it goes in the frame relative to the Z-sorter**: no shadow is a Z object of its own (`fn_00878350`,
  `fn_0080B050` and `fn_0084E200` are not among the 32 callers of `NewZObject` 0x83F310). The land ones go with the
  land, before the flush (`FinishFrame` 0x82F480); the object ones, at the end of each object's Draw: immediately if it
  is opaque, inside its Z object if it is in the queue.
- List: a new entry is [0xFAA7E0] with +4 = the old head (0x87FED6); `fn_0087FD50` makes its texture with
  `fn_008379E0(0, 0xC4, …)` and clears it (0x87FDBE..0x87FDFC); its material +5 = 0 gives no tiling (0x87864D).
  `fn_0087FF10` takes it out and releases the texture (0x87FF40).
- ShadowInfo fields: si+0x08 active (`fn_00814FD0` clears it when the object is hidden, 0x814FEE; `CHand::Hide`
  0x46C2E0 → `fn_00814FD0` 0x814FE4..0x814FF9, so a hidden hand casts nothing); si+0x10 alpha (0 = not drawn,
  `fn_00881030`); si+0x14 base alpha (0x87FED0); si+0x444 the light, si+0x448 its y; si+0x450 = caster − light
  (`fn_00806F60` 0x80708B..0x8070D4); si+0x464 the caster, written by `fn_00874850` (0x874993), never by
  `fn_00814FD0`.
- Which physics objects get one (`fn_007FCE80` 0x7FCE9D..0x7FCEC7): IsShadowOnTextureChroma (vt+0x94 = `fn_007F98D0`,
  LH3DObject +4 bit 13, tested first, 0x7FCEA1), IsShadowOnTexture (vt+0x84 = `fn_007F98A0`, bit 12) or animated
  (vt+0x1AC). It is removed with the physics (`PhysOb::DeInitialise` 0x7FB772).
- The chroma bit is set through vt+0x90(1) (`fn_007F98B0`) by Tree 0x749F96, BigForest 0x43908B, DeadTree 0x510AC6,
  `FelledTree::Create`'s `fn_00510880` (0x5108DA) and the Pot of info 12 (0x66D173, after the `cmp 0xC` of 0x66D164);
  FieldCrop clears it (0x607EAB).
- Other casters: the SuperVillagers' TemporaryShadow (`fn_00825F20` → `fn_00825090`); the PSys mesh atoms with
  CastHumanShadow (`CreateParticle` 0x6A8B5C..0x6A8B7F → `fn_006CA340` → `fn_008745A0`: si+0xC = 1 at 0x8745C8,
  holder+4 = 0 at 0x8745C1; updated by `fn_006CA540` 0x6CA5A9 → `fn_006CA3D0` (0x6CA3D5..0x6CA3E1) → `fn_00874850`
  with the particle's object; removed by `fn_006C7A80` 0x6C7AA6 → `fn_006CA370` → `fn_0087FF10`); the creature
  (LH3DCreature 0x47F543). Not ported: the prediction object (`fn_00646FE0` 0x647245).
- Each new entry is linked at the head of [0xFAA7E0] when its caster gets it (the TemporaryShadow at its script op), so
  the list runs newest first across every kind. openblack (2026-10-26) keeps an entry where it is from frame to frame and
  makes the SuperVillagers' from their own list oldest first, so the newest is at the head
  (`shadow_list::TemporaryShadowsInMakingOrder`, tests `ShadowListOrder.*`); it makes a new entry at the next drawn
  frame, so entries of different kinds made in one frame are listed by producer (**⚠️ approximate**, only the rounding
  where they overlap).
- A PSys atom's object is an LH3DStaticObject (`CreateLH3DObject` 0x6A8AA6, `LH3DObject::Create(0)` 0x80B4F8):
  vt+0x1AC is 0, so `fn_00874850` skips the pose (0x8748A2..0x8748AA); and it never gets vt+0x90(1)
  (0x6A8AA0..0x6A8AFD; the ctor's +4 = 0x10009 leaves bit 13 clear), so it is never chroma.
- Generic update inputs: the position is obj+0x38..0x40 of the drawn (interpolated, 0x7FCED2) matrix; the scale
  obj+0x44 (a PSys atom's: its PSR +0x30, `Particle3DObj::DrawAt` 0x67A009); the ground GetAltitude 0x874789; the fade
  at 0x874872 (complex 0x815002); alpha 0 tests 0x874898 / 0x815041.
- The light: the vertical one at 0x87490A, the fixed sun at 0x8748E7 (0x8748E0..0x874932); the hand's is obj+0xBC
  (CHand 0x46CB28) + (0, 200, 0).
- The caster's bones: the hand's [0xC37D9C] = CHand+0x47F0 (0x46CB1C), a posed model's (`fn_00839980`), else the
  mesh's rest pose.
- The held object: `SetHeldObject` (vt+0x234 = `fn_00816830`, 0x816855) is only called when IsG3DObjectDrawnInHand
  (vt+0x618, 0x46DC86). The hand's render object CHand+0x4904 is set at the press by `fn_0046DC30` (0x46DCDC, with
  SetCastDynamicShadow(0)) and restored by `CHand::ThrowObject` 0x46DDD0.
- Creature light (`fn_00814FD0` 0x815058..0x8151A5, the complex update shared with the hand):
  R = mesh +0x30 (half diagonal) × obj+0x44 (scale) × 3 ([0x8C2C50]); d = light [0xEA9E90] − obj+0x38..+0x40, axis by
  axis, stored as floats; h = sqrt(dz·dz + dx·dx). If h < R: when h < 0.1 (the double [0x8C9D40], 0x8150BF) dx and
  dz + 1; unless d is (0, 0, 0) (0x8150E0..0x81510D), d × R / sqrt((dy·dy + dz·dz) + dx·dx); h again. Then if
  dy / h < 1, dy = h (45° at least, 0x815153..0x815166). The light is obj + d (0x81516E..0x8151A5). That point is
  computed for the hand too and then replaced by obj + (0, 200, 0) when obj+0xBC is set (0x8151B4..0x8151F4).
- The creature's `ShadowInfo` comes from the same constructor as the hand's (`CreateDynamicShadow` 0x80C020): si+0x3C
  = 1 (0x80C037), so its silhouette is written in half rows like the hand's; si+0x00 = obj+0x8C; si+0x0C = 0
  (`fn_0087FD50` 0x87FEE0), so it is drawn over the objects too. All of it only when [0xC3820C] is set (0x80C02A).
- Fade loop (`fn_00874600`): the block under the caster at 0x874603..0x874684; the 3×3 neighbours with x outer and z
  inner, −1..1 (0x87468A..0x874737); cells outside 0..0x1FF (0x87463D / 0x874649) skip the first test and count as no
  block; q = sqrt((dz² + dy²) + dx²) / (scale·radius) with dx, dy, dz stored as floats (0x87478E..0x8747EE); the
  radius is the mesh's +0x30 (half diagonal); the ramp 0x874822..0x874844.
- The box before the first vertex is 0x60AD78EC / 0xE0AD78EC (`fn_00874850` 0x87499B / 0x8749AA, `fn_00806F60`
  0x8070DA).
- Sub-meshes rasterised: those with the LOD 0 bit 0x20000000 ([0xC37D94] = 1), through `fn_0087FA70` → `fn_00850900`;
  caster 0x807109..0x807148, held object 0x8071D3..0x807249. A skinned vertex uses its bone (branch [0xFA93BC] & 1,
  0x850B04; one bone matrix per vertex group {count, bone}, 0x850B04..0x850B2B).
- Projection (`fn_00850900`): the matrix's ty is lowered by the base y (0x85094F..0x85096E; the skinned branch does
  the same to its copy of the bone matrix, 0x850B2D..0x850B35); h = max(0, W.y) (0x8509D4..0x8509E3); the box also
  tracks k = W.z·d.z + W.x·d.x as kMin (0x850A05..0x850A2C, 0x850A1B); Ly is negated at 0x850914; the box grows at
  0x850A70..0x850AB8.
- Two-sided casters: material +5 & 1 or a mist (vt+0x1F8 = IsMist, 1 only in Mist 0x55EB90; no mist gets a ShadowInfo)
  (`fn_00850CC0` 0x850CC9..0x850CEE).
- Winding (`fn_00850CC0`): a one-sided triangle is kept when (r0 − r2)(x1 − x2) ≥ (r1 − r2)(x0 − x2), rows r = ftol(z)
  (0x850D03..0x850DA1), and walked 0→2, 2→1, 1→0; a two-sided one is walked 0→2→1 when (r0 − r1)(x2 − x1) < (r2 −
  r1)(x0 − x1), else 0→1→2 (0x850E29..0x850F6D).
- Grid: [0xC39B08] = 128 and [0xC39B0C] = 64 (0x807077 / 0x807081); one coverage byte per texel at si+0x40, cleared at
  0x80706B..0x807075; bits 0..3 the even subrow, 4..7 the odd one, bit x & 3 for the subsample (masks 0x9A3C74 /
  0x9A3CB4 / 0x9A3CF4 / 0x9A3D34, also 0x9A3D74 / 0x9A3D78).
- Into the grid (0x807265..0x807305): px = (x − x0)·(128/(x1 − x0)), pz = (z − z0)·(64/(z1 − z0)), the factors stored
  as floats first ([0x8C6CA8] = 128, [0x930678] = 64), each clamped to 0 below and to 127 ([0x8C4A00]) / 63
  ([0x9A2BF8]) above.
- Span tables: [0xFA9FC8] the left ends (edges going up), [0xFA97C4] the right ends (going down), the touched rows
  [0xFAA7D0]..[0xFAA7CC]. They are static and never cleared: each triangle only resets the rows ([0xFAA7D0] =
  [0xC39B0C], [0xFAA7CC] = 0, 0x850DB2 / 0x850DBD).
- Edge (`fn_0087FF70`): horizontal edges are skipped (0x87FF7B); edges going up are swapped into the left table
  (0x87FF81..0x87FF9D); off-grid edges are rejected (0x87FFAE / 0x87FFB8); slope = (x1 − x0)/(y1 − y0) with fidiv
  (0x87FFBE..0x87FFD0); y1 is clamped to the rows; with y0 < 0, x0 − y0·slope (0x87FFD8..0x87FFE8); the step is
  ftol(slope·65536) ([0x8AC408]) and each row stores x >> 16 (0x880030).
- Fill (`fn_00880050`): each row of [minRow, maxRow) fills [max(0, l), min(grid x, r)), nothing when empty
  (0x8800A0..0x8800D0); the odd subrow test is `test bl, 1` (0x8800E0); two subrows per texel row
  (0x88019E..0x8801A7).
- Resolve: rows and columns 1..texels − 2 only; the table [0xFA95C4] = popcount(i) << 12 is built by `fn_00880F20`.
  The baked fade is skipped when the alpha is 255 (0x8075BD..0x8075D1).
- The chroma map (`fn_00838F00`): 64×64, cached at texture+0x12C; byte (r, c) = the high byte of texel (ftol(r·h/64),
  ftol(c·w/64)) & 0xF0 (0x838F88..0x838FD0, [0x8D8BD0] = 1/64).
- The chroma path (`fn_0080EE80` into the 16-bit target of `fn_008816F0`): every sub-mesh with the LOD 0 bit
  (0x80EECA..0x80EED5) **except boned ones** (`fn_0084B7D0` 0x84B7F5: [0xFA93BC] & 1), each primitive with its
  texture's map (0x84B97E), through the object's matrix [0xEA1AE8] (no bones), into LH3DP3::Table1 0xE437E0; triangles
  of 6 bytes (0x84B99C..0x84BA71).
- Chroma vertex (0x84B804..0x84B96D): t = (si+0x18 − Ly)/(W.y − Ly) (no clamp of h); x = ((W.x − Lx)·t + Lx −
  x0)·(32/(x1 − x0)), clamped to [1, 31] ([0x8AA390], [0x92B6F4]), z the same; u, v = uv × 63 ([0x9A2BF8]); W = (x·m00
  + z·m20) + y·m10 + t (0x84B83F..0x84B88D).
- Chroma triangle (`fn_00881DE0`): vertices ftol'd (0x84B9AA..0x84BA4C) and clamped to the target
  (0x881DE9..0x881E83), sorted with a the upper vertex (ties: level and to the right; 0x881E86..0x881EEA); edges
  `fn_00881A60` (12-byte x, u, v entries, rows inclusive, steps ftol(d·(1/(y1 − y0 + 1))·65536); a horizontal edge
  stores its ends as they are, not in 16.16); spans `fn_00882080`, inclusive, u and v stepped by an integer division;
  texel index ((v >> 10) & ~0x3F) + (u >> 16) (0x882141..0x882150).
- Order inside `fn_00806F60`: the grid, then a chroma caster (vt+0x94, 0x80732B) drawn into the cleared 0x800-byte
  buffer (0x807339..0x8073E3, its raster 0x8073F5..0x807466 skipped), else the raster; the held object
  (0x807466..0x8075B7); the resolve (0x807601); the chroma blur; the baked fade.
- Visibility of a block (`fn_00877210` 0x8773E4..0x8774E2): the 8 corners through `g_world_to_clipping`; outcodes Z <
  near ([0xE839E0], also read by `CameraModePath::SetUpNearClipping` 0x460F10), X > Z else −Z > X, Y > Z else −Z > Y;
  not visible when one outcode holds for all 8; stored at 0x8774E8 / 0x877CEB.
- Over the land `fn_007FF610` is called from `GLandscape::Draw` 0x5E4E96; `fn_0084E200` draws at once through the
  draw-triangle pointer [0xC386EC] (0x84E8F0).
- Morphable receiver test: when width and depth are equal (or unordered) the depth is taken (0x80E7A7..0x80E7B4).
- The default sun [0xEA1C88] is (0xC8F42400, 0x48F42400, 0xC8F42400) as floats (set by `fn_00818920`): the light of a SuperVillager's high-detail body and its eyes (`fn_008254A0` 0x8254C3..0x8254D1 / 0x82551F), not the frame's light.
- `LH3DObject::IsShadowOnTextureChroma` (vt+0x94 = `fn_007F98D0`, flags +4 bit 13) is set through vt+0x90(1) (`fn_007F98B0`) by Tree (0x749F96), BigForest (0x43908B), DeadTree (0x510AC6), `fn_00510880` of FelledTree::Create (0x5108DA) and the Pot of info 12 (0x66D173); FieldCrop clears it (0x607EAB). Such a caster is drawn textured into a cleared 16-bit target of 0x800 bytes (0x807339, the 32 × 32 case).
- The rasteriser's grid is 4 × 2 subsamples per texel ([0xC39B08] = 4, [0xC39B0C] = 2), so a 32 texels side is 128 × 64 subsamples; the 2 texels of the odd subrow are bits 4..7 of each byte.
- The morphable receiver's box factor is [0x932D08] = 1.41419995 (bytes `8104b53f`), not the float nearest √2 (0x3FB504F3).

## openblack

Status: **done** for the static shadows, the projected shadow list (hand, creatures, thrown objects, the boat, the
SuperVillagers and the PSys meshes), the blobs and the reflections of the hand, what it holds, the physics objects,
the boats and the creature; the physics prediction's shadow is not ported. Progress:
[Shadows](../project/progress/rendering/shadows.md).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Object reflections and hand shadow on objects**
  - openblack: `sea_pass::ReflectsCreature` and `Renderer::DrawCreatureReflection` (its `creature_draw::Body`: the
    drawn bones, morph and skins), the land blocks' states from `shadow_list::List::BlockAt`, which are now built
    every frame.
  - openblack mirrors the fragment's piece, not the rock the original mirrors (pending: `Renderer::DrawObjectReflections`).
  - openblack: `Renderer::DrawBoatReflection` (mode 2 of `vs_object` with the packed rgb).
  - openblack draws it first too.
  - openblack: `Renderer::DrawObjectReflections` in the reflection pass (what the hand holds and the awake bodies of
    the physics list 0xD47814 via `PhysicsObjects::ForEach`, with centre y > −r, r = `physics::Body::Radius`; those thrown from the hand that are not in
    physics, with the box radius), each through
    `Renderer::DrawUnderWater(view, entity, sea_pass::UnderWaterLastDraw())` (mode 3 of `u_objectLight` in
    `vs_object`, plane KeepAbove).
  - openblack: `Renderer::DrawUnderWater(view, entity)` and `DrawCutByPlane(view, entity)` take the entity's own
    model (`EntityInstance::ownMeshId`, the `Mesh`) and the pose the main draw uses (`ecs::DrawnPose`; the
    reflection through `sea_reflection::ReflectedBones`)
  - openblack: `Renderer::DrawUnderWater(view, entity, …)` takes the entity's drawn pose (`ecs::DrawnPose`, the
    cross-faded one of a SuperVillager while it fades) when it fits the mesh, else the mesh's bones
    (`sea_reflection::ReflectedBones`)
  - openblack: `RendererShadows.cpp`.
  - openblack: `shadow_math::ReachesMorphable`, as the original's morphable Draw (`MorphWithTerrain`), which does not
    use `ContainsThisBoundingBox` but its own circle test
    (0x80E78E..0x80E857), and draws with `fn_0080AE40` (the same mode table and the same CULLMODE as `fn_0080B050`,
    with the blended vertices of [0xF05180]): R = (obj+0x44 · mesh+0x30) + max(x1 − x0, z1 − z0) · 1.4142
    ([0x932D08] `8104b53f` = 1.41419995, **not** the float closest to √2), the mesh centre +0x18..0x20 times the
    object matrix obj+0x14 and the distance in x, z to the box centre ((x0 + x1) · 0.5 [0x8AA3B4])
- **Shadows of physics objects**
  - openblack: one more entry of `graphics::shadow_list` (`PhysicsObjects::ForEach` + `CastsPhysicsShadow` from
    `ShadowList.cpp`), rasterised on the CPU with each vertex's pose, with no cap.
  - openblack: the bake (`ecs::land_prints`) takes its casters from the map cells, and the object in the hand and those
    that are flying are out of them (`map_cells::RemoveMapObject` at the pick-up and at the start of physics).
- **Dynamic hand shadow**
  - openblack: the hand's entry in `graphics::shadow_list`, with `k_HandShadowAsOriginal = true` (`ShadowList.h`):
    32×32, at most 4/15, the base at the hand's y and the held object (an orb taken from the dispenser, for example)
    at full density in the same texture, drawn like the others (on each block and on objects at their place in the
    queue).
- **Shadows (three systems in the original)**
  - openblack: baked into the block's texture at its prints step as the original (#222): the mesh's sub-meshes through
    the sun into the 8-sample mask (`block_texture::DrawShadowTriangles`), a tree's and a big forest's from its texture
    alpha (`DrawTextureShadow`), then the darkening (`ApplyShadow`) after the footprints, the river beds and the snow's
    place (`block_texture::BakePrints`, the sources from `ecs::land_prints::SourcesFor`); the casters are the classes
    the original sets the bit for, taken from the map cells over the block: the citadel's heart and its worship sites
    among them, citadel parts being MultiMapFixed (bw1-decomp `Black/CitadelPart.h`), the heart once built (its
    model's bit turns on at 100 %). Both also print their footprint built or not: the heart's 3D object is given the
    footprint bit as it is made (its temple's set-up, whatever its percent), and a worship site's both bits. The old live passes printed every mesh's footprint whatever its class, which hid
    that the static shadow left them out.

### Projected shadows in openblack

- `src/Graphics/ShadowMath.{h,cpp}` (`graphics::shadow_math`, without bgfx or ECS: fade, alphas, lights, projection,
  rasterisation, resolve, chroma filter, baked fade, block and visibility test), with `test_shadow_math`.
- `src/Graphics/ShadowList.{h,cpp}` (`graphics::shadow_list`): the list and its producers (the hand, the creatures, the
  physical objects in flight from `PhysicsObjects::ForEach` with `CastsPhysicsShadow`, and the
  `components::DynamicShadow`: the boat with `useSun`), one 32×32 R8 texture (n·17, CLAMP) per entry, uploaded every
  frame. The pose comes from
  `L3DSubMesh::GetSkinBones` / `GetSkinLocalPositions` and from `ecs::PosesByInstance`.
- `src/Graphics/RendererShadows.cpp`: `UpdateShadows` (once per frame), `DrawLandShadows` (in the block loop of
  `Renderer::DrawPass`, Main view, right after each block's `submit`; program `LandShadow` =
  `vs_land_shadow` / `fs_land_shadow`, which shares `land_position.sh` with `vs_terrain` to give the same Z;
  `render_modes::ZFunc::LessEqualInclusive` (the D3DCMP_LESSEQUAL of 0x82CCC5, which with inverted depth is
  GEQUAL; also the one for the animated and morphable receivers), without writing Z, mode 6, the block's culling; texture in stage 11) and the shadows
  on objects (`CollectShadowReceivers`, `DrawShadowsOnObject`, `DrawShadowsOnCutObjects`, `ClearShadowReceivers`). `shadow.sh`:
  `LandShadowUv`, `ObjectShadowUv`, `ShadowKept` (the 0x400 code).
- `fs_terrain` draws no shadows of its own, and there is no cap of 16.
- Order relative to `graphics::zsort`: the land ones, in Main with their block (before the queue); the object ones,
  after the object's `DrawMesh` in Main or inside its queue entry (`MainBlended`); no new entry in the queue.

Where openblack and the original meet or differ:

- **The hand (done; source: the user's observation of the original, 2026-10-02)**: `shadow_list::k_HandShadowAsOriginal
  = true`, like the original: si+0x3C = 1 (`CreateDynamicShadow` 0x80C037), so `fn_00880050` does not write the
  even subrows (0x880141..0x880146) and the hand reaches at most 4/15; 32×32 (`fn_0087FD50`); base at the hand's own y
  (si+0x18 = obj+0x3C, 0x8152B1..0x8152B4), with t' = 1 on the land; the held object si+0x00
  (= obj+0x8C, `SetHeldObject` `fn_00816830` 0x816855) rasterised into the same `ShadowInfo` with its own base
  (0x807163) and at full density (si+0x3C saved, set to 0 and restored, 0x807532 / 0x80753D / 0x8075B7; in the loop
  over its primitives 0x80714F..0x807259). A chroma held object uses the `ShadowInfo`'s base (si+0x18, `fn_0084B7D0`
  0x84B7E0), not its own. With `false` openblack's look from before the list comes back (64×64, full density, on the
  ground under the hand, without the held object), only for comparison.
  - In the original: the hand's shadow is its silhouette, with the fingers, light grey and half transparent
    (4/15); a held orb casts a darker, round shadow below it (full density); the orbs are drawn whole on top of the
    shadows; the dispenser keeps its shadow.
    - the hand over the dispenser's orb: behind, on the ground, the light silhouette of the hand;
    - the hand with an orb over the dispenser: a dark, round shadow at its foot, the same with another orb;
    - the hand with an orb over the grass: a dark, round shadow displaced down and to the left.
  - Comparison (openblack, scene `OPENBLACK_TEST_DISPENSER=NORSE_ABODE_SPELL_DISPENSER,1826,2670,10,2`,
    `OPENBLACK_CAMERA_LOCK=1816,52,2656,1826,37,2670`, 13 h; the hand picks up the orb with `OPENBLACK_TEST_TUG=1700,2500,10,3`
    and `OPENBLACK_TEST_TUG_MOUSE2`): before, the hand's shadow was a dark silhouette (8/15) and the held orb had no
    shadow; now the silhouette is light grey with the fingers, as in the original, and with the orb in the hand
    the original's dark, round shadow appears below it, on the grass and at the foot of the dispenser; the orb
    stays whole on top.
  - **⚠️ approximate** the darkness of the orb's shadow: on the grass, openblack ends up at 0.46 of the ground's
    luminance (65 / 142) and the original at 0.55 (63 / 114). openblack gives the code's maximum: with the
    trace (`OPENBLACK_SHADOW_TRACE=1`) the shadow carries alpha 255 (fade 255, without
    `BakeAlpha`) and n = 8 at most (4×2 subsamples per texel, `fn_00880FC0`), 8/15 → 1 − 0.533 = 0.467. The difference
    fits a fade in the original below 255 (n' = floor(8·a/255) = 7 for a = 223..254, 0x80769A..0x8076EC; the original's
    camera there is not known), half-covered edge texels or the lighting; nothing has been read in the binary that
    explains it, so the code stays as it is.
  - The dispenser's **own** shadow is not a `ShadowInfo` (the trace only lists the hand's): the dispenser is an
    `Abode`, so it casts a **static** shadow (`ecs::land_prints`, `fn_008721A0`, shear x + h, z + h).
    With the camera on the opposite side (`OPENBLACK_CAMERA_LOCK=1846,47,2690,1826,37,2670`) it is
    seen as a soft blotch in front of the tripod, lightened by the dispenser's disc and sparks; with the original's
    view it falls behind the table.
- **⚠️ approximate** the 0x400 code, which in the original removes whole triangles: on the land, `fs_land_shadow`
  removes the triangle when its three vertices carry the code, but does it per fragment with the interpolated value,
  so along the edges a few stray fragments too many or too few remain; on the objects, `fs_object_shadow` applies it
  per fragment. With vertical light k = 0 and nothing changes.
- **⚠️ approximate** the redraw over the land (Z GEQUAL without writing Z) depends on `vs_terrain` and
  `vs_land_shadow` giving bit-identical depth: they share `land_position.sh`, but they do not carry `precise` /
  `invariant`. On the current backend (D3D11) there are no specks in the captures. If they appear on another backend or
  with another shader compiler, plan B is a minimal bias towards the camera in `vs_land_shadow` (with inverted Z,
  `gl_Position.z += ε·w`) or declaring the position invariant in both. The same applies to the shadow on static
  objects (ZFUNC EQUAL against the object's instanced draw).
- **🔍 inferred** linear filtering of the texture (that of LH3D's default stage); the boat's visibility with the normal
  camera and not the mirror's; an invisible block has its new distance here (the original keeps the old one).
- **The creature**: every drawn creature is a complex entry, made after the hand's, with the hand's
  half rows (`CreatureShadow::k_HalfRows`; full rows before), 32×32, drawn over the land and the
  objects (never over a creature's body). Its light is `CreatureShadow::LightPoint` (raffclar's name over
  `shadow_math::LightCreature`) from the model light; the fade and the alpha are the hand's (`shadow_math::Fade`,
  `AlphaComplex`; `CreatureShadow::Alpha` is the same ramp without the block test). raffclar's
  `Graphics/CreatureShadow` numbers that differ from the original and are not taken: the nudge below 0.2 across the
  ground (the original's is 0.1), a bounding-sphere radius (the original's is the mesh's half diagonal × scale), a
  64-texel GPU cell per creature with four filtered taps, up to 8 creatures, no block test, and his
  `creature_shadow.sh` receiver shader (ours draws the 32×32 texture over each land block and object).
- Not ported: the prediction. The SuperVillagers' shadows and the PSys meshes' (`fn_006CA340` from 0x6A8B76,
  `MeshCreator::InitAtom`) are drawn, through the shadow list.

### Code map

The source files of this page and the symbols their notes describe:

- `src/Graphics/Renderer.cpp`: `SuperVillagerFrame::litByDefaultSun`.
- `src/3D/BlockShadows.cpp`, `src/3D/BlockBake.cpp`, `src/ECS/LandPrints.cpp`: the static shadows baked at the prints step.
- `src/Graphics/RendererShadows.cpp`: `Locator::handSystem::value`, `Renderer::CollectShadowReceivers`.
- `src/Graphics/ShadowList.cpp`: `BuildBlocks`, `IsChroma`, `LightKind::Vertical`, `List::Add`, `List::Frame`, `RenderChroma`, `Update::Generic`.
- `src/Graphics/ShadowMath.cpp`: `BlockOfCell`, `BlockVisible`, `ChromaEdgeOf`, `ChromaTriangle`, `ChromaVertex`, `LightCreature`, `LightGeneric`, `MakeAlphaMap`, `RasterTriangles`, `ReachesMorphable`, `ToGrid`.
- `src/Graphics/ShadowMath.h`: `AlphaGeneric`, `AlphaMap`, `BlockAt`, `LandProjectionFactor`, `LightHand`, `MakeProjection`, `TouchesBlock`.

### Test hooks

- `OPENBLACK_SHADOW_TRACE=1` and `OPENBLACK_DUMP_SHADOWS=<folder>`, and the test `test_shadow_math`
  ([Test hooks](frame.md#test-hooks)); `OPENBLACK_TEST_PHYSICS` and `OPENBLACK_MOUSE_AT` scenes are in
  [Test hooks](model-lighting.md#test-hooks).
- View menu: the "Creature Reflection" switch turns the creature's reflection off.
- The full list: [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- [Projected shadows](#projected-shadows-shadowinfo): the original's prediction object casts one (`fn_00646FE0`
  0x647245). openblack draws the release prediction's object at its predicted pose (the dynamics system's prediction,
  [the physics page](../physics/physics.md#summary)), but the shadow list has no entry for it (`ShadowList.h`).
- [Object reflections](#object-reflections-and-hand-shadow-on-objects): a physics fragment's reflection is its piece
  in openblack, the default-coloured rock in the original (`Renderer::DrawObjectReflections`).
- Original lighting still missing elsewhere: the particles' `UseDynamicLighting` of chains and meshes, in
  [pending](../physics/particles.md#pending).

- Static shadows against raffclar's bake (#339, not taken: ours already casts buildings, the temple, rocks, features
  and trees from the fixed sun into one island texture, with the exe's caster classes, `ecs::land_prints`). Not yet
  compared texel for texel with the original's block bake: his scan of each flattened triangle into eight samples a
  texel; the temple casting from both sides of its triangles; things lying about casting by their shape; trees
  through their leaves; and parts moved by bones, which cast nothing in his bake. A capture of the original at a
  fixed sun angle is what settles each of these.

### From the model rendering notes

- Shadows of physics objects: the 2×2 filter of the trees and the rebaking of the static shadow when a tree
  or a MobileObject leaves are not yet read.
- Question for the user (implemented "as in the original" as read from the binary; they depend only on how the game
  looked):
  - **Projected shadows** (`shadow_list`): (1) the missionaries' boat in Land 1: did its shadow go diagonally (fixed
    sun) and fall on the dock and the sailors? (0x5E11B6 / 0x5E11BE); (2) between 50 and 80 radii, did the shadows of
    thrown objects lighten in steps or smoothly? (0x80769A); (3) was the shadow of a thrown tree soft and as dark as
    that of a rock?; (4) the shadow of the held orb comes out somewhat darker than in the captures (the code's maximum
    8/15 with alpha 255, **⚠️ approximate**); (5) the dispenser's own shadow is the static one (Abode, fn_008721A0),
    which can barely be distinguished in openblack's shots.
