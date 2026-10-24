# Scripted and real-world weather

Weather the story places on purpose: weather objects a challenge script creates and steers, and storms a land's
script lays. The real weather of the player's home town, fetched over the internet, depended on Lionhead's weather
service, which is gone: it is in [not-in-the-original.md](../../not-in-the-original.md#weatherscripted_weather).

**Progress: 10/11 done, 0 chosen, 0 partial, 0 inferred — 91%**

How the original does it, in our wiki: [Weather and climate](../../../world/weather.md).

## Weather objects from challenge scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script creates a weather object at a position, with a size | done | CHL CREATE of a weather thing (`src/CHLApi.cpp`) to `weather_thing::Create` (`src/ECS/Weather/WeatherThing.cpp`: a storm of inner 100 and outer 300 from the info row); test `WeatherTest.AScriptResizesAWeatherThingsStormAndLetsTheWindCarryIt` |
| The script sets its temperature, rainfall, snowfall, overcast and how fast the rain falls | done | `script::ChangeWeatherProperties` (`src/Magic/Script/CHLWeather.cpp`), the arguments in the original's order (checked against v1.42); test `WeatherTest.AScriptChangesAWeatherThingsWeatherLightningAndTimes` |
| The script sets how often it flashes with sheet and forked lightning | done | `script::ChangeLightningProperties`, the fork and then the sheet wait drawn again (`weather_thing::UpdateStats`); test `WeatherTest.AScriptChangesAWeatherThingsWeatherLightningAndTimes` |
| The script sets how long it lasts and how long it fades | done | `script::ChangeTimeFadeProperties`; test `WeatherTest.AScriptChangesAWeatherThingsWeatherLightningAndTimes` |
| The script sets its number of clouds, their blackness and height | done | `script::ChangeCloudProperties` (the number cut to a whole one); test `WeatherTest.AScriptChangesAWeatherThingsCloudsAndTheirStorm` |
| The script sends it towards a point at a speed | done | SET_HEADING_AND_SPEED on a weather thing (`magic::script::SetWeatherHeadingAndSpeed`, `src/Magic/Script/CHLWeather.cpp`): the heading's velocity becomes its storm's wind bytes (`weather_thing::SetMovement`, x 8 kept to -127..127), which the wind it drifts by takes in; MOVE_GAME_THING's destination is `weather_thing::SetTarget`; test `WeatherTest.AScriptSendsAWeatherThingOffAtAHeadingAndSpeed` |
| The script marks whether the wind carries it | done | SET_AFFECTED_BY_WIND (`magic::script::SetAffectedByWind`, `src/Magic/Script/CHLWeather.cpp`) gives the weather thing the value (`weather_thing::SetAffectedByWind`), which its storm's drift reads; checked against v1.42 on 2026-10-20; test `WeatherTest.AScriptResizesAWeatherThingsStormAndLetsTheWindCarryIt` |
| It finishes when its time is up, and scripts can tell | done | Its storm ends with its life; each turn the thing forgets a storm that is over and is deleted unless a script holds it (`ecs::script_held::IsInScript`), and the storm takes whether a script controls the thing (nothing in the game reads that); `weather_thing::ProcessWeatherThings`; test `WeatherTest.AWeatherThingEndsWithItsStormUnlessAScriptHoldsIt`; readout in the Weather window (Weather things) |
| Weather objects are saved with the game | pending | Needs saved games; openblack has no saved games yet, see ../engine/ and the fork's issues #186 (reading the original's save files) and #187 (the writers per class) |
| The land's script lays a storm with its own cloud, rain and lightning settings | done | `FeatureScriptCommands::CreateWeatherStorm` to `map_script::CreateWeatherStorm` (`src/Magic/Script/MapScriptWeather.cpp`); test `WeatherTest.ScriptStormBytes` |

## Options

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather detail option turns falling rain and snow on and off | done | `detail_level::Weather`, read once at start-up (`weather::StartAtmos`): with it on the flakes and streaks are placed; with it off the game has neither, so the rain and the falling snow are neither stepped nor drawn (`RainSystemInterface::Disable`, `SnowfallSystemInterface::Disable`, see rain.md and snow.md); tests `AtmosStart.NothingWithTheWeatherSettingOff`, `TheRainIsThereWithTheWeatherSettingOn`; readouts in the Weather window ([weather.md](../../../world/weather.md#the-drawn-snow)) |
