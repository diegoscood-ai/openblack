# Snow

Cold storms snow. Flakes tumble down near the camera, and the snow piles up on the land, buildings and trees, then melts
away slowly once the storm has gone.

**Progress: 21/22 done, 0 chosen, 1 partial, 0 inferred — 98%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/weather.md).

## Falling snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 256 flakes over an 80 unit square is repeated over every quarter block near the camera where it snows | done | `snowfall::BlockTiles` and `TileOf` (`src/3D/Snowfall`), `IslandTiles` in `src/ECS/Systems/Implementations/SnowfallSystem.cpp`: each block's four 80 x 80 tiles, read at their corners; test `Snowfall.ABlockSnowsOverItsFourTilesEachReadAtItsCorners` ([weather.md](../../../world/weather.md#the-drawn-snow)) |
| Flakes tumble as they drift down in circles | done | `snowfall::Step`; tests `Snowfall.FlakesFallDriftAndTurn`, `ATurnRoundTheCircleTurnsTheHeading` |
| A flake that falls through the ground starts again at the top | done | `snowfall::Step`; test `Snowfall.FlakesUnderTheGroundStartAgainAtTheTop` |
| More flakes and more opaque the harder it snows; none for a little snow; fainter from 50 units away | done | `snowfall::TileOf`; tests `Snowfall.MoreSnowMoreFlakesMoreOpaque`, `FainterFurtherAway` |
| Snow falls from the rain's height at the rain's speed | done | `weather::UpdateFrame` steps the flakes with the rain's fall speed and height (`src/ECS/Weather/WeatherLoop.cpp`); tests `Snowfall.FlakesFallDriftAndTurn`, `Snowfall.FlakesUnderTheGroundStartAgainAtTheTop` |
| Snow is only drawn with the weather detail option on | done | With the Weather setting off at start-up the game makes no falling snow (the original's snow object stays null, 0x835D74): no scatter (`weather::StartAtmos`), and `SnowfallSystemInterface::Disable` keeps every flake still (the original's `Update3D` steps the snow only when it is there, 0x835AA0) and takes no tile (`Render3D` queues none, 0x83662C); test `AtmosStart.NothingWithTheWeatherSettingOff`; readout in the Weather window |
| Snow is not drawn inside the temple, and its flakes wait there | done | A frame inside the temple takes no snow tiles (`snowfall::FrameTakesTiles`, `Renderer::PreDraw`), so the flakes do not move on, as the original's temple draw runs the atmosphere's update but not its draw ([weather.md](../../../world/weather.md#the-drawn-rain)); test `Snowfall.NoFrameInsideTheTempleTakesTheSnow` |

## Snow lying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A snowing storm piles snow on a grid of 40 unit cells, most within its inner radius | done | `snow_cover` (`src/3D/SnowCover`) through `SnowSystem::AddStorm` from each snowing storm (`src/ECS/Weather/Storms.cpp`); tests `SnowCover.StormsSnowBySnowRateAndFade`, `StormsSnowLessBeyondTheirInnerRadius` |
| Towards a storm's edge snow is taken away | done | test `SnowCover.StormsTakeSnowAwayNearTheirEdge` |
| Snow depth is capped | done | test `SnowCover.SnowLiesNoDeeperThanItsMost` |
| Snow melts by 2 in one of eight bands of rows every 0.3 seconds | done | `SnowSystem::Melt` from the weather's turn (`src/ECS/Weather/Atmos.cpp`); test `SnowCover.MeltsABandAtATime` |
| The land turns white where the snow is deep enough, raggedly by a noise image | done | baked into the block's texture at its prints step from the snow depth and the noise image (`block_texture::PrintSnow`, `land_prints::SourcesFor`), the melt's listed blocks painted again; tests `SnowCover.LandWhitensOverTheNoise`, `BlockPrints.SnowWhitensAsTheOriginal` (#222) |
| Buildings, trees and objects standing in snow show a snow texture over them, more the deeper it lies | done | `fs_object` and `snow_object.sh` show the snow texture by `snow_cover::ObjectLevel`; test `SnowCover.ObjectsShowSnowBeyond20` |
| Snow shows on the faces turned upward | done | `SnowUv` (`assets/shaders/snow.sh`) reads the snow texture by the mesh's own position and normal, and a morphing object's by its melted position and its normal turned by its axes, as the original's primitive snow draws (`snow_cover::ObjectUv`, `MorphingObjectNormal`; test `SnowCover.AnObjectReadsTheSnowTextureByItsOwnPositionAndNormal`; [weather.md](../../../world/weather.md#the-snow-on-objects)) |
| A new land starts with no snow | done | the snow lying is reset with the land (`weather::OnLoadMap`, `SnowSystem::Reset`, `src/ECS/Weather/WeatherLoop.cpp`); test `SnowSystem.ResetClearsTheSnow` |
| Snow lying is saved with the game | partial 50% | loaded: a saved game's lying snow comes back, the grid's depths, the blocks' changes and the melting clock and band, as the game object writes the snow cover (`ecs::saved_game_load::StateOf` / `Apply`, `SnowSystemInterface::Restore`; tests `SavedGameLoad.TheSnowLyingComesBackAsSaved`, `...ReferenceSavesHoldTheWholeSnowGrid`); the writing waits for the saves' writers ([saved games](../../../engine/saved-games.md#openblack)) |

## What snow does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A tornado over snow throws up snow dust | done | Read in the original: the dust takes the terrain material of the base's cell, snow (27) where the snow lying there (the weather's snow cover byte, half the snow cover grid) is at least 27 (`Terrain::GetTerrainMaterial`); openblack the same through `ecs::terrain_material::AtCell` (`storm::TornadoDustColour`, `src/Particles/Rules/Storm.cpp`; tests `Storm.theTornadosDustTakesTheTerrainMaterialOfItsCell` and `WeatherTest.ATornadoOverDeepLyingSnowThrowsUpSnowDust` (the snow's dust from a byte of 27, the land's at 26); ../miracles/tornado.md) |
| A creature's footsteps crunch in snow | done | The footstep sound follows the cell's surface type (`sea_cells::GetSurfaceType` in `CreatureAudioSystem.cpp`), whose terrain material is snow where the snow lying at the cell's corner is deep enough (`terrain_material::AtCell`), as the original's (test `WeatherTest.AStepOnDeepLyingSnowSoundsOfSnow`; [creature sounds](../../../audio/creature-and-interface-sounds.md)); a step's dust takes the tint of the snow lying there (`snow_dust::Tint`; test `PhysicsTurn.ADustInTheSnowTakesTheLandsLightByTheSnowsDepth`) |
| The forest miracle grows snowy trees on snow | done | The forest takes its trees from the cell's terrain material (`ecs::terrain_material::AtCell` in `src/Magic/Spells/SpellForest.cpp`), which is snow (27) where the snow lying at the cell's corner is at least 27 (`src/ECS/TerrainMaterial.cpp`); tests `WeatherTest.AStepOnDeepLyingSnowSoundsOfSnow` (the snow material), `SpellForest.realInfoDat` (snow's conifers) |
| Snow puts out fires as rain does | done | `src/ECS/Fire/FireEffect.cpp` reads the larger of the rain and the snow lying there (`weather::GetMaxRainingOrSnowingAt`, `src/ECS/Weather/WeatherQueries.cpp`); falling snow alone does not; test `WeatherTest.FallingSnowIsNotSnowOnTheGround` |
| The creature feels the cold | done | `CreaturePhysiologySystem.cpp`; see ../creature/; test `CreaturePhysiology.WarmthFollowsTheSigmoidOfTheTemperature` |
| Snow-covered peaks painted into the land are not weather and never melt | done | see ../terrain/countries.md |
