# Buildings, building sites and towns (the building side)

The buildings of a town as the original runs them (Abode.cpp, MultiMapFixed.cpp, StoragePit.cpp, BuildingSite.cpp,
Town.cpp of `runblack.exe` W120) and how openblack does it. Owner: session Edificios (2026-10-03). The villagers that
use these buildings (going home, builders, carrying) are in [villagers.md](villagers.md); the physics of a building
that breaks is in [physics.md](physics.md).

- [Resources held by objects](#resources-held-by-objects)
- [The town's temporary pots](#the-towns-temporary-pots)
- [Life and damage](#life-and-damage)
- [Plans and building sites](#plans-and-building-sites)
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

- **Pots and piles**: a pile of a storage pit answers the pit's total (PotStructure::GetResource 0x66EF00); any other
  pot its own amount when the resource is its own (Pot::JustGetResource 0x66D390). RemoveResource of a pit's pile
  (PotStructure 0x66EE10): the touched pile gives n - min(over, n) (over = CalulateAmountOverMaximum 0x733260: wood
  above 25000, food above 15000), then the pit pile 5 -> 1 the rest. A pot that empties (Pot::JustRemoveResource
  0x66D410) loses its reaction, its poison and its fire; a loose pile, a temporary pot, MagicFood or MagicWood is then
  deleted (PotStructure::JustRemoveResource 0x66D9B0), a pit's pile stays. AddResource of a pile: PotStructure 0x66ED70
  (`pot_resource`).
- **Storage pit** (`StoragePitStore`, StoragePit::AddResource 0x732F60 / RemoveResource 0x7332A0): only FOOD and WOOD;
  each pile's JustAddResource (the pile sound with the amount still asked for, the cap, the poison), the town pulse
  +0x5E8 = 1 / +0x5EC = 0 when the pit held none of it and something went in, then DoResourceAdding; removing goes
  pile 5 -> 1 and then DoResourceRemoving (the town's desire before the change, also for villagers).

**The hand's branch.** DoResourceAdding 0x404DF0 and DoResourceRemoving 0x404F60 do more only with a GInterfaceStatus
(`pot_resource::Dropper`); villagers and scripts pass none (0x404E01 `je 0x404EE3`).
- Removing: Town::SetGameTurnResourceLastRemoved 0x7400D0 (Town +0xEC8[player][type] = the turn), the desire after,
  and GAlignment::Update(this, type, -n, before - after) 0x414520 of the **town's owner** (`alignment::UpdateForResource`:
  k = n > 0 ? giveResourceAligmnetChangeMultiplier : take..., 0.5 each; pending += ScaleChange(delta x k)).
- Adding: delta = before - after x Town::GetGameTurnResourceLastRemovedModifier 0x740030 (never taken 1, else
  min((turn - last) / 1000, 1)^3), GAlignment::Update of the dropper's player, and GBelief::AddToBelief 0x437EB0 with
  delta x (not the owner ? multiplierForNonOwnerAddingResource 1.0 : 1) x multiplierForAddingResourceToTown 0.25:
  Town +0x798 GBelief +0xC8 (pending, folded each turn) and +0x28 (recent, decays) += f, +0x48 = the turn
  (`components::TownBelief`). DoCreatureMimicAfterAddingResource (vt +0x68C) is (not ported): no creature.
- **(pending)** the town's per-turn belief fold fn_004383D0 (dev\documentacion\edificios\belief_spec.md), DrawBelief
  0x438800 and BeliefSFX 0x437F40 (it needs the interface's position IS +0x14).

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

## Plans and building sites

`ecs::plans` and `ecs::building_sites` (`src/ECS/Town/BuildingSites.h`), the building's own state in `ecs::abodes`
(`src/ECS/Abodes.h`). Spec with every address: `dev\documentacion\edificios\V6_spec.md`, `V6_pending.md`; two
audits (`audit_v6.md`, `audit_v6_pass2.md`).

- **Plans** (PlannedMultiMapFixed, a GameThingWithPos: not drawn, Draw 0x648930 = `ret`; not in the map cells) live in
  the town's list, oldest first (Town::AddPlanned 0x73D080). The town never invents them: the script
  (CREATE_PLANNED_ABODE), destroyed buildings (V11), scaffolds and the rival AI make them.
- **Choosing**: GetDesireToBeBuilt 0x73A1A0 per type (houses by free adult places, civic buildings once, wonder by the
  For_Wonder desire...), GetBestPlanned 0x73A140 (strictly better, the first on ties), RequestBestPlanned 0x73A650
  (mask 4, no fixed check; fields pass too), RequestANewAbode 0x73B330 (mask 2, with the fixed check
  IsSuitableForFixedAbodeInTown 0x603860: inside the nearest other town's area widened by 4 cells it is accepted with
  no other test).
- **Converting** a plan (CreatePlannedNoFixedCheck 0x405770): the abode is created under construction (+0x58 bit 1,
  percent 0, life 1.0 from the Object ctor), then its building site (BuildingSite 0x43B7E0: the 128 builder positions of
  PosBuilder 0x43AE10 around the mesh, the repair base 1.1 x life - 0.1 for a damaged built building). CHL
  BUILD_BUILDING 130 (GScript::BuildBuilding 0x6FAB30 -> ForceBuildingOfPlannedAtPos 0x73E560) converts the plan at the
  point within the same call, with the desire boost x 5.
- **Building**: BuildBy 0x52ED40 adds to the percent built; at 1 Built 0x52EBB0 / Abode::Built 0x404720 deletes the
  site (builders back to state 163, the wood pile released) and MakeFunctional 0x4047E0 runs (the town's statistics count
  the abode from then on: Add(Abode) 0x7498C0). On a built but damaged building BuildBy repairs (IncreaseLife 0x405ED0,
  Repaired 0x4047B0).
- **Drawing**: DrawBuilding 0x517F90 draws the partial model with GetPercentForDrawBuilding 0x52EFD0
  (`components::DrawMesh`, the mesh of `physics::PartialBuild::BuildMesh`), nothing at exactly 0 %
  (`components::NotDrawn`) but the ground footprint stays (SetFootPrintOnTexture 0x52EA33), no static shadow until
  built (SetShadowOnTexture 0x1000, `abodes::CastsShadowOnTexture`), and no land haze (only fn_00801C90).
- **Graveyard** (`src/ECS/Town/Graveyard.h`): Town +0x748; AddDead fn_00595E50 counts the dead below 50 and sets the
  graves stage max(1, ftol(n x 0.18)) & 7 on the 3D object.

**(pending)** picking a 0 % (invisible) house; Town +0x740 totem / +0x750 workshops / +0xEA4 football; the graves
stage's reader; feature_build should hide with NotDrawn; scaffolds, workshops and footpaths are not ported.
**(not verified)** freeAdultPlaces = MaxVillagers - adults housed. **(approximate)** the town areas recomputed at the
query; a deleted site is freed one turn later.

## Pending

- Town::Process step 17 (0x747450..0x7474A0): empty temporary pots go when a storage pit works.
- CREATE_TOWN_TEMPORARY_POTS (0x716B65..0x716BF0).
- The building-site branches (+0x74) of Abode / StoragePit AddResource and RemoveResource (V6).
- A missing pit pile is created by StoragePit::AddResource (Pot::Create 0x66CF10); openblack makes all six with the
  pit and never deletes them **(approximate)**. How CREATE_ABODE fills a new pit is not read: today it goes through
  StoragePit::AddResource (pile sounds and the pulse at load) **(approximate)**.
- The belief fold, DrawBelief and BeliefSFX (above).
- The citadel as a plan, repairs (V11), the workshop and scaffolds, the town emergency.

## Sources

- Disassembly with `dev\herramientas\dis\bwdis.py`; research in `dev\documentacion\edificios\` (V5_resources_spec.md, V6_spec.md, belief_spec.md) and
  `dev\documentacion\aldeanos\v5\` (abode_add.txt, pit.txt, tpot.txt).
- bw1-decomp `src/Black/Abode.h`, `GameThingWithPos.h` (Flags +0x24), `Town.h`.
