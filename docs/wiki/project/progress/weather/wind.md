# Wind

Each climate has a wind, and storms add their own. The wind blows smoke and steam, carries storms across the island,
is heard at the camera, and moves things scripts mark as caught by it.

**Progress: 15/15 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Weather and climate](../../../world/weather.md).

## The wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a day a climate's wind is its season's least plus most wind along its angle (the game's weight by the rain desire truncates to 0 except at exactly 0.5) | done | `UpdateWind` in `src/ECS/Weather/Climate.cpp`; test `WeatherTest.TemperatureOscillatesAroundTheTarget` |
| Storms add their wind within their radius | done | `storms::CalcAtmos` (`src/ECS/Weather/Storms.cpp`); test `Storm.gatherDescriptorWindAndRain` |
| The wind at a point is part of the weather grid, and blended at the camera | done | `climate::ComputeWeather`, the smooth sampling of `src/ECS/Weather/Atmos.cpp`; `weather::GetWindAt` (`WeatherQueries.cpp`); test `WeatherTest.ClimatesAddTemperatureAndWind` |
| Storms drift with the wind | done | `climate::ProcessAll` (a climate's storms, by the grid's wind x 0.01 m a turn); a weather thing's storm by the wind of `ComputeWeather` when a script lets the wind carry it (`weather_thing::ProcessWeatherThings`); test `WeatherTest.AClimatesStormsDriftWithTheWindWhereTheyAre` |
| The ambient wind of the world is always calm in play | done | The ambient weather is all 0 (`src/ECS/Weather/Atmos.cpp`); test `WeatherTest.AtmosGridAndHeight` (the ambient weather 0) |

## What the wind moves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke and steam from fires drift with the wind | done | `fire::graphic::StepPuffs` (`src/ECS/Fire/FireGraphic.cpp`): each puff's velocity eases towards half the smoothed wind at the burning object, by a tenth of the difference a second, as the original's steam and smoke step (its constants and order re-read in v1.42); test `FireGraphic.SteamAndSmokeDriftWithHalfTheWindAndRise` |
| Particles of spells and effects drift with the wind | done | The particle rules that read the weather's smoothed wind: the storm's cloud cores (`storm::CloudWindVelocity`, `src/Particles/Rules/Storm.cpp`) and the fireball's and the miracles' falling atoms (`fireball_rules::WindVelocity`, `Fireball.cpp`), each towards the wind times its magnification times 0.1 by its damping times the step, as the original's rules (re-read in v1.42); tests `Storm.theCloudCoresDriftWithTheWind`, `FireballDirection.AWindyAtomIsPulledTowardsTheWind` |
| Chimney smoke drifts at random and swirls in the hand's wake, not with the weather's wind | done | `chimney_smoke::Drift` (`src/3D/ChimneySmoke.cpp`); the original's smoke draw reads only the hand's position and motion that `GLandscape::Draw` writes ([0xE9A100..0xE9A124]), no weather; tests `test/test_chimney_smoke.cpp` |
| Trees and ripe crops sway | done | `VegetationSystem` (`src/ECS/Systems/Implementations/VegetationSystem.cpp`), as the trees' pre-draw of v1.42: 16 sways whose speeds are the CRT `Random(1, 2)` every 2 s of the frame's whole game ms (none in pause), the phase moving on by ms x speed x 7/6600 (the float 0.0010606061), the lean -(cos(phase) x 0.03) along z alone (the wind's direction is fixed at 0 and its strength does not enter); tests `VegetationSystem.ASwayStepsAndLeansInTheGamesPrecision` (bits from the exe through the oracle) and `test/test_vegetation.cpp` |
| Clouds drift along their track | done | see ../sky/clouds.md; test `Clouds.DriftAlongTheTrack` |
| Scripts mark an object as blown by the wind | done | SET_AFFECTED_BY_WIND (`magic::script::SetAffectedByWind`): a weather thing keeps the value, which its storm's drift reads; the game's other things take no notice of it either; checked against v1.42 on 2026-10-20; test `WeatherTest.AScriptResizesAWeatherThingsStormAndLetsTheWindCarryIt` |
| Scripts ask whether a wind miracle is at a point | done | IS_WIND_MAGIC_AT_POS (`src/CHLApi.cpp`) asks `weather::storms::IsWindMagicAt` (`src/ECS/Weather/Storms.cpp`): a storm with no clouds whose outer radius reaches the point; the script's radius is popped and not read, as the original; test `WeatherTest.WindMagicIsACloudlessStormOverThePoint`; readout in the Weather window (at the camera) |
| Gusts of wind push particles in some effects | done | `UR_GustyWind` (`src/Particles/ParticleEffect.cpp`): the wind of the original's 3D lattice noise at the atom (`noise::ValueNoise3`, `src/Particles/Noise.cpp`, at (x, y, z) across and (y, z, x) along z) times the wind speed, pulled or added as the original (re-read in v1.42); test `ParticleNoise.TheThreeDimensionalNoiseIsTheOriginals` (eight points from the exe through the oracle) |
| Scripts set and read a player's resistance to wind | done | SET_PLAYER_WIND_RESISTANCE and GET_PLAYER_WIND_RESISTANCE (`src/CHLApi.cpp`) keep and read the player's value (`ecs::wind_resistance`, `src/ECS/WindResistance.cpp`, a `NoWindResistance` component on the player), popping as the original (GET pops the player alone); with the value set a thing the player's hand (or a computer player's) throws has its body's drag cleared right after the throw, as the original's throw from the hand; test `WeatherTest.AScriptSetsAndReadsAPlayersResistanceToWind` (the drag itself is not checked: a body's drag has no reader outside the physics) |

## Hearing the wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wind is heard at the camera, louder the stronger it blows above a threshold | done | The wind volume of `CalculateVolumes` in `src/Audio/SoundMap.cpp` (the wind's speed less 15, over 30); tests `AtmosAudio.MatchesOriginalSoundMap1`, `MatchesOriginalSoundMap2` (`test/audio/test_atmos.cpp`: about 340 of each file's 600 queries in the wind, recorded on the v1.42 exe) |
