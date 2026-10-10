# The sun

The sun is a single textured square about 9900 units across, placed 30000 units out along both minus x and minus z
(the north west) and turned to face the island. It rises and sets with the script hour and is drawn into the sky right
after the dome. At the end of the frame a larger orange glare is drawn over everything. The glare dims when land, the
sea's horizon or (at some detail levels) objects hide the sun.

**Progress: 33/43 done, 4 partial — 81%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sun's x and z never change: 30000 units out along minus x and minus z | done | `sun::Place` (`src/Graphics/Sun.cpp`) |
| It follows the script hour (the stretched hours scripts see), not the visual hour | done | `Renderer::DrawSun` takes the sky's time, the script time; test `Sun.RisesAndSetsWithTheScriptTime` (`test/graphics/test_sun.cpp`) |
| Its height is 7500 × (h − 6) / 6, where h is the hour held between 6 and 18 and mirrored about noon: 0 at 6 and 18, 7500 at noon | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| Before 6 and after 18 it stays at height 0, on the horizon, while it fades | done | `sun::Place` |
| It is turned three eighths of a turn about the vertical so it faces the island | done | `Renderer::DrawSun` (the turn's sense is inferred) |
| It is drawn at the mesh's own size (scale 1) | done | `Renderer::DrawSun` |
| It does not move round the sky: it rises and sets in the same place | done | `sun::Place` |

## Fading in and out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fully shown (255) from 6 to 18 | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| It fades in from 3 to 6 at 85 a script hour ((h − 3) × 85) | done | `sun::Place` |
| It fades out from 18 to 21 at 85 a script hour (255 − (h − 18) × 85) | done | `sun::Place` |
| Before 3 and after 21 it is not drawn at all, and nor is its glare | done | `sun::Place` returns nothing, so neither the sun nor its glare is drawn |
| With the fog option on and an overcast at the camera, its strength is divided by (8 × overcast + 1), overcast capped at 1, truncated to a whole number | todo | `Renderer::DrawSun` does not dim the sun by the overcast |
| Without the fog option an overcast does not dim it | done | Our tree never dims it by the overcast |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's sun model: one square about 9928 units across, facing along its own z | done | `sun.l3d` loaded by the sky (`src/3D/Implementations/Sky.cpp`) |
| Its texture is a 256 by 256 picture (sun.raw); the square shows the part from 0.5 to 0.75 across and from 0.25 down | done | `raw/sun` in `Renderer::DrawSun`; the texture coordinates come from the mesh |
| The texture is cut to 5 bits a channel as the game stores it | done | The raw texture load |
| It is tinted a warm 0x957C63, with the strength as its alpha | done | `Renderer::DrawSun` |
| It is added to the sky behind it, using its alpha | done | `Renderer::DrawSun` (additive, source alpha and one, as our wiki gives it: [rendering.md](../../bw1-notes/rendering.md#sky-sun-moon-and-clouds-original)) |
| Its colour does not change with the time of day, the alignment or the climate, only its strength | done | `Renderer::DrawSun` |
| There are no lens flares or rings: only the disc and its glare | done | Nothing else is drawn |

## Draw order and depth

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky, after the dome and before the moon, before the land, the sea and everything else | partial | The sky stage of the main pass (`src/Graphics/Renderer.cpp`) draws the dome, then the moon, then the sun |
| It leaves no depth, so the land drawn later covers it | done | `Renderer::DrawSun` (no depth write) |
| The game draws no sun in the sea; only a mirrored moon goes into the sea's sky | done | The reflection pass draws only the moon (`src/Graphics/Renderer.cpp`) |
| Scripts can stop it being drawn: SET_SUN_DRAW with true hides the sun and its glare until it is set false again or the game restarts its script | todo | SET_SUN_DRAW is a stub in `src/CHLApi.cpp` |

## The glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The glare is the sun again, 1.8 times as large, drawn in orange 0xA06A35 | done | `sun::k_GlareScale` in `Renderer::DrawSun` |
| It is drawn only in frames where the sun itself was drawn | done | `Renderer::DrawSun` (the same placement) |
| It is drawn last in the 3D frame, over everything, with no depth test (the game sets the depth test to always pass) | done | `Renderer::DrawSun` at the end of the blended pass, depth test always |
| Its alpha is the glare strength × the sun's strength / 255 | done | `Renderer::DrawSun` |
| With the fog option its alpha is also divided by (8 × overcast + 1) | todo | The glare is not dimmed by the overcast |
| It eases towards its target by 0.01 of the gap for every millisecond of game time, kept between 0 and 255 | done | `sun::EaseGlare`; test `Sun.GlareEasesTowardsWhatShows` |
| While the game is paused it holds where it is | done | `sun::EaseGlare` takes the frame's game time |
| The target is 255 × (1 − 0.2 × the number of hidden samples) | done | `sun::EaseGlare` |

## What hides the glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five points are tested: the sun's centre and four points 500 units off it along the world's x and y | partial | `sun::k_GlareSamples`: the four points lie along the sun's own right and the world's up |
| Each point is first pushed further along the line from the camera by the near distance | todo | Not in `Renderer::DrawSun` |
| No point is tested lower than height 10 | done | `sun::k_GlareLowestSample` |
| A point counts as hidden when the land lies between the camera and it, tested along the line by the land's cells | partial | `Renderer::DrawSun` marches the land height along a quarter of the line in 256 steps, not the game's land line test |
| A point also counts as hidden when the line from the camera dips below sea level within 7500 units of the camera, so the sea's horizon hides the setting sun | todo | No sea test in `Renderer::DrawSun` |
| At detail levels 3, 4 and 6, with the depth-reading option on, a point also counts as hidden when the depth buffer at its spot on the screen shows something nearer, so models hide it | todo | Only the land hides the glare in our tree |
| Looking straight at the sun with nothing in the way gives the full glare | done | `sun::EaseGlare` |

## The sun's light

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit from a fixed sun far to the north west and up | done | `model_light::k_DefaultSun` (`src/Graphics/ModelLight.h`) |
| Object shadows fall away from that fixed sun | done | `src/Graphics/ShadowMath.h` (the fixed sun point); the static shadows too, see ../terrain/landscape_rendering.md |
| The land's and models' colour through the day comes from the light palette, not from the sun's height | done | see lighting.md |
| The creature can choose to look at the sun, which it may do only when it is not night | partial | The plan action exists (`src/Creature/CreaturePlanActions.cpp`, LookAtSun as a look-about); the night check is not confirmed; see ../creature/ |
