# Villagers: speed and scripts

## Summary

A villager's walking speed is set each time it changes state: the state's speed group, scaled by its age, sex,
hunger, tiredness, load and the town's needs, and by a map scale that grows with the town's belief in its player. A
wounded villager or one whose town is in an emergency gets its own speeds. Scripts can override the speed, face a
villager at a point, play a clip, put villagers into flocks and towns (the script containers), make a thing walk a
camera track, and release it again.

openblack reproduces the speed formula, the script properties, the flocks and containers, the walk along a camera track
and the release from a script; what is left is in Pending. This page owns "Walking a camera track"; the track file's
format is in [Land and script files](../formats/land-and-script-files.md).

## The original

### `WALK_PATH` (177) of a MobileObject

`GScript::WalkPath` 0x6FBB50 pops (object, forward, track, from, to). If the object is Living it goes to 0x5EE100
(below); if it is a MobileObject (`__RTDynamicCast`) to `fn_006076C0(track, from, to, forward)`; otherwise, "Thing is
invalid for move path". `fn_006076C0`: adds the object once to the list g_game+0x205CD4 (= `GlobalGameLists` +0x130)
and gives it a new `DataPath` (0x30) at +0x64: +0x14 = `ScriptedCamera::Create(track)`, +0x1C = to, +0x20 = forward,
+0x28 = **100** (step), +0x2C = 1, **+0x24 = from × duration** (0x60776B: `fild` duration, × from).

Every turn, in `GGame::ProcessTurn`: `Whale::ProcessAll` 0x54E5C7 (start of turn = Pos), then
`GlobalGameLists::Process` 0x5913ED calls `MobileObject::MoveAlongPath` 0x607790 (vt+0x52C) of each object in the
list, and afterwards `GScript::Process` 0x54E693 (the scripts: the `WALK_PATH` takes effect from the next turn):

1. sample = ftol(+0x24) if forward, ftol(duration − +0x24) otherwise; clamped to 0..duration (0x6077A2 / 0x6077F4).
2. `fn_00844280` on the **position** Running (its point is discarded) and `fn_008439C0` on the **focus** way with the
   segment and t it left: **the object follows the focus curve, with the timing of the position curve**.
3. `fn_00607990`: if +0x24 / duration < to: +0x24 += 100 (capped at the duration) and
   `SetPos(MapCoords(ftol(x·6553.6), ftol(z·6553.6)), relative y 0)` (0x6078A3; back to x · 1/6553.6 in
   `Game3DObject::SetPosition` 0x63B6BD) + `Game3DObject::SetPosition(coords, 0, 0, 0, 1)` (height = GetAltitude +
   0; it also sets rotation 0 and scale 1 on the 3D object, which the shark's `Draw` sets again every frame).
   Otherwise it leaves the list **without moving** that turn (0x607900; the DataPath stays). The `SetPos` is
   vt+0xFC = `GameThingWithPos::SetPos` 0x401940, a plain copy: the object is **not** refiled in the map cells as it
   walks (see [Object lists per cell](../engine/object-size-and-map-cells.md#object-lists-per-cell)).

Units: the sample is the track time in ms and the step of 100 is one 100 ms turn, so the track is traversed in real
time (Track20: 123 turns; Track21: 129). Backwards it goes through the samples from the end; `from` and `to` are
always compared with +0x24 / duration, so `WALK_PATH(o, 0, 21, 0.25, 0.75)` starts at sample 9663 and stops when
+0x24 reaches 9721 (0.75 of the duration). There is no loop: when it finishes, the object stays where it is. A second
`WALK_PATH` on the same object does not add it to the list again and replaces the DataPath (the old one is lost).
`ToBeDeleted` 0x606F4A removes it from the list.

`GET_WALK_PATH_PERCENTAGE` (179, 0x6FBC50): **1.0** for anything that is not Living; for a Living
`Living::GetWalkPathPercentage` 0x5EE520 = +0xAC→+0x24 / duration. No original script calls it.
- `InitStepsXZ`: 0x60BFC3..0x60BFF3: +0x64 = (COS[+0x5C] 0xC31E14 x (+0x5A >> 4)) >> 12, +0x6C the same with SIN 0xC31614, +0x68 = 0 (gutils::StepFromAngle, the inline copy of fn_0074D3A0 / fn_0074D3C0); MapCoords units a turn, kept in metres
- `k_SpeedDivisor`: [0x92B404] = 655 and [0x8AC404] = 0.1: the speed (+0x5A, MapCoords units a turn) in the path's units
- `StartWalkPath`: 0x5EE103..0x5EE114: the old DataPath goes; 0x5EE11E..0x5EE147: a new one
- `StartWalkPath`: 0x5EE184..0x5EE1C2: fild duration; fld focus.length; fild speed; / 655; x 0.1; length / s; duration / that
- `StartWalkPath`: 0x5EE1D3..0x5EE1D7: +0x2C = (float) speed
- `StartWalkPath`: 0x5EE1EC..0x5EE1F5: fild duration; fmul from

#### Living (not ported)

0x5EE100 (misassigned symbol `Animal::DebugText`): deletes the old DataPath at +0xAC, creates one with the track,
+0x18 = track, +0x1C = to, +0x20 = forward, **+0x28 = duration / (focus length (+0x0C) / (speed(+0x5A)/655·0.1))**,
+0x2C = speed, +0x24 = from × duration, and calls vt+0x8FC, Living::SetAnim (not AddFootpath). It is moved by
`Living::MoveAlongPath` 0x5EE230. Used by `FollowUs` (Father, track 22) and `BlindWomanJourney` (track 7). The
villager's walk is ported (below, [Walking a camera track](#walking-a-camera-track)); an animal's and a creature's
are not.
- `StartWalkPath`: 0x5EE1FE..0x5EE212: SetCurrentAndDestinationState(28, final) == 1 -> SetAnim(1)
- `MoveAlongPath`: 0x5EE245..0x5EE30D: the speed changed (fcom +0x2C; test ah 0x40): s = speed / 655 x 0.1, L = s != 0 ? length / s : 0, +0x28 = L > 0.01 (test ah 0x41) ? duration / L : 0, SetAnim(1), +0x2C = speed

### Speed

`Villager::SetStateSpeed` 0x753760 is called unconditionally in both SetTopState; the skips are its own: nothing
changes if the villager is controlled by a script (GameThingWithPos +0x25 & 4, 0x753766) or if it is dancing
(`Living::IsDancing` 0x5ECC10, 0x753772: the DanceGroup of Living +0xD8, whatever the state). Thus the villager that
enters 60 WORSHIPPING_AT_WORSHIP_SITE after FindDanceGroup keeps its speed. Afterwards it takes the speed group entry
of the state it is given (0x75397B..0x7539A1): the virtual `SetStateSpeed` 0x753740 gives it `GetFinalState` (vt
+0xB04), and `MoveAroundFire` 0x75AA67 the PREVIOUS state. Then SetSpeed 0x750ED0 (formulas in
[Walking speed](animation.md#walking-speed)). The age is GetAge (unsigned) and is compared **unsigned** with
grownUpAge (0x750F26, `jae`) and oldAge (0x750F87, `jbe`); the differences are loaded as an unsigned qword,
×0.2×0.1, maximum 0.4. The adult subtracts `GetDesireForFood·0.1` (0x750FDB, POWER 0x75BB60 = `1 − min(food,
1)³`), `life·0.1` and 0.2 if it is a woman.

- The map scale m (0x75379B..0x7538A3), worked out before any branch: the town is GetTown (vt +0x48, 0x751F00,
  Villager +0x12C) and the player GetPlayer (vt +0x1C, 0x7502F0: the town's +0x2C player, null without a town).
  - The power: p = player +0xC8 when above 0.1, else 0.1; 1.0 without a player. GPlayer's TribalPower[9] is at +0x68
    (`Object::GetTribalPower` 0x6399D0) and MaxTribalPower[9] at +0xB8, so +0xC8 is MaxTribalPower[4], the Indian
    tribe's. In vanilla it always equals TribalPower[4]: both start at 1 and GPlayer's turn sets TribalPower[i] =
    MaxTribalPower[i] = max(Max, 1), with no other writer. The neutral player's town gets the term too.
    m = (float)(GLandBalance::Values[4] × p) (0x7537C6..0x7537CE).
  - The belief (0x7537D4..0x753802): only with a town and a player that is not the neutral one
    (g_game->players[g_game +0x205A5B], stride 0xA60 from +0x18). bel = `Town::GetBeliefInPlayer` 0x73BAB0 (the
    GBelief at Town +0x798); d = bel − `Town::GetBeliefInNeutralPlayer` 0x73E4B0, which reads the field Town +0x5D8
    (0.5 from the town's constructor and written by SET_TOWN_BELIEF of the neutral player), not the neutral player's
    belief slot; b = d × 0.1 when d > 0, else the whole of bel (0x75380B..0x753838).
  - The scale k = Values[7] (0x75383A..0x753891). Only when it is exactly 1.0 (an `fcom` equality): in a skirmish
    game (the byte g_game +0x205A0C, set to 1 only after StartPlaygroundGame 0x555990 on the front end's skirmish
    path, 0 in the campaign) or a multiplayer one (IsMultiplayerGame 0x552F80), k = GTownInfo +0x16C
    beliefSpeedScaleMultiPlayer (4.0); else for a land number (g_game +0x205A08, compared unsigned) up to 5, k =
    GTownInfo +0x170 + 4 × land, beliefSpeedScaleStory {1, 1, 1.5, 2, 3, 3}; else 1. m = (k × b + 1) × m
    (0x753895..0x7538A3).
- The four speed branches (0x7538D3..0x753B40): the two wounded ones are (GameFloatRand(0.2) + 0.4) × speed4 and
  (GameFloatRand(0.25) + 0.5) × speedDefault. Emergency: when the villager's town is in a state of emergency
  (`Town::IsInStateOfEmergency` 0x747970), (GameFloatRand(0.5) + 0.75) × speedFleeing (GVillagerInfo +0x108). Then
  the normal branch. Every branch is multiplied by the map scale m.
- The food speed-up (× info +0x39C) applies to the emergency and normal branches only. The two wounded branches jump
  past it (0x7538EB / 0x753934 → 0x753B36).
- Info offsets of the normal branch: BaseForTownNeedsSpeedMod +0x36C; DivisorForTownNeedsSpeedMod +0x370 (2.0 in
  info.dat); SpeedModWhenFullLoadOfWood +0x374; SpeedModWhenFullLoadOfFood +0x378. Without a town the town-needs
  term is 1. Order of the product (0x753AFD..0x753B0D): speed group entry × foodF × woodF × T × m, each rounded to
  float.
- A TRADER disciple (type 9, 0x7539FE) uses MaxTraderFoodCarried / MaxTraderWoodCarried (+0x274 / +0x278) in the
  load factors. Both load factors clamp at 0.75 from below (0x753AA1, 0x753AD1), food too (not +0x378).
- SetSpeed 0x750ED0 is then called with the factor flag 1 (0x753B36..0x753B40).
- the emergency and normal branches go through the food speed-up (0x753976 / 0x753B13 -> 0x753B15); the two wounded ones jump past it (0x7538EB / 0x753934 -> 0x753B36)
- `SetVillagerStateSpeed`: 0x7538D3: GameFloatRand(0.2) + 0.4
- `SetVillagerStateSpeed`: 0x75391C: GameFloatRand(0.25) + 0.5
- `SetVillagerStateSpeed`: 0x753939..0x753972: in a town in a state of emergency (Town::IsInStateOfEmergency 0x747970), (GameFloatRand(0.5) + 0.75) x the fleeing speed (GVillagerInfo +0x108) x m (0x75395A)
- `SetVillagerStateSpeed`: 0x75397F..0x7539F4: T = 1 without a town; with one, base (+0x36C) + clamp(TownNeedsSum fn_00747150 / divisor (+0x370), 0, 0.5) (on the x87 stack: float precision, the x87 control word is 24 bits, 0x7DEE0D)
- `SetVillagerStateSpeed`: 0x7539F8..0x753AF7: the loads of wood and food (+0xF6 / +0xF4), with the trader's capacities for disciple 9
- `SetVillagerStateSpeed`: 0x753AFD..0x753B0D: fild spd; x foodF; x woodF; x T; x m; fstp float (each fmul rounded to float, 0x7DEE0D)
- `SetVillagerStateSpeed`: 0x753B36..0x753B40: Villager::SetSpeed(whole, 1) 0x750ED0, with the factor
- `SetVillagerSpeed`: 0x750ED4: f = 1 (fld [0x8AA390]); a second argument of 0 (0x750EDD..0x750EE3, je 0x751014) keeps it
- Living::GetAge 0x5ECAF0 (vt +0x8D0, an unsigned div), compared unsigned with GLivingInfo +0x138 grownUpAge (0x750F26, jae) and +0x13C oldAge (0x750F87, jbe); the differences are loaded as unsigned qwords (fild, high dword 0: 0x750F47 / 0x750FA6), times 0.2 and 0.1 (0x8AA3AC, 0x8AC404), at most 0.4 (0x8C7A44)
- `SetVillagerSpeed`: 0x750FDB..0x750FEE: GetDesireForFood 0x75BB50 (POWER(food) = 1 - min(food, 1)^3) * 0.1
- `SetVillagerSpeed`: 0x750FF2..0x751008: life (vt +0x11C) * 0.1, and 0.2 more for a woman (GVillagerInfo +0x1F8 == 1, which Villager::sex mirrors)
- `SetVillagerSpeed`: 0x751014..0x75101A: fild whole; fmul f; __ftol
- `SetVillagerStateSpeed`: then __ftol

#### Script properties and facing

- `SET_PROPERTY` Speed (022, 0x70E9F6) → `Villager::SetSpeedInMetres` 0x753110: `whole = ftol(m / 10 × 65536)`
  (0x74DCE0), then `SetSpeed(whole, 0)`: with a second argument of 0 the factor stays 1 (0x750ED4..0x750EE3), so the
  script's speed is the speed (clamped to 0..0xFFFF). It lasts: the state speed skips a villager controlled by a
  script.
- `GET_PROPERTY` Speed (0x70DE4E): in the physics the length of the body's velocity (po +0x104), a dead Living 0,
  else `GetSpeedInMetres` 0x60C070. Scale (8) is the uniform scale (Object +0x50). Age (16): GET `GetAge` as a float,
  SET `Villager::SetAge` 0x7528C0 (the meshes when grownUpAge is crossed, then InitialiseScale and SetScaleForAge
  with their GameFloatRand draws).
- `SET_FOCUS` (034, `GScript::SetFocus` 0x6F90B0) → `Object::SetFocus` 0x6393A0: `SetYAngle(LH3DMath::GetYAngle
  0x841260(Pos, target))` = atan2(dz, dx), + 2π when negative. An instant snap: no focus is kept, the next step or
  LookAtPos overwrites it. A container (a flock or a town) gets it on every member (SetFocusLoopFn 0x6F9090, vt
  +0x510 on each, no script control); a creature is taken under script control first (vt +0x34, then +0x440(1)) and
  turns through its own SetFocus 0x4F6760 (see the creature pages). Anything else that is not an `Object` (the
  `dynamic_cast` at 0x6F9197): "Jonty - Thing must be living to face position!" through `ScriptErrorMessage`,
  nothing done.
- `SET_FOCUS` on any other Object: `Object::SetFocus` 0x6393A0 (all 133 Object classes but Creature) takes the from
  point as the MapCoords x / z × 0.000152587890625 ([0x8AA3A4]) and y = GetAltitude 0x803090 + [+0x1C], then
  `SetYAngle` vt +0x524: MobileWallHug 0x60DAC0 for the livings, `Object::SetYAngle` 0x639260 for the other 93
  classes. That one does nothing when GetYAngle (vt +0x508, +0x4C) equals the new angle (0x63926B); else
  `SetXYZAngles` vt +0x514 (GetXAngle vt +0x504, y, GetZAngle vt +0x50C). Three bodies: `Object::SetXYZAngles`
  0x638D00 (61 classes: abodes, features, fixed, trees, spell seeds and icons, highlights, help spirits, street
  lanterns, totems, GBaseOnly...) ignores x and z (Object's GetX/ZAngle 0x4024F0 / 0x402510 give 0.0): out of the
  map (vt +0x548), +0x4C set (vt +0x520), the 3D object's matrix built upright from y and the scale, back in the
  map; `MobileStatic::SetXYZAngles` 0x608CE0 (Bonfire, DeadTree, FelledTree, Rock, MobileStatic, MagicTeleport, the
  LandscapeVortex classes, Fragment) and `MobileObject::SetXYZAngles` 0x607460 (21 classes: piles, Pot, Ball, PBall,
  Poo, Scaffold, Reward, Whale, OneOffSpellSeed, FieldCrop, Creed, Arrow, the puzzle objects...) store x and z (+0x80
  / +0x84 and +0x5C / +0x60), +0x4C, and build `SetYXZMatrixOnly` 0x7FAC10(y, x, z) through
  `Game3DObject::SetPosition` 0x63B680, out of the map and back.
- OVERRIDE_STATE_ANIMATION 068 (`GScript::OverrideStateAnimation` 0x6F9F50): a clip <= 0 or >= 441 logs "Invalid
  animation forced" (0xC0D69C) and carries on (0x6F9F80..0x6F9F93). The clip is AnimPack[clip], with entry 0
  outside [0, count) (0x6F9FD3..0x6F9FE7). The thing is read at 0x6F9F78; no thing → nothing said (0x6F9F9D); not a
  Living → 0x6F9FC0. When the 3D object's anim (vt +0x184) differs it is set (vt +0x180) with cycle time 0 (vt
  +0x188) (0x6F9FEA..0x6FA00D); DataForScriptRemind (Living +0xB0, 0x6FA013..) gets +0x3C = clip, +0x44 = 0, read
  only by EnterPlayAnim 0x768914's resume. The clip lasts until the next `Living::SetStateAnim` 0x5ECB10.
- GET_PROPERTY Speed (0x70DE4E..0x70DF5F), the other cases: a Living in the physics WITHOUT a PhysicsObject returns
  GetSpeedInMetres with no IsDead test (0x70DEB4); any other thing outside the physics has no speed. The velocity
  length is computed as (z·z + y·y) + x·x, then fsqrt (0x70DEB6..0x70DEE0).
- GET_PROPERTY Age (0x70DFFD): GetAge loaded as an unsigned qword and stored as a float. On a thing that is not a
  Living, the caller logs "Not used on non living objects". SET_PROPERTY age: a Living (the cast at 0x70EB72) →
  `SetAge(ftol(val))` (vt +0x8D4, 0x70EB65): Villager 0x7528C0, Animal 0x4179C0; else "Cannot Set Property".
- GET_PROPERTY health (property 1, jump table 0x70E78C entry 0 = 0x70DB85): `dynamic_cast<Object>`, then the float
  of `GetLife` (vt +0x11C): `Object::GetLife` 0x402600 (+0x48) for every Object (villagers, animals, creatures,
  buildings, rocks, trees…), `PileResource::GetLife` 0x66E3D0 (the amount +0x70 as an unsigned qword, `fidiv` by the
  info's +0x11C) for the piles (potType 1 and 2, the hand's food and wood too; a simple pot, potType 0, is an
  Object). `Spell::GetLife` 0x55CDB0 (+0x38) is not reached through this cast (a Spell is no Object). Not an Object
  (0x70DCEE): "Object not an object" (0xC209DC), then "Cannot Get Property %d" (0xC2094C) and a float 0 (0x70E768):
  the containers (Citadel, Town, Flock, Forest), a dance, a mist, a spell, an influence ring, an arena; a
  CitadelPart, a CitadelHeart and a BigForest are Objects. The pile's info +0x11C is `maxAmountInPot` (GPotInfo
  records 0x144 bytes from `InfoList` 0xD4C660); the `fidiv`'s quotient, stored as a float, is the quotient
  correctly rounded to a float at any FPU precision (rounding first to 53 or 64 bits does not change the float a
  division rounds to). Land 1's nine `SheepBaa` read it every pass and stop at 0 or less.
- GET_PROPERTY scale: 0x70DD4C, `GetScale` (vt +0x120). SET_PROPERTY scale: 0x70EA76, `SetScale` (vt +0x124,
  `Object::SetScale` 0x639200: +0x50 and the matrix when it changes).
- `GScript::SetScriptState` 0x6F82E0 on a villager (0x6F831E..0x6F836F): only when the villager is available and
  IsObjectInMap. StorePreviousState (vt +0x8EC), CallExitStateFunction(s), then CallEntryStateFunction(s), both
  results ignored; SetAnim(1); +0x58 = 0.
- 200 SCRIPT_PLAY_ANIM: with +0x120 == 0 nothing happens; otherwise +0x120 −1, then PlayAnimThenSetState(plays left
  ? 200 : 4 IN_SCRIPT). IsScriptAnimationComplete (0x7689D3..0x7689F3): TOP 23 → 0; TOP 200 → +0x120 == 0; else 1.
  GetTopState is 0x5F27F0.
- EnterInScript (0x5ED7F7..0x5ED989) / EnterPlayAnim (0x768858..0x768957): the same entry → 1. Without a
  DataForScriptRemind (Living +0xB0), or when its +0x44 is not next, also 1. Only with matching remind data do they
  resume the remembered walk or clip and return 0x23.
- ExitInScript 0x5ED9C0 (shared by IN_SCRIPT, MOVE_ALONG_PATH, SCRIPT_WANDER_AROUND_POSITION and, through
  ExitPlayAnim, SCRIPT_PLAY_ANIM): first, whatever follows and whatever it returns, CircleHugInfo::Reset 0x60A9F0 on
  Living +0x70 with the Living as the walker (0x5ED9C4..0x5ED9C8). Reset (the same in the exe as in bw1-decomp):
  SetObjectPtr(NULL) 0x60A770, which takes the walker off the hugged object's walker list (g_CircleHugStateInfo
  +0x10 ObjToMwh), and +0x70 obj = NULL (0x60AA03); TurnsToObj +0x74 = 0xFF (0x60AA17); EntryDistance +0x76 = 0
  (0x60AA1B); the walker off the look-ahead sets g_CircleHugStateInfo +0x30 CompletedLookahead and +0x20
  PendingLookahead (0x60AA21..0x60AA6D) and off the ExtendedEntryDistances map +0x00 (0x60AA72..0x60AAB8).
  Then IsDancing (vt +0x978, 0x5ED9D1) is read and unused; IsScriptState(next) (vt +0x960) → 1. Otherwise it
  creates the DataForScriptRemind if missing, sets its +0x44 = GetFinalState, calls KeepThatInMind 0x5EF1D0, and
  returns ExitNoChangeState(next).
- `SET_FOCUS` pops z, y, x (0x6F90C1..0x6F90EB), then the thing (0x6F910B). A container (0x6F912F): the point at
  g_game +0x250090 +0x40, the loop with SetFocusLoopFn 0x6F9090, no SetControlledByScript. A creature:
  SetControlledByScript(1) (vt +0x440) first (0x6F91A9); a Living at 0x6F91BC.
- `SET_TOWN_DESIRE_BOOST` 0x6FE650 pops the boost (0x6FE660), the desire (0x6FE674), the thing (0x6FE686); the
  re-sort of order 1 is fn_746140 (0x6FE73E). The command itself is on
  [Town desires and distribution](villager-decisions.md#town-desires-and-distribution).
- Living::ExitNoChangeState 0x768780 (vt +0x928): 1 if `next` is script-interruptable (vt +0x964, table file 0x1C, 0xDB9E94), is IN_HAND (vt +0x968) or IsStateExitFunctionSameAs (vt +0x96C); else 0
- The animals' IN_SCRIPT (and only it, in their table) leaves through the same ExitInScript, answered with the
  animals' own state flags: [Animals: the exit functions](animals.md#the-exit-functions).
- else CircleHugInfo::Reset (0x60AB9F), +0x78 = 1 and +0x5E = 0xB STEP_THROUGH (0x60ABA4..0x60ABA8)
- `IsScriptAnimationComplete`: 0x7689E0: 0xC8 SCRIPT_PLAY_ANIM -> +0x120 == 0
- `PlayAnimThenSetState`: 0x5ECADC: CallEntryStateFunction(0x17, state) (vt +0x908): TOP 23 WAIT_FOR_ANIMATION, FINAL state; no clip
- `EnterInScript`: 0x5ED7F7: dynamic_cast<Villager*>: always one here. 0x5ED821: IsStateEntryFunctionSameAs(final, next) -> 1
- `ScriptPlayAnim`: 0x768986..0x768995: PlayAnimThenSetState(times left ? 0xC8 SCRIPT_PLAY_ANIM : 4 IN_SCRIPT, 1)
- `EnterPlayAnim`: 0x768858: dynamic_cast<Villager*>: always one here. 0x768881: IsStateEntryFunctionSameAs(final, next) -> 1
- `InitStepsXZ`: 0x60BFBD: SetTowardsAngle vt +0x868, a villager's MobileWallHug::SetTowardsAngle 0x473E40 = SetGameAngle 0x60DA90 (no turn limit): +0x5C = a, Object::SetYAngle(ConvertGameAngleTo3D(a))
- MobileWallHug::InitStepsXZ 0x60BFA0 (villagers; desvios P1): a = GUtils::GetAngleFromXZ(Pos, GetDestPos) 0x74D240 (the u16 game angle, integer LHArcTan), SetTowardsAngle(a) (vt +0x868 = 0x473E40 -> SetGameAngle 0x60DA90, no turn limit), then the step ((COS / SIN[a] 0xC31E14 / 0xC31614 x (speed +0x5A >> 4)) >> 12, gutils::StepFromAngle) in MapCoords units, kept in metres (WallHug::step). The one copy: SetupMobileMoveToPos and PathfindingSystem's InitializeStepToGoal call it. (approximate, P12) Pos and the goal are metres made MapCoords here
- Villager::IsStateEntryFunctionSameAs 0x7524D0 (a, b): the entry functions (+0x10 of the state rows, 0xD091A8, VillagerStateFunctions.h) of both states are the same (EnterInScript, EnterPlayAnim, EnterBuilding)
- `SetVillagerSpeedInMetres`: 0x75311D: ConvertMetersToWholeDistance (ftol(m / 10 x 65536)); 0x753128: SetSpeed(whole, 0)
- `VillagerSpeedInMetres`: 0x60C072..0x60C077: ConvertWholeDistanceToMeters(+0x5A u16): WallHug::speed holds ToMetres of that u16, the same single rounding of whole x 10 / 65536
- InitialiseScale 0x74FB80, then SetScaleForAge 0x752A90 from it: GameFloatRand (Villager.cpp 0x92F / 0x933), the game's synced draws in the constructor's order (V4;
- MobileWallHug +0x5A, the u16 speed (MapCoords units a turn) the original keeps: WallHug::speed is ToMetres of it, and ConvertMetersToWholeDistance gives it back exactly (whole x 10 / 65536 and its inverse are exact below 2^16). The one copy: InitStepsXZ 0x60BFCB and the walk paths read it
- Villager::SetAge (0x7528C0) scale part: InitialiseScale (0x74FB80) + SetScaleForAge (0x752A90), with the game's synced GameFloatRand. Adults end in (0.95, 1.05]; children take ageToScale[age - 1] plus a random part of the way to ageToScale[age + 1].
- `SetLivingScriptState`: 0x6F82EA: a creature (vt +0x34): fn_0047B140 / fn_004F6E30 / fn_004F6F10 with its +0x128C clip.
- `IsStateEntryFunctionSameAs`: 0x7524D0: the entry functions (+0x10 of the 0x90-byte rows, 0xD091A8) of both states (all four dwords; the first 0 -> the same at once, 0x7524F4)
- `AreWeThereAtDestination`: 0x60AD4B: GetDestPos (vt +0x860), 0x60AD54: AreWeThere(dest, r) (vt +0x85C)
- `SetupMoveToPos`: 0x5F286C: SetupMobileMoveToPos(pos) (the one without a hug state, 0x60AAD0), then 1
- GScript::SetScriptUlong 0x6F8770 on a villager (0x6F884D..0x6F8855): +0x11C = the clip, +0x120 = the times

#### Script flocks and containers

- A Flock is the original's flock class for any Living. Its member list's head is the newest member; the tail is the
  leader (`IAmFlockLeader` 0x5ECF70). AddMember fn_0052FA50 first takes the living out of its old flock (which is
  deleted when emptied), then inserts it before the first node from the head whose Living +0xD4 is >= its own. +0xD4
  is 0 (`Living::SetToZero`) except after AddLeader 0x52FC70 (the tail's + 1, 5 for an empty flock), so a normal
  member becomes the new head and a leader the tail. Living +0xB8 / +0xD4 are the flock and its order.
- A villager leaves its flock when it dies (`Living::Dead` 0x5EC403) and when it is deleted (`Living::ToBeDeleted`
  0x5EC166).
- State 27 MOVE_IN_FLOCK (`Living::MoveInFlock` 0x5ECDB0): inside the domain nothing happens. Outside it, the leader
  goes to a random point of the domain; another member that is farther than flockDistance from the leader goes to a
  random point near the leader, unless that point is outside the domain while the leader is inside. The random point
  is `Living::CalcRandomPos` 0x5ED080 (shared with the animals; a villager's two vt tests are 1).
- Towns, dances and flocks are the script containers (the dance's commands:
  [The scripts' dances](dances.md#the-scripts-dances-script-type-10)). FLOCK_CREATE 036, FLOCK_ATTACH 037 (a flock
  target: merge, living + flock, or a new flock for two livings; a town target: RemoveVillager / AddVillagerToTown,
  pushes the town), FLOCK_DETACH 038 (an animal gets a flock of its own, info +0x10 == 4 = OBJECT_TYPE ANIMAL; a
  villager is only removed; no obj: a random member, not the leader; the member's state is not changed),
  FLOCK_DISBAND 039 (a controlled member goes to IN_SCRIPT), ID_SIZE 040, CALL_IN 055 (a town: its abodes newest
  first, each one's inhabitants, then the homeless; a flock from the head), CHANGE_INNER_OUTER 056, and the container
  loops of SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020 and SET_FOCUS 034.
- FLOCK_MEMBER 041 (`GScript::IsFlockMember` 0x6F00A0) pops the flock, then the thing; `dynamic_cast<Flock>` and
  `dynamic_cast<Living>`, either missing → `ScriptErrorMessage` and false (0x6F0144); else whether the living is on
  the flock's member list (+0x3C, node +8 the living, next at +0), pushed as a bool.
- `Living::GetFlockPos` 0x5ECF60 = the flock's +0x14 (the domain centre); `Flock::GetFlockPos` 0x530570 = the tail's
  (leader's) Pos.
- MOVE_IN_FLOCK details (0x5ECDB0..0x5ECF19): inside the domain (PosWithinDomain(Pos, 1.0)) it returns 1. The
  leader: CalcRandomPos(domain centre, 0, radius), SetupMoveToPos(p, 27), 0x23. Another member: if its distance to
  the leader is at most flockDistance it returns 0; otherwise CalcRandomPos(leader's Pos, 0, flockDistance). If p is
  outside the domain while the leader is inside it returns 0, else it walks there and returns 0x23.
- `Living::CalcRandomPos` 0x5ED080(centre, rMin, rMax): two tries. Each try draws a = GameFloatRand(2π) and r =
  GameFloatRand(rMax − rMin) + rMin (always called), then walks a 25-cell spiral from
  AddDistanceFromAngle(centre, a, r) (0x74D510 inline). The first cell that is in bounds, does not collide with the
  info's collide type (+0x11C) and passes both class tests wins. Failing that, the centre if it passes the turn test
  (vt +0xB3C, 0x5ED1E4..0x5ED218), else the Living's own position (fn_0074D650's step is 0 after `sar 4`). For a
  villager both tests (IsPosValidForTurnAngle 0x473EE0, IsPosValidForMapCellExistance 0x417260) return 1.
- `IsScriptContainer` (vt +0x3F8) is 1 for Town, Dance and Flock: `Town::IsScriptContainer`,
  Dance::IsScriptContainer and `Flock::IsScriptContainer` 0x52F880.
- The loop table 0xC0C73C (row = script type × 24): TownLoop 0x6F7850 (`Town::FindVillager` 0x73E600), DanceLoop
  0x6F78E0, FlockLoop 0x6F78B0 (`Flock::FindLiving` 0x530510). Each member goes through 0x6F7800
  (SetControlledByScript(1) first when the running native is one of the 14 whose table entry has
  field_0xC = 1: SET_SCRIPT_STATE, SET_SCRIPT_STATE_POS, SET_SCRIPT_FLOAT, SET_SCRIPT_ULONG, MOVE_GAME_THING,
  FLOCK_ATTACH, POPULATE_CONTAINER, WALK_PATH, SWAP_CREATURE, ATTACH_TO_GAME, DETACH_FROM_GAME,
  DETACH_UNDEFINED_FROM_GAME, SET_ONLY_FOR_SCRIPTS, START_MATCH_WITH_REFEREE; the flag is GScript (the pointer in the g_game field 0x250090)
  +0x3C, set by the per-call hook sub_6EC750 (0x6EC76C, from `g_scriptFunctionTable[n].field_0xC`, registered by
  fn_006EB1D0 0x6EB1FD) and cleared by `GScript::Reset` 0x6EB2D6; the table is filled by the CRT initialiser 0x6FFFD0); a type without one: "No Loop function for type".
- The same flag reaches every object the native is given: `GetScriptGameThing` 0x70D220 (the slot's thing; none for a
  slot out of range or empty, and none for a thing that is not available, vt +0x2C at 0x70D28B) calls
  `SetControlledByScript(GScript +0x3C)` (vt +0x440, 0x70D2BD..0x70D2D1) on a thing that is not controlled yet
  (+0x25 & 4, 0x70D2B7). A scan of the exe for every read of GScript +0x3C finds only these two readers (0x6F780B,
  0x70D2C9) and the hook's write (0x6EC76C), and the CRT initialiser writes 1 into field_0xC for exactly the 14
  natives above (17..20, 33, 37, 109, 177, 210, 214..218; read from 0x6FFFD0 on). So FLOCK_ATTACH takes the obj and
  then the target under the script's control as it pops them (0x6EF400 / 0x6EF408), and so do SET_SCRIPT_STATE
  (0x6F8396), SET_SCRIPT_ULONG (0x6F87A9), MOVE_GAME_THING (0x6F8EEF), POPULATE_CONTAINER (0x6F1F84, before the type
  is checked), WALK_PATH (0x6FBBB2) and SWAP_CREATURE (0x6F48B6, then 0x6F48D1). The town attach's loop (0x6EF8E5)
  goes through 0x6F7800 too. `SetControlledByScript` is only the bit (0x402240, and the creature's 0x47D2C0 alike), so
  with the flag clear nothing changes. A thing a finder handed over is controlled only through these: its first
  reference controls only a thing the script made or one already controlled (`IncreaseReferenceCount` 0x70D5F0,
  0x70D616..0x70D631).
- `Town::FindVillager` 0x73E600's order: the abodes of +0x754 (next +0x9C, newest first), each one's +0xA0 list
  (next +0xE4), then the homeless +0x768 (next +0xE4).
- `FLOCK_CREATE` (`GScript::CreateFlock` 0x6F21F0): `MapCoords(pos)` (0x6F2243), `Flock::Flock(pos, GFlockInfo
  0xC5E624, a player, 0xABA52)` (0x6F2287..0x6F2293), `AddScriptGameThing(f, 1)` (0x6F22C2).
- `FLOCK_ATTACH` (`GScript::AttachToId` 0x6EF3C0): pops the leader flag, the target, the obj; "Id deleted to attach"
  (0x6EF4B4) / "Id deleted to attach to" (0x6EF4AD); a dance target (0x6EF426): SetControlledByScript(1),
  fn_006F0320; a flock target: SetControlledByScript(target, 1) (0x6EF468) then fn_006EF4E0; a town target:
  fn_006EF8A0 (0x6EF496); else "Thing not added to id" (0x6EF4A6).
- fn_006EF4E0(obj, target, leader): the obj a flock or a Living (0x6EF518..0x6EF56B); the target a flock: the two
  merge (`fn_00530210(target, obj)` 0x6EF601, the target pushed) (0x6EF570..0x6EF5DF); a Living + a flock
  (0x6EF5E3..0x6EF697): AddLeader (0x6EF5F6) or AddMember (0x6EF621, "Living already in Flock"), then
  `SetScriptState(living, 27 MOVE_IN_FLOCK)` either way (0x6EF63A), the flock's id pushed and the living's reference
  incremented.
- fn_006EF4E0, two livings (0x6EF698..): a new flock at the target's Pos (+0x14) (0x6EF700..0x6EF70B),
  `AddScriptGameThing(f, 1)` (0x6EF71D), `IncrementScriptReference(target)` (0x6EF729); the id is pushed before the
  members are added (0x6EF742); AddLeader + Increment(obj), no state 27 (0x6EF752..0x6EF764); the target too, with a
  second reference (0x6EF793..0x6EF7C4); failure "Thing not added to Flock" (0x6EF7E3).
- fn_006EF8A0(obj, target, town) pushes the target (the town's id): a container's members through fn_006EF810
  (0x6EF8C6..0x6EF912); a single villager (0x6EF9A0..0x6EF9BF); a SpellDispenser vt +0x8F0 with the town
  (0x6EF938); exit 0x6EF98F. fn_006EF810: `RemoveVillager` 0x73E210 and `AddVillagerToTown` 0x73A090 (the town at
  g_game +0x250090 +0x50); a non-villager logs "JONTY-Trying to add non villager to town" and pushes a 0 (0x6EF860).
- `FLOCK_DETACH` (`GScript::FlockDetach` 0x6EF9E0): pops the container, then the obj id; a flock → fn_006EFB80
  (0x6EFA2A); a dance → `DetachFromDance` 0x6F0430 (0x6EFA3C); a town → fn_006EFAF0 (0x6EFA66: `RemoveVillager` at
  0x6EFB3C, pushes 0 either way); a Living stands for its flock (0x6EFA8F..0x6EFAA9); a dead container: "From thing
  dead!" and no push at all (0x6EFAD2).
- fn_006EFB80(obj, flock): an animal gets a flock of its own, a villager is only removed (0x6EFBC0..0x6EFC0B),
  Decrement, push; no obj: a random member, not the tail when there are two or more (0x6EFC16..0x6EFC31):
  `FindScriptGameThing`, fn_0070D2F0 (its id, a slot made when it has none), Decrement, push the id
  (0x6EFC56..0x6EFC9B).
- `FLOCK_DISBAND` (`GScript::DisbandId` 0x6EFDC0): from the head, the next one read before each removal (0x6EFE35..0x6EFE90);
  an animal (info +0x10 == 4) with g_game +0x14 & 0x8000 clear (0x6EFE45..0x6EFE5A); DecrementScriptReference
  (0x6EFE73); a controlled one SetScriptState(4) (0x6EFE84); a dance 0x6EFE9B; a town or an abode: nothing (0x6EFF1F
  / 0x6EFF2D); the container is not deleted.
- `ID_SIZE` (`GScript::GetIdSize` 0x6EFF50): a flock's +0x48 (`fild qword` of the dword, 0x6EFF88..0x6EFF99), a
  dance's +0x90, a town's TownStats +0x8 + +0xC (Town +0x618 / +0x61C, 0x6EFFFD..0x6F0017), a football pitch's
  (IsFootball vt +0x464) +0x228 + +0x230, the counts of its HomeTeam and AwayTeam lists (0x6F003F..0x6F0059; they
  fill only during a match, so with football off, as without the add-on, always 0); else
  "Cannot Find Flock/Dance/Town Size" (0x6F0073) and 0.
- <a id="town-and-abode-counts"></a>`ID_ADULT_SIZE` 387, `OBJECT_CAPACITY` 388 and `OBJECT_ADULT_CAPACITY` 389
  (`GScript::GetObjectAdultSize` 0x6FF160, `GetObjectCapacity` 0x6FF270, `GetObjectAdultCapacity` 0x6FF390, one
  shape): POP the id, `GetScriptGameThing`; none → "Object should be alive!" (0xC0DB48) and PUSH 0. Neither IsTown
  (vt +0x1B8) nor IsAbode (vt +0x208) → "Object should be town, or abode" (0xC0DB28), then the same PUSH 0 (the
  jump back to 0x6FF190 / 0x6FF2A0 / 0x6FF3C0). A town: adults = TownStats +0x8 (Town +0x618, `fild qword` of the
  dword: unsigned, 0x6FF1FE); capacity = adult places + child places (+0x644 + +0x650, `fild dword`: signed,
  0x6FF30E); adult capacity = +0x644 (`fild dword`, 0x6FF42E). An abode: adults = (u8) +0xB4 (0x6FF243); capacity
  = its info's (+0x28) MaxVillagersInAbode + MaxChildrenInAbode (+0x174 + +0x178, `fild qword`: unsigned, 0x6FF34F);
  adult capacity = info +0x174 (0x6FF461). The town's places are the running counts of
  [the town's room](villager-life.md#the-towns-room), so a home made functional counts at once. IsAbode is true for
  the Abode class and every class built on it (the fields, the civic buildings, the windmill, the football); the
  citadel is not one. The only shipped caller is Land 2's Workshop lesson (`TheWorkshopComplete`, theworkshop.txt):
  on town 0 it notes OBJECT_ADULT_CAPACITY, then every 4 seconds reads ID_ADULT_SIZE (capped at 40) and
  OBJECT_ADULT_CAPACITY, and its exit test is "homeless share < 40 % or no adults or 10 or more new adult places".
  The test first runs on the turn after the first pass (handlers run before the task's own code, and the pass ends
  on its backward jump), so it reads that pass's counts. OBJECT_CAPACITY has no shipped caller.
- `CALL_IN` 055 (`GScript::CallIn` 0x6F22F0): pops excluding, the container, the sub-type, the type; table
  0xC0C730: FindInTown 0x6F7410, FindInDance 0x6F7520, FindInFlock 0x6F7500; the filter 0x6F79F0 (!IsInScript &&
  0x6F6FA0) when excluding, else 0x6F6FA0 (0x6F2364..0x6F2376). Found → `AddScriptGameThing(found, 0)` (0x6F2383);
  none → warning, push 0 (0x6F23AE); not a container → "Cannot look in object" (0x6F23A1).
- CALL_IN in a town: villagers (types 4 / 5, `Town::FindVillager`), animals (6, `Town::FindAnimal` 0x73E6E0: the
  town's +0x984), its storage pit (16, `Town::GetStoragePit` 0x73B5B0, no filter, 0x6F7442); another type logs
  "Looking for strange type in Town".
- `CHANGE_INNER_OUTER_PROPERTIES` 056 (`GScript::ChangeObjectProperties` 0x70F480): pops calm, outer, inner, the obj;
  on a flock outer ≠ 0 → +0x50 = ftol(outer), inner ≠ 0 → +0x52 = ftol(inner) (0 keeps the field: `fcomp, test ah
  0x40`, 0x70F4FF..0x70F537), +0x5C = ftol(calm) always (0x70F49F / 0x70F53C); a WeatherThing → fn_00774340
  (0x70F546).
- `MoveInFlock`: 0x5ECDD7
- `MoveInFlock`: 0x5ECDDD..0x5ECDED: PosWithinDomain(Pos, 1.0) -> 1
- `MoveInFlock`: 0x5ECDFE..0x5ECE27: CalcRandomPos(Living::GetFlockPos 0x5ECF60 = the flock's +0x14, 0, (float) radius)
- `MoveInFlock`: 0x5ECE49
- `MoveInFlock`: 0x5ECE59..0x5ECE8F: fn_0074CD50(Flock::GetFlockPos 0x530570 = the tail's Pos, Pos); (float) flockDistance >= it -> 0
- `MoveInFlock`: 0x5ECE95..0x5ECEC4: CalcRandomPos(the leader's Pos, 0, flockDistance)
- `MoveInFlock`: 0x5ECEDF..0x5ECF0E: the leader's Pos within the domain, then p; p outside while the leader is inside -> 0
- `MoveInFlock`: 0x5ECF19
- State 27 MOVE_IN_FLOCK, row {0x5B0190 -> vt +0x8B4 = Living::MoveInFlock 0x5ECDB0, no entry, exit or validate}: no flock or within the domain (PosWithinDomain(Pos, 1)) -> 1. The leader (the tail) -> CalcRandomPos(the domain centre, 0, domainRadius), SetupMoveToPos(p, 27), 0x23. Another member: flockDistance >= its distance to the leader -> 0; else p = CalcRandomPos(the leader's Pos, 0, flockDistance), and unless p is outside the domain while the leader is inside (-> 0), SetupMoveToPos(p, 27), 0x23
- GScript::ReleaseContainerContents 0x6EFCD0: a flock's members from the head (the next read first), each controlled one (+0x25 & 4) with a slot (FindScriptGameThing 0x70CF50) ReleaseScriptThingIntoTheGame(member, id, 1); a controlled member without a slot ends the whole loop (0x6EFD2E..0x6EFD30: je 0x6EFD54); another container: "CANNOT RELEASE THIS! USE DISPAND!"
- a thing in a script already has its slot (FindScriptGameThing); else a free slot: fn_0070D870 sets +0xC and clears the count.
- GScript::AddScriptGameThing (0x70D0F0): a command hands a thing to the script. `createdByScript` is the slot's +0xC (1 for the CREATE family, 0 for the finders); a thing already in a script keeps its slot.
- GScript::IncrementScriptReference (0x70CF90) -> ScriptManage::IncreaseReferenceCount (0x70D5F0): a script variable takes it (LHVM's reference callback, ADD_REFERENCE). The first reference sets IsInScript and, for a thing the script created (or one already controlled), ControlledByScript.
- GScript::DecrementScriptReference (0x70CFD0) -> fn_0070D670: one reference less (the release waits for Process)
- `ReleaseControlFromScript`: 0x6EFDAC
- The hunters' test (Animal::HuntingMoveToPos 0x418DD7, fn_00419340 0x419376, fn_004196D0 0x41973B): a target controlled by a script is only for a hunter that is in a script itself

#### Walking a camera track

- WALK_PATH 177 on a villager → Living 0x5EE100: its own DataPath (Living +0xAC), step = duration / (focus.length /
  (speed / 655 x 0.1)), then state 28 MOVE_ALONG_PATH (final 4 IN_SCRIPT) and SetAnim(1). Each turn
  `Living::MoveAlongPath` 0x5EE230: a speed change retimes the step (speed 0 gives step 0: the walker stops in state
  28), the point is the MobileObject path's, below `to` it advances one step, turns towards the point from its Pos at
  the turn's start and moves there (relative y 0); at `to`, SetTopStateToFinal. GET_WALK_PATH_PERCENTAGE 179 is
  current / duration.
- `WALK_PATH`: IsLiving at 0x6FBBD9 → 0x5EE100(track, 4 IN_SCRIPT, from, to, forward); else "Thing is invalid for
  move path" (0x6FBC2E).
- The walk turns towards the point only when it is more than sqrt(0.001) from the turn-start position
  (0x5EE423..0x5EE489). The retimed step is 0 when the length per turn is not above 0.01 (0x5EE245..0x5EE30D).
- 0x5EE4F0(v), the "walk path reached" test: 1 without a DataPath (+0xAC) or when GetWalkPathPercentage >= v.
- The MobileObject path: the start sets current = duration × from (0x60776B, `fild` of the duration); the sample is
  ftol of current (forward) or of duration − current, clamped to 0..duration (0x6077A2 / 0x6077F4); the position
  goes through MapCoords, fixed point ftol(x × 6553.6) (0x6078A3), read back as x / 6553.6 by
  `Game3DObject::SetPosition` 0x63B6BD; a finished walk leaves the list without moving that turn (0x607900).
- `WaitForAnimation`: 0x5EC995: IsReadyForNewAnimation(1) 0x5EC960 -> SetTopStateToFinal 0x5ECA80 and 0; else 1
- `MoveAlongPath`: 0x5EE3D8..0x5EE3EE: GetWalkPathPercentage < to, else SetTopStateToFinal 0x5ECA80
- `WalkPathReached`: 0x5EE4F0: no +0xAC -> 1; GetWalkPathPercentage >= v (fcomp; test ah 1) -> 1
- State 28 MOVE_ALONG_PATH: Living::StateAlongPath 0x5B0040 -> vt +0x52C Living::MoveAlongPath 0x5EE230. A speed change retimes the step (0 stops it); the sample and the point are the MobileObject path's; below `to` the walker advances one step, turns towards the point (SetYAngle(GetYAngle(Pos at the turn's start, point)) when it is more than sqrt(0.001) away) and moves there (MoveMapObject, relative y 0); at `to`, SetTopStateToFinal. 1
- `WalkPathReached`: 0x5EE4F0(v): 1 without a DataPath or when the percentage is >= v

#### Released from a script

- RELEASE_FROM_SCRIPT 159 and the task's end (GScript 0x70D540 / 0x70F600): a villager gets DECIDE_WHAT_TO_DO (in
  the physics as its previous state), then `Villager::ReleaseFromScript` 0x7531D0: out of its flock; in the physics
  or the hand only a villager without a town joins the vagrants; a dead one VillagerDead(0, .., 0, 1) and DEAD
  unless already in a death state (13..17); with a town DecideWhatToDo; else the vagrants, VAGRANT_START and
  VagrantStart. An animal gets WANDER and fn_0041AA00. A controlled container: at the task's end DisbandId and, when
  the script created it, its deletion; with the command its controlled members are released first. A container not
  controlled gives back its members' references.
- 0x70F600 removes the thing's music (`GAudio::RemoveThingMusic` 0x429340, at 0x70F67D) on every path but the
  task-end delete, before the switch on the script type, for every type. In `Villager::ReleaseFromScript`, g_game
  +0x14 & 0x8000 (0x7531D9) is GAME_FLAG_CLEARING_MAP, set only inside `GGame::ClearMap`, so nothing happens while
  the map is cleared; the flock's Shepherd (+0x30) is cleared when it is this villager (0x7531EF); the dead
  villager's player is the one who last dropped it, vt +0x6C, which for a Villager is 0x4018B0 (`xor eax, eax`):
  always none.

### Sources

- bw1-decomp: `src/Black/Villager.cpp`, `Living.cpp`, `GScript*.cpp`; the rest read in the exe with the xref index.

## openblack

**Done** for the villager; the animals' and the creature's walk along a track are not ported.
The natives that take control (the 14 above) take it of the things they are given and of the members their container
loops visit: `script_held::TakesControl` / `TakeControl` (`src/ECS/ScriptHeld.h`), `ScriptObjectsSystem::EnterNative`
/ `Fetch`, told which native runs by the VM's per-native hook in `Game.cpp`, and `FetchForNative` in the natives
(`src/CHLApi.cpp`). A thing given straight to such a native, which no variable keeps, takes a slot first, so that the
end of the turn gives it back to the game. The testbed's own calls of a native go round the hook, so they take no
control. Tests: `FlocksTest.TheNativesThatTakeControlAreTheGamesFourteen`, `ANativeThatTakesControlTakesItOfWhatItIsGiven`,
`ADancerTheAttachTookControlOfIsTheScriptsToGiveBackAtTheDisband` (`test/test_script_flocks.cpp`).

- **Speed**: `ECS/VillagerSpeed.{h,cpp}`. `SetVillagerStateSpeed(e)` and `SetVillagerStateSpeed(e, state)` stand for
  the virtual `SetStateSpeed` and `MoveAroundFire`'s call with the previous state. The map scale is
  `villager_speed::Scale`, fed by `SetStateSpeedOf` through `villager_speed::PlayerTermsOf` with the town owner's
  `PlayerMagic::tribalPower[4]` (openblack keeps no separate most-ever array; the two are equal in vanilla), the
  town's belief in its owner and `TownBelief::beliefInNeutralPlayer`. openblack has neither a skirmish game nor
  multiplayer, so the multiplayer scale is never taken. Dancing is being in a dance's group, whatever the state, as
  in the original; the worship sites' dancers, which are in no dance yet, use `WorshipVillager::dancing`
  **⚠️ approximate**, a queued fix.
- **Script properties**: `ecs::SetVillagerSpeed(e, whole, applyFactor)` / `SetVillagerSpeedInMetres`
  (`ECS/VillagerSpeed.h`); Age SET is `villager::SetAgeAndScale`; health is
  `script::property_rules::HealthProperty` with `ecs::life::LifeOf`, and `PileLife` for a pile; Scale is the
  Transform's uniform scale. GET Speed and Age are `SpeedProperty` and `AgeProperty` (`ECS/LivingAngle.h`): nullopt
  when no speed or age is known (a thing that is not a Living, out of the physics or without a PhysicsObject).
  `LifeOf` (`ECS/Life.h`) is `Object::GetLife` (vt +0x11C, 0x402600). OVERRIDE_STATE_ANIMATION's clip checks are
  `IsInvalidForcedClip` (logged, it goes on) and `ForcedClipIndex` (entry 0 outside the pack). Tests in
  `test/test_script_living_props.cpp` pin the bits of `affine::GetYAngleBetween` (towards −z: 4.712389, `0x4096CBE4`)
  and the speed cases (a dead Living 0; in the physics without a PhysicsObject GetSpeedInMetres even when dead).
- **Facing**: `living::SetFocus` (`ECS/LivingAngle.h`), with `affine::GetYAngleBetween` (formerly `lh_matrix`). The
  three bodies are `script_set_position::FocusYAngle` and the kept angles
  ([The angles of a thing](../world/objects.md#angles-of-a-thing)): nothing when the angle about y the thing keeps
  is the new one (`placed_turn::AnglesAfterScriptSet`), else `placed_turn::SetAngles` with its leans kept (an upright
  `affine::AngleY` for the first kind, `affine::RotationYXZ(y, x, z)` for the other two), then
  `map_cells::OnAnglesOrScaleChanged`. Before, openblack compared the turn instead of the angle and could not turn a
  leaning mobile static or mobile object.
- **Flocks**: `ECS/Flocks.h` is the original's Flock class for any Living. `components::Flock::members` is the
  original's member list **reversed**: front is the tail, the leader; back is the head. Living +0xB8 / +0xD4 are
  `Villager::flock` / `flockOrder` (and `Animal::flock` / `flockOrder`). MOVE_IN_FLOCK is
  `ECS/Villager/VillagerFlock.h`; the random point is `ECS/LivingPos.h`, shared with the animals, where `PosTest` is a
  Living's per-class test of a candidate point (a villager's two are 1, an animal's are its own). FLOCK_MEMBER takes a
  `components::Flock` and a villager, animal or creature, then `flocks::MembersFromHead`.
- **A football pitch's size** ✅: `script_containers::Size` answers 0 for a pitch (`abodes::TypeOf` FootballPitch),
  without the original's error. openblack runs no matches (football is off), so the teams are always empty; when
  matches are ported this must read their two team lists. Test `FlocksTest.AFootballPitchsSizeIsItsTeamsPlayers`.
- **Town and abode counts** ✅: `script_containers::AdultSize`, `Capacity` and `AdultCapacity` (ID_ADULT_SIZE,
  OBJECT_CAPACITY, OBJECT_ADULT_CAPACITY; [the original](#town-and-abode-counts)): a town's `stats.adults` (the count
  ID_SIZE reads) and the places of `town_stats::RoomOf` (the room as it stands now), an abode's `adultCount` and its
  info's room (`abode_villagers::MaxVillagers` / `MaxChildren`); test
  `FlocksTest.AdultSizeAndCapacityCountATownsOrAnAbodesAdultsAndPlaces` (test/test_script_flocks.cpp).
- **Containers**: `ECS/ScriptContainers.{h,cpp}`. `IsContainer` is a flock, a town or a dance (`dances::IsDance`;
  FLOCK_ATTACH with a dance target makes each member join it, `JoinDance`; a dance's own member list, which the game
  never fills, is walked as empty);
  `ForEachMember` is the type's loop function (a flock from its head, a town through `WalkTownVillagers` in
  `Town::FindVillager`'s order; false for a type without one); `CreateFlock` is FLOCK_CREATE (⚠️ approximate: y is
  kept as the script gave it, x and z are quantised); `AttachToFlock` is fn_006EF4E0 (its living + flock part is
  `JoinFlock`), `AttachToTown` fn_006EF8A0 with `MoveToTown` as its loop callback fn_006EF810 (openblack does not
  push the 0 the original's callback pushes for a non-villager at 0x6EF860), `DetachFromFlock` fn_006EFB80,
  `DetachFromTown` fn_006EFAF0, `Disband` FLOCK_DISBAND and `ChangeInnerOuter` CHANGE_INNER_OUTER_PROPERTIES on a
  flock (the WeatherThing's case is pending).
- **Walking a track**: `ECS/LivingWalkPath.h` (`components::LivingWalkPath` is the Living's DataPath, the point is
  `SampleWalkPath`) for the villagers; the MobileObject's walk is `WalkPathSystem` (`ECS/Systems/Implementations/WalkPathSystem.cpp`), whose
  component dies with the entity (the original's `ToBeDeleted` removes it from the list).
- **Release**: `script_held::ReleaseFromScript` / `Process` (`ECS/ScriptHeld.{h,cpp}`).
- Porting decisions (his names over ours): [Port decisions: beings](../project/port-decisions-beings.md).

### Notes on the code

The code that reproduces the facts above, by file, with what the port adds or leaves out.

- `src/ECS/Villager/VillagerFlock.cpp`: `MoveInFlock`, `VillagerStates::MoveInFlock`.
- `src/ECS/Villager/VillagerFlock.h`: `MoveInFlock`.
- `src/ECS/Villager/VillagerScript.cpp`: `ExitNoChangeState`, `IsStateEntryFunctionSameAs`, `AreWeThereAtDestination`, `SetupMoveToPos`, `IsScriptAnimationComplete`, `PlayAnimThenSetState`, `EnterInScript`, `ScriptPlayAnim`, `EnterPlayAnim`, `WaitForAnimation`, `InitStepsXZ`, `SetupMobileMoveToPos`, `VillagerStates::ScriptPlayAnim`, `VillagerStates::WaitForAnimation`, `VillagerStates::InScr`, `WholeSpeed`.
- `src/ECS/Villager/VillagerScript.cpp`, `SetupMobileMoveToPos`: MobileWallHug::SetupMobileMoveToPos 0x60AAD0 (pos): +0x80 = pos (0x60AADC..0x60AAED), InitStepsXZ (0x60AAF2), off the circle-hug lists (fn_00611AC0 / fn_00611610 / fn_00612BB0 / fn_00610590, +0x76 = 0: openblack's WallHug keeps none), then AreWeThere(0) (0x60AB87) -> +0x5E = 1 ARRIVED (0x60AB91);
- `src/ECS/Systems/Implementations/VillagerScript.cpp`, `ExitInScript`: the circle-hug reset is the removal of
  `WallHugObjectReference`, as at MOVE_TO_POS's exit and a reaction's: the component holds the hugged circle (obj),
  `stepsAway` (TurnsToObj) and `entryDistance` (EntryDistance, wide enough to hold the extended entry distances
  too), so all three go with it. The walker lists have no counterpart: openblack finds a circle's huggers by their
  component, and the look-ahead sets are not ported (the look ahead itself is not). `WallHug::needsLookahead` is
  this turn's hand-over flag, not a set membership, and is left alone as the original leaves its own. Tested by
  `VillagerCoreTest.ScriptExitInScriptLetsGoOfTheCircle` (`test/test_villager_core.cpp`).
- `src/ECS/Villager/VillagerScript.cpp`, `ExitInScript`: 0x5ED9FB..0x5EDA33: DataForScriptRemind::Create if none, +0x44 = GetFinalState, KeepThatInMind 0x5EF1D0: not ported 0x5EDA3D: ExitNoChangeState(next) (vt +0x928)
- `src/ECS/Villager/VillagerScript.cpp`, `ScriptPlayAnim`: 0x76899A..0x7689A9: DataForScriptRemind::Create if none (not ported)
- `src/ECS/Villager/VillagerScript.h`: `InitStepsXZ`, `IsStateEntryFunctionSameAs`, `SetScriptAnimation`.
- `src/ECS/VillagerSpeed.cpp`: `SetVillagerStateSpeed`, `SetVillagerSpeed`, `SetVillagerSpeedInMetres`, `VillagerSpeedInMetres`.
- `src/ECS/VillagerSpeed.cpp`, `SetVillagerStateSpeed`: 0x753B15..0x753B38: IsFoodSpeedUp (vt +0x87C, 0x55C980: +0xF0 != 0) -> speed x info +0x39C (foodPowerupIncrease, 🔍 inferred the field at that offset of openblack's GVillagerInfo), kept on the x87 stack (float precision, 0x7DEE0D);
- `src/ECS/VillagerSpeed.cpp`, `VillagerScaleForAge`: before, openblack's own generator)
- `src/ECS/VillagerSpeed.h`: `WholeSpeed`, `VillagerScaleForAge`.
- `src/ECS/LivingWalkPath.cpp`: `k_SpeedDivisor`, `StartWalkPath`, `MoveAlongPath`, `WalkPathReached`, `WholeSpeed`, `VillagerStates::MoveAlongPath`.
- `src/ECS/LivingWalkPath.h`: `MoveAlongPath`, `WalkPathReached`.
- `src/ECS/ScriptHeld.cpp`: `ReleaseContainerContents`, `ReleaseControlFromScript`, `SetLivingScriptState`, `DecrementReference`.
- `src/ECS/ScriptHeld.cpp`, `AddScriptThing`: Not ported: with createdByScript the Object's mesh (+0x40) vt+0x98(0) and +0xA |= 0x40.
- `src/ECS/ScriptHeld.h`: `AddScriptThing`, `IncrementReference`, `DecrementReference`, `MayTarget`.

### Test hooks

None of its own; the scripts' commands are run through the scripted scenarios
([Scenarios](../project/scenarios.md)).

## Pending

- **GET_PROPERTY / SET_PROPERTY Speed, Scale and Age on a thing that is not a villager or an animal**: not ported
  (pending); the original's cases are above. Still to read for GET Speed: the other classes' vt +0x130 and the
  weather's fn_007745A0.
- **An animal's or a creature's state 28** (walking a camera track): not ported; the Living's DataPath above.
- **The worship sites' dancers' speed**: approximate (`WorshipVillager::dancing`), a queued fix.
- **Released from a script**: the flock's shepherd link (+0x30 of the flock), which waits for the shepherds.
