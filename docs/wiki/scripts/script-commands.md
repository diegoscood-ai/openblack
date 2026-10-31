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
| IsWeather (+0x3FC, at 0x6F9015) | `WeatherThing::SetMoveTo` 0x774550: only while the thing has its storm (+0x78 not null), the storm's target (+0x5C, the point it drifts to; its speed +0x68 follows) = the script's point as popped (x, y, z, no MapCoords). The thing's own position is not set |
| IsComputerPlayer (+0x4B8, at 0x6F9036) | `fn_00658510`(pos, 60.0) |
| anything else | "Jonty - Thing must be living to move it!" (0xC0D56C) and `SetPos(coords)` (+0xFC, 0x401940) at 0x6F9058 |

`GameThingWithPos::SetPos` 0x401940: Pos (+0x14) = coords, so the thing's world point is the vector itself. It is
the +0xFC slot of every class with a position but two (the sweep of every vtable's slot: `InfluenceRing::SetPos`
0x55EBF0 and `WeatherThing::SetPos` 0x7742E0, and the weather thing never gets here), so for every kind of thing
openblack has (an abode, a feature, a field, a tree, a rock, a pot, a mobile object...) nothing that hangs on the
position follows:

- no `RemoveMapObject` / `InsertMapObject`: the thing stays in the lists of the cells it was filed in (and keeps its
  collide data) while its Pos is the new one; `Object::InsertMapObject` 0x636740 and `RemoveMapObject` go by
  `Pos.ToMap()`, so the next code that takes it out works from the new position;
- its `Game3DObject` keeps its own position, which only the creation (`MultiMapFixed::CallVirtualFunctionsForCreation`
  0x52E890, `Object::Create3DObjectAtPos` 0x636B20) and the angle and scale setters (`Object::SetXYZAngles` 0x638D00)
  write: `MultiMapFixed::Draw` 0x518090 draws the 3D object (+0x40) at its own point (+0x38), so the model stays
  drawn where it was;
- no footprint, static map, town or influence update.

No shipped script reaches this branch: of the 528 MOVE_GAME_THING calls of the story, every one moves a villager, an
animal, a creature, a flock or a weather thing (the storms of Land 1's `TheStorm` and Land 2's `KillKhazarMain`).

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

<a id="set_target-176"></a>
### SET_TARGET 176

`GScript::SetIdTarget` 0x6FB8C0 (the function table names it SET_TARGET, 5 in, 0 out) throws an object so that it
lands on a point after a time:

- It pops the time, the point (z, y, x) and the object (0x6FB8D0..0x6FB91A). No thing (`GetScriptGameThing`): "Thing
  not valid" (0xC0CD88); then, whatever the thing, a time ≤ 0 (or NaN) is "Invalid time" (0xC0D804). Either ends it.
- A thing that is not an Object (`__RTDynamicCast` to Object, 0x6FB985: a town, a flock, a timer...) is left alone.
- From the thing's Pos: X = x × 10/65536, Z = z × 10/65536, Y = `LH3DIsland::GetAltitude` + its altitude
  (0x6FB997..0x6FB9CC). With t the time, at the game's single precision: fall = −9.81 × (t × t × 0.5)
  (0x6FB9D0..0x6FB9F6, the constants are floats), v = ((tx − X) × (1/t), ((ty − Y) − fall) × (1/t), (tz − Z) × (1/t))
  (0x6FB9FE..0x6FBA78; the − 0 of x and z changes nothing). Under gravity alone that lands on the point at t.
- The spin: three `GameFloatRand(200)` (ScriptFunctions.cpp lines 0x801..0x803, the synchronised stream), each
  (r − 100) × 0.01, in the order x, y, z (0x6FBA7C..0x6FBADD). They are drawn for every Object, flying or not.
- Already in physics (flag 0x40 at +0x24): `PhysicsObject::SearchForPhysicsObject` 0x646950; found, its body's
  velocity (po +0x104) is v and po +0x174 = 0 (PhysOb +0x14C, the drag); the spin is not used. None (a particle
  system carries it): nothing.
- Otherwise `InitialisePhysics` (vt +0x784) with v, the spin, no thrower, a body (true) and no interface status; the
  body it returns gets its drag cleared too (0x6FBB39..0x6FBB3F). So a script's throw flies without drag, in the air
  and in the water.
- The scripts that call it: MagicMushroom L206 (the hippy thrown to where the camera will be), theslavers L967,
  fireonhigh L42 and L209 (the meteors), Land2ShieldChallenge L315 (the boulders), throwbloke L151, landcontrol5
  L1515 and L1888 (Nemesis's meteors).

### SET_HEADING_AND_SPEED 127

`GScript::SetVelocityHeadingSpeed` 0x6FA770 pops the speed, the point (z, y, x) and the thing; no thing, nothing. The
velocity is the line from the thing (its map position, y = the land's altitude there + its own height) to the point,
made as long as the speed (0x6FA7E1..0x6FA8BC): with every part of the line 0 it stays 0; else the squares are summed
**z, then y, then x**, each step rounded to a float (0x6FA87C..0x6FA896), the root taken (`fsqrt`, 24-bit), the speed
divided by it and each part multiplied by that. A weather thing (vt +0x3FC) takes it as its movement
([Weather](../world/weather.md#chl-weather-objects)); an Object flying in the physics gets it as its body's velocity;
one at rest is thrown with it and a tumble: about each axis GameFloatRand(200) − 100 [0x8AB41C], times the float 0.01
[0x8C4B10] (0x3C23D70A), each step a float (0x6FA942..0x6FA9A9; not over 100, which differs by a unit in the last place
for about three draws in ten) (`ScriptPropertyRules`, `HeadingVelocity` / `HeadingSpin` in openblack).

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
   it). The Flock table (0x8DD108) holds 0x401940 at +0xFC, read from the exe: SET_POSITION does not go through
   `SetDomainCentrePos`, so the destination (+0x80) of the member that function also writes is left as it was.
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
- **GET_OBJECT_STATE** 143 (`GScript::GetScriptState` 0x6FB150) pops the object and pushes an integer (`PUSH(v, 1)`):
  - no thing (`GetScriptGameThing` 0x70D220): "Thing invalid" (0xC0D7B8) and 0;
  - a living thing (`IsLiving` vt +0x3C4): `GetFinalState` (vt +0xB04) as a byte (0x6FB18A..0x6FB198). A villager
    (0x751DD0) and an animal (0x41A240) give their top state (+0x8C) when its row is a final one (the state table's
    +0x1C), else the state they head for (+0x8D); a creature (0x477F20) and any other living give 0;
  - a puzzle game (`IsPuzzleGame` vt +0x498): `PuzzleGame::GetPuzzleGameStatus` 0x6DC070. Not complete
    (`PuzzleGame::IsComplete` 0x6D66E0, the same test PLAYED and the puzzle's own turn use): +0x584 ≠ 0 gives 4
    (GAME_IN_PROGRESS), else 5 (GAME_NOT_YET_BEGUN) (`neg; sbb; add 5`, 0x6DC07C..0x6DC086). Complete: the immersion
    puzzle (type 16) gives its +0x57C, the lion and sheep puzzle (type 18) gives 3 (WON_EVILWAY) for +0x490 = 2, 2
    (WON_GOODWAY) for 1, else 0; every other type 0 (WON). +0x584 is cleared by the constructor (0x6D625B) and set by
    each type's turn when its play begins; the fish puzzle (type 14) sets it with its bait and shoals (0x6D823B);
  - anything else: "Thing not a known type for functions" (0xC0D790) and 0.
  - The scripts that call it: MagicMushroom (L124, after PLAYED of its immersion puzzle: 0 is the right mushroom) and
    LionPuzzle (L56, a tilt puzzle, type 12: the introduction starts at 4, or at a click on the highlight).
- After SET_POSITION, the script state of the thing is step 7 above.

<a id="set_script_state-017-set_script_ulong-020-played-064-and-the-script-states"></a>
### SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020, PLAYED 064 and the script states

#### The three commands

- **SET_SCRIPT_STATE** (`GScript::SetScriptState` 0x6F8370) pops the state, then the thing ("Object no longer valid"
  0xC0D428; no thing at 0x6F8396).
  - A script container (+0x3F8, at 0x6F83B9): g_game +0x250090 +0x24 = the state and the type's loop function (the
    table 0xC0C73C by `GetScriptObjectType` vt +0x4E8) with 0x6F8280, which calls `GScript::SetScriptState` 0x6F82E0
    on every Living member **with no IsDrowning test**.
  - Its "No Loop funtion for type" (0x6F83FD) is never given: only a town, a dance and a flock are containers
    (`IsScriptContainer` in bw1-decomp's 1.20), and each has a loop function in the frozen exe's table at 0xC0C740
    (TOWN 0x6F7850, DANCE 0x6F78E0, FLOCK 0x6F78B0; read 2026-10-28). The state passed through +0x24 is written just
    before each loop that reads it (here 0x6F83CF / 0x6F82AB, SET_SCRIPT_ULONG's 0x6F87E2 / 0x6F875E, and its other
    users, 0x6F1120 / 0x6F7B20 and 0x6FA1EC / 0x6FA16B, write it first too) and is not saved (`xref.py field GScript
    0x24`). openblack hands the state to `script_containers::ForEachMember`'s callback instead, with the same result.
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
- **SET_SCRIPT_STATE_POS 018** (`GScript::SetScriptStatePos` 0x6F84C0) pops the point (three values), then the
  thing; no thing → "Object no longer valid" (0xC0D428). A script container (vt +0x3F8): g_game +0x250090 +0x40 = the
  point and the type's loop function (0xC0C73C; none → "No Loop funtion for type") with `SetStatePosLoopFunction`
  0x6F8460, which gives each Villager member (`dynamic_cast`) +0x10C / +0x110 = `MapCoords(point)` x, z. Otherwise a
  `dynamic_cast<Villager*>`, with no availability test: the same two fields; anything else "Object not villager for
  state data" (0xC0D460).
- **SET_SCRIPT_FLOAT 019** (`GScript::SetScriptStateFloat` 0x6F8640) is the same for the number, kept in the
  villager's +0x118 (`SetStateFloatLoopFunction` for a container's villagers), with the same messages.
- **PLAYED** (`GScript::HasPlayed` 0x6F9DC0), by what it reaches:
  - no thing → "Thing no longer valid" and **1** (0x6F9DE1);
  - a creature's plan (0x6F9DF4..0x6F9E7E,
    [Playing what a script gives it](../creature/script-control.md#playing-what-a-script-gives-it-and-played));
  - a villager: IsLiving at 0x6F9E83, the Villager cast at 0x6F9E9C and
    `Villager::IsScriptAnimationComplete` 0x7689D0 called at 0x6F9EAA: TOP 23 WAIT_FOR_ANIMATION → 0; TOP 200 → times == 0; otherwise 1;
  - another Living: `GetFinalState` == 4 IN_SCRIPT (vt +0xB04 at 0x6F9EC4, `sub al, 4; neg; sbb; inc` to
    0x6F9ED8); for an animal `Animal::GetFinalState` 0x41A240 reads its top state (+0x8C) and, when the state
    table does not mark that state final (the animal state table row's +0xC), the state it heads for (🔍 inferred from
    the start of the function and the animal code's own reading of it). openblack:
    `ecs::animal_ai::FinalState` (test `ScriptQueryNatives.PlayedOfAnAnimalIsWhetherItEndsInTheScriptsState`);
  - a weather thing → +0x78 == 0 (0x6F9EF0);
  - a PuzzleGame → `fn_006D66E0` (0x6F9F0B, [Fish puzzle](../world/sea-life-and-scenery.md#fish-puzzle));
  - anything else → "Thing not living" and 1 (0x6F9F31).
  - openblack (`Played` in `src/CHLApi.cpp`, read against 0x6F9DC0 on 2026-10-28) has every branch. It asks for a
    puzzle game before the livings, which changes nothing: a puzzle game is never living. Its creature test is
    `CreatureMindSystem::HasPlayed` (`creature_script_play::Played`: the agenda is over or the creature only idles,
    the rest cut as given up), on the plan the creature is carrying out, as the original's is on its current plan.
    The plan's own validity (`CreaturePlan::IsValid` 0x4F12E0) has no test of its own in openblack: a plan whose
    thing goes is stopped by the mind (`LoseGoneThings`), as `RemoveBeliefsAboutDeletedObjects` 0x4D83C0 clears the
    original's agenda; a plan made invalid any other way has not been compared.

#### PLAYED_PERCENTAGE 260

✅ `GScript::GetPlayedPercentage` 0x6F9D30 pops the thing (`GetScriptGameThing` 0x70D220):

- no thing → "Thing no longer valid" (0xC0C258) and **1.0**;
- a creature (vt +0x34 at 0x6F9D5C) → `LH3DCreature::GetBodyActionFraction` 0x484C60 of its 3D body (+0x160 +0x58):
  `CycleTime[0]` (+0x47D0, a whole number of milliseconds) over the `Duration` of `CurrentAnim`'s animation (+0x49A0,
  `GetAnim(CurrentAnim, 0)`), as `fild` / `fidiv`, stored as a float;
- anything else → "Not coded for non creature" (0xC0D654) and **1.0**.

What `CurrentAnim` and `CycleTime[0]` hold, by the body's state (`LH3DCreature::StateAction` 0x486A00 /
`StateSwitch` 0x4851A0):

- an action played once (INDIVIDUAL, OBJECT_KEEP): the action and its time, which stops short of the length
  (the turn that would pass the end goes back to standing instead);
- a sequence (START_STATIC, DO_STATIC, FINISH_STATIC): `CurrentAnim` is the loop, the start being `CurrentAnim - 1` and
  the end `CurrentAnim + 1`, so every part is measured against the loop's length;
- standing: `StateSet` 0x484EC0 puts `CurrentAnim` to 0, the stand pose (0x484F20), and `DoStandingAction` 0x48D250
  sets `CycleTime[0]` to the breath's point (+0x4988) times the stand's length, below it;
- moving: a walk starts only from standing or moving (`StartMovingToPoint` 0x483FE0 asks `IsMovingOrStanding`), so
  `CurrentAnim` stays 0 and every time is measured against the stand's length. `CycleTime[0]` is the time of
  `CycleAnim[0]`, the first of the blended animations: waiting, the breath (`DoStandingAction`); walking below walking
  speed, the stand pose at the breath; faster, the run kept in step with the walk (`AdvanceMovementAnimations`
  0x48D940); turning on the spot (`DoTheTurningBusiness` 0x48B970) and stepping off (`UpdateMotion` 0x490960, its
  step case) the turn's or step's own time;
- the dance: `StartDanceAction` 0x484830 leaves `CurrentAnim` alone, so its start is measured against the stand's
  length; each move is `CurrentAnim` (0x484F5E), measured against itself; the finish keeps the last move, or the stand
  pose when the dance ended in its start;
- a fight, a knock-out, the actions on things and the catch: below.

✅ In a fight `CurrentAnim` is the action the fighter counts as doing, the one GET_CREATURE_FIGHT_ACTION reads
([Fights: the script natives](../creature/fights.md)):

- the fight's start comes from standing, 0, and the stance (FIGHT_MAIN) puts it back to 0 (`StateSet` 0x4850B7), so
  the start, the stance and the finish are measured against the stand's length;
- a blow or a step (`StartFightAction` 0x4846E0) and a reel (`StartFightRecoilAction` 0x484790) set it to what they
  play;
- a block starts with it set to the block (143, `AttemptAttack` 0x48A112), which its start, its loop and its end are
  measured against; a reel in the block sets it to the block's reel (198, `StartFightRecoilAction`), which the rest of
  the block keeps;
- a cast starts with it set to the cast (111, `CheckFightQueue` 0x4896FD), for its start, loop and end;
- the faint (106, `StartUnknownAction6A` 0x4803A0 at 0x4803B0) and getting up (107, `Resurrect` 0x484E20 at 0x484E3D)
  set it to theirs.

`CycleTime[0]` is the time of the animation each of these states plays. Knocked out after a fight the creature lies in
its faint (106) until it gets up (107), as above.

✅ The actions on things start from standing, so `CurrentAnim` is 0 unless the start writes it; only two do (every
other start below has no write to +0x49A0):

| openblack `creature_object_actions::Kind` | The game's start, body state | Measured against |
|---|---|---|
| PickUp, Destroy | `StartBlendedAction` 0x480E10 (PICKING_UP, DESTRUCTION; `DoBlendedAction` 0x48BCC0) | the stand pose |
| Throw | `ThrowAtPointGivenTime` 0x4811A0 (THROWING) | the stand pose |
| Point | `PointAt` 0x484920 (POINTING) | the stand pose |
| TakeFromHand | `StartPickUpFromHandAction` 0x480FC0 (PICK_UP_FROM_HAND) | the stand pose |
| Keep | `StartObjectKeepAction` 0x484C80 (OBJECT_KEEP), called by 0x500888 with the sub-action's animation | its own animation |
| PutDown, Discard, Lob, Eat | `StartObjectRemoveAction` 0x484E60 (OBJECT_REMOVE, `DoDiscardAction` 0x48C6C0): put down 97 at 0x4FEF54, eat 96 at 0x5002E2, the others with the sub-action's animation (0x500226, 0x4FEFAA) | its own animation |

The kick (`StartKickingAction` 0x480C00) and the kiss (`StartKissing` 0x484590) leave it too, against the stand pose;
openblack has neither as an action on a thing. The catch (`StartCatching` 0x480AC0) waits and reaches against the
stand pose; its sideways step sets `CurrentAnim` to the step, 225 (`DoCatchMain` 0x48860E), which the waiting after it
(CATCH_MAIN) and the reach (CATCH_ACTION) keep, as neither state writes it, until the catch ends or gives up and the
creature stands again (`StateSet` STANDING, 0x484F20). In each, `CycleTime[0]` is the action's own time.

The shipped `challenge.chl` calls it twice, both in `EndOfGameCreatureSequenceGoToVolcano` (Land 5's ending), which
gives the creature clip 214 (SET_SCRIPT_ULONG, state 200) and waits for it to pass 0.8 and then 0.9.

#### The script states

| State | Per-turn function | Entry | Exit | Clip |
|---|---|---|---|---|
| **4 IN_SCRIPT** | `StateInScript` 0x5ED9A0: creates a DataForScriptRemind if there is none; 1 | `EnterInScript` 0x5ED7E0 (vt +0x940): 1 if `IsStateEntryFunctionSameAs(final, next)` 0x7524D0 or there is no script memory | `ExitInScript` 0x5ED9C0 (vt +0x914), below | the state's own |
| **200 SCRIPT_PLAY_ANIM** | `ScriptPlayAnim` 0x768970: with times > 0, one less and `PlayAnimThenSetState(times ? 200 : 4)` | `EnterPlayAnim` 0x768840 (as EnterInScript) | `ExitPlayAnim` 0x7689C0 = ExitInScript | `ScriptAnimation` 0x768A00 = +0x11C |
| **23 WAIT_FOR_ANIMATION** | `WaitForAnimation` 0x5EC990: `IsReadyForNewAnimation(1)` → `SetTopStateToFinal` and 0, otherwise 1 | | | unchanged |
| **199 SCRIPT_WANDER_AROUND_POSITION** | `ScriptWanderAroundPos` 0x7687F0, below | entry 10 = `EnterInScript` (vt +0x940) | exit 26 = the vt +0x914 slot, as ExitPlayAnim (`ExitInScript`) | the state's own |

- **`ScriptWanderAroundPos`** 0x7687F0: creates a DataForScriptRemind if there is none; then `mov ax, [esi+0x58]`,
  `test ax, ax`, +0x58 = ax − 1 (`lea`, flags untouched) and `jg` 0x768818: the counter is read as a **signed** 16-bit
  number, and the villager sets off when it was ≤ 0 (0, or 0x8000..0xFFFF), calling `SetupNewScriptWander` 0x7686D0.
  It returns 1. ✅ Ours reads it the same way, a count past the signed range setting off at once
  (`villager_script_rules::StepWander`, ours, from the exe).
- **`SetupNewScriptWander`** 0x7686D0: MapCoords from the place (+0x10C / +0x110, a `JustWholeMapXZ`, a plain copy of
  x and z, 0x603070); `GameFloatRand(6.2831855f)` (0x40C90FDB, line 0x3C) then `GameFloatRand(+0x118)` (the distance,
  line 0x3D); `GUtils::AddDistanceFromAngle(&coords, bearing, distance)` 0x74D510;
  `Living::SetupMoveToWithHug(coords, 199)` 0x5F2890, its result unused; then +0x58 = +0x11C +
  `GameRand(+0x120 − +0x11C)` (line 0x41), stored as a word. +0x11C / +0x120 are the clip and times
  SET_SCRIPT_ULONG writes (our `ScriptAnimation`), so the pair is shared with SCRIPT_PLAY_ANIM.

- **`ExitInScript`**: `CircleHugInfo::Reset` 0x60A9F0 first, whatever follows (the circle, TurnsToObj 0xFF,
  EntryDistance 0; details in [villager speed and scripts](../beings/villager-speed-and-scripts.md#the-original));
  `IsScriptState(next)` (+0x960, table field 0x18) → 1; otherwise it
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

#### The thing a native is given

✅ `GetScriptGameThing` 0x70D220 (Ghidra C of the v1.42 exe, read 2026-10-28) is the look-up of 225 call sites, every
GScript native that takes a thing (from `CreateDualCamera` to `VortexFadeOut`):
- id 0: "Jonty - Script looking for null object" and none; id above 0x1FF: "Jonty - Out of script range" and none; a
  slot with no thing (`BaseInfo::GetBase`): "Check-Deleted thing still accessed" and none;
- then the thing's `IsAvailable` (vt +0x2C, 0x70D28B); 0 gives none, **before** the take-control step, so a native
  that takes control (`TakesControl`) does not take a thing it cannot reach;
- else the "should be in script" check (vt +0x448) and, when not yet controlled (+0x94 & 4), the native's
  take-control flag (vt +0x440, GScript +0x3C).

Slot +0x2C over every vtable (`xref.py vtcalls 0x2C`): every GameThing class has `GameThing::IsAvailable` 0x401810
(the flag byte +0xA bit 0 clear: not being deleted), except `Villager` and `SpecialVillager`, which have
`Villager::IsAvailable` 0x751D50 (bw1-decomp Villager.cpp): the flag clear **and** `GetFinalState` (vt +0xB04)
not 14 DYING. So the natives see no thing for a thing being deleted and for a villager whose final state is DYING
(13 SET_DYING is still reached). openblack: `ecs::script_held::IsReachable` (`Reachable` is the rule), used by every
native's look-up in `src/CHLApi.cpp` (`ScriptThing`, `CameraThing`, `SpiritThing`, `LeashThing`, `MusicThing`,
`ResourceObject`, `ScriptTown`, `FightNativeCreature`, `FetchForNative` and each native's own test) and in
`src/Magic/Script` (fire, spells, weather's wind, worship, influence); before 2026-10-28 they asked only whether the
entity was there. Tests `ScriptProperties.ANativeReachesAThingThatIsThereNotBeingDeletedAndNoDyingVillager`,
`ScriptProperties.OnlyAVillagerGoingToItsDyingStateIsOutOfTheScriptsReach`. While openblack's deferred deletion is
off (`ecs::SetDeferredDeletion`, ToBeDeleted.h) nothing is ever being deleted in play, so only the dying villager
changes what a native does. The storm natives (cloud, lightning, time fade and weather properties) pass the storm to
the weather, which tests it there; a storm is never a villager.
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
`fn_005FB520`); anything else `CastSpellAtPos` at its position. It pushes the spell, handed to the script as one it made (`AddScriptGameThing(spell, 1)` 0x70C140; `SPELL_AT_POS` the same at 0x70C2E4), so a script holding it controls it; openblack registers it with `Locator::scriptObjects` as the script's own. A script cast's process info
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
| CHANGE_INNER_OUTER_PROPERTIES 056 | `GScript::ChangeObjectProperties` 0x70F480 | calm, outer, inner, the thing | a flock first (vt +0x3EC); then a weather thing (vt +0x3FC) → `fn_00774340`(inner, outer): +0x34 = inner, +0x38 = outer (the descriptor's inner and outer radius, next to +0x3C fade-in and +0x40 life that `SetTimes` 0x774460 writes), then `UpdateStats` 0x774370; the calm is not used. No thing, or anything else: "Invalid thing for Changing Variables" (0xC20A54) |
| SET_AFFECTED_BY_WIND 131 | `GScript::SetAffectedByWind` 0x6FABC0 | the thing, then the value | no thing → "Invalid thing" (0xC0C6EC); otherwise vt +0x4F0 with the value as it was popped: `WeatherThing::SetAffectedByWind` 0x55DF20 stores it at +0x7C, `GameThingWithPos`'s 0x4024C0 does nothing |
| PAUSE_UNPAUSE_CLIMATE_SYSTEM 401 | 0x6FF4E0 | the argument | 0xC24759 = argument ≠ 0 |
| PAUSE_UNPAUSE_STORM_CREATION_IN_CLIMATE_SYSTEM 402 | 0x6FF500 | the argument | 0xC24758 |
| KILL_STORMS_IN_AREA 404 | 0x6FF520 | position, radius | `fn_0083F750` |
| SET_PLAYER_WIND_RESISTANCE 399 | `GScript::SetNoWindResistanceFlag` 0x6FEF10 | the script player (ftol, `ConvertScriptPlayerToGamePlayer`), then the value | GPlayer +0x940 = the value as popped (no player: nothing); nothing pushed. Its reader: `Object::ThrowObjectFromHand` 0x638700, the base of every class's throw: with the hand's player's (`GInterfaceStatus::GetPlayer`) +0x940 ≠ 0 the new physics object's drag +0x174 = 0 (0x63870E). `GPlayer::SetToZero` 0x649340 (its constructor, `GGame::Close`) clears it. openblack: `ecs::wind_resistance` |
| GET_PLAYER_WIND_RESISTANCE 400 | `GScript::GetNoWindResistanceFlag` 0x6FEEB0 | the script player alone | pushes GPlayer +0x940 (0 for no player), type 6 |
| IS_WIND_MAGIC_AT_POS 262 | `GScript::IsWindMagicAtPos` 0x70C8D0 | radius, then the point (z, y, x) | walks the storm list [0xEEA37C] (next +0x3BC, marked storms too): a storm whose descriptor has no clouds (+0x28 = 0) and `GUtils::GetDistance`(point, its position +8) < its outer radius +0x18 plus a float that is the radius's popped *type*, not the radius (0x70C94D: [esp+0x10] is POP's type out-value; the radius itself is overwritten by x at 0x70C932), pushes 1 (type 6), else 0. openblack: `weather::storms::IsWindMagicAt` |

The game's own function table (built by `_$E27` 0x6FFFD0, ebp = 1, ebx = 2, edx = 0 from 0x700016..0x700029) lists
IS_WIND_MAGIC_AT_POS with 1 value in and 1 out (0x704E3D / 0x704E43), and SET_ and GET_PLAYER_WIND_RESISTANCE with 2
in and 1 out (0x70785B..0x70789E), which is how the language declares them; their functions pop 4, 2 (pushing nothing)
and 1. openblack's bindings carry what the functions pop and push, the language's table keeps the declared counts.

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
REMOVE_REFERENCE are the slot table's [references](#references). The creature fight natives (IS_FIGHTING, SET_CREATURE_AUTO_FIGHTING, the fight queue's
and GET_CREATURE_FIGHT_ACTION) are on [Fights: the script natives](../creature/fights.md#fights-the-script-natives).

#### IS_AFFECTED_BY_SPELL 385

- `GScript::AffectedBySpell` 0x6FF000 (read in the v1.42 exe 2026-10-28) pops the magic type, then the thing. No thing:
  "Object dead man!" and false. Not a creature (`IsCreature` vt+0x34): "Object must be a creature!" and false. A
  creature: `GMagicInfo::Infos[type]` → `AsMagicCreatureSpellInfo` (vt+0x3C; 0x5FA830 gives the record itself for a
  creature spell's row, the base 0x5FB780 none), none → false; else
  `CreatureReceiveSpell::IsCreatureSpellActive(row +0x58, the receive-spell type)` on the creature's +0x370 (the
  slot's state ≠ 0, so a spell waiting to start counts, [temple-leash-posts.md](../creature/temple-leash-posts.md))
  pushed as the boolean. The type is not range-checked (a type past 41 reads past the table). The shield's stones ask
  it for spell 33, invisible, before striking the creature (refs/chl/throwthroughshield.txt:138303).
- openblack: `IsAffectedBySpell` in `src/CHLApi.cpp`, through `magic::GetMagicInfoAs<GMagicCreatureSpellInfo>` and
  `creature_spells::SpellOf(creatureReceiveSpellType)` on the creature's `CreatureSpells` (any phase but Off); a type
  outside 0..41 is false. Tests `AffectedBySpellNative.*` (`test/creature/test_affected_by_spell_native.cpp`).

#### SET_MAGIC_RADIUS 230

- `GScript::SetRadius` 0x70C660 (read in the v1.42 exe 2026-10-28) pops the radius, then the thing; none: "Thing not
  valid". A spell (`IsSpell`, vt+0x400): `Spell::SetSpellMagnitude` 0x7202E0, +0xBC (the magnitude
  `GetSpellMagnitude` 0x7202C0 reads, a shield's and a storm's 2D radius, cost and reach) = the radius, and when it has
  an effect (+0xB0) the effect's `SetMagnitude` (psys vt+0x11C) with it. Nothing else is told: the shield's reaction
  and anti rings keep the radius of their cast. The shield's stones resize it as they light
  (refs/chl/throwthroughshield.txt:138976).
- openblack: `SetMagicRadius` in `src/CHLApi.cpp`: the `Spell` component's magnitude, then its effect's
  (`particles::Effect::SetMagnitude`); tests `MagicRadiusNative.*` (`test/test_magic_radius_native.cpp`).

#### BELIEF_FOR_PLAYER 247

- `GScript::GetThingBeliefForPlayer` 0x6FCE60 pops the player, then the thing; either missing is a script error and 0.
  A town (`IsTown` vt 0x1B8) answers `Town::GetBeliefInPlayer` 0x73BAB0; any other thing 1.0 when its `GetPlayer`
  (vt 0x1C) is the player, else 0, so every kind answers through its own player (a building its town's owner, a rock
  or a dead tree the one it was made for, a living tree none, `Tree::GetPlayer` 0x55D8C0). In the shipped game only
  towns are asked: the 21 calls in `challenge.chl` all name a town, and no land script uses it. openblack:
  `chlapi::BeliefOwnerOf` (`effects::HarmedPlayerOf`, none for a living tree), test
  `ScriptProperties.AnyThingThatIsNotATownBelievesInThePlayerItBelongsTo`.

#### Creating things

- **`GScript::CreateObject`**: sub-type 5000 is allowed only for the types that ignore it (0x6F11BD). By type:
  - a scaffold is `Scaffold::Create`(pos, `GScaffoldInfo` 0xD959D0, 0, 0, angle, scale) (0x6F13BF..0x6F13D5);
  - a timer is `fn_007115A0`((float)sub-type), the sub-type being its seconds (0x6F1253..0x6F1263);
  - a vortex is `LandscapeVortex::Create`(&pos, sub-type, 50.0) 0x5FE6E0 (0x6F171B..0x6F1730);
  - a whale (the shark) is `Whale::Create` 0x774C50(pos, &GMobileObjectInfo[24], 0, angle, scale) (0x6F1747);
  - an abode is "Invalid create type" (0x6F191D).
- **A timer thing**: `GScript::CreateTimer` 0x6F1D20 pops the seconds, makes the timer the same way
  (its turns ftol(1000 / [0xD01A38] × seconds), `ScriptTimer::SetTime` 0x711630, with [0xD01A38] = 100 ms set by
  `GGame::Init` 0x54F4A5, so 30 s is 300 turns), adds it with
  `AddScriptGameThing(t, 1)` and pushes it (0 and "Thing not created" when nothing was made).
- **`GScript::CreateFlock`** 0x6F21F0 pops the position (z, y, x) and adds the new flock with
  `AddScriptGameThing(f, 1)`.
- **CREATE_RANDOM_VILLAGER_OF_TRIBE** 353 ✅ (`GScript::CreateRandomVillagerOfTribe` 0x6F2F50, frozen v1.42 exe)
  pops the point (z, y, x), then the tribe (0x6F2F61..0x6F2FA5), draws `GameRand(7)` on the synced stream
  (`ScriptCreateOrCall.cpp` line 0x435, 0x6F2FB8) and takes `GVillagerInfo::InfoList` [0xDA6BE8] row tribe × 7 + the
  draw (932-byte rows, no range check). `Villager::Create`(MapCoords(point), the row, its +0x138 `grownUpAge`, 0)
  0x74FBE0 (0x6F2FFB). Made: `AddScriptGameThing(v, 1)` (a thing the script created; 0x6F300C), `PUSH(id, 4)` and the
  empty create name; nothing else (unlike CREATE, no second age and no IN_SCRIPT). Not made: "Villager not created"
  (0xC0CD98) and 0. Its one caller is Land 1's Pied Piper (`SendTheKidsHome` L253, ip 49498: Norse, tribe 7, at
  the cave flock, one for each child shut in the cave). openblack: `CreateRandomVillagerOfTribe` in `CHLApi.cpp` with
  `villager_script_rules::RandomVillagerRow` and `VillagerArchetype::Create`; a row outside the table (which the
  original reads past) makes nothing, "Villager not created" and 0 (`test/test_villager_script.cpp`,
  `test/test_script_query_natives.cpp`: `RandomVillager*`, `CreateRandomVillager*`).
- Both CreateTimer and CreateFlock also give the thing the script's debug name (`SetScriptNameOfCreate` 0x56FA70),
  which is an empty function in the v1.42 exe: nothing is kept
  ([naming a create](map-script-reader.md#the-natives)).

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
- **CALL_NEAR_IN_STATE** 316 (`GScript::CallNearInState` 0x6F1010) pops excludingScripted, the radius, the point
  (z, y, x), the state, the sub-type, then the type (0x6F1025..0x6F10B5). The type test, the messages and the result
  are CALL_NEAR's, and so is the find function (table +4, 0x6F1101). The state goes to GScript +0x24 (the loop value,
  0x6F1120), and the filter is `FindNearInStateCheck` 0x6F7AE0 (behind `...ExcludingScriptObjects` 0x6F7B40 when
  excludingScripted): `IsLiving`, then the type and sub-type (`FindGeneralCheck`), then `(u8)GetFinalState() ==`
  the state. There is **no distance test**: the nearest such thing in the square of cells within the radius. A town's
  and a creature's find functions do not call the filter, so for them the state is not looked at. The only caller is
  TheLostFlock (L387, ip 59738: a dead sheep, state 15, within 30 of the field; type 6, sub-type 4).
- **CALL_IN** (`GScript::CallIn` 0x6F22F0) pops excluding, the container, the sub-type and the type; found:
  `AddScriptGameThing(found, 0)` (0x6F2383); else a warning and 0 (0x6F23AE).
- **CALL_IN_NEAR** 67 ✅ (`GScript::CallInNear` 0x6F2450, frozen v1.42 exe) pops excludingScripted, the radius, the
  point (z, y, x), the container (`GetScriptGameThing`), the sub-type, then the type (0x6F2465..0x6F24FE). No
  container: the warning "Cannot find in Near. %d.%d" (0xC0CD10, type and sub-type) and 0 (0x6F259C). Else the
  container's own type (vt +0x4E8) picks its row of the table 0xC0C730 (24 bytes a type), whose +4 is the near find:
  `FindInTownNear` 0x6F7470, `FindInDanceNear` 0x6F75C0 and `FindInFlockNear` 0x6F7570 (types 9, 10, 11; none for
  any other type: "Cannot look in object", 0xC0CCE4, then the warning and 0). It is called (container,
  `FindNearGeneralCheck` 0x6F7070 or, when excludingScripted, `...ExcludingScriptObjects` 0x6F7A20, type, sub-type,
  MapCoords(point), radius). Each near find stores the point in GScript +0x14 and the radius in +0x20 and calls the
  same walk as CALL_IN's find: `Flock::FindLiving` 0x530510, `Dance::FindLiving` 0x50C3D0, and for a town
  `Town::FindVillager` 0x73E600 (types 4 and 5) or `Town::FindAnimal` 0x73E6E0 (6). Unlike `FindInTown` 0x6F7410
  there is no storage pit (16): any type but 4, 5 and 6 in a town is "Looking for strange type in Town" (0xC0D390)
  and 0. The filter: not in a script (vt +0x448) when excluding, then `FindGeneralCheck` 0x6F6FA0 (type, sub-type),
  then `GetDistanceInMetres`(point, the thing, or a worship site's totem `GetTotemPos` 0x77CF30) compared with the
  radius: `fcomp` and `test ah, 0x41` pass it when below, equal, or unordered (0x6F70CF..0x6F70EB). Found:
  `AddScriptGameThing(found, 0)` (0x6F2582) and `PUSH(id, 4)`. Callers: Land 1's Pied Piper
  (`PiedPiperIntelligence` L573, ip 50437: a child, type 5, of the Norse town within 100 of the piper, excluding
  scripted), Land 2's slavers (`SlaverGoMad` L522, ip 95008: a villager within 100) and Land 4's gremlin
  (`ControlGremlin` L567, ip 122838: within 45). openblack: `CallInNear` in `CHLApi.cpp` and `ecs::script_containers::FindNear` (`test/test_script_query_natives.cpp`,
  `CallInNear*`).
- **CALL_IN_NOT_NEAR** 141 ✅ (`GScript::CallInNotNear` 0x6F25E0, read in the v1.42 exe 2026-10-28) pops as CALL_IN_NEAR
  and takes the row's +8, the not-near find (0xC0C738 + 24 × type): `FindInTownNotNear` 0x6F7710, `FindInDanceNotNear`
  0x6F76C0 and `FindInFlockNotNear` 0x6F7670, each the near find's twin (the point and radius stored, the same walks
  and the same town types, no storage pit), with `FindGeneralNotNearCheck` 0x6F7190 or, when excludingScripted,
  `...ExcludingScriptObjects` 0x6F7A50 (not in a script, vt +0x448, first). The check passes only when the distance is
  greater than the radius: `fcomp` and `test ah, 0x41` fail it when below, equal or unordered (0x6F7200..0x6F720B).
  Found: `AddScriptGameThing(found, 0)` and `PUSH(id, 4)`. Not a container: "Cannot look in object"; none found, or no
  container: the warning "Cannot find in Near" (0xC0CD2C, without the numbers) and 0. The only caller is Land 2's
  slavers (`SlaversCheck`, refs/chl/theslavers.txt:928: a villager of the slave flock farther than 10 from the pen,
  an escapee). openblack: `CallInNotNear` in `CHLApi.cpp` and `ecs::script_containers::FindNotNear`
  (`test/test_script_query_natives.cpp`, `CallInNotNear*`).
- **CALL_FLYING** 383 ✅ (`GScript::CallFlying` 0x6F31E0, read in the v1.42 exe 2026-10-28) pops excludingScripted, the
  radius, the point (z, y, x), the sub-type, then the type. It stores the radius in GScript +0x20 and the point in +0x14
  (x and z as ftol(× 6553.6), the altitude the point's y less `LH3DIsland::GetAltitude` there, which the 2D distance
  does not read), then walks the physics objects (0xD47814, count 0xD47818, 0x1DC bytes each, the thing at +0x18) in
  their order (fn at 0x646980) with `FindNearGeneralCheck` 0x6F7070 or, when excluding, `...ExcludingScriptObjects`
  0x6F7A20 (CALL_NEAR's filter: the type, the sub-type and the distance not above the radius), the first passing one
  `AddScriptGameThing(found, 0)` and pushed; none: 0, with no message. The type is not checked. Used by the shield's
  bloke, startled by a rock (33) or a creature-thrown thing flying within 40 of him (refs/chl/throwthroughshield.txt:8).
  openblack: `CallFlying` in `CHLApi.cpp`, through `DynamicsSystemInterface::ForEachEntry` (the physics list, in its
  places' order; an entry whose object is gone is skipped where the original would still test it until the next turn
  start); test `CallFlyingNative.*` (`test/creature/test_call_flying_native.cpp`).
- **GET_DEAD_LIVING** 446 ✅ (`GScript::GetDeadLiving` 0x6F3450, read in the v1.42 exe 2026-10-28) pops the radius, then
  the point (z, y, x), stores them as CALL_FLYING does, and asks `FindNearPos` 0x6F7280 (type 0, sub-type 0) →
  `MapCoords::FindNearForScript` (the nearest accepted thing of the map's cells of the square ±r, not cut at r) with
  the filter 0x6F3400: (`IsAnimal` vt+0x454 or `IsVillager(0)` vt+0x2C8) and `IsLiving` vt+0x3C4 and `IsDead`
  vt+0xAF4 (`Living::IsDead` 0x417270: the dead status bit, the top state 15, or not functional). Found:
  `AddScriptGameThing(found, 0)` and pushed; none: 0, with no message. The singing stones' resurrection song asks it
  every 0.3 s (refs/chl/SingingStonesSongs.txt:120). openblack: `GetDeadLiving` in `CHLApi.cpp` through
  `ecs::map_cells::FindNearForScript` with `villager::IsDead` / `animal_ai::IsDead`; test
  `ScriptQueryNatives.GetDeadLiving*`.
- **GET_NEAREST_TOWN_OF_PLAYER** (`GScript::GetNearestTownOfPlayer` 0x6F2A20) pops the radius, the player (a float,
  `__ftol` 0x6F2A96) and the position's z, y and x, converts the player (0x6EB9A0, `GetPlayer` 0x5509B0) and takes
  `GScript::FindPlayerTownAtPos` 0x6F72E0 (only that player's towns, the nearest within r); none is "Did not find
  town" (0x6F2AEA) and 0, else `AddScriptGameThing(town, 0)` (0x6F2B0D).
- **GAME_TYPE** 220 ✅ (`GScript::GetType` 0x6FCBE0, frozen v1.42 exe) pops the thing; none is "Object no longer
  valid" (0xC0D428) and 0; else its `GetScriptObjectType` (vt +0x4E8, 0x6FCC20), the type the finders match. Both
  are pushed as an int (type 1, 0x6FCC11 and 0x6FCC2C). Its one caller is the singing stones' `BringBackTheDead`
  (L176, ip 86051, on a thing that died, cast to a float). openblack: `GameType` in `CHLApi.cpp` pushes
  `ecs::script_type::TypeOf` (`test/test_script_query_natives.cpp`, `GameType*`).

#### Containers

- `GScript::FlockAttach` 0x6EF3C0 pops the leader flag, the target, then the thing.
- `GScript::FlockDetach` 0x6EF9E0 pops the container, then the thing.
- `GScript::DisbandId` 0x6EFDC0 disbands.
- `GScript::IdSize` 0x6EFF50 pushes the members as a float, 0 with "Cannot Find Flock/Dance/Town Size" (0x6F0073).
- CHANGE_INNER_OUTER_PROPERTIES (0x70F480) pops calm, outer, inner and the thing.
- **GET_FIRST_IN_CONTAINER** (`GScript::GetFirstInContainer` 0x6F05A0, frozen v1.42 exe) ✅ [V] pops the container,
  takes `GetScriptGameThing` (0x6F05B3); no thing or not a script container (vt +0x3F8: only Town, Dance and Flock
  override it) logs "Invalid container" (0x6F05CF, string 0xC0C6FC) and pushes 0. Then by `GetScriptObjectType`
  (vt +0x4E8): 9 a town takes its abode list's head (+0x754) and, if that abode has an inhabitant (+0xA0), that
  inhabitant, else the head of its homeless list (+0x768): a later abode is never looked in (0x6F0622..0x6F0636);
  10 a dance takes `GroupBehaviour::GetFirstDancer` 0x597ED0, the head of the first group's dancers (group 0 only;
  no group at all reads through a null pointer); 11 a flock takes its member list's head node's thing
  (+0x3C, node +8). A member is pushed as `AddScriptGameThing(member, 0)` (0x6F0643), a found thing; none is 0.
- **GET_NEXT_IN_CONTAINER** (`GScript::GetNextInContainer` 0x6F0670) ✅ [V] pops the thing it follows first: no
  thing (0, or one `GetScriptGameThing` no longer finds) calls GET_FIRST_IN_CONTAINER, which pops the container
  (0x6F0695). Else it pops the container, the same "Invalid container" test, and walks the type's list to the
  member after that thing, 0 when the thing is the last or not in it. A flock (0x6F071A) uses its member list's
  own cursor (+0x44), set back to the head (+0x3C) at every call, so nothing carries over between calls: the
  script's variable is the cursor. A town (0x6F07B7) walks each abode's inhabitants (+0xA0, next +0xE4), abode by
  abode (next +0x9C), then the homeless (+0x768, next +0xE4). A dance (0x6F0780) walks `GroupBehaviour`'s
  `DanceMembers` (an `LHLinkedList<Living*>`: head +0xA4, count +0xA8), which nothing fills while the game runs,
  so the answer is always 0 ✅ [V]. A sweep of every function of the exe for a write at +0xA4 or +0xA8 (296
  functions) leaves, besides other classes' fields and stack slots, only `GroupBehaviour`'s constructor (empties it),
  `Dance::ToBeDeleted` (removes), `GroupBehaviour::Load` (`GameOSFile::ReadSafe`, refilling what Save wrote, so empty
  again) and `GroupBehaviour::Save`; the only head store paired with a count change outside them is in
  `WorshipSite::AddTown` and an LHInetWeather file reader (0x5E9740). No `lea [x+0xA4]` in the exe feeds a list
  insert, and `LHLinkedList<Living*>::AddToEnd` 0x50E280 is called only by `DanceGroup::Load`.
  Callers in the shipped `challenge.chl`: `theslavers` (Land 2) only, on its slaver posse and its animal flock (both
  flocks), as `Slaver = get first in Posse` / `while Slaver valid ... Slaver = get next in Posse after Slaver`.

#### Poison

- Every thing answers **IsPoisoned** (vt +0x4A4) and every object takes **SetPoisoned** (vt +0x69C) its own way:
  - a Living (a villager, an animal or a creature): the bit 0x2 of +0xB4 (`Living::IsPoisoned` 0x416F90,
    `Living::SetPoisoned` 0x416FA0 writes `poisoned & 1` there);
  - a pot or a pile (Pot, PotStructure, PileFood, PileWood, PileResource, MagicFood, MagicWood, PuzzleGrain): the bit
    0 of +0x74 (`Pot::IsPoisoned` 0x55D4E0, `Pot::SetPoisoned` 0x55D510, `Pot::SetPoisonedResource` 0x55D550);
  - a storage pit: `StoragePit::IsPoisoned` 0x7336B0 is `IsPoisonedResource(0) || IsPoisonedResource(1)`
    (0x733550: the food pile +0xC4, or one of the five wood piles +0xC8, there (vt +0x2C) and poisoned).
    `StoragePit::SetPoisoned` 0x7335D0 is `SetPoisonedResource(0, p)` (0x7335F0): the food pile, when it is there,
    gets `p || pile->IsPoisoned()`, so a script can poison a store's food but never cure it; the wood piles are not
    touched;
  - a MobileObject (`MobileObject::IsPoisoned` 0x6079E0): poisoned when its info is the toadstool's
    (`InfoList` + 0x147C); it has no SetPoisoned of its own (`Object::SetPoisoned` 0x402780 does nothing);
  - everything else is never poisoned (`GameThingWithPos::IsPoisoned` 0x402400) and ignores SetPoisoned.
- **IS_POISONED** 162 (`GScript::IsPoisoned` 0x6FB520): pops the thing; none is "Thing not valid" (0xC0CD88) and
  false, else its IsPoisoned as a bool.
- **ID_POISONED_SIZE** 161 (`GScript::GetPoisonedSize` 0x6FB5B0): pops the thing; none is "Thing not valid" and 0.
  The type's loop function (the table 0xC0C728, +0x14 of the type's 24 bytes: only TOWN 9 `TownLoop`, DANCE 10 and
  FLOCK 11 have one) runs `CountPoisonedFunction` 0x6FB580 on each member, counting the poisoned ones in GScript
  +0x34; the count is pushed as a float. A thing without a loop is "No Loop funtion for type" (sic, 0xC0C688) and 0.
- **SET_POISONED** 173 (`GScript::SetPoisoned` 0x6FB6A0): pops the thing, then the flag; none is "Thing not valid".
  A script container (vt +0x3F8) runs `SetPoisonedLoopFn` 0x6FB680 (SetPoisoned with the flag) on each member
  through its loop function, and nothing when it has none; anything else that is not an Object (`__RTDynamicCast`)
  is "Thing not object" (0xC0D7F0); an Object takes SetPoisoned.
- **CALL_POISONED_IN** 163 and **CALL_NOT_POISONED_IN** 164 (`GScript::CallPoisonedFromId` 0x6F2840,
  `CallNotPoisonedFromId` 0x6F2930): pop excluding, the container, the sub-type and the type, like CALL_IN, but walk
  the container's loop function (+0x14), not its find function (+0x8), with `FindCheckPoisoned` 0x6F6FF0 /
  `FindCheckNotPoisoned` 0x6F7030 (IsPoisoned, or not, and `FindGeneralCheck` 0x6F6FA0 on the type and sub-type),
  behind `FindCheckPoisonedExcludingScriptObjects` 0x6F7A80 / `FindCheckNotPoisonedExcludingScriptObjects` 0x6F7AB0
  (not IsInScript, vt +0x448) when excluding. The first member that passes is added with `AddScriptGameThing(found,
  0)` and pushed as an object. No container gives the warning "Cannot find poisoned in" (0xC0CD40) / "Cannot find not
  poisoned in" (0xC0CD58) and 0; a container without a loop gives the error "Cannot look in object" (0xC0CCE4) first.
- The only script that uses them is Land 2's Plague (`plague.txt`): SET_POISONED on the town (its villagers), on the
  trader and on the store at the start, ID_POISONED_SIZE on the town each pass, CALL_NOT_POISONED_IN for the first
  healed villager, IS_POISONED on him and on the food in the hand, and SET_POISONED false on the store (which leaves
  its food poisoned) and on the town at the end. CALL_POISONED_IN is not called by any shipped script.

#### Properties and flags

- **GET_PROPERTY** (0x70DD4C..):
  - the scale is `GetScale` (vt +0x120, Object +0x50) on the thing as it is (0x70DD4C..0x70DD67, no Object cast; a
    script container has already answered 0 at 0x70DB27). A **creature**'s is `Creature::GetScale` 0x47B190 ✅:
    `CreaturePhysical::GetUserSize` 0x4EF4F0, the 3D body's (`[[creature+0x160]+0x58]`) +0x90, the size
    `LH3DCreature::SetSize` 0x480530 last got, kept to 0.05..4 (the pen's in the temple's pen;
    [The creature's radius](../creature/body.md#the-creatures-radius)). Its SET is `Creature::SetScale` 0x47B160:
    `SetSize(value)` on the 3D body (vt +8), then the physical's user size +0x6C = the value as given. So a SET is read
    back at once, clamped. CreatureGuide's CreatureDevGuideTeachesFight (ips 26732..26750) grows the guide by 0.1 a
    pass while its SCALE is below 1.9; CreateGuideCreature (23472) sets it to 2, HermitMain (57151) reads the
    player's creature's;
  - the speed is read at 0x70DE4E..0x70DF5F (the physics, the dead Living); a **weather thing** (`IsWeather`
    vt +0x3FC, 0x70DF03) gives fn_007745A0: its storm's (+0x78) +0x68, the metres a second it goes to its target, or
    0.0 without a storm ✅. SET SPEED (0x70E9F6) is `SetSpeedInMetres(value, 0)` (vt +0x134) on any thing;
    `WeatherThing::SetSpeedInMetres` 0x774580 writes the storm's +0x68 when it has one;
  - a **creature**'s speed ✅ (read in the exe; no oracle run): `GET` takes the same path, in the physics (+0x24 bit
    0x40) its physics object's velocity, else `IsLiving` (vt +0x3C4, `Living::IsLiving` 0x417460, 1) and
    `Creature::IsDead` (vt +0xAF4, 0x47CD70: the mind's (+0x164) action +0xF60 is 9 or 0x105) give 0, else
    `GetSpeedInMetres` (vt +0x130), which a creature does not override: `MobileWallHug::GetSpeedInMetres` 0x60C070,
    `ConvertWholeDistanceToMeters` of the u16 +0x5A. A creature's +0x5A is written only when it is made: the `Living`
    constructor 0x5EBEC0 calls `SetSpeed` (vt +0x864, `MobileWallHug::SetSpeed` 0x60FC50: below 0 is 0, above 0xFFFF
    is 0xFFFF) with its info's +0x104, the speed group's default (`Creature::Creature` 0x473B90 passes its
    `CreatureInfo`). A scan of every 16-bit store at +0x5A in the exe finds only `SetToZero` 0x60F767,
    `MobileWallHug::SetSpeed` and the editor (0x51F4D7); the other `SetSpeed` callers are villagers', animals' and
    puzzle pieces'. `SET` is `Creature::SetSpeedInMetres` 0x47D810: `LH3DCreature::SetRequiredSpeed` 0x47FA20 on its
    3D body, f = 0 when `!(v > 0)` (`test ah, 0x41`: -0 and NaN too), else `min(v, 1)` (the 1.0 at 0x8AA390); +0x483C =
    f and the follower's (+0x5198) +0x6403C = f × +0x4848 × 1.1 (0x8AB230), the speed it heads for. So SET asks for a
    fraction of the top speed and leaves what GET reads. CreateGuideCreature sets the guide's to 40.5 (ips 23476..23483)
    and TheStorm to 8 (24080..24083): both are all of it;
  - the age (0x70DFFD) is a Living's `GetAge` (vt +0x8D0) as an unsigned qword turned to a float, else "Not used on
    non living objects" (0xC209A8) and 0;
  - the **in hand** (IN_HAND, 9; jump-table entry 0x70DDFA..0x70DE2B) ✅: GameThingWithPos Flags +0x24 bit 4
    (`shr dl, 2; and 1`) pushed as a float, 1.0 or 0.0 (`fild` / `fstp`, VM type 2). The only code that sets the bit
    is the magic hand's add to the hand, fn_005FAFC0 (`or byte [esi+0x24], 4` at 0x5FB014, called from fn_005DC2A0 and
    fn_005DC330), which takes one thing (+0x14 count below 1); it is cleared by fn_005FB060 (0x5FB07C),
    `GMagicHand::RemoveFromHand` (0x5FB105) and `Object::ThrowObjectFromHand` (0x638645) (an operand scan of the whole
    code for `or [reg+0x24], 4` and `and [reg+0x24], 0xFFFB`). So it is a player's hand, never what a creature holds
    (IN_CREATURE_HAND asks that). bw1-decomp names the bit `UNAVAILABLE_FOR_STATE_CHANGE` (`GameThingWithPos.h`);
  - the **flying** (FLYING, 5; jump-table entry 0x70DCF8..0x70DD05) ✅: Flags +0x24 bit 0x40, IN_PHYSICS (`shr edx,
    6; and 1`), pushed as a bool (VM type 6, through 0x70E776) on the thing as it is, with no Object cast; a script
    container has already answered a float 0 at 0x70DB27. So a body come to rest, which the physics keeps to be knocked
    again but which has lost the bit, is not flying, and a thing a tornado or a vortex carries, or one left hanging where
    a hand let go of it, is (see [In physics or not](../physics/physics.md#manager)). The quests wait on it after a
    throw or a drop: the Pied Piper's PiperSetFree (`wait until Piper is not 5`), Throwing Stones' FocusFollow (the
    boulder come to rest), the Blind Woman's boulder, Fire on High's and Land 5's meteor, the Lost Brother's reunion,
    the gate stones' guards, the Sculptor's and the Hermit's waits: about 120 reads in 29 of the
    decompiled scripts (`refs/chl`, `is 5` / `is not 5`);
  - property **10** (openblack's `ObjectPropertyType::InHandGrabType`; jump-table entry 0x70DE2C..0x70DE49, the
    table 0x70E78C indexed by the property − 1 at 0x70DB6F) ✅: Flags +0x24 bit 0x10, the hand's locked-select flag
    that IS_LOCKED_INTERACTION reads (`shr cl, 4; and 1`), pushed as a float 1.0 or 0.0 (VM type 2, through
    0x70E776). The numbering is the enum's: PLAYER, the one the container test spares (0x70DB31, `cmp edi, 0x15`), is
    21 in both. openblack: `script::property_rules::InHandGrabTypeProperty` of `HandSystemInterface::IsInLockedSelect`
    (2026-10-24; before, it logged "not implemented" and gave an integer 0). See
    [the locked select](../hand/states.md#what-the-scripts-read-of-it);
  - **every property but PLAYER (21) of a script container** is a float 0 before the switch (`IsScriptContainer`
    vt +0x3F8, 0x70DB23..0x70DB58);
  - the **angles** (ANGLE, 2; XANGLE, 3; ZANGLE, 4; jump-table entries 0x70DC75, 0x70DBCD and 0x70DC21 in the table
    at 0x70E78C, by property − 1) ✅ [V, oracle on fake objects]:
    1. `dynamic_cast<Object*>` (0x9C7F50); none: "Object not an object" (0xC209DC), "Cannot Get Property %d" and a
       float 0 (0x70DCEE → 0x70E746..0x70E77D). A script container has already had its float 0 (above);
    2. the angle in radians times [0x8C6CAC] (0x3E22F983, 1 / 2π) times [0x92B380] (360.0), two float products at
       the game's 24-bit precision, pushed as a float (VM type 2), with no wrapping: 1.0 gives 57.295776
       (0x42652EE0), π (0x40490FDB) 180.0, 7.0 gives 401.07047 (0x43C88905), −7.5 gives −429.71832 (0xC3D6DBF2),
       0.017453292 gives 0.99999994 (0x3F7FFFFF). `property_rules::AngleToScript` is this product;
    3. XANGLE is `GetXAngle` (vt +0x504) and ZANGLE `GetZAngle` (vt +0x50C). `Object`'s (0x4024F0, 0x402510) give
       0.0; `MobileStatic`'s give the kept +0x80 / +0x84 (0x439610, 0x439620) and `MobileObject`'s +0x5C / +0x60
       (0x425B10, 0x425B20). A scan of every vtable: only the mobile statics (Rock, Bonfire, DeadTree, FelledTree,
       Fragment, MagicTeleport, the landscape vortices) and the mobile objects (Pot, PotStructure, the piles,
       MagicFood, MagicWood, PuzzleGrain, Ball, PBall, Arrow, Creed, FieldCrop, HanoiBlock, OneOffSpellSeed, Poo,
       PuzzleMobileObject, Reward, Scaffold, Whale) have their own pair; a creature, a villager, an animal, every
       building, a feature, a tree, a magic tree, an animated static, a big forest, flowers, a fish farm, a spell
       dispenser and a puzzle totem read 0. So the CreatureGuide's two reads of 4 on its creature (ips 27343, 27667,
       `> 50`) never hold;
    4. ANGLE: a scaffold (`IsScaffold` vt +0x49C, `Scaffold::IsScaffold` 0x55E100) reads its phantom building's
       angle +0x80 when its flags' bit 9 (+0x89 bit 2) is set, else 0.0, never its own Y angle (0x70DC96..0x70DCB7);
       anything else reads `GetYAngle` (vt +0x508), `Object`'s 0x402500 in every class: the float kept at +0x4C;
  - the **altitude** (YPOS, 24; 0x70E70A..0x70E717) ✅ is any thing's +0x1C, the `altitude` of its `MapCoords` Pos
    (+0x14 x, +0x18 z, both whole map units, +0x1C a float), pushed as it is. That altitude is the height **above the
    land** under the point, not the world height: SET_POSITION stores y − `GetAltitude` (above), GET_POSITION and the
    matrices draw `GetAltitude(pos)` + it. XPOS (23, 0x70E6E2) and ZPOS (25, 0x70E719) are `fild` +0x14 / +0x18 ×
    10/65536 (0x8AA3A4);
  - the **player** (PLAYER, 21; jump-table entry 0x70E62D..0x70E6E1, answered for a script container too: the
    container test at 0x70DB31 lets 21 through) ✅ [V]:
    1. `player = thing->GetPlayer()` (vt +0x1C);
    2. if the thing is a town (vt +0x1B8 `IsTown`) and `Town::IsCompletelyDestroyed` 0x740E10 is true, a float 1.0
       (0x70E64F), whoever owned it;
    3. else if `player` is NULL or `&g_game->players[NeutralPlayerIndex]` (0x70E676..0x70E696), a float 0.0
       (0x70E6BF);
    4. else `(float)(GPlayer::GetPlayerNumber() + 1)` (0x64A790, `inc eax`, `fild qword`, `fstp dword`,
       0x70E69A..0x70E6B6): 1.0 for player one, 2.0 for player two and so on.
    - The VM type is 2 (float) on every path.
    - `GetPlayer` per class (bw1-decomp): a town's is `Container::GetPlayer`, its owner (`Container.h`); a
      villager's is its town's owner, NULL without a town (`Villager.cpp`); a creature's is its `owner`
      (`Creature.cpp`); an abode's is its town's player, else the neutral one (`Abode.cpp`); anything
      without an override is the neutral player (`GameThing.cpp`). About 20 other classes override it (fields,
      features, spells, rewards, artefacts and others).
    - `Town::IsCompletelyDestroyed` (`Town.cpp`) is true unless some abode of the town has
      `GetLife() > 0 && !IsField() && (IsBuilt() || GetPercentBuilt() > 0.1)`; a town without abodes is destroyed.
    - Land 1's food lesson (`GuideImpressTownNextLesson`) waits on `21 of TheAztecTown == 1.0`; Land 2's
      `LandControl2`, `KhazarAI`, `UpdateLethysTownTotal`, `TownOwnershipCheck` and `TheWorkshop` read it too
      (the disassembly of `challenge.chl`).
  - the **strength** (STRENGTH, 17; jump-table entry 0x70E081..0x70E1A8) ✅ [read in the v1.42 exe, 2026-10-28], the
    first that holds, each pushed as a float (VM type 2), with no clamp:
    1. a creature (the `dynamic_cast<Creature*>` made before the switch): its physical's (+0x160) +0xC, the strength;
    2. a one-shot miracle orb (`CastOneOffSpellSeed` vt +0xAC): its +0x6C, 1.0 from `OneOffSpellSeed::SetToZero`
       0x72A40A, which `InterfaceTap` 0x72A649 passes to `CreateSpellIntoHand` 0x72A730 as the new seed's multiplier;
    3. a miracle in a hand (`IsSpellSeed` vt +0x4C4, `SpellSeed::IsSpellSeed` 0x727F50 returns itself): its +0x88,
       the cast multiplier its charge and its time are multiplied by when it is cast;
    4. a special effect (`IsParticleContainer` vt +0x4AC): the container's +0x34, the strength it gives its effect
       ([Particles](../physics/particles.md));
    5. a cast miracle (`IsSpell` vt +0x400, every `Spell` class): its +0xE4, the multiplier of the strength its
       prayer power gives ([Spell core](../magic/spell-core.md));
    6. anything else: "Script-Did you want the strength of this?" (0xC2097C) and its life, `GetLife` (vt +0x11C):
       an Object's +0x48 (`Object::GetLife` 0x402600), a pile's (`PileResource::GetLife` 0x66E3D0), and 1.0 for a
       thing that is no Object (`GameThingWithPos::GetLife` 0x4052A0: a forest, a mist, an influence ring; a scan
       of every vtable's +0x11C slot). A town, a flock or a dance has already read 0 (above).
    - Shipped uses on anything but a creature: Land 2's PhysicalShieldChallenge sets 3 on its three one-shot shield
      orbs (`Land2ShieldChallenge.txt:98-191`), the shield hand demo (`PShield`) sets 7 on the shield it puts in the
      hand (`HandDemos.txt:117`), and Land 4's meteorites set 2 on a cast fireball (`land4meteorites.txt:775`). No
      shipped script reads or sets a special effect's.
    - openblack: `GetProperty` in `src/CHLApi.cpp` through `script::property_rules::StrengthKindOf` and
      `LifeInPlaceOfStrength`: `OneOffSpellSeed::scale`, `SpellSeed::castMultiplier`,
      `ParticleSystemInterface::GetSpotVisualStrength` and `Spell::strengthMultiplier` (2026-10-28; before, anything
      but a creature logged "not implemented" and read 0). Tests
      `ScriptProperties.StrengthIsAskedOfACreatureThenAnOrbAHeldMiracleAnEffectAndACastMiracle`,
      `AThingWithNoStrengthReadsItsLifeAndOneWhenItIsNoObject`, `ParticleSystemTest.ASpotVisualsStrengthIsReadFromItsContainer`;
- **THING_VALID** (256, `GScript::IsThingValid` 0x6FD840, read in the v1.42 exe 2026-10-27): POP the object; 0, or no
  thing from `GetScriptGameThing` 0x70D220 (none for a thing being deleted or a villager on its way to dying, vt +0x2C
  `IsAvailable`, [the thing a native is given](#the-thing-a-native-is-given); "Check-Deleted thing still accessed" for
  one gone), pushes false; a town (vt +0x1B8 `IsTown`) pushes `!IsCompletelyDestroyed` (above), whether or not it has
  been deleted; any other thing true. The VM type is 6 (bool). Land 2's impress-the-village lesson reads its town's
  this way (refs/chl/LearnGestures.txt:100756). openblack: `ThingValid` in `src/CHLApi.cpp` through
  `script::property_rules::ThingValid` (2026-10-27; before, it asked only whether the entity was there; until
  2026-10-28 a villager on its way to dying was valid). Tests
  `ScriptProperties.AThingIsValidWhileReachableAndATownWhileNotWhollyDestroyed`,
  `TownDeletionTest.AScriptSeesATownAsValidWhileOneOfItsBuildingsStands`.
- **SET_PROPERTY**:
  - the **health** (HEALTH, 1; 0x70E867..0x70E8A0) ✅: `dynamic_cast<Object*>` (0x9C7F50), none: "Object not an
    object" (0xC209DC, 0x70E9EC) and nothing changes; an Object gets `SetLife(value)` (vt +0x5B0). `Object::SetLife`
    0x63A140 writes +0x48 = the value, except that a thing in a script (+0x24 bit 0x200) while a script holds the
    widescreen (HelpSystem `g_game+0x25005C` +0x45E8 and +0x45EC both set), or an indestructible one (+0x24 bit
    0x4000), keeps its life when the value is not above 0.01 (0x8C5840; `test ah, 0x41`, so a NaN is kept too).
    `Villager::SetLife` 0x756B40 first counts the town's wounded: from above 0.7 (0x8AB238) to below it
    `IncreaseNumWoundedInTown` 0x756BC0, from below to above `DecreaseNumWoundedInTown` 0x756BD0, both strict, and
    then `Object::SetLife`, so the count moves even when the life is kept. The other overrides: `PileResource`
    0x66E400 (the piles, magic food and wood, puzzle grain: an amount from the life), `Tree` 0x74A1E0 and `FieldCrop`
    0x6081E0 (at 0 or less the thing is deleted, vt +0xC, else `Object::SetLife`; a field crop then rescales). A
    totem statue goes to its own setter first ([The village totem](../magic/village-totem.md));
  - the scale is `SetScale` (vt +0x124, `Object::SetScale` 0x639200: +0x50 and the matrix when it changes,
    0x70EA76);
  - the **angles** (ANGLE 0x70E8A1, XANGLE 0x70E92E, ZANGLE 0x70E98F) ✅ [V, oracle on fake objects]: the Object
    cast as GET's (none: "Object not an object", 0x70E9EC); the value times [0x92B20C] (0x3C8EFA35, 0.017453292)
    as a float, with no wrapping (160 gives 2.7925267, 0x4032B8C2; 720.5 gives 12.575097);
    `property_rules::AngleFromScript` is this product:
    - ANGLE on a scaffold (vt +0x49C): +0x80 = the radians and +0x89 |= 2 (flags bit 9: the phantom building no
      longer turns by itself in the hand, and keeps this angle when the plan changes), its Y angle untouched
      (0x70E8DF..0x70E8EB). That is the only writer of bit 9 ([Building sites](../world/building-sites.md));
    - ANGLE on anything else: nothing when `GetYAngle() == radians` (0x70E905), else `SetYAngle(radians)`
      (vt +0x524). `Object::SetYAngle` 0x639260 tests it again and calls `SetXYZAngles(GetXAngle(), radians,
      GetZAngle())` (vt +0x514); `MobileWallHug::SetYAngle` 0x60DAC0 (every Living, the creature too) does that and
      then keeps `ConvertAngle3DToGame(radians)` (0x74DC30) as its game angle at +0x5C;
    - XANGLE is `SetXYZAngles(radians, GetYAngle(), GetZAngle())`, ZANGLE `SetXYZAngles(GetXAngle(), GetYAngle(),
      radians)`;
    - `SetXYZAngles(x, y, z)`: `Object`'s 0x638D00 keeps no x or z: out of its map cells when it is in them
      (`IsObjectInMap` vt +0x178, `RemoveMapObject` vt +0x548), `SetYJustAngle(y)` (vt +0x520, 0x63A800: +0x4C)
      and, with a Game3DObject (+0x40), its matrix built again from `GetScale` and `GetYAngle` at (x / 6553.6,
      `GetAltitude` + altitude, z / 6553.6) in SET_POSITION's four branches, then back into its cells
      (`InsertMapObject` vt +0x544). `MobileStatic`'s 0x608CE0 and `MobileObject`'s 0x607460: out of the cells,
      +0x80 / +0x5C = x, `SetYJustAngle(y)`, +0x84 / +0x60 = z, with a Game3DObject
      `Game3DObject::SetPosition(pos, x, y, z, scale)` 0x63B680 (`SetYXZMatrixOnly(y, x, z)` 0x7FAC10, each cell
      times the scale, at the same place), then back into the cells;
    - the oracle on fake objects: XANGLE 0 on a rock at x 0.5, y 1, z 0.25 leaves +0x80 = 0, +0x4C = 1,
      +0x84 = 0.25; ZANGLE 0 leaves x 0.5 and z 0; ANGLE 90 on a rock sets +0x4C = 1.5707964 (0x3FC90FDB) and keeps
      x and z; XANGLE 45 on a feature changes none of the three;
    - **a creature** ✅ [V, oracle on a fake creature]: the Creature vtable (0x8CC810) has `Object`'s `GetXAngle`,
      `GetYAngle`, `GetZAngle`, `SetXYZAngles`, `SetXYZAnglesAndScale` and `SetYJustAngle` (vt +0x504, +0x508,
      +0x50C, +0x514, +0x518, +0x520) and `MobileWallHug::SetYAngle` (vt +0x524), the same as a villager's and an
      animal's. So GET ANGLE is its +0x4C, XANGLE and ZANGLE 0 (+0x80 / +0x84 are not read); SET ANGLE, when the angle
      differs, writes +0x4C and the game angle +0x5C (`ConvertAngle3DToGame`), takes it out of its cells and back;
      SET XANGLE or ZANGLE only takes it out of its cells and back (+0x4C as it was). The oracle
      (`oracle_creature_angles.py`): +0x4C of 1.0 reads 57.295776 (0x42652EE0), 7.0 reads 401.07047; SET ANGLE 90 on
      +0x4C 1.0 and game angle 7 gives +0x4C 1.5707964 (0x3FC90FDB) and game angle 512, 160 gives 2.7925267
      (0x4032B8C2) and 910, −45 gives −0.7853982 and 1792, 720.5 gives 12.575097 and 2; SET ANGLE 57.29578 (1.0 in
      radians) on +0x4C 1.0 changes nothing; XANGLE or ZANGLE 45 leave +0x4C 1.0, +0x5C 7, +0x80 0.5 and +0x84 0.25.
      That Y angle is not where the creature faces: `Object`'s constructor sets +0x4C to 0 (0x6365B4),
      `Creature::Create` 0x474A20 takes no angle, and no Creature, Living or MobileWallHug code writes +0x4C but
      through the setters (an operand scan of their functions; the one hit, 0x4FF166, writes another object). Of the
      setters' callers (vt +0x514, +0x518, +0x520, +0x524 and `MobileWallHug::SetGameAngle` 0x60DA90) none was found
      to run for a creature: `Creature::SetFocus` 0x4F6760 (vt +0x510) queues a look-at sub action instead of turning,
      the wall-hug walks, the Living dances and reactions and `Living::MoveAlongPath` (vt +0x52C) that set a game
      angle run in the villagers' and animals' states (inferred: the creature walks by its own route follower), and
      creation sets the angles it already has. Its body faces by its LH3DCreature (`Creature::GetCreature3D`, +0x160 → +0x58), whose draw object
      `Creature::Draw` 0x517910 puts in +0x40 every frame (0x517AD1..0x517AE5), so the matrix `SetXYZAngles` would
      build there is (inferred) the body's again at the next draw. The game angle +0x5C is read by `Creature::Initialise`
      (0x474373, CreaturePhysical's start angle) as the creature is made, before any script; no later reader of a
      creature's was found;
    - the kept angles are not the matrix: SET_POSITION and YPOS build the matrix from the Y angle alone and leave
      +0x80 / +0x84, so a later SET of one lean brings the other's old lean back;
    - in the story: The Sculptor sets XANGLE and ZANGLE of QuarryRock to 0 (ips 35345..35356 and 36828..36839) and
      reads its ANGLE (36906) for the gate stone it makes turned half round from it; Land 2's CreateWorkshop
      (77310..77313, 160), GetThroughVortexL2 (110787, 110815) and Land 5's SetupLand5 (140918, 140942) set a
      scaffold's ANGLE; SwapToCow and CreateSlaversDen add 90 to a fence's ANGLE after SET_FOCUS;
  - the age (0x70EB65) is a Living's `SetAge(ftol(val))` (vt +0x8D4: `Villager::SetAge` 0x7528C0 changes the meshes
    when the grown-up age is crossed and sets the scale for the age, `Animal::SetAge` 0x4179C0);
  - the built share of a MultiMapFixed goes to `fn_0052EDD0` (0x70EC69);
  - the **strength** (STRENGTH, 17; jump-table entry 0x70EBA6..0x70EC64) ✅ [read in the v1.42 exe, 2026-10-28]: the
    same order as GET's, each written as given, with no clamp: a creature (`CastCreature` vt +0xA4) its physical's
    strength (0x4F0100, +0xC); a one-shot orb +0x6C; a miracle in a hand +0x88; a special effect
    `GParticleContainer::SetParticleStrength` 0x63E3E0 (0x70EC29); a cast miracle +0xE4; anything else "Cannot set
    strength of this" (0xC20A28) and then "Cannot Set Property %d" (0x70F28C falls into 0x70F294), nothing changed.
    openblack: `SetNonCreatureStrength` in `src/CHLApi.cpp`, through `script::property_rules::StrengthKindOf`;
  - the **altitude** (YPOS, 24; 0x70EF9B..0x70F1C4) ✅: a script highlight (vt +0x48C) gets its draw height
    (`SetDrawHeight` 0x709C40); then, for any thing, +0x1C = the value (0x70EFBF). An Object (vt +0x460) with a
    Game3DObject (+0x40) has its matrix built again at (x / 6553.6, `LH3DIsland::GetAltitude(Pos)` 0x803090 + the
    value, z / 6553.6) from `GetScale` (vt +0x120) and `GetYAngle` (vt +0x508) alone, in the same four branches as
    SET_POSITION (angle 0 / scale 1), and +0x44 / +0x48 = scale / angle: a tilt about x or z is lost. Neither the
    map cell nor the turn-start MapCoords (+0x2C) is touched. A script container hands every property to its members
    (below);
  - **SET_PROPERTY's own frame** (`GScript::SetProperty` 0x70F380): pops the value, the thing, the property; no thing
    is "Object no longer valid" (0xC0D428); a script container (vt +0x3F8: a town, a flock or a dance) stores the
    property and the value at g_game +0x250090 +0x20 / +0x24 and runs its type's loop function (table 0xC0C73C by
    vt +0x4E8) with the worker sub_70F350, so each member gets the same SET; anything else goes to the worker at
    0x70E820 (jump table 0x70F2BC by property − 1);
  - anything else is "Cannot Set Property %d" (0x70F2CC..0x70F2D4 → 0x70F294) and nothing changes.
- **OVERRIDE_STATE_ANIMATION** (`GScript::OverrideStateAnimation` 0x6F9F50) pops the clip, then the thing
  (0x6F9F78). A clip ≤ 0 or ≥ 441 logs "Invalid animation forced" (0xC0D69C) and goes on (0x6F9F80..0x6F9F93); no
  thing is silent (0x6F9F9D); not a Living is refused (0x6F9FC0). The clip's AnimPack entry 0 (0x6F9FD3..0x6F9FE7)
  replaces the 3D object's animation when it differs (vt +0x184 / +0x180, cycle time 0 by vt +0x188,
  0x6F9FEA..0x6FA00D); it also writes the Living's `DataForScriptRemind` (+0x3C = clip, +0x44 = 0, from 0x6FA013),
  read only by `EnterPlayAnim`'s resume (0x768914). The clip lasts until the next `Living::SetStateAnim` 0x5ECB10.
  On a creature the 3D object is its dynamic shadow, a complex object whose animation slots are the base's empty ones
  (+0x180 and +0x188 a bare return, +0x184 returns none), so the clip changes nothing; only the remind record is
  written, which nothing of a creature reads. openblack does nothing for a creature, and says nothing (test
  `CreatureOverrideAnimNativeTest`); no shipped script calls it on a creature.
- **SET_ID_MOVEABLE** (`GScript::SetIdMoveable` 0x6FB3E0): the thing, then the bool; no thing is "Thing not valid"
  (0x6FB411); Flags +0x24 bit 0x1000 = (moveable == 0) (0x6FB421..0x6FB43C).
- **SET_INDESTRUCTABLE** ✅ (`GScript::SetIndestructable` 0x6FDE20, frozen v1.42 exe): the thing (first POP, then
  `GetScriptGameThing`), then the flag; no thing is "Thing not valid" (0x6FDE56). A script container (vt +0x3F8:
  town, dance, flock) hands it to its type's loop function (table 0xC0C73C, stride 0x18, by `GetScriptObjectType`)
  with `SetIndestructableLoopFn` 0x6FDDF0, which sets or clears each member's Flags +0x24 bit 0x4000 (flag & 1, via
  the script's LoopParam) and returns 0 (the walk goes on); the container's own flag is not touched, and a type with
  no loop function does nothing, with no message. Anything else sets or clears its own bit (0x6FDEB3). The shipped
  scripts use it on villagers, creatures, animals, trees (CreatureDevelopment's LeashTree), statics, totems and one
  town (land4meteorites' UndeadTown, L4, both ways). openblack: `SetIndestructable` in `CHLApi.cpp`, a container's
  members through `script_containers::ForEachMember` (test `FlockNatives.SetIndestructableMarksEachMemberOfATownOrAFlockAndAnyOtherThingItself`).
  Every reader of the bit in the frozen exe (a sweep of every `test byte [reg+0x25], 0x40`, `test [reg+0x24],
  0x4000`, and the loads of +0x24 tested with 0x40 in the high byte or shifted by 14), and openblack's place for it:
  - `Object::SetLife` 0x63A140 (0x63A169): a life of 0.01 or less is refused (as for a thing in a script while a
    script owns the widescreen bars); `Villager::SetLife` and every class without its own setter go through it, a
    tree's and a field crop's setter delete the thing at a life of 0 or less first, and a pile's never calls it.
    openblack: `life::SetLife` and `life::SetLifeUnlessProtected` (`life::KeepsLife`), wherever a life is set.
  - `Object::CanBeDestroyedBySpell` 0x639960 (0x639972): never. openblack: `explosion::CanBeDestroyedBySpell`.
  - `Villager::FindTreeNearVillager` 0x75FD6B: a forester passes over such a tree. openblack: `ecs::FindTreeNearVillager`.
  - `Villager::Drowning` 0x76A783: the count is held at 10. openblack: `VillagerDrowning.cpp`.
  - The prey test fn_004196D0 (0x4196F4): never prey. `Living::BeingEaten` 0x5EC4E0 and `Villager::BeingEaten`
    0x76B391: one caught gets up (state 11) instead of dying. openblack: `script_held::CannotBeEaten` in
    `AnimalPredators.cpp`.
  - `Animal::ApplyThisToObject` 0x41B37D and `MobileStatic::ApplyThisToObject` 0x608CB3 (an animal or a fence given
    to a store) and `MobileStatic::ReactToPhysicsImpact` 0x608FD1 (a thrown fence): not taken. openblack:
    `held_apply` (`HeldApply.cpp`) and `DynamicsSystem.cpp`.
  - `Tree::ApplyThisToObject` 0x74BDD4 and `Villager::ApplyThisToObject` 0x752C93 (a sacrifice at a worship site's
    totem): refused. openblack: `AltarOffering.cpp`.
  - The vortex's take fn_005FD9B0 (0x5FDA11): the record keeps the bit. openblack: `Vortex.cpp`.
  - The creature's meal sub_500540 (0x500540): spat out uneaten (desire 4 held back 20 s, the puke action, let go
    with no speed), as a thing in a script while a script owns the widescreen bars. openblack:
    `CreatureObjectActionSystem::EndMeal` (`creature_eating::RefusesMeal`).
  - `LH3DCreature::CanKnockObject` 0x481B20 (the creature's walk knocking things over) and
    `Object::CanBeKickedByCreature` 0x4E43C1 (what it kicks out of its way): not knocked, not kicked. openblack:
    neither choice is ported.
  Writers: this native and its loop, `GameOSFile::LoadInstance` 0x559AA0 / 0x559AED (a saved thing),
  `PuzzleMobileObject` 0x561AAF and the puzzle makers 0x6DA550..0x6DD3BB, the arriving vortex fn_005FE3B0
  (0x5FE5DD) and `LandscapeVortexOut::ToBeDeleted` 0x5FDF3B (clears it). The 1.20 `Object::GetDamageEffect`
  test (bw1-decomp Object.cpp) is not in the frozen exe's 0x637D00.
- **SEX_IS_MALE** 358 ✅ (`GScript::SexIsMale` 0x6FEA00, frozen v1.42 exe) pops the thing; a villager (`IsVillager`,
  vt +0x2C8 with 0: Villager and SpecialVillager) pushes `info [+0x28] +0x1F8 == 0` (`GVillagerInfo` sex, MALE 0;
  `sete` 0x6FEA3E) as a bool (type 6). No thing, or any thing that is not a villager, pushes **true** (0x6FEA52), with
  no message. Only the info's sex is read, nothing kept on the villager. Its one caller is `TheMissionaries`
  (L1376, ip 64541, on its helper villager, to pick the female or male line). openblack: `SexIsMale` in `CHLApi.cpp`
  reads the villager's info row (`ecs::villager::InfoOf`, the row of its tribe and villager number), not
  `Villager::sex` (`test/test_script_query_natives.cpp`, `SexIsMale*`).

#### Timers

- **The countdown timer**: `GScript::StartCountDownTimer` 0x711150 → `InitialiseCountDownTimer` 0x6EB8B0;
  `GScript::RemoveCountDownTimer` 0x711180 clears GScript +8, which `GScript::CountDownTimerExists` 0x7111D0 reads;
  `GScript::GetCountDownTimer` 0x7111A0 gives the whole seconds left (`GetCountDownTimerRemainingTime` 0x6EB950);
  `GScript::HideCountDownTimer` 0x7111F0 / `GScript::RevealCountDownTimer` 0x711210 set GScript +0x10 to 0 / 1. The
  scripts' restart (`GScript::Reset`) sets +8 and +0x10 to 0 and leaves the turns +0xC; a land change writes none
  ([the scripts' restart's other writes](land-change-and-saving.md#the-scripts-restarts-other-writes)); openblack:
  `script_countdown::OnScriptsRestart` from `Game::ResetScripts`.
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
- **Speaking.** `GScript::SpiritSpeaks` 0x710C40 pops the text (0 from 6974 on, `cmp esi, 0x1B3E` at 0x710C6E), then
  the spirit, and pushes `HelpSystem::GetSpiritWhoTalks` 0x5C6E20 == the spirit, as a bool (6). The spirit comes from
  `ConvertScriptSpiritToHelpSpirit` 0x710350 with the local player's alignment
  ([voices](../audio/voices.md#how-it-works)); openblack: `SpiritSpeaks` in `src/CHLApi.cpp`, the same in every branch.

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
- **KEY_DOWN 419.** ✅ `GScript::ScriptKeyDown` 0x70B9F0 pops the key (raw) and pushes, as a bool, the byte
  `LHSys::TheSystem.keyboard.KeyState[key]` (0xE85374 = 0xE85040 +0x32C +8, at 0x70BA03), with no bound check. The
  window's key messages write it (`LHKeyboard::ProcessKeyboard` 0x7DCAF0): 1 on a press and 0 on a release, at the
  message's scan code, plus 0x80 for an extended key. So the key numbers are the PC keyboard's scan codes, as the
  script compiler's `LH_KEY` enum (`chlasm/LHKeyBoard.h`) names them: 0x39 the space bar, 0x9C the keypad's Enter,
  0xC8 the up arrow. The shipped `challenge.chl` calls it 13 times, always with 57 (`PUSHF 57`, `CASTI`), the space
  bar: 12 in the Gods' Playground (`LandControlT` and its `KeepRunning…` lessons, to run a lesson again) and once in
  Land 5's `VillageWavingSequence`, whose last scene holds until it is pressed.
- **The force-feedback mouse.** IMMERSION_EXISTS (`GScript::ImmersionExists` 0x6FDFA0, table entry written at
  0x705B43) pushes whether the Immersion mouse pointer [0xD18588] is set (0x6FDFAB..0x6FDFBB, `setne`). Only
  `GInterface::SetupStatics` 0x5CE3F0 sets it up: it clears it, then 0x5D86A0 makes a `CImmMouse`, stores it at
  0x5D86FD and calls `CImmMouse::Initialize`; when that fails (no such mouse) or the effects file does not open, the
  mouse is deleted and the pointer cleared again (0x5D88A7..0x5D88DE) [V, runblack.exe v1.42, read 2026-10-19]. On a
  machine without the mouse it is therefore false. Land 1's MagicMushroom tests it right after its locals (which
  make its three markers and find the hippy's hut) (challenge.chl ip 67208, `JZ` to 67861, the script's end), so
  without the mouse the mushrooms near the puzzle's spot are not removed, the mushroom puzzle (`CREATE_WITH_ANGLE_AND_SCALE` at ip 67284) is never made and its `PLAYED` wait (ips 67530..67532) is
  never reached; the script ends at once. openblack's `ImmersionExists` answers false, the same.

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
- **GET_TOWN_AND_VILLAGER_HEALTH_TOTAL** 443 (`GScript::GetTownAndVillagerHealthTotal` 0x6FFA90) pops the thing; a
  town (`IsTown`, vt +0x1B8) gives `Town::GetTownAndVillagerHealthTotal` 0x73E670 as a float, anything else "Invalid
  objects" (0xC0DB84) and 0. The sum starts at 0; for each abode of the town's list (+0x754, next +0x9C) it adds
  `Abode::GetVillagerHealthTotal` 0x4076C0 (its villagers +0xA0, next +0xE4, each one's `GetLife`, vt +0x11C, summed
  from 0 on their own) and then the abode's `GetLife` (`Object::GetLife` 0x402600, +0x48); then each homeless
  villager's life (+0x768, next +0xE4). Every step rounds to a float. Its one script is Land 5's Heavenly Fire
  (`fireonhigh.txt`), before and after the meteors.
- `GScript::GetAlignment` 0x6F9A60 pushes the player's alignment (`GPlayer::GetAlignmentValue` 0x64D6A0).
- `GScript::RandomULONG` 0x6F8E20 pops max (0x6F8E2E), then min (0x6F8E40), and pushes `GameRand(max − min + 1)` +
  min on the synchronised stream as an int (0x6F8E4C..0x6F8E68; unsigned, 0 for a range of 0).
- ✅ **SQUARE_ROOT** 397 (`GScript::SquareRoot` 0x6FF4A0) pops the value and pushes `fsqrt` of it as a float (type 2),
  with no test on the sign: a negative value (minus infinity too) gives the processor's default not-a-number (bits
  0xFFC00000, exceptions masked) and minus nought gives minus nought. Not called by the shipped scripts. openblack the
  same since 2026-10-26 (`SquareRoot` in `src/CHLApi.cpp`, test `ScriptQueryNatives.SquareRootIsTheFloatRootOfAnyValue`;
  before, nought for a value of nought or less).

#### GET_TARGET_RELATIVE_POS 136

✅ `GScript::GetTargetRelativePos` 0x6FAEF0 (*get target from ‹from› to ‹to› distance ‹distance› angle ‹angle›*, 8 in,
3 out) pops the angle in degrees, the distance, the second point (z, y, x) and the first (z, y, x), all as floats, and
pushes a point (`POINT_PUSH` 0x6FB100: x, y, z as coordinates). From the frozen exe's code (0x6FAF84..0x6FB0E9), every
step at the FPU's 24-bit precision:

- the direction is second − first, each part stored to a float; it is made one long only when a part compares other
  than nought (`fcomp` with 0 and `test ah, 0x40`, so a not-a-number part counts as nought): the inverse is
  1 / sqrt((z·z + y·y) + x·x), stored to a float, and x·inv, y·inv, z·inv. With no direction (the same point twice) it
  stays nought and the answer is the second point;
- the angle times the float 0.0174532924 (0x92B20C) is the radians; `fcos` is stored to a float, `fsin` stays
  unrounded on the stack (the 1.20 C's `LHMatrix::SetRotationY` and `TransformPoint`, inlined);
- the turned direction is x' = (−sin·z) + (cos·x), stored, and z' = (sin·x) + (cos·z), kept on the stack; y is not
  turned. A positive angle turns +x towards +z and +z towards −x;
- the answer is (x'·distance) + to.x and (y·distance) + to.y, each product stored first, and (z'·distance) + to.z.

The oracle (the native run with its eight pops fed from a table and the push captured) gives the bits of 18 cases,
pinned in `test_script_target_relative`. Its 10 calls in 7 scripts: FreeTheCreature (10 past the statue, straight on),
land4ogre (50 past the gremlin), landcontrol5's FindCirclePointDust and ThrowBeamBlock (the beams at 0 and ±90°),
swaptocow's CattleReactions, and thebigfight's FindCirclePoint and FindCirclePointWE (points round a circle).

### Virtual machine CAST

In the original `ScriptLibraryR.dll` (`Plug Ins`), opcode 23 INTCAST (0x10008EE0, the table 0x100090E4 by type − 1)
**converts**:

- CASTI reads the bits as a float and does `__ftol` (0x1001568C, truncating; the low word of the 64-bit integer);
- CASTF reads the bits as an unsigned 32-bit integer (`fild qword` with the high part 0);
- CASTV, CASTO and CASTB only change the type, and type 5 does nothing.

### Virtual machine EQ and NEQ

In the original `ScriptLibraryR.dll`, the opcode table at 0x10021610 sends opcode 14 EQ to 0x10009900 and 15 NEQ to
0x100099C0; both switch on the instruction's type (read at +8):

- types 1 (int) and 6 (boolean): two POPs (0x10008B00), the integers compared (`sete`), PUSH (0x10008AC0) of the
  result with type 6;
- type 2 (float): two POPs, `fcomp` and `test ah, 0x40` (C3), so a pair that can't be ordered (a NaN) counts as equal;
- any other type (vector, object, none, 5, 7): no POP and no error, PUSH 0 with type 6 (0x1000991A).

NEQ is EQ's same branches followed by a POP of that result and a PUSH of its negation (0x10009A70), so on any other
type it gives true. openblack: `LHVM::Opcode14Eq` / `Opcode15Neq` (test `LhvmCompare.*`); before, it also compared
vectors and objects, and on any other type took two values, pushed false for both and signalled an invalid type. Every
comparison in the shipped challenge file is on floats, so no shipped script reached the difference.

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
  count (one a game turn outside the citadel) × 0.1f: the 64-bit integer load of the count times the float 0.1f at
  0x1001F140, stored as a float (confirmed: data file, the DLL's bytes read with capstone).
- `GScript::ScriptErrorMessage` 0x6F62B0 and `ScriptWarningMessage` 0x6F62C0 are a bare `ret`: the original prints
  none of its script messages.

### The challenge file (.chl)

✅ `Scripts\Quests\challenge.chl` (3 234 236 bytes) read from its header to its last byte with this layout, all
little-endian u32 unless marked (the layout was first given by the openblack project wiki's Challenge File page):

1. `"LHVM"`, then the version: 7 (the Creature Isle files under `CreatureIsle\Scripts` are 8, a different layout).
2. The globals: a count (398), then that many zero-terminated names.
3. The code: a count (157 160), then instructions of 5 u32 each: opcode, access/mode, data type, data, line.
4. The auto-start scripts: a count (1), then script ids.
5. The scripts: a count (514), then per script its name and file name (zero-terminated), its type (1, 2, 4, 8, 16
   or 64 here), its variable offset, its local variable names (a count, then names), its first instruction, its
   parameter count and its id.
6. The data block: a size (10 855), then that many bytes; it ends exactly at the end of the file.

Two claims on that wiki's LHVM page are wrong, and a third that looks wrong is right:
- The access field is 0 (immediate, or backward) or 1 (reference, or forward), not 1/2: the file's 157 160
  instructions hold only 0 and 1.
- Opcode 1 is a conditional jump (JZ), not WAIT: `ScriptLibraryR.dll`'s handler (0x10009F60, entry 1 of the table
  at 0x10021610) pops one value; non-zero goes on to the next instruction, zero jumps to the data (mode 0: there,
  and the task yields; mode 1: data − 1). Read with capstone.
- 🔍 Comparisons with a type they don't handle (EQ/NEQ handle int, float and boolean; GEQ/LEQ/GT/LT int and float)
  pop **nothing** and push false, as that wiki says (EQ 0x1000991A: `push 6; push 0; call push` with no pop before).
  Every comparison in the shipped `challenge.chl` is of type float, so the branch never runs in the game.

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
   pushes `!IsPlayBack(0)` 0x6FDB9A as a boolean, confirmed: xref immediates, so it waits for the demo to end). `drag.hnd` plays its 590 records and text 5219
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
  - animals (`animal_ai::ScriptMoveTo`): only while the animal is in the map's cells (`map_cells::IsObjectInMap`, the
    object's own in-map flag, which the hand, the physics, a storm's carry and a step off the land clear, as for the
    villagers) and not drowning; already there is `walk_arrival::AreWeThere` on whole units with its speed and 0,
    strictly, as `MobileWallHug::AreWeThere` 0x60AD60 (the Animal vtable's +0x85C) → the script state 4, else
    `SetupMoveToPos(pos, IN_SCRIPT)`. Tested by
    `AnimalBehaviourTest.AScriptsMoveTakesAnAnimalInTheMapAndMeasuresItsArrivalInWholeUnits`. Before, "in the map" was
    "not in the hand" (an animal thrown or carried was sent walking) and the arrival was metres and `<=`. The walk it
    then sets up keeps the animals' own units ([Walking round obstacles](../beings/wall-hugging.md)), and its goal
    keeps the point's height above the land (+0x88, `AnimalBrain::goalAltitude`; see GET_POSITION below);
  - a flock (`flocks::SetDomainCentrePos`) sets its tail's destination (a villager's `WallHug::goal`, an animal's
    `AnimalBrain::goal`) and `Flock::domainCentre`. The tail is `Flock::members.front()`: the list is the original's
    reversed, kept in the original's order by `AddMember` / `AddLeader`, and a member that goes leaves it at once
    (`RemoveLiving` from the animals' and the villagers' deletion, as `RemoveLivingFromFlock`), so `flocks::Leader`'s
    first available member from the tail is the original's tail (+0x40 → +8). The original takes the tail with no
    test of it but that it is there (`SetDomainCentrePos` 0x52FC20); openblack's skip of a member that is not available
    never skips one, as `ecs::ToBeDeleted` takes a villager or an animal out of its flock (`animal_ai::Forget`) before
    it marks it unavailable (checked 2026-10-29);
  - a weather thing's storm, while it has one, takes the point as its target (`weather_thing::SetTarget`), and
    drifts there at the speed SPEED sets; the thing follows its storm each turn. Tested by
    `WeatherTest.AScriptMovesAWeatherThingsStormTowardsThePoint`;
  - there are no computer players;
  - anything else: `map_cells::SetPositionKeepingCells` gives the Transform the point and leaves the thing in the
    cells it was filed in, with its collide data, until it is next taken out or put in (Sync does not refile it while
    it stands there); its model keeps its own placement (`object_placement::MoveWithoutModel`,
    [the object's own placement](../engine/matrices-and-zoomer.md#the-objects-own-placement)), so it is drawn, picked
    and tested on the screen at the old place as in the original, a vanilla bug reproduced
    ([physics and objects bugs](../project/progress/vanilla_bugs/physics_and_objects.md#harmless)); ⚠️ a thing whose
    class makes no placement yet (fields, rewards, lanterns, spell icons, totems, whales) is drawn at the new place.
    Tested by `MapCells.APlainPositionSetLeavesTheObjectInItsCells` and
    `ObjectPlacement.AMoveWithoutTheModelLeavesThePlacementBehind`.
- **AreWeThere**: `walk_arrival::AreWeThere` on whole units, r in map units, each product rounded to a float as the
  game's FPU does; the positions come from openblack's metres through `map_coords::FromMetres`.
- **SET_POSITION** (`SetPosition` in `CHLApi.cpp`, the rule in `ecs::script_set_position::PlanFor`, tested by
  `test_script_set_position`):
  - the tests: "in a hand" is the hand's held object; "flying" is the `InPhysics` mark
    (`script_set_position::IsFlying`), openblack's IN_PHYSICS bit: thrown or knocked and still moving, carried by a
    tornado or a vortex, or left hanging where a hand let go of it. A body come to rest keeps its physics entry but not
    the mark, and is moved, as the original's test of the bit (0x6F8A92) moves it; until 2026-10-28 openblack read an
    entry in the physics, so it refused a resting body and moved a carried or hanging thing. The Physics window shows
    how the scripts see the object it names ("flying" or not, and its body); a script marker is the `ScriptMarker` tag `ScriptMarkerArchetype` gives it; an Object is
    `script_type::IsObject` and not a marker; "not in the map" is `map_cells::IsObjectInMap` false for a kind
    openblack files in the map cells;
  - the point goes through `map_coords::FromWorld` / `ToWorld`, so x and z are truncated to the map's units and the
    ground is taken there;
  - a creature goes to `CreatureLocomotionSystemInterface::Place`; a thing that is not an object has its Transform
    position set (a flock its domain centre); any other object is moved by `map_cells::MoveMapObject`, its rotation
    made the turn about Y by the Y angle it keeps and its scale the Transform's uniform scale, its leans kept for the
    next angle set (`placed_turn::StandUpright`; a thing with no angles of its own takes `UprightRotation`, kept as it
    is when it already is a turn about Y), `NotifyTeleported` (the turn-start copy) and, for a villager or an animal a
    script controls, script state 4;
  - before this port openblack put every thing on the ground at the untruncated point, refused nothing, dropped a
    marker's height, snapped the turn start of every thing and left a creature walking.
- **SET_TARGET** (`SetTarget`, the rule in `ecs::script_target` (`ECS/ScriptTarget.h`), ✅ faithful in its numbers,
  🧪 tested by `test_script_query_natives` (`ScriptTargetRule.*`, `SetTargetOf...`)): the thing's MapCoords through
  `map_coords::ToWorld`, `LaunchVelocity`, the three spin draws (`SpinFromDraw`); a flying thing
  (`DynamicsSystemInterface::IsFlying`) gets the velocity on its entry's body, any other Object
  `DynamicsSystemInterface::AddObject` with the spin as the original passes it (`BodySpin`); both get
  `Body::ClearDrag`. AddObject runs each class's start in the world (`PhysicsClassHooks::StartPhysics` with no hand):
  the classes the scripts throw are villagers (`Villager::InitialisePhysics` 0x5EFEF0: the dropped resource, then
  `Living::InitialisePhysics` 0x5EFE10) and rocks (`Object::InitialisePhysics` 0x637480: out of the map, the body,
  the catching creatures, the fire's StartedMoving, all in `DynamicsSystem::Add`). The Living's part, StorePreviousState
  unless FLYING or IN_HAND, then FLYING (refused: no physics), runs before the body as in the original:
  `PhysicsClassHooks::StartPhysicsInWorld` (`villager_physics::VillagerStartPhysicsInWorld`, an animal's
  `animal_ai::InitialisePhysicsWithoutBody`; tests `VillagerCoreTest.VillagerThrownInTheWorld*`, `KnockedAnimalFlies`).
  Before this it logged "not implemented".
- **GET_POSITION** gives an arrived villager's destination, and an animal's goal once it is within a step of it
  (`AnimalArrivedGoal`, the wall huggers' arrival test on `AnimalBrain::goal` and its speed), as the original's
  wall-hugger branch, which takes every one that is not a creature; before, an animal always gave where it stood.
  Tests `ScriptGetPosition.*` (`test_script_get_position`). The point is the goal's map position, at the land's
  height there plus the goal's own height above the land, as the original's `GetAltitude(goal) + goal.altitude`.
  MOVE_GAME_THING keeps the point's y less the land's height there with the goal it sets
  (`map_coords::FromWorld`, the original's `MapCoords(LHPoint)`): a villager's in `components::WalkGoalAltitude`
  (`ECS/WalkGoal.h`, which every goal setter clears), an animal's in `AnimalBrain::goalAltitude`; the saved world's
  load gives a villager's goal its saved height. So a walker sent to a point at y 0 gives y 0 back once there, as
  in the original; before, openblack gave the land's height. Tests
  `ScriptGetPosition.AVillagerThereGivesItsGoalWithTheGoalsHeightAboveTheLand`, `WalkGoal.*`. ⚠️ approximate: the
  walks the game plans itself are given their goals with no height above the land, where the original keeps the
  height of the point each is made from (for a point on the land, the land's own difference between that point and
  its place on the map, under a millimetre); an animal's other walks leave its goal height as it was. GET_DISTANCE
  does not look at y.
- **GET_DISTANCE** measures in x and z with the cut below 0.5 (it used to measure in 3D without the cut, so `== 0`
  was never met).
- **IS_ACTIVE**: `script_highlight::IsActive`, the dispenser's `timer.active`; a reward is active once its player
  has tapped it open (`RewardSystem::IsOpened`, [Opening a chest](map-script-objects.md#openblack)).
- **GET_OBJECT_STATE** (`GetObjectState`, ✅ faithful for the kinds openblack has): `LivingFinalState` in `CHLApi.cpp`
  (`villager::GetFinalState`, `animal_ai::FinalState`, 0 for a creature); a puzzle game through
  `ecs::PuzzleGameStatusOf` / `PuzzleGameStatusFrom` (`ECS/PuzzleGames.h`): complete is `IsPuzzleGamePlayed`, and
  only the fish puzzle begins (its bait made). Types 4, 5 and 15 (the two mazes, the second fish puzzle) and a type
  outside 1..19 are complete from the start, as the original's `IsComplete` has it (the 0x6D6C55 case of its table,
  which also marks the puzzle played): `ecs::CompletesAtOnce`, so PLAYED is 1 and the status won. The other puzzle types are not ported, so they answer 5 (not yet
  begun): LionPuzzle's loop ends only on the click on its highlight, and MagicMushroom never gets past PLAYED. Tested
  by `test_script_query_natives` (`GetObjectState...`, `PuzzleGameStatusRule`). Before this it logged "not implemented"
  and gave 0.
- **GET_TARGET_RELATIVE_POS** (`GetTargetRelativePos` in `CHLApi.cpp`, the formula in
  `script::target_relative::Position`, `ScriptHeaders/ScriptTargetRelative.h`, ✅ faithful): the original's steps and
  roundings through `gutils::x87` (the sine unrounded, the cosine stored), the not-a-number test included; tested
  bit for bit against the oracle by `test_script_target_relative`. Before this it logged "not implemented" and gave
  the point (0, 0, 0).

### Script states and releasing

- **The script states**: `SetScriptState` uses `map_cells::IsObjectInMap`, which is out in the hand, in flight and in
  a storm, as RemoveMapObject clears the bit. `SET_SCRIPT_ULONG` writes the villager's `ScriptAnimation::playsLeft` /
  `clip`; the 200 state's clip is `ClipChoice::Script` of `VillagerAnimate.cpp`.
- **SET_SCRIPT_ULONG** ✅ (`SetScriptUlong` in `src/CHLApi.cpp`), compared branch by branch with
  `GScript::SetScriptStateULData` 0x6F8770 and its loop function 0x6F8730 (2026-10-28): the pops (times, clip, the
  thing); no thing, "Object no longer valid"; the thing taken under the script's control (native 20 is one of the
  14 that take control, `GetScriptGameThing` 0x70D220); a script container (a town, a dance or a flock, the only
  types with a loop function, so "No Loop funtion for type" cannot be reached) takes each member under control and
  gives each villager member the clip and times; a villager, its clip and times (`villager_script::SetScriptAnimation`);
  a creature, its clip and times (`CreatureMindSystem::SetScriptPlay`; every creature has a mind from its archetype);
  anything else, "setting the state of something neither a creature nor a villager". The quests pass it a creature,
  a villager or a container (`<thing> play <clip> loop <times>`, 862 calls); what is not finished is the creature's
  play that SET_SCRIPT_STATE starts ([Playing what a script gives it](../creature/script-control.md#pending)). The
  look-up gives none for a thing being deleted or a villager on its way to dying, as the original's
  `GetScriptGameThing` (vt +0x2C, [the thing a native is given](#the-thing-a-native-is-given); until 2026-10-28 it
  tested only that the thing existed).
- **KEY_DOWN**: `KeyDown` turns the game's key number into SDL's scancode (`input::game_keys::ScancodeOf`, a table
  of the keyboard's scan codes and SDL's, `src/Input/GameKeys.h`) and answers `GameActionInterface::IsKeyHeld`, the
  keys the game's input has seen go down and not up. A number that is no key of the table is not held (the original
  reads whatever byte lies there). The check runs press no key: `OPENBLACK_IGNORE_REAL_INPUT` drops only the mouse, and
  the scripted presses of `QueuePress` do not hold their key, so it stays false there; no script that calls it runs on
  Land 1 or 2 (the Gods' Playground's `LandControlT` is never started). Tested by `test_key_down_native`
  (`GameKeys.*`, `KeyDownNative.*`).
- **PLAYED_PERCENTAGE**: `PlayedPercentage` answers a creature from its `CreatureAnimation` body:
  `creature_layers::PlayedShare`, the body's whole milliseconds over the length (from
  `CreatureAnimationSystemInterface::AnimationDuration`) of `creature_layers::ShareAnimation`: the action, a
  sequence's loop in each of its parts, a dance's move and the finish after one (`BodyAction::danceMoved`), and the
  stand pose otherwise. A dance move's time is the one drawn (`DanceMoveTime`). With no action the time is the legs'
  (`IdleBodyTime` in `CHLApi.cpp`): a turn's or step's `CreatureLocomotion::Move::timeMs`, else the first leg track's
  time at the end of the turn (`creature_locomotion::TrackTime`, which the drawn slots use too), else the breath's
  point (`creature_animation::BreathTime`). In a fight the length is that of the fighter's action
  (`CreatureFightSystemInterface::ActionAnimation`, the fighter's `actionAnimation`, the stand pose in its stance),
  with the shown animation's time; knocked out, against the faint until it gets up, then getting up. An action on
  a thing that is playing (`CreatureObjectAction`, read with `TryGet`) gives its `timeMs` against
  `creature_played_share::ObjectActionAnimation` (the table above), and a catch (`CreatureCatching`) its `timeMs`
  against `creature_played_share::CatchAnimation`: the step once `CreatureCatching::stepped` is set (where the step
  starts; cleared as the catch ends), else the stand pose. No thing and anything else give 1 with the original's messages. (openblack) a creature with no posed body or no length for the animation, which the game always
  has, gives 1 too. Tested by `CreatureLayers.TheSharePlayedIsTheWholeMillisecondsOverTheLength`,
  `CreatureLayers.ASequenceSharesItsLoopsLengthInEveryPart` and `test_played_percentage_native`
  (`PlayedPercentageNativeTest.*`).
- **SET_SCRIPT_STATE_POS / SET_SCRIPT_FLOAT** (`src/CHLApi.cpp`) give a villager, or each villager of a script
  container, its `ScriptStateArguments` place (on the ground, in metres; the map coordinates are taken from it when the
  wander uses it) and number, through `LivingActionSystemInterface::VillagerSetScriptPlace` / `Number`. The pair of
  whole numbers stays in `ScriptAnimation`, as the game shares it. Checked against v1.42 (2026-10-20): the native
  0x6F84C0 pops z, y, x (or the float), then the thing; "Object no longer valid" (0xC0D428) and "Object not villager
  for state data" (0xC0D460); a container's villagers in turn. The place goes in at +0x10C through `MapCoords::Set`
  0x603340 (× 6553.6, `ftol`), the same fixed value `FromMetres` gives; the number raw at +0x118. A container type with no loop
  function would say "No Loop funtion for type", which no shipped type reaches: only a town, a dance and a flock are
  containers, and each has a loop function in the frozen exe's table. The copies a container's loop reads are not
  kept, with the same result: the point goes in GScript +0x40..+0x48 (`FocusPos`, 0x6F8558) and the number in GScript
  +0x20 (the find radius, 0x6F86A7), each written just before the loop that reads it, and neither is saved
  (`GScript::Save` 0x6EBEA0 writes neither). Their every reader, from a scan of each `[[g_game] + 0x250090] + off`
  access in the frozen exe (2026-10-28): +0x40 only `SetStatePosLoopFunction` 0x6F8460 and `SetFocusLoopFn` 0x6F9090,
  each run by its own native (SET_FOCUS 0x6F90B0 writes it first too); +0x20 the loops `SetStateFloatLoopFunction`
  0x6F8600 and SET_PROPERTY's 0x70F350, the find checks 0x6F7070, 0x6F7100 and 0x6F7190 (passed only by functions
  that write the radius first: `FindNearPos`, the `FindIn…Near` / `NotNear` set, CALL_FLYING, IS_FIRE_NEAR, the
  hand demo's `FindNearPos`; `FindTownNearPos` and `FindCreatureNearPos` never call the check) and
  `GetSlowestInId` 0x6FBEE0, which writes it first. openblack hands the point and the number to
  `script_containers::ForEachMember`'s callback instead.
- **The shared storage** (checked 2026-10-20, a scan of every `[reg + 0x10C..0x120]` access in the exe's Villager
  functions). The place (+0x10C / +0x110), the number (+0x118) and the pair (+0x11C / +0x120) are unions with other
  states' data: +0x10C is also DiscipleOnPickUp, ReactionPos, FirePos and HidePos (writers: `EnterInHand` 0x76AFEF,
  the inspect / approach / bewildered reactions, `SetupFleeFromPredator`, `GoAndHideInNearbyBuilding`,
  `LookToSeeIfItIsSafe`); +0x118 the death reason, TargetThing, TradeResource, RequestingToGoHome, Partner, football
  and shepherd data; +0x11C TradeTown (`InHand` 0x76AEF3, `CheckInteractWithAbode` 0x757527); +0x120 the plays left
  (`ScriptPlayAnim`, shared in openblack too, `ScriptAnimation`). The readers of the wander's data are
  `SetupNewScriptWander` 0x7686D0 (all of it) and, for +0x120, `ScriptPlayAnim` and `IsScriptAnimationComplete`
  0x7689D0. A villager can leave state 199 only where `ExitInScript` lets it (script-interruptable states, IN_HAND,
  FLYING, the same exit), and it comes back into 199 by itself only from the hand: on landing alive on dry land
  `Villager::EndPhysics` puts it back in its previous state when that row's go-back-after-landing (file 0xF4) is 1,
  which it is for 199. So the one path on which another state's write is read by the wander is the hand:
  - `EnterInHand` 0x76AFE0 writes the disciple on pick-up (its disciple, 0 for a missionary) as a whole dword into
    +0x10C, the place's x. Once put back down the villager wanders round (that number, its z) in whole map units, at
    the map's edge. openblack does the same (from the exe; `villager_script::SetScriptPlaceX`
    from `villager::EnterInHand`).
  - `InHand` 0x76AEF3 writes the thing's town (vt +0x48, a pointer) into +0x11C, the fewest turns, when a villager
    not made by its script (+0x24 & 0x400 clear, tested at 0x76AE7B) is held near a thing that offers it the trader's
    work (disciple 9). The fewest then is a heap address, which no reconstruction can have: openblack keeps the
    fewest the script gave. This is settled, not open: the value is not a game number.
  - `InterfaceSetInMagicHand` 0x753080 clears +0x118 only for a villager whose top state is 123 (`cmp byte [esi +
    0x8D], 0x7B`), never for one in 199.
  Every other writer belongs to a state the villager reaches only after 199 has let it go, and nothing brings it back
  into 199 from there without the script setting the place, number and pair again.
- **199 SCRIPT_WANDER_AROUND_POSITION**: `villager_script::ScriptWanderAroundPosition`, with
  `villager_script_rules::StepWander`, `NextWanderStop` and `WanderPause`; entry `EnterInScript`, exit
  `ExitInScript`. Tested by `VillagerCoreTest.ScriptWanderAroundPositionSetsOffThenStands` and the
  `VillagerScriptRules` wander tests. Checked against v1.42 (2026-10-20): `SetupNewScriptWander` 0x7686D0 draws
  `GameFloatRand(2π)` (0x40C90FDB) for the bearing, then `GameFloatRand(radius)` (none drawn for 0), goes
  `AddDistanceFromAngle`, `SetupMoveToWithHug(goal, 199)`, then the turns `fewest + GameRand(most − fewest)` as 16
  bits (the difference unsigned); the stand test at 0x768808 is signed (`test ax, ax` … `jg`). The oracle, synced
  seed: centre (0x640000, 0xC80000), radius 25, 10..40, seed 0x88F89F → goal (6506702, 12995566), 26 turns, seed
  0x47184E08; radius 0, 0..5 → goal unchanged, 3 turns, seed 0x5BAD6184; 40..10, radius 12 → 20016 turns. The row
  (GStates) has `EnterInScript` / `ExitInScript`.
- `DataForScriptRemind` is not ported: the branches of EnterInScript / EnterPlayAnim that resume a walk are never
  taken (🔍 inferred).
- **⚠️ approximate**: openblack's PathfindingSystem moves every entity with a mark, whatever its state, so
  `SetScriptState` removes the marks when the villager is no longer in MOVE_TO_POS (without that the father kept
  walking, and overshot, while acting in 200).
- **Releasing**: the villager case uses `map_cells::IsObjectInMap` through `villager::IsObjectInMap`;
  `ecs::script_held::Process` is the release of the things no variable holds (`fn_0070D480`).

### Properties

- **PLAYER (21)**: `chlapi::ThingPlayerProperty` answers it for a town (its owner, 1 when
  `script::property_rules::TownCompletelyDestroyed` holds for its abodes), a villager (`villager::GetPlayerOf`), a
  creature (its owner) and an abode that is not a field (its town's owner, by `Abode::townId`), through
  `script::property_rules::PlayerProperty`, pushed as a float. Before this the property fell to the unwritten ones and
  always gave an integer 0. Tested by `test_script_query_natives` (`PlayerOf...`). The debug bar's World menu has a
  Towns list (closed by default) with each town's owner and the value the scripts read.
- **FLYING (5)**: the `InPhysics` mark (`script_set_position::IsFlying`, the test SET_POSITION makes), pushed as a
  bool: thrown or knocked and still moving, carried by a tornado or a vortex, or left hanging where a hand let go of
  it. Until 2026-10-28 it was whether the thing had a physics entry, so a body come to rest that the physics keeps to
  be knocked again (a boulder that had stopped) still read as flying and a carried or hanging thing did not. The debug bar's Physics window
  shows, for the object it names, whether the scripts take it to be flying. Tested by `test_script_query_natives`
  (`FlyingIsWhetherTheThingIsInThePhysicsAsABool`).
- **GET_PROPERTY of a town, a flock or a dance**: a float 0 for every property but PLAYER, with no message, before the
  property's own case, as the original's test of a script container (0x70DB31) does. Before this only YPOS and the
  angles gave it; the built share read 1, the height an integer 0, the health logged "Object not an object". Tested by
  `ScriptQueryNatives.AFlockHasNoPropertyOfItsOwnButItsPlayer`.
- **SET_PROPERTY on a town, a flock or a dance**: `SetProperty` hands the property and the value to each member, in
  the container's loop order (`script_containers::ForEachMember`), and its work on one thing sets each member as it
  would set the thing itself, for every property, as the original's frame (0x70F380) runs the container's loop
  function with the worker. Before this only YPOS, HEALTH and the angles were handed on (each by its own loop) and every other property
  of a container fell to the thing's own case: Land 2's Spiritual Healer, who sets the age of every villager of his
  town to 16 every three minutes (`TownAge`, refs/chl/spiritualhealer.txt:10), set none. Tested by
  `ScriptQueryNatives.SetPropertyOfAFlockSetsEveryMemberAsItself`.
- **IN_HAND (9)**: `script::property_rules::InHandProperty` with the hand system's held object (`GetHeldObject`,
  the status's hold from the pick-up packet, as the original's bit), 1.0 or 0.0 as a float. Before this the property
  fell to the unwritten ones and gave an integer 0, so a held thing read as not held (Land 1's gate stones, Land 2's
  silver scrolls). openblack has one hand, the local player's; the computer players' hands are not ported. Tested by
  `ScriptProperties.AThingIsInHandOnlyWhileThePlayersHandHoldsIt`. No debug control yet (pending).
- **YPOS (24)** on any thing: `GET` is a highlight's own height as before, 0 for a town, a flock or a dance, else
  `script_set_position::HeightAboveLand` of its Transform (its y less the land's height at its point in whole map
  units, as `map_coords::FromWorld` keeps an altitude). `SET` is a highlight's draw height as before, else
  `chlapi::SetHeightAboveLand`: the Transform's y becomes the land's height there plus the value
  (`script_set_position::AtHeightAboveLand`, x and z kept as they are), and an object other than a creature is stood
  upright on its Y angle with its uniform scale and keeps its leans, as SET_POSITION rebuilds its matrix
  (`placed_turn::StandUpright`); a town, a flock or a dance sets
  each member. Before this both fell to the unwritten properties for anything but a highlight (`GET` 0, `SET`
  nothing), so every "sink until" / "rise until" loop of the story spun for ever (Land 3's FreezeRayStatue inside a
  cinema, the Shaolin guru, Land 4's Nemesis pieces). Tested by `ScriptSetPosition.AThingsYPos...` and
  `ScriptSetPosition.SettingAThingsYPos...`.
- **GET HEALTH (1) of a citadel a script finds** (2026-10-28): the heart's life. Only the heart has the citadel's
  script type: `CitadelHeart::GetScriptObjectType` 0x4680B0 returns 18, while the `Citadel` container, its parts and
  plans, the worship totem and the spell icons keep the base's 0 (`GameThingWithPos::GetScriptObjectType` 0x570200,
  `xref.py vtcalls 0x4E8`), so `get 18 at …` (CALL / CALL_NEAR) can only find a heart, an Object whose `GetLife` is
  `Object::GetLife` 0x402600 (vt +0x11C). openblack's citadel heart is its `Temple` entity, script type 18
  (`script_type::TypeOf`) and an object, so its GET HEALTH is its life, as already. Land 2's control script ends
  Lethys when his town total is 0 and `1 of LethysCitadel <= 0.1` (refs/chl/landcontrol2.txt:283, 359), which so
  waits for his heart. Tested by `ScriptQueryNatives.ACitadelFoundByAScriptIsItsHeartAndItsHealthIsTheHeartsLife`.
- **SET HEALTH (1)** on anything but a totem statue: `chlapi::SetHealthByScript`. A town, a flock or a dance sets
  each member; a forest that is not a big forest, a mist, a spell or an influence ring logs "Object not an object";
  any other object goes to `life::SetLifeUnlessProtected`, protected when it is `Indestructible` or when a script
  holds it (`script_held::IsInScript`) while a script owns the widescreen (`HelpSystem::IsScriptWideScreen`). A
  villager's town counts its injured first, as the original. Before this every SET of a health but a totem's was
  unwritten: Land 1's lost brother was never sick, and Land 2's huts were never mended. Piles, trees and openblack's
  crop fields (`Field`; whether it is the original's `FieldCrop` is not read) are left pending. GET HEALTH reads its
  facts through the const registry now, so it makes no component storage (only the hash's `pools` part may move).
  Tested by `LivingImpactTest.AProtectedThingKeepsItsLifeAgainstAHundredthOrLessButItsTownStillCountsIt`.
- **ANGLE (2), XANGLE (3) and ZANGLE (4)** of an object that is not a scaffold: `GET` is the angle the thing keeps
  (`placed_turn::AnglesOf`, [The angles of a thing](../world/objects.md#angles-of-a-thing)) in degrees
  (`property_rules::AngleToScript`): the angles it was placed at, set by a script or taken from its body, never the
  angles of its turn while it still has the turn they made; the leans of a kind that never leans are 0. A villager's
  or an animal's ANGLE is its own Y angle (`villager_physics::ObjectYAngle`). `SET` (`chlapi::SetAngleByScript`)
  replaces the one angle and keeps the other two (`placed_turn::AnglesAfterScriptSet`; ANGLE does nothing when the
  thing has that angle already), turns the thing to them (`placed_turn::SetAngles`: all three for a thing that leans,
  about y alone for any other, with its scale made even) and puts it back at the head of its cells' lists; a villager
  or an animal turns by its own Y angle setter (`living::SetYAngle`) and keeps no lean. A creature's ANGLE is the
  angle kept for the scripts (`Creature::scriptYAngle`, 0 when it is made), never the way it faces; its `SET` changes
  that angle when it differs and puts the creature back at the head of its cells' lists, its XANGLE and ZANGLE are 0
  and their `SET` only puts it back in its cells; its body is never turned by them. A town, a flock or a dance
  gets a float 0 from `GET` and sets each member; a thing that is no object logs "Object not an object". Before this
  `GET` of ANGLE and of the leans of a thing that may lean logged "not implemented" and gave 0, and every `SET` but a
  scaffold's ANGLE was unwritten. In the story: The Sculptor stands QuarryRock upright (XANGLE and ZANGLE 0) and
  makes its gate stone turned half round from its ANGLE; SwapToCow and CreateSlaversDen add a quarter to a fence's
  ANGLE. Tested by `PlacedTurn.AScriptSetsOneAngleAndKeepsTheOtherTwoAsTheOriginal` (the oracle's numbers),
  `ScriptQueryNatives.AnObjectsAnglesAreTheOnesItKeeps`, `ScriptQueryNatives.ACreaturesAngleIsTheScriptsOwnNotTheWayItFaces`
  and `ScriptIsObject.CreaturesBuildingsFeaturesAndTreesNeverLean`. A creature's ANGLE and every `SET` of its angles
  logged "not implemented" before; no story script reads or sets them but the CreatureGuide's ZANGLE, which reads 0
  as before.
- **ANGLE (2) of a scaffold**: `GET` is `scaffolds::PhantomAngleByScript` in degrees (`property_rules::AngleToScript`):
  the angle a script turned its phantom building to while `Scaffold::k_NoPhantomRotation` is set, else 0, never the
  scaffold's own turn. `SET` (`chlapi::SetAngleByScript`) turns the phantom building to the value in radians
  (`property_rules::AngleFromScript`) and sets that flag (`scaffolds::SetPhantomAngleByScript`), so the phantom no
  longer turns by itself in the hand and keeps the angle when the plan changes; the scaffold's own turn is not
  touched. A town, a flock or a dance sets each member; a thing that is no object logs "Object not an object". The
  conversions are bit for bit the original's (`ScriptProperties.AnglesAreTurnedAsTheOriginalsFloats`). Before this
  both fell to the unwritten properties. Land 2's CreateWorkshop turns Khazar's workshop scaffold to 160, so the
  workshop it builds is tried first at that angle (`scaffolds::BuildBuilding` tries the phantom's angle before the
  plan's own); GetThroughVortexL2 and Land 5's SetupLand5 turn the storage and workshop scaffolds. Tested by
  `ScaffoldPhantom.AScriptTurnsThePhantomBuildingAndItStopsTurningByItself`.
- **SCALE (8) of a creature**: `GET` is `object::GetScale`, the size its body is drawn at
  (`creature_pose::DrawnSize`: the pen's in its temple's pen, else `Creature::size`) kept within
  `creature_morph::ClampScale` (0.05 to 4), as the original's user size. `SET` was already
  `player_creature::SetCreatureScale` (the body given the size at once, its own size as given), so what a script sets
  is read back at once. Before this `GET` logged "not implemented" and gave 0, so CreatureDevGuideTeachesFight's
  "grow the guide by a tenth until 1.9" never ended and set the guide to 0.1 on every pass (a hard stop of Land 1's
  guide path), and HermitMain read the player's creature as size 0. Tested by
  `ScriptQueryNatives.ACreaturesScaleIsTheSizeItsBodyIsDrawnAt`.
- **SCALE (8) of every thing**: `GET` is `object::GetScale` for an object (its uniform scale; a creature's as below)
  and a float 1 for any other thing (`script_type::IsObject` false: a mist, a spell, an influence ring, a weather
  thing, a timer, a puzzle game), as the original's `GetScale` on any thing, whose base gives 1. Before this every
  thing but a villager, an animal or a creature logged "not implemented" and read 0: Land 4's Blind Woman shrinks
  her boulder by 0.05 while its scale reads more than 0.1 (refs/chl/BlindWoman.txt:16), which ended at once. Tested by
  `ScriptQueryNatives.EveryObjectsScaleIsItsOwnAndAnyOtherThingsIsOne`.
- **SCALE (8) of a thing that is not living** (a rock, a feature): `SET` is `living::SetThingScale`, as
  `Object::SetScale` 0x639200: the Transform's uniform scale, only when it differs, and then the thing is taken out of
  the map's cells and filed again with its new size (`map_cells::OnAnglesOrScaleChanged`, as `SetXYZAnglesAndScale`
  0x638F80 removes and inserts it). Throwing Stones' ammunition script sets a rock left far from its pile to half
  size this way; before, it logged "not implemented" and the rock kept its size. A thing with no place is "Object no
  longer valid". Tested by `ScriptLivingPropsVillager.AThingThatIsNotLivingTakesTheScriptsScale`.
- **SCALE (8) of a special effect**: `SET` is the magnitude of the spot visual's effect from then on
  (`ParticleSystemInterface::SetSpotVisualMagnitude`, `Effect::SetMagnitude`), as the original's particle container
  gives its particle system the value; the object's own scale is left as it is. Any other thing that is no object (a
  mist, a spell, an influence ring, a weather thing, a timer, a puzzle game) keeps none, as the base `SetScale` does
  nothing. Before this a special effect's object took the value as its scale, which nothing draws by, and its effect
  kept the magnitude it was made with: the smoke and dust the Crusaders, Free the Creature, the Land 4 meteorites and
  ogre, the big fight, the creature's curse, Fire on High and Land 5's control script scale stayed at their made size.
  Tested by `ParticleSystemTest.AScriptsScaleOfASpotVisualIsItsEffectsMagnitudeAtOnce` and
  `ScriptQueryNatives.AScriptsScaleOfASpecialEffectIsItsMagnitude`.
- **HEIGHT (19) of every other thing** (not a puzzle totem, a totem statue or a creature, which keep their own):
  `GET` is `object::GetHeight` for an object (the height of its mesh at its scale, a magic fireball's its radius) and
  a float 0 for a thing that is no object; `SET` keeps nothing, with no message. The original asks any thing for its
  `GetHeight` (vt +0x42C: `Object::GetHeight` 0x638120 in 132 vtables, `GameThingWithPos::GetHeight` 0x405500, a float
  0, in 51, `MagicFireBall`'s, `GCamera`'s and `Creature::GetHeight` 0x477F50 in one each) and its `SetHeight` (vt
  +0x144: `GameThingWithPos::SetHeight` 0x4019B0, a bare return, in 185 vtables, `Creature::SetHeight` 0x47DE80 in
  one), by `xref.py vtcalls`. Before this both logged "not implemented" and `GET` pushed an integer 0. No shipped script
  reads or sets the height of such a thing. Tested by `ScriptQueryNatives.AnObjectsHeightIsItsOwnAndAnyOtherThingHasNone`.
- **SPEED (11) of a creature**: `SET` sets the fraction of its top speed it goes at
  (`CreatureLocomotion::fraction`, `property_rules::CreatureSpeedFraction`: 0 to 1, 0 for a value that is not a
  number), which its locomotion heads for each turn (`creature_locomotion::TargetSpeed`) until it is next sent somewhere
  at a pace of its own. `GET` is `living::SpeedProperty`: in the physics with its physics entry the length of its
  velocity, else the default speed its kind's info gives when it is made (`living::CreatureMadeSpeedInMetres`), which
  `SET` does not change. Before this both logged "not implemented" (`GET` gave 0). The guide is sent at all of its top
  speed by CreateGuideCreature (40.5) and TheStorm (8). Tested by
  `ScriptProperties.ACreaturesSpeedIsAFractionOfItsTopSpeedFromNoneToAll`,
  `ScriptQueryNatives.ACreatureIsAskedForAFractionOfItsTopSpeed` and
  `ScriptLivingPropsVillager.ACreaturesSpeedIsTheOneItsKindIsMadeWith`.
- **SPEED (11) of a weather thing**: `GET` is `weather_thing::SpeedOf` (its storm's `Storm::speed`, 0 once the storm
  is gone) and `SET` `weather_thing::SetSpeed`. Before this both were unwritten for a weather thing. Land 2's
  `KillKhazarMain` sets the black cloud's speed to 20 after a MOVE_GAME_THING to a point, so the cloud travels there
  at 20 m/s. Tested by `WeatherTest.AWeatherThingsSpeedIsItsStormsUntilTheStormIsGone`.
- **SPEED (11) of anything else** (`SetSpeedByScript` in `src/CHLApi.cpp`): a thing that keeps no speed (every
  class but the livings and the weather, whose `SetSpeedInMetres` is the base's empty one) is left as it is with no
  message, and `GET` on it gives 0 with no message (the base `GetSpeedInMetres` returns 0.0; a script container gives 0
  before any property is read). No thing is "Object no longer valid". A town, a flock or a dance hands the speed to
  each member through SET_PROPERTY's container step (above), as the original's container loop hands the property to
  every member. Before this a container's members kept their speeds and both logged "not implemented". The quests
  that set a container's speed: The Wolves Are Possessed's pack (`FreeTheCreature`, `WolfPack` 0.5 and 0.6), The
  Slavers' `AnimalFlock` and `SlaverPosse` (0.5) and The Greedy Farmer's gang (`Children` 0.4). Tested by
  `FlockNatives.ASpeedSetOnAFlockIsSetOnEachMember`.

### Poison

- `ecs::object_poison` (`src/ECS/ObjectPoison.h`) is the IsPoisoned / SetPoisoned pair of every kind of thing: a
  villager, an animal or a creature its `Poisoned` component (`ecs::life`, which now lets a creature carry it too, as
  its Living bit), a pot or a pile its `Pot::poisoned`, a storage pit through `object_resources::IsPoisoned` (an
  available food or wood pile poisoned) and, to poison, its available food pile only, never cured; a toadstool
  (`MobileObject` of `MobileObjectInfo::Toadstool`) always poisoned.
- `object_resources::IsPoisoned`, which the deliveries (`object_delivery`, `held_apply`) and a villager taking from a
  thing read, answers as `object_poison::IsPoisoned` does: a villager, an animal or a creature its bit,
  a toadstool always (as the original's things answer `IsPoisoned`); it answered a plain toadstool clean before. Test
  `TakeResourceTest.AToadstoolAndAPoisonedLivingThingAnswerPoisoned`.
- `IdPoisonedSize`, `IsPoisoned`, `SetPoisoned`, `CallPoisonedIn` and `CallNotPoisonedIn` (`CallPoisonedOrNotIn`)
  walk a container with `script_containers::ForEachMember`, the loop function of a town, a dance or a flock; "not an
  object" is `script_type::IsObject`. Tested by `test_script_poison_natives` (group `test_ui_script`).
- No check reaches them: the Plague starts five minutes after the player owns its Land 2 village and has a worship
  site there (`PlagueCheck` in `landcontrol2.txt`).

### Magic, weather and worship natives

All in `src/Magic/Script/`. The VM is reached through `Locator::vm::value`.

| File | Natives |
|---|---|
| `CHLFire` | `magic::script::IsFireNear` |
| `CHLInfluence` | `InfluenceObject`, `InfluencePosition`, `GetInfluence`, `GamePlayer` |
| `CHLSpells` | `SpellAtThing` and its `ScriptInfo` |
| `CHLWeather` | `CreateWeatherThing`, `ChangeWeatherProperties`, `ChangeLightningProperties`, `ChangeTimeFadeProperties`, `ChangeCloudProperties`, `ChangeWeatherInnerOuter` (056 on a weather thing, after `script_containers::ChangeInnerOuter` has said it is not a flock; `weather_thing::SetInnerOuterProperties`), `SetAffectedByWind` (131, `weather_thing::SetAffectedByWind`), `PauseUnpauseClimateSystem`, `PauseUnpauseStormCreationInClimateSystem`, `KillStormsInArea`. 056 and 131 checked against v1.42 (2026-10-20): `ChangeObjectProperties` 0x70F480 pops calm (`ftol`), outer, inner, the thing; a flock (vt 0x3EC) as before, a weather thing (vt 0x3FC) through 0x774340 (+0x34 inner, +0x38 outer, `UpdateStats`; calm unused), else "Invalid thing for Changing Variables" (0xC20A54); `SetAffectedByWind` 0x6FABC0 pops the thing, then the value, "Invalid thing" (0xC0C6EC), only the weather thing keeps it (vt 0x4F0), read non-zero at 0x774264 |
| `CHLWorship` | `SetTemperature`, `script::GameSetMana`, `script::GetSpellIconInTemple`, `script::GetTownWorshipDeaths` (the `TownDeaths` component), `SetDispenserActive` (true when the object was a dispenser; SET_ACTIVE and SET_TIMER_TIME on other objects stay in `CHLApi`) |
| `MapScriptMagic` | `CreateSpellDispenser`, `CreateWorshipSite` |
| `MapScriptWeather` | `magic::map_script::CreateWeatherStorm`, `CreateWeatherClimate`, `CreateWeatherClimateRain`, `CreateWeatherClimateTemp`, `CreateWeatherClimateWind` |
| `ScriptPlayer.h` | `ScriptPlayerToGamePlayer` |

<a id="the-quests-natives-read-against-the-original"></a>
### The quests' natives read against the original

✅ Each native below was read in runblack.exe v1.42 (xref and the Ghidra export, 2026-10-28) against its body in
`src/CHLApi.cpp` and what that calls, for every argument form and kind of thing the shipped challenge scripts give it
(the statements `tools/checks/story/script_coverage.py` lists from `challenge.chl`). Each pops in the original's
order, takes the same things and changes the same state. A script player is a float cut toward zero and converted
as `ScriptPlayerToGamePlayer` (0 the neutral slot, n the game player n − 1); a thing is the script's slot lookup,
where openblack takes a live entity. Only the log messages differ (the original's are silent in the shipped game).
What is left on a behaviour's progress row is not the native's.

| Native | The original | openblack |
|---|---|---|
| SET_ACTIVE 255 | `GScript::SetActive` 0x6FD720: the thing, then the value; none → "Object no longer valid"; a script highlight `SetActivated`, a spell dispenser `SetActive` (0x723030), a scaffold `ForceBuildBuilding(0)` 0x6E8860 when the value is not 0 (nothing at 0); anything else "Invalid object type". Scripts give highlights, dispensers and scaffolds, as `enable`/`disable … active` (1 or 0) | `SetActive`: the thing as the scripts reach it first, as `GetScriptGameThing` 0x70D220 gives it (none, or one being deleted, is "Object no longer valid", whatever it is), then the same order of tests (`script_highlight::SetActivated`, `MagicSystem::SetDispenserActive`, `scaffolds::ForceBuildBuilding` with no player); tests `ScaffoldWorld.AScriptsSetActiveMakesAScaffoldBuildAtOnce`, `ScriptQueryNatives.SetActiveLeavesAThingBeingDeletedAlone` |
| SET_AFFECTED_BY_WIND 131 | 0x6FABC0 (bw1-decomp `ScriptFunctions.cpp`): the thing, then the value; vt +0x4F0 is overridden only by `WeatherThing` (0x55DF20, +0x7C), every other class's 0x4024C0 does nothing. Scripts give `disable … affected by wind` on storms, clouds and a meteor | `magic::script::SetAffectedByWind` → `weather_thing::SetAffectedByWind` (nothing for a thing that is not weather); test `WeatherTest.AScriptResizesAWeatherThingsStormAndLetsTheWindCarryIt` |
| SET_OPEN_CLOSE 307 | `GScript::SetOpenCloseState` 0x708E30: the thing, then the word; none → "Thing not found!", not an `AnimatedStatic` → "Thing must be an animated static"; else +0x7C = the word as it is (0x422CC0). Scripts open and close the phone box (an animated static, type 41) and the gates | `SetOpenClose` → `AnimatedStaticSystem::SetOpenState`; test `AnimatedStaticSystemTest.AScriptOpensAndClosesOnlyAnimatedStatics` |
| GET_HAND_STATE 413 | 0x6FF730: pushes GInterface +0x3AC, the hand state of the last turn, as an integer. The scripts compare it with 20 and 22 (gripping the land, zooming it) | `GetHandState` pushes `HandSystem::GetInterfaceHandState` as an integer; how that state is worked out is [The hand's state](../interface/tooltips.md#the-hands-state) |
| MOVE_COMPUTER_PLAYER_POSITION 310 | `GScript::MoveComputerPlayer` 0x6EE8D0: the flag, the speed, z, y, x, the player; with its computer player 0x6EEC20 switches the mind on (+0x1B8 = 1) and unpauses it (`Pause(0)` when +0x1BC), then a flag not 0 → `ForceComputerPlayerToMoveToPointAndPause` 0x658510 (step 1, over the ground), 0 → 0x658590 (step 35, straight): the steps given up, the speed the popped one, let go, one step, busy and held | `ComputerPlayerSystem::MoveHand`: `SwitchOn`, `Pause(false)` for a paused mind, then `computer_hand::TakeForScript` with `MoveHandToPos` or `MoveHandToPosDirectYInterpolation` at the popped speed ([the script's natives](../hand/computer-hands.md#the-scripts-natives)) |
| GET_COMPUTER_PLAYER_POSITION 312 | `GScript::GetComputerPlayerPos` 0x6EEAB0: the player; pushes x, y, z of the hand's map place (`GetHandPos` 0x657FE0): x and z × 10/65536, y the land's height there plus the place's altitude, which each turn of the steps (0x650370) sets to 0; no player or no computer player: 0, 0, 0 | `HandOnGround` (`computer_hand::OnGround`: `map_coords::ToMetres` and the land's height), the origin otherwise |
| ENABLE_DISABLE_COMPUTER_PLAYER 311 and 345 | 311 `SetComputerPlayerOnOff` 0x6EECC0: the player, then the switch, written to +0x1B8 only; 345 `SetComputerPaused` 0x6EEBA0: the player, then the value → `GComputerPlayer::Pause` 0x658610 (paused and held = the value, the steps take the player's speed) | `SwitchOn` (311) and `ComputerPlayerSystem::Pause` (345: `computer_player::Pause` and `computer_hand::Pause`); tests `ScriptQueryNatives.EnableDisableComputerPlayerPopsThePlayerThenTheSwitch`, `ComputerPlayerStart.APausedMindWaitsToStartUntilItGoesOn` |
| SET_COMPUTER_PLAYER_POSITION 313, RELEASE_COMPUTER_PLAYER 370 | [the script's natives](../hand/computer-hands.md#the-scripts-natives) (0x6EE9D0, 0x6EEFD0) | `PutHand`, `Release`; tests there |
| SET_COMPUTER_PLAYER_PERSONALITY 364 | 0x6EED50: the value, the name, the player → 0x664000: every node `FindDesireNode` finds for the name takes the value ([the scripts set the mind](../hand/computer-minds.md#the-scripts-set-the-mind)) | `computer_player::SetPersonality` over `FindNodes`; tests in `test/test_computer_mind.cpp` |
| SET_PLAYER_ALLY 382 | 0x6FED70: the strength, player 2, player 1; with both, `SetAllyValue` both ways as it is ([alliances](../magic/influence-and-alignment.md#alliances)) | `alliances::SetAlliance`; test `PlayerAlliancesKept.SettingAnAllianceSetsItBothWaysAsItIs` |
| SET_TOWN_DESIRE_BOOST 341 | 0x6FE650: the boost, the desire, the thing; a town, a desire up to 16 and −1 ≤ boost ≤ 1 → +0xD4[desire] = boost and the desires re-sorted (0x746140); "Thing not valid!", "Invalid Params" ([villager decisions](../beings/villager-decisions.md)). The scripts give desires 0, 5, 6, 8, 9 and 16 | `town_desire::ScriptSetTownDesireBoost`; test `TownDesireTest.Scripts`. A negative desire, which no shipped script gives, is written outside the array by the original and not by openblack ([a vanilla bug](../project/progress/vanilla_bugs/villagers_and_towns.md)) |
| SET_VIRTUAL_INFLUENCE 254 | 0x6FD6C0: the player, then the switch → `GPlayer::SetVirtualInfluence` 0x64C3B0 ([virtual influence](../magic/influence-and-alignment.md#virtual-influence)). The scripts switch player 1's | `InfluenceSystem::SetVirtualInfluence` → `virtual_influence::SetEnabled` (off: the strength 0) |
| GET_INFLUENCE 062 | 0x6F9C60 (bw1-decomp `ScriptFunctions.cpp`): the position, raw, the player → `CalculatePlayerInfluence`(position, player, default, allies = raw == 0) | `magic::script::GetInfluence`; 0 for no player |
| SET_PLAYER_BELIEF 325 | `GScript::SetBelief` 0x6FD520 (the table's entry: name at 0xC19278, function 0xC19268): the share, the player, the thing; none "Object no longer valid", not a town "Not town!"; a town → `GBelief::SetBelief`(player, its belief in the town's owner × the share), capped. A town always has an owner, the neutral player when none ([towns](../beings/towns.md)) | `town_belief::SetBeliefRelativeToOwner` over `SetBelief` (capped); tests `TownBeliefTest.ScriptSetsBeliefAsAShareOfBeliefInTheOwner`, `TownBeliefTest.SetBeliefClampsToCapOnly` |
| OBJECT_RELATIVE_BELIEF 251 | `GScript::SetObjectRelativeBelief` 0x6FD450 (read 2026-10-29): the amount, the player (`ftol`, converted, `GGame::GetPlayer` 0x5509B0, not checked), the thing; none "Object no longer valid", not a town "Not town!" and nothing; a town → `Town::PlayerTakeTownCheat` 0x73CB50(player, amount): taken by the neutral player, every active player and the neutral one believed in 0; else the one believed in most (strictly more, from 0) set to half that, then the taker to that most plus the amount, each through `GBelief::SetBelief` (bw1-decomp's C and the frozen exe's are the same). The one shipped call gives a town, script player 2 and 1 (`FreeTheCreature`) | `ObjectRelativeBelief`, `town_belief::PlayerTakeTownCheat` (capped `SetBelief`); test `TownBeliefTest.TakingATownHalvesTheMostBelievedAndGivesTheTakerMore`. A script player outside 0..8, which the original hands to the cheat as no player, is refused; the active players are the slots with a player entity on the land, then the neutral one (`PlayersInGameAndNeutral`, `town_belief::PlayersOnLandAndNeutral`; test `TownBeliefRules.TheTakeTownCheatWeighsThePlayersOnTheLandAndTheNeutralOne`). Until 2026-10-30 it asked the player service for every slot, which throws for a slot without a player, so the one shipped call (Land 3's wolves reaching the Indian village) stopped the game |
| CREATE_REWARD 239 | 0x6F2B30: from the sky, the point, the type; 1..60 → `Reward::Create`(point, `GRewardInfo`[type], the local player, in the map at once = not from the sky, angle 0, scale 1, no town), added as the script's creation and pushed; else "Invalid type" and 0 ([reward chests](map-script-objects.md#reward-chests-create_reward-239-create_reward_in_town-240)) | `GiveReward`; tests `ScriptRewardNatives.*` |

### The virtual machine

- **CAST**: `Opcode23Cast` in `LHVM.cpp` converts as the original does (it used to change only the type, so
  `PUSHF 1.0 CASTI` reached SET_SCRIPT_ULONG as 0x3F800000 times).
- **SYS**: `LHVM::Opcode05Sys` counts the pops and pushes of the call.
  - A native that pops **none** of its `stackIn` arguments (a stub that only logs "not implemented") has those values
    taken out from under whatever it pushed, so the next native reads its own arguments.
  - A native that pops only some of them is left as it is: its leftover arguments shift every later read, so each
    native's missing pops are fixed in the native itself, as its own fidelity commit (DANCE_CREATE's six pops).
  - A function with no implementation at all pops `stackIn` values and pushes `stackOut` zeros.
- **The file**: `components/ScriptLibrary/src/LHVMFile.cpp` reads and writes the [challenge file](#the-challenge-file-chl)
  in the same order (a saved game appends the VM's state after the data block). `Opcode01Jz` jumps as the original.
- **EQ and NEQ** ✅: `Opcode14Eq` / `Opcode15Neq` compare only integers, booleans and floats (a NaN pair counts as
  equal) and, on any other type, pop nothing and push false (EQ) or true (NEQ), as the original
  ([Virtual machine EQ and NEQ](#virtual-machine-eq-and-neq); 87b858af2; tests `test/lhvm/test_lhvm_compare.cpp`,
  `LhvmCompare.*`). GEQ/LEQ/GT/LT match it.

### In game

The hand demo (`Input/HandDemo.h`) and the opcodes 266 / 267 / 336 are wired; the dialogue texts are drawn
(`Help/HelpTextDisplay.h`, `Renderer::DrawHelpText`; their look is not compared with the original's yet). The
advisors, the welcome crowd and dance, the villagers' focus and override animations, the intro's JC specials and the
high-detail family are ported too ([State in openblack](land1-intro.md#state-in-openblack)); what the intro still
lacks is on [The Land 1 intro](land1-intro.md#pending).

### Where it is in the code

Each native of `src/CHLApi.cpp` pops what the original's pops, in its order; what differs is said.

- **Helpers**: `ScriptVm` (the VM as GScript asks it); `IsHelpTask` (the task's script type is Help); `ScriptThing`
  and `SpiritThing` (`GetScriptGameThing` with the opcode's own message: 0 is null, and an entity the scripts can
  reach (`ecs::script_held::IsReachable`, [the thing a native is given](#the-thing-a-native-is-given)) is the thing;
  ⚠️ the original's slot table and its messages for a missing slot are not ported); `ScriptCameraMode` (the camera
  opcodes' mode check: openblack always has the player's mode, so only "wrong camera mode" can happen; the citadel
  note of 003 / 004 / 287 is not ported, as the citadel never has a script mode); `CheckScreenFractions` (the
  spirits' "Invalid Y" / "Invalid X").
- **Creating**: `CreateScriptObject` (the sub-type 5000 rule for every type but `ObjectType::Timer`,
  `ObjectType::SpellDispenser` and the others that ignore it; `ObjectType::Scaffold`, `ObjectType::Timer`,
  `ObjectType::Vortex` and `ObjectType::Whale` made as above; `ObjectType::Abode` refused), `CreateTimer`,
  `FlockCreate` and `CreateHighlight` (also refused for a row past the four); none sets the script's debug name.
- **Finding**: `FindForScript` (CALL, CALL_NEAR: the original's messages kept as openblack's diagnostics, "Thing not
  found" at debug level, since the scripts' waiting loops ask every turn) and `FindCreatureForScript`
  (the game's list of creatures, the newest first, as `Creature::Initialise` 0x474350 puts each new one at the head of
  `Creature::CreatureList` and `FindCreatureAtPos` / `FindCreatureNearPos` walk it from the head: openblack's
  `saved_slot::CreaturesInListOrder`, by the creation numbers; test `ScriptQueryNatives.CallNearOfACreatureTakesTheNewestOfTheGameListFirst`;
  the sub-type is taken as the creature's species), `CallNear`, `CallNearInState` (`FindForScript`'s `inState`: `LivingFinalState`, then the type and sub-type,
  then the state; no distance test; ✅ faithful, the same rule as raffclar's `773c6b8e`; tested by `test_script_query_natives`,
  `CallNearInState...`), `CallIn`, `GetNearestTownOfPlayer`.
- **Containers** (`ECS/ScriptContainers.h`): `FlockAttach`, `FlockDetach`, `FlockDisband`, `IdSize`,
  `ChangeInnerOuterProperties`, `GetFirstInContainer` / `GetNextInContainer` (`script_containers::FirstMember` /
  `NextMember`, which keep no cursor: the member after the script's thing in `ForEachMember`'s order, a found
  thing; tests `FlocksTest.FirstAndNext*`, `FlocksTest.FirstInATown*`, `FlocksTest.FirstInADance*` and
  `ScriptQueryNatives.FirstAndNextInContainer*`). `ObjectDelete` disbands a container before the delete mode runs, not after it
  (⚠️ approximate: openblack destroys the entity at once, the original only marks it).
- **Properties**: `GetProperty` and `SetProperty` (the scale's `GET` only for a villager, an animal or a creature,
  its `SET` of any thing with a place, the age
  only for a villager or an animal, the altitude of any thing ([Properties](#properties)): the rest is pending); `SetScriptUlong`; `OverrideStateAnimation` (without the
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
  drawn focus: 🔍 inferred), `GetInclusionDistance` (the distance the camera's fence step works out, `DefaultWorldCameraModel` from
  `CameraZones.cpp`; 1e10 with no fence), `GameThingFieldOfView` and `PosFieldOfView` (`Camera/FieldOfView.h`, no multiplayer case);
  `SetFocusFollowComputerPlayer` and `SetPositionFollowComputerPlayer` pop their argument and log "not
  implemented".
- **The help spirits**: `SpiritEject`, `SpiritHome`, `SpiritAppear`, `SpiritDisappear`, `SpiritPointPos`,
  `SpiritPointGameThing`, `LookGameThing`, `LookAtPosition`, `StopPointing`, `StopLooking`, `PlaySpiritAnim`,
  `SpiritPlayed` (true when the help system is absent: nothing plays), `ClingSpirit`, `FlySpirit`,
  `SpiritScreenPoint`, `SpiritSpeaks` (the local player's alignment, read: openblack's local player is always
  PLAYER_ONE; see Speaking above).
- **The interface**: `ClearClickedObject`, `ClearClickedPosition`, `PositionClicked` (no multiplayer case),
  `SetInterfaceInteraction` (`Help/InterfaceInteraction.h`), `SetInterfaceCitadel`, `GetHandState` (the hand
  system's interface hand state), `IsPlayingHandDemo`, `SetHelpSystem`, `GameCloseDialogue`, `FadeFinished`,
  `GetEventsPerSecond`, `GetTimeSince` and `GetTotalEvents` (`Help/HelpProfile.h`).
- **Highlights, effects, town and player**: `HighlightProperties`, `SetDrawHighlight`, `SpecialEffectPosition`,
  `SpecialEffectObject`, `SetTownDesireBoost` (`ecs::town_desire::ScriptSetTownDesireBoost`),
  `GetTownAndVillagerHealthTotal` (`script_containers::TownAndVillagerHealthTotal`: the town's abodes in
  `town_stats::AbodesOf` order, each one's `Abode::inhabitants`, then the homeless, every life through
  `ecs::life::LifeOf`; tested by `test_script_flocks`), `GetAlignment`,
  `RandomUlong` (the synchronised stream).
- **Sound, music and film**: `SetCreatureSound`, `SetGameSound`, `EnableDisableAlignmentMusic`, `MoveMusic`,
  `SetMusicPlayPosition`, `MusicPlayed191`, `SetAviSequence` (the film played by `Video/VideoPlayer.h`; the fade is
  always cleared, as in the original), `PlayJcSpecial`, and `DllGettime` (the VM's own `PushElaspedTime`).

### Test hooks

- `OPENBLACK_SCRIPT_THING_TRACE=1`: one line per MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and per true
  PLAYED (to follow `FollowUs`).
- `OPENBLACK_SCRIPT_NATIVES=1`, or the debug bar's Debug > Scripts > Trace natives (off by default): one line per
  native call on the `scripting` logger, `native <NAME>(<arguments>) -> <results>, turn <turn>, task <script>`, the
  arguments as the native took them from the stack and the results as it pushed them (an int as its number, a float as
  its shortest text, an object as `obj:<id>`, a boolean as true or false; `ScriptHeaders/NativeTrace.h`, test
  `NativeTrace.*`). A quest run's log is read against the quest's rows by `tools/checks/story/quest_check.py`.
- `OPENBLACK_TEST_TEXT_CLICK=1`: every turn, if a text is waiting for the click (`RUN_TEXT` with interaction 1), it
  does the left click (`HelpSystem::ProcessInterface(true)`, which ignores it until the text has shown for 1 s).

## Pending

### SET_POSITION

- The +0x24 bit 4 is set only by the magic hand's add (see IN_HAND under
  [Properties and flags](#properties-and-flags)), so a creature's hold never sets it; a computer player's magic hand
  would, and openblack, which has no hands for the computer players, refuses only the object the player's hand holds.
- An Object whose kind openblack never puts in the map cells (the whale among them) counts as in the map, so it is
  not refused; whether the original's has its in-the-map bit is not read.
- The matrix takes `GetScale`; openblack takes the Transform's uniform scale, which for a citadel heart and a magic
  shield is their drawn scale, not their object scale.
- The things that are not objects, other than a flock (a town, a timer, a mist, an influence ring, a weather thing, a
  puzzle game, a spell), have their Transform position set; what each reads of its +0x14 is not checked.

### Other commands

- **PLAYER (21)** of the other classes that have a player of their own (fields, features, spells, rewards, artefacts
  and others) gives the unwritten property's 0.
- **GET_PROPERTY SPEED of a dead creature** out of the physics: the original gives 0 while the creature's plan is
  dying (row 9) or lying dead for good (rows 217 and 261, `Creature::IsDead` 0x47CD70); openblack gives the speed its
  kind is made with. No shipped script reads a speed (every one of the 181 SPEED statements is a `SET`); it waits for
  the mind's own dying plan, which the forced rows 9 and 217 are being ported for.
- **ANGLE, XANGLE and ZANGLE**: a thing that keeps no angles of its own (one made by a path that does not keep them,
  or turned by one, such as the hand) reads the angles of its turn (`placed_turn::AnglesOfTurn`), which differ from
  the original's kept ones in the last bits for about one angle in ten: of 200 000 random float angles, the Y angle
  read back from the turn `affine::AngleY` made differs for 13.8% (10.0% once turned to degrees), and 1.76% of
  neighbouring floats have the same cosine and sine floats.
- **SET_PROPERTY HEALTH**: on a pile, a tree or a crop field it is not ported (their own `SetLife`s); the
  original's protection of a nearly dead thing (`Object::SetLife`) holds for every caller, openblack applies it only
  to the scripts' `SET` (`ecs::life::SetLife` itself does not).
- **SET_PROPERTY YPOS**: a creature's matrix is not rebuilt (its Transform's turn and scale are left as its own code
  keeps them); whether the original's creature has a Game3DObject at +0x40 is not read. The turn-start position is not touched,
  as in the original, so the drawn thing moves between turns as openblack draws any move; whether the original's
  draw slides the same way is not checked.
- **SPEED of a creature**: `GET` of a dead creature out of the physics is 0 in the original while its plan is dying
  (row 9) or lying dead for good (rows 217 and 261, `Creature::IsDead` 0x47CD70); openblack keeps no dead creature,
  so it gives the speed its kind is made with. No shipped script reads a speed (every one of the 181 SPEED statements
  is a `SET`); it waits for the mind's own dying plan. `SET` writes the follower's target speed
  at once in the original; openblack's locomotion makes it from the fraction on the creature's next turn, which is
  when the original's follower next reads it (🔍 the order of the script and the creature within a turn is not
  compared).
- **`DataForScriptRemind`** (the walk a villager resumes after a script state) is not ported.
- **The immersion and lion and sheep puzzles**: not ported, so GET_OBJECT_STATE has no outcome of theirs to give.
- The intro's missing parts are listed on [The Land 1 intro](land1-intro.md#pending).
