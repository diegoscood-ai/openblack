# Script camera (GCamera and CameraModeScript)

How the script moves the camera in runblack.exe v1.42 (W120) and what openblack has: the GCamera zoomers, the script
camera mode, the arrival rule, the CHL camera opcodes, the FOV and the hand-over when releasing control.
The `camera.edt` tracks are in [camera-tracks.md](camera-tracks.md); the START/END_CAMERA_CONTROL locks, in
[audio.md](audio.md) (`Help/ScriptControl`).

- [GCamera and its zoomers](#gcamera-and-its-zoomers)
- [The script mode](#the-script-mode)
- [Arrival rule](#arrival-rule)
- [One frame of GCamera::Update](#one-frame-of-gcameraupdate)
- [Following](#following)
- [Dual camera](#dual-camera)
- [Shake](#shake)
- [Zones and fixed rotation](#zones-and-fixed-rotation)
- [Opcodes](#opcodes)
- [Releasing the camera](#releasing-the-camera)
- [openblack](#openblack)
- [Pending](#pending) · [Test hooks](#test-hooks) · [Sources](#sources)

## GCamera and its zoomers

**Faithful.** GCamera (0x1D8 bytes) stores the camera in LH3DLib Zoomers (`SetDestinationWithSpeedAndTime` 0x407D60,
`Update` 0x442720; [engine-math.md](engine-math.md#zoomer-lh3dlib), `src/Common/Zoomer.h`): the focus at +0x88/+0xB8/+0xE8,
the position at +0x118/+0x148/+0x178 and the FOV (radians) at +0x1A8. A MOVE starts from the current value and speed and
arrives with speed 0 at T; T < 0.001 places it immediately; when T ends the value is pinned to the destination. The mode
stack is at +0x28 (12) with the index at +0x58; the player uses `CameraModeNew3` (vtable 0x8C7BFC), which always allows
exiting.

## The script mode

**Faithful.** START_CAMERA_CONTROL (0x6ECCA0), outside the citadel, calls `fn_00461140`: if `CantExitCurrentMode`
0x441B70 there is no mode and the control fails; otherwise, `CameraModeScript` (ctor 0x461180, vtable 0x8C7D5C, inherits
from `CameraModeFollow`). While it lives, `CanExit` 0x461B70 = 0: **no other task takes the camera**. It does not touch
the zoomers. `SetCameraPosition/Focus` 0x461370/0x4612B0 set them and `MoveCameraPosition/Focus` 0x4616F0/0x461430 move
them (with final speed 0); all four first release the track and the following. `RunPath` fn_00461A80 loads `Track%d` and
`UpdatePath` 0x461AB0 advances with the game ms of the frame: position = `CameraWayRunner::Get`, focus = Bezier of the
focus way in the segment and the t of the **position** runner, and `SetPositionAndFocus` 0x4438C0 sets them.

## Arrival rule

**Faithful.** `GCamera::Arrived` 0x443050: with no mode, 1; otherwise, vt+0x34 of the mode. `CameraModeScript::Arrived`
0x461B40: with a track, duration ≤ ms travelled; without a track, `CameraMode::Arrived` 0x441700: |position − destination|²
< 0.001 **and** |focus − destination|² < 0.001 ([0x8AA3B0]). `CameraModeNew3` also uses 0x441700 (its vtable +0x34). It
does not look at the FOV.

## One frame of GCamera::Update

**Faithful** (0x441F80, from `ProcessGraphicsEngine` 0x54D879, per frame):
1. dt = `GetCameraTimeInc` 0x555820 · 0.001 = wall-clock ms (`g_delta_time`), capped at 0.1 s [0x8AB22C].
2. `Update` of the mode (vt+0x08; Script: `UpdatePath` and the following `CameraModeFollow::Update` 0x44C160).
3. The 6 zoomers with dt; NaN → the last good one (0x4420D9).
4. Position and focus almost equal (< 0.001) → position x − 1, y + 1, only in what is drawn (0x4421D5).
5. World disc: if the position's **destination** is more than 3500 from (2560, 0, 2560), new destination
   d / (|d| · 0.000285796) + centre in 3 s (0x44222C).
6. Citadel: what is drawn does not move (0x442337). Otherwise, what is drawn stays 1 m above the ground, also raising the
   focus (0x44242B).
7. FOV: its zoomer with `g_game_time_inc` · 0.001 (**game time**, not wall-clock) and `LH3DTech::ChangeFov` 0x8195B0.

## Following

**Faithful** except where marked (`CameraModeFollow`, from which the script mode inherits; full reading in
`dev\documentacion\camara\step2.md`).
- Fields: +0x08 thing followed by the position, +0x4C (Script) thing followed by the focus (`GetFocusThing` 0x4611F0 =
  +0x4C or, if null, +0x08), +0x0C heading, +0x10 pitch, +0x14 distance, +0x18 time factor (0.2 in Script), +0x1C
  «behind» (1 in Script).
- `Set(cosa)` 0x44BA00: heading and pitch of the zoomers' **destinations** (`GetHeadingAndPitchFromPoints` 0x4428D0),
  distance = height · 8 (`GetThingViewingDistance` 0x441F20); with «behind», heading 0. `fn_0044BA90(cosa, d)`: the same
  with distance d and without setting the heading to 0. `fn_0044BB30` places immediately (Zoomer::SetPosition) and sets
  GCamera+0x68 = 2.
- `CameraModeFollow::Update` 0x44C160, per frame: distance clamped to 2..1500 and stored; T = (+0x68 > 2 ? 1 :
  2 − +0x68 / 2) · factor (0x44C1A5: 0.4 s after the mode change, 0.2 s after 2 s; factor 0 → place); focus towards the
  thing's point (MapCoords: x, z / 6553.6, y = ground + altitude +0x1C; in `Update` the Game3DObject's translation if
  it has one; plus half the height; flock: `Flock::GetFlockPos` 0x530570 with the leader's half height); position =
  `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from that point with the distance, pitch ≥ 0.241661 (stored) and
  the heading, which with «behind» on a MobileWallHug is heading − (object's angle − π/2) (0x44C785).
- `Validate` (0x461270 + 0x44BB10, **once per turn** from `GGame::ProcessTurn` 0x54E74E): releases the thing that is no
  longer there.
- Set/Move of a point release the following on its side (0x461370 `Set(0)`, 0x4612B0 `SetCameraFocus(0)`);
  `RunPath` releases only +0x4C.
- Face of an object (`fn_006ED710`, 106/107): focus = MapCoords point + half height; position at distance d with the
  heading `GetFacingDirection` (vt+0x4EC; normalised to ≤ 2π) and pitch 0.1.
- FollowUs: `SET_FOCUS_AND_POSITION_FOLLOW(Son, 3)` and `CAMERA_PROPERTIES(3, 0, 22.5, true)`: the camera sticks to the
  child, 22.5° relative to where it is facing.

## Dual camera

**Faithful** except where marked (`CameraModeTwoObjects`, 0x30 bytes, vtable 0x8C7DD0, «Dual Cam»; full reading in
`dev\documentacion\camara\step3.md`). It is stacked on top of the script mode (ctor 0x461BB0; with a point fn_00461CB0; one
equal to the current one deletes itself).
- `Update` 0x461DE0, per frame: T = (+0x68 > 1.5 ? 1 : 2 − +0x68 / 1.5), **without factor**; A and B = MapCoords of the
  two things (or the point); focus = midpoint raised by the mean height · 0.5; distance = ((separation in x/z + the two
  `Get2DRadius`, 30 if it is not an Object) · factor (1, or 1.2 with a point) + larger height · 1.4; heading = π/4 − the
  direction of B − A; pitch π/8; the zoomers towards those destinations in T.
- 093 START (0x6ED2E0), 094 UPDATE (0x6ED370, `SetObjects` 0x461C90), 095 RELEASE (0x6ED410: `Delete` and `PopViewMode`,
  GCamera+0x68 = 0), 105 WITH POINT (0x6ED460, without checks). The end of control (fn_006ECD70) removes a dual
  before deleting the script mode. With the dual on top, the script opcodes find «the wrong mode».
- Per turn, `CheckStackedModesForValidity` 0x441D40 removes the dual whose things are no longer there (`IsStillValid`
  0x461D90).
- openblack: `script_camera::State::duals`, a layer on top of the script mode (no mode stack: **(approximate)**); the
  dual takes the player's zoomers as `BeginFrom` and, if it goes away with no script underneath, gives them back.

## Shake

**Faithful.** SHAKE_CAMERA 201 (0x6EE0F0) → `PSysGlobal::StartCameraShake` 0x68F400 → `LH3DCameraChecker::Create` 0x821050
(radius, point, amplitude, ms). It is applied by fn_008210C0 from `LH3DTech::UpdateCamera` 0x819920, **only to what is
drawn** (never to the zoomers): the shake closest to the camera, if it is within its radius (no falloff with distance);
amplitude = remaining / total · amplitude; six `Random` 0x81D180 rolls (pos.z, pos.y, pos.x, focus.z, focus.y, focus.x) or
two with «only y». Each drawn frame (fn_00821270) subtracts `g_delta_time` and it is freed on reaching 0.
openblack: `src/Camera/CameraShake.{h,cpp}` (`camera_shake::`, with `graphics::lh3d::Random`) and
`script_camera::ApplyShake`, every frame from `Game.cpp` with the camera being drawn (the script's or the
player's), as a draw-only offset of `Camera::SetDrawOffset` (from sistemas): the zoomers do not shake.

## Zones and fixed rotation

- SET_CAMERA_ZONE 142 (0x6ED890): `ResetExclusionFile(1)` 0x455320 and `LoadExclusionFile` 0x455370 of
  `.\Data\Zones\%s` (segment «cameraexc»: flags, two limits of 500, n force-field points, exclusions),
  force field switched on. **Faithful** the loader and `InsideInclusion` 0x455E20 (`src/Camera/PlayerCameraScript.{h,cpp}`,
  `player_camera::`); the nine `.exc` in `Data\Zones` are read correctly. **Pending:** what the player camera does with it
  (`CameraModeNew3::Update` 0x45F982: repositioning, shake, pulse, drawing the field, influence 0x5CD32F).
  GET_INCLUSION_DISTANCE 150 (0x6ED990) therefore always gives FLT_MAX **(approximate)**.
- SET_FIXED_CAM_ROTATION 209 (0x6EE1A0): only with the player mode; `ForceRotateAboutPoint` 0x457330 stores the point
  (`player_camera::Get().fixedRotation`). **Pending:** that `DefaultWorldCameraModel` rotates around it (0x45AB00,
  0x460135). No map uses it.

## Opcodes

**Faithful** except where marked. Those that move check the mode: with no mode «Script camera has been removed!»; another
mode «We are in the wrong camera mode!» and they do nothing.

| CHL | Original | What it does |
|---|---|---|
| 001 / 002 SET_CAMERA_POSITION / FOCUS | 0x6EC8F0 / 0x6EC9A0 | sets (without script mode it does nothing) |
| 003 / 004 MOVE_CAMERA_POSITION / FOCUS | 0x6ECAA0 / 0x6ECBA0 | moves in t s of wall-clock time |
| 035 HAS_CAMERA_ARRIVED | 0x6ED170 | arrival rule |
| 119 RUN_CAMERA_PATH | 0x6ED7F0 | camera.edt track |
| 279 SET_CAMERA_LENS | 0x6EE2E0 | **`SetCameraFov(70°, x)`: the argument is the time** (copied) |
| 280 MOVE_CAMERA_LENS | 0x6EE280 | FOV = lens · 0.0174533 in t s of game time |
| 283 / 284 STORE / RESTORE_CAMERA_DETAILS | 0x6EE330 / 0x6EE390 | stores what is drawn / `SetPositionAndFocus` |
| 286 / 287 SET / MOVE_CAMERA_POS_FOC_LENS | 0x6EE3C0 / 0x6EE4B0 | position, focus and FOV, **the lens not converted to radians** (copied; no map uses them) |
| 314 / 315 GET_STORED_CAMERA_POSITION / FOCUS | 0x6EE630 / 0x6EE6A0 | what was stored |
| 377 GET_FACING_CAMERA_POSITION | 0x6EE710 | position + d · forward vector (inferred: unit vector towards the focus) |
| 049 / 276 FOCUS_FOLLOW / SET_FOCUS_FOLLOW | 0x6EDF30 / 0x6EDB40 | the focus follows the thing (0x4619B0) |
| 050 POSITION_FOLLOW | 0x6EDE70 | the position follows the thing (`Set` 0x44BA00) |
| 277 SET_POSITION_FOLLOW | 0x6EDA80 | `Set` + place immediately (fn_0044BB30) |
| 178 / 278 (SET_)FOCUS_AND_POSITION_FOLLOW | 0x6EDDA0 / 0x6ED9B0 | `fn_0044BA90(cosa, d)` (278 also places immediately) |
| 180 CAMERA_PROPERTIES | 0x6EDFF0 | distance, time factor, heading (° · 0.0174533), «behind» |
| 106 / 107 SET / MOVE_CAMERA_TO_FACE_OBJECT | 0x6ED500 / 0x6ED600 | face of an object, set / move in t |
| 203 SET_AVI_SEQUENCE | 0x6FC050 | (approximate) without video: only removes the fade to black (`SetupScreenFadeBackToNormal(0)` 0x6EBB00), as if the video ended instantly |

| 093 / 094 / 095 START / UPDATE / RELEASE_DUAL_CAMERA | 0x6ED2E0 / 0x6ED370 / 0x6ED410 | dual camera |
| 105 CREATE_DUAL_CAMERA_WITH_POINT | 0x6ED460 | dual camera with a point |
| 201 SHAKE_CAMERA | 0x6EE0F0 | shake (only what is drawn) |
| 142 SET_CAMERA_ZONE / 150 GET_INCLUSION_DISTANCE | 0x6ED890 / 0x6ED990 | player camera zone (loaded; its effect, pending) |
| 209 SET_FIXED_CAM_ROTATION | 0x6EE1A0 | player's fixed rotation point (stored; its effect, pending) |

Not ported: the PC player following 372/373 (0x6EDC00 / 0x6EDCD0).

## Releasing the camera

**Faithful.** `fn_006ECD70` (END_CAMERA_CONTROL 0x6ECEF0 and the task stop fn_006ECF20): removes the dual camera; if the
mode is the script one it deletes it and creates a `CameraModeNew3`, which starts from the current zoomers (no jump); the
FOV **always** returns to 70° in 0.5 s; then the script state (`Help/ScriptControl`).

## openblack

- `src/Camera/ScriptCamera.{h,cpp}` (`script_camera::`): the position, focus and FOV zoomers, the script mode
  (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame` (steps 1-5 and 7) and
  `DrawnCamera` (steps 3-6). `UpdateCamera` does it every frame from `Game.cpp` and, while the script mode
  drives, the player model (`DefaultWorldCameraModel`) neither moves the camera nor reads keys (Script has no keys,
  0x44C3BD). Positions and foci go in `Zoomer3d` (`Common/Zoomer.h`, the same one as the player camera).
- **Hand-over of the zoomers (faithful):** GCamera has a single set of zoomers for all modes. openblack has the
  player's (`Camera::GetOriginZoomer/GetFocusZoomer`) and the script's: `Begin` copies the player's as they are (value,
  speed, destination and time: the script mode continues towards where the player's was going, 0x461180 does not touch
  them) and `End` returns the script's to the camera (`HandBack`), from where the player starts like
  `CameraModeNew3::Initialise` 0x456640. With «free start» or the hooks `OPENBLACK_CAMERA_LOCK/FLY` the player never
  released the camera and nothing is copied.
- `CHLApi.cpp`: the opcodes of the table; `StartCameraControl` passes `cameraTaken = script_camera::Begin(...)`;
  END_CAMERA_CONTROL and the task stop (Game.cpp) call `script_camera::End`. When loading a map, `Reset`.
- **(approximate)** While the script drives, the player camera carries what is drawn (with the nudge and the metre above
  the ground), not the script zoomers. Without script mode, 035 compares the player's zoomers with their destination (the
  same rule 0x441700). `SetPositionAndFocus` does not have the early exit of 0x4438C0. The game ms of the frame are those
  of the previous frame's clock (the original updates the camera after the turns).
- **(inferred)** 284/286 without script mode also set the player camera.
- The FOV goes to `config.cameraXFov` (degrees) only when its zoomer changes: a player's own FOV lasts until a
  script touches the lens.
- Mod `game.skip-intro`, «free start» (not original): the opening task does create the script mode (no other one
  takes the camera while it has it), but `Drives()` is false and it moves nothing; its camera opcodes do nothing
  ([mod-library.md](mod-library.md#gameskip-intro)).

## Pending

- 372/373 (following the PC player's hand, `GComputerPlayer::GetHandPos` 0x657FE0).
- SET_AVI_SEQUENCE 203 with video: `PlayFullScreenMovie("data\intro.bik")` 0x54D920 pauses the game and returns it at
  58 s; without Bink in openblack there is no movie nor pause (approximate). Sequence 2 (video of the spell falling).
- (approximate) The creature's angle (LH3DCreature +0x84) does not exist: no heading adjustment and `GetFacingDirection` 0.
  The GameAngle of a villager comes from `WallHug::yAngle` rounded to 2048 per revolution.
- Player camera: shake, zone (repositioning, force field) and fixed rotation (plans in step3.md §B-D).
- (approximate) No mode stack: the dual always goes on top of the script; a dual over the player mode is
  approximated.
- `CameraModeNew3::Reinitialise` 0x4589B0 (how the player takes the camera back), not read.

## Test hooks

- `test_script_camera`: a single mode, arrival, 0.1 s cap, placing with T < 0.001, disc, nudge and ground, FOV with
  game time and the return to 70° in 0.5 s; `ScriptCameraFollow.*`: T rule, distance and pitch, point from
  distance/heading/pitch, heading and pitch between points, «behind», place immediately, face of an object, things that
  disappear.
- `test_script_camera_dual`: the dual (pace, focus, distance, heading, with point, validity, end of control), the shake
  (radius, decay, rolls), the zone loader and `InsideInclusion`.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` win over the script camera (`Drives()`, not original).
- In game: Land 1 without the mod `game.skip-intro` (`--mod game.skip-intro=off`) runs FollowUs and CreaturesInGlade
  with the script camera.

## Sources

- `dev\documentacion\camara\original.md` (step 1) and `dev\documentacion\camara\step2.md` (following, face of an object,
  SET_AVI_SEQUENCE), `dev\documentacion\camara\step3.md` (dual camera, shake, zones, fixed rotation), with their audits
  `audit_step1.md` / `audit_step2.md` / `audit_step3.md` in the same folder.
