# Footpaths and roads

Footpaths are invisible routes the land's script lays between buildings and places. Villagers walk them to get about a
town and between towns, and they bend round obstacles that block them. The worn paths and roads the player sees are
painted into the land's texture and the buildings' footprints.

**Progress: 11/14 done, 0 chosen, 3 partial, 0 inferred — 94%**

## Laying footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes footpaths and gives them their nodes in order | done | `FeatureScriptCommands::CreateFootpath`, `CreateFootpathNode` (`src/LHScriptX/FeatureScriptCommands.cpp`), `ecs::footpaths` (`src/ECS/Footpaths.cpp`) |
| The land's script links footpaths to the building they serve | done | `FeatureScriptCommands::LinkFootpath` gives the footpath to the last created multi-map fixed object, which every creation command now sets (`src/LHScriptX/FeatureScriptCommands.cpp`; tests `FootpathsTest.LinkFootpath*`); a forest is not an entity in openblack, so a link after CREATE_FOREST links nothing |
| A building keeps the footpaths that lead from it and picks the nearest one towards a destination | partial 85% | The link's nearest-path choice is ported (`src/ECS/Footpaths.cpp`; test `FootpathsTest.LinkPicksTheNearestFootpathAndItsDirection`) and a new abode gets its footpath to the storage pit (`footpaths::MakeAbodeFootpath` from `src/ECS/Abodes.cpp`); the link follows its building from plan to building and back, and goes with it (`ECS/Town/BuildingSites.cpp`, `ECS/Abodes.cpp`); a worship site gets its link when a town joins it (`site::AddTown`); the walk to a building asks for it (`villager::SetupMoveToOnFootpath`). Missing: the walks that do not ask for it yet (the walk row below) |
| A building or other fixed object that is deleted takes its footpath link with it, unless the rebuild plan made from it keeps the link; its footpaths stay | done | an abode's, worship site's or citadel heart's in `MultiMapFixedToBeDeleted` (`src/ECS/Abodes.cpp`), any other multi-map fixed object's in `ecs::ToBeDeleted` (`footpaths::DeleteFootpathLinkOf`); the rebuild plan takes it in `plans::CreateFromBuilding` (test `FootpathsTest.ALinkHandedOnOrDeletedLeavesItsFootpaths`); a dance's and a forest's links are kept, as the original keeps them ([routes.md](../../../beings/routes.md)) |
| A new dance (all but the worship ones) gets a footpath from the storage pit of the town it is made for, or of the town nearest it | done | `DanceSystem::Create` (`dance_rules::LaysTownFootpath`, tests in `test/test_dances.cpp`; its end is the walks' arrive point for a dance, `living_footpath::ArrivePosOf`); the scripts', the creatures' and the artefacts' dances go through it ([dances.md](../../../beings/dances.md#making-a-dance)) |
| Footpaths are saved and loaded with the game | partial 40% | Read from the land's footpath file (`src/Serializer/FotFile.cpp`, `GameThingSerializer.cpp`, `footpaths::AttachLoadedLink`; test `FootpathsTest.LoadedLinkGoesToAPlanOrIsDeleted`). Waits for the saved games (#186 reading them, #187 the writers): the saved game's footpaths, footpath finders and footpath walks (`GFootpath::Save`/`Load` 0x535E00/0x535F10, `GFootpathFinder::Save`/`Load` 0x539150/0x5390E0) and the land-script writer (`GFootpath::SaveAll` 0x535550); see ../engine/ |

## Walking the footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager going somewhere joins the nearest footpath when it helps, walks it node to node, and leaves it at the nearest node to the destination | partial 85% | `villager::SetupMoveToOnFootpath` → `footpaths::UseFootpathIfNecessary` → MOVE_ON_PATH 29 (tests `UseFootpathTest.*`; testbed `villagers.footpaths`, `villagers.script_footpaths`). Of the exe's 37 call sites of `Living::SetupMoveToOnFootpath` 0x5EDD20, 31 are ported and take the footpaths (the dance, the creche, the building supplies and site, the workshop's four, the farmer, two fisherman walks, the wood, home, the hiding and fleeing four, the storage pit's four, the breeder, the worship supplies, the altar rest, and the worship walks to the site's gate and to its door, `VillagerWorship.cpp`, tests `WorshipperTest.*`; [routes.md](../../../beings/routes.md#pending) W4). Missing (issue #74): six sit in states not ported yet: the housewife's four (`HousewifeGotoStoragePit` 0x761D3A, `HousewifeArrivesAtStoragePit` 0x761E4D, `HousewifePickupFromStoragePit` 0x761EDC, `HousewifeGossipsAroundStoragePit` 0x76215F; their states are empty rows of the state table), `VillagerBecomesFisherman()` 0x75B540 (the overload without a farm) and `AddMoveToPosWithFootpathReaction` 0x5F1067 (the town celebration's dance, state 203 `DANCE_WHILE_REACTING`, not ported); see ../villager/ |
| Footpaths can be walked either way | done | The direction is kept (`LivingFootpath::reverse`) and walked by MOVE_ON_PATH; a teleport stone takes the footpath's end in that direction as the villager's destination (`villager_teleport::FinalDestination`) |
| Each node keeps the villagers following it, so they move in file | done | `footpaths::AddOccupant` and `RemoveOccupant` (test `FootpathsTest.OccupantsHeadFirst`), used by the walk on a footpath |
| When something blocks a footpath (a dropped rock, a building), the path is sent round it and returns when it is cleared | done | `footpaths::RerouteFootpathsAroundObstacle` and `StopReroutingAroundObstacle`, called when an obstacle enters or leaves the map cells (`src/ECS/MapCells.cpp`); tests `FootpathObstaclesTest.*` |
| New footpaths are made from the routes a creature or villager plans, over a few game turns | done | `footpaths::GenerateRoutesBetween` → `ConvertCreaturePlanToFootpath` for a new abode, a dance and an artefact, and `AttemptRerender` when a path is rerouted, and the footpath finder a worship site starts for each town it serves, which plans over several turns (`GFootpathFinder` 0x538EC0 from `WorshipSite::AddTown` 0x77C8B5: `footpaths::StartFootpathFinder` / `ProcessFootpathFindersTurn`) (tests `FootpathRoutesTest.*`). The eight route calls of `sub_535CD0` are never reached: its only way in is an unreferenced key handler (`sub_641570`) ([routes.md](../../../beings/routes.md)) |
| Hidden nodes can be skipped as a short cut | done | The node walk steps over hidden nodes (`src/ECS/Footpaths.cpp`; test `FootpathsTest.NextNodeStepsOverHiddenNodes`), on the villagers' footpath walks |

## Roads seen on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worn paths and roads are part of the land's painted texture | done | `src/3D/BlockTexture.cpp` paints whatever the land's materials hold |
| Buildings and civic pieces print their paved ground onto the land | done | footprints, see land_marks.md |
