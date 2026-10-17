# Floating point

## Summary

The original runs its game thread with the x87 FPU set to 24-bit precision: every add, subtract, multiply, divide and
square root rounds its result to a float's mantissa, while loading a double constant, comparing with one, and the
sine, cosine and arctangent instructions keep their full precision. Two other threads (the one that pumps the 3D
sound mixer and the music thread of the audio DLL) keep the system's 53-bit precision.

openblack is x64 and computes in SSE2 scalar float, one rounding per operation, with no fused multiply-add. That gives
the same bits for the basic operations; where the original's x87 code needs more (a double constant, an unrounded
sine, an exact `fpatan`), the port writes the step explicitly, and `gutils::x87` models the FPU exactly for the ports
that need it. The double constants of the audio code were audited one by one.

## The original

### Owners, the 24-bit FPU and the double constants

The audit of the audio code's arithmetic, together with the small owner helpers that came with it. Where the sounds
themselves are played is on [The audio engine](../audio/engine.md).

#### The game's FPU runs at 24 bits

`fn_007DEE00` (`fninit`, then `and cw, 0xFCFF` at 0x7DEE0D) leaves the precision control at 00. It is called from
`GGame::InitOneTimeOnly`, `EndTurn` 0x54E964 (also 0x54E974 and 0x54E984), `Process3dEngine` 0x54E426 / 0x54E4D1,
`RenderLoadingFrame` and `GAudio` 0x426F66 / 0x427061; `fn_007DEE20` then calls `_controlfp` with the mask 0x8001F,
which does not touch the precision. So **every `fadd` / `fsub` / `fmul` / `fdiv` / `fsqrt` on the game thread rounds to
a float's mantissa**, also inside LHaudiodllR and QMixer when the game calls them (LHSamplePlay, LHaudio's conversion
to polar 0x100122BC).

What does not round: loading a double constant (`fld` / `fmul qword` uses it whole), `fcomp qword` (compares with the
exact double), and `fsin` / `fcos` / `fpatan` (full precision; see [The x87 arithmetic](#the-x87-arithmetic)).

**Not on QMixer's pump thread.** `QSWaveMixSetPolarPosition` 0x180040B0 only stores the polar point; the conversion to
Cartesian 0x1800AA70 is done by `QSWaveMixPump` 0x18003900 (… → 0x180084A0 → 0x1800AA70), and `QSWaveMixPump` is
called every 20 ms by LHaudio's `timeSetEvent` (0x10015800) on winmm's timer thread, with the control word Win32 starts
threads with (0x27F, 53 bits) (🔍 inferred: nothing on that thread changes it). The only pass on the game thread
(`QSWaveMixPlay`) is redone by the next pump.

**The music thread.** The DLL's music thread (`_lhbeginthread`, `MusicEngine.cpp`) does not go through fn_007DEE00: it
starts with the system's control word (53 bits), so `MusicEngine` stays in double (🔍 inferred).

**One 53-bit window on the game thread.** `LH3DRender::StartFrame` sets the control word to 0x027F for its own timing
(0x82F12A..0x82F144) and calls fn_007DEE00 again at 0x82F275 before it returns, so everything after it in the frame is
at 24 bits again. No other `_controlfp` or `fldcw` raises the precision after start-up (the other `fldcw` are the C
runtime's save and restore pairs), and DirectDraw's cooperative level is set with flags 8 or 0x13 only, never
FPUSETUP or FPUPRESERVE.

The porting rule that follows:

- a comparison with a qword constant is done in double (`static_cast<double>(x) > -0.6`);
- arithmetic with a qword constant does the step in double and stores the result as a float
  (`static_cast<float>(double(a) * c)`);
- float arithmetic stays in float, step by step, in the order of the x87 stack.

#### Audit of the double constants

A sweep of every FPU instruction with a `qword ptr [constant]` operand in runblack.exe, LHaudiodllR.dll and
QMixer.dll, cross-referenced with the addresses cited in `src/Audio`, `CollisionSounds`, `FireSound`,
`Particles/Rules/Sound`, `SoundAction` and `AudioQueries`:

| Address (site) | Previous reading | Real double | Port | Change |
|---|---|---|---|---|
| 0x8CF7D8 / 0x9375E8 (0x69EEC4 / 0x69EEDC) | 0.6f / 0.3f in float | 0.59999999999999998 / 0.29999999999999999 | `SpellSounds.cpp` `SizeFromThrow` | compared in double: 0.6f and 0.3f now fall **above** (size 1 and 2, before 2 and 3) |
| 0x8C4A08 (0x429000) | −0.6f | −0.59999999999999998 | `AtmosBanks.cpp` ProcessBanks | compared in double (with floats it gives the same: no float falls between −0.6f and −0.6) |
| 0x10030450 (0x10012363 … 0x10012510) | 0.318471 (6 digits of `%g`) | 0.31847133757961782 (1 / 3.14) | `QMixerLaws.cpp` `PolarRelative` | the whole value, `atan · 180 · c` in that order, angles and distance in float, steps at 24 bits |
| 0x18037658 · 0x18036550 (QMixer 0x1800AA85) | π / 180 in double | π (double) · 0.0055555557f, in double (53 bits: the pump thread) | `PolarRelative` | k and el = k · elevation in double; az = k · azimuth stored in float; flat, up, right, ahead stored in float (the audit corrected an earlier 24-bit model) |
| 0x980520 / 0x980518 (0x71DEE1 / 0x71DEE7) | 15f and 1 / 30f in float | 15 and 0.033333333333333333 | `SoundMap.cpp` wind | `fild` of the integer sum, `fsqrt`, `− 15`, `· (1/30 double)`, each step to float (emulated: 0 differences in the 5924 values; the old formula differed in 3756) |
| 0x9A3BE8 / 0x8D45D8 / 0x8AB680 (fn_0086A7F0) | already in double, but everything in double | 0.03386318012808897 / 6.2831854820251465 / 1 | `Guidance.cpp` `MoonPhase` | each step to float (emulated: the same on the 8 days tested; the double model gave 3.3044245 instead of 3.3044248 on day 10976) |
| 0x8C49F8, 0x8AB260, 0x8C49F0, 0x8AB680, 0x8C2C48 (four1 0x428D50) | already in double, everything in double | 6.2831853071795898, 0.5, −2, 1, 0 | `LipSync.cpp` `Fft` | steps to float; sin(θ) unrounded (`fsin`) |
| 0x915438 (0x5C36E5) | double | 0.94999998807907104 (= 0.95f) | `AdvisorVoices.cpp` `Say` | in float (the 24-bit subtraction is the float one) |
| 0x915440 (0x5C3822) | double | 0.90000000000000002 | `AdvisorVoices.cpp` (advisor text, 0x5C381D) | already compared in double: no change |
| 0x9804D0 (0x71D210) | double | 0.15000000596046448 | `Guidance.cpp` RemarkOnMoonPhase | already compared in double: no change |
| 0x8CF2B8 (fn_0071C400) | double | 0.40000000000000002 | `Guidance.cpp` HeartBeat | already `float(double(o · 100) · 0.4)`: no change |
| 0x8C49E0 (fn_00427200 0x427227..0x427259), 0x10030468 (LHSamplePlay 0x100114D1 …) | — | 5000 (exact) | — | no change of value; see [Pending](#pending) |

By the same 24-bit rule, these were moved to float where they had been modelled in double without qword constants:
`Guidance` (`Cube`, `LocalFloatRand`, `Interval`, the random list, `DesireSample`, the desire value,
`UpdateHeartBeat`, `HeartBeat`, `PlayBeliefSample`, `BeliefVisibility`, `UpdateAlignmentRemarks`, the moon
calculation), `SpookyVoices` (the probability 0x72E34D, pitch and volume 0x72E3F6..0x72E4CD),
`GameMusic::DiscreteAlignment` 0x414730 and `LipSync` (`MagnitudeSpectrum`, `BandLevel`, `UpdateKey`; and
`AdvisorVoices::Amplitude`).

Checked without change: `SpellSounds` `SizeFromRadius` (0x69F4CA / 0x69F4F6) and `SizeFromImpactSpeed` (fn_006A1630
0x6A16A9 / 0x6A16CF) compare floats with a strict `<` (the old 🔍 inferred mark is removed). With no qword constants
at all: `AnimationSounds`, `LanternSounds`, `CollisionSounds`, `FireSound`, `Particles/Rules/Sound`, `SoundAction`,
`AudioQueries`. Outside the audio (untouched): the 1.5 qword of fn_005E5830 0x5E5A6C (night lights) and
`ReadSpeedFactor` fn_005C6CB0 (`HelpSystem.cpp`, in double; at 24 bits it would be float step by step).

The values were checked against Unicorn emulations of the original code (control word 0x7F or Unicorn's 0, both at 24
bits: the polar position, the moon fn_0086A7F0 and the wind), and QMixer also with 0x7F, 0x27F and 0x37F (Unicorn
respects the precision control: 0x7F gives other digits). With 0x27F the double model of `PolarRelative` gives the 9
points identical.

#### Audit: FPU and constants

Checked in the disassembly: fn_007DEE00 and its callers (above), 0x69EEC4 / 0x69EEDC (`test ah, 0x41`: a strict `>`),
0x429000, 0x71DEE1 / 0x71DEE7 (int8 squared, `fild`, `fsqrt`), 0x86A86B..0x86A888, 0x5C36E5, 0x69F4CA / 0x69F4F6,
0x6A16A9 / 0x6A16CF, four1 0x428DCA..0x428ED4 (the stack order the same as the code), HeartBeat 0x71C491..0x71C546,
CalcKey 0x4289C2..0x4289F0, LHaudio 0x100122BC..0x10012522 (the order of `fadd`, `__ftol` and `fdivr` the same as the
code), LHaudio's five double constants (0x10030440..0x10030460), LHSampleStop 0x10012C50 / 0x10012C76 / 0x10012CA8 and
0x1001439D, QMixer 0x18037658 / 0x18036550 and 0x1800AA85..0x1800AAFC. Everything adds up except:

- **Fixed**: QMixer's half of `PolarRelative` does not run on the game thread but on the pump thread (above), at 53
  bits: `k` and the elevation in double, az / flat / up / right / ahead stored in float; `RelativeAxesExact` holds the
  values of the 53-bit emulation (0x27F). The difference is a few float ulps (inaudible), but it is now the original's.
- Unchanged: `StopOwner` / `NewOwner` (no callers in the game; the `Owner::Object` owners of the rest all come from
  `NewObjectId`, so they do not clash), no new ECS dependencies in `src/Audio`, no new AL sources or buffers, the music
  thread untouched, other owners' public signatures the same; the new tests check emulated values.

### The x87 arithmetic

What the precision control does and does not round, measured on the processor and in the exe:

- **The precision control.** fn_007DEE00 sets the control word's precision field to 24 bits (`and 0xFCFF` at
  0x7DEE0D, what Direct3D also leaves); the oracle runs every call at 0x007F. Each `fadd`, `fsub`, `fmul`, `fdiv` and
  `fsqrt` rounds its result's mantissa to 24 bits (to the nearest, ties to even) but keeps the 15-bit exponent: a chain
  never overflows or underflows half way; only the `fstp dword` brings a value into the float's range.
- **`fsin` / `fcos` are not rounded by the precision control.** Their 64-bit mantissa stays on the stack until the
  first operation on it, which rounds the exact result once, or until a `fstp dword`, which rounds it to a float.
- **The FPU's π.** `fsin` / `fcos` reduce by a 66-bit π, 0xC90FDAA22168C234C × 2^-66. Of 440 000 angles past 100 rad,
  the true π gives another 80-bit value for 344 000; the 66-bit one gives the FPU's. From 2^63 on the operand is left
  unchanged on the stack (C2 set); ±∞ and NaN give NaN; −0 keeps its sign.
- **The FPU's last bit** (measured on one Intel processor). Its `fsin` / `fcos` are not always correctly rounded:
  2.4 % of the 80-bit values (69 000 of 2.9 million) are one unit of the 64th bit away, all where the exact value lies
  0.3 to 0.6 of a unit past a 64-bit value. No float result changed because of it. Other processors (AMD's) may differ
  in those last bits; which one the original's players had is not known.
- **`fpatan`** stored to a float (`fstp dword`) is `atan2` in double rounded once to a float: 2 000 000 pairs of floats
  on the processor at 0x007F, none differ (`atan2` in float: 2 differ).
- **The inline exp.** The original's inline exp, at 24 bits: `fldl2e; fmulp` (rounded to a float: the 64-bit log2(e)
  as the double one, equal but at a tie); `frndint` (to nearest); `fsub` (exact); `f2xm1` (not rounded by the
  precision); `fld1; faddp` (rounded to a float); `fscale` (exact). Its users: `HelpDudeControl::Process`
  0x5C3AB8..0x5C3ACA / 0x5C3AE3..0x5C3AF5 (the spirits) and `HandStateGrain::Update` 0x5B3452..0x5B348B /
  0x5B3561..0x5B3583.
- **Why not a double sine.** `std::sin` in double, rounded once by the product (what the `gutils` angles and the LH
  matrices do), gives the FPU's float for every product measured, but not for sums where the sine cancels:
  `cos(a) + 1` near an odd multiple of π, and `sin(a) + sin(b)` with b near −a. A float sine (rounded twice) differs
  everywhere.

The model of `gutils::x87` (see [openblack](#openblack)) against the processor and the exe:

| Measured against | Cases | `gutils::x87` | double sine | float sine |
|---|---|---|---|---|
| The processor, 32-bit harness at 0x007F: `fsin`·m, `fcos`·m, `fsin a + fsin b`, `fcos + 1`, angles in ±8 | 1 000 000 × 4 | all | all but 8 (`fcos + 1`) | 23 %, 23 %, 40 %, 34 % differ |
| The same, angles near multiples of π/2 up to ±3142 | 200 000 × 4 | all | 9 441 `fcos + 1` differ | |
| The same, cancelling sums (b near −a or a − π, a near (2k+1)π) | 900 000 × 4 | all | 102 sums, 30 `fcos + 1` differ | |
| The same, angles up to ±10^20 (the 66-bit π, the operand left past 2^63) | 250 000 × 4 | all | | |
| The exe (oracle): the fish farm's swim step fn_008248E0 (thiscall, this = the fish, [this] = its sprite, [0xEA9EC0] = ms, [0xEB99F8] = 0; `fcos` / `fsin` at 0x8249D9, 0x8249EB, 0x824A0A, 0x824A0F) | 200 000 | all (position, heading, frame, fright, sprite) | all | 17 % of x / z / heading differ |
| The exe: the fish puzzle's net fn_00829BC0 (`fcos` / `fsin` 0x829C5A / 0x829C61, the wobble's `fcos` 0x829CF0; fn_00822560 and its two virtual calls stubbed) | 100 000 | all | all | 23 % differ |
| The exe: the fish farm shoal's update fn_00824DA0, run whole (`_rand` 0x7C8837 and the fish's own step fn_00824720 stubbed): the scare's `fcos` 0x824F48 stored to a float, its `fsin` 0x824F70 unrounded × 2 (0x824F72..0x824FA9), the scared fish's `fpatan` 0x825052 into a `fstp dword` | 100 000 (57 629 scared) | all | | float `std::sin` / `std::cos`: 63 targets differ |
| The exe: the wisps' orbit in `UR_CreatureSpellGeneric::ModifyAtomCollection` 0x678D50, its stretch 0x67941C..0x6795C5 | 150 000 | all | all | 6 % differ |
| The exe: the flies' circle in `UR_CreatureSpellItch::ModifyAtomCollection` 0x6781A0, from 0x678398 to its end | 100 000 | all | all | 7 % differ |

The two particle rules are entered in the middle: the oracle copies a stack frame laid out by hand onto the stack,
loads the registers the stretch reads (the rule, its AtomData, the collection), patches a jump back at 0x6795C5
(Generic) or returns through the function's own epilogue (Itch), and stubs `AtomCollection::GetAge` 0x674ED0.

### Sources

- The exe's disassembly at the addresses above (xref index), and the oracle runs in the table.
- Unicorn emulations of the audio code at control words 0x7F, 0x27F and 0x37F.
- A 32-bit harness running the processor's own `fsin` / `fcos` / `fpatan` at 0x007F.

## openblack

**Done.** The arithmetic of the ported code follows the porting rule above; the double constants of the audio are the
original's (table above).

- **The build.** The top `CMakeLists.txt` stops the configure on `/fp:fast`, `/fp:contract`, `/arch:AVX2` /
  `/arch:AVX512`, `-ffast-math`, `-mfma` and `-ffp-contract=fast` (they would fuse or reassociate float math).
- **`gutils::x87`** (`src/Common/GUtilsX87.h`, namespace `openblack::gutils::x87`, added 2026-10-15), pinned by
  `test/test_gutils_x87.cpp` (group `test_core`): a shared pure helper for ports whose x87 code must give the
  original's bits; nothing called it when it was added. A value at float precision is a double holding a 24-bit
  mantissa; `Mul`, `Add`, `Sub`, `Div` and `Sqrt` work in double and round to 24 bits, which is exact (53 ≥ 2·24 + 2 bits,
  so the double rounding never moves the second one). `SinCos` reduces the angle by the 66-bit π exactly in
  double-double arithmetic, sums the Taylor series in double-double (14 terms each, |r| ≤ π/4) and rounds to 64 bits:
  an `Extended` (high + low, exact). `Mul` / `Add` / `Sub` on an `Extended` round the exact (double-double) result
  once. All `constexpr` but `Sqrt`.
- **The double-sine routines** (`GetPosFromAngle`, `AddDistanceFromAngle`, `GetPointFromAngle`, the LH matrices) give
  the FPU's floats for their products ([GUtils angles](coordinates-and-angles.md#gutils-angles)); they can move to
  `gutils::x87` with their owners.
- **The inline exp** has a helper of its own (`gutils::ExpSinglePrecision` in `src/ECS/FastExp.h`, formerly `Exp24`
  in `src/ECS/GUtilsExp.h`), used by `Help/Spirits.cpp` and `Magic/HandMotion.cpp`.
- **The audio's 53-bit threads**: openblack's music thread is untouched; `PolarRelative` models the QMixer half at 53
  bits.

### Owner helpers

These are openblack helpers (`Audio.h`) built on the original's sample calls:

| Helper | What it does |
|---|---|
| `StopOwner(Owner)` | Stops all of the owner's channels in **any** bank: `LHSampleStop(bank, owner, 0)` 0x10012C50 (sample 0 = any, 0x10012C76; `sample_play::StopOwner`) for each registered bank 1..`banks::Count()`, each channel with QMixer's 20 ms ramp. With the audio off it stops nothing (each LHSampleStop ends at the first channel, 0x10012CA8). The original does not have this loop: its callers stop bank by bank. |
| `NewOwner()` | An owner of its own: `Owner::Object(NewObjectId())`, never equal to a game owner (things are `Owner::Thing`; the CHL numbers and the voices 0x270C..0x270F are `Owner::Key`; the other objects take their id from the same counter). A 2D or 3D sound without tracking (`PlayOptions::track` = 0, or `PlaySoundEffectAt` 0x42A100 with track 0) needs nothing more. A 3D one with tracking (`PlaySoundEffectAt` 0x42A040 tracks all 3D, +0x0C = is3D) follows `RegisterObject(owner.id, position)` and is stopped on the next turn (owner gone, LHSampleStop 0x1001439D) if nothing is registered or it returns nullopt. When done: `StopOwner` and `UnregisterObject(owner.id)`. |
| `FindSample(bank, "name.wav")` | openblack only: the bank's sample whose .sad name (+0x00, only the file name) is that one, case-insensitive; its number is the +0x104. The original only uses numbers; this is for the tools. No unit test (it needs the banks registered). |

A caller looks up its bank with `FindBank(path)` (or registers its own with `RegisterBank`) and plays with
`PlaySoundEffect` / `PlaySoundEffectAt` and that owner.

### Tests and game

- `test_audio_laws` `RelativeAxesExact`: 9 points against the emulation (QMixer at 53 bits), to 2·10⁻⁷ relative +
  10⁻⁶ (the 24-bit model of QMixer fails by 9·10⁻⁵ at (−300.5; 210.25; −15.5)).
- `test_spell_sounds` `sizeClasses`: 0.6f → 1, 0.3f → 2, the float below each → 2 / 3.
- `test_guidance` `PhaseFromTheRealClock`: six days against the emulation, exact.
- `test_sound_tag_channels` `StopOwnerInEveryBankAndModOwners`: two banks, two owners from `NewOwner`; `StopOwner`
  stops only the owner's, in both banks; a 3D one with tracking follows `RegisterObject`, and the one without a
  registration is stopped.
- The audio suite passed 64/64. Land 1 until frame 5900 with `OPENBLACK_ATMOS_TRACE=20`: the same 36 start-up errors as
  before; ambience and bells as before.

### Floating point

How the 24-bit game thread maps onto openblack's x64 build:

- The original sets the FPU to 24-bit precision for the game logic (fn_007DEE00, `and 0xFCFF` at 0x7DEE0D; also at the
  start of `GGame::EndTurn`): each + − × ÷ and square root rounds to a float. openblack is x64: SSE2 scalar float, one
  rounding per operation, MSVC's default `/fp:precise`, no FMA. For those operations the results are the same,
  (🔍 inferred) except where an intermediate would overflow, underflow or be denormal in float (the x87 keeps its
  extended exponent range).
- The transcendental functions (`pow`, `sin`, `cos`, `atan2`, `exp`, `log` …) of the x64 C runtime are not the
  original's x87 instructions (`fsin`, `fpatan`, `fyl2x` …) nor its C runtime's x87 code: their last bits can differ
  (**not verified** case by case; see [Pending](#pending)). `game_random` already writes its arithmetic one operation
  per statement ([Random numbers](random.md#arithmetic)).
- New threads start with the default MXCSR (0x1F80: round to nearest, no flush-to-zero); nothing in `src` changes it.
  The game logic always runs on one thread.

### Test hooks

None of its own. The tests are `test_gutils_x87` and the audio tests listed in [Tests and game](#tests-and-game).

## Pending

- **⚠️ approximate** The step in double and then to float can round twice on a tie (float · double constant); `sin` /
  `cos` / `atan` are the library's, not the x87's 64-bit ones.
- **🔍 inferred** The FPU of the audio DLL's music thread at 53 bits (it does not go through fn_007DEE00), and that of
  winmm's timer thread that pumps QMixer, also at 53 bits. Verify with the oracle by reading the control word inside
  the thread's entry (`xref.py callers 0x10015800`, then run it with `fnstcw` observed).
- **The 5000 constants** of fn_00427200 (0x427227..0x427259) and LHSamplePlay (0x100114D1): exact, no change of value;
  the code that uses them is not audited.
- **The transcendental functions** of the x64 C runtime against the x87 instructions: to check case by case where they
  reach the game's results.
- **The logic thread's MXCSR**: a planned debug-build check that the game thread keeps 0x1F80.
- **Sources that disagree**: an older draft of the x87 section counted 9 456 `fcos + 1` differences near multiples of
  π/2; the newer measurement, written above, is 9 441.
