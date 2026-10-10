# Mist and haze

Banks of mist the land's scripts lay over valleys and swamps, made of puffs of the smoke texture facing the camera, and
the distance haze that fades the world.

**Progress: 12/17 done, 0 partial — 71%**

How the original does it, in our wiki: [Day, night and weather in the original](../../bw1-notes/day-night-weather.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Map mists

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's script lays puffs of mist with a position, height, colour, size and edge shrink | done | `FeatureScriptCommands::CreateMist` (`src/LHScriptX/FeatureScriptCommands.cpp`), `MistArchetype` (`src/ECS/Archetypes/MistArchetype.cpp`) |
| Each puff faces the camera | done | The billboard in `src/Graphics/RendererMists.cpp` |
| Puffs animate through sixteen frames of the smoke texture, starting at a random frame | done | `MistArchetype` (the start from the CRT stream), `frame_anim::MistCell`; test `test/test_mists.cpp` |
| Puffs that shrink edge on are round from above and flatter seen level | done | The effect branch's non-uniform scale (`src/3D/Mists.cpp`); test `test/test_mists.cpp` |
| Edge-shrinking puffs are lit from above, the others by the land's light where they stand | done | `src/Graphics/RendererMists.cpp` (the effect branch's light from above, the normal branch's land light) |
| The land cells' colour adds to a mist's light | done | The normal branch in `src/Graphics/RendererMists.cpp` |
| Mists are sorted with other translucent things, farthest first | done | `Renderer::CollectMists` into the Z-sorted queue |
| The game moves on only the mists in view; openblack moves them all | done | Our tree also advances only the mists in view (`MistSystem::Update`, `src/ECS/Systems/Implementations/MistSystem.cpp`, `Locator::mistSystem`, with `mists::InView`) |
| Challenge scripts create a mist with a colour, scale and transparency | todo | CREATE_MIST is a stub in `src/CHLApi.cpp` |
| Challenge scripts fade a mist's scale and transparency over a time | todo | SET_MIST_FADE is a stub in `src/CHLApi.cpp` |
| Scripts read how far an object or mist has faded | todo | GET_OBJECT_FADE is a stub in `src/CHLApi.cpp` |
| Mists are saved with the game | todo | openblack has no saved games. See ../engine/ |
| Creatures notice mists as objects in the world | todo | See ../creature/ |

## Distance haze

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world fades into haze with distance from the camera | done | `src/Graphics/Haze.cpp`, `assets/shaders/haze.sh` |
| The haze colour is a third of the land's and its strength follows the land's brightness | done | `src/3D/LandLightTable.cpp` |
| The haze closes in at dusk and under a storm | done | `src/3D/LandLightTable.cpp` (`sky_type::HazeFactor`, the overcast) |
| Particle mists from spells and effects | done | The mist creator of the particles (`src/Particles/Creators/Mist.cpp`; test `Water.mistCreatorMakesTheCloudAtom`); see ../rendering/ |
