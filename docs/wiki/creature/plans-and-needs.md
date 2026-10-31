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
than from the creature's beliefs, and some forced activities of a creature with no plan are missing.

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
- **The town's belief in the creature's player** (`AttributeTownReligiousBeliefInYou::UpdateValue` 0x4D3A60, a slot
  of `CreatureBeliefAboutTown::UpdateAttributes` 0x4D7FC0): a thing that is not a town, or **no player**
  (0x4D3A82..0x4D3A88), gives the value 0 (0x4D3AA5); else `GBelief::GetBeliefInPlayer` (0x4D3A91) against 0.2, 0.4
  and 0.6. So a creature that belongs to nobody, as the challenge script's `CREATE` makes one, reads 0 of every town.
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
- **A forced plan is not asked its row's test** [V]: the action table's +0x10 / +0x14 slot (the "is it possible now"
  predicate, a member pointer, filled by `_$E90`) is read only by the planner's `fn_004F0D60` (0x4F0D91, called from
  `FindBestActionForPlan` at 0x4F0B78) and by `CanCreatureCurrentlyPerformAction` 0x4F1B60 (0x4F1B6B); neither
  `ForceActivityAndForceAction(plan)` 0x4C44B0 nor `ConstructSubActionsFromActionAgenda` 0x4D36A0 (which reads only
  +0x20, +0x24 and +0x30) reads it. A script's CREATURE_DO_ACTION, `ScriptDoAction` 0x4F7000, goes straight through
  `ForceActivityAndForceAction` 0x4C4450 (0x4F7052), as do the reactions, so they are carried out whatever the row's
  test would say. `CanCreatureCurrentlyPerformAction` is called by the stroke's do-it-again `fn_004E06A0` (row 276 made
  11 at 0x4E0CCA, the test at 0x4E0D10, and its copy `sub_50A1F0`), by `MimicForceCopyingAction` (0x4EA1EA, 0x4EA32B,
  0x4EA46C) and by `fn_004F1C40` (from `FindSomethingToDoOnObjectUsingObject`); of the forced paths only those two ask
  the test first. The rows with a test among the actions openblack gates: SleepAtHome (17)
  and PrayAtCitadel (319) `NotTooFarFromHome` 0x4D2D60 (0x49A327); GoHome (162) `IsCreatureNotNearHome` 0x4D2FF0;
  RestToGetBetter (59) and RestOnTheSpot (178) `NothingScareyNearMe` 0x4E44A0; LookAtHand (8) `IsHandMoving` 0x4D2D20;
  GoToHand (89) `IsCreatureAwayFromHand` 0x4D2F50; GoToMiddleOfScreen (88) `IsCreatureAwayFromCentreOfScreen` 0x4D2F20;
  FollowPlayer (22) and ShowPlayerAnObject (24) `IsCreatureFarAwayFromCamera` 0x4D2BC0; BeCrossWithPlayer (207)
  `IsCrossWithPlayer` 0x4D31D0 (all read from `_$E90` in the v1.42 exe; bw1-decomp's `CreatureAction.cpp` table agrees).
  GoHomeToRecover (179) and RunHome (170) have none. `ConstructSubActionsForPrayAtCitadel` 0x4C2450 tests nothing
  itself: it walks to the home and prays. Of these rows the game's `Scripts/Quests/challenge.chl` forces
  (CREATURE_DO_ACTION, function 75) only SleepAtHome (CreatureDevelopment's CreatureDevSeeHome, CreatureGuide's TheStorm
  and GuideImpressTownNextLesson) and LookAtHand (CreatureCurse) [V].

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
  ToAttractPlayersAttention 0x4DF0C0 (`IsPlayerIgnoringMe`, AttractAttention's slot +0x2A8 = 17 x 0x28, written at
  0x4DB9B0) yes when ftol((1000 / msPerTurn) x 120.0f [0x8CF338]) < the turns since the player last looked at it
  (mental +0x18C4C, unsigned); ToGetWarmer 0x4DF110 yes when warmth (physical +0x10) <= -0.3 or NaN; ToGetColder
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
- **`GetObjectToActOnUsefulness(belief, d)`** 0x4E9A70, how useful a thing is to act on [V] (an oracle run):
  1. Kept within an area (`IsConfinedToArea` 0x47CCE0) and `GetDistanceInMetres(creature +0x14, belief +0x24)`
     strictly above the radius (creature +0x11B4; `test ah, 0x41`, so equal or NaN is within): 0, unless d is 35
     HangAroundAtHome and the thing `IsCitadelPart()` (vt +0x35C). The held 1.6 does not save a thing beyond it.
  2. The desire's factor: the per-desire table entry +0x24 (0xC87874 + 0x28 d), a member function on the thing given
     the creature, else 1.0. `_$E175` 0x4DB5F0 fills two of the 40: 3 Play `MultiplyUsefulnessForPlayfulness`
     0x4E35F0 (`IsPlayingFootball` vt +0x364, only the Villager's 0x532DB0, true when the final state's exit is
     `ExitFootball` 0x763280, states 71..84: 5.0; else `IsToy(0)` vt +0x3B4, Ball 0x4363B0 always, MobileStatic
     0x609020 for the meshes 0x18F..0x193: 3.0; else 1.0) and 6 Curiosity `MultiplyUsefulnessIfInteresting` 0x4E35C0
     (`IsDoingSomethingInteresting` vt +0x350, only the Villager's 0x4E3B60: the top state's info +0xF0 (the file's
     +0xE0) is not 6 and `KnowsAction(0, it)` is false: 5.0; else 1.0). Then, for 6 alone, a thing that is an `Object`
     (dynamic cast) and not interesting, whose `GetCreatureBeliefType` (vt +0x10C) is marked at mind +0x1D420 + 4 type:
     0.01 (0x3C23D70A). That mark is written only by sub-action 62 ObserveObject's stage 1 (0x505C30), from the step's
     +0xFE0.
  3. `CalculateDistancePriority(d, belief)` 0x4F1EF0.
  4. The usefulness of tree type 1 (mind +0x2518 + 4d, `CalculateUtility` then `ConvertUtilityToUsefulness`), times
     the distance priority, times the factor, each product rounded to a float.

  Its callers: fn_004F10A0 (the thing done to without a goal, and the stroke repeat), fn_004F1A10 (a creature goal's u
  in the goal path's action pick), the `FindBestObjectToActOn` of Town (0x4DAD7F; its p20 is `TownObjectToActOnUsefulness`
  0.1), Flock, Creature (p20 = it), Forest and Citadel, `FindSomethingToDoOnObjectUsingObject` (two "above 0" tests),
  `SetupReactToCreature`, and sub_50A380 (what to do with a gift, Curiosity, Hunger and PlayWithPlayer each scored by
  its desire's value times this; [giving](../hand/grab-hold-and-throw.md#giving-a-held-object)); `HowInterestingIsObject`
  0x4C3B30 has no caller. None chooses what to eat but the gift's Hunger and the planner's: fn_004C52D0 (from
  `ForceActOnObject`, `ForceMeToActOnObject`, sub_47AA50) only tests the leashed act on.
- **The action and what it is done to**, by the table entry +0x20:
  - 0, the goal path (the flagged desires but Anger): per action, fn_004F1A10 keeps the best inst x u x (prio +
    novelty) on >= (the later of equals), u = the goal's usefulness to act on for a creature goal, else 0.1; then the
    goal belief's `FindBestObjectToActOn` finds the thing (p20 = 0.1), or the action is passed over (up to 30 tries).
  - 1 with a goal, Anger: the thing first, from the goal, then one action pick; p20 = the thing's
    `GetObjectToActOnUsefulness`.
  - 1 without a goal, every other desire (fn_004F0D60): the goal term is 0.1. The action first: GetNth 0..29, its row
    predicate, `KnowsAction` x3, not passed over; prio + novelty kept on >= (the later of equals; NaN never). Then
    fn_004F10A0: an action done to something takes the best of both belief lists by `GetObjectToActOnUsefulness`
    alone (nothing on top of the distance it carries), strictly above 0, the first of equals, and p20 = that; else the
    creature's own belief and p20 = 0.1.
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
  Creature +0x1064 (`LearningEnabled`) set forces {IdleAroundWithPlayer, LookAtCamera, itself} at 1.0
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

#### Mending a building or a totem

HelpRepairHouse (action 35) [V]: its row's test of the thing acted on is vt +0x294 `NeedsRepair`. Every fixed thing
(Abode, Feature, CitadelPart, SpellIcon, the town's totem `TotemStatue` and the rest) takes MultiMapFixed's 0x401600,
`!IsRepaired()` (vt +0x88C); only Field 0x528000 and FishFarm 0x52C580 have their own. MultiMapFixed's own `IsRepaired`
0x438D70 says yes, but Abode's 0x4016A0, SpellIcon's 0x55D3B0 and the totem's 0x561090 say yes only when
`GetPercentRepaired()` (vt +0x884, MultiMapFixed's 0x401500 for all three: `GetLife()`, vt +0x11C) is at least 1.0
(0x8AA390), and the totem's `GetLife` is Object's 0x402600, its own life (+0x48). So a town's totem below its full
life is a thing to mend, as a damaged house is (xref vtable rows `TotemStatue` +0x294, +0x88C, +0x884, +0x11C).

The repair sub-action (37, 0x5044E0) calls the thing's `IncreaseLife(0.2)` (vt +0x5BC, 0x3E4CCCCD, 0x50451D..0x504524)
and counts its aim achieved when its `GetLife()` is then at least 1.0 (0x504530..0x504549). The totem's
`IncreaseLife` 0x737C60 passes the call on to its town centre while it has one (see
[building sites](../world/building-sites.md)), so a totem standing on its centre mends the centre and keeps its own
life; its own life drops only when it was hurt without a centre (its `ReduceLife` 0x737C90 passes harm on the same
way).

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
  clears Creature +0x384 (the fainted flag, see
  [the faint of behaving strangely](hits-and-catch.md#the-faint-of-behaving-strangely)) and +0x380 and jumps to
  0x4737B5, past the agenda step fn_004D37F0 and `ProcessSubAction` 0x4FEDA0: no sub-action and no body action is started. The one exception: with the thinking block
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
- **When a sleep may be chosen.** Each row of `CreatureAgenda::ActionTable` (0x50 bytes, filled by `_$E90`) has at
  +0x10 the test the planner (`FindBestActionForPlan` via fn_004F0D60) and `CanCreatureCurrentlyPerformAction`
  0x4F1B60 call before an action. `Creature::NotTooFarFromHome` 0x4D2D60 is on rows 17 SleepAtHome and 319
  PrayAtCitadel: the creature has a player (vt 0x1C), the player a citadel (+0xA48), and its home (+0x1200) is nearer
  than 1000 m ([0x8AB228]; a distance that is no number counts as near). `Creature::LeashedOrTooFarFromHome` 0x4D2E40
  is on rows 78 SleepByObject and 313 SleepOnTheSpot: true when its leash is on (`GetInterfaceStatusLeashOn`, a tied
  leash too), or it has no player, or the player no citadel, or its home is further than 140 m ([0x8CEFC8]; equal or
  no number is not further, `test ah, 0x41`).
- **Forced actions skip that test.** Only fn_004F0D60 (the planner's `FindBestActionForPlan`) and
  `CanCreatureCurrentlyPerformAction` read the row's +0x10. `ForceActivityAndForceAction` (0x4C4450, then 0x4C44B0 ->
  `ConstructSubActionsFromActionAgenda`) does not, and `Creature::ScriptDoAction` 0x4F7000 (the script's do-action)
  goes straight to it after `PrepareCreatureForScriptedAction`. The one forced caller that asks first is the
  feedback's do-it-again in fn_004E06A0 (from `GPacket::ProcessPacket`): it makes the plan (row 276 carried out as 11,
  0x4E0CCA), then `CanCreatureCurrentlyPerformAction` at 0x4E0D10, which calls the +0x10 test, then the force. The
  other callers of `CanCreatureCurrentlyPerformAction` are the mimicry (`MimicForceCopyingAction`) and the planner's
  object search (fn_004F1C40).
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

### The agendas of the lesson actions

The actions the first land's lessons force on a creature (`CREATURE_DO_ACTION`, function 75, `Creature::ScriptDoAction`
0x4F7000, which goes straight to `ForceActivityAndForceAction`) are built by their rows' `CreatureAgenda::
ConstructSubActionsFor…` builders. Read in bw1-decomp `src/Black/CreatureAction.cpp` and checked on the frozen exe
with xref (the sub-action numbers are bw1-decomp's `CREATURE_SUB_STATE_ACTIONS`, the look and face functions the
pointers each `AddSubAction` 0x4FF240 / `AddMainSubAction` 0x4FF3A0 is given) [V]:

- **Which desires list them** (the frozen `Scripts/info.dat`: the action names at 0x4BD40 + row × 0x100, the three
  desire-action tables after the 328 rows, 30 words a desire, 40, 17 and 40 desires; read 2026-10-20) [V]: Tiredness
  (desire 8) lists SleepAtHome (17), SleepByObject (78) and SleepOnTheSpot (313); Illness (25) lists SleepAtHome among
  Sneeze, Shiver, ShowHotness and Puke; Fear (5) lists BeFrightenedOnTheSpot (174) with RunAwayFromObject and RunHome.
  No desire of any of the three tables lists HoldObject (64), PutDown (67), WaveAtObject (91), PointAtObject (163) or
  DeadForever (261): the planner never chooses them, only scripts and the other forced plans do.
- **Sleep** 0x49B1C0, the builder of `SleepAtHome` (row 17; `SleepOnTheSpot` and `SleepByObject` fall back to it
  when they find no clear place): `GameRand(2)` (0x49B1D5), 0 giving the yawn, individual action 0x39 (57) with
  `LookJustWokenUp` 0x4D1100 and no face; `MOVE_TO_POS` to the home point within min(5.0 [0x8AB6E4], height), with
  `LookWhileGoingTowardsPoint` 0x4D0C80 and no face; `FACE_DOWN_SLOPE`, no look or face; the sleep (the main
  sub-action), no look or face; individual action 0x3F (63, the dazed look about) with `LookJustWokenUp` and
  `SetFaceForActionGrimace` 0x4C87F0. Its only draw is the yawn's; only the waking pulls a face.
- **WaveAtObject** 0x4A5EB0 (row 91): the row's object test is `IsCreature`, so the planner waves only at another
  creature (a forced wave is not asked). `GO_NEAR_OBJECT` (0x3D) to the plan's thing within 3.5 [0x8CF304] × its
  height (vt +0x42C), with `LookWhileGoingTowardsObject` 0x4D0BD0 and `SetFaceForActionSmile` 0x4C87D0;
  `TURN_TO_FACE_OBJECT` (6) for 1.4 s (0x3FB33333) with `LookAtObjectArgument` 0x4D16D0 and the smile; the main
  sub-action, individual action 0x48 (72, the friendly wave), with `LookAtObjectFlutteringEyelids` 0x4D1220 and no
  face (0x4A5FB4..0x4A6006). No draw.
- **PutDown** 0x4A3720 (row 67): one `DISCARD` sub-action (not the main one) with clip 0x61 (97, the put-down),
  no look or face. No draw. The DISCARD sub-action starts `StartObjectRemoveAction` with the sub-action's clip (see
  [script-commands.md](../scripts/script-commands.md)), so it is a put-down, not a toss.
- **SleepByObject** 0x4A4610 (row 78; the row test `LeashedOrTooFarFromHome` 0x4D2E40, as SleepOnTheSpot's; the
  object test `NothingScareyNearMe`, which measures from the creature, not the thing): a clear area about the plan's
  thing (its map coordinates copied whole, thing +0x24) 4.0 [0x8AB418] × its height (vt +0x42C) wide, nothing living in
  it unless a script controls the creature (`FindClearArea` 0x4DFBE0, 0x4A467C). Found: `MOVE_TO_POS` to it within its
  2D radius with `LookWhileGoingTowardsPoint` and no face; then `GameRand(2)` (0x4A472E), 0 giving the yawn 0x39 with
  `LookJustWokenUp`; `FACE_DOWN_SLOPE`; the sleep (main); 0x3F with `LookJustWokenUp` and the grimace. The yawn's draw
  comes after the walk is added, unlike the other two sleeps. None found: `ConstructSubActionsForSleep` (0x4A4886), the
  sleep at home above.
- **HoldObject** 0x4A2BC0 (row 64; no row test; the object test `CanBePickedUpByCreature`): with nothing carried
  (`GetObjectCarried`, 0x4A2BDA) a `PICKUP` of the plan's thing with `LookWhileGoingTowardsObject` 0x4D0BD0 and no
  face; the main sub-action `TURN_TO_FACE_CAMERA` with `LookAtCamera` 0x4D1A30 and
  `SetFaceForActionReflectAttitudeToPlayer` 0x4C8620; then, when the plan's thing is an `Object` (the dynamic cast at
  0x4A2C91..0x4A2CA7) whose type's count in the mind's learning (`HeldObjectActionCounts[type]`, mind learning
  +0x183A8, 0x4A2CBF) is under 2, `HELD_OBJECT_ACTION` with clip `GameRand(4)` + 100 (0x4A2CFC; stroke, shake, smell
  or examine) and `SetFaceForActionSmile`, and the count goes up; last a `WAIT` of
  ftol((40.0 [0x8CF300] + `GameFloatRand(10.0)` (0x4A2D8C)) × turns a second) turns with `LookAtCamera` and the
  attitude face. Draws in order: the play's `GameRand(4)` (only when it plays), then the wait's `GameFloatRand(10)`.
- **DeadForever** 0x4BAAF0 (row 261, `CREATURE_DEAD_FOREVER`; the row's object test is `IsCreature`): the main
  sub-action `FAINT` with clip 0x6A (106), then `WAIT` for ftol(864000.0 [0x8CF320] × turns a second) turns, ten days,
  no look or face on either. No draw. `FAINT`'s start sets the fainted flag, starts the fall and makes a
  REACT_TO_DEATH reaction with no player; its next stage waits until the body lies. `Creature::ProcessState` gets up
  every creature it sees lying unless its action is 9, 0xD9 or 0x105 (261) (0x472F03..0x472F3D, see
  [hits-and-catch.md](hits-and-catch.md)), so a creature dead for good stays down, and the wait, counted from the turn
  it lies, runs out only after ten days.
- **DiePermanently** 0x4AC3B0 (row 217, `CREATURE_DIE_PERMANENTLY`; bw1-decomp `src/Black/CreatureAction.cpp:3669`,
  read in the v1.42 exe; the row has no object test): DeadForever's two sub-actions, `FAINT` with clip 0x6A then
  `WAIT`, the wait for ftol(999999.0 [0x8CF30C] × turns a second) turns (9999990 at 100 ms). `ProcessState` leaves a
  creature of action 0xD9 lying as it leaves 261's (0x472F2D). `Creature::IsDead` 0x47CD70 and the partner test
  count only actions 9 and 0x105 as dead, not 0xD9, so a creature dying for good is still asked to be a partner. No
  desire lists row 217 (the frozen `Scripts/info.dat`); only scripts force it, on the creature itself
  (refs/chl/killkhazar.txt:83076, refs/chl/beginland5.txt:130831). Row 9 Die is another path (below).
- **Die** `ConstructSubActionsForDeath` 0x49B770 (row 9, `CREATURE_DEAD`; bw1-decomp `src/Black/CreatureAction.cpp:650`,
  checked on the v1.42 exe; the row has no possibility or object test, and no desire of the three tables lists it,
  read in the frozen `Scripts/info.dat`): the main `FAINT` with clip 0x6A; `WAIT` for ftol((LH3DCreature +0x90, the size
  it is drawn at, × 4.0 [0x8AB418] + 8.0 [0x8C2C70]) × (1000 div ms a turn)) turns (`fimul`, 0x49B807..0x49B832);
  `TRANPORT_HOME` 0x70 only when `GetPlayer` (vt +0x1C) is not null (0x49B880); `WAIT` ftol(3.0 [0x8C2C50] × turns a
  second); `WAIT_FOR_SPELLS_TO_WEAR_OFF` 0x8B; `REST_TO_GET_BETTER` 0x82 only when exhaustion (phys +0x30) > 0.3
  [0x8AB23C] or life (vt +0x11C) < 0.4 (double [0x8CF2B8]), decided as the agenda is built (0x49B979..0x49B9A8); then
  `SendCandidateHelpScript(0x25, 6)` (miscellaneous stacked, CREATURE_TRANSPORTED_HOME, 0x49B9F4). `Creature::Faint`
  0x476FA0 forces the same row on itself after its own steps (the spells ended, `EndAnyAnimsRapidly`, `StopMoving`, the
  leash let go); `CREATURE_DO_ACTION` forces it without them. The sub-actions, one stage a turn
  ([script-control.md](script-control.md)): FAINT 0 draws the side (`StartDeathAction` 0x484CE0, `GameRand(2)` even on
  a body already falling or lying, which then does not fall again), sets the fainted flag and makes a REACT_TO_DEATH
  reaction with no player; FAINT 1 is done the turn the body is seen lying; WAIT 1 takes one off each turn from the
  next; TRANPORT_HOME is in [drawing.md](drawing.md#carried-home-sub-action-112); `WaitForSpellsToWearOff` (row 0x8B,
  its only stage the perform 0x508C10, written at 0x4FEB58) brings the spells on the creature to their end (fn_004F4B60
  on +0x370) and, when none is left, those on the plan's object (mind +0xF58's +0x30, `CastCreature` vt +0xA4): done
  when neither has any; with no object, or one that is not a creature, it is never done (0x508C2E, 0x508C3F). Both
  shipped scripts force it on the creature itself after it lost a fight, its life set to 0 first
  (refs/chl/land4ogre.txt:121715..121723, refs/chl/thebigfight.txt:224). openblack: `Build::Die` (in the forced-only
  rows), whose one main step waits until the body is free, and `CreatureFightSystem::ForceDie`, called as the action is
  forced, which knocks the creature out as a faint does but keeps its spells and its leash; the lie counts its turns
  by the drawn size from the turn after it is seen lying, the rest is decided as it is forced, and at home, after the
  three seconds, its spells and the thing's are brought to their end until none is left, as every knock-out now waits
  for them (tests `CreatureActionAgendas.DyingIsOnlyForcedAndWaitsOutTheKnockOut`,
  `CreatureFightCarriedHomeTest.ForcedToDieItLiesTheTurnsOfItsDrawnSizeFromWhenItIsSeenLying`,
  `ForcedToDieWhileKnockedOutItStartsAgainFromItsFaint`, `ForcedToDieItWaitsForTheSpellsThenRestsAsDecidedWhenForced`,
  `ForcedToDieOnAThingThatIsNotACreatureItNeverGetsUp`; testbed `creature.script_die`). Forced on a creature already
  knocked out (the shipped case, after the fight's own faint), the knock-out starts again from the faint, a carry home
  under way stopped in full sight at once (the sub-action's abort 0x508B20). Not settled: the turns from the wait at
  home on are the knock-out's (the 3 s counted from the fade back in, the rest's end), and what the spells' wait does
  when the thing it was done to has been deleted [?].
- **BeFrightenedOnTheSpot** 0x4B0250 (row 174): individual action 0x3D (61, the fright) with `LookFrightened`
  0x4D1250 and no face; `WAIT` for ftol(3.0 [0x8C2C50] × turns a second) turns with no look and
  `SetFaceForMoodFrightened` 0x4C84B0; the main sub-action, individual action 0x3D again with `LookFrightened` and no
  face. No draw. The row has no object test of its own. `LookFrightened` sets the eyes wide (LH3D +0x5460 = 0,
  0x4D1258) and looks about (`LookAround` 0x4D1870).

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
- **Where it drinks** (DrinkFromTheSea; its builder `CreatureAgenda::ConstructSubActionsForDrinkFromTheSea`
  0x4A15F0; the table gives the action no condition, so the planner may always weigh it):
  - `Creature::FindNearbyWaterPoint`(1000) 0x47D860 walks the game's list of water points (`CREATE_DRINK_WAYPOINT`,
    head at g_game +0x205C74, next at +0x28) and keeps a point only when it is nearer than 1000 and strictly nearer
    than the best so far. Found: the creature walks there to within its height (`MOVE_TO_POS`, look while going).
  - Else `CreatureExplorationMap::FindNearest` 0x4DF720 for the coast (region type 4) from the creature, then the sea
    (type 5) from that coast, both with preference 1 (not preferring unvisited regions); when the creature is not in
    water (`MapCoords::IsWater` 0x6035B0) it walks to the coast, then it turns to face the sea.
  - A point that fails `LH3DCreature::IsDestinationValid` 0x483850 (radius 7.1) is replaced by
    `SpiralCheckForValidPoint` 0x4839D0 (16 directions π/8 apart, rings 0.5 apart below 1000; the found point's y is 0)
    when that is valid and nearer than 30; else `FinishActionUnsuccessfully` and `SuppressDesire` of Water (14) and
    GetColder (20) for 30 s (0x4A1779, 0x4A1794).
  - Then the individual action 0x47 (the drink animation) as the main sub-action, and the drink sub-action.

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

#### Holding food

The plans that give or eat food first ask whether the creature already holds some: `carried =
creature->physical->GetObjectCarried()` (creature +0x160, then +0x28) and `carried->CanBeEatenByCreature(creature)`
(vt+0x22C), the same slot as above, never a food value. The call sites: `GiveFoodFromFieldToStoragePit` 0x4A3350
(0x4A336F), `GiveFruitFromTreeToStoragePit` 0x4AD820 (0x4AD83F), `GiveMagicFoodToStoragePit` 0x4ADC20 (0x4ADC3F),
`PutFoodFromFieldByWorshipSite` 0x4B8150 (0x4B817D), `EatFromField` 0x4A30E0 (0x4A30FF) and `EatFromTree` 0x4A6140
(0x4A615F); `GiveMagicWoodToStoragePit` asks `IsTree` instead.

- The vt+0x22C slot of every class (the xref vtable dump): `Object` 0x4E48C0 for 117 classes (villagers, animals,
  birds, mobile objects, pots, piles, trees, the football, creeds, features, abodes, lanterns, shields) tail-calls the
  class's own `CanBePickedUpByCreature` (vt+0x258); `MobileStatic` 0x4E48D0 for 11 (`MobileStatic`, `Rock`, `Bonfire`,
  `DeadTree`, `FelledTree`, `MagicTeleport`, the four `LandscapeVortex` kinds, `Fragment`) is `!IsToy` (vt+0x3B4).
- A held thing passes the pick-up test: `Object::CanBePickedUpByCreature` 0x4E3A20, at 0x4E3AD1, asks
  `IsObjectInMap` (vt+0x178) and, when it is off the map, goes on only if `creature +0x160 → +0x28` is this very thing
  (0x4E3ADF..0x4E3AE8), then `CanBePickedUpByCreatureIgnoringWhetherItIsInTheMap` 0x4E3960 (the leash, both radii
  within `CalculateMaximumActionRange(0xE)`, the weight within `MaximumPickUpWeight`, vt+0x628).
- The oracle (2026-10-18, fake `this` buffers with the real vtables): `Creature` 0x474050, `OneOffSpellSeed` 0x4E48F0
  and `PuzzleGrain` 0x561920 give 0, `Field` 0x527FD0 gives 1; `MobileStatic` with `IsToy` stubbed 0 / 1 gives 1 / 0;
  `Object` follows its pick-up test (stubbed 0 / 1 gives 0 / 1). The whole `Object::CanBePickedUpByCreature` on a
  `MobileObject` off the map gives 1 when it is the creature's carried thing and 0 when it is not, with the
  ignoring-the-map test stubbed to 1; and `Object::CanBeEatenByCreature` on that held thing follows the ignoring-the-map
  test (1 / 0).
- So a creature holding a rock, a tree, the football or a mobile object of no food value already "holds food": the
  four handful plans skip the taking and give what it holds, and eating from a field or a tree ends at once.

Ours: `Situation::holdingEatable` is `mind_detail::CanBeEaten` of the held thing (the pure `creature_eating::CanBeEaten`
with the pick-up test, which passes for the thing in this hand: `creature_pick_up::Check::inThisHand`), set for every
plan with the hand's other facts (`SituateHands`); the first port used a positive food value
(`FoodValueOf`), so a held rock or tree did not count. Test `CreatureEatingCanBeEaten.WhatItHoldsIsFoodByTheSameTest`.
Our dead trees have no `MobileStatic` component; `mind_detail::CanBeEaten` gives them the mobile statics' answer, "not
a toy" (a dead tree keeps its tree's mesh, never one of the toy meshes), as the game's `DeadTree` class (vt+0x22C =
0x4E48D0) does, for the held thing and for the food choosers alike (test
`CreatureAnimalFoodTest.ADeadTreeIsFoodAsTheOtherStaticsAre`); the first port asked the pick-up test.

The shields already answer as the game's do. The game's `PhysicalShield` class (an object: vt+0x22C 0x4E48C0, its
pick-up test `Fixed`'s 0x401440, 0) is the spell's shield: its constructor 0x72CB50 has one caller, the save loader
(`GameOSFile::LoadInstance`, 0x5592EF), and the spell makes it otherwise. Ours is a `MagicShield` entity with no
`MobileStatic` component, so it goes by our pick-up test, which never takes a shield: 0. A mobile static of the table's
physical-shield row is made by the mobile static factory 0x608770, which builds a `Bonfire` for row 8 (`Infos` +0x960,
rows of 0x12C bytes), a `Rock` when the row's +0x128 is 2, and a plain `MobileStatic` otherwise: all of the "not a toy"
family, as ours answers. No code change was needed. Fields, fish farms, storage pits (1) and puzzle grain (0) cannot be
held.

#### A portion

Sub-action 0x8A, phase 0 at 0x4FFED0: its amount is `__ftol(1000.0f × drawn size)` (0x4FFF22..0x4FFF68, the drawn size
is LH3DCreature +0x90; the float product, then the SSE2 cut: a NaN gives 0x80000000), no more than the source's
resource (`cmp; jb`, unsigned, 0x4FFF80): the resource of the portion's own row of the pot table (the row's
`GPotInfo::InfoList` entry read, then the source's `GetResource`, vt +0x98; food for the wheat and fruit rows 17 and
15); its scale is 3.75 × the drawn size (0x4FFFCA); it is made at 0x47AD80: the
difference from the creature to the source across the land, unless both its parts test equal to 0 (`fcom` and C3, so a
NaN part counts as 0; 0x47AE34..0x47AE54), is scaled to `(0.95 × pick-up range) / sqrt(z² + x²)`
(0x47AE56..0x47AE8A) and added to the creature's point: 0.95 of the range towards the source, however far it is. As
the hand takes hold, a source that is a pot or a pile loses what the portion holds (0x5000D9..0x500110; a field or a
tree would lose nothing); the game gives the portion the nearest town of its point (`GetNearestTown`, 0x4FFFAD).

#### The end of a meal

0x500540: first, a thing marked indestructible (Flags +0x24 & 0x4000, 0x50055B), or one in a script (& 0x200) while
the help system's WideScreen and WideScreenControl are set (0x500564..0x500583), is refused (0x50075A..0x5007EA): mind
+0xFBC = 0, Hunger (desire 4) suppressed 20 s (the immediate 0x41A00000, 0x50076C), `CommandActivityAndAction(0xB,
none, 50 Puke, its belief)`, then its `InitialisePhysics` (vt +0x784) with a zero velocity and spin, the creature as
thrower and the flag 1 (as the creature's own throw, 0x48D192), and it returns 0: nothing eaten, counted or learnt.
Otherwise a mushroom (vt+0x2A4; `IsMushroom` 0x4E4CE0 / 0x5276D0) or pile food (vt+0x480) is eaten; otherwise the food
value (vt+0x660, a float) compared with the double 5.0 (0x50063B, `test ah, 1`: below 5 or NaN) or a poisoned thing
(vt+0x4A4: a pot's flag `Pot` +0x74 bit 0, 0x55D4E0; the toadstool row, 0x6079E0; a poisoned living, 0x416F90) is spat
out (Hunger suppressed 20 s, the Puke activity), and only when not poisoned the type's not-food count goes up. Eaten,
the type's eaten count goes up and the Hunger desire's value (mind +0x160) becomes `hunger − f / (m × foodToEnergy)`,
m the real size (+0x6C) when below 0.8 or NaN, else 0.8 (0x5006D0), then below 0 or NaN → 0, above 1 → 1
(0x500708..0x500742). A mushroom starts the mushroom effect (mind +0x21D0 = 0x509530 when none runs); the creature's
eaten-by-class counters are 0x4794D0 and 0x479500. Then, still only for a thing available (vt +0x2C,
`Villager::IsAvailable` for a person), `Creature::VillagerDead` 0x475F60 (from 0x500617): for a villager (vt +0x2C8
`IsVillager`, true only for villagers) `Villager::VillagerDead(0 no reason, the creature's player, its life (vt +0x11C
`GetLife`), drop 0)` and the player's condition (`GPlayer::AddToCondition(1, 1)` 0x64DA80, which has no reader), then
`ToBeDeleted(0)` for whatever it was. openblack: `Consume` in `CreatureObjectActionSystem.cpp` calls
`villager::VillagerDead` with `creature_eating::EatenPersonDeathOf` (test
`CreatureEatingMeal.APersonEatenDiesToTheCreaturesPlayerWithNoReasonAndNoDrop`) before the deletion, so the town
counts the death, the owner's alignment moves and the person lets go of what it carries ✅; the Path Finding window
shows a selected villager's town's deaths. The condition is left out (no reader). The kill and mushroom tallies
count under the same test, only a meal still there (`player_statistics::CountCreatureMealEnd`, test
`CreatureTallies.AMealEndCountsOnlyAThingStillThere`) ✅.

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

#### Eating alive

`CreatureAgenda::ConstructSubActionsForEatAlive` 0x49AA20 (row 11 EatAlive, bw1-decomp `CreatureAction.cpp:485`,
matching the frozen exe): unless the creature carries a thing that passes `CanBeEatenByCreature` (vt+0x22C,
0x49AA3F), `GRand::GameRand(2)` (0x49AA59) and on 0 an INDIVIDUAL_ACTION 0x36 (54, the hungry gesture), then PICKUP of
the plan's object with `LookWhileGoingTowardsObject`; then the main EAT of the object. No sub-action has a face
function. EatAfterExamining 0x49CFE0 differs: no draw, a HELD_OBJECT_ACTION 0x67 (the examine, curiosity face) between
the pick-up and the eat. StompAndEat (row 13) is a STOMP with the anger face before the PICKUP, and Stomp (row 18)
draws `GameRand(5)` for a 1-in-5 angry gesture (0x35) before its STOMP.

#### Eating from a food pile, a storage pit, a field or a tree

Read in the v1.42 exe; the same in bw1-decomp.

- `ConstructSubActionsForEatFromFoodPile` 0x4AE450: with something in its hand, the plan ends at once (returns 1 at
  0x4AE479) when it can eat what it holds (`[[creature +0x160] +0x28]`, vt+0x22C at 0x4AE46F: [Holding
  food](#holding-food)); holding anything else it goes ahead. `ProcessSubAction` 0x4FEDA0 then deals with the held thing
  before the portion's pick-up: sub-action 0x8A's row has hand mode 1 (+0x40 of its 0x90-byte row, 0xBE0B98 + 0x8A ×
  0x90 + 0x40, filled at start-up by 0x4F8D70), and mode 1 with LH3DCreature +0x48D0 set calls
  `StartObjectRemoveAction(ForceMeToActOnObject(…))` (0x4FEF88..0x4FEFAA) before the sub-action starts. Ours
  (`Build::EatPortion`): no agenda when `Situation::holdingEatable`, else the agenda whatever it holds; the first port
  refused any full hand (`handFull`). Our pick-up with a full hand fails ("its hands are full") instead of putting the
  held thing down first: a row of the creature's object actions. Else it goes to the pile's
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

### The sacrifice action

The creature's action Sacrifice (row 282 of `Scripts\info.dat`'s action table, `CREATURE_SACRIFICE`) is built by
`CreatureAgenda::ConstructSubActionsForSacrifice` 0x4BEEC0 (xref note #493):

- It fails (`FinishActionUnsuccessfully`, 1) when the creature's player has no temple (vt+0x114 gives none) or the
  temple's six worship-site slots (+0x34 to +0x4C) are all empty. Otherwise the altar is the first filled slot's
  `WorshipSite::GetTotemPos` 0x77CF30.
- Unless the thing its hand holds (creature +0x160, then +0x28) answers vt+0x3C8, the first sub-action (0) picks up
  the action's object (+0x848), with the anger face (`SetFaceForActionAnger` 0x4C86D0) and
  `LookWhileGoingTowardsObject`.
- MOVE_TO_POS (8) to the totem's point within 3 (`__real@4@4000c000000000000000`) times `GetHeight` (vt+0x42C), with
  `LookWhileGoingTowardsPoint`.
- Sub-action 7 to the same point, with `LookAtPosition`.
- Sub-action 1 with the integer 0x61: the put-down clip 97.

The action never sacrifices (a vanilla bug, kept): the let-go 0x48C6C0 puts the thing down with no speed through the
villager's `InitialisePhysicsFromHand` 0x5EFE90, which has no altar test, and the only callers of `ApplyThisToObject`
(vt+0x720) and `IsSacrificeAltar` (vt+0x4B0) are the hand's, the interface's and the creature's queue of forced acts
(raffclar's vanilla-bugs row 135). The thing lands alive near the altar.

### The steal action

The creature's action StealObjectAndTakeToCitadel (row 265 of `Scripts\info.dat`'s action table,
`CREATURE_STEAL_OBJECT_AND_PUT_BY_CITADEL`; desire Steal, source 60 `TO_STEAL`) is built by
`CreatureAgenda::ConstructSubActionsForStealAndPutByCitadel` 0x4BAE20 (xref note #496):

- It fails when the creature's player has no temple (vt+0x1C of the creature's +0x1514 gives none, 0x4BAE31).
- When the creature's hand holds nothing (creature +0x160, then +0x28, 0x4BAE42..0x4BAE4D), the first sub-action (0)
  picks up the action's object (+0x848), with `LookWhileGoingTowardsObject`.
- The main sub-action is MOVE_TO_POS (8, `AddMainSubAction` 0x4BAF82) to the creature's home (+0x1200, its
  `GetLHPoint`) within twice `GetHeight` (vt+0x42C, then `fadd st(0), st(0)`, 0x4BAF05..0x4BAF0B), with
  `LookWhileGoingTowardsPoint`. The home point is also copied to +0x1C78 of the creature's +0x164 first (0x4BAEB7).
- Sub-action 1 with the integer 0x61: the put-down clip 97.

### The poo desire's fart

Settled in the frozen exe and info.dat (2026-10-18). ✅ (the squat's numbers run with the oracle)

- **Two actions.** The poo desire (7) lists CREATURE_POO (19) and CREATURE_FART (177), in that order
  (`creatureDesireAction1` row 7 at file 0x60874: 19, 177, then zeros).
- **The rows** (CreatureActionInfo, file rows of 0x100 from 0x4BD2C; memory offset = file + 0x10): both need an object
  to act on (file +0x94 = 1), plan index 7 (+0x98), known-about categories 6 and 6 (+0x9C, +0xA0), no magic, carried
  object not excluded (+0xA8 = 0), initial opinion 0 (+0xD4), learnable (+0xF0). They differ in the completion
  multiplier +0xC0 (Poo 0.01, Fart 0.5), the learning window +0xD0 (25 s, 30 s), +0xE0 (1, 0), +0xE8 (0.3, 0.5),
  HowNiceChange +0xEC (−0.8, 0) and +0xB0..+0xB8 (Poo 1, 5, 1; Fart 0, 7, 177).
- **The action table** (bw1-decomp `CreatureAction.cpp`): Fart has no creature-level test, no object filter (row +0x34
  NULL: `CreatureAgenda::IsObjectSuitableForAction` 0x4E3880 returns 1 at 0x4E38C4), and its construct
  `ConstructSubActionsForFart` calls `ConstructSubActionsForPoo`, so its agenda is the poo's, draws included: a
  `GameRand(3)` for the "need a poo" action, go near the object, turn away from it, the static action 0x20 for 4 s, the
  poo sub-state. Poo's object filter is `CanBePoodOn` (true for any `Object`, false for a `GameThingWithPos`; Field,
  FishFarm and Dove have their own).
- **Watching the telly** (CREATURE_WATCH_TELLY, 176): its construct `ConstructSubActionsForWatchTelly` 0x4B04B0 is
  only a call of `ConstructSubActionsForPoo`, and no desire nor the town table lists 176, so only forcing reaches it,
  and then it is a poo. openblack: a forced-only executor with the poo's agenda (`k_ForcedOnly` in
  `CreaturePlanActions.cpp`; test `CreatureFart.WatchingTheTellyIsOnlyEverForcedAndIsAPoo`).
- **How the planner picks.** Poo is not one of the desires that look for a goal, so the walk of
  [The plan's priority](#the-plans-priority) without a goal runs: prio + novelty on `>=`, so of a poo and a fart
  wanted as much the fart, the later in the list, is taken. No draw decides between them; their opinions and the turns
  since each was done do.
- **The plan's end** is the poo desire's for both: `SuppressDesire(7, 60.0)` in `UpdateOnCompletion`
  ([the table](#a-plan-carried-out-to-its-end)), and each row's own multiplier.
- **The squat** (`LH3DCreature::DoStaticAction` 0x4875D0, from `StateAction` 0x486C40, from `UpdateTime`, which
  `Creature::ProcessState` calls once a game turn at 0x473939): while the body's clip +0x49A0 is 0x20 (the poo's loop),
  the counter +0x51E4 goes up each call; at 5 (0x487632) the plan's action, mind +0xF60, is read (0 without a
  creature) and the counter goes back to 0 after either branch (0x4877DB, 0x487930). The counter is cleared only by
  `Init` and there; another clip leaves it as it is (0x48793C: only 0x23, the puke, is handled).
- **The puke** (the same `DoStaticAction`, Creature3D.cpp:3880, while the clip is 0x23, the puke's loop): each call,
  so once a game turn, twelve drops of sick from `GetVomPoint` 0x48F200 (the head bone of the turn's pose: the mouth's
  `VomIntersect` is set only by `SetVomIntersect` 0x48F0C0, which nothing calls). Each takes, on the local stream, its
  speed (LocalRand(100) × s − o, −(LocalRand(100) × s), LocalRand(100) × s − o) with s = size × 0.008 [0x8C58A8] and
  o = size × 0.4 [0x8C7A44], then a tint (LocalRand(100) + 80, LocalRand(80) + 60, LocalRand(30) + 20) that each channel
  of the land's light at the creature (`GetColorAndSpecular` 0x801C90, hazed by `GetFogValue` 0x7FEB30) multiplies,
  cut to whole numbers (light × tint / 255), and goes in as a liquid particle of kind 0 and size × 0.2 [0x8AB244]
  (`AddLiquidParticle` 0x845C20). A kind 0 particle (`InitLiquidParticle` 0x845FA0: no rand for its seed;
  `UpdateLiquidParticle` 0x846010) falls under 9.81 a second; once at or below the land under it (the altitude at the
  map coordinates of x × 65536 × 0.1, cut) it slides off at ((rand() % 100) × size × 0.6 − size × 30) on x and on z,
  the C runtime's rand, x first, with no upward speed, its age back to 0, and from then on slows by pow(0.25, the
  seconds) and keeps to the land; it goes once its age is past 3 seconds. `DrawLiquidParticle` 0x846280 draws it solid
  (material mode 9 over `datalobs.raw` with ALPHAREF 200, `sub_845DF0`), at its size, in a frame 32 + rand() % 8 of
  the sheet picked each time it is drawn. openblack (2026-10-27): the same (`creature_puke::DropsOf`, from
  `CreatureAnimationSystem::PoseTurn`, and the drop kind of the physics' puffs, `physics::turn::PuffKind::Drop`, tests
  `CreaturePuke.*`); before, a guessed spray of its own, kept in the physiology system and never drawn.
  - **CREATURE_FART** (`cmp esi, 0xB1` 0x487662): size s = +0x90 (the drawn size) × 15.0 (0x8C2C40), the angle a =
    +0x84 (the body's yaw) + π (the float π, 0x8C36A0), the offset (sin a · s, 0, −(cos a · s)), half h = s × 0.5. Then
    15 times (edi = 15): r1, r2, r3 = `GameFloatRand(s)` (0x4876E4, 0x4876FC, 0x487714, the synchronised stream), and
    `AddLiquidParticle(position +0x78, v, 0x40808080, +0x90 × 4.0, 4)` (0x48775C) with v = ((r3 − h) + offset.x,
    r2 − h, offset.z + (r1 − h)): a puff of kind 4 (one second, no gravity: [the puffs](drawing.md)), its seed one CRT
    `rand() % 16` in the list's add. Then `LH3DSmoke::Create(NULL)` (0x48776C) into the creature's +0x109C, its point
    the anus (`GetAnusPos` 0x4813D0). No lump, no change to the poo amount (physical +0x2C), the poo desire or the
    count of poos (+0x180): those are the other branch's. No sound is played here; the clip's own sounds are the
    poo's.
  - Run with the oracle (`DoStaticAction` on a fake body, the four callees stubbed, the 15 `AddLiquidParticle` calls
    captured; seed 0x88F89F): five yaws and sizes, 75 puffs, every velocity matches the float model above bit for bit,
    with the sine and cosine taken unrounded and each product, sum and difference rounded to a float.
  - **The smoke**: `Create(NULL)` makes ten white (0xFFFFFF) puffs at (0, 0.5 i, 0), hidden, ages i × 90, with two CRT
    `Random` draws each (0 to π, then 1 to 100), the same as a chimney's smoke ([Chimney smoke](../world/objects.md#chimney-smoke-lh3dsmoke));
    its state stays 0, so it puffs on at the anus. `Creature::Draw` draws it each time the creature is drawn
    (`AddDrawing` 0x517F03), and `StopWhatIAmDoing` releases it (0x475BE0), the plan's end among the callers; a second
    fart replaces the pointer without releasing the first.
  - **The fainting** (0x4877A4..0x4877D1): after the smoke, the body's alignment +0x9C (the evil-good target, held to
    ±1) `fcomp` −0.5 (0x8CEFCC), `test ah, 1`: below −0.5, or unordered, `CreateReaction(creature, 0x19, the
    creature's player (vt +0x1C), 0)`, so not stamped. Run with the oracle: −0.5 makes none, the next float below it
    makes one, and a NaN makes one, with those four arguments.
  - **Reaction 25** (info.dat ReactionInfo row 25): priority 200, 600 turns to react and 1200 before reacting again
    (creatures the same), it impresses, it ends in the hand, max distance 50, distance weight 0.4, it grows (radius 1,
    then 0.1 a turn), stealth 20, alignment for the sound 2. It is one of the time-limited kinds, so it ends 600 turns
    after a Living first takes it; until then it lasts while the creature does. Who reacts (isReacting[25], GLivingInfo
    file +0x134 + 4 × 25): every villager row and every creature row, no animal row. What they do is in
    [the villagers' reactions](../beings/villager-reactions.md) (a fainting villager's slot row) and the creature's
    forced sleep.

### A poo by a thing

Read in the v1.42 exe with xref.

- **The thing**: the Poo action (code table row "Poo") acts on a thing, which two tests must take:
  - the desire's object test (`CreatureMental::IsObjectSuitableForDesire` 0x4E3730, the Poo desire's entry, vt
    +0x24C `CanBePoodUponByCreature`): `Object` 0x4E3EB0 is `GetHeight()` (vt +0x42C) > 1.5 (`0x8AB24C`); `Living`
    0x417410 (villagers, animals, creatures, birds) and `GameThingWithPos` 0x4053B0 (towns, forests, planned
    buildings, spell creatures) answer 0;
  - the action's filter (the code table row +0x34, vt +0x2E0 `CanBePoodOn`): `Object` 0x402A30 answers 1, `Dove`
    0x41DD70 (every flying bird), `Field` 0x527F60, `FishFarm` 0x52C4E0 and `GameThingWithPos` 0x4054B0 answer 0.

  Together: a thing that is not alive, taller than 1.5 m, and not a field or a fish farm.
- **The agenda** (`ConstructSubActionsForPoo` 0x49ABB0): one time in three (`GameRand(3)` == 0) the individual action
  0x42 (no face); `GO_NEAR_OBJECT` (0x3D) to the plan's thing (mind +0x848) at distance 0, looking while going
  towards it (`LookWhileGoingTowardsObject` 0x4D0BD0); `TURN_TO_FACE_AWAY_FROM_OBJECT` (0x16) from it; the main static
  action 0x20 for 4.0 s with the smile face; then the `POO` sub-state (0xF) with the smile face.
- **Turning its back** (sub-action 0x16, its stages at 0xBE1848): Initiate 0x503080: d = thing − creature (MapCoords
  × 10/65536); |d| < 0.1 advances at once; else fn_004F00E0 (stop, then turn) to `heading_from_direction_vector(d)` +
  π (`0x8C36A0`), advancing when the turn starts. Perform 0x503160: done once `IsPerformingBodyAction` is false. No
  hold, no facing test.
- **PooAtHome** (row "PooAtHome", predicate `IsCreatureNearHome` 0x4D3000 = not fn_00479450, which is
  `GetDistanceInMetres`(the creature, its home +0x1200) > 20.0 (`0x8C7658`); no act-on filter; constructor
  `ConstructSubActionsForPooDiscretely` 0x4B03C0): `MOVE_TO_POS` (8) to its home point at 2 × its own `GetHeight()`,
  looking while going towards the point (0x4D0C80), then the Poo agenda above, its thing the creature itself.
- openblack: the poo has no thing yet; the creature squats where it stands (the 1-in-3 action and the 4 s static
  match). The thing, the walk to it, the turn away and PooAtHome are pending. raffclar's #557 `CanPooBy` is the same
  rule as the two tests together.

### A creature's throw: the flight time, the aim and the launch

✅ Every creature throw goes through `LH3DCreature::ThrowAtPointGivenTime` 0x4811A0, and only three sub-actions call it:
`THROW` (at a thing, Initiate 0x501130), `THROW_AT_POS` (at a point, 0x500E60) and `THROW_IN_PILE` (into a store,
0x500F30). All three time the flight the same way, from where the creature's body stands (`LH3DCreature` +0x78) as
the sub-action starts, before any turn:

- **The time.** `range_2d` 0x6E8130 (√(dz² + dx²), the ground distance; the heights are left out) times the float at
  0x8D1668, 0x3DD0C43D, then `fsqrt`, stored as a float (0x5011E8..0x5011FF, 0x50121A..0x501234, 0x500EED..0x500F03,
  0x501040..0x501056). 0x3DD0C43D is 1.0f / 9.81f worked out in single precision, one step under the exactly rounded
  1 / 9.81 (0x3DD0C43E). The game thread's FPU runs at 24 bits ([Floating point](../engine/floating-point.md)), so every
  step is a float step. Oracle (2026-10-18, the three Initiates run natively with the creature, its mind and the
  thing as buffers): 40 throws at a thing, 39 at a point and 12 into a store, all equal to √(ground × 0x3DD0C43D) in
  float; dividing by 9.81f instead differs in the last bit for about 4 distances in 10 000 (98.409378 gives
  0x404AB463, the division 0x404AB464).
- **The aim at a thing (`THROW`).** The thing's place (its map coordinates × 10 / 65536, the ground there plus its
  altitude) gives the first time t₁. Then `CalculateWhereIWillBeAfterNSeconds(t₁)` (vt+0x414) gives where it will
  be; the time t₂ is taken to there; then vt+0x414 again with t₂ + `GetApproximateThrowTime(40.0)`, which is always
  0.0 (0x481190 is `fld 0.0; ret 4`), so with t₂. The throw is at that second point and takes t₂. vt+0x414 is:
  - `Object` 0x63B8D0 (also every class that keeps it: the features, the statics, the rocks...) and `Abode` 0x63B940:
    the thing's place, the ground there plus half its `GetHeight` (vt+0x42C) (`fmul 0.5`, then `fadd` the altitude);
    the time is not used, so t₁ = t₂ and the aim is the thing's middle where it is;
  - `Villager` 0x63B9B0: the same middle, plus, unless its top state is a final one (the row of the top state
    +0x8C in `GVillagerStateTableInfo`, its `isFinalState` +0x1C, 0x63BA18..0x63BA30), its step (+0x64 / +0x6C,
    whole map units a turn) times the whole turns in the time. Each part in float steps (0x63BA38..0x63BABA): the
    step is `(step × MetresPerMapCell 10) × 2⁻¹⁶`; the turns are `__ftol((1000 / GGameInfo +0x40, an unsigned
    integer division) × t)`, the product rounded to a float before the truncation (0.7 s gives 7 turns, where a
    double product would give 6); the place is `turns × step + middle`, z the same. The height stays the middle's;
  - `Creature` 0x63BAD0: its body's place (+0x78 of the 3D body, `[this+0x160]+0x58`) moved on along its yaw (+0x84,
    x = sin y · d, z = −cos y · d, `fsin` / `fcos` unrounded, each product rounded) by d = its speed (+0x4838,
    `CurrentSpeed`, units a second) × the time (0x63BADC..0x63BB2B), at the ground there plus its drawn size (+0x90)
    × 15 (0x63BB37..0x63BB88). The ground is `GetAltitude` of the point truncated to map coordinates, each part
    `__ftol((p × 65536) × 0.1f)` (0x63BB47..0x63BB6A); the aim keeps the untruncated x and z.
  - Oracle (2026-10-20, both functions run natively with the villager, the creature and its body as buffers, the
    height and the ground stubbed, 100 ms a turn): 8 villagers (2 in a final state) and 8 creatures (2 standing),
    every bit as those orders of float steps.
  - openblack (2026-10-20): `creature_throw::VillagerAfter` and `creature_throw::CreatureAfter`, through
    `creature_throw::AimAhead`, in `CreatureMindSystem`'s order to the hands. The villager's step is its
    `WallHug::step` (the whole-unit step of its straight walk), its top state's row `state_info::IsFinal`, the turns
    a second `1000 / game_clock::MsPerTurn()`; the creature's speed and heading are its `CreatureLocomotion`'s
    (heading = −yaw; a creature whose walk has not started stands still), its height `object_measures::Height`
    (drawn size × 15), the ground the terrain's height at the truncated point. Anything else keeps the still aim.
- **The aim at a point (`THROW_AT_POS`)** is the sub-action's own point (+0x3C of its entry), as given. **Into a
  store (`THROW_IN_PILE`)** the point is the store's resource place on the ground (`GetResourcePos`, vt+0x8CC), and
  `GetApproximateThrowTime(12.0)` is called and dropped.
- **The launch.** `ThrowAtPointGivenTime` keeps the point and the time (+0x5720, +0x572C), refuses a target nearer
  than 0.66 of the standing height along the ground, and turns first when it is more than 0.01 off. As the hand lets
  go (`DoThrowingAction`, 0x48D0A2..0x48D132), the velocity is: h = (t · t) · 0.5; the fall f = h · −9.81f; the way
  (target − hand) less (0, f, 0); times r = 1.0f / t (`fdiv`), each component (`fmul`). Oracle (2026-10-18, those
  instructions run natively on 40 cases): every bit as that order of float steps; our earlier order (the fall as
  −4.905 · t · t, the way divided by t) differs in a last bit in 25 of the 40.
- Nothing else calls `ThrowAtPointGivenTime`. Two player actions of `PlayerAction` (0x653670, 0x653970, set up at
  0x64EC52 / 0x64EDFE) use the same constant with the **3-D** distance for what the player's hand throws at a point;
  they are not creature throws.

### Kicking and stomping a thing

✅ read in the v1.42 exe (2026-10-20). The plan actions KickTree and Kick kick the thing; Stomp knocks at it. Neither
fells a tree: `FelledTree::Create` 0x5116A0 has one caller, `Villager::ForesterChopsTree`.

- `CreatureAgenda::ConstructSubActionsForKickTree` 0x4C09D0 and `ConstructSubActionsForKick` 0x4B6C80 queue the same
  three sub-actions: GO_NEAR_OBJECT (0x3D, `SubArgumentObjectAndFloat` 0.0, gaze `LookWhileGoingTowardsObject`),
  TURN_TO_FACE_OBJECT (6, 0.1 = 0x3DCCCCCD, gaze `LookAtObjectArgument`) and KICK_OBJECT (0x6F, `SubArgumentObject`,
  gaze `LookAtObjectArgument`); Kick fails "no object to kick" without one (0x4B6E07). `KickBallAround` 0x4BFBE0 queues
  the same steps, again.
- The sub-action table (built in `crt_xc_fn_CreatureSubAction_004F8D70`, entries of 0x90 bytes, the name at +0, the
  two functions at +0x50 and +0x60): KickObject (name 0xBE4A08) → sub_507CE0 (0xBE4A68), which starts
  `LH3DCreature::StartKickingAction` 0x480C00 (anim 0x75, `ValidateKickPositions`, state 0x12); Stomp (name
  0xBE1528, sub-state 0x11) → sub_502830 (0xBE1578) and sub_502B00 (0xBE1588). sub_502830 starts the knock-down
  blended action (`StartBlendedAction(thing, 0x10)` 0x502AE8).
- The kick's contact, `LH3DCreature::DoKickingAction` 0x4887A0: a sound by kind (a tree not in the physics, flag 0x40
  of +0x24: `SoundTag` of `GetRandomSample(0x88, 3)` at the kick point, 0x488A91..0x488ACE, as the grab's creak); then
  a home (`IsAbode` vt +0x208) takes the fragment mesh's impacts and `Abode::ApplyEffectsDueToPhysicalDestruction`
  0x406640 with the creature's player; else a thing that `CanBecomeAPhysicsObject` (vt +0x7B0; `Tree`'s, 0x74B630,
  always 1) and is not immovable (+0x25 bit 0x10) flies: already in the physics, its velocity is set (+0x104,
  0x488F3E); else `Object::InitialisePhysics` 0x637480 (out of the map, vt +0x548) with the kick's velocity, the
  creature, and its player's leader interface (`GPlayer::GetLeaderInterfaceStatus` 0x64A9F0) as the thrower,
  `AdjustToGroundLevel` and `RaiseUntilNotIntersecting` 0x488FC4..0x488FCE; else a creature takes an effect (0x48909C,
  `EffectValues(3)` scaled by the heights). A kicked tree thus ends its flight as a thrown tree does
  (`Tree::EndPhysics`: replanted or a dead tree; [Trees](../world/trees.md)).
- The kick point (+0x4A28, read at 0x488817) is the creature's local point turned into the world by
  `GetPositionMatrix` 0x4EBE10. `LH3DCreature::ValidateKickPositions` 0x4EEA90 sets it once, at the start of a kick
  (`StartKickingAction`), unless already set (flag +0x5244, cleared by `ResetMorphAnims` 0x48D750 from `Init` and
  `MorphAnims`, so after every change of the body): the kicking animation (0x75) is posed at the time +0x4A38
  (`CAnim::FillBuffer` 0x860E00, `LH3DAnim::FinishTransform` 0x839F10) and `LH3DAnim::GetGraspPoint` 0x83A0E0 of the
  bone +0x51CC is the point; without the animation it is (0, 0, 0). `LH3DCreature::LoadBinary` reads the creature
  file's action points in order into +0x51C8 (right hand, 0x4EB6EB), +0x51CC (right foot, 0x4EB701), ... +0x4A34
  (pick-up time), +0x4A38 and +0x4A3C (the two catch times, 0x4EB804 / 0x4EB816), ... +0x4A8C (the kick time,
  0x4EB8D6). So the point is where the right foot grasps in the kicking animation at the first catch time, the moment
  the reaching hand takes what the player's hand gives it (`DoPickupFromHandAction` reads +0x4A38 too), not at the
  kick's own time. `ValidateCatchPositions` 0x4EE900 measures the catch hands the same way.
- The kick of a home (0x488B5F..0x488EC2; bw1-decomp's `DoKickingAction` has the same steps): with the creature's
  `Creature` (+0x4834) and `CanBeKickedByCreature` (vt +0x38C; `Abode`'s 0x4E3FA0: not `IsOnFire` 0x637CC0 and
  `IsBuilt` vt +0x890, the football's own `IsBuilt`), the home's `DestructionMesh` (+0x90) is made from its model
  when it has none (`FragMesh` ctor 0x7F6F00; no repair test, unlike `Abode::ReactToPhysicsImpact`), then, with the
  kick's way `kickDir` ((0, 1, −1) turned by the matrix, normalised when not zero, 0x4888BC..0x488973) and the size +0x90:
  - velocity 1 = kickDir + kickDir; the point P = position (+0x78) + (kick point − position) × 0.5;
  - `GameRand(11)`, `GameRand(100)`, `GameRand(100)` (0x488C95, 0x488CBF, 0x488CE9): t = (draw − 5) × 0.1 (0x8AC404
    = 0.1f) taken by `fild qword` of the unsigned difference with a zero high half, so a draw below 5 gives about
    4.3·10⁸; h1, h2 = draw × 0.01 (0x8C4B10);
  - the altitude under P (`LH3DIsland::GetAltitude` 0x803090 of `(x·65536)·0.1` and `(z·65536)·0.1`, truncated);
    P.y raised to it; the height above it at most size × 3.75 (0x8CF16C);
  - velocity 2 = (2 sin a, 0, −2 cos a) with a = t × 1.5707964 (0x8C78D8) + the heading (+0x84; the game faces
    (sin, −cos) of its heading: `LHPoint forward(sin(heading), 0, -cos(heading))` in bw1-decomp's `Creature3D.cpp`);
  - the radius: 0.6 (0x8C7BDC) × the home's `Get2DRadius` (vt +0x64) when that is below size × 3.75, else size × 3.75;
  - `FragMesh::Impact` 0x7F7D40 at (P.x, height × h1 + altitude, P.z) with velocity 1, then at (P.x, height × h2 +
    altitude, P.z) with velocity 2, both of that radius;
  - `WorkOutFractionRemaining` 0x7F7230 = 1: the mesh is deleted; else `SmashedByCreature` (+0x7C bit 0x20) set
    around `ApplyEffectsDueToPhysicalDestruction` 0x406640 (the `Creature`, its player).
  A home that is not built or burns takes nothing from the kick (and no random number is drawn).
- The knock-down's contact, `LH3DCreature::DoBlendedAction` 0x48BCC0 for a blended action other than the pick-up (0xE,
  0x48C031): a home takes the fragment impact and `ApplyEffectsDueToPhysicalDestruction` (0x48C192..0x48C367); a rock
  (`IsRock` vt +0x1F0, 0x48C371) is split in two with its sound; a living thing (`IsLiving` vt +0x3C4, 0x48C448) cast
  to `Rock`, which it never is, the same (dead code); anything else, a tree and every mobile object among them (vt
  +0x208, +0x1F0, +0x3C4 all 0 for `Tree`, `MobileObject`, `Pot`, `Ball`, `OneOffSpellSeed`, `Scaffold`), takes
  nothing.
- The knock-down's end, sub_502B00 (Stomp's second function): while the body still acts and the object action's
  status is 3, if the thing is in the map (`IsObjectInMap` vt +0x178) and living (`IsLiving` vt +0x3C4:
  `GameThingWithPos::IsLiving` 0x402350 returns 0, `Living::IsLiving` 0x417460 for villagers, animals and creatures),
  `ApplySingleEffect` (vt +0x604; `Object`'s 0x637FF0, `Creature`'s 0x479020) of type 1, strength 1.0, by the
  sub-action, at its position (0x502B88..0x502B98). Then, on the agenda's last sub-action (sub_4792A0) and with the
  thing's life (`GetLife` vt +0x11C, +0x48) at 0 or less, sub_4794D0 counts it by its kind in the agenda (+0x16C,
  +0x170, +0x174) and +0x658 is set. So a tree or a mobile object a creature stomps on stays as it was.

### Emergencies

Each turn, after the turn's own rules and before it decides, `Creature::ProcessState` (0x47358C) calls the emergency
step `fn_0047CFB0` 0x47CFB0, its only caller (`xref.py callers`). Read in v1.42 (raffclar's #544 reading, checked):
it skips while an emergency already runs (creature +0x10F4, 0x47CFB8), under a script (+0x25 bit 4, 0x47CFC9), with a
forced plan (mind +0x1C14, 0x47CFD9; `CommandActivityAndAction` 0x4C4580 sets it after its `ForceActivityAndForceAction`, 0x4C45AD; the forcing itself, 0x4C4450, does not) or in a fight (`IsFighting`,
0x47CFE7). Then:

- **On fire** (`IsOnFire`, 0x47CFF6): the run speed is set first (`LH3DCreature::SetRequiredSpeed`, info +0x258, at
  0x47D01B). With the water miracle (22) seen more than 0.6 (0x8C7BDC) of the times it needs, more than all counting
  (0x47D049..0x47D08A; a share that is no number fails), and its body able to pay for it (`HasEnoughEnergyToCastSpell`,
  0x47D090): `ForceActivityAndForceAction`(15 RestoreHealth, no activity, 240 CastMagicWater, its belief about itself);
  else, the leash on, `GLeashStatus::SetOn(creature, 0)` takes it off (0x47D0B0..0x47D0C8), and 55 DrinkFromTheSea
  the same way (0x47D0E4). The stop that the forcing does sets a player's creature that is not the computer's back to
  its walk speed (`StopWhatIAmDoing`, info +0x254, 0x475AD3..0x475AFD), so only a creature of no player or of the
  computer runs (raffclar's vanilla bug row 136, kept). When the forced plan stands (valid, its sub-actions under way,
  0x47D0FB..0x47D11D) the emergency runs (+0x10F4 and +0x1C14 set) and the step ends; else it goes on to the town.
- **Badly hurt** (not on fire; life, vt +0x11C, below 0.4, 0x8C7A44): `SetCheatDominantDesire`(15 RestoreHealth, 10.0,
  1, 0x47D15B..0x47D163) and the emergency runs (+0x10F4 only) and the step ends.
- **Fire in the nearest town** (`MapCoords::GetNearestTown(150.0)`, 0x47D1E3..0x47D1EB): knowing water
  (`KnowsAction(1, 22)`), the share, at most 1, above 0.6, able to pay, compassion (1) activated, its suppression
  (mind +0xB4) 0 and anger (+0x158) below compassion (+0x154): forced (1 Compassion, its belief about the building's
  town, 238 PutOutFireWithMagicWater, its belief about the building) on `Town::GetMostImportantAbodeOnFire` 0x4E4D00
  (bw1-decomp `CreatureMentalMisc.cpp:32`; the first burning abode of the town's newest-first list, +0x754 then
  +0x9C, that is a storage pit, `StoragePit::IsStoragePit` 0x4E4990 = built, vt +0x890, or a town centre, vt +0x1E0;
  else the last burning abode). The emergency and the forced flag are set with no test of the plan
  (0x47D2A2..0x47D2AE; raffclar's vanilla bug row 137, kept).

`StopWhatIAmDoing` clears both (+0x1C14 at 0x475C1F, +0x10F4 at 0x475C5C); the reactions read +0x10F4 (0x4F290E).

### Sources

- The v1.42 `runblack.exe` through the xref index, the disassembly and the oracle (the field, tree and pit tests);
  bw1-decomp for names.
- `Scripts\info.dat` (the frozen copy): the desire, source, action and species tables.

## openblack

Status: the planner, the completion of a plan, the sleep, sit, thirst, faint and drink rules and the end of a meal are
ported; storage pits, fields and trees are eaten from where they stand, while fish farms and the grain are not yet
food targets; some forced activities of a creature with no plan are
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
- A leash tied to a town's desire flag (`TownDesireFlag`) holds the creature to the flag's desire, no longer choosing
  (`creature_town_compassion::TiedToFlag` from `LeashSystem::TieTo`; a desire whose row has no first action, none and
  free), and holds off the healing while the leash is on and tied to it (`TownCompassionActions`); only the leash key's
  untie (and TOGGLE_LEASH) frees it (`UntiedFromFlag` from `LeashSystem::Carry`), as the original's tie and toggle
  ([the state](#the-draws-finishing-and-the-state)). Taken from raffclar's #990, refitted. Tests
  `CreatureTownCompassion.ALeashTiedToADesireFlagHoldsItToThatDesireUntilTheLeashKeyUntiesIt`,
  `LeashSystemTest.TiedToATownsDesireFlagItHelpsWithThatDesireUntilTheLeashKeyUntiesIt`.
- The act-on object is `creature_planner::BestActOn` over `mind_detail::TownMembers`, with no distance limit and the
  confinement's `outOfReach`. A plan names its town in `Plan::activityObject` (his `about`), and the town's belief for
  the decision trees is his.
- What still differs (see Pending): the goal and the members come from the world in the registry's order, not from the
  creature's beliefs; one draw per planning rather than one per slot-0 look-up; "being compassionate" is his test; the town belief's attributes are his
  numbers. The belief in the creature's player is `creature_town_compassion::ReligiousBeliefIn`: 0 for a creature of
  no player, as the original's.

### The planner in openblack

`creature_planner` (`src/Creature/CreaturePlanner.h`):

- `Priority` multiplies in the game's order; `DistancePriority` as the original; `Best` and `Choose` as the best plan
  and the switch, with no other minimum; `MayPlan` the gates (ToAttractPlayersAttention as
  `PlanFacts::ignoredByPlayer`, `creature_attention::IgnoredByPlayer` on the turns not looked at; not ToFollowPlayer).
- `PlanDesire` the goal path, `GoalSearch` saying whether the desire looks for a goal (`DesireRules::searchesForGoal`
  from the file) and acts on it (Anger); `PlanWithoutGoal` the path without a goal. A thing's worth as a goal
  (`ObjectCandidate::goalUsefulness`) comes from its desire's goal tree, its worth to act on (`usefulness`) from the
  act-on tree, as `GetActivityObjectUsefulness` and `GetObjectToActOnUsefulness` read them
  ([The two trees of a desire](mind.md#the-two-trees-of-a-desire)); `Replaces` keeps the later action
  of equal plans across openblack's groups of actions by target.
- On the path without a goal, an action done to no thing is done to the creature itself (`PlanWithoutGoal`'s
  `itself`, the creature's entity), worth the default 0.1, as the original's act-on chooser gives the creature's own
  belief when the action info's acts-on word (memory +0xA4, file row +0x94) is 0 (fn_004F10A0 0x4F1187..0x4F11A1;
  raffclar #547). So the leash's "plan on another thing" test and the learning context see the creature, as in the
  original. Unverified against v1.42.
- Whether an action is done to a thing still comes from openblack's executor (a target or none), not from the
  acts-on word: 7 actions act on a thing by info.dat but have no target here (Poo, PullSillyFaces, SitDown,
  BeSad, ShowImpressiveAnimation, GoHome, RunHome), and their thing needs the per-action object filter of the agenda's
  action table, not ported (Pending). Every executor with a target acts on a thing by info.dat. BeFrightenedOnTheSpot's
  row has no filter of its own, so its thing is the one the Fear desire accepts, something that frightens it
  (`Target::Frightening`, as RunAwayFromObject); its agenda never uses the thing (`Executor::thingUnused`), so a fright
  forced with none, as after a fizzled miracle, is still made.
- ExamineByLooking is done only to a thing it can look over (`Target::Examinable`: a villager, animal, tree, dead
  tree, mobile static or object, pot, ball, totem, building, temple, worship site, feature, animated static, flowers
  or forest; never a creature, field, fish farm or miracle), raffclar's #547 kinds. Holding something,
  ExamineByPickingUp examines what it holds with no pick-up. Both unverified against v1.42.
- How useful a thing is to act on is `ActOnUsefulness`, as `GetObjectToActOnUsefulness`: the tree's usefulness ×
  `DistancePriority` × `ActOnFactor` (play and curiosity, from `ActOnFacts`), 0 when `BeyondKeptArea`; pinned to the
  original's numbers in `CreaturePlanner.AThingIsWorthActingOnForItsUsefulnessTheDistanceAndTheDesiresFactor`. It
  ranks and scores the thing in `PlanWithoutGoal` and the stroke repeat, weighs a gift in `UseGift` (held), and the
  town member in `BestActOn` (for compassion the factor is 1, so that one is as before). `CreatureMindLearning.cpp`'s `ActOnCandidate` fills the
  facts: a toy (`Ball`, a mobile static with a toy mesh), a villager whose final state's exit is the football one
  (`ecs::villager::k_ExitFootball`), a villager whose top state's `state_info::Interesting` is not 6 and not a known
  skill; a part of the temple is a `CitadelPartBuild`: the heart, a worship site and a site's altar, whose vtables all
  take `CitadelPart::IsCitadelPart` 0x464B00 (1). The spawner's Planner table shows the goal's worth and what the
  plan's thing counted for. The reactions' reader of the same function, `CreatureMindSystem::ObjectToActOnUsefulness`
  (the look-over of another creature, landed with the reactions port), takes the same `BeyondKeptArea` and
  `ActOnFactor`; it already measured the distance from where the player's leash is tied, which the planner's
  `ActOnCandidate` does not yet (Pending).
- Plans are kept across passes (lost when their desire is suppressed for longer or what they rest on is gone, which
  also starts the pass again); the pass is `StartPass` / `NextGoals` / `PassOver`; the walk back is forced at
  `k_ForcedScore`; a stage change gives the plan up (`GiveUpPlan`); `CREATURE_SET_AGENDA_PRIORITY` writes the current
  plan's priority.
- The mind file's increase times are loaded as they are, as the original's.

### The fart in openblack

- `creature_plan_actions` has a `Fart` executor beside `Poo`, with the poo's build (`Build::Poo`), target and
  activity, so the planner weighs it as the original does ([The poo desire's fart](#the-poo-desires-fart)); before,
  the planner skipped the row for want of an executor and the poo desire always pooed. Tests:
  `CreatureFart.TheFartIsCarriedOutWithThePoosAgenda`, `CreatureFart.OfAPooAndAFartWantedAsMuchTheFartIsChosen`,
  `CreatureFart.AFartJustDoneGivesWayToAPoo`.
- Both are done on the spot, as openblack's poo was (target `None`): the original's poo and fart need an object, go
  near it and turn away from it (queued, see Pending).
- The poo's moment (`Effect::Poo`, where openblack drops the lump) is a fart when the plan carried out is the fart
  (`creature_fart::Farts` on `mind_detail::CurrentAction`): `Fart` in `CreatureMindSystem.cpp` takes nothing from the
  body, its desire or its count, and adds 15 puffs of `PuffKind::Dust` to the physics' pool from where the creature
  stands, each `creature_fart::PuffSize` (4 × `creature_pose::DrawnSize`) and grey 0x40808080, flying at
  `creature_fart::PuffVelocities`: the cloud's size 15 × the drawn size, the body's yaw (`mind_detail::BodyYaw`) and
  45 `GameFloatRand` draws, z then y then x for each puff. Tests: `CreatureFart.TheCloudIsBlownBehindTheBodyAsTheOriginalBlowsIt`
  (the oracle's velocities, bit for bit), `CreatureFart.TheFirstDrawOfAPuffGoesAlongZAndTheLastAlongX`,
  `CreatureFart.EachPuffIsFourTimesTheDrawnSize`, `CreatureFart.ThePooMomentIsAFartOnlyWhenThePlanIsTheFart`.
- The fart's smoke is not made yet (queued, see Pending): no smoke puffs at the anus, and the smoke's 20 CRT draws are
  missing.
- An evil creature's fart makes reaction 25 (`Reaction::Fainting`) with `effects::reactions::CreateReaction(creature,
  Fainting, its owner, false)`, after the puffs, when `creature_fart::MakesFaint` of the body's alignment
  (`creature_pose::TargetMorph(...).evilGood`, the motes' own) holds: below −0.5, or no number. It is offered as every
  reaction is (spread at once, then by the reaction step's walk), grows and ends as the type table says. No Living
  takes it up yet: the villagers have no fainting slot row and the creatures' handler does not take kind 25 (both
  queued, see Pending). Test: `CreatureFart.OnlyABodyMoreEvilThanHalfMakesThoseNearItFaint`. Debug: the body panel's
  readout "Fainting reactions it made".
- Debug: the creature window's body panel has a **Fart** button (the plan forced under the poo desire) and the
  readout "Its plan is a fart".

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

openblack's idle mind used to choose an activity of its own (see a need to, show a desire or a stroke, look
something over, throw it about or hurl it, put down what it held, sit, hang around, be idle) whenever its agenda ran
out. That was openblack's stand-in: it was retired, then removed with its debug switch and its testbed twin on
2026-10-22. A creature with no plan does nothing of its own and draws nothing.

- Of the ways an activity starts with no plan, openblack has the adoption, the cut scene's look at the camera
  (`creature_mind::LookAtCameraInCutScene`, forced in `CreatureMindSystem::ProcessTurn` while a script's wide screen
  is on), the emergencies (on fire, badly hurt, a fire in the nearest town: [Emergencies in
  openblack](#emergencies-in-openblack)), fainting, the walk back and mimicry.
- Two things only the removed idle policy did have no counterpart: putting down what the creature holds, and the
  happy or sad face just after a stroke or slap (the original keeps a next action after a young creature's plain
  feedback instead; the stroke no longer clears the wait before the planner shows a desire).
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
  and rest stay in the body's sleep step (`Rest::Asleep`). A script's static action 38 (SET_SCRIPT_STATE: Land 4's Ogre
  in `Land4Ogre`, Land 5's closing `VillageWavingSequence`) is such a step, so it rests the body on each of its held
  turns as the original's `StaticAction` stage 1 0x500AC0 does for clip 38, from the turn after the start to the turn
  its end is asked for (test `CreatureScriptPlay.AScriptsSitRestsTheBodyEachTurnItIsHeld`).
- `creature_physiology::TickTurn`, `ShouldFaint`, `FaintGates`; `CreaturePhysiologySystem::Drink`: the thirst cuts the
  turns a second and then the product to whole numbers (`map_coords::FtoL`, the SSE2 path: a NaN or out-of-range
  product is 2^31 turns, a negative one wraps round as unsigned), and a cut to none parches the creature in one turn.
- A creature faints only from phase 5, owned by a player (not the neutral one) whose `PlayerMagic` player type
  (`magic::players::ReadMagicOf`, the player system's for a player without an entity) is not 2
  (`PlayerMagic::k_ComputerPlayerType`), and not controlled by a script (`ScriptHeld::controlledByScript`, the 0x400
  bit), the three reasons in the original's order.
- Drinking empties the thirst and sets the creature's temperature to 0 through the fire system
  (`FireSystemInterface::SetTemperature`), after which the mind suppresses the water desire for 20 s as before.
- Where it drinks (from raffclar's #533, unverified against v1.42): `mind_detail::DrinkPlaceOf` with the pure
  `creature_drink::NearestWaterPoint` (by the game's distance in metres, `gutils::GetDistanceInMetres`, as 0x47D860
  measures with `GUtils::GetDistanceInMetres`), `StandingPoint`, `Drink` and `NoPlaceFound`. Our `DrinkWaypoint` keeps the
  order it was made in; the stand test is `land_avoid::IsPosValid` / `NearestValid` at 7.1 (the spiral above); the
  coast and the sea come from our map of the land (`creature_exploration_land::FindPlace`, not preferring unvisited
  regions; the low corner of the map when there is none, as the explorations do); in water is `sea_cells::IsWater`.
  The planner (`PlanAgenda`, for its own and a script's plans) and the debug drink take it; a failed place suppresses
  Water and GetColder for 30 s. The original has one search: the builder 0x4A15F0 is reached only through the action
  table (+0x20, called at 0x4D36CB), from the planner (0x4734F8), the forced plans (0x4C453B) and the current plan
  (0x4F15C7).
- A script can tell the creature to go home or to pray at its temple when the planner would not choose them: PR #533's
  home part is covered by our forced plan (`Situation::forced`, 0ba3f5a83): a plan forced on the creature needs only
  its home, as read in the exe.
- What it moved in the checks: on Land 1 the creature sleeps on the spot from turn 32 and, by the day's length, has
  not slept long enough at turn 119, where the old rule woke it; so it does not get up and think at turn 137 (three
  fewer synchronised draws there), and the fidelity run, the cycle and the hand demos move from turn 137, the
  land-view shots too. The temple shot `t120_200` and the testbed scenario `creature.walks_in_temple_pen` move with it
  (that scenario's random part from turn 130, 142 draws against 147). With the mind file's needs read back, a loaded
  creature on Land 1 sleeps longer (its sleep sequence's loop is still playing at turn 105, where the species' defaults
  had it end), so in the temple shots at frame 700 (entered at turns 120 and 120,200) it still lies asleep in front of
  the cave.

### The lesson actions in openblack

**The creature's height.** Every builder below that goes by the creature's height calls `Creature::GetHeight` (vt
+0x42C, 0x477F50: the body's size, clamped to 0.05..4 by `LH3DCreature::SetSize`, times 15). Ours is
`object_measures::Height` (`ecs::object::GetHeight`: `creature_morph::ClampScale` of the drawn size, times 15; at least
0.75, and no mesh box is read), set in `CreatureMindSystem::PlanAgenda` for every build of
`creature_plan_actions::WalksByItsHeight`. The builders read it at: InspectCreature 0x4A2A6F (4 ×),
DanceOutsideWorshipSite 0x4B8A71 / 0x4B8A92 (min(6 ×, 50)), DanceAroundArtefact 0x4B8CD1 / 0x4B8CF2 (min(4 ×, 50)),
BringHome 0x4A583D / 0x4A5860 (min(5, h)), BringFoodHome 0x4AED5C, TakeFoodFromFieldHome 0x4BA1EF / 0x4BA3D0,
StealObjectAndTakeToTown 0x4BAD48 (3 ×), StealFoodFromStoragePit 0x4C1350 and StealWoodFromStoragePit 0x4C16A0. Until
2026-10-20 those nine builds of ours got no height (0), so they went to within 0 of where they went.

`creature_mind::SleepAtHome`, `WaveAt`, `BeFrightenedOnTheSpot`, `DeadForever` and `DiePermanently` (DeadForever's two
steps, the wait 999999 s, since 2026-10-29; test `CreatureActionAgendas.DyingForGoodIsLyingDeadForGoodForLonger`)
(`Creature/CreatureActionAgendas`),
and `creature_mind::PutDownHeld` for the put-down, reached by the executors of rows SleepAtHome, WaveAtObject,
BeFrightenedOnTheSpot, PutDown, DeadForever and DiePermanently (`creature_plan_actions`): a script's CREATURE_DO_ACTION, the planner
where a desire lists the row, and the other forced plans. The sleep at home is the builder above step for step: the
yawn half the time (sleepy eyes), the walk home within min(5, height) looking where it goes, the turn down the slope,
the sleep until rested with closed eyes, and the daze with sleepy eyes and the grimace. Its height is the creature's own
(`creature_plan_actions::WalksByItsHeight`). The planner chooses it only as it chooses to pray at the temple: with a temple of
its player and less than 1000 m from home (`creature_mind::NotTooFarFromHome`); a plan forced on it (`Situation::forced`)
is not asked, but still needs a home. The wave goes near the creature waved at (`Target::Creature`, the row's object test),
smiling and looking where it goes, turns to face it 1.4 s smiling and watching it, then waves with no face of its own,
watching it with fluttering eyelids (`Step::flutterEyelids`). The fright on the spot is a start with wide eyes and no face, a wait of 3 s with the
frightened face, and another start with wide eyes (`Step::heldEyes`). The put-down is our put-down order (`ObjectOrder::Kind::PutDown`, clip 97, the
creature's own put-down when told to), as the original's DISCARD sub-action with clip 97 plays the put-down; the
hand's speed the original passes when it lets go is not read (as for every put-down of ours). A discard order
(`ObjectOrder::Kind::Discard`) chooses by its animation, as the original's DISCARD does: the put-down's (97) lays the
thing down where the creature stands, any other (the toss's, 95) tosses it away (`creature_object_actions::DiscardPutsDown`;
the original's discard is always given its animation, 95, 96 or 97, and its start fails without one, 0x5001F0 →
fn_00484E60: openblack's examine and toy agendas leave the order's animation at 0 for the toss's;
before, every discard tossed, so a creature bringing a thing home or giving one up for its player threw it instead of
putting it down). Dead for good is the original's two steps (fixed 2026-10-29: it was one faint
that lay for the whole 864000 s from its start, then got up): a faint step (`Step::faint`: the reaction to a death,
out cold) done on the first turn it lies (`Step::liesOn`, as FAINT 1 returns done while `IsActuallyDead`) and not got
up, then a wait of 864000 s that keeps it lying, unconscious, eyes closed and looking about at nothing
(`Step::staysDown` on the wait: WAIT has no look function), begun on the turn after it lies, as the original's WAIT
(0x503500 counts down each turn the body is free or lies dead, so lying it is the wait's seconds). Tests
`CreatureActionAgendas.FrightenedPuttingDownAndLyingDead`, `CreatureActionAgendas.LyingDeadForGoodFaintsAndStaysDown`.
Holding a thing up (`creature_mind::HoldObject`, row HoldObject with `Target::Pickable`) is
the builder above step for step; the plays are counted by the thing's object type (`ecs::map_cells::TypeOf`) in
the learning's `ObjectTypeCounts::heldPlays`, counted as the agenda is made (`CreatureMindSystem::PlanAgenda`), and
kept in the mind file as the original keeps them (see [mind.md](mind.md#the-counts-by-object-type)); a thing with no
object type (not an object of the map) is not played with, as the original's cast to `Object` fails for it. Without a camera it does not turn. Sleeping by a thing (`creature_mind::SleepByObject`, row SleepByObject with
`Target::Anything`) looks for the clear place about the thing in `CreatureMindSystem::PlanAgenda` (the same
`FindClearArea` as the sleep on the spot), then follows the builder, else the sleep at home of `SleepOnTheSpot`; the
planner asks `MaySleepOnTheSpot` and `Situation::nothingFrighteningNear` (from the creature, vb#142), a forced plan
neither. Tests: `test/creature/test_creature_action_agendas.cpp`. Pointing at a thing for a script is row
PointAtObject (`creature_mind::PointAtThing`), and a script's sit at a place is row SitDown with the script's object.
Tiredness lists the three sleeps, illness the sleep at home, fear the fright on the spot; no desire lists the hold, the
put-down, the wave, the point or lying dead, so only forced plans reach them.

### Eating in openblack

- The rules are the pure functions of `creature_eating` (`src/Creature/CreatureEating`): `CanBeEaten`,
  `PortionAmount`, `PortionScale`, `PortionPoint`, `JudgeMeal`, `HungerAfterMeal`, `IsFoodPileOutsideStoragePit`,
  `StoragePitHasFood`, `FieldHasFood`, `TreeTallEnough`, with their tests (`test_creature_eating.cpp`).
  `StoragePitHasFood`, `FieldHasFood` and `TreeTallEnough` (with the creature's drawn size) are the planner's
  `Target::StoragePitWithFood`, `Target::FieldWithFood` and `Target::TreeBigEnough` (below), beside `Target::BuiltStoragePit` (a
  storage pit that is fully built), the things a handful is taken from and given to.
- The end of a meal is wired: the energy as the eating starts (`CreatureObjectActionSystem::Start`,
  `CreaturePhysiologySystem::Eat`, filling up to the drawn size); at the eat moment the hand lets go and the thing is
  hidden off the land (`components::NotDrawn`); when the animation ends it is used up and judged
  (`CreatureObjectActionSystem::EndMeal`, `JudgeMeal`; a mushroom is a mobile object of the magic mushroom or toadstool
  class or the magic mushroom feature; poisoned as the original), and the mind told
  (`CreatureMindSystemInterface::EndMeal`: the type's eaten or not-food count in
  `creature_mind_model::Learnt::objectTypes`, read from and written back to the mind file; the hunger by
  `HungerAfterMeal`, or Hunger held back 20 s and Puke). A meal marked indestructible, or in a script under the
  script's widescreen bars (`creature_eating::RefusesMeal`), is refused before that: not used up, the mind told
  `MealEnd::Refused` (held back and Puke, nothing counted), and let go of where it is, drawn again, thrown by the
  creature at no speed (`ReleaseHeld` at a target); tests `CreatureEatingMeal.AThingMarkedIndestructibleOrInAScriptUnderTheWidescreenBarsMayNotBeEaten`,
  `CreatureObjectActionSystemTest.AMealMarkedIndestructibleIsSpatOutWholeAndLetGoOf`. Mind +0xFBC is not kept by
  openblack (the not-food branch clears it too).
- The food choosers take what the creature can eat (`mind_detail::CanBeEaten`, through `Accepts`' Food and LiveFood
  and `NearestFood`), not what has a food value: an object it can pick up, a mobile static that is not a toy (a dead tree
  among them), never a creature (testbed `mind.hungry_takes_what_it_may_eat`); storage pits, fields and trees are eaten
  from where they stand (below); the fish farms and the grain wait on the food-targets row (B4g).
- Eating alive (row 11) is the original's agenda (`creature_mind::EatAlive`, `creature_plan_actions::Build::EatAlive`,
  `Situation::holdingEatable`): the draw of two and the hungry gesture on 0, the pick-up, then the eat, with no face
  of their own. Before 2026-10-28 it was built as eating after examining (no draw, an examine step, the curiosity
  face). Test `CreatureIdleMind.EatingAliveIsHungryOneTimeInTwoThenPickingUpAndEating`. Land 4's ogre quest
  (`Land4Ogre`) forces it; the hungry creature's planner chooses it for villagers and animals. Stomping and eating
  (row 13, which Lethys's creature is forced to do to cattle in So You Couldn't Bear to Be Without Your Creature) is
  the original's agenda since 2026-10-29 (`creature_mind::StompAndEat`, `Build::StompAndEat`, in `k_ForcedOnly` as no
  desire lists the row): unless it holds a thing it can eat, the knock-down with the anger face and the pick-up, each
  with its head on the thing on the way (`Gaze::goingTowards`), then the eat as the main step, with no draw; the
  knock-down crushes the living thing (the kicking and stomping, below). Test
  `CreatureIdleMind.StompingAndEatingKnocksTheThingDownAngryThenPicksItUpAndEatsIt`. Stomp (row 18) still has no
  1-in-5 angry gesture ([Pending](#pending)).
- Eating from a magic food pile is wired as the original's: the plan's object is a pile that passes
  `IsAFoodPileOutsideStoragePit` (`Target::FoodPile`, the nearest town by `map_cells::GetNearestTown` with no limit,
  its pit by `town_queries::GetStoragePit`, the distance by `GetDistanceInMetres` of the two MapCoords); its agenda goes
  to the pile's point (`creature_eating::PileApproach`), turns to face it, then `ObjectOrder::Kind::PickUpPortion`
  (`CreatureObjectActionSystem::PickUpPortion`: a `WheatInHand` pot (row 17) of `PortionAmount` at `PortionPoint`, no
  more than the source holds of the row's own resource (`creature_eating::PortionResource`), the range being the pick-up test's reach, scaled to `PortionScale`; then the ordinary pick-up of it) and the eating of
  what it holds; the source loses what the portion holds (`object_resources::RemoveResource`). Testbed
  `mind.hungry_eats_from_magic_food`.
- Eating from a storage pit, a field or a tree: `creature_mind::EatFromStoragePit` / `EatFromField` / `EatFromTree`
  (`CreatureIdleMind`), through `creature_plan_actions::Build::EatFromStoragePit` / `EatFromField` / `EatFromTree`
  (`Situation::pitPile`, `pitPilePoint`, `height`, `holdingEatable`, and `portionApproach` for the tree); the turn is
  `TurnToFaceObject` held `creature_eating::k_FaceSourceSeconds`, the pit's distance `k_PitApproachHeights`; the faces
  are `Cue::None`; sub-action 0x6C is `Step::Kind::ClearObjectToActOn`, done as it starts with no face and no draw
  (`Commands::clearObjectToActOn`); the portion's row is `ObjectOrder::portion` (`PotInfo::WheatInHand` by default,
  `TreeFruitInHand` for a tree), passed to `CreatureObjectActionSystemInterface::PickUpPortion`. Their executors are
  `EatFromStoragePit`, `EatFromContainer` (the field) and `EatFromTree`, on `Target::StoragePitWithFood`
  (`creature_eating::StoragePitHasFood`: the pit's `Get2DRadius`, the drawn size, `StoragePit::foodPile`, the pile's
  `Unavailable` for the flag bit, its `Pot::amount`), `FieldWithFood` (`FieldHasFood(growth, food)`) and
  `TreeBigEnough` (`TreeTallEnough(Height, drawn size)`), found by `Gather` among the abodes, fields and trees.
  `PlanAgenda` fills the situation for a chosen and a forced plan alike (`SituateEatingFrom`: the pile and its point;
  the hand's eatable thing by `CanBeEaten`; the tree's `PileApproach`; the creature's height through
  `WalksByItsHeight`). `clearObjectToActOn` makes the plan act on the creature itself
  (`creature_planner::ActOnItself`): the original's 0x6C stores the creature's belief about itself, which every
  creature holds (the planner gives each on-the-spot plan that belief, fn_004F10A0 at 0x4F118E, and
  `CreaturePlan::IsValid` 0x4F12E0 fails a plan with none). A field's or tree's portion holds no food, as the
  original's ([vanilla bug](../project/progress/vanilla_bugs/creature.md)). Testbeds `mind.eats_from_store`,
  `mind.eats_from_field`, `mind.eats_from_tree`; the creature spawner's planner readout says what the plan acts on.
  Ported from raffclar's PR #507, unverified against v1.42.
- The planner gives each agenda the creature's measures that the original reads as it makes that agenda: the height
  (`object_measures::Height`) to the actions of `creature_plan_actions::WalksByItsHeight` (rest, home, temple, and
  eating from a storage pit or a field, which read `GetHeight` at 0x4A2EFA and 0x4A318A), and the two radii to the
  actions of `creature_plan_actions::ApproachesByRadii` (a food pile at 0x4AE4E6, a tree at 0x4A61DC..0x4A61F9, both
  `(the thing's Get2DRadius + the creature's) × 1.2f`). Before, the storage pit and field agendas went to within 0 and
  the tree's to within 0.

### The map of the land in openblack

Ported from raffclar's creature-exploration-map work, unverified against v1.42. When the game's first land and its
towns are ready, the land is cut into 64 x 64 regions of 8 x 8 cells (`creature_exploration::RegionMap`, made in
`Game::StartNewLand` and kept for the whole game by `creature_exploration::GameRegions`). As the original, which works
the map out only in `GGame::Init` (`PrecalculateMap` 0x4DF9C0, called at 0x54FC30 and nowhere else; a land change
through `GGame::LoadMap` 0x552F40 does not call it), every later land keeps the first land's regions; openblack's own
testbed makes its own ([countries.md](../project/progress/terrain/countries.md)). Each region
records its highest cell, its first coast and sea cells, whether a town building stands in it, and whether it is a hill
(higher than half the highest region, with no higher region next to it). Each turn the hand does not hold it, a creature
notes the region it stands in (`CreatureExploration`). GoToHillAndLook, GoToHillAndSit, GoToHillAndWalkAlongRidge,
ExploreCoastline, ExploreTowns, SitDownOnBeach, LookAtMountains, LookOutToSea and ThrowInTheSea take their places from
the map (`CreatureMindSystem::SituateExploration`). The ridge walk counts as done at the hill, and the point at the
mountains counts as done when the creature points, before the rest of the agenda (`Effect::Completed`). A creature
below the third stage of growing up neither looks out to sea nor sits down on the beach: the two rows share the
predicate `IsMatureEnoughToLeaveHome` 0x4D2B80, the creature's phase (+0x1268) at least 3 (rows 192 and 218 of the code
table, written at 0x496D00 and 0x4977B2 by `_$E90`; see [casting.md](casting.md)). openblack marks both executors
`leavesHome` (`creature_plan_actions::k_LeaveHomeStage`). The predicate sits in the rows' +0x10 slot, which only the
planner (`fn_004F0D60` 0x4F0D91 from `FindBestActionForPlan`, and `CanCreatureCurrentlyPerformAction` 0x4F1B60) reads;
`ForceActivityAndForceAction` 0x4C44B0 never reaches it, so a forced look out to sea or sit on the beach is carried out
at any stage (ours: not asked of a plan with `Situation::forced`, 2026-10-19). SitDownOnBeach looks for its clear area
(`Creature::FindClearArea` 0x4DFBE0, width 4 × the 2D radius) with nothing living in it unless a script controls the
creature (`ConstructSubActionsForSitDownByBeach`, the third argument `!CONTROLLED_BY_SCRIPT`): the cell test
fn_006059A0 then lets any `Living` in the cell take it (0x605BBC..0x605BDC). Ours passes the same
(`mind_detail::FindClearArea(..., nothingLiving)`, `CellBlocked(..., livingBlocks)`). The cell test is the original's:
a temple's heart (`IsCitadelHeart` vt +0x220) always takes the cell; a tree, a field or a thing the creature must go
round (`CreatureMustAvoid` vt +0x7BC, `route_plan_world::CreatureMustAvoid`) takes it by its shapes within 5 of the
cell's middle; then the livings when asked. The shapes are the thing's collide data (`GetCollideData` vt +0x858): its
row of circles when it has one, else its one circle, each measured from the cell's middle (10x + 5, 0, 10z + 5), the
circles' centres at height 0, so the distance is 2D. Only the map-fixed classes have collide data:
`MultiMapFixed::GetCollideData` 0x401630 (abodes, features, animated statics, fields, mobile statics, dead and felled
trees, totems, worship sites, spell icons, the vortices...) and `SingleMapFixed::GetCollideData` 0x52EB30 (trees and
magic trees with the 0.3 circle of `Tree::CreateCollideData` 0x74C5F0, shields, prayer icons, script highlights), built
from the model (`NewCollide(LH3DObject)` 0x829390) or a point and radius (0x829590); a big forest releases its own
(`BigForest::CreateCollideData` 0x439580). Every Object class (`Object::GetCollideData` 0x419B30: villagers, animals,
creatures, mobile objects, pots and piles, street lanterns, balls) returns none, so it never takes a cell by a shape
however tall it is. Ours reads the collide data the map cells built on insert (`map_cells::CollideDataOf`), which keeps
the same classes (2026-10-27; before, every thing it had to go round with a model kept a circle from its model's box).
The nearest middle is chosen by `GUtils::GetDistanceInMetres` 0x74CD50 (the 16.16 table hypotenuse, 0x4DFF5C), strictly
nearer only, x outer and z inner, so of two middles the table puts equally near the first is kept: from (310.002, 306)
with width 10 and every cell clear, both (305, 305) and (315, 305) are 5.0970459 (`40A31B00`) away and the original
takes (305, 305) (oracle, the whole search run with the cell test answering clear), where the exact distance would take
(315, 305). Ours: `gutils::GetDistanceInMetres` (2026-10-27; before, the exact distance). The point is the MapCoords
the caller passes and the window's cells are its high words; each middle is a MapCoords at height 0: the 16-bit cell
word set, its metres (cell × 10, exact) plus R / 2, then × 65536 / 10 and truncated (0x4DFEC7..0x4DFF4B, the utility
functions' conversion, `map_coords::ToFixedGUtils`, not the setter's × 6553.6). The oracle gives 1990000 for cell 30
and width 7.3 (303.6499 m back, not 303.65) and 8537702 for cell 130 and width 5.5, where × 6553.6 gives 8537703. Ours:
`creature_cast_moves::ClearAreaMiddle`, the search measuring from `map_coords::FromMetres` of the point and giving the
middle back in metres (2026-10-27; before, the exact sum in metres). The debug tools show a creature's last search (the creature spawner's
Learning mind, "Clear-area search", recorded only while its switch is on): the width, the point searched about, what it
found and what it took, and each cell of the window that was not clear with the thing that kept it so and why
(`mind_detail::BlockerOf`, `components::CreatureClearAreaReadout`). The two sleeps pass it too. The sleep on the spot
(`ConstructSubActionsForSleepOnTheSpot`, bw1-decomp CreatureAction.cpp:609): a clear area 4 × its height about
itself; found, half the time a yawn (0x39), the walk to within its 2D radius, the sleep, the waking (0x3F, grimace);
none, `ConstructSubActionsForSleep`: the yawn half the time, home to within min(5, height), face down the slope, the
sleep and the waking. Ours: `creature_mind::SleepOnTheSpot`, the clear area from `PlanAgenda`. Only the waking pulls a face, the grimace (`SetFaceForActionGrimace` on 0x3F only, 0x49B5E7; the yawn, the walk and the turn down the slope have none), so only the waking's face draws its `GameRand(7)`. The sleep by an object
(`ConstructSubActionsForSleepByObject`) is `creature_mind::SleepByObject` (see "The agendas of the lesson actions"). Three quirks of the game are kept as they are:

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

### Rest and home in openblack

Ported from raffclar's creature rest and home work, unverified against v1.42. Nine actions of the game's table are
plan actions (`creature_mind` in `Creature/CreatureRestAndHome.h`), weighed by the planner for the desires that list
them and reached by a script's CREATURE_DO_ACTION:

- **RestToGetBetter** (row 59) faces down the slope and sleeps, mirrored half the time, with its eyes closed. It gets up
  only when its life is full, it is not tired at all and the rest has lasted more than 30 turns
  (`creature_mind::k_FewestRestTurns`). Each turn of the rest puts out its own fire, and the action counts as done once
  it is up. Then it looks about, dazed, with a grimace. While it rests its body heals as in sleep
  (`CreatureNeeds::Rest::Resting`); its energy is paced by the action's row, as for every plan.
- **GoHomeToRecover** (179) walks home to within its height, no more than 5, then rests the same way.
- **HealHimself** (58) casts the heal at itself from the casting pose with a grimace, held three seconds; then its life
  is full and its skin heals as 9600 counts of healing.
- **RestOnTheSpot** (178) waits 3 to 11 seconds, exhausted, looking at its feet.
- **SleepOnTheSpot** (313) is chosen only on a leash (worn, held or tied), with no temple of its player, or further
  than 140 m from home (`creature_mind::MaySleepOnTheSpot`, our own from the exe; `Situation::leashed` from
  `LeashSystemInterface::IsLeashed`). A plan forced on it skips the test (`Situation::forced`, set by
  `CreatureMindSystem::ForceActivity` unless `ForcedActivity::checkedAsChosen`, which only the stroke's do-it-again
  sets), as in the original. The creature window shows the rule's answer under its home.
- **GoHome** (162) walks home to within its height, no more than 5; it is chosen only further than 20 from home.
- **RunHome** (170) half the time is frightened first, with wide eyes, then runs to within two heights of home.
- **EnterCitadel** (205) walks home, turns to the heart with a smile, then walks to a stop short of the heart by 1.3
  times a third of its radius on the ground, found from where it stood (a vanilla bug, kept).
- **PrayAtCitadel** (319) walks home (counted as done on arrival), then plays the pray animation (221). It is chosen
  only with a temple and within 1000 of home; forced on it (a script's CREATURE_DO_ACTION), it prays with no temple
  and from any distance, as long as it has a home to walk to.
- **LookAtCitadel** (190) faces a temple, a worship site or the altar at a worship site's head 2 seconds, watches its
  top 3 seconds, one time in 3 points at it 2 seconds, amazed, then watches its foot 3 seconds and its top 2 seconds.
  The three are the temple parts of `Target::TemplePart`: in the original the citadel heart, the worship site, the
  creature pen and the worship totem (the altar) all give the citadel part belief type 4 (`CitadelPart`
  0x464AA0 at vt +0x10C of each), and the altar is a part of its own. The shipped lands make no pen as a thing of its
  own. The creature window shows the plan's object, and names an altar.

RestToGetBetter and RestOnTheSpot are chosen only when no other creature, of any player, is nearer than 8 times the
creature's height. The home is the shared creature home (`ecs::creature_home::HomeOf`): the home given to it, else its
player's temple's place for it. The go-home desire source (type 49) is the distance from home across the land over the
stage of growing up's go-home distance, up to 1 (with no distance for the stage, 1 away from home and 0 at home); the
hang-around-at-home source (56) falls from 1 at home to 0 a hundred units away (`creature_desires::HomeSource`).

A plan forced on the creature (`Situation::forced`, set by `ForceActivity` unless the plan is
`ForcedActivity::checkedAsChosen`) is not asked the tests that only weigh whether to choose an action, as in the
original ([A forced plan is not asked its row's test](#the-draws-finishing-and-the-state)): praying at the temple near
home, going home when away from it, resting with nothing frightening near, looking at or going to the player's hand,
going to the middle of the screen, following the player or showing it an object when the camera is not on it, and
being cross with it. What the action is made from (a home, water, a camera) is still needed. Only the stroke's
do-it-again is `checkedAsChosen`, as the original asks the row's test there.

Where this differs from raffclar's port:

- The home is the given home first, then the temple's place, where his puts the temple first.
- The rest after a knock-out still gets up once it no longer needs rest (life 0.4 or more, exhaustion 0.3 or less). His
  port gets up only when better and after more than 30 turns, which his notes give as the game's own rule for resting
  to get better (0x5008D0); that is for the fidelity audit to measure.
- Only the knock-out's rest paces energy as resting; the mind's rest goes by its action's row.
- The look at the temple while pointing is its top, and the point is where the temple stands; the frightened look
  before running home is wide eyes for the fright only, not held to the next plan; going into the temple does not yet
  make the creature believe in the heart.
- The two actions that cast a one-shot miracle restoring health (CastOneOffSpellInHandToRestoreHe,
  PickUpAndCastOneOffSpellToRestor) wait for the creature's use of globes. SleepAtHome is in "The lesson actions in
  openblack".

### The kind actions in openblack

Ported from raffclar's creature-kind-actions work (his #419, with the kinds half of his #397, the throw position and
the store throw of his #137 and the hurl of his #232), unverified against v1.42.

- **Picking up by kind.** What a creature may pick up starts from the thing's kind
  (`creature_object_actions::RuleOf`, `DecidedByKind`; `CreatureObjectActionSystem::KindFactsOf`): the football and
  creeds always, toys among the statics, birds low over the land, magic mushrooms and piles unless a script said no,
  and never lanterns, shields, vortices, teleport stones, creatures or what is built into the land. Villagers,
  animals, things lying about, pots, rocks, bonfires, dead trees and trees go by our own test of things in general
  (`creature_pick_up::CreatureMayPickUp`), each with its kind's weight. A standing tree pulled up creaks: in `LH3DCreature::DoBlendedAction`
  0x48BCC0, as the grab takes a tree that is not in the physics (flag 0x40 at +0x24 of the thing acted on),
  `SoundTag::Create(the creature's position +0x78, GetRandomSample(0x88, 3), track 0, mode 3, loops 0, extra 0, 3D 1,
  bank 1 (in game), delay 0)` at 0x48C146..0x48C168 of the v1.42 exe; `GetRandomSample` 0x71ED40 is first +
  `GRand::LocalRand(n)`. Sample 0x88 is LH_SAMPLE_G_TREEBREAK_01_C2 (bw1-decomp `LHSample.h`, 136 to 138, the
  creature's set; the player's hand plays 32 to 34). Ours: `audio::tags::RandomSample(136, 3)` on the same local
  stream, a point tag with the same arguments. His newer lift and giving rules are not taken.
- **The thing used.** An executor may name what it uses (`Executor::use`) and refuse what it holds as its goal
  (`notWhatItHolds`). Planning, the thing used is the nearest usable thing, what it holds first, never the goal, inside
  its leash's area (`creature_planner::ChooseUsed`, `UsedFor` in `CreatureMindLearning.cpp`). With none, the desire's
  next action is tried, on both of the planner's paths; a plan made otherwise chooses its thing as it is taken up.
- **Kicking and stomping** ([the original](#kicking-and-stomping-a-thing)). KickTree and Kick walk right up to the
  thing, face it a tenth of a second and kick it (`creature_mind::KickThing`, the kick order, `KickAway` in
  `CreatureObjectActionSystem.cpp`): a home has pieces broken off it, anything that can fly, a standing tree among
  them, flies off, and a tree lands as a thrown tree does. Stomp is the knock-down: a home takes the blow a thrown thing
  gives, a standing tree and a mobile object nothing. Testbed `objects.kick_tree` and `objects.knock_down_trees`.
  - The thrower of a kick (2026-10-20). What a kick sets flying is the creature's throw for the creature's player:
    the game hands `InitialisePhysics` the creature and its player's leader interface (0x488F64..0x488FAA), whoever
    that player is (every player has one, `GPlayer::Init` and `GInterface::IsLeaderInterface` 0x5D0690), and none
    for a creature of no player's (0x488F87). openblack gives the kick's body the creature as its thrower and the
    creature's player (`creature_object_actions::ThrowerPlayerOf`, `PhysicsEntry::throwerPlayer`) as it starts, so
    the catch offered at the start sees it too: a kicked tree that dies is that player's dead tree and one replanted
    moves that player's alignment, what it hits is hit by that player, the ball's kicks too. Before, only a creature
    of the local player's kicked for its player, and that player was set after the start.
  - The creak of a kicked tree (2026-10-20). A standing tree not in the physics creaks as the foot meets it: a point
    sound tag of one of the three creaks from 136 (`audio::tags::RandomSample(136, 3)`, mode 3, 3D, the in-game bank,
    at once) at the kick point (`KickPoint`): where the right foot grasps in the kicking animation (117) at the
    creature file's first catch time (`CreatureAnimationSystemInterface::GraspInAnimation`, as the catch measures its
    hands), through the body's placement. openblack measures it at each kick; the game keeps it from the first kick
    until the body changes, which poses the same body. Test
    `CreatureObjectActionSystemTest.AKickedStandingTreeCreaksWhereTheFootMeetsIt`.
  - The kick of a home (2026-10-20). A built home that is not burning (`KickHome`) has pieces broken off it by the
    game's two blows (`creature_object_actions::HomeKickCentre` and `HomeKickBlows`, the three draws of the game's
    random numbers in its order, the sine and cosine as its FPU gives them), struck by
    `BuildingDamageSystemInterface::StrikeWithoutBody`: the home's broken model, made from its model when it has none,
    each piece flying as a rock's blow sends it; nothing lost, it is drawn whole again; else it takes the damage of a
    physical impact by the creature for its player. The kick's way is the one a kicked thing flies (`KickDirection`),
    the size the one its speed is taken from; the land under the point is read as the hand reads it; the game's heading
    is ours with the other sign. Until then a kicked home took the blow a thrown thing gives, as the knock-down still
    gives. Tests `CreatureKindActions.AKickOnAHomeStrikesTwiceLowOnItHalfWayToTheFoot`,
    `CreatureObjectActionSystemTest.AKickedHomeTakesTwoBlowsOfTheFootWhenItIsBuiltAndNotBurning`.
  - The knock-down on a mobile object (2026-10-20). A ball, a pot, a seed or anything else mobile a creature stomps on
    stays as it was; until then the blow deleted it. Test `CreatureObjectActionSystemTest.AMobileObjectKnockedDownIsLeftAsItIs`.
  - The knock-down on a living thing (2026-10-29). A villager, an animal or a creature may be knocked down (the hands'
    start no longer refuses it; the planner's targets for Stomp still go by `CanDestroy`: trees, homes, mobile objects
    and statics). The blow does nothing to it; from the turn after the blow until the knock-down's animation ends, each
    turn, before the animation plays on, a living thing still available and in the map takes a crush of 1.0, a metre
    wide, applied by the creature (`creature_object_actions::KnockDownCrushes`, `k_KnockDownCrush`,
    `k_KnockDownCrushRadius`; `CrushKnockedDown` in `CreatureObjectActionSystem.cpp`, through `effects::ApplyEffect`
    as the original's `ApplySingleEffect`), as sub_502B00's stage does while the body acts and the object action's
    status is 3. Test `CreatureObjectActions.AKnockDownCrushesALivingThingInTheMapWhileItPlaysOnAfterTheBlow`. The
    agenda's kill count (sub_4794D0 on the agenda's last sub-action) is not kept: only StompAndEat knocks a living
    thing down, and its knock-down is never the last step.
- **The actions.** GiveWoodFromTreeToStoragePit (to a fully built storage pit), PlantTree, PracticeThrow, ThrowAround (on
  the living), Hurl (a rock, static or dead tree at what thrown things knock down), ThrowToImpress, ThrowBallAtObject
  (a toy at anything), ThrowInTheSea (only what may go in the sea; our walk to the coast stays) and GetHigh. Their
  agendas are pure functions in `Creature/CreatureKindAgendas`. Going to throw, the creature backs away or walks up to
  between two heights (plus what it keeps and the target's radius) and three and a half heights of the target
  (`creature_throw::WhereToThrowFrom`). The throw into a store is given its time from the ground distance as every
  creature throw is (`creature_throw::ThrowFlightTime`); it had been the time of a fall over that distance, divided by
  9.81, which differs in the last bit for about 4 distances in 10 000.
- **Holding something already.** The builders of Hurl and ThrowToImpress (`CreatureAgenda::ConstructSubActionsForHurl`
  0x49ADF0, the carried thing read at 0x49AE04..0x49AE15 before the `GameRand(6)` at 0x49AE27), ShowPlayerAnObject
  (0x49D5E0, 0x49D5EB..0x49D5F1) and ThrowAtCamera (0x4A3DA0, 0x4A3DE4..0x4A3DEE) ask only whether the creature carries
  anything (physical +0x28, `GetObjectCarried() == NULL`), not what: with something in its hand it neither picks the thing to use up
  nor, for the hurl, draws the `GameRand(6)` of its angry start; it throws or shows what it holds. Ours
  (`creature_plan_actions::Agenda`, builds Hurl, ThrowAtCamera and ShowPlayerAnObject) now asks the same of the hand
  (`Situation::held`); the first port picked the chosen thing up while holding another unless a script's plan said its
  hand was full. A script's forced hurl is no exception: `ScriptDoAction` 0x4F7000 and its
  `PrepareCreatureForScriptedAction(1)` 0x4F6A90 stop the action but leave the carried thing in the hand
  (`StopWhatIAmDoing` 0x4758D0 and `EndAnyActions` 0x48F710 do not release it; only the body's discard, throw and
  blended pick-up call `ReleaseHeldObject` 0x48CBB0), and the throw (`THROW`'s Initiate 0x501130, physical +0x28 at
  0x50114F..0x50115A) throws whatever is carried. So in the punishment lesson (`CreatureDevPunishment`, the hurl of the
  trainer at the tiny rock, row 15, the trainer given as the thing to use) a creature that already holds something
  throws that at the rock, the trainer never flies, and the lesson's wait for her to fly never ends: the original's
  soft-lock, and openblack's the same (testbed `objects.hurl_hand_full`; [vanilla bugs](../project/progress/vanilla_bugs/story_scripts.md#soft-locks-and-lost-progress)).
- **Where the head looks.** The builders give each sub-action a look: every pick-up of these actions
  `LookWhileGoingTowardsObject`; GiveWoodFromTreeToStoragePit (bw1-decomp `ConstructSubActionsForGiveWoodFromTreeToStoragePit`)
  none on the walk to the tree, `LookAtObjectArgument` (the store) on the move to the throw place and on the turn to
  it, none on the throw; PracticeThrow and ThrowAround `LookWhileGoingTowardsPoint` on the throw at the point; PlantTree
  (`ConstructSubActionsForRepositionObjectToUseNearObjectToActOn`) `LookWhileGoingTowardsObject` on the walk to the
  thing, `LookAtObjectArgument` on the turn, none on the put-down; GetHigh none on the eating. Ours
  (`Creature/CreatureKindAgendas`) gives the same gazes; the first port gave none but the hurl's and the toy throw's.
- **The throw's time** (ours, from the exe, 2026-10-18; [A creature's throw](#a-creatures-throw-the-flight-time-the-aim-and-the-launch)).
  A throw at a point or a thing is timed as it starts, from where the creature stands before it turns, by the
  distance along the ground (`CreatureObjectActionSystem::Start`, `creature_throw::ThrowFlightTime`); it had been
  timed after the turn by the distance in three dimensions, divided by 9.81. What it throws leaves the hand at the
  game's velocity, in the game's order of float steps (`creature_throw::ThrowVelocity`); the earlier order differed in
  a last bit in most throws. A throw at a thing (the hurl, the throw to impress, the toy's throw) is aimed at the
  thing's middle where it is as the throw starts, half its height above the ground there, in the time to there
  (`creature_throw::AimAtStillThing`, in `CreatureMindSystem`'s order to the hands); it had been aimed at the ground
  at the point the plan chose. Since 2026-10-20 a walking villager is led along its step for the whole turns in the
  time, unless its top state is a final one (`creature_throw::VillagerAfter`), and a creature is aimed at where its
  walk takes it, at its drawn size × 15 above the ground there (`creature_throw::CreatureAfter`); a villager had been
  aimed at where it was, a creature at the plan's point on the ground. The creature window's Hands part shows
  the throw's point and time under "Throw" (closed).
- **Wood to a store.** GiveWoodFromTreeToStoragePit (action 32) is done only to a storage pit that is fully built
  (`Target::BuiltStoragePit`, as the town help actions' handfuls). Its row of `CreatureAgenda::ActionTable` (0x9D1678,
  filled by 0x491BA0) asks the object to act on `GameThingWithPos::IsStoragePit`, vt+0x300 (row +0x34, the thunk
  0x4C3500), called through `CreatureAgenda::IsObjectSuitableForAction` 0x4E3880. Only the StoragePit class answers
  other than no: 0x4E4990 is `IsBuilt` (vt+0x890, `Abode::IsBuilt` 0x4016C0: not under construction and all of it
  built); the Creche 0x50A9C0, TownCentre 0x55DB50, Graveyard 0x55DFE0, Wonder 0x5611F0 and Workshop 0x7792F0 answer
  no, as does every other class (0x401DE0). The thing used is a tree (+0x38 vt+0x33C) it can pick up (+0x3C vt+0x258),
  our `Target::PickableTree`. Our first port took any storage pit, one still going up too.
- **Examining by looking.** ExamineByLooking (action 20, in the Curiosity list) is done to `Target::Examinable`: the
  things the planner finds for "anything", less creatures, fields, fish farms, vortices, teleport stones and miracles
  at work (`creature_object_actions::CanBeExaminedByLooking` by the hands' kind, the rest by component). Its row asks
  `GameThingWithPos::CanBeExaminedByCreature`, vt+0x280 (row +0x34, the thunk 0x4C3510): `Object` 0x402A90 answers yes
  for its 125 classes (villagers, animals, birds, abodes, the temple's parts, worship sites, totems, features, trees,
  statics, rocks, pots, piles, the ball, lanterns and shields among them); `Creature` 0x473FF0, `Field` 0x527FC0,
  `FishFarm` 0x52C540, `MagicTeleport` 0x5FC0B0, `LandscapeVortex` 0x5FD690 (its four classes) and `MagicFireBall`
  0x682AF0 answer no, as do the 52 classes that are only a `GameThingWithPos` (0x405440: towns, citadels, planned
  buildings, every spell). Our vortex and teleport statics are the hands' `Thing::Vortex` and `Thing::TeleportStone`.
  The kind actions port's wider "anything" had let the creature look at them; creatures it always could.
- **Getting high.** GetHigh (action 181) is listed only by the GetHigh desire (34) in `Scripts\info.dat`, and is
  done only to a mushroom (`Target::Mushroom`). The action's own row in `CreatureAgenda::ActionTable` has no object
  test; the desire's does: entry 34 of `CreatureDesireTableEntry::Table` is `GameThingWithPos::IsMushroom`, vt+0x2A4,
  called through `CreatureMental::IsObjectSuitableForDesire` 0x4E3730. Only two classes override it: the
  MobileObject's 0x4E4CE0 (its info row's class is 18, magic mushroom, or 19, toadstool) and the Feature's 0x5276D0
  (feature info 54, the magic mushroom); every other thing, villagers, the ball and statics among them, answers no
  (0x401CB0). Our first port took anything for it, so an idle creature whose GetHigh desire had grown picked up the
  nearest villager or ball and ate it.
- **Becoming high.** A magic mushroom eaten (or a toadstool) makes the creature high once its agenda is done
  (`CreatureMindSystem::BecomeHigh`, a forced plan of getting high); its actions play with stoned eyes
  (`Step::heldEyes`, held only while each action plays, as the rest and home port has it; his port holds them until
  the next agenda, to be settled against the exe).
- **The high, step by step.** The mushroom effect 0x509530 (with its tail 0x509C50), stored in the mind at +0x21D0 by
  the meal's end (0x5005B5) and run by the planner's turn 0x4F0370 (0x4F0495..0x4F04B5) in place of a new plan, forces
  GetHigh (action 0xB5, desire 0x22, about itself; `ForceActivityAndForceAction` at 0x509557), clears the agenda and
  adds: look at the camera 3 s (sub-action 0x42, ticks = (1000 / tick ms) × 3.0), amazed; talk (0x89), fluttering its
  eyelids; look at the camera 2 s with the stoned look, smiling; wait 3.1 s (0x18, the float 0x8D16B4) looking at the
  camera, smiling; action 218 (0xDA), stoned; `GameRand(2) == 0` (0x5097C6): action 55, stoned; action 63, amazed,
  stoned; wait 1.1 s, smiling; `GameRand(2) == 0` (0x50993D): action 220, amazed, stoned; action 63, amazed, stoned;
  `GameRand(2) == 0` (0x509A31): `FindClearArea(4 × Get2DRadius, 0)` (vt+0x64, 0x509A5C..0x509A77) and, when found,
  a walk there within the 2-D radius and a static sit (sub-action 4, animation 38) of `10.0 + GameFloatRand(5.0)`
  seconds (0x509B41..0x509B4B), sad, stoned; then action 56, stoned; action 61, frightened, the frightened look;
  `GameRand(2) == 0` (0x509C8B): action 56, else 219, both with no look of their own (a null look, 0x509CA6 and
  0x509D02); at last (0x509D64) the camera 3 s, amazed; talk,
  stoned; action 218, stoned. Ours (`creature_mind::BecomeHigh`) builds the same steps and draws, in the same order.
  The sit's extra seconds are drawn as `GameFloatRand(5.0)`: the first port drew `GameFloatRand(1.0)` and scaled it by
  five, which rounds otherwise (the game's float draw multiplies the draw by its range before its scale).
- **Two quirks of the game kept** (listed in [vanilla bugs: creature](../project/progress/vanilla_bugs/creature.md)):
  the throw-to-impress check sets the thing's height against 0.64 of the creature's weight, so it never holds anything
  back; a tree is never taken to plant from within 20 m of where the creature last set about copying the player.
- **The second set** (his #420, unverified against v1.42): GiveWoodFromTreeToBuildingSite (to our building site
  target, a building still going up or not repaired below full life), HelpRepairHouse (a town's building below full
  life, not the temple, or a town's totem below its own full life, as the game's test of what wants mending has it,
  see [mending a building or a totem](#mending-a-building-or-a-totem): a tree brought and put down by it, the thing
  mended by a fifth of its life, a totem's mending going to its town centre while it stands on one, the tree used
  up), BreakRock (a practice throw with a rock it can pick up, the rock broken in two as it flies), SmashStoneInHalf
  (a knock-down of any rock), StrokToy (the game's spelling), TakeToyHome (a static toy more than 50 from home, the
  home of `ecs::creature_home::HomeOf`), ThrowDie (watching the die fly, the step `WatchFlying` in his #432 form) and
  PlayThrowingGameWithPlayer (at the nearest of the local player's hands, at the hand's height). KickBallAround now
  kicks: the kick is an object action of its own (clip 117, at the creature file's kick moment), the thing flying
  off at 45 times the shown size, half up and half ahead, tipped by 0.6 of the land's rise across the creature's
  width, at most 0.3; it is silent (vanilla bug 68, kept). Every blow now measures its reach at the right foot. A
  creature's blow on a rock plays one of its four rock taps and splits the rock
  ([physics impacts](../physics/physics-impacts.md#rocks-that-split)); on any other static it does nothing. Talking
  while high flutters the eyelids (blinking every 1.25 s until the next agenda); stoned or wide eyes leave the lids
  and the blink as they were. Kept ours: the sitting rest, the put-down and the toss of our object actions for his
  discard with a clip, and our knock-down of homes, trees and things.

### The end of a gift to a store

The game's builders of the gifts to a storage pit (food from a field, fruit, magic food and magic wood; bw1-decomp
`CreatureAction.cpp`, `ConstructSubActionsForGiveFoodFromFieldToStoragePit` and its neighbours) and of dropping a cow in
a pit (`ConstructSubActionsForDropCowInStoragePit`) end the same way: CONSIDER_ACTION_COMPLETED, CLEAR_OBJECT_TO_USE,
then the throw in as the main sub-action (`AddMainSubAction` of THROW_IN_PILE). CLEAR_OBJECT_TO_USE's work stage is
0x507C40: the mind's thing to use (+0xF5C) becomes `GetBeliefAboutObject` of the creature itself, and it returns 2.
That it is sub-action 0x6D's stage is read from the table: `ProcessSubAction` finds a sub-action's stage function at
0xBE0BE8 + (state x 9 + stage) x 0x10 (0x4FEE02..0x4FEE29), and the table's set-up
(`crt_xc_fn_CreatureSubAction_004F8D70`, 0x4FD788..0x4FD799) stores 0x507C40 at 0xBE4948, 0xBE4958 and 0xBE4968,
state 0x6D's stages 1 to 3, as 0x507C10 at 0xBE48B8 is state 0x6C's (CLEAR_OBJECT_TO_ACT_ON) stage 1 (xref note #630).

In openblack the throw is the agenda's main step, and `Step::Kind::ClearObjectToUse` before it makes the plan use the
creature itself from then on (`creature_planner::UseItself`), as `ClearObjectToActOn` does for the thing acted on.

### The town help actions in openblack

Ported from raffclar's creature-town-help work (his #517, with the face step of his #511), unverified against v1.42.
Fourteen actions of the game's table get an agenda (`creature_town_help` in `Creature/CreatureTownHelp`, pure, with
its tests); their world's side is `mind_town_help` (`CreatureMindTownHelp` in the mind system's implementations).

- **Handfuls to a store.** GiveFoodFromFieldToStoragePit (a field with food), GiveFruitFromTreeToStoragePit (a tree
  tall enough to eat from), GiveMagicFoodToStoragePit (a pile of magic food away from its town's pit, our food pile
  target) and GiveMagicWoodToStoragePit (a pile of the wood miracle; the handful is an evergreen branch, a hardwood
  branch or logs by a draw of three) give to a built storage pit. Unless it holds what it gives (for food, anything it may eat:
  [Holding food](#holding-food); for wood, a tree), the creature walks to
  within one and a half heights of a field (else a fifth more than both radii), faces it a tenth of a second and takes
  a handful: our portion of the kind (`ObjectOrder::Kind::PickUpPortion`), which holds what the source keeps of the
  portion's own resource. Then it goes to its throw place, faces the pit with compassion (the action counts as done)
  and throws it in. PutFoodFromFieldByWorshipSite puts a handful of wheat down within its height of the site's food
  pot (`WorshipSite::foodPot`).
- **Living things.** DropCowInStoragePit throws in a farm animal or tortoise (or their puzzle kinds); MakeBreederDisciple
  strokes a grown villager of a town with fewer than three breeders and no disciple, and makes it a
  breeder (`ecs::villager::SetVillagerDisciple`); TakeVillagerHomeToSleep carries a villager more than 40 m from home
  to 2 m from it; BringVillagersToWorshipSite carries a villager that is not dancing in worship to the site.
- **The cow's throw place.** `CreatureAgenda::ConstructSubActionsForDropCowInStoragePit` 0x4BF490 adds the move to
  the throw place (sub-action 0x10, `MoveToThrowPos`) with `SubArgumentObjectAndFloat(the store, creature->GetHeight())`
  (its one call of vt+0x42C), as the handful and wood throws into a store do. The move's start (0x502390, the sub-action
  table filled by 0x4F8D70) adds that float to the store's radius and twice the creature's height for the nearest
  place it throws from (0x502447..0x502473). Ours (`creature_town_help::DropCowInStore`) now gives the step the
  creature's height to keep; the first port gave none, so the creature stood a height nearer the pit than the game's.
- **Who may be made a breeder.** MakeBreederDisciple's row (action 287) asks
  `Villager::IsVillagerInTownWithoutManyBreeders` 0x4E4510 (vt+0x2D0, the thunk 0x4C3730), in this order: with a
  creature, its mind's (+0x164) opinion of this very action, `CreatureActionOpinions` entry 287 (mind +0x25B8 +
  287 × 4 = +0x2A34; the opinions are constructed at +0x25B8 by 0x4D239D), below 0.0 or no number (`fcomp`, `test ah,
  1`) with mind +0x1C38 (mimicking its player) 0 refuses; then the villager's Flags +0x24 bit 0x400 (controlled by a
  script) clear, `IsAvailable` (vt+0x2C), not `IsChild` (vt+0xAF8, +0xE0 & 8), not `IsSkeleton` (vt+0x4A0,
  `Living` 0x416FF0: +0xB4 & 0x40), not `IsDead` (vt+0xAF4), +0xE0 & 0x80 clear (the football / script bit), a town
  (vt+0x48), +0xE0 & 0x200 clear (a disciple), not `IsChild` again, and the town's breeder count (byte +0x6DD) below 3.
  Ours (`creature_town_help::CanMakeBreeder`): the creature's side first (`BreederFacts::opinion` from its learnt
  opinions, `mimicking` from its mimicry), then a grown, live villager no script controls, not a skeleton, without
  the football bit, of a town, no disciple, with fewer than three breeders. The first port lacked the creature's side,
  the skeleton and the football bit, and refused a disciple's follower (+0xE0 & 0x400), which the game does not ask.
- **The totem.** RaiseTotemPole and LowerTotemPole, for a totem its player's hand could take hold of: from 12 m, the
  summoning clip, then the town's worship share moved by 0.02 a turn to the end (`Step::Kind::RaiseTotem`,
  `LowerTotem`; `Senses::totemShare`; `VillageTotemSystemInterface::SetTownShare`). On a puzzle totem (only a script's
  `CREATURE_DO_ACTION` reaches one) the share is the totem's own ([the puzzle totem](../magic/puzzle-totem.md#the-creature)).
- **Which totem.** Both rows (actions 56, 57, in the Play list) ask vt+0x2F0 (the thunk 0x4C3770),
  `TotemStatue::DoesTotemBelongToATownWhichIsVeryImpressedIndeed` 0x4E3E70; every other class answers no. It needs
  the totem's town (vt+0x48), the creature's `GetNearestCameraInterfaceStatus` 0x47D640 (its player's nearest interface
  status; with no player the local one, none in a multiplayer game) and the totem's
  `ValidForLockedSelectProcess(status)` 0x738500, the hand's own grip test: the status's player is the totem's
  (vt+0x1C both), `IsFunctional` (vt+0xD4), the totem's player's citadel (+0xA48) has a heart (+0x30) that `IsBuilt`
  (vt+0x890), and the totem's town's worship site (vt+0x30C) `IsBuilt`. Ours: `creature_town_help::MayMoveTotem` over
  `TotemFacts` (the town; `watched`, the creature's owner has an interface status here,
  `worship::interface::HasInterface`; `mayGrip`, `VillageTotemSystem::ValidForLockedSelect(totem, owner)`, the hand's
  test, [village totem](../magic/village-totem.md)). Our first port asked instead for the local player's town with its
  town centre built (not asked by the game) and a temple, without the working totem and the built heart. A creature
  with no player is not known here: every creature has an owner, so the game's "the local one" case has no twin.
- **Play.** PlayfullyFrightenVillagers (three heights, facing 2 s, the frightened clip with wide eyes),
  PlayfullyInteractWithVillager (two heights; two times in three the playful clip; a face of the first ten held 5 s,
  a point at the villager with a smile, done, then another face; `Step::Kind::Face`) and PlayfullyKissVillager (the
  kiss clip 212 once the body is free; `Step::Kind::Kiss`).
- **Not yet.** PutFishByWorshipSite waits for the creature's fishing. A town's compassion offers its members, which
  hold no worship site (as the original's town list, which files a site under its citadel), so the planner reaches
  the two worship site actions only through scripts or copying until the citadel belief's path is ported. A point at
  a thing lasts our usual while, not its five seconds.
- **Kept as the game has it:** a handful from a field or a tree holds no food (the vanilla bug of
  [a portion](#a-portion), listed in [vanilla bugs: creature](../project/progress/vanilla_bugs/creature.md)).
### The sacrifice action in openblack
Ported from raffclar's creature sacrifice work, unverified against v1.42. `creature_sacrifice::Sacrifice`
(`Creature/CreatureSacrifice.h`) makes the agenda; `creature_sacrifice::SitePlace` picks the first slot with a site.
The action is possible only with a temple (`Situation::hasTemple`) and is done to any living thing
(`Target::AnyLiving`: a villager, an animal, birds among them, or a creature). The planner weighs it wherever the mind
table names it; a script's CREATURE_DO_ACTION and a copy of the player's deed reach it too. Each fills the situation's
`holdingLiving` and `sacrificeAltar` (the totem's place) through `SituateSacrifice`. Its walk is by the creature's
height as the rest and home actions take it (`object_measures::Height`).
Where this differs from raffclar's port:
- The put-down is our put-down order (`ObjectOrder::Kind::PutDown`), which plays the same clip 97; his is a discard
  with that clip.
- The looks while going are our walk looks (`Gaze::goingTowards`), without his look sweep.
- A held thing is living by our components (villager, animal or creature), not by his thing classification. The game's
  own test is vt+0x3C8 on the held thing; that it means "alive" is his reading, not checked.
- The height is our `object_measures::Height`; his is the height of size one times the shown size.

### The steal action in openblack

Ported from raffclar's creature steal work, unverified against v1.42. `creature_steal::StealAndPutByTemple`
(`Creature/CreatureSteal.h`) makes the agenda and `creature_steal::IsStealable` decides what may be stolen: a toy (the
football or a static with a toy's model), a scaffold or a one-shot miracle's globe, that the creature can pick up, where
its own player's raw influence is not above 0 (`mind_detail::CanSteal`). The action is possible only with a temple
(`Situation::hasTemple`); without a home (`Situation::home`, the shared `creature_home::HomeOf`) there is no agenda. The
planner weighs it with the things `Gather` finds for `Target::Stealable`; a script's CREATURE_DO_ACTION reaches it too.
For this build only, the planner sets the creature's height and whether its hand is full.

CREATURE_SET_PLAYER gives the creature to the local player (the leash takes it, the player system adds it) and the
newest reaction the creature started becomes that player's (`ReactionSystem::SetPlayerOfNewest`).

Where this differs from raffclar's port:

- The put-down is our put-down order (`ObjectOrder::Kind::PutDown`), which plays the same clip 97; his is a discard
  with that clip.
- The looks while going are our walk looks (`Gaze::goingTowards`).
- Scaffolds can be stolen: ours has them; his tree gave `false` for want of them.
- A thing that another creature holds is not taken, as for every pick-up target of ours.
- The influence is our raw-influence function, measured in metres; his is in map coordinates.
- The candidates come from what the mind gathers about it, not from beliefs of things seen.
- CREATURE_SET_PLAYER does not name the creature with the profile's creature name: our creatures have no name yet.

### The thefts from a town

How the original builds them (read in the v1.42 exe; the rows of the action table in bw1-decomp `CreatureAction.cpp`):

- **StealAndPutInTown** (row 264, `ConstructSubActionsForStealAndPutInTown` 0x4BAC00, target `IsStealableByCreature`, no
  "possible" test): the creature's player's towns (player +0xA50, then each town's +0x75C) are walked for the nearest by
  `GetDistanceInMetres` of the town's position to the creature's (strictly nearer, so the first of two at the same
  distance); no town, or the town's `GetStoragePit` 0x73B5B0 gives none, fails. The creature then adds a belief about the
  pit (`AddBeliefAboutObject` 0x4D7BD0; none fails). With an empty hand (+0x160 then +0x28 is 0) sub-action 0 picks up
  the action's thing (+0x848) with `LookWhileGoingTowardsObject`; sub-action 0x3D goes to the pit within `GetHeight`
  (vt+0x42C) times 3.0 with the same look; sub-action 1 with 0x61 puts it down (clip 97). There is no main sub-action.
- **StealFoodFromStoragePit / StealWoodFromStoragePit** (rows 307 / 308, 0x4C11C0 / 0x4C1510): the target is a pit of the
  creature's player (`StoragePit::IsStoragePitBelongingToMyPlayer` 0x4E49E0: vt+0x890 and the two players by vt+0x1C
  equal), the thing used a pit of another (0x4E49A0: vt+0x890 and the players differ). The agenda is the field gift's
  (0x4A3350, the same sub-actions 0x8A, 0x10, 0x61, 0x6D and the main throw by `AddMainSubAction` 0x4FF3A0) but for its
  first step: sub-action 0x3D right up to the pit used (+0x84C, distance 0) with `LookWhileGoingTowardsObject`, where
  the field gift walks to within its reach of the field. The handful is pot row 0x11 (wheat) for food, 0x10 (logs) for
  wood. No test of what the hand holds.
- **StealFoodFromFarm** (row 306): the field gift's builder itself (`ConstructSubActionsForGiveFoodFromFieldToStoragePit`),
  with a field of another player as the thing used (`Field::IsFieldBelongingToAnotherPlayer` 0x4E4900: the players
  differ, no test of its food).

### The thefts from a town in openblack

Ported from raffclar's third creature town actions work, unverified against v1.42. `creature_town_theft` (`Creature/
CreatureTownTheft.h`) holds the pure rules and agendas: `StealAndPutInTown`, and `StealFromStore` on our gift of a handful
to a store (`creature_town_help::GiveHandfulToStore`) with its first walk right up to the pit and its throw as the main
step. The targets are `Target::OwnStoragePit`, `OthersStoragePit` and `OthersField`; a pit's or field's player is its
town's (none without a town). The theft from a farm is our field gift (`Build::GiveFieldFoodToStore`). The planner finds
the pit of the nearest town of the player (`Situation::ownTownPit`) and the creature comes across it as it sets off.
Tests: `test/test_creature_town_theft.cpp` (`CreatureTownTheft`).

Where this differs from raffclar's port:

- The theft from a store is our field gift with its own first walk, not a separate agenda.
- The town's pit is found by our town queries; a creature that cannot add a belief about the pit still sets off (ours
  always can).
- The towns are walked in the registry's order, not the player's list: only towns at exactly the same distance would
  differ.

### Emergencies in openblack

`CreatureMindSystem::StepEmergencies` (`CreatureMindEmergencies.cpp`) runs in `ProcessTurn` for a creature out of the
hand, after the dominant desire's turn and before its desires update, over the pure rules of
`creature_emergencies` (`CreatureEmergencies.h`). raffclar's #544 import, read in v1.42 ([Emergencies](#emergencies)):

- The gates: `CreatureMindState::emergencyRunning` (+0x10F4), the script's control, a forced plan
  (`creature_planner::Plan::forced` of the current plan, for +0x1C14; ours keeps no separate forced flag) and
  `CreatureFighting`.
- Badly hurt: the shared dominant slot, `creature_spell_mind::SetCheatDominant`(RestoreHealth, all, 10 s) with the
  species' floor (`mind_detail::DesireFloorOf`), as the leash and the spells use it.
- On fire: `ForceActivity` with CastMagicWater (its clear spot, then the cool-down that puts the fire out) or, after
  `LeashSystemInterface::TakeOff`, DrinkFromTheSea (a forced drink looks for the nearest water, as a chosen one does).
  A creature of no player or of the computer runs every walk of the plan (`creature_emergencies::KeepsRunSpeed`); a
  player's walks (vanilla bug row 136, kept).
- A fire in the nearest town: `map_cells::GetNearestTown` within 150, the burning building from
  `town_stats::AbodesOf` (newest first) by `creature_emergencies::PickBurningBuilding`, built pits by
  `abodes::IsBuilt`; the plan is about that town. The emergency runs even when the plan could not be made (row 137,
  kept).
- No reaction while `emergencyRunning` (`creature_reaction_rules::Availability::inEmergency`); stopping what it does
  (`StopWhatItWasDoing`) and the end of its plan (`Abandon`) clear it.
- The drink goes where every drink of the planner goes (`mind_detail::DrinkPlaceOf`, the land's water points first,
  from raffclar's #533), as the forced plan's agenda is made by `PlanAgenda`.
- Tests: `test/creature/test_creature_emergencies.cpp` (`CreatureEmergencies`).

### Test hooks

- The testbed's `objects.give_wood_site`, `objects.repair_house`, `objects.kick_ball`, `objects.break_rock`,
  `objects.smash_stone`, `objects.stroke_toy`, `objects.throw_die`, `objects.take_toy_home` and
  `objects.throw_to_player`, each a script's CREATURE_DO_ACTION of the action's row; the mind panel names the steps of
  breaking a rock, mending, using up and watching a thing fly.
- The debug window's body panel: the action, whether it counts as asleep for energy and growth, sitting, the turns
  after which it has slept long enough, and an "Exhaust" trigger (exhaustion 1: it faints at its next turn if the gates
  allow).
- Testbed scenarios: idle.fidget, `creature.walks_in_temple_pen`,
  `mind.hungry_takes_what_it_may_eat`, `mind.hungry_eats_from_magic_food`; for the kind actions
  `objects.pickup_kinds`, `objects.give_wood`, `objects.plant_tree`, `objects.kind_throws` and
  `objects.magic_mushroom`; for the town help actions `objects.field_food_to_store` (a field may start ripe:
  `ObjectSetup::ripeCrop`), `objects.cow_to_store` and `objects.kiss_villager`. The Mind tab names a step played on
  top of the body, a totem being moved and a kiss.
- The creature window's mind panel, "Emergencies" (closed): whether an emergency runs, and "Set it on fire".
- Testbed scenarios of the emergencies: `needs.on_fire_drinks`, `needs.on_fire_drinks_neutral`,
  `needs.on_fire_casts_water`, `needs.badly_hurt`, `needs.town_fire_water` (the command `set alight`).
- Testbed scenarios of rest and home: `needs.rest_to_get_better`, `needs.go_home_to_recover`, `needs.restore_health`,
  `needs.heal_itself`, `mind.go_home`, `mind.run_home`, `mind.enter_temple`, `mind.pray_at_temple`,
  `mind.look_at_temple`.
- Testbed scenario of the sacrifice: `objects.creature_sacrifice` (a worship site built by the map script's command,
  then a script's CREATURE_DO_ACTION of row 282 on a villager).
- Testbed scenario of the steal: `objects.steal_toy` (a script's CREATURE_DO_ACTION of row 265 on a cuddly toy, the
  player's temple far to the west).

## Pending

- **Stomp's angry gesture** ([Kicking and stomping a thing](#kicking-and-stomping-a-thing)): the plan action Stomp
  (row 18) is built by `CreatureAgenda::ConstructSubActionsForStomp` 0x49B660, which draws `GameRand(5)` first and,
  when it is 0, queues the individual action 53 (0x35) with the anger face (`SetFaceForActionAnger`) before the
  stomp; the stomp itself goes towards the thing with `LookWhileGoingTowardsObject` and the anger face. openblack's
  Stomp (`creature_mind::DestroyThing`, `Build::Destroy` in `CreaturePlanActions.cpp`) neither draws the number nor
  plays the gesture, so each stomp of the original draws one game random number that openblack does not. No quest
  forces a stomp (no script forces row 18), so only a creature that chooses it reaches the difference.

- **The steal action**: not compared with v1.42 yet. Each sighting of a stealable thing stirring source 60 by 0.3 is
  not wired: the creature's beliefs see things again (`CreatureMindSystem::StirSourcesOnSighting`), but the steal's
  `creature_steal::k_StealSource` and `k_StealableSightingAmount` are used by no game code yet. Whether the game's
  influence test is the raw influence, and in which units, is for the fidelity audit. The other steals (a spell seed by
  kind, scaffolding from a site, an animal, a villager) are not in yet; the thefts of food and wood and the theft put in
  its own town are ([The thefts from a town in openblack](#the-thefts-from-a-town-in-openblack)).

- **A creature's throw at a moving thing** ([A creature's throw](#a-creatures-throw-the-flight-time-the-aim-and-the-launch)):
  whether the order to throw reaches the hands on the same turn as the game's sub-action starts (the time is taken
  from where the creature stands then). A villager going round a circle is led by the last step of its straight
  walk: openblack's own walk round circles keeps a float step of its own (approximate, until the circle hug is
  ported in whole units), where the game's writes the step it leads by.

- **The sacrifice action**: not compared with v1.42 yet. Whether vt+0x3C8 is the game's "is alive" test, whether our
  put-down leaves a held villager at rest as the game's let-go does, and whether our height is the game's `GetHeight`
  are for the fidelity audit. The deed table's copy actions for the player's sacrifice deed (40) decide whether
  mimicry reaches it.

- **What an action is done to** (raffclar #547): the acts-on word as the planner's "needs a thing", once the agenda's
  per-action object filters (the action table's filter, `CreatureAgenda::IsObjectSuitableForAction` 0x4E3880) are
  read, so that Poo, SitDown, GoHome and the other four find their thing; the goal path's plan object for an action
  done to no thing (the goal belief's `FindBestObjectToActOn`); ExamineByFollowing only at a villager whose work
  teaches a skill the creature does not know (needs the villager state table's taught skill); Fart by the planned thing
  (its executor has no target yet). For SleepByObject (now in), vb#142 is kept: the original's "nothing
  frightening near" test measures from the creature, not from the thing (`Creature::NothingScareyNearPos` 0x4E44B0
  ignores its point, 0x4E44D8; the act-on filter 0x4E4480 passes the thing's position), so a creature sleeps by a thing
  only when no other creature is within 8 of its heights of the creature itself.


- **The agenda rules of #533 not taken** (they stand on parts of raffclar's tree ours has only in its own form or not
  at all): a step that needs a thing in the hand gives up the agenda when the hand is empty, and a fight needs free
  hands (his `HandsNeed`, from #492); a walk to a point where the creature cannot stand gives up the agenda, except
  the leash's, a script's and a step that walks again in its turns (his `giveUpIfUnreachable`, from #137; our mind
  ignores `MoveResult::InvalidDestination` for walks, and the exe's move stage is not read); StoneAndEat aims its
  throw at the rock it holds (vb#128, with raffclar's StoneAndEat of #512).
- **Rest and home**: none of it is compared with v1.42 yet. The knock-out's get-up rule (ours: no longer needing rest;
  raffclar's: better and more than 30 turns), the order of the home (given home or temple first), and whether the heal
  cast at itself is the game's target are for the fidelity audit. The globes that restore health are not in yet.
- **The kind actions**: none compared with v1.42 yet. Not known: whether a tree thrown into a storage pit becomes its
  wood in our physics (only a thrown fence is taken in today; `objects.give_wood` shows it), whether a tree put down
  takes root, and how the hands carry and put down a tree, a static, a ball and a mushroom. The game's high talk
  flutters the eyelids; ours does too since the second set of kind actions
  (`creature_mind::BecomeHigh`, `CreatureKindActions.TalkingWhenHighFluttersTheEyelidsUntilItTakesUpSomethingElse`);
  its high sit is mirrored half the time, as the game's. Kicking a ball
  about kicks it, two to four times (raffclar's PR #420, unverified against v1.42). Whether the hand counts as full is still as each caller of the planner says; the agendas of the kind
  actions read what the hand holds besides.
- **The map of the land**: none of it is compared with v1.42 yet. Not done: how long the creature stayed in each
  region. A map place that cannot be reached counts as reached, because the walk ends when the creature stops. Towns
  built after the land starts are not added to the map. (Done 2026-10-19: the beach's clear place keeps clear of
  living things unless a script controls the creature, and keeps within a kept creature's area, as every clear area
  does.)
- **Compassion and towns**: the draws as the original's, one `GameRand(2)` per slot-0 look-up, need the +0x10
  predicates (0x9D1678 + 0x50 × action) of every town-row action read; the goal and the act-on members from the
  creature's beliefs, in their order, rather than the world's; whether mind +0xF50 is cleared when a plan ends or is
  given up (raffclar's "being compassionate" test stands in for it); the town belief's attribute functions (religious
  belief 0.2/0.4/0.6, size 20/40, what it needs most) against his numbers; +0x21BC's start value. A stroke on a town
  member needs its life below 1 (`CanBePickedUpByCreature` 0x4E3A20); queued as a fix.
- **The plan's priority**: the goals and things from the creature's beliefs rather than the world (Gather, 200 m) are
  queued as a fix. How useful a thing is to act on: no kind of object is ever marked as looked over (openblack has no
  ObserveObject sub-action), so curiosity's hundredth never applies; the goal path's per-action u for a creature goal,
  Anger's thing found from its goal, `SetupReactToCreature` and the leash's act on (sub_50A380,
  `FindSomethingToDoOnObjectUsingObject`) are not yet ported as the original has them; the food the idle mind eats
  first (`NearestFood`) is openblack's own and not a caller of it in the original, and keeps the tree's usefulness
  alone. Not compared yet: the goal path weighs actions in groups by target; the distance is from the
  creature, not from the leashed object; the first pass of a new mind starts empty, where the original's state at
  creation is not read; the wide-screen look at the camera; Look's drop of a held object that is gone; the script's
  agenda priority with no plan carried out; the leash's act-on objects (packets 0x5C / 0x5D) and their priority; no
  instrument is chosen, so B is always 0.1; the gate of ToFollowPlayer; the increase
  time's clamp in learning tests its low bound first and its bounds are the species' (1 and 1200 in every row). Rest
  made most dominant ×1.5 while carried by a moving creature (0x47368E) can't arise: openblack's creatures can't carry
  creatures.
- **The desires' floor**: the original holds a desire at or above the species' `desireFloor` (info +0x274, 0x4DC4A8);
  openblack uses it at a plan's end and where a desire is made most or least dominant (`mind_detail::DesireFloorOf`:
  the spells, the leash, the training, the emergencies, a lost fright), and 0 elsewhere (every row holds 0). Queued as a fix.
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
- **A creature with no plan**: the totem call queued by the leash; queued gesture commands; what a creature with no
  plan does with what it holds. (The cinema's forced plan and the emergencies, on fire, below 0.4 life and a fire in
  the nearest town, are in: [A creature with no plan in openblack](#a-creature-with-no-plan-in-openblack).) A run-time read of
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
  - where in the turn: the original tests for a faint after the mind's plan step; queued as a fix;
  - the thirst: a negative info+0x224 or a NaN product; no row of `info.dat` reaches either case (all are 5000).
- **Eating**: the end of a meal is the original's, and a magic mushroom eaten makes the creature high
  (`CreatureMindSystem::BecomeHigh`). Not yet: the creature's
  eaten-by-class counters as the original's (openblack counts the people, animals and creatures it killed by eating
  them, its poos and its mushrooms in `CreatureTallies`, raffclar PRs #507 and #525, unverified against v1.42) and the
  player's condition a villager eaten adds (an eaten animal is deleted there, where
  openblack kills it as any death goes): fix row F28; in `EatFromFoodPile`, the town the game gives a portion, and,
  with something in its hand, the original's plan that ends at once or deals with it first, where openblack makes no
  agenda. Not determined: the two flags (+0x24 & 0x4000,
  & 0x200 with the game's +0x45E8 / +0x45EC) that make a creature spit a thing out before it is judged; the amount of a
  tree's fruit portion; the pile's +0x78; what the turn's argument 0.1 does (0x4AE5C0). For the pit, field and tree
  plans: what the "return 1" with something eatable in hand does
  to the plan (openblack makes no agenda); whether a NULL face function leaves the face as it was; how many turns 0x6C
  takes (openblack: the turn it starts).
- **The poo and the fart**, queued as fixes: both need an object in the original (file +0x94), the poo's filtered by
  `CanBePoodOn` and the fart's by nothing, and their agenda goes near it and turns away from it; openblack's are done
  on the spot with no object. The poo's thing is read ([a poo by a thing](#a-poo-by-a-thing): not alive, taller than
  1.5 m, not a field or a fish farm), with its walk, its turn away and PooAtHome; none is ported. The squat's moment:
  the original's lump or fart comes on every fifth turn of the poo's
  loop clip, from a counter that carries over from one squat to the next, in the body's turn; openblack's comes once,
  as the loop's time is up, in the mind's step (how many turns the loop plays in the original is not read). The
  fart's smoke: a chimney smoke at the anus, drawn with the creature and released when it stops what it is doing.
- **Reaction 25's takers**, queued: the villagers' fainting slot row (priority 200, the villager its own object,
  state 194, which does nothing until the reaction ends) and the creature's forced sleep (the creature reactions'
  setups).
- **Code the original lacks** ([overview](overview.md#pending)): on its way out: the show-desire timer
  (`IdleMind::showDesireSeconds`), its own measured commit. (Gone or settled: the old idle policy and its switch,
  [A creature with no plan in openblack](#a-creature-with-no-plan-in-openblack); planning every desire at once; a
  forced Watch the telly is a poo, as the original's; Look down a cliff builds nothing, as the original's.)
