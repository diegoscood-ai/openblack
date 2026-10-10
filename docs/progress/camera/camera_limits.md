# Camera limits and collision

Where the world's camera is allowed to go: how high and how far out, how close to the land and sea, how far it may tilt,
the zones a land keeps it out of or inside, and how near it draws.

**Progress: 4/15 done, 8 partial — 53%**

## Bounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera keeps within a disc around the island and is pushed back when it strays outside | done | `Camera::UpdateZoomers` pulls a position destination more than 3500 from the centre back to it in 3 s, in every mode but the temple's (`script_camera::k_DiscRadiusSquared`); `ScriptCamera.DiscOfTheWorld` (`test/test_script_camera.cpp`). The player model also clamps to a 5120 disc (`DefaultWorldCameraModel::ConstrainDisc`) |
| The camera never rises above a ceiling height | partial | the 30 000 ceiling (`k_MaxAltitude`) only bounds the arc-ball tilt step in `DefaultWorldCameraModel::ComputeDistanceFromBoundY`; nothing stops the camera rising past it |
| The camera floats at least a little above the land or sea under it | partial | the player model keeps 3 over the land (`ConstrainAltitude`, `k_FloatingHeight`), matching the recorded runs of `test/camera/test_camera.cpp`; the drawn camera's 1 m clearance (`script_camera::DrawnCamera`) only applies while a script drives. Not checked on steep land |
| The camera's tilt is kept between looking slightly up and nearly straight down | done | pitch clamp in `DefaultWorldCameraModel::TiltZoom`; recorded runs `TiltUpDown`, `TiltDownZoomOut` (`test/camera/scenarios`) |
| What the camera looks at is kept on the land rather than drifting off over the sea | partial | the focus is put on the land under the screen centre in `DefaultWorldCameraModel::UpdateModeCartesian`; not checked away from that mode |
| The focus is kept within a set distance of the camera | todo | not in our tree |

## Collision

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Moving the camera stops short of land in its way | partial | `camera_pan::StopShort` (`src/Camera/CameraPan.cpp`; `CameraPan.StopsThreeShortOfLandInItsWay`) is not wired into the camera yet |
| A move that goes down meets the sea and stops there | partial | `CameraPan.TheSeaIsMetGoingDownNearTheCamera` pins the pure rule; not wired into the camera yet |
| Zooming or turning into a hill does not put the camera inside it | partial | `DefaultWorldCameraModel::ConstrainCamera` re-floats the camera after each frame; not compared with the game |

## Zones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land can have exclusion domes the camera cannot enter, and the camera is eased out of one it strays into | partial | `SET_CAMERA_ZONE` reads the zone's exclusions (`player_camera::SetCameraZone`, `src/Camera/PlayerCameraScript.cpp`; tests in `test/test_script_camera_dual.cpp`), but the player camera does not keep out of them yet |
| Scripts can set an inclusion zone that keeps the camera inside an area, as the first land's tutorial does in stages | partial | `SET_CAMERA_ZONE` loads the inclusion polygon and `player_camera::InsideInclusion` tests it, but the player camera does not use it yet; `GET_INCLUSION_DISTANCE` stays at its start value |
| Clearing a land's zones as a new land loads | todo | `player_camera::Reset` has no caller: the zone is not cleared when a land loads |

## Drawing distance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The near plane comes in close to the ground and goes out with height, so land and objects aren't cut away | done | the near plane is 0.3 + 0.16 x the height over the land, 0.3 to 3.5, made again on a change over 0.01 (`src/Game.cpp`, camera update); the same rule as `near_clipping::NearPlane` (`NearClipping.FollowsTheHeightOverTheLand`, `IsTheGamesExpressionToTheBit`) |
| Scripts and camera paths can bring the near plane right in for close shots | todo | `SET_GRAPHICS_CLIPPING` is a stub in `src/CHLApi.cpp`; `near_clipping::k_Close` (`NearClipping.ScriptsCanClipClose`) is unused |
| The world camera's field of view is the game's default lens | done | 70 degrees (`config.cameraXFov`, `script_camera::k_DefaultFov`), as our wiki says the game sets it at start ([script camera](../../bw1-notes/script-camera.md#gcamera-and-its-zoomers)) |
