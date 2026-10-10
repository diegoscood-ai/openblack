# Day and night

The world runs through day, dusk and night on a clock of its own, about 28 minutes a day by default. Each land's
script sets the length of the day and how much of it is night; challenge scripts can stop, set or move the clock.

**Progress: 20/24 done, 2 partial — 88%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## The clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The time of day runs evenly, a whole day in a set number of seconds of game time, moving on once a turn | done | `DayNightClock::ProcessTurn` (`src/3D/DayNightClock.cpp`) |
| The default day lasts 1700 seconds, 8.3% of it full night and 7% of it changing | done | `DayNightClock::k_DefaultDuration`, `k_DefaultNight`, `k_DefaultChange` |
| A land opens at noon with the clock running | done | `DayNightClock::Reset` from `Game::LoadMap` (`src/Game.cpp`) |
| The sky turns at four hours of each half day: full night, the start and end of dusk, full day | done | `DayNightClock::SetCycle` and `sky_type::SetThresholds` (`src/3D/SkyType.cpp`); test `test/test_sky_type.cpp`; fireflies follow these stages: [../nature/fireflies.md](../nature/fireflies.md#night-and-day) |
| Each land's script sets the day's length and its night and change fractions | done | `FeatureScriptCommands::SetNighttime` (SET_NIGHTTIME, `src/LHScriptX/FeatureScriptCommands.cpp`) |
| Scripts see a stretched time in which those four hours fall at 3.5, 7.5, 8 and 8.5 | done | `DayNightClock::GetScriptTime` |
| Scripts jump the clock to an hour; the sky's dome is rebuilt at once | done | SET_GAME_TIME in `src/CHLApi.cpp` to `DayNightClock::SetScriptTime` and `sky_type::Jump` |
| Scripts read the hour | done | GET_GAME_TIME in `src/CHLApi.cpp` |
| Scripts move the clock to an hour over some seconds, the short way round | done | MOVE_GAME_TIME in `src/CHLApi.cpp` |
| Scripts stop and restart the clock | done | GAME_TIME_ON_OFF in `src/CHLApi.cpp` |
| Scripts change the day's length and fractions, and reset them to the default | done | SET_GAME_TIME_PROPERTIES and RESET_GAME_TIME_PROPERTIES in `src/CHLApi.cpp` |
| The clock stops while the game is paused | done | No game turn runs while paused (the clock moves once a turn) |
| The clock is saved and loaded with the game | todo | openblack has no saved games. See ../engine/ |
| Scripts read the real date and time of the player's computer | todo | GET_REAL_TIME, GET_REAL_DAY, GET_REAL_MONTH and GET_REAL_YEAR are stubs in `src/CHLApi.cpp` |

## What night changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky's dome turns from day through dusk to night | done | see sky_dome.md |
| The land's and models' light follows the time of day | done | see lighting.md |
| The sun sets and the moon rises | done | see sun_and_moon.md |
| Windows of homes with people in light up at night | done | `night_lights::WindowColour` (`src/3D/NightLights.cpp`), with the abode's people at home (`RenderingSystem.cpp`); see ../building/ |
| Village lanterns and fires glow at night and light the land | partial | Street lanterns and the gate's lamps (`night_lights::Update`, `src/3D/NightLights.cpp`, run by `VillageLightSystem::Update`, `Locator::villageLightSystem`); the temple's lanterns are not lit; see ../town/ and ../rendering/ |
| The hand gives off light at night | done | `src/Graphics/HandLight.cpp`; see ../hand/ |
| Villagers go home to bed at night | partial | see ../villager/daily_routine.md |
| Night sounds (crickets, owls) replace the day's | done | The sound map by the sky type (`src/Audio/Services/SoundMap.cpp`); see ../audio/ |
| The haze closes in at dusk | done | `sky_type::HazeFactor` in `LandLightTable::Build` (`src/3D/LandLightTable.cpp`) |
| Temperature falls at night and rises by day | done | The climate's hour factor (`src/ECS/Weather/Climate.cpp`); see ../weather/climates.md |
