# Level of detail

The game's graphics detail levels and what they switch on, and how models and objects get simpler or stop being drawn
with distance. The land's own detail by distance is in [../terrain/](../terrain/); distance haze is in
[render_pipeline.md](render_pipeline.md).

**Progress: 6/13 done, 5 partial — 65%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../world/haze-and-mist.md).

## Detail levels

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Seven detail levels, 0 to 6, 4 by default; level 5 is the player's custom one | done | `src/Graphics/DetailLevel.h` (`k_Levels`), `--detail-level` in `src/main.cpp`; test `test/graphics/test_detail_level.cpp` |
| The sea's texture repeats more finely at higher levels, and is a still square at level 0 | done | `DetailLevel::SeaPeriod`, `src/Graphics/RendererSea.cpp`, `src/Graphics/SeaRows.h`; test `test/test_sea_rows.cpp` |
| Clouds are shown from level 3 up | done | `detail_level::Clouds` read by `src/Graphics/Renderer.cpp` and `src/ECS/Weather/StormClouds.cpp` |
| Cloud shadows have their own detail switch | partial 50% | openblack ties cloud shadows to the clouds' switch (`DetailLevel.h`); re-measured 2026-10-21: the shadows follow the clouds' switch, not a switch of their own |
| The fog setting by level | done | `detail_level::Fog` turns the distance haze on from level 3 (`src/Graphics/Haze.cpp`) |
| The weather (rain and falling snow) is on only from level 3 up | partial 50% | the levels carry the weather and rain splash settings (`DetailLevel.h`) but nothing reads them: the rain is drawn at every level (`src/Graphics/RendererRain.cpp`); no falling snow is drawn; re-measured 2026-10-21: the atmosphere starts with the level's weather switch (Locator.cpp, weather::StartAtmos); the rain draw and the falling snow are not gated by it (see #193) |
| The temple's outside has its own detail level | pending |  |
| The player picks custom detail settings in the options | partial 60% | the options box's five entries set the levels 0 to 3 and, its top entry Maximum Detail, 6 (🎯 user 2026-10-20), as the Options page is left; leaving it on Maximum always sets 6, also from the default 4 that already shows it (🎯 user 2026-10-21) (`src/Gui/GameMenu.cpp`, `detail_level::LevelOfMenuEntry`); `--detail-level` sets it at start; no custom per-setting choices; see [../interface/](../interface/); re-measured 2026-10-21: the entries 0 to 3 and Maximum set their levels; the custom level's own settings cannot be picked |
| The detail settings are remembered between games | pending | no settings file; see [../engine/](../engine/) |
| Low resolution textures for slower machines | partial 20% | the level's high texture flag exists (`DetailLevel.h`) but only sets the static shadows' strength (`Renderer.cpp`); the landscape textures are always full size; re-measured 2026-10-21: the flag only sets the static shadows' strength; the textures stay full size |

## Models by distance and level

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Objects, villagers and animals use simpler meshes at lower detail levels | chosen | 🎯 chosen difference (the user, F99, 2026-10-15): the villagers', animals' and birds' high meshes (`components::DetailMesh`) are drawn only at detail level 6, the top setting (`detail_level::DrawsHighMeshes`, `src/Graphics/DetailLevel.h`, chosen by `ecs::drawn_mesh::Of` for both the draw layout and the rows, `src/ECS/DrawnMesh.h`; a SuperVillager and a skeleton put what they show in both meshes); other objects always draw their first level. The v1.42 exe draws the high meshes at every level, with its level of detail loading disabled ([rendering-objects](../../../rendering/meshes-and-lod.md#villager-blobs-object-reflections-and-lod)) |
| A model's parts are picked by its level of detail mask | done | the parts of the first level are drawn, as the original does with its level of detail loading disabled (`Renderer.cpp` submesh drawing) |
| How far the camera can see before the world is cut off | done | far plane `EngineConfig::cameraFarClip`, the near plane from the camera's height as the original (`src/Camera/NearClipping.h`); the original has no far plane, so the picture is the same ([parity](../../../rendering/frame.md)) |
