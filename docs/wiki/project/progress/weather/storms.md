# Storms and thunder

Natural storms are moving weather systems bred by the climates. Within their outer radius they pull the temperature
towards their own and add rain, snow, cloud and wind, at full strength inside their inner radius. Hot or overcast
storms flash with lightning and roll with thunder.

**Progress: 26/27 done, 0 chosen, 0 partial, 0 inferred — 96%**

How the original does it, in our wiki: [Weather and climate](../../../world/weather.md) and [Storm and tornado](../../../magic/storm-and-tornado.md).

## A storm's life

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A storm fades in, lasts a while at full strength and fades out | done | `storms::UpdateAll` (`src/ECS/Weather/Storms.cpp`); tests `WeatherTest.StormFadeAndCalcAtmos`, `StormFadesInOverFadeInTime` (`test/test_weather.cpp`) |
| A storm travels towards its destination at its speed and drifts with the wind | done | `src/ECS/Weather/Storms.cpp`, `climate::ProcessAll`; tests `WeatherTest.AScriptMovesAWeatherThingsStormTowardsThePoint`, `AClimatesStormsDriftWithTheWindWhereTheyAre` (the drift) |
| A dead storm lingers a couple of turns before it is removed | done | `src/ECS/Weather/Storms.cpp`; test `WeatherTest.DeletionTakesTwoTurns` |
| Storms are applied to a cell newest first, so where they overlap the oldest one's temperature has the last word | done | The storm list, newest first (`src/ECS/Weather/Storms.cpp`), as the original's (a new storm goes at the list's head, and the cell's recompute walks it from there); test `WeatherTest.StormsAreAppliedToACellNewestFirst` |
| A storm's strength, radii and cloud height come from its climate's type | done | `climate::CreateStorm` (`src/ECS/Weather/Climate.cpp`); test `WeatherTest.ClimateStormAtFullDesire` |
| Cold storms snow; the snow share falls away above freezing | done | `climate::CreateStorm`; test `WeatherTest.ClimateStormAtFullDesire` |
| Storms are saved and loaded with the game | pending | A saved game's world file is read and its climates made again (`Game::LoadSavedGame`, `ecs::saved_world_load::MakeClimates`; climates.md), but a load makes no storms: the climates come back without their storms, and the world file's weather things are read (`gamesave::WeatherThingRecord`; test `SaveWorldListRecords.EachRecordHasTheGamesLength`) but not made; the writing waits too ([saved games](../../../engine/saved-games.md#openblack); the fork's issues #186 and #187) |

## The weather at a point

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The weather is held on a grid of 128 by 128 cells of 40 units, calm air plus every storm over the cell | done | `src/ECS/Weather/Atmos.cpp`; test `WeatherTest.AtmosGridAndHeight` |
| Cells are worked out when asked and kept until the storms move | done | `src/ECS/Weather/Atmos.cpp` (the frame stamp); test `WeatherTest.AtmosGridAndHeight` (the stamp) |
| Weather heard and seen at the camera is blended between the four nearest cells | done | The smooth sampling in `src/ECS/Weather/Atmos.cpp` (`atmos::GetWeatherSmooth`); test `WeatherTest.TheSmoothWeatherBlendsTheFourNearestCellsAndGivesWayToTheCalmAirHighUp` (nine cases from the exe through the oracle) |
| High above the ground the weather gives way to calm air | done | The smooth sampling above 200 m (`src/ECS/Weather/Atmos.cpp`: towards the calm air by (height - 200) / 4 of 256); tests `WeatherTest.TheSmoothWeatherBlendsTheFourNearestCellsAndGivesWayToTheCalmAirHighUp` (from the exe through the oracle), `AtmosGridAndHeight` |
| The cloud cover at a point darkens the land and sky | done | The overcast at the camera darkens the land's light (`LandLightTable::Build`) and turns the dome towards the haze (`sky_dome::TintOf`); tests `SkyDome.OvercastTurnsTowardsTheHaze`, `SkyDome.SunAndMoonDimThroughAnOvercast`. See ../sky/lighting.md, ../sky/sky_dome.md |
| The snow lying in a cell is part of its weather | done | The cell's snow cover byte is read from the lying snow at its corner (`atmos`, `snow_cover::GridByte`); tests `SnowWeatherTest.TheGridCellTakesHalfTheSnowAtItsCorner`, `WithoutSnowTheGridKeepsNone`. The lying snow itself: snow.md |

## Lightning and thunder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Once faded in, hot or overcast storms flash now and then, at waits drawn from their climate | done | The fork and sheet timers in `src/ECS/Weather/Storms.cpp` start the flash (`flash::Start`); tests `WeatherTest.StormForkEventCallsTheForkCallback`, `StormWithoutForkLightningNeverCallsTheForkCallback` |
| Bright flashes come with thunder, dim ones with a forked bolt | done | The flashes of 1.0 and 0.5 (`src/ECS/Weather/Storms.cpp`); the audio sets the thunder callback, the game's services the bolt one (`src/Locator.cpp`); test `Lightning.BoltsFlashAtHalfStrength` |
| A flash lasts a moment, flickering down between a tenth and half a second | done | `flash::Age`, `flash::Frame` (`src/ECS/Weather/LightningFlash.cpp`); tests `Lightning.FlashFlickersAsItFades`, `BoltsFlashAtHalfStrength` |
| The flash lights the land and sky while the camera is inside the storm | done | `weather::LightningFlashAtCamera` lights the land through the light table and whitens the dome (`sky_dome::TintOf`); tests `Storm.lightningFlashAtCamera`, `LandLightTable.NoonStormWithFlash`, `SkyDome.DarknessDarkensAndFlashWhitens` |
| The flash value the sky's darkening reads decays each frame, so it is always 0 | done | Read in the exe: the atmosphere's frame update takes the frame's seconds off the value only while it is above 0 and stops at 0; it starts at 0 with the atmosphere and nothing else writes it, so its share of the darkness is always 0 and the darkness is the alignment's alone ([sky.md](../../../world/sky.md#sky-type), the darkness); `sky_dome::Darkness` (`src/3D/SkyDome.cpp`), tests `SkyDome.DarknessOfAGoodSky`, `SkyDome.DarknessDarkensAndFlashWhitens` |
| Lightning glows on the ground where it struck | done | The flash's land light stamp (`flash::Frame`, `src/3D/LandColourStamps.cpp`); tests `LandColourStamps.LightningGlowsGreyFromItsCentre`, `LightningGlowFadesCubed` |
| The thunder is one of eleven claps, heard once the sound has travelled to the camera | done | `audio::tags::Thunder`, set as the sheet callback when the audio starts (`src/Audio/Game/AudioSystem.cpp`); tests `SoundTagTest.ThunderIsAClapOfTheRainBankPickedFromTheTicks`, `TheAudiosStartSetsTheStormsThunder` |
| The forked bolt is drawn from the cloud to the ground | done | The fork callback queues the point (`particles::lightning_strike::Queue`, set in `src/Locator.cpp`); `UR_LightningStrike` makes the strike (`src/Particles/Rules/Lightning.cpp`); test `WeatherTest.StormForkEventCallsTheForkCallback` |
| A bolt that reaches the ground strikes what is there | done | A natural storm's fork goes through the strike queue to `UR_LightningStrike` ([fork strike](../../../magic/fireball-and-lightning.md#the-weathers-fork-strike)); with no spell behind it each fork tip applies the weather lightning effect (burn 10000 within 1 m): `particles::lightning::StrikeTip` / `NoSpellTipValues` / `TipMapPosition` in `src/Particles/Rules/Lightning.cpp`; tests `LightningParticles.strikeTipChoosesTheSpellOrTheWeatherLightning`, `LightningParticles.noSpellTipTakesTheWeatherLightningRow`, `LightningParticles.noSpellTipMapPosition` |
| Rain and wind sound under a storm | done | The rain and wind atmos volumes of `src/Audio/SoundMap.cpp` from the camera's weather; see ../audio/; tests `AtmosAudio.MatchesOriginalSoundMap1`, `MatchesOriginalSoundMap2` (recorded on the v1.42 exe) |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each storm draws up to 16 puffs of cloud wandering about its middle | done | `storm_clouds::DrawFrame` (`src/ECS/Weather/StormClouds.cpp`); see ../sky/clouds.md; test `Storm.puffColour`; scenes `examples.storm_lightning`, `miracles.storm_clouds_below` |
| A storm's clouds shadow the land | done | `storm_clouds::DrawFrame` (the storm's shadow stamp); see ../sky/clouds.md; scene `examples.storm_lightning`; test `LandLight.Stamps` |

## Ending storms

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Scripts end every storm within an area at once | done | `script::KillStormsInArea` (`src/Magic/Script/CHLWeather.cpp`), `storms::KillStormsInArea`; test `WeatherTest.KillStormsInArea` |
| The storm miracle lays its own storm over the land, moved and ended by the miracle | done | The miracle's registered storm (`src/Particles/Rules/Storm.cpp`, `src/Magic/Spells/SpellStormAndTornado.cpp`); see ../miracles/storm.md; test `Storm.gatherRegistersOneStorm` |
