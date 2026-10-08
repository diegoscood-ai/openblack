# Rain

Where a storm rains, streaks of rain fall over the land near the camera, as dense and as opaque as the rain there. Rain
puts out fires, waters crops, drowns out the wildlife and is heard drumming.

**Progress: 14/20 done, 3 partial — 78%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## Falling rain

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 128 streaks is repeated over every land block near the camera where it rains | done | `src/ECS/Weather/Rain.cpp`, `src/Graphics/RendererRain.cpp` |
| Each streak runs from under the ground up to the rain's height, with a dashed texture scrolling down it | done | `src/ECS/Weather/Rain.cpp` (the streaks, the scrolling texture row of `atmos.raw`) |
| Streaks slant, fade in and out over their life and start again elsewhere | done | `src/ECS/Weather/Rain.cpp` |
| A block shows more and more opaque streaks the harder it rains over its middle | done | The rain tiles in `src/ECS/Weather/Rain.cpp` (alpha by the rain of the block's middle cells) |
| Little rain shows nothing, and the rain thins from 100 units away from the camera | done | `src/ECS/Weather/Rain.cpp` (nothing at 5 or less, the 100 to 400 m fade) |
| The rain's height and speed follow the nearest storm's, or the calm air's, stepping towards them a frame at a time | done | `rain::Update` |
| Streaks are drawn with random numbers in the game's order | partial | The CRT draws are in the game's order, but our tree places the streaks at every land load where the game places them twice at start-up ([day-night-weather.md](../../bw1-notes/day-night-weather.md#pending)) |
| Rain is only drawn with the weather detail option on | partial | The Weather detail key is not read: the rain is drawn at every detail level |
| Rain splashes on the ground | todo | The drops on the ground are not ported ([day-night-weather.md](../../bw1-notes/day-night-weather.md#pending)) |
| Rain makes rings on the water | todo | see ../ocean/things_on_the_water.md |
| Rain is not drawn inside the temple | done | The temple pass draws no weather (`src/Graphics/RendererTemple.cpp`) |

## What rain does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rain (or snow) puts out fires and slows their spread | done | `src/ECS/Fire/FireEffect.cpp` (the rain or snow at the fire); see ../physics/ |
| Rain on burning things raises steam | done | The steam of a cooled fire (`src/ECS/Fire/FireGraphic.cpp`) |
| Fields grow faster in rain, by their crop's rain rules while growing and ripening | done | `fields::GrowthStep` with `weather::IsRainingAt` (`src/ECS/Fields.cpp`); see ../resources/ |
| Rain is heard, louder the harder it rains at the camera | done | The rain atmos volume in `src/Audio/Services/SoundMap.cpp` |
| Birdsong and wildlife fade as the weather worsens | done | `CalculateVolumes` in `src/Audio/Services/SoundMap.cpp`; test `test/audio/test_sound_map.cpp` |
| A town that wants rain raises its rain flag; rain on it satisfies the wish | done | `DesireForRain` is 0 (`src/ECS/Town/TownDesire.cpp`). Our wiki differs: the original's desire for rain is always 0 and nothing satisfies it ([villagers.md](../../bw1-notes/villagers.md#town-desires-and-distribution)) |
| Villagers react to rain (shelter, dance for it) | todo | Not found in our tree; see ../villager/ |
| The water miracle is local rain from the hand | partial | see ../miracles/water.md |
| The storm miracle's rain | done | see ../miracles/storm.md |
