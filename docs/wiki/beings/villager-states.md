# Villagers: data and states

## Summary

A villager is a Living with a state machine: three state slots (TOP, the state it is in; FINAL, the state a walk or a
clip leads to; PREVIOUS, the one to come back to), a table of 255 states with an entry, an exit, a validate and a
clip function each, and a turn that runs its reaction, its periodic checks (wear, hunger, growing up, old age, going
home when hurt) and the TOP's state function. Every change of state goes through the old state's exit and the new
state's entry, may pause for a second, and sets the speed and the clips. This page holds the villager's fields and
flags, the state table, the creation, the turn and the state changes; what the states do is on the other villager
pages.

openblack reproduces the fields, the table (with the original's function ids), the creation with its draws, the turn
and the state changes; the approximations and the tests are listed under openblack.

## The original

### Fields

| offset | field | notes |
|---|---|---|
| Object +0x48 | life | 0..1 |
| Living +0xA0 | birth turn | `Living::SetAge` 0x5ED2C0: `turn − age·1500`; the age is `Living::GetAge` 0x5ECAF0 = `(turn − birthTurn) / 1500` unsigned. 1500 = GGameInfo +0xC (0xD01A04) |
| +0xE0 | flags | see [Flags](#flags) |
| +0xE8 | food | what it has in its belly; the constructor leaves it in [0.5; 1.1) |
| +0xEC | last check turn | turn of the last periodic check (reset by CheckHungry 0x75BEDF) |
| +0xF0 | food speed-up | IsFoodSpeedUp 0x55C980; ProcessFoodSpeedup 0x753430 |
| +0xF2 | disciple type | g_DiscipleInfos 0x99A1F8 |
| +0xF4 / +0xF6 | resources held | food and wood it carries |
| +0xF8 | pregnancy | turns remaining (0 = no) |
| +0xFC | building site | the building site it builds ([Builders](villager-jobs.md#builders)) |
| +0x100 | mother | |
| +0x118 | target thing | TargetThing (bw1-decomp `Villager.h`); while building, the site's ring index 0..127 (a union) |
| +0x11C | — | union Football* / TradeTown / WanderArea (bw1-decomp `Villager.h`) |
| +0x128 / +0x12C | abode / town | |

The original's other fields: `next` +0xE4 (home list), `LastPlayerToInteract` +0x104, +0x108 / +0x10C / +0x110,
`fire_effect` +0x114 (fire), +0x124. The sex is GVillagerInfo +0x1F8 (`IsWoman` 0x752620 and SetSpeed read it).

- +0x118 also holds the death reason as a byte (VillagerDead writes `(u8) reason` after SetDying, 0x750929;
  GetDeathReason 0x55CB10): the same union as TargetThing and the builder's ring index. Writers from the building
  side: the field's RemoveFarmer 0x528362 / 0x5283B8 / 0x5283C8 and `Field::DeleteDependancys` 0x52814E (null), the
  fish farm's AddFisherman 0x52D281 (the farm).
- +0x11C / +0x120 under a script: SET_SCRIPT_ULONG on a villager (`GScript::SetScriptUlong` 0x6F8770,
  0x6F884D..0x6F8855) writes +0x11C = the clip and +0x120 = the number of plays. A farmer uses +0x11C as its work
  point.
- +0x28 is the GVillagerInfo pointer (ChangeInfo 0x761A0B). The records are `_VillagerInfos` 0xDA6BE8, stride 0x3A4.
  `GVillagerInfo::Find` 0x752650 returns the FIRST record whose +0x1F4 is the tribe and whose +0x1FC is the villager
  number, or null.
  - ✅ It is the villager's own record, kept from its making: `Villager::Villager` 0x74F950 passes the info it is made
    with to `Living::Living` 0x5EBEC0, and `Object::Object` 0x636520 stores it (0x63655F `mov [esi+0x28], ebx`). The
    only Villager method that writes +0x28 again is ChangeInfo 0x761A00 (0x761A0B), whose only caller is
    ChangeTribeIfRequired 0x761945 (the record of the town's tribe found by `Find`, unless the villager's own record
    has KeepMeshWhenChangeTown). Every reader (`GetInfo`, IsWoman 0x752620, IsHungry, SetSpeed, SetAge...) reads
    +0x28; none looks the record up again by tribe and job (a scan of every `mov [reg+0x28], …` in the Villager,
    Living, Object and GameThing methods found no other villager writer, 2026-10-20). A child grows up with the same
    record (`Abode::ChildToAdult` changes only the counts).
  - In the frozen `Scripts\info.dat` (rows of 916 bytes; row 0's SuperMesh at 0x229AC) rows 63..83 are every special
    villager (the piper 63, the Shaolin monk 64, the idol builder 65, the hermit 66 ... the trainer 77, the sailor 78,
    the crusader 82, the accordion sailor 83), all with tribe −1 and job 1, so `Find` gives the piper for each. Rows
    0..62 are the nine tribes' seven jobs, each pair once. Against the piper's row the other special rows differ in
    their meshes and help texts, FoodType (0 for all but the piper, the idol builder, the hermit, the sailors and the
    sculptor), the sex (the priestess 69 and the trainer 77 are women), KeepMeshWhenChangeTown (0 for the two
    footballers 71 / 72, 1 for the rest), WantSexAutumn, SacrificeValue (the monk 0, every other one 20000) and
    ComputerAttackDesire (0.1 against the piper's and the marauder's 0.11). Their speeds, life, weight, food values, ages and wounded
    thresholds are the piper's.
- Living +0xB4 status: bit 0x1 = dead, bits 4-5 = landType, bit 0x40 = skeleton (`Living::IsSkeleton` 0x416FF0;
  `Villager::SetSkeleton` 0x7562F6..0x756315), bit 0x80 = downed.
- GameThingWithPos +0x24: 0x1 in the map (`Object::IsObjectInMap` 0x6392B0, vt +0x178, tested by SET_SCRIPT_STATE
  0x6F832D, MOVE_GAME_THING 0x6F8F7E and RELEASE_FROM_SCRIPT 0x70F6E2); 0x2 the MultiMapFixed bit (MultiMapFixed ctor
  0x52E1F0); 0x4 in the hand (PlaceObjectInMagicHand 0x5FB014); 0x40 in the physics (set 0x636F2F, cleared 0x6375A0 /
  0x646B56); 0x80 on a structure (only `Living::MoveOnStructure` sets it).
- Living +0x60 is compared with the abode in HomeDeleted 0x7611F3..0x7611FD and set to 0 there. It is the object a
  move-to-object walk follows; no such walk takes an abode, so the clear never fires in play
  ([Home, food, sleep](villager-life.md#home-food-sleep-homeless-and-age)).
- Accessors: `Villager::GetTown` 0x751F00 (vt +0x48) = +0x12C; GetAbode 0x752160 = +0x128; SetAbode 0x750DE0 (+0x128
  = abode, SetTown(0), then SetTown(abode's town) when there is an abode); SetTown 0x756530; IsChild 0x55C970 (vt
  +0xAF8) = flags & 8; `Villager::GetPlayer` 0x7502F0 = its town's +0x2C, or none without a town; GetTribe 0x751EE0 =
  GGame::GetTribe(info +0x1F4), whose +0x10 is the tribe type; `Town::GetTribe` 0x73C840 = the GTribeInfo of Town
  +0x5B8.
- IsSexuallyActive 0x761090: StartHavingSexAge (info +0x228) <= age < StopHavingSexAge (+0x22C).
- IsPregnant 0x752210: info sex (+0x1F8) == 1 and +0xF8 != 0.
- IsVillager is vt +0x2C8 (Villager 0x55CAB0 returns 1). IsTree is vt +0x338 (`Tree::IsTree` 0x55D9D0 returns 1,
  GameThingWithPos 0x402320 returns 0).
- The hand never picks up a villager from inside its home: `Villager::ValidForPlaceInHand` 0x7564A0 (vt +0x6FC)
  returns `IsReachable` 0x756460 (vt +0x530), which refuses a villager at home (+0xE0 & 4, 0x75646E..0x75647C). The
  hand gets villagers out only through the tap on the abode (`Abode::InterfaceTap` 0x406830, 197).
- `CheckEveryTime`: 0x7504EF..0x7504FE: VillagerDead(reason, GetPlayer (vt +0x1C, Villager::GetPlayer 0x7502F0), 0.0, 1)
- Villager::IsChild 0x55C970 (vt +0xAF8): flags & 8
- Villager +0x118 TargetThing (Villager::targetThing): the field, fish farm or felled tree a job works on; The building side writes it too: fields::RemoveFarmer 0x528362 / 0x5283B8 / 0x5283C8 and Field::DeleteDependancys 0x52814E (null), fish_farms::AddFisherman 0x52D281 (the farm). The original's +0x118 is one union with the builder's ring index (Villager::buildPosIndex) and the death reason: they are kept apart here
- `deathReason`: +0x118 (u8) too: the death reason VillagerDead 0x750929 writes after SetDying (GetDeathReason 0x55CB10; SaveDead 0x754CC0). 🔍 inferred a union with the low byte of TargetThing; kept apart, nothing reads TargetThing after death

#### Flags

Villager +0xE0: 0x1 after a tap on its home (SetupAfterTapOnAbode), 0x2 at the worship site (AddVillagerToWorshipSite
0x76C3F0 / RemoveVillagerFromWorshipSite 0x76C440; CheckEveryTime reads it at 0x7504DC), 0x4 inside the home
(ArriveHome), 0x8 child (SetAge; CheckChildGrownUp clears it), 0x10 on the way to worship or to a fire, 0x20 in the
hand (InterfaceSetInMagicHand; EndPhysics / Landed clear it), 0x80 football / script, 0x200 / 0x400 disciple /
follower, 0x800 / 0x1000 entry / exit clip, 0x2000 going to sleep (CheckWhenGoingToBed).

Also: +0xE0 0x40 = counted out of the world population (SetDying 0x76A53E, ~Villager 0x74FBC0); bits 14-15 = the
carried tree type (see [Carrying resources](villager-jobs.md#carrying-resources-and-the-storehouse)).
- Town +0x5E8, read by the disciple check of CheckEveryTime (0x750643): the town's pulse (`Town::buildPulse`, "the town changed": written by fields, scaffolds, the storage pit (StoragePit::AddResource 0x733183), workshops and a villager's death;
- cmp [0x99A204 + 0x1C * t], 1 (0x75061E): g_DiscipleInfos +0xC (VillagerDisciple.cpp, the one table).
- `CheckEveryTime`: 0x75050E..0x75051E: turns since the last check > processChecksEvery (+0x2DC), strictly (jbe)
- `CheckEveryTime`: 0x750557..0x7505C3: hurt -> GO_HOME, and on: life < damageThresholdToGoHome (+0x35C;
- `CheckEveryTime`: 0x7505FC..0x750656: a disciple (flags & 0x200) of a type that ignores the needs, with the raw FINAL 221 DISCIPLE_NOTHING_TO_DO, and a town whose +0x5E8 is set -> DECIDE_WHAT_TO_DO.
- 0x14 (0x24): a moving state. Villager::CheckEveryTime 0x750439, IsMovingForAnimation
- `LifeDrainPerTurn`: 0xF8 (0x108): life lost each turn in the state. CheckEveryTime 0x750442 / 0x75048F (0xDB9F70)

### The function table

The original's is at 0xD09198, 0x90 bytes per state: +0x00 state, +0x10 entry, +0x20 exit, +0x30/+0x40 save/load,
+0x50, +0x60 (clip function), +0x70 (transition clip), +0x80 validate. The **exit** returns **1 = can exit** and
receives the next state; the **entry** receives (previous final, new state) and returns **1** (accepted), **0x23**
(accepted; the function set the states) or another value (rejected); an empty slot is "no function": it counts as 1.

- The fire states are in their rows ([Fire in the table](../world/fire.md#fire-in-the-table)); how hot a
  villager gets is [the heat model](../world/fire.md#the-heat-model).
- 16 DROWNING: EnterDrowning 0x767410 and ExitDrowning 0x767420 only accept (`mov eax, 1; ret`); the state function is
  `Villager::Drowning` 0x76A780 (see [Water](../world/water.md)).
- The table is filled at run time by its static initialiser `_$E32` 0x59D030. 0xD09198 lies past the raw end of
  .data, so the static image holds only zeros. Column +0x50 (0xD091E8 + 0x90·s) is the town-emergency answer, and its
  this-adjustment +0x54 is 0 in all 255 rows. Entry column = 0xD091A8, exit column = 0xD091B8, validate column =
  0xD09218.
- `Villager::IsStateEntryFunctionSameAs` 0x7524D0: compares the entry members of both rows (all four dwords; a first
  dword of 0 counts as equal at once, 0x7524F4). Rows sharing an entry: EnterInScript, EnterPlayAnim, EnterBuilding,
  EnterFarming, EnterFishing.
- The hand's own entry and exit are EnterInHand 0x76AFE0 and ExitInHand 0x76B000.
- IN_HAND's state function `Villager::InHand` 0x76ADE0 marks the villager's home (+0x7C |= 0x10) every turn, and
  ExitInHand clears it when it lets the villager go; the home then shows a teal person
  ([the population read-out](../hand/animation-and-placement.md)).
- WallhugValidate 0x756990 is an empty function (bw1-decomp Villager.cpp:1959; in v1.42 its first byte is `ret`, the
  rest padding). It is the validate of rows 1, 47, 71 and 154 (the validate column's id 1), and the state turn ignores a
  validate's result.
- ExitMoveToPos 0x5EDDA0 (`Living::ExitMoveToPos`, bw1-decomp Living.cpp:169) is the exit column's id 1: rows 1
  MOVE_TO_POS, 2 MOVE_TO_OBJECT and 222 FOOTBALL_MOVE_TO_BALL (and the animals' walking rows). It calls
  `CircleHugInfo::Reset` 0x60A9F0 on this +0x70 with this (`SetObjectPtr`(NULL) 0x60A770: no circle held; TurnsToObj
  +0x74 = 0xFF; EntryDistance +0x76 = 0; out of g_CircleHugStateInfo's lookahead sets +0x30 and +0x20 and its map of
  extended entry distances +0x00), then clears the wall hug's target object +0x60 (0x5EDDAC) and returns 1 whatever
  the next state (0x5EDDB3, `ret 4`). It keeps the walk's move state, goal and step.
- **The state-table column "+0xEC"**: rows are 0x114 bytes from Infos 0xDB9E68, file offset = memory offset − 0x10.
  0xDB9F54 (`SetStateCarriedObject` 0x750241/0x750268/0x750288) is memory +0xEC = file 0xDC; 0xDB9F64
  (`IsAvailableForReaction` 0x7633BA) is memory +0xFC = file 0xEC. Both readers read the right column.
- Other rows and clip functions:

| state | function | entry / exit / clip |
|---|---|---|
| 13 SET_DYING | StateSetDying 0x5AFF40 = jmp [vt +0x6A4] = SetDying 0x76A4C0 | |
| 14 DYING | StateDying 0x5AFE30 = jmp [vt +0x89C] = `Villager::Dying` 0x76A570 | clip DyingAnimation 0x423770 |
| 15 DEAD | StateDead 0x5AFE90 = jmp [vt +0x8A0] = `Villager::Dead` 0x76A5E0 | clip DeadAnimation 0x4237A0 |
| 19 / 20 / 21 / 22 | Living thunks 0x5B0050 / 0x5AFE20 / 0x5B00F0 / 0x5AFE80 → vt +0x8AC / +0x8BC / +0x8B0 / +0x8C0; `Villager::GotoFoodReaction` 0x7646A0, `Villager::GotoWoodReaction` 0x7646D0 | exit thunk 0x5B0100 = ExitReaction 0x7527A0, validate 0x756A00 (ReactionValidate); +0x50 AlwaysReactToTownEmergency 0x5AC990 |
| 23 WAIT_FOR_ANIMATION | IsReadyForNewAnimation(1) → SetTopStateToFinal 0x5ECA80 and 0, else 1 (0x5EC995) | |
| 27 MOVE_IN_FLOCK | thunk 0x5B0190 → vt +0x8B4 = `Living::MoveInFlock` 0x5ECDB0 | none |
| 28 MOVE_ALONG_PATH | StateAlongPath 0x5B0040 → vt +0x52C = `Living::MoveAlongPath` 0x5EE230 | EnterInScript 0x5ED7E0 / ExitInScript 0x5ED9C0 |
| 33 / 34 | GotoStoragePitForFood 0x769830 / ArrivesAtStoragePitForFood 0x7698B0 | |
| 39-41, 184 | 39 `Villager::ArrivesAtStoragePitForBuildingMaterials` 0x758990 | entry EnterBuilding 0x759750 (0 = refused → 163), exit ExitBuilding 0x7597B0; +0x50 AlwaysReactToTownEmergency 0x5AC990; SaveBuilding 0x754A00 / LoadBuilding 0x754A60 |
| 47-50, 52 | | exit ExitForesting 0x75FE20 (+0x118 = 0; 1); 47's validate WallhugValidate 0x756990 |
| 48 FORESTER_GOTO_FOREST | ForesterGotoForest 0x75F710 = jmp CheckSatisfyWoodDesire 0x75F4A0 | |
| 51 / 54 | ForesterChopsTreeForBuilding 0x75FB40 (SetTopState(163), 1) / ArrivesAtBigForestForBuilding 0x75FAB0 (`return 1`) | |
| 52 | ForesterFinishedForestering 0x75FB60: (int16) wood > 0 → GotWoodDecideWhatToDo, else SetTopState(163) | |
| 55 / 56 | `Villager::FishermanArrivesAtFishing` 0x75B5D0 / Fishing 0x75B6A0 | EnterFishing 0x75B820 / ExitFishing 0x75B880; +0x50 AlwaysReactToTownEmergency |
| 67 / 68 / 69 | `Villager::FarmerArrivesAtFarm` 0x759D20 (clip 259 P_FARMER_SOWING_SEEDS) / FarmerPlantsCrop 0x759EC0 / FarmerDigsUpCrop 0x759E40 (clip 257 P_FARMER_HARVESTING) | EnterFarming 0x75A250 / ExitFarming 0x75A2A0 |
| 110 / 111 / 112 | HousewifeStartsGivingBirth 0x7621A0 / HousewifeGivingBirth 0x762430 / HousewifeGivenBirth 0x7624A0 | none |
| 113 CHILD_AT_CRECHE | ChildAtCreche 0x757C90 | none |
| 114 CHILD_FOLLOWS_MOTHER | `Villager::ChildFollowsMother` 0x7578C0 | none; +0x50 AlwaysReactToTownEmergency (0xD0D208 = 0x5AC990, written at 0x5A4046..0x5A404B) |
| 117 EAT_FOOD | EatFood 0x75C000 | clip 254 EatDinner |
| 118 EAT_FOOD_AT_HOME | EatFoodAtHome 0x75C090 | clip -4 |
| 131 MORN_DEATH | `Villager::MornDeath` 0x76AA60 = jmp GoHome 0x760270 | into / out-of clip MournIntoOutofAnimation 0x424300 |
| 186 / 187 | TakeWoodFromTree 0x75FBC0 (clip ForesteringAnimation 0x423AC0) / TakeWoodFromPot 0x75FBB0 (`return 1`) | |
| 197 AFTER_TAP_ON_ABODE | `Villager::AfterTapOnAbode` 0x761440 | clip 273, YawnAnimation 0x424050 |
| 200 SCRIPT_PLAY_ANIM | ScriptPlayAnim (0x768973..0x7689A9) | EnterPlayAnim; exit vt +0x914 = ExitInScript 0x5ED9C0 |
| 205-208 | 0x766680 / 0x766700 / 0x766810 / 0x766850 | 208: MournIntoOutofAnimation 0x424300 |
| 209 NOTHING_TO_DO | 0x760000 (`mov eax, 1; ret`) | |
| 221 DISCIPLE_NOTHING_TO_DO | 0x7540D0 | entry 0x754140; save / load 0x755760 / 0x755780 |
| 238 SLEEP_IN_TENT | `Villager::SleepInTent` 0x761AE0 | clip 381, SleepInTentIntoOutofAnimation 0x424290; +0x50 AlwaysReactToTownEmergency |
| 239 PAUSE_FOR_A_SECOND | `Villager::PauseForASecond` 0x76B0B0 (no entry, exit, save or load; +0x50 AlwaysReactToTownEmergency) | clip function PauseForASecondAnimation 0x424080 (poisoned: 342, no draw; else GameRand(2): 1 → 333, 0 → 332) |
| 243 | `Villager::CongregateInTownAfterEmergency` 0x76B300 | TownEmergencyAnimation 0x424100 |
| 244 SCRIPT_IN_CROWD | `Villager::ScriptInCrowd` 0x756540: IsReadyForNewAnimation(1) → SetStateAnim 0x5ECB10 (the state's clip, nothing when it is the one playing) and +0x90 = 0; 1 either way. openblack: `villager::ScriptInCrowd` (`VillagerScript.cpp`), the crowd clip drawn by `RandomCrowdClip` | none |
| 246 | `Villager::SitAndChillout` 0x76B4E0, entry EnterSitAndChillOut 0x76B570 | SitDownAnimation 0x424210 (+0x60), SitDownIntoOutOfAnimation 0x4243A0 (+0x70) |
| 249 ARRIVES_HOME_FROM_WORSHIP | 0x76B7E0 = jmp ArrivesHome 0x760930 | exit ExitAtHome |
| 250 SLEEP_IN_TENT_FROM_WORSHIP | 0x76B7F0 = jmp SleepInTent 0x761AE0 | |
- `IsAvailableForReaction`: 0x7633C5: +0xE0 & 0x80 (football / script) -> 0
- `IsAvailableForReaction`: 0x7633E0..0x7633FA: GetLife (vt +0x11C) vs LifeWhenCrawlsWounded (+0x380): `test ah, 0x41; je` goes on when above; at or below (or NaN) only type 7 REACT_TO_FOOD goes on

#### The original's function ids

The distinct addresses of each column of the table, numbered in table order (0 = no function there). Only equalities
within a column are used (IsStateEntryFunctionSameAs, IsStateExitFunctionSameAs, the reaction validate and the
building exit), so a port can keep these ids instead of the addresses:

- entry: 1 = 0x5ED7E0, 2 = 0x5EDA50, 3 = 0x767410, 4 = 0x76AFE0, 5 = 0x5EDDD0, 6 = 0x759750, 7 = 0x75B820, 8 =
  0x75A250, 9 = 0x76ACE0, 10 = 0x7687D0, 11 = 0x768840, 12 = 0x75ADC0, 13 = 0x75AF30, 14 = 0x754140, 15 = 0x76A2A0,
  16 = 0x76B570, 17 = 0x761800.
- exit: 1 = 0x5EDDA0, 2 = 0x5ED9C0, 3 = 0x5EDB10, 4 = 0x7527A0, 5 = 0x76ACB0, 6 = 0x5ED580, 7 = 0x768640, 8 =
  0x767420, 9 = 0x5EDDC0, 10 = 0x76B000, 11 = 0x5EE090, 12 = 0x761B40, 13 = 0x7597B0, 14 = 0x76C120, 15 = 0x75FE20,
  16 = 0x75B880, 17 = 0x76C170, 18 = 0x76C1F0, 19 = 0x76C280, 20 = 0x75A2A0, 21 = 0x763280, 22 = 0x7527E0, 23 =
  0x768F50, 24 = 0x76AD80, 25 = 0x768780, 26 = 0x768830, 27 = 0x7689C0, 28 = 0x766390, 29 = 0x759A90, 30 = 0x75AE80,
  31 = 0x75AF80, 32 = 0x76A2D0, 33 = 0x761980.
- validate: 1 = 0x756990, 2 = 0x7569A0, 3 = 0x7569D0, 4 = 0x756A00, 5 = 0x756A50, 6 = 0x756A80, 7 = 0x756AA0.

Each row's state function, by row:

0 0x5EC1D0; 1 0x5EC270; 2 0x5EC2C0; 3 0x5ECD00; 4 0x5ED9A0; 5 0x5EDAD0; 6 0x5AFFE0; 7 0x5B0060; 8 0x5B01B0; 9
0x5B0240; 10 0x5EC330; 11 0x7606E0; 12 0x5B0230; 13 0x5AFF40; 14 0x5AFE30; 15 0x5AFE90; 16 0x76A780; 17 0x5AFF30; 18
0x5AFFD0; 19 0x5B0050; 20 0x5AFE20; 21 0x5B00F0; 22 0x5AFE80; 23 0x5EC990; 24 0x5AFF20; 25 0x5F2640; 26 0x765320; 27
0x5B0190; 28 0x5B0040; 29 0x5EDDE0; 30 0x5B00E0; 31 0x769620; 32 0x7696D0; 33 0x769830; 34 0x7698B0; 35 0x769B30; 36
0x760270; 37 0x760930; 38 0x760B10; 39 0x758990; 40 0x758AF0; 41 0x758C40; 42 0x76BFA0; 43 0x76C080; 44 0x76C100; 45
0x76C110; 46 0x76C2D0; 47 0x75F7D0; 48 0x75F710; 49 0x75F930; 50 0x75FAC0; 51 0x75FB40; 52 0x75FB60; 53 0x75F9E0; 54
0x75FAB0; 55 0x75B5D0; 56 0x75B6A0; 57 0x5EC310; 58 0x76BCC0; 59 0x76BE00; 60 0x76C680; 61 0x76CB00; 62 0x76CB80; 63
0x76CBB0; 64 0x76CBE0; 65 0x76C390; 66 0x76C3C0; 67 0x759D20; 68 0x759EC0; 69 0x759E40; 70 0x763110; 71 0x763120; 72
0x763140; 73 0x75D230; 74 0x75EA10; 75 0x75E170; 76 0x5319C0; 77 0x5319D0; 78 0x75F270; 79 0x75F2C0; 80 0x75F2F0; 81
0x75F300; 82 0x763170; 83 0x7631F0; 84 0x7631B0; 85 0x753DD0; 86 0x76B990; 87 0x76B980; 88 0x76A7E0; 89 0x76A8B0; 90
0x76C7C0; 91 0x768C30; 92 0x768CC0; 93 0x7690D0; 94 0x769070; 95 0x768FB0; 96 0x768DD0; 97 0x769390; 98 0x769460; 99
0x768E30; 100 0x761C10; 101 0x761CE0; 102 0x761D60; 103 0x761EA0; 104 0x761F10; 105 0x761F60; 106 0x761FA0; 107
0x761FC0; 108 0x761FF0; 109 0x762020; 110 0x7621A0; 111 0x762430; 112 0x7624A0; 113 0x757C90; 114 0x7578C0; 115
0x757F10; 116 0x760B20; 117 0x75C000; 118 0x75C090; 119 0x760B30; 120 0x760D70; 121 0x760E50; 122 0x760E60; 123
0x760EE0; 124 0x760F50; 125 0x760F80; 126 0x761010; 127 0x761020; 128 0x761030; 129 0x761320; 130 0x76A8D0; 131
0x76AA60; 132 0x764410; 133 0x764490; 134 0x764610; 135 0x764650; 136 0x764680; 137 0x764690; 138 0x764660; 139
0x764670; 140 0x766C60; 141 0x766D00; 142 0x766D60; 143 0x766E50; 144 0x766FB0; 145 0x75C0F0; 146 0x7678A0; 147
0x767970; 148 0x767A00; 149 0x764AA0; 150 0x764B50; 151 0x764D10; 152 0x764D70; 153 0x759930; 154 0x759990; 155
0x767BA0; 156 0x767CE0; 157 0x767DC0; 158 0x765140; 159 0x767E00; 160 0x767C80; 161 0x7653F0; 162 0x765450; 163
0x7515C0; 164 0x756E10; 165 0x76AA70; 166 0x758390; 167 0x767F70; 168 0x765E00; 169 0x7613F0; 170 0x757260; 171
0x757270; 172 0x757420; 173 0x757590; 174 0x757610; 175 0x757690; 176 0x757720; 177 0x757730; 178 0x757750; 179
0x757760; 180 0x757850; 181 0x757740; 182 0x758EF0; 183 0x758F00; 184 0x758F60; 185 0x7592E0; 186 0x75FBC0; 187
0x75FBB0; 188 0x75FB90; 189 0x75FBA0; 190 0x7690E0; 191 0x768EC0; 192 0x768F20; 193 0x759960; 194 0x7668E0; 195
0x766910; 196 0x766930; 197 0x761440; 198 0x768A10; 199 0x7687F0; 200 0x768970; 201 0x7662F0; 202 0x7663F0; 203
0x766130; 204 0x759A00; 205 0x766680; 206 0x766700; 207 0x766810; 208 0x766850; 209 0x760000; 210 0x759520; 211
0x759450; 212 0x75B940; 213 0x76C5E0; 214 0x766A90; 215 0x765870; 216 0x75AC50; 217 0x75AFE0; 218 0x75B000; 219
0x75B1E0; 220 0x75A7E0; 221 0x7540D0; 222 0x75F380; 223 0x769D20; 224 0x769DC0; 225 0x76A1B0; 226 0x76A220; 227
0x767280; 228 0x7691A0; 229 0x756D30; 230 0x761510; 231 0x763B40; 232 0x7586B0; 233 0x76AC40; 234 0x761810; 235
0x7673A0; 236 0x763D90; 237 0x763F80; 238 0x761AE0; 239 0x76B0B0; 240 0x76B1C0; 241 0x76CA30; 242 0x76B200; 243
0x76B300; 244 0x756540; 245 0x76B3F0; 246 0x76B4E0; 247 0x768A20; 248 0x761B70; 249 0x76B7E0; 250 0x76B7F0; 251
0x766380; 252 0x76B590; 253 0x7615C0; 254 0x76B800.

#### Exits of the reactions (ExitReaction, ExitReactToTeleport)

- **ExitReaction 0x7527A0** (vt +0x910; the rows store the thunk 0x5B0100 = `jmp [vt +0x910]`, 45 rows in `_$E32`):
  `CircleHugInfo::Reset`(+0x70) 0x60A9F0, and if `s` is not reactive (IsReactiveState inline, Infos +0xC8 0xDB9F30 =
  file 0xB8) StopReacting (vt +0x998). Returns 1. Its rows include 215, 6, 7 and 30 (running from and watching a
  miracle), 19-22 (food and wood), 8, 9, 203, 214, 12, 25, 26, 140-168, 194-196, 205-208, 227, 231 and 235-237.
  ExitPutOutFire calls it at the end (0x75AF19).
- **`Villager::StopReacting` 0x7637D0** → **`Living::StopReacting` 0x5F1140**: with TOP 203 and dancing,
  RemoveFromDance(1); with a reaction (+0x94): out of the reaction's list, `fn_005F0FE0(type)` = the type's record
  receives the turn, +0x94 = 0; +0xBC = 0 always. EnterPutOutFire (0x75AE55) also uses it; a mourner leaves one taker
  fewer (0x5F1186 `dec` reaction +0x1C).
- **ExitReactToTeleport 0x766390** (exit of 201, 202 and 251): if not `IsStateExitFunctionSameAs(s)` (vt +0x96C
  0x752530), it leaves its town's on-the-way-to-worship list (GetTown vt +0x48 →
  `Town::RemoveVillagerOnWayToWorshipSite` 0x73E360, 0x7663B3..0x7663C4) and +0xE0 &= ~0x10 (also
  without a town); then ExitReaction(s) and its result.
- **`Villager::IsStateExitFunctionSameAs` 0x752530**: the exit of the GetFinalState row and that of `s` are the same
  (the whole 16-byte pointers; two empty ones count as equal) → 1; otherwise, `s` final (0xDB9E84) → 0, otherwise 1.
  ExitPutOutFire (0x75AE95) uses it.
- **A creature's reaction rows** (146-148, 155-157, 159, 160 and 167, reaction 6): the exit is the ExitReaction thunk,
  the validate `ReactionValidate` 0x756A00, but 167's `WallhugAndReactionValidate` 0x756A80 (the empty
  `WallhugValidate` 0x756990, then `ReactionValidate`); save / load `SaveInspectionReaction` 0x755B80 / `Load` 0x755C20
  for 146-148, `SaveReaction` 0x7558C0 / `LoadReaction` 0x7558D0 for the others; the clip `InspectCreatureAnimation`
  0x423EB0 for 146-148 and 160, `RespectCreatureAnimation` 0x423F60 for 155-157, the info table's for 159 and
  `MoveToPosAnimation` 0x423400 (into / out of `MoveToPosIntoOutofAnimation` 0x4242B0) for 167. Their state functions
  are on [Villager reactions](villager-reactions.md#a-creature-reaction-6).
- `TownEmergencyReaction`: +0x50 of each row (0xD091E8 + 0x90 x state; the member pointer's this-adjustment, +0x54, is 0 in all 255): the town emergency's answer, read only by CallAllVillagersToTownEmergency 0x747890 (0x7478CB, 0x747902) through GetFinalState . Always = Villager::AlwaysReactToTownEmergency 0x5AC990 (`mov eax, 1; ret`, 206 rows), PreviousState = Villager::LookAtPreviousStateReactToTownEmergency 0x756520 (returns byte +0x8E, the PREVIOUS state; only 220 MOVE_AROUND_FIRE), None = an empty slot (48 rows: not called).

### Creation

`Villager::Create` 0x74FBE0 first rolls `GameRand(10) <= 1` (0x74FBF0) to try a SpecialVillager. Afterwards, the
constructor 0x74F950:

1. `Living::Living` 0x5EBEC0 (life = info.life; at 0x5EBF89 it draws GameRand(startAge >> 1), GLivingInfo +0x134, and
   gives the direct `Living::SetAge` 0x5ED2C0 the draw + startAge − (startAge >> 2), which step 2 overwrites: only the
   draw is left; every villager row has start age 25 or 20. Its only callers are the Animal 0x416EB0, Creature
   0x473B90 and Villager ctors; loading uses the default ctors, which draw nothing) and SetToZero 0x74FB20 (all new
   fields to 0).
2. SetAge 0x7528C0: child if `age < grownUpAge` (`flags |= 8`), otherwise `age = max(age, 18)` and `flags &= ~8`;
   meshes and scale (consumes FloatRand); `birthTurn = turn − age·1500`.
3. food speed-up = 0 (0x74FA02); woman (info +0x1F8 == 1): pregnancy = 0 (0x74FA08).
4. food (0x74FA18..0x74FA67): `min(1, GameFloatRand(0.6) + hungryForFood)` is a macro that evaluates twice: if the
   first roll gives < 1 it rolls again and stores the second one without clamping.
5. last check turn (0x74FA6D..0x74FACD): `turn − (GameRand(processChecksEvery) < turn ? GameRand(...) : turn)`. The
   second roll can exceed the turn: on turn 6 with a roll of 7 it ends up 0xFFFFFFFF (literally; the unsigned
   subtraction of GetGameTurnsSinceLastChecked leaves it correct).
6. State counter Object +0x58 = GameRand(500) + 1 (0x74FAB0).
7. **Water rule** (0x74FADC..0x74FAF5): `SetState(TOP, IsWater(pos) ? 16 DROWNING : 85 CREATED)`, the exact
   SetState, without entry, clips or speed, and without calling anything of the water. `IsWater` =
   `MapCoords::IsWater` 0x6035B0.
8. `++g_game+0x205A54` and SetSkeleton 0x7562C0 (0x74FB0C) with Create's fourth argument: the status bit, no meshes
   yet (the 3D object +0x40 is still null), then SetScaleForAge(GetAge) at 0x756436, whose scale is the villager's
   final one. Only ChildBorn 0x7622B9 passes 1 (the mother's IsSkeleton); every other caller passes 0.

Afterwards CallVirtualFunctionsForCreation and AddVillagerToAbode set the home and the town, and the first
SetTopState the speed of 85.

The Land 1 intro script `FollowUs` (it sits between setupland1.txt and setupland2.txt in the chl file order) creates
three villagers in open sea (1578, 2220) on turn 6: they are born in 16 DROWNING. They are the intro's family (CHL
CREATE of type 4 with sub_types 49, 53 and 52): CREATE then sets their age again and puts them in 4 IN_SCRIPT,
drowning or not ([Creation from CHL](../scripts/map-script-reader.md#creation-from-chl)).

- The special-villager roll calls `SpecialVillager::Create` 0x71F1A0. If the special villager comes back without
  +0xA & 1, it is the result (0x74FC1F). Its `Create` calls vt +0x658 too (0x71F21D) and shares `Villager::GetAnimId`
  (SpecialVillager +0x900).

#### The special villager

✅ (exe, oracle) What a successful roll does, in order (`Villager::Create` always passes a null special info, 0x74FC0D):

1. `SpecialVillager::Create` 0x71F1A0: in a multiplayer game (`IsMultiplayerGame` 0x552F80) it returns null before
   any draw (0x71F1A8..0x71F1B5), and a normal villager follows.
2. `GSpecialVillagerInfo::GetRandomName` 0x71F4B0 (called with the villager info's sex +0x1F8, job 0 and 1; the third
   argument is not read): the table's size n (fn_0071F900: 0x30 in multiplayer, else [0xD9B160]); n = 0 returns null
   with no draw. Otherwise **one draw** `GameRand(n)` (0x71F4CA) gives the first row looked at; then each of the n
   rows in turn, `(r + 1) % n`, until one whose sex +0x44 matches (a sex of −1 takes any, 0x71F4E8), whose job +0x48
   matches or is 0 (skipped: the job asked is 0, 0x71F4F6), whose use count +0x5C is 0 and whose name +0x10 is not
   empty. None: null, and a normal villager is made after the one draw.
3. Base::new(0x134) and the constructor 0x71F0E0: the whole `Villager` constructor 0x74F950 (every draw of the
   steps above, as for a normal villager), InfoIndex +0x130 = the row, the row's use count + 1, then
   `Villager::SetAge` 0x7528C0 again with the row's age +0x40, unless the age asked is 1 (0x71F139: a newborn keeps
   it). That SetAge's InitialiseScale and SetScaleForAge draw again (two draws for an adult).
4. vt +0x658 (0x71F21D, `Villager::CallVirtualFunctionsForCreation` 0x74FC70, the same slot as Villager's), then
   the use count + 1 again (0x71F223): a used name counts 2.
5. +0xA & 1 is not written on that path (none of the constructor's callees nor 0x74FC70, 0x5EC9B0, 0x636BE0 writes
   it), so the check at 0x74FC1F is not seen to fail (inferred).

The table (`SpecialVillager::InitOnStartup` 0x71F8F0 → 0x71F640, once at start-up): 148 rows of 0x60 at
`InfoList` [0xD9B154]. It reads the mail link's contacts (`LHMail::GetContactNames`) into the first rows (name
`strncpy` 47, age `LocalRand(40) + 11`, sex alternating 0, 1 in the order read, job 0, married `LocalRand(2)`, tribe
−1, pet `LocalRand(31)`, face 0: local draws only), then copies the info's own 48 (`GSpecialVillagerInfo::Infos`
0xD9B168, from info.dat) after them; the count is 48 in all (the contacts and 48 − contacts own rows, 0x71F7AB..
0x71F862; with 48 contacts or more, only the contacts, 0x71F870). With no mail link the table is the info's 48. `GSpecialVillagerInfo::OnClearMap`
0x71F930 sets every use count to 0. `SpecialVillager` overrides only ToBeDeleted (the speech bubble), Load / Save
(InfoIndex), the save type 0x76, Draw (the bubble), GetVillagerName (the row's name) and DrawVillagerInfo: it lives
as any villager of its row.

Oracle: `GetRandomName` run natively on synthetic tables (48 rows, sexes alternating; seeds 0x88F89F, 0x12345678,
0xCAFEBABE, 1 give the draws 6, 11, 34, 18): man 6 / 12 / 34 / 18, woman 7 / 11 / 35 / 19, any = the draw; one free
man at row 46 → 46; only row 2 free, from 34 → 2 (round past the end); no free man → null after one draw; the first
40 names empty → 40; n = 0 → null, no draw. Pinned in `test/test_special_villagers.cpp` (`SpecialVillagers.PickMatchesTheOriginal`).
- `Villager::InitialiseScale` 0x74FB80 (0x74FB88..0x74FBAB): a child (age < grownUpAge) takes ageToScale[age − 1],
  where ageToScale is GVillagerInfo +0x2E8. At age 0 this reads the dword before the table (+0x2E4, dancingSpeed) as a
  float. An adult takes 0.9 (0x3F666666).
- `Villager::SetScaleForAge` 0x752A90, adult (0x752AF5..0x752B2F): t = (0.05 − GameFloatRand(0.1)) + 1. If the
  current scale < t, the scale becomes a SECOND draw (0.05 − GameFloatRand(0.1)) + 1; otherwise it stays. An adult
  coming from InitialiseScale (0.9) therefore always uses two draws. Child: current + GameFloatRand((ageToScale[age +
  1] − current) × 0.75) (0x752AA4..0x752AE9).
- SetAge (0x752919..0x752A5C) changes the meshes only when the age crosses grownUpAge, comparing against the old
  birth turn. A new child gets +0x20C / +0x208 / +0x204; a new adult gets GetDetailMesh(2 / 1 / 0). InitialiseScale
  runs at 0x752A62 and SetScaleForAge at 0x752A6D.
- The constructor's fourth argument is SetSkeleton (0x74FB0C, `Villager::SetSkeleton` 0x7562C0): status bit 0x40,
  the skeleton mesh or the villager's own meshes, then a SetScaleForAge draw (0x756436).
- The world population g_game +0x205A54 goes down in SetDying and in ~Villager 0x74FBC0, only for a villager not yet
  counted out (+0xE0 & 0x40).
- `Villager::CallVirtualFunctionsForCreation` 0x74FC70 (vt +0x658) calls Living's 0x5EC9B0 and Object's 0x636BE0
  (not MobileObject's): no draws, the scale handed to the new 3D object again (0x636C9F), SetShadowed(1) at 0x74FD4C.
  The villager goes to the head of its cell's mobile list (Object 0x636740) (inferred: after the town and the house).
- `CreateVillagerAtAbode` writes [0xD99384] = the villager (null too); [0xD99380] gets the same, with no reader known.
- CREATE_TOWN_VILLAGER (0x715A61) does not check the villager index and passes 0xDA6BE8 + 0x54 x 0x3A4 = 0xDB9DB8, the
  end of `InfoList` (never written, vtable 0), to `Villager::Create`, which reads its fields from the runtime statics
  that follow (VillagerName::First at +0x70, then state-table rows 0..2). No shipped script names an unknown villager
  (CREATE_VILLAGER is unused; only CreatureIsle and mod scripts carry other names).
- `RollSpecialVillager`: 0x74FBF0: GameRand(10) <= 1 -> SpecialVillager::Create 0x71F1A0; a special one that comes back without +0xA & 1 is the result (0x74FC1F).
- Villager::Create 0x74FBE0's first draw: GameRand(10) <= 1 tries SpecialVillager::Create 0x71F1A0, whose name pick
  is the second draw ([the special villager](#the-special-villager)).

### The turn

The original keeps one living list for villagers and animals together (g_game +0x205BBC, newest first). Each villager
takes `ProcessReaction` 0x5F1270 (see [reactions 7 and 12](villager-jobs.md#food-and-wood-jobs-reactions-7-and-12))
and `ProcessState`, its path step (`Living::MoveToPos` 0x5EC270) inside the state functions that walk; each animal
takes its own turn among them.

ProcessState 0x74FF70:

1. `++turnsSinceStateChange` (+0x90) and ProcessFoodSpeedup 0x753430 (food speed-up != 0 && turn % 10 == 0 → −1).
2. validate (+0x80) of the TOP (+0x8C, 0x74FF91) and of the raw FINAL (+0x8D, 0x74FFD9), only if the row has one; the
   result is not used. The reaction rows (201, 202, 251, 214-218, 220, 6-30, 140-196... all those with the validate
   0x756A00) call ReactionValidate 0x756A00: with no reaction object (+0xBC), or not available, or in the hand if the
   reaction asks for it → `PopFromPrevious` 0x751E50. That is why `ReactToFire` 0x765870 only returns 0 (without
   changing state) when the object is not an `Object` or has no fire, and `GoToTeleportReaction` 0x7662F0 checks
   nothing.
3. If an entry / exit clip is playing (flag 0x800): it waits for it to finish (IsReadyForNewAnimation 0x5EC960 →
   FinishedIntoOutOfAnimation 0x750060) and does nothing else that turn.
4. CheckEveryTime 0x750410 and CallState 0x7521D0 (the TOP's function).

CheckEveryTime:

- controlled by a script (+0x25 & 4; written by `GameThingWithPos::SetControlledByScript` 0x402240) → nothing;
- wear: if the TOP is moving, life drops by the TOP's `LifeDrainPerTurn` and from there the row of the **raw FINAL**
  is looked at; otherwise, it drops by that of the final state (GetFinalState) and the TOP's row is looked at
  (literal quirk);
- if the row has checks (`DoPeriodicChecks`): life == 0 exactly → VillagerDead(CHANT if the final is 248-250 or flags
  & 2, otherwise EXHAUSTION); if **more than** `processChecksEvery` (8) turns have passed → the periodic check, every
  9 turns: CheckDeathFromOldAge when `(turn + UniqueId) % 800 < t`; hurt (`life < 0.3`, outside the home, the row
  allows going home, not knocked down, and in 19/20 only with `food > hungryForFood`) → SetTopState(36 GO_HOME)
  (0x7505C3), which walks it home (GoHome 0x760270 → DoGoingHome 0x760280: home, path, ARRIVES_HOME);
  CheckChildGrownUp, WomanSpecial and CheckHungry (which resets the last check turn);
- otherwise: the disciple check (flags & 0x200, a type that ignores needs according to g_DiscipleInfos +0xC, raw FINAL
  221, town with +0x5E8, the town's "something changed" pulse) → 163.

- The living list: head g_game +0x205BBC, count +0x205BC0, next = Living +0xA4. `Living::Living` 0x5EBEC0 inserts
  every new Living at the head (0x5EBFBD..0x5EBFD4), so the list runs newest first with villagers, animals and
  creatures mixed (the Creature ctor 0x473B90 calls `Living::Living` at 0x473B9F). `Living::ToBeDeleted` 0x5EC0A0
  unlinks a Living and zeroes its next (0x5EC11A). `Living::RemoveFromGame` 0x5EE560 (vt +0x7DC) also unlinks, but
  nothing in the game calls that slot. A loaded game takes the saved list order (LoadAllGame 0x558B64; not read).
- `Living::ProcessLiving` 0x5EC810 reads next (+0xA4) before each turn (0x5EC8A8). As a result: a Living unlinked by
  an earlier turn is skipped; one unlinked during its own turn does not stop the loop; if the next one is unlinked
  during the current turn, it still takes its turn (it is still in memory) and the loop then ends (its next is 0); a
  Living created during the loop sits at the head and first runs on the next turn.
- Per Living: Pos (+0x14) is copied to +0x2C (0x5EC8AE..0x5EC8C1). Then a villager runs ProcessReaction (0x5EC8C6)
  and ProcessState (vt +0x620, 0x5EC8CF); an animal runs `Animal::ProcessState` 0x417EE0. After the loop, 0x5EC8E1
  writes g_game +0x2502CC = 0x14: the displacement 0x2502CC occurs once in the exe, at that write, and nothing reads
  0x2502CD..CF; no sub-object covers the offset. Before the loop, 0x5EC811..0x5EC898 sets the creatures' Creature3D
  +0x6405C from [0xBE026C] = (CreatureList count [0xC5FCFC] < 3) + 1.
- The 18 villager rows whose state function reaches `MobileWallHug::MoveTo` 0x60AF20 (directly, or through
  `Living::MoveToPos` / MoveToObject / PerformDance 0x5EF9F0): 1 MOVE_TO_POS 0x5EC270, 2 MOVE_TO_OBJECT 0x5EC2C0, 3
  MOVE_ON_STRUCTURE 0x5ECD00, 5 IN_DANCE 0x5EDAD0, 29 MOVE_ON_PATH 0x5EDDE0, 47 FORESTER_MOVE_TO_FOREST 0x75F7D0, 60
  0x76C680, 66 RESTART_WORSHIPPING_CREATURE 0x76C3C0, 71 FOOTBALL_WALK_TO_POSITION 0x763120, 90 WORSHIPPING_CREATURE
  0x76C7C0, 140 MOVE_TOWARDS_OBJECT_TO_LOOK_AT 0x766C60, 153 DANCE_FOR_EDITING_PURPOSES 0x759930, 154
  MOVE_TO_DANCE_POS 0x759990, 167 MOVE_TOWARDS_CREATURE_REACTION 0x767F70, 193 DANCE_BUT_NOT_WORSHIP 0x759960, 203
  DANCE_WHILE_REACTING 0x766130, 222 FOOTBALL_MOVE_TO_BALL 0x75F380, 230 ARTIFACT_DANCE 0x761510. No other villager
  state walks. Direct callers of MoveTo: `Living::MoveToPos` 0x5EC280, `Living::MoveOnFootpath` 0x5EDDE9,
  FootballWalkToPosition 0x763120, FootballMoveToBall 0x75F415, MoveTowardsCreatureReaction 0x767F88.
- `Living::MoveToPos` 0x5EC270 does nothing (returns 0, no MoveTo) while the villager is in a hand (+0x24 & 4,
  0x5EC273..0x5EC27C).
- A villager marked for deletion during CheckEveryTime still runs its state function that turn: there is no test
  between 0x750049 and CallState 0x750050.
- CheckEveryTime info offsets: processChecksEvery = GVillagerInfo +0x2DC; the hurt threshold damageThresholdToGoHome =
  +0x35C (life < it, strict); hungryForFood = +0x2C0. The "reacting to food" exception reads rows 19 / 20 (0xDBB2E4 /
  0xDBB3F8). The disciple check is at 0x750643.
- Town +0x5E8, the town's "something changed" pulse, is read by CheckEveryTime's disciple check, `Town::Process` and
  DiscipleNothingToDo 0x7540D8; its writers are under [Town lists and queries](towns.md#town-lists-and-queries).
- `Villager::IsHungry` 0x752600: food <= hungryForFood (+0x2C0). GetGameTurnsSinceLastChecked 0x750670 = turn −
  +0xEC (unsigned); SetGameTurnLastChecked 0x7506A0 sets +0xEC = the turn.
- POWER 0x75BB60 = 1 − min(x, 1)³, the cube computed by a loop of two fmul (0x75BB81..0x75BB8B). GetDesireForFood
  0x75BB50 = POWER(food).
- The two "turns per year" fields (1500 for the age, 36000 / 365.25 for the pregnancy) are on
  [Home, food, sleep, homeless and age](villager-life.md#home-food-sleep-homeless-and-age).
- `IsHungry`: 0x752600: fcomp food, info +0x2C0; test ah, 0x41 -> food <= hungryForFood
- `GetGameTurnsSinceLastChecked`: 0x750670: g_game +0x205A40 - +0xEC (unsigned)
- `SetGameTurnLastChecked`: 0x7506A0: +0xEC = g_game +0x205A40
- `CheckEveryTime`: 0x750471..0x75049F: GetFinalState's drain (0xDB9F70); `st` stays the TOP's
- `CheckEveryTime`: 0x750524..0x750546: (turn + UniqueId) % 800 < t (unsigned div by 0x320)
- Villager::IsHungry 0x752600: food <= info.hungryForFood (+0x2C0)
- Villager::GetGameTurnsSinceLastChecked 0x750670 / SetGameTurnLastChecked 0x7506A0

### State changes

Codes: 1 done, 0x2E the exit rejected (nothing changes), 0x2F the entry rejected.

- **`Villager::SetTopState`(s)** 0x752010: if `CanPauseForASecond(s)` (TOP ≠ 239, row with pause, no script): `x = 1
  − life` (life·0.5 if poisoned), and if `GameFloatRand(1) − 0.5·x³ < pauseForASecondChance (0.01)` →
  SetupPauseForASecond(s) 0x76B090 = SetCurrentAndDestinationState(239, s). Otherwise, `Living::SetTopState`; if it
  gives 0x2F, CallEntryStateFunction(163).
- **`Living::SetTopState`(s)** 0x5F28E0: exit(s) → 0x2E; `out` = CallOutofAnimationFunction(s) (0x5F2900); entry(s) →
  0x2F; SetStateSpeed (0x5F291B, unconditionally); if `out ≠ −1`, SetAnim(out); otherwise, SetStateAnim 0x5ECB10 and
  CallIntoAnimationFunction(s) (0x5F2947).
- **`Living::SetCurrentAndDestinationState`(c, d)** 0x5F2980: the same, but the exit (0x5F298B), the exit clip
  (0x5F299C) and the entry clip (0x5F29EB) receive **`d`**; only the double entry (vt +0x908) receives both.
- **CallExitStateFunction(s)** 0x752320: the TOP's exit and, if the final state is another one, also its own; 1 only
  if both give 1.
- **CallEntryStateFunction(s)** 0x7523D0: `entry[s](final, s)`; 1 → SetState(0, s). The double one 0x752440: that of
  `c`, then `entry[d](previous final, d)`; 1 → SetState(1, d).
- **`Villager::SetState`(i, s)** 0x753690: PREVIOUS does not store a state with 0x10; the old state of any index, if
  it is final, leaves the town modifiers, and the new one enters (also in PREVIOUS: literal quirk); setting TOP first
  clears FINAL (with its adjustment) and sets +0x90 to 0. It does not touch the counter +0x58, nor clips, nor speed.
- **AdjustTownModifier** 0x753560: `town.desire.doingNow[d] ±= amount` and `doingNowCount[d] ±= 1` (TownDesire
  floats, town +0x510 / +0x554).
- **SetupMoveToWithHug(pos, final)** 0x5F2890: SetCurrentAndDestinationState(GLivingInfo +0x124 `moveState`, final)
  (0x5F2894..0x5F28AF; the 63 villager rows of info.dat have 1 MOVE_TO_POS) and, only if it gives 1, the walk. Used
  by fire and teleport.
- **Arrival of MOVE_TO_POS** (`Living::MoveToPos` 0x5EC270): `MobileWallHug::MoveTo` == 0xA → SetTopStateToFinal
  (0x5EC28E) = `Villager::SetTopState`(FINAL): pause roll, exit of MOVE_TO_POS (ExitMoveToPos 0x5EDDA0,
  `CircleHugInfo::Reset`) and exit / entry of the FINAL. MoveTo 0x60AF20 only gives 0xA in ARRIVED (0x60AFC0, if
  AreWeThere(0)) and in FINAL_STEP (0x60AF6C), and both first put the object at the destination (Pos = destination,
  MoveMapObject vt +0x55C); otherwise it returns 0 / 1 / 6 / 7 and `Living::MoveToPos` does nothing: **there is no
  "abandoned" walk**. 0xA comes one turn after STEP_THROUGH sets FINAL_STEP.
- The cases where the destination is inside the circle being hugged, and the step from one circle to another, are
  `MoveToCircleHugCircleSquareSweep`<0/1> 0x614C40 / 0x6159F0 ([Walking round obstacles](wall-hugging.md)); a walk
  with no obstacles goes straight to the destination in STEP_THROUGH (0x60B02A); ARRIVED is left when the walker has
  **not** arrived (0x60AFC0).
- 163 is `Villager::DecideWhatToDo` 0x7515C0 (see
  [Deciding what to do and leisure](villager-decisions.md#deciding-what-to-do-and-leisure)).
- CallOutofAnimationFunction 0x756620 / CallIntoAnimationFunction 0x756590: see
  [Which clip plays](animation.md#which-clip-plays).
- `Villager::GetFinalState` 0x751DD0 (vt +0xB04): the TOP if its row is final (0xDB9E84, file 0x0C), else the FINAL.
- `Living::SetTopState`: when the entry is refused (0x2F), the transition flags and the out-of clip already set stay
  as they are (literal).
- `Villager::SetCurrentAndDestinationState` is 0x7520E0 (vt +0x8DC); a 0x2F there also runs
  CallEntryStateFunction(163) (0x752105).
- PlayAnimThenSetState(s) 0x5ECAC0: if CallExitStateFunction(s) is 0, nothing happens (0x5ECAD3). Otherwise
  CallEntryStateFunction(23, s) runs (the double entry, vt +0x908): TOP = 23 WAIT_FOR_ANIMATION, FINAL = s, with no
  clip of its own.
- SetupWaitForCounter (0x76B060; 0x76B06E..0x76B086): SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final). Only
  when that gives 1 is the state counter +0x58 set to the count; otherwise it returns 0 and leaves the counter alone.
- `Living::SetupMoveToPos` 0x5F2830 (also `Villager::SetupMoveToPos` 0x763800): the walking state is the byte at
  GLivingInfo +0x124, or 3 MOVE_ON_STRUCTURE when +0x24 & 0x80. It calls SetCurrentAndDestinationState(walk, final),
  and if that gives 1, SetupMobileMoveToPos.
- `MobileWallHug::SetupMobileMoveToPos` 0x60AAD0: +0x80 = the goal, InitStepsXZ, the object leaves the circle-hug
  lists, then AreWeThere(0) → +0x5E = 1 ARRIVED (0x60AB91); otherwise `CircleHugInfo::Reset`, +0x78 = 1 and +0x5E =
  0xB STEP_THROUGH.
- `MobileWallHug::InitStepsXZ` 0x60BFA0: a = `GUtils::GetAngleFromXZ`(Pos, goal) 0x74D240; SetTowardsAngle(a) (vt
  +0x868, a villager's 0x473E40 = SetGameAngle 0x60DA90, with no turn limit); step x = (COS[a] 0xC31E14 × (+0x5A >>
  4)) >> 12, z the same with SIN 0xC31614, y = 0.
- AreWeThere (0x60AD64..0x60ADB0): R = speed (+0x5A as a float) + r; arrived when dx² + dz² < R² (strict).
  AreWeThereAtDestination uses GetDestPos (vt +0x860). dx and dz are the int32 differences of the two MapCoords
  (0x60AD68, 0x60AD7E), loaded exactly (`fild`); the u16 speed is loaded the same way (0x60AD8D) and r is added to
  it, so **r is in map units**: the falling log's wait in ArrivesAtWoodReaction pushes 0.5f (0x7647E6),
  MOVE_AROUND_FIRE (0x75A801), GO_TOWARDS_TELEPORT (0x766319) and MOVE_GAME_THING
  ([the map script reader](../scripts/map-script-reader.md)) push 0. Under the game's 24-bit FPU
  ([The game's FPU runs at 24 bits](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)) each square, their
  sum, R and R² round to a float; the compare is `fcompp` with `test ah, 0x41` (R² <= d² → 0). So a walker within
  the reach in exact integers can be out of it: speed 65535 at (65534, 362) gives 4294836200 < 4294836225, but both
  round to 4294836224.
- `Living::LookAtPos` 0x5EC550, one turning step per call: at most 0x40 / 0x80 / 0x100 (modes 0 / 1 / 2) of the
  2048-unit circle, or the mode value itself for any other mode (0x5EC57A; it takes two arguments, ret 8). The short
  way: d > 0 turns + if d < 0x400, else −; the result is & 0x7FF. It returns 1 and snaps to the target when |d| < the
  step. LookAtObject 0x5EC520 = LookAtPos(the object's position), 1 for a null object.
- `MobileWallHug::SetGameAngle` 0x60DA90 stores +0x5C and calls `Object::SetYAngle` 0x639260(ConvertGameAngleTo3D
  0x74DC50(a)). SetYAngle 0x60DAC0 (Villager vt +0x524) does the reverse with ConvertAngle3DToGame 0x74DC30.
  `Object::GetYAngle` 0x402500 (vt +0x508) reads +0x4C.
- `Villager::StorePreviousState` 0x763470 writes PREVIOUS raw (`LivingAction::SetState` 0x5ECC90, no town
  modifiers). A passing final state (one never kept in PREVIOUS, file 0x10, or a reactive one, 0xB8) keeps what
  PREVIOUS had.
- `Living::ExitNoChangeState` 0x768780 (vt +0x928): 1 when next is script-interruptable (file 0x1C), is IN_HAND, or
  has the same exit; else 0.
- `Villager::IsAvailable` 0x751D50 (vt +0x2C): 0 while being deleted (+0xA & 1) or when GetFinalState is 14 DYING;
  else 1. `Villager::IsFunctional` 0x751CF0 (vt +0xD4): IsAvailable and TOP not 13 / 14.
- `Villager::IsVillagerAvailable` 0x752290: not controlled by a script (+0x25 & 4), IsAvailableForStateChange
  (GameThingWithPos 0x401A30: not in the hand) and GetVillagerAvailableState & 1.
- `Living::IsDead` 0x417270 is also true when the villager is not functional (vt +0xD4, 0x417287), not only for
  status & 1 or TOP 15.
- `Living::IsDeathState` 0x5ECAA0 (vt +0x970): 13..17, unsigned.
- `SetTopState`: 0x7520B8
- `SetTopState`: 0x7520C4: CallEntryStateFunction(163) (vt +0x90C), its result unused
- `EnterTopState`: 0x5F28F1: CallExitStateFunction (vt +0x904) -> 0: 0x2E, nothing changed
- `EnterTopState`: 0x5F290D: CallEntryStateFunction (vt +0x90C) -> 0: 0x2F (the flags and the out-of clip stay touched: literal)
- `EnterTopState`: 0x5F291B SetStateSpeed, 0x5F2921..0x5F295C the clips
- `SetCurrentAndDestinationState`: 0x7520F0
- `SetCurrentAndDestinationState`: 0x752105: CallEntryStateFunction(163) (vt +0x90C)
- `ApplyCurrentAndDestinationState`: 0x5F29A5..0x5F29B1: CallEntryStateFunction(c, d) (vt +0x908)
- `ApplyCurrentAndDestinationState`: 0x5F29BB..: SetStateSpeed and the clips, the into function told d (0x5F29EB push ebx)
- `SetState`: 0x753695..0x7536BD: PREVIOUS never keeps a state with Infos +0x20 (file 0x10) set
- `CallExitStateFunction`: 0x752328..0x75233B: the raw TOP and GetFinalState, read first
- `CallExitStateFunction`: 0x75234E..0x75236E: `exit[top](next)` (slot +0x20, 0xD091B8), else 1
- `CallExitStateFunction`: 0x752370..0x75239E: and `exit[final](next)` when final != top
- `CallExitStateFunction`: 0x7523A0..0x7523A7: 1 only if both are 1
- `CallEntryStateFunction`: 0x7523D7: GetFinalState first
- `CallEntryStateFunction`: 0x7523F2..0x75240A: `entry[s](final, s)` (slot +0x10, 0xD091A8), else 1
- `CallEntryStateFunction`: 0x752411: SetState(0, s)
- `CallEntryStateFunction`: 0x752429..0x75242E: 0x23 -> 1 (the function set the states), else 0
- `CallEntryStateFunction`: 0x752447: GetFinalState before the first entry
- 0x75245A: CallEntryStateFunction(c) (vt +0x90C) -> 0: 0
- `CallEntryStateFunction`: 0x752475..0x75248D: `entry[d](final, d)`, else 1
- `CallEntryStateFunction`: 0x752494: SetState(1, d)
- `CallEntryStateFunction`: 0x7524AC: 0x23 -> 1; else 0 (TOP stays c, FINAL 0: the Villager wrapper's 0x2F then enters 163)
- `SetupWaitForCounter`: 0x76B06E: SetCurrentAndDestinationState(57 WAIT_FOR_COUNTER, final) (vt +0x8DC); anything but 1 -> 0 with the counter untouched (0x76B086)
- `SetupWaitForCounter`: 0x76B07E: +0x58 = the count (the Object u16 state counter)
- `GetGameAngle`: +0x5C (u16). ⚠️ approximate WallHug::yAngle in radians, rounded to 2048ths (LookAtPos 0x5EC58E's reading)
- `SetYAngle`: 0x60DACF..0x60DAD8: +0x5C = ConvertAngle3DToGame(a) (u16).
- Villager::SetCurrentAndDestinationState 0x7520E0 (vt +0x8DC): Living's, 0x2F -> CallEntryStateFunction(163)
- Living::SetCurrentAndDestinationState 0x5F2980: the exit (0x5F298B), the out-of clip (0x5F299C) and the into clip (0x5F29EB) all get the destination `d` (ebx = arg 2, 0x5F2981); only the entry gets both (c, d) (0x5F29A5..0x5F29B1). So: out = VillagerCallOutOfAnimation(e, d); ...; VillagerApplyStateClips(e, d, out), whose into function is the TOP's (= c after the entry) with (1, d).
- `ServedDesireAmount`: 0x08 (0x18): how much of it. AdjustTownModifier 0x753560 (0xDB9E80)
- `NotStoredAsPrevious`: 0x10 (0x20): never kept in PREVIOUS. Villager::SetState(2, s) 0x7536A2 (0xDB9E88)
- GetVillagerAvailableState 0x751F40
- `NoOutOfClip`: 0xF0 (0x100): no out-of clip when entering it. CallOutofAnimationFunction 0x75663E (0xDB9F68)
- fn_00436A80 -> UniqueKeyHeap::GetUniqueIdFromAddress 0x7E19A0: the heap key of the object.

### States 85 and 239

- **85 CREATED** (`Villager::VillagerCreated` 0x753DD0): `v = +0x58; +0x58 = v − 1; if v == 0 → +0x58 = 0 and
  SetTopState(163)`. It moves to 163 on call number counter + 1 (1..501 turns).
- **239 PAUSE_FOR_A_SECOND** (0x76B0B0): when the clip ends (IsReadyForNewAnimation(1)), SetTopStateToFinal 0x5ECA80
  = SetTopState(FINAL): the exit and the entry of `s` run again; it does not pause again (TOP == 239). While it lasts,
  GetFinalState = s.

### Addresses

The comments of openblack's code describe what it does in plain words; the original's addresses they used to quote
are kept here, with the openblack symbol each corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `MobileWallHug::SetYAngle vt+0x524 (0x639260), Object::GetYAngle vt+0x508 (+0x4C)` | the heading setter and getter used by THING_JC_SPECIAL | `SetYAngle / GetYAngle (super_villager)` |
| `+0x39A / +0x38A` | GVillagerInfo::drowningTime offset: +0x39A in memory, +0x38A in info.dat (600 turns for every villager) | `ecs::DrowningTime (VillagerDrowning.cpp)` |
| `0x76A7A0..0x76A7CB` | `Villager::Drowning`'s death call: VillagerDead(reason 6, player, 0.01f = 0x3C23D70A, 1), player = GetPlayerWhoLastDroppedMe (vt +0x6C)->GetPlayer (vt +0x1C), else lastPlayerToInteract (+0x104), else neutral | `ecs::VillagerDeadDrowned` |
| `dec word ptr [esi + 0x58]` | the state counter (Villager +0x58, u16) is decremented without a check, so it wraps when already 0 | `ecs::VillagerDrowningState` |

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `VillagerStates.cpp`, `Living.cpp`, `LivingStates.cpp`; the table read from
  `_$E32` in the exe with the xref index.

## openblack

**Done** for the fields, the table, the creation, the turn and the state changes. The special villager's roll, name
pick, age and draws are ported ([the special villager](#the-special-villager): `villager::RollSpecialVillager`,
`special_villager::PickName`, `MakeSpecialVillager`, the `NamedVillager` component and the land's names given in
`RegistryContext::namedVillagersUsed`; the pure pick, `NamedVillager` and the context field are raffclar's, from his
`f1c55c148`, wired into our constructor so that the draws stay the original's); its name over the head and its save
are not (its speech bubble is not in the original: see
[Not in the original](../project/not-in-the-original.md)). openblack has no mail link and no multiplayer game, so its table is always the info's 48
rows, as the original's is with no mail link. openblack differs: the predators' life writes skip `SetLife`
(`TargetPounce` and `ProcessDownedVillagers` in `src/ECS/AnimalPredators.cpp`), a fix row. The circle look takes the runtime's arccosine as the original's
`__CIacos` (0x616515, 0x61576D; 0x7C76E0) works it out (`wall_hug::ArcAngle`, see
[Walking round obstacles](wall-hugging.md#the-original)).

Code:

- `ECS/Villager/VillagerCore.{h,cpp}`: the turn (ProcessState, CheckEveryTime), the state changes (SetTopState,
  SetCurrentAndDestinationState, SetState, exits and entries), creation (`villager::Construct`), CREATED (85), the pause
  (239), SetupMoveToWithHug;
- `ECS/Villager/VillagerStateInfo.h`: names of the fields of the info.dat state table (`field0xdc`,
  `availableForReaction`);
- `ECS/Villager/VillagerStateTable.h`: the row of the function table (state, entry, exit, validate...);
- `ECS/Villager/VillagerStateFunctions.h`: `k_StateFunctionIds`, an id for each row's entry, exit and validate function
  in the original ([The original's function ids](#the-originals-function-ids)), with `k_ReactionValidate`;
- `ECS/Villager/VillagerDebugHooks.cpp`: the trace and the test hooks;
- `ECS/Systems/Implementations/LivingActionSystem.cpp`: the table (`k_VillagerStateTable`) and the turn loop;
- `ECS/Components/Villager.h` (the fields), `LivingAction.h` (the three states and the counters), `Town.h`
  (`TownDesire`), `ECS/VillagerAge.*` (the age rules), `ECS/VillagerNeeds.*` (the own needs' rules),
  `ECS/VillagerRoutine.*` (the day's rules: spots, idle choice, sleep, emergency), `ECS/Villager/VillagerAge.h` (age),
  `ECS/Archetypes/VillagerArchetype.cpp` (creation), `ECS/VillagerSpeed.*` (speed), `ECS/VillagerAnimation.*` (the clip
  rules), `VillagerAnimate.*` (a state's clip), `ECS/VillagerAnimations.*` (clips of the state changes),
  `Implementations/VillagerScript.*` (a villager in a script's hands) and `ECS/Villager/VillagerScript.*` (the walk to
  a point and the state helpers it uses);
- `ECS/Villager/VillagerHome.*`, `VillagerFood.*`, `VillagerAge.*`, `VillagerResources.*`,
  `ECS/Town/AbodeVillagers.*`, `ECS/Town/TownVillagers.*` (see
  [Home, food, sleep, homeless and age](villager-life.md#home-food-sleep-homeless-and-age));
- the other villager pages list their files in their openblack sections.
- Progress: [villager](../project/progress/) (what our tree does of it, row by row).

The villager's data lives in ECS components and its behaviour in systems reached through the Locator, with no globals;
the state functions stay free of global state, so they are unit tested with fakes. See
[the conventions](../project/refactor-conventions.md).

Field names and mirrors: `life` (`ecs::life`), `birthTurn` (`villager::GetAge`), `flags`, `food`, `lastCheckTurn`,
`foodSpeedUp`, `discipleType`, `resourceHeld`, `pregnancy`, `buildingSite`, `mother`, `targetThing`,
`buildPosIndex` (the +0x118 ring index kept apart from the union), `workPos` (the +0x11C work point; openblack keeps no
union), `abode` / `town`. `lifeStage` is a mirror of `flags & 0x8` (read by DetailMeshes, drawing and sounds) and
`sex` is a mirror of GVillagerInfo +0x1F8. `task` is not from the original. The map flag is `map_cells::IsObjectInMap`,
which `villager::IsObjectInMap` returns for the script's tests; before, it was "the top state is not IN_HAND", so a
thrown or flying villager was taken (test: `test_villager_core` `ScriptRefusesAVillagerOutOfTheMap`). The entry / exit
clip flags are `SkeletalAnimation::transitionFlags`. The worship 0x2 is `Villager::k_FlagAtWorshipSite`, set and
cleared by the worship code's AddVillagerToWorshipSite / RemoveVillagerFromWorshipSite as the original's (since
2026-10-29; before, it lived apart in `WorshipVillager::atSite`, which only some readers read). The disciple bits 0x200 / 0x400 are
`Villager::k_FlagDisciple` / `k_FlagDiscipleFollower`, set by `villager::SetVillagerDisciple`
(`Villager/VillagerDisciple.cpp`). The after-tap bit 0x1 is `Villager::woken`, and the bits 0x4 and 0x2000 are
`components::AtHome` (on the villager while it is inside, its `beenToBed` the 0x2000 of the stay): CheckWhenGoingToBed
is only asked while the villager is inside, and LeaveHome clears both together. The hand-marked home is
`villager::MarkHomeOfHeld` / `UnmarkHomeOfHeld`. The hand's refusal at home is applied in HandPlacement.cpp
(`villager::IsReachable`).

The table in openblack: an unported state (`k_TodoEntry`) does nothing (returns 0) and warns **once per state** with
the original's address; if the original has an entry, exit or validate in a row that openblack leaves empty, it warns
once (`TODO: Unimplemented entry function of ... (0x...): taken as 1`); the worship exits (58, 59, 60, 213) keep their
old convention (false = can exit) and the row adapts them; 16 DROWNING is `ECS/VillagerDrowning.*`. The rows with the
ExitReaction thunk that nobody enters (12, 25, 26, 140-168, 194-196, 205-208, 227, 231, 235-237) remain empty; the
unported rows 8, 9, 203 and 214 use `TodoWithExitReaction`. The rows of a creature's reaction (146-148, 155-157, 159,
160 and 167) are `villager_creature`'s with the shared ExitReaction, ReactionValidate through `VillagerCallValidate`,
and for 167 `villager_creature::MoveTowardsCreatureValidate` (the wall hug's own does nothing); save / load not ported. ExitReaction's `CircleHugInfo::Reset` removes the
`WallHugObjectReference` **⚠️ approximate**. StopReacting is `villager_reactions::StopReacting`, which calls
`villager_fire::StopReacting` (reaction 0, null object, RefreshRecord of REACT_TO_FIRE),
`villager_teleport::StopReacting` (clears its state, RefreshRecord of REACT_TO_TELEPORT), `villager_shield::StopReacting`
and `villager_mourning::StopReacting`; `villager_reactions::IsReacting` is true for any of the four; the record's turn
is `reactions::RefreshRecord`. RemoveFromDance for 203 is not ported (203 has no state function).
`villager::IsStateExitFunctionSameAs` compares the ids of `VillagerStateFunctions.h`; ExitPutOutFire uses it instead of
the hand-written list it had. `Dead`'s SetHumanShadowed(0) is ported: Dead clears `Villager::humanShadowed`
(`Villager/VillagerDeath.cpp`) and the ground blobs' draw leaves out a villager that is not shadowed
(`Renderer.cpp`), so a dead villager casts none (test `VillagerDeathTest.Dead`).

The creation in openblack: `villager::Construct` draws the living part's age first (`living::ConstructorAgeDraw`,
shared with the animals and the creatures) and calls SetScaleForAge last; ChildBorn gives a skeleton mother's child the
skeleton look (`SetSkeletonLook`) without a second draw. The state counter is `turnsUntilStateChange`; the water test
is `pot_resource::IsWater` (with the water merge, `sea_cells::IsWater`). The archetype sets the home and the town and
the speed of 85 **⚠️ approximate**. The intro family is made as in the original. `CREATE_TOWN_VILLAGER` with an unknown
villager is refused with a warning (`k_NoVillagerInfo`).

The turn in openblack: `living_turn::ProcessLiving` (ECS/LivingTurn) runs the villagers' turn: first openblack's test
hooks, then the living list, following the creation counter, highest first. `LivingList` stands for the original's
list (openblack keeps none): the available villagers and animals by their creation index, highest first, one without
an index at the tail (a stable sort); the creatures, which the original's list also holds, are not on it yet
(pending). `WalkList` is ProcessLiving's loop: the next one is taken before each turn, and one unlinked during the
current turn still takes its turn if it is still in the registry, then the loop ends. `MoveToStep` is
`MobileWallHug::MoveTo` 0x60AF20 for a villager (the PathfindingSystem's step; nothing without the system, as in the
unit tests). In the table, `VillagerMoveToPos` is state 1's `Living::MoveToPos` 0x5EC270: no walk while the local hand
holds the villager (⚠️ approximate: only the local hand is asked), else the step and, only on 0xA,
SetTopStateToFinal; `TodoWalk` is a row not ported whose state function walks: the step, then the TODO
(⚠️ approximate: the rest of the function and the use of the step's result are not ported). MOVE_ON_PATH 29 is no
longer one of them: its row is `MoveOnPathRow`, which walks on to the next node ([Routes](routes.md#openblack)). LANDED's disciple branch tests `Villager::k_FlagInHand` (still "in the hand"
after a gentle put-down, 0x7606EA..0x7606FF) and the disciple bit. The teleport code's `FinalState` is
`Villager::GetFinalState`. `LivingActionSystem::Update` runs only what
follows: the souls (once a turn, see [Death](villager-hand-and-death.md#death)). The reaction validate is
`villager_reactions::ReactionValidate`, called from `LivingActionSystem::VillagerCallValidate`, which also gives the
rows whose original validate is WallhugValidate (1, 47, 71, 154; `villager::k_WallHugValidate`) the empty one: they
pass and warn of nothing ✅; the custom exits
🔍 inferred that did its work are gone; `GoToTeleportReaction` only returns 0 if it does not store a stone, instead of
reading a null. Script control is `ecs::script_held`.

The walk's arrival in openblack (`living_turn::WallHugMoveToResult`, ECS/LivingTurn, which MOVE_TO_POS, MOVE_ON_PATH
and 167 MOVE_TOWARDS_CREATURE_REACTION read): it arrives when it has the FinalStep (or Arrived) mark and is
already at its destination (PathfindingSystem sets it with AreWeThere and places it at the destination on the next
turn). Previously a walk that the PathfindingSystem abandoned (without marks) counted as an arrival wherever it was:
that is how the teleport villagers went round in circles 1 ⇄ 201 without ever reaching the stone. The unported cases
of the PathfindingSystem (destination inside the circle being hugged, TODO #864, and the step from one circle to
another, TODO #865): `AbandonMove` no longer drops the walk, it stays in STEP_THROUGH **⚠️ approximate** and the
villager arrives through AreWeThere (a walk round a circle that holds none is no longer one of them: it heads straight
on as the original's, see [Walking round obstacles](wall-hugging.md#openblack)); the PathfindingSystem's ARRIVED was inverted and is fixed. The invented idle walk
(radius 40, FINAL 209) **no longer exists**; only the debugging tools ("Move To Point") prepare walks with FINAL 0,
which on arrival return to 163 via the compatibility path (the original would do SetTopState(0)). The clips of a state
change are `VillagerCallOutOfAnimation`, `VillagerApplyStateClips`. AreWeThere is `walk_arrival::AreWeThere`
(`src/ECS/WalkArrival.*`) on whole units, the one copy behind `villager::AreWeThere`: both positions are made MapCoords
from the metres openblack keeps (`map_coords::FromMetres`) and the speed is `WholeSpeed`. MOVE_AROUND_FIRE and
GO_TOWARDS_TELEPORT call it too (their float copies in metres are gone), and the falling log's reach is half a unit,
not half a metre. **⚠️ approximate** only through the metres: a position that went through a MapCoords may come back one
unit short (`map_coords::Quantise`). Tests: `test_walk_arrival` (`AreWeThere*`), `test_villager_core`
`ScriptAreWeThereIsStrict`. `VillagerMove.h` forwards SetupMoveToWithHug to VillagerCore.

Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

#### Its own info row

✅ A villager keeps the row of the villager tables it is made with, as the original keeps its info pointer
([Fields](#fields), +0x28): `components::Villager::infoRow`. `VillagerArchetype::Create` sets it from the row it is
given, so every way of making one sets it: the land script's CREATE_VILLAGER / _POS / CREATE_TOWN_VILLAGER /
CREATE_SPECIAL_TOWN_VILLAGER, the challenge script's CREATE (villager and child) and CREATE_RANDOM_VILLAGER_OF_TRIBE,
the town's births (`VillagerChildFactory`), the vortex's arrivals, the sea test hook, the testbed and the editor. A
child that grows up keeps it. `villager::ChangeInfo` (a villager taking another tribe's looks) sets the new row.
`VillagerInfoOf` (and with it `villager::InfoOf`), the physics' and the fire's object info
(`DynamicsSystem::ObjectInfo`, so the help texts and the sacrifice value too, and `BodyMassOf`'s weight) and the
editor's copy of a villager read it through the pure `VillagerInfoIn`. Before, they looked the row up by tribe and job,
which gave every special villager the piper's row: the piper's food type to the predators, its sex to the priestess and
the trainer, its KeepMeshWhenChangeTown to the footballers, its meshes when the age crossed the grown-up age (growing
up, or a script setting the age) and to the soul of a dead one, and row 63 to the script's sub-type and the vortex's
saved line. An ordinary villager's row is the only one of its tribe and job, so it reads what it read before.
(openblack) a villager made without a row (only the tests' made-up ones) still finds the first row of its tribe and job.
Tests: `test_villager_speed` `VillagerInfoRow.*` (made-up tables where the piper, the hermit and the trainer share a
tribe and job), `test_villager_home` `VillagerHomeTest.ChangeInfoKeepsTheNewRow`.

#### Compatibility path

`LivingActionSystem::VillagerSetState`:

| call | what it does |
|---|---|
| TOP, `skipTransition = false` (worship; the pick-up and the start of a flight through `ecs::SetVillagerStateWithExit`, see [Picked up or thrown](villager-hand-and-death.md#picked-up-or-thrown)) | exact `villager::SetTopState` |
| TOP, `skipTransition = true` (the physics' landing, animals, the water, the arrival of a walk with FINAL 0, LANDED, Gui) | if it is the same state, nothing; otherwise, exact `villager::SetState(0, s)` + clips and speed. **⚠️ approximate**: the original goes through SetTopState, not ported |
| FINAL / PREVIOUS | exact `villager::SetState(i, s)` |

### Tests

`test/test_villager_core.cpp` (fake table in the Locator and scripted rolls with `game_random::testing::SetGameRand`):
food with one and two rolls, lastCheckTurn, counter and water rule, random order of the constructor, 85, 0x2E
by TOP and by FINAL, 0x2F (→ 163), 0x23, setting TOP clears FINAL with the town, the PREVIOUS rule,
AdjustTownModifier, the pause (with and without poison, without a roll in 239 or without the mark), 239 → FINAL, check
every 9 turns, wear, EXHAUSTION / CHANT (with the at-site flag), hurt → 36 (switched off in one case; 19
with food, knocked down), SetupMoveToWithHug with `moveState` keeps FINAL (and 0x2F without a walk), POWER.

`test/test_villager_food.cpp`: the hunger batch (0.79919), the strict damage, the interruptions (0xD0 / 0xD4, the
disciple, STARVING / CHANT), the reference amounts (74, 65, 63, 54, 83, 85), ChangeStateToFindFoodToEat (117 / 118 /
36 / 33 / what it carries / 0), EatFoodHeld (1.0 and 0.9864865, NaN → 0), the double take of GetFoodFromHome, 117 /
118 / 212 and 34. `test/test_villager_home.cpp`: 37, ExitAtHome with PresentAtHome, HomeDecideWhatToDo (0.7875,
GameRand(4)), pregnancy and child, sleeping (6 cycles from 0.4 to 0.70000005), CheckWhenGoingToBed once per stay, the
tent (tree, the other side, full → (−20 m, +10 m)), DoGoingHome without a home, the score and
FindAbodeWithSpaceInTown, the home's list, CheckNeedNewAbode → 129 → 36, 130, 238, 234, the Shuffle and
Abode::Process (1001 turns). `test/test_villager_age.cpp`: the pure layer (63 years, r³), SetScaleForAge with scripted
GameFloatRand, the 13-year-old child (18 years, counts, 234), old age and UpdatePregnancy (the original's WomanSpecial). In
`test_villager_decide.cpp` a hungry villager eats immediately (117) and one without a home or town goes to 130.
`test/test_villager_death.cpp`: the help table, the dying time and clips, DeadTick, the soul's clip / alpha / expiry,
the death alignment and the mourning turns; VillagerDead's guards, fields and order, the owner / killer / town
counters / pulse, the help sprites, the world population, Dying, Dead (smoke, skeleton, vanish, script-controlled,
SACRIFICE), CannotExitState, DeleteDependants, the orphans, the mourning priority, setup and states 205-208 (no
landscape in the tests: everything is water there, so no soul).

`test/test_villager_build.cpp` (the fixture of test_villager_resources plus a mock building side): the build factor
and amount at 24-bit float steps, the cycles (29 / 35 / 130 / 82), the ring index, the wood source scores, 41 (the
call order value → RemoveResource → BuildBy → stats, the next ring point, the pile cap, the release without
RemoveBuilder), 40 (the walk, the index range, the drop of all the wood, the kept counter), GotoBuildingSite, 184,
GotoStoragePitForBuildingMaterials (+0xFC only from a non-building TOP), 39, EnterBuilding / ExitBuilding, the four
CheckSatisfy functions (+0x5E4 before the request), SetupBuildingObject(abode) and Repair_Town's plans.

`test/test_villager_farming.cpp` (mock field / fish-farm sides): the field and fish-farm scores (GDM 0.5 at half the
limit), the catch by season / capacity / tribal power, FindBestField's strict order, CheckSatisfyFoodDesire (the field
when the farm has a fisherman, a tie keeps k's order, the drop-off, the flock TODO), SetFarmerGotoField (the point
after the walk), 67 / 68 / 69 (the digging for 0, the unsigned cost at 170 held), Enter/ExitFarming, 55, the trip 37 /
74 / 111 / 148 / 150 → 31, Enter/ExitFishing. `test/test_villager_reactions.cpp`: the pure scores, the standard turns
(3000 / 2222), the same-type flags, SetReactionDoneWhen, IsAvailableForReaction(type), a wood reaction's life (score
78, the setup, the same-type refusal, 21's walk, ProcessReaction's end, ShutDown, the initiator gone), the wood gates
(124 / 125 held, life 0.3), a miracle reaction type with no slot, 20 / 21 / 22. `test/test_villager_forester.cpp`:
FindTreeNearVillager's code, IsTouching, the raw 47, GotWoodDecideWhatToDo, 52, 50 after the fall, 53 without a town.

`test/test_villager_repair.cpp` (the fixture of test_villager_home plus a mock building side): the +0x50 column (206 /
1 / 48, spot rows), ReactsToTownEmergency by GetFinalState (the walk home FINAL 37 yes, at home TOP 38 no, FINAL 40
yes, 220 by PREVIOUS), CallToTownEmergency (a builder: its exit told 242 is the real ExitBuilding, RemoveBuilder and no
site; no draw; at home unchanged), the tap (not inside: nothing; inside: the two draws first, FINAL 197, PREVIOUS 163,
flag 1, out of the home; DYING: nothing), 197, 242 (no town; the 2 pi draw, the ring point at d = 20 for 20 people),
CongregationDistance (10 / 15 / 20 / 29.5 / 30), 243 (GameRand(12) / GameRand(5) branches, no town),
CheckSatisfyToRepair (a mock GetBestRepairBuildingSite with the +0x638 and desire > 0 filter: the ordinary site of a
rock-damaged house and a +0x638 site of desire 0 are not taken; the real filter is the building side's, tested there),
ArrivesHome's repair branch (repairs; site full → in; hungry, hurt, full life → in) and the repair cycles (the real
life::IncreaseLife).

`test/test_living_landing.cpp`: OriginalRows (the drawn pi / 2), the villager's three poses and their yaws (reference
values: flat 0, on the right / left side, the strict thresholds and NaN), the animal's reversed landType and heading,
YawFromRotation, VillagerLandedClip, villager::SetYAngle (+0x4C drawn, GameAngle),
InterfaceSetInMagicHand's +0xE0 0x20, ObjectYAngle (exact from the game angle, else the GetYXZ yaw),
EndPhysicsWithoutBody (landType 3, nothing for a non-Living).

**Runs checked.** Land 1, 58 villagers, all 85 → 163 with code 1, checks every 9 turns, wear 2e-6 per turn when
walking; with `LIFE=0.2` and `STATE=246`, 14 of 55 pause (239 → 246; ~27 % was expected) and on the next check they go
to 36; with `LIFE=0`, `died (EXHAUSTION)` without a hang; `OPENBLACK_TEST_THROW_VILLAGER`: 85 → 10 → 11 → 163. Land 2,
`OPENBLACK_TEST_WORSHIP="1,0.5"`: they reach 59 and 60; three villagers are born in the sea in 16. After the audit:
normal Land 1, 338 turns, no changes. Land 2, `WORSHIP="1,0.5"` and `LIFE=0.25`, 2452 turns: 25 direct exits 60 → 248
and 8 after a pause (239 → 248), spread among 21 villagers until the end (before, from turn 2333 only one came out).
Land 2, `OPENBLACK_TEST_HUNT_VILLAGER="0,3"` (turn 3, before the script creates the sea villagers, so the first one in
the registry is on land): 85 → 17 DOWNED → 18 BEING_EATEN → eaten.

Fire and teleport through the core: Land 1, `OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 650 turns: the same
states as before (85/1 → 215 → 220 ⇄ 216, 163/1 → 219) and one entry and one exit per change (3213 `EnterPutOutFire(216,
220) = 1` for 3213 changes 216 → 220, 3221 for 220 → 216, 11 `(215, 220) = 1`), plus those of replanning and arriving
(22 `ExitPutOutFire(220, 220)` / `EnterPutOutFire(220, 220)`, 6 `ExitOnFire(219, 219)` / `EnterOnFire(219, 219)`), no
0x2E nor 0x2F; before, those last ones were missing and ExitOnFire was never called. The 216 ⇄ 220 back-and-forth every
turn was already there before. Teleport (`OPENBLACK_TEST_TELEPORT="1785,2655,1830,2660,7,walk"`, `_TURN=300`): 1 ⇄ 201
as before and, in one of the passes, 201 → 202 → jump (saving 26 m) → `SetTopState 202 → 163 = 0x1`.

A second review: teleport as above, 1365 turns: the 6 villagers that react make **one** walk to the stone (1 with FINAL
201, about 70 turns), arrive (1 → 201 → 202), jump (saving 44-45 m) and `PopFromPrevious stored 209 -> resume 163`; the
back-and-forth 1 ⇄ 201 is over. The villagers keep walking and arriving (208 walks finished, median 170 turns; none in
MOVE_TO_POS without moving between two summaries), 18 unported cases of the PathfindingSystem remain in STEP_THROUGH.
Fire (`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,20"`, 747 turns): the same states (85 → 215 → 220 ⇄ 216 → 163, 163 →
219), `ExitReaction` on each exit of 215..220 and 12 `StopReacting` when it goes out; no `pause 239` nor `Stuck in an
invalid state`. `OPENBLACK_TEST_MAP_CYCLE` over Land1-5 without hangs.

With `ReactionValidate` connected (with a temporary trace in `VillagerCallValidate` that does not stay in the code):
fire (`OPENBLACK_TEST_FIRE="1785.2,2652.6,450,abode,60"`, 785 turns) 12 villagers 85/1/114 → 215 → 220 ⇄ 216 and back
to 163 with `StopReacting` when it goes out; `ReactionValidate` runs every turn for the TOP (1080 times in 216, 54 in
220, 13 in 215) and for the FINAL (410 in 220, 3 in 215), without any pop (the object, the home, is still available;
when it goes out `ExitPutOutFire` → 163 does `StopReacting` before a reaction state without an object is validated).
Teleport (`OPENBLACK_TEST_TELEPORT="1715,2595,1760,2640,7,walk"`, `_TURN=550`): 1 → 201 → (20 turns walking, FINAL 201
validated every turn) → 202 → jump of 63.6 m → 163 and `PopFromPrevious stored 245 -> resume 163`. **Beware of the
`walk` hook**: its walk with FINAL 163 does not admit reactions (`IsAvailableForReaction` 0x763390: the +0xEC of 163 is
0), so the hook's villager does not react; the test was done with FINAL 245 changed by hand in
`TeleportDebugHooks.cpp` (not saved). `OPENBLACK_TEST_MAP_CYCLE` over Land1-5, Greek God, TwoGods and Kapa's Land1
without hangs.

`test/test_living_turn.cpp`, `LivingTurnTest.ThePlainWalksValidateChecksNothing`: rows 1, 47, 71 and 154, as TOP and as
FINAL, pass the validate, warn of nothing and keep their state, counter, walk mark and hugged circle.
`LivingTurnTest.LeavingAPlainWalkLetsGoOfTheCircle`: rows 1, 2 and 222 let the villager go into any state (1), drop
the circle it held (only its own), keep the walk's move mark and the state, and warn of nothing.
A walk left keeps its move state and goal, as the original's (`Villager::SetTopState` and `GScript::SetScriptState`
0x6F82E0 touch no walk: StorePreviousState vt +0x8EC, the exit vt +0x904, the entry vt +0x90C, SetAnim(1) vt +0x8FC,
+0x58 = 0): `villager_reactions::SetTopState` and `villager_script::SetScriptState` no longer take the move marks off
(`VillagerCoreTest.AReactionsStateChangeKeepsTheWalk`, `ScriptSetScriptStateKeepsTheWalk`).

### Assumptions

<a id="assumptions-inferred--approximate"></a>

1. "Dancing" (Living +0xD8, the DanceGroup) is being in a dance's group (`dances::DanceOf`, VillagerSpeed.cpp), as
   the original, for the scripts' dancers and the worship sites' dancers alike ([Dances](dances.md)).
2. GRand: `villager::GameRand/GameFloatRand` forward to `game_random` (LHRand over the synchronised seed,
   [Random numbers](../engine/random.md)). **⚠️ approximate** the sequence is not that of an original game: other
   systems that draw from the same stream are not all called in the original's order.
3. **⚠️ approximate** The archetype sets the speed of 85 CREATED when creating the villager; in the original the first
   SetTopState sets it. It is not visible: CREATED does not walk.
4. **⚠️ approximate** UniqueId (`UniqueKeyHeap::GetUniqueIdFromAddress` 0x7E19A0) is the creation index
   (`object_index`, +0x3C): it decides in which periodic check old age is looked at, and the SetSpeed factor.
5. **🔍 inferred** +0x11C: the type of the union comes from bw1-decomp; it is not ported until a job reads it.
6. The game turn is `game_clock::Turn` (g_game +0x205A40); in tests it is 0 or the one `game_clock::SetTurn` sets.
7. **⚠️ approximate** Turn order: villagers and animals in two passes, everyone's movement before the logic, and the
   villagers in registry order (not that of the list g_game +0x205BBC).
8. VillagerDead keeps the villager as a corpse (SetDying, 14, 15) and CallState runs after a death; the player of
   VillagerDead is GetPlayer (the town's owner).
9. **⚠️ approximate** The TOP changes of the animals, LANDED and the Gui do not go through exits or entries. The hand's
   pick-up and a flight's start go through the old state's exit (a reaction ends), and a straight change out of
   IN_HAND runs IN_HAND's exit; IN_HAND's and FLYING's own entries and FLYING's exit are not ported (queued fix).
   Those of fire and teleport do (core).
10. A walk with FINAL 0 (only debugging tools) returns to 163 via the compatibility path (the original would do
    SetTopState(0)). With a FINAL other than 0 the arrival is the exact one (SetTopStateToFinal, 0x5EC28E).
11. **⚠️ approximate** The exit of MOVE_TO_POS, MOVE_TO_OBJECT and FOOTBALL_MOVE_TO_BALL (ExitMoveToPos 0x5EDDA0) is
    `VillagerExitMoveToPos` (LivingActionSystem.cpp): it removes the `WallHugObjectReference` and returns 1, as
    ExitReaction's reset does. openblack keeps none of the circle hug's lookahead sets or extended entry distances,
    and its villagers keep no wall hug target (+0x60; their walk to an object, row 2, is not ported), so there is
    nothing else to clear.
12. **⚠️ approximate** AdjustTownModifier ignores a desire outside 0..16 (info.dat has none).
13. **⚠️ approximate** A disciple type outside the g_DiscipleInfos table (13 rows) does not ignore needs.
14. **⚠️ approximate** Without clip resources (the tests), IsReadyForNewAnimation gives "finished".
15. Bit 0x2 of +0xE0 (at the worship site) is `flags`' own bit, written only where the original writes it (since
    2026-10-29).
16. The "hurt → 36 GO_HOME" rule of CheckEveryTime (0x7505C3) is switched on: 36 walks to the door, and 37
    ARRIVES_HOME is ported.
17. Neutral (they do not invent behaviour): SpecialVillager. ProcessReaction, the Town +0x5E8 pulse, DROWNING 16, the
    world population and the skeleton are in, with the constructor's skeleton setting. CheckHungry,
    CheckChildGrownUp, WomanSpecial (`UpdatePregnancy` in openblack) and CheckDeathFromOldAge are in.
18. Neutral (does nothing, with TODO and address): RemoveFromDance. Now in: DiscipleDecideWhatToDo 0x751720 (every job
    case, `Villager/VillagerDisciple.cpp`), IsMotherAlive 0x757F40 and ChildGotoCreche 0x7579F0
    (`Villager/VillagerChild.cpp`), CheckHomelessMoveIntoAbode,
    ChangeStateToFindFoodToEat, CheckWhenGoingToBed, CheckNeedNewAbode, the homeless branch of DoGoingHome (tent 238 /
    130), ExitAtHome 0x761B40, and `TownDesire::CheckVillagerNeededForTownDesire` 0x745FF0 (leaves 0 or 1 in eax,
    0x7460EB / 0x7460F7); Town +0xF1C is written by SetInStateOfEmergency.
19. The door: see [The house and its inhabitants](villager-life.md#the-house-and-its-inhabitants). The abode's matrix
    is built once from its map position, y angle and scale; openblack keeps the angle on the abode (`Abode::yAngle`)
    because the Transform's rotation floats give it back only to their rounding (and in (−π, π]).
20. **🔍 inferred** Abode IsAvailable (+0xA & 1) = the entity is valid; the home's GAbodeInfo is that of its number and
    mesh. IsBuilt reads the abode's own build state (`abodes::IsBuilt`, Abodes.cpp), as `Abode::IsBuilt` 0x4016C0
    does: not built while +0x58 & 2 (under construction), else GetPercentBuilt (vt +0x880) >= 1.0. For a NaN percent
    ours says built and the original says not built; no writer produces a NaN.
21. CheckForClearArea's radius is the object's Get2DRadius, the same as the original's (`Object::Get2DRadius`
    0x638180, in Abode's vtable at +0x64): GetScale (vt +0x120) × the larger of the mesh's half-extents in x and z
    (LH3DMesh +0x24 / +0x2C; z wins a tie, 0x6381B4..0x6381BF). The mesh box is made at load by
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 from every vertex of every primitive of every submesh (0x87FB20 →
    0x807F30), half of max − min (0.5 at 0x8AA3B4), as openblack's (`object::Get2DRadius`, the union of the submesh
    vertex boxes). The cells hold the same objects (a MultiMapFixed in every cell it covers, map_cells), and the answer
    is yes / no, so the order of the cell's lists does not matter. Villagers sit as far from the door as in the
    original: the radius and the walk are the same.
22. **⚠️ approximate** The town's list of homes (+0x754) goes from newest to oldest (AddStructureToTown
    0x7399C3..0x7399CF inserts at the head); openblack sorts it by creation index (+0x3C) from highest to lowest, as if
    each home entered its town when created. It decides the height of GetCongregationPos (that of the last one read:
    the oldest), the fallback base with one home and which homes remain in the ring with more than 100. The plans
    (+0x9A8) go in order of arrival (AddPlanned 0x73D08A..0x73D0AD appends at the end), like `plannedAbodes`.
23. SetupMoveToOnFootpath 0x5EDD20 takes the object's footpaths since #74 W3: the choice
    (`footpath_walk::ChooseWalkToObject`), then the link's (`footpaths::UseFootpathIfNecessary`), which enters
    MOVE_ON_PATH 29 through SetupMoveOnFootpath. The callers it has in the original and openblack does not port yet
    still walk directly; see [Routes and footpaths](routes.md#routes-and-footpaths).
24. **⚠️ approximate** LookAtPos 0x5EC550: the angle Living +0x5C is `WallHug::yAngle` (radians, rounded to 2048ths).
25. **⚠️ approximate** 114: the "available" mother (vt +0x2C) = valid entity; IsNavigable (Collide & 2 and not & 8):
    openblack's Collide only knows land / water (bit 8 is never set).
26. **⚠️ approximate** SET_TOWN_CONGREGATION_POS: openblack's script reader (`PositionParameter` in Script.cpp) takes
    x and z from the string and the terrain height as y: the cache height is 0, the literal behaviour for two fields. A
    three-field string reaches the command with its x and z, but its third field (the height, which `MapCoords::Set`
    0x6032E4 reads unscaled) is dropped. None of the game's scripts uses three fields.
27. **⚠️ approximate** The build factor and amount are computed in float steps (the x87 runs at 24 bits in the game
    logic, GameDistance.h): alignment −0.1 gives u = 51, −0.3 gives 52, and the housewives' u at −0.25 / −0.75 is 42 /
    46 (the earlier 50 / 53 / 41 / 45 came from an extended product). Not verified in the game.
28. **(openblack)** `buildPosIndex` is its own member instead of the +0x118 union; SetupBuildingObject(abode) tries
    AddBuildingSite once (the original loops); null-town guards where the original would read null.
29. DecideHowToGetWood's forest results walk to the forest / BigForest (SetupGetBuildingSupplies 0x75876F /
    0x75878B). The town's forest list (filled at map load as `Town::AsssignTownFeature` 0x73EAC0 does) gives the
    forest first, else the global FindForest.
30. FPU at 24 bits (fn_007DEE00, `and cw, 0xFCFF` at 0x7DEE0D): the x87 intermediates of the villager code are float
    precision even on the x87 stack; the double models are superseded: LoadFactors / TownNeedsSum / TownNeedsFactor /
    DropOffScore / DropOffFraction / DroppedLogValue / the speed product, PointTurns / MournTurns / SoulAlpha and the
    desire chains of TownDesire.cpp are float steps now.
31. **(not ported)** The rows 51 FORESTER_CHOPS_TREE_FOR_BUILDING, 54 ARRIVES_AT_BIG_FOREST_FOR_BUILDING, 185
    ARRIVE_AT_PUSH_OBJECT, 188 / 189 (wood from a tree / pot for building) and 232 WAIT_FOR_WOOD stay `k_TodoEntry`:
    none is entered in the game (no `push` of those states before a state change; CheckForClearArea finds no pushable
    object; SetupWaitForWood needs g_game +0x14 & 0x40000, never set).
32. **⚠️ approximate** `workPos` (+0x11C) is its own member instead of the union with the script's clip / loops. The
    field / fish-farm lists of the town (+0x780 / +0x788) are the building side's component walks, newest first.
    Fishing's "on the arrive point" compares x / z of the villager with the arrive point through the metres its
    walk's goal went through (openblack keeps no walking altitude).
33. **🔍 inferred** "+0x24 & 0x40" (in the physics) = `PhysicsObjects::IsFlying` (in the physics and not a resting
    proxy: a resting proxy is in the list without the bit; confirmed by the physics code); "+0x24 & 4" = in the hand
    (`fire::traits::InHand`); a pot's IsAPotFromABuildingSite (+0x74 bit 3) = a building site's pile
    (`building_sites::SiteOfPile`); the reaction a miracle reaction map follows = the one of its type its object
    started (their maps keep no public id); `PhysicsObject::GetPlayer` = the local player when the hand threw it.

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Villager/VillagerCore.cpp`: `IsHungry`, `GetGameTurnsSinceLastChecked`, `SetGameTurnLastChecked`, `CheckEveryTime`, `IsAvailableForReaction`, `SetTopState`, `EnterTopState`, `SetCurrentAndDestinationState`, `ApplyCurrentAndDestinationState`, `SetState`, `CallExitStateFunction`, `CallEntryStateFunction`, `SetupWaitForCounter`, `GetGameAngle`, `TownField0x5E8`, `UniqueId`, `DeathReason::Exhaustion`, `DiscipleIgnoresNeeds`, `Villager::k_FlagFootball`, `Reaction::ReactToFood`, `Index::Previous`, `Index::Final`, `VillagerStates::WaitForCounter`.
- `src/ECS/Villager/VillagerCore.cpp`, `TownField0x5E8`: also read by Town::Process and DiscipleNothingToDo 0x7540D8)
- `src/ECS/Villager/VillagerCore.cpp`, `UniqueId`: ⚠️ approximate openblack has no UniqueKeyHeap: the creation index (Object +0x3C) stands for it
- `src/ECS/Villager/VillagerCore.cpp`, `DiscipleIgnoresNeeds`: Types past the table: (guard) record 0, no
- `src/ECS/Villager/VillagerCore.cpp`, `RollSpecialVillager`: the roll, then `special_villager::PickName` on the info's
  48 rows and the land's use counts; `MakeSpecialVillager` after `Construct` (called by `VillagerArchetype::Create`):
  the name marked given, the `NamedVillager` component, SetAge with the row's age (1 kept) and its two scale steps. The
  debug villager labels (`Gui::ShowVillagerNames`) show a special villager's name
- `src/ECS/Villager/VillagerCore.cpp`, `CheckEveryTime`: fcomp, test ah, 1), not at home (flags & 4), the state goes home when hurt (+0xF8 memory), not downed (Living::status +0xB4 & 0x80: components::DownedVillager), the state does not forbid it (+0xE8 memory), and not reacting to food (rows 19 / 20: 0xDBB2E4 / 0xDBB3F8) unless food > hungryForFood (test ah, 0x41)
- `src/ECS/Villager/VillagerCore.cpp`, `CheckEveryTime`: openblack's towns have no +0x5E8 (it is 0), so this never fires yet
- `src/ECS/Villager/VillagerCore.cpp`, `IsAvailableForReaction`: 0x5F121E: IsBeingUsedInTheDanceEditor 0x5ECC20 (IsDancing and a DanceEditState): , never in openblack 0x5F1227..0x5F1248: the final state 15, 16, 17, 18, 14, 23 or 5 -> 0
- `src/ECS/Villager/VillagerCore.cpp`, `SetYAngle`: ⚠️ approximate openblack keeps +0x5C as WallHug::yAngle in radians (GetGameAngle rounds it back): the game angle's ConvertGameAngleTo3D, so GetGameAngle returns it exactly
- `src/ECS/Villager/VillagerCore.h`: `IsChild`, `IsHungry`, `GetGameTurnsSinceLastChecked`, `SetCurrentAndDestinationState`, `ApplyCurrentAndDestinationState`, `RollSpecialVillager`, `SetTargetThing`.
- `src/ECS/Villager/VillagerCore.h`, `RollSpecialVillager`: the name's row, or none (a normal villager);
- `src/ECS/Villager/VillagerCore.h`, `SetTargetThing`: entt::null clears it.
- `src/ECS/Villager/VillagerDebugHooks.cpp`, `RunDebugHooks`: OPENBLACK_TEST_TOWN_EMERGENCY="<town>" : the buildings code' town_emergency::SetInStateOfEmergency 0x7479A0 on the town with that id (CallAllVillagersToTownEmergency -> villager::CallToTownEmergency, housed villagers)
- `src/ECS/Villager/VillagerStateFunctions.h`: `TownEmergencyReaction`.
- `src/ECS/Villager/VillagerStateFunctions.h`, `TownEmergencyReaction`: Generated from vst_fn.txt's f50 column, so the answer does not depend on which rows openblack has ported (k_VillagerStateTable's field0x50 is not used)
- `src/ECS/Villager/VillagerStateInfo.h`: `ServedDesireAmount`, `NotStoredAsPrevious`, `IsMoving`, `NoOutOfClip`, `LifeDrainPerTurn`, `AvailableState`.
- `src/ECS/Villager/VillagerStateInfo.h`, `AvailableState`: 0xA8 (0xB8): 1 / 3 / 4 (IsVillagerAvailable tests & 1).
- `src/ECS/Components/Villager.h`: `deathReason`, `DeathReason::None`.

### Test hooks

- `OPENBLACK_VILLAGER_TRACE=1` (or `=<n>`, creation index): `Villager trace:` lines with the creation (position, age,
  food, lastCheckTurn, counter, 85 or 16), each `SetState`, `SetTopState a → b = code`,
  `SetCurrentAndDestinationState`, `pause 239 → s (rand, threshold)`, `AdjustTownModifier`, each periodic check, a
  summary every 100 turns and each call to the fire entries and exits (`EnterPutOutFire(final, s) = r`,
  `ExitPutOutFire(final, s) = 1`, `EnterOnFire`, `ExitOnFire`), `ExitReaction(s) reactive r`, `ExitReactToTeleport(s)
  same r`, `StopReacting`, `PopFromPrevious stored a -> resume b = code`; the summary carries the position, the
  destination and the walk mark (L / O / E / S / F / A / -).
- `OPENBLACK_TEST_VILLAGER_LIFE="<life>[,<n>]"`: on turn 2 sets the life of all villagers or of villager n.
- `OPENBLACK_TEST_VILLAGER_STATE="<state>[,<n>]"`: on turn 2 calls `villager::SetTopState` and writes the code.
- `OPENBLACK_TEST_VILLAGER_BORN_IN_WATER="x,z"`: on turn 2 creates a Celtic villager (Housewife, 25 years old) there.
- `OPENBLACK_TEST_VILLAGER_POISONED=<n>`: on turn 2 poisons villager n.
- `OPENBLACK_TEST_ABODE_LIFE="<index>,<life>[,<pit>]"`: on turn 2 the index-th abode that is not a storage pit (the
  order of `OPENBLACK_TEST_HIT_ABODE`; with pit 1 the index-th storage pit) down to `life` through `abodes::ReduceLife`
  (the taps, the site and, for a pit or a town centre crossing 0.75, the emergency).
- `OPENBLACK_TEST_VILLAGER_KILL="<reason 0-9>[,<n>[,<turn>]]"`: at turn 2 (or `<turn>`) VillagerDead(reason,
  GetPlayerOf, its life, 1) of villager n (or all); with `OPENBLACK_TEST_CORPSE_TURNS=<n>` the corpse's counter is n.
  The trace adds `death: <reason> killer <p> owner <p> amount <a> drop <d> help <kvwlp>`, `death: town <id>
  deaths[<r>] = <n> total <n>`, `setdying: counter <c> (graveyard <0|1>)`, `dying: -> 15 (reaction ...)`, `dead:
  smoke, soul <clip> mesh <m>, skeleton` / `dead: smoke, water, skeleton`, `dead: <counter>` every 100 turns, `dead:
  vanish`, `mourn: <state> of <dead>`, `orphan: -> 131`; the alignment trace adds `Alignment: player <p> death <r>
  <v>`.
- `OPENBLACK_VILLAGER_TRACE`: the distribution writes `civic: t=<t> k=<k> d=<d> v=<v> tmp=<m> ->
  skip(child)|skip(nocs)| cut|cs=0|cs=1`.
- `OPENBLACK_TEST_BUILD_AT="<x>,<z>[,<desire>]"`: on turn 2, the building side's ForceBuildingOfPlannedAtPos(MapCoords(x,
  0, z), desire × 5) (what BUILD_BUILDING does; the Land 1 temple `"1915.05,2508.89"` waits for the CitadelHeart). The
  trace adds `build: need|supplies|pit for materials|abodes desire|civic desire`, `build goto|184:` (ring, distance,
  footpath), `build 39:`, `build 40:` (ring, distance, look, wood dropped / added), `build 41:` (a, f, u, pile, value,
  x, % built, next / supplies / release), `build enter|exit <state> site <id> builders <n>`; `OPENBLACK_TOWN_TRACE`
  adds `sites:` (every 50 turns: each site's building, builders / max, pile, % built, repair).
- The town hooks (`OPENBLACK_TOWN_TRACE`, `OPENBLACK_TEST_TOWN_DESIRE`, `OPENBLACK_TEST_TOWN_EMERGENCY`) are on
  [Towns](towns.md#test-hooks); the home and age hooks on [Villager life](villager-life.md#test-hooks).

## Pending

- **The door of a multi-map fixed object that is not an abode** (no `Abode::yAngle`) takes its angle back from the
  Transform's rotation (`map_cells::detail::YAngleOf`), exact only to the rounding of its floats; so does an abode
  whose rotation was changed after it was placed. GetArrivePos has no test with a loaded mesh yet (the rule itself is
  pinned in test_villager_routine).
- **SpecialVillager**'s name over the head and its fade (`DrawVillagerInfo` 0x71F240, `CanShowName` 0x71F470) are
  ported as [the villagers' names](../interface/screen-and-readouts.md#the-villagers-names); not ported: the save of its row (`Save` 0x71F560 / `Load`
  0x71F5D0, save type 0x76). The speech bubble (`MakeHimSpeak` 0x71EF70) is only reached by the gathering box's
  messages, which are gone, so it is not ported ([Not in the original](../project/not-in-the-original.md)). The roll, the pick and the age are ported
  (it goes through the same `VillagerArchetype::Create`, `SetFirstClip` included).
- openblack differs: a downed villager's 0.05 and an eaten one's 0 write `Villager::life` straight, past
  `life::SetLife`, so the town's injured count misses them; the original goes through `Villager::SetLife` 0x756B40 in
  both (0x5EC491, 0x76B3C2) (`TargetPounce` and `ProcessDownedVillagers` in `src/ECS/AnimalPredators.cpp`), a fix row. The count is read by the
  creature's choice to heal the town (`town_compassion::HealFirst`), so the miss can reach it.
- **Script flocks**: the readers of the flock's shepherd (+0x30), and a town's own animals for the script's find in
  a container (queued fix; no shipped call asks a town for one, see
  [Villager speed and scripts](villager-speed-and-scripts.md#the-original)); the rest on
  [Villager speed and scripts](villager-speed-and-scripts.md#pending).
- **The soul's per-frame update** (openblack: once a turn, the clip runs per frame) (queued fix).
- **VillagerDead's leftovers**: the disciples and the sacrifice are ported (`Villager/VillagerDisciple.cpp`,
  `ECS/AltarSacrifice.cpp`); GameStats (+0x4C, +0x106C, +0x1124), Town +0xA08 and the multiplayer FUN_0064DA80 have
  no reader, so leaving them out changes nothing.
- **Reactions**: dancing: out of the dance on reacting, and back to it after (queued fix); the disciple columns, to
  check against the disciples now ported. The belief-only branch of the spread (0x763410) is ported
  (`ImpressOnly`, [Villager reactions](villager-reactions.md#openblack)), and 186's
  MakeCreatureEmpathiseWithPlayerTownDesire is (`creature_mimic::EmpathiseWithTownDesire`, `VillagerForester.cpp`).
  The rest is on [Villager reactions](villager-reactions.md#pending).
- **What villager the original makes for an unknown index** in CREATE_TOWN_VILLAGER, or whether it crashes later:
  known, the info at 0xDB9DB8 is past `GVillagerInfo::InfoList`, so its fields are the runtime statics that follow
  (the `_SecondsPerYear` copies 0xDB9DD4.., `VillagerName::First` 0xDB9E28, `GVillagerStateTableInfo::Infos`
  0xDB9E68), and no shipped script reaches it. Still open: their values in a running game, which the oracle's
  baseline does not build (`Villager::Create` 0x74FBE0 with that info).
- **The walk's bit-for-bit traces** (`GoesRoundClockwise`, `AreWeThere`): see [Wall hugging](wall-hugging.md#pending).
- **The walk round a circle, the shapes of the heart and the worship site, a thing that leaves the map**: on
  [Walking round obstacles](wall-hugging.md#pending) and [Routes](routes.md#pending).
- **[0xD99380]**: written by `CreateVillagerAtAbode` with the villager, no reader known; leaving it out changes nothing.
