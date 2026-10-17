# Villagers: picked up, dropped, hurt and dead

## Summary

The player's hand can pick a villager up, put it down gently or throw it. Picking up and throwing go through the
villager's full change of state (IN_HAND 24, FLYING 10), so the state it leaves runs its exit and a reaction it was
following ends. On landing the villager takes a pose by how it fell, plays its landing clip and goes back to what it
was doing or decides again. A villager whose life falls under 0.7 counts as injured in its town. When it dies the
owner's alignment and the town's counters move, the neighbours mourn, a soul rises and the corpse turns to a skeleton
before it is removed.

openblack reproduces the pick-up, the throw, the soft drop and landing, the injured count and the death with its
mourning and soul; the disciples' interaction after a landing waits for the disciples.

## The original

### Picked up or thrown

- **The pick-up.** `Villager::InterfaceSetInMagicHand` 0x753080: +0xE0 |= 0x20 (0x753083); FINAL 0x7B with a
  partner (+0x118): the partner's +0x118 = 0 and its DecideWhatToDo (vt +0x8C8, 0x7530B0), then +0x118 = 0; the
  hand's player is the villager's and IsSexuallyActive 0x761090 → CreateReaction(this, 0x20, player, 0) 0x6E3D70
  (0x7530F2). Then `Living::InterfaceSetInMagicHand` 0x5ECCB0 (0x7530FD): **StorePreviousState** (vt +0x8EC,
  0x5ECCB5 → Villager 0x763470), then **SetTopState(24 IN_HAND)** (vt +0x8E8, 0x5ECCC1 → `Villager::SetTopState`
  0x752010), its result unused; returns 1.
- **The start of a flight.** In the world, `Villager::InitialisePhysics` 0x5EFEF0: CreateDroppedResource 0x750940
  (0x5EFF01), then `Living::InitialisePhysics` 0x5EFE10 (0x5EFF1E): TOP already 10 FLYING (0x5EFE18) → on to
  `Object::InitialisePhysics` with no state change; otherwise StorePreviousState unless TOP is 24
  (0x5EFE23..0x5EFE2B), then **SetTopState(10 FLYING)** (0x5EFE37), and anything but 1 returns {0, 0}
  (0x5EFE42..0x5EFE4C): no physics at all. From the hand, `Villager::InitialisePhysicsFromHand` 0x5EFE90 →
  `Living::InitialisePhysicsFromHand` 0x5EFD80: a dancer leaves its dance (RemoveFromDance(1), vt +0xB08),
  StorePreviousState unless TOP is 24 (0x5EFDA3..0x5EFDAB), `Object::InitialisePhysicsFromHand` 0x636F00 (0x5EFDCC);
  a body of this object (po +0x18) that did not land (po +0x1D8 & 8 clear) → **SetTopState(10)** (0x5EFDEB), its
  result unused; no body → EndPhysics(NULL, 1) (vt +0x790, 0x5EFE00).
- **Both go through the whole SetTopState**, so the old state's exit runs, in that order: StorePreviousState first,
  then inside SetTopState the pause test, then `Living::SetTopState` 0x5F28E0's CallExitStateFunction (vt +0x904,
  0x5F28F1 → 0x752320: the TOP's exit, and the final state's when it is another; 0 → 0x2E, nothing changes), the
  out-of clip, the entry, SetStateSpeed and the clips. No state is skipped; only an already flying villager is left
  alone (above). CanPauseForASecond 0x752120 reads the row's +0xD4 (file 0xC4), which is 0 for both 10 and 24 in the
  frozen info.dat: no pause and no draw. Both rows have NoOutOfClip (file 0xF0 = 1): no out-of clip.
- **A reaction ends there.** The reaction rows' exit, ExitReaction 0x7527A0, stops the reaction (StopReacting, vt
  +0x998) unless the next state is reactive (file 0xB8), and neither 10 nor 24 is. So a villager picked up or thrown
  while it follows a reaction stops following it at once: it leaves the reaction's followers and its record of the
  type gets the turn. The other exits do their own work the same way (a farmer, fisherman, forester or builder
  leaves its job, a fireman its fire).
- **What can refuse.** `Villager::ExitInHand` 0x76B000 → `Living::ExitInHand` 0x5ED500: 1 only for 10, 11, a death
  state (vt +0x970) or 16; then the abode's +0x7C &= ~0x10. `cmp bl, 0xa` / `cmp bl, 0xb` (0x5ED505 / 0x5ED50A), then
  vt +0x970 (0x5ED51A), which is `Living::IsDeathState` 0x5ECAA0 in both Living's and Villager's vtables (13 ≤ state
  ≤ 17, `jb` / `ja` at 0x5ECAA7 / 0x5ECAAC), then `cmp bl, 0x10` (0x5ED524), already a death state; anything else
  returns 0. So the hand lets a villager go into 10, 11 and 13..17 only. `Villager::ExitInFlying` 0x76ACB0: 1 only
  for 24, 11, 14, 15 or 16, so neither lets the villager into its own state again. CannotExitState 0x768640 lets 24
  and 10 out; ExitNoChangeState 0x768780 lets out to 24 and to a script-interruptable state (file 0x1C), which 10 is.
  IN_HAND's and FLYING's own entry (EnterInHand 0x76AFE0) always give 1.
- The hand's `PlaceObjectInMagicHand` 0x5DA6F0 calls the object's InterfaceSetInMagicHand before its add
  (0x5DC330), which takes it out of the physics and of the map cells.
- `CannotExitState`: 0x768650 IsStateExitFunctionSameAs (vt +0x96C) -> 1; 0x76865A / 0x76865F: 0x18 IN_HAND, 0xA FLYING -> 1; else 0
- `PointAtDeadPerson`: 0x7666DB..0x7666EC: (i16) counter > t (jle) -> SetTopState(0xCE)
- `GoTowardsDeadPerson`: 0x7667EF..0x7667F8: SetTopState(0xCF)

### Soft drop and landing

- **Axes**. From the hand (put down or thrown) the original builds the body from the held object's drawn matrix
  (`Object::ThrowObjectFromHand` 0x638669 from HandAngles, SetUpPos 0x7FC779): the body's columns are the rows as
  they are. Only the world path (`Object::InitialisePhysics` 0x6374D9) uses `Object::GetWorldMatrix` 0x638200 =
  AngleY(+0x4C) while drawing at +0x4C + pi / 2 (fn_0051AF00). Every formula below reads the rows.
- **Pose** (`Villager::EndPhysics` 0x5F0A60, from the turn-start rows, a = right.y = po+0xD8): a < −0.5 (or NaN) →
  landType 1, yaw = GetYAngle(up) as it is; a > 0.5 → 2, yaw = Wrap(float(GetYAngle(up) + pi)); else 0, yaw =
  Wrap(float(GetYAngle(fwd) + pi)). The yaw goes to SetYAngle (MobileWallHug 0x60DAC0: +0x4C and GameAngle +0x5C =
  ConvertAngle3DToGame); the caller's second SetXYZAngles (0x646B72 / 0x645F08) then draws it at the GetYXZ yaw of
  the current matrix, so GameAngle (the walking heading) and the drawn yaw differ for poses 1 / 2. Empathy (ANGER
  0.5 / COMPASSION 0.1 when po.GetPlayer). Not a gentle put-down (po LANDED clear) → +0xE0 &= ~0x20. Then
  `Object::EndPhysics` (BackInMap), +0xB4 bits 4-5, the water / dead branches ([Death](#death)), LANDED.
- **Soft drop.** A villager put down gently (not thrown, on dry land, normal.y >= 0.7) is taken out of the physics
  at once through EndPhysics and never enters FLYING (Living 0x5EFDD7..0x5EFDEB): IN_HAND → LANDED, +0xE0 & 0x20
  kept (set by `Villager::InterfaceSetInMagicHand` 0x753083). On flat ground the pose is 0 and the heading the one it
  had in the hand.
- **Back to the previous state** (`Villager::EndPhysics` 0x5F0D0C..0x5F0D6F, alive on dry land, after the water and
  death branches):
  1. +0xB4 & 0x400 (`test word [esi + 0xB4], 0x400`) → `Villager::TestSpecial` 0x753340: with a town
     `Town::RemoveVillager` and `SetTown(0)`, without one `ReleaseFromScript`; then +0xB4 &= ~0x400. No writer of the
     bit was found in the exe ([The flag +0xB4 & 0x400](villager-reactions.md#a-thing-thrown-from-the-hand-reaction-9)).
  2. prev = byte +0x8E, the PREVIOUS state (0x5F0D21), read before LANDED.
  3. `SetTopState(11 LANDED)` (vt +0x8E8, 0x5F0D31).
  4. +0x24 & 0x400 (controlled by a script, 0x5F0D37) or the state table's dword at 0xDB9F6C + prev × 0x114 ≠ 0
     (0x5F0D41..0x5F0D5C) → `SetTopState(prev)` (0x5F0D63). 0xDB9F6C − 0xDB9E68 = 0x104, file offset 0xF4.
  - In the frozen info.dat (the table at file offset 0x730C0) the field is 1 only in the rows 4 IN_SCRIPT, 5
    IN_DANCE, 198 WEAK_ON_GROUND, 199 SCRIPT_WANDER_AROUND_POS, 200 SCRIPT_PLAY_ANIM, 226 MISSIONARY_DISCIPLE and 244
    SCRIPT_IN_CROWD; 0 in every other row.
- **LANDED** (`Villager::Landed` 0x7606E0): a disciple (+0xE0 & 0x200) still flagged 0x20 waits for the clip, drops
  the 0x20 reaction and the flag, and looks for something to work at (FindCloseObjectsForInteract 0x756E80 → begin
  state or SetupInspectObject 0x76AAB0 → 233 → 171-175 CHECK_INTERACT_WITH_*); otherwise (or nothing found) the 0x20
  reaction goes and PlayAnimThenSetState(163). Its clip (LandedAnimation 0x423730): landType 0 → 308 / 309 (+0xF1),
  1 → 306 P_LANDED, 2 / 3 → 307 P_LANDED_FROM_BACK.
- **CHECK_INTERACT_WITH_FIELD 173** (0x757590): the target (+0x118) field's player (its town's) is the villager's and
  SetFarmerGotoField(field, 1) → 1 (creature empathy TownDesire 0, 0.5); else 0. **174 FISH_FARM** (0x757610): the
  same with VillagerBecomesFisherman. **172 ABODE** (`Villager::CheckInteractWithAbode` 0x757420):
  built-and-not-needing-repair or another player's → CheckMoveHouse 0x757180, another player's (+0x11C = its town,
  163), workshop (163), its own home (GoHome); else (same player; another town only for a town centre →
  ForceMoveVillagerToAbode) SetupBuildingObject 0x758530 (`SetupBuildingObjectForBuilding`) == 1 → 1, else
  DecideWhatToDo. These states are only reached from the disciple's inspection. The inspection states 171-175 CHECK_INTERACT_WITH_* are bw1-decomp's `VillagerInteract.cpp`
  (0x757260..0x757740); they have no entry or exit function, and their state function reads the inspected object
  (+0x118) and sets the villager to work at it.
- **+0x4C**: the y angle `Object::SetYAngle` 0x639260 keeps and GetWorldMatrix 0x638200 reads (AngleY, no pi / 2).
- **No body** (`Living::InitialisePhysicsFromHand` 0x5EFDF8 → EndPhysics(NULL)): the villager jumps to 0x5F0B79 (no
  pose / empathy / SetYAngle, +0xE0 & 0x20 kept), the animal to 0x5F0DF9 (no SetYAngle); then the same code as with
  a body (`Object::EndPhysics`, landType 3, the water / death branches without a player, LANDED). Drawn from +0x4C
  (the hand's tilt goes).
- **Out of the hand**: Villager / Living / Animal vt +0x704 is `Object::InterfaceSetOutMagicHand` 0x637670 (the
  FireEffect's SetOutMagicHand).
- `Living::MoveToPos` 0x5EC270, state 1: MobileWallHug::MoveTo (0x5EC280) and, only on 0xA, SetTopStateToFinal
  (0x5EC287..0x5EC28E); it returns MoveTo's result (0x5EC293).
- Landed 0x7606E0, the creature part (0x760707..0x760756, on the first turn, +0x90 == 1):
  GetInterfaceStatusWhoLastDroppedMe 0x63A710, then for its player ConsiderMakingCreatureMimicPlayer(status, disciple
  + 0x15, this, 0) 0x760744 when disciple + 0x15 < 0x2E, and fn_00414600(player + 0x60, player, this) 0x760756 when
  +0x10C != disciple.
- Landed's disciple branch, when FindCloseObjectsForInteract finds something: first the town change
  (g_DiscipleInfos +0x18, CheckMoveIntoTown 0x757210, ChangeTribeIfRequired 0x7618C0). Then the record's start state
  if != 0, else SetupInspectObject 0x76AAB0 (+0x118 = the object, SetupMoveToWithHug(…, 233) or SetTopState(233));
  1. The reaction removed is RemoveAllReactionsOfTypeInitiatedByObject(this, 0x20). `Villager::InHand` (0x76AE27)
  also calls FindCloseObjectsForInteract.
- 173 / 174: `Field::GetPlayer` 0x528940 and `FishFarm::GetPlayer` 0x52C850 are their town's (two "none" count as the
  same). The creature empathy is MakeCreatureEmpathiseWithPlayerTownDesire(0, 0.5, Pos) 0x4C80F0; 172's is (5, 0.5,
  Pos). 172 also needs `Abode::GetPlayer` 0x405F70 (town-less `GameThing::GetPlayer` 0x570130) and the other player's
  town pointer at +0x11C (0x757527).
- `VillagerDead`: 0x750722..0x75073C: owner = GetPlayer (0x7502F0), else the neutral player (g_game +0x18 + 0xA60 x [+0x205A5B]); 0x750740..0x750760: killer none -> the neutral player
- `GetPlayerOf`: Villager::GetPlayer 0x7502F0: town ? Town +0x2C : none
- `GoTowardsDeadPerson`: 0x7667C7..0x7667DD: SetupMoveToWithHug(that point, 0xCE)
- `CheckInteractWithField`: 0x7575B1..0x7575C6: field->GetPlayer (Field 0x528940: its town's, +0x118) == GetPlayer (Villager 0x7502F0)
- `CheckInteractWithFishFarm`: 0x757631..0x757646: farm->GetPlayer (FishFarm 0x52C850: its town's) == GetPlayer
- 173 CHECK_INTERACT_WITH_FIELD: Villager::CheckInteractWithField 0x757590: the field +0x118 (dynamic_cast<Field>, read without a null test, 0x7575AF) of the villager's player (Field::GetPlayer 0x528940 = its town's; the villager's 0x7502F0 = its town's; else 0
- `CheckInteractWithFishFarm`: else 0

### Injured villagers (the town's count)

- `Villager::SetLife` 0x756B40 is the villager's only life setter (ReduceLife, IncreaseLife, ApplyEffect and the
  scripts all reach it). Before it calls `Object::SetLife` 0x63A140 it compares the old life (`GetLife`, vt +0x11C)
  and the new one with the **float** 0.7 at 0x8AB238 (a `dword ptr` operand, so 0.7f):
  - old > 0.7 (`fcomp; test ah, 0x41; jne`, so not for an equal or unordered old life) and new < 0.7 (`test ah, 1`)
    → fn_00756BC0: `GetTown` (vt +0x48), and with a town `inc dword [town + 0x714]` (0x756BC9);
  - otherwise old < 0.7 and new > 0.7 → fn_00756BD0: `dec dword [town + 0x714]` (0x756BD9), with **no test** of the
    count;
  - a life of exactly 0.7 before or after changes nothing; no town, nothing.
- These are the only writers of Town +0x714. Its only reader is `CreatureMental::GetNthActionForPlan` 0x4F184A:
  `cmp [town + 0x714], 0; jle`, a signed test, so the count is a 32-bit signed integer and a negative count reads as
  "no injured". It goes below 0 when a villager already under 0.7 joins a town and then recovers (the join does not
  count it).
- The count is made before the life is written, so it also counts a life `Object::SetLife` then refuses (an
  indestructible object at or below 0.01, see [Object life](../magic/influence-and-alignment.md#object-life)).
- `GetPlayerOf`: 0x7502F0: GetTown ? Town +0x2C : 0
- `FindChildrenAndOrphanThem`: 0x756BE7: GetTown (vt +0x48); none -> nothing

### Death

- **DeleteDependancys 0x74FD60**: after the SET_DYING, the TOP is read again (0x74FD9F); not 29 MOVE_ON_PATH and on
  a footpath node (+0xCC) that lists it, every entry of it is unlinked (0x74FDB2..0x74FDFB); then +0xCC = 0 always
  (0x74FE05), the footpath (+0xC8) kept.
- **VillagerDead 0x7506C0** (reason, killer, amount, drop): nothing while the villager flies (+0x24 & 0x40) or once
  dead (`Living::IsDead` 0x417270: status & 1 or TOP 15). Then: the help sprites (table 0x99A368, A / B / C per
  reason: KillingPeople when the killer is the local player, else DeathInVillageSFX when the owner is; with a town
  WorshippersDying for CHANT, LosingVillagers above GTownInfo +0x150 adults, else LowOnPeople), the drops
  (CreateDroppedResource only with `drop`; DropWood / DropFood always), the **owner's** alignment (GPlayerInfo +0x20
  + 4r, twice for a child, no ScaleChange; a villager without a town changes none), the town's counters (+0x38,
  +0x5C[0], +0x7C[reason], +0xA4[killer, none = neutral], +0xD8 = the turn, +0xDC), the town's pulse (+0x5EC = 0,
  +0x5E8 = 1), SetDying and then the reason (+0x118).
- **SetDying 0x76A4C0** (also row 13): life 0, SetTopState(14) (the exits of the state left run; the dying clip is
  chosen with the landType of the last landing), status |= 1, DeleteDependancys (out of the abode and town, a
  mother's orphans), status |= 0x30, the counter = 600 turns (120 turns with a functional graveyard in the town), out of the
  world population (+0xE0 & 0x40).
- **14 DYING 0x76A570**: SACRIFICE → 15 at once; else PlayAnimThenSetState(15): 23 WAIT_FOR_ANIMATION plays the
  dying clip (253 P_DYING, 246 P_DEAD2 after landing on the left side, 283 in the water). Not at home:
  REACT_TO_DEATH. It runs the same turn as the death (ProcessState calls CallState after CheckEveryTime).
- **15 DEAD 0x76A5E0**, then `Living::Dead` 0x5EC400: a fire on it goes; not script-controlled: on the first DEAD
  turn a smoke puff (`SmokyStuff`, half its height up), out of the water a soul (a child's ChildMeshHigh forced to
  heaven, else StdDetail; Random(0, 100) < 50 heaven; the clip pair 244 / 245, or 247 / 248 when the corpse's clip is
  named "M_P_DEAD1", literal) and the mesh becomes the skeleton 0x1FF PersonSkeletonMale (every turn). The counter
  runs down one per turn (shared with the animals); at 0 (counter + 1 turns) or for SACRIFICE a second puff and
  `Villager::ToBeDeleted`. A script-controlled corpse never smokes, never turns skeleton and never times out (except
  7). The dead clip is 243 P_DEAD1 (landType 3 after SetDying), 246 for a corpse thrown back on its left side, 249 in
  the water. DEAD's exit is CannotExitState 0x768640 (only IN_HAND 24, FLYING 10 or a state with the same exit).
- **Corpses, the hand and the physics**: a corpse in 15 (or playing its dying clip, FINAL 15) is available and
  reachable (IsAvailable is false only with FINAL 14). In the hand or flying it does not count down. Landing on land
  (`Villager::EndPhysics` 0x5F0A60): the landType (the turn-start matrix's right row y: < -0.5 → 1, > 0.5 → 2, else
  0) into status bits 4-5; life <= 0: already dead → 15 with its remaining counter, else VillagerDead(5
  PLAYER_INTERACTION, the hand's player); then the landType again. In the water: dead → 14 with 600 (never the
  graveyard's time), else VillagerDead(6). A villager killed in flight (impact, spell, fire) dies at rest with reason
  5 / 6 (literal). `Villager::HasSunk` 0x750AB0: not available (FINAL 14) → not sunk; the status bit alone (+0xB4 &
  1, not IsDead) → 14, 600; else DROWNING.
- **A predator's pounce** (fn_005EC480): every Living, a corpse too, gets the downed mark, life 0.05 and
  SetTopState(17) (`Villager::SetTopState`); DEAD's exit refuses the state of a corpse (to check in the game).
- **Mourning**: REACT_TO_DEATH (23) spreads once over 60 m. Priority (0x766440) 100 unless my town has a functional
  graveyard, I am the dead one or 10 follow it now (reaction +0x1C: AddReaction `inc`, StopReacting `dec`). Setup
  (0x7665B0): GameRand(2) 0 → 205 with the counter 0, 1 → 206. 205 points (clip 395) once facing it, for
  ftol((GameFloatRand(20) + 2) x (1000 / ms per turn = 10)) facing turns (x87 precision, one truncation); 206 walks to
  4 m (maxDistanceToRunAwayFromObject) from it when farther than 4.8 m; 207 turns to face it; 208 mourns (285 → 313 →
  321) for ftol((GameFloatRand(3) + 4) x 10) turns, then StopReactingAndSetState. The exit is ExitReaction, the
  validate ReactionValidate (the corpse gone, unavailable or in the hand → PopFromPrevious).
- **Orphans**: FindChildrenAndOrphanThem 0x756BE0 (DeleteDependancys of a woman, `Town::RemoveVillager`): every
  villager of the town's abodes and homeless list whose mother she is → 131 MORN_DEATH (GoHome with the mourning
  clips) if available, and the mother link cleared (grown-up children too, literal).
- **Deletion**: `Villager::ToBeDeleted` 0x7521B0 (no death: no reason, counters or corpse): DeleteDependancys
  0x74FD60 (SET_DYING through the real exits unless already 13-15, the orphans, out of the abode / town /
  vagrants), then `Living::ToBeDeleted` 0x5EC0A0's StopReacting (any reaction, the mourning too).
  `Town::RemoveVillager` 0x73E210 calls RemoveVillagerFromWorshipSite 0x76C440 when the villager is at the site
  (+0xE0 & 2), before SetTown(0). The beam (Explosion) deletes.
- **Callers**: CheckEveryTime (EXHAUSTION / CHANT, owner), CheckHungry (STARVING / CHANT, owner), old age (9, none),
  Drowning and EndPhysics in the water (6), EndPhysics on land (5), BeingEaten (3, owner: then a normal corpse),
  ReduceVillagerLifeByChant (4, owner; GET_TOWN_WORSHIP_DEATHS reads the count of reason 4), DestroyedByEffect (2:
  spells with the effect's player and damage, fire with its player and 0 (0x72F506), tornado (player, 1.0),
  impacts).
- The graveyard: Town +0x748 and its IsFunctional (SetDying's 120 / 600, ReactToDeathPriority); fn_0073E440 adds the
  dead to the graveyard when +0x748 is set (0x73E48B; the graveyard tests IsFunctional and the 50 itself).
- VillagerDead: the multiplayer call FUN_0064DA80(0, 1) on the killer runs only for a reason not in {0, 1, 3, 4, 8,
  9} and only when IsMultiplayerGame 0x552F80. The owner falls back to the neutral player g_game +0x18 + 0xA60 ×
  [+0x205A5B], and so does a missing killer. The town's adults (+0x618) are read before SetDying, so the dying
  villager still counts. `GAlignment::Update` 0x4143B0 does nothing without a player. fn_0073E0A0 adds the amount to
  Town +0xA08[killer × 32 + reason]. The TownStats counters are fn_00749780 (via fn_0073E440): the death counts by
  reason, TownStats (Town +0x610) +0x7C[reason] ++ (0x7497DA). Only VillagerDead writes them; the TownStats
  constructor fn_007491F0 zeroes them (0x74921C, 0x74922E, 0x749234 and the `rep stosd` 0x7492AE..0x7492CD) and the
  per-turn recount of TownStats does not touch them. The reader found is `GetDeaths` 0x740D70 (+0x7C[4], for
  [GET_TOWN_WORSHIP_DEATHS](../magic/worship.md)); readers relative to TownStats were not searched. For SACRIFICE the
  counter +0x58 = 0.
- HelpSpritesLowOnPeople is 0x71CBE0. The on-screen test of the help is fn_0071CE70 (IS + 0x30) on the villager's
  bounding box (fn_0081F1A0).
- DestroyedByEffect 0x7502D0 is vt +0x5F8, called from `Object::ApplyEffect` when the life reaches 0.
  `Villager::SetLife` is 0x756B40 (vt +0x5B0).
- SetDying: DyingTimeWithoutGraveyard = info +0x290 (600), DyingTimeWithGraveyard = +0x294 (120), low words. The
  counter is set also when the villager was already dead.
- Dying 0x76A570: REACT_TO_DEATH is created only "not at home and (no town or no graveyard)", but SetDying has
  already removed the town, so the graveyard test never stops it (literal). The call is CreateReaction(me, 23, no
  player, 0).
- Dead 0x76A5E0: the soul is fn_00828790, made only out of the water. The child mesh is +0x204 ChildMeshHigh (heaven
  forced), the adult mesh `GVillagerInfo::GetMesh` 0x74F880 (+0x214 StdDetail). The skeleton mesh is the static
  0xDCB164 = MeshPack[0x1FF], set every turn through vt +0xF4, then the 3D object's (+0x40) vt +0xC4 is called with 0
  (0x76A763..0x76A76A). In all eight LH3DObject vtables (0x9A22E8, 0x9A251C, 0x9A2748, 0x9A2974, 0x9A2BFC, 0x9A2E34,
  0x9A3068, 0x9A32A0) that slot is fn_007F99F0, which sets or clears the drawn object's flag 0x04000000 (+4);
  `Villager::CallVirtualFunctionsForCreation` sets it (0x74FD47), Dead clears it. It is not a reset of the 3D object.
- `Graveyard::MakeFunctional` 0x595E00 sets Town +0x748 and `Graveyard::DeleteDependancys` 0x595CE0 clears it.
- DeleteDependancys 0x74FD60: a villager walking a footpath (TOP != 29 MOVE_ON_PATH and +0xCC set) leaves the
  footpath's walker list (+0x28, count +0x2C), +0xCC = 0. With a home it calls
  `Abode::RemoveDeletedVillagerFromAbode` 0x404220, which itself calls `Town::RemoveVillager` when the home has a
  town; else `Town::RemoveVillager` 0x73E210; else out of the vagrants (g_game +0x205BFC). SET_DYING runs ExitAtHome
  (row 13's 0xC0 is 0: LeaveHome).
- `Living::ToBeDeleted` 0x5EC0A0 also frees the data path (+0xAC) and the script remind (+0xB0) and leaves a dance
  (vt +0x978 / +0xB08) before the flock. `Object::ToBeDeleted` is 0x636670. `Living::Dead`'s tail:
  CreateSmokyStuff(0, 1.0, 0xFFFFFFFF), ToBeDeleted(0), returns 5.
- The soul (fn_00828790 / fn_00828900 / fn_00828950 / fn_00828990): a 12-byte record {next, LH3DObject*, elapsed ms}
  at the head of the list 0xEB9A7C, `LH3DObject::Create(2)` 0x80B4D0. It takes the source's drawn position and
  orientation with scale 1. Random(0, 100) (0x81D180, the CRT stream) is always drawn, before the clip name is
  tested. Alpha 105 (colour alpha << 24 | 0xFFFFFF, white). In the last 500 ms: ftol((1 − (elapsed − (duration −
  500)) × 0.002) × 105). Freed when elapsed + 110 > the clip's duration (anim +0x20). Updated every frame by
  fn_00828950 from fn_005E5CD0 (0x5E6171) with g_game_time_inc. The clip statics are 0xEB9A80 / 0xEB9A88 (244 / 245)
  and 0xEB9A84 / 0xEB9A8C (247 / 248), and the name "M_P_DEAD1" is 0xC38664.
- Mourning: a creature as the dead initiator gets REACT_TO_DEATH's priority (byte 0xD4FFBC) without the graveyard /
  self / 10-mourner tests, and in the setup the villager takes reaction 7 LOOKING_AT_OBJECT_REACTION instead of 205 /
  206. 206 walks only when 1.2 R < d and d − R > 1, to me + the step of the whole metres of d − R towards the dead
  (fn_0074E1D0 / fn_0074D3E0). R = ReactionInfo +0x44 (4.0 for row 23). The point and mourn rolls are drawn every
  facing / mourning turn.
- MakeChildOrphaned 0x7580D0: the child's mother is not her → 0; IsVillagerAvailable → SetTopState(131); mother = 0;
  1. FindChildrenAndOrphanThem walks the structures (+0x754, next +0x9C), their inhabitants (+0xA0, next +0xE4) and
  the homeless list (+0x768).
- Town +0x748, the town's graveyard (graveyard::GetGraveyard; set by Graveyard::MakeFunctional 0x595E00, cleared by Graveyard::DeleteDependancys 0x595CE0) and its IsFunctional (vt +0xD4, abode_queries::IsFunctional): nullopt none, else whether it is functional. Read through the villagers' world queries
- MeshPack[0x1FF] (0x76A611 / 0x7562E2): PersonSkeletonMale, for men, women and children alike
- fn_0071CE70(IS + 0x30, me): the villager's object on screen (fn_0081F1A0, its bounding box). ⚠️ approximate its position through GameQueries::pointOnScreen (fn_0081F1D0): the queries have no bounding-box test
- `PlayDeathHelp`: 0x71CBE0
- `LivingDead`: 0x5EC403..0x5EC417: a flock (+0xB8) -> RemoveLivingFromFlock(this, 1), SetFlock(0) (ECS/Flocks.h)
- `LivingDead`: 0x5EC447..0x5EC45F: CreateSmokyStuff(0, 1.0, 0xFFFFFFFF), ToBeDeleted(0) (vt +0xC = Villager::ToBeDeleted 0x7521B0); 5
- `HelpFor`: 0x750770..0x7507B8: the killer is the local player -> A; else the owner is -> C
- `HelpFor`: 0x75085B..0x7508EF: CHANT -> the owner local: worshippers dying; else adults (+0x618) > GTownInfo +0x150 (unsigned, jbe) -> B, the killer not the owner and the owner local: losing villagers; else the owner local: low on people
- `DyingTime`: 0x76A523 `mov cx, word [eax + 0x294]` / 0x76A533 `mov ax, word [edx + 0x290]`: the low words
- `VillagerDead`: 0x7506C3: +0x24 & 0x40 (in the physics): nothing; EndPhysics 0x5F0A60 kills it at rest with reason 5 / 6
- `VillagerDead`: 0x7507C4..0x7507E2: drop != 0 -> CreateDroppedResource(0, 0, 0) 0x750940; then DropWood(0) 0x751240 and DropFood(0) 0x7511E0 always (the town's carried totals go back)
- `VillagerDead`: 0x7508FD..0x750923: not dead yet (+0xB4 & 1) -> SetDying (vt +0x6A4);
- `DestroyedByEffect`: 0x7502D0..0x7502E3: VillagerDead(2 SPELL, player, amount, 1); 1
- `SetDying`: 0x76A4EB SetTopState(14 DYING): the exits of the state left run and the dying clip is chosen now, with the landType of the last landing (the 0x30 below comes after).
- `SetDying`: 0x76A506..0x76A53A: the counter (+0x58) = the town's graveyard (+0x748) functional (vt +0xD4) ? DyingTimeWithGraveyard (+0x294) : DyingTimeWithoutGraveyard (+0x290); also when it was dead already
- `SetDying`: 0x76A53E..0x76A558: not counted out (+0xE0 & 0x40) -> --g_game +0x205A54 (the world population) and flags |= 0x40.
- `Dying`: 0x76A575..0x76A593: GetDeathReason (vt +0x444) 7 SACRIFICE -> SetTopState(15) (no dying clip); else PlayAnimThenSetState(15, 1) 0x5ECAC0: 23 WAIT_FOR_ANIMATION until the dying clip has played, then 15
- `Dead`: 0x76A5F9..0x76A620: the static 0xDCB164 = MeshPack[0x1FF], once
- `IsDead`: 0x417287: IsFunctional (vt +0xD4) != 1. Villager::IsFunctional 0x751CF0: IsAvailable (vt +0x2C, 0x751D50) == 1 and TOP not in 13..14 (SET_DYING, DYING: cmp 0xD / jb, cmp 0xE / jbe)
- `SetSkeleton`: 0x756328..0x75633D: obj3d SetMesh(MeshPack[0x1FF])
- `SetSkeleton`: 0x756429..0x756436: SetScaleForAge(GetAge) (draws GameFloatRand)
- `DeleteDependancys`: 0x74FD9D..0x74FE05: TOP != 29 MOVE_ON_PATH and a footpath (+0xCC): out of its walker list (+0x28, count +0x2C); +0xCC = 0.
- Villager::ToBeDeleted 0x7521B0: 0x7521B3 DeleteDependancys 0x74FD60, then 0x7521BF Living::ToBeDeleted 0x5EC0A0: 0x5EC0AA..0x5EC0BA a reaction (+0x94) -> StopReacting (vt +0x998: villager_reactions::StopReacting, the fire's, teleport's, shield's and the mourning's); out of the Living list; a dance (vt +0x978 / +0xB08, pending); then the flock.
- `ToBeDeletedOverride`: 0x5EC166..0x5EC17B: a flock (+0xB8) -> RemoveLivingFromFlock(this, 1), SetFlock(0) (ECS/Flocks.h)
- `DeadTick`: 0x5EC41E: not controlled by a script (+0x25 & 4): 0x5EC425..0x5EC437 ax = +0x58; +0x58 = ax - 1; ax == 0 -> vanish
- `DeadTick`: 0x5EC439..0x5EC443: GetDeathReason (vt +0x444) == 7 SACRIFICE -> vanish
- Villager::SetDying 0x76A508..0x76A53A: a functional graveyard in the town -> DyingTimeWithGraveyard (+0x294, 120), else DyingTimeWithoutGraveyard (+0x290, 600)
- Row 13 SET_DYING: StateSetDying 0x5AFF40 = jmp [vt +0x6A4] = SetDying
- Row 14 DYING: StateDying 0x5AFE30 = jmp [vt +0x89C] = Villager::Dying 0x76A570
- Row 15 DEAD: StateDead 0x5AFE90 = jmp [vt +0x8A0] = Villager::Dead 0x76A5E0, then Living::Dead 0x5EC400. 1, or 5 once the corpse went (the entity is gone then)
- Living::IsDead 0x417270: status & 1, or TOP 15 DEAD, or not functional (vt +0xD4, Villager::IsFunctional 0x751CF0: IsAvailable and TOP not in 13..14)
- Villager::SetSkeleton 0x7562C0 (SET_SKELETON, the constructor's arg 4): the status bit 0x40, the mesh (0x75633D the skeleton MeshPack[0x1FF], else the villager's own meshes) and 0x756436 SetScaleForAge.
- VillagerDead's first test (+0x24 & 0x40, 0x7506C3): in the physics.
- Whether the town has a functional graveyard (the villagers' world queries; Villager::SetDying 0x76A50A..0x76A51E, ReactToDeathPriority 0x766469..0x766486)
- Living::Dead 0x5EC400's counter (0x5EC41E..0x5EC443), shared by the villagers and the animals: not controlled by a script (+0x25 & 4) -> c = counter, counter = c - 1, c == 0 -> vanish (`test ax` before the decrement); then reason 7 SACRIFICE -> vanish
- fn_0074E1D0 / fn_0074E200 (the same code): ftol(fild(ftol(m)) / 10 [0x99A1BC] x 65536 [0x8AC408]): the whole metres (truncated) as a whole distance (each x87 step rounded to float: 24-bit control word, 0x7DEE0D)
- `ReactToDeathPriority`: 0x766447..0x766455: the initiator (+0x14) IsCreature (vt +0x34) -> the priority
- `ReactToDeathPriority`: 0x766462..0x766488: my town (vt +0x48) with a graveyard (+0x748) that IsFunctional (vt +0xD4) -> 0
- `ReactToDeathPriority`: 0x76648A: I am the initiator -> 0; 0x76648E: reaction +0x1C >= 10 (unsigned jae) -> 0
- `SetupReactToDeath`: 0x7665B8..0x7665CC: a creature initiator -> AddReaction(r, 7 LOOKING_AT_OBJECT_REACTION) (vt +0x990)
- `SetupReactToDeath`: 0x7665CE..0x7665F5: GameRand(2) (VillagerReaction.cpp 0x726): 0 -> +0x58 = 0 (ax), AddReaction(r, 0xCD); else AddReaction(r, 0xCE)
- `SetupReactToDeath`: 0x766603..0x766608: +0xBC = the dead villager
- Living::StopReacting 0x5F1140: with a reaction (+0x94), unlinked from its list (+0x18) and 0x5F1180..0x5F1187 `dec [edi+0x1C]` (one taker fewer: the cap of ReactToDeathPriority counts the current mourners), then fn_005F0FE0 gives its record the turn; +0x94 and +0xBC go
- `PointAtDeadPerson`: ReactionValidate 0x756A03 asks IsAvailable
- `PointAtDeadPerson`: 0x766698: ++counter (+0x58); 0x7666AB GameFloatRand(20) (VillagerReaction.cpp 0x744), drawn every facing turn
- `GoTowardsDeadPerson`: ReactionValidate 0x756A03 asks IsAvailable
- `GoTowardsDeadPerson`: 0x76676C GetAngleFromXZ(me, the dead one); 0x766795..0x7667B2: fn_0074E1D0 / fn_0074E200 (d - R) then fn_0074D3E0 / fn_0074D400 (angle, that): me + the step of the whole metres of d - R towards it
- `LookAtDeadPerson`: ReactionValidate 0x756A03 asks IsAvailable
- `MournDeadPerson`: 0x76685B ++counter; 0x766869 GameFloatRand(3) (VillagerReaction.cpp 0x77A), drawn every turn
- `MournDeadPerson`: 0x766899..0x7668A5: (i16) counter > t -> StopReactingAndSetState (vt +0x99C)
- `FindChildrenAndOrphanThem`: 0x756BF1..0x756C27: the structures (+0x754, next +0x9C), each one's inhabitants (+0xA0, next +0xE4); the next is read after MakeChildOrphaned (which unlinks nothing)
- `FindChildrenAndOrphanThem`: 0x756C29..0x756C4C: the homeless list (+0x768, next +0xE4)
- `MakeChildOrphaned`: 0x7580D3..0x7580DD: mother (+0x100) is not her -> 0
- `MakeChildOrphaned`: 0x7580DF..0x7580F3: IsVillagerAvailable 0x752290 -> SetTopState(0x83 MORN_DEATH) (vt +0x8E8). No age test: grown-up children mourn too (literal)
- `MakeChildOrphaned`: 0x7580F9: mother = 0; 1
- PointAtDeadPerson 0x76669C..0x7666D6: ftol((GameFloatRand(20) + 2) x (1000 / msPerTurn [0xD01A38] = 100, an unsigned division)) facing turns
- MournDeadPerson 0x766854..0x766894: ftol((GameFloatRand(3) + 4) x (1000 / 100)) turns
- Villager::ReactToDeathPriority 0x766440: a creature initiator -> REACT_TO_DEATH's priority (byte 0xD4FFBC); else my town has a functional graveyard (+0x748, vt +0xD4), I am the dead one, or 10 Livings already took it (reaction +0x1C) -> 0; else the priority
- Villager::FindChildrenAndOrphanThem 0x756BE0: every villager of the town's structures (+0x754, their inhabitants) and of its homeless list (+0x768) whose mother (+0x100) is `mother` -> MakeChildOrphaned 0x7580D0
- Villager::MakeChildOrphaned 0x7580D0 (mother): the child's mother (+0x100) is not her -> 0; IsVillagerAvailable 0x752290 -> SetTopState(131 MORN_DEATH); mother = 0; 1
- The source's current clip name (LH3DObject vt +0x184 -> the anim's name) is "M_P_DEAD1" (0xC38664, the strcmp 0x82882C..0x828856)
- `SoulClip`: 0x828859..0x8288D1: the pair by the name, heaven when roll < 50 (fcomp [0x8C6CA4]; test ah, 1)
- `SoulClip`: 0x82888D..0x8288A0 / 0x8288D7..0x8288EA: heavenForced -> SetAnim(the heaven clip) again
- `SoulAlpha`: 0x8289DD..0x8289EA: eax = duration - 500 (0x1F4); elapsed > eax (signed jle) -> the fade
- `SoulAlpha`: 0x8289EC..0x828A08: fild (elapsed - start); fmul 0.002 (0x8C78E8); fsubr 1.0; fmul 105.0 (0x9A3960); __ftol (each x87 step rounded to float: 24-bit control word, 0x7DEE0D)
- `SoulExpired`: 0x8289B0..0x8289B5: lea ecx, [edi + 0x6E]; cmp ecx, duration; jle -> stays
- `Create`: 0x82881D..0x8288EA: the clip; Random(0, 100) (0x81D180, the CRT stream) drawn always, before the name test is used
- `Update`: 0x8289A5..0x8289B5: elapsed + 110 > the clip's duration -> freed (fn_00828900: the LH3DObject released)
- `Update`: 0x8289CD..0x828A2E: the alpha and SetColour(alpha << 24 | 0xFFFFFF); drawn (vt +0x100)
- fn_00828790 0x828821..0x8288EA: Random(0, 100) (the CRT stream, drawn always) < 50 (fcomp; test ah, 1) -> heaven, else hell; the source's current clip name (vt +0x184) "M_P_DEAD1" (0xC38664) -> the pair (247 P_DEAD2_GOTO_HEAVEN, 248 P_DEAD2_GOTO_HELL) (statics 0xEB9A84 / 0xEB9A8C), else (244 P_DEAD1_GOTO_HEAVEN, 245 P_DEAD1_GOTO_HELL) (0xEB9A80 / 0xEB9A88), set by Villager::Dead 0x76A677..0x76A6DA; heavenForced (a child) -> the heaven clip after the roll
- fn_00828990 0x8289D2..0x828A0D: 105 (0x69); in the last 500 ms (elapsed > duration - 500, signed) ftol((1 - (elapsed - (duration - 500)) x 0.002) x 105)
- fn_00828990 0x8289AD..0x8289B5: elapsed + 110 > duration (signed jle) -> the soul is freed
- `DyingClip`: 0x42377F: 0x11B; 0x423786..0x423798: (landType bits 0x30 - 0x20) != 0 -> 0xF6 + 7, else 0xF6
- `DeadClip`: 0x4237AF: 0xF9; 0x4237B6..0x4237C8: != 2 -> 0xF6 - 3, else 0xF6
- `SetSkeleton`: 0x7562F6..0x756315: status bit 6 = on & 1
- ~Villager 0x74FBC0: not counted out (+0xE0 & 0x40) -> --g_game +0x205A54.
- Villager::GetDeathReason 0x55CB10: +0x118
- Living::IsSkeleton 0x416FF0: status & 0x40
- `TurnsPerSecond`: 1000 / [0xD01A38] (game_clock::MsPerTurn, 100): the unsigned `div` of 0x7666B7 / 0x766875 (the original faults on 0; 0 here)
- `PointTurns`: 0x7666C5 fadd 2.0 (0x8AB478); 0x7666D2 fimul (1000 / [0xD01A38]); 0x7666D6 __ftol (truncation): on the x87 stack until the one truncation, each step rounded to float (24-bit control word, 0x7DEE0D)
- `MournTurns`: 0x766883 fadd 4.0 (0x8AB418); 0x766890 fimul (1000 / [0xD01A38]); 0x766894 __ftol: float steps on the x87 stack (24-bit control word, 0x7DEE0D)
- `ReactToDeathPriority`: byte 0xD4FFBC
- `ApplyReaction`: Living +0x94: it already follows a reaction (0x6E40D0).
- `ApplyReaction`: 0x6E4109: +0x2C = the turn if still 0
- `GoTowardsDeadPerson`: 0x76671B..0x76672C: R = GetReaction->GetInfo +0x44 (maxDistanceToRunAwayFromObject, 4.0 for row 23)
- `GoTowardsDeadPerson`: 0x766730..0x766743: d = fn_0074CD50 (GetDistanceInMetres) from me (+0x14) to the dead one (+0x14)
- `GoTowardsDeadPerson`: 0x766747..0x76675B: 1.2 R < d (fcompp; test ah, 1) and 0x766771..0x76678D: d - R > 1 (test ah, 0x41)
- `PointAtDeadPerson`: 205 POINT_AT_DEAD_PERSON 0x766680
- `GoTowardsDeadPerson`: 206 GO_TOWARDS_DEAD_PERSON 0x766700
- `LookAtDeadPerson`: 207 LOOK_AT_DEAD_PERSON 0x766810
- `MournDeadPerson`: 208 MOURN_DEAD_PERSON 0x766850

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `VillagerStates.cpp`, `Living.cpp`; the rest read in the exe with the xref
  index.

## openblack

**Done**, except the disciples' interaction after a landing and the empathy of a creature (no creature yet).

- **Picked up or thrown**: `villager_physics::InterfaceSetInMagicHand` and `villager_physics::VillagerStartPhysics`
  (`VillagerPhysics.cpp`), both through `ecs::SetVillagerStateWithExit` (`VillagerAnimations.cpp`), which calls
  `LivingActionSystem::VillagerSetState` without skipTransition, which is `villager::SetTopState` (already in the
  state: nothing), and only then drops the walk and redraws the villager where it is: the exit comes first, as in
  `Living::SetTopState`. No ported exit reads the walk (its tags, the goal, the drawn position), so the order changes
  nothing yet. In the hand's pick-up openblack takes the villager out of the physics before this and out of the map
  cells after it; no ported exit reads the physics entry or the map cells, so that order changes nothing yet
  either. Before the exits ran, openblack kept the reaction, and when the reacting thing's physics ended,
  StopReactingAndSetState gave the villager its resume state in the hand or in the air. Tests in
  `test/test_villager_core.cpp`.
- **The hand's exit**: IN_HAND's row has the exit `VillagerExitInHand` (`LivingActionSystem.cpp`), with
  `villager_animation::HandLetsGoInto` (10, 11 or `IsDeathState`). The changes that skip the exits
  (`LivingActionSystem::VillagerSetState` with skipTransition: the gentle put-down's LANDED, the water's) run it too
  when they leave IN_HAND, as the original's `SetTopState` does there; a refusal keeps the villager in the hand. None
  of the exits openblack has refuses 24 or 10. Tests: `test_villager_animation`
  TheHandLetsAVillagerGoOnlyIntoAFlightALandingOrADeath, `test_living_turn`
  TheHandLetsGoOnlyIntoAFlightALandingOrADeath.
- **Landing**: `VillagerPhysics.{h,cpp}` (EndPhysics, the pure pose functions, InterfaceSet{In,Out}MagicHand,
  `villager_physics::SetLandedState`), `VillagerCore` (`villager::SetYAngle`), `VillagerAnimations`
  (`VillagerLandedClip`), LANDED's state function (`LivingActionSystem.cpp`), `Villager/VillagerInteract.{h,cpp}`
  (173 / 174); it uses `affine::ArcTanOctant` / `GetYAngle` / `WrapAngle`, `PhysicsObjects::SyncTurnStart` and the
  late FLYING of [Physics](../physics/physics.md). Tests `test/test_living_landing.cpp`. From the hand the body's
  columns are the rows as they are (`living::BodyRows`). On the world path openblack's body is the drawn rotation,
  so right = -column 2, up = column 1, fwd = column 0 (`living::OriginalRows`, until the physics builds those bodies
  as the original) **(inferred, not verified in game)**: `OPENBLACK_VILLAGER_TRACE` / `OPENBLACK_ANIMAL_TRACE` print
  `landing: … (original right.y a, openblack column 0 y b) …`; a put-down on flat ground gives 0 / 0; a villager
  thrown so it lands on its side tells which of the two the clip 306 / 307 agrees with. openblack has the bit
  `Villager::k_StatusSpecial` but nothing sets it and the special test is not ported. The return's field is
  `field0xf4`, `state_info::GoesBackAfterLanding`; openblack sets both states straight (`ecs::SetVillagerState`), not
  through `SetTopState`'s exits, entries and pause. +0x4C is `living::ObjectYAngle`: openblack keeps only the drawn
  Transform = AngleY(+0x4C + pi / 2): exact when it is still the one drawn from the game angle (WallHug::yAngle =
  ConvertGameAngleTo3D(a) for villagers, ConvertGameAngleTo3D(AnimalBrain:: angle) for animals: the SetGameAngle
  writer), else 🔍 inferred the GetYXZ yaw of the drawn matrix's original rows. The body-less end is
  `living::EndPhysicsWithoutBody`; out of the hand is `living::InterfaceSetOutMagicHand`.
- **Injured**: `life::SetLife` (`ECS/Life.cpp`), `living::InjuredChange` and `k_InjuredLife` (`Physics/LivingRules`),
  `Town::injured`, an `int32_t` that is decremented with no guard, as the exe. openblack ports no refusal of
  `Object::SetLife`. No hash part reads the count, and nothing in openblack reads it yet (the creature's plans are not
  ported). Tests `test/test_living_impact.cpp` (`LivingInjured`, `AVillagersTownCountsItInjured…`).
- **Death**: `Villager/VillagerDeath.{h,cpp}` (VillagerDead, SetDying, the states 13 / 14 / 15, IsDead, GetPlayerOf,
  DeleteDependants (was DeleteDependancys), Delete, SetSkeleton, the shared `living::DeadTick`),
  `Villager/VillagerMourning.{h,cpp}` (REACT_TO_DEATH, 205-208, the orphans and 131), `Villager/VillagerSoul.{h,cpp}`
  (the soul, `villager_soul`), `Components/TownDeaths.h` (the town's death counters, `TownDeaths::byReason`, one per
  `DeathReason::_COUNT`: a component of its own because the TownStats recount does not touch them, assigned zeroed at
  a town's first death, which is the same as zero at the town's creation); the
  dead branches of `VillagerPhysics.cpp` (land) and `VillagerDrowning.cpp` (water); test
  `test/test_villager_death.cpp`. The footpath node is `components::LivingFootpath`'s, unlinked by
  `footpaths::RemoveEveryOccupant`. A flying villager is a flying body (`PhysicsObjects::IsFlying`). The owner's
  alignment is `effects::alignment::UpdateForDeath`; the world population (`magic::players::WorldPopulation`) skips
  the dying. The killer and owner are `std::optional<PlayerNames>` (none = neutral); the local player is PLAYER_ONE
  **🔍 inferred**. Deletion is `villager::Delete` = `ecs::ToBeDeleted`, whose villager branch calls
  `villager::ToBeDeletedOverride` when it is marked. A new map clears the mourning in MagicLoop's OnLoadMap. The
  graveyard is `graveyard::GetGraveyard(town)`, its IsFunctional `abode_queries::IsFunctional`, and
  `graveyard::AddDead`. The soul's alpha goes through `components::Alpha`, not the object colour 🔍 inferred.
- Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Villager/VillagerDeath.cpp`: `GraveyardOf`, `SkeletonMesh`, `PlayDeathHelp`, `LivingDead`, `HelpFor`, `DyingTime`, `DyingClip`, `DeadClip`, `VillagerDead`, `DestroyedByEffect`, `SetDying`, `Dying`, `Dead`, `CannotExitState`, `IsDead`, `GetPlayerOf`, `SetSkeleton`, `ToBeDeletedOverride`, `DeadTick`, `DeathReason::Chant`, `IsInPhysics`, `Villager::k_StatusDead`, `DeathReason::Spell`, `VillagerStates::Dying`, `HasFunctionalGraveyard`, `Villager::k_FlagCountedOut`, `DeathReason::Sacrifice`, `GetDeathReason`, `VillagerStates::Fly`, `VillagerStates::InHand`, `Villager::k_StatusSkeleton`, `SexType::Female`.
- `src/ECS/Villager/VillagerDeath.cpp`, `VillagerDead`: SACRIFICE: the killer's GameStats +0x1124 ++  and the counter (+0x58) = 0
- `src/ECS/Villager/VillagerDeath.cpp`, `SetDying`: ⚠️ approximate villager_reactions::SetTopState: it also ends openblack's walk, as MOVE_TO_POS' exit ExitMoveToPos 0x5EDDA0 would (not ported)
- `src/ECS/Villager/VillagerDeath.cpp`, `SetDying`: openblack counts the entities (magic::players::WorldPopulation skips the counted-out ones)
- `src/ECS/Villager/VillagerDeath.cpp`, `SetSkeleton`: 0x756348..0x756423: a child (GetAge < grownUpAge) its child meshes (+0x20C / +0x208 / +0x204), else GetDetailMesh(2 / 1 / 0): openblack's one mesh of the LODs (VillagerHome's SetVillagerMeshes)
- `src/ECS/Villager/VillagerDeath.cpp`, `DeleteDependancys`: (not ported) openblack's villagers walk no footpaths 0x74FE0B..0x74FE17: IsAMother 0x751110 (GVillagerInfo +0x1F8 sex == 1) -> FindChildrenAndOrphanThem 0x756BE0
- `src/ECS/Villager/VillagerDeath.cpp`, `ToBeDeletedOverride`: the data path (+0xAC) and script remind (+0xB0) (not ported); Object::ToBeDeleted 0x636670 is ecs::ToBeDeleted's tail
- `src/ECS/Villager/VillagerDeath.cpp`, `Delete`: Villager::ToBeDeleted 0x7521B0 (vt +0xC) through ecs::ToBeDeleted, whose villager branch is ToBeDeletedOverride. openblack counts the entities: the deleted one is no longer one
- `src/ECS/Villager/VillagerDeath.h`: `DyingTime`, `SetDyingState`, `Dying`, `Dead`, `IsDead`, `GetDeathReason`, `GetPlayerOf`, `IsSkeleton`, `HasFunctionalGraveyard`, `DeadTick`, `SetSkeleton`, `IsInPhysics`.
- `src/ECS/Villager/VillagerDeath.h`, `SetSkeleton`: (pending) no caller sets it yet: CHL SET_SKELETON is Intro's, and the constructor's arg 4
- `src/ECS/Villager/VillagerDeath.h`, `IsInPhysics`: ⚠️ approximate a flying body (PhysicsObjects::IsFlying: openblack's resting proxies are not counted, as Living::SetDying's test in ECS/AnimalAI)
- `src/ECS/Villager/VillagerMourning.cpp`: `TurnsPerSecond`, `WholeDistanceOfWholeMetres`, `PointTurns`, `MournTurns`, `ReactToDeathPriority`, `SetupReactToDeath`, `ApplyReaction`, `StopReacting`, `PointAtDeadPerson`, `GoTowardsDeadPerson`, `MournDeadPerson`, `FindChildrenAndOrphanThem`, `MakeChildOrphaned`, `VillagerStates::GoTowardsDeadPerson`, `VillagerStates::LookAtDeadPerson`, `LookAtDeadPerson`.
- `src/ECS/Villager/VillagerMourning.cpp`, `ApplyReaction`: Replacing it by a higher scoring one (0x6E4134, reactions::MaySwitch) is not ported for the villagers (as the fire's, teleport's and shield's)
- `src/ECS/Villager/VillagerMourning.cpp`, `PointAtDeadPerson`: 0x766686..0x766694: LookAtObject(+0xBC, 1) 0x5EC520 (LookAtPos of its position, step 0x80) facing it -> (guard) 0x766686 tests only +0xBC != 0;
- `src/ECS/Villager/VillagerMourning.cpp`, `GoTowardsDeadPerson`: 0x766708..0x766716: no +0xBC -> 0 (guard) 0x766708 tests only +0xBC != 0;
- `src/ECS/Villager/VillagerMourning.cpp`, `LookAtDeadPerson`: 0x766813..0x766835: LookAtObject(+0xBC, 1) == 1 -> counter (+0x58) = 0, SetTopState(0xD0) (guard) 0x766813 tests only +0xBC != 0;
- `src/ECS/Villager/VillagerMourning.cpp`, `FindChildrenAndOrphanThem`: (guard) GetTown 0x751F00 only reads +0x12C: stands for the missing town unlinking
- `src/ECS/Villager/VillagerMourning.h`: `PointTurns`, `MournTurns`, `ReactToDeathPriority`, `PointAtDeadPerson`, `GoTowardsDeadPerson`, `LookAtDeadPerson`, `MournDeadPerson`, `FindChildrenAndOrphanThem`, `MakeChildOrphaned`.
- `src/ECS/Villager/VillagerSoul.cpp`: `SourceClipIsDead1`, `SoulClip`, `SoulAlpha`, `SoulExpired`, `Create`, `Update`.
- `src/ECS/Villager/VillagerSoul.h`: `SoulClip`, `SoulAlpha`, `SoulExpired`.
- `src/ECS/Villager/VillagerInteract.cpp`: `CheckInteractWithField`, `CheckInteractWithFishFarm`, `GetPlayerOf`.
- `src/ECS/Villager/VillagerInteract.h`, `CheckInteractWithField`: both none counts as the same) and SetFarmerGotoField(field, 1) 0x759C40 == 1 -> 1 (with a player MakeCreatureEmpathiseWithPlayerTownDesire(0, 0.5, Pos) 0x7575F6:);
- `src/ECS/Villager/VillagerInteract.h`, `CheckInteractWithFishFarm`: 174 CHECK_INTERACT_WITH_FISH_FARM: Villager::CheckInteractWithFishFarm 0x757610: the same for the fish farm +0x118 (FishFarm::GetPlayer 0x52C850 = its town's) and VillagerBecomesFisherman 0x75B560 == 1 -> 1 (TownDesire 0, 0.5:);

### Test hooks

- `OPENBLACK_VILLAGER_TRACE` / `OPENBLACK_ANIMAL_TRACE` print each landing's pose (above).

## Pending

- **The landing's return to the previous state**: `Villager::TestSpecial` and who sets +0xB4 & 0x400; LANDED and the
  return are set straight rather than through `SetTopState` (queued fix).
- **Picked up or thrown**:
  - the StorePreviousState before SetTopState(24) in the pick-up (0x5ECCB5) and before SetTopState(10) in the start
    of a flight from the hand or, from the world, a villager not IN_HAND (0x5EFDAB, 0x5EFE2B): openblack's pick-up
    (`InterfaceSetInMagicHand`) and `VillagerStartPhysics` store none yet; the knocked and body-less starts do
    (queued fix);
  - IN_HAND's and FLYING's own entry and FLYING's exit (EnterInHand 0x76AFE0, ExitInFlying 0x76ACB0) are taken as 1;
    IN_HAND's exit is ported. No ported exit refuses FLYING; if an exit ported later refuses it, the flight from the
    world must start no physics at all, as the original's `Living::InitialisePhysics` does when SetTopState(10) is
    not 1 (0x5EFE42). openblack builds the body before `VillagerStartPhysics` runs (queued fix).
- **The world path's axes** 🔍 inferred: the physics is to build the bodies from AngleY(ObjectYAngle) (on
  `OriginalRows`); settle with the landing trace above.
- **The disciples' landing**: LANDED's disciple branch and the CHECK_INTERACT_WITH_* states 171-175 wait for the
  disciples; 172 ABODE is not ported.
- **The creature's empathy** on a landing (ANGER 0.5 / COMPASSION 0.1) and in 173 / 174 / 172: no creature yet.
- **A predator's pounce on a corpse**: whether DEAD's exit refuses SetTopState(17) in the game (to check in a run).
- **Dead's vt +0xC4 call** (not ported, a port row): it clears the 3D object's flag 0x04000000 (fn_007F99F0, set at
  creation 0x74FD47). Still open: what reads the bit; no `test` / `and` with 0x04000000 alone was found in the exe
  ([Villagers: states](villager-states.md#pending) takes it as the shadow flag).
