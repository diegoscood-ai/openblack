# Object size and map cells

## Summary

Two foundations that almost every game system leans on. **Object size**: how big an object is (its 2D radius, its
radius, its height, its bounding sphere), measured from its mesh's box, with per-class overrides (a field is always
5 m wide, a food pile shrinks as it empties). **Map cells**: the original's 512 × 512 grid of 10 m cells, each holding
two sorted lists of the objects in it (fixed and mobile), which every search, collision test and script query walks.

openblack ports both **faithfully**: the size API in `src/ECS/ObjectMetrics.{h,cpp}` (`openblack::ecs::object`),
used at the level (mesh or object) the original uses at each place, and the cell lists in `src/ECS/MapCells.{h,cpp}`
(`ecs::map_cells`), kept in the original's order by insertion hooks and a per-turn sync.

## The original

### Object size

These are the virtual functions of `Object` that give the 2D radius, the radius and the height of an object from its
mesh's box: about **540 calls** in the original (151 to vt+0x64, 48 to vt+0x60 and 343 to vt+0x42C, a heuristic
count). Everything is in float (the FPU at 24 bits, fn_007DEE00), without double or FMA.

#### The mesh box

`LH3DMesh::ComputeBoundingBox` **0x8081B0** (on load) joins the boxes of all the submeshes and stores in +0x18..+0x20
the centre, in **+0x24 / +0x28 / +0x2C the half-extents** `(max − min) × 0.5` [0x8AA3B4] (0x80831C..0x80835B) and in
**+0x30 the half-diagonal** `√((hz² + hy²) + hx²)` (`fld hx; fld hy; fld hz`, then hz·hz + hy·hy + hx·hx and `fsqrt`,
0x80835E..0x808379). An animated mesh (flag +4 bit 0x100) first goes through `LH3DAnim::SetTransform` 0x83A1D0.

#### Two levels

The original has both, and they do not give the same result:

- **Mesh level**: reads the fields inline, without going through the vtable, so it **does not see any override**.
  `IsSuitableForFixed` 0x603E1E..0x603E5B, fn_00604020 0x604042, `Scaffold` 0x6E956A and 0x6EAC14..0x6EAC56,
  0x7350A5, the creature 0x4778E9..0x4779B1; half-heights in `Field::Draw` 0x5287B9..0x5287D3, `PhysOb::Initialise`
  0x7FB7D9, `Tree::Draw` 0x74ABB0, `WorshipTotem::Create` 0x780995, `CitadelHeart` 0x4653FE / 0x467777,
  `Abode::DrawPercentFull` 0x407111, `TownArtifact::Draw` 0x51C9A3 and others. There a field measures what its mesh
  measures.
- **Object level**: the virtual call, with the table of overrides of the `??_7` vtables in the symbols that derive
  from `Object` (a sweep of slots +0x60, +0x64, +0x120, +0x13C, +0x42C, +0x568, +0x590, +0x5F4, +0x630, +0x64C,
  +0x6C4, +0x798 and +0x7C4 in all the vtables). The classes that are **not** `Object` are not covered: the `Citadel`
  0x8C7E68 (and the `Planned*`, `SpellSeedGraphic`) keeps `GameThing` 0x405140 / 0x405150 = 0,
  `GameThingWithPos::GetHeight` 0x405500 = 0 and `GetScale` 0x4247E0 = 1; `SpellShield` 0x72B440
  (`GetSpellMagnitude` 0x7202C0) / 0x72B450, `SpellStormAndTornado` 0x72D950 / 0x72D960, `Town` 0x73D6E0, `GArena`
  0x424780, `Reaction` 0x55C7D0, `BuildingSite` 0x43D050 and `AtomCore` 0x673C70 have their own `GetRadius` /
  `Get2DRadius`; `GStreetLight` 0x735110 (radius 20 [0x8C7658], fn_00735060) and `Mist` 0x6067D0
  (`Mist::Get2DRadius` 0x606660) their own `GetDistanceFromObject(MapCoords)`.

A port must use, at each place, **the level the original uses there**: a single-level API would put a 5 m field into
`IsSuitableForFixed` or into the construction sites.

#### The size functions

| Level | Original | What it does |
|---|---|---|
| **Mesh** | | |
| half-extents | +0x24/+0x28/+0x2C, 0x80831C..0x80835B | `(max − min) × 0.5`; without a mesh, nothing |
| half-diagonal | +0x30, 0x80835E..0x808379 | `√((hz² + hy²) + hx²)`, in that order |
| 2D radius | 0x6381B1..0x6381E2; inline at 0x603E1E, 0x604042, 0x6E956A, 0x6EAC22, 0x7350A5 | `s × max(hx, hz)` (`fld [m+0x24]; fld [m+0x2C]; fcompp; test ah, 1` takes hx if hz < hx, 0x6381CB / 0x6381DF) |
| height | 0x638136..0x63813D; inline in `Tree::Draw` 0x74ABA2..0x74ABC0 | `2 × (hy × s)` (`fmul` and then `fadd st0, st0`) |
| half-height | 0x5287C5, 0x7FB7D9, 0x74ABB0 | +0x28, unscaled |
| **Object** | | |
| scale | vt+0x120: Object 0x402520 = the +0x50 field; Creature 0x47B190 → `GetUserSize` 0x4EF4F0 | a `MapShield`'s is its object scale (`SetScale` 0x639200), not the drawn one. The creature's is `GetUserSize`'s 3D body +0x90, the drawn size (the pen size in its pen, else the real size) |
| the +0x50 field | read by `GetHeight` (0x638139) | not the virtual scale. The citadel heart's +0x50 is the plan's scale (`CitadelHeart::Create` 0x464E20 → the MultiMapFixed constructor 0x52E1E0); its drawn temple's is 1.0 (0x46761F) |
| `Object::Get2DRadius` 0x638180 itself (called directly by `PileFood` at 0x66F192) | | `GetScale` (vt+0x120, 0x63818E) `× max(+0x24, +0x2C)`; without a mesh 0 (0x6381E9) |
| `Object::GetHeight` 0x638120 itself | | `2 × +0x28 × [+0x50]`; without a mesh 0 (0x638140) |
| `Get2DRadius` | vt+0x64 | Field 0x528E80 and FishFarm 0x52C470 = **5** [0x8AB6E4]; `MagicTeleport::Get2DRadius` 0x5FCCB0 → fn_005FCCA0 = **6** [0x92C108] (one reader, 0x5FCCA1); `MagicFireBall::Get2DRadius` 0x682D20 = `GetScale × 1` [0x935910] (one reader, 0x682D29); PileFood / MagicFood / PuzzleGrain 0x66F180 = `GetProportionRaised` (vt+0x86C) × `Object::Get2DRadius` (`fmul` 0x66F197); Creature 0x477F40 = LH3DCreature +0x5228 ([The creature's radius](../creature/body.md#the-creatures-radius)); the rest, 0x638180 |
| `GetRadius` | vt+0x60: Object 0x638110 = `jmp [vt+0x64]` | the same as `Get2DRadius` (Creature 0x4792C0 repeats its read) |
| `GetHeight` | vt+0x42C | MagicFireBall 0x682D30 = `jmp [vt+0x64]`; Creature 0x477F50 = size × **15** [0x8C2C40] (the size being the scale: **🔍 inferred**); the rest 0x638120 (Field, FishFarm and PileFood do **not** change it) |
| `GetTopPos` | vt+0x630: Object 0x638160; MapShield / MagicShield / PhysicalShield 0x72C1C0 = **0** | the altitude above the ground (+0x1C) + `GetHeight` |
| `GetHeightForHandAboveInteractObject` | vt+0x64C: Object 0x638150 = `jmp [vt+0x42C]`; FishFarm 0x52C840 = **5** [0x8AB6E4] | |
| `GetMeshRadius` | vt+0x568: Object 0x636BD0 = +0x30 unscaled; Field 0x528A30 / FishFarm 0x52C480 = 5 | |
| `GetProportionRaised` | vt+0x86C: PileFood 0x66EB60, PileWood 0x66F1B0 | below |
| **Derived** | | |
| `GetHoldRadius` | Object 0x638C00: ABOVE (`GetHoldType` = 1) → `GetHeight × 0.75` [0x8AB274] (0x638C14); otherwise `Get2DRadius`; Tree 0x74B610 / DeadTree 0x5110E0 = `Get2DRadius × 0.2` [0x8AB244] (0x74B612 / 0x5110E2) | SpellSeed 0x728640 (`GetScale × info+0x150`) |
| `GetDefaultFireRadius` | Object 0x639AC0 = `jmp [vt+0x64]`; DeadTree 0x510E10 = `GetHeight × 0.35` [0x8D6974] (0x510E12); WorshipSite 0x77DE10 → 0x77DDD0 = **14** [0x99C9EC] | |
| `GetVillagerHugRadius` | Object 0x4026B0 = `Get2DRadius × 1.05 + 0.0005` [0x8AA3A0] (0x4026B5) [0x8AA39C] (0x4026BB); Tree 0x74A1A0 = `min(Get2DRadius × 0.1, 0.25)` [0x8AB22C] [0x8AB3D4] (0x74A1A5..0x74A1CA: above 0.25 → 0.25, else the product again) | |
| `GetRoutePlanRadius` | vt+0x7C4: Object 0x6384C0 without a creature = `Get2DRadius` (0x6384CF); Tree 0x74A140 (a copy of 0x74A1A0); CitadelHeart 0x4680C0 = `Get2DRadius × 0.33` [0x8CA268]; with a creature 0x6384D8 ([The nav radius and the route plan](../creature/body.md#the-nav-radius-and-the-route-plan)) | |
| `GetDistanceFromObject(Object*)` | vt+0x6C4: Object 0x637FB0; WorshipSite 0x77DE20 | `GetDistanceInMetres` 0x74CD70 (0x637FC1) − (R2D(b) (0x637FD1) + R2D(a)): the radii added first (0x637FDC..0x637FDF) and the sum subtracted from the distance (`fsubr` 0x637FE5). The worship site measures from `CalculateCentrePos` 0x77DD40 and subtracts `14 + R2D(b)` (`GetRealRadius` 0x77DDD0; 0x77DE36..0x77DE60) |
| `GetDistanceFromObject(MapCoords)` | vt+0x13C: 0x5702B0 (Object 0x4027C0 calls it) | `GetDistanceInMetres` (0x5702BC) − `GetRadius` (vt+0x60, `fsubr` 0x5702CF). No `Object` class overrides it |
| `IsTouching` | 0x637E00 (`≤ m`, 0x637E0D..0x637E16), 0x637E30 (`≤ 0`, `fcomp [0x8AA398]` 0x637E3D) | through the two above, with their overrides |
| `GetBoundingSphere` | vt+0x798: Object 0x637730; Living 0x5ED2F0; MobileStatic 0x608F40 | `h = GetHeight × 0.5` (0x63773C); `r = √(R2D² + h²)` (0x63774D..0x637762); centre = that of the MapCoords (x, z = the integer × 10/65536, 0x637781..0x637795) with `y = (GetAltitude 0x803090 + altitude) + h` (0x637771..0x637798). Living (villagers, animals) and MobileStatic (rocks, dead and felled trees, bonfires, fragments, teleport stones) use `R2D × 0.5` (0x5ED30D / 0x608F5D). Creature 0x479970 → `LH3DCreature::GetBoundingSphere` 0x47F8D0 |
| `WorshipSite::CalculateCentrePos` 0x77DD40 | | `right × 12.55 [0x99C9E8] − forward × 26.1 [0x99C9E4] + position`, per component (0x77DD61..0x77DDB1), then made a MapCoords (0x77DDBD). The matrix is that of the site's object (🔍 inferred: `[this+0x40]+0x14`) |

#### Positions around an object

All with `Get3DAngleFromXZ` and `GetPosFromAngle` ([GUtils angles](coordinates-and-angles.md#gutils-angles)):

- `Object::GetNearestPosOfObject(Object*)` 0x636D30: this + GetPosFromAngle(angle to the other (0x636D44),
  R2D(other) + R2D(this)) (vt+0x64 of both, 0x636D54 / 0x636D5F).
- The nearest edge towards a position (0x636DB0..0x636DE1): this + GetPosFromAngle(angle, R2D(this)); with an extra
  reach (0x636DF9..0x636E1F): R2D + extra from the object's MapCoords.
- `Object::GetWorkingPos(Object*)` 0x639550: this + GetPosFromAngle(angle (0x639564), R(this) + R(other)) with
  `GetRadius` vt+0x60 of both (0x639574 / 0x63957F), not `Get2DRadius`; the sum at 0x605520.
- `Tree::GetWorkingPos` (0x74C051..0x74C08D): R2D(other) + 0.9 [0x8C5844] (0x74C064).
- The big forest's arrival position (0x439373..0x4393AF): `GetRadius × 0.5`.

#### How full a pile looks

**GetProportionRaised** (0x66EB60, food): `p = amount / maxAmountInPot` (an unsigned 64-bit `fild`, `fidiv`;
0x66EB79..0x66EB85); p < 0 → 0 without a floor (0x66EB98); p > 1 → 1 (0x66EBAF); **p = 0 stays at 0**
(0x66EBB7..0x66EBC2); otherwise `p = (1 − 0.05)·p + 0.05` [0x933014] (0x66EBCA, `fadd` 0x66EBD2). It returns
`1 − (1 − p)²` (`fsubr 1` 0x66EBD8) clamped to 0..1 (0x66EBE2..0x66EC11). The wood one (0x66F1B0) applies the floor
if p > 0 (`test ah, 0x41` 0x66F1E1; 0x66F1EC) and clamps (0x66F1FA..0x66F222), without the square. With
`maxAmountInPot = 0` the original gives inf (→ 1) or NaN (→ 0), the same as dividing by 1.

#### Where each level is used

`Tree::Draw` (the crown when bending, 0x74ABA2..0x74ABC2) reads the mesh level; the height of the tallest tree
(fn_0053A740, 0x53A75D), the felled tree (0x5116C2) and the rustle of more than 10 m (0x74B1CF) the object level.
The field's sinking adds `2·v·scale·[m+0x28]` with the **half**-height (0x5287C2..0x5287D1). The animals'
`GetWorkingPos` 0x639550 uses vt+0x60, the town's `CheckForClearArea` 0x741457 and the forests' `AddTreeAround`
0x439220 vt+0x60. The storm's `CanSuckUp` (0x6D214C) and its search (0x6D238A) use `Get2DRadius`; the forest spell
fn_005FADF0 (0x5FAE40) `Get2DRadius`; the seed fn_006022C0 the mesh 2D radius of its info's mesh × `GetScale`
(0x6022D9), `GetTopPos` vt+0x630 at 0x602388 and `Get2DRadius` vt+0x64 at 0x6023E9 / 0x6023F4; the shield
`Get2DRadius` 0x72B908 / 0x72B9C2 and `GetHeight` 0x72B948; `ApplyEffectToMapPos` `GetHeight` vt+0x42C at 0x52536E;
the heal rule vt+0x64 at 0x6A0DC3; the flock spell `fn_006D0C20` vt+0x60; the dispenser 0x722B46; the pot's size
0x66E90A / 0x66E918; the abode 0x40327E / 0x40329A; the hand's hold parameters vt+0x42C, the seed's with the mesh of
its info through `Object::GetHeight` 0x638120, and the radius vt+0x64 like `Object::GetHoldRadius`
0x638C22..0x638C26. The fire centre of a worship site is `GetDefaultFireCentrePos` 0x77DDE0 (= `CalculateCentrePos`
0x77DD40, the altitude above the land via `Set` 0x603340), with its 14 m radius (0x77DE10).

The body's rest counter: `PhysOb::Initialise` 0x7FB7D9 reads mesh+0x28, the drawn mesh's half height y, × scale ×
1000, rounded to nearest by the `fistp` at 0x7FB7F1; 0x5F0007 in `Villager::SetUpPhysOb` is `GetHeight` for the
body's shape, not a second half height. The fragments' half height comes from the Rock info MS[2] = 0xD3A930's mesh,
scale 1 (0x76E9E4 / 0x76EA32). The physics broad phase's box has the half size `|v.xz| · 0.1 + R` with R = PhysOb
+0x150, the 3D radius (fn_006E8160 is the xz length; RaiseUntilNotIntersecting uses R alone); the water ring of a
thrown object takes +0x178 = PhysOb +0x150, the body radius, with no guard.

The hand above a locked pile (`ObtainRequiredHandPosition`): with `IsLockedInInteract` (vt+0x6A0, 0x5B3EB3) the
original does not measure the pile with `GetHeight`. It takes the position stored in CHand+0x78, converts it to
MapCoords with `ftol(x · 65536 · 0.1)` (0x5B3ECD..0x5B3F26; it is `ToFixed`: multiplying by 2^16 is exact and
0x3DCCCCCD · 2^16 = 0x45CCCCCD = 6553.6f, so it rounds the same real number as `x · 6553.6f`, as in fn_004427B0),
measures `GetAltitude` (0x5B3F3B → esp+0x30), calls `GetHeightForHandAboveInteractObject` (vt+0x64C, 0x5B3F49) and
jumps to 0x5B3FDE, where `fadd [esp+0x30]` adds the two and stores the sum (0x5B3FE5): the hand's height is the
ground at the held position plus the object's height above for the hand [V].

<a id="object-lists-per-cell-ecsmap_cells"></a>
### Object lists per cell

The original's GMap grid (g_game+0x59B8, 8-byte MapCell at +0x59FC, 512 × 512 by `GMap::Init(0x200, 0x200)`
0x6014C0): each cell has **two linked and sorted lists**, +0 the mobile one (`SetFirstObjectMobile` 0x601B60) and +4
the fixed one (0x601B70). Object +0x24 bit 0 says "in the map" (0x636798 / 0x6367B7).

#### Which list

It is decided by the **type** of the info (+0x10), not the class: `DoesObjectTypeCountAsFixed` 0x601510, table
0x60152C (fixed: 0, 6-9, 11, 12, 14, 18, 19, 21-26, 28, 29, 31-41, 43, 44; above 0x2C, also −1 and −2 unsigned,
not). `InitialiseIsFixedForMapList` 0x63A640 stores it in bit 15 of Object +0x24. The type is read from info.dat.
MagicTeleport 0x5FC130 (GMobileStaticInfo 0xD3B614), Fragment 0x76E9D0 (0xD3A930) and DeadTree (a Rock, whose
constructor 0x6E6F70 takes a GMobileStaticInfo) take a GMobileStaticInfo row, as does `GStreetLantern::Create`
0x7346E0. The one-off orb's constructor 0x72A3A0 gives `MobileObject` the `GMobileObjectInfo` 0xD39F3C, row 25, whose
type is 20 MOBILE_OBJECT.

#### Which end

| Class | Insertion | End |
|---|---|---|
| SingleMapFixed (Tree, MagicTree, MapShield, ScriptHighlight: its `CallVirtualFunctionsForCreation` 0x709AAD calls SingleMapFixed's 0x52E880) | 0x52E620 → Fixed 0x52E530 → 0x636740 → SingleMapFixed 0x52F440 → `Fixed::InsertMapObjectToCell` 0x52DEA0 | head of the fixed list, its cell |
| MultiMapFixed (Abode 0x403EE0 = `jmp 0x52E650`: houses, fields, storage pits, town centres; Feature, AnimatedStatic, MobileStatic, DeadTree, BigForest, TotemStatue, WorshipSite, the citadel heart, SpellIcon, MagicTeleport, Fragment) | 0x52E650 → `AllocateMultiChild` 0x52EA50, `JustMapXZ::Init` 0x5E1920, then `AssumeFixed` 0x52DEE0 in each cell; the children sorted by x and z (`SortChildren` 0x52DC10, a qsort with `CompareMultiChilds` 0x52DBC0) | head of the fixed list, all its cells |
| FishFarm | 0x52CA10 (AssumeFixed 0x52DEE0); `GetNextPos` 0x52C940 gives **a single** position, its own | head of the fixed list, its cell |
| Object (Villager, Animal, Creature, GStreetLantern, Pot and piles, the one-off orbs, MobileObject 0x607250, the Whale) | 0x636740 → `Object::InsertMapObjectToCell` 0x636830 (vt+0x54C) | with bit 15, **tail** of the fixed list (pots and piles, street lamps); without it, head of the mobile list (doubly linked, +0x38; the orbs: their info is of type 20) |
| SpellSeed, MagicFireBall, Town, Forest... | `ret` (0x728F30, 0x682D10) | off the map |

The next object in a cell's list is `GetMapChild` vt+0x53C: Object 0x418C90 (+0x20); MultiMapFixed 0x52E400 (a
bsearch 0x52DC30 by the cell's x and z); FishFarm 0x52CAD0 (its one position). `SetMapChild` vt+0x540: Object
0x418CC0; MultiMapFixed 0x52E420 (a linear search: the children may not be sorted yet while the insertion runs).
`CallVirtualFunctionsForCreation` inserts each object as it is made (0x636BE0+0xD8 / 0x607150+0xA9 / 0x52E890+0x184),
and skips the insertion when +0x24 bit 2 or +0xA & 0x11 is set (MobileObject 0x6071E8 / 0x6071EF, MultiMapFixed
0x52EA04 / 0x52EA0A).

Deleting (`RemoveMapObjectFromCell` 0x6368D0; RemoveMapObject vt+0x548: 0x6367A0 / 0x52E600 / 0x52E7B0 /
0x52CA70) does not change the order of the others. Moving (`MoveMapObject` vt+0x55C): a single-cell object is only
reinserted if it changes cell (`Object::MoveMapObject` 0x636A40, the words +0x16 / +0x1A); a MultiMapFixed, if its
MapCoords changes (0x52E4F0, `operator==` 0x605660); FishFarm's vt+0x55C is MultiMapFixed's 0x52E4F0 (vtable
0x8DADC0). `ActualMoveMapObject` 0x638040 leaves it **at the head**. `SetXYZAnglesAndScale` (0x638F80 / 0x6074E0 /
0x608D60) also removes it and puts it back in. The original removes an object before it moves or turns it. What has
left the map is out of the lists: `Object::RemoveMapObject` from InitialisePhysics (0x637480+0x3A),
InitialisePhysicsFromHand (+0x63), the hand's pick-up and `CleanupWhenDeleted` 0x6377F0.

#### Footpaths told on insert and removal

Fixed / MultiMapFixed insertion (0x52E53F..0x52E558 / 0x52E72B..0x52E744) and removal (0x52E5CE..0x52E5E7 /
0x52E7F9..0x52E816), after Object's part on every path (also when ToMap is null, 0x63674E / 0x6367AE, where +0x24
bit 0 stays clear and 0x52E539 / 0x52E5C8 go on), call `CreatureMustAvoid(0)` (vt+0x7BC): the footpaths go round it
(or back), with its `Get2DRadius` and its +0x14. The altitude is read by neither. FishFarm 0x52CA10 has no such call.
The MultiMapFixed removal runs after `ProcessRemoveFromMap` 0x52E7EF and before `ReleaseCollideData` 0x52E820. Then
the fixed insertion walks `Creature::CreatureList` [0xC5FCF8] (0x52E560..0x52E5B2; MultiMapFixed 0x52E74C..0x52E79E):
each creature it must avoid whose LH3D state +0x5198 → +0x64054 is 2..4 gets `AddToRoutePlan` (vt+0x7C0)(its
RPHolder, creature, 1, 0).

#### Cells of a MultiMapFixed

`NewCollideDescriptor` 0x46A860 / `NewCollideDescriptor::Init` 0x46AB10 / `GetNext` 0x46AD80:

1. The shape is the mesh's (`NewCollide` 0x829390) and `reach = scale · mesh+0x30 + 1` ([0x8AA390]; kept on the
   stack, 0x46AB68..0x46AB6E).
2. The box `ftol((c ∓ reach) · 0.1)` ([0x8AC404], 0x46AB7A; 0x46AB74..0x46ABC2: `fld centre; fsub reach; fmul 0.1;
   __ftol`, each product at extended precision). If the low corner is negative it becomes 0, and only then the high
   one too (0x46ABC9..0x46ABE7).
3. x outside, z inside, the mask index always moving on. A cell is marked if its **7.1 m** circle (`push 0x40E33333`
   at 0x46AC23) at `(10i + 5, 10j + 5)` touches the shape, only if it is on the map (x < [0x59C8], z < [0x59C4],
   unsigned; 0x46AC7D..0x46AD04). If none is marked, the middle one, `(w/2)·d + d/2` (0x46AD06..0x46AD3B).
4. The insertion **stops** at the first marked cell outside the map (0x52E70D).

`NewCollide(LH3DObject)` 0x829390 turns the shape by LH3DObject +0x48 (pushed at 0x8294A3 into `Obj(point, a, b,
angle)` 0x82ADD0, +0x14), the object's Y angle, which `Game3DObject::SetPosition` 0x63B680 (0x63B68F) and
`Object::SetXYZAnglesAndScale` 0x638F80 (0x6391E1) store; the x and z angles do not enter. It is the y of
`LHMatrix::GetYXZ` 0x7FAB30 (row 2), not row 0 ([LH matrices](matrices-and-zoomer.md#lh-matrices)). The box's centre
goes through the full matrix (0x8293CE..0x829413).

#### Reads of a cell

- The original's traversal (`GetFirstIterator` 0x6034D0 + fn_006827E0, and the inline copies) is **the fixed list
  from its head and then the mobile list from its own**. The mobile list alone is 0x603490.
- FindType(cell, t, previous) (0x6045C0 → `FindTypeOnMap` 0x6015E0): with −1, the fixed list and then the mobile one
  (when the fixed list ends it jumps to the mobile one if the previous one's type counts as fixed, 0x601621); with
  another type, **only its list** (0x601646).
- `FindFixedOnMap` 0x601690. `IsFixed` (0x603790 → 0x601EA0) looks **only at the head** of the fixed list: whether it
  is a MultiMapFixed (+0x24 bit 1). The own-cell test is fn_00604F40.
- `MapCell::Clean` 0x601380 empties a cell: no objects, no links.

#### Collision with what is in the cell

- The fixed list alone from the head (`GetFirstObjectFixed` 0x6034B0 + `GetMapChild`) is the chain of 0x60CAA0
  (ObjectCircleIterator), 0x74B9C0 (`Tree::EndPhysics`) and 0x601D56.
- `MapCoords::Collide` 0x6033C0 → `MapCell::Collide(MapCoords)` 0x601CE0 → `MapCell::Collide` 0x601BD0:
  - outside ToMap, `0xFFFFFFFF` (0x6033CC);
  - otherwise, the terrain part (0x601BEE..0x601C62, the landscape cell's water bit, 1 when there is no block): 0x10
    outside the game map (fn_00601E00, and then no object is checked any more, 0x601C6E), otherwise 1 water (or no
    block, 0x601C5D) / 2 land (`sete dl; inc edx`, 0x601C7D);
  - then the **fixed list** from the head (0x601C78..0x601CAE, `[edi + 4]`, next vt+0x53C at 0x601CA6; the mobile one
    is not read), the type of each (`[obj + 0x28] + 0x10`): type 6 FOREST_TREE `|= 0x20` (0x601C9E), type 0x12 FIELD
    `|= 4` (0x601C99);
  - **bit 8 never comes out**: 0x601CE0 calls `CollideWithFixe` only if the result of 0x601BD0 has bit 8 (`test bl,
    8`, 0x601CEB), and 0x601BD0 never sets it. That branch is dead.
- `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe` 0x601D10:
  - outside ToMap, `0xFFFFFFFF` (0x604FEC);
  - otherwise, the bits of 0x601BD0 (0x601D18) and `| 8` (0x601DAD) if a **0.5 m** circle (`push 0x3F000000` at
    0x601D4C, `NewCollide::Obj` 0x82AD90) at `(x, z) = MapCoords · 10/65536` ([0x8AA3A4], 0x601D23..0x601D3C; the
    altitude is the point's y, the collision is in x, z) touches the `GetCollideData` (vt+0x858, 0x601D61) of some
    object in the fixed list (0x601D56..0x601D8A, none skipped but those without data; `NewCollide::Obj::Collide`
    0x829140).
- The collision data are made **on insertion**: `SingleMapFixed::InsertMapObject` calls vt+0x864 at 0x52E633 and
  `MultiMapFixed::InsertMapObject` vt+0x908 at 0x52E669. `GetCollideData` vt+0x858 reads SingleMapFixed +0x58
  (0x52EB30) and MultiMapFixed +0x78 (0x401630). Read in the vtables:

  | Class | GetCollideData / CreateCollideData |
  |---|---|
  | Object, MobileObject, Pot, GStreetLantern, Villager | `Object::GetCollideData` 0x419B30 = `xor eax, eax`: the pots, piles and street lamps at the tail of the fixed list do not collide |
  | Tree, MagicTree | `Tree::CreateCollideData` 0x74C5F0: fn_00829590(the world matrix's position, 0.3) (`push 0x3E99999A` at 0x74C622) |
  | MapShield and the other SingleMapFixed | `SingleMapFixed::CreateCollideData` 0x52F510: `NewCollide(LH3DObject +0x40)` 0x829390, the mesh's shape |
  | MultiMapFixed (houses, fields, features, rocks, stumps, pieces, totems, icons, stones) | `MultiMapFixed::CreateCollideData` 0x52F550: `NewCollide(LH3DObject)`, the same shape that gives its cells |
  | BigForest | 0x439580 = `jmp ReleaseCollideData` 0x52F6D0: none |
  | FishFarm | 0x52CA10 does not call vt+0x908; the MultiMapFixed constructor sets +0x78 = 0 (0x52E26F): none |
  | WorshipSite, CitadelHeart | 0x77E490 / 0x468FB0, their own shapes: the heart a 15.05 m circle and 32 circles of 3.311 m on 7 spokes, the site 10 circles of 1.82 / 2.6 / 3.9 m with no centre circle ([layout](../magic/temple-and-citadel.md#the-hearts-and-the-worship-sites-collide-shapes)) |

#### Searches

- `FindNearType` 0x6045F0: a single list (−1 is the mobile one, 0x60475C); the corners 0x6045F8..0x6046E6, x outer
  and z inner as signed words (`movsx`, `jg`), InBounds per cell; best from FLT_MAX [0x93004C]; it does not clip to r.
- `MapCoords::FindNearForScript` 0x604370 (`ret 0x10`): a signed ±r square, x from the low corner to the high one
  (signed words, `jg`, 0x604471, 0x60449E); the z count is `(high & 0xFFFF) − low + 1` (0x6044A7..0x6044AF: `movsx`
  low, `and esi, 0xFFFF` high) and z goes up as a 32-bit counter whose low word is the cell (fn_00601F40 stores
  words); InBounds on each cell, the full walk; among those accepted, the nearest, measured from the totem for a
  worship site (`GetTotemPos` 0x77CF30, vt+0x304 IsWorshipSite), else from the object's MapCoords +0x14; strict `<`
  from FLT_MAX [0x93004C]: not cut at r. The scripts' filters (`CALL_NEAR` 0x6F7070, `IS_FIRE_NEAR` 0x6F7100) compare
  that same point with r.
- `FindNearestInSpiral` fn_00604AF0 / fn_00604C30: `max(3, ftol(ceil(2r/10)))²` cells (`fld r; fadd st0, st0; fdiv
  10; _ceil` in double; `__ftol`; `cmp eax, 3; jg`, 0x604C33..0x604C80), dir = count = 1 (0x604C8A / 0x604C8E),
  FindType(ANY) in each; `d < r` (0x604D08), then d < best or no best yet (0x604D13..0x604D1A); with a best, it stops
  when `1.5·best + 10` ([0x8AB24C] / [0x930050], 0x604CAC / 0x604CB2) is below the distance to the cell's walk point
  (0x604C98..0x604CBF).
- `FindNearInfluenced` 0x604870: `GetDistanceModifier(d, r)` 0x74F290 (r is the last argument, read at 0x604A33); the
  first argument's slot is zeroed at 0x6048D0.
- fn_006022C0 (MapCoords this, Object* self, bool skipLiving): the highest `GetTopPos` (vt+0x630) of the objects of
  this cell (fixed, then mobile) other than self (and, with skipLiving, not living nor moving) whose in-cell offset
  (the low words of the MapCoords × 1/65536 × 10, 0x6022E0..0x60231A) is within `dx² + dz² < r_obj² + r_self²` of
  self's (`fcompp; test ah, 1` 0x602417; 0x6023A1..0x60241E); 0 if none or off the map (0x60231E).

#### Towns

Towns do not use the cells:

- `GetNextPlayerAndNeutral` 0x550980: slots 0..7, the neutral one last, × the list of each player, which is filled
  filled **at the tail** by `GPlayer::AddTown` 0x64C090 (it walks the list, head +0xA50, next Town+0x75C, and
  appends, 0x64C0A4..0x64C0C6; count +0xA54), whose only caller `Town::AddTownToPlayer` 0x73A904 is called from the
  Town constructor (0x739597) and `ClaimTown` (0x6498F2): the order in which the player got each town, a captured
  town last [V].
- `GetNearestTown` 0x6020E0 and `GetNearestCitadel` 0x602200: strict `<` from r. `GetNearestTownWithCentre`
  fn_00602160: the same, only the towns with +0x9A4 (0x6021A7) **or**, otherwise, fn_00741020 (0x6021B3), the
  town-has-a-centre test (read in full, 0x741020..0x741070): 1 (0x74103A `jne 0x741069`) if among its houses (+0x754,
  next +0x9C; 0x741024..0x741044) there is one with `IsTownCentre` (vt+0x1E0; only TownCentre 0x55DB70 gives 1,
  `mov eax, 1`; GameThingWithPos 0x401AF0 gives 0) or among its planned ones (+0x9A8, next +0x44;
  0x741046..0x741062) one whose info (+0x40) gives `GetAbodeNumber` (vt+0x44, `GAbodeInfo::GetAbodeNumber` 0x401260
  = info +0x124; the other planned things' infos, GFeatureInfo 0x421E90, give −1) == 0xC
  (ABODE_NUMBER_TOWN_CENTRE; the «GetComputerSeen» of the slot's old name was false). In info.dat the houses of type
  0x404 are exactly those of number 12.
- `MapCoords::GetNearestTown(Town**, uint*, excl, tribe)` 0x601F90 (`ret 0x10`): the whole-cell distance
  `max(|dx|, |dz|) + (min >> 1)` (the high words' differences, `sar 1`, 0x602003 / 0x60200B), best = 10 000 000
  (0x989680), not the excluded town and not of the tribe. The code: 1 when the position's cell is within 4 cells of
  the found town's rectangle (+0x728..+0x73A; the high words +0x72A, +0x72E, +0x736, +0x73A, compared unsigned, `jb` /
  `ja`: a cell below 4 wraps to "outside"; an empty rectangle, min 0x7FFF and max 0, is never "within"), else 2
  (0x602076..0x6020C6).
- `Town::GetNearestTownToPos` 0x73B170 (pos, tribe, abode, r): the town walk, 0x74CD50 (= `GetDistanceInMetres`) <
  best, best = r; the tribe (GetTribe() +0x10) when given; `0x7FFF` is any house; with another type it skips the
  towns `IsAbodeTypeInTown` 0x73D6B0 finds it in (the town's abodes, +0x754 next +0x9C, with info (+0x28) +0x120 ==
  abode): **it accepts the towns that do not have it**. The fields are in that list: `Field::Field` 0x527DD0 calls
  `Abode::Abode` (0x527DFA) with its town, which calls `Town::AddStructureToTown` 0x7399A0 (0x4013BB), whose
  dynamic_cast to Abode (0x7399B7) links it at +0x754 (0x7399C3..0x7399CF).
- fn_00552FF0 (`ret 4`, one MapCoords): the global town list g_game+0x205C84, the newest town first (the Town
  constructor's head insertion 0x73964D..0x739656). **It has no ID branch** (read): the first one always (0x55300E),
  then `<` (`fcomp; test ah, 1`): an exact tie goes to the newer town.
- `GScript::FindPlayerTownAtPos` 0x6F72E0 (used by GET_NEAREST_TOWN_OF_PLAYER, 0x6F2ADD): only the list of **that**
  player (GPlayer+0xA50, next +0x75C), `GetDistanceInMetres` 0x74CD70, best = r and **`≤`** (`fcom; test ah, 0x41`,
  0x6F7312): in a tie the town that comes later wins. It is not `GetNearestTown` 0x6020E0.
- In the worship code: `Citadel::RequestANewWorshipSite` 0x4633F0 calls `GetNearestTownToPos(the citadel's coords,
  tribe, 0x7FFF, FLT_MAX)` at 0x46345C. `AssignTownsToWorshipSite` 0x77AF70, `CreateBuiltWorshipSite` 0x465110,
  fn_00464F50 and `GPlayer::PostLoadCleanup` 0x64AB90 walk the player's towns.
- `MapCoords::FindWorshipSite` 0x602460: **a single** `FindTypeOnMap(8, 0)` (0x602479). If it is a WorshipSite
  (0x602493), that one; if it is a WorshipSpellIcon (0x6024AC), its site (vt+0x30C); if it is something else (the
  citadel heart, a town-centre icon), null.

#### Clearing and physics

- `GlobalGameLists::ClearMap` 0x591A92 → `fn_006013D0` (the GMap at g_game +0x59B8) runs `MapCell::Clean` 0x601380 on
  every cell (0x6013FF). `CleanupWhenDeleted` 0x6377F0 takes an object out of its cells through vt+0x548: `Object`
  0x6367A0, `MultiMapFixed` 0x52E7B0, `SingleMapFixed` 0x52E600.
- **When physics ends** (`Object::EndPhysics` 0x6375A0, 0x637613..0x63763A): if the object is still available,
  `MapCoords::InBounds` 0x6042C0 of its position. Inside the 512 × 512 cells it goes back into the lists
  (`InsertMapObject`, vt+0x544); outside **it is deleted** (`ToBeDeleted(0)`, vt+0xC). Everything that lands goes
  through there: villagers, animals, pots, scaffolds, grain and the fixed ones (rocks, trees, fragments).
  `Object::EndPhysics` always returns the object itself (never NULL). The villager (0x5F0B81) and the animal
  (0x5F0E01) call it in the middle of their EndPhysics, after SetYAngle and before their landing, water and death
  work; the other classes after their part. A tree that becomes a dead tree (`Tree::EndPhysics` 0x74BBD9..0x74BC3A)
  never calls it: the DeadTree goes in the cells with no InBounds test (0x74BC0A) and is returned, and GameTurnUpdate
  makes it the resting proxy (0x645EE0).
- **The creature** goes into the lists when it is made: `Creature::Create` 0x474A20 calls
  `CallVirtualFunctionsForCreation` (vt+0x658, 0x474A7E), which for a Living is 0x5EC9B0 → `Object` 0x636BE0 →
  `InsertMapObject` (vt+0x544, 0x636CB8; skipped only with +0x24 bit 2 or +0xA & 0x11), and only then `SetAnim`. Its
  type (CREATURE, 1) is not fixed, so it enters at the **head of its cell's mobile list**.
  `Creature::MoveMapObject` 0x479E50 first runs the look-at and long-range vision updates, and with +0x10CC set it
  returns 6 **without moving at all**.
- **Objects on a `WALK_PATH`** are **not** refiled: `MobileObject::MoveAlongPath` 0x607790 sets the position through
  vt+0xFC (0x6078D7), which for MobileObject and Whale is `GameThingWithPos::SetPos` 0x401940, a plain copy of the
  MapCoords into +0x14, no `MoveMapObject`. A walking MobileObject stays in the list of the cell it was in; the Whale
  (the only `WALK_PATH` user of the original scripts) is not in the lists at all, since
  `Whale::CallVirtualFunctionsForCreation` 0x774CA0 never calls `InsertMapObject`.

### Sources

- The exe, with the xref index and a sweep of every vtable's size slots; info.dat for the types and the town-centre
  numbers.

## openblack

**Done**, faithful except what is marked. Port decisions are in
[Port decisions: engine and world](../project/port-decisions-engine-world.md).

### The size API

`src/ECS/ObjectMetrics.{h,cpp}`, namespace `openblack::ecs::object`. openblack once had these sizes hand-written 33
times, with 6 wrappers, and each copy knew at most one of the class overrides; the wrappers `effects::ObjectHeight` /
`Object2DRadius` (the `Object` routine **without** overrides) are gone and all their callers go through the API.

- Mesh level: `HalfExtents` (of a box) and `MeshHalfExtents` (of a mesh id), `HalfDiagonal`, `MeshHalfDiagonal`,
  `Radius2D`, `MeshRadius2D`, `Height`, `MeshHeight` (each of half-extents and a scale), `MeshHalfHeight`.
- Object level: `GetScale` (the uniform scale of the `Transform`; a `MapShield`'s `objectScale`; the creature's
  `creature_pose::DrawnSize`, which is `CreatureDrawPose::size` in its pen, else `Creature::size`), `GetScaleField`
  (a creature's is the same as `GetScale`: what the original's +0x50 holds for a creature is not read, and a creature
  reaches no reader of it; the temple's is its plan's scale), `FootprintRadius` (was `ObjectGet2DRadius`),
  `ObjectHeight` (was `ObjectGetHeight`), `Get2DRadius` (the creature's through `creature_morph::Radius`),
  `GetRadius`, `GetHeight`, `GetTopPos`, `GetHeightForHandAboveInteractObject`, `GetMeshRadius`,
  `PileFoodProportionRaised`, `PileWoodProportionRaised`, `GetProportionRaised`.
- Derived: `GetHoldRadius` (with the hold-above flag; the hold type is known by the hand; the seed's is set by the caller),
  `GetDefaultFireRadius`, `GetVillagerHugRadius` (the tree's `TreeHugRadius`), `GetRoutePlanRadius` (the temple, the
  original's CitadelHeart, × 0.33; with a creature `GetRoutePlanRadius(e, creature)`), `GetDistanceFromObject` (of an object or a point), `IsTouching`, `GetBoundingSphere` (the ground is the island's, `LandIsland::HeightAt` through
  `map_coords::ToWorld`), `WorshipSiteCentre` (openblack keeps the point, not a MapCoords), `GetNearestPosOfObject`,
  `GetNearestEdgeToPos`, `GetNearestEdge`, `GetWorkingPos`, `TreeGetWorkingPos`, `BigForestGetArrivePos`. Constants:
  `k_MagicTeleportRadius`, `k_MagicFireBallRadius`, `k_ProportionFloor`, `k_TreeWorkingReach`.

Users, at the original's level: `EffectValues`, `FireObjectTraits` (fire, and with it FireEffect, FireGraphic,
Explosion and VillagerFire), `PotResource`, `PotArchetype::SetSize`, `SpellWater`, `SpellFlock`, `Heal` (including the
rule scale), `OneOffSpellSeed` (the Z-sorter advance), `SpellDispenser`, `TestDispensers`, `TownQueries`, `Trees`,
`Rocks` (and with it `LanternSounds` and the rock physics), `AnimalFlee`, `AbodeArchetype`,
`HandHolding::ComputeHoldParameters`, `Storm.cpp`, `SpellForest.cpp`, `SpellSeed.cpp`, `MapShield.cpp` (its
`CollisionScale` is `object::GetScale`; its copies `map_shield::Get2DRadius` / `GetHeight` are gone),
`MagicTeleport.h` (`k_Radius = object::k_MagicTeleportRadius`). At mesh level: `Fields` and the crown of `Trees`.
`HandSystem::Update` no longer repeats the proportion with a fixed 1600: it asks for the parameters again every
frame (in info.dat HandFood is potType 1 = PileFood with `maxAmountInPot` 1600, so only the empty hand changes, which
now measures 0). `Audio/Services/LanternSounds.cpp` calls `Rocks::Height`, which is now `object::GetHeight`.

Fidelity fixes the API brought, each visible in play:

1. **A field is 5 m** in fire, the animals' working position, the town's clear-area test, healing, the forests and
   SpellFlock / SpellWater.
2. **A fish farm is 5 m**: it was not anywhere.
3. **A food pile × its proportion** in all the object queries (before only in `pot_resource`).
4. **MagicFireBall** (radius and height = scale) outside fire: Heal and everything that goes through the API.
5. **The empty pile gives 0**, not 0.0975 (`PotResource.cpp`, and its test `test_food_wood`).
6. **The field sank twice as much**: `Fields.cpp` stored the whole height; now it is the mesh's half-height.
7. **Without a mesh, 0**: the hand (0.5 / 1.0), the pile when sinking (1.0) and the field (1.0) gave invented sizes.
8. **The MapShield with a single scale**: a shield's `GetScale` is its `objectScale` in all the queries.

Also: `Trees.h` cited `ComputeBoundingBox` at 0x808180 (it is 0x8081B0); MagicTeleport = 6 and the food pile's
proportion now count in fire and in water; a burning worship site uses 14 m; the height of a creature for fire and
healing is 15 × its scale; a food pile picked up from the map (PileFood, MagicFood, PuzzleGrain) opens the hand
according to how full it is, and the empty HandFood closes it completely.

Tests: `test_object_metrics`, `test_food_wood`.

### The cell lists

`src/ECS/MapCells.{h,cpp}` and `MapCellsLists.h` (`ecs::map_cells`), behind raffclar's `MapInterface` /
`MapProduction` (`src/ECS/Systems/MapCellsSystemInterface.h`, `Implementations/MapCellsSystem`): `GetFixedInGridCell`,
`GetMobileInGridCell`, `GetAllInCell`, `Sync`, `Refile`.

- The kinds (`InsertKind`: Object, SingleMapFixed, MultiMapFixed, FishFarm) by the entity's components
  (`map_cells::KindOf`) and the list by its info.dat type (`map_cells::TypeOf`; openblack keeps no info row for a
  MagicTeleport, Fragment, DeadTree or StreetLantern, so the first GMobileStaticInfo row's type, 🔍 inferred).
- Reads: `ForEachInCell` / `ObjectsInCell` (fixed then mobile), `MobileInCell`, `FindType`, `IsFixed`, `IsOwnCell`,
  `ForEachFixed(cell, fn)`, `Collide(MapCoords)` (the terrain part through `sea_cells::CollideLandscape`; with no island,
  as in the unit tests, no block, so water), `CollideWithFixed(MapCoords)`, `CollideDataOf` (kept with the link,
  built on insertion as the original's; constants `k_TreeCollideRadius`, `k_FixedTestRadius`, `k_CellCircleRadius`,
  `k_CellsPerMetre`), `DescriptorCells`, `GetMapChild`, `IsObjectInMap`, `Clear`.
- Searches: `FindNearType`, `FindNearForScript` with `ScriptDistancePoint`, `FindNearestInSpiral`
  (`k_SpiralStopFactor`), `FindNearInfluenced` (nobody uses it yet), `TallestOverlapping`.
- Towns: `ForEachTown` / `TownsOf`, `GetNearestTown`, `GetNearestCitadel`, `GetNearestTownWithCentre` with
  `TownHasCentre` (it checks the number of the town's `Abode`, `townId`; test `TownCentreInfosAreNumber12`),
  `GetNearestTownCells` (`k_TownCellsStart`; the rectangle is `Town::areaMin` / `areaMax`, kept by
  `town_placement::SetTownArea`), `GetNearestTownToPos`, `FindNearestTownInList` (`town_queries::TownsNewestFirst`),
  `FindPlayerTownAtPos`, `site::FindAt`.
- **Maintenance.** The hooks `InsertMapObject` / `RemoveMapObject` / `MoveMapObject` / `OnAnglesOrScaleChanged` sit
  where the original calls the vtable; `InsertMapObject` does nothing if the object is already in the map. The magic
  code calls them for its own objects: SpellForest trees, piles, storm pieces, orbs, MapShield, the stones and the
  teleported living being, and whatever the tornado carries (`SetHeldOutOfMap`). The other owners' objects come in
  through `Sync()`, which `MapProduction::Sync` calls at the start of every turn, on load and in `Reactions`: first the
  removals (destroyed, in the hand, in physics, class change, a one-cell object off the map) and then the additions
  and the moves by creation index (🔍 inferred: the turn's processing order). Every read also skips what the
  original would already have taken out: invalid, in the hand or flying. `TellFootpaths` tells the footpaths on every
  insertion and removal, as above.
- **When physics ends**: `physics::EndPhysics` (PhysicsObjects.cpp, through `PhysicsObjects::BackInMap` from the
  villager physics) does the original's InBounds test and refiles or deletes.
- **Mobile objects**: `MobileObjectArchetype::Create` calls `InsertMapObject` as the original's
  `CallVirtualFunctionsForCreation` does (0x607150+0xA9: vt +0x544, 0x607250 → 0x636830).
- **The creature**: `CreatureArchetype::Create` calls `InsertMapObject` right after its Transform, as the original
  does; every move of the turn goes through `MoveMapObject` (`creature_pose::CommitTurnPose`).
- **Walk paths**: `WalkPathSystem::ProcessTurn` writes the position directly, with no hook, and `Sync` follows the same
  rules (`map_cells::IsFiledInMapCells` and `IsRefiledWhenMoved`): a shark is never filed, by `Sync` or by
  `InsertMapObject` (so not when physics ends either), and an object with a `WalkPath` is not moved to its new
  cell by `Sync` while it walks.
- The villagers' walk round obstacles reads these lists and the things' collide data (`thing_circles::CirclesInCell`,
  [Walking round obstacles](../beings/wall-hugging.md#walking-round-obstacles-mobilewallhug)); its own fixed grid of
  bounding circles (`FileObstacles`, `ObstaclesIn`) is gone, as are the old mobile grid and
  `effects::ObjectsInMapCell` / `FixedObjectsInMapCell`.
- The shape's angle is `YAngleOf` (`map_cells::detail`), the y of `GetYXZ` from the Transform. The shapes come from
  `map_collide::FromMesh` (`FromMesh` of the object's mesh).
- Inside `MapCells.cpp`: `CellsAndCollide` (the cells an insertion puts the object in, and its collide data),
  `CellCollide` (the terrain and fixed-list bits; without `Locator::terrainSystem::has_value`, no island, so water),
  `ChildLess` (the children's x, z order), `ChildOf` / `SetChildOf` (the map child), `ScriptSquare` (the walk of the
  script search's square). `TypeOf` gives the orb the row `InfoConstants::mobileObject` [`MobileObjectInfo::OneOffSpellSeed`]
  and `TownHasCentre` checks `Locator::infoConstants::has_value` before reading the planned abodes; the removal of an
  `InsertKind::MultiMapFixed` uses the cells it was put in.

**⚠️ approximate** differences:

- Without a mesh (fields, teleport stones): the cell of its position, and no collision shape.
- The descriptor's reach and products in float, where the original keeps them at extended precision; the spiral's
  `2r/10` in float too.
- The box's centre rotates only in xz by the Y angle, where the original goes through the full matrix.
- A MultiMapFixed's removal uses the cells it was put in; the original rebuilds the descriptor from the current state,
  which is the same because it removes before it moves or turns.
- The move test does not compare the altitude (the ground openblack settles while drawing must not send the object to
  the head every turn), a new mesh counts as a change of shape, and a fish farm keeps the cell test (it does not move).
- The worship site's and the citadel heart's own collision shapes: openblack differs: one child radius from the mesh
  box (`src/ECS/MapCollide.h:26-33`), a fix row.
- `GetNearestTownCells` without the town's rectangle in some callers.
- When a walk ends openblack drops the `MobileWalkPath` and the next `Sync` refiles the object, where the original
  leaves it in the old cell until something else moves it; no original script walks anything but the whale.

Tests: `test_map_cells`, `test_map_cells_index`.

### Test hooks

- `OPENBLACK_MAPCELLS_CHECK=1` checks the lists on every `Sync` and writes `map_cells: N objects, M cells, E errors`.

## Pending

### Object size

The rule has been to migrate **only** where it has been read which level the original uses (the virtual call or the
inline read).

- `HandPlacement.cpp:659-666` (locked pile): already the original's sum, `GetHeightAt(at) +
  GetHeightForHandAboveInteractObject(source) + height` ([above](#object-size)); the third term, the hold height, is
  not traced in the original.
- `HandPlacement.cpp:719-725` (the radius of the living being under the hand) and `:741-749` (the radius of what is
  held): no address.
- `HandTrees.cpp:202-207` (the trunk of the felled tree: `0.2 × Get2DRadius` = Tree's `GetHoldRadius`, 0.3 without a
  mesh) and `:331-341` (the root dust: another formula, unscaled half-axes): no address from the original.
- `LanternSounds.cpp:92, :131`: the value is already the API's; it only remains to call the API directly.
- **Plausible, not closed**: `HandHolding.cpp:187-192` (picking up a tree: `maxima.y`, not max − min) and
  `Particles/TownBelief.cpp:191` (`maxima.y` of the town centre): it remains to read `UR_TownCentreBelief` 0x69C17A.
- `ComputeHoldParameters` still has its own table of hold types instead of `object::GetHoldRadius` (the hand's table
  also carries the lowering and the type; the radii are already the API's).
- MagicFireBall in physics and in Storm: it depends on whether the ball is an entity with a `Mesh` in those queries.
- **Doubtful, not migrated** (no record of what the original does at that place): the shadow of thrown objects
  (formerly `Graphics/PhysicsShadows.cpp:155`; now `ShadowList.cpp` uses mesh+0x30 × obj+0x44, the radius of
  `fn_00874600`); `HandTrees.cpp:375, :409` (`0.5 × Size().x`); `Particles/TownBelief.cpp:183-196` (the height of
  the top of the totem); `ECS/FireFlies.cpp:98-108` (`MeshHeight` + 2 of houses and street lamps: fn_0052B1D0 is only
  the filter, `IsAbode` / `IsStreetLight`; where the height is added is not read); `Physics/PartialBuild.cpp:148` (the
  cut of the construction site: in fn_00816AD0, not read); `ECS/Trees.cpp:1104-1114` (`MeshHalfDiagonal` of the
  sources that bend trees: scale × +0x30, and `GetMeshRadius` 0x636BD0 has no scale; no address).
- `Creature::GetBoundingSphere` 0x479970 (`LH3DCreature::GetBoundingSphere` 0x47F8D0): not ported, the creature uses
  the `Object` one **🔍 inferred**.
- The classes that are not `Object` (Citadel, SpellShield, SpellStormAndTornado, Town, GArena, Reaction,
  BuildingSite, AtomCore, GStreetLight, Mist) do not go through the API; if any of them comes to ask for it, its
  branch has to be added.
- The other `GetScale`: `ShowNeedsVisuals` 0x55DD80 (+0x58), `PlannedMultiMapFixed` 0x4050C0 and `SpellSeedGraphic`
  0x727340.
- The mesh-level places openblack does not have yet (`IsSuitableForFixed` 0x603E1E, `Scaffold` 0x6E956A / 0x6EAC14,
  0x7350A5, `PhysOb::Initialise` 0x7FB7D9...): the API is ready for them.
- The box of an animated mesh: the original computes it after `LH3DAnim::SetTransform` 0x83A1D0 (bit 0x100); openblack
  joins the boxes of the vertices as they are. Villagers and animals could have somewhat different half-axes (not
  measured).
- **To check with a screenshot**: the field sinks half as much when emptied; a field or a fish farm measures 5 m for
  fire, the town (where buildings fit), the animals and the miracles; a food pile measures according to how full it
  is (and empty, 0) for fire and the town.

### Object lists per cell

- `Creature::MoveMapObject` 0x479E50's look-at and vision updates and its +0x10CC early return: not ported.
- The creature list walk of the fixed insertion (`AddToRoutePlan` for creatures that must avoid the object): not
  ported (the creature's route plan).
- The worship site's and citadel heart's own collision shapes (0x77E490 / 0x468FB0) are read: see
  [the heart's and the worship site's collide shapes](../magic/temple-and-citadel.md#the-hearts-and-the-worship-sites-collide-shapes).
  openblack differs: one child radius for both shapes (`src/ECS/MapCollide.h:26-33`), a fix row.
- The town order is the order the player got its towns (`GPlayer::AddTown` 0x64C090, above). Still open: how
  openblack orders a player's towns after a takeover (not checked; a candidate fix row if it sorts by id).
