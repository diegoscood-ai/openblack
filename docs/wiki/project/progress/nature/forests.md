# Forests

Groups of trees the land scripts set out as forests, and the big forest models. A forest grows new trees, gives its town
wood, has footpaths and is a place for creatures and animals.

**Progress: 10/14 done, 0 chosen, 1 partial, 0 inferred — 75%**

How the original does it, in our wiki: [Trees and forests](../../../world/trees.md).

## Forests of trees

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts make forests and put trees in them by number | done | CREATE_FOREST and CREATE_TREE (`src/LHScriptX/FeatureScriptCommands.cpp`) to `ecs::CreateForest` and `ResolveForestId` (`src/ECS/Trees.cpp`): an unknown number leaves the tree without a forest; the ids follow the original's counter (a forest made without one takes the counter's, a given id raises the counter only when it is below it), so two forests may share an id, each kept under its own key, and a tree joins the newest forest with its number (`ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat`); tests `ScenicForestTest.*`, `test/test_villager_forester.cpp` |
| Each turn a forest with full-grown trees may plant a young one: c x f x T / 300 > 2000 + rand(1000), with c its tries, f 0.05 per full-grown tree (at most 1) and T the turns since any forest last planted; a town's scenic forest plants none | done | `ProcessForests` (`src/ECS/Trees.cpp`), from the world turn (`magic::ProcessForests`); covered by the fidelity run's random trace (a draw a forest a turn, Land 1's forests); test `ForestTreeBuckets.*` (the pass order) |
| The young tree goes at the first free spot round one of the rand(n/2 + 1) full-grown trees nearest the forest's centre | done | `ProcessForests` (`ecs::ForestPlantsATree`, the parent from `ecs::ParentTreeDraws`) and `PlantTreeNear` (`ecs::FindTreeSpot`: 32 angles by 5 radii, the first free spot), the spots' angles a sixteenth of a half turn apart as the original (`ecs::k_TreeRingStep`); tests `TreeRings.TheRingsAreASixteenthOfAHalfTurnApart`, `TreeRings.TheSpotIsTheFirstFreeOfFiveRadiiARingAndThenTheNextRing`, `TreeRings.NoFreeSpotAfterThirtyTwoRingsOfFive`, `TreeGrowth.AForestPlantsWhenItsAttemptsOutgrowItsThreshold` |
| An empty forest (no trees, no big forest) waits 2000 turns and is deleted, and taken from its towns | done | `ProcessForests` and `DeleteForest` (`src/ECS/Trees.cpp`); test `test/test_scenic_forest.cpp`, `ScenicForestTest.AnEmptyForestIsDeletedOnItsTwoThousandthEmptyTurn` |
| A forest belongs to the towns near it; a town's foresters work it, and it is removed from them when it's gone | done | Tests `ScenicForestTest.*`, `test/test_villager_forester.cpp`, `ScaffoldWorld.AFoundedTownMeasuresItsForestsFromItsWoodStore` (`test/test_scaffold_world.cpp`); `MakeScenicForest` and `AssignForestsToTown` from `src/ECS/Town/TownFeatures.cpp` and `src/ECS/Scaffolds.cpp`; `DeleteForest` drops it from every town; the foresters (`src/ECS/Villager/VillagerForester.cpp`) |
| A forest's wood is the sum of its trees' | done | `ecs::ForestWood` (its big forest's and each tree's); test `test/test_villager_forester.cpp`; see [../resources/wood.md](../resources/wood.md) |
| Villagers' footpaths lead into forests | pending | see ../terrain/footpaths.md |
| Forests are places for the creature to go, play and vent its anger | pending | see ../creature/ |
| Wild animals choose lairs near forests | done | The predators' lairs from the land's forests (`src/ECS/AnimalLairs.cpp`); test `test/test_animal_system.cpp`, `AnimalSystemTest.PredatorsLairsFollowTheForestRule`; see ../animal/wild_animals.md |
| The hand over a forest shows help and makes its own sound | pending | see ../interface/ |

## Big forests

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place big forest models, turned and scaled | done | CREATE_BIG_FOREST and CREATE_NEW_BIG_FOREST (`src/LHScriptX/FeatureScriptCommands.cpp`) to `BigForestArchetype` (`src/ECS/Archetypes/BigForestArchetype.cpp`), with its own forest; tests `LandScriptMeshCommandsTest.CreateBigForestTakesItsAngleAndScaleAsTheyAre`, `LandScriptMeshCommandsTest.CreateNewBigForestHoldsItsRowsWoodAtItsScaleAndOwnsItsForest`, `LandScriptMeshCommandsRealDataTest.LandOnesScaledBigForestHoldsItsRowsWoodAtItsScale` (`test/test_land_script_mesh_commands.cpp`); how a big forest is lit is in [../rendering/models_and_lighting.md](../rendering/models_and_lighting.md) (drawn unlit) |
| The creature and villagers walk round big forests | partial 50% | The route planner takes a big forest as an obstacle (`src/ECS/RoutePlanWorld.cpp`); missing: the creature's walk is not checked |
| The hand grabbing a big forest is not tugged: it takes a conifer's wood (350) from the forest and holds a new conifer | done | `HandSystem::CreateTreeFromForest` (`HandTrees.cpp`, from `HandTurn.cpp`), as [the wiki](../../../hand/grab-hold-and-throw.md#pick-up-rules-and-bigforest); tests `HandInputWorld.ABigForestPutInTheHandGivesAConiferMadeWhereItStandsAndItsWood`, `HandInputWorld.ABigForestWithLessWoodThanATreeGoesAndStillGivesTheTree` (`test/hand/test_hand_input_world.cpp`) |
| A big forest gives wood, starting at its type's wood value times its scale | done | `ecs::BigForestRemoveWood`; test `TakeResourceTest.ABigForestAndAFishFarmGiveOnlyTheirOwnResource` |

The forest miracle's forests are in `../miracles/forest.md`.
