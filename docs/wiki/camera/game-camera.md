# The game camera

## Summary

The original keeps its camera in one object, `GCamera`: a set of zoomers (smooth movers) for the focus, the position
and the field of view, and a stack of camera modes on top of them (the player's mode, the script's, the dual camera,
the path cameras). Once a drawn frame the current mode sets the zoomers' destinations, the zoomers step, and the result
is kept inside the world, above the ground and away from its own focus before it is drawn, with a near plane that
follows the camera's height above the land. The lens is a horizontal field of view of 70 degrees with no far plane.

The player's camera (`Camera` with `DefaultWorldCameraModel`) owns one set of zoomers, and the script camera module
stands for `GCamera`'s zoomers while one of its modes holds the camera, taking the player's as they are and handing
them back; its modes (the script mode and the dual cameras) sit on a stack over the player's mode
(`camera_stack::ModeStack`). The frame's steps, the world disc, the ground clearance, the near plane, the lens and the
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
    clears it (0x6EBBEF), only when the scripts restart: a land change keeps it
    ([what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets)).
  - `CameraModePath::SetUpNearClipping` 0x460F10 writes 0.1 when a path camera is made, and its `Cleanup` 0x460F69
    puts the saved one back, but `GCamera::Update` writes the near plane again on every frame after them: its store
    (0x4424B4) is on the one way through the function (every branch before it joins at 0x4424AF, its only `ret` is at
    0x4426F7), after the mode's own update (0x441FD9), and its callers are the game's loop and graphics frame. So the
    path's 0.1 never reaches a draw. The other writers are the temple's inner camera (0.2, `InnerCamera::PreDraw`), the
    renderer's start (3.5) and a developer effect (`RunAlexFX`).

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
  mode whose `IsStillValid` (vt+0x14, 0x441D5E) is 0, the current one after its `Cleanup` (vt+0x18, 0x441D7C), then
  deleted (vt+0 with 1, the deleting destructor, 0x441D86); when the current one goes, the new current one is restarted
  (vt+0x10, 0x441DEB) and the mode's seconds are not reset. The script and dual modes on the stack are on
  [the script camera](script-camera.md#the-script-mode).
- The GCamera constructor sets the field of view to 70 degrees at once (0x441A78..0x441A83): `SetCameraFov(fn_00443670,
  0)`, where fn_00443670 = fld [0x8C762C] = 1.2217305. A new game starts there (`GGame::Init` makes the camera again).
  A land change keeps the camera and its field of view (no `SetCameraFov` caller is reached from `LOAD_MAP`), while
  `ClearMap`'s `ClearCameraStack` 0x442FE0 (0x552C12) deletes every mode, a script mode too, and makes a new
  `CameraModeNew3`; the scripts' restart clears the stack the same way (0x6EBCD8)
  ([what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets)).
- **The mode's seconds**, GCamera +0x68, the seconds since the last mode change: 0 in `SwitchToViewMode` 0x441CD0 (at
  0x441D30 for a dual camera) and in `PopViewMode` (0x441C86), plus the frame's camera seconds at 0x441FCD (before the
  mode's Update), and 2 after `CameraModeFollow::SetToDestinationPosition` (0x44C141). The follow and the dual camera ease their pace by it
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
    π [0x8C36A0] (0x40490FDB) − `heading_from_direction_vector`(v), the call at 0x44293A with its value still on the FPU stack (`fsubr`
    at 24 bits, 0x44293F; `fstp` 0x44294D), and pitch = `atan360`(sqrt(v.z² + v.x²), v.y) (call 0x44296D, `fstp`
    0x442976; 0x442950..0x442976). `heading_from_direction_vector` 0x7FAA50 is 0 when
    x² + z² ≤ 1e-6, else `atan360`(−z, x), an octant arc tangent
    ([Angles](../engine/coordinates-and-angles.md)).
  - `GCamera::GetThingViewingDistance` 0x441F20: the thing's `GetHeight` (vt +0x42C) × 8 ([0x8C7108]).

### One frame of GCamera::Update

**Faithful** (0x441F80, from `ProcessGraphicsEngine` 0x54D879, once a drawn frame):

1. dt = `GetCameraTimeInc` 0x555820 · 0.001 [0x8AC418] = the frame's wall-clock ms (`g_delta_time`), integer ms
   (0x441F95..0x441FAA), capped at 0.1 s [0x8AB22C] (0x441FB0..0x441FC1). **The previous frame's ms:**
   `ProcessGraphicsEngine` calls `GCamera::Update` (0x54D879) before `Process3dEngine` (0x54D88E), whose
   `LH3DRender::StartFrame` 0x82F0E0 is what writes `g_delta_time` [0xC38134] (0x82F18D; ≤ 0 → 1), so the camera
   reads the ms the last `StartFrame` took (bw1-decomp `Game.cpp:2043-2068`). The same holds for the camera update
   before the loop (`Game.cpp:1770`) and the multiplayer start's (`:1807-1808`). `g_delta_time`'s data value is 1, but
   the loading screens' and front end's `StartFrame` calls write it before the first game frame, so the first frame's
   camera reads their last frame's wall-clock ms (not deterministic). With the recorded interface playing back it
   reads `g_game_time_inc`, which the frame clock (step 3 of [the loop](../engine/loop.md#the-original-loop)) set
   before the camera: that frame's.
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
10. The moved flag and the weather at the camera (checked 2026-11-21, assembly 0x4425DE..0x4426C0): g_camera's
    position is copied to [esp+0x10] at 0x4425DE, **before** `UpdateCamera`, so it is the camera drawn the frame
    before (shaken, the debug camera's). `UpdateCamera` gets ecx = &[esp+0x1C] (position) and edx = &[esp+0x28]
    (focus), the locals of steps 3-6, and only copies them (0x819968..0x81998F; its `pos = position` copy, bw1-decomp
    `LH3DTech.cpp:434`), so the debug camera and the shake never reach them. GCamera +0x74 = (that previous g_camera
    ≠ the local position, 0x442627..0x44266E; in mode 0 not written during a playback). `GetWeatherSmooth` 0x835180 is
    called at 0x4426BA with &[esp+0x1C] in every mode: **this frame's held position** (the zoomers', NaN-replaced,
    nudged and 1 m over the land; in modes 1 and 2 the frame before's g_camera held over the land), never the shaken
    or debug-moved one; the result is GCamera's weather (+0x80) and the overcast `Cloud` · 0.01. openblack:
    `script_camera::ApplyShake` sets `Camera::SetWeatherPoint` to the held camera before the debug camera and the
    shake (`CameraShake.TheWeatherAtTheCameraIsReadAtTheHeldCameraNotTheShakenOne`), read by
    `Clouds::WeatherOvercastAtCamera` and `audio::CameraWeather`; before 2026-11-21 mode 0 read `GetOrigin`, the
    shaken camera. Inside the citadel openblack still reads `GetOrigin` (the zoomers'), where the original reads the
    held previous g_camera: not yet checked against the temple's rooms.

- The world disc constants: centre [0x8C7618] = 2560, squared limit [0x8C7614] = 1.225e7 (3500²), scale [0x8C7610] =
  0.000285796 (0x3995D6E2), time 3 s (0x40400000).
- The original clamps in the citadel too (0x44222C comes before the citadel test at 0x442337, `cmp
  [g_game+0x205A28], 1`). The temple's rooms never meet the disc [V]: they draw through their own cameras
  (`InnerCamera::PreDraw` 0x796920, vtable slot +4 of InnerCamera, CreatureRoomCamera, ChallengeRoomCamera and
  WorldRoomCamera: their own zoomers, a fixed π/2 field of view, `LH3DTech::UpdateCamera` at 0x7969CD), not through
  GCamera's position zoomer (+0x118), which stays the land camera's.
- The 1 m ground clearance is [0x8AA390] (0x44242B..0x4424AB). It is measured under the nudged position. It is skipped
  in CameraModeFree and when GCamera+0x78 is set (never in v1.42: the byte is always 0, [not in the
  original](../project/not-in-the-original.md#cameracamera_limits)). openblack: `script_camera::HoldDrawn`, applied to the drawn camera of every mode
  outside the citadel in `ApplyShake` (as a draw offset, the zoomers kept), before the debug camera and the shake, as
  `UpdateCamera` 0x819920 comes after it; a script mode's drawn camera has it already (2026-11-10).
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
  moves the target [V]; `LH3DTech::UpdateCamera` (0x819F50, the falling spell's camera) repeats the same two tests (0x819FAA,
  0x81A008).
- **The shake**: then `LH3DCameraChecker::AdjustCameraPosTarget` 0x8210C0 (called at
  0x819A0B) moves the drawn position and focus by the nearest shake, so a shake moves the drawn camera only,
  never GCamera's zoomers, and does not build up. The shakes themselves (SHAKE_CAMERA, their list, the six draws) are on
  [the script camera](script-camera.md#shake).

#### What is on the screen

The scripts' "is it on the screen" tests: GAME_THING_FIELD_OF_VIEW 011 (`GScript::IsGameThingFieldOfView` 0x6F8130)
and POS_FIELD_OF_VIEW 012 (`GScript::IsPosFieldOfView` 0x6F8060). Both go through the drawn camera's
`LH3DTech::g_world_to_clipping` [0xEA9E40], the near plane [0xE839E0] and the screen of g_info_transform.

- POS_FIELD_OF_VIEW: inside the temple or with no view, false (0x6F80ED..0x6F8109); else the point on the screen
  (0x6F810F).
- GAME_THING_FIELD_OF_VIEW: nothing, or inside the temple, false (0x6F819F..0x6F81B4). An Object (`dynamic_cast`,
  0x6F81BA): its 3D object's (+0x40) mesh (vt +0xF8, none → false) and its bounding sphere through `IsObjectOnScreen` 0x81F1A0
  (`LH3DBoundingBox::CheckRegionOnScreen` 0x868C80, with the box centre of 0x868CBC). Any other GameThingWithPos: the
  point (x, `GetAltitude` + its +0x1C, z) of its MapCoords (0x6F81F4..0x6F822B).
- `IsObjectOnScreen` 0x81F1A0 has no temple test of its own (its caller has it).

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
  `GGame::ProcessGraphicsEngine`'s call. `GetAspect` is the W / H the projection was given.
- **The frame's time** (`src/Camera/CameraFrameTime.h`, `camera_frame::`; test `test_camera_frame_time`): every
  camera update of the frame (`Game.cpp`'s camera step: the player's mode and its zoomers, the follows, the placed
  paths, the dance camera, the island's camera behind the temple, and the player's mode's controls in the input step)
  moves on by the frame's whole camera ms (`game_clock::CameraFrameMs()`, as `GetCameraTimeInc` 0x555820), never the
  frame's own µs. `camera_frame::Seconds` is the whole ms as a float × the float 0.001 (0x441F95..0x441FAA), which rounds
  as the product, not as ms / 1000 (10 ms is 0x3C23D70B, one step above 0.01f); `CappedSeconds` is at most 0.1
  (0x441FB0..0x441FC1). The capped one is GCamera's TimeDelta and its time: the zoomers, the script camera's modes
  (`script_camera::Frame`), Creature Mode's follow's time and its keys (`CameraModeFollow::ProcessKeyMovement`,
  `CameraModeFollow.cpp:421`, reads TimeDelta) and the paths' key step. The player's mode reads it uncapped, as
  `CameraModeNew3::Update` and `ProcessKeyMovement` take `GetCameraTimeInc() · 0.001f` themselves
  (`CameraModeNew3.cpp:2249`, `:1353`). With the checks' fixed step of 16 ms the frame's µs were already 16 000 and
  16 × 0.001f is 16 / 1000 rounded (a power of two), so only frames whose real ms are not the step differ from before:
  the frame after a film holds its frames, and the controls of a frame before the real clock has had a frame (its
  starting 1 ms). Left as they are: the temple's rooms
  (`Temple::Update`, their own cameras, not GCamera's modes) and the field of view's zoomer, on the game's ms
  (step 8).
- **The previous frame's ms** (`game_clock::CameraUpdateMs()`, `TimeSystemInterface::GetPreviousFrameRealTime`; test
  `GameClockTest.TheCameraReadsTheLastFramesRealMs`): openblack takes the frame's real ms (`UpdateRealClock`, the
  original's `StartFrame`) in the frame-time step, before the camera step, so the camera step and Creature Mode's
  follow (`CreatureCameraModel.cpp`) read the ms the frame start before took, as the original's camera, which runs
  before its frame's `StartFrame`. The input step's controls run before `UpdateRealClock` and read the same value
  through `CameraFrameMs()`. Under the checks' fixed step every frame start takes 16 ms, so the camera reads the
  same 16 on every frame but two kinds: the first frame, which reads the clock's starting 1 ms where it read the first
  frame start's ms (openblack has no loading-screen frame starts before it; the original's first frame reads the last
  loading frame's wall-clock ms, which no check can repeat), with the frame after it reading that first ms in turn;
  and a film's held frame, whose held ms the camera now reads one frame later. A land change does not reset the clock,
  so the cycle's land starts read the last frame's ms as any frame.
- **The script camera module** (`src/Camera/ScriptCamera.{h,cpp}`, `script_camera::`): the position, focus and field of
  view zoomers, the script mode (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame`
  (steps 1-5 and 8) and `DrawnCamera` (steps 3-6). `UpdateCamera` does it every frame from `Game.cpp` and, while the
  script mode drives, the player model (`DefaultWorldCameraModel`) neither moves the camera nor reads keys (Script has
  no keys, 0x44C3BD). Positions and foci go in `Zoomer3` (`Common/Zoomer.h`, the same one as the player camera).
  `ScriptModeCurrent` and `DualCurrent` stand for the two casts. What the script mode does is on
  [the script camera](script-camera.md). The module's pieces and the original's functions they stand for:
  - the script mode: `Begin` (`CameraModeScript::Create` 0x461140), `End` (`GScript::StartCameraControl` 0x6ECCA0 at 0x6ECD70, with `ReleaseDual` first), `SetPosition` /
    `SetFocus` (`CameraModeScript::SetCameraPosition` 0x461370 / `SetCameraFocus` 0x4612B0), `MovePosition` /
    `MoveFocus` (0x461702 / 0x461442), `RunPath` (`CameraModeScript::SetCameraPath` 0x461A80, through `DropPath`,
    `CameraModeScript::ReleasePath` 0x461A60), `SetFov` (`GCamera::SetCameraFov`);
  - the follow: `ResetFollow` (the constructors' state), `StopPositionFollow` (`CameraModeFollow::Set(0)` 0x44BA00),
    `FocusFollow`, `PositionFollow`, `FocusAndPositionFollow`, `SetFollowProperties` (CAMERA_PROPERTIES),
    `PlaceFollowNow` (`CameraModeFollow::SetToDestinationPosition` 0x44BB30), `UpdateFollow` (`CameraModeFollow::Update`), `PositionFor`, `FollowPoint`,
    `FollowHeading`, `FacePoint`, `FacePosition` and `FaceObject` (`GScript::MoveCameraToFaceObject` 0x6ED600 at 0x6ED710), `Validate`; the follow's least pitch
    is 0.241661 (0x3E7775FA), the face's heading limit 8π (0x41C90FDB);
  - the dual cameras: `StartDual`, `StartDualWithPoint`, `UpdateDual`, `PushDual` (`SwitchToViewMode`),
    `UpdateDualMode` (`CameraModeTwoObjects::Update`, a thing that is not an Object with radius 30, 0x41F00000),
    `CheckDualModes` (`GCamera::CheckStackedModesForValidity` with `CameraModeTwoObjects::IsStillValid` 0x461D90),
    the map's clear `camera_checks::ClearStack` (`ClearCameraStack` 0x442FE0, from `ClearMap` 0x552C12; since 2026-11-19
    the follow and the placed paths go with the script camera's modes, the follow giving the camera back with no
    fly-back, and the player's camera starts again; test
    `CameraChecksPass.TheMapsClearTakesTheFollowAndThePathsOffWithTheScriptCamerasModes`);
    `ReleaseDual` (`GScript::ReleaseDualCamera` 0x6ED410) and `HandBackAfterDual` (since 2026-11-19 a placed path under
    the released dual camera is current again with no restart of the player's camera, as `PopViewMode` 0x441C50
    restarts the mode under it, the path's near plane only; test
    `CameraChecksPass.AReleasedDualCameraOverAPathLeavesThePathTheCameraWithNoRestartOfThePlayersCamera`);
  - `RegistryThing` reads a thing's point, height, radius and facing from the registry;
    `GCamera::GetHeadingAndPitchFromPoints` is `HeadingAndPitchFromPoints`.
  The rules of each are on [the script camera](script-camera.md#following) and
  [the dual camera](script-camera.md#dual-camera).
- **The stack of modes ✅ faithful** for the script's modes: the player's mode is `Camera` + `DefaultWorldCameraModel`,
  the bottom of the stack, and `camera_stack::ModeStack` holds the script mode and the dual cameras over it, each new
  one on top of the current one ([the dual camera](script-camera.md#dual-camera)). Creature Mode's follow and the
  placed paths are its other entries, in the same push order, and the stack's 12 places count every mode
  ([their places](#openblack), below).
- **Where the stack lives** (2026-11-19): `camera_stack::ModeStack` in `src/Camera/CameraModeStack.{h,cpp}`, moved out of
  `ScriptCamera` with behaviour identical; `CameraModeStackDifferences.*` pinned the differences then left, each
  turned into the original's rule since by its own commit (the last two, [the follows](#openblack), 2026-11-19).
  The stack and the camera's models kept under the new player's modes are owned by the camera stack service,
  `Locator::cameraStack` (`ECS/Systems/CameraStackInterface.h`, `Implementations/CameraStack.{h,cpp}`), made and reset
  with the script state that held them; the script camera reaches it through `script_camera::Modes()`, behaviour identical.
- **The paths' and the follow's places** (2026-11-19, behaviour identical): each placed path and Creature Mode's follow
  is an entry of the same stack (`StackedMode::Kind::Path`, by its owner's key, and `Kind::Follow`), in
  `ModeStack::OtherEntries`, in the order they were pushed, the first the lowest. Their own systems push and take off
  their entries (`CameraPathSystem`, `CreatureModeSystem`) and keep their own data by them (the path's playback, the
  follow's model). The stack counts them in its places (`OtherCount`, the `under` of `Push`), and its full stack deletes
  `LowestOther` through its owner (`script_camera::DeleteDroppedOtherModes`) when it was pushed before the script
  camera's lowest mode, else that mode.
- **The full stack ✅ faithful** (2026-11-19), `SwitchToViewMode` 0x441CD0 (every push, its 12 callers: at the 12th
  place the bottom mode is deleted, whatever its kind, without its `Cleanup`, and the rest move down a place): a path's
  push (`CameraPathSystem::Begin`) and a follow's (`CreatureModeSystem`'s lock-on) go through the stack's one push with
  the cap, as the script camera's pushes do (`ModeStack::MakeRoom`, from `Push`, `PushPath` and `PushFollow`), so the
  stack never holds more than 12 modes. The deleted bottom goes through its owner (`script_camera::DeleteDroppedModes`):
  the player's own mode (gone until the stack empties), a dual camera, a script mode or a new player's mode (with the
  camera's model it kept), a path (`CameraPathSystem::DeleteLowest`, nothing restarted) or a follow
  (`CreatureModeSystem::DeleteFollow`, what was under it handed to the mode over it). Tests
  `CameraModeStackFullPush.*`, `CameraPathSystemTest.APathPlacedOnAFullStackDeletesTheBottomModeThroughItsOwner`,
  `CreatureModeSystemTest.AFollowLockedOnWithTheStackFullDeletesTheBottomModeThroughItsOwner`.
  A stack is never left empty: popped to nothing, `PopViewMode` 0x441C50 sets the index to -1 and makes a new
  `CameraModeNew3` (0x441CB4, which pushes itself), and `CheckStackedModesForValidity` 0x441D40 does the same for a
  stack its pass emptied. Every pop of openblack's that can empty a stack whose player's mode a full stack deleted
  makes that new player's mode at once (`ModeStack::NewPlayersModeIfGone`), starting it from the camera as it is with
  no flight back (`Restart`, the mode's seconds from 0, `PlayerCameraRestarted`): the turn's check
  (`camera_checks::CheckModes`), the script mode's and the interaction's camera's pops (`Popped::NewPlayersMode`), the
  dual camera's release (`HandBackAfterDual`), the pop of a player's mode (`script_camera::PopPlayersMode`), and since
  2026-11-19 a follow let go of (`CreatureModeSystem::Leave`: C, `GGame::DoAction` 0x63F940's pop, `PCInput.cpp:717-725`,
  the end of a creature hold, a hand demo, a bookmark's flight) and a placed path taken off by the end of a creature
  hold (`CameraPathSystem::TakeOffCurrent`, through `script_camera::StartNewPlayersModeIfGone`); the end of the hold
  (`CHand::OperateInteractionCamera` 0x46CB60, a `PopViewMode` and nothing more) then flies nothing back. Until then
  those two gave the camera the model from before them and the player's mode stayed counted as gone until a turn's
  check took a mode off. The player's model restarted is the one the camera had under the follow or the path (the
  follow's own record keeps it), as at every other of these pops; the original's new mode is a new object, and what
  of it the restart does not set again is the constructor's (`Reinitialise(false)`, bw1-decomp
  `CameraModeNew3.cpp:1213-1223`). Tests
  `CreatureModeSystemTest.CLettingGoOfAFollowThatEmptiesTheStackWithThePlayersModeGoneStartsANewPlayersMode`,
  `CameraPathSystemTest.APathTakenOffAStackItLeavesEmptyWithThePlayersModeGoneStartsANewPlayersMode`,
  `APathTakenOffOverThePlayersModeMakesNoNewOne`.
- **The push order âœ… faithful** (2026-11-19), `SwitchToViewMode` 0x441CD0 and the makers (bw1-decomp
  `CameraModeTwoObjects.cpp:27-74`, `CameraModePath.cpp:16-28`, `CameraModeFollow.cpp:37-53`): every mode has its place
  in one push order whatever its kind (`StackedMode::pushed`), and only the top is current: the script camera's
  `ScriptCurrent`, `DualCurrent`, `InteractionCurrent` and `PlayerCurrent` are false while another system's entry was
  pushed after them (`ModeStack::OtherCurrent`), and a placed path has the camera only with no follow pushed after it
  (`ModeStack::FollowOverPath`, `CameraPathSystem::HoldsCamera`). So a path started over a current dual camera takes the
  camera (its maker refuses only a mode that cannot be left), and the dual camera's release (`ReleaseDualCamera`
  0x6ED410, which pops only a current dual camera) then pops nothing; a follow started over a current dual camera is
  pushed over it (its maker refuses only a mode that cannot be left: `CameraKeptFromTheFollow` no longer refuses a
  dual camera, nor, since 2026-11-19, a dual camera over a script mode); a follow started while a path has the camera is the current mode, the path under it, current again
  once the turn's check takes the follow off (C letting go of it puts a new player's mode over the path at once, since
  2026-11-19, [the follows](#openblack)). The path's `Cleanup` that the push runs
  (bw1-decomp `CameraModePath.cpp:41-47`) changes nothing openblack draws: its near plane is set again every frame
  ([the near plane](#camera)), its light maps' blur has no reader, and its `CHand::Show(1)` is ported for every one of the path's cleanups
  (2026-11-22, [the hand's row](../project/progress/camera/camera_paths.md)). The zoomers are
  the same for every mode: a dual camera that drove hands them back as the path or the follow goes over it
  (`script_camera::HandOverToAnotherMode`), and goes on from the camera's as that mode comes off over it
  (`script_camera::TakeBackFromAnotherMode`, from `ModeStack::RemovePath` / `RemoveFollow` answering that the entry was
  current). A follow over the paths leaving the camera fence, or let go of by the cursor keys alone or a grip of the
  land, puts its new player's mode over them too (`CoverPaths`).
  The full stack deletes the mode pushed first, a dual camera older than a path included. A save writes the current
  mode, so a follow over a dual camera is saved as the follow. The Camera window's stack readout gives each mode's place
  in the push order. Tests `CameraModeStackPushOrder.*`,
  `CameraPathSystemTest.APathStartedOverADualCameraTakesTheCameraAndTheDualsReleasePopsNothing`,
  `AFollowStartedOverAPathHasTheCameraUntilItComesOff`,
  `CreatureModeSystemTest.AFollowStartedOverADualCameraGoesOnTopAndTheDualsReleasePopsNothing`,
  `SavedCamera.AFollowOverADualCameraIsSavedAsTheFollowAndADualCameraOverItAsNoMode`. The new player's mode put over the paths is on their
  entries (`CoverPaths`, `UncoverPaths`, `PathUnder`), and the one over a follow on that follow's own record in
  Creature Mode ([the follows](#openblack)). The script camera's own clear (`ModeStack::Clear`) leaves the entries to
  their systems, as before.
- **The follows ✅ faithful** (2026-11-19), bw1-decomp `CameraModeFollow.cpp:37-53` and `:419-529`,
  `CameraModeScript.cpp:13`, `ScriptCamera.cpp:257-270`, `PCInput.cpp:712-737`, `GCamera::PopViewMode` 0x441C50 and
  `CheckStackedModesForValidity` 0x441D40: every follow is a mode of its own. Its maker refuses only a current mode that
  cannot be left (`CameraModeFollow::Create`, which would also refuse a thing the current follow follows, has no caller:
  C, the double click and the gathering's follows call the constructor), so a new follow goes over the one before,
  which stays under it. "Cannot be left" is the current mode's `CanExit` (`GCamera::CantExitCurrentMode` 0x441B70):
  false only for a script mode still valid (`CameraModeScript.cpp:150-153`) and the interaction's camera
  (`CameraModeCtrInteract.h:33`), true for the dual camera (`CameraModeTwoObjects` keeps the base's, `CameraMode.h:58`)
  and every other mode. openblack asks the same of the stack (`ModeStack::CanLeaveCurrent`, from
  `CameraKeptFromTheFollow`; since 2026-11-19, until then any script mode on the stack refused it, so a follow over a
  dual camera over a script mode was refused; test
  `CreatureModeSystemTest.AFollowStartedOverADualCameraOverAScriptModeGoesOnTop`). The original's C is read in the key
  pass whatever mode is current (`GGame::DoAction`, `PCInput.cpp:712-737`), so it pushes a follow over a driving dual
  camera; the double click is the player's mode's own, made in its update (`CameraModeNew3.cpp:3383-3387`), run only
  while that mode is current. openblack since 2026-11-20 reads C on the frames the script camera drives too, where
  Creature Mode's update does not run (`CreatureModeSystemInterface::ReadCreatureKey`, from the frame's camera step;
  until then C was read only in that update, so a follow went over a driving dual camera only with a placed path
  current over it); the double click stays the player's camera's (tests
  `CreatureModeSystemTest.COverADrivingDualCameraLocksOnOverIt`, `COverADrivingScriptModeMakesNoFollow`). The script
  mode is a follow too (`CameraModeScript`'s constructor calls the follow's), so C's test of the current mode finds it
  when its thing is the player's creature and pops it as it pops Creature Mode's follow, the script not told
  (`PopViewMode`, then a new player's mode unless one is current, `PCInput.cpp:717-725`); openblack since 2026-11-20
  (`CreatureModeSystem::LetGoOfScriptFollow`: `script_camera::TakeOffScriptMode`, the mode under it started again, then
  `camera_checks::PutNewPlayersModeOver`; test
  `CreatureModeSystemTest.CPopsAScriptModeFollowingThePlayersCreatureAndNoOther`; before, C refused it as any script
  mode still going). The maker's two other refusals are not the original's
  camera rule: a hand demo playing, and the interface inactive under a script's cinema bars (the original's C is held
  back there by `DoAction`'s wide-screen test, `PCInput.cpp:699-702`, before any mode is made; the double click and a
  load's follow are not known to be held back there). Under the bars a follow reads no key step, but its update's swing clear with Ctrl and Shift held still runs, as the original's (`CameraModeFollow.cpp:386-388`, which reads the two through the control map, blocked only by the interface level), with the key moves its last key step left (since 2026-11-21, `CreatureCameraModel::SwingClearWithTheModifiers`; test `CreatureCameraModelTest.UnderTheCinemaBarsCtrlAndShiftStillSwingItClearOfTheLand`). `CreatureModeSystem` keeps a record per follow (its key on the stack, `StackedMode::follow`, the
  thing, its `CreatureCameraModel`, the model held while a newer follow or a new player's mode is over it, the
  player's camera it took the camera from, the number of the new player's mode over it), the newest the one C, the
  saves and the debug readouts see. A script's camera mode goes over the follow (START_CAMERA_CONTROL's
  `CameraModeScript::Create` refuses only a mode that cannot be left), and the follow stays under it with its model,
  not updated while it is not current (`GCamera::Update` updates the top only); a dual camera did so already. The
  script's end deletes the script mode and puts a new player's mode on top (`ActualEndCameraControl`): the follow's
  model goes under it, and the camera has a new player's camera from the camera as it is
  (`CreatureModeSystemInterface::KeepUnderPlayersMode`, from `script_camera::End`). The turn's check then deletes only
  the ended script mode; the pop of the new player's mode (a creature hold's end, `PopViewMode`) gives the follow the
  camera's view again, its `Restart` the base one and the mode's seconds the stack's one count, from 0 at a pop and run
  on at the turn's check (`CheckModes` hands the count to the follow current again whatever went over it). C pops a
  current follow of the player's creature and, when the mode then current is not a player's mode, makes a new player's
  mode over it whatever it is (`PCInput.cpp:717-725`, `CameraModeNew3`'s constructor, refused only by a mode that
  cannot be left): a follow under it, and since 2026-11-19 a dual camera or a placed path the follow went over
  (the pop first restarts the mode under it, `PopViewMode`: when that is the player's own mode it starts again keeping
  a valid fight and flies back to its own view in 0.1 s, `CameraModeNew3::Restart`, `Reinitialise(1)`; openblack since
  2026-11-20, `CreatureModeSystem::RestartPlayersModeUnder`, the dual camera's release's `Resume`, test
  `CreatureModeSystemTest.CLettingGoRestartsThePlayersCameraUnderItAndLeaveDoesNot`; before, the player's camera was
  given back with no restart)
  (`camera_checks::PutNewPlayersModeOver`, the one way openblack makes a new player's mode over any current mode; until
  then that mode was left current). A dual camera that drove hands the camera its zoomers first, and a follow whose model
  the camera has again (under that dual camera or path) keeps it under the new mode; the paths go under it too. Tests
  `CreatureModeSystemTest.AFollowStartedOverADualCameraGoesOnTopAndTheDualsReleasePopsNothing`,
  `CLettingGoOverADualCameraOverAFollowPutsANewPlayersCameraOverThemBoth`,
  `CLettingGoOverAPlacedPathPutsANewPlayersModeOverIt`. The hold's end pops the new mode with nothing more, the mode
  under it current again. The cursor keys alone (no Ctrl or Shift), a grip of the land and the follow's place leaving the
  camera fence do the same over the current follow (`CameraModeFollow.cpp:415`, `:488-491`, `:525-528`, since
  2026-11-19; until then the keys and the grip took the follow off, so a dual camera, a path or a follow under it had
  the camera again). The grip is read at every key step of the current follow (`ProcessKeyMovement`, which
  `GGame::ProcessMapKeys` runs for the current mode each frame with the interface active, `PCInput.cpp:639-654`), as
  the interface's hand state 20 Grip Landscape (`CameraModeFollow.cpp:525`); Zoom Landscape (22, the action button held
  too) is no grip there. So a grip held from before the follow had the view (C locking on, a pop of the mode over it)
  puts a new player's mode over it at its first update. The double click that locks on is no grip: the window gives
  its second down as the double click alone (WM_LBUTTONDBLCLK, `SetButtons(0x10)`), no grab press, so no land grip
  starts, and the first press's grip ended at its up ([how the buttons reach the interface](../hand/states.md#how-the-buttons-reach-the-interface),
  step 4); the follow it starts stays however long that press is held. openblack since 2026-11-19 reads the same
  every update, `creature_mode::GripPutsPlayersCameraOver` on `HandSystemInterface::GetInterfaceHandState` (as the
  miracle's path reads its take-back's grip), and its double click's second down holds no button either
  (`input::ApplyMouseButton`); until then only a fresh grip counted (the left button newly down, the middle's too
  before 2026-11-10), on the stated reason that the double click's button was still down, so a grip held as C locked
  on or as the mode over the follow was popped did nothing until it was let go and taken again. The follow makes a new player's mode over itself, `CameraModeNew3`'s constructor
  (`CameraModeNew3.cpp:1214-1222`), which refuses only a current mode that cannot be left, starts from the camera's
  zoomers as they are (`Reinitialise(false)` touches no zoomer) and is pushed by `SwitchToViewMode` 0x441CD0 (the
  follow's `Cleanup` is the base one, which does nothing). The follow stays under it with its model and its view
  (`IsStillValid`, `CameraModeFollow.h:41`, is true while its thing is there and available), until the turn's check
  finds its thing gone or the new mode's pop gives it the view again; the keys' ask and the place off the fence are
  spent as it goes under (`CreatureCameraModel::GoneUnder`), as the original reads them afresh at each update of the
  current follow. Creature Mode's status panel shows the followed creature only while that follow is the current mode
  with the camera (`CreatureModeSystemInterface::GetCreatureInView`), as the original's follow sets the panel's values
  in its own update, run for the current mode only ([the panel](../creature/fights.md#the-creatures-status-panel-drawn-after-it));
  the temple's and the realm's keys (`PCInput.cpp:709-710`, `:739-740`, `GPlayer::ZoomToCitadel` 0x64A7A0, bw1-decomp
  `Player.cpp:771-803`) fly only a player's mode: with any other mode current they make a new one first
  (`CameraModeNew3`'s constructor, refused only by a current mode that cannot be left) and fly it; refused, the second
  try finds no player's mode and nothing flies. So since 2026-11-19 over the follow that is the current mode a new
  player's mode from the view drawn goes on top and every follow stays under it, as the cursor keys alone do (until
  then the keys took the newest follow off, and an older follow had the camera again), and since 2026-11-19 over a
  dual camera or a placed path current too, a follow under it keeping its model under the new mode (until then a
  follow under it was taken off, and with none the player's camera under the dual camera or the path was flown); over
  a script's camera mode still going or the creature interaction's camera the keys fly nothing (until then the
  player's camera under them was flown). `ShortcutKeys` makes the mode through `camera_checks::PutNewPlayersModeOver`,
  the keys' tap and way back started afresh as the new mode's `Reinitialise` clears them; tests
  `CreatureModeSystemTest.APlaceKeyPutsANewPlayersCameraOverTheFollowWhosePopGivesItBack`,
  `APlaceKeyOverTwoFollowsLeavesBothUnderTheNewPlayersCamera`, `APlaceKeyWithNoFollowMakesNoMode`,
  `APlaceKeyOverADualCameraOverTheFollowPutsANewPlayersCameraOverThemBoth`,
  `APlaceKeyOverADualCameraPutsANewPlayersModeOverIt`,
  `APlaceKeyOverAPlacedPathOverTheFollowPutsANewPlayersCameraOverThemBoth`,
  `APlaceKeyOverAPlacedPathPutsANewPlayersModeOverIt`, `APlaceKeyOverAScriptsCameraModeMakesNoneAndFliesNothing`
  (the helper as the keys call it; the keys' own update reads real keys only). The turn's check takes off every follow whose thing is gone, from the bottom, and what was under it is
  under the mode over it from then on (the player's camera given to the new player's mode over it, or to the newer
  follow over it); a full stack deletes the lowest follow the same way (`DeleteFollow`); the map's clear takes every
  follow off (`LeaveAll`). Inside the citadel (a script's change of land there, the one load that keeps the player inside: `land_load::TempleBeforeLoad`) the clear, `GCamera::ClearCameraStack`
  0x442FE0 (every mode deleted, then a new `CameraModeNew3`), leaves a new player's mode that the way out reinitialises:
  since 2026-11-20 the island's camera behind the temple, which has the current follow's model, has the model under it
  again at once and starts it again as that new mode (`CreatureModeSystemInterface::LeaveAllBehindTemple`, from
  `camera_checks::ClearStack`; the way out's `LeaveCitadel` then starts the player's mode again; test
  `CreatureModeSystemTest.AClearInsideTheTempleGivesTheIslandsCameraANewPlayersModeThatTheWayOutStartsAgain`); until
  then the follow, let go of, waited off the stack and handed the model on only at Creature Mode's next update after the
  way out (`ReleaseLeaving`), not started again. The temple (and so the Creature Cave, its creature room: F5 is
  `GoInsideCitadel(TEMPLE_ROOM_CREATURE)`, `PCInput.cpp:745-746`) leaves every follow and placed path on the stack
  since 2026-11-19, as the original's way in touches no camera mode (`GGame::GoInsideCitadel` 0x553E10: its
  callees, `Temple::InitEngine` 0x793C60 among them, leave GCamera's stack; the rooms draw through their own
  `InnerCamera`s, and the citadel's own camera mode, `CameraModeCitadel`, bw1-decomp `CameraModeCitadel.cpp:22-44`,
  has no maker in the exe: no caller of 0x44A1A0). Inside, the current mode's update still runs in the original
  (`GCamera::Update` 0x441F80 calls it every frame), but the world it reads stands still (single player is paused
  there, `GoInsideCitadel` 0x553F83 calling `PauseGame(1)` when not multiplayer, and a paused game gets no turn:
  `LocalTimerSaysDoATurn` 0x54C4A0 gives none at 0x54C528 but with the debug "as fast as possible" flag, [the turn's
  end](../audio/banks-and-channels.md); so no turn runs and no turn's `CheckStackedModesForValidity` 0x441D40) and no
  camera key reaches it (`GGame::ProcessMapKeys`, `PCInput.cpp:641`, skips the citadel): so in openblack the follows
  do nothing of their own while the temple is active (`CreatureModeSystem::Update`), and a placed path has not the
  camera there (`CameraPathSystem::HoldsCamera`, so the keys' take-back does not run). The path's own update
  (`CameraModePath::Update`, bw1-decomp `CameraModePath.cpp:49-55`: its animation's `UpdateCamera` on GCamera's zoomers)
  runs there as the current mode's: since 2026-11-20 openblack runs it on the island's camera behind the temple
  (`CameraPathSystemInterface::UpdateBehindTemple`, from `camera_checks::UpdateBehindTemple`, its time the animation's
  last drawn frame, which stands still with the paused world; test
  `BehindTempleStack.APlacedPathCurrentRunsItsUpdateOnTheIslandsCamera`), until then only the zoomers went on towards
  where it last sent them. The temple keeps
  the island's camera (`TempleInterior::GetWorldCamera`, its zoomers and view as they were going in) with the model the
  camera had, the follow's or the player's, and the frame's camera update moves it under the rooms' own camera
  (`camera_checks::UpdateBehindTemple`, since 2026-11-19, raffclar's 992bab7c9 refitted): the camera's model updates it
  when it is the current mode (`CameraModelIsCurrent`: the player's mode, a follow, the creature interaction's camera),
  with none of the player's controls (`SkipActions`, as the original's player mode has `InputDisabled` set inside, by
  `GGame::ProcessMapKeys` `PCInput.cpp:656-660`, and cleared by its `Reinitialise`, `CameraModeNew3.cpp:1578`), and its
  zoomers go on in every mode; the way out hands back the model and the zoomers as they are then, so a flight or a
  follow's ease under way going in has gone on (tests `BehindTempleStack.*`, `BehindTemple.*`). Until then the island's
  zoomers were kept aside whole and the model stood still inside. The way out
  (`GGame::LeaveInsideCitadel` 0x553A70) gives the model back and reinitialises only a current player's mode
  (`CameraModeNew3`, `Reinitialise(1)`, or `(0)` for a point of the map, after the zoomers are set to it); with another
  mode current it does nothing, but leaving for a point of the map it makes a new `CameraModeNew3` over it: openblack's
  `camera_checks::LeaveCitadel` (raffclar's `world_camera::OnLeave`, his 992bab7c9), from `TempleInterior::Deactivate`,
  over any current mode but a player's that can be left, through `camera_checks::PutNewPlayersModeOver` (the follow
  that is the current mode, a placed path, and since 2026-11-19 a dual camera, until then left current with no new
  mode, the cut to the point dropped as the dual camera drove on; the dual camera's zoomers are the camera's in the
  original, so openblack's take the cut too, and the new mode starts there; a follow under a placed path keeps its
  model under the new mode, where until then the camera kept the follow's model). A save inside writes the mode the player went in with
  (`CreatureModeSystemInterface::GetCurrentFollow`, and the script's mode). Tests
  `CreatureModeSystemTest.AFollowGoesOnThroughTheTemple`, `LeavingTheTempleForTheMapPutsANewPlayersModeOverTheFollow`,
  `AFollowUnderANewPlayersModeStaysUnderItThroughTheTemple`, `LeavingTheTempleForTheMapPutsANewPlayersModeOverADualCamera`,
  `CameraPathSystemTest.APlacedPathWaitsThroughTheTempleAndLeavingForTheMapPutsAPlayersModeOverIt`,
  `SavedCamera.InsideTheCitadelASaveWritesTheModeThePlayerWentInWith`. Until 2026-11-19 the temple took every follow
  off as Creature Mode's update found it active (`LeaveAll`), and its way out stopped every placed path
  (`CameraPathSystem::Stop`) and started whatever model the camera had again.
  A save writes the current mode only: a script's mode over a follow is saved as the script's
  mode, a follow under a new player's mode as the player's. Tests `CameraModeStackFollows.*`,
  `CreatureModeSystemTest.AScriptsCameraOverTheFollowLeavesItUnderAndThePopOfThePlayersModeAfterItsEndGivesItBack`,
  `AScriptModeOverTheFollowPoppedLeavesTheFollowCurrent`,
  `CLockingOnOverAnotherFollowLeavesItUnderAndCLettingGoPutsANewPlayersCameraOverIt`,
  `ASecondFollowWhoseCreatureGoesLeavesTheFirstTheView`,
  `AFollowDeletedFromUnderANewerOneLeavesThatOneThePlayersCameraUnderIt`,
  `CWithTheFollowUnderANewPlayersCameraLocksOnAfresh`,
  `TheCursorKeysAlonePutANewPlayersCameraOverTheFollowWhosePopGivesItBack`,
  `AGripOfTheLandHeldAcrossFramesPutsANewPlayersCameraOverTheFollowAtEachOfItsUpdates`,
  `TheDoubleClickThatLocksOnLeavesTheFollowWhileItsSecondPressIsHeld`,
  `CreatureModeGrip.OnlyTheGripLandscapeHandStatePutsThePlayersCameraOver`,
  `TheCursorKeysOverAFollowOverADualCameraLeaveTheDualCameraUnder`,
  `AFollowAGripWentOverIsTakenOffByTheTurnsChecksOnceItsCreatureGoes`,
  `AFollowLeavingTheFenceGoesUnderANewPlayersCameraWhosePopGivesItBack`.
- **The turn's validity check ✅ faithful** (2026-11-19), `GCamera::CheckStackedModesForValidity` 0x441D40: one pass
  over the three owners of the modes, `camera_checks::CheckModes` (`src/Camera/CameraChecks.{h,cpp}`, from the turn's
  `CameraChecks` step). The original's pass goes once from the bottom of the stack (0x441D57) and runs a deleted
  mode's `Cleanup` (vt 0x18) only when its index is the current one; deleting from the bottom, it meets the current
  mode last, so only the mode current as the pass began is ever cleaned up. openblack since 2026-11-21 freezes that
  mode for the pass (`ModeStack::BeginValidityPass` / `EndValidityPass`): every owner's removal answers "it was the
  current mode" for that one only (`ModeStack::WasCurrent`), where before a path under a dual camera that the pass
  deleted first counted as current and was cleaned up (test
  `CameraModeStackPushOrder.TheValidityPassCleansUpOnlyTheModeCurrentAsItBegan`). The order in which the owners
  delete the other modes cannot be seen: a mode that is not current only runs its destructor (vt 0), and none of the
  follow, the path and the dual camera defines one of its own (bw1-decomp `CameraModeFollow.cpp`,
  `CameraModePath.cpp`, `CameraModeTwoObjects.cpp`), the script mode's freeing only its own path (0x461240);
  openblack's removal of one touches only its owner's records, the zoomers being handed on only for the current one. It notes the current mode's kind (`CurrentTop`, the top of the push order: the script camera's
  own mode when it is current and drives, the creature interaction's camera or a new player's mode when current, else a
  placed path with no follow over it, else the follow that has the view, else the player's mode); then each owner deletes its modes that are no
  longer valid, cleaning up only its current one and restarting nothing: `script_camera::Validate` (the dual cameras;
  a driving one's cleanup hands the zoomers back to the camera, and the followed things gone are dropped),
  `CameraPathSystemInterface::RemoveInvalid` (the paths let go of) and `CreatureModeSystemInterface::RemoveInvalid`
  (a follow whose thing has gone; with the camera, its cleanup gives the camera's model back to the player's); then a
  stack left empty with the player's own mode gone makes a new player's mode (`script_camera::NewPlayersModeIfGone`);
  then, only if the current mode went, the new current one restarts once by its kind (`camera_checks::RestartAfter`,
  pure): a path its near plane (`CameraModePath` restart, which the frame's update overwrites before any draw, so
  nothing more here), the player's mode `CameraModel::Resume` with `PlayerCameraRestarted` (`Restart` for a new one,
  its seconds from 0), the follow, a dual camera, the script mode and the creature interaction's camera nothing. The
  mode's seconds run on. So a dual camera gone over a path leaves the path the camera with nothing restarted, a path
  gone over the follow restarts nothing, and two modes gone in one turn restart the final one once (tests
  `CameraChecks.*`, `CameraChecksPass.*`). A path or a follow over a dual camera taken off by the check leaves the dual
  camera current, going on from the camera's zoomers ([the push order](#openblack)).
- **The frame's camera update** (2026-11-19, behaviour identical): `Game.cpp`'s camera step dispatches from one
  `switch` on `camera_checks::CurrentFrameUpdate` (`FrameUpdateFor`, pure, tests `CameraChecks.*`): the script
  camera's update runs every frame and, when it drove the camera (its own mode current, pushed after the others), a
  follow under it stays as it is; else Creature Mode's update runs first (it takes the camera or gives it back), then
  a placed path that has the camera moves it, else the camera's own model (the player's mode, or the follow). It is
  not a switch on `CurrentTop`, which would differ: inside the citadel a script mode does not drive, the script
  stack's interaction camera or new player's mode can sit over a path that still has the camera, and the path's hold
  is read after Creature Mode's update. The Camera debug window's "The camera's stack of modes" (closed at first)
  shows the current mode's kind, the script camera's modes from the bottom (kind, dead), the paths', the follows' and
  the dance cameras' entries (each follow by its key, each dance camera by its dance and whether it has ended), and the
  places taken of the 12.
- **The dance camera** (2026-11-19, `CameraModeDance` from bw1-decomp whole;
  [the dance camera](../beings/dances.md#the-dance-camera-actions-18-19)): one of the other systems' entries of the
  stack (`ModeStack::PushDance`, `Kind::Dance`, keyed by its dance), owned by the camera checks. A dance's key frame
  makes it (`camera_checks::MakeDanceCamera`), refused while the current mode cannot be left; it goes on top of the
  whole stack, over a follow or a placed path too (`ModeStack::OverPath`: the path no longer has the camera). While it
  is current, outside the citadel as a path, the frame's update is its own (`FrameUpdate::Dance`,
  `camera_checks::UpdateDanceCamera`): no move of its own, the zoomers going on to where they were heading, and its
  ends (its dance gone or marked to go, the hand gripping the land, `useCamera` cleared, which clears the stack). The
  turn's check deletes it once ended (`ModeStack::RemoveInvalidDances`, `Went::dance`), the mode under it restarting
  by its kind; a full stack deletes it as any other entry; the map's clear takes it off (`ModeStack::ClearDances`). A
  new player's mode can go over it (`PutNewPlayersModeOver`). A save made while it is current writes no mode of its
  own, as the original's save number 0. Tests `CameraModeStackDances.*`, `CameraChecks.TheDanceCamera*`,
  `DanceCamera.*` (`test/test_script_camera_dual.cpp`).
  Checked against the original (2026-11-19): the zoomers going on is `GCamera::Update` 0x441F80, which calls the
  current mode's update (vt+8, 0x441FD9) and then updates the six zoomers, so a flight under way finishes under the
  dance camera; over a placed path it is the push order itself (the mode's maker pushes itself through
  `SwitchToViewMode` 0x441CD0, and only the top is current). Inside the citadel the original's update still runs every
  frame, but what the dance camera reads (its dance's flags) changes only in a turn, and no turn runs while the game
  is paused there (`GoInsideCitadel` 0x553F83, `LocalTimerSaysDoATurn` 0x54C4A0): so inside the island's camera
  behind the temple (`camera_checks::UpdateBehindTemple`) only goes on with its zoomers under it, as the original's.
- **Hand-over of the zoomers ✅ faithful:** GCamera has a single set of zoomers for all modes. openblack has the
  player's (`Camera::GetOriginZoomer/GetFocusZoomer`) and the script's: `Begin` copies the player's as they are (value,
  speed, destination and time: the script mode continues towards where the player's was going, 0x461180 does not touch
  them) and `End` returns the script's to the camera (`HandBack`), from where the player starts like
  `CameraModeNew3::Initialise` 0x456640. With the hooks `OPENBLACK_CAMERA_LOCK/FLY` the player never released the
  camera and nothing is copied.
- `CHLApi.cpp`: the opcodes of the table; START_CAMERA_CONTROL goes through `help::script_control::StartCameraControl`
  to `ScriptControlSystem::StartCameraControl`, which calls `script_camera::BeginFrom`; END_CAMERA_CONTROL and the task
  stop (`help::script_control::OnTaskStopped` from Game.cpp) release it through `ScriptControlSystem`, which calls
  `script_camera::End`. A land change calls `ClearModes` (the
  modes go, the lens stays); only a new game (the first land, a playground's start, as the original
  starts a playground only from a new game's start, and the testbed: `land_load::ResetsFor`) calls `Reset`, which also
  puts the field of view at 70 degrees at once (`ScriptCamera.ALandChangeEndsTheModesAndKeepsTheLens`,
  `LandLoadScripts.OnlyANewGameMakesTheCameraAgain`).
- **⚠️ approximate** While the script drives, the player camera carries what is drawn (with the nudge and the metre
  above the ground), not the script zoomers. Without script mode, 035 compares the player's zoomers with their
  destination (the same rule 0x441700). `SetPositionAndFocus` 0x4438C0: when the drawn camera's position and focus,
  g_camera +0x0..+0x14, each compare equal to the ones asked for (0x4438E6..0x443955, `fcomp` and C3, so a part
  that is not a number reads as equal), it only writes GCamera +0x74 = 0 and returns, leaving the position and focus
  zoomers as they are, a glide included; otherwise it sets each zoomer's value, start and destination to the point
  with its speeds 0 and writes +0x74 = 1 (0x443B27); what reads +0x74 is not read. openblack's
  `script_camera::SetPositionAndFocus` has the same way out, against the drawn camera (shake included,
  `script_camera::DrawnThereAlready`; test `ScriptCameraDetails.SettingTheCameraWhereItIsDrawnLeavesAGlideGoing`).
  The game ms of
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
- **The near plane ✅ faithful:** one near clip a frame, `DrawListNearClip` in `Game` (`near_clipping::NearPlane`
  through `draw_list::NearClipFor`), from the height of the camera drawn the frame before over the land read at
  ftol((x·65536)·0.1) (`map_coords::MetresToFixedForHandLookup`), with the close clipping of
  `CinematicDirectorSystem` (`SetCloseClipping`, cleared by `Game::ResetScripts` through
  `CinematicDirectorSystem::OnScriptsRestart` when the scripts restart, kept by a land change; test
  `CinematicDirector.TheScriptsRestartClearsTheCloseClippingAndTheFade`). The object draw list, the picking and the projection all take it; the projection is set whenever it
  changes. A camera with a lens of its own (the temple's) keeps its own. A placed path holding the camera does not
  bring it in, as the original's frame overwrites the path's 0.1 before any draw.
- **The lens** (`src/Camera/ScreenPoint.h`, `screen_point::`): `CameraPointFromScreen` (⚠️ approximate: the near
  cancels out, the original's rounding goes through it), `ProjectCamera` and `ToWorld`. Pure maths, shared by the
  falling spell (in its own camera space) and the advisor spirits (in the world through the drawn camera).
- **The drawn camera:** `ApplyShake` puts the debug camera (`SetDebugCameraMode`, `SetDebugCameraPosition` /
  `SetDebugCameraFocus`, kept in the script state's `DebugCameraState`) and the shake in the draw offset, and counts
  the shakes down once a drawn frame
  (`camera_shake::Tick` with the frame's real ms; **🔍 inferred** after the camera, as `ProcessGraphicsEngine` runs
  before the draw). The debug camera's writers are the intro's (`IntroSystem`, PLAY_JC_SPECIAL) and its `ReleaseAll`,
  which runs in `Game::ResetScripts` (a new game, a playground's start, the testbed): a land change keeps the debug
  camera, as the original's (
  [what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets)); the Land Island window's
  readout shows its mode.
- **The points from angles ✅ faithful:** `script_camera::PointFromDistanceHeadingAndPitch` and
  `HeadingAndPitchFromPoints` are GCamera's two helpers, step by step. `std::sin` / `std::cos` in double give
  `SetPointFromPointDistanceHeadingAndPitch`'s x87 results bit for bit: 20 000 oracle cases on 0x442810 (headings to
  ±30 rad, pitches to ±7, distances to 3000), 0 differences. Every point openblack's cameras build from angles
  goes through them; the list is on [the player camera](player-camera.md#openblack).
- **What is on the screen** (`src/Camera/FieldOfView.{h,cpp}`, `field_of_view::`): `PosInView`, `ThingInView` and
  `ObjectOnScreen`, through the drawn camera's `billboard::CameraFrame::lh.worldToClipping` and `nearZ`, as
  `ShadowMath::BlockVisible`; both tests false inside the citadel (`game_clock::IsInsideCitadel`). **⚠️ approximate** openblack's "Object" is an entity with a Mesh: its box centre through
  the Transform (`affine::BoxCentreThroughObject`, the original's order; the cells are openblack's model's) and the
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
    (`particles::forest_camera::UpdateFrame`), and lets go at frame 999 or when it goes.
  - `FollowAt(owner, pathMs, playing)` keeps the clip time and the playing flag, as `DrawAt` and the particle's turn
    write +0x28 and +0x3C. `Update` is the mode's update (the glide once, then the 0.3 s aim every update) followed by
    `Camera::UpdateZoomers`. The rules: `camera_path::PlacedGlideSeconds`, `PlacePoint`, `PathTimeFromFrame`,
    `ReachedLastFrame`, `TakesCameraBack`.
  - `Release(owner)` is `StopCamera`, wherever the owner's path is: it only marks the path no longer valid (fn_00441E00
    sets the mode's +0x10 = 0), and the path keeps the camera and follows on (`CameraModePath::Update` 0x460FC0 and
    `UpdateCamera` 0x67AA70 have no stop test) until the turn's camera checks (`RemoveInvalid`, in the turn's one
    pass, [the stack of modes](#openblack)) take it off; when the one with the camera goes, the path under it is current
    again, and with none the new current mode restarts by its kind: the player's camera (`CameraModeNew3::Restart`
    0x4587D0: `CameraModel::Resume`, a flight back to its view with a rise of 0.1, and `PlayerCameraRestarted`), the
    follow nothing, the creature interaction's or a script's camera nothing (2026-11-09, the rework raffclar's
    ad233f7c8 pointed at; 2026-11-19 the one pass; tests `CameraPathSystemTest.AReleasedPathFollowsOnUntilTheTurnsCameraChecksTakeItOff`,
    `CameraChecksPass.*`).
    Debug: the Camera window's placed path shows the released path until the turn takes it off. The
    take-back puts a new player's mode on top, one for a movement key's step and one more for the land grip in the
    same frame, as `ProcessKeyMovement` 0x460FE0 makes a `CameraModeNew3` for each (bw1-decomp
    `CameraModePath.cpp:81-87`): the paths stay placed under it, their releases taking them off at the turn without a
    restart, and have the camera again only if it is popped (`camera_stack::ModeStack::CoverPaths` /
    `UncoverPaths` on the paths' stack entries since 2026-11-19, `script_camera::PushPlayersMode`, `camera_path::PlayersModesOnTakeBack`, 2026-11-12;
    tests `CameraPathSystemTest.TheTakeBackPutsANewPlayersModeOverThePathsWhichComeBackWhenItIsPopped`,
    `AKeyAndAGripInOneFramePutTwoPlayersModesOnTop`, `ANewPlayersCameraOverThePathsKeepsThemUnderItUntilItIsPopped`).
    The new mode starts from the camera as it is, its constructor's `Reinitialise(0)` (no flight, no fight), and the
    player's camera under it keeps its own model and view (`CameraModel::KeptUnder`).
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
- `test_magic`: `ForestCameraRules.*` and `ForestCamera.*` (`test_particle_forest_camera.cpp`), the forest's camera
  particle that drives the placed path.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` win over the script camera (`Drives()`, not original).
- In game: Land 1, when the tutorial requester's answer does not skip the tutorial, runs FollowUs and
  CreaturesInGlade with the script camera.
- The debug bar's Camera window: the placed path's read-out and its Release button (above).
- **A saved game's camera** (`saved_camera::Load`, `src/ECS/SavedWorld/LoadCamera.cpp`): the record's focus, position
  and field of view zoomers go back into the camera and the script camera, every mode goes and the player's camera
  starts again from them, as `GCamera::Load` 0x4433A0 then `ClearCameraStack` do; a save writes them and the mode's
  seconds from the camera ([the camera comes back](../engine/saved-games.md#the-saves-in-play); tests `SavedCamera.*`).
  The debug bar's Camera window shows the restart in its restart count and the mode's seconds.

## Pending
- The one stack of camera modes (found differences, 2026-11-19): the push order is the original's since 2026-11-19 for
  the paths, the follows and the dual cameras ([the push order](#openblack)), and a script mode over a follow and
  several follows since 2026-11-19 ([the follows](#openblack)), a follow over a dual camera over a script mode since
  2026-11-19, and the full stack's cap for every push since 2026-11-19 ([the full stack](#openblack)), and C letting go
  of a follow with a dual camera or a path under it since 2026-11-19 ([the follows](#openblack)), and C over a driving
  dual camera since 2026-11-20 ([the follows](#openblack)).
  The temple keeps the camera's modes since 2026-11-19, and its island camera goes on behind it ([the follows](#openblack)),
  a placed path's own update too since 2026-11-20.
  Left there: inside, the clear view's keys (Ctrl and Shift) do not reach the player's mode behind
  the temple, where the original's `CameraModeNew3::Update` reads them outside its `InputDisabled` block
  (`CameraModeNew3.cpp:2402-2403`; not checked in play, and raffclar's 992bab7c9 keeps them). Read 2026-11-20: held,
  the keys' grab starts from the render hand's place and the object under the interface's pointer
  (`CameraModeNew3.cpp:2415-2421`), moves the render hand onto that object while it is there (`:2454-2460`) and eases
  the camera's destinations to a close view of it over half a second (`:2469`, `:3529-3532`), the way out's
  `Reinitialise` putting the grab back at once (`:1572`); what the render hand and the pointer's object are inside the
  citadel, where the rooms have the hand, is not read, so it is not ported. Whether the frame that goes in still reads
  the controls (the original sets `InputDisabled` at the end of `ProcessMapKeys`) is not read either. A save's follow
  loaded while the player is inside is not made again (`CreatureModeSystem::Enter` refuses while the temple is active),
  where the original's `GCamera::Load` 0x4433A0 makes it on the stack after its clear (`ClearCameraStack` 0x442FE0)
  (not checked in play); read 2026-11-20: every saved-game load in openblack takes the player out of the temple first
  (`land_load::TempleBeforeLoad`, only a script's change of land keeps them inside), so openblack never loads a follow
  inside; whether the original's save room or quick load reaches `GCamera::Load` with the player still inside is not
  read (`LeaveInsideCitadel` 0x553A70's eight callers, the rooms' updates and keys among them, are not read for a
  load), so it is not ported.
  The land grip reaches the follow from openblack's interface hand state, which the hand's update takes from the
  grip its last placement started or ended, and which Creature Mode reads before the next hand update: a grab's
  release and the double click's second down in one frame still read Grip Landscape at the new follow's first
  update, which then goes under a new player's mode at once. Whether the original's hand state is already past the
  grip there (its follow reads it in `ProcessMapKeys`, before that frame's `CMouse::ProcessButtons` and the
  interface's pass, `Game.cpp:1538-1552`) is not yet read; the hand state's timing is the hand's.
