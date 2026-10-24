# Flocks

## Summary

The flock miracles release a stream of animals from the hand: FLYING_FLOCK (24) gives doves for a good god and bats
for an evil one, flying to a point far ahead of the camera; GROUND_FLOCK (25) gives wolves that run down a corridor,
hunting what they find in it. Each animal is a real animal of the game with its own spell class, fading out instead
of dying when the miracle ends, when it arrives or when a shield stops it. Particles follow each animal (a trail of
glows, a rain of feathers or dust).

openblack ports the spell, its exit loop, the destination, the fade-out, the wolves' corridor and hunting, and the
flocking particles faithfully. The wolves chase but do not get to the leap (see Pending). The flocks' particle rules are on [Flock rules](../physics/particle-rules.md#flock-rules).

## The original

### Flocks

The FLYING_FLOCK (24, doves or bats) and GROUND_FLOCK (25, wolves) miracles. The animals are real animals; what their
spell classes SpellDove / SpellBat (AnimalDove.cpp) and SpellWolf (AnimalSpellWolf.cpp) do differently is below.

#### The spell

**Faithful.**

- **Classes:** `SpellFlock` (SpellWithObjects, ctor 0x7231C0 / fn_00723210) and its two children: `SpellFlockFlying`
  (AllocSpell 0x723100, 0x120 bytes) and `SpellFlockGround` (AllocSpell 0x723180, 0x110 bytes). Data: +0xF4 the
  Flock, +0xF8 created, +0xFC accumulator, +0x100/+0x104/+0x108 last exit point (MapCoords and height above the
  ground), +0x110 the PSys of the cast (only the flying one). `SetToZero` of the spell data is 0x7239E0.
  `SpellFlockGround::InitWithPos` 0x724220 = `SpellFlock::InitWithPos`.
- **Constants:** 12 animals per second (GetNumToEmitPerSecond 0x7230E0 / 0x7230F0), angle variation 2.0
  (0x723550 / 0x723560), distance `info+0x60` = 800 m (0x723530 / 0x723540), how many = `fistp(numberToCreate ×
  GetTribalPower)` (0x723B80 / 0x724250: 12 doves, 14 wolves; rounding to nearest even), hunting radius `info+0x64` =
  45 m (fn_00723170). `GMagicFlock*Info` offsets: numberToCreate +0x58, alignmentSwitch +0x5C (alongside +0x60 /
  +0x64).
  - Constant addresses: 12 per second at 0x9819B0 / 0x9819B4; angle variation 2.0 at 0x9819C0 / 0x9819C4; jitter 0.1
    at 0x9819CC / 0x9819D8; scales 2.8 / 3.0 (birds) at 0x9819D0 / 0x9819D4 and 1.5 / 2.0 (wolves) at 0x9819DC /
    0x9819E0, so the extra is `GameFloatRand(top − base)`.
- **`InitWithPos` 0x7232D0:** last point = the hand (PSysProcessInfo +0xC) in MapCoords (ftol × 6553.6) and its
  height above the ground; `new Flock(pos, GFlockInfo 0xC5E624, player, 0xABA52)` (Flock::Flock 0x52F780: radius
  0x50, and the spell sets the flock distance = the radius, 80); then `Spell::InitWithPos`. The flying one (0x723A80)
  in addition, if this computer's interface casts, creates SF_FlockFlyingCastGood/Evil (122 + evil) at the point,
  without a spell (`GJPSysInterface::Create(NULL, ...)`), with its player; `Draw` 0x724100 advances it each frame and
  it closes (vt 0x118) when all the animals are there.
- **Good or evil:** player's alignment (`GPlayer+0x60 → +8`) `< alignmentSwitch` (−0.2) → evil: particles 125
  FlockFlyingRainEvil (otherwise 124, GetParticleType 0x723A30) and **SpellBat** (GAnimalInfo 21), otherwise
  **SpellDove** (20). The ground one is always 126 FlockGroundDust (0x724290) and **SpellWolf** (22).

##### The exit loop

Flying `Process` 0x723BC0, Ground 0x7242A0, each turn while created < N:

1. the spell is placed where the leader is (the first member of the Flock, +0x40 → +8);
2. `emit += 12 × 100 ms × 0.001` (in float); the exit segment goes from the last point to the current hand;
3. while `created < emit`: `created++`; `f = (created − emitBefore) / (emit − emitBefore)`; S = old +
   `fistp((new − old) × f)` in x, z and the interpolated height; the birth point carries a jitter
   `GameFloatRand(0.2) − 0.1` m in x and then in z (the destination does not);
4. it is skipped (counting all the same) if the jittered point is off the map (`MapCoords::InBounds` 0x6042C0), if a
   human casts and `CalculatePlayerInfluence(spell position, player, 0, 0, 1) <= 0`, or if fn_00723570 fails;
5. the animal: `fn_00419D10(S, info, no town, the Flock, age 0)`; scale = `GetScale()` (1, 0x4247E0) × 2.8 +
   `GameFloatRand(0.2)` (birds) or × 1.5 + `GameFloatRand(0.5)` (wolves), the random first; `SetGameAngle(S → T)`
   (GetAngleFromXZ 0x74D240); to the spell's list (at the front) and, all of them, `AddTarget` to the main PSys;
6. **birds:** height of the point = that of the hand; T at `altitudeNormal` (GAnimalInfo +0x278, 45 m). The leader:
   `SpellEvent{2, its position, its movement (zero, as it has just been made), 1.0, no shield test}` (reaction 37
   ReactToImpressiveSpell; `SpellFlockFlying::Process` 0x723FCD..0x72403F through the spell's vt +0x52C, the same in
   `SpellFlockGround::Process` 0x7246A5..0x72470F: the movement is the leader's `GetMovementDirection` vt +0x168,
   `MobileWallHug::GetMovementDirection` 0x60C040, whose step +0x64 / +0x6C the `MobileWallHug` constructor 0x474890
   zeroes and only `RebuildMoveByStep` 0x609D10 writes, after the event),  `SetupMoveToPos(T, START_WANDER
   0x1F)`, `SetState(0, SPECIAL_MOVE_TO_POS 0x2C)` and the Flock +0x78 = FOLLOW_FLOCK, +0x80 = 3 (formation), +0x7C =
   DECIDE_WHAT_TO_DO. The others: `SetupMoveToPos(the leader's GetDestPos (+0x80), 0x1F)` and the same 0x2C. **So the
   ±2 rad fan is only the angle each bird is born with; they all fly to the leader's T**;
7. **wolves:** born on the ground (height 0, T height 0) with `SetPlayer(caster)`, a cloud `CreateSpotVisual(S, 9
   MAGIC_OBJECT_CREATED, 1, 0)`; the leader does the `SpellEvent 2` and `fn_00420F50(S, T, 45)`; the others
   `fn_00420F50(S, the leader's GetDestPos, 45)`.
8. every bird is a target of the rain effect: `fn_00723160` = 1, i.e. `created % 1 == 0`.

- **The birth angle** (faithful by reading): `SetGameAngle` 0x60DA90 (+0x5C and `SetYAngle`) from `GetAngleFromXZ`
  0x74D240 (jittered point, T; 0x7245E8..0x7245FB) = `LHArcTan` 0x74D0C0 of T − S.
- **fn_00723570 (the destination):** direction d = that of the camera (PSysProcessInfo +0x18) if a human casts,
  otherwise `castPos − hand`; y = 0; (1, 0, 0) if |d|² < 0.0001. Side: human → v = normalise(S − castPos) and `cross
  = v.z·d.x − v.x·d.z`: +1 if > 0.1, −1 if < −0.1; otherwise (and for the AI) +1 with created even, −1 odd. Angle θ =
  2 · created · side / N; d rotated θ in Y (fn_00518BF0, row vector: `(x cos − z sin, x sin + z cos)`), set to the
  length (fn_006805F0). T = S with its 10 m cell (the high word) moved to `ftol((cell·10 + d)/10)` and the part inside
  the cell the same; while T is outside and the next distance (half) exceeds 10 m it is repeated with half (800, 400,
  ..., 12.5). Returns `InBounds(T)`.
- **`SpellFlock::Process` 0x7233D0** (the ending of both): if it is not closed, for each member
  `fn_006D0C20(+0x2C, +0x14, GetRadius)` looks for a shield crossed this turn; if there is one, `SpellEvent{4, its
  position, target = the shield's spell}` (`costPerShieldCollide` 50) and **if it answers 0 (the shield holds) the
  member `SetDying` (vt+0x6A4), which in these classes is the fade-out**; with the shield broken it passes; in both
  cases a spark on the shield (fn_006D0AF0). Then `SpellWithObjects::Process` 0x721290 (its loop 0x721040 calls
  `Animal::ProcessBySpell` 0x417700 = `ProcessFadeOut`), and **the flock** (its +0x14, the centre of its domain) is
  placed where the leader is (0x7234F2..0x723519; not the spell, which only moves at the start of each class
  `Process`, 0x723BE6 / 0x7242C8).
- **Cost** 0x723240: `members × costPerEvent (0) + costPerGameTurn` (40 / 35). **CloseDown** 0x723270 =
  SpellWithObjects 0x721300: `CoreCloseDown` and `SetDying` to each object (GetSetObjectsDyingOnCloseDown 0x55CF50 =
  1). **ToBeDeleted** 0x720FD0: CloseDown, the list out; the flying one deletes its cast PSys (fn_007239B0).
- **`NeedsContinualPackets` 0x723280**: true while `created < N` if a human casts and it is the interface's player;
  otherwise, `Spell::NeedsContinualPackets` 0x7214C0. It decides whether the interface sends packets with the hand.
- **The real duration with a hand seed** (inferred from the chant rules, measured): 5000 initial chants / 40 (doves)
  or 35 (wolves) per turn, without recharge: 12.5 s and 14.5 s, not the 25 s / 60 s of the timer (the script cast is
  by the neutral player, which replenishes: there it closes by the timer).

#### SpellDove, SpellBat and SpellWolf (what concerns the spell)

- **The fade-out ✅ faithful:** alpha 0..255 in a LH3DLib Zoomer (SpellDove +0x148..+0x174, SpellWolf
  +0x168..+0x194; starts at 255, 0x41F280 / 0x420930). `SetDying` (SpellDove 0x41F5C0, SpellWolf 0x420CF0) **never
  calls Living::SetDying**: if the destination is not yet 0, `vt+0xBD4` (fn_0041F2F0 / fn_00420A20, the
  `SetDestinationWithSpeedAndTime(0, 0, t)` of the Zoomer: quartic from the current value and speed) with t =
  `GetNumTurnsToDieOver` (20, 0x41F620 / 0x420D50) × 100 ms = 2 s. `ProcessFadeOut` (0x41F4C0 / 0x420BF0) advances
  0.1 s per turn and with the alpha exactly 0 does `ToBeDeleted`. "The destination" is the second field of the Zoomer
  (+0x14C / +0x16C; the value is +0x148 / +0x168): once started, another `SetDying` (shield, CloseDown, the arrival)
  does not restart it. The mesh colour is `fistp(alpha) << 24 | 0xFFFFFF` and translucent if it is not 255 (SetColor
  0x41F630; SpellWolf::Draw 0x51C6EC); the alpha only changes in the turn. The animal keeps moving while it fades.
- **Wolf, the corridor** (fn_00420F50, faithful): normal `(DZ, −DX)/|D|` of D = destination − start ((1, 0) if |D|²
  < 0.0001; it is the normal, not the direction), offset `−normal·start`, half width 45 m, `+0x80` and `+0x148` =
  destination (0x421054..0x421081; not the start) and then `SetRunToFinalDest` 0x4209C0 (0x421084: speed = scale ×
  info.speed4 × 1.1, `SetupMoveToPos(+0x148, SET_DYING)`) **in the spell's turn** (the wolf runs from its first
  turn). Those that follow the leader take its `GetDestPos` (vt 0x860 = `MobileWallHug` +0x80, 0x416F70; call at
  0x724737): **while the leader hunts, +0x80 is the prey** (`SetupMobileMoveToObject` 0x60ACE3 writes its
  `GetWorkingPos` there), so those wolves run to it and fade on arriving at 30 m (read that way; in Land 1 next to
  the town almost all go away in 2-4 s).
- The class ctor (AllocSpell 0x4207F0) does not use the age of fn_00419D10: `fn_0041FD30` receives `grownUpAge + 1`
  (0x420816..0x420822: an adult, which hunts live prey) and the hunger +0xE4 = `info.hunger` (150,
  0x420885..0x42088E).
- `IsPosOnCorridor` 0x420E10: `|normal·p + offset| <= 45` and the point no more than 45 m behind the corner of the
  wolf's 10 m cell along the corridor (not of the start).
- `SpellWolf::MoveToPos` 0x421300: `Living::MoveToPos`; if the hunger +0xE4 `>=` info+0x20C (the ctor 0x4207F0 sets
  it that way: hungry from the start) `ReactToAnimalFoodNeeds` (vt+0xBC0, the lion's hunt); and less than 30 m
  (0x8BF51C, `GetDistanceInMetres` 0x74CD70 in XZ) from the final destination, `SetDying` (the fade-out).
- `IsHuntingTargetValid` 0x420D60: a Living that is not in DYING/DEAD/DOWNED/BEING_EATEN (0xE, 0xF, 0x11, 0x12) nor
  lying down (+0xB4 & 0x80), inside the corridor and with `IsPosValidForTurnAngle` (vt+0xB3C); in addition the wolf is
  not fading (+0x16C ≠ 0). It is vt+0xBB4: called by the prey search fn_004196D0 (0x419715) and
  `Animal::HuntingMoveToPos` 0x418DB0 (0x418E36, if not valid → vt+0xBB8). The SpellWolf branch of `Abandon` is
  `HuntingMoveToPosAbaondon` 0x420F30.
- **The wolves' speed**: their `SetSpeed` does nothing (0x4209B0), so they run at ~1.5 m/turn and the radius of
  `SetTowardsAngle` (2 × speed / turn, 0x418681) is ~29 m; inside it the reduction at 0x4186B4..0x4186FC almost
  cancels the turn.
- **The wolves lean into their turns**: SpellWolf's own `GetTimeToBank` 0x420EF0 is 1 s ([0x8BE8E0]) and its
  `GetBankAngle` 0x420F00 `min(|speed x turn| x 0.5 / 20000, 0.5)` ([0x8BE8E4], [0x8BE8E8]), with the speed (+0x5A,
  MapCoords a turn) and the turn `Animal::SetTowardsAngle` takes (0x418733..0x41875B, 0x4188D6..0x4188FE; minus
  for a turn the other way); `SpellWolf::Draw` 0x51C560 steps the bank and rolls the drawn matrix by it, as the doves'
  draw does. The land's wolves (`Animal`'s 0x41B1D0 / 0x41B1E0) never lean.
- **Others:** SpellDove/SpellBat `GetTimeToBank` 0.5 s, `ReactToAnimalNeeds` 0x24, SpellBat
  `CanBeFrighteningToCreature` = 1; SpellWolf `SetSpeed` does nothing, `DecideWhatToDo` / `Wander` /
  `HuntingMoveToPosAbaondon` = SetRunToFinalDest.
- The wolves are `GAnimalInfo` 22 SpellWolf at 0xC50DB8. Per-animal ctors: SpellDove 0x41EAC0, SpellBat 0x41EF60,
  SpellWolf 0x4207F0. SpellBat's vtable 0x8BABC4 has the same vt +0x6A4 (`SetDying`) entry as SpellDove (0x41F5C0).

#### The particles

**Faithful.**

- **UR_FollowTargets** 0x6A04B0 (ctor 0x6BFA20, properties 0x6B1DD0): flags 2 without 4 (waits for its targets until
  closing). If the effect is not closing, one new atom per step: with `RemoveTargetFromManager` (1) it takes the last
  added target (TakeTargetObject 0x671030), otherwise the first without removing it (fn_00671080); with
  `UseLHPointTargets` (0) and no objects, a point (which it does not use). The atom stores its target, its rule scale
  = the target's scale, and `SoundCreate` plays except with `SoundOneOnly` (1) if it is not the only atom in the
  collection. Each step each atom is placed on its target (GlobalToLocal); a target that went away is forgotten and,
  with `RemoveAtomWhenTargetDies` (0), its atom is deleted. In SF_FlockFlyingRain* (SOUND_SPELL_DOVES / _BATS) and
  SF_FlockGroundDust (SOUND_SPELL_WOLVES) the UR_WillowWisp trail (3 or 5 glows) hangs from each bird or wolf.
- **UR_Flocking** 0x683580 (ctor 0x683470, properties 0x6ABCD0; used by SF_Forest, SF_Butterflies*, SF_Flies* and
  SF_CreatureSpellItch*): a flock of particles. Per collection it stores the flock velocity V (CollectionData
  0x560EC0 +0x24, starts at 0). Each step (dt that of the effect, [0xD4E0EC]):
  - the mean of the velocities and of the positions C of its atoms; the target P = the parent atom (or the origin) in
    the collection's frame (fn_00674B10);
  - `V = V (1 − dt K_FlockDamping) + unit(P − C) K_IdealVel f(|P − C|, IdealVelAccnType, F_InvertAccnIdealVel) dt`;
  - for each atom `a` that has another: the sum over the others b of `unit(b − a) f(|b − a| LocalScale,
    NeghbourAccnType, NeighbourAccnInvert)` × `K_NeighbourAccn / n`; the nearest gives the avoidance `unit(nearest −
    a) f(|..| LocalScale, 2, no) K_NeighbourAvoidance`; the centre the attraction `unit(C − a) f(|C − a|, AxisChosen,
    F_InvertAccn) K_CentralAttraction`; and the matching `(mean − (v_a − V_before)) K_VelocityMatching` (×
    LocalScale). Acceleration = neighbours + matching + attraction − avoidance, clipped to `F_MaxAccn`; `v' = (v_a −
    V_before)(1 − dt K_Damping) + accel·dt + V`, clipped to `F_MaxVel`;
  - the orientation: with `SpriteRotation` (1 by default) `SetAngleY(atan2(−y, x) + π/2)` of v' seen from the camera
    (fn_006840E0; the camera is `g_world_to_camera` 0xEA1D28, written at 0x81A114 by the camera set and set to identity
    at 0x818E0B, so x = v·right and y = v·up); otherwise, `UpdateBanking` 0x684160: `SetAngleY(−π/2) × Rx(bank) × Rz(−pitch) ×
    SetAngleY(heading)` (product of LHMatrix rows), heading = atan2(v.z, v.x), pitch = atan2(v.y, |v.xz|) ×
    ReducePitchBy, bank = atan((a.z v.x − a.x v.z) / |v.xz| / GravityForBanking) with a = (v' − v_a)/dt; local +Z
    faces the velocity;
  - at the end each atom advances `pos += v·dt`.

  The law fn_00683520 (d, type, invert): x = max(d, 0.01) × ScaleModifier; type 0 → 1, 1 → x, 2 → x²; 1/that unless
  inverted (the types are integers 0..2, the setters 0x6AC0D0.. do not accept others; the file writes them as
  FLOAT). By default (ctor): K 1 / 1 / 0 / 1 / 0 / 0 / 0, F_MaxAccn 10, F_MaxVel 100, GravityForBanking 10,
  ReducePitchBy 1, ScaleModifier 1, types 0 / 2 / 1, SpriteRotation 1.
- **EventConditionAtomNearVillagers** 0x67D8E0: the atom's position +0x80 (in metres, as is) in MapCoords is on the
  map and one of the objects of that 10 m cell is a villager. SF_FlockGroundDust declares it but no rule uses it.
#### More addresses from the code

- The destination's move by a distance d within the cell: fmul 10 [0x9819A4], fadd d, fdiv 10, `__ftol`, and the word
  stored back; all of it in float.
- `SpellWolf::IsPosOnCorridor` 0x420E67 reads the cell's high words unsigned and multiplies by 10 [0x8BE8D4].
- The cast's effect is SF_FlockFlyingCast (Good or Evil), made at the cast point with no spell.

## openblack

Status: **done** for the spell, the animals' spell behaviour and the particles; the wolves' leap is open (Pending).
Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **The exit loop's running total** is summed in floats, each product and sum rounded as the game's FPU (24-bit
  precision) rounds them (`spell_flock::NextEmitted`): 1.2000000477 a turn, exactly 6 at the end of the fifth turn, so
  six animals are made by then and eight by the sixth (test `FlockEmit.TheTotalStepsInFloatsAsTheGamesFpuSumsIt`).
- **A birth point's nudge and height** are worked in floats, each step rounded as the game's FPU (24-bit precision)
  rounds it (`spell_flock::Jittered`, `spell_flock::EasedHeight`; 0x723DB6..0x723E42 and 0x723D81..0x723DA1): the
  coordinate x 10 rounded, into metres, plus the draw less 0.1, back x 65536 / 10 and truncated. Worked in double they
  land a unit or two of MapCoords away in more than half the cases (tests `FlockBirth.*`).
- **The spell**: `Magic/Spells/SpellFlock` (`components::FlockSpell` holds the spell's data; component
  `SpellFlockAnimal` holds what the spell classes do differently). The animals are animals of the animals module
  (`ECS/AnimalAI.h`: `CreateAnimal`, `MoveTo`, `SetStateRaw`, `SetFinalDestination`, `Destination`, `SetAlpha`,
  `Remove`); the flock's centre is the domain of `components::Flock`.
- **The birth angle** is `detail::AngleOfMapCoords`; the drawn face is that of the animals' `FaceAngle` (their
  `ConvertGameAngleTo3D` convention).
- **The fade-out**: `SetAlpha(alpha / 255)` in the turn. The vt+0x6A4 of the three classes replaces `Living::SetDying`
  (`animal_ai::SetSpeciesDying`, `ECS/AnimalApi.cpp`; there is no longer DYING nor a corpse for them).
- **The wolves' lean**: `TimeToBank` / `BankAngle` (`ECS/AnimalBirds.cpp`) give the spell wolf its second and its
  angle of the speed and the turn, drawn by the animals' bank roll (`ECS/MobileDrawing.cpp`); test
  `FlockWolves.TheSpellWolvesLeanByTheirSpeedAndTurnOverASecond`.
- **The wolves**: the hunting in the corridor, the start in the spell's turn (`SetupWolf`) and the arrival in the
  wolf's state are done, with small hooks in the animal files: `AnimalPredators.cpp` (`IsHuntingTargetValid`, the
  SpellWolf branch of `Abandon`, `SpellWolfMoveToPos`), `AnimalAI.cpp` (the MOVE_TO_POS state of the SpellWolf calls
  `SpellWolfMoveToPos`; `Lion::Eat` vt+0xB50 and `Animal::StartWander` vt+0xB48 also for it) and
  `AnimalAIDetail.h` (the declaration). The arrival at 30 m is no longer checked in the spell's turn. The other
  SpellWolf overrides are in `ECS/AnimalAI` and `AnimalPredators`.
- **The particles**: `Particles/Rules/Flock.cpp` (UR_FollowTargets, UR_Flocking).
- **`NeedsContinualPackets`** is not ported: in local play openblack refreshes `PSysProcessInfo` every turn
  (`creator::UpdateSpellInfo`), so it changes nothing 🔍 inferred. Load/Save are not ported either.
- **Approximate**: the previous position (+0x2C) used for the shield test is that of the spell's previous turn.
#### Where the code is

- `Magic/Spells/SpellFlock.cpp`: `AddAnimal` (the per-animal part of the three ctors), `SpawnAllowed` (the loop's
  common checks), `MakeSpawn` (the interpolation and the jitter), `FlockInitWithPos`, `FlyingInitWithPos`,
  `FlyingProcess` (every bird a target of the rain), `GroundInitWithPos`, `GroundProcess`, `CommonOps`; the fade-out is
  registered with `animal_ai::SetSpeciesDying(AnimalInfo::SpellDove, ...)` and its two siblings.
- `LeaderEvent` (`SpellFlock.cpp:275-283`) passes velocity 0, strength 1 and no shield test, the same as the
  original's [leader's event](#flocks) for the animal just made (called at :664 / :789). UR_Flocking's
  `SpriteRotation` uses the world→camera rotation as `UR_OrientSpriteWithVelocity` does (`Rules/Orient.cpp`), as the
  original.
- `SpellFlock.h`: `FlyingAnimal` (bat or dove by the alignment) and `SpawnPoint` (the point between the last two hand
  positions).

### Test hooks

- `OPENBLACK_TEST_SPELL=FLYING_FLOCK,x,z` and `GROUND_FLOCK,x,z` (the info.dat names; or 24 / 25): the neutral player
  casts from 30 m above the point, so d = (1, 0) and the side alternates; `OPENBLACK_TEST_FLOCK_SHOT=
  "<turns>,<path.png>[;...]"` requests screenshots that many turns after the cast.
- `OPENBLACK_SPELL_TRACE=1`: each animal writes `SpellFlockFlying|Ground <n> #<N> entity e at (x, z) +h m -> (Tx,
  Tz)` and every 10 turns each member its position, state, alpha, prey (`target`, its distance) and speed; `SetDying
  (state, final)` says when and in which state the fade-out began.
- To follow them with the camera: `OPENBLACK_TEST_VIEW_ANIMAL="0,35,-90" OPENBLACK_TEST_ANIMAL_SPECIES=20` (22 the
  wolves) `OPENBLACK_TEST_VIEW_LOCK=1`.
- `test_flock`: N with the fistp rounding, good/evil, the fan (direction, side, angle, rotation), the destination
  (cell, truncated towards 0, the halves), the exit interpolation, the wolf's corridor (normal, edge, behind the cell)
  and the fade-out (2 s, 79.7 at half, 0 at 20 turns); with `OPENBLACK_GAME_PATH`, the real rows (12/14, −0.2, 800,
  45, 40/35 per turn, 50 per collision, 25/60 s, reaction 37, altitudeNormal 45); and from UR_Flocking the law
  fn_00683520 and the UpdateBanking rotation (heading, reduced pitch, bank).

## Pending

- **The wolves' leap** (pending, animals): in openblack the wolves chase (HUNTING_MOVE_TO_POS 41 against villagers
  inside the corridor) but do not get to the leap: they circle at 30-50 m from the prey. The radius of
  `SetTowardsAngle` and its reduction are read the same in the original, but not checked against the original game:
  it may be faithful or a bug in the animals' movement. Settle it by measuring a wolf hunt in the original.
- **`NeedsContinualPackets`, Load and Save**: not ported (see openblack).
