# CHL commands that move things

## Summary

The challenge scripts (CHL) tell the story of each land. They create the people and things of a scene, walk them to
places, make them play an animation, wait until they have finished and delete them when the scene is over. The same
set of commands also drives the camera, the help spirits, the timers, the day's clock, the sound and music, the intro
film, and the magic, weather and worship of a land.

A script holds every thing it uses through a slot table with reference counts. A thing that no script variable holds
any more is given back to the game at the next turn, and goes on with its normal life.

The commands are natives of the script virtual machine (`ScriptLibraryR.dll`). The VM's `SYS` calls a native, which
pops its own arguments; the VM's `CAST` converts numbers. The commands that move things are `MOVE_GAME_THING`,
`SET_POSITION`, `SET_SCRIPT_STATE`, `SET_SCRIPT_ULONG`, `PLAYED` and `DELETE`.

This page owns the commands themselves; what a being does when a command reaches it is on the being's page (the
creature: [Script control](../creature/script-control.md)). openblack runs every command on this page with the
original's rules, so the Land 1 intro (`FollowUs`) runs through the film, the rescue and the hand demo; the script
memory a villager keeps of its walk (`DataForScriptRemind`) is not ported yet.

## The original

**Faithful**, read in the v1.42 `runblack.exe` with the 1.20 symbols, except what is marked. The script messages
quoted below are all silent in the shipped game ([What the game asks of the VM](#what-the-game-asks-of-the-virtual-machine)).

The creature's side of `SET_POSITION`, `SET_FOCUS` / `RELEASE_FROM_SCRIPT` (0x6FB380 → 0x70D540 → 0x70F600 →
0x4F7060), `SET_SCRIPT_STATE` / `SET_SCRIPT_ULONG` / `PLAYED`, `MOVE_GAME_THING` and `CREATURE_DO_ACTION` is on
[The creature under a script](../creature/script-control.md); `PLAYED`'s busy test and the completion two turns
later are in [Playing what a script gives it, and PLAYED](../creature/script-control.md#playing-what-a-script-gives-it-and-played).

<a id="script-commands-that-move-things-move_game_thing-033-and-friends"></a>
### Script commands that move things (MOVE_GAME_THING 033 and friends)

The pattern every scene uses, as the **Land 1** intro shows it (CHL `FollowUs`, `Scripts\Quests\challenge.chl`; in
the decompiled listing of challenge.chl from line 49528):

1. **Create.** The family is made with `CREATE` 027: mother = VILLAGER 49, father = 53, son = 52.
2. **Move and wait.** `MOVE_GAME_THING(thing, point, 0.0)`, then a loop on
   `GET_DISTANCE(GET_POSITION(thing), point) == 0` (`FollowUs_loop_4` and `_loop_6`) or `< 1` (`_loop_5`,
   `_loop_77..79`).
3. **Act and wait.** `SET_SCRIPT_ULONG(thing, clip, times)` + `SET_SCRIPT_STATE(thing, 200)`, then a loop on
   `PLAYED(thing)` (`_loop_8`, 11, 17, 45, 48, 49, 74, 80..82).

### MOVE_GAME_THING

`GScript::MoveGameThing` 0x6F8E80 pops the radius, z, y, x and the thing (0x6F8E91..0x6F8EE9); `GetScriptGameThing`
0x70D220 (none: "Thing no longer valid" 0xC0C258, at 0x6F8EEF). `MapCoords(pos)` 0x603160 (x and z; y stays relative
to the land). Then it goes by type, in this order:

| Test (vtable) | What it does |
|---|---|
| IsCreature (+0x34, at 0x6F8F15) | `dynamic_cast<Creature*>` (0x6F8F2B; "no creature for script" 0xC0D598), IsObjectInMap (+0x178) → `fn_004F6B60`(pos, radius) at 0x6F8F60: `Creature::PrepareCreatureForScriptedAction` 0x4F6A90 and sub-actions (`AddSubAction` 0x4FF240). Only here is the radius used ([Sent somewhere by a script](../creature/script-control.md#sent-somewhere-by-a-script-move_game_thing)) |
| IsLiving (+0x3C4, at 0x6F8F70) | IsObjectInMap (0x6F8F7E) and not IsDrowning (+0x17C, 0x6F8F90), otherwise nothing (0x6F907E); `AreWeThere(coords, 0.0)` (+0x85C, 0x60AD60, at 0x6F8FB7) == 0 → `Living::SetupMoveToPos(coords, 4 IN_SCRIPT)` 0x5F2830 (0x6F8FCA); already there → `GScript::SetScriptState(thing, 4)` 0x6F82E0 (0x6F8FD7) |
| IsFlock (+0x3EC, at 0x6F8FEA) | `Flock::SetDomainCentrePos` 0x52FC20: the destination (+0x80) of the flock's **tail** member (0x52FC2C..0x52FC53: a villager's or an animal's), then the flock's centre (+0x14) |
| IsWeather (+0x3FC, at 0x6F9015) | `fn_00774550`: its system's (+0x78) +0x5C = pos |
| IsComputerPlayer (+0x4B8, at 0x6F9036) | `fn_00658510`(pos, 60.0) |
| anything else | "Jonty - Thing must be living to move it!" (0xC0D56C) and `SetPos(coords)` (+0xFC, 0x401940) at 0x6F9058 |

`GameThingWithPos::SetPos` 0x401940: Pos = coords, so the thing's world point is the vector itself.

#### How a Living sets off and arrives

- **`Living::SetupMoveToPos`** 0x5F2830(pos, final): the movement state is the byte at GLivingInfo +0x124
  (1 MOVE_TO_POS for every villager), or 3 MOVE_ON_STRUCTURE if GameThingWithPos +0x24 & 0x80 (set only by
  `Living::MoveOnStructure`). It calls `SetCurrentAndDestinationState(movement, final)` (+0x8DC) and, only if that
  gives 1, `MobileWallHug::SetupMobileMoveToPos(pos)` 0x60AAD0: destination +0x80 = pos, `InitStepsXZ` 0x60BFA0, out
  of the detour lists (`fn_00611AC0` / `00611610` / `00612BB0` / `00610590`, +0x76 = 0), and `AreWeThere(0)` →
  +0x5E = 1 ARRIVED; otherwise `CircleHugInfo::Reset`, +0x78 = 1, +0x5E = 0xB **STEP_THROUGH** (straight, without
  going round: not the LINEAR of `SetupMoveToWithHug` 0x5F2890).
- **`MobileWallHug::AreWeThere(pos, r)`** 0x60AD60: `dx² + dz² < (speed (u16 +0x5A) + r)²` (strict, in MapCoords);
  `AreWeThere(r)` 0x60AD40 = `AreWeThere(GetDestPos() (+0x860), r)`
  ([Walking round obstacles](../beings/wall-hugging.md)).
- The walk advances only from the MOVE_TO_POS function (`Living::MoveToPos` 0x5EC270 →
  `MobileWallHug::MoveTo` 0x60AF20).

<a id="set_position-024-gscriptsetposition-0x6f8a00"></a>
### SET_POSITION 024

`GScript::SetPosition` 0x6F8A00, read in the exe (bw1-decomp has only the table entry,
`{GScript::SetPosition, 4, 0, 0, "SET_POSITION"}`, and the declaration). The messages are all silent in the shipped
game (`ScriptErrorMessage` 0x6F62B0 is a bare `ret`).

1. **Pops.** z, y, x, then the thing (`ScriptDLL::POP` 0x6F6BC0 at 0x6F8A13 / 0x6F8A29 / 0x6F8A3D / 0x6F8A57).
   `GetScriptGameThing` 0x70D220; none: "Thing no longer valid" and return (0x6F8A6B).
2. **Held or flying: refused.** GameThingWithPos +0x24: bit 4 (in a hand) "Setting object in hand to a position"
   (0x6F8A85); bit 0x40 (flying, in the physics) "Trying to set position - Object is flying" (0x6F8A98); either:
   **return, nothing moves** (`test +0x24, 0x44` at 0x6F8AA5).
3. **The point.** `dynamic_cast<Object*>` (0x6F8ABE); `MapCoords(LHPoint)` 0x603160 → `MapCoords::Set` 0x603340:
   x, z = ftol(v × 6553.6), altitude = y − `GetAltitude`. Unless `IsScriptMarker` (vt +0x488;
   `ScriptMarker::IsScriptMarker` 0x561020, the base 0x402390 returns 0) the altitude is set to 0 (0x6F8AE4): **the
   thing goes on the ground; only a script marker keeps its height**.
4. **Not an Object** (a ScriptMarker, a Flock, a Town...): `SetPos` vt +0xFC, which is `GameThingWithPos::SetPos`
   0x401940 for the Flock, ScriptMarker and Town tables, a plain copy into +0x14 (0x6F8C7C..0x6F8C85), and return. A
   flock's +0x14 is its domain centre (`Flock::SetDomainCentrePos` 0x52FC20 writes it; `GetFlockPos` 0x530570 reads
   it).
5. **An Object not in the map** (+0x24 bit 1 clear): "Trying to set position - Object not in map" (0x6F8AF9) and
   return (0x6F8B0B).
6. **A creature** (`CastCreature` vt +0xA4, 0x6F8B15): `Creature::ForceMoveMapObjectWithoutWalking` 0x479EB0
   (0x6F8B26) and **return** (0x6F8B2B): no turn-start copy and **never script state 4**
   ([A script putting a creature somewhere](../creature/script-control.md#a-script-putting-a-creature-somewhere)).
7. **Any other Object**: `MoveMapObject` vt +0x55C (0x6F8B3B; `Object::MoveMapObject` 0x636A40 for Villager, Animal,
   MobileObject, Tree, Pot, Living; `MultiMapFixed::MoveMapObject` 0x52E4F0 for Abode, Feature, MobileStatic). With a
   Game3DObject (+0x40), its matrix +0x14 is built again from `GetScale` (vt +0x120 = Object +0x50) and `GetYAngle`
   (vt +0x508 = Object +0x4C) at (x / 6553.6, GetAltitude + altitude, z / 6553.6), in the four branches on angle == 0
   / scale == 1 (SetScale, Translation / PostTranslation, RotateY), and +0x44 = scale, +0x48 = angle
   (0x6F8B41..0x6F8C3C): **a tilt about x or z is lost**. Then the turn-start MapCoords +0x2C = +0x14
   (0x6F8C40..0x6F8C53) and, if `IsLiving` (vt +0x3C4) and +0x25 & 4 (controlled by a script),
   `GScript::SetScriptState(living, 4 IN_SCRIPT)` 0x6F82E0 (0x6F8C5A..0x6F8C6D). There is no walk stop.

### What the script queries afterwards

- **GET_POSITION** (`GScript::GetPosition` 0x6F88A0): a flock (IsFlock at 0x6F8927) gives its first member's Pos
  (0x6F8931..0x6F893F), or `GetFlockPos` 0x530570 (the centre +0x14) when it has no first member; **a MobileWallHug
  that is not a creature (+0x408, +0x34) gives its destination (+0x80) if `AreWeThere(0)`** (0x6F8977..0x6F89AF),
  otherwise its Pos. The height is `GetAltitude` + the +8 of those MapCoords.
- **GET_DISTANCE** (`GScript::GetDistance` 0x6F8CA0) pops the two vectors (0x6F8CB1..0x6F8D07), then
  `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 = `hypotenuse(dx, dz)` 0x74F6C0: **only x and z** (y ignored at
  0x74CDE8..0x74CDF3). It is 0 if |dx| and |dz| ≤ 0.0001 (0x8BF518), otherwise `1 / InvSqrt(dx² + dz²)` with the
  root approximated by the table `_FUN_0074f620` (table 0xDA5A10, the same as the animals' lairs); **below 0.5
  (0x8AA3B4) it gives 0** (the cut at 0x6F8D4B), so `== 0` is met near the point.
- **IS_ACTIVE** 361 (`GScript::IsActive` 0x6FD7E0): no thing → `ScriptErrorMessage` and false; else vt +0x40C:
  `ScriptHighlight` 0x709750 (+0x60, which `SetActivated` 0x70A630 writes), `SpellDispenser` 0x7226C0 (+0xD0, which
  `fn_00723030` of SET_ACTIVE writes), `Reward` 0x6E5660 (+0x7C, set only by `Reward::InterfaceTap` 0x6E5D9C), every
  other class 0 (`GameThingWithPos` 0x4021E0).
- After SET_POSITION, the script state of the thing is step 7 above.

<a id="set_script_state-017-set_script_ulong-020-played-064-and-the-script-states"></a>
### SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020, PLAYED 064 and the script states

#### The three commands

- **SET_SCRIPT_STATE** (`GScript::SetScriptState` 0x6F8370) pops the state, then the thing ("Object no longer valid"
  0xC0D428; no thing at 0x6F8396).
  - A script container (+0x3F8, at 0x6F83B9): g_game +0x250090 +0x24 = the state and the type's loop function (the
    table 0xC0C73C by `GetScriptObjectType` vt +0x4E8) with 0x6F8280, which calls `GScript::SetScriptState` 0x6F82E0
    on every Living member **with no IsDrowning test**.
  - Otherwise a `dynamic_cast<Living*>` and not IsDrowning (the one Living test, at 0x6F841D) →
    `GScript::SetScriptState(living, state)` 0x6F82E0; otherwise "Object not living for set state" 0xC0D440.
- **`GScript::SetScriptState`** 0x6F82E0, the per-thing step:
  - on anything but a creature: `IsAvailable` (+0x2C; Villager 0x751D50: not being deleted and the final state ≠ 14
    DYING) and `IsObjectInMap` (+0x178 at 0x6F832D: `Object::IsObjectInMap` 0x6392B0, +0x24 & 1, the same slot in
    the Villager, Living and Object tables) → `StorePreviousState` (+0x8EC, 0x763470), `CallExitStateFunction(state)`
    (+0x904) and `CallEntryStateFunction(state)` (+0x90C) without looking at the result, `Living::SetAnim(1)`
    (+0x8FC, 0x5ECB80 → `SetAnim(GetAnimId(), 1)` 0x5ECBA0: the state's clip from 0) and +0x58 = 0;
  - a creature's path (0x6F82EA, 0x6F82F1..0x6F831B) plays its +0x128C clip
    ([Playing what a script gives it](../creature/script-control.md#playing-what-a-script-gives-it-and-played)).
- **SET_SCRIPT_ULONG** (`GScript::SetScriptStateULData` 0x6F8770) pops the times, the clip and the thing (no thing at
  0x6F87A9). By what it reaches:
  - a villager (0x6F8841): +0x120 = times, +0x11C = clip;
  - a creature (0x6F886D): +0x1290 = times, +0x128C = clip;
  - a script container (vt +0x3F8, 0x6F87CD): g_game +0x250090 +0x24 = clip, +0x28 = times and its type's loop
    function (0xC0C73C) with 0x6F8730, which gives each Villager member +0x11C = clip, +0x120 = times;
  - anything else: "setting the state of something neither a creature nor a villager" (0xC0D484, at 0x6F8879).
- **PLAYED** (`GScript::HasPlayed` 0x6F9DC0), by what it reaches:
  - no thing → "Thing no longer valid" and **1** (0x6F9DE1);
  - a creature's plan (0x6F9DF4..0x6F9E7E,
    [Playing what a script gives it](../creature/script-control.md#playing-what-a-script-gives-it-and-played));
  - a villager: IsLiving at 0x6F9E83, the Villager cast at 0x6F9E9C and
    `Villager::IsScriptAnimationComplete` 0x7689D0 called at 0x6F9EAA: TOP 23 WAIT_FOR_ANIMATION → 0; TOP 200 → times == 0; otherwise 1;
  - another Living: `GetFinalState` == 4 (0x6F9EC4..0x6F9ED8);
  - a weather thing → +0x78 == 0 (0x6F9EF0);
  - a PuzzleGame → `fn_006D66E0` (0x6F9F0B, [Fish puzzle](../world/sea-life-and-scenery.md#fish-puzzle));
  - anything else → "Thing not living" and 1 (0x6F9F31).

#### The script states

| State | Per-turn function | Entry | Exit | Clip |
|---|---|---|---|---|
| **4 IN_SCRIPT** | `StateInScript` 0x5ED9A0: creates a DataForScriptRemind if there is none; 1 | `EnterInScript` 0x5ED7E0 (vt +0x940): 1 if `IsStateEntryFunctionSameAs(final, next)` 0x7524D0 or there is no script memory | `ExitInScript` 0x5ED9C0 (vt +0x914), below | the state's own |
| **200 SCRIPT_PLAY_ANIM** | `ScriptPlayAnim` 0x768970: with times > 0, one less and `PlayAnimThenSetState(times ? 200 : 4)` | `EnterPlayAnim` 0x768840 (as EnterInScript) | `ExitPlayAnim` 0x7689C0 = ExitInScript | `ScriptAnimation` 0x768A00 = +0x11C |
| **23 WAIT_FOR_ANIMATION** | `WaitForAnimation` 0x5EC990: `IsReadyForNewAnimation(1)` → `SetTopStateToFinal` and 0, otherwise 1 | | | unchanged |

- **`ExitInScript`**: `CircleHugInfo::Reset`; `IsScriptState(next)` (+0x960, table field 0x18) → 1; otherwise it
  stores the memory and `ExitNoChangeState(next)` 0x768780 = 1 if next can be interrupted by a script (field 0x1C),
  is IN_HAND (`IsStateForInterface` 0x417070) or `IsStateExitFunctionSameAs`.
- **`Living::PlayAnimThenSetState`** 0x5ECAC0: `CallExitStateFunction(s)` and, if 1, `CallEntryStateFunction(23, s)`:
  TOP 23 and FINAL s; the clip does not change.

#### The script memory

`DataForScriptRemind` (Living +0xB0, `Create` 0x5EF190, `KeepThatInMind` 0x5EF1D0, `fn_005EF2A0`) is how a villager
taken out of a script state remembers its walk and resumes it on returning.

<a id="delete-gscriptdeleteobject-0x6f92d0"></a>
### DELETE

`GScript::DeleteObject` 0x6F92D0:

- It pops the mode, then the object (0x6F92E1); nothing for a thing no longer there (`GetScriptGameThing`
  0x70D220).
- Every other path calls `RemoveScriptGameThing` 0x70D1A0 after the mode's action; a container (a town, a flock, a
  dance) is disbanded there (`PUSH(id, 4)` + `DisbandId` 0x6EFDC0). It frees no script slot
  ([Deleting by script](map-script-objects.md#deleting-by-script-object_delete)).
- The modes (jump table 0x6F9490):

  | Mode | Who | What |
  |---|---|---|
  | 0 (0x6F9344) | anything | `GameThing::ToBeDeleted(0)` (vt +0xC) |
  | 1 (0x6F935D) | IsObject vt +0x460 only | a creature gets `Creature::SetFizz(1.0, 2.0, true)` 0x47AB90 and is not deleted here; anything else `GoolooGooloo` 0x5E6540 then `ToBeDeleted(0)` |
  | 2 (0x6F93DA) | IsObject only | `fn_00681230(obj, 0)`, the mesh broken up (15.0, 3.0), then deleted |
  | 3 (0x6F9410) | CitadelHeart only | `fn_00681260(heart, pos, 80.0, 3.0, 0)` or `DestructionSequenceStart` 0x465AB0 |

- A PuzzleGame's own delete is `fn_006D6CC0`.

<a id="the-scripts-things-scriptmanage-slots"></a>
### The script's things (ScriptManage slots)

The ScriptManage code is at 0x70CEE0..0x70D8A0.

#### The slot table

- `GScript::GetScriptGameThing` 0x70D220 looks the id up in the slot table 0xD967F8: 511 slots of 0x14 bytes. Each
  caller prints its own miss: "Thing no longer valid" 0xC0C258, "Thing not found!" 0xC0CFAC, "Object no longer
  valid" 0xC0D428.
- Slot +0xC = created by the script (`GScript::IsCreatedByScript` 0x70D440); +0x10 = the number of script variables
  holding it (`ScriptManage::IncreaseReferenceCount` 0x70D5F0 stops at 0xFF).
- `AddScriptGameThing` 0x70D0F0(thing, created): 1 for the CREATE family, 0 for the finders (CALL, GET_...). A thing
  already in a script keeps its slot (`FindScriptGameThing` 0x70CF50); otherwise a free slot, where `fn_0070D870`
  sets +0xC and clears the count. With created = 1 it also calls the Object's mesh (+0x40) vt +0x98(0) and sets
  +0xA |= 0x40.

#### References

- `IncrementScriptReference` 0x70CF90 → `IncreaseReferenceCount` 0x70D5F0 (the VM's reference callback,
  ADD_REFERENCE): the first reference sets IsInScript and, for a thing the script created (or one already
  controlled), ControlledByScript. `DecrementScriptReference` 0x70CFD0 → `fn_0070D670`: one reference less; the
  release waits for Process.
- The natives `ADD_REFERENCE` = `GScript::AddReference` 0x6FA450 (pushes nothing) and `REMOVE_REFERENCE` =
  `GScript::RemoveReference` 0x6FA470. ScriptLibraryR.dll calls them for a popped object and the variable's old one
  (POP 0x10008BC0) and for a stopped task's object locals (0x10006604); object 0 is the scripts' null (0x10008A64).
- `Object::SetInScript` 0x639B20 sets the +0x24 & 0x200 bit.

#### Each turn: the things nobody holds

`fn_0070D480`, called from `GScript::Process` (0x6EB6DB) right after the scripts' LookIn: every slot without a
reference is released (`ReleaseControlFromScript` 0x70D540, then `SetInScript(0)`) and freed (`fn_0070D800`:
ClearThingOnly + count 0). A thing still controlled there logs "Thing should be released! PANIC" (0x70D504).

#### Giving a thing back to the game

- `ReleaseControlFromScript` 0x70D540(thing, id, fromCommand), only when controlled (+0x25 & 4): a container at a
  task's end is DisbandId'd (0x70D57C..0x70D5AC) and, when the script created it, `SetControlledByScript(0)` +
  `ToBeDeleted(0)` and nothing more; with fromCommand its controlled members are released first
  (`ReleaseContainerContents`, 0x70D5B4); then `ReleaseScriptThingIntoTheGame` (0x70D5C6). Not controlled: a
  container's members give back their references (`DecreaseContainerContentsScriptReference` 0x6EFD60, from a
  flock's head, 0x6EFDAC).
- `ReleaseContainerContents` 0x6EFCD0: a flock's members from the head (the next one read first); each controlled one
  with a slot gets `ReleaseScriptThingIntoTheGame(member, id, 1)`; a controlled member without a slot ends the whole
  loop (0x6EFD2E..0x6EFD30: `je 0x6EFD54`); another container logs "CANNOT RELEASE THIS! USE DISPAND!".
- `ReleaseScriptThingIntoTheGame` 0x70F600(thing, id, fromCommand): `SetControlledByScript(0)` (0x70F60C). At a
  task's end (fromCommand 0) a thing deleted when released (`IsDeletedWhenReleasedFromScript` 0x4021C0: only a
  ScriptTimer, 0x561300 = 1) or a highlight the script created goes with `ToBeDeleted(0)`, logging "Deleting thing
  not created by script" when +0xC is clear (0x70F647..0x70F670). Then `RemoveThingMusic` 0x429340 and, by
  `GetScriptObjectType` (the byte table 0x70F76C, the jump table 0x70F754):

  | Type | Case |
  |---|---|
  | 4 / 5 villager (0x70F6DE..0x70F736) | not in the map (vt +0x178 at 0x70F6E2, the +0x24 & 1 flag) and not in the physics → "Releasing object not in map"; in the physics (+0x24 & 0x40) `LivingAction::SetState(2, 163)` 0x6F82C0 (the previous state, +0x8E), else `SetScriptState(163 DECIDE_WHAT_TO_DO)`; then `Villager::ReleaseFromScript` 0x7531D0 |
  | 6 / 21 animal (0x70F6B1..0x70F6DB) | `SetScriptState(32 WANDER)` the same way, then `fn_0041AA00` |
  | 12 creature (0x70F739) | `fn_004F7060` |
  | 28 ball (0x70F6A4) | +0x2FC = 1 |
  | 24 / 31 / 34 | "Unknown thing-Tell Jonty" |
  | every other type | nothing (37 → 0x70F750) |

#### RELEASE_FROM_SCRIPT and land changes

- `RELEASE_FROM_SCRIPT` 159 (`GScript::ReleaseActorFromScript` 0x6FB380; older notes called it
  `GScript::ReleaseFromScript`): no thing → "Thing not valid" (0xC0CD88); a controlled thing →
  `ReleaseControlFromScript(thing, id, 1)` (0x6FB3B9), so it is never deleted (the 1 skips 0x70F61A..0x70F670). The
  slot and its references stay.
- What a land change does to the slots: [A variable holding a thing of the land before](land-change-and-saving.md#changing-land-load_map).

### Magic, weather and worship natives

The natives of fire, influence, spells, weather and worship. Each one's effect is on the page of its system.

#### Fire and temperature

- **IS_FIRE_NEAR** (`GScript::IsFireNear` 0x6F7910): `MapCoords::FindNearForScript`(the predicate 0x6F7100, 0, 0, r)
  != 0 (0x604370: the cells of the square ±r, the fixed objects then the mobile ones). The predicate: `IsOnFire(0)`
  (vt 0x298) and `GetDistanceInMetres` 0x74CD70 from the script's point to the object (a worship site:
  `GetTotemPos` 0x77CF30) ≤ r (`fcomp; test ah, 0x41` at 0x6F7171). A fireball is not in the map
  (`MagicFireBall::InsertMapObject` 0x682D10 is a bare `ret`), so it never counts.
- **SET_TEMPERATURE** 174 (`GScript::SetTemperature` 0x6FB840): `Object::SetTemperature` 0x639A60 (no source).

#### Influence and script players

- **INFLUENCE_OBJECT** 060 (`GScript::InfluenceObject` 0x6F9AA0): pops anti, player (the game's index, not a script
  player), radius, object; pushes the ring (`AddScriptGameThing`) or 0.
- **INFLUENCE_POSITION** 061 (`GScript::InfluencePosition` 0x6F9B60): anti, player (the game's index), radius,
  position → `InfluenceRing::Create` 0x5CD9D0.
- **GET_INFLUENCE** 062 (`GScript::GetInfluence` 0x6F9C60): position, raw, player (a float, through
  `GScript::ConvertScriptPlayerToGamePlayer` 0x6EB9A0) → `CalculatePlayerInfluence`(pos, player, 0, 0, allies = raw
  == 0).
- **Script players**: `GScript::ConvertScriptPlayerToGamePlayer` 0x6EB9A0 + `GGame::GetPlayer` 0x5509B0: script
  player 0 is the player at g_game +0x205A5B (the neutral one), n is game player n − 1; `GetPlayer` takes the game's
  index 0..7 and returns null from 8 on.

#### Spells

**SPELL_AT_THING** (`GScript::SpellAtThing` 0x70BFA0) pops the curl, the duration, the radius, the from point, the
thing and the magic. An Object → `fn_0070BB20` (its vt 0x2C check off, then the GMagicInfo cast on an object,
`fn_005FB520`); anything else `CastSpellAtPos` at its position. It pushes the spell. A script cast's process info
(0x70BE3C..): +0x0C the from point, +0x18 target − from, +0x24 the direction, power 1, +0x34 1.0 × curl, enabled.

#### Weather

`CREATE` of a weather thing (0x6F151E) makes it from GWeatherInfo[subtype]. A land script's weather storm is
`fn_0083F4A0`(LHPoint(x, land + 0, z), 0, 0) ([Weather](../world/weather.md)).

| Native | Function | Pops | Calls |
|---|---|---|---|
| CHANGE_WEATHER_PROPERTIES 123 | `GScript::ChangeWeatherProperties` 0x6FA490 | storm, temperature, rainfall, snowfall, overcast, fall speed | `fn_00774400` |
| CHANGE_LIGHTNING_PROPERTIES 124 | 0x6FA6B0 | storm, sheet min / max, fork min / max | `fn_00774520` |
| CHANGE_TIME_FADE_PROPERTIES 125 | 0x6FA570 | storm, duration, fade time | `fn_00774460` |
| CHANGE_CLOUD_PROPERTIES 126 | 0x6FA600 | storm, number of clouds, blackness, elevation | `fn_00774500`(blackness, ftol(numClouds), elevation) |
| PAUSE_UNPAUSE_CLIMATE_SYSTEM 401 | 0x6FF4E0 | the argument | 0xC24759 = argument ≠ 0 |
| PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM 402 | 0x6FF500 | the argument | 0xC24758 |
| KILL_STORMS_IN_AREA 404 | 0x6FF520 | position, radius | `fn_0083F750` |

#### Worship

See [Worship](../magic/worship.md).

- A site's mana is set through `fn_0077B060` and the site's +0xF0; the temple's spell icon is `fn_00463AD0`.
- A town's worship deaths are `Town::GetDeathsFromWorshipping` 0x740D60 → `GetDeaths` 0x740D70: the TownStats
  +0x7C[4 CHANT], written by `Villager::VillagerDead` (none before the town's first death).
- SET_ACTIVE 0x6FD720 and SET_TIMER_TIME 0x711280 on a spell dispenser go to `fn_00723030`.
- The land script's CREATE_SPELL_DISPENSER (90: town, pos, abode, magic, y angle, scale, period):
  `SpellDispenser::Create`, +0xD4 = the magic, `fn_00723030(1)` (an orb at once), +0xC8 = ftol(period) (turns; 0 →
  `fn_00723030(0)`, deactivated).
- CREATE_WORSHIP_SITE (0x7160C8): the player's citadel with its heart → `CitadelHeart::CreateBuiltWorshipSite`
  0x465110 for the tribe (the script's position and site info are not used by it).

### The other natives

Read in the exe, with each native's pops in the order it makes them. `GetScriptGameThing` 0x70D220 is the lookup of
every native that takes a thing; where it finds none the native prints its own message
([the slot table](#the-slot-table)) and does nothing. SET_SCRIPT_ULONG is
[above](#set_script_state-017-set_script_ulong-020-played-064-and-the-script-states); ADD_REFERENCE and
REMOVE_REFERENCE are the slot table's [references](#references).

#### Creating things

- **`GScript::CreateObject`**: sub-type 5000 is allowed only for the types that ignore it (0x6F11BD). By type:
  - a scaffold is `Scaffold::Create`(pos, `GScaffoldInfo` 0xD959D0, 0, 0, angle, scale) (0x6F13BF..0x6F13D5);
  - a timer is `fn_007115A0`((float)sub-type), the sub-type being its seconds (0x6F1253..0x6F1263);
  - a vortex is `LandscapeVortex::Create`(&pos, sub-type, 50.0) 0x5FE6E0 (0x6F171B..0x6F1730);
  - a whale (the shark) is `Whale::Create` 0x774C50(pos, &GMobileObjectInfo[24], 0, angle, scale) (0x6F1747);
  - an abode is "Invalid create type" (0x6F191D).
- **A timer thing**: `GScript::CreateTimer` 0x6F1D20 pops the seconds, makes the timer the same way, adds it with
  `AddScriptGameThing(t, 1)` and pushes it (0 and "Thing not created" when nothing was made).
- **`GScript::CreateFlock`** 0x6F21F0 pops the position (z, y, x) and adds the new flock with
  `AddScriptGameThing(f, 1)`.
- Both CreateTimer and CreateFlock also give the thing the script's debug name (`SetScriptNameOfCreate` 0x56FA70).

#### Finding things

- **CALL and CALL_NEAR** (`GScript::Call` 0x6F0D60, and `GScript::CallNear` 0x6F0EB0, which pops
  excludingScripted, the radius, the point (z, y, x), the sub-type, then the type, 0x6F0EC5..0x6F0F41):
  - a type outside 1..41 is "Invalid type=%d" (0xC0CB48) and 0;
  - the point goes to MapCoords (0x603160) and to the type's find function (table 0xC0C728, 24 bytes a type: +0 for
    CALL, +4 for CALL_NEAR); NONE, MARKER, DANCE, FLOCK, INFLUENCE_RING and WEATHER_THING have none ("No find function
    for type=%d", 0xC0CB18, and 0);
  - found: `AddScriptGameThing(thing, 0)` 0x70D0F0 and `PUSH(id, 4)` (0x6F0FFD); else "Thing not found" (0xC0CB38)
    and 0.
- **The find functions**:
  - `FindAtPos` 0x6F7220 / `FindNearPos` 0x6F7280 store the point and 1.0 or r in GScript +0x14 / +0x20 and call
    `MapCoords::FindNearForScript`(filter, type, sub-type, 1.0 / r) 0x604370: the nearest accepted thing of the
    square's cells, not cut at r. The filter is 0x6F6FA0 (type and sub-type) for CALL and 0x6F7070 for CALL_NEAR
    (also `GetDistanceInMetres`(point, the thing or its totem) ≤ r), behind 0x6F79F0 / 0x6F7A20, which reject a thing
    in a script (IsInScript vt +0x448) when excludingScripted is set.
  - A town: `FindTownAtPos` 0x6F7340 is `FindTownNearPos` 0x6F7370 with 10.0, which is
    `MapCoords::GetNearestTown(r)` 0x6020E0 (type, sub-type, filter and excludingScripted unused).
  - A creature: `GScript::FindCreatureAtPos` 0x6F7380 / `FindCreatureNearPos` 0x6F73C0 take the first creature of
    `Creature::CreatureList` 0xC5FCF8 within 1.0 m ([0x8AA390], no type test) or within r and with info +0x1F4 equal
    to the sub-type.
- **CALL_IN** (`GScript::CallIn` 0x6F22F0) pops excluding, the container, the sub-type and the type; found:
  `AddScriptGameThing(found, 0)` (0x6F2383); else a warning and 0 (0x6F23AE).
- **GET_NEAREST_TOWN_OF_PLAYER** (`GScript::GetNearestTownOfPlayer` 0x6F2A20) pops the radius, the player (a float,
  `__ftol` 0x6F2A96) and the position's z, y and x, converts the player (0x6EB9A0, `GetPlayer` 0x5509B0) and takes
  `GScript::FindPlayerTownAtPos` 0x6F72E0 (only that player's towns, the nearest within r); none is "Did not find
  town" (0x6F2AEA) and 0, else `AddScriptGameThing(town, 0)` (0x6F2B0D).

#### Containers

- `GScript::FlockAttach` 0x6EF3C0 pops the leader flag, the target, then the thing.
- `GScript::FlockDetach` 0x6EF9E0 pops the container, then the thing.
- `GScript::DisbandId` 0x6EFDC0 disbands.
- `GScript::IdSize` 0x6EFF50 pushes the members as a float, 0 with "Cannot Find Flock/Dance/Town Size" (0x6F0073).
- CHANGE_INNER_OUTER_PROPERTIES (0x70F480) pops calm, outer, inner and the thing.

#### Properties and flags

- **GET_PROPERTY** (0x70DD4C..):
  - the scale is `GetScale` (vt +0x120, Object +0x50);
  - the speed is read at 0x70DE4E..0x70DF5F (the physics, the dead Living);
  - the age (0x70DFFD) is a Living's `GetAge` (vt +0x8D0) as an unsigned qword turned to a float, else "Not used on
    non living objects" (0xC209A8) and 0;
  - the altitude (0x70E70A) is any thing's +0x1C.
- **SET_PROPERTY**:
  - the scale is `SetScale` (vt +0x124, `Object::SetScale` 0x639200: +0x50 and the matrix when it changes,
    0x70EA76);
  - the age (0x70EB65) is a Living's `SetAge(ftol(val))` (vt +0x8D4: `Villager::SetAge` 0x7528C0 changes the meshes
    when the grown-up age is crossed and sets the scale for the age, `Animal::SetAge` 0x4179C0);
  - the built share of a MultiMapFixed goes to `fn_0052EDD0` (0x70EC69);
  - the altitude (0x70EF9B) sets a highlight's draw height (`SetDrawHeight` 0x709C40), then any thing's +0x1C and its
    3D object;
  - anything else is "Cannot Set Property %d" (0x70F2CC..0x70F2D4 → 0x70F294) and nothing changes.
- **OVERRIDE_STATE_ANIMATION** (`GScript::OverrideStateAnimation` 0x6F9F50) pops the clip, then the thing
  (0x6F9F78). A clip ≤ 0 or ≥ 441 logs "Invalid animation forced" (0xC0D69C) and goes on (0x6F9F80..0x6F9F93); no
  thing is silent (0x6F9F9D); not a Living is refused (0x6F9FC0). The clip's AnimPack entry 0 (0x6F9FD3..0x6F9FE7)
  replaces the 3D object's animation when it differs (vt +0x184 / +0x180, cycle time 0 by vt +0x188,
  0x6F9FEA..0x6FA00D); it also writes the villager's `DataForScriptRemind` (+0x3C = clip, +0x44 = 0, from 0x6FA013),
  read only by `EnterPlayAnim`'s resume (0x768914). The clip lasts until the next `Living::SetStateAnim` 0x5ECB10.
- **SET_ID_MOVEABLE** (`GScript::SetIdMoveable` 0x6FB3E0): the thing, then the bool; no thing is "Thing not valid"
  (0x6FB411); Flags +0x24 bit 0x1000 = (moveable == 0) (0x6FB421..0x6FB43C).
- **SET_INDESTRUCTABLE** (`GScript::SetIndestructable` 0x6FDE20): the thing, then the flag; a script container hands
  it to its type's callback (table 0xC0C73C), anything else sets or clears Flags +0x24 bit 0x4000 (0x6FDEB3).

#### Timers

- **The countdown timer**: `GScript::StartCountDownTimer` 0x711150 → `InitialiseCountDownTimer` 0x6EB8B0;
  `GScript::RemoveCountDownTimer` 0x711180 clears GScript +8, which `GScript::CountDownTimerExists` 0x7111D0 reads;
  `GScript::GetCountDownTimer` 0x7111A0 gives the whole seconds left (`GetCountDownTimerRemainingTime` 0x6EB950);
  `GScript::HideCountDownTimer` 0x7111F0 / `GScript::RevealCountDownTimer` 0x711210 set GScript +0x10 to 0 / 1.
- **Timer things**: `GScript::SetTimerTime` 0x711280 pops the time, then the thing: a script timer starts again from
  this turn (0x711630), a spell dispenser takes it as its period (0x71132A..0x711362), anything else is "Invalid
  script thing". `GScript::GetTimerTimeRemaining` 0x711370 gives a timer's seconds left (`fn_00711670`, 0 once out),
  0.0 with the error otherwise; `GScript::GetTimerTimeSinceSet` 0x711410 the seconds since it was set
  (`fn_007116D0`), FLT_MAX ([0x95722C]) otherwise.

#### The day's clock and the game speed

- `GScript::SetGameTime` 0x710E20 forces the visual time from the script time; `GScript::GetGameTime` 0x710E60 maps
  the visual time back.
- `GScript::GameTimeOnOff` 0x710E90 sets the visual time scale to 1 or 0.
- `GScript::MoveGameTime` 0x710EC0 slides the visual time to the hour in `duration` seconds of game time.
- `GScript::SetGameTimeProperties` 0x7114B0 sets the cycle (duration, night share, change share) and
  `GScript::ResetGameTimeProperties` 0x711520 puts back the defaults.
- `GScript::SetGameSpeed` 0x6FAAE0 sets the game speed.
- **INSIDE_TEMPLE** (`GScript::InsideTemple` 0x6FF5A0) pushes g_game +0x205A28 == 1 as a bool.

#### The camera

- **Taking control.** `GScript::StartCameraControl` 0x6ECCA0 makes no camera mode inside the citadel (g_game
  +0x205A28 == 1); outside it creates the script mode (fn_00461140, 0x6ECCBA) unless
  `GCamera::CantExitCurrentMode` 0x441B70.
- **The mode check.** The camera opcodes (0x6ECAA0 and the others) check the mode first: none is "Script camera has
  been removed!" (0xC0C0CC), a mode that is not `CameraModeScript` "We are in the wrong camera mode!" (0xC0C0EC;
  SET_CAMERA_POSITION 0x6EC8F0 says nothing), and the opcode does nothing.
- **Two things in view.** `GScript::StartDualCamera` 0x6ED2E0 pops b then a; either missing is "Thing invalid for
  dual cam" (0xC0C1EC), else a `CameraModeTwoObjects`(camera, a, b) 0x461BB0 goes on top of whatever mode is current.
  `GScript::UpdateDualCamera` 0x6ED370 calls `SetObjects(a, b)` 0x461C90 when the current mode is a two-things one
  (0x6ED3F0).
- **Properties.** `GScript::CameraProperties` 0x6EDFF0 pops behind (raw), the angle in degrees (× 0.0174533), the
  speed and the distance, and with the script mode sets +0x14 distance, +0x18 speed (0 places at once), +0x1C
  behind, +0x0C angle and +0x20 = 0 (0x6EE0C1..0x6EE0D9).
- **Fixed rotation.** `GScript::SetFixedCamRotation` 0x6EE1A0 pops the point and "on"; only with the player's
  `CameraModeNew3` ("Wrong camera mode", 0xC0C29C, otherwise) it calls `ForceRotateAboutPoint` (vt +0x5C, 0x457330).
- **The lens.** `GScript::SetCameraLens` 0x6EE2E0 calls `SetCameraFov`(70°, x) (0x6EE302..0x6EE325): its argument is
  the time and the lens goes back to the default (its one use is SET_CAMERA_LENS(0)).
  `GScript::SetCameraPosFocLens` 0x6EE3C0 calls `SetPositionAndFocus` 0x4438C0 and `SetCameraFov`(lens, 0) with the lens not turned to radians
  (0x6EE480); MOVE_CAMERA_POS_FOC_LENS 0x6EE4B0 moves with `CameraModeScript::MoveCameraPosition` /
  `MoveCameraFocus` in t and the lens likewise not in radians (0x6EE613); no map uses either.
- **Facing.** SET_CAMERA_TO_FACE_OBJECT with no thing (`fn_006ED710`) says "no object to face" (0xC0C218) and then
  reads through the null pointer (🔍 inferred). `GScript::GetFacingCameraPosition` 0x6EE710 pushes the camera position
  + d × its forward vector.
- **Inclusion distance.** `GScript::GetInclusionDistance` 0x6ED990 pushes [0xC5E13C], written by
  `CameraModeNew3::Update` (0x45FBFD / 0x45FCD9 / 0x45FCE1).
- **Following a computer player.** SET_FOCUS_FOLLOW_COMPUTER_PLAYER (0x6EDC00) pops the player (a float), converts it,
  checks the mode and calls `fn_004619F0` (0x4619F0): the path dropped, `SetCameraFocus(0)` 0x4619B0 and +0x50 = the
  player, which `CameraModeFollow::Update` follows by `GComputerPlayer::GetHandPos` 0x657FE0;
  SET_POSITION_FOLLOW_COMPUTER_PLAYER (0x6EDCD0) does the same with `fn_00461A10` (0x461A10): `Set(0)` 0x44BA00 and
  +0x54.
- **Field of view.** `GScript::IsGameThingFieldOfView` 0x6F8130 (in a multiplayer game it pushes 1 without popping)
  tests the drawn camera's screen against an Object's bounding sphere or a thing's point;
  `GScript::IsPosFieldOfView` 0x6F8060 pushes 0 inside the temple, else the point's screen test (fn_0081F1D0).

#### The help spirits

Each native pops the spirit last; its type becomes a help spirit through `ConvertScriptSpiritToHelpSpirit` 0x710350,
by the local player's alignment. A help script is one whose task type is 2
([help script test](#what-the-game-asks-of-the-virtual-machine)).

- **Coming and going.** `GScript::SpiritEject` 0x710410 → `HelpSystem::SpiritEject` 0x5C6570(t, help script) →
  fn_005C4BD0, where a help script's spirit appears (0x5C3400) and any other is ejected (0x5C32C0);
  `GScript::SpiritHome` 0x710490 → `HelpSystem::SpiritHome` 0x5C6670 → fn_005C5200 (a help script's spirit
  vanishes, 0x5C3540; any other flies home, 0x5C3590); `GScript::SpiritAppear` 0x710460 and
  `GScript::SpiritDisappear` 0x7104E0 are the same with 1 (Appear, Vanish).
- **Pointing and looking.**
  - `GScript::SpiritPointAtPos` 0x710510 pops in_world (the raw dword), the position and the spirit →
    `HelpSystem::SpiritPoint` 0x5C6590 → fn_005C4EF0 (eject, then point mode 1 with 8.0 / 5.0);
  - `GScript::SpiritPointAtGameThing` 0x7105B0 the same for a thing (no thing: "Object no longer valid", 0x7105FD) →
    0x5C6650 → fn_005C4FA0, re-sent every turn by 0x5C50C0;
  - `GScript::LookGameThing` 0x710630 → fn_005C65F0 → fn_005C4E50 (the per-turn refresh 0x5C5170 points, a bug of
    the original);
  - `GScript::LookAtPosition` 0x7108C0 → fn_005C65D0 → fn_005C4E00;
  - `GScript::StopPointing` 0x710860 → fn_005C6630 → fn_005C5060; `GScript::StopLooking` 0x710890 → fn_005C6610 →
    fn_005C5090.
- **Animations.** `GScript::SpiritPlayAnim` 0x710940 pops the time, the anim (raw), y, x and the spirit; an anim
  outside 0..0x50 is "Invalid enum" (0xC20B70), then the screen checks, none stopping it →
  `HelpSystem::SpiritPlayAnim` 0x5C6690 → fn_005C4C80. `GScript::SpiritPlayed` 0x710A50 pushes
  `!HelpSystem::IsSpiritPlayingAnim` 0x5C66C0 as a bool.
- **On the screen.** `GScript::ClingSpirit` 0x710AA0 → `HelpSystem::SpiritCling` 0x5C66E0 → fn_005C4D40 and
  `GScript::FlySpirit` 0x710B70 → `HelpSystem::SpiritFly` 0x5C6700 → fn_005C4DA0 pop y, x and the spirit;
  `GScript::SpiritScreenPoint` 0x710CA0 likewise, the pixel (ftol(W x), ftol(H y)) with W / H = [0xE85058] /
  [0xE8505A] (0x710D45..0x710D7B) → fn_005C65B0 → fn_005C4F50.
- **The screen checks** (0x7109CA..0x710A28, 0x710AE8..0x710B4B) log "Invalid Y" then "Invalid X" (0xC20B64 /
  0xC20B58) for a value below 0 (NaN too) or above 1 and go on.
- **Speaking.** `GScript::SpiritSpeaks` 0x710C40 pops the text (0 past 6974, 0x710C6E), then the spirit, and pushes
  `HelpSystem::GetSpiritWhoTalks` 0x5C6E20 == the spirit.

#### The interface

- **Clicks.** `GScript::ClearClickedObject` 0x70B0E0 clears GInterface +0x45C +4 / +8;
  `GScript::ClearClickedPosition` 0x70B100 clears +0x46C..+0x474 (the turn +0x478 stays);
  `GScript::PositionClicked` 0x70B120 pops the radius, then the position, and asks `fn_005D0460`(MapCoords(pos),
  radius) whether the last clicked land point is within it (0x70B1C7..0x70B1DA; a multiplayer game logs "This is not
  multiplayer friendly yet!" and pushes true, 0x70B183..0x70B1B2).
- **What the player may do.** `GScript::SetInterfaceInteraction` 0x70B200 hands the level to 0x70B220.
  `GScript::SetInterfaceCitadel` 0x70B9A0 stores the raw value at (g_game +0x250090) +0xA0, read only by
  `CitadelEntrance::InterfaceValidToTap` 0x468F50.
- **The hand.** GET_HAND_STATE (0x6FF730) pushes GInterface +0x3AC, the hand state of the last turn (fn_005D7E40).
  `GScript::IsPlayingHandDemo` 0x6FDB80 pushes `!IsPlayBack(0)` (0x6FDB9A..0x6FDB9E).
- **Help and dialogue.** `GScript::SetHelpSystem` 0x6FC020 stores the value as it is at HelpSystem +0x45F8
  (0x6FC03D); `GScript::GameCloseDialogue` 0x6FF700 closes the help text (fn_005CB010) and
  `HelpSystem::ClearAllText`. FADE_FINISHED (0x6FCE40) is true when no fade is in progress.

#### The help events

- `GScript::GetEventsPerSecond` 0x70B7F0 pops the HELP_EVENT_TYPE (1..48, else "Invalid event" and 0.0) and pushes
  `HelpProfile::GetTriggerPerSecond` 0x448FC0 of g_game +0x250060's accumulator.
- `GScript::GetTimeSince` 0x70B880 is the same with `CameraHelpAccumulator::GetTimeSinceLastUsed` 0x448F40.
- `GScript::GetTotalEvents` 0x70B910 pushes the accumulator's count (+8 + type × 0x10C): the turns in which the
  event happened (TeachRotate 25, TeachPitch 28, TeachZoom 29, DoubleClicking 30 / 31).

#### Highlights and effects

- **CREATE_HIGHLIGHT.** `GScript::CreateHighlight` 0x6F1C20 pops the challenge id (0x6F1C30), the point (z, y, x,
  0x6F1C44..0x6F1C70) and the info row (0x6F1C88: `GScriptHighlightInfo` 0xD96390 + row × 0x110), makes it with
  `ScriptHighlight::Create`(MapCoords(point), info, challenge, 0.0, 1.0) 0x709A40, adds it
  (`AddScriptGameThing(thing, 1)`, 0x6F1CC6), names it (`SetScriptNameOfCreate`, 0x6F1CEA) and pushes it (0x6F1CD7);
  none is "Highlight not created" (0xC0CC18, 0x6F1CF5) and 0.
- `GScript::HighlightProperties` 0x6FE1F0 pops the DYK_CATEGORY, the text and the thing ("Thing not valid",
  0xC0CD88) and calls `ScriptHighlight::SetScriptId`(text, category) 0x709A20 (+0x78, +0x84).
- `GScript::SetDrawHighlight` 0x708CB0 stores the value as it is at GScript +0x80 (0x708CCD).
- **Effects.** `GScript::SpecialEffectPosition` 0x70C330 →
  `GParticleContainer::CreateSpotVisualWithSpecifiedDuration` 0x63E580; `GScript::SpecialEffectObject` 0x70C460
  makes the same at the thing's place when made (obj +0x14, type, 1.0, duration, obj; 0x70C50B): it does not follow
  the thing, and the thing's loss closes the effect.

#### Sound and music

- `GScript::SetCreatureSound` 0x710020 stores the value at GScript +0x84 (0x71003D).
- `GScript::SetGameSound` 0x7100B0 with false stops every sample (`LHSampleStopAll`, fn_004287D0) and sets GScript
  +0x90 = 1, so only the dialogue banks play (`GAudio::PlaySoundEffect` 0x429F7C); with true it sets it to 0.
- ENABLE_DISABLE_ALIGNMENT_MUSIC (0x710120) stores the value at GScript +0x94 (0x71013D).
- MOVE_MUSIC (0x70FCA0) pops "to" first, then "from", and needs both (0x70FCEE..0x70FCF4).
- SET_MUSIC_PLAY_POSITION (0x70FE60) pops z, y and x (0x70FEDB), then the thing.
- MUSIC_PLAYED (0x70FF40) is true for an invalid thing (0x70FF71), else whether the music has finished.

#### The intro film and the JC specials

- **SET_AVI_SEQUENCE** (`GScript::SetAviSequence` 0x6FC050) pops the sequence (compared with 1 and 2), then "on", and
  calls `PSysGlobal::StartAVISequence` 0x68F450 or `StopAVISequence` 0x68F4F0; it returns at once.
  - Stopping 2 jumps to `GGame::EndFallingSpellVideo` 0x553A10 (0x68F4FD); stopping anything else does nothing.
  - Starting 1 (0x68F477..0x68F4E9) plays `data\intro.bik` full screen (`PlayFullScreenMovie` 0x54D920: the tip
    video cleared, the pause kept and the game paused, the wide screen on; the player object is made even when the
    file does not open, 0x54AC05). It sets g+0x25018C = 58 × fps and g+0x250190 = 60 × fps (the pause is given back
    after 58 s and the picture fades out up to 60 s, 0x54DB27..0x54DB7F) and g+0x250530 = 1, and clears the black fade
    at once (`GScript::SetupScreenFadeBackToNormal(0)` 0x6EBB00, 0x68F4D6..0x68F4E4) ([Bink videos](../video/bink.md)).
- **PLAY_JC_SPECIAL**: 14 sets [0x9CD384] = 1, the bookmarks on (0x708F5A); 15 sets it to 0 (0x708F66; only
  `FollowUs` uses it); 7..13 and above 15 do nothing (0x708F70; `FollowUs` also plays 18).

#### Town, player and random numbers

- `GScript::SetTownDesireBoost` 0x6FE650 pops the boost, the desire and the thing; for a town, a desire below 17 and a
  boost in [−1, 1] it sets +0xD4[desire] and re-sorts order 1 (0x6FE73E).
- `GScript::GetAlignment` 0x6F9A60 pushes the player's alignment (`GPlayer::GetAlignmentValue` 0x64D6A0).
- `GScript::RandomULONG` 0x6F8E20 pops max (0x6F8E2E), then min (0x6F8E40), and pushes `GameRand(max − min + 1)` +
  min on the synchronised stream as an int (0x6F8E4C..0x6F8E68; unsigned, 0 for a range of 0).

### Virtual machine CAST

In the original `ScriptLibraryR.dll` (`Plug Ins`), opcode 23 INTCAST (0x10008EE0, the table 0x100090E4 by type − 1)
**converts**:

- CASTI reads the bits as a float and does `__ftol` (0x1001568C, truncating; the low word of the 64-bit integer);
- CASTF reads the bits as an unsigned 32-bit integer (`fild qword` with the high part 0);
- CASTV, CASTO and CASTB only change the type, and type 5 does nothing.

### Virtual machine SYS (a call to a native)

In the original each native pops its own arguments (`ScriptDLL::POP` 0x6F6BC0, one call per argument, in the order
the native reads them) and pushes its result (`PUSH` 0x6F6BA0); DANCE_CREATE (`GScript::CreateDance` 0x6F0170) and
FLOCK_ATTACH (`GScript::AttachToId` 0x6EF3C0) are read examples ([Dances](../beings/dances.md)).

### What the game asks of the virtual machine

- The VM as GScript asks it: `ScriptDLL::TaskNumber` 0x6F69F0, `GetCurrentTaskScriptType` 0x6F6A90,
  `ScriptDLL::GetScriptType` 0x6F6C50 (`GetScriptType`), `StopTasksOfType` 0x6F68F0, `PUSH` 0x6F6BA0, `POP`
  0x6F6BC0, `StartScript` 0x6F6880.
- A help script is `GetCurrentTaskScriptType() == 2` (0x710433..0x71044B: `sub 2, neg, sbb, inc`).
- `DLL_GETTIME`: the game's opcode table has a NULL handler for it (`{NULL, 0, 1}`, 0x70041E..0x70042A);
  `ScriptLibraryR.dll`'s Initialise (0x10002308) installs its own 0x1000ABD0, `LHVM::PushElaspedTime`: the VM tick
  count (one a game turn outside the citadel) × 0.1f.
- `GScript::ScriptErrorMessage` 0x6F62B0 and `ScriptWarningMessage` 0x6F62C0 are a bare `ret`: the original prints
  none of its script messages.

### In game (Land 1)

What `FollowUs` does once these commands run (with the texts that wait for a click clicked, as a player would). The
whole intro is on [The Land 1 intro](land1-intro.md).

1. **To the beach.** It passes `_loop_4` at about 15 s (the family reaches the kiss markers), walks to the beach, the
   son runs into the sea, the camera jumps with the script's `SET_CAMERA_POSITION` calls, the advisors' texts appear
   (4431..4437 and 5189..5192) and the family passes `_loop_77..79` (it reaches `StartPath`).
2. **The film and the rescue.** With the script camera (035 `HAS_CAMERA_ARRIVED`, 003 / 004,
   [The game camera](../camera/game-camera.md)) and `SET_AVI_SEQUENCE` (203, `INTRO.bik`,
   [Bink videos](../video/bink.md)), `FollowUs` runs through the film (about frame 20000), the rescue and `RUN Drag`
   (52154).
3. **The hand demo.** In `Drag` (12627..12722), after `PLAY_HAND_DEMO(279, 1, 0)` (12663, 266) the script waits
   `HAND_DEMO_TRIGGER` (336) three times (12672 / 12682 / 12692) and then `IS_PLAYING_HAND_DEMO` (267, 12699; it
   pushes `!IsPlayBack(0)` 0x6FDB9A, so it waits for the demo to end). `drag.hnd` plays its 590 records and text 5219
   follows.
4. **The hand-over.** 52172..52175 runs (`SET_WIDESCREEN 0`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`,
   `END_DIALOGUE`): the bars go, the camera and the dialogue are released, about 4 minutes of game after the start.

### Sources

- runblack.exe (v1.42, with the 1.20 symbols): `GScript::MoveGameThing` 0x6F8E80, `SetScriptState` 0x6F8370,
  `SetScriptStateULData` 0x6F8770, `HasPlayed` 0x6F9DC0, `GetPosition` 0x6F88A0, `GetDistance` 0x6F8CA0,
  `HasCameraArrived` 0x6ED170, `SetPosition` 0x6F8A00; the Living, Villager and MobileWallHug functions they call; a
  disassembly of `Plug Ins\ScriptLibraryR.dll` (INTCAST).

## openblack

Files: `src/CHLApi.cpp` (the natives), `components/ScriptLibrary/src/LHVM.cpp` (the VM),
`src/ECS/Villager/VillagerScript.*`, `ecs::script_held`, `ecs::script_set_position`. Port decisions:
[Port decisions: audio, video and scripts](../project/port-decisions-audio-video-scripts.md).

### Moving things

- **MOVE_GAME_THING**:
  - a creature goes to `CreatureMindSystem::MoveForScript`;
  - villagers move exactly (`ecs::villager::SetupMoveToPos`, with the PathfindingSystem's `MoveStateStepThroughTag`
    or `MoveStateArrivedTag` mark; MOVE_ON_STRUCTURE never happens, openblack has no `MoveOnStructure`);
  - animals (⚠️ approximate): `animal_ai::MoveTo(…, IN_SCRIPT)`, the destination's altitude above the land (+0x88)
    taken as 0 and, if already there, `animal_ai::SetState(IN_SCRIPT)` instead of SetScriptState;
  - a flock sets the first member's `AnimalBrain::goal` and `Flock::domainCentre`;
  - there are no weather things or computer players;
  - anything else has only its `Transform` moved (⚠️ approximate).
- **AreWeThere**: `walk_arrival::AreWeThere` on whole units, r in map units, each product rounded to a float as the
  game's FPU does; the positions come from openblack's metres through `map_coords::FromMetres`.
- **SET_POSITION** (`SetPosition` in `CHLApi.cpp`, the rule in `ecs::script_set_position::PlanFor`, tested by
  `test_script_set_position`):
  - the tests: "in a hand" is the hand's held object; "flying" is an entry in the physics (what the test
    GET_PROPERTY's Flying reads); a script marker is the `ScriptMarker` tag `MarkerArchetype` gives it; an Object is
    `script_type::IsObject` and not a marker; "not in the map" is `map_cells::IsObjectInMap` false for a kind
    openblack files in the map cells;
  - the point goes through `map_coords::FromWorld` / `ToWorld`, so x and z are truncated to the map's units and the
    ground is taken there;
  - a creature goes to `CreatureLocomotionSystemInterface::Place`; a thing that is not an object has its Transform
    position set (a flock its domain centre); any other object is moved by `map_cells::MoveMapObject`, its rotation
    made the turn about Y by its Y angle (`UprightRotation`, kept as it is when it already is one) and its scale the
    Transform's uniform scale, `NotifyTeleported` (the turn-start copy) and, for a villager or an animal a script
    controls, script state 4;
  - before this port openblack put every thing on the ground at the untruncated point, refused nothing, dropped a
    marker's height, snapped the turn start of every thing and left a creature walking.
- **GET_POSITION** gives the land height at the destination (⚠️ approximate: the wall hug stores only x and z, and
  GET_DISTANCE does not look at y).
- **GET_DISTANCE** measures in x and z with the cut below 0.5 (it used to measure in 3D without the cut, so `== 0`
  was never met).
- **IS_ACTIVE**: `script_highlight::IsActive`, the dispenser's `timer.active`; a reward is never active, as no
  openblack code taps one open yet.

### Script states and releasing

- **The script states**: `SetScriptState` uses `map_cells::IsObjectInMap`, which is out in the hand, in flight and in
  a storm, as RemoveMapObject clears the bit. `SET_SCRIPT_ULONG` writes the villager's `ScriptAnimation::playsLeft` /
  `clip`; the 200 state's clip is `ClipChoice::Script` of `VillagerAnimate.cpp`.
- `DataForScriptRemind` is not ported: the branches of EnterInScript / EnterPlayAnim that resume a walk are never
  taken (🔍 inferred).
- **⚠️ approximate**: openblack's PathfindingSystem moves every entity with a mark, whatever its state, so
  `SetScriptState` removes the marks when the villager is no longer in MOVE_TO_POS (without that the father kept
  walking, and overshot, while acting in 200).
- **Releasing**: the villager case uses `map_cells::IsObjectInMap` through `villager::IsObjectInMap`;
  `ecs::script_held::Process` is the release of the things no variable holds (`fn_0070D480`).

### Magic, weather and worship natives

All in `src/Magic/Script/`. The VM is reached through `Locator::vm::value`.

| File | Natives |
|---|---|
| `CHLFire` | `magic::script::IsFireNear` |
| `CHLInfluence` | `InfluenceObject`, `InfluencePosition`, `GetInfluence`, `GamePlayer` |
| `CHLSpells` | `SpellAtThing` and its `ScriptInfo` |
| `CHLWeather` | `CreateWeatherThing`, `ChangeWeatherProperties`, `ChangeLightningProperties`, `ChangeTimeFadeProperties`, `ChangeCloudProperties`, `PauseUnpauseClimateSystem`, `PauseUnpauseStormCreationInClimateSystem`, `KillStormsInArea` |
| `CHLWorship` | `SetTemperature`, `script::GameSetMana`, `script::GetSpellIconInTemple`, `script::GetTownWorshipDeaths` (the `TownDeaths` component), `SetDispenserActive` (true when the object was a dispenser; SET_ACTIVE and SET_TIMER_TIME on other objects stay in `CHLApi`) |
| `MapScriptMagic` | `CreateSpellDispenser`, `CreateWorshipSite` |
| `MapScriptWeather` | `magic::map_script::CreateWeatherStorm`, `CreateWeatherClimate`, `CreateWeatherClimateRain`, `CreateWeatherClimateTemp`, `CreateWeatherClimateWind` |
| `ScriptPlayer.h` | `ScriptPlayerToGamePlayer` |

### The virtual machine

- **CAST**: `Opcode23Cast` in `LHVM.cpp` converts as the original does (it used to change only the type, so
  `PUSHF 1.0 CASTI` reached SET_SCRIPT_ULONG as 0x3F800000 times).
- **SYS**: `LHVM::Opcode05Sys` counts the pops and pushes of the call.
  - A native that pops **none** of its `stackIn` arguments (a stub that only logs "not implemented") has those values
    taken out from under whatever it pushed, so the next native reads its own arguments.
  - A native that pops only some of them is left as it is: its leftover arguments shift every later read, so each
    native's missing pops are fixed in the native itself, as its own fidelity commit (DANCE_CREATE's six pops).
  - A function with no implementation at all pops `stackIn` values and pushes `stackOut` zeros.

### In game

The hand demo (`Input/HandDemo.h`) and the opcodes 266 / 267 / 336 are wired; the dialogue texts are drawn
(`Help/HelpTextDisplay.h`, `Renderer::DrawHelpText`; their look is not compared with the original's yet). Still
missing in the intro: the advisors, the welcome crowd and dance, the villagers' focus and override animations, the
intro's JC specials and the high-detail family models ([The Land 1 intro](land1-intro.md#pending)).

### Where it is in the code

Each native of `src/CHLApi.cpp` pops what the original's pops, in its order; what differs is said.

- **Helpers**: `ScriptVm` (the VM as GScript asks it); `IsHelpTask` (the task's script type is Help); `ScriptThing`
  and `SpiritThing` (`GetScriptGameThing` with the opcode's own message; ⚠️ approximate: 0 is null and a valid entity
  stands for a live thing, where the original looks the id up in its slot table); `ScriptCameraMode` (the camera
  opcodes' mode check: openblack always has the player's mode, so only "wrong camera mode" can happen; the citadel
  note of 003 / 004 / 287 is not ported, as the citadel never has a script mode); `CheckScreenFractions` (the
  spirits' "Invalid Y" / "Invalid X").
- **Creating**: `CreateScriptObject` (the sub-type 5000 rule for every type but `ObjectType::Timer`,
  `ObjectType::SpellDispenser` and the others that ignore it; `ObjectType::Scaffold`, `ObjectType::Timer`,
  `ObjectType::Vortex` and `ObjectType::Whale` made as above; `ObjectType::Abode` refused), `CreateTimer`,
  `FlockCreate` and `CreateHighlight` (also refused for a row past the four); none sets the script's debug name.
- **Finding**: `FindForScript` (CALL, CALL_NEAR: the original's messages kept as openblack's diagnostics, "Thing not
  found" at debug level, since the scripts' waiting loops ask every turn) and `FindCreatureForScript`
  (⚠️ approximate: the registry's order, as openblack has no creature list; the sub-type is taken as the creature's
  species), `CallNear`, `CallIn`, `GetNearestTownOfPlayer`.
- **Containers** (`ECS/ScriptContainers.h`): `FlockAttach`, `FlockDetach`, `FlockDisband`, `IdSize`,
  `ChangeInnerOuterProperties`. `ObjectDelete` disbands a container before the delete mode runs, not after it
  (⚠️ approximate: openblack destroys the entity at once, the original only marks it).
- **Properties**: `GetProperty` and `SetProperty` (the scale and the age only for a villager or an animal, the
  altitude only for a highlight: the rest is pending); `SetScriptUlong`; `OverrideStateAnimation` (without the
  `DataForScriptRemind` record); `SetIdMoveable`; `SetIndestructable` (no container callback); `AddReference` (the
  binding's one output is kept as the object) and `RemoveReference`.
- **Timers** (`ECS/ScriptTimer.h`, `script_countdown`): `StartCountdownTimer`, `RemoveCountdownTimer`,
  `GetCountdownTimer`, `CountdownTimerExists`, `HideCountdownTimer`, `RevealCountdownTimer`, `SetTimerTime`,
  `GetTimerTimeRemaining`, `GetTimerTimeSinceSet`.
- **The day's clock** (`DayNightClock`): `SetGameTime`, `GetGameTime`, `GameTimeOnOff`, `MoveGameTime`,
  `SetGameTimeProperties`, and `ResetGameTimeProperties` with `DayNightClock::k_DefaultDuration` and
  `DayNightClock::k_DefaultNight`; `SetGamespeed`; `InsideTemple`.
- **The camera** (`Camera/ScriptCamera.h`): `StartCameraControl`, `StartDualCamera`, `UpdateDualCamera`,
  `CameraProperties`, `SetFixedCamRotation` (not read by the world camera model yet), `SetCameraLens`,
  `SetCameraPosFocLens` and `MoveCameraPosFocLens` (the lens not in radians, as the original), `SetCameraToFaceObject`
  (no thing: nothing more is done), `GetFacingCameraPosition` (the forward vector taken from the drawn position to the
  drawn focus: 🔍 inferred), `GetInclusionDistance` (⚠️ approximate: the writer of the distance is not ported, so it
  stays FLT_MAX), `GameThingFieldOfView` and `PosFieldOfView` (`Camera/FieldOfView.h`, no multiplayer case);
  `SetFocusFollowComputerPlayer` and `SetPositionFollowComputerPlayer` pop their argument and do nothing, as
  openblack has no computer players.
- **The help spirits**: `SpiritEject`, `SpiritHome`, `SpiritAppear`, `SpiritDisappear`, `SpiritPointPos`,
  `SpiritPointGameThing`, `LookGameThing`, `LookAtPosition`, `StopPointing`, `StopLooking`, `PlaySpiritAnim`,
  `SpiritPlayed` (true when the help system is absent: nothing plays), `ClingSpirit`, `FlySpirit`,
  `SpiritScreenPoint`, `SpiritSpeaks` (the local player taken as PLAYER_ONE: 🔍 inferred).
- **The interface**: `ClearClickedObject`, `ClearClickedPosition`, `PositionClicked` (no multiplayer case),
  `SetInterfaceInteraction` (`Help/InterfaceInteraction.h`), `SetInterfaceCitadel`, `GetHandState` (the hand
  system's interface hand state), `IsPlayingHandDemo`, `SetHelpSystem`, `GameCloseDialogue`, `FadeFinished`,
  `GetEventsPerSecond`, `GetTimeSince` and `GetTotalEvents` (`Help/HelpProfile.h`).
- **Highlights, effects, town and player**: `HighlightProperties`, `SetDrawHighlight`, `SpecialEffectPosition`,
  `SpecialEffectObject`, `SetTownDesireBoost` (`ecs::town_desire::ScriptSetTownDesireBoost`), `GetAlignment`,
  `RandomUlong` (the synchronised stream).
- **Sound, music and film**: `SetCreatureSound`, `SetGameSound`, `EnableDisableAlignmentMusic`, `MoveMusic`,
  `SetMusicPlayPosition`, `MusicPlayed191`, `SetAviSequence` (the film played by `Video/VideoPlayer.h`; the fade is
  always cleared, as in the original), `PlayJcSpecial`, and `DllGettime` (the VM's own `PushElaspedTime`).

### Test hooks

- `OPENBLACK_SCRIPT_THING_TRACE=1`: one line per MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and per true
  PLAYED (to follow `FollowUs`).
- `OPENBLACK_TEST_TEXT_CLICK=1`: every turn, if a text is waiting for the click (`RUN_TEXT` with interaction 1), it
  does the left click (`HelpSystem::ProcessInterface(true)`, which ignores it until the text has shown for 1 s).

## Pending

### SET_POSITION

- Which things set the +0x24 bit 4 besides the player's hand (a computer player's hand, a creature's hold) is not
  read; openblack refuses only the object the player's hand holds.
- An Object whose kind openblack never puts in the map cells (the whale among them) counts as in the map, so it is
  not refused; whether the original's has its in-the-map bit is not read.
- The matrix takes `GetScale`; openblack takes the Transform's uniform scale, which for a citadel heart and a magic
  shield is their drawn scale, not their object scale.
- The things that are not objects, other than a flock (a town, a timer, a mist, an influence ring, a weather thing, a
  puzzle game, a spell), have their Transform position set; what each reads of its +0x14 is not checked.

### Other commands

- **MOVE_GAME_THING**: on a flock the original moves the tail member and openblack the first one; animals and
  non-living things are approximate ([Moving things](#moving-things)).
- **SET_SCRIPT_STATE on a script container** (g_game +0x250090 +0x24 and the loop function from the table 0xC0C73C)
  is not ported.
- **PLAYED on another Living** (`GetFinalState` == 4, 0x6F9EC4) is not ported.
- **`DataForScriptRemind`** (the walk a villager resumes after a script state) is not ported.
- **DELETE of a PuzzleGame**: openblack's DELETE does not call the puzzle's own delete (`fn_006D6CC0`) yet.
- The intro's missing parts are listed on [The Land 1 intro](land1-intro.md#pending).
