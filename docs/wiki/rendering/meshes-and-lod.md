# Meshes and level of detail

## Summary

How the original's meshes are drawn: the villagers' ground blobs (two quads from the feet), the reflections of the
hand, the creature and the boats mirrored in the sea, the per-sub-mesh choice of skinned or rigid drawing, the physics
and LOD 0 sub-meshes, the cut by the water plane for sharks and swimmers, everything that faces the camera (sprites,
PSys sprites, the moon, chains) and the meshes stuck to the ground (shields, discs, ground marks, flames). In the v1.42
exe every model draws LOD 1 ([The model level of detail in v1.42](visible-blocks-and-lod.md#the-model-level-of-detail-in-v142)).

openblack draws each of these through one API per family (`graphics::billboard`, `openblack::land_morph`), with the
original's vertices. The animals' blobs and the drawing between turns belong to the beings pages:
[Animals: blobs and mesh](../beings/animal-life.md#animals-blobs-and-mesh) and [Animation](../beings/animation.md).

## The original

### Villager blobs, object reflections and LOD

**Faithful** (checked with a Unicorn emulation of `fn_0081FFF0`).
- **Blobs** (`fn_0081FFF0`, from the Draw of animated objects `fn_00812170`): feet = bones 21 and 18 (end of the two
  legs), on the ground + 0.2; D = O·s − ((O·s)·n)·n with O = (√2, 0, √2), s = scale, n = terrain normal; quad 1 from
  foot 21 with V = D + (P18 − P21)/2, quad 2 symmetric; corners C − 0.02V ± U and C + V ± U with U = 0.2·norm(1, 0, −1)
  (fixed width 0.4); UV (0.0)(1.0)(1.1)(0.1), alpha 1 at the feet and 0 at the tip; mode 6, no Z, two-sided,
  `human_shadow.raw` (byte & 0xF0 as alpha). Not if y ≤ 0.2 (in the water), dead or in the creature's hand. Animals:
  points from their EBone data (2 or 4 quads). `human_shadow.raw` is cut to 4 bits on load (0x81FCDD, [ARGB4444
  textures](textures.md#argb4444-textures)) and `fs_blob` only samples it.
  - The quad is built by `fn_0081FE50`: v0 = C − 0.02V + U, v1 = C − 0.02V + W, v2 = C + V + W, v3 = C + V + U; opaque
    at the feet.
  - The feet come from the drawn matrix **with the slope shear**: `Villager::Draw` (0x51BA74 / 0x51BAD5) calls
    `fn_0051B3D0`, which calls `fn_0051AF00` (0x51B3DA) then `fn_0051B220` (0x51B3E1): MulPre(+0x14, S) with S =
    identity + the shear cells m1 / m7 (0x51B35E..0x51B3B8, `fn_007FAE60` at 0x51B3B8), before the 3D draw.
    `Animal::Draw` does the same (0x51C34C / 0x51C353).
  - `fn_0081FFF0` reads the bone buffer [0xC37D9C] that `fn_00812170` fills with the frame's skinning (0x8125DE);
    without a clip, the mesh's rest locals as `LH3DAnim::SetTransform` 0x83A1D0 skins them (only with obj+0x80 null,
    0x812351..0x8123B4).
  - A blob point: M = Mul(bone, clipping-to-world [0xEA9DE0]) (feet 0x8205FB / 0x820667, EBone points 0x8200F3), and
    for an EBone point MulPre(M, E) (0x8200FF), whose translation reads only E's position; the point is the
    translation row.
  - The terrain normal is `LH3DIsland::GetNormal` 0x803630 at the drawn matrix's translation x, z ([edi+0x38] /
    [edi+0x40], 0x812819..0x81282D; call 0x812859).
  - The blob material is mode 6 [0xEB998C] (`fn_0081FAA0` 0x81FD42).
- **Sea reflections** (`GLandscape::Draw` 0x5E490F): `DrawUnderWater` draws the object mirrored at y = 0, unlit,
  clipped so that only what is above the water is reflected: the hand (0x65A0A0A0) and what it holds, **the creature's
  body** (0x65A0A0D0 + specular 0x30; not what it carries), boats (0xFF303070), physics objects (their colour).
  Sharks and swimming SuperVillagers are drawn **cut below the water** (`DrawCutByPlane`, 0xFF303070); the fish-farm
  fish are sprites. Details in the next two sections and in
  [Fish shoals](../world/sea-coast-and-rivers.md#fish-farm-fish-shoals).
- **LOD**: `g_last_distance` is the linear depth of the sphere centre; S = min((importance + 1) · radius ·
  LevelOfDetail, 100000). In this executable the two loads of LevelOfDetail are disabled with NOP, S = 100000:
  **always LOD 1**, with no fading or impostors (with the design value 0.5, a villager would switch at 11.5 / 33 / 43 u).
- **The creature and the Z-sorter**: the creature is never queued for the Z-sorter. No base creature mesh has the
  mesh flag 0x200 (only the Lion's six variants and the three hand meshes have it), so its body is drawn at once
  ([the creature's body](../creature/drawing.md); [the transparent queue](transparency-and-render-modes.md#the-single-transparent-queue-lh3dzsorter)).
### Physics and LOD 0 submeshes

**Faithful** (L3D loader rule: the `isPhysics` bit of the submesh header and `lodMask`). A submesh with `isPhysics`
(or without bit 1 of `lodMask`, except windows) is only for physics (`BuildFromVertices` 0x7FBAE0 uses it as a hull)
and is **never drawn**. Examples: the miracle dispenser (mesh 557 `SpellSpellCreator`, the one of
`ABODE_SPELL_DISPENSER` in info.dat) has a physics submesh 0 of 16 vertices, a 7-sided prism frustum (radius 1.9 at
the bottom, 1.5 at the top, height 3.1, UV 0, skin 0x4F), and the orb `O_Bibble_up` a smooth sphere. Checked in
captures: the prism does not appear on any dispenser.

- The original's static shadow pass also takes LOD 0 only (`fn_00806DA0`).
- The original's ray/mesh test `fn_00865020` gives the hit triangle's normal norm((V1 − V0) × (V2 − V0)), left as it
  is when zero (0x8652C5..0x865405; the zero test at 0x865396..0x8653C7), and gives it turned along the ray, away
  from the eye (0x8657A1..0x865805; [the hand's mesh test](../hand/animation-and-placement.md#the-hands-mesh-test)).

### Skinned and rigid sub-meshes

**Faithful**. The original skins or not **per sub-mesh**, by bit 0 of the sub-mesh's flags
(`L3DSubmeshHeader::Flags::hasBones`). It does not look at the mesh's `HasBones` flag (0x100) or at the bone count.
- Every primitive rasteriser starts with `mov eax,[0xFA93BC]; test byte [eax],1` on the current sub-mesh, which
  `fn_0080D910` stores at 0x80D928: unlit `fn_00856D40` 0x856D52, lit `fn_0084BA90` 0x84BACA, `fn_0084E200` 0x84E210,
  chroma `fn_0084B7D0` 0x84B7F5, shadow `fn_00850900` 0x850937.
- Bit set (0x856F27..): skinned. Each vertex group {u16 count, u16 bone} (primitive +0x20 count, +0x24 groups) takes
  its bone's matrix, [0xE9FE48] + bone·48. A sub-mesh with **one** bone is skinned as well: that bone's rest matrix is
  not the identity (the only such mesh, `MSH_B_PLAYER_ICON01_PLINTH`, swaps Y and Z and lifts it by 2.753).
- Bit clear (0x856D6E..): rigid, with the object's matrix (0x856D7B [0xEA9EA0]), even inside a boned mesh.
- No bone count is compared anywhere on that path.

A rigid sub-mesh gets the single matrix variant, and inside a boned mesh its draw is given the identity in place of
the bone matrices, so that only the instance's (the object's) matrix moves it. A skinned one gets the variant that
holds its mesh's bones, never the single one. The static shadows baked at the prints step (`ecs::land_prints`) and the
projected shadow casters (`ShadowList.cpp`, `ProjectCaster`) follow the same rule; the loader already used the bit for the bounding
box, the picking positions and the seam blends.

It is the same skinning: the same matrices bit for bit, the same clamp of the bone index, the same instances culled
the same way. Only the order of the draws changes (each sub-mesh for all the instances, rather than each instance
whole), so a mesh is drawn this way only where that order cannot show: drawn at once (not queued for the Z-sorter),
every primitive tested against and writing the depth without blending (mode 2, which nearly every villager and animal
has, and mode 0; not the chroma mode 9 of some birds), and no instance in view with a shadow over it or a read-out
row, which are drawn after their own object. The hand, the creatures and the meshes that move with the land are drawn
as before, an instance at a time, and so is a mesh with fewer than two instances in view. Fragments of two instances
at exactly the same depth are the one case where the order could still tell.

Where it matters, from a scan of every L3D in the Collection (2232 meshes): no mesh of the main game's `Data\` has a
sub-mesh whose bit differs from its mesh's flag, so the main game draws exactly as with the mesh-level rule. The
exceptions are all outside it:
- `MSH_P_PIG_THING` (`CreatureIsle` and `Ultimate` `AllMeshes.g3d`, index 85): sub-mesh 0 skinned with 22 bones,
  sub-meshes 1..22 rigid;
- `CreatureIsle\Data\CreatureMesh\c_shepherd2_{base - Kopie,evil,good}.l3d`: the mesh flag set, 51 bones, the one
  sub-mesh rigid.
(`C_Croc_Evil`, `C_shepherd2_{fat,strong,thin,weak}` and `Fox_{evil,good}` have bones but neither flag: rigid under
both rules.)

### Cutting by the water plane

**Faithful** (done).

- **DrawCutByPlane** (vt+0x11C: animated `fn_00811C70`; static `LH3DStaticObject` vt 0x9A2974 = `fn_0080C050`, it is
  **not** a `ret`: the `ret` `fn_00815F90` is only the base vtable 0x9A2748): plane from `fn_00822560`, (0, −1, 0, 0) →
  what remains is **y ≤ 0**, (0, 1, 0, 0) → y ≥ 0; CPU clipping per triangle (`fn_0081D2C0`), per-vertex light
  `fn_00858BA0`: I = 255·(L·n) with the light 0xF03140 (that of `fn_0084BA90`), I < 0 → 90, otherwise 90 + (255 − 90)·I
  >> 8 (`[0xC39264]` = 90); rgb = colour.rgb·I >> 8, A = colour.A, the object's specular; the material's mode. Used by
  SuperVillagers with `M_P_Swim2`, sharks (`MSH_SHARK_BONED`: the bottom part before the sea in 0xFF303070 and the top
  part in its Draw with table[255]) and the fish puzzle net (Land 4, static). **It does not apply in Land1**. inside the
  Reflection pass the mesh is un-mirrored at y = 0 (`unmirror`; culling goes back to CCW).
  `Renderer::DrawCutByPlane(view, entity, SeaPlane, argb, specular)` and `DrawCutBelowWater` (in the reflection pass,
  before the fish: the entities with `components::CutByPlane`, KeepBelow with the shark's own plane `k_SharkPlane`,
  0x774FF5..0x77501A). The top part is called by the object's owner instead of its normal draw: `CutByPlane::drawAbove`
  (the sharks) makes the normal pass skip that instance and `Renderer::DrawCutAboveWater` draw it with KeepAbove and
  `LandLightTable::GetRaw(255)`, in the main pass after the instanced meshes. `DrawCutByPlane` uses the
  `SkeletalAnimation` pose if there is one. Hook: `OPENBLACK_TEST_CUT=1` with `OPENBLACK_TEST_SEA`.
- The fish puzzle net (FishPlot, 0x829A30..0x829D54): one static LH3DObject of `Data\MISC\Fishplot.l3d` moved to each
  float (SetPosition vt+0x20 + DrawCutByPlane vt+0x11C); its +0x4C is the LH3DMeshedObject default 0xFFFFFFFF (ctor
  0x8164F7), lit per vertex through `fn_0080C050` 0x80C0FD → [0xC37D8C]. The FishPlot ctor never calls SetColour, and
  the only other writers of +0x4C are the draws at vt+0x100 / +0x110 / +0x130 / +0x154, which the net never goes
  through.
- The shark's colour under the water: SetColorSpecular vt+0x2C at 0x775030, then vt+0x11C at 0x775037.

### Objects that face the camera (billboards)

**Faithful**, except what is marked. Everything that orients itself towards the camera goes through a single API, `graphics::billboard`
(`src/3D/Billboard.{h,cpp}`, glm maths only), with one function for each of the original's modes.

**Conventions.**
- LH3D uses row vectors (p' = p·M, fn_0084BA90). Row k is the image of local axis k; in glm it is column k, with the
  same memory.
- LH3D rotations go the opposite way from `glm::rotate`: `SetAngleY(a)` 0x674360 (rows (c,0,s) / (0,1,0) / (−s,0,c)) is
  `glm::rotate(−a, Y)`. The in-place rotations RotateY 0x5198F0 and fn_0086AFA0 mix **rows**: in glm they go **on the
  right** (`M·R(−a)`). UpdateRuleRotatePrincipalAxis 0x6A1150 mixes the components of each row: it goes **on the
  left** (`R(−a)·M`). It is all in `affine` (was `lh_matrix`; [LH matrices](../engine/matrices-and-zoomer.md#lh-matrices)).

**Pass camera.** `CameraFrame::From(camera)` is built once per pass, with the main camera or the
reflected one. It contains:
- the eye, g_camera 0xEA1DB8;
- right / up / forward;
- the view, W2C 0xEA1D28 (`UpdateWorldToCamera` 0x819690, openblack's lookAtLH), its inverse (SetInverse 0x7FB290)
  and the W2C rotation;
- the near plane [0xE839E0], taken from the projection. `Game.cpp` computes it as
  `LandFeature::GetNearClipping` 0x5E2F30 (`near_clipping::NearPlane`, see
  [camera](../camera/game-camera.md#camera)) and makes the projection again whenever it changes;
- the mist matrix 0xEA1C98 = mat3(right, −forward, up) (UpdateCamera 0x819A62..0x819AF3 and fn_00819F50).
- UpdateCamera (its call with `push 1`, 0x819A41) also keeps the normalised forward D in [0xEA1DD4..0xEA1DDC] every
  update; its CPU readers are 0x442CE2, 0x67C701, 0x6EE726, 0x74A8D8, 0x7FEE9D, 0x7FF0E5 and 0x8799F8. D is the third
  column of the W2C (cells 2, 5, 8).

In the reflection pass, the eye and right / up / forward are still those of the main camera, because
`ReflectionXZCamera` only reflects `GetViewMatrix`. On the other hand, the view, its inverse, the W2C rotation and the mist
matrix do come out reflected. In that pass the two groups must not be mixed. Today only `DrawMoon` and `drawSprite` build
a `CameraFrame` in the reflection, and they only use the view and its inverse.

**`Sprite`.** These are the useful fields of the 0x34-byte LH3DSprite, with the default values of SetToZero 0x8404F0:

| Field | Contents |
|---|---|
| +0x00 | position |
| +0x0C | size (half-width) |
| +0x10 | stretch (half-height = size × stretch) |
| +0x14 | angle |
| +0x18 / +0x1C | origin, in world units; **it is subtracted** |
| +0x20 | colour |
| +0x28 | cell (bits 0-5) and 0x40 horizontal |
| +0x30 | cells per row (8) |

`SpriteQuad` returns the 4 corners in the v0..v3 order of 0x840530 and their UVs. `CellUv` does col·(1/n) +
(8/n)·{0, .125, .125, 0} and row·(1/n) + (8/n)·{0, 0, .125, .125} (0xC390CC / 0xC390DC, 8 = [0x8C2C70]), with the cell
& 0x3F. The triangles are {0,1,2}, {0,2,3}. The `components::Sprite` (night lights, fireflies, dust and the hand's
effects) take their `uvMin` from `CellUv(cell, 8)[0]`. Their `Transform::rotation` is not used, because mode A only has
the angle +0x14. A `components::Sprite` names its texture and its alpha by their ids in the texture cache
(raffclar's design), and the draw looks the graphics card's handles up; a sprite whose texture the cache does not have
is not drawn.
- The triangles {0,1,2}, {0,2,3} are at 0x840B47 / 0x840B57; CellUv is computed at 0x8408D3..0x84092F (1/n, k = 8/n).
- A sprite with flag 0x80 is drawn in the smoke material [0xEA1ABC] (0x840C01..0x840C38).
- The smoke material is also used by the boats (PetitNavire 0x5E1328) and the SmokyStuff puffs (`SmokyStuff::Create`
  0x823D21).

| Mode (function) | Original | Maths | Users in openblack |
|---|---|---|---|
| `Screen` (mode A) | `LH3DSprite::Draw` 0x840530, flag 0x40 = 0 | Square in the screen plane at the sprite's depth: it is parallel to the screen and does not turn towards the eye. local x = {−s − ox, s − ox}, y = {hs − oy, −hs − oy} (0x840831..0x8408CF). Local x goes to (cos, −sin) on screen and y to (sin, cos) (0x84071D..0x84082B), that is, a clockwise rotation. Angle 0 = no rotation (0x840770). Order TL, TR, BR, BL. Draws nothing if the depth is ≤ near (`InFrontOfNear`, 0x84055D..0x840585) | PSys sprites (`RendererParticles.cpp`, was `RendererPSys.cpp`: all the SF, TownBelief, FireGraphic), DisappearSmoke puffs (was `SmokyStuff`; `RendererBoat.cpp`) and, on the GPU, the chimney smoke and the `components::Sprite` |
| `ScreenSpriteModel` | the same mode A in `vs_sprite.sc` | T(pos)·Rz(−angle)·S(half-width, half-height, 1). The shader adds u_invView·(model·(x, y, 0, 0)) in the −1..1 plane, with v = 0 at the top: it is `Screen` with origin 0. The near cut is done on the CPU | `drawSprite` (night lights, fireflies, dust, hand effects, temple sparkles, camera markers) and `DrawChimneySmoke` |
| `Horizontal` (mode B) | flag 0x40 (0x8405FE..0x840704): `SetHorozontal` 0x6AA093, `GWater::InitialiseCircles` 0x54BA84, fn_00824740 0x8247EF | Ry(angle), with rows (c,0,s) / (0,1,0) / (−s,0,c), plus the position. x = {−s − ox, s − ox}, z = {−hs − oy, hs − oy} (0x84085D). It does not depend on the camera and has no near cut | boat wake, fish-farm fish, water rings, the horizontal branch of PSys (SF_ManaPathNew, light maps) |
| `PlaneOfMatrix` | `LH3DSprite::DrawSpecial1` 0x840CC0 | The mode B square in the XZ plane of a given matrix, rotated about its local Y if the angle ≠ 0 (r0' = c·r0 + s·r2, r2' = c·r2 − s·r0, 0x840CEF..0x840D82). It does not use the sprite's position | the 7 rings of the influence ripple (fn_00827500, `RendererInfluence.cpp`) |
| `YawToEye` (mode C) | inline code: `TownCentre::DrawPSys` 0x69BE76..0x69BE8A, fn_00466BB0, `TownDesireFlags::Draw` 0x746BFC, fn_00719E90, `ScriptHighlight::Draw` | θ = atan2(eye.z − p.z, eye.x − p.x) + π/2 ([0x8C78D8]); axes `affine::AngleY(θ)` (the rows of SetAngleY 0x674360). Local +Z goes from the eye to the object | the town desire flags (`TownDesireSystem::UpdateFlags`); the influence columns, ShowNeeds and ScriptHighlight do not use it yet |
| `ParticleYaw` (C') | `Particle3DObj::DrawAt` 0x679FD0, FaceCamera +0x4D, 0x67A032..0x67A1C3 | θ = atan2(d.z, d.x) − atan2(r2.z, r2.x), with d = p − eye in XZ; r0' = c·r0 + s·r2, r2' = c·r2 − s·r0; r1 × HeightStretch | PSys meshes with FaceCamera (`Particles/Creators/Mesh.cpp`) |
| `FullSprite` (D) | FaceCameraSprite +0x4C, 0x67A250..0x67A451 | Identity × scale. Then each row (x, y) := (cos φ·x + sin φ·y, cos φ·y − sin φ·x), with φ = π/2 − atan2(d.y, \|d.xz\|) (0x67A367), and then fn_0067A4A0(ψ), with ψ = atan2(d.z, d.x). Local +Y faces the eye and Z stays horizontal | `Mesh.cpp`, after FaceCamera as in the original; no SF activates it |
| `LookAtCentre` | the bubble: fn_00518720, from `OneOffSpellSeed::Draw` 0x518E90 and `DrawOutOfMap` 0x518C50 | d = W − eye, with W = the box centre through the object's current 3D matrix, rotation included (0x518733..0x5187B8): the physics pose while it flies, the hand's while it is held. If \|d.x\| and \|d.z\| are < 1e-4 (the double [0x8C79D8]), d.x becomes ±1e-4 (0x518875..0x5188B4). D = normalize(d), U = normalize(Y − (Y·D)D). The rows (U×D, −D, U) come from inverting with fn_007FB3F0 (0x518B0C). Then M = T(−c)·R·s and translation W − c·R·s (fn_00518B90, fn_00518BF0, fn_0044CF90). After the 1e-4 nudge, d and U are never zero, so the zero test of 0x5188BC..0x5188ED never fires and is not ported | `Magic/Core/OneOffSpellSeed.cpp` (the one-shot orb), with the matrix `ecs::DrawnModel` gives that frame ([the spell core](../magic/spell-core.md), issue #133) |
| `BandToEye` | the power bands: fn_0051A830 (if the byte [0xBE8E8E] = 1, which nobody writes), from `DrawSpellGraphic` 0x51A773 | d = T − eye, with T the band's translation; the same 1e-4 nudge as the bubble; D = d / sqrt(d.y² + d.z² + d.x²), U = normalize(Y − (Y·D)D) with Y = (0, 1, 0) at 0xCC62C0. The column matrix (−D, U, U×D) is inverted with fn_007FB3F0 (0x51AB5A), so its rows are −D, U, U×D. Then M = M·R with fn_0046D9D0 and T is put back. In glm, R·(rotation and scale of the band) | `Worship/SpellSeedGraphic.cpp` (the bands of the ball and the icons) |
| `MoonBasis` / `MoonModel` / `MoonHalo` (E) | fn_0086AC60 and fn_0086A930, read in full | See the moon, below | `Renderer::DrawMoon` |
| `MistBasis` / `MistShrunkSize` (F) | `LH3DMist::Draw` fn_007FA300 0x7FA38F, effect branch 0x7FA483..0x7FA539 | The 9 cells are 0xEA1C98. With the effect, row 0 carries the size and rows 1-2 size / (1 + (k − 1)(1 − \|d.y\|/\|d\|)), with no limit. **⚠️ approximate** 1/\|d\| is computed with `std::sqrt` and not with the InverseSquareRoot table 0x841170. **🔍 inferred** with d = 0 it returns the size | mists (`RendererMists.cpp`, `mists::Submit`) and clouds (`Renderer::DrawCloud`) |
| `ScreenVelocity` (G) | `UR_OrientSpriteWithVelocity` 0x69A790 (0x69A8ED..0x69A94B) and fn_006840E0 (UR_Flocking) | x = w·right, y = w·up (the W2C rotation); `SetAngleY(atan2(−y, x) + π/2)`. With `Screen`, the sprite's +y lies along the on-screen velocity | `Particles/Rules/Orient.cpp`, `Particles/Rules/Flock.cpp` |
| `RibbonSide` / `RibbonHalfWidth` (H) | fn_0067B3F0 (reads g_camera at 0x67B4BA) | side of each end of the segment = (eye − joint) × (tail − head) (0x67B86C..0x67B924: the view from that joint, first the head and then the tail), which is the same direction as normalize(cross(normalize(segment), normalize(joint − eye))). Vertices = joint ± side·(+0xC of the joint)/\|side\| (0x67B9E6..0x67BA70). That +0xC is the PSR scale (ChainJoint::DrawAt 0x679E9A), the same that a sprite takes as its half-size, so the **half-width is the scale** | `Graphics/RendererChain.cpp` (lightning, forks, gesture trail) |
| `VolumeBlendBasis` (I, was `VolBlend`) | `RenderParticleVolBlendMesh::DrawAt` 0x67CCB0 ([0xC029C4] = 1) | a = normalize(row 0), d = normalize(eye − p), b = normalize(a × d), c = d × b; rows (c, b, d) × scale | nobody (no SF uses ParticleVolBlendMeshCreator) |

**PSys sprites** (`Particle3DSprite::DrawAt` 0x67AE80):
- size = the PSR scale, with a minimum of 0.0001 (0x67AEAF);
- angle = atan2(M[0][2], M[0][0]) of the PSR, except with IgnoreRotation (0x67AF6B..0x67AF87, [0xC029A8] = 1);
- origin: ox = OriginX·size and oy = OriginY·size·stretch (CreateSprite 0x6AA0A8..0x6AA0BD and 0x67AEC4..0x67AF2D);
- CentreAtBase: y += stretch·size·0.5 (0x67AFB4, [0x8AA3B4]);
- cell = (FileOffset + frame) & 63.
- The 0.0001 is [0x8BF518] (0x67AEA4..0x67AED7).
- [0xC029A8] is 1 in the binary and only read there, so the roll is always applied.
- The cell is put in the flags' low 6 bits at 0x67B0A2..0x67B0BA.
- Their material: CreateMaterial(6) + SetMaterialProperties 0x57E120 (`fn_006AA030` 0x6AA052 / 0x6AA05F): mode 13 or 6
  (12 or 5 with MaterialUpdateZBuffer), Z test on, two-sided.

All of that also applies with SetHorozontal. The `FireGraphic` flames use the origin of their shared sprite 0xDA09E8:
+0x1C = −2·size (0x732382..0x732395), that is, `SpriteOriginY` = −1, and not CentreAtBase. That way the base stays at the
point along the screen's "up".

**Moon (mode E).**
- Basis (fn_0086AC60 0x86AC67..0x86AEBD):
  - v = the position in the camera, n = normalize(v), t = normalize(n.z, 0, −n.x), u = n × t;
  - it is brought into the world with SetInverse(W2C) (0x86AE64 / 0x86AE6F) and × [0xFA2750] = 4.0 (0x86A3F4, which overwrites the
    3.0 of 0x86A3D6);
  - local +Z goes from the eye to the moon and X stays horizontal in the view.
- Mesh: that basis, tilted with fn_0086AFA0(α = [0x9A3BF0] = −0.1309), then RotateY(phase + π) 0x5198F0, then
  × 0.65 [0x8AC420]. In glm it is Rz(+7.5°)·Ry(−(phase + π)).
- Halo (fn_0086A930):
  - corners p ∓ 500(r0 + r1) and p ± 500(r0 − r1) (500 = [0x8C78EC], with the rows already × 4);
  - UV (0.25, 0.25), (0.49375, 0.25), (0.25, 0.49375), (0.49375, 0.49375) (0xEDC304; v0 = bottom left);
  - indices {0,1,3, 3,2,0} (0xEDC310).
- Reflection (fn_0086B010 0x86B662..0x86B69C): the second call, at (x, −y, z) with [0xFA2774] = 1, only draws its halo
  (0x86AC0F skips the moon object). The reflected moon is the DrawUnderWater (vt+0x118) of the first one. With openblack's
  reflected camera:
  - the halo is built with the reflected view;
  - the moon uses the main view (`view·mirror(y)`).

**Chains (`RibbonSide`, mode H).**
- `fn_0067B3F0` normalises the side with InverseSquareRoot (0x67B943); the fsqrt at 0x67BA57 then takes out the
  table's error. An exactly zero side stays zero (0x67BA04..0x67BA39), so a zero-length segment collapses to its
  joint.
- Where two segments meet, both pairs of vertices move to their midpoints (0x67BD2D..0x67BE82); [0xD4EC14], 0 here,
  would copy the previous tail instead.
- UseDynamicLighting (not ported) multiplies the chain colour by clamp(0.6 + 0.4·n·L) (0x67BB10..0x67BCCF);
  `ChainJoint::DrawAt` 0x679E80 also jitters the joints (not ported).
- The chain material (`fn_006AA800`) goes through SetMaterialProperties 0x57E120 at 0x6AA84A.
- The look-at basis of a model that faces the eye (`fn_00518720`) is built from the static up vector (0, 1, 0) at 0xCC62D0: D = normalize(W − eye), U = normalize(Y − (Y·D) D), the matrix with columns (U × D, −D, U) inverted in place (0x518B0C); after the 1e-4 push of d its zero test cannot fire.

### Meshes stuck to the ground (land_morph)

**Faithful**, except what is marked. Everything that conforms to the terrain goes through a single API, `openblack::land_morph`
(`src/3D/LandMorph.{h,cpp}`), and on the GPU through a single include, `assets/shaders/land_altitude.sh`. Every height is
`LH3DIsland::GetAltitude` 0x803090: on the CPU `LandIsland::HeightAt` (exact, 16.16), on the GPU `LandAltitude` (the same
logic in float: `split` diagonal and flattening next to the sea), at the point's world x, z (`fn_004427B0` /
`fn_00653150`: x·65536·0.1 truncated, [0x8AC408], [0x8AC404]). In the original there is no single routine: there is one
height and four algorithms that apply it.

| # | Algorithm | Original | When | API | Users in openblack |
|---|---|---|---|---|---|
| A | Cut the mesh by the terrain and raise each vertex `y += H(v) − H(origin)` | `fn_00686980` + cut `fn_00686D90` | once on creation (0x6867F9), only with `DoRaiseAboveLandscape`; without the per-frame "breathing" (0x686805) | `SplitByPlane`, `CellPlanes`, `RaiseAboveLandscape` | `Particles/Rules/SurfRevol.cpp`: `SF_TeleportVortex`, `SF_SpellDispenserVortex` |
| B | *Melting*: a delta per vertex `(H(v) − H0) / scale`, in model space, along the local Y | `UpdateMelting` 0x8168F0 + morphable Draw 0x80E550 | on creation (`Snapshot`); `PhysicalShield` on every draw (`Live`) | `components::MorphWithTerrain{mode}`, `Melting`, `MeltingDeltas`, `LandMelting` (GPU), `ObjectProgram` | vs_object_hm_instanced: morphable buildings, fields, piles, BigForest, physical shield, ark and dinosaur, ground marks |
| C | *Bake*: the same delta written into the vertices | FragMesh `fn_007F72B0`; ClampToLandscape `RenderParticleGJMesh::DrawAt` 0x67C313; MeltBorder 0x816350; citadel vt+0x208 `fn_00882B10` | FragMesh when breaking; ClampToLandscape every frame, all primitives, no cut | `Bake`, `Raised`, `BakeAgainstY` | `Physics/DamageMesh.cpp`; `SurfRevol.cpp` (`SF_LandscapeVolcano*`, `SF_LandscapeVortex*`); flames of a morphable object (`ECS/Fire/FireGraphic.cpp`); the totem point over the town centre (`GetExtraPos` 0x80FF20, `AbodeArchetype.cpp`); the temple's outside (`TempleExteriorMorph::BakeToLand`) |
| D | Geometry made on the ground, `y = H + constant` | blobs `fn_0081FFF0`; InfluenceCircle `fn_008265F0`; leash `fn_008491B0`; creature quads `fn_0081F360` | on creation / every frame | `OnGround`, `k_BlobLift`, `k_LeashRibbonLift`, `k_CreatureQuadLift`, `InfluenceCurtain` | blobs (`Renderer::DrawGroundBlobs`); picking of morphed objects (`HandPlacement.cpp`); the influence border (`ECS/Influence/InfluenceCircles.cpp`) |

**A, cut and raise.** `fn_00686980(M, mesh)` takes the mesh into the world with the atom's matrix (`fn_00673E40` at
0x6867E8: the local frame of `fn_00673DB0` in the hierarchy, `fn_006752D0`), gets the box of all the primitives
(`fn_006C99E0`: centre = (max + min)·0.5 and half-axes = (max − min)·0.5) and cuts **only the first primitive of the
first group**:

| Step | Detail | Address |
|---|---|---|
| Indices | x0 = −1 − ftol(−0.1·min.x), x1 = 1 − ftol(−0.1·max.x), likewise in z (= ⌊min/10⌋ − 1 .. ⌊max/10⌋ + 1 with x ≥ 0) | 0x6869EF..0x686A41, [0x8C7B10] |
| Planes | x = 10i (n = (1,0,0)), z = 10j (n = (0,0,1)), x + z = 10k and x − z = 10k (n = (s,0,±s), s = 1/√2 in double, through (10i, 0, 10·z1); i from x0 − dz to x1 and from x0 to x1 + dz, dz = z1 − z0) | 0x686A53, 0x686AF9, 0x686BDB, 0x686C8B; [0x8AB414], [0x8AC410] |
| Plane d | −n·p with `fn_00453F50` ((z z' + y y') + x x') | 0x686AB2..0x686ACC |
| Cut | dist = ((y ny + z nz) + x nx) + w; dist ≤ 0 counts as negative; if the three signs are equal there is no cut; A = the first vertex whose sign is the product of the three | `fn_00686D90` 0x686E3B..0x686F9A |
| New vertices | t = −dA/(dO − dA); position, UV and normal = A + t(O − A) (the normal not renormalised); each colour byte cA + ((cO − cA)·ftol(255t) >> 8); diffuse, specular and normals only if there is one per position | 0x686FEB..0x68732B, [0x8AB270] |
| Triangles | n1 (edge AB) and n2 (AC) at the end; tri := (A, n1, n2), (B, C, n2) and (B, n2, n1) are added; only the triangles that already existed are tested | 0x687386..0x687413 |
| Raise | H0 = H(M+0x24, M+0x2C); y = (H(v) − H0) + y in the first primitive | 0x686D01..0x686D55 |
| Back | the inverse of M (`SetInverse` 0x7FB290) | 0x686D61..0x686D78 |

- A GJ mesh primitive is 0x94 bytes: +0x08 positions, +0x1C diffuse ARGB, +0x30 specular ARGB, +0x44 UVs, +0x6C
  triangles, +0x80 normals. `fn_00686D90` cuts the colours and normals only when there is one per position
  (0x686DAA..0x686DC3), and always cuts the UVs.
- The box of `fn_006C99E0` starts with min = 1e7 (0x4B189680) and max = −1e7; centre and half are computed at
  0x6C9A3C..0x6C9AF9 and turned back into min / max at 0x6869BE..0x6869EB.
- `fn_00686980` exits with no group (0x68699F) or no primitive in the first one (0x6869B8).
- The distances go into the static buffer 0xD4E730; each lerp step is stored to a float (`fn_0044CF90`, the LHPoint
  ctor 0x442700, `fn_004605F0`; UVs `fn_0067FEC0` / `fn_0067FEE0`, normals `fn_0044E9F0`); the colour weight is stored
  as a byte (0x687194), the channels lerped at 0x687185..0x68724D and the specular at 0x68726A.
- The plane given to `fn_00686D90` is (n, −n·p) (`fn_0045A7D0`, `fn_00453F50`, fchs); s = 1/sqrt(2.0) is formed at
  0x686B93..0x686BC3.
- The raise order (H(p) − H0) + p.y is the same at 0x686D3D..0x686D4E, 0x67C373..0x67C383 and 0x73235A..0x732366.

Since `GetAltitude` is linear within each land triangle and the cuts follow the cell edges and their two diagonals
(whatever the `split` bit), the cut mesh follows the terrain **exactly**. **⚠️ approximate** The frame is the one
`surf_revol::Collect` uses when drawing (rotation · baseScale · ruleScale, without the Y stretch of `fn_00673DB0`):
the M of `fn_00673E40` is taken as the drawn PSR of `fn_00673EA0`, and `Collect` does not apply the stretch. Both
users have stretch 1 (no `StretchVertically` nor rules that change it). Before, each vertex was raised when drawing
and with no cuts, so the disc's chords cut through the folds of the terrain. `HeightAboveLandscape` and
`RaiseAboveLandscapeRadius` are not read by anyone (0x6B30EF, 0x6B312A).

**B, melting.** `UpdateMelting` 0x8168F0 exits if there is no delta buffer ([+0x80] == 0) or if vt+0x1B0 ≠ 0 (0x7F9BF0
= 0 in both morphable vtables); a0 = H(+0x38, +0x40), inv = 1/[+0x44]; for each vertex of each submesh, w = v·M + pos
and delta = (H(w) − a0)·inv (0x816A5E..0x816A77); a0 and 1/[+0x44] at 0x816915..0x816977 (the inverse at 0x816964);
each vertex x = ((z·m2c + y·m20) + x·m14) + px and z = ((x·m1c + z·m34) + y·m28) + pz (0x8169E1..0x816A18). The Draw
0x80E550 adds it to the model's y before the matrix. With the column (0, s, 0) it is exactly `y += H(w) − H(origin)`,
as before. The only tilted morphed object is the ripe field: its sway (`Field::Draw` 0x528570) shears column 1 along
z, so its vertices rise H − H0 and also shift 1.75·sway·(H − H0) in z, as in the original (before they only rose). The
morphable classes (Get3DType 1 / 8) are in `MorphWithTerrain.h`. The physical shield is one (Get3DType 0x72CE50 = 1);
the magic one is not (0x72C340 → the static one).
- **Snapshot / Live ⚠️ approximate.** `MorphWithTerrain::mode` records when the original takes the deltas: `Snapshot` on
  creation, `Live` on every draw. `Live` is only `PhysicalShield`: `CallVirtualFunctionsForCreation` 0x72CD23,
  `SetUpPhysOb` 0x72CEB8 and `DrawShield` 0x72D01E, after interpolating the matrix and the scale. The shader computes them on every
  draw for both: frozen deltas would be needed per instance and per vertex, and instances share the
  mesh. They only differ if the land changes after the object is created, and in openblack it only changes in
  `FlattenLandUnderTemple` (`CitadelArchetype.cpp`), while the map is loading.
- **One program.** `land_morph::ObjectProgram(shaders, morph, pass)` is the only choice between `ObjectInstanced` and
  `ObjectHeightMapInstanced` (and their `Shadow`): the main pass, the blended one, the object reflection and the hand
  shadow on objects.
- **Cut by the plane.** The `DrawCutByPlane` of the morphable vtables (vt+0x11C) is 0x80BA50, a `ret`: a cut morphable
  object is not drawn. `Renderer::DrawCutByPlane` exits likewise. Today no morphed object carries `CutByPlane` except the
  food pile of `OPENBLACK_TEST_SEA=...,pot` with `OPENBLACK_TEST_CUT=1`, which is no longer drawn cut.

**C, bake.** `Bake` / `Raised` do `y = (H(v) − H0) + y`, the original's float order:
- FragMesh `fn_007F72B0` (v.y − (H0 − H) on each FragVertex, 0x7F7510..0x7F7563; only if `IsStaticMorphable`,
  vt+0x1F4 → [0xE920E8], 0x7F6FF5): `damage::Mesh::FromEntity`.
- **ClampToLandscape** (`RenderParticleGJMesh::DrawAt` 0x67C313..0x67C38C; the byte +0x22 set by 0x686432 from the
  rule +0x6C): every frame, all primitives, **no cut**, with H0 at the position of the drawn matrix. Only
  the 5 `SF_Landscape*` carry it (terrain volcano and vortex, `FunctionIndex` 1-3). It is in `surf_revol::Collect`.
- The flames of a morphable object (bit 0 of +0xB5, `fn_00732220` 0x7322A9..0x7322D2 and 0x73230A..0x732366): the same
  delta on each flame.
- MeltBorder 0x816350 (only 0x49D303) and the citadel's `fn_00882B10`: `BakeAgainstY`. `fn_00882B10` is the
  alignment morph's blend (`ProcessAlignement`, vt+0x208), and its bake (`v.y − (pos.y − H)`, 0x882DBE..0x882DD7, the
  world x, z summed in the melting's order) lays every blended vertex on the land. It is called from
  `Citadel::Process` at 0x462FE2 and from `InitTemple` at 0x8829AF. The temple's outside uses it
  ([the temple's outside: alignment and size](../magic/temple-and-citadel.md#the-temples-outside-alignment-and-size)); MeltBorder is not ported.

**D, on the ground.** `OnGround(ground, xz, k) = H + k`. The villager and animal blobs put each point at
H + [0xEAA3C4] (= [0xC381DC] = 0.2, from 0x81E350). The picking in `HandPlacement` puts the origin of a morphed object at
H + the pile's sinking: **⚠️ approximate**, because the drawn origin is the stored y; it makes no difference as long as the stored
y is that one. `InfluenceCurtain` is the curtain of `fn_008265F0`, read in full:

| Datum | Value | Address |
|---|---|---|
| Segments N | clamp(ftol(2πr·0.05), 8, 250) | [0x8AB210], [0x8C7BD4], 0x826659..0x826670 |
| Vertices | 3 per segment, plus 3 that close the ring at angle 0: (cos·r + cx, (H − H0) + H0 + {0, 20, 40}, sin·r + cz) | [0x8C7658], [0x8CF300] |
| U | += (1 − ftol(2πr·(−1/111))) / N | [0x9A3930] |
| V | += min(ftol(r/60), 6) / N; rows v, v + 0.2 and v + 0.4 | [0x8C5818], [0x8AB244], [0x8C7A44] |
| Colour | the player's, [0xEA9EFC + 4·player] | 0x82695B |
| Triangles | (b, b+3, b+4), (b, b+4, b+1), (b+1, b+4, b+5), (b+1, b+5, b+2), b = 3i | 0x8269DA..0x826A34 |

The curtain's caps: 250 segments at 0x826666 and 6 turns at 0x826818; the u step 0x826760..0x8267A8, H0 + 0 / 20 /
40 at 0x8267AC..0x826805, the v step 0x826809..0x826842, the angle 2πi/N at 0x826851..0x826878, the closing column at
angle 0 at 0x826A57..0x826B93.

The leash (`fn_008491B0`: edges at H + 0.1, [0x8AB22C]; used at 0x8497E8 / 0x849818) is ported: its shadow strip lies
on the land at `leash_rope::k_ShadowLift`, checked against `land_morph::k_LeashRibbonLift` (`Graphics/LeashDraw`,
[drawing the leash](../creature/leash-rope.md#drawing-the-leash)). The creature quads (`fn_0081F360`: H + 0.15, [0x8CF110]; used at
0x81F778) only have their constant: they are not ported.

**Ground marks** (`ecs/GroundMarks`). `fn_00825240` (list 0xEB9A00) is an `LH3DObject::Create(1)` of mesh
0x251 (`MeshId::TreeRootsPile`). It melts into the land once (0x8252C3), lasts 15000 ms and fades out in the last second:
`fn_00825350`, from 0x5E6197, sets alpha ftol(life·0.255) with life ≤ 1000 ([0x9A2BA8]), subtracts `g_game_time_inc` and at 0
deletes it with `fn_00825300`. It has two users:
- the crater of an uprooted tree (`fn_008251F0` ← `fn_0074BD20`, scale (x + z extent)·scale·0.3, [0x8AB23C]);
- the mark of an explosion on dry land (`fn_008251C0` ← `UR_Explosion::InitCollection` 0x67E395: rotation
  `PSysFloatRand(2π)` with `SetPosition` 0x423140, scale [0x9357D4] = 8).

The list is emptied with the map (`ClearAllStuff` 0x82AED0, from `GGame::ClearMap` 0x552F22): `ground_marks::Clear` in
`Game::LoadMap`. **⚠️ approximate** `g_game_time_inc` is an integer of ms that is subtracted as is (0x8253B1..0x8253C0); the
openblack step is float, so the fraction waits for the next frame.

Not ported: the `SmokyStuff::Create(pos, 1, 1, 0xFFFFFFFF)` of 0x8252EB, because SmokyStuff mode 1 is not
ported (the crater keeps the grip dust).

**Not part of this family.** Shadows on the land (`ShadowInfo`, `fn_008745A0` / `fn_00874850` / `fn_00878350`:
they redraw the land itself; see [Shadows of physics objects](shadows-and-reflections.md#shadows-of-physics-objects)), the
foundations (`GetAltitudeFondation` 0x63ABC0: a rigid sinking), the flattening under the temple (0x882730: it edits the
land), the mist (`LH3DMist`, static Draw) and flat sprites (flag 0x40 of `LH3DSprite::Draw`).
- The box that raises a mesh above the land is the box of every primitive's positions (`fn_006C99E0` → `fn_006C98D0`).

## openblack

Status: **done** for the blobs, the physics and LOD 0 sub-meshes, the skinned and rigid sub-meshes, the cut by the
water plane, the billboards (`graphics::billboard`) and the meshes stuck to the ground (`openblack::land_morph`);
what is not ported is listed in [Pending](#pending).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Villager blobs, object reflections and LOD**
  - openblack: `Renderer::DrawGroundBlobs`
  - LOD: 🎯 chosen difference (the user, 2026-10-15): the villagers', animals' and birds' high meshes
    (`components::DetailMesh`) are drawn only at detail level 6, the top one (`detail_level::DrawsHighMeshes`, chosen
    by `ecs::drawn_mesh::Of` for both the draw layout and the rows written each frame); other objects draw their
    first level. The options box's Maximum Detail entry sets level 6 (the user, 2026-10-20 and 2026-10-21;
    [the options box's detail control](visible-blocks-and-lod.md#openblack)).
- **Physics and LOD 0 submeshes**
  - In openblack all paths skip it: `Renderer::DrawSubMesh` (everything that goes through `DrawMesh`: objects,
    sorted transparents, PSys mesh atoms, reflections, the hand, boats, sharks, fish; only the mesh viewer paints it
    with `drawAll`), the static shadow baked at the prints step (`ecs::land_prints`), projected shadows (`ShadowList.cpp`), `FragMesh`
    (building pieces), `PartialBuild` and picking (`L3DMesh::RayIntersect`).
- **Skinned and rigid sub-meshes**
  - openblack: `bone_budget::ForSubMesh` picks the program variant of each sub-mesh in `Renderer::DrawSubMesh`.
  - openblack draws the posed villagers and animals of a mesh in the main view in one instanced draw (the bone
    palette, `Graphics/BonePalette.h`): every instance's bones, the pose it is drawn with or its mesh's own, go into
    one RGBA32F texture each frame, four texels a bone (its matrix's columns), and `vs_object_palette_instanced`
    reads them where its instance says they start, in place of the bone matrices uploaded as uniforms.
- **Cutting by the water plane**
  - openblack (with [sea_pass](../world/sea-coast-and-rivers.md#the-under-sea-pass)): `L3DMeshSubmitDesc::sea =
    sea_pass::Cut(plane, argb, specular, pass)` → mode 4 of `u_objectLight` in `vs_object` (the same integer
    arithmetic; I is stored with `fistp` at 0x858CDF: round to nearest, halves to even, not truncated; + the
    specular in w) and per-fragment discard in `fs_object` (`SeaPlaneDiscard`) instead of CPU clipping.
- **Meshes stuck to the ground (land_morph)**
  - openblack does it in `ZR_SurfRevol::MakeSurface`, when creating the surface, with the atom's frame in the
    hierarchy (`Effect::LocalToGlobal` / `GlobalToLocal`), and leaves it in local space: when drawing it moves and
    scales with the atom, like the original's local deltas.
  - In openblack it is done by `LandMelting` in vs_object (programs `ObjectHeightMap*`): in the world it is column 1
    · (H(w) − H(origin)) / scale, with column 1 of the drawn matrix (its up axis) and the scale of column 0
    (rotation times uniform scale; the sways only touch column 1).

### Billboards in openblack

What changed against openblack's earlier billboards:

- The rotation of PSys sprites went the wrong way. It was compensated by two rules that also rotated the wrong way:
  - `UR_OrientSpriteWithRandomAngle` (0x6A2100; DefineProperties 0x6AC660: +0x20 RandomAngle, +0x24 DefaultAngle)
    now does SetAngleY((1 − PSysFloatRand(2))·RandomAngle + DefaultAngle) at every step (0x6A2187..0x6A21AC);
  - `UpdateRuleRotatePrincipalAxis` is now `rotate(−dt·AngularVel)` (Z 0x6A116B, Y 0x6A1218, X fn_006A12F0). The
    original also rotates the 4th row of the AtomCore matrix (+0x68..+0x70), which SetAngleY 0x674360 sets to zero;
    it is not ported (🔍 inferred: it is not used for drawing).

  Since both are fixed, the meshes that use the second one also rotate like the original.
- The origin was added; now it is subtracted.
- CentreAtBase added the whole h·s; now it adds h·s·0.5.
- The horizontal PSys branch had no origin, IgnoreRotation or CentreAtBase, and had the Vs reversed: v0/v1
  go to −hs − oy (0x84085D..0x840897).
- Mode A's near cut was missing (0x84055D..0x840590).
- The moon used the view plane and had the signs of the tilt and the phase inverted.
- The halo's Vs were reversed.
- `MistShrunkSize` no longer clamps \|d\| to ≥ 1. It only changes anything with a mist or a cloud less than 1 m from the eye.

All of these are verified in the binary. Confirmed by the user against the original (2026-10-02): the moon, the
width of the lightning ribbons (half-width = the PSR scale, fn_0081C780), the orb's ring that sometimes covers the
bubble depending on the animation, and that the bubble no longer flickers when its atlas starts over.

Everything else gives the same vertices as before: wake, fish, rings, DisappearSmoke, mists and clouds beyond 1 m,
FaceCamera, chains and the bubble.

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/L3DMesh.h`: `GetEBonePoint0`, `GetNewEntrancePoints`, `RayIntersect`, `UpdateSkin`.
- `src/3D/L3DSubMesh.h`: `GetCollisionNormals`, `GetSkinBones`.
- `src/3D/LandMorph.cpp`: `LerpChannel`, `PlaneThrough`.
- `src/Graphics/Renderer.cpp`: `BlobBones`, `BlobPointExact`, `Renderer::DrawHumanShadows`, `SkeletalAnimation::locals`.
- `src/Graphics/Renderer.h`: `DrawHumanShadows`.
- `src/Graphics/RendererChain.cpp`: `ChainCreator::SegmentUv`, `Renderer::DrawPSysChain`.
- `src/Graphics/RendererCut.cpp`: `Renderer::DrawCutBelowWater`.
- `src/Graphics/RendererFallingSpell.cpp`: `Renderer::DrawFallingSpellOverlay` draws the falling spell's sparks far
  to near in one buffer (each spark one Z object in the original, two triangles {0,1,2}, {0,2,3}) and its smoke in the
  smoke material (`render_modes::materials::k_Smoke`, the sprites' flag 0x80).
- `src/Graphics/RendererParticles.cpp` (was `RendererPSys.cpp`): `Renderer::DrawParticleSprites` (was
  `Renderer::DrawPSysSprites`) builds each PSys sprite as [PSys sprites](#objects-that-face-the-camera-billboards) says:
  size, origin, angle, CentreAtBase, cell and material.
- `src/ECS/GroundMarks.h` (`ground_marks`): `Create` makes a ground mark of `MeshId::TreeRootsPile`,
  `CreateExplosionMark` the explosion's (a random turn, scale 8), `Update` ages and fades them, `Clear` empties the
  list with the map.

### Test hooks

- A trap: `OPENBLACK_MOUSE_AT` on an edge of the window activates edge scrolling and moves the
  camera; use interior points.
- The scenes for billboards, the cut and the meshes stuck to the ground are listed in
  [Test hooks](model-lighting.md#test-hooks); the full list is in
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

### Billboards: inferred and approximate

- 🔍 inferred The w vector of UR_OrientSpriteWithVelocity before 0x69A8ED.
- 🔍 inferred With IgnoreRotation, +0x14 keeps the 0 of SetToZero 0x8404F0, each creator having its own sprite
  (`CreateSprite` 0x6AA070 calls fn_006A84C0, not read, taken as `LH3DSprite::Create`).
- 🔍 inferred The drawing of light maps as a horizontal sprite.
- 🔍 inferred The 4th row of the AtomCore that UR_RotatePrincipalAxis rotates is not used for drawing.
- ⚠️ approximate The near of `CameraFrame` lags up to 0.01 behind [0xE839E0].
- ⚠️ approximate `MistShrunkSize` uses `std::sqrt`, not the 128-byte InverseSquareRoot table 0x841170
  (MakeInverseSqrtLookupTable 0x8411D0, plus a Newton step at 0x8411B0..0x8411C2).
- ⚠️ approximate The steam and smoke of fire come out centred. In the original they inherit the oy = −2·size of the last
  flame drawn with the sprite 0xDA09E8, because 0x7323F0..0x7325C5 do not write +0x18 / +0x1C. fn_00732200 draws, for
  each fire, the flames, then the steam and then the smoke, so it is that fire's oldest flame, or that of the previous
  fire if it has no flames. So in the original they come out 2 times that size higher. Porting it would require an
  origin per atom in `particles::manager::DrawAtom` and knowing the order of the fires.
- Not ported, though read:
  - the clamping to [0, 639] × [0, 479] of the vertices of an unclipped sprite (0x840A47 / 0x840A85);
  - the fn_007A8DB0 path with [0xEA9EB4] ≠ 0.

### Meshes stuck to the ground: approximate and not ported

- ⚠️ approximate Snapshot and Live are the same on the GPU (above).
- ⚠️ approximate `LandAltitude` in float versus the 16.16 arithmetic of 0x803090.
- 🔍 inferred The fire's +0x98 / +0xA0, which gives the flames' H0, is the object's position.
- Not ported: the temple entrance and the half-built temple (0x4676CD, `DrawPartialyBuilt` 0x816AD0), the scaffold
  (`Scaffold::SetPhantomMesh` 0x6E8B03), the other users of `GetExtraPos` 0x80FF20 (vt+0x1CC, no direct
  calls; the only ported one is the totem, which does the same sum, (H(p) − H(pos)) + p.y, 0x8100B7..0x8100D0), and the
  curtain, the leash and the quads of D.

### From the model rendering notes

- Billboards:
  - the original faces the influence columns, the desire flags, ShowNeeds and ScriptHighlight with `YawToEye`, and
    the HelpDude ones with the basis (R, U, D) (HelpDude::Update1 0x5BE302); openblack's users still do their own
    facing maths (queued fix);
  - the oy inherited by the fire's steam and smoke;
  - `MoonModel`: "The moon mesh's matrix, fn_0086AC60 0x86AEC6..0x86AF97: the halo's (fn_005FEDA0), tilted by
    fn_0086AFA0(alpha)". What `fn_005FEDA0` is (a copy of the halo's basis?) is not explained;
  - `VolumeBlendBasis`: "fn_00460710 then runs on c: 🔍 inferred a normalisation, a no-op on that unit vector";
  - PSys sprites: "🔍 inferred +4 alpha = 1: with 0 they would be 8 / 3, the same states".
- Skinned and rigid sub-meshes: read, every sub-mesh's bone numbers are local to its own block of the frame's bones (the
  pointer moves on by each sub-mesh's `numBones`·48, `fn_0080C790` 0x80C8D1). Open: whether openblack's loader offsets
  each sub-mesh's bone numbers to match; openblack gives every skinned sub-mesh its mesh's whole bone list, which is the
  same for a mesh with one skinned sub-mesh. A skinned sub-mesh inside a mesh without the `HasBones` flag stays rigid in
  openblack, as its callers give such a mesh no bone matrices (no file has one). (not read) whether the plinth's one
  bone is ever posed, and whether its mirrored bone (determinant −1) changes which faces the original's software culling
  keeps. The rule is shown by unit tests only: no testbed scenario can show it, as the meshes where it differs are not
  in the main game's data.
