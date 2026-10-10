# Reflections

The sea mirrors the world above it: the sky and the land are drawn upside down through the sea level before the sea is
blended over them, so islands, trees and buildings show in the water.

**Progress: 12/12 done, 0 partial — 100%**

How the original does it, in our wiki: [Water in the game](../../bw1-notes/water.md).

## The mirrored world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky is drawn mirrored first, in its own pass | done | The sky stage of the reflection pass (`src/Graphics/Renderer.cpp`) |
| The land is drawn mirrored from a camera reflected through the sea level | done | The reflection pass in `src/Graphics/Renderer.cpp` (`Camera::Reflect`) |
| The mirrored land is lit at half the land's light | done | `sea_pass::ForPass` (`src/Graphics/SeaPass.h`), `src/Graphics/Renderer.cpp` |
| The mirrored land has no small bump detail | done | The reflection pass drops the small bump (`sea_pass::SeaPassState::smallBump`) |
| Only what stands above the sea is mirrored: models are cut at sea level | done | The reflection draws no models (`drawEntities` off), only the parts drawn under the water: the hand, what it holds, flying objects and the boat (`DrawUnderWater`, `DrawObjectReflections`). Our wiki differs: the original mirrors no models, only those few under-water draws ([rendering.md](../../bw1-notes/rendering.md#sea-skyraw--skyaraw)) |
| Models in the reflection take half the land's light | done | No models are mirrored; the hand's under-water draw takes its fixed colour (`sea_pass::k_HandColour`). Our wiki differs: the reflection holds no models ([rendering.md](../../bw1-notes/rendering.md#sea-skyraw--skyaraw)) |
| Translucent things (mists, clouds, the hand) blend in the reflection, farthest first | done | The reflection draws no sprites or mists (`drawSprites` off). Our wiki differs: the original's reflection has no models or sprites ([rendering.md](../../bw1-notes/rendering.md#sea-skyraw--skyaraw)) |
| The game mirrors only the sky, the land and a few moving things; openblack mirrors everything | done | Our tree now mirrors as the game: the sky, the land (LandRef) and the under-water draws (`src/Graphics/Renderer.cpp`) |
| The reflection target resizes with the view | done | The reflection target follows the main view's size (`Renderer::UpdateReflectionTarget`, `src/3D/OceanInterface.h`) |
| Swimmers, sharks and fishing nets are cut by the sea plane | done | `DrawCutBelowWater` (sharks), `DrawFishPlots` (nets) and the swimmers' cut in the reflection pass (`src/Graphics/Renderer.cpp`) |
| The sun and moon do not show in the sea | done | The sun is not reflected; the moon is (`Renderer::DrawMoon` in the reflection pass). Our wiki differs: the original draws the moon's reflection, with its glow ([rendering.md](../../bw1-notes/rendering.md#sky-sun-moon-and-clouds-original)) |
| The temple's pool reflects the room above it | done | `DrawTempleReflection` (`src/Graphics/Renderer.cpp`), `assets/shaders/fs_reflection.sc`; see ../temple/ |
