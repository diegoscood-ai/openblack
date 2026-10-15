# Storms and thunder

Natural storms are moving weather systems bred by the climates. Within their outer radius they pull the temperature
towards their own and add rain, snow, cloud and wind, at full strength inside their inner radius. Hot or overcast
storms flash with lightning and roll with thunder.

**Progress: 18/26 done, 4 partial — 77%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md).

## A storm's life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A storm fades in, lasts a while at full strength and fades out | done | `storms::UpdateAll` (`src/ECS/Weather/Storms.cpp`); tests `WeatherTest.StormFadeAndCalcAtmos`, `StormFadesInOverFadeInTime` (`test/test_weather.cpp`) |
| A storm travels towards its destination at its speed and drifts with the wind | done | `src/ECS/Weather/Storms.cpp`, `climate::ProcessAll` |
| A dead storm lingers a couple of turns before it is removed | done | `src/ECS/Weather/Storms.cpp`; test `WeatherTest.DeletionTakesTwoTurns` |
| Later storms are laid on top of earlier ones | done | The storm list, newest first (`src/ECS/Weather/Storms.cpp`) |
| A storm's strength, radii and cloud height come from its climate's type | done | `climate::CreateStorm` (`src/ECS/Weather/Climate.cpp`) |
| Cold storms snow; the snow share falls away above freezing | done | `climate::CreateStorm` |
| Storms are saved and loaded with the game | todo | openblack has no saved games. See ../engine/ |

## The weather at a point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather is held on a grid of 128 by 128 cells of 40 units, calm air plus every storm over the cell | done | `src/ECS/Weather/Atmos.cpp`; test `WeatherTest.AtmosGridAndHeight` |
| Cells are worked out when asked and kept until the storms move | done | `src/ECS/Weather/Atmos.cpp` (the frame stamp) |
| Weather heard and seen at the camera is blended between the four nearest cells | done | The smooth sampling in `src/ECS/Weather/Atmos.cpp` |
| High above the ground the weather gives way to calm air | done | The smooth sampling above 200 m (`src/ECS/Weather/Atmos.cpp`) |
| The cloud cover at a point darkens the land and sky | partial | The overcast at the camera darkens the land's light (`Clouds::WeatherOvercastAtCamera`, `LandLightTable::Build`); the sky's dome is not tinted. See ../sky/lighting.md |
| The snow lying in a cell is part of its weather | partial | The snow cover byte is there but stays 0: the lying snow is not ported; test `WeatherTest.FallingSnowIsNotSnowOnTheGround` |

## Lightning and thunder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once faded in, hot or overcast storms flash now and then, at waits drawn from their climate | done | The fork and sheet timers in `src/ECS/Weather/Storms.cpp` start the flash (`flash::Start`) |
| Bright flashes come with thunder, dim ones with a forked bolt | done | The flashes of 1.0 and 0.5 (`src/ECS/Weather/Storms.cpp`); the audio sets the thunder callback, the game's services the bolt one (`src/Locator.cpp`) |
| A flash lasts a moment, flickering down between a tenth and half a second | done | `flash::Age`, `flash::Frame` (`src/ECS/Weather/LightningFlash.cpp`); tests `Lightning.FlashFlickersAsItFades`, `BoltsFlashAtHalfStrength` |
| The flash lights the land and sky while the camera is inside the storm | partial | `weather::LightningFlashAtCamera` lights the land through the light table; the sky's dome takes no flash. See ../sky/sky_dome.md |
| Lightning glows on the ground where it struck | done | The flash's land light stamp (`flash::Frame`, `src/3D/LandColourStamps.cpp`) |
| The thunder is one of eleven claps, heard once the sound has travelled to the camera | done | `audio::tags::Thunder`, set as the sheet callback when the audio starts (`src/Audio/Game/AudioSystem.cpp`) |
| The forked bolt is drawn from the cloud to the ground | done | The fork callback queues the point (`psys::lightning_strike::Queue`, set in `src/Locator.cpp`); `UR_LightningStrike` makes the strike (`src/Particles/Rules/Lightning.cpp`); test `WeatherTest.StormForkEventCallsTheForkCallback` |
| A bolt that reaches the ground strikes what is there (unconfirmed for natural storms) | done | With no spell behind it each fork tip applies the weather lightning effect (burn 10000 within 1 m): `psys::lightning::StrikeTip` / `NoSpellTipValues` / `TipMapPosition` in `src/Particles/Rules/Lightning.cpp`; tests `Lightning.strikeTipChoosesTheSpellOrTheWeatherLightning`, `Lightning.noSpellTipTakesTheWeatherLightningRow`, `Lightning.noSpellTipMapPosition` |
| Rain and wind sound under a storm | done | `src/Audio/Services/SoundMap.cpp`; see ../audio/ |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each storm draws up to 16 puffs of cloud wandering about its middle | done | `storm_clouds::DrawFrame` (`src/ECS/Weather/StormClouds.cpp`); see ../sky/clouds.md |
| A storm's clouds shadow the land | done | `storm_clouds::DrawFrame` (the storm's shadow stamp); see ../sky/clouds.md |

## Ending storms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts end every storm within an area at once | done | `script::KillStormsInArea` (`src/Magic/Script/CHLWeather.cpp`), `storms::KillStormsInArea`; test `WeatherTest.KillStormsInArea` |
| The storm miracle lays its own storm over the land, moved and ended by the miracle | done | The miracle's registered storm (`src/Particles/Rules/Storm.cpp`, `src/Magic/Spells/SpellStormAndTornado.cpp`); see ../miracles/storm.md |
| A storm can be forced over the whole island, and storms ended, from the debug window | n/a | openblack-only, `src/Debug/Weather.cpp`; see ../debug/ |
