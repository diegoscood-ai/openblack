# Objects

## Summary

The world's loose objects: pots and piles of food and wood, taking from them in batches, the storage pit and what it
holds, the static objects and rocks, the fields, the football, and the chimney smoke. Their sounds are on
[World sounds](../audio/world-sounds.md#sounds); the trees on [Trees and forests](trees.md); the buildings' side of the stores
(abodes, building sites, workshops) on [Buildings](buildings.md).

openblack ports the pots and piles (with the gentle release), the multi pick-up, the store, the static objects, the
fields and the chimney smoke; the football is barely started. Everything is ✅ faithful except what is marked
**🔍 inferred** or **⚠️ approximate**.

## The original

The map loading is in [The land script reader](../scripts/map-script-reader.md).

### Pots and piles

- `Pot::Create` (0x66CF10): potType 0 = simple Pot, 1 = PileFood, 2 = PileWood. The hand pots (HandWood 11,
  HandFood 12) are also piles.
- **Giving a pot** (any pot or pile in the hand, the hand's HandWood / HandFood included):
  `Pot::ValidToApplyThisToObject` 0x66DD50 is 1 when the target `IsResourceStore` of the pot's type, or is a Pot
  (RTDynamicCast) of the same type. `Pot::ApplyThisToObject` 0x66DDD0: a store → `DeleteObjectAndTakeResource` → 3
  (0 if it took nothing); otherwise `Pot::AddResourceToPos(own MapCoords +0x14, IS, type, GetResource, IsPoisoned, 0)`,
  `GoolooGooloo`, `ToBeDeleted(0)`, 3. So a hand pot pressed over a pit, its rim or one of its piles of that type joins
  the pit's single total.
- **Simple pot**: scale `min(5, amount/scaleEvery + 0.25)` (`Pot::GetScaleFromAmount` 0x66D4A0). `scaleEvery` is not
  used for anything else.
- **Pile**: it does not scale; it sinks. `PileResource::SetSize` (0x66E900): target `(GetProportionRaised − 1)·height`,
  animated over 1 s with the Zoomer; it is drawn at `GetAltitude(pos) + desplazamiento` and is not visible if it is fully buried.
- `GetProportionRaised` (0x66F1B0 wood / 0x66EB60 food): x = amount/maxInPot in [0.1];
  p = x > 0 ? 0.05 + 0.95x : 0; food: 1 − (1 − p)². A single copy, `ecs::object::GetProportionRaised`
  ([Object size](../engine/object-size-and-map-cells.md#object-size)); the 2D radius of a food pile is
  `GetProportionRaised × Object::Get2DRadius` (0x66F180), so when empty it measures 0.
- When created, every pile starts buried (`−height`) and rises over 1 s (`CallVirtualFunctionsForCreation` 0x66E300).
- Creation scales: **MagicFood 0.3**, **MagicWood 0.7** (constructors 0x5FA9F0 / 0x600E20); the rest 1.
- `PileFood::Draw` (0x51BF80): the store's food pile (info 2) and the magic food (info 10) offset the
  texture in V by `0.25·(1 − clamp(offset/height + 1))` (LH3DObject vfunc 0xE8, 🔍 inferred as a UV
  offset). The grain seems still and the pile "shrinks".
- `FoodPile` (info 8, MSH_B_WORSHIPGRAIN) is the pile of the worship sites; its mesh is offset.
- Pot flags at +0x74: bit 0 poisoned (`Pot::IsPoisoned` 0x55D4E0 / `SetPoisoned` 0x55D510), bit 3 from a building site
  (`IsAPotFromABuildingSite` vt +0x830 = Pot 0x55D590), bit 4 speed-up (`IsSpeedUp` vt +0x4A8 = Pot 0x55D4F0).
  `PotStructure::SetSpeedUp` 0x55D530 only keeps the flag. `PileFood::SetSpeedUp` 0x66E220 also shows it: switching it
  on starts the PILEFOOD_SPEEDUP spot visual (46, scale 1, forever, on the pile); switching it off closes it.
- Resource virtuals: `GetResourceType` vt +0x690 = DeadTree 0x5110C0 / Tree 0x74B820 WOOD, PileFood 0x66EC50 FOOD,
  PileWood 0x66EC60 WOOD, Pot 0x55D4C0 its +0x68, Object 0x402750 −1; `RemoveResource` vt +0xA0 = DeadTree 0x511370,
  PotStructure 0x66EE10, Pot 0x66D3F0; `GetCarriedTreeType` vt +0x820 = DeadTree 0x511A20, Tree 0x55D900, Object
  0x402AF0 (0); `IsDeadTree` vt +0x47C = DeadTree 0x510990.
- **Adding** (`JustAddResource` vt 0x8C): `PileResource::JustAddResource` 0x66D330 plays the pile sound with the amount
  asked for (not for the hand pots, infos 11 and 12). Then `Pot::JustAddResource` 0x66D2B0 caps at maxAmountInPot
  **only when nextPotForResource < 19** (so not for magic or loose piles), sets poisoned = poisoned || IsPoisoned, and
  calls SetSize (vt 0x85C). The amount (+0x70) is a full 32-bit dword: there is no 16-bit limit. ✅ openblack matches:
  `Pot::amount` is 32 bits, and `pot_resource::AddToPotDirect` adds the whole amount (test
  `BuildingSitesTest.AFinishedWorkshopsWoodHasNoLimit`).
- **Removing** (`Pot::JustRemoveResource` 0x66D410, the type is not tested): n ≥ the amount → all of it, and the emptied
  pot loses its reaction (`RemoveReaction` 0x66D6A0), its poison and its fire (+0x44, deleted, 0x66D431..0x66D43F);
  then SetSize (0x66D451). `PotStructure::JustRemoveResource` 0x66D9B0 deletes an emptied pile that is not part of a
  structure (0x66D9E0). A pile of a storage pit, building site or workshop stays (0x66D9D3; `Workshop::CreatePileWood`
  0x779630; a site's pile until `BuildingSite::ToBeDeleted` 0x43B960 releases it). The plain Pot class has no such
  step and stays (`Pot::RemoveResource` 0x66D3F0).
- `PotStructure::GetResource` 0x66EF00: a pile linked to its building site (`IsPartOfStructure` vt +0x860, 0x66DA00;
  `IsLinkedToThisBuildingSite` vt +0x11C, 0x43D830) reports its own amount (0x66EF12..0x66EF47); a workshop's pile
  reports the workshop's mirror (`Abode::GetResource` 0x404D30, 0x66EF2F). `Object::GetResource` 0x639520: the type
  must equal `GetResourceType` (vt +0x690), then `GetDefaultResource` (vt +0x694; Scaffold 0x6E9D30 = value ×
  WoodValue). `Scaffold::GetResourceType` 0x55E0F0 = WOOD.
- <a id="own-resource"></a>**Each class's own resource** (vt +0x690 / +0x694, the classes whose vt +0x98 is
  `Object::GetResource`; FOOD 0, WOOD 1, NONE −1) ✅:
  - Tree, MagicTree: WOOD (0x74B820), `ftol(GetWoodValue)` (0x74B7A0; `Tree::GetWoodValue` 0x74B7B0).
  - DeadTree, FelledTree: WOOD (0x5110C0), `ftol(scale × (woodValue × multiplier))` (0x511330: the tree row at +0x94,
    the multiplier at +0x9C).
  - Animal and the 35 classes that share its table (cows, sheep, the chess pieces...): FOOD (0x417530),
    `ftol(GetFoodValue(3))` (0x41BC80; `Object::GetFoodValue` 0x4026D0 = `GetFoodType & 3` ? info +0x68 : 0).
  - MobileObject (arrows, balls, crops, poo...): FOOD and `ftol(info +0x68)` only for info +0x104 = 0x11, 0x12, 0x13
    (Champi, MagicMushroom, Toadstool), else NONE and 0 (0x607C20 / 0x607BF0).
  - MobileStatic: WOOD and `ftol(GetWoodValue)` for a fence, else NONE and 0 (0x6096B0 / 0x6096D0, above).
  - Rock, Bonfire and Fragment (one table): WOOD (0x6E70C0), `ftol(scale × woodValue)` (0x6E7090: info +0x6C read
    with `fimul dword`, signed; the life does not count). Oracle: 200 wood at scale 0.7935 → 158, 1.37 → 274, 2.9 → 580,
    0.123 → 24.
  - Field FOOD (0x528010) and FishFarm FOOD (0x52D760), both with Object's default 0 (0x402760). A field answers
    through `Abode::GetResource` 0x404D30 (its own amounts at +0xBC + 4 × type, the type not range-checked); a fish
    farm through `Object::GetResource`, so it answers 0 for every type (its fish go through its own calls).
  - Every other Object (abodes, BigForest, WorshipSite, Villager, Creature, Feature...): NONE (0x402750) and 0, so
    `GetResource` gives 0 even for NONE. What is not an Object answers 0 (`GameThing` 0x4051B0).
- <a id="taking-by-kind"></a>**What each class gives when a resource is taken** (`RemoveResource` vt +0xA0) ✅:
  - Not an Object, and Tree, Animal, Villager, Scaffold and every other Object without its own: 0 (`GameThing`
    0x4051D0).
  - MultiMapFixed (0x52F1C0; the citadel's parts, features, mobile statics, spell icons...): its building site (+0x74)
    takes it, every argument passed on, whatever the type; without one 0. The abodes, the storage pit and the workshop
    have their own (below).
  - WorshipSite 0x77C670: [its building site or its food pot](../magic/worship.md#the-food-pot-and-what-a-site-takes).
  - BuildingSite 0x43C530 / WorkshopBuildingSite 0x43DB60 / CitadelBuildingSite 0x43D3F0: their own takes
    ([building sites](building-sites.md)).
  - DeadTree, FelledTree 0x511370: have = `GetResource(type)`; have ≤ n → `ToBeDeleted(0)`, have; else
    `SetScale((have − n) / (woodValue × multiplier))`, n. Asked for any type but WOOD it has 0 ≤ n, so it is deleted and
    gives 0.
  - Rock, Bonfire, Fragment 0x6E70D0: the same test; else s = (have − n) / woodValue (`fidiv`), the altitude above the
    ground (Pos +0x1C) becomes altitude × s / GetScale, then `SetScale(s)`; n (0x6E7105..0x6E7150). Asked for any
    type but WOOD it is deleted and gives 0. Oracle: 37 from 158 at scale 0.7935, altitude 0.75 → scale 0.605
    (0x3F1AE148), altitude 0.57183367 (0x3F1263B1); 273 from 274 at 1.37, altitude 1.5 → 0.005, 0.0054744524;
    asked for all of it or more → deleted. The life does not change.
  - BigForest 0x4390D0: WOOD only ([trees](trees.md)); FishFarm 0x52CF20: FOOD only ([water](water.md)).
  - With what an Object answers when asked ([its own resource](#own-resource)), a big forest answers 0 though it
    gives wood, and a fish farm 0 though it gives food.
- Add/remove redirects: `PotStructure::AddResource` 0x66ED70 / `RemoveResource` 0x66EE10 send everything to the
  building site when the pile is linked to it (0x66ED7E..0x66EDB9 / 0x66EE1E..0x66EE4F), or to the structure (for
  example `Workshop::AddResource` 0x779E00 / `RemoveResource` 0x779EC0) when it is part of one but not linked; without a
  structure they use `JustRemoveResource` (0x66EEAB). `Pot::AddResource` is 0x66D290.
- <a id="worship-food-pot"></a>A worship site's food pot is part of the site (`Pot::Create` with the site as its
  structure, `CreateFoodPot` 0x77AD90; `RemovePotFromStructure` clears the site's +0xB4). Taken from, it follows the
  structure path of `PotStructure::RemoveResource` ✅: over = `MultiMapFixed::CalulateAmountOverMaximum` 0x4015D0 = 0,
  so the pot's `JustRemoveResource` (0x66D9B0) gives what it can of all that is asked, whatever the type, and stays
  when emptied (0x66D9D3); the site's `DoResourceRemoving` is `MultiMapFixed`'s 0x422080 (returns 0, does nothing);
  then, if it gave less than asked, the site's `RemoveResource` (0x77C670) for the rest (0x66EED9..0x66EEF5): its
  building site's wood (or any), or its food pot again. Read, it answers through `WorshipSite::GetResource` (0x66EF2F),
  the same number as its own amount.
- Deletion: `PileFood::ToBeDeleted` 0x66E100 closes its speed-up visual first (wood piles: `PotStructure` 0x66D960,
  nothing); both end in `Pot::ToBeDeleted` 0x66D110 → `RemoveReaction` 0x66D6A0.
- `IsResourceStore` (vt +0x680): `StoragePit` 0x55CD20 = 1 for any type; `PotStructure` 0x66DA30 asks its structure
  (and the type must be the pile's own or ANY −2); `Pot` 0x66F560 = 0; `MultiMapFixed` 0x52F1F0 (any building) = WOOD
  with a building site (+0x74); `Workshop` 0x77A650 = MultiMapFixed's, or WOOD, or ANY; `WorshipSite` 0x77DEC0 =
  MultiMapFixed's, or FOOD, or ANY; `Scaffold` 0x6EAEA0 = WOOD while it belongs to a building site (+0x68,
  `IsAvailableToBeBuiltWith` 0x6E8B10 false), its take 0x6EAEC0 = `DoDeleteObjectAndTakeResource` only with that site
  and its `AddResource` 0x6EAEF0 = the site's (every argument passed on), else 0; `Object` 0x4192D0, `Rock` 0x439710 and `Tree` 0x55D8F0 = 0. What each class is
  worth when given: an animal FOOD (0x417530), `ftol(GetFoodValue(3))` 0x41BC80 = `(GetFoodType & 3) ? foodValue :
  0` (0x4026D0); a fence (`MobileStatic::IsFence` 0x609110: meshes 0x38, 0x51, 0x52) WOOD (0x6096B0),
  `ftol(Object::GetWoodValue)` 0x6096D0 = `woodValue × life × scale³` 0x6395C0; any other MobileStatic −1. info.dat
  gives the Short and Tall Celtic Fence woodValue 25; no MobileStatic row of info.dat uses the American fence's mesh
  (0x38), so no fence of that mesh is ever made from a row. A pile's structure (+0x78, `Pot::Create`'s fourth argument):
  a building site's pile (`CreatePileWood` 0x43D7E3, and the citadel site's six, `CreatePilesOfWood` 0x43D303) is the
  site's `GetBuilding` 0x43BC70 when it is made; a worship site's food pot is the worship site (`CreateFoodPot`
  0x77ADDE). What such a pile is given goes through `PotStructure::AddResource` 0x66ED70: to the structure's site when
  that site claims the pile (vt +0x11C), else to the structure's own `AddResource`.
- **Putting down** (`Pot::AddResourceToPos`): the search radius is multiplied by
  `GetRadiusMultiplierForApplyingPotToPos`: Pot 0x66F520 = 2, Object 0x63AAD0 and WorshipSite 0x77E480 = 1.2. The cells
  are walked fixed list first, then the mobile list (0x66F2CA); pots (type 21) are at the tail of the fixed list.
  Every new pile is in the cells at once (`CallVirtualFunctionsForCreation` 0x607150 → `InsertMapObject` at 0x6071F9).
- Script type: `Pot` 0x66F530 = 16 for every pile (PileFood, PileWood, MagicFood, MagicWood, PotStructure).
- `ADD_RESOURCE` (`GScript::AddResource` 0x6FAD10): POP the thing, the amount (ftol 0x6FAD49), the RESOURCE_TYPE
  (raw). No thing → "No thing for resource", not an Object → "Not object for resource", both push 0; else
  `Object::AddResource` (vt +0x9C)(type, amount, IS 0, poisoned 0, pos 0, 0) and PUSH (float)(uint64) what it took.
  A villager's (`Villager::AddResource` 0x7564D0) carries what it is given and returns 0
  ([Carrying resources](../beings/villager-jobs.md#carrying-resources-and-the-storehouse)).
  openblack's "not an Object" is a town, flock, dance, script marker, timer, mist, influence ring, weather thing,
  puzzle game or spell (`script_type::IsObject`); a camera or a particle container has no component to tell it by and
  reaches `object_resources::AddResource`, which gives 0 as well, so only the log line differs.
- `GET_RESOURCE` (`GScript::GetResource` 0x6FAC50): POP the thing (`GetScriptGameThing` 0x70D220), then the
  RESOURCE_TYPE (raw). No thing → "No thing for resource" (0x6FACDF), not an `Object` (RTDynamicCast from
  GameThingWithPos, 0x6FAC95) → "Not object for resource" (0x6FACD8), both push 0; else PUSH (float)(uint64)
  `Object::GetResource` (vt +0x98)(type) (`fild qword` 0x6FACBE, high word 0).
- `REMOVE_RESOURCE` (`GScript::RemoveResource` 0x6FAE00): POP the thing, the quantity (ftol 0x6FAE39), the
  RESOURCE_TYPE (raw); the same two errors (0x6FAEB5, 0x6FAEAE), both push 0; else PUSH (float)(uint64)
  `Object::RemoveResource` (vt +0xA0)(type, quantity, IS 0, poison out 0) (0x6FAE73..0x6FAE7B).
- The three resource natives share the two checks. openblack: `ResourceObject` in `CHLApi.cpp` (a script marker,
  a dance or a thing `script_type::IsObject` refuses is not an object, as above), then `object_resources::GetResource`,
  `AddResource` and `RemoveResource`. Before this the GET and REMOVE natives were stubs that popped nothing,
  leaving their arguments on the script stack.

- Pot::JustRemoveResource 0x66D410: n >= +0x70 (0x66D41B) -> n = +0x70, +0x70 = 0, RemoveReaction 0x66D6A0, poison off (0x66D42D) and the fire (+0x44) deleted (0x66D431..0x66D43F); else +0x70 -= n
- SetSize (vt +0x85C, 0x66D451)
- PotStructure::JustRemoveResource 0x66D9B0: still something -> SetSize again (0x66D9EA, the same result here). Empty: a pile of a structure (+0x78, 0x66D9D3) stays; any other pot, pile, MagicFood or MagicWood is deleted (ToBeDeleted vt +0xC, 0x66D9E0). The Pot class (Pot::RemoveResource 0x66D3F0 -> 0x66D410) has no such step: it stays. (the deletion: ecs::ToBeDeleted, out of the physics, the map cells, then the entity)
- A building site's pile is part of its building (Pot::Create's structure, SetMultiMapFixed vt +0x868; V6): it stays too, until BuildingSite::ToBeDeleted 0x43B960 releases it A workshop's pile is part of the workshop (Workshop::CreatePileWood 0x779630): it stays too
- PileFood::ToBeDeleted 0x66E100 closes its +0xB8 speed-up visual first, then the common deletion (on the dead list when the deferral is on). A town's temporary pot that goes stays in Town::temporaryPots until Town::Process step 17 (town_stores::ProcessTemporaryPots) finds it unavailable; GetTemporaryResourceStorePotOrPos and SetStoragePit ask IsAvailable (vt +0x2C) meanwhile
- Object::GetResourceType (vt +0x690) of the objects held here: a pot or a pile its info's type (GPotInfo resourceType), a scaffold WOOD (Scaffold 0x55E0F0, `mov eax, 1`); ResourceType::None for any other class (a tree's is answered by ObjectDelivery's ResourceOf)
- Pot::JustRemoveResource 0x66D410 (the type is not tested) and PotStructure::JustRemoveResource 0x66D9B0: amount >= what it has -> all of it, and the emptied pot loses its reaction (RemoveReaction 0x66D6A0), its poison (+0x74 bit 0) and its fire (+0x44, ToBeDeleted); SetSize. A pile without a structure that empties is deleted (0x66D9E0, ToBeDeleted vt +0xC); a storage pit's or a building site's empty pile stays (0x66D9D3). Returns what was removed
- StoragePit::IsResourceStore 0x55CD20 answers 1 for any type; PotStructure::IsResourceStore 0x66DA30 asks the structure it is part of (and the pile's own type); Pot::IsResourceStore 0x66F560 is 0, then IsPot and the type
- A cell's lists in the order Pot::AddResourceToPos walks them (the inline iterator at 0x66F2CA: the fixed list +4, then the mobile one +0, each from its head; ecs::map_cells), the storage pits and the pots / piles among them. The pots (type 21) are at the tail of the fixed list. In the original every new pile is there at once (CallVirtualFunctionsForCreation 0x607150 calls InsertMapObject vt +0x544 at 0x6071F9); here only the ones made with an InsertMapObject hook (CreateMagicResourcePile: MagicFood / MagicWood, the storm's piles) are, the others (PotArchetype::Create from the villagers or the hand) wait for the next map_cells::Sync (aproximado, their owners add the hook)
- GetRadiusMultiplierForApplyingPotToPos: Pot 0x66F520 = 2, Object 0x63AAD0 / WorshipSite 0x77E480 = 1.2
- PileFood::SetSpeedUp 0x66E220: the flag, and on switching on the PILEFOOD_SPEEDUP spot visual (46, scale 1, for ever, on the pile); off closes it
- JustAddResource (vt +0x8C, PileResource 0x66D330 -> Pot 0x66D2B0): the pile sound with the n still asked for, the cap, the poison, SetSize (0x73304B..0x733061)
- While n != 0: each existing pile's JustRemoveResource (vt +0x90 = PotStructure 0x66D9B0, a pit's empty pile stays; the out pointer is not passed, 0x7332C6)
- +0x74 bit 4 (Pot::IsSpeedUp 0x55D4F0 / PileFood::SetSpeedUp 0x66E220)
- CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9): InsertMapObject (vt +0x544, 0x6071F9): a pot or pile (type 21, counted as fixed) at the tail of its cell's fixed list at once, so Pot::AddResourceToPos sees it
- PotStructure::SetSize 0x66D480 -> Pot::GetScaleFromAmount 0x66D4A0 (the only reader of scaleEvery).
- PileWood::Draw 0x51BC40 / PileFood::Draw 0x51BF80: t += frame time; past the 1 s duration the offset is the target.

#### A pot let go gently

- `Pot::InitialisePhysicsFromHand` 0x66DF00 (slot vt +0x780 of every pot and pile class) makes no physics object: it
  calls `StartMultiPutdown(is, GetResourceType(), IsPoisoned(), 0.75)` 0x68FAE0 (the put-down particles,
  `CreatePutdownPSys` 0x671420, for the local interface's status only), then
  `AddResourceToPos(&pos, is, type, GetResource(type), IsPoisoned(), 0)` 0x66F270, then `GoolooGooloo` and
  `ToBeDeleted(0)`, and returns 0. `IsPoisoned` 0x55D4E0 is +0x74 & 1 in all 8 vtables. The slot's other callers are
  `Object::ThrowObjectFromHand` 0x6386F1 and `LH3DCreature::DoDiscardAction` 0x48C987 / 0x48C9C5: a creature dropping a
  pot takes the same path.
- The `StartMultiPutdown` call is at 0x66DF54, and 0x68FAE0 is its only caller; it makes the WOOD / FOOD (POISONED)
  PUTDOWN particle (`PSysGlobal::StartMultiPutdown`; 🔍 inferred: the 0.75 is a lifetime, compared with the particle's
  age in fn_00671340). `AddResourceToPos` is called at 0x66DF8D with the whole amount: the 9-cell merge, the rest a new
  MagicFood / MagicWood pile, the PILE(WOOD|FOOD)[SMALL]_0x sounds (SMALL below 200). A hand pot thrown (not let go
  gently) merges on landing through `Pot::EndPhysics`.
- **The resource type is virtual.** Only the plain Pot (vtable 0x8E6604) reads it from its row: `GetResourceType`
  0x55D4C0 = +0x68, written by the constructor 0x66CEA3 from info +0x118; `GetResource` 0x66D3D0 → `JustGetResource`
  0x66D390 = +0x70. PileFood 0x93305C, MagicFood 0x92B724 and PuzzleGrain 0x8ED30C answer FOOD (0x66EC50); PileWood
  0x8E5D8C and MagicWood 0x92F7A4 answer WOOD (0x66EC60); their `GetResource` is PotStructure's 0x66EF00 (the
  structure's pile when they belong to one). PotStructure 0x8F8DCC is made only by its two constructors (0x55D60A from
  `GameOSFile::LoadInstance` 0x55A3B4, and 0x66D93C inside the PileResource constructor), PileResource 0x9338D4 is
  abstract. `Pot::Create` 0x66CF10 picks the class by potType (info +0x114): 0 → `Pot` 0x66CE60; 1 → PileFood, or
  MagicFood for row 10; 2 → `PileWood` (vtable set at 0x66CFE7), or MagicWood for row 9.
- **The 19 pot rows** of info.dat all have resType 0 (food) or 1 (wood): rows 0 and 15, 17, 18 are food pots, rows 1,
  13, 14, 16 wood pots; rows 2, 8, 10, 12 food piles; rows 3..7, 9, 11 wood piles. The pile classes' constants agree
  with their rows.
- **PuzzleGrain** (constructor fn_006DBFE0: the PileFood one, then +0x68 = −1 at 0x6DC018) is made by fn_006DC140 with
  row 12 (HandFood, 0xD4D590) and 10 units (0x6DC18B..0x6DC1AF). Its type answer is FOOD, so the −1 is never read; it
  can be held (`ValidForLockedSelectProcess` 0x561940 = 0, `InterfaceSetInMagicHand` 0x5619F0 = 1), and let go gently
  it puts down 10 food. When a slot's grain is no longer available, the next `PuzzleGame::Process` (0x6D8A87 →
  fn_006DC740, over the 20 slots +0x494[i], 0x6DC74B..0x6DC770 → fn_006DC140) deletes it (`GoolooGooloo` 0x6DC158) and
  makes a new PuzzleGrain (a hand-food pile of 10, GPotInfo row 12) at the slot's place (fn_006DC030), so a taken or
  used-up grain grows back next turn; an empty slot is skipped. The later loop (0x6DC7E0..) handles a grain that is off
  its slot and not held.
- **What the hand can hold.** `GInterface::PlaceObjectInMagicHand` 0x5DA6F0 tests `ValidForPlaceInHand` (vt +0x6FC,
  0x5DA73B) and `IsCannotBePickedUp` (vt +0x180, 0x5DA74E, 0x401A10 in every pot vtable). A Pot answers `Mobile`
  0x425B00 = 1 (`InterfaceSetInMagicHand` `Pot` 0x66D710 removes its reaction): a loose food or wood pot is lifted whole.
  A pile answers `PileResource` 0x66ECE0: only when its row is the hand's (`GetHandPotInfoType` 0x66EC30: 12 for food,
  11 for wood) and it holds something (0x66ECE6..0x66ED2C). Every other pile is a locked select
  (`PileResource::ValidForLockedSelectProcess` 0x66E4F0, the multi pick-up).
- **Where the resource goes** (`AddResourceToPos` 0x66F270, static): nothing when out of bounds (0x66F27A); a spiral of
  9 cells (0x66F2BE), fixed list then mobile list; an object takes it when it `IsResourceStore(type)` (vt +0x680,
  0x66F304) or is a pot (`IsPot` 0x55D500) of the same type (0x66F312..0x66F32C), within its radius × its multiplier
  (0x66F375). The leftover, on land only (`IsWater` 0x66F42D), goes to fn_005FA8B0 (0x66F464): food → a new `MagicFood`
  (0x5FA9F0, 0xC0 bytes), wood → a new `MagicWood` (0x600E20, 0xB8 bytes), any other type → nothing
  (0x5FA8B4..0x5FA8BD). Then the new pile gets the player, the poison and the speed-up, and `GGuidance::ResourceDropSFX` plays (wood 2, food 1) for the local status only. So a gently released fish with nothing to merge into becomes a MagicFood pile.
- **A thrown tree and the stores.** `Tree::ReactToPhysicsImpact` 0x74B6B0 asks the hitter `IsResourceStore(WOOD)`
  (0x74B6CE) and then, with the impact matrix copied into `StoredMatrix`, `DeleteObjectAndTakeResource` (0x74B6F9); otherwise it calls `Object::ReactToPhysicsImpact`
  0x6377C0, an empty `ret 8`. `DeadTree::ReactToPhysicsImpact` 0x510CA0 does the same, with no fallback. The answers:
  `MultiMapFixed` 0x52F1F0 = wood with a building site (+0x74); `Workshop` 0x77A650 = that, or WOOD or ANY;
  `WorshipSite` 0x77DEC0 = that, or FOOD or ANY; `StoragePit` 0x55CD20 = 1 for any type; `PotStructure` 0x66DA30 =
  its structure's answer (part of one through 0x66DA00) for its own type or ANY; `Pot` 0x66F560 = 0; `Scaffold`
  0x6EAEA0 = wood with its site (+0x68). The takes re-test: `MultiMapFixed::DeleteObjectAndTakeResource` 0x52F460
  returns 0 without a site, Scaffold 0x6EAEC0 needs +0x68, `Pot::DeleteObjectAndTakeResource` 0x66DD30 and Workshop 0x779F20 always take.

### Taking in batches (multi pick-up)

- Press on a pile: 25 immediately into a hand pot. Every 0.1 s turn: `(int)(8 + 62·t²)`, t = n/60,
  up to **20000** per batch (`maxAmountCanBePickedUp`). Verified: 413 at 3 s, 1746 at 6 s.
- The hand stays fixed in x,z over the pile, y = ground + height of the pile.
- Releasing the button leaves the pot in the hand; another press + release puts it down or throws it.
- Putting down: it joins a nearby pile or store of the same resource (approximate radius 15 m) or creates MagicWood/MagicFood.
- Particles (`SF_MultiPickUpWood/Food`, `ER_MultiPickup::ModifyAtomCollection` 0x6A77C0): 8 per second, each one goes
  in a straight line over 1 s from the ground under the hand to the current position of the hand; they are destroyed when picking up stops.
  Wood = mesh MSH_I_OFFERING_WOOD at 0.35 with `AppearanceRuleTumble`; food = grains from S_SpriteSheet1 (32 frames,
  20 fps); fish = S_Spangle_A cells 48..63 at 40 fps (`SF_MultiPickUpFoodFish`; openblack:
  `HandSystem::UpdatePickupParticles`, see Pending for its colour).

### Store (StoragePit)

- A single total for 5 piles of wood + 1 of food (`StoragePit::AddResource` 0x732F60 /
  `RemoveResource` 0x7332A0).
- Adding wood: piles 1→5, each up to 5000 except the last one (no cap). Taking out: **5→1**, regardless of which
  pile it is taken from. A pile at 0 stays buried and is not visible.
- `PotStructure::GetResource` (0x66EF00): a store pile reports the store's total.
- Layout: food pile at +0xC4, wood piles 1..5 at +0xC8..+0xD8.
- `StoragePit::AddResource` 0x732F60 (type, n, IS, poisoned, pos, k): when the pit has a building site that is not
  built yet (+0x74), WOOD and ANY (−2) go to the site (0x732F67..0x732F99). Only FOOD and WOOD fill piles (0x732FA2 /
  0x733083). A missing pile is made first (`Pot::Create` at `GetResourcePos`, 0x732FF8 / 0x7330D2), and only an
  available pile is filled (0x73302C..0x733037).
- Pulse: if the store held none of that resource before and something went in, Town +0x5E8 = 1 and +0x5EC = 0
  (0x73316D..0x73318D). Then `DoResourceAdding` (0x7331B1) with what the piles took; it returns the amount stored
  (0x7331B9). The mirror (`GetResource`, 0x73315E) is read after the piles but before `DoResourceAdding`, so it still
  holds the old total.
- `StoragePit::RemoveResource` 0x7332A0: only FOOD and WOOD (0x7332E9; any other type 0, with no
  `DoResourceRemoving`). Wood piles 5 → 1 (0x7332EF..0x73331B), each through `PotStructure::JustRemoveResource` (an
  empty pit pile stays). If something came off, `DoResourceRemoving` gets the removed total (0x73331D..0x733337).
- `PotStructure::RemoveResource` 0x66EE10 on a pit pile: over = `CalulateAmountOverMaximum` (vt +0x8EC,
  0x66EE71..0x66EE9A). The touched pile gives n − min(over, n) when over > 0, otherwise n, and an empty pile is still
  asked. If r ≠ 0, the pit's `DoResourceRemoving(r)` runs (0x66EE9C..0x66EED3). If r < n, the pit's `RemoveResource(n −
  r)` takes the rest (0x66EED9..0x66EEF5).
- `StoragePit::CalulateAmountOverMaximum` 0x733260 (signed): WOOD = total − 5 × GPotInfo[Wood Pile 1].maxAmountInPot
  (0xD4CB48); any other type = total − GPotInfo[Storage Pit Food Pile].maxAmountInPot (0xD4CA04).
- Poison: `StoragePit::IsPoisoned` 0x7336B0 = `IsPoisonedResource(FOOD) || (WOOD)` 0x733550 (an available pile whose
  `Pot::IsPoisoned` is set); `StoragePit::SetPoisoned` 0x7335D0 ORs each pile's flag, so it is never cleared. An
  abode's `IsPoisoned` is 0 (GameThingWithPos 0x402400).
- **Abode/store resource economy** (`Abode::DoResourceAdding` 0x404DF0, vt +0x8E4): with no interface or no town, only
  the add itself (`JustAddResource` 0x404D40: +0xBC[type] += n, no cap). Otherwise, the town desire
  `CallDesireFunction(type != FOOD)` 0x745D80 is read before and after. The difference is multiplied by
  `Town::GetGameTurnResourceLastRemovedModifier` 0x740030 (0x404E70). It then calls `GAlignment::Update` 0x414520 for
  the interface's player (0x404E87), and `GBelief::AddToBelief` 0x437EB0 of the town with delta × (not the owner ?
  multiplierForNonOwnerAddingResource : 1) × multiplierForAddingResourceToTown (0x404E8C..0x404EC3). Last comes
  `DoCreatureMimicAfterAddingResource` (vt +0x68C: StoragePit 0x733810, MultiMapFixed 0x52F210). The town side is in
  [Resources held by objects](buildings.md#resources-held-by-objects).
- **The storage pit's creature deed** (`StoragePit::DoCreatureMimicAfterAddingResource` 0x733810, disassembled
  from the exe; found by the creature mimicry RE). `MultiMapFixed`'s test runs first (0x52F210: WOOD on a pit that is
  still a building site gives deed 7 and stops). Then the status's +0x128, the player who owned what the hand picked
  up (set at every pick-up, see the next point), is compared with the status's own player (vt +0x1C), pointer to
  pointer: a NULL +0x128 is never the player's own. The pit's owner is not read. FOOD (0x73382C): the same player gives 2 (PUT_FOOD_IN_STORAGE_PIT, 0x733864); another one, or
  none, draws `GameRand(2)` at 0x733844 (synced stream) and gives 43 + (draw != 0) (STEAL_FOOD_FROM_FARM /
  _FROM_STORAGE_PIT, pushed at 0x733861, reported at 0x7338B6). The draw comes **before** any creature check
  (`ConsiderMakingCreatureMimicPlayer` 0x4EA900), so it happens even when the player has no creature. Any other type
  draws nothing: 4 (PUT_WOOD_IN_STORAGE_PIT) for the same player, 45 (STEAL_WOOD_FROM_STORAGE_PIT, 0x73389A)
  otherwise. Workshops (0x77A680) and pots (`Object` 0x63AAE0, vt +0x68C of Pot, PotStructure and PileResource) never
  draw.
- It is asked up to twice for one deposit: at the end of `Abode::DoResourceAdding` (0x404ED2, with a status and a
  town, whatever was added; `StoragePit::AddResource` reaches it at 0x7331B1), then by the deposit itself:
  `Object::DoDeleteObjectAndTakeResource` (0x63A9C1, when something was taken) and `Pot::AddResourceToPos` (0x66F3BB,
  for every store or pot that was offered, whatever it took). the deed is only returned
  until the creature's mimicry is ported.
- **Putting down into a pit passes the status** (disassembled from the exe). `Pot::AddResourceToPos` 0x66F270 calls every
  offered object's `AddResource` (vt +0x9C, 0x66F3A9) with (type, what is left, the status, poisoned, the position,
  0): a storage pit gets the status in `StoragePit::AddResource`, so with a town its `DoResourceAdding` side runs
  (desire, alignment, belief and the first deed), then the put-down's own deed (0x66F3BB). A pile of a store
  forwards every argument to its store: `PotStructure::AddResource` 0x66ED70 calls the structure's `AddResource`
  (vt +0x860 gives the structure; the call at 0x66EDE6), or the structure's building site's when the pile is linked
  to it (0x66EDB9), and its own `JustAddResource` only without a structure (0x66EE04); the pile itself tells the
  creature nothing (`Object` 0x63AAE0). The poison goes with it: `Pot::AddResourceToPos` passes its poisoned argument
  to every store or pot offered (0x66F3A9) and `PotStructure::AddResource` forwards it to the structure (0x66EDE6).
  `object_resources::AddResource` on a pit's pile passes the dropper on too
  (an object given to a pile: its pit's town side and deed).
- **Who owned what the hand picked up** (`GInterfaceStatus` +0x128, a `GPlayer*`; disassembled from the exe, every
  write to +0x128 of a status in the exe listed). `GInterfaceStatus::SetToZero` 0x5DBA00 clears it to NULL (0x5DBAE1;
  called by the status's constructor 0x5DB8AD and `Init` 0x5DD1A9, and by `GInterface::SetToZero` from
  `GInterface::Init` 0x5CE676, which `GPlayer::Init` 0x64929B calls). Then:
  - every object put in the hand: `GInterface::PlaceObjectInMagicHand` 0x5DA6F0 calls the status's add-to-hand
    step `fn_005DC330` (0x5DA7C1) when the object's `InterfaceSetInMagicHand` returned 1; on success it sets
    LastPickedUpObject (+0x120) and +0x128 = `MapCoords::CalculateMostInfluentialPlayer` 0x603830 of the object's
    position (0x5DC3E9..0x5DC3F7). `Influence::CalculateMostInfluentialPlayer` 0x5CD630 starts from the neutral
    player and keeps a player only with an influence above the best so far (start 0), so it is never NULL.
  - a locked select's start overwrites it after its hand pot went in the hand: `PileResource` 0x66E7EC = the pile's
    `GetPlayer` (vt +0x1C, read after the first scoop); `Field` 0x5299B6 = the most influential player at the
    field's position; `FishFarm` 0x52D853 = `FishFarm::GetPlayer` 0x52C850: the town's player (`Container` +0x2C,
    0x462A50), **NULL without a town**.
  - A pile's `GetPlayer`: `PotStructure::GetPlayer` 0x66F230 asks the structure it is part of (+0x78, a storage
    pit's piles: `Abode::GetPlayer` 0x405F70, the town's player, else `GameThing::GetPlayer` 0x570130, the neutral
    player), else its own +0x7C, else the neutral player; +0x7C is 0 in the constructor (0x66D94D) and set to
    `GetPlayer` by `PotStructure::CallVirtualFunctionsForCreation` (0x66DBA4), so a loose plain pile is the neutral
    player's. `MagicFood::GetPlayer` 0x5FA980 (+0xBC) and `MagicWood::GetPlayer` 0x600DA0 (+0xB4) answer their own
    player (the neutral one when made with none, 0x600E64..0x600E8A); the workshops' and building sites' piles are
    Magic Wood, so they are the neutral player's unless a player made them. `Pot::Create` 0x66CF10 makes the class by
    the info's row (stride 0x144): row 9 Magic Wood (0x66CF63), row 10 Magic Food (0x66D00B), any other wood row
    `PileWood`, food row `PileFood`. `Workshop::CreatePileWood` 0x779630 asks for row 9 (InfoList+0xB64, 0x779674), so
    a workshop's pile answers with its own player even while linked to the workshop (Magic Wood's slot +0x1C
    overrides `PotStructure`'s). **A worship site's food pot** (`WorshipSite::CreateFoodPot` 0x77AD90: row 2,
    InfoList+0x288, linked to the site, 0x77ADD5) is a `PileFood`, so it asks the site: `CitadelPart::GetPlayer`
    0x469750 (the WorshipSite's slot +0x1C) = the citadel's (+0x80) player (+0x2C). A building site's own `GetPlayer`
    is `GameThing::GetPlayer` (the neutral player), but its piles are Magic Wood (above).
  - Every deposit by that status carries it: the hand's (objects given or thrown, pots put down) and the food and
    wood miracles, whose IS is the leader interface of the spell's player ([Food, wood, water and heal](../magic/food-wood-water-heal.md)).
  - openblack: the locked-select start records `object_resources::PlayerOfPile` (read before the first scoop: a
    storage pit's pile the pit's town's owner, a worship site's food pot the site's player, any other pile its own;
    the Game Detail Overlay's "Hand Took From" line shows it), the field's most influential player or
    `fish_farms::PlayerOf` (none without a town). `Pot::owner` is the neutral player unless a miracle or a hand pile
    set it. Every `Dropper` the hand builds (`HandSystem::InterfaceStatus`) and the miracles' carry it.
- `Abode::DoResourceRemoving` 0x404F60 (vt +0x8E8): the desire before (0x404FA0); with an interface and a town,
  `Town::SetGameTurnResourceLastRemoved` 0x7400D0, the desire after, and `GAlignment::Update(this, type, −n, before −
  after)` for the **town's owner** (0x404FEF..0x405005; −n is the amount asked for).
- `Abode::AddResource` 0x404D90 / `RemoveResource` 0x404F10 send WOOD or ANY to the abode's building site (+0x74) when
  it has one (0x404D9C..0x404DE9 / 0x404F1A..0x404F4E); `Abode::JustRemoveResource` 0x404D60 = min(n, what it has).
- **Giving an object to a store** (`Object::DeleteObjectAndTakeResource`, vt +0x684; callers first ask
  `IsResourceStore` vt +0x680): `Tree::ApplyThisToMapCoord` 0x74C008 (put down on a store;
  `MapCoords::IsWithinBuilingSite` 0x605250) and `Tree::ReactToPhysicsImpact` 0x74B6F9 (it hit one;
  `GetGameObjectWhoHitMe` 0x644F00). `Object` 0x63A930 = 0. StoragePit 0x733750 and WorshipSite 0x77E7B0 wrap
  `Object::DoDeleteObjectAndTakeResource` 0x63A940.
- `StoragePit::DeleteObjectAndTakeResource` 0x733750: it reads the player of IS first (0x73375A..0x733767). If the
  object is in physics and its PhysicsObject's interface is the local one (`GGame::MyInterfaceStatus` 0x555880), it
  calls `HelpProfile::Trigger(6 SUPPLY)` 0x5C46E0 (0x73376D..0x7337A6; the same code in WorshipSite at
  0x77E7B6..0x77E7EC). Then comes `DoDeleteObjectAndTakeResource` (0x63A940), then `Reaction::CreateReaction(this, 22
  REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, player, 1)` 0x6E3D70 (0x7337B4..0x7337BA; its result unused). It returns
  1 (0x7337C5).
- `DoDeleteObjectAndTakeResource` 0x63A940: `this->AddResource(object's type, object's amount, IS, object's poison,
  &object->pos, 0)` (0x63A965..0x63A99C). If something was taken and the interface is the local one, `ResourceDropSFX`
  (a store's guidance resource type is 0, so it is silent). The physics list is 0xD47814, 0x1DC bytes per entry. Then
  `GoolooGooloo` 0x5E6540 (a 500 ms ghost of the object's 3D model) and `ToBeDeleted(0)` (0x63AAA3 / 0x63AAB1).

- Pit life cycle: `StoragePit::StopBeingFunctional` 0x733960 → `Pot::SetupReaction` 0x66D660 on each pile that holds
  something; `RestartBeingFunctional` 0x7339D0 → `RemoveReaction` on each available pile. `StoragePit::ToBeDeleted`
  0x732C30 (before `Abode::ToBeDeleted`, 0x732CB8) runs `DeleteDependancys`, then for each available pile
  `SetMultiMapFixed(0)` and its `ToBeDeleted(now)`, and clears the slot (0x732C3E..0x732CB3). An unavailable pile keeps
  its slot. `StoragePit::MakeFunctional` 0x732F30 → `Town::SetStoragePit` 0x73EA60.

- Object::DeleteObjectAndTakeResource (vt +0x684 of the receiver, (Object*, GInterfaceStatus*)): a structure takes the object given to it and the object goes. Its callers ask IsResourceStore (vt +0x680) first: Tree::ApplyThisToMapCoord 0x74C008 (put down on a store: MapCoords::IsWithinBuilingSite 0x605250; 1 -> returns 3) and Tree::ReactToPhysicsImpact 0x74B6F9 (it hit one: GetGameObjectWhoHitMe 0x644F00, `is` the PhysicsObject's +0x24). Object 0x63A930 returns 0 (nobody takes it); StoragePit 0x733750 and WorshipSite 0x77E7B0 (here and worship::site::DeleteObjectAndTakeResource) wrap the shared Object::DoDeleteObjectAndTakeResource 0x63A940 (the resource moved and the object deleted: ecs::object_delivery, ECS/ObjectDelivery.h). The other overrides (MultiMapFixed 0x52F460, Pot 0x66DD30, Scaffold 0x6EAEC0, Workshop 0x779F20) are (pending).
- 0x73376D..0x7337A6 (StoragePit) = 0x77E7B6..0x77E7EC (WorshipSite), the same code in both: the object is in the physics (Object +0x24 & 0x40) with a PhysicsObject (SearchForPhysicsObject 0x646950) whose interface (+0x24) is the local one (GGame::MyInterfaceStatus 0x555880) -> HelpProfile::Trigger(6 SUPPLY) 0x5C46E0 (g_game +0x250060). The receiver's own `is` is not what is tested. TODO(Intro HEAD): the trigger is only logged until help_profile::Trigger lands (TakeResource.cpp).
- StoragePit::DeleteObjectAndTakeResource 0x733750 (object, is): the player of `is` (is ? is->GetPlayer() : NULL, 0x73375A..0x733767, before anything else), TriggerSupplyHelpIfThrownByMe(object), DoDeleteObjectAndTakeResource( object, is) 0x7337AF, then Reaction::CreateReaction(this, 0x16 REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, that player, 1) 0x7337BA (its result is not used). Returns 1 (0x7337C5).
- TownDesire::CallDesireFunction 0x745D80 (type != FOOD) of the abode's town: d = 1 (Wood) or 0 (Food), 0x404E16 / 0x404F93
- Abode::RemoveResource 0x404F10 (0x404F1A..0x404F4E): +0x74 (the building site) with WOOD or -2 (ANY) -> the site's RemoveResource (vt +0xA0, status passed on)
- Abode::AddResource 0x404D90 (0x404D9C..0x404DE9): +0x74 with WOOD or -2 (ANY) -> the site's AddResource(type, n, IS, poisoned, NULL, 0) (vt +0x9C; the position NULL: a CitadelBuildingSite would add nothing)
- Object::RemoveResource (vt +0xA0): StoragePit 0x7332A0 (StoragePitStore); Abode 0x404F10 -> DoResourceRemoving 0x404F60 (JustRemoveResource 0x404D60: min(amount, what it has)); a pile of a storage pit PotStructure::RemoveResource 0x66EE10 (the touched pile gives n - min(over, n), over = the pit's amount above its maximum, then the pit's own order, pile 5 -> 1, the rest); a building site's pile the site's RemoveResource (0x66EE1E); any other pot JustRemoveFromPot (Pot 0x66D3F0, PotStructure 0x66D9B0). An abode with a building site (+0x74) and WOOD or -2: the site's RemoveResource (0x404F1A). (pending) the out poison flag (0x66EE60: *out = IsPoisoned) is not returned: the callers ask IsPoisoned. Returns what was removed
- Object::AddResource (vt +0x9C): the storage pit first (it has an Abode too): StoragePit::AddResource 0x732F60 (StoragePitStore; with a building site, WOOD or -2 go to the site, 0x732F67); Abode 0x404D90 (the same +0x74 redirect) -> DoResourceAdding 0x404DF0 -> JustAddResource 0x404D40 (+0xBC[type] += amount, no cap); a building site's pile the site's AddResource (0x66EDB9); a pot or a pile PotStructure::AddResource 0x66ED70 / Pot 0x66D290 (pot_resource::PotStructureAddResource). Returns what was taken
- The object's IsPoisoned (vt +0x4A4) as GetResourceFrom 0x7533EC reads it: StoragePit 0x7336B0 = IsPoisonedResource(FOOD) || (WOOD) 0x733550 (an available pile of +0xC4 / +0xC8 whose Pot::IsPoisoned is set); an abode 0 (GameThingWithPos 0x402400); a pot its own flag
- 0x733083: only FOOD and WOOD fill piles; ⚠️ approximate for any other type the original still runs the pulse test and DoResourceAdding with 0
- (*) 0x73315E: GetResource (vt +0x98, the mirror) is read after the piles but before DoResourceAdding, so it is still the total from before this call
- 0x732FBA / 0x733094: while n != 0. A missing pile is created first (Pot::Create 0x66CF10 at GetResourcePos, 0x732FF8 / 0x7330D2): ⚠️ approximate openblack creates all six with the pit (AbodeArchetype) and never deletes them, so there is none to make. 0x73302C..0x733037: only an available pile (IsAvailable vt +0x2C)
- 0x7331B1: DoResourceAdding(type, added, IS, poisoned, pos, k) (vt +0x8E4). The original fills the piles first and DoResourceAdding's desire "before" (0x404E22) still reads the old mirror;; its n is what the piles took (justAdd's value). Returns `added` (0x7331B9)
- 0x73316D..0x73318D: none of it before, something added, a town -> the pulse +0x5E8 = 1, +0x5EC = 0 (before DoResourceAdding in the original; only Town::Process reads it, so after it here)
- 0x7332E9: only FOOD and WOOD; any other type 0, with no DoResourceRemoving
- 0x73331D..0x733337: removed != 0 -> DoResourceRemoving(type, removed, IS, out) (vt +0x8E8). Its desire "before" reads the mirror, still the old total: Something comes off exactly when the store holds some and n != 0
- StoragePit::CalulateAmountOverMaximum 0x733260: WOOD mirror - 5 x GPotInfo[3] max ([0xD4CB48]); any other type mirror[type] - GPotInfo[2] max ([0xD4CA04])
- StoragePit::AddResource 0x732F60 (type, n, IS, poisoned, pos, k). With a building site (+0x74, not built yet) WOOD and -2 go to the site's AddResource (0x732F67..0x732F99, V6). Only FOOD and WOOD (0x732FA2 / 0x733083): food to the food pile (+0xC4), wood to Wood Pile 1..5 (+0xC8..+0xD8) while n != 0, each pile's JustAddResource (vt +0x8C: the pile sound with the n still asked for, the cap at maxAmountInPot when next < 19, the poison, SetSize). The pulse: the store held none of it before and something went in -> Town +0x5E8 = 1, +0x5EC = 0 (0x73316D..0x73318D). Then DoResourceAdding 0x404DF0 (0x7331B1; the mirror follows the piles here). Returns the amount stored (0x7331B9). `dropper` is the GInterfaceStatus (the hand), default none. ⚠️ approximate CREATE_ABODE's initial fill (AbodeArchetype) goes through here too: one pile sound per pile reached and the pulse; how the original fills a new pit is (pending)
- StoragePit::CalulateAmountOverMaximum 0x733260: WOOD total - 5 x GPotInfo[Wood Pile 1].maxAmountInPot ([0xD4CB48]), any other type total - GPotInfo[Storage Pit Food Pile].maxAmountInPot ([0xD4CA04]); signed

#### Giving an object to a store


### Static objects (MobileStatic, rocks)

- Position `GetAltitude(pos) + altitud del script`, rotation `SetYXZMatrixOnly(y, x, z)`, uniform scale
  (`Game3DObject::SetPosition` 0x63B680, `MobileStatic::GetWorldMatrix` 0x608DE0).
- The original does **not** settle them on the ground: `GetAltitudeFondation` is only used for buildings.
- **Fences are wood.** A fence (`MobileStatic::IsFence` 0x609110) is WOOD worth `ftol(woodValue × life × scale³)`
  (0x6096B0 / 0x6096D0 / 0x6395C0; info.dat Short and Tall Celtic Fence 25). In the hand:
  `MobileStatic::ValidToApplyThisToObject` 0x608BB0 = fence and `target->IsResourceStore(WOOD)`; `ApplyThisToObject`
  0x608C30: fence, store, not INDESTRUCTIBLE (+0x24 bit 0x4000) → `DeleteObjectAndTakeResource` → 3. Thrown:
  `MobileStatic::ReactToPhysicsImpact` 0x608FC0: fence, not INDESTRUCTIBLE, the hit (`GetGameObjectWhoHitMe`, no
  availability test) `IsResourceStore(WOOD)` → the hit's `DeleteObjectAndTakeResource(this, po +0x24)`. Only the hand's
  path also has a branch for the static of info +0x128 == 0x31 (GateTotemApe, the carved gate stones) given to an
  AnimatedStatic of type 1 (the gate stone plinth): `AddGateStone` 0x422D50, its result ignored, a ghost, the stone
  deleted, 3; `ReactToPhysicsImpact` has none, so a thrown stone is not laid. The scripts read and set the plinth and the gate with two
  natives: SET_OPEN_CLOSE 307 (0x708E30: the object popped first, then the word, kept as it is; an error for a null or
  non-AnimatedStatic object) and OBJECT_INFO_BITS 320 (0x6FE000: `CountGateStones` 0x422DF0, ape 1, tiger 2, cow 4,
  pushed as a float; on a null or non-AnimatedStatic object an error and no push at all).
- `GObjectInfo::IsOkToCreateAtPos` 0x638C40: `MapCoords::CollideWithFixed` 0x604FE0 on the live map cells, x and z
  only; bit 8 clear → yes. Off the map the value is 0xFFFFFFFF, which has the bit set.
- Script types: CREATE_MOBILE_STATIC (0x716DC1 → fn_00608840 / fn_00608770) makes a GBaseOnly for info 6 (type 0), a
  Bonfire for info 8 (type 8), a Rock for mobileType 2 (`Rock` 0x6E79E0 = 33), otherwise a MobileStatic (0x609330 =
  8). A Fragment is 8 (0x76F7C0 jumps to MobileStatic's).

<a id="angles-of-a-thing"></a>
### The angles of a thing

✅ [V: a scan of every vtable's slot +0x514 and the oracle on fake objects,
[script-commands.md](../scripts/script-commands.md#properties-and-flags)]

- Every object keeps its angle about y apart from its matrix: the float at GameThing +0x4C (`SetYJustAngle` vt +0x520,
  0x63A800; `GetYAngle` vt +0x508, 0x402500, the same in every class). The mobile statics keep their angles about x and
  z at +0x80 / +0x84 (`GetXAngle` 0x439610, `GetZAngle` 0x439620) and the mobile objects at +0x5C / +0x60 (0x425B10,
  0x425B20); every other class's `GetXAngle` / `GetZAngle` (0x4024F0, 0x402510) give 0.
- `SetXYZAngles(x, y, z)` (vt +0x514) is the one setter of the three:
  - `Object::SetXYZAngles` 0x638D00 (102 classes: the living, the buildings, the features, the trees, the totems, the
    football `Football`, the fixed things, the citadel's parts...) keeps no x or z: out of its map cells, +0x4C = y,
    its matrix built again from `GetScale` and `GetYAngle` in SET_POSITION's four branches (no turn when the angle is
    0), back into its cells;
  - `MobileStatic::SetXYZAngles` 0x608CE0 (Bonfire, DeadTree, FelledTree, Fragment, the four LandscapeVortex classes,
    MagicTeleport, MobileStatic, Rock) and `MobileObject::SetXYZAngles` 0x607460 (Arrow, Ball, Creed, FieldCrop,
    HanoiBlock, MagicFood, MagicWood, MobileObject, OneOffSpellSeed, PBall, PileFood, PileResource, PileWood, Poo, Pot,
    PotStructure, PuzzleGrain, PuzzleMobileObject, Reward, Scaffold, Whale) keep x and z, set +0x4C = y and build the
    matrix with `Game3DObject::SetPosition(pos, x, y, z, scale)` 0x63B680: `SetYXZMatrixOnly(y, x, z)` 0x7FAC10 (the
    turn about z, then x, then y), each cell times the scale. The singing stone's base is a `GBaseOnly`, an Object.
- Creation keeps the angles a thing is made with: CREATE_MOBILE_STATIC sets the three angles on what it made
  ([Making a static object](#making-a-static-object)), and every creation's angle about y is stored at +0x4C (🔍
  inferred from the constructors' `SetYJustAngle`; not read class by class). So a thing reads back the angles it was
  made with, not the angles of its matrix.
- **The physics hands a moved body's angles back each turn**: `PhysicsObject::GameTurnUpdate` takes the body matrix's
  angles with `LHMatrix::GetYXZ` 0x7FAB30 (at 0x645C3F and 0x645D51) and gives them to the object's `SetXYZAngles`
  (0x645D0A, 0x645D6A); `PhysicsObject::RemoveObject` does the same once more (0x646A57). So a thing that leans takes
  the body's three angles and its matrix is made again from them; any other thing keeps only the angle about y and
  stands upright.
- SET_POSITION and SET_PROPERTY YPOS build the matrix from `GetYAngle` alone and leave +0x80 / +0x84: a moved thing
  stands upright but keeps its leans, which come back when any of its angles is set again
  ([SET_POSITION](../scripts/script-commands.md#set_position-024)).

### Fields (Field)

- The 6 GFieldTypeInfo are identical: ageGrowth 80, ageRecolt 1200 (ripe), timesToSow 30, foodValueTakenWithHand 25,
  totalFoodInField 350, maxFarmerInFarm 10, sun 0.5/1.5, rain 1.5/1.5, ratioBeforeRipe 0.2. The `IsUnripe` symbol
  (0x5298D0) returns **ripe** (growth ≥ 1200). The field rows of info.dat are at file offset 0x49898 + 0x144·i.
- Layout: +0xD4 / +0xD8 the farmers list (8-byte nodes, new ones inserted at the head) and its count, +0x118 the town
  (`Field::GetTown` 0x528960), +0x11C the turn offset, +0x120 the GFieldTypeInfo (set in the ctor 0x527E23). Info
  offsets: +0x120 ageGrowth, +0x124 ageRecolt, +0x128 timesToSow, +0x130 totalFoodInField, +0x134 maxFarmerInFarm,
  +0x138/+0x13C sun growing/ripening, +0x140/+0x144 rain growing/ripening, +0x14C ratioBeforeRipe, +0x150
  effectOfWaterSpell.
- Order: `GlobalGameLists::Process` 0x591370 calls `Field::Process` (vt +0x5FC) for every field every turn (list g_game
  +0x205C04, newest first). The town's abode pass fn_747600 (0x74762A) also calls it every processAbodeEvery turns, so
  on those turns it runs twice. A town's fields (+0x780) are newest first (head insertion in the ctor
  0x527E64..0x527E75).
- `Process` 0x529020 every 10 turns (+ an offset 0..9), with the 30 crops sown and not ripe:
  d = 2·(0.5·alignment + 1)·(0.5 growing | 1.5 ripening or with rain); growth += d, food += d·350/1200.
- `Field::Process` 0x529020 first calls `Abode::Process` 0x404440 (0x529026: the site's `MultiMapFixed::Process`
  0x52F700 and the empty-abode counters +0xB0 / +0xB9). Then: (turn + offset) % 10 (0x52902B..0x529047); **no growth
  while on fire** (`Object::IsOnFire` 0x637CC0, 0x52904F); the field must be fully sown ((float)crops < timesToSow →
  nothing, 0x52905D) and not past ripe (0x529084).
- Growth inputs: `MapCoords::GetAlignment` 0x6057B0 at the field (0x5290A1) and `GClimate::IsRaining` 0x7714B0 at its
  point (0x5290BF); it is "growing" while growth < ageGrowth, read before the step (0x5290CA). Then growth += d
  (0x52911C) and food += d × totalFoodInField / ageRecolt (0x529133..0x52914F).
- The town pulse after growth (0x52912C / 0x529159, `IsUnripe` called twice on the same value, first true and then
  false) can never fire.
- `Field::ApplyWaterSpell` 0x528F78..0x528FEA: a field past ripe gets nothing; if crops ≤ timesToSow, crops =
  ftol(timesToSow + 1).
- `RemoveFood(n)` 0x5295A0: 0 without food or unsown; cost = n ripe, (int)(1.2·n) unripe; if it is not enough:
  unripe it is emptied and gives (int)(0.2·n); ripe it gives what is left and is deleted entirely (it has to be sown again).
  In detail: unripe cost = ftol(amount × ratioBeforeRipe + ftol(amount)) (0x5295F0..0x529624); the cost is compared as
  unsigned with food (0x529626). When it is enough, food −= cost and it returns ftol(amount) (0x5296CF..0x5296E8). When
  it is not enough: `SetTemperature(0)` 0x639A60 (0x529643), the town pulse +0x5E8 = 1 / +0x5EC = 0
  (0x52964E..0x52966A); unripe → food = 0 and it returns ftol(amount × ratioBeforeRipe); ripe → it returns ftol(food),
  and food, crops and growth are set to 0 (0x5296A8..0x5296CC).
- `Field::GetPercentFull` 0x529500 = crops / timesToSow. `GetFieldActivity` 0x529350: < 1 full → 1 (sow); growth ≥
  ageGrowth → 2 (harvest); otherwise 0.
- `Field::GetDesireToBeFarmed` 0x5293A0: a fire effect at +0x44 (the pointer, not `IsOnFire`) or not functional (vt
  +0xD4) → 0. Otherwise a = 1 − min(1, farmers / maxFarmerInFarm); activity 2 → growth ≥ ageRecolt ? a : 0; activity 1
  → (1 − min(full, 1))·a³; otherwise 0.
- `Field::PlantCrop` 0x5291A0: if crops < timesToSow, crops++ and true (the position is not read). `GetPlantCropPos`
  0x529210 is misnamed: it answers "still sowing" (crops < timesToSow).
- Farm points: `RandomFarmPoint` fn_00528970 = the position + (5 − GameFloatRand(10)) metres on x, then on z
  ("Field.cpp" lines 0x164 / 0x165, 5.0 at 0x8AB6E4); `RipeFarmPoint` fn_00529240 draws nothing unless growth ≥
  ageRecolt. `Field::IsTouching` 0x529290 (vt +0x6B4): fx − 5 ≤ px < fx + 5, and the same on z. `GetArrivePos`
  0x529330 = the position. `GetFoodValue` 0x529700 = growth < ageRecolt ? 0 : food.
- Farmers: `Field::AddFarmer` 0x5283E0 adds at the head, with no maximum and no duplicates (maxFarmerInFarm is only used
  in the desire). `RemoveFarmer` fn_00528340 unlinks every node of the villager and **always** clears the villager's
  target (+0x118), even when it was not in the list.
- `Field::DeleteDependancys` 0x528100 (from `Field::ToBeDeleted` 0x5280F0 → `Abode::ToBeDeleted` 0x402C6F, vt +0x910):
  for each farmer, head first and with the next one taken before the call, `SetTopState(163 DECIDE_WHAT_TO_DO)` (its
  exit `ExitFarming` 0x75A2A0 unlinks it) and target = 0 (0x52814E). Then it leaves the town's list +0x780
  (`Town::SetTownArea`, 0x5281C3), the map (0x5281CA..0x5281DC) and the global list (0x5281F1..0x52826C). The fence
  (+0xC8, fn_00528CA0) has a builder 0x528B80 with no caller.
- Hand: action button over the field (locked selection; needs growth > 0 and food > 1). It starts with
  (int)min(25, food), **half if it is ripe**, removed from the field; per turn (int)min(8 + 62t², food), t =
  min(turns/60, 1), ≤ 20000 − what is in the hand, half if ripe; the hand receives n (oddity). Grain particles
  (`SF_MultiPickUpFood`), sound G_PICKUPFOOD. It cannot be given back.
- Drawing `Draw` 0x528570: one mesh (MSH_T_WHEAT); only with growth ≥ 20 and food ≥ 25; it sinks v = food/350 − 1
  over 1 s (y += 2·v·scale·height) and fades out below v = −0.8; colour from olive to light green while growing, to
  white while ripening; the ripe ones sway with the trees' wind.
- The original's hand leaves the last unit of food of a ripe field forever: its halved and truncated amounts reach 0,
  and `RemoveFood` only deletes the field if it is asked for more than it has.
- **Colour and sway of the mesh** (`Field::Draw` 0x528570):
  `BlendColor` 0x5284C0 (k = 0 gives a, 255 gives b, `(a(255−k) + b·k)/255` truncated): growing, olive (121,145,25) →
  light green (170,212,67) with k = 255·(1 − food/350); ripening, olive → white with k = 255·(growth − 80)/1120;
  ripe, white. It multiplies byte by byte the object's terrain light, `(c·tinte) >> 8` (fn_0080BF10), before the
  haze and the N·L: in openblack it goes in the x of the instance's fifth column, negative
  (`−1 − r·65536 − g·256 − b`, `argb_colour::PackInstanceTint`, [The object colour fields in the instance](../rendering/model-lighting.md#the-object-colour-fields-in-the-instance)). The
  ripe ones sway: column 1 (up axis) is sheared in world z with `1.75 × scale × T0[i]`, `T0 = −0.03·cos(fase)`
  of 16 phases (`Tree::PreDraw` 0x74A7C0: speed Random(1, 2) every 2 s, phase += ms·speed·0.0010606061 (the float 7/6600, bits 0x3A8B0409; the ms loaded as an integer, each product and the sum rounded to a float by
  the game's 24-bit FPU precision) with ms the
  frame's whole game ms g_game +0x205D48; the 0.03 is the double [0x99A168] = (double)0.03f, times the extended cosine,
  rounded once to a float; openblack's float step and double `std::cos` give the exe's bits on 24101 of 24101 oracle
  cases; the wind angle is always 0, and the weather's wind is not read), `i` fixed per field (in the original, bits of its address); only the drawn matrix.
  `ecs::FieldDrawColour`, `VegetationInterface::GetFieldMatrix` / `GetLean`. The trees use the same table ([Drawing](trees.md#drawing)).
- Sway timer: the 16 speeds are at 0xDA3A4C, past the raw end of .data, so they are 0 until the first draw (**no sway
  for the first 2 s**); the 2 s counter [0xDA579C] += the frame ms (0x74A7D6) and is reset above 2000 (0x74A879). The
  field colours are startup constants (0x528440 / 0x528470 / 0x5284A0).
- **No fade when sunk:** `Field::Draw` works out a fade for a crop sunk past 80% (`(shown + 0.8) × 2 + 1` into the
  colour's alpha), but then calls `LH3DObject::CombineColorFromPos` (0x52886A / 0x52888A), which writes over the whole
  colour, alpha included; so the crop is always drawn opaque and the fade never shows. before 2026-10-15 it drew the fade.

- 0xDA3A4C, past .data's raw end (0xC3D000), so 0 until the first draw: no sway for the first 2 s
- 0x529026: Abode::Process 0x404440 first: MultiMapFixed::Process 0x52F700 (the building site's Process) and the empty-abode counters +0xB0 / +0xB9 (a field is always empty of villagers; its ReduceLife 0x52A0A0 at +0xB0 = 1 is GetLife, nothing changes). +0xB9 has a reader: StopBeingFunctional's GameStats test (>= 200)
- Entt's on_destroy<Field> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): Field::ToBeDeleted 0x5280F0 -> Abode::ToBeDeleted 0x402C6F -> vt +0x910 DeleteDependancys 0x528100. The component is still there during the signal
- 0x5290CA..0x5290E4: fld growth; fcomp ageGrowth (+0x120); test ah, 1 (below: growing)
- 0x5291A0..: (float)crops < timesToSow -> ++crops, 1; the position argument is not read
- GetPlantCropPos 0x529210: (float)crops < timesToSow
- 0x5295F0..0x529624: unripe (growth < ageRecolt, test ah, 1): cost = ftol(amount x ratioBeforeRipe (+0x14C) + k) (fmul, then fiadd dword k: each rounded to float; ⚠️ approximate a k above 2^24 is rounded to float before the add); ripe: k
- 0x528960: +0x118;
- Field::DeleteDependancys 0x528158..0x5281BD: a field marked for deletion is out of the town's +0x780
- The ctor's head insertion (0x527E64..0x527E75): newest first
- 0x528158..0x5281C3 (out of the town's +0x780, Town::SetTownArea), 0x5281F1..0x52826C (out of g_game +0x205C04): nothing to do in openblack (TownFields / the components are the lists; that SetTownArea, the field still in +0x754, is followed by RemoveStructureFromTown's in abodes::ToBeDeleted, which is the one that shows). 0x5281CA..0x5281DC RemoveMapObject: ecs::ToBeDeleted's generic part. 0x5281E2..0x5281EC the fence (+0xC8, fn_00528CA0): not ported (no fences, the builder 0x528B80 has no caller)
- Field::Process 0x529020, once per game turn (GlobalGameLists::Process 0x591379, the first loop): every 10th turn (plus the field's +0x11C) a fully sown field that is not on fire grows until it is ripe, by 2 (0.5 alignment + 1) x the sun / rain multiplier of its GFieldTypeInfo, and its food with it
- Field::Process 0x529020 for one field: Abode::Process 0x404440 first (0x529026: the site's Process, +0xB0 / +0xB9), then the growth above. Called by ProcessFieldsTurn (every turn) and by the town's abode pass fn_747600 (0x74762A, every processAbodeEvery turns: a town's field is a structure of it, Abode ctor 0x4013BB), so on those turns it runs twice, as in the original
- Field::DeleteDependancys 0x52813D / FishFarm::DeleteDependancys 0x52C6C9: the worker's SetTopState(163 DECIDE_WHAT_TO_DO) (vt +0x8E8, villager::SetTopState); its exit function ExitFarming 0x75A2A0 / ExitFishing 0x75B880 unlinks it (RemoveFarmer / RemoveFisherman). Nothing for a villager no longer valid
- The field's GFieldTypeInfo (+0x120, the ctor 0x527E23): InfoConstants::fieldType[type]
- The growth of one Field::Process step (0x5290A1..0x529110): a = 2 (alignment x 0.5 + 1); x the rain multipliers (+0x140 growing / +0x144 ripening) when it rains, else the sun ones (+0x138 / +0x13C); growing while growth < ageGrowth (+0x120), read before the step
- Field::GetPercentFull 0x529500: (float)crops / timesToSow
- Field::GetFieldActivity 0x529350 (its argument is never read, `ret 4`): GetPercentFull < 1 -> 1 (to sow); growth >= ageGrowth -> 2 (to harvest); else 0
- Field::GetDesireToBeFarmed 0x5293A0: a fire effect (+0x44) or not functional (vt +0xD4) -> 0; a = 1 - min(1, farmers / maxFarmerInFarm); activity 2: growth >= ageRecolt ? a : 0; activity 1: (1 - min(GetPercentFull, 1)) a^3; else 0
- Field::PlantCrop 0x5291A0 (its position is not read): (float)crops < timesToSow -> crops++ and true; else false
- Field::GetPlantCropPos 0x529210 (misnamed: "still sowing"): (float)crops < timesToSow
- Field::GetArrivePos 0x529330 (vt +0x104): the position +0x14
- Field::GetFoodValue 0x529700: growth < ageRecolt ? 0 : food
- Field::AddFarmer 0x5283E0: already in the list or null -> nothing; else at the head (+0xD8++). No maximum (maxFarmerInFarm is only in GetDesireToBeFarmed); TargetThing is not written
- Field::GetTown 0x528960: +0x118 (Field::town, a Town::id), entt::null without one
- Town +0x780: the town's fields, newest first (head insertion in the ctor 0x527E64..0x527E75): the Field entities with that town, by creation index from high to low. 🔍 inferred the same order: fields never change town
- "in g_game +0x205C04" (ExitFarming 0x75A2A0's test): a valid entity with a Field
- Field::DeleteDependancys 0x528100 (Abode::ToBeDeleted 0x402C6F, vt +0x910; run by an on_destroy<Field> listener that AddFarmer connects, so ecs::ToBeDeleted / Registry::Destroy reach it): every farmer, head first, the next taken before the call: ReleaseWorker (SetTopState(163) 0x528146) and TargetThing = null (0x52814E, TODO(the villagers' area)). The town list +0x780, the global list +0x205C04 and SetTownArea 0x5281C3 need nothing in openblack (the components are the lists; town_placement recomputes the rectangle when it is read); RemoveMapObject is ecs::ToBeDeleted's
- +0x120: the GFieldTypeInfo (the ctor 0x527E23), an index of InfoConstants::fieldType
- +0x120: the GFieldTypeInfo the ctor keeps (0x527E23)
- 0x527E64..0x527E82: at the head of the town's +0x780, then fn_0073AC90 with the Field vtable (Get2DRadius 0x528E80, 5.0): the field's own radius, which Abode::Abode's SetTownArea had as 0

### The football (Ball)

The football is a town's plaything: a `MobileObject` subclass (vtable 0x8C4F74, 0xB8 bytes) of its own info,
`GBallInfo::Infos` 0xC58498 (one row). `GGame::FootballEnabled` (0xD0196C) is 0 in single player
([Buildings](buildings.md)), and no shipped script creates a ball, so a game never has one unless a script asks.

- **CHL CREATE, type 28 BALL.** The case of the create switch `fn_006F11A0` (jump table 0x6F1A70, entry 27 = 0x6F178F),
  inside what the symbol file labels `GScript::CallNearInState` 0x6F1010 with a size of 0xB10, which is wrong: the real
  `CallNearInState` ends at its `ret` 0x6F119C, `fn_006F11A0` runs from 0x6F11A0 to its jump table's end 0x6F1B14 (padding
  to `GScript::CreateThing` 0x6F1B20), and its callers are `CreateThing` 0x6F1BBC, `PopulateContainer` 0x6F20CE and
  `CreateWithAngleAndScale` 0x6F2EF0. The ball case does not read the sub-type (5000 is accepted, 0x6F11BD):
  - 0x6F17A4 `Ball::Create(coords, &GBallInfo[0], 0, 6.0, 4.0)` 0x435B90 (the last three arguments are not read);
  - 0x6F17BD `Town::GetNearestTownToPos(coords, tribe -1, abode 0x7FFF, FLT_MAX [0x9421A0])` 0x73B170: any tribe, any
    abode, however far; the towns walked player by player (`GGame::GetNextPlayerAndNeutral` 0x550980, the neutral one
    last), each player's town list from its head, a town taken when strictly nearer (`GUtils::GetDistanceInMetres`
    0x74CD50 to the town's MapCoords);
  - no town: "Ball Created but unable to add it to a town." (0xC0CB6C, `ScriptErrorMessage`), the ball stays;
  - else 0x6F17EB `Town::AddPlaythingIfNoneThere(ball)` 0x73B7A0 on that town, after a dynamic cast of the ball to
    Object. The ball is returned to the script in every case (edi), then the common step 0x6F1591.
- **Ball::Create** 0x435B90: `Base::new(0xB8)`, the constructor 0x4359B0, `SetScale(1.0)` (vt +0x124),
  `CallVirtualFunctionsForCreation` (vt +0x658 = `MobileObject`'s 0x607150, which ends in `InsertMapObject`, vt +0x544:
  the ball enters its map cell at once), then its 3D object's vt +0x98 with 0 (not read).
- **The constructor** 0x4359B0: `MobileObject(coords, info, no owner, 0.0, 1.0)` 0x606E40; +0x68 links it at the head
  of the game's ball list (g_game +0x205BAC, count +0x205BB0); +0x6C..+0x74 and +0x78..+0x80 both the position (x,
  `GetAltitude` + relative y, z); `Reaction::CreateReaction(ball, 11 REACT_TO_BALL, GetPlayer(), 0)` 0x6E3D70;
  `SetToZero` 0x435C00 (+0x90 state, +0x94, +0x98, +0xA0 town, +0xB0, +0xB4 = 0). `Ball::GetPlayer` 0x435F60: +0x98's
  player when set (vt +0x1C), else the player at g_game +0x205A5B, the neutral one.
- **The ball's town.** `Town::AddPlaythingIfNoneThere` 0x73B7A0: `FindPlaything(the object's info type)` 0x73B720
  walks the town's list (Town +0x97C, count +0x980) for one that is available (`IsAvailable` vt +0x2C, flags +0xA bit
  0 clear) and of the same info type; found, it returns 0 and the town keeps it; else a node goes at the head and it
  returns 1. The script's path never sets the ball's own town (+0xA0): only `fn_00436000` (Ball::SetTown) does, from
  `Villager::ArrivesAtPickupBallReaction` 0x765320 (0x76539D): when the ball is free (`IsBallFree` 0x4360E0, +0x90 = 0)
  and the villager has a town (vt +0x48), +0xA0 = that town and +0x90 = 1 once the town took it, and the villager goes
  home (`GoHome` 0x760270).
- **Its removal.** The destructor `fn_00435B10` unlinks it from the ball list, removes the reactions it initiated
  (0x6E4750), with +0x90 = 2 calls `fn_00531C20` on +0x94 (the football game's side), then `fn_00436050`: with a town
  (+0xA0), `Town::RemovePlaything` 0x73B800 takes it off that town's list and +0xA0 / +0x90 go to 0. A ball given by the
  script has no +0xA0, so its town's list keeps the freed pointer.
- **Each turn.** `GGame::ProcessTurn` calls `Ball::ProcessBalls` 0x435F30 at 0x54E665, between the fire list and the
  reactions; it walks the ball list calling vt +0x5FC, `Ball::Process` 0x435C30, which only returns 1. The kicks
  (0x435C40, 0x435DF0: the physics object's throw, +0x84..+0x8C its destination) belong to the football game.

### Moved to other pages

- Trees: rules, fire and sacrifice → [Trees and forests](trees.md) ([tug](../hand/grab-hold-and-throw.md#tug),
  [pick-up rules](../hand/grab-hold-and-throw.md#pick-up-rules-and-bigforest), [fire](fire.md#fire-trees), [sacrifice](trees.md#sacrifice)).
- Creation from CHL → [Creation from CHL](../scripts/map-script-reader.md#creation-from-chl).
- Map mist (CREATE_MIST) → [Map mist (CREATE_MIST)](haze-and-mist.md#map-mist-create_mist).
- Animals and herds → [Animals and flocks (CREATE_FLOCK, CREATE_NEW_ANIMAL)](../scripts/map-script-reader.md#animals-and-flocks-create_flock-create_new_animal).
- Map simulation data → [Map simulation data (data only, nothing is drawn)](../scripts/map-script-reader.md#map-simulation-data-data-only-nothing-is-drawn).
- Fish farms → [Fish farms](../scripts/map-script-reader.md#fish-farms).
- The missionaries' boat (`PetitNavire`) → [The missionaries' boat (PetitNavire)](sea-life-and-scenery.md#the-missionaries-boat-petitnavire).
- Construction percentage of a Feature →
  [Build percentage of a Feature](../scripts/map-script-reader.md#build-percentage-of-a-feature).
- Fish puzzle: the script side → [Fish puzzle](sea-life-and-scenery.md#fish-puzzle).
- Map script objects →
  [Map script objects (street lanterns, bonfires, dead trees, gates)](../scripts/map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates).

### Chimney smoke (LH3DSmoke)

**Faithful** (done; deviations at the end). Everything read from the
W1.20 disassembly; the doubtful points (fn_007F8E00, 0x7F9F10, 0x5E4310, fn_005DBC60) were reread when porting it.

- **Creation** (`Abode::CallVirtualFunctionsForCreation` 0x403200): if the mesh has the flag 0x400 (in openblack
  `L3DMeshFlags::HasChimney`, formerly `Unknown11`), `LH3DSmoke::Create` 0x7F8B60 at the chimney = **extra point [1]** of
  the mesh (the [0] is the door) times the object's 3×3 matrix (rotation and scale) + its position (with the altitude of the
  foundations) (`LH3DStaticObject::GetChimneyPos` 0x7F9F10). Colour 0x808080 if it is a workshop, 0xFFFFFF otherwise. Houses,
  crèches, workshops, storehouses and wonders; `MSH_B_AMCN_5` has the point at (0,0,0) (smoke from the base, like the
  original). ARK, ARK_DRY_DOCK and APPLE_BARREL have the flag but are not Abodes.
- **When** (`Abode::Draw` 0x516288): only if the building appeared on screen this frame; off screen the smoke
  freezes. `PresentAtHome` (+0xB6, villagers inside) ≠ 0, or a workshop with the scaffold count (+0xC4) ≠ 0 → state 0;
  otherwise, 0 → 2 (it dies out: each puff finishes its life and is reborn hidden) and, when none is drawn, 3 (dead). No
  day/night, weather or fire condition.
- **Particles** (`fn_007F8E00`, the Z-sorter callback, which simulates while drawing): 10 sprites, initial age i·90
  (in 1/255 s), hidden; angle Random(0, π), random spin direction, spin ±dt·0.765. Frame drift W (the same
  for all 10): the **hand** (at < 15 u from the chimney and speed > 1: `handWind`) or Random(−3, 3) in x and z. On passing
  900 it is reborn at the chimney with τ = age/255; otherwise τ = dt. v' = v + 1.5·τ·W; p += (v + v')·τ/2; rises 2.55 u/s. Cell
  (age/20) & 15 (8 per row of `smoke.raw`), half-width age/450 + 0.5, alpha 79 up to 225 and then linear to 0 at 900
  (integers). Material `g_smoke_mat` (mode 6: SRCALPHA/INVSRCALPHA, Z test without write, no light or fog):
  **at night the smoke stays white**, as in the original. Lifetime 3.53 s.
  - `g_smoke_mat` is made by `fn_0080BBD0` (0x80BC76) and used by `LH3DSmoke::Create` (0x7F8CEC); `smoke.raw` is pure
    white in the cells the smoke uses (0-15), so the colour is the vertex colour alone.
  - The Z-sorter entry is made by `LH3DSmoke::AddDrawing` 0x7F8D30.
  - The workshop countdown +0xC4 is run by `fn_007798A0` / `Workshop::Process` 0x7797F0.
- **Hand wind** (`GLandscape::Draw` 0x5E4310): if the hand's 3D object has a mesh, handPos = its position,
  handSpeed = clamp(|v|·0.1, 0.5, 5), handWind = v/|v|·handSpeed (v = 0 → 0), with v = `GInterfaceStatus::HandVelocity`
  (`fn_005DBC60`: v += 0.6·(Δpos·1000/100 − v), per turn 🔍 inferred). Initial values handPos (−10000, 0, 0),
  handWind (1, 0, 0). It does not read the weather or LH3DAtmos.
- The age step is the original's, truncated every frame (`frame_anim::SmokeAgeStep`, as fn_007F8E00 0x7F8F25..0x7F901E,
  run on the exe with the oracle 2026-10-22; before, the fraction was kept): a puff lives 3.75 s at 60 fps, 6.3 s
  at 144 fps, and the smoke stands still under 4 ms a frame, as the original. A dying smoke therefore goes out
  (state 3) by the same whole steps, and the frames it is not out are the frames its drift draws from the CRT
  stream. Tests `ChimneySmoke.DropsThePartOfAStepLeftOver`, `ChimneySmoke.ADyingSmokeGoesOutByWholeSteps`.
- **Deviations**: "on screen" is the sphere of the building's box against the frustum ⚠️ approximate; the hand "with a mesh" = the hand is not
  at the origin ⚠️ approximate; the random drift is per frame like the original, so its amplitude depends on
  the fps (as in the original). The workshops' scaffold count is missing (no workshops that produce).
- **Hook**: `OPENBLACK_TEST_CHIMNEY=all` (all chimneys smoke).

### Store details

- DoResourceRemoving's n is what came off (0x733329: the removed total), min(n, the store's total)

### Smoky stuff

- 0x823DA7..0x823E18: mode != 0 (ground_marks, RootsPile 0x8252EB): 1.5 x size along the direction ([0x8AB24C])

### Field details

- The push 0x41200000 (10.0) of RandomFarmPoint's two GameFloatRand (0x52898A / 0x5289AC)
- SetTopState(163) (vt +0x8E8, push 0xA3: 0x52813F / 0x52C6C2). a villager already gone (a registry Reset clears the pools one after another) is not called
- 0x5290A6..0x5290BB: fmul 0.5; fadd 1; fadd st0, st0; fstp a
- 0x5290E6..0x5290F7: +0x140 effectRainWhenGrowing / +0x144 effectRainWhenRipening
- 0x5290FF..0x529110: +0x138 effectSunWhenGrowing / +0x13C effectSunWhenRipening
- 0x529501..0x529517: fild (u8 crops); fdiv timesToSow (+0x128)
- 0x529358..0x529365: GetPercentFull < 1 -> 1
- 0x52937A..0x529387: growth >= ageGrowth (+0x120; test ah, 1; jne) -> 2
- 0x5293A6..0x5293AB: a fire effect at +0x44 (the pointer, not IsOnFire)
- 0x5293B1..0x5293BC: IsFunctional (vt +0xD4) != 1
- 0x5293C2..0x529413: fild qword {+0xD8, 0}; fidiv dword maxFarmerInFarm (+0x134, read signed), rounded to float; fcom 1; test ah, 1; jne: below 1 (or unordered) kept, else 1; a = 1 - that (fsub, fstp dword)
- 0x529419..0x529438: p = GetPercentFull below 1, else 1
- 0x52944E..0x529476: growth below ageRecolt (+0x124; test ah, 1; jne) -> 0, else a
- 0x529480..0x529493: fld 1; fsub p; fmul a; fmul a; fmul a, each rounded to float
- 0x5289C0..0x528A1B: ftol((pos x 10 x 2^-16 + r) x 65536 / 10) on each axis; the altitude (+8) copied
- 0x5283E3..0x5283FB: already in the list -> nothing; 0x5283FD: a null villager -> nothing
- 0x528401..0x528420: a new node at the head, +0xD8++ (the deletion listener connected first)
- fn_00529240 RipeFarmPoint: growth < ageRecolt -> false (no draw); else out = RandomFarmPoint (2 draws), true
- Is the villager in the list (AddFarmer's test 0x5283F2..0x5283FB)
- +0xD4 / +0xD8: the farmers (8-byte nodes {next, villager}, head insertion in AddFarmer 0x528401..0x528420), newest first; the count is the size

### Making a static object

- 0x6087E1: nothing
- 0x608869: nothing (lanterns come from CREATE_STREET_LANTERN)

### Making a field

- The constructor (0x527DD0) starts the field empty and the town's farmers sow it. Mod world.crops (no villager jobs yet): it starts sown and ripe, as the fields look when a land starts.
- The ctor: GameRand(10) at 0x527E90 into +0x11C

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `GetSizeEBone 0x4038E0` | the original's size of an extra-bone block of an L3D file; the footprint block keeps its size at +8 | `l3d::L3DFile (extra bones block)` |
| `GetSize*Data 0x403730..0x403A30` | the original's per-block size readers that Abode::GetNewEp sums to reach the entrance points | `l3d::L3DFile (additional data blocks)` |
| `jle 0x40367F` | in Abode::GetNewEp, the branch that reads no entrance point when the count is 0 or less | `l3d::L3DFile (NewEP block)` |
| `LH3DStaticObject::GetChimneyPos 0x7F9F10` | reads the L3D extra point [1], the chimney | `l3d::L3DMeshFlags::HasChimney` |
| `fn_0081FFF0` | the animal ground blobs, which use the first 2 or 4 EBone points | `l3d::L3DFile (EBone block)` |
| `Abode::GetNewEp 0x403590` | reads a building's entrance points from the NewEP block | `l3d::L3DFile (NewEP block)` |
| `Terrain::GetAtmosType 0x7352B0` | the cell's ATMOS_TYPE, (flags >> 2) & 0xF | `lnd::LNDCell` |


### Sources

- Disassembly of `runblack.exe`, `LHaudiodllR` and QMixer; the QMixer volume, distances and axes were checked by
  running the original code under an x86 emulator (Unicorn).

## openblack

Status: **partial**. Progress rows: [stores and piles](../project/progress/resources/stores_and_piles.md) (75%),
[food](../project/progress/resources/food.md) (87%), [wood](../project/progress/resources/wood.md) (89%),
[storehouse](../project/progress/town/storehouse.md) (95%),
[fields and fish farms](../project/progress/building/fields_and_fish_farms.md) (85%),
[football](../project/progress/town/football.md) (25%).

- `src/ECS/PotResource.{h,cpp}` (`pot_resource::AddResourceToPos`), `src/ECS/StoragePitStore.{h,cpp}`,
  `src/ECS/ObjectResources.{h,cpp}`, `src/ECS/TakeResource.h`, `src/ECS/ResourceStores.cpp` (`IsStoreForType`,
  `KindOf`, `StructureOfPile`, `TreeReactToImpact`), `src/ECS/Fields.{h,cpp}`, `src/3D/ChimneySmoke.cpp`, and the archetypes `PotArchetype`, `FieldArchetype` and `MobileStaticArchetype`;
- the hand's side: `HandResources.cpp` (`PutDownHandPot`) and `src/Hand/HandGrabRules.cpp`
  (`hand_grab::PlaceInHandRuleOf`, `PileValidForPlaceInHand`);
- an object's state lives in components or Locator services; the pile proportion, the field growth and the mixer gain
  are pure functions tested with fakes.

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#world).

#### Pots and piles

- openblack: `ecs::held_apply` (the Pot rows); the release still puts a hand pot down where it lands (`PutDownHandPot`, `AddResourceToPos`), as the original's release does.
- openblack (2026-10-16): a building site's scaffold is a wood store while it belongs to the site
  (`resource_stores::StoreKind::Scaffold`, `src/ECS/ResourceStores.cpp`), so a tree thrown into it, a pressed tree, a
  fence or a wood pot given to it is taken as the site's wood, as Scaffold `IsResourceStore` /
  `DeleteObjectAndTakeResource` do. Tests: `ResourceStoreTable.AScaffoldTakesWoodOnlyForItsBuildingSite`,
  `ResourceStoresTest.AScaffoldTakesWoodWhileItBelongsToABuildingSite`.
- openblack: `ecs::resource_stores` (`IsResourceStore`, `DeleteObjectAndTakeResource`, the pure `IsStoreForType`, `PileIsStoreForType`, `AnimalFood`, `FenceWood`), and `object_delivery`'s resource of an animal or a fence.
- openblack (2026-10-16): `resource_stores::StructureOfPile` answers both (the site's root building, `WorshipSiteOfFoodPot`), and `object_resources::AddResource` passes a worship site's food pot on to the worship site.
- `HandSystem::PutDownHandPot` (HandResources.cpp): a hand pot let go on the land goes through `AddResourceToPos` at the pot's position, with its amount and poison; off the map nothing is added, and a new pile gets the food reaction once.
- `OfferTo` (PotResource.cpp, `pot_resource`): offers what is left to one store or same-resource pot around the position; `RadiusMultiplierForApplyingPotToPos` gives the reach (2 for a pot, 1.2 for an object or a worship site).
- `PotArchetype::UpdateSizes`: each frame moves a pile's mesh offset towards its target over 1 s, as the piles' Draw does.
- `PotType::Pot` (ObjectResources.cpp, `JustRemoveFromPot`): an emptied pile is deleted unless it is a plain pot, a store's pile or part of a building site or workshop.
- `ResourceType::Any` (ObjectResources.cpp, `AddResource` / `RemoveResource`): an abode with a building site passes Wood or Any on to the site.
- `CallResourceDesire` (ObjectResources.cpp): the town's desire for wood or food, as an abode reads it.
- [Each class's own resource](#own-resource): `object_resources::GetResource` / `GetResourceType` answer it for
  every kind openblack has (a scaffold, a tree or a dead tree through `TreeWood`, an animal, a mushroom, a rock, the
  bonfire or a fragment through `object_physics::RockWood` and `store_rules::RockWood`, a fence, a field, a fish
  farm), the same numbers `object_delivery::ResourceOf` reads for a tree, an animal or a fence. Tests:
  `TakeResourceTest.RocksHoldWoodByTheirScale`, `LivingAndGrowingThingsAnswerTheirOwnResource`,
  `AFishFarmIsFoodButAnswersNone`, `StoreRules.ARocksWoodIsItsScaleTimesItsWoodValue` (the oracle's numbers).
- [What each class gives](#taking-by-kind): `object_resources::RemoveResource` passes a worship site to
  `worship::site::RemoveResource`, a building site to `building_sites::RemoveResource`, a structure on a building site
  (a citadel's part, `abodes::GetBuildingSite`) to its site, a dead tree to `RemoveWood` (deleted when asked for
  anything but wood), a big forest to `BigForestRemoveWood`, a fish farm to `fish_farms::RemoveResource` and a thing
  of the rock class to `object_physics::TakeFromRock` (`store_rules::RockScaleLeft`, `RockAltitudeLeft`);
  `object_resources::GetResource` answers a worship site with `worship::site::GetFoodResource` and a building site
  with `building_sites::GetResource`. No interface position reaches a citadel's site from here (the same as the
  abodes' redirect). Tests: `TakeResourceTest.AWorshipSiteGivesTheFoodInItsPot`,
  `ABuildingSiteAndTheStructuresOnItGiveItsPilesWood`, `ADeadTreeGivesItsWoodAndGoesWhenAskedForAnythingElse`,
  `ABigForestAndAFishFarmGiveOnlyTheirOwnResource`, `ARockShrinksAsItsWoodIsTakenAndGoesWhenAllOfItIs`,
  `StoreRules.ARockKeepsTheShareOfItsWoodLeftAsItsScale`.
- [A worship site's food pot](#worship-food-pot): `object_resources::RemoveResource` takes from it, then asks the site
  (`worship::site::RemoveResource`) for the rest; `RemoveFromPotDirect` keeps it when emptied, as it keeps the piles of
  a storage pit, a building site and a workshop. Before (bug): an emptied pot was deleted, and the site made a new one
  at the next food it took. Tests: `TakeResourceTest.AWorshipSitesFoodPotIsPartOfTheSiteAndStaysWhenEmptied`,
  `AWorshipSitesFoodPotGivesWhatItHasThenTheSiteTheRest`.
- The building-site branches (+0x74): `object_resources::AddResource` / `RemoveResource` send an abode's wood (or
  any) to its building site, and `StoragePitStore::AddResource` sends an unbuilt pit's wood (or any) to its site
  (`src/ECS/ObjectResources.cpp`, `src/ECS/StoragePitStore.cpp`), as `Abode::AddResource` 0x404D90, `RemoveResource`
  0x404F10 and `StoragePit::AddResource` 0x732F67..0x732F99. `StoragePit::RemoveResource` 0x7332A0 has no such
  branch in the exe (it tests the type first and takes from the piles), and ours has none. Test:
  `TakeResourceTest.WoodOnAnUnbuiltPitIsBuildingSiteWood`; the abode's two branches have no test of their own.
- `PotInfo::StoragePitFoodPile` with `MagicFood` (the hand grab and villager resource tests): the pit's food pile and the temporary food pot have no cap.

#### Store (StoragePit)

- **Matched** in openblack (`StoragePitStore::DoCreatureMimicAfterAddingResource`, called from `object_resources::DoResourceAdding`, `object_delivery::DoDeleteObjectAndTakeResource` and `pot_resource::AddResourceToPos`)
- **Matched** in openblack: `pot_resource::AddResourceToPos` gives a pit the dropper and the poison through `pot_resource::StoragePitTakesPutDownResource` (the pit's `AddResource`, then the deed) and a pit's pile through `pot_resource::PotStructureAddResource`
- **Matched** in openblack: `pot_resource::Dropper::sourceOwner` (`std::optional`, none = NULL) is the hand's record (`HandSystem`, read by others through `HandSystemInterface::GetSourceOwner`): `HandSystem::PickUp` records `alignment::MostInfluentialPlayer` of the object's position
- openblack: `take_resource::StoragePit` and `worship::site::DeleteObjectAndTakeResource`, the trigger in `take_resource::TriggerSupplyHelpIfThrownByMe` (`help_profile::Trigger(Supply)` for a flying body the local hand threw).
- **Matched** in openblack (`object_delivery`, the ghost through `ecs::object_ghosts`, rendering-objects.md).
- openblack: DynamicsSystem::Find and its throwerPlayer compared with the local player (one interface per player, so the local player's is "MyInterfaceStatus").
- openblack's desire reads the piles' total (TownDesire GatherInputs: StoragePitStore::GetResource), so the piles are filled inside its JustAddResource step, between the two desire calls (the same values)
- openblack's mirror is the piles' total, so the desire is taken before the piles change.
- `StoragePitStore::AddResource`: only food and wood fill the pit's piles (all six are made with the pit and never deleted); the total from before the call is what `DoResourceAdding` reads.
- `StoragePitStore::RemoveResource`: only food and wood; each existing pile gives what it can, then `DoResourceRemoving` gets what came off, at most the store's total.
- `take_resource::StoragePit` (TakeResource.cpp): makes the store's `Reaction::ReactToHandPuttingStuffInStoragePit` (reaction 22), started by the store and stamped with the turn.

#### Static objects (MobileStatic, rocks)

- openblack: `held_apply`'s GateStone rows and `animatedStaticSystem.LayGateStone`.
- openblack: `ecs::held_apply` (the Fence rows), the fence branch of the physics' impact (`PhysicsObjects.cpp`), `object_delivery` (its wood and the mulch sound, WOOD that is not a pot).

#### The angles of a thing

- `ecs::placed_turn` (src/ECS/PlacedTurn.{h,cpp}) and the `TurnAngles` component (ported from raffclar's lane,
  85ec4ceae and 1c38abad2, unverified against v1.42): a thing keeps its angles beside its turn. `placed_turn::CanLean`
  is the classes of the scan above (the mobile statics but the singing stone's base, the mobile objects, the dead and
  felled trees, the fragments, the teleport stones, the landscape vortices, the football `Ball`, the sharks, the one-off
  spell seeds, the rewards, the scaffolds; `script_type::KeepsTiltWhenTurned` answers with it); `TurnOf` is
  `affine::RotationYXZ(y, x, z)` for those and `affine::AngleY(y)` for the others; `SetAngles` is `SetXYZAngles`.
- The archetypes keep the angles they place a thing at: the mobile statics (the bonfire's set angles too), the dead
  trees, the mobile objects, the pots, the sharks, the features, the trees, the abodes, the animated statics, the big
  forests and the flowers. A thing keeps no angles of its own until one of those makes it.
- The kept angles hold while the thing has the turn they were kept with (`TurnAngles::turn`). A code path that turns a
  thing without them (the hand, a creature) leaves angles that are no longer its own, and then the angles come from the
  turn, as `GetYXZ` takes them (`placed_turn::AnglesOfTurn`, `affine::DecomposeYXZ`).
- The physics hands a moved body's angles back each turn: `DynamicsSystem::SyncTransform` turns every moving thing
  but a villager or an animal by `RotationFromRows` (the turn made again from the body's angles: all three for a
  thing that leans, the angle about y for a tree or a feature) and keeps the angles it found; the dead tree a tree's
  end of physics leaves, and a thing a tornado lets go, do the same. Before, a thing that leans took the body's
  rows as they were, which equal that turn only to the rounding of their floats.
- The halves of a split rock take the angle about y the rock keeps (`object_physics::SplitRock`).
- The editor's turn of a thing that leans keeps its leans (`editor::Turn`).
- Tested by `PlacedTurn.*` (test/test_placed_turn.cpp, group test_core).

#### Fields (Field)

- openblack: `ecs/Fields`, `HandFish.cpp` (`TryPickUpField`, `UpdateFieldPickUp`), hook `OPENBLACK_HAND_TEST_FIELD=1`. The engine is faithful: the field is born empty and only the farmers sow it (`PlantCrop` from the farmer's
  `FARMER_PLANTS_CROP`, `src/ECS/Villager/VillagerFarmer.cpp`; test `VillagerFarmingTest.FarmerPlantsCrop`). The growth
  reads the land's alignment and the rain at the field (`effects::alignment::LandAlignmentAt`, `weather::IsRainingAt`
  into `GrowthStep`; tests `FieldsTest.GrowthStepMultipliers`, `OnlyASownCropNotYetPastRipeGrows`).
- openblack draws it opaque too (`FieldSystem`: no `Alpha` below 80%, only held there)
- openblack keeps the Town::id (FieldArchetype, CREATE_TOWN_FIELD)
- `FieldArchetype::Create`: the field keeps its `FieldTypeInfo` (default `FieldTypeInfo::Wheat`), joins its town's field list with its own radius, starts empty for the farmers to sow, and takes a turn offset `GameRand(10)`.
- `GrowthStep` (Fields.{h,cpp}): the growth of one field step, `2 (alignment x 0.5 + 1)` times the rain or sun multiplier for growing or ripening.
- `ApplyWaterSpellToField` (Fields.cpp): a field with at most `Field::k_TimesToSow` crops gets `k_TimesToSow + 1`.
- `HasFarmer` (Fields.h): is the villager in the field's farmer list; `OnFieldDestroyed`: entt's `on_destroy<Field>`, the field's dependants released while the component is still there.

#### The football (Ball)

- openblack: `components::Ball` (its last kick's destination) and `BallArchetype::Create` (the creation number, `Transform`, `Mobile`, `Mesh` of the info's mesh, the neutral player's `ReactToBall`, unstamped, then `map_cells::InsertMapObject`: an `Object` kind, of `GBallInfo`'s type, `map_cells::TypeOf`); to the scripts it is type 28 BALL (`Ball::GetScriptObjectType` 0x436100, vt +0x4E8), with no sub-type (`fn_006F6D00`'s default: 9999) (`script_type::TypeOf`); `BallArchetype::CreateForNearestTown` is the CREATE case (`map_cells::GetNearestTownToPos`, then `town_playthings::GiveBall` on `Town::playthings`, the newest first; "still about" is valid, not `Unavailable` and a ball). A destroyed ball's entity in a list is no longer valid, so it no longer counts, where the original reads freed memory. The per-turn walk has nothing to port; `Magic/MagicLoop.cpp` notes its place. Testbed: `objects.ball_nearest_town` (the `balls` fixture calls the same CREATE function). raffclar's `town_playthings::Nearest` (towns by id within each player) stays unused: the town walk is `map_cells::GetNearestTownToPos`, which follows each player's list order (`Town::ownerListStamp`).

#### Chimney smoke (LH3DSmoke)

- **openblack**: `ecs::components::ChimneySmoke` (src/ECS/Components/ChimneySmoke.h, on the Abode from `AbodeArchetype::Create`), `chimney_smoke` (src/3D/ChimneySmoke.{h,cpp}: Create, Drift, UpdateState, Advance, LookOf) and `Locator::chimneySmokeSystem` (Attach, UpdateHandWind, Drift), drawing in `Renderer::CollectChimneySmoke` / `Renderer::DrawChimneySmoke` (src/Graphics/RendererSmoke.cpp; depth test, no depth write, no culling; `LoadAlpha` builds the smoke material from `smokea.raw`'s alpha, since `smoke.raw` is white in the cells used): one object per chimney in the back-to-front list of the main pass (`ZObject::smoke`, key the squared distance to the chimney), its puffs in order 0..9 with the Sprite shader (`smokea.raw`, premultiplied tint, ONE/INVSRCALPHA = mode 6, rotation −angle in the screen plane). `Abode::presentAtHome` is raised and lowered by the villagers (0x405FA0 / 0x405FB0), and the windows read it like the original (`Abode::Draw` 0x515F78: +0xB6 ≠ 0 and then `IsVisualNight` 0x5575E0).

#### Field details

- openblack guard

### Test hooks

- `OPENBLACK_TEST_FIELD_GROWTH=0..1200`: every field starts at that growth, with the food it would have.
- `OPENBLACK_HAND_TEST_STORE_TAKE="wood,food"`: takes that much from every storage pit (the store's visuals).
- `OPENBLACK_HAND_TEST_FIELD=1`: taking from a field.
- `OPENBLACK_HAND_TEST_FISH=1`: taking from a fish farm; traces the pick-up loop (one start, pitch 0.60 → 1.02).
- `OPENBLACK_AUDIO_TRACE=1`: `sample_play` channels (hand in the water, collisions, ambience).
- `OPENBLACK_ATMOS_TRACE=<n>`: the ambience (sound map and banks).
- Trees and map loading: the hooks of [Test hooks](trees.md#test-hooks) and
  [Test hooks](../scripts/map-script-reader.md#test-hooks).

## Pending

- **Verify with xref/oracle:** the 3 **🔍 inferred** and 7 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- **The angles of a thing**: the living, the creature and the citadel's parts keep no angles of their own
  (`TurnAngles`), so their angle comes from their turn; the singing stone's base made by a land script with x and z
  angles is drawn leaning, where the original's `GBaseOnly` stands upright on its angle about y; an upright thing's
  turn is `affine::AngleY`, whose zeros are all +0 and which turns by a not-a-number angle, where the original's four
  branches give the zeros the signs of their products and no turn for an angle of 0 or not a number (raffclar's
  `GameUprightTurn`, not read in the exe here).
- The put-down particles of a pot let go gently (`StartMultiPutdown`): openblack does not make them yet.
- The puzzle grain: openblack has no entity for it (not ported, a port row of the puzzle).

- **The football:** the villagers' pick-up reaction (`Villager::GotoPickupBallReaction` /
  `ArrivesAtPickupBallReaction` 0x765320 → `fn_00436000` → `AddPlaythingIfNoneThere`, then `GoHome`): our villagers
  have no `ReactToBall` handler, so a ball's reaction reaches no one, and the ball has no town of its own (+0xA0) nor
  state (+0x90) yet; with them, the destructor's `RemovePlaything`. The football game (pitch, teams, kicks, the +0x90 = 2
  branch), the hand and physics of a ball (the testbed's balls are mobile objects of row `MobileObjectInfo::Ball`, not
  this class) are not ported.
- **Reward chests:** opening one is imported, unverified against v1.42 (see
  [Land script objects](../scripts/map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240));
  240 naming the town as the script's creation; the physics once landed. And the dust's time while the camera stands still and the chest is off the screen: the
  landscape's list then draws only what was active at its last full pass, so the time likely stands still (inferred:
  who clears `g_b_last_on_screen` is not read); openblack counts it down every frame.
- Taking in batches: the fish particles' colour. They are made (`HandSystem::UpdatePickupParticles`,
  `HandEffects.cpp`: `S_Spangle_A` cells 48..63 at 40 fps, as `SF_MultiPickUpFoodFish`), but tinted by the mean colour
  of those cells where the original uses 200 x the landscape colour (**⚠️ approximate**).
- `PileFood::Draw`: confirm that LH3DObject's vfunc 0xE8 is the UV offset (🔍 inferred).
- The status's +0x128 (who owned what the hand picked up): openblack's record starts empty with the hand system and
  is not cleared again when a land is set up (`GPlayer::Init` → `GInterface::Init` → `GInterfaceStatus::SetToZero`);
  whether a land change reaches that path is not traced. A town's player can be NULL in the original (`Container`
  +0x2C); openblack's `Town::owner` is always a player (the neutral one when none), so a farm or pit of such a town
  records the neutral player instead of NULL; whether a land makes such a town is not checked.

