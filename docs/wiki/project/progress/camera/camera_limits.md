# Camera limits and collision

Where the world's camera is allowed to go: how high and how far out, how close to the land and sea, how far it may tilt,
the zones a land keeps it out of or inside, and how near it draws.

**Progress: 6/17 done, 0 chosen, 9 partial, 0 inferred — 64%**

How the original does it, in our wiki: [what the player camera does with the zones](../../../camera/player-camera.md#what-the-player-camera-does-with-the-zones)
and [GCamera and its zoomers](../../../camera/game-camera.md#gcamera-and-its-zoomers).

## Bounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The camera keeps within a disc around the island and is pushed back when it strays outside | done | `Camera::UpdateZoomers` pulls a position destination more than 3500 from the centre back to it in 3 s, in every mode but the temple's (`script_camera::k_DiscRadiusSquared`), as the original's camera update; `ScriptCamera.DiscOfTheWorld` (`test/test_script_camera_mode.cpp`), `CameraSphere.ACameraModelWithALensOfItsOwnStaysOffTheWorldDisc` |
| The player's camera is also held inside the map's 5120 disc each frame | partial 75% | (added) `DefaultWorldCameraModel::ConstrainDisc`, measured across the ground only (x and z, from 2560, 2560) and moving only x and z, the height kept, as the original (`camera_zones::KeepInsideMapDisc`; `MapDisc.*` in `test/camera/test_camera_zones.cpp`; debug: the camera window's distance from the map's middle); every frame the origin and the focus go through the map's middle and back in single precision, which rounds coordinates below 1280, as the original ([the round trip](../../../camera/player-camera.md#player-camera-map-middle-rounding); `MapDisc.RoundsTheOriginAndTheFocusThroughTheMiddleAsTheOriginal`, the oracle's bits; debug: the camera window's last rounding); the original also counts it as a collision for the camera help ([camera_help.md](camera_help.md)). Missing: the original leaves it out in the editor (bw1-decomp's player camera update) |
| The camera never rises above a ceiling height | partial 10% | the 30 000 ceiling (`k_MaxAltitude`) only bounds the arc-ball tilt step in `DefaultWorldCameraModel::ComputeDistanceFromBoundY`. Missing: the original's height limit (30 000, or a zone's limits) and the slide back under it along the view, step 1 and step 8 of the zones' steps |
| A land's zone can cap how high the camera goes and how far above the land | pending | (added) the zone file's highest height and height above the land are read into `camera_zones::Zones` (`useMaxAltitude`, `useHeightAboveLand`), but the camera does not use them (step 1 of the zones' steps) |
| The camera floats at least a little above the land or sea under it | partial 60% | the player model keeps 3 over the land (`ConstrainAltitude`, `k_FloatingHeight`, 2.9999 to match the recorded runs of `test/camera/test_camera.cpp`). Missing: the original measures the land before the frame's movement, raises it over exclusion domes, and its camera update keeps what is drawn 1 m over the land in every mode, where openblack's drawn 1 m (`script_camera::DrawnCamera`) only applies while a script drives |
| The camera's tilt is kept between looking slightly up and nearly straight down | done | pitch clamp to −π/6 .. 7π/16 in `DefaultWorldCameraModel::TiltZoom`, the original's bounds; recorded runs `TiltUpDown`, `TiltDownZoomOut` (`test/camera/scenarios`) |
| What the camera looks at is kept on the land under the middle of the screen, at least 10 away | partial 40% | `DefaultWorldCameraModel::UpdateModeCartesian` puts the focus on the land under the screen centre, matching the recorded runs in that mode. Missing: the original sets the focus every frame along the view at the distance to the land under the screen centre (else a smoothed distance), at least 10; openblack does it only in its idle mode, one short of the land and at least 0.1 (merged with the former row "the focus is kept within a set distance of the camera") |
| The camera moves at most a set distance each frame | pending | (added) the original holds a frame's move to 1.6 times the focus distance a second, at least 300, but not in the both-buttons mode or while zooming; openblack's `ConstrainCamera` only re-grips past that distance, it does not hold the move |

## Collision

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Dragging the land stops the camera 3 short of land in its way | done | `DefaultWorldCameraModel::StopPanShortOfLand` with `camera_pan::StopShortOfLand`, from the grip camera; `CameraPan.StopsThreeShortOfLandInItsWay`, `WorldCameraPan.TheLandStopsTheCameraWhereverTheDrawnCameraIs` ([the land grip](../../../camera/player-camera.md#player-camera-the-land-grip); the original stops short only in the land drag) |
| A drag that goes down meets the sea and stops there, within 7500 of the camera | done | `CameraPan.TheSeaIsMetGoingDownNearTheCamera`, `WorldCameraPan.TheSeaStopsTheCameraOnlyWithin7500OfTheDrawnCamera` |
| Zooming or turning into a hill does not put the camera inside it | partial 40% | `DefaultWorldCameraModel::ConstrainCamera` floats the camera over the land after each frame. Missing: the original's land probe (3 rings of 8 directions, step 4 of the zones' steps) |

## Zones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land can have exclusion domes the camera cannot enter, and the camera is eased out of one it strays into | partial 20% | `SET_CAMERA_ZONE` reads the zone's exclusions faithfully (`player_camera::SetCameraZone`, `src/Camera/PlayerCameraScript.cpp`; tests in `test/test_script_camera_dual.cpp`, `CameraZoneSystem.*`). Missing: the push out of domes and cylinders and the domes raising the land under the camera (step 3 of the zones' steps) |
| Scripts can set an inclusion zone that keeps the camera inside an area, as the first land's tutorial does in stages | partial 80% | `SET_CAMERA_ZONE` loads the inclusion polygon, and the player camera is put back inside it when it goes out, with the shake, the out of bounds sound and the controls held back (`DefaultWorldCameraModel::KeepInsideFence`, `camera_zones::PushInsideFence`; tests `CameraZones.*`); `GET_INCLUSION_DISTANCE` gives the distance to the fence the camera's update finds (1e10 without a fence). Missing: the force field drawn and its pulse; the fence comes in openblack's own order of the frame's steps (approximate in the wiki) |
| Clearing a land's zones as a new land loads | partial 70% | `cameraZoneSystem` is made again with each land (`src/Locator.cpp`), as the original's map clear resets the zones. Missing: the script reboot's own reset is not checked, the fixed rotation is not cleared (`player_camera::Reset` has no caller), and no test |

## Drawing distance

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The near plane comes in close to the ground and goes out with height, so land and objects aren't cut away | partial 90% | `near_clipping::NearPlane` in the camera update (`src/Game.cpp`), the original's expression bit for bit (`NearClipping.FollowsTheHeightOverTheLand`, `IsTheOriginalsExpressionToTheBit`). Missing: the original reads the height of the eye drawn the frame before; openblack's GPU near plane takes the camera just updated (only the draw list takes the previous eye; a queued fix in the wiki's pending) |
| Scripts and camera paths can bring the near plane right in for close shots | done | `SET_GRAPHICS_CLIPPING` keeps its second pop, as the original, in `CinematicDirectorSystem::SetCloseClipping`, which the near plane reads with a holding camera path (`src/Game.cpp`), cleared when the scripts restart and kept by a land change (`Game::ResetScripts`); `NearClipping.ScriptsCanClipClose` ([camera](../../../camera/game-camera.md#camera)) |
| The world camera's field of view is the game's default lens | done | 70 degrees (`config.cameraXFov`, `script_camera::k_DefaultFov`), as the original's camera sets it at start ([GCamera](../../../camera/game-camera.md#gcamera-and-its-zoomers)); only a new game puts it back, a land change keeps the lens a script set (`script_camera::ClearModes`, 2026-10-22); `ScriptCamera.FovFollowsGameTime`, `ScriptCamera.ALandChangeEndsTheModesAndKeepsTheLens`; the Land Island window shows it |
