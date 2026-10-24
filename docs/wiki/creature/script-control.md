# The creature under a script

## Summary

The scripts of a land move the creature, take it under their control, make it play animations, send it walking, force
an action on it and make it look at the camera. A creature under a script's control stops thinking: its desires are not
updated and it plans and mimics nothing; it only carries out the sub-actions the script gave it, and its head looks at
the script's focus. PLAYED tells the script when what it gave the creature is over. In a cut scene (the wide-screen
bars of a script) every thinking creature is made to look at the camera.

openblack places the creature, takes and releases the control, plays what a script gives it, answers PLAYED, sends it
walking, carries out the forced looks and the scripted sit, and the looks at the camera, each as the original's plan
{ObeyPlayer, FollowPlayer} or the forced action. The commands' own side (their pops and their checks) is on
[Script commands](../scripts/script-commands.md); this page is the creature's side.

## The original

Read in the v1.42 `runblack.exe` with the 1.20 symbols ([V] read in the code; 📼 1.20 only likely; [?] unknown). `msPerTurn` is
[0xD01A38], set to 100 by `GGame::Init` 0x54F4A5 (and by the `MapCommandProcess` setup command 0x714DBE), so 1000 /
msPerTurn = 10 whole turns a second.

#### A script putting a creature somewhere

`SET_POSITION` on a creature (`GScript::SetPosition` 0x6F8A00; bw1-decomp has only its table entry; the command is on
[SET_POSITION](../scripts/script-commands.md#set_position-024)) [V]:

- The thing is refused, with nothing moved, when it is in a hand (+0x24 bit 4, "Setting object in hand to a position"),
  flying in the physics (0x40, "Trying to set position - Object is flying"), not in the map (bit 1 clear) or no longer
  valid; every message is silent in the shipped game (`ScriptErrorMessage` 0x6F62B0 is a bare `ret`).
- A creature (`CastCreature` vt +0xA4, 0x6F8B15) then goes through `Creature::ForceMoveMapObjectWithoutWalking`
  0x479EB0 and the native returns (0x6F8B2B), before the IsLiving / +0x25 & 4 test: SET_POSITION never puts a creature
  in script state 4, and script control makes no difference.
- `ForceMoveMapObjectWithoutWalking`: the creature is put on the ground there (the given height dropped; the altitude
  0 unless it is a script marker; the body to (x, ground, z) through `Morphable::SetPos`), moved in the map
  (`Creature::MoveMapObject` 0x479E50: the mountains and water it looks at, its long-range vision; not when +0x10CC,
  carried by a creature, is set), its size given again (`SetSize`), and `LH3DCreature::ResetLook` 0x4831B0: the route
  and its plans freed and its follower reset at the new place, the pending turn dropped, `StateSet(0)` (the 3D state,
  the speed and the animation to nothing), the look target zeroed (openblack: `CreatureAnimation::lookAt` dropped after `Place`, raffclar #567) and the body
  posed again, the pose copied into the
  last one so that nothing blends from the old place. Its yaw, the turn-start map coords (+0x2C, copied only for
  things that are not creatures), the mind's action, plan and agenda, the leash and the home are left as they were. No
  draw was found (some virtual calls of the long-range vision's beliefs are not resolved [?]).

#### In its hand, for a script (IN_CREATURE_HAND)

`IN_CREATURE_HAND` (076, 2 arguments, 1 result) is `GScript::InCreatureHand` 0x6F3DE0 (bw1-decomp has only its
table entry) [V]:

- It POPs the creature first (0x6F3DEE), then the thing (0x6F3E09), each through `GetScriptGameThing` 0x70D220.
- Either one missing: `ScriptErrorMessage` "Invalid creature or thing" (0xC0CEBC, 0x6F3ECA) and false pushed.
- `__RTDynamicCast` of the first to `Creature` (0x6F3E38). Not one: "Not a creature" (0xC0CED8, 0x6F3E46) and false
  pushed (0x6F3E5C, the null cast result pushed as the bool); then it **does not return**: it falls through to
  0x6F3E61 and reads the creature's physical (+0x160) through the null pointer, so the original crashes there. The
  false it had already pushed is its only answer. A crash in vanilla; see
  [vanilla bugs: engine](../project/progress/vanilla_bugs/engine.md).
- A creature: true when its physical (+0x160) +0x28, the thing in its hand, is the thing (0x6F3E61..0x6F3E76).
- Else true when the current sub-action of the agenda's sub-action list (mind +0x164, the list at +0xFA8: current
  index +0xFB4, entries of 0x60 from +0xFD0) has the action (+0x8, so mind +0xFD8) 2, `CREATURE_SUB_STATE_ACTIONS_EAT`,
  and its belief (+0xC, mind +0xFDC) is set and points (`CreatureBelief::Pointer` +0x30) at the thing
  (0x6F3E7F..0x6F3EC1). The index is read without the count test (+0xFBC).
- Otherwise false (0x6F3ED7).
- The messages are silent in the shipped game (`ScriptErrorMessage` is a bare `ret`).

#### A creature under a script's control

##### Taking control: SET_FOCUS

- **`SET_FOCUS`** (`GScript::SetFocus` 0x6F90B0) pops z, y, x and the object. A creature: `SetControlledByScript(1)`
  (vt +0x440, `Creature::SetControlledByScript` 0x47D2C0: creature +0x24 bit 10, that is +0x25 & 4), then
  `Creature::SetFocus` 0x4F6760: focus on (mind +0x1D3F4 = 1), of a position (+0x1D3F8 = 1), the point at +0x1D400,
  then `PrepareCreatureForScriptedAction(1)` 0x4F6A90 (called by `SetFocus(point)` at 0x4F67AB and `SetFocus(object)`
  at 0x4F688B), then `AddSubAction(7 TurnToFacePos, point, LookAtPosition)`. A script container's members get their own
  SetFocus with no SetControlledByScript; another object, a Y-angle snap.
- **`PrepareCreatureForScriptedAction(1)`**, in order:
  1. `SetControlledByScript(1)` only if not already controlled (`GScript::SetFocus` has already done it).
  2. The stop: `FinishActionUnsuccessfully(reason = null, 1, 1)` 0x475730: if the body has a partner (+0x528C)
     `ModifyStrength([0xC6DAF0])`; mind +0x1CEA0[current action] = 0; mind +0x1C14 = mind +0x1C18 = 0;
     `FinishActionGeneric(1, 1)` 0x475880 → `StopWhatIAmDoing(1, 1)` 0x4758D0, then, only if mind +0x1BE8 (a partner):
     mind +0x20D30 = 0, mind +0x1BE8 = 0, `FinishActionUnsuccessfully("Stopping because my partner told me to")`. No
     spinner text (the reason is null, and it needs [0xD00DF8] anyway).
  3. `StopWhatIAmDoing(1, 1)`, in order: the current sub-state's stop member (table 0xBE0C18) if sub-actions are
     queued; the release of mind +0x21C0 (`fn_0073D5A0`); creature +0x10D4 = 0; the dead-body check; `fn_004F4890`
     (particles +0x11E4 closed, +0x11E8 / +0x11F0 = 0); the novelty credit (below) with mind +0x21F8; mind +0x21F8 = 0;
     body +0x28 = 0 unless LH3D +0x48D0; `CreatureAgenda::Clear` 0x4D3760; the plan's desire mind +0xF50 = 40;
     `fn_004FF5C0` on mind +0xFA8 (the sub-action agenda emptied: +8 = 1, +0xC / +0x10 / +0x14 / +0xC2C / +0xC34 = 0,
     +0x1C = −1, the 32 slots' +0x34 = 0); `fn_004F81F0` (the held miracle and particles +0x1078..+0x1084 released,
     [The release](casting.md#the-release)); `fn_004F4C30`; `Dance::SetState(1)` if dancing;
     `SetRequiredSpeed(CreatureInfo +0x254)` unless the player's +0x8E0 == 2; mind +0x1BE0 / +0x1BE4 = 0; the body's
     partner told to stop; `fn_0048F710(1, 1)` on the body (state 6 → 7, 3 → 4, 2 / 5 flag +0x5238, a camera-mode case
     for 0x16..0x22, 8 → 9, then `StopMoving`); mind +0x20CEC = 1 (perception back on); `fn_004791E0` (the +0x1090
     list cleared); mind +0x21F4, +0x1C74, +0x658 = 0; `StopReacting` (vt +0x998) if creature +0x94; creature +0x380,
     +0x1068 = 0; the smoke +0x109C released; the object +0x10A0 deleted; mind +0x1C00 / +0x1C14 / +0x1C18 = 0;
     creature +0x10B0 / +0x10AC = 0; `fn_00475CC0` (LH3D +0x546C = 5000, the blink interval; +0x5460 = 2; +0x5464 =
     clamp(1 − 2·body +0x30, −1, 0.73)); creature +0x11B8, mind +0x1C3C, creature +0x10F4, mind +0x1AA60..+0x1AA6C, mind
     +0x1C10, creature +0x1118, +0x12A8 = 0.
  4. The goal list is refilled, once, by that `CreatureAgenda::Clear`: `CreaturePlanState::Initialise` 0x4F13E0 on mind
     +0x718 (+0x780 = +0x784 = 40), then `SetupRemainingGoalsToPlanFor` 0x4F1480, which pushes every desire that is
     activated and not suppressed (desires +0xA8 + 4i == 0), from 39 down to 0, into mind +0xEA0 (count mind +0xF44).
     `Clear` also zeroes the plan's +0xC..+0x2C (`SetToZero` 0x4F12B0 at mind +0xF48, a folded symbol) and agenda fields
     (mind +0x21F8, +0x21FC, …).
  5. The scripted plan: `CreaturePlan(desire 24, action 22, b1 = null, b2 = GetBeliefAboutObject(self), b3 = null,
     FLT_MAX)` (0x4F1230 → `fn_004F1270`: +8 the desire, +0x18 the action, +0xC / +0x10 / +0x14 the beliefs,
     +0x1C..+0x2C the priorities, all FLT_MAX), copied into mind +0xF48 +4..+0x2C (the vtable kept). It is valid
     (`IsValid` 0x4F12E0: desire < 40, the +0x10 belief, action ≠ 0). Desire 24 is `CREATURE_DESIRE_TO_OBEY_PLAYER`
     (bw1-decomp `include/chlasm/Enum.h`; openblack's `Desire::ObeyPlayer`); action 22 is `CREATURE_FOLLOW_PLAYER` (`include/chlasm/CreatureEnum.h`;
     info.dat action row 22 "FollowPlayer", the table at file offset 0x4BD40, rows of 0x100; row 0 "Undefined", row
     314 = 0x13A "LookAtCameraInWideScreen").
  6. `fn_004FF5C0` on mind +0xFA8 again: the sub-action agenda emptied, so SetFocus's TurnToFacePos that follows is the
     only sub-action.

  Not set: no desire value, no suppression, no activation, no cheat, no priority beyond the plan's own FLT_MAX.

##### What runs while controlled

`Creature::ProcessState`'s mind block is 0x4735A9..0x4737A0, gated on `[0xD00DDC] && mind +0x20CE4 && !(creature +0x25
& 4) && mind +0x20D30 == 0`. While controlled the block is skipped: no `UpdateDesires`, no `DecideWhatToDo` (so no
planning and no mimicry), no no-plan bookkeeping, and fn_004D37F0 does not run. When the gate fails (0x4737A0),
`ProcessSubAction` runs if mind +0x20CE4 is set, without `fn_004D37F0`, so the plan-turn counter mind +0x21F8 does not
grow while the creature is controlled. The faint (fn_0047CF00) and fn_0047CFB0 test the control themselves. The block's
own calls, in order [V]:

| Address | Call | What it does |
|---|---|---|
| 0x4735DD..0x47361D | `fn_004C39F0` × (0x3D8 / (1000 / msPerTurn)) = 984 / 10 = 98 times a turn (984 = 3 × 328, the action count) | `GetActionPriority(action creature +0x1098, the plan at mind +0x718 + 0x30·table)` 0x4C3990, the result thrown away (`fstp st(0)`), then creature +0x1098 = (+1) mod 0x148. `GetActionPriority` and `IsConfinedToArea` are pure and +0x1098 is read nowhere else (written only by `Creature::Initialise` 0x474489): dead work, a round-robin index. |
| 0x473621 | `fn_004CA670` | unless `HelpSystem::IsDialogueControlled`, walks the 42 lists at creature +0x220 (8 bytes each) and, for the first entry whose +0x18 is set, calls `fn_004C9FE0` (the help spirits' messages), stamps +0x14 = the game turn, clears +0x18; at most one a turn |
| 0x473633 | `UpdateHelp` 0x4C9C90 | only if the creature has a player |
| 0x47363E | `fn_004C4D20` | the list at mind +0x1A9F4: each entry's +8 ×= 0.999999, +0xC ×= 0.9999 |
| 0x47364D | `CreatureDesires::UpdateDesires` 0x4DC430 | first `UpdateSources` 0x4DE620 (each source: its reader from table 0xC8D478 if any, then × its multiplier; then desires +0x704 (mind +0x70C) ×= 0.99), then for each activated desire: the suppression countdown (desires +0xA8 + 4i, −1 if above 0), value += the sources' increment if above 0 and not suppressed, else value ×= its decay (desires +0x288 + 4i); clamped to the species floor (CreatureInfo +0x274) and the desire's maximum (desires +0x468 + 4i); the sum into desires +0x654 |
| 0x47365B | `StorePreviousValues` 0x4DC500 | a bare `ret` in this build |
| 0x47368E | `MakeDesireMostDominant(23 Rest, info, 1.5)` | if creature +0x10CC and the body is moving |
| 0x47369D | `fn_004F0370` | if creature +0x10E8 == 0: `DecideWhatToDo` and the rest |
| 0x4736E9..0x473708 | `ForceActivityAndForceAction(plan(24, 0x13A))`, mind +0x1C14 = 1 | the look at the camera in a cut scene ([below](#looking-at-and-pointing-at-the-camera)) |
| 0x473767 | plan valid and sub-actions left: `fn_004D37F0` (mind +0x21F8 += 1, the turns spent on the plan) then `ProcessSubAction` | |
| 0x47376E | no plan: mind +0xF74 = 0, mind +0x21FC += 1, creature +0x384 = +0x380 = 0 | |

The sub-actions still run while controlled, and one of them plans by itself: the stage that applies a desire to a
thing (0x5081F0, the last step of helping or attacking a town with a friend) calls
`Creature::ForceActivityAndDecideAction` 0x4C43A0, which ends the action as a failure (`FinishActionUnsuccessfully`
0x475730), makes the desire fully dominant (`MakeDesireFullyDominant` 0x4DC920), finds the best action for a plan of
that desire on the thing (`FindBestActionForPlan` 0x4F0A70) and takes it up (`ConstructCurrentPlanFromPlan`
0x4F1560). None of these, nor `FindBestActionForPlan`'s own callees (0x4F0C10, 0x4F0C30, 0x4F0C50, 0x4F0D60,
0x4F10A0, `FindBestObjectToUseIfNecessary` 0x4F11B0), reads the control bit (creature +0x25 & 4) or mind +0x20D30
[V, xref 2026-10-17]. So a desire forced on a thing is planned under a script's control too; openblack's
`CreatureMindSystem::ForceDesireOn` passes the script guard of `PlanCreature` for that reason.

What still runs while controlled, outside the block, in `ProcessState`'s order: `fn_0047D580`, `fn_004F48C0`,
`ProcessSpells`, `fn_0047C770`, `ProcessPlayerInterestingness` (which decays mind +0x18C68), the dead check,
`RemoveBeliefsAboutDeletedObjects`, `fn_004EFCE0`; [the limbo jump at 0x472F77]; `fn_0047DC50`, `UpdateLeash`, the
leash countdown; the confined-area block (itself skipped when controlled, 0x47312A); the cheat let-go; under
[0xD00DE8]: `fn_00477130` (the map object put at the body's position), `fn_004C58D0` (a bare `ret`), `fn_004EF510` (if
[0xD00DE0] and phase ≥ 1) or the zeroing of table-flagged desires, `fn_004DF620` (the cell marked explored, the visits
counted), `fn_004D82A0`, `fn_00477A00`; `Creature::Look` (under [0xD00DD8] and mind +0x20CEC); the restart check;
`fn_0047CE80` / `fn_0047CF00` only if not controlled; `fn_0047CFB0` (returns at once when controlled); fn_004D03B0, the
head's look point, which reads the focus; the +0x10E8 countdown; then `ProcessSubAction` and the body. Of the per-turn
bookkeeping a creature's mind keeps:

- **The contexts' age**: no per-turn counter. `CreatureContext::CalculateLearningPriority` 0x4E0600 computes the age
  when asked: `(g_game +0x205A40 − context +0x4C) / (1000 / msPerTurn)`, an unsigned integer division, so whole
  seconds, then `1 − s / window` (table 0xC6C570) while s < window, else 0; 1 while running (+0x48 == 0); 0 if not
  learnable (table 0xC6C590). A turn stamp, so contexts age while controlled as if free.
- **The turns since an action was done**: no per-turn counter. `CreaturePreviousActions` at mind +0x1CE98 (data at
  +0x1CEA0, 328 dwords; the constructor 0x4F1FF0 fills them with `GameRand(120)` if [0xD00DEC], else 0). Zeroed for
  the action done or failed: `UpdateOnCompletion` 0x475F00, `FinishActionUnsuccessfully` 0x475762,
  `FollowerCallbackFunction` 0x475418, `ProcessState` 0x4731FF (the confined-area case). Grown only in
  `StopWhatIAmDoing` 0x475A37: `+= mind +0x21F8` (the turns the stopped plan ran) for each of
  `GetNthActionForPlan(plan, n)`, n = 0..29, that is not the plan's own action. Read by
  `GetActionPriorityModificationFromWhenLastDone` 0x4F0CA0: `min(count, CreatureInfo +0x2A8) × 4 / CreatureInfo
  +0x2A8`, × 0.025 if the action table's 0xC6C538 field is 0, clamped to 0..0.01. Its only source, mind +0x21F8, is
  counted inside the block, so under script control nothing grows.
- **The perceived desires' fade**: `fn_00477A00` (0x47338C) → `fn_004C7EB0` → `fn_004C7F20` (the 40 + 17 values at mind
  +0x18C80 × 0.9995, the constant at 0x8CF3D4) and `fn_004C7F60` (the 30-deep ring of past values, on even game turns
  only). Outside the block (the [0xD00DE8] block): it runs while controlled.
- **The desires of a stage of growing up**: not in `ProcessState`. `Creature::MoveToDevelopmentPhase` 0x4C59C0, called at
  once by `SET_CREATURE_DEV_STAGE` 0x6F4894, `DEV_FUNCTION` 0x6FC17F, `Config::ProcessOneGameTurn` and packets:
  creature +0x1268 = the phase, +0x126C = 0, `UnsuppressAllDesires`, all 40 deactivated, then phases 0..phase add (10
  slots) and remove (4 slots) from table 0xC848C4 (stride 0x84), creature +0x194 = 0, `CreatureAgenda::Clear` (which
  refills the goal list); also creature +0x11B4 = 0 if confined, not leashed and phase ≥ 5. Event-driven, so it happens
  during control, at the script call.
- **"Seconds alone"**: two turn counters in `CreatureAttitudeToPlayer` (mind +0x18C40), both in `fn_00477A00`: while the
  player's interface looks at the creature (`GetInterfaceStatusLookingAtCreature`) +0x18C48 += 1, +0x18C50 += 1,
  +0x18C4C = 0; otherwise +0x18C4C += 1 and +0x18C48 = 0. Always +0x18C54 += 1, zeroed only by `ForceActOnObject`
  0x4C629B / `ForceActOnMapCoord` 0x4C6B11. Source 36 (0x4DEA10) is the whole seconds of +0x18C4C / 60, source 37
  (0x4DEAD0) the whole seconds of +0x18C54 / 120, both clamped 0..1. Outside the block: they count while controlled.
- **The feedback time**: no counter. `UpdateAttitudeToPlayerFromFeedback` 0x4E1080 → `fn_004C81D0`: the feedback into
  mind +0x18C5C and the game turn into mind +0x18C60 (and the attitude +0x18C58 = 0.7·old + 0.05·feedback). No direct
  reader of +0x18C60 was found by displacement: a stamp.
- **The cheat-dominant let-go**: `ProcessState` 0x473295..0x4732F5: if mind +0x700 (the dominant desire, set by
  `SetCheatDominantDesire` 0x4DCE34) ≠ 40: `UnsuppressDesire(it)`, mind +0x704 += 1, and once `mind +0x704 / (1000 /
  msPerTurn) > mind +0x708`: `UnsuppressAllDesires`, mind +0x700 = 40. Every path from the leash and the confined-area
  code reaches 0x473295: it runs while controlled (only the limbo jump at 0x472F77 skips it).

##### Turning to the focus

- **TurnToFacePos** (sub-state row 7: stage 0 0x501660, stage 1 0x501730). Stage 0: the target is the sub-action's
  point (mind +0xFE4 / +0xFEC of the slot mind +0xFB4), the body's position LH3D +0x78 / +0x80. If the flat distance
  (`fn_006E8160`, √(dx² + dz²)) is below 0.1, it returns 2 (no turn); the yaw is the angle of (dx, dz) (0x7FAA50, a
  folded symbol), and if |wrap(yaw − LH3D +0x84)| ≤ 0.392699 (π/8, 22.5°, the 0.3927 rad of a facing turn; the wrap 0x7FAAF0 into ±π) it returns 2;
  else `fn_004F00E0(yaw)`: `StopMoving`, then `StartTurningAction(yaw)` 0x481160, which only when
  `IsPerformingBodyAction` is false (body state 0) calls `InitialiseTurning(yaw)` 0x48B790 and `StateSet(0x23)` and
  returns 1. Stage 0 returns 2 if the turn started, else 0: it stays in stage 0 and tries again the next turn. Stage 1
  waits for the turn to end; then the plan finishes successfully (`StopWhatIAmDoing`: the plan invalid, the goal list
  refilled).
- **Every body turn** goes through the same `InitialiseTurning` and state 0x23 (`fn_0048B970`; see
  [the turn on the spot](pose-and-animation.md#the-turn-on-the-spot)). The callers of
  `StartTurningAction`: `fn_004F00E0` (TurnToFacePos stage 0, and 0x504146 in sub-action 87
  TurnToFaceOtherPlayersCamera's stage 1, 0x504070 [V]; FaceDownSlope is row 68, whose stage 0 is 0x504160), `fn_0048F7B0` (the plain "turn to this yaw"), `StateSet`'s tail 0x484F19 (a deferred turn) and
  `CreatureRoom::Draw` (the temple). The navigation step `fn_00483AB0` calls `InitialiseTurning` directly (no state
  0x23, turning inside the move, threshold 0.261799 = 15°). `fn_0048F7B0` differs only when the body is busy: in state 0
  it starts the turn (with LH3D +0x5240 = the yaw); otherwise it ends the body action (`fn_0048F710(1, 1)`, which also
  stops moving) and queues the turn (+0x523C = 1, +0x5240 = the yaw), which `StateSet` starts when the body returns to
  a state. TurnToFacePos instead calls `StopMoving` first and, with the body busy, does not end the action but tries
  again the next turn, with its own thresholds (0.1 m, 22.5°).
- **The turn itself**, `InitialiseTurning` 0x48B790 and state 0x23's timing, is on
  [the turn on the spot](pose-and-animation.md#the-turn-on-the-spot): no angular rate, so a turn lasts one length of
  the (blended) turn animation whatever its angle, the same for TurnToFacePos and a plain turn.
- **Draws**: none. Stage 0, `fn_004F00E0`, `StopMoving`, `StartTurningAction`, `InitialiseTurning`, `StateSet(0x23)`
  and the turning action draw nothing (from 0x501660 the walk reaches one `GameRand`, `StateSet` 0x484F4D, which is only
  for state 6; from 0x48B970, none). The only possible draw is indirect: the sound events `CheckSounds` queues are
  played later by `UpdateTime` → `fn_004818D0` → … `fn_004ED250` with `LocalRand`, only if the turn anims carry sound
  events.

##### The focus and its release

- **The focus** (bw1-decomp `CreatureMental.h`: +0x1D3C0 is `CreatureLookState`, 0x38 bytes, ending at 0x1D3F8, so
  +0x1D3F4 is its `field_0x34`; +0x1D3F8..+0x1D40B is `field_0x1d3f8[0x14]`: +0x1D3F8 the kind (0 object, 1 point),
  +0x1D3FC the object, +0x1D400 the point). Its readers, every access in the exe:
  - **`fn_004D03B0`, every turn** (`ProcessState` 0x473593, outside the mind block, so also while controlled): if
    +0x1D3F4, the head's look point (`LH3DCreature` SetLookPoint `fn_00482C90`, stored at LH3D +0x48C0) is the focus:
    the point +0x1D400 (kind 1) or the object's position with its height (kind 0); other kinds, nothing. It returns
    there, so the focus overrides the current sub-action's look function (mind +0x1010 + 0x60·index) and the default
    look point (`fn_004D1870`) ([the look each turn](#the-look-each-turn)).
  - `RemoveBeliefsAboutDeletedObjects` 0x4D866C: an object focus (+0x1D3F8 == 0) whose object is being deleted is
    dropped (+0x1D3FC = +0x1D3F4 = 0).
  - Writers: `SetFocus(point)` 0x4F6773 / 0x4F677F / 0x4F6790, `SetFocus(object)` 0x4F685E / 0x4F6875 / 0x4F6885, the
    release 0x4F706F, `ReleaseObjectFocus` 0x6F5761, `RemoveBeliefsAboutDeletedObjects` 0x4D8696.
- **`RELEASE_FROM_SCRIPT`** is not a Creature vtable slot: `GScript::ReleaseFromScript` 0x6FB380 →
  `GScript::ReleaseControlFromScript(thing, id, 1)` 0x70D540 → (not a script container) `ReleaseScriptThingIntoTheGame`
  0x70F600: `SetControlledByScript(0)` (clears creature +0x25 & 4); the third argument is 1, so no delete;
  `GAudio::RemoveThingMusic`; then a switch on `GetScriptObjectType` (vt +0x4E8; `Creature` 0x47C8B0 returns 12; the
  byte table 0x70F76C, the jump table 0x70F754): type 12 → a direct call to 0x4F7060 (unnamed in bw1-decomp; no pointer
  to it anywhere in the exe), which sets the focus off (mind +0x1D3F4 = 0) and calls `FinishActionUnsuccessfully("finishing
  script", 1, 1)`: the same stop as above (`StopWhatIAmDoing(1, 1)`, `CreatureAgenda::Clear` with the goal list
  refilled, the plan's desire 40, the sub-action agenda emptied, mind +0x1CEA0[current action] = 0, mind +0x20CEC = 1,
  the blink interval reset, …). The focus's kind +0x1D3F8 and point +0x1D400 stay as they were; harmless, since every
  reader tests +0x1D3F4 first. `GScript::ReleaseObjectFocus` 0x6F5761 also clears only +0x1D3F4.
- In Land 1, `CreatureDevSeeHome` focuses the creature on the camera on turn 8 and `LandControl1` releases it on turn
  30: it turns once to the camera (turns 9 to about 11) and stands with its mind stilled until then; on turns 9..30 its
  head looks at the camera point taken at SET_FOCUS time (a fixed point), and from turn 31 (the focus off) it goes back
  to the sub-action's or the default look. On turn 30 the plan is already empty (TurnToFacePos finished on about turn
  10, whose `StopWhatIAmDoing` zeroed the plan's action too), so the release's visible effects are the refill,
  `fn_0048F710(1, 1)`'s `StopMoving` and the focus flag.

##### Playing what a script gives it, and PLAYED

The commands themselves are on
[SET_SCRIPT_STATE, SET_SCRIPT_ULONG, PLAYED and the script states](../scripts/script-commands.md#set_script_state-017-set_script_ulong-020-played-064-and-the-script-states);
the creature's side [V unless marked]:

- **`SET_SCRIPT_ULONG`** (0x6F8770) pops the times, the clip and the thing. A creature: times into +0x1290, the clip
  into +0x128C (0x6F888B..0x6F8893). A script container sets its Villager members only (0x6F8730), never a creature.
- **`SET_SCRIPT_STATE`** (0x6F8370): a container sends every Living member, with no drowning test, through 0x6F8280 to
  `GScript::SetScriptState(Living*, state)` 0x6F82E0; otherwise a Living that is not drowning goes there, and anything
  else is "Object not living for set state" (0x6F8447). On a creature 0x6F82E0 ignores the state:
  `IsAnimIndividual(+0x128C)` 0x47B140 (the clip is 52 or more, compared unsigned) chooses
  `ScriptPlayIndividualAnimation` 0x4F6E30, else `ScriptPlayStaticAnimation(clip, 10.0)` 0x4F6F10.
- Both begin with `PrepareCreatureForScriptedAction(1)` and add `times` sub-actions, then sub-action 133
  `WaitUntilFree` with no argument (0x4F6EF2, 0x4F6FDC). An individual action is sub-action 10 `IndividualAction`
  (a `SubArgumentInteger` of the clip; stage 0 0x501C90 `LH3DCreature::StartIndividualAction(clip)` 0x484B00, which
  draws `GameRand`, tried again each turn until it starts; stage 1 0x501CD0 done once `IsPerformingBodyAction` is 0).
  A static action is sub-action 4 `StaticAction` (a `SubArgumentIntegerAndFloat` of the clip and 10.0; stage 0
  0x500A30 `StartStaticAction(clip)`, which plays clip − 1, clip and clip + 1 as start, loop and end (see
  [the pose](casting.md#the-pose)), tried again until it starts, then sets a count of ftol(10.0 × (1000 / the ms per
  turn [0xD01A38])) turns at +0x58; stage 1 0x500AC0 counts it down and then calls `EndStaticAction`, and for clip 38
  (sitting) also rests the body each of its turns (exhaustion − 0.2 × the sleep's rest, within 0..1, see
  [Asleep, sleeping and sitting](plans-and-needs.md#asleep-sleeping-and-sitting)); stage 2 0x500B50 done once the body
  is free). `WaitUntilFree` (stage 1 0x508830) is done once the body plays no action; its stages 0 and 2 are empty, as
  `IndividualAction`'s stage 2. None of these sub-actions has a look or a face callback.
- **The turns a play takes** (`ProcessSubAction` 0x4FEDA0 and the agenda's step fn_004FF570 / fn_004FF540): one stage
  function is called a turn; a stage that returns 2 moves the stage on (to the next sub-action at stage 3) and the turn
  ends; an empty stage is skipped at the start of a turn, and the skipping goes on in the same turn into the next
  sub-action's first stage that is not empty. So after the last play is over (the turn F its body is seen free),
  `WaitUntilFree`'s stage 1 runs on F + 1 (after the individual action's empty stage 2, or as the turn after the static
  action's stage 2) and the plan is completed on F + 2, its empty stage 2 skipped: PLAYED is false on F and F + 1. With
  no plays, the wait's stage 1 runs on the agenda's first turn and the plan is completed on the next.
- **The sub-action table** (its initialiser 0x4F8D70): row t's name is at 0xBE0B98 + t × 0x90 and its stage functions
  at 0xBE0BE8 + t × 0x90 + stage × 0x10, as `ProcessSubAction` 0x4FEE29 indexes them. Rows read here: 4
  StaticAction, 6 TurnToFaceObject, 7 TurnToFacePos, 8 MoveToPos, 10 IndividualAction, 24 Wait, 68 FaceDownSlope, 81
  PointAtObject, 133 WaitUntilFree, 134 StopBeingOnFire.
- **`PLAYED`** on a creature (0x6F9DF4..0x6F9E7E) is false only while the plan is valid (`CreaturePlan::IsValid`
  0x4F12E0 on mind +0xF48: a desire below 40, an action, the objects its rows need), the sub-action agenda has a
  sub-action left (index +0xFB4 below the count +0xFBC) at a stage (+0xFB8) below 3, and the plan's action (mind
  +0xF60) is not 161 `BeIdle`. Otherwise it calls `ClearSubActionAgenda` 0x4FF5C0 (index, count and stage to 0, every
  step's argument cleared) and answers true. The same busy test is the mind's (`fn_004F0370` 0x4F038D..0x4F03A8) and
  `ProcessSubAction`'s (0x4FEDAE..0x4FEDD2). Stage 3 is a sub-action that is over: on the last one `fn_004FF570` then
  completes the plan ([A plan carried out to its end](plans-and-needs.md#a-plan-carried-out-to-its-end)), so a plan
  whose agenda PLAYED clears is not completed.
- **`PrepareCreatureForScriptedAction(n)`**: n goes through `FinishActionUnsuccessfully(null, n, 1)`,
  `FinishActionGeneric(n, 1)` and `StopWhatIAmDoing(n, 1)` to `LH3DCreature::EndAnyActions(n, 1)` 0x48F710: the 1
  ends a dance, a fight, a static action and pointing; n also stops the walk (`StopMoving`). Every caller passes 1
  but `ScriptMoveToPos`, which passes 0: a creature sent walking is not stopped first. `StopWhatIAmDoing` 0x4758D0
  also calls `DestroySpell` 0x4F81F0 (0x475AAE), letting go of a miracle the creature holds, before `EndAnyActions`
  (0x475B76). `EndStaticAction` 0x4848C0 ends a static action at once: in its loop (body state 3) the end plays
  (state 4); in its start (state 2) the end is asked for after the start (+0x5238). A casting pose is a static action
  too ([the pose](casting.md#the-pose)).
- **The other callers of 0x6F82E0** (`xref callers`: 18 calls in 11 functions) can hand it a creature: FLOCK_ATTACH
  (`AttachToId` 0x6EF63A, 0x6EF781, 0x6EF7B5: any `Living` after a cast, state 27), its dance branch fn_006F0320
  (0x6F0403, state 5), DANCE_CREATE (`CreateDance` 0x6F02F4, state 5) and FLOCK_DISBAND (`DisbandId` 0x6EFE84 for a
  flock's members, 0x6EFEF5 for a dance's, each only when controlled by a script, state 4); a creature among them would
  play its script play. They never do: fn_006F11A0 and `PopulateContainer` (they make villagers and animals),
  `SetPosition` (a creature is placed and the native returns first) and `MoveGameThing` (its creature branch returns
  first), `ReleaseScriptThingIntoTheGame` (its switch on the script type sends a creature, type 12, to
  `ScriptReleaseFromScript`; its two calls are the villagers' and the animals'). `SetStateLoopFunction` is
  SET_SCRIPT_STATE's container loop.

##### Sent somewhere by a script (MOVE_GAME_THING)

[V] The command is on [MOVE_GAME_THING](../scripts/script-commands.md#move_game_thing).

- **`MOVE_GAME_THING`** on a creature: in the map (`IsObjectInMap` vt +0x178, 0x6F8F4A) →
  `Creature::ScriptMoveToPos(point, radius)` 0x4F6B60 (0x6F8F60); out of it, nothing.
- **`ScriptMoveToPos`**: `PrepareCreatureForScriptedAction(0)`, so the walk it was on is not stopped. The first leg's
  distance is the flat distance to the point (x87, `fsqrt`) × 1/6 ([0x8AB268], a float) stored as a float; the most it
  may be is 0.7 ([0x8D15C4]) × the 3D body's +0x90 × 15 ([0x8C2C40]) + 22 ([0x8D15C0]); the creature's
  `GetHeight` (vt +0x42C) is compared with it (0x4F6BD1..0x4F6C05): a height **greater** than the sixth is taken,
  otherwise the sixth, capped at the most. With a radius (0x4F6C09: not 0) both legs are `MoveToPos` sub-actions 8
  (`SubArgumentPointAndFloat` of the point and the radius); without, the first leg at that distance and the second
  at `GetHeight` (0x4F6DB4). Each leg's look callback is `LookAtPosition` 0x4D1460.
- **The two heights are one**: `Creature::GetHeight` 0x477F50, the creature's vt +0x42C (`xref vtcalls 0x42c --class
  Creature`), is `GetCreature3D()` (+0x160 +0x58) +0x90 × 15, the very value the cap reads.
- **`LookAtPosition`** 0x4D1460: the look target is the sub-action's point at x and z on the ground (the MapCoords'
  altitude 0); none while the creature is moving (`LH3DCreature::IsMoving` 0x484300) and nearer than 10 ([0x8AB414],
  `GetDistanceInMetres` 0x74CD50).
- **`LookAtObjectArgument`** 0x4D16D0 (for the agendas below): the object of the sub-action's belief, not when it is
  what the creature holds (Creature +0x160 +0x28); none while moving and nearer than 10 to the belief's position
  (+0x24); the look is at the object's x and z, at its altitude (+0x1C) + its `GetHeight`, its top. With no object,
  the belief's position on the ground. **`LookAtObjectArgumentBottom`** 0x4D1640: the plan's belief (mind +0xF58), the
  same 10 while moving, the belief's position on the ground; it does not test what the creature holds.
- **`LookAtFeet`** 0x4D1980 [V]: the game angle of the way the body faces (`ConvertScawenAngleToGameAngle` of the 3D
  body's +0x84, Creature +0x160 +0x58), then each axis of the creature's position (+0x14, +0x18) is stepped 4 m
  (`0x40800000`) along it in the utilities' way: `(x × [0x8CF860] (10.0) × 2^-16 + GetXByAngle(a, 4)) × 2^16 / 10`,
  `__ftol`, and z the same with `GetZByAngle`. It writes only x and z and answers 1, whether moving or not; the caller
  (fn_004D03B0) zeroes the MapCoords first (0x4D0521..0x4D0529), so the look is at the ground there. Its callers are the
  agendas LookAtReflection (five steps), ThrowStonesInTheSea, ShowFriendMyHome and ShowFriendMyCitadel.
  **`LookDown`** 0x4D10A0 [V], RestOnTheSpot's look, is not it: the creature's own position, altitude 0 (the ground
  under it).

###### The look each turn

fn_004D03B0, from `ProcessState`: a script's focus first (its point, or its object's position and +0x1C over the
ground), and nothing else. Otherwise, when the exhaustion (`CreaturePhysical` +0x30) is above 0.5, the creature is not
under a script's control and mind +0x1BDC / 40 (a signed division) is odd (`imul 0x66666667; sar edx, 4` at
0x4D0471..0x4D048E) [V], the look is set to its own position on the ground, and the turn
goes on. Then, while the plan is valid and a sub-action is under way (the same busy test as PLAYED's), the sub-action's
look callback (the step +0x1010 of the agenda +0xFA8) is called when it has one and mind +0xFC8 is 0; when it answers 1
its point is the look. In every other case, a sub-action with no look callback and a callback that answers 0 included,
`LookAround` 0x4D1870: the interesting thing chosen by the scan (mind +0x1AA40, its point by fn_004D1BB0), else
`GetDestPos` (Creature +0x1214, where it last set off to). So a sub-action with no look of its own looks about at every
stage.

##### An action forced by a script (CREATURE_DO_ACTION)

[V unless marked]

- **`CREATURE_DO_ACTION`** (`GScript::CreatureDoAction` 0x6F3C50) pops the thing it is done with (0 for none, else
  `GetScriptGameThing`), the thing it is done to, the action's row and the creature. No creature: "No creature for
  script" (0x6F3CC4); no thing to do it to: "No object-to-act-on for script" (0x6F3CD5); either missing, nothing. Not a
  `Creature` (the cast at 0x6F3CF9): "No creature for script" again. Then `SetControlledByScript(1)` (vt +0x440,
  0x6F3D20) and `Creature::ScriptDoAction(action, target, with)` 0x4F7000.
- **`ScriptDoAction`**: [0xC64148] = 1, `PrepareCreatureForScriptedAction(1)`, `AddBeliefAboutObject` 0x4D7BD0 for
  the target and for the thing it is done with (if any), then `ForceActivityAndForceAction(24 ObeyPlayer, no activity
  belief, the action, the target's belief, the with belief, 1, 1)` 0x4C4450.
- **[0xC64148]**: written 1 by `ScriptDoAction`, fn_0047B010 (0x47B10D) and sub_4C5339 (0x4C5344), and by `OnClearMap`
  (0x47D837); read by sub_4F6340, sub_4F6410 and sub_4F64D0, the three called from sub_503A50, each of which does
  nothing once it is set (sub_4F6340: otherwise it weighs four desires with
  `CalculateDesirabilityOfProposedActivity` and may send help 40 / 5 through `SendCandidateHelpScript` 0x4C9FE0). So it
  is a once-only switch that stops those creature help prompts.
- **Which desires list the scene actions**: none. In `Scripts/info.dat` the action rows' names lie at 0x4BD40 + row ×
  0x100 (row 1 `MoveToPos`, 161 `BeIdle`, 163 `PointAtObject`, 168 `PointAtCamera`, 188 `LookButDontApproach`, 189
  `LookForever`), and the three desire-action tables follow the 328 rows (30 words a desire: 40, 17 and 40 desires). No
  list holds 163, 188 or 189; `SitDown` (81) and `BeIdle` are in desire 23's, `PointAtCamera` in desire 9's
  (IdleAroundWithPlayer). The rows start 0x14 before their names, so the lists start at 0x4BD2C + 328 × 0x100. So a
  creature plans LookForever, LookButDontApproach and PointAtObject only when forced.
- **A forced fight** (row 21 `Fight`) is built by `ConstructSubActionsForFight` 0x49BCD0, whose main sub-action
  starts the duel: [Fights: a fight a script forces](fights.md#fights-a-fight-a-script-forces).
- **The builders read** (each from the action table only, `_$E90`):
  - `ConstructSubActionsForLookForever` 0x4B0D50: sub-action 6 `TurnToFaceObject` (`SubArgumentObjectAndFloat` of the
    action's object, 2.0) with `LookAtObjectArgument`; then the main sub-action 24 `Wait` of ftol(1000 / ms per turn ×
    86400) turns, also with `LookAtObjectArgument`.
  - `ConstructSubActionsForLookButDontApproach` 0x4B0970: the same turn and look; then the main `Wait` of
    ftol((`GameFloatRand(1.2)` + 1.1) × (1000 / ms per turn)) turns (0x4B0A13..0x4B0A3F), with
    `LookAtObjectArgumentBottom`. The draw is the game's synchronised one, made as the agenda is built.
  - `ConstructSubActionsForPointAtObject` 0x4AEB40: one main sub-action 81 `PointAtObject` (`SubArgumentObjectAndFloat`
    of the object, 5.0) with `LookAtFlyingObject` 0x4D17E0 and `SetFaceForActionAmazed` 0x4C8830. Its stages (read
    2026-10-18 [V]; the sub-action table's entry 0x51 at 0xBE3978, set at 0x4FC454..0x4FC46F): the start 0x5068C0, once
    the body is free, `PointAt` the object's map coords × 0x8AA3A4 with its +0x1C over the land's altitude, and the timer
    +0x58 = ftol(seconds × (1000 / ms per turn)); the step 0x5069A0, each turn, counts the timer down and, until 0,
    **aims again** at the object's place now (`UpdatePointPoint` 0x484AC0), at 0 `StopPointing` and done; the end 0x506A40
    waits for the body. So pointing at an object follows it for the whole point (openblack's `PointAtFor` with a thing,
    from raffclar's #213, does the same). 0x50 PointAtPoint (start 0x506740, step 0x506890 `StopPointing` only) and
    0x81 PointAtArena (0x506820, 0x14 turns, the same step) aim once. The look is not read.
  - `ConstructSubActionsForSitDown` 0x4A4AB0 reads the action's object's position (+0x24). Not controlled by a script
    (creature +0x25 & 4 clear): `FindClearArea(position, 4 × Get2DRadius, 0)` 0x4DFBE0, and with none
    `FinishActionUnsuccessfully("failed", 1, 1)`; found, the walk to the clear spot. Controlled: the walk to the
    object's position itself. Either way the walk is `MoveToPos` within the creature's `Get2DRadius` with
    `LookWhileGoingTowardsPoint` 0x4D0C80 (below), mind +0x1C78 = the position, `TurnToFaceObject` (the object,
    0.1) with `LookAtObjectArgument`, sub-action 68 `FaceDownSlope` with no argument, and the main sub-action 4
    `StaticAction` of clip 38 for `GameFloatRand(5.0)` + 10 seconds ([0x8AB414]) with `LookAround` 0x4D1870 and no
    face.
  - `FaceDownSlope` stage 0 0x504160: of the eight points 10 from the creature (the directions (1, 0), (1, 1), (0, 1),
    (−1, 1), (−1, 0), (−1, −1), (0, −1), (1, −1), shorts at 0xDA59D8 written by 0x74CA50), each made in MapCoords,
    the lowest in the map (the first of equals; none in the map leaves the map's corner); within 0.1 of it, done;
    otherwise a turn to its heading (fn_004F00E0: `StopMoving`, then `StartTurningAction` 0x481160, which refuses while
    `IsPerformingBodyAction`), tried again until it starts. Stage 1 (0x504300) is done once `IsPerformingBodyAction`
    0x4842D0 is 0, that is once the 3D body's state (+0x4994) is 0: the turn (state 0x23) over and no other action,
    walk or static action under way; stage 2 is empty.
- **`LookWhileGoingTowardsPoint`** 0x4D0C80: the current sub-action's point (agenda +0xFA8, step +0x3C) to fn_004D0CD0
  (which `LookWhileGoingTowardsObject` 0x4D0BD0 shares with the object's position and `GetHeight`). It makes MapCoords
  of the point (× 6553.6, altitude 0) and takes `GetDistanceInMetres` from the creature (+0x14): at most 8
  ([0x8C2C70]), it gives nothing (so the head looks about, see [the look each turn](#the-look-each-turn)); at most the
  CreatureInfo's +0x308, the point's ground. Farther, by the look state at mind +0x1D3C0 (fn_004D0B80 sets it as the
  mind is made: all 0 but the sweep's step +0x1D3E0 = info +0x2F0, its distance +0x1D3E4 = info +0x2F4 and its distance
  step +0x1D3E8 = info +0x2FC), when +0x1D3D8 is 1.0 the look of fn_004D0E70, else one of three looks by +0x1D3C0: 0
  the point's ground; 1 fn_004D0E70 (the interesting thing, mind +0x1AA40, its point by fn_004D1BB0; else, within 2 ×
  (1000 div ms) turns of +0x1D3D4, the position and `GetHeight` at +0x1D3C8 of the last moving thing
  `AddBeliefAboutObject` 0x4D7BD0 met, which writes +0x1D3D8 = 0.5; else +0x1D3D8 = 0 and the point's ground); 2
  fn_004D0F30, the sweep: the angle +0x1D3DC grows by the step; at or past info +0x2EC either way the step changes sign
  and the distance grows by its step, back to info +0x2F4 when not below info +0x2F8; the look is the creature's
  MapCoords (its altitude kept) moved by `GetXByAngle` / `GetZByAngle` of `ConvertScawenAngleToGameAngle`(the 3D body's
  yaw +0x84 + the angle) at the distance, in whole units (`ftol((offset + x / 6553.6) × 65536 / 10)`). Then +0x1D3C4
  counts the turn, and past 2 × (1000 div ms) it goes back to 0 and +0x1D3C0 to the next of the three. Only
  `AddBeliefAboutObject` (0.5) and fn_004D0E70 (0) write +0x1D3D8 (`refs` of 0x1D3D8 in the code), so the 1.0 branch is
  never taken. In `Scripts/info.dat` every species has (file offsets, 0x10 below the runtime's) 1.5 radians at 0x2DC, a
  step of 0.1 at 0x2E0, 15 at 0x2E4, 95 at 0x2E8, 10 at 0x2EC and 25 at 0x2F8.

##### Looking at and pointing at the camera

`LookAtCamera` (row 82) is in no desire's list: the player's slight touch and scripts force it. `PointAtCamera` (row
168) is in IdleAroundWithPlayer's list (desire 9, table 1, beside GoToMiddleOfScreen, GoToHand, WaveAtPlayer,
PointAtHand and LookAtHand), so a creature idling with its player may choose it by itself [V]:

- **The camera a creature sees**, `GetNearestCameraInterfaceStatus` 0x47D640: with no player, the single player game's
  own interface status (`MyInterfaceStatus`; none in a multiplayer game); with a player, the nearest of that player's
  interface statuses to the creature (none if it has none). Its camera is the status's +0xB0.
- **`ConstructSubActionsForLookAtCamera`** 0x4A4ED0: one main sub-action 0x42 with ftol(1000 / ms per turn × 5.0)
  turns, its look `Creature::LookAtCamera` 0x4D1A30, its face `SetFaceForActionCuriosity` 0x4C8660 (one of three faces
  by the mind's face count +0x1D414 mod 3, no player test). It never fails.
- **`ConstructSubActionsForPointAtCamera`** 0x4AF760: with no camera status, `FinishActionUnsuccessfully("failed to
  construct subactions", 1, 1)`. Else sub-action 0x1F `TurnToFaceCamera` with the look 0x4D1A30 and no face, then the
  main sub-action 0x50 `PointAtPoint` at the camera (`SubArgumentPointAndFloat`, 1.0 s), its look `LookAtPosition`
  0x4D1460, its face `SetFaceForActionAmazed` 0x4C8830.
- **The look 0x4D1A30**: with no player it gives no point (returns 0); with one, the camera's point, its height above
  the land there.
- **Sub-action 0x42**, stage 1 0x502D70 (the table at 0xBE0BE8 + 0x90 × sub-action + 0x10 × stage, filled by
  0x4F8D70): with no player, done; else 0x502E70 (the camera, 100.0, its turns over the turns per second): within 0.1
  of the camera along the ground, done; more than a right angle off (`angle_correct`, [0x8C7B48]) a turn to it
  (fn_004F00E0); done once the mind's turn count +0x1BDC over the turns per second is past the seconds, once the camera
  is more than 100 away through the air (`GUtils::GetDistance` from the 3D body's +0x78), or, with a player, once the
  camera status's count +0xA0 passes turns per second × seconds.
- **The status's count +0xA0** (fn_005DBC60, from `GInterfaceStatus::Process` 0x5DC4E7): each turn the hand's move
  (+0xA4 − +0xC8) is added to +0xF4; while that sum is shorter than 1.0 ([0x8AA390]) the count goes up by one, else the
  count and the sum go to 0. `SetToZero` 0x5DBA89 clears it. So the look ends once the player's hand has stayed still
  more than five seconds.
- **Sub-action 0x1F**: stage 0 0x503DE0 with no player done; within 0.1 of the camera along the ground done; else the
  turn to its heading (done if the turn says so) and `StopPointing`. Stage 1 0x503EC0: no player done; waits while the
  body acts; done within 0.1 or within 0.2 rad ([0x8AB244]) of the camera's heading; else turns again.
- **The sub-action's turn count** mind +0x1BDC: `ProcessSubAction` 0x4FEDA0 adds one to it (0x4FF0AB) right before
  each call of the current stage, so its first call reads 1; a stage with no function is passed over at once, without
  a count (0x4FEE33). It goes to 0 when the agenda moves to its next sub-action (fn_004FF540 → fn_004FF5B0, from
  fn_004FF570 once the third stage is past) or is cleared (`ClearSubActionAgenda` 0x4FF5C0); besides, only 0x502D53
  and 0x506F10 write it (every write of +0x1BDC and of the agenda's +0xC34, by a scan of `.text`).

###### Looking at the camera in a cut scene

`LookAtCameraInWideScreen`, row 314 [V]:

- **Who forces it**: `Creature::ProcessState`, every turn of every creature whose mind thinks (the block from 0x4735A9:
  [0xD00DDC] set, mind +0x20CE4 set, not under a script's control (+0x25 & 4), mind +0x20D30 clear), right after
  `UpdateDesires`, the desires' stored values and fn_004F0370 (the fizzle shown, or the next activity decided): when
  the help system's `WideScreen` (+0x45E8) and `WideScreenControl` (+0x45EC) are both set (0x4736A2..0x4736BC) and the
  mind's action +0xF60 is not 314 (0x4736C4), it builds `CreaturePlan(24 ObeyPlayer, 314, no belief, its belief about
  itself, no belief, 1.0)` (0x4736E9) and calls `ForceActivityAndForceAction(plan, 1, 1)` 0x4C44B0 (0x4736F9), then
  sets mind +0x1C14 = 1 (forced). No desire's list in `info.dat` holds row 314, and this is the only code that names it
  besides the action table (a scan of `.text` for the value 314: the others are `Villager::DanceAnimation`'s
  animation numbers, the source line given to `operator new` in `ForceActOnMapCoord` 0x4C66BE, sub_4CD800 0x4CDC16 and
  fn_00577BA0 0x577C30, an argument of sub_629130 0x629435 and a `rand() % 314` in sub_87E930). A script could still
  force the row by number (`CREATURE_DO_ACTION`); whether a land's script does is not read.
- **`WideScreenControl`** is the script task that holds the bars: `HelpSystem::SetWideScreen` 0x5C6AD0 (only when the
  state changes) stores the flag and, when on, its second argument; of its eight callers only `GScript::SetWideScreen`
  0x6F7BF0 (the scripts' `SET_WIDESCREEN`) passes a non-zero one, its task number. A film's bars
  (`PlayFullScreenMovie`, `FinishedVideo`), the interface's playback (`StartPlayBack`, sub_5DACCB), the reset and the
  dialogue's release pass 0. So only a script's cut scene forces the look; once the creature does anything else (the
  look over and something else decided), it is forced again the same turn, until the bars go.
- **`ConstructSubActionsForLookAtCameraInWideScreen`** 0x4A4F70 draws four `GameRand`s, always, in this order:
  `GameRand(2)` (0x4A4F89): on 0, with the creature's camera status (`GetNearestCameraInterfaceStatus`, read only
  then), sub-action 0x50 `PointAtPoint` at the status's camera (+0xB0) for 1.0 s, look `LookAtPosition`, face Amazed.
  Then the main sub-action 0x88 (argument 5 s in turns, unused), look `Creature::LookAtCamera`, face Curiosity.
  `GameRand(2)` (0x4A50F6): on 0, sub-action 0x0A (play an action) of clip 0x48 (72, the friendly wave), look
  `LookAtCamera`, face Amazed. `GameRand(2)` (0x4A5173): on 0, sub-action 0x89 (talk), no argument, look
  `LookAtCamera`, face Curiosity. `GameRand(3)` (0x4A51CB): on 0, sub-action 0x0A of clip 0x3F (63, confused), Amazed,
  then a talk 0x89, Curiosity. Last, the main sub-action 0x88 again. Every look but the point's is
  `LookAtCamera`. It never fails (returns 0).
- **Sub-action 0x88**, stage 1 0x502E00 (stages 0 and 2 empty): with no player, done; else it calls 0x502E70 with the
  camera, 100.0 and 5 and **drops its answer** (eax is overwritten at 0x502E5C), so the 100 m, the five seconds, the
  0.1 and the still hand end nothing; only the turn to the camera when more than a right angle off (and not within 0.1
  along the ground, where 0x502E70 returns before turning) is kept. It is done once the sub-action's turn count +0x1BDC
  is more than 20 (0x502E5E), the 21st call.
- **Sub-action 0x89 `Talk`**: stage 0 0x508BD0 `StartConcurrentAction(0xCD)` (animation 205, the gestures' talk, on
  top of the body; nothing when one already plays, 0x484369), done; stage 1 0x508BF0 waits while a concurrent action
  plays (the 3D creature's +0x5738), done once none does.

### GET_TARGET_OBJECT

`GScript::GetTargetObject` 0x6FA220 pops the object and looks it up (`GetScriptGameThing` 0x70D220): none, it says "no
object" and pushes **nothing** (0x6FA23E..0x6FA24C). Not a creature (`__RTDynamicCast` to `Creature`): "Only for
creatures" and 0. Otherwise it reads the mind (creature +0x164), the plan's belief at mind +0xF58 (the plan is the
`CreaturePlan` at mind +0xF48, its belief at +0x10), and that belief's object (+0x30). No belief: 0, with no message; a
belief with no object: "Richard: strange. No pointer for belief" and 0; else the object is pushed through
`AddScriptGameThing`. Nothing tests whether the plan is still valid (`CreaturePlan::IsValid` 0x4F12E0 is not called), and
the plan's end does not clear the pointer: only `CreaturePlan::SetToZero` 0x4F12B0 does, and none of its 21 callers
zeroes the plan at +0xF48 (the deleted-object clean-up zeroes the per-desire plans at +0x718 and the plan at +0xF78). So
the native gives the thing of the last plan after the plan stops, until a new plan takes its place [V]. A removed
thing's belief is not cleared there either (raffclar's vanilla bug 96).
#### What it holds, for a script (GET_OBJECT_HELD)

- The creature's form of GET_OBJECT_HELD (273, 1 argument, 1 result) is `GScript::GetObjectHeldByObject` 0x6F5310.
  It POPs the object for GetScriptGameThing 0x70D220; none → ScriptErrorMessage "No creature" (0xC00AFC) and an
  object 0 pushed. Then `__RTDynamicCast` to `Creature` (0x6F535B); not one → "Not creature" (0xC0D088) and 0. A
  creature: its physical (+0x160) +0x28, the thing in its hand (the same field the steal agendas test for an empty
  hand); 0 → 0 pushed; else `AddScriptGameThing(obj, 0)` 0x70D0F0 (a found thing, not a made one) and its id pushed.
- The hand's form (199) is on [the hand's picking page](../hand/picking.md).

### Sources

- The v1.42 `runblack.exe` (1.20 symbols from bw1-decomp), xref; the frozen `Scripts\info.dat` for the action names
  and the species' look values; Land 1's script (`CreatureDevSeeHome`, `LandControl1`).

## openblack

Status: **done** for SET_POSITION's place, SET_FOCUS and RELEASE_FROM_SCRIPT, the script play and PLAYED,
MOVE_GAME_THING, CREATURE_DO_ACTION with LookForever, LookButDontApproach and the scripted sit, and the looks at the
camera, and PointAtObject; **not ported**: the look about when tired (a port row), the sit's clear-spot search for a creature that sits by itself, the original's
per-turn bookkeeping outside the mind block (see [Pending](#pending)). The port decisions are on
[Port decisions: creature](../project/port-decisions-creature.md).

- **Placing**: `CreatureLocomotionSystemInterface::Place` (raffclar's name) gives up the walk and the route and puts the
  creature on the ground at the point, facing as it did, with nothing to blend from (the turn's pose and the drawn
  position both moved). The script's side calls it.
- **Control**: `CreatureMindSystem::FocusByScript` (from SET_FOCUS) takes the creature under control when it is not
  yet, keeps the point, stops what it was doing and plans {ObeyPlayer, FollowPlayer} with one `TurnToFacePoint` step,
  carried out as that plan (`CarryOutForScript`, as the play and the move), so it is completed when the turn is over,
  as the original's plan is when its last sub-action ends; `ReleasedByScript` (from RELEASE_FROM_SCRIPT, after the
  control is cleared) sets the focus off and stops it again; each stop starts the pass over its desires again, as the
  goal list's refill does. While it is controlled its desires are not updated and it plans and mimics nothing; its head
  looks at the focus. The turn is the locomotion's `TurnToFace`, with the same 0.1 and pi/8 rule as TurnToFacePos, so
  `Movement::FacePoint` is still carried out by nothing.
- **The focus on a thing**: SET_FOCUS_ON_OBJECT on a creature calls `CreatureMindSystem::FocusOnThingByScript`, which
  keeps the thing (`CreatureMindState::scriptFocusThing`, the original's kind 0) in place of a point, and carries out
  `creature_mind::FaceThingForScript`: one `TurnToFaceObject` step held two seconds, the original's sub-action 6 with
  2.0 (from raffclar's PR #347). Its head looks every turn at where the thing stands (its Transform, which is the map
  point and its height over the ground), and the focus lets go once the thing is gone, as the original's deletion of
  the belief does. On any other object the thing's position is faced as SET_FOCUS faces a point (the same snap, now
  `TurnObjectToFace` in `CHLApi.cpp`); a marker or a group does nothing. RELEASE_OBJECT_FOCUS calls
  `ReleaseFocusByScript`, which only takes the focus off: the creature stays under the script's control. A point or
  a thing focus replaces the other, as the original's one flag with its kind does. The belief about the thing
  (`AddBeliefAboutObject` 0x4D7BD0, called by `Creature::SetFocus` 0x4F6850 at 0x4F6898, after
  `PrepareCreatureForScriptedAction(1)` and before `AddSubAction(6, thing, 2.0)`) is `ComeAcross`, as for the
  reactions' callers. ✅ v1.42 (2026-10-21): `GScript::SetFocusOnObject` 0x6F91F0 pops the thing looked at, then the
  looker; either missing logs "Thing not found!" (0x6F9234) and returns; a creature (vt+0x34) goes to
  `Creature::SetFocus` (mind +0x1D3F4 = 1, +0x1D3F8 = 0, +0x1D3FC = the thing); any other `Object` (the cast at
  0x6F9272) gets vt+0x510 with the thing's map point (×1/6553.6) at the land's altitude plus its height, which ours
  reads as `GET_POSITION` does, from its Transform. `GScript::ReleaseObjectFocus` 0x6F5700: none, "Thing not found!";
  not a creature, "Thing not creature!" (0xC0CF98) and nothing; a creature, mind +0x1D3F4 = 0 and nothing else.
- **The play**: SET_SCRIPT_ULONG keeps the request (`CreatureMindSystem::SetScriptPlay`); SET_SCRIPT_STATE, for a
  creature that is not drowning and for a container's creature members, calls `PlayForScript`:
  `creature_script_play::Agenda` makes an Action step per play, or for a static action a Static step of clip − 1,
  clip and clip + 1 held 10 seconds, then a `WaitUntilFree` step (it starts even while the body is busy, looks at the
  body from its first turn and is done on the turn after it first sees it free, pulling no face, so with no draw); it
  is carried out after `PrepareForScript` as the Told plan {ObeyPlayer, FollowPlayer}, which is completed when the
  agenda ends. `PrepareForScript` lets go of a miracle the creature holds (`MagicSystemInterface::ReleaseCreatureCast`,
  the port of `DestroySpell`) and ends a sit or a casting pose there and then (`creature_layers::EndLoop` on the body).
  PLAYED calls `HasPlayed`: `creature_script_play::Played` (the agenda is over, or the creature only idles), and when it
  says so with steps left (only while idling) the rest of the agenda is cut and counts as given up. openblack's script
  containers take only villagers and animals (`ecs::script_containers` IsLiving), so a creature joins no flock or dance
  and none of the other callers of the play reaches one; `SetLivingScriptState`'s leaving a creature alone is never
  reached. The static action 0 is the native's NotImplemented line.
- **The move**: MOVE_GAME_THING, for a creature `map_cells::IsObjectInMap` says is in the map, calls
  `CreatureMindSystem::MoveForScript`: `creature_mind::MoveForScript` (two `ToPoint` legs, each with the point as its
  gaze) carried out after `PrepareForScript` as the {ObeyPlayer, FollowPlayer} plan; `ecs::object::GetHeight` (the
  creature's drawn size × 15) is both heights. Its walk is stopped first, unlike the original's: openblack's walk step
  only starts once the body is still, so the new walk starts at once. A step's gaze is the head's look while the step
  lasts (`Commands::gaze`, in place of looking about): a point's ground, a thing's top or its ground (`Gaze::bottom`),
  the camera, or the ground at its feet (`Gaze::feet`, `creature_mind::FeetLookPoint`: LookAtFeet's step of 4 m along
  the game angle of `BodyYaw`, each axis through `ToFixedGUtils`; always, walking or not; no agenda of ours uses it yet,
  as none of LookAtFeet's four agendas is ported); none while walking within 10 of it, nor a thing it holds (for the top). When the gaze gives nothing
  that turn, or names nothing (the steps with no look of their own: the play's steps, the sit's turn down the slope and
  its sit), the head looks about, as `LookAround`.
- ✅ **In its hand** (IN_CREATURE_HAND 076, `src/CHLApi.cpp` `InCreatureHand`): the original's pops, lines and
  falses (`script::property_rules::InCreatureHandNative`); a creature's answer is true for what
  `CreatureObjectActionSystemInterface::GetHeld` says it holds, or what `GetEating` says it eats: the thing of its
  `Kind::Eat` object action, which is ours for the eating sub-action, held at the start and then the meal until the
  action ends. The crash after "Not a creature" is guarded, always on: the line and the false only. Tests:
  `test/test_script_properties.cpp` (`InCreatureHand*`). Its readout is the creature debug window's "Eating" line,
  under Hands, beside "Holding".
- **The forced action**: CREATURE_DO_ACTION with the original's pops and messages, the control taken first, calls
  `CreatureMindSystem::ScriptDoAction`: the mind set up if the creature has not thought yet, `PrepareForScript`, then
  `ForceActivity` {ObeyPlayer, the row's action on the target, with the other thing} at the most a plan can be
  pressing; an action openblack cannot carry out is the native's NotImplemented line. openblack has no candidate help
  scripts, so [0xC64148] has nothing to stop. The executors `LookForever` and `LookButDontApproach` build the agendas
  above (the look's seconds drawn with `GameFloatRand(1.2)` + 1.1 and turned into whole turns as the agenda is made),
  and `SitDown`, for a creature under a script's control told to sit at a thing, `SitDownAt`: the walk within its
  `Get2DRadius`, its head on the walk's look (`creature_mind::LookWhileGoing` with the species'
  `GCreatureInfo::lookSweep*` and `lookAtPointWithin`, its state `CreatureMindState::walkLook` set from them the first
  time; the scan of what interests it is made every turn of that walk), the turn to the thing with its head on it,
  `FaceDownSlope` (`creature_mind::DownSlope` on the land's heights, the turn by the locomotion's `TurnToFace`, the step
  done once the body is free: not moving and playing no action) and the sit for `GameFloatRand(5)` + 10 seconds in
  whole turns, pulling no face.
- **The camera**: the executors `LookAtCamera` (`creature_mind::LookAtCamera`, the idle mind's LookAtCamera step with
  the Curiosity face and its head on the camera) and `PointAtCamera` (`creature_mind::PointAtCamera`: with a player the
  turn to face the camera, its head on it, no face; then the point at the camera for a second, amazed, its head on the
  camera's point). The step, each turn after its start: done without a player (`Senses::hasPlayer`, the creature's
  owner not neutral) or without a camera; done within 0.1 of the camera along the ground; turned to the camera when
  more than a right angle off; done after five whole seconds, beyond 100 m through the air (`Senses::cameraDistance`)
  or with the player's hand still more than five seconds of turns (`HandSystemInterface::GetStillTurns`, the hand's
  count kept by `HandSystem::UpdateTurnMovement`). A creature without a player does not look at the camera
  (`GazePoint`). PointAtCamera with no camera can be chosen and its agenda then can't be made, so the plan fails;
  LookAtCamera never fails.
- **The cut scene's look**: `LookAtCameraInWideScreen`, executor `Build::LookAtCameraInCutScene`
  (`creature_mind::LookAtCameraInCutScene`, raffclar's): the four chances drawn in the game's order, the point (when
  there is a camera) as PointAtCamera's, the two watches as LookAtCamera steps with `Step::turns` = 20
  (`k_CutSceneWatchTurns`): its first turn counted, done without a player, turned to the camera when more than a right
  angle off unless within 0.1 along the ground, and done once more than 20 turns have gone by, whatever the camera's
  distance or the hand do. The wave and the confused look are action steps of clips 72 and 63, the talk an `Overlay`
  step of animation 205 (`creature_layers::animations::k_Talk`) on the gesture layer, done once nothing plays there.
  The trigger is in `CreatureMindSystem::ProcessTurn`, after the turn's fizzle, feedback and planning: a creature not
  under a script's control whose current action (`mind_detail::CurrentAction`) is not row 314 is forced it
  (`ForceActivity`, ObeyPlayer, done to itself, score 1.0) while the help system's wide screen is on with an owner
  (`creature_mind::StartsLookAtCameraInCutScene`).
- ✅ **What it holds** (GET_OBJECT_HELD 273, `src/CHLApi.cpp` `GetObjectHeld273`): the exe's two messages and 0s; a
  creature's answer is `CreatureObjectActionSystemInterface::GetHeld` (the `CreatureHeldObject` of the creature's
  hand), registered with the scripts as a found thing, or 0. Tests: `test/test_script_query_natives.cpp`
  (`GetObjectHeldOfACreature*`). Its readout is the creature debug window's "Holding" line, under Hands.

**GET_TARGET_OBJECT** (`CHLApi.cpp`, raffclar's PR #508, unverified against v1.42): the plan's thing, or the last
plan's once the plan stopped (`creature_planner::TargetObject`; every plan stop goes through
`creature_planner::StopPlan`, which keeps it). It gives 0 for a thing that is gone, where the original would read the
removed belief (guarded; see [vanilla bugs](../project/progress/vanilla_bugs/creature.md)). The creature window's planner
shows it.

### Test hooks

None of its own; the scenarios drive the commands through the land scripts.

## Pending

- **A script putting a creature somewhere** ([A script putting a creature
  somewhere](#a-script-putting-a-creature-somewhere)): `StateSet(0)` and the zeroed look target, which openblack's body
  (an action's animation, the head's look) does not take yet, are queued as a fix; the map move's looking and vision,
  and the turn-start map coords, which openblack moves as for any teleport, are not compared.
- **A creature under a script's control** ([A creature under a script's control](#a-creature-under-a-scripts-control)):
  the steps outside the mind block that follow other rules than openblack's are queued as a fix: contexts age by a turn
  stamp in whole seconds (openblack sums 0.1 s floats); the turns since an action was done grow only as a plan stops,
  by the turns it ran (openblack counts every action every turn);
  "seconds alone" is ported as two counters with their resets (`turnsNotLookedAt`, `turnsSinceMadeToAct`, in whole
  seconds, [the attention its god pays it](mind.md#the-attention-its-god-pays-it)); the feedback time is a stamp
  nothing reads.
  Not compared: the zeroing of the stopped plan's counts (+0x1CEA0); the locomotion's `TurnToFace` still snaps an angle
  within π/12 at once (the turn's own shape is ported); the focus's look while the mind leaves the body alone (paused,
  leashed, fighting).
- **The look about when tired** ([The look each turn](#the-look-each-turn), fn_004D03B0 0x4D03B0): not ported (a port
  row).
- **Unread**: mind +0x1A9F4 (faded by `fn_004C4D20`) and desires +0x704 (× 0.99 in `UpdateSources`)
  are unnamed; a reader of the feedback stamp mind +0x18C60 through the `CreatureAttitudeToPlayer` pointer (+0x20) may
  exist; [0xD00DEC] (the random start of the previous-actions counts) reads 1 after the `GGlobal` constructor (the oracle run
  of 0x54A6E0, see [the gate](mimicry.md#gplayermakecreatureempathisewithplayertowndesire)); its other writers are not
  checked.
- **The play and PLAYED**:
  - The static action 0 would start with clip −1 (not settled): `StartStaticAction` 0x4843A0 asks
    `Morphable::GetAnim(clip − 1, 1)` 0x619650 first (0x4843B9), which hands `GetSetAnim` 0x619690 the body's set
    (+0x98; 6 and 7 read as 1). That reads its cache at this + 0x1D4 + 4 × (anim + 696 × set), so −1 reads the word
    just before the set's own (the last entry of the set before, or +0x1D0 for set 0): a value there is taken as the
    clip; 0 sends it to the loader's switch on the set (0x6196D5, sets 0 to 5) with −1, whose result was not followed.
    Whether the start is refused (and the sub-action tried for ever) or something is played depends on the body's
    memory, which the code alone does not settle; the oracle cannot run it without a live 3D creature. openblack does
    not play it.
  - The static loop is held 10 seconds of summed turn times in openblack, the original a whole number of turns.
  - The play's steps, having no look of their own, look about at every stage in the original
    ([the look each turn](#the-look-each-turn)) and in openblack (a gaze that names nothing). Steps of openblack's own
    agendas with no gaze still look about only in a Wait and in a Static step's loop, and look at nothing otherwise,
    where the game's sub-actions with no look callback look about; and openblack's look about falls back on the point
    ahead, the game's on `GetDestPos`. Both are for every creature, so not changed here.
  - `StartStaticAction`'s `GameRand(2)` for the mirrored variant is not drawn by openblack's static steps (as for
    sitting).
  - What `PrepareCreatureForScriptedAction` does that `PrepareForScript` does not: it ends a dance and a fight at once.
    RELEASE_FROM_SCRIPT goes through `StopWhatIAmDoing` too (its `DestroySpell` and its end of a sit or pose at once);
    openblack's `ReleasedByScript` does neither yet.
  - What a creature in a flock or a dance does besides the play (the flock's and the dance's own moves) is not read:
    open with the containers.
- **The move**:
  - The first leg's sixth is reckoned in single precision; the original takes the square root in the x87's precision
    and rounds once, as it stores the product.
  - The look at a thing measures the 10 from the thing itself, the original from the belief's remembered position;
    with the thing gone it looks at that position, openblack looks about.
  - The walk the original's move does not stop: openblack stops it so that the new leg starts on the same turn;
    whether the original's `MoveToPos` stage 0 takes over the walk under way without a still turn is not traced.
- **The forced actions**:
  - `PointAtObject` (row 163, no row or object test): the executor `PointAtObject` builds `creature_mind::PointAtThing`,
    a point at the thing for 5 s (`ObjectOrder::Kind::PointAt`, carried out by `CreatureObjectActionSystem::PointAtFor`,
    which aims again at the thing every turn, as the sub-action's step does), amazed, its head on the thing itself
    (`Gaze::itself`): `LookAtFlyingObject` 0x4D17E0 looks at the plan's thing where it is unless the creature carries it
    (the dynamic cast and the compare with the carried object at 0x4D1826..0x4D1839), as the watch of a flying thing.
    No draw.
  - A creature that sits down by itself sits on the spot in openblack; the original first looks for a clear spot four
    times its radius about the thing and walks there (and fails without one). A change for every sitting creature, so
    a commit of its own.
  - The sit by itself draws `random(5)` whole seconds in openblack; the original's `GameFloatRand(5.0)`.
  - The walk's look: with nothing interesting, its second look takes the last moving thing met within two seconds
    (+0x1D3C8, kept as the scan meets each moving thing); openblack keeps it since 2026-10-21
    ([mind.md](mind.md#the-last-moving-thing-seen)). The scan of what interests it runs every turn in the original, in openblack only while looking about or on such a walk.
    The sweep's point is made in single precision (the original adds in the x87's). Only the scripted sit's walk takes
    this look in openblack; the original's other agendas with it (sleeping, dancing, the hills, the sea) have no gaze.
  - The look's and the sit's seconds are drawn and turned into turns in single precision (the original adds in the
    x87's precision before `ftol`); the Wait and Static steps then count the turns' seconds.
  - `FaceDownSlope`'s points are made in the map's whole units in the original, in metres in openblack.
  - `FaceDownSlope`'s stage 0 stops a walk under way and tries the turn again each turn until it starts, the lowest
    point looked for again each time; openblack's step only starts once the body is free, and turns once.
- **The looks at the camera**:
  - The camera and the hand are the local player's for every creature with a player: a rival's creature has the
    original's nearest status of its own player, which is not read for a computer player.
  - Without a player the original keeps the empty turn sub-action 0x1F, done as it starts; openblack leaves it out and
    starts the point at once. Whether that costs the original a turn is not read.
  - The turn to the camera (0x1F) uses the locomotion's own `TurnToFace`; its 0.1 and 0.2 rad ends and its wait while
    the body acts are not compared.
  - The cut scene's force runs in openblack only past the mind's own early ends of its turn (paused, on the leash,
    fighting, knocked out or fainting); the original's gate is the thinking block's, which does not test the leash or
    a faint.
  - LookAtCamera's own step (0x42) does no check on its start turn, which the original's count +0x1BDC already counts:
    its ends may come a turn later than the original's. Not changed here.
  - The copied action of the creature's mimicry builds its agenda with no camera, so a camera action copied that way
    fails.
- **The lesson actions a script forces**: on its way, each its own measured commit: CREATURE_DO_ACTION of PointAtObject
  (five seconds pointing, aimed again every turn, amazed, its head on the thing itself), SleepAtHome, SleepByObject,
  WaveAtObject, PutDown, DeadForever and HoldObject get their executors, and BeFrightenedOnTheSpot its own agenda
  ([plans and needs](plans-and-needs.md#pending)); today each of the first seven is the native's NotImplemented line.
  The sit down a script forces follows in a series of its own.
