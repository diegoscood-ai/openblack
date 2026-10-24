# Rain

Where a storm rains, streaks of rain fall over the land near the camera, as dense and as opaque as the rain there. Rain
puts out fires, waters crops, drowns out the wildlife and is heard drumming.

**Progress: 18/19 done, 0 chosen, 1 partial, 0 inferred — 99%**

How the original does it, in our wiki: [Weather and climate](../../../world/weather.md).

## Falling rain

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One set of 128 streaks is repeated over every land block near the camera where it rains | done | `src/3D/Rain.cpp`, `src/Graphics/RendererRain.cpp`; test `Rain.StreaksShowByTheStrengthNotTheMostRain`; scenes `light.rain_splashes(_night)` |
| Each streak runs from under the ground up to the rain's height, with a dashed texture scrolling down it | done | `src/3D/Rain.cpp` (the streaks, the scrolling texture row of `atmos.raw`); test `Rain.PlacedInTheGamesOrder`; scenes `light.rain_splashes(_night)` |
| Streaks slant, fade in and out over their life and start again elsewhere | done | `src/3D/Rain.cpp`; they move on after a frame where any block's rain was queued, also one too far to show streaks (`rain::Queued`, `RainSystem::TakeTiles`); tests `Rain.StepsAndStartsAgain`, `StreaksFadeInAndOut`, `AQueuedBlockCountsWhateverItShows` |
| A block shows more and more opaque streaks the harder it rains over its middle | done | The rain tiles in `src/3D/Rain.cpp` (alpha by the rain of the block's middle cells); test `Rain.StrengthIsEightyEightOfAHundredInWholeSteps` |
| Little rain shows nothing, and the rain thins from 100 units away from the camera | done | `src/3D/Rain.cpp` (no streaks at a strength of 5 or less, which is 6 % of rain or less, `rain::ShowsStreaks`; the 100 to 400 m fade); test `Rain.StreaksShowByTheStrengthNotTheMostRain` (the 100 to 400 m fade: scenes only) |
| The rain's height and speed follow the nearest storm's, or the calm air's, stepping towards them a frame at a time | done | `rain::Update`; test `Rain.FollowsTheNearestStorm` |
| Streaks are placed with the CRT random numbers in the game's order, twice at start-up and never on a land load | done | `weather::StartAtmos`, `RainSystem::Reset`; tests `Rain.PlacedInTheGamesOrder`, `AtmosStart.ScattersTheSnowTwiceThenPlacesTheRainTwice`, `AtmosStart.ALandsLoadDrawsNoAtmosNumbers` ([weather.md](../../../world/weather.md#the-drawn-rain)) |
| Rain is only placed, stepped and drawn with the weather detail option on | done | The Weather key, read once at start-up, gates the start-up placing (`weather::StartAtmos`); with it off the game has no rain at all, so no streak moves, no rain tile is drawn and no drop splashes (`RainSystemInterface::Disable`, `RainSystem::Update` and `TakeTiles`); the fall's height and speed still follow the storms, as the exe's `Update3D` does before its rain test. Tests `AtmosStart.NothingWithTheWeatherSettingOff`, `AtmosStart.TheRainIsThereWithTheWeatherSettingOn`; readout in the Weather window ([weather.md](../../../world/weather.md#the-drawn-rain)) |
| Rain splashes on the ground | done | Heavy rain's drops make rings where they land, on land and sea alike (`RainSystem::Splash` from `Renderer::DrawRainTile`, `rain::Splash`); tests `Rain.OnlyStrongRainSplashes`, `Rain.SplashFallsOnItsBlockInTheGamesOrder`; scenes `light.rain_splashes(_night)` ([weather.md](../../../world/weather.md#the-drawn-rain)) |
| Rain makes rings on the water | done | The splashes' rings (the row above); see ../ocean/things_on_the_water.md; tests `Rain.OnlyStrongRainSplashes`, `SplashFallsOnItsBlockInTheGamesOrder` |
| Rain is not drawn inside the temple | done | The temple pass draws no weather (`src/Graphics/RendererTemple.cpp`), and a frame inside the temple takes no rain tiles, so the streaks do not move on there (`rain::FrameTakesTiles`, `Renderer::PreDraw`), as the original's temple draw, which runs the atmosphere's update but not its draw; test `Rain.NoFrameInsideTheTempleTakesTheRain` |

## What rain does

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rain (or snow) puts out fires and slows their spread | done | `src/ECS/Fire/FireEffect.cpp` (the rain or snow at the fire); see ../physics/; test `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` |
| Rain on burning things raises steam | done | Rain counts as cooling (`src/Fire/FireModel.cpp`), and a cooled fire above 75 degrees, hotter than at its last burst, steams (`fire::graphic::StartsSteam`, `src/ECS/Fire/FireGraphic.cpp`), as the original's fire graphic; tests `FireGraphic.ACooledFireAboveSeventyFiveDegreesSteams`, `FireModel.SomethingNotBurningCoolsBySurfaceOverCapacityFasterInTheWetAndRain` (the rain's cooling flag) |
| Fields grow faster in rain, by their crop's rain rules while growing and ripening | done | `fields::GrowthStep` with `weather::IsRainingAt` (`src/ECS/Fields.cpp`); see ../resources/; test `test/test_fields.cpp` (`fields::GrowthStep` in the rain) |
| Rain is heard, louder the harder it rains at the camera | done | The rain atmos volume of `CalculateVolumes` in `src/Audio/SoundMap.cpp` (the camera's rain over the info's rain for the most volume); tests `AtmosAudio.MatchesOriginalSoundMap1`, `MatchesOriginalSoundMap2` (`test/audio/test_atmos.cpp`: 600 queries each recorded on the v1.42 exe, about 340 of them in the rain) |
| Birdsong and wildlife fade as the weather worsens | done | `CalculateVolumes` in `src/Audio/SoundMap.cpp` (the weather's fade of the day-only ambiences, `sound_map::DayOnlyShare`); tests `AtmosAudio.MatchesOriginalSoundMap1`, `MatchesOriginalSoundMap2` (the jungle, desert, countryside and swamp volumes in the rain and snow, recorded on the v1.42 exe) and `test/audio/test_sound_map.cpp` |
| A town's desire for rain is always 0, so it raises no rain flag and rain satisfies nothing | done | `DesireForRain` is 0 (`src/ECS/TownDesire.cpp`), as the original's (a constant 0, re-read in v1.42) with no satisfy check ([villagers.md](../../../beings/villager-decisions.md#town-desires-and-distribution)); test `TownDesireTest.ForRainIsAlwaysNothing` |
| The water miracle is local rain from the hand | partial 82% | As measured in ../miracles/water.md (82%) |
| The storm miracle's rain | done | see ../miracles/storm.md |
