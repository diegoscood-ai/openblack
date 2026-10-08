# The moon

The moon is a small half sphere that keeps its place beside the camera, 4000 units along plus x (east). It swings up
and down on an ellipse with the script hour and is turned to face the camera. Its face is lit by the real moon's phase,
taken from the computer's date, and it is tinted the palette's moon colour for the alignment. A soft glow from the
atmosphere texture is added round it, and a mirrored copy is drawn for the sea.

**Progress: 34/37 done, 1 partial — 93%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is placed relative to the camera, not the world, so it never gets nearer as the camera moves | done | `Renderer::DrawMoon` (`src/Graphics/Renderer.cpp`: the camera's origin plus the offset) |
| The offset is (4000, 1100 × cos(a) − 150, 800 × sin(a)), with a = script hour × π/12 | done | `moon::Place` (`src/Graphics/Moon.cpp`); test `Moon.ShowsAroundMidnight` (`test/graphics/test_moon.cpp`) |
| So it is highest (950 up) at midnight and lowest (1250 down) at noon, swinging north and south by 800 as it goes | done | `moon::Place` |
| It follows the script hour, not the visual hour | done | `Renderer::DrawMoon` takes the sky's time, the script time |
| The offset is worked out when the weather is updated each frame | done | Worked out every frame in `Renderer::DrawMoon` |

## Showing and fading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its strength is floor(0.5 × height − 110), capped at 200 | done | `moon::Place`; test `Moon.ShowsAroundMidnight` |
| It is drawn only while that is above 0: height over 220, about 19:20 to 04:40 script time | done | `moon::Place` returns nothing below |
| It is at its full 200 from height 620 up, about 21:00 to 03:00 | done | `moon::Place` |
| With the fog option on and an overcast, its strength is divided by (8 × overcast + 1), truncated | todo | `Renderer::DrawMoon` does not dim the moon by the overcast |
| When the moon is down, neither it nor its glow is drawn | done | `Renderer::DrawMoon` returns early |

## Its shape and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's moon model, a half sphere about 100 units in radius | done | The sky's moon mesh (`SkyInterface`, `src/3D/Implementations/Sky.cpp`) |
| Its axes are built square to the line from the camera and kept upright, then scaled by 4 | done | `moon::Basis`; tests `Moon.FacesTheCamera`, `Moon.FacesTheEyeInTheView` |
| The mesh leans back by 7.5 degrees (0.1309 radians) | done | `moon::Model` (`k_Tilt`) |
| It is turned about its upright axis by the phase plus half a turn | done | `moon::Model` |
| It is drawn at 0.65 of the basis, so about 270 units in radius at about 4100 units away | done | `moon::Model` (`k_MeshScale`) |

## Its face and phase

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The phase comes from the computer's clock, in whole days since 1 January 1970 | done | `moon::Phase`; test `Moon.PhaseFollowsTheRealMoon` |
| Days are counted from 10962 days after 1970 (6 January 2000, a new moon) | done | `moon::Phase` |
| Moon months are days × 0.0338631801 (one every 29.53 days); the phase is (1 − the fraction of a month) × 2π | done | `moon::Phase` |
| The game reads the clock at most once every 2 seconds and keeps the phase between reads | done | Our tree reads it every frame, which gives the same phase |
| The face's texture coordinates are projected from the mesh's points turned by the phase: u = (x·cos p − z·sin p) × 0.0025 + 0.25, v = y × 0.0025 + 0.25 | done | `assets/shaders/vs_celestial.sc` (the phase's cosine and sine) |
| So the face shows the top-left quarter of weather.raw, and the lit part swings across it with the phase | done | `raw/weather` in `Renderer::DrawMoon` |
| The disc's outline comes from weathera.raw, the alpha texture | done | `raw/weathera` and `assets/shaders/fs_celestial.sc` |
| There is no separate earthshine: the dark side is whatever the texture shows | done | Nothing else is drawn for it |
| Scripts ask how full the moon is: 1 − phase / π below π, (phase − π) / π from π, so 0 halfway through the moon month and 1 at either end | todo | GET_MOON_PERCENTAGE is a stub that returns 0 in `src/CHLApi.cpp` |

## Colour

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is tinted the light palette's moon colour for the alignment the sky shows | done | `LandLightTable::GetMoonColour`; see lighting.md |
| The tint's alpha is the moon's strength | done | `Renderer::DrawMoon` |
| The moon's colour doesn't change through the night, only its strength | done | `Renderer::DrawMoon` |

## The glow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A square glow is drawn first, centred on the moon, 500 units each way along the moon's axes (so 4000 units across once scaled by 4) | done | `moon::MakeGlow`; test `Moon.GlowIsASquareAboutTheMoon` |
| It shows the part of the atmosphere texture from 0.25 to 0.49375 both ways, with its alpha texture | done | `moon::MakeGlow` (`raw/ATMOS`, `raw/ATMOSA`) |
| Its colour is the moon colour with red a sixth, green a fifth and blue a quarter, at the moon's strength | done | `moon::GlowColour` |
| It is added to the sky | done | `Renderer::DrawMoon` (additive) |

## Draw order, depth and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky after the dome and the sun, before the land and everything else | done | The sky stage of the main and reflection passes (`src/Graphics/Renderer.cpp`) |
| The land drawn later covers it | done | The moon writes no depth and the land is drawn after it |
| A second moon is drawn for the sea's sky: the same moon with its height below the camera flipped and its lean reversed, seen through the sea | done | `Renderer::DrawMoon` in the reflection pass: the glow from the mirrored view and the mesh mirrored, as our wiki describes ([rendering.md](../../bw1-notes/rendering.md#sky-sun-moon-and-clouds-original)) |
| Moonlight: at night the land and models take the palette's moon colour | done | `src/3D/LandLightTable.cpp`; see lighting.md |
| The creature can choose to look at the moon, only at night | partial | The plan action exists (`src/Creature/CreaturePlanActions.cpp`, LookAtMoon as a look-about); the night check is not confirmed; see ../creature/ |
| Near a full moon, at night, the game's guidance shows a help sprite about the moon now and then | done | `guidance::RemarkOnMoonPhase` (`src/Audio/Services/Guidance.cpp`: at visual night near the full moon, then a long wait) |
