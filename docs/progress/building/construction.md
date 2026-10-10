# Construction

Towns grow by building. A town keeps plans of buildings it means to put up; a building site opens on a plan (or where
the player drops a scaffold), builders bring wood and build it up stroke by stroke, the scaffold rising round the
growing building until it is finished and becomes functional.

**Progress: 16/19 done, 2 partial — 89%**

How the original does it, in our wiki: [Buildings, building sites and towns (the building side)](../../bw1-notes/buildings.md).

## Plans and sites

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts place planned abodes and planned citadels a town will build | done | CREATE_PLANNED_ABODE (`src/LHScriptX/FeatureScriptCommands.cpp`, `ecs::plans`) and the planned citadels (`src/ECS/Archetypes/CitadelArchetype.cpp`) |
| A plan shows as a ghostly outline of the building on the ground | done | plans are kept in the town's list and never drawn (`ecs::plans`, `src/ECS/Town/BuildingSites.cpp`). Our wiki differs: a plan is not drawn at all, so there is no outline ([buildings](../../bw1-notes/buildings.md#plans-and-building-sites)) |
| A town picks the best plan for what it wants and opens a building site on it | done | `building_sites::RequestBestPlanned` and `RequestANewAbode` with `GetDesireToBeBuilt` (`src/ECS/Town/BuildingSites.cpp`), asked by the builders' desires |
| A building site clears its ground and checks nothing fixed is in the way | done | the fixed check before a plan is built (`plans::CreatePlanned`, `town_placement::IsSuitableForFixedAbodeInTown`, `src/ECS/Town/TownPlacement.cpp`); `test/test_town_placement_cells.cpp` |
| A site needs a number of builders, set by its size | done | the info's builders needed (`building_sites::GetBuildersNeeded`, `src/ECS/Town/BuildingSites.cpp`) |
| A site needs a set amount of wood, a pile of wood beside it filling as builders bring it | done | the wood needed from the info's wood value and the site's own pile (`src/ECS/Town/BuildingSites.cpp`); `test/test_building_sites.cpp` |
| Builders stand round the site at spaced places and build | done | the builders' ring round the mesh (`PosBuilderProcess`, `src/ECS/Town/BuildingSites.cpp`; `GotoBuildingSite`, `src/ECS/Villager/VillagerBuild.cpp`); See ../villager/jobs.md |
| Each stroke uses wood and raises the building's percentage built | done | each building cycle takes wood from the pile and builds by it (`BuildBy`, `src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Abodes.cpp`); `test/test_villager_build.cpp` |
| The building shows partly built: the model cut at the height reached, with a scaffold round it | done | the partial model cut at the percent built, with the scaffold rising and cut (`src/ECS/Physics/PartialBuild.cpp`) |
| A finished building is built, functional, and villagers react to it | partial | `abodes::Built` deletes the site and the building becomes functional (`src/ECS/Abodes.cpp`); the new building reaction is not made yet (only logged) |
| A building that's had its site removed or its town lost stops being built; its builders leave | done | a deleted site sends its builders back to deciding and releases its pile (`building_sites::ToBeDeleted`, `src/ECS/Town/BuildingSites.cpp`) |
| Building sites with no use are pruned each turn | done | `building_sites::PruneSites` each town turn (`src/ECS/Town/TownProcess.cpp`) |
| A script forces a planned building to be built at a place | done | BUILD_BUILDING (`src/CHLApi.cpp`) through `building_sites::ForceBuildingOfPlannedAtPos` |
| A town's citadel building site (when the citadel is planned) | partial | the citadel plan and its six-pile site (`src/ECS/Archetypes/CitadelArchetype.cpp`, `src/ECS/Town/BuildingSites.cpp`); See ../temple/ |

## Wood for building

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Builders decide whether to fetch wood from the storage pit, a forest or a pot | done | `DecideHowToGetWood`: the store, a forest or the town's wood pot (`src/ECS/Villager/VillagerBuild.cpp`, `src/ECS/Villager/VillagerForester.cpp`) |
| The town counts wood at its building sites in its stores | done | the wood at the sites counts in the town's stats (`town_stats::Compute`, `src/ECS/Town/TownStats.cpp`) |
| Wood dropped by hand on a site is added to its pile | done | a building with a site takes held wood into its pile (`resource_stores::IsResourceStore`, `src/ECS/ResourceStores.cpp`); See ../resources/ |
| The player's scaffolds count as most of a building's wood | todo | a scaffold joins its building's site (`building_sites::AddScaffold`, `src/ECS/Scaffolds.cpp`); no wood credit for it was found; See workshop_and_scaffolds.md |
| Wood the town uses for building is taken from its store | done | builders take the wood from the storage pit (`ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS`, `src/ECS/Villager/VillagerBuild.cpp`); See ../town/storehouse.md |
