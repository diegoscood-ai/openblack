# Snow

Cold storms snow. Flakes tumble down near the camera, and the snow piles up on the land, buildings and trees, then melts
away slowly once the storm has gone.

**Progress: 5/21 done, 1 partial — 26%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## Falling snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 256 flakes over an 80 unit square is repeated over every quarter block near the camera where it snows | todo | The drawn snow is not ported ([day-night-weather.md](../../bw1-notes/day-night-weather.md#the-drawn-snow)) |
| Flakes tumble as they drift down in circles | todo | The drawn snow is not ported |
| A flake that falls through the ground starts again at the top | todo | The drawn snow is not ported |
| More flakes and more opaque the harder it snows; none for a little snow; fainter from 50 units away | todo | The drawn snow is not ported |
| Snow falls from the rain's height at the rain's speed | todo | The drawn snow is not ported (the rain's height and speed are in `src/ECS/Weather/Rain.cpp`) |
| Snow is only drawn with the weather detail option on | todo | The drawn snow is not ported |

## Snow lying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A snowing storm piles snow on a grid of 40 unit cells, most within its inner radius | todo | The lying snow is not ported: the grid's snow cover stays 0 (`src/ECS/Weather/Atmos.cpp`; [day-night-weather.md](../../bw1-notes/day-night-weather.md#lh3datmos-the-40-m-grid-atmoscpp)) |
| Towards a storm's edge snow is taken away | todo | The lying snow is not ported |
| Snow depth is capped | todo | The lying snow is not ported |
| Snow melts by 2 in one of eight bands of rows every 0.3 seconds | todo | The lying snow is not ported |
| The land turns white where the snow is deep enough, raggedly by a noise image | todo | The lying snow is not ported |
| Buildings, trees and objects standing in snow show a snow texture over them, more the deeper it lies | todo | The lying snow is not ported |
| Snow shows on the faces turned upward | todo | The lying snow is not ported |
| A new land starts with no snow | done | The weather is cleared with the land (`magic::OnLoadMap`); there is never lying snow in our tree |
| Snow lying is saved with the game | todo | openblack has no saved games. See ../engine/ |

## What snow does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature's footsteps crunch in snow | partial | The footstep sound follows the cell's surface type (`CreatureAudioSystem.cpp`); there is no lying snow to make it crunch |
| A tornado over snow throws up snow dust | todo | No lying snow in our tree; see ../miracles/tornado.md |
| The forest miracle grows snowy trees on snow | done | `TerrainMaterial` in `src/Magic/Spells/SpellForest.cpp` takes the snowy material where it snows hard (the snow byte of the weather there) |
| Snow puts out fires as rain does | done | `src/ECS/Fire/FireEffect.cpp` (the larger of rain and snow) |
| The creature feels the cold | done | `CreaturePhysiologySystem.cpp`; see ../creature/ |
| Snow-covered peaks painted into the land are not weather and never melt | done | see ../terrain/countries.md |
