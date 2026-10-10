# Footpaths and roads

Footpaths are invisible routes the land's script lays between buildings and places. Villagers walk them to get about a
town and between towns, and they bend round obstacles that block them. The worn paths and roads the player sees are
painted into the land's texture and the buildings' footprints.

**Progress: 4/12 done, 7 partial — 62%**

## Laying footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes footpaths and gives them their nodes in order | done | `FeatureScriptCommands::CreateFootpath`, `CreateFootpathNode` (`src/LHScriptX/FeatureScriptCommands.cpp`), `ecs::footpaths` (`src/ECS/Footpaths.cpp`) |
| The land's script links footpaths to the building they serve | todo | `FeatureScriptCommands::LinkFootpath` logs not implemented |
| A building keeps the footpaths that lead from it and picks the nearest one towards a destination | partial | The link's nearest-path choice is ported (`src/ECS/Footpaths.cpp`; test `FootpathsTest.LinkPicksTheNearestFootpathAndItsDirection`) and a new abode gets its footpath to the storage pit (`footpaths::MakeAbodeFootpath` from `src/ECS/Abodes.cpp`); no villager asks for it yet |
| Footpaths are saved and loaded with the game | partial | Read from the land's files (`src/Serializer/GameThingSerializer.cpp`, `footpaths::AttachLoadedLink`; test `FootpathsTest.LoadedLinkGoesToAPlanOrIsDeleted`); saving todo, see ../engine/ |
| Footpaths are drawn for debugging as lines | n/a | openblack-only (`RenderingSystemCommon.cpp`, the debug window's footpaths option) |

## Walking the footpaths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager going somewhere joins the nearest footpath when it helps, walks it node to node, and leaves it at the nearest node to the destination | partial | `footpaths::UseFootpathIfNecessary` and `living_footpath::SetupMoveOnFootpath` are ported (`src/ECS/LivingFootpath.cpp`; tests `UseFootpathTest.*`), but nothing calls them and the walking state is not ported (`LivingActionSystem.cpp`); see ../villager/ |
| Footpaths can be walked either way | partial | The direction is kept (`LivingFootpath::reverse`), but no villager walks a footpath yet |
| Each node keeps the villagers following it, so they move in file | partial | `footpaths::AddOccupant` and `RemoveOccupant` (test `FootpathsTest.OccupantsHeadFirst`); dormant while nobody walks a footpath |
| When something blocks a footpath (a dropped rock, a building), the path is sent round it and returns when it is cleared | done | `footpaths::RerouteFootpathsAroundObstacle` and `StopReroutingAroundObstacle`, called when an obstacle enters or leaves the map cells (`src/ECS/MapCells.cpp`); tests `FootpathObstaclesTest.*` |
| New footpaths are made from the routes a creature or villager plans, over a few game turns | partial | A new abode's footpath is planned (`footpaths::GenerateRoutesBetween`); `ConvertCreaturePlanToFootpath` is ported (tests `FootpathRoutesTest.*`) but has no caller |
| Hidden nodes can be skipped as a short cut | partial | The node walk steps over hidden nodes (`src/ECS/Footpaths.cpp`; test `FootpathsTest.NextNodeStepsOverHiddenNodes`); dormant while nobody walks a footpath |

## Roads seen on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Worn paths and roads are part of the land's painted texture | done | `src/3D/BlockTexture.cpp` paints whatever the land's materials hold |
| Buildings and civic pieces print their paved ground onto the land | done | footprints, see land_marks.md |
