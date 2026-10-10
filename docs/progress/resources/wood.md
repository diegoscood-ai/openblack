# Wood

What the towns build and repair with. Foresters fell trees and carry the logs to the store; the player can pull up trees
or pour the wood miracle; builders take wood from the store to building sites and the workshop.

**Progress: 13/17 done, 2 partial — 82%**

How the original does it, in our wiki: [Objects and resources](../../bw1-notes/objects-and-resources.md).

## Where wood comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Foresters fell trees of the town's forests and carry the logs to the store | done | `FORESTER_GOTO_FOREST`, `FORESTER_CHOPS_TREE` (`src/ECS/Villager/VillagerForester.cpp`); test `test/test_villager_forester.cpp`; see `../villager/` |
| A tree gives wood by its kind and size | done | `ecs::TreeWoodValue` (`src/ECS/Trees.cpp`): the info table's wood value by the tree's life and scale |
| A tree a forest miracle grew gives more wood, by its caster's tribal power | done | the multiplier kept on the tree (`magic_tree`, `Tree::woodValueMultiplier`) is read by `ecs::TreeWoodValue`, so felling it gives more |
| Big forests give wood without being used up (unconfirmed) | done | `ecs::BigForestRemoveWood` (`src/ECS/Trees.cpp`). Our wiki differs: a big forest's wood goes down by what is taken, and with too little left the forest is deleted ([trees](../../bw1-notes/trees.md#pick-up-rules-and-bigforest)) |
| Uprooted (dead) trees and felled trunks count as wood | done | a dead tree's wood by its kind and scale (`ecs::TreeWood`, `src/ECS/Trees.cpp`) |
| A tree pulled up by the hand is held as branches or logs by its kind (evergreen, fruit, hardwood) | todo | the hand holds the uprooted tree itself (`HandHolding.cpp`); see [handling](resource_handling.md) |

## Storing and using wood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting wood | done | `AbodeArchetype` (CREATE_ABODE's wood amount) |
| The store's wood shows as five piles that fill in turn | done | see [stores and piles](stores_and_piles.md) |
| Builders carry wood from the store to building sites and scaffolds | done | `src/ECS/Villager/VillagerBuild.cpp`; see `../building/` |
| The workshop takes wood to make scaffolds | done | the workshop takes its wood value per scaffold and makes one in a free slot (`src/ECS/Town/Workshops.cpp`); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Damaged buildings are repaired with wood | done | the repair path of `src/ECS/Villager/VillagerBuild.cpp` |
| Wood poured near a workshop, building site or scaffold goes to it | done | `pot_resource::AddResourceToPos` offers it to every store in reach, workshops and building sites included (`src/ECS/PotResource.cpp`, `src/ECS/ResourceStores.cpp`) |
| A town short of wood raises its wood desire flag | done | the town's wood desire reads its store (`src/ECS/Town/TownDesire.cpp`); see `../town/` |
| Giving wood to a town that wants it impresses the town | done | `DoResourceAdding` (`src/ECS/ObjectResources.cpp`) |
| Wood poured on a store or by a building site teaches the creature to do the same | partial | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`) but not passed on to the creature |
| The creature carries wood and builds with it | todo | see `../creature/` |
| Scripts read, add and take a town's wood | partial | ADD_RESOURCE works (`src/CHLApi.cpp`); GET_RESOURCE and REMOVE_RESOURCE are stubs |

The food and wood miracles are in `../miracles/`.
