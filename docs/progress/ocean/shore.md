# The shore

Where land meets the sea: the land's texture fades out towards the water, the lowest land lies flat at sea level, and
shallows can be waded.

**Progress: 9/10 done, 0 partial — 90%**

How the original does it, in our wiki: [Water in the game](../../bw1-notes/water.md).

## Coast

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's texture fades to clear from altitude 4 down to altitude 1, so the sea shows through at the coast | done | `src/3D/CoastAlpha.cpp` and the block texture's alpha (`src/3D/BlockTexture.cpp`); test `test/test_block_texture.cpp` |
| The fade is ragged by the land's noise, so the waterline wanders | done | `src/3D/CoastAlpha.cpp` |
| Land at sea level next to the sea is flattened to height 0 | done | `LandIslandInterface::GetHeightAt` and `GetDrawnHeightAt` |
| Cells of open sea are not drawn as land | done | `src/3D/LandBlock.cpp` (their triangles collapsed), `src/3D/BlockTexture.cpp` |
| Coast cells are flagged so fire, forests and spells treat them as wet | done | The cell bits (`src/ECS/SeaCells.cpp`) read by `src/ECS/Fire/FireEffect.cpp` and the water queries |
| There are no breaking waves or foam lines on the shore | done | None in our tree, as the game |

## Wading and splashing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature wades in shallow water and keeps out of the deep | done | `CreatureLocomotionSystem.cpp`; see ../creature/ |
| Splash sprites rise round a creature's feet as it walks through water | todo | Not ported |
| A creature's footsteps sound wet in water | done | `CreatureAudioSystem.cpp` (the water surface) |
| The hand gripping the land at the water makes a splash ring | done | The grip in water (`HandPlacement.cpp`) and its ring (`src/ECS/Systems/Implementations/HandFish.cpp`) |
