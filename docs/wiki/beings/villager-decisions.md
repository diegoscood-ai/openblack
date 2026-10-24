# Villagers: deciding what to do

## Summary

When a villager has nothing to do it runs state 163 DECIDE_WHAT_TO_DO: the town emergency first, then its duties as a
disciple or a child, then what it is needed for (worship, the town's desires, its own hunger and tiredness), then
carrying resources to the store, and at last leisure: going home, chilling out by its house or in the town, sitting
down. The town keeps 17 desires (food, wood, homes, civic buildings, children, building, repairs, play, rest, sleep,
protection, mercy and others), recomputed every turn, sorted, and handed out to the villagers who check them.

openblack reproduces the decision, the leisure states and the 17 desire functions with their modifications and the
original's sort, bit for bit where marked ✅ faithful. The town's turn that drives the desires is on
[Towns](towns.md#townprocess-step-by-step).

## The original

### Deciding what to do and leisure

- **163 DECIDE_WHAT_TO_DO** (0x7515C0, returns 1): town emergency (`Town::IsInStateOfEmergency` 0x747970, Town +0xF1C,
  written by `SetInStateOfEmergency`, see
  [the town emergency](villager-jobs.md#repairs-the-tap-on-a-home-and-the-town-emergency)) → 242; disciple / follower
  (`DiscipleDecideWhatToDo`); `SetTopState(163)`; child → `ChildDecideWhatToDo` (`CheckChild`, town distribution,
  creche, → 114); `CheckNeededForSomething` (homeless: `CheckHomelessMoveIntoAbode` first → `CheckNeededForSpecial`:
  **worship**, civic (computes the trigger and clears `flags & 1`; the distribution in
  [Town desires and distribution](#town-desires-and-distribution)), own desires with threshold 0.3) →
  `CheckTakeResourcesToStoragePit` (→ 31) → `SetupNothingToDo`. Worship is not at the start of 163: it is in its place
  (0x760013), before the idle branch, and is also checked from 246.
- Desires: food = 1 − min(food, 1)³, life = 1 − ((life − min(0.3, life)) / 0.7)²; the largest first, strict
  comparisons with 0. `CheckSatisfySleep` 0x761490 does not look at the time of day (with a home → 36).
- **`SetupNothingToDo`** 0x753B50: GameRand(9), table 0x753C64 = 0,1,1,1,2,2,2,2,2. Branch 0: functional home → 36;
  otherwise GameRand(100) < 10 → 36, otherwise falls through to 1. Branch 1: with a home → 245; otherwise falls
  through to 2. Branch 2: with a town, walks to `GetChillOutPos` (meeting point + R..10R, ±22.5° on its side, R =
  0.1·GTownInfo +0x140) with FINAL 246; otherwise 36. It always returns 1.
- **209** returns 1 (only scripts set it). **245** `GoAndChilloutOutsideHome` 0x76B3F0 and **252**
  `GoAndChilloutInTown` 0x76B590 → `GetMeToMyChillOutPos` 0x76B610 (far: walks to GetPosOutside(3, R/2, R/2) of the
  door; near and clear (`CheckForClearArea` with 1.2·radius): `LookAtPos` one step and 246; occupied:
  FindClearArea(5, 1)). **246** `SitAndChillout` 0x76B4E0: entry 500 turns (+0x394), then a check every 101 calls
  (+0x396 = 100): emergency, `CheckNeededForSomething`, GameRand(10) == 0 → `SetupNothingToDo` without going through
  163. SitDown clip: the 0x800 bit before the current clip.
- **36 GO_HOME** = DoGoingHome(37, 238): with a home, walks to the door with FINAL 37 (37 makes it go in). Without a
  home: the tent or 130. The "hurt → 36" rule of `CheckEveryTime` is switched on in the game.
- **114 CHILD_FOLLOWS_MOTHER** 0x7578C0: `CheckChild`, distribution, creche; otherwise, walks to the mother (or to the
  home) + 5 m at a random angle (GameFloatRand(2π), bw1-decomp `VillagerChild.cpp` 0x39) if the point is navigable;
  without mother or home, `CheckNeedNewAbode` (0 for a child, so it stays in 114). Row 114 carries +0x50
  `AlwaysReactToTownEmergency` (0xD0D208 = 0x5AC990), like 36 and 209.
- Town: `GetCongregationPos` 0x7408B0 with a cache at Town +0xF10 (also written by SET_TOWN_CONGREGATION_POS); mean of
  the homes that are not fields (with < 3, plus the plans) and FindClearArea(130, 3, 10, `BlocksTownClearArea`);
  otherwise, base + 10..20 m. The list of homes (+0x754) goes from newest to oldest (`AddStructureToTown` inserts at
  the head, 0x7399C3..0x7399CF); that of plans (+0x9A8) from oldest to newest (`AddPlanned` appends at the end,
  0x73D08A..0x73D0AD). The height of the result is that of the last one read (0x7409C1..0x7409DB).
  SET_TOWN_CONGREGATION_POS: `GetScriptPos` 0x718250 → `MapCoords::Set` 0x603280 (x, z; height 0, or the third field
  unscaled if present, 0x6032E4); the offset 0xD99724 is null when loading a land (`LoadMapFeatures` 0x7180FE) and
  only the vortex sets it (fn_0076FA50).
- 163's disciple branch (0x7515F4..0x751680): for a disciple (0x200) or a disciple follower (0x400), a follower loses
  its disciple type (0x751618). If g_DiscipleInfos[type] +4 is set (types 1, 2, 3, 4, 6, 8), the villager creates
  Reaction 0x18 (Reaction::CreateReaction(this, 0x18, GetPlayer, 1) 0x6E3D70) and returns 1. A follower with
  nothing to do stops following (0x751666..0x751680).
- `SetupNothingToDo` always returns 1, so the SetTopState(36) after it in 163 (0x7516C4..0x7516D3) is dead code. A
  table entry above 8 would also give 36.
- `CheckNeededForSomething` 0x75FF80 (homeless: `CheckHomelessMoveIntoAbode` 0x761360 first);
  `CheckNeededForSpecial` 0x760010 (worship `CheckNeededForWorship` 0x76BA60, then civic, then own desires);
  `CheckNeededForCivic` 0x758180 = a town and fn_7581A0 == 1; `Villager::CheckNeededForTownDesire` 0x757C80 = jmp
  fn_7581A0. fn_7581A0 always clears flags & 1 when there is a town (0x7581CF).
- `Villager::GetOwnDesiresTrigger` 0x7581E0: 0 after a tap on the home (flags & 1). Otherwise f = IsHungry ?
  GetDesireForFood : 0; l = GetDesireForLife − ownDesireThreshold (info +0x38C), when that is positive, else 0; t =
  max(f, l) + 0.5 × min(f, l); a child gets at least 0.11 (0x8CA280); the result is min(t, 1).
- `GetDesireForLife` 0x75BBA0 = GetLifeDesireFromLife(life) 0x75BBE0.
- `Villager::CheckSatisfyOwnDesire` 0x760050(t): dF = food desire − t, dL = life desire − t. Food first when dF > dL
  and dF > 0 (`CheckSatisfyOwnFoodDesire` 0x75BF00 = IsHungry ? ChangeStateToFindFoodToEat : 0); otherwise life
  first, which is `CheckSatisfySleep` when dL > 0.
- `CheckSatisfySleep` 0x761490: after a tap (flags & 1) with life >= the threshold → 0. Inside its home:
  `CheckWhenGoingToBed` (see [Home, food, sleep](villager-life.md#home-food-sleep-homeless-and-age)), then 119. With
  a home → 36. With TOP 238 SLEEP_IN_TENT → 1. Else 0.
- `CheckTakeResourcesToStoragePit` 0x7516E0: the thresholds are info +0x26C (wood) and +0x270 (food), compared
  signed.
- `Villager::CheckChild` 0x757E80: not a child → GoHome; mother not alive → mother = 0; hungry → GoHome.
  `ChildDecideWhatToDo` 0x757EC0: `CheckChild`, `CheckNeededForTownDesire`, `ChildGotoCreche`, else 114; always 1.
- 114's step: 5 m = the constant 0x99A934 (10, read only here) × 0.5. The point must be navigable
  (`MapCoords::IsNavigable` 0x603840: Collide & 2 and not & 8).
- 245 `GoAndChilloutOutsideHome`: R = GTownInfo +0x144 (maxDistanceFromHouseThatPeopleChillOut). The look-at point is
  door + GetPosFromAngle(angle from the home to the door, 10 R), i.e. out of the door.
  `Villager::GetPosOutsideMyHouse` 0x753D50 = Abode::GetPosOutside(3, 0.5 R, 0.5 R); 0 without a town or a home.
- `Villager::GetChillOutPos` 0x753C70: angle GameFloatRand(π/4) − π/8 + the angle from the meeting point to me;
  distance GameFloatRand(9 R) + R.
- `GetMeToMyChillOutPos` 0x76B610: near means distance <= R. The clear-area radius is Get2DRadius × 1.2, tested with
  IsObject (FUN_00761BB0 → vt +0x460). Its "R + 5 < distance: step 5 m towards A" branch (0x76B6FD..0x76B741) is
  dead code, because it only runs when distance <= R.
- 246's entry `EnterSitAndChillOut` 0x76B570: +0x58 = initialChillOutTime (info +0x394).
- `SitDownAnimation` 0x424210: while an into / out-of clip plays (flag 0x800), the current clip decides (367 → 369,
  370 → 372); otherwise GameRand(2): 0 → 369, 1 → 372.
- `DecideWhatToDo`: 0x7515F4..0x751680: a disciple (0x200) or a disciple follower (0x400)
- `DecideWhatToDo`: 0x751618: a follower loses its disciple type
- `DecideWhatToDo`: 0x75162A..0x75165F: g_DiscipleInfos[type] +4 (0x99A1FC, DiscipleReaction18) -> Reaction::CreateReaction(this, 0x18, GetPlayer, 1) 0x6E3D70; 1 either way (0x7516D9). (pending) the reaction 0x18 is not made
- `DecideWhatToDo`: 0x751666..0x751680: a follower that has nothing to do stops following
- `DecideWhatToDo`: 0x751687: SetTopState(163) (vt +0x8E8; 163 does not pause, FINAL is cleared)
- `GoAndChilloutOutsideHome`: 0x76B3F8..0x76B410: no abode or no town -> SetTopState(163)
- `GoAndChilloutOutsideHome`: 0x76B484..0x76B4B0
- `GoAndChilloutInTown`: 0x76B599..0x76B5A2: no town -> SetTopState(163)
- `ChildFollowsMother`: 0x7578C6..0x757904: CheckChild, CheckNeededForTownDesire == 1, ChildGotoCreche -> 1
- `ChildFollowsMother`: 0x75790A..0x757965: the mother (+0x100) if available (vt +0x2C = Villager::IsAvailable 0x751D50, 0x757916), else the abode; neither: CheckNeedNewAbode
- `ChildFollowsMother`: 0x757969..0x7579AA: pos += GetPosFromAngle(GameFloatRand(2 pi) (VillagerChild.cpp 0x39), 0x99A934 (10, read only here) x 0.5)
- `ChildFollowsMother`: 0x7579AF MapCoords::IsNavigable 0x603840: Collide & 2 (dry land) and not & 8.
- `ChildFollowsMother`: 0x7579BC..0x7579C5: SetupMoveToWithHug(pos, 114)
- `CheckNeededForSomething`: 0x75FF83..0x75FF9C: homeless and CheckHomelessMoveIntoAbode -> 1
- `CheckNeededForSpecial`: 0x76002D..0x760043: CheckSatisfyOwnDesire(info +0x38C ownDesireThreshold) == 1
- `CheckNeededForCivic`: 0x758185..0x75819C: a town and fn_7581A0 == 1
- `CheckNeededForTownDesire`: fn_7581A0: no town -> 0 (0x7581DB)
- `CheckNeededForTownDesire`: 0x7581AF: the trigger
- `GetOwnDesiresTrigger`: 0x7581E6: flags & 1 (after a tap on its abode) -> 0
- `GetOwnDesiresTrigger`: 0x7581FC..0x758212: f = IsHungry ? GetDesireForFood : 0
- `GetOwnDesiresTrigger`: 0x75821A..0x75824E: l = GetDesireForLife - info +0x38C when > 0 (test ah, 0x41), else 0
- `GetOwnDesiresTrigger`: 0x758256..0x758295: max(f, l) + 0.5 x min(f, l)
- `GetOwnDesiresTrigger`: 0x75829B..0x7582BE: a child gets at least 0.11 (0x8CA280; 0x3DE147AE)
- `GetOwnDesiresTrigger`: 0x7582C7..0x7582E1: min(t, 1)
- `GetDesireForLife`: 0x75BBA0: GetLifeDesireFromLife(GetLife) (vt +0x11C)
- `GetLifeDesireFromLife`: 0x75BBE0..0x75BBF6: x = (life - m) / (1 - D); 1 - x^2
- `CheckSatisfyOwnDesire`: 0x760054..0x76006C: dF = GetDesireForFood - t, dL = GetDesireForLife - t
- `CheckSatisfyOwnDesire`: 0x760070..0x76007D: dF > dL (test ah, 0x41) and dF > 0: food first
- `CheckSatisfyOwnDesire`: 0x7600A5..0x7600B8
- `CheckSatisfyOwnDesire`: 0x7600C2..0x760102: (dF <= dL, or dF <= 0) life first
- `CheckSatisfySleep`: 0x761493..0x7614B4: flags & 1 and life >= D (fcomp, test ah, 1) -> 0
- `CheckSatisfySleep`: 0x7614B6..0x7614DB: inside its home (flags & 4): CheckWhenGoingToBed -> SetTopState(119 GOTO_BED_AT_HOME); 1
- `CheckSatisfySleep`: 0x7614DD..0x7614F7: an abode -> SetTopState(36 GO_HOME); 1
- `CheckSatisfySleep`: 0x7614F9..0x761507: TOP 238 SLEEP_IN_TENT -> 1
- `SetupNothingToDo`: 0x753B5A..0x753B74: GetTown, GetAbode, GameRand(9) (Villager.cpp 0xB46)
- `GetChillOutPos`: 0x753C79..0x753C80
- `GetPosOutsideMyHouse`: 0x753D58..0x753D5D: a town
- `GetPosOutsideMyHouse`: 0x753D5F..0x753D70: R' = info +0x144 x 0.5
- `GetPosOutsideMyHouse`: 0x753D74..0x753D7B: an abode
- `GetPosOutsideMyHouse`: 0x753D7D..0x753DAF: GetPosOutside(3, R', R')
- 246's entry: Villager::EnterSitAndChillOut 0x76B570: the counter (+0x58) = initialChillOutTime (+0x394); 1
- Villager::CheckNeededForSomething 0x75FF80: homeless -> CheckHomelessMoveIntoAbode 0x761360 (VillagerHome.h); then CheckNeededForSpecial == 1
- Villager::CheckNeededForSpecial 0x760010: worship (CheckNeededForWorship 0x76BA60), civic, own desires
- Villager::CheckNeededForCivic 0x758180: with a town, fn_7581A0 == 1
- fn_7581A0 (also Villager::CheckNeededForTownDesire 0x757C80, a jmp): GetOwnDesiresTrigger, the town desire's CheckVillagerNeededForTownDesire 0x745FF0 and flags &= ~1 (0x7581CF)
- Villager::GetOwnDesiresTrigger 0x7581E0
- Villager::GetDesireForLife 0x75BBA0 = GetLifeDesireFromLife(GetLife)
- Villager::CheckSatisfyOwnDesire 0x760050: the larger of food and life desire (minus the trigger) served first
- CheckSatisfyOwnFoodDesire 0x75BF00 and ChangeStateToFindFoodToEat 0x75B990 are in VillagerFood.h  Villager::CheckSatisfySleep 0x761490 (inside: CheckWhenGoingToBed 0x760B60, VillagerHome.h, then 119)
- Villager::CheckTakeResourcesToStoragePit 0x7516E0: wood (+0xF6) > minWoodToShowGraphic or food (+0xF4) > minFoodToShowGraphic (signed, jg) -> SetTopState(31); 1
- Villager::GetChillOutPos 0x753C70: around the town's congregation point, on my side (+-22.5 degrees), R to 10 R away (R = 0.1 x GTownInfo +0x140). 0 without a town
- Villager::GetPosOutsideMyHouse 0x753D50: Abode::GetPosOutside(3, 0.5 R, 0.5 R), R = GTownInfo +0x144. 0 without a town or an abode
- `DesireForChildren`: 0x748530..0x748561: al = GetPlayer ? 0.5 x GetAlignmentValue^3 (the loop of two fmul) : 0, stored
- `NothingToDo`: 0x760000
- `ChildDecideWhatToDo`: Always 1

#### Test hooks and checks

The leisure states were checked by running the game with the hooks of [Test hooks](#test-hooks) and the villager
trace. Land 1, 2532 turns, all villagers: 62 `SetupNothingToDo` (r = 0..8: 11, 3, 10, 8, 6, 4, 7, 5, 8) → 36 × 11,
245 × 18, 246 × 33; 245: 20 "far", 17 "near and clear", 11 "occupied" (by its home, see
[Assumptions](villager-states.md#assumptions), item 21); 246: 274 "again" and 26 "nothing"; a single warning of 37
ARRIVES_HOME; no 163/209/245/246 without function. Meeting point of towns 0 (1789.3, 2681.3) and 4 (2479.1, 2542.7)
by the mean. Land 2 with `OPENBLACK_TEST_WORSHIP="1,0.5"`, 3335 turns: r = 0..8 spread out (30..45 each), 1734
checks of 246 "again" and 185 "nothing", 10 towns with a meeting point, worship continues (11 worshippers in 59/60).

### Town desires and distribution

- **Order in the turn**: the towns are processed once per turn by `GPlayer::Process` 0x6494E0, player by player:
  towns, teleport travellers (fn_005FCC70, 0x6496BC) and alignment (0x6496C5). It runs after
  `InfluenceRing::ProcessRings` (0x54E63C), so `Town::Process` sees this turn's rings; then come the dances,
  GlobalGameLists, the forests and, at the end, Living (`GPlayer::ProcessPlayers` 0x54E641 goes before
  `Living::ProcessLiving` 0x54E65B).
- **Table** ✅ faithful: 0xDA32C8 + d·0x68, filled by crt_xc 0x744BD0: name, function (+0x10), Amount/Desired
  (+0x20/+0x30, only 5, 6, 7; only the trace 0x745EC0 reads them), the villager's CheckSatisfy (+0x40), modification
  (+0x50), children (+0x60: 2, 3, 4, 15, 16) and +0x64 (no reader). Per-desire info 0xDA2930 + d·0x90 (+0x18
  trigger, +0x58 TribeMultiplier[9]). Names for `TOWN_DESIRE_BOOST`: fn_747270 (`_stricmp`).
- **Functions** (faithful, x87 float steps, see [Assumptions](villager-states.md#assumptions)): Food =
  `Town::CalculateDesireForFood` 0x747F00 (thunk 0x747340): `1 − (food + 1e-4)/(5·Σ foodReqiredForDinner + 1e-4)`,
  with the warning `HelpSpritesLowOnFood(min(v,2) − 0.9)` (0x747FA0) if v ≥ 0.95 and the town belongs to the local
  player; Wood 0x747FF0 with S = min(R5+R6+R9+R12, 3), a = (craftsmen + 0.001)/(adults + 0.001) + S, **k =
  max(abodes/10, 1)**, B = 500k, C = 5000k and the LowOnWood warning (0x7481BC, min(v,2) − 1); Abodes 0x748210
  (max(a, c)⁴·(1 − R9)(1 − D6)); Civic 0x748330 (PopulationWhenNeeded of `GAbodeInfo::Find` 0x405B30, the first
  match); For_Children 0x748430 (the player's alignment and TribalPower[4]); To_Build 0x748640 and Repair_Town
  0x7486B0 (`Abode::GetDesireToBeRepaired` 0x406970 with the building's life); Playtime 0x7487B0 (0.1 if D0, D1, D5,
  D6, D9 < trigger and turn > 4000); Relaxation 0x7488C0 and Sleep 0x748960 (the sky type and the evening ramp over
  the visual time of day; Sleep reaches 6.25 at night). Protection 0x7488A0 / Mercy 0x7488B0 read Town +0xEC0 /
  +0xEBC, the sums of the attacks on the town (see [Town aggression](towns.md#town-aggression)); For_Wonder 0x748740;
  Supply_Worship, For_Rain, For_Sun, Suppy_Workshop = 0 (literal).
- **`TownDesire::Process`** 0x745AE0 ✅ faithful: +0x164, the 17 in order 0..16 (a desire that reads another of higher
  index sees the previous turn's), `CallDesireFunction` 0x745D80 (raw +0x168 = f·TribeMultiplier unclamped; desire
  +0x118 = clamp(raw·modification, −1, 1)), modifications 0x746490 / 0x7462A0 (150 = GVillagerInfo[10]
  maxFoodCarried) / 0x746350 (250) / 0x746400, the two orders and, every 50 turns, `(2R0 + R1 + max(R3,R4) +
  max(R5,R6))/5` with `HelpSpritesVillagerUnhappy` (0x745C8A) if it exceeds 0.6 and belongs to the local player. The
  player's statistic (GPlayer +0xA44) goes to the game statistics: the count and the sum of 1 − average.
- **Orders** ✅ faithful: order 1 (+0x278, value GetDesire, +0 = boosts A + script) and order 2 (+0x344, GetRawDesire,
  +0 = boost A) with the VC6 CRT `_qsort` 0x7C7E64 (CUTOFF 8, `_shortsort` 0x7C7FB8, pivot in the middle; it is not
  stable: with everything at 0 it ends up `8 1 2 3 4 5 6 7 0 9 … 16`).
- **Distribution** `CheckVillagerNeededForTownDesire` 0x745FF0 ✅ faithful: trigger 0 → 0.001; t = min(trigger + info
  +0x18, 1); the entries without CheckSatisfy and, for a child, those without +0x60 are skipped without cutting off;
  it cuts off (0) at the first eligible one with `TempMod(k)·valor ≤ t` and returns 1 when a CheckSatisfy gives 1.
  **Quirk**: `TempMod` is requested with the loop index k, not with the desire's. It is called by fn_7581A0.
- **CheckSatisfy**: Sleep as in [Deciding what to do and leisure](#deciding-what-to-do-and-leisure); Food and Wood in
  [Food and wood jobs](villager-jobs.md#food-and-wood-jobs-reactions-7-and-12), Abodes / Civic and To_Build in
  [Builders](villager-jobs.md#builders), Repair_Town in
  [Repairs](villager-jobs.md#repairs-the-tap-on-a-home-and-the-town-emergency); the others below.
- **`Town::Process`** 0x747380: the turn's TownStats, +0x5E4 = 0, `TownDesire::Process`, every 10 turns the
  worshippers needed and worshipping (fn_7489F0), the pulse +0x5E8/+0x5EC and the countdown +0xF20; the steps are in
  [Town::Process step by step](towns.md#townprocess-step-by-step). The influence (+0x5C8, steps 3-5: 0x7473A0 /
  0x7473AD / 0x7473BD) is computed there too.
- **Script** ✅ faithful: CHL `SET_TOWN_DESIRE_BOOST` (341) = `GScript::SetTownDesireBoost` 0x6FE650 (town, d < 17,
  −1 ≤ v ≤ 1 → +0xD4[d] = v and re-sorts only order 1; "Thing not valid!" / "Invalid Params"); CHL `GET_DESIRE`
  (234) = 0x6FCCA0 (invalid d → "Invalid desire" and 0 **without popping the object**; otherwise, GetRawDesire); the
  map command `TOWN_DESIRE_BOOST` 0x7179EC writes +0xD4 without re-sorting or checking the range (Land2.txt: "Abodes"
  / "Civic_Buildings" −0.75).
- **Readers**: `GetDesireSignificanceToVillager` 0x746660, `GetMostDesired` 0x745E50, `GetMostSignificantRawDesire`
  0x745EA0; order 2 (+0x344 = Town +0x37C value / +0x380 type) is what `CheckTownDesiresSFX` 0x71B130 reads; +0x410
  is read only by the alignment code.
- The villagers' other CheckSatisfy functions: Playtime = `CheckSatisfyPlaytimeDesire` 0x763130 (`xor eax, eax;
  ret`); Supply_Worship = `CheckSatisfySuppyWorship` 0x76CC00 → `GotoStoragePitForWorshipSupplies` 0x76BFA0;
  Supply_Workshop = `CheckSatisfySupplyWorkshop` 0x7593A0 (`Town::GetBestWorkshop` 0x740250); Relaxation =
  `CheckSatisfyRelaxation` 0x761460 = `Town::SetVillagerActivity` 0x73FF10, which takes the highest
  GetVillagerActivityDesire (vt +0x4C) among the football (+0xEA4), the player's creature (+0xA4C) and the town's
  artifacts (+0x994, next +0x20); best 0 → 0, else that object's vt +0x50. The Suppy_Workshop (sic) desire function
  is 0x748730; For_Rain 0x748690 and For_Sun 0x7486A0 have no CheckSatisfy; these all use `ModificationGeneral`
  0x746490.
- `CheckNeededForTownDesire`: 0x7581CA: town +0x34 TownDesire::CheckVillagerNeededForTownDesire(this, trigger) 0x745FF0, the jobs' share-out (ECS/TownDesire). Void in the PDB (QAEX), but it leaves 0 or 1 in eax (0x7460EB / 0x7460F7), which the caller compares with 1. At night Sleep (16) is first and CheckSatisfySleep sends the villagers with an abode to 36, which takes them in : 37 -> 38 -> CheckSatisfySleep inside -> 119 -> 120
- Villager::CheckSatisfyPlaytimeDesire 0x763130 (2): `xor eax, eax; ret`, literal 0
- Villager::CheckSatisfySuppyWorship 0x76CC00 (7) -> GotoStoragePitForWorshipSupplies 0x76BFA0.
- Villager::CheckSatisfySupplyWorkshop 0x7593A0 (13: Town::GetBestWorkshop 0x740250).
- Villager::CheckSatisfyRelaxation 0x761460 (15) = Town::SetVillagerActivity 0x73FF10(villager): the highest GetVillagerActivityDesire (vt +0x4C) of the football +0xEA4, the player's creature (+0xA4C) and the artifacts (+0x994, next +0x20); best 0 -> 0, else its vt +0x50.
- Football::GetVillagerActivityDesire 0x532220: a villager whose age (vt +0x8D0) is under 15 -> 0 (0x532232); both teams at 5 or more (+0x228, +0x230) with a game on (+0x220) -> 0; no game played yet (+0x1EC == 0) -> 1.0 ([0x8AA390], 0x53227C); else `GRand::GameFloatRand` of the whole rest periods since the last game, min(now - last, 0x708) / 0x708 (0x532297..0x5322CD), which is 0 under 1800 turns. The pitch is wanted at 1, not 1.5. openblack: `ecs::relaxation_rules::FootballDesire` (raffclar PR #294), unverified against v1.42.
- `DesiredSupplyWorship`: 2 Playtime 0x7487B0; CheckSatisfyPlaytimeDesire 0x763130; 0x746490; +0x60
- `DesiredSupplyWorship`: 7 Supply_Worship 0x748320; 0x747D70 / 0x747DD0; CheckSatisfySuppyWorship 0x76CC00; 0x746490
- `DesiredSupplyWorship`: 10 For_Rain 0x748690; 0x746490
- `DesiredSupplyWorship`: 11 For_Sun 0x7486A0; 0x746490
- `DesiredSupplyWorship`: 13 Suppy_Workshop (sic) 0x748730; CheckSatisfySupplyWorkshop 0x7593A0; 0x746490
- `DesiredSupplyWorship`: 15 Relaxation 0x7488C0; CheckSatisfyRelaxation 0x761460; 0x746490; +0x60
- `ShortSort`: 0x7C7FC2 / 0x7C7FFF (ja)
- `ShortSort`: 0x7C7FD6 (ja: p > hi ends)
- `ShortSort`: 0x7C7FE3 (jle)
- `DesireForFood`: 0x747F29..0x747F3E: v = 1 - a / (fn_747AF0 + 1e-4), stored (fst; at 24 bits (0x7DEE0D) the value the comparison below reads is the stored float)
- `DesireForFood`: 0x747F42..0x747FA0: v >= 0.95 (fcomp, test ah, 1), a player, the local one: HelpSpritesLowOnFood(min(v, 2) - 0.9) with the stored v
- `DesireForFood`: 0x747FA5..0x747FE5
- `DesireForWood`: 0x747FF7..0x74803A: R12 + R9 + R6 + R5, the raws then the boosts then the boosts A, in this order; < 3 or 3
- `DesireForWood`: 0x748040..0x74809D: fcomp 3; test ah, 1; je -> 3 (so unordered keeps the sum)
- `DesireForWood`: 0x7480BB..0x7480D3: a = (craftsmen (byte +0x6E0) + 0.001) / (adults (fild qword) + 0.001) + S, stored
- `DesireForWood`: 0x748104..0x748152: v = (1 - min(w / C, 1)) x (1 - min(w / B, 1) + a) (fcom 1; test ah, 1; jne keeps x < 1)
- `DesireForWood`: 0x74815A..0x7481BC: v >= 0.95, a player, the local one: fn_0071CAF0 (LowOnWood)(min(v, 2) - 1)
- `DesireForWood`: 0x7481C1..0x748201
- `DesireForPlaytime`: 0x7487B5..0x748870: D0, D1, D5, D6, D9 all strictly below their DesireTriggersVillagerAction (+0x18; fcomp, test ah, 1, je -> 0)
- `DesireForPlaytime`: 0x748872..0x748881: the game turn > 4000 (cmp 0xFA0; jbe -> 0)
- `DesireForAbodes`: 0x748216..0x74824B: a = adults (dword) / (adult places +0x644 (fild dword) + 1e-5), < 1.5 or 1.5, stored
- `DesireForAbodes`: 0x748253..0x7482B2: c = children / (child places +0x650 + 1e-5), min(c, 1.5); compared with a (fcomp, test ah, 0x41, jne: c <= a keeps a), else m = min(c, 1.5) stored
- `DesireForAbodes`: 0x7482B4..0x7482D9: m^4 x (1 - R9), stored
- `DesireForAbodes`: 0x7482DD..0x74831B: (1 - D6) x that, clamped to [0, 1]
- `DesireForCivicBuildings`: 0x748338..0x748345 (ebp)
- `DesireForCivicBuildings`: 0x7483F2..0x7483F6 (jb 0x10)
- `DesireForCivicBuildings`: 0x748353: the town has none of that abode number (+0x718[i] == 0)
- `DesireForCivicBuildings`: 0x748360..0x74837D: GAbodeInfo::Find(tribe +0x5B8, i) +0x1B4 PopulationWhenNeeded > -1 (jle)
- `DesireForCivicBuildings`: 0x74837F..0x7483BA: p == 0, or homeless (fild qword) / pop (fidiv dword) < 0.2 (fcomp; test ah, 1: also unordered, so pop 0 with no homeless passes)
- `DesireForCivicBuildings`: 0x7483BC: p <= pop (signed, jg)
- `DesireForCivicBuildings`: 0x7483C0..0x7483EE: s += 0.5 (pop - p + 0.001) / (p + 0.001) + 0.5, stored
- `DesireForCivicBuildings`: 0x7483FC..0x748422: min(s, 1) (fcomp 1; test ah, 1; je -> 1)
- `DesireForChildren`: 0x748436..0x748478: A = R0 >= 1 ? 0 : (R0 > 0 ? 1 - R0 : 1)
- `DesireForChildren`: 0x7484DB -> 0x74846C: 1 - 1
- `DesireForChildren`: 0x74846C
- `DesireForChildren`: 0x748478..0x748490: B = children < child places (unsigned, jb) ? 1 : 0
- `DesireForChildren`: 0x748498..0x7484AE: C = adults < adult places ? 1 : 0.5
- `DesireForChildren`: 0x7484B6..0x74852A: P = D3 > 0 ? D3 : 0 (stored), M = D4 > 0 ? D4 : 0; Q = (1 - M)(1 - P), stored
- `DesireForChildren`: 0x748569..0x748597: X = fn_73E5E0(4) (player ? player +0x68[4] : 1); v = X (1 + al) Q C B A, stored
- `DesireForChildren`: 0x74857C..0x7485B4: no creche (+0x744) or not IsFunctional (vt +0xD4) == 1 -> v x 0.5
- `DesireForChildren`: 0x7485B8..0x7485F5
- `DesireToBuild`: 0x74866A..0x748689: 1 < s -> 1
- `DesireForRain`: 0x748690
- `DesireForSun`: 0x7486A0
- `AbodeDesireToBeRepaired`: 0x406976..0x406991: GetPercentRepaired (vt +0x884) > town info +0x10C thresholdToStartRepairing (fcomp; test ah, 0x41; je) -> 0
- `AbodeDesireToBeRepaired`: 0x406993..0x4069A7: a home (AbodeType +0x120 & 2) with nobody (+0xA4 == 0) -> 0
- `DesireToRepair`: 0x7486B5..0x7486E1: the abodes +0x754 (next +0x9C) vt +0x8D8 GetDesireToBeRepaired, each stored
- `DesireToRepair`: 0x748706..0x748726: 1 < s -> 1
- `DesireToSupplyWorkshop`: 0x748730
- `DesireToBuildWonder`: 0x748757..0x7487A4: b x (1 - min(0.5 (D0 + (D1 + D5)), 0.5)) (fcom 0.5; test ah, 1; jne keeps < 0.5)
- `DesireForRelaxation`: 0x74890C..0x74892C: x = 1 - sky; x > 0 (fcom 0; test ah, 0x41; je keeps) else 0; R = ramp x x
- `DesireForRelaxation`: 0x74892E..0x74895A: R < 0.1 (or unordered) -> 0.1; R > 1 -> 1
- `DesireForSleep`: 0x7489A2..0x7489B9: s > 0 (test ah, 0x41; je keeps) else 0; s^2 (not clamped: 6.25 at night)
- `ModificationFood`: 0x746310..0x74633D: 1 - min(n / (fn_747AF0 + 1e-4), 1)
- `ModificationWood`: 0x746353..0x74637E: (u64) g[0xDA92B8] (250) x +0x510[d] + 1e-4, stored
- `ModificationWood`: 0x746382..0x7463BC: + the storage pit's wood (GetResource(WOOD)), stored
- `CallDesireFunction`: 0x745DC6..0x745DFF: x GetInfo(d) +0x58[(GetTribe 0x73C840 - 0xDA57A8) / 0x1C] TribeMultiplier -> +0x168[d]
- `ProcessDesire`: 0x745D4E..0x745D56: +0x118[d] = CallDesireFunction(d)
- _qsort 0x7C7E64 (num 17, width 12; element indices here: the byte test `higuy - 1 - lo >= hi - loguy` of 0x7C7F43..0x7C7F51 is (higuy - lo) > (hi - loguy) in elements)
- `MsvcQsort`: 0x7C7EAF
- `MsvcQsort`: 0x7C7EA6..0x7C7EAE
- `MsvcQsort`: 0x7C7EBA
- `MsvcQsort`: 0x7C7EE3..0x7C7EED: the middle (lo + size / 2) swapped to lo as the pivot
- `MsvcQsort`: 0x7C7EFA..0x7C7F10: do loguy++ while loguy <= hi && comp(loguy, lo) <= 0
- `MsvcQsort`: 0x7C7F12..0x7C7F22: do higuy-- while higuy > lo && comp(higuy, lo) >= 0
- `MsvcQsort`: 0x7C7F27..0x7C7F38: higuy < loguy -> break; else swap(loguy, higuy)
- `MsvcQsort`: 0x7C7F3A: swap(lo, higuy)
- `MsvcQsort`: 0x7C7F4F (jl -> the else)
- `MsvcQsort`: 0x7C7F53..0x7C7F72: lo + 1 < higuy -> push (lo, higuy - 1)
- `MsvcQsort`: 0x7C7F74..0x7C7F7E: loguy < hi -> lo = loguy, again
- `MsvcQsort`: 0x7C7F83..0x7C7F9A: loguy < hi -> push (loguy, hi)
- `MsvcQsort`: 0x7C7F9C..0x7C7FAE: lo + 1 < higuy -> hi = higuy - 1, again
- `MsvcQsort`: 0x7C7EC2..0x7C7EE1: pop, or done
- `Process`: 0x745B24..0x745B30: fn_745CA0(d) for d = 0..16
- `Process`: 0x745B34 fn_746140
- `Process`: 0x745B3B fn_746190
- `CheckVillagerNeeded`: 0x745FF3..0x74600E: trigger == 0 (fcomp 0; test ah, 0x40: also unordered) -> 0.001
- `CheckVillagerNeeded`: (child: the caller's IsChild, vt +0xAF8 0x74601F)
- `CheckVillagerNeeded`: 0x7460DB..0x7460E2
- `CheckVillagerNeeded`: 0x746031..0x74605F: t = trigger + DesireSort::GetInfo 0x746570 +0x18; < 1 or 1 (fcom 1; test ah, 1; je), stored as a float
- `CheckVillagerNeeded`: 0x74605F..0x746073: a child and no +0x60 -> next (no cut)
- `CheckVillagerNeeded`: 0x746075..0x74609A: no CheckSatisfy (+0x40) -> next (no cut)
- `CheckVillagerNeeded`: 0x74609C..0x7460B1: GetTemporaryDesireVillagerModification(k) x value <= t (fcomp; test ah, 0x41; jne) -> return 0. Literal oddity (PLAN §0.5): the modification is asked with the loop index k, not with the desire's index d, so position k of the order is corrected with the counters of desire number k
- `CheckVillagerNeeded`: 0x7460B3..0x7460D9: CheckSatisfy (+0x40 / +0x44 on the villager) == 1 -> 1
- `ScriptSetTownDesireBoost`: 0x6FE696..0x6FE6B2: no thing, or not IsTown (vt +0x1B8) -> "Thing not valid!" (0xC0DA54)
- `ScriptSetTownDesireBoost`: 0x6FE6C3..0x6FE6EF: desire >= 17 (signed, jge), boost < -1 (or unordered), boost > 1 -> "Invalid Params"
- `ScriptSetTownDesireBoost`: 0x6FE6F7..0x6FE73E: both right -> +0xD4[desire] = boost (Town +0x108) and fn_746140 (order 1 only). (aproximado) desire < 0, which the original writes out of the array, is not written
- `ScriptGetDesire`: 0x6FCCB5..0x6FCCF1: d out of [0, 17) -> "Invalid desire" (0xC0CFC0) and PUSH 0 without the second POP
- `ScriptGetDesire`: 0x6FCCF2..0x6FCD3B: POP the object, GetScriptGameThing 0x70D220; none -> "Object no longer valid" (0xC0D428)
- `ScriptGetDesire`: 0x6FCD3C..0x6FCD48: not IsTown (vt +0x1B8) -> PUSH 0 (no message: je 0x6FCD1E is after it)
- `ScriptGetDesire`: 0x6FCD4A..0x6FCD5C: PUSH GetRawDesire(d) (a float)
- `MapTownDesireBoost`: 0x7179EC: FindTownWithID 0x552FA0 (the caller), fn_747270(name); both -> +0xD4[d] = value (0x717A26), no re-sort, no range check
- the plans +0x9A8 (oldest first) for Repair_Town: PlannedMultiMapFixed::GetDesireToBeRepaired 0x648910 (+0x30 ? info +0x118 : 0; building_sites::DesireInputsOf)
- `owner`: < that player (its GameStats +0xA44, 0x745C2A)
- `children`: < +0x60: a child may serve it (CheckVillagerNeededForTownDesire 0x746073)
- `DesireForRain`: 0x748690 / 0x7486A0 (`fld 0; ret`)
- `DesireToSupplyWorkshop`: 0x748730 (`fld 0; ret`)
- The VC6 CRT _qsort 0x7C7E64 (CUTOFF 8 -> _shortsort 0x7C7FB8, else the middle as pivot and _swap 0x7C8006; not stable) with the comparator 0x746110 (value: a < b -> 1, == -> 0, else -1: descending, NaN -> 1)
- fn_00747150 (this = TownDesire, Town +0x34; SetStateSpeed 0x7539BD): S = 0.2 x (raw 13 + 12 + 9 + 7 + 6 + 5 + 4 + 3 + 1 + 0) (+0x168, no boosts) clamped to [0, 1]; returned on the x87 stack (float precision: the x87 control word is 24 bits, 0x7DEE0D). 0 without a town
- `GetSortedDesires`: +0x278 (GetSortedDesire 0x7465D0 = &order1[k]) and +0x344 (Town +0x378: value +0x37C, type +0x380, the one GGuidance::CheckTownDesiresSFX 0x71B130 reads). An empty (all 0) array without a town
- Town::CalculateDesireForFood 0x747F00 now, literal: with HelpSpritesLowOnFood when v >= 0.95 for the local player (Villager::ArrivesAtFoodReaction 0x764A45 calls it too)
- TownDesire::CallDesireFunction 0x745D80 now, on a town entity (Abode::DoResourceRemoving 0x404FA0 / DoResourceAdding 0x404E22 call it before the resource changes): +0x168[d] (the raw) is rewritten and the function's warning may play (HelpSpritesLowOnFood / LowOnWood); returns clamp(raw x modification, -1, 1). 0 without a town
- GScript::SetTownDesireBoost 0x6FE650 after its three POPs (boost, desire, thing): "Thing not valid!" without a town, "Invalid Params" unless desire < 17 (signed) and -1 <= boost <= 1; both right: +0xD4[desire] = boost and fn_746140 (order 1 only). (aproximado) a negative desire, which the original writes out of the array, is not written. Returns whether it wrote; `errors` gets the messages (ScriptErrorMessage 0x6F62B0)
- GScript::GetDesire 0x6FCCA0 after its first POP (d): d out of [0, 17) -> "Invalid desire" and 0 without the second POP (literal: the object stays on the stack); else `popObject`, no thing -> "Object no longer valid" and 0, not a town -> 0 (no message, 0x6FCD48), else GetRawDesire(d)
- Villager::CheckSatisfyToRepair 0x759370 (12): GetBestRepairBuildingSite 0x747EA0 -> SetupBuildingObject
- The comparator 0x746110: fld a+4; fcomp b+4; test ah, 1 (less or unordered) -> 1; test ah, 0x40 (equal) -> 0; -1
- `ShortSort`: _swap 0x7C8006 (a == b: nothing)
- `ProcessDesire`: 0x745CAC..0x745CC0: +0x4DC[d] < 0 (fcomp; test ah, 1: also unordered) -> 0
- `ProcessDesire`: 0x745CCB..0x745CE0: the copies +0x454 / +0x498
- `ProcessDesire`: 0x745CF8..0x745D1C / 0x745D23..0x745D47: Amount / Desired (fild qword: unsigned)
- fn_746140 0x74614F..0x74616C: {+0xD4 + +0x90, +0x118 + +0xD4 + +0x90, d} for d in order, then _qsort 0x746182
- fn_746190 0x7461A0..0x7461BD: {+0x90 (a dword copy), +0x168 + +0xD4 + +0x90, d}, then _qsort 0x7461D3
- `TownNeedsSum`: 0x747150..0x747186: fld +0x19C (13); fadd +0x198 (12), +0x18C (9), +0x184 (7), +0x180 (6), +0x17C (5), +0x178 (4), +0x174 (3), +0x16C (1), +0x168 (0): the raw desires (+0x168, no boosts), in that order on the x87 stack, each fadd rounded to float (24-bit x87 control word, 0x7DEE0D)
- `TownNeedsSum`: 0x74718C: x 0.2 ([0x8AA3AC], a float); 0x747192..0x7471B7: < 0 -> 0, > 1 -> 1
- `GatherInputs`: ⚠️ approximate a slot whose entity is gone counts as empty
- `GatherInputs`: 0x73E5E0(4): player +0x68[4]
- `tribalPower4`: < 0x73E5E0(4) = player ? player +0x68[4] : 1 (PlayerMagic::tribalPower)

#### Desire functions

- `Town::GetDesire` = +0x118 + +0xD4 + +0x90 of TownDesire, at Town +0x14C / +0x108 / +0xC4 [0x73E400];
  GetRawDesire [0x73E420].
- Food: the available food is ftol(TownStats foodCarried) + the storage pit's food (GetStoragePit →
  GetResource(FOOD)) + that of the temporary pot +0x600, the latter with no IsAvailable test [fn_747A90,
  0x747A94..0x747AC6].
- Food: the warning is evaluated on the already stored float (fst), which is why v ≥ 0.95 compares the rounded value
  [0x747F29..0x747FA0]; the 0.95 threshold is at [0x8CF000].
- Wood: the available wood is ftol(wood in building sites + wood carried) + the storage pit's + that of the
  temporary pot +0x604 [fn_747B00, 0x747B03..0x747B3C].
- Wood: B and C come from GTownInfo +0xE0 minimumWoodForDesire and +0xE4 maximumWoodForDesire multiplied by k =
  max(abodes (+0x758) / info +0xE8 numOfBuildingsForDesiredWood, 1) [fn_747BE0, fn_747B60].
- Wood: v = (1 − min(w / C, 1)) × (1 − min(w / B, 1) + a), with S = R12 + R9 + R6 + R5 summing first the raws, then
  the boosts and then the A boosts, and S < 3 or 3 (NaN keeps the sum) [0x747FF7..0x748152].
- Abodes: a = adults / (adult places +0x644 + 1e-5) capped at 1.5; c = children / (child places +0x650 + 1e-5)
  capped at 1.5; m = the larger; result clamp((1 − D6) × m⁴ × (1 − R9), 0, 1) [0x748216..0x74831B]; 1e-5 at
  [0x99A100].
- Civic_Buildings: for each abode number i = 0..15 the town does not have (+0x718[i] == 0), p =
  PopulationWhenNeeded (+0x1B4) of the tribe's GAbodeInfo (+0x5B8); if p > −1, (p == 0 or homeless / population <
  0.2) and p ≤ population: s += 0.5 × (pop − p + 0.001) / (p + 0.001) + 0.5; result min(s, 1)
  [0x748338..0x748422].
- Civic_Buildings: with population 0 and no homeless the comparison is "unordered" and the condition passes
  [0x74837F..0x7483BA].
- For_Children: A = R0 ≥ 1 ? 0 : (R0 > 0 ? 1 − R0 : 1); B = children < child places ? 1 : 0; C = adults < adult
  places ? 1 : 0.5; Q = (1 − max(D4, 0)) × (1 − max(D3, 0)); al = 0.5 × alignment³ (0 with no player); X = the
  player's TribalPower[4] (1 with no player); v = X × (1 + al) × Q × C × B × A, ×0.5 with no functional creche
  (+0x744), clamp [0, 1] [0x748436..0x7485F5].
- Playtime: the cut-off turn 4000 is `cmp 0xFA0; jbe` (strictly greater) [0x748877].
- To_Build: sum of `BuildingSite::GetDesireForVillagers` over the sites +0x790, capped at 1 [0x748640..0x748689].
- Repair_Town: sum of `Abode::GetDesireToBeRepaired` of the abodes +0x754 (no site or +0x638 filter, 0x7486CB) plus
  the GetDesireToBeRepaired of each plan +0x9A8, capped at 1 [0x7486B5..0x748726].
- `Abode::GetDesireToBeRepaired` [0x406970]: 0 if life > GTownInfo +0x10C thresholdToStartRepairing; 0 if it is a
  dwelling (type & 2) with nobody in it (+0xA4 == 0); otherwise MultiMapFixed [0x52ECE0]: 0 with life ≥ 1, else
  min(((1 − life) × 0.5 + 0.5) × info +0x118 DesireToBeRepaired, 1).
- For_Wonder: b × (1 − min(0.5 × (D0 + D1 + D5), 0.5)), with b = the owner's GetBeliefInPlayer
  [0x748743..0x7487A4; `Town::GetBeliefInPlayer` 0x73BAB0].
- Relaxation: sky = Time2SkyType(GetVisualTime); ramp = fn_557AE0(0.5 × info +0xEC relaxationMod, the same); x =
  max(1 − sky, 0); R = ramp × x with minimum 0.1 (also NaN) and maximum 1 [0x7488C4..0x74895A].
- Sleep: s = fn_557AE0(1, 0) + sky − info +0xD8 bedTimeMod; s > 0 or 0; returns s² unclipped (hence 6.25 at night)
  [0x748964..0x7489B9].
- GetVisualTime is `GGameInfo::GetVisualTime` = [0xBF3380] [0x5575A0].
- Supply_Worship: amount = (storage pit and worship site) ? the pit's food : 0 [0x747D70]; desired fn_73C980 via the
  worship site (fn_77B920) [0x747DD0]; the desire function is `fld 0; ret` [0x748320].
- Amount / Desired for Abodes: amount = Town +0x620 [0x747CF0]; desired = ftol(((adults + children) / +0x10 if
  +0x10, else 0) + +0x10 + 0.001) / (+0x30 + 0.001)) with signed fild [0x747D00, fn_749BE0].
- Amount / Desired for Civic: Town +0x62C [0x747D50] / +0x634 [0x747D60].
- `FoodAvailable`: 0x747A94..0x747A9A
- `FoodAvailable`: 0x747AA3 GetStoragePit
- `FoodAvailable`: 0x747AB9 GetResource(FOOD) (vt +0x98)
- `FoodAvailable`: 0x747AC6 +0x600
- `WoodAvailable`: 0x747B03..0x747B10
- `WoodAvailable`: 0x747B2F GetResource(WOOD)
- `WoodAvailable`: 0x747B3C +0x604
- fn_747BE0 (factor info +0xE0 minimumWoodForDesire) / fn_747B60 (+0xE4 maximumWoodForDesire): k = abodes (fn_747C60 +0x758, fild qword) / info +0xE8 numOfBuildingsForDesiredWood; `fcomp 1; test ah, 0x41; jne` -> k <= 1 uses 1 (so k = max(k, 1)); returned in st0 (float precision at 24 bits, 0x7DEE0D): Wood stores it, the modification 0x7463C0 does not, which gives the same value
- `AmountAbodes`: 0x747CF0: Town +0x620
- `DesiredAbodes`: 0x747D00: fn_749BE0 = +0x10 ? (adults + children) / +0x10 : 0; then ftol((that + +0x10 + 0.001) / (+0x30 + 0.001)) (fiadd, fild dword: signed)
- `AmountCivic`: 0x747D50: Town +0x62C
- `DesiredCivic`: 0x747D60: Town +0x634
- `AmountSupplyWorship`: 0x747D70: storage pit && worship site ? the pit's food : 0. 0
- `DesiredSupplyWorship`: 0x747DD0 -> fn_73C980 (worship site -> fn_77B920).
- `DesiredSupplyWorship`: 5 Abodes 0x748210; Amount 0x747CF0, Desired 0x747D00; CheckSatisfyAbodesDesire 0x758E30; 0x746490
- `DesiredSupplyWorship`: 6 Civic_Buildings 0x748330; 0x747D50 / 0x747D60; CheckSatisfyCivicBuildings 0x758E90; 0x746490
- `DesireForFood`: 0x747F08..0x747F25: a = (u64) fn_747A90 + 1e-4, stored
- `DesireForWood`: 0x7480D9..0x748100: w = (u64) fn_747B00, B = fn_747BE0, C = fn_747B60 (each stored)
- `DesireForSupplyWorship`: 0x748320: fld 0; ret
- `DesireToBuild`: 0x748640..0x748668: sum of BuildingSite::GetDesireForVillagers 0x43BD70 over the sites +0x790 (each stored)
- MultiMapFixed::GetDesireToBeRepaired 0x52ECE0: IsRepaired (Abode 0x4016A0: life >= 1) -> 0
- `AbodeDesireToBeRepaired`: 0x52ECF3..0x52ED2A: min((1 - life) x 0.5 + 0.5) x info +0x118 DesireToBeRepaired, 1) (fcom 1; test ah, 1; jne)
- `DesireToRepair`: 0x7486E3..0x748704: the plans +0x9A8 (next +0x44) vt +0x514 = PlannedMultiMapFixed::GetDesireToBeRepaired 0x648910 (+0x30 ? info +0x118 : 0), each stored. +0x30 is PlannedAbode::wasBuilt, written by PlannedMultiMapFixed(MultiMapFixed*) 0x648820 from PlannedAbode::Create(Abode*) 0x405660; the values come from building_sites::DesireInputsOf -> plans::GetDesireToBeRepaired. The abodes above are summed with no site or +0x638 filter (0x7486CB): a rock-damaged house raises Repair_Town although CheckSatisfyToRepair cannot take its ordinary site (literal)
- `DesireToBuildWonder`: 0x748743..0x748753: b = GetBeliefInPlayer(GetPlayer) 0x73BAB0, stored.
- `DesireForRelaxation`: 0x7488C4..0x7488DC: sky = Time2SkyType(GetVisualTime) (stored)
- `DesireForRelaxation`: 0x7488E0..0x748907: fn_557AE0(0.5 x info +0xEC relaxationMod, the same) (both stored floats)
- `DesireForSleep`: 0x748964..0x74897C: sky = Time2SkyType(GetVisualTime) (stored)
- `DesireForSleep`: 0x748983..0x7489A0: s = fn_557AE0(1, 0) + sky - info +0xD8 bedTimeMod
- GetStoragePit 0x73B5B0 and its GetResource (PotStructure 0x66EF00: StoragePitStore)
- `visualHour`: < GGameInfo::GetVisualTime 0x5575A0 (DayNightClock::GetVisualTime)
- `DesireForSupplyWorship`: 0x748320 (`fld 0; ret`)
- Abode::GetDesireToBeRepaired 0x406970 (vt +0x8D8, with MultiMapFixed's 0x52ECE0)

#### Modifications

- `GetDesireVillagerModification` [0x746270] calls the entry's +0x50 / +0x54 function with d.
- `ModificationGeneral`: 1 − min(doingNow +0x510[d] / ((u64)(adults + children) + 1e-5 in double), 1)
  [0x746493..0x7464E2]; 1e-5 double at [0x99A0E8].
- `ModificationFood`: n = 150 × doingNow[d] + 1e-4 + (the storage pit's food, the pot is not read); 1 − min(n /
  (DesiredFood + 1e-4), 1) [0x7462A3..0x74633D].
- `ModificationWood`: the same with 250 and the pit's wood, divided by C (fn_747B60) + 1e-4 [0x746353..0x7463ED].
- 150 / 250 are read from a fixed row of the exe: GVillagerInfo[10] ("African Farmer Male", 0xDA6BE8 + 10 × 0x3A4)
  +0x264 / +0x268, into g[0xDA92B4] / g[0xDA92B8] (load_variables 0x42C9AF).
- `ModificationToBuild`: a = 1e-4 + Σ builders of the site (+0x634), b = 1e-4 + Σ fn_43BBD0 (maximum builders);
  1 − min(a / b, 1); with no sites it gives 0 [0x746404..0x746480].
- `GetTemporaryDesireVillagerModification(k)` [0x7464F0]: 1 − min(max(+0x4DC[k] − +0x454[k], 0) / (Town +0x618 +
  +0x61C + 1e-5), 1).
- g[0xDA92B4] / g[0xDA92B8]: GVillagerInfo[10] (load_variables 0x42C9AF) +0x264 / +0x268
- `ModificationWood`: 0x7463C0..0x7463ED: 1 - min(n / (fn_747B60 + 1e-4), 1)
- `GetTemporaryDesireVillagerModification`: 0x7464F3..0x74651D: pop = (u64) Town +0x618 + +0x61C + 1e-5 (double)
- `GetTemporaryDesireVillagerModification`: 0x746523..0x746548: x = +0x4DC[k] - +0x454[k]; `fld 0; fcomp st(1); test ah, 0x41; jne` keeps x >= 0, else 0
- `GetTemporaryDesireVillagerModification`: 0x746548..0x746561: 1 - min(x / pop, 1)
- TownDesire::GetDesireVillagerModification 0x746270: the entry's +0x50 with d
- TownDesire::GetTemporaryDesireVillagerModification 0x7464F0(k): 1 - min(max(+0x4DC[k] - +0x454[k], 0) / (pop + 1e-5), 1), pop = Town +0x618 + +0x61C

### Notes from the code

- One node {score, object, k} of CheckSatisfyFoodDesire's list (12 bytes on the stack, 0x759F54..0x75A00A)
- `CheckSatisfyFoodDesire`: 0x759F3B: GetTown (vt +0x48).
- `CheckSatisfyFoodDesire`: 0x759F50..0x75A00A: for k 0..2 a node {score, object, k}: k 0 fn_0073E750 (the fish farms), k 1 FindBestField 0x73E870 (the fields), k 2 fn_0073E7F0 (the flocks without a shepherd), each the strictly best above 0 or {0, null}; inserted before the first node it beats strictly (`fld new; fcomp cur; test ah, 0x41; je`): high to low, ties keep k's order.
- `CheckSatisfyFoodDesire`: 0x75A010..0x75A058: frac = (float)(1 - (GetFoodCapacity (0x7514D0) + 1e-5) / (MaxFoodCarried (+0x264) + 1e-5)); 0x75A05C..0x75A07C: GetDistanceModifier(GetDistanceInMetres(GetResourceDropoffPos(FOOD), me), 500) x frac (DropOffScore; asking for the drop-off point may make the town's temporary food pot: a side effect, literal)
- `CheckSatisfyFoodDesire`: 0x75A084..0x75A090: drop > head.score (fcomp dword; `test ah, 0x41; jne`) -> GotoStoragePitForDropOff 0x769620 (its result), after the list is freed
- `CheckSatisfyFoodDesire`: 0x75A0C1..0x75A0C6: head.object == 0 -> 0x75A1F4: 0
- `CheckSatisfyFoodDesire`: 0x75A0CC..0x75A1EF: switch head.k; r == 1 -> 1, else 0
- `CheckSatisfyFoodDesire`: 0x75A18B: VillagerBecomesFisherman(farm) 0x75B560
- `CheckSatisfyFoodDesire`: 0x75A151: VillagerBecomesFarmer(field) 0x759C00
- `CheckSatisfyFoodDesire`: 0x75A0E5: VillagerBecomesShepherd(flock) 0x768BE0. (approximate until) 0: the field / farm behind it is not tried (the original tries only the head too)

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `VillagerChild.cpp`, `VillagerStates.cpp`; the desire functions and the
  modifications read in the exe with the xref index; `_qsort` run under an emulator.

## openblack

**Done.** Code: `Villager/VillagerDecide.{h,cpp}`, `Villager/VillagerHome.{h,cpp}`, `Villager/VillagerSatisfy.{h,cpp}`
(the CheckSatisfy functions), `Town/TownQueries.{h,cpp}`, `Town/AbodeQueries.{h,cpp}`, `TownDesire.{h,cpp}` (table,
functions, Process, orders, distribution, read API, scripts), `Town/TownProcess.{h,cpp}` (`Town::Process` and the
player loop), `Town/TownStats.{h,cpp}`, `Components/Town.h` (`TownDesire`, `DesireSort`, `TownStats` and the town
fields). Tests: `test/test_villager_decide.cpp` (14 cases) and `test/test_town_desire.cpp` (18 cases); the orders are
compared with the exe's `_qsort` run under an emulator.

- **Order in the turn** ⚠️ approximate: `town_process::ProcessPlayers` is called once per turn from
  `magic::ProcessTurnStart` (`Magic/MagicLoop.cpp`, slot 3 of the turn), after the influence rings and before the
  villagers; it iterates over `map_cells::ForEachTown`. The original does towns, teleport and alignment player by
  player; here each step goes through all players before moving on to the next. The influence of `Town::Process` is
  computed every turn by `influence::ProcessTowns` (`influence::ProcessTurn`, inside `magic::ProcessTurn`), not in
  `town_process::ProcessTown`.
- The town's emergency turn is `Town::emergencyTurn` (+0xF1C) and the meeting point's cache `Town::congregationPos`
  (+0xF10).
- The sort is the VC6 `_qsort` ported literally. The distribution is `villager::CheckNeededForTownDesire`
  (`VillagerDecide.cpp`).
- The desire functions read the alignment and TribalPower[4] of `PlayerMagic`, the life of `ecs::life`, and
  `sky_type::At` and `EveningRamp` over the visual time of day of `Game`'s clock. For_Wonder is 0 while
  `GetBeliefInPlayer` is not wired (see Pending).
- The player's statistic goes to `game_stats::TownDesire`.
- `Town::Process` every 10 turns calls `worship::percentage::GetWorshipersNeeded(1, 0)` /
  `AdjustWorshipersWorshipping(n, 1, 0)` of the worship code.
- **API for other systems** (`ECS/TownDesire.h`): `GetDesire` / `GetRawDesire`, `GetSortedDesires` (+0x278),
  `GetSortedRawDesires` (+0x344), `GetField` (+0x90 / +0xD4 / +0x118 / +0x168), `GetDesireSignificanceToVillager`,
  `GetMostDesired`, `GetMostSignificantRawDesire`, `CalculateDesireForFood` (with warning) / `FoodDesireValue`
  (without warning), `SetBoost`, `AlignmentTurns` (+0x410). Audio's `desireTowns` / `townResourceNeeds` are filled
  from it (`ECS/AudioQueries.cpp`).
- **Disciples and children**: 163's disciple branch runs `DiscipleDecideWhatToDo` (`Villager/VillagerDisciple.{h,cpp}`),
  a follower's and a job disciple's reactions included; a child runs `ChildDecideWhatToDo` (its check, the town desires,
  `ChildGotoCreche` and state 113 in `Villager/VillagerChild.{h,cpp}`, else following its mother, 114). See
  [Disciples](villager-life.md#disciples).
- **The supplies and relaxation**: the workshop's supply (desire 13) takes a villager to the town's best workshop
  that wants wood (`CheckSatisfySupplyWorkshop`, states 211 and 210); the worship supply asks
  `villager_worship::GotoStoragePitForWorshipSupplies`, which the original's desire of 0 never reaches; the playtime
  desire is the original's literal 0; relaxation offers only the town's artefacts
  (`ArtefactSystem::OfferRelaxation`).
- Porting decisions (his names over ours): [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Villager/VillagerDecide.cpp`: `DecideWhatToDo`, `NothingToDo`, `GoAndChilloutOutsideHome`, `GoAndChilloutInTown`, `ChildFollowsMother`, `CheckNeededForSomething`, `CheckNeededForSpecial`, `CheckNeededForCivic`, `CheckNeededForTownDesire`, `GetOwnDesiresTrigger`, `GetDesireForLife`, `GetLifeDesireFromLife`, `CheckSatisfyOwnDesire`, `CheckSatisfySleep`, `SetupNothingToDo`, `GetChillOutPos`, `GetPosOutsideMyHouse`, `TownEntityOf`, `AbodeOf`, `Villager::k_FlagDiscipleFollower`, `Villager::k_FlagDisciple`, `VillagerStates::DecideWhatToDo`, `CheckVillagerNeeded`, `Villager::k_FlagAfterTapOnAbode`, `DesireForFood`, `Villager::k_FlagAtHome`, `Index::Top`, `VillagerStates::SleepInTent`.
- `src/ECS/Villager/VillagerDecide.cpp`, `TownEntityOf`: Villager::GetTown 0x751F00 (vt +0x48): +0x12C, the town entity (entt::null without one)
- `src/ECS/Villager/VillagerDecide.cpp`, `TownEntityOf`: (guard) GetTown 0x751F00 only reads +0x12C: stands for the missing town unlinking
- `src/ECS/Villager/VillagerDecide.cpp`, `AbodeOf`: Villager::GetAbode 0x752160: +0x128 (entt::null without one, or when the abode is gone)
- `src/ECS/Villager/VillagerDecide.cpp`, `AbodeOf`: (guard) GetAbode 0x752160 only reads +0x128: stands for Abode::ToBeDeleted's unlinking
- `src/ECS/Villager/VillagerDecide.cpp`, `DecideWhatToDo`: 0x7516C4..0x7516D3: SetupNothingToDo always gives 1, so the SetTopState(36) after it is dead code
- `src/ECS/Villager/VillagerDecide.cpp`, `SitAndChillout`: 0x76B532..0x76B551: GameRand(10) (VillagerStates.cpp 0x3FF) == 0 -> SetupNothingToDo, without going through 163
- `src/ECS/Villager/VillagerDecide.cpp`, `ChildFollowsMother`: (aproximado) openblack's Collide (ecs::animal_ai::detail::Collides) knows water / land only: bit 8 is never set (the tests have no land: navigable)
- `src/ECS/Villager/VillagerDecide.h`: `EnterSitAndChillOut`, `CheckNeededForSomething`, `CheckNeededForSpecial`, `CheckNeededForCivic`, `CheckNeededForTownDesire`, `GetOwnDesiresTrigger`, `GetDesireForLife`, `CheckSatisfyOwnDesire`, `CheckSatisfySleep`, `CheckTakeResourcesToStoragePit`, `GetChillOutPos`, `GetPosOutsideMyHouse`, `NothingToDo`.
- `src/ECS/Villager/VillagerDecide.h`, `NothingToDo`: 209: Villager::NothingToDo 0x760000 (`mov eax, 1; ret`)
- `src/ECS/Villager/VillagerDecide.h`, `ChildDecideWhatToDo`: Villager::DiscipleDecideWhatToDo 0x751720 is in VillagerDisciple.h  Villager::ChildDecideWhatToDo 0x757EC0: CheckChild, CheckNeededForTownDesire, ChildGotoCreche, else 114.
- `src/ECS/Villager/VillagerSatisfy.cpp`: `CheckSatisfyFoodDesire`.
- `src/ECS/Villager/VillagerSatisfy.cpp`, `CheckSatisfyFoodDesire`: (openblack, guard) the desires only reach villagers of a town
- `src/ECS/Villager/VillagerSatisfy.h`: `CheckSatisfyPlaytimeDesire`, `CheckSatisfyToRepair`.
- `src/ECS/Villager/VillagerSatisfy.h`, `CheckSatisfySuppyWorship`: 0
- `src/ECS/Villager/VillagerSatisfy.cpp`, `CheckSatisfySupplyWorkshop`: the town's best functional workshop wanting wood
- `src/ECS/Villager/VillagerSatisfy.cpp`, `CheckSatisfyRelaxation`: only the artefacts are offered (football and the creature are not ported)
- `src/ECS/TownDesire.cpp`: `FoodAvailable`, `WoodAvailable`, `WoodScale`, `AmountAbodes`, `DesiredAbodes`, `AmountCivic`, `DesiredCivic`, `DesiredSupplyWorship`, `Compare`, `ShortSort`, `ContextFor`, `DesireForFood`, `DesireForWood`, `DesireForPlaytime`, `DesireForAbodes`, `DesireForCivicBuildings`, `DesireForSupplyWorship`, `DesireForChildren`, `DesireToBuild`, `DesireForRain`, `DesireForSun`, `AbodeDesireToBeRepaired`, `DesireToRepair`, `DesireToSupplyWorkshop`, `DesireToBuildWonder`, `DesireForRelaxation`, `DesireForSleep`, `ModificationFood`, `ModificationWood`, `GetTemporaryDesireVillagerModification`, `CallDesireFunction`, `ProcessDesire`, `MsvcQsort`, `SortDesires`, `SortRawDesires`, `Process`, `CheckVillagerNeeded`, `TownNeedsSum`, `GatherInputs`, `ScriptSetTownDesireBoost`, `ScriptGetDesire`, `MapTownDesireBoost`, `AmountSupplyWorship`, `VillagerDisciple::Craftsman`.
- `src/ECS/TownDesire.cpp`, `AmountSupplyWorship`: the worship site's part;
- `src/ECS/TownDesire.cpp`, `DesiredSupplyWorship`: 0
- `src/ECS/TownDesire.cpp`, `DesireToBuildWonder`: not ported (0)
- `src/ECS/TownDesire.cpp`, `ProcessDesire`: 0x745D5D..0x745D6E: [0xDA2770] = ftol([0xDA2770] + desire), GNetwork::ResetStateDebug's checksum: not ported
- `src/ECS/TownDesire.cpp`, `Process`: 0x745B42 fn_7457C0: the debug trace (only with [0xCD3C74] != 0): openblack's is OPENBLACK_TOWN_TRACE 0x745B47..0x745B7D: turn % 50 == 0, the town (+0x160) and its player
- `src/ECS/TownDesire.cpp`, `GatherInputs`: +0x600 / +0x604 the temporary pots (ecs::town_stores): fn_747A90 0x747AC6 / fn_747B00 0x747B3C test the slot only (no IsAvailable), then GetResource (vt +0x98: PotStructure 0x66EF00, object_resources).
- `src/ECS/TownDesire.cpp`, `GatherInputs`: Town::GetBeliefInPlayer 0x73BAB0 is not ported (For_Wonder 0)
- `src/ECS/TownDesire.cpp`, `GatherInputs`: GGameInfo::GetVisualTime 0x5575A0 = [0xBF3380]: the Game's DayNightClock (the single source) (openblack, guard) without a Game (the tests) noon
- `src/ECS/TownDesire.h`: `planRepairDesires`, `owner`, `tribalPower4`, `visualHour`, `children`, `DesireForSupplyWorship`, `DesireForRain`, `DesireToSupplyWorkshop`, `AbodeDesireToBeRepaired`, `GetDesireVillagerModification`, `GetTemporaryDesireVillagerModification`, `MsvcQsort`, `TownNeedsSum`, `GetSortedDesires`, `CalculateDesireForFood`, `CallDesireFunctionNow`, `ScriptSetTownDesireBoost`, `ScriptGetDesire`, `PlayerNames::NEUTRAL`, `DesireForFood`.

### Test hooks

- `OPENBLACK_TEST_VILLAGER_FOOD="<food>[,<n>]"`, `OPENBLACK_TEST_VILLAGER_NOTHING="<r>[,<n>]"` (forces the next
  GameRand(9)), `OPENBLACK_TEST_VILLAGER_SHOT="<turn>,<png>[;...]"` (capture on that turn). The villager trace adds
  `decide: …`, `chill 245/252: …`, `sit 246: check -> …`, `home 36: …`, `child 114: …` and `congregation town …`.

## Pending

- **For_Wonder**: 0 in openblack while the town's `GetBeliefInPlayer` is not read by it; the original's formula is
  above.
- **Relaxation's CheckSatisfy** offers only the artefacts in openblack (no football match or creature activity yet);
  the original's Relaxation is `Town::SetVillagerActivity` 0x73FF10, above. Playtime is 0 in both.
- **The player loop's order** (approximate, above): one step for all players at a time instead of player by player.
