# The sky dome

The sky is a dome of pictures: for each alignment (good, neutral, evil) one by day, one at dusk and one at night,
blended for the time of day and mixed by the alignment the sky shows, then tinted by the weather.

**Progress: 11/16 done, 1 partial — 72%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Time of day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky type runs from 0 at night through 1 at dusk to 2 by day | done | `sky_type::At` and `sky_type::Frame` (`src/3D/SkyType.cpp`); test `test/test_sky_type.cpp`. Our wiki differs: the original's sky type is 2 at night, 1 at dusk and 0 by day ([day-night-weather.md](../../bw1-notes/day-night-weather.md#sky-type-src3dskytype)) |
| The dome blends day into dusk, or dusk into night, by the sky type | done | `Sky::BlendDome` (`src/3D/Implementations/Sky.cpp`), `sky_type::DomeWeightOf`; test `test/test_sky_type.cpp` |
| The blend is rebuilt only once the sky has moved on far enough (a hysteresis of 0.03) | done | `sky_type::Dome().Advance` (the 0.03 hysteresis) |
| It is then rebuilt a band of 32 rows a frame, top down | done | `Sky::UpdateDome`; the GPU gets the dome once the last row is done |
| After the clock jumps the whole dome is rebuilt at once | done | `sky_type::Jump` from `DayNightClock::SetScriptTime` |
| Texels blend in whole steps of the pictures' 5-bit channels | done | `sky_type::BlendRows555`; test `test/test_sky_type.cpp` |
| The blend is done on the GPU rather than on the CPU as the game does | done | Our tree blends the times on the CPU, as the game does (`Sky::BlendDome`); `assets/shaders/fs_sky.sc` only mixes the alignments |

## Alignment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each alignment has its own dome; the sky mixes the two nearest by the alignment it shows | partial | One layer per alignment, mixed linearly in `assets/shaders/fs_sky.sc`; how the original mixes them is not read yet |
| The alignment's opacity is cut to 255ths | todo | `assets/shaders/fs_sky.sc` mixes with the unquantised alignment |
| The sky turns towards the alignment at the camera over time, none while paused | done | `SkyAlignment::Update` from `AlignmentSystem.cpp` (0.001 a ms, nothing while paused) |
| An evil sky is darkened, up to 90 of 255, with the weather detail option on | todo | The Weather detail option is not read (`detail_level::Level::weather`), and the dome is not darkened |
| Inside the temple the sky dome is still drawn (seen through its openings), without the sun | done | `src/Graphics/RendererTemple.cpp` draws the dome and no sun; see sun_and_moon.md and ../temple/ |

## Weather on the sky

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An overcast turns the dome towards the haze's colour, with the fog detail option on | todo | The dome's draw (`assets/shaders/fs_sky.sc`) takes no weather |
| A flash of lightning turns the dome towards white | todo | The dome's draw takes no flash |
| The sky is drawn first, before the scene, and in the sea's reflection | done | The sky stage of the main and reflection passes (`src/Graphics/Renderer.cpp`) |
| Stars are part of the night pictures, with no separate star field | done | The night domes carry them |
