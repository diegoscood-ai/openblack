# Town storehouse

The storage pit is the town's store of food and wood: piles of wood and a pile of food beside it, which grow and shrink
as villagers bring and take, and which the player can add to by hand or miracle. Without one the town makes do with
temporary pots and its abodes' own stores.

**Progress: 15/21 done, 4 partial — 81%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit holds five wood piles and a food pile placed by its model | done | the pit's five wood piles and food pile at its model's points (`src/ECS/Archetypes/AbodeArchetype.cpp`, `components::StoragePit`) |
| Scripts start the pit with food and wood | done | CREATE_ABODE's food and wood go in through `StoragePitStore::AddResource` (`src/ECS/Archetypes/AbodeArchetype.cpp`) |
| Piles rise out of the ground and sink as they fill and empty | done | each pile's size follows its amount (`src/ECS/PotResource.cpp`); `test/test_resource_stores.cpp` |
| Wood is added and taken pile by pile in the game's order | done | wood goes into piles 1 to 5 and comes out of 5 to 1 (`StoragePitStore::AddResource` and `RemoveResource`, `src/ECS/StoragePitStore.cpp`) |
| The town's total food and wood follows the piles | done | the abode's totals are kept in step with the piles (`StoragePitStore::SyncTotals`); a pile reports the store's total |
| A pit can hold only so much; the excess is left over | done | each pile caps at its pot's maximum (`AddResource`), and `StoragePitStore::AmountOverMaximum` gives the excess |
| Villagers drop off food and wood they gather at the pit | done | the drop-off states (`GOTO_STORAGE_PIT_FOR_DROP_OFF`, `ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF`, `src/ECS/Villager/VillagerResources.cpp`); See ../villager/jobs.md |
| Housewives and builders take food and wood from the pit | partial | builders fetch wood (`ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS`) and villagers fetch their own food (`GOTO_STORAGE_PIT_FOR_FOOD`, `src/ECS/Villager/VillagerFood.cpp`); the housewife's states are TODO rows of the state table |
| Food and wood miracles cast on the pit fill it | done | each landing grain goes into the store it reaches (`pot_resource::AddResourceToPos`, `StoragePitTakesPutDownResource`, from `src/Magic/Spells/SpellResource.cpp`); see ../miracles/ |
| The pit is the town's resource drop point; the nearest edge is used | done | villagers walk to the store's nearest edge (`GetResourceNearestEdge`, `src/ECS/Villager/VillagerResources.cpp`) |
| Without a pit, temporary pots near the town hold its resources | partial | the town's temporary food or wood pot is made when a villager needs a store and the town has none (`town_stores::GetTemporaryResourceStorePotOrPos`, `src/ECS/Town/TownStores.cpp`) and goes once there is a pit; CREATE_TOWN_TEMPORARY_POTS is an empty stub (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Abodes keep some food of their own for dinner | done | the abode's own food (`Abode::foodAmount`), taken when a villager eats at home (`src/ECS/Villager/VillagerFood.cpp`) |

## The player and the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can take food or wood from the pit | done | the hand scoops from the pit's piles (`HandSystem::PickUp`, `src/ECS/Systems/Implementations/HandResources.cpp`); See ../resources/ |
| Dropping food or wood into the pit by hand adds it, and villagers react | partial | a handful put on the pit goes in, with the town's alignment and belief (`src/ECS/HeldApply.cpp`, `src/ECS/ObjectResources.cpp`); the town's spoken answer is pending (`src/ECS/Town/TownStores.cpp`) |
| Taking from a town's pit lowers its belief in the taker for a while | done | the turn of the taking is kept (`town_stores::SetGameTurnResourceLastRemoved`) and later gifts of that resource earn less belief (`src/ECS/ObjectResources.cpp`). Our wiki differs: taking lowers no belief at once; it cuts what later gifts of that resource earn, for up to 1000 turns ([buildings](../../bw1-notes/buildings.md#resources-held-by-objects)) |
| A pit belonging to another player gives its belief to that player when touched | done | giving to a town that is not the giver's earns the giver belief, times the non-owner multiplier (`DoResourceAdding`, `src/ECS/ObjectResources.cpp`) |
| Food in the pit can be poisoned, harming those who eat it | done | the piles keep their poison (`StoragePitStore::AddResource`), and a villager who eats poisoned food loses life and shows it (`SHOW_POISONED`, `src/ECS/Villager/VillagerFood.cpp`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| Magic food in the pit speeds up those who take from it, with a sparkle | partial | the pile's speed-up sparkle (`pot_resource::SetSpeedUp`) and the villagers' food speed-up rule (`src/ECS/VillagerSpeed.cpp`, `ProcessFoodSpeedup`) exist, but nothing gives a villager the speed-up; see ../villager/daily_routine.md |
| The creature can eat from the pit and copy what the player does with it | todo | the deed a creature would copy is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`) but the mimicry is pending; See ../creature/ |
| Damaging the pit puts the town in a state of emergency | done | `abodes::ReduceLife` (`src/ECS/Abodes.cpp`) and the pit on fire (`src/ECS/Town/TownEmergency.cpp`); See emergencies_and_aggression.md |
| The pit casts a shadow at night | todo | see ../rendering/shadows.md |
