# Land height and mountains

The shape of the land: a height for every cell corner, interpolated over each cell's two triangles, with the sea level
flattened, and the queries everything uses to stand on the ground.

**Progress: 12/12 done, 0 chosen, 0 partial, 0 inferred — 100%**

How the original does it, in our wiki: [Coordinates, terrain, object size, game clock, matrices and Zoomer](../../../world/terrain.md).

## Height

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every cell corner has a height of 0 to 255 units | done | `components/lnd` cell altitude; test `LandWorld.TheLandStandsAtItsCornersAndOnTheCellsSplitDiagonal` |
| Heights between corners follow the cell's two triangles, split one way or the other per cell | done | `LandIsland::GetHeightAt` (`src/3D/Implementations/LandIsland.cpp`); test `LandWorld.TheLandStandsAtItsCornersAndOnTheCellsSplitDiagonal` (`test/test_land_world.cpp`, on synthetic and the game's data) |
| Height is worked out in the game's fixed-point units, so objects stand exactly where the game puts them | done | `LandIsland::GetHeightAt`, `src/3D/MapCoords.h` |
| Land at or below altitude 3 next to the sea is drawn and treated as flat at sea level | done | The drawn mesh puts every corner of altitude 3 or less at 0 (`GetDrawnHeightAt`); the height queries flatten it only next to a corner of 4 or less (`GetHeightAt`), as `LH3DIsland::GetAltitude` 0x803090 ([meshes-and-lod.md](../../../rendering/meshes-and-lod.md#meshes-stuck-to-the-ground-land_morph)); checked by the fidelity runs; test `LandWorld.NextToTheSeaLowLandStandsAtSeaLevel` (`test/test_land_world.cpp`, on synthetic and the game's data) |
| The land's normal is the flat normal of the triangle a point is on, worked out through the game's lookup tables | done | `src/3D/LandNormal.cpp`; test `test/test_land_normal.cpp` |
| Mountains and hills are just high land: there are no separate cliff or rock-face meshes in the land itself | done | Rock features are objects, see ../nature/ |
| The highest point of each block is known for drawing and the creature's view of the land | done | Worked out from the block's cells as the land opens, the file's value not kept, and never after a land change, as `fn_007FEA60` does (the largest altitude byte of the 289 cells, from the loader `fn_008015D0` and `LH3DIsland::Create`): `LandBlock::HighestAltitude` from `LandIsland::Build`; test `test/test_land_block_highest.cpp`. The five story lands store the same value in every block (668 blocks checked). Its one reader in the 3D engine is the block's box in `fn_00877210` 0x87722C (the cull, the distance, so the visible order and the draw order), ported in the object draw list (`draw_list::NextBlockState`, `src/ECS/DrawList/BlockCull.cpp`; tests `test/test_draw_list_blocks.cpp`) and the haze class (`haze::BlockCorners`); no creature code reads it (the creature's map of the land keeps its own highest cell per region, see countries.md); the block texture queue does not either ([terrain.md](../../../world/terrain.md#the-blocks-highest-point)). The land's own GPU draw is not culled by the block boxes yet: the far-block row of land_loading.md |
| Scripts ask the height of the land at a point | done | GET_LAND_HEIGHT in `src/CHLApi.cpp` (`sea_cells::ScriptLandHeight`); test `SeaCells.PredicatesOnHandMadeIsland` |
| Things dropped or thrown land on the ground and objects stand on it | done | The physics tests every point against the land height and normal (`Body::TouchGround`); see ../physics/; tests `PhysicsBodyPose.SettleOnLand`, `PhysicsObjects.ItsHeightIsKeptAboveTheLandWhereItStands` |
| A point on the land is found under the mouse by the game's line test over the land's cells | done | The island ray from a pixel, `PickingSystemInterface::LandFrom2DPoint` over the land line (`src/3D/LandLine.cpp`), for the hand and the camera; the interface's own land, `LandUnderPixel`; tests `LandFrom2DPoint.*` (`test/test_land_from_2d_point.cpp`), `LandRayCast.*` |
| Particles and spells can be kept at a fixed height above the land or forced onto it | done | Meshes raised above, melted into or clamped to the land (`src/3D/LandMorph.cpp`; tests `LandMorph.*`), the floor of the fireball's gravity and `EventConditionAtomBelowHeight` (`src/Particles/ParticleEffect.cpp`), and `AR_FadeAlphaWithHeightAboveLandscape` (0x6A4DE0, its defaults from 0x6BC010) as `fireball_rules::AlphaWithHeight` (`src/Particles/Rules/Fireball.cpp`; test `FireballRules.AlphaWithHeightAboveTheLand`) |
| The land is flattened where a land vortex opens and where a temple is built; villagers, animals, creatures and wood and food piles follow the new ground, everything else stays where it was | done | `src/ECS/Vortex.cpp`, `src/ECS/Archetypes/CitadelArchetype.cpp`, `src/ECS/LandReseat.cpp`; tests `LandReseat.*`, `LandBlockReach.PartialRebuildMatchesTheFullOne` ([vortex.md](../../../world/vortex.md#after-a-land-change)). The land script's COUNTRY_CHANGE and HEIGHT_CHANGE do nothing in the exe (the feature script's jump table at 0x717EBC sends both to its default) |

