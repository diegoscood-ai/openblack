# Wood

What the towns build and repair with. Foresters fell trees and carry the logs to the store; the player can pull up trees
or pour the wood miracle; builders take wood from the store to building sites and the workshop.

**Progress: 11/17 done, 0 chosen, 5 partial, 0 inferred — 89%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## Where wood comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters fell trees of the town's forests and carry the logs to the store | done | `FORESTER_GOTO_FOREST`, `FORESTER_CHOPS_TREE` (`src/ECS/Villager/VillagerForester.cpp`); test `test/test_villager_forester.cpp`; see `../villager/` |
| A tree gives wood by its kind and size | done | Test `StoreRules.ATreesWoodScalesOnceWithItsSizeAndWithItsLife`; `ecs::TreeWoodValue` (`src/ECS/Trees.cpp`): the info table's wood value by the tree's life and scale |
| A tree a forest miracle grew gives more wood, by its caster's tribal power | done | Test `TakeResourceTest.AForestMiraclesTreeGivesWoodByItsCastersTribalPower` (`test/test_take_resource.cpp`: 0.25 x the tribal power times an ordinary tree's wood, kept by the dead tree it becomes, as [the forest miracle](../../../magic/forest.md)); the multiplier kept on the tree (`magic_tree`, `Tree::woodValueMultiplier`) is read by `ecs::TreeWoodValue`, so felling it gives more |
| A big forest's wood goes down by what is taken, and with too little left the forest is deleted | done | `ecs::BigForestRemoveWood` (`src/ECS/Trees.cpp`), as [the wiki](../../../hand/grab-hold-and-throw.md#pick-up-rules-and-bigforest); test `TakeResourceTest.ABigForestAndAFishFarmGiveOnlyTheirOwnResource` |
| Uprooted (dead) trees and felled trunks count as wood | done | Tests `StoreRules.ADeadTreesWoodIgnoresItsLife`, `TakeResourceTest.ADeadTreeGivesItsWoodAndGoesWhenAskedForAnythingElse`; a dead tree's wood by its kind and scale (`ecs::TreeWood`, `src/ECS/Trees.cpp`) |
| A tree pulled up by the hand is held as branches or logs by its kind (evergreen, fruit, hardwood) | pending | the hand holds the uprooted tree itself (`HandHolding.cpp`); see [handling](resource_handling.md) |

## Storing and using wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting wood | partial 90% | No test; `AbodeArchetype` (CREATE_ABODE's wood amount). How the original fills a new store is still open ([the store](../../../world/objects.md#store-storagepit): CREATE_ABODE's fill is approximate), so no value can be pinned yet |
| The store's wood shows as five piles that fill in turn | done | see [stores and piles](stores_and_piles.md) |
| Builders carry wood from the store to building sites and scaffolds | done | Test `VillagerResourcesTest.AtStructureAddResource`; `src/ECS/Villager/VillagerBuild.cpp`; see `../building/` |
| The workshop takes wood to make scaffolds | done | Tests `ResourceStoreTable.AWorkshopTakesWoodAndAny`, `ResourceStoresTest.AWorkshopsPileTakesWoodThroughItsWorkshop`; the workshop takes its wood value per scaffold and makes one in a free slot (`src/ECS/Town/Workshops.cpp`); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Damaged buildings are repaired with wood | done | Scene `physics.building_repaired`; the repair path of `src/ECS/Villager/VillagerBuild.cpp` |
| Wood poured near a workshop, building site or scaffold goes to it | done | Tests `ResourceStoresTest.ABuildingTakesWoodWhileItHasABuildingSite`, `AScaffoldTakesWoodWhileItBelongsToABuildingSite`; `pot_resource::AddResourceToPos` offers it to every store in reach, workshops and building sites included (`src/ECS/PotResource.cpp`, `src/ECS/ResourceStores.cpp`) |
| A town short of wood raises its wood desire flag | done | Test `TownDesireTest.Wood` (`test/test_town_desire.cpp`: the wood desire from the store's wood, k = max(abodes / 10, 1), 500 k and 5000 k, and the LowOnWood warning, as [the desire functions](../../../beings/villager-decisions.md)); the flag flies by that desire (`TownDesireFlags.AShownFlagEasesUpToItsDesireNeverPastWhole`, `AFlagRisesFourMetresAndTwoFifthsWhenWhollyWanted`); the town's wood desire reads its store (`src/ECS/TownDesire.cpp`); see `../town/` |
| Giving wood to a town that wants it impresses the town | done | Test `StoreRules.AGiftToATownCountsForTheDropInItsDesire` (`test/test_store_rules.cpp`, as [the hand's branch](../../../world/buildings.md#resources-held-by-objects)); `DoResourceAdding` (`src/ECS/ObjectResources.cpp`, `store_rules::GiftToTown`) |
| Wood poured on a store or by a building site teaches the creature to do the same | done | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`) and passed to `creature_mimic::Consider` (`ReportResourceDeed`, `src/ECS/ObjectResources.cpp`); tests `TakeResourceTest.WoodOnAnUnbuiltPitIsBuildingSiteWood`, `OwnFoodAndAnyWoodDrawNothing` |
| The creature carries wood and builds with it | partial 50% | Scenes `objects.give_wood`, `objects.give_wood_site`, `objects.repair_house`; missing: not checked against v1.42; see `../creature/` |
| Scripts read, add and take a town's wood | done | Tests `TakeResourceTest.*`; GET_RESOURCE, ADD_RESOURCE and REMOVE_RESOURCE work through `object_resources` (`src/CHLApi.cpp`, `src/ECS/ObjectResources.cpp`): every kind answers and gives its own resource, a villager given resources carries them, and a worship site, a building site, a dead tree, a rock, a big forest and a fish farm read and give theirs as the original's do |

The food and wood miracles are in `../miracles/`.
