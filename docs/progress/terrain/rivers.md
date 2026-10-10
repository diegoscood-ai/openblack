# Rivers and lakes

Rivers are laid by the land's script as a chain of points. Along each stretch a channel cuts the land so the water
underneath shows, and a bed is blended into the land's colour. Lakes are low land below sea level that shows the same
water.

**Progress: 13/18 done, 4 partial — 83%**

## Laying the rivers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes each river and gives it its points in order | done | `FeatureScriptCommands::CreateStream`, `CreateStreamPoint`; `src/ECS/Components/Stream.h` |
| A river runs from each point to the next, never from the last back to the first | done | `Stream::points` in script order, `ecs::CreateRiverFootprints` (`src/ECS/Rivers.cpp`) |
| Each stretch has a channel mesh, turned and stretched to reach the next point | done | `ecs::CreateRiverFootprints` (turned to the segment, stretched by its length / 30) |
| The channel clears the land's alpha so the water drawn underneath shows as the river | done | `RenderPass::LandAlpha`, `assets/shaders/fs_land_alpha.sc`, `assets/shaders/fs_terrain.sc` |
| Each stretch has a bed mesh blended into the land's colour, as a building's footprint is | done | The river bed in the footprint pass (`src/Graphics/Renderer.cpp`) |
| Rivers look the same in the sea's reflection | partial | The land alpha is applied to the mirrored land; not checked against the game |
| Rivers are drawn for debugging as lines between their points | n/a | openblack-only (`RenderingSystemCommon::PrepareDraw`, its streams option) |
| Rivers are saved and loaded with the game, with their points | todo | See ../engine/ (saving) |
| The game can find the nearest point of a river to a position | done | `NearestStreamPos` and `FindNearestStreamPosTo` (`src/ECS/WaterQueries.cpp`), used by the search for drinking water |

## Water in rivers and lakes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The water in a river or lake is the sea, with its rows, ripple and colour, seen through the land | done | see ../ocean/sea_surface.md |
| Lakes are land below sea level, drawn flat with the water showing | done | `LandIslandInterface::GetDrawnHeightAt`, the coast alpha (`src/3D/CoastAlpha.cpp`) |
| Running water is heard near rivers | done | Cell ambient sound type 9 (`src/Audio/Services/SoundMap.cpp`); see ../audio/ |
| Still fresh water is heard near lakes | done | Cell ambient sound type 2 (`src/Audio/Services/SoundMap.cpp`) |
| Fires near or in water go out; a fireball over water steams | done | `src/ECS/Fire/FireEffect.cpp` (water and coast cells), `src/ECS/Fire/FireGraphic.cpp`; see ../physics/ |
| Trees and forests don't grow in water | partial | The forest's new trees take any free point (`src/ECS/Trees.cpp`). Our wiki differs: read literally, the original counts a water cell as free for a new tree ([trees.md](../../bw1-notes/trees.md#growth-treeprocess-0x74a290-treegrow-0x74a3f0)) |
| A creature wades in shallow water and avoids deep water | done | `CreatureLocomotionSystem.cpp`; see ../creature/ |
| Villagers and creatures can drown or swim in deep water | partial | Villagers drown (`src/ECS/VillagerDrowning.cpp`); creatures do not. See ../villager/ and ../physics/ |
| Waterfalls pour with spray and sound where the land's script places them | done | `FeatureScriptCommands::CreateWaterfall` makes nothing, and the Land 3 waterfall is `src/ECS/DesignedScenery.cpp`. Our wiki differs: the script's waterfall object is empty in the original, it neither draws nor sounds ([water.md](../../bw1-notes/water.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)) |
| Splashes and rings on rivers and lakes where things fall in | partial | see ../ocean/things_on_the_water.md |
