# Models and lighting

How the game draws its 3D models (buildings, trees, villagers, animals, creatures, the hand) and how it lights them: the
material modes, skinning, and the sun, ambient and night lights that shade them. A creature's look and body morphs are
in [../creature/](../creature/), the hand's look in [../hand/](../hand/), the sky's light by time of day in
[../sky/](../sky/).

**Progress: 16/26 done, 6 partial — 73%**

How the original does it, in our wiki: [Model rendering: original versus openblack](../../../rendering/model-lighting.md), [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## Meshes and skinning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models from the game's mesh pack and model files are drawn with their own textures | done | `src/3D/L3DMesh.cpp`, `src/Graphics/Renderer.cpp` (mesh and submesh drawing) |
| Many copies of one model are drawn together | done | instanced draws in `src/Graphics/Renderer.cpp`; openblack batches them, the look is the same |
| Boned models are posed by their skeletons as they animate | done | villagers and animals posed by their clips (`components::SkeletalAnimation`, `src/3D/SkeletalPose.cpp`, drawn per pose in `Renderer.cpp`), the creature (`CreatureAnimationSystem`) and the hand (`src/3D/HandAnimator.cpp`) |
| Where the parts of a boned body meet, seam vertices are pulled towards their partners so shoulders, neck and tail bend instead of tearing | pending | no seam blending between a boned body's parts in our tree |
| Poses cross-fade from one animation into the next | partial 50% | the hand (`src/3D/HandAnimator.cpp`) and the high-detail villagers (`StepCrossFade`, `ComputeBlendedPose` in `src/ECS/Animations.cpp`, `src/3D/SkeletalPose.cpp`) cross-fade; ordinary villagers and animals switch clips at once; re-measured 2026-10-21: the hand and the high-detail villagers cross-fade; ordinary villagers and animals switch at once |
| Parts of a model shown only in some states (building stages, graves) | partial 40% | an object draws the parts whose status is no more than its own (`graphics::mesh_status`); only a graveyard has one (its graves); the building stages are not drawn by status; re-measured 2026-10-21: a graveyard's graves are drawn by status; the building stages are not |
| Collision-only parts of models are never drawn | done | `Renderer.cpp` skips physics submeshes |

## Materials and render modes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each material is drawn in one of the game's 19 render modes: its blending, alpha test, depth write and how texture alpha combines with vertex colour | done | `src/Graphics/RenderModes.cpp` (`render_modes::State`) |
| Two-sided materials are drawn from both sides, the others have their back faces culled | done | per-primitive culling in `Renderer.cpp`, as the original sets it for every primitive; a primitive that keeps the previous one's state must have the same two-sided flag (`graphics::shared_primitive_state::SharesState`, test `test/test_shared_primitive_state.cpp`; before, the Norse workshop's yard fence and 26 other primitives got their neighbour's culling, #189); the mesh viewer shows each primitive's culling |
| Cut-out textures (leaves, fences) drop their see-through texels | done | alpha-tested render modes (`src/Graphics/RenderModes.cpp`), the threshold minus 5 for an object with its own alpha; a DXT1 pack texture with transparent texels is decoded with its alpha and uploaded as RGBA8, since bgfx's Vulkan BC1 has none (`src/3D/DxtAlpha.h`, test `test/test_dxt_alpha.cpp`) |
| Textures that slide across a model or play frames | partial 80% | frame-animated textures (`src/3D/FrameAnim.cpp`, test `test/test_frame_anim.cpp`) and sliding textures for mists, particle models and the temple; whether every sliding material of the game's models is covered is unconfirmed; re-measured 2026-10-21: frame-playing and sliding textures are drawn; that every sliding material of the models is covered is not checked |
| A model's materials swapped for others while the game runs | pending | only the particle meshes swap their materials' properties (`src/Particles/Creators/Mesh.cpp`); no other material swaps |
| Textures cut to the game's low colour formats (16-bit) | done | the ARGB4444 and 555 cuts at load time (`src/Graphics/Argb4444.h`, `src/Graphics/Rgb16.h`, `Texture2DLoader` in `src/Resources/Loaders.cpp`); tests `test/test_argb4444.cpp` |
| Shiny environment map over some models (frozen things' ice, the cave's trophies) | pending | no environment map: the cave's trophies are drawn without it (`src/Graphics/RendererTemple.cpp`) |

## Lighting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Models are lit per vertex by one light and an ambient level, in whole numbers | done | `src/Graphics/ModelLight.cpp`, `assets/shaders/model_light.sh`; test `ArgbColour.ScaleRgbShift8KeepAlphaMatchesModelLight` |
| The light is a distant sun by day and moves beside the hand in the darker half of the night; inside the temple a light of the temple's own | done | `model_light::UpdateFrameLight` (`src/Graphics/ModelLight.cpp`); inside the temple `model_light::InsideTemple` (−1000, 1000, −500 from its origin, read in the exe) for the whole temple pass (`src/Graphics/RendererTemple.cpp`), with the temple's colours (`src/3D/TempleLight.cpp`); test `ModelLight.TheTemplesLightInsideTheTemple` |
| Models take the colour of the land's light where they stand | done | the cell light texture filtered in `vs_object` (`assets/shaders/vs_object.sc`, `land_light.sh`) |
| Trees take the land's light scaled by a brightness worked out each frame from where the camera looks against the light | done | `src/Graphics/TreeBrightness.cpp`; test `test/test_tree_brightness.cpp` |
| Big forests, dead trees and flowers are drawn unlit | partial 20% | an unlit colour path exists (`vs_object.sc`, used for windows and reflections); which models the original draws unlit is not in our wiki; re-measured 2026-10-21: the unlit path exists; it is not applied to those models |
| The hand is drawn half again as bright as the land's light under it | done | `lightBoost` 1.5 for the hand's mesh (`src/Graphics/Renderer.cpp`) |
| Homes with people inside show lit windows at night, blended by their material's alpha | done | `night_lights::WindowColour` (`src/3D/NightLights.cpp`) with the villagers' presence at home |
| Village lights and the hand's light brighten models near them at night | partial 70% | the lanterns and the hand's light brighten the land's light (`src/3D/NightLights.cpp` through `Locator::villageLightSystem`, `src/Graphics/HandLight.cpp`), which the models take; whether models are lit exactly as the game does is unconfirmed; re-measured 2026-10-21: models take the land's light the lanterns and hand brighten; the exact lighting of models is not checked |
| Lights at night cast light and shadows on nearby objects | pending | the game adds lights of several kinds to a dynamic light and shadow list; not started |
| A lightning flash brightens the world | done | the flash goes into the land light table at the camera (`src/3D/LandLightTable.cpp`, `src/3D/Lightning.cpp`), which land and models take; test `test/test_lightning_flash.cpp`; see [../weather/](../weather/) |
| Hot and burning objects glow red-orange, flickering | done | `src/ECS/Fire/FireGraphic.cpp`; see [../physics/](../physics/) |
| Scripts give objects their own colour and alpha | partial 40% | an object colour and alpha exist in the mesh draw (`RendererInterface.h`); SET_OBJECT_FADE_IN no longer is a stub: a creature drops out of sight and fizzes back in over the seconds, and the game fades nothing else in (`SetObjectFadeIn`, `src/CHLApi.cpp:7985`); which other script calls reach the colour and alpha is unconfirmed; re-measured 2026-10-21: the colour and alpha exist in the draw; SET_OBJECT_FADE_IN is a stub |
