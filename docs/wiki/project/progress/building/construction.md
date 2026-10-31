# Construction

Towns grow by building. A town keeps plans of buildings it means to put up; a building site opens on a plan (or where
the player drops a scaffold), builders bring wood and build it up stroke by stroke, the scaffold rising round the
growing building until it is finished and becomes functional.

**Progress: 13/17 done, 0 chosen, 3 partial, 1 inferred — 91%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../../world/building-sites.md).

## Plans and sites

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place planned abodes and planned citadels a town will build | done | CREATE_PLANNED_ABODE (`src/LHScriptX/FeatureScriptCommands.cpp`, `ecs::plans`) and the planned citadels (`src/ECS/Archetypes/CitadelArchetype.cpp`); `test/test_citadel_plan.cpp` (`PlanIsOnlyAPlan`), the fidelity run (Land 1's script has six planned abodes) |
| A town picks the best plan for what it wants and opens a building site on it | done | `building_sites::RequestBestPlanned` and `RequestANewAbode` with `GetDesireToBeBuilt` (`src/ECS/Town/BuildingSites.cpp`), asked by the builders' desires; the desire is tested (`test/test_building_sites.cpp`), and so is the choice: the civic mask 4 and the living quarters' 2 (0x73A652 / 0x73B332), the most wanted plan strictly, the first on ties (`GetBestPlanned` 0x73A140; [the desire to be built](../../../world/building-sites.md#the-desire-to-be-built)); tests `BuildingSitesTest.ATownAsksForTheMostWantedPlanOfTheKindItBuilds`, `PlansListAndRepairDesire` (`test/test_building_sites.cpp`) |
| A building site clears its ground and checks nothing fixed is in the way | partial 75% | the fixed check before a plan is built (`plans::CreatePlanned`, `town_placement::IsSuitableForFixedAbodeInTown`, `src/ECS/Town/TownPlacement.cpp`); `test/test_town_placement_cells.cpp`. The builders' push of things off the site's clear area finds nothing, as no object type is pushable (`CheckForClearArea`, inferred equivalent, `src/ECS/Villager/VillagerBuild.cpp`); ARRIVE_AT_PUSH_OBJECT is never entered and not ported |
| A site needs a number of builders, set by its size | done | the info's builders needed (`building_sites::GetBuildersNeeded`, `src/ECS/Town/BuildingSites.cpp`); `test/test_building_sites.cpp` (`BuildersNeeded`) |
| A site needs a set amount of wood, a pile of wood beside it filling as builders bring it | done | the wood needed from the info's wood value and the site's own pile (`src/ECS/Town/BuildingSites.cpp`); `test/test_building_sites.cpp` |
| Builders stand round the site at spaced places and build | done | the builders' ring round the mesh (`PosBuilderProcess`, `src/ECS/Town/BuildingSites.cpp`; `GotoBuildingSite`, `src/ECS/Villager/VillagerBuild.cpp`); `test/test_villager_build.cpp` (`RingAndWoodSource`, `GotoBuildingSite`); See ../villager/jobs.md |
| Each stroke uses wood and raises the building's percentage built | done | each building cycle takes wood from the pile and builds by it (`BuildBy`, `src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Abodes.cpp`); `test/test_villager_build.cpp`, `test/test_building_sites.cpp` (`BuildByUntilBuilt`) |
| The building shows partly built: the model cut at the height reached, with a scaffold round it | done | the partial model cut at the percent built, with the scaffold rising and cut (`src/ECS/Physics/PartialBuild.cpp`); `test/test_partial_build_cap.cpp`, `test/test_building_sites.cpp` (`PercentForDrawBuilding`) |
| A finished building is built, functional, and villagers react to it | partial 95% | `abodes::Built` deletes the site, makes a civic building's "new building" reaction (15) for its town's player, counts the building in the player's statistics and makes it functional, with the footpath to the storage pit (`src/ECS/Abodes.cpp`); `test/test_building_sites.cpp`; the reaction's villagers: tests `NewBuildingReactionTest.*` (`test/test_villager_reactions.cpp`), [villager reactions](../../../beings/villager-reactions.md#a-new-building-reaction-15). Missing: a test of Built's own choice of buildings |
| A building that's had its site removed or its town lost stops being built; its builders leave | done | a deleted site sends its builders back to deciding and releases its pile (`building_sites::ToBeDeleted`, `src/ECS/Town/BuildingSites.cpp`); `test/test_building_sites.cpp`, `test/test_villager_build.cpp` (`BuildingRelease`) |
| Building sites with no use are pruned each turn | done | `building_sites::PruneSites` each town turn (`src/ECS/Town/TownProcess.cpp`); `test/test_building_sites.cpp` (`PruneBuiltAndRepaired`) |
| A script forces a planned building to be built at a place | done | BUILD_BUILDING (`src/CHLApi.cpp`) through `building_sites::ForceBuildingOfPlannedAtPos`; `test/test_citadel_plan.cpp` (`BuildBuildingConvertsAtOnce`); test `CitadelPlanTest.ConversionMakesTheCitadelBuildingSite` |
| A town's citadel building site (when the citadel is planned) | partial 80% | the citadel plan and its six-pile site (`src/ECS/Archetypes/CitadelArchetype.cpp`, `src/ECS/Town/BuildingSites.cpp`); `test/test_citadel_plan.cpp`. Not checked: that Land 1's builders build it as the original; See ../temple/ |

## Wood for building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Builders decide whether to fetch wood from the storage pit, a forest or a pot | done | `DecideHowToGetWood`: the store, a forest or the town's wood pot (`src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Villager/VillagerForester.cpp`); `test/test_villager_forester.cpp` (`DecideHowToGetWood`) |
| The town counts wood at its building sites in its stores | done | the wood at the sites counts in the town's stats (`town_stats::Compute`, `src/ECS/Town/TownStats.cpp`); `test/test_building_sites.cpp` (`TheWoodDesireReadsTheWoodAtTheSitesAsItStands`) |
| Wood dropped by hand on a site is added to its pile | done | a building with a site takes held wood into its pile (`resource_stores::IsResourceStore`, `src/ECS/ResourceStores.cpp`); `test/test_resource_stores.cpp`; See ../resources/ |
| The player's scaffolds count as most of a building's wood | inferred | not seen in the exe: the wood a site still needs (`BuildingSite::GetWoodNeededToBuild` 0x43C5F0) and a scaffold joining a site (`Scaffold::TryToBuildPlannedBuilding` 0x6E9080) have no scaffold credit; a scaffold is worth its value x 2500 wood as a resource (`Scaffold::GetDefaultResource` 0x6E9D30), whether a site counts that is not read. A scaffold joins its building's site (`building_sites::AddScaffold`, `src/ECS/Scaffolds.cpp`); See workshop_and_scaffolds.md |
