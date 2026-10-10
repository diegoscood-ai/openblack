# Script camera (GCamera and CameraModeScript)

How the script moves the camera in runblack.exe v1.42 (W120) and what openblack has: the GCamera zoomers, the script
camera mode, the arrival rule, the CHL camera opcodes, the FOV and the hand-over when releasing control.
The `camera.edt` tracks are in [camera-tracks.md](camera-tracks.md); the START/END_CAMERA_CONTROL locks, in
[audio.md](audio.md) (`Help/ScriptControl`).

Progress: [Script camera](../progress/camera/script_camera.md) (what our tree does of it, row by row).

- [GCamera and its zoomers](#gcamera-and-its-zoomers)
- [The script mode](#the-script-mode)
- [Arrival rule](#arrival-rule)
- [One frame of GCamera::Update](#one-frame-of-gcameraupdate)
  - [The lens](#the-lens)
- [Following](#following)
- [Dual camera](#dual-camera)
- [Shake](#shake)
- [Zones and fixed rotation](#zones-and-fixed-rotation)
  - [Player camera features (CameraHelp)](#player-camera-features-camerahelp)
- [Opcodes](#opcodes)
- [Releasing the camera](#releasing-the-camera)
- [Placed paths (CameraModePath)](#placed-paths-cameramodepath)
- [openblack](#openblack)
- [Test hooks](#test-hooks)
- [Pending](#pending)

## GCamera and its zoomers

**Faithful.** GCamera (0x1D8 bytes) stores the camera in LH3DLib Zoomers (`SetDestinationWithSpeedAndTime` 0x407D60,
`Update` 0x442720; [engine-math.md](engine-math.md#zoomer-lh3dlib), `src/Common/Zoomer.h`): the focus at +0x88/+0xB8/+0xE8,
the position at +0x118/+0x148/+0x178 and the FOV (radians) at +0x1A8. A MOVE starts from the current value and speed and
arrives with speed 0 at T; T < 0.001 places it immediately; when T ends the value is pinned to the destination. The mode
stack is at +0x28 (12 modes, 0x441CEA) with the index at +0x58; the player uses `CameraModeNew3` (vtable 0x8C7BFC), which
always allows exiting.

- The GCamera ctor sets the FOV to 70 degrees at once (0x441A78..0x441A83): `SetCameraFov(fn_00443670, 0)`, where
  fn_00443670 = fld [0x8C762C] = 1.2217305.
- GCamera +0x68 = seconds since the last mode change: 0 in `SwitchToViewMode` 0x441CD0, plus the frame's camera seconds
  at 0x441FCD (before the mode's Update), 2 after fn_0044BB30 (0x44C141).
- The script opcodes find the current mode with `__RTDynamicCast` to 0x9CE188 (CameraModeScript); the dual opcodes use
  0x9CE790 (CameraModeTwoObjects; 0x6ED3F0 / 0x6ED43D).
- `GCamera::SetCameraFov` 0x443680: with t == 0 or t < 0.001 it sets the value; otherwise it moves towards the FOV with
  final speed 0 (an inline copy of 0x407D60, 0x4436CB..0x4437D7).
- Degrees to radians is [0x92B20C] = 0.0174532924 (SET_CAMERA_LENS, MOVE_CAMERA_LENS, CAMERA_PROPERTIES 0x6EE025).

## The script mode

**Faithful.** START_CAMERA_CONTROL (0x6ECCA0), outside the citadel, calls `fn_00461140`: if `CantExitCurrentMode`
0x441B70 there is no mode and the control fails; otherwise, `CameraModeScript` (ctor 0x461180, vtable 0x8C7D5C, inherits
from `CameraModeFollow`). While it lives, `CanExit` 0x461B70 = 0: **no other task takes the camera**. It does not touch
the zoomers. `SetCameraPosition/Focus` 0x461370/0x4612B0 set them and `MoveCameraPosition/Focus` 0x4616F0/0x461430 move
them (with final speed 0); all four first release the track and the following. `RunPath` fn_00461A80 loads `Track%d` and
`UpdatePath` 0x461AB0 advances with the game ms of the frame: position = `CameraWayRunner::Get`, focus = Bezier of the
focus way in the segment and the t of the **position** runner, and `SetPositionAndFocus` 0x4438C0 sets them.

- `fn_00461140` is called from START_CAMERA_CONTROL at 0x6ECCBA; the citadel never has a script mode (StartCameraControl
  0x6ECD33).
- CameraModeScript is a CameraModeFollow built with no thing (ctor 0x44B800): time factor 0.2 (0x461189), «behind» 1
  (0x461185..0x461193), heading 0 (from `Set(0)`, 0x44BA7D), and mode seconds from 0 (`SwitchToViewMode` from 0x44B947).
- (inferred) The ctor 0x44B800 calls `Set(0)`, which writes neither pitch nor distance; they are never read before Set,
  fn_0044BA90 or CAMERA_PROPERTIES write them.
- CameraModeScript +0x48 = alive: 1 from the ctor 0x461180, 0 from `Delete` 0x4611E0. `CanExit` 0x461B70 = (+0x48 == 0).
  `IsStillValid` 0x4611D0 = +0x48.
- `CameraModeScript::Update` 0x461290: `UpdatePath`, then `CameraModeFollow::Update` 0x44C160 (call at 0x4612A1).
- `SetCameraPosition` 0x461370 calls `Set(0)` (0x46137E) and sets +0x54 = −1. `SetCameraFocus` 0x4612B0 calls
  `SetCameraFocus(0)` 0x4619B0 (0x4612BE) and sets +0x50 = −1. `MoveCameraPosition` releases at 0x461702 and
  `MoveCameraFocus` at 0x461442.
- Dropping the track is fn_00461A60: it frees the ScriptedCamera (fn_00446AC0) and sets +0x58 = 0.
- `RunPath` fn_00461A80 first calls `Reset` 0x461A30: +0x4C = 0, +0x48 = 1, the old ScriptedCamera freed, +0x50 = +0x54 =
  −1, +0x08 kept. Then it starts track `number` from the beginning.

## Arrival rule

**Faithful.** `GCamera::Arrived` 0x443050: with no mode, 1; otherwise, vt+0x34 of the mode. `CameraModeScript::Arrived`
0x461B40: with a track, duration ≤ ms travelled; without a track, `CameraMode::Arrived` 0x441700: |position − destination|²
< 0.001 **and** |focus − destination|² < 0.001 ([0x8AA3B0]). `CameraModeNew3` also uses 0x441700 (its vtable +0x34). It
does not look at the FOV.

## One frame of GCamera::Update

**Faithful** (0x441F80, from `ProcessGraphicsEngine` 0x54D879, per frame):
1. dt = `GetCameraTimeInc` 0x555820 · 0.001 [0x8AC418] = wall-clock ms (`g_delta_time`), capped at 0.1 s [0x8AB22C].
2. `Update` of the mode (vt+0x08; Script: `UpdatePath` and the following `CameraModeFollow::Update` 0x44C160).
3. The 6 zoomers with dt; NaN → the last good one (0x4420D9).
4. Position and focus almost equal (< 0.001) → position x − 1, y + 1, only in what is drawn (0x4421D5).
5. World disc: if the position's **destination** is more than 3500 from (2560, 0, 2560), new destination
   d / (|d| · 0.000285796) + centre in 3 s (0x44222C).
6. Citadel: what is drawn does not move (0x442337). Otherwise, what is drawn stays 1 m above the ground, also raising the
   focus (0x44242B).
7. FOV: its zoomer with `g_game_time_inc` · 0.001 (**game time**, not wall-clock) and `LH3DTech::ChangeFov` 0x8195B0.

- The world disc constants: centre [0x8C7618] = 2560, squared limit [0x8C7614] = 1.225e7 (3500²), scale [0x8C7610] =
  0.000285796, time 3 s (0x40400000).
- **(approximate)** The world disc in openblack (`Camera::UpdateZoomers`) is skipped while the camera's model has a lens
  of its own (`CameraModel::GetLens`), which only the temple's has: our temple interior sits at the origin, about 3620
  from the centre, outside the disc. The original clamps in the citadel too (0x44222C comes before the citadel test at
  0x442337), so its interior is not where ours is; where the original puts it has not been read. Test:
  `CameraSphere.ACameraModelWithALensOfItsOwnStaysOffTheWorldDisc` (test_temple_camera).
- The 1 m ground clearance is [0x8AA390] (0x44242B..0x4424AB). It is measured under the nudged position. It is skipped
  in CameraModeFree and when GCamera+0x78 is set.
- NaN in the zoomers: the last good drawn camera is kept in the statics 0xC59B48 (position) and 0xC59B38 (focus), first
  (1000, 0, 1000) (0x4420D9..0x4421D5).
- The FOV goes to `ChangeFov` at 0x4425C3, and not inside the citadel (0x4424F6).
- `LH3DTech::UpdateCamera` 0x819920 is called from `GCamera::Update` 0x442622, outside the citadel and not during a
  playback.

### The lens

- `ChangeFov` 0x8195B0 / `UpdateViewPort` 0x81909C: T = tan(fov / 2) (0x8195B8..0x8195D7), fx [0xE83A00] = 1 / T, fy
  [0xE83A04] = aspect / T, with aspect [0xE839EC] = W / H.
- `Get3DPointFromScreen` 0x81B370 works with the depth along the camera's forward axis, not with a ray length. It uses
  [0xC3812C] = near·T and [0xC38130] = near·T / aspect.
- In camera space: ((x − hW)·near·T / hW, (hH − y)·near·T / aspect / hH) × depth / near, z = depth. Then the camera's
  rotation and g_camera take it to the world: eye + right·x + up·y + forward·z (0x81B3BE..0x81B43A).
- Projection of `LH3DSprite::Draw` (0x840930..0x8409D0) / `ProjectPoint` 0x819390, without clipping: sx = (X / Z + 1)·hW,
  sy = hH − Y / Z·hH, with X = fx·x and Y = fy·y.

## Following

**Faithful** except where marked (`CameraModeFollow`, from which the script mode inherits).
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
- Constants: distance kept in 2 ([0x8C785C]) .. 1500 ([0x8C78E4]): ≤ 2 → 2, < 1500 → itself, else 1500
  (0x44C16B..0x44C1A2). Pitch at least [0x8C78E0] = 0.241661 (0x44C758..0x44C778, 0x44BF7E), stored back.
- T constants: the threshold 2 s [0x8C7874], 1 [0x8C7870], 2 [0x8C786C]; "≤ 2" takes the slow start (0x44C1A5..0x44C1E1).
- `GetThingViewingDistance` 0x441F20 = GetHeight (vt +0x42C) × 8 ([0x8C7108]).
- With «behind» on a MobileWallHug the heading correction is GameAngle (+0x5C) × 2 × [0x8C78DC] (π/2048) − [0x8C78D8]
  (π/2) (0x44BFD1 / 0x44C7B1, 0x44C775..0x44C7C1). CAMERA_PROPERTIES writes the script mode at 0x6EE0C1..0x6EE0D9;
  «behind» is tested `!= 0` at 0x44C7E6.
- The thing's point: MapCoords +0x14, x and z × 1/6553.6 ([0x8AA3A4]), y = `LH3DIsland::GetAltitude` 0x803090 + the
  altitude above the land +0x1C (0x44BCD2..0x44BD03).
- In `CameraModeFollow::Update`, an Object with a Game3DObject (+0x40) uses that matrix's position (Game3DObject +0x38)
  instead (0x44C513..0x44C539, 0x44C715..0x44C73B), but fn_0044BB30 does not. For villagers and animals that matrix is
  the one drawn between turns (fn_0051AF00).
- A creature as the focus thing writes 0xCC62E4..0xCC6310 (1 − life, its body's +0x1C / +0x30) in 0x44C1E5..0x44C245,
  read elsewhere. Not ported.
- Without a position thing, `Update` follows the computer player: `GetComputerPlayerFocus` (vt +0x4C) != −1
  (0x44C59C..) and `GetComputerPlayerFollow` (vt +0x50) != −1 (0x44C905..). fn_0044BB30 has the same branches
  (0x44BD52.., 0x44C056..). Not ported.
- fn_0044BB30 only runs with +0x20 == 0 (0x44BB46). It places with `Zoomer::SetPosition` 0x441AC0 (and an inline copy
  for x) and does not clamp the distance (0x44BF73..0x44C13C).
- `SetCameraFocus(thing)` 0x4619B0 first drops the track (0x4619B4). Then +0x4C = the thing if `IsAvailable` (vt +0x2C)
  is 1, else 0 (0x4619BD..0x4619DC).
- `GameThing::IsAvailable` 0x401810 = !(byte +0xA & 1). Only `SetCameraFocus` (0x4619BD) and `Validate` (0x461273 /
  0x44BB13) read it here.
- `GetFacingDirection` (vt +0x4EC): GameThingWithPos 0x4024B0 = 0; MobileWallHug 0x60C020 =
  ConvertGameAngleToScawenAngle(+0x5C); Creature 0x477EC0 = its LH3DCreature's angle (+0x160 → +0x58 → +0x84) + 2π − 2.5.
- `GetHeadingAndPitchFromPoints` 0x4428D0(a, b): v = a − b. If |v.x| and |v.z| are both below [0x8C7620] = 0.01 (a
  double), heading 0 and pitch 1.5393804 (0x3FC50A6B, 0x442925 / 0x44292B).
- Otherwise heading = π ([0x8C36A0]) − fn_007FAA50(v), as a 24-bit `fsubr` (0x44293F). fn_007FAA50 gives 0 when x² + z²
  ≤ 1e-6, else fn_007FA990(−z, x). Pitch = fn_007FA990(sqrt(x² + z²), y) (0x442950..0x442976).
- `SetPointFromPointDistanceHeadingAndPitch(out, p, d, h, q)` 0x442810 (0x442811..0x442856): `fcos` of q stored as a
  float [esp] (0x44281B); then on the stack z = `fcos`(h) × [esp] × d + p.z, y = `fsin`(q) × d + p.y, x = `fsin`(h) ×
  [esp] × d + p.x, each a product or sum in that order, and the three `fstp` to out at the end (0x442851..0x442856).
  - **Precision.** The game thread's FPU runs at 24 bits (see [audio.md](audio.md#the-games-fpu-runs-at-24-bits)):
    `fsin` / `fcos` are not touched by the precision control and give the full 64-bit sine; each `fmul` / `fadd` rounds
    the mantissa to 24 bits (the exponent stays extended, which changes nothing at the camera's sizes), and the last
    `fstp` is then exact. So only cos q is rounded to a float before a product; sin q, sin h and cos h are not:
    x = float(float(float(sin h × float(cos q)) × d) + p.x), y = float(float(sin q × d) + p.y),
    z = float(float(float(cos h × float(cos q)) × d) + p.z).
  - **Evidence.** In exact arithmetic (the sine and cosine to 64 bits, each step rounded to 24), the double click's
    flight (DoubleClickFlyTo: p = (1080.70996, 0, 966.307434), d = 100, h = 0, q = 0.53133231, 0x3F080565) gives
    (1080.70996, 50.6682434, 1052.52075) = (0x448716B8, 0x424AAC48, 0x448390AA), the recorded flyToTargetOrigin to the
    bit in all three axes. With sin q rounded to a float first (glm's `euclidean`), the height is 50.6682396, one float
    step low; x and z are the same either way there (sin 0 and cos 0 are exact). Done in double (the sine and cosine
    in double, each product with one of them rounded to a float, the rest in float), the result is the exact model's
    on 100 000 random points, distances, headings and pitches in all three axes; with the sine and cosine rounded to
    floats first, 27 370 of them differ.
  - The callers (every `call 0x442810`): `GCamera::GCamera` (0x4418EC, 0x4419C0), `GetHeadingAndPitchFromPoints`
    0x442A03, CameraModeCitadel vt +4 0x44A400, CameraModeFlyAndClick vt +4 0x44AFF0, fn_0044B2C0 0x44B2E0,
    fn_0044BB30 (0x44BFFC, 0x44C0A2), `CameraModeFollow::Update` (0x44C7DB, 0x44C8BA, 0x44C9A7),
    `CameraModeFollowHeading::Update` 0x44DA9F, `CameraModeFree::Update` 0x44E65F, fn_004576C0 0x45772D,
    `ZoomToCitadel` (0x457D69, 0x457EB7), fn_00458E50 0x458F33, `FindBestAngle` (0x458FB1, 0x45908A),
    `CameraModeNew3::Update` (the clear view 0x45B27A and 0x45B601, the double click 0x45E29C, the fight 0x45E89C,
    mouse mode 2 0x45E9D1, mouse mode 1 0x45EF0E, the fixed rotation 0x46019F), `CameraModeTwoObjects::Update`
    0x46213E (the dual), `MoveCameraToFaceObject` 0x6ED7D9, `Town::SetTownArea` 0x73AC6D, `AdjustBubbleZForPitch`
    0x78A10E.
  - Mouse mode 2 (a camera input without a grip, 0x45E8FB..0x45E9D1) and mouse mode 1 (0x45ED92..0x45EF0E) build the
    frame's origin [esp+0x14] from +0x12C, the distance [esp+0x4C], the heading [esp+0xE0] and the pitch kept within
    −π/6 (0xBF060A92 at or below −0.523599 [0x8C7CDC]) and [esp+0xE8].
- Face of an object (fn_006ED710): the focus is the MapCoords point plus half the height, with no flock and no
  Game3DObject (0x6ED728..0x6ED76F).
- Face heading (0x6ED774..0x6ED7D9): a heading ≥ 8π ([0x942190]) logs "Invalid heading" (0xC0C208) and carries on.
  While it is > 2π ([0x8AB210]), 2π is subtracted. Pitch 0.1 (0x6ED7D0).
- With no object, fn_006ED710 logs "no object to face" (0xC0C218) and then reads through the null thing (inferred: it
  would crash) (0x6ED717..0x6ED725).

## Dual camera

**Faithful** except where marked (`CameraModeTwoObjects`, 0x30 bytes, vtable 0x8C7DD0, «Dual Cam»). It is stacked on top of the script mode (ctor 0x461BB0; with a point fn_00461CB0; one
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
- Constants (ctor 0x461BB0 / fn_00461CB0): heading +0x20 = π/4 (0x3F490FDB), pitch +0x24 = π/8 (0x3EC90FDB), distance
  factor +0x28 = 1 (0x461C2C) with two things or 1.2 (0x3F99999A, 0x461D62) with a point. Debug name string at 0x461C60.
- Fields: +0x08 thing A (0x461BC8), +0x0C thing B (0x461BCB), +0x10 the point (0x461CCC..0x461CDF), +0x1C two things (1)
  / point (0) (0x461BCE / 0x461CE2), +0x2C alive (1 at 0x461C25..0x461C41).
- T constants: 2 ([0x8C7DBC]) right after the mode change, down to 1 ([0x8C7DC0]) at 1.5 s ([0x8C7DC4])
  (0x461DED..0x461E26).
- A and B are MapCoords points (x, z × 1/6553.6, GetAltitude 0x803090 + the altitude +0x1C), not the Game3DObject nor a
  flock's GetFlockPos. With a point, B has no +0x1C (0x461E2A..0x461EB2).
- The midpoint is (B + A) × 0.5 ([0x8AA3B4], 0x461EB6..0x461F10). With a point, the second height in the mean is 1
  (0x461F2A).
- The pitch is kept ≥ [0x8C78E0] and stored back (0x462003..0x462021).
- The radius of a thing that is not an Object is 30 (0x46204A, [0x8BF51C]). An Object's comes from `Get2DRadius` (vt
  +0x64) via `dynamic_cast<Object*>` (0x462031 / 0x462067). The larger height is multiplied by 1.4 ([0x8C7E18],
  0x4620B1).
- Heading = +0x20 − the direction of v = B − A (fn_007FAA50, a 24-bit `fsubr`, 0x46211C). It stays +0x20 when |v.x| and
  |v.z| are both ≤ 0.01 ([0x8C7A10], a double; 0x4620E9 / 0x4620FC).
- The position heads for `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from the focus (0x462126..0x462318).
- START_DUAL_CAMERA checks neither the camera mode nor the citadel. START / UPDATE_DUAL_CAMERA pop b then a; either
  missing → "Thing invalid for dual cam" (0xC0C1EC) and nothing.
- `IsStillValid` 0x461D90 = +0x2C while +0x08 (and +0x0C with two things) is still there and `IsAvailable` is 1.
  Otherwise `CheckStackedModesForValidity` deletes it (vt+0 with 1, 0x441D86), walking from the bottom of the stack
  (0x441D57). It is called from `GGame::ProcessTurn` 0x54E743.
- When the current mode is deleted, the new current mode gets Restart (vt +0x10, 0x441DEB). Restart does nothing for
  CameraModeScript (0x44A390) and CameraModeTwoObjects. The mode seconds are not reset there.
- RELEASE: `CameraModeTwoObjects::Delete` 0x461C50 (vt +0x30, called at 0x6ED44D) sets +0x2C = 0. `GCamera::PopViewMode`
  0x441C50 calls Cleanup (vt +0x18, nothing), deletes the mode, lowers the index and sets +0x68 = 0 (0x441C86).
- The end of control (fn_006ECD70) removes only one dual camera (`ReleaseDualCamera` 0x6ED410 at 0x6ECDB1). It deletes
  the script mode only if that mode is current (0x6ECDD0..0x6ECE2E). With another mode current (a second dual camera,
  or the player's), it logs "We are in the wrong camera mode! - excep" (0xC0C14C) and the script mode stays.
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
player's), as a draw-only offset of `Camera::SetDrawOffset`: the zoomers do not shake.

- `LH3DCameraChecker` is 0x24 bytes: `LH3DMem::Alloc(0x24)` zeroed, then put at the head of the list g_first [0xEB99A8]
  (0x82106D..0x821078), newest first.
- Checker fields: +0x04 max distance (0x82107E), +0x08 point (0x821085..0x821095), +0x14 amplitude (0x8210A0), +0x18
  total ms (0x8210A7), +0x1C remaining ms (0x8210AA), +0x20 «only y» (0x8210AD).
- `StartCameraShake` 0x68F400: ms = seconds × 1000 ([0x8AB228]) stored as a float, then `fistp` (0x68F406..0x68F41E). It
  always passes «only y» = 0 (0x68F426).
- fn_008210C0 (called from `UpdateCamera` at 0x819A0B) does nothing with no shakes or with [0xC383B8] == 0 (0x8210CB /
  0x8210D3). That flag is 1 in .data and never written.
- The nearest shake is measured from g_camera ([0xEA1DB8] / [0xEA1DC4]), the camera drawn the frame before. A later
  shake wins only if strictly nearer, so the newest wins ties (0x8210E0..0x821195). The distance is fn_004C2B90
  (subtract) and fn_004A1BA0 = sqrt((z·z + y·y) + x·x) (0x4A1BA8..0x4A1BB8).
- a = remaining / total × amplitude (0x8211A9..0x8211B7). A shake of 0 ms divides 0 by 0 (`fidiv` 0x8211B1) and draws a
  NaN camera until it is freed.
- Roll order: «only y» rolls position y then focus y (0x8211D1..0x8211F0). Otherwise six rolls: position z, y, x, then
  focus z, y, x (0x8211F7..0x821264).
- fn_008210C0 is the real "AdjustCameraPosTarget". bw1-decomp's symbol 0x437E70 with that name is another function.
- The countdown fn_00821270 runs from `LH3DRender::StartFrame` 0x82F270 (sub g_delta_time, `jg` keeps it,
  0x821287..0x8212CE).
- Other shake creators, not ported: `CameraModeNew3::Update` 0x45FC84 (the force field: 100, 1.0, 400 ms),
  `AddSoundToAtom` 0x69DDBD and fn_006E63C0 0x6E6453.
- Debug camera g_camera_mode [0xEA9EC8], read by `UpdateCamera` at 0x81997A before the shake (0x81997A..0x8199DD): 2
  draws the camera from [0xEA1B58] towards [0xEA1B68], 1 only turns the focus to [0xEA1B68], any other value does
  neither. GCamera's zoomers are not touched.
- Writers of [0xEA9EC8]: PLAY_JC_SPECIAL 1 / 2, `Intro::ReleaseAll` 0x5DFCCA and `CleanGameForScriptReboot` 0x6EBC0C (0).
  The GCameraEditor's writers are debug only.

## Zones and fixed rotation

- SET_CAMERA_ZONE 142 (0x6ED890): `ResetExclusionFile(1)` 0x455320 and `LoadExclusionFile` 0x455370 of
  `.\Data\Zones\%s` (segment «cameraexc»: flags, two limits of 500, n force-field points, exclusions),
  force field switched on. **Faithful** the loader and `InsideInclusion` 0x455E20 (`src/Camera/PlayerCameraScript.{h,cpp}`,
  `player_camera::`); the nine `.exc` in `Data\Zones` are read correctly. **Pending:** what the player camera does with it
  (`CameraModeNew3::Update` 0x45F982: repositioning, shake, pulse, drawing the field, influence 0x5CD32F).
  GET_INCLUSION_DISTANCE 150 (0x6ED990) therefore always gives FLT_MAX **(approximate)**.
- SET_FIXED_CAM_ROTATION 209 (0x6EE1A0): only with the player mode; `ForceRotateAboutPoint` 0x457330 stores the point
  (`player_camera::Get().fixedRotation`). **Pending:** that `DefaultWorldCameraModel` rotates around it (0x45AB00,
  0x460135). No map uses it. Without CameraModeNew3 it logs "Wrong camera mode" (0xC0C29C).
- `ResetExclusionFile(id)` 0x455320: `RemoveByID` 0x454A40 (the CameraExclusions with +8 == id); [0x9CE6B0] = 1
  (0x45532F); DrawForceField [0xC5E144] = 0 (0x455339); [0xC5E14C] = [0xC5E148] = 0 (0x45533E / 0x455343); [0x9CE6AC] =
  [0x9CE6A8] = 500 (0x455348 / 0x455352); ForceFieldPointCount = 0 (0x45535C).
- SET_CAMERA_ZONE path ".\Data\Zones\%s" (0xC0C248, 0x6ED8F7), opened with `LHReleasedFile::Open` 0x7BD730.
- After `LoadExclusionFile` (0x6ED92A), SET_CAMERA_ZONE sets [0x9CE6B0] = 1 and DrawForceField = 1 whatever the file
  says (0x6ED936..0x6ED940).
- A zone file that does not open logs "Couldn't load zone file-%s" (0xC0C22C) and the zone stays reset
  (0x6ED956..0x6ED96F).
- `LoadExclusionFile` 0x455370 opens segment "cameraexc" (0x9CE6F4, `LHFile::OpenSegment` 0x455395).
- Read order (0x45539A..0x455466): int32 header, [0x9CE6B0], DrawForceField, [0xC5E14C], [0xC5E148], floats [0x9CE6AC]
  and [0x9CE6A8], the point count and the points (12 bytes each), the record count and the record size.
- The force-field points go to `ForceFieldPoints` [0xC5B130]..ForceFieldPointCount [0xC5E130]: room for 1024 (0x3000 /
  12). Each is an inclusion polygon point (x, z) with a height.
- Records of size 0x28 (0x45546F) become CameraExclusions of that id (fn_00454960, +0 and +8 kept; list [0xC5E160];
  0x4554CF..0x455510). Any other size is read into a buffer and thrown away (0x45547C..0x4554B9).
- Readers of the zone flags (meaning not read): [0x9CE6B0] by `CameraModeNew3::GetAltitude` 0x459C7B; [0xC5E14C],
  [0x9CE6AC], [0xC5E148], [0x9CE6A8] by `CameraModeNew3::Update` 0x45C28C / 0x45C29B / 0x45C2B5 / 0x45C2BC.
- The inclusion distance [0xC5E13C] is FLT_MAX ([0x8C7BB0]) at start (0x4548D0) and in `ResetCameraModeNew3` 0x460B2E.
  `CameraModeNew3::Update` writes 1e10 (0x45FCE1) while there is no force field.
- `InsideInclusion` 0x455E20 only tests when DrawForceField is on (0x455E29..0x455E3F) and the polygon has at least 3
  points (0x455E5E).
- It counts ray crossings from the last point round the polygon: odd = inside (0x45624C..0x456252). Epsilons [0x8C7BD8]
  = 1e-8 (squared distances) and [0x8C79D8] = 1e-4 (a double). The best ray parameters start at 1e20 (0x455EA2 /
  0x455EAF). The nearest polygon point in x/z is the output (0x4561CB..0x45624A).
- `ForceRotateAboutPoint` 0x457330: +0x88 = 1 and +0x7C = the point (0x457338..0x45734F), or +0x88 = 0 for null. `Update`
  copies +0x7C over +0x12C (0x45AB00, 0x460135). `Reinitialise` 0x458B22 (and 0x458D23, 0x45E8D6) clears it.

### Player camera features (CameraHelp)

- `CameraHelp`'s EnabledFeatures [0x9CDD6C]: initial value 0x1BF, also set by ClearMap and
  SET_INTERFACE_INTERACTION(NORMAL). Auto-pitch parameters [0x9CDD64] = π/6 and [0x9CDD68] = 75.
- `CameraHelp::EnableCameraFeatures` 0x447430: features = (features & ~mask) | new. Every SET_INTERFACE_INTERACTION level
  passes mask −1, so the whole set is replaced (auto-pitch included).
- Auto-pitch fn_004473F0(p1, p2, on): EnableCameraFeatures(on ? 0x40 : 0, on ? 0 : 0x40), then stores p1 and p2.
- Writers: SET_INTERFACE_INTERACTION 0x70B220, `GScript::CleanGameForScriptReboot` 0x6EBCAE and `GGame::ClearMap`.
- Readers: `HandStateCamera::Update` (bits 0x1, 0x2, 0x4), `HandStateHolding::Update` (0x1, 0x2),
  `GInterface::CalculateCanSelectLock` (0x1), fn_005D6980 (0x4), fn_005C9D00 (0x2) and
  `CameraModeNew3::GetCameraFeatures` 0x456260 (vt +0x58).
- What `CameraModeNew3::Update` does with the features: without Zoom the zoom input is 0 (0x45C0E4..0x45C0F1); without
  Rotate it skips rotation (0x45C6CC); without Pitch it skips pitch (0x45C809). The bits as it reads them:
  - 0x01 Pitch; 0x02 Rotate; 0x04 Zoom;
  - 0x08 the land grab and strafe (the pan 0x45D3A6, the keyboard move 0x45C943..0x45C948);
  - 0x10 the double-click flight, and the flight after a land grip too far ahead (0x45DDA3..0x45DDA8);
  - 0x20 the middle button's turn round the mouse and both buttons (0x45ADC8);
  - 0x40 auto pitch;
  - 0x80 adds tricon display bit 8 when GInterface +0x48 is 0 (`UpdateTricons` 0x459557), and while the inputs are
    dropped after a given-up grip (0x4595F3..0x4595FE).
- The bit names (Zoom, Rotate, Pitch...) are inferred from the SCRIPT_INTERFACE_LEVEL names that set them (consistent
  over the 16 levels at 0x70B7A8).
- Player zoom scale (0x45C104..0x45C1B5): 3 × the height difference between the camera and focus destinations (GCamera
  +0x118 / +0x88 y), at least 60 ([0x9CE65C]), at most 240 ([0x8AB418] × 60) when the camera is below the focus
  (0x45C108..0x45C111) and 2000 ([0x9CE660]) when above.
- Zoom delta = input × 0.0015 ([0x8C7CF8], 0x45C0D5) × scale.
- The player's zoom, rotate and pitch of the frame go to `CameraHelp::CameraHelpCallback` 0x449140
  (0x45C38E..0x45C3AD / 0x45C6A8..0x45C838): help events 25..29 ([intro.md](intro.md#timers-help-events-and-field-of-view)).
- The double-click flight reports at 0x45DE85, or 0x306 DoubleClickObject at 0x45DE1E when it was on an object.
- The input mask the camera help calls get (0x45C06A..0x45C0C5, names at 0x9CDDF0): 0x01 "keyboard" (any camera input
  but the middle button), 0x02 "mouse button" (+0x8C & 1, the land drag), 0x04 "both mouse buttons", 0x08 "wheel spin",
  0x10 "wheel down" (the middle button held).
- The Rotate (0x300) event compares the turn input [0xC5B0C8] itself: |input| > 0.01, then CW if > 0 and CCW if < 0
  (0x45C71F..0x45C7A4). The edge-rotate drag sends 0x300 only with [0xC5E168], then 0x301 for an angle above 0.01 and
  **0x302 for an angle below 0.01** whatever [0xC5E168] is (0x45D2C7..0x45D346). That last one is a bug in the original:
  every still frame of an edge drag counts as RotateCCW.
- **Auto pitch (0x45BEE4..0x45C02B, 0x45F10B..0x45F22E):**
  - With feature 0x40 and mouse mode 0 or 2 (nothing gripped), the pitch of origin → +0x12C
    (`GetHeadingAndPitchFromPoints`, positive with the camera above) gives tilt = (AutoPitchParam1 [0x9CDD64] − pitch)
    × 0.2 [0x8AB244], stored in the tilt input [0xC5B0C4]. It counts only above 0.01 (the double 0x8C7A10): clamped to
    ±dt (the camera seconds) and × −150 [0x8C7CFC], it becomes the tilt input, the mode becomes 2 and the auto-tilt
    flag (esp+0x87 / bl) is set. Smaller tilts set the tilt input to 0 (0x45C021), so the player cannot tilt while auto
    pitch is on and the land is not gripped.
  - In an auto-tilt frame `UpdateTricons` runs with +0x8C = 0, so the idle tricons follow the cursor (0x45C02D).
  - It is applied without the Pitch feature, and never sends Pitch 0x303 (0x45C80C..0x45C81C, `test bl,bl; jne`). The
    confirmation feed fn_00454930 is still called. Rotate needs the player's turn input and Zoom the zoom input, so auto
    pitch alone never reaches the help events 25..29; [0xC5E168] is worked out before it, from the player's inputs.
  - The height: in frames with mouse mode 0 or an auto tilt, the camera goes to (start.x, altitude + AutoPitchParam2
    [0x9CDD68], start.z), where start is GCamera +0x118's destination at the frame's start (0x45AABB); the focus keeps
    its offset from it and +0x12C becomes the new focus (0x45F19D..0x45F22E). Not in modes 1 and 3, nor in mode 2 from
    the player's input while the tilt is under 0.01.
  - The defaults are π/6 and 75; SET_INTERFACE_INTERACTION's three auto levels (1, 2 and 10) call fn_004473F0(0.448799,
    15, 1) (0x70B2FA, 0x70B358, 0x70B5B0). In the challenge script they are in `MoveAroundMountain` (LandControlT) and
    `FollowUs` (Land 1, not run with the tutorial skipped).
  - The moved camera is what ProcessSpecialTriggers (44 LookAtLand, 45 TooClose, 46 LookAtSky), the field-of-view tests
    011 / 012 and the camera getters read.
- GConfirmation feeds: fn_00454900 (0x45C7D6) and fn_00454930 (0x45C8B1) get the frame's turn and tilt over the frame's
  seconds, 0 on frames without them (0x46053F / 0x46055C).
- `CameraModeNew3::FindBestAngle` 0x459144 uses `LH3DIsland::GetNormal` 0x803630 and the point + normal (0x459149).
- **The double click's flight, its view (0x45E08B..0x45E305):** the point is the double click's ([esp+0x24], after the
  inclusion test). d1 = |point − the screen-centre hit +0xF0| (the focus [esp+0x3C] instead when ebx is 0, which no
  path reaching here leaves, (inferred) `StartFight` keeping ebx as callees do) and d2 = |point − the frame's origin
  [esp+0x14]|, both in 3D.
  - The distance: d2 > 100 [0x9CE640] × 1.5 [0x8AB24C] and d1 > 10 [0x8AB414], both strict, give the 100 flight, +0x78
    = 0 and the woosh flag [esp+0x23] = 1. Otherwise 60 [0x9CE644] when 60 × 1.5 < d2, else 30 [0x9CE648] when 30 × 1.5
    < d2, else 15 [0x9CE64C], with +0x78 = 1.5 × 0.5 and no woosh. After a given-up grip ([esp+0x4B]) the distance is
    1000, +0x78 = 0.01, the woosh flag 1, and bl = 0 (0x45E1D5..0x45E1F1). The distance is used as it is: no shaping.
  - The heading and pitch: `GetHeadingAndPitchFromPoints(the frame's origin [esp+0x14], its focus [esp+0x3C])`
    (0x45E20A). With bl = 1, which every flight but the given-up one has (0x45E0C5), heading = `FindBestAngle(heading,
    the distance, point, &pitch, null)` (0x45E22F): the same call for the 100, 60, 30 and 15 flights, with no score
    pointer. Then the pitch: ≤ π/8 [0x8C6CA0] (or unordered) → π/8; else not below 1.496 [0x8C7C9C] → 1.496
    (0x3FBF7CD1, 0x45E23B..0x45E27F). After `FindBestAngle`, which keeps it within π/8 .. π/3, this changes nothing.
  - The focus becomes the point and the origin `SetPointFromPointDistanceHeadingAndPitch(point, distance, heading,
    pitch)` (0x45E29C). With +0x78 == 0 (the 100 flight only) `SetupVia(origin, focus, [esp+0x110], 0.1)` follows
    (0x45E2DB), then the woosh when its flag is set.

### Player camera: the land grip

- **Mouse modes (+0x8C, `MouseButtons`, 0x45BDE9..0x45C00E):** 0 idle, 1 land drag, 2 camera move, 3 both. Mode 1 needs
  the hand state of the last turn (GInterface +0x3AC) to be 20 Grip Landscape, or 27 with the left button. A grip with
  only a keyboard move stays 1 and the move is dropped. A change of mode calls `UpdateClickParams` and sets HandStatus
  +0x1AC = 0. `UpdateTricons` runs once a frame at 0x45C01A, after the mode and before the drags.
- **`UpdateClickParams` 0x459610:** +0x128 the tick, +0x1B0 the normalised cursor, +0x1B8 = 0, +0x2D4 the cursor, +0xEC
  the grip's depth, +0x12C the focus, +0x148 HandHit, the grip plane +0x14C / +0x158, the grip camera +0x160 / +0x16C /
  +0x178, +0x1A8 the distance, +0x1C0 the average distance.
- **The grip:**
  - The grab point +0xCC is the land under the cursor, or the focus with none (0x45BFDD).
  - Its depth +0xEC is `CalcPerpDistance` 0x458DB0(origin, focus, grab) = |(grab − origin) · unit(focus − origin)|. In
    each frame with the Zoom feature it becomes max(+0xEC + zoom, 3.1), as +0x1C0 does (0x45CAE7..0x45CB51).
  - The plane (0x4596F5..0x459A85): ground = (origin.x, min(alt(origin.xz), grab.y), origin.z); t = unit(grab − ground);
    a = unit(t.z, 0, −t.x); n = unit(t × a); d = n · grab (a double, +0x158).
  - With no edge tricon at the press (tricons & 3 == 0), HandStatus = 3, the pan, at once (0x45CC47 → 0x45CEDC).
- **Giving up (0x45D36D..0x45D399), before the pan's own tests:** a depth above 3000 [0x8C7CC0] sets +0x90 = 0x10,
  [esp+0x4B] = 1 and +0x2F0 = 1, and skips the pan.
  - +0x2F0 drops everything: in each frame with the grip [0xC5B0F0], the right button [0xC5B0B4], the middle button
    [0xC5B0EC], both buttons [0xC5B0B0] or any of the inputs [0xC5B0C4..0xC5B0D4], all of them are zeroed; the first
    frame with none clears it (0x45ACEE..0x45ADB9).
  - +0x90 = 0x10 makes `UpdateTricons` blink display bit 8, the zoom icon, every 200 ms ((tick / 200) & 1, 0x45959C).
    **(inferred, from the order of the calls)** It is not seen: from the next frame the grip is dropped, so with +0x8C
    = 0 `UpdateTricons` rewrites +0x90 from the cursor (0x459276), and while +0x2F0 is set the display is 0, or bit 8
    alone with feature 0x80 (0x4595CB..0x4595FE).
  - [esp+0x4B] runs the double-click flight's body the same frame (0x45DD92), with feature 0x10 (0x45DDA3). The point
    is the one under the hand: the object at GInterface +0x3C8, reported with CameraHelpCallback(0x306, inputs 0)
    (0x45DE1E), or else fn_00800C30's point, reported with CameraHelpCallback(0x305, inputs 0) (0x45DE85); no point
    ends the body with no report (0x45DE78). These are the double click's own reports, once in the frame the body runs.
    It passes the exclusion and 5120 disc tests, and no fight is looked for (0x45DFC5). At 0x45E1D5 it
    takes the distance 1000, +0x78 = 0.01 and the woosh flag [esp+0x23] = 1, and skips `FindBestAngle` (bl = 0). The
    heading and pitch are the camera's (`GetHeadingAndPitchFromPoints` 0x45E20A), the pitch kept within π/8 .. 1.496
    (0x45E23B..0x45E27F). With +0x78 not 0 there is no flight over a midpoint (vt +0x54 with 0.1 only for +0x78 == 0,
    0x45E2A1). The woosh is 46 + (tick & 3) (0x45E2E6..0x45E305).
- **The pan (0x45D39E), which needs feature 8, HandHit +0x144 now and +0x148 at the grip:**
  - g_camera is swapped to the grip camera +0x160 / +0x178 for two `Get3DPointFromScreen` calls (the cursor now and
    +0x2D4), then restored.
  - The rays r = p − cam. Each needs n · r beyond ±0.001 (doubles 0x8C7CB8 / 0x8C7CB0) on the same side, and
    t = (d − n · cam) / (n · r) > 0.0001 for both, in doubles.
  - moved = r_now·t_now − r_grip·t_grip, clamped to max(√(pixels²) × +0x1A8 × 0.011, 50) (0x45C40C..0x45C469,
    0x45D6CF..0x45D74F).
  - 0x308 (Drag, 33) goes to CameraHelpCallback with [0xC5E168] (0x45D668..0x45D687), in every frame of the pan that
    passes the two plane tests, before the step clamp and the stop; its inputs are the frame's input mask [esp+0xB8].
    [0xC5E168] (0x45BE94..0x45BEDD) is any camera input [0xC5B0E8], or the grip [0xC5B0F0], the middle button
    [0xC5B0EC] or both buttons [0xC5B0B0] with |MouseDelta +0xA8| > 2 or |+0xAC| > 2. +0xA8 / +0xAC is
    `ControlMap::MouseDelta`, the mouse's move since the last camera update, cleared as it is read (0x45AC79..0x45AC91).
    [0xC5B0E8] (0x45B85C..0x45B90E) is the zoom [0xC5B0CC], the keyboard moves [0xC5B0D0] / [0xC5B0D4], the turn
    [0xC5B0C8] or the tilt [0xC5B0C4] not 0, or the middle button. With the grip and only a keyboard move it is cleared
    with the move (0x45BE1B..0x45BE31). Without feature 0x20 the middle button and both buttons are cleared before
    (0x45ADC3..0x45ADD9).
  - When |pitch| > π/4, +0x1A8 is scaled by exp(−2·dy/h·k), clamped to [0.970874, 1.13] and ±200·dt; but
    k = max(π/4 − |pitch|, 0) = 0, so it never changes (0x45D753..0x45D8D9).
  - focus = focus₀ − moved; origin = focus + unit((origin₀ − moved) − focus) × +0x1A8.
- **Stopping short (0x45DA4A..0x45DC24):**
  - `LH3DIsland::RayCast` fn_00802550 from the grip camera through the new origin. It casts a ray to the map's edge,
    not the segment ([miracles.md](miracles.md)); without land, the y = 0 crossing of a line going down counts within
    7500 across from g_camera, the drawn camera, already restored. It gives only x and z.
  - With a hit and the move's x–z length above 3: stop = 3 / |m.xz|; share = toLand.x / m.x if |m.x| > |m.z|, else
    toLand.z / m.z, bailing out when that component is ≤ 1e-4; share −= stop.
  - Only 0 < share < 1 counts: origin and focus come back by m·(1 − share), and 0x200 is sent with [0xC5E168].
- **The cursor:** the pan never warps it.

### Player camera: the mouse

- **Tricons (`UpdateTricons` 0x459230, only while +0x8C == 0; +0x90 = 0x80 idle):**
  - The normalised cursor (0x45B916..0x45BA1C): +0x98 x = cx / w − 0.5, +0xA0 = |x|; +0x9C y = (cy − h / 2) / viewH,
    viewH = h − ftol(h − 0.5625 w) with the cinema bars (g_game +0x25005C → +0x45E8). The drag's sums +0x1B8 += dx /
    viewH and +0x1BC += dy / w go across by the height and down by the width.
  - |x| > 0.45 [0x9CE690] → 0x81; y > 0.43 [0x9CE68C] → | 1; y > c → | 2 Pitch; y < −c, or no land under the cursor
    (+0x144 == 0) and y < −0.4 [0x8C7C70], → | 7 Rotate | Pitch | Top; Rotate → | 0x40.
  - **c = 0.49 [0x8C7C74], or 0.45 [0x8C7C78] when [0xE850B4] ≠ 0 (0x459280..0x459297).** [0xE850B4] is LHScreen
    +0x64, `windowed` (the screen at 0xE85050, bw1-decomp `LHScreen.h`): with it the game sizes its window with
    `AdjustWindowRect(WS_OVERLAPPEDWINDOW)` / `MoveWindow` (0x7DC9A0) and reads the cursor through `GetCursorPos` /
    `ScreenToClient` (0x7DBEB0). The same flag keeps the virtual cursor +0xC0 / +0xC4 from running past the screen's
    edge (0x45AEAB..0x45AF27).
  - In a fight (+0x44) Pitch is cleared. In mode 1 only 0x80 is cleared (and 0x40 set with Rotate); in modes 2 and 3
    +0x90 is kept.
- **The drag classifier (0x45CC3A..0x45CEDC),** while +0x8C == 1, tricons & 3, HandStatus == 0 and |moved|² ≥ 0.02²:
  - elapsed = GetTickCount − +0x128; towards = −press, across = (press.y, −press.x), each made unit when longer than 0.1.
  - Rotate and Pitch both: with features 1 and 2, |mx| > |my| → 4 (edge rotate), else pitch; otherwise feature 2 → 4,
    else pitch. Rotate only: with features 2 and 8, 4 when |along| < |across|, along < 0 or elapsed > 300 [0x9CE674],
    else 3 (pan); otherwise feature 2 ? 4 : 3. Pitch only: pitch.
  - Pitch is 5, or 6 with tricon 4 (Top). A pitch with my > 0, elapsed < 80 [0x9CE678] and land under the cursor
    becomes 3.
  - Before a decision HandStatus 0 moves nothing.
- **Edge rotate, state 4 (0x45D002..0x45D34E), only with feature 2 (vt +0x58 & 2 at 0x45D013; without it nothing
  happens, the cursor included):**
  - x = (cx − w / 2) / (w / 2), y = (cy − h / 2) / (viewH / 2). When x² + y² < 0.89 or > 0.91, the cursor goes onto
    the ring 0.9 (scale 0 at the centre): cx = ftol(((s x + 1) w + 1) / 2), cy = ftol((s y viewH + h + 1) / 2). The
    squared distance is tested against 0.89..0.91 while the ring is at 0.9, so a cursor on the ring (x² + y² ≈ 0.81) is
    put back on it every frame.
  - angle = atan2(ring − centre) − atan2(+0x2D4 − centre), wrapped to ±π; +0x2D4 = ring.
  - CHand +0x486C / +0x4870 = ring with +0x4874 = 1, and `LHMouse::SetPosition(ring)` unless `IsPlayBack` (0x45D23D..
    0x45D27E).
  - The yaw +0x50 and the working heading get + angle; +0x90 = 1; the confirmation feed fn_00454900(angle, dt); then
    the events (above).
- **Pitch drag, states 5 and 6 (0x45DC4D..0x45DD6E, the same code), only with feature 1:**
  - 0x303 is sent with [0xC5E168].
  - step = dy / h (the whole height [0xE8505A]) × 2.33333 [0x9CE62C] × the horizontal field of view [0xEA1DD0];
    pitch −= step, +0x54 += step; the feed fn_00454930(step); +0x90 = 2. The pitch is clamped later to [−π/6, 7π/16].
  - The 0.051 lift runs only when [0x9CE6B4] == 0 or with the middle button; otherwise [esp+0x4A] keeps the eye where
    it was at the frame's start. [0x9CE6B4] is 1 to begin with and written beside [0xC5E154] by the player profile's
    load and save (0x66B81C, 0x66BE2C).
- **Wheel (0x45B704..0x45B7A1), only when +0x2EC == 0 and the clear view [0xC5B100] < 0.01:**
  - On action 3 (ZOOM_OUT) v = [0xC5E8D8] (the raw wheel sum, 120 a notch), or −120 when it is 0; else on action 4
    (ZOOM_IN) v = [0xC5E8D8], or +120 when 0. [0xC5E8D8] is cleared in every such frame, so a wheel turn without
    either action is lost.
  - The zoom input [0xC5B0CC] −= v × 0.5 [0x9CE61C]: **60 a notch**, and the keys ±60 a frame, not scaled by the frame
    time. Then zoom delta = [0xC5B0CC] × 0.0015 × the zoom scale; without feature 4 both are 0.
  - The input mask gets 0x08 when v ≠ 0.
- **Both buttons (0x45B784..0x45B85C), with [0xC5B0B0]:**
  - Three ways set it: the hand state of the last turn is 22 (0x5D8090), the raw both-buttons bits [0xE85304] & 1 and
    & 2 while the window has focus (0x45ABDA), or the grip with the right button (0x45ACB4). Each clears the grip and
    the right button. Feature 0x20 clears it and the middle button (0x45ADC3..0x45ADD9), after the given-up drop.
  - Zoom: [0xC5B0CC] += dy × 1.9 [0x9CE620] (feature 4 still zeroes the zoom later; the turn stays).
  - Turn: threshold = w × 0.025 [0x9CE680]; when |dx| > threshold or |cx − [0xC5E184]| > threshold, [0xC5E180] = 1;
    while it is set [0xC5B0C8] += dx × 1.9. Without both buttons [0xC5E180] = 0 and [0xC5E184] = cx. So state 22 "Zoom
    Landscape with both buttons" and the two-button turn are the same code: zoom always, turn once the mouse has
    moved 2.5 % of the width.
  - The middle button (rotate around the mouse): turn += dx × 1.9 [0x9CE628], tilt −= dy × 1.7 [0x9CE624].
  - The freeze: `SetTurnOffMouseMove(MMB || two-button)` (0x45AE96) sets [0xE8C0FA], also set in playback (0x54A710).
    At the end of Update, unless in playback, while it is set the cursor = +0x2D4 with `LHMouse::SetPosition` every
    frame, and once more SetPosition(+0xB0) on the first frame after (0x460564..0x4605C2). With both buttons +0x2D4 is
    refreshed from the held cursor (0x45AC44); the middle button holds +0xB8 (0x45AF8B..0x45AFAF).
- **Clear view, Ctrl + Shift (0x45AFD3..0x45B704, 0x45F231..0x45F344), with no feature gate:**
  - While action 6 (ZOOM_ON, Ctrl) and 15 (ROTATE_ON, Shift) are performed and the zoomer [0xC5B100] < 0.01, the start
    (0x45B0A4..0x45B4FB): the cursor to [0xC5B060], the origin to +0x1EC and the focus to +0x204; the target
    [0xC5B050] = the hand's point (CHand +0x482C → +0x38), or the object under the hand (GInterface +0x3C8 →
    [0xC5E17C]) at its MapCoords and altitude + its +0x1C; then target.y = the land's altitude at it (0x45B16D..0x45B1B4).
    `GetHeadingAndPitchFromPoints(origin, focus)` gives [0xC5B014] / [0xC5B018]; the distance [0xC5B010] (and
    [0x9CE6CC]) is 50 when |origin − focus| > 300, 10 when < 50, else 15; the pitch becomes (pitch + 3π/4 [0x8C7D00])
    × 0.25. A frame [0xC5B020..0xC5B048] is built from them, and +0x2D4 = the cursor.
  - Every frame while held (0x45B501..0x45B614): with the object still `IsAvailable` the target follows it and
    `Morphable::SetPos(CHand, target)` (0x45B568); a gone object is dropped. target.y = the altitude; +0x1F8 = target,
    +0x1E0 = `SetPointFromPointDistanceHeadingAndPitch(+0x1F8, distance, heading, pitch)`;
    `Zoomer::SetDestinationWithSpeedAndTime([0xC5B100], 1, 0, 0.5)`, or 0 once let go, re-planned every frame; the
    zoomer steps with the camera seconds (inline, the Zoomer's own update, 0x45B627..0x45B702).
  - Above 0.01 (0x45F231): origin = +0x1EC + (+0x1E0 − +0x1EC) v and focus = +0x204 + (+0x1F8 − +0x204) v, after
    everything else. The wheel is off (0x45B710), the tricons show only bit 4 above 0.5, `HandStateCamera::Update`
    0x5B05F6 changes the hand unless HandStatus == 3, and the tricon draw 0x456E8F places the icons differently.
- **Grip with other input (0x45BE04..0x45BE31):** a grip with only a keyboard move stays mode 1 and the move is
  dropped; with a zoom, turn or tilt input or the middle button ([esp+0x87], 0x45B85C..0x45B90E) it is mode 3.

### Player camera: flights to a place

`CameraModeNew3` flies in two legs, through a middle point, with the woosh.

- **`FlyToPosFoc(pos, foc, t)` 0x4587F0 (vt +0x50):** `InsideInclusion(pos, unit(foc − pos))` false → pos = its hit
  (only with the force field on, see the zones); +0x78 = 0; t ≥ 0 (only t < 0 skips, `test ah,1`) → `SetupVia(pos,
  foc, the origin zoomer's current value, t)` (+0x118 / +0x148 / +0x178); the woosh 46 + (GetTickCount() & 3) when the
  current origin is more than 100 [0x9CE640] × 1.5 from pos (0x458967..0x45899B).
- **`SetupVia(pos, foc, from, t)` 0x457A20 (vt +0x54):** via +0x104 = (pos + from) × 0.5 [0x8AA3B4] per axis (the x sum
  kept on the FPU, the y and z sums stored as floats first: the same at 24 bits); via.y = sqrt((from.z − pos.z)² +
  (from.x − pos.x)²) × t + via.y, the distance across the land only; then the land's altitude under the via
  (`GetAltitude` at the map coords FtoL((x × 65536 [0x8AC408]) × 0.1 [0x8AC404]), the same for z, altitude 0) + 10
  [0x8AB414] replaces via.y when strictly above it (`fcom`, `test ah,0x41`, 0x457AF4..0x457B0F); +0x100 = +0x101 = 1
  (flying, first leg pending), +0x110 = pos, +0x11C = foc.
- **The legs, at the end of `Update` (0x460390..0x4604A2):** with +0x100, once the origin zoomer's CurrentTime (GCamera
  +0x12C) is past 1.5 [0x9CE610] × 0.5, both Zoomer3d go to +0x110 / +0x11C in 1.5 s, +0x78 = 0 and +0x100 = 0; before
  that, with +0x101, the origin to the via in 1.5 × 0.9 s and the focus to +0x11C in 1.35 s, and +0x101 = 0. While
  +0x100 is set the frame's own origin and focus are not given to the zoomers.
- **Any camera input drops the flight (0x45BE69..0x45BE8D):** +0x100 = 0 when [0xC5B0E8] (a zoom, a keyboard move x or
  z, a turn or a tilt input that is not 0, or the middle button, 0x45B85C..0x45B8C5), the mouse mode being worked out
  (esi: 1 with the grip, + 2 with [0xC5B0E8], 0x45BDE9..0x45BE14), the grip [0xC5B0F0], the middle button [0xC5B0EC]
  or both buttons [0xC5B0B0] is set. A keyboard move alone while gripping clears [0xC5B0E8] (0x45BE20..0x45BE31), but
  the grip drops the flight anyway.
  - Only +0x100 is cleared. +0x101, the via +0x104, +0x110 / +0x11C and +0x78 are left as they are, and the zoomers are
    not touched there. +0x101 is only read under +0x100 (0x460402), and every `SetupVia` sets both again.
  - The camera does not stop: at the end of the same `Update` the frame's own origin and focus go to the zoomers with
    the frame's times (0x4604A4..0x4604D2). The frame starts from the zoomers' destinations (Zoomer +4, 0x45AA45..
    0x45AA76), so it goes on from where the flight's leg was heading, as the controls move it, and the last leg never
    comes.
  - The inputs are dropped after a given-up grip (+0x2F0, 0x45ACEE..0x45ADB9) and feature 0x20 clears the middle and
    both buttons (0x45ADC3..0x45ADD9) before the test. Zoom (feature 4, 0x45C0E4), Rotate (0x45C6CC) and Pitch
    (0x45C809) are read after it, so a zoom, turn or tilt they then take away still drops the flight. The auto pitch's
    tilt is set after it (0x45BEE4) and does not.
  - The double click's flight is set later in the same `Update` (`SetupVia` at 0x45E2DB), so it is kept in the frame it
    starts and only a camera input in a later frame drops it. The flight after a given-up grip (+0x78 = 0.01) calls no
    `SetupVia` and has no flight to drop.
- **The callers and their t** (a scan of every `call [reg + 0x50]` after a float push, and of the CameraModeNew3
  casts; (inferred) complete, as a call through a mode pointer kept elsewhere would not show):
  - the bookmark keys: fn_0043A070 0x43A1C8, from `GGame::ProcessKey` 0x63F632 (a bookmark key without a modifier):
    0.3 (0x3E99999A), to the bookmark's saved view;
  - fn_0043AA60 0x43AC80, from `GGame::ProcessKey` 0x63F681: 0.3, to a town's storage pit (or its +0x9A4 object) seen
    from x − 24.45 [0x8C6B44], y + 28.3786 [0x8C6B48], z + 47.4 [0x8C6B4C] (which key and which town not read);
  - the debug camera editor, `camera_editor_callback` 0x449C99 and 0x449D47: 0.3;
  - `CameraModeNew3::Restart` 0x4587EB (after `Reinitialise(1)`): 0.1, to +0x0C / +0x18;
  - `CameraModeNew3::ZoomToCitadel` 0x457D80: 0 (ebx, zeroed at 0x457B9F); 0x457DEA and 0x457ED2: 0.4 (0x3ECCCCCD);
  - `StartFight` 0x45A71D: 0 (see watching a fight);
  - the double click calls `SetupVia` itself (vt +0x54, 0x45E2DB) with 0.1 (0x3DCCCCCD), only for the 100 flight
    (+0x78 == 0, 0x45E2A1), from [esp+0x110].

### Player camera: watching a fight

There is no fight camera mode: `CameraModeNew3` keeps a fight of its own (bw1-decomp names, `CameraModeNew3.h`):
`HasFight` +0x44, the `GArena` +0x48 (fighters at +0x38 / +0x3C), `Yaw0` +0x50, `Pitch0` +0x54, `FightDistance` +0x58,
`FightTimeLeft` +0x5C (int ms), `TimeInArena` +0x60 (int ms) and `FightStatus` +0x64 (0 on, 1 lingering, 2 ended).
The addresses above come from the disassembly of the fight camera's functions.

- **`StartFight(GArena*)` 0x45A4D0:** returns at once without the arena or either fighter (0x45A4DD..0x45A4F3). Then
  HasFight = 1, the arena, and the in-view test `WantToQuitFight(GetPos, +0xF0, 1.0)` (0x45A530). It watches when a
  fighter's `GetPlayer` (vt +0x1C) is the local player and either has Flags +0x24 & 0x400 (controlled by a script), or
  when the arena is in view (0x45A53D..0x45A5AC). Watching (0x45A5B2..0x45A71D): FightDistance = 5.5 [0x9CE6C4],
  Yaw0 = π/2 (0x3FC90FDB), Pitch0 = 0.408407 (0x3ED11ABA), FightStatus = FightTimeLeft = 0, +0x4C..+0x4E = 0,
  +0x2D0 = 1 (the first frame), the turn zoomer +0x210 zeroed field by field and the focus Zoomer3d +0x240 / +0x270 /
  +0x2A0 `SetPosition(0)`, +0x78 = 0, then `FlyToPosFoc(pos, foc, 0)` (vt +0x50, 0x45A71D) to the view fn_00458E50
  builds (below), with P the arena's centre (`GetPos` vt +0x100: x and z × 1/6553.6 [0x8AA3A4], `GetAltitude` + its
  altitude) and Q = (P.x + r, P.y + r × 0.5, P.z) with r = `GetRadius` (+0x30). The creature's arenas are made with
  altitude 0 at the quantised middle of the two fighters (0x5074B1..0x50750C), so P.y is the land there. Not watching
  (0x45A72A..0x45A7B7): HasFight = arena = 0, and the `CreatureFight` help sprite fn_0071CD40 for the local player's
  fighter; the camera does not fly.
- **The fly-to view, fn_00458E50(from, focus, &pos, &foc) 0x458E50** (cdecl; its callers are StartFight 0x45A706 and
  `CameraModeFollow::Update` 0x44C867): foc = focus; heading and pitch from `GetHeadingAndPitchFromPoints(from,
  focus)` 0x4428D0; d = |focus − from| (3D, float subtractions, (dz² + dy²) + dx², a float); d < 50 [0x8C6CA4] →
  (50 − d) × 0.8 [0x8C4A04] + d, then d > 100 [0x8AB41C] → (100 − d) × 0.1 [0x8AB22C] + d (not a number passes both
  unchanged); heading = `FindBestAngle(heading, d, focus, &pitch, null)`; pos =
  `SetPointFromPointDistanceHeadingAndPitch(focus, d, heading, pitch)` 0x442810. It touches no zoomer. No random draw
  in its tree (`__ftol` is the only runtime call).
- **`CameraModeNew3::FindBestAngle(heading, distance, point, &pitch, &score)` 0x458F40** (static, cdecl): for each of
  32 headings heading + i × π/16 [0x8AB504] (a float), the score is the sum over j = 3..7 of point.y − the land's
  altitude at `SetPoint(point, j × distance × 0.125, that heading, 0)` (map coords (m × 65536) × 0.1, truncated), plus
  50 × cos(i × π/16) (50 a double, `fcos` of the float i × π/16); the first strict maximum wins (from −1e20,
  0xE0AD78EC; ties keep the lower i). The same loop computes `SetPoint(point, distance × 0.3, heading + i × π/16, 0)`
  and its altitude and does not use them. Then the pitch: pitch × 0.2 [0x8AB244] + (normal pitch × 0.5) × 0.2
  [0x8C7C68, a double] + 3π/25 (0.37699114390179034 [0x8C7C60], a double), where the normal pitch is
  `GetHeadingAndPitchFromPoints(point + n, point)`'s with n = `GetNormal` at the point's map coords (1.5393804 on level
  land); ≤ π/8 [0x8C6CA0] (or unordered) → π/8, else ≥ π/3 [0x8C79E0] → π/3. Returns heading + best × π/16; `score`
  gets the best score when not null. On level land the arena's view keeps the heading (3π/2, back towards the centre)
  and gets the pitch 0.2 × atan(−0.5) + 0.1539 + 0.3770 ≈ 0.4382. The double click's flight calls it too (0x45E22F).
- **The hand-over:** the fly-to runs once, in StartFight; the per-frame orbit never calls it. The orbit runs from the
  next `Update` and writes the frame's origin and focus, but while +0x100 is set the tail gives the zoomers the
  flight's legs instead (see "Player camera: flights to a place"). After about 0.75 s the last leg is set and +0x100
  cleared (that frame skips the orbit's place too); from the next frame on the orbit's place replaces the last leg.
  So the view of the arena is an approach, not reached and held. The orbit's zoomer time [esp+0xB8] (0 each frame at
  0x45E359, set at 0x45E3C3..0x45E408) is 2.5 [0x8C581C] × (GCamera +0x68 > 2 [0x8C7BC0] ? 0.8 [0x9CE6C0] : 2
  [0x8C7BBC] + (0.8 − 2) × +0x68 / 2): 2 s once the mode has run 2 s, 5 s right after a mode change. It is set only
  when the orbit runs, after its skips and its HasFight, arena and fighters tests (0x45E350..0x45E3A4). Not zero, it
  replaces the tail's time for the origin and the focus (0x460229..0x46024A, after the self tilt's 1 s at 0x460216);
  the flight's legs (0x460390..) keep their own times.
  - The mode seconds, GCamera +0x68 (see "GCamera and its zoomers"): `GCamera::Update` adds the frame's camera seconds
    (`GGame::GetCameraTimeInc` × 0.001, at most 0.1, 0x441F95..0x441FC1, also kept at +0x6C) at 0x441FCD, before the
    current mode's Update; `SwitchToViewMode` sets it to 0 for any mode pushed (0x441D30: the script mode, a dual
    camera, a follow camera), and `PopViewMode` for any mode popped (0x441C86, back to the mode below; 0x441CB9, the
    last one popped and a new CameraModeNew3 made); fn_0044BB30 sets it to 2 (0x44C141). StartFight does not touch it:
    the player's mode goes on, so a fight started 2 s or more after the last mode change eases in over 2 s.
- **Its callers:** the creature's fight start 0x50380A (when the camera's mode `dynamic_cast`s to CameraModeNew3);
  lingering in `Update` 0x45BB64..0x45BCA1 (not fighting, the screen-centre ray on the land +0xC8, and for an arena
  of the list g_game +0x205C7C, linked by +0x48, both the camera and its focus strictly within its radius across the
  land, with both fighters: TimeInArena += g_delta_time, StartFight above 1000 ms; TimeInArena is only reset while
  HasFight, 0x45A980); and the double click's flight onto an arena 0x45DFC5..0x45E086 (feature 0x10, not the give-up
  flight, not fighting, the target within an arena's radius).
- **`WantToQuitFight(camera, focus, k)` 0x45A390:** false without HasFight, the arena or +0xC8. With r = GetRadius
  (vt +0x60) × k and the across-the-land distances (dz² + dx², the arena's altitude read and dropped): true when
  r × 3.2 [0x8C7C8C] < |arena − focus| and r × 4.2 [0x8C7C88] < |arena − camera|, or r × 6 [0x8AB35C] < |arena −
  camera|. StartFight passes `LH3DCamera::GetPos` and the screen-centre hit +0xF0 with k = 1.
- **`Update` 0x45A971..0x45A9CA, first:** with HasFight, TimeInArena = 0; FightStatus 1 counts FightTimeLeft down by
  the frame's game ms g_game +0x205D48 and turns 2 below 0 (`jns`); FightStatus 2 → `EndFightNow(1)`; else an arena
  gone or not `IsAvailable` (vt +0x2C) → fn_0045A800(1) and arena = 0. Then (0x45AA7A..0x45AAB7) with HasFight, an
  arena and mouse mode 0 or 2: +0x12C = the focus and +0x4C..+0x4E = 0.
- **Dragging away 0x45BAFF..0x45BB5F:** after the screen-centre ray, when 1.5 × 3 [0x9CE60C × 0x8C2C50] < +0x78,
  `WantToQuitFight(camera, +0xF0, 0.75)` and the mouse mode +0x8C & 1 (the previous frame's: +0x8C is set later, at
  0x45BDE9): `EndFightNow(0)` and `UpdateClickParams(camera, focus, 1)`. +0x78 is the time since the camera's last
  flight began: += the camera's seconds every frame (0x4601A7..0x4601B7), 0 at StartFight, 0 / 0.75 / 0.75 / 0.01 for
  the double click's 100 / 60 / 30-15 / give-up flights (0x45E13D..0x45E1DD), 0 at a flight's last leg (0x4603EF).
- **The turn and tilt (0x45C6A8..0x45C8A9, 0x45D2B2, 0x45DCCF):** Yaw0 += turn × π / w (with feature 2) and the edge
  rotate's angle; Pitch0 −= tilt × 0.002 (with feature 1, or with a local flag, (inferred) the self tilt), and += the
  pitch drag's step (the other way from the camera's own pitch).
- **The keyboard skip [esp+0xCF] (0x45E36F):** 0 each frame (0x45B930). With feature 8 (`GetCameraFeatures` & 8,
  0x45C943..0x45C948) a keyboard move input [0xC5B0D4] or [0xC5B0D0] not 0 first reports Drag (`CameraHelpCallback`
  0x308 with the input mask [esp+0xB8], 0x45C94E..0x45C986, in any mouse mode); then, in mouse mode 2 (+0x8C,
  0x45C98E: camera input, nothing gripped), the move is added to the frame's origin [esp+0x14], focus [esp+0x3C] and
  the click focus (0x45C9A1..0x45CAA7), and with either input still not 0 the flag is set (0x45CAD0). That block is
  the only one in `CameraModeNew3::Update` that applies the keyboard moves; the other uses of [0xC5B0D0] / [0xC5B0D4]
  there test them (0x45AD12..0x45AD38, 0x45B86F..0x45B882) or clear them (0x45ADAF, 0x45AE68, 0x45BE2C, 0x45CC08).
  So in mode 2 a keyboard move skips the orbit that frame and the move stands; the watch goes on (no keyboard input
  ends it: the drag away needs mode & 1).
- **Watching, each frame (0x45E37E..0x45E8D6):** skipped while +0x8C is 1 (the land alone dragged), or 2 with
  [esp+0xCF] (0x45E36F, above); needs HasFight, the arena and both fighters. With A = +0x38 and B = +0x3C, their
  LH3DCreature (+0x160 → +0x58) positions +0x78 and radii +0x5228 (`Creature::GetRadius` 0x4792C0; set by
  `LH3DCreature::SetSize` 0x48064C):
  - the middle (A + B) × 0.5, its y raised by (fn_004867B0(B) + fn_004867B0(A)) × 0.25;
  - d = |A − B| across the land; with a zoom ([esp+0xC8] = the zoom delta, [0xC5B0CC] × 0.0015 × the zoom scale) ≠ 0,
    FightDistance = ((((d + rB × FD) + rA) + zoom × 0.3) − d − rA) / rB (0x45E47D..0x45E51F);
  - FightDistance > 40 [0x9CE6C8] → `EndFightNow(0)` (0x45E538), and the frame goes on; then FD ≤ 0 → 0, FD < 40
    or 40; Pitch0 ≤ 0.241661 [0x8C78E0] → it, < 1.32914 [0x8C7C98] or it;
  - the distance (d + rB × FD) + rA (0x45E5A9..0x45E5EE);
  - the heading of B − A by fn_007FAA50 when |x| or |z| > 0.01 (the double 0x8C7A10), else the turn zoomer's value;
  - the first frame (+0x2D0): the turn zoomer and the focus Zoomer3d set to the heading and the middle, +0x2D0 = 0;
    otherwise the turn's destination value + W(W(heading) − W(value)) (W = fn_0045A8C0: as it is within [−π, π], else
    (t − ftol(t)) × 2π with t = a × 0.159155 [0x8C7C90], then twice: > π − 2π, < −π + 2π) and the focus' the middle,
    both speed 0 over 5 s (`SetDestinationWithSpeedAndTime`); both step with the camera seconds (the turn inline, the
    Zoomer's own update);
  - the origin `SetPointFromPointDistanceHeadingAndPitch(focus, distance, Yaw0 − turn, Pitch0)` 0x45E89C; +0x12C =
    the focus; the mode switch [esp+0xB4] = 3 (no mode case runs); RotateAroundPoint +0x88 = 0. The tail (the self
    tilt's height 0x45F10B with feature 0x40) still runs after it.
- **Ending:** `EndFightNow(int)` 0x45A830: with [0x9CE6BC] (= 1) or the argument, HasFight = arena = FightTimeLeft =
  0, FightStatus = 2. fn_0045A800(int) 0x45A800: with [0x9CE6BC] or the argument, and only while FightStatus is 0,
  FightTimeLeft = 3000 when ≤ 0, FightStatus = 1; a second call does nothing. Its callers: `Validate` 0x45A8AF,
  `Update` 0x45A9C5, fn_004845F0 0x48466D and 0x5038A6 (the fight's end, in the creature fight rules).
- **Features (`GetCameraFeatures` 0x456260, vt +0x58):** HasFight ? EnabledFeatures & ~0x10 : EnabledFeatures. Of its
  17 calls in `Update` only 0x45DDA3 tests 0x10: the double click's flight and the give-up flight are off while a
  fight is watched, the 3 s linger included. EnabledFeatures itself does not change.
- **Tricons (`UpdateTricons` 0x459398..0x45939F):** with HasFight, +0x90 &= ~2, the Pitch zone; the classifier then
  cannot start an edge pitch drag. The pitch icon still shows from +0x94 & 2 (the tilt input, or HandStatus 5 / 6;
  0x45934B..0x45936E, 0x45943C..0x45946C).

## Opcodes

**Faithful** except where marked. Those that move check the mode: with no mode «Script camera has been removed!»
(0xC0C0CC); another mode «We are in the wrong camera mode!» (0xC0C0EC) and they do nothing; `SET_CAMERA_POSITION`
0x6EC8F0 says nothing. 003 / 004 / 287 also have a "Script moving camera in citadel" note (0xC0C110).

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

The field-of-view tests (011 / 012, used by the intro, [intro.md](intro.md#timers-help-events-and-field-of-view)):
- 011 GAME_THING_FIELD_OF_VIEW = `GScript::IsGameThingFieldOfView` 0x6F8130. 012 POS_FIELD_OF_VIEW =
  `GScript::IsPosFieldOfView` 0x6F8060.
- Both use the drawn camera's `LH3DTech::g_world_to_clipping` [0xEA9E40], the near plane [0xE839E0] and the screen of
  g_info_transform.
- 012: false inside the temple or with no view (0x6F80ED..0x6F8109), else the point test (0x6F810F).
- 011: false with no thing or inside the temple (0x6F819F..0x6F81B4). For an Object (`dynamic_cast`, 0x6F81BA), its 3D
  object's (+0x40) mesh (vt +0xF8; no mesh → false) bounding sphere goes through fn_0081F1A0 →
  `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80, with the box centre at 0x868CBC. Any other GameThingWithPos uses its
  point (x, GetAltitude + its +0x1C, z) (0x6F81F4..0x6F822B).

## Releasing the camera

**Faithful.** `fn_006ECD70` (END_CAMERA_CONTROL 0x6ECEF0 and the task stop fn_006ECF20): removes the dual camera; if the
mode is the script one it deletes it and creates a `CameraModeNew3`, which starts from the current zoomers (no jump); the
FOV **always** returns to 70° in 0.5 s; then the script state (`Help/ScriptControl`).

## Placed paths (CameraModePath)

**Faithful.** A particle with a camera (`Particle3DAnimWithCamera`, the forest miracle's, see
[particles.md](particles.md) and [miracles.md](miracles.md)) takes the camera with `CameraModePath` (vtable 0x8C7D10,
0x14 bytes). The particle decides when the mode is made, when it plays and when it lets go; the mode does the rest.

- Creation, ctor 0x460E50: it asks the current mode (stack +0x28, index +0x58) its vtable slot 0x1C, `CanExit`
  (0x460E74). 0 makes the new mode delete itself (0x460E7B..0x460E85), and the particle's "mode made" flag is already
  set, so it never asks again. `CameraModeScript::CanExit` 0x461B70 is 0 while the script mode lives. The dual camera
  and the path mode share slot 0x1C = 0x44A2E0, `mov eax, 1`: a dual camera over the script mode does not refuse a
  path, and a path goes on top of another path.
- Accepted: +8 = the particle, +0x10 = 1 (valid; `IsStillValid` 0x460ED0 returns it), then `SetUpNearClipping`
  0x460F10 (the near plane [0xE839E0] saved at +0xC, set to 0.1 at 0x460F24, and [0xC3812C] / [0xC38130] worked out
  again from it), then `SwitchToViewMode` 0x441CD0 (the old top's `Cleanup`, the path pushed). `Restart` 0x460F50 is
  `SetUpNearClipping` again.
- `Update` 0x460FC0 calls `UpdateCamera` 0x67AA70 with the position zoomers (camera +0x118, first) and the focus ones
  (+0x88). The sampler's first point, the position, goes to +0x118 and the second, the focus, to +0x88
  (0x67AAB8..0x67AB43). With no creator or no path it does nothing; otherwise:
  - playing (particle +0x3C): the path sampled at the clip time +0x28, and each of the six coordinates sent there with
    `SetDestinationWithSpeedAndTime(v, 0, 0.3)` (0x67AAC3), every camera update;
  - not playing, once (+0x35 set and +0x36 clear): the same sample with the time pause (+0x38) + 0.3 (`fadd`
    0x67AB86, stored as a float at 0x67AB93), then +0x36 = 1. The third coordinate of each point is an inline copy of
    0x407D60 with the same placing below 0.001 s (0x67ABBE..0x67ACBC, 0x67ACFA..0x67AE04). Then nothing until it plays.
- The clip time +0x28 is 0 from the ctor fn_006C8540 (0x6C8556) and is written by `Particle3DAnim::DrawAt` 0x67A8FC as
  `GetCycleTimeFromFrame(drawn frame)`, the clip's ms × frame / 1000 in integers: for Forest.cam's 6633 ms, frame 999
  is 6626 ms.
- The sampler fn_0086D4E0 is openblack's `CameraPath::SampleAt`. It then places both points with the object's matrix
  (object +0x14: x row +0x14, y row +0x20, z row +0x2C, translation +0x38), each coordinate ((z · m_z + y · m_y) +
  x · m_x) + t in the order of the x87 stack (0x86D6B0..0x86D74D), in single precision (the game thread's 24-bit FPU,
  [audio.md](audio.md#owners-the-24-bit-fpu-and-the-double-constants)).
- The stop: the particle's `DrawAt` 0x67AA45..0x67AA62, at a drawn frame ≥ the frames of a cycle (1000) − 1, calls
  `StopCamera` 0x6C86C0, and fn_00441E00 sets +0x10 = 0 on every path mode of the stack with that particle
  (0x441E16..0x441E48).
- The take-back: `ProcessKeyMovement` 0x460FE0 (actions 9 / 10 and 7 / 8, ±400 × the camera's frame time +0x6C,
  `__ftol`) makes a `CameraModeNew3` for a step that is not 0 (0x46108C), and so does the interface state +0x3AC ==
  0x14, the land grip (0x4610C3). Rotating, tilting and zooming are not read.
- `Cleanup` 0x460F60: the near plane from +0xC, [0xC3812C] / [0xC38130] again, `CHand::Show(1)` (0x460FA4) and
  0x6CA5D0 on [0xD4EDB0]. No caller hides the hand for the path.
- None of these draws a random number.

## openblack

> **Code rules.** The script camera's state lives in ECS components or Locator services, not in globals; the zone files
> and `camera.edt` load through the resource caches; resources are owned with RAII and standard types; the camera maths
> are tested with fakes in `test/`, never through the locator; comments describe behaviour in plain English, with no
> decompiled names or addresses (those belong here). See [the conventions](../refactor/README.md).

- `src/Camera/ScriptCamera.{h,cpp}` (`script_camera::`): the position, focus and FOV zoomers, the script mode
  (`Begin`/`End`/`Active`/`Drives`), Set/Move/RunPath/SetFov, `ScriptArrived`, `Frame` (steps 1-5 and 7) and
  `DrawnCamera` (steps 3-6). `UpdateCamera` does it every frame from `Game.cpp` and, while the script mode
  drives, the player model (`DefaultWorldCameraModel`) neither moves the camera nor reads keys (Script has no keys,
  0x44C3BD). Positions and foci go in `Zoomer3` (`Common/Zoomer.h`, the same one as the player camera).
- **Hand-over of the zoomers (faithful):** GCamera has a single set of zoomers for all modes. openblack has the
  player's (`Camera::GetOriginZoomer/GetFocusZoomer`) and the script's: `Begin` copies the player's as they are (value,
  speed, destination and time: the script mode continues towards where the player's was going, 0x461180 does not touch
  them) and `End` returns the script's to the camera (`HandBack`), from where the player starts like
  `CameraModeNew3::Initialise` 0x456640. With the hooks `OPENBLACK_CAMERA_LOCK/FLY` the player never released the
  camera and nothing is copied.
- `CHLApi.cpp`: the opcodes of the table; `StartCameraControl` passes `cameraTaken = script_camera::Begin(...)`;
  END_CAMERA_CONTROL and the task stop (Game.cpp) call `script_camera::End`. When loading a map, `Reset`.
- **(approximate)** While the script drives, the player camera carries what is drawn (with the nudge and the metre above
  the ground), not the script zoomers. Without script mode, 035 compares the player's zoomers with their destination (the
  same rule 0x441700). `SetPositionAndFocus` does not have the early exit of 0x4438C0. The game ms of the frame are those
  of the previous frame's clock (the original updates the camera after the turns).
- **(inferred)** 284/286 without script mode also set the player camera.
- The FOV goes to `config.cameraXFov` (degrees) only when its zoomer changes: a player's own FOV lasts until a
  script touches the lens.
- **The land grip (faithful, `DefaultWorldCameraModel`):**
  - A drag grips at the camera's first update after the press (`LandGrip`: the camera, the cursor,
    `camera_pan::PlaneThrough`, the depth, whether there was land under the cursor). The depth follows the zoom.
  - Too deep, the drag is given up before the pan: every control is dropped until a frame with none, the hand cue is
    `camera_drag::tricon::k_GivenUp`, and with feature 0x10 the camera goes 1000 from the hand's point the same frame,
    keeping its heading, the pitch clamped, without a midpoint, with the woosh.
  - Otherwise `camera_pan::Pan` from the grip camera, then `StopPanShortOfLand`: `LandIslandInterface::RayCast` from
    the grip camera, with the drawn camera for the 7500, and `camera_pan::StopShortOfLand`. The cursor is not warped.
  - The camera help (faithful): each frame of the pan counts Drag (33) when the frame has a camera input, or the grip
    (or the middle button or both buttons, with feature 0x20) is held with the mouse moved more than 2 pixels across
    or down (`_dragCountsForHelp`, from `GetMouseDelta`). The flight after a drag given up reports as the double
    click's flight does, once (`ReportFlightToHand`).
  - Tests: `WorldCameraLandGrip.*`, `WorldCameraPan.*`, `CameraPan.*` and the recorded `DragUpDown`.
- **The mouse drags (faithful, `DefaultWorldCameraModel::FollowDrag`, `camera_drag::`):**
  - The hints and the classifier are raffclar's `IdleTricons` / `DragClassifier`; the very bottom and the top start at
    0.45 in a window, read from openblack's display mode (`EngineConfig::displayMode` Windowed).
  - Until the drag is decided the camera stays (Cartesian). A pan is the land grip above.
  - The edge rotate (his `EdgeRotate`) warps the cursor with `GameActionInterface::WarpCursor` and turns the camera by
    the angle swept, as turn input × w / π; without feature 2 it does nothing. It reports Rotate with the drag flag,
    RotateCW above 0.01 and RotateCCW below 0.01, the original's quirk included.
  - The pitch drag (his `PitchStep`, the camera's horizontal field of view) tilts the camera, without feature 1 it does
    nothing; it reports Pitch with the drag flag.
  - The player's own turn and tilt reports leave the drag's out, and a grip with only a keyboard move drops the move.
  - Tests: `WorldCameraMouse.*` (edge drag, still frames, turning taken away, pitch drag, the window's edge),
    `CameraDrag.*` and `WorldCameraHints.*`.
- **The wheel (faithful):** raffclar's 60 a notch (`k_WheelZoomPerNotch`) from the action map's
  `GetMouseWheelDelta`, read only while ZOOM_OUT or ZOOM_IN is performed (out first), a notch its way without the
  wheel; it replaces openblack's 20 × 400 × the frame's seconds. Tests: `WorldCameraMouse.TheWheelZoomsSixtyANotch*`,
  `WorldCameraMouse.AZoomKeyIsANotchAFrameAndTheWheelWithoutOneIsLost`.
- **Both buttons and the middle button (faithful):** both buttons (the action map's TWO_BUTTON_CLICK, which drops
  the grip and the action as it is read) zoom dy × 1.9 and turn dx × 1.9 through raffclar's `camera_drag::TwoButtonTurn`
  (strict > w × 0.025, the drift since the press); both, and the middle button's ArcBall and turn, only with feature
  0x20 (our JustZoom bit), checked after the given-up drop. The cursor freeze is the input's `CursorFreeze`, switched on
  by `Game.cpp` (`AllowCursorFreeze`) for the player's world camera with feature 0x20, outside the temple and the
  script camera. Tests: `WorldCameraMouse.BothButtons*`, `WorldCameraMouse.WithoutTheirFeature*`, `test_cursor_freeze`,
  the recorded `TwoButtonZoomOutIn` and `MiddleDragRightUp`.
- **The clear view (faithful in the camera):** a `Zoomer` re-planned every frame to 1 or 0 in 0.5 s with Ctrl and
  Shift (ZOOM_ON and ROTATE_ON) in HandleActions; `PlanClearView` at its start, while under 0.01: the camera's view,
  the hand's point, `script_camera::HeadingAndPitchFromPoints` and the 50 / 10 / 15 distance and (p + 3π/4) / 4 pitch;
  every held frame the point at the land's height and `PointFromDistanceHeadingAndPitch`. Update blends last, above
  0.01; the wheel is off above 0.01. Tests: `WorldCameraMouse.CtrlAndShift*`, `TheClearViewIs*`,
  `TheWheelIsOffDuringTheClearView`, `WithoutTheHandOverTheLandTheClearViewLeavesTheCamera`. **(approximate)** The
  point is the action map's hand position (the double click's), not CHand's 3D origin; without one over the land the
  camera is not moved ((inferred) the original's CHand +0x482C is always there).
- **Auto pitch (faithful):** `camera_help::AutoPitchInput` (a fifth of the way) in HandleActions while nothing is
  gripped, from the original's `HeadingAndPitchFromPoints` (`script_camera::`) of the camera and its focus at the
  click; with nothing to tilt the player's tilt is dropped. It tilts without the Pitch feature and is left out of the
  camera help's Pitch. The height (`GetAutoPitchDistance` over the land at the frame's start) is applied in Update after
  the constraints in the frames it tilts or nothing is gripped or done (`_idleMouse`); the focus keeps its offset and the
  focus at the click follows. Tests: `CameraHelp.TheSelfTiltingCameraTiltsAFifthOfTheWayAtMostTheFramesSeconds`,
  `WorldCameraMouse.TheSelfTilting*`, `WorldCameraMouse.GrippingTheLandStopsTheSelfTiltingCamera`.
  **(approximate)** The tilt is worked out in the controls (raffclar's place), so it is not while a hand demo plays,
  and the height there follows the last controls' frame; the altitude is `GetHeightAt`, not the map coords' fixed point.
  - **(approximate)** Floats where the original has doubles (n · r, t). The drag needs feature 0x08 and the hand over
    the land to start (whether the original gives up a grip without 0x08 is not read). The flight's point is the
    hand's, as the double click's. Both flights report 0x305, never 0x306 (the camera doesn't know the object under the
    hand), and the Drag's input mask is 0, as for every camera help call of openblack's camera.
- **Flights to a place (`DefaultWorldCameraModel`, `CameraModel::CharterFlight`):** `SetFlight` is `FlyToPosFoc`
  without the zone test, `CharterFlight` is `SetupVia` and `ComputeUpdateReturnInfo` the legs. Faithful: the via (the
  two origins added and halved, the distance across the land times t, at least the land + 10 under it, the land read
  at `map_coords::ToMetres(MetresToFixedForHandLookup(m))` as the hand's turn reads it), the legs' times (1.35 s, then
  1.5 s once past 0.75 s) and the woosh beyond 150. t is 0.3 for `SetFlight` (the bookmark keys; the debug editor and
  the test hooks take it too, as the original's camera editor does) and 0.1 for the double click's 100 flight.
  **(inferred)** `GetHeightAt` stands for `GetAltitude`, as for the hand. Tests (`test_camera`,
  `test/camera/test_camera_flight.cpp`): `CameraFlight.*` (the original's recorded double click via, the floor over a
  hill, t = 0, the raise across the land only, the land's lookup) and the recorded `DoubleClickFlyTo`. The watched
  fight's flight goes through the same `FlyTo` with t = 0. Any camera input drops the flight as the original does: in
  `HandleActions`, the same input test as the Drag help's (a zoom, move, turn or tilt input, or the middle button; a
  keyboard move alone not while gripping), the grip or both buttons reset `_flightPath`, leaving `_flightSeconds`
  alone. The update then gives the camera the frame's own target, which starts from the zoomers' destinations as the
  original's does. The controls run before the update, so the double click's flight, set in the update, is kept in its
  first frame. Tests (`test/camera/test_world_camera_mouse.cpp`): `WorldCameraMouse.*Flight*`.
- **A point from a distance, heading and pitch (faithful, `script_camera::PointFromDistanceHeadingAndPitch`):**
  `SetPointFromPointDistanceHeadingAndPitch`'s steps in its order: the sine and cosine in double, cos q rounded to a
  float, each product with a sine or cosine rounded to a float, the other products and the sums in float. Every point
  openblack's cameras build from angles goes through it: the script camera's follow, face and dual positions, the
  clear view, `camera_flight::FindBestAngle`'s samples and `ViewOfPoint`, the watched fight's orbit, the creature
  follow (`CreatureFollow`), and in `DefaultWorldCameraModel` the double click's flight, the Polar mode (the original's
  mouse mode 2) and the ArcBall's point (glm's `euclidean`, which rounds the sine first, is no longer used). Tests
  (`test_camera`): `CameraFlightView.TheDoubleClicksOriginIsTheOriginalsToTheBit` (the recorded flyToTargetOrigin,
  every axis to the bit) and `CameraFlightView.APointFromAnglesKeepsTheSinesUnrounded` (three points worked out with
  the original's steps); `ScriptCameraFollow.PointFromDistanceHeadingAndPitch` (`test_ui_script`).
  - **(inferred)** `std::sin` / `std::cos` in double stand for the x87's 64-bit `fsin` / `fcos`: a product rounded
    from 53 bits and from 64 can differ only when it lies within about 2⁻⁵³ (relative) of halfway between two floats (none
    in 100 000 random cases).
- **The view a flight goes to (`src/Camera/CameraFlight.{h,cpp}`, `camera_flight`), faithful:** `ShapeDistance` (the
  50 / 0.8 and 100 / 0.1 rules), `FindBestAngle` (the 32 headings, the five samples at pitch 0 through
  `script_camera::PointFromDistanceHeadingAndPitch`, the cosine bonus with 50 a double, the first strict maximum, the
  unused sample at 0.3 read and dropped, the pitch with the original's float and double constants and its two
  comparisons in order, so not a number goes to π/8), `ViewOfPoint` (fn_00458E50, the heading and pitch from
  `script_camera::HeadingAndPitchFromPoints`) and `ArenaLookPoint` (Q). The land is read at
  `map_coords::ToMetres(MetresToFixedForHandLookup(m))` for x and z. **(inferred)** `GetHeightAt` and `GetNormalAt`
  stand for `GetAltitude` and `GetNormal`. The watched fight uses them, and so does the double click's flight
  (`DefaultWorldCameraModel::UpdateModeFlying`): for the 100, 60, 30 and 15 flights, not the one after a drag given up,
  `FindBestAngle` takes the camera's heading and pitch, the flight's distance unshaped and the hand's point, as the
  original's call does; then the pitch is kept within π/8 and 1.496 as before. Tests: `CameraFlightView.*` (the
  distance at 10, 30, 50, 75, 100, 200 and not a number; level land: the heading kept, the pitch 0.4382 and the 192
  land reads; a wall ahead: the open side; the pitch's bounds and not a number; the arena's view),
  `WorldCameraMouse.OnLevelLandTheDoubleClicksFlightKeepsTheCamerasHeading` and
  `WorldCameraMouse.OverAHillTheDoubleClicksFlightLooksFromTheOpenSide`, and the recorded `DoubleClickFlyTo`.
- **Watching a fight (`DefaultWorldCameraModel` as `camera::FightWatch`, `src/Camera/FightOrbit.{h,cpp}`):**
  `GetFightWatch` returns the world camera itself (the other models have none). The fight's side (the creature fight rules)
  makes the watch decision and calls `StartFight(a, b, centre, radius)` for a watched fight, `EndFight` at its end;
  `WantToQuitFight(centre, radius)` is the in-view test it asks first (the camera's current origin, without the shake,
  and the screen-centre hit; false without one). Faithful:
  - the state (`_fight`) and StartFight's values (5.5, π/2, 0.408407, the first frame, the zoomers reset, the flight
    clock 0); EndFightNow; EndFight acting only while the fight is on, so a second call keeps the time left; the linger
    counted down by the frame's whole ms at the start of Update;
  - the flight to the arena in StartFight: P = the centre's x and z through a map position (`map_coords::FromMetres`,
    altitude 0) and back with the land's height (`map_coords::ToWorld`), Q = `camera_flight::ArenaLookPoint`, the view
    `camera_flight::ViewOfPoint(P, Q)`, then `FlyTo` with t = 0: `CharterFlight`'s middle point, the woosh beyond 150,
    and the flight clock 0. The legs and the hand-over are `ComputeUpdateReturnInfo`'s: the orbit's place reaches the
    camera only once the last leg is set. Without a land loaded there is no flight;
  - the drag away: the flight clock `_flightSeconds` (+0x78, kept by the double click's flights and the last leg as
    above) past 4.5 s, the camera's target and the screen-centre hit too far at 0.75 of the radius, and the land
    gripped in the frame before: EndFightNow and the click parameters again;
  - the focus at the click set to the focus at the start of each frame while not gripping;
  - the turn and tilt into Yaw0 / Pitch0 (the edge-pitch drag the other way);
  - the orbit in place of the controls' mode, skipped while the land alone is gripped (`_gripOnly`, mode 1), and in a
    frame of a keyboard move with the land grab allowed and nothing gripped (`keyboardMoveStands`, mode 2 with
    [esp+0xCF]: the keyboard's move stands, as `TiltZoom` and the controls' mode give it, and the watch goes on): the
    middle, the zoom in the second fighter's radii, the end past 40 with the frame still placed, the clamps, the
    distance, the heading through `affine::GetYAngle` and `fight_orbit::WrapAngle`, the two 5 s zoomers on
    `Common/Zoomer`, and `script_camera::PointFromDistanceHeadingAndPitch`; the self tilt's height and the clear view
    still come after it;
  - the orbit's ease time: `fight_orbit::EaseSeconds` of the mode seconds, which are `script_camera::State::modeSeconds`
    (GCamera +0x68: 0 at the script mode's start and end and at a dual camera's, plus the frame's camera seconds, at
    most 0.1, in `script_camera::Frame` before the player's camera moves), kept in `_fight.easeSeconds`: 0 each frame,
    set only once the orbit has both fighters, and when not 0 the time of the orbit's place in
    `ComputeUpdateReturnInfo`, in place of the model's own time; the flight's legs keep theirs. It reaches the camera in
    whole microseconds, as every time the model gives does;
  - the features less 0x10 (`camera_help::DuringFight`) in both the controls and the update, and the Pitch tricon
    cleared from the idle hints.
  - Tests (`test_camera`, `test/camera/test_world_camera_fight.cpp`): `FightOrbit.*` (the radii test, the wrap, the
    turn the short way, the distance, zoom and clamps, the 0x10 mask) and `WorldCameraFight.*` (the orbit and the
    tilt's bound, no orbit while the land alone is gripped, zoom past 40, EndFightNow, EndFight once, a fighter gone,
    the drag away after 4.5 s and not without a grip, WantToQuitFight, no double click flight, no Pitch tricon, and
    `TheWatchFliesToTheArenaFirst`: the via leg in 1.35 s, nothing for 22 frames, the last leg to `ViewOfPoint` in
    1.5 s, then the orbit's place in 5 s),
    `FightOrbit.TheOrbitEasesInOverFiveSecondsAfterAModeChangeAndTwoOnceTheModeIsSettled` and
    `WorldCameraFight.TheOrbitEasesInOverTwoSecondsOnceTheModeIsSettledAndFiveAfterAModeChange` (2 s past 2 s of the
    mode, 3.5 s at 1 s, 5 s after the script camera taken and given back, the model's own time once the watch is over),
    `WorldCameraFight.AKeyboardMoveWithTheLandGrabSkipsTheOrbitThatFrameAndTheWatchGoesOn` and
    `WorldCameraFight.WithoutTheLandGrabAKeyboardMoveLeavesTheOrbit`.
  - **(approximate)** The fighters' radii come from the radius function the model is made with, by default
    `ecs::object::GetRadius`, whose creature branch (LH3DCreature +0x5228) is not ported: the generic footprint radius
    stands in, 0 without a loaded mesh. A radius of 0 is divided by as the original does (IEEE floats, see Pending).
    The positions are the fighters' `Transform`s. The arena is "gone" when either fighter is no longer a valid entity with a Transform. The
    fighters are the arena's in the order R05 passes them (the creature, then its opponent).
- **Placed paths (faithful):** `ecs::systems::CameraPathSystem` (`Begin`, `FollowAt`, `Release`, `HoldsCamera`,
  `HandlePlayerControl`, `Update`, and `CurrentPlaced` for the debug window) and the rules in
  `Camera/CameraPathControl.h` (`camera_path::PlacedGlideSeconds`, `PlacePoint`, `PathTimeFromFrame`,
  `ReachedLastFrame`, `TakesCameraBack`). The one caller of `Begin` is the forest's camera particle
  (`ParticleAnimWithCameraCreator`, [particles.md](particles.md#the-particle-meshes-creatorsmeshcpp-particle3dobjdrawat-0x679fd0-and-the-shield-dome)):
  it asks at its first step that is the local player's, gives the path's time and the playing flag every frame from
  its drawn frame (`psys::forest_camera::UpdateFrame`), and lets go at frame 999 or when it goes.
  - `Begin(owner, path, placement, pause)` is the mode's creation: refused while the script mode is the current one
    (`script_camera::ScriptModeCurrent`, a fake in the tests), without a path or a camera, and for an owner that has
    one already. The paths stack, the last the current one. They move the camera's own zoomers, the one set GCamera
    has, so a path starts from wherever the camera was heading.
  - `FollowAt(owner, pathMs, playing)` keeps the clip time and the playing flag, as `DrawAt` and the particle's turn
    write +0x28 and +0x3C. `Update` is the mode's update (the glide once, then the 0.3 s aim every update) followed by
    `Camera::UpdateZoomers`.
  - `Release(owner)` is `StopCamera`, wherever the owner's path is; the path under it is the current one again. The
    take-back drops every path ((inferred) the player's mode goes on top, and none under it is current again).
  - Game.cpp, once a frame: `HandlePlayerControl` before the player's camera keys (a key moving the camera left,
    right, forwards or backwards, the camera's frame ms, and the land grip, the interface's hand state 20 of the last
    turn, as +0x3AC == 0x14); while `HoldsCamera`, `camera.HandleActions` is skipped and the path's `Update` runs where
    `camera.Update` would (after the script camera and Creature Mode's update, as before); the camera particles'
    `UpdateFrame` with the frame updaters, after the camera's update and before the draw, as the particle's `DrawAt`.
  - The near plane: `near_clipping::NearPlane(height, close)` with `close` while `HoldsCamera`, the 0.1 of
    `SetUpNearClipping`. openblack sets the near plane every frame, so nothing is saved and put back.
  - The debug bar's Camera window has a "Placed path" header (closed at first): who placed the path that has the
    camera, the glide left, the path's time, whether it plays, its particle's last drawn frame, and a Release button.
  - The speed-up factor reaches the camera only through the drawn frame. The earlier port's own clock ((t − pause) ×
    speed-up × 1000), its end at 999/1000 of the clip's ms and its end with the miracle are not the original's and are
    gone.

## Test hooks

- `test_script_camera`: a single mode, arrival, 0.1 s cap, placing with T < 0.001, disc, nudge and ground, FOV with
  game time and the return to 70° in 0.5 s; `ScriptCameraFollow.*`: T rule, distance and pitch, point from
  distance/heading/pitch, heading and pitch between points, «behind», place immediately, face of an object, things that
  disappear.
- `test_script_camera_dual`: the dual (pace, focus, distance, heading, with point, validity, end of control), the shake
  (radius, decay, rolls), the zone loader and `InsideInclusion`.
- `test_camera`: `CameraPathSystemTest.*` (the refusal under a script camera, the glide over the pause and 0.3 s, the
  0.3 s aim while playing, `Release`, a path over another, the take-back, the debug read-out) and `CameraPathRules.*`
  (the glide's float sum, the point's order, 6633 ms at frame 999 is 6626, the stop at 999 and not 998).
- `test_magic`: `ForestCameraRules.*` and `ForestCamera.*` (`test_psys_forest_camera.cpp`), the forest's camera
  particle that drives the placed path.
- `OPENBLACK_CAMERA_LOCK` / `OPENBLACK_CAMERA_FLY` win over the script camera (`Drives()`, not original).
- In game: Land 1, when the tutorial requester's answer does not skip the tutorial, runs FollowUs and
  CreaturesInGlade with the script camera.

## Pending

- 372/373 (following the PC player's hand, `GComputerPlayer::GetHandPos` 0x657FE0).
- SET_AVI_SEQUENCE 203 with video: `PlayFullScreenMovie("data\intro.bik")` 0x54D920 pauses the game and returns it at
  58 s; without Bink in openblack there is no movie nor pause (approximate). Sequence 2 (video of the spell falling).
- (approximate) The creature's angle (LH3DCreature +0x84) does not exist: no heading adjustment and `GetFacingDirection` 0.
  The GameAngle of a villager comes from `WallHug::yAngle` rounded to 2048 per revolution.
- Player camera: shake, zone (repositioning, force field) and fixed rotation.
- (approximate) No mode stack: the dual always goes on top of the script; a dual over the player mode is
  approximated.
- Placed paths:
  - how the stack drops a mode whose `IsStillValid` is 0 was not read: openblack drops it at once. The 12 places of
    the stack are not counted;
  - a script camera or a dual camera started while a path holds the camera goes on top of it in the original (the
    path's `CanExit` is 1), and the path's `Cleanup` then puts the near plane back. In openblack the script camera's
    update comes first and the path waits under it, but the near plane stays the close one while the path holds the
    camera, and the path's take-back is still read;
  - the movement keys are read before the frame's clocks are updated, so the camera's frame ms is the last frame's;
  - `Cleanup`'s `CHand::Show(1)` (0x460FA4) and 0x6CA5D0 on [0xD4EDB0] (2) are not ported: what `Show(1)` does to a
    hand that is shown was not read, and the path never hides it;
  - steps 4 and 6 of GCamera::Update (the nudge, 1 m above the ground) on a path's frames: `Camera::UpdateZoomers` does
    steps 3 and 5;
  - the particle copies its atom's matrix into the object every turn (0x6C874A); openblack takes the placement once,
    at `Begin` ((inferred) the same for the forest, whose group 2 no rule moves);
  - `CameraModeNew3`'s ctor 0x4572E0 on a take-back, not read.
- `CameraModeNew3::Reinitialise` 0x4589B0 (how the player takes the camera back), not read.
- Still unread for the debug camera: the focus object [0xEA9ECC] (its +0x38, 0x8199E1..0x819A01) and fn_00819F50's copy
  of the test (0x819FAA, the falling spell's camera).
- CameraHelp's features: "[0x9CDD6C]'s initial value, ClearMap's and SET_INTERFACE_INTERACTION(NORMAL)'s (0x80 / 0x100:
  only here, pending)": the meaning of bits 0x80 / 0x100 is not read.
- The land grip, not ported:
  - the move report 0x200 with [0xC5E168];
  - the given-up flight's exclusion and disc tests, and the follow camera on a creature (0x45E30A);
  - the input mask of the camera help calls (0 for all of them, Drag included), and 0x306 for a flight on an object;
  - +0x78 (0 for the 100 flight, 0.75 for 60 / 30 / 15, 0.01 after a given-up grip) is read: the time since the last
    flight, which only the drag away from a watched fight reads (see "Player camera: watching a fight");
  - the tricon display while the inputs are dropped (0, or bit 8 with feature 0x80): the hand cue carries +0x90 only;
  - mode 3, a grip with another camera input: openblack's drag modes take the drag's mode alone (an edge rotate or a
    pitch drag adds the player's other inputs to its own, a pan drops them).
- The mouse drags, not ported:
  - CHand +0x486C / +0x4874, the hand's copy of the ring cursor: the action map's `GetCursorWarp` has it, no hand code
    reads it yet;
  - the pitch drag's lift with [0x9CE6B4] == 0 (a player profile option, not named): openblack keeps the eye, as with
    its initial value 1;
  - (inferred) openblack's Borderless display mode counts as full screen for the 0.45 edge (only Windowed is a window),
    and a display mode changed after the start is not seen;
  - the tick count is the controls' frame time, not GetTickCount.
- (inferred) The cursor freeze is not switched on while the script camera drives: the original freezes only from the
  player camera's own update (0x45AE96); whether a script camera ever freezes it has not been read.
- Both buttons: the hand state 22 route is the raw buttons in openblack (the same buttons); the freeze in a hand
  demo's playback (0x54A710) is not ported (openblack's demos press no camera buttons).
- The clear view, not ported: the object under the hand as the target and the hand set onto it with
  `Morphable::SetPos` (0x45B568; what that does to the hand is inferred: pinned to the object) — openblack's camera has
  no object under the hand, as for 0x306; the hand's change in `HandStateCamera` (the hand cue `clearViewGrip` stays
  false); the tricon display (bit 4 only above 0.5) and the icons' placing; the saved cursor [0xC5B060], the frame
  [0xC5B020..0xC5B048] (no reader outside Update found) and +0x2D4.
- The wheel: the +0x2EC gate (not named); with the gate closed the original keeps [0xC5E8D8] for a later frame, where
  openblack's wheel delta is the frame's only.
- Points from angles, not ported or not read: `FeatureScriptCommands::StartCameraPos` (the feature scripts'
  START_CAMERA_POS) still builds its origin with glm's `euclidean` from 120 at 12.8571° / 157.51°, and the original's
  START_CAMERA_POS has not been read; `zoom_to::OriginAround` (`Camera/ZoomToPlaces`, not connected to the game) has
  maths of its own, where the original's `ZoomToCitadel` calls 0x442810 (0x457D69, 0x457EB7); the mouse mode 1 point
  (0x45EF0E) and the fixed rotation's (0x46019F) have no openblack counterpart; openblack's ArcBall (the middle
  button) is its own model, where the original's middle button turns and tilts through mouse mode 2 (0x45E9D1), so
  only its rounding follows the original's.
- Auto pitch: the player's tilt alone, dropped, leaves openblack's camera in its Polar mode where the original is in
  mode 2; the tilt is worked out in the controls, so not during a hand demo (see the openblack entry).
- Flights to a place, not ported:
  - `SetFlight` does not set the flight clock `_flightSeconds` to 0, as `FlyToPosFoc` sets +0x78;
  - the other callers of `FlyToPosFoc` have no openblack flight: fn_0043AA60's town view (0.3), `Restart` (0.1) and
    `ZoomToCitadel` (0 / 0.4; openblack's temple and realm keys, `Camera/ZoomToPlaces`, are not connected to the game);
  - the double click's flight takes the camera's heading and pitch from `EulerFromPoints` (glm's `atan`, straight up
    when both x and z are within 0.1), where the original's are `GetHeadingAndPitchFromPoints`' (in openblack
    `script_camera::HeadingAndPitchFromPoints`, within 0.01, a double); whether openblack's target origin and focus at
    the click are the original's frame origin [esp+0x14] and focus [esp+0x3C] at that point of the frame is not checked;
- Watching a fight, not ported:
  - the arena's own altitude for the arenas a script makes (`GScript::GetArena` 0x6F4C36, not read): openblack's
    flight takes the land's height at the centre, which is right for the creature's arenas (altitude 0);
  - the arena's centre is quantised for the flight only: the in-view test and the drag away read the centre the fight
    side gives (the fighters' float middle), where the original reads `GetPos` (the quantised one) everywhere;
  - the zone test of `FlyToPosFoc` (`InsideInclusion`) on the fight's flight, as for every openblack flight;
  - the two starts of the camera's own, lingering over an arena for 1000 ms and the double click onto one: openblack
    has no query of the arenas (the list g_game +0x205C7C); TimeInArena is kept and reset as the original does, unread;
  - the middle's raise by a quarter of fn_004867B0 for each fighter (fn_004813B0's point.y less LH3DCreature vt +0x28
    of it; neither is read);
  - the keyboard move's own needs: the original applies it only with feature 8 and in mouse mode 2
    (0x45C943..0x45CAA7), and reports Drag 0x308 for it with feature 8 (0x45C986); openblack's `TiltZoom` applies it
    without either test (the orbit, when it runs, still replaces it), and no Drag goes to the camera help for it;
  - g_game +0x250538 (fn_005D2990, the interface's fight state 0x10 for the fight music and sound filter), and the
    other readers of HasFight: 0x5D4B16, `HandStateHolding::Update` 0x5B5F61, the draw fn_00456270 from 0x5CCCCC;
  - the flags +0x4C..+0x4E (set by the turn, the tilt and the edge rotate; cleared each frame in modes 0 / 2), with
    no reader found;
  - the mode seconds are not set to 0 by openblack's camera models outside the script camera (Creature Mode's, the
    temple's); which of the original's mode switches those models stand for is not read, so the orbit's ease time
    after them is not known to match;
  - the arenas' radii in play, not measured; `GArena::IsAvailable` (vt +0x2C) is approximated by the fighters being
    there;
  - (approximate) the in-view test reads the camera's current origin without the shake, where the original reads
    `LH3DCamera::GetPos`;
  - the creature's own radius (LH3DCreature +0x5228) and so the orbit's distance: `ecs::object::GetRadius` stands in
    (its creature branch is not ported), see the openblack entry;
  - a fighter's radius of 0 (no loaded mesh in openblack; the original's creatures always have a radius from
    `LH3DCreature::SetSize`), which openblack takes as the original does, IEEE floats under /fp:precise with no
    trapping (a `static_assert` on `is_iec559` beside the division). The original, at rB = 0, divides in floats
    without a crash, and only in frames with a zoom (the division is
    skipped when [esp+0xC8] == 0); without a zoom it watches on at the distance d + rA. With a zoom out the numerator
    is about 0.3 z > 0, so FD = +inf and FD > 40 ends the watch on that same frame (0x45E538), then FD is clamped to
    40; with a zoom in FD = −inf, which does not end it (FD ≤ 0 → 0); and when the numerator rounds to exactly 0
    (0.3 z lost against d + rA) FD = 0 / 0 = NaN, which compares false with 40 (fcomp unordered, so no end) and then
    clamps to 0. rA = 0 is never divided by and the original watches on. Ours matches it: the end is the same
    `FD > 40`, and the clamps make the original's two comparisons in its order (not above 0 → 0, then below 40 or 40;
    not above the lower pitch → it, then below the upper or it), so not a number goes to 0 as there. Tests:
    `WorldCameraFight.ASecondFighterWithNoRadius*`, `FightOrbit.TheDistanceCountsTheSecondFightersRadii`.
