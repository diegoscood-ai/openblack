# Particle rules

## Summary

The rules move a particle effect's atoms: the belief sprites that rise from a town, the beam, the glints on a target,
the rules that act on game objects, the storm's flashes in the hand, and the rules of each miracle (explosion,
fireball, flock, forest, gesture, heal, lightning, shield, storm). This page holds the rules as the original runs them;
the miracles that use them are on their own magic pages.

openblack runs every rule listed here on its own particle engine, ported from the original and checked against the
oracle where the numbers matter; the ones still missing are in the [Rule index](particles.md#rule-index) and in
[Pending](#pending).

## The original

### The belief sprites

When a town gains belief, a sparkle in the believing player's colour rises from the place, wiggles, pulses and fades.
Read from the v1.42 exe and run in the oracle on fake atoms (289 cases, all bit-exact with `BeliefSpriteMaths`; 46 of
them are pinned in `test/test_belief_sprite.cpp`).

- **What queues one.** `GBelief::DrawBelief` 0x438800 (v = ftol(f × 10000), nothing for v ≤ 0, at the thing's point +
  its height, in the player's colour or 0xFFFFFFFF) is called by `GBelief::AddToBelief` 0x437EB0 when its draw flag
  is set (0x437F19) and by `ReduceBelief` (0x43803C, a negative amount: nothing). AddToBelief's callers:
  `Villager::UpdateHowImpressed` 0x763567 (an impressed villager), `Abode::DoResourceAdding` 0x404EC3,
  `Living::PerformDance` 0x5EFC60, `Reward::InterfaceTap` 0x6E5EA0, `Town::GiveBeliefForArtifactIfNecessary` 0x741211 and
  sub_53206F 0x532176. The town's fold of its pending belief, fn_004383D0, also queues one per player at the town
  centre (0x438471). Both go through fn_0069B550, the queue (see [Buildings](../world/buildings.md), the belief section).

- **The effect.** SF_BeliefSprite (particle type 24) is the utility slot +0x1C: made when missing and stepped once a
  turn with the turn's time on the local stream (NET 0), see [Utility effects](../magic/hand-casting.md#utility-effects);
  drawn every frame by `PSysGlobal::DrawLoop` 0x68F5E0 → fn_00672210 → fn_00671D80, `Draw_(1)` (vt +0x108, push 1):
  the Sorted path, interpolated with the turn's fraction. Its file: one `UR_BeliefSprite` in group 0 (InitiallyCreated)
  with a plain `ParticleSpriteCreator` (S_SpriteSheet2, FileOffset 40, 8 frames at 24 a second, looping, random first
  frame and direction, ColorA 120, InitialScale 1.4, StretchVertically 1.1, additive). No player symbol: the
  `ParticleSymbolSpriteCreator` (ctor 0x6AA280, CreateParticle 0x6AA300) is used by SF_PlayerIconFountain only.
- **The rule's object** (0x80 bytes, `StaticCreate_UR_BeliefSprite` 0x6C01C0, DefineProperties 0x6B2180): flags |= 6
  (a creator); PCreator +0x28; RiseSpeed +0x2C (10), DieAge +0x30 (0: calloc, never set), MaxWiggleOffset +0x34 (10),
  AgeAtMaxOffset +0x38 (2), NoiseFreq +0x3C (1), ScaleMax +0x40 (1), ScaleMin +0x44 (0.3), ScaleFreq +0x48 (1),
  FadeTimeStart +0x4C (1.5), FadeTimeEnd +0x50 (2.5), AlphaMin +0x54 (20), AlphaMax +0x58 (150), ValueAlphaMin +0x5C
  (10), ValueAlphaMax +0x60 (100), UseLinearNoise +0x64 (1), SoundOfBelief +0x68 (NO_SOUND, surface 1, size 2,
  alignment 2). The file sets AgeAtMaxOffset 0.5, DieAge 6, MaxWiggleOffset 2, NoiseFreq 2, RiseSpeed 12, ScaleFreq 12,
  ScaleMax 1, ScaleMin 0 and SOUND_SPELL_BELIEF_SPRITE. **DieAge and UseLinearNoise are never read** by the rule (no
  reader of +0x30 or +0x64 in 0x69B750). AtomData (0x30 bytes, ctor 0x55F4F0): +0x20 the start point, +0x2C the start
  alpha byte.
- **`ModifyAtomCollection` 0x69B750.** No PCreator: returns 0 (the rule detaches). Then, while the queue's count
  [0xD4ED08] is not 0, the **last** entry is copied out and the count lowered (0x69B778..0x69B790: last in, first out,
  every entry in one step, no limit per step):
  - `AtomCore::Create` 0x6737F0 (its random byte +0x12C = PSysRand(0x100), the first draw), `CommonInitNewAtom` +
    `AddSubCollections` (fn_00674DD0, NextGroups is empty), then PCreator vt +0x10 (the sprite's two draws: its first
    frame Rand(8) and its direction Rand(0x100)) into +0x128. So three draws per sprite, on the effect's stream, the
    local one.
  - The atom's data: start = the entry's point; start alpha = (uint8)fistp((AlphaMax − AlphaMin) × t + AlphaMin) with
    t = clamp01((v − ValueAlphaMin) / (ValueAlphaMax − ValueAlphaMin)) (fild / fidiv, 0 for t ≤ 0 or NaN)
    (0x69B823..0x69B890).
  - The atom's colour +0x8C: the entry's RGB, the creator's alpha kept (0x69B893..0x69B8B3); its position +0x80 =
    the start; its rule scale +0x78 = 1.0 (0x69B8C5).
  - v > ValueAlphaMin: `AtomCore::StartSound(SoundOfBelief)` (0x69B8CC..0x69B8D7). The oracle: 11 and more sound, 10
    and less do not.
- Then **every atom** of the collection, the new ones too (0x69B8E9..0x69BBF2), with age = `GetAtomAge` and k = its
  random byte, every operation rounded to a float:
  - t = age × NoiseFreq; wiggle W = clamp01(age × (1 / AgeAtMaxOffset)) × MaxWiggleOffset;
  - x noise = N(k + t) + N(2t + k) × 0.5, z noise = N(0.3k + t) + N(0.3k + 2t) × 0.5 (0.3 [0x8AB23C], 0.5 [0x8AA3B4];
    N = `Noise::VSNoise1To1` 0x590BB0);
  - rise = RiseSpeed × ((k mod 13) × 0.0249 [0x936FE4] + 1) × age;
  - position = start + (W × x noise, rise, W × z noise), summed as the original adds (0, rise, 0) first;
  - fade f = age > FadeTimeStart ? clamp01((age − FadeTimeStart) × (1 / (FadeTimeEnd − FadeTimeStart))) : 0; the alpha
    byte = fistp(a0 − a0 × f), a0 the start alpha (0x69BAD5..0x69BB58);
  - only when the atom's particle is a `Particle3DSprite` (vt +0x104 `IsParticle3DSprite`, 0x69BB66): the **stretch**
    +0x7C (not the scale) = (sin(((k × 1/256 [0x936FC4]) × 0.3 + 1) × age × ScaleFreq + k × 0.23 [0x936FE0]) × 0.5 +
    1) × (ScaleMax − ScaleMin) + ScaleMin, the sine unrounded into its product (0x69BB70..0x69BBC8). With the file's
    scales it pulses between 0.5 and 1.5;
  - age > FadeTimeEnd + 0.3 [0x8AB23C] (or not a number): `DeleteFromAtomList` (0x69BBCB..0x69BBE8). With the file's
    times a sprite is opaque for 1.5 s, fades to 2.5 s and goes after 2.8 s (2.8 itself stays).
- So raffclar's constants (a rise step of 0.0249 over 13 steps, a pulse phase of 0.23 and pace of 0.3 / 256, gone 0.3 s
  after the fade) are the original's.

- RenderParticleGJMesh::DrawAt 0x67C150 of the exploded pieces, to graphics::world_triangles. Called WHERE THEY ARE DRAWN (after model_light::UpdateFrameLight of the frame): the original draws them at once, no Z object (0x67C150 does not read [0xC0215D]); EXPLODE_OBJECT is Sorted, drawn by fn_006718A0's Draw_(1) (0x6718A9..0x6718AB) from PSysGlobal::DrawLoop 0x68F60C (fn_00672210 0x672228), after GGame::Draw 0x54E00A and before the queue's drain (FinishFrame 0x82F460).
- 0x67E347..0x67E55B: three water rings (PSys/PSysWaterRings, the one implementation: growth [0x9357D8] = 10 x 0.5, x 0.7 and x 1, angle 0, aspect and rate 1, cell 0x30, 0xFFFFFFFF), or on dry land (MapCoords::IsDryLand, altitude >= 4) the crater (a pack mesh, not a sprite)
- The wiggle: value noise at the atom's shuffled phase, x and z by the ideal's box, y upwards only (the phase offsets +0.3 [0x8AB23C] at 0x6892D3 and +0.7 [0x8AB238] at 0x689308)
- 0x6918E8..0x6919C0: (d1 + (c, 0, s)) x 0.5 [0x8AA3B4] x 0.7 [0x8AB238] x the shorter length (`fcomp; test ah, 1`: length1 when it is the smaller, else length2)

### The simple beam

Wiggling ribbons from the effect's origin to each object or point it is given. Used by `SF_SimpleBeam` (particle type
119, spot visual 20 MAGIC_BEAM), `SF_SimpleBeamCreatureCast` (type 34, spot visual 13 CREATURE_CAST_VISUAL) and
`SF_SimpleBeamCitadelBU` (named by no table). `SF_SimpleBeamCitadel` and `SF_SimpleBeamCreatureSwap` use `UR_Plasma`
instead. All three files give BeamGroup 1, MaxJointsPerFork 20, NumSplinePoints 6, NumBeams 3, RandomFrac 3,
WiggleFreq 3, WiggleSpeed -4, SpeedV 1, MinHeight -4.31602e8, ForkScaleMin/Max 0.1/0.8 and a ParticleChainCreator
(S_Beam.raw).

- The ctor 0x675C50 sets the modifier flags `|6 ^4` = 2: the effect waits for targets while it is not closing, and the
  rule is not a creator. Defaults: NumSplinePoints +0x34 = 5, NumBeams +0x38 = 3, BeamGroup +0x3C = -1, WiggleFreq
  +0x40 = 4, WiggleSpeed +0x44 = 1, SpeedV +0x48 = 1, RandomFrac +0x50 = 0.1, ForkScaleMin/Max +0x54/+0x58 = 1.
  MaxJointsPerFork +0x2C and MinHeight +0x4C are 0: the class factories (0x6C0620 / 0x6C0650) allocate with
  `operator new(size, file, line)` 0x7DB790, which is `_calloc`, and the ctor does not set them (every file gives
  them). PCreator is +0x28 (DefineProperties 0x6B28E0). The DefineProperties ranges (MinHeight 0..5, RandomFrac
  0..0.2, ...) are the editor's only: the float setter 0x584650 stores the value unclamped, so the files' 3, -4 and
  -4.3e8 are used as written.
- AtomData (0x40 bytes, ctor 0x55F110): +0x20 the game turn the object was last seen (unused), +0x24 the object,
  +0x28 the target point, +0x34 the origin. It comes from `PSysBase::new` 0x6755B0 -> fn_006756D0, which fills every
  block with 0xCDCDCDCD (0x6758B6..0x6758C9, `rep stosd`), and the ctor leaves +0x28..+0x3C alone: an object beam's
  target is (-431602080, -431602080, -431602080) until the object is followed, and stays so for an object that is not
  available the step it is taken (or is no GameThingWithPos).
- SubCollectionData (0x34 bytes, ctor 0x55F160): +0x20 the key-point spline (+0x28 = 1: zero end slopes), +0x2C the
  beam's number, +0x30 NumBeams.
- `ModifyAtomCollection` 0x6760E0:
  - Unless the effect is closing (`IsInState(1)`), while the spell targets hold objects or points (fn_006710B0, the two
    counts added), one carrier atom per target: `AtomCore::Create`, fn_00674DD0 (CommonInitNewAtom, then NumBeams
    sub-collections of BeamGroup), no creator (never drawn). Origin = `GetCurrentParentPos` 0x674AF0. An object is
    taken first (`TakeTargetObject`, the last one, cast to GameThingWithPos), else a point (fn_00670F00, the last one).
    The atom's position is the origin.
  - `PCreator->IsParticleBaseChainCreator()` (vt +0x24). For each sub-collection from the head (number 0 = the newest)
    +0x2C = its index; with a chain, `CreateChain(MaxJointsPerFork)` (vt +0x28) and the chain's scroll rate +0x4C =
    SpeedV (0x6762EE); a null chain detaches the rule. Then MaxJointsPerFork joints: `Create`, `InitNewAtom(atom, -1)`,
    `CreateChainParticle(atom, chain, i)` (vt +0x2C), or `CreateParticle` (vt +0x10) without a chain.
  - Then every carrier from the head: an object that `IsAvailable` (vt +0x2C) stamps the turn and gives
    `GetPSysBeamTargetPos` (vt +0x150) into +0x28; otherwise the object is dropped and the point stays. The origin is
    read again and each sub-collection from the head is laid by fn_00675D00.
- fn_00675D00, one beam: keys i = 0..N-1 at t = i x (1 / (N - 1)), P = origin + (target - origin) x t
  (fn_0044CF90, fn_0044E9F0, fn_004605F0). For 0 < i < N - 1, with the globals [0xC022A4] (bulge) and [0xC022A0]
  (noise) both 1 and never written: x += VSNoise1To1(age x speed + t x freq + n) x RandomFrac x b; z the same with
  age x speed x 0.7 [0x8AB238]; y += (VSNoise1To1(age x speed x 1.3 [0x8C7A18] + t x freq + n) + 1) x RandomFrac x b x
  0.5; b = 1 - (2t - 1)^2, age the beam collection's, n its number. Then if y - GetAltitude(x, z) < MinHeight, y =
  ground + MinHeight (fn_004427B0 makes the map coordinates, 0x803090). The spline is built by fn_006771C0 ->
  fn_00677490 and read by fn_006771F0 (Numerical Recipes' spline / splint on a vector key of 0x1C bytes: t, value,
  second derivative); splint leaves its output untouched when the two keys share t. Joints from the head, u = j x (1 /
  (count - 1)): position = splint(u) into a buffer that first holds the last key point; ruleScale (+0x78) = (1 - (2u -
  1)^2) x (max - min) + min, with min/max = magnitude (+0xA0) x ForkScaleMin/Max. One joint gives u = 0 x (1 / 0), not
  a number.
- `GetPSysBeamTargetPos`: GameThingWithPos 0x5702E0 = the position + GetHeight (vt +0x42C) x 0.5; Creature 0x4F8B30 =
  the translation of one bone of its 3D creature (fn_004813B0, bone index LH3DCreature +0x51B8), unchanged without a
  3D creature.

- The world position of a target: its MapCoords +0x14 as a point (fild x, z x 10 / 65536, GetAltitude 0x803090 + the height above the land +0x1C; 0x67E9E1..0x67EA15, 0x67EB8F..0x67EBCF)
- GetCurrentParentPos 0x674AF0 (the parent atom's +0x80, or the origin), then y = the land's altitude there
- UR_HealInHand::ModifyAtomCollection 0x6A0F40 (DefineProperties 0x6AEB80, WiggleFreq 1 by default): each atom goes to the parent's position (GetCurrentParentPos 0x674AF0: the parent atom's +0x80, or the effect's origin) times sin(age x WiggleFreq x 2 pi), the odd ones with the opposite sign
- The array property's setter (0x6AE170 and its siblings): the file's flat (t, value) pairs (an odd last value is dropped: count / 2 * 2), then fn_005B3760, the cubic spline's second derivatives (Numerical Recipes' spline(): first slope yp1 and last slope ypn). Every rule's ctor sets the array's flag (+8 = 1), which passes yp1 = ypn = 0 (zero slopes at the ends); without it 1e30 (0x7149F2CA) would make a natural spline.
- fn_00691E00: an object's tip is its MapCoords as a point (x, z = fild x 10 / 65536 0x691E30..0x691E43, y = GetAltitude 0x803090 + its altitude +0x1C 0x691E22..0x691E2D) raised by GetHeight (vt +0x42C, 0x691E48); a ground point is itself
- 0x6906A1..0x6906AF: GetAltitude 0x803090 at the MapCoords fn_004427B0(x, z) = ftol(x x 65536 x 0.1), which is ToFixed (65536 x 0.1f is 6553.6f exactly), + 2 [0x8AB478]; x and z stay the float point
- GetCurrentParentPos 0x674AF0: the parent atom's +0x80 (its position in the collection's frame), or the origin
- GetCurrentParentPos 0x674AF0: the parent's +0x80, or the origin
- AtomCollection::GetCurrentParentPos 0x674AF0: the parent atom's position, the manager's origin without one
- 0x6D452C..0x6D4601 measure the slope between here and the next step and scale a copy of the horizontal velocity by 1 - 0.9 x (angle / (pi / 2)): that copy is never read (the position below uses the velocity just stored), so the slope has no effect in the exe 0x6D466C: p += v x dt (fn_0044E9F0, fn_004605F0)
- PSysFloatRand(0.7, 1.0) + 0.7 (0x8AB238), times the Radius provider: 1.4..1.7 x R
- 0x8C7A18
- 0x8AB238, 0x8C5844

### The glints on a target

Sparkles laid on the vertices of a target object's mesh. MAC 0x6A53A0, the glint update fn_006A5520. Used by
`SF_ScriptHighlightGlintsGold`, `Silver` and `Bronze` (particle types 99, 100 and 101) and by
`SF_CreatureSpellFreezeOnHolder` (type 92); of the 132 spell files only these four contain the class.

- Flags 7: a creator. Properties: +0x28 PCreator, +0x2C MaxAtoms (default 20), +0x30 MaxAlpha (40), +0x34 GlintGroup
  (−1), +0x38 GlintCreator, +0x3C PulseMagnitude (1), +0x40 PulseSpeed (1), +0x44 AtomAgeMaxSize (0.25), +0x48
  AtomAgeZeroSize (2); also NextGroups (+0x20).
- The four files give MaxAtoms 3, MaxAlpha 80, GlintGroup 1, AtomAgeMaxSize 0.15, AtomAgeZeroSize 0.9, Pulse 1 / 1, a
  16-frame sprite with RandomiseInitFrame 1 and RandomiseFrameDirection 1, InitialScale 1.5 and DeleteOnCloseDown 1.
- MAC 0x6A53A0:
  - no PCreator: returns false;
  - if the effect has targets, `TakeTargetObject` (one per step); a non-null object gets a parent atom:
    `AtomCore::Create` (PSysRand(0x100)), `PCreator->CreateParticle`, `InitNewAtom(atom, GlintGroup)` (the parent's
    sub-collection is GlintGroup), parent data {first = 1, due = 0, made = 0, turn, object};
  - then every parent from the head (newest first): its data is made if missing; an object that is non-null and
    `IsAvailable() == 1` stamps turn = now; otherwise object = 0 and the parent is deleted (`DeleteFromAtomList`; the
    atom's destructor unlinks its sub-collections first, so +0x1C reads 0 afterwards and the deleted parent's glints
    are not processed). If the parent's first sub-collection exists, fn_006A5520(parent data, first sub-collection);
  - returns true.
- fn_006A5520:
  - no GlintCreator: returns. The first flag is cleared. rate = float(MaxAtoms) / AtomAgeZeroSize; rate ≤ 0 or NaN:
    returns;
  - due += dt × rate, **before** the vertex count. n = `object->ForDrawFXGetNumVertices()`. If n > 0 and made < due,
    loop: stop if the collection's atom count ≥ MaxAtoms; made++; `Create` (Rand 0x100); fn_00674DD0(atom,
    NextGroups); `GlintCreator->CreateParticle` (the sprite draws RandomiseInitFrame Rand(16) and
    RandomiseFrameDirection Rand(0x100)); base scale +0x74 ×= `object->GetScale()`; vertex = PSysRand(n); while
    made < due;
  - then every glint from the head: its data (the vertex index) is made if missing; if
    `ForDrawFXGetVertexPos(index, &p)` succeeds, position +0x80 = p (world: no GlobalToLocal, and every glint file
    has Hierarchies 0);
  - pulse = cos(age × PulseSpeed × 2π) × PulseMagnitude × 0.5 + 1, set to 0 when < 0 or NaN;
  - size = age < Max ? age / Max : 1 − (age − Max) / (Zero − Max), set to 0 when ≤ 0 or NaN and to 1 when ≥ 1;
    rule scale +0x78 = size × pulse; the alpha byte (the ARGB top byte, +0x8C) = (uint8)MaxAlpha;
  - age > Zero (not NaN): the glint is deleted.
- The vertex providers: ScriptHighlight 0x70AA70 / 0x70AAC0 (its 3D object +0x40) and SpellSeedGraphic 0x7277B0 /
  0x727800 (its 3D object +0x2C). The count is the sum over the submeshes [mesh+0xC] of the sum over their primitives
  [part+4] of their vertex counts (+0x10). An index greater than the total returns false (the `jg` at 0x727867);
  otherwise it is the index-th vertex in file order (32-byte vertices at +0x14), turned into a world point with the 3D
  object's matrix (rows m0, m1, m2 and the translation t at obj+0x14; ScriptHighlight 0x70AB7F..0x70ABF8), in float
  and in this order: x = ((vz × m2.x + vy × m1.x) + vx × m0.x) + t.x, and the same for y and z. Any other object has 0
  vertices and returns false.
- GetScale: ScriptHighlight is `Object::GetScale` (+0x50); SpellSeedGraphic 0x727340 returns +0x54; any other object 1.
- Which matrix the glints read: ScriptHighlight::Draw draws its 3D object (SingleMapFixed::Draw, 0x709FCA) before the
  glint step, so its glints read this frame's matrix. A seed graphic steps its holder effect before DrawSpellGraphic,
  so its glints read the previous draw's matrix ([SpellSeedGraphic: the seed that floats in the orb and in the icons](../magic/worship.md#spellseedgraphic-the-seed-that-floats-in-the-orb-and-in-the-icons)).
- Who steps it. ScriptHighlight::Draw, at 0x70A126..0x70A1C1: if the glints +0x6C exist, `Process_` (vt +0xFC) with
  info {three zero points, power 1.0, +0x80 = 0, enabled 1} and `g_game_time_inc`, then `Draw_(1.0, true)`
  (vt +0x104). Then, at 0x70A1C7..0x70A262, its **active** effect (+0x70) gets a vt +0x124 call and the same `Process_`
  and `Draw_`, also per drawn frame. The Draw is reached only from the object draw list
  ([The script highlight in the list](../rendering/object-draw-list.md#the-script-highlight-in-the-list)), so the glints step only on the frames the
  list draws the highlight.
- `GJPSysInterface::Process_` 0x6736B0: if `PSysManager::IsInState(1)` and the file's +0x1C flag, it returns 5
  without stepping; otherwise fn_00673300. fn_00673300: if +0xAD, fn_00673340 once more and +0xAD cleared; then
  fn_00673340. +0xAD is 1 for every effect at its init (fn_00672B50 0x672BF7), so an effect's **first** `Process_`
  steps twice. fn_00673340: dt [0xD4E0EC] = ms × 0.001 (0x673402), 1/dt into 0xD4E0F0.
- The data: only `SF_CreatureSpellFreezeOnHolder` reaches a seed graphic, as the holder of CREATURE_SPELL_FREEZE
  (seed 12, useMesh 1); the seeds without a mesh have no glint holder, so with the game's data a seed graphic without
  a mesh never gets glints (read in the main game's `Scripts\info.dat`; Creature Isle's was not checked).
- Where ours still differs (pending):
  - The first Draw of a highlight lit at a land's start comes on a frame whose game ms is 100, and the first step is doubled, so its active effect starts 0.2 s old and
    stays 0.1 s ahead of the turn count (measured on `particles.highlight_lit`: age 0.200 at the first Draw, 3.400 at
    about frame 200 against 3.3 s of turns). What g_game_time_inc is on the original's first frame after a land starts
    is not known. Stepped per frame or per turn, the effect itself ages at the same rate: at 0.016 s steps the conical
    emitter (one atom per step) emits at its own rate (EmissionFreq 9 with Randomise, about 12 a second), where 0.1 s
    steps held it to 10 a second.
  - A seed graphic without a `Mesh` (useMesh 0) has no points in ours; the original walks `info.mesh` through the frozen
    creation matrix. Unreachable with the game's data (no glint file holds those seeds).
  - The creation matrix takes the graphic's mesh position as openblack keeps it; the original builds its point from
    the map coordinates (x and z × 1/6553.6, the land's altitude + the coordinates' altitude), so x, z and y may differ
    by the rounding of that path.
  - The seed graphic's holder (and so its glints) is stepped every frame by the orb and the icons, not from the object
    draw list, so it also steps on frames where the original's list does not draw the orb or the icon.
  - The seed graphic's holder steps once at its first step; the original's first `Process_` steps twice (see Pending).
  - The pulse's cosine is `std::cos` on a float; the original's `fcos` keeps extended precision before the next
    multiply, which can differ in the last bit.

- ZR_ChainGesture (ModifyAtomCollection 0x68A080, emission fn_0068A330, DefineProperties 0x6B0170): while the effect is enabled (PSysManager::IsInState(2), PSysProcessInfo +0x38) one head atom at the gesture position trails a chain: its NextGroups sub-collection's joints are a shift register, a new joint at the head every MinEmitDist of movement (at most count / DieAge joints per second). When the effect stops being enabled the head stops emitting and goes 5 s later.
- UR_FollowLocalHand::ModifyAtomCore 0x69A6A0 (DefineProperties 0x6B1B90): the atom goes to the gesture position (PSysManager::GetCurrentGesturePosn 0x673600 = PSysProcessInfo +0x0C) and its velocity is the move x [0xD4E0F0] (1 / the step). UseGraspPos (+0x20) is read, not used here.
- The max(dt, eps) is a port guard: the original multiplies by [0xD4E0F0] = 1/dt directly
- [0xD4E0F0]

### The object rules

`CreateRule_GameObjectRef` keeps an unseen atom on each object an effect is given; `ER_EmitFromParentAtom` lets atoms
out from random points of what its collection's parent atom stands for. Files: `SF_HighlightOnObject` (34, DoShower 1,
Alpha 100), `SF_ButterfliesOnObject` (38), `SF_FliesOnObject` (40, OffsetY 1), `SF_SparklesFromObject` (46
PILEFOOD_SPEEDUP, the only object file with the emitter), `SF_SparklesBurstFromObject` (no table entry; its
`ER_BurstFromParentAtom` 0x6A58B0 is not ported) and `SF_CreatureSpell` (the emitter under `UR_CreatureSpell`). Their
target is the spot visual's owner (see the container above).

- `CreateRule_GameObjectRef`: ctor 0x6BF6B0 (flags `|6 ^4`: keeps the effect alive while it is not closing, not a
  creator; +0x44 DoShower 0, +0x48 Alpha 0xFF, +0x4C OffsetY 0), DefineProperties 0x6B1930 (PCreator is not read),
  MAC 0x69DED0:
  - when the effect has object targets (`GetNumObjects`), one is taken (`TakeTargetObject`, the last added) and cast to
    `Object` (`__RTDynamicCast`, 0x69DEF4..0x69DF0A); a target that is not one is dropped. The exe's RTTI (the class
    hierarchy of each primary vtable) gives 135 classes with `Object` as a base: every Fixed (MultiMapFixed,
    SingleMapFixed: abodes and fields, FishFarm, features, BigForest, trees, MapShield, ScriptHighlight, the spell
    icons, TotemStatue, the citadel parts with CitadelHeart and WorshipSite, PrayerSite, the MobileStatic rocks,
    bonfires, dead trees, fragments, MagicTeleport and LandscapeVortex), every Mobile (Living: villagers, animals,
    creatures; MobileObject: Pot and the piles, Whale, Scaffold, OneOffSpellSeed, Ball, Arrow, Reward...) and some
    direct ones (CitadelEntrance, FireFly, Furniture, GStreetLantern, HelpSpirit, MagicFireBall, ShowNeedsVisuals,
    SpellSeed, TownDesireFlags...). 52 GameThingWithPos classes are not Objects: GInterface (the hand), GCamera, the
    Containers (Town, Citadel, Flock, Forest), Mist, GParticleContainer, the spells, SpellSeedGraphic, GStreetLight,
    ScriptMarker, ScriptTimer, the planned buildings, InfluenceRing, WeatherThing... For an Object: `Create`
    (`Rand(0x100)`), the alpha byte = the low byte of Alpha (0x69DF22..0x69DF45), the render particle
    `RenderParticleGoldenShower` with DoShower (vtable 0x8FAD68) else `RenderParticleGameObjectRef` (0x939574, ctor
    fn_006C8150), then fn_00674DD0 with NextGroups (no position of its own);
  - every atom from the head whose render particle has an object (vt+0x11C `GetGameObject`): x, z = its MapCoords x
    10/65536, y = (`GetAltitude(MapCoords)` + MapCoords altitude (+0x1C)) + OffsetY (0x69DFE9..0x69E035). An atom whose
    object has gone stays where it was.
  - `RenderParticleGameObjectRef::GameUpdate` 0x6C8500 (render particle vt+0x108), called from the atom's post-update
    fn_00673EA0 (0x674073..0x674080) after its sub-collections have been updated: an object that is not `IsAvailable`
    is let go. So the step it is found unavailable it is still placed from, and its emitters still read it.
  - `GetRandomSurfacePos` 0x6C8230 (vt+0x110, through `AtomCore::GetRandomSurfacePos` 0x673E60 with the atom's world
    matrix fn_00673E40): no object, the matrix's translation; no 3D object (obj+0x40), the point is left as it was;
    an animated 3D object (`LH3DObject::IsAnimated`, vt+0x1AC at 0x6C8294; see below), with both `GetMesh` (vt+0xF8)
    and `GetCurrentAnim` (vt+0x184) set (else nothing and no draw, 0x6C82B4..0x6C82BE), `PSysRand([GetMesh+4] &
    0xFF)` picks a row of the
    table [0xC37D9C] filled by `GetRandomSurfacePosInit` 0x6C81C0 (fn_00839980) and gives its translation; a static
    one, `PSysRand(parts)` ([mesh+0xC], the file's sub-meshes), `PSysRand(items)` ([part+4], its primitives),
    `PSysRand(triangles)` ([item+0x18]), the corners from the file's 32-byte vertices ([item+0x14], uint16 indices at
    [item+0x1C]), then a = `PSysFloatRand(1)`, b = `PSysFloatRand(1)`, both turned to 1 minus themselves when
    a + b > 1; p = ((c1 - c0) a + c0) + (c2 - c0) b; then through the game object's `GetWorldMatrix` (vt+0x63C):
    ((row2 z + row1 y) + row0 x) + translation, per axis (0x6C8490..0x6C84EC).
  - `GetWorldMatrix` (vt+0x63C, read in every vtable): 101 classes use `Object::GetWorldMatrix` 0x638200, 21
    `MobileObject::GetWorldMatrix` 0x607560 (every class under MobileObject), 11 `MobileStatic::GetWorldMatrix`
    0x608DE0 (every class under MobileStatic) and `CitadelHeart` its own, 0x467870. All of them put the translation at
    (x x 10/65536, `GetAltitude(MapCoords)` + MapCoords altitude, z x 10/65536).
    - Object 0x638200: S = `GetScale` (vt+0x120), a = `GetYAngle` (vt+0x508). a == 0 (or not a number, `test ah,0x40`
      on the fcomp): S down the diagonal, or `SetIdentity` when S == 1 (or not a number). Otherwise rows (0 + c S, 0,
      0 + s S), (0, S, 0), (0 - s S, 0, c S - 0), with fcos / fsin unrounded and each product rounded once; with S == 1
      the sine is stored as a float and the cosine rounded by the sums with 0 (0x63827F..0x6383BB).
    - MobileObject 0x607560 and MobileStatic 0x608DE0 (the same code): `SetYXZMatrixOnly(GetYAngle, GetXAngle,
      GetZAngle)` 0x7FAC10, then, unless S == 1 (or not a number), each of the 9 cells x S.
    - CitadelHeart 0x467870: a copy of its 3D object's matrix (obj+0x40, +0x14, 12 dwords).
  - Which 3D objects are animated: of the types `LH3DObject::Create` 0x80B4D0 makes (switch 0x80B844), only type 2
    ANIMATED (vtable 0x9A32A0) answers `IsAnimated` with 1 (0x80B900); STATIC, MORPHABLE, COMPLEX (3, whose
    `IsComplex` is 1), the sprites, MIST and CITADEL answer 0, and no other LH3DObject-shaped vtable answers 1. An
    object's type is `Object::Get3DType` 0x6364F0 through `Object::Create3DObject` 0x6365F0: `(flags >> 7) & 2` of its
    mesh, so ANIMATED exactly when the L3D header has the bones flag (0x100), else STATIC. The overrides never give 2:
    MORPHABLE for BigForest, CitadelPart (CreaturePen, WorshipSite), Creche, Flowers, Football, StoragePit, TownCentre,
    Graveyard, PileFood (MagicFood, PuzzleGrain), PhysicalShield and Workshop; CITADEL for CitadelHeart; 0 or 1 for
    Wonder (the table at 0x7791B0); STATIC made explicitly for Creed, SpellIcon (and its subclasses) and
    ArenaSpellIcon. Made ANIMATED explicitly: the Whale (0x774CC8) and the Reward's chest (0x6E5744). A creature's 3D
    object is replaced by its LH3DCreature's +0x482C (`Creature::Create` 0x474B24..0x474B42, also in `ResolveLoad`
    0x4E6D08 and `Draw` 0x517AE5), which `Morphable::MorphInit` makes as type 3 COMPLEX (0x617315, its only writer):
    a creature takes the static branch, over the body mesh of the level of detail it was last drawn at
    (`LH3DCreature::PrepareForDrawing`, `[c3d + 0xB4 + 4 x [c3d + 0x98]]`, 0x4ED483..0x4ED49C; nothing before its
    first draw, +0x98 = -1), through `Object::GetWorldMatrix`.
  - `RenderParticleGoldenShower::DrawAt` 0x67B0F0 makes a shower object (fn_006CA7A0) and draws it at the object's 3D
    position with the atom's alpha; `RenderParticleGameObjectRef::DrawAt` draws nothing (0x6C81B0).
- `ER_EmitFromParentAtom`: ctor 0x6A5B00 (flags 7, a creator; MaxAtoms 20, MaxAlpha 40, PulseMagnitude 1,
  PulseSpeed 1, AtomAgeMaxSize 0.25, AtomAgeZeroSize 2, DoScaling 1, DeleteAtoms 1, EmitOnlyAboveLandscape 0),
  DefineProperties 0x6AFB80, CollectionData 0x560040 (+0x24 owed, +0x28 emitted), MAC 0x6A5BA0:
  - no PCreator or no parent atom, false (detached); rate = MaxAtoms / AtomAgeZeroSize, not above 0, false;
  - with EmitConditionOfParent, it is tested on the parent; owed += dt x rate; while owed > emitted: stop at MaxAtoms
    alive; emitted++; the parent's `GetRandomSurfacePos` into the frame's one point (esp+0x14, 0x6A5C99); with
    EmitOnlyAboveLandscape, the atom is not made when `GetAltitude(ftol((x x 65536) x 0.1), ftol((z x 65536) x 0.1))`
    >= y (0x6A5CB1..0x6A5CFD: the constants 0x8AC408 and 0x8AC404 are floats, so each product is a float one; the fcomp
    with `test ah,1` makes the atom for a y that is not a number); `Create`, fn_00674DD0, `CreateParticle`, then
    `GlobalToLocal` turns the point into the atom's local position in place (0x6A5D21) and it is copied to the atom's
    +0x80; again while emitted < owed. So a later pick of the same step that writes nothing starts from the previous
    atom's local position, and is tested against the land and converted again;
  - every atom: with DoScaling, +0x78 = clamp01(ramp) x max(cos(age x PulseSpeed x 2 pi) x PulseMagnitude x 0.5 + 1,
    0), ramp = age / AgeMaxSize below it, else 1 - (age - AgeMaxSize) / (AgeZeroSize - AgeMaxSize); a ramp or a pulse
    that is not a number gives 0; with DeleteAtoms, deleted once age > AgeZeroSize. MaxAlpha is not read.
  - The other parents: `RenderParticle::GetRandomSurfacePos` 0x6C79D0 gives the atom's place;
    `Particle3DObj` (mesh creators) 0x6C7C10 and `RenderParticleCreatureRef` 0x6C7F30 have their own.

- 0x67E79E..0x67E88E: five times LH3DObject::Create(0) with MeshPack 0x237 (vt 0xF4 SetMesh), SetPosition (vt 0x20 = 0x423140) at centre + (rand(-4, 4), 0, rand(-4, 4)) (the z rand first, 0x67E7E3 / 0x67E7F6), turned PSysFloatRand(2 pi) (0x67E847) about Y and scaled PSysFloatRand(0.8, 1.2) (0x67E837, drawn before the angle), fn_006812B0(it, its matrix +0x14, centre - 5 m (fn_0067E8C0), BlastSpeed, 6.0, 0) and the object deleted (vt 4): only its pieces exist. The queue is emptied by UR_ExplodeObject (Rules/ExplodeObject.cpp) in this turn's PSysGlobal::GameLoopEnd
- fn_00686980 when the surface is made (0x6867F9), with the matrix fn_00673E40 gives at 0x6867E8: the atom's local frame (fn_00673DB0: its rotation x baseScale x ruleScale, Y also x the stretch, at +0x80) in the collection's hierarchy (fn_00673D20 -> fn_006752D0). The mesh goes to the world (fn_006C9BD0), is cut along the land's cells and raised there (land_morph::RaiseAboveLandscape) and comes back with the inverse (SetInverse 0x7FB290), so the draw moves the raised mesh with the atom (and scales it with a later UR_ChangeScale, as the original's local deltas do). (aproximado) The frame is the one Collect draws with (rotation x baseScale x ruleScale, no stretch): the original's drawn PSR (fn_00673EA0 -> fn_00673DB0 at 0x673ED6) is taken as the same matrix as M, and Collect leaves the stretch out. Both users have stretch 1 (SF_TeleportVortex, SF_SpellDispenserVortex: no StretchVertically, no rule writes it).
- The size: the unit-radius mesh goes through the atom's drawn matrix only (RenderParticleGJMesh::DrawAt 0x67C150 multiplies each vertex by DrawData+4, the PSR at atom+0xD0). PostUpdateAtoms fn_00673EA0 already puts the hierarchy into it (cur = parent PSR x local fn_00673DB0, scale = +0x74 x +0x78 x the parent's), which Effect::PostUpdate does into atom.current.scale. So the radius is this atom's 1 (AtomCore ctor) x Scale x the group-2 parent's InitialScale x UR_ChangeScale: 6 m for SF_SpellDispenserVortex (6 x 1), 10 m for SF_TeleportVortex (2 x 5); the parent's scale must not be applied a second time. !DoRaiseAboveLandscape: the twists breathe with sin(fmod(collection age x +0x68, 2 pi)); +0x68 has no property (1.0 from the ctor 0x686200)

### The storm in the hand's flashes

Read in the exe (`ModifyAtomCollection` 0x6A6910, its emission test fn_006A6860, `DefineProperties` 0x6AF9C0, factory
0x6BDF90, `CollectionData` ctor 0x55FDC0, `Load` 0x699AC0) and run in the oracle.

- Used by group 1 of `SF_LightningStormInHand` (EmissionFreq 0.8), `..PU1` (1.8), `..PU2` (2.8) and
  `SF_LightningStormOnHolder` (0.8), each with MaxAtoms 4, Randomise 1 and SpecLife left at 0.5: the sprites
  (`ParticleSpriteCreator_Lightning`, S_SpriteSheet3 from 40, 8 frames) of a collection under the cloud's mist atom
  (group 2). All four files (the base, PU1, PU2 and the holder's) give it the `Condition` "not closing"
  (EventConditionTrueOnCloseDown1), tested by the collection step
  before the rule, as for every rule.
- Properties: +0x20 `NextGroups`, +0x28 `PCreator`, +0x2C `SpecLife`, +0x30 `EmissionFreq`, +0x34 `MaxAtoms`, +0x38
  `MaxTotalAtomsToEmit`, +0x3C `Randomise`; defaults 0.5, 0.001, −1, −1, 1 (0x6BDFB7..0x6BDFED). Modifier flags |= 6
  (0x6BDFC5): a creator.
- The step: with no `PCreator` it returns 0 (detached), before anything else. Its collection data (0x34 bytes, found
  by modifier in the list at collection +0x24, else made: +0x20 the flash's start, +0x24 striking 0, +0x28 the due
  time 0, +0x2C the count 0, +0x30 first 1).
- The emission test fn_006A6860 (one sprite a step at most):
  - the first time, the due time = the collection's age (`GetAge` 0x674ED0) and first = 0;
  - none when `MaxTotalAtomsToEmit` ≠ −1 and the count ≥ it (signed);
  - none unless age + dt ([0xD4E0EC]) > the due time (`test ah,0x41`: equal or NaN is not yet);
  - none when `MaxAtoms` ≥ 0 and the collection's atom count (+0x44) > `MaxAtoms`;
  - else w = 1 / `EmissionFreq` (a float), times `PSysFloatRand(0.75)` + 0.25 with `Randomise` (the only draw, on the
    effect's stream, 0x6A68CC..0x6A68E1); the due time += w and the count + 1.
- A sprite: striking = 1 and the flash's start = the age (0x6A6990..0x6A6999), then `AtomCore::Create`,
  `atom+0x128 = PCreator->vt+0x10(atom)` and fn_00674DD0 with `NextGroups`, as the other create rules.
- The flash (0x6A69BE..0x6A6A4E), while striking: t = age − start; when t > `SpecLife` (`test ah,0x41`), striking = 0
  and the specular is 0xFF000000; else v = ftol((1 − t / `SpecLife`) × 255) & 0xFF (a division, not the reciprocal
  of `UR_CloudGather`) and the specular is ARGB ((255 v) >> 8, (200 v) >> 8, (200 v) >> 8, (255 v) >> 8). It is
  written to the parent atom's +0x90 (collection +0x3C; nothing without a parent). So the strike's own step gives
  0xFEC7C7FE, and t = `SpecLife` exactly gives 0 (alpha 0 too) and ends on the next step. With no flash on, the
  parent keeps its specular. The parent is the cloud's mist atom, whose specular is drawn as the storm cloud's glow
  ([The cores and the clouds](../magic/storm-and-tornado.md#the-cores-and-the-clouds)):
  `RenderParticleMist` passes it as the vertices' specular.

### Exploding objects

- The LH3DMesh of MeshPack[index] (LH3DMesh::MeshPack 0xE9FE34: AllMeshes.g3d), read once; nullptr if it is not there
- A piece atom (its RenderParticleGJMesh +0x128) and its GJMesh (ctor 0x67FF20, filled by fn_0057D630): three vertices per triangle (+8 positions about the centroid, 0x680E5A..0x680E89; +0x44 uvs; +0x80 normals, the source's as they are, not turned by the matrix), the triangles b, b + 1, b + 2 (+0x6C), no colours of its own (+0x24 != the vertex count) and the source primitive as its material (GJMesh +0, 0x680C49). Drawn as the original does (RenderParticleGJMesh::DrawAt 0x67C150 -> Draw3DWorldTriangle 0x81C090): one CPU-made triangle list per frame (gj_mesh::Build, graphics::world_triangles), Creator::Kind::GJMesh, no bgfx buffer per piece and no handle limit
- RenderParticleGJMesh::DrawAt 0x67C150 0x67C175..0x67C1F6 (its +0x21, set by ExplodeMesh 0x680B9B): the DrawData colour (0xAARRGGBB) times the land light under the drawn position (fn_00801C90), byte by byte, alpha included
- DrawAt 0x67C150 of one piece with the drawn matrix `model` and the DrawData colour `argb`: - the base colour: argb x the land light under the matrix's translation (+0x21, 0x67C175..0x67C1F6, LitColour); - every vertex through the matrix into the world (0x67C279..0x67C30A); +0x22 (the vertices stuck to the land, 0x67C310..0x67C38C) stays 0 for the pieces (ctor fn_006C8A90, 0x6C8AA5); - every vertex of the base colour (+0x24 != the count, 0x67C47F..0x67C4C4), then the model light ([0xC029C0] = 1 and one normal per vertex, 0x67C4CE..0x67C6B2): the light [0xEA9E90] through SetInverse of the matrix (0x67C508..0x67C51E), I = fistp(255 n.l) (0x67C61D..0x67C62C), the ambient [0xC39264] (model_light::Apply). The second light [0xD4EC08] (0x67C6C3) is 0; - the alpha table 0xC387C8 when the DrawData alpha byte is not 0xFF (0x67C9B7..0x67C9C0), back to 0xC38728 after the draw (0x67C9F7). Appends one batch (or joins the last one) with `tag`

### Explosion rules

- 0x67EA77: a target inside a shield by 2 m is shielded
- [0x9357C8]: a map cell
- [0x9357E4]: the smoke's magnitude
- [0x9357E8]: ftol((1000 div [0xD01A38]) x 4) & 0xFFFF turns
- 0x67EC6F / 0x67E868: fn_00681260's / fn_006812B0's 6.0f (stored, unread)
- +0x38 = 0x41200000
- +0x40 = 0x40A00000
- +0x44 = 0x40600000
- +0x48 = 0x40400000
- 0x67EE90..0x67EEA6: CreateSpotVisualWithSpecifiedDuration(MapCoords(centre) 0x603160, BEAM_EXPLOSION_FX, 1.0, 60 turns, NULL): the column and cones. The effect starts at that MapCoords as a point (fn_0063E410 0x63E418..0x63E47B: fild x, z x 10 / 65536, GetAltitude + the altitude): ToWorld of FromWorld
- 0x67EEF8..0x67EF2C: an inline copy of NumGameTicksPerSecond 0x711630 (`div [0xD01A38]` of 1000, fild qword, fmul 4 [0x9357E8], __ftol), the same as TicksForSeconds(4), then `and eax, 0xFFFF`: TURNS to CreateSpotVisualWithSpecifiedDuration (0x67EF3B), at MapCoords(centre) 0x603160 (0x67EF35)
- fn_00681F10(0)
- 0x67E35C..0x67E395: fn_008251C0(centre, PSysFloatRand(2 pi) (0x40C90FDB), [0x9357D4] = 8, mesh 0x251), a ground mark that melts into the land and fades after 15 s (ecs/GroundMarks.h; its SmokyStuff is not made)
- The targets: every available object of the ceil((r + 20) / 10)^2 cells of the spiral around the centre that is counted in that cell (fn_00604F40: its own cell) and nearer than its Get2DRadius (vt +0x64, 0x67E6CD) + r (GUtils::GetDistanceInMetres 0x74CD70 from the centre's MapCoords, 0x67E6E1); r = MaxDistance x the tribal power (1..5)
- g_game +0x205A40 (0x67E702)
- The centre's MapCoords (ToFixed of x and z, 0x67E56A..0x67E598) and its copy walked by GUtils::Spiral 0x74D7E0 from dir = count = 1 (0x67E627..0x67E634), MapCoords::InBounds 0x6042C0 on each cell, += JustMapXZ 0x605470
- 0x67E65C..0x67E678: [eax+4] (the fixed list) first, then the mobile one (fn_006827E0), each from its head (ecs::map_cells)
- fn_00604F40 (0x67E6AA): only in the cell of its own MapCoords
- 0x40C90FDB
- g_game +0x205A40 (0x67E95F, 0x67EB39, 0x67EB76)
- Reached by the ring: |p - centre|^2 <= (GetRadius (vt 0x60) + spread)^2, in 3D: (dz dz + dy dy) + dx dx (0x67EA1C..0x67EA41) against (R + spread) x (R + spread) (0x67EA4B..0x67EA60)
- SetPSysCloseDown::ModifyAtomCore 0x6A26D0 (DefineProperties 0x6ACC60: the base ones only): PSysManager::SetState(1) 0x672FF0, the effect closes, for every atom that passes the condition
- Abode::DestroyedByBeam 0x402CB0: ReduceLife(GetLife(0)) (vt 0x5B8 / 0x11C). Abode::ReduceLife 0x405D90 -> MultiMapFixed::ReduceLife 0x52F5E0 (built: Object::ReduceLife, life 0); below 1 every inhabitant SetStateWhenTappedOnAbode 0x752B80, StopBeingFunctional (vt 0x918) / the town's emergency, and with a town and no site Town::AddBuildingSite 0x73B8E0 (site +0x640 = 1.1 x life - 0.1). With the site IsDrawBuilding 0x52F0C0 (+0x74 != 0) holds and, with no DestructionMesh, GetPercentForDrawBuilding 0x52EFD0 = min(the built percentage, GetPercentRepairedFromWhenDamaged 0x52F010 = 0.98 x life (0x52F0A6, [0x8CF3FC])) = 0: Abode::Draw 0x515F70 -> MultiMapFixed::Draw 0x518090 draws nothing, the building is gone from view until it is repaired. 0x402CB4..0x402CC4: ReduceLife(GetLife(), NULL) through ecs::abodes::ReduceLife (a field's vt +0x5B8 0x52A0A0 changes nothing). ⚠️ approximate TotemStatue::ReduceLife 0x737C90 and a dispenser without an Abode: Object's

### Fireball rules

- The atom, or one of its parents, has been deflected (EC_DeflectionInAtomsHierarchy 0x67DAF0)
- EventConditionAtomHasBeenDeflected 0x67DB70: the atom's flag bit 3
- EC_DeflectionInAtomsHierarchy 0x67DAF0 (an atom) / EC_DeflectionInCollectionsHierarchy 0x67DB20 (a collection: its parent atom and theirs)
- EventConditionAtomCloseWater 0x67DCA0 (CutOffHeight +0x10): not deflected, lower than CutOffHeight above the land and over water (or outside the land)
- EventConditionFireBallSteam 0x67DDD0: not deflected and raining or snowing there (GClimate::GetMaxRainingOrSnowing)
- UpdateRuleGravityWithFloor::ModifyAtomCollection 0x6A1880 (UpdateRuleGravity 0x6AC1B0: +0x20 MaxSpeed, +0x24 Gravity, +0x28 Damping, +0x2C WindMagnification, +0x30 UseDamping, +0x31 UseWind, +0x32 / +0x33 Disable{Wind,Damping}ForNonHuman; its own +0x34 DampingHorozontalBounce, +0x38 DampingVerticalBounce, +0x3C GroundDrag, +0x44 ImpactSound, +0x5C its condition, +0x60..+0x68 ImpactSpeed{Small,Medium,Large}, +0x6C MinAlphaForImpactSoundOrRipple, +0x70 UseSurfaceForBounce, +0x72 CheckShieldDeflections). The spell files that use it: SF_ExplodeObject (fragments, NO_SOUND: no impact, no ripple) and SF_FireBallThrow / PU / PU2 (the thrown fireball).
- 0x6A1D3F (0x8BF518 = 1e-4); the fallback (1e-4, 0, 0) at 0x6A1D4C
- AtomCore::GlobalToLocal 0x673D40 into atom +0x80
- fn_0069E6B0: the hand's direction pitched up 0.3 rad (clamped to +-pi/2)
- fn_0069E620: the hand's speed through (0 -> 0, 50 -> 50, 450 -> 200), 200 above 450
- CreateWithInitialDirection::ModifyAtomCollection 0x69E950 (a OnceOnlyCreateRule; DefineProperties 0x6B1230) 🔍 inferred the property defaults below: the ctor was not read
- A script, computer or creature cast: a ballistic arc from the gesture position to 0.8 of the way to the origin, in max(0.025 D, 0.5) s, or at 30 degrees (0x9375F0: 0.5237 rad) when that would be steeper (cwid.txt; the re-solve 0x69EC3E..0x69ECC1 was re-read in the fuego2 audit: delta [esp+0x64] (0x69EB1B), distance [esp+0x38] (0x69EB3F), time max(0.025 D, 0.5) (0x69EB43..0x69EB5A), v = delta / t - (0, -g [esp+0x20] x 0.5 t, 0) (0x69EB62..0x69EC39, re-read by the fuego2 audit); the target at 0.8 and the origin fallback before 0x69EAF0 stay UNVERIFIED, destructive.md §3.4). g = the group's UpdateRuleGravity +0x24, 30 without one (0x69E9A0)
- 0x69EA5C / 0x69EA6E / 0x69EA81 (0x8AB22C = 0.1)
- The re-solve for any launch that moves: |v|^2 > 0.01 (fcomp qword [0x8C7620], the double 0.01, at 0x69EC60; read as a float it looked like 89129), then steeper than 30 degrees: fld qword [0x9375F0] (the double 0.52370351552963257) fptan 0x69EC77, compared in the FPU with v.y / sqrt(horizontal2) (0x69EC7B..0x69EC85) horizontal2 is stored as a float (fstp [esp+0x24], 0x69EC50), then + y x y in the FPU (0x69EC54..0x69EC5C)
- 0x69EC92..0x69ECAA: (delta.y - distance x tan) x (-2 [0x8C7CE0] / g), all in the FPU
- Fcom 0 [0x8AA398] at 0x69ECAC: 0.1 (0x8AB22C, 0x69ECBB), then fsqrt 0x69ECC1
- The sound's size from the speed / 200 (fn_0069E610), > 0.6 (0x8CF7D8) large, > 0.3 (0x9375E8) medium
- Not ported: for the local interface's cast the atom gets a DrawOffset (fn_006C7840) that draws it from the hand to the start (destructive.md §3.4), so here the ball appears at the gesture point
- The whoosh (0x683184..0x68327F): the drawn position (+0xF4) within 40 m of LH3DTech::g_camera now (squared < 1600 [0x9361B8]) and not the step before (+0xBC: squared > 1600, 0x68322B), and faster than 20 (+0x34 squared > 400 [0x8C7728]): GAudio::PlaySoundEffect 0x429D60(NULL, 0x40 G_FireballPast_01 + GetTickCount() % 5, mode 2, loops 0, +0x10 0, is3D 0, IN_GAME), a 2D one
- EventAlways::ModifyAtomCore 0x69DBB0: every step a type-2 event at the atom (its global position, the last global movement, strength 1, no shield check)
- GetLastGlobalMovement 0x674420 (inf: the step's move)
- UR_SideSpin::ModifyAtomCore 0x69E300 (+0x20 ScaleAngularVelocity, +0x24 MaxAngularVelocity, +0x28 TimeToFade): the first time, with a player behind the effect, w = curl (mgr +0x58) x scale, clamped to +-max, then w (w / max)^2; every step the velocity turns about y by w (1 - age / TimeToFade) dt. 🔍 inferred the defaults: the ctor was not read (the fireball file sets Scale 0.5, Max 1.2, TimeToFade 1.5, destructive.md §3.4)
- AR_FadeAlphaWithHeightAboveLandscape::ModifyAtomCore 0x6A4DE0: alpha from AlphaAtZero on the land to AlphaAtRefHeight at RefHeight above it (and beyond). 🔍 inferred the defaults: the ctor was not read
- AddSubCollectionsToAtom::ModifyAtomCore 0x69F220: once per atom (its atom data +0x20), the NextGroups under it
- UR_Trail (AtomCreateRule; MAC 0x6A40F0, first step fn_006A41B0, then fn_006A4280): NumAtoms atoms behind the parent atom along the history of its last NumGameTurnsToSpreadOver positions (a ring, +0x24 of the collection data), covering at most MaxTrailLength (x the parent's scale), spaced 0.5 k^2 (UseNonLinearSpacing) or evenly, the newest last. The first and last atoms take the HeadGroup / TrailGroup init (-1: no creator, not drawn). 🔍 inferred the property defaults below: the ctor / DefineProperties of UR_Trail were not read
- The whole ring back along the velocity (fn_006A4280 0x6A4308..)
- fn_006A8020: the samples lerped
- fn_006A7E30: i-th newest sample

### Flock rules

- UR_FollowTargets::AtomData (0x28 bytes, vtable 0x8FD3EC, ctor 0x560210): +0x24 the target, +0x20 the game turn it was last seen (unused)
- UR_FollowTargets::ModifyAtomCollection 0x6A04B0 (ctor 0x6BFA20, DefineProperties UR_FollowTargets_vfunc3 0x6B1DD0 on top of AtomCreateRule's). Flags 2 without 4 (0x6BFA4C / 0x6BFA7B): no creator, but the effect waits for its targets until it closes.
- The GetSetIntegerProperty setters 0x6AC0D0 / 0x6AC100 / 0x6AC130 take 0..2 only (the files write them as FLOAT)
- FindTypeOnMap(-1, prev) 0x67D936 / 0x67D95F: the fixed list, then the mobile one (ecs::map_cells; one read snapshot for the whole walk)
- 0x8C5840 / 0x3C23D70A

### Forest rules

- AtomData of UR_ForestPath (0x30 bytes, ctor 0x55FAC0): +0x20 first, +0x24 / +0x28 / +0x2C three PSysFloatRand(2 pi)
- UR_ForestPath::ModifyAtomCollection 0x6A3770 (DefineProperties 0x6AEE60, ctor 0x6A35D0: ThetaSpeed +0x20, PhiSpeed +0x24, SphereRadius +0x28 and ScaleX/Y/Z +0x30..+0x38 all 1, ScaleSphereRadius +0x2C NULL, RadiusSpline +0x3C and HeightSpline +0x48 with their own default keys, both with zero end slopes)
- The ctor's own keys (0x6A363B..0x6A3715, four per curve) are (no portado): every spell file gives both curves

### Gesture rules

- fn_0068DE90 / fn_0068DEA0: when the collection's pulse starts, and how long it lasts
- The light sheet's points (fn_0083E710)
- The sparkle group (none in SF_Gesture): the atom runs along the ideal once every 2 s and fades out in its last 2 s (age / 2 [0xC02648] at 0x68872D, < 2 [0x8AB478] at 0x68877E, x 127.5 [0x92B6EC] at 0x68878B)
- The LightSheet (+0x2C, fn_0083E710): 50 points along the ideal, the player's colour, height = scale x LightSheetHeightScale, +0x18 = 0.03, and its alpha 1 - (2f - 1)^2 over LightSheetDieAge. The data is kept; LH3D's LightSheet draw is not ported.
- UR_GesturingRecognised::AtomData (0xC8 bytes, fn_00688140)
- [0xD4EB60] is never written: the scale is always the ideal's length / 100
- ZR_ChainGesture::AtomData (0x48 bytes, fn_0068A290)
- CreateRuleMakeChain::ModifyAtomCollection 0x69FD10 (a OnceOnlyCreateRule, NumAtoms 0x6B0B60): NumAtoms joints of PCreator (the chain creator numbers them), once

### The hand-follow rule

- UR_FollowCastPosn::ModifyAtomCore 0x69FE30: the atom goes to the gesture position (no velocity). For this computer's interface (NetUnsafeIsMyInterfaceCasting 0x673540) the collection's interpolation flag (+0x38 bit 1) is cleared and the atom gets a DrawOffsetLT (0x6C75A0) that draws it at the hand between steps: not ported (the atoms are drawn where the step left them).

### Heal rules

- 0xD4ED38 (count 0xD4ED3C): the objects some chakra is on, so that two heal spells don't chakra the same villager
- Living::SetSpecularColor 0x417480 (vt 0x5A0; Object 0x4025C0 does nothing): Living +0xD0 = the whole dword. The chakra's fade always writes alpha 0xFF (0x6A0EF5 `mov byte [esp+0x17], 0xff`), so even an RGB of 0 keeps the dword non-zero and the draw tests (fn_0051B3D0 0x51B416, Animal::Draw 0x51C4D6 `test eax, eax`) still see an own specular (it wins over the poison tint): the component stays. Only the dtor's SetSpecularColor(0) (0x6A0A28) clears it.
- fn_006A09A0 0x6A0A28..0x6A0A2A: SetSpecularColor(0), the whole dword 0: no own specular any more
- UR_HealSpellChakra::AtomData (0x38 bytes, vtable 0x93768C; ctor fn_006A0920, dtor fn_006A09A0)
- 0x6A0D28..0x6A0D51: the collection's first atom, sound not started (+0x2E) and GetAtomAge > 0 -> StartSound. The original's list grows at its head (AtomCollection +0x40, fn_00674BD0 at 0x674BEE), so its first atom is the newest one; A brand new atom has age 0, so the sound waits for the next step either way
- 0x6A0D8B: picked up by the hand ends the chakra (0x6A0DCB), but the target is kept, so the destructor still takes its glow away
- CreateRuleFusedSphericalExplode::ModifyAtomCollection 0x69F610 (DefineProperties 0x6B1580, defaults from the ctor before 0x6BF52B): once the collection is FuseTime old, NumAtoms atoms fly out in random directions (the upper half with OnlyHemisphere) at one random speed in [MinSpeed, MaxSpeed), y x ScaleYSpeed; the first plays SoundExplode and DisableParent hides the parent atom (flag 0x10). Then the rule detaches.

### Key-point rules

- 0 or 0x7149F2CA
- 0x5B37A2: the first slope; "> 0.99e30" (the double 0x9003C0) makes it natural
- UR_KPStretchHeight::ModifyAtomCore 0x6A50C0 (ctor 0x6A4F50: StartTime 0, StopTime 5, keys (0, 1), (5, 1); +0x20 the keys, +0x2C StartTime, +0x30 StopTime): inside [StartTime, StopTime] (the step that passes StopTime evaluates at it) the atom's stretch (+0x7C) = the curve at the atom's age (the keys are atom seconds)
- UR_KPMoveAtoms::ModifyAtomCollection 0x6A60B0 (ctor 0x6A5F40: StartTime 0, StopTime 5, keys (0, 0), (5, 0), MovePropAtomIndex 0; +0x20 StartTime, +0x24 StopTime, +0x28 KeyPointsY, +0x34 MovePropAtomIndex): every atom inside [StartTime, StopTime] of its own age goes to GetCurrentParentPos + (0, curve, 0); with MovePropAtomIndex the offset x index / (count - 1), index 0 being the head of the original's list (the newest atom)

### Lightning rules

- The lightning bolt's classes (SF_LightningBolt / PUOne / PUTwo, SF_LightningStrike, SF_LightningStorm; PSysLightning.cpp 0x68FD50-0x6941B0): UR_Lightning picks the objects inside a cone in front of the hand and each step grows a tree of forks (chains of joints, ParticleChainCreator, Creators/Chain.cpp) to the ones it strikes: a trunk that splits in two until every branch holds one target (fn_00691F30). It drops a light map and sends the spell a "landed" event at each struck tip, which is what burns and kills. Two bolts cast from the hand that meet are linked (fn_006916B0): both trunks end at the clash point, where a glow sits, and the older one goes on from there, three times as thick, to its target. UR_LightningStrike is the one-shot script / climate strike. Report: tmp_dis\miracles\destructive.md §4; wiki docs/bw1-notes/miracles.md, "Rayo".
- fn_00691F30 0x69226C / 0x69228B: the margin of the shield test fn_006D0BC0 and of FindIntersect (push 0x40200000)
- The objects of one 10 m map cell in the inline iterator's order (0x690344 / 0x69094B: the fixed list +4, then the mobile one +0, each from its head; ecs::map_cells); MapCoords::InBounds 0x6042C0 first (0x690333 / 0x690939)
- GameThing::IsAvailable 0x401810 (vt +0x2C): not being deleted (+0xA & 1: ecs::IsAvailable), and for a villager Villager::IsAvailable 0x751D50: its final state is not DYING (ecs::villager::IsAvailable). The other vt +0x2C overrides of the image (GGame 0x54B9A0 and the rooms of the front end) are not things on the map
- +0x18: written when the targets are found (0x6910CE) and at each strike (0x692949), ticked down every step (0x691C26..0x691C31) but never read to skip a target: kept as data only
- Fadd [edi + 4] (0x691E4E)
- AddSubCollection(CommonGlowGroup) on its last joint when it has none (0x691B28..0x691B42)
- fn_00673B20 on its last joint: every sub-collection goes (0x691AF6..0x691B03)
- UR_Lightning_CollectionData (0xC0 bytes, ctor 0x68FE20): what the rule keeps for one collection between steps
- +0x3C the origin of the last target search (0x690F72..0x690F85)
- +0x59 the rule's CastingFromHand at the ctor (0x68FECA..0x68FECD)
- +0x60 |centroid - origin| (fn_00690F50 0x69114D..0x691174), 1.0 from the ctor (0x68FE7F) until the first search
- +0xB0 g_game +0x205A40 at the ctor (0x68FE9F..0x68FEB2)
- fn_00691AD0(bolt, other, group): `bolt` (+0x20) is linked to `other`, or unlinked with 0. The old partner forgets it (+0x24 = 0, 0x691ADF..0x691AE3); a new link gives the other +0x24 = bolt (0x691B11). Linking from none puts a CommonGlowGroup collection on the last joint of the bolt's fork 0 (fn_00691B50, 0x691B1B..0x691B42), unlinking takes that joint's sub-collections off (fn_00673B20, 0x691AEF..0x691B03). The original changes the other bolt's atoms at once; here that waits for the bolt's own step (ApplyGlow), so the glow may come or go one turn late (aproximado): the other bolt's atoms are not safe to touch from this one's step
- fn_0068FF50, the data's dtor: a bolt linked to this one is unlinked (0x68FF55..0x68FF68) and the one this is linked to forgets it (0x68FF6D..0x68FF74); then it leaves the list (0x68FFAE..0x68FFDC)
- 0x691601..0x69164C: with SoundLightning (+0x80) not NO_SOUND, every step the root atom's sound of that action (GetSoundOfAction 0x674550) is started when it has none while the manager IsInState(2) (StartSound 0x6745D0), and stopped when it has one otherwise (StopSound 0x674500)
- 0x691601..0x691691: UpdateForkStructure only while the manager IsInState(2) (PSysProcessInfo +0x38, enabled)
- 0x691B28..0x691B42: only on a joint with no sub-collection; AddSubCollection 0x673BB0 skips -1. The new collection is drawn without interpolation (`and byte [edi+0x38], 0xFD`, 0x691B42)
- fn_006916B0(collection, data): a bolt cast from the hand that is not linked yet looks for an older one (+0xB0 smaller) in the list, also cast from the hand and with targets, not linked or linked to it. With d1 = that one's centroid - its origin and d2 = its centroid - this origin, the clash point is T = its centroid - 0.7 x 0.5 x (normalize(d1) + (cos h, 0, sin h)) x min(|d1|, |d2|), h this heading (0x6918E8..0x6919F3). They are linked (fn_00691AD0(older, this, CommonGlowGroup), this +0xB4 = T, 0x691A9B..0x691AB6) when their headings agree (cos h' cos h + sin h' sin h > 0, 0x6918AF..0x6918E2), T is closer than SearchRadius (fn_006901D0, 0x691A25..0x691A45) and normalize(T - this origin) lies within 1.0367 rad [0x936D90] of h (0x691A47..0x691A72). An older bolt that fails the test is unlinked if it was linked (0x691A74..0x691A81). The first link ends the search
- 0x6916BE..0x6916D0
- 0x6916D6..0x691707
- 0x691718..0x691737: cast from the hand, with targets, not linked to a third one
- 0x69173D..0x691749 (`jae`: the same turn does not clash)
- LHPoint::Normalise inline (0x69179D..0x6918AD): a zero vector stays zero with length 0
- fn_00460710: normalises and returns the length
- 0x6913BD..0x69140B: d = +0x3C - +0x30 squared as (dz dz + dy dy) + dx dx, against (R x frac)^2: no root
- fn_00690F50: the three target modes, tested in this order (0x690F88 CastingFromHand +0x75, 0x690F9E TakeTargetsFromManager +0x74): fn_006901E0 from the hand (the cone), fn_00690C70 from the manager's SpellTargets and fn_00690880 around the parent atom (the storm: no cone, a circle of the radius)
- 0x690F72..0x690F85
- 0x691072..0x6910CE: every target starts with +0x18 = PSysRand(LightmapSteps()) 0x6910BD..0x691177: with it, +0x48 = the tips of every target (fn_00691E00) summed, x 1 / their number, and +0x60 = sqrt((dz dz + dy dy) + dx dx) from the origin (+0x30) to it
- fn_006901E0: the spiral of 4 ceil(R/10)^2 cells around the origin (0x6902A1..0x6902A4); an object counts when it is available, not a spell seed, and its horizontal direction is inside the cone of half-angle SplitAngle about the heading. fn_00690880 (`cone` false): ceil(R/10)^2 cells only (0x690902..0x690906, no x4), and an object counts when dx^2 + dz^2 < R^2 (0x6909E5..0x690A1D). dx, dz are the object's MapCoords in metres minus the origin (fild x 10 / 65536, 0x6903DF / 0x6909E5)
- Fdiv [0x936C60]; _ceil; __ftol
- The origin's MapCoords (ToFixed of x and z, 0x69024A..0x690286 / 0x6908B7..0x6908EA), walked by GUtils::Spiral 0x74D7E0 from dir = count = 1 (0x6902A7..0x6902BB / 0x69090D..0x69091E) and MapCoords += JustMapXZ 0x605470
- 0x690410..0x6904A8: LHPoint(dx, 0, dz) normalised unless it is zero (x 1 / sqrt((x x + z z) + y y)), then counted when dx cos(heading) + sin(heading) dz > cos(SplitAngle)
- 0x690A06..0x690A1D: dz dz + dx dx < R R ([esp + 0x18] = R x R, 0x690902..0x69091A)
- fn_006901E0's tail: when fewer than MinLightningObjects were found, the rest are ground points at heading + rand(pi/4) and rand(0.6 R) away, two metres above the land. fn_00690880's tail (0x690B0B..0x690BF2, `aimed` false): at rand(2 pi) (0x40C90FDB) instead
- CreateForkStructure 0x691190: one atom in the collection, with two fork sub-collections per target (0x69119B.. 0x6911A6, the ForkGroup list of 2N), each holding MaxJointsPerFork chain joints (0x6912C3..0x6912FA)
- 0x69130C..0x69134C: cast from the player's own hand (fn_00691B80), every joint of every fork gets a DrawOffsetLT (SetDrawOffset 0x673AF0), so the bolt's start follows the hand between the steps
- 0x691C16..0x691C3D: +0x18 counts down; nothing reads it to skip a target
- 0x691C3F..0x691C55 -> 0x691CF5..0x691D56: a bolt in a clash strikes ONE target, the first active one from PSysRand(N) on The start index is the PSysRand drawn at 0x691CF9 (in UpdateForkStructure); a second draw, of 2 × the sub-collections, is spent at 0x691DA0.
- 0x691D70..0x691DE6: fork 0 is the trunk, the next free fork is 1 (+0xA0), depth 0, scale 1.0. +0xA4, the collection of fork PSysRand(2 x forks) or none (0x691D98..0x691DC3), is set but not used by the recursion: only its draw from the random stream is kept
- 0x691F42..0x691F60: no fork (the structure ran out) or no target: nothing. The `joints < 2` is a port guard (MaxJointsPerFork < 2): the original goes on with 0 atoms (0x69243F -> 0x692598) and would read the child origin from a null atom, and divides by n - 1 = 0 with 1; no spell file gives fewer than 2
- 0x691F9C..0x69203E: cast from the player's own hand (fn_00691B80), each joint's DrawOffsetLT gets SetRefPos (vt 0x100, 0x6C7600) with this step's origin (data +0x30) and the weight w - w i / (n - 1), w = 1 on the trunk (depth 0) and 0 on the child forks (0x691FC7..0x691FD1); a joint without one ends the fork (0x692007)
- 0x692177..0x692208: with two or more targets the fork stops at origin + (centroid - origin) x (0.2 + rand(0.4)) (0x3ECCCCCD, 0x8AB244); with one it goes all the way to it
- 0x692308..0x69231D
- 0x6923A0..0x6923C6: sqrt((dz dz + dy dy) + dx dx), inline (not a GUtils call)
- 0x6923D0..0x6923FC: with NumTexturesToTile (this +0x48) != -1 the fork's chain (+0x48) is cut in max(1, ftol(NumTexturesToTile x length / data +0x60)) repeats (chain +0x30); data +0x60 is the distance from the origin to the targets' centroid (fn_00690F50 0x691174). SF_LightningStrike / SF_LightningStormPush give 15
- 0x69244D..0x6924C3: origin + (split - origin) x i / (n - 1)
- +0x8F, the colour's alpha byte (0x692548..0x69257C): 0xFF, or PSysRand(0x7F) + 0x80 (0x692551..0x69255D)
- The last joint, `lea ebx, [ecx+0x80]` (0x69279A)
- 0x69276D..0x6927DA: each list gets the next free fork (+0xA0), depth first, with the scale [esp+0x58]; when the forks run out the recursion stops there (0x692786, 0x6927C6)
- 0x6927EC..0x6928EA: the linked bolt goes on from the clash point to its one target: a new list with it, the next free fork (none: stop, 0x6928B6), depth + 1, the scale x 3 (so opaque and thick)
- 0x6928ED..0x6928F3: ended by a clash or a shield
- 0x6928FD..0x692949: +0x18 = PSysRand(LightmapSteps()); data only
- 0x692951..0x692971: once per target, when +0x5C > 0.2
- 0x692976..0x6929F4: SpellEventInfo type 3 at the target's tip (fn_00691E00), no movement, no shield test; strength 1, 2 when the bolt is linked to another one (data +0x20, 0x6929DE..0x6929EE)
- (to do, M5) without a spell (a script / climate strike, global 0xC029D0) the original applies the static EffectValues of info 0xCC9704 at the tip (0x692A10..0x692A9B). UNVERIFIED which GEffectInfo row that is.
- +0x48 (ctor 0x690180: -1)

### The orientation rule

- UR_OrientSpriteWithVelocity::ModifyAtomCore 0x69A790 (DefineProperties 0x6AC610: +0x20 SmoothFactor, +0x24 ProportionDefault)

### Shield rules

- 0xD4EE48 (head) / 0xD4EE4C (count): the newest first. Never destroyed: the effects (and their ~Effect, which removes their spheres) can outlive a plain static at exit
- AtomCore::GetLastGlobalMovement 0x674420: the drawn position minus the one before (0 until it was drawn twice)
- The orientation fn_006A2D34 / fn_006A3AA7 give an atom from a point about the sphere's centre: the identity turned by the colatitude pi/2 - atan2(y, |xz|) (x' = c x + s y, y' = c y - s x on every row), then by the yaw atan2(z, x) (x' = c x - s z, z' = c z + s x). The Y row ends up along the point's direction.
- UR_AddDefensiveSphere::ModifyAtomCollection 0x6A2A60 (DefineProperties 0x6AE8C0: +0x20 SphereRadius, IsMagical). The first time the collection runs it (CollectionData +0x20 = 1) a DefensiveSphere is made at GetCurrentParentPos with the provider's value; after that only its radius follows the provider. The centre never moves.
- CheckShieldDeflections::ModifyAtomCore 0x6A2570 (DefineProperties 0x6AF540): an atom not yet deflected (flags +0x94 bit 3, 0x08) that is inside a shield sparks there and sends SpellEvent 4 {its position, the last move, 1, the shield's spell}; on 0 it takes the reflected velocity and, per its properties, the deflected flag, another group (MoveToBaseGroup) and the close down of its effect (SetState 1). No ported spell file uses the class.
- Not ported: GroupToMoveToIfDeflected (AtomCore::MoveToBaseGroup 0x673BD0); the property is read and ignored
- AtomData +0x20..+0x28: PSysFloatRand(2 pi) x 3 (0x6A2CF7..0x6A2D16); +0x2C: ftol(PSysFloatRand(4) + 2) (0x6A2D1E..0x6A2D31, a float draw, not PSysRand); +0x30: the orientation (rebuilt from the position each step: the spark atom never moves)
- UR_InitialSpin::ModifyAtomCore 0x69E490 (DefineProperties 0x6B1520: +0x20 ScaleAngularVelocity, +0x24 MaxAngularVelocity, +0x28 TimeToFade, all three with the editor range 0..1 that nothing applies). Once the effect has a player: spin = PSysProcessInfo curl (manager +0x58, fn_00673640) x ScaleAngularVelocity, clamped to +-MaxAngularVelocity. Every step the atom turns about its own Y axis by spin (1 - clamp(age / TimeToFade, 0, 1)) dt. The defaults are the rule's constructor (0x6BF450: +0x20 and +0x24 1.0 at 0x6BF47B..0x6BF483, +0x28 2.0 at 0x6BF48C); SF_DefenseSphere.txt gives all three anyway (1, 5 and 5)
- GetCurrentParentVel 0x674BB0 x dt
- 0x57CFAF
- 0x937598
- DefensiveShield (PSysBase, ctor 0x6D0A60; +0x14 prev, +0x18 next, +0x1C the owning PSysManager) and its only kind, DefensiveSphere (0x30 bytes, fn_006D0D20): TSphere {radius +0x20, centre +0x24}
- fn_006D0B20: a new sphere at the head of the global list 0xD4EE48 (count 0xD4EE4C); its id
- fn_006D0B70: unlink and delete (CollectionData of UR_AddDefensiveSphere, fn_006A2A30)
- GJUtils::PointIsInSphere 0x57C9F0 (vt 0x108 IsPointInShield 0x6D0DA0): |p - c|^2 < (r + margin)^2
- Vt 0x104 HasCrossedIntoShield 0x6D0DC0: `to` inside and `from` not (both with the margin)
- Vt 0xFC FindIntersect 0x6D0D50 -> GJUtils::FindIntersect 0x57CCD0: where the segment from -> to meets the sphere of radius r + margin (out = `to` when nothing better; false when the segment misses it)
- Vt 0x100 DeflectOffShield 0x6D0D80 -> GJUtils::DeflectOffSphere 0x57CFD0: v -= 2 (v.n) n, n = normalize(p - c)
- fn_006D0BC0 / fn_006D0BE0: the first sphere of the list that contains the point
- fn_006D0C20 / fn_006D0C40: the first sphere the move from -> to crossed into

### The sprinkle rule

- UR_HandSprinkle::ModifyAtomCollection 0x6A0220 (DefineProperties 0x6B19D0): one source atom at the gesture position, no higher than 58 m above the land. The first step also starts the hand's raise (HandStateGrain) when this computer's interface casts it. FracToCloseDownOn (+0x38) and KeyPoints (+0x44) are read but not used here: the hand uses its own key points (the same ones).
- AppearanceRuleTumble::ModifyAtomCore 0x6A6200 (DefineProperties 0x6ABC10): the atom turns by |v| x TumbleSpeed x dt (at most MaxTumbleSpeed with RestrictMaxRotation) about z when it moves more along x than along z, else about x

### Storm rules

- ================================================================================================================ UR_CloudMoverNew (ctor 0x6D4150, props 0x6ACD10, ModifyAtomCollection 0x6D41C0) ================================================================================================================
- CollectionData@UR_CloudMoverNew (0x24, ctor 0x560C10): +0x20 the first call
- +0x24 (ctor 0x3D75C28F)
- +0x28 (ctor 0x42700000)
- 0x6D4242: the first call puts every atom at the manager's origin
- 0x6D427B: nothing before the manager's age reaches DelayBeforeMove
- 0x6D42EA: no wind for a script's storm; else fn_00771B10(p, smooth 1), the climate wind / 8
- 0x6D433A..0x6D446D: T = wind x WindMagnification x 0.1; v += WindDamping x (T - v) x dt
- 0x6D46A7: the spell's position (+0x14) follows the core (MapCoords::Set 0x603340)
- 0x6D46C3: the tornado's cores bounce off shields
- ================================================================================================================ UR_CloudGather (ctor 0x6D4700, props 0x6ACD70, ModifyAtomCollection 0x6D4A70) ================================================================================================================
- AtomData@UR_CloudGather (0x38, ctor 0x560B60)
- CollectionData@UR_CloudGather (0x58, ctor 0x560AF0; its dtor fn_006D4900)
- fn_006D4900: a detached lightning collection is deleted, and fn_006D4950 marks the storm (fn_0083F7B0)
- The clouds of the current collection that are past half formed (the static GJArray 0xD4EE88, count 0xD4EE90, grow 10: shared by every gather, emptied at each call)
- Ctor 0x6D471B: flags |= 6
- 0x6D4A7A: the three float providers must exist
- 0x6D4B14: IsInState(1), closing: the storm is marked (fn_006D4950) and no longer registered
- 0x6D4B5D
- 0x6D4B8C: only the gather under the first cloud core registers the storm (fn_00673CA0: the parent's index in its collection is 0; [0xC09790] is 1), and at level 1 it gets the tornado too
- 0x6D4C27: emit at the rate while the count is below the emitted amount and NumAtoms
- 0x6D4CF3: the strike ends: the lightning collection is let go of its cloud (fn_00674A30(0)); it stays with the gather, not updated, until the next strike
- 0x6D4D68: the collection's own ramps, over its first 10 s
- 0x6D4F4D: a cloud older than TimeToForm starts again (fn_006D4880, age 0)
- 0x6D4F8E: [0xC0979C]: fn_006D56B0(0.2, 2 - 2f) past half formed, its result discarded
- 0x6D4FD8 ([0xC0978C])
- 0x6D5012: the angular speed peaks at half formed ([0xC097B0]: x (1 - (2f - 1)^2))
- [0xC097B4], the double 0x8D45D8 (2 pi)
- 0x6D5064 [0xC09798]: the radius wobbles by 1 + 0.3 cos(collection age x 0.1 + theta)
- 0x6D51C9: +0x8C = (alpha, c, c, c); +0x78 the scale; +0x7C the ratio x the collection's ratio scale
- 0x6D51D5: y = baseScale x height x ruleScale + the CloudHeight provider (the land is added below)
- 0x6D5205: the lightning's light on its cloud, SpecLife long: +0x90 = (A, R, G, B) = (v, 200v/256, 200v/256, v) >> 8 with v = ftol((1 - t / SpecLife) x 255) (bytes 0x6D524C..0x6D5283). It is drawn: DrawData +0xC (0x679BF4) -> RenderParticleMist::DrawAt 0x67A6D6 -> SetColour 0x7F9770 (mist +0x50, which the effect branch of fn_007FA300 leaves alone) -> fn_0080DB30 0x80DEF5 [0xE9FE2C] -> the vertices' specular (0x84D645), added with D3DRS_SPECULARENABLE on (fn_0082C8F0 0x82CC45..0x82CC54, off only on a Voodoo): Creators/Mist.cpp
- 0x6D54D8 [0xC09784]: the clouds sit at the mean land altitude of a 3 x 3 grid of 0.33 x radius around the core (the tornado's: at the manager's origin, the tornado's base)
- 0x93A604
- 0x6D55B9 [0xC09788]: every cloud up by that height
- 0x6D561F: the LH3DStorm, registered once by the first core's gather, kept alive at the core every step
- fn_0083F6F0
- fn_0083F8D0: gone or marked -> 0 (and the next step registers a new one); else fn_006D5950 moves it
- fn_006D56F0: the storm spell's GMagicStormAndTornadoInfo (rainAmount, file +0x58); < 0 without one
- fn_00673CE0(0): the atom's age is 0
- fn_006D4970: NumAtoms clouds at once, each at a random age up to TimeToForm
- 0x6D52F9..0x6D54D6: a strike from a random formed cloud
- PSysRand(count) 0x6729E0 (UR_CloudGather 0x6D535C)
- AtomCore::StopAllSounds, then the thunder with a random size class (0x8CA268 0.33 -> 3, 0x8CF128 0.66 -> 2, else 1) and flags |= 0x22 (delayed by the distance, snapped to the ground)
- ================================================================================================================ UR_Tornado (ctor 0x6D1680, props 0x6AD550, ModifyAtomCollection 0x6D18B0) ================================================================================================================
- CollectionData@UR_Tornado (0x74, ctor 0x5608A0). The original keeps the one being processed in 0xD4EEC0 and its collection in 0xD4EEC4.
- FlyingAtomData@UR_Tornado (0x2C, ctor 0x5609F0)
- +0x28 rand(1) + 0.5 (fn_006D1C40)
- DebrisCollectionData@UR_Tornado (0x28, ctor 0x560950) and FlyingCollectionData (0x28, 0x5609A0): +0x20 made, +0x24 to make
- fn_006C9FC0 0x6C9FD8..0x6CA02F: not available (vt +0x2C: a dying villager too), only let go (+0x1C = 0, the erase above), no kill or deletion
- fn_006C9EF0 0x6CA007 (physics::psys_objects::Release): back on the map where the atom is (its angles from the matrix, altitude 0), EndPhysics(NULL, 1), +0xA &= ~0x10; then DestroyedByEffect(player, 1.0) 0x6CA01A: a villager or an animal dies there. The object's matrix is the atom's (UpdateCarriedObjects)
- Villager::DestroyedByEffect 0x7502D0 (fn_006C9EF0's player, 1.0) -> VillagerDead(2 SPELL)
- UpdateDebrisAtoms 0x6D2B9B..0x6D2BB2: MapCoords(LHPoint) 0x603160 of the tornado's position, then its high words (the port keeps the check against the island's side: the original passes them on unchecked)
- +0xC8 (0x3F84B36D)
- 0x6D1924: the TornadoScale provider (1 without one) and strength 1
- 0x6D196A
- 0x6D1A2A: the parent (the tornado root) and the funnel's atom sit at the base
- 0x93A594
- PSysManager::SetOrigin 0x672E10
- fn_006D2860: the sprites' scale at h, (BaseScale + (TopScale - BaseScale) h^2) x tornadoScale
- fn_006D2710: the angular speed at h, lerp(BaseThetaDot, TopThetaDot, bias(ThetaBias, h)) x speed x strength (it also writes ThetaBias clamped to 0..1 back into the rule)
- 0x8C36A0
- fn_006D2700
- 0x900C60
- The velocity a debris or pretend object is thrown out with: baseVelocity + tornadoScale x speed x (cos th sin ph, cos ph, sin th sin ph), ph = rand(spread), th = rand(2 pi), speed = rand(0.33, 0.66) x the rule's (0x3EA8F5C3 / 0x3F28F5C3)
- 0x6D2C21..0x6D2CC5: each channel of the creator's colour x the dust's >> 8, the alpha kept
- The tornado collection's age (0xD4EEC4), g_game +0x205A40 % 3
- The tornado's MapCoords (ToFixed of x and z, 0x6D228D..0x6D22A7) and its copy walked by GUtils::Spiral 0x74D7E0 from dir = count = 1 (0x6D22E9..0x6D22F7), MapCoords::InBounds 0x6042C0 on each cell, += JustMapXZ 0x605470
- 0x93A564
- fn_00604F40 (0x6D2369): the object's own cell is this one (a fixed object in several cells is seen once)
- 0x6D246E..0x6D2569: a pile gives lerp(ResourceAmountRemoveMin, Max, clamp(tornadoScale, 0, 1)) (fistp, rounded) as a new pile of its kind (fn_0066F0D0 -> Pot::Create), scaled by rand(0.7, 1.2) x clamp(tornadoScale, 0.2, 1)
- fn_0066F0D0: at most GetResource (vt 0x98: a store pile answers with its store's total, as the hand reads it)
- Pot::Create with the GPotInfo of vt 0x870 GetHandPotInfoType: PileFood 0x66EC30 = 12 HAND_FOOD, PileWood 0x66EC40 = 11 HAND_WOOD (the hand's own pile kind), at the pile's position
- Pot::Create's CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9): into its cell at once
- 0x6D257A..0x6D269D: a flying atom that carries the object (RenderParticleGameObject, fn_006CA0E0), from the object's world matrix (fn_00674150), rising to rand(0.7) of the funnel's height
- fn_006CA0E0: not IsAvailable (vt +0x2C at 0x6CA120, a dying villager too) -> the atom carries nothing (+0x1C = 0, 0x6CA162); already carried by a PSys (+0xA & 0x10, 0x6CA148..0x6CA14C -> 0x6CA169) -> no take, but the atom keeps it (+0x1C kept); IN_PHYSICS (flying, 0x6CA14E) -> nothing; else fn_006CA060 (physics::psys_objects::Take: the class's InitialisePhysics with no body, a Living FLYING, out of the map cells, GameThing +0xA |= 0x10), which may refuse it too
- 0x3F333333
- fn_006D1C40
- A new one: state 2 (ctor 0x5609F0)
- fn_006D28E0: up or down towards lerp(base.y, top.y, blend), k = +0x60 (0.1) for game objects, +0x5C (0.3)
- fn_006D27E0: plus the funnel's own velocity at that height
- CollectionData@UR_StormCast (0x40, fn_006D5F40)
- AtomData@UR_StormCast (0x2C, fn_006D5F70)
- +0x64 (ctor 0x6BD033)
- 0x6D5A1F
- 0x6D5A68: the heading scaled to the length +0x64 (20, 0x41A00000: set by the ctor, no property)
- fn_0083F490 -> fn_0083F3F0 (the position stays 0: fn_006D5950 sets it next)
- Inner = max(R, 60), outer = max(max(2.5 R, inner + 20), 80): 0x6D5751 / 0x6D576E / 0x6D5782 are `fcomp; test ah, 0x41; je` that keep the value only when it is above the constant
- 0x93A608 / 0x8C6CA8
- +0x18 = 0x4E6E6B28
- The storm miracle's classes (SF_LightningStormPush and SF_StormCast; PSysTornado.cpp 0x6D15D0-0x6D6160): the cloud cores drift with the wind (UR_CloudMoverNew), the clouds gather round each core and register an LH3DStorm that rains on the land (UR_CloudGather), fork lightning strikes from random clouds (UR_Lightning of Rules/Lightning.cpp in its parent mode) and, at power-up level 1, a tornado sucks up objects and piles (UR_Tornado). UR_StormCast is the swirl at the hand while casting. Research: tmp_dis\miracles\impl\m6st\ (the disassembly read for this port); wiki: docs/bw1-notes/miracles.md, "Tormenta".
- Schlick's bias (fn_006D2710 / fn_006D2910): x^(ln b / ln 0.5), the exponent through 0xD4EEA0 = 1 / ln 0.5 (crt_xc_fn_PSysTornado_006D1660)
- UR_CloudGather 0x6D4D68..0x6D4E23: the collection-age smoothstep s = (3 - 2t) t^2 with t = clamp(age x 0.1, 0, 1); returns c + (1 - c) s (CloudRatioMaxCollection or CollectionRadiusInitialScale as c)

### Surfaces of revolution

- (esi from the xor at 0x685AA6)
- TestFunnelSpout 0x686940: r = 1.5 t, y = 3 (sqrt 2t - 1)
- TestFunnelParab 0x686910: r = t, y = 3 (t^2 - 1)

## openblack

Status: **partial**: the rules of this page are ported; the index of the rest is on
[Particles: the PSys core](particles.md#rule-index).

- `src/Particles/Rules/` (one file per rule family: `Belief.cpp`, `Explosion.cpp`, `ExplodeObject.h`, `Fireball.cpp`,
  `Flock.cpp`, `Forest.cpp`, `Gesture.cpp`, `HandFollow.cpp`, `Heal.cpp`, `KeyPoints.cpp`, `Lightning.cpp`,
  `Orient.cpp`, `Shield.cpp`, `Sprinkle.cpp`, `Storm.cpp`, `SurfRevol.cpp`) and `Particles/ParticleBeamRules.cpp`,
  `Particles/ParticleObjectRules.cpp`;
- the pure maths of the rules (for example `BeliefSpriteMaths`) is tested with fakes and pinned to the oracle.

Where it is in the code (the rules' helpers, by rule file):

- `Rules/Explosion.cpp`:
  - `PositionOf`: a target's world position, its map coordinates as a point plus its height above the land.
- `Rules/Fireball.cpp`:
  - `CloseWater`: the event condition "close to water": not deflected, below the cut-off height above the land and
    over water (or off the land);
  - `FireBallSteam`: the steam condition: not deflected, and raining or snowing there;
  - `HasBeenDeflected`: the atom's deflected flag;
  - `DeflectedInHierarchy` / `DeflectionInHierarchy`: the atom or one of its parents was deflected (the condition on
    an atom and on a collection);
  - `PitchedUp`: the hand's direction pitched up 0.3 rad, clamped to ±pi/2.
- `Rules/Flock.cpp`:
  - `AtomNearVillagers`: the "near villagers" condition, walking the cell's fixed objects then the mobile ones from
    one map-cell read.
- `Rules/Heal.cpp`:
  - `SetSpecularColour`: the target's own specular colour, written whole (the chakra's fade keeps alpha 0xFF);
  - `TargetPosition`: the healed target's place (its centre or its base); the sound starts on the newest atom, which
    is the last of openblack's vector.
- `Rules/Lightning.cpp`:
  - `CellObjects`: the objects of one 10 m map cell, fixed list first, then the mobile one;
  - `EffectOf`: the effect of a linked lightning (its sound and fork update only while the manager is enabled).
- `Rules/Shield.cpp`:
  - `LastGlobalMovement`: the drawn position minus the one before (0 until the atom was drawn twice);
  - `OrientAlong`: the orientation of an atom from its point about the sphere's centre (colatitude, then yaw).
- `Rules/Storm.cpp`:
  - `CollectionScale` (`Storm.h`): the collection-age smoothstep c + (1 - c) s of the cloud gather;
  - `HolderOf`: the cloud that holds a collection, or none;
  - `ParentPosition`: the parent atom's position, the effect's origin without one;
  - `TornadoCarrier::Release`: lets a carried object go back on the map where its atom is, and a villager or an
    animal dies there; an unavailable object is only let go;
  - `TornadoDustColour`: the dust colour of the land material under the tornado;
  - `TouchExitGuard`: a static guard that marks the program's exit, so the rule's data are not touched during it.

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#particles).

#### The belief sprites

- openblack: `ecs::town_belief::DrawBelief` / `QueueBeliefSprite`.
- openblack: `Rules/Belief.cpp` registers `UR_BeliefSprite` and runs the step above through `BeliefSpriteMaths`, popping `ecs::town_belief::PopBeliefSprite` (last in, first out) until the queue is empty. The utility slot steps it on the local stream (`Particles/Utility.cpp`); the manager draws it on the Sorted path with the turn's fraction, as every effect, so the sprites need nothing new in the renderer. The sound is `audio::spell_sounds::StartSound`.

#### The simple beam

- openblack does the same.
- In openblack: `ParticleBeamRules.cpp` (the rule) and `BeamMaths` (the pure key points and joints). The curve is the key-point spline of `Rules/KeyPoints` (one per axis on the same t: the vector build fn_00677490 is the scalar fn_005B3760 per component), read by `key_points::EvaluateVector` in fn_006771F0's order, (a y_lo + b y_hi) + S x ((h x h) x 1/6), which is not the scalar reader's (see miracles.md, the other missing classes). The land is `GetHeightAt`, the noise `SignedValueNoise`, an object's place its Transform + half of `GetHeight`. The new atoms draw only their `Rand(0x100)`: 1 + NumBeams x MaxJointsPerFork per target, on the effect's stream. No run of the checks parses the class: it is started only by the script natives SPECIAL_EFFECT_POSITION / SPECIAL_EFFECT_OBJECT with type 20, from challenge scripts (FindCirclePointDust, FreezeRayStatue, KillChild, LethysVortex, LookAfterShieldStone, SlaverCheckAnimal, SlaverGoMad, TheWorkshopComplete, ThrowThroughShield); types 13, 21 and 41 are never started by a script.

#### The glints on a target

- In openblack: **registered**. The rule is `Particles/ParticleGlintRules.cpp` (`RegisterGlintRules()`, in `PSysRegistry.cpp`'s list after `RegisterBeamRules()`), its pure maths `Particles/GlintMaths` (the rate, the pulse, the size, the vertex walk and the per-axis sum), and the targets `Locator::glintTargets` (`GlintTargetsInterface`, implemented in `ECS/Systems/Implementations/ParticleWorldGlints.cpp`): a script highlight gives its `Mesh`'s vertices through the matrix it is drawn with this frame (`DrawnModel`, no slope shear) and its `scale`; a seed graphic gives its `Mesh`'s vertices through `SpellSeedGraphic::glintModel` and its `scale`. That matrix is the creation matrix above at creation (`seed_graphic::CreationGlintModel`), and after each `DrawSpellGraphic` that draws the mesh it is that draw's matrix (`seed_graphic::DrawnGlintModel`, the `affine::PlacementMatrix` of `SetPosition`). The holder steps before the draw, as in the original, so its glints read the previous draw's matrix. `seed_graphic::Create` gives the holder its graphic as the target, then its player, as fn_00727190 does. The vertices are the file's positions (`GetSkinLocalPositions`) walked by each primitive's range (`GetCollisionVertexRanges`). An object is available while it is a valid entity without `Unavailable` and has a `Transform`. The script highlight starts its glints as an effect it steps itself (`script_highlight::Create`: `StartForSpell` on the local stream, SetPerFrame, Sorted, AddTarget), so `manager::ProcessTurn` leaves them alone. Its Draw (`script_highlight::Draw`), which only the object draw list calls ([Pending](../rendering/object-draw-list.md#pending)), steps them with power 1, enabled and dt = the frame's game ms × 0.001, twice at their first step, after it has set the highlight's pose for that frame, so the glints read this draw's pose. On a frame where the Draw did not step them they are not drawn (`script_highlight::EndDrawnFrame`, the effect's `drawn` flag). The highlight's active effect is made the same way (`SetActivated`, without a target) and its Draw moves it to the mesh's centre (`SetOrigin`) and steps it after the glints, twice at its first step; the gold scroll's sparkles are not a PSys effect ([Script highlights](../interface/script-highlights.md#script-highlights)).
- openblack's clock gives the whole first turn to that frame

#### The object rules

- In openblack every villager (`MSH_P_*`) and animal (`MSH_A_*`) mesh is boned and keeps the default type, and the components with a `SkeletalAnimation` that are Objects (the whale, the animated statics of info.dat, all boned) are ANIMATED: so the animated set is `SkeletalAnimation`, `Villager` and `Animal`, the game's rule for every entity openblack makes.
- In openblack: `GameObjectRef` (`KeepsAlive`) and `EmitFromParentAtom` (`Creates`), registered by `RegisterObjectRules`. A target is kept when `object_surface::IsGameObject`: a valid entity with one of the components that stand for the Object classes above (a fish farm with no mesh is one, and gets its atom and its Rand(0x100); the hand is not). The place is `map_coords::ToWorld(ecs::object::MapCoordsOf(object))` + OffsetY; an object found unavailable is let go at the start of the next step (the same reads as the game's). The surface point walks the loaded mesh's sub-meshes, primitives and triangles with the file's vertices (`object_surface::RandomPoint`, the sums in the game's order), through `object_surface::WorldMatrixOf`: the translation from the MapCoords, and the class's rotation (`MobileWorldMatrix` for the mobile objects and mobile statics, `ObjectWorldMatrix` for the others). The emitter keeps one point for the step and converts it in place, as the game does; its land test is `object_surface::LandTestCell`, (x x 65536) x 0.1 in floats. 65536 x 0.1f is 6553.6f exactly, so that is `map_coords::ToFixed` value for value: the earlier form gave the same cells. All draws are on the effect's stream: Synced for the spot visuals.

#### The storm in the hand's flashes

- openblack: `storm_flash::ShouldEmit`, `FlashSpecular`, `FlashStep`, pinned to the oracle in `test/test_storm_flash.cpp`, and the rule `LightningSprite` (`RegisterStormFlashRules`): its data is a `storm_flash::State` on the collection, the draw `Effect::Random` on the effect's stream (the in-hand effect is Local, stepped every frame with the frame's dt), the sprite `Effect::NewAtom`, the flash written to `Collection::parent->specular`. Before it, the class was not run: the storm in the hand showed no lightning and its cloud never lit. The in-hand mist is drawn through `mist_atoms::Describe` with that specular, as the cast storm's clouds.

#### Heal rules

- openblack's vector grows at the back (PSys.cpp NewAtom), so the newest is the last.

### Test hooks

- None of their own: `OPENBLACK_TEST_PSYS` ([Particles: the PSys core](particles.md#test-hooks)) runs any spell
  file and so any rule; the rules' maths is pinned by the unit tests named in each section.

## Pending

- **Verify with xref/oracle:** the 4 **🔍 inferred** and 1 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- `ER_EmitFromParentAtom` and `CreateRule_GameObjectRef` (SF_SparklesFromObject, SF_ButterfliesOnObject, creatures): no
  player miracle uses them; ported, with what is left, in [The object rules](#the-object-rules).
- **Stage D and the highlight's Draw** (`script_highlight::Draw`). The three early exits return 1 and step nothing.
  Otherwise the pose (`script_highlight::PoseOfDraw`: the turn by this frame's game ms or the did-you-know's facing,
  then the distance scale, from the list's eye) is written to the Transform, then the mesh's on-screen test
  (`draw_list::OnScreen`) with `field_of_view::ObjectOnScreen`'s centre and radius through that pose, then the glints'
  step ([The glints on a target](#the-glints-on-a-target)), the active effect
  moved to the centre and stepped, and a gold scroll's sparkles queued ([Script highlights](../interface/script-highlights.md#script-highlights)). The
  flag is the test's result, or 1 when the glow branch (an active scroll that is not a did-you-know) or the sparkles
  branch (row 3) runs; every highlight has its sparkles from its creation. `EndDrawnFrame` then hides the effects the
  list did not step; the queued sparkles step at the frame's end, in the renderer's PreDraw (`DrainSparkles`), far
  first, as the Z queue's drain steps them.
- The object rules ([above](#the-object-rules)):
  - DoShower's golden shower (`RenderParticleGoldenShower::DrawAt` 0x67B0F0, fn_006CA7A0..fn_006CA990) is not ported:
    SF_HighlightOnObject's atom is kept on its object but nothing is drawn for it.
  - The animated branch of `GetRandomSurfacePos` (one `PSysRand([GetMesh()+4] & 0xFF)` and a row of the table
    [0xC37D9C] filled by fn_00839980) is not decoded: openblack lets nothing out from an object with a
    `SkeletalAnimation`, a `Villager` or an `Animal` (and logs it once), where the game draws one random number and
    lets the atom out at a bone. That set is the game's animated objects (above).
  - The creature's static walk over the body mesh of its last drawn level of detail (above) is not ported: openblack
    lets nothing out from a creature (and logs it once), where the game walks that mesh's parts, items and triangles
    with the static branch's draws (none before the creature's first draw).
  - The mesh parents (`Particle3DObj::GetRandomSurfacePos` 0x6C7C10) are not ported (no file emits from one).
  - The point the emitter starts each step with is an uninitialised stack local in the game: esp+0x14 of the MAC's own
    frame (`sub esp,0x1C`, 0x6A5C99..0x6A5CA5). It is not heap memory (no 0xCDCDCDCD), and the release prologue does
    not fill the frame (no 0xCCCCCCCC), so it holds whatever the previous call at that stack depth left there (the
    caller's loop calls every modifier at the same esp). It cannot be known from the code; only a trace or an emulation
    could show it. It is read only when the step's first pick writes nothing: a parent object with no 3D object
    (obj+0x40 = 0), a null part or item pointer, the animated branch without a mesh, or an atom with no render particle
    (+0x128, `AtomCore::GetRandomSurfacePos` 0x673E60). openblack starts it at the parent's place; from the first atom
    of the step on it is the game's (the previous atom's local position).
  - The cast to `Object` is the RTTI's; openblack has no classes, so `IsGameObject` lists the components that stand for
    the Object classes (the RTTI list above). The Objects openblack has no entity component for (the firefly, the
    furniture, the help spirit, the town desire flags...) cannot be targets; a new component that
    stands for an Object class has to be added to the list.
  - An object destroyed at once (no deferred deletion) is read no more, where the game still reads its MapCoords and
    its 3D object until the step's post-update lets it go.
  - The world matrix's angles: openblack keeps them for no object, only the Transform. For a mobile object or a mobile
    static the Transform's rotation is taken as `SetYXZMatrixOnly` of its angles (it is made from them, at creation as
    `AngleY`, and again at the end of the physics). `AngleY(y)` and `RotationYXZ(y, 0, 0)` (the game's
    `SetYXZMatrixOnly(y, 0, 0)`) have the same values, but not the same signed zeros: the latter has -0 in m1 and a
    zero whose sign follows the angle in m5, where `AngleY` has +0. Through the surface point's sums that changes
    nothing (a zero product only changes the sign of a sum that is all zeros). For the others, with a scale of 1 the
    cells are the Transform's (the angle's cosine and sine stored as floats, exact while the Transform is `AngleY` of
    the angle). With another scale the angle is read back from the Transform (`DecomposeYXZ`, whose arctangent
    quotient is a float), and that is not exact: simulated over the angles, the angle read back differs in 13.8% of
    those in [-pi, pi] and the cells in 12.8%; for angles in [0, 2 pi] in 57% and 47%, the angles beyond pi coming back
    wrapped. It is reached only by a scaled Y-only object (a tree, an abode, a feature) given spot visual 46 by a
    script's SPECIAL_EFFECT_OBJECT: the engine's own route to the emitter is spot visual 46 on a food pile, a mobile
    object and so exact. No script source in Scripts/ names it; the compiled challenge scripts are not checked. The
    temple's (its 3D object's matrix) is taken as the Transform's

    rotation x scale at its position, not shown to be the same floats. The cosine and sine are taken in double and
    rounded once, where the x87 keeps 64 bits: the last bit may differ in rare cases.

  - The surface walk counts the loaded sub-meshes; a sub-mesh our loader drops (no vertices or no triangles) is still
    one of the game's parts. No such mesh is known among the objects these files are given.
