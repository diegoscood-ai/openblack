# Particles (PSys)

The original's particle engine as the miracles use it: the particle types, the class registry, the PSys linked to a
spell, the hierarchies, the creators (meshes, chains, light maps, mist), the sound of the particles and an index of
the rules with the page where each one is. The drawing of the world particles (and what remains of the PSys in the
render) is in [The PSys in the world](#the-psys-in-the-world-format-step-drawing-and-water-rules); the miracles, in
[miracles.md](miracles.md); the core of the magic, in [magic.md](magic.md).

- [Particle types](#particle-types-m0-srcpsysparticletypes)
- [PSys class registry](#psys-class-registry-m0-srcpsyspsysregistry)
- [PSys linked to the spell](#psys-linked-to-the-spell-psysspelllinkh)
- [Fix in the PSys core: the hierarchies](#fix-in-the-psys-core-the-hierarchies)
- [Creators](#creators)
- [Sound of the particles](#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)
- [The PSys in the world: format, step, drawing and water rules](#the-psys-in-the-world-format-step-drawing-and-water-rules)
- [Rule index](#rule-index)
- [Pending](#pending)
- [Test hooks](#test-hooks)
- [Sources](#sources)

## Particle types (M0, `src/PSys/ParticleTypes`)

`fn_0068EA00` fills once the table of 150 names `0xD4EBB0` with `.\Data\Spells\ZSpellFiles\<nombre>.txt`; 121
have a file and the rest stay NULL (NONE, TORNADO, FIREWORK, FOOD_IN_HAND...). Some names are stored through a
register (`mov eax, str; mov [ecx+off], eax`), which the old table `psys\pt_table.md` lost: FOOD and FOOD_POISONED →
SF_Food, HEAL and HEAL_FX → SF_HealChakra, LANDSCAPE_VORTEX_OUT_BEFORE → SF_LandscapeVortexInBefore (sic). Reconstructed
with `documentacion\miracles\ptnames.py`.

## PSys class registry (M0, `src/PSys/PSysRegistry`)

Class name → factory for the modifiers (rules, emitters) and the creators. `PSys.cpp` registers the classes that
already existed (`RegisterCoreModifiers`); each new rules or creators file (`src/PSys/Rules/`, `src/PSys/Creators/`)
will have its `RegisterXxx()`, called from the explicit list `RegisterAll()` in `PSysRegistry.cpp`. Whatever is not
registered remains "not ported yet" (no effect). The registered creators derive from `Creator` and call
`ReadCreatorProperties` first.

## PSys linked to the spell (`PSys/SpellLink.h`)

- The spell owns its effect and advances it itself with its PSysProcessInfo (`manager::StartForSpell` /
  `ProcessForSpell`); `ProcessTurn` does not touch it.
- `StrengthFloatProvider` = `info.power`; `EventConditionTrueWhenEnabled` = `info.enabled`.
- `LandscapeCollide` with `SendEvent` sends event 3 (global position, movement of the step, strength 1) before deleting
  the atom. Event 1 is sent at the start (fn_00673070).
- (inf) While a PSys class is not ported, a spell's effect counts it as a creator until it is closed.
  Otherwise, an effect with no ported classes would end in the first turn and cut the spell's cycle short.

## Fix in the PSys core: the hierarchies

- In the original (ctor of `AtomCollection` 0x6748B0, `LocalToGlobal` 0x6751D0 / fn_006752D0, fn_00673DB0,
  `CommonInitNewAtom` 0x674C60): a collection is in a hierarchy if **any** ancestor belongs to a group marked in
  `Hierarchies` (not only the parent); its frame is the product of the local matrices of **the ancestor atoms whose
  group is marked** (the others do not count), and that matrix carries the atom's **scale** (baseScale × ruleScale, the Y
  × the stretch). A new atom is born at 0 if its parent belongs to a marked group and otherwise at the parent's position.
- Ported in `Effect::CreateCollection`, `LocalToGlobal` / `GlobalToLocal` (new), `GlobalPosition`,
  `SpawnPosition` and `PostUpdate` (the frame and its scale pass down through the unmarked atoms; the drawn scale of an
  atom in a hierarchy is also multiplied). The dome needs it: the patches (group 2) and the sparks (group 4) are in the
  frame of the root, which grows from 0.1 to 1 in 2 s (`UR_ChangeScale`) and shrinks when it closes. It also changes,
  towards the original, SF_Bonfire, SF_Smoke/Steam (root 0.8) and the effects in the hand and on the pedestal that scale
  their root (`SetScale`). No rule with its own `GlobalToLocal` (HandFollow, Fireball) is in an affected collection.

## Creators

The registered creators derive from `Creator` ([PSys class registry](particles.md#psys-class-registry-m0-srcpsyspsysregistry)). Those of the miracles:

### The particle meshes (`Creators/Mesh.cpp`, Particle3DObj::DrawAt 0x679FD0) and the shield dome

- **Faithful:** the ctor of `ParticleMeshCreator` 0x6A8960 sets `MeshChangeMaterialProps` = 1 and two-sided = 1 (before,
  0 was taken): thus **`UseAdditiveAlpha` applies to the dome** (SF_DefenseSphere does not give MeshChangeMaterialProps)
  and to the lightning bolt. With it, `GJUtils::SetMaterialProperties` 0x57E120 gives mode 13 (SRCALPHA / ONE, no Z
  write): the transparent pass now draws it that way (`RenderContext::additiveInstances`, `Renderer.cpp`).
- **Faithful:** `UsePlayerColor` / `UsePlayerColorBlend` (fn_006A85E0, for **all** creators, sprites included: it was not
  applied in any of them): pc = `GetPlayerColour` of the effect's player with alpha 0xFF (the neutral's black becomes
  white); with blend b = ftol(Blend × 255) & 0xFF ≠ 255, each channel 255 + ((c − 255) b >> 8); the atom's colour × pc
  >> 8 per channel. The dome uses 0.5 (with player 1's red: (254, 162, 162)). `psys::TintWithPlayerColour`.
- **Faithful:** `FaceCamera` (0x67A032): θ = atan2(d.z, d.x) − atan2(r2.z, r2.x) with d = position − camera in x/z and r2
  the Z row; r0' = cos θ r0 + sin θ r2, r2' = cos θ r2 − sin θ r0; the Y row × `HeightStretch` (ctor 1).
  `FaceCameraSprite` (0x67A250) is not used by any file; it is ported (`billboard::FullSprite`) for completeness. Both
  are in [rendering-objects.md](rendering-objects.md#objects-that-face-the-camera-billboards).
- With this **the shield dome is now visible** (note from review 3a): the 32 plates
  `MSH_S_SPELLBALLSURFACE02` are drawn in additive mode with the player's colour and form a clear bubble
  (`m6b_dome4_t30.png`), instead of the barely visible patch from before.
- **`DrawCutByPlane` does cut:** the call changes (fn_00679F20 `test al, 4` 0x679F29: vt 0x11C 0x679F4A instead of
  vt 0x104 0x679F52, in both paths), and for the `LH3DStaticObject` of a particle (`LH3DObject::Create(0)`
  0x80B4F8, vtable 0x9A2974) vt 0x11C is fn_0080C050, which is **not** a direct draw: plane from fn_00822560, clipping
  per triangle (fn_0081D2C0) and light fn_00858BA0, the same as the animated fn_00811C70
  ([rendering-objects.md](rendering-objects.md#cutting-by-the-water-plane-drawcutbyplane)). The bit is +0x24 & 4 of the
  atom, set from +0x5F of the creator (0x6A8B94..0x6A8B9A): `psys::mesh_atoms::Instance::cutByPlane`, which
  `RenderingSystem` sends to `sea_pass::CutAtoms` (default plane, what is at y ≥ 0 is visible).
- **(approximate)** the colour goes through the object tint of `vs_object` (−1 − r·65536 − g·256 − b in the x of the fifth
  column, `lh3d_colour::PackInstanceTint`; without `DrawWithLandscapeColor`, 1 + rgb with `PackInstanceColour`), which multiplies the ground light: that is what `DrawWithLandscapeColor` (fn_0080BEC0) does; without that flag the
  original sets only the colour (`SetColour` vt 0x2C → obj +0x4C / +0x50). Not ported: `UseScriptHightlightPulse`
  (fn_0070A510), `UseDynamicLighting` (bit 0x20), `UseGlobalAlpha` and the per-object Z
  order. `CastHumanShadow` (+0x5D) is read and **is not ported** (pending): CreateParticle 0x6A8B76 creates a
  ShadowInfo per atom (fn_006CA340) and DrawAt puts it, with the particle's object, in the list [0xD4EDCC]
  (0x67A45D..0x67A494); every frame GGame::Process3dEngine 0x54DEAD → PSysLightMaps::AddDrawing 0x6CA6E0 →
  fn_006CA540 walks the list and updates each shadow (fn_006CA3D0, called at 0x6CA5A9 → fn_00874850), so the
  original does cast it. It is not noticeable because it is 0 in the 20 mesh creators of the data; if a file has 1,
  a one-time warning (`MeshCreator::castHumanShadow`). (Corrects the plan audit, which said that fn_006CA3D0 had no
  callers.) They do not receive shadows either: +0x54 (vt+0x78) is always 0 (`k_ReceivesShadow`).
- **`ParticleAnimCreator`** (the forest butterflies and bats, SF_Butterflies, SF_ButterfliesOnObject; U7):
  each atom is a boned mesh that plays an .anm. **Faithful**, except for what is marked.
  - **On creation** (CreateParticle, vt 0x10 0x6A98C0 → `fn_006A97F0`): each particle has its own type 2 object
    (`CreateLH3DObject` 0x6A9760: `LH3DObject::Create(2)`, the mesh with vt 0xF4 and the clip with vt 0x180). In the atom
    (0x6A9843..0x6A98AD): the rate +0x110 = 1000 ([0x8AB228]) / ms of the clip (LH3DAnim +0x20) × `SpeedUpFactor` (+0x44)
    × 1000; +0x114 = 1000 frames per cycle; +0x118 `PlayAnim` (+0xA1) and +0x119 `LoopAnim` (+0xC). With
    `RandomiseInitFrame` (+0xA2), the first frame is `PSysRand(1000)` (SetFrame 0x674100, in +0x108 and +0x10C).
    Default values of the ctor (0x6A93A7..0x6A93E5): SpeedUpFactor 1, PlayAnim 0, RandomiseInitFrame 0,
    NeverClip 0, UseDynamicLighting 1, UseGlobalAlpha 1, FrameToStartBlend 0, FrameToEndBlend 1000.
  - **On drawing** (`Particle3DAnim::DrawAt` 0x67A8E0): the integer frame of fn_00679920 (DrawData +0x10, 0..999)
    becomes clip time with `GetCycleTimeFromFrame` 0x6C85F0: ms × f / 1000, in integers (imul and then
    × 0x10624DD3 sar 6, which rounds towards 0). That time goes to the particle (+0x28) and to the object (vt 0x188 fn_0080B880:
    +0x84). The drawing of the type 2 object (fn_008175B0, 0x8177B8..0x8177CE) sets the bones with `LH3DAnim::GetPose`
    0x839980 of clip +0x80 at that time. **Frame 0 is not drawn**: 0x67A9B7..0x67A9BE exits before vt 0xF8 and
    the draw.
  - **The clip** (`fn_006A9570`): with AnimEnum −1, the `AnimFileName` loaded with fn_00839900 (the whole file with
    LHLoadData 0x83993C, then the fixes of fn_0083A610; thus LH3DAnim +0x20 is the 0x20 of the header).
    S_Butterfly_Flap.anm lasts 366 ms (2732 frames per second) and M_Bat_Flap.anm 800 ms (1250 per second). The
    meshes: `S_Butterfly.l3d` (with bones, flags 0x22103) and `MSH_A_BAT_1`.
  - **In openblack**: `psys::AnimFrameRate` and `psys::AnimCycleTime` (`PSys/Creators/Mesh.h`). `mesh_atoms::Collect`
    puts the bones in `Instance::pose` with `graphics::ComputePose`, the same code as the villagers and the animals.
    `RenderingSystem` passes them to `RenderContext::instancePoses`, and `ecs::PosesByInstance(renderCtx)` merges them with
    those of the entities. The renderer draws each atom with its bones, like a villager: the 32-bone variants of
    `vs_object` (`Renderer::BonesVariant32`), no new shader. `mesh_atoms::Any()` replaces `!Collect().empty()`
    in `magic::Update`, so as not to compute the poses twice per frame.
  - (faithful, game_random) `PSysRand` (function pointer [0xD4E0BC]) is the one from `game_random::psys`: `s % n` over the
    step's stream (0x6A98A2 in ParticleAnimCreator, 0x6A8EE2 in AnimTextured).
  - (pending) `AnimEnum` (+0x7C, `LH3DAnim::AnimPack` [0xEDD508], pack[0] out of range, 0x6A957C..0x6A959A):
    no file uses it. The DrawAt blend 0x67A946..0x67A9B1 to `MeshFileName1/2` (+0x38 / +0x3C, both are
    needed) between `FrameToStartBlend` and `FrameToEndBlend` (vt 0xDC fn_007F9A80): `NULL_STRING` in all of them.
    `UseSuperSortedPolys` (vt 0xD4; 0 in all), `UseDynamicLighting` (vt 0x58 fn_008168C0, the model light of
    the shaders session), `UseGlobalAlpha` (vt 0x48 fn_007F9D60, bit 0x80 of the object's +4; 1 in all: the alpha is
    drawn as in every PSys mesh) and `NeverClip` (vt 0x98 fn_007F98E0 with !NeverClip). The renderer culls by
    the sphere of the rest box, which is not the original's clipping. `ParticleAnimWithCameraCreator` (the goddess and
    the camera) is still not ported.

### Chains and light maps (`PSys/Creators/{Chain,LightMap}.cpp`, `Graphics/RendererChain.cpp`)

- **`ParticleChainCreator`** 0x6AA900: the atoms of a collection are the joints of **one** ribbon. The per-collection
  drawing (fn_0067B3F0) orients it to the camera: at each joint the side vector is
  `normalize(cross(cámara − articulación, dirección del tramo)) · escala` and the vertices are `articulación ± lado`
  (the PSR's whole scale is the half-width, 0x67B9E6..0x67BB0D; an exactly null side, e.g. a segment of
  length 0, stays null, 0x67BA04..0x67BA39). Where two segments meet, the vertices are moved to their midpoint
  (0x67BD2D..0x67BE82). Indices per segment (0, 1, 2) and (1, 3, 2) (0x67B77A..0x67B7D8). UV (fn_006C8920, with uv0 at
  head + side): **U goes across the width**, `[(cuadro+FileOffset)·FrameWidth, +FrameWidth]/256`, with frame
  `FrameOfHead` in the last repetition, `FrameOfTail` in the first and 0 in the rest. **V goes along the length**,
  `FrameHeight/256` per repetition; there are `NumTexturesForWholeChain` repetitions, −1 = one per segment. Details in
  [rendering-objects.md](rendering-objects.md#frame-animated-textures).
  - Default values of the creator: 64 high, 32 wide, −1 (ctor 0x6AA739..0x6AA747).
  - Thus the lightning bolt uses strip 0 of `S_Lightning.raw` (white core and cyan halo), 4 times.
  - Before, the port had 256×256 and the axes swapped, and the ribbon came out almost transparent.

  For this the `Effect` has a new `CollectChains` that returns the chain-type collections with their
  joints **in order** (the normal `Collect` flattens them). It is drawn after the sorted sprites, in
  `MainBlended`. A collection without bit 2 of +0x38 (the forks of the lightning bolt) is drawn without interpolation
  (fn_00679920 0x67999E); the frame is always interpolated (0x679A79).
  - The vertical UV scroll over time (chain +0x3C, `frame_anim::ChainScroll`, 0x67BE91, `fmod` by
    `FrameHeight/256`) is ported, but its rate +0x4C is only set by UR_SimpleBeam and UR_Plasma, not ported: it is 0
    in all of openblack's chains.
  - Not ported: `UseDynamicLighting` (colour × `clamp(0,6 + 0,4·(n·L))`).
  - `OPENBLACK_PSYS_CHAIN_TRACE=1` writes per frame how many ribbons there are, with how many joints, their texture and
    where they go from and to: it serves to tell "is not drawn" apart from "there are none in that frame".
- **`ParticleLightMapCreator`** 0x6A9D80: `GJBitmap::LoadBitmapFromFile(nombre, Pitch, 3, NumFramesInFile,
  NumFramesInUse)` are square `Pitch × Pitch` frames stacked, RGB (3 B/px) or grey (1 B/px) — the lightning bolt's,
  `S_lightning_lightmap_with_border.raw`, is 1200 B = 5·5·16·3. The original puts them in the list 0xD4EDB8 and
  `PSysLightMaps::AddDrawing` 0x6CA6E0 **stamps** them (fn_0086CFF0) onto the terrain's dynamic light texture.
  - **Deviation (it is not a mod, it is a drawing approximation):** the port does not have that dynamic texture, so each
    frame is scaled to 32×32 in an 8×8 atlas and the atom is drawn as a flat additive square **on the
    ground** (`SetHorozontal`), with the terrain height under the tip. The light does not follow the slope nor tint the
    objects on top. `ShiftX`/`ShiftZ` (≈10, which compensate the +10 of the stamping) are not used.
- Fix along the way: `TextureBaseName` resolves the name of the `TextureFileName` with **the capitalisation the file has
  in `Data\Textures`** (the spell files write `S_Lightning.raw` and the file is `S_lightning.raw`), so
  now the sprites that were not drawn before also find their texture.

### `ParticleMistCreator` (`PSys/Creators/Mist.cpp`)

- Constructor 0x6AA380: RandomiseScale 0, IsShadowMap 1, LoadLightMap 1, TakeRatioFromMatrix 0, Pitch 12, 1 frame,
  InitialScaleMin 1, Ratio 0. Properties 0x6B3C00 (Pitch 1..12, frames 1..32, InitialScaleMin and Ratio 0..5).
- `CreateParticleMist` 0x6AA610: atom scale = `RandomiseScale ? PSysFloatRand(InitialScaleMin, InitialScale) :
  InitialScale`. `CreateLH3DMist` 0x6AA5A0: an `LH3DObject` of type 7 (the `mist.l3d` dome), `k = Ratio` or
  `2,5 + LocalFloatRand(2,5)` if it is 0, and `+0x80 |= 2` (the effect branch).
- `RenderParticleMist::DrawAt` 0x67A670: size = the PSR's scale, with TakeRatioFromMatrix `k = M[1][1]/M[0][0]`, colour
  = the atom's × `[0xFA26A4]` (the base of the terrain light table, with alpha forced to 0xFF): per channel
  `(c × g) >> 8`. Here every frame `mist_atoms::SubmitFrame` (from `magic::Update`) passes each atom to
  the "map" `mists::Submit` (the same `DrawMist`). The base is read by
  `LandLightTable::Current().GetRawBase()` (the global copy of the last table, from the water lane; before,
  `LastBuiltBase` from the storm lane).
- **PSys randomness** (`game_random::psys`, engine-math.md "Random numbers"): each effect carries its NET_GAME_TYPE
  (+0xAC, `Effect(…, NetGameType)`); its step (`Effect::Step` = fn_00673340) sets the synchronised or the local stream and
  at the end the "0" one (0x67349B). Outside a step `PSysFloatRand`/`PSysRand` give 0 without drawing (what
  `fn_00673070` creates when the effect is born happens outside a step, as in the original). `RandR3` without the cap of
  64 attempts. `CreateParticle3DSprite`: the random scale (0x6AA1B4) is only for the sprites, the initial frame is
  `PSysRand(NumFrames)` (0x6AA1E4), the direction `PSysRand(0x100) > 0x80` (0x6AA231), `AtomCore::Create` draws
  `PSysRand(0x100)` (0x673816); the noise grid comes from seed 0 (inferred).
- Each PSys mist carries its own atlas counter (`Atom::mist`), as the original carries one per object. It starts at
  `Random(0,16) & 15` (0x7F95F8; the CRT's `rand()`, `game_random::crt`, the same as the map mists and the storm puffs, not the PSys series) and only
  advances if the mist is on screen (`mists::InView`). The k of each mist is `Ratio`, or `LocalFloatRand(2,5) + 2,5` when it is 0 (CreateLH3DMist 0x6AA5C0..0x6AA5EE, `Atom::mistK`), and the order of CreateParticleMist 0x6AA610 is the original's: the counter (CRT), the k (local GRand) and then the scale (`PSysFloatRand(min, max)` 0x6AA66D). The terrain shadow /
  light map of a mist with `TextureFileName` (the storm) is not ported.
- SF_Water: the cloud (183, 181, 255, 200), scale 0.2 (0.4 in PU), Ratio 2, and in its group 4 the rain cone
  `MSH_S_RAIN_CONE` (`ParticleMeshCreatorAnimTextured`, from 0.1 to −24 m, scale 0.7 / 1.4, sliding UV).
- **Fix in `PSys.cpp` (`MakeCreator`)**: the registered creators are looked up before requiring the name to start
  with "Particle" and end with "Creator"; `ParticleMeshCreatorAnimTextured` ends with "Textured", so the rain cone
  (and any other AnimTextured) was never created.

## Sound of the particles (lane S, `src/Audio/SpellSounds`, `src/PSys/Rules/Sound.cpp`)

The loops and hits of the miracles (teleport pool, tornado, shield, lightning bolts, fireballs...) are played by
the PSys atoms. Report: `visuals_sound.md` §3; what follows is verified in the exe and in `LHaudiodllR.dll`
(`documentacion\sound\dlldis.py`).

- **`SOUND_ACTION` property** (`SoundActionProperty::ReadProperty` 0x585A70, `src/PSys/SoundAction`):
  `<SOUND_*|NO_SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b`. The name is looked up in `Data\SoundAction.h`, which
  the game reads on the fly (fn_00585590, `LHParseFile::FindEnumVal` 0x7BE530); unknown or NO_SOUND → -1. It stores
  LOOPING (bit 0), SOFTRELEASE (bit 2) and USESURFACE (bit 3); ONLYONE is read and discarded. The code sets 0x22 (bit 1
  "delay by distance", bit 5 "ground height") on the thunderclaps.
- **PSysSoundAction** (0x18): action, then the slots passed to the bank (surface +4, size +8, alignment
  +0xC), FadeStep +0x10 and the flags +0x14. Default values (inline ctor, e.g. CreateRuleAnAtom 0x69F350):
  surface 1, **size 2**, alignment 2, FadeStep 0. Correction to the report: without SoundRadiusFP the size is 2, not 0
  (the result is the same in the "*" rows).
- **`AtomCore::StartSound` 0x6745D0**: global position of the atom (with 0x20, terrain height); with USESURFACE,
  `GSoundMap::GetSurfaceType`; alignment of the owning player (`GetDiscreteAlignmentValue` 0..6 → 1,1,2,2,2,3,3; still
  not linked to the player: the slot stays, no row of spells.sad looks at it). It creates a `PSysSound` (0x40, ctor
  fn_006D0F70) at the start of the atom's list (+0x2C) and in the global one 0xD4EE70. With bit 1 it does not play: +0x34 =
  distance to the camera / **347**. Otherwise, `GAudio::SamplePlayAnimEffect` 0x42A4B0 (mode 0) with the attributes
  {size, alignment, 1, surface, action} over spells.sad.
- **`GSoundMap::GetSurfaceType` 0x71D8E0** (`src/Audio/SoundMap`): 6 outside the 512-cell grid or without a block;
  7 if the cell is not land (`MapCoords::IsLand` 0x603720: bit 0x10 of the cell properties, `hasWater`); otherwise,
  the material's `surfaceSound` (1..8, other → 3). The animation sounds now use this same function (before,
  "height ≤ 0 → 7").
- **Bank** (`LHSamplePlayAnimEffect` 0x100146F0 of the DLL): `LHFindAttribRow` (most exact row, `src/Audio/AnimEffectBank`,
  alias of the core's `AnimEffectTable` since audio's B2: its tables are copied from those of the registered bank,
  shared with the animation sounds), a random sample from the list and **it does not play if the camera is farther away
  than the sample's maxDist** (S_TeleportPool: 170). The sample's playback mode (+0x274, bit 0x400) 2 =
  does nothing if it is already playing for that object; thus the loop can be "re-emitted" every turn without duplicating.
  The mode argument: **0 play, 1 `LHSampleStop`, 2 `LHSampleReleaseLoop`** (resolves the doubt in `visuals_sound.md` §7.3).
  `GAudio::SamplePlayAnimEffect` also filters by help/cinema modes and the inside of the citadel: not ported.
- **Lifetime of the PSysSound** (fn_006D11A0, from `PSysGlobal::GameLoopEnd` 0x68F5B0 once per turn, with the turn's
  duration):
  - live atom, LOOPING: if the camera is closer than **1200**, it is re-emitted (mode 0).
  - live atom, delayed: if +0x34 > 0 the turn is subtracted from it; when it passes 0 (strictly negative) it plays. A delay
    that lands exactly on 0 never plays (it is kept).
  - dead atom (StopSound 0x674500 sets +0x38 = 0; the atom's destructor does the same): if it is still playing, with
    FadeStep it lowers the volume `vol − FadeStep` (0..127, no less than 0) every turn, and once (+0x3C) sends mode 2 with
    SOFTRELEASE (the loop finishes its pass) or 1 without it (cut). When it is no longer playing, it is deleted.
  - `PSysSound::Get3DSoundPos` 0x6D1000: the drawn position of the atom (+0xF4), with 0x20 at ground level.
- **Rules** (`Rules/Sound.cpp`): `StartStopSoundOnCondition` 0x69DC40 (SoundCondition true or none → plays if the
  atom does not already have that action; false → stops it with FadeStep), `AddSoundToAtom` 0x69DCA0 (once per atom on
  reaching Delay and with the condition; StopOtherSoundsFirst; the camera shake `LH3DCameraChecker::Create` is not ported),
  `RemoveSoundFromAtom` 0x69DDD0 (once: stops that action with FadeStep).
- **Who plays on creation**: CreateRuleAnAtom (SoundOfCreate; with SoundRadiusFP: < Small 200 → 3, < Medium 500 → 2,
  otherwise 1), CreateRuleSphere (only the first atom), EmitterRuleConical and UR_WillowWisp (SoundEmission on each atom).
  For the fireball lanes: `SizeFromThrow` (> 0.6 → 1, > 0.3 → 2, otherwise 3) and `SizeFromImpactSpeed` of
  UpdateRuleGravityWithFloor fn_006A1630 (< Medium → 3, < Large → 2, otherwise 1; before that it requires alpha ≥
  MinAlpha, |v| ≥ Small and that the atom does not already have that action).
- In openblack: `audio::spell_sounds::ProcessTurn` goes in slot 11, inside `magic::ProcessPSysGameLoopEnd`
  (`MagicLoop.cpp`), which `Game::GameLogicLoop` calls at 0x54E688, after `psys::manager::ProcessTurn` (slot 9),
  the fireflies and the physics, and before `GScript::Process`. Thus the sound sees this turn's atoms, as in the
  original, and in the original's order. The bank loader in
  `Game.cpp` stopped reading a
  .sad at the first empty sample: spells.sad has an empty one at 31, so 32..88 were missing (teleport
  pool, lightning bolts, fireworks...). Now it skips it and continues.
- Test: `OPENBLACK_PSYS_SOUND_TRACE=1 OPENBLACK_TEST_PSYS="SF_TeleportVortex,1478,2129,0,1,5"` in Land1 (initial
  camera at 157 from the point, within the 170 of S_TeleportPool): "start SOUND_SPELL_TELEPORT_POOL (76) ... ->
  spells.sad/39 (S_TeleportPool.wav) looping", at 5 s "release loop" and half a second later "deleted" (it finishes its
  pass). `test_spell_sounds` checks the enum, the flags, the sizes and, with `OPENBLACK_GAME_PATH`, the real
  rows of spells.sad.
- Not ported: the owner's alignment, the camera shake, the game-state filters, the finite repetitions
  (loop > 0 is played once) and the DLL's global distance cap (+0x44 of the audio system).

## The PSys in the world: format, step, drawing and water rules

Comes from rendering.md (the part of the PSys that the render had). **Faithful** except for what is stated as not ported.

Full report (format, 136 classes, formulas, runtime, drawing, effect tables):
`documentacion\psys\psys_report.md`; the 132 decompressed files in `documentacion\psys\zzz\`.
- Files: `Data\Spells\ZSpellFiles\SF_X_txt.zzz` (u32 size + zlib) with editor text: header
  `BEGINPROPERTIES` (DeleteOnCloseDown, Hierarchies[25], InitiallyCreated[25], MaxSpellAge) and blocks
  `BEGINCLASS <Clase> <Nombre>`. A loose `.txt` with the same name takes priority (`LHLoadData`): useful for mods.
- Model: each modifier has a `Group` (0..24) and a `Condition`; a *collection* is a live instance of a group;
  `InitiallyCreated` creates the roots at the origin; `NextGroups` gives each new atom its subcollections; `Hierarchies`
  puts the child atoms in the parent's local frame. Nothing moves by itself: only the rules.
- Per-turn step (dt = 0.1 s) with the previous and current draw state, interpolated when drawing with the turn fraction.
  The atom's frame is stored by fn_00673EA0 in [0, 2N) together with the previous one, and it is only moved with PlayAnim
  (`Atom::playAnim`, +0x118; without it neither step nor wrap-around); fn_00679920 interpolates the frame **number**
  and truncates it (a step, without blending two frames): `frame_anim::PSysFrameAdvance` /
  `PSysFrameLerp` / `PSysFrameIndex`, see [rendering-objects.md](rendering-objects.md#frame-animated-textures).
  End: without atoms nor creation rules, or age > MaxSpellAge; `CloseDown` activates `TrueOnCloseDown`, releases the
  `RemoveOnCloseDown` rules and deletes immediately if `DeleteOnCloseDown`.
- Drawing: **three paths** depending on who draws the effect (`psys::DrawPath`, `PSysManager::SetDrawPath`; verdict in
  `documentacion\miracles\polish\psys_draw_paths_verdict.md`, interface in `drawpath_fix.md`). **Sorted** = `Draw_(t, 1)`
  (fn_00679840, +0xAE = 1 at 0x67984E): the effect has no Z-sorter object; each sprite (0x840C70), each mesh,
  the opaque ones too (fn_00679F60, key its translation), each mist (fn_007FA7F0, key mist+0x38) and each chain
  (fn_0067B380, key the joint n/2) enter the queue on their own; the ZR_SurfRevol surfaces are drawn
  immediately (0x67CBA0). It is used by `Spell::Draw` 0x720441, the storm 0x72DCB7, the shield 0x72D160, the teleport
  0x5FCDC5, the dispenser 0x722A13, the flock 0x72420D, the hand utilities, `TownCentre::DrawPSys` 0x69BF19:
  it is the default path. **Queued** = `AddDrawing` 0x6797D0 (+0xAE = 0 at 0x6797DE): **one** object at `GetOrigin`
  and inside it all the atoms in the order of fn_006798B0 (atoms of the collection, its chain, child collections); only
  the seed in the ball or the icon (0x51A2CA) and the containers with `GSpotVisualInfo` +0x4C `SingleZSort` = 1
  (0x63E190, 0x63E26A; all the info.dat entries with a file have it). **Immediate** = `Draw_(t, 0)`: everything
  right where the call is; the effect in the hand (`CHand::DrawSpellInHand` 0x46E76A, inside the hand
  object). openblack: `manager::CollectSorted` / `CollectQueued` / `HandEffects`; until the renderer uses them
  (`manager::k_DrawByPath`) everything is still drawn as one object per effect at its origin. Sprites from
  `S_SpriteSheet{1,2,3}` (8×8 cells of 32 px, cell = (FileOffset + frame) & 63), screen-aligned quad
  with rotation atan2(M[0][2], M[0][0]) or XZ plane (`SetHorozontal`); additive mode 13 (102 of 137) or 6, no Z write
  except `MaterialUpdateZBuffer`; no light nor haze except `UseLandscapeColor` (not done).
- Scripts: `SPECIAL_EFFECT_POSITION` / `_OBJECT` (CHL 52/53) → `GParticleContainer` with the `GSpotVisualInfo` table
  (50 entries → PARTICLE_TYPE → file); duration in seconds (−1 forever, 0 the table's lifetime); it follows the object
  and closes if it disappears; returns an object that the script can delete.
- openblack: `src/PSys/PSysFile` (reader), `PSys` (collections, atoms, rules: CreateRuleAnAtom/Sphere, emitters
  Simple/Disk/Conical, UR_WillowWisp, deletion rules, AR_FadeAlpha/FadeCollectionAlpha/FadeOutOnceConditionTrue,
  UR_ChangeScale, SetScale, SetAtomAlpha, UpdateRuleGravity, UR_UpdatePosnFromVelocity, UR_GustyWind (with its own
  noise: VLNoise3To1 not ported), UpdateRuleRotatePrincipalAxis, FollowOrigin, UR_FollowParent, ForceConstant*,
  UR_SphereSurfaceTracer, UR_OrientSpriteWithRandomAngle; conditions and float providers), `PSysManager`
  (effects, script containers, test hook) and `Graphics/RendererPSys.cpp`. The unported classes are logged
  once ("not ported yet") and do nothing.
- **`UpdateRuleGravityWithFloor`** (`PSys/Rules/Fireball.cpp`, a single class shared with the miracles; ctor 0x6A1510, `ModifyAtomCollection` 0x6A1880).
  Default values of the ctor: MaxSpeed 100, Gravity 10, Damping 0, WindMagnification 100, UseWind 1, bounces 0.5/0.5,
  GroundDrag 0, ImpactSpeed 5/20/40, MinAlphaForImpactSoundOrRipple 60, ripple distance 2 (+0x40) and ripple enabled
  (+0x71 = 1), with no property. Per atom, without the per-atom `Condition`:
  - damp = UseDamping and (not DisableDampingForNonHuman or `IsHumanPlayerCasting` 0x673580, which is 0 without a `Spell`);
    wind likewise with UseWind / DisableWindForNonHuman. With wind: v += (wind·WindMagnification·0.1 − v)·Damping·dt
    (wind = `fn_00771B10` = `GClimate::GetWeather(p, 1)`: (int8 x/8, 0, int8 z/8); here `weather::GetWindAt(p, true)` from ECS/Weather); otherwise,
    with damping: v ·= 1 − dt·Damping.
  - v is stored and the atom moves **before** gravity. Ground = `GetAltitude(x, z)`; lowest point = global y
    (`RenderParticle::GetLowestPoint` 0x6C79B0; the mesh one, `Particle3DObj` 0x6C7AE0, is not there because there are no
    mesh particles). Above: v.y −= clamp(v.y + MaxSpeed, 0, 1)·Gravity·atom gravity·dt.
  - Below: it is raised to the ground; d = n·v with the terrain normal; if d < 0, impact (`fn_006A1630`) and bounce:
    vn = n·d, vt = v − vn, drag m = min(dt·GroundDrag, |vt|) in the direction of vt (if |vt|² < 1e-4 the direction
    is +x), v = vt·DampingHorozontalBounce·surface − vn·DampingVerticalBounce. Surface (UseSurfaceForBounce):
    table 0x937574 by `GetSurfaceType` = 1,1,1,1,1,1,**0.2** (deep water),**0.2** (shallow),1,1,25.
  - Impact `fn_006A1630`: nothing if alpha < MinAlpha, if ImpactSound is NO_SOUND (−1), if |d| < ImpactSpeedSmall, if the
    atom's sound is still playing or if ImpactSoundCondition fails; level 3/2/1 according to ImpactSpeedMedium/Large; and a
    ripple on the water. Consequence: the pieces of `SF_ExplodeObject` (NO_SOUND) **never** make a ripple; only the fireball
    (`SF_FireBallThrow*`, SOUND_SPELL_FIREBALL_HIT) makes one. The atoms of this engine do not play sounds yet
    (`AtomCore::StartSound` 0x6745D0), so the "while it is playing" wait does not apply. `CheckShieldDeflections`
    (creature shields) is not there. Unit test `test_psys_water`.
- **`UR_Explosion`** (`PSys/Rules/Explosion.cpp` from Milagros, [magic.md](magic.md); the rings here are
  `PSys/PSysWaterRings` `AddExplosionRings`, the only implementation; ctor 0x67E090, `ModifyAtomCollection` 0x67ECE0, `InitCollection`
  0x67E200), in `SF_BeamExplosionSingle/Many/Loads`. By default InitialDelay 3.5, SmokeDelay 3, BeamDelay 0. Point =
  `GetCurrentParentPos` (the +0x80 of the parent atom or the origin) with y = ground altitude. If the effect closes, it
  closes the lightning container. With collection age > InitialDelay: water rings (above) or scorching; > BeamDelay:
  visual spot BEAM_EXPLOSION_FX (magnitude 1, 60 turns); > SmokeDelay: **SMOKE on dry land, STEAM over water**
  (`IsDryLand`), magnitude 8, 4 s. The 3rd argument of `CreateSpotVisualWithSpecifiedDuration` is the effect's
  magnitude (`GJPSysInterface::Create` 0x68F3A1 `SetScale`). The damage to objects, the `SpellEvent` 2 and the shield are
  done by the Milagros one; the ground mark (`fn_008251C0`: a morphable mesh 0x251 of scale 8, not a shadow; `TemporaryShadow` is
  `fn_00825090`) is in `ecs/GroundMarks` ([rendering-objects.md](rendering-objects.md#meshes-stuck-to-the-ground-land_morph));
  not ported: the mesh debris.
  Test: `OPENBLACK_TEST_PSYS="SF_BeamExplosionSingle,1464,2016,0,1"`, camera `1452,14,2002,1464,0,2016`, capture at
  frame 272 of 300 (rings) or 360 of 400 (steam); `OPENBLACK_PSYS_TRACE=1` writes the explosion.
- Test: `OPENBLACK_TEST_PSYS="SF_Bonfire,1790,2630,0,1"` with the camera `1775,45,2600,1790,30,2630`, `-n 5000`
  (bonfire with flames and smoke); `OPENBLACK_PSYS_TRACE=1` writes the atoms and age of each effect every 20 turns.
- **Beliefs over the town centre** (`src/PSys/TownBelief.cpp`; report `documentacion\psys\towncentre_notes.md`): each functional centre has TOWN_BELIEF (SF_TownBelief, `UR_TownCentreBelief` 0x69BF30), which advances once per frame with dt = 0.1 s. One symbol per player with belief: the first one (rank 0) still 2 units above the top of the totem; the others orbit (radius and speed by belief, at 2.5 per height rank) and the second one fights (flashes). It is drawn with two glows from S_SpriteSheet3 (player colour and white spinning) and the symbol. The human's symbol is the "player symbol" cell of the profile (registry; 0 without it, as in this installation) copied from ChooseSymbol (PlayerSymbol::OpenOnce 0x5DE2F0); the rivals use .cps images (not done). Base: the totem (`components::TotemStatue` of fields): x/z of the pedestal, y = baseY + height of the icon mesh × scale + 2. The owner's SpellColumn column is missing.

## Rule index

Each PSys class that the miracles use, with its address and where it is described. Those in `src/PSys/Rules/` are
registered in `PSysRegistry.cpp`; the rest remain "not ported yet".

| Class | Address | File | Where |
|---|---|---|---|
| `CreateRuleAnAtom`, `CreateRuleSphere`, `EmitterRuleConical` | ctor 0x69F350 (CreateRuleAnAtom) | — | [Sound of the particles](particles.md#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp) |
| `StartStopSoundOnCondition`, `AddSoundToAtom`, `RemoveSoundFromAtom` | 0x69DC40, 0x69DCA0, 0x69DDD0 | `Rules/Sound.cpp` | [Sound of the particles](particles.md#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp) |
| `StrengthFloatProvider`, `EventConditionTrueWhenEnabled`, `LandscapeCollide` (SendEvent) | — | `SpellLink.h` | [PSys linked to the spell](particles.md#psys-linked-to-the-spell-psysspelllinkh) |
| `MagnitudeFloatProvider` | UpdateParams 0x69DA90 | — | [The size of the fireball thrown with the hand](magic.md#the-size-of-the-fireball-cast-with-the-hand-inferred-users-recollection) |
| `UR_FollowLocalHand`, `UR_FollowCastPosn` | 0x69A6A0, 0x69FE30 | `Rules/HandFollow.cpp` | [The hand](magic.md#the-hand-handmagicfxcpp-phandfx-and-the-effect-in-the-hand) |
| `ZR_ChainGesture`, `CreateRuleMakeChain` | 0x68A080, 0x69FD10 | `Rules/Gesture.cpp` | [Utility effects](magic.md#utility-effects-psysutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_GesturingRecognised` | 0x6884F0 / 0x688910 | — | [Utility effects](magic.md#utility-effects-psysutilitycpp-psysutilitypsys-0xd4e0e8) |
| `UR_HandSprinkle`, `UR_WillowWisp`, `AppearanceRuleTumble` | 0x6A0220, —, 0x6A6200 | `Rules/Sprinkle.cpp` (UR_HandSprinkle) | [Food and wood](miracles.md#food-and-wood-m3-magicspellsspellresource-magicobjectsmagicfoodwood-ecspotresource) |
| `UR_HealSpellChakra` | 0x6A0B20 | `Rules/Heal.cpp` | [`UR_HealSpellChakra` 0x6A0B20](miracles.md#ur_healspellchakra-0x6a0b20-psysruleshealcpp) |
| `CreateRuleFusedSphericalExplode` | 0x69F610 | `Rules/Heal.cpp` | [`CreateRuleFusedSphericalExplode` 0x69F610](miracles.md#createrulefusedsphericalexplode-0x69f610) |
| `UR_HealInHand` | 0x6A0F40 | `Rules/Heal.cpp` | [`UR_HealInHand` 0x6A0F40](miracles.md#ur_healinhand-0x6a0f40) |
| `UR_KPStretchHeight`, `UR_KPMoveAtoms` | 0x6A50C0, 0x6A60B0 | `Rules/KeyPoints.cpp` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `UR_FollowTargets`, `UR_Flocking`, `EventConditionAtomNearVillagers` | 0x6A04B0, 0x683580, 0x67D8E0 | `Rules/Flock.cpp` | [The particles](miracles.md#the-particles-psysrulesflockcpp-faithful) |
| `UR_ForestPath`, `ParticleGoodEvilCreator` | 0x6A3770, 0x6AAA00 | `Rules/Forest.cpp`, `PSys.h` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `ParticleAnimWithCameraCreator`, `ParticleAnimCreator` | —; fn_006A97F0, DrawAt 0x67A8E0 | not ported; `Creators/Mesh.cpp` | [The particle meshes](#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome), [Forest](miracles.md#forest-m4b-magicspellsspellforest-magicobjectsmagictree-ecstrees) |
| `UpdateRuleGravityWithFloor`, `CreateWithInitialDirection`, `AttatchFireBallToAtom`, `SetAtomHasBeenDeflected`, `UR_SideSpin`, `AR_FadeAlphaWithHeightAboveLandscape`, `AddSubCollectionsToAtom`, `UR_Trail` | 0x6A1880, 0x69E950, 0x682FD0, 0x6A26C0 | `Rules/Fireball.cpp` | [Fireball](miracles.md#fireball-magic_type-1-3-seed-2-fire) |
| `UR_OrientSpriteWithVelocity` | 0x69A790 | `Rules/Orient.cpp` | [The other missing classes](miracles.md#the-other-missing-classes) |
| `UR_Lightning`, `UR_LightningStrike` | 0x6914C0, 0x6937A0 | `Rules/Lightning.cpp` | [Lightning bolt](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-psysruleslightningcpp) |
| `LightningForkFlicker` | 0x6B24D0 | not ported | [Lightning bolt](miracles.md#lightning-magic_type-4-6-seed-6-lightning_bolt-psysruleslightningcpp) |
| `UR_AddDefensiveSphere`, `UpdateRuleShieldSpark`, `UR_InitialSpin`, `UR_VapourEndEffect`, `SetCollectionAlpha`, `UR_AtomsAtEPTarget`, `CheckShieldDeflections` | 0x6A2A60, 0x6A2BF0, 0x69E490, 0x6A39E0, 0x6A2720, 0x69A960, 0x6A2570 | `Rules/Shield.cpp` | [The particles](miracles.md#the-particles-psysrulesshieldcpp) |
| `UR_SphereSurfaceTracer` | 0x6A32B0 | `PSys.cpp` | [The particles](miracles.md#the-particles-psysrulesshieldcpp) |
| `ZR_SurfRevol`, `UR_ChangeScale` | 0x686370 | `Rules/SurfRevol` | [SF_TeleportVortex and ZR_SurfRevol](miracles.md#sf_teleportvortex-and-zr_surfrevol-srcpsysrulessurfrevol-srcgraphicsrenderersurfrevolcpp) |
| `UR_Explosion` | 0x67E200, 0x67ECE0, 0x67E900 | `Rules/Explosion.cpp` | [`UR_Explosion`](miracles.md#ur_explosion-r5-initcollection-0x67e200-modifyatomcollection-0x67ece0-update-0x67e900) |
| `SpreadingDiskEmitter`, `DiskEmitter`, `SetPSysCloseDown`, `EventConditionCollectionDelay` | 0x6A6610, 0x6A64D0, 0x6A26D0 | — | [The files](miracles.md#the-files-faithful) |
| `UR_MoveAtom`, `UR_ChangeScaleXYZ` | 0x6A5E50, 0x6A5240 | — | [The visible lightning bolt](miracles.md#the-visible-beam-sf_beamexplosionfx-pt-138) |
| `UR_ExplodeObject`, `UR_ExplodeObject2`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef` | 0x6814E0, 0x681560 | not ported | [Not ported / pending](miracles.md#not-ported--pending) |
| `UR_CloudMoverNew`, `UR_CloudGather` | 0x6D41C0, 0x6D4A70 | `Rules/Storm` | [The cores and the clouds](miracles.md#the-cores-and-the-clouds-ur_cloudmovernew-0x6d41c0-ur_cloudgather-0x6d4a70) |
| `UR_Tornado`, `UR_FollowParent` | 0x6D18B0 | `Rules/Storm` | [The tornado](miracles.md#the-tornado-ur_tornado-0x6d18b0-ctor-0x6d1680) |
| `UR_StormCast` | 0x6D59B0 | `Rules/Storm` | [The whirlwind](miracles.md#the-whirlwind-ur_stormcast-0x6d59b0-sf_stormcast) |
| `ParticleMeshCreator`, `ParticleMeshCreatorAnimTextured` | ctor 0x6A8960; 0x6A8B00, 0x6A8DA0 | `Creators/Mesh.cpp` | [The particle meshes](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome) |
| `ParticleChainCreator`, `ParticleLightMapCreator` | 0x6AA900, 0x6A9D80 | `Creators/{Chain,LightMap}.cpp` | [Chains and light maps](particles.md#chains-and-light-maps-psyscreatorschainlightmapcpp-graphicsrendererchaincpp) |
| `ParticleMistCreator` | ctor 0x6AA380 | `Creators/Mist.cpp` | [`ParticleMistCreator`](particles.md#particlemistcreator-psyscreatorsmistcpp) |
| `ParticlePointCreator`, `ParticleSpriteCreator` | — | already there | [Not ported / not verified](miracles.md#not-ported--not-verified-heal) |

## Pending

- Sound: the owner's alignment, the camera shake, the game-state filters, the finite repetitions
  and the global distance cap (see [Sound of the particles](particles.md#sound-of-the-particles-lane-s-srcaudiospellsounds-srcpsysrulessoundcpp)).
- Chains: `UseDynamicLighting` (the V scroll is 0 in all the data); light maps
  stamped onto a dynamic terrain light texture (it does not exist in the port).
- Meshes: `UseScriptHightlightPulse`, `UseDynamicLighting`, `UseGlobalAlpha`, the per-object Z
  order and `FaceCameraSprite`; from `ParticleAnimCreator`, `AnimEnum`, the blend to `MeshFileName1/2`,
  `UseDynamicLighting` and `UseGlobalAlpha`.
- Mist: one atlas counter per mist and the terrain shadow / light map.
- World PSys: mesh creators, mist, chains, animation, light maps (they are stamped onto the terrain light), the
  spell and town rules (`UR_TownCentreBelief` is already there: see above), `CreateRule_GameObjectRef` (the glow of the
  keys of the Land1 gate, SF_HighlightOnObject), the sounds, and moving the hand effects of `HandEffects.cpp` onto this engine.
- The unported rules of the index (pieces, `LightningForkFlicker`, `ER_EmitFromParentAtom`, `CreateRule_GameObjectRef`).

## Test hooks

- `OPENBLACK_TEST_PSYS`, `OPENBLACK_PSYS_SOUND_TRACE` and `OPENBLACK_PSYS_CHAIN_TRACE`, in
  [openblack-internals.md](openblack-internals.md#debug-environment-variables); `test_spell_sounds` and
  `test_lightning`.

## Sources

- `dev\documentacion\miracles\visuals_sound.md` (§3 the sound), `dev\documentacion\sound\dlldis.py` (`LHaudiodllR.dll`),
  `dev\documentacion\miracles\ptnames.py` (the names of the particle types), `psys\pt_table.md` (the old table),
  `psys\part_render.md` and `dev\documentacion\miracles\impl\` (`m4a` mist, `m6b` meshes, `m6s` hierarchies).
