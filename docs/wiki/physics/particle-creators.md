# Particle creators

## Summary

A creator turns each atom of a particle effect into something drawn: a sprite, a mesh (the shield dome and the beams
are particle meshes), a chain of segments, a light map stamped on the land, or the mist. This page covers the
creators the miracles use, their materials and how each is drawn.

openblack ports the mesh creators (with the shield dome), the chains, the light maps and the mist creator from the
original; the engine they plug into is on [Particles: the PSys core](particles.md).

## The original

#### Particle meshes and the shield dome

The drawing of the PSys meshes (`ParticleMeshCreator`, `UsePlayerColor`, `FaceCamera`, `DrawCutByPlane`), which makes
the shield dome and the beam visible, is in [Particle meshes](#the-particle-meshes-and-the-shield-dome).

- **Materials** ✅ faithful. Each creator rewrites the materials of its mesh with `GJUtils::SetMaterialProperties` fn_0057E1D0
  the first time it looks it up, on its first particle. The port does it on the first `InitAtom`, with
  `L3DMesh::SetMaterialProperties`. Depending on the creator:
  - `ParticleMeshCreator`: fn_006A8A40 0x6A8A5F..0x6A8A6E with the pack mesh and `MeshChangeMaterialProps`;
  - `ParticleMeshCreatorAnimTextured`: fn_006A8CC0 0x6A8CDF..0x6A8CEE;
  - the mesh file: `PGetSharedMesh` 0x57DF18 when loading it;
  - `ParticleAnimCreator`: fn_006A95E0 0x6A9602 with +0x93 = 1, or fn_0057D420 0x57D433 with the file, always.

  `MaterialProperties` = +0x55 additive, +0x56 Z, +0x57 two-sided, +0x58 change, +0x59 alpha (no property, 1 in the
  ctors 0x6A897D / 0x6A8BD0; for the animated one, +0x90..+0x94). The pack meshes change for all their users, as in
  the original. The forest seed (Seed.L3D, type 2) stays at 8 `TexturedAlphaNz`, which blends with the alpha.
  **⚠️ approximate** if two creators load the same file with different properties, here each applies its own; the
  original kept those of the first.
- **`UseGlobalAlpha`** ✅ faithful. It is bit 0 of the particle's +0x24. `Particle3DObj::DrawAt` 0x67A216..0x67A227 /
  `Particle3DAnim::DrawAt` 0x67A9D6 pass it to `SetGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 of the object's +4). With
  it, the drawing uses the table 0xC387C8 (fn_0080DB30 0x80DEED..0x80DF09), which takes the opaque modes 0, 2, 4, 9 and 17 to
  their alpha versions. Without it it uses 0xC38728: the colour's alpha only shows in the blending modes. Who sets it:
  - `ParticleMeshCreator`: **never**. Its property +0x5A is read (0x6B3902), but `CreateParticle` 0x6A8B00 does not use it, and
    the ctor 0x6C7A23 clears the bit;
  - `AnimTextured`: yes, from +0x5A (0x6A8E04..0x6A8E17);
  - `ParticleAnimCreator`: yes, from +0xA5 (ctor 1 at 0x6A93CA, 0x6A983A..0x6A9840; 0 while it blends two meshes at
    0x67A9A4, which no file does).

  In the port, `Instance::globalAlpha`:
  - only the additive ones and those with global alpha with alpha < 1 go with the fading meshes (table 0xC387C8);
  - the others go with their mesh, and the alpha (1 − [0][3]) is only taken by their blending primitives;
  - modes 12 and 13 are the same in both tables, so the additive ones do not change;
  - in the files, the only non-additive `ParticleMeshCreator` is the SF_Forest seed, and its mode 8 is also the same
    in both tables.
- **DrawData specular** (+0xC, the atom's +0x90). `Particle3DObj::DrawAt` gives it
  to the object along with the colour: fn_0080BEC0(colour, specular) 0x67A012, which fn_0080BF10 adds to the ground's specular, or
  `SetColour` vt 0x2C 0x67A023 (obj +0x50, fn_007F9770).
  - `mesh_atoms::Instance::specular` carries it. It goes in the w of the fourth column as 3e6 + 7 bits per channel, like
    `components::SpecularColour`, and the PSys branch of `vs_object` no longer sets it to 0.
  - **⚠️ approximate**: the low bit of each channel is lost.
  - With `DrawWithLandscapeColor` that w carries the colour, so the specular is not sent. No file gives specular to
    a mesh (`SpecColorR/G/B` 0).

- +0x59, MaterialProperties +4 (no property; 1 from the ctors 0x6A897D / 0x6A8BD0, ParticleAnimCreator's +0x94 0x6A9225): 0 would make every material TexturedAlpha (GJUtils::SetMaterialProperties 0x57E13D)
- The mesh is fetched at the creator's first particle (CreateParticle 0x6A8B33 / 0x6A8DE7 / 0x6A97FE while +0x50 / +0x34 is 0), and with it GJUtils::SetMaterialProperties fn_0057E1D0 rewrites every material of the mesh: fn_006A8A40 0x6A8A5F..0x6A8A6E (a pack mesh, if MeshChangeMaterialProps +0x58), fn_006A8CC0 0x6A8CDF..0x6A8CEE (the same for AnimTextured), GetSharedMesh 0x57DFB0 -> PGetSharedMesh 0x57DF18..0x57DF24 (a mesh file, if +0x58, when it is first loaded), fn_006A95E0 0x6A9602..0x6A9617 (ParticleAnimCreator's pack mesh, if +0x93: 1 from the ctor, no property) or fn_0057D420 0x57D433 (its mesh file, always). The pack meshes are changed in place, for every user, as in the original. (aproximado) a mesh file two creators load with different properties: each creator applies its own here, where the original kept the first loader's
- ParticleMeshCreator::CreateParticle 0x6A8B00: the Particle3DObj. Its CastHumanShadow node (0x6A8B55..0x6A8B7F) is the shadow list's entry of the atom, made when the shadow list first meets it in HumanShadows (Mesh.h, castHumanShadow)

### Creators

The registered creators derive from `Creator` ([PSys class registry](particles.md#psys-class-registry)). Those of the miracles:

#### The particle meshes and the shield dome

- ✅ faithful: the ctor of `ParticleMeshCreator` 0x6A8960 sets `MeshChangeMaterialProps` = 1 and two-sided = 1 (before,
  0 was taken): thus **`UseAdditiveAlpha` applies to the dome** (SF_DefenseSphere does not give MeshChangeMaterialProps)
  and to the lightning bolt. With it, `GJUtils::SetMaterialProperties` 0x57E120 gives mode 13 (SRCALPHA / ONE, no Z
  write): the transparent pass now draws it that way (`RenderContext::additiveInstances`, `Renderer.cpp`).
- **Faithful:** `UsePlayerColor` / `UsePlayerColorBlend` (fn_006A85E0, for **all** creators, sprites included: it was not
  applied in any of them): pc = `GetPlayerColour` of the effect's player with alpha 0xFF (the neutral's black becomes
  white); with blend b = ftol(Blend × 255) & 0xFF ≠ 255, each channel 255 + ((c − 255) b >> 8); the atom's colour × pc
  >> 8 per channel. The dome uses 0.5 (with player 1's red: (254, 162, 162)). `particles::TintWithPlayerColour`.
- **Faithful:** `FaceCamera` (0x67A032): θ = atan2(d.z, d.x) − atan2(r2.z, r2.x) with d = position − camera in x/z and r2
  the Z row; r0' = cos θ r0 + sin θ r2, r2' = cos θ r2 − sin θ r0; the Y row × `HeightStretch` (ctor 1).
  `FaceCameraSprite` (0x67A250) is not used by any file; it is ported (`billboard::FullSprite`) for completeness. Both
  are in [Objects that face the camera (billboards)](../rendering/meshes-and-lod.md#objects-that-face-the-camera-billboards).
- With this **the shield dome is now visible**: the 32 plates
  `MSH_S_SPELLBALLSURFACE02` are drawn in additive mode with the player's colour and form a clear bubble,
  instead of the barely visible patch from before.
- `ParticleBaseMeshCreator` ctor 0x6A87C0: MeshEnum −1, HeightStretch 1, FaceCamera / FaceCameraSprite / the pulse 0;
  `ParticleMeshCreatorAnimTextured`'s ctor 0x6A8BB0 calls it and sets the same flags as `ParticleMeshCreator`
  (0x6A8BBD..0x6A8BF6).
- MaterialProperties +0x59 (the alpha flag, no property, 1 from the ctors): 0 would make every material TexturedAlpha
  (`GJUtils::SetMaterialProperties` 0x57E13D).
- `Particle3DObj::DrawAt` 0x679FD0 tests `FaceCamera` (+0x4D) first (0x67A040) and only otherwise `FaceCameraSprite`
  (+0x4C, 0x67A250), which restarts from the identity times the scale.
- The object's scale obj+0x44 is the drawn PSR's +0x30 (0x67A000..0x67A009 / 0x67A445..0x67A44E).
- Frame count of an atom (+0x114): NumFrames, or 1000 for the sliding textures (AnimTextured) and the animated meshes
  (fn_006A97F0 0x6A9854 / 0x6A988C); AnimTextured's first frame is `PSysRand(N)` over that N (0x6A8E6E, 0x6A8EE2).
- **`DrawCutByPlane` does cut:** the call changes (fn_00679F20 `test al, 4` 0x679F29: vt 0x11C 0x679F4A instead of
  vt 0x104 0x679F52, in both paths), and for the `LH3DStaticObject` of a particle (`LH3DObject::Create(0)`
  0x80B4F8, vtable 0x9A2974) vt 0x11C is fn_0080C050, which is **not** a direct draw: plane from fn_00822560, clipping
  per triangle (fn_0081D2C0) and light fn_00858BA0, the same as the animated fn_00811C70
  ([Cutting by the water plane](../rendering/meshes-and-lod.md#cutting-by-the-water-plane)). The bit is +0x24 & 4 of the
  atom, set from +0x5F of the creator (0x6A8B94..0x6A8B9A): `particles::mesh_atoms::Instance::cutByPlane`, which
  `RenderingSystem` sends to `sea_pass::CutAtoms` (default plane, what is at y ≥ 0 is visible).
- **⚠️ approximate** the colour goes through the object tint of `vs_object` (−1 − r·65536 − g·256 − b in the x of the fifth
  column, `argb_colour::PackInstanceTint`; without `DrawWithLandscapeColor`, 1 + rgb with `PackInstanceColour`), which multiplies the ground light: that is what `DrawWithLandscapeColor` (fn_0080BEC0) does; without that flag the
  original sets only the colour (`SetColour` vt 0x2C → obj +0x4C / +0x50). Not ported: `UseScriptHightlightPulse`
  (fn_0070A510), `UseDynamicLighting` (bit 0x20), `UseGlobalAlpha` and the per-object Z
  order. `CastHumanShadow` (+0x5D) is ported: CreateParticle 0x6A8B76 creates a
  ShadowInfo per atom (fn_006CA340) and DrawAt puts it, with the particle's object, in the list [0xD4EDCC]
  (0x67A45D..0x67A494); every frame GGame::Process3dEngine 0x54DEAD → PSysLightMaps::AddDrawing 0x6CA6E0 →
  fn_006CA540 walks the list and updates each shadow (fn_006CA3D0, called at 0x6CA5A9 → fn_00874850), so the
  original does cast it. It is not noticeable because it is 0 in the 20 mesh creators of the data. They do not
  receive shadows either: +0x54 (vt+0x78) is always 0 (`k_ReceivesShadow`).
- `CastHumanShadow` +0x5D exists only in `ParticleMeshCreator` (DefineProperties 0x6B393E): in AnimTextured +0x5D is
  its `NeverClip` (vt+0x98, 0x6A8D7C) and ParticleAnimCreator has none. The particle's dtor fn_006C7A80 0x6C7AA6 takes
  its shadow out (fn_006CA370 → fn_008745E0 → fn_0087FF10).
- **`ParticleAnimCreator`** (the forest butterflies and bats, SF_Butterflies, SF_ButterfliesOnObject):
  each atom is a boned mesh that plays an .anm. **Faithful**, except for what is marked.
  - **On creation** (CreateParticle, vt 0x10 0x6A98C0 → `fn_006A97F0`): each particle has its own type 2 object
    (`CreateLH3DObject` 0x6A9760: `LH3DObject::Create(2)`, the mesh with vt 0xF4 and the clip with vt 0x180). In the atom
    (0x6A9843..0x6A98AD): the rate +0x110 = 1000 ([0x8AB228]) / ms of the clip (LH3DAnim +0x20) × `SpeedUpFactor` (+0x44)
    × 1000; +0x114 = 1000 frames per cycle; +0x118 `PlayAnim` (+0xA1) and +0x119 `LoopAnim` (+0xC). With
    `RandomiseInitFrame` (+0xA2), the first frame is `PSysRand(1000)` (SetFrame 0x674100, in +0x108 and +0x10C).
    Default values of the ctor (0x6A93A7..0x6A93E5): SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0,
    NeverClip 0, UseDynamicLighting 1, UseGlobalAlpha 1, FrameToStartBlend 0, FrameToEndBlend 1000.
  - The ctor 0x6A9200 also sets +0x90 UseAdditiveAlpha 0, +0x91 Z write 0, +0x92 double-sided 1, +0x93 and +0x94 1;
    it has no `MeshChangeMaterialProps` property.
  - **On drawing** (`Particle3DAnim::DrawAt` 0x67A8E0): the integer frame of fn_00679920 (DrawData +0x10, 0..999)
    becomes clip time with `GetCycleTimeFromFrame` 0x6C85F0: ms × f / 1000, in integers (imul and then
    × 0x10624DD3 sar 6, which rounds towards 0). That time goes to the particle (+0x28) and to the object (vt 0x188 fn_0080B880:
    +0x84). The drawing of the type 2 object (fn_008175B0, 0x8177B8..0x8177CE) sets the bones with `LH3DAnim::GetPose`
    0x839980 of clip +0x80 at that time. **Frame 0 is not drawn**: 0x67A9B7..0x67A9BE exits before vt 0xF8 and
    the draw.
  - **The clip** (`fn_006A9570`): with AnimEnum −1, the `AnimFileName` loaded with fn_00839900 (the whole file with
    LHLoadData 0x83993C, then the fixes of fn_0083A610; thus LH3DAnim +0x20 is the 0x20 of the header).
    `AnimFileName` is a std::string at creator +0x80 whose pointer +0x84 fn_006A9570 reads (0x6A959D).
    S_Butterfly_Flap.anm lasts 366 ms (2732 frames per second) and M_Bat_Flap.anm 800 ms (1250 per second). The
    meshes: `S_Butterfly.l3d` (with bones, flags 0x22103) and `MSH_A_BAT_1`.
  - **In openblack**: `particles::AnimFrameRate` and `particles::AnimCycleTime` (`Particles/Creators/Mesh.h`). `mesh_atoms::Collect`
    puts the bones in `Instance::pose` with `graphics::ComputePose`, the same code as the villagers and the animals.
    `RenderingSystem` passes them to `RenderContext::instancePoses`, and `ecs::PosesByInstance(renderCtx)` merges them with
    those of the entities. The renderer draws each atom with its bones, like a villager: the 32-bone variants of
    `vs_object` (`Renderer::FewBonesVariant`), no new shader. `mesh_atoms::Any()` replaces `!Collect().empty()`
    in `magic::Update`, so as not to compute the poses twice per frame.
  - (faithful, game_random) `PSysRand` (function pointer [0xD4E0BC]) is the one from `game_random::particles`: `s % n` over the
    step's stream (0x6A98A2 in ParticleAnimCreator, 0x6A8EE2 in AnimTextured).
  - (pending) `AnimEnum` (+0x7C, `LH3DAnim::AnimPack` [0xEDD508], pack[0] out of range, 0x6A957C..0x6A959A):
    no file uses it. The DrawAt blend 0x67A946..0x67A9B1 to `MeshFileName1/2` (+0x38 / +0x3C, both are
    needed) between `FrameToStartBlend` and `FrameToEndBlend` (vt 0xDC fn_007F9A80): `NULL_STRING` in all of them.
    `UseSuperSortedPolys` (vt 0xD4; 0 in all), `UseDynamicLighting` (vt 0x58 fn_008168C0, the model light), `UseGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 of the object's +4; 1 in all: the alpha is
    drawn as in every PSys mesh) and `NeverClip` (vt 0x98 fn_007F98E0 with !NeverClip). The renderer culls by
    the sphere of the rest box, which is not the original's clipping. `ParticleAnimWithCameraCreator` is below.
- **`ParticleAnimWithCameraCreator`** (only SF_Forest has one, `ParticleAnimWithCameraCreator0` of group 2;
  `Particles/Rules/Forest.cpp`, its rules in `Particles/Rules/Forest.h`, `particles::forest_camera`): a `ParticleAnimCreator`
  with no mesh whose particle carries a camera path. **Faithful**, except for what is marked.
  - **The creator** (vtable 0x937C54, ctor 0x6A9920): `PauseBeforePlay` +0xBC, **4.0** by default (0x6A9957), the path
    +0xA8 and its file name +0xAC; DefineProperties 0x6B42A0 (`PauseBeforePlay` float 0..10, `CameraFileName`). The
    `.cam` is loaded once (vt 0x14 0x6A9A00 and vt 0x1C 0x6A9AE0 → fn_006A9A20: LHFileLength, LHLoadData, copied into
    +0xA8 by fn_0086D4A0). Here: once into the camera path cache, as `psys/SPELLS/Anims/Forest.cam`.
  - **No mesh:** the anim creator's ctor sets MeshEnum −1 (0x6A93A0) and fn_006A95E0 then loads `MeshFileName`, which is
    `NULL_STRING`: the particle's object (`CreateLH3DObject` 0x6A9760, type 2 with the clip) has no mesh, and nothing is
    drawn. Here the creator is the anim creator's own reading of the object (its registered factory) without the two
    mesh properties: mesh id 0, which `mesh_atoms` skips.
  - **The particle** (CreateParticle vt 0x10 0x6A9B00: fn_006A85E0, `Particle3DAnimWithCamera` of 0x40 bytes, the anim
    creator's setup fn_006A97F0; ctor 0x6C8630): +0x34 "wants the camera" = 1, +0x35 "mode made" = 0, +0x36 "glide
    set" = 0, +0x3C "playing" = 0, +0x38 the pause. Here `forest_camera::Flags` (`wanted`, `asked`, `playing`), kept in
    the file's module of the particle system; +0x36 is the camera path system's.
  - **Its step**, `GameUpdate` 0x6C86F0 (vt 0x108), called at the very end of the atom's update fn_00673EA0 (0x674080,
    after its sub-collections and its frame step, while [0xC029D8] is set): `forest_camera::Step` from
    `Creator::AfterAtomStep`.
    1. +0x34 &= ([0xC029E0] && `PSysManager::NetUnsafeIsMyInterfaceCasting` 0x673540) (0x6C86F0..0x6C871C). [0xC029E0]
       is 1 and nothing writes it; the query is 1 for an effect with no spell (psys +0xA8 null), else the spell's.
    2. Not mine (fn_00673500, the same function) with +0x35 set: `StopCamera` (0x6C8728..0x6C873A). It then finds +0x34
       already cleared by step 1 and does nothing, so a taken camera is not given back that way (a spell's answer does
       not change during its life, so this is never met).
    3. With +0x34: the atom's matrix (atom +0xD0) is copied into the object (0x6C874A..0x6C875F); at the first such step
       `new CameraModePath(GetCamera(), this)` and +0x35 = 1 (0x6C8769..0x6C8797), at a later one +0x3C = 1 once the
       atom's age > the pause (strict, 0x6C879D..0x6C87AE).
    4. Always: the atom's PlayAnim (+0x118) = 1 once its age > the pause (0x6C87B2..0x6C87C4). Its frame moves from the
       next step, this step's frame step being done.

    Here: `CameraPathSystemInterface::Begin(owner, path, placement, pause)` with the atom as the owner and its drawn
    matrix (rotation × scale, Y × stretch, position) as the placement. ⚠️ approximate The matrix is taken once, at
    `Begin`: no rule of SF_Forest moves group 2, so the copy every step is the same matrix.
  - **Its draw**, `DrawAt` 0x67AA30: the anim particle's draw sets the clip time +0x28 from the drawn frame
    (`GetCycleTimeFromFrame`), the path's time; then with +0x35 and the drawn frame ≥ the frames of a cycle − 1 (frame
    999, 0x67AA45..0x67AA62), `StopCamera` 0x6C86C0: with +0x34, fn_00441E00 marks its path mode invalid, then +0x34 = 0.
    Here: `forest_camera::UpdateFrame`, once a frame after the camera's update (Game.cpp, with the frame updaters):
    `Atom::DrawnFrame` at the effect's draw fraction, `camera_path::PathTimeFromFrame` and `FollowAt` (the time and
    +0x3C), and `Release` at frame 999. It runs every frame for every live camera particle, as the original's does:
    - **Read in the original: nothing culls it, off screen or at alpha 0.** The chain to it is
      `GGame::Process3dEngine` → `Spell::DrawSpells` 0x7203F0 (0x54E023, every spell of g_game +0x205BC4, with no test)
      → `Spell::Draw` 0x720430 (SpellForest's vt +0x50C; the spell's effect +0xB0, made in `Spell::InitWithPos` at
      0x71FF91) → `GJPSysInterface::Draw(true)` vt 0x108 → `PSysManager::Draw` 0x679840 → fn_00679860 (every
      collection) → fn_006798B0 (every atom of a collection, then its sub-collections) → fn_00679920 → the particle's
      vt 0xFC, this `DrawAt` (0x679C1F). Along it there are only tests that never fail or are not about the screen:
      [0xC029AC], [0xC029C8] and [0xC0299C] are the particle system's draw switches, 1 in the data and never written
      (their one indexed reader, fn_0068F540, only reads them); the atom has a particle (+0x128); and the atom's +0x94
      has bit 0x10 (set as it is made, `CommonInitNewAtom` 0x674CB7, and cleared only by the fireworks' and the fused
      explosion's rules) and has been stepped: +0x94 & 6 is 2 after its first step and 4 after its second
      (fn_00673EA0 0x673F80), and the draw takes 4, or 2 with bit 0x01 (which only the emitters set).
      No point of the atom is tested against the camera or the screen, and the alpha the draw data carries (worked out
      in fn_00679920 at 0x679A1D..0x679A71 and scaled by [0xC0215C]) is never compared before the call.
    - Inside, `Particle3DAnim::DrawAt` 0x67A8E0 sets the clip time +0x28 (0x67A904) and the object's frame (vt 0x188)
      before anything else; its exit at frame 0 and the object's own draw (vt 0x100 / 0x104, where the 3D library clips)
      come after. The stop at frame 999 is tested after the base draw returns (0x67AA45), whatever it drew. So the
      path's time and the stop happen whether the particle is on screen or not, and whatever its alpha.
    - Here: no cull either; `UpdateFrame` reads the drawn frame of every live camera particle with no camera, screen or
      alpha test. The one gap is the first two steps: the original's draw skips an atom not yet stepped twice, while
      `UpdateFrame` reads it from `InitAtom`. Before its first step no path has been asked for (`Begin` comes at that
      step), so `FollowAt` finds none; between the first and second steps it gives the path time 0 and "not playing",
      the particle's own values until its first draw (+0x28 and +0x3C are 0 from the ctors 0x6C8540 / 0x6C8630, and
      +0x3C is set only past the 4 s pause). Nothing differs.
  - **Its removal**, the dtor 0x6C86A0 (its symbol says `GCamera::InvalidatePathMode`): with +0x35, `StopCamera`, then
    the base dtor fn_006C85C0. Here `Creator::AtomRemoved`. The atom dies at 25 s (`RemoveRuleOldAgeOnly_Camera`), or
    with its effect.
  - **Its time line in SF_Forest** (`Tree_Goddess_test.anm` and `Forest.cam` both 6633 ms, Forest.cam 198 points,
    `SpeedUpFactor` 0.6): asked at its first step; plays at the first step past 4 s of age, its frame moving from the
    next one; 1000 / 6633 × 0.6 × 1000 ≈ 90.46 frames a second, so frame 999 comes about 11 s later and lets the camera go at about 15 s of age,
    well before the fade out (22.1 s) and its death (25 s). The test with the game's data checks these numbers.
  - No random number: fn_006A97F0 draws only with `RandomiseInitFrame` (0 here), and none of the functions above
    draws one.
  - The camera's side, `CameraModePath`: [Placed paths (CameraModePath)](../camera/script-camera.md#placed-paths-cameramodepath).
  - Tests: `test_particle_forest_camera.cpp` (test_magic): `ForestCameraRules.*` (the rules on their own),
    `ForestCamera.*` (the creator on synthetic effects with a recording camera path system, the same random draws as
    the plain creator it replaced, and SF_Forest's own numbers with the game's data).

- +0x5D CastHumanShadow (DefineProperties 0x6B393E; ParticleMeshCreator only: AnimTextured's +0x5D is its NeverClip, vt+0x98 0x6A8D7C). CreateParticle 0x6A8B55..0x6A8B7F gives each atom's particle a node of 0x10 bytes (+0x1C; fn_006CA340: +0 / +4 the links, +8 the holder of fn_008745A0 -> fn_0087FD50, whose si+0xC = 1 at 0x8745C8 and holder+4 = 0 at 0x8745C1, +0xC 0); every Particle3DObj::DrawAt puts the particle's object (+0x20) in the node's +0xC and pushes the node on the list [0xD4EDCC] (0x67A45D..0x67A494, on every path of DrawAt); every frame GGame:: Process3dEngine 0x54DEAD -> PSysLightMaps::AddDrawing 0x6CA6E0 -> fn_006CA540 (0xD4EDB0, its +0x1C is that list) updates each node's shadow with its object (fn_006CA3D0 0x6CA5A9 -> fn_00874850, the generic update of the physics objects) and fn_006CA660 empties the list (0x6CA69E..0x6CA6CC). The particle's dtor fn_006C7A80 0x6C7AA6 takes the shadow out (fn_006CA370 -> fn_008745E0 -> fn_0087FF10). Here: mesh_atoms::HumanShadows, the list of the last Collect, which graphics::shadow_list reads. The property is 0 in all 20 mesh creators of the spell files (tmp_dis\psys\stats.txt)
- The frame count of an atom (+0x114): NumFrames, or 1000 for the sliding textures and the animated meshes (fn_006A97F0 0x6A9854 / 0x6A988C: 0x3E8)
- fn_006A97F0 0x6A985F..0x6A9882: the atom's frame rate +0x110 = 1000 ([0x8AB228]) / the clip's ms (LH3DAnim +0x20, fild) x SpeedUpFactor x 1000 ([0x8AB228]): one cycle of 1000 frames in the clip's length, faster by the factor
- Then (0x6AA65E..0x6AA680): RandomiseScale (+0x78) ? PSysFloatRand(InitialScaleMin (+0x70), InitialScale (+0x30)) (0x6AA66D) : InitialScale -> atom +0x74 (it replaces fn_006A85E0's)
- The clip (fn_006A9570, set on each particle's object by vt 0x180 fn_008185C0 at 0x6A97A2) and the ctor's defaults (0x6A93A7..0x6A93B5): SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0. Every spell file (SF_Forest, SF_Butterflies, SF_ButterfliesOnObject) names an AnimFileName and no AnimEnum. (pendiente) AnimEnum (+0x7C, LH3DAnim::AnimPack [0xEDD508], pack[0] out of range, 0x6A957C..0x6A959A); the blend of DrawAt 0x67A946..0x67A9B1 to MeshFileName1/2 (+0x38 / +0x3C, both needed) between FrameToStartBlend and FrameToEndBlend (vt 0xDC fn_007F9A80): NULL_STRING in every file; UseSuperSortedPolys (vt 0xD4, 0 everywhere), UseDynamicLighting (vt 0x58 fn_008168C0) and NeverClip (vt 0x98 fn_007F98E0 with !NeverClip). UseGlobalAlpha (+0xA5, 1 in every file) is honoured: Instance::globalAlpha
- CastHumanShadow: ParticleMeshCreator's +0x5D only (DefineProperties 0x6B393E..0x6B3949); AnimTextured's +0x5D is its NeverClip and ParticleAnimCreator has none. Its shadow: MeshCreator::castHumanShadow, mesh_atoms::HumanShadows
- 0x6A8EE2: PSysRand(N), N = NumFrames (+0x64) or the slide's 1000 (0x6A8E6E)

#### Chains and light maps

- **`ParticleChainCreator`** 0x6AA900: the atoms of a collection are the joints of **one** ribbon. The per-collection
  drawing (fn_0067B3F0) orients it to the camera: at each joint the side vector is
  `normalize(cross(camera − joint, segment direction)) · scale` and the vertices are `joint ± side`
  (the PSR's whole scale is the half-width, 0x67B9E6..0x67BB0D; an exactly null side, e.g. a segment of
  length 0, stays null, 0x67BA04..0x67BA39). Where two segments meet, the vertices are moved to their midpoint
  (0x67BD2D..0x67BE82). Indices per segment (0, 1, 2) and (1, 3, 2) (0x67B77A..0x67B7D8). UV (fn_006C8920, with uv0 at
  head + side): **U goes across the width**, `[(frame+FileOffset)·FrameWidth, +FrameWidth]/256`, with frame
  `FrameOfHead` in the last repetition, `FrameOfTail` in the first and 0 in the rest. **V goes along the length**,
  `FrameHeight/256` per repetition; there are `NumTexturesForWholeChain` repetitions, −1 = one per segment. Details in
  [Frame-animated textures](../rendering/textures.md#frame-animated-textures).
  - Default values of the creator: 64 high, 32 wide, −1 (ctor 0x6AA739..0x6AA747).
  - Thus the lightning bolt uses strip 0 of `S_Lightning.raw` (white core and cyan halo), 4 times.
  - Before, the port had 256×256 and the axes swapped, and the ribbon came out almost transparent.
  - `ChainJoint::DrawAt` 0x679E80; the ribbon fn_0067B3F0 calls the UV fn_006C8920 at 0x67BEFD with frame 0. Textures
    `S_Lightning.raw` / `S_Beam.raw`; users: the lightning bolt, the storm lightning, the gesture trail and the
    creature beam.
  - `ParticleChainCreator` ctor 0x6AA6E0..0x6AA751; `CreateChain` 0x6AA880 copies the layout to the Chain (ctor
    0x6C8830): FrameWidth +0x64 → chain +0x20, FileOffset → chain +0x34, repeats → chain +0x30.
  - Repeat of each segment (fn_006C8920): segment s falls in repeat k = ((s + 1)·T − 1) / (n − 1) (integer division),
    which starts at segment k·(n − 1)/T; with a single repeat the head frame wins (the k == T − 1 test comes first).

  - `NumTexturesForWholeChain` 0 (allowed by DefineProperties) makes the original divide by zero (idiv 0x6C893E);
    only −1 is replaced.

  For this the `Effect` has a new `CollectChains` that returns the chain-type collections with their
  joints **in order** (the normal `Collect` flattens them). It is drawn after the sorted sprites, in
  `MainBlended`. A collection without bit 2 of +0x38 (the forks of the lightning bolt) is drawn without interpolation
  (fn_00679920 0x67999E); the frame is always interpolated (0x679A79).
  - The vertical UV scroll over time (chain +0x3C, `frame_anim::ChainScroll`, 0x67BE91, `fmod` by
    `FrameHeight/256`) is ported. Its rate +0x4C is only set by UR_SimpleBeam (SpeedV, ported: see
    [The simple beam](particle-rules.md#the-simple-beam)) and UR_Plasma (not ported); it is 0
    in every other chain. The chain's V scroll and offset are gated by [0xC029B8] (1).
  - Not ported: `UseDynamicLighting` (colour × `clamp(0.6 + 0.4·(n·L))`).
  - `OPENBLACK_PSYS_CHAIN_TRACE=1` writes per frame how many ribbons there are, with how many joints, their texture and
    where they go from and to: it serves to tell "is not drawn" apart from "there are none in that frame".
- **`ParticleLightMapCreator`** 0x6A9D80: `GJBitmap::LoadBitmapFromFile(name, Pitch, 3, NumFramesInFile,
  NumFramesInUse)` are square `Pitch × Pitch` frames stacked, RGB (3 B/px) or grey (1 B/px) — the lightning bolt's,
  `S_lightning_lightmap_with_border.raw`, is 1200 B = 5·5·16·3. The original puts them in the list 0xD4EDB8 and
  `PSysLightMaps::AddDrawing` 0x6CA6E0 **stamps** them (fn_0086CFF0) onto the terrain's dynamic light texture.
  - `ParticleLightMapCreator` ctor 0x6A9CE0 defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter; its
    bitmap (creator +0x34) comes from `GetBitmap` 0x6A9D40.
  - `ParticleLightMap::DrawAt` 0x67B220: frame = DrawData +0x10, position = the atom's +0x24, alpha = DrawData alpha /
    255 ([0x9357AC]); the record goes to the list 0xD4EDB8 (0x67B35A), which fn_006CA660 empties after
    `PSysLightMaps::AddDrawing` 0x6CA6E0.
  - `UseRandJitter`: three `LocalFloatRand(RandJitter)` at every draw, the first to z, the second to y, the third to x
    (0x67B264..0x67B2A3).
- Fix along the way: `TextureBaseName` resolves the name of the `TextureFileName` with **the capitalisation the file has
  in `Data\Textures`** (the spell files write `S_Lightning.raw` and the file is `S_lightning.raw`), so
  now the sprites that were not drawn before also find their texture.

- fn_006C8920 (called from fn_0067B3F0 at 0x67BEFD with frame 0): the chain is cut in T repeats (chain +0x30); segment s falls in repeat k = ((s + 1) T - 1) / (n - 1) (integer division), which starts at segment k (n - 1) / T and holds (k + 1)(n - 1) / T - that of them; FrameOfHead in the last repeat, FrameOfTail in the first (with a single repeat the head wins, the k == T - 1 test comes first), else 0, + FileOffset (chain +0x34) CreateChain 0x6AA880: chain +0x30 = NumTexturesForWholeChain, or joints - 1 when -1 (0x6AA8DC..0x6AA8EB). (aproximado) the joints drawn now, where the original counts the ones the chain was made with
- Only -1 is replaced; 0 (which DefineProperties allows) is left to ChainSegmentUv's openblack guard, where the original would divide by zero (idiv 0x6C893E)
- ParticleChainCreator (props 0x6B4760; ctor 0x6AA6E0..0x6AA751, CreateChain 0x6AA880 copies the layout to the Chain)
- Ctor 0x6A9CE0 defaults: Pitch 1, 1 frame, FrameRate 1, no animation, no jitter
- GetBitmap 0x6A9D40: LoadBitmapFromFile(TextureFileName, Pitch, 3, NumFramesInFile, NumFramesInUse)

#### `ParticleMistCreator`

- Constructor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame,
  InitialScaleMin 1, Ratio 0. Properties 0x6B3C00 (Pitch 1..12, frames 1..32, InitialScaleMin and Ratio 0..5).
- `CreateParticleMist` 0x6AA610: atom scale = `RandomiseScale ? PSysFloatRand(InitialScaleMin, InitialScale) :
  InitialScale`. `CreateLH3DMist` 0x6AA5A0: an `LH3DObject` of type 7 (the `mist.l3d` dome), `k = Ratio` or
  `2.5 + LocalFloatRand(2.5)` if it is 0, and `+0x80 |= 2` (the effect branch).
- `RenderParticleMist::DrawAt` 0x67A670: size = the PSR's scale, with TakeRatioFromMatrix `k = M[1][1]/M[0][0]`, colour
  = the atom's × `[0xFA26A4]` (the base of the terrain light table, with alpha forced to 0xFF): per channel
  `(c × g) >> 8`. Here every frame `mist_atoms::SubmitFrame` (from `magic::Update`) passes each atom to
  the "map" `mists::Submit` (the same `DrawMist`). The base is read by
  `land_light::CurrentTable().GetLandColour()` (the table the renderer built last), through `mist_atoms::MistColour`.
- `GetBitmap` 0x6AA540 loads only with `LoadLightMap` (+0x7A): 1 byte per texel with `IsShadowMap` (+0x79), else 3.
  🔍 inferred the file's Pitch is used as written, outside the 1..12 property range: `S_SMClouds16` of
  SF_LightningStormPush is Pitch 16 (256 bytes, 16 × 16 × 1).
- A `Ratio` of NaN counts as 0 (fcomp 0; test ah, 0x40), so it also draws `LocalFloatRand(2.5) + 2.5`.
- `CreateParticleMist` 0x6AA683..0x6AA6AF: atom +0x110 = 1.0, +0x114 = 1, PlayAnim 0, LoopAnim = the creator's +0x0C:
  one frame that never steps.
- `RenderParticleMist::DrawAt` 0x67A774: in a Sorted effect ([0xC0215D] set) vt 0x100 = fn_007FA7F0 (own Z object at
  mist +0x38, NewZObject 0x7FA87B); otherwise vt 0x104 = fn_007FA790 (0x67A78C), drawn at once inside the effect's
  draw.
- **PSys randomness** (`game_random::particles`, engine-math.md "Random numbers"): each effect carries its NET_GAME_TYPE
  (+0xAC, `Effect(…, NetGameType)`); its step (`Effect::Step` = fn_00673340) sets the synchronised or the local stream and
  at the end the "0" one (0x67349B). Outside a step `PSysFloatRand`/`PSysRand` give 0 without drawing (what
  `fn_00673070` creates when the effect is born happens outside a step, as in the original). `RandR3` without the cap of
  64 attempts. `CreateParticle3DSprite`: the random scale (0x6AA1B4) is only for the sprites, the initial frame is
  `PSysRand(NumFrames)` (0x6AA1E4), the direction `PSysRand(0x100) > 0x80` (0x6AA231), `AtomCore::Create` draws
  `PSysRand(0x100)` (0x673816); the noise grid comes from seed 0 🔍 inferred.
- Each PSys mist carries its own atlas counter (`Atom::mist`), as the original carries one per object. It starts at
  `Random(0,16) & 15` (0x7F95F8; the CRT's `rand()`, `game_random::crt`, the same as the map mists and the storm puffs, not the PSys series) and only
  advances if the mist is on screen (`mists::InView`). The k of each mist is `Ratio`, or `LocalFloatRand(2.5) + 2.5` when it is 0 (CreateLH3DMist 0x6AA5C0..0x6AA5EE, `Atom::mistK`), and the order of CreateParticleMist 0x6AA610 is the original's: the counter (CRT), the k (local GRand) and then the scale (`PSysFloatRand(min, max)` 0x6AA66D). The terrain shadow /
  light map of a mist with `TextureFileName` (the storm) is stamped into the land cells like the light maps.
- SF_Water: the cloud (183, 181, 255, 200), scale 0.2 (0.4 in PU), Ratio 2, and in its group 4 the rain cone
  `MSH_S_RAIN_CONE` (`ParticleMeshCreatorAnimTextured`, from 0.1 to −24 m, scale 0.7 / 1.4, sliding UV).
- **Fix in `ParticleEffect.cpp` (`MakeCreator`)**: the registered creators are looked up before requiring the name to start
  with "Particle" and end with "Creator"; `ParticleMeshCreatorAnimTextured` ends with "Textured", so the rain cone
  (and any other AnimTextured) was never created.

- GetBitmap 0x6AA540 (0x6AA54B..0x6AA586): only with LoadLightMap (+0x7A); bpp 1 with IsShadowMap (+0x79), else 3. 🔍 inferred the file's Pitch as written, not the [1, 12] property range: SF_LightningStormPush's S_SMClouds16 is Pitch 16 (256 bytes, 16 x 16 x 1)
- CreateParticleMist 0x6AA610, after fn_006A85E0 (0x6AA61D, NewAtom's common part): CreateLH3DMist 0x6AA5A0 (0x6AA649) first. LH3DObject::Create(7) 0x6AA5A9 runs the LH3DMist ctor 0x7F9560, which starts +0x84 at ftol(Random(0, 16)) & 15 (0x7F95DC..0x7F95FB) on the CRT rand (game_random::crt), not the PSys stream
- 0x6AA5C0..0x6AA5EE: k (+0x8C) = Ratio (+0x74), or LocalFloatRand(2.5) + 2.5 ([0x8C581C]) when it is 0 (or NaN: fcomp 0; test ah,0x40)
- 0x6AA683..0x6AA6AF: atom +0x110 = 1.0, +0x114 = 1, +0x118 PlayAnim = 0, +0x119 LoopAnim = the creator's +0x0C: one frame that never steps
- RenderParticleMist::DrawAt 0x67A774: with [0xC0215D] set (the effect drawn with Draw_(t, 1), DrawPath::Sorted) vt 0x100 = fn_007FA7F0, the mist's own Z object at mist +0x38 (NewZObject 0x7FA87B): mists::Submit; else vt 0x104 = fn_007FA790 (0x67A78C), drawn at once inside its effect's draw (manager::CollectQueued / HandEffects, mist_atoms::Describe). Until the renderer draws by path (manager::k_DrawByPath) every mist still goes to mists::Submit

### Notes from the code

These notes on the original fit no section above yet.

- ParticleLightMapCreator (props 0x6B45E0)
- ParticleBaseMeshCreator (0x6B37A0) + ParticleMeshCreator (0x6B38B0) / ParticleMeshCreatorAnimTextured (0x6B3970) / ParticleAnimCreator (0x6B3D70)

## openblack

Status: **partial**, with the particle engine ([the particles progress page](../project/progress/rendering/particles.md)).

- `src/Particles/Creators/Mesh.{h,cpp}`: the mesh creators (`ParticleMeshCreator`, the animated and textured ones), with
  `mesh_atoms` for the instances; `src/Particles/Creators/Chain.cpp` and `Graphics/RendererChain.cpp`: the chains;
  `src/Particles/Creators/LightMap.cpp`: the light maps, stamped into the land cells; `src/Particles/Creators/Mist.cpp`: the
  mist creator.

Where it is in the code:

- `Creators/Mesh.h`: `FramesPerAtom`, an atom's frame count (NumFrames, or 1000 for the sliding textures and the
  animated meshes).
- `Creators/LightMap.cpp`: `MakeLightMapCreator` reads the creator (defaults Pitch 1, one frame, frame rate 1) and
  loads its bitmap with 3 bytes a texel.
- `Creators/Mist.cpp`: `MakeMistCreator` loads the light map only with LoadLightMap (1 byte a texel for a shadow map,
  else 3), at the file's own Pitch; `MistCreator::InitAtom` starts the mist's frame counter on the CRT random stream,
  not the particle one, takes the ratio (a random one when it is 0 or not a number) and the scale.

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#particles).

#### Particle meshes and the shield dome

- In openblack: `Creators/Mesh.{h,cpp}`, the `mesh_atoms::Instance` path in `RenderingSystem.cpp` and the PSys branch of `vs_object`.
- The dome patches are drawn by `Creators/Mesh.cpp` (player colour with blend
  0.5, additive by the ctor's `MeshChangeMaterialProps`, and `DrawCutByPlane` also clips the static mesh, fn_0080C050: `mesh_atoms::Instance::cutByPlane`); see
  [Particle meshes](#the-particle-meshes-and-the-shield-dome). The tracer's `OrientToSurface` is already
  ported (above); `MoveToBaseGroup` (in the core) is still unused here (see Pending).
- The tornado's funnel: `ParticleMeshCreatorAnimTextured` does have the property `DrawWithLandscapeColor`
  : its `DefineProperties` 0x6B3970, after those of
  `ParticleBaseMeshCreator` 0x6B37A0 and its own, reads it as the last property into its +0x84 (0x6B3AEF..0x6B3AFD; the ctor
  0x6A8BB0 leaves it at 0 at 0x6A8BF6), and its `CreateParticle` 0x6A8F0D..0x6A8F20 passes it to bit 1 of +0x24 (the one checked by
  `Particle3DObj::DrawAt` 0x67A00C for fn_0080BEC0). The file's `DrawWithLandscapeColor=1` counts: the funnel is
  multiplied by the ground light (`Creators/Mesh.cpp`). `UseGlobalAlpha` (+0x5A) is bit 0 of +0x24
  (0x6A8E04..0x6A8E17) → vt 0x48 `SetGlobalAlpha(1)` (0x67A216..0x67A227), the table 0xC387C8, which leaves the material's mode 6
  unchanged (type 4 → 6 at 0x57E120: `SRCALPHA/INVSRCALPHA`, alpha = texture × diffuse). That it is barely visible **is largely
  faithful**: the mesh's texture (skin 0x16AA, 256 × 256 ARGB4444 inside the .l3d) is white with alpha 1/15..12/15
  (mean ≈ 0.45), × `ColorA` 60/255 → ≈ 0.1 per layer; there are 2 two-sided meshes (`CreateRuleSphere_TornadoMesh`
  `NumAtoms=2`). `ParticleMeshCreator::CreateParticle` 0x6A8B00 never sets bit 0 (`UseGlobalAlpha`). Already ported:
  see [Particle meshes](#particle-meshes-and-the-shield-dome). Checked again by day with the ground light: the funnel looks like a translucent grey
  column with what it drags inside; faint but visible, as the data give.

#### The particle meshes and the shield dome

- In openblack `mesh_atoms::HumanShadows` (the list of the last Collect) is read by `graphics::shadow_list`.

#### Chains and light maps

- openblack counts the joints drawn now, the original the ones the chain was made with ⚠️ approximate.
- In openblack `Particles/Creators/LightMap.cpp` stamps each atom into the land cells with `land_light::AddStamp` (fn_0086CFF0, centred, + (10, 0, 10)), and the mist creator stamps the storm's shadow map the same way.

### Test hooks

- `OPENBLACK_PSYS_CHAIN_TRACE=1`: the chains' segments and UVs. The other particle hooks are on
  [Particles: the PSys core](particles.md#test-hooks).

## Pending

- **Verify with xref/oracle:** the 3 **🔍 inferred** and 4 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`tools/oracle/GUIDE.md`).

- The tracer's `MoveToBaseGroup` (`Effect::MoveToBaseGroup`, already in the core) is not used by the mesh creators
  yet (not checked whether a file needs it).
