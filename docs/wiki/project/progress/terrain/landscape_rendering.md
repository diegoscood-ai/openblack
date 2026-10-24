# Landscape rendering

How the land is drawn: each block painted from its countries' materials, lifted by noise and a bump map, lit by a
per-cell luminosity through a light table that follows the time of day and the alignment, faded into the sea at the
coast, and hazed with distance.

**Progress: 25/26 done, 0 chosen, 0 partial, 1 inferred — 96%**

How the original does it, in our wiki: [World rendering: original versus openblack](../../../rendering/frame.md).

## Painting the blocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each block is painted 16 by 16 texels a cell, in 4 bits a channel | done | `src/3D/BlockTexture.cpp`; test `test/test_block_texture.cpp` |
| Each texel takes its country's material for the height there | done | `src/3D/BlockTexture.cpp` |
| The land's noise map shifts the height a texel's material is picked by, so material edges are ragged | done | `src/3D/BlockTexture.cpp` |
| The bump map shades each texel | done | `src/3D/BlockTexture.cpp` |
| Where a cell's corners lie in different countries, each corner paints the texel and the four are blended by distance | done | `src/3D/BlockTexture.cpp`; test `test/test_block_texture.cpp` |
| The texture fades to clear towards the coast, below altitude 4, so the sea drawn underneath shows through | done | `src/3D/CoastAlpha.cpp`, `src/3D/BlockTexture.cpp`, `assets/shaders/fs_terrain.sc` |
| Open-sea cells are not drawn at all | done | `src/3D/BlockTexture.cpp` |
| A small bump texture is drawn over the land near the camera for fine detail | done | `LandIsland::CreateSmallBumpTexture`, the per-vertex fade in `vs_terrain` and the blend in `assets/shaders/fs_terrain.sc` ([rendering.md](../../../world/terrain.md#terrain-detail-small-bump)) |

## Light and colour

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each cell corner has a luminosity that lights the land through a 256-level light table | done | `src/3D/LandLightTable.cpp`; test `test/test_land_light.cpp` |
| The light table is rebuilt every frame from the weather palette for the time of day and the alignment the sky shows | done | `LandLightTable::Build` from `src/Graphics/Renderer.cpp`, `src/3D/LandLightFrame.cpp` |
| The darkest levels ramp through a warm colour, and the brightest come out brighter than the palette | done | `src/3D/LandLightTable.cpp` |
| Each cell's own colour is added to the land as a light, the haze applied | done | The land's specular (the cell colour plus the haze) in `src/Graphics/Haze.cpp` and `assets/shaders/haze.sh` |
| Night lights of villages and the hand are stamped onto the cells | done | `night_lights::Update` (`src/3D/NightLights.cpp`), run by `VillageLightSystem::Update` (`Locator::villageLightSystem`) after the frame's land light; see ../rendering/ and ../town/ |
| Cloud shadows darken the cells as clouds pass | done | `Clouds::StampShadows`, `src/3D/LandColourStamps.cpp`; see ../sky/clouds.md |
| Lightning stamps a glow on the land where it strikes | done | `src/3D/LandColourStamps.cpp`, `src/ECS/Weather/LightningFlash.cpp`; test `test/test_land_colour_stamps.cpp` |
| Static shadows of buildings and trees are baked into the land | done | Baked into each block's texture at its prints step, as the game (`block_texture::BakePrints`, `LandIsland::BakeBlock`; #222; land_marks.md) ([rendering.md](../../../rendering/shadows-and-reflections.md#shadows-three-systems-in-the-original)) |
| Overcast darkens the land and draws the haze in | done | `LandLightTable::Build` (`src/3D/LandLightTable.cpp`) |
| Normals of the land are worked out per cell triangle for lighting the things on it | done | `src/3D/LandNormal.cpp`; test `test/test_land_normal.cpp` |

## Distance and detail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Distant land fades into a haze a third of the land's colour, closing in at dusk | done | `src/Graphics/Haze.cpp`, `assets/shaders/haze.sh`; test `test/test_haze_land_light.cpp` |
| Each block picks its own haze class | done | `haze::BlockClassOf` (`src/Graphics/Haze.cpp`) |
| The fog detail option turns the haze and overcast tinting on or off | done | The Fog key switches the haze (`haze::Params::on`) and the sky's overcast tint towards the haze colour (`sky_dome::TintOf`, `src/3D/SkyDome.cpp`) |
| Land at sea level is drawn flat at height 0 | done | `LandIslandInterface::GetDrawnHeightAt` |
| The land under the sea is drawn mirrored, half lit and without bump detail, for the reflection | done | The reflection pass (`src/Graphics/Renderer.cpp`); see ../ocean/reflections.md |
| Snow lying on the land whitens it, raggedly by a noise image | done | Baked into each block's texture and low-resolution tile at its prints step, as the original does (`block_texture::PrintSnow` in `src/3D/BlockPrints.cpp`, fed by `land_prints::SourcesFor`); the blocks the melt lists are painted again (`SnowSystem::TakeMarkedBlocks`); tests `BlockPrints.SnowWhitensAsTheOriginal`, `SnowCover.TheMeltListsTheBlocksItsSnowChangedAsTheOriginal`, pinned to the original run natively (#222) ([terrain.md](../../../world/terrain.md#the-block-texture-queue-in-openblack)); see ../weather/snow.md |
| Rivers clear the land's alpha so the water shows | done | The channels `src/ECS/Rivers.cpp` lays lower the block's alpha at its prints step (`block_texture::PrintFootprint`'s alpha mode; #222); see rivers.md |
| Building footprints are blended into the land's colour | done | Baked into the block's texture at its prints step (`block_texture::PrintFootprint`; #222); see land_marks.md |
