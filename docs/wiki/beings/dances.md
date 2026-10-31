# Dances

## Summary

A dance is a group of dancers that move together to a dance file's key frames: the worship dance at the temple's
worship sites, the creature's dances, the meetings and the dances round buildings, people and objects. A dance is made
by a script (DANCE_CREATE) or by the game, holds its dancers in groups that take shapes (circles, spirals, a heart,
shapes read from files), turns its clock every turn, and lets dancers join (FLOCK_ATTACH) and leave (FLOCK_DETACH,
FLOCK_DISBAND). A villager in a script's dance walks to its place and plays the dance's clip.

openblack reads the dance files, makes the scripts' dances with their groups, shapes, moves and clips, and runs them
in the turn; the worship sites do not use the dance yet, and the dance camera and lights wait for the camera code.

## The original

### The dance table and the files

- `GDanceInfo` rows (info.dat 0x72168, stride 0xA0; in memory 0xCC4B80, rows of 0xB0): `startsAutomatically` (+0x98
  in memory), `impressiveness` (+0xAC). Rows: 0 Worship (NewWorship2), 1 Creature (CreatureDanceWithLightsAndCamera), 2
  Meeting (ChildrenDancing), 3 Housewife (Test), 4 SmallBuilding, 5 LargeBuilding, 6 DanceAroundPerson, 7
  DancePrayInwards, 8 DanceAroundTownCentre, 9 CreatureTellingStory, 10 CreaturePlayingWithVillagers, 11
  CreatureDanceWithVillagersWatching, 12 CreatureImpressingVillagers, 13 CreatureDanceAmorous, 14
  NewDanceAroundPerson, 15..18 DanceAroundObjectDiameter{Less7, Less10, Less20, Greater20}, 19..24 CitadelDance1..6 =
  NewWorship2, NewWorship1, NewWorship, NewWorship2, NewWorship1, NewWorship.
- The names are `scripts\\Dance\\<name>` (two backslashes in the data), with no extension. **Row 7's file is not
  shipped** (only `DancePrayingInwards.dat` is). The table names 20 files of their own; 9 more in `Scripts\Dance` are
  in other formats or unused (`Worship3` is version 7). The file format is
  [The .dnc format](../formats/land-and-script-files.md#the-dnc-format).
- The table holds 25 rows in .bss. What follows it: row 25 reads duration 0xFFFFFFFF and totalNumBeats bits
  0x4BF0BF10, the rest zero (static initialisers 0x50CC10, 0x50CBEC ..); rows 26 and 27 read zeros (two sprintf
  buffers no code reaches); from row 28 on it reads runtime state (the DanceLight list, bitmap pointers, heap
  pointers), unknowable. The game's own scripts pass only rows 14 and 6.
- The scripts that make a dance: the intro's welcome dance (row 14, 30 turns, around the crowd's first villager) and
  the Pied Piper's round (row 6, 30 turns, around the piper). Both rows start on their own (`startsAutomatically` 1)
  and their duration column is 0 (the script gives the time).
- The "Untitled" names of version 0 files and the global version 0xD06AF8 are read only by a saved game's loading.

### Making a dance

- `Dance::Dance` 0x50B6A0 → `Initialise` 0x50B7A0: State = WAITING (1); its groups deleted and one made
  (0x50B833..0x50B870, Index 0); `SetWorshipSpeed(0.25)` (0x50B893); first in `GameLists.dances`; Duration;
  `LoadDance(file)`; FollowTarget. For every type but Worship (0) and the citadel dances (19..24):
  `GFootpath::GenerateRoutesBetween` from the town's storage pit (the town given, else `GetNearestTown(FLT_MAX)`
  0x50B939; 0x50B944, 0x50B94F). `LoadDance` refuses nothing: the key frames are ordered by time on reading, a short
  file keeps what it read, bytes after the end are left; a dance whose file is missing is made, with one group and no
  key frames. Its defaults follow the constructor: loop length 5, round length (Denominator) 100.
- The footpath (0x50B90C..0x50B94F): the type is tested as `type != 0 && (type <= 0x12 || type > 0x18)` (the compare
  at 0x50B91E), so every row lays one but the worship dance (0) and the citadel dances (19..24): rows 1..18 of the
  table, and a row past it too (rows 25.. are read from what follows the table, above), which no shipped script asks
  for. The town is
  `Initialise`'s last argument: the script's CreateDance passes none (0x6F021B), a town artefact's dance passes the
  artefact's town (+0x18, `TownArtifact::SetVillagerActivity` 0x4264AB). With none, `MapCoords::GetNearestTown` 0x6020E0
  of the dance's own +0x14 with `DanceTownMaxDistance` [0x8D2858] = 0x7F7FFFFF (FLT_MAX, no writer; its only reader is
  0x50B930); no town found: nothing. A town given is used as it is, with no fall-back. Then `Town::GetStoragePit`
  0x73B5B0; none: nothing; else `GenerateRoutesBetween(pit, this)` (the pit first). The dance's end of it is
  `Dance::GetArrivePos` 0x50CB40: +0x14 with x + 0x8000 (`add ecx, 0x8000`, wrapping), z and the altitude kept; the
  dance's `AddFootpath` makes its link (+0xE8) on first use.
- `GroupBehaviour` constructor 0x596070: Speed = LastSpeed = 1 (+0x9C, +0xA0), editorValue 30 (+0x44), Denominator
  100 (+0x98), Length 5 (+0x74), numGroups 0 (+0x40), the proportion counter 0 (+0x94).
- `DanceGroup::Initialise` 0x50CF30: hidden, not a path, circle, Size 20 (+0x50), angle 0, out of its parent's
  children.
- `DanceGroup::Process` 0x50CFF0 ends (0x50D103..0x50D18D) by writing the group's Centre (+0x88), LastCentre (+0x94)
  and Speed (+0x80, the integer distance between the two centres); `Initialise` sets Speed to
  `GVillagerInfo::InfoList[0].WalkingSpeed` and clears HasLastCentre (+0x84). Only Process itself and the group's Save
  0x50D640 / Load 0x50D9C0 read them. The first-dancer test there calls `RemoveFromDance` on a living with no group,
  which returns at once (0x5EF93B).
- `Dance::SetWorshipSpeed` 0x50C340: Speed = (float)ftol(speed × 10) × 0.1f × 4; while DANCING a new Speed resets the
  clock to 0. `Dance::GetAngle` 0x50C310: a worship site target → `GetAngleFromCitadel` 0x77BD60, else 0.
- Scripts: `GScript::CreateDance` 0x6F0170 (DANCE_CREATE 054). Worship sites: `GetDancePos` 0x77CD90 (special point 8),
  `Infos[19 + slot]`, intensity through fn_0077B8D0.

### The dance's turn

`Dance::ProcessDances` 0x50BB60 is step 4 of `GGame::ProcessTurn` (0x54E646, after the players), the list from its
head (newest first). `Dance::Process` 0x50BB90: the target gone → the dance goes (its dancers to DECIDE_WHAT_TO_DO);
FollowTarget; WAITING, `startsAutomatically` and `HasDanceProperlyStarted` 0x50C290 → `StartDance`, else Duration over
→ WAITING; DANCING with dancers: the dance camera count, then `GroupBehaviour::Process` 0x5964A0: the first key frame
due by the half second (`IsTimeEqualTo` 0x50EC70), `UpdateGroupsFromKeyFrame` 0x596570, the groups' moves, the lights,
the clock + 1 back to 0 at 5 × Length × 120.

- `UpdateGroupsFromKeyFrame` 0x596570 → `UpdateGroupsBasedOnAction` 0x5965C0: each group number of the action, from
  the stack's head; a number not below the group count makes **one** group (0x5965FC..0x596681, its Index the number
  asked for, appended); then the group at that number is looked up (0x59669F..0x5966C8), none when it is still past
  the end; `UpdateGroupBasedOnAction` 0x596700 (vt +0x504). No shipped file asks for a group past its count.
- `UpdateGroupBasedOnAction` 0x596700 jumps on the action type (0x596716); SET_ACTION (3, 0x596768) calls
  `UpdateGroupBasedOnDancerAction` 0x596FD0 with the clock truncated. That copies the move to the group
  (+0x5C..+0x68), works its rate out, and then, for every move (none excepted, and with no test of its end), walks the
  group's dancers in their list order (0x59710A..0x597161): a creature (vt +0x34) is passed over; any other plays its
  state's clip again (`Living::SetAnim(1)`, vt +0x8FC → 0x5ECB80, which never starts a dancer's clip over), and, when
  its mesh has a clip and `IsReadyForNewAnimation(1)` 0x5EC960 holds (the clip has played through in the turns since
  its state changed), the clip's time goes to 0 (vt +0x188) and so do those turns (+0x90, 0x597156). So it happens
  once for each action and group, inside the key frame's loop, before the groups' moves go on (and may finish the
  move).
- `UpdateNumeratorsAndDenominator` 0x597190: the largest proportion of the shared groups (+0xEC); none → nothing; the
  largest number from it down that divides them all; each group's numerator (+0xF0) = its proportion over it, the
  Denominator (+0x98) = 100 over it.

### Joining and leaving

- `FindDanceGroup` 0x597400: the sex mask 3; a villager 1 or 2 (its info +0x1F8, the woman's flag, + 1:
  0x59743C..0x59744D); the living's dance type (vt +0xB14: `Living::DanceType` 0x417320 = 0; the creature's own
  0x474000). The fixed groups (+0x80 list) first, then the shared ones (+0x88) by the proportion counter.
- `Living::RemoveFromDance` 0x5EF930 (the Villager's 0x751510 calls it): NumDancers (+0x90) − 1, a fixed group's count
  (+0x78) − 1, every node of the living taken out of its group's list, `ReassignIndices` 0x50D490, the group pointer
  (+0xD8) cleared. `Living::IsDancing` (vt +0x978): the group pointer is set.
- `Villager::RestartDance` 0x751980 (bw1-decomp Villager.cpp:1272; unreachable: not virtual, no caller, and its
  address is stored nowhere in the exe, so it is dead code in v1.42): while dancing (vt +0x978), the
  dancer's place (`Living::CalculateDancePosition`) and a walk there (`Villager::SetupMoveToPos`) with the given state;
  1, else 0.
- `GroupBehaviour::FindFirstDanceMember` 0x5981D0: the groups 0..numGroups − 1 (+0x40, the file's count: **none for a
  dance without a file**), the first dancer that is not the given thing.
- `Dance::FindLiving` 0x50C3D0: every group of the list (+0x78), each one's dancers (+0x1C) in order, the first the
  filter takes.

### The scripts' dances (script type 10)

`Dance::GetScriptObjectType` 0x50C3C0 = 10; `Dance::IsScriptContainer` 0x50B710 = 1. Its row of the script type table
(0xC0C730 + 10 × 0x18): no find at a point, `FindInDance` 0x6F7520, `FindInDanceNear` 0x6F75C0, `FindInDanceNotNear`
0x6F76C0, `DanceLoop` 0x6F78E0 (each dancer through `LoopFnCheck` 0x6F7800 via `Dance::FindLiving`).

- FLOCK_ATTACH (`GScript::AttachToId` 0x6EF3C0) pops the leader flag, the target's id and the obj's id, in that order
  (so the script pushes the obj, the target, the flag); no obj → an error at 0x6EF4B4, no target → 0x6EF4AD. The
  target is tried as a dance first (vt +0x3F0), then a flock (vt +0x3EC, 0x6EF4E0), then a town (vt +0x1B8,
  0x6EF8A0), else the error 0xC0C330. With a dance target (0x6EF426): `SetControlledByScript(dance, 1)`, then
  fn_006F0320(obj id, target id, dance, leader) (0x6EF444):
  - a container obj (0x6F0335): the dance kept at g_game +0x250090 +0x4C, the obj's loop with 0x6F02A0 on each member:
    a living (others passed over) leaves any dance (vt +0x978 / +0xB08), `FindDanceGroup`, `SetScriptState(5
    IN_DANCE)`, `SetControlledByScript(1)`, 0x70D020 (its slot's reference + 1; "Jonty-No slot for thing! VBAD" when it
    has none); "No Loop funtion for type" without a loop; the target pushed;
  - a living: with the leader flag it becomes the dance's Target (+0xFC, 0x6F03D8); leaves any dance;
    `FindDanceGroup`; `SetScriptState(5)`; `IncrementScriptReference(obj)`; the target pushed (0x6F0412). Any living,
    not only villagers;
  - else "JONTY-Trying to add non living to dance" and 0 (0x6F03AD).
- FLOCK_DETACH from a dance (`DetachFromDance` 0x6F0430): an obj that is a living: `RemoveFromDance(0)`,
  `DecrementScriptReference`, its id pushed; not living: "Trying to remove non living from dance", "Thing not removed
  from Dance", 0. No obj: `FindFirstDanceMember(NumDancers == 1 ? none : Target)` (0x6F04BB..0x6F04CD); none → "Thing
  not removed from Dance", 0; else `RemoveFromDance(0)`, fn_0070D2F0 (its id, a slot made when it has none), the id
  pushed, `DecrementScriptReference`.
- FLOCK_DISBAND (`DisbandId` 0x6EFDC0, the dance at 0x6EFE9B): while NumDancers ≠ 0: `FindFirstDanceMember(none)`,
  `RemoveFromDance(0)`, `DecrementScriptReference`, a controlled one `SetScriptState(4)`; none found → "Should never
  happen" and it asks again (an endless loop: never reached by the game's dances).
- ID_SIZE (`GetIdSize` 0x6EFF50, 0x6EFFC1): NumDancers (+0x90) as a float.
- Releasing a controlled dance: disbanded (`ReleaseControlFromScript` 0x70D540 → `DisbandId`);
  `ReleaseContainerContents` 0x6EFCD0 and `DecreaseContainerContentsScriptReference` 0x6EFD60 do flocks only ("CANNOT
  RELEASE THIS! USE DISPAND!" for a dance).

### The scripts' dance state (IN_DANCE 5, MOVE_TO_DANCE_POS 154)

- `GScript::CreateDance` 0x6F0170 (DANCE_CREATE 054): pops the time (0x6F0181, read as a float), z, y, x (the LHPoint
  at S+0x14), the type (0x6F01DB) and the object (`GetScriptGameThing` 0x70D220, may be null, not checked). `new
  0x12C`, `__ftol(time)` 0x6F021F, `Infos + type × 0xB0` with **no range check** (0x6F0224..0x6F022F; the type is the
  raw popped dword), `MapCoords(LHPoint)` 0x603160, `Dance::Dance(pos, info, object, ftol, 0, no town)` 0x50B6A0. Then
  `+0xB4 = 1` (0x6F0253), `AddScriptGameThing(dance, 1)` 0x70D0F0, `PUSH(id)`. "Dance not created" and 0 only when
  `new` fails: a dance whose file is missing is made. No random draw in the native, the constructor or `Initialise`.
- **+0xB4, made by a script**: `Initialise` clears it (0x50B7BC), only CreateDance sets it. Read by `Dance::Process`
  0x50BD22 (the 10 s camera watch flag +0xB0 is never set for it) and by `UpdateGroupBasedOnAction` 0x596CEB (action
  18 starts no dance camera for it). These are its only readers, with `GroupBehaviour::Save` 0x59855C and `Load`
  0x598922 for saved games: a scan of every [reg+0xB4] access in the code, with the 326 entries of Dance's vtable
  0x8D3968 checked.
- The game's dance list (g_game +0x205BD4, count +0x205BD8) is **newest first** (prepended, 0x50B898..0x50B8B3);
  `ProcessDances` 0x50BB60 reads the next link before each `Process`; a removal unlinks in place.
- `Dance::Process` 0x50BB90: an object set (+0xFC) that is not `IsAvailable` (vt +0x2C) → the dance itself not
  available (0x50BBB1) → return at once (0x50BD82); else a creature object's +0x1074 = 0, +0xFC = 0, `ToBeDeleted(0)`
  (vt +0xC). `Villager::IsAvailable` 0x751D50: the unavailable flag clear and the **final** state not 14 DYING; every
  other thing `GameThing::IsAvailable` 0x401810, the flag only.
- `Dance::ToBeDeleted` 0x50B970: for each group, each dancer **whose unavailable flag is clear**: `RemoveFromDance(1)`
  and `SetStateAfterFinishingDance` (vt +0xB0C: Villager 0x759B80 = SetTopState(163); Living 0x417300 nothing). A
  villager in 5 or 154 then has its exit `ExitDanceInScript` 0x5EDB10 told 163: not a script state, so it leaves the
  dance and answers `ExitNoChangeState(163)` 0x768780 = 0: it **stays in 5 / 154 with no group** until the script sets
  its state again.
- Attach (`fn_006F0320`, see above) → `SetScriptState(living, 5)` 0x6F82E0: a creature plays its script clip
  (+0x128C) instead; any other living that is available and in the map gets its exit and entry for 5 called (results
  ignored), `SetAnim(1)`, word +0x58 = 0. It happens whether `FindDanceGroup` found a group or not. An animal's entry
  (0x41A310) has no function behind it and sets the state straight. Villager row 5 (0x59D511..0x59D584): state
  `Living::StateInDance` 0x5EDAD0, entry `EnterDanceInScript` 0x5EDA50, exit `ExitDanceInScript` 0x5EDB10, clip
  `Villager::DanceAnimation` 0x423850. Row 154 (0x5A668B..0x5A6701): `Villager::MoveToDancePos` 0x759990, no entry, no
  exit, clip `MoveToPosAnimation` 0x423400, validate `WallhugValidate` 0x756990. Animal row 5 (0x5ACEF4..0x5ACF14):
  `StateInDance`, no entry, exit, save or load; its clip slot is not written there. Animals have no row 154.
- `StateInDance`: with a group, `PerformDance(dance pos, 5, group +0x5C)` 0x5EF9F0: the place
  (`CalculateDancePosition` 0x597F20 with the index +0xDC), a discarded distance (0x5EFA21), d = the 2D distance from
  pos +0x14 to the place (0x74CD50), the facing by the action (table 0x5EFD10) through `MobileWallHug::SetGameAngle`
  0x60DA90; d > 0: a villager `SetupMoveToWithHug` 0x5F2890 when d > 4 m (the hug mode 0xC), else
  `Villager::SetupMoveToPos` 0x763800, then `LivingAction::SetState(0, 154)` 0x5ECC90 (the top byte and +0x90 = 0 only:
  FINAL stays 5, no exit, entry, clip, speed or town); another living `Living::SetupMoveToPos` 0x5F2830 (no 154); both
  then `MoveToPos` 0x5EC270, the first step this turn (0x5EFCF9). No random draw. The facing 2 is LookAtObject of
  another group's first dancer, and 14 the belief.
- The point of DANCE_CREATE: `MapCoords(LHPoint)` 0x603160 zeroes the three fields and calls `MapCoords::Set`
  0x603340: x = ftol(p.x × 6553.6f), z = ftol(p.z × 6553.6f) (24-bit products), altitude 0, then altitude = p.y −
  `LH3DIsland::GetAltitude` 0x803090 of that position (0x603371..0x60337C).
- The facing's angle to the group's centre, `DanceGroup::GetAngleFromPosToGroupCentre` 0x50D460: `CalculateActualPos`
  0x50D510 (the dance's `GetPos` 0x401960, plus `MetersToWholeX/Z` 0x74E1D0 / 0x74E200 of the offset +0x48/+0x4C of the
  group and of each parent through +0x70: ftol, int to float, ÷ 10, × 65536, ftol), then
  `GUtils::GetAngleFromXZ(pos, centre)` 0x74D240, its int loaded as a float (fild, exact). Action 7 ftol's it back
  (the same integer); action 8 adds 1024.0 and takes 2048.0 off when strictly above (exact in float).
- `MoveToDancePos` 0x759990: no group → 0; the group's moved flag (+0x7C, cleared at the start of
  `DanceGroup::Process` 0x50CFFE, set at 0x50D0FC) → the goal +0x80 = the new place, the walk not set up again (no
  re-aim); then `MoveToPos` 0x5EC270; arrival (10) `SetTopStateToFinal` 0x5ECA80 → 5, with no pause draw (row 5 cannot
  pause).
- `Villager::SetStateSpeed` 0x753760 keeps the speed while `IsDancing` (+0xD8 set), whatever the state.
- `DanceAnimation` 0x423850: no group 0x181; the dance stopped (+0x100 == 1) 0x171; else by the group's action (table
  0x423A00), action 4 the only random draw: `GameRand(3)` 0x6DE510 + 0x13A (info +0x1F8 == 0, a man) or 0x109; action
  9 tests `IsMoving`.
- The script's reminder (`DataForScriptRemind`) is made each turn in IN_DANCE, kept by the exit and read by the entry.

### Shapes

46 shapes × 256 points (`DanceShapePoints` 0xCADB70), from the table 0x8D4838 (rows of 0x84: the maker and a file
name): 0..12 computed (`MakeShapeCircle` 0x50F9A0 .. `MakeShapeRandomScatter` 0x50FD90), 13..45 read from
`.\data\<name>.dan` by `GData::LoadShape` 0x510340 (u32 count + count × (i32 x, i32 z); all 35 shipped files hold 256
points). The table's makers for the read shapes are `MakeShape::LoadShape` 0x510330 (bw1-decomp `DanceShape.cpp`: the
"Star", "Man", "Cock", "Fish", "Lightning"... rows), a ten-byte jump to `GData::LoadShape`.

The precision control is 24 bits while the shapes are made: `RenderLoadingFrame(char*)` (0x5503F3), the call just
before `InitialiseShapes` (0x550401), ends with `LHResetFPU` 0x54C16A. So every fmul, fadd, fsub, fdiv, fsqrt and fiadd
rounds to a float, while the results of fsin, fcos and fpatan stay extended on the x87 stack until the next operation
rounds the product. The circle is ftol(r24(fcos(a) × 65535)) (0x50F9CC..0x50F9D4), with a = r24(e ×
0.012271846302734375); the wavy circle, the spiral, the wave (shape 3, ftol(r24(fsin(a) × -65535)), 0x50FE70) and the
octagon's corners take their sines and cosines the same way. Rounding the sine to a float before the product moves
points 54, 74, 138 and 246 of the circle and points 155 and 165 of the wave by 1 unit (e.g. circle point 54: sin =
0.97003123952671…, × 65535 = 63570.99728, which rounds to the float 63570.99609375 and truncates to 63570).

- **CalculatePointOnLine** 0x510400 (from, to, i, n), used by the octagon, the triangle and the heart: dx = to.x −
  from.x and dz = to.z − from.z loaded with fild, angle = fpatan(dx, dz) kept extended (0x510433); dist =
  fsqrt(r24(r24(ez²) + r24(ex²))) with ex = from.x − to.x, ez = from.z − to.z (0x51044B..0x510455); x =
  ftol(r24(r24(r24(r24(fsin(angle) × i) × dist) / n) + from.x)) (0x510463..0x510471), z the same with fcos and from.z
  (0x510476..0x510486).
- **Triangle (8)**, `MakeShapeTriangle` 0x50FF50: c = 0.5235987755833333 (qword 0x8D6000). P1 = (ftol(r24(sin c ×
  65535)), ftol(r24(cos c × 65535))) = (32767, 56754), P2 = (−65535, 0) (the literal 0xFFFF0001 at 0x50FF5E), P3 =
  (P1.x, ftol(r24(cos c × −65535))) = (32767, −56754). Points 0..84 = Line(P2→P1, i = 0..84, n = 85)
  (0x50FFBA..0x50FFF0), 85..169 = Line(P1→P3, i = 0..84, n = 85) (0x50FFFD..0x51003A), 170..255 = Line(P3→P2, i =
  0..85, n = 85) (0x510047..0x510087); the last point is P2 again.
- **Heart (9)**, `MakeShapeHeart` 0x510090, with k = 0.03695991357058823 (0x8D6020), h = 1.57079632675 (0x8D6018) and r
  = 32767: points 0..41, e = 0, 2, .., 82 (0x5100A8..0x5100F0): a = r24(r24(e × k) − h), x = ftol(r24(fcos a ×
  −32767)), z = ftol(r24(r24(fsin a × 32767) − 32767)); points 42..84, e = 0, 2, .., 84 (0x5100FA..0x510142): x the
  same, z = ftol(r24(r24(fsin a + 1.0) × 32767)). Corners (0x510144..0x510164): Pa = (0, −65534), Pb = (0, 65534), Pc =
  (131068, 0). Points 85..169 = Line(Pb→Pc, i = 0..84, n = 85), 170..255 = Line(Pc→Pa, i = 0..85, n = 85). So two half
  circles on the −x side and a tip at +x; the last point is the first again.
- Points from an exact x87 model (trig correctly rounded to 64 bits, every other step rounded to a float): triangle
  [1] = (−64378, 667), [42] = (−16962, 28043), [94] = (32767, 44735), [127] = (32767, 667), [212] = (−15805, −28710);
  heart [1] = (−2419, −65444), [20] = (−32627, −35790), [41] = (−3625, −201), [42] = (0, 0), [63] = (−32761, 32161),
  [84] = (−1210, 65511), [128] = (66304, 32381), [169] = (129526, 770), [212] = (66304, −32381), [255] = (0, −65534).
  No shipped dance file sets shape 8 or 9. The smallest gap to a float rounding tie after a trig step is 3.3e-4 of an
  ulp, far above x87's or the C library's trig error, so the result does not depend on which one computes it. This is
  a static model, not a capture from the running game.
- **RandomScatter (12)** (verified with xref) draws per point r = `GameFloatRand(65536)` then a = `GameFloatRand(2π)` (0x50FDB3, 0x50FDCB):
  512 synced draws. `GData::InitialiseShapes` 0x50F960 is called once per run, from `GGame::LoadFiles` 0x550390 (0x550401) ←
  `InitOneTimeOnly` 0x54EF40 (0x54F091) ← `pc_main` (0x641DAA), before `StartGame` → `GGame::Init` sets both seeds to
  0x88F89F (0x54F4BA); every later land `GData::Reset` 0x510750 sets them to 0 (see
  [Random numbers](../engine/random.md#random-numbers)). So these draws never reach a land's random stream. No dance file
  uses shape 12.

### A creature's turn in a dance with villagers

- The dance sub-action's turn (the part at 0x504700 of the sub-action table entries set at 0x4F9D3F and 0x4FA9E3, run
  every turn of the sub-action: its tail walks the creature back to its place when further than 0.2, to within 0.1)
  first ends the actions the move stops (0x504745), then switches on the group's move (`[group+0x5C]` − 0x1F,
  0x5047C8). The Action move calls `LH3DCreature::StartIndividualAction(anim)` (0x504923), the second action calls it
  with 0xDA (0x504953), the Pose move calls `StartStaticAction(anim)` (0x50496A), the Talk move
  `StartConcurrentAction(0xCD)` (0x50493B, no draw), the Face move `StartFacialAction(anim, 2000)` (0x504A52).
- Both one-argument starts draw the mirroring **before** they ask whether the body is free:
  `StartIndividualAction` 0x484B00 takes `GameRand(2)` (0x484B41; `LocalRand(2)` without a game creature, 0x484B5C)
  unless the creature holds an object (+0x48D0), when the mirroring is the holding hand's (`+0x5230 == 0`, no draw);
  `StartStaticAction` 0x484310 takes `GameRand(2)` (0x484329) or `LocalRand(2)`. Only then do the two-argument starts
  (0x484B80, 0x4843A0) refuse while `IsPerformingBodyAction`. So every turn an Action, second-action or Pose move is
  the creature's move, the game's random numbers move by one, whether or not the body starts anything.
- The dance moves themselves: `StateSet(5)` draws `GameRand(7)` + 147 (0x484F4D) and the later moves `GameRand(5)` +
  147 (0x485709), each drawn again until the species has the animation.
- The creature's last dance (read 2026-10-18): its turn +0x10FC and its type +0x1100 are written only by
  `Creature::SetToZero` 0x474130 (0 for both, 0x474279 / 0x47427F, run by both constructors 0x473D56 / 0x474878) and by
  `CreatureAgenda::ConstructSubActionsForDanceImpressivelyWithVillagers` 0x49C960 (the turn and the type, 0x49C972 /
  0x49C983). `LastDanceWasntToDanceToImpress` 0x4D2DB0 refuses while the type is 0xC (12) or the game turn
  `[g_game+0x205A40]` less the last turn is at most 0xC8 (200, unsigned); `LastDanceWasntDanceCreature` 0x4D2DE0
  and `LastDanceWasntDanceStory` 0x4D2E10 the same way with type 1 and 0x12C (300), type 9 and 0x1C2 (450). So a
  new creature refuses all three for its first 200, 300 and 450 turns of the land.
- The creature actions' table (filled by `_$E216` 0x491BA0, rows 0x50 apart: the name at +0, the builder's member
  pointer at +0x20, its dance type at +0x30) [V]: "PlayGameWithVillagers" (name at 0x499723, builder
  `ConstructSubActionsForDanceWithVillagers` 0x49C9A0 at 0x499644, type 0xA = 10 at 0x49972D) and
  "DanceAmorouslyWithVillagers" (name at 0x49A06A, builder 0x49C9A0 at 0x499F87, type 0xD = 13 at 0x49A074), the types
  being the rows 10 CreaturePlayingWithVillagers and 13 CreatureDanceAmorous of the dance table. openblack:
  `creature_plan_actions::For` (`k_GameDanceType`, `k_AmorousDanceType`); test
  `CreatureDance.TheGameAndTheAmorousDanceAreDancesWithVillagersOfTypesTenAndThirteen`.

### Sources

- bw1-decomp: `src/Black/Dance*.cpp`, `DanceShape.cpp`, `Villager.cpp`; the rest read in the exe with the xref index;
  the shapes checked against an exact x87 model.

## openblack

**Done** for the scripts' dances; **not yet** the worship sites' dance, the dance camera and lights, a few actions.
Code: `components/dance` (the `.dnc` reader, `DanceFile`), `src/ECS/Components/Dance.h` (the `Dance`, `DanceGroup` and
`Dancer` components), `src/ECS/DanceRules.{h,cpp}` (start, speed, membership, key frames, clock),
`src/ECS/DanceShapes.{h,cpp}`, `src/ECS/DanceMoves.{h,cpp}` (moves, places, facing, clips), `src/ECS/Dances.{h,cpp}`
(make, join, leave, a dance's turn), `src/ECS/Systems/Implementations/DanceSystem.{h,cpp}` (`Locator::danceSystem`,
step 4 of the turn), the dance branches of `src/ECS/ScriptContainers.cpp`, the file cache `DanceFileLoader`
(`src/Resources/Loaders.cpp`). The worship site's side of the dance is on
[The temple and the citadel](../magic/temple-and-citadel.md). Progress: [Dance scripts](../project/progress/).
Porting decisions: [Port decisions: beings](../project/port-decisions-beings.md).

### What openblack does

- The reader reads as `LoadDance`: nothing refused, the key frames ordered by time on reading, a short file keeps what
  it read (`DanceResult::ErrTruncated`, logged), bytes after the end are left (`ErrTrailingBytes`, logged). The file
  cache gives no file when it can't be opened; the dance is made all the same, with one group and no key frames.
- `Dance` defaults follow the constructor: loop length 5, round length (Denominator) 100.
- A key frame naming a group past the last makes one group, as the game.
- The scripts' container commands (FLOCK_ATTACH, FLOCK_DETACH, FLOCK_DISBAND, ID_SIZE, CALL_IN, the container loops)
  take dances as above. A dance's script type is 10. FLOCK_DISBAND's endless loop ends instead.
- ✅ The town footpath: `DanceSystem::Create` takes the town the dance is made for (none from DANCE_CREATE, the
  artefact's `TownArtefact::town` for an artefact's dance) and, for the rows `dance_rules::LaysTownFootpath` gives
  (1..18: the original's rule, every row but 0 and 19..24, on the rows of the table; the native refuses the rows past
  it, below), lays `footpaths::GenerateRoutesBetween(pit, dance)` from that town's storage pit, or with none given from the
  pit of `map_cells::GetNearestTown(place, FLT_MAX)`. The dance's end is the walks' arrive point for a dance
  (`living_footpath::ArrivePosOf`, half a cell along x, `k_DanceArriveOffset`). A creature's dance
  (`CreatureMindSystem::CarryOutDance`) gives no town, as `Creature::SubStatePerformSetDance` 0x606103 passes none. Tests: `test/test_dances.cpp`. The path finding window's "Move On
  Footpath" tab counts the links and their holders, the dance's among them.
- `Locator::danceSystem` is emplaced; its turn runs at step 4 (`Magic/MagicLoop.cpp`), the dances newest first by
  their making order (`Dance::made`), so a dance that goes leaves the others' order. The worship sites do not use the
  dance yet.
- DANCE_CREATE (`src/CHLApi.cpp`) makes a dance through the dance system: the point made a map position as the game
  does (`map_coords::FromWorld`: x and z to 16.16, its height above the land as the altitude), the time truncated to
  turns, made by a script (`Dance::madeByScript`, which waits for the dance camera, its only use), added as a thing the
  script made, its id pushed. A row outside the table makes no dance ("Dance not created", 0), by decision
  (2026-10-25): the original makes it with no check (0x6F0224..0x6F022F), its rows 25..27 reading the empty memory
  after the table (no file, no start of its own, and a footpath by the rule above) and the rows from 28 on, and the
  negative ones, runtime state no one can know; no shipped script asks for one (`challenge.chl` passes only 14 and 6),
  so emulating those reads, which only a mod could reach, is not progress.
- The dance's target is gone by the game's rule (the flag; a villager's final state DYING); a dance that is itself
  going waits; its deletion passes over dancers that are going and tells the others to decide, which a dancer in the
  scripts' dance state refuses.
- IN_DANCE 5 and MOVE_TO_DANCE_POS 154 (`src/ECS/Systems/Implementations/VillagerDance.cpp`, the place and facing in
  `dances::DancerTurnOf`), on our walk: `villager::SetupMoveToWithHug` past 4 m, `villager::SetupMoveToPos` otherwise,
  154 written straight, the first step the same turn through MOVE_TO_POS's step. The places are taken through metres
  (our walks keep their goals in metres), so a dancer that has walked to its place is at it. An animal attached to a
  dance runs its own IN_DANCE (`AnimalAI.cpp`): it walks straight to its place and never takes 154. The facing's angle
  is `dance_moves::GroupCentre` (the same whole-metre steps) and `gutils::GetAngleFromXZ`, bit for bit.
- The key frame's clip step: `dance_rules::ApplyKeyFrame` tells `startsMove` each group as the action starts its move,
  and `villager_dance::PlayClipAgain` does the dancer's part (villagers; an animal dancer's part is not ported).
- The group's Centre, LastCentre and Speed (read only by the group's Process and its save / load) are not kept.
- The dancers' clip is `DanceAnimation`'s; "dancing" for the state speed is being in a dance's group (the worship
  site's own dancers keep their stand-in until the site uses the dance).
- The computed shapes (`src/ECS/DanceShapes.cpp`) are worked out in double with each step rounded to a float, as the
  game; the sines, cosines and line angles stay double until the product that uses them, as the x87 keeps them
  extended. The triangle (8) and the heart (9) are made as above. A test pins the points and a weighted sum Σ(k+1)(3x
  + 7z) over all 256 points of each: circle −4809760068, wavy −4853608447, spiral −1463442656, wave 657463520, octagon
  −4561453056, triangle −2895563378, heart 3813671733.
- The shapes from files (13 on) are read from the data folder's `.DAN` files once, when a dance first needs them
  (`DanceSystem::Shapes`, `src/ECS/Systems/Implementations/DanceSystem.cpp`, through the resource cache
  `DanceShapeLoader`; the reader `components/dance/src/DanceShapeFile.cpp`, test `DanceShapeFile.*` in
  `test/test_dance_file.cpp`); a file that can't be read keeps its shape at the centre. Ported from raffclar's tree,
  not yet compared with v1.42.
- The "Untitled" names of version 0 files and the global version are not kept. openblack does save and load games
  ([Saved games](../engine/saved-games.md)); its dance writer (`src/ECS/SavedWorld/WriteDances.cpp`) refuses a group
  with no name that no loaded record holds.
- A creature in a dance with villagers (`creature_mind`'s dance step, `src/Creature/CreatureIdleMind.cpp`) draws the
  mirroring of an Action, second-action or Pose move every turn the move is its move, with the game's random numbers,
  and starts it only when its body is free, as above; an action while it holds something takes no draw and is
  mirrored unless it holds it in its left hand (`CreatureHeldObject::mirrored`, the take's mirror, as +0x5230), as
  `StartIndividualAction` 0x484B00.
- A species without the dance's start, first move or finish (145, 147, 146) does not dance
  (`CreatureAnimationSystem`), as `LH3DCreature::StartDanceAction` 0x484830 refuses. The two differ in when they
  refuse (the original never starts the dance; ours clears it at the animation's turn), but no shipped species reaches
  it: all 17 `Data\CTR\*.CBN` (and the 24 of Creature Isle) have a base animation with frames at 145 Nstart,
  146 Nfinish and 147 Ca (ctrspec27, read 2026-10-18 from each file's Creature block offsets and frame counts).
- The last dance starts at turn 0 and type 0 (`CreatureDance`, `creature_dance::MayDanceAgain`, the record absent
  until a creature's first impressive dance), as the original's creature does.
- A creature's dance around a thing (action row 251) is done to a thing worth playing with (`Target::Playable`): the
  row has no object filter of its own in the original, so the Play desire's test decides,
  `GameThingWithPos::CanBePlayedWithByCreature`. `Living` (0x417390) refuses a villager or an animal that is dead
  (`IsDead`, vt +0xAF4: the dead status bit, the DEAD state or not functional) and otherwise asks its type's "may be
  played with" flag (type info +0xC8), as `Object` (0x402A60) does; `Creature`, `MobileObject`, `MobileStatic` and
  `Tree` answer yes. Ours refuses the dead ones (`villager::IsDead`, `animal_ai::IsDead`); the type flag is not read yet.

### Test hooks

None of its own; the scripts' dances run in the intro and the Pied Piper's challenge.

## Pending

- The dances outside the temple and around a thing (raffclar #580, unverified against v1.42): they make no dance of their own and start the creature's last one (`creature_temple_dance`); not compared with the original, and their own dance clips wait for his #586.

- **DANCE_CREATE with a creature** as the dance's object, its +0x1074 cleared when the dance goes (queued fix).
- **The scripts' dance state**:
  - the script's reminder (`DataForScriptRemind`), and with it the reactions' dance tests (`VillagerReactions.cpp`:
    out of the dance on reacting, back to it after) (queued fix); a dancer already notices a thrown thing only when
    it comes at it ([Villager reactions](villager-reactions.md#openblack));
  - facing 2 (LookAtObject of another group's first dancer) and 14 (the belief) (queued fixes);
  - the walk past 4 m is our `SetupMoveToWithHug`'s LINEAR walk, not the game's hug mode 0xC (queued fix);
  - action 9's `IsMoving` test in the clip (queued fix);
  - a creature attached to a dance (it plays its script clip; openblack does not take creatures into dances) (queued
    fix); the animal row 5's clip (queued fix).
- **An animal dancer's clip played again** (`Animal::GetAnimId` 0x417FA0) (queued fix).
- **RandomScatter's points**, drawn at start-up (see above) (queued fix).
- **The actions** CHANGE_GROUP (7), SET_STATE (9, 10) and REMOVE_PATH's re-index (5) (queued fixes).
- **The dance camera** (actions 18, 19) and the lights (20..23) (queued fix).
