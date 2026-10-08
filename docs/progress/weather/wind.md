# Wind

Each climate has a wind, and storms add their own. The wind blows smoke and steam, carries storms across the island,
is heard at the camera, and moves things scripts mark as caught by it.

**Progress: 9/15 done, 3 partial — 70%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## The wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A climate's wind lies between its season's extremes by how close it is to rain | done | The wind step in `src/ECS/Weather/Climate.cpp` |
| Storms add their wind within their radius | done | `storms::CalcAtmos` (`src/ECS/Weather/Storms.cpp`) |
| The wind at a point is part of the weather grid, and blended at the camera | done | `climate::ComputeWeather`, the smooth sampling of `src/ECS/Weather/Atmos.cpp`; `weather::GetWindAt` (`WeatherQueries.cpp`) |
| Storms drift with the wind | done | `climate::ProcessAll` |
| The ambient wind of the world is always calm in play | done | The ambient weather is all 0 (`src/ECS/Weather/Atmos.cpp`) |

## What the wind moves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Smoke and steam from fires drift with the wind | done | `src/ECS/Fire/FireGraphic.cpp` (the wind at the fire) |
| Particles of spells and effects drift with the wind | done | The particle rules that read the weather's wind (`src/Particles/Rules/Storm.cpp`, `Fireball.cpp`) |
| Chimney smoke drifts, and swirls in the hand's wake | partial | `src/ECS/ChimneySmoke.cpp` reads the hand's wind; whether it also takes the weather's wind is not confirmed |
| Trees and ripe crops sway | partial | `ecs::WindSway` (`src/ECS/Fields.cpp`: 16 phases at random speeds); whether the game sways them by the wind's strength is not confirmed |
| Clouds drift along their track | done | see ../sky/clouds.md |
| Scripts mark an object as blown by the wind | todo | SET_AFFECTED_BY_WIND is a stub in `src/CHLApi.cpp` |
| Scripts ask whether a wind miracle is at a point | todo | IS_WIND_MAGIC_AT_POS is a stub in `src/CHLApi.cpp` |
| Gusts of wind push particles in some effects | partial | The gusty-wind rule is in `src/Particles/PSys.cpp`, with a stand-in noise; see ../rendering/ |
| Scripts set and read a player's resistance to wind | todo | SET_PLAYER_WIND_RESISTANCE and GET_PLAYER_WIND_RESISTANCE are stubs in `src/CHLApi.cpp`; see ../miracles/ |

## Hearing the wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Wind is heard at the camera, louder the stronger it blows above a threshold | done | The wind volume in `src/Audio/Services/SoundMap.cpp` |
