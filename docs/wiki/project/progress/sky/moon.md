# The moon

The moon is a small half sphere that keeps its place beside the camera, 4000 units along plus x (east). It swings up
and down on an ellipse with the script hour and is turned to face the camera. Its face is lit by the real moon's phase,
taken from the computer's date, and it is tinted the palette's moon colour for the alignment. A soft glow from the
atmosphere texture is added round it, and a mirrored copy is drawn for the sea.

**Progress: 38/38 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/sky.md).

## Where it stands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is placed relative to the camera, not the world, so it never gets nearer as the camera moves | done | `Renderer::DrawMoon` (`src/Graphics/Renderer.cpp`: the camera's origin plus the offset); test `Moon.SkyAnglesFromTheCamera` |
| The offset is (4000, 1100 × cos(a) − 150, 800 × sin(a)), with a = script hour × π/12 | done | `moon::Place` (`src/Graphics/Moon.cpp`); test `Moon.ShowsAroundMidnight` (`test/graphics/test_moon.cpp`) |
| So it is highest (950 up) at midnight and lowest (1250 down) at noon, swinging north and south by 800 as it goes | done | `moon::Place`; test `Moon.ShowsAroundMidnight` |
| It follows the script hour, not the visual hour | done | `Renderer::DrawMoon` takes the sky's time, the script time (`SkySystem::SetTime` from `GetScriptTime` each turn, `src/Game.cpp`); test `Moon.ShowsAroundMidnight` |
| The offset is worked out once a game turn, when the weather is updated | done | The original in `LH3DAtmos::UpdateGame` 0x8356E0, called only from `GGame::ProcessTurn` 0x54E5D7; ours in `Renderer::DrawMoon` each frame from the script time the sky takes once a turn, so the same offset; tests `Moon.TheOffsetIsTheSameInEveryFrameOfATurn`, `Moon.ShowsAroundMidnight` |

## Showing and fading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Its strength is floor(0.5 × height − 110), capped at 200 | done | `moon::Place`; test `Moon.ShowsAroundMidnight` |
| It is drawn only while that is above 0: height over 220, about 19:20 to 04:40 script time | done | `moon::Place` returns nothing below; test `Moon.ShowsAroundMidnight` |
| It is at its full 200 from height 620 up, about 21:00 to 03:00 | done | `moon::Place`; test `Moon.ShowsAroundMidnight` |
| With the fog option on and an overcast, its strength is divided by (8 × overcast + 1), truncated | done | `Renderer::DrawMoon` weakens the moon and its glow through `sky_dome::ThroughOvercast` with the Fog key (`src/3D/SkyDome.cpp`), as the sun; test `SkyDome.SunAndMoonDimThroughAnOvercast` |
| When the moon is down, neither it nor its glow is drawn | done | `Renderer::DrawMoon` returns early, as the original's m > 0 gate (`fn_0086B010` 0x86B61D); test `Moon.ShowsAroundMidnight` |

## Its shape and facing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The mesh is the weather folder's moon model, a half sphere about 100 units in radius | done | `moon.l3d` loaded by the sky (`src/ECS/Systems/Implementations/SkySystem.cpp`); the night land shot (22 h); tests `MoonData.TheWeatherFoldersMoonIsAHalfSphereAboutAHundredUnitsInRadius` (with the game's data), `SkyArchetypeTest.MakesTheDomeTheSunAndTheMoonWithWhatTheyAreDrawnFrom` |
| Its axes are built square to the line from the camera and kept upright, then scaled by 4 | done | `moon::Basis`; tests `Moon.FacesTheCamera`, `Moon.FacesTheEyeInTheView` |
| The mesh leans back by 7.5 degrees (0.1309 radians) | done | `moon::Model` (`k_Tilt`), as the exe's [0x9A3BF0] at 0x86AEF3; test `Moon.FacesTheCamera` |
| It is turned about its upright axis by the phase plus half a turn | done | `moon::Model`; test `Moon.FacesTheCamera` |
| It is drawn at 0.65 of the basis, so about 270 units in radius at about 4100 units away | done | `moon::Model` (`k_MeshScale`); test `Moon.FacesTheCamera` |

## Its face and phase

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The phase comes from the computer's clock, in whole days since 1 January 1970 | done | `moon::Phase`; test `Moon.PhaseFollowsTheRealMoon` |
| Days are counted from 10962 days after 1970 (6 January 2000, a new moon) | done | `moon::Phase`; tests `Moon.PhaseFollowsTheRealMoon`, `Moon.MonthFractionCountsWholeDays` |
| Moon months are days × 0.0338631801 (one every 29.53 days); the phase is (1 − the fraction of a month) × 2π | done | `moon::Phase`; test `Moon.PhaseFollowsTheRealMoon` |
| The game reads the clock at most once every 2 seconds and keeps the phase between reads | done | `sky_frame::TakeMoonPhase` (`src/3D/SkyFrame.cpp`); test `SkyFrame.TheDateIsReadAtMostOnceEveryTwoSeconds` (`test/test_sky_frame.cpp`) |
| The face's texture coordinates are projected from the mesh's points turned by the phase: u = (x·cos p − z·sin p) × 0.0025 + 0.25, v = y × 0.0025 + 0.25 | done | `assets/shaders/vs_celestial.sc` with the phase's cosine and sine from `moon::FaceInputs` (`src/Graphics/Moon.cpp`); the night land shot (22 h); tests `CelestialShader.TheMoonsFaceIsProjectedFromItsPointsTurnedByThePhase`, `Moon.TheFaceIsTurnedByThePhasesCosineAndSine` |
| So the face shows the top-left quarter of weather.raw, and the lit part swings across it with the phase | done | `raw/weather` in `Renderer::DrawMoon`; the night land shot (22 h); test `MoonData.TheFaceShowsTheTopLeftQuarterOfTheWeatherPictureSwungByThePhase` (with the game's data: the model's points reach a little past the quarter, at most 105 units from its middle, 0.2625 of the picture) |
| The disc's outline comes from weathera.raw, the alpha texture | done | `raw/weathera` and `assets/shaders/fs_celestial.sc`; the night land shot (22 h); tests `MoonData.TheDiscsOutlineIsTheAlphaPicturesDiscInTheTopLeftQuarter` (with the game's data), `CelestialShader.TheMoonsOutlineIsTheAlphaOfItsSecondTexture`, `SkyArchetypeTest.MakesTheDomeTheSunAndTheMoonWithWhatTheyAreDrawnFrom` |
| There is no separate earthshine: the dark side is whatever the texture shows | done | Nothing else is drawn for it; the original's sky stage `fn_0086B010` draws only the dome, the sun, the moon's glow and the moon; test `SkyStage.TheMainPassDrawsTheDomeTheSunAndTheMoonThenQueuesTheClouds` |
| Scripts ask how full the moon is: 1 − phase / π below π, (phase − π) / π from π, so 0 halfway through the moon month and 1 at either end | done | GET_MOON_PERCENTAGE pushes `moon::ScriptPercentage` of the phase the moon last showed with (`src/CHLApi.cpp`); tests `test/graphics/test_moon.cpp`, `test/test_moon_percentage.cpp` |
| Its phase is taken only while it shows: 0 until it first shows (so scripts are told 1, a new moon), then kept while it is down | done | `sky_frame::TakeMoonPhase` only for a showing moon in `Renderer::DrawMoon`, as the exe's [0xFA2724], written only at 0x86AEDD; test `SkyFrame.TheMoonStartsAsANewMoonWithNoDateRead` ([sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)) |

## Colour

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is tinted the light palette's moon colour for the alignment the sky shows | done | `LandLightTable::GetMoonColour` (row 5 of `palette.raw` by alignment); see lighting.md; the night land shot (22 h); test `LandLightTable.TheNightIsLitByTheLandRowsNotTheMoonsColour` |
| The tint's alpha is the moon's strength | done | `Renderer::DrawMoon`; the night land shot (22 h); test `Moon.TheTintsAlphaIsTheStrengthAndItsColourStaysPut` |
| The moon's colour doesn't change through the night, only its strength | done | `Renderer::DrawMoon`; the night land shot (22 h); test `Moon.TheTintsAlphaIsTheStrengthAndItsColourStaysPut`, test `LandLightTable.TheMoonsColourStaysPutThroughTheNight` |

## The glow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A square glow is drawn first, centred on the moon, 500 units each way along the moon's axes (so 4000 units across once scaled by 4) | done | `moon::MakeGlow`; test `Moon.GlowIsASquareAboutTheMoon` |
| It shows the part of the atmosphere texture from 0.25 to 0.49375 both ways, with its alpha texture | done | `moon::MakeGlow` (`raw/ATMOS`, `raw/ATMOSA`); test `Moon.GlowIsASquareAboutTheMoon` |
| Its colour is the moon colour with red a sixth, green a fifth and blue a quarter, at the moon's strength | done | `moon::GlowColour`; the night land shot (22 h); test `Moon.GlowIsASquareAboutTheMoon` |
| It is added to the sky | done | `Renderer::DrawMoon` (additive); the night land shot (22 h); test `Moon.TheGlowIsAddedToTheSky` |

## Draw order, depth and the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is drawn in the sky after the dome and the sun, before the land and everything else | done | The sky stage of the main and reflection passes (`src/Graphics/Renderer.cpp`); the night land shot (22 h); test `SkyStage.TheMainPassDrawsTheDomeTheSunAndTheMoonThenQueuesTheClouds`, test `SkyStage.TheSeasSkyHasTheMoonButNoSun` |
| The land drawn later covers it | done | The land is drawn after it; the moon tests depth less or equal and writes it, as the original's alternate mode table (`Renderer::DrawMoon`; [sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)); the night land shot (22 h); test `Moon.TheLandDrawnLaterCoversIt` |
| A second moon is drawn for the sea's sky: the same moon with its height below the camera flipped and its lean reversed, seen through the sea | done | `Renderer::DrawMoon` in the reflection pass: the glow from the mirrored view and the mesh mirrored, as our wiki describes ([rendering.md](../../../world/sky.md#sky-sun-moon-and-clouds-original)); test `Moon.TheSeaShowsTheGlowOfACopyMirroredThroughSeaLevel` |
| Moonlight: at night the land and models take the palette's moon colour | done | `src/3D/LandLightTable.cpp`; see lighting.md (its tests there) |
| The creature can choose to look at the moon, only at night | done | `creature_plan_actions::Possible` (`src/Creature/CreaturePlanActions.cpp`): LookAtMoon only while `Situation::visualNight` (the sky type of the visual time above 1.2, as the original's test), unless forced; test `CreatureRestAndHome.TheSunIsLookedAtOnlyByDayAndTheMoonOnlyAtNight`; the look itself is the creature's, see ../creature/idle_behaviour.md ([sky.md](../../../world/sky.md#sky-type)) |
| Near a full moon, at night, the game's guidance shows a help sprite about the moon now and then | done | `guidance::RemarkOnMoonPhase` (`src/Audio/Services/Guidance.cpp`: at visual night near the full moon, then a long wait); test `test/audio/test_guidance.cpp` |
