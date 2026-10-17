# Lightning explosion and exploding objects

## Summary

The lightning explosion (the beam explosion) brings a column of light down on a point, then blasts everything
within reach: about 50 damage events at the centre, smoke or steam, a crater on land or rings on water, five rocks
that burst into pieces, and a ring that grows outwards breaking up to 15 objects into flying fragments and deleting
up to 15 more. Its powered-up versions add six or fifty-six more explosion points. A building it destroys disappears
from view and becomes a building site.

openblack ports the explosion, the beam, the crater, the flying pieces and the missing particle classes faithfully;
the towns' building sites are missing, so a destroyed building stays drawn whole. Its particle rules are on [Explosion rules](../physics/particle-rules.md#explosion-rules) and [Exploding objects](../physics/particle-rules.md#exploding-objects).

## The original

### Lightning explosion and missing PSys classes

The lightning explosion (MAGIC_TYPE 7-9 EXPLOSION_ONE, EXPLOSION_ONE_PU_ONE and _PU_TWO; seed BEAM_EXPLOSION) is a
simple `Spell` (class General): everything it does comes from the events of `UR_Explosion`. It casts particle types 11
/ 12 / 13 (`SF_BeamExplosionSingle` / `Many` / `Loads`). The particle meshes it uses (the rocks, the column and the
cones) are on [the particle meshes and the shield dome](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome).

#### The files

**Faithful.**

- Group 8 (created at the start): a control atom with `SetPSysCloseDown` 0x6A26D0 after 6 s
  (`EventConditionCollectionDelay`; 13.9 s in Loads), which closes the effect (`PSysManager::SetState(1)` 0x672FF0).
  In Many/Loads a `SpreadingDiskEmitter` distributes 6 (Many, radius 30→50) or 6 + 50 (Loads, 25→40 and 20→60) more
  explosion points.
- Group 7: the parent of each explosion (dies at 16 s); group 6: the `UR_Explosion` collection, without atoms.
- Values of `UR_Explosion` in the three files: `InitialDelay` 0.4, `TimeToDoEventsFor` 5, `SmokeDelay` 1.2,
  `BlastSpeed` 50, `SpreadSpeed` 20, `MaxDistance` 15, `MaxObjectsToDelete` 15, `MaxObjectsToExplode` 15;
  `BeamDelay` does not appear (0, the ctor's). Default values of the ctor 0x67E090: MaxObjectsToDelete / ToExplode
  20, MaxDistance 100, BlastSpeed 10, SpreadSpeed 10, TimeToDoEventsFor 5, InitialDelay 3.5, SmokeDelay 3, BeamDelay 0.
- `SpreadingDiskEmitter` (MAC 0x6A6610, DefineProperties 0x6AFA90) emits **several atoms per step** (loop with
  `ShouldEmit`) and moves each one `(cos θ r, Height, sin θ r)` with r = PSysFloatRand(StartRadius, StopRadius) and θ
  = PSysFloatRand(2π). Its `StartTime` / `StopTime` (+0x5C / +0x60) are properties that the class **does not read**.
  `DiskEmitter` 0x6A64D0: one atom per step, random direction in the disc with radius PSysFloatRand(Radius) and
  Height in y. `ShouldEmit` 0x6A63A0: period 1/EmissionFreq with `Randomise` × (0.5 + rand(0.5)),
  `MaxTotalAtomsToEmit` and `MaxAtoms`.

#### `UR_Explosion`

- Properties (DefineProperties 0x6B0B90, an `AtomCreateRule`): +0x2C MaxObjectsToDelete, +0x30 MaxObjectsToExplode,
  +0x34 MaxDistance, +0x38 BlastSpeed, +0x3C SpreadSpeed, +0x40 TimeToDoEventsFor, +0x44 InitialDelay, +0x48
  SmokeDelay, +0x4C BeamDelay. Per-collection data (0x58 bytes, ctor fn_0067E140): centre +0x24, "not started"
  +0x20, "target remains" +0x21, ring +0x30, list {turn, object} +0x34, exploded +0x48, deleted +0x4C, smoke / beam /
  stopped +0x50 / +0x51 / +0x52 and the beam's container +0x54.
- **Each step** (ModifyAtomCollection 0x67ECE0): centre = `GetCurrentParentPos` with y = the ground height. If it is
  neither stopped nor closing: with the collection's age > InitialDelay, `InitCollection` (once). While age <
  InitialDelay + TimeToDoEventsFor, **one `SpellEvent 2` at the centre per step** (velocity 0, strength 1, no
  shields): the spell's default event, burn / crush / hit of the effect (BEAM 200 / 0.01 / 0.01, PU1 400, PU2 800 /
  0.02 / 0.02) in its radius (5, 5, 10) with `ApplyEffectToMapPos`, paying `costPerEvent` (10 chants) each time: about
  50 events. With age > BeamDelay, spot visual 36 `BEAM_EXPLOSION_FX` at the centre (scale 1, 60 turns); with age >
  SmokeDelay, the 23 SMOKE on dry land or the 22 STEAM on water (`MapCoords::IsDryLand` 0x603620), **magnitude 8**
  for 4 s. Constants: smoke magnitude [0x9357E4] = 8, smoke seconds [0x9357E8] = 4; spot visual numbers pushed at
  0x67EE99 (36), 0x67EEDE (23), 0x67EEF3 (22).
  - **Turns, not seconds** ✅ faithful: `CreateSpotVisualWithSpecifiedDuration` 0x63E580 receives turns. They are the
    60 of 0x67EE92 and, for the smoke, an inline copy of `NumGameTicksPerSecond`: `(1000 div [0xD01A38]) · 4`, ftol,
    `and 0xFFFF` (0x67EEF8..0x67EF2C; it does not call 0x711630).
  - Both effects come out at `MapCoords(centre)` 0x603160. The container's effect starts at that MapCoords as a
    point: fn_0063E410 0x63E418..0x63E47B, `GetAltitude` + the altitude.
  - Stopped or closing: the beam is closed (`GParticleContainer::CloseDown` 0x63E370) and the list is emptied.
    Afterwards, always, the update 0x67E900.
- **The symbol `RecursiveUpdateForkStructure@UR_Lightning` 0x67E900 is misnamed**: it is the update of
  `UR_Explosion` (without recursion). Each step the ring grows `SpreadSpeed × dt` ([0xD4E0EC]); targets that are no
  longer available leave; with exploded ≥ MaxObjectsToExplode **and** deleted ≥ MaxObjectsToDelete the list is
  emptied. It walks the list and **handles at most one object per step**: the first one with a 3D object (+0x40) that
  the ring reaches in 3D (`|p − centre|² ≤ (GetRadius + ring)²`) receives, if it is inside a shield with margin 2
  (fn_006D0BC0), a spark (fn_006D0AF0) and an event 4 to the shield (with 0 it is saved); then
  `GetActualObjectToEffect` (vt 0x5D8) and the question `SpellEvent 7` (CanDestroy). With 1: if it is not a creature
  and there are some left, exploded + 1 and the mesh in pieces (fn_00681260, see "The pieces" below); if there are
  deleted ones left, deleted + 1 and `DestroyedByBeam` (vt 0x500) if [0xC029EC] (1). Whatever it answers, the object
  leaves the list; the one that answers 1 cuts that step's walk.
- **InitCollection 0x67E200**: margin = the radius of the spell's effect (`GMagicEffectInfo` +0x2C = file 0x1C), 5
  without a spell. Inside a shield (fn_006D0BC0 with that margin): the point where a ray from 200 m higher up cuts the
  sphere (vt 0xFC FindIntersect), spark and event 4 at the centre to the shield's spell; **with 0 the explosion stops
  completely** (+0x52). Then: on dry land a mark (fn_008251C0, the crater below); on water **three rings** (growth 5,
  7 and 10; age 0, angle 0, aspect 1, rate 1, cell 0x30, white; +0x24 = 1.0 unidentified). The targets: r =
  MaxDistance × the spell's tribal power between 1 and 5; the `ceil((r + 20) / 10)²` cells of the spiral
  (`GUtils::Spiral` 0x74D7E0) from the centre's one; from each cell, the available mobile and fixed objects **whose
  own cell is that one** (fn_00604F40) and within `Get2DRadius + r` in x/z (`GetDistanceInMetres` 0x74CD70, a
  hypotenuse). A target's world position is its MapCoords +0x14 as a point (x, z as fild × 10 / 65536; GetAltitude
  0x803090 + its height above the land +0x1C) (0x67E9E1..0x67EA15, 0x67EB8F..0x67EBCF); the target list is cleared
  with fn_00681F10(0). A map cell's objects are read fixed list (+4) first, then the mobile list (+0), each from its
  head (Explosion 0x67E65C..0x67E678, Lightning 0x690344 / 0x69094B, `EventConditionAtomNearVillagers`
  FindTypeOnMap 0x67D936 / 0x67D95F). Ring = 0. Finally five rocks `MSH_Z_SPELLROCK01` (567) at the centre ± 4 m that
  break into pieces (see "The pieces" below).
- **The crater** (0x67E35C..0x67E395, only if `MapCoords::IsDryLand` 0x67E353, that is, if the rings do not come
  out). The class is called `RootsPile` (Mac symbols: `__ct__9RootsPileFRC7LHPointffl`, `DrawAll__9RootsPileFv`). **It
  is not `TemporaryShadow`**: that is fn_00825090 (list 0xEB99FC, a dynamic shadow).
  - `new RootsPile(centre, PSysFloatRand(2π), [0x9357D4] = 8, mesh 0x251)` = fn_008251C0 → fn_00825240. The centre is
    the same point that is queried with `IsDryLand` (0x67E347 and 0x67E392 push the same `LHPoint`): the effect's
    origin, it is not lowered to the ground. What moulds it is `UpdateMelting`, which stores per vertex −(height under
    the vertex − height under the origin) / scale (0x81695B and 0x816A59..0x816A77).
  - The mesh is `MeshPack[0x251]` = 593 `TreeRootsPile`, the same mound of earth as the uprooted tree. The first
    `RootsPile` created stores it in [0xEB9A04], and the following ones use that one even if they ask for another.
    Both callers ask for 0x251.
  - `LH3DObject::Create(1)` 0x80B4D0: a **morphable** object (vtable 0x9A2E34). With `UpdateMelting` (vt 0x1E8, once)
    it moulds itself to the terrain: on a slope the hole bends with it.
  - `SetPosition` 0x423140 (vt 0x20): the origin at the centre, rotated by the angle around Y, scale 8 on all three
    axes. vt 0x58(1) sets flag 0x20 only with Light detail [0xC38224] (fn_008168C0); vt 0x40(0) clears 0x10
    (fn_007F97A0).
  - Life +8 = 0x3A98 = **15000 ms**, and a `SmokyStuff::Create(centre, 1, 1.0, −1)` (0x8252EB). It is the brown dust
    of mode 1: speed of 1.5 × size (0x823DA7), 0x68503D, gone in 1.5 s. Its six draws per puff (three offsets, the
    angle, two direction parts; 0x823D39..0x823D80) are `Random` 0x81D180 = the C runtime's `_rand` (0x81D181), not
    the game's synced or local stream; the rubble's angle before it is `PSysFloatRand` (0x67E38A), the effect's
    stream.
  - **`RootsPile::DrawAll` fn_00825350**, each frame from fn_005E5CD0 (0x5E6197, just before the smoke fn_00824140),
    with `g_game_time_inc` in ms. With ≤ 1000 ms the colour's alpha is `ftol(ms × 0.255)` ([0x9A2BA8]) and
    `SetGlobalAlpha(1)` (vt 0x48, flag 0x80, fn_007F9D60) is called; afterwards ms −= g_game_time_inc. With ≤ 0,
    fn_00825300 takes it out of the list and deletes the object; otherwise, it is drawn as a normal object
    (`AddDrawing` 0x815A70, land light fn_00801C90). It does not sink or change scale: it lasts 14 s whole and fades in
    the last second.
  - `ClearAllStuff` 0x82AEFD deletes them when changing map.
  - The uprooted tree's mound (fn_0074BD20 → fn_008251F0, scale `(M+0x24 + M+0x2C) × scale × 0.3`) is the same
    object, with the same mode 1 dust.
- **`Object::CanBeDestroyedBySpell`** (vt 0x778, 0x639960; answers event 7, 0x720DBD, which returns `== 1`):
  `IsEffectReceiver(NULL)` and not the flag +0x25 & 0x40, and if it is in a script (vt 0x448 with g_game +0x25005C →
  +0x45E8 and +0x45EC) only for a spell with +0x25 & 4. They say 0: `Creature` 0x47B1E0, `Field` 0x529FF0 and
  `CitadelPart` 0x4695D0 (citadel heart, parts, pen, worship site, worship totem). `GetActualObjectToEffect` of the
  citadel is CitadelHeart 0x468C30 and CitadelPart 0x469780.
- **`Object::DestroyedByBeam`** (vt 0x500, 0x63AB20) = `ToBeDeleted(0)`. `Abode::DestroyedByBeam` 0x402CB0 (all the
  Abode classes, the store, the dispenser, the totem): `ReduceLife(GetLife(0))`. A field's vt +0x5B8 (0x52A0A0)
  changes nothing; `TotemStatue::ReduceLife` 0x737C90.
- **What happens to the building in the original** (it is not a ruin or a ghost):
  - `Abode::ReduceLife` 0x405D90 → `MultiMapFixed::ReduceLife` 0x52F5E0. A built building (`IsBuilt`, vt 0x890) uses
    `Object::ReduceLife`, so the life stays at 0. One half built loses percentage built (fn_0052EDD0).
  - With life < 1, each inhabitant (+0xA0, next +0xE4) does `SetStateWhenTappedOnAbode` 0x752B80. If it crosses
    `GetPercentRepairedForNonFunctional` (vt 0x894): `StopBeingFunctional` (vt 0x918) and, if
    `CausesTownEmergencyIfDamaged`, `Town::SetInStateOfEmergency`.
  - With a town and without a building site (+0x74), `Town::AddBuildingSite` 0x73B8E0. With a site, +0x640 = 1.1 ×
    life − 0.1.
  - Drawing: with the site, `IsDrawBuilding` 0x52F0C0 = 1. Without `DestructionMesh` (+0x90, the rocks' one),
    `GetPercentRepairedFromWhenDamaged` 0x52F010 gives 0.98 × life (0x52F09C) and `GetPercentForDrawBuilding`
    0x52EFD0 = min(built, that) = 0.
  - `Abode::Draw` 0x515F70 → `MultiMapFixed::Draw` 0x518090 → `DrawBuilding` 0x517F90: at 0 it draws nothing
    (0x517FEF), only the fire if it is burning. **The building disappears from view** and remains as a building site
    until it is repaired. Without a town there is no site and it is still drawn whole.

#### The visible beam (SF_BeamExplosionFX, PT 138)

- `UR_MoveAtom::ModifyAtomCore` 0x6A5E50 (DefineProperties 0x6AE240: +0x20 StartTime, +0x24 StopTime, +0x28
  MoveSmoothly, +0x2C..+0x34 Start, +0x38..+0x40 Stop): between StartTime and StopTime (inclusive) t = (age − start)
  / (end − start), 1 on the step that reaches the end, `t²(3 − 2t)` with MoveSmoothly; the local position = start +
  (end − start) t. The column `MSH_S_BLAST_CENTRE` goes down from 120 m to the ground in 0.4 s.
- `UR_ChangeScaleXYZ::ModifyAtomCore` 0x6A5240 (DefineProperties 0x6ADE60): ruleScale = the interpolated XZ, stretch
  = Y / XZ (0 if XZ ≤ 0.0001, [0x8BF518]); after StopTime, only the first step writes the final values. The four
  cones `MSH_S_BLAST_CONE` open from 0 to 12 in XZ with Y 8.
- The column carries `FaceCamera`, the light map `S_BeamSingleLightMap.raw`, the sound `LASERBEAM_1` and the
  `AddSoundToAtom` of `LASERBEAM_EXPLODE`, and a camera shake (200 m, 0.7 s, up and down, 0.35 s in;
  [the particles](../physics/particles.md), "The sound rule's camera shake").

#### The other missing classes

- `UR_KPStretchHeight` 0x6A50C0 and `UR_KPMoveAtoms` 0x6A60B0 (the mushroom of the powered-up heal,
  SF_HealChakraPU): `KPSplineInterpolator` curves. The property setter (0x6AE170) copies the pairs (t, value) and
  fn_005B3760 computes the second derivatives (the `spline()` of Numerical Recipes) with **slope 0 at the ends**,
  because each rule's ctor sets the array flag (+8 = 1; without it 1e30, natural); `EvalAtT` 0x6A7EB0 is `splint`
  (bisection, h²/6), without clipping outside the keys. StretchHeight: stretch = curve(atom age) between StartTime
  and StopTime (ctor 0 and 5). MoveAtoms: position = `GetCurrentParentPos` + (0, curve(age), 0), × index / (n − 1)
  with `MovePropAtomIndex` (index 0 is the newest atom, the head of the original's list).
  - The float steps (each a float rounding at the game's 24-bit FPU): fn_005B3760 (0x5B37C4..0x5B39A0) multiplies by
    reciprocals: u0 = ((dy0 x (1/h0)) - yp1) x (3/h0); y2[i] = 1 / ((1 / (sig - 1)) x p); u[i] = (dy_i x (1/h_i)) -
    (dy_{i-1} x (1/h_{i-1})), then ((6/D) x u[i]) - sig x u[i-1], then (1/p) x u[i]; un = (ypn - dy x (1/h)) x (3/h);
    y2[n-1] = (un - qn x u[n-2]) x (1 / (qn x y2[n-2] + 1)); back substitution y2[k+1] x y2[k] + u[k]. `EvalAtT`
    (0x6A7F19..0x6A7F5D, and the inline copy in `UR_KPStretchHeight` 0x6A516D..0x6A51B4) reads ((S x ((h x h) x 1/6))
    + a y_lo) + b y_hi, S = (b^3 - b) y2_hi + (a^3 - a) y2_lo. When h is 0 or not a number (fcom, C3) `EvalAtT` writes
    nothing; the inline copy then stores what its result slot holds, the bits of the atom pointer argument (0x6A51BB),
    into the stretch. The vector spline of the beams (fn_00677490) builds the same way per component but reads in
    another order (see the particles page, the simple beam).
  - `UR_KPStretchHeight` ctor 0x6A4F50: StartTime 0, StopTime 5, keys (0, 1), (5, 1); +0x20 the keys, +0x2C
    StartTime, +0x30 StopTime; the step that passes StopTime evaluates at StopTime; it writes the atom's stretch
    (+0x7C).
  - `UR_KPMoveAtoms` ctor 0x6A5F40: StartTime 0, StopTime 5, keys (0, 0), (5, 0), MovePropAtomIndex 0; +0x20
    StartTime, +0x24 StopTime, +0x28 KeyPointsY, +0x34 MovePropAtomIndex; the window is each atom's own age.
  - The key point array setter drops an odd last value (count / 2 × 2).
- `UR_OrientSpriteWithVelocity` 0x69A790 (the flames of SF_FireBallInHand): the first time v = the velocity and k =
  −10 ln(1 − SmoothFactor); each step v += (velocity − v)(1 − e^(−k dt)); u = −v + (0, ProportionDefault, 0) in the
  camera frame and `SetAngleY(atan2(−u_y, u_x) + π/2)` 0x674360.
- `UR_ForestPath` 0x6A3770: with the collection's age, r = RadiusSpline and h = HeightSpline (the same KP curves); per
  atom three random angles (2π) the first time, θ = fmod(age · ThetaSpeed + θ0, 2π), φ likewise with PhiSpeed; p =
  SphereRadius · r (· ScaleSphereRadius) · (cos θ cos φ SX, sin φ SY, sin θ cos φ SZ), + GetCurrentParentPos outside a
  hierarchy, and + h in y; the velocity = movement / dt.
  - Ctor 0x6A35D0: ThetaSpeed +0x20, PhiSpeed +0x24, SphereRadius +0x28 and ScaleX/Y/Z +0x30..+0x38 all 1,
    ScaleSphereRadius +0x2C NULL, RadiusSpline +0x3C and HeightSpline +0x48 with their own default keys (four per
    curve, 0x6A363B..0x6A3715: every spell file gives both) and zero end slopes.
- The rules that derive a velocity from a move multiply by [0xD4E0F0] = 1 / the step (UR_ForestPath,
  UR_FollowLocalHand, UR_HandSprinkle, the shield sparks, UR_Tornado).
- `UR_FollowCastPosn` for this computer's interface also clears the collection's interpolation flag (+0x38 bit 1).
- `ParticleGoodEvilCreator` 0x6AAA00 (DefineProperties 0x6B40C0, ctor 0x6AA990: `AlignmentSwitch` −0.5): the evil
  creator if the effect has a player with alignment (GPlayer +0x60 → +0x08) < AlignmentSwitch, otherwise the good one.
  The butterflies move with `UR_Flocking` ([the flocks' particles](flocks.md#the-particles)).

#### The pieces

EXPLODE_OBJECT. **Faithful.**

- **Who asks for them.** `fn_00681260(object, origin, velocity, 6.0f, 0)` (0x67EC86; also `GScript::DeleteObject`
  0x6F93EF / 0x6F9460, which are not used here) takes the world matrix (vt 0x63C) and the 3D object (+0x40);
  `fn_006812B0(3D object, matrix, origin, velocity, 6.0f, flag)` takes its mesh (vt 0xF8; without a mesh it does
  nothing) and puts {mesh, matrix (LHMatrix, 0x30), origin, velocity, 6.0} (0x48 bytes) at the end of the queue
  0xD4E320, or of 0xD4E308 with flag ≠ 0. The 6.0 (a float, not an integer) is read by nobody. The four callers pass
  flag 0: the queue 0xD4E308 and `UR_ExplodeObject2` (0x681560 → fn_0067FFB0, RandomFactor +0x2C 0.3, MaxDepth +0x30
  4) never do anything. `LH3DMesh::MeshPack` is 0xE9FE34 (AllMeshes.g3d).
- **The five rocks** (0x67E79E..0x67E88E, at the end of `InitCollection`): `LH3DObject::Create(0)` with
  `MeshPack[0x237]` (0 if the pack is shorter); `SetPosition` 0x423140 at centre + (rand(−4, 4), 0, rand(−4, 4)): the
  **z** rand first (0x67E7E3), then the x one; scale `PSysFloatRand(0.8, 1.2)` ([0x8C4A04] / [0x8C6C98] × [0x9357DC]
  = 1) and then the angle `PSysFloatRand(2π)`; matrix rows X = (cos, 0, sin) s, Y = (0, s, 0), Z = (−sin, 0, cos) s;
  `fn_006812B0(rock, its matrix, centre − 5 m (fn_0067E8C0), BlastSpeed, 6.0, 0)` and the rock is deleted (vt 4): of
  the rocks only the pieces exist.
- **Who empties the queue.** `PSysGlobal::InitializeOneTimeOnly` 0x68F750 only creates the singleton
  `PSysUtilityPSys` (0xD4E0E8). The effect is made by `fn_006718E0` when +0x10 is NULL: `PSysInterface::Create(NULL,
  23, (0, 0, 0), (0, 0, 0), 1.0, 0)`, from fn_006717F0, which `PSysGlobal::GameLoopEnd` 0x68F5B0 → fn_006721B0 calls
  **once per turn** (step 11 of `GGame::ProcessTurn`, after the spells, 0x54E688): it processes it (vt 0x100) and, if
  it returns 5 (finished: `MaxSpellAge` 25 of `SF_ExplodeObject`), deletes it (fn_006718C0) and on the next turn
  another is made. `OnClearMap` 0x68F820 empties the two queues (fn_006811D0, fn_00681200).
- **`UR_ExplodeObject`** (vtable 0x938A98; ctor 0x6BECA0: +0x10 |= 6, a creating rule; DefineProperties 0x6B0C70:
  `RandomFactor` +0x30 = 0.3 and `MaxTrigsPerFrag` +0x2C = 15; the file sets RandomFactor 0.889381 and does not give
  MaxTrigsPerFrag). `ModifyAtomCollection` 0x6814E0 takes the entries out **from the last one** and calls
  `ExplodeMesh(collection, NextGroups +0x20, mesh, matrix, MaxTrigsPerFrag, origin, velocity)`.
- **`ExplodeMesh` 0x6807B0** (they are not vertices but **edges**):
  - Only the submeshes with the flag 0x20000000 (bit 0 of the LOD mask, bits 29..31) and **without the state bits**
    0x3F0 (bits 4..9: scaffolding, gravestones). Each primitive separately.
  - The three edges of each triangle (fn_0067DF00: (t0, t1), (t1, t2), (t2, t0)); fn_0067DFD0 first stores the end
    with the smaller (z + y) + x (on a tie, the second) and the length² ((dz² + dy²) + dx²).
  - `qsort` (0x7C7E64, the one from the MSVC 6 CRT) by length with a comparator that **never gives 0** (0x682550: 1
    if a ≥ b); of each run of consecutive equal edges (the six floats, fn_00460640) one is kept.
  - For each unique edge, the list of triangles that have it (search fn_00682580: `bsearch` with 0x682770, and if it
    fails a full walk for the first equal one).
  - The pieces: from the first unused triangle, a **path**: the triangle goes in (fn_0057D630), and the next one is
    the first unused triangle that shares one of its edges (corners 0, 1, 2; the list in its order); it continues
    while it finds one and the piece has ≤ MaxTrigsPerFrag, so a piece has **up to 16** triangles. When the last one
    has no free neighbour the piece ends. The next one starts at the first free triangle from the current one.
  - Each piece is an atom (`AtomCore::Create` + fn_00674DD0 with NextGroups) with a `RenderParticleGJMesh` (ctor
    0x6C8A90, +0x21 = 1) whose `GJMesh` (0x94 bytes, ctor 0x67FF20) has 3 new vertices per triangle (position by the
    matrix, uv and the **normal as is, unrotated**) and the primitive's material (+0 = the primitive).
  - Position +0x80 = the centroid (sum × 1/n) in world space, vertices minus the centroid, rotation the identity.
  - A piece's GJMesh layout: +8 the positions about the centroid (0x680E5A..0x680E89), +0x44 the UVs, +0x80 the
    normals, +0x6C the triangles b, b + 1, b + 2; +0x24 differs from the vertex count (no colours of its own). +0x22
    (vertices stuck to the land) stays 0 for the pieces (ctor fn_006C8A90, 0x6C8AA5).
  - Velocity +0x34 (0x680F34..0x6810AC): d = centroid − origin scaled to `velocity`; r = `PSysRandR3` scaled to |d| ×
    RandomFactor; v = (d.x + r.x, d.y + **r.y × 0.5**, d.z + r.z): the 0.5 ([0x8AA3B4]) only goes on Y.
- **Afterwards**, the file's rules: gravity 3.345 with floor, spin (`AppearanceRuleTumble`), alpha 255→0 from 0 to
  3 s, scale 1→0 from 1 to 5 s, and gone at 6 s.
- **The drawing, `RenderParticleGJMesh::DrawAt` 0x67C150**: colour = that of the `DrawData` × the land light under
  the drawn position (+0x21; fn_00801C90, byte by byte, alpha included; fn_00801C90 gives two outputs: in its 2nd
  argument the interpolated [brightness] table, 0x8020F0, and in the 3rd the cells' own colours, dword +0 |
  0xFF000000, interpolated the same way, 0x801F81; DrawAt 0x67C184 only uses the first, the other goes to [ebp −
  0x40] and nobody reads it; the 255 × 255 >> 8 leaves the opaque alpha at 254), the same for all vertices (the GJMesh
  has no colours: +0x24 ≠ vertices); then the model light ([0xC029C0] = 1): the light [0xEA9E90] by the inverse of
  the drawn matrix, normalised, I = fistp(255 n·l), f = I < 0 ? 90 : 90 + ((255 − 90) I >> 8), RGB × f >> 8; the
  second light [0xD4EC08] is 0. The light normalisation sums (x² + z²) + y² (0x67C5AE..0x67C5BF). With the
  `DrawData` alpha ≠ 255 (byte +0xB, before the land light) it is drawn with the alpha mode table (0xC387C8,
  0x67C9B7..0x67C9C0; back to 0xC38728 at 0x67C9F7). World matrix identity (0x67C8ED..0x67C979: the vertices are
  already in world space) and `Draw3DWorldTriangle` 0x81C090 with the primitive's material (GJMesh +0,
  0x67C9DD..0x67C9F2): **without haze or specular** (0x81C2BF), the material's culling (+5 bit 0, 0x81C30C /
  0x81C556..0x81C58F) and **one** `DrawTriangle` per primitive, **immediately**: it does not look at [0xC0215D], there
  is no Z object. EXPLODE_OBJECT is Sorted: it is drawn by fn_006718A0 with `Draw_(1)` from `PSysGlobal::DrawLoop`
  0x68F60C, after `GGame::Draw` 0x54E00A and before the queue flush (`FinishFrame` 0x82F460). The vertices are put
  into world space in the order of sums of 0x67C29B..0x67C303. The land light's integer interpolation is
  0x801CB8..0x8020F7: cells ftol(x × 0.1), weights ftol(frac × 256), per channel a + ((b − a) w >> 8), off the map or
  without a block table[255]; on the last row / column of the map it reads cell 17 of the block (+0x88 / +0x90). The
  ambient is [0xC39264]. Draw3DWorldTriangle clips in software (0x81C28B..0x81C2A0) and discards triangles of area ≤ 0
  on the CPU (0x81C4C6..0x81C510). [0xEA9EB4] is `LH3DTech::g_b_P3`, written only by
  `LH3DP3::Init` 0x7AC9ED from `LH3DP3::IsPentium4` [0xE83A30], called once from `LH3DRender::Open` 0x82B52C.
  `LH3DP3::CheckCPU` 0x7ACA00 sets it only when `__intel_cpu_indicator` [0xFAC840] has a bit ≥ 0x200, which
  `__intel_cpu_indicator_init` 0x8A2610 gives only for CPUID base family 0xF with SSE2 and OS FXSR (0x8A266B,
  0x8A2683, 0x8A2694). On such a CPU (Pentium 4, and AMD K8 and later) `LH3DIsland::GetColorAndSpecular` 0x801C90 calls
  `GetColorAndSpecularWrapper` fn_007ACD90 → the SSE `LH3DP3::GetColorAndSpecular` 0x7A3EC0 instead of its x87 body
  at 0x801CB8. On family 6 (every Intel Core, the measuring machine) the indicator is 0x80 (0x8A26D5), the P3 branch
  0x7ACAE0 runs and g_b_P3 = 0 (oracle on 0x7AC9E0: 0x80, 0, 0), so it is the x87 code described here. g_b_P3 has 13
  readers (ClipTriangle, `LH3DSprite::Draw`, the shadows, the land blocks' polygon lists); both CheckCPU branches set
  MXCSR flush-to-zero (0x7ACA8C / 0x7ACAEC). ALPHAREF of modes 9 / 15
  with the table 0xC387C8 is scaled with the object's diffuse [0xC37D8C], which DrawAt does not write (it is that of
  the last LH3DObject drawn).

## openblack

Status: **done** for the explosion, the beam, the crater, the pieces and the missing classes; the destroyed
building's building site is missing. Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Files**: `Particles/Rules/{Explosion,KeyPoints,Orient,Forest}.cpp`, `Particles/Rules/ExplodeObject.cpp`; the spell
  is `SpellGeneral.cpp`. The `UR_Explosion` ctor defaults are in the port since the merge with water.
- **`SpreadingDiskEmitter`** (fixed, faithful): it was registered as a normal `DiskEmitter`, which reads `Radius` (0):
  all the explosions of Many and Loads fell on the same point.
- **Spot visuals in turns**: the port passes them through `manager::StartSpotVisual` in turns. Before, it passed
  seconds and `CreateSpotVisual` converted them again: with a turn other than 100 ms the count did not come out the
  same.
- **The water rings**: a single implementation, `psys::water_rings::AddExplosionRings` of `Particles/PSysWaterRings`.
- **The crater** is `ecs::ground_marks` (`src/ECS/GroundMarks`; see
  [meshes stuck to the ground](../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph)): an entity
  `Transform` + `Mesh` + `MorphWithTerrain` (once, `Melting::Snapshot`) (+ `Alpha` in the last second), with the mode
  1 `DisappearSmoke` (was `SmokyStuff`); `MorphWithTerrain` does what `UpdateMelting` does. The uprooted tree's mound
  uses the same `ground_marks::Create`. It casts no static shadow (`CastsStaticShadow` requires `Fixed` /
  `MobileStatic` / …, and the entity is not one), as in the original. The alpha of the last second goes as `Alpha`
  (the renderer's translucent pass), which is what `SetGlobalAlpha` does. **Approximate**: the land light of
  fn_00801C90 on the mound's colour is done by the normal mesh lighting.
- **What the beam destroys**: trees and dead trees with `DeleteTree`, animals with `animal_ai::Remove`, the rest like
  the generic `DestroyedByEffect`. **Approximate**: the villagers go with `life::Kill` (`Villager::ToBeDeleted` is not
  ported); `TotemStatue::ReduceLife` and a dispenser without an Abode take Object's path. **Inferred**: openblack does
  not carry flag 0x40 nor the script objects for `CanBeDestroyedBySpell`: they are taken as 0. openblack's towns have
  no building sites (`Town::AddBuildingSite`, the repair), so the life drops to 0 and the building is still drawn
  whole, as with fire.
- **The beam**: the light map, the sounds and the camera shake are ported.
- **The other classes**: `key_points::Make` / `Evaluate` take the float steps; `UR_OrientSpriteWithVelocity` in
  `Particles/Rules/Orient.cpp` (🔍 inferred: the matrix 0xEA1D28 is taken as the world → camera rotation, x = u ·
  right, y = u · up); `UR_ForestPath` in `Particles/Rules/Forest.cpp` (its default keys are not ported: every spell
  file gives both); `ParticleGoodEvilCreator` with `Creator::Resolve` (PSys.h).
- **The pieces**: the matrix is that of the `Transform` (position, rotation × scale) and the mesh that of the `Mesh`
  component read from `AllMeshes.g3d` (`explode_object::PackMesh`): a mesh that is not from the pack (e.g. an already
  broken building, `BuildingDamage`) does not break (it is written in the trace). `explode_object::GameLoopEnd` runs
  from `magic::ProcessSpellParticlesEndOfLoop` and `Clear` from `magic::OnLoadMap`. fn_0067FFB0 is registered, not
  ported.
- **The pieces' drawing** ✅ faithful: the pieces are `Creator::Kind::MeshPiece` (was `GJMesh`; outside `mesh_atoms`,
  `psysAtoms` and any Z queue). Each frame, when drawing, `psys::mesh_pieces::Build` does on the CPU what DrawAt does:
  the PSR of fn_00679920, each vertex to world space in the original's order of sums, the `DrawData` colour × the land
  light (`LitColour`, the integer interpolation of fn_00801C90), the per-vertex model light with
  `model_light::LightInMeshSpace` / `Intensity` / `Apply` and the table 0xC387C8 if the alpha ≠ 0xFF. It gathers them
  in batches per source primitive (`graphics::world_triangles::Frame`, `Graphics/WorldTriangles.{h,cpp}`, common
  helper) and `world_triangles::Submit` uploads it into **a single transient vertex buffer** per frame and draws one
  batch per primitive with the `WorldTriangles` program (`vs_world_triangles` + `fs_object`: the alpha test and the
  alpha of the models' stage 0, `render_modes::PrimitiveAlpha` / `PrimitiveState` with the material's culling), in
  `RenderPass::Main` after the models and before the queue (`Renderer.cpp::DrawPass`). There is no bgfx mesh per piece
  nor a GPU buffer cap: 6000 pieces alive at once in BEAM_EXPLOSION_PU2 without hanging.
  - **Approximate**: the CPU land light does not carry the cloud-shadow cap that the port applies on the GPU to the
    other objects; it follows the x87 route the family-6 machines take ([0xEA9EB4] = 0). The floating-point operations are 32-bit (the x87 is not
    imitated). On the last row / column of the map the port takes the last cell. The light normalisation adds x² + y²
    + z² (last bit of the float). ALPHAREF is scaled with the `DrawData` alpha. The software clipping and the
    discarding of triangles of area ≤ 0 stay on the GPU (CULLMODE, same result except for degenerate ones). All the
    Sorted pieces go together at that point of `Main`, not interleaved with the other DrawLoop effects.
  - **Inferred**: no fog (Draw3DWorldTriangle does not call fn_007FEB30); no second colour set on the pieces (+0x30 /
    +0x38, fn_0057D630 not fully read), so never the 0x67CAEE branch; texture sampling with or without repetition like
    the models ([0xECA614] g_b_need_tilling not checked in DrawLoop); there are no pieces in the reflection (DrawLoop
    runs once per frame).
  - openblack's guard: if the transient vertex buffer (32 MB, `init.limits.transientVbSize` in `Renderer.cpp`) is not
    enough, the last batches are not drawn that frame (single warning `world_triangles: ...`).
  - The Queued / Immediate paths (pieces of an effect with a single Z object or of the hand): `Renderer.cpp` builds
    once per frame a `world_triangles::Frame` with `mesh_pieces::Build(Queued)` and `mesh_pieces::Build(Immediate)`
    and `drawOrderedEffect` (branch `Kind::MeshPiece`) draws each atom in its place in the order of fn_006798B0 with
    `world_triangles::Submit(..., atom)` (DrawAt 0x67C150 does not read [0xC0215D]). Today no effect in the data uses
    them: EXPLODE_OBJECT is Sorted (🔍 inferred: not all the effect files were walked).
  - The texture of a primitive is given by `world_triangles::PrimitiveTexture(mesh, skinId)` (formerly `GetTexture`
    of `Renderer.cpp`): skins of the mesh or of its SetSkinSource, the texture manager, the skin 0x1001 of the extra
    packs and the error only once; `DrawSubMesh` and `DrawStaticShadowPass` also use it.

### Test hooks

- `OPENBLACK_TEST_SPELL="BEAM_EXPLOSION,x,z"` casts the explosion (`EXPLOSION_ONE_PU_ONE` / `_PU_TWO` for Many /
  Loads); `OPENBLACK_TEST_EXPLOSION_SHOT="<turns>,<path.png>[;...]"` requests screenshots that many turns after the
  first explosion starts ([debug environment variables](../project/debugging.md#debug-environment-variables)).
- `OPENBLACK_SPELL_TRACE=1`: `Explosion: started ...` with its targets (distance, radius, class), `Explosion: ground
  mark ...` with its angle, each destroyed object (ring, exploded, deleted), the queued rocks and each broken mesh
  (`ExplodeObject: mesh ... in N pieces, from ... at ...`).
- `test_explosion`: ChangeScaleXYZ, MoveAtom, the cadence of events 2 and the closing of a file such as Single, the
  player tint, FaceCamera, the KP curves, the good / evil creator and the mode 1 `DisappearSmoke` dust at 1.5 × size
  (the mark's life and alpha are tested by `ground_marks`); the pieces: the path along edges (16 + 4 in a strip of 20,
  loose pieces, MaxTrigsPerFrag 0), the velocity (the random Y halved) and the queue emptied into atoms at the
  centroid (only LOD 0 without state).

## Pending

- **`UR_ExplodeObject2`** → fn_0067FFB0 is not ported (nobody calls it: the queue 0xD4E308 stays empty).
- **`manager::SortedFrame`** has no row of its own for the pieces (`Kind::MeshPiece` falls into `others`,
  `PSysManager.cpp`); the flushing by draw paths will have to call `mesh_pieces::BuildAtom` for a Queued /
  Immediate.
- **The villager's death** with `life::Kill` ⚠️ approximate and the flag +0x25 & 0x40 and the script objects
  🔍 inferred: see openblack.
- **The "destroyed" building**: the town's building site is missing (`Town::AddBuildingSite` 0x73B8E0, the repair).
- **The citadel's `GetActualObjectToEffect`** (CitadelHeart 0x468C30, CitadelPart 0x469780) is not ported.
- **The crater's two drawing flags** of the `LH3DObject` have no equivalent: 0x20 of the Light detail (fn_008168C0)
  and 0x10 cleared (fn_007F97A0).
- **The water rings' +0x24 = 1.0** is unidentified: verify with `xref.py readers` on the ring record's +0x24.
