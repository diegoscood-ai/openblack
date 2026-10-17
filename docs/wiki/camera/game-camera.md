# The game camera

## Summary

The original keeps its camera in one object, `GCamera`: a set of zoomers (smooth movers) for the focus, the position
and the field of view, and a stack of camera modes on top of them (the player's mode, the script's, the dual camera,
the path cameras). Once a drawn frame the current mode sets the zoomers' destinations, the zoomers step, and the result
is kept inside the world, above the ground and away from its own focus before it is drawn, with a near plane that
follows the camera's height above the land. The lens is a horizontal field of view of 70 degrees with no far plane.

openblack has no mode stack: the player's camera (`Camera` with `DefaultWorldCameraModel`) owns one set of zoomers, and
the script camera module stands for `GCamera`'s zoomers while a script holds the camera, taking the player's as they
are and handing them back. The frame's steps, the world disc, the ground clearance, the near plane, the lens and the
script field-of-view tests are ported **faithfully**. What the player's mode does is on
[the player camera](player-camera.md); what the scripts do with the camera is on [the script camera](script-camera.md).

## The original

### Camera

- Horizontal field of view 70 degrees, no far plane; the near plane follows the height above the ground.
- **Near plane** (`LandFeature::GetNearClipping` 0x5E2F30, its only caller `GCamera::Update` 0x4424AF, which stores
  it in [0xE839E0] every frame, without a threshold, before `ChangeFov`):
  - 0.1 [0x8AB22C] while [0xD1A2F8] is not 0 (close clipping);
  - otherwise h = g_camera.y − `GetAltitude` at the MapCoords (ftol(x·65536·0.1), ftol(z·65536·0.1)), stored as a
    float (0x5E2F46..0x5E2F9A);
  - h ≤ 0 (or not a number): 0.3 [0x8AB23C]; h > 20 [0x8C7658]: 3.5 [0x8CF304];
  - else (h·0.05 [0x8C7BD4])·3.2 [0x8C7C8C] + 0.3, each step rounded to a float by the game thread's 24-bit FPU
    (0x5E2FD0..0x5E2FE0). It is not 0.3 + 0.16·h: that rounds one or two steps differently for about half the
    heights between 0 and 20 (at most 2.4·10⁻⁷), and both give 0.3 at 0 and 3.5 at 20
    ([Floating point](../engine/floating-point.md#the-x87-arithmetic)).
  - h is taken from g_camera before this frame's `UpdateCamera` (0x4425F6..0x442602 runs after 0x4424AF), that is
    from the eye drawn the frame before.
  - `SET_GRAPHICS_CLIPPING` (`GScript::SetGraphicsClipping` 0x708C50) pops its two arguments and keeps only the first
    (the second pop): [0xD1A2F8] = 1 if it is not 0 (0x708C71..0x708C77). `GScript::CleanGameForScriptReboot`
    clears it (0x6EBBEF).
  - `CameraModePath::SetUpNearClipping` 0x460F10 writes 0.1 when a path camera is made, but `GCamera::Update` writes
    the near plane again on every frame after it: **🔍 inferred** that 0.1 never reaches a draw.

### GCamera and its zoomers

**Faithful.** GCamera (0x1D8 bytes) stores the camera in LH3DLib Zoomers (`SetDestinationWithSpeedAndTime` 0x407D60,
`Update` 0x442720; [the zoomer](../engine/matrices-and-zoomer.md#zoomer-lh3dlib)): the focus at +0x88/+0xB8/+0xE8,
the position at +0x118/+0x148/+0x178 and the field of view (radians) at +0x1A8. A move starts from the current value
and speed and arrives with speed 0 at T; T < 0.001 places it immediately; when T ends the value is pinned to the
destination. The player's mode sets its destinations every frame with `Zoomer3d::SetDestinationWithTime` 0x44E760
(0x4604A4..0x4604D2).

- **The mode stack** is at +0x28 (12 modes, 0x441CEA) with the index at +0x58. The player uses `CameraModeNew3`
  (vtable 0x8C7BFC), which can always be left. `SwitchToViewMode` 0x441CD0 pushes a mode; `PopViewMode` 0x441C50
  cleans the current one up (vt+0x18), deletes it, lowers the index and restarts the mode under it (vt+0x10); a mode
  that cannot be left blocks a new one (`GCamera::CantExitCurrentMode` 0x441B70). `GCamera::CheckStackedModesForValidity`
  0x441D40 runs once a turn (`GGame::ProcessTurn` 0x54E743) and deletes, from the bottom of the stack (0x441D57), every
  mode whose `IsStillValid` is 0 (vt+0 with 1, 0x441D86); when the current one goes, the new current one is restarted
  (vt+0x10, 0x441DEB) and the mode's seconds are not reset. The script and dual modes on the stack are on
  [the script camera](script-camera.md#the-script-mode).
- The GCamera constructor sets the field of view to 70 degrees at once (0x441A78..0x441A83): `SetCameraFov(fn_00443670,
  0)`, where fn_00443670 = fld [0x8C762C] = 1.2217305. A new map starts there, with no script mode (the constructor and
  `GScript::Reset` 0x6EB2D0).
- **The mode's seconds**, GCamera +0x68, the seconds since the last mode change: 0 in `SwitchToViewMode` 0x441CD0 (at
  0x441D30 for a dual camera) and in `PopViewMode` (0x441C86), plus the frame's camera seconds at 0x441FCD (before the
  mode's Update), and 2 after fn_0044BB30 (0x44C141). The follow and the dual camera ease their pace by it
  ([Following](script-camera.md#following)).
- The script opcodes find the current mode with `__RTDynamicCast` to 0x9CE188 (CameraModeScript); the dual opcodes use
  0x9CE790 (CameraModeTwoObjects; 0x6ED3F0 / 0x6ED43D).
- `GCamera::SetCameraFov` 0x443680: with t == 0 or t < 0.001 it sets the value; otherwise it moves towards the field of
  view with final speed 0 (an inline copy of 0x407D60, 0x4436CB..0x4437D7). Its t is in seconds of game time.
- Degrees to radians is [0x92B20C] = 0.0174532924 (0x3C8EFA35), used by SET_CAMERA_LENS 0x6EE2E0, MOVE_CAMERA_LENS
  0x6EE280 and CAMERA_PROPERTIES 0x6EE025.
- **Two point helpers of GCamera** that every camera built from angles uses:
  - `SetPointFromPointDistanceHeadingAndPitch` 0x442810: from a point, a distance, a heading and a pitch; the sine and
    cosine at the x87's precision, cos q kept as a float, then each product in order (0x442811..0x442856).
  - `GetHeadingAndPitchFromPoints` 0x4428D0(a, b): v = a − b (0x4428D3..0x4428F3); when |v.x| and |v.z| are both below
    [0x8C7620] = 0.01 (a double), heading 0 (0x442925) and pitch 0x3FC50A6B = 1.5393804 (0x44292B); else heading =
    π [0x8C36A0] (0x40490FDB) − fn_007FAA50(v), the call at 0x44293A with its value still on the FPU stack (`fsubr`
    at 24 bits, 0x44293F; `fstp` 0x44294D), and pitch = fn_007FA990(sqrt(v.z² + v.x²), v.y) (call 0x44296D, `fstp`
    0x442976; 0x442950..0x442976). fn_007FAA50 (the symbols call it `SetUnitDirectionVectorFromScreenPoint`) is 0 when
    x² + z² ≤ 1e-6, else fn_007FA990(−z, x), an octant arc tangent
    ([Angles](../engine/coordinates-and-angles.md)).
  - `GCamera::GetThingViewingDistance` 0x441F20: the thing's `GetHeight` (vt +0x42C) × 8 ([0x8C7108]).

### One frame of GCamera::Update

**Faithful** (0x441F80, from `ProcessGraphicsEngine` 0x54D879, once a drawn frame):

1. dt = `GetCameraTimeInc` 0x555820 · 0.001 [0x8AC418] = the frame's wall-clock ms (`g_delta_time`), integer ms
   (0x441F95..0x441FAA), capped at 0.1 s [0x8AB22C] (0x441FB0..0x441FC1).
2. `Update` of the mode (vt+0x08, 0x441FD9; the script mode: `UpdatePath` and the follow's `CameraModeFollow::Update`
   0x44C160).
3. The 6 zoomers with dt (the position 0x441FEE..0x442000, the focus 0x442017..0x442029); a NaN gives back the last
   good one (0x4420D9).
4. Position and focus almost equal (< 0.001) → position x − 1, y + 1, only in what is drawn (0x4421D5..0x442228).
5. World disc: if the position's **destination** is more than 3500 from (2560, 0, 2560), new destination
   d / (|d| · 0.000285796) + centre in 3 s (0x44222C..0x44232A).
6. Citadel: what is drawn does not move (0x442337). Otherwise what is drawn stays 1 m above the ground, also raising
   the focus (0x44242B).
7. The near plane ([Camera](#camera), 0x4424AF).
8. Field of view: its zoomer with `g_game_time_inc` · 0.001 (**game time**, not wall-clock) and `LH3DTech::ChangeFov`
   0x8195B0 (0x4424F6..0x4425C3).
9. `LH3DTech::UpdateCamera` 0x819920 (0x442622): the debug camera and the shake, then g_camera
   ([The drawn camera](#the-drawn-camera-the-debug-camera-and-the-shake)).

- The world disc constants: centre [0x8C7618] = 2560, squared limit [0x8C7614] = 1.225e7 (3500²), scale [0x8C7610] =
  0.000285796 (0x3995D6E2), time 3 s (0x40400000).
- The original clamps in the citadel too (0x44222C comes before the citadel test at 0x442337, `cmp
  [g_game+0x205A28], 1`). The temple's rooms never meet the disc [V]: they draw through their own cameras
  (`InnerCamera::PreDraw` 0x796920, vtable slot +4 of InnerCamera, CreatureRoomCamera, ChallengeRoomCamera and
  WorldRoomCamera: their own zoomers, a fixed π/2 field of view, `LH3DTech::UpdateCamera` at 0x7969CD), not through
  GCamera's position zoomer (+0x118), which stays the land camera's.
- The 1 m ground clearance is [0x8AA390] (0x44242B..0x4424AB). It is measured under the nudged position. It is skipped
  in CameraModeFree and when GCamera+0x78 is set.
- NaN in the zoomers: the last good drawn camera is kept in the statics 0xC59B48 (position) and 0xC59B38 (focus),
  first (1000, 0, 1000) (0x4420D9..0x4421D5).
- The field of view goes to `ChangeFov` at 0x4425C3, and not inside the citadel (0x4424F6).
- `LH3DTech::UpdateCamera` 0x819920 is called from `GCamera::Update` 0x442622, outside the citadel and not during a
  playback.

#### The lens

- `ChangeFov` 0x8195B0 / `UpdateViewPort` 0x81909C: T = tan(fov / 2) (0x8195B8..0x8195D7), fx [0xE83A00] = 1 / T, fy
  [0xE83A04] = aspect / T, with aspect [0xE839EC] = W / H (the W / H that `SetProjectionMatrixPerspective` is given).
- `Get3DPointFromScreen` 0x81B370 works with the depth along the camera's forward axis, not with a ray length. It uses
  [0xC3812C] = near·T and [0xC38130] = near·T / aspect.
- In camera space: ((x − hW)·near·T / hW, (hH − y)·near·T / aspect / hH) × depth / near, z = depth. Then the camera's
  rotation and g_camera take it to the world: eye + right·x + up·y + forward·z (0x81B3BE..0x81B43A).
- Projection of `LH3DSprite::Draw` (0x840930..0x8409D0) / `ProjectPoint` 0x819390, without clipping: sx = (X / Z + 1)·hW,
  sy = hH − Y / Z·hH, with X = fx·x and Y = fy·y.

#### The drawn camera: the debug camera and the shake

`LH3DTech::UpdateCamera` 0x819920 takes the position and focus `GCamera::Update` worked out and makes them g_camera
[0xEA1DB8] / [0xEA1DC4], the camera drawn this frame:

- **The debug camera** g_camera_mode [0xEA9EC8], read at 0x81997A first (0x81997A..0x8199DD): 2 draws the camera from
  [0xEA1B58] towards [0xEA1B68], 1 only turns the focus to [0xEA1B68], 0 (or any other value) neither. GCamera's
  zoomers are not touched, so it holds only where `GCamera::Update` calls `UpdateCamera` (not in the citadel). Its
  writers: PLAY_JC_SPECIAL 1 / 2, `Intro::ReleaseAll` 0x5DFCCA and `CleanGameForScriptReboot` 0x6EBC0C (0); the
  GCameraEditor's writers are debug only. The focus object [0xEA9ECC] (its +0x38, 0x8199E1..0x819A01) has no
  writer and stays null (its address occurs only at the reads 0x8199E1 and 0x81A008; 0 after start-up), so it never
  moves the target [V]; fn_00819F50 (0x819F50, the falling spell's camera) repeats the same two tests (0x819FAA,
  0x81A008).
- **The shake**: then fn_008210C0 (the real `AdjustCameraPosTarget`: bw1-decomp's symbol 0x437E70 is another function,
  called at 0x819A0B) moves the drawn position and focus by the nearest shake, so a shake moves the drawn camera only,
  never GCamera's zoomers, and does not build up. The shakes themselves (SHAKE_CAMERA, their list, the six draws) are on
  [the script camera](script-camera.md#shake).

#### What is on the screen

The scripts' "is it on the screen" tests: GAME_THING_FIELD_OF_VIEW 011 (`GScript::IsGameThingFieldOfView` 0x6F8130)
and POS_FIELD_OF_VIEW 012 (`GScript::IsPosFieldOfView` 0x6F8060). Both go through the drawn camera's
`LH3DTech::g_world_to_clipping` [0xEA9E40], the near plane [0xE839E0] and the screen of g_info_transform.

- POS_FIELD_OF_VIEW: inside the temple or with no view, false (0x6F80ED..0x6F8109); else the point on the screen
  (0x6F810F).
- GAME_THING_FIELD_OF_VIEW: nothing, or inside the temple, false (0x6F819F..0x6F81B4). An Object (`dynamic_cast`,
  0x6F81BA): its 3D object's (+0x40) mesh (vt +0xF8, none → false) and its bounding sphere through fn_0081F1A0
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80, with the box centre of 0x868CBC). Any other GameThingWithPos: the
  point (x, `GetAltitude` + its +0x1C, z) of its MapCoords (0x6F81F4..0x6F822B).
- fn_0081F1A0 has no temple test of its own (its caller has it).

### Sources

- runblack.exe v1.42 through xref and its disassembly; bw1-decomp's symbols for the names.
- The camera's code notes moved out of openblack's `src/Camera` (the zoomers, the script camera, the lens, the field of
  view tests).

## openblack

> **Code rules.** The script camera's state lives in ECS components or Locator services, not in globals; the zone files
> and `camera.edt` load through the resource caches; resources are owned with RAII and standard types; the camera maths
> are tested with fakes in `test/`, never through the locator; comments describe behaviour in plain English, with no
> decompiled names or addresses (those belong here). See [the conventions](../project/refactor-conventions.md).

Progress: [camera](../project/progress/INDEX.md) (what our tree does of it, row by row). Porting decisions against
raffclar's tree: [port decisions](../project/port-decisions-hand-camera-interface.md).

- **The zoomers** (`src/Camera/Camera.{h,cpp}`, `Common/Zoomer.h`): `Camera::Update` calls `Camera::UpdateZoomers`
  (`UpdateZoomers` with the model's `CameraModel::CameraInterpolationUpdateInfo`), the zoomers' part of the frame: the
  model's new destinations, then each zoomer's update with min(dt, 0.1) (`k_MaxFrameSeconds`). It runs in the place of
  `GGame::ProcessGraphicsEngine`'s call.
  **⚠️ approximate** The original's dt is the integer ms of the frame; ours is the frame's float seconds. `GetAspect` is
  the W / H the projection was given.
- **The script camera module** (`src/Camera/ScriptCamera.{h,cpp}`, `script_camera::`): the position, focus and field of
  view zoomers, the script mode (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame`
  (steps 1-5 and 8) and `DrawnCamera` (steps 3-6). `UpdateCamera` does it every frame from `Game.cpp` and, while the
  script mode drives, the player model (`DefaultWorldCameraModel`) neither moves the camera nor reads keys (Script has
  no keys, 0x44C3BD). Positions and foci go in `Zoomer3` (`Common/Zoomer.h`, the same one as the player camera).
  `ScriptModeCurrent` and `DualCurrent` stand for the two casts. What the script mode does is on
  [the script camera](script-camera.md). The module's pieces and the original's functions they stand for:
  - the script mode: `Begin` (fn_00461140), `End` (fn_006ECD70, with `ReleaseDual` first), `SetPosition` /
    `SetFocus` (`CameraModeScript::SetCameraPosition` 0x461370 / `SetCameraFocus` 0x4612B0), `MovePosition` /
    `MoveFocus` (0x461702 / 0x461442), `RunPath` (`CameraModeScript::RunPath` fn_00461A80, through `DropPath`,
    fn_00461A60), `SetFov` (`GCamera::SetCameraFov`);
  - the follow: `ResetFollow` (the constructors' state), `StopPositionFollow` (`CameraModeFollow::Set(0)` 0x44BA00),
    `FocusFollow`, `PositionFollow`, `FocusAndPositionFollow`, `SetFollowProperties` (CAMERA_PROPERTIES),
    `PlaceFollowNow` (fn_0044BB30), `UpdateFollow` (`CameraModeFollow::Update`), `PositionFor`, `FollowPoint`,
    `FollowHeading`, `FacePoint`, `FacePosition` and `FaceObject` (fn_006ED710), `Validate`; the follow's least pitch
    is 0.241661 (0x3E7775FA), the face's heading limit 8π (0x41C90FDB);
  - the dual cameras: `StartDual`, `StartDualWithPoint`, `UpdateDual`, `PushDual` (`SwitchToViewMode`),
    `UpdateDualMode` (`CameraModeTwoObjects::Update`, a thing that is not an Object with radius 30, 0x41F00000),
    `CheckDualModes` (`GCamera::CheckStackedModesForValidity` with `CameraModeTwoObjects::IsStillValid` 0x461D90),
    `ReleaseDual` (`GScript::ReleaseDualCamera` 0x6ED410) and `HandBackAfterDual`;
  - `RegistryThing` reads a thing's point, height, radius and facing from the registry;
    `GCamera::GetHeadingAndPitchFromPoints` is `HeadingAndPitchFromPoints`.
  The rules of each are on [the script camera](script-camera.md#following) and
  [the dual camera](script-camera.md#dual-camera).
- **No mode stack ⚠️ approximate:** the player's mode is `Camera` + `DefaultWorldCameraModel`, and the module stands for
  GCamera's zoomers while a script mode lives. The dual cameras always sit above the script mode, and the stack's 12
  places are not counted.
- **Hand-over of the zoomers ✅ faithful:** GCamera has a single set of zoomers for all modes. openblack has the
  player's (`Camera::GetOriginZoomer/GetFocusZoomer`) and the script's: `Begin` copies the player's as they are (value,
  speed, destination and time: the script mode continues towards where the player's was going, 0x461180 does not touch
  them) and `End` returns the script's to the camera (`HandBack`), from where the player starts like
  `CameraModeNew3::Initialise` 0x456640. With the hooks `OPENBLACK_CAMERA_LOCK/FLY` the player never released the
  camera and nothing is copied.
- `CHLApi.cpp`: the opcodes of the table; `StartCameraControl` passes `cameraTaken = script_camera::Begin(...)`;
  END_CAMERA_CONTROL and the task stop (Game.cpp) call `script_camera::End`. When loading a map, `Reset` (the field of
  view at 70 degrees at once).
- **⚠️ approximate** While the script drives, the player camera carries what is drawn (with the nudge and the metre
  above the ground), not the script zoomers. Without script mode, 035 compares the player's zoomers with their
  destination (the same rule 0x441700). `SetPositionAndFocus` does not have the early exit of 0x4438C0. The game ms of
  the frame are those of the previous frame's clock (the original updates the camera after the turns).
- **🔍 inferred** 284/286 without script mode also set the player camera.
- **The world disc (same result, another route):** `Camera::UpdateZoomers` skips it while the camera's model has a lens of its own
  (`CameraModel::GetLens`), which only the temple's has: our temple interior sits at the origin, about 3620 from the
  centre, outside the disc. The original's interior never meets the disc either (see
  [One frame of GCamera::Update](#one-frame-of-gcameraupdate)), so skipping the disc for the temple's own lens gives the
  original's result. Test:
  `CameraSphere.ACameraModelWithALensOfItsOwnStaysOffTheWorldDisc` (test_temple_camera).
- **The field of view** goes to `config.cameraXFov` (degrees) only when its zoomer changes, which every rebuild of the
  projection reads: a player's own field of view lasts until a script touches the lens. Not inside the citadel.
- **The near plane ✅ faithful:** `near_clipping::NearPlane` (`Camera/NearClipping.h`) with the close clipping of
  `CinematicDirectorSystem` (`SetCloseClipping`, cleared in `Game::LoadMap` with the script reboot), recomputed in
  `Game` after the camera's update and set whenever it changes. A camera with a lens of its own (the temple's) keeps
  its own. While a placed path holds the camera, `NearPlane(height, close)` takes `close`, the 0.1 of
  `SetUpNearClipping`; openblack sets the near plane every frame, so nothing is saved and put back. The GPU near plane
  takes the camera just updated and `GetHeightAt` at `map_coords::ToFixed` (x·6553.6): the same as the original's
  previous-frame eye and ftol(x·65536·0.1) unless a coordinate lies on a boundary; only the object draw list already
  takes the previous frame's eye (`NearClipFor`).
- **The lens** (`src/Camera/ScreenPoint.h`, `screen_point::`): `CameraPointFromScreen` (⚠️ approximate: the near
  cancels out, the original's rounding goes through it), `ProjectCamera` and `ToWorld`. Pure maths, shared by the
  falling spell (in its own camera space) and the advisor spirits (in the world through the drawn camera).
- **The drawn camera:** `ApplyShake` puts the debug camera (`SetDebugCameraMode`, `s_DebugCameraPosition` /
  `s_DebugCameraFocus`) and the shake in the draw offset, and counts the shakes down once a drawn frame
  (`camera_shake::Tick` with the frame's real ms; **🔍 inferred** after the camera, as `ProcessGraphicsEngine` runs
  before the draw).
- **The points from angles ✅ faithful:** `script_camera::PointFromDistanceHeadingAndPitch` and
  `HeadingAndPitchFromPoints` are GCamera's two helpers, step by step. `std::sin` / `std::cos` in double give
  `SetPointFromPointDistanceHeadingAndPitch`'s x87 results bit for bit: 20 000 oracle cases on 0x442810 (headings to
  ±30 rad, pitches to ±7, distances to 3000), 0 differences. Every point openblack's cameras build from angles
  goes through them; the list is on [the player camera](player-camera.md#openblack).
- **What is on the screen** (`src/Camera/FieldOfView.{h,cpp}`, `field_of_view::`): `PosInView`, `ThingInView` and
  `ObjectOnScreen`, through the drawn camera's `billboard::CameraFrame::lh.worldToClipping` and `nearZ`, as
  `ShadowMath::BlockVisible`. **⚠️ approximate** openblack's "Object" is an entity with a Mesh: its box centre through
  the Transform (`lh_matrix::BoxCentre_00868CBC`, the original's order; the cells are openblack's model's) and the
  box's half diagonal × the largest scale; without a Mesh its Transform position. False without a view (no camera or
  window, the unit tests).
- **Placed paths:** `ecs::systems::CameraPathSystem` and `Camera/CameraPathControl.h`; what the path does is on
  [the script camera](script-camera.md#placed-paths-cameramodepath). Its wiring:
  - `Begin(owner, path, placement, pause)` is the mode's creation: refused while the script mode is the current one
    (`script_camera::ScriptModeCurrent`, a fake in the tests), without a path or a camera, and for an owner that has
    one already. The paths stack, the last the current one. They move the camera's own zoomers, the one set GCamera
    has, so a path starts from wherever the camera was heading. Its one caller is the forest's camera particle
    (`ParticleAnimWithCameraCreator`,
    [the particle meshes](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome)): it asks at its
    first step that is the local player's, gives the path's time and the playing flag every frame from its drawn frame
    (`psys::forest_camera::UpdateFrame`), and lets go at frame 999 or when it goes.
  - `FollowAt(owner, pathMs, playing)` keeps the clip time and the playing flag, as `DrawAt` and the particle's turn
    write +0x28 and +0x3C. `Update` is the mode's update (the glide once, then the 0.3 s aim every update) followed by
    `Camera::UpdateZoomers`. The rules: `camera_path::PlacedGlideSeconds`, `PlacePoint`, `PathTimeFromFrame`,
    `ReachedLastFrame`, `TakesCameraBack`.
  - `Release(owner)` is `StopCamera`, wherever the owner's path is; the path under it is the current one again. The
    take-back drops every path (🔍 inferred the player's mode goes on top, and none under it is current again).
  - Game.cpp, once a frame: `HandlePlayerControl` before the player's camera keys (a key moving the camera left,
    right, forwards or backwards, the camera's frame ms, and the land grip, the interface's hand state 20 of the last
    turn, as +0x3AC == 0x14); while `HoldsCamera`, `camera.HandleActions` is skipped and the path's `Update` runs where
    `camera.Update` would (after the script camera and Creature Mode's update); the camera particles' `UpdateFrame`
    with the frame updaters, after the camera's update and before the draw, as the particle's `DrawAt`.
  - The speed-up factor reaches the camera only through the drawn frame. The earlier port's own clock ((t − pause) ×
    speed-up × 1000), its end at 999/1000 of the clip's ms and its end with the miracle are not the original's and are
    gone.
  - The debug bar's Camera window has a "Placed path" header (closed at first, `CurrentPlaced`): who placed the path
    that has the camera, the glide left, the path's time, whether it plays, its particle's last drawn frame, and a
    Release button.

### Test hooks

- `test_script_camera_mode`: a single mode, arrival, 0.1 s cap, placing with T < 0.001, disc, nudge and ground, field
  of view with game time and the return to 70 degrees in 0.5 s; `ScriptCameraFollow.*`: T rule, distance and pitch,
  point from distance/heading/pitch, heading and pitch between points, «behind», place immediately, face of an object,
  things that disappear.
- `test_script_camera_dual`: the dual (pace, focus, distance, heading, with point, validity, end of control), the shake
  (radius, decay, rolls), the zone loader and the fence it sets; `test_camera_zones`: `camera_zones::CrossFence`.
- `test_camera`: `CameraPathSystemTest.*` (the refusal under a script camera, the glide over the pause and 0.3 s, the
  0.3 s aim while playing, `Release`, a path over another, the take-back, the debug read-out) and `CameraPathRules.*`
  (the glide's float sum, the point's order, 6633 ms at frame 999 is 6626, the stop at 999 and not 998).
- `test_magic`: `ForestCameraRules.*` and `ForestCamera.*` (`test_psys_forest_camera.cpp`), the forest's camera
  particle that drives the placed path.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` win over the script camera (`Drives()`, not original).
- In game: Land 1, when the tutorial requester's answer does not skip the tutorial, runs FollowUs and
  CreaturesInGlade with the script camera.
- The debug bar's Camera window: the placed path's read-out and its Release button (above).

## Pending

- **The near plane's eye** ([Camera](#camera)): the original takes h from the eye drawn the frame before and its
  MapCoords are ftol(x·65536·0.1); openblack's GPU near plane takes the camera just updated and `map_coords::ToFixed`.
  Only the object draw list takes the previous eye (`NearClipFor`). Queued fix: take the previous frame's eye for the
  GPU near plane too.
- **The frame's time**: the camera's dt is integer ms in the original; ours is float seconds ⚠️ approximate.
