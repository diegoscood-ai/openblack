# Sprites and billboards

Flat textured pictures drawn in the world rather than models: camera-facing sprites, sprites lying in their own plane,
the glows of lights and lanterns, and text laid out in the world. Particle sprites are in [particles.md](particles.md);
the moon and sun in [../sky/](../sky/); the influence ring in [../worship/](../worship/).

**Progress: 10/12 done, 0 chosen, 1 partial, 0 inferred — 83%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## Sprites in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Sprites turn to face the camera | done | `src/ECS/Components/Sprite.h`, `src/3D/Billboard.cpp`; test `test/test_billboard.cpp` |
| Sprites lie in their own plane, as their placement turns them | done | `billboard` horizontal quads (`src/3D/Billboard.cpp`); test `Billboard.horizontalQuadIsRyOfTheAngle` |
| A sprite either adds light to what is behind it, like a glow, or blends over it by its alpha | done | `Sprite.h` render modes; `render_modes::SpriteMode`; test `WorldTrianglesFrame.ASpriteAddsItsLightOrBlendsByItsAlpha` (`test/test_world_triangles_frame.cpp`) |
| A colour texture takes its alpha from its companion alpha file; a texture without one is a single channel that is both alpha and brightness | done | the texture and its alpha file paired at load time (`Texture2DLoader` in `src/Resources/Loaders.cpp`, `argb4444::PackRaw`); tests `Argb4444.PackRaw`, `ColourAlphaTexture.Argb4444IsWhatPackRawGives` |
| Sprites take their place among everything else that blends, farthest first | done | `src/Graphics/ZSort.cpp`; test `test/test_zsorter.cpp` |
| Animated billboards running on their own frame clocks | done | `src/3D/FrameAnim.cpp` (frame-animated textures on their own clocks); test `test/test_frame_anim.cpp` |

## Glows of lights

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A light from the light files glows as two sprites added over each other from the atmosphere texture, the second half as strong and whiter | done | `src/3D/Light.cpp`, `src/ECS/Archetypes/GlowArchetype.cpp`; test `LightEmitter.SizesAndColoursTheGlowByTheLight` |
| Glows of lights set in walls (the temple's windows) lie flat against the wall instead of facing the camera | done | glows with their own axes (`AppendGlowQuad` in `src/Visual/LightBeams.cpp`, the temple's windows); tests `LightEmitter.AlignsGlowsAndGivesSpotLightsCones`, `LightBeams.GlowQuadLiesAlongItsAxesWithTheSpritesCoordinates` |
| Village lanterns and campfires burn with two flames and a glow, the flames playing the fire's frames backwards on one clock and flickering in size | done | `src/3D/NightLights.cpp` (the lantern flames and glow); tests `VillageLights.FlamesPlayBackwards`, `VillageLights.FlamesLoopEverySevenHundredMilliseconds`, `VillageLights.FlickerEveryThirtyMilliseconds` |
| Plain glows the game draws at points in the world | partial | the game has a glow manager drawing glows and white glows at points; its only users (xref callers, 2026-11-07) are the temple's world room, over its map: a glow over the marker of the temple, of the creature and of each challenge not yet done (`GlowManager::DrawGlowAtPoint` 0x83D860, from `WorldRoom::DrawCitadel`, `DrawCreature` and `DrawChallenges`; a challenge's glow is colour (0x7C, 0x6E, 0x61), lifted over its marker, bw1-decomp `WorldRoom.cpp`), and a white glow from the room's own draw (`DrawWhiteGlow` 0x83DFE0, from `WorldRoom::Draw`); nothing in the land view draws them. openblack draws the glow under each marker of the pool's map, the temple's, the creature's and each challenge's not yet done (the challenge's at its own place in the world), 1.3 either side of its point, in that colour, before the markers (`Renderer::DrawTempleMapMarkers`, `TempleMapMarker::glow`; 60bf2801c5, 94f652fdbc, 09fba2aefb; test `TempleMap.GlowsUnderTheMarkersInAWarmGrey`; [temple interior](../../../interface/temple-interior.md#the-map-in-the-pool)); the white glow the room draws as the player walks out of the citadel, the exit door filling with light: raffclar's `temple_exit_glow` (his 772ed2733, `src/Graphics/TempleExitGlow.h`, tests `TempleExitGlow.*`) drawn by `Renderer::DrawTempleExitGlow` while `TempleInteriorInterface::GetWayOutSeconds` answers (2026-11-19; not yet seen in a run). Read 2026-11-19: `WorldRoom::Draw` (bw1-decomp `WorldRoom.cpp:565-601`), while the camera goes through a door to no room (the way out): t = (the state's seconds − 2.2) × 1.5, at least 0, past 2 growing half as fast again, at most 4; grey t × 127 and alpha t × 75 (each at most 255), spread (t − 2) × 7 (at least 0); `DrawWhiteGlow` from (80, 5, 0) to (60, 5, 0) turned about y by the selected door × π/4, heights 20 and t × 10 + 20, the spread, the width t × 15, Z always. `GlowManager::DrawWhiteGlow` 0x83DFE0 (Ghidra's C only): 32 vertices in 8 groups of four across the side line (steps (i − 4) / 4), the start edge 20 high in the colour, the end edge its height centred, alpha 0; uvs sin of (the manager's +0x24 × 0.1, stepped π/4) × 0.1 + 0.625 / 0.375; 34 triangles (102 indices: each group's quad, then three passes joining the groups), `Draw3DWorldTriangle` with `LH3DAtmos::mat1` (atmos.raw, render mode 13, additive, two-sided). The uvs' phase is taken as the room's spot-light cones' drift (the seconds times the main room's cones times 0.1, as openblack's cones drift, `Renderer::DrawTempleLightBeams`; what writes the manager's +0x24 is still not read); the selected door on the way out is the third, door 2 (the world room's tooltip table gives door 6 to the scrolls' wall) |

## Text and symbols in the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Text laid out in the world, letter by letter from the font, along a frame | done | `src/3D/OrientedText.cpp` (the temple's signs and scrolls); see [../temple/](../temple/); tests `OrientedText.EachGlyphIsAQuadDownItsLetters`, `OrientedText.StandsOffThePageAlongTheAxisLeft` |
| A player's symbol drawn into textures in the player's colour | pending | the game draws player symbols into textures (unconfirmed where they show) |
