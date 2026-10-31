# Weather and climate

## Summary

The weather has two layers: a grid of the weather made by the registered storms (the 3D engine's atmosphere, 128 × 128
cells of 40 m) and the game's climates, which add temperature and wind and create natural storms. Everything else in
the game asks the climate for the weather at a point. The scripts make weather objects, and the rain and snow are drawn
from the same values.

openblack ports both layers, the queries, the script weather objects and the drawn rain; the snow and a few draw
details are partly ported. The storms and the tornado themselves are on
[Storm and tornado](../magic/storm-and-tornado.md).

## The original

The day and the night, the sky and its clouds are on [Day and night](day-and-night.md) and [Sky](sky.md).

### Climate

The game weather (climates, storms, rain and its drawing) is ported and described below, in
[Weather and climate](#weather-and-climate). What remains here are the sky notes from the first reading.

- Sky clouds (`CloudInSky`): done, see [Sky: sun, moon and clouds (original)](sky.md#sky-sun-moon-and-clouds-original) (placement, colour by time and alignment,
  sky alignment smoothed 0.001/ms). What depends on the weather goes through `Clouds::WeatherOvercastAtCamera()`
  (byte 3 of `weather::atmos::GetWeatherSmooth` at the camera × 0.01; 0 with a clear sky).

Water and weather: **there is no wet ground** in the original; the sea only changes with the weather through the overcast cap of the
light table and the lightning flash (`LandLightTable::Build`). The overcast has a single source,
`Clouds::WeatherOvercastAtCamera()` (reads `weather::atmos::GetWeatherSmooth` at the camera); `3D/SkyWeather` only gives the flash (`weather::LightningFlashAtCamera` from
`ECS/Weather/LightningFlash` at the camera position).
- **Storm clouds**: `GClimate::CreateStorm` 0x772E00 → `GWeather` 0x83F590 → `DrawClouds`
  0x83FC90, a group of up to 16 balls (confirmed: xref immediates; 8 by default, `CHANGE_CLOUD_PROPERTIES` changes number, blackness and height) at
  lx, lz ∈ −1..1, ly ∈ −10..10 + height; every 400 frames a new target (ly 0..20 + height), 1/400 of the way
  per frame; world X = R·lx/2 + cx, Z = R·lz/2 + cz, Y = ground + ly with R = radius(t) + radius2; size R·2·Random(0.01,
  0.015); colour [0xFA26A4]·(1 − 0.5·blackness), alpha·intensity·0.75, with haze; they are not drawn with alpha ≤ 5. They are the
  only cloud groups in the original. Already ported: [The flash and the clouds of the registered storms](../magic/storm-and-tornado.md#the-flash-and-the-clouds-of-the-registered-storms).
  - A puff's colour scales each RGB byte by (1 − 0.5 × blackness) only when the blackness is above 0
    (0x83FF56..0x840000).
  - The puffs get the haze of fn_007FEB30 with a grey specular min(255, ftol(flash f3 [storm +0x8C] × 127 [0x8C4A00]))
    in all four bytes (0x83FFEE..0x840027), put in +0x50 by SetColorSpecular and drawn by the mists' effect branch: the
    storm's own flash lights its clouds.
  - The storm's shadow s = (blackness + 0.7) × fade is stamped with 1 when s ≥ 1, and otherwise only when s > 0.01
    ([0x8C5840]); only with the "CloudShadows" option [0xC381F4]: fn_0086CFF0(+0xA0, 0xEE9D3C, 40, 1, s, 2, 0)
    (0x840069..0x8400C6).

The weather items first read here (`GClimate`, `LH3DAtmos::Render3D` with rain and snow by 80×80 tiles, lightning,
overcast weather in the light table and the haze) are described in [Weather and climate](#weather-and-climate);
what is left is in [Pending](#pending).

- fn_0083F6F0 -> fn_0083F590: a new GWeather at the head of the list (speed 1, target = its position)
- 0x83724C..0x837278: fn_0086CFF0(pos, 0xED92F0, [0xEDC2F0] = 64, 1, f3, 1, 0): the land light stamp, centred, mode 1 (land_light::AddStamp); the bitmap of fn_00837110 (rows of 0xC0 bytes, the three channels equal)
- The colour of a puff (0x83FF56..0x840000): the land light table's base colour [0xFA26A4], each RGB byte x (1 - 0.5 x blackness) (ftol) when the blackness is above 0, and the alpha ftol(fade x 0.75 x the base's alpha byte)

### Weather and climate

Two layers, as in the original: **LH3DAtmos** (the 3D engine) keeps a grid of the weather made by the registered
*storms*, and **GClimate** (the game) adds the temperature and wind of the *climates* to it and creates natural storms.
All queries from the rest of the game go through `GClimate::ComputeWeather`.

#### `WeatherInfo`

It is returned in `edx:eax`, and the same layout is a grid cell, the tail of an LH3DStorm (+0x48) and the
result of `ComputeWeather`: `temperature` (degrees), `rain` (0..100), `snow`, `overcast`, `windX`, `windZ`
(× 1/8 = m/s), `snowCover` and `stamp` (the frame in which the cell was computed).

The arithmetic is on signed bytes, and the original mixes two forms: the temperature **wraps** (`add cl, al`,
`WrapAdd`) and the rest is **clamped** to -128..127 (`ClampAdd`). The interpolation is `a + ((b - a) × w >> 8)` with
arithmetic shift (`LerpByte`).

#### LH3DAtmos: the 40 m grid

- 128 × 128 cells of 40 m (0xEDC350) = 5120 m, exactly the map. Outside the grid the *ambient weather* is used
  (0xEDC348), which in a game is all 0 (it is only written by `LHInetWeather` and the saved game).
- A cell is recomputed **on demand** (`fn_00834EE0` / `fn_00834E20`) when its `stamp` is not the current frame
  (0xEDC340): it starts from the ambient weather and adds each registered storm at the **corner** of the cell
  (`ix × 40, 0, iz × 40`), not at the requested point. That is why the rain comes out in 40 m steps.
- The grid 0xEDC350 (0x20000 bytes) is allocated by fn_00835AD0, which also loads `sstorm.raw` (40 × 40 grey, 0x640
  bytes) into 0xEE9D3C at 0x835E15: the storms' land shadow.
- A cell recompute (`fn_00834E20`) starts from the ambient weather (`fn_00834E10`), applies each storm (vt+0xC, list
  [0xEEA37C], next +0x3BC), stamps [0xEDC340] into byte 7 (0x834EAD..0x834EB8) and then **overwrites** byte 6
  (snowCover, 0x834EC5; it is not added) with `fn_00834DD0(corner)` = clamp(ftol(`fn_0086CA80`(x, z) × 0.5), −128, 127)
  (0x834DE4..0x834E04; `ftol` truncates towards zero), 0 without [0xEDC344]. `fn_0086CA80` (reached through
  `fn_0086CB80(p)`) filters the 128 × 128 float grid bilinearly (cells of 40, × 0.025; 0 outside [0, 127) in x or z).
  `SnowCover::Init` 0x86C960 zeroes it (0x10000 bytes) and loads `data\WeatherSystem\snowmap.raw`, with no draws.
  raffclar's tree (`WeatherSystem.cpp`) has the filter, the bounds and the overwrite, but **not the × 0.5**: its
  lying snow is twice as deep in the grid (tornado dust `>= k_SnowDust` and the debug "Lying" bar; `IsSnowCoveredAt`
  is unaffected).
- The cell lerp fn_00835620 (and the smooth sampling) lerps bytes 0..6 with a wrapping add and keeps the stamp of the
  first cell.
- A new land (`InitStaticsValues` 0x54A8CC..0x54A907): every storm deleted (0x54A8D1), the grid and the ambient
  weather zeroed, the frame back to 1.
- `GClimate::ComputeWeather` 0x771640 is reached through `GClimate::GetWeather` 0x771490; only the camera asks for the
  smooth sampling.
- `LH3DAtmos::UpdateGame` 0x8356E0 (the first call of `GGame::ProcessTurn`, with the visual time and 0.1 s) updates
  the storms and advances the frame; on overflowing 256 it clears all stamps and goes back to 1. The sun position that
  it also computes (0xEDD378: 4000, cos(t·π/12) × 1100 − 150, sin × 800) is for lighting, it is not used here.
- `LH3DAtmos::GetWeather` 0x834F80: the cell of (x, z) and then the height: **above 50 m the temperature drops
  0.075 per metre**, and above 200 m it is a fixed −11 (`add al, 0xF5`).
- `LH3DAtmos::GetWeatherSmooth` 0x835180: bilinear between the four cells in steps of 1/256; above 200 m
  it tends towards the ambient weather with weight `(height − 200) / 4` (cap 256) and then applies the same drop with height. It is the
  one used by the camera (clouds) and `GetWindAt(p, true)`; all the GClimate getters ask for the non-smoothed one.
- `SnowCover` (0xEDC344, the snow accumulated on the ground, `fn_0086CB80 × 0.5`) is ported as the `snowSystem`
  (`3D/SnowCover`, `snow_cover::GridByte`): the cell recompute overwrites `snowCover` with the × 0.5 and the truncation.
  With no snowing storm the grid stays 0, and so does every cell's byte.

- Ftol(snow x snowCoverRate x fade): with SnowCover (0xEDC344) fn_0086C2D0 lays snow in the inner/outer radius at amount x seconds x 0.03. Ported: `snow_cover::StormSnowPerTurn` and `SnowSystem::AddStorm`, from each snowing storm (`ECS/Weather/Storms.cpp`), with tests (`SnowCover.StormsSnowBySnowRateAndFade` and its neighbours). Each storm and each melted band also counts the snow's change for its land block (`snow_cover::BlockChanges`, the storm's edge rule at 0x86C4C0..0x86C50C), and after each band fn_0086C5D0 lists the blocks whose change passed 96 (0x9A3BE4) among the 16 that changed most (0xFA277C / 0xFA27BC, count 0xFA291C), marking a block to be painted again when the full list evicts it or at the first band's flush: `snow_cover::MarkChangedBlocks` (tests `SnowCover.TheMeltListsTheBlocksItsSnowChangedAsTheOriginal`, `StormsLaySnowAsTheOriginal`, pinned to the original run natively). An outer radius not past the inner one is taken 0.01 cells past it (0x86C31E, [0x8C5840]).
- Sstorm.raw (40 x 40 grey, 0x640 bytes) that fn_00835AD0 0x835E15 loads into 0xEE9D3C: the storms' land shadow

#### GClimate: the climates

List `g_game+0x205CF4`, the newest first; the **world climate** (id 0, `g_game+0x250534`) is at (2560, 2560)
with radius 5120 and type `WORLD`. `ComputeWeather` creates it if it is missing, so in practice it always exists.

- They are created by the map script with `CREATE_WEATHER_CLIMATE(id, info, "x,z", r1, r2)` (`fn_00771300`, case 60): id 0 makes
  the world one (replacing the previous one and ignoring the rest of the arguments), any other id a local one with the radii
  sorted and `maxStorms = int(r2 × 0.001 + 1)`. `CREATE_WEATHER_CLIMATE_RAIN/TEMP/WIND` then set the rest.
  **Land1 has four** (lines 2270..2285 of `Scripts\Land1.txt`): the world one with 10.8 degrees and wind (24, 0), two
  warm ones of 37.8 degrees at (2157, 2425) and (1740, 3145), and one **very cold one of −35.2** at (2701, 2568) with wind (40, 0).
- `GClimateInfo` (7 rows of info.dat) gives per season `rainMin/Max`, `tempMin/Max` and `windMin/Max`. The season comes from
  `GGameInfo::GetSeason`: 0 spring (day 79), 1 summer (171), 2 autumn (263), 3 winter.
- **Temperature** (`fn_00773ED0` / `fn_00773F40`, every turn): the target is
  `hourFactor[hour] × monthFactor[month] × (max − min) + min`, with the tables 0xC249D8 (0.5 at 0 h, 1.5 at 16 h) and
  0xC249A4 (1.0 in July, 0.1 in January; **February is 0**, as in the exe). The temperature approaches the target by
  `|int(target)| × 0.1` degrees per turn (the 0.1 a double [0x8C9D40], the product rounded once to a float at the
  game's 24-bit FPU, 0x773FBD..0x773FC9 / 0x773FE2..0x773FEC), so it **never converges: it oscillates** around it (in Land1 the script stores 10.8
  with target 12).
- **Rain** (`fn_00773D60`, once per game day): if it is already raining it only counts the days; if not, the desire grows
  with a roll of `rainMin × 0.02` and one of `rainMax × 0.02` and is clamped to 1. The month, hour and nature terms of `GClimateRainInfo`
  are 0 because that table **is not in info.dat** (0xDCB8D0 stays zeroed).
- **Wind** (`fn_00774AA0`, per day): from the season's band in the `windAngle` direction. The weight is
  `int(exp(−((desire − 0.5) × 5)²))`, which is only 1 with a desire of exactly 0.5, so in practice it is always
  `min + max`.
- **One game day** is 36000 turns per year / 365.25 = **98.56 turns** (9.9 s). The land starts on 5 May
  1998 at 18:05:30 (`GGameInfo::GGameInfo` 0x557730), that is, on day 125.75 of the year.
- **Calendar**:
  - `_SecondsInDay` 0x8DF8E0 = 86400 and `_NumDaysInYear` 0x8DF8DC = 365.25.
  - The month lengths 0xBEC948 are made cumulative by the `GGameInfo` ctor 0x557730; **October has 30 days** in the
    original, 364 days in all.
  - `GGameInfo::GetDaysFromStart` 0x557940 = (turn + start) × (+0x18) / 86400, without the whole years of the start
    date (the day of the year is the same, they are multiples of 365.25 days).
  - fn_00557960: the month 1..12 (the first whose cumulative day count is above the day of the year; 12 past the last);
    fn_005579C0: the day of the month from 0.
  - fn_00557810 builds the start from `SetStartDate(1998, 5, 5)` and `SetStartTime(18, 5, 30)`: the 120 days before May
    from the table, the day of the month (5, added whole), then hours, minutes and seconds, in turns. The map commands
    that change the date (GSetup 0x714D57 / 0x714D81) are not used by the original lands.
- Climate fields: +0x68 storm elevation 500 (ctor 0x77113E, fn_00771170 0x7712C6); +0x6C fall speed = windMax[season]
  × 1/30 [0x980518] + 0.5 [0x8AB260] (0x77114B); +0x70 = 1.0, meaning unknown: saved and loaded, never read (0x771137, 0x7712B6); +0x74..+0x80 the
  lightning ranges of its hot storms, 5 and 60 s (0x77111B..0x771134, 0x7712A6..0x7712C3).
- A climate, local (fn_00771170) or the world's (0x771020), starts from its info for the season (`GetSeason`):
  `fn_00773D30`(rainMin [info +0x40 + season × 4], rainMax [+0x50 + season × 4]) sets the desire (+0x34) **to rainMin**
  and the raining days (+0x3C) to int(rainMin × 100); its second argument, rainMax, is never read (after its
  `push esi` both reads, 0x773D30 and 0x773D45, are of `[esp + 4]` and `[esp + 8]`, the same first argument). The
  oracle gives desire 0.29 and 29 days for (0.29, 0.75), and 0.57 / 57 for (0.57, 0.1). `fn_00773ED0`(tempMin [+0x60],
  tempMax [+0x70]) sets the temperature and its target (+0x44, +0x48) to `hourFactor[hour] × monthFactor[month] ×
  (max − min) + min` (oracle: hour 14, July, −5..20 gives 19; hour 15, September, −35..−32 gives −32.27).
  fn_00774A90 does nothing, so there is no wind until the script sets it. Every land script sets the rain and
  temperature of each of its climates right after creating it (`CREATE_WEATHER_CLIMATE_RAIN` / `_TEMP`), so these
  starts last only where a land has no climate lines: the world climate made on demand (`LandT.txt` and the
  playgrounds without them).
- Script setters: `CREATE_WEATHER_CLIMATE_RAIN` 0x717250 → 0x773200 (id 0 = the world climate, made if missing);
  `_TEMP` 0x7172A2 → 0x773290 writes +0x44 / +0x48 and reads the world climate **without making it** (id 0 needs it to
  exist already); `_WIND` 0x7172E0 → 0x7732D0 writes +0x4C..+0x54.
- Globals: last day 0xDCB8B8, last season 0xDCB8BC, the season the climates use 0xDCB8C0 (changed on the first day of
  a new season), next id 0xC2475C. A new land (`InitStaticsValues` 0x54A829..0x54A849): no climate, the climate system
  and storm creation on, day, season and id counters back to 0 / 0 / 1.
- `GClimate::ComputeWeather` 0x771640:
  1. the grid at the point (smoothed or not);
  2. the temperature and wind **of the world climate** as the base;
  3. **plus each climate in the list, the world one included again**, with the radius weight (1 inside the inner one, 0 at
     the outer one). It is not a copying oversight: the original reads 0x250534 and then walks the complete list, so
     **the world climate counts twice**. That is why Land1 gives 20 degrees on the plain (9 + 9) and not 10.
  4. the result is added with clamping to the grid bytes.
- `GClimate::ProcessAll` 0x771BE0 (turn, after the scripts) and `fn_00772330` per climate: updates the temperature;
  its storms drift with the grid wind (× 0.01 m per turn); on a new day, a storm that left
  its climate (or, for the world one, that entered any climate) jumps to the fade-out stretch and the desire goes back to 1; and if
  it has already rained enough this season it also fades out, and the desire starts again (fn_00773D50). "Rained
  enough" is the raining days × the double 0.01 [0x8C7620], the product rounded once to a float by the 24-bit FPU
  (`fild` + `fmul qword`, 0x772582 / 0x772E09 / 0x772FCB), against the season's rainMax (rainMin for the life): so 30 days
  are 0.3 and spend a rainMax of 0.3 (oracle on `CreateStorm`: (0.3, 30), (0.1, 10), (0.15, 15) have rained enough). Then, on a new day, rain and wind, and with desire 1 a
  new storm.
- `GClimate::CreateStorm` 0x772E00: `FindWhereToCreateStorm` 0x772BE0 chooses the place (the world one, a random 10 m cell
  out of the 512, drawn again, up to 20 tries in all, only while no climate's centre (the list g_game +0x205CF4, the
  world's own among them) is within the creating climate's own outer radius (`fcomp [ebp+0x24]` at 0x772D9C, `ebp` =
  this from 0x772BE6) and the place is off the island; openblack the same, since 2026-10-25, before which it took each
  other climate's radius: the map's water test, compared with 1, answers 1 only outside the 512 × 512 cells or in a cell of no
  land block, since a cell's water bit is 0x10; a local one, at `r² × innerRadius` in a random direction, drawn again
  only while it is off the island). The size is `rand(1000)` (world) or the distance to the centre, clamped
  to 160..900; the outer one is `int(size × 1.1)`; the life `(rainMax × 100 − rainingDays) × 10` s with a minimum of 20;
  the elevation 500. The life's clamps are on one path (read 2026-10-25): below 20 [0x8C7658] it becomes 20
  (0x773001..0x77300E; a NaN too, as `test ah, 1` takes it as below), then, only when it is above 0, below 8
  [0x8C2C70] it becomes 20 again (0x773140..0x773162) and the storm is made; so the second clamp never fires and the
  storm is always made, as openblack's `climate::CreateStorm` does in the same order. It is placed with fn_0083F4A0(pos, inner, outer) at its 10 m cell at a height of 300
  (0x43960000, 0x772F5B) and fades in over 10 s (0x772FAF). The distances of `FindWhereToCreateStorm` (0x772D86 /
  0x772D97), `CreateStorm` (0x772ED6, truncated) and fn_00772330 (0x7724E2 / 0x77254C) are
  `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0. The blackness is `exp(−((t − 30) / 15)²)`, and above 30 degrees it is an electrical storm with the
  climate's lightning. **Below 0 degrees it is pure snow**; above, the snow part is `exp(−(t × 0.2)²)`
  (all rain beyond about 10 degrees) and the rest rain.
- `PAUSE_UNPAUSE_CLIMATE_SYSTEM` and `PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM` (0xC24759 / 0xC24758) switch off the
  update and the creation. The natives are 0x6FF4E0 (it gates the temperature, rain and wind updates) and 0x6FF500.

- GGameInfo::GetDaysFromStart 0x557940: (turn + start) x (+0x18) / 86400, without the whole years of the start date (the day of the year is the same: they are multiples of 365.25 days)
- +0x74..+0x80: 5 / 60 (0x40A00000 / 0x42700000) stored at 0x77111B..0x771134 and 0x7712A6..0x7712C3
- fn_00771300 (CREATE_WEATHER_CLIMATE, case 60 0x7171F5): id 0 makes the world climate (GClimate(0) 0x771020, which replaces the old one and ignores the rest), any other a local one (fn_00771170: radii sorted, the season's rain and temperature). Both go to the head of the list.
- CREATE_WEATHER_CLIMATE_RAIN (0x717250 -> 0x773200): the four values; id 0 = the world (made if missing)
- CREATE_WEATHER_CLIMATE_WIND (0x7172E0 -> 0x7732D0): +0x4C..+0x54
- PAUSE_UNPAUSE_CLIMATE_SYSTEM 0x6FF4E0 (0xC24759): the temperature, rain and wind updates
- PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM 0x6FF500 (0xC24758)
- 0xDCB8C0: the season the climates use (changed on the first day of a new season)
- GUtils::GetDistance(LHPoint, LHPoint) 0x74CDE0 = hypotenuse(float, float) 0x74F6C0, the table root (FindWhereToCreateStorm 0x772D86 / 0x772D97)
- 0x41200000 (0x772FAF)

#### Queries

All of them on `ComputeWeather` at an LHPoint (x, z in metres and the **absolute height** in y, which is what lowers the
temperature):

| Query | Original | Note |
|---|---|---|
| `GetMaxRainingOrSnowingAt` | 0x771600 | `max(rain, snow)`; fire cools with `1 + 0.01 ×` this |
| `GetRainAt` / `GetSnowAt` | 0x771570 / 0x7715B0 | 0 as long as there is no world climate |
| `IsRainingAt` / `IsSnowingAt` / `IsSnowCoveredAt` | 0x7714B0 / 0x7714F0 / 0x771530 | the byte > 0 |
| `GetTemperatureAt` | `GClimate::GetTemp` 0x771A80 | it is **not** the ambient temperature of fire, which is the constant 24.7 of `MapCoords::GetTemperature` 0x605CC0 |
| `GetWindXAt` / `GetWindZAt` | fn_00771AB0 / fn_00771AE0 | the bytes |
| `GetWindAt` | fn_00771B10 | `(windX/8, 0, windZ/8)`; read by fire, `UR_CloudMoverNew` and the bounce of the fireballs |

The first five check beforehand that the world climate exists (they return 0 / false if not); the others create it.

#### CHL weather objects

`CREATE` of a `SCRIPT_OBJECT_TYPE_WEATHER_THING` (type 15) creates a WeatherThing with a storm made from
`GWeatherInfo[subtype]` (inner radius 100, outer 300, life 100 s, clouds at 500, and the temperature, humidity, snow,
overcast and wind of the row as bytes). It keeps a **copy** of the descriptor; the natives edit it and `UpdateStats`
0x774370 dumps it into the storm (reloading the lightning timers): `CHANGE_WEATHER_PROPERTIES` (123),
`CHANGE_LIGHTNING_PROPERTIES` (124), `CHANGE_TIME_FADE_PROPERTIES` (125), `CHANGE_CLOUD_PROPERTIES` (126).
`WeatherThing::ProcessWeatherThings` 0x7741A0 runs every turn: if its storm is no longer there it forgets it, with
`SetAffectedByWind` the storm drifts with the wind of `ComputeWeather` × 0.01, and the object moves with it.

`CREATE_WEATHER_STORM` from the map script (case 0x717328) creates a storm of a climate. Its third string
(`"overcast,snow,temp,rain,windX,windZ"`) is read with `%d` **over the bytes** of the descriptor, 4 bytes per value,
so each value overwrites the next three: only temperature, rain and wind survive (snow and overcast end up as
0 or −1). No original land uses it.

- `WeatherThing` is 0x88 bytes, in the list g_game +0x205C54 (next +0x80), newest first; its descriptor copy is at
  +0x28; +0x7C is `SetAffectedByWind` 0x55DF20.
- CHL `CREATE`: GScript 0x6F151E → 0x7741F0 → ctor 0x774030(pos, 100, 300, 100, &GWeatherInfo[subtype]); fn_00770EA0
  writes the info's temperature, wetness, snowFall, overCast and wind as the storm's bytes at LHPoint(pos, land + y);
  fn_00770F10 makes the storm at once as a 0x3C8 object (fn_00770E50 sets its +0x3C0 to 0).
- `WeatherThing::Process` 0x774230 returns 1 while it has a storm; the drift is fn_00771A30 (`ComputeWeather`'s wind at
  the storm × the factor); the thing's position is the storm's x, z as MapCoords (ftol × 6553.6, 0x774280..0x7742A2),
  y 0; the storm's +0x3C0 (the weather thing's storm is a 0x3C8 `GWeather`, fn_00770F10 → 0x770E50, the plain
  storm's 0x3C0 object, fn_0083F6F0, plus that field) takes bit 10 of the thing's flags +0x24 (controlled by a script, 0x7742AC..0x7742C1; no
  reader of that field in the exe); the thing copies the descriptor back. A thing whose storm is gone forgets it (+0x78
  = 0) and, when `IsInScript` (vt +0x448) is false, is deleted (`ToBeDeleted` vt +0xC, 0x77424A..0x77425D). `ProcessWeatherThings` runs after `GLandAlignement::UpdateTime`.
- `ProcessWeatherThings` sets 0xD016CD to 1 while some thing has a storm and to 2 when none (its reader is not ported).
- Natives: fn_00774400 `CHANGE_WEATHER_PROPERTIES` (temperature in whole degrees; rain, snow and overcast × 100; fall
  speed), fn_00774460 `CHANGE_TIME_FADE_PROPERTIES` (lifeTime, fadeInTime), fn_00774500 `CHANGE_CLOUD_PROPERTIES`
  (blackness, number of clouds as an int, elevation), fn_00774520 `CHANGE_LIGHTNING_PROPERTIES` (sheet and fork
  ranges).
- `WeatherThing::SetMovement` 0x774480 writes the wind bytes as clamp(v × 8, −127, 127); fn_00774550 / fn_00774580 set
  the storm's target and speed. Its only caller is SET_HEADING_AND_SPEED (`GScript::SetVelocityHeadingSpeed` 0x6FA8D3,
  [the velocity](../scripts/script-commands.md#set_heading_and_speed-127)); openblack: `magic::script::SetWeatherHeadingAndSpeed`.

- WeatherThing::SetMovement 0x774480: the wind bytes, clamp(v x 8, -127, 127)
- fn_00774550 / fn_00774580: the storm's target and speed
- LH3DStorm (0x50), defaults of its constructor fn_0083F3F0. Names from the CHL setters (CHANGE_*_PROPERTIES, WeatherThing 0x774400..0x774520).

#### The drawn rain

Inside the temple the original's draw (`DoCitadelDraw` 0x555410) calls `LH3DAtmos::Update3D` (0x555422) but not
`Render3D` (its only caller is `GGame::Process3dEngine` 0x54E0EB), so the rain is not drawn there and, its "drawn since"
flag never set, its streaks are not stepped (openblack: `rain::FrameTakesTiles`); the same holds for the snow's
flakes (`snowfall::FrameTakesTiles`).

`LH3DAtmos` has a rain object (`fn_00833DA0`) with **128 streaks** of 0x1C bytes. Each streak (`fn_00833D10`) is a
line from `(x, −50, z)` to `(x + dx, elevation, z + dz)` around the centre of the tile, with `x, z` in ±80, the
slant `dx, dz` in ±15, a texture offset 0..1 and a speed of 0.1..0.2 turns per second. The u goes
from the offset to offset + 1 (the texture repeats across the width) and the v is fixed, 0x3F010000 = 0.50390625, the
row 129 of `Data\Textures\atmos.raw`, which is a row of white strokes.

- `LH3DAtmos::Update3D` 0x8357A0 (per frame): the storm **closest to the camera** sets the elevation and the
  fall speed (160 and 1 with no storm); both approach by 0.3 of the way per frame and are clamped to
  40..640 m and 0.3..5. Then, *if it was drawn in the previous frame* (0xEDC300), the streaks advance: the phase rises 2.4
  per second and on overflowing the streak is placed again.
- The 128 streaks (0x1C bytes each) are at 0xEDC358; the elevation is 0xC38E10 (160 at start) and the fall speed
  factor 0xC38E14 (1 at start).
- fn_00833D10 places a streak with 7 draws of `Random` 0x81D180 (the CRT `rand()`, not GRand) (0x833D1D..0x833D8F);
  the step fn_00833EA0 makes 4 more draws per reborn streak (0x833F23..0x833F5F).
- The streaks are placed **once per process, twice**, not per land: the `LH3DAtmos` init `fn_00835AD0` (from
  `start_system` 0x6433C2 with flags = −(Weather != 0), [0xC381FC] = 1 by default, read from the "Weather" key at
  0x823A0C; also `DoCitadelMultiplayer` 0x5555A8 with flags −1) calls `fn_00833DA0` at 0x835DA8 (the constructor)
  and again at 0x835DC4: 2 × 128 × 7 = 1792 CRT draws at start-up, after the snow's (below). Land load does not reach
  it.
- `LH3DAtmos::Render3D` 0x836250: for each land block, two grid samples (the centre of the block and the
  centre + 40) and the largest of their rain and snow bytes; if it exceeds 5, it queues an object in the Z-sorter with the three
  low bytes of its user data = `(x/80, z/80, value × 88 / 100)` (`fn_008341B0`). When flushing the Z-sorter
  (`fn_0082F280`) the callback `fn_00833F80` unpacks them and calls `fn_00834370(x, z, 0, 128, alpha)` only when that
  alpha byte is above 5 (`cmp esi, 5; jle` 0x833FD3): a block of 6 % (alpha 5) is queued but shows no streaks. That is why there
  is a single tile per block, centred on its origin + 80.
- The tile alpha in `Render3D` is 88 × max / 100 (imul 0x51EB851F, 0x836431..0x83645D); fn_008341B0 clamps it to 0xFF
  (0x8341B8) and drops only a negative one (0x8341CC).
- `fn_00834370`: nothing beyond 400 m from the camera; between 100 and 400 the alpha and the number of streaks are multiplied by
  `1 − (d − 100) / 300`. The bottom alpha is that, and the top one `alpha / ((2d/400 + 1) × 5)`, so the streak
  fades towards the cloud. Each streak is also attenuated by its phase: below 0.05, `phase × 20`; above 0.95,
  `(1 − phase) × 20`. The material is `AtmosMaterial` in mode 6 (alpha) with Z test and no Z write.
- **The splashes** (the tile's callback `sub_833F80`, 0x833FF3..0x83410D, after the streaks). Only when the tile's
  queued strength byte (the 88 % value before the distance fade) is above 0x2C (44). The colour is
  `(clamp(strength × 255 / 88, 0, 255) << 24) + (g_darkness_to_color[200] & 0xFFFFFF)`, the 1/88 done by `imul
  0x2E8BA2E9; sar 4`, truncated, so this frame's light table at 200. It makes [0xEB996C] drops: the `RainSplash`
  detail key, 0, 0, 3, 5, 8, 8, 8 by level, written by `fn_008237B0` 0x823991 and `fn_00823AD0` 0x823C10. For each
  drop, with the CRT `Random(a, b)` 0x81D180 (`a + rand() × scale × (b − a)`):
  - z = `Random(0, 160)` + centre z − 80, then x = `Random(0, 160)` + centre x − 80: the first number is the z;
  - y = `LH3DIsland::GetAltitude` 0x803090 at the MapCoords (ftol(m × 65536 × 0.1), the same as `GetHeightAt`) + 1;
  - `g_water_drop_cb(point, Random(0, 0.3) + 0.5, colour)`. The callback is `water_drop_cb` 0x54EEA0, set by
    `GGame::InitOneTimeOnly` 0x54EFC3, and it is never null in play. It puts a ring in the first free slot, with no
    water test: growth the given size, age 0, angle 0, the width and depth scales 1, rate 1, cell 0x30, the given
    colour. With the 1024 slots full it makes no ring, but the three numbers are drawn all the same.
  The rings fall on the land as on the sea. The callback runs for every tile queued in `Render3D`, also one past the
  400 m of the streaks, whose `fn_00834370` draws nothing. The drawn snow (below) is ported as the `snowfallSystem`.

- The lightning's light flash of the registered storms (the 0x24-byte object at GWeather +0x70, fn_00837110.. fn_008372D0) and what the camera sees of it (LH3DAtmos::Update3D 0x8357A0 -> [0xFA2768]). See [Storm and tornado](../magic/storm-and-tornado.md#the-flash-and-the-clouds-of-the-registered-storms).
- LH3DAtmos::Update3D 0x8357D8..0x835903: the storm nearest the camera in x, z (descriptor position, squared distance, the first of equals; only those not marked for deletion), "inside" when its d^2 < ((inner + outer) / 2)^2 of the descriptor's radii — a flag that stays set when a later, nearer storm is outside (bl at 0x83585C is never cleared) — and then [0xFA2768] = inside ? ftol(clamp(nearest flash f1, 0, 1) x 255) : 0
- 0xC38E14: the scroll speed factor (1 at start)

#### The snow on objects

The snow lying under an object is drawn over it by its primitives' snow draw (vt +0x124 / +0x128 of the 3D object,
from the whole draw), read in the frozen v1.42 exe:
- `LH3DPrimitive::DrawSnow` 0x851870 (STATIC, ANIMATED and COMPLEX objects, and the base class) reads the texture at,
  from the vertex's own position (x, y, z) and normal (nx, ny, nz) as the primitive holds them (0x8524D9..0x85255B):
  a = (y × 0.3 + z) + 20, b = z × 0.1 + x, u = b − (whole(b) + 2.5), v = a − (whole(a) + 2.5), each whole part
  `ftol`, towards zero; the texture's u = (u − whole(u)) × 0.2 + ny, its v = (v − whole(v)) × 0.2 + nx.
- `LH3DPrimitive::DrawSnowWithVerticalTranslation` 0x859D90 (MORPHABLE objects and the CITADEL heart, through
  fn_0080AF80) does the same with the vertex moved by the melting (`g_vertical_translate_vertices`) and the normal
  turned by the object's matrix (+0x14) after `NormaliseMatrixOnly` (0x859DC3..0x859DFA, 0x859F45..0x85A004).
- openblack: `SnowUv` (`assets/shaders/snow.sh`), fed by `vs_object` the mesh's position and normal, and for a
  morphing object the position raised by the melting in the model's units and the normal turned by the instance's
  axes made unit length; its twin `snow_cover::ObjectUv` / `MorphingObjectNormal` (test
  `SnowCover.AnObjectReadsTheSnowTextureByItsOwnPositionAndNormal`).

#### The drawn snow

256 flakes of 0x34 bytes at [0xEDC354], only when the "Weather" setting is on (`fn_00835AD0`,
`test [flags], 2` at 0x835D27).

- Placement: `fn_00834890` (0x834890..0x8348BA), per flake `fn_00834700` (`Random(−80, 80)·0.5` 0x834710,
  `Random(0, [0xC38E10] = 160)` 0x834727, `Random(−80, 80)·0.5` 0x83473A, then `fn_008345F0` with 10 draws
  0x834611..0x8346D0: fall `(0,1) + 7`, sway speed `(0,10) − 5`, heading `(0,2π)`, sway `(0,π)`, spin `(0,2π)` ×3, spin
  speed `(0,π/2)` ×3), then y = `Random(−20, 160)` into +4 (0x8348AA): **14 CRT draws per flake, 3584 per call**.
- The init calls it **twice** in a row (0x835D51 as the constructor, then 0x835D6D on the same object, [0xEDC354] set
  at 0x835D67): **7168 CRT draws at start-up**, before the rain's 1792. Land load does not reach it.
- Per frame (`Update3D` 0x835AA0..0x835AB9): only if a snow tile was drawn in the previous frame ([0xEDC2FC], set
  at 0x836627), the step `fn_008348C0` clears the flag and moves the flakes: a flake at y ≤ −20 is placed again by
  `fn_00834700` (13 draws), and `fn_00834770` adds `Random(0, π/4)` to the heading each time the sway passes 2π
  (0x8347B9). With no snowing storm there are no per-frame draws.
- Drawing (`Render3D` 0x8365CD..0x83665C): for a block whose largest snow byte (offset 2) is > 5, `fn_00834900`
  (prepare the flakes) once per frame, then `fn_00834290` queues the tile with alpha snow·256/100 and the Z-sorter
  callback `fn_00834120` (0x834354). `fn_00834120` is the snow tile's **draw callback, not a scatter**: it unpacks
  [0xEE9D30] (x/80, z/80, amount), calls `fn_00834BF0` when the amount is > 5, and makes no draws.
- raffclar's tree (`SnowfallSystem.cpp`, `3D/Snowfall.cpp`): the same 13 placement ranges plus `(−20, 160)` in
  `Scatter` (14 per flake; the "13 × 256 = 3328" count is short by one per flake), and the step gated like
  [0xEDC2FC]; but it scatters **once**, at Locator emplace, where the original does it twice.

#### Order in the turn

In the steps of `GGame::ProcessTurn` 0x54E5C0 ([The engine loop: turns and frames](../engine/loop.md) §2): `ProcessTurnStart` from
`Magic/MagicLoop.cpp` (`LH3DAtmos::UpdateGame`, 0x54E5D7); `ProcessWeatherThings` (0x54E6CB) and `ProcessClimate`
(`GClimate::ProcessAll`, 0x54E6DA, with the bookmarks and highlights in between) from `Game::GameLogicLoop`; and
`UpdateFrame` every frame from `magic::Update` (the rain streaks). `OnLoadMap` leaves everything empty; the rain's
streaks and the snow's flakes are kept from the start-up (`StartAtmos`), with no CRT draw, as in the original.
Right after the atmosphere's start, `start_system` draws the CRT stream once more: `GatheringText::SetupGameFonts`
0x6433C7 → `FontCache::Init` 0x830750 shuffles the glyph cache's 256-byte hash permutation with 255 `rand`
(0x83076C). see
[Text: the original's fonts and the hand message](../interface/screen-and-readouts.md#text-the-originals-fonts-and-the-hand-message) and
[Random numbers](../engine/random.md#random-numbers).

### Weather objects

- fn_00774400 (CHANGE_WEATHER_PROPERTIES): temperature (whole degrees), rain, snow and overcast x 100, fall speed
- fn_00774460 (CHANGE_TIME_FADE_PROPERTIES): lifeTime, fadeInTime
- fn_00774500 (CHANGE_CLOUD_PROPERTIES): blackness, number of clouds (int), elevation. The native
  (`GScript::SetCloudProperties` 0x6FA600) pops the height, the blackness, the number and the thing in that order,
  logs "Jonty-Weather properties on non weather object" for anything else (0x6FA66B) and cuts the number with `__ftol`
  (0x6FA695); `WeatherThing::SetClouds` keeps the blackness at +0x4C, the number at +0x48 and the height at +0x50 and
  calls `UpdateStats` (0x774515), which hands the descriptor on to the storm.
- fn_00774520 (CHANGE_LIGHTNING_PROPERTIES): sheet and fork ranges

### Atmosphere grid details

- SnowCover fn_0086C7A0(seconds): ported as `snow_cover::Melt` / `SnowSystem::Melt`, from the weather's turn (`ECS/Weather/Atmos.cpp`); test `SnowCover.MeltsABandAtATime`. The lying snow whitens the objects standing in it, and the land through its blocks' prints step: the blocks the melt lists (fn_0086C5D0) are painted again from the weather's turn, and each block's texture and tile bake the snow over the noise image (fn_0086CBA0, [terrain.md](terrain.md#the-block-texture-queue-in-openblack)) (progress/weather/snow.md); still missing there: the rows that wait for saved games.

### Storm details

- A handle: the original keeps the GWeather pointer and asks fn_0083F8D0 whether it is still in the list
- +0x68 metres per second towards the target (0 in the ctor fn_0083F4E0)
- +0x70 the lightning's light flash (0x24 bytes, ctor fn_00837110): started by the fork (0.5) and sheet (1.0) lightning of GWeather::Update (fn_00837290 at 0x83FBF0 / 0x83FC5D), aged each turn (fn_008372D0), its light worked out each frame (fn_00837200, ECS/Weather/LightningFlash.h)
- fn_0083F8D0: the storm while it is in the list and not marked for deletion, else nullptr
- Vt 0x10 (the deleting destructor; fn_0083F630 unlinks it): gone at once. GClimate::ToBeDeleted 0x7713E0 does this with its storms.
- The engine's callbacks for the storms' lightning (GWeather::Update): the fork one creates a PSys strike (0xEEA384 = 0x68E8F0, PSysGlobal::InitializeOneTimeOnly), the sheet one plays the thunder (0xEEA388 = 0x429CE0, GAudio); both get the storm, the point (storm x, z, land height + elevation) and the outer radius. Unset = nothing; openblack sets both, the fork one to the strike queue (`src/Locator.cpp`, `particles::lightning_strike::Queue`) and the sheet one to the thunder when the audio starts (`src/Audio/Game/AudioSystem.cpp`); the light flash fn_00837290 is Storm::flash (LightningFlash.h).
- fn_008372D0(+0x70, seconds) (0x83FC7F): the flash object's own step
- Vt 0x10, the deleting destructor (fn_0083F630 unlinks it)

### Lightning flash

- fn_00837110's bitmap (0xED92F0, 64 x 64, built once for all flashes, refcount 0xEDD3A8): at d = |(x, y)|, x, y in -32..31, ftol((32 - d) x 9) capped at 255 inside d < 32 (0x8CF134, 0x8FFEC8), 0 outside; the three channels equal
- 0x8372D7: `dec eax; jne` — only an active (== 1) flash is tested; test ah, 0x41 -> off when age > 0.8

### The calendar

- fn_00557960: the month 1..12 (the first whose cumulative day count is above the day of the year; 12 past the last)
- fn_005579C0: the day of the month from 0 (the day of the year minus the days of the months before)
- fn_00557810 with SetStartDate(1998, 5, 5) and SetStartTime(18, 5, 30): the months before May (120 days, from the cumulative table), the day of the month (5, added whole), the hours, minutes and seconds; in turns

### Climate details

- fn_00773D30(rainMin, rainMax): the desire starts at rainMin and the raining days at int(rainMin x 100); rainMax is not read
- fn_00774A90 does nothing: no wind until the script sets it
- [0x900AE8] (0x772F6A)

### Storm clouds

- The storms that went take their puffs with them (fn_0083F630 deletes the mists)
- A new target every 400 frames (0x190), reached in steps of 0.0025 of the way (0x9A3B18), counted in frames, as the original.

## openblack

Status: **partial**. Progress rows: [climates](../project/progress/weather/climates.md) (97%),
[rain](../project/progress/weather/rain.md) (99%), [storms](../project/progress/weather/storms.md) (96%),
[wind](../project/progress/weather/wind.md) (97%), [scripted weather](../project/progress/weather/scripted_weather.md)
(91%), [snow](../project/progress/weather/snow.md) (26%).

In `src/ECS/Weather/`:
- `Weather.h` / `WeatherQueries.cpp`: the queries (the small header that the rest of the game includes);
- `WeatherInfo.h`: the 8-byte structure and its byte arithmetic;
- `Atmos.{h,cpp}`: the grid of 128 × 128 cells of 40 m;
- `Storms.{h,cpp}`: the registered volumes; `StormClouds.cpp` and `LightningFlash.cpp`: their clouds and flashes;
- `Climate.{h,cpp}`: the climates, `ComputeWeather`, `ProcessAll` and the natural storms;
- `Calendar.{h,cpp}`: the date part of the game info (day of the year, month, season);
- `WeatherThing.{h,cpp}`: the script weather objects;
- `WeatherState.h`: what the weather keeps between turns (owned by the `WeatherSystem`); `WeatherLand.h`: the land
  queries the weather asks (the height, and the storm placement's no-land test);
- `WeatherLoop.{h,cpp}`: the calls from the turn; `WeatherDebugHooks.cpp`: the test hooks;
- outside it, the drawn rain: `3D/Rain.{h,cpp}` (the pure streak and tile rules), the `rainSystem`
  (`Systems/Implementations/RainSystem.{h,cpp}`) and `Graphics/RendererRain.cpp`;
- the scripts: `Magic/Script/MapScriptWeather.cpp` (map commands) and `Magic/Script/CHLWeather.cpp` (script natives).

Where it is in the code, by file:
- `WeatherThing.h`: the script objects' setters, one per CHL command: `SetWeatherProperties` (temperature in whole
  degrees, rain, snow and overcast, fall speed), `SetTimeFadeProperties` (life time and fade-in time),
  `SetCloudProperties` (blackness, number of clouds, elevation; test
  `WeatherTest.AScriptChangesAWeatherThingsCloudsAndTheirStorm`), `SetLightningProperties` (the sheet and fork ranges)
  and `SetTarget` (the storm's target and speed).
- `Storms.h`: `StormId`, a handle to a storm, checked against the list as the original checks its pointer;
  `LightningCallback`, the engine's callbacks for the fork lightning (a particle strike) and the sheet lightning (the
  thunder), given the storm, the point and the outer radius; unset, they do nothing.
- `LightningFlash.h`: `flash::AtCamera` (`AtCamera`), the flash level at the camera: the nearest live storm in x and z,
  "inside" when within the mean of its inner and outer radii (a flag a later, nearer storm outside does not clear),
  then the flash scaled to 0..255, or 0 outside.
- `Calendar.{h,cpp}`: `GetMonth` (the month 1..12, 12 past the last), `GetDayOfMonth` (from 0) and `StartTurns` (the
  start date, 5 May 1998 at 18:05:30, in turns).
- `Climate.{h,cpp}`: `SetRain` (the four rain values; id 0 is the world's climate, made if missing), `SetWind` (wind x,
  z and angle), `CurrentSeason` (the season the climates use, changed on the first day of a new season) and
  `InitFromInfo` (a climate from its info: the rain desire starts at the season's minimum and the raining days at the
  minimum x 100, and the temperature and its target at the hour's and month's share of the range; no wind until the
  script sets it; tested by `WeatherTest.AClimateStartsWithTheSeasonsLeastRain`).
- `Climate.h`, reached through `Systems/Implementations/WeatherSystem.{h,cpp}`: `SetClimateSystemEnabled` (pauses the
  temperature, rain and wind updates) and `SetStormCreationEnabled` (pauses the climates' storm creation).
- `StormClouds.{h,cpp}`: `PuffColour` (a puff's colour: the land light's base colour darkened by up to half with the
  blackness, alpha the fade x 0.75 x the base alpha) and `StormShadowImage` (`sstorm.raw`, 40 x 40 grey, the storms'
  shadow on the land).

The weather state lives in components or Locator services; the byte arithmetic and the formulas are pure functions
tested with fakes. Porting decisions taken from raffclar's tree (weather, snow cover, snowfall):
[Port decisions: engine and world](../project/port-decisions-engine-world.md#weather).

#### CHL weather objects

- CHL `CREATE` of a weather thing makes its storm at the point's map position on the land there, the script's height
  dropped, as the original's create (its altitude 0); before, the script's y was added to the land. Test
  `WeatherTest.AScriptCreatesAWeatherThingOnTheLandUnderItsPoint`.
- A weather thing whose storm is over is deleted unless a script holds it, as the original
  (`weather_thing::ProcessWeatherThings`, asking `ecs::script_held::IsInScript`; the Weather window lists the things);
  while its storm lasts it stands at the storm's x and z cut to a map position, y 0, and the storm takes whether a
  script controls the thing (`Storm::controlledByScript`, read by nothing, as in the original). Test
  `WeatherTest.AWeatherThingEndsWithItsStormUnlessAScriptHoldsIt`.
- SET_HEADING_AND_SPEED gives it its movement (`magic::script::SetWeatherHeadingAndSpeed` to
  `weather_thing::SetMovement`, the wind bytes clamp(v × 8, −127, 127)); test
  `WeatherTest.AScriptSendsAWeatherThingOffAtAHeadingAndSpeed`. The script's changes of its weather, lightning and
  times are pinned by `WeatherTest.AScriptChangesAWeatherThingsWeatherLightningAndTimes`.
- The wind natives ([script commands](../scripts/script-commands.md), Weather natives): IS_WIND_MAGIC_AT_POS asks
  `weather::storms::IsWindMagicAt` (a storm with no clouds whose outer radius reaches the point, by the 2D table
  distance; the popped radius is not read, only its type taken as a float, as the original's `GScript::IsWindMagicAtPos`
  0x70C8D0; test `WeatherTest.WindMagicIsACloudlessStormOverThePoint`); SET_ and GET_PLAYER_WIND_RESISTANCE keep and
  read the player's value (`ecs::wind_resistance`, a `NoWindResistance` component on the player), and a thing that
  player's hand throws has its drag cleared, as `Object::ThrowObjectFromHand` 0x638700 (test
  `WeatherTest.AScriptSetsAndReadsAPlayersResistanceToWind`). Their bindings carry what the functions pop and push
  (test `ChlApiStack.WindFunctionsPopAndPushAsTheOriginal`).

#### The drawn rain

- In openblack the streaks are one-pixel lines with the `WorldTextured` program (`atmos.raw` + `atmosa.raw`) in the `MainBlended` pass. The streaks are placed as the original's: twice, 2 × 896 CRT draws, once at start-up (`weather::StartAtmos` from `InitializeEngine`, right after the snow's scatter and before any other CRT draw), and never on land load; the fall's height and speed carry over from one land to the next, as the original's globals do.

- A block shows its streaks only when its strength (88 % of its most rain) is above 5, as the callback tests it
  (`rain::ShowsStreaks`; test `Rain.StreaksShowByTheStrengthNotTheMostRain`).
- The Weather setting off: the original's start-up (`fn_00835AD0` with flags 0, `test [flags], 4` at 0x835D7A)
  leaves the streaks' object null ([0xEDC358] = 0 at 0x835DCB), and every frame then skips it: `Update3D` steps the
  streaks only while it is not null (0x835A7E..0x835A8A), and `Render3D` queues no block's rain (0x836427..0x83642F),
  so no tile is drawn and no drop splashes; the fall's height and speed are still followed (`Update3D` before
  0x835A7E). openblack: `weather::StartAtmos` calls `RainSystemInterface::Disable`, after which `RainSystem::Update`
  only follows the fall and `RainSystem::TakeTiles` takes nothing (tests `AtmosStart.NothingWithTheWeatherSettingOff`,
  `AtmosStart.TheRainIsThereWithTheWeatherSettingOn`); the Weather window says "No rain" then.
- The streaks step on in the next update when a block's rain was queued in the frame (`rain::Queued`: its most rain
  over 5), as the original's 0xEDC300: `Render3D` clears it (0x8362A4) and sets it at the first queued block
  (0x836422), before any distance or strength test, so a block 400 to 560 m away, which shows no streaks, counts too
  (`RainSystem::TakeTiles`; test `Rain.AQueuedBlockCountsWhateverItShows`).
- The splashes: `RainSystemInterface::Splash`, from `Renderer::DrawRainTile` as the tile drains from the frame's
  transparency queue, as the original's callback, with the detail level's `rainSplash` drops and the CRT random
  numbers (`rain::Splash`, `rain::SplashAlpha`; tests `Rain.OnlyStrongRainSplashes`,
  `Rain.SplashFallsOnItsBlockInTheGamesOrder`). A block past the streaks' 400 m, or whose streaks thin to none, is
  still taken as a tile with no streaks when its strength splashes. The rings go into the water ring system and are
  drawn from the next frame on, as the rings are drawn before the transparency queue. The debug switch is the
  Weather window's "Rain splashes" box. Testbed scenes `light.rain_splashes` and `light.rain_splashes_night`.

#### The drawn snow

- openblack: the `snowfallSystem`. Each land block near the camera is read as the original's four 80 x 80 tiles, along x then z, each at its four corners (40 either side of its centre), and a tile whose most snow is above 5 is drawn about its smallest corner (`snowfall::BlockTiles`, `TileOf`); until 2026-10-22 only one tile a block was drawn, so the snow covered a quarter of the land (#193).
- openblack: the original's start-up, `weather::StartAtmos` (from `InitializeEngine`, once a process, before any land): `SnowfallSystemInterface::Scatter` twice, then `RainSystemInterface::Reset` twice, 7168 + 1792 CRT draws, the first of the process. Drawing a snow tile scatters nothing; the step is gated by the frame before as the original's. The gate is the "Weather" column of the detail level (`detail_level::Weather`, 0001111, the original's [0xC381FC] from `fn_00823AD0` 0x823C49; level 4 by default, so on), read once at start-up.
- openblack: with the Weather setting off the original's snow object [0xEDC354] stays null (0x835D74), `Update3D` steps
  the snow only when it is there (0x835AA0) and `Render3D` queues no snow tile (0x83662C); `weather::StartAtmos` calls
  `SnowfallSystemInterface::Disable`, after which `SnowfallSystem::Update` moves no flake and `TakeTiles` takes nothing
  (test `AtmosStart.NothingWithTheWeatherSettingOff`); the Weather window says "No falling snow" then.

#### Order in the turn

- openblack makes the same shuffle at the same point (`graphics::GameFont::MakeHashPermutation`, right after `weather::StartAtmos` in `InitializeEngine`)

#### Storm clouds

- openblack counts the puffs' 400-frame wander in frames, as the original (`storm_clouds::DrawFrame`); its frame
  rate is not the original's, so the wander takes a different time in seconds.

### Test hooks

`OPENBLACK_TEST_WEATHER="x,z,radius[,rain[,fade[,temperature]]]"`, `OPENBLACK_TEST_WEATHER_AT="x,z[;x,z...]"` and
`OPENBLACK_WEATHER_TRACE=1`; see
[Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending


- The hand light's strength (integer mean): whether any run of the verification set has a base
  mean from 105 to 119 while the hand is shown is not traced (Land 1 is light up to about turn 9 and then has a mean
  of about 81; vis night has 68), so the fidelity, vis and hand-demo runs are expected not to move, but that is not
  measured. Whether `LandLightTable::GetLandColour` reproduces `fn_00869850`'s integer lerp and its
  255 − [0xFA2754]·96 cap bit for bit is still not checked (see raffclar's tree above).
- The hand light's colour overload (0..1 fractions) does not clamp a channel to 0..1 before it rounds it to its byte,
  so a channel above 1 would add more than 255 to the sum. No caller passes one today; the original reads bytes.
- The snow, still to read in the original (openblack has taken raffclar's version or a stand-in until then):
  - the drawn snow tile's alpha and fade (raffclar's formula, unverified);
  - the snow cap of a field's crops (openblack uses the rate and a cap of 255, with no per-instance cap);
  - snow on the hand and the particle atoms (openblack draws it only on land-lit mode-1 instances in the main view).
- The virtual influence of the weather, and the ambient weather of the saved game (saved games: the fork's issues
  #186 and #187). `LHInetWeather`, the real weather from the gone service, is not in the original as it can be played:
  [not-in-the-original.md](../project/not-in-the-original.md#pc_integrationreal_weather).
- `GClimate+0x84` ("lightning even if it is below 30 degrees", a byte read by `CreateStorm` at 0x77308C / 0x77309C):
  the only store to it found is the constructor's 0 (0x7712E0, `GClimate::GClimate(int)`); none in the rest of the
  climate code (0x770000..0x775000), the map script's commands (0x7165CA..0x717EBA), the challenge script's climate
  calls (sub_6DEA00), `Save` / `Load` (which neither write nor read it) or the climate natives (0x6FE000..0x701000).
  So it is 0 in every game, as openblack takes it (read 2026-10-25; a store through a pointer of another kind is not
  excluded by this scan).
- The per-block distance field (+0x9BC) that `Render3D` uses to discard blocks: openblack's `RainSystem::TakeTiles`
  uses the 2D distance from the block's centre to the camera with the same threshold (400 + 160), and the real cut-off
  is still the 400 m one of `fn_00834370`. The field itself is now read: the visible-block pass writes it, the 3D
  distance from the block's centre to the camera, and leaves a culled block's old value
  ([visible blocks](../rendering/visible-blocks-and-lod.md#stage-a-the-visible-blocks)); whether `Render3D` walks every
  block or only the visible ones, and so whether a stale value can let a block's rain through, is not yet read.
- The sounds of the houses chosen by the sky type; the
  no-blend dome mode, the 565 path and the light table after `Jump`, not yet read.
- The dome's draw: the screen effect `LH3DAtmos::Render2D` 0x8366A0 → 0x836990 and
  `fn_00836200` (→ `fn_00836E20`) that run while the darkness [0xEDC374] > 0, with the "Weather" key: what they draw is not
  yet read.
- The dome's alignment X: the original's is [0xBF3378] (0 good, 1 neutral, 2 evil, moved 0.001 per ms by `DrawSky`);
  openblack's is 1 − its `SkyAlignment`, which moves in openblack's units (−1 evil to 1 good), so X is not bit for
  bit the original's once the alignment has moved (queued fix).

