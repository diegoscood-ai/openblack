# Villagers: jobs

## Summary

A town's villagers work for its desires. They carry food and wood to the storage pit, build and repair buildings at
the sites the town plans, farm the fields, fish at the fish farms and fell trees in the forests. They also pick up the
food and wood they notice lying around (reactions 7 and 12). When a key building is damaged the town calls an
emergency and its villagers gather at the meeting point. Tapping on a home sends its inhabitants out. Villagers also
beat out fires.

openblack reproduces the carrying, the builders and repairers, the emergency, the farmers, fishermen and foresters,
and the food and wood reactions, with the tree and forest searches they use. Shepherds, the BUILDER disciple and a
few states the game never enters are not ported. Fire fighting is on [Fire](../world/fire.md#fire-in-the-table); the
building side (sites, damage, repairs) is on [Building sites](../world/building-sites.md) and
[Buildings](../world/buildings.md).

## The original

Villager states are named by their number in the state table (163 is DecideWhatToDo, 31 GOTO_STORAGE_PIT_FOR_DROP_OFF,
and so on). A state function returns 1 when it handled the turn. The game logic runs with the x87 at a 24-bit
precision control (fn_007DEE00, 0x7DEE0D), so every step of the formulas below rounds to a float, on the stack too.

### Trees for the villager jobs

#### Wood in a tree and a trunk

- **Wood value**: `Tree::GetWoodValue` 0x74B7B0 = life × 1 × woodValue × scale × GLandBalance[5];
  `DeadTree::GetWoodValue` 0x511AD0 = life × woodValue × scale³ (the original cubes the scale there).
- **What a store receives** (from the hand over a store, or from a thrown tree that hits a pit) is
  `GetDefaultResource(WOOD)`: `Tree` 0x74B7A0 = (int)GetWoodValue, `DeadTree` 0x511330 = (int)(woodValue × 1 × scale).
  An object's resource is its `GetDefaultResource` (`Object::GetResource` 0x639520: its own if the type matches, 0
  otherwise).
- **Removing wood from a trunk** (`DeadTree::RemoveResource` 0x511370): if it has ≤ n left, it is deleted and gives
  what it had; otherwise it **shrinks**: `scale = (wood − n)/(woodValue × multiplier 1)`. Checked: a dead beech of
  scale 1, 700 → remove 100 → 600, scale 0.857.
- **Trunk type when carrying it**: `Tree::GetCarriedTreeType` 0x55D900 = info.dat `carriedType`;
  `DeadTree::GetCarriedTreeType` 0x511A20 = 0-3 if its mesh is one of the 4 trunks of `CarriedObject::Init` 0x462600
  (MeshPack 406, 347, 348, 349), otherwise its tree's `carriedType` (beech = 3, hardwood).

#### Deleting trees and forests

- `Tree::ToBeDeleted` 0x74A210 / `DeadTree::ToBeDeleted` 0x510C90: the tree leaves the forest and the physics, the
  listeners (fire, reactions, hand) are told, and it is deleted. Shrinking a tree that would reach 0 deletes it
  (fn_0074A3A0). A Tree or DeadTree sunk in the sea (the physics' code 4) is deleted the same way.
- `Forest::ToBeDeleted` 0x539C60 deletes each tree from its two lists and leaves the forest list (also the empty forest
  after 2000 turns). It also takes the forest out of every town's list: fn_0053AE10 walks the towns (g_game
  +0x205C84), and each drops it from Town +0x608 (fn_00741A70). Forest delete is Forest vt +0xC.

#### Felling a tree

`FelledTree::Create` 0x5116A0 is called only by `Villager::ForesterChopsTree` 0x75FAC0 (at 0x75FAE2). In order:

1. **No 3D object**: for a tree without its 3D object (+0x40) it returns 0 at once (0x5116A8..0x5116B5).
2. **The trunk**: the tree becomes `DeadTree` + `FelledTree` with its mesh. `DeadTree::DeadTree` 0x510880 takes over
   the tree's 3D object (mesh, matrix, scale: 0x5108B2..0x5108B7) and info, without releasing the roots (that flag is
   only set by `Tree::EndPhysics`). Its player is the local one (g_game + 0x18 + 0xA60 × byte g_game +0x205A5B,
   0x511747..0x511759). `SetScale(tree scale)` 0x51092D → `Object::SetScale` 0x639200 rebuilds the matrix only when
   the scale differs from the DeadTree's own 1.0 (fcomp 0x63920B). The rebuild uses the DeadTree's angles, all 0
   (SetYAngle(0) 0x52DDFB), so the result is axis-aligned; at scale 1.0 the tree's matrix and its yaw stay.
3. **The throw**: it enters the physics thrown by the woodcutter. The half height is stored at 0x5116C8 / 0x5116D8,
   `k = 0.4 × height × 0.5` at 0x5116DE. The yaw `a = atan2(x, −z)` of the direction woodcutter→tree (fn_007FAA50,
   0x51179E..0x5117F8) is kept on the FPU stack in double for the fsin / fcos, not rounded to a float; |d|² ≤ 0.001
   (0x8AA3B0, a length under √0.001) gives a = 0 (0x5117BE). The velocity is `(sin a, 0, −cos a)·k` (along that
   direction).
4. **The spin**: w = (cos a·k, 0, sin a·k) × (1 / h2), each product a float (0x5117F8..0x511838), that is
   `0.4·(cos a, 0, sin a)` rad/s **in body space**. `PhysicsObject::AddObject` 0x6443A0 builds the angular momentum
   `L = Σ (w·I)_i · row_i` over the rows of the new DeadTree's matrix (`SetUpPos` 0x63A603), with its Y rotation: in
   world space it is `R·(cos a, 0, sin a)·0.4`, so how the tree falls depends on its orientation.
   PhysOb::Integrate 0x7FE260 rotates the rows with `R(ŵ, angle)`.
5. **Back in ForesterChopsTree**: `Villager::ForesterChopsTree` 0x75FAE7 deletes the tree with `ToBeDeleted`(0) (the call at 0x75FAF8; also when
   Create returned 0, so the tree goes and +0x118 = 0), and the fire passes to the trunk (fn_00730960).
6. **Settling**: `PhysOb::AdjustToGroundLevel(false, true)`; flag 2 (0x51186B, "pushed by a Living": a villager's
   body does not hit it, `GameTurnUpdate` 0x64583E); `RaiseUntilNotIntersecting` 0x644800; kind +0x1A4 = 2
   (0x511883, a felled tree).
7. **The fall's sound**: when it has toppled (row 1's y < 0.98, 0x6460A8) and is taller than 10 it sounds once,
   `SoundTag::Create(MapCoords, GetRandomSample(31, 1), track 0, mode 3, 3D, InGame)` (0x6460D5..0x6460F0), and
   becomes kind 3.
8. **Reactions**: two reactions 0x0C ("there is wood here") are made: one from the DeadTree constructor 0x510957 and
   another from FelledTree::Create 0x511889 (0x51187D..0x511889, after RaiseUntilNotIntersecting).
   FelledTree::EndPhysics 0x511970 does not add the landing one. The DeadTree enters the map when the physics puts it to rest (EndPhysics,
   vt +0x544).

`FelledTree::Draw` 0x511990 adds the trunk to the drawing twice without fire (a `return` is missing in the original):
no visible effect.

### Searches for trees and forests for the villagers

#### A tree to fell

- **Trees in a cell** (`MapCoords::FindType(6)` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0): type 6
  (`OBJECT_TYPE_FOREST_TREE`) goes in the cell's list of **fixed** objects (MapCell +4), and
  `Fixed::InsertMapObjectToCell` 0x52DEA0 puts each object **at the head**: the first is the last one inserted.
- **`Villager::FindTreeNearVillager` 0x75FD00**: the 9 cells around, in the order of `GUtils::Spiral` (0x74D7E0, table
  0xDA59FC, starting with dir 1 and steps 1: (0.0) (−1.0) (−1,−1) (0,−1) (1,−1) (1.0) (1.1) (0.1) (−1.1); the spiral
  starts at 0x75FD1A). In each cell FindType(6, 0) is called (0x75FD57) and **only the first tree** counts: when the
  first FOREST_TREE is not a Tree (__RTDynamicCast 0x75FD5D) the cell is skipped, and so is one that is INDESTRUCTIBLE
  (bit 0x4000 of +0x24, tested at 0x75FD6B; only set by puzzle objects and `LandscapeVortexOut`; no tree in a normal
  game). The nearest by `Dist2D(villager, working position)` from 99999 wins. No more rules: no maximum distance, no
  "of a village" bit (+0x5E & 2), no size, no forest.
- **Its result**: 0 (none), 1, or 10 when already touching the tree's working point (`IsTouching`; `neg; sbb; and 9;
  inc`, 0x75FDF3..0x75FDFD). The villager side decides what to do with it.
- **Working position** (`Tree::GetWorkingPos` 0x74C040): the tree's position plus, towards the villager,
  `Get2DRadius(villager) + 0.9` (0x8C5844). The radius is **the villager's**. `Object::Get2DRadius` 0x638180 = scale ×
  max(x, z half-axes of its box).

#### Forests and big forests

- **Forest** (0x58 bytes): +0x34 empty countdown, +0x38 BigForest, +0x3C = 1 "village" forest (scenic), +0x40 id.
  `Forest::Process`: it only counts as empty without a BigForest and without trees; **a scenic forest is not
  processed** (its trees do not grow and it does not plant). `Forest::GetForestCentreTree` 0x53ABF0.
- **A BigForest has its Forest** (its ctor 0x438CE0 creates it at +0x80 and sets +0x38 on it), so the Conifer it gives
  when picked up and the Pine it plants at its edge belong to that forest. `BigForest::ToBeDeleted` 0x438EA7 sets
  forest +0x38 = 0; the Forest stays. RemoveMapObject is `MultiMapFixed` 0x52E7B0 (out of all its cells).
- **A big forest's wood** as it is made (`BigForest::Create` 0x438F13..0x438F2C): `GetScale` × the info's wood value
  (+0x6C, the integer as it is: `fimul`), into +0x84; `GetWoodValue` 0x4390B0 is life × that. A row worth no wood
  holds none, and its first taking empties and deletes it. The shipped rows BigForest1..4 are worth 15000.
- **Taking wood from a BigForest** (`RemoveResource` 0x4390D0): it asks for n / life; if it does not have enough, it
  gives what it has and is deleted; if it does, it subtracts and **only when** its wood drifts more than 250.0
  (0x8C6210) away from life × scale × woodValue is it rescaled and plants a Pine at the edge. The hand uses the same
  (350 per tree, so it always rescales). The trees in the hand or in flight leave the map, so they are in no cell.
  `GetCarriedTreeType` 0x438DE0 (type 3); `BigForest::GetArrivePos` 0x439360 (`GetArrivePos`) is its position plus, towards the
  villager, 0.5 × its 2D radius.
- **Planting at the edge** (`AddTreeAround` 0x439220): the point is position + GetPosFromAngle(GameFloatRand(2π), R)
  (0x439250..0x439274). The free test (0x43928E..0x4392C9) walks the point's cell, fixed list then mobile, and the
  BigForest itself counts: no object may have distance (fn_00605CD0, 0x4392A1) plus its GetRadius under 4. That radius
  is vt +0x60 at 0x4392AE, not Get2DRadius. The tree has size 0.05 and maximum size **0.75** (0x8AC3F8) + random(0.5):
  maxSize is drawn first (0x4392FC, + 0.75 at 0x439309), then yAngle (0x439322); the ctor keeps arg 4 as +0x64
  maxSize (0x749E4A).

#### The village's forests

- **Scenic village forest** (`Town::MakeScenicForest` 0x741B40, 0x741BB9..0x741CC5): takes the trees closer than
  250 + 10 m to the village centre that have no forest, or whose forest is scenic and that are closer to the village
  centre than to that forest's centre (2D distances). The cells are those of the `GUtils::Spiral` spiral from a copy
  of the town centre's MapCoords (fn_0073AE10), at most 99999 (0x1869F) cells; it stops at the first cell further than
  R (stop test fn_00605CD0, 0x741BC2..0x741BD0): 1369 cells, Chebyshev radius 18, not the whole 260 m disc. In each
  cell FindType(−1, 0) 0x741BDE .. FindType(−1, obj) 0x741CA1 walks the fixed list, then the mobile one, each from its
  head (CastTree vt +0xBC); the trees join the scenic forest in that order. A tree with a scenic forest (+0x3C,
  0x741C1B) whose centre is farther than the town centre (0x741C21..0x741C44) is taken out of that forest by
  fn_0053A220. The scenic forest is made only before its first tree (0x741C52..0x741C8C): if the village had none, it
  is created at the centre **only if there is any tree**.
- **Village forest list** (`Town::AssignForestsToTown` 0x73EB00, Town +0x608): it is emptied and filled with each
  forest whose nearest point (the edge of its BigForest or its centre, fn_0053ADB0) is closer than
  `GTownInfo::maxDistanceForTownForest` (250, +0x164) to the store (or the temporary point) and that has wood
  (fn_0053B280: its BigForest's plus that of each tree). It walks the global forest list newest first; fn_00741AF0
  inserts at the head of the town's list, with no duplicates. The edge is `Object::GetNearestEdgeToPos` 0x636DA0
  (vt+0x83C of BigForest): pos + GetPosFromAngle(angle towards `pos`, Get2DRadius).
- **Who calls `MakeScenicForest` and `AssignForestsToTown`**: `Town::AsssignTownFeature` 0x73EAC0 (for each village
  on map load, `MakeScenicForest` first) and the finished buildings (`Scaffold::BuildBuilding` 0x6E932C). The list is
  not touched when trees are created or replanted.
- **Replanting in a village**: the tree joins the **last scenic forest** of the village's list (`Tree::EndPhysics`
  0x74BA2B); if there is none it keeps the forest of the nearest tree already found, and with neither of the two,
  **no forest**.
- **Nearest forest** (`Town::FindNearestForestToPos` 0x73EC10): in the village's list, the one with the nearest point
  (0 if inside the BigForest's radius) closer than 250; a non-scenic one wins, the scenic one only if there is no
  other. fn_0053A1A0 (pos, max, onlyEmpty), `FindForest`: over the global list, the one with the nearest **centre**
  (empty = without trees and without a BigForest).

### Carrying resources and the storehouse

#### The load

- **What a villager holds**: food +0xF4 and wood +0xF6; the carried tree type in the flags' (+0xE0) bits 14-15,
  overwritten on each wood pick-up and never cleared by a drop.
- **Picking up** (`PickupResource` 0x7513F0): food: `add word +0xF4, n` (0x751402) and, with a town, Town +0x708 +=
  (float)(movsx n) (0x751409..0x751429); wood: `add word +0xF6, n` (0x751435), Town +0x70C += n
  (0x75143E..0x75145E), and the tree type (0x751460..0x751480). Villager::PickupFood 0x751490 = PickupResource(FOOD, n, 0);
  Villager::PickupWood 0x7514B0 = PickupResource(WOOD, n, tree).
- **Dropping**: DropFood 0x7511E0 (0x7511E6..0x751236, on +0xF4 and Town +0x708) and DropWood 0x751240
  (0x751246..0x751296, on +0xF6 and Town +0x70C; the tree bits of +0xE0 are not cleared, literal). DropFood(0) /
  DropWood(0) drop everything (0x7511EB). DropResource 0x7511B0: 1 WOOD → DropWood (0x7511B4), 0 FOOD → DropFood
  (0x7511C6), else 0 (0x7511D7, also −2). The town's +0x708 / +0x70C are the **carried** totals; the stock is the
  storage pit's.
- **Capacities**: MaxFoodCarried 150 / MaxWoodCarried 250 (GVillagerInfo +0x264 / +0x268, the same in all 63
  records). GetFoodCapacity 0x7514D0 (0x7514D0..0x7514E8, +0x264 − +0xF4) and GetWoodCapacity 0x7514F0
  (0x7514F0..0x751508, +0x268 − +0xF6) are a 16-bit `max − held`: only ax is meaningful (the callers `movsx ax`), so
  it is negative above the maximum.
- **`Villager::AddResource` 0x7564D0** always returns 0 (literal): food is PickupFood(n) (0x7564D0..0x7564E0) and,
  with the poisoned argument, SetPoisoned(1) (0x7564E5..0x7564F3); wood is PickupWood(n, 0) (0x7564F8).
- **Only the larger load per trip**: GetResourceHeld 0x751570 sets type = −1 (0x751574), compares `word +0xF4, +0xF6`
  (0x75157A..0x751599, `jbe`): food if food > wood; else wood if any (0x75159C..0x7515B0); else 0 with the type left
  at −1 (0x7515B3). A tie goes to the wood. With food 120 and wood 60, the food is dropped and
  CheckTakeResourcesToStoragePit (wood > MinWoodToShowGraphic 50 or food > MinFoodToShowGraphic 100) sends it back for
  the wood.

#### The carried object

`Villager::SetStateCarriedObject` 0x7501A0, called from GetAnimId 0x750133, gives the CARRIED_OBJECT that the caller
stores in +0xF1 (SkeletalAnimation::carriedObject):

- With the final state 4 IN_SCRIPT or the TOP (+0x8C) 0xC8 = 200 SCRIPT_PLAY_ANIM, the previous +0xF1 is kept
  (0x7501AB / 0x7501B3).
- Otherwise it first sets +0xF1 = 1 NONE (0x7501C8). Nothing at life ≤ LifeWhenCrawlsWounded (+0x380; `test ah, 0x41;
  jne` skips when ≤, or NaN, 0x7501CF..0x7501E1).
- wood (movsx +0xF6) > MinWoodToShowGraphic +0x26C (50) (`jle`, 0x7501E6..0x7501FC) → the log of its tree type,
  `Villager::GetWoodCarriedObject` 0x7502A0: (+0xE0 >> 14): 1 → 0xD 13 TREE_1, 2 → 0xE 14 TREE_2, 3 → 0xF 15 TREE_3,
  else 0xC 12 WOOD (0x7502A2..0x7502C7).
- else food (movsx +0xF4) > MinFoodToShowGraphic +0x270 (100) (`jle`, 0x750204..0x75021E) → 6 BAG, unless the final
  state's exit is ExitBuilding 0x7597B0 (a builder).
- The state rows' object wins: Infos[GetFinalState] +0xEC (0xDB9F54) != 0 → it (0x750225..0x75026F), then Infos[TOP]
  +0xEC (0x750275..0x750293).

"The final state's exit is ExitBuilding" is FUN_00753140 0x753140: GetFinalState (vt +0xB04, 0x753143), then the exit
member pointer (slot +0x20) of the row 0xD091B8 + 0x90·s compared with &ExitBuilding, Villager::ExitBuilding 0x7597B0, with zero adjustors
(0x75314D..0x75318C, the compare at 0x753162).

CARRIED_OBJECT values (`CarriedObject::Init`, table 0xC5E19C): 1 NONE, 6 BAG, 12 WOOD, 13..15 TREE_1..3; 16 and above
are not carried (CreateDroppedResource 0x75095F).

#### Taking from and giving to a structure

- **The storage pit** (`Villager::GetStoragePit` 0x751F10): the town's (`Town::GetStoragePit` 0x73B5B0) or, without
  one, **the villager's own home**.
- **The resource edge** (vt +0x8D4, GetResourceNearestEdge): StoragePit::GetResourceNearestEdge 0x733400 = its
  GetArrivePos (vt +0x104); MultiMapFixed::GetResourceNearestEdge 0x401590 → GetResourcePos (vt +0x8CC, 0x401560) = its position (+0x14).
- **The drop-off point** (Villager::GetResourceDropoffPos 0x753E20 (type)): GetStoragePit functional (vt +0xD4) → its
  GetArrivePos (0x753E49..0x753E79); else, with a town, the town's storage pit functional → its GetArrivePos
  (0x753E7C..0x753E99); else with a town the point of `Town::GetTemporaryResourceStorePotOrPos` 0x73E900
  (0x753E9B..0x753ECC): the town's temporary pot of that type and its edge towards the villager, made now if the town
  has none (a side effect, literal), the point returned whole (0x753EB0..0x753EC9); without a town, the villager's
  position (0x753ED3..0x753EEB). The pots are on
  [The town's temporary pots](towns.md#the-towns-temporary-pots).
- **`Villager::GetResourceFrom` 0x753390**(object, type, n): c = object.RemoveResource(type, (movsx) n, 0, 0) (vt +0xA0,
  0x7533A0..0x7533AD); a negative n is a huge unsigned: the whole store (literal). `test bx, bx` (0x7533B5): c != 0 →
  PickupResource(type, c, object.GetCarriedTreeType) (0x7533BA..0x7533C9; Object 0x402AF0 is `xor eax, eax`).
  object.IsSpeedUp (vt +0x4A8, 0x7533CE..0x7533E2; GameThingWithPos 0x402410 is `xor al, al` for homes and storage
  pits, the piles have their own) → SetFoodSpeedup; object.IsPoisoned (vt +0x4A4, 0x7533E8..0x7533FC) →
  SetPoisoned(1). Returns c.
- **`Villager::AtStructureRemoveResource` 0x76A2F0**(object, type, n): pos = object.GetResourceNearestEdge(type, this, 1)
  (0x76A31C). Within the villager's 2D radius of it (`IsCloseToEqual`, `MapCoords::IsCloseToEqual` 0x6053C0 with GetRadius, vt +0x60 =
  Get2DRadius: distance ≤ r, 0x76A33A..0x76A351): c = GetResourceFrom; 0 → 0, c < n (`cmp, sbb`) → 0x24, else 1
  (0x76A353..0x76A381). Not there → SetupMoveToWithHug(pos, GetFinalState), 0x24 (0x76A384..0x76A39E). 0x24 is the
  value of VILLAGER_STATE_GO_HOME reused as "on the way".
- **`AtStructureAddResource` 0x76A3B0**(object, type, &n, …): p = GetPlayer (vt +0x1C); for the object's own player p =
  0; status = p ? fn_0064A9C0(p) (the player's first non-null interface slot of +0x14[0..17] → +0x39C) : 0
  (0x76A3C9..0x76A3EA). 🔍 inferred: a villager drops into its own town's pit or home, so the status is 0 in every
  path. edge = object.GetResourceNearestEdge(type, this, 0) (0x76A3EC..0x76A3FD). Within the villager's **speed**
  (GetSpeedInMetres, vt +0x130, MobileWallHug 0x60C070; not the radius, unlike AtStructureRemoveResource) of the edge
  (0x76A41B..0x76A435): added = object.AddResource(type, n, status, 0, 0, status != 0) (vt +0x9C,
  0x76A437..0x76A451); 0 → 0 (0x76A459); else DropResource(type, added), n = added, 1 (0x76A45D..0x76A46C). Not there →
  SetupMoveToWithHug(edge, GetFinalState), n = 0, 0x24 (0x76A484..0x76A4A8).

#### Dropping off: 31 and 32

- **31 GOTO_STORAGE_PIT_FOR_DROP_OFF** (`Villager::GotoStoragePitForDropOff` 0x769620): the pit (or home) functional
  (vt +0xD4) → SetupMoveToOnFootpath(pit, pit.GetArrivePos (vt +0x104), 0x20 32), 1 (0x769626..0x769670). Holding
  nothing (GetResourceHeld's type neither 0 FOOD nor 1 WOOD) → SetTopState(0xA3 163), 0 (0x76967A..0x7696A2). Else
  SetupMoveToWithHug(GetResourceDropoffPos(type), 0x20 32), 1 (0x7696A9..0x7696C4). It is also called as a function by
  CheckSatisfyFoodDesire (0x75A094) and CheckSatisfyWoodDesire (0x75F506), which return its result.
- **32 ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF** (0x7696D0, clip 347 P_PUT_DOWN_BAG): n = GetResourceHeld(&type) (only the
  larger of the two: the other stays in hand, literal); 0 → SetTopState(163), 1 (0x7696D5..0x7696E7 → 0x7697D7).
  With a functional pit: AtStructureAddResource(pit, type, &n, 0) (0x769708..0x769717); 0x24 → 1
  (0x76971C..0x76971F → 0x7697E6); added or refused, the walk back always goes to the pit's arrive point:
  SetupMoveToOnFootpath(pit, pit.GetArrivePos, 0xA3 163), 1 (0x769725..0x769748), not 163 at once.
- **32 without a functional pit or home**: without a town → SetTopState(163), 1 (0x76974D..0x769756 → 0x7697D7).
  Else pot = Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900 (asking creates it; null → 163, 1;
  0x769758..0x769790); AreWeThere(pos, 0) (vt +0x85C, 0x769792..0x7697A5); there: pot.AddResource(type, n, 0, 0, 0, 0)
  (vt +0x9C, `PotStructure::AddResource` 0x66ED70: object_resources::AddResource), the result ignored
  (0x7697A7..0x7697BD); DropFood(n) for 0 FOOD, else DropWood(n), then SetTopState(163), 1 (0x7697C3..0x7697FD). Not
  there → SetupMoveToWithHug(pos, GetFinalState), 1 (0x7697FF..0x769817).

#### Taking from a pit: 34

`ArrivesAtStoragePitForResource` 0x7698D0(type, n, ok, fail):

- n ≤ 0 (unsigned: 0) → SetTopState(fail), 1 (0x7698DC..0x7698E1).
- With a functional pit: m = min(n, pit.GetResource(type)) (`cmp; jb` keeps n, 0x76990D..0x769939); m == 0 →
  SetTopState(fail), 0 (0x76993B). AtStructureRemoveResource(pit, type, m, 0) (0x76993F..0x76994C): 0x24 → 0x24 (on
  the way, or part of it; 0x769951); 1 with ok != 0 → SetupMoveToOnFootpath(pit, pit.GetArrivePos, ok), 1
  (0x769960..0x769994); else SetTopState(fail), 0 (0x7699A3).
- No functional pit and no town → SetTopState(fail), 1 (0x7699BE..0x7699C7 → 0x769B0A).
- **The pot branch**: pot = Town::GetTemporaryResourceStorePotOrPos(me, &pos, type) 0x73E900, its point not used here
  (0x7699CD..0x7699FD). p = pot.GetNearestEdgeOfObject(this) (vt +0x834, Object 0x636CD0 → GetNearestPosOfObject
  0x636D30: the pot's and the villager's 2D radii), back to MapCoords (x / z: ftol(x 6553.6); the altitude, which
  AreWeThere does not read, is dropped) (0x769A03..0x769A88). AreWeThere(p, 0) (vt +0x85C, 0x769A8F). There:
  pot.RemoveResource(type, n, 0, 0) (vt +0xA0, 0x769A99..0x769AA3), **its result discarded** (eax is overwritten at
  0x769AA9), then PickupResource(type, n, pot.GetCarriedTreeType (vt +0x820, Object 0x402AF0: 0))
  (0x769AA9..0x769AB8). So a villager takes the whole meal even from an empty temporary pot, and since there is no
  GetResourceFrom there is no poison and no food speed-up from it (literal quirk, not verified in the real game).
  Then ok != 0 → SetTopState(ok), 1 (0x769ABD..0x769AC7); else SetTopState(fail), 0 (0x769AC9..0x769ADB). Not there →
  SetupMoveToWithHug(p, GetFinalState), 0x24 (0x769AE4..0x769AFE).

The Food desire weighs the jobs and the drop-off against each other (CheckSatisfyFoodDesire 0x759F30, see
[Food and wood jobs](#food-and-wood-jobs-reactions-7-and-12)); its drop-off score is
`GDM(distance, 500) × frac` with frac = (float)(1 − (capacity + 1e-5) / (MaxFoodCarried + 1e-5)) (stored), `held`
being +0xF4 and capacity = (int16)(max − held): `fild (movsx) capacity; fadd 1e-5; fild qword MaxFoodCarried; fadd
1e-5; fdivp; fsubr 1; fstp float` (0x75A031..0x75A058, within 0x75A010..0x75A07C), then GetDistanceModifier(distance,
500) × frac (0x75A061..0x75A07C).

#### Speed

SetStateSpeed 0x753760 (0x75397B..0x753B2A) multiplies the base speed by:

- **Load factors** `clamp(1 + SpeedModWhenFullLoad − held / max, 0.75, 1)`: the maxima are MaxTraderWood /
  FoodCarried (+0x278 / +0x274) for disciple 9 TRADER, else +0x268 / +0x264, each `fild qword` (the high dword 0:
  unsigned; exact in a float: at most 500 in info.dat) (0x7539F8..0x753A53). Wood: (SpeedModWhenFullLoadOfWood
  (+0x374) + 1) − (movsx +0xF6) / MW, stored as a float (0x753A57..0x753A81); food: (SpeedModWhenFullLoadOfFood
  (+0x378) + 1) − (movsx +0xF4) / MF, kept on the stack (0x753A85..0x753A9B). woodF < 0.75 (0x8AB274) → 0.75, > 1 → 1
  (0x753A9D..0x753AC9); the same for foodF, with 0.75 too, not +0x378 (0x753AD1..0x753AF7). With wood 0.75 and food
  0.85, a villager is slower only above 187.5 wood / 127.5 food.
- **The town-needs term** `BaseForTownNeedsSpeedMod (+0x36C, read at 0x7539AE) + clamp(S / DivisorForTownNeedsSpeedMod
  (+0x370), 0, 0.5)` (0x7539AE..0x7539F4; < 0 → 0, > 0.5 → 0.5, 0x7539C2..0x7539F4); info.dat has 0.85 and 2.0, so
  `0.85 + clamp(S / 2, 0, 0.5)`. S = clamp(0.2 × the sum of ten raw desires (13, 12, 9, 7, 6, 5, 4, 3, 1, 0), 0, 1)
  (fn_00747150). Villagers of towns with positive desires walk up to 1.35 / 0.85 = 1.59 times faster than with the
  base 0.85 alone.
- **The food speed-up** ×4 (+0x39C) when IsFoodSpeedUp 0x55C980.

The whole formula is on [Speed](villager-speed-and-scripts.md#speed).

#### The dropped log

A villager with wood > 50 lets its log fall as a Pine DeadTree when it is put into physics by anything but the hand
(`Villager::InitialisePhysics` 0x5EFEF0), is thrown by the hand without landing (0x6373FA), or dies (VillagerDead
0x7507D0, which also drops food and wood to nothing, 0x7507D9 / 0x7507E2). Villager::CreateDroppedResource 0x750940 (a, b, c):

- c0 = +0xF1 (SkeletalAnimation::carriedObject; 1 NONE without one) (0x750950). Nothing (and no DropWood) unless c0 is
  2..15 (c0 ≤ 1 `jle` or c0 ≥ 16 `jge`, 0x750950..0x750962) and wood (movsx +0xF6) > MinWoodToShowGraphic (+0x26C)
  (`jle`, 0x750968..0x75097A): 0x750956..0x75097A → 0x750A9A.
- The log is DeadTree::Create fn_00510BB0 (0x510BB0)(my position, GTreeInfo Pine 0xDA49D8, 0, scale 1.0, angle π/2,
  0, 0, mesh CarriedObject[c0], the mesh override 0xCC5F10) with life 1 (the 4th argument, 0x7509B0 → the ctor's
  SetLife) and scale 1.
- +0x9C = wood / the new log's GetWoodValue (vt +0x664, DeadTree 0x511AD0 = life × woodValue × scale³; 0x7509D7):
  `fild wood` (stored as a float, exact); `fdivr GetWoodValue`; `fstp +0x9C` (0x7509C6..0x7509E7). 51 wood gives back
  a 51 log (DeadTree::GetDefaultResource 0x511330: 350 × float(51/350) = 50.99999905 rounds to 51.0f, the x87 at 24
  bits, 0x7DEE0D).
- Then InitialisePhysics (vt +0x784) with the velocity a (none: 0, and b ignored), the angular b, po +0x90 = c when
  given, po +0x1D8 |= 0x10, `PhysOb::AdjustToGroundLevel` 0x7FCB80 and
  PhysicsObject::RaiseUntilNotIntersecting 0x644800; finally DropWood(0): all the wood (0x750A91..0x750A95).

Reaction 9 (REACT_TO_FLYING_OBJECT) is not about picking up resources: it makes villagers flee (6) or point (162) at a
flying object ([A thing thrown from the hand](villager-reactions.md#a-thing-thrown-from-the-hand-reaction-9)). Picking
up dropped food is reaction 7, wood reaction 12.

### Builders

The building states are 39 (at the pit for materials), 40 (at the site), 41 BUILDING and 184 (re-entering the
building state). A BUILDER disciple is +0xF2 == 4. WoodUsedPerBuildCycle is info +0x27C (GetWoodUsedPerBuild
0x758E20).

#### From the desires to a site

The town desires Abodes (5), Civic_Buildings (6), To_Build (9) and Repair_Town (12) send a villager from 163 to a site.

- **CheckNeededForBuilding** 0x758340 (any site): GetTown (vt +0x48, 0x758343); `Town::IsBuildingHappening` 0x73E2F0
  (+0x794 != 0), else 0 (0x75834A); GetBestBuildingSite(me +0x14, includeFull = +0xF2 == 4, `sete al`)
  (0x758353..0x75836F); a site and SetupBuildingObject(site) == 1 → 1 (0x758375..0x758387).
- **Abodes, CheckSatisfyAbodesDesire** 0x758E30: CheckNeededForBuilding == 1 → 1 (0x758E33; **any** site, not only an
  abode's: literal). Then once a turn per town: Town +0x5E4 == 0 → +0x5E4 = 1, before the request, also when it fails
  (0x758E46..0x758E57); `Town::RequestANewAbode`(2) 0x73B330 == 1 && CheckNeededForBuilding == 1 → 1
  (0x758E61..0x758E80).
- **Civic, CheckSatisfyCivicBuildings** 0x758E90: the same with CheckNeededForBuilding (0x758E93), +0x5E4 first
  (0x758EA6..0x758EB7) and `Town::RequestBestPlanned` 0x73A650 (called at 0x758ECA; 0x758EC1..0x758EDE).
- **To_Build, CheckSatisfyToBuild** 0x759330: GetTown, else 0 (0x759333); GetBestBuildingSite(me +0x14, +0xF2 == 4);
  a site and SetupBuildingObject == 1 → 1 (0x75933C..0x759351).
- **Repair_Town, `Villager::CheckSatisfyToRepair`** 0x759370: `Town::GetBestRepairBuildingSite` 0x747EA0 (0x75937E);
  a site and SetupBuildingObject == 1 → 1. Only the sites with +0x638 and a desire > 0 (ProcessTownRepairs' ones): a
  rock-damaged house's site is ordinary and is repaired through To_Build instead (literal: Repair_Town rises for it and
  this returns 0; see [Repairs](#repairs-the-tap-on-a-home-and-the-town-emergency)).
- **The desire values**: To_Build sums the sites' GetDesireForVillagers (newest first), its modification the sites'
  builders and places; Repair_Town adds the plans' GetDesireToBeRepaired after the abodes
  ([Desire functions](villager-decisions.md#desire-functions)).

**`Villager::SetupBuildingObject(BuildingSite*)` 0x7584B0**: GetTown, else 0 (0x7584BA); the site's building
(`BuildingSite::GetBuilding` 0x43BC70), else 0 (0x7584C4..0x7584CD); a building IsBuilt (vt +0x890) == 1 and
IsRepaired (vt +0x88C) != 0 is refused, 0 (0x7584D3..0x7584EA); CheckForClearArea(building +0x14, GetClearAreaRadius
0x43BDE0) == 1 → 1 (0x7584EE..0x758505); else SetupGetBuildingSupplies(site) == 1 ? 1 : 0 (0x758510..0x758525). It is
also called by the worship side, CheckNeededForWorshipSiteBuilding 0x76C98A. SetupBuildingObjectForBuilding tests
`SetupBuildingObject(site) == 1` (`dec, neg, sbb, inc`, 0x758584..0x758591).

**`Villager::SetupBuildingObject(MultiMapFixed*)` 0x758530** (an abode; called by ArrivesHome 0x760AB3 and
CheckInteractWithAbode 0x7574C1, …): the building's site (`Town::GetBuildingSiteInList` 0x73CE40, else
`Town::AddBuildingSite` 0x73B8E0, looped on until a site appears: a repair site) and SetupBuildingObject(site). A
villager arriving at a damaged or unbuilt home thus makes the home's (repair) site.

**CheckForClearArea 0x7590A0 is always 0** in the game: it runs fn_0074E4C0(point, radius,
`ClearAreaPoint::ProcessPoint` 0x7591E0, 10.0, 1), keeping the nearest IsPushable object with CalculateForceAppliedBy > 0.05, but every Object vtable
points +0x814 at `Object::IsPushable` 0x402AE0 (`xor eax, eax`; 135 vtables): no Object class is pushable. The visit
draws no GameRand.

#### Fetching the wood

**SetupGetBuildingSupplies** 0x7586E0: GetTown, else 0 (0x7586EB); `Town::IsBuildingSiteValid` 0x73CF00, else 0
(0x758706); `BuildingSite::ShouldIGetWood`(me) 0x43C680 == 0 → straight to the ring, GotoBuildingSite(site)
(0x75871A..0x758726; it asks for GetResourceDropoffPos(WOOD) 0x753E20 itself, only where it needs it). g_game +0x14 &
0x40000 → SetupWaitForWood 0x7585A0 and on (0x758733..0x758744); 🔍 inferred the bit is never set in the game (the only
reference in .text; GGame init writes g_game +0x14 = 0x20). Then DecideHowToGetWood(1, &bigForest, &forest) (0x758757):

- 3 forest → +0xFC = site; VillagerGotoForest(forest, 0x31 49 FORESTER_ARRIVES_AT_FOREST) 0x75F720, its result
  (0x75876F..0x758788).
- 2 BigForest → +0xFC = site; SetupMoveToOnFootpath(bigForest, BigForest::GetArrivePos(me) 0x439360, 0x35 53
  ARRIVES_AT_BIG_FOREST); 1 (0x75878B..0x7587B9).
- 1 store → GotoStoragePitForBuildingMaterials(site) (0x7587BF).

**DecideHowToGetWood** 0x75F510(mode, &bigForest, &forest): mode 1 is the builder's, mode 0 the wood desire's
(CheckSatisfyWoodDesire, 0x75F4B4).

- pos = (0, 0, 0); D = GTownInfo maxDistanceForTownForest ([0xDA28E4], 250) (0x75F51D..0x75F538).
- **The store**: GetStoragePit 0x751F10 (the town's or the home) and its IsFunctional (vt +0xD4) → its GetArrivePos
  (vt +0x104) and GetResource(WOOD) (vt +0x98) (0x75F53E..0x75F57C); else Town::GetTemporaryResourceStorePotOrPos(me,
  &pos, WOOD) 0x73E900 (called at 0x75F596; the pot is made if the town has none: a side effect, literal) and the pot's
  GetResource(WOOD) (0x75F57E..0x75F5A6). GetWoodCapacity is read at 0x75F5AB, its result unused.
- **The weights** (0x75F5B4..0x75F62F, GetWoodCapacity again). Builder mode (1): the forest 0.5 ([0x8AA3B4]); the store
  1 when its stock (u32) is above the capacity (movsx, compared unsigned as (u32)(int32): `jbe`), else 0
  (0x75F5B8..0x75F5E0). A negative capacity (more than MaxWoodCarried held) is huge unsigned, so the store weight is 0
  (literal). Mode 0: frac = (float)(1 − (capacity + 1e-5) / (MaxWoodCarried (+0x268) + 1e-5)) (as the food
  drop-off's); the store scores frac, the forest (float)(1 − frac) (0x75F5E2..0x75F62F).
- **The store's score**: GetDistanceModifier(GetDistanceInMetres(pos, me) (fn_00605CD0, 0x75F63C), D) × storeW, stored
  as a float (0x75F63C..0x75F65A).
- **The forest**: with a town `Town::FindNearestForestToPos`(me) 0x73EC10, none → FindForest(me, D, 0) fn_0053A1A0
  (0x75F660..0x75F693). Its score is a forest ? GDM(GetDistanceInMetres(forest +0x14, me), D) × forestW : 0
  (0x75F673..0x75F6A1; the distance 0x75F673..0x75F68A, from the forest's MapCoords +0x14 at 0x75F675).
- **The choice**: store > forest (`fcomp; test ah, 0x41; jne`: the store wins only when strictly higher) → 1
  (0x75F6AC..0x75F6C1). Else forest != 0 and a forest: its +0x38 (the BigForest) → 2, with *bigForest = +0x38
  (0x75F6E2), else 3 with *forest = it (0x75F6F5) (0x75F6C4..0x75F6FF); else 0 (0x75F63C..0x75F6EC; `isBigForest` is the forest's +0x38).

**GotoStoragePitForBuildingMaterials** 0x7587D0 (to the pit's wood edge, FINAL 39): GetTown and IsBuildingSiteValid,
else 0 (0x7587DD / 0x7587F8). GetWoodCapacity ≤ 0 (`test ax, ax; jg`) → GotoBuildingSite(site)
(0x758824..0x758831). GetStoragePit 0x751F10, kept even when it is not functional (0x758841); pos =
pit.GetResourceNearestEdge(WOOD, me, 0) (vt +0x8D4, 0x75885A..0x758882); without a pit, GetResourceDropoffPos(WOOD)
0x753E20 (which may make the town's temporary wood pot; 0x75888D). The villager must be in the site's builder list
(+0x18, 0x7588A6..0x7588B6), or the site needs builders (NeedsBuilders fn_0043BC60), or be a BUILDER disciple, else 0
(`!NeedsBuilders && +0xF2 != 4`, 0x7588BA..0x7588D4). When the exit of the raw TOP's (+0x8C) row is not ExitBuilding
0x7597B0, +0xFC = site (0x7588D7..0x758912). A pit (or home), functional or not (literal) → SetupMoveToOnFootpath(pit,
pos, 39); else SetupMoveToWithHug(pos, 39); 1 (0x75891E..0x758940).

**39 ArrivesAtStoragePitForBuildingMaterials** 0x758990 (clip 340): +0xFC null or not a valid site → SetTopState(163),
1 (0x75899C / 0x7589A6 → 0x7589E4). cap = (int)(movsx GetWoodCapacity) (0x7589B1); cap == 0 → GotoBuildingSite(site)
== 1 ? 1 : SetTopState(163), 1 (0x7589BD..0x7589C8). Else ArrivesAtStoragePitForResource(WOOD, cap, 184, 163)
(0x7589CD..0x7589DC): up to its capacity from the pit and on to 184. The cap is pushed as is: a negative cap is a huge
unsigned n (literal).

The forest results (2 BigForest / 3 forest) walk to the forest (see
[Food and wood jobs](#food-and-wood-jobs-reactions-7-and-12)).

#### The ring: 40 and 184

- **GotoBuildingSite** 0x758A00: GetTown and IsBuildingSiteValid, else 0 (0x758A0E / 0x758A22); not in the builder
  list, !NeedsBuilders and +0xF2 != 4 → 0 (0x758A2F..0x758A53). SetTopState(163) (0x758A62): the builder leaves
  through ExitBuilding (RemoveBuilder, +0xFC = 0) and re-enters the list; +0xFC = site after the SetTopState
  (0x758A74); GetRandomBuildPos(&pos, me, &+0x118) (vt +0x128, one GameFloatRand; 0x758A7F); the walk with FINAL 40
  (0x758A8E..0x758ACD).
- **The walk to a ring point** (GotoBuildingSite 0x758A8E..0x758ACD, ReenterBuildingState 0x758FF5..0x759032): beyond
  40 m (`k_FootpathDistance` [0xC23704] = 40.0, read at 0x758A9B and 0x759002; `fcomp; test ah, 0x41`: strictly)
  SetupMoveToOnFootpath(GetBuilding(site), pos, 40), else SetupMoveToWithHug(pos, 40).
- **184 ReenterBuildingState** 0x758F60, also where a builder resumes after a reaction (the "after" state of the
  building rows): GetTown and IsBuildingSiteValid, else SetTopState(163), 1 (0x758F74 / 0x758F8B → 0x75904F).
  ShouldIGetWood → SetupGetBuildingSupplies == 1 ? 1 : SetTopState(163), 1 (0x758F94, 0x759042..0x759058).
  GetRandomBuildPos(&pos, me, &+0x118) (one GameFloatRand, drawn also when touching; 0x758FB3). Touching the building,
  IsTouching(GetBuilding(site), 0.001) (vt +0x6B8, Villager 0x55C9A0 → Object 0x637E00; the margin 0x3A83126F at
  0x758FCF) → SetTopState(40), 1 (0x758FCA..0x758FEB); else the walk with FINAL 40 (0x758FF5..0x759032).
- **40 ArrivesAtBuildingSite** 0x758AF0 (clip 348): !IsBuildingSiteValid(GetTown, +0xFC) → SetTopState(163), 1, +0xFC
  kept (the exit clears it) (0x758B05 → 0x758C25). The index +0x118 must be in 0..0x7F (`test eax, eax; jl` → 163;
  `cmp eax, 0x80; jge` → 163, the ring's 128 entries, 0x758B20; 0x758B12..0x758B25). The ring point is
  (ftol(ring[i].x × 6553.6), ftol(ring[i].z × 6553.6), 0) (0x758B2B..0x758B7D). GetDistanceInMetres(me, pos)
  fn_00605CD0 > 0.2 m ([0x8AB244]; `test ah, 0x41; jne`: the walk only when strictly farther) → SetupMoveToWithHug(pos,
  40), 1 (0x758B85..0x758BAF, the test 0x758B85..0x758B9A). At the point it turns to the building:
  LookAtObject(GetBuilding(site), 1) 0x5EC520 (`push 1` at 0x758BBA; null → 1; else LookAtPos(its +0x14, 1)) != 1 → 1,
  still turning (0x758BB8..0x758BC8). Then with wood carried (+0xF6): site.AddResource(WOOD, (int16)+0xF6, 0, 0, &me
  +0x14, 0) (vt +0x9C, its result ignored), DropWood(0) (**all** of it, whatever the pile took: literal) and
  SetStateCarriedObject (0x758BCA..0x758BFC). Finally t = +0x90; PlayAnimThenSetState(41) 0x5ECAC0 (TOP 23
  WAIT_FOR_ANIMATION, FINAL 41); +0x90 = t, keeping the state counter, so 23 ends when the 348 clip begun on entering 40
  has played once (0x758C01..0x758C13).

#### The build cycle: 41

State 41 BUILDING is `Villager::Building` 0x758C40, run once per play of the BuildingAnimation clip (0x423E20: hammer
276 / saw 354 / mallet 380):

1. GetTown, else 0 (0x758C55). IsReadyForNewAnimation(1) 0x5EC960 (the clip played once), else 1
   (0x758C62..0x758C69); +0x90 = 0, one build cycle per clip (0x758C6F). !IsBuildingSiteValid → release (0x758C82).
2. **The build factor** `f = 1 − 0.2·alignment`, the alignment from `MapCoords::GetAlignment` 0x6057B0 on +0x14
   (LandAlignmentAt, 0x758C8F..0x758C92): `fmul 0.2f; fsubr 1.0; fst f` ([0x8AB244] 0.2 = 0x3E4CCCCD, [0x8AA390] 1.0;
   0x758C97..0x758CA3); fe < 1 (`test ah, 1`) → 1.0 (0x758CA7..0x758CBC); else the stored f > 1.2f (`test ah, 0x41;
   jne` skips when ≤) → 1.2f, `k_MaxBuildFactor` [0x8C6C98] = 0x3F99999A (0x758CBE..0x758CD7, read at 0x758CC2,
   0x758CCF). So 1 on good land, at most 1.2 (0x758C97..0x758CD7).
3. **The amount** u = min(ftol(WoodUsedPerBuildCycle × f), pile): GetWoodUsedPerBuild 0x758E20 (+0x27C, a float);
   `fmul f`; `__ftol` (truncation; a float product at 24 bits) (0x758CDA..0x758CE3); then GetResource(WOOD) (vt +0x98)
   < u (`cmp; jae`, unsigned) → u = it, else the same product again (0x758CEA..0x758D18; 0x758CDA..0x758D18). That is 50
   for the men, 40 for the housewives. GetBuilding is kept for the IsBuilt test below (0x758D1C).
4. **Building** when u != 0 (0x758D21..0x758D71): x = (float)u / GetWoodValue 0x43C0C0, the value read **before** the
   removal: `fild qword u; fstp float; fdivr GetWoodValue; fstp float` (0x758D27..0x758D51). site.RemoveResource(WOOD,
   u, 0, 0) (vt +0xA0), its result ignored (0x758D55). fn_0043D080 → GetBuilding→BuildBy(x) (vt +0x900; may Build and
   delete the site; 0x758D62). The wood-used stat fn_0073B620(GetTown, u) → TownStats +0xEC, and the player's
   GameStats +0xA44 +0xA8 (0x758D71).
5. **Then**: !site.IsAvailable (vt +0x2C, GameThing 0x401810) → release: the site was deleted by Built, and the walk out
   ToBeDeleted gave this villager is replaced by the 163 below (literal) (0x758D7A). IsBuilt && IsRepaired → release,
   without RemoveBuilder (literal) (0x758D87..0x758D9E). Wood left in the pile → the next ring point,
   GetNextPosFromIndex(&pos, &+0x118) (vt +0x124: GameFloatRand then GameRand(2)); SetupMoveToWithHug(pos, 40); 1
   (0x758DA6..0x758DCF). None → SetupGetBuildingSupplies(site): 0 leaves the builder in 41, hammering an empty site
   (literal) (0x758DE3).
6. **The release** sets +0xFC = 0 **first**, so ExitBuilding skips RemoveBuilder, then SetTopState(163); 1
   (0x758DEF..0x758E02).

A hut (1400) takes 29 cycles of 50 / 35 of 40; the temple (6500) would take 130.

#### The builder count

- **EnterBuilding** 0x759750 (the entry of 39 / 40 / 41 / 184): IsBuildingSiteValid(GetTown, +0xFC), else 0, refused:
  the Villager wrapper enters 163 (0x759762..0x75976D). When the previous final state had another entry
  (!IsStateEntryFunctionSameAs(final, next) 0x7524D0) → AddBuilder(+0xFC, me) 0x43BE40 (0x759788..0x759794).
- **ExitBuilding** 0x7597B0: towards another building state (IsStateExitFunctionSameAs(next), vt +0x96C 0x752530) → 1,
  the builder stays one (0x7597BB). Else GetTown && IsBuildingSiteValid(+0xFC) → RemoveBuilder(+0xFC, me) 0x43BE90
  (0x7597CC..0x7597F7); +0xFC = 0; 1 (0x7597FC).
- So hunger, the hand, death and reactions take the villager out of the list; after a reaction 184 finds +0xFC = 0 and
  the builder goes to 163 🔍 inferred.

#### Never entered, and other addresses

- Never entered in the game: 51, 54, 185, 188, 189, 232 (no state change pushes them; 232 WAIT_FOR_WOOD needs g_game
  +0x14 & 0x40000, never set). `Villager::ForesterChopsTreeForBuilding` 0x75FB40 (row 51) is SetTopState(163), 1;
  `Villager::ArrivesAtBigForestForBuilding` 0x75FAB0 (row 54) is `mov eax, 1; ret`, `return 1`.
- `BuildingSite::GetBuilding` 0x43BC70; GetClearAreaRadius 0x43BDE0; NeedsBuilders fn_0043BC60; AddBuilder 0x43BE40;
  RemoveBuilder 0x43BE90; GetWoodValue 0x43C0C0; GetWoodNeededToBuild 0x43C5F0; BuildBy fn_0043D080 → the building's
  vt +0x900; `Town::IsBuildingSiteValid` 0x73CF00; SetupWaitForWood 0x7585A0.

| Constant | Value | Where |
|---|---|---|
| `k_PointTwo` | [0x8AB244] = 0x3E4CCCCD (0.2) | the alignment's weight (Building 0x758C97) and the ring point's reach (ArrivesAtBuildingSite 0x758B85) |
| `k_MaxBuildFactor` | [0x8C6C98] = 0x3F99999A (1.2) | the build factor's cap (0x758CC2, 0x758CCF) |
| `k_FootpathDistance` | [0xC23704] = 40.0 | the footpath beyond it (GotoBuildingSite 0x758A9B, ReenterBuildingState 0x759002) |
| `k_TouchingMargin` | 0x3A83126F (0.001) | ReenterBuildingState's IsTouching margin (0x758FCF) |
| `k_RingSize` | 0x80 | the ring's entries (ArrivesAtBuildingSite `cmp eax, 0x80` 0x758B20) |
| LookAtObject's mode | 1 | ArrivesAtBuildingSite `push 1` 0x758BBA |

### Repairs, the tap on a home and the town emergency

#### Repairs

- **A repairer is a builder** ([Builders](#builders)): no state of its own. On a built building SetupBuildingObject
  0x7584B0 goes on as for a build; the wood fetched is `GetWoodNeededToBuild` = (1 − life) × WoodValue − pile; each
  cycle 41 adds `x = u / GetWoodValue` with BuildBy → IncreaseLife (capped at 1) and at life 1 Repaired deletes the
  site (the builders are released as at Built). Cycles from life 0.6: Norse hut 12 of 50 / 14 of 40, storage pit 33 /
  41; from 0.1 the Citadel heart 117 / 147 (float model).
- **RepairAmount** (GVillagerInfo +0x280) has no reader in the game: no access to +0x280 in the villager, abode or
  site code (the exe's 63 instructions with that displacement all belong to other classes).
- **Who comes**: a rock / spell / fire hit goes through `Abode::ReduceLife` 0x405D90, whose site is **ordinary**
  (+0x638 = bit 2 of +0x58, 0 on a finished house). So `Villager::CheckSatisfyToRepair` 0x759370 (desire 12, only
  +0x638 sites with a desire > 0: ProcessTownRepairs' ones) does not take it; the ordinary builders do (To_Build 9,
  Abodes 5, Civic 6 through GetBestBuildingSite), while `GetBuildersNeeded` > 0, i.e. life ≤ 0.9 and, for a house,
  someone lives there. Literal: Repair_Town still rises for every damaged abode (no site filter); a house at life in
  (0.9, 1) keeps an unserved site (only BUILDER disciples repair above 0.9). A rebuild plan's site has +0x638 = 1 but
  its new building starts at life 1, so it is built through To_Build too.
- **Its own inhabitants** (ArrivesHome 0x760930): arriving at a home that is not built-and-at-full-life, with life ≥
  0.3 and food ≥ 0.5 (strict), SetupBuildingObject(abode) 0x758530 (the site made if missing); 1 → it repairs, else
  in. Hungry: in (with a SetTopState(163) overwritten when the home is not functional, literal).

#### The tap on a home

- **`Villager::SetStateWhenTappedOnAbode` 0x752B80** is called by `Abode::ReduceLife` 0x405DF3, for each +0xA0
  inhabitant when the new life is below 1, and by `Abode::InterfaceTap` 0x406855. Only a villager available (vt +0x2C,
  `Villager::IsAvailable` 0x751D50) and **inside** its home (+0xE0 & 4) answers: SetupAfterTapOnAbode(FindPosOutsideAbode
  (NULL) 0x753470, 0xA3 163); 1 (0x752BA6..0x752BB2). FindPosOutsideAbode draws GameFloatRand(1.5) then
  GameFloatRand(pi/4). Its 1 / 0 result is discarded by both callers (they discard eax).
- **`Villager::SetupAfterTapOnAbode` 0x761400**(pos, s): LivingAction::SetState(2, s) 0x5ECC90 on +0x8C, the raw
  PREVIOUS = 163 (no Villager::SetState, so no town modifier and no +0x10 filter; 0x761404..0x761410);
  Villager::SetupMoveToPos 0x763800 (pos, 0xC5 197 AFTER_TAP_ON_ABODE) (= `Living::SetupMoveToPos` 0x5F2830;
  0x761414..0x761421): the walk leaving the at-home row runs its exit ExitAtHome 0x761B40, which takes the villager out
  of the home; +0xE0 |= 1 (0x761426): no own desires and no sleep until the next share-out clears it (read by
  GetOwnDesiresTrigger 0x7581E6 / CheckSatisfySleep 0x761493, cleared by fn_7581A0).
- **197** (AfterTapOnAbode 0x761440): PlayAnimThenSetState(+0x8E, 1) 0x5ECAC0; 1 (0x761443..0x761451): the yawn, then
  PREVIOUS, 163.

#### The emergency call

- **Who answers** (the body of CallAllVillagersToTownEmergency 0x747890, run at once from `Town::SetInStateOfEmergency`
  0x7479A0 when +0xF1C was 0): every housed villager whose **GetFinalState** row (vt +0xB04 = 0x751DD0, `& 0xFF`) has
  a +0x50 answer (0x7478B6..0x7478CB; 0x7478B6..0x747922) goes to SetTopState(242); its exit runs, so a builder leaves
  the site. The +0x50 slot (k_TownEmergencyReaction): Always (0x5AC990) → true; PreviousState
  (`Villager::LookAtPreviousStateReactToTownEmergency` 0x756520, `movzx eax, byte +0x8E`, the caller's `test eax, eax`;
  row 220 only) → PREVIOUS (+0x8E) != 0; an empty slot → false, not called (0x7478D1 / 0x747908).
- **The rows**: 206 rows always answer (walks, jobs, carrying, going home 35..37, 163, reactions); 220 MOVE_AROUND_FIRE
  answers when its PREVIOUS is not 0; 48 never (at home 38 / 100 / 118..120, in the hand, script, dance, dying / dead,
  fainting, the fire rows 215..219, 242 / 243, chilling out 245 / 246 / 252). So a villager walking home (FINAL 37
  ARRIVES_HOME) answers, as does a walk to a site (FINAL 40); only one already inside (TOP 38 / 100 / 118..120) stays.
  No villager walk has FINAL 38 in the game (the only `push 0x26` before SetupMoveToPos / SetupMoveToWithHug is the
  cow's 0x41D3D7, an animal enum). Homeless villagers join when they next run 163 or 246.
- **242 GotoCongregateInTownAfterEmergency** 0x76B200: GetTown (vt +0x48), else 0, staying in 242 (literal)
  (0x76B208..0x76B213). pos = `Town::GetCongregationPos` 0x7408B0 (0x76B21A..0x76B222); n = +0x618 adults + +0x61C
  children (a u32 add, 0x76B22E..0x76B24D). The distance: k = n × 0.025f (`fild qword` of the u32 count,
  0x76B24D..0x76B251); !(k < 1) → 1 (`fcom; test ah, 1`, 0x76B255..0x76B296); d = (float)(k × 20 + 10)
  (0x76B29C..0x76B2A9; 0x76B24D..0x76B2A9 in all): 10 m for an empty town, 20 m at 20 people, 30 m from 40 on. The
  angle a = GameFloatRand(2 pi = 0x40C90FDB) (0x76B2AF..0x76B2BB); pos += GetPosFromAngle(a, d) 0x74D580
  (MapCoords::operator+= 0x605410; 0x76B2C1..0x76B2D8); SetupMoveToWithHug(pos, 0xF3 243); 1 (0x76B2DD..0x76B2F3).
  The constants: 0.025 [0x8D150C], 20.0 [0x8C7658], 10.0 [0x8AB414].
- **243 CongregateInTownAfterEmergency** 0x76B300 (TownEmergencyAnimation, one clip per stop): GetTown &&
  `Town::IsInStateOfEmergency` 0x747970 (0x76B303..0x76B31C). During the emergency GameRand(12) (0x76B31E): != 0 → 242,
  0 → 0x24 36 GO_HOME (`neg; sbb; and 0xCE; add 0x24`, 0x76B31E..0x76B343). After it GameRand(5) (0x76B34F): != 0 →
  242, 0 → 0xA3 163 (`and 0x4F; add 0xA3`, 0x76B34F..0x76B374).
- During the 1200 turns DecideWhatToDo sends every villager that reaches 163 to 242, so a damaged pit is only repaired
  after it (inferred, not verified in the original).

### Food and wood jobs, reactions 7 and 12

#### The Food desire

**CheckSatisfyFoodDesire** 0x759F30 weighs three job candidates, in k order, each the strictly best of its list
(newest first):

- **Fish farms** (FindBestFishFarm, fn_0073E750): `Score × GDM(d, 500)`. best = 0, bestFarm = 0
  (0x73E75B..0x73E767); the town's fish farms +0x788 from the head (0x73E772..0x73E7CC); fn_00605CD0 (farm +0x14, me
  +0x14) (0x73E779); the farm's score fn_0052D2F0 (0x73E784): (float)(u64)Score, `fild qword {score, 0}` (the u32 as a
  u64), `fstp float`, × GetDistanceModifier(d, 500 (0x43FA0000)) (`fmul` of the stored float, 0x73E7A7;
  0x73E784..0x73E7AC); `fcom best; test ah, 0x41; jne`: only strictly above (0x73E7B3..0x73E7BC); *score = best when
  the pointer is given (0x73E7D0..0x73E7DC). A fish farm scores 1 **only with no fisherman** (`ftol(1 − min(1, n /
  4))`): through the desires a farm gets one fisherman. Puzzle fish farms have no town, so no desire reaches them.
- **Fields** (`Town::FindBestField` 0x73E870; Town::FindBestField 0x73E8A4..0x73E8C2 is the score): `GDM(d, 300) × GetDesireToBeFarmed`. *score = 0 (unconditional), best =
  0, bestField = 0 (0x73E87B); the town's fields +0x780 from the head (newest first, 0x73E87D..0x73E8E2); fn_00605CD0
  (field +0x14, me +0x14) (0x73E89F); GetDesireToBeFarmed (0x73E8AA), stored (`fstp float`); GDM(d, 300 =
  0x43960000) (0x74F290, 0x73E8BD; the limit at 0x73E8B7) `fmul` (0x73E8AA..0x73E8C2; 0x73E8A4..0x73E8C2); `fcom
  best; test ah, 0x41; jne`: only strictly above (0x73E8C9..0x73E8D8); *score = best (0x73E8EA..0x73E8F2).
- **Flocks without a shepherd** (fn_0073E7F0): `GDM(d, 300)`.

They are sorted high to low, ties keeping k's order. The drop-off (`GDM(d, 500) × frac`, see
[Carrying](#carrying-resources-and-the-storehouse)) beats the head only when strictly higher; then the head's job. A
free flock at the head goes to VillagerBecomesShepherd 0x768BE0.

#### Farmers

The farmers are bw1-decomp's `VillagerFarmer.cpp` (0x759BF0..0x75A35E). They sow and harvest at random points within
5 m of the field's centre. Villager +0x118 is the field, +0x11C the work point.

- **VillagerBecomesFarmer** (Villager::VillagerBecomesFarmer 0x759C00, with the field): no field → GetTown (none → 0) and `Town::FindBestField`(me, &tmp) (none →
  0) (0x759C04..0x759C23); then SetFarmerGotoField(field, 0) (0x759C25..0x759C2A).
- **SetFarmerGotoField** 0x759C40: GetArrivePos (vt +0x104), copied before the activity is asked (0x759C54);
  Field::GetFieldActivity 0x529350 (a) (1 sow, 2 harvest, else 0; a never read; 0x759C83); 1 (0x759CC8) and 2
  (0x759C98) are the same code: SetTopState(163) (vt +0x8E8; a farming exit clears +0x118 of a previous field first;
  0x759C98..0x759CA1); +0x118 = the field (0x759CB1); SetupMoveToOnFootpath(field, arrive, 0x43 67) 0x5EDD20 (0x759CB7)
  to its centre; **then** +0x11C = RandomFarmPoint fn_00528970 of +0x118 (two GameFloatRand(10), x then z;
  0x759CBC..0x759D0F). Nothing between 0x759CB1 and 0x759CBC changes +0x118 (the footpath setup does not), so it is
  always the field.
- **67 FarmerArrivesAtFarm** 0x759D20 (clip 259): GetFieldActivity(0) (0x759D47); activity 0 → SetTopState(163), 1
  (0x759DBC). Sowing: AreWeThere(+0x11C, 0) (vt +0x85C, 0x759DEA); at the point +0xF1 = 1 NONE (0x759DE3, overwritten
  by the state change that follows either way), +0x11C = RandomFarmPoint (2 draws) and PlayAnimThenSetState(0x44 68,
  1) (0x759DF4..0x759E1A); else the walk to it, `Villager::SetupMoveToPos`(+0x11C, 67) 0x763800 (0x759E2A).
  Harvesting: RipeFarmPoint fn_00529240(me, &discard): unripe → 163 (no draw); ripe: 2 draws, the point discarded
  (0x759D56..0x759D69); AreWeThere(+0x11C, 0) (0x759D6B..0x759D80); at the point +0x11C = RandomFarmPoint and
  PlayAnimThenSetState(0x45 69, 1) (0x759D86..0x759DAC).
- **68 FARMER_PLANTS_CROP** (`Villager::FarmerPlantsCrop` 0x759EC0): PlantCrop(my pos) 0x5291A0 (the position is not
  read) false → 163 (0x759EC3..0x759ED4); else GetPlantCropPos 0x529210 (still sowing) ? 67 : 163
  (0x759ED6..0x759F01); 1. One crop per clip; 30 crops a field, shared.
- **69 FARMER_DIGS_UP_CROP** (`Villager::FarmerDigsUpCrop` 0x759E40, clip 257 P_FARMER_HARVESTING): cap = movsx
  GetFoodCapacity; Field::RemoveFood 0x5295A0 ((float)cap), the EAX result (0x759E4D..0x759E63); `test eax, eax; jbe`
  (unsigned): anything but 0 (0x759E68) → PickupFood((int16)got) 0x751490 (0x759E6F). Then the "overfull" test,
  (float)(int16)GetFoodCapacity < [0xD394DC] (`k_DigCapacityLimit`, 0.0, never written; `test ah, 1`;
  0x759E74..0x759E99) → GotoStoragePitForDropOff 0x769620, its result: a negative capacity (> 150 held) goes to 31.
  Else SetTopState(0x43 67), 1 (0x759EA8). **Quirk**: with a full load (capacity 0) the farmer digs for 0 again until
  something else takes it out (literal).
- **The field's farmer list** (EnterFarming / ExitFarming, the entry and exit of rows 67-69): added once on the first
  farming state, kept across 67 ↔ 68 / 69 and the walks (same entry / exit functions), removed (and +0x118 cleared) on
  any other state. `Villager::EnterFarming`(final, next) 0x75A250: +0x118 == 0 → 0, refused: the Villager wrapper
  enters 163 (0x75A253..0x75A260); another entry function before (!IsStateEntryFunctionSameAs(final, next) 0x7524D0) →
  GetTown (unused), AddFarmer(me) 0x5283E0 (0x75A263..0x75A291); 1. `Villager::ExitFarming`(next) 0x75A2A0: the same
  exit → 1; else with +0x118 and the field still in the global field list g_game +0x205C04 (next +0xC4) →
  RemoveFarmer fn_00528340, which clears +0x118; 1. For a deleted field +0x118 is kept.
- Disciples: DiscipleDecideWhatToDo 0x751771 asks the best field's activity.

#### Fishermen

- **FishermanLookForWater** (`Villager::FishermanLookForWater` 0x75B4C0, from DiscipleDecideWhatToDo 0x7517CE): GetTown,
  then fn_0073E750 on GetTown (called again) (0x75B4C6..0x75B4EB); a town and a best fish farm → VillagerBecomesFisherman
  (its result unused), 1 (0x75B4F0); else 0.
- **VillagerBecomesFisherman** 0x75B560: SetTopState(163) (vt +0x8E8, 0x75B568); +0x118 = farm (0x75B57B);
  SetupMoveToOnFootpath(+0x118, farm.GetArrivePos (vt +0x104 = `FishFarm::GetArrivePos` 0x52C490), 0x37 55)
  (0x75B584..0x75B5BC).
- **55 FishermanArrivesAtFishing** 0x75B5D0: out of the farm's map cell (!IsAtValidFishingPos) →
  SetupMoveToOnFootpath(farm, farm.GetArrivePos, 55), 1 (0x75B5DD..0x75B60E).
  Villager::IsAtValidFishingPos 0x75B670: the high words of x and z (+0x16, +0x1A: the map cell) equal the farm's, `(u16)me +0x16 == farm +0x16 && me
  +0x1A == farm +0x1A` (0x75B670..0x75B688). On the arrive point (my +0x14 == farm.GetArrivePos, an exact
  `MapCoords::operator==` 0x605660: x and z `cmp`, then the altitude `fcompp` / `test ah, 0x40`, checked against the
  exe; 0x75B616..0x75B627) → the fishing spot, `Villager::SetupMoveToPos`(FishingSpot fn_0052C870 (2 GameFloatRand(5) −
  2.5, x then z), 55) (0x75B629..0x75B63E). The walk ends with Pos = goal (MobileWallHug::MoveTo's ARRIVED 0x60AFC0 /
  FINAL_STEP snap), so a villager that walked to the arrive point is on it. Else PlayAnimThenSetState(0x38 56, 1), 1
  (0x75B64E).
- **56 Fishing** 0x75B6A0 (clip 262), once a clip: the roll GameRand(farm +0x84, the fishermen) must be 0. The catch
  (0x75B6F8..0x75B7A4; GetSeason and GetFoodCapacity are read twice on the clamp path):
  - base = (float)((qword)MaxFoodCarried) × 0.25 [0x8AB3D4] (`fild qword {MaxFoodCarried, 0}; fmul 0.25; fstp float`):
    37.5 for 150 (0x75B6F8..0x75B735).
  - the season's multiplier, a stack table (0x75B70F..0x75B72D, indexed `[esp + eax × 4 + 0x14]` at 0x75B75B): 1.0,
    0.9 (0x3F666666), 0.7 (0x3F333333), 0.6 (0x3F19999A) for GetSeason 0..3 (GetSeason never gives more).
  - v = base × mult; v > (float)cap (`test ah, 0x41; jne` keeps v when ≤) → cap (0x75B74A..0x75B78C; 0x75B757..0x75B7A4).
  - n = ftol(GetTribalPower(7) × v) (`__ftol`, truncation, a 32-bit int; 0x75B796..0x75B7A4). Tribal power index 7 for
    every tribe, literal. By season: 37 / 33 / 26 / 22.
  - f = `fild qword {n, 0}` (the u32 as a u64) stored as a float; f != 0 → PickupFood((int16)ftol(f))
    (0x75B7A9..0x75B7C4). (float)GetFoodCapacity < f (`test ah, 1`) → GotoStoragePitForDropOff (0x75B7D7..0x75B7F2);
    GetFoodCapacity != 0 → 1, else GotoStoragePitForDropOff (0x75B7F4..0x75B7FE, 0x75B800): 31 when full.
  - The farm's stock is never touched.
- **The farm's fisherman list**: `Villager::EnterFishing`(final, next) 0x75B820 (entry of 55 / 56): the same entry
  function → 1 (55 → 56 adds nothing); no town → 1; not in the farm's list (+0x80) → AddFisherman fn_0052D250 (which
  writes +0x118 = the farm); 1. `Villager::ExitFishing`(next) 0x75B880 (exit of 55 / 56): the same exit → 1; +0x118
  available → RemoveFisherman 0x52D290; +0x118 = 0; 1.

#### Foresters

- **The Wood desire** (CheckSatisfyWoodDesire 0x75F4A0) = DecideHowToGetWood(0, &bigForest, &forest) 0x75F510
  (0x75F4B4; see [Fetching the wood](#fetching-the-wood)): 1 → GotoStoragePitForDropOff 0x769620, its result
  (0x75F506), so 31; 2 → SetupMoveToOnFootpath(big, BigForest::GetArrivePos(me) 0x439360, 0x35 53), 1
  (0x75F4DC..0x75F4FA); 3 → VillagerGotoForest(forest, 0x31 49), its result (0x75F4C9..0x75F4D2). 48
  FORESTER_GOTO_FOREST (`Villager::ForesterGotoForest` 0x75F710) is `jmp CheckSatisfyWoodDesire`.
- **VillagerGotoForest** 0x75F720: `Forest::GetForestCentreTree` 0x53ABF0 (0x75F72D); with one its GetWorkingPos(me)
  (Tree 0x74C040), else the forest's +0x14 (its centre); SetupMoveToWithHug(p, state) 0x5F2890 (0x75F793); then
  LivingAction::SetState(0, 0x2F) 0x5ECC90, the **raw** TOP 47 (index 0: +0x90 = 0 too; no exit / entry / clips;
  0x75F79C..0x75F7A6).
- **47 FORESTER_MOVE_TO_FOREST** (`Villager::ForesterMoveToForest` 0x75F7D0; `moveResult` is Living::MoveToPos 0x5EC270's,
  the MOVE_TO_POS step the table runs first): MoveToPos == 7 (a new map cell) && (u8)+0x5E == 2 → the look-ahead
  (0x75F7D6..0x75F7E8); 1 (0x75F918). MobileWallHug +0x5E == 2 is never set, so the look-ahead is off (literal). It
  (0x75F7EE..0x75F913) probes me + fn_0074D6A0(+0x5C + offset, 10.0) (MapCoords::operator+ 0x605520;
  0x75F821..0x75F848) at the game angle + {0xFE00, 0, 0x200} (0x75F7F3..0x75F7FF). From every FindType(6) object of
  the probe's cell (the chain), as a Tree with a forest (+0x68) (0x75F84D..0x75F8E1), it takes the one whose working
  point is nearest the probe, d = GetDistanceInMetres(TreeWorkingPos(tree, me), probe) < best (`test ah, 1`;
  0x75F885..0x75F8B2), first best 10000; then SetupMoveToWithHug(TreeWorkingPos(best, me), 49) (0x75F8F5..0x75F913).
  The probe distance 0x41200000 = 10.0 and first best 0x461C3C00 = 10000.0 (0x75F82D / 0x75F806).
- **Touching** (`Villager::IsTouching`(pos) 0x753040, vt +0x6B4): GetSpeedInMetres (vt +0x130, 0x75305A) >
  GetDistanceFromObject(pos) (vt +0x13C = Object 0x4027C0 → GameThingWithPos 0x5702B0: distance − radius; 0x75304C)
  (`fcomp; test ah, 0x41; jne` → 0: equal is not touching).
- **49 ForesterArrivesAtForest** 0x75F930 (clip 217): FindTreeNearVillager(&tree) 0x75FD00 (0x75F93B). Touching the
  nearest tree → +0x118 = 0; SetGameAngle(+0x5C − 0x124) 0x60DA90 (0x124 game angles of 2048 a turn, 0x75F951);
  PlayAnimThenSetState(0x32 50, 1), 1 (0x75F94D..0x75F973). A tree not touched → SetupMoveToWithHug(GetWorkingPos(me),
  0x31 49) != 0 (0x75F97D..0x75F9A1). None → an empty forest within 50 m, fn_0053A1A0(me, 50.0, onlyEmpty 1), is
  deleted by its ToBeDeleted (Forest vt +0xC = DeleteForest) (0x75F9A8..0x75F9C5); SetTopState(0x34 52) and
  ForesterFinishedForestering's result (0x75F9C8..0x75F9D6).
- **50 ForesterChopsTree** 0x75FAC0: +0x118 == 0 && FindTreeNearVillager(&tree) != 0 (touching or not)
  (0x75FAC4..0x75FADA) → FellTree: FelledTree::Create(tree, me) 0x5116A0 (0x75FAE2) and tree.ToBeDeleted(0)
  (0x75FAF8) ([Felling a tree](#felling-a-tree)); +0x118 = the felled tree **after** it (0x75FAEE): its reactions 12
  spread inside, so a villager that takes one changes state before +0x118 is written. While it is in the physics it
  waits: +0x118 && +0x24 & 0x40 && IsAvailable == 1 → 1, still falling, the clip 217 looping (0x75FB03..0x75FB1B);
  +0x24 & 0x40 IN_PHYSICS (set at 0x636F2F, cleared at 0x6375A0 / 0x646B56) is PhysicsObjects::IsFlying (in the
  physics and not a resting proxy), as in 22. Then +0x118 = 0; SetTopState(0x34 52), 1 (0x75FB1D..0x75FB2D).
- **52 FORESTER_FINISHED_FORESTERING** (`Villager::ForesterFinishedForestering` 0x75FB60): (int16)+0xF6 > 0 (`jle`) →
  `jmp GotWoodDecideWhatToDo` (0x75FB60..0x75FB68); else SetTopState(163), 1 (0x75FB6F..0x75FB7C). **The forester
  does not pick the log up**: reaction 12 does.
- **GotWoodDecideWhatToDo** (Villager::GotWoodDecideWhatToDo 0x75FC30, from 52, 53 at 0x75FA63, and 22 at 0x76489F): (u16)+0xF6 == 0 (`jbe`) →
  SetTopState(163) (0x75FC33..0x75FC3B → 0x75FCDC). A disciple held at the job (+0xE0 bit 9 && g_DiscipleInfos[+0xF2]
  +0xC == 1) → 163 (0x75FC41..0x75FC6A). Its building site (+0xFC) valid (GetTown, `Town::IsBuildingSiteValid`
  0x73CF00) with builders needed (fn_0043BC60) or a BUILDER disciple (+0xF2 == 4), and GotoBuildingSite 0x758A00 == 1 →
  1 (0x75FC6C..0x75FCBC). CheckNeededForBuilding 0x758340 != 0 → 1 (0x75FCBE..0x75FCC7). Else SetTopState(0x1F 31
  GOTO_STORAGE_PIT_FOR_DROP_OFF), 1 (0x75FCC9..0x75FCD5). Always 1.
- **53 ArrivesAtBigForest** 0x75F9E0: cap = movsx GetWoodCapacity; GetTown && cap != 0, a negative one goes on
  (0x75F9E8..0x75F9FD). `Town::FindNearestForestToPos`(me) 0x73EC10 and its +0x38, the BigForest (0x75F9FF..0x75FA13).
  p = BigForest::GetArrivePos(me) 0x439360; AreWeThere(p, 0) (vt +0x85C) (0x75FA15..0x75FA35); not there →
  SetupMoveToWithHug(p, GetFinalState), 1 (0x75FA83..0x75FA95). There: n = RemoveResource(WOOD, (u32)cap, 0, 0) (vt
  +0xA0 = BigForest 0x4390D0; a negative cap is a huge unsigned, literal; 0x75FA37..0x75FA48); n != 0 → PickupWood(n,
  GetCarriedTreeType 0x438DE0, type 3) (0x75FA4A..0x75FA63) and GotWoodDecideWhatToDo; then SetTopState(163) at
  0x75FA68 **even after** GotWoodDecideWhatToDo: what it set up is overwritten (literal); 1.
- **186 TAKE_WOOD_FROM_TREE** (`Villager::TakeWoodFromTree` 0x75FBC0): FindTreeNearVillager == 0 → SetTopState(163), 1
  (0x75FBC9..0x75FBD2); else GetPlayer (vt +0x1C) → `GPlayer::MakeCreatureEmpathiseWithPlayerTownDesire`(1, 0.5, +0x14)
  0x4C80F0 (0x75FBD4..0x75FBF1), then SetTopState(0x31 49) (0x75FBF6..0x75FC04) and ForesterArrivesAtForest's result.
- **187 TAKE_WOOD_FROM_POT** (`Villager::TakeWoodFromPot` 0x75FBB0): `mov eax, 1; ret`, `return 1`.
- **ExitForesting** (`Villager::ExitForesting`(next) 0x75FE20, the exit of 47-50, 52): +0x118 = 0; 1.

#### Reactions 7 and 12

- **The slot**: Living +0x94 / +0xBC hold the reaction and its object (the type table 0xC09CC0 has a row for every
  type, described on
  [Running from and watching miracles](villager-reactions.md#running-from-and-watching-miracles-reactions-3-21-and-37)).
  The slot table: setup vt +0x9BC Living 0x5F14C0 (food) / vt +0x9C0 Villager 0x765B70 (wood); turnsToReact vt +0xAC0
  Living 0x5F18C0; turnsBeforeAgain vt +0xAC4 Living 0x5F1920, and for wood vt +0xAD8 Villager 0x7648D0 (the final
  state 47..50 or 52 → 0, else the standard; it tests the final state as a signed byte at 0x7648DB..0x7648ED: `0x2F <=
  s <= 0x32` or `s == 0x34` → 0). Initialiser 0x6E0E80. Priorities: food 60 [0xD4F97C], wood 55 [0xD4FB70];
  maxReactionDistance 35 for both ([0xD4F998] / [0xD4FB8C]).
- **The spread's per-villager body** 0x6E3F90: the whole `Villager::IsAvailableForReaction` 0x763390 (worship site,
  football, death states, life ≤ 0.15 except for 7, Living 0x5F11F0; `Villager::IsAvailableForBeliefButNotReaction` is
  0x763410, vt +0x988). With no reaction held: score > 0 (fn_006E4620: the type's priority, counted only when
  isReacting[t] (GLivingInfo +0x144 + 4t) is set and d ≤ maxReactionDistance) and the records (again 0 for both:
  47-50 / 52 make 12's 0 anyway) → MarkStarted and the setup (AddReaction 0x763440: StorePreviousState only without a
  reaction, SetTopState, then +0x94). With a reaction held: the switch rule (never to the same type, +0x04 = 0; food ↔
  wood after max(10, cur / new × 20 − 10) s, the seconds counted as (now − RecordTurn(current type)) / (1000 /
  [0xD01A38]), both unsigned divisions; SetReactionDoneWhen).
- **The end**: ProcessReaction 0x5F1270 ends it (reaction gone → StopReacting; object gone → StopReactingAndSetState;
  past the standard 2000..3000 turns). ReactionValidate reads ReactionInfo (`Reaction::GetInfo` 0x6E4709, table
  0xD4F6B0) +0x28 whetherReactionFinishesIfInitiatorInHand (0xD4F6D8). `Reaction::ShutDown` 0x6E4720 (RemoveAll* /
  Prune: a handler per Living class, before the erase) stops its followers newest first; it runs at the initiator's
  ToBeDeleted. `CircleHugInfo::Reset` 0x60A9F0 sets TurnsToObj to 0xFF.
- **7, food**: ReactToFoodPriority 0x5F1710: not in the hand or physics; the object has food (GetResource vt +0x98);
  within ReactionInfo[7] +0x44 (100 m). IsInterestedInFoodObject 0x764DF0: `boost × capacity share × POWER(food) ×
  state interest × GDM(d, 35)` > 0.25, else the town's GetDesire(FOOD) term (its trigger GetInfo(0) +0x18) > 0.01 → 19
  (walk to its working point) → **20 ArrivesAtFoodReaction** 0x764920: a disciple held at the job → 163 before the
  drop-off test. Food is taken **only from a PileFood** (the pit's pile, the hand's, magic food: a plain Food Pot
  attracts and gives nothing, literal); hungry → 117; else `CalculateDesireForFood × load share × GDM(d pit, 100)` >
  0.1 → 31, else 163. The drop-off modifier is GDM(d, MaxDistCarryFoodPit, info +0x350, 100) on the LOAD fraction
  (held / Max), not the capacity: 0x7649DB..0x7649FF take GetResourceDropoffPos(FOOD) 0x753E20 (which may make the
  town's MagicFood pot) and fn_005EC9F0(that pos, 100).
- **12, wood**: ReactToWoodPriority 0x5F17A0 has only the hand test (no physics, resource or distance test).
  IsInterestedInWoodObject 0x764F60: wood capacity == 0 → 0; no town → 0; the villager's building site (+0xFC) valid
  with GetWoodNeededToBuild ≤ 0 → 0; a building-site pile (IsAPotFromABuildingSite) or a wood-fetching disciple
  (+0x10), when IsVillagerAvailable → 1; a **dead tree with less than 125 held** is always wanted; else the town's raw
  wood desire term (GetRawDesire(WOOD) and its trigger GetInfo(1) +0x18), with life > 0.3. The capacity fractions are
  capacity / Max (int16 / int). → 21 → **22** 0x764720: while the log falls (in the physics) it waits beside it (0.5 m,
  LookAt, 57 for 10 turns: SetupWaitForCounter(10, GetFinalState) 0x76B060), giving up after 1000 turns; landed →
  RemoveResource(capacity), PickupWood(its tree type), GotWoodDecideWhatToDo.
- **The walks 19 / 21** go through `Living::GotoFoodReaction` 0x5F2550 / `Living::GotoWoodReaction` 0x5F25C0: the object
  (+0xBC, a dynamic_cast to Object) still available → SetupMoveToWithHug(its working pos, final); else
  StopReactingAndSetState; both return 1. Villager 21 0x7646A0 first stops (and returns 1) when the object is gone, not
  available, in the hand or on fire (IsOnFire 0x637CC0; 0x7646D3..0x7646F9), then calls it with FINAL 0x16
  (0x7646FD). 22 also stops when the log is on fire.
- **Where the wood reactions come from**: a felled tree (the local player) makes one at once (the DeadTree ctor
  0x510880, 0x510957) and a second one after its physics set-up (0x511889). A dropped log (fn_00510BB0 → ctor
  0x510A30, 0x510BF4) makes none until it lands: `DeadTreeEndPhysicsReaction` (0x511413; not a felled tree), the
  DeadTree endPhysics handler, and `Tree::EndPhysics` 0x74BBE2 → `CreateDeadTreeReaction` for a tree thrown by the
  hand.
- **The object questions** the two reactions ask are virtuals with a default for every other class: IsSpeedUp (vt
  +0x4A8: Pot 0x55D4F0, bit 4 of +0x74, `Pot::speedUp`, for the pots and piles; GameThingWithPos 0x402410 → 0),
  IsDeadTree (vt +0x47C: DeadTree 0x510990; GameThingWithPos 0x402340 → 0), IsAPotFromABuildingSite (vt +0x830: Pot
  0x55D590, bit 3 of +0x74; Object 0x402B30 → 0; 🔍 inferred: the bit marks a building site's pile, its writer was
  not traced), GetResourceType (vt +0x690: DeadTree 0x5110C0 and Tree 0x74B820 → WOOD, PileFood 0x66EC50 → FOOD,
  PileWood 0x66EC60 → WOOD, Pot 0x55D4C0 → its +0x68, the GPotInfo resource type, 🔍 inferred copied by the
  constructor; Object 0x402750 → −1), RemoveResource (vt +0xA0: DeadTree 0x511370, PotStructure 0x66EE10, Pot
  0x66D3F0) and GetCarriedTreeType (vt +0x820: DeadTree 0x511A20, Tree 0x55D900, Object 0x402AF0 → 0).
- **The distance modifiers** of an object and of a position: fn_005EC9D0 (object, max) = GDM(GetDistanceInMetres(me,
  object), max), fn_005ECA20 being 0x74F290; fn_005EC9F0 (pos, max) does the same for a position.
- **SET_INTERACT_DESIRE** (feature script case 76, 0x71782F) writes g_game +0x205A50, the "interact" term of both
  interest scores (read at 0x764ED4, 0x764F20 and 0x765102). It starts at 0 (g_game is allocated zeroed) and
  `GGame::LoadMap` 0x552F40 sets it to 0 at 0x552F64, before LoadMapFeatures; the other LoadMapFeatures callers do not.
  No shipped script sets it.

### Beating out a fire

`Villager::PutOutFireByBeating` 0x75AC50 has no callers through a call instruction: it is reached from the fire rows
of the state table ([Fire in the table](../world/fire.md#fire-in-the-table)).

1. The villager must be beside the fire (fn_0075ABA0(fire, 2.0)).
2. It turns to the fire's object (`LookAtObject(object, 2)` at 0x75AC7F, the 2 its turn speed, at most 0x100 a turn)
   and returns 1 until it faces it.
3. It returns 1 until its clip is ready for a new one (`IsReadyForNewAnimation(1)`).
4. It strikes: an `EffectValues` of burn −8 applied through `FireEffect::ApplyEffectToFireEffectIfNecessary`.
5. While the fire is above its reaction temperature (`IsAboveReactionTemperature`, then vt +0x2C) it walks round it to
   its next fire-fighting position (`GetFireFightingPos`, `SetupMoveAroundFire`). When the fire has no object any more
   (`GetObjectA` 0x75ACCB, `jne` 0x75ACD8, 0x75ACDA..0x75ACDE) it returns 0 with no state change.
6. When the fire is not above its reaction temperature, or its vt +0x2C fails, it goes to state 0xA3 (DecideWhatToDo)
   and returns 1 (0x75AD70).

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `VillagerStates.cpp`, `VillagerCivic.cpp` (0x758340..0x75980F), `Tree.cpp`,
  `Forest.cpp`, `BigForest.cpp`; the rest read in the exe with the xref index.

## openblack

**Done**: carrying, builders and repairers, the tap and the emergency, farmers, fishermen, foresters, reactions 7 and
12, the tree and forest searches, beating out a fire (`PutOutFireByBeating`). **Not ported**: shepherds
(VillagerBecomesShepherd: a free flock at the head returns 0, approximate), the BUILDER disciple, the states 51, 54,
185, 188, 189, 232 (SetupWaitForWood / 232 WAIT_FOR_WOOD), SaveBuilding / LoadBuilding, the two felling reactions 0x0C
and the landing reaction of a dropped log, fn_0064A9C0 (AtStructureAddResource's status) and the player's GameStats
(+0xA44 +0xA8) of the build cycle. Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### By area

- **Trees** (API `src/ECS/Trees.h`): `DeleteTree`, `DeleteForest`, `ShrinkAllTrees`, `TreeWoodValue`, `TreeWood`,
  `RemoveWood`, `TreeCarriedType`, `FellTree`, `TreesInCell`, `FindTreeNearVillager`, `TreeWorkingPos`,
  `Object2DRadius`, `MakeScenicForest`, `AssignForestsToTown`, `TownForestId`, `FindNearestForestToPos`, `FindForest`,
  `ForestCentreTree`, `ForestWood`, `BigForestArrivePos`, `BigForestRemoveWood`, `ecs::BigForestStartWood` (it used
  max(wood value, 1) before), `ecs::CreateDroppedLog`. The generic deletion `ecs::ToBeDeleted`
  (`src/ECS/ToBeDeleted.cpp`) also sends Tree and DeadTree to `DeleteTree`. What a store receives goes through
  `object_delivery`, `ecs::held_apply` and `DepositInStore`. openblack has no per-cell lists: `Tree::mapInsertion`
  stores the insertion order (when creating the tree and when replanting it, `InsertMapObject` in
  `Fixed::EndPhysics`). The BigForest's Forest is ported too. A tree replanted in a village with no scenic forest is
  left with none (before, openblack created one). `AddTreeAround`'s maximum is 0.75 (before, openblack used 0.5).
- **Felling**: the tree and its trunk are the same entity (`ecs::FellTree` does both the Create and the ToBeDeleted),
  so the deletion listeners are told and the fire passes to the trunk. openblack's other nulls, a mesh not loaded or a
  chopper without a Transform, take the original's no-3D-object path. The spin axis is **negated**: openblack's
  right-handed `PhysicsBody` (was `PhysOb`) rotates by −angle for the original's `R(ŵ, angle)` (its sign convention,
  see [Physics](../physics/physics.md)).
- **Carrying**: `Villager/VillagerResources.{h,cpp}`, `Villager/VillagerSatisfy.cpp`, `VillagerSpeed.cpp`,
  `VillagerAnimate.cpp` (the carried object), `TownDesire` (`TownNeedsSum`, `town_desire::TownNeedsSum`),
  `VillagerPhysics.cpp` (the villager's InitialisePhysics); tests `test/test_villager_resources.cpp`. The load is
  `resourceHeld`, the tree type `k_FlagTreeTypeMask`, the town totals `TownStats::foodCarried / woodCarried`; the pot
  is `town_stores::GetTemporaryResourceStorePotOrPos`, reached through the villagerStores service (also for
  DecideHowToGetWood, `VillagerBuild.cpp`). `object_resources::AddResource` / `object_resources::RemoveResource` stand
  for the structures' AddResource / RemoveResource (no GInterfaceStatus); poison is
  `ecs::life::TakePoisonedResource`. The dropped log goes through the physics' `PhysicsObjects::AddDroppedObject`;
  **⚠️ approximate** from a physics release other than the hand the handler runs after the body was added and without
  the angular velocity.
- **Builders**: `Villager/VillagerBuild.{h,cpp}`, the rows 39 / 40 / 41 / 184 of `LivingActionSystem.cpp`,
  `TownDesire.cpp` (the site and plan inputs, `DesireInputsOf`); tests `test/test_villager_build.cpp`. The building
  side (sites, the 128-point ring, the pile, BuildBy, Built) is `ecs::building_sites` / `ecs::abodes`; the villager
  reaches it through `villager::BuildingSiteOps` (mocks in the tests). Town +0x5E4 is `requestedPlanThisTurn`.
  `abodes::BuildBy` skips a CitadelHeart (its plan is not ported), so the temple site is refused; the in-game check
  was done with the Aztec houses.
- **⚠️ approximate, the town's forest list**: it is not filled on load yet (+0x608, `Town::AsssignTownFeature` 0x73EAC0
  on load), so the global FindForest decides in DecideHowToGetWood; nobody calls `MakeScenicForest` and
  `AssignForestsToTown` but the hook `OPENBLACK_TEST_TREE_QUERIES`. openblack's Trees keeps no MapCoords for a forest,
  so **⚠️ approximate** the forest's MapCoords +0x14 (0x75F675) is made from its centre in metres (a round trip: may be
  one fixed unit off).
- **Repairs and the emergency**: `Villager/VillagerEmergency.{h,cpp}` (197, 242, 243, the tap, `CallToTownEmergency`),
  the +0x50 column `k_TownEmergencyReaction` in `Villager/VillagerOriginalFns.h`, ArrivesHome's repair branch
  (`VillagerHome.cpp`; `villager::GetTown`, VillagerHome.h), CheckSatisfyToRepair (`VillagerBuild.cpp`); tests
  `test/test_villager_repair.cpp`. The damage, the sites, ProcessTownRepairs, GetBestRepairBuildingSite, BuildBy /
  Repaired and SetInStateOfEmergency are the building side's (`ecs::abodes`, `ecs::building_sites`,
  `ecs::town_emergency`). The 2π draw is VillagerStates.cpp 0x3A1; GameRand(12) and GameRand(5) are 0x3B0 / 0x3AC.
- **Food and wood jobs**: `Villager/VillagerFarmer.{h,cpp}`, `Villager/VillagerFisherman.{h,cpp}`,
  `Villager/VillagerForester.{h,cpp}`, `Villager/VillagerSatisfy.cpp` (the Food desire),
  `Systems/Implementations/VillagerResourceReactions.{h,cpp}` (7 / 12), `Systems/Implementations/VillagerReactions.{h,cpp}`
  and `Effects/Reactions.{h,cpp}` (the slot framework), `Components/LivingReaction.h`, the rows 19-22, 47-50, 52, 53,
  55, 56, 67-69, 186, 187 of `LivingActionSystem.cpp` (47's MOVE_TO_POS step is LivingActionSystem's
  VillagerMoveToPos), the three wood reactions of `Trees.cpp`; tests `test/test_villager_farming.cpp`,
  `test_villager_reactions.cpp`, `test_villager_forester.cpp`. The field / fish-farm side is `ecs::fields` /
  `ecs::fish_farms` (reached through `villager::FieldOps` / `FishFarmOps`, mocks in the tests; the town's fields are
  `ecs::fields::TownFields`). Villager +0x118 is the field (`Villager::targetThing`), +0x11C the work point
  (`Villager::workPos`); the field side is reached through the Locator's villagerFields service
  (`Field::GetFieldActivity` too). The walk's snap at its goal is the PathfindingSystem's. One
  `components::LivingReaction` serves the types openblack runs through it (7, 12, and 3, 21, 37); **the miracle
  reaction types (fire, teleport, shield, death) keep their maps and code**: the slot is only consulted when a slot
  type meets them (their priority functions score them as the current reaction). 22's InPhysics test is
  VillagerResourceReactions'. SET_INTERACT_DESIRE is `MapScriptGlobals::interactDesire`, reset when a land loads
  (`ResetForLandLoad`).
- **⚠️ approximate, the reaction shutdown**: `Reaction::ShutDown` runs at BeginTurn's Prune (or an owner's
  RemoveAll*), not at the initiator's ToBeDeleted: a log or pile destroyed mid-turn reaches ProcessReaction's first
  test (StopReacting, no state reset) first.
- **Beating out a fire** differs in two places, both fix rows: where the fire has lost its object it goes to
  DecideWhatToDo and returns 1 instead of returning 0 with no state change
  (`src/ECS/Systems/Implementations/VillagerFire.cpp:661`); and it turns the villager to the fire at once instead of
  0x100 a turn with `LookAtObject(object, 2)` (`src/ECS/Systems/Implementations/VillagerFire.cpp:656`).

### Notes on the code

The functions that reproduce the facts above, by file, and what the port adds or leaves out.

| File | Functions and constants |
|---|---|
| `src/ECS/Villager/VillagerBuild.cpp` | `k_PointTwo`, `k_MaxBuildFactor`, `k_FootpathDistance`, `k_TouchingMargin`, `k_RingSize`, `k_LookMode`, `WalkToRingPoint`, `BuildFactor`, `BuildAmount`, `BuildStep`, `BuildPosOk`, `NearRingPoint`, `WoodSourceWeights`, `ChooseWoodSource`, `CheckNeededForBuilding`, `SetupBuildingObject`, `SetupBuildingObjectForBuilding`, `CheckSatisfyAbodesDesire`, `CheckSatisfyCivicBuildings`, `CheckSatisfyToBuild`, `CheckSatisfyToRepair`, `SetupGetBuildingSupplies`, `DecideHowToGetWood`, `GotoStoragePitForBuildingMaterials`, `GotoBuildingSite`, `EnterBuilding`, `ExitBuilding`, `ArrivesAtStoragePitForBuildingMaterials`, `ArrivesAtBuildingSite`, `BuildingState`, `ReenterBuildingState`, `DropOffFraction`, `GetStoragePit`, `GetTemporaryStore`, `ResourceType::Wood`, `GetWoodCapacity`, `GetResourceNearestEdge`, `GetResourceDropoffPos`, `IsBuildingExitState` |
| `src/ECS/Villager/VillagerBuild.h` | `BuildFactor`, `BuildAmount`, `BuildStep`, `BuildPosOk`, `NearRingPoint`, `SetupBuildingObject`, `SetupBuildingObjectForBuilding`, `BuildingState` |
| `src/ECS/Villager/VillagerEmergency.cpp` | `k_PerVillager`, `k_GoHomeOneIn`, `SetStateWhenTappedOnAbode`, `SetupAfterTapOnAbode`, `AfterTapOnAbode`, `ReactsToTownEmergency`, `CongregationDistance`, `GotoCongregateInTownAfterEmergency`, `CongregateInTownAfterEmergency`, `LivingAction::Index::Previous`, `VillagerStates::AfterTapOnAbode`, `VillagerStates::InvalidState`, `VillagerStates::CongregateInTownAfterEmergency` |
| `src/ECS/Villager/VillagerEmergency.h` | `SetStateWhenTappedOnAbode`, `SetupAfterTapOnAbode`, `ReactsToTownEmergency`, `CongregationDistance`, `AfterTapOnAbode` |
| `src/ECS/Villager/VillagerFarmer.cpp` | `k_FieldDistanceLimit` (0x43960000 = 300.0, FindBestField's limit), `k_DigCapacityLimit` ([0xD394DC] = 0.0), `k_CarriedNone` (FarmerArrivesAtFarm's 1 NONE), `FieldScore`, `FindBestField`, `VillagerBecomesFarmer`, `SetFarmerGotoField`, `FarmerArrivesAtFarm`, `FarmerPlantsCrop`, `FarmerDigsUpCrop`, `EnterFarming`, `VillagerStates::FarmerArrivesAtFarm`, `GetFoodCapacity`, `PickupFood` |
| `src/ECS/Villager/VillagerFarmer.h` | `FieldScore`, `OverFull`, `VillagerBecomesFarmer`, `FarmerPlantsCrop`, `FarmerDigsUpCrop`, `EnterFarming`, `ExitFarming`, `FieldActivityOf` |
| `src/ECS/Villager/VillagerFisherman.cpp` | `FishFarmScore`, `FishCatchBase`, `FishCatch`, `FindBestFishFarm`, `VillagerBecomesFisherman`, `FishermanLookForWater`, `IsAtValidFishingPos`, `FishermanArrivesAtFishing`, `Fishing`, `GetFoodCapacity` |
| `src/ECS/Villager/VillagerFisherman.h` | `FishFarmScore`, `FishCatchBase`, `FishSeasonMultiplier`, `FishCatch`, `IsAtValidFishingPos`, `EnterFishing`, `ExitFishing`, `FishermanLookForWater` |
| `src/ECS/Villager/VillagerForester.cpp` | `k_ChopTurn` (0x124), `k_ProbeMetres` (10.0 and 10000.0), `k_ProbeAngles` ({0xFE00, 0, 0x200}), `ForesterLookAhead`, `IsTouching`, `VillagerGotoForest`, `GotWoodDecideWhatToDo`, `CheckSatisfyWoodDesire`, `ForesterMoveToForest`, `ForesterGotoForest`, `ForesterArrivesAtForest`, `ForesterChopsTree`, `ForesterChopsTreeForBuilding`, `ForesterFinishedForestering`, `ArrivesAtBigForest`, `ArrivesAtBigForestForBuilding`, `TakeWoodFromTree`, `TakeWoodFromPot`, `ExitForesting`, `TouchingRule`, `CheckNeededForBuilding`, `DecideHowToGetWood`, `GotoStoragePitForDropOff`, `VillagerStates::ForesterFinishedForestering`, `GetWoodCapacity`, `VillagerStates::ForesterArrivesAtForest` |
| `src/ECS/Villager/VillagerForester.h` | `TouchingRule`, `ForesterGotoForest`, `TakeWoodFromTree`, `TakeWoodFromPot`, `ExitForesting`, `GotWoodDecideWhatToDo`, `ForesterMoveToForest`, `ForesterChopsTree`, `ForesterChopsTreeForBuilding`, `ForesterFinishedForestering`, `ArrivesAtBigForest`, `ArrivesAtBigForestForBuilding` |
| `src/ECS/Villager/VillagerResources.cpp` | `k_ExitBuilding`, `HeldLarger`, `WoodCarriedObject`, `IsBuildingExitState`, `CarriedObjectFor`, `LoadFactors`, `DropOffFraction`, `DropOffScore`, `DroppedLogFor`, `PickupResource`, `PickupFood`, `PickupWood`, `DropFood`, `DropWood`, `DropResource`, `GetFoodCapacity`, `GetWoodCapacity`, `AddResourceToVillager`, `GetResourceFrom`, `FinalStateIsBuilding`, `GetResourceDropoffPos`, `GetResourceNearestEdge`, `AtStructureRemoveResource`, `AtStructureAddResource`, `ArrivesAtStoragePitForResource`, `GotoStoragePitForDropOff`, `ArrivesAtStoragePitForDropOff`, `CreateDroppedResource`, `ExitBuilding`, `Villager::k_TreeTypeShift`, `VillagerStates::InScript`, `ResourceType::Wood`, `GetStoragePit`, `VillagerStates::InvalidState`, `TreeInfo::Pine` |
| `src/ECS/Villager/VillagerResources.h` | `WoodCarriedObject`, `IsBuildingExitState`, `TownNeedsFactor`, `DropOffFraction`, `PickupFood`, `PickupWood`, `GetWoodCarriedObject`, `FinalStateIsBuilding`, `SetStateCarriedObject`, `GetStoragePit`, `GetResourceDropoffPos`, `GetTemporaryStore`, `GetResourceNearestEdge`, `AtStructureRemoveResource`, `GotoStoragePitForDropOff`, `GetResourceFrom` |

`VillagerOriginalFns.h` keeps the original's state function addresses only (`k_OriginalStateFns`, the exit
ExitBuilding 0x7597B0 that `IsBuildingExitState` compares with).

**Guards the original does not have** (it dereferences or divides where openblack tests):

- `CheckNeededForBuilding`: a null-town test (the original's callers have a town). `DecideHowToGetWood`: no pot → 0;
  without a town the original dereferences null at 0x75F596 (openblack uses pos (0, 0), stock 0). `EnterBuilding` and
  `ArrivesAtStoragePitForBuildingMaterials`: no town → refused, where the original calls IsBuildingSiteValid on
  GetTown's null (0x75975D / 0x7589A1).
- `TownComponentOf`, `GetStoragePit`, `GetResourceDropoffPos`: GetTown 0x751F00 only reads +0x12C, and GetAbode
  0x752160 only reads +0x128; the guards stand for the missing town unlinking and Abode::ToBeDeleted's unlinking.
  `ArrivesAtStoragePitForResource`'s null pot → 0x769B0A happens only without a town in openblack.
- Test-only guards for the tests' zero-filled infos: `LoadFactors` with a capacity of 0 gives no load term (the
  original would give 0.75 there: 0 / 0 = NaN, then the clamp; info.dat has 150 / 250 / 500 in all 63 records);
  `TownNeedsFactor` with a divisor of 0 gives no term (the original would give 0.5: S / 0 = inf, then the clamp;
  info.dat has 2.0).
- `SetFarmerGotoField`: the fallback to `field` where the original reads +0x118 unchecked. `FishSeasonMultiplier`:
  the clamp to 3 where the original indexes the 4 floats unchecked (only a test could pass more).

**Other port notes**:

- `ArrivesAtBigForest` reads the carried type before the removal: the BigForest that gives its last wood is deleted.
  `ArrivesAtWoodReaction` (22) likewise reads the tree type before RemoveWood, which may delete the log that gave its
  last wood; the original reads it after, from the object still in memory.
- `TakeWoodFromTree` with no creature: the empathy is a no-op.
- `ForesterChopsTreeForBuilding` and `ArrivesAtBigForestForBuilding`: the rows stay k_TodoEntry (never entered in the
  game). `ForesterMoveToForest`'s look-ahead needs 7 and MobileWallHug +0x5E == 2: never in openblack (WallHugByte5E).
  **⚠️ approximate** its angle wraps with the 2048 mask (the original indexes past its table for a negative one).
- `FishermanArrivesAtFishing`: **⚠️ approximate** x and z only, and the arrive point through the metres the walk's
  goal went through (openblack keeps the villager in float metres; the altitude of a walking villager is not kept).
- `ArrivesAtStoragePitForResource`'s pot edge: **⚠️ approximate** without the original's LHPoint round trip (at most 1
  MapCoords unit).
- `CreateDroppedResource` in `VillagerResources.h`: the log is not made where there is no DeadTree::Create 0x510BB0 /
  InitialisePhysics API yet: **⚠️ approximate** the wood is lost.
- `src/ECS/Systems/Implementations/VillagerReactions.cpp` (the slot framework for the villagers): `SwitchAllowed` is
  the switch from the reaction a villager follows to a new one at distance d (the same type only with the table's
  +0x04 and never to the reaction it follows; the seconds since the current type's record turn, with a guard against
  a division by 0; ⚠️ approximate: a current miracle reaction no longer in the list scores 0, where the original's
  ShutDown would have stopped the villager first). `ApplySlotReaction` is the spread's body for a villager:
  turnsBeforeAgain, the score and the records, MarkStarted, the setup, SetReactionDoneWhen and leaving the current
  reaction's followers. `VillagerReaction` is the entry for every type: `IsAvailableForReaction` first, then for a
  miracle type met by a slot reaction the slot is unlinked and the miracle's own ApplyReaction runs, the slot being put
  back when it did not take the villager.
- `src/ECS/Systems/Implementations/VillagerResourceReactions.cpp` (reactions 7 and 12, rows 19-22): `ResourceTypeOf`,
  `RemoveWoodFrom` and `CarriedTreeTypeOf` stand for the GetResourceType, RemoveResource and GetCarriedTreeType
  virtuals; `DistanceModifierTo` is fn_005EC9D0; `DiscipleGoesBack` is the disciple test of 20 and of
  GotWoodDecideWhatToDo (`villager::DiscipleHeldAtJob`); `LivingGotoReaction` is the Living walk of 19 / 21;
  `FoodInterestScore` and `FoodDropOffScore` keep the original's order of float products; `ReactionFallLimit` is 22's
  give-up turns (100 s, with a guard against a division by 0); `IsForesterWorkState` is 12's turnsBeforeAgain state
  test; `IsInterestedInFoodObject` and `IsInterestedInWoodObject` are the two interest scores (the town terms read
  `TownDesireInfo::ForFood` and `TownDesireInfo::ForWood`, the distance `Reaction::ReactToWood`'s
  maxReactionDistance); `ReactToWoodPriority`, `SetupReactToWood` (AddReaction into `VillagerStates::GotoWoodReaction`;
  food's goes into `VillagerStates::GotoFoodReaction`) and `TurnsBeforeReactingToWoodAgain` fill the wood row;
  `GotoWoodReaction` and `ArrivesAtWoodReaction` are 21 and 22. The interact term is the value
  `FeatureScriptCommands::SetInteractDesire` stores in the map script globals.

### Test hooks

- `OPENBLACK_TEST_FELL="x,z"`, `OPENBLACK_TEST_TREE_QUERIES="x,z"`, `OPENBLACK_HAND_TEST_FOREST=1`.
- `OPENBLACK_TEST_VILLAGER_CARRY="<food|wood>,<amount>[,<n>[,<tree 0-3>]]"` (turn 2, through PickupResource); the
  trace adds `carry:`, `drop 31:` / `drop 32:`, `carried:`, `speed:`, `food-desire:`, `pot:` and `dropped log:`
  lines; `OPENBLACK_TOWN_TRACE` adds `pots:` / `pit:`.
- `OPENBLACK_TEST_BUILD_AT` and the build traces: see [Test hooks](villager-states.md#test-hooks).
- Repairs (`OPENBLACK_VILLAGER_TRACE`): `tap: inside -> out (x, z) prev 163` / `tap: not inside`, `197: yawn -> s`,
  `emergency: row f always|previous(p)|none -> 242|stay`, `242: congregation (x, z) n d a -> (x, z)` / `242: no town`,
  `243: emergency b r r -> s`, `home 37: repair abode e built b life l -> 0|1`, `repair: site … needs builders …
  builders … life …` (SetupBuildingObject on a built building), `repair desire: best repair site e -> r`.
- Jobs (`OPENBLACK_VILLAGER_TRACE`): `food-desire: farm <id> <s> field <id> <s> flock <id> <s> drop <s> ->
  31|fisher|farmer|shepherd TODO|0`, `farm: goto|add|remove`, `farm 67|68|69:`, `fish: goto|add|remove`, `fish
  55|56:`, `wood-desire:`, `forest: goto`, `forest 49|50|52|53:`, `gotwood:`, `react 7|12: <obj> ... -> setup|skip`,
  `react 19|20|21|22:`, `AddReaction`, `ProcessReaction:`. The existing hooks start the jobs:
  `OPENBLACK_TEST_TOWN_DESIRE="0,1"` (Food) / `"1,1"` (Wood), `OPENBLACK_TEST_FIELD_GROWTH=1200`,
  `OPENBLACK_TEST_FELL="x,z"`, `OPENBLACK_TEST_VILLAGER_CARRY`.

## Pending

- **Who calls `MakeScenicForest` and `AssignForestsToTown`** in openblack: nobody yet but the test hook, so the
  villages have no forest list and a tree replanted in a village is left without a forest; wire them where the
  original calls them (`Town::AsssignTownFeature` on map load, `Scaffold::BuildBuilding`). This also closes the
  approximate forest choice of DecideHowToGetWood.
- **The felling reactions 0x0C** (the DeadTree constructor's 0x510957 and `FelledTree::Create`'s 0x511889) and the
  dropped log's landing reaction (`DeadTreeEndPhysicsReaction` 0x511413, which has no caller yet in openblack; and
  `Tree::EndPhysics` 0x74BBE2 → `CreateDeadTreeReaction`) (queued fix).
- **Shepherds**: VillagerBecomesShepherd 0x768BE0 is not ported; a free flock at the head of the Food desire returns
  0 ⚠️ approximate.
- **The temple's build**: the CitadelHeart's plan is not ported, so `abodes::BuildBy` refuses the temple site (130
  cycles in the original).
- **The emergency's 1200 turns** (a damaged pit repaired only after them, inferred): verify in a run of the original.
- **The pot branch of 34** (the whole meal from an empty pot): not verified in the real game.
- **Beating out a fire, two fix rows**: with no fire object (0x75ACDA) openblack goes to DecideWhatToDo instead of
  returning 0 and keeping its state (`src/ECS/Systems/Implementations/VillagerFire.cpp:661`); and it turns the villager
  to the fire at once, not 0x100 a turn (0x75AC7F) (`src/ECS/Systems/Implementations/VillagerFire.cpp:656`;
  `LookAtPos` exists in `ECS/Villager/VillagerCore.h:180`).
