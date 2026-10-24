# Coordinates, distances and angles

## Summary

The original keeps every position on the map as a **MapCoords**: x and z in 16.16 fixed point (the high word is the
10 m cell, the low word the fraction) and a float height above the ground. On top of it sit the `GUtils` distances,
which all go through a table-based inverse square root (so 100 m measure 100.0244 m), the sigmoids that turn a
distance into a weight, and the game angles: an 11-bit integer, 2048 per turn, from an arctangent table.

openblack ports the three families bit for bit as pure modules: `map_coords` (`src/3D/MapCoords.{h,cpp}`),
`gutils` distances (`src/Common/GUtilsDistance.{h,cpp}`) and `gutils` angles (`src/Common/GUtilsAngle.{h,cpp}`), with
the points around an object in `ecs::object`. openblack still stores positions as float metres, so each conversion to
MapCoords can lose one unit (0.15 mm) on an already quantised value; the cells never change. The progress rows are in
[the progress index](../project/progress/INDEX.md).

## The original

The FPU runs at 24 bits on the game thread ([Floating point](floating-point.md#the-games-fpu-runs-at-24-bits)), so
every product and quotient below is a float operation, and `fsin` / `fcos` come out unrounded
([The x87 arithmetic](floating-point.md#the-x87-arithmetic)). Everything on this page is ✅ faithful (read in the
exe) unless marked.

### MapCoords

It is the original's only representation of a position on the map and everything uses it: 136 calls to the
`MapCoords(LHPoint)` constructor, 196 to `GetLHPoint`, 71 to `ToMap`, 80 to `InBounds` and 58 to `GUtils::Spiral`.

#### The structure

bw1-decomp `MapCoords.h`: `{int32 x (+0), int32 z (+4), float altitude (+8)}`. x and z are 16.16: the high word is the
10 m cell and the low word the fraction (one cell = 0x10000). `altitude` is the height **above the ground**; the
absolute one is `GetAltitude(pos) + altitude`. `JustMapXZ = {int16 x, z}` is a step in cells.

Because the FPU runs at 24 bits (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D; called in `pc_main` and in every
`GGame::EndTurn`), every product is rounded to float. The exact replica is `(int32)(m * 6553.6f)` done in float; doing
the product in double gives ±1 unit (0.15 mm) in 41 % of the values.

#### The conversions, cells and bounds

| openblack (`map_coords`) | Original | What it does |
|---|---|---|
| `k_FixedPerMetre` = 6553.6f | [0x8AC400] = 0x45CCCCCD (6553.60009765625) | metres → 16.16 |
| `k_MetresPerFixed` = 10/65536 | [0x8AA3A4] = 0x39200000, exact | 16.16 → metres |
| `k_MapCells` = 512 | g_game+0x59C4 / +0x59C8, set by `GMap::Init(0x200, 0x200)` in `GGame::Init` (0x54F650+0x2A0); 0x6014C8 and 0x6014F1 are inside `GMap::Init` 0x6014C0 | cells per side |
| `ToFixed(m)` | `fld; fmul [0x8AC400]; __ftol` 0x7A1400 (`Set` 0x603346..0x603367, 258 inline copies) | truncates towards 0 |
| `ToMetres(f)` | `fild; fmul [0x8AA3A4]` (`GetLHPoint` 0x605C40 = `ConvertToLHPoint` 0x6041C0) | a single rounding (above 2^24, a prior `(float)f` would round twice) |
| `ToFixedGUtils(m)` | `fmul 65536 [0x8AC408]; fdiv 10 [0x99A1BC]; __ftol` (0x74D52F, 0x74D595, 0x74D85A … 0x74F3A3; `MapCoords::SetX` of bw1-decomp) | a **different** function: 1464 m gives 9594470, and `ToFixed` gives 9594471 |
| `Quantise(m)` | `ToMetres(ToFixed(m))` | the position as a MapCoords stores it; **not idempotent** (`ToFixed(ToMetres(8090858))` = 8090857, as in the original) |
| `CellOf(f)`, `CellX` / `CellZ`, `Cell`, `CellOf(vec2/vec3)` | `xor; mov si, [ecx+2]` (the **unsigned** high word, `ToMap` 0x603433) | a negative is cell 0xFFFF: outside |
| `SignedCellOf(f)` | `movsx` of `JustMapXZ` (0x5E1950; `ApplyEffectToMapPos` 0x525212) | the signed high word |
| `InBounds(MapCoords / ivec2 / vec3, cells = 512)` | `MapCoords::InBounds` 0x6042C0 = `JustMapXZ::InBounds` 0x5E1860: the unsigned high words `jae` against [g_game+0x59C8] (cx) and [g_game+0x59C4] (cz) (0x6042CB, 0x6042D9) | the 512 map, not the extent of the land; openblack's islands may pass their `GetCellsPerSide()` |
| `CellIndex` | `ToMap` 0x603430 = 0x5E1950: `cx * [+0x59C4] + cz`; the cell is at g_game + 0x59FC + index·8 | −1 instead of NULL |
| `AddCells(MapCoords&, JustMapXZ)` | `operator+=(JustMapXZ)` 0x605470: `add word [ecx+2]`, `add word [ecx+6]` | a 16-bit addition: the fraction is kept |
| `k_Neighbours4` | 0xDA59FC..0xDA5A0B (static initialiser 0x74CA10): (1, 0), (0, 1), (−1, 0), (0, −1) | the spiral's table; also read on its own (fn_00602C70 0x602CC4, `Villager::CalculateNearestFreeDestination` 0x768374, the wall hug 0x770104) |
| `k_Neighbours8` | 0xDA59D8..0xDA59FB (static initialiser 0x74CA60, read by fn_00504143 at 0x50417E): (1, 0), (1, 1), (0, 1), (−1, 1), (−1, 0), (−1, −1), (0, −1), (1, −1), (0, 0) | anticlockwise from +x, then the cell itself; another table |
| `Spiral{dir = 1, count = 1}.Next()` | `GUtils::Spiral` 0x74D7E0: `dec [count]; jne` (0x74D7E9); `++dir; count = dir/2` (cdq/sub/sar, 0x74D7EF..0x74D7F7); then `&table[dir & 3]` (0x74D7FE) | updates first, then reads |
| `SpiralIncrement(MapCoords&, Spiral&, step)` | `GUtils::SpiralIncrement(MapCoords&, long& dir, long& count, float step)` 0x74D810 (only called by `Town::FindClearArea`, at 0x741384) | the spiral's rule (0x74D81B..0x74D82B), then each axis moves `step` metres and is truncated again: `x = ftol((table.x · step + x · 10 · (1/65536)) · 65536 / 10)` (0x74D836..0x74D8A8). `x · 10` [0x99A1BC] then `· 1/65536` [0x8AC41C] is `ToMetres` (a power of 2 does not change the rounding) |
| `CellSpiralSize(r)` | `GetMapCellSpiralSizeFromRadius` 0x74F520: `n = ftol(r · 0.2 [0x8AA3AC])`; `cmp eax, 1; jae` (unsigned: only 0 becomes 1); n² | 4 callers |
| `IncrementSpiralSize(r, step)` | `GetIncrementSpiralSizeFromRadius` 0x74F540: `n = ftol(r · −2 [0x8C7CE0] / step)`; (1 − n)² | 1 caller |
| `FromWorld(vec3)` / `ToWorld(MapCoords)` | `MapCoords(LHPoint)` 0x603160 / `Set` 0x603340 (`altitude = y − GetAltitude(this)` at the truncated position, 0x603371..0x60337C) and `GetLHPoint` 0x605C40 (the sum ground + altitude at 0x605C4D) | the height is given by the island's `GetHeightAt`; without an island it is 0 |
| `FtoL` | `__ftol` 0x7A1400, the branch every SSE2 CPU takes (`HasSSE2` [0xE83A20] set): `fstp qword; cvttsd2si eax, xmm0` (0x7A1414..0x7A141A) | towards 0, and the "integer indefinite" 0x80000000 for a NaN or a value outside the int32 range. The x87 branch (0x7A141F: `fistp qword`, then corrected towards 0, the low 32 bits of the int64) would give another value outside that range; it is not reproduced |

#### The spiral

All the callers reviewed start with `dir = count = 1`: for example `Reaction::SpreadReaction` 0x6E3E51,
`GMagicHealInfo::FindTargets` 0x5FBB9A, the lightning 0x6902A7..0x6902BB, the tornado 0x6D22E9 and `Tree::EndPhysics`
0x74B98A. They process the central cell first and then advance with `+= *Spiral()`. The `InBounds` check is done on
each cell; the radius is not clipped. The steps from the start are (−1, 0), (0, −1), (+1, 0)×2, (0, +1)×2, (−1, 0)×3,
(0, −1)×3, (+1, 0)×4 …

The caller copies its MapCoords and moves it with `operator+=(JustMapXZ)` 0x605470, which adds **16 bits to the high
word**: the fraction does not change and the cell wraps around. So a spiral that starts to the left of the map
(x ∈ (−10, 0), cell 0xFFFF) enters cell 0 with the first +1 step.

Each caller computes its number of cells itself:

- `CellSpiralSize`, in 4 places;
- `ceil(2R/10)²` in the heal (0x5FBB51);
- `max(3, ceil(2R/10))²` at 0x604AF8, and `max(ftol(ceil(2R/10)), 3)²` in `AnyMultiMapFixedNear` (fn_00604C30, with
  `InBounds` at 0x604CC9);
- `ceil((r+20)/10)²` in the explosion (0x67E5F2);
- `4·ceil(R/10)²` in the lightning (0x690276);
- `ftol(ceil(…))²` in the firefly (0x52A6A7);
- `Animal::LookForGrazePos`: `fild; fdiv 10 [0x8AB744]; fld st(0); fmul st(1); __ftol` (0x41A8B3..0x41A8DD), the
  square of the float quotient truncated: radius 35 gives 12 cells, not 9.

#### What is not MapCoords

- The 64×64 grid of fn_005E1890 / `SpellGrid` (`x>>16>>3`).
- The polar spiral of `SpellForest::SpellEvent` 0x725830.
- `fistp(x·0.1)` of `AttemptToAddSoundEvent` 0x6465DF (`sea_cells::RoundedCellOf`).
- `ftol(x·0.1 [0x8AC404])` of `GScript::GetLandHeight` 0x6FB1F0, of `LandAvoid` ([0x8AB22C]) and of the creation of
  `CitadelHeart` (0x8827C7..0x882810, with `jl` / `jg` against 0..0x1FF).
- The distances (`GetDistance` 0x74CCB0, `GetDistanceInMetres` 0x74CD70): see [GUtils distances](#gutils-distances).

#### Readings at the call sites

What the original does at the sites the port was checked against:

- `GMagicHealInfo::FindTargets` 0x5FBB00: the copy 0x5FBB6B, `InBounds` 0x5FBBBA, `ToMap` 0x5FBBCB, `Spiral` 0x5FBCE5,
  `+=` 0x5FBCF1.
- The fire's spread (copy 0x72F5E1, `InBounds` 0x72F60C, `GetDistanceInMetres` 0x72F674, `Spiral` 0x72F6C3, `+=`
  0x72F6D0): the distance is measured between the two MapCoords; since the fraction is the same, the difference is
  whole cells.
- The tornado's search for what it carries off (fn_006D21B0): `Spiral` + `+=` from the tornado's MapCoords (`ToFixed`,
  0x6D228D..0x6D22A7), `InBounds` on each cell in the loop itself (0x6D2311), the object's own cell by its MapCoords
  (fn_00604F40), each cell walked fixed then mobile (0x6D2327), `GetDistanceInMetres` from the **initial** MapCoords
  (0x6D2398, not from the cell being walked) and the `CanDestroy` event's position as the MapCoords in metres
  (0x6D23BA..0x6D2419). The tornado's dust takes its cell from `MapCoords(LHPoint)` (0x6D2BA3), without a check against
  the island's side.
- The water miracle (copy 0x7250A2, `Spiral` 0x725166, `+=` 0x725173; 9 cells, `ebp = 9`).
- `Living::CalcRandomPos` (the starting point with `ToFixedGUtils` 0x5ED0FE..0x5ED152, `+=` 0x5ED1C8),
  `Animal::LookForGrazePos` (`+=` 0x41A945), the merging of flocks (`+=` 0x41A76A) and the predators' prey search
  fn_00419490 (`+=` 0x41954B).
- The lightning and the explosion walk their cells from the origin's MapCoords (`ToFixed`, 0x69024A / 0x6908B7 /
  0x67E56A). The lightning measures from the object's MapCoords in metres (`fild · 10/65536`) with its sums in the x87
  order (cone 0x690410, circle 0x690A06, renew 0x6913BD without a root, branch length 0x6923A0); its tip is the
  MapCoords plus the object's height (fn_00691E00, vt+0x42C); the ground of its land points is that of the MapCoords
  fn_004427B0 (= `ToFixed`: 65536·0.1f is exactly 6553.6f); its cooldown is `ftol(AverageLightmapLife / ([0xD01A38] ·
  0.001))` (0x691072, 0x6928FD), passed straight to `PSysRand` (0x691091 / 0x69292F) without a sign check.
  `PSysManager::PSysRand` 0x6729E0 calls through [0xD4E0BC], which fn_00673340 sets to 0x672AF0 (`GameRand(n, file,
  0x400)` 0x6DE510, synced network effects) or 0x672B40 (`GRand::LocalRand(n)` 0x6DE570) by the effect's network
  type, and to 0x6729A0 (`return 0`) at start-up and reset (0x672A8A, 0x6734A5). Both draws end in `_LHRand` 0x7DB600
  with an **unsigned** modulo (`div`): n = 0 gives 0 without a draw (0x6DE574), and a negative n draws and returns the
  seed modulo 2^32 − |n| (oracle: n = −1, −5, −1000 all return the raw seed) [V]. Before
  striking, it asks `IsAvailable` (vt+0x2C, 0x69038E / 0x690997).
- The explosion searches with `GetDistanceInMetres` (0x74CD70) against the 2D radius (vt+0x64); its ring uses the
  radius (vt+0x60) and `(dz² + dy²) + dx²` (0x67EA1C); the targets are the MapCoords as a point (0x67E9E1); the turn is
  g_game +0x205A40; the smoke lasts `NumGameTicksPerSecond` · 4 turns, written as an **inline copy** at
  0x67EEF8..0x67EF2C (`div [0xD01A38]; fild qword; fmul 4; ftol; and 0xFFFF`), not a call to 0x711630, and its visual
  is made with `CreateSpotVisualWithSpecifiedDuration` 0x63E580 (60 and that count of turns).
- `SpellSeed`'s cell and offset (fn_006022C0) come from `ToFixed` (0x6022E2..0x602300); `TopOfObjectsUnder` 0x602388;
  the hand's seed circle is `ftol(x·6553.6)`, `ftol(z·6553.6)`, altitude 0 (0x5D33DD..0x5D3400); the forest miracle's
  position is `ToMetres(ToFixed(p))` (0x725943..0x72595C).
- `MagicTeleport` (`DoTeleport` 0x5FC818 / 0x5FC826 through the twin 0x74CD50, fn_00604C30 at 0x604CFD, fn_0064D6B0).
- `magic::ToMap` and the hand's cast position (0x72056B..0x720595) truncate to 16.16; an abode's door (0x63AFF2).
- `ApplyEffectToMapPos`: corners with `ToFixedGUtils(ToMetres(x) ∓ r)`, signed high words and `InBounds` on each cell
  (0x52514F..0x525259).
- A mobile outside the map does not enter the grid (`ToMap` gives NULL).
- `SpreadReaction`: the size is `CellSpiralSize` (0x74F520, with its unsigned `jae`) and the position advances with
  `+=` on the MapCoords (0x6E3F6E).
- The congregation's average position (0x7409F3..0x740A1E) does `fild qword` (exact) and `fdiv` at 24 bits, so the
  quotient is rounded once to float before the `__ftol`: with 100 positions it goes above 2^24 and the truncation can
  come out one unit different.
- `GClimate`: neither `ProcessAll` (0x771DA0) nor `FindWhereToCreateStorm` (0x772D3E, 0x772D6F) reads the high word:
  both build the LHPoint with `fild; fmul [0x8AA3A4]`. The centre cell in `FindWhereToCreateStorm` (0x772C38,
  0x772C65) is the **unsigned** high word (`xor eax, eax; mov ax, [ebp+0x16]`), added with `fiadd`. The two distances
  of ProcessClimate fn_00772330 use the unsigned high word × 10 (0x7724A6..0x7724DE and 0x772510..0x772548).
  `FindWhereToCreateStorm` compares with the radius of the climate that creates the storm (`fcomp [ebp+0x24]` at
  0x772D9C, with `ebp = this`); `ProcessAll` (0x771DBF) compares with `[esi+0x24]`, the other climate's: the
  difference between the two routines is the original's.
- `SpellFlock`: `DestinationAt` (0x7238E2..0x723905) reads the unsigned high word and computes in float;
  `IsPosOnCorridor` (0x420E67) likewise.
- `MobileWallHug::SetSpeed` 0x60FC50 stores the u16 as it is in +0x5A; the original never converts that speed to
  metres (it adds it to a MapCoords).

### GUtils distances

The original's family of distances (the `Utils` unit, 0x74CCA0..0x74F780, plus two `MapCoords` functions): about
**450 direct calls** in `runblack.exe`. It sits on top of MapCoords. All the good distances are **2D (x, z)**: y is
never used. The whole family is float, without double and without FMA.

The weather's weighting does not call it: `GClimate::ComputeWeather` 0x771640 inlines the distance (dx, dz at
0x7717D7..0x7717E1, `dz·dz + dx·dx` stored as a float 0x7717E5..0x7717EF), skips when `d2 > outer²`
(0x7717F7..0x77180B), gives 1.0 when `d2 <= inner²` (0x771811..0x771827), else `(sqrt(d2) − inner) / (outer − inner)`
(0x77182F..0x77183E). The one `GetDistance` 0x74CDE0 call in the climate code (0x771DBA) is in `ProcessAll`'s debug
overlay, followed by `GDebug::SetMessage` 0x511D80 calls (0x771DD0..0x771E3D) [V].

#### The table-based inverse root

`InvSqrt` 0x74F620 reads a 1024-entry table at 0xDA5A10 that is filled once (fn_0074F590, guarded by the flag
[0xDA6A10]; `GUtils::SetupUtils` 0x74CCA0 is only a `jmp` to it, and `GGame::InitOneTimeOnly` calls it at 0x54F07B,
right **after** setting the FPU to 24 bits). Entry i keeps the top 10 bits of the mantissa (`& 0x7FE000`) of 1/√f, with
f the float `(0x3F800000 & 0xFF003FFF) | ((i & 0x3FF) << 14)` = `0x3F000000 | i << 14` (0x74F5AE..0x74F5CC), so f is in
[0.5, 1) for i < 512 and in [1, 2) above; an exact 1 is stored as 0x7FE000 (0x74F5E0..0x74F5ED).

The root's error is from **−0.097 % to +0.092 %** (full sweep). Visible consequences: a 10 m cell measures 10.0049 m,
100 m measure 100.0244 m and 400 m measure 400.0977 m. `1/InvSqrt(1)` gives 0.99951171875 (0x3F7FE000), not 1.

#### The functions

| openblack (`openblack::gutils`) | Original | Notes |
|---|---|---|
| `InvSqrtTable` | fn_0074F590, table 0xDA5A10, flag [0xDA6A10] | above |
| `InvSqrt(x)` | `_FUN_0074f620` 0x74F620 | `exp = ((0xBE000000 − (bits & 0x7F800000)) >> 1) & 0x7F800000` (0x74F626..0x74F645), mantissa from the table at `(bits >> 14) & 0x3FF`; the sign is not checked, and x = 0 gives ≈ 2^63 |
| `Hypotenuse(int32, int32)` | `hypotenuse` 0x74F680 | 16.16 in and out: `x = dx·2^-16` [0x99A1D4], `s = float(z·z + x·x)` (0x74F695..0x74F69F, stored as a float at 0x74F69F), `ftol(65536.0 [0x99A1D8] / InvSqrt(s))`. It **truncates** and has no cutoff at zero |
| `Hypotenuse(float, float)`, `k_HypotenuseEpsilon` | `hypotenuse` 0x74F6C0 | 0 if \|a\| and \|b\| are both within 1e-4 ([0x8BF518] = 0x38D1B717; `fabs; fcomp; test ah, 0x41` on both sides, 0x74F6C0..0x74F6EC; C0 \| C3 is also what an unordered compare sets, so a NaN side counts as within 1e-4); otherwise `1 / InvSqrt(float(a·a + b·b))` [0x8AA390] (the sum stored as a float, 0x74F6ED..0x74F700). It does **not** truncate |
| `ConvertWholeDistanceToMeters(i)` | 0x74DCC0 | `fld 10 [0x99A1BC]; fmul 2^-16 [0x8AC41C]; fimul i`: the integer is exact before rounding (= `map_coords::ToMetres`) |
| `ConvertMetersToWholeDistance(m)` | 0x74DCE0 | `ftol(m / 10 · 65536 [0x8AC408])`; 65536 is a power of two, so it is `ToFixedGUtils` |
| `GetDistance(MapCoords, MapCoords)` | 0x74CCB0 = its byte-for-byte twin 0x74CCE0 | `Hypotenuse(b.x − a.x, b.z − a.z)` (0x74CCC3..0x74CCC9) |
| `GetDistanceToCell(MapCoords, JustMapXZ)` | fn_0074CD10 (0x74CD20..0x74CD43), with fn_0074E2D0 = `(short(cell) << 16) + 0x8000` | to the **centre** of the cell |
| `GetDistanceInMetres(...)` | 0x74CD70 = 0x74CD50 = `MapCoords::GetDistanceInMetres` 0x605CD0 | `ConvertWholeDistanceToMeters(GetDistance(a, b))`. **383 calls** (202 + 62 + 119). Overloads: MapCoords, `vec3` / `vec2` in metres (truncated to 16.16 like 0x603160) and `ivec2` in 16.16 |
| `GetDistanceInMetresToCell` | fn_0074CD90 | |
| `GetDistance(vec3, vec3)` | `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 | the x / z differences stored as floats (0x74CDE8..0x74CDFA; LHPoint keeps x at +0 and z at +8), then `Hypotenuse(float, float)`: metres, without going through MapCoords |
| `GetMetresDistanceSq` | `MapCoords::GetMetresDistanceSq` 0x605FB0 (0x605FB6..0x605FE5) | the **exact** square in float (mx stored as a float at 0x605FC2, mz kept in st0, the same at 24 bits): no table, so it is **not** `GetDistanceInMetres` squared (100 × 100 m gives 20000, not 20018) |
| `FastDistance` | `GUtils::FastDistance` 0x74CE10 | `max + (min >> 1)` (`sar`) in MapCoords units: not a Euclidean length |
| `ChebyshevDistance` | fn_0074CED0 (with the `abs` fn_0074DD00) | `max(\|dx\|, \|dz\|)`, compared **unsigned** (0x74CF05); one caller |
| `k_Sigmoid`, `detail::k_SigmoidBits` | table 0xC23284, 41 floats in .data (only 0x74F203 reads it; nothing writes it) | **the bits** are copied; T[0] is exactly 0 and T[37..40] exactly 1. It is a logistic `1/(1+e^(−1.0232·(i−20)))` (🔍 inferred: by fitting) |
| `SigmoidThreshold(a, b)` | `GUtils::SigmoidThreshold` 0x74F170 | `a == 1 → 0` (0x74F174; a NaN in a too); otherwise `T[min(ftol((clamp(clamp(b) − a) + 1)·20.5 [0x99A1D0]), 40)]`, with the subtraction `fsub [esp + 4]` at 0x74F1B8. **The threshold is the FIRST argument** |
| `SigmoidClamp` | 0x74F18C..0x74F1B8 and 0x74F1BC..0x74F1E8 | `fcom -1 [0x8AB678]; test ah, 1` takes −1 when the value is below (and when the compare is unordered, so a NaN becomes −1), then `fcom 1 [0x8AA390]; test ah, 0x41` keeps it while it is below or equal to 1 |
| `GetDistanceModifier(d, max)` | 0x74F290 = its copy fn_005ECA20 (`ret 8`) | `SigmoidThreshold(0.5 [push 0x3F000000], 1 − min(d, max)/max)`: the distance is kept only while strictly below the maximum (`fcomp; test ah, 1`, 0x74F290..0x74F2A9); `fdiv; fsubr 1; fstp dword` at 0x74F2B4. It **decreases** with distance, from T[30] = 0.99996 at d = 0 to T[10] = 3.6e-5 at d ≥ max (21 of the 41 steps). With max = 0, `0/0` gives NaN and the result is T[0] = 0. `Tree::ApplyWaterSpell` calls it with max = 3 at 0x74C41D (the immediate 0x40400000 at 0x74C40C); `Villager::ReactToFire` with the immediate 400 (0x43C80000) at 0x765A77 and the distance from 0x765A81 |
| `DistanceChangeToBelief(x, y)` | `GBelief::DistanceChangeToBelief` 0x438770 = its copy fn_00657F30 | `SigmoidThreshold(−0.9 [push 0xBF666666 at 0x43877E], float(−(x/y)))` (`fdiv; fchs; fstp dword` at 0x438775..0x43877B): another curve over the same table |
| `CreatureSigmoidThreshold(a, b)` | `Creature::SigmoidThreshold` 0x4F78C0 | adds `b ≤ 0 → 0` (`fcomp 0; test ah, 0x41` at 0x4F78C4); called from `CreatureDesires::GetIncrementFromSources` |

**Not one routine** (they give different results): the two `hypotenuse`; `GetDistanceInMetres` (quantised to 1/65536
of a cell) and `GetDistance(LHPoint)` (direct float); the distance to a cell (to its centre, +0x8000);
`GetMetresDistanceSq` (no table); `FastDistance` and Chebyshev; `SigmoidThreshold` and the creature's;
`GetDistanceModifier` (a = 0.5) and `DistanceChangeToBelief` (a = −0.9).

**Dead code** (no `call`, reference or pointer): 0x74CDB0, 0x74CE50, 0x74CE80, 0x74F660, 0x74F720 and 0x74F740.
The 1.20 symbols are wrong in two places: 0x74CD50 is named `ReactionInfo::GetInfo` (it is the twin of
`GetDistanceInMetres`) and `hypotenuse` 0x74F680 is typed `void` when it returns an int in eax.

#### The callers read

The distance each caller uses, read in the exe:

- `WorshipScore` fn_0073C590 calls `SigmoidThreshold(0.5, x)`, and multiplies by life³ (0x73C63A..0x73C644: `mov eax,
  2`, and two iterations of `dec eax; fmul life; jne` over st0 = life; the modifier is multiplied at the end, at
  0x73C646). `AdjustWorshipersWorshipping` 0x73C0F0 sorts from the highest score down (0x73C180..0x73C1A6), so **the
  nearest villagers go to pray first** ([The worship percentage](../magic/worship.md#the-worship-percentage-and-the-villagers)).
- The villagers' fire reaction uses `GetDistanceModifier(d, 400)` (above); the threshold that follows is > 0.1
  ([0x8AB22C]). With max = 400 m it gives 0.0444 at 250 m and 0.264 at 220 m.
- The fire's distances, read one by one: `HeatTransfer` fn_0072F980 0x72FA44, `NearestFireToFight` fn_00730070
  0x73010A, `IsBesideFire` fn_0075ABA0 0x75ABC7, `OnFire` 0x75B27B, `ReactToFirePriority` 0x765610 at 0x76567E and
  0x76582B, `ReactToFire` 0x765870 at 0x7658D9 and 0x765A81.
- `MobileWallHug`'s circle hug (0x60D9F0) is based on `GetMetresDistanceSq` 0x605FB0 (0x60D9D0), all float at 24 bits
  even though its two constants are loaded as `qword`.
- The script's nearest town (fn_00552FF0) calls fn_00605CD0 (0x553016, 0x55302E) and compares the distances (not their
  squares).
- The heal (`GMagicHealInfo::FindTargets` 0x5FBB00) has one MapCoords in its frame, the copy of the argument
  ([ebp−0x24], 0x5FBB6B..0x5FBB82), and it is the one that **walks the spiral** (`operator+=` 0x605470 at 0x5FBCF1).
  Both cutoffs measure from it: `GetDistanceInMetres(coords, object)` at 0x5FBBEE `< R`, and the exact square
  0x5FBC54..0x5FBCA3 (`fild [ebp−0x24]` / `[ebp−0x20]`; `(coords − object)²` `< R²`, `test ah, 0x41`). So the heal
  takes any object within R of the **spiral point** that visits its cell, not of the miracle's centre.
- `ApplyReactionToLivingObjectsAtSquare` 0x6E3F90 (an ongoing reaction compared with a new one) takes
  `Reaction::GetPos` 0x6E45C0 (0x6E4142), extracts its **cell** with fn_005E17C0 (the high words, 0x6E414C) and measures
  with fn_0074CD90 (0x6E4157) from the animal's MapCoords to the **centre** of that cell: up to 7.07 m from the
  distance to the initiator. It feeds the score fn_006E4620 (0x6E4173).
- `GStreetLantern::Create` 0x7346E0 walks the cell with `MapCoords::FindType(0x1C)` 0x6045C0 and cuts off with
  `d < 0.5` [0x8AA3B4] (`test ah, 1` at 0x73470C). `GStreetLantern::IsALaternWithinDistance` 0x734A30 is a
  **different** routine (the global list of lanterns g_game+0x205C34 and `d <= r`, `test ah, 0x41`).
- Others read: `GET_DISTANCE` (0x6F8CA0 → 0x74CDE0); `SpreadReaction` (0x6E3E91); the climate's
  `FindWhereToCreateStorm` / `CreateStorm` / fn_00772330 (0x74CDE0; `GClimate::ProcessAll` 0x771DBA calls 0x74CDE0
  once, where not yet read); the wolf's arrival (0x421300); the water miracle (0x7250EC);
  `EffectValues::ApplyEffectToMapPos` (0x525307); `DistanceToForest` 0x53A890 / 0x53AC20 and the forest score
  fn_0053AD00; `PosWithinDomain` 0x5ED010, `SetNewWander` 0x41A3F0, `KeepFlockMemberWithinFlockArea` 0x41ABB0;
  `ReactToFoodPriority` 0x5F1710, `SetupReactToFlyingObject` 0x4204A0, `ProcessReaction` 0x5F1270; the predators'
  fn_00419340; `IsReactionBlockedByShield` 0x72B9B2; `ReactToRainOnFire` fn_0072DCC0 0x72DCE0; the forest miracle's
  fn_005FADF0 0x5FAE30 and fn_007255C0 0x7255CF; the shield's `GetNearestTown` 0x602112 / 0x602193, `IsUnder` 0x72BD3C
  (from the cast position +0xCC) and `FindShieldAt` 0x72BA4B (from the **original** cast position +0xC0, with a strict
  `Get2DRadius > distance`); the pot's `IsCloseToEqual` 0x6053C0, from `Pot::AddResourceToPos` 0x66F375. The citadel's
  nearest town (0x46345C) calls 0x73B170 ([Object lists per cell](object-size-and-map-cells.md#object-lists-per-cell)).
- Not called by any ported code yet: `GetDistanceToCell` in `CreatureMental` 0x4D2B3D. `MobileWallHug::MoveTo`
  0x60AF20 calls `GetMetresDistanceSq` 0x605FB0 (🔍 inferred).

### GUtils angles

The angle family of the `Utils` unit (0x74D0C0..0x74E2D0). It sits on top of MapCoords and the distances.

#### Three kinds of angle

- **Game angle**: an 11-bit integer, **2048 per turn**, stored as a `u16` (MobileWallHug +0x5C). 0 = +x, 0x200 = +z,
  0x400 = −x, 0x600 = −z. It is `atan2(dz, dx)` in 2048ths through the arctangent table: at most **2.27 steps** of
  error (about 0.4°; at 10 m, about 7 cm). `SetGameAngle` 0x60DA90 stores it as it is (0x60DAA1);
  `SetYAngle` 0x60DAC0 goes through `ConvertAngle3DToGame`.
- **3D angle**: radians in float, the same direction, in [0, 2π) when it comes out of GUtils. `Get3DAngleFromXZ` is
  **not** `atan2` in float: it is the game angle quantised and converted to radians.
- **Scawen angle**: the 3D one + π/2 (the creature, PBall, `Dove::Dying`, `GetFacingDirection`).

The game-angle routines are integer (`LHArcTan` does `shl 8; div`). `fsin` / `fcos` come out in extended precision and
are rounded to float only once, by the following `fmul`; that is why `GetPosFromAngle`, `AddDistanceFromAngle` and
`GetPointFromAngle` take the cosine in **double** and round the product once. With `cosf` it is rounded twice: with an
exact model of `fcos`, 38 of 20 000 values (0.19 %) come out 1 MapCoords unit different; with the double, none.

#### The functions

| openblack (`openblack::gutils`) | Original | Notes |
|---|---|---|
| `k_GameAngleMask` = 0x7FF | the mask of `LHArcTan` 0x74D1E2, `ConvertAngle3DToGame` 0x74DC3F and `ConvertGameAngleTo3D` 0x74DC57 | |
| `ArcTanTable()` | table 0xC2307C (257 `u16`, .data) | `trunc(atan(i/256)·1024/π)`; in double with trunc it gives the 257 entries (with round, 122 differ). Only `LHArcTan` reads it |
| `SinTable()`, `Sin(a)`, `Cos(a)` | tables 0xC31614 (2560 `i32`) / 0xC31E14 = SIN + 512 | `trunc(65536·sin(i·2π/2048))`; the 2560 entries match (with round, 1220 differ). The original indexes with `a & 0xFFFF`; openblack with `a & 0x7FF` (the same below 0x800; entries 2048..2559 = 0..511) |
| `LHArcTan(dx, dz)` | `?LHArcTan@@YAXHH@Z` 0x74D0C0 | x = −dx (0x74D0C5); 0 for the origin (0x74D0CB..0x74D0DD); the eight branches (0x74D0ED / 0x74D10E, 0x74D12D / 0x74D153, 0x74D172 / 0x74D18F, 0x74D1AE / 0x74D1C8) with signed comparisons (`jl`) and unsigned divisions; ties go to the first branch; `& 0x7FF` (0x74D1E2). `n << 8` keeps the low 32 bits, like the `shl` |
| `GetAngleFromDXDZ(dx, dz)` | 0x74D200 | `LHArcTan` (0x74D20A) `& 0xFFFF` (0x74D212) |
| `GetAngleFromXZ(from, to)` | 0x74D240 (0x74D240..0x74D259; 0x74D220 with four integers, 3 callers) | overloads MapCoords, `ivec2` (16.16) and `vec2` (metres: each point becomes a MapCoords **before** subtracting) |
| `Get3DAngleFromXZ(from, to)` | 0x74D270 (0x74D289 / 0x74D28F) | `ConvertGameAngleTo3D(GetAngleFromDXDZ(to − from))` |
| `ConvertAngle3DToGame(r)`, `k_Angle3DToGame` | 0x74DC30 | `ftol(r · 325.94931 [0x99A1C8] = 0x43A2F983 = 2048 / 2π) & 0x7FF` (0x74DC34): truncates, and a negative wraps around (−0.5 → 1886). A round trip loses 1 in **365 of 2048** angles |
| `ConvertGameAngleTo3D(a)`, `k_GameAngleTo3D` | 0x74DC50 | `(a & 0x7FF) · 0.0030679617 [0x99A1CC]` (= 0x3B490FDB = 2π (float) / 2048, 0x74DC6C), one rounding; it is bit for bit `float(a) · 2π_f / 2048` |
| `ConvertScawenAngleToGameAngle(r)`, `k_ScawenOffset` | 0x74E290 | `ConvertAngle3DToGame(float(r − π/2 [0x8C78D8] = 0x3FC90FDB))` (`fsub`, `fstp` to a float, 0x74E295 / 0x74E29B) |
| `ConvertGameAngleToScawenAngle(a)`, `k_HalfGameAngleTo3D` | 0x74E2B0 | `float(2a) · 0.0015339808 [0x8C78DC] (= 0x3AC90FDB = π (float) / 2048, 0x74E2C3) + π/2` (0x74E2C9), **without** `& 0x7FF` |
| `GetXByAngle` / `GetZByAngle(a, int d)` | fn_0074D320 / 0x74D340 | `(C·d) >> 16` with a 32-bit `imul` and `sar` |
| `GetXByAngle` / `GetZByAngle(a, float d)` | fn_0074D360 / 0x74D380 | `float(C) · d · 2^-16` |
| `StepFromAngle(a, whole)` | fn_0074D3A0 / fn_0074D3C0 | `((whole >> 4)·C) >> 12` and the same with SIN, both `sar` (`sar ecx, 4; imul; sar eax, 0xC`, 0x74D3B4..0x74D3BA; `whole` is signed). The MobileWallHug step (`InitStepsXZ` 0x60BFD2, inline) |
| `StepFromAngleCoarse(a, whole)` | fn_0074D3E0 / 0x74D400 | `((whole >> 8)·C) >> 8` (`sar ecx, 8; imul; sar eax, 8`, 0x74D3F4..0x74D3FA) |
| `GetX/ZByAngleMetersDistance(a, m)` | 0x74D420 / 0x74D450 | `ftol(float(C) · float(m / 10))` |
| `GetPosFromGameAngle(a, int whole)` | fn_0074D650 (0x74D668 / 0x74D671) | `{StepFromAngle(a, whole), 0}` |
| `GetPosFromGameAngle(a, float m)` | fn_0074D6A0 | the same with `whole = ConvertMetersToWholeDistance(m)`; the `sar 4` discards the 4 low bits |
| `GetPosFromAngle(r, m)` | `GUtils::GetPosFromAngle(float a, float m)` 0x74D580 (60 callers) | `x = ftol(float(cos(r)·m) · 65536 [0x8AC408] / 10 [0x99A1BC])` (`fcos; fmul m; fmul; fdiv; __ftol`, 0x74D58F..0x74D5A1), z with sin, altitude 0 (the literal of `mov [esp+8], 0` 0x74D587, copied out at 0x74D60E..0x74D612). The `GetDistanceInMetres(origin, p)` it calls on the way (0x74D5F4) is thrown away, with its temporary origin `{ftol(0 / 10), ftol(0 / 10), 0}` (0x74D5C5..0x74D5EC) |
| `AddDistanceFromAngle(p, r, m)` | 0x74D510 | `p.x = ftol((float(cos(r)·m) + ToMetres(p.x)) · 65536 / 10)` (`fcos; fmul m; fild p.x; fmul 10; fmul 2^-16; faddp; fmul 65536; fdiv 10; __ftol`, 0x74D510..0x74D53B), then z with `fsin` (0x74D540..0x74D569); the altitude does not change |
| `GetPointFromAngle(r, m)` | fn_0074D620 (0x74D628..0x74D643) | `(cos(r)·m, 0, sin(r)·m)` in float |
| `GetAngleDifference(a, b)` (🔍 inferred name) | fn_0074D740 | `d = \|a − b\|` (`sub; cdq; xor; sub`); `d > 0x400 ? 0x800 − d : d` (`cmp 0x400; jbe`, 0x74D748..0x74D757) |
| `GetAngleSign(from, to)` (🔍 inferred name) | fn_0074D6F0 | `d = to − from` (0x74D6F0..0x74D6F4); 0 → 0; if `\|d\| > 0x400` (unsigned, `cmp 0x400; jbe` 0x74D728 / 0x74D709) it wraps around by ±0x800 (0x74D711 / 0x74D722); −1 if d < 0, otherwise +1. **With \|d\| == 0x400 it does not wrap**: +0x400 → +1, −0x400 → −1 |

`MapCoords` also has `operator+` 0x605520, `operator-` 0x6055C0, `+=` 0x605410 and `-=` 0x6054A0: they add or subtract
x, z **and the altitude**.

#### Points around an object

They are all `this + GetPosFromAngle(angle, r)` with `MapCoords::operator+`, so they keep **this's altitude**. Each has
its own radius, and they are not interchangeable (G3D = `Get3DAngleFromXZ`, R2D = the 2D radius vt +0x64, R = the
radius vt +0x60):

| openblack (`ecs::object`) | Original | Angle and radius |
|---|---|---|
| `MapCoordsOf(e)` | Object +0x14 | `map_coords::FromWorld` of its `Transform` |
| `GetNearestPosOfObject(this, o)` | 0x636D30 | `G3D(this, o)`, `R2D(o) + R2D(this)` |
| `GetNearestEdgeToPos(this, p)` | 0x636DA0 | `G3D(this, p)`, `R2D(this)` |
| `GetNearestEdge(this, angle, extra)` | 0x636DF0 | the angle is given by the caller; `R2D(this) + extra` |
| `GetWorkingPos(this, o)` | 0x639550 | `G3D(this, o)`, `R(this) + R(o)` |
| `TreeGetWorkingPos(tree, o)` | `Tree::GetWorkingPos` 0x74C040 | `G3D(tree, o)`, `R2D(o) + 0.9` [0x8C5844]: only the other's radius |
| `BigForestGetArrivePos(forest, v)` | `BigForest::GetArrivePos` 0x439360 | `G3D(forest, v)`, `R(forest) · 0.5` [0x8AA3B4] |

#### Readings at the call sites

- `SetTowardsAngle` (0x418560) turns with `GetAngleSign` and `GetAngleDifference`; at exactly +0x400 it turns
  positive (`jbe` at 0x74D709).
- `GetFireFightingPos` 0x75AAE2 / 0x75AB59 and the flight of `OnFire` 0x75B368 use `Get3DAngleFromXZ` +
  `GetPosFromAngle` on MapCoords; `OnFire`'s two `GameFloatRand` are drawn angle first, then distance
  (0x75B32D..0x75B34A).
- Trees: fn_0053A010 0x53A094, `Tree::GetWorkingPos`, the forest edge of fn_0053ADB0 (= `GetNearestEdgeToPos`),
  `BigForest::GetArrivePos` and `AddTreeAround` 0x439264. The tree and the forest keep their altitude
  (`operator+`).
- `WorshipSite::GetSpellIconPosFromSlot` 0x77AFC0 sets the altitude to 0 with ring > 0 (0x77B002, `mov [esp+0x14], 0`
  = MapCoords +8) before the `+=`: those icons end up **on the ground**; ring 0 keeps the special point's height. The
  step between rings is 15 (0x77B100).
- `Rock::SplitInTwo` 0x6E75B1: `pos + o` and `pos − o` (0x6E76A9 / 0x6E76CE) on this +0x14: the two halves **keep the
  rock's altitude**.
- `IsPosValidForTurnAngle` (0x41B210): the centres of the two turning circles are `me + fn_0074D6A0(a ± 0x200, R)` in
  MapCoords, with the `sar 4` that discards R's 4 low bits, and the distance is `GetDistanceInMetres` 0x74CD70 (with the
  table); R is converted to metres with `ConvertWholeDistanceToMeters` (× 10 / 65536). There is no turn test: with a
  turn angle of 0 the quotient is inf (NaN without speed), `__ftol` gives 0x80000000, R = −327680 m and both distances
  exceed it (true).
- `Living::CalcRandomPos` (0x5ED0DB..0x5ED152): the random offset is `AddDistanceFromAngle` on the centre's MapCoords,
  inlined: x and z go to metres (`fild`, × 10 `[0x92B400]`, × 1/65536 [0x8AC41C]), cos(a)·r is added and the sum goes
  back to 16.16 with GUtils' × 65536 [0x8AC408] / 10 and `__ftol`.
  The final `me + fn_0074D650(+0x5C, 10)` is `me + (0, 0)`, because `10 >> 4 = 0`. The two random numbers are
  `GameFloatRand` (0x5ED0BE the angle, 0x5ED0D2 the radius, the latter **always**); `GameFloatRand` 0x6DE530 /
  fn_005106B0 gives 0 for 0 and otherwise `float(LHRand(0xFFFF)) · max · 1/65535` ([0x8D6050] = 0x37800080), also with a
  negative `max` ([Random numbers](random.md#the-synchronised-and-local-grand)). `SquarePos` (fn_0074F310) uses the
  same one.
- The bird formation (fn_0041E890, 0x41E96A..0x41EA05) is not `AddDistanceFromAngle`: x uses the row and z the column,
  and the order is `(cos·row)·10` (`fimul`, then `fmul 10`), two roundings, on the leader's MapCoords.
- `Dove::Dying` (0x41F1B0): the velocity is `(sin(s)·v, 0, −cos(s)·v)` with `s = ConvertGameAngleToScawenAngle`.
- `Animal::SetNewWander(MapCoords const&, int, int)` 0x41A3F0: the distance becomes an integer with `__ftol`
  (0x41A421) and is compared **as an int** with the maximum and the minimum (0x41A426 `cmp; jle`, 0x41A430
  `cmp; jge`).
- The flock's two orientations go through 0x74D240.
- No copy to port: `GScript::CastSpellAtPos` 0x70BDD1 computes the angle and throws it away;
  `Living::GetFleeingPositionFromStationaryObject` 0x5F2010 normalises in float; 0x463670 (the citadel) is dead code.
  Callers not ported yet: `GetXByAngleMetersDistance` (`Creature::GetMovementDirection`, `GetRandomLookAhead`,
  `RunAwayFromObjectReaction`), `StepFromAngleCoarse` (the villager's `Approach*`, PuzzleHorse) and `GetPointFromAngle`
  (`SetupInspectObject`).

**Outside the family.** No callers: 0x74D2A0 (the angle between two LHPoints) and 0x74D770 (turning towards an angle
with a maximum step), with no `call`, `jmp`, `jcc` or pointer in the whole image. 0x74D480 (the octagonal step) is only
called by fn_005E1890, not ported. **Not of this family**: `LH3DMath::GetYAngle` 0x841290 and fn_007FAA50 (LH3D, see
[LH matrices](matrices-and-zoomer.md#lh-matrices)), `Atan2Positive` 0x7DB770 (the gestures) and PuzzleGame's own
conversion 0x6F184C.

### Sources

- The exe's disassembly: 0x603160, 0x603340, 0x603430, 0x6041C0, 0x6042C0, 0x605470, 0x605C40, 0x5E1860, 0x5E1950,
  0x74CA10, 0x74CA60, 0x74D7E0, 0x74D810, 0x74F520, 0x74F540, 0x7A1400, 0x525100..0x525260, 0x63AFF2, 0x7204D0
  (0x72056B..0x720595), 0x882730 (0x8827C7..0x882810) and 0x7DEE00; on a second pass 0x6014C0, 0x54F650+0x2A0,
  0x5FBB40..0x5FBD10, 0x72F5C0..0x72F6E0, 0x725000..0x725180, 0x5ED080, 0x41A8B0, 0x41A640..0x41A780, 0x419490,
  0x60FC50, 0x7238C0, 0x420E10, 0x771BE0, 0x772BE0, 0x74CDE0 and 0x7409C0..0x740A60.
- The distances: 0x74CCA0:1D0, 0x74CE6E:C0, 0x74DCC0:50, 0x74DD00, 0x74E2D0, 0x74F170:A0, 0x74F290:40, 0x74F580:1A0,
  0x74F620, 0x74F680, 0x74F6C0, 0x605CD0, 0x605FB0, 0x5ECA20, 0x657F30, 0x438770, 0x4F78C0, 0x73C5C0..0x73C64E
  (life³), 0x552FF0, 0x5252E0, 0x60D9D0 and 0x6E3E60; the bytes of 0xC23284 (164 B), 0x99A1D0, 0x99A1D4, 0x99A1D8,
  0x99A1BC, 0x8AC408, 0x8AC41C, 0x8AC400, 0x8AA390, 0x8AA3A4, 0x8AB678, 0x8AB680, 0x8AB41C, 0x8BF518 and 0x930670.
- The angles: 0x74D0C0:120, 0x74D200:A0, 0x74D320:180, 0x74D510:180, 0x74D6F0:80, 0x74DC30:50, 0x74E290:40,
  0x605410, 0x6054A0, 0x605520, 0x6055C0, 0x636D30:110, 0x639550:60, 0x74C040:70, 0x439240:50, 0x439360:70,
  0x53A060:70, 0x6E7560:200, 0x77AFC0:C0, 0x75AA90:F0, 0x75B320:80, 0x41B210:140, 0x418CD0:60, 0x41E930:F0 and
  0x41F1B0:80; the tables 0xC2307C and 0xC31614 dumped from the exe.
- bw1-decomp: `src/Black/MapCoords.h` (`MapCoords.h:137`), `Map.h`, `Utils.h` (signatures only; `GetDistance` appears
  as `void`), `Lionhead/LH3DLib/development/LH3DMapCoords.h` and `LH3DMath.h:33`.
- The other modules the old shared page described keep their sources on their own pages:
  [the game clock](game-clock.md#game-clock), [object size](object-size-and-map-cells.md#object-size) and
  [matrices and the Zoomer](matrices-and-zoomer.md#lh-matrices). Their source lists, as they stood: the game clock
  0x54C4A0, 0x54C570, 0x54CC30 (0x54CD0F..0x54CD58), 0x54D2A8..0x54D3D3, 0x54AE60:90, 0x5557E0:60, 0x555820,
  0x82F14E:50, 0x711630:30, 0x711610, 0x711280:F0, 0x70CC30:140, 0x66CD00, 0x66CD30:B0, 0x5C6250:50, 0x714DB0 and, in
  the audit, 0x818C60:60, 0x8189F0:190, 0x54CD93, 0x54D338:50, 0x879B0A:40, 0x54E5C0, 0x52AF90, 0x5DBD4E, 0x5C61B0:D0,
  0x5C68C0:E0, 0x5537F0:E0, 0x634B40, 0x76EAF0 and 0x54E763 (bw1-decomp `src/Black/Game.cpp`: `PauseGame`,
  `SetSpeed`, `LocalTimerSaysDoATurn`, `ResetLocalGameTimer`, `ProcessNetworkPackets`, `Loop` l. 1834-1996,
  `ResolveLoad`); the speed 0x5537F0:E0, 0x63F830:D0, 0x5CBF10:120, 0x5DACC0:30, 0x6FAA60:30, 0x6FAAE0:50,
  0x54C190:150, 0x558BF0:140, 0x634A70:60, the callers of `SetSpeed` and the references to g+0x205B74, g+0x205D68 and
  its factor (+0x205E70, +0x205E74) in the whole `.text`; object size 0x638110:E0, 0x8082C0:C0, 0x66EB60:C0,
  0x66F180, 0x66F1B0:80, 0x477F40, 0x47B190, 0x4EF4F0, 0x638C00, 0x74B610, 0x5110E0, 0x728640, 0x639AC0, 0x510E10,
  0x77DE10, 0x77DDD0, 0x4026B0, 0x74A1A0, 0x74A140, 0x6384C0, 0x637730, 0x637FB0, 0x5702B0, 0x4027C0, 0x637E00,
  0x636D30, and the migrated places 0x53A740, 0x5116A0, 0x74AB80, 0x74B12F, 0x439220, 0x639550, 0x66E900, 0x722B30,
  0x403270, 0x6A0D51 and 0x5287A0 (bw1-decomp `src/Black/Object.cpp:1062-1104`); matrices, Zoomer and normal
  0x7FAC10:B0, 0x5198F0:70, 0x86AFA0:70, 0x674200:160, 0x674360:50, 0x6743E0:60, 0x6A1150:D0, 0x6A1218, 0x6A12C7,
  0x6A12F0, 0x6A6250:70, 0x7FB180:110, 0x7FB290:160, 0x423140:250, 0x407D60:170, 0x442720:90, 0x441F80:C0,
  0x5B4200:180, 0x803630:260, 0x803890:F0 and 0x516AA0:50, with the constants 0x9A2BEC, 0x9A2BE8, 0xC3720C, 0x8AB41C,
  0xC371D4, 0x8AA3B0, 0x8AB26C, 0x8AB268 and 0x8AB414.

## openblack

**Done**: all three families are ported and faithful.

- `src/3D/MapCoords.{h,cpp}`, namespace `openblack::map_coords` (2026-10-01).
- `src/Common/GUtilsDistance.{h,cpp}`, namespace `openblack::gutils` (2026-10-01).
- `src/Common/GUtilsAngle.{h,cpp}`, namespace `openblack::gutils` (2026-10-02).
- The points around an object in `ecs::object` (`ObjectMetrics.h`).
- `map_coords::FtoL` is the single `__ftol`, used by `ToFixed`, `ToFixedGUtils`, `CellSpiralSize`,
  `IncrementSpiralSize`, `ConvertMetersToWholeDistance` and the game angle.

These are pure modules with no global state, reached through their namespaces; constants are `constexpr` with the
original's bits (`std::bit_cast`) where they matter; the coding rules are in
[Refactor conventions](../project/refactor-conventions.md#coding-conventions-summary) and the layout in
[openblack's internals](../project/internals.md).

### What uses the API

- **MapCoords.** `CellOf` is the one cell rule; `sea_cells::CellOf/InBounds` and `MapInterface::GetGridCell` are wrappers (`GetGridCell` returns the
  unsigned high word, 0xFFFF, for a negative instead of undefined behaviour). Migrated: the animals (AnimalAI and
  AnimalAIDetail, AnimalLairs, AnimalPredators, AnimalWallHug), the villagers and towns (VillagerSpeed, `TownQueries` with its conversions, sizes, `SpiralIncrement` and the spiral of `CheckForClearArea`, AbodeQueries, StreetLantern,
  WorshipSite), the map and objects (WaterQueries, PotResource, MapProduction, MapCollide, Trees, MobileWalkPaths),
  the effects and magic (Reactions, EffectValues, FireEffect, CastRules, `magic::ToMap`, `Spell::castPos`,
  SpellFlock, SpellWater, SpellSeed (its `CellOffset`), HandSpellSeed, SpellForest's `ToMapCoords`, MagicTeleport's
  `AnyMultiMapFixedNear`), the weather (Climate, WeatherLand), the particles (PSys `Flock`, the tornado's search in
  `Storm.cpp`, `Lightning.cpp`, `Explosion.cpp`) and the audio (`ThingMusic.cpp`'s round trip). **Every spiral now
  advances on a MapCoords** with `AddCells` (`detail::Spiral::Advance` wraps it), not on a cell in an `int` nor on
  metres in float.
- **Distances.** The three copies of the 1/√ table and of `InvSqrt` were deleted (`WaterQueries.cpp`,
  `AnimalLairs.cpp`, `CHLApi.cpp`), with the four `hypotenuse` and the three private `SigmoidThreshold` (AnimalLairs,
  Trees, WorshipPercentage). Users: `WaterQueries` (`DistanceInMetres`, the `NearestCoastal` cutoff, the distance to
  the river points), `AnimalLairs` (`MapDistance`, `ForestScore`), `CHLApi` `GET_DISTANCE`,
  `TownQueries::GetDistanceInMetres` (and with it `VillagerDecide` and the town search radii). Float substitutes were
  replaced **only where the original's call was read** (the list under [The callers read](#the-callers-read)):
  `FireEffect` and `VillagerFire` (`Distance2D`), `Reactions`, `Climate`, `CastRules`, `SpellFlock::WolfArrived`,
  `SpellWater::ApplyWaterSpell`, `EffectValues`, `Trees`, `AnimalAI`, `AnimalFlee` (`AnimalReaction` with
  `GetDistanceInMetresToCell`), `AnimalPredators`, `AnimalWallHug`, `StreetLantern`,
  `FeatureScriptCommands::FindNearestTown`, `MagicTeleport` (`Distance2D` and `FastDistance`), `MapShield`, `Storm`,
  `SpellStormAndTornado`, `SpellForest`, `SpellShield` and `ECS/PotResource.cpp`. `AnimalPredators`' `FindPrey` and `AnimalAI`'s `CalcRandomPos` and `LookForFoodPos` walk their spirals on MapCoords. With the table root the distances are
  no longer exact (100 m give 100.02 m; `test_teleport` checks it that way).
- **Angles.** `town_queries::GetAngleFromXZ` / `Get3DAngleFromXZ` / `GetPosFromAngle` and
  `animal_ai::detail::Cos` / `Sin` / `Step` / `AngleOfMapCoords` are one-line forwards to `gutils` (the copies of the
  tables and of `LHArcTan` were deleted from `AnimalAI.cpp`). `AngleOf(vec2)` and `AngleDiff` no longer exist.
  `VillagerCore.cpp`'s `setGameAngle` uses `gutils::ConvertGameAngleTo3D`; `SpellFlock.cpp` uses
  `gutils::GetAngleFromXZ(created, target)`.
- `GetNearestPosOfObject` is ported; nobody calls it yet in openblack. Nor `ChebyshevDistance`,
  `CreatureSigmoidThreshold`, the four-integer `GetAngleFromXZ` and the other unported callers above.

### What the port had wrong

Fixed when the modules were ported, each against the reading above:

- **MapCoords.** Double products changed to float (WaterQueries' `k_WorldToFixed` and the `x·65536.0·0.1f` of
  `FindNearestStreamPos`; TownQueries, which used the double 6553.6; AbodeQueries' door, SpellFlock and PSys
  `Flock`). Climate's return conversion used 0.000152588f = 0x39200008 instead of 0x39200000 (3.9 mm at 5 km), and its
  signed `CellCentre()` is gone (`Climate::CellCentre` stays only for ProcessClimate's two distances; the other readers use `Centre()`). `InBounds` by the land's extent with
  a strict `>` (AnimalAI, Reactions) became the 512 map: x = 0 and the cells without a block now count. `floor` instead
  of truncating (FireEffect, CastRules) differed for x ∈ (−1.5e−4, 0). `GetGridCell(pos − radius)` with negatives
  (undefined behaviour) in `ApplyEffectToMapPos` and `MapProduction`. `magic::ToMap` and the cast position now
  truncate, and `ToWorld` does not truncate again (a second round trip can lose a unit). `SpreadReaction` added 10 m in
  float. `MobileWalkPaths` rounded twice above 2^24 units. A spiral on a cell in an `int` reached 0x10000 from cell
  0xFFFF and stayed outside forever. `TruncateToInt` (was `Ftol`) said "x87 extended" but receives a float. The
  citadel's `x·0.1` was already right (a planned fix was a false positive). The tornado's `SpiralStep`, MagicTeleport's
  `k_UnitsPerMetre` and `ToUnits`, and Storm's `PotsByCell` were deleted.
- **Distances.**
  1. `WorshipScore` had the arguments of `SigmoidThreshold` reversed (`(x, 0.5)`): the curve came out mirrored and
     **the farthest villagers went to pray first** (the error was repeated in `WorshipPercentage.h`, `test_worship.cpp`
     and the magic notes).
  2. `WorshipScore` multiplied by life², not life³.
  3. `VillagerFire::DistanceModifier` was an invented `smoothstep` (0.156 at 250 m and 0.352 at 220 m, and saturating
     to 1 / 0).
  4. The `Trees.cpp` sigmoid table was rounded to 4 decimals (35 of 41 entries different; T[1..10] were 0 and T[30..36]
     were 1), and 5. the `WorshipPercentage.cpp` one to 5 decimals (T[1..8] = 0, T[32..36] = 1).
  6. The `AnimalLairs.cpp` sigmoid was computed in double (a different index in 50-60 of 3321 cases at the table's
     jumps, and 1 in 300 000 with random values).
  7. `hypotenuse(int)` in double (WaterQueries, AnimalLairs) differed in 31 678 of 200 000 samples, up to 2431 units =
     **0.37 m at 1 km**.
  8. `TownQueries::GetDistanceInMetres` used `std::hypot` without the table and converted the integer to float before
     multiplying (different in 22 572 of 100 000 integers above 2^24, more than 2560 m).
  9. `AnimalWallHug` `MoveToCircleHug` did the root and the ×128 − 1 in double, from the wrong base.
  10. `FeatureScriptCommands::FindNearestTown` compared squared distances.
  11. `CastRules::FindHealTargets` (`FindHealTargets`, the port of `FindTargets`) measured from the casting point, not the spiral point.
  12. `AnimalFlee` `AnimalReaction` measured to the initiator, not to the centre of the reaction's cell.
  13. `SpellShield`'s `FindShieldAt` measured from the cast position and accepted `<=`.
- **Angles.**
  1. The animals' `AngleDiff` (`((b − a + 1024) & 2047) − 1024`) gave −0x400 at exactly +0x400: an animal facing exactly
     away from its goal turned the other way, and the birds' banking had its sign flipped.
  2. `GetPosFromAngle` used `std::cos(float)` (double rounding); it reaches every caller of
     `town_queries::GetPosFromAngle` (Abode, VillagerDecide, VillagerShield, the congregation).
  3. Unquantised float angles in `VillagerFire`, `Trees`, `WorshipSite`, `AnimalFlee` and `Rock::SplitInTwo` (whose
     halves were put on the ground).
  4. `GetSpellIconPosFromSlot` kept the special point's height for every ring.
  5. `Tree::GetWorkingPos` and `BigForest::GetArrivePos` took the terrain height.
  6. `IsPosValidForTurnAngle` used `glm::distance` and / 6553.6, with a `turn <= 0` branch that was removed (it made no
     difference).
  7. `CalcRandomPos` added in float metres and had a `range > 0` branch; its centre now becomes a MapCoords with `FromMetres`. Its generator is ⚠️ approximate (`LHRand`
     is openblack's generator here, 0..0xFFFE), and its centre arrives in metres, so a centre that was not a MapCoords
     may already be one unit off (`Quantise` is not idempotent).
  8. The bird formation, 9. `Dove::Dying` (the same mathematically, different in bits), 10. the animals'
     `AngleOf(vec2)` (subtracted in metres; nine uses in AnimalAI, AnimalBirds, AnimalFlee, AnimalPredators and
     AnimalWallHug; `AnimalFlee` now uses `Object::GetWorkingPos`), 11. `OnFire`'s two draws as arguments of one call (no guaranteed order), 12. `SetNewWander`
     comparing the float distance (with d in (rMax, rMax + 1) the animal went towards the centre), 13. `static_cast`
     instead of `__ftol` (undefined out of range), and 14. `VillagerCore.cpp`'s hand-written constant.

### Test hooks

The module tests (this page's):

- `test_map_coords` (`test/test_map_coords.cpp`): the constants by bits; `ToFixed`, `ToFixedGUtils` and `ToMetres`
  with the 24-bit values (1464 m → 9594471 against 9594470; 16777217 → 0x45200001); the round trip that loses one
  unit; cells and `InBounds` with negatives; `AddCells` (the fraction is kept, the cell wraps through 0xFFFF and a
  spiral that starts in cell 0xFFFF enters cell 0); the two neighbour tables; the exact sequence of the spiral and the
  4×4 square it covers; `SpiralIncrement` and the two sizes.
- `test_gutils_distance` (`test/test_gutils_distance.cpp`): the 41 dwords of the sigmoid table by bits, and that
  `k_Sigmoid` holds those bits; entries 0, 1, 2, 511, 512 (the 0x7FE000 of the exact 1), 513 and 1023 of the 1/√ table;
  `InvSqrt(1)` = 0x3F7FE000 and the ≈ 2^63 of zero; `Hypotenuse(int)` with 0, one cell (65568), the diagonal (92691)
  and the side of the map (33570824); `Hypotenuse(float)` by bits, with the 1e-4 cutoff on both sides and with a NaN
  side (0, like the unordered compare); the two unit conversions, including the `fimul` above 2^24;
  `GetDistanceInMetres` (100 m → 100.0244; 400 m → 400.098), the distance to the centre of a cell,
  `GetMetresDistanceSq`, `FastDistance` and Chebyshev; `SigmoidThreshold` with the threshold first, the two clamps and
  `a == 1`; `GetDistanceModifier` with the 400 m of the fire reaction, the 3 of the water miracle on trees, and
  `max = 0`; `DistanceChangeToBelief`.
- `test_gutils_angle` (`test/test_gutils_angle.cpp`): the constants by bits; the two tables against the exe dump (sums,
  a weighted sum and single entries) and `COS = SIN + 512`; `LHArcTan` on the axes, the diagonals, one point per
  octant, the overflow of the `shl 8` and the error ≤ 2.27 steps against atan2; the conversions (−0.5 → 1886, the
  NaN, the 365 angles that lose 1 in the round trip, the Scawen angle by bits); `StepFromAngle` with a negative
  `whole` (`sar`), the 32-bit `imul` of `GetXByAngle(int)`, the (0, 0) of `GetPosFromGameAngle(a, 10)` and the 4 lost
  bits of the float overload; `GetPosFromAngle` with two cases where `cosf` gives another unit, and
  `AddDistanceFromAngle`; `GetAngleDifference` and `GetAngleSign` at ±0x400; `MapCoords::operator+` / `operator-`
  with the altitude.
- `test_object_metrics` `PointsAroundAnObject`: the six functions, each with its radius and the altitude of `this`.
- `test_worship` calls the real `worship::percentage::WorshipScore`: life 0.5 at the centre gives 0.5³ · 0.99996, life
  1 beyond d2 gives 3.6e-5 (with the arguments reversed or with life² it fails).

The old shared page listed the other engine modules' tests here too; they belong to those pages:

- `game_random` (`test/test_game_random.cpp`): `SeededRandom` (was `LHRand`) from 0x88F89F and from 0, `GameRand`
  after `Init` and after `Reset`, the floats bit for bit (the order (u·x)·k against (u·k)·x), 0 for 0 / −0 / NaN
  without touching the seed, x < 0, `PSysRand` ≠ floor(`PSysFloatRand`), the PSys outside a step (0, without drawing),
  `RandR3`, the scope that does not nest, the CRT from 1, `Save` / `Load` and the hooks.
  `game_random::testing::ScopedState` saves and restores the whole state; `testing::SetGameRand` is the hook that used
  to be in VillagerCore. `OPENBLACK_TRACE_GAME_RAND=1` gives the original's trace ([0xCD3C80], formats 0xBE899C /
  0xBE89D0: seed, turn and place) through spdlog at trace level. `OPENBLACK_TEST_PSYS_RAND_OUTSIDE=1` counts each PSys
  draw outside an effect's step, logs its place once and the total per place on exit (Land 1 with a storm and a water
  miracle: none). See [Random numbers](random.md#random-numbers).
- The font cache's start-up shuffle (`test/test_game_font_hash.cpp`, `GameFont::MakeHashPermutation` with a fake
  `rand()`): the tables of two hand-computed streams, exactly 255 draws over the ranges 256 down to 2 in order, a
  permutation of 0..255, and the table kept by `resources::Resources` as it was set.
- `test_object_metrics` (`test/test_object_metrics.cpp`), with test mesh boxes
  (`object::detail::SetMeshBoxProviderForTests`): the box fields (half-axes, half-diagonal), `Radius2D` and `Height`;
  the mesh level sees no overrides and gives 0 without a mesh; the `Object` base and the 0 without a mesh or without
  `Mesh`; Field and FishFarm = 5 (radius and `GetMeshRadius`; the height is still the mesh's), MagicTeleport = 6,
  MagicFireBall = scale in radius and height, the MapShield's object scale and the creature's height;
  `GetProportionRaised` of food and wood, with the empty pile at 0, and the food pile's radius; `GetHoldRadius`,
  `GetDefaultFireRadius` (dead tree, worship site), `GetVillagerHugRadius` and the tree's `GetRoutePlanRadius`, the
  two distances, `IsTouching`, `GetBoundingSphere` and `GetTopPos`; the derived overrides (`DerivedOverrides`): the
  sphere of Living and MobileStatic, the shield's `GetTopPos`, the hand's height above the fish farm, the
  `CitadelHeart`'s route radius, and the distance and `IsTouching` of the worship site with its `WorshipSiteCentre`.
  `test_food_wood`: `PileFoodProportionRaised(0, 1000)` is 0. See [Object size](object-size-and-map-cells.md#object-size).
- `test_game_clock` (`test/test_game_clock.cpp`) drives the clock with a test `GetTickCount`
  (`game_clock::SetTickSource`) and 33 ms frames: the first turn in the first frame, with the turn already incremented,
  `visual = 100` and dt 100; 3 s at 33 ms give 30 turns (not 22), in frames 4, 7, 10 … with gaps of 3 or 4; the
  remainder, the fraction and the dt frame by frame (33, 0.33 … 0.99 and, with turn 2, 32); the cap of 99 on the
  remainder, the dt of 199 and a single turn per frame; a lag of more than 2 s is dropped and the next turn waits for
  `turn · 100`; the pause (no turn, dt 0, the fraction frozen, and the stopped time does not count when unpausing); the
  speed (the time already elapsed keeps the previous one, the dt follows it, and while paused it is only saved);
  `OnLoad` (`visual = turn · 100`) and `Start` (the next turn is due at once); `TicksForSeconds` (truncates; with
  `SetMsPerTurn(300)`, 1000 / 300 = 3 per second); the wall clock (≥ 1), `EngineMs` and the two selectors (cap of
  500), stopped (0 and 1 ms) until `StartEngineTimer` and then counting from about 0; `EngineTimerAfterLongUptime`
  (with `GetTickCount` = 0xCE000000, 40 days, the engine ms are exact). `OPENBLACK_START_PAUSED=1` starts the game
  paused (`game_clock::Start(true)`). See [Game clock](game-clock.md#game-clock).
- `test_zoomer` (`test/test_zoomer.cpp`): `SetPosition`, the 0.001 s threshold (also NaN and negatives) and the arrival
  at `t ≥ duration` without extrapolating; the step from 0 to 10 in 2.5 s (2.6171875 at T/4) and the determinant floor
  with T = 0.05 / 0.04 / 0.02 s, by bits; start and destination speeds; two recorded frames of the original's camera;
  `Zoomer3`. `test_camera` `ZoomerMatchesRecording`: each zoomer state of the 11 recordings, coefficients and values bit
  for bit; `ValidateRecordedData` loads the recorded zoomers into the `Camera` and uses `Camera::UpdateZoomers`.
- `test_game_matrix` (`test/test_game_matrix.cpp`, was `test_lh_matrix`): where each axis goes with a quarter turn of
  each constructor (the signs); the bit-for-bit equalities that follow from the cells (`RotationYXZ(a, 0, 0) =
  AngleY(a)`, `RotateY(I, a) = AngleY(a)` and `AxisAngle(X, a) = RotationXYZ(a, 0, 0)`); the glm composition of each
  one and the ones that are not (left / right, `eulerAngleXYZ`, the + angles); the floor of `Inverse` (±100 instead of
  1e4); `PlacementMatrix` and `Model`. See [Matrices and the Zoomer](matrices-and-zoomer.md#lh-matrices).
- `test_land_normal` (`test/test_land_normal.cpp`): the two tables by bits; a flat cell (1 + 1 ulp) and a slope; the
  four triangles (by bits and against the plane of the three corners); the quantisation of T2 in a steep cell (length
  0.998). See [Terrain normal](../world/terrain.md#terrain-normal).

Checked in the game:

- Worship (`OPENBLACK_TEST_WORSHIP="1.0.5"` + `OPENBLACK_WORSHIP_TRACE=1`, Land 2): the 11 villagers nearest to the
  site go (232–278 m); with the old bug the farthest went. The tie-break by life³ is not visible (life is not in the
  trace).
- Fire (`OPENBLACK_TEST_FIRE="1785.2.2652.6.450,abode,20"` + `OPENBLACK_FIRE_TRACE=1`, Land 1): the 12 nearby villagers
  react and put it out (215 → 220 ⇄ 216). **Not checked in the game:** the 400 m term with villagers 150–300 m from
  their town (no hook; only `test_gutils_distance`).
- Heal (`OPENBLACK_TEST_HURT_VILLAGERS="1814.0.2660.5.30.0.3.0,200.1,0"`): it heals the cells the spiral walks
  (villagers at 13.3 m with R = 10) and not those it does not visit, like the original.
- Worship site icons (`OPENBLACK_TEST_WORSHIP_SITE="NORSE,0,...,11"`): the outer ring at ground level, the inner one on
  the platform.
- Clock (`OPENBLACK_CLOCK_TRACE=1`, 1356 turns): 10.01 turns/s, 50 turns every 4.995–5.003 s with a single game; the
  loading hitches recover without losing turns.

## Pending

**Positions in float.** openblack's positions are float metres. As long as they are not stored as integer MapCoords,
each `ToFixed` of an already quantised value can lose one unit (0.15 mm). The cells do not change: a position at an
exact multiple of 0x10000 comes back intact.

### MapCoords

- `Lightning.cpp` `CanBeStruck`: the original first asks `IsAvailable`; the port does not.
- `Lightning.cpp`: `fn_00690C70` (the manager's targets) and `fn_00691E80` (the light map's height, now `LandAt +
  0.1`) not read. The cooldown's `PSysRand(int)` 0x6729E0 is settled (above): openblack's `RandomBelow`
  (`Particles/Rules/Lightning.cpp:100-103`, through `game_random::particles::Rand`) does the same unsigned draw.
- `Explosion.cpp`: `manager::CreateSpotVisual` receives seconds and converts them back to turns with `MsPerTurn()`;
  the original passes turns, so with a turn other than 100 ms the count is not the same. Its position does not go
  through `MapCoords(LHPoint)` 0x603160 either. The code comment should say the smoke's count is an inline copy of
  `TicksForSeconds(4)`.
- `Audio/SoundMap.cpp`: done; the cell centres and the receiver now go through `map_coords` (`k_FixedPerCell`,
  `ToMetres`, `src/Audio/SoundMap.cpp:142-152`).
- Not migrated, doubtful: `CHLApi.cpp:2699` (`MOVE_GAME_THING`: truncating there would make a second conversion when
  walking); `SpellResource.cpp:68` (`AddResourceToPos` already converts once, like the original); `Trees.cpp:1179-1186`
  (the cells of the tree bending, with signed cell differences).
- `WeatherLand.h` and `WorshipSite.cpp` also replace the MapCoords cell (high word) with `floor`. `PotResource` could
  walk its spiral with `AddCells`, for consistency.
- Owned elsewhere, found while auditing: `Climate.cpp:246` (`FindWhereToCreateStorm`) compares with
  `other.outerRadius`, where the original compares with the creating climate's radius (above);
  `AnimalAI.cpp` `LookForFoodPos` counts its cells as `(radius/10)²` in integer (the original's gives 12 cells for
  radius 35, not 9). See [Weather](../world/weather.md) and [the animals](../beings/animals.md).
- `VillagerSpeed`'s conversion of the speed back to metres is 🔍 inferred: the original never converts it; openblack
  stores it in metres with `ToMetres`, the original's conversion everywhere else.

### GUtils distances

The rule has been to migrate **only** where the original's `GetDistance*` / `hypotenuse` call was read.

- From other owners, not migrated yet: the miracles' `ECS/Influence/Influence.cpp:178-181` (`detail::DistanceXZ`,
  `std::hypot`, cites 0x74CD70), `ECS/Systems/Implementations/VillagerWorship.cpp:200-203` (`FlatDistance`),
  `Worship/WorshipSite.cpp:131, :147`, `Magic/Script/CHLFire.cpp:74` and `Magic/Script/CHLSpells.cpp:185`; the audio's
  `ECS/Fire/FireSound.cpp:49-63` (`CameraDistance`; the camera does not go through MapCoords either) and
  `Audio/GameQueries.h:47, :72-76` (`nearestTown`, described but not implemented in `Game.cpp`).
- Doubtful, not migrated (no reading that the original uses the routine there): `ECS/Trees.cpp:244, :718, :799, :1038`
  (no address in the citation; `Worship/Citadel.cpp`'s `NearestTownOfTribe` no longer exists, its town search is `map_cells::GetNearestTownToPos`);
  `ECS/AnimalFlee.cpp:596, :619, :653` and `ECS/AnimalPredators.cpp:447, :580, :641, :785` (marked 🔍 inferred by
  their place in the file); `ECS/Systems/Implementations/PathfindingSystem.cpp:100, :218` (they port
  `MobileWallHug::MoveTo`; their input would have to move to MapCoords first).
- `ECS/Weather/Climate.cpp:447` stays as it is: it is `GClimate::ComputeWeather` 0x771640, which inlines `d2 > r²`
  and `sqrt(d2)` (above); the climate's one `GetDistance` (0x771DBA, in `ProcessAll` 0x771DA0) is debug text.
- **Visible changes to check with a screenshot**: who goes to pray first (now the nearest, with life³), who goes to put
  out a fire (400 m), whom the heal miracle heals (everything within R of the spiral point, in a square of
  `ceil(2R/10)` cells a side), when an animal changes reaction (the distance to the centre of the ongoing reaction's
  cell), tree growth with the water miracle (`GetDistanceModifier(size, 3)`), and the predators' lairs.

### GUtils angles

It has only been migrated where the original's call at that point was read.

- `MobileWallHug::InitStepsXZ` 0x60BFA0: done; there is one copy, `villager::InitStepsXZ`
  (`src/ECS/Villager/VillagerScript.cpp:230-243`: `gutils::GetAngleFromXZ`, `SetGameAngle`, `gutils::StepFromAngle`),
  which `InitializeStepToGoal` in `ECS/Systems/Implementations/PathfindingSystem.cpp:62-65` calls. The orbit's own
  detour angles (`InitializeStepAroundObstacle`, :67-77, and :725) still use `glm::atan` and the float step
  `(cos, sin) · speed`. Owner: the wall hug.
- `ECS/Villager/VillagerCore.cpp:1009-1010` (`LookAtPos`) reads the game angle as `lround(yAngle · 2048 / 2π)`. The
  faithful thing is to store the `u16` +0x5C; until then the `lround` is right (`ConvertAngle3DToGame` would give a − 1
  in 365 of the 2048 angles that `setGameAngle` writes).
- For the animals' owner: the changes to `AnimalAI.cpp` (`AngleDiff`, `IsPosValidForTurnAngle`, `CalcRandomPos`, the
  uses of `AngleOf`), `AnimalBirds.cpp` (formation and `BirdDying`), `AnimalFlee.cpp`, `AnimalPredators.cpp` and
  `AnimalWallHug.cpp`.
- For the worship owner: with the altitude at 0, the icons of rings > 0 end up on the ground. The step between rings
  is 15 m, as the original (0x77B100; `k_IconRingStep`, `Worship/WorshipSite.cpp:87`)
  ([Worship](../magic/worship.md#the-icons-and-the-charge)).
- Postponed, no copy of GUtils: `Particles/Rules/Lightning.cpp:212` and `Particles/Rules/Storm.cpp:1504` (PSys
  atan2), `Magic/Objects/MapShield.cpp:180` and `Magic/Spells/SpellForest.cpp:422` (the polar spiral).
- Already exact, style only: the hand-written `a · 2π_f / 2048` left in `AnimalAI.cpp` (`FaceAngle`,
  `SetTowardsAngle`) gives `ConvertGameAngleTo3D` bit for bit (without the `& 0x7FF`). `AngleOfRotation` is the inverse
  of `FaceAngle`, not a routine of the original.
- Their own trigonometry, nothing to migrate: FishShoals:207, TestDispensers:281/304, PhysicsObjects:490, Rivers:46,
  FishFarmArchetype:68, WorshipSite:721, Climate:224 and SpellWater:179; Sharks:110 is `LH3DMath::GetYAngle`.
- **Visible changes to check with a screenshot**: the spell icons of the worship site's outer rings (now on the
  ground), which way an animal facing exactly away from its goal turns, and the working positions next to trees and
  forests (with the tree's altitude).
- The routines that take a double sine (`GetPosFromAngle`, `AddDistanceFromAngle`, `GetPointFromAngle`, the LH
  matrices) could move to `gutils::x87` ([The x87 arithmetic](floating-point.md#the-x87-arithmetic)) with their owners.
  Not changed.
- The `SinTable` index mask (`& 0x7FF` against the original's `& 0xFFFF`) gives the same value for every angle below
  0xA00 (Sin) / 0x800 (Cos): the table's entries 2048..2559 equal 0..511 (oracle peek of 0xC31614; the GUtils readers
  mask with `and eax, 0xffff`, 0x74D344, 0x74D364, 0x74D384, 0x74D3A8). Still open: whether any of the 25 indexed
  readers of each table (0xC31614, 0xC31E14) ever passes an angle of 0x800 or more, where the original reads past its
  table.
