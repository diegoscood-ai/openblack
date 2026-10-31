# The player camera

## Summary

The player's camera in the original is `CameraModeNew3`, the mode at the bottom of [the game camera](game-camera.md)'s
stack. It turns, tilts and zooms with the keys, the wheel and the mouse, grips the land and pans it, flies to a place
on a double click (in two legs, over a middle point, with a woosh), tilts itself towards a set pitch, shows the
"tricons" (the small rotate, pitch, zoom and cross icons at the hand), and is held by the zones a script loads (a
fence the camera cannot leave, height limits, exclusion domes). Which controls the player may use is a set of feature
bits the scripts set (`CameraHelp`).

openblack's player camera is `Camera` with `DefaultWorldCameraModel`. The land grip, the pan and its stop short of the
land, the mouse drags, the wheel, both buttons and the middle button, the clear view, the auto pitch, the flights and
their views, the double click on an object and the land under a pixel are ported **faithfully** (some with marked
approximations), and so is what the camera help hears: the player's own turn, tilt, zoom and drags of each frame, with
their input mask, never what a script's camera or a hand demo left. The zone file is read and the camera is kept
inside the fence and the map's disc (across the ground, its height kept, with the origin and focus rounded through the
map's middle each frame as the original); the other
zones' steps are not ported; the fixed rotation is stored and not used. The movement keys go by the zoom's step and
only with the land grab allowed. The tricon icons and their fade are drawn (sized from a 4:3 screen as tall as the
screen, a 🎯 chosen difference), and the land's Tilt, Rotate, Zoom out and Move tooltips are shown.

## The original

### Zones and fixed rotation

- SET_CAMERA_ZONE 142 (0x6ED890): `ResetExclusionFile(1)` 0x455320 (at 0x6ED8E8) and `LoadExclusionFile` 0x455370 of
  `.\Data\Zones\%s` (segment «cameraexc»: flags, two limits of 500, n force-field points, exclusions), force field
  switched on. The nine `.exc` in `Data\Zones` are read correctly.
- `LoadExclusionFile` 0x455370 calls `ResetExclusionFile(id)` (0x45537A), opens segment "cameraexc" (0x9CE6F4,
  `LHFile::OpenSegment` 0x455395) and reads, in order (0x45539A..0x455466): a local int32 header (the version, 1 in all
  nine files), `_UseExclusions` 0x9CE6B0, `CameraModeNew3::DrawForceField` 0xC5E144, `_UseMaxHeight` 0xC5E14C,
  `_UseMaxDistance` 0xC5E148, the floats `_MaxHeight` 0x9CE6AC and `_MaxDistance` 0x9CE6A8,
  `CameraModeNew3::ForceFieldPointCount` 0xC5E130 and that many 12-byte points into `ForceFieldPoints` 0xC5B130
  (0x45541A..0x455441, no bound: the array has room for 1024, 0x3000 / 12), then the record count (its local zeroed
  first) and size (0x455443..0x455466). Records of 0x28 bytes (0x45546F) each become a CameraExclusion of that id
  (`CreateDome` 0x454960 read over, its link and id, +0 and +8, put back; list [0xC5E160]; 0x4554CF..0x455510); any
  other size is read into a scratch buffer and thrown away (0x45547C..0x4554B9). No read is checked
  (`LHFile::GetSegmentData` 0x7BE040 each time, as `GetSegmentData(ptr, size, -1)`). The file is a Lionhead segment file
  ("LiOnHeAd", then segments of a 32-byte name, a u32 size and the data).
- Each force-field point is an inclusion polygon point (x, z) with a height.
- SET_CAMERA_ZONE path ".\Data\Zones\%s" (0xC0C248, 0x6ED8F7), opened with `LHReleasedFile::Open` 0x7BD730.
- After `LoadExclusionFile` (0x6ED92A), SET_CAMERA_ZONE sets [0x9CE6B0] = 1 and DrawForceField = 1 whatever the file
  says (0x6ED936..0x6ED940).
- A zone file that does not open logs "Couldn't load zone file-%s" (0xC0C22C) and the zone stays reset
  (0x6ED956..0x6ED96F).
- `ResetExclusionFile(id)` 0x455320: `RemoveByID` 0x454A40 (deletes the CameraExclusions with +8 == id); [0x9CE6B0] = 1
  (0x45532F); DrawForceField [0xC5E144] = 0 (0x455339); [0xC5E14C] = [0xC5E148] = 0 (0x45533E / 0x455343); [0x9CE6AC] =
  [0x9CE6A8] = 500 (0x43FA0000; 0x455348 / 0x455352); ForceFieldPointCount = 0 (0x45535C).
- A new land clears them: `GGame::LoadMap` 0x552F40 calls `ClearMap` 0x552BB0, which calls `CameraExclusion::RemoveAll`
  0x454A70, `ResetExclusionFile` and `ResetCameraModeNew3` 0x460B20 (RemoveAll and `ResetExclusionFile(0)` again) at
  0x552D59..0x552D67; a script reboot resets them too (`GScript::CleanGameForScriptReboot` 0x6EBCB4).
- Readers of the zone flags: [0x9CE6B0] by `CameraModeNew3::GetAltitude` 0x459C7B; [0xC5E14C], [0x9CE6AC], [0xC5E148],
  [0x9CE6A8] by `CameraModeNew3::Update` 0x45C28C / 0x45C29B / 0x45C2B5 / 0x45C2BC. Where they are off, both limits are
  30000 (0x46EA6000, 0x45C2B4).
- The inclusion distance [0xC5E13C] is FLT_MAX ([0x8C7BB0]) at start (0x4548D0) and in `ResetCameraModeNew3` 0x460B2E.
  `CameraModeNew3::Update` writes 1e10 (0x501502F9, 0x45FCE1) while there is no force field. GET_INCLUSION_DISTANCE
  150 (`GScript::GetCameraInclusionDistance` 0x6ED990, read at 0x6ED991) pushes it.
- **`InsideInclusion` 0x455E20** (`CameraExclusion::InsideInclusion(point, dir, LHPoint* closest, LHPoint* normal)`)
  only tests when DrawForceField is on (0x455E29..0x455E3F; with a hit pointer it is filled first, 0x455E40..0x455E55)
  and the polygon has at least 3 points (0x455E5E):
  - from the last point (0x455E94..0x455E97) round the polygon, each edge's end becoming the next start (0x4560E7);
    the normal ahead starts at (0, 1) ([esp+0x28], 0x455EC0..0x455EC8); the edge e = a − b (0x455EEF..0x455EFF);
  - on a point when the squared distance is below [0x8C7BD8] = 1e-8 (0x455F24..0x455F3D); an edge counts when its
    squared length is above it (0x455F43..0x455F52); −e.x as a double (`fchs`, 0x455F60);
  - the ray against the edge when |denominator| > [0x8C79D8] = 1e-4 (9.9999997e-05, a double; 0x455F74..0x455F83):
    numerator ((b.x·e.z + b.z·(−e.x)) − p.x·e.z) − p.z·(−e.x) (0x455F89..0x455FA4), the crossing q = p + t·dir
    (0x455FAC..0x455FDE), s = ((q.x − b.x)·e.x + (q.z − b.z)·e.z) / |e|² (0x455FE2..0x455FFD), counted when 0 ≤ s < 1
    (0x456001..0x45601D); |t| below 1e-4 is on the edge (0x456023..0x456032); t > 0 counts a crossing
    (0x456038..0x456043, 0x45604F), t < 0 keeps the nearest one behind (0x45608E..0x4560AA). The best ray parameters
    start at 1e20 (0x60AD78EC; 0x455EA2 / 0x455EAF);
  - the hit ahead (0x456102..0x456164) or else behind (0x456168..0x4561C6), and the nearest polygon point in x/z when
    nearer is the output (0x4561CB..0x45624A);
  - odd crossings = inside (0x45624C..0x456252).
- **SET_FIXED_CAM_ROTATION** 209 (0x6EE1A0): only with the player mode; `ForceRotateAboutPoint` 0x457330: +0x88 = 1
  and +0x7C = the point (0x457338..0x45734F), or +0x88 = 0 for null (0x457355). `Update` copies +0x7C over +0x12C
  (0x45AB00, 0x460135), so the player's mode turns about that point. `Reinitialise` 0x458B22 (and 0x458D23, 0x45E8D6)
  clears it. No map uses it. Without CameraModeNew3 it logs "Wrong camera mode" (0xC0C29C).

#### What the player camera does with the zones

The steps of `CameraModeNew3::Update` 0x45A960 that read the zones, in order. All products and sums are rounded to
float (the 24-bit FPU); `fsin` / `fcos` are not affected by the precision control. The locals: p the camera's
destination origin (GCamera +0x11C/+0x14C/+0x17C, [esp+0x14..0x1c], set at 0x45AA45), f the destination focus
(GCamera +0x8C/+0xBC/+0xEC, [esp+0x3c..0x44], 0x45AA5D), s a copy of p at the start (`[esp+0xf8..0x100]`, 0x45AABB),
+0x12C the mode's own focus point (the heading and pitch are taken from p to it, `GetHeadingAndPitchFromPoints` at
0x45C494), +0x1AC the drag state (0 none, 4 the turning drag, 5/6 the pitch drag, 7..10 the edge scroll; 0x45CC54..).

The ground under a point is `LH3DIsland::GetAltitude` 0x803090 at mc.x = ftol(float(float(x·65536)·0.1)), the same for
z, mc.y = 0 (0.1f = 0x3DCCCCCD, 65536 = 0x47800000; ftol truncates towards 0). `CameraModeNew3::GetAltitude` 0x459C30
is the same plus, with `_UseExclusions`, the dome exclusions (type 0): inside a dome's radius (NaN counts as inside),
h = sqrt(1 − |p − e|²xz / r²) × the dome's +0x1C + e.y, and the ground is raised to h when not above it. The zone
steps use the plain island altitude, except the land probe.

1. **The height limit** (0x45C242..0x45C387, before any of the frame's movement), from the ground at p as it is then:
   `[esp+0xf4]` = ground(p) + 3.0 (0x40400000, 0x45C291), and the limit `[esp+0xec]` (0x45C334 / 0x45C387):
   ```
   highest   = _UseMaxHeight   ? _MaxHeight   : 30000
   aboveLand = _UseMaxDistance ? _MaxDistance : 30000
   limit = (ground(p) + aboveLand <= highest or unordered) ? ground(p) + aboveLand : highest
   ```
   (the ground read by three `GetAltitude` calls of the same p). Also +0x1C4 = min(|p.y − (g + 3)|, |p.y − limit|) (a
   double compare), 0 if p.y < [esp+0x40]. `UpdateClickParams` (0x45BB5F / 0x45C009) and
   `CalcPerpDistance` (0x45BDAD) take p by reference but do not write it [V]: 0x459610 stores only into the mode
   (+0xCC, +0xD8, +0x12C, +0x138, +0x14C, +0x160, +0x16C, +0xFC, +0x158) and its stack, and 0x458DB0 only into its
   stack (0x458DBF..0x458DFB).
2. All the movement (strafe, drags, zoom, edge scroll).
3. The exclusion push (domes and cylinders, with `_UseExclusions`, 0x45F671).
4. The land probe: 3 rings × 8 directions, `GetAltitude` with the domes (0x45F79B, 0x45F856).
5. **The fence** (0x45F982..0x45FBEC, only with DrawForceField; without it the inclusion distance is 1e10 and the step
   is skipped):
   ```
   towards = (|s − p|²xz < 1e-4 (0x38D1B717) or NaN) ? f : s     // GetDistance2DSq 0x4606F0
   dir = towards − p                                              // not normalised
   inside: inclusion distance = |closest − p|xz; done
   way = closest − p, y = 0, normalised (0x460710: left alone when x, y and z are 0, NaN counting as 0;
         else c × 1 / sqrt((z·z + y·y) + x·x)); the normal normalised too, and unused
   r = (+0x1AC == 4) ? 50 : 20;  target = closest
   for k = 0..31, for i in (−k, +k):                              // k = 0 tested twice
       a = float(i · 0.0981747732f)                               // fild, fmul 0x3DC90FDB, rounded to float
       s_ = float(fsin(a)·r), c_ = float(fcos(a)·r)               // x87 extended, one rounding
       cand = closest + (s_·(−way.z) + c_·way.x, 0, s_·way.x + c_·way.z)
       InsideInclusion(cand, dir) → target = cand; stop
   f −= p; +0x12C −= p (0x460620); p.x = target.x; p.z = target.z (p.y kept); +0x12C += p; f += p
   ```
   then **the fence hit** (0x45FBEC..0x45FCC9), in order: [esp+0x4B] = 1; the inclusion distance = 0; CameraHelp
   0x202 if the help is on; `GInterface::StartImmersion(0x2A, 0x80000000)` (force feedback) if there is an interface;
   `GAudio::PlaySoundEffect(0, LH3DTech::GetCameraPosition(), 0x2B, 2, 0, 0, 1, 1)` (sound 43 of InGame.sad,
   G_OutOfBounds, at the camera's current position); `LH3DCameraChecker::Create(100, p, 1.0, 400 ms, false)` (a shake,
   0x45FC84); `+0x2F0 = 1` (the inputs held back) unless +0x1AC == 4; with `ForceField` 0xC5E150,
   `LightSheet::PulseForceField(p, 200)` 0x83E610: nothing unless r > 0, the sheet's +0x5C and points; for each point
   with d = sqrt(dz² + dx²) < r (or NaN) its strength += 1 − d / r. The force field's points are set by
   `SetForceFieldPoint(Count)` 0x454B40 / 0x454B50, its texture `data\textures\forcefield.raw`.
6. The mode-3 focus fix (`[esp+0xb4]` == 3, 0x45FCEB).
7. **The lowest height** (0x45FD5B): unless GCamera +0x78, p.y < `[esp+0xf4]` (NaN clamps too) → p.y = `[esp+0xf4]`,
   from the ground of step 1.
8. **The slide under the limit** (0x45FD8F..0x45FE2B), with `_UseExclusions`, when p.y > limit (≤, == and NaN skip):
   d = float(p.y − limit) ([esp+0x88]); line = f − p (`LHPoint::operator-` 0x44CF90); when |line.y| > 0.01 (the double
   0x8C7A10, 0x3F847AE140000000): k = −(d / line.y), line ×= k (`LHPoint::Mul`), and p, f and +0x12C each get + line
   (`LHPoint::Add`); help 0x203.
9. The focus set again, f = Normalise(f − p) × +0x2E4 + p, unless bl (0x45FE57).
10. The map disc (MapSize 5120 about MapCentre, 0x45FEAE..0x45FF61), skipped in the editor ([0xD00DCC]): p and f
    less MapCentre (2560, 0, 2560); d² = z² + x² of p, across the ground; only when d² > 5120² (≤, == and NaN skip):
    s = 5120 / √d², p.x × s and p.z × s (p.y kept), bl = 1 and, with [0xC5E168], help 0x202; then p and f plus
    MapCentre again. Then the largest move (MaxMoveSpeed 1.6, at least 300, 0x45FF67). openblack:
    `camera_zones::KeepInsideMapDisc` in `DefaultWorldCameraModel::ConstrainDisc`, across the ground with the height
    kept, and help 0x202 with `_dragCountsForHelp`
    ([0xC5E168]) as the fence's; tests `MapDisc.*` (`test_camera_zones`; the report, like the fence's, has no test:
    the move accumulators have no reader outside the help profile);
    debug: the camera window's "Camera across the ground from the map's middle" line.
    <a id="player-camera-map-middle-rounding"></a>**The round trip through the middle** (✅, #207):
    the steps, every frame `CameraModeNew3::Update` runs (it has one exit, 0x4605CF, and no jump passes over the
    block) unless [0xD00DCC]: `LHPoint::operator-=` 0x460620 on p, then on f (each component `fld`, `fsub`, `fstp
    dword`: x, y, z); the disc test on p (`fld z, fmul z, fld x, fmul x, faddp`, against `fld MapSize, fmul MapSize`;
    `fsqrt`, `fdivp` MapSize / √d², then that × x and × z, each `fstp dword`); then `LHPoint::Add` 0x44EA40 on p, then
    on f (MapCentre + p, each component `fstp dword`). The game runs the x87 at 24-bit precision (control word 0x007F),
    so every step is a single-precision rounding, as SSE float arithmetic. MapCentre is a function static set on the
    first call from HalfMapSize [0x9CE634] = 2560 (x, z) and 0 (y) (0x45A9CD..0x45A9FE); MapSize [0x9CE630] = 5120.
    Hence p and f are rounded in every frame, inside the disc too: a coordinate below 1280 (negative ones included)
    loses its low bits (0.1 → 0.10009765625, 100.3 → 100.300048828125, −37.81 → −37.81005859375, 1e-5 → 0), from 1280
    up it is exact (from 1280 to 5120 by Sterbenz; above, x − 2560 keeps x's grid), and a height of −0 becomes +0.
    Pinned with the oracle (`LHPoint::operator-=` then `Add` on nine points; their bits in the test
    `MapDisc.RoundsTheOriginAndTheFocusThroughTheMiddleAsTheOriginal`). openblack does the same in `KeepInsideMapDisc`
    to `_targetOrigin` and `_targetFocus` at the same step of its frame (after the lowest height and the focus re-set,
    before the largest move); debug: the camera window's "Last rounding through the map's middle" line, the last
    change it made to the origin and the focus.

The other callers of `InsideInclusion`: 0x45DF19 in `Update` (the mouse ray's hit against the fence with dir (1, 0, 0):
outside, CameraHelp 0x101 and the hit becomes the closest point; then `InsideExclusion`, help 0x100); 0x44C3AD and
0x44CB16 in `CameraModeFollow::Update`; `CameraModeNew3::FlyToPosFoc` ([Flights](#player-camera-flights-to-a-place));
and `Influence::CalculatePlayerRawInfluence` (0x5CD2D9..0x5CD339, at 0x5CD32F): a player's contribution counts only
when the point (x, z) = float(double(mc) × 0.000152587890625) (`fild`, `fmul` 0x39200000 = 10/65536, one rounding:
map coords pass 2^24 beyond x = 2560) is inside, with dir (1, 0, 0); else it skips (`je` 0x5CD264).

#### Player camera features (CameraHelp)

- `CameraHelp`'s EnabledFeatures [0x9CDD6C]: initial value 0x1BF, also set by ClearMap and
  SET_INTERFACE_INTERACTION(NORMAL). Auto-pitch parameters [0x9CDD64] = π/6 (the float 0x3F060A92, 0.52359879, in
  the data and in `ClearMap`'s write at 0x552E88) and [0x9CDD68] = 75.
- `CameraHelp::EnableCameraFeatures` 0x447430: features = (features & ~mask) | new (`not` mask; `and` old; `or`
  features). Every SET_INTERFACE_INTERACTION level passes mask −1, so the whole set is replaced (auto-pitch included).
- Auto-pitch fn_004473F0(p1, p2, on): EnableCameraFeatures(on ? 0x40 : 0, on ? 0 : 0x40) (`neg; sbb; and 0x40` and
  `neg; sbb; and -0x40; add 0x40`), then stores p1 and p2.
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
  - 0x80 adds tricon display bit 8, the cross, when GInterface +0x48 is 0 (`UpdateTricons` 0x459557), and while the
    inputs are dropped after a given-up grip (0x4595F3..0x4595FE).
- The bit names (Zoom, Rotate, Pitch...) are 🔍 inferred from the SCRIPT_INTERFACE_LEVEL names that set them
  (consistent over the 16 levels at 0x70B7A8).
- Player zoom scale (0x45C104..0x45C1B5): 3 × the height difference between the camera and focus destinations (GCamera
  +0x118 / +0x88 y), at least 60 ([0x9CE65C]), at most 240 ([0x8AB418] × 60) when the camera is below the focus
  (0x45C108..0x45C111) and 2000 ([0x9CE660]) when above.
- Zoom delta = input × 0.0015 ([0x8C7CF8], 0x45C0D5) × scale.
- **The movement keys (0x45C93E..0x45CAD0)**, inside the mouse modes 2 and 3 block (0x45C690..0x45C6A2): only with
  feature 0x08 (`GetCameraFeatures` & 8, 0x45C943..0x45C948), and moved only in mode 2 (+0x8C, 0x45C98E..0x45C99B). The
  step is the zoom's scale above ([esp+0x5C], written only at 0x45C15C..0x45C1AB; `EdgeScrollMinSpeed` and
  `EdgeScrollMaxSpeed` have no writers): scale = step × 0.001 ([0x8AA3B0]); slope = `fptan`(pitch), ≤ 0.2 (the float
  0.2 as a double, [0x8C6B38]) or unordered → 0.2, else ≥ 2 ([0x8AC410]) → 2, kept unrounded; across = −(scale ×
  [0xC5B0D4]) stored as a float; ahead = (scale × [0xC5B0D0]) / slope; c = `fcos`(−yaw) stored as a float, s =
  `fsin`(−yaw) unrounded; x = across × c + ahead × (−s), z = across × s + ahead × c (0x45C9A1..0x45CA4C), added to the
  origin, the focus and the click focus. The inputs are 400 × the frame's seconds a key (`ProcessKeyMovement`).
- ✅ **The turn and tilt keys add up on their own** (`ProcessKeyMovement` 0x458220, 0x4582B4..0x45836D): four separate
  tests, ROTATE_RIGHT adding 400 × t to the turn input [0xC5B0C8], ROTATE_LEFT taking it away, TILT_DOWN taking
  400 × t from the tilt input [0xC5B0C4] and TILT_UP adding it, so both keys of a pair held cancel out and the camera
  stays still. All four are skipped while `GGlobal::Global.EditorMode` (+0x2D2AC, [0xD00DCC]) is set; its only two
  writers clear it (ProcessNetworkPackets 0x54CCEB, EndTurn 0x54E9E7) and it is in .bss, so it is 0 throughout the
  game and openblack reads the keys always. openblack: `KeyPairInput(less, more)` in
  `Camera/KeyboardMoveSpeed.h`, -1 + 1 for both held, in `DefaultWorldCameraModel::HandleActions`. Test `KeyboardMoveSpeed.BothKeysOfATurnOrTiltPairHeldCancelOut`. The movement keys
  (forwards and backwards, left and right) are also four separate tests in the original (`ProcessKeyMovement`, decomp
  CameraModeNew3.cpp:1389..1446), so both keys of a pair held cancel out whatever the modifier; openblack adds them
  the same way through `KeyPairInput`. Their modifier is
  `IsRotateKeyModifierActive` 0x457F30: off with the zoom key held, else on while a fight is watched in its arena
  (`HasFight` and `arena`), else the turn key's: so with the zoom key the keys zoom (forwards and backwards) or turn
  (left and right), with the modifier on they tilt or turn, otherwise they move. openblack: `TurnModifierOn` in
  `Camera/KeyboardMoveSpeed.h`, with `DefaultWorldCameraModel`'s watched fight.
- The player's zoom, rotate and pitch of the frame go to `CameraHelp::CameraHelpCallback` 0x449140
  (0x45C38E..0x45C3AD / 0x45C6A8..0x45C838): help events 25..29 of GET_TOTAL_EVENTS, counted once a turn
  ([Timers, help events and field of view](../interface/advisors-and-help.md#timers-help-events-and-field-of-view)).
  - The zoom (0x304) is reported in every update whose zoom step is not 0 (0x45C38E..0x45C3AD), whatever the mouse
    mode; the turn (0x300..0x302) and the tilt (0x303) inside the mouse modes 2 and 3 block (0x45C690..0x45C6A2),
    which any turn, tilt, zoom or movement-key input puts the camera in ([0xC5B0E8], set at 0x45B8BD..0x45B8C5, read
    at 0x45BE04). So a zoom or a key's turn while the land is gripped (mode 3) counts too.
  - ✅ **Only the player's own input.** The camera help's 29 callers are all `CameraModeNew3`'s (`ZoomToCitadel`
    0x457F01, `UpdateStrafe` ×4, `Update` ×24; xref), and only the camera's current mode is updated (`GCamera::Update`
    0x441FD9) and given the keys (`GGame::ProcessMapKeys` 0x63F7C2, vt +0x28): a script's camera, a pull-back, a
    flight or a hand demo's camera never reaches it. The turn, tilt and zoom inputs [0xC5B0C8] / [0xC5B0C4] /
    [0xC5B0CC] are written only by `ProcessKeyMovement` 0x458220 (the keys, cleared first), by `Update`'s own reads of
    the middle button, the wheel and both buttons (0x45AF6D, 0x45AF85, 0x45B7A1, 0x45B7C7, 0x45B847) and by the auto
    pitch (0x45BF3E..0x45C021), and cleared at the end of the mode 2/3 block (0x45CC08..0x45CC30) and by a new player
    camera mode (`Reinitialise` 0x4589B0, 0x458BB9..0x458BC5). The end of a script's camera control makes one
    (`EndCameraControl` 0x6ECEF0 → 0x6ECD70: the script mode removed, `new CameraModeNew3` at 0x6ECE1D), as do the
    other ways back to the player's camera (`PopViewMode` 0x441CB4, `LeaveInsideCitadel` 0x553DA9, the follow and path
    modes' keys...): an input read before a script took the camera is never heard when it comes back.
  - The keys also need the interface active (`ProcessMapKeys` 0x63F730..0x63F74E, `GInterface::IsActive` 0x5CE2E0),
    which a script's cinema bars make inactive. In the game the bars come only with a script's camera: every bars-on
    call of challenge.chl follows its START_CAMERA_CONTROL (316 of 316, `begin cinema`), so the keys stop with the
    script's camera in every case the scripts make, as openblack's do.
  - During a hand demo the keys still go to the camera's current mode (`ProcessMapKeys` runs during the playback,
    0x54C359; only the mouse buttons wait, 0x54C389; `StartPlayBack` makes the interface active,
    `GInterface::SetActive(1)` at 0x5DAE3D, which `IsActive` 0x5CE2E0 and `ProcessMapKeys` 0x63F730 test), and that
    mode is a script's in every demo the game plays: each `PLAY_HAND_DEMO` of challenge.chl is inside a cinema
    (Rotating, Pitching, Zoom, ZoomReminder, GiveFood, Drag, Throwing, PShield, Fireball, Worship, ImpressFood,
    MiracleHandDemo, LearnGestures, LearnInfluence). So no demo the game plays moves the player's camera, as in
    openblack, which reads no camera controls while a demo plays. Only openblack's test hook
    (`OPENBLACK_TEST_HAND_DEMO`) plays one without a script's camera: not the game's behaviour.
- The double-click flight reports at 0x45DE85, or 0x306 DoubleClickObject at 0x45DE1E when it was on an object.
- The input mask the camera help calls get (0x45C06A..0x45C0C5, names at 0x9CDDF0..0x9CDE00), in the order it is
  built: 0x08 "wheel spin" (the wheel, or a zoom key's notch, read this frame), 0x10 "wheel down" (the middle button
  held), 0x01 "keyboard" ([0xC5B0E8], any camera input but the middle button: the keys, the wheel's zoom, both
  buttons' zoom and turn; a movement key alone only without the land gripped, which clears it, 0x45BE1B..0x45BE31),
  0x02 "mouse button" (+0x8C & 1, the land gripped, which the middle button and both buttons let go of) and 0x04 "both
  mouse buttons". The zoom, turn, tilt and drag reports get it; the collisions, the double clicks and the temple's zoom
  get 0. Only the camera help's own input tables take it (0xC5A320, drawn by its debug page fn_004492F0): the scripts'
  counts do not.
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
  - In an auto-tilt frame `UpdateTricons` runs with +0x8C = 0 (0x45C039), so the idle tricons follow the cursor
    (0x45C02D).
  - It is applied without the Pitch feature, and never sends Pitch 0x303 (0x45C80C..0x45C81C, `test bl,bl; jne`). The
    confirmation feed `SmoothStrafeSpeedZ` 0x454930 is still called. Rotate needs the player's turn input and Zoom the zoom input, so auto
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
- GConfirmation feeds: `SmoothStrafeSpeedX` (0x45C7D6) and `SmoothStrafeSpeedZ` (0x45C8B1) get the frame's turn and tilt over the frame's
  seconds, 0 on frames without them (0x46053F / 0x46055C).
- `CameraModeNew3::FindBestAngle` 0x459144 uses `LH3DIsland::GetNormal` 0x803630 and the point + normal (0x459149).
- **The double click's flight, its view (0x45E08B..0x45E305; 0x45E0C3..0x45E305):** the point is the double click's
  ([esp+0x24], after the inclusion test). d1 = |point − the screen-centre hit +0xF0| (the focus [esp+0x3C] instead when
  ebx is 0, which no path reaching here leaves, 🔍 inferred `StartFight` keeping ebx as callees do) and d2 = |point − the
  frame's origin [esp+0x14]|, both in 3D.
  - The distance: d2 > 100 [0x9CE640] × 1.5 [0x8AB24C] and d1 > 10 [0x8AB414], both strict (0x45E119..0x45E151), give
    the 100 flight, +0x78 = 0 and the woosh flag [esp+0x23] = 1. Otherwise 60 [0x9CE644] when 60 × 1.5 < d2, else 30
    [0x9CE648] when 30 × 1.5 < d2, else 15 [0x9CE64C], with +0x78 = 1.5 × 0.5 and no woosh. After a given-up grip
    ([esp+0x4B]) the distance is 1000, +0x78 = 0.01, the woosh flag 1, and bl = 0 (0x45E1D5..0x45E1F1). The distance
    is used as it is: no shaping. The double click's flight whooshes without `FlyToPosFoc`'s own distance test.
  - The heading and pitch: `GetHeadingAndPitchFromPoints(the frame's origin [esp+0x14], its focus [esp+0x3C])`
    (0x45E20A). With bl = 1, which every flight but the given-up one has (0x45E0C5), heading = `FindBestAngle(heading,
    the distance, point, &pitch, null)` (0x45E22F): the same call for the 100, 60, 30 and 15 flights, with no score
    pointer. Then the pitch: ≤ π/8 [0x8C6CA0] (or unordered) → π/8; else not below 1.496 [0x8C7C9C] → 1.496
    (0x3FBF7CD1, 0x45E23B..0x45E27F). After `FindBestAngle`, which keeps it within π/8 .. π/3, this changes nothing.
  - The focus becomes the point and the origin `SetPointFromPointDistanceHeadingAndPitch(point, distance, heading,
    pitch)` (0x45E29C). With +0x78 == 0 (the 100 flight only) `SetupVia(origin, focus, [esp+0x110], 0.1)` follows
    (0x45E2DB), then the woosh when its flag is set (0x45E2F0..0x45E305).

#### Player camera: the land grip

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
    frame with none clears it (0x45ACEE..0x45ADB9). The wheel and the zoom in and out actions are read after it
    (0x45B708..0x45B7A1, into [0xC5B0CC]), so in a held frame they still zoom, set the input flag [0xC5B0E8]
    (0x45B85C..0x45B8C5, after them) and count, and they neither are dropped nor keep the hold. The same hold follows
    the fence (below).
  - +0x90 = 0x10 makes `UpdateTricons` blink display bit 8, the red cross (not the zoom icon, as the old notes said:
    bit 4 is the magnifier), every 200 ms ((tick / 200) & 1, 0x45959C). **(inferred, from the order of the calls)** It
    is not seen: from the next frame the grip is dropped, so with +0x8C = 0 `UpdateTricons` rewrites +0x90 from the
    cursor (0x459276), and while +0x2F0 is set the display is 0, or bit 8 alone with feature 0x80 (0x4595CB..0x4595FE).
  - [esp+0x4B] runs the double-click flight's body the same frame (0x45DD92), with feature 0x10 (0x45DDA3). The point
    is the one under the hand: the object at GInterface +0x3C8, reported with CameraHelpCallback(0x306, inputs 0)
    (0x45DE1E), or else `LH3DIsland::RayCastFrom2DPoint` 0x800C30's point, reported with CameraHelpCallback(0x305, inputs 0) (0x45DE85); no point
    ends the body with no report (0x45DE78). These are the double click's own reports, once in the frame the body runs.
    It passes the exclusion and 5120 disc tests, and no fight is looked for (0x45DFC5). At 0x45E1D5 it takes the
    distance 1000, +0x78 = 0.01 and the woosh flag [esp+0x23] = 1, and skips `FindBestAngle` (bl = 0). The heading and
    pitch are the camera's (`GetHeadingAndPitchFromPoints` 0x45E20A), the pitch kept within π/8 .. 1.496
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
  - `LH3DIsland::RayCast` 0x802550 from the grip camera through the new origin. It casts a ray to the map's edge,
    not the segment ([spells](../magic/spell-core.md)); without land, the y = 0 crossing of a line going down counts
    within 7500 across from g_camera, the drawn camera, already restored. It gives only x and z.
  - With a hit and the move's x–z length above 3: stop = 3 / |m.xz|; share = toLand.x / m.x if |m.x| > |m.z|, else
    toLand.z / m.z, bailing out when that component is ≤ 1e-4; share −= stop.
  - Only 0 < share < 1 counts: origin and focus come back by m·(1 − share), and 0x200 is sent with [0xC5E168].
- **The cursor:** the pan never warps it.

#### Player camera: the land under a pixel

- **`LH3DIsland::RayCastFrom2DPoint` 0x800C30 (size 0x155 in bw1-decomp's symbols), (pixel, &x, &z, shift,
  depth):** the integer pixel's `Get3DPointFromScreen` at depth 0 (the near point), and with the shift both the camera
  and that point moved forward along the line by the depth (the near clip when the depth is not above 0); then
  `LH3DIsland::RayCast` 0x802550 along the line: the land, or the y = 0 crossing within 7500 across from `g_camera`.
  It gives only x and z. The details of the shift are in [the mouse ray](../hand/animation-and-placement.md#the-mouse-ray)
  (the hand's land, the same function at depth near + 1).
- `CameraModeNew3::Update` 0x45A960 calls it with the shift on and depth 0 for the mouse (0x45BA22, HandHit → +0x144)
  and for the screen's middle pixel (w / 2, h / 2) (0x45BA6E, +0xC8). Their heights are
  `GetAltitude(LH3DMapCoords(x, z))` (0x45BA79..0x45BAF7): the altitude at ftol(x · 65536 · 0.1), the hand's quantised
  point.
- `UpdateClickParams` 0x459610: the average distance +0x1C0 from the 16 pixels (w / 2, h · i / 16), i = 0 .. 15, in
  integers, each with the shift on at depth 0 and its height at the land's altitude; the sum starts at 1 / 50 and the
  count at 1, +0x1C0 = 1 / (sum / count).
- The double click on the land (no object at GInterface +0x3C8) takes the same call at the mouse (shift on, depth 0),
  then y = `GetAltitude` of the point; so does the flight after a given-up grip (see the land grip above).
- The pan's stop is `RayCast` 0x802550 itself, along the line from the grip camera (0x45DA4A).
- The camera is `g_camera` and its axes, the drawn camera of the frame before **(inferred, from the order of the calls:
  the camera update reads the land before it moves the camera)**.
- **The double click on an object** (0x45DDBE..0x45DE92): the object is GInterface +0x3C8 (0x45DDC9), the render
  collide's object, which `UpdateHandRenderCollide` 0x5D0610 copies from the action collide's +0x400 in
  `PreDrawProcess` (0x5D0645..0x5D064B); the land grip keeps it, since the action collide is neither zeroed (0x5CEA57)
  nor picked again (0x5D5A9C) while +0x38 is set. Its other writers in the interface's code (a scan of
  0x5CE000..0x5DD000 for stores to +0x3C8) are the constructor, `SetToZero`, `ResolveLoad` 0x5D052F and fn_005D1260
  0x5D1296 (with +0x400). With an object the point is its `Pos`: x and z are the fixed point × 0.000152587890625
  [0x8AA3A4] (0x45DE1C..0x45DE36, the inlined `ConvertMapCoordToLandscapePoint` 0x613750, whose `GetAltitude(Pos) +
  Pos.altitude` at 0x45DE05 is overwritten), no island ray is cast, and the report is `CameraHelpCallback(0x306)` with
  inputs 0 (0x45DE3A); without one, `LH3DIsland::RayCastFrom2DPoint` 0x800C30 and 0x305 (0x45DE6F / 0x45DE8A). Both then take y = `GetAltitude` at
  ftol(x × 65536 [0x8AC408] × 0.1 [0x8AC404]) (0x45DE92..0x45DED5) and run the same flight. The same branch serves the
  flight after a given-up grip ([esp+0x4B], 0x45DD92). `IsCreature` (vt +0x34, 0x45DE4B) sets the follow flag
  [esp+0xC3], which builds `CameraModeFollow` at 0x45E34B once the flight is set up.
- **When +0x3C8 is written and read** (frame order in [the original loop](../engine/loop.md#the-original-loop), step 4):
  `ProcessGraphicsEngine` 0x54D850 calls `GCamera::Update` at 0x54D879, in which the player mode's double click reads
  +0x3C8 (0x45DDC9); then `PreDrawProcess` 0x5CE9E0, whose `UpdateHandRenderCollide` (called at 0x5CE9E5) copies
  +0x400 into +0x3C8 (0x5D0645..0x5D064B) and, with no action in progress, zeroes +0x400 (0x5CEA57); then the draw
  (`Process3dEngine` 0x54DA80), whose `SendObjectDrawCollision` 0x5D56C0 fills +0x400; `PostDrawProcess` 0x5CEAB0
  (`UpdateInterfaceCollide` at 0x5CEABA); and the frame's inputs (`ProcessFrameInputs` 0x54C340). So the camera of
  frame N reads the copy made in frame N − 1, of the object the draw of frame N − 2 picked. Each pump of the
  interface's messages (fn_005D9A20, at 0x5D9B1C through fn_005D1120) checks +0x400
  (`InterfaceActionValidateObjectsForStates` 0x5D1010): not an `Object` (0x5D1089), not `IsInteractable` (vt 0x190,
  0x5D1099) or +0x24 & 4 (0x5D10A3) ends the action (0x5D10B8), which clears +0x400 and +0x3C8. fn_005D1120 calls it
  first (0x5D1123), fn_005D9A20 calls fn_005D1120 unless a hand demo's playback took the pump (0x5D9B12..0x5D9B1C),
  and 0x5D1010 reads +0x400 with no test of +0x38 (0x5D1070): it runs with or without an action. With none, +0x400
  holds only what `SendObjectDrawCollision` accepted, `IsInteractable` (0x5D56DB) and without +0x24 & 4 (0x5D56E9), so
  the check can only fail a kept object or one a press put there.
- **The object in the hand** never reaches +0x400 or +0x3C8 through its draw [V]. `SendObjectDrawCollision` tests
  only +0x38 (0x5D56CD), `IsInteractable` (0x5D56DB) and +0x24 & 4 (0x5D56E9), and `UpdateInterfaceCollide` 0x5D5A70
  has no test of the held object; but `Object::DrawInHand` 0x5190E0 calls `DrawOutOfMap` (vt 0x614, 0x5192FD) with
  false (`push 0` at 0x5192F9), and every `DrawOutOfMap` then hands `Game3DObject::AddForDrawing` (0x51C860 /
  0x51C8A1) a null object (Object 0x51C820 at 0x51C85E and 0x51C89F, Pot 0x51BC0F, Tree 0x74B382, OneOffSpellSeed
  0x518E07; SpellSeed 0x5190C9 and `Scaffold::DrawInHand` 0x51862D always null; the Creed draws only its glow).
  `AddForDrawing` 0x63B5D0 sends the collision (0x63B66E) only with an object (`test ebp, ebp` 0x63B637), so
  g_b_last_on_screen [0xEA1AF0], g_last_selected_box [0xEA1AD0] and fn_005FAFC0's bit (0x5FB014) never matter for
  it. The hand's clip test (CHand +0x4904, 0x5B60B1) is a second guard.

#### Player camera: the mouse

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
  - In a fight (+0x44) Pitch is cleared (0x459398). In mode 1 only 0x80 is cleared (and 0x40 set with Rotate); in
    modes 2 and 3 +0x90 is kept.
- **The drag classifier (0x45CC3A..0x45CEDC),** while +0x8C == 1, tricons & 3, HandStatus == 0 and |moved|² ≥ 0.02²:
  - elapsed = GetTickCount − +0x128; towards = −press, across = (press.y, −press.x), each made unit when longer than 0.1.
  - Rotate and Pitch both: with features 1 and 2, |mx| > |my| → 4 (edge rotate), else pitch; otherwise feature 2 → 4,
    else pitch. Rotate only: with features 2 and 8, 4 when |along| < |across|, along < 0 or elapsed > 300 [0x9CE674],
    else 3 (pan); otherwise feature 2 ? 4 : 3. Pitch only: pitch. State 4 is set at 0x45CD9B / 0x45CDDC.
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
  - The yaw +0x50 and the working heading get + angle; +0x90 = 1 (0x45D2B8); the confirmation feed `SmoothStrafeSpeedX`(angle,
    dt); then the events (above).
- **Pitch drag, states 5 and 6 (0x45DC4D..0x45DD6E, the same code), only with feature 1:**
  - 0x303 is sent with [0xC5E168].
  - step = dy / h (the whole height [0xE8505A]) × 2.33333 [0x9CE62C] × the horizontal field of view [0xEA1DD0];
    pitch −= step, +0x54 += step; the feed `SmoothStrafeSpeedZ`(step); +0x90 = 2 (0x45DD6E). The pitch is clamped later to
    [−π/6, 7π/16].
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

##### The tricons: what is shown

`UpdateTricons` keeps two words on the camera and writes a third, global one (**verified** in the disassembly):

| Word | Meaning | Written by |
|---|---|---|
| camera +0x90 | the **zones** the cursor is in: 1 rotate, 2 pitch, 4 top, 0x10 given up, 0x40 turning, 0x80 idle. Bit 8 is tested but never set: the writers are 0x459276..0x459345, 0x45D2B8 (= 1), 0x45D380 / 0x45D7F2 / 0x45E90E (= 0x10), 0x45D3AE (= 0), 0x45DD6E (= 2), `Reinitialise` 0x458BE9 (= 0) | `UpdateTricons`, `Update` |
| camera +0x94 | the **active** controls: 2 the tilt input [0xC5B0C4] ≠ 0 or HandStatus 5/6 (0x45934B..0x45936E), 1 the turn input [0xC5B0C8] ≠ 0 or HandStatus 4 (0x459375..0x459391); and the keys' modifiers, 9 or 3 (see below). Cleared at the start of the key movement (0x458266) and with the wheel gate +0x2EC (0x45AE37) | `ProcessKeyMovement`, `UpdateTricons` |
| [0xC5B0F4] | the **display** word, the icons to draw: 1 Pitch, 2 Rotate, 4 Zoom, 8 the red cross; plus 0x100 << i = "only a hint" (idle) | `UpdateTricons`, the hand demo playback |
| [0xC5B0AC] | the rotate icon's angle, the octant arc tangent of (+0x98 x, +0x9C y) − π/2 ([0x8C78D8]) when +0x90 & 0x40, 0 with the middle button (0x45941E..0x459437, 0x459574..0x459596; helper 0x7FA990, **likely** atan2(y, x)) | `UpdateTricons`, playback |

The display word (0x4593A6..0x459605): 0 at entry (0x45923A); +0x90 & 2 → | 1 (| 0x100 if +0x90 & 0x80); +0x90 & 1 →
| 2 (| 0x200 idle); +0x90 & 8 → | 4 (| 0x400 idle). Not in mode 1, the active word fills the gaps (bit 1 from +0x94 &
2, its hint only when the tilt input is 0; bit 2 from +0x94 & 1, likewise with the turn input, which also clears the
angle; bit 4 from +0x94 & 8 and the zoom input [0xC5B0CC]). The clear view above 0.5 makes it 4 exactly
(0x4594F4..0x459507). Then the features: no 1 → &~0x101, no 2 → &~0x202, no 4 → &~0x404; feature 0x80 with GInterface
+0x48 == 0 (the hand **outside the player's influence**) → | 8 (0x459550..0x45956D). Given up (+0x90 & 0x10): 8 when
(GetTickCount / 200) & 1, else &~8 (0x45959C..0x4595C4). The wheel gate +0x2EC or the dropped inputs +0x2F0 → 0, and
with +0x2F0 and feature 0x80 → 8 (0x4595CB..0x4595FE). So the magnifier (4) shows only in the clear view and the cross
(8) is the out-of-influence mark (feature 0x80 is on in the default 0x1BF); the zoom key offers the magnifier too
(below).

✅ **The keys' modifiers in the active word** (verified in the exe, `ProcessKeyMovement` 0x458220..0x4582AE; decomp
CameraModeNew3.cpp:1358..1368): the key movement first puts +0x94 at 0 (0x458266), then, only while the land is not
gripped ([0xC5B0F0] == 0, 0x45826C..0x458277), ORs in 9 with ZOOM_ON (action 6) held (0x458286..0x458295), else 3 when
`IsRotateKeyModifierActive` 0x457F30 (off with ZOOM_ON, on while a fight is watched in its arena, else ROTATE_ON). So
bit 8 → the Zoom icon, bit 2 → Pitch and bit 1 → Rotate: the zoom key offers zooming and turning, the turn modifier
tilting and turning. They come from the modifiers alone, set before any movement key is read: no key of a pair is
needed. [0xC5B0F0] is the land gripped: `Update` sets it from the hand state GRIP_LANDSCAPE (or, in the fight hand
state, the MOVE action) at 0x45AB95..0x45ABCF and clears it with both buttons, with ACTION, with the dropped inputs
+0x2F0, with ROTATE_AROUND_MOUSE_ON and in the wheel gate (writers: `Update`, `Reinitialise`, `ResetCameraModeNew3`).
The keys are read in `GGame::ProcessFrameInputs` (→ `ProcessMapKeys`), after the frame's `ProcessGraphicsEngine` (→
the camera's `Update` and `UpdateTricons`) in `GGame::Loop`, so an update shows the bits of the keys read after the
update before, gated by that update's grip. `UpdateTricons` then ORs 2 / 1 for the tilt / turn inputs, and uses the
word only out of mode 1 and only for an icon the mouse's zones did not set (the mouse's own hint, and its rotate angle,
are kept): each such icon is "only a hint" (0x100 << i) while its input ([0xC5B0C4], [0xC5B0C8], [0xC5B0CC]) is 0, in
use otherwise, and the key-set Rotate puts the angle [0xC5B0AC] at 0 and stops the zones' turning (0x459472..0x4594B0).
In a fight only the zones' Pitch is cleared (0x459398), not the keys'.

Who reads them:

| Reader | Address | Reads |
|---|---|---|
| The fade, a finish-frame callback | 0x4562E0 (`Initialise` 0x4567F0: `RegisterFinishFrameCallback(1000, before = 1, 0x4562E0, 0)`) | [0xC5B0F4] |
| The draw | 0x456E00 (called at 0x45661E) | the alphas [0xC5B0D8..E4], [0xC5B0AC] |
| The empty hand's clip and up, `ObtainRequiredHandPosition` | 0x5B5E70, block 0x5B6866..0x5B6AE6 | +0x90; in playback [0xC5B0F4] |
| The camera hand state's clip, `HandStateCamera::Update` | 0x5B05D2..0x5B0743 | +0x90 |
| The land tooltips | fn_005D6980, 0x5D69B1..0x5D6A71 | +0x90 |
| The hand demo recorder / playback | fn_005DB4D0 0x5DB5CD / fn_005DAEE0 0x5DB0D1..0x5DB0EA | [0xC5B0F4], [0xC5B0AC] |
| Script `WITHIN_ROTATION` (416) | `GScript::GameWithinRotation` 0x70B9D0 | pushes [0xC5B0F4] whole as VM type 6, the boolean (read in the v1.42 exe 2026-10-28), so any icon shown, offered or in use, is true; the word as the fade left it (0x45648F masks it with ~0x707 where the hand state doesn't `AllowCameraTricons` or the tooltips are off, before the script's turn reads it); kept while the player's camera is not updated and its fade does not run (a script's camera), 0 from `Reinitialise` 0x458B78 and `ResetCameraModeNew3` 0x460BEC. openblack: `Game::FillHandTricons` keeps the shown word on the camera (`Camera::SetShownHandIcons`, after `hand_tricons::Shown`), 0 when the player's camera starts again and at the map's clear; `WithinRotation` in `src/CHLApi.cpp` answers it ≠ 0; tests `WithinRotationNative.*`. Used by the tutorial's turn lesson (refs/chl/LandControlT.txt:620) |
| `CameraModeNew3::DrawHandTricon` (a player-coloured sprite at the hand, zoomer 0xC5B078 → GInterface +0x48) | 0x4568F0 | nothing refers to its address: dead code |

`HandState::AllowCameraTricons` is vtable slot +0x10 (read at 0x456488): HandState 0x46BE80 false; Camera 0x5B02E0 and
Normal 0x5B5C90 true; Holding and Grain 0x5B3A70 true unless the locked-select object (+0x39C → +0x3C)
`IsLockedInInteract` (vt 0x6A0); Citadel, Creature, MultiPickUp, PlayAnim, Totem, Tug and Invisible false. The cursor
itself is not changed by the tricons (**likely**: no reader found in the cursor code).

The draw (0x456E00): one `LH3DSprite` [0xC5E170] (`Create(1, 1)` 0x4567C7) with `AtmosMaterial` [0xEDC368] (`atmos.raw`
+ `atmosa.raw`, 256×256, 4 cells a row, sprite +0x30 = 4 at 0x45682C, so 64×64 cells, the cell the flags' low 6 bits).
Icon i (display bit 1 << i) is cell 12 + i, the bottom row: 1 Pitch (cell 12, a vertical double arrow), 2 Rotate (13,
a curved double arrow, turned by [0xC5B0AC], sprite +0x14 only for i == 1, 0x45701E..0x457034), 4 Zoom (14, a
magnifier), 8 the cross (15, red). White, alpha = ftol(a_i × 255) << 24 (0x456FF6..0x457021), nothing when a_i ≤ 0 or
ftol gives 0. Placed at the hand's point (CHand +0x482C → +0x38) projected (`ProjectPoint` 0x819390), with the clear
view at most 0.01 a quarter of the way to CHand +0x486C / +0x4870 ((3·hand + that) / 4, signed, rounded toward zero,
0x456EB4..0x456EDC; above 0.01 at the hand: `fcomp` 0.01 then `test ah, 0x41` / `je` at 0x456E8F..0x456EB2). CHand
+0x486C / +0x4870 is the cursor the hand state follows, copied from +0x485C / +0x4860 at the end of each hand state's
update (`HandStateNormal::Update` 0x5B744B..0x5B7461, `HandStateCamera::Update` 0x5B0B53, and the others), so the
ring point while the edge turns the camera. Clamped to
x ∈ [16, W − 16], y ∈ [top + 16, top + viewH − 16] with the cinema bars (HelpSystem +0x45E8, 0x456EEE..0x456F7A), then
`Get3DPointFromScreen` at the hand's depth z; its size max(z × 1/22 [0x8C7BF4], 0.0001) at depth z, a constant size on
screen (sprite +0xC, +0x18 / +0x1C, 0x457045..0x45708A; +0xC is the sprite's half size: the quad spans ±Size − OffsetX by
±Aspect × Size − OffsetY, `LH3DSprite::Draw` 0x840831..0x8408CF [V]). All four at the same place, in bit order (the cross on top), with ZFUNC ALWAYS (render state 23 =
8, put back to 4, 0x456FA7..0x456FC9 / 0x4572A5..0x4572CB): over everything 3D. The "before" callback at priority 1000
draws after the hand and the Z-sorter flush, after the falling spell and the help dude (100), before the sun glare, the
cinema bars, the 2D rectangles (10000) and the help text (20000).

The fade (0x4562E0 → 0x456499..0x456618), per icon, with the camera's time step (`GetCameraTimeInc` × 0.001): a bit on
aims at 0.6 with its hint bit, else 0.9 while the grip [0xC5B0F0], the middle button [0xC5B0EC] or a camera input
[0xC5B0E8] is on in CameraModeNew3, else 1.0, rising and falling at 5/s ([0x9CE638]); the cross aims at 0.2, 0.6 with
an object under the hand (GInterface +0x400), 1.0 while the right button [0xC5B0B4] is held (copied by `Update`
0x45AC07..0x45AC3C into [0xC5E164]), 0 in a hand demo's playback or while the suppression timer [0xC5E178] > 0, falling
at 0.3/s and rising at 5/s ([0xC5B0E4], 1 at full, has no reader). A bit off falls at 5/s to 0. `CameraModeNew3::SetNoCross(250)`
keeps [0xC5E178] at least 250 ms, counted down by g_delta_time in `UpdateTricons`; its callers are in the interface's
hand state (fn_005D7E40 / fn_005D7F20): the state row's flag (0xD1828C, 0x5D7F08), a leash under the hand (0x5D7F6D),
an interactable object with the hand in the influence or not needing it (vt 0x714, 0x5D7FB5), a bubble (0x5D8060).

The fade and the draw need (0x4562E0..0x45641D) a camera, the local player, the interface, g_game +0x250538 ≠ 0
(**unknown** name), the camera's mode CameraModeNew3, or CameraModeScript in a hand demo's playback, the hand shown
(CHand +0x483C == 0), the interface active (vt 0x40C), not in the citadel (g_game +0x205A28 == 0) and no active
SetupBox with +0x94; when one fails the alphas stay as they were. Then 0x456457..0x45648F clears the icons 1, 2, 4 and
their hints (&~0x707) when TOOLTIP_LEVEL (HelpSystem +0x45FC, profile "TOOLTIP_LEVEL", default 2, fn_005C6CF0) is 0, or
the hand's state does not `AllowCameraTricons`; not the cross. In a hand demo's playback the same draw adds a mouse
picture (`CameraHelp::DrawKeyOrMouse`, size 0x20, 0x4570A2..0x4572A2) 32 px left or right of the hand (hysteresis
[0xC5E174]: past 2W/3 left, below W/3 right), lit with the recorded buttons (GInterface +0x39: both → action 6, grab →
1, action → 2), and the help text 0x1407 `HELP_TEXT_HAND_DEMO_TOOLTIP` «Demo». The place is the icons' own, after the
lean and the clamp (0x456F37..0x456F7A); TOOLTIP_LEVEL does not matter and there is no fade (alpha8 0xFF every
frame). With the mouse on the left, `DrawKeyOrMouse(anim, click, row, text, x − 32, y, 32, align 0x11, c1, c2, 0xFF)`,
on the right x + 32 and align 0; c1 bytes {00, 00, FF, FF} (red, 🔍 inferred by the tooltip's yellow's byte order), c2
white. With no button lit the picture is animType 0, clickType 0 and the mouse row while action 1 is on the mouse (else
3); else `ConvertActionToKMIcon`, which falling back gives 0, 0 and the mouse row (0x4570C0..0x45719C). openblack
(2026-10-21, fork issue #157): `Game::FillHandTricons` fills `OverlayFrame::handDemoMouse` while `hand_demo::IsPlaying`
(`hand_tricons::LabelOnLeft`, `DemoMouseAction`, `PlaceDemoMouse`; tests `HandTricons.TheDemoMouse*`), and
`Renderer::DrawHandDemoMouse` draws it after the icons.
- **The lit buttons** (GInterface +0x39 bits 0 / 1, traced 2026-10-21): `Msg_GrabDown` 0x5D98E0 / `Msg_ActionDown`
  0x5D9910 set them only while +0x38's low byte is 0 or has bit 0 (idle, the land grip, or an action that keeps it 0);
  the busy states latch the low byte (0x10: the grab waiting 0x5D17D1, a bubble 0x5D433A, a seed's apply 0x5D2745, the
  pick-up 0x5D2872; 0x20 the give 0x5D30C1), so a press then lights nothing. The ups always set 0x2000 / 0x4000. They
  go out: in the idle state at any up (0x5D51A9); in the land grip (state 2, 0x5D4A40) an up clears only its own
  button; when what the press started ends (the tap 0x5D390B, the pick-up's 0x10, the leash's double click 0x5D4311,
  the states' own ends). Playback feeds the recorded messages through the same handlers (fn_005DAEE0 → fn_005D9BC0,
  0x5DB2EE) but never calls the per-message clear fn_005D9A10, so the rule holds as it is. So a pick-up's button goes
  out as the pick-up is sent, though it is still held. openblack: `hand_tricons::StepLitButtons` once a frame at the end
  of `HandSystem::Update` (busy: a grab waiting, a screen grip, a seed's apply, a gift given; ended: a tap sent, the
  waiting grab resolved, the leash's double click), read through `CameraIconFacts::moveLit` / `actionLit`; tests
  `HandTricons.APressTheHandTakesStaysLitUntilWhatItStartedEnds`, `HandTricons.GrippingTheLandAReleasePutsOutOnlyItsOwnButton`.
  Once a frame rather than once a message, which is **equivalent** for the demos the game plays (checked 2026-10-21
  on the 16 shipped `Data\HandDemo\*.hnd`): the draw comes only after every record due that frame (fn_005DAEE0, then
  the frame's one `InterfaceActionProcess`), and two button messages are never closer than 150 ms but once,
  zoomreminder.hnd's grab up and action up 10 ms apart, which both ways put both buttons out (in the land grip each up
  clears its own and then none is left, 0x5D4A40; idle the first up clears both). Test
  `HandTricons.BothUpsInOneFrameEndTheZoomAsTwoMessagesDo`.

The hand's poses from the zones (**verified**): in the empty hand (HandStateNormal passes allowTricons = 1 at 0x5B71A7,
HandStateHolding 0 at 0x5B3F79), with no object under the cursor (0x5B60BB), not the leash state 0x15, in
CameraModeNew3: +0x90 & 1 and feature 2 → clip 0x2D Crotate, up = normalise(focus − hand point, with y += 0.3 × the
length) (the look-at [0xEA1DC4], 0.3 [0x8AB23C], 0x5B68F4..0x5B69B7; with & 1 and feature 2 off, no pitch test either);
else +0x90 & 2 and feature 1 → 0x30 Cpitch; CHand +0x4874 (the edge rotate's ring copy) → 0x2D with the same up
(0x5B69BB..). In playback the display word maps back (2 → bit 1, 1 → bit 2, 4 → bit 8) and CHand +0x4858 = 1
(0x5B68CB). The camera hand state (0x5B05D2..0x5B0743): HandStatus 3 or the clear view above 0.01 → 0x2A Cgrip; else
+0x90 & 1 → (feature 2) 0x2D Crotate with the same up and a lift of 3.2 × the hand's scale × 0.33 [0x8CA268]; else & 8
→ (feature 4) 0x33 Czoom; else & 2 → (feature 1) 0x30 Cpitch; otherwise clip 0, Cwiggle ([esp+0x10] = 0 at
0x5B040E). The poses are on [the navigation poses](../hand/animation-and-placement.md#the-navigation-poses).

The land tooltips, fn_005D6980, after `CalculateLeashToolTip` (the leash wins), the action preset to 1:

| Order | Condition | Text | Action | Align | Help event |
|---|---|---|---|---|---|
| 1 | +0x90 & 2 and feature 1 | 0xE76 «Tilt» | 1 | 0x300 (up/down arrows) | 0x1C |
| 2 | +0x90 & 1 and feature 2 | 0xE77 «Rotate» | 1 | 0xF00 (all four) | 0x19 |
| 3 | the point is water, in the influence, a fish within 2 | 0xE73 | 2 | the caller's | 0 |
| 4 | CameraModeNew3, +0xC8 (the screen centre on land): `HelpProfile::Trigger(0x2C)`; and +0x2E4 < 15 (the eye to the screen-centre land point, at least 10, 0x45BD29..0x45BD8E), `CalculatePitch` > 0.55 and feature 4 | 0xE8B «Zoom out» | 4 | the caller's, always 0 ([tooltips: state 3](../interface/tooltips.md#state-3-normal-the-land-texts)) | 0x1D |
| 5 | otherwise | 0xE7E «Move» | 1 | 0xF00 | 0x21 |

They read the zones (+0x90), not the display: no blink, no cross, no TOOLTIP_LEVEL gate, the features directly. In
`Scripts\info.dat` (the last 170 × 12 bytes) 0xE76, 0xE77, 0xE8B, 0xE7E and 0xE73 are all priority 0.5, display 1.0,
after-focus 0, so each shows 2.5 × 1.0 × 10 = 25 turns once taken over. `SubmitToolTips` 0x5C9A70: while the shown
text's turns run only a forced text takes over (0x5C9AE0..0x5C9AEE), so «Rotate» replaces «Move» at an edge only after
«Move»'s 2.5 s; at TOOLTIP_LEVEL 1 they are all dropped (priority < 0.9, 0x5C9AB0..0x5C9ACA). The tricon texts exist
only inside the land tooltips, which hand states 3 and 20 call, and the others only as a fallback: 5 (out of the
influence, 0x5D6D54), 9 (0x5D6F28), 24 (0x5D6FB7 → 0x5D6FCD), 13 (0x5D78B7) and `CalculateCanSelectLock`'s land branch
(0x5D76AC); so an object's tooltip always wins over a tricon one, and the leash over both.

#### Player camera: flights to a place

`CameraModeNew3` flies in two legs, through a middle point, with the woosh.

- **`FlyToPosFoc(pos, foc, t)` 0x4587F0 (vt +0x50, 0x4587F0..0x4588D6):** dir = foc − pos, per component rounded to
  float, made unit unless it is 0 (NaN counting as 0) with 1 / sqrt((z·z + y·y) + x·x); `InsideInclusion(pos, dir)`
  false → pos = its closest point (written through the caller's reference; only with the force field on, see the
  zones); +0x78 = 0; t ≥ 0 (only t < 0 or NaN skips, `test ah,1`) → `SetupVia(pos, foc, the origin zoomer's current
  value, t)` (+0x118 / +0x148 / +0x178); the woosh 46 + (GetTickCount() & 3), sounds 46..49 G_Woosh_01..04,
  non-positional (`GAudio::PlaySoundEffect` 0x429D60 with mode 3, loops 0, 2D, InGame), only when the current origin is
  more than 100 [0x9CE640] × 1.5 = 150 from pos, sqrt((dy² + dx²) + dz²) (0x458967..0x45899B).
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
  casts; 🔍 inferred complete, as a call through a mode pointer kept elsewhere would not show):
  - the bookmark keys: `Bookmark::SetCameraToBookMark` 0x43A070 at 0x43A1C8, from `GGame::ProcessKey` 0x63F632 (a bookmark key without a modifier):
    0.3 (0x3E99999A), to the bookmark's saved view;
    - `Bookmark::SetFocusToOwnedTown` 0x43AA60 at 0x43AC80, on Tab with neither Shift (previous bookmark, 0x63F654)
    nor Ctrl (next bookmark, 0x63F672) held (`GGame::ProcessKey` 0x63F681; LH_KEY 15 `KB_TAB` through the jump table
    at 0x63F45E) [V]: 0.3, to the player's next town after the last one focused (`Bookmark::FocusTownID` [0x9CD388];
    the list walked from its end, wrapping; towns with neither a storage pit nor a town centre skipped), at its
    storage pit, else its town centre (+0x9A4), seen from x − 24.45 [0x8C6B44], y + 28.3786 [0x8C6B48], z + 47.4
    [0x8C6B4C]; bookmark 8 is set there (0x43ABE6), and it flies only from a `CameraModeNew3` (0x43AC21);
  - the debug camera editor, `camera_editor_callback` 0x449C99 and 0x449D47: 0.3;
  - `CameraModeNew3::Restart` 0x4587EB (after `Reinitialise(1)`): 0.1, to +0x0C / +0x18;
  - `CameraModeNew3::ZoomToCitadel` 0x457D80: 0 (ebx, zeroed at 0x457B9F); 0x457DEA and 0x457ED2: 0.4 (0x3ECCCCCD);
  - `StartFight` 0x45A71D: 0 ([watching a fight](../creature/fights.md#player-camera-watching-a-fight));
  - the double click calls `SetupVia` itself (vt +0x54, 0x45E2DB) with 0.1 (0x3DCCCCCD), only for the 100 flight
    (+0x78 == 0, 0x45E2A1), from [esp+0x110].

The camera that watches a creature fight (`StartFight`, the orbit, the drag away) is on
[the creature's fights](../creature/fights.md#player-camera-watching-a-fight); openblack's side of it is below.

#### Player camera: the bookmarks' markers

Read 2026-10-27 in bw1-decomp (`src/Black/Bookmark.cpp`) and the exe; the numbers are the oracle's.

- **The pulse.** Once a drawn frame of the land, the object list fn_0x005e5cd0 calls
  `BookmarkGraphic::DrawLoopUpdate` 0x43A9E0 (0x5E6176, its only caller, after the game list's draws and just before
  `Bookmark::DrawAll` 0x439B90). It adds `g_game_time_inc` [0xEA9EC0] (the frame's whole game ms, 0 while paused) to
  `PulseTime` [0xC58CC8], an int; above 2000 (0x7D0) it takes the remainder (`idiv`), and exactly 2000 stays. Then
  `PulseWidth` [0xC58B24] = |fcos(float(t) × 0.0005f [0x8C6B40] × 6.2831855f [0x8AB210])| + 0.2 (the double of 0.2f
  at [0x8C6B38]), stored as a float, and `PulseHeight` [0xC58B20] the same at (t + 200) % 2000. Each product rounds to
  a float (the 24-bit FPU); the cosine and the sum round once, at the store. Nothing else writes the three statics
  and only `BookmarkGraphic::Draw` reads the two sizes, so every marker shares one clock that nothing resets: it is
  kept from land to land and stands still in pause. It does not move in the temple, under the falling spell's film or
  behind a full screen film, which skip the land ([where the list runs](../rendering/object-draw-list.md#where-it-runs-in-a-frame)).
  The oracle's `DrawLoopUpdate` over every time 0..2000 and seven wraps matches openblack's formula in all 2008 cases.
- **The markers.** `Bookmark::DrawAll`: only while `ShowBookmarks` [0x9CD384] is set and the help system's wide screen
  is not held; for each of the bookmarks 0..7 that is set, its graphic is made once by `BookmarkGraphic::Create(i, -1)`
  0x43A7D0: an `LH3DSprite` in the material [0xEA1AB0] (misc0.raw, mode 6, two-sided), colour 0xFFFFFF00 (yellow),
  frame i + 0x18 (0x38 for a creature's graphic), its origin 0 (`SetToZero` 0x8404F0). It stands at the target's 3D
  position (its land point when it has no 3D object), or the land point of the bookmark's map position, plus 2 in y,
  and goes to the Z-sorter (`AddDrawing` 0x43A880).
- **The draw** (`BookmarkGraphic::Draw` 0x43A8D0): half width = max(PulseWidth, 0.0001) (the double 0.0001
  [0x8BF518]), stretch = PulseHeight / PulseWidth, the origin scaled by the new size over the old (0 stays 0), then
  `LH3DSprite::Draw`, which makes the half height stretch × half width. A creature's graphic (type ≠ -1) draws a
  second sprite: frame `fn_0081F340(type)` & 0x3F, angle −π/2 (0xBFC90FDB), half width max(0.7 × PulseHeight,
  0.0001), stretch (0.7 × PulseWidth) / PulseHeight; then the angle and frame are put back (the size and stretch stay).
  The turn sends its local x up the screen ([the screen sprite](../rendering/meshes-and-lod.md#objects-that-face-the-camera-billboards)).
  The oracle's run of the draw (the sprite draw and the type's frame stubbed) leaves size 0x3F9355D2 and stretch
  0x3F2F34F5 at the pulse of 100 ms, and for a creature's graphic size 0x3F0D2BCD and stretch 0x3F82EAD1.
- **The creature's marker, and which frame's pulse it reads.** `Creature::Initialise` makes it
  (`BookmarkGraphic::Create(1, type)` at 0x474644, the type from the creature's info +0x1F4); sub_4F4CC0, the tail
  of `PSysGlobal::DrawLoop` 0x68F5E0 (0x68F655), places it while the creature's +0x11F0 is set, at its target thing's
  point (`GetCreatureTargetFromPos`) or else the point +0x11D8, with no 2 added, and adds it to the Z-sorter. The
  second picture's frame is `fn_0081F340(type)`: [0xC38230 + 16 × type], types 0..17 giving 0, 2, 4, 4, 4, 4, 3, 5, 6,
  1, 1, 2, 0, 0, 0, 2, 0, 7 and type 18 or more type 0's. In `GGame::Process3dEngine` 0x54DA80 (bw1-decomp
  `src/Black/Game.cpp:2078`) both run in the world view's case only: `landscape.Draw()` (0x54DEDE, with the pulse's
  step) comes before `PSysGlobal::DrawLoop()` (0x54E019), and every marker is drawn when the Z-sorter is drawn after
  them, so the creature's marker and the bookmarks' read the same frame's pulse, the one just stepped. openblack
  steps it in the object draw list's step, which runs on the same frames (the world drawn: not in the temple, under
  the falling spell's film or behind a full screen film); a draw of the creature's marker reads it after that step.

### Sources

- runblack.exe v1.42 through xref and its disassembly (all of `CameraModeNew3::Update` 0x45A960 was read for the
  zones); bw1-decomp's symbols for the names.
- The camera's code notes moved out of openblack's `src/Camera` (the zones, CameraHelp, the player's model).
- The tricons' study of the disassembly, with a render of `atmos.raw` for the icons.

## openblack

Porting decisions against raffclar's tree (the zones, the tricons, the hand's poses):
[port decisions](../project/port-decisions-hand-camera-interface.md).

- **The zones** (`src/Camera/PlayerCameraScript.{h,cpp}`, `player_camera::`; `src/Camera/CameraZones`): the loader
  (`SegmentReader`, `player_camera::ReadZoneFile`), SET_CAMERA_ZONE (`CameraZoneSystem::SetZones`, which drops the
  zones set before, as `CameraExclusion::ResetExclusionFile`, then reads the file, as
  `CameraExclusion::LoadExclusionFile`) and `ForceRotateAboutPoint` (`CameraModeNew3::ForceRotateAboutPoint`) are ✅
  faithful; `CameraModeNew3::ForceFieldPoints` and `ForceFieldPointCount` (0x3000 bytes) are `forceFieldPoints`, and
  `InsideInclusion` is `camera_zones::CrossFence`. openblack keeps the
  file in the resources (`CameraZoneLoader`, by "camera/zones/<name>"), read once, and the zones it sets in
  `cameraZoneSystem` (`camera_zones::Zones`: `_UseExclusions` is `exclusionsOn`, `DrawForceField` `fenceOn`,
  `_UseMaxHeight` / `_MaxHeight` `useMaxAltitude` / `maxAltitude`, `_UseMaxDistance` / `_MaxDistance`
  `useHeightAboveLand` / `heightAboveLand`; the file's `player_camera::Zone` has the same names, and `useExclusions`
  for `_UseExclusions` and `drawForceField` for `DrawForceField`, its reset values pinned by
  `PlayerCameraZone.AZoneStartsAsTheGameResetsIt`; a script's zones always have `exclusionsOn` and `fenceOn`, whatever
  the file says); `cameraZoneSystem` is made again with each land. The constants:
  `k_ZoneSegment`, `k_ZoneFolder`, `k_DefaultZoneLimit` (500), `k_MaxForceFieldPoints` (1024; **🔍 inferred** more
  would overrun the original's array, so they are clamped), `k_ExclusionRecordSize` (0x28),
  `k_NoInclusionDistance`, `k_NoForceFieldDistance` (and `camera_zones::k_NoFenceDistance`). `ReadZoneFile` says the
  file was not read whole when the file or the segment is not there or is short (**🔍 inferred**: `GetSegmentData`'s
  own checks were not read; what was read before stays), and `SetZones` returns false, with no zones, when the file
  can't be read. GET_INCLUSION_DISTANCE pushes `inclusionDistance`, which the fence step writes.
- **The fence** (step 5 of [the zones' steps](#what-the-player-camera-does-with-the-zones)), from raffclar's newer
  master where it matches the exe (2026-10-21; his `towards` test and the way's normalisation took a distance or a part
  that is not a number the other way, corrected): `camera_zones::PushInsideFence` (the line towards where the camera
  started the frame, or towards the focus when it moved less than 1e-4 squared across the ground or the distance is not
  a number; inside, the inclusion distance; outside, the way normalised unless all its parts are 0 or not a number,
  the 32 × 2 tries at radius 20, or 50 in the turning drag, `camera_zones::SearchRadius`; the crossing itself when none
  is inside), and `DefaultWorldCameraModel::KeepInsideFence` after `ConstrainCamera`: the origin to the target across
  the ground, the focus and the turning point (`_focusAtClick`, +0x12C) moved with it (`MovedWithOrigin`), then the
  hit: `CameraHelpCallback(HitAllowedAreaEdge 0x202)` in a frame of the player's input (`_dragCountsForHelp`, [0xC5E168]), the out
  of bounds sound (InGame 43, no owner, at the drawn camera, mode 2, 3D), `camera_shake::Create(100, origin, 1.0,
  400 ms, not y only)`, and the controls but the wheel dropped until they are let go (`_fenceHeldBack`, +0x2F0, the
  hold of a drag given up, below) unless turning by the edge drag. The fence's adjustment counts as the frame's origin
  adjusted, as [esp+0x4B] = 1. Without a fence the inclusion distance is 1e10, as the original's update writes. Tests
  `CameraZones.ACameraOutsideTheFenceIsPutBackPastWhereItCrossed`,
  `CameraZones.WhatMovesWithTheCameraKeepsItsPlaceFromIt`; the Camera debug panel shows the zone, the inclusion
  distance and how many times the camera was put back. Not ported: the force feedback effect (openblack has no
  force-feedback mouse, and the original plays none without one) and the force field's pulse, as the force field
  itself is not drawn. **⚠️ approximate**: openblack's update is not in the original's step order, so the fence comes
  after its own constraints (`ConstrainCamera`) and before the self-tilting camera's height and the clear view.
- **CameraHelp** (`src/Camera/CameraHelp.{h,cpp}`, `camera_help::`, kept by `Locator::cameraHelpSystem`):
  `CameraHelp::features` (`k_NormalFeatures` 0x1BF),
  `autoPitch` / `autoPitchHeight` (`k_DefaultAutoPitchAngle` 0.52359879, the exe's 0x3F060A92; 75), `EnableCameraFeatures`,
  `SetAutoPitch` and the `Feature` bits (`Feature::AutoPitch`, `AutoPitch` 0x40; the names 🔍 inferred from the levels); the 0x80 and 0x100
  bits are only set here. `camera_help::Reset` puts the features, the self-tilt's angle and its height back to their
  start at every land load, after the statistics' clear, as `ClearMap` writes them; the features are written again by the interface interaction's normal level
  only when the scripts restart (`Game::ResetScripts`), and a land change keeps the interface interaction
  ([what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps-and-what-only-a-script-restart-resets)). Their save and load with
  the game (`GGame::Save`) are not ported.
- **The player's model** (`DefaultWorldCameraModel`): the features in `DefaultWorldCameraModel::Update` and
  `HandleActions`, the bookmark flights in `DefaultWorldCameraModel::SetFlight` and the woosh in
  `DefaultWorldCameraModel::PlayWoosh`. In detail: the features in `Update` and `HandleActions`
  (`camera_help::GetEnabledFeatures`, the double click needs 0x10, the land grab 0x08, both buttons and the middle
  button 0x20 as our bit `Feature::JustZoom`); the zoom scale (`k_StepScaleMin` 60, `k_ZoomScaleMaxBelowFocus` 4 × 60,
  `k_ZoomScaleMax` 2000) and the
  zoom delta × 0.0015; the camera help's events through `help_profile::OnPlayerCameraMove` and
  `help_profile::CameraHelpCallback` (`CameraReason::DoubleClickPos`); the confirmation feeds; the woosh
  (`PlayWoosh`: `audio::PlaySoundEffect` with 46 + (`audio::TickCount()` & 3)).
- **The camera help hears only the player's own controls ✅ faithful** (fork issue #216):
  - The turn, tilt and zoom of a frame are reported in the update of the frame whose controls gave them, whatever the
    camera's mode (`help_profile::ReportsOfPlayerCamera`, pure: zoom, then the turn above 0.01 and its way, then the
    tilt), as the original's zoom in every update and turn and tilt in mouse modes 2 and 3 (a zoom or a key's turn
    with the land gripped counts too). Before, only the Polar and ArcBall modes reported.
  - An update without its frame's controls hears none of them: `Camera::SkipActions` (Game's input step, when a
    script's camera, a hand demo or a camera path has the camera as the frame begins) and `Camera::SetModel` (the model
    handed the camera) tell the model, and `Update` clears what it heard (`_controlsRead`). Before, the controls read in
    the frame a script took the camera were counted once more when it gave the camera back, and those of the frame
    before a hand demo every frame of a demo played without a script's camera.
  - The input mask of the turn, tilt, zoom and drag reports (`help_profile::InputMask`, pure, from `HandleActions`:
    the wheel `notches`, the middle button, the turn, tilt and zoom read, the movement keys, the land gripped without
    the middle button or both buttons held, and both buttons). The collisions and the double clicks keep 0.
  - Tests `PlayerCameraReports.*`, `CameraInputMask.*`, `TimersEventsTest.TheInputMaskChangesNothingTheScriptsCount`
    (`test_timers_events.cpp`), `WorldCameraMouse.AKeysTurnCountsWithTheKeysAsItsInput`,
    `TheMiddleButtonsTurnHasOnlyTheMiddleButtonAsItsInput`, `AZoomWhileTheLandIsGrippedCounts`,
    `TheCameraHelpHearsNoControlsLeftFromAFrameWithoutAnUpdate`, `AModelHandedTheCameraHearsNoControlsItReadBefore`.
  - Debug: the Camera window's "The camera help" (closed at first): whether the last update heard its own frame's
    controls, their input by source, its reports and the game turns counted for the scripts. With `-L input=trace` the
    log has a line for each move the camera help hears (`Camera help: 0x300 from the input 0x01 in game turn ...`).
- **The controls of an earlier frame do not move the camera ✅ faithful** (fork issue #216): in a
  frame whose controls are not read (`DefaultWorldCameraModel::SkipActions`: a script's camera, a hand demo or a camera
  path has the camera as the frame begins, or the model is handed the camera), the player's camera is left as a frame
  with nothing held leaves it: no turn, tilt, zoom or move, the land let go of, no hold after a drag given up or the
  fence, no self-tilt, and the mode of a frame without input. So nothing read in the frame a script took the camera
  moves it when the script gives it back, as the original's player camera, made again with no input when a script's
  camera control ends (`Reinitialise` clears the inputs, the mouse mode and the hand's status). Before, the turn, tilt,
  zoom, move or drag of that frame moved it once more. Tests
  `WorldCameraMouse.ATurnReadAsAScriptTakesTheCameraDoesNotMoveItWhenItComesBack`,
  `TheLandGrippedAsAScriptTakesTheCameraIsLetGoOfWhenItComesBack`. Debug: the Camera window's "The player camera's
  controls" (closed at first) counts the frames whose controls were not read.
- **The player's camera made again when a script's camera control ends ✅ faithful** (fork issue
  #216): `script_camera::End`, where the script's mode is deleted, calls `CameraModel::Restart`, as the original makes a
  new `CameraModeNew3` (0x6ECE1D: `Initialise` 0x456640, then `Reinitialise(false)` 0x4589B0, decomp
  CameraModeNew3.cpp:1213..1221, 1546..1619). `DefaultWorldCameraModel::Restart`:
  - the controls dropped as in a frame without them (`DropControls`: the inputs [0xC5B0C4..0xC5B0D4], the buttons and
    the grip, the hand's status, the hold +0x2F0, the mode);
  - the easing's time from 0 (`_elapsedTime`, ElapsedTime +0x78: the next 1.5 s ease the camera from 1.5 s down to
    0.3 s, as a new mode's do);
  - the clear view put away at once (`_clearView.SetPosition(0)`, the zoomer [0xC5B100]);
  - a flight on its way dropped (`_flightPath`, +0x100 / +0x101);
  - a watched fight let go, with its orbit where `Reinitialise(false)` leaves it: not watching, no arena, turned a
    quarter round, untilted, 1 out, no time left or over an arena, ended, its first frame to come and its turn and
    focus zoomers at 0 (HasFight, arena, Yaw0, Pitch0, FightDistance, FightTimeLeft, TimeInArena, FightStatus 2,
    +0x2D0, +0x210, +0x240).
  - What openblack keeps no counterpart of: the zoom to the citadel's toggle and its timer (+0x8, +0x2F8;
    `ZoomToCitadel` 0x457B60 is not ported), the wheel gate +0x2EC (not ported), the edge scroll's speed +0x68
    (`UpdateStrafe` 0x45A080, not ported), IdleTime +0x2E8 (no reader in the mode's code), the mouse's positions and the
    mouse's and the wheel's accumulators (openblack reads each frame's mouse afresh), the turning point (RotatePoint,
    RotateAroundPoint: the middle button's point is taken as it is pressed), and the distance, heading and pitch and
    the fallback place read from the camera (HeadingDistance, Yaw1, Pitch1, FallbackOrigin, FallbackHeading: openblack
    works them out from the camera at the next change of mode).
  - The camera help icons by the hand start again: `Reinitialise` zeroes the icons' fades
    [0xC5B0D8..0xC5B0E4] (0x458B80..0x458B92) and the rotate icon's angle [0xC5B0AC] (0x458B98), so each fades in
    afresh from nothing (`tricondraw` 0x4562E0, decomp CameraModeNew3.cpp:895..945, keeps them from frame to frame and
    leaves them while a script's mode is current). `script_camera::End` publishes `PlayerCameraRestarted` after
    `Restart`, and the game's handler calls `hand_tricons::Restart` on `Game`'s `_handTriconFades` and
    `_handTriconAngle`. Their display word [0xC5B0F4] (zeroed at 0x458B78) has no kept counterpart: `UpdateTricons`
    0x459230 sets it from 0 every update of the mode (decomp :1714), as openblack picks the icons afresh every frame
    (`hand_tricons::WorldCamera`). Test `HandTricons.TheEndOfAScriptsCameraControlStartsTheIconsAgainFromNothing`.
    Debug: the debug overlay's "Camera Icons" line shows their strengths, the Camera window counts the restarts.
  - Tests `WorldCameraMouse.TheEndOfAScriptsCameraControlRestartsThePlayersCamera`,
    `ARestartedCameraEasesFromTheStartWithNoClearViewAndNoFlight`,
    `WorldCameraFight.ARestartedCameraLetsTheWatchGoAndAWatchAfterStartsAfresh`. Debug: the Camera window's "The player
    camera's controls" counts the restarts and gives the last one's game turn.
- **The land grip ✅ faithful:**
  - A drag grips at the camera's first update after the press (`LandGrip`: the camera, the cursor,
    `camera_pan::PlaneThrough`, the depth, whether there was land under the cursor). The depth follows the zoom.
  - Too deep, the drag is given up before the pan: every control is dropped until a frame with none, the hand cue is
    `camera_drag::tricon::k_GivenUp`, and with feature 0x10 the camera goes 1000 from the land under the mouse the same
    frame, keeping its heading, the pitch clamped, without a midpoint, with the woosh.
  - The wheel in this hold and in the fence's ✅ faithful (fork issue #216): the wheel and the zoom
    in and out actions are read after the hold, as the original's, so they still zoom and count, and neither are
    dropped nor keep the hold. Before, the hold dropped the wheel's zoom and was kept by it. Test
    `WorldCameraLandGrip.TheWheelStillZoomsAfterADragGivenUpAndDoesNotKeepItsHold`. Debug: the Camera window's "The
    player camera's controls" shows whether the controls are held back, and the zoom asked for.
  - Otherwise `camera_pan::Pan` from the grip camera, then `StopPanShortOfLand`: `LandIslandInterface::RayCast` from
    the grip camera, with the drawn camera for the 7500, and `camera_pan::StopShortOfLand`. The cursor is not warped.
  - The camera help ✅ faithful: each frame of the pan counts Drag (33) when the frame has a camera input, or the grip
    (or the middle button or both buttons, with feature 0x20) is held with the mouse moved more than 2 pixels across
    or down (`_dragCountsForHelp`, from `GetMouseDelta`). The flight after a drag given up reports as the double
    click's flight does, once (`ReportFlightToHand`).
  - **⚠️ approximate** Floats where the original has doubles (n · r, t). The drag needs feature 0x08 and the hand over
    the land to start (whether the original gives up a grip without 0x08 is not read). The flight's point is the
    double click's: the object under the hand (0x306), else the land under the mouse (0x305); the object is the one
    the interface had as the grip started, kept through it.
  - Tests: `WorldCameraLandGrip.*`, `WorldCameraPan.*`, `CameraPan.*` and the recorded `DragUpDown`.
- **The mouse drags (faithful, `DefaultWorldCameraModel::FollowDrag`, `camera_drag::`):**
  - The hints and the classifier are raffclar's `IdleTricons` / `DragClassifier`; the very bottom and the top start at
    0.45 in a window, read from openblack's display mode (`EngineConfig::displayMode` Windowed).
  - The hints follow the cursor only while no drag is held (`camera_drag::HintsFollowCursor`), so the frame a drag is
    pressed keeps the frame before's, as the original's mode is set before `UpdateTricons` runs (0x45BDE9..0x45C00E, then
    0x45C01A). Before 2026-10-20 the press frame read them at its own cursor, and a fast drag already past an edge
    threshold started an edge turn instead of a pan (fork issue #151). Test
    `CameraDrag.ThePressKeepsTheHintsOfTheFrameBefore`.
  - Until the drag is decided the camera stays (Cartesian). A pan is the land grip above.
  - The edge rotate (his `EdgeRotate`) warps the cursor with `GameActionInterface::WarpCursor` and turns the camera by
    the angle swept, as turn input × w / π; without feature 2 it does nothing. It reports Rotate with the drag flag,
    RotateCW above 0.01 and RotateCCW below 0.01, the original's quirk included.
  - The pitch drag (his `PitchStep`, the camera's horizontal field of view) tilts the camera, without feature 1 it does
    nothing; it reports Pitch with the drag flag.
  - The player's own turn and tilt reports leave the drag's out, and a grip with only a keyboard move drops the move.
  - The tricons the model works out are `CameraModel::HandCues` (`GetHandCues`: the zones idle, the classifier's while
    dragging, `k_GivenUp`); the hand's poses and the helper icons read them.
- **The helper icons by the hand (the tricons' draw)**, raffclar's `hand_tricons` (`src/Hand/HandTricons`, its four
  commits up to 9b8718f30, unverified against v1.42) on our camera: `DefaultWorldCameraModel` fills
  `HandCues::icons` (`hand_tricons::WorldCameraFrame`: the hints, the grip, what the keys' modifiers offer, the turn,
  tilt and zoom asked for, the drags, the middle button, a watched fight, the clear view, the features, a drag given up)
  and `HandCues::clearView`; `Game::FillHandTricons` adds the hand's influence and the cross's nudges
  (`HandSystemInterface::GetCameraIconFacts`), the tooltip level and the hand state's allow-list
  (`hand_tooltips::ShowsCameraIcons`), fades them with the camera's frame time (`game_clock::CameraFrameMs`), places
  them at the hand's point projected, the point the tooltip's icon uses, as the original (his anchor at the posed
  hand's wrist bone, `hand_tricons::AnchorPoint`, is not used), leaning towards the cursor the hand follows (`hand_tricons::Place`; while a hand demo plays, the demo's own mouse, `hand_tricons::FollowedCursor`, as the original's icons lean to the cursor its hand state copies: since 2026-10-29, before which they leaned to the player's real pointer and took the demo mouse and its word «Demo» with them, test `HandTricons.TheIconsLeanTowardTheCursorTheHandFollows`) and lays them out (`hand_tricons::Sprites`) into `OverlayFrame::handTricons`;
  `Renderer::DrawHandTricons` draws them in the atmos material after the floating numbers and before the cinema bars.
  A hand demo's recorded hints and angle (`hand_demo::PlayingHints`, `PlayingHintAngle`) replace the camera's while it
  plays, with no cross. The cross's hold-back (`SetNoCross(250)`): `HandSystem::InterfaceHandState` reports the turns
  whose hand state holds it back (a screen bubble, a lock starting or ending, the row of a bubble grip, an object the
  hand may act on: `hand_tooltips::HoldsCrossBack`; a creature in the influence, 🔍 inferred), counted in
  `CameraIconFacts::crossHoldBacks`; each new one keeps the cross away for at least 250 ms, counted down every frame
  (🔍 inferred: by the wall clock's frame time for g_delta_time), and the cross goes at once while it is held. The
  leash's hold-back waits for the leash's hand state (not ported). Not drawn, the strengths kept, with no camera or hand, the interface put away, in the temple or
  under a box, or under a script's camera. 🎯 **Chosen difference (user, 2026-10-20)**: the icons' size is taken from
  the width of a 4:3 screen as tall as the screen (`hand_tricons::IconHalfSize`, his deviation 11), so a wide screen
  does not make them bigger; on 4:3 and taller screens it is the original's focal length over 22. Debug: the stats
  overlay's "Camera Icons" line (`Game::GetHandTriconFades`).
  - ✅ **The keys' modifiers light their icons** (the active word above): `ModifierIconHints` in
    `Camera/KeyboardMoveSpeed.h` (zoom key → zoom and rotate; else `TurnModifierOn`, the turn key or a fight watched in
    its arena → pitch and rotate; none with the land gripped), worked out in `DefaultWorldCameraModel::HandleActions`
    from the keys and the grip the previous frame's controls left, as `WorldCameraFrame::keyHints`;
    `hand_tricons::WorldCamera` fills with them only the icons the mouse left. Before, a watched fight without the turn
    key offered nothing, and the grip of the frame itself held them back. Tests
    `KeyboardMoveSpeed.TheModifiersAloneOfferTheirIconsUnlessTheLandIsGripped`,
    `WorldCameraFight.AWatchedFightOffersTheTiltAndTurnIconsAsTheTurnKeyDoes`,
    `WorldCameraLandGrip.TheKeysIconsWaitForTheFrameAfterTheGripEndsAndGoTheFrameAfterItStarts`,
    `HandTricons.TheTiltKeysShowThePitchArrow`. Debug: the stats overlay's "Camera Icons" line.
  - ✅ **A map's clear starts them again**: `GGame::ClearMap` 0x552BB0 (from `LoadMap` 0x552F4F on
    every load with a path, `Close`, `LoadAllGame`) calls `ResetCameraModeNew3` 0x460B20 (0x552D67), which zeroes the
    word [0xC5B0F4] (0x460BEC), the fades [0xC5B0D8..E4] (0x460B35..) and the rotate angle [0xC5B0AC] (0x460C0F), and
    puts `NoCross` (the cross's hold-back) at 0 (decomp CameraModeNew3.cpp:3953..3977). openblack: `Game::PrepareNewLand`
    with `resets.mapClear` (every load but the campaign's first land, `land_load::ResetsFor`) calls
    `hand_tricons::Restart` on `_handTriconFades` / `_handTriconAngle` and puts `_handCrossHeldMs` at 0; the word has
    no kept counterpart (picked every frame). Tests
    `HandTricons.TheEndOfAScriptsCameraControlStartsTheIconsAgainFromNothing` (the rule),
    `LandLoadScripts.EveryLoadButTheCampaignsFirstLandClearsTheMap` (when). The rest of
    `ResetCameraModeNew3` (the camera's inputs, the clear view's zoomer [0xC5B100], the exclusion zones) is the player
    camera's, not checked here.
  - Tests: `WorldCameraMouse.*` (edge drag, still frames, turning taken away, pitch drag, the window's edge),
    `CameraDrag.*` and `WorldCameraHints.*`.
  - The land tooltips (0xE76 «Tilt», 0xE77 «Rotate», 0xE73, 0xE8B «Zoom out», 0xE7E «Move», in the table's order,
    from the camera's hints and the features): `hand_tooltips::LandToolTip`
    (`src/ECS/Systems/HandToolTipRules.h`) through `HandSystem::SubmitLandToolTips`; tests
    `HandToolTips.OverTheLandTheHandOffersToMoveOnTheLeftButtonWithAllArrows`, `AHintComesBeforeAFishAndAFishBeforeTheMove`,
    `ZoomOutNearTheLandWithTheCameraLookingDown`, `TheHintsAndTheFishComeBeforeZoomOut`
    (`test/hand/test_hand_tool_tips.cpp`); the rest is on [the tooltips](../interface/tooltips.md).
- **The wheel ✅ faithful:** raffclar's 60 a notch (`k_WheelZoomPerNotch`) from the action map's
  `GetMouseWheelDelta`, read only while ZOOM_OUT or ZOOM_IN is performed (out first), a notch its way without the
  wheel; it replaces openblack's 20 × 400 × the frame's seconds. Tests: `WorldCameraMouse.TheWheelZoomsSixtyANotch*`,
  `WorldCameraMouse.AZoomKeyIsANotchAFrameAndTheWheelWithoutOneIsLost`.
- **The movement keys ✅ faithful in the step and the feature (2026-10-22):** `TiltZoom` moves the camera only with
  feature 0x08 (`GrabLand`), by `KeyboardMoveOffset` (`src/Camera/KeyboardMoveSpeed.h`), the original's steps in its
  order, with the step `GetZoomScale` gives the zoom of the same frame (it was a fixed 60 before). Tests:
  `KeyboardMove.*` (`test_keyboard_move_speed`), `WorldCameraMouse.TheMovementKeysGoAsFarAsTheZoomsStep`,
  `WorldCameraMouse.TheMovementKeysDoNothingWithoutTheLandGrab`; the recorded `MoveBackwardForward` and
  `MoveRightLeft` keep the step 60 (all their frames are within 4 of the focus's height). Debug: the camera window's
  "The scripts allow" line shows the feature. `TiltZoom` runs in the Polar mode (the original's mode 2) and the
  ArcBall (the middle button, which the original also drives through mode 2). Each frame of a move with the keys
  sends Drag 0x308 to the camera help, with the feature, before the move, as the original at 0x45C94E..0x45C986
  (with the frame's input mask, and only with the frame's own controls). Test:
  `WorldCameraMouse.EachFrameOfAMoveWithTheKeysCountsAsADrag`.
- **Both buttons and the middle button ✅ faithful:** both buttons (the action map's TWO_BUTTON_CLICK, which drops
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
  `TheWheelIsOffDuringTheClearView`, `WithoutTheHandOverTheLandTheClearViewLeavesTheCamera`. **⚠️ approximate** The
  point is the action map's hand position (the double click's), not CHand's 3D origin; without one over the land the
  camera is not moved (🔍 inferred the original's CHand +0x482C is always there).
- **Auto pitch ✅ faithful:** `camera_help::AutoPitchInput` (a fifth of the way) in HandleActions while nothing is
  gripped, from the original's `HeadingAndPitchFromPoints` (`script_camera::`) of the camera and its focus at the
  click; with nothing to tilt the player's tilt is dropped. It tilts without the Pitch feature and is left out of the
  camera help's Pitch. The height (`GetAutoPitchDistance` over the land at the frame's start) is applied in Update after
  the constraints in the frames it tilts or nothing is gripped or done (`_idleMouse`); the focus keeps its offset and the
  focus at the click follows. Tests: `CameraHelp.TheSelfTiltingCameraTiltsAFifthOfTheWayAtMostTheFramesSeconds`,
  `WorldCameraMouse.TheSelfTilting*`, `WorldCameraMouse.GrippingTheLandStopsTheSelfTiltingCamera`.
  **⚠️ approximate** The tilt is worked out in the controls (raffclar's place), so it is not while a hand demo plays,
  and the height there follows the last controls' frame; the altitude is `GetHeightAt`, not the map coords' fixed point.
- **Flights to a place (`DefaultWorldCameraModel`, `CameraModel::CharterFlight`):** `SetFlight` is `FlyToPosFoc`
  without the zone test, `CharterFlight` is `SetupVia` and `ComputeUpdateReturnInfo` the legs. Faithful: the via (the
  two origins added and halved, the distance across the land times t, at least the land + 10 under it, the land read
  at `map_coords::ToMetres(MetresToFixedForHandLookup(m))` as the hand's turn reads it), the legs' times (1.35 s, then
  1.5 s once past 0.75 s) and the woosh beyond 150 (`k_FlyingDistanceThresholds`). t is 0.3 for `SetFlight` (the
  bookmark keys; the debug editor and the test hooks take it too, as the original's camera editor does) and 0.1 for the
  double click's 100 flight (`k_DoubleClickFlightRise`). **🔍 inferred** `GetHeightAt` stands for `GetAltitude`, as for
  the hand. Tests (`test_camera`, `test/camera/test_camera_flight.cpp`): `CameraFlight.*` (the original's recorded
  double click via, the floor over a hill, t = 0, the raise across the land only, the land's lookup) and the recorded
  `DoubleClickFlyTo`. The watched fight's flight goes through the same `FlyTo` with t = 0. Any camera input drops the
  flight as the original does: in `HandleActions`, the same input test as the Drag help's (a zoom, move, turn or tilt
  input, or the middle button; a keyboard move alone not while gripping), the grip or both buttons reset `_flightPath`,
  leaving `_flightSeconds` alone. The update then gives the camera the frame's own target, which starts from the
  zoomers' destinations as the original's does. The controls run before the update, so the double click's flight, set
  in the update, is kept in its first frame. Tests (`test/camera/test_world_camera_mouse.cpp`):
  `WorldCameraMouse.*Flight*`.
- **A flight's start sets the camera's easing time ✅ faithful**: the original keeps one ElapsedTime
  +0x78 for the drag away's clock and the easing (`Update`'s flyTime ramp, decomp CameraModeNew3.cpp:3803..3816);
  openblack keeps it twice, `_flightSeconds` and `_elapsedTime`, and now sets both where the original sets +0x78:
  `FlyTo` (the bookmarks' and scripts' `SetFlight`, the watched fight's) to 0, as `FlyToPosFoc` 0x4587F0 does at
  0x4588DA; the double click's flight to 0 with a woosh, FlyTime / 2 = 0.75 s without one, 0.01 after a drag given up
  (decomp :3339..3355; `camera_flight::EasingTimeAtStart`). The frame's time is added after the update's own setting
  and before the easing reads it (`ElapsedTime += dt` at decomp :3803 comes after the double click's), a move that
  changes nothing else (nothing reads `_elapsedTime` between). Where the flight's last leg comes, it is zeroed again
  as before; the easing differs only where a flight is dropped before it, and for a double click's short flight, which
  has no legs. Tests `CameraFlight.AFlightStartsTheCamerasEasingAtItsOwnStartTime`,
  `WorldCameraMouse.ABookmarkFlightStartsTheCamerasEasingAgain`, `AShortDoubleClickFlightEasesFromHalfwayDownTheRamp`.
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
    - `std::sin` / `std::cos` in double stand for the x87's 64-bit `fsin` / `fcos` [V]: 20 000 oracle cases on 0x442810
    give 0 differences, and for the fence search's 63 angles × 2 radii `static_cast<float>(std::sin(double(a)) * r)`
    (and cos) gives the x87's values for all 252 (the oracle and an exact series agree).
- **The view a flight goes to (`src/Camera/CameraFlight.{h,cpp}`, `camera_flight`), faithful:** `ShapeDistance` (the
  50 / 0.8 and 100 / 0.1 rules), `FindBestAngle` (the 32 headings, the five samples at pitch 0 through
  `script_camera::PointFromDistanceHeadingAndPitch`, the cosine bonus with 50 a double, the first strict maximum, the
  unused sample at 0.3 read and dropped, the pitch with the original's float and double constants and its two
  comparisons in order, so not a number goes to π/8), `ViewOfPoint` (`CameraModeNew3::SuggestBestCameraPos` 0x458E50, the heading and pitch from
  `script_camera::HeadingAndPitchFromPoints`) and `ArenaLookPoint` (Q). The land is read at
  `map_coords::ToMetres(MetresToFixedForHandLookup(m))` for x and z, the normal with `GetNormalAt`. **🔍 inferred**
  `GetHeightAt` and `GetNormalAt` stand for `GetAltitude` and `GetNormal`. The watched fight uses them, and so does the
  double click's flight (`DefaultWorldCameraModel::UpdateModeFlying`): for the 100, 60, 30 and 15 flights, not the one
  after a drag given up, `FindBestAngle` takes the camera's heading and pitch, the flight's distance unshaped and the
  land under the mouse, as the original's call does; then the pitch is kept within π/8 and 1.496 as before. Tests:
  `CameraFlightView.*` (the distance at 10, 30, 50, 75, 100, 200 and not a number; level land: the heading kept, the
  pitch 0.4382 and the 192 land reads; a wall ahead: the open side; the pitch's bounds and not a number; the arena's
  view), `WorldCameraMouse.OnLevelLandTheDoubleClicksFlightKeepsTheCamerasHeading` and
  `WorldCameraMouse.OverAHillTheDoubleClicksFlightLooksFromTheOpenSide`, and the recorded `DoubleClickFlyTo`.
- **The land under a pixel:** `PickingSystemInterface::LandFrom2DPoint(pixel, shift, depth)` (the eye `Camera::GetEye`,
  the near point `Camera::PixelNearPoint` on the near plane of the last camera update,
  `hand_land_point::ShiftedAlongView`, `land_line::LandOrSeaAlong`, the height at the quantised point).
  `Camera::RaycastMouseToLand` and `Camera::RaycastScreenCoordToLand` take integer pixels and call it with the shift on
  at depth 0; the default camera model's mouse and middle points (`UpdateRaycastHitPoints`), its 16 pixels
  (`GetVerticalLineInverseDistanceWeighingRayCast`), the land at the press (`UpdateFocusPointInteractionParameters`)
  and the double click's point (`UpdateModeFlying`) go through them, with the camera as it is drawn (the zoomers'
  current values). There is no plane at y = 0 beyond the 7500 of the sea, and features' bodies do not stop the line.
  Tests: `LandFrom2DPoint.*`, the camera scenarios (their fake answers a pixel from the recordings; the 16 rows,
  rounded down here, are recorded rounded to the nearest).
- **The double click on an object:** `camera_flight::DoubleClickPoint` (the object's `MapCoordsOf`, its metres, else
  the land under the mouse; either at the land's height read again at the hand's quantised point, since 2026-10-25
  for the land too) in `UpdateModeFlying`, for the double click and the given-up grip; then, as 0x45DED9..0x45DFBF,
  the fence (`camera_zones::CrossFence` along (1, 0, 0): outside, CameraHelp 0x101 and the point becomes the closest),
  the exclusions (`camera_zones::InsideExclusion`, the margin 3 [0x9CE618] on the radius and on a dome's height;
  inside, CameraHelp 0x100 only; a kind other than 0 and 1 has nothing inside, `Exclusion::Kind::Other`) and the map
  disc (`camera_zones::InsideMapDisc`, strictly; beyond it the flight goes nowhere) (tests
  `CameraZones.APointIsInsideACylinderOrADomeWithTheMarginAndNeverInsideAnotherKind`,
  `MapDisc.APointIsInsideOnlyStrictlyWithinItsRadiusAcrossTheGround`,
  `CameraFlightDoubleClick.WithNoObjectItIsTheLandUnderTheMouseAtTheLandsHeightReadAgain`); the object is the hand's `HandSystemInterface::GetCursorObject`, read by the camera's
  controls (`DefaultWorldCameraModel::HandleActions`), which run before the hand in each frame. The hand keeps it as
  the interface does: `_collideObject` (+0x400), set in the hand's `Update`, is the frame's object under the cursor
  (`ResolveCursorPoint`, earlier in the same frame) while no action is in progress (`hand_action_point::CollideObject`,
  `HandSystem::ActionInProgress`: the land grip, the grab, the seed's apply on release, the bubble grip), kept through
  one, the grabbed object from the grab's start when it is interactable (fn_005D5E40 at 0x5D17C3: vt 0x190
  `IsInteractable`, else none), cleared with `_renderCollideObject` when it can no longer be interacted with
  (`Interactable`, every hand frame, with or without an action, as the original's check) and by `EndAction`
  (fn_005D1260, 0x5D1288 / 0x5D1296); `_renderCollideObject` (+0x3C8) is copied from it as the hand's `Update` starts.
  So the camera of frame N reads the copy made in frame N − 1, of the object frame N − 2's hand found, as the
  original's; a destroyed object reads as none (`hand_action_point::ExistingObject`). With an object the flight needs
  no land under the mouse and reports 0x306 (help event 31). openblack's pick skips the hands and the held, tugged and
  render-held objects (`HandSystem::PickObjectAlongRay`), kept until the held object's case is read. Tests:
  `CameraFlightDoubleClick.*`, `WorldCameraMouseOverObject.*`, `HandActionPoint.TheObjectIs*`,
  `HandActionPoint.AnActionKeepsTheObject*`, `HandActionPoint.ADestroyedObjectIsNoLongerTheInterfaces`.
- **Watching a fight, the camera's side** (`DefaultWorldCameraModel` as `camera::FightWatch`,
  `src/Camera/FightOrbit.{h,cpp}`; the original is on
  [the creature's fights](../creature/fights.md#player-camera-watching-a-fight)): `GetFightWatch` returns the world
  camera itself (the other models have none). The fight's side (the creature fight rules) makes the watch decision and
  calls `StartFight(a, b, centre, radius)` for a watched fight, `EndFight` at its end; the camera starts one itself
  after lingering 1000 ms over an arena (`fight_orbit::ArenaUnderView` in `Update`) and on a double click onto one
  (`fight_orbit::ArenaAtPoint` in `UpdateModeFlying`), over `CreatureFightSystemInterface::GetArenas`; `WantToQuitFight(centre, radius)`
  is the in-view test it asks first (the camera's current origin, without the shake, and the screen-centre hit; false
  without one). Faithful:
  - the state (`_fight`) and StartFight's values (5.5, π/2, 0.408407, the first frame, the zoomers reset, the flight
    clock 0); the easing's time `_elapsedTime` 0 too (ElapsedTime +0x78 = 0 at 0x45A64D, decomp
    CameraModeNew3.cpp:2115, before `FlyToPosFoc` zeroes it again at 0x4588DA; openblack keeps +0x78 twice, as the
    flight clock and as the easing's time, so a flight dropped before its last leg eases from a new camera's 1.5 s as
    the original's does; `WorldCameraFight.AWatchStartingStartsTheCamerasEasingAgain`); EndFightNow; EndFight acting
    only while the fight is on, so a second call keeps the time left; the linger counted down by the frame's whole ms
    at the start of Update;
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
    [esp+0xCF]: the keyboard's move stands, as `TiltZoom` and the controls' mode give it, and the watch goes on; only
    reached when the keys were read before the watch began, since the watch keeps the turn modifier on and the keys
    then tilt and turn, [creature fights](../creature/fights.md)): the
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
  - **⚠️ approximate** The fighters' radii come from the radius function the model is made with, by default
    `ecs::object::GetRadius`, whose creature branch (LH3DCreature +0x5228) is not ported: the generic footprint radius
    stands in, 0 without a loaded mesh. A radius of 0 is divided by as the original does (IEEE floats, see Pending).
    The positions are the fighters' `Transform`s. The arena is "gone" when either fighter is no longer a valid entity
    with a Transform. The fighters are the arena's in the order the fight passes them (the creature, then its
    opponent).
  - Tests (`test_camera`, `test/camera/test_world_camera_fight.cpp`): `FightOrbit.*` (the radii test, the wrap, the
    turn the short way, the distance, zoom and clamps, the 0x10 mask) and `WorldCameraFight.*` (the orbit and the
    tilt's bound, no orbit while the land alone is gripped, zoom past 40, EndFightNow, EndFight once, a fighter gone,
    the drag away after 4.5 s and not without a grip, WantToQuitFight, no double click flight, no Pitch tricon, and
    `TheWatchFliesToTheArenaFirst`: the via leg in 1.35 s, nothing for 22 frames, the last leg to `ViewOfPoint` in
    1.5 s, then the orbit's place in 5 s),
    `FightOrbit.TheOrbitEasesInOverFiveSecondsAfterAModeChangeAndTwoOnceTheModeIsSettled` and
    `WorldCameraFight.TheOrbitEasesInOverTwoSecondsOnceTheModeIsSettledAndFiveAfterAModeChange` (2 s past 2 s of the
    mode, 3.5 s at 1 s, 5 s after the script camera taken and given back, the model's own time once the watch is over),
    `WorldCameraFight.AMovementKeyDuringAWatchTiltsRoundTheFightAndTheOrbitGoesOn` and
    `WorldCameraFight.WithoutTheLandGrabAMovementKeyStillTiltsRoundTheFight` (the movement keys tilt during a watch;
    before 2026-10-24 these two tests held a forwards key as a move over the land during the watch, which the original
    never reaches).

### The bookmarks' markers in openblack

- The pulse is `bookmark_pulse` (`src/Camera/BookmarkPulse.h`): `Step` and `Wave` are the original's
  `DrawLoopUpdate`, and `MarkerHalfSize` the draw's half width and half height. `CameraBookmarkSystem` keeps the one
  clock (not reset by a new land) and its `Update(frameGameMs)` runs in the object draw list's step, after the list,
  with `game_clock::FrameGameMs()`: only when the land is drawn, still in pause. Every bookmark on the land takes the
  half size as its Transform's scale, which the sprite draw uses. Tests: `BookmarkPulse.*`
  (`test/camera/test_bookmark_pulse.cpp`, pinned to the oracle). The debug Camera window shows the pulse, closed at
  first ("The bookmarks' pulse").
- A bookmark keeps where it was dropped as a map position (`CameraBookmark::position`, `map_coords::FromWorld` of the
  point), and each update puts its marker 2 above that position's land point as the land is that frame
  (`bookmark_markers::MarkerPosition`, `src/Camera/BookmarkMarkers.h`: `map_coords::ToWorld` plus 2, as DrawAll's
  `ConvertMapCoordToLandscapePoint` and its 2.0f). Tests: `BookmarkMarkers.*` (`test/camera/test_bookmark_markers.cpp`).
- The marker is `CameraBookmarkArchetype::MarkerSprite`: its cell (24..31) of misc0.raw with its alpha misc0a.raw,
  blended (`additive` false: `render_modes::SpriteMode(false)`, the misc material [0xEA1AB0]'s mode 6), tinted
  yellow, as the original's (`BookmarkMarkers.AMarkerIsItsCellOfTheMiscSheetBlended`).
- The markers are drawn only while the bookmarks are on (`SetEnabled`, the scripts' PLAY_JC_SPECIAL 14 / 15) and no
  script holds the wide screen (`help::HelpSystem::IsScriptWideScreen`: the wide screen and its owner both set, as
  DrawAll's `WideScreen` and `WideScreenControl`): `bookmark_markers::MarkersShown`. A marker has a Transform only while
  it is drawn; the pulse moves on either way, as `DrawLoopUpdate` runs before DrawAll's test
  (`BookmarkMarkers.TheMarkersShowWhileOnAndWithoutAScriptsWideScreen`, `AHiddenMarkerIsNotDrawnAndComesBackWhereItWas`).
- Not yet the original's: see [Pending](#pending).
- **Dropping one, and on a thing.** The key's modifiers are `input::ActionOfBookmarkKey` (`src/Input/BookmarkKeys.h`),
  as `GGame::ProcessKey` 0x63EF20 reads them (0x10 Shift, 0x20 Ctrl, 0x40 Alt): Shift with Ctrl clears, Ctrl without
  Alt drops, Shift follows, the key alone flies. Ctrl drops it at the place the
  interface status was synced to at the last turn (`HandSystemInterface::GetSyncedPlace`, the status's Pos the key
  passes), or on the interface's object when it has one (`GetActionObject`, the kept collide object, as
  `SetBookMark(MapCoords)` 0x439E50 hands +0x400 to `SetBookMark(Object)` 0x439F70): its map position is then the
  thing's. Once a turn, where the original's `Bookmark::ProcessAll` 0x439DD0 runs (`GGame::ProcessTurn` 0x54E6D0,
  before the script highlights), `CameraBookmarkSystem::ProcessTurn` takes the thing's map position again, and forgets
  the thing once it is no longer available (`Bookmark::ValidateTarget`'s GAME_THING_FLAG_UNAVAILABLE test), or once it
  is gone from the registry or has no place: the bookmark then stays set where the thing was at its last turn, its
  marker there and the flight going there. While it has its thing the marker stands 2 above where the thing is drawn
  this frame (`ecs::DrawnPosition`, as DrawAll's `Game3dObject` matrix position). Tests:
  `BookmarkMarkers.ABookmarkOnAThingFollowsItAndStaysWhenItGoes`, `BookmarkKeys.TheModifiersChooseWhatTheKeyDoes`.
- **The flight.** `SetBookMark` keeps the camera's place as an offset from the land point of the bookmark's map position
  (`CameraOffset` = `g_camera` − that point, 0x439E50 / 0x439F70; openblack's `CameraBookmark::cameraOffset` from the
  camera's origin at the key). The key alone (no modifier, not inside the citadel) flies there as
  `SetCameraToBookMark` 0x43A070 does: the focus the land point of the map position as it is at the flight (a bookmark
  on a thing has followed it), the origin that point plus the offset, `FlyToPosFoc(pos, foc, 0.3)`
  (`CameraBookmarkSystemInterface::FlightTo`, then `SetFlight`, whose rise is that 0.3). Test:
  `BookmarkMarkers.AFlightLooksAtTheLandPointFromTheDroppedOffset`.
- **The keys 1 to 7, cleared and followed.** Only KB_1..KB_7 (LH_KEY 2..8) reach the bookmark case: the key pass's
  byte table at 0x63F6FC gives 0 to them, 3 (nothing) to KB_8..KB_EQUAL, 1 to KB_BACKSPACE and 2 to KB_TAB (oracle
  peek; jump table 0x63F6EC), so the key 8 is no bookmark key although DrawAll draws eight (`input::IsBookmarkKey`).
  Shift and Ctrl clear a set bookmark (`Bookmark::Clear` 0x439D40: its thing forgotten and its flag cleared; the follow
  is left). Shift with a set bookmark's key is `SetFollow`: on no thing, a jump to it (`SetCameraToBookMark`) and
  nothing followed; on a thing, it is followed, or no longer when it already was. Dropping a bookmark follows nothing
  and makes it the current one; a jump (`SetCameraToBookMark`, the key alone) makes it the current one and stops a
  follow of another. Each turn `ProcessAll`, after following the things, flies to the followed bookmark while the
  camera's current mode is `CameraModeNew3` (openblack: `DefaultWorldCameraModel`), else follows nothing; and a turn
  whose camera input counted for the camera help ([0xC5E168], `DefaultWorldCameraModel::InputCountsForHelp`) stops the
  follow after that turn's flight (`CameraBookmarkSystemInterface::ToggleFollow`, `JumpTo`, `ProcessTurn`, `ClearBookmark`).
  The current bookmark is only read by Ctrl / Shift with Tab, not ported. Tests:
  `BookmarkMarkers.ShiftFollowsABookmarkOnAThingEachTurn`, `ShiftJumpsToABookmarkOnNoThingAndShiftCtrlClearsOne`,
  `BookmarkKeys.TheKeysOneToSevenAreBookmarkKeys`.
- **The view before a jump.** `SetCameraToBookMark` 0x43A070, after its flight, sets bookmark 10
  (BOOKMARK_INDEX_BEFORE_JUMP): no thing, Pos = the interface status's Pos, CameraOffset = `g_camera` − its land point.
  Backspace (KB_BACKSPACE, LH_KEY 0x0E: the key pass's table gives it case 1, 0x63F690) calls
  `SetFocusBeforeJumpBookmark` 0x43A380, a jump to bookmark 10, under the keys' gates (no script's wide screen, the key
  shortcuts on) but not `ShowBookmarks`; that jump sets bookmark 10 again from the view it leaves, so Backspace goes
  back and forth. Bookmark 10 is not drawn (DrawAll draws 0..7) and `ClearMap` clears it. openblack:
  `ecs::systems::k_BeforeJumpBookmark` in `CameraBookmarkSystem` (`JumpTo` with `BookmarkJumpFrom`: the synced place,
  the camera's origin, the citadel), forgotten by `Initialize` at a new land; `input::BeforeJumpKeyActs`. Tests:
  `BookmarkMarkers.AJumpKeepsTheViewItLeavesForBackspace`, `BookmarkKeys.BackspaceActsWithTheBookmarksOff`.
- **Saved games.** `Bookmark::SaveAll` / `LoadAll` (each bookmark's flags, offset, thing, Pos; then FollowBookmark,
  CurrentBookmark and ShowBookmarks) wait for the saved games' work (S1).
- The testbed scene `hand.camera_bookmarks` (debug only) shows them: its commands "set a camera bookmark" (a bookmark
  dropped at a point of the land or on a creature, as Ctrl with a number key), "show camera bookmarks" (PLAY_JC_SPECIAL
  14 / 15's switch), "tap a camera bookmark key" (a number key or Backspace tapped with Shift and Ctrl as given, through the game's
  key handling) and "log
  camera bookmarks" (the pulse's time and bits, the camera, and each marker drawn, its place, half size and blend)
  drop two in front of a still camera and one on a creature that walks off (followed with Shift and the key, then
  let go) and goes, log them, turn them off and on, hide them behind a script's cinema bars, fly back to the first from
  another view, clear the second (Shift and Ctrl) and go back with Backspace
  (`TestbedScenarios.DropsTwoCameraBookmarksAndLogsTheirMarkers`).

### Test hooks

- The recorded camera scenarios `DragUpDown`, `TwoButtonZoomOutIn`, `MiddleDragRightUp` and `DoubleClickFlyTo` (with
  their fake land under a pixel), and the unit tests named above in `test_camera`, `test_cursor_freeze`,
  `test_camera_zones` and `test_script_camera_dual` (the zone loader and the fence).
- The cross-page debug variables are on [debugging](../project/debugging.md#debug-environment-variables).

## Pending

- **What the player camera does with the zones** ([the steps](#what-the-player-camera-does-with-the-zones)): the height
  limit, the exclusion push, the land probe, the lowest height, the slide and the largest move are not ported; the
  disc (its test, its pull across the ground, its help report and the round trip through the map's middle, step 10)
  is, and the fence is (above), but for the force field's pulse, which waits for the force field to be drawn. Port
  them in the original's order, with the ground of step 1.
- **The fixed rotation**: stored (`player_camera::Get().fixedRotation`) and not used: `DefaultWorldCameraModel` should
  turn about it (0x45AB00, 0x460135).
- **The tricons**: the wheel gate +0x2EC is not ported; while openblack fades no icons (a box up, the temple, the interface off) it keeps the last shown word for WITHIN_ROTATION, where the original's `UpdateTricons` may still write it, unmasked, in the camera's update; of the active word
  +0x94 the keys' modifiers are (above) and the turn, tilt and zoom are read as this frame's inputs; whether the
  self-tilting camera's tilt counts as the tilt input is **undetermined**; the hand's poses take the zones from `GetHandCues`. The original shows no tricons under a script camera (the
  CameraModeNew3 casts), so the port should gate on the player's camera mode, not on `Drives()`. The help text
  0x1407's words, the tooltip 4's alignment and g_game +0x250538's name are unknown (`python xref.py str 0x1407`).
- **CameraHelp**: its features are not saved and loaded with the game (`GGame::Save`).
- **The player's camera made again when a script's camera ends** (found with fork issue #216, 2026-10-23; the camera
  help's side, the controls of an earlier frame, the wheel in a hold and the restart are done, above):
  - 🔍 the first self-tilt of the new mode reads +0x12C (`Heading0x12c`), which neither `Initialise` 0x456640 nor
    `Reinitialise` sets: undetermined. openblack tilts itself again from the next frame whose controls are read;
- **The double click on an object** ⚠️ approximate: openblack's camera does not tell the double click on a position
  from the one on an object in every path; the camera help's reports are now 0x305 / 0x306 as the original's.
- **The clear view's hand**: the camera's clear view calls `Morphable::SetPos(CHand, target)` with the object under the
  hand each frame (0x45B568): not ported. The camera hand state's grip pose above 0.01 is ported
  (`CameraModel::HandCues::clearViewGrip` while a drag is held, and the grip of a camera state without a drag;
  [the navigation poses](../hand/animation-and-placement.md#the-navigation-poses)).
- **The town's storage pit flight** (Tab, `Bookmark::SetFocusToOwnedTown` 0x43AA60): not ported (a port row).
- **The bookmarks' markers** ([the original](#player-camera-the-bookmarks-markers)): the bookmarks are not saved yet
  (the saved games' work, S1). The creature's leash target marker, its ring and turned footprint at the same pulse, is
  drawn ([the leash](../creature/leash.md#tying-the-leash-in-openblack)).
- **The radius of 0 in the fight orbit** and the creature's radius branch: on
  [the creature's fights](../creature/fights.md#player-camera-watching-a-fight).

### The controls, not ported

- The readers of CameraHelp's feature bits 0x80 and 0x100 (bw1-decomp's `CAMERA_FEATURE_HELP` and
  `CAMERA_FEATURE_EDGE_SCROLL`): [0x9CDD6C]'s initial value, ClearMap's (0x552E74) and the NORMAL interface level's
  0x1BF set them, as openblack's `camera_help::k_NormalFeatures` does. 0x80 is read only by the hand's tricon
  (`CameraModeNew3::DrawHandTricon`, `test byte [0x9CDD6C], 0x80` at 0x456A75: the tricon while the inputs are
  dropped, below), 0x100 only by `CameraModeNew3::IsEdgeScrollEnabled` 0x45A060 with the profile's edge scroll
  option [0xC5E154] (the edge scroll, [its row](../project/progress/camera/world_camera_controls.md)); both readers
  wait for those ports (xref refs 0x9CDD6C, 2026-10-28).
- The land grip, not ported:
  - the move report 0x200 with [0xC5E168];
  - the tricon display while the inputs are dropped (0, or bit 8 with feature 0x80): the hand cue carries +0x90 only;
  - mode 3, a grip with another camera input: openblack's drag modes take the drag's mode alone (an edge rotate or a
    pitch drag adds the player's other inputs to its own, a pan drops them).
- The double click on an object, not ported:
  - the follow camera on a creature (see [The player's follow mode](script-camera.md#the-players-follow-mode)): openblack flies to
    the creature and stays;
  - the hand's own readers of +0x3C8 (its placement, its clip, the creature under it) still take the frame's object
    under the cursor (`_cursorObject`), not the kept one the camera reads (left for a change of the hand of its own);
  - the pick itself: openblack's hand picks along its own ray in the frame it is placed, where the original picks the
    drawn objects in the draw; equal only once the interface's pick comes from the draw list
    (`PickingSystemInterface::PickUnderCursor`);
  - whether the held object can be the interface's object: not determined;
  - the end of the action when its kept object can no longer be interacted with: openblack clears the objects only;
  - the +0x24 & 4 test of the interface's object (UNAVAILABLE_FOR_STATE_CHANGE in bw1-decomp's names), not ported.
- The mouse drags, not ported:
  - CHand +0x486C / +0x4874, the hand's copy of the ring cursor: the action map's `GetCursorWarp` has it, no hand code
    reads it yet;
  - the pitch drag's lift with [0x9CE6B4] == 0 (a player profile option, not named): openblack keeps the eye, as with
    its initial value 1;
  - 🔍 inferred openblack's Borderless display mode counts as full screen for the 0.45 edge (only Windowed is a
    window), and a display mode changed after the start is not seen;
  - the tick count is the controls' frame time, not GetTickCount.
- 🔍 inferred The cursor freeze is not switched on while the script camera drives: the original freezes only from the
  player camera's own update (0x45AE96); whether a script camera ever freezes it has not been read.
- Both buttons: the hand state 22 route is the raw buttons in openblack (the same buttons); the freeze in a hand
  demo's playback (0x54A710) is not ported (openblack's demos press no camera buttons).
- The clear view, not ported: the object under the hand as the target and the hand set onto it with
  `Morphable::SetPos` (0x45B568; what that does to the hand is inferred: pinned to the object). openblack's camera has
  no object under the hand, as for 0x306; the tricon display (bit 4 only above 0.5) and the icons' placing; the saved cursor [0xC5B060], the frame
  [0xC5B020..0xC5B048] (no reader outside Update found) and +0x2D4.
- The wheel: the +0x2EC gate (not named); with the gate closed the original keeps [0xC5E8D8] for a later frame, where
  openblack's wheel delta is the frame's only.
- Points from angles, not ported or not read: `FeatureScriptCommands::StartCameraPos` (the feature scripts'
  START_CAMERA_POS) still builds its origin with glm's `euclidean` from 120 at 12.8571° / 157.51°, and the original's
  START_CAMERA_POS has not been read; `zoom_to::OriginAround` (`Camera/ZoomToPlaces`, the temple and realm keys) has
  maths of its own, where the original's `ZoomToCitadel` calls 0x442810 (0x457D69, 0x457EB7); the mouse mode 1 point
  (0x45EF0E) and the fixed rotation's (0x46019F) have no openblack counterpart; openblack's ArcBall (the middle
  button) is its own model, where the original's middle button turns and tilts through mouse mode 2 (0x45E9D1), so
  only its rounding follows the original's.
- Auto pitch: the player's tilt alone, dropped, leaves openblack's camera in its Polar mode where the original is in
  mode 2; the tilt is worked out in the controls, so not during a hand demo.
