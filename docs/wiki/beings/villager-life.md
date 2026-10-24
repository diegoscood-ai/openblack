# Villagers: home, food, sleep, age and births

## Summary

A villager lives in a home of its town: it goes home at night, sleeps until it is rested, eats from the home or the
town's store when it is hungry, and leaves home in the morning. Without a home it is homeless and sleeps in a tent;
without a town it is a vagrant. Homes keep a list of their inhabitants, and the town moves villagers between homes to
balance men, women and room. Villagers age, grow from child to adult, may die of old age, and women get pregnant at
bedtime and give birth. Children follow their mother or go to the creche. Disciples are villagers given a job by the
player.

openblack reproduces the home, food, sleep, homeless, age, pregnancy, birth, creche and the town's housing rules
**faithfully**, and the disciples' jobs (farmer, forester, fisherman, builder, breeder, craftsman, trader,
missionary, worship, from the vortex) through `DiscipleDecideWhatToDo`.

## The original

### Home, food, sleep, homeless and age

- **The night** ✅ faithful: Sleep (16) on top → distribution → `CheckSatisfySleep` 0x761490 → 36 → door → **37**
  ArrivesHome 0x760930 → `Villager::ArriveHome` 0x751FA0 (bit 4 of +0xE0, the abode's present-at-home count +0xB6
  `inc`, mesh hidden by the −4 clip) → **38** AtHome 0x760B10 = HomeDecideWhatToDo 0x75FEA0 → CheckSatisfySleep inside
  → CheckWhenGoingToBed 0x760B60 (returns 1 unless it dies of old age; once per stay, bit 0x2000) → **119**
  GotoBedAtHome 0x760B30 → **120** SleepingAtHome 0x760D70 (counter RestAtHomeTime 100; without a town it does not
  count) → DoSleeping 0x760DB0 every 100 turns (+0.05 life unless poisoned; continues while Sleep is the first of
  order 1 or life < 0.7: from 0.4 to 0.70000005 in 6 cycles). By day DoSleeping gives 0 → 38 → distribution or leisure
  → the exit **ExitAtHome** 0x761B40 of 35..38 and 118..121 does LeaveHome (0x751FD0: bits 4 and 0x2000 off, the
  present count `dec`) if the next state does not stay at home (info.dat row, file 0xC0). 121 WakeUpAtHome 0x760E50 =
  GoHome (no code sets it).
- **37** ✅ faithful: has not arrived (AreWeThere(door, 0)) → again to the door with FINAL 37 (literal, also from 249);
  built and repaired (life ≥ 1, IsRepaired 0x4016A0) → inside; damaged (< 0.3): functional home → inside, otherwise
  tent (238); hungry (food < 0.5 strict): `SetTopState(163)` if it is not functional and inside in the same turn
  (literal); otherwise SetupBuildingObject 0x758530 (see
  [Repairs](villager-jobs.md#repairs-the-tap-on-a-home-and-the-town-emergency)) and inside. Without a home → 129 and
  0.
- **38** ✅ faithful: emergency → 119; CheckNeedsAtHome 0x760110 (the pregnant woman stays; threshold
  `0.9·max(GetLifeDesireFromLife(0.7), POWER(0.5))` = 0.7875, the larger, not the smaller; 0.9 for the disciple that
  ignores needs; the child goes through CheckChildActivity = ChildDecideWhatToDo, always 1); the disciple;
  CheckNeededForSomething (also the worship every turn); HomeNothingToDo 0x75FFB0 (inside, GameRand(4) == 0 → 119
  with counter 0).
- **Homeless** ✅ faithful: DoGoingHome 0x760280 without a town → 130 (0x7604CD); more than 100 m from its town → walk
  to 10..35 m from it, on its side (FINAL the TOP); near → GetTentPos 0x7604F0 → 238, or a walk of 10..30 m. The tent:
  the nearest tree within 50 m (fn_00604AF0 with IsTree) if fn_0074C650 finds room for it (2 m from the tree, on the
  other side of the occupant; a villager in 238 or a MultiMapFixed within 4 m counts; two = full, and no other tree is
  tried); otherwise, 3 attempts: free cell (`Collide & 0x19 == 0`) and no villager in 238 within 5 m in the 9 cells of
  a spiral that **moves the point**: the tent ends up at (−20 m, +10 m) from the tested spot (literal quirk). **238**
  SleepInTent 0x761AE0, **129** HomelessStart 0x761320, **130** VagrantStart 0x76A8D0 (a town of its tribe within
  200 m → AddVillagerToTown and 163; hurt → tent; otherwise a walk of 10..30 m forward).
- **Eating** ✅ faithful: **CheckHungry** 0x75BCC0 (batch = turns·9e-5 divided by TribalPower[3] (player +0x74) and by
  the speed if it exceeds 1 and it is moving; damage 0.001 with hunger (food < 0.5, strict: IsHungry uses ≤) or
  poison: the `max(…, 1)` leaves the factor at 1; interruptions 0xD0 / 0xD4 of the final state's row; life 0 →
  STARVING, or CHANT from worship); the amount GetAmountOfFoodToEat 0x75BC20 = `ftol((1 − 0.3·clamp(the town's Food
  desire))·(float)(POWER(food)·85))` (74 with food 0.5); **ChangeStateToFindFoodToEat** 0x75B990 (needs 0 → 117, or
  118 inside; its functional home with enough → 36 / 118; the storehouse —the town's or, if there is none, its home—
  functional with enough → 33; without a functional storehouse → to the delivery point with FINAL 34; if it carries
  something, it eats it; otherwise 0); **117 / 118** EatFoodHeld 0x75BF20 (`eaten/toEat·1.2 + food`, clamped to [0,
  1], NaN → 0; `Town::UseFood` 0x73B5E0 adds to the town's food used +0x6F8); **GetFoodFromHome 0x75C040 takes
  twice** (GetResourceFrom already does PickupResource: the home loses n and the villager gains 2n, literal quirk);
  **34** ArrivesAtStoragePitForResource 0x7698D0 (takes min(what it needs, what there is) and returns to the door with
  FINAL 163; then it eats what it carries); **212** ShowPoisoned 0x75B940; **35** ArrivesAtHomeWithFood 0x769B30 (the
  housewife's). The home subtracts its food with DoResourceRemoving 0x404F60 (the town's CallDesireFunction first).
- **Age** ✅ faithful: CheckChildGrownUp 0x751050 at 13 → bit 8 off, age 18, ChildToAdult of the home (or of the town)
  and ChildBecomesAdult 0x757F10 (mother 0, CheckNeedNewAbode, **234** GoHomeAndChange 0x761810); the adult mesh
  arrives at the exit of 234 (ExitGoHomeAndChange 0x761980 → ChangeTribeIfRequired 0x7618C0 → ChangeInfo 0x761A00),
  not in SetAge. Otherwise, it rescales on the turns that are multiples of 375 turns, when its 9-turn check falls on one
  (`CheckChildGrownUp` 0x751050, from `CheckEveryTime` 0x7505D9; `turn % 375 == 0` at 0x7510D6..0x7510F1): with
  gcd(9, 375) = 3, a third of the children (check phase a multiple of 3) every 1125 turns, the rest not until a state
  without checks shifts the phase (CheckHungry 0x75BEE1 resets the check turn). Old age CheckDeathFromOldAge 0x760CA0 (in the periodic check, ≈ every 800 turns, and in
  CheckWhenGoingToBed): age > 60, `n = ftol(r³·40)` (the cube, not the square), `GameRand(n)`, dies if age + d > 100:
  nobody before 63. WomanSpecial 0x752240 (the pregnancy countdown) is literal; see
  [Pregnancy and births](#pregnancy-and-births). The scale of the constructor and of SetScaleForAge uses the
  synchronised GameFloatRand.
- **The two "turns per year" fields** are two values, not two readings of one. GGameInfo +0xC (1500, int) is read only
  by `Living::GetAge` 0x5ECB03, `SetAge` 0x5ED2C0 and the quarter-year growth checks 0x417A0D and 0x7510DC. +0x10
  (36000) / 365.25 = +0x14 (fn_00557810) is read only by `HousewifeStartsGivingBirth` 0x7621AF/0x7621DD (98.5626).
  SET_TURNS_PER_YEAR (0x714DAA) writes +0x10 without recomputing +0x14; only SET_DATE or SET_TIME would, and no
  shipped script uses any of the three.
- **Home and town** ✅ faithful: the abode's inhabitants are the ordered list +0xA0 (the head, the most recent: decides
  who moves in the Shuffle and the partner at bedtime), the partners +0xA8 / +0xAC, the adult / adult male / child
  counts +0xB4 / +0xB5 / +0xB7, the empty timer +0xB0; the town's homeless +0x768, ordered. AddVillagerToAbode
  0x404060, RemoveAliveVillagerFromAbode 0x404340 (inside → 163; its exit does the LeaveHome; the partner is not
  touched), RemoveDeletedVillagerFromAbode 0x404220 (clears both partners), RemoveAllVillagersFromAbode 0x404560 (the
  home destroyed → HomeDeleted → MakeHomeless), the score 0x404B40, FindAbodeWithSpaceInTown 0x73B370 (the newest wins
  ties), AddVillagerToTown 0x73A090 (the worship code's CheckAddWorshipSite with the first villager),
  CheckNeedNewAbode 0x757F90 (with percentTooCrowded 0.5, an adult alone in a home for 2 is already «too many»: it
  moves if there is something better or becomes homeless, literal). `Town::Process` step 4 (`Abode::Process` 0x404440:
  an empty built home loses 0.0001 life every 1001 processed turns, in float) and step 24
  (ShuffleVillagersAroundAbodes 0x741540 with VC6's `_qsort`, one move per call).
- **Creation by script**: 0x715A4C is CREATE_TOWN_VILLAGER (command 16, 0x715AA8..0x715AE6): it creates the villager
  at the second argument, looks for the town with FindTownWithID of integer slot 0 (0x715AA8, [ebp+0x6000]) and calls
  AddVillagerToTown, which chooses the home; without that town, the one nearest to the villager's position
  (fn_00552FF0). The command table 0xC20F20 gives 15 CREATE_VILLAGER "AAL", 16 CREATE_TOWN_VILLAGER "NAAN", 17
  CREATE_SPECIAL_TOWN_VILLAGER "NANN", 18 CREATE_VILLAGER_POS "AALN". `LHScriptX::ScanLine` 0x7E7540
  (0x7E77C3..0x7E77F0) writes one slot per argument position, and only for an 'N' (atol): command 16's slot 0 is its
  own N0 (its age slot 3, [+0x600C]), command 18's age its own N3. Only CREATE_VILLAGER reads a slot its line does not
  write (its age from slot 2, which its 'L' leaves as the last command wrote it). CREATE_VILLAGER /
  CREATE_VILLAGER_POS (command 18, `cmp eax, 0x12` at 0x715B9B; 0x715D84..0x715DF1) look the abode up at A0, otherwise
  FindAbodeWithSpaceInTown / AddVillagerToTown, otherwise the vagrants (0x715DC0). `Villager::Create` 0x74FBE0 houses
  nobody: the script handlers do it.
- **IsReachable** (0x756460): available, not at home, TOP ≠ 236.
- The non-villager occupant of fn_0074C650 (+0x24 & 2, 0x74C6F8) is a MultiMapFixed, FishFarm included: only the full
  MultiMapFixed constructor 0x52E1E0 sets the bit (`or byte ptr [esi+0x24], 2` at 0x52E207; bw1-decomp
  `GAME_THING_WITH_POS_FLAG_FIXED`, Fixed.cpp:225), nothing clears it, and objects start zeroed (0x4368C4). The same
  evidence answers bw1-decomp TODO #343.
- **🔍 inferred**: TribalPower[3] is 1.0 (nobody writes it); +0x24 & 4 of IsReachable and IsAvailableForStateChange is
  «in the hand» (PlaceObjectInMagicHand 0x5FB014); 115 only by script; IsInScript of a home (+0x24 & 0x200) is 0.
- TownStats::Add 0x7492E0 / Remove 0x7493C0 keep the town's counts (adults or children, the sex,
  FoodRequiredForDinner +/- the info's, TotalFood / TotalWood +/- what it carries, the disciple count) up to date as
  villagers join and leave. `Abode::ReduceLife` 0x405D90 is the home's own life loss.
- DoGoingHome 0x760280 (arrive, tent): a villager already inside → SetTopState(38). GetFinalState already equal to
  `arrive` → 1 (already on the way). A dancing villager first leaves its dance (IsDancing vt +0x978 →
  RemoveFromDance((flags >> 1) & 1) vt +0xB08). Homeless and near the town, the tent search starts from me +
  GetPosFromAngle(GameFloatRand(2π), GameFloatRand(8) + 2). The far walk is town + GetPosFromAngle(GameFloatRand(π/2) −
  π/4 + the angle from the town to me, GameFloatRand(25) + 10).
- SetupMoveToOnFootpath 0x5EDD20: when I stand on the object's arrive point and the target is elsewhere, a direct
  walk. Otherwise object.UseFootpathIfNecessary (vt +0x80: MultiMapFixed 0x52EEC0 / GameThingWithPos 0x570350): with
  a footpath link `GFootpathLink::UseFootpathIfNecessary` 0x5362E0, without one SetupMoveToWithHug (0x57037E).
- ArrivesHome 0x760930: IsRepaired is Abode 0x4016A0 (life not below 1, vt +0x884). For a damaged non-functional
  home, the tent search starts from me + GetPosFromAngle(angle from the home to me + (π/8 − GameFloatRand(π/4)),
  GameFloatRand(5) + 5).
- `Villager::ArriveHome` 0x751FA0 calls `Abode::ArriveHome` 0x405FA0; LeaveHome 0x751FD0 (only when inside: flags &=
  0xDFFB) calls `Abode::LeaveHome` 0x405FB0.
- HomeDecideWhatToDo 0x75FEA0: in the town's emergency the villager hides in bed (119). A disciple whose type ignores
  needs: a BREEDER (5) with Sleep first in the town's order 1 and CheckSatisfySleep → 1; otherwise DecideWhatToDo (vt
  +0x8C8) and its result.
- CheckNeedsAtHome 0x760110: a woman with WomanSpecial == 1 → 1; a pregnant woman → 1 (she stays home doing
  nothing). The thresholds (D, F) are (+0x360, +0x2C0), or (+0x35C, +0x2C4) for a disciple that ignores needs. A child
  goes through CheckChildActivity 0x757F00.
- GotoBedAtHome: SetTopState(120), then +0x58 = RestAtHomeTime (info +0x24C). SleepingAtHome without a town never
  counts down: it sleeps forever (literal).
- DoSleeping 0x760DB0: poisoned → 0 (no sleep, no healing). Life below info.life (+0x128) → IncreaseLife(f ×
  RestAtHomeRestoresLifeBy +0x250) (Villager 0x753460 → Object 0x637870). It goes on while Sleep is first in order 1,
  or life < DamageThresholdToSleepUntil (+0x360); then +0x58 = RestAtHomeTime.
- SleepInTent 0x761AE0: a counter that is not 0 → −1, 1. Then DoSleeping(1) != 0 → 1. A homeless villager tries
  CheckHomelessMoveIntoAbode. Then r = HomeDecideWhatToDo; when r == 0 or the TOP is still 238, the counter is reset
  to RestAtHomeTime and decremented.
- GetTentPos 0x7604F0: Collide(0x19) means 0x10 off the game map, 8 a fixed object, 1 water (`MapCoords::Collide`
  0x6033B0 = Collide 0x6033C0 & type). Between the three tries, tmp += GetPosFromAngle(GameFloatRand(2π),
  GameFloatRand(5) + 3). The 5 is drawn first because it is the last argument pushed (0x760654..0x7606A0).
- HomelessStart 0x761320: calls CheckHungry directly, which resets LastCheckTurn and so moves the phase of the
  periodic check. Then CheckNeededForSomething, then CheckHomelessMoveIntoAbode; else SetupNothingToDo.
- VagrantStart 0x76A8D0: `MapCoords::GetNearestTown` 0x6020E0 within 200 m, and of my tribe. A hurt villager's tent
  search starts from me + GetPosFromAngle(GameFloatRand(2π), GameFloatRand(5)), drawn d first, then a. The forward
  walk is a = yaw + (GameFloatRand(π/4) − π/8), d = GameFloatRand(20) + 10, only InBounds, FINAL 130.
- CheckHomelessMoveIntoAbode 0x761360: FindAbodeWithSpaceInTown(me, 0) → out of the homeless list,
  AddVillagerToAbode, SetTopState(36); 1.
- MakeHomeless 0x761220 = MakeHomelessNoStateChange 0x761240 + SetTopState(129). MakeHomelessNoStateChange: out of
  its abode (SetAbode(0), SetTown(town)); no town or already in the list → 0; out of the vagrants; at the head of Town
  +0x768, ++ +0x76C; 1.
- HomeDeleted 0x7611F0: with an abode → MakeHomeless; else `Villager::TownDeleted` 0x750B50. Living +0x60, which it
  clears first (0x7611F8), is `MobileWallHug::target`, the object a move-to-object walk follows; only `SetupMobileMoveToObject` 0x60ACD0 sets it, for
  footballs, players and flocks, never an abode, so the clear is a guard that never fires in play.
- CheckNeedNewAbode 0x757F90: a child → 0 (so a child in 114 with no mother and no home stays there). A home that is
  not too crowded → 0. No town → VagrantStart, 1. Otherwise s = the score of the current home for me (0 without one)
  and FindAbodeWithSpaceInTown(me, s). On a find, MoveVillagerToAbode == 1 → SetTopState(36) if IsVillagerAvailable,
  1. Otherwise MakeHomeless unless the villager is already in the town's homeless list; 1.
- MoveVillagerToAbode 0x758080: room left (GetRoomLeftForChildren / ForAdults, signed) > 0 →
  ForceMoveVillagerToAbode, 1. ForceMoveVillagerToAbode 0x756240: same town → AddVillagerToAbode. Otherwise the old
  town's RemoveVillager, then AddVillagerToAbode below 100 % full (GetPercentAbodeFullWithChildren vt +0x8A0 /
  WithAdults vt +0x89C), else the new town's AddVillagerToTown.
- FindPosOutsideAbode 0x753470(abode; null = its own): door + GetPosFromAngle(angle from the home to the door + (π/8 −
  GameFloatRand(π/4)), GameFloatRand(1.5) + 1.5). The 1.5 is drawn first.
- GoHomeAndChange (234) 0x761810: 163 is taken only when the villager has no abode (0x76181E). With an abode, away
  from the door it walks there with FINAL 234; at the door it takes 38 if inside (+0xE0 & 4), else 37
  (0x76185A..0x76186B). A scale below 0.95 is reset with SetScaleForAge (0x76187D..0x7618A1); the reset follows both
  SetTopState calls, not the walk.
- ExitGoHomeAndChange 0x761980: the same exit → nothing. Otherwise ChangeTribeIfRequired(the town's tribe +0x5B8, or
  the info's +0x1F4 without a town; leaving = next's row 0xC0 == 0), then a CHANGE_HOUSE disciple (+0xF2 == 10,
  0x7619D9) → SetVillagerDisciple(0).
- ChangeTribeIfRequired 0x7618C0: KeepMeshWhenChangeTown (+0x388) != 0 → nothing. Otherwise
  `GVillagerInfo::Find`(tribe, my number +0x1FC). With a town, Town +0x6F4 += new +0x2D8 − old +0x2D8; then
  ChangeInfo. When leaving the home, a smoke puff CreateSmokyStuff(1, 1.0, −1) 0x63A810 half the villager's height up.
- ChangeInfo 0x761A00: +0x28 = the new info. A child's three meshes all become +0x204 ChildMeshHigh (SetAge uses
  +0x20C / +0x208 / +0x204: an oddity of the original). An adult gets GetDetailMesh(2 / 1 / 0) (vt +0x60C).
- SetAge 0x752907 / CheckChildGrownUp 0x751079: an adult is at least 18 (0x12)
- SetScaleForAge 0x752ABA: 0.75 (0x8AB274) of the way to the next age's scale
- SetScaleForAge 0x752AFF / 0x752B0B / 0x752B16: GameFloatRand(0.1 [0x3DCCCCCD]), 0.05 [0x8AC3F4] - it, + 1 [0x8AA390]
- `RescaleTurn`: 0x7510D6..0x7510EF: g_game +0x205A40 % ([0xD01A04] >> 2) (unsigned div)
- `OldAgeRange`: 0x760CEA..0x760CF4: the loop of two fmul: r x r x r (24-bit FPU: each one rounded to a float)
- `OldAgeRange`: 0x760CF6..0x760D15: fild qword (u32) (retirement - old); fmulp; __ftol
- `ScaleForAge`: 0x752AA4..0x752AE9: (ageToScale[age + 1] - GetScale) x 0.75 (fsubr, fmul, stored as the argument); GameFloatRand (Villager.cpp 0x92F); SetScale(GetScale + it)
- `ScaleForAge`: 0x752AF5..0x752B1C: t = (0.05 - GameFloatRand(0.1)) + 1 (0x933)
- `ScaleForAge`: 0x752B20..0x752B2F: GetScale < t (test ah, 1) -> another (0.05 - GameFloatRand(0.1)) + 1 (0x933); else GetScale
- `SetAgeAndScale`: 0x752919..0x7529B1 / 0x7529B6..0x752A5C: the meshes only when the age crosses grownUpAge (GetAge with the old birth turn): a new child the child meshes (+0x20C / +0x208 / +0x204), a new adult GetDetailMesh(2 / 1 / 0). The skeleton branch (IsSkeleton vt +0x4A0, [0xDA6BC8]) is V12's
- `SetAgeAndScale`: 0x752A62 InitialiseScale, 0x752A6D SetScaleForAge
- `CheckChildGrownUp`: 0x751059..0x751065: GetAge >= grownUpAge (+0x138)
- `CheckChildGrownUp`: 0x751067: flags &= ~8
- `CheckChildGrownUp`: 0x751070..0x751088: SetAge(max(grownUpAge, 18)) (vt +0x8D4)
- `CheckChildGrownUp`: 0x7510AA / 0x7510CE: return ChildBecomesAdult
- `CheckChildGrownUp`: 0x7510D6..0x751100: turn % 375 == 0 -> SetScaleForAge(GetAge)
- `ChildBecomesAdult`: 0x757F13: mother = 0
- `ChildBecomesAdult`: 0x757F1D CheckNeedNewAbode; 0x757F24..0x757F2B SetTopState(0xEA 234 GO_HOME_AND_CHANGE); 1
- `CheckDeathFromOldAge`: 0x760CAC..0x760CB8: GetAge <= oldAge (+0x13C, jbe) -> 0
- `CheckDeathFromOldAge`: 0x760CCD..0x760CDE: GameFloatRand(1) (VillagerHome.cpp 0x258); 0x760D09..0x760D1B: GameRand(n) (0x25A)
- `CheckDeathFromOldAge`: 0x760D2C..0x760D3D: GetAge (again) + d > retirementAge (+0x140)
- `CheckDeathFromOldAge`: 0x760D3F..0x760D51: VillagerDead(9 OLD_AGE, 0 (0x760D4B push 0: no player), info +0x128 life, 1)
- `WomanSpecial`: 0x752243..0x75225A: pregnant, and +0x24 & 0x400 clear (not controlled by a script)
- Living::GetAge 0x5ECAF0: (turn - birthTurn) / 1500, an unsigned division (`xor edx, edx; div`, 0x5ECB01)
- Living::SetAge 0x5ED2C0: birthTurn = turn - age * 1500 (`imul`, `sub`, 0x5ED2C5..0x5ED2D6)
- CheckChildGrownUp 0x75105F: GetAge >= grownUpAge (+0x138), unsigned (jb)
- CheckChildGrownUp 0x7510D6..0x7510F1: the rescale turn of a child: turn % (TimeScale >> 2 = 375) == 0
- CheckDeathFromOldAge 0x760CBE..0x760D1A: n = ftol(r^3 x (retirementAge - oldAge)) (the two fmul of POWER's loop, then fild qword, fmulp), the GameRand's range
- `OldAgeDies`: 0x760D2C..0x760D3D: age + d > retirementAge (unsigned, jbe) -> dies
- Villager::SetScaleForAge 0x752A90 from the scale `current`: a child current + GameFloatRand((ageToScale[age + 1] - current) x 0.75) (Villager.cpp 0x92F); an adult t = (0.05 - GameFloatRand(0.1)) + 1 (0x933), current < t ? (0.05 - GameFloatRand(0.1)) + 1 : current. Draws the game's synced GameFloatRand
- Villager::InitialiseScale 0x74FB80 / SetScaleForAge 0x752A90 on the villager's Transform scale (Object +0x120 / +0x124 GetScale / SetScale: the uniform scale)
- Villager::HousewifeStartsGivingBirth 0x7621A0, WomanSpecial's tail, is in VillagerBirth.h
- `BirthCounterRange`: 0x7621AF..0x7621BF: fld [0xD01A0C]; __ftol 0x7A1400 (truncated)
- `BirthCounter`: 0x7621CA..0x7621F6: fild qword (r, unsigned), fld [0xD01A0C], fmul 0.25, faddp, fadd 1, __ftol, mov word [+0x58]
- `HousewifeStartsGivingBirth`: 0x7621AF..0x7621F6: +0x58 = BirthCounter(GameRand(ftol(+0x14)) (VillagerHousewife.cpp 0x120))
- `HousewifeStartsGivingBirth`: 0x7621FA..0x762200: SetTopState(0x6F 111 HOUSEWIFE_GIVING_BIRTH) (vt +0x8E8)
- `ChildBorn`: ⚠️ approximate called here for a skeleton mother only, so its SetScaleForAge draw (0x756436) happens only then
- HousewifeStartsGivingBirth 0x7621AF..0x7621C5: GameRand(ftol([0xD01A0C])), GGameInfo +0x14 = turns per year (+0x10, 36000.0f) / 365.25 (fn_00557810 0x55782F): ftol(98.5626f) = 98
- `BirthCounter`: 0x7621CA..0x7621F6: +0x58 = ftol(fild(r) + [0xD01A0C] x 0.25 [0x8AB3D4] + 1 [0x8AA390]) (u16, float steps): r + 25
- State 110 HOUSEWIFE_STARTS_GIVING_BIRTH (0x7621A0; 🔍 inferred no code sets 110: no `push 0x6E` before SetTopState)
- `Fallback`: 0x7518E7..0x7518F3: GetFinalState (vt +0xB04) == 0xDD 221 -> 0
- `FindDisciplePrayerPos`: 0x754208..0x754243: out = centre +0x14 + GetPosFromAngle(a, r) 0x74D580 (MapCoords::operator+ 0x605520); 1
- `HungerBatch`: 0x75BCE7..0x75BCF7: (fild qword: unsigned) turns x info +0x2BC, stored
- `HungerBatch`: 0x75BD0B..0x75BD20: with a player, drop / player +0x74 (fdivr)
- `HungerBatch`: 0x75BD24..0x75BD4B: speed > 1.0 (double; test ah, 0x41) and IsMoving -> drop = speed x drop
- `HungerBatch`: 0x75BD4F..0x75BD77: food - drop, below 0 -> 0
- `FoodToEat`: 0x75BC2A..0x75BC49: POWER(food) x (fimul, u32 read as a qword's low half) foodReqiredForDinner, stored
- `FoodToEat`: 0x75BCA8
- `FoodToEat`: 0x75BC5B..0x75BC87: below 0 -> 0, above 1 (test ah, 0x41) -> 1
- `FoodToEat`: 0x75BC8D..0x75BC9D: ftol((1 - c x 0.3) x t)
- `EatHeld`: 0x75BF26..0x75BF67: eat (fild qword: unsigned) and held (fild: movsx) as floats; the smaller (fcomp; test ah, 1)
- `EatHeld`: 0x75BF7C..0x75BFC1: eaten / eat x info +0x2B8 + food; below 0 or unordered (test ah, 1) -> 0; above 1 -> 1
- `GetAmountOfFoodToEat`: 0x75BC4D..0x75BC5B: with a town, its Town +0x14C (TownDesire +0x118[0], the desire without the boosts)
- `CheckSatisfyOwnFoodDesire`: 0x75BF03: IsHungry (food <= hungryForFood) -> ChangeStateToFindFoodToEat
- `ChangeStateToFindFoodToEat`: 0x75B998: need = GetAmountOfFoodRequiredForMeal; 0x75B99F..0x75B9B2: eat = inside ? 118 : 117
- `ChangeStateToFindFoodToEat`: 0x75B9B4: need 0 -> SetTopState(eat); 1
- `ChangeStateToFindFoodToEat`: 0x75B9BA..0x75B9FD: a functional abode whose GetResource(FOOD) + (movsx) held >= need (unsigned, jb) -> 36, or 118 inside (0x24 + 0x52 x inside)
- `ChangeStateToFindFoodToEat`: 0x75BA26..0x75BA74: GetStoragePit (the town's, else its abode) functional: enough -> 33; else on to what it carries
- `ChangeStateToFindFoodToEat`: 0x75BA75..0x75BAAF: pos = GetResourceDropoffPos(FOOD); not IsCloseToEqual(pos, me, 0) -> walk, FINAL 34; 1
- `ChangeStateToFindFoodToEat`: 0x75BAB0..0x75BAD3: it carries some (+0xF4 != 0) -> SetTopState(eat); 1; else 0
- `EatFoodHeld`: 0x75BF6B..0x75BF77: DropFood(ftol(eaten)) (DropFood(0) drops all of it: 0x7511EB)
- `EatFoodHeld`: 0x75BF7C..0x75BFC1: the belly
- `EatFoodHeld`: 0x75BFCB..0x75BFE9: with a town, Town::UseFood(ftol(eaten))
- `GetFoodFromHome`: 0x75C04F..0x75C06F: m = min(n, GetResource(FOOD)) (cmp; jb keeps n)
- `GetFoodFromHome`: 0x75C071..0x75C077: GetResourceFrom(abode, FOOD, m): RemoveResource + PickupResource
- `GetFoodFromHome`: 0x75C07C..0x75C07F: PickupFood(what it took) again (literal: the villager gains twice what the abode lost)
- 0x75C003 EatFoodHeld; 0x75C00E..0x75C025: PlayAnimThenSetState(poisoned ? 0xD4 212 : 0xA3 163, 1)
- `EatFoodAtHome`: 0x75C094..0x75C0AC: the held food read first; n = GetAmountOfFoodToEat - held; > 0 -> GetFoodFromHome(n)
- `EatFoodAtHome`: 0x75C0B3 EatFoodHeld; 0x75C0BA..0x75C0D5: SetTopState(poisoned ? 0xD4 212 : 0x26 38)
- `ShowPoisoned`: 0x75B946..0x75B967: inside -> SetupMoveToWithHug(FindPosOutsideAbode(0), 212)
- `ShowPoisoned`: 0x75B96C..0x75B975: PlayAnimThenSetState(163, 1)
- `GotoStoragePitForFood`: 0x769836..0x769880: GetStoragePit functional -> SetupMoveToOnFootpath(pit, pit.GetArrivePos, 34)
- `GotoStoragePitForFood`: 0x76988A..0x7698A1: else SetupMoveToWithHug(GetResourceDropoffPos(FOOD), 34)
- `ArrivesAtStoragePitForFood`: 0x7698B1..0x7698C7: ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal, 163, 163)
- `ArrivesAtHomeWithFood`: 0x769B33..0x769B62: with an abode, abode.AddResource(FOOD, (movsx) DropFood(0), 0, 0, 0, 0)
- `ArrivesAtHomeWithFood`: 0x769B6C ArrivesHome 0x760930 (its result)
- CheckHungry's batch 0x75BCD6..0x75BD77: drop = (float)(u64) turns x 9e-5 (+0x2BC); / tribalPower (player +0x74, when the villager has a player); x speed when speed > 1 (double compare) and it is moving; food = max(food - drop, 0)
- Villager::CheckSatisfyOwnFoodDesire 0x75BF00: IsHungry ? ChangeStateToFindFoodToEat : 0
- 117 EAT_FOOD: Villager::EatFood 0x75C000: EatFoodHeld; PlayAnimThenSetState(poisoned ? 212 : 163). 1
- 118 EAT_FOOD_AT_HOME: Villager::EatFoodAtHome 0x75C090: eat - held > 0 -> GetFoodFromHome; EatFoodHeld; SetTopState(poisoned ? 212 : 38). 1
- 34 ARRIVES_AT_STORAGE_PIT_FOR_FOOD: Villager::ArrivesAtStoragePitForFood 0x7698B0 = ArrivesAtStoragePitForResource(FOOD, GetAmountOfFoodRequiredForMeal, 163, 163)
- DoGoingHome 0x76034D: more than 100 m (0x8AB41C) from the town -> towards it
- GetTentPos 0x760505: fn_00604AF0's radius 50 (0x42480000); 0x7605DC the 238 villagers within 5 (0x8AB6E4)
- GetTentPos 0x760584: MapCoords::Collide(0x19) (0x10 off the game map, 8 a fixed, 1 water)
- GetTentPos: 3 tries (byte [esp + 0x13], 0x7606AB `cmp al, 3`), 9 spiral cells each (0x7605A1)
- `k_VagrantTownRadius`: VagrantStart 0x76A8DB: towns within 200 (0x43480000)
- GoHomeAndChange 0x761887: a scale below 0.95 (0x8CF000) is reset for the age
- `k_NeedsAtHome`: CheckNeedsAtHome 0x760204: x 0.9 (0x8C5844)
- VillagerDisciple 10 CHANGE_HOUSE (ExitGoHomeAndChange 0x7619D9 `cmp byte +0xF2, 0xA`)
- GameThingWithPos::IsVillager (vt +0x2C8: Villager 0x55CAB0 1, the rest 0)
- GameThingWithPos::IsTree (vt +0x338): Tree::IsTree 0x55D9D0 (1), GameThingWithPos 0x402320 (0). DeadTree is another class
- MapCoords::Collide(COLLIDE_TYPE) 0x6033B0 = Collide 0x6033C0 & the type, through the villagerTentQueries service
- `IsReachable`: 0x75648F..0x75649A: TOP != 236 (0xEC)
- `IsVillagerAvailable`: 0x752299 IsAvailableForStateChange (GameThingWithPos 0x401A30: !(+0x24 & 4), in the hand: 0x5FB014)
- `IsVillagerAvailable`: 0x7522A5 GetVillagerAvailableState 0x751F40 (the file 0xA8 of GetFinalState's row) & 1
- 0x751FA3..0x751FBC: with an abode, flags |= 4, Abode::ArriveHome 0x405FA0
- `DoGoingHome`: 0x760289..0x7602A5: IsDancing (vt +0x978) -> RemoveFromDance((flags >> 1) & 1) (vt +0xB08).
- `DoGoingHome`: 0x7602B8..0x7602C5: inside (flags & 4) -> SetTopState(38 AT_HOME)
- `DoGoingHome`: 0x7602CA..0x7602E4: GetFinalState == arrive -> 1 (already going)
- `DoGoingHome`: 0x7602EA..0x7602FE: SetupMoveToOnFootpath(abode, abode.GetArrivePos, arrive)
- `DoGoingHome`: 0x760310..0x76031B: no town -> SetTopState(130 VAGRANT_START)
- `DoGoingHome`: 0x760343..0x76035B: GetDistanceInMetres (0x74CD50) > 100 (test ah, 0x41; jne: <= 100 or unordered -> near)
- `DoGoingHome`: 0x76036B..0x7603EC: the TOP (+0x8C) read first; a = (GameFloatRand(pi/2) (0x127) - pi/4) + Get3DAngleFromXZ(town, me); d = GameFloatRand(25) (0x128) + 10; pos = town + GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP)
- `DoGoingHome`: 0x7603F1..0x76043D: d = GameFloatRand(8) (0x12C) + 2, a = GameFloatRand(2 pi) (0x12C): pos = me + GetPosFromAngle
- `DoGoingHome`: 0x760442..0x760450: GetTentPos(pos) -> SetupMoveToWithHug(pos, tent)
- `DoGoingHome`: 0x760452..0x7604B9: the TOP read; d = GameFloatRand(20) (0x130) + 10, a = GameFloatRand(2 pi) (0x130); pos += GetPosFromAngle(a, d); SetupMoveToWithHug(pos, TOP): a stroll, then 36 again
- `SetupMoveToOnFootpath`: 0x5EDD39..0x5EDD63: me == object.GetArrivePos && pos != me -> SetupMoveToWithHug(pos, final)
- `SetupMoveToOnFootpath`: 0x5EDD72..0x5EDD7D: object.UseFootpathIfNecessary(this, pos, final) (vt +0x80): MultiMapFixed 0x52EEC0 / GameThingWithPos 0x570350 with a GetFootpathLink -> GFootpathLink::UseFootpathIfNecessary 0x5362E0; without one SetupMoveToWithHug(pos, final) (0x57037E).
- `ArrivesHome`: 0x760938..0x760941: no abode -> SetTopState(0x81 129 HOMELESS_START); 0
- `ArrivesHome`: 0x760A91..0x760A9E: ArriveHome; SetTopState(0x26 38)
- `ArrivesHome`: 0x760947..0x760965: AreWeThere(door, 0) (vt +0x85C) -> else SetupMoveToOnFootpath(abode, door, 0x25 37): the literal 37, also from 249
- `ArrivesHome`: 0x76096B..0x760985: IsBuilt (vt +0x890) && IsRepaired (vt +0x88C: Abode 0x4016A0, the life (vt +0x884) not below 1)
- `ArrivesHome`: 0x76098B..0x7609A3: life < DamageThresholdToGoHome (+0x35C; test ah, 1)
- `ArrivesHome`: 0x7609A9..0x7609B5: a functional abode -> in
- `ArrivesHome`: 0x7609BB..0x760A2D: a = Get3DAngleFromXZ(abode, me) + (pi/8 - GameFloatRand(pi/4) (0x1D1)); d = GameFloatRand(5) (0x1D2) + 5; pos = me + GetPosFromAngle(a, d)
- `ArrivesHome`: 0x760A2D..0x760A4D: GetTentPos -> SetupMoveToWithHug(pos, 0xEE 238), the literal 238 (also from 249); 1
- `ArrivesHome`: 0x760A5E..0x760A72: food < HungryForFood (+0x2C0; test ah, 1: strict)
- `ArrivesHome`: 0x760A74..0x760A8B: not functional -> SetTopState(0xA3 163) and on to ArriveHome / 38 (no jump between: the 163 is overwritten in the same turn, literal)
- `ArrivesHome`: 0x760AB0..0x760ABB: SetupBuildingObject(abode) 0x758530 == 1 -> 1. V11 : so a villager arriving at a home that is not (built and at full life), with life >= 0.3 and food >= 0.5, repairs it when the site takes it (it is a builder of it already, the site needs builders: home life <= 0.9 with inhabitants, or it is a BUILDER disciple); else (full, or life in (0.9, 1)) it goes in
- `HomeDecideWhatToDo`: 0x75FEA3..0x75FED5: an abode, a town, and the town's emergency -> SetTopState(0x77 119); 1 (it hides in bed)
- `HomeDecideWhatToDo`: 0x75FEDC..0x75FEE8: CheckNeedsAtHome == 1 -> 1
- `HomeDecideWhatToDo`: 0x75FEEA..0x75FF57: a disciple (flags & 0x200) whose type ignores the needs
- `HomeDecideWhatToDo`: 0x75FF1D..0x75FF46: a BREEDER (5) with Sleep (16) first in the town's order 1 and CheckSatisfySleep -> 1
- `HomeDecideWhatToDo`: 0x75FF59..0x75FF65: CheckNeededForSomething == 1 -> 1
- `CheckNeedsAtHome`: 0x760114..0x76013E: a woman: WomanSpecial == 1 -> 1; pregnant -> 1 (she stays at home doing nothing)
- `CheckNeedsAtHome`: 0x76013F..0x760204: t = the larger of GetLifeDesireFromLife(D) and POWER(F) (POWER not below it keeps POWER): a disciple that ignores the needs (D, F) = (+0x35C, +0x2C4), else (+0x360, +0x2C0)
- `CheckNeedsAtHome`: 0x760204..0x760219: CheckSatisfyOwnDesire(t x 0.9) == 1 -> 1
- `HomeNothingToDo`: 0x75FFB3..0x75FFE1: inside: GameRand(4) (VillagerHome.cpp 0x4B) == 0 -> counter (+0x58) = 0, SetTopState(0x77 119)
- `ExitAtHome`: 0x761B40..0x761B5F: Infos[next] (0xDB9F38 + 0x114 next: memory +0xD0 = file 0xC0, StaysAtHomeOnExit) == 0 -> LeaveHome
- `GotoBedAtHome`: 0x760B33..0x760B47: SetTopState(0x78 120), then +0x58 = info +0x24C RestAtHomeTime
- `SleepingAtHome`: 0x760D73..0x760D7A: no town -> nothing (it sleeps for ever, literal)
- `SleepingAtHome`: 0x760D7C..0x760D85: --counter (u16); not 0 -> 1
- `SleepingAtHome`: 0x760D87..0x760D9D: DoSleeping(1) == 0 -> SetTopState(0x26 38)
- `DoSleeping`: 0x760DB6..0x760DBE: poisoned (vt +0x4A4) -> 0 (no sleep, no healing)
- `DoSleeping`: 0x760DC4..0x760DF9: life < info +0x128 (life) -> IncreaseLife(f x +0x250 RestAtHomeRestoresLifeBy) (Villager 0x753460 -> Object 0x637870)
- `DoSleeping`: 0x760DFB..0x760E2E: (a town and GetSortedDesire(0).index == 16) or life < +0x360 DamageThresholdToSleepUntil -> +0x58 = RestAtHomeTime; 1
- `CheckWhenGoingToBed`: 0x760B64..0x760B70: flags & 0x2000 -> 1; 0x760B76: flags |= 0x2000 (LeaveHome clears it)
- `CheckWhenGoingToBed`: 0x760B82..0x760B8E: CheckDeathFromOldAge -> 0
- `CheckWhenGoingToBed`: 0x760B90..0x760BC9: no town, GetRawDesire(16) < 1 (test ah, 1) or not sexually active -> 1
- `SleepInTent`: 0x761AE3..0x761AE8: counter != 0 -> --counter; 1
- `SleepInTent`: 0x761AEA..0x761AF6: DoSleeping(1) != 0 -> 1 (the counter was reset)
- `SleepInTent`: 0x761AF8..0x761B0C: no abode and CheckHomelessMoveIntoAbode -> 1
- r == 0 or TOP still 238 -> counter = RestAtHomeTime, then --counter
- `GetTentPos`: 0x7604F9..0x760547: the nearest tree to me (+0x14) within 50 and fn_0074C650 -> pos = its point; 1
- `GetTentPos`: 0x76056D..0x7606B1: tmp = pos; three tries
- `GetTentPos`: 0x760584..0x760591: MapCoords::Collide(0x19) != 0 -> on to the next try's move
- `GetTentPos`: 0x760597..0x76064A: free = 1; 9 cells from tmp's, tmp moved one spiral step after each
- `GetTentPos`: 0x7605A6..0x76062B: the cell's objects (the fixed list, then the mobile one): a villager (vt +0x2C8) within 5 m of tmp (fn_00605CD0; test ah, 1) whose TOP is 238 -> free = 0, on to the next cell
- `GetTentPos`: 0x76062D..0x760644: GUtils::Spiral, tmp += the step
- `GetTentPos`: 0x760650..0x7606CC: free -> pos = tmp (9 spiral steps from the cell tried); 1
- `GetTentPos`: 0x760654..0x7606A0: tmp += GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(5) + 3) (0x16D: the 5 is drawn first, it is the last argument pushed)
- `TentNextToTree`: 0x74C655..0x74C77E: 9 spiral cells from the tree's
- `TentNextToTree`: 0x74C6AF..0x74C6DA: fn_00605CD0(tree, object) - Get2DRadius (vt +0x64) < 4 (test ah, 1)
- `TentNextToTree`: 0x74C6DC..0x74C6FC: a villager in 238, or a non-villager with +0x24 & 2 (the MultiMapFixed bit, MultiMapFixed ctor 0x52E1F0)
- `TentNextToTree`: 0x74C6FE..0x74C71E: a second occupant -> 0; the first one's position kept
- `TentNextToTree`: 0x74C784..0x74C7D7: out = the tree; += GetPosFromAngle(a, 2), a = Get3DAngleFromXZ(occupant, tree) (the other side) or Get3DAngleFromXZ(tree, villager)
- `HomelessStart`: 0x761323..0x76134D: CheckHungry == 1 (the direct call resets LastCheckTurn: the periodic check's phase moves), CheckNeededForSomething == 1, CheckHomelessMoveIntoAbode != 0 -> 1;
- `VagrantStart`: 0x76A8D8..0x76A918: MapCoords::GetNearestTown(200) 0x6020E0; of my tribe (Town::GetTribe 0x73C840 = the GTribeInfo of +0x5B8, Villager::GetTribe 0x751EE0 = the info's +0x1F4) and AddVillagerToTown -> SetTopState(0xA3 163); 1
- `VagrantStart`: 0x76A92A..0x76A942: life < DamageThresholdToGoHome (+0x35C; test ah, 1)
- `VagrantStart`: 0x76A944..0x76A9B2: d = GameFloatRand(5), a = GameFloatRand(2 pi) (VillagerStates.cpp 0x27F, in that order); pos = me + GetPosFromAngle(a, d); GetTentPos -> SetupMoveToWithHug(pos, 0xEE 238)
- `VagrantStart`: 0x76A9C3..0x76AA4E: a = GetYAngle + (GameFloatRand(pi/4) (0x287) - pi/8); d = GameFloatRand(20) (0x288) + 10; pos = me + GetPosFromAngle(a, d); InBounds -> SetupMoveToWithHug(pos, 0x82 130)
- `CheckHomelessMoveIntoAbode`: 0x761366..0x76136D: a town
- 0x761224 MakeHomelessNoStateChange; 0x76122B SetTopState(0x81 129); its result
- `MakeHomelessNoStateChange`: 0x761246: town = GetTown
- `MakeHomelessNoStateChange`: 0x76124D..0x761271: an abode -> RemoveAliveVillagerFromAbode, SetAbode(0), SetTown(town)
- `MakeHomelessNoStateChange`: 0x761276..0x76128F: no town -> 0; already in its list -> 0
- `MakeHomelessNoStateChange`: 0x761290..0x7612EF: out of the vagrants; 0x7612F9..0x761312: the head of +0x768, ++ +0x76C; 1
- `HomeDeleted`: 0x7611F3..0x7611FD: +0x60 == GetAbode -> +0x60 = 0.
- `CheckNeedNewAbode`: 0x757FC5..0x757FCE / 0x758060: no town -> VagrantStart; 1
- `CheckNeedNewAbode`: 0x758010..0x758036: found and MoveVillagerToAbode == 1 -> IsVillagerAvailable ? SetTopState(0x24 36); 1
- `CheckNeedNewAbode`: 0x75803E..0x758058: not in the town's homeless list -> MakeHomeless; 1
- `MoveVillagerToAbode`: 0x758086..0x7580B6: a child GetRoomLeftForChildren, an adult GetRoomLeftForAdults; > 0 (jg) -> Force; 1
- `ForceMoveVillagerToAbode`: 0x756248..0x75625C: my town and the abode's
- `ForceMoveVillagerToAbode`: 0x7562AA AddVillagerToAbode
- `ForceMoveVillagerToAbode`: 0x75625E..0x756265: my town's RemoveVillager
- `ForceMoveVillagerToAbode`: 0x75626A..0x756299: the abode's GetPercentAbodeFullWithChildren (vt +0x8A0) for a child, WithAdults (+0x89C) for an adult, below 1 (test ah, 1; jne) -> AddVillagerToAbode; else its town's AddVillagerToTown
- `GoHomeAndChange`: 0x761820..0x76184A: door = GetArrivePos; not AreWeThere(door, 0) -> SetupMoveToWithHug(door, 0xEA 234); 1
- `GoHomeAndChange`: 0x76185A..0x761877: SetTopState(inside ? 38 : 37)
- `GoHomeAndChange`: 0x76186D: SetTopState(0xA3 163)
- `GoHomeAndChange`: 0x76187D..0x7618A1: GetScale < 0.95 (test ah, 1) -> SetScaleForAge(GetAge)
- `ExitGoHomeAndChange`: 0x761988..0x761997: IsStateExitFunctionSameAs(next) (vt +0x96C) -> nothing
- `ExitGoHomeAndChange`: 0x76199B..0x7619B1: the town's tribe (+0x5B8) or, without one, the info's (+0x1F4)
- `ExitGoHomeAndChange`: 0x7619B7..0x7619D4: ChangeTribeIfRequired(tribe, Infos[next] 0xC0 == 0)
- `ExitGoHomeAndChange`: 0x7619D9..0x7619EA: discipleType (+0xF2) 10 CHANGE_HOUSE -> SetVillagerDisciple(0, 0, 0) 0x756000
- `ChangeTribeIfRequired`: 0x7618C8..0x7618D3: KeepMeshWhenChangeTown (+0x388) != 0 -> nothing
- `ChangeTribeIfRequired`: 0x7618D9..0x7618E5: GVillagerInfo::Find(tribe, +0x1FC my number) 0x752650
- `ChangeTribeIfRequired`: 0x7618EA..0x761940: with a town, +0x6F4 += new +0x2D8 - old +0x2D8 0x761942 ChangeInfo
- `ChangeTribeIfRequired`: 0x76194A..0x761964: leaving -> CreateSmokyStuff(1, 1.0 [0x3F800000], colour -1) 0x63A810: the object's point half its height up (🔍 inferred §3), SmokyStuff::Create(point, 1, 1.0, -1)
- `ChangeInfo`: 0x761A0E..0x761AD1: a child: the three meshes = +0x204 ChildMeshHigh (rareza: SetAge uses +0x20C / +0x208 / +0x204); an adult: GetDetailMesh(2 / 1 / 0) (vt +0x60C)
- `IsSexuallyActive`: 0x761099..0x7610BA: GetAge >= +0x228 (jb) and GetAge < +0x22C (jae)
- Villager::SetTown 0x756530: +0x12C = town
- Villager +0xE0 & 4: inside its home (ArriveHome 0x751FAC / LeaveHome 0x751FE0)
- Villager::IsVillagerAvailable 0x752290: !(+0x25 & 4) (script), IsAvailableForStateChange (!(+0x24 & 4), not in the hand) and GetVillagerAvailableState 0x751F40 (GetFinalState's row, file 0xA8) & 1
- Villager::DoGoingHome 0x760280 (arrive, tent): with an abode, inside -> 38; already on the way -> nothing; else the walk to its door with FINAL `arrive`. Without one: no town -> 130 VAGRANT_START; more than 100 m from the town -> a walk to 10..35 m from it on my side (FINAL the TOP); else a tent near me (FINAL `tent`) or a stroll (FINAL the TOP). Always 1
- Villager::CheckHomelessMoveIntoAbode 0x761360: a town and FindAbodeWithSpaceInTown(me, 0) -> out of the homeless list, AddVillagerToAbode, SetTopState(36); 1. Else 0
- Villager::MakeHomeless 0x761220: MakeHomelessNoStateChange and SetTopState(129); its result
- Villager::MakeHomelessNoStateChange 0x761240: out of its abode (SetAbode(0), SetTown(town)); no town -> 0; already in the list -> 0; out of the vagrants; at the head of the town's homeless list; 1
- `HomeDeleted`: an abode -> MakeHomeless;
- Villager::MoveVillagerToAbode 0x758080: room left (children / adults, signed) > 0 -> ForceMoveVillagerToAbode, 1; else 0
- Villager::ForceMoveVillagerToAbode 0x756240: the same town -> AddVillagerToAbode; else the old town's RemoveVillager and, below 100% full (children / adults), AddVillagerToAbode, else the new town's AddVillagerToTown
- Villager::FindPosOutsideAbode 0x753470 (abode;
- Villager::SetupBuildingObject 0x758530 (abode): villager::SetupBuildingObjectForBuilding in VillagerBuild.h  Villager::IsSexuallyActive 0x761090: StartHavingSexAge (+0x228) <= age < StopHavingSexAge (+0x22C)
- ageToScale (GVillagerInfo memory +0x2E8): index i is ageToScale[i]; i = -1 reads the dword before (+0x2E4, dancingSpeed) as a float (InitialiseScale 0x74FB90 at age 0)
- `InitialScaleForAge`: 0x74FB88..0x74FBAB: age < grownUpAge (jae) -> ageToScale[age - 1]; else 0.9
- `IsPregnant`: 0x752210: info +0x1F8 == 1 && +0xF8 != 0
- Villager::InitialiseScale 0x74FB80: a child (age < grownUpAge) ageToScale[age - 1] (+0x2E4 + 4 age: at age 0 the dword before the table, dancingSpeed, read as a float), an adult 0.9 (0x3F666666)
- Villager::IsPregnant 0x752210: info sex (+0x1F8) == 1 and +0xF8 != 0
- `FoodRequiredForMeal`: 0x75BC08..0x75BC1A: eat - (movsx) held; <= 0 -> 0
- GetPlayer (vt +0x1C, Villager::GetPlayer 0x7502F0: the town's owner, none without a town)
- GetAmountOfFoodRequiredForMeal 0x75BC00: max(eat - held, 0), signed (setle)
- Villager::GetAmountOfFoodRequiredForMeal 0x75BC00
- 33 GOTO_STORAGE_PIT_FOR_FOOD: Villager::GotoStoragePitForFood 0x769830. 1
- `TownOf`: Villager::GetTown 0x751F00 (vt +0x48): +0x12C
- `YAngleOf`: Object::GetYAngle 0x402500 (vt +0x508): +0x4C.
- `SetTown`: 0x756534
- `IsVillagerAvailable`: 0x752293: +0x25 & 4 (controlled by a script) -> 0
- `HomeDecideWhatToDo`: 0
- `CheckNeedsAtHome`: 1) == 1 -> 1
- `FindVillagerInfo`: 0x752659..0x75267F: _VillagerInfos 0xDA6BE8, stride 0x3A4: the first with +0x1F4 == tribe and +0x1FC == number
- The exported copy (VillagerHome.cpp's TownOf; VillagerEmergency uses it). VillagerBuild / VillagerFarmer / VillagerFisherman / VillagerForester still keep private copies (audit_v11_patch.md L-6: a separate cleanup)

#### The house and its inhabitants

- `Abode::IsBuilt` [0x4016C0]: !(+0x58 & 2) && GetPercentBuilt (+0x5C) ≥ 1.
- `Abode::IsFunctional` [0x406200] (vt +0xD4): `MultiMapFixed::IsFunctional` [0x52EF70] (IsAvailable, IsBuilt vt
  +0x890 and GetPercentRepairedForNonFunctional 0x407290 = info +0x1B8 thresholdForStopBeingFunctional <
  GetPercentRepaired 0x401500 = the life) and IsBuilt again [0x406211].
- `Abode::GetArrivePos` [0x401770] = `MultiMapFixed::GetDoorPos` [0x52E370] (vt +0x864): the world door × 6553.6 with
  ftol (0x63AFF2..0x63B017, and 0); with no door point, or with x == 0 or z == 0 (0x52E3A4 / 0x52E3AC), the abode's
  position.
- The world door: `Game3DObject::GetDoorPosition(MapCoords)` 0x63AFE0 → (LHPoint) 0x63AFC0 → the LH3DObject's vt
  +0x1C4, 0x7F9FB0 in all three static object vtables (0x9A2974, 0x9A3068, 0x9A32A0). It needs the mesh (+0x7C) flag
  0x800 and reads the first extra point (mesh +0x44); each axis is ((z F + y U) + x R) + T with the matrix's rows R
  (+0x14), U (+0x20), F (+0x2C) and translation T (+0x38), a float step each. The matrix is the one
  `MultiMapFixed::CallVirtualFunctionsForCreation` sets with `LH3DObject::SetPosition` 0x423140 (point, y angle,
  scale), the translation x / z being the map position × 10 / 65536. Checked with the oracle: the whole chain run
  natively on ten placements of Land 1's homes.
- `Abode::GetPosOutside`(p1, p2, p3) [0x4072E0]: door + GetPosFromAngle(Get3DAngleFromXZ(pos, door) + GameFloatRand(2π
  / p1) − π / p1, GameFloatRand(p3) + p2); the two draws in that order (Abode.cpp 0x94A and 0x94B).
- Inhabitant list +0xA0 (next = villager +0xE4) / +0xA4; AddVillagerToAbode inserts at the head [0x40415A].
- GetRoomLeftForAdults [0x404660] = info +0x174 − (u8) +0xB4; GetRoomLeftForChildren [0x404680] = info +0x178 − (u8)
  +0xB7; signed.
- `Abode::IsTooCrowded` [0x4046C0]: MaxVillagers 0 → 1; otherwise adults / MaxVillagers ≥ percentTooCrowded (info
  +0x1A0).
- GetPercentAbodeFullWithAdults [0x407050] (vt +0x89C): adults / MaxVillagers (fidiv); MaxVillagers 0 → 1.
- GetPercentAbodeFullWithChildren [0x407090] (vt +0x8A0): children / MaxChildren with INTEGER division (`div`,
  0x4070B3): it only gives 0 or 1; MaxChildren 0 → 1.
- AddVillagerToAbode score [0x404B40]: f = min(count / max, 1) (adults +0x174 / +0xB4 or children +0x178 / +0xB7
  depending on IsChild); room = 1 − f; sex = empty list ? 1 : ((1 − same_sex / list_size) + 1) × 0.5 (the list
  includes children and the villager himself if already there); result (GetDistanceModifier(d, 500) + 1) × 0.5 × sex
  × room; max 0 → 0 [0x404B4F..0x404CA7]; 500 at 0x43FA0000 and 0.001 at [0x8AA3B0].
- `Villager::IsMaleVillager` [0x55CAC0] (vt +0x44C): info +0x1F8 == 0 with no child test; IsFemaleVillager
  [0x55CAE0]: == 1.
- AddVillagerToAbode [0x404060]: removes him from the town's homeless list (--+0x76C) [0x404079], from his old abode
  [0x4040DE..0x4040F8] and from the vagrants [0x4040FA..0x404151]; head and ++count; SetAbode [0x750DE0] (the
  villager's town = the abode's or 0); if the abode's town is not his, AddVillagerToTown and VillagerMoveIntoAbode;
  child ++ChildCount, adult MaleFemale[sex] if empty, ++AdultCount, AdultMaleCount += IsMale [0x40417C..0x404210].
- RemoveAliveVillagerFromAbode [0x404340]: counts (never below 0) [0x404360..0x4043B6], list [0x4043BC..0x404406],
  SetAbode(0), with a town `TownStats::VillagerMoveOutOfAbode` [0x7494C0].
- RemoveDeletedVillagerFromAbode [0x404220]: if MaleFemale[sex] is him, MaleFemale[0] and [1] to 0
  [0x404227..0x40424D]; with a town `Town::RemoveVillager` [0x73E210] [0x4042FC..0x404326].
- RemoveAllVillagersFromAbode [0x404560]: from the head, reads the next (+0xE4) before `Villager::HomeDeleted`
  [0x7611F0].
- `Abode::ChildToAdult` [0x404CC0]: --ChildCount if not 0, ++AdultCount, AdultMaleCount += IsMale; with a town
  `Town::ChildToAdult` [0x73AF50] → `TownStats::ChildToAdult` [0x749490] (with abode +0x4C − 1, +0x50 + 1; children −
  1, adults + 1).
- `Abode::Process`: the abode has to be without adults or children, built, not in a script
  (`GameThingWithPos::IsInScript` 0x402280: +0x24 & 0x200) and, with a town, the town not uninhabitable (+0x5F4)
  [0x404448..0x4044B4].
- `Abode::Process`: +0xB0 += 0.001 per processed turn; on reaching 1, +0x7C |= 0x40, ReduceLife(info +0x1B0
  emptyAbodeLifeReducer, no player), +0x7C &= ~0x40, +0xB0 = 0 [0x4044B6..0x404503]; the 0x40 bit only lives during
  the call (the statistic in FUN_004073F0 reads it).
- `Abode::Process`: the byte +0xB9 counts up to 200 (no reader found) [0x404503..0x40450F].

#### Moving villagers between houses (Shuffle, step 24)

- `Abode::CalculateDesireToGainMale` [0x4074A0]: MaxVillagers 0 or no town → 0; otherwise (men + 0.001) / (women +
  0.001) − (adultMales + 0.001) / ((adults − adultMales) + 0.001), with men / women = Town +0x664 / +0x668 = TownStats
  +0x54 / +0x58 [0x4074A6..0x407531].
- `Abode::CalculateDesireToGainVillager` [0x407540]: MaxVillagers 0 or no town → 0; otherwise (town adults +0x618 +
  0.001) / (adult places +0x644 + 0.001) − GetPercentAbodeFullWithAdults [0x407566..0x40759E].
- <a id="the-shuffles-adult-places"></a>Both town fields are read at the call, from the town (vt +0x48 GetTown,
  0x40755F): no copy is made at the town's turn. +0x644 is [the town's room](#the-towns-room)'s adult places
  (TownStats +0x34). Its only writers are `TownStats::SetToZero` 0x749219, `TownStats::Add(Abode*)` 0x749951 (with
  places) and `TownStats::Remove(Abode*)` 0x749A1A. Add's one caller is `Town::AddAbodeToTownStats` 0x739A20, from
  `Abode::MakeFunctional` 0x404824. Remove's is `RemoveAbodeFromTownStats` 0x739A40, from `RemoveStructureFromTown`
  0x739AA5. MakeFunctional (vt +0x914) is called from `Abode::Built` 0x404799, `Abode::Repaired` 0x4047C7,
  `Abode::Init` 0x403177 and `SpellDispenser::Create` 0x722929. No villager move writes +0x644:
  VillagerMoveIntoAbode, VillagerMoveOutOfAbode and ChildToAdult move only the free places. A scan of every store to
  [reg + 0x644] in the code finds no other writer of the town's field (only building sites' own +0x644 and a network
  routine). The shuffle is step 24 of `Town::Process` (0x7475E8), after the abodes' turns of step 4. So an abode
  finished, repaired or gone earlier in the same town turn is in the places the shuffle reads. Its callees
  (SwapMaleForFemaleFrom, TakeVillagerFrom) move villagers between houses, never an abode into or out of the town, so
  the places stay the same for the whole list.
- ShuffleVillagersAroundAbodes [0x741540]: entries = the functional structures of +0x754 with MaxVillagers +
  MaxChildren != 0, each one {abode, m = DesireToGainMale, v = 0.5 × DesireToGainVillager} [0x741549..0x74161E]; with
  no entries nothing [0x74158B].
- They are sorted with _qsort(list, n, 12, 0x7417C0); the comparator gives −1 if |b.m² + b.v²| < |a.m² + a.v²|,
  otherwise 1 (never 0: the largest first).
- Loop of pairs i = 0 .. n − 2; the first move that returns 1 ends the call [0x74163D..0x741798].
- For each a it looks for the next c with |(c.m + a.m)² + (c.v + a.v)²| strictly less than |a.m² + a.v²|
  [0x741685..0x7416B9].
- moreV = a.v > b.v; with opposite signs of v a swap is done unless the winner (moreV ? a : b) is below 100 % adults;
  moreM = a.m > b.m [0x7416C3..0x74173A].
- Swap: moreM ? a.SwapMaleForFemaleFrom(b) : b.SwapMaleForFemaleFrom(a); otherwise: moreV ? a.TakeVillagerFrom(b,
  moreM) : b.TakeVillagerFrom(a, !moreM) [0x74173E..0x741777].
- `Abode::SwapMaleForFemaleFrom`(y) [0x407620] on x: the first man in y's list and the first woman in x's who are not
  inside (+0xE0 & 4); ForceMoveVillagerToAbode(man → x) and (woman → y); 1, or 0 with no pair.
- `Abode::TakeVillagerFrom`(y, male) [0x4075B0] on x: the first of y of that sex who is not inside →
  ForceMoveVillagerToAbode(→ x) [0x407603]; 1, or 0.

#### Town, homeless and vagrants

- Homeless list +0x768 (next = villager +0xE4) / +0x76C, at the head (MakeHomelessNoStateChange
  0x7612F9..0x761312); `Town::IsVillagerInHomelessList` [0x73B580].
- The unlinking from the list is inline in AddVillagerToAbode 0x404085, CheckHomelessMoveIntoAbode 0x76137D and
  RemoveVillager 0x73E259.
- Global vagrants g_game +0x205BFC / +0x205C00, at the head and without duplicates (ReleaseFromScript
  0x7532F8..0x75331A / 0x7532A2..0x7532CA); `Villager::IsVagrant` [0x7531A0].
- Writers of the vagrant list: CREATE_VILLAGER / _POS with no abode or town (0x715DC0), ChildBorn 0x762396,
  TownDeleted 0x750B58 and ReleaseFromScript 0x75329C (and 0x7532F2); removed from it by AddVillagerToAbode 0x4040FA,
  MakeHomelessNoStateChange 0x761290 and DeleteDependancys 0x74FE4B. So the list does fill up. Its only in-game reader
  is the help query (`HelpSystem::GetHelpQueryOnGameThingWithPos`, IsVagrant at 0x5C9569); the others are save/load
  and ClearMap. So no game state depends on it.
- `Town::AddVillagerToTown` [0x73A090]: uninhabitable (+0x5F4) → 0 [0x73A095]; TownStats::Add and SetTown; abode of
  this town → done; of another → out of it and SetAbode(0), which also clears the villager's town (+0x12C = 0,
  0x750DE8, literal); FindAbodeWithSpaceInTown(v, 0) → AddVillagerToAbode and 1, without checking the worship site;
  otherwise MakeHomelessNoStateChange [0x73A105].
- FindAbodeWithSpaceInTown [0x73B370]: only functional abodes (vt +0xD4) and a score strictly greater than the best,
  which starts at the minimum passed in [0x73B371..0x73B3B6].
- `Town::RemoveVillager` [0x73E210]: FindChildrenAndOrphanThem [0x73E21C]; TownStats::Remove [0x73E231]; with an
  abode RemoveAliveVillagerFromAbode and SetAbode(0), otherwise out of the homeless; RemoveVillagerOnWayToWorshipSite
  [0x73E360, 0x73E29F]; SetTown(0) [0x73E2B7]; adults + children == 0 → +0xF20 = 50 [0x73E2BF..0x73E2CD]; mother
  (+0x100) = 0 [0x73E2D8].
- <a id="the-towns-room"></a>**The town's room** (TownStats at Town +0x610). The free adult places
  VillagerSpaceLeftInAbodes (+0x4C, Town +0x65C), the free child places (+0x50), the total space left (+0x30), the adult
  places MaxVillagersInAbodes (+0x34, Town +0x644) and the child places MaxChildrenInAbodes (+0x40, Town +0x650) are
  running counts, never rebuilt:
  - `TownStats::Add(Abode*)` 0x7498C0: +0x4C += the info's MaxVillagersInAbode (info +0x174), +0x50 += MaxChildren
    (+0x178), +0x30 += both; with places (both summed != 0, 0x749937) +0x10 ++, +0x34 += max villagers, +0x40 += max
    children. Its one caller is `Town::AddAbodeToTownStats` 0x739A20, from `Abode::MakeFunctional` 0x404824, once per
    abode (AddedToTownStats). `TownStats::Remove(Abode*)` 0x749990 is the inverse, from RemoveAbodeFromTownStats
    0x739A40 in RemoveStructureFromTown 0x739AA5 when the abode IsBuilt; Abode::Delete and ToBeDeleted run
    DeleteDependancys (RemoveAllVillagersFromAbode) first, so its villagers have moved out while it still had its town.
  - `TownStats::VillagerMoveIntoAbode` 0x749500 (an adult: +0x4C −1, a child: +0x50 −1; +0x30 −1 both), from
    AddVillagerToAbode 0x4041B6 with a town; `VillagerMoveOutOfAbode` 0x7494C0 (the same, +1), from
    RemoveDeletedVillagerFromAbode 0x404326 and RemoveAliveVillagerFromAbode 0x40442A with a town. The adult or child
    test is the villager's at the move (vt +0xAF8).
  - `TownStats::ChildToAdult` 0x749490: with an abode (GetAbode 0x752160) +0x4C −1 and +0x50 +1; always children −1,
    adults +1. From `Town::ChildToAdult` 0x73AF5B, called by Abode::ChildToAdult 0x404D1A and by CheckChildGrownUp
    0x7510C7 for a villager without a home. Oracle run on a zeroed stats block with +0x4C 7, +0x50 2, adults 5,
    children 3: with a home 6 / 3 / 6 / 2, without one 7 / 2 / 6 / 2.
  - So between the town's turns +0x4C is the max villagers of the abodes made functional less the adults housed in
    them, as it stands at that moment. Its readers: `Town::IsSpaceForNewVillager` 0x73FFD0 (+0x65C as a signed int
    > 0; +0x644 == 0 → the town info's DefaultVillagerCapacity (+0xF4) − (adults +0x618 + children +0x61C)), called by
    Villager::CheckMoveIntoTown 0x757228 and TownCentre::GetDiscipleStateIfInteractedWith 0x74491D;
    `Town::GetDesireToBeBuilt` 0x73A1A0, the living quarters case (0x73A2EF..0x73A301: +0x65C − the homeless +0x76C,
    and the population); `SetupBreederDisciple` 0x769EE0 (children +0x61C − child places +0x650 at 0x769F1C, adults
    +0x618 − adult places +0x644 at 0x769F46); `PopulateTown` 0x739D43 (a count of 0 means +0x65C; no direct caller).
- `Town::UseFood`: +0x6F8 += n (unsigned qword fild) and the owner's GameStats +0xA44 +0xA4 += n
  [0x73B5EA..0x73B60F].
- `SetAbode`: 0x750DEA: +0x128 = abode; 0x750DF0 SetTown(0); 0x750DF5..0x750E03: with an abode SetTown(abode.GetTown)
- Villager::SetAbode 0x750DE0: +0x128 = abode; SetTown(0); with an abode SetTown(abode.GetTown)

### Pregnancy and births

- How a woman gets pregnant: CheckWhenGoingToBed 0x760B60 runs once per stay (bit 0x2000) and returns 1 unless she
  dies of old age. With no town, raw Sleep < 1 or not sexually active, it returns 1 without anything else. Otherwise
  (0x760BDD..0x760C6D) CheckGetPregnantAtHome 0x760C80 runs for the partners in the home: a woman (IsFemale, vt
  +0x450, 0x760BCF) checks herself (0x760C17) when the first man (IsMale, vt +0x44C) of the abode's list is inside
  (+0xE0 & 4); a man (0x760C26) checks every woman of the list that is inside, in list order (0x760C5C). Either way it
  returns 1.
- CheckGetPregnantAtHome 0x760C80: WillHousewifeGetPregnant 0x7624C0 → HousewifeGetsPregnant 0x762570; 0 when she
  will not.
- WillHousewifeGetPregnant 0x7624C0 (the argument is not read): needs a home, not already pregnant, a town and
  IsSexuallyActive. s = the town's GetDesireSignificanceToVillager(8 FOR_CHILDREN) 0x746660; n = the home's
  ChildCount (+0xB7) + its pregnant inhabitants. True when s > 0 and MaxChildrenInAbode (abode info +0x178) > n
  (unsigned).
- HousewifeGetsPregnant 0x762570: +0xF8 = TimePregnantFor (info +0x240, u16); not at home → GoHome (its result). At
  home it returns the info pointer, never 0; the callers ignore it.
- WomanSpecial 0x752240: only for a pregnant woman not controlled by a script. +0xF8 −= GetGameTurnsSinceLastChecked
  (a 16-bit sub); still > 0 (signed) → 0; otherwise HousewifeStartsGivingBirth and its result.
- HousewifeStartsGivingBirth 0x7621A0: +0xF8 = 0; +0x58 = ftol(r + T × 0.25 + 1) with r = GameRand(ftol(T)), where T
  = GGameInfo +0x14 (0xD01A0C) = GGameInfo +0x10 (36000.0f, ctor 0x55775E) / 365.25 (fn_00557810) = 98.5626. So r =
  GameRand(98) and the counter is r + 25. Then SetTopState(111) and HousewifeGivingBirth. SET_TURNS_PER_YEAR (GSetup
  0x714DAA) also writes GGameInfo +0x10. No code sets 110 (no `push 0x6E` before a SetTopState): WomanSpecial calls
  the function itself 🔍 inferred.
- HousewifeGivingBirth 0x762430: −1 on +0x58 (u16); not yet 0 → 1. At 0: ChildBorn; if there is a child,
  SoundTag::Create(child pos, GetRandomSample(0x14, 0xA) 0x71ED40) 0x71EB60 (not tracked, mode 2, no loops, 3D, bank
  1 IN_GAME, no delay); SetTopState(112); 1.
- HousewifeGivenBirth 0x7624A0: +0xF8 = 0; jmp GoHome 0x760270.
- ChildBorn(mother) 0x762220 (cdecl): r = GameRand(100). When BoyGirlChance (info +0x25C) >= r, the child's info is
  `GVillagerInfo::Find`(the tribe type, GameRand(6) + 1), villager numbers 1 FORESTER .. 6 TRADER (null → the
  mother's); otherwise the mother's info. Villager::Create(mother pos, info, age 1, mother.IsSkeleton). Then the
  mother's home (AddVillagerToAbode 0x404060), else her town (AddVillagerToTown 0x73A090), else the head of the
  vagrants (g_game +0x205BFC, ++ +0x205C00) unless the child is there already. The child's +0x100 = the mother. The
  player's GameStats (+0xA44) +0x48 "Total Nacimientos" ++ (0x7623CD..0x7623E5). A poisoned mother gives a poisoned
  child (vt +0x4A4 → vt +0x69C). Finally ChildDecideWhatToDo(child). In a multiplayer game with a player there is also
  a step on player +0xF8 (fn_0056F8D0, fn_00775720(1)) (0x7622D3..0x762354).
- The housewife's day (states 100..109) and the meal call (ShallIWaitForDinner 0x7611B0, HousewifeAskForMeal
  0x762600, HousewifeCalledToMakeDinner 0x7625A0) are dead code in the game.
- GGameInfo +0xC (0xD01A04): turns in a year, ftol(1500.0f) from 0x8DF8D8 in GGameInfo::GGameInfo 0x557730
- HousewifeGivingBirth 0x762450..0x762462: GetRandomSample(0x14, 0xA) 0x71ED40, SoundTag::Create(.., 0, 2, 0, 0, 1, 1, 0) 0x71EB60: not tracked, mode 2, no loops, flag 0x10 off, 3D, bank 1 (IN_GAME), no delay
- `WillHousewifeGetPregnant`: 0x7624C6: GetAbode first; 0x7624CF..0x7624F4: IsPregnant -> 0; no town (vt +0x48) -> 0; no abode -> 0; !IsSexuallyActive 0x761090 -> 0
- `WillHousewifeGetPregnant`: 0x7624F6..0x762509: s = (town +0x34) GetDesireSignificanceToVillager(8 FOR_CHILDREN) 0x746660
- `WillHousewifeGetPregnant`: 0x76250D..0x762534: n = abode +0xB7 (ChildCount, a byte) + the inhabitants (+0xA0, next +0xE4) that IsPregnant
- `WillHousewifeGetPregnant`: 0x762536..0x762550: s <= 0 (fcomp 0, test ah 0x41) -> 0; abode info +0x178 MaxChildrenInAbode <= n (jbe) -> 0
- `HousewifeGetsPregnant`: 0x762570..0x762581: +0xF8 = info +0x240 TimePregnantFor (mov dx, word)
- `HousewifeGetsPregnant`: 0x76257A / 0x762588: not at home (+0xE0 & 4) -> GoHome 0x760270 (its result)
- `HousewifeGetsPregnant`: ⚠️ approximate the exe returns eax = the info pointer here (0x762570), never 0; its callers ignore it
- `HousewifeStartsGivingBirth`: 0x762206..0x762208: HousewifeGivingBirth (its result)
- `HousewifeGivingBirth`: 0x762433..0x76243E: --+0x58 (dec word); not 0 -> 1
- `HousewifeGivingBirth`: 0x762442: ChildBorn(*this)
- `HousewifeGivingBirth`: 0x76244C..0x762474: a child -> SoundTag::Create(child +0x14, GetRandomSample(0x14, 0xA), 0, 2, 0, 0, 1, 1, 0)
- `HousewifeGivingBirth`: 0x762477..0x76247D: SetTopState(0x70 112 HOUSEWIFE_GIVEN_BIRTH); 1
- `HousewifeGivenBirth`: 0x7624A0: +0xF8 (word) = 0
- `HousewifeGivenBirth`: 0x7624A9: jmp GoHome 0x760270
- `ChildBorn`: 0x76227D..0x7622AD: GVillagerInfo::Find(GetTribe +0x10, GameRand(6) + 1 (0x137)) 0x752650; null -> the mother's (+0x28). GetTribe 0x751EE0 = GGame::GetTribe(info +0x1F4), whose +0x10 is the tribe type (bw1-decomp TribeInfo.h:17 `type /* 0x10 */`)
- Villager::WillHousewifeGetPregnant 0x7624C0 (the argument is not read, ret 4): an abode, not pregnant, a town and IsSexuallyActive; s = the town's GetDesireSignificanceToVillager(8 FOR_CHILDREN) 0x746660; n = the abode's ChildCount (+0xB7) + its pregnant inhabitants (+0xA0, IsPregnant). s > 0 (test ah 0x41) and MaxChildrenInAbode (abode info +0x178) > n (unsigned, jbe) -> true
- Villager::HousewifeGetsPregnant 0x762570 (the argument is not read, ret 4): +0xF8 = TimePregnantFor (info +0x240, u16); not at home (+0xE0 & 4) -> GoHome (its result). ⚠️ approximate at home 1: the exe returns the info pointer
- Villager::HousewifeStartsGivingBirth 0x7621A0 (WomanSpecial 0x752276, and the state 110's function): +0xF8 = 0; +0x58 = BirthCounter(GameRand(98)) (VillagerHousewife.cpp 0x120); SetTopState(111); HousewifeGivingBirth (its result)
- Villager::HousewifeGivingBirth 0x762430: --+0x58 (u16) != 0 -> 1. At 0: ChildBorn; a child -> SoundTag::Create(its +0x14, GetRandomSample(0x14, 0xA)) 0x71EB60; SetTopState(112) (0x762479); 1
- State 112 HOUSEWIFE_GIVEN_BIRTH: Villager::HousewifeGivenBirth 0x7624A0: +0xF8 (u16) = 0; jmp GoHome 0x760270
- `CheckWhenGoingToBed`: 0x760BDD..0x760C1F: the first man of the abode's list that is inside -> CheckGetPregnantAtHome (hers); 1
- `CheckWhenGoingToBed`: 0x760C26..0x760C6D: each woman of the list that is inside -> her CheckGetPregnantAtHome
- `CheckGetPregnantAtHome`: 0x760C83..0x760C8C: WillHousewifeGetPregnant(0) 0x7624C0 == 0 -> 0
- `CheckGetPregnantAtHome`: 0x760C8E..0x760C92: HousewifeGetsPregnant(0) 0x762570 (+0xF8 = TimePregnantFor, GoHome if outside), its result
- Villager::CheckGetPregnantAtHome 0x760C80: WillHousewifeGetPregnant(0) 0x7624C0 -> HousewifeGetsPregnant(0) 0x762570 (VillagerBirth.h); the last call's result, 0 when she will not

### Love outside the home and the breeder

Out of doors two villagers of an age and of opposite sexes make love only through a breeder disciple: the three
callers of `GoAndHaveSexWith` 0x7672C0 are StartHavingSex 0x760EAF (meeting again), ReactToBreeder 0x7672A4 and
BreederJustLanded 0x76B8EB. (CHECK_INTERACT_WITH_VILLAGER 0x757760 also sends a pair to 122, through
MakeVillagesMeet itself, 0x7577E3; that state is not ported yet.)

- **Meeting** 🔍 read: `MakeVillagesMeet`(other, state, d) 0x752690: IsTouching(other, d) (vt +0x6B8: the gap between
  the two edges at most d) → both MakeVillagerFaceObject 0x752760 (Get3DAngleFromXZ, ConvertAngle3DToGame,
  SetGameAngle), SetTopState(state) on the first only, 1; the target things are not touched. Else
  SetupMoveToPos(GetMidPoint(me, other, d), state) for me, then the same with the two swapped for the other; both
  TargetThing (+0x118) set to each other; 0.
- `GUtils::GetMidPoint`(a, b, d) 0x74ECC0: p = a; m = GetDistanceInMetres(a, b); m != 0 → p += GetPosFromAngle(
  Get3DAngleFromXZ(a, b), (m − d) × 0.5) (float steps). So each walker stops a gap short of halfway.
- `GoAndHaveSexWith`(mate) 0x7672C0: MakeVillagesMeet(mate, 122 START_HAVING_SEX, 0.562); then TargetThing both ways.
- **122 START_HAVING_SEX** 0x760E60: the target (the mate) and IsAvailable → mate->IsTouching(me, 0.57): both face, then
  StartHavingSex(mate, 123) == 1 → 1; not touching → GoAndHaveSexWith(mate), 1. Otherwise SetTopState(163), 1.
- StartHavingSex(mate, state) 0x760F90: mate not available → 0. t = (mate's HavingSexTime + 2 × own) / 2 (unsigned);
  the mate first: SetTopState(state), its TargetThing = me, +0x58 = t + 1; then me the same; 1.
- **123 HAVING_SEX** 0x760EE0: IsReadyForNewAnimation(1) → a woman (info sex) of an age draws GameFloatRand(1) <
  PregnancyChance → HousewifeGetsPregnant (pregnant already or not); then SetTopState(124). 1.
- **124 STOP_HAVING_SEX** 0x760F50: inside (+0xE0 & 4) → GoHome; else SetTopState(163). 1.
- 125-127 (at home): their state functions only return 1 (0x760F80, 0x761010, 0x761020); exit ExitAtHome. Nothing
  sets them.
- EnterSex 0x76ACE0 returns 1. **ExitSex** 0x76AD80 (122-124) always returns 1: IsStateExitFunctionSameAs(next)
  (vt +0x96C) → at once. Else TargetThing = 0; the old partner, when there was one and its raw TOP (+0x8C) is my raw
  TOP: its TargetThing = 0 and SetTopState(124) on it (0x76ADB4..0x76ADCC).
- **SexValidate** 0x756A50 (the validate of 122 and, from 1.20 on, of 225): a TargetThing no longer available → 0.
- **The hand** (Villager::InterfaceSetInMagicHand 0x753080): +0xE0 |= 0x20; the raw FINAL (+0x8D) == 123 and a
  target → the target's TargetThing = 0, its DecideWhatToDo (vt +0x8C8), mine = 0. Then, its own player's villager of
  an age: CreateReaction(me, 32, player, 0). Then the Living's.
- **The breeder's job** 🔍 read: `SetupBreederDisciple` 0x769EE0 (DiscipleDecideWhatToDo case 5). Pregnant →
  SetTopState(36), 1. No town → 0. (children − child places) < AmountOverCapacityThatDiscipleBreedersBreedUntil
  (town info +0x14C) and (adults − adult places) < 4 × it (both as signed ints made floats; Town +0x61C − +0x650,
  +0x618 − +0x644), else 0. The nearest structure of the town's list (+0x754, next +0x9C) strictly within 300 m
  (GetDistanceInMetres). None → spot = GetResourceDropoffPos(FOOD) + GetPosFromAngle(GameFloatRand(2π),
  GameFloatRand(2) + 3) (the distance drawn first, 0x76A002), SetupMoveToPos(spot, 225). One → spot = its
  GetArrivePos (vt +0x104) + GetPosFromAngle(Get3DAngleFromXZ(it, its door) + GameFloatRand(π / 4) × (GameRand(2) ?
  1 : −1), GameFloatRand(R / 2) + R / 2) with R its Get2DRadius (vt +0x64; draws in the order GameRand(2),
  GameFloatRand(π / 4), GameFloatRand(R / 2)), SetupMoveToOnFootpath(it, spot, 225). Then TargetThing = 0, the wander
  area (+0x11C) = spot; 1.
- **225 BREEDER_DISCIPLE** 0x76A1B0: pregnant, or Sleep (16) first in the town's order 1 → SetTopState(36). Else the
  counter (+0x58, s16) − 1, and while it was > 0 → 1; else SetupWander(wander area, 225). 1. The town is read without a
  test (a breeder with no town would crash).
- SetupWander 0x76ACF0: p = the wander area; AddDistanceFromAngle(p, GameFloatRand(2π), GameFloatRand(8)) (the angle
  drawn first); SetupMoveToWithHug(p, state); then +0x58 = GameRand(40) + 30.
- EnterBreeder 0x76A2A0: unless the map is being cleared, CreateReaction(me, 30 REACT_TO_BREEDER, player, 1); 1.
  ExitBreeder 0x76A2D0: RemoveAllReactionsOfTypeInitiatedByObject(me, 30); 1.
- **Reaction 30**: SetupReactToBreeder 0x7671E0: AddReaction(r, 227), ReactionObject = the breeder.
  ReactToBreederPriority 0x767210: the initiator a villager (vt +0x2C8), not me, I am of an age (IsSexuallyActive), of
  the other sex (info +0x1F8), the reaction has **no follower yet** (its followers' list size, +0x1C, is 0, 0x767254)
  and I am not under a script (+0x25 & 4) → the table's priority (ReactionInfos[30]); else 0. 227 REACT_TO_BREEDER
  0x767280: the object a villager → GoAndHaveSexWith(it), 1; else 0. Its row: exit ExitReaction, validate
  ReactionValidate.
- **254 BREEDER_JUST_LANDED** 0x76B800 (the breeder's landing state): the first villager (FindType(VILLAGER)) of each
  of 9 spiral cells from mine, of an age and of the other sex (IsFemaleVillager, vt +0x450), the nearest
  (MapCoords::GetDistance, strictly) → GoAndHaveSexWith(it); none → SetTopState(163). 1.
- The emergency column of 122-127, 225, 227 and 254 is AlwaysReactToTownEmergency.

### Children and the creche

- IsMotherAlive 0x757F40: the mother (+0x100) is set, IsAvailable, of the child's tribe (info +0x1F4), IsAMother
  0x751110 (info +0x1F8 == 1 FEMALE), and her status bit 0 (dead) is clear → 1; else 0.
- ChildGotoCreche 0x7579F0: the town's creche (+0x744) functional → SetupMoveToOnFootpath(creche, its door
  (`MultiMapFixed::GetDoorPos` 0x52E370, vt +0x864), 113) 0x5EDD20; 1. Else 0.
- 113 CHILD_AT_CRECHE (ChildAtCreche 0x757C90): CheckChild == 1 → 1; no town → 0; CheckNeededForTownDesire == 1 → 1;
  no creche → 0. By day (`LH3DSky::Time2SkyType`(GetVisualTime) 0x86A1B0 <= 1.2, i.e. not
  `GGameInfo::IsVisualNight` 0x5575E0) with a functional creche: SamplePlayAnimEffect 0x42A4B0 on the creche with the
  key {0, 0, 0x13, 0, 0x52} (editor.sad, GAudio +0x3B0) and the next promenade point (SetupMoveToPos(p, 113)); 1.
  Otherwise: a home whose first inhabitant (+0xA0) is at home → GoHome; no home and touching the creche (IsTouching
  0.001, vt +0x6B8, Object 0x637E00) → the next point; 0.
- GetNextDstPromemade(creche, index, from) 0x757A50: n = the creche mesh's extra metrics / 5 (signed). index = (k <<
  16) | p. At the door (`MapCoords::operator==` 0x605660): p = GameRand(n), k = 0. Elsewhere: p clamped to [0, n −
  1], ++k, and k > 9 → k = 0, p = GameRand(n). For k 5..9 the path is walked back (j = 9 − k). n == 0 → the door.
  Else the extra metric 5p + j in the world (LH3D vt +0x1CC), plus two jitters GameFloatRand(1) − 0.5 in x and z.
  Both jitters are drawn whenever n != 0, so the RNG order does not depend on finding the point.
- GetNextDstPromemade 0x757AF0..0x757B00: a path is 5 extra metrics (imul 0x66666667, sar 1: a signed / 5)
- ChildAtCreche 0x757CE1..0x757CFF: LH3DSky::Time2SkyType(GGameInfo::GetVisualTime) > the double 1.2, the inline GGameInfo::IsVisualNight 0x5575E0 (DayNightClock::IsVisualNight), through the villagers' world queries.
- The walk to the creche's next promenade point: GetNextDstPromemade(creche, this +0xC4, &out, this +0x14) 0x757A50, then Villager::SetupMoveToPos(out, 0x71 113) 0x763800
- `NextPromenadeStep`: 0x757A65..0x757A71: p = index & 0xFFFF, k = index >> 16 (sar)
- GameRand(n) (`push ebp`), kept only in [0, n), else 0 (0x757B35..0x757B3D, 0x757B79..0x757B81)
- `NextPromenadeStep`: 0x757B23..0x757B41: p = GameRand(n) (VillagerChild.cpp 0x6B); k = 0
- `NextPromenadeStep`: 0x757B43..0x757B4F: p < 0 -> 0; p >= n -> n - 1
- `NextPromenadeStep`: 0x757B52..0x757B57: ++k; negative (jns) -> 0
- `NextPromenadeStep`: 0x757B59..0x757B81: k > 9 -> k = 0, p = GameRand(n) (0x81)
- `NextPromenadeStep`: 0x757C2C..0x757C3C: k 5..9 -> j = 9 - k
- `NextPromenadeStep`: 0x757C1A..0x757C25 / 0x757C61..0x757C70: index = (k << 16) | p
- `IsMotherAlive`: 0x757F44..0x757F4C: no mother (+0x100) -> 0.
- `IsMotherAlive`: 0x757F4E..0x757F58: IsAvailable (vt +0x2C, Villager::IsAvailable 0x751D50) == 1
- `IsMotherAlive`: 0x757F5A..0x757F6C: the same tribe (info +0x1F4)
- `IsMotherAlive`: 0x757F6E..0x757F78: IsAMother 0x751110 (info +0x1F8 == 1 FEMALE) == 1
- `IsMotherAlive`: 0x757F7A..0x757F84: Living +0xB4 & 1 (dead) -> 0; else the 1 of IsAMother
- `ChildGotoCreche`: 0x757A1F..0x757A34: SetupMoveToOnFootpath(creche, creche GetDoorPos (vt +0x864 = MultiMapFixed::GetDoorPos 0x52E370, abode_queries::GetArrivePos), 0x71 113) 0x5EDD20; 1
- `GetNextDstPromemade`: 0x757A75..0x757B00: n = the mesh's (vt +0xF8) extra metric count (flag 0x100000 at +4, the count at the extra data's +4) / 5; no extra data -> 0
- `GetNextDstPromemade`: 0x757B02..0x757B1F: from == GetDoorPos (vt +0x864), MapCoords::operator== 0x605660 (x, z and the altitude). (approximate, P12) x and z only, from the Transform: the altitudes are 0 on the ground
- `GetNextDstPromemade`: 0x757B85..0x757B87: n == 0 -> out = GetDoorPos (0x757C41)
- `GetNextDstPromemade`: 0x757B8D..0x757BA4: the obj3d's vt +0x1CC(5p + j, &point) (Game3DObject::GetSpecialPos: worship::GetSpecialPoint)
- `GetNextDstPromemade`: 0x757BA4..0x757BFC: a = GameFloatRand(1) - 0.5, b = GameFloatRand(1) - 0.5 (VillagerChild.cpp 0x90 both), drawn whenever n != 0, the point found or not, so the RNG order does not depend on it
- `GetNextDstPromemade`: 0x757C00: MapCoords(LHPoint) 0x603160: x, z by ToFixed
- `ChildAtCreche`: 0x757E38..0x757E66: the next promenade point, SetupMoveToPos(out, 113); 1
- GetNextDstPromemade 0x757A50's index step over n paths (`atDoor`: the child stands on the creche's door, 0x757B1A MapCoords::operator==): at the door p = GameRand(n) (VillagerChild.cpp 0x6B), outside [0, n) -> 0, k = 0. Else p clamped to [0, n - 1]; ++k, negative -> 0; k > 9 -> k = 0, p = GameRand(n) (0x81), outside [0, n) -> 0
- Villager::IsMotherAlive 0x757F40: the mother (+0x100) is set, IsAvailable (vt +0x2C) == 1, of the child's tribe (info +0x1F4), IsAMother 0x751110 (info +0x1F8 FEMALE) and her Living +0xB4 bit 0 (dead) is clear -> 1; else 0
- GetNextDstPromemade(Creche*, long& index, MapCoords* out, const MapCoords& from) 0x757A50: n = the creche mesh's extra metrics / 5 (signed); NextPromenadeStep(index, n, from == the door); n == 0 -> the door; else the extra metric 5p + j in the world (LH3D vt +0x1CC) + (GameFloatRand(1) - 0.5 (0x90, the second draw), GameFloatRand(1) - 0.5 (the first)) in x and z. index = (k << 16) | p
- State 113 CHILD_AT_CRECHE: Villager::ChildAtCreche 0x757C90: CheckChild == 1 -> 1; no town -> 0; CheckNeededForTownDesire == 1 -> 1; no creche -> 0. Day (sky type <= 1.2) and a functional creche: SamplePlayAnimEffect(creche, {0, 0, 0x13, 0, 0x52}), the next promenade point (SetupMoveToPos(it, 113) 0x763800); 1. Else: an abode whose first inhabitant is at home -> GoHome; no abode and touching the creche (vt +0x6B8, 0.001) -> the next point; 0

### Disciples

- g_DiscipleInfos 0x99A1F8: 13 records of 0x1C bytes. +0x0 the start state, a byte (`Villager::Landed` 0x760845
  enters it; 0 = inspect); +0x4 != 0 creates Reaction 0x18 in 163 (types 1, 2, 3, 4, 6, 8); +0x8 IsGlowing, the 3D object's vt
  +0xCC argument (SetVillagerDisciple 0x756087 / ResolveLoad 0x75488F): vt +0xCC is fn_007F9A20 in all eight
  LH3DObject vtables, which sets the drawn object's glow flag 0x10000000 from it; IsGlowing is 1 for the disciples
  1-10 (FARMER..CHANGE_HOUSE), 0 for WORSHIP (11), FROM_VORTEX (12) and none (oracle peek 0x99A1F8); +0xC "held at the
  job", ignores needs (`cmp .., 1` at its 9 readers, e.g. CheckHungry 0x75BE3A, CheckEveryTime 0x75061E); +0x10
  fetches wood (FORESTER, BUILDER, CRAFTSMAN; IsInterestedInWoodObject 0x76504F); +0x14 the TOWN_DESIRE_INFO it
  serves, −1 none (FOOD 0, WOOD 1, 8, 9; fn_00741220 0x74123E); +0x18 moves into another town when dropped on its
  object (Landed 0x7607F0, CheckMoveIntoTown 0x757210).
- Types as the jump table orders them: 1 FARMER, 2 FORESTER, 3 FISHERMAN, 4 BUILDER, 5 BREEDER, 6 PROTECTION, 7
  MISSIONARY, 8 CRAFTSMAN, 9 TRADER, 10 CHANGE_HOUSE, 11 WORSHIP, 12 FROM_VORTEX.
- `Villager::SetVillagerDisciple`(thing, disciple, h) 0x756000 (ret 0xC; `thing` and `h` are not read): outside 0..12
  → 0, nothing done. With a town and a new type: TownStats (Town +0x610) DecrementNumOfDisciples(old) 0x749C80 /
  IncrementNumOfDisciples(new) 0x749C60 (the bytes +0xC8 + type; type 0 skipped). A disciple: +0xE0 = (+0xE0 &
  ~0x400) | 0x200, vt +0xCC(record +0x8), +0xF2 = type. Type 0: +0xE0 &= ~0x600, vt +0xCC(0), +0xF2 = 0. Returns 1.
- DiscipleDecideWhatToDo 0x751720 (the jump table 0x75193C on +0xF2 − 1; 0 and 13..255 go to the fallback). FARMER:
  `Town::FindBestField` 0x73E870; no town or no field → fallback. If the field's GetFieldActivity is 2 (harvest) and
  the food carried > minFoodToShowGraphic (+0x270, signed) → 31; else VillagerBecomesFarmer 0x759C00. FORESTER:
  CheckSatisfyWoodDesire. FISHERMAN: FishermanLookForWater 0x75B4C0. BUILDER: CheckNeededForBuilding 0x758340.
  BREEDER: SetupBreederDisciple 0x769EE0. PROTECTION: the fallback. MISSIONARY: 0 at once. CRAFTSMAN:
  CheckSatisfySupplyWorkshop 0x7593A0 != 0. TRADER: CheckTrader 0x769B80. CHANGE_HOUSE: SetVillagerDisciple(0)
  first, then CheckMoveHouse 0x757180(`GUtils::FindClosestAbode` 0x74DD70) == 1 → SetTopState(234), and with another
  player `GPlayer::AddToCondition`(0xD, 1) (0x64DA80: only a multiplayer game keeps the win conditions); 1.
  CheckMoveHouse 0x757180: an `Abode` (RTTI) below 100% full for the villager (a child vt +0xAF8 → +0x8A0
  GetPercentAbodeFullWithChildren, else +0x89C WithAdults, < 1.0) → `ForceMoveVillagerToAbode` 0x756240, 1; else its
  town (vt +0x48) → `CheckMoveIntoTown` 0x757210; no abode or no town → 0. WORSHIP: SetTopState(58); 1. FROM_VORTEX: SetTopState(record 12 +0x0 = 244
  SCRIPT_IN_CROWD), then SetVillagerDisciple(0); 1. Any job result other than 1 goes to the fallback.
- The fallback 0x7518E7: FINAL already 221 → 0. A disciple held at the job → SetDiscipleNothingToDo 0x754070 != 0 →
  1; else 0.
- SetDiscipleNothingToDo 0x754070: FindDisciplePrayerPos → +0x58 = 0, SetupMoveToWithHug(p, 221); 1. 0 when there is
  no point.
- FindDisciplePrayerPos 0x754180: no town → 0. No town centre (+0x9A4) → the town's position, 1. Else the centre +
  GetPosFromAngle(the angle from the centre to me + GameFloatRand(π/2) − π/4, GameFloatRand(4) + the centre's
  Get2DRadius); 1.
- 221 DiscipleNothingToDo 0x7540D0: the town's pulse (+0x5E8) → +0x58 = GameRand(10). Then −1 on +0x58; still > 0
  (s16) → 1. At 0 with the clip not done → +0x58 = 1. Otherwise DiscipleDecideWhatToDo == 0 → +0x58 = 300. Always 1.
  Its entry EnterDiscipleNothingToDo 0x754140: with a town centre, LookAtObject(centre, 2); always 1.
- A disciple that dies is cleared first: VillagerDead calls SetVillagerDisciple(0, 0, 0) (0x7507F3..0x750800).
- The glow and the symbol are drawn with the villager's own 3D object, every rendered frame, in both of its draws
  (fn_00812170 0x812727 and fn_00815A70 0x815EAA: IsGlowing, vt +0xC8, then fn_008241B0, the GlowVillager draw). The
  point is the object's drawn matrix: x = +0x38, y = +0x3C + scale (+0x44, fadd, stored as a float), z = +0x40; the
  size is the scale. So the glow follows the villager where it is drawn this frame (between turns, in the hand), not
  where its last turn left it. fn_008241B0: the glow sprite (one of 0x400, colour 0x80C18119, size 1.0 [0xC383D0] x
  the scale, at least 0.0001); when the object being drawn is GlowVillager::g_the_one (the disciple in or under the
  hand), the symbol sprite (made once, colour 0xFFFFFF00 at 0x82426E: alpha, red, green, blue, an opaque yellow; the
  glow's 0x80C18119 read the same way is its half-bright orange): frame g_index - 32, y = the glow point's y + 2.5 [0x8C581C] x the scale, size 16
  [0x8AB6E8] / the half screen width x the distance to the camera x 0.5, at least 0.0001.
- The script native SET_DISCIPLE (369, 3 arguments, none returned) is `GScript::SetDisciple` 0x6FEB90. It POPs three
  raw values: the first (the "with sound" bool) into edi, the second (the disciple type) into ebx, the third (the
  object) for GetScriptGameThing 0x70D220. No thing → ScriptErrorMessage "Object no longer valid!" (0xC0DA90),
  nothing else. vt +0x2C8(0) (IsVillager: Villager and SpecialVillager) == 0 → "Thing must be villager" (0xC0DAA8);
  it then calls vt +0x2C8 twice more and goes on only if the last answer is non-zero, so a non-villager gets the
  message and nothing else. A villager: SetVillagerDisciple(0, type, sound) 0x6FEC28, the type passed as it was
  popped, so the range test 0..12 is SetVillagerDisciple's own and the sound (its `h`) is never read. No sound, no
  help message, no state change follows.
- Villager::SetupBreederDisciple 0x769EE0.
- Villager::CheckTrader 0x769B80.
- The missionary's control: `Town::AddMissionary` 0x740FF7 puts a new `MissionaryControl` at the head of the town's
  list. Its constructor 0x756760 zeroes its position (`GameThingWithPos::SetToZero` 0x75677F; no SetPos) and only then
  makes its REACTION 39 (0x75679D, `CreateReaction` 0x6E3D70), which spreads at once from `Reaction::GetPos`, the
  standard one, the initiator's position (decomp `Reaction.cpp:515`): the first spread is from the map's origin.
  `MissionaryControl::Process` (decomp `Villager.cpp:3305`, the town's missionary step) sets its position to the
  missionary's only from its first turn on.
- `SetVillagerDisciple`: 0x756018..0x756054: a town (vt +0x48) and a disciple other than +0xF2 -> TownStats (Town +0x610) DecrementNumOfDisciples(old) 0x749C80, IncrementNumOfDisciples(new) 0x749C60 (the bytes +0xC8 + disciple, 0 skipped).
- `SetVillagerDisciple`: 0x756082..0x75608E: the 3D object (+0x40) vt +0xCC, edx = g_DiscipleInfos[disciple] +0x8 IsGlowing (0x99A200 + 0x1C d).
- `DiscipleDecideWhatToDo`: 0x7517B8..0x7517CB (FORESTER): CheckSatisfyWoodDesire 0x75F4A0 == 1 -> 1
- `DiscipleDecideWhatToDo`: 0x7517F4..0x751807 (BREEDER): SetupBreederDisciple 0x769EE0 == 1 -> 1.
- `DiscipleDecideWhatToDo`: 0x751820..0x751833 (TRADER): CheckTrader 0x769B80 == 1 -> 1.
- `DiscipleDecideWhatToDo`: 0x751834..0x751844 (CHANGE_HOUSE): p = GetPlayer (vt +0x1C); SetVillagerDisciple(this, 0, 0): no longer a disciple, whatever follows
- `DiscipleDecideWhatToDo`: 0x75189F..0x7518B3 (WORSHIP): SetTopState(0x3A 58 GOTO_WORSHIP_SITE_FOR_WORSHIP); 1
- `DiscipleDecideWhatToDo`: 0x7518B4..0x7518E6 (FROM_VORTEX): SetTopState(g_DiscipleInfos[12] +0x0 = 0xF4 244 SCRIPT_IN_CROWD), then SetVillagerDisciple(0, 0, 0); 1
- `SetDiscipleNothingToDo`: 0x754070..0x754098: p = {0, 0, 0}; FindDisciplePrayerPos(&p) == 0 -> 0
- `SetDiscipleNothingToDo`: 0x75409C..0x7540AE: +0x58 = 0, then SetupMoveToWithHug(p, 0xDD 221) 0x5F2890 (its result unused); 1
- `FindDisciplePrayerPos`: 0x754189..0x75418E: GetTown (vt +0x48); none -> 0 (0x75426A), out untouched
- `FindDisciplePrayerPos`: 0x754194..0x75419C: no town centre (+0x9A4) -> 0x754246: out = the town's +0x14; 1
- `FindDisciplePrayerPos`: 0x7541A2..0x7541B0: Get3DAngleFromXZ(centre +0x14, me +0x14) 0x74D270: from the centre towards me
- `FindDisciplePrayerPos`: 0x7541B4..0x7541E1: a = GameFloatRand(pi / 2) (0x3FC90FDB, Villager.cpp 0xC5B) - pi / 4 [0x8C6C9C] (0x3F490FDB) + that, kept on the x87 stack until the fstp at 0x7541E1: within +-45 degrees of my side. Float arithmetic is that exactly: the exe runs the FPU at 24-bit precision (fn_007DEE00, `and cw, 0xFCFF` 0x7DEE0D), so the fsub and the fadd each round to a float mantissa already
- `FindDisciplePrayerPos`: 0x7541CE..0x754204: r = GameFloatRand(4) (0x40800000, 0xC5C), then + the centre's Get2DRadius (vt +0x64)
- `DiscipleNothingToDo`: 0x7540D3..0x7540F6: the town's +0x5E8 (Town::buildPulse, a building or resource pulse) -> +0x58 = GameRand(10) (Villager.cpp 0xC35).
- `DiscipleNothingToDo`: 0x7540FA..0x754105: --+0x58; (s16) still > 0 (`jg`) -> 1
- `DiscipleNothingToDo`: 0x754107..0x754112: IsReadyForNewAnimation(1) 0x5EC960 == 0 -> 0x75412C: +0x58 = 1 (wait for the clip); 1
- `EnterDiscipleNothingToDo`: 0x754143..0x75415B: a town and its centre (+0x9A4) -> 0x75416F LookAtObject(centre, 2) 0x5EC520 = LookAtPos(centre +0x14, 2) (the centre is not null); 1 (ret 8)
- `DiscipleFetchesWood`: +0x10 != 0 (IsInterestedInWoodObject 0x765056 `test eax, eax`): FORESTER, BUILDER and CRAFTSMAN fetch wood
- `DiscipleTownDesire`: +0x14 (fn_00741220 0x74123E): the town desire the disciple serves, -1 none (FOOD 0, WOOD 1, 8, 9)
- `DiscipleMovesIntoTown`: +0x18 != 0 (Villager::Landed 0x7607F6 `test eax, eax`): dropped on another town's object, it moves into that town
- Villager::FindDisciplePrayerPos 0x754180(MapCoords* out) (ret 4): no town -> 0; no town centre (+0x9A4) -> out = the town's +0x14; else out = centre +0x14 + GetPosFromAngle(Get3DAngleFromXZ(centre, me) + GameFloatRand(pi / 2) - pi / 4, GameFloatRand(4) + the centre's Get2DRadius). 1
- Villager::CheckHungry 0x75BCC0 (the periodic check's last one, CheckEveryTime 0x7505F3, and 129's)
- `DiscipleNothingToDo`: 1
- `SetDiscipleNothingToDo`: 1. 0 when there is no point
- `DiscipleNothingToDo`: --+0x58 (s16) > 0 -> 1; at 0 the clip not done -> +0x58 = 1; Always 1
- `EnterDiscipleNothingToDo`: Always 1

### Worship: return home

CheckVillagerGoBackToTownFromWorship 0x76BEC0 (worship code) returns the code of SetTopState(248) == 1
(0x76BF59..0x76BF6D), not "TOP == 248": if it pauses first (239 with FINAL 248) it also returns 1 and the villager
has already left. The worship site itself is on [The temple and the citadel](../magic/temple-and-citadel.md).

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `VillagerStates.cpp`, `VillagerChild.cpp`, `Abode.cpp`, `Town.cpp`; the rest
  read in the exe with the xref index and, for the door, the oracle.

## openblack

**Done** ✅ faithful for the home, food, sleep, homeless, age, pregnancy and births, children and the creche, and the
town's housing, and the disciples: `DiscipleDecideWhatToDo` with every job case (`Villager/VillagerDisciple.cpp`,
`VillagerBreeding.cpp`, `VillagerTrader.cpp`, `VillagerMissionary.cpp`), and their glow (`components::Disciple`).
The glow and the symbol of the disciple in or under the hand stand over the villager where it is drawn this frame
(`ecs::DrawnPosition`, after the villagers' drawn positions are set; `disciple::GlowPoint`, `SymbolPoint` in
`ECS/DiscipleRules`), as the original draws them with the villager's object (2026-10-20). The symbol is drawn in the
original's yellow (`disciple::k_SymbolColour`, `TintOf`; it was white before, 2026-10-20).

- Code: `Villager/VillagerHome.{h,cpp}` (36/37/38, 119/120/121, 129, 130, 234, 238, the tent, the moves),
  `Villager/VillagerFood.{h,cpp}` (CheckHungry, amounts, 117/118/212, 33/34/35), `Villager/VillagerAge.{h,cpp}`
  (growing up, scale, old age, pregnancy), `Villager/VillagerBirth.cpp` (the births), `Villager/VillagerResources.{h,cpp}`
  (what it carries and takes: the food half of the carrying), `Town/AbodeVillagers.{h,cpp}` (the home's list,
  PresentAtHome, the score, Abode::Process, the Shuffle's moves), `Town/TownVillagers.{h,cpp}` (homeless, vagrants,
  AddVillagerToTown, FindAbodeWithSpaceInTown, UseFood, Shuffle); tests `test/test_villager_food.cpp`,
  `test_villager_home.cpp`, `test_villager_age.cpp`. These state functions stay in `Villager/VillagerHome` under our
  names, behind the living interface ([the conventions](../project/refactor-conventions.md)).
- Fields: `Abode::presentAtHome` (+0xB6), `Abode::inhabitants`, `maleFemale`, `adultCount` / `adultMaleCount` /
  `childCount`, `emptyTimer`; `Town::homelessVillagers`; `Town::foodUsed`; the disciple type `Villager::discipleType`. The town's CallDesireFunction before the
  home's food is `town_desire::CallDesireFunctionNow`.
- **APIs** for other systems: `villager::IsAtHome`, `IsReachable` (used by AnimalPredators instead of its test of
  states 13..18, and by HandPlacement.cpp: the hand never picks up a villager from inside its home, see
  [Picked up or thrown](villager-hand-and-death.md#picked-up-or-thrown)), `LeaveHome`;
  `abode_villagers::VillagersOf`, `PresentAtHome`, `RemoveAllVillagersFromAbode`; `town_villagers::Homeless`,
  `AddVillagerToTown`; `town_desire::CallDesireFunctionNow`.
- **The door**: `affine::PlacementMatrix`, pinned to the oracle in test_villager_routine (`villager_routine::DoorPoint`).
  Before, openblack added the door to the Transform's position, which is the script's metres and not the map position
  (up to one unit apart): about one door axis in four of Land 1's homes came out one unit off.
- **Love outside the home and the breeder** (ported from raffclar PR #301; its constants checked against the v1.42
  read above, 2026-10-21: the meeting distance 0.562, the kiss's gap 0.57, the breeder's reach of home 300; see
  [The original](#love-outside-the-home-and-the-breeder)): `Villager/VillagerBreeding.{h,cpp}` (122-127, ExitSex,
  SexValidate, the hand's LetPartnerGo, SetupBreederDisciple, 225 with EnterBreeder / ExitBreeder and SetupWander, 227,
  254, reaction 30's priority and set-up), the pure rules in `VillagerBreedingRules.{h,cpp}` (the kiss's turns, the
  over-full test, the spots, the wander), `villager::MakeVillagesMeet` / `MakeVillagerFaceObject`
  (`Villager/VillagerInteract`) and `gutils::GetMidPoint` (`Common/GUtilsDistance`). The partner is
  `Villager::targetThing` and the spot `Villager::wanderArea`, as in the original; reaction 30 is a villager slot type
  (`k_ReactToBreeder`, `VillagerReactions.cpp`), whose followers are counted through `LivingReaction`. Its pregnancy is
  the existing `HousewifeGetsPregnant`. EnterBreeder's map-clearing test has no counterpart: openblack's map change
  enters no states. (openblack, guard) a breeder with no town goes home (the original reads the null town). Tests:
  `test/test_villager_breeding.cpp` (the rules), `test_villager_reactions.cpp` (the exit, the validate, the hand, the
  priority, the job), `test_gutils_distance.cpp` (GetMidPoint). Debug window Disciples: "Make it a breeder", each
  breeder's state, spot, partner and pregnancy, and the villagers making love.
- **Script creation**: openblack applies `lhscriptx::Script::IntSlot` only to CREATE_VILLAGER
  (FeatureScriptCommands.cpp). openblack's former rule «the home within 1 m² of the script position» was removed.
- **🔍 inferred** in openblack: «in the hand» is `fire::traits::InHand`; IsTree is the Tree component.
- **⚠️ approximate**: the speed +0x5A comes from `WallHug::speed`; IsMoving = the WallHug's last step is not zero;
  TownStats is recomputed in each `Town::Process` with `males` / `females` (the original only keeps them up to date);
  AddVillagerToTown and `Town::RemoveVillager` update them at once as `TownStats::Add` / `Remove` do, and ChildToAdult
  the adults and children; the homes' influence goes in the influence code's hook, not in step 4;
  `Abode::ReduceLife` has no entry point (`Object::ReduceLife` of `ecs::life` is used); the types of openblack's
  signatures stand in for the exe's type strings for the integer slots.
- ✅ [The town's room](#the-towns-room) is read as it stands by its three readers (`town_villagers::IsSpaceForNewVillager`,
  `SetupBreederDisciple`, the house case of `plans::GetDesireToBeBuilt`): `town_stats::RoomOf` sums the max villagers
  less the adults housed, and the adult and child places, over the abodes made functional, which is what the original's
  running counts hold. Before, they read the copy of the last town turn, so a villager moving into or out of a home or
  growing up in one, and an abode made functional or leaving the town, went unseen by them until the town's next turn.
  Debug window Disciples, "Towns' room": each town's room now and at its turn. Not ported: the town centre's reader
  (GetDiscipleStateIfInteractedWith) and PopulateTown's.
- ✅ [The shuffle's adult places](#the-shuffles-adult-places) are the town's as they stand: `town_villagers::ShuffleEntries`
  reads `town_stats::RoomOf(town).adultPlaces` once for its list and gives it to
  `abode_villagers::CalculateDesireToGainVillager`. Reading it once gives the same as the original's read for each house,
  since no abode comes or goes while the list is made. Before, the desire read the town stats counted at the start of
  the town's turn, so an abode made functional or gone in steps 1 to 23 of the same turn (a site finished in the
  abodes' turns of step 4) was not in them. The town's adults were already kept at once. Debug window Disciples,
  "Towns' room": the adult places now and at the town's turn.
- **The return from worship**: before the fix, after a pause, ProcessInWorship carried on with a villager that was no
  longer at the site (again in the return queue, one more worshipper requested, extra chant damage and a stale entry
  at the front of the queue).
- ArrivesAtHomeWithFood (35, the housewife's) is ported (`VillagerFood.cpp`, its row in `LivingActionSystem.cpp`, exit
  ExitAtHome): with an abode, the food it carries goes into the abode.
- Tests of the births and of the disciple type: `test/test_villager_birth.cpp` (a man, here
  `VillagerNumber::Forester`, is not a mother, so CheckChild gives 0) and `test/test_villager_disciple.cpp`
  (`SetVillagerDisciple` with `VillagerDisciple::Farmer` sets the disciple bits and +0xF2 and returns 1, as
  0x75605D..0x75609B).
- ✅ SET_DISCIPLE (`src/CHLApi.cpp` `SetDisciple`) is the exe's: the "with sound" argument is popped and not read,
  a thing that is not there gives "Object no longer valid", a thing without the `Villager` component gives "Thing
  must be villager" and nothing else, and a villager goes through `ecs::villager::SetVillagerDisciple` with the type
  as the script gave it (out of range: nothing). Tests: `test/test_script_query_natives.cpp` (`SetDisciple*`). Its
  readout is the Disciples debug window's list.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Villager/VillagerAge.cpp`: `k_AdultAge`, `k_ChildScaleStep`, `k_AdultScaleRand`, `AgeToScaleAt`, `RescaleTurn`, `OldAgeRange`, `InitialScaleForAge`, `ScaleForAge`, `SetAgeAndScale`, `CheckChildGrownUp`, `ChildBecomesAdult`, `CheckDeathFromOldAge`, `IsPregnant`, `WomanSpecial`, `GrownUp`, `Villager::k_FlagChild`, `DeathReason::OldAge`.
- `src/ECS/Villager/VillagerAge.h`: `k_TurnsPerYear`, `AgeFromBirthTurn`, `BirthTurnForAge`, `GrownUp`, `RescaleTurn`, `OldAgeRange`, `OldAgeDies`, `InitialScaleForAge`, `ScaleForAge`, `InitialiseScale`, `IsPregnant`.
- `src/ECS/Villager/VillagerBirth.cpp`: `k_BirthSampleFirst`, `BirthCounterRange`, `BirthCounter`, `WillHousewifeGetPregnant`, `HousewifeGetsPregnant`, `HousewifeStartsGivingBirth`, `HousewifeGivingBirth`, `HousewifeGivenBirth`, `ChildBorn`, `TownDesireInfo::ForChildren`, `IsAtHome`, `VillagerStates::HousewifeGivingBirth`, `VillagerStates::HousewifeGivenBirth`.
- `src/ECS/Villager/VillagerBirth.cpp`, `ChildBorn`: The constructor's SetSkeleton(arg 4) 0x74FB0C (VillagerCore.cpp Construct: (pending) not called there).
- `src/ECS/Villager/VillagerBirth.h`: `BirthCounterRange`, `BirthCounter`, `WillHousewifeGetPregnant`, `HousewifeGetsPregnant`, `HousewifeStartsGivingBirth`, `HousewifeStartsGivingBirthState`, `HousewifeGivingBirth`, `HousewifeGivenBirth`.
- `src/ECS/Villager/VillagerChild.cpp`: `k_PointsPerPath`, `WalkTheCreche`, `NextPromenadeStep`, `IsMotherAlive`, `ChildGotoCreche`, `GetNextDstPromemade`, `ChildAtCreche`, `VisualNight`.
- `src/ECS/Villager/VillagerChild.cpp`, `VisualNight`: (guard) no game: day
- `src/ECS/Villager/VillagerChild.cpp`, `IsMotherAlive`: (guard) an entity that is no longer a villager: 0
- `src/ECS/Villager/VillagerChild.h`: `NextPromenadeStep`, `IsMotherAlive`, `GetNextDstPromemade`, `ChildAtCreche`.
- `src/ECS/Villager/VillagerDisciple.cpp`: `Fallback`, `DiscipleDecideWhatToDo`, `SetDiscipleNothingToDo`, `FindDisciplePrayerPos`, `DiscipleNothingToDo`, `EnterDiscipleNothingToDo`, `VillagerStates::DiscipleNothingToDo`, `SetupBreederDisciple`, `CheckTrader`, `VillagerDisciple::None`, `VillagerStates::GotoWorshipSiteForWorship`, `DiscipleStartState`.
- `src/ECS/Villager/VillagerDisciple.cpp`, `Fallback`: 0x751920..0x751933: SetDiscipleNothingToDo 0x754070 != 0 -> 1
- `src/ECS/Villager/VillagerDisciple.cpp`, `SetupBreederDisciple`: the breeder's job (`VillagerBreeding.cpp`)
- `src/ECS/Villager/VillagerDisciple.cpp`, `CheckTrader`: the trader and its route (`VillagerTrader.cpp`)
- `src/ECS/Villager/VillagerDisciple.cpp`, `SetVillagerDisciple`: ⚠️ approximate a no-op: openblack recomputes TownStats::disciples at each Town::Process (TownStats.cpp, flags & 0x200 by +0xF2), and no reader of the counts between two Processes is known
- `src/ECS/Villager/VillagerDisciple.cpp`, `SetVillagerDisciple`: that LH3DObject call is the glow flag 0x10000000 from IsGlowing: nothing is drawn 0x756094..0x756098: GetPlayer (vt +0x1C), its result unused 0x75609B: +0xF2 = disciple
- `src/ECS/Villager/VillagerDisciple.cpp`, `DiscipleDecideWhatToDo`: every job case; a job result other than 1 goes to the fallback, as the exe; CHANGE_HOUSE moves into the closest abode of any town (`abode_queries::FindClosestAbode`, `villager::CheckMoveHouse` in `VillagerHome.cpp`) and then goes home to change, 234 (2026-10-19)
- `src/ECS/Villager/VillagerDisciple.cpp`, `DiscipleNothingToDo`: (pending) the exe reads GetTown's +0x5E8 without a null test; (guard) no town: no pulse
- `src/ECS/Villager/VillagerDisciple.cpp`, `DiscipleNothingToDo`: 0x754114..0x754125: DiscipleDecideWhatToDo 0x751720 == 0 (nothing found) -> +0x58 = 300 (0x12C);
- `src/ECS/Villager/VillagerDisciple.h`: `DiscipleFetchesWood`, `DiscipleTownDesire`, `DiscipleMovesIntoTown`, `FindDisciplePrayerPos`, `DiscipleStartState`, `DiscipleReaction18`, `SetDiscipleNothingToDo`, `DiscipleNothingToDo`, `EnterDiscipleNothingToDo`.
- `src/ECS/Villager/VillagerDisciple.h`, `DiscipleStartState`: +0x0 as the byte DiscipleDecideWhatToDo 0x7518C1 reads (`mov al, byte`): the state the disciple starts in
- `src/ECS/Villager/VillagerDisciple.h`, `DiscipleReaction18`: +0x4 != 0 (DecideWhatToDo 0x751642 `test eax, eax`): 1, 2, 3, 4, 6, 8
- `src/ECS/Villager/VillagerDisciple.h`, `SetDiscipleNothingToDo`: Villager::SetDiscipleNothingToDo 0x754070: FindDisciplePrayerPos -> +0x58 = 0, SetupMoveToWithHug(p, 221);
- `src/ECS/Villager/VillagerDisciple.h`, `DiscipleNothingToDo`: 221: Villager::DiscipleNothingToDo 0x7540D0: the town's pulse (+0x5E8) -> +0x58 = GameRand(10); else DiscipleDecideWhatToDo == 0 -> +0x58 = 300.
- `src/ECS/Villager/VillagerDisciple.h`, `EnterDiscipleNothingToDo`: 221's entry: Villager::EnterDiscipleNothingToDo 0x754140 (ret 8): a town with a centre -> LookAtObject(centre, 2) 0x5EC520.
- `src/ECS/Villager/VillagerFood.cpp`: `HungerBatch`, `FoodToEat`, `FoodRequiredForMeal`, `EatHeld`, `CheckHungry`, `GetAmountOfFoodToEat`, `CheckSatisfyOwnFoodDesire`, `ChangeStateToFindFoodToEat`, `EatFoodHeld`, `GetFoodFromHome`, `EatFood`, `EatFoodAtHome`, `ShowPoisoned`, `GotoStoragePitForFood`, `ArrivesAtStoragePitForFood`, `ArrivesAtHomeWithFood`, `DeathReason::Starving`, `GetAmountOfFoodRequiredForMeal`, `AbodeEntityOf`, `ResourceType::Food`, `ArrivesHome`.
- `src/ECS/Villager/VillagerFood.cpp`, `AbodeEntityOf`: (guard) GetAbode 0x752160 only reads +0x128: stands for Abode::ToBeDeleted's unlinking
- `src/ECS/Villager/VillagerFood.h`: `HungerBatch`, `FoodRequiredForMeal`, `CheckHungry`, `GetAmountOfFoodRequiredForMeal`, `CheckSatisfyOwnFoodDesire`, `EatFood`, `EatFoodAtHome`, `GotoStoragePitForFood`, `ArrivesAtStoragePitForFood`.
- `src/ECS/Villager/VillagerHome.cpp`: `k_FarFromTown`, `k_TreeRadius`, `k_TentCollide`, `k_TentTries`, `k_VagrantTownRadius`, `k_GrownUpScale`, `k_NeedsAtHome`, `k_DiscipleChangeHouse`, `TownOf`, `IsVillagerObject`, `CollideAt`, `SetAbode`, `SetTown`, `IsReachable`, `IsVillagerAvailable`, `ArriveHome`, `DoGoingHome`, `SetupMoveToOnFootpath`, `ArrivesHome`, `HomeDecideWhatToDo`, `CheckNeedsAtHome`, `HomeNothingToDo`, `ExitAtHome`, `GotoBedAtHome`, `SleepingAtHome`, `DoSleeping`, `CheckWhenGoingToBed`, `CheckGetPregnantAtHome`, `SleepInTent`, `GetTentPos`, `TentNextToTree`, `VagrantStart`, `CheckHomelessMoveIntoAbode`, `MakeHomeless`, `MakeHomelessNoStateChange`, `CheckNeedNewAbode`, `MoveVillagerToAbode`, `ForceMoveVillagerToAbode`, `GoHomeAndChange`, `ExitGoHomeAndChange`, `ChangeTribeIfRequired`, `ChangeInfo`, `FindVillagerInfo`, `IsSexuallyActive`, `GrownUp`, `VillagerStates::GoAndHideInNearbyBuilding`, `VillagerStates::SleepingAtHome`, `Villager::k_FlagGoingToBed`, `CheckDeathFromOldAge`, `WillHousewifeGetPregnant`, `VillagerStates::AtHome`, `VillagerStates::ArrivesHome`, `Locator::infoConstants::value`.
- `src/ECS/Villager/VillagerHome.cpp`, `k_DiscipleBreeder`: VillagerDisciple 5 BREEDER (HomeDecideWhatToDo 0x75FF1D `cmp cl, 5`)
- `src/ECS/Villager/VillagerHome.cpp`, `TownOf`: (guard) GetTown 0x751F00 only reads +0x12C: stands for the missing town unlinking
- `src/ECS/Villager/VillagerHome.cpp`, `IsTree`: 🔍 inferred the subclasses of Tree are openblack's Tree component;
- `src/ECS/Villager/VillagerHome.cpp`, `YAngleOf`: (aproximado) openblack's WallHug::yAngle, in radians
- `src/ECS/Villager/VillagerHome.cpp`, `DoGoingHome`: openblack's villagers dance only at the worship site , not in a DanceGroup that GO_HOME leaves 0x7602AD: GetAbode
- `src/ECS/Villager/VillagerHome.cpp`, `SetupMoveToOnFootpath`: the footpath walk or the direct walk, by `footpath_walk::ChooseWalkToObject` (see [Routes](routes.md))
- `src/ECS/Villager/VillagerHome.cpp`, `HomeDecideWhatToDo`: 0x75FF4D..0x75FF51: DecideWhatToDo (vt +0x8C8): its result
- `src/ECS/Villager/VillagerHome.cpp`, `HomeDecideWhatToDo`: 0x75FF67..0x75FF6E: HomeNothingToDo;
- `src/ECS/Villager/VillagerHome.cpp`, `CheckNeedsAtHome`: 0x76021E..0x760238: a child: CheckChildActivity 0x757F00 (= ChildDecideWhatToDo;
- `src/ECS/Villager/VillagerHome.cpp`, `HomeNothingToDo`: 0x75FFE8: SetupNothingToDo
- `src/ECS/Villager/VillagerHome.cpp`, `SleepInTent`: 0x761B0E..0x761B30: r = HomeDecideWhatToDo;
- `src/ECS/Villager/VillagerHome.cpp`, `HomelessStart`: else SetupNothingToDo
- `src/ECS/Villager/VillagerHome.cpp`, `HomeDeleted`: Living +0x60 is not identified in openblack 0x761206..0x761218: an abode -> MakeHomeless; else TownDeleted 0x750B50
- `src/ECS/Villager/VillagerHome.cpp`, `ChangeInfo`: 0x761A0B: +0x28 = the info (openblack keeps its row, `Villager::infoRow`, which VillagerInfoOf reads; see [Its own info row](villager-states.md#its-own-info-row))
- `src/ECS/Villager/VillagerHome.h`: `SetAbode`, `SetTown`, `IsAtHome`, `IsVillagerAvailable`, `CheckGetPregnantAtHome`, `CheckHomelessMoveIntoAbode`, `MakeHomeless`, `MakeHomelessNoStateChange`, `MoveVillagerToAbode`, `ForceMoveVillagerToAbode`, `IsSexuallyActive`, `DoGoingHome`, `HomeDeleted`.
- `src/ECS/Villager/VillagerHome.h`, `DoGoingHome`: IsDancing / RemoveFromDance (0x760289).
- `src/ECS/Villager/VillagerHome.h`, `HomeDeleted`: Villager::HomeDeleted 0x7611F0 (its abode is destroyed): +0x60 == the abode -> 0 (+0x60 is not identified); else TownDeleted 0x750B50
- `src/ECS/Villager/VillagerHome.h`, `FindPosOutsideAbode`: entt::null -> its own): door + GetPosFromAngle(Get3DAngleFromXZ(abode, door) + (pi/8 - GameFloatRand(pi/4)) (Villager.cpp 0xA97), GameFloatRand(1.5) + 1.5 (0xA96))
- `src/ECS/Villager/VillagerHome.h`, `GetTown`: Villager::GetTown 0x751F00 (vt +0x48): +0x12C, a valid town entity with its Town component, else entt::null.

### Test hooks

- `OPENBLACK_TEST_VILLAGER_AGE="<age>[,<n>]"`: on turn 2, only Living::SetAge (the birth turn), without meshes or bits
  (12.99 → 13 and old age). `OPENBLACK_TEST_HOMELESS=<n>`: on turn 2, MakeHomeless of villager n.
- `OPENBLACK_VILLAGER_TRACE` adds `home 36: …` (to the door / no abode → far / tent / wander / vagrant 130), `home 37:
  not there|arrive (present <n>)|tent|hungry 163+arrive|builds its home`, `home 38:
  emergency|needs(t=…)|disciple|something|nothing r4=<r>`, `exit-home <s> -> <next> (stay|leave, present <n>)`, `sleep
  120: life <l> -> keep|wake`, `tent: tree|spiral try|fail`, `food: …`, `eat: …`, `home-food: took <m> held <h>`,
  `age: grown|rescale|old age r= n= d= -> die|live`, `homeless: into abode|list`, `abode: moves|too crowded`, `vagrant
  130: …` and, every 100 turns, `home: town <id> inside <n> asleep <m> tents <k> homeless <h> vagrants <v>`.
  `OPENBLACK_TOWN_TRACE`: `shuffle: <abode> -> <abode> (swap|take …) = <r>`.

## Pending

- **Love outside the home**: the call a held villager of its own player makes to the others (reaction 32,
  WAIT_FOR_MATE 235) and CHECK_INTERACT_WITH_VILLAGER 179 (the other caller of MakeVillagesMeet) are their own port
  rows. The breeder's save and load are not ported (the game's saves are not).
- **Disciples**: HousewifeStartsGivingBirth makes no disciple call in the original (it calls only GameRand,
  its vt +0x8E8 SetTopState and HousewifeGivingBirth 0x762430), so nothing of it waits for the disciples.
- **In-game captures** of these states are a verification gap, not a difference.
- **Which script command the slot rule is applied to**: openblack applies the integer-slot rule only to
  CREATE_VILLAGER; confirm with the scripts that use 15 and 18.
- **GoHomeAndChange's order of state changes**: covered above (0x76185A..0x76186B); compare openblack's 234 against it
  in a run.
