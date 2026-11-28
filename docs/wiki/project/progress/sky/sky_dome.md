# The sky dome

The sky is a dome of pictures: for each alignment (good, neutral, evil) one by day, one at dusk and one at night,
blended for the time of day and mixed by the alignment the sky shows, then tinted by the weather.

**Progress: 17/17 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/sky.md).

## Time of day

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky type runs from 0 at night through 1 at dusk to 2 by day | done | `DayNightClock` sky type (`src/3D/SkyType.cpp`); test `SkyType.At` (`test/test_sky_type.cpp`) |
| The dome blends day into dusk, or dusk into night, by the sky type | done | `sky_type::DomeBlend`, `SkySystem::BlendDome`; tests `SkyType.DomeWeight`, `SkyType.DomeBlendHysteresisAndRows` |
| The blend is rebuilt only once the sky has moved on far enough (a hysteresis of 0.03) | done | `sky_type::k_Hysteresis` (`src/3D/SkyType.h`); tests `SkyType.DomeBlendHysteresisAndRows`, `SkyType.DomeBlendHysteresisFloatDifference` |
| It is then rebuilt a band of 32 rows a frame, top down | done | `sky_type::DomeBlend` (`src/3D/SkyType.cpp`); test `SkyType.DomeBlendHysteresisAndRows` |
| After the clock jumps the whole dome is rebuilt at once | done | SET_GAME_TIME through `SetScriptTime`; test `SkyType.ForceScriptTimeJumps` |
| Texels blend in whole steps of the pictures' 5-bit channels | done | `sky_type::BlendRows555`; test `SkyType.BlendTexel555` |
| The blend is done on the CPU, a band of rows at a time, as the game does | done | `SkySystem::BlendDome` → `sky_type::BlendRows555` (`src/ECS/Systems/Implementations/SkySystem.cpp`); test `SkyType.BlendTexel555` |
| At the two lowest detail levels the dome is not blended: one copy of the day pictures at half size, tinted by the sky type | done | `SkySystem::CopyDayDome` (the day pictures halved by `sky_type::HalveTexels`, copied once) and `sky_dome::NoBlendColour` (the land's colour by the sky type) in `Renderer::UpdateSkyTint`, at `detail_level::SkyNoBlend`; tests `SkyDome.NoBlendTintMatchesTheOriginal` (the exe's colours), `SkyType.HalveTexelsKeepsEveryOtherRowAndColumn`; the World debug menu shows which dome is drawn ([sky.md](../../../world/sky.md#sky-type)) |

## Alignment

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each alignment has its own dome; the sky mixes the two nearest by the alignment it shows | done | `src/3D/SkyDome.cpp`; test `SkyDome.AlignmentPairs` (`test/test_sky_dome.cpp`) |
| The alignment's opacity is cut to 255ths | done | `src/3D/SkyDome.cpp` (an integer weight of 255, as `fn_0086B010`); test `SkyDome.AlignmentPairs` |
| The sky turns towards the alignment at the camera over time, none while paused | done | `AlignmentSystem` (`src/ECS/Systems/AlignmentSystemInterface.h`); tests `Alignment.theSkyTurnsTowardItsTargetByGameTime`, `Alignment.withoutAnOverrideTheSkyFollowsTheCamera` (`test/test_alignment.cpp`) |
| A **good** sky is darkened, up to 90 (d = ftol(((1 - X/2) - 0.6) x 225), X the sky alignment 0 good to 2 evil), with the weather detail option on. Differs from raffclar's reading (an evil sky darkened): `DrawSky` 0x5E21F1..0x5E2220, `LH3DAtmos::Update3D` 0x835A2D..0x835A78; see [sky.md](../../../world/sky.md#sky-type), the dome's draw | done | `sky_dome::Darkness` fed X; test `SkyDome.DarknessOfAGoodSky` |
| Inside the temple the sky dome is still drawn (seen through its openings), without the sun | done | `src/Graphics/RendererTemple.cpp` draws the dome and the moon, no sun and no glare; see sun_and_moon.md and ../temple/; the temple scene shots; test `SkyStage.InsideTheTempleTheDomeAndTheMoonAreDrawnWithoutTheSun` |

## Weather on the sky

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An overcast turns the dome towards the haze's colour, with the fog detail option on | done | `src/3D/SkyDome.cpp`; test `SkyDome.OvercastTurnsTowardsTheHaze` |
| A flash of lightning turns the dome towards white | done | `src/3D/SkyDome.cpp`, `fs_sky.sc`; test `SkyDome.DarknessDarkensAndFlashWhitens` |
| The sky is drawn first, before the scene, and in the sea's reflection | done | `Renderer::DrawPass` draws the sky dome before anything else in both the `RenderPass::Main` and `RenderPass::Reflection` views, culled by the mirrored camera in the reflection (`sea_pass::Surface::Sky`; test `SeaPass.FaceCullIsTheOldExpressions`); the day and night land shots |
| Stars are part of the night pictures, with no separate star field | done | The night domes carry them; the original's sky stage (`fn_0086B010`) calls only the dome (`fn_0086B6B0`), the sun (`fn_0086C140`) and the moon (`fn_0086AC60`, `fn_0086A930`), so it adds none; test `SkyStage.TheMainPassDrawsTheDomeTheSunAndTheMoonThenQueuesTheClouds` |
