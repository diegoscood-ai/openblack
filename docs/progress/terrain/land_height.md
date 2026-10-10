# Land height and mountains

The shape of the land: a height for every cell corner, interpolated over each cell's two triangles, with the sea level
flattened, and the queries everything uses to stand on the ground.

**Progress: 9/12 done, 3 partial — 88%**

How the original does it, in our wiki: [Coordinates, terrain, object size, game clock, matrices and Zoomer](../../bw1-notes/engine-math.md).

## Height

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every cell corner has a height of 0 to 255 units | done | `components/lnd` cell altitude |
| Heights between corners follow the cell's two triangles, split one way or the other per cell | done | `LandIsland::GetHeightAt` (`src/3D/Implementations/LandIsland.cpp`) |
| Height is worked out in the game's fixed-point units, so objects stand exactly where the game puts them | done | `LandIsland::GetHeightAt`, `src/3D/MapCoords.h` |
| Land at or below altitude 3 next to the sea is drawn and treated as flat at sea level | done | `LandIslandInterface::GetHeightAt` and `GetDrawnHeightAt` (the sea flattening) |
| The land's normal is the flat normal of the triangle a point is on, worked out through the game's lookup tables | done | `src/3D/LandNormal.cpp`; test `test/test_land_normal.cpp` |
| Mountains and hills are just high land: there are no separate cliff or rock-face meshes in the land itself | done | Rock features are objects, see ../nature/ |
| The highest point of each block is known for drawing and the creature's view of the land | partial | The field is read from the file but not used |
| Scripts ask the height of the land at a point | done | GET_LAND_HEIGHT in `src/CHLApi.cpp` |
| Things dropped or thrown land on the ground and objects stand on it | done | The physics tests every point against the land height and normal (`PhysicsBody::GroundAndWater`); see ../physics/ |
| A point on the land is found under the mouse by the game's line test over the land's cells | partial | The game's land line test is ported (`LandIslandInterface::RayCast`; tests `LandRayCast.*`, `test/test_land_raycast.cpp`) and used by the lightning and the hand, but the mouse is picked with a Bullet ray (`DynamicsSystem::RayCastClosestHit` from `src/Camera/Camera.cpp`) |
| Particles and spells can be kept at a fixed height above the land or forced onto it | partial | Particle rules in `src/Particles` follow the land; see ../rendering/ |
| The land never changes shape in play; buildings sit on it as they are | done | As the original; the land script's height change is in countries.md |

## The test land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| openblack's flat test land, with a lake, shallows, a bank and patches of sand and snow | n/a | openblack-only, and our tree has no flat test land; see ../debug/ |
