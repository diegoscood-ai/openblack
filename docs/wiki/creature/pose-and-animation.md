# The creature's pose and animation

## Summary

The game's logic reads a creature's bones from a skeleton posed once a game turn, not from the frame being drawn. At
the end of each creature turn the body's clocks are set from the turn's whole milliseconds, its animation layers
(face, gesture, breath, body action, head) move on in whole ms, and the body is posed once in world space, leaned onto
the slope; the frame then draws a blend of the last two turn poses. A turn on the spot blends the species' spin
animations by the angle.

openblack poses the creature once a turn through `CreatureAnimationSystem::AdvanceTurn` and `PoseTurn`, with the
clocks, the layers' integer advances, the lean and the turn on the spot pinned to the oracle, and draws the blend of
the two turn poses. Some readers and tracks still take the frame's pose or float times; they are listed under
[What openblack does with the turn's pose](#what-openblack-does-with-the-turns-pose) and in [Pending](#pending).

## The original

#### Movement, body over time, spells, fights, throws

The creature's movement, its body over time, its spells, fights and throws are separate components in openblack (see
[openblack](#openblack)); the creature's sound actions per spell are 0x76..0x7E.

##### The turn on the spot

Read in v1.42 and pinned with the oracle. Two ways in reach the same `InitialiseTurning` 0x48B790: the walk's start
(`GetStartState` 0x483AB0, motion 4 when the route's next point is not within π/12, 0x8C7A08, of the heading and no
step fits) and the body action of state 0x23 (`StartTurningAction` 0x481160, refused while a body action plays; from
fn_004F00E0 = `StopMoving` + it, which the mind's turn-to-face sub-states call, `EndAnyActionsThenTurnToHeading` and
the creature room).

- `InitialiseTurning(target)` keeps the target at +0x4840 and takes the angle `angle_correct`(target − heading
  +0x84), a float: above 0 the spins 6, 7, 8 (+0x521C = 1), else 3, 4, 5 (+0x521C = 2) with the angle negated
  (`fcomp 0; test ah, 0x41`: 0 and a NaN go to 3, 4, 5). Without all three of the side's no blend is made (+0x5220 =
  0); a blend (0x38 bytes) is kept at +0x5220. +0x522C (the mirror) is set to 0.
- Below a quarter turn (1.5707964f [0x8C78D8], `test ah, 1`: a NaN counts as below) the first two are blended by
  angle × 0.63661975f ([0x8CF14C], a multiplication), else the last two by (angle − 1.5707964f) × 0.63661975f;
  neither the angle nor the weight is held anywhere. The blend is `CAnim::CAnim(a, frame, b, frame, w)` 0x85E9D0,
  whose length is `fistp`(float(float(len b − len a) × w) + len a): every step a float at the game's 24-bit
  precision, then to the nearest, halves to even (a NaN weight gives 0x80000000). The time +0x47D0 starts at 0.
- Each step `DoTheTurningBusiness` 0x48B970: with the blend its time goes on by the animation step +0x48BC and, at
  its length or beyond, the heading takes the target, the time is zeroed and the blend freed (it returns 1); without
  one, the heading becomes `angle_correct`(float(time × `angle_correct`(target − heading) / stand's length) + heading)
  with the time before the step, then the time goes on, the heading taking the target at the stand's length.
  The blended turn plays through `CheckSounds(4, from, to)`, which queues its sound events; they are played later with
  `LocalRand` (`UpdateTime` → `fn_004818D0` → `fn_004ED250`), the only draw a turn can make, and only if the turn
  animations carry sound events. There is no angular rate: a turn lasts one length of the (blended) animation,
  whatever its angle.
- The walk's turn (`UpdateMotion` 0x490C3A) sets off as soon as it returns 1 (`ProcessMoveStraight` with a route,
  else motion 0). The body action's turn ends only in `StateSwitch` (state 0x23, 0x485CBD), run before the step on
  each `UpdateTime`: |heading − target| (a float) not at least 1e-4 (the double [0x8C79D8], 1e-4f widened; a NaN
  counts as within) puts the heading on the target, `StateSet(0)` and frees the blend; so a turn whose spins have run
  out is over at the next step, and a turn already within 1e-4 at the first.
- Oracle (`InitialiseTurning` with `GetAnim`, `new` and the `CAnim` constructor stubbed, the constructor itself with
  fake animations of no frames, `StateSwitch` in state 0x23, `DoTheTurningBusiness` both ways): target 0x39561F77
  from 0 gives the weight 0x3908509B (a division by float(π/2) gives 0x3908509C); 1200 and 1701 ms blended by
  0x3F000001 give 1450 (1450.50003 is the float 1450.5); the heading 0 is not within 1e-4 of 0x38D1B717 and is of
  0x38D1B716.

### The spec file's animation numbers

`Data/ctrspec27.txt`, counted from the first animation after the version line, skipping `=` section lines:

| Number | Animation |
|---|---|
| 0 | stand |
| 16-27 | faces (the first ten pulled when idle) |
| 28-30 | sleep: start, loop, end |
| 31-33 | poo: start, loop, end |
| 34-36 | puke: start, loop, end |
| 37-39 | sit: start, loop, end |
| 52-74 | actions |
| 75 | head turn right / left |
| 76 | head down / up |
| 77, 78 | the same while sitting |
| 96 | eat |
| 106, 107 | faint, get up |
| 200-206 | gestures (202 yawn) |

The morph animation header's fields: duration +0x0, looping +0x4, stride rate +0x8, stride length +0xC, displacement
+0x10..+0x18 (still `unknown0x0..` in our `MorphFile.h`). `HairHeader`'s +0x0 is the species' sound-object key.

### The body posed for the turn

**Who poses, and when.** `LH3DCreature::UpdateTime` 0x481DF0 (vt +0x14) is called on a game creature's body only from
`Creature::ProcessState` 0x473939, so the body is posed once per game turn, at the end of the creature's turn: after
`ProcessSpells` (0x472EBB, which reads the hand and its mirror of the pose before), `fn_004EFCE0` (`SetSize`), the
leash, the look, the plans and `ProcessSubAction`. It is skipped (a jump to 0x47393C) when [0xD00DE4] is 0 or the
mind's +0x20CE8 is 0 (0x4737B5, 0x4737DB, 0x4737ED). Its argument is `ftol(physical+0x48 × [0xD01A38])`, the turn's
ms. A held creature (+0x24 bit 0x10, +0x3CC == 1, not `IsPerformingBodyAction`) goes straight there after the freeze
below; +0x3CC == 2 calls `fn_00477D70`, which returns 0.

#### The turn's clocks

[V] The argument (0x473917..0x473936) is `fld` physical+0x48 (f32), `fimul` (int)[0xD01A38]
(`GGameInfo.MillisecondsPerGameTurn`), `__ftol`: at the game's control word (PC24) the exact product is rounded once
to a float and cut towards zero. Physical +0x48 is 1.0, or 1 − ratio while frozen (the freeze's ease, 0x4F54C3,
clamps the ratio to 0..1 first), so a 100 ms turn frozen by 0.1 lives 90 ms (0.89999998 × 100 rounds to 90.0), by
0.333 66, by 0.9 10 (10.000002), by 0.99 0. `UpdateTime` then sets, once (0x481E01..0x481E7C), from that one whole
number:

- `IntTimeInc` +0x48B4 = the ms;
- `FloatTimeInc` +0x48B8 = `fild` ms × the float 0.001 [0x8AC418] (0x3A83126F), stored as a float: 100 ms is
  0x3DCCCCCD, 33 ms 0x3D072B02;
- `AnimTimeInc` +0x48BC, **the step**: the ms when +0x4A90 (real-speed animation) is set, else
  `ftol((1.6 [0x8CF024] − (size +0x90 × 0.5 [0x8AA3B4]) × 0.85 [0x8CEFAC]) × ms)`, every constant a float, each
  operation rounded to a float, `__ftol` cutting towards zero; then doubled (`shl 1`) when +0x5730
  (`EndAnyAnimsRapidly`) is set, after the cut and whichever way the step was made.

Oracle (`UpdateTime` run to its +0x47BC early exit, its callees stubbed): 214 cases (sizes 0.05..4, ms 0..200, both
flags, the frozen turns above), the values openblack's tests pin (see [openblack](#openblack)). Some steps:
(1 ms, size 0.05) 1; (100, 0.05) 157; (100, 0.5) 138; (200, 0.05) 315; (100, 1) 117; (99, 1.3) 103; (50, 0.7) 65;
(100, 1.75) 85; (100, 2) 75; (100, 3) 32; (200, 3) 64; (33, 4) −3; (117, 4) −11 (past a size of about 3.76 the step
goes back); (1, 1.5) 0; ms 0 → 0; doubled (100, 1) 234, (50, 0.7) 130; at the real speed 100, both flags 200.

The bounding-sphere and mesh-intersect caches (+0x5274, +0x57A8) are cleared here and filled by whichever reader comes
first afterwards; a reader inside `StateAction`, before the pose, fills them from the pose before.

#### The layers' whole-ms advances

[V] Every animation time `UpdateTime` moves is an `int32` moved by one of two functions, `GetAnim(anim, 0)`'s first
dword being the animation's duration (signed), and `CheckSounds(anim, old, new)` 0x617970 looking for its moments
(sounds, hair) over a stretch:

- `Morphable::AdvanceSimple` 0x617A80 (played once): `n = t + d` (32-bit add); `n = dur` when `dur < n` (`jl`,
  signed), i.e. `min(dur, t + d)`; then always `CheckSounds(t, n)`, even when `n == t` or `t` was past the end (it
  comes back to `dur`); returns `n`. A negative `d` moves it back.
- `Morphable::AdvanceCyclic` 0x617A10 (looping): `d == 0` returns `t` at once, even past the end, with no check.
  Else `n = t + d`, `CheckSounds(t, n)`; when `n ≥ dur`: `n −= dur` once (not a modulo) and, when `n > 0`,
  `CheckSounds(t − dur, n)` (the duration reread from the anim); then `n ≥ dur` gives `dur − 1`. So exactly reaching
  the end gives 0 with one check, a step of two lengths or more holds at `dur − 1`, and a duration of 0 gives −1 (two
  checks of the same stretch).
- `OperateFace` 0x480D40 (the first thing after `PlayQueuedSoundEffects`), on FacialActionTime +0x49B4, Requested
  +0x49B0, Current +0x49AC and FacialAnimTime +0x49B8: when +0x49B4 > 0 it becomes `+0x49B4 − IntTimeInc`, held at 0
  (`sub; sets; dec; and`: 0 when the 32-bit difference is negative); a value below 0 is never counted down. When
  +0x49B4 == 0, Requested := 0. When Requested ≠ Current: if Current, the delta is IntTimeInc, `sar 2` (0x480D96, an
  arithmetic shift: a quarter rounded towards −∞) when Requested is 0, FacialAnimTime := FacialAnimTime − delta held
  at 0 the same way, and at 0 Current := 0; then if Current is 0, Current := Requested (FacialAnimTime is not reset).
  Else (Requested == Current) when Current and `GetAnim(Current, 0)`: FacialAnimTime := `AdvanceSimple(Current,
  FacialAnimTime, IntTimeInc)`.

Oracle (`GetAnim` stubbed to a duration cell or to 0, `CheckSounds` stubbed and its arguments captured): 27 (t, d,
dur) cases through each advance and 23 + 2 face cases, the values openblack's tests pin (see [openblack](#openblack)). Some: once (1200, 0, 1000) → 1000, check (1200, 1000); looping (900, 117,
1000) → 17, checks (900, 1017), (−100, 17); (883, 117, 1000) → 0, one check; (0, 2500, 1000) → 999; (950, 117, 100)
→ 99, checks (950, 1067), (850, 967); (0, 117, 0) → −1; (1200, 0, 1000) → 1200; (2147483647, 1, 1000) →
−2147483648 both ways. Face relaxing from 300 by 103 ms → 275; by 3 → 300; by −3 → 301; by −100 → 325; held at −5
keeps playing (300 → 400); a face without its animation stays.

#### The order of the turn's advance

[V] After the clocks, the blink timer (+0x5468 −= IntTimeInc, 0x481FF1), the breath period's ease (`ProcessBreath`)
and the sway spring, and unless +0x47BC sends it straight to `UpdateBuffers` (0x48248E..0x4824A7):
`PlayQueuedSoundEffects` (0x4824AA, the sounds the turn before queued), `OperateFace` (0x4824B1), the gesture
(0x4824B6..0x482514), `StateSwitch` (0x48256A), `StateAction` (0x482571), `UpdateLook` (0x482578), `UpdateBuffers`
(0x48257F). The frame's `PrepareForDrawing` (below) moves none of these times.

- The gesture (+0x5738, its time +0x5734): skipped when +0x5738 is 0; delta = AnimTimeInc, halved (`cdq; sub; sar 1`,
  rounding towards 0) when +0x5730 is set; when `t + delta ≥ Duration` (`jl`, signed) it is finished and delta :=
  Duration − t; +0x5734 := `AdvanceCyclic(g, t, delta)`; then, finished, +0x5738 := 0. So the last turn plays it to
  its end (one check of (t, Duration)) and it is gone.
- `StateSwitch` ends an action when `CycleTime[0] + AnimTimeInc ≥ Duration`, then plays it to the end with
  `AdvanceSimple`.
- `StateAction` starts with the breath (0x486A07..0x486A34): +0x4988 := FloatTimeInc / +0x498C + +0x4988, less its
  `__ftol` (the fraction kept), before any state's action. Then (bw1-decomp, `Black/Creature3D.cpp`): the static
  actions `CycleTime[0] = AdvanceCyclic(anim, t, AnimTimeInc)`; the fight's moves by
  `(long)(GetFightMul() × IntTimeInc)`; the kiss by IntTimeInc; the walk by distance,
  `AdvanceCyclic(walk, t, (long)(Duration × cycles))`, the stand at `(long)(Duration × breath)`, the run at
  `CycleTime[1] × Dur0 / Dur1` (integer division).
- `UpdateLook` turns the head by FloatTimeInc (`RotateSmoothly`'s argument read at 0x482FE2 and 0x48300D); +0x5744 −=
  AnimTimeInc.

**Inside `UpdateTime`**, in order: the sparkles and blood, the sway spring and `UpdateLook` read the pose before;
`StateAction` 0x486A00 sets the slot animations and times (its state handlers `fn_0048A5E0`, `fn_0048BAB0`,
`fn_0048C6C0`, `fn_0048CEB0` and `fn_0048F200` read the pose before); then `UpdateBuffers` 0x4EC590 poses; then the 8
destruction points, a carried creature (put at the leaf mean of this one's hand, then posed itself) and the physics
sphere read the new pose. The sparkles are in
[The evil and good sparkles](drawing.md#the-evil-and-good-sparkles).

#### Rotating the buffers

**`UpdateBuffers`** rotates the buffers: +0x5174 (the previous pose) := +0x5178, the work goes into +0x517C, and at
the end +0x517C and +0x5178 swap (unless +0x5218, which nothing ever sets: `fn_00480C50`, its only writer besides
`Init`, has no call, jump or pointer anywhere in the image). The first time (+0x51DC == 0) +0x5178 is also copied
into +0x5174. The pose is built in world space under the body's matrix (`fn_004EBE10`,
[The lean onto the slope](#the-lean-onto-the-slope)). `ResetLook` 0x483269 re-poses through `UpdateBuffers` too,
from `ReconnectToGame`, `ForceMoveMapObjectWithoutWalking` (which then copies +0x5178 into +0x5174),
`FallingSpell::Init`, `fn_00490960` (inside `StateAction`, after a `SetPos`) and the function at 0x4912F0: a creature
can be posed more than once in a turn, and every pose moves the current pose to the previous.

#### The frozen copy

`fn_004806E0` (from `fn_00477D00`, the held branch of `ProcessState`, before the pose) sets +0x5270 and copies
+0x5178 into +0x5180; the hand's start of a locked select on a creature (`NetworkFriendlyStartLockedSelect`
0x5DA950, at 0x5DA9CA; not `GInterface::PlaceObjectInMagicHand`) copies it again. `ReconnectToGame` 0x480730
re-poses and clears +0x5270. `GetSafeBuffer` 0x4842B0 is +0x5180 while +0x5270 is set, else +0x5178; while frozen,
+0x5178 keeps being rebuilt each turn, from one action only. The +0x526C flag that `fn_004806C0` sets with the freeze
is what lets `HandStateCreature::Update` run (it returns at 0x5B17F4 without it).

#### The drawn body

`LH3DCreature::PrepareForDrawing` 0x4ED320, every world-view frame (`Process3dEngine`): f = min(turn fraction, 2.0)
(the double constant 2 at 0x4ED33A); the drawn bones +0x47F0 = `fn_004EECD0`(+0x5178, f, first) then (+0x5174,
1 − f, add): f·current + (1 − f)·previous, float by float, in world space, not normalised again. `fn_004EECD0` is
`dst = w·src` (first) or `w·src + dst`, per float, over bones·12 floats (the count at 0x4ED340 is bones·3·4): the 3×4
matrices, which have no bottom row. No animation is sampled and no clock moves per frame. The frame's own ms reach
only the hair (`fn_00848350` at 0x4ED5DC, TimeInc [+0x205D48] × 0.001 held to 0..1, or g_delta_time in the citadel or
while [0xC4CCEE]) and the sounds with a fraction below f. So the body is posed once a turn and drawn moving from the
last turn's pose to this one, a turn behind. What the drawing does around it is in
[Drawing between turns](drawing.md#drawing-between-turns).

#### The hand's leaf mean

`fn_0083A0E0`, used by `FillAverageHandPos` 0x48CC80 and for the carried creature: from the start bone along its
following siblings, a bone without children adds its translation to the sum (rounded to a float after each add) and
is counted, a bone with children recurses into its first child; the sum times 1 / count. The mirror bone is the
file's table (`GetMirrorBone` 0x481390, +0x51F0: the creature block's last number for each bone).

#### When the spell info reads it

📼 1.20 only `Creature::UpdateSpellInfo` 0x4F8750 is vt+0x5C, reached from `Spell::ProcessSpells` (the game turn at 0x54E66F)
→ `ProcessMaintainRequests` 0x720315 → `ProcessMaintainRequest` 0x7203B5 → 0x72055D, after `ProcessLiving`
0x54E65B: it reads this turn's pose. Physical +0x48 is 1.0 from the `CreaturePhysical` constructor (0x4EF461) and at
the freeze's end (sub_4F54F0, 0x4F5506), 1 − clamp(ratio, 0, 1) during the freeze's ease (sub_4F5430, 0x4F54CB), and
restored by `Creature::Load` (0x4E68A6); no other store reaches it [V].

#### Which pose each reader takes

📼 1.20 only

- **The footprints** read the drawn bones +0x47F0, the world blend, as the drawing left them: a footstep (sound
  actions 3 to 5) in `PlayASoundEffect` takes the foot bone [+0x51CC] and its mirror from +0x47F0
  (0x4834C0..0x4834F9), keeps the lower by the translation's y (+0x28), and lays the print at that bone's
  translation, turned by `GetYXZ` of its matrix (0x48354C..0x4835A0). The queued sounds play in `PrepareForDrawing`
  before its blend (0x4ED39C..0x4ED434, the blend at 0x4ED43A), and the leftovers in `UpdateTime`'s
  `PlayQueuedSoundEffects`, so both read the blend of the frame before.
- **The hand's touch** reads the drawn bones of the same frame: the hand's mesh test (fn_00865020) takes a
  creature's `TransformedMatrices` +0x47F0, and `HandStateCreature::Update` (its mesh hits, the parts it strokes)
  runs inside `CHand::PrepareForDrawing`, which `Process3dEngine` calls after the creatures' `PrepareForDrawing`
  ([The frame order](../hand/animation-and-placement.md#the-frame-order)).
- **What the creature holds** is placed by `FillHeldObjectMatrix` 0x48F280, which takes the bones to read as its
  argument: the hand bone [+0x51C8] (or its mirror while +0x5230), its leaf mean (`GetGraspPoint` 0x83A0E0) and the
  bone's rotation, normalised, write the held object's 3D matrix and place (0x48F2AD..0x48F53B). It has three
  callers: the drawing (`DrawNow` 0x48E299 → 0x48F550, which passes +0x47F0, 0x48F584), the let go
  (`DoDiscardAction` 0x48C6C0, from `StateAction`, which passes +0x5178, 0x48C930: the turn pose as the last turn
  left it, before this turn's `UpdateBuffers`) and `SaveCreatureSnapshot`.
- **The let go** [V]: the Discard sub-action's start 0x5001F0 calls fn_00484E60(body, anim), which fails while
  `IsPerformingBodyAction`, without the animation or with nothing held (3D +0x48D0), else sets +0x49A0 = anim and
  `StateSet(0xC)`, keeping the holding hand's mirror. State 12 (`DoDiscardAction` 0x48C6C0) plays that animation alone
  and lets go once the animation time +0x47D0 reaches its release time from the body file (`LH3DCreature::LoadBinary`):
  95 → +0x4908 "DiscardTime", 96 → +0x4960, **97 → +0x4A40 "PutDownTime"** (0x48C6E2..0x48C727). For 97 the velocity is
  zero (95 throws with 0.6 × the hand's velocity) and the object, placed at the hand bone, goes to
  `InitialisePhysicsFromHand`; 96 drops it with no physics (eaten).
- **The leash's collar** is read every frame after the drawing, from the drawn bones +0x47F0 at the leash bone
  ([The leash's collar end](leash-rope.md#the-leashs-collar-end)); `GetLeashStartPoint`'s flag chooses +0x5178 (set)
  or +0x47F0 (clear, as `UpdateAllLeashes` passes it) for a creature the leash is tied to (0x5D8ED0..0x5D8EE6).
- **How far through the body's action is**, for a slap or a stroke (`HandStateCreature::Update`), is
  `GetBodyActionFraction` 0x484C60: `fild` CycleTime[0] +0x47D0, `fidiv` the animation's whole-ms duration, so the
  turn's whole ms, which move only in `UpdateTime`. A slap is skipped while it is at or below the float 0.35
  [0x8D6974] (`fcomp`, `test ah, 0x41`, 0x5B1DF0..0x5B1E00) and a stroke while at or below the double 0.8 [0x900AC8]
  (0x5B21CF..0x5B21DF).

#### The lean onto the slope

Read in the v1.42 `runblack.exe` and run in the oracle. `LH3DCreature::GetPositionMatrix` 0x4EBE10 builds the body's
matrix; bw1-decomp has no body for it (`Creature3D.h` declares it, and the radius as `field_0x5228`).

- **The radius** d = +0x5228 = +0x94 × +0x5224 (`SetSize` 0x480642..0x48064C): the drawn scale (size clamped to
  0.05..4, × 15 / rest height) times `CalculateRadius2D` 0x48F5B0 (0x4EBD32 in `LoadBinary`, stored at +0x5224): the
  furthest of the first stand frame's bones from the vertical axis, 0 without bones or without the animation. It is
  the creature's `Get2DRadius` (0x477F49 reads +0x5228), see [The creature's radius](body.md#the-creatures-radius).
- **The four heights** (0x4EBE18..0x4EBEF6): the land at (x + d, z), (x, z + d), (x − d, z) and (x, z − d), in that
  call order, through vt+0x28 (`LH3DCreature::GetAltitude` 0x47F720: `ftol(x × 65536 × 0.1)`, the same for z,
  `LH3DIsland::GetAltitude` of those map coords), x, y, z the object's position +0x78..+0x80. While +0x51EC is set
  all four are the creature's own y (0x4EBEA9..0x4EBEB8), so it leans on nothing.
- **+0x51EC** is written only by `Init` (0, 0x4801EC) and `LH3DCreature::SetInHand` 0x490860 (1 when
  `Creature::CanCurrentlyBePickedUp`, else unchanged; 0 when let go), which is called only from `DoBlendedAction`
  0x48C087 and `DoDiscardAction` 0x48C8C1 on the creature another creature picks up or puts down (a scan of every
  `[reg + 0x51EC]` in the image). The player's hand does not set it.
- **The lean** (0x4EBF33..0x4EBFB7): a = (h(x+d) − h(x−d)) × 0.6 / (d + d) and b = (h(z+d) − h(z−d)) × 0.6 /
  (d + d), each operation a float; then a ≥ 0.3 → 0.3 and "not > −0.3" → −0.3 (`fcomp`, `test ah, 1` /
  `test ah, 0x41`), the same for b. A lean that is not a number takes −0.3: a body of radius 0 on flat land (0 / 0)
  leans −0.3 both ways (oracle). No smoothing, no other limit, no foot placement.
- **The matrix.** Rows (cos, 0, sin), (0, 1, 0), (−sin, 0, cos) from the angle +0x84 (`fcos` / `fsin`, each stored
  as a float), post-multiplied (`LHMatrix::PostMultiply` 0x7FAFF0, this = this × M) by M = rows (1, a, 0), (0, 1, 0),
  (0, b, 1): each row r becomes ((r.z × 0 + r.y × 0) + r.x, (r.x × a + r.z × b) + r.y, (r.x × 0 + r.z) + r.y × 0),
  in that order (signed zeros: the −0 of −sin 0 becomes +0). Then all twelve cells × +0x94 and the position added to
  the translation (0x4EC024..0x4EC0A9). A point of the body rises by a × its world x offset + b × its world z offset:
  a shear in world axes after the turn.
- **When.** Its callers: `UpdateBuffers` 0x4EC5AD (the pose, from `UpdateTime` and `ResetLook`: once a game turn),
  whose world bones the drawing blends, and `DoCatchMain` 0x488600, `DoKickingAction` 0x488812, `AttemptAttack`
  0x489DEE and `DoFightActionAction` 0x48A77F (their points). So the drawn body leans as its two turn poses blend;
  nothing recomputes it per frame.
- **Oracle**: `GetPositionMatrix` run natively on a creature buffer whose vt+0x28 is a made-up land
  a x + b z + c x z + d (code written into the stub pool), nine cases: flat, along x, along z, both with the body
  turned and scaled, both limits, curved, carried, radius 0, a lean of exactly 0.3. Pinned in `test_creature_lean`.

## openblack

The creature's movement and body over time are components: `CreatureLocomotion` (speed towards a fraction of the top
speed, slower uphill, walk and run blended by distance), `CreaturePhysiology` (age, growth, energy and fat,
tiredness, thirst, warmth through `gutils::SigmoidThreshold`, strength, poo), `CreatureSpells` (freeze, small, big,
weak, strong, fat, thin, invisible, nice, nasty, itchy and the unfinished ones; each eases in, holds and eases out),
`CreatureFight` and `CreatureFightHud` (the duel in a circular arena and its panel) and `CreatureThrow` (the release
velocities). The porting decisions (what of raffclar's creature code is taken and what is left out) are in
[Port decisions: the creature](../project/port-decisions-creature.md).

**The turn on the spot.** `creature_locomotion::SpinsFor(yaw, targetYaw, lengthMs)` gives the spins and the blend's
length, `TurnIsOver` the 1e-4 end and `SpinlessYaw` the heading without spins, all in the original's yaws (a
locomotion heading h is the yaw −h, so the left spins 6, 7, 8 turn a heading down). `CreatureLocomotionSystem`'s
`StartTurn` is `InitialiseTurning` for both ways in, by the species' animations' own lengths; its `TurnStep` checks
`TurnIsOver` first when turning to face something (`TurnToFace`), and when the time is up puts the heading on the
target and, turning to face, leaves the turn to end at its next step. Tests:
`CreatureLocomotion.ATurnOnTheSpotBlendsTheSidesSpinsByTheAngleAsTheOriginal`,
`ATurnThatIsNotANumberTakesTheFirstPairAndTheIndefiniteLength`,
`WithoutAllThreeOfTheSidesSpinsATurnHasNone`, `ATurnToFaceIsOverWithinATenThousandthOfItsTarget`,
`WithoutSpinsTheYawComesRoundByTheShareOfTheStandGoneOfWhatIsLeft`.

**The lean.** `creature_lean` (`src/Creature/CreatureLean.h`), pure: `Sample` (the four points, the carried case),
`FromHeights`, `Leaned` (the product in the original's order) and `Model` (then `affine::Model`'s scale and
position). `creature_pose::LeanedModel` takes the land (`GetHeightAt`, as the sway's kick), the radius
(`creature_morph::Radius(DrawnSize, restHeight, reach)`) and `HeldByCreature` for +0x51EC; without the land or the
body's measures it places the body upright. `CreatureAnimationSystem::PoseTurn` places the turn's pose with it, so
the drawn body (the blend of the turn poses) and every reader of the turn's pose lean.

#### What openblack does with the turn's pose

- `ecs::components::CreatureTurnPose` (`src/ECS/Components/CreatureTurnPose.h`) on the creature: `current`
  (+0x5178), `previous` (+0x5174) and `frozen` (+0x5180 while +0x5270 is set). Empty bones are "no pose yet": a
  reader then has no bones, as a spell then keeps its old hand position.
- `creature_turn_pose` (`src/Creature/CreatureTurnPose.h`), pure: `Step` (the step above), `LivedMs` (the turn's ms
  at the playback scale), `ClockFor` / `ClockOfTurn` (the three clocks from one whole number, pinned to the oracle's
  numbers; `ClockOfTurn` gives the turn's advance its clock), `Advance` (once per pose: previous = current, current =
  the new one, and the first time previous too), `Freeze` / `Reconnect`, `Safe` (`GetSafeBuffer`), `BlendInto`
  (`fn_004EECD0`), `Drawn` (`PrepareForDrawing`'s blend, f taken up to 2), `InWorld`, `MirrorBone` (the file's table,
  `CreatureRig::fileTail`) and `LeafMean` (`fn_0083A0E0`; openblack's skeletons put a bone's first child at the
  lowest index and its siblings in index order). Tested with made-up bones (`test_creature_turn_pose`).
- `creature_layers` (`src/Creature/CreatureLayers.h`), pure, in whole ms: `AdvanceOnce` (`AdvanceSimple`),
  `AdvanceLooping` (`AdvanceCyclic`), each giving the new time and the stretches passed (`TimeAdvance::Passed`, in
  the order `CheckSounds` looks at them, so the sound queue of `CreatureSoundQueue` can take its moments from them),
  and `AdvanceFaceByTurn` (`OperateFace`, the `>> 2`) over a `TurnFace`. Pinned to the oracle's cases above
  (`test_creature_layers`). On them, the layers' turn: `AdvanceFaceLayerByTurn` (the face's times taken to whole ms,
  and relaxed, its cue cleared, once let go of), `AdvanceGestureByTurn` (the gesture's cut to its end, above; a
  blow's wobble too) and `AdvanceBodyByTurn` (an action played once, a sequence's start and its end are over the turn
  their step reaches their end, played to it by `AdvanceOnce`; until then, and in the loop, `AdvanceLooping`). Each
  gives what of its animation it passed. The float `AdvanceBody`, `AdvanceFace` and `AdvanceGesture` are left to the
  Creature Cave's copy, moved by its frames.
- **The turn's advance.** `CreatureAnimationSystemInterface::AdvanceTurn`, called in `creature_loop::ProcessTurn`
  just before `PoseTurn` (where `UpdateTime` advances, before its `UpdateBuffers`), moves each creature's layers once
  a turn by `ClockOfTurn(playbackScale, MsPerTurn, DrawnSize)` (neither flag is set by anything yet), in the
  original's order (`AdvanceTurnLayers`): the face by the ms, the gesture then the wobble by the step, the breath by
  the seconds, the body's action by the step, the head by the seconds. The work is in `CreatureAnimationSystem`. A creature whose rest pose is not built for
  its shape, or without its rig or stand, moves nothing. The frame (`Update`) no longer moves any layer or the
  breath: it samples the layers as the turn left them (before, the frame's `AdvanceLayers` moved them by its
  `FrameClock`, in float ms, about a turn late). `AdvanceTurn` is characterised with fake layers: one turn of 100 ms
  at size 1 moves the body by a 117 ms step and the face by 100 ms; a frozen case at scale 0.5 lives 50 ms, step 58. The blink counts the ms the creature lives in the turn
  (`LivedMs`), not the 100 of the turn. What each layer passed (`TurnPassed`, in the order the original checks its
  moments) is not read yet: the sounds still take their moments from the frames (see
  [The creature's sounds](../audio/creature-and-interface-sounds.md#the-creatures-sounds)), and the original's
  `PlayQueuedSoundEffects` comes before the face.
- `CreatureAnimationSystemInterface::PoseTurn`, called last in `creature_loop::ProcessTurn` (after the miracles and
  the autoscale, which change the size, as `ProcessSpells` and `SetSize` come before the original's pose), fills
  `current` through `Advance` with the body sampled as it plays at the turn's end
  (`creature_loop::AnimationInputsAt(1)`: the fight's, the walk's and the object actions' animations at the whole
  turn, in the frame's order), placed by the creature's Transform at the scale it is drawn at, leaned onto the land
  there (`creature_pose::LeanedModel`, `creature_pose::DrawnScale`; [The lean onto the slope](#the-lean-onto-the-slope)).
  It only samples: no animation clock moves. The spell info reads this turn's pose, as openblack's creature loop runs
  before the magic's turn (`Game.cpp`, then `creator::UpdateSpellInfo`).
- **What openblack draws now.** Every frame, after the body is posed, `CreatureAnimationSystem::Update` gets the turn
  fraction f (the game clock's, 0..0.99, passed from `creature_loop::UpdateFrame`) and sets
  `CreatureAnimation::drawnBones` to `Drawn(pose, f)` (`PrepareForDrawing`'s blend) seen from the body's drawn
  matrix: `creature_turn_pose::RelativeTo(row, bones)`, the row's inverse times each bone, in double. openblack's 4×4
  matrices blend the same twelve values and keep the bottom row (0, 0, 0, 1). The instance row
  (`ecs::DrawnBodyModel`) stays what it was, so the land melting, the light, the culling, the sorting and the hair's
  light keep it; the GPU's row × drawn bone is the original's blended bone, rounded to a float, and the body is placed
  by the blend alone. So the body is drawn one turn behind the game, between its last two turn poses, and the pen's
  size grows smoothly within each turn instead of stepping once a turn. `creature_turn_pose::BonesDrawn` chooses the
  drawn bones over the frame's own pose (`boneMatrices`) for the body and its shadows (`ecs::PosesByInstance`,
  `creature_draw::Body::bones`), the eyes, the hair's roots and the drawing of a held object (not its per-turn
  placement, which is hashed).
- The drawn bones are empty, and the body is drawn as the frame poses it, when it has no turn pose yet, when a turn
  pose has not as many bones as the mesh, and while it is drawn in the hand or in the physics (both move it every
  frame; what the original draws for a held creature is open). The frozen copy is not drawn: `PrepareForDrawing`
  reads +0x5178 and +0x5174 directly. A teleport (`ecs::SnapTurnStart`: a script's SET_POSITION, the hand's release,
  the end of the physics) drops `current` and `previous`, keeping `frozen`, so the body is drawn as the frame poses
  it until it is posed for a turn at its new place; the original re-poses at once and copies +0x5178 into +0x5174
  (`ForceMoveMapObjectWithoutWalking`), so ours differs for at most the rest of that turn. Nothing of this is hashed
  and no random number is drawn for it.
- **Which pose each reader takes:**
  - **The footprints**: openblack keeps the drawing's blend in the world (`CreatureAnimation::drawnInWorld`,
    `CreatureAnimationSystem::DrawnInWorld`) beside the drawn bones, and `creature_loop::UpdateFrame` plays the
    sounds the drawing has reached before `CreatureAnimationSystem::Update` blends the body again, so
    `FootprintSystem::Step` lays the print under the body as it was last drawn. A body not drawn from its turn poses
    (in the hand, in flight, before its first turn) lays it under the frame's pose and its drawn matrix, as before.
  - **The hand's touch**: the capsules and stroke parts (`CreatureHandSystem`'s `BodyOf`, `PartsOf`) take
    `creature_turn_pose::BonesDrawn` under the drawn matrix; the hand's update runs after
    `creature_loop::UpdateFrame`, so it sees that frame's blend. The capsules themselves stay a stand-in for the mesh
    test.
  - **What the creature holds**: the per-turn placement of the held thing's Transform
    (`CreatureObjectActionSystem::ProcessTurn`, before the turn's pose) takes the palm of `CreatureTurnPose::current`
    in the world, so the Transform is where the original places it as it lets go; a body not posed for a turn yet, or
    posed with other bones, is read as the frame poses it under its Transform. The drawing already took the drawn
    bones (`UpdateHeldDraw`). Its turn stays the creature's Transform's times the held turn, not the hand bone's
    normalised rotation (Pending).
  - **The leash's collar**: `CollarPoint` (`LeashSystem`) takes `creature_turn_pose::BonesDrawn` under the drawn
    matrix, in the frame's leash update, after the creatures are drawn; for the rope's end, a creature it is tied to,
    and the rope's seed alike.
  - **How far through the body's action is**: `CreatureMindSystem::ForceAction` reads `body.timeMs`, which the turn's
    advance moves in whole ms, so it already reads the same time; the test is `creature_feedback::MayInterrupt` (at
    or below is refused; the float share compared with the slap's float and the stroke's double,
    `k_SlapInterruptsAfter`, `k_StrokeInterruptsAfter`). Before, a share exactly at the slap's 0.35 was let through.
- Not yet as the original:
  - what still reads the frame's time or pose: the frame still samples the body every frame (`boneMatrices`), which
    no game reader takes now but the fight's body; the sounds take their moments from the frames' layer times, which
    now jump once a turn, so they bunch on the frame after a turn until they are queued by the turn; the object
    actions step by the fixed 100 ms at the float rate; the fight's moves by the turn's ms without `GetFightMul`'s
    casts; the walk's cycles (`BlendGait`) and the stand slot (by the breath, now per turn) as floats, where the
    original cuts each to whole ms (a 117.5 ms step is 117 there); the object actions drawn through `SlotsAt`; the Creature Cave's copy by
    its frames as floats;
  - +0x4A90 (real-speed animations) and +0x5730 (`EndAnyAnimsRapidly`, which doubles the step and halves the
    gesture's share) are not set by anything, and `UpdateTime`'s skip gate ([0xD00DE4], the mind's +0x20CE8) is not
    modelled;
  - the pose is made in mesh space and placed, with the slots blended as local poses and without the head, hands,
    feet and eyes scaled by size;
  - the fight's body still reads the frame's pose under the Transform, and nothing freezes the pose yet;
  - the leash's rope is seeded at the first frame it is worn, from the drawn body; the original seeds it as it is
    laid or tied (`GLeashStatus::SetOn`), from +0x5178;
  - what the creature holds is turned with the creature's Transform, not by its hand bone's normalised rotation as
    `FillHeldObjectMatrix` turns it; the palm at an action's moment, when the animations can't give it, falls back to
    the frame's pose.

### Test hooks

None of the page's own; the creature's debug controls are listed in [Debugging](../project/debugging.md).

## Pending

- **The turn on the spot, not compared yet**: our `TurnToFace` faces at once a point within π/12
  (`StartTurningAction` has no such test; its callers have their own); the step +0x48BC is ours in float
  milliseconds (queued with the pen size's readers); the steps' pair (`GetStartState`'s motion 5 reads the same two
  constants and builds a `CAnim` too) keeps `PairFor`'s division and clamp.
- **The lean onto the slope** ([The lean onto the slope](#the-lean-onto-the-slope)), ported, still open: the readers
  that still take the frame's pose under the Transform (the fight's and the catch's points) stay upright until each
  reads the turn's pose; so does the body while it has no turn pose, or is drawn by the hand or the physics. Whether
  the original's body in the player's hand leans (it is posed at its own position through the same matrix, but what
  the hand draws is open). The land's height is `GetHeightAt`, as for the sway's kick, not compared here with
  `LH3DCreature::GetAltitude`'s map coords; the angle's bits (+0x84 against our Transform's `affine::AngleY`) are not
  compared.
- **The body posed for the turn** ([The body posed for the turn](#the-body-posed-for-the-turn)):
  - the gates [0xD00DE4] and [0xD00DE8] are 0 at load and no writer is found; the creature plainly animates, so only
    a run-time read settles it (what turns them off: a mind turned off by a packet, the citadel?); **verify with
    xref/oracle**: `python xref.py writers 0xD00DE4`;
  - [0xBE007C] (the extremity scaling on, 1 in the file): only the read at 0x4EC0C1 was found;
  - several creatures: the original poses each inside its own `ProcessState`; queued with the creature's turn inside
    `Living::ProcessLiving`;
  - whether the body's position moves inside `UpdateTime` (`StateAction`) or in `ProcessSubAction`, and
    `fn_004EBE10`'s operand order;
  - +0x4A90: which bodies play at the real ms (its writers are not read); +0x5730 is `EndAnyAnimsRapidly`;
  - whether our `DrawnSize` is the original's size +0x90 at `UpdateTime`'s time (`SetSize` runs before it in
    `ProcessState`);
  - the layers: which of openblack's face, gesture and wobble are +0x5738 and +0x49AC (the sway's leans 0xC2..0xC5
    and its springs are ported, [Struck by a thrown thing](hits-and-catch.md#struck-by-a-thrown-thing));
  - the purposes of `fn_0048B650`, `fn_0048F200` and `fn_0048BAB0`; the caller of the function at 0x4912F0; when
    `fn_00490460` runs in the turn;
  - the fight's look-ahead (0x48A791..0x48A8C2) writes +0x5180 without checking +0x5270;
  - the drawn bones' (+0x47F0) readers are not all known (the creature-side ones found: `fn_0048F550`,
    `fn_0068E430`, `DrawFightSparkles`, `Creature::Draw` 0x517DC8, `fn_00483290` 0x4834C6, `fn_005CFDE0`,
    `fn_005D3DD0`, `HandStateCreature::Update`); `fn_0048F280` places the carried object twice, from two poses;
  - what `fn_0048F180` does (from `FallingSpell::Close` 0x526580);
  - the hair's per-frame time (`fn_00848350`): its argument at 0x4ED553 ([esp+0x14]) is not decoded;
  - the freeze and reconnect calls, the re-poses inside `StateAction`, the two per-turn caches and every reader wait
    on the 3D creature's states.
- **The animation clock, still to port** (each its own measured commit): the object actions take the turn's step
  as whole ms (`action.timeMs += Step(LivedMs, size)`), which moves the catch's grasp and the kick's contact moments
  by up to a turn; the fighters by `(long)(GetFightMul() × IntTimeInc)`; the walk's `(long)` cycle times and the
  integer run ratio; the sounds queued by the turn, in the same joint as anything that makes the layers jump once a
  turn; a closed-by-default readout of the turn clock (ms, step, seconds) per creature in the creature debug window.
