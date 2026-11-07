# The sea

The sea around every island: a textured surface drawn in rows across the screen, each row rippling on its own and
fading with distance, coloured by the land's light, blended over the mirrored world beneath it.

**Progress: 18/18 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Sea, coast and rivers](../../../world/sea-coast-and-rivers.md#sea).

## Drawing the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is a square 30000 units across at sea level about the middle of the map, reaching the horizon | done | `sea::ComputeScreenRange` (`src/Graphics/SeaRows.cpp`); test `SeaRows.MatchesTheEmulatedOriginal` (`test/test_sea_rows.cpp`) |
| The sea is drawn in screen rows two pixels apart, from where it meets the top of the screen down to the bottom | done | `src/Graphics/SeaRows.cpp`, `src/Graphics/RendererSea.cpp`, `assets/shaders/fs_water.sc`; tests `SeaRows.*`; check: land view shots |
| Every other row repeats the texture of the one before, so each texture line holds for two pixels then blends | done | `assets/shaders/fs_water.sc`; check: land view shots |
| Each row ripples along the view by its own step of a 16-step sine, close up only | done | `assets/shaders/fs_water.sc` and the sea's uniforms (`src/Graphics/RendererSea.cpp`); check: land view shots |
| The ripple moves on a step each frame, only while the game runs | done | The sea's frame counter (`src/Graphics/RendererSea.cpp`); check: land view shots |
| The sea is fully opaque near and fades to 80 of 255 far away, showing what lies beneath | done | The row alpha, 255 to depth 7000, 80 from 14000 (`assets/shaders/fs_water.sc`); check: land view shots |
| A first row that starts inside the screen is nearly clear, softening the horizon | done | Row 0 at alpha 0x20 (`assets/shaders/fs_water.sc`, `src/Graphics/SeaRows.cpp`); check: land view shots |
| The sea's texture takes the land's light at full luminosity, so it darkens at night and reddens under an evil sky | done | The light table's last entry (`src/Graphics/RendererSea.cpp`, `assets/shaders/fs_water.sc`); check: night land shots |
| The sea's texture repeats more finely at higher detail levels | done | `detail_level::Level::waterTiling`, `Level::SeaPeriod` (`src/Graphics/DetailLevel.h`); test `DetailLevel.SeaTilingAndPastTheLastLevel` |
| At the lowest detail the sea is a still square, its texture repeated 50 times, fully opaque | done | The level 0 quad of 70000 in `src/Graphics/RendererSea.cpp` and `fs_water.sc`, its drift `sea::Drift::ScrollLowDetailSea`; test `SeaRows.AtTheLowestDetailTheSeaIsAStillSquareOfFiftyRepeats` (offsets from the original, oracle) |
| The sea's texture drifts with the ambient wind, which is still in a game | done | `sea::Drift` (`src/Graphics/SeaRows.cpp`); tests `SeaRows.DriftWrapsAtThePeriod`, `SeaRows.TheRowsDriftStepIsTheGamesOwn` |
| The sea's texture is kept at 4 bits a channel, as the game stores it | done | The ARGB4444 cut of `sky.raw` / `skya.raw` (`src/Resources/Loaders.cpp`); test `test/test_argb4444.cpp` |
| The sea is drawn after the mirrored world and before the land, which blends over it with its coast alpha | done | `src/Graphics/Renderer.cpp` ([sea-coast-and-rivers.md](../../../world/sea-coast-and-rivers.md#sea)); check: land view shots |
| The sea takes no distance haze, only its own distance fade | done | `assets/shaders/fs_water.sc` ([haze-and-mist.md](../../../world/haze-and-mist.md#distance-haze-original-fog-detail-levels-36)); check: land view shots |
| The sea is not drawn inside the temple, which has its own pool | done | The temple pass draws no sea (`src/Graphics/Renderer.cpp`); see ../temple/ |
| The view under the sea is drawn at the size of the view | done | The reflection target follows the main view (`Renderer::UpdateReflectionTarget`) |

## Sounds of the sea

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sea is heard from cells of sea, louder by how much of the view is sea and by height | done | `src/Audio/SoundMap.cpp`; test `test/audio/test_sound_map.cpp`; see ../audio/ |
| Waves on the shore are heard on the coast | done | The coast ambient type (`src/Audio/SoundMap.cpp`); test `test/audio/test_sound_map.cpp` |
