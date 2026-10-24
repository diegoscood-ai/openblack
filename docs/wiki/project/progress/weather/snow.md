# Snow

Cold storms snow. Flakes tumble down near the camera, and the snow piles up on the land, buildings and trees, then melts
away slowly once the storm has gone.

**Progress: 18/22 done, 0 chosen, 2 partial, 0 inferred — 82%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/weather.md).

## Falling snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 256 flakes over an 80 unit square is repeated over every quarter block near the camera where it snows | done | `snowfall::BlockTiles` and `TileOf` (`src/3D/Snowfall`), `IslandTiles` in `src/ECS/Systems/Implementations/SnowfallSystem.cpp`: each block's four 80 x 80 tiles, read at their corners; test `Snowfall.ABlockSnowsOverItsFourTilesEachReadAtItsCorners` ([weather.md](../../../world/weather.md#the-drawn-snow)) |
| Flakes tumble as they drift down in circles | done | `snowfall::Step`; tests `Snowfall.FlakesFallDriftAndTurn`, `ATurnRoundTheCircleTurnsTheHeading` |
| A flake that falls through the ground starts again at the top | done | `snowfall::Step`; test `Snowfall.FlakesUnderTheGroundStartAgainAtTheTop` |
| More flakes and more opaque the harder it snows; none for a little snow; fainter from 50 units away | done | `snowfall::TileOf`; tests `Snowfall.MoreSnowMoreFlakesMoreOpaque`, `FainterFurtherAway` |
| Snow falls from the rain's height at the rain's speed | done | `weather::UpdateFrame` steps the flakes with the rain's fall speed and height (`src/ECS/Weather/WeatherLoop.cpp`) |
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
| Snow shows on the faces turned upward | partial | `SnowUv` places the snow by the face's normal (`assets/shaders/vs_object.sc`); not yet read against the original |
| A new land starts with no snow | done | the snow lying is reset with the land (`weather::OnLoadMap`, `SnowSystem::Reset`, `src/ECS/Weather/WeatherLoop.cpp`); test `SnowSystem.ResetClearsTheSnow` |
| Snow lying is saved with the game | pending | openblack has no saved games. See ../engine/ |

## What snow does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature's footsteps crunch in snow | partial | The footstep sound follows the cell's surface type (`CreatureAudioSystem.cpp`); there is no lying snow to make it crunch |
| A tornado over snow throws up snow dust | pending | No lying snow in our tree; see ../miracles/tornado.md |
| The forest miracle grows snowy trees on snow | done | `TerrainMaterial` in `src/Magic/Spells/SpellForest.cpp` takes the snowy material where it snows hard (the snow byte of the weather there) |
| Snow puts out fires as rain does | done | `src/ECS/Fire/FireEffect.cpp` (the larger of rain and snow) |
| The creature feels the cold | done | `CreaturePhysiologySystem.cpp`; see ../creature/ |
| Snow-covered peaks painted into the land are not weather and never melt | done | see ../terrain/countries.md |
