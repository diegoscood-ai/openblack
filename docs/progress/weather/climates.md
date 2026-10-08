# Climates and seasons

Every land has a global climate over the whole map and a few local ones its script places (a cold mountain, a hot
desert, a wet valley). Each climate has a temperature that follows the hour and the month, a wind that follows the
season, and a desire to rain that builds day by day until it breeds a storm.

**Progress: 27/29 done, 1 partial — 95%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## Climates from the land's script

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes the global climate and local ones with a type, a centre and two radii | done | `map_script::CreateWeatherClimate` (`src/Magic/Script/MapScriptWeather.cpp`) to `climate::Create` (`src/ECS/Weather/Climate.cpp`) |
| The global climate covers the whole map with the first climate type, whatever the script says | done | `climate::Create` (id 0 replaces the world climate and ignores the rest) |
| The script sets each climate's rain desire, dry days, raining days and whether it is raining | done | `climate::SetRain` (CREATE_WEATHER_CLIMATE_RAIN) |
| The script sets each climate's temperature and target temperature | done | `climate::SetTemperature` (CREATE_WEATHER_CLIMATE_TEMP) |
| The script sets each climate's wind and its angle | done | `climate::SetWind` (CREATE_WEATHER_CLIMATE_WIND) |
| Climate types come from the game's info tables: storm cloud height, speed, lightning waits, overcast, most storms | done | The climate info rows of info.dat (`src/ECS/Weather/Climate.cpp`) |
| Climates are saved and loaded with the game | todo | openblack has no saved games. See ../engine/ |

## Calendar and seasons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A game year lasts 36000 turns, starting on 5 May 1998 | done | `calendar::GetDaysFromStart` (`src/ECS/Weather/Calendar.cpp`); test `WeatherCalendar.StartDateAndSeasons` (`test/test_weather.cpp`) |
| The year has months and four seasons, spring, summer, autumn and winter, by day of the year | done | `calendar::GetMonth`, `GetSeason` |
| Each climate's rain budget and desire start from the season's values | done | `climate::Create` (a local climate from its info) |

## Temperature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each climate's temperature steps towards a target for the hour of the day and the month | done | `UpdateTemperature` and `TargetTemperature` (the hour and month tables) in `src/ECS/Weather/Climate.cpp` |
| The hour table is mapped onto the land's own dusk and dawn | done | `RelativeHour` in `src/ECS/Weather/Climate.cpp` |
| Once at its target the temperature settles, where the game swings across it ten times a second | done | Our tree keeps the game's swing (`UpdateTemperature`); test `WeatherTest.TemperatureOscillatesAroundTheTarget` |
| Storms pull the temperature towards their own within their radius | done | The storm's part of the grid (`src/ECS/Weather/Storms.cpp`); test `WeatherTest.StormFadeAndCalcAtmos` |
| Rain turns to snow below freezing | done | `climate::CreateStorm` (the snow share) |
| The creature feels the temperature where it stands | done | `CreaturePhysiologySystem.cpp` (`weather::GetTemperatureAt`); see ../creature/ |

## Rain desire and storm breeding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a game day each climate's desire to rain grows at random | done | The rain step in `src/ECS/Weather/Climate.cpp`; test `WeatherTest.RainDesireDrawsFromMinThenMax` |
| The monthly and hourly rain terms are 0, as the game never loads their table | done | `src/ECS/Weather/Climate.cpp` (as the game) |
| When the desire reaches 1 the climate makes a storm, if it has raining days left and room for another | done | `climate::CreateStorm`; test `WeatherTest.ClimateStormAtFullDesire` |
| A local climate makes its storm near its middle; the global one anywhere on the island | done | The storm placement in `src/ECS/Weather/Climate.cpp` |
| A storm spot over water is tried again only when it is off the island, as in the game | done | The storm placement in `src/ECS/Weather/Climate.cpp` |
| Raining days count up while it rains and down while it is dry, capping how long it may rain | done | `climate::ProcessAll` |
| Days since the last rain are counted | done | `Climate::Rain::dryDays` |
| A storm leaving its climate (or the global one's storm entering a local climate) starts to clear and the climate wants a new one | done | `climate::ProcessAll` |

## Wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A climate's wind lies between the season's extremes by how close it is to rain | done | The wind step in `src/ECS/Weather/Climate.cpp` |
| Storms drift with the wind where they are | done | `climate::ProcessAll` |

## Script control

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenge scripts pause and restart the whole climate system | done | `script::PauseUnpauseClimateSystem` (`src/Magic/Script/CHLWeather.cpp`) |
| Challenge scripts pause and restart storm creation | done | `script::PauseUnpauseStormCreationInClimateSystem` |
| The weather detail option turns the weather system off | partial | The Weather detail key is not read (`detail_level::Level::weather`): the climates run and the rain is drawn at every level |
