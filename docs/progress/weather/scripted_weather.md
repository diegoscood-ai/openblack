# Scripted and real-world weather

Weather the story places on purpose: weather objects a challenge script creates and steers, storms a land's script
lays, and the real weather of the player's home town, fetched over the internet.

**Progress: 6/15 done, 1 partial — 43%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## Weather objects from challenge scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script creates a weather object at a position, with a size | done | CHL CREATE of a weather thing (`src/CHLApi.cpp`) to `weather_thing::Create` (`src/ECS/Weather/WeatherThing.cpp`: a storm of inner 100 and outer 300 from the info row) |
| The script sets its temperature, rainfall, snowfall, overcast and how fast the rain falls | done | `script::ChangeWeatherProperties` (`src/Magic/Script/CHLWeather.cpp`) |
| The script sets how often it flashes with sheet and forked lightning | done | `script::ChangeLightningProperties` |
| The script sets how long it lasts and how long it fades | done | `script::ChangeTimeFadeProperties` |
| The script sets its number of clouds, their blackness and height | done | `script::ChangeCloudProperties` |
| The script sends it towards a point at a speed | todo | SET_HEADING_AND_SPEED is a stub in `src/CHLApi.cpp` (`weather_thing::SetTarget` and `SetSpeed` exist) |
| The script marks whether the wind carries it | todo | SET_AFFECTED_BY_WIND is a stub in `src/CHLApi.cpp` (`weather_thing::SetAffectedByWind` exists) |
| It finishes when its time is up, and scripts can tell | partial | Its storm ends with its life and the thing forgets it (`weather_thing::ProcessWeatherThings`); the thing itself is kept |
| Weather objects are saved with the game | todo | openblack has no saved games. See ../engine/ |
| The land's script lays a storm with its own cloud, rain and lightning settings | done | `FeatureScriptCommands::CreateWeatherStorm` to `map_script::CreateWeatherStorm` (`src/Magic/Script/MapScriptWeather.cpp`); test `WeatherTest.ScriptStormBytes` |

## Real-world weather

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player gives their country and town, from the game's country list | todo | Not in our tree |
| Outside multiplayer, the game asks a weather server for the day's weather where the player lives | todo | The game's weather service no longer exists; a replacement source would be needed |
| The real weather is turned into a preset that sets the land's rain, snow, temperature and wind | todo | Not in our tree |
| The last weather fetched is kept on disk for when there is no connection | todo | Not in our tree |

## Options

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather detail option turns falling rain and snow on and off | todo | The Weather detail key is not read (`detail_level::Level::weather`, `src/Graphics/DetailLevel.h`) |
| The weather debug window shows the climates and storms, forces storms and strikes lightning | n/a | openblack-only, `src/Debug/Weather.cpp`; see ../debug/ |
