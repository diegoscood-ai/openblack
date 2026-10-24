# Countries and ground types

Every cell belongs to one of the land's countries (a style of land such as grassland, desert, snowy mountain or
jungle). A country picks the ground's materials by height, and each material has a ground type that decides the sounds
of footsteps and the ambience.

**Progress: 10/10 done, 0 chosen, 0 partial, 0 inferred — 100%**

## Country styles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land has up to sixteen countries, each with its own table of materials by altitude | done | `components/lnd` (countries), `src/3D/BlockTexture.cpp` |
| The ground climbs through the country's materials with height (beach, grass, rock, snow on the peaks) | done | `src/3D/BlockTexture.cpp` |
| Countries meet with a soft blend rather than a hard line | done | `src/3D/BlockTexture.cpp`; test `test/test_block_texture.cpp` |
| Different lands use different styles (the Norse snow of land 2, the Japanese and Aztec lands of land 3) | done | Read from each land's file |

## Ground types

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each material has a ground type (grass, sand, rock, snow, mud and so on) | done | `LandIslandInterface::GetMaterialInfo` (`LandMaterialInfo::type`, `src/3D/LandIslandInterface.h`) |
| A creature's footsteps sound by the ground it walks on, and differently in water and snow | done | `src/ECS/Systems/Implementations/CreatureAudioSystem.cpp` (the surface from `sea_cells::GetSurfaceType`); see ../creature/ |
| Each cell has an ambient sound type (sea, fresh water, coast, jungle, arctic, desert, countryside, swamp, running water) heard around the camera | done | `src/Audio/SoundMap.cpp`; test `test/audio/test_sound_map.cpp`; see ../audio/ |
| Trees from the forest miracle are picked by the ground they grow on | done | `spell_forest::RandomTreeType` and the cell's terrain material (`src/Magic/Spells/SpellForest.cpp`); see ../miracles/forest.md |
| Temperature of the land comes from its climates, not its country | done | see ../weather/climates.md |
| The creature learns the land by regions (highest point, type of ground) as it explores | done | The original's map of the land for the creatures (`CreatureGlobalExplorationMap`, 64 x 64 regions of 8 x 8 cells) is `creature_exploration::RegionMap` with each creature's visited regions (`components::CreatureExploration`); how each region is worked out is checked against the exe (land_loading.md, region questions). When it is made matches the exe too: the original works the map out once a game, in `GGame::Init` (0x54FC30), after the start's land is loaded (a new game's map script and `OnNewGame`, a saved game, or a skirmish playground, whichever the start mode picks) and its control map updated; `PrecalculateMap` 0x4DF9C0 has no other caller and nothing else writes the map, and a land change (`GScript::LoadMap` → `GGame::LoadMap` 0x552F40: `ClearMap`, the landscape's release, `LoadMapFeatures`) does not make it again, so every later land's creatures read the first land's regions; a new game (`pc_main` runs `GGame::StartGame` → `Init` again) makes them afresh. openblack keeps them for the whole game in `creature_exploration::GameRegions` (in the registry's context, carried over each land's reset by `Game::ResetRegistryForNewLand`; made by `Game::StartNewLand` on the game's first land; a new game makes a new registry). The testbed, openblack's own land, makes its own. Test `CreatureExploration.TheGameKeepsTheFirstLandsRegionsOnEveryLaterLand`; debug readout "Creature regions made from" in the Land Island window. See ../creature/idle_behaviour.md |
