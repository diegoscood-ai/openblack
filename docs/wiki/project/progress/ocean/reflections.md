# Reflections

The sea mirrors the world above it: the sky and the land are drawn upside down through the sea level before the sea is
blended over them, so islands, trees and buildings show in the water.

**Progress: 13/13 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Sea, coast and rivers](../../../world/sea-coast-and-rivers.md#the-under-sea-pass).

## The mirrored world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sky is drawn mirrored first, in its own pass | done | The sky stage of the reflection pass (`src/Graphics/Renderer.cpp`); check: land view shots |
| The land is drawn mirrored from a camera reflected through the sea level | done | The reflection pass in `src/Graphics/Renderer.cpp` (`Camera::Reflect`); test `SeaPass.Mirror` (`test/test_sea_pass.cpp`) |
| The mirrored land is lit at half the land's light | done | `sea_pass::ForPass` (`src/Graphics/SeaPass.h`); test `SeaPass.PassState` |
| The mirrored land has no small bump detail | done | `sea_pass::SeaPassState::smallBump`; test `SeaPass.PassState` |
| The mirrored land keeps the land's blend, so the river channels show in the reflection | done | The pass's land state keeps the blend and only turns the Z write off (`src/Graphics/SeaPass.h`, `Renderer.cpp`), as the original's mirrored land goes through the land's own block draw; test `SeaPass.MirroredLandKeepsTheLandsBlend` ([sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#rivers)) |
| The reflection holds only the sky, the moon, the land and the under-water draws: no models, sprites or mists | done | `drawEntities` and `drawSprites` off in the reflection pass (`src/Graphics/Renderer.cpp`); [sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#sea) |
| The hand, what it holds, flying objects and the boat are drawn mirrored under the water in fixed colours | done | `DrawUnderWater`, `DrawObjectReflections`, `sea_pass::k_HandColour`; tests `SeaReflection.*` (`test/graphics/test_sea_reflection.cpp`), `SeaPass.Colours` |
| A creature in the shallows in view is mirrored | done | `DrawCreatureReflection` (`src/Graphics/RendererCut.cpp`); tests `SeaPassCreature.*` (`test/test_sea_pass.cpp`) |
| Swimmers, sharks and fishing nets are cut by the sea plane | done | `DrawCutBelowWater` (sharks), `DrawFishPlots` (nets), the swimmers' `DrawCutByPlane` (`src/Graphics/Renderer.cpp`); tests `SeaPass.KeptSide`, `SeaPass.PlanesAreTheBinarysDwords` |
| Effects by the sea (the shield dome, mesh particles) are cut flush with the water | done | `sea_pass::CutAtoms`, each atom's own colour, specular and alpha (`particles::mesh_atoms::Fade`, the vertex alpha the cut takes from the object's colour); test `SeaPass.CutAtomsTakeTheirOwnAlpha` ([sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#the-under-sea-pass)) |
| Morphable objects that stick to the ground (houses, fields, the ark) are not mirrored | done | The reflection draws no models and they have no under-water draw (the original's is empty); [sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#the-under-sea-pass) |
| The moon and its glow show in the sea; the sun does not | done | `Renderer::DrawMoon` in the reflection pass, no sun there; [sky.md](../../../world/sky.md#sky-sun-moon-and-clouds-original); check: night land shots |
| The temple's pool reflects the room above it | done | `DrawTempleReflection` (`src/Graphics/Renderer.cpp`), `assets/shaders/fs_reflection.sc`; see ../temple/ |
