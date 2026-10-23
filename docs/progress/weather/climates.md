# Climates and seasons

Every land has a global climate over the whole map and a few local ones its script places (a cold mountain, a hot
desert, a wet valley). Each climate has a temperature that follows the hour and the month, a wind that follows the
season, and a desire to rain that builds day by day until it breeds a storm.

**Progress: 28/29 done, 0 chosen, 0 partial, 0 inferred — 97%**

How the original does it, in our wiki: [Weather and climate](../../../world/weather.md).

## Climates from the land's script

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script makes the global climate and local ones with a type, a centre and two radii | done | `map_script::CreateWeatherClimate` (`src/Magic/Script/MapScriptWeather.cpp`) to `climate::Create` (`src/ECS/Weather/Climate.cpp`); tests `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid`, `ClimatesAddTemperatureAndWind` |
| The global climate covers the whole map with the first climate type, whatever the script says | done | `climate::Create` (id 0 replaces the world climate and ignores the rest); test `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid` |
| The script sets each climate's rain desire, dry days, raining days and whether it is raining | done | `climate::SetRain` (CREATE_WEATHER_CLIMATE_RAIN); test `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid` |
| The script sets each climate's temperature and target temperature | done | `climate::SetTemperature` (CREATE_WEATHER_CLIMATE_TEMP); test `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid` |
| The script sets each climate's wind and its angle | done | `climate::SetWind` (CREATE_WEATHER_CLIMATE_WIND); test `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid` |
| Climate types come from the game's info tables: storm cloud height, speed, lightning waits, overcast, most storms | done | The climate info rows of info.dat (`src/ECS/Weather/Climate.cpp`); test `WeatherTest.ClimateStormAtFullDesire` |
| Climates are saved and loaded with the game | pending | Needs saved games: the original saves and loads each climate with its own values (`GClimate::Save` / `Load`, [weather.md](../../../world/weather.md#gclimate-the-climates)); openblack has no saved games yet, see ../engine/ and the fork's issues #186 (reading the original's save files) and #187 (the writers per class) |

## Calendar and seasons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A game year lasts 36000 turns, starting on 5 May 1998 | done | `calendar::GetDaysFromStart` (`src/ECS/Weather/Calendar.cpp`); test `WeatherCalendar.StartDateAndSeasons` (`test/test_weather.cpp`) |
| The year has months and four seasons, spring, summer, autumn and winter, by day of the year | done | `calendar::GetMonth`, `GetSeason`; tests `WeatherCalendar.StartDateAndSeasons`, `TheWeatherSystemKeepsTheCalendar` |
| Each climate's rain budget and desire start from the season's values | done | `climate::Create` (a local climate, or the world's, from its info: the desire and the raining days from the season's least rain, the temperature from its range); test `WeatherTest.AClimateStartsWithTheSeasonsLeastRain` (`test/test_weather.cpp`) |

## Temperature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each climate's temperature steps towards a target for the hour of the day and the month | done | `UpdateTemperature` and `TargetTemperature` (the hour and month tables) in `src/ECS/Weather/Climate.cpp`; test `WeatherTest.TemperatureOscillatesAroundTheTarget` |
| The hour table is mapped onto the land's own dusk and dawn | done | `RelativeHour` in `src/ECS/Weather/Climate.cpp`: the visual hour cut to a whole one, through the clock's script-to-visual map, cut again, as the original; test `WeatherTest.TheHourTableIsReadAtTheLandsOwnHour` (the map's numbers from the exe through the oracle) |
| Once at its target the temperature keeps swinging across it by a tenth of the target's whole degrees a turn | done | `UpdateTemperature`, as the game (the tenth a double, the product rounded once to a float); tests `WeatherTest.TemperatureOscillatesAroundTheTarget`, `TheTemperatureStepsByATenthInTheGamesPrecision` (bits from the exe through the oracle) |
| Storms pull the temperature towards their own within their radius | done | The storm's part of the grid (`src/ECS/Weather/Storms.cpp`); test `WeatherTest.StormFadeAndCalcAtmos` |
| Rain turns to snow below freezing | done | `climate::CreateStorm` (the snow share); test `WeatherTest.ClimateStormAtFullDesire` (the snow share) |
| The creature feels the temperature where it stands | done | `CreaturePhysiologySystem.cpp` (`weather::GetTemperatureAt`); see ../creature/; scene `needs.cold` |

## Rain desire and storm breeding

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a game day each climate's desire to rain grows at random | done | The rain step in `src/ECS/Weather/Climate.cpp`; test `WeatherTest.RainDesireDrawsFromMinThenMax` |
| The monthly and hourly rain terms are 0, as the game never loads their table | done | `src/ECS/Weather/Climate.cpp` (as the game); test `WeatherTest.RainDesireDrawsFromMinThenMax` |
| When the desire reaches 1 the climate makes a storm, if it has raining days left and room for another | done | `climate::CreateStorm`; tests `WeatherTest.ClimateStormAtFullDesire`, `TheRainingDaysCapTheSeasonsRainInTheGamesPrecision` |
| A local climate makes its storm near its middle; the global one anywhere on the island | done | The storm placement in `src/ECS/Weather/Climate.cpp` (`FindWhereToCreateStorm`); tests `WeatherTest.ALocalClimateMakesItsStormNearItsMiddle`, `TheWorldClimateMakesItsStormAnywhereOnTheIsland`; the original's placement run on the exe through the oracle for 6000 random local places gives the same cells |
| A storm spot over water is tried again only when it is off the island, as in the game | done | The storm placement in `src/ECS/Weather/Climate.cpp` with `IsOffMapOrNoLand` (`src/ECS/Weather/WeatherLand.h`: off the 512 cells or no land block, by the island's block lookup), 20 tries at most; test `WeatherTest.AStormSpotWithNoLandBlockIsTriedAgain` |
| Raining days count up while it rains and down while it is dry, capping how long it may rain | done | `UpdateRain` and the storm test of `climate::ProcessAll` / `climate::CreateStorm` (the days x the double 0.01, rounded once to a float, against the season's most rain); tests `WeatherTest.TheRainingDaysCountUpWhileItRainsAndDownWhileItIsDry`, `TheRainingDaysCapTheSeasonsRainInTheGamesPrecision` (the cap from the exe through the oracle) |
| Days since the last rain are counted | done | `Climate::Rain::dryDays`, counted once a day while dry, back to 0 with each new storm or when the season's rain is spent; tests `WeatherTest.TheRainingDaysCountUpWhileItRainsAndDownWhileItIsDry`, `TheRainingDaysCapTheSeasonsRainInTheGamesPrecision` |
| A storm leaving its climate (or the global one's storm entering a climate, the world's own among them) starts to clear and the climate wants a new one | done | `climate::ProcessAll`, on a new day; test `WeatherTest.AStormThatLeavesItsClimateStartsToClear` |

## Wind

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once a day a climate's wind is the season's least plus most wind along its angle (the game's weight by the rain desire truncates to 0 except at exactly 0.5) | done | `UpdateWind` in `src/ECS/Weather/Climate.cpp`; test `WeatherTest.TemperatureOscillatesAroundTheTarget` (min + max along the angle) |
| Storms drift with the wind where they are | done | `ProcessClimate` (the grid's wind at the storm x 0.01 m a turn) in `climate::ProcessAll`; test `WeatherTest.AClimatesStormsDriftWithTheWindWhereTheyAre` |

## Script control

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Challenge scripts pause and restart the whole climate system | done | `script::PauseUnpauseClimateSystem` (`src/Magic/Script/CHLWeather.cpp`) |
| Challenge scripts pause and restart storm creation | done | `script::PauseUnpauseStormCreationInClimateSystem` |
| The climates run whatever the weather detail option | done | The exe reads the Weather key only in `start_system` (xref readers of the setting), for the drawn rain and snow (see rain.md); `climate::ProcessAll` runs at every level |
