# Clouds

Seventy puffs of cloud drift with the wind along a track across the island, fading in and out at its ends, with two
huge ones pinned on the horizon. Their shadows pass over the land. Storms bring dark clouds of their own.

**Progress: 18/19 done, 1 partial — 97%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Fair-weather clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A new land lays 70 clouds at random along a track, 300 to 500 high, up to 5000 either side | done | `Clouds::Clouds` (`src/3D/Clouds.cpp`, the CRT draws), rebuilt when a land opens (`Clouds::OnLandscapeOpened`, then `CloudSystem::Reset`, `Locator::cloudSystem`) |
| The first two are pinned at the track's ends on the horizon, 300 times the size | done | `src/3D/Clouds.cpp` (`Cloud::pinned`) |
| Clouds move along the track at 70 units a second of game time, back to the start past its end | done | `Clouds::Update` (wraps past the end), from `CloudSystem::Update` (`src/ECS/Systems/Implementations/CloudSystem.cpp`) |
| The track lies three eighths of a turn about the middle of the map | done | `Clouds::WorldPosition` (3 pi / 4 about (1280, 1280)) |
| Clouds fade in over the first 2000 units of the track and out over the last | done | `Clouds::EdgeAlpha` |
| Clouds are puffs of mist that shrink edge on, seen from the side | done | The mist draw of `src/3D/Mists.cpp` (the edge-on scale) |
| Clouds animate through frames of the smoke texture | done | `Clouds::AdvanceAnimation`, `frame_anim::MistCell` (`src/3D/FrameAnim.h`); test `test/test_mists.cpp` |
| Clouds are white for good, grey for neutral and dark orange for evil, in the land's light | done | `Clouds::Colour` (by the sky alignment, through the light table's last entry). Our wiki differs: on good land the clouds have alpha 0, so they are not seen at all; neutral ones are light grey and evil ones opaque ochre ([rendering.md](../../bw1-notes/rendering.md#sky-sun-moon-and-clouds-original)) |
| Clouds stop while the game is paused | done | `Clouds::Update` and `AdvanceAnimation` take the game time step, 0 while paused |
| Clouds are sorted with other translucent things | done | `Renderer::UpdateClouds` sends them to the Z-sorter (`src/Graphics/Renderer.cpp`) |
| The cloud detail option turns clouds on or off | done | `detail_level::Clouds` (`src/Graphics/DetailLevel.h`) |
| Clouds' shadows darken the land as they pass | done | `Clouds::StampShadows` (`sclouds.raw` through `land_light::AddStamp`) |
| The cloud shadow option is separate from the clouds option | partial | `detail_level::Level::clouds` stands for both the Clouds and the CloudShadows keys |
| The track follows the climate's wind | done | The track's direction is the fixed 3 pi / 4 (`Clouds::WorldPosition`). Our wiki differs: the original's track angle is a constant and does not follow the wind ([rendering.md](../../bw1-notes/rendering.md#sky-sun-moon-and-clouds-original)) |

## Storm clouds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each natural storm draws its own clouds: up to 16 puffs of mist wandering about its middle at its cloud height | done | `storm_clouds::DrawFrame` (`src/ECS/Weather/StormClouds.cpp`, up to 16 puffs about every storm's centre at its elevation), from `WeatherLoop.cpp` |
| Storm clouds are greyed by the storm's overcast and grow with its radius | done | `storm_clouds::PuffColour` and the puff size from the radius. Our wiki differs: the puffs are darkened by the storm's blackness, not its overcast ([day-night-weather.md](../../bw1-notes/day-night-weather.md#climate)) |
| A storm's clouds cast a shadow on the land, stronger with its overcast | done | `storm_clouds::DrawFrame` stamps `sstorm.raw` by (blackness + 0.7) x fade. Our wiki differs: the shadow grows with the blackness, not the overcast ([day-night-weather.md](../../bw1-notes/day-night-weather.md#climate)) |
| The storm miracle's clouds gather and drift | done | `src/Particles/Rules/Storm.cpp`; see [../miracles/storm.md](../miracles/storm.md) |
| Scripts change a storm's number of clouds, darkness and height | done | `script::ChangeCloudProperties` (`src/Magic/Script/CHLWeather.cpp`); see [../weather/scripted_weather.md](../weather/scripted_weather.md) |
