# Food, wood, water and heal

## Summary

The resource miracles pour grain or logs from the hand: FOOD, its power-up and WOOD drop a stream that lands as a
single growing pile at the point where the stream started, or tops up a store or pile already there. WATER rains a
drop a turn around the hand, putting out fires, growing trees and fields and leaving coloured ripples on land and sea.
HEAL finds the living things around the cast point and heals them one by one with a glowing chakra; it also cures
poison, which villagers catch from poisoned food they are handed.

openblack ports the four miracles, the piles, the hand's grain state, the drop's reach, the chakras and their glow,
and the poison's flag and damage faithfully. The poison tint is not drawn yet, and the store's wood diversion to its
building site is not ported. The creature casting these miracles from above is on
[the creature's casting](../creature/casting.md#casting-from-above-the-food-wood-and-water-rows). Their particle rules are on [Heal rules](../physics/particle-rules.md#heal-rules), [Key-point rules](../physics/particle-rules.md#key-point-rules) (the heal power-up's mesh) and [The sprinkle rule](../physics/particle-rules.md#the-sprinkle-rule).

## The original

### Food and wood

What follows has been read in the exe except where marked.

- **SpellResource** (MAGIC_TYPE 14 FOOD, 15 FOOD_PU1, 21 WOOD; `GMagicResourceInfo::AllocSpell` 0x5FAC20, +0xEC
  "first event done" set to 0 in fn_00724C80; no Process of its own). `SpellResource::GetMagicInfo` 0x724C70 (the
  runtime record has its fields at file + 0x14).
  - `SpellEvent` 0x724D80: first the default event (EffectValues of FOOD/WOOD: only alignment 1, reaction 21); then,
    except for event 1 or if it is closed, n = `resourceAmountFirstEvent` the first time and `resourceAmountPerEvent`
    afterwards; `PayFor(costPerUnit × n)` (the result is not checked); off the map, on water (`IsDryLand` 0x603620:
    altitude byte ≥ 4) or with strength 0 it pays and nothing falls; otherwise, `Pot::AddResourceToPos(pos, IS, type,
    ftol(tribalPower × n), 0, x)`. The IS that `fn_00724CE0` / `fn_00724D30` pass is the leader interface of the
    spell's player; only a human player has one.
  - Food: 200 the first time (1400 chants), then 18 (126); FOOD_PU1 20; wood 500 (1500), then 20 (60).
  - `HasEnoughChantsAndLifeForRecast` 0x724C90 = costPerUnit × first ≤ chants.
  - **The `poisoned` field of FOOD_PU1 does not poison**: fn_00724CE0 pushes `(pos, IS, FOOD, n, 0, poisoned == 1)`
    and in `AddResourceToPos` the 5th argument ([esp+0x60]) goes to `SetPoisoned` (vt 0x69C, with `IsPoisoned` vt
    0x4A4) and the 6th ([esp+0x64]) to `SetSpeedUp` (vt 0x864, with vt 0x4A8). PU food makes "speed-up" piles
    (sparkles PILEFOOD_SPEEDUP, 46).
- **`Pot::AddResourceToPos` 0x66F270** (used by the hand when dropping a pot and by the grains):
  - off the map, 0; walks 9 cells of 10 m with `GUtils::Spiral` 0x74D7E0 (the one at pos and its 8 neighbours), in
    each the fixed list (+4) and then the mobile one (+0), while something remains;
  - accepts an object if `IsResourceStore(type)` (vt 0x680) or `IsPot` (vt 0x4B4) and `GetResourceType` (vt 0x690) ==
    type, and `IsCloseToEqual(pos, GetDefaultFireCentrePos, Get2DRadius × GetRadiusMultiplierForApplyingPotToPos)`
    (pot 2, store/object 1.2); the first one that qualifies takes what it accepts (`AddResource(type, rest, IS,
    poisoned, &pos, 0)`, vt 0x9C), not the nearest;
  - whatever is left over, if pos is not water, makes a new pile (fn_005FA8B0) with the pile sound fn_0066D1A0 (<
    200: `77 + t % 6` / `92 + t % 6`; otherwise `75 + (t & 1)` / `86 + t % 6`), `SetPoisoned` and `SetSpeedUp`.
  - **The guidance voice on dropping (fully read).** 0x66F4D8..0x66F509: only if the IS is
    `GGame::MyInterfaceStatus` 0x555880, with the point and a `RESOURCE_RAIN_TYPE` = 1 for food (type 0) and 2 for
    wood (type 1), 0 for everything else (0x66F4EB..0x66F502). `GGuidance::ResourceDropSFX` 0x71B570 requires:
    `GGuidance::PlayNow(1)` 0x71AF50 on the IS's guidance (+0x30) non-zero; a town less than 100 m away ([0x98013C],
    `MapCoords::GetNearestTown` 0x6020E0); and `GGuidance::GetResourceDropSample` 0x71B5F0, which adds three floats of
    that town (food +0xC4 + +0x108 + +0x19C; wood +0xC8 + +0x10C + +0x1A0; type 3, the one from
    `DoDeleteObjectAndTakeResource` 0x63A9E6, uses +0xEC + +0x130 + +0x1C4) and at 0.5 or more ([0x980140]) picks with
    `LocalRand(3)` a HELP_TEXT id among 0x1352/0x1353/0x1354 (food) or 0x1355/0x1356/0x1357 (wood), below 0.25
    ([0x980144]; 0x71B64F `fcomp`, `test ah, 1`, `je` past it) among 0x135B/0x135C/0x135D (food; wood repeats the same
    three) and in between nothing. Then
    `GGuidance::PlaySample` 0x71C6F0 (1, the sample, player +0xB5, 1, 0x7F, 0x64, 0x5A, the point, 200 [0x980148], 1).
  - **The diversion of a store's wood.** `StoragePit::AddResource` 0x732F60, before anything else
    (0x732F67..0x732F99): if its +0x74 is not null and the type is WOOD (1) or ANY (−2), it forwards the whole call to
    the `AddResource` (vt 0x9C) of that object and returns its answer. **+0x74 is `MultiMapFixed::building_site`**, a
    `BuildingSite*` (bw1-decomp `src/Black/MultiMapFixed.h`; StoragePit inherits from Abode, whose own fields start at
    0x7C): a store under construction sends its wood to the building site.
  - **The 2D radius of the pile:** `Object::Get2DRadius` 0x638180 = scale × max(+0x24, +0x2C) of the mesh, which
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 fills with the half extent (max − min)/2 of all submeshes;
    `PileFood::Get2DRadius` 0x66F180 multiplies it by `GetProportionRaised` (vt 0x86C). In Land1: MagicFood of 200 →
    1.95 m (accepts at 3.9 m), grows with the pile (accepts at 4.14 m with 218); MagicWood of 500 → 2.04 m (accepts at
    4.07 m).
- **Piles**: MagicFood 0x5FA9F0 (PotInfo 10, MSH_S_GRAIN_PILE, scale 0.3, owner +0xBC) and MagicWood 0x600E20 (PotInfo
  9, MSH_B_WOOD_01, scale 0.7, owner +0xB4); no Process or expiry.
  - `MagicFood` = PileFood(pos, GPotInfo 0xD4D308 = info 10, amount, town, 0, 0, 1.0), `SetScale(0.3)` in
    `fn_005FAAE0`. `MagicWood` = PileResource(pos, GPotInfo 0xD4D1C4 = info 9) → PileWood, `SetScale(0.7)` in
    `fn_00600EE0`. A NULL owner becomes the neutral player (g_game +0x18 + byte g_game[0x205A5B] × 0xA60; wood
    0x600E64..0x600E8A).
  - Both piles (type 21, counted as fixed) enter their cell at once, at the tail of its fixed list (MobileObject
    0x607150+0xA9 → `Object::InsertMapObject` 0x636740).
  - `IsAWoodPileOutsideStoragePit` always answers 1 for the magic wood pile (creature AI).
  - **The shadows of the food pile.** `MagicFood::CallVirtualFunctionsForCreation` 0x5FAAB0, after the PileFood one
    0x66E1A0, calls two setters of the Game3DObject +0x40: vt 0x78(0) at 0x5FAAC8 and vt 0x80(0) at 0x5FAAD2. In the
    LH3DObject vtable 0x9A2974 they are fn_008168A0 (clears bit 0x40 of obj+4: the bit checked by the receiver of
    projected shadows, vt+0x7C fn_007F9870, test 0x80E457) and fn_007F9880 (clears bit 0x1000): **the magic food pile
    does not receive projected shadow**. bw1-decomp calls those slots `SetCastDynamicShadow` / `SetShadowOnTexture`,
    but the exe code is the receiving one. Both take the argument in edx (0x5FAAD0 `xor edx, edx`).
  - **The wood pile does keep them:** `MagicWood::CallVirtualFunctionsForCreation` 0x600F10 is just a call to the
    PileResource one 0x66E300, without those two setters.
- **SF_Food / SF_Wood effect**:
  - `UR_HandSprinkle` 0x6A0220: a source atom at the gesture position (at most 58 m above the ground); on the first
    step, if this computer's interface is casting, it starts `HandStateGrain` (fn_005B2F70). Velocity (0, Δy/dt − 20
    if a human casts, 0).
  - The grains are emitted by `UR_WillowWisp` with the spell *enabled*: max(dt × MaxAtoms / DieAge, moved / 2.5 m) per
    step, i.e. **18 per second even if the hand is still** (the plan said a still hand drops nothing: that is not
    so). Each grain that touches land (`LandscapeCollide SendEvent 1`) is a `SpellEvent 3`.
  - `AppearanceRuleTumble` 0x6A6200 (logs): the angle `sqrt(vz² + vy² + vx²) × TumbleSpeed` (+0x24), kept within
    ±MaxTumbleSpeed (+0x28) with RestrictMaxRotation (+0x20), × the step; every row's (x, y) turns about z when
    |vz| < |vx| (0x6A6270..0x6A628D), else its (y, z) about x. openblack: `sprinkle::TumbleAngle`, `TumblesAboutZ`.
  - `AppearanceRuleTumble` and `ParticleMeshCreator` 0x6A8B00 / `…AnimTextured` 0x6A8DA0 (logs
    MSH_I_OFFERING_WOOD, rain cone): mesh atoms are drawn as instances along with the objects.
- **HandStateGrain** (hand state 8): `fn_005B3000` stores (ClampHand, TotalTime, height, angle, loop) and the hand
  position (+0x1EC); `fn_005B2DA0` on each turn: t += dt / TotalTime, natural cubic spline of fn_005B3760 through (0,
  0) (0.2, 1) (0.8, 1) (1, 0) (the ctor passes 1e30 at both ends because +0x148 = 1, 0x5B2CF2; the peak is 1.61);
  height = v × 10 m, tilt = v × 1.07 rad (peak at t = 2 s: 16.1 m and 1.73 rad). With ClampHand (food and wood)
  `HandStateHolding::Update` 0x5B3FD4 (vt 0x1C) sets the required hand position at the point where it started, and
  adds the height (vt 0x18). `Spell::CoreCloseDown` 0x720160 stops it.
  - **The direction of the tilt rotation (verified; faithful).** The angle comes from the file and is **positive**:
    `AngleToRaise 1.07257` (SF_Food.txt line 143, SF_Wood.txt line 114), and fn_005B2DA0 uses it as is (0x5B2EF3:
    height = v × +0x134, tilt = v × +0x138). `ObtainRequiredHandPosition` 0x5B6DE0 builds the normalised hand → camera
    axis (`LH3DTech::g_camera` minus the position, 0x5B6E1B..0x5B6ECE), calls fn_007FB180(matrix, axis, angle) and
    transforms the vector (0, 1, 0). fn_007FB180 writes the **standard** Rodrigues matrix by rows (0x7FB1F7 `[ecx]` =
    x² + (1 − x²)c, 0x7FB207 `[ecx+0xC]` = xy(1 − c) + zs, 0x7FB21C `[ecx+0x18]` = xz(1 − c) − ys, …), but the
    transform at 0x5B6EE8 is by **row vector**: out.x = R00·v.x + R10·v.y + R20·v.z + t.x, that is vᵗ·M = Rᵗ·v =
    R(−angle)·v.
  - **With ClampHand the hand does not move while the grain falls**, so all grains fall on the same point and make a
    single pile that grows (the plan expected a 20 m line: there is none with `ClampHand 1`, and water, which has
    `ClampHand 0`, does follow the hand). The point is the one the hand had when the stream started, and the loop
    does not take it again.

### Water

MAGIC_TYPE 22 WATER and 23 WATER_PU1, seed 9 WATER. Status: ✅ faithful except where marked.

#### The spell

- `GMagicWaterInfo::AllocSpell` 0x5FAC70: 0xF4 bytes, vtable 0x8F553C; fn_00724EC0 sets to 0 +0xEC (age of the last
  ring) and +0xF0 (the "putting out a fire" reaction). **The vtable only overrides `Process`** (vt 0x528); the rest is
  the normal `Spell` (InitWithPos, SpellEvent, CloseDown...).
- Getters (`info+0x10 − 0x16`): rain radius fn_005FACE0 = **6** (WATER) / **12** (PU) / 1; ring growth fn_005FACC0 =
  **2** / **4** / 1; `GetRippleEvery` 0x5FAD00 = **0.1**.
- Constants: rain radius 6 / 12 at 0x92BFA8 / 0x92BFAC; ring growth 2 / 4 at 0x92BFA0 / 0x92BFA4; the reach factor
  2.5 at 0x8C581C (0x72510E).
- `SpellWater::Process` 0x724ED0, **one drop per turn** (checked instruction by instruction):
  1. `Spell::Process` 0x720710 and its result, which is always returned; if the reaction +0xF0 is no longer
     available, to 0; if the spell is closed, nothing more.
  2. `r = GameFloatRand(R) × 0.7 + 0.3` (0.3 … 0.7·R + 0.3 m: 4.5 m with WATER, 8.7 m with PU) and `a =
     GameFloatRand(2π)`; the drop P = cast position (+0xCC, the one that follows the hand in a cast with the hand) +
     (r cos a, r sin a), with `y = GetAltitude(P) + 0.2`.
  3. `SpellEvent{2, P, movement 0, strength 1, 0, no target}` (vt 0x52C → `ApplyDefaultSpellEffect`): the
     EffectValues of WATER (**burn −4000**, radius 1 m) × strength × tribal power, `costPerEvent` 10, reaction 21. Its
     result is not checked: the drop and the ring come out even if there are no chants left.
  4. The 9 cells of 10 m from the one of P (`GUtils::Spiral`, direction 1, count 1), each with its fixed list and
     then the mobile one (fn_00603500 / fn_007252D0): each object with `2.5 × GetPower > dist2D(obj, P) − GetRadius`
     (strict) receives `ApplyWaterSpell` (vt 0x67C). `GetPower` of a spell is that of `GameThingWithPos` 0x56FE60 = 1,
     so the reach is **2.5 m from the edge**. `GetRadius` (vt 0x60) is `Get2DRadius` (Object 0x638110 jumps to vt
     0x64): **5 m for a field** (Field 0x528E80), for the rest the half extent of the mesh × scale. The 2D distance
     goes through `hypotenuse` 0x74F680 with an inverse root on MapCoords 16.16.
  5. A ring when `0.1 < age − last` **in single precision** (fld/fsub/fstp dword; with the age adding 0.1 per turn it
     comes out on almost every turn, not on all: not on the first turn, 0.1 − 0 is not greater than 0.1). Colour
     `{0xFF80CBC5, 0xFF8599C5, 0xFFBA97B2, 0xFFB9CA86, 0xFFBD9C8A}[GameRand(5)]`, angle `GameFloatRand(2π)`, in the
     first free slot of the 1024 at 0xEAB7C8: +0 P, +0x0C flag 1, +0x10 age 0, +0x18 growth 2/4, +0x20 angle, +0x24
     1, +0x28 aspect 1, +0x2C rate 1, +0x30 cell 0x30, +0x34 colour. **The rings fall on land**, not only on water
     (the colour is one of those constants, not the light table, and the ring keeps it all its life). When the 1024
     ring slots are full, no ring is made (0x725236).
- `ApplyWaterSpell` per class (the symbol list only has these three):
  - **Object** 0x63A8E0: if the object is burning (`IsOnFire`) and +0xF0 is empty, `+0xF0 = CreateReaction(spell, 34
    REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE, the spell's player, 1)`. Returns 0.
  - **Tree** 0x74C390: the Object part and then the tree's water rule (grow, with PU go past the maximum, or a forest
    sprout every > 40 turns). With sprout and player: `GPlayer::FUN_0064DA80(0xE, 1)` (**only does something in
    multiplayer**, `IsMultiplayerGame` 0x552F80; here nothing) and `GAlignment::Update(player, new tree, 1)` 0x4145A0.
    Returns 1.
  - **Field** 0x528F30: the Object part; with player, `ConsiderMakingCreatureMimicPlayer(action 0x21, magic 0x16)`
    (deed CAST_WATER_ON_CROPS); if it is not burning: with `crops ≤ 30` (`timesToSow`) it jumps to **31** at once
    (sown); otherwise, while `growth ≤ 1200`, `growth += 2` (`effectOfWaterSpell`, info.dat, the same in the 6 rows)
    and `food += 2 × 350 / 1200`. `IsUnripe` is called and discarded.
- **Putting out fires**: done by part 3 (burn −4000 in 1 m): `ApplyEffectToFireEffectIfNecessary` takes the
  temperature towards ambient − 4000 with the cap `max(10·ΔT/capacity, ΔT)`, so **a single drop puts out** a tree
  burning at 500 (Tc 110, capacity 100). Since the event comes before `ApplyWaterSpell`, the object is no longer
  burning when the water reaches it, and reaction 34 is only born if the drop did not reach it with the 1 m radius but
  did with the 2.5 m. The storms (fn_0072DCC0) are in [the storm's spell](storm-and-tornado.md#the-spell).
- The original's mist for the `SPELL_AT_POS` is at the height of "from"; with the hand, at the hand, which
  `UR_HandSprinkle` raises 8 m in 8 s without holding it (`ClampHand 0`).

The water mist (`ParticleMistCreator`, also the one of the storm clouds) is in
[`ParticleMistCreator`](../physics/particle-creators.md#particlemistcreator).

#### Fix in `ECS/Effects`: the map cells

In the original `ApplyEffectToMapPos`, water and fire heat walk each cell's fixed list from the head and then the
mobile one, **without "already seen"** (`ApplyEffectToMapPos` 0x525274..0x5253BF, water 0x7250CC..0x725179 and fire
heat 0x72F699..0x72F6B6 with `HeatTransfer` 0x72F980 have none: a multi-cell object receives the effect once per cell).
The original links each object in the cell of its position. What openblack changed is under openblack.

### Heal

MAGIC_TYPE 10 HEAL and 11 HEAL_PU_ONE, seed 7 HEAL. The spell does not heal anything by itself: **it looks for the
targets and the PSys heals them one by one** with an event of type 5 per chakra.

#### Finding targets and casting

- `GMagicHealInfo::FindTargets` 0x5FBB00: radius `dummyVar` 10 (35 with PU) and cap `maxToHeal` 20 (100), both × the
  tribal power if there is a spell; spiral of `ceil(2R/10)²` cells; each living thing that accepts the effect and
  `CanBeHealedByHealSpell` within R becomes a target of the PSys (`AddTarget_`, vt 0x114). **It does not check whether
  it is missing life**: a healthy one also receives its chakra (checked in the game: villagers with life 1 light up).
- `SpellHeal::InitWithPos` 0x72D870 = `Spell::InitWithPos` and, **if it returns 1**, `FindTargets(pos, this)`; it does
  not check how many it found. The hand check is `GMagicHealInfo_vfunc12` 0x5FBD20 (vt 0x30): only with MAGIC_TYPE 10
  or 11 and `FindTargets(pos, NULL) > 0`.
- `SpellTargets::TakeTargetObject` 0x671030 takes out **the last one** of the list, so the PSys consumes the targets
  in the reverse order to the one it found them.
- `CanBeHealedByHealSpell` (vt 0xB18) is `!IsDead` in `Living` (0x5EE550 → vt 0xAF4, `Living::IsDead` 0x417270) and
  gives 0 for **the whole `Dove` class** (`Dove::CanBeHealedByHealSpell` 0x41EAB0 is an `xor eax,eax`), not only for
  the spell's dove: no bird is healed (crow, dove, swallow, pigeon, seagull, bat, the two spell ones and also the
  **vulture**, `class Vulture : public Dove`, whose vtable 0x8BB7F8 +0xB18 is 0x41EAB0). The creature gives 1.
  CitadelDove / CitadelBat: unidentified class.
- The power-up (SF_HealChakraPU) adds the mesh `MSH_S_HEAL_MESH` (532, the mushroom) with `UR_KPStretchHeight`
  0x6A50C0 and `UR_KPMoveAtoms` 0x6A60B0 (keypoint interpolation `KPSplineInterpolator::EvalAtT` 0x6A7EB0, a cubic
  spline with the factor 1/6; `MovePropAtomIndex` scales the displacement by `index / (NumAtoms − 1)`; see
  [the other missing classes](lightning-explosion.md#the-other-missing-classes)) and the sound HEAL_MUSHROOM. The
  chakra works the same in the PU (radius 35, up to 100 targets). Mesh 532 is type 4 with byte +5 = 5 (two-sided) and
  192 of its 512 triangles face inwards, but the original **does not change its material**:
  `ParticleMeshCreatorAnimTextured::CreateLH3DObject` 0x6A8D20 only does `LH3DObject::Create` and six calls from its
  vtable, none to `GJUtils::SetMaterialProperties` 0x57E220 (the change 4 → 13 of the physical shield).
- SF_HealChakra only uses `ParticlePointCreator` and `ParticleSpriteCreator`: **the mist is not needed for healing**.

#### `UR_HealSpellChakra`

`UR_HealSpellChakra` 0x6A0B20. Constructor 0x6A0810 (sets the modifier's flags 6 and clears 4: **it is not a creator,
but it keeps the effect alive until it closes**), properties 0x6B1E40. Each turn:

1. Takes targets out of `SpellTargets` while there are any. Those that already have a chakra (global list 0xD4ED38,
   `g_Chakraed`) or no longer exist are discarded; the rest:
   - **sends `SpellEvent{type 5, the target's centre, strength 1, target}`**, which is what really heals
     (`ApplyDefaultSpellEffect` → `ApplyEffect(EV, 0)` → `IncreaseLife`, and `SetPoisoned(0)` if it was poisoned);
   - creates a point atom with its `AtomData` (0x38 bytes, ctor fn_006A0920): the target, its `Get2DRadius` (+0x30)
     and its `GetHeight` (+0x34), and puts it at the start of the global list (fn_006A0A60);
   - the position is fn_006A0AB0: the target's point with the terrain height and, with `TakeCentrePos`, **half the
     height of the object** higher up.
2. Walks its atoms: the **first** one in the list, as soon as it has age > 0, plays `SoundHeal` (SOUND_SPELL_HEAL,
   only once per atom; `SoundSpacing` 0.2 is read and not used). The original's list grows **at the head**
   (`AtomCollection+0x40`; fn_00674BD0, the `mov [ecx+0x40], eax` at 0x674BEE, called from
   `AtomCollection::CommonInitNewAtom` 0x674C7A), so that first atom is the **newest**. Each chakra follows its target
   and, with `ScalePropObjectSize`, its rule scale (+0x78) is the target's `Get2DRadius`.
3. `fn_006A0E30` gives the chakra's intensity from the age of its **subcollection** (the burst of the next group): `t
   = age / AtomAgeMaxAlpha` up to 1 and then `1 − (age − AtomAgeMaxAlpha) / (AtomAgeZeroAlpha − AtomAgeMaxAlpha)`,
   clamped to 0..1. With it:
   - the burst atoms receive alpha `int(t × MaxAlpha)` (100.46 → 100 at the peak);
   - the target receives `SetSpecularColor(int(t × SpecularColor))` (`Living` 0x417480, +0xD0), that is the glow
     (200, 255, 255) × t: **the villager lights up** while the chakra lasts.

   When the burst runs out of atoms (after 3 s) the chakra is deleted, and when its `AtomData` is destroyed
   (fn_006A09A0) it leaves the global list and removes the glow from the target (`SetSpecularColor(0)`).
   `SetSpecularColor` stores the whole dword (0x417484) and the fade always sets the alpha to 0xFF (0x6A0EF5), so even
   if the RGB reaches 0 the dword is not 0: the draws, which test the whole dword (fn_0051B3D0 0x51B416,
   `Animal::Draw` 0x51C4D6), keep seeing an own specular (which wins over the poison tint) until the
   `SetSpecularColor(0)` of the destructor (0x6A0A28).
- A chakra whose target disappears is deleted (0x6A0D7D: in addition the atom forgets the target); if a chakra has no
  subcollection the rule is released (0x6A0E1C returns 0).
- Bit 4 of `Object+0x24` (0x6A0D8B) also ends the chakra, **without** forgetting the target, so the destructor of the
  `AtomData` removes its glow. That bit is `GameThingWithPos::Flags` 1 << 2, the `UNAVAILABLE_FOR_STATE_CHANGE` of
  `bw1-decomp/src/Black/GameThingWithPos.h` (only read inverted, in `IsAvailableForStateChange`), and the only place
  that sets it is `GInterface::PlaceObjectInMagicHand` (0x5DA7C1 → fn_005DC330 → fn_005DC2A0 → fn_005FAFC0, the `or
  byte [esi+0x24], 4` at 0x5FB014): **it is the villager picked up by the hand**.
- The specular glow is added to the specular of the terrain light, as fn_0080BF10 does from fn_0080BEC0
  (`Villager::Draw` fn_0051B3D0, `Animal::Draw` 0x51C4D6).

#### `CreateRuleFusedSphericalExplode`

`CreateRuleFusedSphericalExplode` 0x69F610. The burst of each chakra (5 sprites from `S_SpriteSheet1`, FileOffset 57,
7 frames, scale 5, additive). When the collection reaches `FuseTime` (0 in the chakra) it makes `NumAtoms` atoms at
once: direction from `PSysRandR3` (rejecting the null vector), normalised, with `|y|` if `OnlyHemisphere`; **a single
speed** for the whole burst, `MinSpeed + rand(MaxSpeed − MinSpeed)` (0.6 in the chakra), with the y × `ScaleYSpeed`.
Only the first atom plays `SoundExplode`; with `DisableParent` (1 by default) the parent atom stops being drawn (flag
0x10). Afterwards the rule is released. Constructor default values: NumAtoms 100, FuseTime 20, ScaleYSpeed 2,
DisableParent 1.

#### `UR_HealInHand`

`UR_HealInHand` 0x6A0F40. The hand wave (SF_HealChakraInHand and SF_HealChakraOnHolder, `WiggleFreq` 0.6; 1 by
default): each atom is placed at `GetCurrentParentPos × sin(collection age × WiggleFreq × 2π)`, and the odd atoms with
the opposite sign. It is literally a multiplication of the parent's position, not an offset.

#### Life and poison

- `Object::IncreaseLife` 0x637870 (`Villager::IncreaseLife` 0x753460 just calls it): if life + amount > 1, the amount
  is clipped to `1 − life`; with amount ≤ 0 it touches nothing; otherwise, `SetLife(life + amount)`. Returns the new
  life. The heal effect is `Object::GetHealEffect` 0x637D80 = `EV.heal × defenceMultiplierHeal` (there is no other
  clipping than the 1).
- With heal effect 1 (the HEAL row of info.dat), a villager at 0.3 life goes to 1 at once.

##### Poison

- Poisoned: bit 1 of `Living+0xB4` (`Living::IsPoisoned` 0x416F90 vt 0x4A4 / `SetPoisoned` 0x416FA0 vt 0x69C; the
  `Object::SetPoisoned` 0x402780 of pots is another flag). `ApplyDefaultSpellEffect` (0x720E34) removes it with HEAL
  or HEAL_PU_ONE.
- **Who poisons**: there is no spell that poisons. The two calls to `SetPoisoned(1)` on a living thing are
  `Villager::AddResource` 0x7564D0 (0x7564E5..0x7564F3: food with the "poisoned" flag, e.g. a poisoned pile put into
  their hands) and `Villager::GetResourceFrom` 0x753390 (0x7533E8..0x7533FC: taking a resource from an object whose
  `IsPoisoned` is 1, `Pot::IsPoisoned` 0x55D4E0, `StoragePit::IsPoisonedResource` 0x733550, which also passes on the
  `IsSpeedUp`). That is, **one is poisoned on receiving the food, not on eating it**. Eating only changes the
  animation: `Villager::EatFood` 0x75C00E and `EatFoodAtHome` 0x75C0BE pick 0xD4 instead of 0xA3 / 0x26 if poisoned.
- **What the poison does to it**: `Villager::CheckHungry` 0x75BCC0 applies the hunger damage also when the villager is
  **not** hungry, just for being poisoned (0x75BD83..0x75BD9E). The amount (0x75BDA6..0x75BDE0) is written `max(1 −
  food / hungryForFood, 1) × hungerToLifeMultiplier`, with the `max` read at 0x75BDBB..0x75BDCA (`fcom 1.0`, `test
  ah,0x41`, `je` keeps the value only if it is **greater** than 1): since food is never negative
  (0x75BD59..0x75BD6E clips it to 0), the first term never wins and **the damage is a fixed `hungerToLifeMultiplier`**
  per periodic check. In addition `Villager::DoSleeping` 0x760DB6 **skips the life recovery while sleeping** while
  poisoned.
- **The tint**: fn_0051B3D0 (the drawing helper of `Living::Draw` 0x51AEEC and `Villager::Draw` 0x51BA74/0x51BAD5), at
  0x51B43D..0x51B45D: if the living thing does **not** have its own specular colour (`Living+0xD0`, the chakra's, which
  wins) and is poisoned, it is drawn with diffuse 0xFFE8FFDD (0x51BB50, which the symbol file calls
  `Pot::GetPoisonSpecular` but goes in the diffuse slot of fn_0080BEC0, the same one the white 0xFFFFFFFF uses) and
  specular 0xFF001000 (0x51BB60, which the symbol file calls `Object::GetFireEffect`); fn_0080BF10 **adds** the
  specular to the object's colour channel by channel with saturation (0x80BF2D..0x80BF68). The `Pot::GetPoisonColor`
  0x80BEC0 of the symbol file is not a colour: it is the "paint with these two colours and `AddForDrawing`" of the 3D
  object.
#### More addresses from the code

- `MagicFood::MagicFood` 0x5FA9F0 and `MagicWood::MagicWood` 0x600E20 are the two piles' constructors.

## openblack

Status: **done** for food, wood, water and heal; **partial** for the poison (the tint is not drawn; the sleeping rule
is not ported). Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Food and wood**: `Magic/Spells/SpellResource`, `Magic/Objects/Magic{Food,Wood}`, `ECS/PotResource` (`Pot::
  AddResourceToPos`; the dropper is `dropper.isMyInterface`). The guidance voice is
  `audio::guidance::PlayResourceDropRemark`, called from `ECS/PotResource.cpp` for a drop by the local interface (and
  from `ECS/ObjectDelivery.cpp` for what a structure takes); the town's three values are its food, wood and rain
  desires' raw value plus their two boosts (`ECS/AudioQueries.cpp`). The store's wood diversion is not ported
  (`ECS/PotResource.cpp` still says there are no building sites): a store can now have a building site (a damaged
  one's, `building_sites::AddBuildingSite`), so the wood dropped on such a store goes to the store, not to its site.
- **The food pile's shadows**: openblack already complies, and in addition leaves it out of all three shadows:
  `CastsStaticShadow` of `RenderingSystem.cpp` rejects any Pot and its `ReceivesDynamicShadow` cites this same call
  (0x5FAAC8) for the MagicFood and HandFood types; `CastsPhysicsShadow` of `Graphics/ShadowList.cpp` also rejects any
  Pot. `IsAWoodPileOutsideStoragePit` (creature AI) is not ported.
- **The effect**: `Particles/Rules/Sprinkle.cpp`, `Particles/Creators/Mesh.cpp`. A fix to the spell file reader:
  ARRAYs can carry floats (`KeyPoints` of UR_HandSprinkle); before, the whole read stopped there and SF_Food, SF_Wood
  (and any such file) did not load.
- **HandStateGrain**: `HandGrain.cpp`; `HandPlacement.cpp` rotates with `-tilt` (`glm::rotate(mat4(1), -tilt,
  axis)`).
- **Water**: `Magic/Spells/SpellWater` (`water::SpellWaterData`), `Particles/Creators/Mist`. The rings go through
  `WaterRingSystemInterface::Add`, from `AddDropRing` in `SpellWater.cpp`. The tree's rule is
  `ecs::ApplyWaterSpell(tree, magicType == 23)` and its alignment `alignment::UpdateForTree(player, true)`; the field's
  is `ecs::ApplyWaterSpellToField` in `ECS/Fields.cpp`, with the creature's mimicry `creature_mimic::Consider`
  (`Magic/Spells/SpellWater.cpp`). The villagers reaction 34 reaches are only impressed by it, once per their
  standard wait, and the first one in a map cell ends that cell (since 2026-10-21; before, openblack's villagers
  ignored it; [reactions that only impress](../beings/villager-reactions.md#reactions-that-only-impress-34-39-and-40)).
  **Approximate**: the 2D distance uses the exact root (sub-millimetre differences).
  **Inferred**: the test hook puts the `SPELL_AT_POS` mist 30 m above the target.
- **The map cells** (replaced): `ApplyEffectToMapPos`, water, fire, lightning, explosion, storm, healing and pots now
  walk the ordered lists of `ecs::map_cells` ([object lists per cell](../engine/object-size-and-map-cells.md#object-lists-per-cell)):
  the fixed one from the head and then the mobile one, without "already seen". The two functions
  `effects::FixedObjectsInMapCell` / `ObjectsInMapCell` (EffectValues.h) were removed on 2026-10-07: TownQueries had
  moved to the map cells and nothing called them. Before ⚠️ approximate: the openblack grid (`MapProduction`) only put a
  fixed object in the cells whose centre was less than its radius + 1 m away, so a small tree far from the centre of
  its cell was in none, and neither `ApplyEffectToMapPos` (fire, lightning, water) nor water saw it (the burning bush
  next to the Land1 store was not put out); then the fixed list of a cell became the grid's plus the fixed objects
  whose position falls in it, ordered by entity.
- **Heal**: `Magic/Spells/SpellHeal.cpp`, `Particles/Rules/Heal.cpp`; `FindTargets` in `Magic/CastRules.cpp`;
  `particles::Effect::TakeTarget`. The chakra's rule is `Modifier::KeepsAlive`; its fade is `particles::heal::ChakraFade`. The
  openblack atom vector grows at the end (`ParticleEffect.cpp`, `NewAtom`), so the sound goes on the last one (a freshly made
  atom has age 0, so in both cases the sound waits for the next step). `components::SpecularColour` stays with RGB 0
  and only the destructor removes it (`Heal.cpp` `ClearSpecularColour`). The glow is drawn in `RenderingSystem` /
  `vs_object`: `components::SpecularColour` goes in the y of the fifth column of the instance
  (`argb_colour::PackInstanceSpecular`, 8 bits per channel, [the object colour fields](../rendering/model-lighting.md#the-object-colour-fields-in-the-instance)).
  **Approximate**: openblack does not have the hand's flag, so it uses the object the hand is carrying
  (`HandSystem::GetHeldObject`). The birds' rule is ported in `CastRules.cpp` with `ecs::animal_ai::IsFlyingSpecies` +
  the vulture separately (IsFlyingSpecies does not include it). openblack does not create CitadelDove / CitadelBat
  (inferred: they are healed). The power-up's mushroom is ported: the two rules are registered in
  `Particles/Rules/KeyPoints.cpp`, the creator `ParticleMeshCreatorAnimTextured` is in `Particles/Creators/Mesh.cpp`
  and the `SoundOfCreate` in `Particles/ParticleEffect.cpp`; openblack already reads the two-sided flag from the mesh itself
  (`L3DSubMesh.h`, byte +5 bit 0), so there is nothing to fix there. The mist remains for water and weather.
- **Poison** (`ECS/Life.h`, `ecs::life`): the flag is `components::Poisoned` (the pots' is `components::Pot::poisoned`),
  with `ecs::life::IsPoisoned` / `SetPoisoned`. The eating side belongs to the villagers: the API is
  `ecs::life::TakePoisonedResource`. The damage is `ecs::life::ProcessPoison`, called from `CheckHungry` (the rest of
  `CheckHungry` is still pending in the villagers). The tint is data, `ecs::life::k_PoisonDiffuse` and
  `k_PoisonSpecular`: **drawing it belongs to the shaders** (`LH3DColor`), not visible yet.
#### Where the code is

- `ResourceInfoOf` (`SpellResource::GetMagicInfo`); `CreateMagicFood` and `CreateMagicWood` (`Magic/Objects/MagicPiles.h`,
  the pile made with `PotInfo::MagicFood` or the wood's info, the scale `PotArchetype`'s).
- Water (`SpellWater.h`): `DropDistance`, `InReach` and `RippleDue`, the three rules above as pure functions.

### Test hooks

- `test_food_wood`: spline, loop, costs, sounds, `GetProportionRaised`, the sprinkle emission and the reading of
  KeyPoints; with `OPENBLACK_GAME_PATH` the real rows.
- `test_water`: the getters, the drop distance and the reach (strict, 5 m for a field), the rings in single precision,
  the mist colour, the `UR_HandSprinkle` cloud with its mist and, with `OPENBLACK_GAME_PATH`, the rows of info.dat
  (burn −4000, radius 1, `costPerEvent` 10, 6 / 10 s, reaction 21, particles 16/17, cost 5000/7000),
  `effectOfWaterSpell` = 2 in the 6 field rows and SF_Water / SF_WaterPU1 executed (cloud + cone).
- `OPENBLACK_TEST_WATER_SHOT` ([debug environment variables](../project/debugging.md#debug-environment-variables)).
- `OPENBLACK_TEST_HURT_VILLAGERS="x,z,radius,life[,poisoned[,turn[,heal[,repeat]]]]"`: hurts the villagers in the
  radius, can cast the miracle right there and writes every change of life, poison and glow.
- `test_heal`: the chakra curve (`ChakraFade`, the alpha 100 at the peak and the glow 160 at 1.2 s), that the rule
  keeps the effect alive without atoms until the `CloseDown`, the fuse and the single speed of the burst, the hand
  wave, the two colours of the poison tint with its damage per check (`Heal.poisonData`) and, with
  `OPENBLACK_GAME_PATH`, the real values of SF_HealChakra and SF_HealChakraInHand.

## Pending

- **Food and wood**: the diversion of the wood of a store with a building site (`StoragePit` +0x74 = `BuildingSite`)
  is not ported, though openblack's stores can now have one (`ECS/PotResource.cpp`).
- **Water**: SF_WaterInHand / SF_WaterOnHolder use the same mist, which is now drawn; they have not been reviewed in a
  screenshot. The 2D distance uses the exact root, not `hypotenuse` 0x74F680 ⚠️ approximate.
- **Heal**: it is still inferred that the dead are not healed: openblack has no `IsDead` (the dying states are not
  ported), so a living villager or animal always passes the filter. CitadelDove / CitadelBat are unidentified.
- **Poison**: the tint is not drawn (the shaders); `Villager::DoSleeping`'s skipped recovery is not ported; the rest
  of `CheckHungry` waits in the villagers.
