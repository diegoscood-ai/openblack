# Vortexes and the tornado: objects swallowed, carried and flung

Code: `src/ECS/Vortex.{h,cpp}` and `src/ECS/Components/LandscapeVortex.h` (the vortex
objects), `src/ECS/Physics/ParticleCarriedObjects.{h,cpp}` (was `PSysObjects`; what a particle system carries) and `src/Particles/Rules/Storm.cpp`
(the tornado's rule). The tornado as a miracle (the storm spell, its funnel, dust and pick-up) is in
[miracles.md](miracles.md#the-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680); this page covers the objects.

Progress: [Tornado](../progress/miracles/tornado.md), [Portals between lands](../progress/story/portals.md) (what our tree does of it, row by row).

- [The vortex objects](#the-vortex-objects-landscapevortex-magicvortexcpp-0x5fd2d00x600460)
- [States and fades](#states-and-fades)
- [Taking objects in](#taking-objects-in-the-in-vortex)
- [Bringing them out](#bringing-them-out-the-out-vortex)
- [The land under a vortex](#the-land-under-a-vortex)
- [The objects a particle system carries](#the-objects-a-particle-system-carries-tornado-and-vortex)
- [Drawing](#drawing)
- [What is ported and what is pending](#what-is-ported-and-what-is-pending), [Sources](#sources)
- [Pending](#pending)

## The vortex objects (LandscapeVortex, MagicVortex.cpp 0x5FD2D0..0x600460)

- Three classes, made by `LandscapeVortex::Create(pos, type, 50.0)` 0x5FE6E0 from the CHL `CREATE Vortex` (GScript
  0x6F171B): **In** (0x104 bytes, ctor 0x5FD7F0), **Out** (0x134, 0x5FDE20) and **Volcano** (0xEC, 0x5FD740). Any
  other type makes nothing.
- `CallVirtualFunctionsForCreation` 0x5FEE30 reads the type's **GVortexInfo** row (info.dat, table 0xD38248, stride
  0x40; openblack `InfoConstants::vortex`):
  - the initial state (info +0x20 -> +0xE4);
  - the mesh 0x230 scaled by `baseScale` (+0x28);
  - up to four particle systems: ObjectMover +0x18 (the one that pulls objects), PreLandscape +0x10 (on the ground),
    PostLandscape +0x14 and LightMap +0x1C;
  - for the Volcano only, a looping sound (0xAC).
  The `maxToCreate*` fields are not read: the Out's villager count is a literal 30 (fn_005FD480).
- CHL: `VORTEX_FADE_OUT` 257 (GScript 0x6FD8C0 -> StartFadeOut 0x5FFFD0) and `VORTEX_PARAMETERS` 328 (0x6FE090:
  SetTown 0x5FDFE0 and SetFlockParams 0x5FDFF0 of an Out: the town the arrivals join, where the arriving animals settle
  and the flock's two word fields).
- The global list [0xD38220] (linked through +0x88) is walked once a frame (fn_005FF310 from GLandscape::Draw
  0x5E4E91) and once a turn (fn_005FF330 from PSysGlobal::GameLoopEnd 0x68F5B9).
- A vortex cannot be picked up (ValidForPlaceInHand 0x5FF300 = 0) and is not an effect receiver. In the physics it is
  never a flying body. Only the In is hit by thrown objects: InteractsWithPhysicsObjects 0x5FD8C0 = 1, no raise, row
  13 of PhysicsConstants.

## States and fades

- +0xE4 is `VortexStateType`: 0 Inactive, 1 Active, 2 FadeIn, 3 FadeOut. StartFadeIn fn_005FFFB0 and StartFadeOut
  set the state and +0xB4 = the turn.
- The seconds in the state, `e` (fn_005FF920): `(turns since +0xB4 + the turn fraction) × ms per turn × 0.001`.
- The presence `f` (fn_005FFAD0):
  - Inactive 0, Active 1;
  - FadeIn: 0 for 2 s, then `smooth((e − 2) / 5)`;
  - FadeOut: `smooth(1 − e / 5)` for 5 s, then 0;
  - with `smooth(x) = (3 − 2x)·x·x`.
- The fade and land curves are fn_005FFAD0, fn_005FFAA0 and fn_005FF350; with the retail spline (every y 0) their
  values are exact in float.
- Once a turn (fn_005FF4F0), after 7 s: a FadeIn becomes Active, and a FadeOut becomes Inactive and is deleted
  (`ToBeDeleted`).

## Taking objects in (the In vortex)

- **Once a turn**, only when Active (ProcessContentsOfVortex 0x5FDB60):
  - a spiral walk of up to 9999 cells from the vortex, within Create's 50 m (+0xE8) on every 3rd turn and 25 m on
    the others; the walk stops at the first cell farther than that;
  - every object of each cell that `CanBeSuckedIntoVortex` (vt +0x640) is taken in at rest;
  - a pile gives up to 1000 into a new pile, scaled `(rand(0.3)·0.4 + 0.3) × scale`, and that one is taken.
- **A thrown object that hits it** (ReactToPhysicsImpact 0x5FD8E0) is taken in with its velocity and its body matrix.
  This needs Active, `CanBeSuckedIntoVortex` and IN_PHYSICS.
- The take (fn_005FD9B0):
  - a creature only fizzes, `SetFizz(1, 3, true)`, and only when closer than `Get2DRadius × 0.7`;
  - anything else that `CanBecomeAPhysicsObject`, when the vortex has its ObjectMover particle system (+0x98,
    0x5FDA33), becomes a **VortexObjectInfo**: the object, "from the physics",
    velocity, matrix, the script-held flag, the vortex;
  - the record is queued for the particle rule `UR_VortexAttract` 0x6D39C0. That rule turns each record into a
    particle carrying the object on a spiral into the vortex.
- What arrives and is not script-held is **written to `vortex.txt`** in the save folder (VortexSave, the In's +0xEC,
  `obj->SaveObject` vt +0x82C, fn_0076FA00): see the next section.

## Bringing them out (the Out vortex)

- An In and an Out are **not linked in memory**. The In writes what it swallows to `vortex.txt` (VortexSave mode 0),
  and the Out of the next land reads it back (mode 1, the land-script parser `LHScriptX::InitIfLevel` 0x7E7530).
  That is how the villagers and objects taken on one island come out on the next.
- Once a turn (ProcessContentsOfVortex 0x5FE030), on even turns:
  - it emits at a cell 16 m away at a random angle: the next object of the file, and when the file is empty
    **30 new villagers**;
  - the new villagers are of the town's tribe (type 7, Norse, without a town), half of them housewives and the rest a forester,
    fisherman, farmer, shepherd or leader; every 5th one is a child of `rand(9) + 1` years, the others `rand(6) + 16`.
  The statistics +0x118 / +0x120 / +0x124 count objects, food + wood and villagers.
- The hand-over (fn_005FE3B0):
  - a villager:
    - joins the Out's town, and becomes a disciple 12 (FROM_VORTEX) when there is also a script flock;
    - joins that flock (AddMember); when the flock is in a script, the villager becomes the script's too;
    - is kept alive while thrown (SetLife(1.0));
  - an animal is pointed at the Out's animal Flock (Living::SetFlock), made at SetFlockParams' position:
    - the flock is made at the first arrival, with SetFlockParams' two numbers as its flock distance and domain radius;
    - a new flock replaces it when its tail holds an animal of another kind;
    - SetFlock does not add a member, so the flock stays empty and every animal joins it, whatever its kind;
    - the animal also gets the Out's town (fn_00417C50);
  - anything but a Dove is then **flung** (fn_005FE5F0):
    - `v = (sin a·B, C, −cos a·B)`, with `B = (rand(5) + 8)·s` and `C = (rand(5) + 10)·s`;
    - spin `w = (−v.z, 0, v.x) / (height / 2)` in the body's axes (0x5FE691 reads v.z, 0x5FE6A3 v.x);
    - `InitialisePhysics` with the vortex as thrower: a real physics flight.

## The land under a vortex

- An In or an Out (not the Volcano) **flattens the 11 × 11 cells under it to their mean altitude** as it fades in
  (fn_005FF4F0, fn_005FF350):
  - the cells' original altitudes are saved the first time;
  - each turn the factor `q = 1 − (1 − f)²` grows;
  - every cell becomes `round(alt0 + q·b)`, with `b = mean − alt0` within 50 m, blending to 0 at 56 m.
- The vortex's spline (5 keys at 0, 10, 30, 50, 58 m) has every height 0 in the retail data, so it adds nothing.
- `q` only grows, because FadeOut takes it as 1: the land is **not restored** when the vortex goes.

## After a land change

- The only writer of a cell altitude, fn_00800DA0, has three callers: the vortex's flattening (0x5FF8F0 in
  fn_005FF4F0), a restore of the saved altitudes (0x5FF2C5 in 0x5FF230, which nothing calls: the land is never
  restored) and the temple's flattening (0x88293C in fn_00882730, the temple's InitTemple, 17 × 17 cells). None of
  them walks the map cells, sends a notification or moves another object; no random draw in any of them. The temple
  only re-takes its own melting (vt+0x1E8 `UpdateMelting` 0x8168F0, at 0x88296C) and bakes its outside (vt+0x208,
  0x8829AF).
- So there is **no re-seat step**: an object follows the new land only if its draw reads the ground.
  - **Follow at once**, drawn at `GetAltitude(MapCoords) + altitude` every frame: every Living (`Living::Draw`
    0x51AEC0 / `Villager::Draw` 0x51B940, `Animal::Draw` 0x51C310, `Dove::Draw`, `SpellWolf::Draw`, through
    fn_0051AF00 0x51AFBF / 0x51AFF1 / 0x51B0D9); the wood and food piles (`PileWood::Draw` 0x51BD8F, `PileFood::Draw`
    0x51C153: ground + sink offset); the spell icon 0x5197DA, `GBaseOnly` 0x609562, `MagicTeleport` 0x5FCCD3, the totem
    0x51AC3C and its statue 0x738B63, the dispenser 0x722A21, the town artifact 0x51C9C3, the scaffold 0x6EA69A, the
    fragment 0x76EC0F; physics objects in flight read the ground every turn (0x645CE7..).
  - **Stay** where they were, drawn at the y their 3D object was given when it was placed (`MultiMapFixed::Draw`
    0x518090, `MobileObject::Draw` 0x518150): every abode, Feature, BigForest, AnimatedStatic, WorshipSite,
    MobileStatic (rock, bonfire, dead or felled tree), pots and mobile objects at rest, trees (`Tree::Draw` 0x74AB00)
    and fields. The morphable meshes' per-vertex melting (`UpdateMelting`) is a snapshot taken at creation.
  - A tree is re-seated only as a side effect: when it grows (`Tree::Grow` 0x74A4CA..0x74A561, forest trees only) and
    when the cursor is over it (the hand's draw collision writes `GetWorldMatrix` into its matrix, 0x5D58DC..0x5D58FB).
- Where it follows: `y = GetAltitude(x, z) + altitude`, at the object's own point, with `altitude = y − GetAltitude`
  taken when the position was set (`MapCoords::Set` 0x603340).
- **openblack.** The Transform holds the absolute y, so the things that follow are moved when the land changes:
  `FlattenLand` (ECS/Vortex.cpp) and `FlattenLandUnderTemple` (CitadelArchetype.cpp) record the ground
  (`GetHeightAt`) under every villager, animal and wood or food pile whose cell reads a corner of the flattened box,
  rewrite the altitudes, rebuild the land, then publish `events::LandAltitudesChanged` with those grounds; its handler
  (`ecs::land_reseat`) sets each one at `new ground + (y − old ground)` (a pile's sink base the same way), and leaves
  the thing exactly where it was when its ground did not change bitwise. Buildings, trees, features, fields, rocks
  and pots are not touched.

## The objects a particle system carries (tornado and vortex)

`RenderParticleGameObject` (0x58 bytes, ctor 0x6C9E60) is what a particle holds. The physics part
(`physics::particle_carried_objects`, was `psys_objects`):

- **Attach** fn_006CA0E0: a flying object is not carried (the atom stays empty); otherwise **Take** fn_006CA060.
- **Take**: the class's `InitialisePhysics` with **add 0**. The object leaves the map cells and is flagged
  IN_PHYSICS without a body, and GameThing +0xA & 0x10 ("carried") is set. A villager or an animal enters FLYING; a
  villager first drops what it carries.
- While carried, the object is drawn at the particle's matrix (DrawAt 0x67B170).
- **Release** fn_006C9EF0: angles from the particle's matrix, position on the ground under it, `EndPhysics(NULL)`
  (back in the map), the carried bit cleared. No flight and no impact.
- **The tornado** (UR_Tornado, miracles.md): when the particle dies, a living thing is released and dies
  (`DestroyedByEffect`), and anything else is deleted (the tornado destroys what it carries).
- **The vortex** (UR_VortexAttract): at the end of the spiral the object is released and, when the record says so,
  flung out (above). Otherwise the In's file takes it.
- IN_PHYSICS (+0x24 & 0x40) is set only by `Object::InitialisePhysics` 0x6374A5 and cleared by `RemoveObject` 0x646B56
  and `EndPhysics` 0x6375B9. A resting proxy body never has it.

## Drawing

Read in runblack.exe W120 on 2026-10-08. Five things are drawn: the Z box (mesh 560), the ground decal (a hole and a
ring on the vortex's land block), the PreLandscape effect (before the land), the PostLandscape effect (in the
Z-sorter) and the LightMap effect. All of them are set up in `CallVirtualFunctionsForCreation` 0x5FEE30. openblack has
the three local effects, their steps and the decal's state (`ECS/Vortex.cpp`), the Z box and the decal's drawing in the
main land, and the ground effect's funnels drawn before the land (`Graphics/RendererVortex.cpp`).

### The GVortexInfo rows (info.dat 0x8889C, stride 0x30; in memory 0xD38248 + type × 0x40, fields from +0x10)

| Type | PreLandscape +0x10 | PostLandscape +0x14 | ObjectMover +0x18 | LightMap +0x1C | initial state +0x20 | fadeWhenDeleted +0x24 | baseScale +0x28 |
|---|---|---|---|---|---|---|---|
| 0 In | 78 SF_LandscapeVortexInBefore | 79 SF_LandscapeVortexInAfter | 76 SF_LandscapeVortexObjectMover | 77 SF_LandscapeVortexLightMap | 2 FadeIn | 0 | 0.28 |
| 1 Out | 80 -> SF_LandscapeVortexInBefore | 81 SF_LandscapeVortexOutAfter | 0 (none) | 77 SF_LandscapeVortexLightMap | 1 Active | 1 | 0.28 |
| 2 Volcano | 82 SF_LandscapeVolcanoBefore | 83 SF_LandscapeVolcanoAfter | 0 (none) | 84 SF_LandscapeVolcanoLightMap | 1 Active | 0 | 0.75 |

- The particle type -> file table is filled in fn_0068EA00 (0x68ECDF..0x68ED5A, table [0xD4EBB0] + 4 × type). **80 is
  InBefore again** (the same register, 0x68ED1F): there is no "OutBefore" file.
- SF_LandscapeVolcanoAfter and SF_LandscapeVolcanoLightMap have `InitiallyCreated` all 0, so they make no atom and draw
  nothing.

### Creation (CallVirtualFunctionsForCreation 0x5FEE30)

- Volcano only (fn_005FD410 == 2, 0x5FEE42): `SoundTag::Create(this, 0xAC, false, 2, -1, 0, 1, 1, 0)` (0x5FEE5A).
- Position +0xD4 = (x, GetAltitude + MapCoords y, z) (0x5FEE6D..0x5FEE99); state +0xE4 = info +0x20 (0x5FEEAB).
- Unless the object is UNAVAILABLE (+0xA & 1, 0x5FEEBB), in this order:
  - the Z box +0xAC (0x5FEEC8..0x5FEFA5): a static `LH3DObject::Create(0)` with mesh MeshPack[0x230] = 560
    MSH_S_ZCHEATBOX at +0xD4 with **y = 0**, scaled by baseScale; the first primitive of its mesh gets material type
    18 (Z only, alpha test), which edits the shared mesh;
  - ObjectMover (info +0x18), only if +0x98 is empty: `GJPSysInterface::Create(NULL, type, +0xD4, (0,0,0), 1.0, NET 1)`
    -> +0x98, then SetPlayer(GetPlayer()) (0x5FEFB8..0x5FF002);
  - PreLandscape (info +0x10): `PSysInterface::Create(NULL, type, (x, GetAltitude(x, z), z), (0,0,0), 1.0, NET 0)` ->
    +0x9C (0x5FF017..0x5FF0A6);
  - PostLandscape (info +0x14): the same at +0xD4, NET 0 -> +0xA0 (0x5FF0B3..0x5FF0EA);
  - LightMap (info +0x1C): `GJPSysInterface::Create(..., +0xD4, (0,0,0), 1.0, NET 0)` -> +0x94 (0x5FF0F7..0x5FF12E).
- `PSysInterface::Create` 0x68E910 only forwards to `GJPSysInterface::Create` 0x68F2F0. Type 0 gives no effect
  (0x68F2F5). The effect's +0xAC = (NET == 1) (0x68F3AE): the ObjectMover is synchronised, the other three are local.
- openblack (`vortex::Create`): Pre, Post and LightMap through `psys::manager::StartForSpell`, Local, Pre and Post
  stepped per frame. The effects stand at (x, the land's height, z): the script's vectors have y = 0, so this is +0xD4.
  The ObjectMover is **not made**: `TakeIn` uses it as the "+0x98 exists" gate (0x5FDA33). The original hands each record
  to the effect (AddTarget_ vt +0x114, 0x5FDAB1), and `UR_VortexAttract` takes the object. Without that rule our queue
  would take the same objects again every turn, so the ObjectMover comes with `UR_VortexAttract`.

### Once a turn (fn_005FF4F0, from fn_005FF330 <- PSysGlobal::GameLoopEnd 0x68F5B9)

In this order:
1. the contents (vt +0x90C);
2. the LightMap +0x94 `Process_(info)` (vt +0x100, the turn's ms [0xD01A38]);
3. the ObjectMover +0x98 `Process_(info)`;
4. the state change and the land (above).

Both steps get the PSysProcessInfo {all 0, power +0x30 = 1.0, curl +0x34 = 0, enabled +0x38 = 1} (0x5FF512..0x5FF5EF).
openblack: `vortex::ProcessAll` steps the LightMap after the contents, with the turn's length; the ObjectMover step is
pending.

### Once a frame, before the land (fn_005FFBB0(1), from fn_005FF310 <- GLandscape::Draw 0x5E4E91)

This is stage 4h of [original-frame.md](original-frame.md): after the sea (4g), before the land (4i, fn_007FF610 at
0x5E4E96).
1. The Z box +0xAC is drawn at once, whatever the state (vt +0x104 = fn_00815980: on-screen and distance test, then
   DrawNow vt +0x108; 0x5FFBB6..0x5FFBC2).
2. `f` = fn_005FFAD0 (the fade). Only when f differs from the last value +0x8C (-1.0 at construction, fn_005FE9D0
   0x5FEA1F; an x87 equality test, 0x5FFBD3):
   - +0x8C = f;
   - if f != 0: the decal is made (fn_005FEA70; it does nothing while +0xA4 is set);
   - if f == 0 and the decal exists: it is released (fn_005FE8B0);
   - the Z material +0xA8's ALPHAREF byte (+4) = `min(ftol(f × 255), 235)`, an unsigned compare (0x5FFC1F..0x5FFC44).
3. If f != 0 and +0x9C (PreLandscape) exists (0x5FFC47..0x5FFC64):
   - with the argument set (always 1 here): `SetOrigin((x, GetAltitude(x, z) − ((0.3 − 2.5) × q + 2.5), z))`, q =
     fn_005FFAA0: ground − 2.5 + 2.2 q (0x5FFD3E..0x5FFD7B, vt +0x124). GetAltitude 0x803090 reads the LH3DMapCoords
     (ftol(x × 65536 × 0.1), ftol(z × 65536 × 0.1));
   - `Process_(info, g_game_time_inc)` (vt +0xFC, the frame's game ms) with info {all 0, power = **f**, curl 0,
     enabled 1} (0x5FFD81..0x5FFD95);
   - `Draw_(1.0, true)` (vt +0x104 -> fn_00679840: the Sorted path, fraction 1; 0x5FFDA3..0x5FFDAA). ZR_SurfRevol
     surfaces are drawn there and then (particles.md, 0x67CBA0), so the PreLandscape funnels are drawn **before the
     land**, after the Z box. At f == 0 the effect is neither stepped nor drawn.

openblack, the box: `Renderer::DrawVortexBoxes` (`Graphics/RendererVortex.cpp`), called by `DrawPass` in the main view
after the sea and before the land, for every `LandscapeVortex` (available or not: fn_005FF310 walks the whole list with
no test). Mesh 560 from the mesh cache (`MeshId::SpellZCheatBox`), placed by `vortex_draw::ZBoxModel` (x, 0, z, the
3x3 scaled by baseScale, not turned; `test_vortex` ZBoxModel), drawn in mode 18 (`render_modes::State(ChromaDepthOnly)`:
depth written, blend ZERO / ONE, so no colour) with the primitive's own culling. The mode's alpha test is left out: the
material's ALPHAREF is 0, so every pixel passes. The "Celestial" program draws it (plain L3D geometry placed by one
model matrix).
- The mesh-level `NoDraw` flag of 560 does not stop the draw (inferred, see Creation above): the only test of it is the
  load-time pass at 0x80889C.
- fn_00815980's distance test (0x815A23..0x815A47) skips an object farther than `radius × scale(+0x44) × 300` from the
  camera. 560's radius is at least 90 (half the box's width), so that is at least 7.5 km for an In or Out (20 km for
  the Volcano), more than a land's diagonal (about 7.2 km): (inferred) never reached in play, so it is not ported. The
  on-screen test is bgfx's clipping.
- Only in the main view: the mirrored land (stage 4c) comes before 4h and draws no box.

openblack, the effects: `vortex::UpdateGroundEffects`, from `magic::Update` before every other per-frame effect step there (the
fire graphics, the hand's effects, the shields, the teleport stones). It sets the decal's state (`NextDecal`,
`DecalAlphaRef`), then the PreLandscape's origin (`PreLandscapeHeight`) and its step with power f, for every vortex.
`vortex::UpdateOverLandEffects`, after the teleport stones and before the falling spell, does the Post steps and the
LightMap alphas (below).

openblack, the ground effect's draw (`Graphics/RendererVortex.cpp`):
- `Renderer::SplitVortexParticles`, at the end of the frame's PSys collects (`CollectFrameParticles`, main view with
  the entities): for every `LandscapeVortex`, `vortex_draw::RoleOf` tells each collected effect apart
  (`test_vortex` EffectRoles). A vortex shows while its `decal.made`, which `NextDecal` sets exactly while f != 0.
  - The surfaces of a shown vortex's PreLandscape (a Sorted effect, as the original's `Draw_(1.0, true)`) leave the
    at-once list for `groundSurfaces`.
  - Its sprites, if any (the Volcano's steam and fire), stay in the Z-sorter: on the Sorted path the original
    queues them as Z objects too.
- `Renderer::DrawVortexGroundEffects`, right after `DrawVortexBoxes` (main view, with the land and the entities),
  draws `groundSurfaces` in the walk's order through `DrawParticleSurface`. The solid funnel writes depth there, and
  the land is drawn over both funnels except in the decal's hole.
- (approximate) With several vortices the original draws each vortex's box and then its funnels, one vortex after
  the other. openblack draws every box, then every funnel; the two differ only where one vortex's box hides another's
  funnels.
- At f == 0 (0x5FFC47: no step and no draw) the PreLandscape effect is neither stepped (`UpdateGroundEffects`) nor
  drawn: its surfaces, sprites and chains are dropped from the frame.

### The ground decal (fn_005FEA70, released by fn_005FE8B0)

- +0xA4 = `CreateMaterial(6, texture(fn_005FD4C0))`: mode 6 (SA/ISA, alpha = texture × diffuse, no Z write) with
  **S_VortexBaseMultiRing.raw** (In, Out) or **S_Volcano_Base.raw** (Volcano) (table 0xBF3F5C).
- +0xA8 = `CreateMaterial(0x12, texture(fn_005FD4D0))`: mode 18 (Z only, alpha test) with **S_VortexBaseAlphacopy.raw**
  (In, Out) or **S_Volcano_Base_Alpha.raw** (Volcano) (table 0xBF3F68).
- +0xB0 = the LandBlock holding the vortex's cell: `g_index_block[(ftol(x × 0.1) >> 4) × 32 + (ftol(z × 0.1) >> 4)]`,
  none when either cell is outside 0..511 (0x5FEAA5..0x5FEB08). Without a block the decal stops after the materials.
- The block's **+0x90C is its x corner and +0x910 its z corner** (block index × 160): LH3DIsland::Create stores
  [esp+0x18] × 160 at +0x90C (0x8043A5) and [esp+0x14] × 160 at +0x910 (0x8043B5), and files the block at
  `g_index_block[[esp+0x18] × 32 + [esp+0x14]]` (0x8042CE..0x8042D1, 0x8043C7), the same index order as the decal's
  (x first). So the decal pairs the land's u with z and its v with x, as openblack's land does (`vs_terrain`: u = local
  z / 160, v = local x / 160).
- The block gets (0x5FECF8..0x5FED94): +0x94C = 3, +0x950 = the Z material, +0x954 = the ring material, and the same UV
  matrix in +0x958 and +0x988 (fn_005FEDA0). The matrix is the identity × s, s = 1 / baseScale, with the translation
  `m9 = ((b910 − z) × 0.00625) × s + 0.5` and `m11 = ((b90c − x) × 0.00625) × s + 0.5`, in float steps
  (0x5FEB40..0x5FECF4). A vertex's (u, v) becomes (s u + m9, s v + m11): centred on the vortex, 160 × baseScale m
  across (44.8 m for the In and Out, 120 m for the Volcano).
- Release: both materials' +8 = 0, +0xA4 = +0xA8 = 0, the block's +0x94C = 0 (fn_005FE8B0). Several vortices on one
  block share its fields: the last one made there holds them, and the first one released clears the block's flags.
- **How the land draws it** (x87 fn_00874AA0; the SSE fn_007A1800 reads the same field at 0x7A2D37, 0x7A32D5,
  0x7A347F):
  - +0x94C bit 0 (0x87535D): first fn_00878F70, a Z-only pass of the block's triangles in the Z material, each vertex's
    (u, v) through the matrix and put back with its inverse afterwards (0x87907D). Then ZFUNC = EQUAL (render state
    0x17 = 3) for the block's own pass, and back to LESSEQUAL (4) after it (0x875BB1..0x875BDD).
  - +0x94C bit 1 (0x875BE8): fn_008790F0, the ring pass in the ring material. It rebuilds the block's vertices at its
    LOD (+0x930) with the diffuse = the light table entry ([0xEDD90C]) of the cell's byte 3 and the specular = the
    cell colour | 0xFF000000 (0x87915A..0x87917B), with no haze, and the UVs through the matrix +0x988. It is drawn
    under ZFUNC EQUAL when bit 0 is also set (0x875BED..0x875C3C).
  - Net effect (inferred from the states): the Z pass writes depth only where the mask's alpha >= ALPHAREF (mode 18's
    function writes ZWRITEENABLE 1 itself, 0x82E39D). The land under EQUAL is then drawn only there, so a **hole**
    opens where the mask is below `f × 255` (capped at 235). Through it shows what was drawn before: the sky, the sea,
    the Z box and the PreLandscape funnels. The ring is laid over the rest of the block in the land's light and does
    not fade with f.
  - It covers only the block holding the vortex's centre, so it is cut at that block's edge. The UVs clamp (no tiling:
    material +5 bit 2 and g_b_need_tilling, 0x87901D). The mask's border is alpha 255 and the ring's 0, so nothing
    shows outside.
  - Texture alpha (256 × 256, row 128 every 16 px): Alphacopy 255,255,255,190,180,87,85,35,64,37,78,72,183,178,255,255
    (min 16); Volcano_Base_Alpha 0 / 255 only (a hard disc); MultiRing / Volcano_Base 0,29,184,255...255,226,81. The
    game cuts them to ARGB4444 when it loads them (as openblack's texture loader does).
  - **The mirrored land** (fn_007FF4F0, stage 4c) calls the same block function (0x7FF5C0) on the same blocks (the
    transition blocks are copied whole, +0x94C included, 0x7FF593..0x7FF59F). Neither fn_00874AA0 nor its two decal
    passes read the mirror flag [0xFA92DC], set around the mirrored land (0x7FF535, 0x7FF5FD); only the vertex
    builders fn_00875C60 / fn_00876910 do. So the hole and the ring are drawn in the reflection too. There the land is
    drawn with ZWRITEENABLE 0 (0x5E48C5), but the Z pass's mode 18 turns it on again (0x82E39D); (not traced) whether
    anything in the block draw turns it off again for the mirrored blocks after it.

openblack (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`):
- `CollectVortexDecals`, in the main view's land pass: every `LandscapeVortex` whose `decal.made` (C1's `NextDecal`)
  gives its block (`vortex_draw::DecalBlock`: the cells truncated, none off the 512 cells), its UVs
  (`vortex_draw::DecalUvTransform`, the float steps above), its `decal.alphaRef` and its textures from the resource
  cache (`raw/<mask>a`, `raw/<ring>`, `raw/<ring>a`; `vortex_draw::DecalTexturesFor`). One decal per block: the first
  vortex found holds it (approximate: the original keeps the last one made).
- In `DrawPass`'s block loop, for that block only: `DrawVortexDecalMask` (program "VortexDecalMask" = `vs_terrain` +
  `fs_vortex_mask`: mode 18's state, depth written where `round(alpha × 255) >= ref`, discarded elsewhere), then the
  block in its usual state with the depth test EQUAL, then `DrawVortexDecalRing` ("VortexDecalRing" = `vs_terrain` +
  `fs_vortex_ring`: mode 6 under EQUAL, colour = ring × the land light × the pass's light scale + the cell colour,
  alpha = the ring's, `u_hazeBlock` 0 for no haze), then the block's projected shadows as before. Both programs use the
  land's own vertex program and the block's uniforms, so their depth is the block's to the bit. The textures are
  clamped.
- (inferred) the light table entry's alpha is 255 (openblack's land light texture has 255 there), and the ring pass has
  the land's SPECULARENABLE on.
- The mirrored land in the reflection draws no decal yet (pending, see above).

### The Z-sorted part (LandscapeVortex::Draw 0x5FFDC0, from the object draw list)

- `SendInvisibleDrawCollision(this, +0xD4, 0.7 × Get2DRadius)` (the hand's collision); the ObjectMover +0x98
  `Draw_(true)` (vt +0x108); `AddDrawing` 0x5FFE10: `NewZObject(this, 0x5FFE70, |GetLHPoint(+0x14) − camera|²)`. The
  vortex's own mesh 559 MSH_S_VORTEXCYLINDER (`GetMesh` 0x5FEE20) is never drawn: it only gives the radius.
- **What queues it.** The object draw list fn_005E5CD0 (stage 4m) calls Draw (vt +0x610) for every available object of
  `GLandscape::DrawObjects` (0x5E6067..0x5E6086). Draw queues the Z object with no on-screen test of its own, and sets
  `g_b_last_on_screen` = 1 (0x5FFDFD), so the vortex is always "active". It is drawn again in the still-camera pass too
  (0x5E60E2..0x5E610F, which redraws only the active ones). How the list is built, rebuilt and walked is in
  [original-frame.md](original-frame.md#6-the-object-draw-list-stage-4m). Two corrections to what this page said
  before: the turn trigger is "more than 10 turns since the last rebuild" (an unsigned `ja`, so every 11 turns with no
  other trigger), and the vt +0x74 skip on the object's +0x40 is GetDontDraw, not a footpath link (+0x40 is the
  Game3DObject).
  So the Post step and the light-map alpha run only for a vortex in a visible block (or the global list) at the last
  rebuild, which may be up to 10 turns old. openblack has no logic-side copy of that list or of the block culling (it is
  in the renderer), so it steps them every frame (pending).
- The callback 0x5FFE70:
  - if +0xA0 (PostLandscape) and f != 0: `Process_(info {power = f, enabled 1}, g_game_time_inc)`, then
    `Draw_(1.0, true)`. Its origin stays at +0xD4;
  - then, if +0x94 (LightMap): the effect's alpha byte (PSys3D +0x6C) = `ftol(L × 255)` with L = fn_005FF980, then
    `Draw_(true)` (vt +0x108, the game's draw fraction g_game +0x205D64, 0x673700).
- fn_005FF980, the light map's L(e) (e = fn_005FF920, stored to a float at 0x5FF989):
  - Inactive 0; Active 0.6 ([0x92D2DC]);
  - FadeIn: `e / 2` while e < 2; then `(0.6 − 1) c + 1` with c = (e − 2) / 5, set to 0 when not above 0 (`test ah,
    0x41`) and to 1 when not below 1 (0x5FF9B7..0x5FFA1E);
  - FadeOut (any other state): `(1 − 0.6)(e / 5) + 0.6` while e < 5; then `1 − c` with c = (e − 5) / 2, kept to 0..1
    the same way (0x5FFA20..0x5FFA95).
  It is a flash to 1, then a rest at 0.6.
- openblack: at f == 0 (0x5FFE89) the PostLandscape effect is neither stepped nor drawn (`SplitVortexParticles`
  drops it from the frame, as it does a hidden PreLandscape). Shown, its stars are drawn at once with the other Sorted
  effects' surfaces, before the Z-sorter's drain. The original draws them inside the vortex's Z object, at its place
  in the drain (pending).
- The light map's stamp follows the effect's alpha: the effect's collect multiplies every atom's alpha by it
  (`Effect::CollectCollection`: alpha × global / 255 in float). The original scales the DrawData alpha byte by
  `(global × alpha) >> 8` when the global byte is not 0xFF (fn_00679920, 0x679BC2..0x679BDF, the byte set from
  PSys3D +0x6C by fn_00679860 0x679875). This is the generic particle collect: every effect with a global alpha
  rounds this way (pending, Particles).
- **Precision.** fn_005FF980, fn_005FFAA0, the height at 0x5FFD4E..0x5FFD73 and the two `× 255` run on the x87 under
  the game's 24-bit precision control ([engine-loop.md](engine-loop.md)), so every step rounds as a float step.
  openblack computes them in float in the same order; `test_vortex` pins the bit patterns, e.g. the height at ground 0,
  q 0.3 is 0xBFEB851E (in double it would be 0xBFEB851F). `ftol` truncates.

### Deletion (ToBeDeleted 0x5FE8F0)

Unlinks the vortex, deletes the Z box (vt +4) and the four effects at once (vt +4 with 1: no CloseDown), releases the
decal (fn_005FE8B0), then MobileStatic::ToBeDeleted. openblack: `vortex::OnDeleted` deletes the effects
(`psys::manager::Delete`) and clears the decal. `ecs::ToBeDeleted` calls it for a `LandscapeVortex`, whether the
deletion is at once or deferred (`test_vortex` VortexDeletion). A vortex found unavailable in `ProcessAll` or the frame
updates loses them as well. A land change clears every effect (`psys::manager::Clear`).

### The spell files (Data\Spells\ZSpellFiles\SF_*_txt.zzz, all present)

- **InBefore** (the In's and the Out's PreLandscape):
  - group 0 point (InitialScale 2, `SoundOfCreate SOUND_ACTION_VORTEX LOOPING SOFTRELEASE`);
  - then group 2 point (InitialScale 40, FollowOrigin);
  - then two funnels, `ZR_SurfRevol_AlphaFunnel` (TestFunnel, Scale 0.7, S_Teleport_Vortex_Texture.raw, additive, no
    Z, ClampToLandscape 1) and `ZR_SurfRevol_Solid` (Scale 0.8, S_Teleport_Vortex_Texture01.raw, colour 120,120,120,
    **Z write**, MaterialUseTextureAlpha 0).
- **InAfter** / **OutAfter** (PostLandscape):
  - group 0 point (OutAfter adds the same SOUND_ACTION_VORTEX loop);
  - then group 2 point (InitialScale 40, `SetScale` = StrengthFloatProvider × 1, ForceConstantHeight 15);
  - then `ZR_SurfRevol_Stars` (TestFunnelSpout, Scale 0.65, S_Teleport_Stars.raw, additive, `SetAtomAlpha` = strength
    × 255, SpeedV **+1 In / −1 Out**).
  The stars follow f through the power.
- **ZR_SurfRevol's material** (`ModifyAtomCollection` 0x686370): `CreateMaterial(6, texture)` (0x6863EC), then
  `GJUtils::SetMaterialProperties` 0x57E120 with the rule's {+0x3C UseAdditiveAlpha, +0x3D MaterialUpdateZBuffer,
  +0x3E MaterialSetDoubleSided, +0x40 MaterialUseTextureAlpha} (DefineProperties 0x6B2E80). The ctor's defaults are
  0, 0, 1, 1 (0x686231..0x68623F).
  - The rules, in order: 4 -> 6; UseTextureAlpha 0 -> 3; additive -> 13. With Z: 6 -> 5, 13 -> 12, 8 -> 3, 16 -> 9.
    Without Z: 5 -> 6, 12 -> 13, 3 and 2 -> 8, 9 -> 16.
  - So the Solid funnels (InBefore, VolcanoBefore: Z, UseTextureAlpha 0) are **mode 3**, and the alpha funnels and
    the stars mode 13.
  - Of the 132 spell files only those two Solid funnels set MaterialUseTextureAlpha 0. SF_TeleportVortex (6) and
    SF_SpellDispenserVortex (13) keep their modes.
  - The mode functions 3 (fn_0082D920) and 5 (fn_0082DD90) are the same code but for the mode cache's id: SA/ISA,
    no alpha test, ZWRITEENABLE 1, COLOROP and ALPHAOP MODULATE(TEXTURE, DIFFUSE), the material's texture. So mode 3
    draws as mode 5 does,
    and it still honours the texture's alpha. openblack: `Surface::useTextureAlpha` (Particles/Rules/SurfRevol) goes
    to `ModeFromProperties` in `DrawParticleSurface` (`test_vortex` FunnelMaterials: 3 and 5 give the same state).
- **LightMap**: one `ParticleLightMapCreator` atom, `Teleport_Vortex_LightMap.raw` (12 × 12 × 3, ShiftX 6.78761, ShiftZ
  8.60177).
- **ObjectMover**: `UR_VortexAttract` and `AppearanceRuleTumble` on group 0.
- **VolcanoBefore**: two fire funnels, a rock funnel, and two disc emitters (steam and fire sprites).

### Random draws

| Where | Stream | When | Count |
|---|---|---|---|
| Effect creation (fn_00673070 from 0x68F3EC) | none: outside a step PSysRand is the 0 function | creation | 0 |
| PreLandscape / PostLandscape steps | **local** (NET 0) | every **frame** while f != 0, in the draw | one `PSysRand(0x100)` per atom made (AtomCore::Create 0x673816), all in the first steps: InBefore 4, InAfter / OutAfter 3; VolcanoBefore the same plus its emitters' sprites every frame |
| LightMap step | **local** | every turn | 1 when its atom is made; `LocalFloatRand(0)` × 3 at each draw makes no draw |
| ObjectMover step | **synchronised** (NET 1) | every turn | only for taken objects (not made in openblack) |
| CRT rand() | none found in these paths | - | 0 |

The local seed is in the per-turn state hash, so a vortex's effects change the hash's random part. openblack steps
Pre and Post in the logic frame update (as the teleport stone's vortex):
- every Pre first, before the other per-frame effect steps, as stage 4h comes before the object draw, the hand and the
  particles;
- every Post afterwards, in list order. The original steps the Posts in Z-sorted order (by distance), interleaved with
  any other Z object whose callback steps an effect.

## What is ported and what is pending

> **Code rules.** The vortex state lives in ECS components (`LandscapeVortex`) and its systems are reached through
> Locator services; the info rows come through the resource caches; pure logic such as the fades is tested with
> fakes in `test/`; comments describe behaviour in plain English, with no decompiled names or addresses (those
> belong here). See [the conventions](../refactor/README.md).

| Part | State |
|---|---|
| GVortexInfo, the three classes, Create (radius 50, scale 1), the state and fades, FadeIn -> Active / FadeOut -> delete at 7 s, the turn from GameLoopEnd | ported (Vortex.cpp, MagicLoop.cpp) |
| The land flattening (fn_005FF4F0 / fn_005FF350, the retail zero spline) | ported (`SetCellAltitude` + `RebuildAltitudes`, as the citadel) |
| VORTEX_FADE_OUT / VORTEX_PARAMETERS / CREATE Vortex | ported (CHLApi.cpp) |
| The In's take (spiral, ReactToPhysicsImpact, CanBeSuckedIntoVortex 0x639B60, the same-cell test, VortexObjectInfo) | ported; (pending) the pile split (Storm.cpp's SplitPile to be shared) |
| UR_VortexAttract (the spiral of the carried objects) | pending (the PSys rules) |
| particle_carried_objects Take / Release / Fling, the carried bit | ported (ParticleCarriedObjects.cpp); the tornado's side is in Storm.cpp |
| The Out's emission (the cell 16 m away, the file's objects, 30 new villagers), the hand-over to its town, the thrown list kept alive, the fade held while it emits, the fling | ported |
| The hand-over's flocks (the script flock, the animals' Flock) | ported; (pending) the disciple FROM_VORTEX, the flock's info and player, the animal's town (they need the villager and animal APIs) |
| VortexSave (`vortex.txt` between lands) | the writers: `lhscriptx::WriteCommand` and the physics classes' SaveObject ([land-script-save.md](land-script-save.md)); the reader: one line at a time through the land-script interpreter with the position offset and the last created object (Script's map state); (pending) the other classes' writers and the save folder |
| Drawing: the PreLandscape, PostLandscape and LightMap effects (made, stepped, deleted), the light map's L(e), the decal's change detector and alpha reference, the ground effect's height | ported (Vortex.cpp, MagicLoop.cpp; tested in `test_vortex`) |
| Drawing: the Z box (mesh 560 at (x, 0, z) × baseScale, mode 18, after the sea and before the land, main view) | ported (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`; tested in `test_vortex`) |
| Drawing: the decal's hole and ring on the vortex's block (mask, block under EQUAL, ring), in the main land | ported (`Graphics/RendererVortex.cpp`, `fs_vortex_mask`, `fs_vortex_ring`; tested in `test_vortex`) |
| Drawing: the PreLandscape funnels right after the boxes and before the land, Pre and Post undrawn while f == 0, the Solid funnel's mode 3 (MaterialUseTextureAlpha), the light map stamp following the effect's alpha | ported (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`, `Particles/Rules/SurfRevol.cpp`, `Graphics/RendererRevolvedSurface.cpp`; tested in `test_vortex`) |
| Drawing: the decal in the mirrored land, the stars drawn at the vortex's Z object, the global alpha's rounding, the ObjectMover | pending (the renderer, the particle rules) |
| Save / Load of the vortex | not ported (openblack has no saved games) |
| The vortex that leaves a land | the scripts open it with CHL `CREATE` of an In at a marker (`challenge.chl`: Land 1 `LeaveThroughVortexL1` at (1700.23, 11.40, 2520.18), once `OpenVortexL1` is set or at once when the creature guide is skipped; Land 2 at (1061.11, 147.79, 3597.97); Land 4 at (2060.18, 11.39, 2591.70); Land 3's script is given its place when it starts). Opening it is ported; going through it to the next land is not (`LeaveLandNow`, then `LandControlAll`'s `LOAD_MAP`, an empty native) |
| Debug window | Debug > Windows > Vortices (`Debug/Vortices`, `VorticesModel`): the vortices on the land with fade out and delete, a new In / Out / Volcano at the hand or at a click, and each land's leaving vortex opened at the script's place |

## Sources

- runblack.exe W120: MagicVortex.cpp 0x5FD2D0..0x600460, VortexSave.cpp 0x76F840..0x7700B0, PSysTornado.cpp
  (UR_VortexAttract 0x6D39C0, UR_Tornado 0x6D18B0), RenderParticleGameObject 0x6C9E60..0x6CA162.

## Pending

- The drawn vortex (the Z box, the ground decal and its ring, the funnels) follows the addresses above, but its look has
  not been compared with a shot of the retail game yet.
- The In's pile split (a pile gives up to 1000 into a new pile, which is taken), to be shared with the tornado's.
- `UR_VortexAttract`: the spiral that carries the taken objects into the vortex.
- The hand-over: the disciple FROM_VORTEX, the flock's info and player, and the animal's town.
- VortexSave: the writers of the other classes, and the save folder.
- Drawing, the renderer's side:
  - the decal's hole and ring in the mirrored land (the original draws them there too, and its Z pass turns the
    mirrored land's Z writes back on; see "The ground decal");
  - several vortices on one block: the last one made should hold the decal (openblack: the first one found);
  - with several vortices, each one's box then its funnels, vortex by vortex (openblack: every box, then every
    funnel);
  - the PostLandscape stars drawn inside the vortex's Z object, at its place in the drain (openblack: at once with
    the other Sorted surfaces, before the drain);
  - the global alpha as `(global × alpha) >> 8` on the alpha byte (openblack: alpha × global / 255 in float, in the
    generic particle collect, so for every effect with a global alpha).
- The ObjectMover, with `UR_VortexAttract` (a synchronised effect: a game-logic change).
- (not traced) the audio DLL's sample choice for SOUND_ACTION_VORTEX and the volcano's SoundTag 0xAC.
- The Post step and the light-map alpha only for a vortex in the land's object draw list (a visible block within
  VanishObjectDist, or the global list, at the last rebuild: see "What queues it" above). This needs the logic-side
  object draw list planned in [original-frame.md](original-frame.md#the-object-draw-list-what-openblack-needs);
  openblack steps them every frame.
- Save / Load of the vortex (openblack has no saved games).
- After a land change (see above), deliberate differences of openblack:
  - a re-seated villager or animal whose own turn came before the change is drawn sliding to its new height over the
    rest of that turn (the draw goes from its position at the turn's start) instead of at once;
  - the other classes that follow the land in the original (spell icon, GBaseOnly, MagicTeleport, totem and statue,
    dispenser, town artifact, scaffold, fragment) are not re-seated;
  - openblack's morphable meshes take the melting on the GPU at every draw, so a field or a morphable building
    follows the new land, while the original's melting is frozen;
  - the tree's re-seat when it grows or is under the cursor is not ported.
- After a land change: the creature's drawn height (`Creature::Draw` 0x517910 and `LH3DCreature` not traced); whether
  footpaths are drawn in the retail game.
