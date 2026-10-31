# Matrices and the Zoomer

## Summary

The original places every object, bone and camera with LH3D's 4×3 matrices: three rows and a translation, a point
being a row vector. Each kind of object builds its matrix with one of a few constructors (a yaw only, or yaw, pitch
and roll), and the camera's matrices are built the same way each frame. The **Zoomer** is LH3D's interpolator: a
quartic curve that carries a value from where it is, at its current speed, to a destination in a given time, used by
the camera, the hand, the totem and many small animations.

openblack reproduces both bit for bit: the matrix constructors and products in `openblack::affine`
(`src/3D/ObjectMatrix.{h,cpp}`, `src/3D/AffineMatrix.{h,cpp}`), with the original's operation order and float
rounding, and the Zoomer in `src/Common/Zoomer.{h,cpp}`, checked against recordings of the original.

## The original

### LH matrices

#### Convention and precision

An `LHMatrix` is 3 rows + translation (row vector, p' = p·M): row k is the image of local axis k. In glm it is
**column** k, with the same memory (`glm::mat4x3` is the 12 floats of an LHMatrix). LH3D rotations go the opposite
way to glm's: the original's angle a is the −a of `glm::rotate`.

The FPU runs at 24 bits ([The game's FPU runs at 24 bits](floating-point.md#the-games-fpu-runs-at-24-bits)), but
fsin/fcos are not rounded by the precision control. What the original leaves on the stack is taken in double and
rounded once by the product that uses it; what it stores (`fstp dword`) is a float. It is the same rule as for the
angles ([GUtils angles](coordinates-and-angles.md#gutils-angles)). **⚠️ approximate** The double is not the 80-bit
register: in rare cases the last bit changes.

#### The constructors

| Original | What it builds | In glm |
|---|---|---|
| `LHMatrix::SetYXZMatrixOnly` 0x7FAC10 | yaw, pitch, roll | `eulerAngleYXZ(−y, −x, −z)` = `Ry(−y)·Rx(−x)·Rz(−z)` |
| `AtomCore::SetAngleY` 0x674360; the rotation of `LH3DObject::SetPosition` 0x423140 and `Object::GetWorldMatrix` 0x638200 | yaw only | `eulerAngleY(−a)` = `Ry(−a)`; equal to the YXZ matrix of (a, 0, 0) bit for bit |
| `AtomCore::SetAngleXYZ` 0x674200 | x, y, z angles | `Rz(−z)·Ry(−y)·Rx(−x)` (it is not `eulerAngleXYZ`) |
| `LHMatrix::RotateY` 0x5198F0, in place | turn about the object's Y | `m·Ry(−a)` (**on the right**: the object's axes) |
| fn_0086AFA0, in place | turn about the object's Z | `m·Rz(−a)` (on the right) |
| `UpdateRuleRotatePrincipalAxis` 0x6A1150, `AppearanceRuleTumble` 0x6A6200 | turn about a world axis | `R_axis(−a)·m` (**on the left**: the world) |
| fn_007FB180 (Rodrigues by rows) | axis and angle | `rotate(−a, axis)` |
| `LHMatrix::SetInverse` 0x7FB290 | inverse | the inverse, with the determinant floor |
| `LH3DObject::SetPosition` 0x423140 (vt+0x20) | placement | `T(p)·Ry(−a)·S(s)` |
| what all the `Set*` write | model | `T(p)·R·S`, the position as is |

Cell by cell:

- **SetYXZMatrixOnly 0x7FAC10** (a = Y, b = X, c = Z): `m0 = (ca·cc) − ((sc·sb)·sa)`, `m1 = −(sc·cb)`,
  `m2 = ((sc·sb)·ca) + (sa·cc)`, `m3 = ((sa·cc)·sb) + (sc·ca)`, `m4 = cc·cb`, `m5 = (sc·sa) − ((ca·cc)·sb)`,
  `m6 = −(cb·sa)`, `m7 = sb`, `m8 = cb·ca`. `cb` (0x7FAC23), `sc` (0x7FAC39), `ca·cc` (0x7FAC41) and `sa·cc`
  (0x7FAC4F) are stored in float; `ca`, `sa`, `sb` and `cc` stay on the stack (0x7FAC15, 0x7FAC1B, 0x7FAC2B,
  0x7FAC31); `sc·sb` stays on the stack rounded to 24 bits (0x7FAC45..0x7FAC49). The cells are written
  0x7FAC53..0x7FACB2 in the order above. It does not touch the translation. CAnim calls it with the stored float3
  (v0, v1, v2) as the YXZ matrix of (v1, v0, v2) (0x85F28E..0x85F29D).
- **GetYXZ 0x7FAB30**, the angles back from a matrix: y = ArcTanOctant(m8, −m6) (0x7FAB42..0x7FAB54), stored as a
  float (0x7FAB5F); c = float cos(−y), s = sin(−y) kept; m0' = c·m0 − s·m2, m2' = m2·c + s·m0, m8' = s·m6 + c·m8
  (0x7FAB70..0x7FABAA); x = ArcTanOctant(m8', m7) (0x7FABB0..0x7FABBB), stored (0x7FABC6); then with c2, s2 of −x,
  t = s2·m2' + c2·m1 and z = ArcTanOctant(m0', −t) (0x7FABD7..0x7FAC03).
- **SetAngleY 0x674360**: rows (c, 0, s) / (0, 1, 0) / (−s, 0, c), with `c` and `s` stored in float (0x674384,
  0x674390).
- **SetAngleXYZ 0x674200**: rows (1, 0, 0) / (0, cx, −sx) / (0, sx, cx) (cx, sx in float, 0x674224..0x674235). Then,
  in each row, `(e0, e2) → (cy·e0 − sy·e2, cy·e2 + sy·e0)` (0x674244..0x6742A2) and
  `(e0, e1) → (cz·e0 + sz·e1, cz·e1 − sz·e0)` (0x6742D2..0x674330), cy, sy, cz and sz on the FPU stack.
  `AtomCore::RandomiseOrientation` 0x6743E0 draws three `PSysFloatRand(2π)`: the **first is z**, the second y, the
  third x (each `fstp [esp]` lands in the slot of the argument it has just pushed).
- **RotateY 0x5198F0**: `r0' = c·r0 + s·r2`, `r2' = c·r2 − s·r0` (0x5198FC..0x519956); r1 and the translation
  unchanged. **fn_0086AFA0**: `r0' = c·r0 − s·r1`, `r1' = c·r1 + s·r0` (0x86AFAC..0x86B008). Inline copies keep c and
  s at other precisions: `HelpDude::Update1`'s HUD pose stores c as a float and keeps s on the FPU stack
  (0x5BE467..0x5BE4DE, 0x5BE604..0x5BE69D); its pitch (0x5BE3DF..0x5BE456) is the same pattern on rows 1 and 2,
  `r1' = c·r1 − s·r2`, `r2' = c·r2 + s·r1`, a turn the original has only inline.
- **The world-axis turns**: in each row, Z axis (x, y) → (c·x + s·y, c·y − s·x) (0x6A116B..0x6A1211,
  0x6A6293..0x6A62AC); Y axis (x, z) → (c·x − s·z, c·z + s·x) (0x6A1218..0x6A12C0); X axis (y, z) →
  (c·y + s·z, c·z − s·y) (fn_006A12F0, 0x6A6317..0x6A6330); the third component is not touched. 0x6A1150 stores `c`
  in float for Z and Y (0x6A117C, 0x6A1229) and leaves `s` on the stack; its X axis (fn_006A12F0) and the Tumble
  0x6A627E leave both (0x6A627A..0x6A6284).
- **fn_007FB180**: `m0 = ((1 − xx)·c) + xx`, `m3 = (xy − xy·c) + s·z`, `m1 = (xy − xy·c) − s·z`,
  `m6 = (xz − xz·c) − s·y`, `m2 = (xz − xz·c) + s·y`, `m4`, `m7 = (zy − zy·c) + s·x`, `m5 = (zy − zy·c) − s·x`, `m8`
  (0x7FB1E7..0x7FB26E; c and s kept from 0x7FB189 / 0x7FB18F); translation 0 (0x7FB273..0x7FB279). The axis is unit
  length. The hand transforms (0, 1, 0) as a row vector (0x5B6EE8): `AxisAngle(axis, a)·v`.
- **SetInverse 0x7FB290**: `det = ((m2·m7 − m8·m1)·m3 + (m5·m1 − m2·m4)·m6) + (m8·m4 − m7·m5)·m0`
  (0x7FB290..0x7FB2C6). If `|det| < 1e-10` [0xC371D4], `det = ±1e-10` with the sign of det (+ for 0;
  0x7FB2C8..0x7FB2EE). Then `1/det` (`fdivr [0x8AA390]` at 0x7FB2F0), each cofactor × it (0x7FB2F8..0x7FB38D), and
  the translation `−(t·A⁻¹)` (0x7FB392..0x7FB3DF), each column in its own order.
- **SetPosition 0x423140**: four branches on a == 0 and s == 1 (0x423145 / 0x423151); with a ≠ 0, an inline RotateY
  on diag(s) (0x4231B3..0x42321C): rows (c·s, 0, s·s) / (0, s, 0) / (−s·s, 0, c·s). With s ≠ 1 the translation is
  `0 + p` (the cells set to 0 plus p: 0x423195..0x4231B0 and 0x423312..0x42332D), which turns −0 into +0; with s == 1
  it is copied (mov, 0x42325A..0x423268).
- The scale multiplies the rows (in glm, the columns) and the translation is written as is (0x423195, 0x6382B7,
  0x607606).

#### Other LH3D helpers

- **fn_007FA990 ArcTanOctant(a, b)** = atan2(b, a) by octants, fpatan of the smaller over the larger (float
  quotients); the symbols' name at this address is folded code:
  - a ≥ |b| → atan(b/a) (0x7FA990..0x7FA9AE, `fdiv` 0x7FA9B4, `fpatan` 0x7FA9BA, not rounded);
  - b ≥ |a| → [0x8C7B48] (float π/2 as a double, 1.5707963705062866) − atan(a/b) (0x7FA9BD..0x7FA9DB, `fsubr`
    0x7FA9E9 at 24 bits);
  - a ≤ −|b| → atan(b/a) ± [0x8D45D0] (float π as a double, 3.1415927410125732; − when !(b ≥ 0), tested with
    `fcomp [0x8AA398]` at 0x7FAA14, −0 counts as ≥ 0; 0x7FA9F0..0x7FAA34);
  - else −π/2 [0x9361E8] − atan(a/b) (0x7FAA3B..0x7FAA47).

  Ties go to the first branch; (0, 0) gives NaN; the first branch returns the unrounded fpatan, the other three a
  24-bit result. Both the quotient and the last addition or subtraction are rounded once to 24 bits from the exact
  value, in the registers' exponent range: a double rounding would put a sum that falls half way between two floats
  on the even one (a = ±2^-24, b = ±1 gave ∓1.57079625 for ∓1.57079637), and a float quotient below the float's range
  flushes where the FPU keeps it (7.56 and −2^-149: −1.85e-46, not −0). The oracle (`ArcTanOctant.json`): every float
  result of 4045 cases equal; 18 doubles of the first branch 1 ulp apart (the 80-bit fpatan against the double
  arctangent), which the callers round away: every caller stores the angle as a float or takes the YXZ angles from
  it (GetYXZ 0x7FAB30 turns the unrounded angle through cos and sin).
- **fn_007FAA50 GetYAngle(v)** (the symbols' `GUtils::SetUnitDirectionVectorFromScreenPoint` is folded code here):
  x·x + z·z (float products and sum, 0x7FAA5D..0x7FAA69) ≤ 1e-6 [0x9A2BAC] (0x358637BD) → 0 [0x8AA398]; else
  ArcTanOctant(−z, x) = atan2(x, −z) (0 along −z, π/2 along +x; 0x7FAA7A..0x7FAA88); y is not read.
  `Villager::EndPhysics` turns a row into a yaw this way at 0x5F0AE0..0x5F0AF6 and at 0x5F0B1A..0x5F0B30:
  GetYAngle(row), `fadd` π [0x8C36A0] (0x40490FDB) at 24 bits, `fstp`, WrapAngle. Along +x the sum is 3π/2 rounded,
  then less 2π: one bit off −π/2.
- **LH3DMath's yaw of a point** `LH3DMath::GetYAngle(LHPoint*)` 0x841290 is another function: atan2(z, x)
  (`fld z; fld x`, 0x841294..0x841297), plus 2π when negative, in [0, 2π). `LH3DMath::GetYAngle(LHPoint* a,
  LHPoint* b)` 0x841260: fpatan of (b.z − a.z) over (b.x − a.x) (24-bit subtractions, 0x841264..0x841270), so
  atan2(dz, dx); negative (`fcom [0x8AA398]`) → + 2π [0x8AB210], a 24-bit `fadd` (0x841272..0x841287 /
  0x841299..0x8412AE). The fpatan is extended when not negative, the float sum otherwise; the caller's `fstp` rounds
  it (`Object::SetFocus` 0x6393EA).
- **fn_007FAAF0 WrapAngle(a)** (the symbols' `GVillagerStateTableInfo::GetInfo` is folded code): a > π [0x8C36A0] →
  a − 2π [0x8AB210] (0x40C90FDB; `fsub` 0x7FAB05); else a < −π [0x8C79A4] (0xC0490FDB) → a + 2π (`fadd` 0x7FAB1D);
  else a (0x7FAB24). Once only (10 → 3.7168); ±π stays.
- **InverseSquareRoot 0x841170**: index (bits >> 17) & 0x7F (the exponent's last bit and the mantissa's first 6,
  0x84118E..0x84119D), exponent (0x5F000000 − (e << 22)) & 0xFF800000 (0x841179..0x8411A0), one Newton step
  ((3 − (x·y)·y)·y)·0.5 ([0x8C2C50] = 3, [0x8AA3B4] = 0.5, 0x8411B0..0x8411C2). The 128-byte table is
  `MakeInverseSqrtLookupTable`'s (0x8411D0): ((bits(1/√x) + 0x2000) >> 15) & 0xFF for x = bits((i | 0x1F80) << 17),
  0.5 ≤ x < 2 (0x8411E7..0x841213: `fsqrt` and `fdivr` of the double 1.0 [0x8AB680], each rounded to a float), then
  entry 0x40 = 0xFF (0x841224). A little below the true value.
- **fn_007FB5C0 NormaliseRows**: each row times InverseSquareRoot of its length squared (0x7FB5E5 / 0x7FB620 /
  0x7FB65B), no re-orthogonalisation.
- **fn_007FAFF0 Mul**: a copied first (in place is safe); per row, column 0 (ai2·b6 + ai1·b3) + ai0·b0, columns 1
  and 2 (ai0·b0c + ai2·b2c) + ai1·b1c (0x7FAFF3..0x7FB177). **fn_007FAE60 MulPre**: the result's row r is b's row r
  times a (0x7FAE63..0x7FAFE3).
- **LH3DTech::UpdateWorldToCamera 0x819690**(eye, target), up (0, 1, 0) at [0xEA1B48]: d = target − eye
  (0x819697..0x8196B5); |dx| and |dz| < 1e-4 (the double 9.9999997e-5 [0x8C79D8]) → dx = ±1e-4 ([0x8BF518] /
  [0x8D8738]) (0x8196B9..0x8196F2); an all-zero d is not normalised (0x8196FA..0x819727; the normalisation
  0x819729..0x819763); U = up − (up·D)D, not normalised if zero (0x8197CF..0x819854); R = U × D
  (0x819864..0x819896); rows (Rx, Ux, Dx), (Ry, Uy, Dy), (Rz, Uz, Dz); translation −(R·eye, U·eye, D·eye), each
  ((−x0·e.x) − x1·e.y) − x2·e.z (0x8198BE..0x819904).
- **UpdateCamera 0x819920** (0x819AF8..0x819BAB): W2C at [0xEA1D28]; W = the world-to-clipping matrix [0xEA9E40] is
  W2C with its x column × sx [0xE83A00] = 1/tan(fov/2), its y column × sy [0xE83A04] = aspect × sx, z (and the
  translation) copied; aspect [0xE839EC] = W/H (UpdateViewPort 0x81909C); the inverse [0xEA9DE0] by SetInverse at
  0x819BB0. **ChangeFov 0x8195B0..0x8195F3**: `fmul` 0.5, `fptan`, `fdivr` 1 → [0xE83A00], `fmul` [0xE839EC] →
  [0xE83A04]; 1/T is rounded once from the x87's extended value.
- **Summation orders** of a point through W, each site its own: fn_0081F1D0 0x81F1D0..0x81F26B
  ((m0·x + m6·z) + m3·y) + m9 (Z 0x81F1D0..0x81F1F4, X 0x81F214..0x81F232, Y 0x81F24B..0x81F26B);
  `LH3DBoundingBox::CheckRegionOnScreen` 0x868D07..0x868D75 z term first, x last, in all three columns;
  fn_00877370 0x877370..0x8773D8 (the corner on the stack as [z, y, x]; Y's order differs from X's and Z's); the box
  centre 0x868CBC..0x868D04, cx = ((m3·by + m6·bz) + bx·m0) + m9, cy = ((m1·bx + m4·by) + m7·bz) + m10,
  cz = ((m2·bx + m5·by) + m8·bz) + m11, then on the stack as [cz, cy, cx].
- **fn_00812170** 0x8121E6..0x8123B0, the root of the bones: obj+0x14 through W, every cell summed x, z, y; not
  Mul(object, W) of fn_00811010 ([0xEA9E10]), whose column 0 sums z, y, x.
- **fn_00839980** 0x839B3D..0x839B59: each bone in order (parents first), out[i] = Mul(local[i], parent or root); the
  exe counts the parent indices from the start of each part of the mesh (0x839A26..0x839B59, 0x83A200..0x83A2D4).
  Every boned mesh of AllMeshes.g3d has its bones in one part.

#### Which constructor each object uses

By vt+0x63C, searched in the vtables:

- `Object::GetWorldMatrix` 0x638200, Y only (`T(x, GetAltitude + y, z)·Ry(−GetYAngle)·S`): Abode, Windmill, the
  animals, AnimatedStatic, Feature, BigForest, the creature (vtable 0x8CCE4C), Field, Tree, Villager, the worship
  sites, the spell icons, Totem, StoragePit…
- `MobileObject::GetWorldMatrix` 0x607560 and `MobileStatic::GetWorldMatrix` 0x608DE0, YXZ:
  - MobileObject 0x607560: Arrow, Ball, Pot, PileWood, PileFood, Whale, MagicFood, MagicWood…
  - MobileStatic 0x608DE0: Bonfire, DeadTree, FelledTree, Rock, MagicTeleport, Fragment…
- `Game3DObject::SetPosition` 0x63B740 (LHPoint) / 0x63B680 (MapCoords): `T(p)·YXZ(y, x, z)·S`, with the 9 cells × s.
- `Game3DObject::SetPositionAndXZYScale` 0x63B390: `T(p)·Ry(−a)·diag(s·xz, (s·xz)·(y/xz), s·xz)`.

#### The object's own placement

An object's 3D object (Game3DObject, Object +0x40) keeps its own matrix +0x14..+0x40 (three rows, the translation
at +0x38..+0x40), its scale +0x44 and its y angle +0x48. Nothing makes it again from the MapCoords when it is read:
every reader gets what the last writer left (read 2026-10-28, xref notes #3738..#3743).

- **The cells** (oracle, 2026-10-28):
  - upright, `LH3DObject::SetPosition(p, a, s)` 0x423140 and its inline copies (`Object::SetXYZAngles` 0x638D00,
    `SetXYZAnglesAndScale` 0x638F80, `Object::GetWorldMatrix` 0x638200, `Living::PreDrawScale` 0x51AF00 through
    `RotateY` 0x5198F0): with a ≠ 0 and s ≠ 1, diag(s) turned in place with the cosine and the sine of `fcos` / `fsin`
    unrounded, so m0 = m8 = R(c·s), m2 = R(sin·s), m6 = −R(sin·s), one 24-bit product each (0x4231B3..0x42321C). In
    402 random cases `affine::PlacementMatrix` gives the same bits in every one; the model of a Transform with
    `AngleY(a)` and scale s, R(R(c)·s), is one bit off in 162 (m0, m2, m6, m8), e.g. a = 0x403B9275,
    s = 0x3FA282A3: 0xBF9EE9E6 against 0xBF9EE9E7. At s = 1 or a = 0 they are equal. The zero cells keep the sign of
    the turn's products (−0 where both are −0); openblack's are +0;
  - leaning, `Game3DObject::SetPosition(p, x, y, z, s)` 0x63B740 / 0x63B680: `SetYXZMatrixOnly(y, x, z)`, then each of
    the nine cells × s (one float product): 300 of 300 cases equal to `Model(p, RotationYXZ(y, x, z), s)`.
- **Its writers**: the creation (Abode and every MultiMapFixed 0x52E890, Abode again after its sink 0x403200, Tree
  0x749F70, AnimatedStatic, CitadelHeart, GStreetLantern; leaning: MobileObject 0x6071E3, MobileStatic, Field,
  DeadTree, Reward); the angle and scale setters (102 classes), the script's `SET_POSITION` 0x6F8B78, `Tree::Grow`
  0x74A3F0; the moves of mobile things (`MobileObject::MoveAlongPath` 0x6078EF, the rock's and the teleport's
  `ActualMoveMapObject`, `SpellIcon::MoveMapObject` 0x72664A) and the physics in flight (`PhysOb::PrepareForDraw`
  0x7FCE80); and every draw of the living and of a few others (`Living::PreDrawScale` 0x51AF00: the point between the
  turn's two places by the turn's fraction, `GetYAngle() + π/2`, the drawn scale; totems, piles, highlights).
- **Not written**: `Object::ActualMoveMapObject` 0x638040 (remove, `SetPos`, insert), so a static moved by
  `MOVE_GAME_THING`'s fallback is still drawn, picked and tested on the screen where it was (✅ a behaviour of the
  original, not an openblack fault: openblack does the same, `object_placement::MoveWithoutModel` in `MoveGameThing`); a land change (the
  heights of [Where each class takes its drawn height](../world/terrain.md#where-each-class-takes-its-drawn-height));
  a living thing whose block is not drawn keeps the frame it was last drawn in.
- **Its readers**: `IsObjectOnScreen` 0x81F1A0 (`GAME_THING_FIELD_OF_VIEW`, the help bubble, the guidance's two
  remarks), the draw's screen test and the hand's object under the cursor (`AddForDrawing`, `CheckRegionOnScreen`
  0x868C80), the points of a mesh through it (doors, a store's piles, the chimney, the leash posts, a worship site's
  centre and food point), the collide data, the script camera's follow (+0x38) and every draw.

<a id="zoomer-lh3dlib"></a>
### Zoomer (LH3DLib)

The structure is 0x30 bytes (bw1-decomp `Lionhead/LH3DLib/development/Zoomer.h`):

- the value +0x00;
- the destination +0x04;
- the destination speed +0x08;
- the speed +0x0C;
- TimeM2 +0x10 (only ever set to 0, at 0x407D95, 0x441ADC and 0x442744);
- the time (CurrentTime) +0x14;
- the duration +0x18;
- the start value +0x1C;
- the start speed +0x20;
- `c2`, `c3`, `c4` +0x24..+0x2C (the coefficients of t²/2, t³/6, t⁴/24).

It is a quartic that starts from the current value and speed and reaches the destination at T with the destination
speed and acceleration 0. Everything is in float and in the x87 order:

- **`Zoomer::SetPosition(p)` 0x441AC0**: value = destination = start = p, and the rest to 0 (0x441AC6..0x441AE8).
- **`SetDestinationWithSpeedAndTime(dest, vDest, T)` 0x407D60**:
  - with `T < 0.001` [0x8AA3B0] (or NaN: `fcomp; test ah, 1` at 0x407D67..0x407D75 sets C0 on "<" and on an
    unordered compare), the same stores as `SetPosition(dest)` (0x407D77..0x407DA8);
  - otherwise: `A = (T·T)·0.5`, `B = (A·T)·0.33333334` [0x8AB26C] (0x3EAAAAAB), `C = (A·A)·0.16666667` [0x8AB268]
    (0x3E2AAAAB);
  - the matrix M (rows (C, B, A) / (B, A, T) / (A, T, 1)) is inverted with `LHMatrix::SetInverse` (0x7FB290);
  - `r1 = (dest − start) − T·v_start`, `r2 = vDest − v_start`;
  - `c4 = (inv10·r2 + inv00·r1) + inv.t.x` (0x407E6D..0x407E81, 0x407EC5), `c3 = (inv01·r1 + inv11·r2) + inv.t.y`
    (0x407E83..0x407EA5), `c2 = (inv12·r2 + inv02·r1) + inv.t.z` (0x407EA1..0x407EC2);
  - the order in the exe: A [0x407DAB..0x407DC3] (with the start speed = speed [0x407DBC] and the start value =
    value [0x407DC9] in between), B [0x407DD0..0x407DE0], C [0x407DE6..0x407DF9]. Then the matrix M on the stack,
    esp+0x10..+0x3C, with translation 0 [0x407E0A..0x407E40], the inverse [0x407E44], r1 [0x407E49..0x407E55] and
    r2 [0x407E57..0x407E5D].
- **The determinant floor.** For this M, `det = −T⁶/144`, which drops below 1e-10 with **T < 0.0493 s**. Then the
  coefficients come out × `(T⁶/144)/1e-10`. The zoomer barely moves and jumps to the destination at the end. A step
  from 0 to 10 is worth, at T/4: 2.6171875 with T = 2.5 s and 0.7444 with T = 0.04 s (the closed form would give
  2.617).
- **`Zoomer::Update(dt)` 0x442720**: `t = dt + time`. If `t ≥ duration`, it stays at the destination and its speed,
  with `time = duration`: it does not extrapolate. Otherwise, with `a = (t·t)·0.5`, `b = (t·a)·0.33333334` and
  `C = (a·a)·0.16666667`:
  - `speed = ((t·c2 + a·c3) + b·c4) + v_start`;
  - `value = ((((C·c4) + b·c3) + a·c2) + t·v_start) + start`;
  - `t = dt + time` is `fld dt; fadd [+0x14]` [0x442720..0x442724]. The comparison `fcom [+0x18]; test ah, 1; jne`
    [0x442727..0x442732] follows the curve if t < duration **or if it is unordered**: with t NaN it does not go to
    the destination;
  - the ranges: destination and its speed [0x442734..0x44274E]; a [0x442751..0x442757]; b [0x44275D..0x442761];
    speed [0x442767..0x44277D]; C [0x442780..0x442784]; value [0x44278A..0x4427A5].
- **`Zoomer3d`** (0x90 bytes: x +0x00, y +0x30, z +0x60):
  - `Zoomer3d::SetDestinationWithTime` 0x44E760: destination speed 0. There is no three-axis
    `SetDestinationWithSpeedAndTime`: no place in the binary has been seen that gives a Zoomer3d a destination speed
    other than 0. The x axis does `push T; push 0; push p.x; call 0x407D60` [0x44E770..0x44E77A]; the y and z axes
    are inlined [0x44E77F..0x44E9E5], with the same threshold and the same matrix; the vector times the inverse is
    done by fn_00418A50: `((v.z·m2i + v.y·m1i) + v.x·m0i) + t.i`, with v.z = 0, which only adds a 0 to the sums of
    0x407D60;
  - `GetCurrentValue` 0x4605D0;
  - `Update`: the three `Zoomer::Update` (GCamera::Update 0x441FEE..0x442029).

#### Who uses it

- The player camera: a `Zoomer3d` of the position (GCamera +0x118) and of the focus (+0x88). `CameraModeNew3` gives
  them a destination every frame with `SetDestinationWithTime` (0x4604A4..0x4604D2). GCamera::Update first calls the
  mode (vt+8, 0x441FD9) and then `Update(min(dt, 0.1 [0x8AB22C]))`
  ([GCamera and its zoomers](../camera/game-camera.md#gcamera-and-its-zoomers)).
- The script camera: position, focus and FOV.
- The hand's roll (CHand+0xD4, 0.4 s) and its distance (g_HandDistZoomer).
- The animals (roll), the piles (sinking, 1 s), the fields (1 s), the totem and the spell's doves and wolves (fade).
- **Units matter.** The 0.001 threshold and the determinant floor depend on T, so a Zoomer has to be in the
  original's unit. The totem's one (TotemStatue +0x9C) is in **milliseconds**: SetWorshipPercentage 0x738270 gives
  it T = |Δ|·5200 [0x999A98] ms (with the threshold in ms, 0x738293, and an inline copy of 0x407D60 from 0x7382FC),
  and TotemStatue::Draw 0x738960 updates it with the clock's whole ms (0x738967..0x7389B5). In seconds, the floor
  would have slowed changes of |Δ| < 0.0095; in ms it only slows those of |Δ| < 9.5·10⁻⁶.
- The callers do not repeat the threshold: `Animal::SetTowardsAngle` calls 0x407D60 directly (0x4185D5), and the
  threshold at 0x407D67..0x407DA8 already does the `SetPosition`.

### Sources

- bw1-decomp: `Lionhead/LH3DLib/development/Zoomer.h` (the structure); the rest read in the exe with the xref index.
- The oracle: `ArcTanOctant.json` (4045 cases) and GetYXZ (4008 cases).
- Eleven recordings of the original's zoomers (the camera test below).

## openblack

**Done.** The matrices and the Zoomer are ✅ faithful, checked bit for bit against the original. Port decisions are
in [Port decisions: engine and world](../project/port-decisions-engine-world.md).

### The matrix module

`openblack::affine` in `src/3D/ObjectMatrix.{h,cpp}` (the rotations: `RotationYXZ`; `AngleY`;
`RotationXYZ`; `RotateY`, `RotateZ`, `RotateX`; `TurnRows(m, axis, a)` and
`(m, axis, c, s)`; `AxisAngle`; `ArcTanOctant`; `GetYAngle`, `GetYAngleBetween`, `GetYAngleOfXZ`; `WrapAngle`;
`InverseSquareRoot`, `NormaliseRows`; `Inverse`; `PlacementMatrix`; `Model(p, R, s)` and
`Model(Transform)`) and `src/3D/AffineMatrix.{h,cpp}` (the 12-float matrix: `Mul`, `MultiplyReversed`, `Inverse`,
`WorldToCamera`, `WorldToClipping`, `LensScalesFromFov` (the horizontal field of view from
`Camera::GetHorizontalFieldOfView`, in radians), `FrameMatrices`, and one function per summation order of a point
through W: `ToClipForScreenTest` (fn_0081F1D0's order), `ToClipForBoxTest` (0x868D07), `ToClipForShadowBlocks`
(fn_00877370), `BoxCentreThroughObject` (0x868CBC), `BoneRootToClip` (fn_00812170) and `SkinBones` (fn_00839980);
these were once named after the addresses). `GetYAngleBetween` and `GetYAngleOfXZ` share `PositiveYAngle`, the
fpatan plus 2π when negative. The inverse square root of the models is `gutils::ModelInverseSquareRoot`
(`src/Common/ModelInverseSquareRoot.{h,cpp}`, with its 128-entry table built as 0x8411D0 builds it). Built with /fp:precise and no FMA contraction; `test_affine_matrix` pins one product to catch a
change. Tests: `test_affine_matrix`, `test_game_matrix`, `test_game_angles`.

The tests pin the original's bits where an order of operations matters:
- `GetYAngle`'s threshold: 0.001 squared is `0x358637BE`, one bit over 1e-6, while x = `0x3A83126E`, z = `0x34A10FB0`
  sum to `0x358637BD` itself and give 0.
- An object turned and tilted at Land 1 whose box centre rounds apart in `BoxCentreThroughObject`'s order from glm's
  (glm gives cx `0x44E0B8F9`), and an object where `BoneRootToClip` and `Mul(object, W)` round apart (cell 3: `Mul`
  gives `0xBEC5F0E1`), with a two-bone chain under it.
- One box per column where the box centre's order rounds apart from glm's or `ToClipForScreenTest`'s: cx from both
  (`0x44E16A61`), cy and cz from `ToClipForScreenTest`'s (`0x41E29C48`, `0x452CCFE9`; glm sums cy and cz in the box
  centre's order).

- `TurnRows` has two overloads, `(m, axis, a)` and `(m, axis, c, s)`, because the original rounds `c` to a float for
  the Z and Y axes and leaves `s` on the stack, while its X axis and the Tumble leave both unrounded
  ([The constructors](#the-constructors)).
- `ArcTanOctant` against the original (oracle, `ArcTanOctant.json`): the 18 doubles of the first branch that are 1 ulp
  apart are rounded away by every caller; for example a = 0x4084784C, b = 0x401AA07B gives the original's
  0x3FE0E7CCD80212B1 and our 0x3FE0E7CCD80212B2, both the float 0x3F073E67; the YXZ angles taken from it agree 4008
  of 4008.
- `GetYAngleBetween` is ⚠️ approximate: the original's fpatan (the yaw between two points, [Other LH3D helpers](#other-lh3d-helpers)) is 80-bit, and
  `std::atan2` in double may differ in the last bit before the caller's float store.
- `affine::ArcTanOctant` rounds as the FPU does: the quotient through a double rounded to odd, then 24 bits with its
  exponent kept; the constant and the arctangent through a two-sum rounded to odd, then a float.
- `PlacementMatrix` makes the same four branches as 0x423140, including `0 + p`.
- **⚠️ approximate** `LensScalesFromFov` takes 1/T in double, then rounds: the original rounds once from the
  extended value, so a halfway case could differ by one ulp.

Where each is used. The creations with `AngleY` are the archetypes of Abode, AnimatedStatic, BigForest, Feature,
Tree, Pot, MobileObject and Shark, plus the creature, `DesignedScenery`, the rivers, the worship sites and their
icons, the script's citadel and the ground marks. They also use it when moving: the shark, the paths and the drawing
of villagers and animals (the «Scawen» angle = `angle + π/2`). Also the physical shield (`MapShield.cpp`: the inline
RotateY of 0x72D4DB..0x72D558 on the identity, with `c` in float, is `AngleY` bit for bit), the rotating glow of the
belief symbols (`TownBelief.cpp`: the angle of sprite +0x14 of 0x69D8C5 carried as the SetAngleY matrix) and
`billboard::YawToEye`. `RotationYXZ` is used by MobileStatic, DeadTree and the hand (`HandAnimator`). `RotationXYZ`
is in `RandomiseOrientation` (PSys). `RotateY`/`RotateZ` are in the moon (`moon::Model`, `Graphics/Moon.cpp`) and in the ships
(`MissionaryBoat`). `TurnRows` is used by RotateAxis (PSys), the Tumble (`Sprinkle`) and the pick-up particles
(`HandEffects`). `AxisAngle` is in the hand's tilt (`HandPlacement`) and in the physics rotation
(`physics::Body::Integrate`). `Model` is used by the renderer and the boxes: `RenderingSystem`,
`RenderingSystemTemple`, `Renderer`, `CarriedProps`, `FeatureBuild`, `Buildings`, `Sharks` and `Archetypes/Utils`.

Fixed after reading the binary:

- `ParticleEffect.cpp` RandomiseOrientation: it was `eulerAngleXYZ(x, y, z)` (the transposed composition), with the random
  numbers in the order x, y, z. Now it is `z, y, x` and `RotationXYZ`.
- `HandEffects.cpp`, the flipping of the pieces when picking up: it went on the right with +a and now goes on the left
  with −a, like 0x6A6200 and `Sprinkle.cpp`.
- `CreatureArchetype.cpp`: `eulerAngleY(+y)` becomes `AngleY(y)`. Not visible today: the script passes π.
- `RenderingSystem.cpp` / `RenderingSystemTemple.cpp`: the model was `R·T(p·R)·S`; now it is `T(p)·R·S`. The earlier
  translation was `R·Rᵀ·p`: a few ulp from p if R is a rotation, but far from p (proportional to |p| ≈ 1000-3000 m)
  if it is not. There are three users whose R is not a rotation, and in them the change is visible:
  - the hand's bands while they fly (`HandMagicFX.cpp` SetTransform: `mat3(M)/scale` of a linear interpolation of
    two matrices);
  - the objects villagers carry on slopes (`CarriedProps.cpp`: the shearX/shearZ shear of fn_0051B220);
  - the physical shield between two turns (`MapShield.cpp` DrawPhysical: interpolates the rows, 0x72CEEC).
- `physics::Body::Integrate`, the rotation: the direction was already the original's and now it is built like it.
  fn_007FE260 (0x7FE706..0x7FE748) does `inv = 1/angle` (fdiv), `axis = step·inv`, `fn_007FB180(axis, angle)` and the
  rows by that matrix (fn_0046D9D0: `r_k' = r_k·M`). Its torque is `F × r` (0x7FE0F9..0x7FE11F and
  0x7FD7FD..0x7FD823) and openblack's `r × F`, so the original's ω and axis are openblack's with the sign flipped:
  `AxisAngle(−step·inv, angle)·R`. ≈ulp.
- `Particles/Rules/Shield.cpp` (VapourEndEffect, fn_0057D2B0 at 0x6A3D7C): the direction has been read. It is the
  quaternion `(cos(a/2), sin(a/2)·n)` of fn_0057D1D0, its matrix via fn_0057D0B0 (`m1 = xy + wz`: the rotation by +a
  with the right-hand rule, by rows) and the rows multiplied by it (fn_007FAFF0). It rotates each row by +a about
  `n = last × p` (fn_006A3E20), like openblack's `glm::rotate(+a, n)`. The cells are not those of the quaternion
  **⚠️ approximate**.
- `AbodeArchetype.cpp`, a store's piles: each stands at its point of the store's mesh through the store's whole
  placement (`affine::PlacementMatrix` with its place, turn and scale), as the original; the scale was missing.

Without a source, left as they were and marked **🔍 inferred**:

- `HandHolding.cpp` HeldSway: it remains to read which angle and which axis each fn_007FB180 carries
  (0x5B49B6..0x5B4ACE).
- The hand's bands (`HandMagicFX.cpp`): they only fit if the bone has Y and Z swapped.
- `HandTrees.cpp`: the toppled tree and the tug (0x5B8700).
- The tree bending in `RenderingSystem.cpp` (0x74B016).
- The sun (`Renderer.cpp`, fn_0086C020).
- `TempleInterior.cpp` (always 0) and `HandArchetype.cpp` (`eulerAngleXYZ`, no effect: HandPlacement rewrites it).
- The villager's initial angle (0x74F950).
- The X-only scale of the rivers.
- The direction of the hand's roll (`HandPlacement.cpp`, the Zoomer CHand+0xD4). The original rotates with
  `fn_007FB180(dir, [CHand+0xD4] + vt+0x14 + [esp+0x20])` (0x5B49A0..0x5B49C8, `rotate(−a)`), but it remains to read
  the axis dir ([esp+0xA4]) and how that matrix reaches the hand's (fn_007FAFF0 0x5B4AE5). openblack rotates by
  glm's +roll about `forward`. Not visible today: the destination is always 0.

### The object's own placement in openblack

`components::ObjectMatrix` (`src/ECS/Components/ObjectMatrix.h`: the 12 cells, the scale, the y angle), made by
`ecs::object_placement` (`src/ECS/ObjectPlacement.{h,cpp}`): `PlaceYaw` with `affine::PlacementMatrix` and
`PlaceYXZ` with `affine::PlacementYXZ`, the original's two constructors bit for bit but the zeros' signs; `Cells`
gives the placement's matrix, or the Transform's model for an object that has none. The Transform stays the game's
position. Test `test_object_placement` (the oracle's cases: 15 upright, 6 leaning). Its readers move to it one at a
time, each measured (below).

Where it is made, as the original makes it:
- at the creation, upright: trees, abodes (at their place after the sink), features, flowers, big forests, animated
  statics (`PlaceYaw` in their archetypes), worship sites (scale 1); leaning: mobile objects and pots (angles 0, y, 0), mobile statics (and a
  bonfire's angles set after it), dead trees (`PlaceYXZ`);
- by the setters, `object_placement::PlaceAgain`, from the Transform's position and scale and the kept angles: an
  angle set (`placed_turn::SetAngles`), a script's placing (`placed_turn::StandUpright`: `SET_POSITION` and the height
  above the land), a scale set (`living::SetThingScale`) and a tree's growth (`GrowTree`).
- The translation is the Transform's position at that moment, which stands for the original's MapCoords point.
- Read through `object_placement::Of` / `Cells`: the placement kept, while the Transform is the one it was made from
  (`ObjectMatrix::madeFrom`). When the Transform has moved since through a move that makes no placement of its own
  in openblack yet (a walk, the physics' landing, the hand's drop, the moves listed in [Pending](#pending)), the
  placement its class's constructor makes from the Transform now, as the original's writers of those moves do; and the
  Transform's model while its turn is not one a constructor makes (the physics in flight, the hand). So a reader that
  moves to it differs from the Transform's model only in the constructor's bits, but where a move leaves the placement
  behind (below); a land change does not keep it yet.

Its readers so far (each its own measured change):
- the field of view's test of an object (`field_of_view::ObjectOnScreen`: the script's field of view, the help
  bubble's sign, the guidance's building remark), [Script camera](../camera/script-camera.md);
- the builders' ring round a building site (`PosBuilderProcess`, [Towns](../beings/towns.md)) and the wood point of a
  worship site's site (`(9, 0, −50)` through the site's cells, `StandardBuildingSite::GetResourcePosAndYAngle`
  0x43C220; the worship site is made upright at scale 1, so its cells are AngleY's as before).
- the draw list's on-screen test of an object (`draw_list::OnScreenInputsOf`: the centre, the origin and the scale),
  [The object draw list](../rendering/object-draw-list.md);
- the drawing of what stands where it is placed (`DrawnModel`'s plain branch: not in the hand, not in the physics, not
  between turns, not a creature in its pen), the town centres' light columns, a building's partial build and its
  damage mesh, the fragments at rest, the river prints, the temple's bake onto the land and its interior, and the
  temple destruction's placement; the citadel heart now makes its placement at its creation (upright, scale 1: the
  cells AngleY gives). A change of the placement has the drawn instances written again (it is not part of the draw
  layout);
- the hand's pick and its feel of a mesh (`hand_mesh_model::ObjectModel` and `GroundedPosition`: the cells and the
  origin; a one-shot orb keeps its facing), [Picking](../hand/picking.md#the-pick-in-openblack);
- **Left behind** (`object_placement::MoveWithoutModel`): `MOVE_GAME_THING`'s fallback for a thing that is not
  alive keeps the placement it had for the Transform the move leaves, so the thing is drawn, picked and tested on the
  screen at its old place, as the original's; a later move of its Transform reads it as placed again. openblack's
  other plain map moves of things that are not alive are followed by a writer (`SET_POSITION`) or by the physics
  (the scaffold's push), as in the original. Test `ObjectPlacement.AMoveWithoutTheModelLeavesThePlacementBehind`.
- Not a reader: an abode's entrance points go through its world matrix made fresh from its game position
  (`Object::GetWorldMatrix` 0x638200), now `affine::PlacementMatrix` at its map position
  ([Buildings](../world/buildings.md)).

The editor's inspector shows it ("Own placement": the point, the angle, the scale, how many cells the Transform's
model has otherwise, and how far the game position has gone from it).

🎯 **Chosen difference (2026-10-28).** The villagers' and animals' game logic reads the placement worked out in the
turn, never the one the last drawn frame left. The original's `Living::PreDrawScale` 0x51AF00 writes their matrix
on every draw (between the turn's two places, by the turn's fraction), and its game readers (the field of view, the
help bubble, the guidance, the hand's pick) read that one, so a turn's result there depends on the frames drawn before
it. In openblack a turn never depends on them.

### The Zoomer

`openblack::Zoomer` and `openblack::Zoomer3` in `src/Common/Zoomer.{h,cpp}`, the same fields (`value`,
`destination`, `destinationSpeed`, `speed`, `time`, `duration`, `startValue`, `startSpeed`, `c2`..`c4`; TimeM2 is
not kept, since it is only ever zeroed), the constants `k_Third` and `k_Sixth` with the original's bits, and the
matrix inverted with `affine::Inverse`. `Zoomer::SetDestinationWithSpeedAndTime`, `Zoomer::Update` and
`Zoomer3::SetDestinationWithTime` keep the original's operation order line for line.

- Users: the player camera (`Camera::UpdateZoomers`, a `Zoomer3` of the position and one of the focus), the script
  camera (`ScriptCamera` keeps its position and focus in two `Zoomer3`), the hand's roll (`HandPlacement`), the
  animals, the piles, the fields, the totem and the spell's doves and wolves.
- `ZoomInterpolator` (the old player camera interpolator) is gone. It was the same quartic with normalised t, not a
  quintic, and it differed: with p0 == p1 it ignored the speed; it divided by p1 − p0; it extrapolated with t > 1; it
  did not have the 0.001 threshold or the determinant floor.
- The totem's Zoomer is fed whole engine ms, as the original's: the unsigned difference since its last draw, through
  the statue's own inline advance (`AdvanceRise` / `percentage::StepRise` in `src/Worship/WorshipPercentage.cpp`;
  test `test/test_village_totem.cpp`).
- Checked: `test_camera` `ZoomerMatchesRecording` goes through the original's 11 recordings. The coefficients of
  each curve (51 669) and the value and speed of each state (51 636) come out **bit for bit**. Also `test_zoomer`.

### Test hooks

None of their own. The cross-page list is in [Debugging](../project/debugging.md).

## Pending

- The rotations without a source in [LH matrices](#lh-matrices) (HeldSway, bands, toppled tree and tug, bending,
  sun, temple, hand, villager, rivers): left as they were until their constructors are read. Verify with
  xref (`xref.py show` on each address listed above).
- `Game3DObject::SetPositionAndXZYScale` 0x63B390 has no function of its own: no place needs it yet
  (`Game3DObject::SetPosition` 0x63B740 is `affine::PlacementYXZ`).
- [The object's own placement](#the-objects-own-placement): the readers not listed
  [in openblack](#the-objects-own-placement-in-openblack) still build the Transform's model; each moves to
  `object_placement::Cells` in its own measured commit. The zero cells' signs are not kept. Not made yet: the
  classes whose creation makes none (fields, rewards, street lanterns, spell icons, totems, whales: they read as the
  Transform's model); the mobile moves (`MoveAlongPath`, the rock's and the teleport's moves, the spell icon's move),
  the forest's shrinking trees and big forests and a rock's size, which `object_placement::Of` reads as placed again;
  the physics in flight (the Transform's model); `SetPositionAndXZYScale`; and the per-draw writers (totems, piles,
  highlights; the living keep none, the 🎯 chosen difference of 2026-10-28).
- The player camera and the script camera each have their own `Zoomer3`; in the original they are the same ones from
  GCamera **🔍 inferred**. The player camera's dt is the frame's in µs, not the whole ms of `GetCameraTimeInc`
  0x555820 **⚠️ approximate**.
- `CameraModeNew3`: missing are the second ×2 of the duration, the 1.0 of `MaintainSpell & 0x40` and `[esp+0xB8]`
  (0x4601A9..0x46024A). It is not part of the Zoomer.
- The hand: the Zoomer3d 0xD13FB0 of the «up» when holding (0.4 s), the stretch of the tug +0x11C (0.3 s) and the
  conditions of 0x5B4251..0x5B42CD for the roll. The direction of the roll rotation **🔍 inferred**: it remains to read
  the axis of 0x5B49C8 and what fn_007FAFF0 does at 0x5B4AE5. The sum at 0x5B42DF puts c4·b before c3·a, a different
  order from `Zoomer::Update`: the last bit **⚠️ approximate**.
- The player's FOV (TODO #707) is not a Zoomer.
- `LandIsland::GetNormalAt` compares with `GetCellsPerSide()` instead of 0x200 (the same in the game's maps).
- The normal tables are computed as in fn_00803890 assuming the FPU at 24 bits during the island initialisation
  **🔍 inferred**.
- `Particles/Rules/Shield.cpp`: the cells of the quaternion of fn_0057D0B0 (the direction has been read)
  **⚠️ approximate**.
- The interpolation of the physical shield between turns: the original interpolates the matrix with the scale inside
  (0x72CEEC); openblack interpolates the rows of the rotation and the scale separately.
