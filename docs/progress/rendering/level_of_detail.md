# Level of detail

The game's graphics detail levels and what they switch on, and how models and objects get simpler or stop being drawn
with distance. The land's own detail by distance is in [../terrain/](../terrain/); distance haze is in
[render_pipeline.md](render_pipeline.md).

**Progress: 6/12 done, 4 partial — 67%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../bw1-notes/parity.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Detail levels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Seven detail levels, 0 to 6, 4 by default; level 5 is the player's custom one | done | `src/Graphics/DetailLevel.h` (`k_Levels`), `--detail-level` in `src/main.cpp`; test `test/graphics/test_detail_level.cpp` |
| The sea's texture repeats more finely at higher levels, and is a still square at level 0 | done | `DetailLevel::SeaPeriod`, `src/Graphics/RendererSea.cpp`, `src/Graphics/SeaRows.h`; test `test/test_sea_rows.cpp` |
| Clouds are shown from level 3 up | done | `detail_level::Clouds` read by `src/Graphics/Renderer.cpp` and `src/ECS/Weather/StormClouds.cpp` |
| Cloud shadows have their own detail switch | partial | openblack ties cloud shadows to the clouds' switch (`DetailLevel.h`) |
| The fog setting by level | done | `detail_level::Fog` turns the distance haze on from level 3 (`src/Graphics/Haze.cpp`) |
| The weather (rain and falling snow) is on only from level 3 up | partial | the levels carry the weather and rain splash settings (`DetailLevel.h`) but nothing reads them: the rain is drawn at every level (`src/Graphics/RendererRain.cpp`); no falling snow is drawn |
| The temple's outside has its own detail level | todo |  |
| The player picks custom detail settings in the options | partial | the options box steps through the levels (`src/Gui/GameMenu.cpp`) but changes nothing yet; `--detail-level` sets it at start; no custom per-setting choices; see [../interface/](../interface/) |
| The detail settings are remembered between games | todo | no settings file; see [../engine/](../engine/) |
| Low resolution textures for slower machines | partial | the level's high texture flag exists (`DetailLevel.h`) but only sets the static shadows' strength (`Renderer.cpp`); the landscape textures are always full size |

## Models by distance and level

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects, villagers and animals use simpler meshes at lower detail levels | n/a | openblack always draws the most detailed meshes. Our wiki differs: in this version of the original the level of detail loading is disabled, so models always draw their first level ([rendering-objects](../../bw1-notes/rendering-objects.md#villager-blobs-object-reflections-and-lod)) |
| A model's parts are picked by its level of detail mask | done | the parts of the first level are drawn, as the original does with its level of detail loading disabled (`Renderer.cpp` submesh drawing) |
| Objects beyond a distance stop being drawn | n/a | openblack draws everything. Our wiki differs: the original never fades or hides models by distance and has no far plane ([parity](../../bw1-notes/parity.md)) |
| Objects off screen are skipped | n/a | openblack leaves this to the GPU; it only affects speed |
| How far the camera can see before the world is cut off | done | far plane `EngineConfig::cameraFarClip`, the near plane from the camera's height as the original (`src/Camera/NearClipping.h`); the original has no far plane, so the picture is the same ([parity](../../bw1-notes/parity.md)) |
