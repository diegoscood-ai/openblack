# Sun and moon

How the sun and the moon share the sky. The detail is in [sun.md](sun.md) and [moon.md](moon.md). This page covers what
they have in common: the hours that switch between them, and how they behave in the temple, while paused, in cut scenes
and with the detail options.

**Progress: 11/12 done, 1 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/sky.md).

## Day and night between them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Both follow the script hour, which the clock stretches so dusk and dawn fall at fixed hours | done | `DayNightClock::GetScriptTime` (`src/3D/DayNightClock.cpp`); see day_night_cycle.md; tests `Sun.RisesAndSetsWithTheScriptTime`, `Moon.ShowsAroundMidnight` |
| The sun shows from 3 to 21 and the moon from about 19:20 to 04:40, so both are up at dusk and dawn | done | `sun::Place`, `moon::Place`; tests `Sun.RisesAndSetsWithTheScriptTime`, `Moon.ShowsAroundMidnight` |
| Jumping or moving the clock by script moves them at once | done | SET_GAME_TIME and MOVE_GAME_TIME in `src/CHLApi.cpp` (`Game::SetTime` gives the sky the new script time); test `SkyType.ForceScriptTimeJumps` |
| The sky's dome, the land's light and the haze change with the hour on their own; the sun and moon light nothing themselves | done | see sky_dome.md and lighting.md; the sun's and moon's draws (`fn_0086C140`, `fn_0086A930`) set no light |

## In the temple, paused and in cut scenes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Inside the temple the sky is drawn without the sun and without its glare; the moon is still drawn | done | `src/Graphics/RendererTemple.cpp` draws the dome and the moon with no sun and no glare, as the citadel flag [0xFA2760] gates `fn_0086C140` and `fn_0086BB60`; the temple scene shots |
| While the game is paused the clock stops, so neither moves, and the glare holds | done | No game turn while paused; `sun::EaseGlare` holds with 0 ms; test `Sun.GlareEasesTowardsWhatShows` |
| The moon's phase follows the real date even while paused | done | `moon::Phase` through `sky_frame::TakeMoonPhase`, read by the machine's clock; test `SkyFrame.TheDateIsReadAtMostOnceEveryTwoSeconds` |
| The moon stays beside the camera wherever it moves, cut scenes included | done | `Renderer::DrawMoon`; test `Moon.SkyAnglesFromTheCamera` |
| Cut scenes don't change how they are drawn | done | No cut-scene switch in `Renderer::DrawSun` or `DrawMoon`; the exe's sun reads only the citadel flag (`fn_0086C140`) |
| The script switch that hides the sun is cleared when the game restarts its scripts | done | `Game::PrepareNewLand` shows the sun again at the script reboot (`ecs::sun_draw::ShowAgain`), and so does a frame drawn inside the temple, as the original; see sun.md; test `test/test_sun_draw.cpp` |

## Options and openblack-only places

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The fog option decides whether an overcast dims the sun, its glare and the moon | done | The Fog key (`detail_level::Fog`) gates `sky_dome::ThroughOvercast` for the sun, its glare and the moon (`Renderer.cpp`); tests `SkyDome.SunAndMoonDimThroughAnOvercast`, `Sun.GlareAlphaIsAWholeNumberThroughTheOvercast` |
| At detail levels 3, 4 and 6 the glare can also be hidden by models, through the depth buffer | chosen | An occlusion query at each sample's pixel at the levels 3, 4 and 6 (`Renderer::AskGlareSampleDrawn`). 🎯 Chosen difference (user, 2026-10-21): its answer comes a frame or two late, where the original reads that frame's depth buffer; see sun.md |
