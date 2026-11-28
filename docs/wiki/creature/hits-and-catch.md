# Hits and catches

## Summary

What a creature does when a thrown thing strikes it, and when a thing starts to fly near enough for it to catch.

A strike that is not a toy sways the body on its springs, crushes the creature by the impact over its weight, sours
its attitude to the thrower's player and feeds its fear and anger. A thing thrown near a free creature may make it
plan a catch: it steps, reaches and takes the thing into its hand if its timing allows.

openblack runs both: the strike (`creature_struck`), with the sway, and the catch walk and its turn-by-turn catch
(`creature_catch`, `CreatureCatching`). The reach test, the wait, step or catch and the blend's height and side are bit
for bit.

## The original

#### Struck by a thrown thing

`Creature::ReactToPhysicsImpact` 0x479990 (vt +0x7AC, `ret 8`) [V]. Arg1 is the creature's own physics entry; its
+0x20 is the entry of the body that struck it, and +0x18 of that the thing
([the manager's step 6](../physics/physics.md#manager)).

1. No striking entry: return (0x47999B..0x4799A2). A striking thing that `IsToy(0)` (vt +0x3B4, tested twice,
   0x4799A8..0x4799D4) returns too; with no thing (+0x18 null) the test is skipped and the strike goes on. `IsToy` is
   1 for a `Ball` (0x4363B0, the football), and for a `MobileStatic` whose info's mesh (info vt +0x2C, read as
   `GetMesh` 📼 1.20 only) is 0x18F..0x193, `ObjectToyBall` to `ObjectToySkittle` (`MobileStatic::IsToy` 0x609020); 0 for
   anything else (`GameThingWithPos` 0x4020C0).
2. `fn_0047FAC0(striker +0xC8, entry +0xC)` on the 3D body (0x4799DA..0x4799EE): the sway (below).
3. When Flags +0x25 & 4 (+0x24 & 0x400, a script controls it) and the creature's player (vt +0x1C) is
   `g_game`'s player at the byte [+0x205A59] (players 0xA60 apart from +0x18; the player at this machine 📼 1.20 only), return
   (0x4799F3..0x479A26): such a creature is not hurt, frightened or angered at all, by anyone's throw.
4. `EffectValues(3, striking thing, PhysicsObject::GetPlayer(striking entry))` 0x524FE0 (0x479A2C..0x479A3E): the
   crush preset g_EffectInfo[3], applied by the striking thing (+0x28), the striking entry's status's player at +0x3C
   (0x647460: +0x24 → vt +0x1C, none without a status).
5. `amount = fn_0047D600(own entry +0x8)` (0x479A43..0x479A4E): `impact / (GetMass(3D body) × 9.81)` (0x8CF02C), then
   100 (0x8AB41C) when 100 is below it or it compares with nothing (`fcomp; test ah, 1`), then × 0.01 (0x8C4B10). No
   lower bound. `LH3DCreature::GetMass` 0x47FA80 = `((+0xAC + +0xA4) × 0.15 + 1) × 100 × L³`, L = +0x90 × 8.333334
   (0x8CF114), in that order: the drawn size (+0x90) and the thin-fat and weak-strong **targets** (+0xA4, +0xAC,
   [The fatness the body shows](body.md#the-fatness-the-body-shows)), not the shape drawn. The effect's seven numbers × amount
   (`operator*=` 0x525720 on +0x8, 0x479A5B).
6. Only when amount > 0.005 (0x8C7674; `test ah, 0x41`, so 0.005 itself and a value that compares with nothing are
   out; the cap makes the latter 1 first) (0x479A60..0x479A6F), else straight to the destructor (0x479B66):
   - the effect's place from the striking entry's +0xC8 point (map x and z, ftol'd, 0x479A75..0x479ACB) and its height
     above the land (0x803090, 0x479AE5), then `ApplyEffect(values, 0)` (vt +0x5CC, 0x479B04) with the byte
     [0xC6414D] = 1 around the call (0x479AF4, 0x479B0C), which
     [A creature taking an effect](../magic/spell-core.md#a-creature-taking-an-effect) reads as keeping it out of the fight's
     blocking;
   - when the creature's player (vt +0x1C) is `GetPlayer(own entry)` (0x479B13..0x479B25), the status the entry took
     from what struck it: `UpdateAttitudeToPlayerFromFeedback(−amount)` 0x4E1080 on the mind (0x479B37);
   - `ChangeSource(0x12, amount)` then `ChangeSource(9, amount)` 0x4DE730 on mind +0x8 (0x479B4C, 0x479B61): fear from
     damage, then anger from damage.

No GameRand, GameFloatRand or LocalRand on the way, beyond what `ApplyEffect` itself draws.

##### The sway

Step 2 [V]: `LH3DCreature::ApplyForce(point, force)` 0x47FAC0 (bw1-decomp's name, `ret 8`), the kick.

- **The gated states.** It returns at once (0x47FBE4) when the 3D state +0x4994 is 2 (0x47FACA), 3 (0x47FAD3), 4
  (0x47FADC), 0x23 (0x47FAE5), 0x1E (0x47FAEE), 0x1F (0x47FAF7) or 0x24 (0x47FB00), tested in that order, or when it
  is 1 (0x47FB09) and +0x5190 is 4 (0x47FB14) or 5 (0x47FB1D). What they are, from the writers: in `LH3DCreature`,
  +0x4994 is written by `StateSet` 0x484EC0 (61 calls) and otherwise only by `Init`, `StartMovingToPoint` (1),
  `StopPointing` (9) and `DoMovingAction`; +0x5190 by the motion code. `StateSwitch` 0x4851A0 dispatches on it (table
  0x485D5C, from state 2).
  - **2, 3, 4: a static action's start, loop and end.** `StartStaticAction` 0x4843A0 plays clip − 1, clip and
    clip + 1 and sets 2 (0x4843F5); `StateSwitch`'s case 2 moves it to 3 (0x4854C0), or to 4 (0x4854B0);
    `EndStaticAction` 0x4848C0 moves 3 to 4 (0x4848CD). It is the `StaticAction` sub-action's (0x500A30), the sit's
    and the cast's (0x500B70).
  - **0x1E, 0x24, 0x1F: fainting, lying fainted, getting up.** `StartDeathAction` 0x484D30 sets 0x1E (0x484DD8)
    and `IsDying` 0x484E00 is `+0x4994 == 0x1E`; its only caller is the faint sub-action's start 0x5022D0, which also
    sets the fainted flag +0x384 = 1, and whose next stage waits for `IsActuallyDead` 0x484E10, `+0x4994 == 0x24`
    (set by `StateSwitch`'s case 0x1E at 0x485C74 once the fall has played). `Resurrect` 0x484E20, from 0x1E or
    0x24 only, plays clip 0x6B (107, the get-up) and sets 0x1F (0x484E50). So the fight states' reading of 0x1E as
    `Faint` holds, 0x1F is the get-up, and lying is 0x24, not 0x23.
  - **0x23: a turn on the spot to face a heading.** `StartTurningAction` 0x481160 calls `InitialiseTurning`
    0x48B790 and sets 0x23 (0x48117C): the `TurnToFacePos` sub-action's turn and `EndAnyActionsThenTurnToHeading`.
  - **State 1 with +0x5190 4 or 5: a move's start.** `PrepareAnims` 0x47F300 sets +0x5190 from `GetStartState`
    0x483AB0: 4 when it must turn on the spot first (`InitialiseTurning`, 0x483EB8), 5 when it steps off turning
    (a `CAnim` blended from clips 10..12 or 13..15 by the angle into +0x5220, 0x483E4C), 6 when it nearly faces its
    way (within 0.2618, 0x483E92). `UpdateMotion` 0x490960 runs 4 as `DoTheTurningBusiness` 0x48B970 and 6 as
    `ProcessMoveStraight` 0x491180; 0 is standing (`IsStandingStill` 0x483050, `GetStopDist` 0.05 in state 4).
- **The spring it pushes, by the height above the land.** The height is `point.y − GetAltitude(point)`, through
  vt +0x28 (0x47FB40), which for `LH3DCreature` is `GetAltitude` 0x47F720: `LH3DIsland::GetAltitude` 0x803090 at the
  map coords `ftol(x × 65536 × 0.1)` and `ftol(z × 65536 × 0.1)`, the land under the point, not the body's base.
  When that height is more than 9 × +0x90 ([0x8CF118], `test ah, 0x41`: equal or unordered is not more) it takes
  the upper spring +0x487C, else the lower +0x4870 (0x47FB46..0x47FB5B).
- **The push.** k = +0x48B8 / (`GetMass` 0x47FA80 × 0.5); the spring's x += k × force.x and z += k × force.z, its y
  set to 0 (0x47FB61..0x47FB93); when √(z² + x²) is more than 8 ([0x8C2C70], equal or unordered is not), all three
  are scaled by 8 / that length (0x47FBB3..0x47FBCA). +0x48AC = 1 on both ways out (0x47FBCE, 0x47FBDC). +0x48B8 is
  `UpdateTime`'s ms × 0.001 (0x481E2D), stored at the start of the creature's turn; the physics runs after the
  creature loop, so a strike reads this turn's.
- Its other caller, `UpdateTime` 0x482182, is the leash's body force, which never runs: the leash drag +0x48B0 is set
  by no running path ([The leash's tug](leash-rope.md#the-leashs-tug)).
- **The spring, once a creature turn** (`UpdateTime` 0x482208..0x482485, only while +0x48AC is set, before
  `StateAction` and `UpdateBuffers`): the drive +0x48A0 += (push − drive) × 0.15, the push 0 but in the leash's
  branch; s = +0x48B8 × 0.1, m = `GetMass` × 0.5, stiffness m × −30, damping m × 5, gain s / m. For the lower part
  (speed +0x4870, offset +0x4888) then the upper (+0x487C, +0x4894), 10 times: push = drive + stiffness × offset
  (each axis); when speed² (summed z² + y², then + x²) is more than 36 the speed is scaled by 6 / √ of it; speed +=
  (push − damping × speed) × gain; offset += s × speed. +0x48AC is cleared when, for both parts, |offset| and |speed|
  (each summed from z to x) are at most 0.05. Run natively with the oracle (an entry at 0x482208 with esi = the body,
  `GetMass` stubbed, a `ret` at 0x48248E): 400 random cases match a float model with those orders bit for bit; the
  same model summing x first differs in 61 of them.
- **The leans** (`UpdateBuffers` 0x4ECCDC..0x4ECF70, while +0x48AC is set, after the two look layers and before
  the +0x49AC and +0x5738 layers): the yaw is +0x84, plus `angle_correct(+0x4840 − +0x84) × +0x47D0 / [+0x5220]`
  while a step-off's blend +0x5220 plays (`fimul`, `fidiv`). With c = cos(−yaw) and s = sin(−yaw), stored as floats,
  each part's offset (lower first) gives a side lean c × x + (−s) × z, drawn by clip 0xC4 (lower) or 0xC2 (upper),
  and a front lean c × z + s × x, by 0xC5 or 0xC3: skipped when |lean| ≤ 1e-4 (a double compare of the float),
  else at time `ftol((clamp(lean, −1, 1) + 1) × 0.5 × [anim])`, [anim] being the clip's length in ms (the same field
  `GetStartState` writes as ms at 0x483E47), by `ModifyBufferByDifferenceFromFrame` relative to frame
  (count − 1) / 2 into the work pose +0x517C. The body's matrix from +0x84 (`GetPositionMatrix` 0x4EBE10) has rows
  (cos, 0, sin), (0, 1, 0), (−sin, 0, cos): the leans are the offset along the body's own first and third axes.

##### The targets and the flag

📼 1.20 only 3D +0xA4 and +0xAC are written only by `Update3DCreatureFromAttributes` (0x4772B8: clamp(2 · phys+0x18 − 1);
0x477342: the strength axis), `SetStrength` 0x4F0149 and `ModifyStrength` 0x4F01E7, and the physics (0x54E67E) runs
after `ProcessLiving` (0x54E65B): a strike sees this turn's shown fatness and the live strength, the quantities
openblack computes at the strike, in the same order (the creature loop, then the physics). Flags +0x24 & 0x400 has
one writer, `Creature::SetControlledByScript` 0x47D2C0: a script's control, openblack's `ScriptHeld`.

#### The faint of behaving strangely

`ConstructSubActionsForBehaveStrangely` 0x4AB9B0 (action 213) lays, one time in two (`GameRand(2)` 0x4ABAB3 is 0),
FAINT 0xE (argument 0x6A, the fall), WAIT 0x18 for ftol((r + 5) × (1000 / ms per turn)) ticks with r =
`GameFloatRand(5)` 0x4ABB6B (0x4ABB81..0x4ABB92: r + 5.0, × the integer ticks a second, `__ftol`), and RESURRECT 0x65.
The count is stored in the sub-action's entry (+0x38) as the agenda is built (`AddSubAction` 0x4FF240 calls the
argument's vt+0). The stage table (0xBE0BE8 + (type × 9 + stage) × 16, filled by 0x4F8D70): FAINT 0x5022D0 / 0x502320
/ none; WAIT none / 0x503500 / none; RESURRECT none / 0x5076A0 / none. `ProcessSubAction` 0x4FEDA0 runs a missing
stage and goes on in the same turn (0x4FEE4C, back to 0x4FEE02); a stage returning 2 is advanced (fn_004FF570) and the
turn ends there.

- FAINT 0: the fainted flag Creature +0x384 = 1 (0x5022DE), `StartDeathAction`, a REACT_TO_DEATH (0x17) reaction with
  no player (0x50230B); returns 2. FAINT 1 returns 2 only while the body lies (`IsActuallyDead` 0x484E10, +0x4994 ==
  0x24). WAIT 1 takes one off the count each turn the body is free or lying and returns 2 at 0 or less. RESURRECT 1:
  `Resurrect`, +0x384 = 0, `SetLife(0.4)` (vt+0x5B0); returns 2.
- **But it never gets there** ✅ (static read of the frozen v1.42 exe). Near the top of `Creature::ProcessState`, before
  the planner and `ProcessSubAction` (0x4737B0), 0x472F03..0x472F3D: when the body lies (`IsActuallyDead`) and the
  current action (mind +0xF60, the first plan's action: the plan at mind + 0x710 + 0x838, its action at +0x18; the
  three builders of the sub-actions read it, 0x4734EB, 0x4C452E, 0x4F15BA) is not 9 (dead), 0xD9 (die permanently) or
  0x105 (dead forever), it calls `Resurrect` 0x484E20: clip 107, state 0x1F, the get-up. The needs faint
  (`Creature::Faint` 0x476FA0) forces action 9 (0x47700A), so it stays down; behaving strangely is 213, so on the first
  turn that `ProcessState` sees the creature lying it gets up, before FAINT 1 looks. FAINT 1 then never sees it lying
  again (only FAINT 0 starts the fall), so the WAIT, the RESURRECT and the two confused actions are never reached: no
  wait, no `SetLife(0.4)`, and +0x384 stays 1.
- **The flag left set.** +0x384 is read by `Creature::Faint` (0x476FA6: set, it does nothing, so the creature does not
  faint for its needs), by fn_004F0370 (0x4F0437: set, it returns before choosing anything, `DecideWhatToDo` vt+0x8C8 or
  the queued plan), and by `IsAvailableForReaction` 0x4F2820 (0x4F28BF: set, no reaction is taken up). Its writers
  are FAINT 0, `Faint` (0x477043), RESURRECT 1 (0x5076B8), `SetToZero` (0x47420F), and ProcessState's block at
  0x47376E (0x473792), which runs on a turn the planner runs (0x4735A9..0x4735D7: not bypassed by [0xD00DDC] = 0, a
  stilled mind +0x20CE4 = 0, the script's control Flags +0x24 & 0x400, or mind +0x20D30) and the first plan is not valid or its agenda is spent (0x473717..0x47374A), after
  fn_004F0370 has run (0x47369D). So the creature chooses nothing on that turn and is free from the next.
- **The stall.** The agenda stays on FAINT 1, valid, with the flag set, so fn_004F0370 never replaces it. What ends it,
  as far as read: something that forces a plan from outside the decision, as a script (`ForceActivityAndForceAction`
  0x4C44B0 does not read +0x384), the cut scene's look at the camera (0x4736F9), a fire or a low life
  (fn_0047CFB0 0x47358C, which does not read +0x384; its other tests are not read here 🧪 `python xref.py show
  fn_0047CFB0`); or the hand's locked select (Flags +0x24 &
  0x10 sets mind +0x1C28, 0x472F83, and the turn after it, with the agenda still valid, 0x4733C7..0x473506 ends the
  action and builds the same plan again, BehaveStrangely from its stare, with new draws). The flag is cleared only once
  such a plan's agenda is spent. The 200-odd other callers of `ForceActivityAndForceAction` are not read 🧪 (`python
  xref.py callers ?ForceActivityAndForceAction@Creature@@QAEXAAVCreaturePlan@@HH@Z`).

#### The creature's catch

What a creature does when a thing starts to fly near it [V]. bw1-decomp's C has `StartCatching`, `DoCatchMain`,
`DoCatchStep`, `DoCatchAction`, `StateSwitch` and `EndAnyAnimsRapidly`; the walk, its two tests and the sub-action
were read in the disassembly. Notes on each function are in the xref notes.

##### The walk

`Creature::CheckAllCreaturesForCatching` 0x47CBD0 has two callers (xref): 0x63741C in
`Object::InitialisePhysicsFromHand` and 0x63756A in `Object::InitialisePhysics`. For each creature of
`Creature::CreatureList` 0xC5FCF8, in list order:

1. `fn_0047C960` (0x47CBF3), 0 = skip. It returns 0 when Flags (+0x24, u16) & 0x10, bw1-decomp's
   `GAME_THING_WITH_POS_FLAG_INTERACTING` (the creature the hand is on: `CanCurrentlyBePickedUp` and the stats display
   test it; not the locked select's own flag, which is 0x20); when mind (+0x164) +0xF60, the current plan's action
   (`IsAvailableForReaction` 0x4F2898 compares it with 0x15), is 0x15 FIGHT; when `IsFighting` 0x47B1C0; when byte
   +0x25 & 4, Flags bit 10 `CONTROLLED_BY_SCRIPT`; when vt +0x984 `IsAvailableForReaction(0)` is 0 and mind +0x700 is
   0x28 (+0x700 is the script's only desire: 0x28, the desire count, is "none", written by
   `SET_CREATURE_ONLY_DESIRE_OFF` 0x6F47F7 and three other sites); and when the current plan's action is 0xED (237,
   "Catch", the action table's row).
2. Flags & 0x10 again (0x47CC00).
3. `Get2DRadius` (vt +0x64) of the creature and of the thing, both popped unused (0x47CC0E..0x47CC1A).
4. The thing's `GetWeight` (vt +0x638) must be below `MaximumPickUpWeight` 0x4E3D30 (called at 0x47CC2C), the
   creature's `GetWeight` × 0.8 (`fcomp`, `test ah, 0x41`: equal or unordered skips).
5. The thing's `CanBePickedUp` (vt +0x628): `Object` 0x4026A0 none; `Living` 0x4172A0, `MobileObject` 0x425B30,
   `MobileStatic` 0x439670 and `Tree` 0x55D8B0 not Flags bit 13 `CANNOT_BE_PICKED_UP`; `Villager` 0x55CA50 also not
   while +0xE0 & 4, the at-home flag ([animals](../beings/animals.md)). By vtable: animals and birds the Living's; pots, piles,
   scaffolds, creeds, rewards, balls, the one-off seed and the field crop the MobileObject's; rocks, dead and felled
   trees, fragments, the bonfire, vortices and the teleport the MobileStatic's; the football, features, fixed things,
   totems and spell icons none.
6. `fn_0047C9D0(thing, physics entry)` (0x47CC52), the reach test: the entry's +0x1C (its thrower) is the creature →
   0. The entry's +0x24 (`GInterfaceStatus`; of `InitialisePhysics`'s 30 call sites, vt +0x784, only the creature's
   own launches pass one: `LH3DCreature::UpdateTime` 0x482B01, `DoKickingAction` 0x488FAA and `DoThrowingAction`
   0x48D192 give its player's `GPlayer::GetLeaderInterfaceStatus` 0x64A9F0, or 0 without a player (0x482ADF / 0x488F87
   / 0x48D16F); 24 sites pass 0 [V]; of `InitialisePhysicsFromHand`'s 3, vt +0x780, `Object::ThrowObjectFromHand`
   0x6386F1 passes the hand's own interface, any player's, and `LH3DCreature::DoDiscardAction` 0x48C987 / 0x48C9C5, the
   creature letting go of what it holds, its player's leader interface (0x48C971) or 0 without a player (0x48C99C),
   with the creature as the thrower, +0x4834 pushed at 0x48C96E / 0x48C99E [V]) with a `GetPlayer` that is not the creature's and not `IsAllied`
   0x64D5D0 → `GRand::GameRand(100)` (Creature.cpp line 0x134E) above 2 → 0. Then, in this order and each step a
   float (the game's control word gives 24-bit precision): speed² = vz·vz + vx·vx (+0x10C, +0x104), below 1 → 0;
   s = √speed²; the way back b = (−1/s)·vx, (−1/s)·vz; the seconds to closest t = (bz·dz + dx·bx) / s, with d = thing
   (+0xC8, +0xD0) − creature (LH3DCreature +0x78, +0x80), above 5 → 0; rate = 1.6 − (size +0x90 × 0.5) × 0.85; the
   miss x = (vx·t + thing x) − creature x, z = (vz·t + thing z) − creature z; spare = t − catch (+0x4A3C, an int) /
   (rate × 1000), below 0.05 → 0; step = the animation 225's duration (`GetAnim(0xE1)` [+0], an int) / (rate × 1000);
   reach = (step × 0.5 + spare) × (−(its travel [+0x10], the x of `CAnim::Movement`, × model scale +0x94) / step); it
   reaches when reach > √(mz² + mx²).
7. It reaches: `EndAnyAnimsRapidly` 0x48F750, `StopMoving` 0x484260, `AddBeliefAboutObject(creature, thing)`,
   `CreaturePlan(3 Play, 0xED Catch, activity = GetBeliefAboutObject(itself), act on = the thing's belief, use = none,
   1.0)` (the ctor's `ret 0x18`: six arguments; 1.0 pushed at 0x47CC7D) and `ForceActivityAndForceAction(plan, 1, 1)`.

+0x4A3C is the second of the file's two "catch" times: `LH3DCreature::LoadBinary` reads, in the .cbn's order, +0x4A34
(pick up), then for a version above 8 +0x4A38 and +0x4A3C (0x4EB804, 0x4EB816), then +0x4A80, +0x4A84 (destroy),
+0x4908, +0x4960, +0x4964 and +0x4A40 (put down). The first, +0x4A38, is the take of `DoPickupFromHandAction` and
`ValidateKickPositions`, not a catch.

##### The plan

`ConstructSubActionsForCatch` 0x4B6E20 adds one main sub-action, 110 (its row at 0xBE4988 = 0xBE0B98 + 110 × 0x90),
on the thing, with the smile face. Its Initiate `sub_507C70` returns 0 (wait) while `IsPerformingBodyAction`, 2 when
`StartCatching` succeeds and 1 otherwise; it has no Perform nor Finish, so the plan is carried out as soon as the
catch starts and the body carries the catch on by itself.

##### The start

`LH3DCreature::StartCatching` 0x480AC0 (its only caller is the sub-action): the four catching animations 228..231
must exist; not with +0x51EC set nor while `IsPerformingBodyAction`; the thing in the physics with vx² + vy² ≥ 1
(across and up or down, not along z). It faces back along the flight (`heading_from_direction_vector(v) + π`),
`InitialiseTurning` when off by more than 0.2617994; `ValidateCatchPositions` 0x4EE900 measures, once in the
creature's life (+0x5248), the hand's grasp point (`GetGraspPoint` 0x83A0E0, the leaf mean, on the right hand
+0x51C8) in each of 228..231 at +0x4A3C, into +0x4A1C, +0x4A10, +0x4A04 and +0x49F8 (231 first); then state 0x13.

##### Each turn

`UpdateTime` runs `StateSwitch` and then `StateAction` with the turn's step `AnimTimeInc`:

- 0x13 `DoCatchMain` 0x4884A0: the turn first; out of the physics or behind (forward · offset < 0) it stands; the side
  (right · offset) beyond −((h3.x − h2.x) × 1.2 + h2.x) × Size2 takes the step 225 (`ReverseAnim` = side ≤ 0) if
  `IsDestinationValidAndClear` 0x483780 (not moving, and outside the route planner's circles of its route square);
  in reach, arriving (distance over the closing speed) within −0.01 .. +0.1 s of catch / ((1.6 − Size1 × 0.5 × 0.85) ×
  1000) starts state 0x15 (`ReverseAnim` = side < 0), sooner it stands, later it waits (`DoStandingAction`).
- 0x14 `DoCatchStep` 0x488320: moves by Movement × (step × Size2 / duration), turned, the whole move negated when
  reversed; a place `IsDestinationValidForCreature` (radius 7.05) refuses goes to `SpiralCheckForValidPoint` (16
  directions, radius 7.1, from 0.5 by 0.5 below 1000). `StateSwitch` returns it to 0x13 once time + step ≥ duration.
- 0x15 `DoCatchAction` 0x487FA0: height (thing y − lowY) / (highY − lowY) with lowY = h1.y × Size2 + y, highY = h3.y ×
  Size2 + y; side as above (negated when reversed); both kept within −0.2..1.2 (a clamp is a miss); the four weights;
  reaching (1) and caught (3) play on (`AdvanceCyclic`), missed (2, also once out of the physics) plays back. Reaching
  at time ≥ +0x4A3C: a clamp misses, else `RemoveObject(thing, 0, 1)` (no EndPhysics), held, and a Living
  `StorePreviousState` + `SetTopState(IN_HAND)`. `StateSwitch` stands it when played on to the end of 231, or when
  played back to less than a step.

##### The axes of the wait and the blend (read 2026-11-19)

Both states measure along the creature's heading +0x84 (the game's, minus openblack's walk heading), never through a
matrix. 0x13 (0x48850C..0x48854E): ahead = (−cos H) × dz + dx × sin H, each of sin H and −cos H stored as a float
first (`fstp`), the sum stored. The side (0x488565..0x4885A3, and again in 0x15 at 0x488009..0x48806B): the angle
H − 1.5707964 ([0x8C78D8]) as a float, its `fsin` stored as a float, its `fcos` `fchs` left on the stack unrounded into
its product with dz; side = (−cos) × dz + dx × sin, the y term left out (right.y is 0). The reach −(((h3.x − h2.x) × 1.2)
+ h2.x) × Size2 is compared with |side| (`fcompp`, `test ah, 1`): the step when the reach is not at least |side|
(unordered too), `ReverseAnim` = side not above 0 (`test ah, 0x41`). The closing speed (0x4886EF..0x488709) is
(−vz) × (−cos H) + (−vx) × sin H from the physics entry's +0x104/+0x10C, the arrival ahead / closing (`fdivr`); the
wait when it is at least lead + 0.1 ([0x8AB22C]), the stand when it is not above lead − 0.01 ([0x8C5840], unordered
too), else the reach with `ReverseAnim` = side below 0 (unordered too). 0x15's height (0x487FCC..0x488001):
(y − (h1.y × Size2 + pos.y)) / ((h3.y × Size2 + pos.y) − (h1.y × Size2 + pos.y)), and its side
(side − leftX) / (rightX − leftX) with leftX = −(h2.x × Size2), rightX = −(h3.x × Size2); each kept at −0.2 when not at
least −0.2 (`test ah, 1`: a NaN too) and at 1.2 when above it.

##### The look (read 2026-11-19)

`UpdateTime` runs `UpdateLook` (bw1-decomp Creature3D.cpp:1741) right after `StateAction` (Creature3D.cpp:1593..1595).
In 0x13 and 0x14 with an action object it sets the look point to the object's map position as a point
(`ConvertMapCoordToLandscapePoint`: x and z of its map coordinates, y its altitude over the land there), over the
physics position 0x13's `DoCatchMain` set (Creature3D.cpp:4180) once turned; the head then turns to it, as
`CanUpdateLook` (Creature3D.cpp:1941) allows 0x13 and 0x14. 0x15 sets no point and `CanUpdateLook` refuses it, so the
head turns back ahead while the point stays, still read by the eyes (Creature3D.cpp:6512, any state); `DoCatchAction`
clears it as the hand closes, caught or clamped (`LookPoint.SetNull()`, Creature3D.cpp:4091), not on a miss for want
of the physics. A turn that stands the creature again (0x13's give-up, the end of 0x15) leaves the point as it was,
looked at by the head (STANDING may update the look). `SetLookPoint` (Creature3D.cpp:1722) sets nothing while the
creature is INTERACTING (the hand on it) unless `AlwaysSetLookPoint`, which only the hand's own state sets.
The hand-close clear is a plain write, not through `SetLookPoint` (the frozen exe's `DoCatchAction` stores 0 in the
three coordinates before its clamp test, as the C), so while the hand is on the creature it wipes the point the hand's
state set; that state sets it again on its next update, every frame, at the eye or near the hand
(`HandStateCreature.cpp`:415..427), so the eyes look ahead only until then.

## openblack

Both are done. Porting decisions: [Port decisions: the creature](../project/port-decisions-creature.md).

### The strike

`PhysicsGameHooks`' Creature case gathers the strike (`ecs::creature_struck::Strike`: the striking entry, its thing
and centre, whether the hand threw it and whether the creature's own entry took that, the turn's force and mean force,
the owner, `ScriptControlled`, `creature_pose::DrawnSize` and the thin-fat and weak-strong of
`creature_pose::TargetMorph`), and `creature_struck::HurtCreature` takes the steps above against
`physics_hooks::World`: the toy test (`World::IsToy`: `creature_struck::IsToy`, the football or a static of a toy's
model, as the original's two; until 2026-10-28 ours missed the football), the sway (`World::KickSway`, below), the local player's
scripted creature (`World::LocalPlayer`), `living::CreatureMass` and `living::CreatureCrush`, the crush (`World::Crush`,
scaled, then `World::ApplyEffect` as a blow from the striking thing, put down to the hand's player when the hand threw
it, or to no player), the feedback when the own entry was the hand's and the creature is the local player's
(`World::UpdateAttitudeFromFeedback`), then the sources 18 and 9 (`World::ChangeDesireSource`). An entry's "status" is
openblack's `throwerPlayer`: the player who threw it, which the struck entry takes from what struck it (the same rule as the
original's step 6 of the manager). As the original, an earlier turn's hitter is kept when the body last hit is not
listed (`ecs::physics::hitter::FindHitter`; `PhysicsHitter.TheHitterSurvivesTheTurnStart`,
`PhysicsHitter.AGrowthInTheLoopKeepsTheOldHitterAndNoCredit`). The testbed scenario `physics.rock_strikes_creature`
shows it.

Not ported in this path: the effect's place (the values
carry the striking body's centre as the source's point, which nothing reads yet); the fight's skip on [0xC6414D] (the
source's `blow` flag is set, and nothing reads it yet).

### The sway

`creature_sway` is wired: the strike calls `World::KickSway` with the turn's force (`PhysicsEntry::forceSum`) at the
striking body's centre, before the script test, for any counted strike; `CreatureAnimationSystem::KickSway` takes the
land under the point (`LandIslandInterface::GetHeightAt`, the same ftol'd map coords), 9 × the drawn size and the mass
`living::CreatureMass` of the drawn size and the target shape, as the crush does. The gate
(`CreatureAnimationSystem::IgnoresKick`) maps the states to ours: 2, 3, 4 and the out-of-fight faint (0x1E, 0x24,
0x1F) are a body `Sequence` that a fight does not time (the static steps: sitting, sleeping, the poo, being sick, a
cast, the faint's fall, lying and getting up); in a fight, `creature_fight::State` `Faint`, `Lying` and `GetUp`; 0x23
and state 1's 4 are the locomotion's `Turning` (with `facingOnly` for 0x23), and 5 is `Stepping`. `PoseTurn` sets the
turn's seconds (`creature_turn_pose::LivedMs` of the playback scale and `MsPerTurn`, × 0.001) and steps the spring
before the pose; `SampleBody` layers the leans after the look, by the columns of the Transform's rotation
(`affine::AngleY(−heading)`, the original's matrix), in the turn pose and the frame pose alike. The leash's branch is
not wired (it never runs; its holder point, the leash's interface status +0xC8, is not read), and our pull stays
`CreatureLeash::pull`.

### The catch

`PhysicsClassHooks::OfferToCatchingCreatures` is the walk (`DynamicsSystem::CheckAllCreaturesForCatching` calls it
from both of the exe's sites): `PhysicsGameHooks::OfferToCatchingCreatures` calls `ecs::creature_catch_walk::Offer`
with the game's world (`GamePhysicsHooksWorld`). The pure rules are `creature_catch` (`MayTry`, `MostWeight`,
`GoesForOtherPlayersThrow`, `Reaches`, `FastEnoughToStart`, `ReadyToCatch`, `Weigh`, `StepMove`, the clocks).
`MayTry`'s "only desire" (mind +0x700 not 0x28) is the mind's one kept dominant desire
(`CreatureMindState::dominantDesire`, which `SET_CREATURE_ONLY_DESIRE`, the leashes, a tied town and the mood spells
share, as +0x700 is shared): a creature not free to react, a script creature's reactions off or its player's order,
still tries a catch while one is set (2026-11-03, refitted from raffclar's #619; it was always false before; test
`CreatureCatch.ADesireMadeDominantCountsAsTheOnlyDesire`).
`Reaches` is worked out in the game's order and matched the original on 4000 random cases and 40 at the edge of the
reach (the oracle); 20 of them are pinned in `test_creature_catch`. The plan is the mind's `ForceActivity(Play,
"Catch", the thing, score 1)`, the action a forced-only executor (`creature_plan_actions::ForcedFor`) whose agenda is
one object step; its order starts `CreatureObjectActionSystem::Catch`, and the catch is then the `CreatureCatching`
component, a turn at a time in `ProcessTurn`, starting from the turn after its start (the sub-actions run after the
body).

The side step's clear test is the original's (`IsDestinationValidAndClear`, bw1-decomp Creature3D.cpp:1968, called
from `DoCatchMain` Creature3D.cpp:4221) on openblack's port of its route planner: the step's end is the step clip's
whole travel, only its x turned round for the left hand, through the body's leaned placement at its drawn size
(`GetPositionMatrix`, `creature_pose::LeanedModel`); it is refused while the creature moves, without the step clip, or
strictly inside any circle (a swallowed one too) of the one route square at (x / 80, height / 80) that the creature's
own plan, emptied first, gets from the world (`route_plan_world::IsStepEndClear`, `ObstacleGrid::IsClearInSquare`).
The second index is the end's height, not its z, as the exe reads it (0x4837B8, xref note #7955). A square off the grid
reads no circle; the original reads outside its table there, which no creature on the land reaches. Tests
`RoutePlanTest.AStepEndIsClearOnlyOutsideEveryCircleOfTheSquareItReads`, `ASquareIsTheMetresOverItsSizeTruncated`,
`RoutePlanWorldCreatureTest.ASideStepEndsOnlyOutsideTheCirclesOfTheSquareOfItsXAndItsHeight`; the creature panel's
Hands section shows the last step's end and whether it was taken (the Catch node, closed by default).

The wait, the step and the blend are measured as the original does (`creature_catch::AheadOf`, `RightOf`,
`ReadyToCatch`, `Weigh`), from the creature's heading as the game holds it (`creature_catch::Stance`: minus
`CreatureLocomotion::heading`, or of the way the body faces before it has walked) and the thing's body centre in the
world, each step a float in the game's order, the right's cosine taken unrounded into its product, and a NaN kept and
judged as the exe's compares judge it. They matched the original on 600 random cases each (the oracle running the two
states with the creature's other calls stubbed: 53 catches, 274 steps, 38 waits, 235 give-ups, and every blend's height
and side to the bit); 12 and 8 of them are pinned in `CreatureCatch.ItWaitsStepsAndCatchesAsTheOriginalWorksItOut` and
`TheBlendIsTheOriginalsToTheBit`. Until 2026-11-19 they were measured in the creature's own axes through its
Transform. The Catch node also shows the heading, the thing's last place ahead and to the right, and the blend's
height and side.

The look is the original's (`CreatureCatching::lookPoint`, `creature_catch::LookWhileCatching`, `LookTurn` in
`CreatureObjectActionSystem`, after each turn of the catch and over the mind's look of that turn): turning, waiting and
stepping, the head and the eyes are on the thing's map point (`map_coords::ToWorld(map_coords::FromWorld(position))`);
the turn the reach starts keeps the body centre the wait set; reaching, the head turns back ahead and the eyes keep
the point until the hand closes; the turn the catch ends, the head and the eyes are on the point kept, if any, and
from the next the mind's look is the creature's again. While the player's hand is on the creature the catch sets no
point (the hand's look stays), turns the head back as it reaches, and the turn its hand closes lets go of the eyes'
point the player's hand set, which the player's hand sets again in its next frame (`creature_catch::LookUnderTheHand`).
Tests `CreatureCatch.ItLooksAtTheThingUntilItReachesAndItsEyesUntilTheHandCloses`,
`UnderThePlayersHandItsHandClosingLetsGoOfTheHandsPoint`; the Catch node shows the point.

The executor's target is `Target::Pickable`, as the action table's row 237 has it: its object test (row +0x34, the
thunk 0x4C38D0) is `CanBePickedUpByCreature` vt+0x258, needsObject 0, and Catch is in no desire's list. Only
`CreatureAgenda::IsObjectSuitableForAction` 0x4E3880 asks that test, and its two callers are
`CreatureMental::IsObjectSuitableForAction` 0x4E3800 (the planner's object searches, and sub_50A380, whose address
`Creature::ForceActOnObject` takes) and `Creature::MimicForceCopyingAction` 0x4EA130; the walk's
`ForceActivityAndForceAction(plan, 1, 1)` 0x4C44B0 does not, and
neither does our forced plan (`CreatureMindSystem::PlanAgenda`), so the target changes nothing in play. It was
`Target::Anything` before.

What differs:

- The creature list's order is the order creatures were added to it; openblack asks them by entity number.
- openblack gives the player's hand's throws the local player (`throwerPlayer`), the computer hand's drop its
  computer player (`computer_hand::LetGoCreditedTo`), a
  creature's let-go the creature as its thrower (`PhysicsEntry::thrower`, which the walk's first test reads) and its player (`creature_object_actions::ThrowerPlayerOf`: none for a creature of no player's), its throw at a target
  its player too (`DoThrowingAction` 0x48D154..0x48D192 hands `InitialisePhysics` the creature and its player's leader
  interface) and its kick
  its player, and every other none. A creature draws on a throw of another
  player's, unless that player is allied to the creature's (`PhysicsHooksWorld::IsAllied`, `ecs::alliances`), when it
  goes for it without a draw.
- The thing's weight is the physics' body weight (`DynamicsSystem::WeightOf`): `GetWeight` but at least 0.01, a
  fragment's 30 × its area (the original's fragment weighs scale³ × its info's weight). A fragment can be caught by its
  class.
- `EndAnyAnimsRapidly` (+0x5730, the body's action played at twice its speed until its state changes; states 2..12,
  14, 16, 22..25, 37) is ported since 2026-11-15 as `CreatureMindSystem::Hurry` (the body's `creature_layers::Hurry`,
  the object action's and the fight's own hurries), called where the original calls it: the catch
  (`GamePhysicsHooksWorld::SetAboutCatching`), the gift from the hand (`TakeFromHand`), the faint, the leash's orders
  (`LeashSystem::ActOn` after the pull away, the walk to a tapped point and the sea's and sleep's orders unless it
  already walks as ordered, the walk back into the kept area), the cast for the leash and the script's forced finish.
  The forced plan that follows (`ForceActivityAndForceAction` → `FinishActionUnsuccessfully(…, 1, 1)` →
  `StopWhatIAmDoing` → `EndAnyActions(1, 1)` 0x48F710) no longer cuts the body's action short: `Replan` ends a loop
  and a dance (`EndLoop`, `EndDance`: `EndStaticAction` 0x4848C0, `EndDanceAction` 0x4848F0) and lets an action
  played once play on, the plan's steps waiting for the body. Tests
  `CreatureMindSystemTest.ANewPlanLetsWhatTheBodyPlaysComeToItsEnd`, `CreatureMindSystemTest.AFaintHurriesWhatTheBodyPlays`.
  Since 2026-11-22 the hands' action is not cancelled by `ForceActivity` either: as `EndAnyActions(1, 1)` 0x48F710
  (bw1-decomp `Creature3D.cpp:6862`: `EndDanceAction`, `EndFighting`, `EndStaticAction`, `StopPointing`; the object
  states play on), its animations on a thing play on to their end (`CreatureObjectActionSystem::StopForNewAction`) and
  a point brings its arm down: `StopPointing` (`Creature3D.cpp:2523`) puts `POINTING` to `FINISH_POINTING`, whose `DoPointAction`
  goes on round the low left animation past the point moment and whose `StateSwitch` stands the body once a step takes
  the time to the high left animation's end (`Creature3D.cpp:3029`); the point's step is done at the stop, its sub-
  action's `StopPointing` (`SubStatePerformPointAtObject`), and the body is busy until it stands
  (`CreatureObjectAction::pointEnding`, `creature_object_actions::PointEnds`; tests
  `CreatureObjectActionSystemTest.AStoppedPointBringsItsArmDownBeforeTheBodyStands`,
  `CreatureObjectActions.APointEndsOnTheTurnItsStepReachesTheHighLeftEnd`). A point's own end, its hold run out, does the
  same, where it stood at once.
  The other stops of the mind that end what it does as a failure go the same way since 2026-11-22, each reaching
  `StopWhatIAmDoing` in bw1-decomp: a new plan (`DecideOnNewPlan` 'overriding action with new one', `CreaturePlan.cpp:344`;
  `CreatureMindSystem::Adopt`, also for following the player's deed), the plan started again (`ProcessState` 'starting
  again', `Creature.cpp:336`; `StartPlanAgain`), the take from the hand (`TakeFromPlayersHand` →
  `ForceActivityAndForceAction`, `Creature.cpp:3030`; `TakeFromHand`) and a desire forced on a thing
  (`ForceActivityAndDecideAction` 0x4C43A0, `CreatureAction.cpp:7009`; `ForceDesireOn`). So does the player's feedback, a stroke
  or a slap (`RespondToPlayerFeedback`, 'player feedback', `CreatureMentalLearn.cpp:155`;
  `CreatureMindSystem::ReceiveFeedback`). Still cancelling, read 2026-11-22: the fight's start and the faint
  (`CreatureFightSystem` `StopForTheFight`, `Faint`), whose originals (`Creature::Faint`, `Creature.cpp:1818`, through
  `ForceActivityAndForceAction`) let the hands play on too, but ours also drops what the creature holds there at once, where
  the original puts it down first: its Faint and Fight sub-actions are `CREATURE_SUB_STATE_HELD_OBJECT_PUT_DOWN`
  (`CreatureSubAction.cpp:51`, `:65`), so `ProcessSubAction` starts the put-down action `GetPutDownAction` chooses
  (`StartObjectRemoveAction`) and the faint or the fight starts on a later turn, its hands empty (read 2026-11-22; not
  ported: our faint and fight start are the fight system's, outside the mind's steps that wait for that let-go); and the stroke's or slap's own action (`CreatureMindSystem::ForceAction`),
  whose original start of the reaction against an object state still playing is not read. The states it leaves at their pace (the v1.42 exe's byte table at 0x48F780 sets the flag for
  2..12, 14, 16, 22..25 and 37 only, as bw1-decomp's switch) are left at their pace in ours too since 2026-11-15: the
  faint's fall, lying and getting up (0x1E, 0x24, 0x1F, the only users of `holdLoop`) by `creature_layers::Hurry`
  (raffclar's 9d0f6d494, test `CreatureLayers.AFaintKeepsItsPaceWhenHurried`); the throw (0xD), the gift from the
  hand (0xF), the kiss (0x11) and the kick (0x12) by `creature_object_actions::HurriedByForcedFinish`; the catch
  (0x13..0x15, `CreatureCatching`) and the turn on the spot (0x23, the locomotion's) are never hurried; the block,
  the fight's cast and its faint (0x1A..0x1D, 0x20..0x22) by `creature_fight::HurriedByForcedFinish`.
- The catch's take, read 2026-11-15 (`DoCatchAction` 0x488286..0x488301): `RemoveObject(thing, 0, 1)`, `DropHeldObject`
  0x48CC20 (the held object, linked, its human shadow off), then for a Living `Living::StorePreviousState` 0x417040
  (vt +0x8EC) and `Living::SetTopState` 0x5F28E0 (vt +0x8E8) with 0x18, IN_HAND for a villager and an animal alike
  (neither overrides the two), and the creature's carried object; no `InterfaceSetInMagicHand`. openblack's take is
  the same since 2026-11-15 (`HoldCaught` in `CreatureObjectActionSystem`, `animal_ai::PlaceInCreaturesHand`; test
  `AnimalSystemTest.AnAnimalACreatureCatchesIsHeldAndStaysInItsFlock`): before, it took the thing as the hand does,
  which also set the villager's in-hand mark, let a kissing partner go, moved an animal out of its flock into one of
  its own and gave an artefact to the creature's player.
- The belief the creature holds about itself (the plan's activity object) is not kept by openblack's minds.
- openblack's planner does not weigh "Catch" on its own.

The thing's pick-up test (step 5) is `GamePhysicsHooksWorld::MayBeCaught`: the kinds above, a villager not at home, and
the cannot-be-picked-up flag alone (`object_flags::HasCannotBePickedUpFlag`). A beach temple puzzle piece's own refusal
(`hanoi_puzzle::RefusesPickUp`, the original's vt +0x180, `HanoiBlock::IsCannotBePickedUp` 0x6DE440) is not asked: the
pieces' vt +0x628 is `MobileObject::CanBePickedUp` 0x425B30 (`xref.py vtable HanoiBlock`), and neither the walk nor its
two helpers 0x47C960 / 0x47C9D0 call vt +0x180. Before 2026-11-14 openblack read `object_flags::IsCannotBePickedUp`,
which adds that refusal, so a creature was kept from catching a base, a covered ring or a ring of a restless or gone
puzzle; test `CreatureCatch.APuzzlePiecesOwnRefusalDoesNotStopTheCatch`.

### The faint of behaving strangely

`creature_odd::BehaveStrangely` keeps the draws in the exe's order (`random(2)`, then the five to ten seconds), and
the faint's static step (`Step::faint`) follows the exe: as it starts, those near react as to a death and the mind is
out cold (`IdleMind::outCold`); on the first turn the body is seen lying (the body's loop, which starts on the same
test and in the same turn as the exe's 0x24, the mind's turn running before the body's as `ProcessSubAction` runs
before `UpdateTime`) it ends the loop, opens its eyes (state 0x1F's eye mode 2) and stays on the step for good, its
life unchanged. `IsUnconscious` is false from the get-up on. While out cold the creature plans nothing
(`PlanCreature`), does not faint for its needs and takes up no reaction (`IsAvailableForReaction`'s `fainted`); on the
first turn it has nothing left to do it comes to (`creature_mind::ComesTo`) and chooses nothing that turn. Pinned by
`CreatureOddAgendas.BehaveStrangelyGetsUpAsItLiesAndStaysOnTheFaint` and `OutColdComesToWithNothingLeftToDo`. The
debug panel's mind readout shows the faint step and "Out cold". A vanilla bug, kept: see
[the creature's vanilla bugs](../project/progress/vanilla_bugs/creature.md).

What differs: the exe's ways out of the stall are ours only where ours has them (a script's plan, the cut scene's
look); the locked select's restart of the same action is not ported. The get-up's clip starts to move
one turn later than the exe's, as every static step's end does in ours (the body's loop is left on the turn its end
is asked for without moving the clip, where the exe's `StateSwitch` moves the finish in that turn's `UpdateTime`): a
body-layer row of its own.

### Test hooks

- The testbed scenario `physics.rock_strikes_creature` (the strike, the crush and the sway).
- `test_creature_catch` pins 20 of the oracle's reach cases.

## Pending

- **Struck by a thrown thing** ([Struck by a thrown thing](#struck-by-a-thrown-thing)):
  - the sway, still open: the leans' yaw while a step-off plays is our locomotion's heading of that turn
    (`CreatureLocomotionSystem`, through `creature_locomotion::SpinlessYaw` when the turn has no spins; the earlier
    `LerpHeading` is gone), the same form as the original's term (`LH3DCreature::UpdateBuffers`, the heading plus the turn's share by the
    turn animation's time), read at the same turn's time; the leans read the Transform's rotation, whose layout is the
    original's yaw matrix (`affine::AngleY`), its cosine and sine the original's bits (oracle, 40001 headings, 2026-11-25;
    test `CreatureSwayOracle.TheLeansRotationHasTheOriginalsBits`); the
    turn's seconds take our playback scale, below 1 while frozen, for physical +0x48, which the original sets to 1.0
    with no other writer found (🔍 inferred, as `creature_turn_pose::LivedMs`); the leash's push on the sway
    (`LeashForce`) is left out on purpose: it runs only while the pull is above 0.05, which no running path sets ("The
    leash drag is set by no running path"), so leaving it out is identical, and its holder point (the leash's
    interface status +0xC8) is not read;
  - the effect's place, which the source carries and nothing reads (the football counts as a toy since 2026-10-28,
    as above); the fight's skip is read
    (`EffectValues::blow`: a struck fighter takes no fight part, [fights](fights.md#miracles-blocking-and-fainting));
  - whether any land script reaches a creature with a thrown thing in the check runs: to be measured.
- **The catch** ([The creature's catch](#the-creatures-catch)):
  - which throws carry another player's status (+0x24): only the creature's own launches pass a status (see
    [the reach test](#the-creatures-catch)); still to read: sub_500540 0x5007DC (a sub-action stage that pushes the
    byte its object's vt+0x4A4 returns, an odd value for a status), `PhysicsObject::GameTurnUpdate` 0x645FB3 (ebx, 0 on
    the path read) and sub_4259B0 0x425AE9 (pushed far earlier, not followed);
  - the creature list's order (the catch asks the creatures by their entities' numbers, `GamePhysicsHooksWorld::Creatures`, though their own turns walk the living list since 2026-11-23), the hurry of `EndAnyAnimsRapidly`,
    the take's states and whether the planner weighs "Catch" itself are not ported (above);
  - what a fragment's info weight is was not read;
  - +0x51EC in `StartCatching`, and `StateSwitch`'s end request +0x5238 for 0x13, are not read.
