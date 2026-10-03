# Buildings, building sites and towns (the building side)

The buildings of a town as the original runs them (Abode.cpp, MultiMapFixed.cpp, StoragePit.cpp, BuildingSite.cpp,
Town.cpp of `runblack.exe` W120) and how openblack does it. Owner: session Edificios (2026-10-03). The villagers that
use these buildings (going home, builders, carrying) are in [villagers.md](villagers.md); the physics of a building
that breaks is in [physics.md](physics.md).

- [Resources held by objects](#resources-held-by-objects)
- [The town's temporary pots](#the-towns-temporary-pots)
- [Life and damage](#life-and-damage)
- [Pending](#pending)
- [Sources](#sources)

## Resources held by objects

`ecs::object_resources` (`src/ECS/ObjectResources.h`) is the original's Object::GetResource (vt +0x98),
RemoveResource (vt +0xA0), AddResource (vt +0x9C) and IsPoisoned (vt +0x4A4) for every object that holds food or wood:

- **Abode**: +0xBC[type] (`Abode::foodAmount` / `woodAmount`). GetResource 0x404D30; RemoveResource 0x404F10 ->
  DoResourceRemoving 0x404F60 (the town's CallDesireFunction before the removal, then JustRemoveResource 0x404D60 =
  min(amount, held)); AddResource 0x404D90 -> DoResourceAdding 0x404DF0 -> JustAddResource 0x404D40 (no cap).
- **Storage pit**: the total of its piles (PotStructure::GetResource 0x66EF00, StoragePit::RemoveResource 0x7332A0,
  `StoragePitStore`).
- **IsPoisoned**: StoragePit 0x7336B0 (any available pile poisoned), Pot 0x55D4E0, an abode 0 (GameThingWithPos
  0x402400), a villager its own flag.

DoResourceAdding 0x404DF0 and DoResourceRemoving 0x404F60 do more only when a GInterfaceStatus is passed (the
hand): the desire before and after, Town::GetGameTurnResourceLastRemovedModifier 0x740030, GAlignment::Update 0x414520,
GBelief::AddToBelief 0x437EB0 (Town +0x798) and DoCreatureMimicAfterAddingResource (vt +0x68C). With no interface
(0x404E01 `je 0x404EE3`) only JustAddResource runs, so the villagers' path is complete **(pending: the hand's branch)**.

## The town's temporary pots

`ecs::town_stores::GetTemporaryResourceStorePotOrPos(town, from, type)` is Town::GetTemporaryResourceStorePotOrPos
0x73E900: where villagers drop resources while the town has no working storage pit.

- The pot of Town +0x600[type] (FOOD +0x600, WOOD +0x604; `components::Town::temporaryPots`) if it is available
  (vt +0x2C).
- Otherwise a new one: GetCongregationPos 0x7408B0 + GetPosFromAngle 0x74D580(0, WOOD ? 5 : 0) (0x73E96E), moved by
  FindClearArea 0x7412F0(45, 1.5, 2) away from MultiMapFixed objects (the filter 0x73EA50 is GameThingWithPos::Flags
  +0x24 bit 1, the MultiMapFixed bit; its result is not tested), then Pot::Create 0x66CF10 with GPotInfo 10 MagicFood /
  9 MagicWood and **amount 0**. For those two infos Pot::Create calls the MagicFood 0x5FA9F0 / MagicWood 0x600E20
  constructors (player NULL; the town is not passed on) and CallVirtualFunctionsForCreation (vt +0x658): openblack's
  `magic::objects::CreateMagicResourcePile(..., allowEmpty = true)`.
- Either way the point returned is the pot's GetNearestEdgeToPos(from) (vt +0x83C, Object 0x636DA0).
- There is always a pot: Pot::Create fails only when its allocation does (0x66CF81), and 0x73EA18 uses the result
  unchecked.
- **(approximate)** `town_queries::GetCongregationPos` returns x and z only; the cache's altitude is dropped (the pile
  stands on the land anyway).
- openblack's `PotArchetype::Create` makes no pile of amount 0 unless `allowEmpty`: an openblack rule, not the
  original's (Pot::Create has no such check).

## Life and damage

**(pending)** The building's side of physical damage (Abode::ApplyEffectsDueToPhysicalDestruction 0x406640,
StopBeingFunctional 0x4073C0, DestroyedByEffect 0x403F80) moves here from [physics.md](physics.md). The original
lowers the life through the effect system (EffectValues(3) 0x524FE0, damage = max(0, life - remaining) /
GetDefenseMultiplier 0x637930, Object::ApplyEffect vt +0x5CC -> Abode::ReduceLife 0x405D90), not by setting it, and
shows HelpSpritesDestroyBuilding 0x71D070 when less than 0.4 is left (0x406753) and the player is the local one.

## Pending

- The hand's GInterfaceStatus branch of DoResourceAdding / DoResourceRemoving (with `pot_resource::Dropper`).
- GetResource / RemoveResource / AddResource of pots and piles (PotStructure 0x66EF00 / 0x66EE10 / 0x66ED70).
- StoragePit::AddResource 0x732F60 in full (building-site branch, pulse +0x5E8 / +0x5EC).
- Town::Process step 17 (0x747450..0x7474A0): empty temporary pots go when a storage pit works.
- CREATE_TOWN_TEMPORARY_POTS (0x716B65..0x716BF0).
- Plans and building sites (V6), the citadel as a plan, repairs, the workshop and scaffolds, the town emergency.

## Sources

- Disassembly with `dev\herramientas\dis\bwdis.py`; research in `dev\documentacion\edificios\` and
  `dev\documentacion\aldeanos\v5\` (abode_add.txt, pit.txt, tpot.txt).
- bw1-decomp `src/Black/Abode.h`, `GameThingWithPos.h` (Flags +0x24), `Town.h`.
