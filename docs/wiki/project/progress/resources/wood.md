# Wood

What the towns build and repair with. Foresters fell trees and carry the logs to the store; the player can pull up trees
or pour the wood miracle; builders take wood from the store to building sites and the workshop.

**Progress: 10/11 done, 0 chosen, 1 partial, 0 inferred — 95%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## Where wood comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters fell trees of the town's forests and carry the logs to the store | done | `FORESTER_GOTO_FOREST`, `FORESTER_CHOPS_TREE` (`src/ECS/Villager/VillagerForester.cpp`); test `test/test_villager_forester.cpp`; see `../villager/` |
| A tree gives wood by its kind and size | done | Test `StoreRules.ATreesWoodScalesOnceWithItsSizeAndWithItsLife`; `ecs::TreeWoodValue` (`src/ECS/Trees.cpp`): the info table's wood value by the tree's life and scale |
| A tree a forest miracle grew gives more wood, by its caster's tribal power | done | Test `TakeResourceTest.AForestMiraclesTreeGivesWoodByItsCastersTribalPower` (`test/test_take_resource.cpp`: 0.25 x the tribal power times an ordinary tree's wood, kept by the dead tree it becomes, as [the forest miracle](../../../magic/forest.md)); the multiplier kept on the tree (`magic_tree`, `Tree::woodValueMultiplier`) is read by `ecs::TreeWoodValue`, so felling it gives more |
| A big forest's wood goes down by what is taken, and with too little left the forest is deleted | done | `ecs::BigForestRemoveWood` (`src/ECS/Trees.cpp`), as [the wiki](../../../hand/grab-hold-and-throw.md#pick-up-rules-and-bigforest); test `TakeResourceTest.ABigForestAndAFishFarmGiveOnlyTheirOwnResource` |
| Uprooted (dead) trees and felled trunks count as wood | done | Tests `StoreRules.ADeadTreesWoodIgnoresItsLife`, `TakeResourceTest.ADeadTreeGivesItsWoodAndGoesWhenAskedForAnythingElse`; a dead tree's wood by its kind and scale (`ecs::TreeWood`, `src/ECS/Trees.cpp`) |

## Storing and using wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Builders carry wood from the store to building sites and scaffolds | done | Test `VillagerResourcesTest.AtStructureAddResource`; `src/ECS/Villager/VillagerBuild.cpp`; see `../building/` |
| The workshop takes wood to make scaffolds | done | Tests `ResourceStoreTable.AWorkshopTakesWoodAndAny`, `ResourceStoresTest.AWorkshopsPileTakesWoodThroughItsWorkshop`; the workshop takes its wood value per scaffold and makes one in a free slot (`src/ECS/Town/Workshops.cpp`); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Damaged buildings are repaired with wood | done | Scene `physics.building_repaired`; the repair path of `src/ECS/Villager/VillagerBuild.cpp`; test `BuildingSitesTest.ABuiltBuildingNeedsItsMissingLifeOfItsWoodValueLessTheWoodAtItsSite` |
| Wood poured near a workshop, building site or scaffold goes to it | done | Tests `ResourceStoresTest.ABuildingTakesWoodWhileItHasABuildingSite`, `AScaffoldTakesWoodWhileItBelongsToABuildingSite`; `pot_resource::AddResourceToPos` offers it to every store in reach, workshops and building sites included (`src/ECS/PotResource.cpp`, `src/ECS/ResourceStores.cpp`) |
| A town short of wood raises its wood desire flag | done | Test `TownDesireTest.Wood` (`test/test_town_desire.cpp`: the wood desire from the store's wood, k = max(abodes / 10, 1), 500 k and 5000 k, and the LowOnWood warning, as [the desire functions](../../../beings/villager-decisions.md)); the flag flies by that desire (`TownDesireFlags.AShownFlagEasesUpToItsDesireNeverPastWhole`, `AFlagRisesFourMetresAndTwoFifthsWhenWhollyWanted`); the town's wood desire reads its store (`src/ECS/TownDesire.cpp`); see `../town/` |
| The creature carries wood and builds with it | partial 50% | Scenes `objects.give_wood`, `objects.give_wood_site`, `objects.repair_house`; tests `CreatureKindActions.GivingWoodPullsUpTheTreeAndThrowsItIntoTheStore`, `CreatureKindActions.GivingWoodToABuildingSiteIsGivingWoodToAStore`; missing: not checked against v1.42; see `../creature/` |

The store's starting wood and the scripts reading, adding and taking wood are in [food](food.md) (one row each for
food and wood); the store's five wood piles in [stores and piles](stores_and_piles.md); a tree pulled up by the hand,
giving wood to a town and the creature copying it in [handling](resource_handling.md).

The food and wood miracles are in `../miracles/`.
