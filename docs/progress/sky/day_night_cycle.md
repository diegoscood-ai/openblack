# Day and night

The world runs through day, dusk and night on a clock of its own, about 28 minutes a day by default. Each land's
script sets the length of the day and how much of it is night; challenge scripts can stop, set or move the clock.

**Progress: 23/24 done, 0 chosen, 0 partial, 0 inferred — 96%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/day-and-night.md), [World rendering: original versus openblack](../../../world/sea-coast-and-rivers.md).

## The clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The time of day runs evenly, a whole day in a set number of seconds of game time, moving on once a turn | done | `DayNightClock::ProcessTurn` (`src/3D/DayNightClock.cpp`), as `GLandAlignement::UpdateTime` ([day-and-night.md](../../../world/day-and-night.md#clock)); test `DayNightClock.ADayLastsItsDuration` |
| The default day lasts 1700 seconds, 8.3% of it full night and 7% of it changing | done | `DayNightClock::k_DefaultDuration`, `k_DefaultNight`, `k_DefaultChange` (`ResetGameTimeProperties` 0x711520); test `SkyType.DefaultCycleThresholds` |
| A new game opens at noon with the clock running; a land change keeps the clock as it was | done | `DayNightClock::Reset` from `Game::PrepareNewLand` only at a new game (`land_load::Resets::newGame`: the first land, a playground's start, the testbed), as `GLandAlignement::Open`, which does nothing once the land is open until the game closes; a land change keeps the time, the cycle, the clock's switch and a time move going on (2026-10-22, #206; before, every land opened at noon); `LandLoadScripts.OnlyANewGameMakesTheCameraAgain`; the debug window's clock readout ([day and night](../../../world/day-and-night.md#notes-on-the-originals-sections)); checked by `fidelity` and `cycle` |
| The sky turns at four hours of each half day: full night, the start and end of dusk, full day | done | `DayNightClock::SetCycle` and `sky_type::SetThresholds` (`src/3D/SkyType.cpp`); test `test/test_sky_type.cpp`; fireflies follow these stages: [../nature/fireflies.md](../nature/fireflies.md#night-and-day) |
| Each land's script sets the day's length and its night and change fractions | done | `FeatureScriptCommands::SetNighttime` (SET_NIGHTTIME, `src/LHScriptX/FeatureScriptCommands.cpp`) to `DayNightClock::SetCycleFromLand` (night <= 1, change <= 1 - night, as 0x557BB0); test `DayNightClock.ALandsCycleIsClamped` |
| Scripts see a stretched time in which those four hours fall at 3.5, 7.5, 8 and 8.5 | done | `DayNightClock::GetScriptTime`; test `DayNightClock.ScriptTimeFallsOnTheStandardHours` |
| Scripts jump the clock to an hour; the sky's dome is rebuilt at once | done | SET_GAME_TIME in `src/CHLApi.cpp` to `DayNightClock::SetScriptTime` and `sky_type::Jump`; test `SkyType.ForceScriptTimeJumps` |
| Scripts read the hour | done | GET_GAME_TIME in `src/CHLApi.cpp` (`GetScriptTime`); test `DayNightClock.ScriptTimeFallsOnTheStandardHours` |
| Scripts move the clock to an hour over some seconds, the short way round | done | MOVE_GAME_TIME in `src/CHLApi.cpp` (`MoveScriptTime`); test `DayNightClock.MovesToAnHourInItsSeconds` |
| Scripts stop and restart the clock | done | GAME_TIME_ON_OFF in `src/CHLApi.cpp` (`SetRunning`, scale 1 or 0 as 0x710E90); test `SkyType.ClockRunsUntilStopped` |
| Scripts change the day's length and fractions, and reset them to the default | done | SET_GAME_TIME_PROPERTIES and RESET_GAME_TIME_PROPERTIES in `src/CHLApi.cpp` (`SetCycle`, as `SetVisualTimeCycle` 0x557620); test `SkyType.DefaultCycleThresholds` |
| The clock stops while the game is paused | done | No game turn runs while paused (the clock moves once a turn); test `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| The clock is saved and loaded with the game | pending | Needs saved games: the original's game load sets the visual time it read from the file and then its day's length and fractions ([sky.md](../../../world/sky.md#sky-type)); openblack has no saved games yet, see ../engine/ and the fork's issues #186 (reading the original's save files) and #187 (the writers per class) |
| Scripts read the real date and time of the player's computer | done | GET_REAL_TIME, both GET_REAL_DAY, GET_REAL_MONTH and GET_REAL_YEAR read the computer's local time through `script::real_time` (`src/ScriptHeaders/ScriptRealTime.h`), each float as the original's, settled with the exe oracle ([day-and-night.md](../../../world/day-and-night.md#the-computers-clock-in-the-scripts)); test `test_script_real_time`; the Moon debug window shows what they read |

## What night changes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky's dome turns from day through dusk to night | done | see sky_dome.md |
| The land's and models' light follows the time of day | done | see lighting.md |
| The sun sets and the moon rises | done | see sun_and_moon.md |
| Windows of homes with people in light up at night | done | `night_lights::WindowColour` (`src/3D/NightLights.cpp`), with the abode's people at home (`RenderingSystem.cpp`), as `Abode::Draw` ([day-and-night.md](../../../world/day-and-night.md#night-lights)): the offset cut towards nought, the strength from 20.5 and up to 3 o'clock, the flicker table and the grey scaled below 256 as the exe's; test `VillageLights.WindowsOfAHomeWithPeopleInLightUpAtNight`, the night land view shot. See ../building/ |
| Village lanterns and fires glow at night and light the land | done | Every light the original stamps on the land at night is made for a street lantern (a town light, or a country campfire light one unit up), a Norse gate's lamp or the temple door's two lamps: `night_lights::Update` (`src/3D/NightLights.cpp`), run by `VillageLightSystem::Update` (`Locator::villageLightSystem`), and `worship::citadel_lanterns`; tests `test_village_light_system` (the campfire's light type 1 too), `test_citadel_lanterns`, `test_village_lights`. A Norse gate's two lamp models are drawn only on a frame whose draw of the gate found it on the screen, as the original's (`draw_gate::GateLampHidden`; test `DrawGate.AGatesLampIsHiddenUnlessItsGatesDrawFoundTheGateOnTheScreen`), and lit and hazed at the gate's place, as the gate's draw hands both lamps its own colour and fog (`draw_gate::LampLightOrigin`; test `DrawGate.AGatesLampIsLitAtItsGatesPlace`; readout: the Land Island window's "Gate lamps' light"). A big burning object lights the land with its fire's own light map, see [../physics/fire.md](../physics/fire.md) ([day-and-night.md](../../../world/day-and-night.md#night-lights)) |
| The hand gives off light at night | done | `src/Graphics/HandLight.cpp`; see ../hand/ |
| Villagers go home to bed at night | done | The town wants sleep by the day clock and its villagers go home (see ../villager/daily_routine.md) |
| Night sounds (crickets, owls) replace the day's | done | The sound map by the sky type (`src/Audio/SoundMap.cpp`); see ../audio/ |
| The haze closes in at dusk | done | `sky_type::HazeFactor` in `LandLightTable::Build` (`src/3D/LandLightTable.cpp`) |
| Temperature falls at night and rises by day | done | The climate's hour factor (`src/ECS/Weather/Climate.cpp`); see ../weather/climates.md |
