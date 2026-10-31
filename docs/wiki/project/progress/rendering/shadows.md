# Shadows

The shadows the game draws: the fixed shadows of trees, rocks and buildings on the land, the creature's and the hand's
moving shadows, and the villagers' small ground blobs. Cloud shadows are in [../sky/](../sky/); particle effects that
darken the land are in [particles.md](particles.md).

**Progress: 13/18 done, 0 chosen, 3 partial, 0 inferred — 78%**

How the original does it, in our wiki: [Model rendering: original versus openblack](../../../rendering/shadows-and-reflections.md), [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/shadows-and-reflections.md).

## Shadows on the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Trees, rocks, buildings and features cast shadows onto the land from a fixed far sun, as long as they are high | done | baked into each block's texture at its prints step as the game does (`block_texture::DrawShadowTriangles` and `DrawTextureShadow`, the casters from `ecs::land_prints::SourcesFor`; tests `BlockShadows.*`, the original's masks byte for byte; #222) |
| Where shadows overlap the land gets no darker; a fully covered spot is half as bright | done | 8 samples a texel, each set once however many shadows cover it; the texel darkened by 255 x the share covered / 8, halved: x 128 / 256 fully covered (`block_texture::ApplyShadow`, `ShadowTable` read back from the original); tests `BlockPrints.AShadowDarkensTheColourByItsCoverage`, `BlockPrints.TheShadowTableAsTheOriginalBuildsIt`, `BlockShadows.ASampleIsItsTexelsBitByRowAndColumn` |
| Shadows follow objects as they are built, felled or moved | done | the blocks under an object are painted again with their shadows when it is put on the map's cells or taken off them, or a building is finished (`ecs::land_print_marks::MarkBlocksUnder`, `block_texture::BlocksMarkedBy`); tests `BlockTextureMarkedBlocks.AHouseMarksTheBlocksItsShadowAndFootprintReach`, `BlockTextureMarks.AChangeBeforeTheFirstDrawIsPaintedAtOnceAndOneAfterItWaitsForTheDraw` |
| At night homes, the village centre, storehouse, trees and totems cast shadows from the lights | pending | the game marks which objects cast shadows at night (unconfirmed how they are drawn) |
| Shadows projected over other objects, not only the land | done | the projected shadow list (`src/Graphics/ShadowList.cpp`, `RendererShadows.cpp`): the hand, the creature and the boat also onto the objects behind them; tests `ShadowMath.*`, `ShadowReceiver.*` |
| The land's own baked shadow map | pending | our baked static shadows cover the objects' shadows only; what this baked map is is not in our wiki |
| Short-lived shadows the game keeps for a while | partial | a SuperVillager's temporary shadow: a projected shadow lit by the fixed sun, on the land only, which turns its ground blobs off while it lasts (`components::DynamicShadow` made in `src/ECS/SuperVillager.cpp`, drawn by `src/Graphics/ShadowList.cpp`); it is cast with the villager's own matrix and clip, not the turned and cross-faded copy its body is drawn with, as the original (tests `SuperVillagerSmooth.ItsShadowTakesTheObjectsMatrixNotTheTurnedCopy`, `OtherCastersKeepTheirDrawnMatrix`); the original keeps its projected shadows in one list in the order they were made, whatever their kind, each new one at the head (a SuperVillager's at its script op); openblack's list keeps that order too, and makes the SuperVillagers' shadows from their own list, oldest first, so that of several made together the newest is at the head, as the original (`shadow_list::TemporaryShadowsInMakingOrder`; tests `ShadowListOrder.TheTemporaryShadowsAreMadeOldestFirstSoTheNewestIsAtTheHead`, `OnlyAnAvailableVillagerWhoseShadowLastsHasOne`). Left: openblack makes a new shadow at the next drawn frame, not at the moment its caster gets it, so shadows of different kinds made in the same frame are listed by kind (the hand, the creatures, the flying objects, the SuperVillagers, the boat, the particles), which can change only the rounding where they overlap |

## Creature shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's silhouette, seen from the light, is drawn into a soft small texture and projected onto the land and what stands on it | done | `shadow_list` complex caster with the creature's light (`src/Graphics/ShadowList.cpp`, `ShadowMath.cpp`), onto the land and what stands on it; tests `ShadowMath.RasterFaces`, `ShadowMath.ReachesMorphable`, `ShadowMath.Land` |
| The light is kept at least 45 degrees up and no closer than three of the creature's radii | done | `shadow_math::LightCreature`; test `ShadowMath.Lights` |
| The shadow fades out as the camera pulls away, between 50 and 80 creature radii | done | the fade by distance in `src/Graphics/ShadowMath.cpp`; test `ShadowMath.FadeByDistance` |
| The creature's hair casts no shadow | partial 50% | the creature's body is the caster (`ShadowList.cpp`); whether its hair is left out is unchecked |
| Creature shadows don't fall on creatures themselves | done | creatures are not receivers (`src/ECS/ShadowReceiver.h`); test `ShadowReceiver.ACreatureDoesNotReceive` |
| Several creatures' shadows at once, the nearest first | partial 60% | every drawn creature gets a shadow, with no cap (`ShadowList.cpp`); the game's limit is unconfirmed; re-measured 2026-10-21: every drawn creature casts, nearest first; the original's cap is not confirmed |

## Hand shadow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand's silhouette is projected onto the land, darkening it | done | the hand's 32x32 silhouette with its vertical light, as the original's (`shadow_list`, compared against the original); test `ShadowMath.HandRestPose` |
| The hand's shadow fades as the camera pulls away from the ground below it | done | `shadow_math` fade (`src/Graphics/ShadowMath.cpp`); test `ShadowMath.FadeByDistance` |

## Ground blobs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each villager's foot has a short dark blob stretched over the land away from a low light, fading as it goes | done | `src/Graphics/GroundBlobs.cpp` from the posed feet; test `test/test_ground_blobs.cpp` |
| Things in the sea or very low have no blobs | done | `ground_blobs::k_LowestHeight` (`src/Graphics/GroundBlobs.h`), compared for the villagers and the animals in the blobs' collection (`src/Graphics/Renderer.cpp`); `test/test_ground_blobs.cpp` covers the blobs, not this cut-off, which is one height compare in the renderer |
| Animals have ground blobs | done | the land animals' blobs from their mesh's bone points (`src/Graphics/Renderer.cpp` with `GroundBlobs.cpp`); the blob's shape is the villagers' (`test/test_ground_blobs.cpp`); no test of the animals' bone points, which the renderer reads from the frame's skinned bones |
