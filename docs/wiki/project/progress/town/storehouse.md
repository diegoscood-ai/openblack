# Town storehouse

The storage pit is the town's store of food and wood: piles of wood and a pile of food beside it, which grow and shrink
as villagers bring and take, and which the player can add to by hand or miracle. Without one the town makes do with
temporary pots and its abodes' own stores.

**Progress: 15/19 done, 0 chosen, 4 partial, 0 inferred — 95%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/buildings.md).

## The store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storage pit holds five wood piles and a food pile placed by its model | partial 85% | the pit's five wood piles and food pile at its model's points (`src/ECS/Archetypes/AbodeArchetype.cpp`, `components::StoragePit`; `ResourceStoresTest.APitAndItsPilesAnswerByThePilesType`); all six are made with the pit and never deleted, where the original makes a missing pile in `StoragePit::AddResource` ([buildings](../../../world/buildings.md#pending)) |
| Scripts start the pit with food and wood | partial 80% | CREATE_ABODE's food and wood go in through `StoragePitStore::AddResource` (`src/ECS/Archetypes/AbodeArchetype.cpp`; fidelity); how the original fills a new pit is not read yet (the pile sounds and the pulse at load may differ, [buildings](../../../world/buildings.md#pending)) |
| Piles rise out of the ground and sink as they fill and empty | done | each pile's size follows its amount (`src/ECS/PotResource.cpp`); `test/test_resource_stores.cpp` |
| Wood is added and taken pile by pile in the game's order | done | wood goes into piles 1 to 5 and comes out of 5 to 1 (`StoragePitStore::AddResource` and `RemoveResource`, `src/ECS/StoragePitStore.cpp`; fidelity), as StoragePit::RemoveResource 0x7332A0 |
| The town's total food and wood follows the piles | done | the abode's totals are kept in step with the piles (`StoragePitStore::SyncTotals`); a pile reports the store's total; `HeldApplyTest.AHandPotOfWoodOverAPitGoesWholeIntoItsTotal` |
| A pit can hold only so much; the excess is left over | done | each pile caps at its pot's maximum (`AddResource`), and `StoragePitStore::AmountOverMaximum` gives the excess (wood above 25000, food above 15000); `StoreRules.APileGivesOnlyWhatIsWithinItsStoresMaximum` |
| Villagers drop off food and wood they gather at the pit | done | the drop-off states (`GOTO_STORAGE_PIT_FOR_DROP_OFF`, `ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF`, `src/ECS/Villager/VillagerResources.cpp`; `VillagerResourcesTest.ArrivesAtStoragePitForDropOff`); See ../villager/jobs.md |
| Builders, workshop carriers and hungry villagers take wood and food from the pit | done | builders fetch wood (`ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS`, `VillagerBuildTest.ArrivesAtStoragePitForBuildingMaterials`), carriers take wood to the workshop (`ArrivesAtStoragePitForWorkshopMaterials`, `src/ECS/Villager/VillagerSatisfy.cpp`) and villagers fetch their own food (`VillagerFoodTest.ArrivesAtStoragePitForFood`). The housewife's day (states 100..109) is dead code in the game ([villager life](../../../beings/villager-life.md)) |
| Food and wood miracles cast on the pit fill it | done | each landing grain goes into the store it reaches (`pot_resource::AddResourceToPos`, `StoragePitTakesPutDownResource`, from `src/Magic/Spells/SpellResource.cpp`; `test/test_take_resource.cpp`, `test/test_food_wood.cpp`); see ../miracles/ |
| The pit is the town's resource drop point; the nearest edge is used | done | villagers walk to the store's nearest edge (`GetResourceNearestEdge`, `src/ECS/Villager/VillagerResources.cpp`; `VillagerResourcesTest.GetResourceDropoffPos`) |
| Without a pit, temporary pots near the town hold its resources | partial 75% | the town's temporary food or wood pot is made when a villager needs a store and the town has none (`town_stores::GetTemporaryResourceStorePotOrPos`, `src/ECS/Town/TownStores.cpp`) and goes once there is a pit (step 17, `ProcessTemporaryPots`; `test/test_villager_resources.cpp`, `test/test_deadlist_buildings.cpp`); the script's CREATE_TOWN_TEMPORARY_POTS (0x716B58, which asks for both pots) is an empty stub (`src/LHScriptX/FeatureScriptCommands.cpp`) |
| Abodes keep some food of their own for dinner | done | the abode's own food (`Abode::foodAmount`), taken when a villager eats at home (`src/ECS/Villager/VillagerFood.cpp`; `VillagerFoodTest.GetFoodFromHomeTakesTwice`) |

## The player and the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand can take food or wood from the pit | done | the hand scoops from the pit's piles (`HandSystem::PickUp`, `src/ECS/Systems/Implementations/HandResources.cpp`; `TakeResourceTest.PickUpFromAPileRecordsThePilesPlayer`); See ../resources/ |
| Dropping food or wood into the pit by hand adds it, and villagers react | done | a handful put on the pit goes in, with the dropper's alignment, the town's belief and its belief sound (`DoResourceAdding` 0x404DF0: `src/ECS/HeldApply.cpp`, `src/ECS/ObjectResources.cpp`, `town_stores::AddToBelief`; `test/test_hand_apply.cpp`, `test/test_take_resource.cpp`); the drop remark is silent for a pit in the original too (its type is 0) |
| Taking from a town's pit lowers its belief in the taker for a while | done | the turn of the taking is kept (`town_stores::SetGameTurnResourceLastRemoved`) and later gifts of that resource earn less belief (`src/ECS/ObjectResources.cpp`). Our wiki differs: taking lowers no belief at once; it cuts what later gifts of that resource earn, for up to 1000 turns ([buildings](../../../world/buildings.md#resources-held-by-objects)); `StoreRules.GivingBackWhatWasTakenLatelyCountsForLess` |
| A pit belonging to another player gives its belief to that player when touched | done | giving to a town that is not the giver's earns the giver belief, times the non-owner multiplier (`DoResourceAdding`, `src/ECS/ObjectResources.cpp`) |
| Food in the pit can be poisoned, harming those who eat it | done | the piles keep their poison (`StoragePitStore::AddResource`), and a villager who eats poisoned food loses life and shows it (`SHOW_POISONED`, `src/ECS/Villager/VillagerFood.cpp`; `TakeResourceTest.APoisonedPutDownPoisonsThePit`); see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) |
| The creature can eat from the pit and copy what the player does with it | partial 60% | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`, `test/test_take_resource.cpp`) and handed to the creature's mind (`creature_mimic::Consider`, `test/creature/test_creature_mimic.cpp`); eating from the pit in place (`creature_mind::EatFromStoragePit`) is raffclar's, unverified against v1.42; See ../creature/learning_by_observation.md |
| Damaging the pit puts the town in a state of emergency | done | `abodes::ReduceLife` (`src/ECS/Abodes.cpp`) and the pit on fire (`src/ECS/Town/TownEmergency.cpp`; `test/test_repairs.cpp`); See emergencies_and_aggression.md |
