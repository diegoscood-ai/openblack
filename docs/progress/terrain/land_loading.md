# Land loading and the map

Each land is a landscape file of up to 255 blocks of 16 by 16 cells laid on a 32 by 32 block map, opened by the land's
script, with a fixed-point grid of map positions that everything in the game stands on.

**Progress: 15/23 done, 6 partial — 78%**

How the original does it, in our wiki: [Map loading and script functions](../../bw1-notes/map-loading.md).

## Opening a land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script names the landscape file to open, and the land is built from it | done | LOAD_LANDSCAPE (`src/LHScriptX/FeatureScriptCommands.cpp`), `Game::LoadLandscape` (`src/Game.cpp`) |
| A landscape file holds the blocks, which block of the map each one fills, the countries, the material textures, the noise and bump maps | done | `components/lnd` (`LNDFile`), `src/3D/Implementations/LandIsland.cpp` |
| Map cells hold an altitude, a colour, a luminosity, a country, water, coast and open-water bits, and an ambient sound type | done | `components/lnd/include/LNDFile.h` (`LNDCell`) |
| Map squares with no block are open sea | done | `LandIslandInterface::HasBlockAt` and the sea cells (`src/ECS/SeaCells.cpp`) |
| The five story lands and the playground lands all open | done | `Game::LoadMap` (`src/Game.cpp`) |
| Opening a new land clears the old land's weather, rings, footprints, snow and clouds | partial | `Game::LoadMap` clears the weather (`magic::OnLoadMap`), the particles, the ground marks, the night lights and the clouds' layout; our tree has no lying snow to clear |
| Blocks are built once into one shared vertex buffer and a physics shape | done | `src/3D/LandBlock.cpp` (the shape is for the Bullet ray casts) |
| Far blocks get a coarser mesh and lower-resolution texture, and blocks off screen are skipped | partial | Every block is drawn at full detail with no culling (`src/3D/LandBlock.cpp`) |
| Block textures are painted in the background while the game plays | partial | All are painted as the land opens (`src/3D/BlockTexture.cpp`): the same picture, a different timing |
| The land can be saved back to a landscape file (the game's own editor) | todo | No landscape writer in our tree; see ../debug/ |
| Multiplayer and online lands are downloaded and opened like local ones | todo | See ../multiplayer/ |

## Map positions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Positions are 16.16 fixed point, a 10 m cell per whole unit, with an altitude above the ground | done | `src/3D/MapCoords.h`; tests `MapCoords.*` (`test/test_map_coords.cpp`) |
| Metres convert to map positions and back with the game's single-precision rounding, losing a unit on some round trips | done | `map_coords::ToFixed`, `ToMetres`; tests `MapCoords.ToFixedTruncatesTheFloatProduct`, `ToMetresRoundsOnce` |
| The map is 512 by 512 cells; positions off it are out of bounds | done | `map_coords::InBounds`; test `MapCoords.CellsAndInBounds` |
| Searches walk a square spiral of cells out from the centre | done | `map_coords::Spiral`; tests `MapCoords.SpiralSequence`, `SpiralIncrementAndSizes` |
| Each map cell keeps the objects standing in it, buildings first, so searches find things near a point | done | `src/ECS/MapCells.cpp`; tests `MapCells.FixedListHeadForFixedTailForObjects` and the rest of `test/test_map_cells.cpp` |
| Buildings mark the cells their outline covers | done | `src/ECS/MapCells.cpp`; test `MapCells.MultiCellObjectInEveryCellOfItsShape` |
| A point on the screen is turned into a point on the land (for the hand and the camera) | partial | The mouse ray is a Bullet ray cast against the land blocks (`DynamicsSystem::RayCastClosestHit` from `src/Camera/Camera.cpp`); the game's land line test is ported (`LandIslandInterface::RayCast`) but not used for it |
| Region questions: does an area hold coast, water, land, hill, forest, field, town or citadel, and where exactly | partial | Water, coast, stream and drinking-water searches (`src/ECS/WaterQueries.cpp`) and dry land (`pot_resource::IsDryLand`); hill, field and town region searches are todo |
| Land is told apart as dry land, coast and water by its cell bits | done | `components/lnd` cell properties, `src/ECS/SeaCells.cpp` |

## Islands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land may hold several separate islands on the one map, with sea between them | done | Blocks are laid where the file puts them (`src/3D/Implementations/LandIsland.cpp`) |
| The sea stretches out to the horizon all round the map | done | `src/3D/Implementations/Ocean.cpp`, `src/Graphics/SeaRows.cpp`; see ../ocean/sea_surface.md |
| The camera and hand cannot leave the map's bounds | partial | See ../camera/ for the camera limits |
