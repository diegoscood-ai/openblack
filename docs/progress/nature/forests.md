# Forests

Groups of trees the land scripts set out as forests, and the big forest models. A forest grows new trees, gives its town
wood, has footpaths and is a place for creatures and animals.

**Progress: 10/16 done, 1 partial — 66%**

How the original does it, in our wiki: [Trees and forests](../../bw1-notes/trees.md).

## Forests of trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make forests and put trees in them by number | done | CREATE_FOREST and CREATE_TREE (`src/LHScriptX/FeatureScriptCommands.cpp`) to `ecs::CreateForest` and `ResolveForestId` (`src/ECS/Trees.cpp`): an unknown number leaves the tree without a forest |
| A forest grows new young trees near its trees over time | done | `ProcessForests` and `PlantTreeNear` (`src/ECS/Trees.cpp`), from the world turn (`magic::ProcessForests`) |
| A forest gains a tree no sooner than so many turns after the last any forest gained (41), at the first free spot round the tree | done | `ProcessForests` (the turns since the last tree any forest planted) and `PlantTreeNear` (32 angles by 5 radii, the first free spot) |
| A forest's trees die back now and then (unconfirmed when) | todo | Not found in our tree (`ShrinkAllTrees` exists for the miracles); not confirmed in the game |
| A forest belongs to the towns near it; a town's foresters work it, and it is removed from them when it's gone | done | `MakeScenicForest` and `AssignForestsToTown` from `src/ECS/Town/TownFeatures.cpp` and `src/ECS/Scaffolds.cpp`; `DeleteForest` drops it from every town; the foresters (`src/ECS/Villager/VillagerForester.cpp`) |
| A forest's wood is the sum of its trees' | done | `ecs::ForestWood` (its big forest's and each tree's); see [../resources/wood.md](../resources/wood.md) |
| Villagers' footpaths lead into forests | todo | see ../terrain/footpaths.md |
| Forests are places for the creature to go, play and vent its anger | todo | see ../creature/ |
| Wild animals choose lairs near forests | done | The predators' lairs from the land's forests (`src/ECS/AnimalLairs.cpp`); see ../animal/wild_animals.md |
| The hand over a forest shows help and makes its own sound | todo | see ../interface/ |
| Scripts read and change forests | todo | No forest functions in `src/CHLApi.cpp` |

## Big forests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place big forest models, turned and scaled | done | `BigForestArchetype` (`src/ECS/Archetypes/BigForestArchetype.cpp`), with its own forest |
| Big forests are lit as the game lights them | done | Drawn as the other models |
| The creature and villagers walk round big forests | partial | The route planner takes a big forest as an obstacle (`src/ECS/RoutePlanWorld.cpp`); the creature's walk is not checked |
| The hand tugging a big forest pulls a tree out of it | done | `HandSystem::TakeTreeFromForest` (`HandTrees.cpp`). Our wiki differs: a big forest is not tugged; grabbing it takes 350 of its wood and puts a new conifer in the hand ([trees.md](../../bw1-notes/trees.md#pick-up-rules-and-bigforest)) |
| A big forest gives wood (unconfirmed how much) | done | `ecs::BigForestRemoveWood` (its wood starts at its type's wood value times its scale) |

The forest miracle's forests are in `../miracles/forest.md`.
