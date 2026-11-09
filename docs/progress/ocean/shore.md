# The shore

Where land meets the sea: the land's texture fades out towards the water, the lowest land lies flat at sea level, and
shallows can be waded.

**Progress: 8/8 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Sea, coast and rivers](../../../world/sea-coast-and-rivers.md#coast) and [Water](../../../world/water.md).

## Coast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's texture fades to clear from altitude 4 down to altitude 1, so the sea shows through at the coast | done | `src/3D/CoastAlpha.cpp` and the block texture's alpha (`src/3D/BlockTexture.cpp`); tests `test/test_block_texture.cpp`, `test/test_block_texture_fast.cpp` |
| The fade is ragged by the land's noise, so the waterline wanders | done | `src/3D/CoastAlpha.cpp` (identical texel by texel on Land 1); test `test/test_block_texture_fast.cpp` |
| Land at sea level next to the sea is flattened to height 0 | done | `LandIslandInterface::GetHeightAt` and `GetDrawnHeightAt`; test `test/test_flat_land.cpp` |
| Cells of open sea are not drawn as land | done | `src/3D/LandBlock.cpp` (their triangles collapsed), `src/3D/BlockTexture.cpp`; check: land view shots |
| Coast cells are flagged so fire, forests and spells treat them as wet | done | The cell bits (`src/ECS/SeaCells.cpp`) read by `src/ECS/Fire/FireEffect.cpp` and the water queries; tests `SeaCells.*`, `test/test_water_queries.cpp` |

## Wading and splashing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature wades in shallow water and keeps out of the deep | done | The `LandAvoid` mask (`src/3D/LandAvoid.cpp`) used by `CreatureLocomotionSystem.cpp`; see ../creature/ and [water.md](../../../world/water.md#the-creatures-landavoid-mask) |
| A creature's footsteps sound wet in water | done | `CreatureAudioSystem.cpp` (the surface from `sea_cells::GetSurfaceType`); see ../creature/ |
| The hand gripping the land at the water makes a splash ring | done | The grip in water (`HandPlacement.cpp`) and its ring (`src/ECS/Systems/Implementations/HandFish.cpp`) |
