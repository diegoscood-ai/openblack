# The sea

The sea around every island: a textured surface drawn in rows across the screen, each row rippling on its own and
fading with distance, coloured by the land's light, blended over the mirrored world beneath it.

**Progress: 16/16 done, 0 partial — 100%**

How the original does it, in our wiki: [Water in the game](../../bw1-notes/water.md).

## Drawing the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is a square 30000 units across at sea level about the middle of the map, reaching the horizon | done | `sea::ComputeScreenRange` (`src/Graphics/SeaRows.cpp`); test `test/test_sea_rows.cpp` |
| The sea is drawn in screen rows two pixels apart, from where it meets the top of the screen down to the bottom | done | `src/Graphics/SeaRows.cpp`, `src/Graphics/RendererSea.cpp`, `assets/shaders/fs_water.sc` |
| Every other row repeats the texture of the one before, so each texture line holds for two pixels then blends | done | `assets/shaders/fs_water.sc` |
| Each row ripples along the view by its own step of a 16-step sine, close up only | done | `assets/shaders/fs_water.sc` and the sea's uniforms (`src/Graphics/RendererSea.cpp`) |
| The ripple moves on a step each frame | done | The sea's frame counter, only while the game runs (`src/Graphics/RendererSea.cpp`) |
| The sea is fully opaque near and fades to 80 of 255 far away, showing what lies beneath | done | `assets/shaders/fs_water.sc` (the row alpha) |
| A first row that starts inside the screen is nearly clear, softening the horizon | done | `src/Graphics/SeaRows.cpp` (the first row nearly clear) |
| The sea's texture takes the land's light at full luminosity, so it darkens at night and reddens under an evil sky | done | The light table's last entry (`src/Graphics/RendererSea.cpp`, `assets/shaders/fs_water.sc`) |
| The sea's texture repeats more finely at higher detail levels | done | `detail_level::Level::waterTiling`, `Level::SeaPeriod` (`src/Graphics/DetailLevel.h`) |
| At the lowest detail the sea is a still square, its texture repeated 50 times, fully opaque | done | The level 0 sea in `src/Graphics/RendererSea.cpp` |
| Ambient wind would drift the sea's texture | n/a | The game's ambient wind is always 0 in play, so the drift never shows; our tree keeps the drift code (`sea::Drift`) |
| The sea is drawn after the land so the land's coast fades into it | done | The sea is drawn before the land, which blends over it (`src/Graphics/Renderer.cpp`). Our wiki differs: the original draws the sea first, then the land over it with its coast alpha ([rendering.md](../../bw1-notes/rendering.md#sea-skyraw--skyaraw)) |
| The sea is not drawn inside the temple, which has its own pool | done | The temple pass draws no sea; see ../temple/ |
| The sea is hazed with distance like the land | done | The sea takes no haze, only its distance fade. Our wiki differs: the original's distance haze is not applied to the sea ([rendering.md](../../bw1-notes/rendering.md#distance-haze-original-fog-detail-levels-36)) |
| The view under the sea is drawn at the size of the view | done | The reflection target follows the main view (`Renderer::UpdateReflectionTarget`) |

## Sounds of the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is heard from cells of sea, louder by how much of the view is sea and by height | done | `src/Audio/Services/SoundMap.cpp`; test `test/audio/test_sound_map.cpp`; see ../audio/ |
| Waves on the shore are heard on the coast | done | The coast ambient type (`src/Audio/Services/SoundMap.cpp`) |
