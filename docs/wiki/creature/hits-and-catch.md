# Hits and catches

## Summary

What a creature does when a thrown thing strikes it, and when a thing starts to fly near enough for it to catch.

A strike that is not a toy sways the body on its springs, crushes the creature by the impact over its weight, sours
its attitude to the thrower's player and feeds its fear and anger. A thing thrown near a free creature may make it
plan a catch: it steps, reaches and takes the thing into its hand if its timing allows.

openblack runs both: the strike (`creature_struck`), with the sway, and the catch walk and its turn-by-turn catch
(`creature_catch`, `CreatureCatching`). The reach test is bit for bit; the side and height of the catch are measured
in openblack's own axes.

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
   / 0x48D16F); 24 sites pass 0 [V]) with a `GetPlayer` that is not the creature's and not `IsAllied`
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

## openblack

Both are done. Porting decisions: [Port decisions: the creature](../project/port-decisions-creature.md).

### The strike

`PhysicsGameHooks`' Creature case gathers the strike (`ecs::creature_struck::Strike`: the striking entry, its thing
and centre, whether the hand threw it and whether the creature's own entry took that, the turn's force and mean force,
the owner, `ScriptControlled`, `creature_pose::DrawnSize` and the thin-fat and weak-strong of
`creature_pose::TargetMorph`), and `creature_struck::HurtCreature` takes the steps above against
`physics_hooks::World`: the toy test (`World::IsToy`), the sway (`World::KickSway`, below), the local player's
scripted creature (`World::LocalPlayer`), `living::CreatureMass` and `living::CreatureCrush`, the crush (`World::Crush`,
scaled, then `World::ApplyEffect` as a blow from the striking thing, put down to the hand's player when the hand threw
it, or to no player), the feedback when the own entry was the hand's and the creature is the local player's
(`World::UpdateAttitudeFromFeedback`), then the sources 18 and 9 (`World::ChangeDesireSource`). An entry's "status" is
openblack's `byPlayer` flag: the hand's throw, which the struck entry takes from what struck it (the same rule as the
original's step 6 of the manager). The testbed scenario `physics.rock_strikes_creature` shows it.

Not ported in this path: openblack has no football, so only the toy statics are toys; the effect's place (the values
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
`Reaches` is worked out in the game's order and matched the original on 4000 random cases and 40 at the edge of the
reach (the oracle); 20 of them are pinned in `test_creature_catch`. The plan is the mind's `ForceActivity(Play,
"Catch", the thing, score 1)`, the action a forced-only executor (`creature_plan_actions::ForcedFor`) whose agenda is
one object step; its order starts `CreatureObjectActionSystem::Catch`, and the catch is then the `CreatureCatching`
component, a turn at a time in `ProcessTurn`, starting from the turn after its start (the sub-actions run after the
body).

What differs:

- The creature list's order is the order creatures were added to it; openblack asks them by entity number.
- openblack gives the player's hand's throws the local player (`byPlayer`) and every other none, so only a rival's
  creature draws, on the player's throws. No players are allied in openblack.
- The thing's weight is the physics' body weight (`DynamicsSystem::WeightOf`): `GetWeight` but at least 0.01, a
  fragment's 30 × its area (the original's fragment weighs scale³ × its info's weight). A fragment can be caught by its
  class.
- `EndAnyAnimsRapidly` (+0x5730, the body's action played at twice its speed until its state changes) is not ported:
  openblack's forced plan ends the body's action at once (`Replan`), so there is nothing left to hurry.
- `IsDestinationValidAndClear`'s route circles: openblack's routes don't lay them out; only "not moving" is tested.
- The look at the thing in 0x13 and 0x14 (`UpdateLook` sets the look point) is not ported.
- The catch's take uses openblack's hand take (`InterfaceSetInMagicHand`), as the creature's pick-up does, not
  `StorePreviousState` + `SetTopState(IN_HAND)`.
- The belief the creature holds about itself (the plan's activity object) is not kept by openblack's minds.
- The side and height are measured in openblack's own creature axes, not with the game's heading sines, so they are not
  bit-exact; the reach test is.
- openblack's planner does not weigh "Catch" on its own.

### Test hooks

- The testbed scenario `physics.rock_strikes_creature` (the strike, the crush and the sway).
- `test_creature_catch` pins 20 of the oracle's reach cases.

## Pending

- **Struck by a thrown thing** ([Struck by a thrown thing](#struck-by-a-thrown-thing)):
  - the sway, still open: the leans' yaw while a step-off plays is our locomotion's lerped heading (`LerpHeading`),
    the same form as the original's +0x5220 term but not compared bit for bit; the leans read the Transform's
    rotation, whose layout is the original's matrix (`affine::AngleY`), the angle's bits not compared with +0x84; the
    turn's seconds take our playback scale, below 1 while frozen, for physical +0x48, which the original sets to 1.0
    with no other writer found (🔍 inferred, as `creature_turn_pose::LivedMs`); the leash's push on the sway
    (`LeashForce`) is left out on purpose: it runs only while the pull is above 0.05, which no running path sets ("The
    leash drag is set by no running path"), so leaving it out is identical, and its holder point (the leash's
    interface status +0xC8) is not read;
  - the football as a toy, once there is one; the effect's place and the fight's skip, which the source carries and
    nothing reads (they come with the fight's miracle side, R22);
  - whether any land script reaches a creature with a thrown thing in the check runs: to be measured;
  - the hitter: the original's physics keeps an earlier turn's hitter when the body it last hit is not listed
    (openblack's forgets it every turn); queued as a fix in the physics.
- **The catch** ([The creature's catch](#the-creatures-catch)):
  - which throws carry another player's status (+0x24): only the creature's own launches pass a status (see
    [the reach test](#the-creatures-catch)); still to read: sub_500540 0x5007DC (a sub-action stage that pushes the
    byte its object's vt+0x4A4 returns, an odd value for a status), `PhysicsObject::GameTurnUpdate` 0x645FB3 (ebx, 0 on
    the path read) and sub_4259B0 0x425AE9 (pushed far earlier, not followed);
  - the creature list's order, the hurry of `EndAnyAnimsRapidly`, the step's route circles, the look at the thing,
    the take's states and whether the planner weighs "Catch" itself are not ported (above);
  - what a fragment's info weight is was not read;
  - +0x51EC in `StartCatching`, and `StateSwitch`'s end request +0x5238 for 0x13, are not read.
