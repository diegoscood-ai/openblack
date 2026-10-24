# The sun

The sun is a single textured square about 9900 units across, placed 30000 units out along both minus x and minus z
(the north west) and turned to face the island. It rises and sets with the script hour and is drawn into the sky right
after the dome. At the end of the frame a larger orange glare is drawn over everything. The glare dims when land, the
sea's horizon or (at some detail levels) objects hide the sun.

**Progress: 42/43 done, 1 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/sky.md).

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sun's x and z never change: 30000 units out along minus x and minus z | done | `sun::Place` (`src/Graphics/Sun.cpp`); test `Sun.RisesAndSetsWithTheScriptTime` |
| It follows the script hour (the stretched hours scripts see), not the visual hour | done | `Renderer::DrawSun` takes the sky's time, the script time; test `Sun.RisesAndSetsWithTheScriptTime` (`test/graphics/test_sun.cpp`) |
| Its height is 7500 × (h − 6) / 6, where h is the hour held between 6 and 18 and mirrored about noon: 0 at 6 and 18, 7500 at noon | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| Before 6 and after 18 it stays at height 0, on the horizon, while it fades | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| It is turned three eighths of a turn about the vertical so it faces the island | done | `Renderer::DrawSun`: the original places it with `SetPosition` (vt+0x20, 3π/4, scale 1, 0x86C178..0x86C187), T(p)·Ry(−a) in glm as [matrices](../../../engine/matrices-and-zoomer.md) gives it, so glm's −3π/4 as ours; the day land shot |
| It is drawn at the mesh's own size (scale 1) | done | `Renderer::DrawSun` (scale 1.0 at 0x86C178); the day land shot |
| It does not move round the sky: it rises and sets in the same place | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |

## Fading in and out

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fully shown (255) from 6 to 18 | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| It fades in from 3 to 6 at 85 a script hour ((h − 3) × 85) | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| It fades out from 18 to 21 at 85 a script hour (255 − (h − 18) × 85) | done | `sun::Place`; test `Sun.RisesAndSetsWithTheScriptTime` |
| Before 3 and after 21 it is not drawn at all, and nor is its glare | done | `sun::Place` returns nothing, so neither the sun nor its glare is drawn (the exe's A = 0 gates, 0x86C159, 0x86BB61); test `Sun.RisesAndSetsWithTheScriptTime` |
| With the fog option on and an overcast at the camera, its strength is divided by (8 × overcast + 1), overcast capped at 1, truncated to a whole number | done | `Renderer::DrawSun` weakens it through `sky_dome::ThroughOvercast` with the Fog key (`src/3D/SkyDome.cpp`), the overcast held to 1; test `SkyDome.SunAndMoonDimThroughAnOvercast` |
| Without the fog option an overcast does not dim it | done | `sky_dome::ThroughOvercast` with the Fog key off, as the exe's [0xC37204] test at 0x86C1C1; test `SkyDome.SunAndMoonDimThroughAnOvercast` |

## How it looks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's sun model: one square about 9928 units across, facing along its own z | done | `sun.l3d` loaded by the sky (`src/ECS/Systems/Implementations/SkySystem.cpp`); the day land shot |
| Its texture is a 256 by 256 picture (sun.raw); the square shows the part from 0.5 to 0.75 across and from 0.25 down | done | `raw/sun` in `Renderer::DrawSun`; the texture coordinates come from the mesh; the day land shot |
| The texture is cut to 5 bits a channel as the game stores it | done | The raw texture load: `sun` is in the 5-bit stems (`k_Rgb555Stems`, `src/Resources/Loaders.cpp`); the day land shot |
| It is tinted a warm 0x957C63, with the strength as its alpha | done | `Renderer::DrawSun`, as the exe at 0x86C1F6; the day land shot |
| It is added to the sky behind it, using its alpha | done | `Renderer::DrawSun` (additive, source alpha and one, as our wiki gives it: [rendering.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)); the day land shot |
| Its colour does not change with the time of day, the alignment or the climate, only its strength | done | `Renderer::DrawSun` (the exe's colour is the constant 0x957C63); the day land shot |
| There are no lens flares or rings: only the disc and its glare | done | Nothing else is drawn, as the original's sky stage (`fn_0086B010`) and its one glare call (`fn_0086BB60` from `FinishFrame`); the day land shot |

## Draw order and depth

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky, after the dome and before the moon, before the land, the sea and everything else | done | The sky stage of the main pass (`Renderer::DrawPass`, `src/Graphics/Renderer.cpp`) draws the dome, then the sun, then the moon, as the original's sky ([sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)); the Moon debug window says which shows; the day land shot |
| It leaves no depth, so the land drawn later covers it | done | `Renderer::DrawSun` (no depth write, mode 13); the day land shot |
| The game draws no sun in the sea; only a mirrored moon goes into the sea's sky | done | The reflection pass draws only the moon (`src/Graphics/Renderer.cpp`), as `fn_0086C140` is called once; the day land shot |
| Scripts can stop it being drawn: SET_SUN_DRAW with true hides the sun and its glare until it is set false again or the game restarts its script | done | SET_SUN_DRAW keeps its value on the sun's entity (`ecs::sun_draw`, `src/ECS/SunDraw.h`; `components::Sun::hiddenByScript`): any value but nought hides the sun and its glare (`Renderer::DrawSun`) until a script sets nought, a frame is drawn inside the temple or the scripts restart (a new game, a playground's start; a land change keeps it, `SkySystem::KeepForNextLand`), as the original's shared flag ([sky.md](../../../world/sky.md#the-sun-and-its-glare)); test `test_sun_draw`; the Moon debug window's switch |

## The glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The glare is the sun again, 1.8 times as large, drawn in orange 0xA06A35 | done | `sun::k_GlareScale` in `Renderer::DrawSun`; the day land shot |
| It is drawn only in frames where the sun itself was drawn | done | `Renderer::DrawSun` (the same placement and the same script switch, where the original keeps a "sun drawn" flag [0xFA275C]); the day land shot |
| It is drawn last in the 3D frame, over everything, with no depth test (the game sets the depth test to always pass) | done | `Renderer::DrawSun` at the end of the blended pass, depth test always; the day land shot |
| Its alpha is the glare strength × the sun's strength / 255 | done | `sun::GlareAlpha`; test `Sun.GlareAlphaIsAWholeNumberThroughTheOvercast` |
| With the fog option its alpha is also divided by (8 × overcast + 1) | done | `sun::GlareAlpha` (`src/Graphics/Sun.cpp`); test `Sun.GlareAlphaIsAWholeNumberThroughTheOvercast` |
| It eases towards its target by 0.01 of the gap for every millisecond of game time, kept between 0 and 255 | done | `sun::EaseGlare`; test `Sun.GlareEasesTowardsWhatShows` |
| While the game is paused it holds where it is | done | `sun::EaseGlare` takes the frame's game time; test `Sun.GlareEasesTowardsWhatShows` |
| The target is 255 × (1 − 0.2 × the number of hidden samples) | done | `sun::EaseGlare`, `sun::HiddenSamples`; tests `Sun.GlareEasesTowardsWhatShows`, `Sun.GlareCountsEveryHiddenPoint` |

## What hides the glare

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five points are tested: the sun's centre and four points 500 units off it along the world's x and y | done | `sun::k_GlareSamples` (`src/Graphics/Sun.h`): the middle and the four corners 500 units out across the world's x and up its y; test `Sun.GlareLooksAtTheMiddleAndTheFourCorners` |
| Each point is first pushed further along the line from the camera by the near distance | done | `sun::GlareSamplePoints` carries each point on by the near clip distance; test `Sun.GlarePointsStayAboveTheLowestAndFollowTheLineToTheSun` |
| No point is tested lower than height 10 | done | `sun::k_GlareLowestSample`; test `Sun.GlarePointsStayAboveTheLowestAndFollowTheLineToTheSun` |
| A point counts as hidden when the land lies between the camera and it, tested along the line by the land's cells | done | `PickingSystem::LandOrSeaAlong` → `land_line::LandOrSeaAlong`, the land line test by cells (`src/3D/LandLine.cpp`); tests `LandLine.*` (`test/test_picking.cpp`); the day land shot |
| A point also counts as hidden when the line from the camera dips below sea level within 7500 units of the camera, so the sea's horizon hides the setting sun | done | The same line test's sea part (`land_line::SeaLevelAlong`, 7500 units from the camera); test `LandLine.TheSeaIsMetGoingDownNearTheCamera` |
| At detail levels 3, 4 and 6, with the depth-reading option on, a point also counts as hidden when the depth buffer at its spot on the screen shows something nearer, so models hide it | chosen | `Renderer::AskGlareSampleDrawn` asks the drawing at each sample's pixel with an occlusion query at the levels 3, 4 and 6 (`detail_level::GlareDepthTest`). 🎯 Chosen difference (user, 2026-10-21): the answer comes a frame or two later than the original's, which reads that same frame's depth buffer ([sky.md](../../../world/sky.md#the-sun-and-its-glare)) |
| Looking straight at the sun with nothing in the way gives the full glare | done | `sun::EaseGlare`; test `Sun.GlareEasesTowardsWhatShows` |

## The sun's light

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit from a fixed sun far to the north west and up | done | `model_light::k_DefaultSun` (−500000, 500000, −500000, `src/Graphics/ModelLight.h`), by day (at full night the light is taken beside the hand, see lighting.md); tests `test/graphics/test_model_light.cpp` |
| Object shadows fall away from that fixed sun | done | `src/Graphics/ShadowMath.h` (the fixed sun point); the static shadows too, see ../terrain/landscape_rendering.md; tests `test/graphics/test_creature_shadow.cpp`, `test/test_block_shadows.cpp` |
| The land's and models' colour through the day comes from the light palette, not from the sun's height | done | see lighting.md (its tests there) |
| The creature can choose to look at the sun, which it may do only when it is not night | done | `creature_plan_actions::Possible` (`src/Creature/CreaturePlanActions.cpp`): LookAtSun only while not `Situation::visualNight` (the sky type of the visual time at most 1.2, as the original's test), unless forced; test `CreatureRestAndHome.TheSunIsLookedAtOnlyByDayAndTheMoonOnlyAtNight`; the look itself is the creature's, see ../creature/idle_behaviour.md ([sky.md](../../../world/sky.md#sky-type)) |
