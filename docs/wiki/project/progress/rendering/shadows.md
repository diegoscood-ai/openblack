# Shadows

The shadows the game draws: the fixed shadows of trees, rocks and buildings on the land, the creature's and the hand's
moving shadows, and the villagers' small ground blobs. Cloud shadows are in [../sky/](../sky/); particle effects that
darken the land are in [particles.md](particles.md).

**Progress: 13/18 done, 2 partial — 78%**

How the original does it, in our wiki: [Model rendering: original versus openblack](../../../rendering/shadows-and-reflections.md), [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/shadows-and-reflections.md).

## Shadows on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, rocks, buildings and features cast shadows onto the land from a fixed far sun, as long as they are high | done | baked into each block's texture at its prints step as the game does (`block_texture::DrawShadowTriangles` and `DrawTextureShadow`, the casters from `ecs::land_prints::SourcesFor`; tests `BlockShadows.*`, the original's masks byte for byte; #222) |
| Where shadows overlap the land gets no darker; a fully covered spot is half as bright | done | 8 samples a texel, each set once however many shadows cover it; the texel darkened by 255 x the share covered / 8, halved: x 128 / 256 fully covered (`block_texture::ApplyShadow`, `ShadowTable` read back from the original) |
| Shadows follow objects as they are built, felled or moved | done | the static shadow pass is drawn each frame (`Renderer.cpp`) |
| At night homes, the village centre, storehouse, trees and totems cast shadows from the lights | pending | the game marks which objects cast shadows at night (unconfirmed how they are drawn) |
| Shadows projected over other objects, not only the land | done | the projected shadow list (`src/Graphics/ShadowList.cpp`, `RendererShadows.cpp`): the hand, the creature and the boat also onto the objects behind them; tests `ShadowMath.*`, `ShadowReceiver.*` |
| The land's own baked shadow map | pending | our static shadow pass covers the objects' shadows only; what this baked map is is not in our wiki |
| Short-lived shadows the game keeps for a while | pending | the high-detail villagers' temporary shadows are not ported |

## Creature shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's silhouette, seen from the light, is drawn into a soft small texture and projected onto the land and what stands on it | done | `shadow_list` complex caster with the creature's light (`src/Graphics/ShadowList.cpp`, `ShadowMath.cpp`), onto the land and what stands on it |
| The light is kept at least 45 degrees up and no closer than three of the creature's radii | done | `shadow_math::LightCreature`; test `ShadowMath.Lights` |
| The shadow fades out as the camera pulls away, between 50 and 80 creature radii | done | the fade by distance in `src/Graphics/ShadowMath.cpp`; test `ShadowMath.FadeByDistance` |
| The creature's hair casts no shadow | partial 50% | the creature's body is the caster (`ShadowList.cpp`); whether its hair is left out is unchecked; re-measured 2026-10-21: the body casts; whether our caster leaves the hair out is not checked |
| Creature shadows don't fall on creatures themselves | done | creatures are not receivers (`src/ECS/ShadowReceiver.h`); test `ShadowReceiver.ACreatureDoesNotReceive` |
| Several creatures' shadows at once, the nearest first | partial 60% | every drawn creature gets a shadow, with no cap (`ShadowList.cpp`); the game's limit is unconfirmed; re-measured 2026-10-21: every drawn creature casts, nearest first; the original's cap is not confirmed |

## Hand shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's silhouette is projected onto the land, darkening it | done | the hand's 32x32 silhouette with its vertical light, as the original's (`shadow_list`, compared against the original); test `ShadowMath.HandRestPose` |
| The hand's shadow fades as the camera pulls away from the ground below it | done | `shadow_math` fade (`src/Graphics/ShadowMath.cpp`) |

## Ground blobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each villager's foot has a short dark blob stretched over the land away from a low light, fading as it goes | done | `src/Graphics/GroundBlobs.cpp` from the posed feet; test `test/test_ground_blobs.cpp` |
| Things in the sea or very low have no blobs | done | `GroundBlobs.h`; test `test/test_ground_blobs.cpp` |
| Animals have ground blobs | done | the land animals' blobs from their mesh's bone points (`src/Graphics/Renderer.cpp` with `GroundBlobs.cpp`) |
