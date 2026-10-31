# The script camera and camera tracks

## Summary

A map script can take the camera from the player: START_CAMERA_CONTROL puts a script mode on top of the camera, and
while it lives no other task can take the camera. The script then sets or moves the position and the focus, makes
either follow a thing, runs a recorded track of `Data\camera.edt`, stacks a dual camera that frames two things, shakes
what is drawn, and changes the lens. END_CAMERA_CONTROL gives the camera back to the player from where it is. A
particle with a camera (the forest miracle's) takes the camera the same way with a placed path.

openblack reproduces the script mode, the arrival rule, the following, the dual camera, the shake, the opcodes, the
release and the placed paths (the script camera, `src/Camera/ScriptCamera` and its neighbours). It has no camera mode
stack: the dual camera is a layer on top of the script mode (⚠️ approximate). The player's own follow mode after a
double click on a creature, the following of a computer player and the PC player's hand are not ported.

## The original

Everything here is read in the v1.42 `runblack.exe`, with bw1-decomp's 1.20 C as the guide; the camera code was not
changed by the patch. The camera object, its zoomers and its frame update are in
[The game camera](game-camera.md#gcamera-and-its-zoomers).

### The script mode

**Faithful.** START_CAMERA_CONTROL (0x6ECCA0), outside the citadel, calls `CameraModeScript::Create` 0x461140: if `CantExitCurrentMode`
0x441B70 there is no mode and the control fails; otherwise it makes a `CameraModeScript` (ctor 0x461180, vtable
0x8C7D5C, which inherits from `CameraModeFollow`). While it lives, `CanExit` 0x461B70 = 0: **no other task takes the
camera**. It does not touch the zoomers. `SetCameraPosition/Focus` 0x461370/0x4612B0 set them and
`MoveCameraPosition/Focus` 0x4616F0/0x461430 move them (with final speed 0); all four first release the track and the
following. `RunPath` `CameraModeScript::SetCameraPath` 0x461A80 loads `Track%d` and `UpdatePath` 0x461AB0 advances with the game ms of the frame:
position = `CameraWayRunner::Get`, focus = Bezier of the focus way in the segment and the t of the **position** runner,
and `SetPositionAndFocus` 0x4438C0 sets them.

- `CameraModeScript::Create` 0x461140 is called from START_CAMERA_CONTROL at 0x6ECCBA; the citadel never has a script mode
  (StartCameraControl 0x6ECD33).
- CameraModeScript is a CameraModeFollow built with no thing (ctor 0x44B800): time factor 0.2 (0x461189), "behind" 1
  (0x461185..0x461193), heading 0 (from `Set(0)`, 0x44BA7D), and the mode seconds from 0 (`SwitchToViewMode` from
  0x44B947).
- 🔍 inferred The ctor 0x44B800 calls `Set(0)`, which writes neither pitch nor distance; they are never read before
  `Set`, `CameraModeFollow::Set` 0x44BA90 or CAMERA_PROPERTIES write them.
- CameraModeScript +0x48 = alive: 1 from the ctor 0x461180, 0 from `Delete` 0x4611E0. `CanExit` 0x461B70 = (+0x48 ==
  0). `IsStillValid` 0x4611D0 = +0x48.
- `CameraModeScript::Update` 0x461290: `UpdatePath`, then `CameraModeFollow::Update` 0x44C160 (call at 0x4612A1).
- `SetCameraPosition` 0x461370 calls `Set(0)` (0x46137E) and sets +0x54 = −1. `SetCameraFocus` 0x4612B0 calls
  `SetCameraFocus(0)` 0x4619B0 (0x4612BE) and sets +0x50 = −1. `MoveCameraPosition` releases at 0x461702 and
  `MoveCameraFocus` at 0x461442.
- Dropping the track is `CameraModeScript::ReleasePath` 0x461A60: it frees the ScriptedCamera (`ScriptedCamera::Release` 0x446AC0) and sets +0x58 = 0.
- `RunPath` `CameraModeScript::SetCameraPath` 0x461A80 first calls `Reset` 0x461A30: +0x4C = 0, +0x48 = 1, the old ScriptedCamera freed, +0x50 =
  +0x54 = −1, +0x08 kept. Then it starts track `number` from the beginning.

### Arrival rule

**Faithful.** `GCamera::Arrived` 0x443050: with no mode, 1; otherwise vt+0x34 of the mode. `CameraModeScript::Arrived`
0x461B40: with a track, duration ≤ ms travelled; without a track, `CameraMode::Arrived` 0x441700: |position −
destination|² < 0.001 **and** |focus − destination|² < 0.001 ([0x8AA3B0]). `CameraModeNew3` also uses 0x441700 (its
vtable +0x34). It does not look at the field of view.

### Following

**Faithful** except where marked. `CameraModeFollow` is the mode the script mode inherits from.

#### The fields and Set

- Fields: +0x08 the thing followed by the position, +0x4C (Script) the thing followed by the focus (`GetFocusThing`
  0x4611F0 = +0x4C or, if null, +0x08), +0x0C heading, +0x10 pitch, +0x14 distance, +0x18 time factor (0.2 in
  Script), +0x1C "behind" (1 in Script).
- `Set(thing)` 0x44BA00: heading and pitch of the zoomers' **destinations** (`GetHeadingAndPitchFromPoints` 0x4428D0),
  distance = height · 8 (`GetThingViewingDistance` 0x441F20); with "behind", heading 0. `CameraModeFollow::Set(thing, d)`: the
  same with distance d and without setting the heading to 0. `CameraModeFollow::SetToDestinationPosition` 0x44BB30 places at once (`Zoomer::SetPosition`)
  and sets GCamera+0x68 = 2.
- `GetThingViewingDistance` 0x441F20 = GetHeight (vt +0x42C) × 8 ([0x8C7108]).
- Set/Move of a point release the following on its side (0x461370 `Set(0)`, 0x4612B0 `SetCameraFocus(0)`);
  `RunPath` releases only +0x4C.
- `SetCameraFocus(thing)` 0x4619B0 first drops the track (0x4619B4). Then +0x4C = the thing if `IsAvailable` (vt
  +0x2C) is 1, else 0 (0x4619BD..0x4619DC).
- `GameThing::IsAvailable` 0x401810 = !(byte +0xA & 1). Only `SetCameraFocus` (0x4619BD) and `Validate` (0x461273 /
  0x44BB13) read it here.
- `Validate` (0x461270 + 0x44BB10, **once per turn** from `GGame::ProcessTurn` 0x54E74E) releases the thing that is
  no longer there.

#### One frame of the follow

`CameraModeFollow::Update` 0x44C160, every frame:

- The distance is clamped to 2..1500 and stored: 2 ([0x8C785C]) .. 1500 ([0x8C78E4]); ≤ 2 → 2, < 1500 → itself, else
  1500 (0x44C16B..0x44C1A2).
- T = (+0x68 > 2 ? 1 : 2 − +0x68 / 2) · factor (0x44C1A5): 0.4 s after the mode change, 0.2 s after 2 s; a factor of 0
  places at once. The constants: the threshold 2 s [0x8C7874], 1 [0x8C7870], 2 [0x8C786C]; "≤ 2" takes the slow start
  (0x44C1A5..0x44C1E1).
- The focus goes towards the thing's point plus half its height. The point is MapCoords +0x14, x and z × 1/6553.6
  ([0x8AA3A4]), y = `LH3DIsland::GetAltitude` 0x803090 + the altitude above the land +0x1C (0x44BCD2..0x44BD03). In
  `Update`, an Object with a Game3DObject (+0x40) uses that matrix's position (Game3DObject +0x38) instead
  (0x44C513..0x44C539, 0x44C715..0x44C73B), but `CameraModeFollow::SetToDestinationPosition` 0x44BB30 does not. For villagers and animals that matrix is the one
  drawn between turns (`Living::PreDrawScale` 0x51AF00). A flock uses `Flock::GetFlockPos` 0x530570 with the leader's half height.
- The position is `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from that point with the distance, the pitch and
  the heading. The pitch is at least [0x8C78E0] = 0.241661 (0x44C758..0x44C778, 0x44BF7E), stored back.
- The clear view (Ctrl and Shift, with "behind" off, the auto reposition on and no key move): `CameraModeNew3::SuggestBestCameraPos` 0x458E50 at 0x44C867,
  that is `FindBestAngle` 0x458F40, whose land samples along each of the 32 headings are j = 3..7 only (`mov edi, 3`
  at 0x458F75, `cmp edi, 8` at 0x459005), at j × distance × 0.125: from three eighths out to seven eighths, never at
  the focus or the first two eighths. openblack: `creature_follow::ClearHeading` samples the same eighths (test
  `CreatureFollow.ClearViewLooksFromThreeEighthsOut`); it read 0..7 until 2026-10-21.
- With "behind" on a MobileWallHug the heading is heading − (the object's angle − π/2) (0x44C785): GameAngle (+0x5C)
  × 2 × [0x8C78DC] (π/2048) − [0x8C78D8] (π/2) (0x44BFD1 / 0x44C7B1, 0x44C775..0x44C7C1). CAMERA_PROPERTIES writes the
  script mode at 0x6EE0C1..0x6EE0D9; "behind" is tested `!= 0` at 0x44C7E6.
- A creature as the focus thing writes 0xCC62E4..0xCC6310 (1 − life, its body's +0x1C / +0x30) in
  0x44C1E5..0x44C245, read elsewhere. Not ported.
- Without a position thing, `Update` follows the computer player: `GetComputerPlayerFocus` (vt +0x4C) != −1
  (0x44C59C..) and `GetComputerPlayerFollow` (vt +0x50) != −1 (0x44C905..). `CameraModeFollow::SetToDestinationPosition` 0x44BB30 has the same branches
  (0x44BD52.., 0x44C056..). Not ported.
- `CameraModeFollow::SetToDestinationPosition` 0x44BB30 only runs with +0x20 == 0 (0x44BB46). It places with `Zoomer::SetPosition` 0x441AC0 (and an inline copy
  for x) and does not clamp the distance (0x44BF73..0x44C13C).
- FollowUs (Land 1's sharks): `SET_FOCUS_AND_POSITION_FOLLOW(Son, 3)` and `CAMERA_PROPERTIES(3, 0, 22.5, true)`: the
  camera sticks to the child, 22.5° relative to where it is facing.

#### Facing an object

- `GScript::MoveCameraToFaceObject` 0x6ED600 at 0x6ED710 (opcodes 106 / 107): focus = the MapCoords point + half the height, with no flock and no Game3DObject
  (0x6ED728..0x6ED76F); the position at distance d with the heading `GetFacingDirection` (vt+0x4EC) and pitch 0.1
  (0x6ED7D0).
- The heading (0x6ED774..0x6ED7D9): a heading ≥ 8π ([0x942190]) logs "Invalid heading" (0xC0C208) and carries on.
  While it is > 2π ([0x8AB210]), 2π is subtracted.
- With no object, `GScript::MoveCameraToFaceObject` 0x6ED600 at 0x6ED710 logs "no object to face" (0xC0C218) and then reads through the null thing (inferred: it
  would crash) (0x6ED717..0x6ED725).
- `GetFacingDirection` (vt +0x4EC): GameThingWithPos 0x4024B0 = 0; MobileWallHug 0x60C020 =
  ConvertGameAngleToScawenAngle(+0x5C); Creature 0x477EC0 = its LH3DCreature's angle (+0x160 → +0x58 → +0x84) + 2π −
  2.5: `fld` [+0x84], `fadd` dword [0x8AB210] (2π as a float, 0x40C90FDB), `fsub` dword [0x8C581C] (2.5), each step
  rounded to a float at the game's 24-bit precision. ✅ Oracle, 2026-10-21: 410 yaws (ten picked, 400 drawn in
  ±6.3) through 0x477EC0 give float(float(yaw + 2π) − 2.5) every time; one rounding of the whole sum misses 53 of
  them (yaw 4.0 gives 0x40F90FDC, the single rounding 0x40F90FDB).

#### Heading and pitch from two points

`GetHeadingAndPitchFromPoints` 0x4428D0(a, b): v = a − b. If |v.x| and |v.z| are both below [0x8C7620] = 0.01 (a
double), heading 0 and pitch 1.5393804 (0x3FC50A6B, 0x442925 / 0x44292B). Otherwise heading = π ([0x8C36A0]) −
`heading_from_direction_vector`(v), as a 24-bit `fsubr` (0x44293F). `heading_from_direction_vector` 0x7FAA50 gives 0 when x² + z² ≤ 1e-6, else `atan360`(−z, x).
Pitch = `atan360`(sqrt(x² + z²), y) (0x442950..0x442976).

#### A point from a distance, a heading and a pitch

`SetPointFromPointDistanceHeadingAndPitch(out, p, d, h, q)` 0x442810 (0x442811..0x442856): `fcos` of q stored as a
float [esp] (0x44281B); then on the stack z = `fcos`(h) × [esp] × d + p.z, y = `fsin`(q) × d + p.y, x = `fsin`(h) ×
[esp] × d + p.x, each a product or sum in that order, and the three `fstp` to out at the end (0x442851..0x442856).

- **Precision.** The game thread's FPU runs at 24 bits (see
  [Floating point](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)): `fsin` / `fcos` are not touched by the
  precision control and give the full 64-bit sine; each `fmul` / `fadd` rounds the mantissa to 24 bits (the exponent
  stays extended, which changes nothing at the camera's sizes), and the last `fstp` is then exact. So only cos q is
  rounded to a float before a product; sin q, sin h and cos h are not: x = float(float(float(sin h × float(cos q)) ×
  d) + p.x), y = float(float(sin q × d) + p.y), z = float(float(float(cos h × float(cos q)) × d) + p.z).
- **Evidence.** In exact arithmetic (the sine and cosine to 64 bits, each step rounded to 24), the double click's
  flight (DoubleClickFlyTo: p = (1080.70996, 0, 966.307434), d = 100, h = 0, q = 0.53133231, 0x3F080565) gives
  (1080.70996, 50.6682434, 1052.52075) = (0x448716B8, 0x424AAC48, 0x448390AA), the recorded flyToTargetOrigin to the
  bit in all three axes. With sin q rounded to a float first (glm's `euclidean`), the height is 50.6682396, one float
  step low; x and z are the same either way there (sin 0 and cos 0 are exact). Done in double (the sine and cosine in
  double, each product with one of them rounded to a float, the rest in float), the result is the exact model's on
  100 000 random points, distances, headings and pitches in all three axes; with the sine and cosine rounded to floats
  first, 27 370 of them differ.
- The callers (every `call 0x442810`): `GCamera::GCamera` (0x4418EC, 0x4419C0), `GetHeadingAndPitchFromPoints`
  0x442A03, CameraModeCitadel vt +4 0x44A400, CameraModeFlyAndClick vt +4 0x44AFF0, `CameraModeFlyAndClick::MoveToDestination` 0x44B2E0,
  `CameraModeFollow::SetToDestinationPosition` (0x44BFFC, 0x44C0A2), `CameraModeFollow::Update` (0x44C7DB, 0x44C8BA, 0x44C9A7),
  `CameraModeFollowHeading::Update` 0x44DA9F, `CameraModeFree::Update` 0x44E65F, `CameraModeNew3::CameraModeNew3` 0x45772D,
  `ZoomToCitadel` (0x457D69, 0x457EB7), `CameraModeNew3::SuggestBestCameraPos` 0x458F33, `FindBestAngle` (0x458FB1, 0x45908A),
  `CameraModeNew3::Update` (the clear view 0x45B27A and 0x45B601, the double click 0x45E29C, the fight 0x45E89C,
  mouse mode 2 0x45E9D1, mouse mode 1 0x45EF0E, the fixed rotation 0x46019F), `CameraModeTwoObjects::Update`
  0x46213E (the dual), `MoveCameraToFaceObject` 0x6ED7D9, `Town::SetTownArea` 0x73AC6D, `AdjustBubbleZForPitch`
  0x78A10E.
- Mouse mode 2 (a camera input without a grip, 0x45E8FB..0x45E9D1) and mouse mode 1 (0x45ED92..0x45EF0E) build the
  frame's origin [esp+0x14] from +0x12C, the distance [esp+0x4C], the heading [esp+0xE0] and the pitch kept within
  −π/6 (0xBF060A92 at or below −0.523599 [0x8C7CDC]) and [esp+0xE8].

#### The player's follow mode

The same `CameraModeFollow` is the player's mode after a double click on a creature (`IsCreature` vt +0x34 at
0x45DE4B, `CameraModeFollow(camera, object, 1.0, 0, 0)` at 0x45E34B, constructor 0x44B800). Read in bw1-decomp's C
(`src/Black/CameraModeFollow.cpp`), with the constants seen among the exe's reads (xref):

- `CameraModeFollow::IsStillValid` 0x44B960 (vtable +0x14): the thing is there and `IsAvailable` (vt +0x2C) is 1.
- `CameraModeFollow::ProcessKeyMovement` 0x44CFE0 (vtable +0x28, shared by CameraModeScript): the forward / backward
  and left / right actions give a key move of ±400 × the camera's frame time; the zoom actions take the wheel delta,
  or ∓120 without one, then clear it; the distance becomes at least `CameraFollowMinViewingDistance`, times (wheel ×
  −0.15 + 500) / 500. A key move with Ctrl turns the heading by −move × π / screen width and scales the distance by
  (move + 500) / 500; with Shift it turns the heading the same way and tilts the pitch by `CameraFollowKeySpeed`,
  kept between `CameraFollowMinPitch` and `CameraFollowMaxPitch`; with neither, it makes a `CameraModeNew3` (the
  player takes the camera back). The distance is then kept between the minimum and 1000. The land grip (hand state
  `HAND_STATE_GRIP_LANDSCAPE`) also makes a `CameraModeNew3`. Its constants (read at 0x44D112..0x44D2C0) [V]:
  `CameraFollowMinViewingDistance` [0x8C785C] = 2.0, `CameraFollowMinPitch` [0x8C7860] = −π/4 (0xBF490FDB),
  `CameraFollowMaxPitch` [0x8C7864] = 1.3744469 (0x3FAFEDE0, 0.4375 π), `CameraFollowKeySpeed` [0x8C7868] = 0.002;
  the key move is ±400 ([0x8C772C] / [0x8C7728]) × the time step, truncated (`__ftol`); a wheel step with no delta
  counts ±120; the distance factor is (wheel × −0.15 + 500) × 0.002 ([0x8C78F0], [0x8C78EC], [0x8C78E8]).
- **The camera mode's seconds** (GCamera +0x68, which `GCamera::Update` adds the frame time to). The follow's
  constructor ends with `SwitchToViewMode` (0x44B947), which puts it on the stack and sets +0x68 = 0 (0x441D30),
  unless `CantExitCurrentMode` (0x44B816) holds, when it deletes itself and nothing changes. Its other builder is
  `GGame::DoAction`'s creature action (0x63FB4B, a CameraModeFollow on the player's creature, its last argument 1 when the
  camera is nearer than 80.0 to it in x/z, 0x63FB25); the same action on a follow of that creature pops it (`PopViewMode` 0x63FA3D, which
  sets +0x68 = 0 at 0x441C86) and puts a new `CameraModeNew3` on top when the mode under it is not one (0x63FAAA).
  The keys alone and the land grip make a `CameraModeNew3`, whose constructor's `SwitchToViewMode` sets +0x68 = 0 too.
  A follow whose thing is gone is dropped by `CheckStackedModesForValidity` 0x441D40: it removes every mode whose
  `IsStillValid` (vt +0x14) is 0 and, when the top went, restarts the new top (vt +0x10) without touching +0x68; only
  an emptied stack gets a new `CameraModeNew3` (0x441DD4). So taking the camera, the creature action letting go, the
  keys and the grip each start the mode's seconds again; the thing going does not.

### Dual camera

**Faithful** except where marked (`CameraModeTwoObjects`, 0x30 bytes, vtable 0x8C7DD0, "Dual Cam"). It is stacked on
top of the script mode (ctor 0x461BB0; with a point `CameraModeTwoObjects::CameraModeTwoObjects` 0x461CB0; one equal to the current one deletes itself).

- `Update` 0x461DE0, every frame: T = (+0x68 > 1.5 ? 1 : 2 − +0x68 / 1.5), **without the factor**; A and B = the
  MapCoords of the two things (or the point); focus = the midpoint raised by the mean height · 0.5; distance =
  ((separation in x/z + the two `Get2DRadius`, 30 if it is not an Object) · factor (1, or 1.2 with a point) + the
  larger height · 1.4; heading = π/4 − the direction of B − A; pitch π/8; the zoomers go towards those destinations
  in T.
- 093 START (0x6ED2E0), 094 UPDATE (0x6ED370, `SetObjects` 0x461C90), 095 RELEASE (0x6ED410: `Delete` and
  `PopViewMode`, GCamera+0x68 = 0, only when the current mode is a `CameraModeTwoObjects`, `__RTDynamicCast` at
  0x6ED43D), 105 WITH POINT (0x6ED460, without checks). The end of control (`GScript::StartCameraControl` 0x6ECCA0 at 0x6ECD70)
  removes a dual before deleting the script mode. With the dual on top, the script opcodes find "the wrong mode".
- Every turn, `CheckStackedModesForValidity` 0x441D40 removes the dual whose things are no longer there
  (`IsStillValid` 0x461D90).
- Constants (ctor 0x461BB0 / `CameraModeTwoObjects::CameraModeTwoObjects` 0x461CB0): heading +0x20 = π/4 (0x3F490FDB), pitch +0x24 = π/8 (0x3EC90FDB), distance
  factor +0x28 = 1 (0x461C2C) with two things or 1.2 (0x3F99999A, 0x461D62) with a point. Debug name string at
  0x461C60.
- Fields: +0x08 thing A (0x461BC8), +0x0C thing B (0x461BCB), +0x10 the point (0x461CCC..0x461CDF), +0x1C two things
  (1) / point (0) (0x461BCE / 0x461CE2), +0x2C alive (1 at 0x461C25..0x461C41).
- T constants: 2 ([0x8C7DBC]) right after the mode change, down to 1 ([0x8C7DC0]) at 1.5 s ([0x8C7DC4])
  (0x461DED..0x461E26).
- A and B are MapCoords points (x, z × 1/6553.6, GetAltitude 0x803090 + the altitude +0x1C), not the Game3DObject nor
  a flock's GetFlockPos. With a point, B has no +0x1C (0x461E2A..0x461EB2).
- The midpoint is (B + A) × 0.5 ([0x8AA3B4], 0x461EB6..0x461F10). With a point, the second height in the mean is 1
  (0x461F2A).
- The pitch is kept ≥ [0x8C78E0] and stored back (0x462003..0x462021).
- The radius of a thing that is not an Object is 30 (0x46204A, [0x8BF51C]). An Object's comes from `Get2DRadius` (vt
  +0x64) through `dynamic_cast<Object*>` (0x462031 / 0x462067). The larger height is multiplied by 1.4 ([0x8C7E18],
  0x4620B1).
- Heading = +0x20 − the direction of v = B − A (`heading_from_direction_vector` 0x7FAA50, a 24-bit `fsubr`, 0x46211C). It stays +0x20 when |v.x|
  and |v.z| are both ≤ 0.01 ([0x8C7A10], a double; 0x4620E9 / 0x4620FC).
- The position heads for `SetPointFromPointDistanceHeadingAndPitch` 0x442810 from the focus (0x462126..0x462318).
- START_DUAL_CAMERA checks neither the camera mode nor the citadel. START / UPDATE_DUAL_CAMERA pop b then a; either
  missing → "Thing invalid for dual cam" (0xC0C1EC) and nothing.
- `IsStillValid` 0x461D90 = +0x2C while +0x08 (and +0x0C with two things) is still there and `IsAvailable` is 1.
  Otherwise `CheckStackedModesForValidity` deletes it (vt+0 with 1, 0x441D86), walking from the bottom of the stack
  (0x441D57). It is called from `GGame::ProcessTurn` 0x54E743.
- When the current mode is deleted, the new current mode gets Restart (vt +0x10, 0x441DEB). Restart does nothing for
  CameraModeScript (0x44A390) and CameraModeTwoObjects. The mode seconds are not reset there.
- RELEASE: `CameraModeTwoObjects::Delete` 0x461C50 (vt +0x30, called at 0x6ED44D) sets +0x2C = 0.
  `GCamera::PopViewMode` 0x441C50 calls Cleanup (vt +0x18, nothing), deletes the mode, lowers the index and sets
  +0x68 = 0 (0x441C86).
- The end of control (`GScript::StartCameraControl` 0x6ECCA0 at 0x6ECD70) removes only one dual camera (`ReleaseDualCamera` 0x6ED410 at 0x6ECDB1). It deletes
  the script mode only if that mode is current (0x6ECDD0..0x6ECE2E). With another mode current (a second dual camera,
  or the player's), it logs "We are in the wrong camera mode! - excep" (0xC0C14C) and the script mode stays.

### Shake

**Faithful.** SHAKE_CAMERA 201 (0x6EE0F0) → `PSysGlobal::StartCameraShake` 0x68F400 →
`LH3DCameraChecker::Create` 0x821050 (radius, point, amplitude, ms). It is applied by `LH3DCameraChecker::AdjustCameraPosTarget` 0x8210C0 from `LH3DTech::UpdateCamera` 0x819920, **only
to what is drawn** (never to the zoomers): the shake closest to the camera, if it is within its radius (no falloff
with distance); amplitude = remaining / total · amplitude; six `Random` 0x81D180 rolls (pos.z, pos.y, pos.x, focus.z,
focus.y, focus.x), or two with "only y". Each drawn frame (`LH3DCameraChecker::Process` 0x821270) subtracts `g_delta_time`, and the shake is
freed on reaching 0. The `Random` rolls are the C runtime's stream
([Random streams between turn and frame](../engine/random.md#random-streams-between-turn-and-frame)).

- `LH3DCameraChecker` is 0x24 bytes: `LH3DMem::Alloc(0x24)` zeroed, then put at the head of the list g_first
  [0xEB99A8] (0x82106D..0x821078), newest first.
- Its fields: +0x04 max distance (0x82107E), +0x08 point (0x821085..0x821095), +0x14 amplitude (0x8210A0), +0x18 total
  ms (0x8210A7), +0x1C remaining ms (0x8210AA), +0x20 "only y" (0x8210AD).
- `StartCameraShake` 0x68F400: ms = seconds × 1000 ([0x8AB228]) stored as a float, then `fistp`
  (0x68F406..0x68F41E). It always passes "only y" = 0 (0x68F426).
- `LH3DCameraChecker::AdjustCameraPosTarget` 0x8210C0 (called from `UpdateCamera` at 0x819A0B) does nothing with no shakes or with [0xC383B8] == 0 (0x8210CB /
  0x8210D3). That flag is 1 in .data and never written.
- The nearest shake is measured from g_camera ([0xEA1DB8] / [0xEA1DC4]), the camera drawn the frame before. A later
  shake wins only if strictly nearer, so the newest wins ties (0x8210E0..0x821195). The distance is `LHPoint::Sub` 0x4C2B90
  (subtract) and `LHPoint::GetNorme` 0x4A1BA0 = sqrt((z·z + y·y) + x·x) (0x4A1BA8..0x4A1BB8).
- a = remaining / total × amplitude (0x8211A9..0x8211B7). A shake of 0 ms divides 0 by 0 (`fidiv` 0x8211B1) and
  draws a NaN camera until it is freed.
- Roll order: "only y" rolls position y then focus y (0x8211D1..0x8211F0). Otherwise six rolls: position z, y, x,
  then focus z, y, x (0x8211F7..0x821264).
- bw1-decomp names 0x8210C0 `LH3DCameraChecker::AdjustCameraPosTarget`; 0x437E70 is `GBelief::GetBeliefInPlayer`.
- The countdown `LH3DCameraChecker::Process` 0x821270 runs from `LH3DRender::StartFrame` 0x82F270 (sub g_delta_time, `jg` keeps it,
  0x821287..0x8212CE).
- `AddSoundToAtom` 0x69DDBD: a particle sound rule's shake, its ms cut towards zero and up and down only
  ([Sound of the particles](../audio/magic-and-weather-sounds.md#sound-of-the-particles)).
- `CameraModeNew3::Update` 0x45FC84, the fence putting the player's camera back (100, 1.0, 400 ms):
  `DefaultWorldCameraModel::KeepInsideFence` ([the player camera](player-camera.md#the-original)).
- Other shake creators, not ported: fn_006E63C0 0x6E6453.

#### The debug camera

g_camera_mode [0xEA9EC8], read by `UpdateCamera` at 0x81997A before the shake (0x81997A..0x8199DD): 2 draws the camera
from [0xEA1B58] towards [0xEA1B68], 1 only turns the focus to [0xEA1B68], any other value does neither. GCamera's
zoomers are not touched. Writers of [0xEA9EC8]: PLAY_JC_SPECIAL 1 / 2, `Intro::ReleaseAll` 0x5DFCCA and
`CleanGameForScriptReboot` 0x6EBC0C (0). The GCameraEditor's writers are debug only.

### Opcodes

**Faithful** except where marked. Those that move check the mode: with no mode "Script camera has been removed!"
(0xC0C0CC); with another mode "We are in the wrong camera mode!" (0xC0C0EC), and they do nothing;
`SET_CAMERA_POSITION` 0x6EC8F0 says nothing. 003 / 004 / 287 also have a "Script moving camera in citadel" note
(0xC0C110).

| CHL | Original | What it does |
|---|---|---|
| 001 / 002 SET_CAMERA_POSITION / FOCUS | 0x6EC8F0 / 0x6EC9A0 | sets (without the script mode it does nothing) |
| 003 / 004 MOVE_CAMERA_POSITION / FOCUS | 0x6ECAA0 / 0x6ECBA0 | moves in t s of wall-clock time |
| 005 / 006 GET_CAMERA_POSITION / FOCUS | 0x6ECFF0 / 0x6ED0B0 | the drawn camera's (`LH3DTech::g_camera` +0x0 / +0xC); inside the citadel (g_game+0x205A28 == 1, 0x6ED020 / 0x6ED0E0) `SetPointOutOfWorld` 0x6EB890 (its only two callers): (50000, 50000, 50000), [0x942164] on every axis |
| 035 HAS_CAMERA_ARRIVED | 0x6ED170 | the arrival rule |
| 119 RUN_CAMERA_PATH | 0x6ED7F0 | a camera.edt track |
| 279 SET_CAMERA_LENS | 0x6EE2E0 | **`SetCameraFov(70°, x)`: the argument is the time** (copied); 70° is fn_00443670's 1.22173047 [0x8C762C] (confirmed: xref constants) |
| 280 MOVE_CAMERA_LENS | 0x6EE280 | FOV = lens · 0.0174533 in t s of game time |
| 283 / 284 STORE / RESTORE_CAMERA_DETAILS | 0x6EE330 / 0x6EE390 | stores what is drawn / `SetPositionAndFocus` |
| 286 / 287 SET / MOVE_CAMERA_POS_FOC_LENS | 0x6EE3C0 / 0x6EE4B0 | position, focus and FOV, **the lens not converted to radians** (copied; no map uses them). 286 pops the lens, the focus, then the position, and calls `SetPositionAndFocus` 0x6EE47B whatever the mode, then `SetCameraFov(lens, 0)` 0x6EE497; 287 pops the time, the lens, the focus, the position, and only in the script mode moves both in t and calls `SetCameraFov(lens, t)` 0x6EE61C |
| 314 / 315 GET_STORED_CAMERA_POSITION / FOCUS | 0x6EE630 / 0x6EE6A0 | what was stored |
| 377 GET_FACING_CAMERA_POSITION | 0x6EE710 | `g_camera` +0x0 + d · `g_camera` +0x1C (`LookAtVector`), per component: the unit view direction UpdateWorldToCamera writes (0x819773..0x819782), normalize(focus − eye) with dx put to ±1e-4 first when \|dx\| and \|dz\| < 1e-4 ([visible blocks](../rendering/visible-blocks-and-lod.md)). ✅ openblack: the drawn position + d · the view direction of `affine::WorldToCamera` (the same normalisation and nudge), per component, since 2026-10-26; pinned bit for bit to the exe's results (oracle: UpdateWorldToCamera then this native, a slant, straight down from 300 m and 1 m, the focus on the eye) by `ScriptCameraDetails.AFacingPositionGoesAlongTheViewDirectionAsTheOriginalRoundsIt` |
| 049 / 276 FOCUS_FOLLOW / SET_FOCUS_FOLLOW | 0x6EDF30 / 0x6EDB40 | the focus follows the thing (0x4619B0) |
| 050 POSITION_FOLLOW | 0x6EDE70 | the position follows the thing (`Set` 0x44BA00) |
| 277 SET_POSITION_FOLLOW | 0x6EDA80 | `Set` + place at once (`CameraModeFollow::SetToDestinationPosition` 0x44BB30) |
| 178 / 278 (SET_)FOCUS_AND_POSITION_FOLLOW | 0x6EDDA0 / 0x6ED9B0 | `CameraModeFollow::Set(thing, d)` (278 also places at once) |
| 180 CAMERA_PROPERTIES | 0x6EDFF0 | distance, time factor, heading (° · 0.0174533), "behind" |
| 106 / 107 SET / MOVE_CAMERA_TO_FACE_OBJECT | 0x6ED500 / 0x6ED600 | facing an object, set / move in t |
| 203 SET_AVI_SEQUENCE | 0x6FC050 | on 1: `data\intro.bik` full screen (confirmed: xref immediates) (`PlayFullScreenMovie` 0x54D920, the game paused and given back at 58 s); on 2: the falling spell's film; off 2 ends that film; a start also removes the fade to black at once (`SetupScreenFadeBackToNormal(0)` 0x6EBB00) ([film playback](../video/playback.md#schedule-5-s-fade-and-the-intro-at-60-s), [the falling spell](../video/playback.md#the-falling-spell-fallbik)) |
| 093 / 094 / 095 START / UPDATE / RELEASE_DUAL_CAMERA | 0x6ED2E0 / 0x6ED370 / 0x6ED410 | the dual camera |
| 105 CREATE_DUAL_CAMERA_WITH_POINT | 0x6ED460 | the dual camera with a point |
| 201 SHAKE_CAMERA | 0x6EE0F0 | the shake (only what is drawn) |
| 142 SET_CAMERA_ZONE / 150 GET_INCLUSION_DISTANCE | 0x6ED890 / 0x6ED990 | the player camera's zone ([Zones and fixed rotation](player-camera.md#zones-and-fixed-rotation)) |
| 209 SET_FIXED_CAM_ROTATION | 0x6EE1A0 | the player's fixed rotation point (the same section) |

Not ported: the PC player following 372 / 373 (0x6EDC00 / 0x6EDCD0).

#### The field-of-view tests

The field-of-view tests 011 / 012 are used by the intro
([Timers, help events and field of view](../interface/advisors-and-help.md#timers-help-events-and-field-of-view)):

- 011 GAME_THING_FIELD_OF_VIEW = `GScript::IsGameThingFieldOfView` 0x6F8130. 012 POS_FIELD_OF_VIEW =
  `GScript::IsPosFieldOfView` 0x6F8060.
- Both use the drawn camera's `LH3DTech::g_world_to_clipping` [0xEA9E40], the near plane [0xE839E0] and the screen of
  g_info_transform.
- 012: false inside the temple or with no view (0x6F80ED..0x6F8109), else the point test (0x6F810F). "Inside the
  temple" is g_game+0x205A28 == 1 (0x6F80ED; 011 reads the same field), the citadel's sequence mode. openblack
  (2026-10-28): both tests read `game_clock::IsInsideCitadel` (`field_of_view::PosInView`, `ThingInView`); until
  then they took openblack as always outside, so a script asking from inside the temple got the camera's answer. Test
  `TimersEventsTest.APointIsNeverOnTheScreenInsideTheCitadel`.
- 011: false with no thing or inside the temple (0x6F819F..0x6F81B4). For an Object (`dynamic_cast`, 0x6F81BA), its 3D
  object's (+0x40) mesh (vt +0xF8; no mesh → false) bounding sphere goes through `IsObjectOnScreen` 0x81F1A0 →
  `LH3DBoundingBox::CheckRegionOnScreen` 0x868C80, with the box centre at 0x868CBC. Any other GameThingWithPos uses its
  point (x, GetAltitude + its +0x1C, z) (0x6F81F4..0x6F822B).
- Which branch (read 2026-10-28): the cast is to `Object`, so every Object takes the sphere and every other
  GameThingWithPos the point, whatever it draws: a street lantern (`GStreetLantern`, an Object) and a spell seed
  (`SpellSeed`, an Object) the sphere; a field (an `Abode`), a bonfire (a `Rock`) and a fish farm (a `MultiMapFixed`)
  the sphere too; a script's marker, a timer, a mist, an influence ring, a spell, a town, a flock or a dance the point.
  openblack (2026-10-28): `field_of_view::IsObjectForTheScreenTest` (`script_type::IsObject` less a script's marker and
  a dance, which that test still counts as objects) picks the branch; until then it was "has a Mesh", so an object
  openblack keeps no Mesh for (a fish farm) was tested at its point and a non-object with a Mesh at its sphere. An
  object with no Mesh is never on the screen, as an Object whose 3D object has no mesh. Test
  `ScriptQueryNatives.FieldOfViewTakesAThingAsAnObjectByWhatItIsNotByItsMesh`.
- The point of a thing that is not an object (0x6F81F4..0x6F822B): `fild` of its Pos x and z × 10/65536 (0x8AA3A4),
  y = `GetAltitude(Pos)` + its altitude +0x1C, each stored as a float. openblack (2026-10-28):
  `field_of_view::MapPointOf`, the Transform position through `map_coords::FromWorld` and `ToWorld` (x and z × 6553.6
  truncated and back, the height above the land under the truncated point and the land added again), the point the
  game keeps for that position; until then the raw Transform position, up to a fixed-point unit (0.00015 m) off in x
  and z. Test `ScriptQueryNatives.FieldOfViewTestsAThingThatIsNoObjectAtTheGamesPointOfIt` (a point left of the
  screen's edge comes back onto its first column).
- The things with no place of their own: a `ScriptTimer` is made at (0, 0, 0) (fn_007115A0, `SetToZero` 0x5705D0;
  [Script timers](../interface/advisors-and-help.md)), so its point is (0, `GetAltitude` at the map's corner, 0); a
  dance's Pos is the place it is made about (`Dance::Initialise` 0x50B7A0 at 0x50B84C..; `Dance::Process` copies its
  target's Pos only while it follows one, 0x50BBF1..0x50BC0A, which openblack's dances do not yet). openblack
  (2026-10-28): `field_of_view::PointOfThing` gives a timer, which has no Transform, the land at the corner and a dance
  its `Dance::place`; until then both were never on the screen. Test
  `ScriptQueryNatives.FieldOfViewTestsATimerAtTheMapsCornerAndADanceAtItsPlace`.
- The sphere (read 2026-10-24): `IsObjectOnScreen` 0x81F1A0 passes the mesh's box (mesh +0x14) and the 3D object;
  the radius is the object's scale +0x44 times the box's +0x1C, the mesh's half diagonal (mesh +0x30, `fld`/`fmul`
  at 0x868CA6..0x868CA9, one product), the centre the box's centre (mesh +0x18..+0x20) through the object's matrix
  (0x868CBC..0x868D04), and the eye-inside test takes the object's own position (+0x38..+0x40, 0x868DA1). openblack
  (2026-10-24): `field_of_view::ObjectSphereRadius`, the Transform's uniform scale times `ecs::object::HalfDiagonal`
  of the union box's half extents, as the original's `ComputeBoundingBox`; until then the radius was the box's full
  diagonal halved after the root, times the largest of the three scales. Test
  `FieldOfView.AnObjectsSphereIsItsScaleTimesItsMeshsHalfDiagonal`. Since 2026-10-28 the centre goes through the object's own
  placement's cells, the radius takes its scale and the eye's test its position (`object_placement::Of`,
  [the object's own placement](../engine/matrices-and-zoomer.md#the-objects-own-placement)): an upright object at a
  scale other than 1 had cells one bit off the original's in 40% of turns. The help bubble's sign and the guidance's
  building remark go through the same test. A villager or animal has no placement of its own: its test reads its
  turn's Transform (🎯 chosen difference, 2026-10-28).

#### Seeing the camera

339 GAME_THING_CAN_VIEW_CAMERA = `GScript::GameThingCanViewCamera` 0x6FE4D0 (no bw1-decomp C; Ghidra's and the
assembly, read 2026-10-28). Used by the tutorial island's control script (the guide, 3 calls) and the Shaolin quest
(the monk), all at 180 degrees.

- It pops the degrees (a float), then the thing. No thing, or a thing that is not an Object (`IsObject` vt +0x460):
  "Thing not valid!" (0x6FE515) and 0. `GetCamera` is called and its answer unused; the camera is
  `LH3DTech::g_camera`'s position (0x6FE551..0x6FE56E), the drawn one.
- A creature (vt +0x34): `Creature::IsLookingTowardsPos` 0x477520 with the camera's `MapCoords` (0x603160: x and z
  × 6553.6, truncated) and `ftol(360 / degrees)` (0x6FE587..0x6FE591, a float division). It takes
  `LH3DCreature::GetLookDirection` 0x480C90 (towards the look target, or the body's yaw without one) into
  `Creature::IsDirectionClose` 0x477550: game angles `ConvertScawenAngleToGameAngle(look)` and `GetAngleFromXZ(its
  Pos, camera)`, both 16-bit, their difference taken straight (no fold round the circle), close when it is below
  `0x800 / parts` (unsigned division, signed compare, 0x47759C..0x4775A2), or when the camera's cell x and z (the high
  words) are the creature's. Parts 0 (more than 360 degrees) divides by zero.
- Any other object: the point (Pos x × 10/65536, GetAltitude + its altitude, Pos z × 10/65536) (0x6FE5B6..0x6FE5E9),
  its Y angle `GetYAngle` vt +0x508 (`Object::GetYAngle` 0x402500, +0x4C, in every Object class), the half angle
  (degrees × 0.5) × 0.0174532924 (0x6FE5FB..0x6FE615, floats), into fn_0074F3D0 (point, angle, half, camera):
  d = camera − point in x and z (float differences); for e = angle − half, `dz·cos e − dx·sin e < 0` (or unordered) →
  0 (0x74F3F5..0x74F434); for e = angle + half, `> 0` → 0, else 1 (0x74F440..0x74F481, an unordered result passes).
  The cosine stays on the FPU stack (extended), the sine is stored as a float (0x74F40D, 0x74F450); the height is not
  used. So at 180 degrees it is the half of the ground in front, along (cos angle, sin angle); below a half turn the
  angle in front; above it the two lines no longer bound a cone, and at a full turn only a camera on the line counts.
- openblack (2026-10-28): `field_of_view::ThingCanViewCamera` (`src/Camera/FieldOfView.cpp`). A creature: its map
  position and the camera's (`map_coords::FromMetres`), `creature_perceived_desires::LookYaw` of its head's look target
  and `creature_pose::BodyYaw`, `creature_player_attention::IsDirectionClose` with `CreatureViewParts` (360 / degrees,
  `FtoL`; 0 answers false, an openblack guard where the original faults). Any other object: its map position
  (`map_coords::Quantise` of x and z), its Y angle (`villager_physics::ObjectYAngle` for a villager or an animal, else
  `placed_turn::AnglesOf`, the angle GET_PROPERTY reads), `HalfAngleOfDegrees` and `CameraWithinAngle`, the cosine in
  double and the sine a float. 20 000 random cases on fn_0074F3D0 through the oracle (a fifth on an edge), 0
  differences; 20 of them pinned in `FieldOfView.AThingSeesTheCameraWithinTheAngleAsTheOriginalWorksItOut`. A
  script's marker is not an object here either (`ScriptQueryNatives.CanViewCameraTakesTheThingAndTheDegreesAndIsFalseForWhatIsNotAnObject`).
  The camera debug window shows the answer for player one's creature at 180 degrees.

### Releasing the camera

**Faithful.** `GScript::StartCameraControl` 0x6ECCA0 at 0x6ECD70 (END_CAMERA_CONTROL `GScript::EndCameraControl` 0x6ECEF0, and the task stop in it at 0x6ECF20) removes the dual camera; if
the mode is the script one it deletes it and creates a `CameraModeNew3`, which starts from the current zoomers (no
jump); the field of view **always** returns to 70° in 0.5 s; then the script state (`Help/ScriptControl`).

### Placed paths (CameraModePath)

**Faithful.** A particle with a camera (`Particle3DAnimWithCamera`, the forest miracle's, see
[Particles](../physics/particles.md) and [Forest](../magic/forest.md#forest)) takes the camera with `CameraModePath`
(vtable 0x8C7D10, 0x14 bytes). The particle decides when the mode is made, when it plays and when it lets go; the mode
does the rest.

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
    0x407D60 with the same placing below 0.001 s (0x67ABBE..0x67ACBC, 0x67ACFA..0x67AE04). Then nothing until it
    plays.
- The clip time +0x28 is 0 from the ctor `Particle3DAnim::Particle3DAnim` (0x6C8556) and is written by `Particle3DAnim::DrawAt` 0x67A8FC
  as `GetCycleTimeFromFrame(drawn frame)`, the clip's ms × frame / 1000 in integers: for Forest.cam's 6633 ms, frame
  999 is 6626 ms.
- The sampler fn_0086D4E0 is openblack's `CameraPath::SampleAt`. It then places both points with the object's matrix
  (object +0x14: x row +0x14, y row +0x20, z row +0x2C, translation +0x38), each coordinate ((z · m_z + y · m_y) + x ·
  m_x) + t in the order of the x87 stack (0x86D6B0..0x86D74D), in single precision (the game thread's 24-bit FPU,
  [Floating point](../engine/floating-point.md#owners-the-24-bit-fpu-and-the-double-constants)).
- The stop: the particle's `DrawAt` 0x67AA45..0x67AA62, at a drawn frame ≥ the frames of a cycle (1000) − 1, calls
  `StopCamera` 0x6C86C0, and fn_00441E00 sets +0x10 = 0 on every path mode of the stack with that particle
  (0x441E16..0x441E48).
- The take-back: `ProcessKeyMovement` 0x460FE0 (actions 9 / 10 and 7 / 8, ±400 × the camera's frame time +0x6C,
  `__ftol`) makes a `CameraModeNew3` for a step that is not 0 (0x46108C), and so does the interface state +0x3AC ==
  0x14, the land grip (0x4610C3). Rotating, tilting and zooming are not read.
- `Cleanup` 0x460F60: the near plane from +0xC, [0xC3812C] / [0xC38130] again, `CHand::Show(1)` (0x460FA4) and
  0x6CA5D0 on [0xD4EDB0]. No caller hides the hand for the path.
- None of these draws a random number.

### Camera tracks

RUN_CAMERA_PATH plays a track of `Data\camera.edt`; a script also walks a MobileObject along one with `WALK_PATH`
(177), reads the walk with `GET_WALK_PATH_PERCENTAGE`, and turns a track's points into the world with
`CONVERT_CAMERA_POSITION` / `CONVERT_CAMERA_FOCUS`. The file, the way runner and the walk (the `DataPath` of a
MobileObject, the sharks of Land 1) are in [The file](../formats/land-and-script-files.md#the-file).

### Sources

- bw1-decomp: `src/Black/CameraModeFollow.cpp` (the follow mode, its key movement and validity).
- The xref index of `runblack.exe` (callers, vtables, string and constant reads of every address above).
- The double click's recorded flight (the 24-bit evidence above) and 100 000 random points against the exact model.

## openblack

**Done** for the script mode, the arrival rule, the following (without the computer player and the creature's focus
writes), the dual camera, the shake, the opcodes, the release and the placed paths; the row by row status is in
[the camera paths progress](../project/progress/camera/camera_paths.md). The rest of the camera's openblack side
(files, the frame and the zoomers) is in [The game camera](game-camera.md#openblack); the port decisions (placed camera
paths, the camera editor's file and its tracks, script control of the camera) are in
[Port decisions: hand, camera and interface](../project/port-decisions-hand-camera-interface.md).

- **Dual camera**: START_DUAL_CAMERA and RELEASE_DUAL_CAMERA ✅ (`StartDualCamera`, `ReleaseDualCamera` in
  `src/CHLApi.cpp`), compared with 0x6ED2E0 and 0x6ED410 (2026-10-28): b popped first, then a; either missing gives
  "Thing invalid for dual cam" and nothing, with no mode or citadel check; a dual equal to the current one is not
  added; the release deletes and pops only a current dual, the mode seconds from 0. Every call in the quests (six,
  in SwapCreatures, CreatureBreeder, the creature guide and development sets) is inside `begin cinema`, so on top of
  the script mode that START_CAMERA_CONTROL has just made, the faithful case; only a camera control taken inside the
  citadel, which makes no script mode, would put a dual over the player's mode, the approximate case below.
- **Dual camera's layer**: `script_camera::State::duals`, a layer on top of the script mode (no mode stack:
  ⚠️ approximate); the dual takes the player's zoomers as `BeginFrom` and, if it goes away with no script
  underneath, gives them back. The creature interaction's camera, which cannot be left, covers the script's modes
  while it has the view (`script_camera::Cover` / `Uncover`, `State::covered`; [The hand on a
  creature in openblack](../hand/states.md#the-hand-on-a-creature-in-openblack)): none of them is current meanwhile,
  as the original's `CameraModeCtrInteract` is on top of the stack.
- **Shake**: `src/Camera/CameraShake.{h,cpp}` (`camera_shake::`, with `graphics::lh3d::Random`) and
  `script_camera::ApplyShake`, every frame from `Game.cpp` with the camera being drawn (the script's or the
  player's), as a draw-only offset of `Camera::SetDrawOffset`: the zoomers do not shake. The particle sound rule's
  shake is `camera_shake::StartEffectShake`.
- **Facing an object**: `script_camera::FaceObject`. A creature's facing is `script_camera::CreatureFacing` of its
  body's yaw (the `LH3DCreature` +0x84 above): minus `CreatureLocomotion::heading` once it has moved, else read from
  its body's rotation (`creature_pose::ReadHeading`): the shared `creature_pose::BodyYaw`. Each step is a float
  add, as the original's 24-bit FPU rounds it. Pinned to the oracle's bits by
  `ScriptCameraFollow.ACreatureFacesByItsBodysYawPlusAFullTurnLessTwoAndAHalf`.
- **Placed paths**: the sampler is `CameraPath::SampleAt`. While the creature interaction's camera has the view, no
  placed path has the camera and none is placed (`CameraPathSystemInterface::SetCovered`), as the original's path
  `Cleanup` / `Restart` and its constructor's `CanExit` test. While a path has the camera the player's camera reads
  none of its controls (`camera_path::PlayerCameraReadsControls`), as the path mode on top reads only its take-back;
  test `CameraPathControl.WhileAPathHasTheCameraThePlayersOtherControlsAreNotRead`.
- **SET_AVI_SEQUENCE** (`SetAviSequence`, `src/CHLApi.cpp`): sequence 1 plays `Data/intro.bik` through `video::Get()`
  (`Play`, then `ScheduleIntro`: the pause given back after 58 s and the picture faded out up to 60 s), sequence 2
  starts the falling spell's film (`video::GetFallingSpell().KickOff()`) and off 2 ends it; a start clears the black
  fade at once (`FadeBackToNormal(0)`). The films themselves are on [film playback](../video/playback.md#openblack).
- **Camera tracks**: `src/3D/CameraTrack.{h,cpp}` (the evaluation), the `WalkPath` component and `WalkPathSystem`
  (the walk), and `src/CHLApi.cpp` (the natives), as listed in
  [Land and script files](../formats/land-and-script-files.md#openblack). A walk path's state is a component on its
  entity, `camera.edt` loads through the resource caches, and the track maths are pure functions tested with fakes in
  `test/`. The eye and focus along three of the game's own tracks (13, the glade where the creature is chosen; 29, the
  start of Land 4; 39, the lost brother's) are checked bit for bit against the exe by
  `CameraTrackOriginal.TheGamesTracksGiveTheOriginalsEyeAndFocus` (an integration test, skipped without the game's
  data): 189 of the oracle's samples, from one runner per track sampled every 40 ms to the track's duration, then at
  −5 and 100 000 ms. The oracle ran the way set-up `LH3DWay::AdjustPtr` 0x844570 on each way's bytes as
  `ScriptedCamera::Create` 0x447060 copies them, the runner `LH3DWay::Running::Running` 0x843ED0,
  `Running::GetPosAtTime` 0x844280 with the clamped time as `CameraModeScript::UpdatePath` 0x461AB0 inlines it, and the
  focus way's `LH3DWay::GetPosAtSegment` 0x8439C0 at the runner's segment (+0x0) and parameter (+0x204).
- **GET_CAMERA_POSITION / GET_CAMERA_FOCUS** (`src/CHLApi.cpp`): the camera's drawn origin and focus, or inside the
  citadel (`game_clock::IsInsideCitadel`, the sequence mode 1 that g_game+0x205A28 is)
  `script_camera::k_OutOfWorldPoint`, through the pure `script_camera::ReadCameraPoint` (test
  `ScriptCameraMode.InsideTheCitadelTheCameraIsReadAsAPointOutsideTheWorld`).
- **Tests that pin the original's timing**: `test_script_camera_dual` checks the dual's zoomer time after 0.075 s,
  0.075 / 1.5 × (1 − 2) + 2 = 1.95 s (0x461E09..0x461E26); `test_script_camera_mode` checks the 0.1 s cap on a
  frame's camera seconds (0x441FB0): 5 frames of 1 s are 0.5 s, so a 1 s move has not arrived after 5 frames and
  has after 11.

### Test hooks

The camera's debug variables are listed in [The game camera](game-camera.md#test-hooks).

## Pending

- **GAME_THING_FIELD_OF_VIEW's sphere centre** ([the field-of-view tests](#the-field-of-view-tests)): the original
  takes the mesh's box centre through its 3D object's own matrix (0x868CBC..0x868D04), whose cells the object's class
  writes as it is placed, turned or scaled (`LH3DObject::SetPosition` 0x423140, T·Ry(−a)·S, and the other `Set*`, each
  with its own float roundings, [the matrices](../engine/matrices-and-zoomer.md)). openblack sums in the original's
  order (`affine::BoxCentreThroughObject`) but over `affine::FromModel(affine::Model(Transform))`, the Transform's
  rotation as each archetype or system built it, which openblack keeps for every object in place of the 3D object's
  matrix. The centre is bit for bit the original's only where those cells are; making them so is the shared fix every
  object's matrix needs (drawing, picking and culling read the same Transform), not one of this native's, so it is
  left to that fix. Until then the sphere's centre may differ in the last bits, which changes the answer only for an
  object exactly on the screen's edge.

- Camera tracks: past the table (u < 0 or u > 1) the original reads the object's other fields; what it gets there is
  not read.
- 372 / 373 (following the PC player's hand, `GComputerPlayer::GetHandPos` 0x657FE0).
- The GameAngle of a villager comes from `WallHug::yAngle` rounded to 2048 per revolution.
- The player's follow mode on a creature (the double click, `CameraModeFollow::ProcessKeyMovement` 0x44CFE0,
  `IsStillValid` 0x44B960) is Creature Mode (`CreatureModeSystem`, its own `CreatureCameraModel`; there is no mode
  stack). Its camera mode's seconds (`script_camera::State::modeSeconds`) start again as it locks on, as C lets go,
  and as the keys alone or a fresh grip give the camera back (`script_camera::ModeChanged`); the creature going leaves
  them running. The Game Detail Overlay shows them. openblack's `k_FollowMinPitch` (`Camera/ScriptCamera.h`) is the script mode's own floor,
  not `CameraFollowMinPitch`.
- The player camera: the zones' steps other than the fence and the map's disc, the force field's pulse and the fixed
  rotation ([the player camera's Pending](player-camera.md#pending)); the fence's shake and putting back are ported.
- ⚠️ approximate No mode stack: the dual always goes on top of the script; a dual over the player mode is approximated.
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
  - steps 4 and 6 of `GCamera::Update` (the nudge, 1 m above the ground) on a path's frames: `Camera::UpdateZoomers`
    does steps 3 and 5 ([One frame of GCamera::Update](game-camera.md#one-frame-of-gcameraupdate));
  - the particle copies its atom's matrix into the object every turn (0x6C874A); openblack takes the placement once,
    at `Begin` (🔍 inferred the same for the forest, whose group 2 no rule moves);
  - `CameraModeNew3`'s ctor 0x4572E0 on a take-back, not read.
- Forest: the camera particle's draw runs off screen and at alpha 0, in the original as here (read in
  [The particle meshes and the shield dome](../physics/particle-creators.md#the-particle-meshes-and-the-shield-dome)).
- `CameraModeNew3::Reinitialise` 0x4589B0 is read for the end of a script's camera control (`CameraModel::Restart`,
  [the player camera](player-camera.md#openblack)); a placed path's take-back does not restart the player's camera.

The player camera's pieces that are not ported (the land grip, the double click, the mouse drags, the clear view, the
wheel's gate, the auto pitch) are on [the player camera's Pending](player-camera.md#pending), and the fight watch's on
[the creature's fights](../creature/fights.md#pending).
