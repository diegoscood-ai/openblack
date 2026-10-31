# Fire

## Summary

Anything that can burn has a temperature. When something gets hotter than its burning point it catches fire: it gets
hotter still, loses life, turns black and heats everything near it. That is how a fire runs from tree to tree and
house to house, until it burns itself out, villagers beat it out, or water and rain cool it. Villagers run from a
fire, fight it or walk round it. A creature reacts to a fire and may put out a burning town with the water miracle.
This page covers the heat model, the spreading, the flames and the sound, the villagers' fire states, the creature's
answer, the scripts' fire commands and the trees' fire values.

openblack ports the heat model bit for bit (its formulas are pinned against the original's functions), and also the
spreading, the fire groups, the flames, steam and smoke, the sound and the villagers' fire states. What is left is in
[Pending](#pending).

## The original

### Fire

The heat model is `FireEffect`, the code of `SpreadEffect.cpp` (0x72E940..0x7310F0). Everything below was read in the
exe except what is marked unverified or 🔍 inferred. The fireball and the lightning, which start most fires, are on
[Fireball and lightning](../magic/fireball-and-lightning.md#fireball-and-lightning).

#### The fire record

Every object hotter than the air carries a `FireEffect` (0x50 bytes, save type 0x29) at Object +0x44.

- **The list.** There is one global list, newest first, at g_game +0x205C14. `FireEffect::ProcessList` 0x730760 walks
  it once per turn (0.1 s; slot 6 of `GGame::ProcessTurn`).
- **The vtable** is 0x9996D4 ("SpreadEffect:").
- **Fields:**

| Offset | What it holds |
|---|---|
| +0x14 | the temperature T |
| +0x18 | the temperature at the end of the last turn's update |
| +0x1C | the burning object: the constructor (fn_0072EB00 0x72EB3B) takes it from its first argument and writes the fire back to the object's +0x44 (0x72EB52). bw1-decomp's header differs (it names +0x1C the source and +0x24 the thing); the exe shows +0x1C is the object |
| +0x20 | the player responsible (`GetPlayer` 0x72EAB0) |
| +0x24 | the fire's source, the thing that heated it (the constructor's third argument, 0x72EB3E) |
| +0x28 | the reaction it set up: REACT_TO_FIRE (10) from the turn's update (0x72F75B..0x72F763), or 33, a burning object in the hand, from `StartedMoving` (0x730A98..0x730AA0); cleared with its reactions by `StartedMoving`, `ToBeDeleted` and `SetOutMagicHand` |
| +0x2C | its flames' graphic (a FireGraphic, made by `CreateSprites` 0x730AD0; not saved, made again at a load) |
| +0x30 | the process tag, copied from 0xDA09E5 at creation; the fire is processed while it equals 0xDA09E4 (both are 0 every turn) |
| +0x34 | the charring |
| +0x38 | flags (bit 0x20: the sound is playing). Each turn's update (fn_0072EFB0) first clears the low four (0x72F00A), then sets 2 when T is above 3 Tc (fn_00730170 = Tc × 3.0 [0x8C2C50], strictly, 0x72F01C..0x72F02B: very hot) and 1 when the turn's previous temperature (+0x18) was below Tc and T is at or above it (0x72F02F..0x72F053: it caught) |
| +0x3C | its link in the list of every fire (not saved; a load rebuilds the list, 0x594250) |
| +0x40 | the next fire of the group (fn_00732AD0 reads it) |
| +0x44 | the group's root (`GetFirstCaused` 0x732AE0 reads it) |
| +0x48 | the firemen list (kept only by the root) |
| +0x4C | the root's firemen count: `AddFireman` 0x7309A0 pushes a node at the head of the root's list and adds 1 (0x7309CB), `RemoveFireman` 0x7309E0 takes out every node of that villager and subtracts 1 for each (0x730A0F), and `AddToMyFireGroup` 0x72FBE0 appends the other root's list and adds its count (0x72FC55), clearing both on it; the root change fn_0072FD20 moves the list and its count together and zeroes the old root's (0x72FD5E..0x72FD6A), and the constructor (fn_0072EB00 0x72EB0E) and the reset fn_00731460 (0x73147C) zero both at once; no other code writes it, so it is always the list's length, which is what openblack counts (`firemen.size()`, checked 2026-10-26) |

#### Object values

- **The burning point.** `Tc = max(combustionTemperature, 40)` (fn_00730180), with combustionTemperature at
  `GObjectInfo` +0xB4 (`Object::GetCombustionTemperature` 0x639A30). The ceiling is `Tmax = 2·Tc` (fn_007301B0).
- **The heat capacity** is `max(heatCapacity, 1)` (fn_007301D0), with heatCapacity at `GObjectInfo` +0xB0
  (`Object::GetHeatCapacity` 0x637CE0, vt 0x5E4). `MagicFireBall::GetHeatCapacity` 0x682D40 is
  r² × 0.0625 × info.heatCapacity × the effect's strength (manager +0x54).
- **The defence multipliers** are at `GObjectInfo` +0x80..; the burn one is info +0x90 (fn_00730230). These are read
  in `FireObjectTraits.cpp`.
- **The ambient temperature** is `MapCoords::GetTemperature` 0x605CC0 = **24.7 across the whole map** (fld 0x930080);
  the fire's helper for it is fn_007301C0.
- **The rain cooling multiplier** (vt 0x5EC) is 0.01 for an Object (0x639AB0). `MagicFireBall` 0x682DB0 gives
  +0x58 ? 0.01 : 0 (0 when the script cast it).
- **The fire centre** (vt 0x5F0) is the object's position (0x639AA0). For a `DeadTree` (0x510CE0) it is the world
  matrix × the mesh centre (L3D +0x18) in x and z, with its own height above the land (+0x1C) in y.
- `Field::Field` 0x527DD0 hands its GAbodeInfo to `Abode::Abode` (0x527DFA), which keeps it as the object's info.

#### Starting a fire

- **Creating.** `FireEffect::Create` fn_0072ECF0 makes no fire if the object refuses a burn (`IsEffectReceiver` of the static
  EffectValues 0xDA0980 = BURN 100), cannot be set on fire (+0x0A bit 3), has Tc 0 or is being deleted. Otherwise T
  starts at the object's temperature, the fire goes to the head of the list as the root of its own group, gets its
  graphic (`CreateSprites` 0x730AD0 → fn_00731160) and the object is told `StartOnFire`.
- **Reading an object.** `Object::GetTemperature` 0x639A10 returns the fire's T, else the ambient.
  `Object::IsOnFire` 0x637CC0.
- **Setting the temperature.** `FireEffect::SetTemperature` 0x72EF10 (from `Object::SetTemperature` 0x639A60): a new fire when the value is
  hotter than the object, then T = t; an existing fire just takes t, also a lower one.
- **Setting on fire.** `FireEffect::SetOnFire` 0x72EF60 (from `Object::SetOnFire` 0x639A40): T = 2·Tc·speed + Tc.
- **Whose fire.** Both make a new fire with the object's own `GetPlayer` (vt+0x1C, 0x72EF37): a villager's town's
  owner (none without a town), a creature's owner, a building's town's owner (the neutral player without one, a field
  none), a rock's player, and the neutral player for the classes that keep `GameThing`'s. openblack credited only a
  villager's town, none for the rest, until `fire::traits::PlayerOf` (the per-class answer of
  `effects::HarmedPlayerOf`; test `FirePlayer.AFireIsCreditedToTheObjectsOwnPlayer`).
- **A burn or a cooling effect.** `FireEffect::ApplyEffectToFireEffectIfNecessary` 0x730670 (the start of `Object::GetDamageEffect` 0x637D00): a
  burn drives T towards ambient + burn with T += min(10·dT / capacity, dT); a negative one (water, beating) cools it;
  a villager not yet on fire runs (`SetupOnFire`).
- **Helpers:** `GetMaxFireRadius` 0x730000 = 1.25 × the fire radius; `GetSafeFireRadius` 0x730020 = min(fire radius,
  max) + 1; the heat content fn_00730600 = (T − Tamb) × capacity; fn_00730630: T += q / capacity, at most dTmax by
  magnitude; fn_0072F960 = 0.1 × 100 × dT. `SetOutMagicHand` (`FireEffect::SetOutMagicHand` 0x730AB0); clear fn_0072ED80.

#### The heat model

- **Thresholds.** The object **burns** when `T >= Tc` (`IsOnFire` 0x730360). It reacts with `T >= 100` or `T >= Tc`
  (`IsAboveReactionTemperature` 0x730380).
- **Sizes.** The fire fraction (0x7303E0) is `(T - 0.8·Tc)/(2·Tc - 0.8·Tc)`, limited to `2·life` and to 0..1. The fire
  radius is `1.25 ·` the object's radius `·` the fraction (0x72FF10). The flame height is
  `1.25 · height · (T - Tamb)/(2·Tc - Tamb)` (fn_0072FF70).
- **Per turn** (`fn_0072F5B0`, inside ProcessList; the whole step is fn_0072EFB0):
  - In water it cools 50 times faster; with rain or snow the multiplier is `rainCoolingMultiplier·rain + 1`.
  - If it was given heat this turn: `T += 0.1·T/(2·Tc)`, with a ceiling of `2·Tc`.
  - Otherwise: `T -= (T + 10 - Tamb)·(4·height·radius)·0.1·multiplier/capacity` (the "area" 4·H·r).
  - While burning, the damage to the life is `(T - Tc)/(2·Tc - Tc) · defenceMultiplierBurn · 0.1` (0x72EEC0).
    **Charring** grows +0.04 per turn while life < 0.6, capped at `(0.6 - life)/0.6`; while cooling it drops 0.02 per
    turn.
  - The constants: 0.1 [0x999630] (0x72F418), 0.6 [0x999658] (0x72F428), +0.04 [0x999654] (0x72F435), 1 / 0.6
    [0xDA09C0] (made at 0x72EA16, used at 0x72F467); charring −0.02 [0x999650] (0x72F558).
- **Reactions.** The reaction `REACT_TO_FIRE` (10) is created when T passes the reaction temperature and deleted when
  it drops below it. In the hand it is `REACT_TO_BURNING_OBJECT_IN_HAND` (33), `FireEffect::StartedMoving` 0x730A60.
- **Death.** When the life reaches 0 the fire calls `DestroyedByEffect` (vt 0x5F8) with the fire's `GetPlayer`
  (0x72F509) and 0 (0x72F506 `push ebp`, ebp = 0 since 0x72F216), not with the burn damage. A creature is not
  destroyed. What each class does is in [What each class does with fire](#what-each-class-does-with-fire).

#### Spreading

- **The search.** Each turn a burning object walks a spiral of 10 m cells while the cell is within `fire radius + 10 m`
  of the fire centre. Each object in those cells receives heat (`fn_0072F980`). The walk ends at the first cell beyond
  that distance (0x72F685), so a fire reaching less than 14.1 m searches only its own cell and the next one along -x (the
  spiral's second step, at -x -z, is already out of reach). The wind (fn_00771B10) **is computed and discarded**: it
  does not move the search.
- **The objects of a cell** are taken in the order `MapCoords::FindType(-1)` walks them (0x72F68E / 0x7308B8, each
  call with the previous one): the fixed list, then the mobile one only when the last fixed object's type counts as
  fixed (`MapCell::FindTypeOnMap` 0x6015E0, 0x601621). The fire's own object is skipped. There is no "done" set, so an
  object in several cells of the spiral is heated once per cell. The held object is off the map.
- **The reach** (fn_0072F980): a target is heated only when `GetDefaultFireRadius(target) + GetFireRadius() > d`
  (0x72FA54..0x72FA67, strict; d the table distance of the two fire centres); then, unless both centres are under 3 m
  above the land [0x8C2C50] (0x72FA6D..0x72FA8D), only when `GetHeight(target) + flame height > source y − target y`
  (fn_0072FF70, 0x72FA8F..0x72FABD, strict). openblack: `fire::Reaches` and `fire::ReachesHeight`
  (`src/Fire/FireModel.cpp`), test `FireModel.AFireReachesOnlyWhatIsStrictlyWithinBothRadii` ✅.
- **A villager putting out a fire** receives no heat (fn_0072F980; see `Villager::IsFireMan` below).
- **An object in the hand** (`Object +0x24` bit 2, `GetPlayerHoldingThis` 0x63A190) spreads only where
  `CalculatePlayerInfluence` 0x5CD170 (with 1, 0, 0) of the holder is > 0 (0x72F61C).
  `FireEffect::CheckToSeeIfObjectIsNearOnFireObject` 0x730860 (from `Object::ProcessInHand` 0x639AD0): every fire in
  the map cell of the held object's fire centre heats it (0x7308B8 / 0x7308DB).

#### Fire groups

- Each fire is born as the root of its group (+0x44 root, +0x40 next). `AddToMyFireGroup` 0x72FBE0 chains the new fire
  right behind the one that lit it.
- A root that stops burning hands the group to the first member that is burning: fn_0072FD20 hands it to a member,
  which goes first, and the firemen list of the old root is appended to the new one's (0x72FC30 / 0x72FD36).
- `IsInSameFireGroupAs` tests the membership (`FireEffect::IsInSameFireGroupAs` 0x72FEA0) and `RemoveFromFireGroup`
  takes a fire out (`FireEffect::RemoveFromFireGroup` 0x72FDD0).
- The firemen list is the root's, newest first: `FireEffect::AddFireman` 0x7309A0 and
  `FireEffect::RemoveFireman` 0x7309E0. fn_0072F910 / fn_0072F930 stop the group's firemen (all, or those fighting this fire).

<a id="fire-record-details"></a>
#### Passing a fire to another object

- **Copying.** `CopyFromObjectToObject` 0x7308F0 (fn_007308F0) (size 0x70; `FireSystemInterface::CopyFire`): nothing if either object is null
  or `from` has no fire. A `to` without a fire gets one, made with the source fire's player and its source (+0x24); if
  that fails, nothing more. The source fire then `AddToMyFireGroup`s it. If its T is below the source's, both its T and
  Tprev become the source's, else both its own T (an unordered compare takes the source's): same group, T = Tprev =
  the larger of both. The source is left as it was. Its only callers are `Rock::SplitInTwo` (0x6E7774) and the scaffold
  tap split (0x6EA123).
- **Moving.** `MoveFromObjectToObject` 0x730960 (size 0x40; `FireSystemInterface::MoveFire`): nothing if either object is
  null or `from` has no fire. Then `to`'s fire pointer becomes the fire, with no test of whether `to` already burns,
  and `from`'s is cleared. A reaction's initiator becomes `to` (`SetInitiator` 0x6E4830, fn_006E4830), and the fire's
  object becomes `to`. Its group, sound slot and graphic hold the fire itself, so they follow it unchanged. Its only
  caller is the dead tree made from a tree (0x510942).
- fn_0072EB00 is also part of the record's code (not described).

### What each class does with fire

- **Who takes a burn** (`Object::IsEffectReceiver`, vt 0x774): an Object, 1; `Pot::IsEffectReceiver` 0x66D650, only with
  something in it (+0x70 != 0); `Field::IsEffectReceiver` 0x528900, a burn only while `ValidForPlaceInHand` (vt 0x6FC; 🔍 inferred: while it has
  food to give); a `Villager` (0x751D70) refuses only a heal when dead.
- **`StartOnFire`** (vt 0x6BC): `MultiMapFixed` 0x52EC60 (also Rock, MobileStatic, AnimatedStatic, Fragment, Field,
  FishFarm, BigForest, the citadel and worship classes, spell icons) and `DeadTree` 0x510E20 drop their reactions;
  `Pot` 0x66D6C0 its own (RemoveReaction). Tree and the living do not override it.
- **`EndOnFire`** (vt 0x6C0): `DeadTree` 0x510E60 creates REACT_TO_WOOD (0xC) with its GetPlayer (vt 0x1C) and 0
  while available (openblack: `fire::traits::EndOnFire` passes `DeadTree::owner`); `MagicTree` 0x5FD0E0 creates REACT_TO_MAGIC_TREE again (no availability test);
  `Pot::EndOnFire` 0x66D6D0 re-creates its reaction (fn_0066D660, info +0x128).
- **`ReduceLifeDueToBurning`** (vt 0x5C4): for an Object (0x637C20) it is vt +0x5B8 ReduceLife unless +0x0A bit 2:
  `Abode::ReduceLife` 0x405D90 for abodes, `Creature::ReduceLife` 0x47DD00 for a creature (the Creature's vtable holds
  it at +0x5B8 while +0x5C4 stays the Object's), `Object::ReduceLife` 0x637810 for the rest. For a Field (0x52A050) it
  is RemoveFood(damage × GFieldTypeInfo +0x130 totalFoodInField) and returns 1 (the 6 rows are identical). Then, with
  a town and a damage != 0, `Town::UpdateAggressor` with EffectValues(BURN, T, 0, 1, player) and damage × the info's
  `AggressorValue` (see [Town aggression](../beings/towns.md#town-aggression)).
- **`DestroyedByEffect`** (vt 0x5F8): an Object (0x6378E0, features too) is `ToBeDeleted`; a Villager (0x7502D0) dies;
  an Abode (0x403F80: building site, ghost; openblack: `abodes::DestroyedByEffect`, test
  `DeadlistBuildingsTest.AFireThatBurnsAHouseDownDestroysItAsAnEffectDoes`). A Field (0x52A010) empties: fn_0052A030 sets crops, growth and food to 0
  and `Object::SetTemperature(0, null)` (0x52A046 → `FireEffect::SetTemperature` 0x72EF10: the fire's T = 0); then the
  fire (+0x44) is deleted (vt 0xC, 0x52A024). The two arguments are `player` and `amount` (the fire passes its
  GetPlayer and 0).
  For a tree, the Object's `ToBeDeleted` is the tree's own (`Tree::ToBeDeleted` 0x74A210: out of its forest, then
  `Object::ToBeDeleted` 0x636670, which deletes the fire too); openblack destroyed a burnt tree's entity directly,
  without telling what watches trees going (a magic tree's reactions and forest, the hand holding it), until
  `fire::traits::DestroyedByEffect` called `ecs::DeleteTree` (test `BurntTree.ATreeBurntAwayGoesThroughTheTreesOwnDeletion`).
- **`CleanupWhenDeleted`** 0x6377F0: RemoveMapObject vt +0x548.
- **A creature** gets 3 flames (random points in its box), each through `LH3DCreature::BeCut` 0x486390 (bw1-decomp's
  symbol; the old notes named it `Creature3D::AddFlame`, fn_00486390), called by fn_0072EFB0 at 0x72F1D4.

### The fire's sound

The sound is `FireSound.cpp` ([world sounds](../audio/world-sounds.md)).

- The table has **2** slots (0xDA09CC, count 2 at 0x72E956), but in effect only the fire **closest** to the camera
  with fraction > 0.1 makes sound, a looping `G_Fire` on the object: the choice below always takes the first slot (see
  Choosing the slots).
- A fire that starts its sound sets +0x38 |= 0x20 before the play (0x72EE1C). The channels' owner of each fire that
  played is the FireEffect (LH_SamplePlayOptions +0x20, 0x72EE3B); its position is
  `FireEffect::Get3DSoundPos` 0x72EE70 until the fire goes.
- **Choosing the slots.** The farthest slot's distance is 0xDA09C8. At the end of fn_0072EFB0 (0x72F7C6..0x72F875) a
  fire nearer than the farthest slot (or with no slot yet) takes the first slot that is empty or not nearer, then the
  farthest distance is recomputed; a fire already in a slot can take the other one too. The original stops the sound of
  the fire the slot held before the new one takes it, whatever fire that is, the same one too (fn_0072EDC0 at
  0x72F82C, for any fire in the slot); the same fire then plays again with the slots' next turn. openblack's
  `sound::Consider` does the same, the same fire too (test `FireSound.AFireRetakingItsOwnSlotStopsItsCrackleFirst`;
  before, a fire retaking its own slot kept playing). Read literally the second slot stays empty: the
  first slot is taken when it is empty or its distance is not below the farthest one (0x72F808..0x72F81A), and the
  farthest is the larger of the two slots' distances, the first's once the second is empty, so a nearer fire always
  replaces the first slot's; the second is reached only while the farthest is 0, a fire at the camera's own point.
- **The distances** are `GetDistanceInMetres` 0x74CD70 from the object's MapCoords (+0x14 of `GetObject`) to the
  camera's (`GGame::GetCamera` +0x14): at the take (0x72F7C6) and for every slot at the start of each `ProcessList`
  (0x730776..0x7307A2), the farthest then worked out again (0x7307B5..0x7307E7). openblack: `CameraDistance` in
  `FireSound.cpp`, `gutils::GetDistanceInMetres` of the two MapCoords (test
  `FireSound.AFiresCameraDistanceIsTheGamesMapDistance`; before 2026-10-26 the float hypotenuse of the positions,
  a few thousandths of a metre off the game's), refreshed by `sound::RefreshDistances` at the start of `ProcessList`.
- `ProcessList` 0x730823..0x73083E calls fn_0072EDE0 for each slot every turn.
- **Stopping.** fn_0072EDC0: a fire whose sound plays (+0x38 & 0x20) stops it through GGlobal fn_0042A240(fire, 2, 1)
  → `GAudio::StopPlayingSoundEffect` (InGame, owner the fire, sample 2); then the bit is cleared.

### Flames, steam and smoke

The visual is `FireGraphic.cpp` (`PSysBase` 0xD0, fn_00731160/1560/2200): `S_Fire.raw` flames, additive white steam
and grey smoke from `S_SpriteSheet3`, and a light map.

#### The graphic object

- FireGraphic vtable 0x9997D4, made only for an object with a 3D object; `~FireGraphic` 0x7313C0.
- fn_00731560 updates it with the frame time (g_game_time_inc × 0.001). The original updates it only when drawn
  (`FireEffect::Draw` 0x730330), with a catch-up after 10 turns.
- `FireGraphic::SpritePos` (`SpritePos`) is 0x48 bytes (vtable 0x9998D4); its age is +0x2C.
- **Materials:** 0xDA09EC (S_Fire, mode 13), 0xDA09F0 (S_SpriteSheet3, 13), 0xDA09F4 (S_SpriteSheet3, 6). The shared
  LH3DSprite 0xDA09E8 has size 1 and height 2 (Create 0x73130F / 0x731316).
- **The cell** is the sprite flags' low 6 bits (0x7323B7..0x7323CA), on 8 cells a row (LH3DSprite +0x30 = 8 from
  SetToZero 0x8404F0; 🔍 inferred: S_Fire is drawn 8 × 8 like S_SpriteSheet3). The flame cells need 64 frames.
- **Drawing.** fn_00732200 goes through the renderer's PSys sprite path, with one Z object per fire at the object
  (fn_007325D0). It draws a fire's flames (fn_00732220, newest first, so the oldest last), then its steam, then its
  smoke.

#### The flames

- **How many and how big.** `GetPSysFireMaxFlames` 0x732A30: trees 2; otherwise max(2D radius, height) < 3 ? 2 : 7.
  `GetPSysFireLocalFlameScale` 0x732950: trees 0.2 × height, the rest 0.5 × height (impressive objects and citadel
  parts 0.3).
- **Where.** `Object::GetPSysFireLocalRndFlamePos` 0x732770 (fn_00590460) takes a random point of a random triangle of
  the mesh in mesh coordinates (the LOD 0 triangles; 🔍 inferred: fn_0080B2A0 counts the drawn mesh's triangles, and
  the sampling steps), trees × 0.5. `Object::GetPSysFireWorldFlamePos` 0x732660 (`GetPSysFireWorldFlamePos`) is the object's matrix × that point.
  `GetPSysFireFlameMatrix` (`Object::GetPSysFireFlameMatrix` 0x732630) copies the object's G3D matrix (vt 0x570), the one it is drawn with; its
  translation goes to +0x98 (0x7316FD..0x731713) and +0x74.
- **Following the land.** With flag bit 0 (+0xB5 & 1) the flames follow the land like the morphed object: H0 at the
  graphic's +0x98 / +0xA0 (0x7322A9..0x7322D2), y = (H(flame) − H0) + y (0x73230A..0x732366).
- **Look** (fn_00732220): mode 13, orange 0xFF713C, cell `int(fmod(-25·age, 32) + 32)` (fn_007321B0; cell 32 at age
  0).
- **How many by now** (fn_007317F0, read 2026-10-25): while the fire fraction is not 0, the graphic's accumulator
  (+0x38) gains the most flames (+0x6C) / 4.3 [0x999664] × the fraction × dt (0x73181C..0x73183C, on the x87 at the
  game's 24-bit precision); the first time the fire's flags have bit 2 (very hot) it gains the most flames once more,
  marked at +0xA8 (0x73183F..0x73185F); a turn with bit 1 (it caught) adds (most + 1) / 2, an integer halving
  (0x731862..0x731879). Then a flame is added while the count (+0x3C) is below the accumulator. openblack:
  `graphic::AccumulateFlames` (test `FireGraphic.FlamesGrowInWithTheFiresStrengthAndAtOnceWhenItCatchesOrIsFirstVeryHot`).
- **Life:** 4.3 s [0x999664], fade-in 1 s [0x999660]. Scale (fraction + 1) × 0.5 [0x8AA3B4] × localScale (fn_007317F0
  0x7319D0). Alpha clamp(…, 0, 1) × 250 [0x8C7B2C] (0x731A34): age / 1 under 1 s, else 1 − (age − 1)·(1 / 3.3), each
  step in float, truncated by `__ftol`; a flame is removed once its age is over 4.3 (0x73199B). openblack:
  `graphic::MaxFlames` and `graphic::FlameAlpha`, tests `FireGraphic.TreesHaveTwoFlamesAndBigThingsSeven` and
  `FireGraphic.AFlameFadesInOverASecondAndLivesFourPointThree` ✅. A NaN size or age answers as the exe's unordered
  compares do (0x732A8C `test ah, 1`: 2 flames; 0x7319A3, 0x7319E7, 0x731A10, 0x731A42: the flame stays and its alpha is
  0), since 2026-10-25 (test `FireGraphic.ASizeOrAnAgeThatComparesWithNothingGivesTwoFlamesAndNoAlpha`).
- **Origin.** Each flame sets the shared sprite's +0x18 = 0 and +0x1C = height × size × −1 ([0x8AB678]) = −2 × size
  (0x732382..0x732395). `LH3DSprite::Draw` subtracts it, so the flame's local y runs from 0 to 4 × size on the
  screen's up and its base stays on the point.

#### Steam and smoke

- **Look** (fn_007323F0 / fn_007324E0): white steam (additive), grey smoke (alpha 6), cell `int(fmod(25·age, 32))`
  (`SteamGetOffsetFromAge` 0x7321E0 at 0x7324B2, and fn_0073250A 0x7325A2). The smoke is grey with or 0x707070
  (0x732593).
- **Emission:** in bursts of 30 turns (the constant at [0x999694 / 0x9996B4]), 4 per second (steam [0x999670],
  0x731B50; smoke [0x9996A0], 0x731ED7). Steam only while cooling and T > 75 [0x999638] (0x731AD9). The burst is over
  when `turn > start + 30` (0x731B33..0x731B46, compared in x87 from the integers, so exact); each frame adds 4·dt to
  the accumulator and a puff comes while `count < accumulator` (0x731B50..0x731B71 and 0x731CA1..0x731CAC), so the first
  at once. openblack: `graphic::BurstOver` and `graphic::PuffsDue`, test
  `FireGraphic.SmokeAndSteamComeInBurstsOfThirtyTurnsAtFourASecond` ✅.
- **Puffs:** life 3 s [0x999680 / 0x99969C]; scale ((2.6 − 0.2)·t + 0.2) × baseScale (steam [0x999688] / [0x999684]
  at 0x731D2D, smoke [0x9996A8] / [0x9996A4] at 0x73209A); pulled by the wind (wind × 0.5 − v) × 0.1 × dt (fn_00731AB0
  0x731D71..0x731DB9, smoke 0x7320DE..0x732126; 0.5 [0x999674], 0.1 [0x999678]). Steam alpha 100 [0x999690] and rise
  1 [0x99967C]; smoke alpha 180 [0x9996B0] and rise 2 [0x9996B8].
- **Their origin.** fn_007323F0 / fn_007324E0 never write +0x18 / +0x1C of the shared sprite 0xDA09E8, so they keep
  the origin −2 × size of the last flame drawn with it; a fire with no flame takes the last flame of the fire drawn
  before it. In the original they sit 2 × that size higher on the screen's up. The order of the fires between them is
  not read.
- **The sizzle** (fn_007314E0, 0x731B0F): LH_SamplePlayOptions with bank +0x04 GGlobal+0x3AC (InGame), sample +0x24
  0x35 (G_Steam_01), owner +0x20 the FireGraphic, is3D +0x08 1, track +0x0C 0, the point +0x30 its +0x98, then
  `GAudio::PlaySoundEffect` 0x429E30. 🔍 inferred: +0x98 (copied to +0x20 by fn_00731560 0x7316FD, its writer not
  read) is the burning object's position. openblack: `graphic::SizzleOptions`, test
  `FireGraphic.TheSizzleIsTheInGameSteamSampleIn3DUntracked` ✅ (the point stays 🔍).

#### The light map

- `S_LMFireBall` (pitch 6, 3 bytes, 1 frame, loaded once into 0xDA0960, +0x30) is given only when the object is a
  MultiMapFixed and its Get2DRadius > 2 (0x7312B6..0x7312EF). The flag `Object +0x24 & 2` (0x7312D5) is set only by
  the `MultiMapFixed` ctor (0x52E207: houses, BigForest, Feature...), not by a standalone tree. openblack:
  `graphic::GetsLightMap` (strict > 2), test `FireGraphic.OnlyAMultiCellFixedObjectWiderThanTwoMetresLightsTheLand` ✅.
- With flag bit 4 (+0xB5) its alpha is 0.6 [0x8C7BDC] × GetFireFraction × (1 + 0.2 [0x99964C] × VLNoise(0.6 [0x999644]
  × (turn + fraction) + (this & 0xFFFF))) × (1 − charring +0x34), only above 0, at most 1 (0x731633..0x7317B2).
  VLNoise is 0x590C30.

#### The burnt colour

- **The tint of a burning tree** (fn_0074B3A0): grey 50, or `max(50, 255 − (1 − life)·2550)` with life > 0.9,
  **capped unsigned at the frame's tree brightness [0xC22FA0]** (0x74B47B), the same one that multiplies a tree
  without fire at 0x74B077. Run natively (the oracle, 13 lives and brightnesses), openblack's
  `graphic::TreeDrawColour` gives the same grey (test `FireGraphic.ABurningTreesTintIsCappedByTheTreeBrightness` ✅).
- **The narrowing** (fn_0074B3A0 0x74B527..0x74B5BA): below 0.2 [0x8AB244] life, rows 0 and 2 of the 3D object's
  matrix (+0x14..+0x1C and +0x2C..+0x34) are multiplied by `1 − (0.2 − life)·5` [0x8DAD58] for the draw and put back
  after it; the height row is kept. At the game's 24-bit precision every step rounds to float, so just under 0.2 gives
  the float just under 1, not 5·life. openblack: `graphic::BurntTreeWidth`, test
  `FireGraphic.ABurntTreeNarrowsBelowAFifthOfItsLife` ✅ (oracle).
- **The grey of the charring** (fn_00730570): `k = ftol(c·255) & 0xFF` and each channel `((unsigned)(−175k) >> 8) − 1`
  (0x730585..0x7305D7) = **`255 − ceil(175k/256)`**: 255 with k = 0, **80** with k = 255. The brightness is
  `GetFireEffectCharingColor` 0x730480.

#### The forced ALPHAREF

- `Tree::Draw` (0x74B070) and `Tree::DrawOutOfMap` (0x74B320) call fn_0074B3A0 only for a tree with a fire
  (obj+0x44 ≠ 0).
- **Heat** = 255 if T > 1.5·Tc (`GetTemperature` 0x639A10 = the fire's +0x14, `GetCombustionTemperature` 0x639A30 =
  info+0xB4; an unordered compare computes), else `ftol((T − Tc)·255 / (1.5·Tc − Tc))` (0x74B3D3..0x74B402).
- **The value** (0x74B4D6..0x74B51E): `(255 − 230 [0x99A11C])·heat·(1/255 [0x900058]) + 230`, stored as a float in
  0xDA39C8, 254.0 when not below 254 [0x99A17C]. `OverrideMaterial` [0xECA658] = 1 and `OverrideRenderMode`
  [0xECA65C] = `ftol` of it. The tree is drawn (`AddForDrawing` 0x74B5A6 / 0x74B5CB) and the override is put back to 0
  (0x74B5D8) before `DrawFireEffect` (0x74B5E2).
- **Range 180..254**, not 230..254: a tree whose fire has cooled below Tc gets a negative heat (down to −510 at T = 0):
  180 at T = 0, 230 at Tc, 254 from about 1.5·Tc (the oracle, fn_0074B3A0 run natively with stubbed temperatures, 63
  cases).
- **Who reads it.** The six alpha tested mode functions read it as ALPHAREF (D3DRS 0x18) while the override is on:
  0x82E080 (9, 0x82E166 / 0x82E175, then scaled by the object's alpha with the global table), 0x82E2A0 (18), 0x82E470
  (15, scaled as 9), 0x82E6A0, 0x82E830, 0x82E9C0 (10, 11, 16); no other state changes.
- **The Z-sorter.** `AddForDrawing` → vt+0x100 = 0x815A70 draws at once (vt+0x108) unless the object's Z-sort bit
  (vt+0x44 = 0x7F97C0, flag 0x10) queues it in the Z-sorter (`NewZObject` 0x815F53), whose drain runs after the
  override is off: a queued tree (a held one, with its Z-sort bit set by the hand) is drawn with its own ALPHAREF.

### Villagers and fire

The villagers' fire code is `VillagerFire.cpp`, `VillagerFireman.cpp` (0x75A3D0-0x75B460) and `ReactToFire` 0x765870.

#### The states

- The states are 215 `REACT_TO_FIRE`, 216 `PUT_OUT_FIRE_BY_BEATING`, 219 `ON_FIRE` and 220 `MOVE_AROUND_FIRE`. The
  water ones (217, 218) **in the exe give up on the spot** (`DECIDE_WHAT_TO_DO`), so nobody carries water. Their rows
  are in [Fire in the table](#fire-in-the-table).
- `Villager::IsFireMan` 0x75B400: its final state's exit is `ExitPutOutFire` (rows 216..218 and 220), or it is in
  215. Such a villager is not heated.
- `Villager::GetFinalState` 0x751DD0: the top state if it is a final one, else the destination state.
- `Villager::SetupReactToFire` 0x765540, the start function of the `REACT_TO_FIRE` reaction, tests only for a null
  object, with no availability test. The setups store the reaction's object at +0xBC after `AddReaction` (0x5F14F2 /
  0x765BA2).
- **Catching fire.** `SetupOnFire` 0x75B170 stores the previous state and destination and switches to `ON_FIRE` with
  the fire that heats it. `Villager::FinishBeingOnFire` 0x75B3D0 brings back the saved destination and the stored
  state.
- `Villager::StopFireFighting` 0x75B020 is called when a fire group dissolves.

#### Joining and choosing a fire

The decision is in `ReactToFire` 0x765870. The villager looks for the fire of the group closest to it that is above
the reaction temperature (`fn_00730070`).

- **Facing the fire.** `ReactToFire` (0x7658BE) and `PutOutFireByBeating` (0x75AC7F) call `LookAtObject`(object, 2)
  0x5EC520, which is `LookAtPos` 0x5EC550 (object +0x14, mode 2 = steps of 0x100 of 0x800), and 1 with no object.
  While the villager is more than a step from facing the object it turns one step and the state returns 1 with
  nothing else done that turn. In `ReactToFire` the fire is stored at +0x114 first (0x7658B8), and the look comes
  before everything below. In `PutOutFireByBeating` the look comes after the beside-the-fire test (fn_0075ABA0, band
  2.0) and before `IsReadyForNewAnimation`(1) (0x75AC90). Half a turn away the villager takes four turning turns and
  decides on the fifth (a step is taken while the distance is not strictly below 0x100). openblack does the same:
  `villager_fire::ReactToFire` and `PutOutFireByBeating` call `villager::LookAtPos`(object, 2) at the
  same points (tests `VillagerReactionsTest.AFireReactionTurnsToTheFireAStepATurnBeforeItDecides`,
  `AFireReactionFacingTheFireWithinAStepDecidesAtOnce`, `BeatingAFireTurnsToItBeforeTheFirstBlow`).
- **Joining.** A fire within `2 · (10 + maxReactionDistance)` (the row's field at 0xD4FAC4; `GetDistanceInMetres`
  0x74CD70 at 0x76582B) of the fire the villager already fights is taken into that fire's group.
- **Choosing to fight.** At 0x765A5B a villager with a town that is not on its way to worship (+0xE0 0x10) may fight
  it. The score is the group's burning priority (fn_007302E0) × the room left around it × the town distance term
  (`GetDistanceModifier` 0x74F290, 400 m); above 0.1 (0x8AB22C) it goes to beat the fire.
- **The room** is fn_00730290 × 2π / (GetRadius × 4), and the score's room term is (room − the firemen count, +0x4C of
  the root) / room, 0 when the room is 0.
- No random term: fn_00730360 (`IsOnFire`) is called at 0x765B09 and its result unused.
- **The two group helpers** (traced 2026-10-16). fn_00730290 is the sum over the group (from `GetFirstCaused` +0x44,
  next +0x40) of the object's `GetRadius` (vt +0x60, which is `Get2DRadius` vt +0x64) of the members on fire
  (`IsOnFire` 0x730360; a member with no object adds 0, fn_00730270). fn_007302E0 is the highest
  `GObjectInfo::BurningPriority` (object +0x28, info +0xB8) over every member with an object, from 0, strictly greater
  (`fcom; test ah, 0x41`). The +0x4C count is the root's fire fighters.
- **The steps** (0x765A99..0x765B25): room = (sum × 2π [0x8AB210]) / (the villager's `GetRadius` × 4 [0x8AB418]);
  0 stays 0 (`test ah, 0x40`), else (room − the fire fighters, `fild qword`) / room; will = priority × room ×
  `GetDistanceModifier`(town distance, 400); it fights when will > 0.1 [0x8AB22C] and `DecideHowToPutOutFire`
  returns non-zero. Each step is rounded to a float in that order.
- Checked with the function oracle: the whole `ReactToFire` run with its callees stubbed, 612 cases, 301 that fight,
  300 of them next to 0.1. `fire::WillFightFire` (`src/Fire/FireModel.cpp`) gives every one; 29 are pinned in
  `test/test_fire_model.cpp` (`FireModel.AVillagerFightsAFireAsTheOriginalGameDecides`).

#### Beating a fire out

`Villager::PutOutFireByBeating` 0x75AC50 (216): the villager stands beside the fire, turns to it and strikes it with a
burn of `-8` (0x99A968) through `ApplyEffectToFireEffectIfNecessary`, walking round it while it is still above its
reaction temperature. When the fire has no object any more (0x75ACDA) the beating returns 0 and keeps its
state, a branch the beside test before it (fn_0075ABA0, which needs the object) never lets through. The
steps are on [Villagers: jobs](../beings/villager-jobs.md#beating-out-a-fire).

**The firefighter's spot** (`GetFireFightingPos` 0x75AA90):

- The angle is `Get3DAngleFromXZ`(the fire's centre fn_0072FEF0, the villager +0x14) (0x75AAD4..0x75AAE2), and the
  spot is the object + `GetPosFromAngle(angle, distance)` (0x75AB59..0x75AB6A).
- So it lies on the line from the fire to the villager, at `max(safe radius, object radius)` (0x75AAF2..0x75AB16) +
  the villager's radius (0x75AB23) + `GameFloatRand(1)`.
- The arrival of `MOVE_AROUND_FIRE` and of `GO_TOWARDS_TELEPORT` is `MobileWallHug::AreWeThere` 0x60AD60: strict
  `d² < (step +0x5A + extra)²`, where the step of `RebuildMoveByStep` 0x609D10 is `WallHug::speed`
  ([Villagers: data and states](../beings/villager-states.md), "AreWeThere").

#### Going round the fire

**A fire is not an obstacle on the map**. The burning thing keeps the collide data it had, and the fire
adds none. `FireEffect` is a `GameThing` (no position, never in a map cell); nothing in the fire code (0x72E000..
0x733000) calls a cell insert or remove, `CreateCollideData` / `ReleaseCollideData` or `GetCollideData` (direct calls,
or the slots vt +0x54C / +0x550 / +0x858 / +0x85C / +0x908), and `Object::StartOnFire` 0x419300,
`DeadTree::StartOnFire` 0x510E20, `MultiMapFixed::StartOnFire` 0x52EC60 and the `EndOnFire`s only add or remove
reactions. The wall hug, the circle hug and the footpath (`MobileWallHug`, `CircleHugStateInfoT`, `GFootpath*`) call
nothing of the fire's. So a walker meets a burning hut or tree exactly as an unburnt one, and the fire goes out with
nothing to clear. The only ways round a fire are the villagers' reactions: fleeing (215), going round it (220) or
fighting it (216).

**The three ways into `MOVE_AROUND_FIRE`** (`SetupMoveAroundFire` 0x75A770, called at 0x75A422, 0x75AD61, 0x7655EB
and 0x765B55):

- `ReactToFire` 0x765870 when it does not fight: the goal is the wall hug's destination (`GetDestPos` vt +0x860) and
  the state after it the saved one (+0x8E).
- `DecideHowToPutOutFire` 0x75A3D0 and `PutOutFireByBeating` 0x75AC50: to the fire fighting spot, then 216.
- `Villager::SetupReactToFire` 0x765540, for a villager **on its way to worship** (+0xE0 & 0x10): s = the final state
  (vt +0xB04), or the saved one (+0x8E) when s's row is never stored as previous (`Infos` +0x20 = 0xDB9E88, the row's
  file offset 0x10); then `AddReaction`(reaction, 215) (vt +0x990) as for anyone; then, when the object has a fire
  (+0x44), the villager's fire (+0x114) = it and `SetupMoveAroundFire`(`GetDestPos`, s). It walks round the fire
  towards the worship site instead of stopping to look at it. Not on its way: only `AddReaction`. The object (+0xBC)
  is stored before the null test; s and the flag are read before `AddReaction` (0x765570..0x7655B6), `GetDestPos`
  after it (0x7655E2). The walk round is the plain `MOVE_AROUND_FIRE` (below). openblack does the same:
  `villager_fire::SetupReactToFire` with `WorshipVillager::onWay`, the row's `keepsPreviousState` and
  `villager::GetDestPos` (tests `VillagerReactionsTest.OnItsWayToWorship...` and
  `NotOnItsWayToWorshipAVillagerStopsAtAFire`; testbed scene `miracles.fire_walk_round`).

`Villager::MoveAroundFire` 0x75A7E0, with `GetViaPoint` 0x75A440.

- **On arriving** it runs PopFromPrevious (0x75A815) and sets PREVIOUS = 163 **raw** (0x75A81A..0x75A827,
  `LivingAction::SetState` 0x5ECC90 with ecx = +0x8C: without the table +0x10 skip nor the town), not
  `Villager::SetState`.
- **Not arrived:** with no fire or no fire object it returns 0. The wall hug's goal (+0x80) takes the saved destination
  (+0x10C) first (0x75A85A..0x75A874). The margin is the villager's `Get2DRadius` (vt +0x64) + 1 (0x75A8AC).
- **Its own fire first.** `GetViaPoint` (cdecl, 9 arguments: from, to, centre, radius, out point, margin, the detour
  and inside flags, the side; returns the angle) is called first for the villager's fire: centre = the fire object's
  position (+0x14), radius = `GetSafeFireRadius` 0x730020, to = the goal (`GetDestPos`, vt +0x860), side 0. An angle
  ≠ 0 takes its point as the target and the angle as the side. With detour and inside after that first call:
  `Villager::SetState`(PREVIOUS, 163) (vt +0x938), goal = saved destination = the target (which stays the destination
  if the angle was 0), and it returns 1.
- **Then the group.** Otherwise it walks the group from its root (`GetFirstCaused`, then fn_00732AD0). A member whose
  call gives an angle ≠ 0, or detour and inside, takes the point and the angle (0 included) as the side, and the walk
  through the group starts again from the root. The 1000th restart (0x75AA20, `cmp 0x3E8; jae`) returns 0. Every
  member's object position is read without a check (a member with no object would fault at 0x75A9B6).
- **The walk.** The target is then the destination unless a detour changed it (fn_00603260), and it calls
  `SetupMoveToWithHug`(target, 220): 0 returns 0; else `SetStateSpeed` 0x753760 with the PREVIOUS state (+0x8E), and
  1.
- Neither function draws a random number. `SetStateSpeed` draws `GameFloatRand` (the synchronised stream) in its
  wounded and emergency branches, as on every state change.

**`GetViaPoint` 0x75A440:**

1. Radius and margin × 6553.6 (0x99A96C). d = |centre − from|. If d ≤ radius: inside, angle 0, no detour.
2. reach = radius + margin, s = reach / d, c = √(1 − s²) (not a number when reach > d, and then no detour: the
   `dot > c` test at 0x75A57C fails).
3. If |to − from| < 0.0001 (0x8BF518): nothing.
4. With the unit vectors u (to the centre) and t (the way), dot = t·u. If dot ≤ c the way clears the circle: nothing.
5. The side: cross = tx·uz − ux·tz; side A when (cross < 0 and side ≤ 0) or side < 0 (0x75A5AB..0x75A5DA), else B.
   A: point = centre + reach × (−c·uz − s·ux, c·ux − s·uz), angle −acos(dot); B: centre + reach × (c·uz − s·ux,
   −c·ux − s·uz), angle acos(dot). Each coordinate is `fiadd`ed to the centre's and truncated (`__ftol`). Detour set.
6. If |to − centre| < radius: inside, the angle. Else the angle only when |point − from| < |to − from|, otherwise 0.

The map-unit differences are loaded with `fild` and used whole in the squares and in ux / d and tx / length (the z
ones are stored as floats first). Checked natively against the exe (the function oracle, 6012 cases: hand-picked
ones, villagers near fires, anywhere on the map up to 2^25 units): every detour and inside flag and every point is the
same, and the angle's sign and zero are the same; only those are used.

#### Validating the reaction

`Villager::ReactionValidate` 0x756A00 is the "validate" column (+0x80) of the state table 0xD09198 in the reaction rows
(201, 202, 251, 215-218, 220, 6-30, 140-146). `Villager::ProcessState` 0x74FF91/0x74FFD9 runs it every turn, before
the state, for the top state (+0x8C) and the saved one (+0x8D). It calls `PopFromPrevious` 0x751E50 if:

- the reaction's object (+0xBC) does not exist or is not available (`GameThing::IsAvailable` 0x401810, vt 0x2C); or
- the `ReactionInfo` row (0xD4F6B0, `Reaction::GetInfo` 0x6E4709) asks for
  `whetherReactionFinishesIfInitiatorInHand` (+0x28) and the object is in the hand (+0x24 & 4).

`ReactToFire` 0x765870 and `GoToTeleportReaction` 0x7662F0 check nothing more; the first only returns 0 if the object
is not an `Object` or has no fire, without changing state.

`Living::ProcessReaction` 0x5F1270 also runs every turn: a reaction that is not available → `StopReacting`; an object
+0xBC that is null or not available, or past the turns of table 0xC09CF0 for its type → `StopReactingAndSetState`.

#### Leaving the reaction when the fire goes out

What takes the villager out of 215 when the object stops burning is not the state. The fire removes its
`REACT_TO_FIRE` when dropping below the reaction temperature (fn_0072EFB0 0x72F781), when being deleted
(`FireEffect::ToBeDeleted` 0x72EC4C) or when moving, with `RemoveAllReactionsOfTypeInitiatedByObject` 0x6E4780.

That calls `Reaction::ShutDown` 0x6E4720 on each one: +0x34 = 1 and, while there is any follower left (+0x1C),
`StopReactingAndSetState` (vt +0x99C, 0x5F11C0) of the first in list +0x18 (0x6E4731..0x6E4743).
`StopReactingAndSetState` is `ResetStateAfterReacting` 0x751E10 (`PopFromPrevious`, and `DECIDE_WHAT_TO_DO` if the
final state is a reaction one), then `StopReacting`. That way the villager goes back to what it was doing in the same
turn, whether it is in 215, fleeing towards 215 or putting out the fire.

A `REACT_TO_FIRE` can also be removed some other way: `Pot::RemoveReaction` 0x66D6A0 removes all those of an object.

#### Fire in the table

The static initialiser `_$E32` (0x5AA2D9..0x5AA767) sets +0x10 / +0x20 of each row:

| row | state | entry (+0x10) | exit (+0x20) | where in `_$E32` |
|---|---|---|---|---|
| 215 | REACT_TO_FIRE | — | ExitReaction 0x7527A0 (the thunk 0x5B0100 = `jmp [vt +0x910]`) | — |
| 216 | PUT_OUT_FIRE_BY_BEATING | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA2D9 / 0x5AA2EC |
| 217 | PUT_OUT_FIRE_WITH_WATER | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA421 / 0x5AA434 |
| 218 | GET_WATER_TO_PUT_OUT_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA4BE / 0x5AA4CB |
| 219 | ON_FIRE | EnterOnFire 0x75AF30 | ExitOnFire 0x75AF80 | 0x5AA611 / 0x5AA642 |
| 220 | MOVE_AROUND_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA75D / 0x5AA767 |

What they return (read in the disassembly):

- **EnterPutOutFire(final, s)**: 1 if `IsStateEntryFunctionSameAs(final, s)` 0x7524D0 (both rows have the same
  entry: from 216/217/218/220 to another of them). Otherwise, with a live fire (+0x114) (fn_0075AD90; if it no longer
  exists, +0x114 = 0), its vt +0x2C and a reaction (+0x94) not shut down (Reaction +0x34, set by ShutDown 0x6E4723):
  **0** if it is already in the root's firemen list (0x75AE75); if not, it adds it (AddFireman 0x7309A0) and gives
  **1**. In any other case it gives **0** and, if the previous final is reactive (table +0xB8, 0xDB9F30),
  StopReacting (vt +0x998). A 0 is 0x2F, and `Villager::SetTopState` enters 163.
- **ExitPutOutFire(s)**: always **1** (0x75AECB and 0x75AF20). If the exit is not "the same" (vt +0x96C, 0x752530:
  another row with ExitPutOutFire, or a non-final state) it leaves the firemen list (0x75AEDB, `RemoveFireman`) and the
  town's on-the-way-to-worship list (0x75AEEE, `Town::RemoveVillagerOnWayToWorshipSite` 0x73E360). If it was not in
  the list, only +0x114 = 0 and it returns without ExitReaction; otherwise, ExitReaction 0x7527A0 (ends the reaction
  unless `s` is reactive).
- **EnterOnFire(final, s)**: **1** without fire or with its vt +0x2C at 0; **0** if it is already in the list
  (0x75AF78); otherwise it adds it and **1**.
- **ExitOnFire(s)**: always **1**; it leaves the list if it is there and **+0x114 = 0 always** (it does not look at
  `s`; 0x75AF83..0x75AFBF, 0x75AFA4 stores the list's end, 0).

The state changes around them:

- The entries and exits also run when the final state does not change. When replanning or arriving
  (SetupMoveToWithHug / SetTopStateToFinal) a fireman in 220 goes through ExitPutOutFire(220) and
  EnterPutOutFire(220, 220), which change nothing. A villager in 219 goes through ExitOnFire, which **clears +0x114**:
  after its first walk it stops fleeing from other fires and only stays in 219 while it is itself burning (OnFire
  0x75B1E0 calls SetupMoveToWithHug 0x5F2890 at 0x75B39E).
- SetupMoveAroundFire 0x75A770 only stores the destination and next state if SetTopState(220) gives 1 (0x75A783).
- **PopFromPrevious** 0x751E50 (one for fire and teleport): SetTopState of the resume state of what was saved (Infos
  +0x30 0xDB9E98 = file 0x20); if it gives 0x2E, TOP = 163 raw (`LivingAction::SetState` 0x5ECC90) and afterwards
  PREVIOUS = 0 raw. **With nothing saved** it is row 0, whose resume state is **0** in info.dat: SetTopState(0) =
  INVALID_STATE (`Living::InvalidState` 0x5EC1D0 returns 0 each turn).
- **ResetStateAfterReacting** 0x751E10 (vt +0x9A0): PopFromPrevious and, if the final state is reactive (file 0xB8),
  SetTopState(163). **StopReactingAndSetState** 0x5F11C0 (vt +0x99C): that, and then StopReacting if it is still
  reacting. ReactToFire uses it at 0x765A48 (already a fireman) and TeleportReaction at 0x76642F (after the jump).
- The exit of MOVE_TO_POS is ExitMoveToPos 0x5EDDA0.
- **The pause 239 does not come out in fire nor teleport.** CanPauseForASecond 0x752120 looks at the row of the
  **destination** state (Infos +0xD4 in memory, 0xDB9F3C = file 0xC4), which is 0 in 163, 201, 202 and 215..220:
  those SetTopState do not roll the pause. It can only come out when PopFromPrevious returns to a state that pauses
  (a job with 0xC4 = 1) or in 248 (worship).

### The creature and a fire

How a creature answers a fire (the creature's own planning is on [the creature's mind](../creature/mind.md)):

- **The reaction row.** Reaction type 10 is ReactToFire (the member table 0xC09CC0 + 0xC0·t, built by 0x6E0E80: setup
  vt +0x9CC `SetupReactToFire`, priority vt +0xA60 `Creature::ReactToFirePriority` 0x4F2EE0, turns vt +0xAC0 /
  +0xAC4; +0x04 = 1, the same-type switch is allowed). Its info.dat row (`DETAIL_REACTION`, 0x29 rows at 0xD4F6B0,
  stride 0x64): priority 210, turns to react 100000 and before reacting again 0 (for creatures too),
  maxReactionDistance 35.0, howImportantIsDistance 0.0, whetherReactionFinishesIfInitiatorInHand 1, stealth 5. Every
  one of the 17 species reacts to type 10 (GCreatureInfo `IsReacting`, file +0x134).
- **The score** (fn_006E4620): 0 when the species does not react to the type or the distance is past the row's
  maximum; else `ftol(min(255, priority × (1 + 0.5 × howImportant × (R − d) / R)))`. For a fire that is **210 at any
  distance up to 35 m**. The creature's turns are `Creature::StandardNumGameTurnsToReactFunction` 0x4F29C0.
- **Nothing skips the fire's initiator.** `Reaction::ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 (from
  `SpreadReaction` 0x6E3F4B) never compares the living with the source; the MapShield test (fn_0072B990) is the only
  filter on the source's place. A creature that is itself the burning thing scores the full 210.
- `SetupReactToFire` ends with Creature +0xBC = the burning thing and `Creature::AddReaction(reaction, 0)` 0x4F2680
  (0x4F3064..0x4F3075). Before a forced action, `Living::StopReacting` 0x5F1140 clears +0x94 and +0xBC.
- **The town-fire initiative** (fn_0047CFB0, from `ProcessState` 0x47358C): a creature that burns casts water on
  itself (action 0xF0, with a water share of at least 1, or above 0.6 and paid) or else drinks from the sea (0x37);
  below life 0.4 it forces the "water" desire. Otherwise, with the nearest town within 150 m, the water miracle known
  and paid for, Compassion active and above Anger, and a burning building of that town (fn_004E4D00: a burning
  storage pit or town centre first, else the last burning one), it forces action 238 ("cast water to put out fire")
  on the building (0x47D176..0x47D2AE).
- Action 238 is never chosen by the free planner (`GetNthActionForPlan` 0x4F17F0 takes the town table for
  Compassion); it comes only from the fire reaction, the initiative above and the mimic row 34. Action 37
  (PutOutFire) is listed by no desire table, no mimic row and no forced call: nothing data-driven reaches it.

### CHL natives

| No. | Native | Address | Notes |
|---|---|---|---|
| 170 | `IS_ON_FIRE` | 0x6FB4C0 | |
| 171 | `IS_FIRE_NEAR` | 0x6F7910 | `FindNearForScript` with the predicate 0x6F7100; a fireball is not in the cells, so it does not find it |
| 174 | `SET_TEMPERATURE` | 0x6FB840 | → `Object::SetTemperature` 0x639A60 (no source) → 0x72EF10 |
| 175 | `SET_ON_FIRE` | 0x6FB780 | |
| 321 | `SET_HURT_BY_FIRE` | 0x6FDF40 | bit 2 of Object +0x0A |
| 426 | `SET_SET_ON_FIRE` | 0x6FDEE0 | bit 3 of Object +0x0A |

### Fire (trees)

The heat model common to all objects is [above](#the-heat-model); here is what it gives for a tree (the values of the
GTreeInfo table, on [Trees and forests](trees.md)). ✅ faithful.

- **Values.** Tc = max(110, 40); it burns if T ≥ Tc. defenceMultiplierBurn = 0.01 in the 22 tree types and in Tree
  Logs (info.dat), and Tmax = 2·Tc.
- **Heating and cooling.** Burning: T += 0.1·T/(2Tc) up to 2Tc. Cooling (T ≤ previous): T −= (T + 10 −
  amb)·4·H·R·0.1·k/capacity (k = 50 over water with y < 2, 1 + 0.01·rain).
- **Damage.** life −= (T − Tc)/Tc·0.001 per turn, so it dies in ~100 s. This is the general
  `(T − Tc)/(2·Tc − Tc) · defenceMultiplierBurn · 0.1` with the tree's values. It is charred with life < 0.6. At life
  0 the tree disappears.
- **Spreading.** Every turn it searches within R + 10 m, R = 1.25·radius2D·clamp((T − 0.8Tc)/1.2Tc). The heat
  q = min(10·dT, 0.5·(Ts − amb)·cap_s), and the target gains q/cap_t (the bushes catch fire ~10× sooner).
- **In the hand.** A burning tree can be picked up and keeps burning; held over something that is burning, or thrown,
  it sets fire to what it touches; when it falls it becomes a burning DeadTree.
- No lightning nor random fire.
- **Visual.** Colour × max(50, 255 − (1 − life)·2550)/256 (almost black on losing 8 %; capped by the brightness
  [0xC22FA0]); ALPHAREF forced to 230 + heat·25/255 (cap 254, `OverrideRenderMode`; see
  [the forced ALPHAREF](#the-forced-alpharef)); narrower by 1 − (0.2 − life)·5 below 0.2 ([the burnt colour](#the-burnt-colour)). Flames `FireGraphic` (sprites
  `S_Fire.raw`, smoke `S_SpriteSheet3.raw`, light `S_LMFireBall.raw`): 2 flames per tree of 0.2·height. A looping fire
  sound.

## openblack

Status: **partial**, 17 of 79 rows done, 57 partial and 1 inferred in
[the fire progress page](../project/progress/physics/fire.md) (81%).

- `src/ECS/Fire/FireEffect.cpp`: the fire list, the spread and the fire groups; `src/ECS/Fire/FireObjectTraits.cpp`:
  the per-class answers (who burns, who is destroyed).
- `src/Fire/FireModel.cpp`: temperature, heat, damage and charring, as pure functions.
- `src/ECS/Fire/FireGraphic.cpp`: the flames, steam and smoke; `src/ECS/Fire/FireSound.cpp`: the looping sound
  ([world sounds](../audio/world-sounds.md)).
- `src/ECS/Systems/Implementations/VillagerFire.cpp`: the villagers' fire states, with `Fire/ViaPoint` for going round
  a fire.
- Tests: `test/test_fire.cpp` and `test/test_fire_model.cpp`, pinned to the original's numbers (the oracle).

Porting decisions taken from raffclar's tree:
[Port decisions: engine and world](../project/port-decisions-engine-world.md#fire).

#### Where it is in the code

- `src/ECS/Fire/FireEffect.cpp`, `FireEffect.h`:
  - `CellObjects`: a snapshot of a 10 m cell's objects in the original's walking order (fixed list, then mobile,
    through `ecs::map_cells::FindType`); the held and flying objects are skipped.
  - `MoveFiremen` appends the old root's firemen to the new one's; `HandRootTo` hands a root's group to a member,
    which goes first; `StopFiremen` stops the group's firemen (all, or those fighting this fire).
  - `FireEffect::NearestFireToFight`: the group's member above its reaction temperature whose keep-off edge
    (the larger of the safe and object radius) is nearest to a position; the search starts from `1e7` (the float bits
    `0x4B189680`).
  - `MaxFireRadius` and `SafeFireRadius` (1.25 × the fire radius; min(fire radius, max) + 1), `HeatContent`
    ((T − Tamb) × capacity) and `AddHeat` (T += q / capacity, limited by magnitude).
  - `FireEffect::k_SoundPlaying` (0x20): the flag set before the sound starts. The sound's owner is an
    `audio::Owner::Object` number.
  - `RemoveReactions` calls `villager_fire::ShutDownReaction` before removing a `REACT_TO_FIRE` (the order of the
    followers is 🔍 inferred: by entity).
- `src/Fire/FireModel.cpp`: `HeatFromDifference` (0.1 × 100 × dT); a fire's reach, `Reaches` and `ReachesHeight`
  (test `FireModel.AFireReachesOnlyWhatIsStrictlyWithinBothRadii`); `FightFireWill` and `WillFightFire`, the
  villager's will to fight a fire ([Joining and choosing a fire](#joining-and-choosing-a-fire)), pinned to the
  oracle's cases in `test/test_fire_model.cpp`.
- `src/ECS/Fire/FireObjectTraits.h`: `AbodeInfo` (an abode's or a field's `GAbodeInfo`, null for any other object),
  `CombustionTemperature` (0 without an info, which refuses the fire), `HeatCapacity`, `DefenceMultiplierBurn`,
  `FireCentre` (the position, or a dead tree's mesh centre), `RainCoolingMultiplier` and `IsBurnReceiver` (who takes
  a burn).
- A fish farm takes its info row's fire values (combustion temperature 9999, heat capacity 50000, burn multiplier 0),
  so a burn never takes its life; when a fire or a miracle takes the last of its life it is deleted as any thing is,
  its fishermen sent away and its shoal with it, and its reactions go when it starts burning
  (`FireObjectTraits.cpp`, 2026-10-22; test `RepairsTest.AMiracleWearsAFishFarmDownAndItsLastLifeDeletesItStillAttackingItsTown`;
  [Fish farms](sea-life-and-scenery.md#fish-farms)).
- The reaction a water miracle or a storm's rain starts when it puts a fire out only impresses the villagers near it:
  a villager available for it is impressed once per its wait, takes nothing up, and the first one in a map cell ends
  that cell (`VillagerReactions.cpp`, 2026-10-22; tests `VillagerReactionsTest.OnlyThreeTypesOnlyImpress`,
  `VillagerReactionsTest.AFirePutOutByMagicWaterIsOnlyRecordedAndEndsTheCell`;
  [Reactions that only impress](../beings/villager-reactions.md#reactions-that-only-impress-34-39-and-40)).
- The creature: action 238 has its executor, `PutOutFireWithMagicWater` at `Target::Burning`, the water cast at the
  burning thing (test `CreaturePlanActionsCasting.AFireIsPutOutWithWaterScatteredOnTheThingThatBurns`), and a burning
  creature casts water on itself and puts its own fire out (`Build::CastWaterOnItself`); row 37 (`PutOutFire`) has no
  executor ([Casting](../creature/casting.md)).
- The creature takes up the fire's reaction ([The creature and a fire](#the-creature-and-a-fire)): the creatures'
  handler scores it with `creature_fire::Priority` (strictly within the two 2D radii, and its height over the burning
  thing's above 0.2; a burning creature passes for its own fire), then `TakeUpFire` stops the reaction it followed (as
  the original's forced plan does), and the mind's `ReactToFire` comes across the burning thing and what it belongs to,
  weighs that with `creature_fire::Choose` (usefulness to compassion times compassion above 0.05, the water miracle
  seen at least as often as needed) and forces `PutOutFireWithMagicWater` for compassion or `RunAwayFromObject` for
  fear on the burning thing, neither remembered as forced by its player; then it follows the reaction
  (`CreatureReactions.cpp`, `CreatureMindReactions.cpp`; tests
  `CreatureReactionHandlerTest.AFireIsTakenUpOnlyWithinTheBurningThingsReachAndItsOwn`,
  `CreatureReactionHandlerTest.ACreatureLeavesTheReactionItFollowedForAFireAndRefreshesItsRecord`, `CreatureFire.*`).
  The log line "Creature N takes up the fire on M" (game channel, debug) says which way it went.
- `src/ECS/Fire/FireGraphic.cpp`: `FlameCreator` and `SteamCreator` (the PSys creators: the flame's origin at its
  base, `SpriteOriginY = -1`, so oy = OriginY × size × stretch; steam and smoke centred), `LocalRandomFlamePosition` (a
  random point of a random LOD 0 triangle, trees × 0.5), `WorldFlamePosition` and `DrawnObjectPosition` (the object's
  drawn matrix and its translation), `UpdateFlames`, `UpdateSteam`, `UpdateSmoke` and `UpdatePuffs` (the emitters and
  the puffs' growth and wind pull, with the constants above), and `CollectFires` (one drawable per fire, flames then
  steam then smoke, for the PSys sprite path). The burst's start is `graphic::StartsSteam` (cooled this turn by water,
  rain or its own cooling, above 75 degrees and hotter than at its last burst) and a puff's step `graphic::StepPuffs`
  (3 s of life, 0.2 to 2.6 times its base scale, the alpha faded from its first value, the velocity eased by a tenth of
  the difference a second towards half the smoothed wind at the burning object, the rise), in the original's constants
  and order (tests `FireGraphic.ACooledFireAboveSeventyFiveDegreesSteams` and
  `FireGraphic.SteamAndSmokeDriftWithHalfTheWindAndRise`, 2026-10-22). Pure functions taken out unchanged, each with
  its test in `test/test_fire.cpp`: the bursts' end and puff count (`graphic::BurstOver`, `graphic::PuffsDue`), the
  steam's sizzle (`graphic::SizzleOptions`), the most flames and a flame's alpha (`graphic::MaxFlames`,
  `graphic::FlameAlpha`), the light map's choice (`graphic::GetsLightMap`) and a burnt tree's narrowing
  (`graphic::BurntTreeWidth`). The cells are `frame_anim::FireCell` and `frame_anim::SteamCell`; with
  64 frames `PSysFrameIndex` keeps them as they are. The flames follow the land through `land_morph::Raised`.
- Burning tree: `fire::graphic::BurningTreeAlphaRef` (float step by step) and `TreeAlphaRef`, put per instance in
  `RenderContext::forcedAlphaRefs` by the tree's row (`RenderingSystem`). The object pass draws each such instance on
  its own (`instance_runs::Split`) with `L3DMeshSubmitDesc::forcedAlphaRef` → `render_modes::PrimitiveAlpha`
  `forced` → `u_skyAlphaThreshold.y`, and a fading one drawn at once too; a queued one is not forced. The tint is
  capped by `ecs::TreeBrightness`.
- `src/ECS/Systems/Implementations/VillagerFire.cpp`, `VillagerFire.h`:
  - `FinalState` (the original's `GetFinalState`), `IsFireMan`, `StopFireFighting`, `FinishBeingOnFire`, `AddFireman`
    (the root's firemen list) and `FireFightingPosition` (the beating spot).
  - `villager_fire::MoveAroundFire` with `Fire/ViaPoint`.
  - `SetupMoveAroundFire` enters `VillagerStates::MoveAroundFire` and stops when `SetTopState` does not give 1; its
    check of the town before `VillagerStates::ArrivesAtWorshipSiteForWorship` is a guard standing for the town
    unlinking openblack lacks.
  - `villager_fire::ReactToFire` decides to fight a fire through `fire::WillFightFire` (`Fire/FireModel`), with the
    group's burning radius and priority (`GroupBurningRadius`, `GroupBurningPriority`) and the root's fire fighters.
  - `ApplyFireReaction`: a villager of a cell with no reaction of its own starts reacting to a fire whose score is
    above 0 (`SetupReactToFire`); the shield test is done for every living class by `reactions::SpreadReaction`.
  - `PutOutFireWithWater`: rows 217 and 218 go straight to `DECIDE_WHAT_TO_DO`, as in the original.
  - `villager_fire::SetupReactToFire` also skips an unavailable object (a guard; the original tests only null).
- `src/ECS/Systems/Implementations/VillagerReactions.h`: `SetSlotObject` stores the reaction's object after
  `AddReaction`. `villager_reactions::ReactionValidate` is the original's `ReactionValidate`, and
  `villager_reactions::PopFromPrevious` is the one PopFromPrevious for fire and teleport.
- `LivingActionSystem::VillagerCallValidate` calls the validate on every row without its own validate whose original
  validate is 0x756A00 (`VillagerStateFunctions.h`); the custom 🔍 inferred exits of `ReactToFire` and
  `GoToTeleportReaction` are gone (details in [Villagers: data and states](../beings/villager-states.md)).
- The state changes of `VillagerFire.cpp` and `VillagerTeleport.cpp` are those of the core: `villager::SetTopState`
  (with the pause roll, the exit of TOP and of the final and the entry, once each, and the codes 1 / 0x2E / 0x2F; both
  go through `villager_reactions::SetTopState`, which also ends openblack's walk), `villager::SetState(2, s)` for the
  saved state (vt +0x938: skips those of table +0x10 and adjusts the town) and `villager::SetupMoveToWithHug`. There
  is no longer a local SetTopState, nor `CallEntry` / `CallExit`, nor `villager_fire::CallFinalStateExit`; the worship
  exits (58, 59, 60, 213) run through their rows. The arrival of `MOVE_AROUND_FIRE` and `GO_TOWARDS_TELEPORT` calls
  `villager::AreWeThere` (whole units).

<a id="the-heat-model-1"></a>
<a id="the-creature-and-a-fire-1"></a>
#### Differences from the original

Each of these is open in [Pending](#pending) unless it says it was fixed.

- **Burning damage to a creature.** openblack takes the body's life off; the fight health of a creature with an
  opponent is not taken off yet.
- **Updating the graphic.** The original updates a fire's graphic only when drawn, with a catch-up; openblack updates
  every fire ⚠️ approximate.
- **Steam and smoke origin.** openblack draws them centred (origin 0) ⚠️ approximate: `particles::manager::DrawAtom` has no
  per-atom origin.
- **The light map's noise.** openblack uses two sines for VLNoise 0x590C30, leaves out the turn fraction and uses the
  fire's id for the pointer ⚠️ approximate.
- **`GetViaPoint` arithmetic.** openblack does those few operations in double and rounds once, the rest in float (the
  FPU at 24 bits). 22 of the oracle cases are pinned in `test/test_fire.cpp`.
- **The creature's reactions.** Fixed: the living that started a reaction is offered it like every other
  (`effects::reactions` spread), so a burning creature takes up its own fire.
- **What a burning thing belongs to.** The fire reaction weighs the owner `CreatureMindSystem::BelongsTo` gives: a
  villager's, abode's or field's town, an animal's flock, a creature itself. The original's
  `Creature::AddBeliefAboutAssociatedObject` 0x4D8BB0 also gives a tree its forest (else the nearest forest with trees
  within 1000 m), a temple part its temple and a totem statue its town, and none for a town, flock or forest being
  deleted. openblack's forests are no entities a belief can be about, so a creature always runs from a burning tree,
  temple part or totem statue. Open in [Pending](#pending).
- **The beating's last test.** Before a blow the original asks whether the fire itself is still available (the fire's
  vt +0x2C, 0x75ACC0), openblack whether the burning object is (`villager_fire::PutOutFireByBeating`). The two agree:
  an object being deleted takes its fire with it in both (`Object::ToBeDeleted` 0x636670; openblack's `ToBeDeleted`
  deletes the object's fire), so no beating meets a live fire on an object being deleted. The original's return of 0
  with no fire object (0x75ACDA) is not reached: the beside test before it (fn_0075ABA0) already fails without an
  object, and both then go to DecideWhatToDo.
- **A reaction removed another way.** `Living::ProcessReaction` 0x5F1270 runs for the fire's reaction too, every turn
  of a reacting villager before its state (2026-10-25, `villager_fire::ProcessReaction`, called by `ProcessReaction` in
  `src/ECS/Villager/VillagerCore.cpp` after the slot reactions and the shield's): the reaction no longer available ->
  `StopReacting`, the state kept; its object none or gone -> `StopReactingAndSetState`; longer than the standard
  turns to react at its distance to the object (row 2's turns, vt +0xAC0 through 0x6E4FE0, read in the oracle after
  the type table's initialiser 0x6E0E80) -> `StopReactingAndSetState`. Before, a `REACT_TO_FIRE` removed other than by
  the fire was ended by `ReactToFire` with `StopReactingAndSetState`, and the reaction had no time limit. Test
  `VillagerFire.AFiresReactionEndsAfterItsTurnsToReactOrWhenItIsNoLongerAvailable`. The teleport's and the death's
  reactions are processed the same way ([teleport](../magic/teleport.md),
  [mourning](../beings/villager-hand-and-death.md)).
- **The walk a reaction's SetTopState leaves** is kept, as the original's: its exit of MOVE_TO_POS (ExitMoveToPos
  0x5EDDA0, ported as `VillagerExitMoveToPos`, see [the function table](../beings/villager-states.md#the-function-table))
  only lets go of the circle, and the walk goes on if the villager comes back to it.
- **Dead villagers.** The Villager's refusal of a heal when dead is ported (2026-10-25, `effects::IsEffectReceiver`):
  a dead villager takes no heal, and any other effect only while it can be reached.
- **Not ported:** a creature's 3 flames (to do, M8); `Pot::EndOnFire`'s reaction; the 0.3 flame scale of impressive
  objects and citadel parts.

Fixes pinned to the original:

- **A burnt building.** A burnt abode's `DestroyedByEffect` (0x403F80) is the abode's own:
  `fire::traits::DestroyedByEffect` calls `abodes::DestroyedByEffect`, as an effect's kill does; before, a burnt house
  stayed at life 0 (test `DeadlistBuildingsTest.AFireThatBurnsAHouseDownDestroysItAsAnEffectDoes`).
- **A burnt tree** goes through the tree's own deletion, `ecs::DeleteTree` (test
  `BurntTree.ATreeBurntAwayGoesThroughTheTreesOwnDeletion`, [What each class does](#what-each-class-does-with-fire)).
- **The fire's player.** A fire set on an object is credited to the object's own player as its class answers it,
  `fire::traits::PlayerOf` (test `FirePlayer.AFireIsCreditedToTheObjectsOwnPlayer`, [Starting a fire](#starting-a-fire)).
- **The crackle's slot.** A fire taking a slot stops the sound of the fire it held, the same fire too (test
  `FireSound.AFireRetakingItsOwnSlotStopsItsCrackleFirst`, [The fire's sound](#the-fires-sound)).

- **A worshipper meets a fire.** A villager on its way to worship that takes a fire's reaction walks round the fire at
  once towards where it was going, then goes on to worship; before, the port stopped it in 215
  ([Going round the fire](#going-round-the-fire)).
- **Facing the fire.** A villager reacting to a fire, or beating one, turns to the burning object a step of an eighth
  of a turn a turn, and decides or strikes only once it faces it; before, the port faced it at once
  ([Joining and choosing a fire](#joining-and-choosing-a-fire)).
- **The burnt tree's tint** is capped by the frame's tree brightness; before, the port used 255, and at night the
  burnt tree came out lighter.
- **The charring grey** rounds up (`255 − ceil(175k/256)`); before, the port truncated and gave 81 at k = 255.
  `test_fire` compares it with the exe's integer code for all 256 k.
- **The forced ALPHAREF** is ported with its range 180..254; `test_fire` pins 18 of the 63 oracle cases.
- **The firefighter's spot** is at the maximum of the two radii; before, the port took the minimum, and the villager
  went back and forth 216 ⇄ 220 every turn. The arrival step is `WallHug::speed` (before, 1 m). Checked: Land1,
  `OPENBLACK_TEST_FIRE="1785.2.2652.6.450,abode,20"`, `OPENBLACK_VILLAGER_TRACE=1`: 13 changes 216 → 220 and 25
  220 → 216 over the whole life of the fire (before, 3213 in 650 turns), each villager dozens of turns in each state.
- **Leaving 215.** `villager_fire::ShutDownReaction` ends the followers' reaction when the fire removes it, as
  `Reaction::ShutDown` does.
- **PopFromPrevious with nothing saved** gives INVALID_STATE as in the original; previously 163 was invented.
- **StopReactingAndSetState** runs PopFromPrevious first; before it was the other way round (first StopReacting and
  then PopFromPrevious).
- **The entries and exits** run also when the final state does not change, as in the original (checked in a game).

### Test hooks

- `OPENBLACK_TEST_FIRE="x,z,T[,kind[,turn]]"` sets the temperature T of the nearest object with fire values to
  (x, z) (`kind`: any, tree, abode, villager or field) at that game turn (by default the land's first); with T ≤ 1 it
  sets the object on fire with that fraction instead (`src/ECS/Fire/FireDebugHooks.cpp`). It runs once per land.
- `OPENBLACK_FIRE_TRACE=1`: the fire's log (new fires, ignitions, deaths, reactions, villagers);
  `OPENBLACK_VILLAGER_TRACE=1` logs the villagers' states around a fire.
- The testbed scene `realinput.fire` (no pointer commands): a burning beech close in front of the camera and a kind
  tiger that knows water 40 m north of it. Played through `tools/checks/real_input_tap.sh`, the hand carries the tree
  to the tiger, and the log's "Creature N takes up the fire on M" line shows the creature's fire reaction. A first
  guess at the steps, to be checked against the run's shot: `press:120:512:400:3:150 move:170:512:300 move:200:512:200
  move:230:512:130` and 900 frames.

## Pending

- **Verify with xref/oracle:** the **🔍 inferred** and **⚠️ approximate** marks in The original above. Each is settled
  by reading the function named next to it (`python tools/re/xref/xref.py show <function>`) and, where a number is in
  doubt, by an oracle run of that function (`tools/re/oracle/GUIDE.md`).
- **The burning tree's forced ALPHAREF** ([the forced ALPHAREF](#the-forced-alpharef)): not yet seen in a capture (a
  burning tree by day, its foliage cut down to the texels of alpha 180..254). The rest of the fire's render items of
  the same list stay open: the newest-first walk of the fire graphics, VLNoise for the charring glow, the update only
  when drawn, the light map `S_LMFireBall` under a MultiMapFixed object ([the light map](#the-light-map)). The
  villagers' `Living::ProcessReaction` 0x5F1270 runs for the fire's reaction since f6dd0af00 (see "A reaction removed
  another way" in [Differences from the original](#differences-from-the-original)).
- **The owner of a burning tree, temple part or totem statue** ([Differences from the
  original](#differences-from-the-original)): the creature's fire reaction needs the owner beliefs of
  `AddBeliefAboutAssociatedObject` 0x4D8BB0 that `BelongsTo` lacks; the forest needs forests a belief can be about.
- **Fire passed on** ([Passing a fire to another object](#passing-a-fire-to-another-object)): the scaffold tap split
  does not yet `CopyFire` to the new scaffold. (`MoveFire` needs no caller: openblack's dead tree is the tree's own
  entity, so a burning tree that falls burns on as its dead tree with nothing to move, test
  `FireWorld.ABurningTreeThatFallsBurnsOnAsItsDeadTree`; a split rock's halves take its fire through `CopyFire`,
  `object_physics::SplitRock`.) A bonfire has no fire to pass: it is never placed in the hand nor put into the physics
  (`object_physics::IsBonfire`), and a creature's blow or shatter splits it into two bonfires with their own flames
  ([What a split makes](../physics/physics-impacts.md#what-a-split-makes)).
- **Going round a fire** (`MoveAroundFire` 0x75A7E0): the original reads every group member's object position without
  a check (0x75A9B6); openblack skips such a member. Whether a member can be without an object there is not
  determined. The FPU is taken at 24 bits on this path, as for the rest of the game logic, without tracing the control
  word to this call. The saved destination and the positions are held in float metres and converted to map units for
  the search, so a position can be one unit off the original's ([Going round the fire](#going-round-the-fire)).
- **Render modes:** whether the dispenser's stone ring should cover the fading bubble is not yet read. The forced
  ALPHAREF's other writers (the town artifact, the scaffolds, the ghosts' cut-off of 10, FragMesh) are not ported; the
  burning tree's is ([the forced ALPHAREF](#the-forced-alpharef)).
- The order of the fires between them when the steam and smoke borrow the last flame's origin is not read
  ([Steam and smoke](#steam-and-smoke)).
