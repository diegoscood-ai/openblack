# Plans and needs

## Summary

A creature acts through plans: its mind weighs a plan for each activated desire (what to do, what to do it to, what
with), adopts the best one when it is clearly better than the current one, and carries it out step by step. A plan
carried out to its end counts towards satisfying its desire. Between plans a creature does nothing of its own. Its
body has needs that the plans answer: sleep and rest, thirst and drinking, hunger and eating; Compassion is special,
since it helps a town with the town's own desires.

openblack's planner multiplies the plan's priority in the original's order, keeps plans across passes, runs the
completion of a plan carried out to its end, and has the original's sleep, sit, thirst, faint and drink rules and the
eating rules as pure functions, most of them wired. What still differs: the goals and things come from the world rather
than from the creature's beliefs, some forced activities of a creature with no plan are missing, and storage pits,
fields and trees are not yet food targets.

## The original

### Compassion and towns

Compassion (desire 1) is the only desire whose actions are not its own row of the desire action table: it helps a
town with one of the town's desires, with the actions of that desire's row in the compassion-for-town table [V].

#### The tables

[V] `load_variables` 0x42D13D..0x42D180 -> fn_00432980 loads rows of 0x88 bytes (GBaseInfo 0x10 + 30 dwords):
`DETAIL_CREATURE_DESIRE_ACTION_TABLE`, 40 rows at 0xC66950 (+0x10 = 0xC66960); the compassion-for-town table, 17 rows
at 0xC65778 (+0x10 = 0xC65788); the compassion-for-creature table, 40 rows at 0xC64230, which is never read at run
time (only the loader and its static constructor fn_00491A50). The town table, by `TOWN_DESIRE_INFO`: 0 Food 28, 29,
48, 31, 285, 305, 325; 1 Wood 49, 32, 104, 326, 327, 247, 248, 304, 49; 3 Protection 286 CastShieldAroundTown; 4
Mercy 38 Stroke, 101 CastHealSpell, 102, 229, 233; 5 Abodes 247, 248, 326, 327; 8 Children 287 MakeBreederDisciple; 9
To build as Wood; 10 Rain 239, 326, 327, 325; 12 Repair 35 HelpRepairHouse, 101, 102, 229, 326, 327; 15 Relaxation
288, 309, 299, 295, 294; 16 Sleep 289; the others empty. The desire action table's row 1 is read for no desire.

#### The action for a plan

**`CreatureMental::GetNthActionForPlan(plan, n)`** 0x4F17F0 [V]: a desire other than 1 returns its row's slot n
(0xC66960 + (34 × desire + n) × 4). For desire 1 with no goal belief: 0. With a town as the goal:

1. **Heal first** (0x4F184A..0x4F1921): the town's injured count (+0x714, signed: `jle`) above 0; the creature's
   leash (`GetInterfaceStatusLeashOn` 0x4CF060) not tied to a `TownDesireFlags`; Heal's sightings (mind +0x17D60) /
   `NumTimesMagicActionMustBeSeenBeforeItIsLearnt(10)` **strictly above 0.6** (0x8C7BDC; 0.6 fails, NaN fails, x/0
   with x > 0 passes); n == 0; then one `GameRand(2)` (0x4F1909) == 0 returns 0x65 CastHealSpell.
2. Otherwise the town's desires to help (fn_004F2070, below); with a non-empty list that does not hold the desire it
   helps (mind +0x20D1C), that becomes list[0], its index (+0x20D20) 0 and the remembered value (+0x20D28) its raw
   desire × 0.85 (0x8CEFAC). An empty list changes nothing.
3. The desire d it helps, outside [0, 17) -> 0; else slot n of its town row (0xC65788 + (34 × d + n) × 4); a non-zero
   action also sets 0xCADA94 = d, which fn_004F1C40 copies to 0xBDF608, which nothing reads.

With a creature as the goal, `FindDominantDesireWhichCanBeHelped` 0x4DC680 runs and its result is dropped: 0. The one
`GameRand(2)` is the only randomness.

#### The town's desires, the goal and the members

- **The town's desires to help**, fn_004F2070 [V]: the town's raw order (Town +0x378, 17 entries of 12 bytes): an
  entry whose town row has no first action is skipped; the sum raw (+0x168) + boost (+0xD4) + boostA (+0x90), in that
  order, equal to 0 or NaN ends the walk; the rest are listed (up to 17).
- **The goal**, fn_004F0620 -> fn_004F08D0 [V]: over the mind's beliefs, not the creature itself, those that are
  suitable (`IsSuitableForCreatureActivity` vt +0x1DC) and that compassion applies to (vt +0x1C4
  `IsActivityObjectWhichCompassionAppliesTo`: 1 only for Town 0x739230 and Creature 0x474020), scored by
  `CalculateDistancePriority(1, belief)` 0x4F1EF0 × `GetActivityObjectUsefulness(belief, 1)` 0x4E9900 (0 for a town
  with no member), the best strictly above 0, the first of equals in the belief list. No goal, no plan.
- **Moving on while planning** (0x4F06A3..0x4F07E4) [V]: compassion planned, the current plan's desire (mind +0xF50)
  not compassion, no activated desire above it (`GetRankPositionOfDesire(1)` 0x4DC760 == 0), the goal a town, +0x20D24
  set, and more than 30 turns since +0x21BC (unsigned): +0x21BC = the turn, +0x20D2C = 0, the index moves on; past the
  end of the list, nothing; else that desire, remembered at raw × 0.85; an empty list leaves -1 and 0. No draw.
- **Taking up a plan**, fn_004F0810 [V]: a valid compassion plan whose goal is a town sets +0x21BC = the turn
  (0x4F083E..0x4F0872).
- **The act-on object**, `CreatureBeliefAboutTown::FindBestObjectToActOn` 0x4DAD10 [V]: over the town belief's members
  (`AddBeliefAboutAssociatedObject` 0x4D8BB0: its abodes and villagers not being deleted, its fields, its totem
  statue), those `IsObjectSuitableForDesire(member, 1)` 0x4E3730 and the action's filter (fn_004E3800, row +0x34) take,
  the strictly highest `GetObjectToActOnUsefulness(member, 1)` 0x4E9A70: the distance priority from the creature, with
  no 200 m limit, × the usefulness; 0 when the creature is kept within an area and the member lies beyond its radius.
  A creature's belief (`CreatureBeliefAboutCreature` 0x4DAE70) returns itself, but its action list is empty, so it
  never acts.
- **The owner belief** [V]: `AddBeliefAboutAssociatedObject` stores the owner at belief +0x14 and adds the belief to
  the owner's member list +0x18, counted at **+0x1C** (0x4D8D6A..0x4D8DA2); a new belief gets its owner at once
  (`CreatureBeliefList::AddBeliefAboutObject` 0x4D7D44). "Being deleted" is the thing's +0xA bit 0, set by
  `GameThing::ToBeDeleted` 0x56FB70 (bit 1 is `ProcessDead`'s), so a town, flock, forest or statue's town that is being
  deleted gives no owner. The forest owner is fn_0053A1A0(position, 1000.0, 0): over the list at g_game +0x205BB4 (next
  +0x44) [L: the forests], the one with trees (+0x54 + +0x4C ≠ 0) 📼 1.20 only nearest by `GetDistanceInMetres`, strictly
  nearer than the best so far, so of equals the first in list order wins. fn_004637E0(citadel, thing) is true when the
  thing is in one of the citadel's three part lists (+0x4C next +0x84, +0x54 next +0x8C, +0x64 next +0x84).
- **An owner with no known member is useless** [V]: for a desire whose initial desire info has the flag at +0x30 (the
  file row's +0x20 of `CreatureInitialDesireInfo`, rows of 0x1B0 from file 0x64BA4: 1 for Impress, Compassion, Anger,
  BeFriends, ObeyCreature, TellCreatureWhatYouThink, EducateFriend and MissFriend), `GetActivityObjectUsefulness` gives
  0 when the belief's member count +0x1C is 0 and its type is not 8, a creature (0x4E9912..0x4E9944). With the same
  flag `CreaturePlan::IsValid` 0x4F12E0 needs an object (+0xC).

#### The draws, finishing and the state

- **The draws** [V]: one `GameRand(2)` per `GetNth(plan, 0)` while the heal conditions hold: once in fn_004F1C40, once
  more for each action that reaches fn_004F1A10 (which looks up the action's slot from slot 0), all of it again on each
  retry of the act-on search (up to 30), and in `StopWhatIAmDoing` (0x475A17) and
  `FindSomethingToDoOnObjectUsingObject` (0x4C542F, 0x4C5497).
- **Finished helping**, fn_00475450 [V] (from `ProcessSubAction` 0x4FEE5E, 0x4FEF44, 0x4FF181, before
  `FinishActionGeneric(1, 1)`): the current plan compassion's, +0x20D24 set, the goal a town, the desire in [0, 17):
  +0x21BC = the turn; raw desire **above** the remembered value (NaN fails) -> `GameRand(2)` (0x475559) r, and while
  kept ≤ r + 2 and the desire is not 15 (Relaxation), ++kept and it stays; else kept = 0, the index moves on (wrapping
  to 0), raw × 0.85, or -1 and 0 for an empty list. A forced compassion plan on a town reaches it too.
- **The state** [V]: the `CreatureMental` constructor (0x4D24A4..0x4D24CF) sets +0x20D24 = 1, +0x20D1C = -1, +0x20D20,
  +0x20D28, +0x20D2C = 0. A leash tied to a `TownDesireFlags` (fn_005E6BD0, 0x5E6D30..0x5E6D94) sets +0x20D1C = the
  flag's desire (+0x5C) and +0x20D24 = 0 when its row has an action, else -1 and 1; untying (fn_005E7140) -1 and 1.
- **A forced plan that fails to build** [V]: `ForceActivityAndForceAction(plan)` 0x4C44B0 first calls `StopReacting`
  (vt +0x998) when a reaction is held and `FinishActionUnsuccessfully` 0x475730 (which already sets the plan's desire
  mind +0xF50 to 40, none, and stops the agenda, the cast and the reaction), and only then copies and builds the plan
  ([The fizzle shown](casting.md#the-fizzle-shown) has the order). `ConstructSubActionsFromActionAgenda` 0x4D36A0
  returns 0 on success; it also fails "outside area" for a creature kept within an area (+0x11B8 == 0 and agenda
  +0x14C4, with its own `FinishActionUnsuccessfully`). On a failure the caller does nothing more: the copied plan stays
  in mind +0xF48 unless the construct itself called `FinishActionUnsuccessfully` (StartFire 0x4A41D0, a construct with
  no instrument, "outside area"), which sets the desire to 40.

### The plan's priority

How `CreatureMental::DecideWhatToDo` 0x4F0560 plans a desire and weighs its plan [V]. The game thread's FPU runs at
24 bits (fn_007DEE00, see [The game's FPU](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)), so every
product below rounds to a float.

- **The pass**: two goals a call ([0xBE026C] = 2; `Living::ProcessLiving` 0x5EC810 sets it, 1 with three or more
  creatures) from `SetupRemainingGoalsToPlanFor` 0x4F1480 (desires 39 down to 0 that are activated with a suppression
  counter of 0). In the call whose loop empties the list (or that starts with it empty): the best plan fn_004F1410, the
  switch fn_004F0500, and the list rebuilt in that call when nothing is taken.
- **One desire**, fn_004F0620: the per-desire table 0xC87850 + 0x28 d (filled at 0x4DB5F0). Entry +0x00 is a gate;
  when it says no, only the plan's priority (+0x2C) is set to 0 (0x4F065A). The gates (none writes or draws):
  Wanderlust 0x4DF090 no when the development phase (+0x1268, signed) > 5 and exhaustion (physical +0x30) >= 0.8;
  ToAttractPlayersAttention 0x4DF0C0 yes when ftol((1000 / msPerTurn) x 120.0) < the turns since the player last looked
  at it (mental +0x18C4C, unsigned); ToGetWarmer 0x4DF110 yes when warmth (physical +0x10) <= -0.3 or NaN; ToGetColder
  0x4DF130 yes when warmth >= 0.3; ToRunAwayFromPlayer 0x4DF150 yes when the attitude to the player (mental +0x18C58)
  < 0 or NaN; ToRest 0x4DF180 always; ToFollowPlayer 0x4DF190 yes when the last detected player action (mental
  +0x1A9E4) is not 46 (none); ToHangAroundAtHome 0x4DF1C0 yes when GetDistanceInMetres(position, home +0x1200) < 60 or
  NaN.
- **The goal**, fn_004F08D0, only for the desires whose initial desire info has +0x30 (the file row's +0x20) set:
  Impress, Compassion, Anger, BeFriends, ObeyCreature, TellCreatureWhatYouThink, EducateFriend, MissFriend. Over the
  mind's beliefs: `CalculateDistancePriority` 0x4F1EF0 x `GetActivityObjectUsefulness`, the best strictly above 0 (the
  first of equals); the product is the plan's goal term +0x1C. No goal: priority 0 (IsValid 0x4F12E0 fails).
- **`CalculateDistancePriority`**: from the creature (or the leashed object, `GetInterfaceStatusLeashOn` +0x24) to the
  belief's position, d = GetDistanceInMetres; held 1.6 (info +0x2C8); d > 200 (info +0x2C4) gives exactly 1 - w (200
  and 1.6 in every one of the 17 rows of info.dat [V]); else (NaN included) 1 - (w x d) / 200, w = the desire's
  distance weight (initial desire info +0x68, the file's +0x58).
- **The action and what it is done to**, by the table entry +0x20:
  - 0, the goal path (the flagged desires but Anger): per action, fn_004F1A10 keeps the best inst x u x (prio +
    novelty) on >= (the later of equals), u = the goal's usefulness to act on for a creature goal, else 0.1; then the
    goal belief's `FindBestObjectToActOn` finds the thing (p20 = 0.1), or the action is passed over (up to 30 tries).
  - 1 with a goal, Anger: the thing first, from the goal, then one action pick; p20 = the thing's
    `GetObjectToActOnUsefulness`.
  - 1 without a goal, every other desire (fn_004F0D60): the goal term is 0.1. The action first: GetNth 0..29, its row
    predicate, `KnowsAction` x3, not passed over; prio + novelty kept on >= (the later of equals; NaN never). Then
    fn_004F10A0: an action done to something takes the best of both belief lists by `GetObjectToActOnUsefulness` alone
    (no distance), strictly above 0, the first of equals, and p20 = that; else the creature's own belief and p20 = 0.1.
    Nothing to do it to: the action is passed over, up to 30 tries.

  A plan always has a thing it is done to (IsValid).
- **The priority**, fn_004F1D50: `((((min(prio, 0.01) x p20) x float(desire x goal)) x 100000) x B)`, B = the
  instrument's usefulness or 0.1; fn_004F1E10 is `p < 0.01 ? p : 0.01` (a cap). The desire is CreatureDesires +0x148[d].
  The instrument [V]: `FindBestActionForPlan` 0x4F0A70 calls `FindBestObjectToUseIfNecessary` 0x4F11B0 per action,
  which searches only when the action row's +0x44 flag is set ([the list](mind.md#the-players-feedback)):
  `FindBestObjectToUseForAction` 0x4F1E30 walks the mind's beliefs (+0x222C, next +8) but the target, keeps those the
  row's +0x38 / +0x3C filters take (fn_004E3820), scores `CalculateDistancePriority(desire, belief)` ×
  `GetUsefulnessOfUsingObject(belief, action)` clamped to [0, 1], and the strictly highest above 0 becomes plan +0x14
  with its usefulness at +0x24; none found, the action scores 0. Without the flag +0x14 = 0 and +0x24 = 0.1
  (0x8D152C).
- **The best plan**, fn_004F1410: all 40 plans by +0x2C alone, strictly above the best from 0, the first of equals (a
  NaN is taken, and every later plan then replaces it). Plans are not cleared between passes: only SuppressDesire
  0x4DC260 (when the counter grows), `RemoveBeliefsAboutDeletedObjects` 0x4D8451 and fn_004F0620 itself write them.
- **The switch**, fn_004F0500: (best - current) > current, strict and ordered, then IsValid; adopting copies the plan,
  priority included, into the current plan (+0xF74).

#### The current plan's priority

Every writer of +0xF74 that runs in 1.20 📼 1.20 only:

- the adoption's copy (`ConstructCurrentPlanFromPlan` 0x4F1560, also from ForceActOnObject, ForceMeToActOnObject,
  0x4C43A0, 0x4CEC00);
- `ForceActivityAndForceAction` 0x4C4450 at FLT_MAX (the walk back or a walk the creature is sent on, 0x4C7220;
  ForceActOnObject; packet 0x5E; the reactions; the scripts);
- 0 when the plan is invalid or its agenda is done (ProcessState 0x473774), by `Agenda::Clear` 0x4D3785 after every
  FinishActionUnsuccessfully / FinishActionGeneric, in MoveToDevelopmentPhase, ForceActOnObject and Look (the held
  object gone), and by the feedback code (0x4E0911 when a stroke rewards running away from the player, 0x4E0C02 when a
  slap finds a valid plan);
- the player feedback 0x4E06A0 first finishes what the creature does ("player feedback"), and a slight one with
  Creature +0x1064 set [?] forces {IdleAroundWithPlayer, LookAtCamera, itself} at 1.0
  ([The player's feedback](mind.md#the-players-feedback));
- {ObeyPlayer, 314 LookAtCameraInWideScreen, itself} at 1.0 under a wide-screen camera flag (0x4736F9);
- `ScriptSetAgendaPriority` 0x4F70F0 the script's value, whatever state the plan is in.

**Dead in 1.20** (nothing calls, jumps to or points at them): 0x4CE8E0 with its 0.1 at 0x4CEAC3 (the leash tug
packet 0x57, whose only sender 0x4CE750 is unreferenced) and its Clears 0x4CECF0 / 0x4CEE80; 0x479090 and 0x47C780,
which force {ObeyPlayer, 90 WaveAtPlayer, itself} at 1.0.

The mind file's increase times are read unchecked (fn_004E84A0).

### A plan carried out to its end

What the original does when a plan is completed, `Creature::UpdateOnCompletion` 0x475D30 [V]. It is called from the
agenda's step fn_004FF570 (0x4FF590, from `ProcessSubAction` 0x4FEE53 and 0x4FF176) when the step's stage counter
(agenda +0x10) reaches 3 on the plan's last sub-action (index +0xC == count +0x14 − 1). So it runs **once for each plan
carried out to its end**, never for an effect inside the plan (eating, drinking, a poo), and not for a plan given up
(`FinishActionUnsuccessfully` and `StopWhatIAmDoing` do not call it). A second call at 0x507470 sits in a
protected-code block (the `call 0x401879` stub pattern) and is not followed [?].

The state it uses, in `CreatureDesires` (mind +0x8, 40 desires, each array 0xA0 bytes; `Initialise` 0x4DC100): +0x508
the completions needed (float: the initial desire info's memory +0x78, the file's `field0x68`; feedback moves it by
`field0x68` × 1/3 × feedback when the row's `field0x6c` is set, then to 1 below 1, NaN included, else to 8 above 8,
`ModifyDesireAfterFeedback` 0x4DCBE1..0x4DCC32; nothing else writes it and the mind file does not carry it); +0x5A8 the
completions counted (unsigned, 0 at `Initialise`, read and written only by 0x4DCD50, not in the mind file); +0x650 "the
action achieved its aim", one int for all desires, 0 at `Initialise` and in `StopWhatIAmDoing` 0x475BB0.

In order:

1. `CreaturePhysical::UpdateAttributesAfterPerformingAction` 0x4EFB60 with the plan's action (mind +0xF60), always:
   the body's cost of the action.
2. The gate (0x475D54..0x475D79): steps 3 to 5 are skipped when mind +0x1C14 is set and mind +0x1C18 is not, or when
   the plan (mind +0xF48) is not `IsValid` 0x4F12E0. Mind +0x1C14 is the forced plan (set by
   `CommandActivityAndAction`, the leash, `ForceMeToActOnObject`, `ForceActOnObject`, `ForceActOnMapCoord`, `Faint`,
   `ReceivedFightImpact`, the teleport, the after-feedback reactions and others; cleared by `StopWhatIAmDoing`,
   `FinishActionUnsuccessfully` and others). Mind +0x1C18 is set only at 0x654E1E, with +0x1C14, by the player-action
   handler 0x654D10, ForceCreatureToPerformAction (in the player-action table at 0xC00090; its row's name at 0xC00050,
   copied from 0xC00838) [V], which forces ObeyPlayer on a believed object. So a forced plan counts only when it is that
   player action.
3. d = the plan's desire (mind +0xF50). `UpdateAfterActionCompletionToSeeIfUrgeIsSatisfied(d)` 0x4DCD50: count[d] + 1;
   when count[d] >= `__ftol(needed[d])` (unsigned compare, the float truncated) the count goes back to 0 and the urge
   is satisfied. Only then:
   - `ClearSourcesAfterSatisfyingDesire(d)` 0x4DE7C0: each source of d whose type's row of the source table has its
     flag (memory +0x14, the file's `field0x4`) gets value 0 (+0x0; the threshold +0x4 and the drive +0x8 stay);
   - `UpdateDesiresAfterPerformingAction(action, creature info)` 0x4DC510: dr = the **action row's** desire (file
     +0x98, not the plan's); only if `IsActivated(dr)` (0x4DCDA0: activated, or the game flag 0x2000 at g_game +0x14)
     **and** (the row's file +0xC8 is set **or** +0x650 is set): value[dr] × the row's multiplier (file +0xC0), then
     below the species' `desireFloor` (memory +0x274, file +0x264; a NaN too) the floor, else above the desire's most
     (+0x468) the most. The floor is tested first.
4. By the plan's desire d, satisfied or not (0x475DC1; byte table 0x475F48, jump table 0x475F28):

   | d | desire | what |
   |---|---|---|
   | 7 | Poo | `SuppressDesire(7, 60.0)` |
   | 8 | Tiredness | `MakeDesireLeastDominant(8, info, 1.3)` |
   | 9 | IdleWithPlayer | `SuppressDesire(9, 120.0)` |
   | 15 | RestoreHealth | `SetLife(GetLife() + 0.5)` ([0x8AA3B4] read at 0x475E5C), no clamp |
   | 17 | AttractAttention | `SuppressDesire(17, 60.0)` |
   | 19 | GetWarmer | `SuppressDesire(19, 120.0)` |
   | 20 | GetColder | `SuppressDesire(20, 120.0)` |
   | other | | nothing |

   `Object::SetLife` 0x63A140 refuses only a life at or below 0.01 for some flags; vt +0x5B0 is that function in the
   Creature vtable.
5. When the row's file +0xF4 is set: `SetLife(1.0)`, then the temperature to the climate's at its position
   (0x475E9F..0x475ED3). Still inside the gate.
6. Outside the gate, for a valid plan (0x475ED8): mind +0x1CEA0 + 4 × action = 0 (the turns `StopWhatIAmDoing` adds
   for unfinished tries of that action, read by `GetActionPriorityModificationFromWhenLastDone` 0x4F0CA0), and mind
   +0x2200 = the row's file +0xCC (read only by 0x4E1E70 against the seconds spent on the plan; its meaning [?]).

+0x650 is set to 1 only by six sub-actions, when theirs achieved its aim: 9 `RunAway` (0x501ACF) and 59
`RunAwayFromPlayer` (0x501C6C) after a float compare, 17 `Stomp` (0x502C3C) when the stomped thing's life is 0 or less,
28 `Fight` (0x503A76) when fn_004792A0 returns non-zero, 37 `Repair` (0x504549) after a float compare, 41 `PutOutFire`
(0x504636) when the thing is no longer on fire. What the float compares measure and what fn_004792A0 is [?].

#### The data

`Scripts\info.dat` [V]:

- **The completions needed** (`field0x68`): 2 for Impress, Compassion, Anger, Play and ManifestState; 3 for Illness,
  ObeyCreature, TellPlayer and Steal; 8 for BringStuffHome; 1 for every other desire. Learnt by feedback
  (`field0x6c` = 1): desires 0, 1, 2, 3, 9, 12, 13, 16, 17, 18, 19, 20, 21, 22.
- **The action rows' +0xC8** (the multiply only with +0x650) is 0 for 11 EatAlive, 12 EatAfterExamining, 13
  StompAndEat, 14 StoneAndEat, 15 Hurl, 16 RunAwayFromObject, 18 Stomp, 34 HelpBuildHouse, 35 HelpRepairHouse, 37
  PutOutFire, 156 RunAwayFromPos, 158 EatFromFoodPile, 197 HurlObjectInHand, 266 BreakRock and 275..278 (Examine, Eat,
  Stroke, Throw ObjectInHand); 1 for every other row. No eating sub-action sets +0x650, so of the eating rows only
  EatFromStoragePit (65), EatFromContainer (66), EatFromTree (94), FishAndEat (155), BringFoodHome (164) and
  GoOutAndLookForFood (166) multiply Hunger at their end.
- **The action rows' +0xF4** is set for 9 Die, 17 SleepAtHome, 58 HealHimself, 59 RestToGetBetter, 170 RunHome, 179
  GoHomeToRecover and 313 SleepOnTheSpot.
- Rows with a desire of 40 or more (the gestures 143..154, 216 CastTeleportAndUseIt, 217 DiePermanently) would index
  the arrays out of the 40 in step 3; whether a completed, unforced plan can have one [?].
- **The source table's flag** (`field0x4`) is set for every source but 20 (curiosity), 28 (wanderlust), 44 (rest), 48
  (sadness) and 49 (going home).
- `desireFloor` is 0.0 in all 17 species rows.

### A creature with no plan

What `Creature::ProcessState` 0x472DC0 (vt+0x620, called once a turn per living thing by `Living::ProcessLiving`
0x5EC810) does on a turn its mind has no plan [V]:

- With no plan, the block at 0x47376E sets the current priority (mind +0xF74) to 0, counts the turn at mind +0x21FC,
  clears the leash control (Creature +0x384 and +0x380) and jumps to 0x4737B5, past the agenda step fn_004D37F0 and
  `ProcessSubAction` 0x4FEDA0: no sub-action and no body action is started. The one exception: with the thinking block
  bypassed ([0xD00DDC] == 0, +0x25 bit 4, or mind +0x20D30) and mind +0x20CE4 set, `ProcessSubAction` still runs
  (0x4737A0).
- The rest of the turn runs as with a plan: spells, leash, look, desires, fn_004F0370 (when Creature +0x10E8 is 0;
  with no plan it clears +0x1C14 and +0x1C18, stops a reaction through `Living::StopReacting` vt +0x998, and runs
  `DecideWhatToDo` or the kept next action) and the pose (`UpdateTime` 0x481DF0).
- The body keeps the `LH3DCreature` state it had 📼 1.20 only; state 0's handler fn_0048D250 holds animation 0 at the fraction
  +0x4988 with no draw. `UpdateLook` 0x482D90 draws nothing; `UpdateTime`'s one draw (0x482028) is on the local stream
  (+0x205A3C), a blink timer, `LocalRand(5000)` 📼 1.20 only. No default activity or idle-animation picker was found in
  `Creature` or `LH3DCreature` [?].
- On such a turn the synchronised stream is drawn only when the creature is mimicking (`ProcessMimicState` 0x4EA06E) or
  the goal planned in this call is Compassion with a town and a learnt heal (the `GameRand(2)` at 0x4F1909).

#### What can start an activity with no plan

[V] The planner's adoption as its pass ends; a cinema's forced plan (desire 24, action 0x13A, when the help system's
wide-screen flags are on, `HelpSystem::SetWideScreen` 0x5C6B17); fn_0047CFB0 (on fire, desire 15 with action 0xF0 when
heal-self is learnt, else 0x37; a learnt heal, desire 1 action 0xEE on the nearest town within 150 m; life below 0.4,
`SetCheatDominantDesire(15, 10.0)`); fainting fn_0047CF00; a totem call queued by `UpdateLeash` (the member 0x50A760 at
mind +0x21D0); queued gesture commands fn_004C74E0; the walk back into the confined area; mimicry.

#### The ProcessState switches

The five switches ProcessState tests ([0xD00DD8] the look, [0xD00DDC] the mind block, [0xD00DE0] the desire
evaluators, [0xD00DE4] the body, [0xD00DE8] the position block and the checksum) and [0xD00DEC] are not free
variables: they lie in `GGlobal::Global` (0xCD3B20, size 0x2D500, one object up to 0xD0143C), and its constructor
`GGlobal::GGlobal` 0x590F00 sets them through `[esi + offset]` with esi = &Global 📼 1.20 only: `mov eax, 1` (0x590F1D), then
+0x2D2B4 .. +0x2D2D0 and +0x2D2DC = 1, i.e. [0xD00DD4], [0xD00DD8], [0xD00DDC], [0xD00DE0], [0xD00DE4], [0xD00DE8],
[0xD00DEC] (0x590F48), [0xD00DF0] and [0xD00DFC] = 1; and +0x2D2AC, +0x2D2D4, +0x2D2D8, +0x2D2E0, +0x2D2E4, i.e.
[0xD00DCC], [0xD00DF4], [0xD00DF8], [0xD00E00] and [0xD00E04] = 0 (0x590F5A..0x590F72). The constructor runs once at
the C runtime's start-up, before the game's `main` (`fn_0054A6E0` = `mov ecx, 0xCD3B20; jmp 0x590F00`, from
`crt_xc_fn_Game_0054A6D0`, entry 0x9C6940 of the C++ initialiser table). Scans for the absolute addresses miss it: the
displacement 0x2D2CC appears only at 0x590F4A, and 0xD00DEC itself only at its 11 reads. Besides it,
`Creature::CreateCreature` 0x474B87 writes [0xD00DDC] under the GatheringFlag. So in a single-player game they hold
these values for the whole run (a static read cannot exclude only a stray write): the gated code (the map position
following the body, the shown fatness, the checksum, the creation draws) runs. [0xD00DEC] is a constructor default, not
a cheat flag: it gates the desires' increase-time and source jitter, the personality and the previous actions'
`GameRand(120)` at creation ([Random streams](../engine/random.md#random-streams)).

#### Land 1's first turns

On Land 1 the creature is under script control from the turn after it is loaded: the land's script focuses it on the
camera (`GScript::SetFocus` 0x6F90B0 → `Creature::SetFocus` 0x4F6760, `PrepareCreatureForScriptedAction`(1) 0x4F6A90)
and releases it only with `RELEASE_FROM_SCRIPT` about 22 turns later, after `SET_CREATURE_DEV_STAGE(13)`
(`Creature::MoveToDevelopmentPhase` 0x4C59C0, 33 desires activated). Under script control the mind block is shut: no
desire update, no planning, only the scripted turn to face the camera, then the standing pose; no synchronised draw,
only the blink. Planning starts again after the release, two goals a call, so the first pick comes 17 calls later
(🔍 inferred from the turn numbers of the land's script; details in
[A creature under a script's control](script-control.md#a-creature-under-a-scripts-control); the mind block's table
and the bookkeeping that still runs each turn are on
[What runs while controlled](script-control.md#what-runs-while-controlled)).

### Asleep, sleeping and sitting

`phys` is creature +0x160, the mind creature +0x164, the species' row creature +0x28.

- **Which actions count as asleep.** The body goes by the action the mind carries out (mind +0xF60, a row of the
  creature action table in `info.dat`, the script's `CREATURE_ACTION` numbers), never by an animation. The energy
  drain gets +3 on its divisor when the action is 17 or 59 (0x4EF6D1..0x4EF6E1); the growth is multiplied by 3 when
  it is 17 or 78 (`CalculateHeightIncrease`, 0x4EFB35..0x4EFB45). In `info.dat` row 17 is `SleepAtHome`, 59
  `RestToGetBetter`, 78 `SleepByObject`; `SleepOnTheSpot`, the sleep openblack's creature does, is row 313, which
  counts as neither (read from the table's names, 256 bytes a row).
- **Sleeping** is the sleep sub-action 0x5086D0, one call a turn: life = clamp(life + info+0x23C, 0, 1) (`SetLife`),
  exhaustion phys+0x30 = clamp(exhaustion − info+0x240, 0, 1). It wakes (`fn_004848C0`) when the exhaustion is 0, it
  is not night (`IsVisualNight`) and the sub-action's turns (mind +0x1BDC) are more than 50; or when the exhaustion
  is below 0.1, those turns are at least `size(phys+0x6C) · info+0x374 · [0xD01A40] · 5` and more than 50
  (0x5087D4..0x508807). [0xD01A40] is `GGameInfo` +0x48 (`GGameInfo` at 0xD019F8): the day cycle's duration in
  seconds, written raw by `SetVisualTimeCycle` 0x557620 (0x557633), 1700 by default. The product is made in that
  order on the game thread's 24-bit FPU and stored as a float before the compare. With a sleep length of 0.1 a size-2
  creature sleeps 1700 turns of the default day.
- **Sitting** is the second stage 0x500AC0 of the sub-action `StaticAction`, which counts down the turns its start
  0x500A30 set (the seconds × (1000 / ms a turn)): when the sub-action's clip is 38, the sit (0x500AD7), exhaustion
  (`CreaturePhysical` +0x30, Creature +0x160) = clamp(exhaustion − 0.2 · info+0x240, 0, 1) on each of those turns
  (0x500AE4..0x500B24), from the turn after the start, while the sitting down still plays, to the turn the count runs
  out and `EndStaticAction` is called, that turn included. info+0x240 is the sleep's rest (the runtime CreatureInfo is
  the file's row 0x10 bytes on). Life does not change.
- **Where in the turn.** In `Creature::ProcessState` the body's time-passing step (0x473337) comes before the mind's
  part of the turn (`Look` 0x4733A9, the plan checks and `ConstructSubActionsFromActionAgenda` 0x4734F8,
  `UpdateDesires` 0x47364D, `ForceActivityAndForceAction` 0x4736F9) and before the sub-actions (`ProcessSubAction`
  0x4737B0): the body reads mind +0xF60 as it stands before the mind's part of the turn, and the sleep and sit
  sub-actions run after the body, in the same turn, once a turn.

### Thirst, fainting and drinking

`phys` is creature +0x160, the mind creature +0x164, the species' row creature +0x28 (`info`).

- **Thirst** (`UpdateAttributesAsTimePasses`, 0x4EF7F4..0x4EF85D): from phase 3 (creature +0x1268), phys+0x34 =
  clamp(phys+0x34 + 1 / n, 0, 1), where n = `__ftol`((1000 / [0xD01A38]) · info+0x224): the turns a second are an
  unsigned integer `div` (0x4EF807), loaded with `fild` and multiplied by the float info+0x224 (0x4EF819), and the
  product is cut towards zero (`__ftol`, 0x4EF81F; 0x7A1400 takes `cvttsd2si eax` on a CPU with SSE2, `LH3DP3::HasSSE2`
  [0xE83A20], so a NaN or a product out of the int32 range gives INT32_MIN, 0x80000000; the x87 path without SSE2 would
  keep the low half of a 64-bit `fistp`); n is the low half `eax`, loaded back as an unsigned 64-bit integer (high half
  0, 0x4EF828..0x4EF82C) and divides 1 (`fdivr`, 0x4EF830). There is no test for n = 0: 1 / 0 is +infinity and the clamp
  makes the creature fully thirsty at once. Below 0 the result is set to 0, above 1 to 1. In `info.dat` every one of the
  17 species' rows has info+0x224 = 5000, so at 10 turns a second n = 50000 and the cut changes nothing there.
- **Fainting** has two gates. In `Creature::ProcessState`, after the mind's plan step, 0x47355B..0x473585 call
  fn_0047CE80 and then the faint test fn_0047CF00 only when `GetPlayer` (vt +0x1C) is not null, that player's type
  (`GPlayer` +0x8E0, `PLAYER_TYPE`) is not 2 (the computer) and the creature is not controlled by a script
  (`Flags` +0x24 & 0x400, 0x473576). fn_0047CF00 itself needs phase ≥ 5 (0x47CF03) and tests the script bit again
  (0x47CF10); then, in this order, life (`GetLife`, vt +0x11C) ≤ 0, energy phys+0x1C ≤ 0, exhaustion phys+0x30 ≥ 1
  each call fn_004C9FE0(0x1E, reason 3, 1 or 5, 0, 0, 0) and `Creature::Faint` 0x476FA0.
- **fn_0047CE80** 📼 1.20 only, called at 0x47357E under those gates (and Flags +0x25 & 4 clear): while +0x1118 is 0 it adds 1
  to Creature +0x111C (the creature's, not the mind's); when +0x111C is above ftol(15 × (1000 / ms a turn, whole))
  (unsigned) it resets it, sets mind +0x1C38 and mind +0x1C14 to 0 and suppresses desire 33 (ToFollowPlayer) for
  60 s (0x4DC260). +0x1118 is set at 0x473551 while the plan block holds a plan whose desire (mind +0xF50) is not 33,
  cleared by `StopWhatIAmDoing` 0x475C9C and zeroed at 0x474653. So a creature left following the player with nothing
  else to do gives it up after 15 s.
- **Drinking** is the sub-action 0x5052B0: phys+0x34 = 0, `SuppressDesire`(14, 20 s) on mind +8, and
  `Object::SetTemperature`(0, no source) 0x639A60 (0x5052DB): an existing fire on the creature takes the temperature 0;
  without one nothing is made, 0 not being hotter than it ([Fire](../world/fire.md#fire)).

### Eating: what a creature may eat, the portions and the end of a meal

The game thread's FPU rounds every step to a float
([The game's FPU](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)).

#### What a creature can eat

`CanBeEatenByCreature` (vt+0x22C), as its eating plans ask it, is called by the agenda's eating constructors
(`ConstructSubActionsForEatAlive` 0x49AA3F, `EatAfterExamining`, `StompAndEat`, `EatFromField`, `EatFromTree`,
`FishAndEat`, `EatFromFoodPile`, `BringFoodHome` and the plans that carry food to a pit, a worship site or home: 16
sites in all, and 5 room-drawing calls of another class's slot).

- `Object` 0x4E48C0 (villagers, animals, mobile objects, pots, abodes) is the pick-up test (vt+0x258) and nothing else.
- `MobileStatic` 0x4E48D0 is "not a toy" (vt+0x3B4: `MobileStatic::IsToy` 0x609020 is 1 for the meshes 0x18F..0x193
  of its info's `GetMesh` vt+0x2C, the toy ball, bowling ball, cuddly toy, dice and skittle; `Ball::IsToy` 0x4363B0 is
  always 1).
- `GameThingWithPos` 0x405340, `Creature` 0x474050 and `OneOffSpellSeed` give 0; `Field` 0x527FD0, `FishFarm`
  0x52C550, `StoragePit` 0x55CD10 and `PuzzleGrain` 0x561920 have their own.
- `GameThingWithPos::CanCreatureEatMe` 0x4E4B90 (vt+0x230: the not-food counts, the computer's own villagers, toys,
  then the pick-up test) is never called: no direct call and no call through vt+0x230. The not-food counts are written
  by the end of a meal (0x50068E) and by the mind file's reader; they gate no eating plan.

#### A portion

Sub-action 0x8A, phase 0 at 0x4FFED0: its amount is `__ftol(1000.0f × drawn size)` (0x4FFF22..0x4FFF68, the drawn size
is LH3DCreature +0x90; the float product, then the SSE2 cut: a NaN gives 0x80000000), no more than the source's
resource (`cmp; jb`, unsigned, 0x4FFF80); its scale is 3.75 × the drawn size (0x4FFFCA); it is made at 0x47AD80: the
difference from the creature to the source across the land, unless both its parts test equal to 0 (`fcom` and C3, so a
NaN part counts as 0; 0x47AE34..0x47AE54), is scaled to `(0.95 × pick-up range) / sqrt(z² + x²)`
(0x47AE56..0x47AE8A) and added to the creature's point: 0.95 of the range towards the source, however far it is. As
the hand takes hold, a source that is a pot or a pile loses what the portion holds (0x5000D9..0x500110; a field or a
tree would lose nothing); the game gives the portion the nearest town of its point (`GetNearestTown`, 0x4FFFAD).

#### The end of a meal

0x500540: a mushroom (vt+0x2A4; `IsMushroom` 0x4E4CE0 / 0x5276D0) or pile food (vt+0x480) is eaten; otherwise the food
value (vt+0x660, a float) compared with the double 5.0 (0x50063B, `test ah, 1`: below 5 or NaN) or a poisoned thing
(vt+0x4A4: a pot's flag `Pot` +0x74 bit 0, 0x55D4E0; the toadstool row, 0x6079E0; a poisoned living, 0x416F90) is spat
out (Hunger suppressed 20 s, the Puke activity), and only when not poisoned the type's not-food count goes up. Eaten,
the type's eaten count goes up and the Hunger desire's value (mind +0x160) becomes `hunger − f / (m × foodToEnergy)`,
m the real size (+0x6C) when below 0.8 or NaN, else 0.8 (0x5006D0), then below 0 or NaN → 0, above 1 → 1
(0x500708..0x500742). A mushroom starts the mushroom effect (mind +0x21D0 = 0x509530 when none runs); the creature's
eaten-by-class counters are 0x4794D0 and 0x479500, and a villager eaten adds to the player's condition (0x475F60,
where an eaten animal is deleted).

#### Piles and pits, fields and trees

- `MagicFood::IsAFoodPileOutsideStoragePit` 0x4E4A20 (the only class with it): true with no nearest town or no storage
  pit in it, else unless `1.1f × the pit's Get2DRadius` is above the distance in metres (`test ah, 0x41`: equal or NaN
  is outside).
- `StoragePit::IsStoragePitWithFoodInIt` 0x4E4D90: false when the pit's `Get2DRadius` is above 15 × the creature's
  drawn size (equal or NaN passes), else its food pile (+0xC4) must be there, with `[pile+0xA] & 1` clear and its
  amount (+0x70) not 0.
- `Field::IsFieldWithFoodInIt` 0x4E4930 is true when the field's growth (+0xD0) and its food (+0xDC) are both above 0
  (`fcomp` with 0.0, `test ah, 0x41`: equal, below or NaN fail; −0 fails, the smallest denormal passes).
- `Tree::IsTreeBigEnoughForCreature` 0x74C0A0 (also MagicTree's, vt+0x344) takes the tree's `GetHeight` (vt+0x42C, the
  generic `Object::GetHeight` 0x638120) against `15.0f × the drawn size` stored as a float (0x74C0AC..0x74C0BC),
  `test ah, 1`: below or NaN fail, equal passes.
- The oracle (the three tests run with `GetCreature3D` and the height or radius stubbed) gives: field (1, 1) 1, (0, 1)
  (1, 0) (−0, 1) (−1, 1) and a NaN either side 0, (denormal, denormal) and (inf, inf) 1; tree at size 1.1 (product
  0x41840000) 16.5 and 0x41840001 pass, 0x4183FFFF fails; size 0.37 (product 5.55f, 0x40B1999A) 0x40B1999A passes,
  0x40B19999 fails; height or size NaN fails; the pit at size 1.2 (product 18.0) radius 18.0 and 0x418FFFFF pass,
  0x41900001 fails, a NaN radius or size passes, the flag byte's other bits (0xFE) do not count, an amount of
  0x80000000 is food.

#### Eating from a food pile, a storage pit, a field or a tree

Read in the v1.42 exe; the same in bw1-decomp.

- `ConstructSubActionsForEatFromFoodPile` 0x4AE450: with something in its hand, the plan ends at once when it can eat
  what it holds (0x4AE46F) or deals with it first (sub-action 0x8A's hand check, 0x4FEF88); else it goes to the pile's
  point to within `(the pile's Get2DRadius + the creature's) × 1.2f` (0x4AE4E6..0x4AE503), turns to face it (the
  argument 0.1 to the turn, 0x4AE5C0, not read), picks up a portion and eats what it holds.
- `ConstructSubActionsForEatFromStoragePit` 0x4A2E10: no check of the hand; the pit's food pile
  (`StoragePit::GetResourcePile(FOOD, 0)` 0x7337D0) and a belief about it (`AddBeliefAboutObject` 0x4D7BD0), else
  `FinishActionUnsuccessfully("failed to construct subactions")` and 1; then MOVE_TO_POS (8) to the belief's point
  within `GetHeight() × 1.4f` (vt+0x42C, 0x4A2EFA..0x4A2F00, the constant 0x3FB33333), TURN_TO_FACE_OBJECT (6) the
  belief for 0.1f (0x3DCCCCCD, 0x4A2F94), CLEAR_OBJECT_TO_ACT_ON (0x6C, 0x4A3001), CREATE_PICK_UP_THEN_REMOVE (0x8A)
  the belief, pot row 0x11 (0x4A3033), and the main EAT_CREATED_OBJECT (0x80).
- `...EatFromField` 0x4A30E0: a carried thing that `CanBeEatenByCreature` (vt+0x22C, 0x4A30FF) returns 1 at once (no
  `FinishActionUnsuccessfully`); else MOVE_TO_POS the field's point within `GetHeight()` (0x4A318A, stored as is),
  TURN 0.1f, 0x6C, 0x8A the field with row 0x11, EAT.
- `...EatFromTree` 0x4A6140: the same hand check; MOVE_TO_POS within `(the tree's Get2DRadius + the creature's) ×
  1.2f` (vt+0x64 twice, 0x4A61DC..0x4A61F9), TURN 0.1f, 0x8A the tree with row 0xF (0x4A6320), EAT; no 0x6C.
- Every look and face function passed is NULL. Sub-action 0x6C's work (0x507C10): the mind's +0xF58 becomes
  `GetBeliefAboutObject(the creature itself)` (0x4D7B80, a lookup that adds nothing), and it returns 2.

### Sources

- The v1.42 `runblack.exe` through the xref index, the disassembly and the oracle (the field, tree and pit tests);
  bw1-decomp for names.
- `Scripts\info.dat` (the frozen copy): the desire, source, action and species tables.

## openblack

Status: the planner, the completion of a plan, the sleep, sit, thirst, faint and drink rules and the end of a meal are
ported; storage pits, fields and trees are not yet food targets; some forced activities of a creature with no plan are
missing. How raffclar's planner and compassion code were taken, row by row:
[Port decisions: creature](../project/port-decisions-creature.md).

### Compassion in openblack

raffclar's `creature_town_compassion` rules (`src/Creature/CreatureTownCompassion.h`) run on openblack's mind.

- Compassion plans only from the town table, never its own row (`creatureDesireAction1`, `2` and `3` are the three
  tables).
- The goal is `CompassionTown`: of the towns and creatures, the most worth it for the distance, none when that is a
  creature, and a town with no member (`mind_detail::TownHasMembers`) passed over.
- The list ends at a sum of nothing or NaN; the injured count is signed; the heal share is strictly above 0.6, x87
  cases included. `StartHalfMinute` restarts the timer when a town plan is taken up.
- The act-on object is `creature_planner::BestActOn` over `mind_detail::TownMembers`, with no distance limit and the
  confinement's `outOfReach`. A plan names its town in `Plan::activityObject` (his `about`), and the town's belief for
  the decision trees is his.
- What still differs (see Pending): the goal and the members come from the world in the registry's order, not from the
  creature's beliefs; one draw per planning rather than one per slot-0 look-up; a leash is never tied to a town desire
  (openblack has no town desire flags); "being compassionate" is his test; the town belief's attributes are his
  numbers.

### The planner in openblack

`creature_planner` (`src/Creature/CreaturePlanner.h`):

- `Priority` multiplies in the game's order; `DistancePriority` as the original; `Best` and `Choose` as the best plan
  and the switch, with no other minimum; `MayPlan` the gates (not ToAttractPlayersAttention and ToFollowPlayer).
- `PlanDesire` the goal path, `GoalSearch` saying whether the desire looks for a goal (`DesireRules::searchesForGoal`
  from the file) and acts on it (Anger); `PlanWithoutGoal` the path without a goal; `Replaces` keeps the later action
  of equal plans across openblack's groups of actions by target.
- Plans are kept across passes (lost when their desire is suppressed for longer or what they rest on is gone, which
  also starts the pass again); the pass is `StartPass` / `NextGoals` / `PassOver`; the walk back is forced at
  `k_ForcedScore`; a stage change gives the plan up (`GiveUpPlan`); `CREATURE_SET_AGENDA_PRIORITY` writes the current
  plan's priority.
- The mind file's increase times are loaded as they are, as the original's.

### A plan carried out to its end in openblack

`CreatureMindSystem::FollowAgenda` runs the completion for a plan carried out to its end (its agenda run out under the
same plan, not given up):

- `FinishAction` for the body (skipped when a step of the plan ate, which charged it already);
- then `creature_learning::CompletePlan` with the plan's desire, the action row's desire (none at 40 or more),
  multiplier and `alwaysApplies` (the row's +0xC8), the plan's `forced` and the species' floor: `CountCompletion` on
  `DesireState::completions` against `weight` (the completions needed), and when satisfied
  `ClearSourcesAfterSatisfying` (`Source::spentWhenSatisfied`, from the source table) and `AfterAction` (only with
  `alwaysApplies` or `Desires::achieved`), then `AfterCompletion`, the table above;
- for RestoreHealth the life goes up by 0.5 (`creature_learning::LifeAfterRestoringHealth`) through `life::SetLife`,
  with no upper limit, as the original's.

`Desires::achieved` goes back to false in `Abandon` and `StopWhatItWasDoing`. Forced plans are `ForceActivity`'s (the
after-feedback reactions, the slight touch's look, the stroked action done again) and the fizzle's and the happy
plan's; the planner's own plans are not. The step effects no longer touch the desires, but the eating's (its own row).

### A creature with no plan in openblack

openblack's idle mind used to choose an activity of its own (look about, pull faces, sit, hang around, as the
[Stand-ins](overview.md#stand-ins-and-unverified-constants) list) whenever its agenda ran out. That was openblack's
stand-in and is retired: a creature with no plan now does nothing of its own and draws nothing. The creature window's
old idle policy switch brings it back for comparison; it is off by default and no check run uses it.

- With the planner's passes, which leave turns without a plan, the testbed scenario idle.fidget against its twin with
  the old policy on (idle.fidget_old_policy) differs at 900 frames only in the random draws; at 3000 frames the old
  policy first moves the creature off its spot at turn 164, where the game's policy keeps it on its planned action.
- Of the ways an activity starts with no plan, openblack has the adoption, fainting, the walk back and mimicry, and
  the learnt heal only as the first action of a compassion plan.
- Two things only the retired idle policy did have no counterpart: putting down what the creature holds, and the
  happy or sad face just after a stroke or slap (the original keeps a next action after a young creature's plain
  feedback instead).
- The ProcessState switches and [0xD00DEC] are taken as set, as they are; openblack runs the code they gate.

### Sleep, sit, thirst, faint and drink in openblack

- `creature_physiology::SlowsEnergy`, `GrowsAsleep`, `LongEnoughAsleep`, `SleepTurn`, `SitTurn`: at its own turn the
  body asks the mind for the action it carries out (`CreatureMindSystemInterface::CurrentActionOf`: its plan's action,
  or the row its idle activity counts as, the same rows feedback is credited to), as the mind's turn, planning and
  learning of the turn before left it, and keeps it in `CreatureNeeds::action`; it takes the energy and growth flags
  from it and the day's length from `DayNightClock::GetDuration` (the cycle the land or a script last set).
- The mind rests a sitting creature (`CreaturePhysiologySystemInterface::Sit`) once a mind turn, after the body's turn,
  on each turn its sit counts its time (`creature_mind::SitCounting`: a static step whose loop is 38, past the turn it
  started on and before its end is asked for; `CreatureNeeds::sitting`), the sitting down included. The sleep's heal
  and rest stay in the body's sleep step (`Rest::Asleep`).
- `creature_physiology::TickTurn`, `ShouldFaint`, `FaintGates`; `CreaturePhysiologySystem::Drink`: the thirst cuts the
  turns a second and then the product to whole numbers (`map_coords::FtoL`, the SSE2 path: a NaN or out-of-range
  product is 2^31 turns, a negative one wraps round as unsigned), and a cut to none parches the creature in one turn.
- A creature faints only from phase 5, owned by a player (not the neutral one) whose `PlayerMagic` player type
  (`magic::players::ReadMagicOf`, the player system's for a player without an entity) is not 2
  (`PlayerMagic::k_ComputerPlayerType`), and not controlled by a script (`ScriptHeld::controlledByScript`, the 0x400
  bit), the three reasons in the original's order.
- Drinking empties the thirst and sets the creature's temperature to 0 through the fire system
  (`FireSystemInterface::SetTemperature`), after which the mind suppresses the water desire for 20 s as before.
- What it moved in the checks: on Land 1 the creature sleeps on the spot from turn 32 and, by the day's length, has
  not slept long enough at turn 119, where the old rule woke it; so it does not get up and think at turn 137 (three
  fewer synchronised draws there), and the fidelity run, the cycle and the hand demos move from turn 137, the
  land-view shots too. The temple shot `t120_200` and the testbed scenario `creature.walks_in_temple_pen` move with it
  (that scenario's random part from turn 130, 142 draws against 147). With the mind file's needs read back, a loaded
  creature on Land 1 sleeps longer (its sleep sequence's loop is still playing at turn 105, where the species' defaults
  had it end), so in the temple shots at frame 700 (entered at turns 120 and 120,200) it still lies asleep in front of
  the cave.

### Eating in openblack

- The rules are the pure functions of `creature_eating` (`src/Creature/CreatureEating`): `CanBeEaten`,
  `PortionAmount`, `PortionScale`, `PortionPoint`, `JudgeMeal`, `HungerAfterMeal`, `IsFoodPileOutsideStoragePit`,
  `StoragePitHasFood`, `FieldHasFood`, `TreeTallEnough`, with their tests (`test_creature_eating.cpp`; the last three
  are not wired to any plan yet).
- The end of a meal is wired: the energy as the eating starts (`CreatureObjectActionSystem::Start`,
  `CreaturePhysiologySystem::Eat`, filling up to the drawn size); at the eat moment the hand lets go and the thing is
  hidden off the land (`components::NotDrawn`); when the animation ends it is used up and judged
  (`CreatureObjectActionSystem::EndMeal`, `JudgeMeal`; a mushroom is a mobile object of the magic mushroom or toadstool
  class or the magic mushroom feature; poisoned as the original), and the mind told
  (`CreatureMindSystemInterface::EndMeal`: the type's eaten or not-food count in
  `creature_mind_model::Learnt::objectTypes`, read from and written back to the mind file; the hunger by
  `HungerAfterMeal`, or Hunger held back 20 s and Puke).
- The food choosers take what the creature can eat (`mind_detail::CanBeEaten`, through `Accepts`' Food and LiveFood
  and `NearestFood`), not what has a food value: an object it can pick up, a mobile static that is not a toy, never a
  creature (testbed `mind.hungry_takes_what_it_may_eat`); the fields, fish farms, storage pits and grain wait on the
  food-targets row (B4g).
- Eating from a magic food pile is wired as the original's: the plan's object is a pile that passes
  `IsAFoodPileOutsideStoragePit` (`Target::FoodPile`, the nearest town by `map_cells::GetNearestTown` with no limit,
  its pit by `town_queries::GetStoragePit`, the distance by `GetDistanceInMetres` of the two MapCoords); its agenda goes
  to the pile's point (`creature_eating::PileApproach`), turns to face it, then `ObjectOrder::Kind::PickUpPortion`
  (`CreatureObjectActionSystem::PickUpPortion`: a `WheatInHand` pot (row 17) of `PortionAmount` at `PortionPoint`, the
  range being the pick-up test's reach, scaled to `PortionScale`; then the ordinary pick-up of it) and the eating of
  what it holds; the source loses what the portion holds (`object_resources::RemoveResource`). Testbed
  `mind.hungry_eats_from_magic_food`.
- Eating from a storage pit, a field or a tree: `creature_mind::EatFromStoragePit` / `EatFromField` / `EatFromTree`
  (`CreatureIdleMind`), through `creature_plan_actions::Build::EatFromStoragePit` / `EatFromField` / `EatFromTree`
  (`Situation::pitPile`, `pitPilePoint`, `height`, `holdingEatable`, and `portionApproach` for the tree); the turn is
  `TurnToFaceObject` held `creature_eating::k_FaceSourceSeconds`, the pit's distance `k_PitApproachHeights`; the faces
  are `Cue::None`; sub-action 0x6C is `Step::Kind::ClearObjectToActOn`, done as it starts with no face and no draw
  (`Commands::clearObjectToActOn`); the portion's row is `ObjectOrder::portion` (`PotInfo::WheatInHand` by default,
  `TreeFruitInHand` for a tree), passed to `CreatureObjectActionSystemInterface::PickUpPortion`. No executor uses them
  yet, so no plan reaches them.

### The map of the land in openblack

Ported from raffclar's creature-exploration-map lane, unverified against v1.42. When a land starts, the land is cut
into 64 x 64 regions of 8 x 8 cells (`creature_exploration::RegionMap`, made in `Game::StartNewLand`). Each region
records its highest cell, its first coast and sea cells, whether a town building stands in it, and whether it is a hill
(higher than half the highest region, with no higher region next to it). Each turn the hand does not hold it, a creature
notes the region it stands in (`CreatureExploration`). GoToHillAndLook, GoToHillAndSit, GoToHillAndWalkAlongRidge,
ExploreCoastline, ExploreTowns, SitDownOnBeach, LookAtMountains, LookOutToSea and ThrowInTheSea take their places from
the map (`CreatureMindSystem::SituateExploration`). The ridge walk counts as done at the hill, and the point at the
mountains counts as done when the creature points, before the rest of the agenda (`Effect::Completed`). Three quirks of
the game are kept as they are:

- **The furthest region once all are known.** A search that prefers regions the creature has not been into keeps the
  last region of the kind it met when it has been into all of them. The creature then goes to the furthest region in
  reach, not the nearest. Regions are never forgotten, so a creature that has explored every coast, hill or town in
  reach always goes to the furthest one (`CreatureExplorationMap::FindNearest` 0x4DF720 writes the output on every hit
  and returns only on an unvisited one; the visited bits are cleared only by the constructor 0x4DF4F0).
- **Nothing found: the low corner.** When the map has no region of the kind and no land region in reach, the hill,
  coast and town actions still send the creature to the map's corner (0, 0), because the output point keeps its zero
  start (`ConstructSubActionsForGoToHillAndLook` 0x49D750, and the ridge, coast and town ones). ThrowInTheSea,
  SitDownOnBeach and DanceOnYourOwnByTheSea ignore the result too. On a real land a land region is always in reach.
- **GoToHillAndSit does not sit.** It only walks to within three heights of the hill
  (`ConstructSubActionsForGoToHillAndSit` 0x49D8D0 adds one MOVE_TO_POS step and nothing else).

### Test hooks

- The creature window's old idle policy switch (off by default): the retired idle mind, for comparison.
- The debug window's body panel: the action, whether it counts as asleep for energy and growth, sitting, the turns
  after which it has slept long enough, and an "Exhaust" trigger (exhaustion 1: it faints at its next turn if the gates
  allow).
- Testbed scenarios: idle.fidget and idle.fidget_old_policy, `creature.walks_in_temple_pen`,
  `mind.hungry_takes_what_it_may_eat`, `mind.hungry_eats_from_magic_food`.

## Pending

- **The map of the land**: none of it is compared with v1.42 yet. Not done: how long the creature stayed in each
  region. A map place that cannot be reached counts as reached, because the walk ends when the creature stops. Towns
  built after the land starts are not added to the map. The beach's clear place does not yet keep clear of living
  things, nor keep within a confined creature's area.
- **Compassion and towns**: the draws as the original's, one `GameRand(2)` per slot-0 look-up, need the +0x10
  predicates (0x9D1678 + 0x50 × action) of every town-row action read; the goal and the act-on members from the
  creature's beliefs, in their order, rather than the world's; whether mind +0xF50 is cleared when a plan ends or is
  given up (raffclar's "being compassionate" test stands in for it); the town belief's attribute functions (religious
  belief 0.2/0.4/0.6, size 20/40, what it needs most) against his numbers; +0x21BC's start value. A stroke on a town
  member needs its life below 1 (`CanBePickedUpByCreature` 0x4E3A20); queued as a fix.
- **The plan's priority**: the goals and things from the creature's beliefs rather than the world (Gather, 200 m) are
  queued as a fix. Not compared yet: the goal path weighs actions in groups by target; the distance is from the
  creature, not from the leashed object; the first pass of a new mind starts empty, where the original's state at
  creation is not read; the wide-screen look at the camera; Look's drop of a held object that is gone; the script's
  agenda priority with no plan carried out; the leash's act-on objects (packets 0x5C / 0x5D) and their priority; no
  instrument is chosen, so B is always 0.1; the gates of ToAttractPlayersAttention and ToFollowPlayer; the increase
  time's clamp in learning tests its low bound first and its bounds are the species' (1 and 1200 in every row). Rest
  made most dominant ×1.5 while carried by a moving creature (0x47368E) can't arise: openblack's creatures can't carry
  creatures.
- **The desires' floor**: the original holds a desire at or above the species' `desireFloor` (info +0x274, 0x4DC4A8);
  openblack uses it at a plan's end and 0 elsewhere (every row holds 0). Queued as a fix.
- **A plan carried out to its end**, each queued as a fix: the +0xF4 rows' life of 1 and climate temperature; the six
  sub-actions that set +0x650 (until then `Desires::achieved` stays false, so Hurl, Stomp, the run-aways, the house
  help, PutOutFire and the in-hand rows never multiply); the +0x1CEA0 and +0x2200 bookkeeping; `FoughtFight`, which
  multiplies Anger at every fight's end with no plan, until the fight's forced plan is read; the eating effect's
  multiply of EatAfterExamining's desire, which the original never does (row 12's +0xC8 is 0); the drink's 20 s Water
  hold-down and the poo's value of 0 and 60 s hold-down at their effects, where the original holds Water and GetColder
  down 30 s as it builds the drink's agenda (0x4A1783 / 0x4A179E), Water 20 s in a sub-state (0x5052D0) and Poo 60 s
  in `LH3DCreature::DoStaticAction` (0x48791D). Not read: whether the stroked
  action done again and the stroke's happy plan are forced in the original (openblack takes them as forced); the
  `IsActivated` game flag 0x2000; the second call at 0x507470.
- **A creature with no plan**: the cinema's forced plan; going to water when on fire; the heal of the nearest town as
  its own forced plan; the totem call queued by the leash; queued gesture commands; what a creature with no plan does
  with what it holds. The cheat-dominant desire below 0.4 life (fn_0047CFB0) is queued as a fix. A run-time read of
  the five ProcessState switches and [0xD00DEC] is pending: the original does not run on this machine (user,
  2026-10-14); they stay assumed set.
- **What counts as asleep, still open**:
  - the action a creature carries out while it lies out after a duel's knock-out (which does not go through
    `Creature::Faint`), what mind +0xF60 holds while its mind is paused, and which row it carries out when it goes to
    sleep by itself: not read;
  - openblack's sitting is the body's sit loop (animation 38), not the play-animation sub-action's own number (mind
    +0xFA8);
  - the sit rest and the sleep step: the original's plan checks (0x4733C0..0x4736F9) come before its sub-actions
    (0x4737B0), and the sleep 0x5086D0 runs after the body; queued as a fix.
- **Thirst, fainting and drinking, still open**:
  - fn_0047CE80 (the follow-player counter, Creature +0x111C): not ported, queued as a fix;
  - the help message fn_004C9FE0(0x1E, reason) of each faint waits on the creature help;
  - where in the turn: the original tests for a faint after the mind's plan step; queued as a fix;
  - the thirst: a negative info+0x224 or a NaN product; no row of `info.dat` reaches either case (all are 5000).
- **Eating**: the end of a meal is the original's. Not yet: the mushroom effect a mushroom starts, the creature's
  eaten-by-class counters and the player's condition a villager eaten adds (an eaten animal is deleted there, where
  openblack kills it as any death goes): fix row F28; in `EatFromFoodPile`, the town the game gives a portion, and,
  with something in its hand, the original's plan that ends at once or deals with it first, where openblack makes no
  agenda; storage pits, fields and trees are not food targets (B4g). Not determined: the two flags (+0x24 & 0x4000,
  & 0x200 with the game's +0x45E8 / +0x45EC) that make a creature spit a thing out before it is judged; the amount of a
  tree's fruit portion; the pile's +0x78; what the turn's argument 0.1 does (0x4AE5C0). For the pit, field and tree
  plans: whether a creature holds a belief about itself (so whether 0x6C leaves the plan's object as the creature or as
  nothing; the mind does not apply `clearObjectToActOn` yet); what the "return 1" with something eatable in hand does
  to the plan (openblack makes no agenda); whether a NULL face function leaves the face as it was; how many turns 0x6C
  takes (openblack: the turn it starts).
</content>
