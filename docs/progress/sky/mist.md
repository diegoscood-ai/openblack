# Mist and haze

Banks of mist the land's scripts lay over valleys and swamps, made of puffs of the smoke texture facing the camera, and
the distance haze that fades the world.

**Progress: 15/16 done, 0 chosen, 0 partial, 0 inferred — 94%**

How the original does it, in our wiki: [Day, night and weather in the original](../../../world/sky.md), [World rendering: original versus openblack](../../../world/haze-and-mist.md).

## Map mists

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script lays puffs of mist with a position, height, colour, size and edge shrink | done | `FeatureScriptCommands::CreateMist` (`src/LHScriptX/FeatureScriptCommands.cpp`), `MistArchetype` (`src/ECS/Archetypes/MistArchetype.cpp`); Land 1's script makes mists: `fidelity` and the land view shots |
| Each puff faces the camera | done | The billboard in `src/Graphics/RendererMists.cpp`; the land view shots |
| Puffs animate through sixteen frames of the smoke texture, starting at a random frame | done | `MistArchetype` (the start from the CRT stream), `frame_anim::MistCell`; the counter moves by each frame's whole steps, the fraction dropped as v1.42 ([frame clocks](../../../rendering/textures.md#frame-animated-textures-in-openblack), 2026-10-22); tests `test/test_mists.cpp`, `MistSystemTest.TheFractionOfAStepIsDroppedEachFrame`, `FrameAnim.mistStepAsTheOriginal`; debug: the Weather window's mists show their counter and cell |
| Puffs that shrink edge on are round from above and flatter seen level | done | The effect branch's non-uniform scale (`src/3D/Mists.cpp`); test `test/test_mists.cpp` |
| Edge-shrinking puffs are lit from above, the others by the land's light where they stand | done | `src/Graphics/RendererMists.cpp` (the effect branch's light from above, ambient 210, the normal branch's land light), as [haze-and-mist.md](../../../world/haze-and-mist.md#drawing); the land view shots |
| The land cells' colour adds to a mist's light | done | The normal branch in `src/Graphics/RendererMists.cpp`; the land view shots |
| Mists are sorted with other translucent things, farthest first | done | `Renderer::CollectMists` into the Z-sorted queue; the land view shots |
| The game moves on only the mists in view | done | `MistSystem::Update` (`src/ECS/Systems/Implementations/MistSystem.cpp`, `Locator::mistSystem`, with `mists::InView`); tests `MistSystemTest.OnlyTheMistsOnScreenMoveOn`, `NothingMovesWhileTheGameIsPaused` |
| Challenge scripts create a mist with a colour, scale and transparency | done | `CreateMist` in `src/CHLApi.cpp`, through `MistArchetype`; `test/test_script_mist_natives.cpp` |
| Challenge scripts fade a mist's scale and transparency over a time | done | `SetMistFade` in `src/CHLApi.cpp`, `ecs::mist_fade` (`src/ECS/MistFades.cpp`, stepped in the game turn's global lists); `test/test_mists.cpp`, `test/test_script_mist_natives.cpp` |
| Scripts read how far an object or mist has faded | done | GET_OBJECT_FADE answers the mist's fade progress, its steps taken over its steps (`ecs::mist_fade::Progress`, `src/ECS/MistFades.h`; a finished fade stays on its mist), 1 for what is not a mist, and for a mist never faded the not-a-number of nought over nought, as the original's zeroed fields give ([haze-and-mist.md](../../../world/haze-and-mist.md#map-mist-create_mist)); tests `ScriptMistNatives.ScriptObjectFade*`; the Weather debug window's Mists list |
| Mists are saved with the game | pending | Needs saved games: the original saves and loads each mist with its own values, as a thing of its own type in the saved game ([haze-and-mist.md](../../../world/haze-and-mist.md#map-mist-create_mist)); openblack has no saved games yet, see ../engine/ and the fork's issues #186 (reading the original's save files) and #187 (the writers per class) |

## Distance haze

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world fades into haze with distance from the camera | done | `src/Graphics/Haze.cpp`, `assets/shaders/haze.sh`; tests `Haze.*` (`test/test_haze_land_light.cpp`) |
| The haze colour is a third of the land's and its strength follows the land's brightness | done | `src/3D/LandLightTable.cpp`; tests `Haze.LitAndHazed`, `LandLightTable.DuskClear` |
| The haze closes in at dusk and under a storm | done | `src/3D/LandLightTable.cpp` (`sky_type::HazeFactor`, the overcast); tests `SkyType.LightColumnAndHaze`, `LandLightTable.NoonStormWithFlash` |
| Particle mists from spells and effects | done | The mist creator of the particles (`src/Particles/Creators/Mist.cpp`; test `Water.mistCreatorMakesTheCloudAtom`); see ../rendering/ |
