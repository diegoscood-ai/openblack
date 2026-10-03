# Villagers: data, state machine and speed

The runblack.exe (W120) villager as ported by the "aldeanos" session (branch `local/aldeanos`, milestone V1: the core of
the state machine). The clips (which clip per state, transitions, size) remain in [animation.md](animation.md);
here go the villager's data, the turn, the state changes and the speed rules. Research with
addresses: `C:\Users\diewgarc\dev\documentacion\aldeanos\` (`V1_spec.md`, `PLAN.md`, `core.md`, `team_apis.md`, dumps in
`core\` and `v1\`).

In openblack:
- `ECS/Villager/VillagerCore.{h,cpp}`: the turn (ProcessState, CheckEveryTime), the state changes (SetTopState,
  SetCurrentAndDestinationState, SetState, exits and entries), creation (constructor), CREATED (85), the pause (239),
  SetupMoveToWithHug, the provisional death;
- `ECS/Villager/VillagerStateInfo.h`: names of the fields of the info.dat state table;
- `ECS/Villager/VillagerStateTable.h`: the row of the function table (state, entry, exit, validate...);
- `ECS/Villager/VillagerOriginalFns.h`: the addresses of the original's functions for each row (for the warnings);
- `ECS/Villager/VillagerDebugHooks.cpp`: the trace and the test hooks;
- `ECS/Systems/Implementations/LivingActionSystem.cpp`: the table (`k_VillagerStateTable`) and the turn loop;
- `ECS/Components/Villager.h` (the fields), `LivingAction.h` (the three states and the counters), `Town.h`
  (`TownDesire`), `ECS/Villager/VillagerAge.h` (age), `ECS/Archetypes/VillagerArchetype.cpp` (creation),
  `ECS/VillagerSpeed.*` (speed), `ECS/VillagerAnimations.*` (clips of the state changes);
- `test/test_villager_core.cpp`: 20 cases (see [Tests](#tests));
- V4: `ECS/Villager/VillagerHome.*`, `VillagerFood.*`, `VillagerAge.*`, `VillagerResources.*`, `ECS/Town/AbodeVillagers.*`,
  `ECS/Town/TownVillagers.*`; `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp` (V4,
  see [Home, food, sleep, homeless and age (V4)](#home-food-sleep-homeless-and-age-v4)).

## Fields (Villager.h)

| offset | field | notes |
|---|---|---|
| Object +0x48 | `life` | 0..1 (`ecs::life`) |
| Living +0xA0 | `birthTurn` | Living::SetAge 0x5ED2C0: `turno − edad·1500`; the age is Living::GetAge 0x5ECAF0 = `(turno − birthTurn) / 1500` unsigned (`villager::GetAge`). 1500 = GGameInfo +0xC (0xD01A04) |
| +0xE0 | `flags` | see [Flags](#flags) |
| +0xE8 | `food` | what it has in its belly; the constructor leaves it in [0.5; 1.1) |
| +0xEC | `lastCheckTurn` | turn of the last periodic check (reset by CheckHungry 0x75BEDF) |
| +0xF0 | `foodSpeedUp` | IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430 |
| +0xF2 | `discipleType` | g_DiscipleInfos 0x99A1F8 |
| +0xF4 / +0xF6 | `resourceHeld` | food and wood it carries |
| +0xF8 | `pregnancy` | turns remaining (0 = no) |
| +0x100 | `mother` | |
| +0x118 | `targetThing` | TargetThing (bw1-decomp `Villager.h`) |
| +0x11C | — | union Football* / TradeTown / WanderArea (bw1-decomp `Villager.h`). V1 does not add it: no V1 code reads it |
| +0x128 / +0x12C | `abode` / `town` | |

`lifeStage` is a mirror of `flags & 0x8` (read by DetailMeshes, drawing and sounds) and `sex` is a mirror of
GVillagerInfo +0x1F8 (`IsWoman` 0x752620 and SetSpeed read the info one). `task` is not from the original.

Missing from the original (they arrive with their milestone): `next` +0xE4 (home list, V4), `building_site` +0xFC (V6),
`LastPlayerToInteract` +0x104 (V12), +0x108 / +0x10C / +0x110, `fire_effect` +0x114 (Milagros), +0x124.

### Flags

Villager +0xE0 (direct writers: `documentacion\aldeanos\v1\scanE0.py`): 0x1 after a tap on its home
(SetupAfterTapOnAbode), 0x2 at the worship site (AddVillagerToWorshipSite / RemoveVillagerFromWorshipSite), 0x4 inside
the home (ArriveHome), 0x8 child (SetAge; CheckChildGrownUp clears it), 0x10 on the way to worship or to a fire, 0x20 in
the hand (InterfaceSetInMagicHand; EndPhysics / Landed clear it), 0x80 football / script, 0x200 / 0x400 disciple /
follower, 0x800 / 0x1000 entry / exit clip (in openblack `SkeletalAnimation::transitionFlags`), 0x2000 going to
sleep (CheckWhenGoingToBed). In V1 only 0x8 is written (SetAge); 0x2, 0x4 and 0x200 are read. The worship 0x2 is
written by Milagros in `WorshipVillager::atSite` (AddVillagerToWorshipSite 0x76C3F0 / RemoveVillagerFromWorshipSite
0x76C440, team_apis.md (C)); CheckEveryTime (0x7504DC) reads `flags & 2` **or** `atSite` until it moves to `flags`. 0x4 and
0x200 are set by nobody today.

## info.dat state table (VillagerStateInfo.h)

`GVillagerStateTableInfo::Infos` 0xDB9E68, 0x114 bytes per state; the disassembly reads each field at the file
offset + 0x10. The fields of `InfoConstants.h` keep their `field0x..` names (other sessions use them);
`state_info::` gives them names: `Clip` (0x00), `ServedDesire` / `ServedDesireAmount` (0x04 / 0x08, AdjustTownModifier),
`IsFinal` (0x0C), `NotStoredAsPrevious` (0x10), `IsMoving` (0x14), `ResumeState` (0x20), `SpeedGroup` (0x24),
`CanPauseForASecond` (0xC4), `NoGoHomeWhenHurt` (0xD8), `DoPeriodicChecks` (0xE4), `GoHomeWhenHurt` (0xE8),
`NoOutOfClip` (0xF0), `LifeDrainPerTurn` (0xF8)... (full list in the file).

## The function table (VillagerStateTable.h)

The original's is at 0xD09198, 0x90 bytes per state: +0x00 state, +0x10 entry, +0x20 exit, +0x30/+0x40
save/load, +0x50, +0x60 (clip function), +0x70 (transition clip), +0x80 validate. In openblack:
- the **exit** returns **1 = can exit** and receives the next state; the **entry** receives (previous final,
  new state) and returns **1** (accepted), **0x23** (accepted; the function set the states) or another value (rejected);
- an empty slot is "no function": it counts as 1, as in the original;
- an unported state (`k_TodoEntry`) does nothing (returns 0) and warns **once per state** with the original's
  address; if the original has an entry, exit or validate in a row that openblack leaves empty, it warns once
  (`TODO: Unimplemented entry function of ... (0x...): taken as 1`);
- the Milagros worship exits (58, 59, 60, 213) keep their old convention (false = can exit) and the row
  adapts them;
- the fire ones are in their rows, as `_$E32` fills them (see [Fire in the table](#fire-in-the-table));
- 16 DROWNING: EnterDrowning 0x767410 and ExitDrowning 0x767420 only accept (`mov eax, 1; ret`); the state function
  (Villager::Drowning 0x76A780) arrives with water.

### Fire in the table

The static initialiser `_$E32` (0x5AA2D9..0x5AA767) sets +0x10 / +0x20 of each row:

| row | state | entry (+0x10) | exit (+0x20) | where in `_$E32` |
|---|---|---|---|---|
| 215 | REACT_TO_FIRE | — | ExitReaction 0x7527A0 (the thunk 0x5B0100 = `jmp [vt +0x910]`) | — |
| 216 | PUT_OUT_FIRE_BY_BEATING | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA2D9 / 0x5AA2EC |
| 217 | PUT_OUT_FIRE_WITH_WATER | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA421 / 0x5AA434 |
| 218 | GET_WATER_TO_PUT_OUT_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA4BE / 0x5AA4CB |
| 219 | ON_FIRE | EnterOnFire 0x75AF30 | ExitOnFire 0x75AF80 | 0x5AA611 / 0x5AA642 |
| 220 | MOVE_AROUND_FIRE | EnterPutOutFire 0x75ADC0 | ExitPutOutFire 0x75AE80 | 0x5AA75D / 0x5AA767 |

What they return (read in the disassembly):
- **EnterPutOutFire(final, s)**: 1 if `IsStateEntryFunctionSameAs(final, s)` 0x7524D0 (both rows have the same
  entry: from 216/217/218/220 to another of them); otherwise, with a live fire (+0x114) (fn_0075AD90; if it no longer
  exists, +0x114 = 0), its vt +0x2C and a reaction (+0x94) not shut down (Reaction +0x34, set by ShutDown 0x6E4723):
  **0** if it is already in the root's firemen list (0x75AE75), and if not it adds it (AddFireman 0x7309A0) and gives
  **1**. In any other case it gives **0** and, if the previous final is reactive (table +0xB8, 0xDB9F30), StopReacting
  (vt +0x998). A 0 is 0x2F and Villager::SetTopState enters 163.
- **ExitPutOutFire(s)**: always **1** (0x75AECB and 0x75AF20). If the exit is not "the same" (vt +0x96C,
  0x752530: another row with ExitPutOutFire, or a non-final state) it leaves the firemen list and the town's
  on-the-way-to-worship list (0x73E360); if it was not in the list, only +0x114 = 0 and it returns without ExitReaction;
  otherwise, ExitReaction 0x7527A0 (ends the reaction unless `s` is reactive).
- **EnterOnFire(final, s)**: **1** without fire or with its vt +0x2C at 0; **0** if it is already in the list
  (0x75AF78); otherwise it adds it and **1**.
- **ExitOnFire(s)**: always **1**; it leaves the list if it is there and **+0x114 = 0 always** (it does not look at `s`).

The state changes of `VillagerFire.cpp` and `VillagerTeleport.cpp` are now those of the core: `villager::SetTopState`
(with the pause roll, the exit of TOP and of the final and the entry, once each, and the codes 1 / 0x2E / 0x2F;
both go through `villager_reactions::SetTopState`, which also ends openblack's walk),
`villager::SetState(2, s)` for the saved state (vt +0x938: skips those of table +0x10 and adjusts the town) and
`villager::SetupMoveToWithHug`. There is no longer a local SetTopState, nor `CallEntry` / `CallExit`, nor
`villager_fire::CallFinalStateExit`; the worship exits (58, 59, 60, 213) run through their rows. In addition:
- SetupMoveAroundFire 0x75A770 only stores destination and next state if SetTopState(220) gives 1 (0x75A783).
- PopFromPrevious 0x751E50 (`villager_reactions::PopFromPrevious`, a single one for fire and teleport):
  SetTopState of the resume state of what was saved (Infos +0x30 0xDB9E98 = file 0x20); if it gives 0x2E, TOP = 163
  raw (LivingAction::SetState 0x5ECC90) and afterwards PREVIOUS = 0 raw. **With nothing saved** it is row 0, whose
  resume state is **0** in info.dat: SetTopState(0) = INVALID_STATE, as in the original (Living::InvalidState 0x5EC1D0
  returns 0 each turn). Previously 163 was invented.
- MoveAroundFire 0x75A7E0 on arriving: PopFromPrevious (0x75A815) and PREVIOUS = 163 **raw** (0x75A81A..0x75A827,
  LivingAction::SetState 0x5ECC90 with ecx = +0x8C: without the table +0x10 skip nor the town), not Villager::SetState.
- ResetStateAfterReacting 0x751E10 (vt +0x9A0): PopFromPrevious and, if the final state is reactive (file 0xB8),
  SetTopState(163). StopReactingAndSetState 0x5F11C0 (vt +0x99C): that and then StopReacting if it is still reacting. It
  is used by ReactToFire (0x765A48, already a fireman) and TeleportReaction (0x76642F, after the jump; before it was the
  other way round: first StopReacting and then PopFromPrevious).
- **(approximate)** The exit of MOVE_TO_POS (ExitMoveToPos 0x5EDDA0) is not ported: these SetTopState remove
  openblack's walk marks (except with 0x2E), as the local SetTopState did.

Differences from before (checked in a game, see below): the entries and exits also run when the final state
does not change, as in the original. When replanning or arriving (SetupMoveToWithHug / SetTopStateToFinal) a fireman in
220 goes through ExitPutOutFire(220) and EnterPutOutFire(220, 220), which change nothing; a villager in 219 goes through
ExitOnFire, which **clears +0x114**: after its first walk it stops fleeing from other fires and only stays in 219 while it
is itself burning (the same in the original: OnFire 0x75B1E0 calls SetupMoveToWithHug 0x5F2890 at 0x75B39E).

**The pause 239 does not come out in fire nor teleport.** CanPauseForASecond 0x752120 looks at the row of the
**destination** state (Infos +0xD4 in memory, 0xDB9F3C = file 0xC4), which is 0 in 163, 201, 202 and 215..220: those
SetTopState do not roll the pause. It can only come out when PopFromPrevious returns to a state that pauses (a job with
0xC4 = 1) or in 248 (worship).

### Exits of the reactions (ExitReaction, ExitReactToTeleport)

- **ExitReaction 0x7527A0** (vt +0x910; the rows store the thunk 0x5B0100 = `jmp [vt +0x910]`, 45 rows in `_$E32`):
  CircleHugInfo::Reset(+0x70) 0x60A9F0 (in openblack it removes `WallHugObjectReference`, **approximate**), and if `s` is
  not reactive (IsReactiveState inline, Infos +0xC8 0xDB9F30 = file 0xB8) StopReacting (vt +0x998). Returns 1.
  In the table: 215 and the unported rows 6-9, 203 and 214 (`TodoWithExitReaction`); the other rows with that thunk
  (12, 19-22, 25, 26, 30, 140-168, 194-196, 205-208, 227, 231, 235-237) remain empty because nobody enters them.
  ExitPutOutFire calls it at the end (0x75AF19).
- **Villager::StopReacting 0x7637D0** → **Living::StopReacting 0x5F1140**: with TOP 203 and dancing, RemoveFromDance(1)
  (unported: 203 has no state function); with a reaction (+0x94): out of the reaction's list,
  `fn_005F0FE0(tipo)` = the type's record receives the turn (`reactions::RefreshRecord`), +0x94 = 0; +0xBC = 0 always.
  In openblack `villager_reactions::StopReacting` calls `villager_fire::StopReacting` (reaction 0, null object,
  RefreshRecord of REACT_TO_FIRE) and `villager_teleport::StopReacting` (clears its state, RefreshRecord of
  REACT_TO_TELEPORT). EnterPutOutFire (0x75AE55) also uses it.
- **ExitReactToTeleport 0x766390** (exit of 201, 202 and 251): if not `IsStateExitFunctionSameAs(s)` (vt +0x96C
  0x752530), it leaves its town's on-the-way-to-worship list (GetTown vt +0x48 → 0x73E360) and +0xE0 &= ~0x10 (also
  without a town); then ExitReaction(s) and its result.
- **IsStateExitFunctionSameAs 0x752530** (`villager::IsStateExitFunctionSameAs`): the exit of the GetFinalState row and
  that of `s` are the same (the whole 16-byte pointers; here the addresses of `VillagerOriginalFns.h`, two empty ones
  count as equal) → 1; otherwise, `s` final (0xDB9E84) → 0, otherwise 1. ExitPutOutFire (0x75AE95) now also uses it,
  instead of the hand-written list.

## Creation (Villager::Create 0x74FBE0 and the constructor 0x74F950)

`Villager::Create` first rolls `GameRand(10) <= 1` (0x74FBF0) to try a SpecialVillager (TODO V14: there are none, so
a normal one always comes out). Afterwards, the constructor (`villager::Construct`):
1. Living::Living (life = info.life) and SetToZero 0x74FB20 (all new fields to 0).
2. SetAge 0x7528C0: child if `edad < grownUpAge` (`flags |= 8`), otherwise `edad = max(edad, 18)` and `flags &= ~8`;
   meshes and scale (consumes FloatRand); `birthTurn = turno − edad·1500`.
3. `foodSpeedUp = 0` (0x74FA02); woman (info +0x1F8 == 1): `pregnancy = 0` (0x74FA08).
4. food (0x74FA18..0x74FA67): `min(1, GameFloatRand(0,6) + hungryForFood)` is a macro that evaluates twice: if the
   first roll gives < 1 it rolls again and stores the second one without clamping.
5. lastCheckTurn (0x74FA6D..0x74FACD): `turno − (GameRand(processChecksEvery) < turno ? GameRand(...) : turno)`. The
   second roll can exceed the turn: on turn 6 with a roll of 7 it ends up 0xFFFFFFFF (literally; the unsigned
   subtraction of GetGameTurnsSinceLastChecked leaves it correct).
6. State counter Object +0x58 (`turnsUntilStateChange`) = GameRand(500) + 1 (0x74FAB0).
7. **Water rule** (0x74FADC..0x74FAF5): `SetState(TOP, IsWater(pos) ? 16 DROWNING : 85 CREATED)`, the exact
   SetState, without entry, clips or speed, and without calling anything of the water. `IsWater` = MapCoords::IsWater
   0x6035B0 (`pot_resource::IsWater`; with the water merge, `sea_cells::IsWater`).
8. `++g_game+0x205A54` and SetSkeleton 0x7562C0: TODO(V12).

Afterwards (as before V1) the archetype sets the home and the town (in the original, CallVirtualFunctionsForCreation and
AddVillagerToAbode: V4) and the speed of 85 **(approximate)**.

In Land2 the script creates three villagers in open sea (1578, 2220) on turn 6: they are born in 16 DROWNING, as in the
original; until the water state function arrives they stay still in the water.

## The turn (Villager::ProcessState 0x74FF70)

`LivingActionSystem::Update`: first the test hooks, then for each villager (registry order)
`ProcessReaction` 0x5F1270 (no-op: TODO Milagros M-5) and `ProcessState`; at the end `FlushDeaths`.
**(approximate)** the original keeps villagers and animals in a single list (g_game +0x205BBC) and the path step
(Living::MoveToPos 0x5EC270) goes inside the state function; openblack moves everyone first (PathfindingSystem) and
processes the animals afterwards.

ProcessState:
1. `++turnsSinceStateChange` (+0x90) and ProcessFoodSpeedup 0x753430 (`foodSpeedUp != 0 && turno % 10 == 0` → −1).
2. validate (+0x80) of the TOP (+0x8C, 0x74FF91) and of the raw FINAL (+0x8D, 0x74FFD9), only if the row has one; the
   result is not used. The reaction rows (201, 202, 251, 214-218, 220, 6-30, 140-196... all those with the original
   validate 0x756A00 in `VillagerOriginalFns.h`) call `villager_reactions::ReactionValidate` 0x756A00 from
   `LivingActionSystem::VillagerCallValidate` (V2 merge, 2026-10-01): with no reaction object (+0xBC), or not
   available, or in the hand if the reaction asks for it → `PopFromPrevious` 0x751E50. That is why `ReactToFire` 0x765870
   only returns 0 (without changing state) when the object is not an `Object` or has no fire, and `GoToTeleportReaction`
   0x7662F0 checks nothing (openblack only returns 0 if it does not store a stone, instead of reading a null); the
   custom exits (inferred) that did that work are gone.
3. If an entry / exit clip is playing (flag 0x800): it waits for it to finish (IsReadyForNewAnimation 0x5EC960 →
   FinishedIntoOutOfAnimation 0x750060) and does nothing else that turn.
4. CheckEveryTime 0x750410 and CallState 0x7521D0 (the TOP's function).

CheckEveryTime:
- controlled by a script (+0x25 & 4; written by GameThingWithPos::SetControlledByScript 0x402240, in openblack
  `ecs::script_held`) → nothing;
- wear: if the TOP is moving, life drops by the TOP's `LifeDrainPerTurn` and from there the row of the **raw
  FINAL** is looked at; otherwise, it drops by that of the final state (GetFinalState) and the TOP's row is looked at
  (literal quirk);
- if the row has checks (`DoPeriodicChecks`): life == 0 exactly → VillagerDead(CHANT if the final is 248-250 or
  flags & 2 / `WorshipVillager::atSite`, otherwise EXHAUSTION); if **more than** `processChecksEvery` (8) turns have
  passed → the periodic check, every 9
  turns: CheckDeathFromOldAge when `(turno + UniqueId) % 800 < t`; hurt (`vida < 0,3`, outside the home, the row
  allows going home, not knocked down, and in 19/20 only with `food > hungryForFood`) → SetTopState(36 GO_HOME) (0x7505C3).
  **Until V4 the rule is evaluated but not applied** (TODO(V4); `SetGoHomeEnabledForTests` switches it on in the tests):
  row 36 has no state function (GoHome 0x760270 → DoGoingHome 0x760280: home, path, ARRIVES_HOME) and the
  hurt villager would stay still forever (the audit saw 9 frozen in Land1 with `LIFE=0.25`); the trace
  writes `hurt (life ...): GO_HOME skipped until V4`;
  CheckChildGrownUp, WomanSpecial and CheckHungry (which resets `lastCheckTurn`);
- otherwise: the disciple check (flags & 0x200, a type that ignores needs according to g_DiscipleInfos +0xC, raw FINAL
  221, town with +0x5E8) → 163. Town +0x5E8 does not exist yet (0).

## State changes

Codes: 1 done, 0x2E the exit rejected (nothing changes), 0x2F the entry rejected.

- **Villager::SetTopState(s)** 0x752010: if `CanPauseForASecond(s)` (TOP ≠ 239, row with pause, no script):
  `x = 1 − vida` (life·0.5 if poisoned), and if `GameFloatRand(1) − 0,5·x³ < pauseForASecondChance (0,01)` →
  SetupPauseForASecond(s) 0x76B090 = SetCurrentAndDestinationState(239, s). Otherwise, Living::SetTopState; if it gives
  0x2F, CallEntryStateFunction(163).
- **Living::SetTopState(s)** 0x5F28E0: exit(s) → 0x2E; `out` = CallOutofAnimationFunction(s) (0x5F2900); entry(s)
  → 0x2F; SetStateSpeed (0x5F291B, unconditionally); if `out ≠ −1`, SetAnim(out); otherwise, SetStateAnim 0x5ECB10 and
  CallIntoAnimationFunction(s) (0x5F2947).
- **Living::SetCurrentAndDestinationState(c, d)** 0x5F2980: the same, but the exit (0x5F298B), the exit clip
  (0x5F299C) and the entry clip (0x5F29EB) receive **`d`**; only the double entry (vt +0x908) receives both.
- **CallExitStateFunction(s)** 0x752320: the TOP's exit and, if the final state is another one, also its own; 1 only if
  both give 1.
- **CallEntryStateFunction(s)** 0x7523D0: `entry[s](final, s)`; 1 → SetState(0, s). The double one 0x752440: that of
  `c`, then `entry[d](final de antes, d)`; 1 → SetState(1, d).
- **Villager::SetState(i, s)** 0x753690: PREVIOUS does not store a state with 0x10; the old state of any index,
  if it is final, leaves the town modifiers, and the new one enters (also in PREVIOUS: literal quirk); setting TOP
  first clears FINAL (with its adjustment) and sets +0x90 to 0. It does not touch the counter +0x58, nor clips, nor speed.
- **AdjustTownModifier** 0x753560: `town.desire.doingNow[d] ±= cantidad` and `doingNowCount[d] ±= 1` (TownDesire
  floats, town +0x510 / +0x554).
- **SetupMoveToWithHug(pos, final)** 0x5F2890: SetCurrentAndDestinationState(GLivingInfo +0x124 `moveState`, final)
  (0x5F2894..0x5F28AF; the 63 villager rows of info.dat have 1 MOVE_TO_POS) and, only if it gives 1, the walk
  (WallHug). Used by fire and teleport (`VillagerMove.h` forwards to VillagerCore).
- **Arrival of MOVE_TO_POS** (Living::MoveToPos 0x5EC270): MobileWallHug::MoveTo == 0xA → SetTopStateToFinal
  (0x5EC28E) = Villager::SetTopState(FINAL): pause roll, exit of MOVE_TO_POS (ExitMoveToPos 0x5EDDA0,
  CircleHugInfo::Reset, unported: counts as 1) and exit / entry of the FINAL. MoveTo 0x60AF20 only gives 0xA in ARRIVED
  (0x60AFC0, if AreWeThere(0)) and in FINAL_STEP (0x60AF6C), and both first put the object at the destination (Pos =
  destination, MoveMapObject vt +0x55C); otherwise it returns 0 / 1 / 6 / 7 and Living::MoveToPos does nothing: **there is
  no "abandoned" walk**. In openblack (`WallHugMoveToResult`): it arrives when it has the FinalStep (or Arrived) mark and is
  already at its destination (PathfindingSystem sets it with AreWeThere and places it at the destination on the next turn,
  as the original gives 0xA one turn after STEP_THROUGH sets FINAL_STEP). Previously a walk that the PathfindingSystem
  abandoned (without marks) counted as an arrival wherever it was: that is how the teleport villagers went round in
  circles 1 ⇄ 201 without ever reaching the stone.
- **The unported cases of the PathfindingSystem** (destination inside the circle being hugged, TODO #864, and the step
  from one circle to another, TODO #865; in the original MoveToCircleHugCircleSquareSweep<0/1> 0x614C40 / 0x6159F0):
  `AbandonMove` no longer drops the walk, it stays in STEP_THROUGH (0x60B02A: straight to the destination, without
  obstacles) **(approximate)** and the villager arrives through AreWeThere. In addition the PathfindingSystem's ARRIVED
  was inverted (it left ARRIVED just when it had arrived; 0x60AFC0 leaves when it has **not** arrived).
- The invented idle walk (radius 40, FINAL 209) **no longer exists** (V2): 163 is `Villager::DecideWhatToDo` 0x7515C0
  (see "Deciding what to do and leisure (V2)"). Only the debugging tools ("Move To Point") prepare walks with
  FINAL 0, which on arrival return to 163 via the compatibility path (the original would do SetTopState(0)).
- CallOutofAnimationFunction 0x756620 / CallIntoAnimationFunction 0x756590: see [animation.md](animation.md)
  (`VillagerCallOutOfAnimation`, `VillagerApplyStateClips`).

### Compatibility path (`LivingActionSystem::VillagerSetState`)

| call | what it does |
|---|---|
| TOP, `skipTransition = false` (worship) | exact `villager::SetTopState` |
| TOP, `skipTransition = true` (hand, physics, animals, the arrival of a walk with FINAL 0, LANDED, Gui) | if it is the same state, nothing; otherwise, exact `villager::SetState(0, s)` + clips and speed. **(approximate)**: the original goes through SetTopState with EnterInHand / ExitInHand..., not ported |
| FINAL / PREVIOUS | exact `villager::SetState(i, s)` |

## States 85 and 239

- **85 CREATED** (Villager::VillagerCreated 0x753DD0): `v = +0x58; +0x58 = v − 1; si v == 0 → +0x58 = 0 y
  SetTopState(163)`. It moves to 163 on call number counter + 1 (1..501 turns).
- **239 PAUSE_FOR_A_SECOND** (0x76B0B0): when the clip ends (IsReadyForNewAnimation(1)), SetTopStateToFinal 0x5ECA80
  = SetTopState(FINAL): the exit and the entry of `s` run again; it does not pause again (TOP == 239). While it lasts,
  GetFinalState() = s.

## Speed (Villager::SetStateSpeed 0x753760)

It is called unconditionally in both SetTopState; the skips are its own: nothing changes if the villager is controlled by
a script (GameThingWithPos +0x25 & 4, 0x753766) or if it is dancing (Living::IsDancing 0x5ECC10, 0x753772: the DanceGroup
of Living +0xD8; in openblack `WorshipVillager::dancing` or TOP == IN_DANCE, **(approximate)**). Thus the villager that
enters 60 WORSHIPPING_AT_WORSHIP_SITE after FindDanceGroup keeps its speed. Afterwards it takes the final state and SetSpeed
0x750ED0 (formulas in [animation.md](animation.md)). The age is GetAge (unsigned) and is compared **unsigned** with
grownUpAge (0x750F26, `jae`) and oldAge (0x750F87, `jbe`); the differences are loaded as an unsigned qword, ×0.2×0.1,
maximum 0.4. The adult subtracts
`GetDesireForFood()·0,1` (0x750FDB, POWER 0x75BB60 = `1 − min(food, 1)³`), `vida·0,1` and 0.2 if it is a woman.

## Deciding what to do and leisure (V2)

Full spec: `dev\documentacion\aldeanos\V2_spec.md`. Code: `Villager/VillagerDecide.{h,cpp}`, `Villager/VillagerHome.{h,cpp}`,
`Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`; tests `test/test_villager_decide.cpp` (13 cases).

- **163 DECIDE_WHAT_TO_DO** (0x7515C0, returns 1): town emergency (Town::IsInStateOfEmergency 0x747970, +0xF1C
  `Town::emergencyStartTurn`, nobody writes it yet: TODO(Milagros)) → 242; disciple / follower (DiscipleDecideWhatToDo
  neutral, V14); `SetTopState(163)`; child → ChildDecideWhatToDo (CheckChild, town distribution neutral, creche neutral,
  → 114); CheckNeededForSomething (homeless: neutral V4 → CheckNeededForSpecial: **Milagros worship**, civic (V3:
  computes the trigger and clears `flags & 1`; the distribution, since V3, in [Town desires and distribution (V3)](#town-desires-and-distribution-v3)), own desires with threshold 0.3) → CheckTakeResourcesToStoragePit
  (→ 31) → SetupNothingToDo. Worship no longer goes at the start of 163: it is in its place (0x760013), before the idle
  branch, and is also checked from 246.
- Desires: food = 1 − min(food, 1)³, life = 1 − ((life − min(0.3, life)) / 0.7)²; the largest first, strict
  comparisons with 0. CheckSatisfySleep 0x761490 does not look at the time of day (with a home → 36).
  ChangeStateToFindFoodToEat neutral (V4).
- **SetupNothingToDo** 0x753B50: GameRand(9), table 0x753C64 = 0,1,1,1,2,2,2,2,2. Branch 0: functional home → 36;
  otherwise GameRand(100) < 10 → 36, otherwise falls through to 1. Branch 1: with a home → 245; otherwise falls through
  to 2. Branch 2: with a town, walks to GetChillOutPos (meeting point + R..10R, ±22.5° on its side, R = 0.1·GTownInfo
  +0x140) with FINAL 246; otherwise 36. It always returns 1.
- **209** returns 1 (only scripts set it). **245** GoAndChilloutOutsideHome 0x76B3F0 and **252** GoAndChilloutInTown
  0x76B590 → GetMeToMyChillOutPos 0x76B610 (far: walks to GetPosOutside(3, R/2, R/2) of the door; near and clear
  (CheckForClearArea with 1.2·radius): LookAtPos one step and 246; occupied: FindClearArea(5, 1)). **246** SitAndChillout
  0x76B4E0: entry 500 turns (+0x394), then a check every 101 calls (+0x396 = 100): emergency, CheckNeededFor
  Something, GameRand(10) == 0 → SetupNothingToDo without going through 163. SitDown clip: the 0x800 bit before the
  current clip.
- **36 GO_HOME** = DoGoingHome(37, 238): with a home, walks to the door with FINAL 37 (V4: 37 makes it go in). Without a
  home: the tent or 130 (V4). The "hurt → 36" rule of CheckEveryTime is switched on in the game.
- **114 CHILD_FOLLOWS_MOTHER** 0x7578C0: CheckChild, distribution, creche; otherwise, walks to the mother (or to the home)
  + 5 m at a random angle (GameFloatRand(2π), VillagerChild.cpp 0x39) if the point is navigable; without mother or home,
  CheckNeedNewAbode (neutral V4). Row 114 carries +0x50 AlwaysReactToTownEmergency (0xD0D208 = 0x5AC990), like 36 and
  209.
- Town: GetCongregationPos 0x7408B0 with cache `Town::congregationPos` (+0xF10; also written by
  SET_TOWN_CONGREGATION_POS); mean of the homes that are not fields (with < 3, plus the plans) and FindClearArea(130, 3, 10,
  BlocksTownClearArea); otherwise, base + 10..20 m. The list of homes (+0x754) goes from newest to oldest
  (AddStructureToTown inserts at the head, 0x7399C3..0x7399CF); that of plans (+0x9A8) from oldest to newest
  (AddPlanned appends at the end, 0x73D08A..0x73D0AD). The height of the result is that of the last one read
  (0x7409C1..0x7409DB). SET_TOWN_CONGREGATION_POS: GetScriptPos 0x718250 → MapCoords::Set 0x603280 (x, z; height 0, or
  the third field unscaled if present, 0x6032E4); the offset 0xD99724 is null when loading a land (LoadMapFeatures
  0x7180FE) and only the vortex sets it (fn_0076FA50).

### Deviations and visible effects until V4

- Villagers in 37 ARRIVES_HOME (no function) and in 36 without a home (DoGoingHome returns 1 without doing anything) stay
  still forever; the number grows during the game (Land1: 4 → 5 → 6 in 37 on turns 400 / 800 / 1200, plus 3 in 36
  without a home). Accepted (P-1).
- **Homeless children stay still in 114** until V4: without a mother (those created at map start have none, the mother is
  null when the age < grownUpAge) and without a home, ChildFollowsMother calls CheckNeedNewAbode 0x757F90 (neutral) and
  returns 1; if they are hungry, CheckChild → GoHome → the homeless branch of DoGoingHome 0x760310 (neutral) returns 1.
  Before V2 they walked around (invented walk). Land1: 5–6 children in 114 standing still (12, 13 at (1748.4, 2679.6)
  since their creation). The trace says so once per villager: `child 114: no mother, no abode -> CheckNeedNewAbode …` and
  `home: no abode -> DoGoingHome's homeless branch …`. Land1 (2026-10-01, `_mapa_run\audit_v2_fix.log`, 367 turns): children 60, 12 and 67 via
  CheckNeedNewAbode, villagers 1992 and 1991 via the homeless branch of DoGoingHome.
- Those that stay in 37 or in 36 without a home do not return to CheckNeededForSomething: the Milagros worship
  (CheckNeededForWorship) cannot recruit them and the pool of worshippers shrinks during the game.
- Other paths also end in 37: a villager that survives being eaten is left with life 0.05 (AnimalPredators
  ProcessDownedVillagers → LANDED → 163) and the "hurt → 36" rule takes it to the door and to 37; the same for those hurt
  by fire or by `OPENBLACK_TEST_HURT_VILLAGERS`.
- New hooks: `OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]"`, `OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]"` (forces
  the next GameRand(9)), `OPENBLACK_TEST_VILLAGER_SHOT="<turno>,<png>[;...]"` (capture on that turn). The trace adds
  `decide: …`, `chill 245/252: …`, `sit 246: check -> …`, `home 36: …`, `child 114: …` and `congregation town …`.

Checked (2026-10-01): Land1, 2532 turns, all villagers: 62 SetupNothingToDo (r = 0..8: 11, 3, 10, 8, 6, 4, 7, 5,
8) → 36 × 11, 245 × 18, 246 × 33; 245: 20 "far", 17 "near and clear", 11 "occupied" (by its home, see assumption 21);
246: 274 "again" and 26 "nothing"; a single warning of 37 ARRIVES_HOME; no 163/209/245/246 without function. Meeting point
of towns 0 (1789.3, 2681.3) and 4 (2479.1, 2542.7) by the mean. Land2 with `OPENBLACK_TEST_WORSHIP="1,0.5"`, 3335
turns: r = 0..8 spread out (30..45 each), 1734 checks of 246 "again" and 185 "nothing", 10 towns with a meeting
point, worship continues (11 worshippers in 59/60).

## Town desires and distribution (V3)

Full spec: `dev\documentacion\aldeanos\V3_spec.md` (disassemblies `town_dis\desire.txt`, `desire_fns.txt`, `rep.txt`;
`v3\emu_dtab_all.py` extracts the table, `v3\emu_qsort.py` runs the exe's `_qsort`). Code: `Town/TownDesire.{h,cpp}`
(table, functions, Process, orders, distribution, read API, scripts), `Town/TownProcess.{h,cpp}` (Town::Process and the
player loop), `Town/TownStats.{h,cpp}`, `Villager/VillagerSatisfy.{h,cpp}` (the CheckSatisfy), `Components/Town.h`
(`TownDesire`, `DesireSort`, `TownStats` and the new town fields); tests `test/test_town_desire.cpp` (16 cases) and
a new case in `test/test_villager_decide.cpp`.

- **Order in the turn** (milagros2, with asistente's OK): `town_process::ProcessPlayers` is called once per turn
  from `magic::ProcessTurnStart` (Magic/MagicLoop.cpp, slot 3 of GGame::ProcessTurn). It goes after
  `InfluenceRing::ProcessRings` (0x54E63C), so Town::Process sees this turn's rings. Afterwards come the teleport
  travellers (fn_005FCC70, 0x6496BC) and the alignment (0x6496C5), and then the dances, GlobalGameLists, the
  forests and, at the end, Living (0x54E65B). **(approximate)**: GPlayer::Process 0x6494E0 does towns, teleport and
  alignment player by player; here each step goes through all players before moving on to the next.

- **Table** (faithful): 0xDA32C8 + d·0x68, filled by crt_xc 0x744BD0: name, function (+0x10), Amount/Desired (+0x20/+0x30,
  only 5, 6, 7; only the trace 0x745EC0 reads them), the villager's CheckSatisfy (+0x40), modification (+0x50), children
  (+0x60: 2, 3, 4, 15, 16) and +0x64 (no reader). Per-desire info 0xDA2930 + d·0x90 (+0x18 trigger, +0x58 TribeMultiplier[9]).
  Names for `TOWN_DESIRE_BOOST`: fn_747270 (`_stricmp`).
- **Functions** (faithful, x87 in double, see assumptions): Food = `Town::CalculateDesireForFood` 0x747F00 (thunk 0x747340):
  `1 − (comida + 1e-4)/(5·Σ foodReqiredForDinner + 1e-4)`, with the warning `HelpSpritesLowOnFood(min(v,2) − 0,9)`
  (0x747FA0) if v ≥ 0.95 and the town belongs to the local player; Wood 0x747FF0 with S = min(R5+R6+R9+R12, 3), a =
  (craftsmen + 0.001)/(adults + 0.001) + S, **k = max(abodes/10, 1)**, B = 500k, C = 5000k and the LowOnWood warning
  (0x7481BC, min(v,2) − 1); Abodes 0x748210 (max(a, c)⁴·(1 − R9)(1 − D6)); Civic 0x748330 (PopulationWhenNeeded of
  GAbodeInfo::Find 0x405B30, the first match); For_Children 0x748430 (milagros2's alignment and TribalPower[4] of
  `PlayerMagic`, 1.0); To_Build 0x748640 and Repair_Town 0x7486B0 (Abode::GetDesireToBeRepaired 0x406970 with the life
  `ecs::life`); Playtime 0x7487B0 (0.1 if D0, D1, D5, D6, D9 < trigger and turn > 4000); Relaxation 0x7488C0 and Sleep
  0x748960 (`sky_type::At` and `EveningRamp` over the visual time of day of `Game`'s clock; Sleep reaches 6.25 at night).
  Protection 0x7488A0 / Mercy 0x7488B0 read Town +0xEC0 / +0xEBC, which are 0 until the aggressions (**pending**);
  For_Wonder 0x748740 = 0 without `GetBeliefInPlayer` (**pending**, milagros2); Supply_Worship, For_Rain, For_Sun,
  Suppy_Workshop = 0 (literal).
- **TownDesire::Process** 0x745AE0 (faithful): +0x164, the 17 in order 0..16 (a desire that reads another of higher index
  sees the previous turn's), `CallDesireFunction` 0x745D80 (raw +0x168 = f·TribeMultiplier unclamped; desire +0x118 =
  clamp(raw·modification, −1, 1)), modifications 0x746490 / 0x7462A0 (150 = GVillagerInfo[10] maxFoodCarried) /
  0x746350 (250) / 0x746400, the two orders and, every 50 turns, `(2R0 + R1 + max(R3,R4) + max(R5,R6))/5` with
  `HelpSpritesVillagerUnhappy` (0x745C8A) if it exceeds 0.6 and belongs to the local player. The player's statistic
  (GPlayer +0xA44) is **pending**.
- **Orders** (faithful): order 1 (+0x278, value GetDesire, +0 = boosts A + script) and order 2 (+0x344, GetRawDesire,
  +0 = boost A) with the VC6 CRT `_qsort` 0x7C7E64 ported literally (CUTOFF 8, `_shortsort` 0x7C7FB8, pivot in the
  middle; it is not stable: with everything at 0 it ends up `8 1 2 3 4 5 6 7 0 9 … 16`). The tests compare with
  `emu_qsort.py`.
- **Distribution** `CheckVillagerNeededForTownDesire` 0x745FF0 (faithful): trigger 0 → 0.001; t = min(trigger + info
  +0x18, 1); the entries without CheckSatisfy and, for a child, those without +0x60 are skipped without cutting off; it
  cuts off (0) at the first eligible one with `TempMod(k)·valor ≤ t` and returns 1 when a CheckSatisfy gives 1. **Quirk
  kept**: `TempMod` is requested with the loop index k, not with the desire's. It is called by fn_7581A0
  (`villager::CheckNeededForTownDesire`, VillagerDecide.cpp).
- **CheckSatisfy** (V3): Sleep is the V2 one; Playtime 0 and Relaxation 0 (literal: no football, creature or artefacts);
  Food (V8), Wood (V9), Abodes / Civic (V6/V7), Supply_Worship (milagros2), To_Build (V7), Repair (V11) and Workshop
  return 0 (**pending**). Consequence: in V3 only Sleep produces behaviour.
- **Town::Process** 0x747380 (`town_process::ProcessTown`): the turn's TownStats, +0x5E4 = 0, TownDesire::Process, every
  10 turns `worship::percentage::GetWorshipersNeeded(1, 0)` / `AdjustWorshipersWorshipping(n, 1, 0)` from milagros2
  (fn_7489F0; before nobody called it), the pulse +0x5E8/+0x5EC and the countdown +0xF20; the other steps with TODO and
  their address (building sites V6, homes V4, artefacts, flags, aggressions, repairs V11, emergency, creature, pots
  V5, missionaries, belief, alignment by desires, Shuffle V4). `town_process::ProcessPlayers` iterates over
  `map_cells::ForEachTown` and is called in `Game::GameLogicLoop` between the sharks and the PuzzleGames, before the
  villagers (GPlayer::ProcessPlayers 0x54E641 goes before Living::ProcessLiving 0x54E65B). The influence (+0x5C8, steps
  3-5 of the original: 0x7473A0 / 0x7473AD / 0x7473BD) is not called here: it is computed every turn by milagros2's
  `influence::ProcessTowns` from its own hook (`influence::ProcessTurn`, inside `magic::ProcessTurn`).
- **Script** (faithful): CHL `SET_TOWN_DESIRE_BOOST` (341) = GScript::SetTownDesireBoost 0x6FE650 (town, d < 17,
  −1 ≤ v ≤ 1 → +0xD4[d] = v and re-sorts only order 1; "Thing not valid!" / "Invalid Params"); CHL `GET_DESIRE` (234) =
  0x6FCCA0 (invalid d → "Invalid desire" and 0 **without popping the object**; otherwise, GetRawDesire); the map command
  `TOWN_DESIRE_BOOST` 0x7179EC writes +0xD4 without re-sorting or checking the range (Land2.txt: "Abodes" /
  "Civic_Buildings" −0.75).
- **API for other sessions** (`ECS/Town/TownDesire.h`): `GetDesire` / `GetRawDesire`, `GetSortedDesires` (+0x278),
  `GetSortedRawDesires` (+0x344 = Town +0x37C value / +0x380 type, what CheckTownDesiresSFX 0x71B130 reads), `GetField`
  (+0x90 / +0xD4 / +0x118 / +0x168), `GetDesireSignificanceToVillager` 0x746660, `GetMostDesired` 0x745E50,
  `GetMostSignificantRawDesire` 0x745EA0, `CalculateDesireForFood` (with warning) / `FoodDesireValue` (without warning),
  `SetBoost`, `AlignmentTurns` (+0x410, only milagros2). Filling audio's `desireTowns` / `townResourceNeeds` is left
  for audio (**pending**, P-2).

### Deviations and visible effects until V4

- At night Sleep is the first one (raw up to 6.25, desire 1) and CheckSatisfySleep sends to 36; V4 puts them in the
  home (37 → 38 → 119 → 120, see [V4](#home-food-sleep-homeless-and-age-v4)). By day the distribution cuts off at
  Relaxation/Playtime (CheckSatisfy 0) and everything continues as in V2.
- milagros2's worship now receives, every 10 turns, the worshipper adjustment from Town::Process (fn_7489F0).
- **(approximate)** TownStats is recomputed at the start of Town::Process from the entities (the original adds when
  adding and removing); same counts, the float sums in a different order. **(approximate until V6)** all script homes
  count as functional, and `Town::storagePit` (+0x30) / `Town::creche` (+0x744) are set by script creation
  (AbodeArchetype) instead of StoragePit / Creche::MakeFunctional (the last storehouse wins; the first creche).
- **(approximate)** the x87 (80-bit) chains are computed in double and stored in float where the original does
  `fstp dword`.
- **(approximate)** the turn's influence (milagros2, `magic::ProcessTurn`) is computed after the desires and not inside
  each Town::Process (the desires do not read it).
- **(approximate)** `SET_TOWN_DESIRE_BOOST` with negative d does not write (the original writes outside the array).
- **(inferred)** +0x90 is 0 in a new game (only Load writes it); TRIBE_TYPE = the `Tribe` enum for
  TribeMultiplier; "local player" = PLAYER_ONE; the list +0x770 is empty; fn_555240 (step 23) needs no call.
- Not ported (no behaviour): the network checksum [0xDA2770], the debug trace 0x7457C0 (replaced by
  `OPENBLACK_TOWN_TRACE`) and the functions without calls (0x745E80, 0x745FA0, 0x7461E0, 0x746220, 0x7465F0, 0x7466B0,
  0x7468E0).

## Home, food, sleep, homeless and age (V4)

Full spec: `dev\documentacion\aldeanos\V4_spec.md` (disassemblies in `v4\`: `home.txt`, `homeless.txt`, `food.txt`,
`age.txt`, `abode.txt`, `town.txt`, `misc*.txt`, `helpers.txt`, `scanline.txt`; test values in `v4\v4calc.py`).
Code: `Villager/VillagerHome.{h,cpp}` (36/37/38, 119/120/121, 129, 130, 234, 238, the tent, the moves),
`Villager/VillagerFood.{h,cpp}` (CheckHungry, amounts, 117/118/212, 33/34/35), `Villager/VillagerAge.{h,cpp}` (growing up,
scale, old age, pregnancy), `Villager/VillagerResources.{h,cpp}` (what it carries and takes: the food half of V5),
`Town/AbodeVillagers.{h,cpp}` (the home's list, PresentAtHome, the score, Abode::Process, the Shuffle's
moves), `Town/TownVillagers.{h,cpp}` (homeless, vagrants, AddVillagerToTown, FindAbodeWithSpaceInTown, UseFood,
Shuffle); tests `test/test_villager_food.cpp`, `test_villager_home.cpp`, `test_villager_age.cpp`.

- **The night** (faithful): Sleep (16) on top → distribution → CheckSatisfySleep 0x761490 → 36 → door → **37** ArrivesHome
  0x760930 → `Villager::ArriveHome` 0x751FA0 (bit 4 of +0xE0, `Abode::presentAtHome` +0xB6 `inc`, mesh hidden by the
  −4 clip) → **38** AtHome 0x760B10 = HomeDecideWhatToDo 0x75FEA0 → CheckSatisfySleep inside → CheckWhenGoingToBed
  0x760B60 (returns 1 unless it dies of old age; once per stay, bit 0x2000) → **119** GotoBedAtHome 0x760B30 → **120**
  SleepingAtHome 0x760D70 (counter RestAtHomeTime 100; without a town it does not count) → DoSleeping 0x760DB0 every 100
  turns (+0.05 life unless poisoned; continues while Sleep is the first of order 1 or life < 0.7: from 0.4 to 0.70000005
  in 6 cycles). By day DoSleeping gives 0 → 38 → distribution or leisure → the exit **ExitAtHome** 0x761B40 of 35..38 and
  118..121 does LeaveHome (0x751FD0: bits 4 and 0x2000 off, `presentAtHome` `dec`) if the next state does not stay at
  home (info.dat row, file 0xC0). 121 WakeUpAtHome 0x760E50 = GoHome (no code sets it).
- **37** (faithful): has not arrived (AreWeThere(door, 0)) → again to the door with FINAL 37 (literal, also from 249);
  built and repaired (life ≥ 1, IsRepaired 0x4016A0) → inside; damaged (< 0.3): functional home → inside, otherwise tent
  (238); hungry (food < 0.5 strict): `SetTopState(163)` if it is not functional and inside in the same turn (literal);
  otherwise SetupBuildingObject 0x758530 (neutral: V7/V11) and inside. Without a home → 129 and 0.
- **38** (faithful): emergency → 119; CheckNeedsAtHome 0x760110 (the pregnant woman stays; threshold
  `0,9·max(GetLifeDesireFromLife(0,7), POWER(0,5))` = 0.7875, the larger, not the smaller; 0.9 for the disciple that
  ignores needs; the child goes through CheckChildActivity = ChildDecideWhatToDo, always 1); the disciple;
  CheckNeededForSomething (also milagros2's worship every turn); HomeNothingToDo 0x75FFB0 (inside, GameRand(4) == 0 → 119
  with counter 0).
- **Homeless** (faithful): DoGoingHome 0x760280 without a town → 130; more than 100 m from its town → walk to 10..35 m
  from it, on its side (FINAL the TOP); near → GetTentPos 0x7604F0 → 238, or a walk of 10..30 m. The tent: the nearest
  tree within 50 m (fn_00604AF0 with IsTree) if fn_0074C650 finds room for it (2 m from the tree, on the other side of the
  occupant; a villager in 238 or a MultiMapFixed within 4 m counts; two = full, and no other tree is tried); otherwise, 3
  attempts: free cell (`Collide & 0x19 == 0`) and no villager in 238 within 5 m in the 9 cells of a spiral that **moves
  the point**: the tent ends up at (−20 m, +10 m) from the tested spot (literal quirk). **238** SleepInTent 0x761AE0,
  **129** HomelessStart 0x761320, **130** VagrantStart 0x76A8D0 (a town of its tribe within 200 m → AddVillagerToTown and
  163; hurt → tent; otherwise a walk of 10..30 m forward).
- **Eating** (faithful; P-1: the storehouse's 33/34 come in V4): **CheckHungry** 0x75BCC0 (batch = turns·9e-5 divided by
  TribalPower[3] (player +0x74) and by the speed if it exceeds 1 and it is moving; damage 0.001 with hunger (food < 0.5,
  strict: IsHungry uses ≤) or poison: the `max(…, 1)` leaves the factor at 1; interruptions 0xD0 / 0xD4 of the final
  state's row; life 0 → STARVING, or CHANT from worship); the amount GetAmountOfFoodToEat 0x75BC20 =
  `ftol((1 − 0,3·clamp(deseo de Food del pueblo))·(float)(POWER(food)·85))` (74 with food 0.5);
  **ChangeStateToFindFoodToEat** 0x75B990 (needs 0 → 117, or 118 inside; its functional home with enough → 36 / 118; the
  storehouse —the town's or, if there is none, its home— functional with enough → 33; without a functional storehouse →
  to the delivery point with FINAL 34; if it carries something, it eats it; otherwise 0); **117 / 118** EatFoodHeld
  0x75BF20 (`comido/aComer·1,2 + food`, clamped to [0, 1], NaN → 0; Town::UseFood 0x73B5E0 adds to `Town::foodUsed`
  +0x6F8); **GetFoodFromHome 0x75C040 takes twice** (GetResourceFrom already does PickupResource: the home loses n and
  the villager gains 2n, literal quirk); **34** ArrivesAtStoragePitForResource 0x7698D0 (takes min(what it needs, what
  there is) and returns to the door with FINAL 163; then it eats what it carries); **212** ShowPoisoned 0x75B940; **35**
  ArrivesAtHomeWithFood 0x769B30 (the housewife's, V14). The home subtracts its food with DoResourceRemoving 0x404F60
  (the town's CallDesireFunction first, `town_desire::CallDesireFunctionNow`).
- **Age** (faithful): CheckChildGrownUp 0x751050 at 13 → bit 8 off, age 18, ChildToAdult of the home (or of the town) and
  ChildBecomesAdult 0x757F10 (mother 0, CheckNeedNewAbode, **234** GoHomeAndChange 0x761810); the adult mesh arrives at
  the exit of 234 (ExitGoHomeAndChange 0x761980 → ChangeTribeIfRequired 0x7618C0 → ChangeInfo 0x761A00), not in SetAge.
  Otherwise, it rescales every 375 turns (only the children whose check falls on those turns: gcd(9, 375) = 3,
  **(inferred)**). Old age CheckDeathFromOldAge 0x760CA0 (in the periodic check, ≈ every 800 turns, and in
  CheckWhenGoingToBed): age > 60, `n = ftol(r³·40)` (the cube, not the square), `GameRand(n)`, dies if age + d > 100:
  nobody before 63. WomanSpecial 0x752240 (the pregnancy countdown) is literal; childbirth is V14. The scale of the
  constructor and of SetScaleForAge now uses the synchronised GameFloatRand (V1 used openblack's generator: it changes the
  order of the constructor's rolls).
- **Home and town** (faithful): `Abode::inhabitants` is the ordered list +0xA0 (the head, the most recent: decides who
  moves in the Shuffle and the partner at bedtime), `maleFemale` +0xA8 / +0xAC, `adultCount` / `adultMaleCount` /
  `childCount` +0xB4 / +0xB5 / +0xB7, `emptyTimer` +0xB0; `Town::homelessVillagers` +0x768, ordered. AddVillagerToAbode
  0x404060, RemoveAliveVillagerFromAbode 0x404340 (inside → 163; its exit does the LeaveHome; the partner is not
  touched), RemoveDeletedVillagerFromAbode 0x404220 (clears both partners), RemoveAllVillagersFromAbode 0x404560 (the
  home destroyed, Buildings.cpp → HomeDeleted → MakeHomeless), the score 0x404B40, FindAbodeWithSpaceInTown 0x73B370 (the
  newest wins ties), AddVillagerToTown 0x73A090 (milagros2's CheckAddWorshipSite with the first villager),
  CheckNeedNewAbode 0x757F90 (with percentTooCrowded 0.5, an adult alone in a home for 2 is already «too many»: it moves
  if there is something better or becomes homeless, literal). Town::Process step 4 (Abode::Process 0x404440: an empty
  built home loses 0.0001 life every 1001 processed turns, in float) and step 24 (ShuffleVillagersAroundAbodes 0x741540
  with VC6's `_qsort`, one move per call).
- **Creation by script** (faithful; P-6): CREATE_VILLAGER_POS ("AALN", 0x715A4C) creates the villager at the second
  argument and looks for the town with FindTownWithID of integer slot 0. LHScriptX::ScanLine 0x7E7540 only writes the
  slot of an 'N' argument (atol), so it holds the id of the last command with an 'N' first (in the lands, the
  CREATE_ABODE or CREATE_TOWN just before: `lhscriptx::Script::IntSlot`); without that town, the one nearest to the
  villager's position (fn_00552FF0). Then AddVillagerToTown chooses the home. openblack's rule «the home within 1 m² of
  the script position» was removed.
- **APIs** for other sessions: `villager::IsAtHome`, `IsReachable` (0x756460: available, not at home, TOP ≠ 236; used by
  AnimalPredators instead of its test of states 13..18), `LeaveHome`; `abode_villagers::VillagersOf`,
  `PresentAtHome`, `RemoveAllVillagersFromAbode`; `town_villagers::Homeless`, `AddVillagerToTown`;
  `town_desire::CallDesireFunctionNow`.
- **(inferred)**: TribalPower[3] is 1.0 (nobody writes it); the list of vagrants is empty (its writers are
  V12/V14); the non-villager occupant of fn_0074C650 (+0x24 & 2) is a MultiMapFixed (+0x24 & 4 of IsReachable and
  IsAvailableForStateChange is «in the hand», PlaceObjectInMagicHand 0x5FB014: `fire::traits::InHand`); 115 only by
  script; IsInScript of a home (+0x24 & 0x200) is 0; IsTree = the Tree component.
- **(approximate)**: the speed +0x5A comes from `WallHug::speed`; IsMoving = the WallHug's last step is not zero;
  TownStats recomputed in each Town::Process (V3) with `males` / `females`, and AddVillagerToTown, Town::RemoveVillager
  and ChildToAdult update adults, children and sexes immediately; the homes' influence goes in milagros2's hook, not in
  step 4; Abode::ReduceLife 0x405D90 has no entry point (Object::ReduceLife of `ecs::life` is used); the provisional
  Kill does LeaveHome first (until V12); the types of openblack's signatures stand in for the exe's type strings
  for the integer slots; without the temporary pot (V5), the delivery point is the villager's position.
- **Pending**: CheckGetPregnantAtHome neutral and no births (V14, P-2: a pregnant woman without a birth would stay at
  home forever); the temporary pot (V5); SetupBuildingObject on arrival (V7/V11); the dance in DoGoingHome;
  SetVillagerDisciple in 234 and HousewifeStartsGivingBirth (V14); Town::RemoveVillager only with lists and counts (V12);
  the worship rows 248-250 (`DoGoingHome(249, 250)`, ArrivesHome, SleepInTent, ExitAtHome) await milagros2's approval;
  the hand that picks up a villager from inside (P-9); the night windows still use `inhabitants` until sistemas applies
  `presentAtHome` (Abode::Draw 0x515F78); the in-game captures (V4_spec §13).

## Carrying resources and the storehouse (V5)

**Pending** (2026-10-03: the research was stopped by the user's decision before writing the specification; there is no
V5 code). What already exists from V4: `Villager/VillagerResources.{h,cpp}` with states 33/34 (eating from the
storehouse). Missing: picking up and dropping resources (PickupResource / Drop*), the capacities per villager type, the
temporary pot (Town::GetTemporaryResourceStorePotOrPos 0x73E900), taking food and wood to the storehouse
(StoragePit::AddResource 0x732F60, Abode::DoResourceAdding 0x404DF0), CheckSatisfyFoodDesire 0x759F30, the carried object
(SetStateCarriedObject 0x7501A0), CreateDroppedResource 0x750940 and reaction 9. Dumps to pick it up again:
`dev\documentacion\aldeanos\v5\README.md`.

## Worship: return home

CheckVillagerGoBackToTownFromWorship 0x76BEC0 (Milagros' file) returns the code of SetTopState(248) == 1
(0x76BF59..0x76BF6D), not "TOP == 248": if it pauses first (239 with FINAL 248) it also returns 1 and the villager has
already left. Previously, after a pause, ProcessInWorship carried on with a villager that was no longer at the site
(again in the return queue, one more worshipper requested, extra chant damage and a stale entry at the front of the
queue).

## Death (provisional until V12)

`villager::VillagerDead` marks the villager, writes `Villager <n> died (<motivo>)` and `FlushDeaths` kills it at the end
of the turn (`ecs::life::Kill`). The original leaves it alive (SetDying → 13) and keeps calling CallState; here it no
longer does **(approximate until V12)**. V4: before the Kill, `LeaveHome` (if it was inside), so that `presentAtHome` does
not stay high (in the original the exit of the state towards 13 would do it).

## Test hooks

- `OPENBLACK_VILLAGER_TRACE=1` (or `=<n>`, creation index): `Villager trace:` lines with the creation (position,
  age, food, lastCheckTurn, counter, 85 or 16), each `SetState`, `SetTopState a → b = código`,
  `SetCurrentAndDestinationState`, `pause 239 → s (rand, umbral)`, `AdjustTownModifier`, each periodic check, a
  summary every 100 turns and each call to the fire entries and exits (`EnterPutOutFire(final, s) = r`,
  `ExitPutOutFire(final, s) = 1`, `EnterOnFire`, `ExitOnFire`), `ExitReaction(s) reactive r`,
  `ExitReactToTeleport(s) same r`, `StopReacting`, `PopFromPrevious stored a -> resume b = código`; the summary carries
  the position, the destination and the walk mark (L / O / E / S / F / A / -).
- `OPENBLACK_TEST_VILLAGER_LIFE="<vida>[,<n>]"`: on turn 2 sets the life of all villagers or of villager n.
- `OPENBLACK_TEST_VILLAGER_STATE="<estado>[,<n>]"`: on turn 2 calls `villager::SetTopState` and writes the code.
- `OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z"`: on turn 2 creates a Celtic villager (Housewife, 25 years old) there.
- `OPENBLACK_TEST_VILLAGER_POISONED=<n>`: on turn 2 poisons villager n.
- `OPENBLACK_TOWN_TRACE=1[,<cada>][,raw]` (V3): per town, every `<cada>` turns (50) and whenever the first of order 1
  changes: `town <id> turn <t> pop <p>: [16 Sleep 1.000 raw 6.250] [15 Relaxation 0.100] …` (the 17, order 1) and
  `avg <a>` on the 50-turn turns; with `,raw` also order 2.
- `OPENBLACK_VILLAGER_TRACE` (V3): the distribution writes `civic: t=<t> k=<k> d=<d> v=<v> tmp=<m> -> skip(child)|skip(nocs)|
  cut|cs=0|cs=1`.
- `OPENBLACK_TEST_TOWN_DESIRE="<d>,<boost>[,<pueblo>]"` (V3): on turn 2, SetBoost like SET_TOWN_DESIRE_BOOST (re-sorts
  order 1) in all towns or in the one with that id.
- `OPENBLACK_TEST_VILLAGER_AGE="<edad>[,<n>]"` (V4): on turn 2, only Living::SetAge (the birth turn), without
  meshes or bits (12.99 → 13 and old age). `OPENBLACK_TEST_HOMELESS=<n>` (V4): on turn 2, MakeHomeless of villager n.
- `OPENBLACK_VILLAGER_TRACE` (V4) adds `home 36: …` (to the door / no abode -> far / tent / wander / vagrant 130),
  `home 37: not there|arrive (present <n>)|tent|hungry 163+arrive|repair TODO(V7)`, `home 38: emergency|needs(t=…)|
  disciple|something|nothing r4=<r>`, `exit-home <s> -> <next> (stay|leave, present <n>)`, `sleep 120: life <l> ->
  keep|wake`, `tent: tree|spiral try|fail`, `food: …`, `eat: …`, `home-food: took <m> held <h>`, `age: grown|rescale|
  old age r= n= d= -> die|live`, `homeless: into abode|list`, `abode: moves|too crowded`, `vagrant 130: …` and, every 100
  turns, `home: town <id> inside <n> asleep <m> tents <k> homeless <h> vagrants <v>`. `OPENBLACK_TOWN_TRACE`:
  `shuffle: <casa> -> <casa> (swap|take …) = <r>`.

Checked (2026-10-01): Land1, 58 villagers, all 85 → 163 with code 1, checks every 9 turns, wear 2e-6 per turn
when walking; with `LIFE=0.2` and `STATE=246`, 14 of 55 pause (239 → 246; ~27 % was expected) and on the next check they
go to 36 (before deferring GO_HOME to V4); with `LIFE=0`, `died (EXHAUSTION)` without a hang; `OPENBLACK_TEST_THROW_VILLAGER`:
85 → 10 → 11 → 163. Land2, `OPENBLACK_TEST_WORSHIP="1,0.5"`: they reach 59 and 60; three villagers are born in the sea in 16.

After the audit (2026-10-01): Land1 with `LIFE=0.25`, 731 turns: no villager enters 36 (3066 `GO_HOME
skipped` lines), they keep walking and arriving (35 arrivals via the bridge to 163); normal Land1, 338 turns, no changes.
Land2, `WORSHIP="1,0.5"` and `LIFE=0.25`, 2452 turns: 25 direct exits 60 → 248 and 8 after a pause (239 → 248), spread
among 21 villagers until the end (before, from turn 2333 only one came out). Land2,
`OPENBLACK_TEST_HUNT_VILLAGER="0,3"` (turn 3, before the script creates the sea villagers, so the first one in the
registry is on land): 85 → 17 DOWNED → 18 BEING_EATEN → eaten.

Fire and teleport through the core (2026-10-01): Land1, `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 650
turns: the same states as before (85/1 → 215 → 220 ⇄ 216, 163/1 → 219) and one entry and one exit per change (3213
`EnterPutOutFire(216, 220) = 1` for 3213 changes 216 → 220, 3221 for 220 → 216, 11 `(215, 220) = 1`), plus those of
replanning and arriving (22 `ExitPutOutFire(220, 220)` / `EnterPutOutFire(220, 220)`, 6 `ExitOnFire(219, 219)` /
`EnterOnFire(219, 219)`), no 0x2E nor 0x2F; before, those last ones were missing and ExitOnFire was never called. The
216 ⇄ 220 back-and-forth every turn was already there before. Teleport (`OPENBLACK_TEST_TELEPORT="1785,2655,1830,2660,7,walk"`,
`_TURN=300`): 1 ⇄ 201 as before and, in one of the passes, 201 → 202 → jump (saving 26 m) → `SetTopState 202 → 163 =
0x1`.

Milagros' review (2026-10-01, `_scratch\mapa\tele3.log`, `fire1.log`, `cycle2.log`): teleport as
above, 1365 turns: the 6 villagers that react make **one** walk to the stone (1 with FINAL 201, about 70 turns),
arrive (1 → 201 → 202), jump (saving 44-45 m) and `PopFromPrevious stored 209 -> resume 163`; the back-and-forth
1 ⇄ 201 is over. The villagers keep walking and arriving (208 walks finished, median 170 turns; none in MOVE_TO_POS
without moving between two summaries), 18 unported cases of the PathfindingSystem remain in STEP_THROUGH. Fire
(`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 747 turns): the same states (85 → 215 → 220 ⇄ 216 → 163,
163 → 219), `ExitReaction` on each exit of 215..220 and 12 `StopReacting` when it goes out; no `pause 239` nor
`Stuck in an invalid state`. `OPENBLACK_TEST_MAP_CYCLE` over Land1-5 without hangs.

With `ReactionValidate` connected (V2 merge, 2026-10-01, `_scratch\mapa\p_fire.log`, `p_tele3.log`, with a
temporary trace in `VillagerCallValidate` that does not stay in the code): fire (`OPENBLACK_TEST_FIRE=
"1785.2,2652.6,450,abode,60"`, 785 turns) 12 villagers 85/1/114 → 215 → 220 ⇄ 216 and back to 163 with
`StopReacting` when it goes out; `ReactionValidate` runs every turn for the TOP (1080 times in 216, 54 in 220, 13 in 215)
and for the FINAL (410 in 220, 3 in 215), without any pop (the object, the home, is still available; when it goes out
`ExitPutOutFire` → 163 does `StopReacting` before a reaction state without an object is validated). Teleport
(`OPENBLACK_TEST_TELEPORT="1715,2595,1760,2640,7,walk"`, `_TURN=550`): 1 → 201 → (20 turns walking, FINAL 201
validated every turn) → 202 → jump of 63.6 m → 163 and `PopFromPrevious stored 245 -> resume 163`. **Beware of the
`walk` hook**: since V2 its walk with FINAL 163 does not admit reactions (`IsAvailableForReaction` 0x763390: the +0xEC
of 163 is 0), so the hook's villager does not react; the test was done with FINAL 245 changed by hand in
`TeleportDebugHooks.cpp` (not saved). `OPENBLACK_TEST_MAP_CYCLE` over Land1-5, Greek God, TwoGods and Kapa's Land1
without hangs.

## Tests

`test/test_villager_core.cpp` (fake table in the Locator and scripted rolls with `villager::SetRandForTests`):
food with one and two rolls, lastCheckTurn, counter and water rule, random order of the constructor, 85, 0x2E by TOP and
by FINAL, 0x2F (→ 163), 0x23, setting TOP clears FINAL with the town, the PREVIOUS rule, AdjustTownModifier, the
pause (with and without poison, without a roll in 239 or without the mark), 239 → FINAL, check every 9 turns, wear,
EXHAUSTION / CHANT (also with `WorshipVillager::atSite`), hurt → 36 (switched on since V2; switched off in one case; 19
with food, knocked down), SetupMoveToWithHug with `moveState` keeps FINAL (and 0x2F without a walk), POWER.

`test/test_villager_food.cpp` (V4): the hunger batch (0.79919), the strict damage, the interruptions (0xD0 / 0xD4,
the disciple, STARVING / CHANT), the amounts from `v4calc.py` (74, 65, 63, 54, 83, 85), ChangeStateToFindFoodToEat
(117 / 118 / 36 / 33 / what it carries / 0), EatFoodHeld (1.0 and 0.9864865, NaN → 0), the double take of
GetFoodFromHome, 117 / 118 / 212 and 34. `test/test_villager_home.cpp`: 37, ExitAtHome with PresentAtHome,
HomeDecideWhatToDo (0.7875, GameRand(4)), pregnancy and child, sleeping (6 cycles from 0.4 to 0.70000005),
CheckWhenGoingToBed once per stay, the tent (tree, the other side, full → (−20 m, +10 m)), DoGoingHome without a home,
the score and FindAbodeWithSpaceInTown, the home's list, CheckNeedNewAbode → 129 → 36, 130, 238, 234, the Shuffle and
Abode::Process (1001 turns). `test/test_villager_age.cpp`: the pure layer (63 years, r³), SetScaleForAge with scripted
GameFloatRand, the 13-year-old child (18 years, counts, 234), old age and WomanSpecial. In `test_villager_decide.cpp`
two V2 cases change (a hungry villager eats immediately, 117; one without a home or town goes to 130).

## Assumptions (inferred / approximate)

1. **(approximate)** "Dancing" (Living +0xD8, the DanceGroup) is approximated with `WorshipVillager::dancing` (set by
   AddDancer / FindDanceGroup and removed by ExitAtWorshipSite / RemoveVillagerFromWorshipSite) or `TOP == IN_DANCE`
   (VillagerSpeed.cpp).
2. GRand: `villager::GameRand/GameFloatRand` forward to `game_random` (LHRand over the synchronised seed,
   engine-math.md «Random numbers»). **(approximate)** the sequence is not that of an original game: other
   systems that draw from the same stream (animals, trees…) still use openblack's generator or are not ported.
3. **(approximate)** The archetype sets the speed of 85 CREATED when creating the villager; in the original the first
   SetTopState sets it. It is not visible: CREATED does not walk.
4. **(approximate)** UniqueId (UniqueKeyHeap::GetUniqueIdFromAddress 0x7E19A0) is the creation index (`object_index`,
   +0x3C): it decides in which periodic check old age is looked at, and the SetSpeed factor.
5. **(inferred)** +0x11C: the type of the union comes from bw1-decomp; it is not ported until a job reads it.
6. The game turn is `Game::GetTurn` (g_game +0x205A40); without Game (tests) it is 0 or that of `SetTurnForTests`.
7. **(approximate)** Turn order: villagers and animals in two passes, everyone's movement before the logic, and
   the villagers in registry order (not that of the list g_game +0x205BBC).
8. **(approximate until V12)** VillagerDead kills at the end of the turn and the state function no longer runs after
   death; the player of VillagerDead (GetPlayer, vt +0x1C) is not passed (NEUTRAL).
9. **(approximate)** The TOP changes of the hand, physics, animals, LANDED and the Gui do not go through exits or
   entries (their Enter/Exit of the original are not ported). Those of fire and teleport now do (core).
10. (V2: the invented idle walk has been removed.) A walk with FINAL 0 (only debugging tools) returns to 163
    via the compatibility path (the original would do SetTopState(0)). With a FINAL other than 0 the arrival is the
    exact one (SetTopStateToFinal, 0x5EC28E).
11. **(approximate)** The exit of MOVE_TO_POS / MOVE_TO_OBJECT (ExitMoveToPos 0x5EDDA0: CircleHugInfo::Reset and
    +0x60 = 0) is not ported: it counts as 1 (what it returns) and warns once.
12. **(approximate)** AdjustTownModifier ignores a desire outside 0..16 (info.dat has none).
13. **(approximate)** A disciple type outside the g_DiscipleInfos table (13 rows) does not ignore needs.
14. **(approximate)** Without clip resources (the tests), IsReadyForNewAnimation gives "finished".
15. **(approximate)** Bit 0x2 of +0xE0 (at the worship site) is read from `flags` or from `WorshipVillager::atSite`
    (Milagros) until it moves to `flags`.
16. (V2: no longer an assumption.) The "hurt → 36 GO_HOME" rule of CheckEveryTime (0x7505C3) is switched on: 36 walks to
    the door. (V4: 37 ARRIVES_HOME is now ported.)
17. Neutral until their milestone (they do not invent behaviour): ProcessReaction (Milagros M-5), Town +0x5E8 (V3),
    SpecialVillager (V14), villager counter and skeleton (V12), DROWNING 16 (water). (V4: CheckHungry, CheckChildGrownUp,
    WomanSpecial and CheckDeathFromOldAge are now in.)
18. V2 neutrals (return 0 / do nothing, with TODO and address) (V4: CheckHomelessMoveIntoAbode,
    ChangeStateToFindFoodToEat, CheckWhenGoingToBed, CheckNeedNewAbode, the homeless branch of DoGoingHome and ExitAtHome
    are now in);
    (V3: TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 is now in, leaves 0 or 1 in eax, 0x7460EB / 0x7460F7);
    DiscipleDecideWhatToDo 0x751720, IsMotherAlive 0x757F40 (keeps the mother), ChildGotoCreche 0x7579F0, RemoveFromDance
    (V14); the homeless branch of DoGoingHome (tent 238 / 130, V4); Town +0xF1C (ProcessTownEmergency will write it,
    Milagros); ExitAtHome 0x761B40 (V4, counts as 1).
19. **(approximate, P-5)** The door (Game3DObject::GetDoorPosition 0x63AFE0 via LH3D vt +0x1C4, without symbols): the
    L3D's door point (`L3DMesh::GetDoorPos`) by rotation·scale of the home's Transform; without a door, the
    home's position (literal, 0x52E3A4).
20. **(inferred)** Abode IsAvailable (+0xA & 1) = the entity is valid; IsBuilt (+0x58 & 2, +0x5C ≥ 1) = yes for all
    homes (there are no construction sites until V6) **(approximate until V6)**; the home's GAbodeInfo is that of its
    number and mesh.
21. **(approximate)** CheckForClearArea: openblack has no per-cell object lists; the entities of each
    cell are taken (`effects::ObjectsInMapCell`) and their radius `Object2DRadius`: same set, different order (the
    result is yes / no). With the radius of openblack's homes (≈5.4 m) and the door at ≈1.5 m from the centre, many 245
    spots fall "occupied by its home" and FindClearArea moves them aside: the villagers sit somewhat further from the door
    than in the original (effect of 19).
22. **(approximate)** The town's list of homes (+0x754) goes from newest to oldest (AddStructureToTown
    0x7399C3..0x7399CF inserts at the head); openblack sorts it by creation index (+0x3C) from highest to lowest, as
    if each home entered its town when created. It decides the height of GetCongregationPos (that of the last one read:
    the oldest), the fallback base with one home and which homes remain in the ring with more than 100. The plans
    (+0x9A8) go in order of arrival (AddPlanned 0x73D08A..0x73D0AD appends at the end), like `plannedAbodes`.
23. **(approximate, P-9)** SetupMoveToOnFootpath 0x5EDD20: GFootpathLink::UseFootpathIfNecessary 0x5362E0 is not
    ported: always the direct walk (SetupMoveToWithHug), which is the literal behaviour without a footpath link.
24. **(approximate)** LookAtPos 0x5EC550: the angle Living +0x5C is `WallHug::yAngle` (radians, rounded to 2048ths).
    It takes two arguments (ret 8); with a mode other than 0 / 1 / 2 the step is the mode itself (0x5EC57A).
25. **(approximate)** 114: the "available" mother (vt +0x2C) = valid entity; IsNavigable (Collide & 2 and not & 8):
    openblack's Collide only knows land / water (bit 8 is never set).
26. **(approximate)** SET_TOWN_CONGREGATION_POS: openblack's script parser (Script.cpp GetParameter) only makes a
    vector from a two-field string (and its y is the terrain height, not a third field): the cache height is 0,
    the literal behaviour for two fields. A three-field string (MapCoords::Set 0x6032E4 would read it unscaled) does not
    reach the command in openblack. None of the game's scripts uses three fields.

No longer assumptions: the writer of +0x24 & 0x400 (controlled by script) is GameThingWithPos::SetControlledByScript
0x402240 (`ecs::script_held`; also the vortex, fn_005FE3B0 0x5FE474); the walking state of SetupMoveToWithHug is
GLivingInfo +0x124 (`moveState`).
