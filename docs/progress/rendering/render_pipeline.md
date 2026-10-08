# Render pipeline

How a frame is put together: the passes drawn before the scene, the sky first, the opaque world, then everything that
blends sorted farthest first, the near clipping plane and the distance haze. The sea's reflection pass is in
[../ocean/](../ocean/), the temple's rooms in [../temple/](../temple/).

**Progress: 13/16 done, 2 partial — 88%**

How the original does it, in our wiki: [The original B&W frame (runblack.exe W120, D3D7, LH3D)](../../bw1-notes/original-frame.md), [Graphics engine parity: original versus openblack](../../bw1-notes/parity.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Frame structure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Before the scene: the land's luminosity and cell colours, the sky dome's bands, the rivers' alpha, the objects' shadows and the creatures' and hand's silhouettes | done | `src/Graphics/RenderPass.h`, the per-frame preparation in `src/Graphics/Renderer.cpp` (land light, sky, static shadows, the projected shadow list of `src/Graphics/ShadowList.cpp`) |
| The sky is drawn first and everything else over it | done | `RenderPass.h`, `Renderer::DrawScene` |
| The frame's updaters run in the game's order, getting everything ready before drawing | partial | the renderer prepares each frame in the original's stage order (`Renderer::DrawScene`, see our wiki's original frame); not every updater of the original's frame is ported |
| Films overlaid on the picture, and the order the frame is finished in | done | `Renderer::DrawFinishFrameOverlays`: the bars, the film (`DrawVideoOverlay`), then the script fade, as the original finishes its frame |
| The game's pointer is drawn over everything | done | `RenderPass.h` |

## Transparency and ordering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Everything that blends is drawn after the rest of the scene, farthest from the camera first, whatever it is | done | `src/Graphics/ZSort.cpp` (`graphics::zsort`, stable, capped at 2048); test `test/test_zsorter.cpp` |
| The sort uses the distance squared, in single precision, as the game does | done | `ZSort.cpp`; test `test/test_zsorter.cpp` |
| Models that fade or have alpha, sprites, mists, clouds and the hand all go through the one sort | done | `ZSort.cpp`: models with their mesh's sorted flag, sprites, mists, clouds and the hand in the one queue |
| Chimney smoke, particles and rain each take their place in the sort | done | `src/Graphics/RendererSmoke.cpp`, the particle draw paths, `src/Graphics/RendererRain.cpp`, all through `ZSort.cpp` |
| The see-through hand is drawn from both sides, the inside first | done | the hand is drawn whole in the queue with its materials' own culling (`Renderer.cpp`), as the original |
| Footprints are blended onto the land before the rest of what blends | done | the footprint pass before the blended queue (`Renderer.cpp`) |

## Clipping and haze

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The near clipping plane moves out with the camera's height over the land | done | 0.3 + 0.16 x height, clamped to 0.3..3.5, in `src/Game.cpp`; test `NearClipping.FollowsTheHeightOverTheLand` |
| Scripts can set the clipping close | partial | the close plane is in `near_clipping::NearPlane` (`src/Camera/NearClipping.h`; test `NearClipping.ScriptsCanClipClose`), but nothing in the game uses that file yet |
| Far land and models fade into the distance haze | done | `src/Graphics/Haze.cpp`, `assets/shaders/haze.sh` (land per vertex, models once per object, with the storm and lightning haze); test `test/test_haze_land_light.cpp` |
| Lines and triangles game code draws straight into the world | done | `src/Graphics/WorldTriangles.cpp` (`world_triangles::Submit`), used by the leashes, the influence border, exploded pieces and the creature |
| Objects drawn slightly away from where they are, the gap closing over time | todo | the game keeps decaying draw offsets for objects (unconfirmed which) |
