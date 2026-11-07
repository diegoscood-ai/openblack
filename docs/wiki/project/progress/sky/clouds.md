# Clouds

Seventy puffs of cloud drift with the wind along a track across the island, fading in and out at its ends, with two
huge ones pinned on the horizon. Their shadows pass over the land. Storms bring dark clouds of their own.

**Progress: 19/19 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/sky.md).

## Fair-weather clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new land lays 70 clouds at random along a track, 300 to 500 high, up to 5000 either side | done | `Clouds::Clouds` (`src/3D/Clouds.cpp`, the CRT draws), rebuilt when a land opens (`Clouds::OnLandscapeOpened`, then `CloudSystem::Reset`, `Locator::cloudSystem`); tests `CloudSystem.*` (`test/graphics/test_cloud_system.cpp`, the 420 draws in the original's order) |
| The first two are pinned at the track's ends on the horizon, 300 times the size | done | `src/3D/Clouds.cpp` (`Cloud::pinned`); test `Clouds.LayoutPinsTwoAtTheEnds` |
| Clouds move along the track at 70 units a second of game time, back to the start past its end | done | `Clouds::Update` (wraps past the end), from `CloudSystem::Update` (`src/ECS/Systems/Implementations/CloudSystem.cpp`); tests `Clouds.DriftAlongTheTrack`, `CloudSystem.UpdateMovesTheCloudsWithTheWind` |
| The track lies three eighths of a turn about the middle of the map | done | `Clouds::WorldPosition` (3 pi / 4 about (1280, 1280)); test `Clouds.TurnWithTheWindAboutTheMiddle` |
| Clouds fade in over the first 2000 units of the track and out over the last | done | `Clouds::EdgeAlpha`; test `Clouds.FadeAtTheTracksEnds` |
| Clouds are puffs of mist that shrink edge on, seen from the side | done | The mist draw of `src/3D/Mists.cpp` (the edge-on scale); test `Mists.ShrinkEdgeOn` |
| Clouds animate through frames of the smoke texture | done | `Clouds::AdvanceAnimation`, `frame_anim::MistCell` (`src/3D/FrameAnim.h`); the counter moves by each frame's whole steps, the fraction dropped as v1.42 ([frame clocks](../../../rendering/textures.md#frame-animated-textures-in-openblack), 2026-10-22); test `test/test_mists.cpp` |
| Clouds take their colour by the sky's alignment, in the land's light: none on good land (alpha 0), light grey and half clear for neutral, opaque ochre for evil, never pure white | done | `Clouds::Colour` (by the sky alignment, through the light table's last entry), as [sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original); test `Clouds.ColourFollowsTheSkysAlignment` |
| Clouds stop while the game is paused | done | `Clouds::Update` and `AdvanceAnimation` take the frame's game time step, 0 while paused, as the original's step and mist draw take the game time increment ([sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)); tests `CloudSystem.NothingMovesOrAnimatesWhileTheGameIsPaused`, `GameClockTest.PauseStopsTheTimerKeepsTheFractionAndNoExtraTurn` |
| Clouds are sorted with other translucent things | done | `Renderer::UpdateClouds` sends them to the Z-sorter (`src/Graphics/Renderer.cpp`); checked by the land view shots; test `SkyStage.TheMainPassDrawsTheDomeTheSunAndTheMoonThenQueuesTheClouds`, test `ZSorter.farToNearAndStable` |
| The cloud detail option turns clouds on or off | done | `detail_level::Clouds` (`src/Graphics/DetailLevel.h`); test `test/graphics/test_detail_level.cpp` |
| Clouds' shadows darken the land as they pass | done | `Clouds::StampShadows` (`sclouds.raw` through `land_light::AddStamp`, 40 wide, mode 2, as [haze-and-mist.md](../../../world/haze-and-mist.md#haze-and-land-light-the-common-api)); stamp tests `LandLight.Stamps`, `StampLayout`; the land view shots |
| The cloud shadow option is separate from the clouds option | done | `detail_level::Level::clouds` stands for both the Clouds and the CloudShadows keys; the game's detail table gives both keys the same value at every level it starts with (custom detail is never reached at start-up, [frame.md](../../../rendering/frame.md)), so one field gives the same result; test `DetailLevel.TheCloudsAndTheirShadowsShareOneSetting` |
| The track keeps one fixed direction (three eighths of a turn); it does not follow the climate's wind | done | `Clouds::WorldPosition` (the constant 3 pi / 4), as [sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original); test `Clouds.TurnWithTheWindAboutTheMiddle` |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each natural storm draws its own clouds: up to 16 puffs of mist wandering about its middle at its cloud height | done | `storm_clouds::DrawFrame` (`src/ECS/Weather/StormClouds.cpp`, up to 16 puffs about every storm's centre at its elevation), from `WeatherLoop.cpp`; scenarios `miracles.storm_village`, `miracles.storm_clouds_below`, `examples.storm_lightning`; test `Storm.upToSixteenPuffsAboutTheMiddleOfEachStorm` |
| Storm clouds are darkened by the storm's blackness (each byte x (1 - blackness / 2)) and spread and grow with its radii | done | `storm_clouds::PuffColour` and the puff size from the inner plus outer radius (`src/ECS/Weather/StormClouds.cpp`), as [weather.md](../../../world/weather.md#climate); test `Storm.puffColour` |
| A storm's clouds cast a shadow on the land, stronger with its blackness | done | `storm_clouds::DrawFrame` stamps `sstorm.raw` by (blackness + 0.7) x fade, with the cloud shadow option ([haze-and-mist.md](../../../world/haze-and-mist.md#haze-and-land-light-the-common-api), as the original's `GWeather::DrawClouds`); stamp tests `LandLight.Stamps`; scenario `miracles.storm_village` |
| The storm miracle's clouds gather and drift | done | `src/Particles/Rules/Storm.cpp`; tests `Storm.cloudRamps`, `Storm.cloudMistAtoms`; see [../miracles/storm.md](../miracles/storm.md) |
| Scripts change a storm's number of clouds, darkness and height | done | `script::ChangeCloudProperties` (`src/Magic/Script/CHLWeather.cpp`) to `weather_thing::SetCloudProperties`, as the original's CHANGE_CLOUD_PROPERTIES: the height, the blackness and the number popped in that order, the number cut to a whole one, then the thing's descriptor and its storm updated ([weather.md](../../../world/weather.md#climate)); test `WeatherTest.AScriptChangesAWeatherThingsCloudsAndTheirStorm`; see [../weather/scripted_weather.md](../weather/scripted_weather.md) |
