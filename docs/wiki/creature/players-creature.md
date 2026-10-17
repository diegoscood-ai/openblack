# The player's creature

## Summary

The creature a profile brings to a land. A land's script loads it with `LOAD_MY_CREATURE` from the profile's mind file
in `Scripts\CreatureMind`; it lives in one slot per player, its home follows the temple's pen, and it is drawn smaller
inside the pen. At a land change the original saves its mind and physique over the profile's files, deletes it with
the land, and the next land's `LOAD_MY_CREATURE` makes it again from them. Land 2's rival creatures are made by the
challenge script with `LOAD_CREATURE`, named, scaled, taught and befriended by other natives.

openblack loads the profile's creature (a setting, `--creature-file`), keeps the slot on the player, follows the
temple's pen, draws it at the pen size, saves it at a land change and makes it again, and runs Land 2's natives. Its
creation draws are its own mind's, and some readers of the drawn size still differ.

## The original

The creature classes have only headers in bw1-decomp; the functions below were read in the exe.

### The player's creature

#### Loading the profile's creature

- **`LOAD_MY_CREATURE`** (native 250, `GScript::LoadMyCreature` 0x6FD260) pops z, y and x, drops y, and calls
  `GGame::LoadMyCreatureIntoNewMap(cell x, cell z)` 0x552AC0 (the Mac's name; `fn_00552AC0` in the exe's symbols) with
  the high words of `ftol(metres × 6553.6)`. It is that function's only caller; the lands' scripts reach it with their
  own `LOAD_MY_CREATURE` (Land 1 ip 68025, Land 2 ip 84316, Land 3 ip 113848, VortexEntry ip 128).
  1. If the local player has a creature (`GPlayer` +0xA4C), nothing.
  2. The file is the local `LHPlayer`'s +4 string (interface +0x1BC), or "Dummy": the profile's registry value
     `HKCU\Software\Lionhead Studios Ltd\Black & White\LHMultiplayer\Profiles\<profile>\file`, read with
     `LHNetGetCurrentProfileString("file")` (0x66BF5B). It is a `C<8 hex>.erc` name the profile keeps; it is not a
     CRC32 of the profile's name.
  3. `Creature::Load(name, MapCoords(cells), player, NULL)` 0x4E7FF0 → `LoadFullyQualified(".\scripts\CreatureMind\" +
     name)` 0x4E8040: the version into 0xCAB174, then the species row (< 17), `Creature::CreateCreature(coords,
     &CreatureInfo[row] (0xC60460 + row × 0x394), player)` 0x474B50, +0x1058 = (start data and not 1), the path to
     +0x1228, `CheckValidation` from version 0x15, then the mind (`fn_004E84A0`, which builds the body through
     `Update3DCreatureFromAttributes` at 0x4E8A05). A missing or empty file or a bad row: no creature, no message.
  4. `SetFizz(1.0, 0, false)` then `SetFizz(0.0, 3.0, false)` (0x47AB90): sound tag sfx 0x27 at the creature and the
     fizz at 1, then a fade to 0 at -1/3 per second (+0x12AC lock, +0x12B0 current, +0x12B4 target, +0x12B8 rate). No
     `rand()`.
  `MapCoords(cell, cell)` 0x602FC0 puts the point in the middle of the cell (low words 0x8000), altitude 0.
- **`CURRENT_PROFILE_HAS_CREATURE`** (463, `GGame::CurrentProfileHasACreature` 0x555A30): whether
  `.\Scripts\CreatureMind\<that file>` exists.
- **`CreateCreature`** 0x474B50 returns NULL when the player already has a creature; otherwise `Creature::Create`
  0x474A20 (memory zeroed by `Base::new` 0x4366F0, so the development phase +0x1268 starts at 0). The home +0x1200 is
  the player's +0xA48+0x30 object's position (the citadel's), else the creation point; the creation point is also
  copied to +0x1214. The size is the species' +0x1FC. Its draws are all on the synchronised stream, in the order and
  counts given under [Random streams](../engine/random.md#random-streams): the Living constructor's age (`GameRand(11)` at 0x5EBF89, the
  start age being 23), strength and fatness ± 0.1 (`GameFloatRand(0.2)` at 0x4EF3FA and 0x4EF426: (r + the species'
  strength) − 0.1 and (r + 0.5) − 0.1), each desire's decay and increase time (0x4DC14E, 0x4DC346: 80), two per desire
  source (0x4DE1B2, 0x4DE21B: 116), 18 to 36 for the personality in `fn_004E2E70` (0x4E2E93..0x4E33F6) and 328
  `GameRand(120)` in `fn_004F1FF0` (0x4F201F): 545 to 563. All but the first three are gated by [0xD00DEC], which is
  1; `SpreadReaction` (0x6E3F18) draws nothing then, and a range of 0 draws nothing. Loading from a file adds 2 per
  saved source (0x4E870D).

#### The Living block: age and strength

A creature's `Living` fields +0x9C (strength) and +0xA0 (birth turn) are write-only state [V, a static scan]:

- `Living::Living` 0x5EBEC0 writes +0x9C = ftol(info+0x12C) (0x5EBF62), then +0xA0 = 0 and calls `Living::SetAge`
  0x5ED2C0 directly with age = startAge − startAge/4 + GameRand(startAge/2) (0x5EBFA7 / 0x5EBFAD; one GameRand,
  "Living.cpp" line 0x40, the draws 0x5EBF54..0x5EBFAD). It is called from 0x473B9F, the Creature(MapCoords, info)
  constructor, whose symbol is mislabelled `SetupLandscapeBox::ControlCallback` at 0x473B90 and which sets the vtable
  0x8CC810 at 0x473D50; the rest of that constructor (0x220 bytes) does not touch +0x9C or +0xA0.
- `Living::SetAge` writes +0xA0 (0x5ED2D8); for a creature its only caller is that constructor (its other direct
  callers: `Animal::SetAge` at 0x4179D9 and `Villager::SetAge` at 0x752A75). `Living::SetToZero` 0x5EC030 writes
  +0x9C = 0 (0x5EC045), from the Living constructor at 0x5EBF4F and inlined in the Creature default constructor (used
  by load) at 0x4746D3.
- `Living::Save` 0x5EE630 writes both as 4 raw bytes each (0x5EE6BD / 0x5EE6F6), adding each value's low byte plus 4
  to GameOSFile+0x214, a running sum that looks like a save checksum; `Creature::Save` calls it at 0x4E4EB2.
  `Living::Load` 0x5EE960 reads them back the same way (0x5EE9EB / 0x5EEA2A), from `Creature::Load` at 0x4E6002.
- `Living::GetAge` 0x5ECAF0, the only reader of +0xA0 for game logic, has no direct caller, and a creature's vtable
  (0x8CC810 against Living's 0x8CD394) sends slot 0x8D0 to `Creature::GetAge` 0x47B1A0 (`[[this+0x160]+8]`, the
  physical block's age) and slot 0x8D4 to `Creature::SetAge` 0x47B1B0 (the same field; never +0xA0). Their virtual
  users: `GameStats::GetCreatureAge` 0x565340 (`jmp [vt+0x8D0]` at 0x565351), and `GScript::GetProperty`'s age getter
  (0x70E01C, after `dynamic_cast<Living>`) and setter (0x70EB95, `ftol` then 0x8D4). No vtable slot holds a getter for
  Living +0x9C; the only strength functions are `CreaturePhysical::SetStrength` 0x4F0100 and `ModifyStrength`
  0x4F0170, on another field.
- Not these fields, though at the same offsets: `Creature::ProcessState` 0x473805 / 0x473823 (an object at
  [ebp+0x482C]); `Update3DCreatureFromAttributes` 0x4772FC, `ApplyEffect` 0x478E7E and `ReduceLife` 0x47DD85 (the
  LH3DCreature); `CreatureDesires::Initialise` 0x4DC12A; `FleeFromSpellPriority` 0x4F2AD6 and
  `Living::LookAtNiceSpellPriority` 0x5F16C5 (Spell+0xA0); `SetupReactToNice/NastyMagic` 0x4F3120 / 0x4F32F0;
  `SubStatePerformAddVillagersToDance` 0x501D94 / 0x501DE1; the decision tree arrays fn_004D7290 and its neighbours.
  The age divisor [0xD01A04] is read only by fn_004179F0 (Animal), `Living::GetAge`, `Living::SetAge` and
  `Villager::CheckChildGrownUp` 0x7510DD; none of the 54 `dynamic_cast<Living>` and 128 `dynamic_cast<Creature>`
  sites (RTTI 0x9C8DE8 / 0x9C8060) reads +0x9C or +0xA0.

So the one effect beyond the stored bytes is the constructor's GameRand draw; a byte-exact save would also need the two
values in the Living block. The scan covered named class functions and the ranges 0x470000–0x510000 and
0x5EB000–0x5F4000 in full; other code only by the [0xD01A04] and cast scans.

#### The slot

The player's creature (`GPlayer` +0xA4C) 📼 1.20 only is one slot per player, the neutral one included (`GGame::GetPlayer`
0x5509B0 answers players 0..7). Its writers: `GPlayer::GPlayer` (0x648E05) and `GPlayer::SetToZero` (0x64937E) put
0; `GPlayer::SetCreature` 0x64B4F0 puts the creature (0x64B4F4), then `ReflectCreatureInTowns` 0x64B510
(`TownCentre::SetPlayersCreature` 0x743BD0 for each town of the player); the creature's destructor fn_00475170 puts
0 in the slot of its own player (+0x1070) **without checking the slot holds it** (0x47526F..0x475279, and without the
towns' call); `GScript::CreatureSetPlayer` 0x6F41C0 writes the local player's (0x6F423B). `SetCreature` is called by
`Creature::Initialise` whenever the creature has a player (0x47439D..0x4743A2), by `SwapMinds` (0x47C317, 0x47C323)
and by `LOAD_CREATURE`'s fn_0065BA40 (0x65BB5F), so the newest creature made for a player is theirs, whichever they
had. `Creature::CreateCreature` 0x474B50 makes nothing for a player whose slot is full (0x474B58..0x474B60); the
feature script's `CREATE_CREATURE` (0x716F03..0x716F4A), the map script's `CREATE_CREATURE`
(`GSetup::MapCommandProcess`, 0x714E30..0x714E7F), `Creature::LoadFullyQualified` (0x4E8257) and the packets
(0x63C581, 0x63C70C, 0x63C754, 0x63D9DD) go through it. `Creature::Create` 0x474A20 does not look at the slot:
`Config::ProcessOneGameTurn` tests the local player's slot first (0x46B42B..0x46B436), fn_004F6610 picks a player
whose slot is empty (0x4F6624), and `LOAD_CREATURE` and `CREATE_CREATURE_FROM_FILE` (fn_0065BA40, `Create` at
0x65BB51) do not test it, so their creature takes the slot. The neutral player's slot names the last neutral creature
made: the playgrounds `0- SandBox Creature Training`, `Kapa's Land1 Playground` and `The Ultimate Sandbox Map` make
several, none refused.

The removal: the destructor runs after `Creature::ToBeDeleted` 0x474F00, which has no write to +0xA4C. In the
original `GInterface::UpdateAllLeashes` (0x5D9185..0x5D919A) and `DrawAllLeashes` (0x5D93F3) go through the player's
+0xA4C, so with the slot at 0 the leash is neither updated nor drawn; neither clears the leash status
(`GLeashStatus` +0x14) there.

#### The creature's files

- **`Physique<file>`** in the same folder is written by `Creature::Save3D` 0x4E70C0 (every 600 ticks for the local
  creature when interface +0x1FC and +0x1058 are 0, on `ToBeDeleted` and on `ClearMap`) and read only by the front
  end's tattoo editor (0x4E7310, from 0x54284A); never by `LOAD_MY_CREATURE`. Layout: int species; floats size
  (LH3D +0x90), strength (physical +0xC), fat (physical +0x14) and alignment; then two counted dword lists (skin marks,
  LH3D +0x5184). `PhysiqueC4ba71b36.erc`: Mandrill, size 2.0, strength 0.84212, fat 0.4, alignment 0.118912, 0 and 3
  entries.
- **`Profiles\<name>\creature.lhp`** is written by `fn_0047B440` with the same 600-tick save: the creature's values
  into `.\scripts\creature_html.tmpl` (`%s\html\%s.html`), then `GenerateCreatureHTML` `fn_00578330` writes an
  "LHNILD" archive with a "CREATURE" section and up to five `creatureshot_%d.jpg`. Nothing reads it back. Its header
  (`fn_005776E0`) is the game's only `srand(time)` (0x577721), followed by a `LocalRand(0x8000)` (0x577731).
- `GGame::GetNextPlayerWithNoCreature` 0x550A30 has no callers (it also ignores its argument).

#### The natives Land 1 reaches on it

| Native | Original | Pops, in order | Does |
|---|---|---|---|
| 197 `CALL_PLAYER_CREATURE` | 0x6F3D40 | player | the game player's creature (+0xA4C) through `AddScriptGameThing(c, 0)`; none: "No creature of player %d" and 0 |
| 223 `SET_CREATURE_HOME` | 0x6F4A20 | z, y, x, the object | a creature: +0x1200 / +0x1204 = `ftol(x × 6553.6)` / `ftol(z × 6553.6)`, +0x1208 = 0 (y dropped) |
| 208 `SET_CREATURE_DEV_STAGE` | 0x6F4820 | the stage (an int), the object | a creature: `MoveToDevelopmentPhase(stage, 0)` |
| 205 `DEV_FUNCTION` | 0x6FC130 | the function (an int) | the table below |
| 282 `CREATURE_IN_DEV_SCRIPT` | 0x6F5680 | the object, the value | a creature: +0x10C0 = the value as popped |

The object natives print "Thing not found!" / "Thing not creature!" and do nothing for anything else.

Who reads the two fields [V]. +0x10C0 (`CREATURE_IN_DEV_SCRIPT`) is read by `Creature::SendCandidateHelpScript`
(0x4CA113: while it is set, a help script is sent only with an object), by sub_4C58E0 (0x4C598A, which answers "free"
when it is 0; a table entry with no static caller), saved by `CreatureMental::SaveMind` (0x4E7EA5) and shown by
`Creature::DisplayStatistics` (0x4767EA). `SetToZero` (0x474249), `GScript::SetCreatureInDevScript` (0x6F56F3) and the
13 CreatureHelp starters sub_4CAC40..sub_4CAE80 (each starts its help script and sets it to 1) write it. +0x1108 is
the first of the four leash-known ints (+0x110C..+0x1114 the evil, rope and good leashes): `GGame::DoAction` reads them
by leash type (0x63FE50, 0x63FEE0) and `SaveMind` saves them (from 0x4E7EB6); `SetToZero` (0x474311),
`GPacket::ProcessPacket` (0x63DA64) and fn_0065BA40 (0x65BBA3) write them.

**`DEV_FUNCTION`** (`SCRIPT_DEV_FUNCTION`, bw1-decomp `include/chlasm/ScriptEnums.h`; jump table 0x6FC4E4), on the
local player's creature:

| n | Name | Does |
|---|---|---|
| 1 | START_DEVELOPMENT_SCRIPTS | `MoveToDevelopmentPhase(0, 0)`, the home copied to +0x11A8, +0x11B4 = 12.0 (no null check) |
| 2 | ROPE_LEASH_ENABLED | +0x1110 = 1 |
| 3 | OTHER_LEASHES_ENABLED | +0x1114 = 1 (0x6FC233), +0x110C = 1 (0x6FC25C): the good and evil leashes known; no slot is written |
| 4 | LOAD_MY_CREATURE | `SetPacket(0x4E)` (0x6FC270) |
| 5 | RESET_ESCAPE_STATE | `TutorialState` = 1 |
| 6 | MY_CREATURE_POINT_OUT_HIGHLIGHT | a spiral search for a `ScriptHighlight` that is not a did-you-know, then a forced plan |
| 7 | CLEAR_INTERACTION_MAGNITUDE | mind +0x18C5C = 0 |
| 8, 9 | MY_CREATURE_CAN_DIE, CANNOT_DIE | +0x1158 = 1, 0 |
| 10, 11 | CREATURE_HELP_ON, OFF | nothing; 42 calls of `fn_004CA580` |
| 12 | ENTER_SAVEGAMEROOM | `GoInsideCitadel(5, 0)` |

Packet 0x4E (handler 0x63D86B, jump table 0x63DDCC) makes or loads the sender's creature at its citadel's home
(`CLANCREATECREATURE<nn>` start data: `CreateCreature` and phases 0-12; otherwise `Creature::Load`, at 0x63DA4F),
then sets +0x1108, +0x110C, +0x1110 and +0x1114 to 1 and phase 13: multiplayer and skirmish only.

#### The development phases

`MoveToDevelopmentPhase` 0x4C59C0: when confined, unleashed and at phase 5 or more, +0x11B4 = 0; the phase to
+0x1268, +0x126C = 0; every desire unsuppressed and deactivated; then for each phase from 0 to the one asked, its "add"
list (10 slots) activated and its "remove" list (4 slots) deactivated (42 = empty); +0x194 = 0 and the agenda cleared.
No draw. The table is `CreatureDevelopmentPhaseEntry[14]` at 0xC84878 (stride 0x84) from `info.dat`'s
DETAIL_CREATURE_DEVELOPMENT (records of 0x74 bytes from 0x10B6C; the add list at +0x4C in memory, the remove list at
+0x7C). Adds by phase: 0: 5, 8, 9, 17, 18, 21, 24, 28, 35, 23; 1: 4, 6, 29, 38, 3; 2: 15, 19, 20, 36; 3: 14; 4: 33, 7;
5 and 6: 16, 37; 7: 31; 8: none; 9: 0, 10, 27, 13; 10: 2; 11: 1; 12: 0, 10, 27; 13: 1, 32, 34, 39, and 29 removed. At
phase 13, 33 desires are active; off are 11, 12, 22, 25, 26, 29 and 30.

#### A script's focus

`SET_FOCUS` on a creature (`GScript::SetFocus` 0x6F90B0): a creature is first put under script control (vtable
+0x440, bit 0x400 of +0x24), then `Creature::SetFocus(LHPoint)` 0x4F6760 (vtable +0x510; other objects take
`Object::SetFocus` 0x6393A0, the yaw snap). It stores the point in the mind (+0x1D3F4 = +0x1D3F8 = 1, the point at
+0x1D400), `PrepareCreatureForScriptedAction(1)` 0x4F6A90 (script control, `FinishActionUnsuccessfully`, a scripted
plan of desire 0x18 and action 0x16 into mind +0xF48, the sub-action agenda at mind +0xFA8 reset), then queues
sub-action 7 "TurnToFacePos" with `LookAtPosition` 0x4D1460. Each turn its phase 0 (0x501660) is done when the point
is less than 0.1 away or within π/8 of the creature's heading; otherwise `StopMoving` and `StartTurningAction`;
phase 1 (0x501730) waits for the body action to end. No draw on this path (the agenda's `GameRand(7)` at 0x4FF086
needs a second function, which this sub-action has none of). Its release: `RELEASE_FROM_SCRIPT` 0x6FB380 →
`ReleaseControlFromScript` 0x70D540 → `ReleaseScriptThingIntoTheGame` 0x70F600, script type 12 → 0x4F7060: the focus
off, `FinishActionUnsuccessfully`.

#### The tutorial's creatures

`CreaturesInGlade`, answers 0-2: made by `CREATE` type 12 (`GScript::CreateThing` 0x6F1B20, case at 0x6F149C) with no
player, then `CREATURE_SET_PLAYER` 0x6F41C0 (+0xA4C, +0x1070, the name). Offered: the Cow (row 1), the Ape (row 0) and
the Tiger (row 2) by (2233, 3152) (ip 44076-44096).

#### The home follows the temple

`Creature::ProcessState` (0x472E1A..0x472EA5) rewrites the home +0x1200 / +0x1204 / +0x1208 on every step, with no
state test, whenever the player, `player+0xA44`, the citadel (`player+0xA48`) and its heart (`Citadel+0x30`, a
`CitadelHeart`) exist: the heart's `Game3dObject` (`Object+0x40`) special point #15 (vtable +0x1CC with edx = 15,
`Game3DObject::GetSpecialPos` 0x63B040) in world space, x and z × 6553.6 truncated, altitude 0. So `SET_CREATURE_HOME`
only lasts while there is no heart. On Land 1 point #15 of `b_first_temple.l3d` is the last of its 16 extra metrics,
local (-9.318, 0, -20.437); with the heart at (1915.05, 2508.89) turned 36.0 rad (`CREATE_PLANNED_CITADEL` angle
36000 × 0.001) it is (1895.97, 2520.75), 0.87 m from the script's HomePos.

#### Drawn smaller in the pen

`Creature::ShrinkDownIfNearCitadel` (its Mac name; `fn_004EFCE0` in the exe's symbols, called only from
`Creature::ProcessState` at 0x472F72) takes the real scale (`[+0x160]+0x6C`, read there directly at 0x4EFCEC;
`GetScale`, vtable +0x120 0x47B190 → `CreaturePhysical::GetUserSize` 0x4EF4F0, reads the drawn +0x90 instead) and,
when the owner's citadel heart is built (`CitadelPart::IsBuilt` 0x464AD0, vtable +0x890), the distance `d` from the
creature to its home +0x1200 is at most 16 m (`GUtils::GetDistanceInMetres` 0x74CD70) and the creature is between
the pen's walls, draws it at `0.22 + (real − 0.22) × (clamp(d, 14, 16) − 14) / 2` (constants 0x8D14C4, 0x8D14C8,
0x8D1520) through `LH3DCreature::SetSize` 0x480530; elsewhere at its real size. The walls: θ0 = the heart's
`GetYAngle` (+0x4C, radians as stored) + 3.83, θ1 = θ0 + 0.897598 (2π/7); with dx, dz the drawn creature (+0x78,
+0x80) minus the heart's point in metres, inside is `cos θ0·dz − sin θ0·dx ≥ 0` and `sin θ1·dx − cos θ1·dz ≥ 0`
(the heart's arms 0 and 1). The real size is never written, so it comes back as the creature walks out; no draw; no
state. In the Creature Cave (`CreatureRoom::InitEngine` 0x787BD8) the creature's copy takes the real size. On Land 1
the home is inside the pen (9.78 and 9.01; 0.87 m), so from turn 8 the creature is drawn at 0.22.

#### Everything SetSize derives

Because `ShrinkDownIfNearCitadel` calls `LH3DCreature::SetSize` 0x480530 every turn (all its paths end at
0x4EFE59..0x4EFE69), with the pen size in the pen and the real size `[physical+0x6C]` elsewhere, every value SetSize
sets is the pen's while the creature is there. With `arg` its argument and `s` = ClampScale(arg) (0.05..4):

| Address | Field (names from the sync log `fn_0048F830`'s strings) | Value |
|---|---|---|
| 0x480533..0x48054F | local rate | 1.6 − (arg × 0.5) × 0.85 = 1.6 − 0.425·arg |
| 0x480553..0x48055F | local f, stored as a float | (arg × 0.5) × 1.75 + 0.25 = 0.875·arg + 0.25, **arg unclamped** |
| 0x480599 | +0x90 UserSize | s |
| 0x4805A1..0x4805B1 | +0x94, the drawn scale | s × 15 / +0x8C |
| 0x4805B7..0x4805C1 | +0x4844 WalkSpeed | f × 8 |
| 0x4805C7..0x4805D1 | +0x4848 RunSpeed | f × 20 |
| 0x4805D7..0x4805F0 | +0x4838 CurrentSpeed | min(current, RunSpeed) |
| 0x480608 | +0x484C Acceleration | 12 |
| 0x4805FE..0x480612 | +0x4858 (head look) | rate × +0x485C |
| 0x480618..0x480622 | +0x4850 BodyTurnAccel | rate × π |
| 0x480628..0x48063C | +0x498C / +0x4990 breath | 5 / (1 / √s) = 5·√s |
| 0x480642..0x48064C | +0x5228 radius | +0x94 × +0x5224 ([The creature's radius](body.md#the-creatures-radius)) |
| 0x480652..0x4806A7 | leash lengths | `fn_005E6980` per untied leash |

The size at +0x90 is 0.05 when the size is not above 0.05 (`fcomp` [0x8AC3F4] 0.05f, `test ah, 0x41` at 0x48056F, so
also for a NaN), else the size when it is below 4 (`fcomp` [0x8AB418] 4.0f, `test ah, 1` at 0x480580), else 4.

The walk reads them so: the gait 0x48D940 (called from the walk step 0x49145E with the turn's distance) blends stand
and walk below WalkSpeed with weight speed / +0x4844, and walk and run above it with (speed − walk) / (+0x4848 −
walk); its stride is the animation's stride × **+0x94** × the weight (0x48D9BA..0x48D9C3, 0x48DA59..0x48DA63), and the
walk advances by walk duration × distance / stride. The step-or-walk choice divides the distance by +0x94
(`fn_00483AB0` 0x483CB7), and a step moves IntTimeInc × **+0x94** / duration × displacement a frame (0x490F79). So
in the pen the creature walks at the pen size's speeds with the small body's stride: at the same fraction of its top
speed its feet cycle in proportion to (0.875·s + 0.25) / s, 2.011 at 0.22 against 1.125 at size 1 and 1 at size 2.

The other readers of the size it is drawn at: the animations' playback rate (`UpdateTime` 0x481E45..0x481E66, 1.6 −
0.425 × +0x90, also the turning's and the object actions'; the frame's step is kept as an integer, AnimTimeInc
+0x48BC = ftol(ms × rate)); the breath period; the leash lengths (`fn_005E6980`, refreshed each turn);
`Creature::GetHeight` (0x477F50, +0x90 × 15) and `GetScale` (0x47B190 → `GetUserSize` 0x4EF4F0, +0x90); the hand's
holding and touch height (`ObtainRequiredHandPosition`, through GetHeight); the follow camera's height
(`CameraModeFollow::Update` 0x44C541, 0x44C6C8, 0x44C744); the sound size key (`fn_00483290` 0x483337..0x483360,
clamp(ftol((2 − +0x90) × 1.5 + 1), 1, 3)); the footprint's side (0x483582) and the hair's scale (0x4ED548). The duel's
too: `AttemptAttack` 0x489CE0 takes each blow's reach and height from a per-blow table times +0x94 (so in mesh units;
0x489F68, the height against the opponent's), the back (anim 0x8A) and forward (0x89) steps' strides × +0x94
(0x48A1E3, 0x48A20C), and lands a blow only between 0 and 7.5 × +0x90 deep (0x48A21A). The opponent's height there is
`fn_004867B0`, its head bone in the drawn bone buffer (`fn_004813B0`, bone +0x51B8) above the ground, so it follows
the pen size as well. The fight's action step 0x48A5E0 moves the body by the animation's displacement × rate ×
IntTimeInc × +0x94 / duration (0x48A651); its blow test casts rays 0.2 × 15 × +0x90 long, twice that for anim 0x82
(0x48A96B), from the blow's bone points against the opponent's drawn body (`fn_00483160`, `fn_008683C0`); the height
share a blow lands at is the table's height × +0x94 over `fn_004867B0` (0x48AD89); the damage factor is (+0xAC +
3)/(the opponent's +0xAC + 3) × +0x90 / the opponent's +0x90 × the blow's speed, within [0.04, 2], doubled for 0x82
(0x48B064..0x48B0CA).

#### A land change: the creature saved and made again

- **The save.** `GGame::ClearMap` 0x552BB0 (from `GGame::LoadMap` 0x552F40, which `LOAD_MAP` calls; see
  [Random numbers](../engine/random.md#the-seeds) for the name) begins by saving the local player's creature (the
  player at g_game+0x205A59, its creature +0xA4C; 0x552C3A..0x552D54): only when there is one, the player has a
  leader interface status, and creature +0x1058 is 0. The name is the local `LHPlayer`'s +4 string (interface
  +0x1BC), the profile's `file` value. Then:
  - `CreatureMental::SaveMind(".\Scripts\CreatureMind\" + name)` 0x4E7820 (0x552CE9, the format string 0x9CF968);
  - `Creature::Save3D(".\Scripts\CreatureMind\Physique" + name)` 0x4E70C0 (0x552D54, the format string 0x9CF944).
  So the file is the profile's own mind file, the same one `LOAD_MY_CREATURE` and `CURRENT_PROFILE_HAS_CREATURE`
  read, written over at each land change. The name is joined as it is, also when it is empty: both savers open their
  path with `LHOSFile::Open` 0x7BC730 mode 0 (`CreateFileA`, `GENERIC_READ|GENERIC_WRITE`, `CREATE_ALWAYS` or
  `TRUNCATE_EXISTING`), and on a failed open they only clear their "open" flag (SaveMind 0x4E78BC, Save3D 0x4E7149)
  and go on without writing, so the second saver is called whatever the first did. With an empty name the mind's path
  is `.\Scripts\CreatureMind\`, the folder, which `CreateFileA` cannot open: no mind file is written; the physique's
  is `.\Scripts\CreatureMind\Physique`, a file of that name, which is written.
- **+0x1058** is 0 for a creature of the campaign: `LoadFullyQualified` sets it to "start data given and its byte not
  1" (0x4E8271..0x4E8282; `LOAD_MY_CREATURE` gives none), the constructor's helper fn_00474130 to 0 (0x474136), and
  the multiplayer creature packet to 1 (0x63DA16). So only a multiplayer creature is not saved.
- **The physique file** (`Save3D`): u32 the species row (`CreatureInfo` +0x1F4), f32 the 3D body's size (+0x90, the
  size it is drawn at, so the pen's smaller size when it is there), f32 strength (physical +0xC), f32 fatness (+0x14),
  f32 the alignment as saved (fn_004F5FA0: the value before a nice spell, else before a nasty one, else the
  creature's), then fn_00481040: a count and that many u32 from the 3D body's +0x5184 list (+8), and a second count
  and list (+0x1008). `PhysiqueC4ba71b36.erc` is 14, 2.0, 0.84212, 0.4, 0.118912, 0 and 3 entries.
- **The creature goes with the land.** `GlobalGameLists::ClearMap` 0x591520 (0x552DBF) first calls `ToBeDeleted(0)`
  on every creature (the list g_game+0x205BBC, next +0xA4; 0x591537..0x591555), and the creature's destructor
  fn_00475170 clears its player's +0xA4C (0x475279). So the next land's `LOAD_MY_CREATURE` finds no creature and
  loads it from the file just saved: Land 2 at ip 76347 (`Land2VortexEntry`), Land 3 at ip 103099 (`SetupLand3`), and
  `VortexEntry` at ip 106. This corrects the note that the creature goes with the player and the next
  `LOAD_MY_CREATURE` does nothing.
- **Skirmish.** The original carries no profile creature into a skirmish: the start mode 4 (`GGame::Init` 0x54F716)
  runs `ResetAndStartPlaygroundGame` 0x555990 and no CHL; `Creature::Load` 0x4E7FF0 is called only from fn_00552AC0
  (`LOAD_MY_CREATURE`, 0x6FD2D7) and the creature packet 0x4E (0x63DA4F), which only `DEV_FUNCTION(4)` sends and
  `challenge.chl` never calls. None of the 19 scripts in `Scripts\Playgrounds` calls `LOAD_MY_CREATURE` or makes a
  creature for `PLAYER_ONE`; their creatures are the computer players' and neutral ones (`CREATE_CREATURE_FROM_FILE`).

#### Land 2's creatures

Khazar's and Lethys's creatures are not made by `Scripts\Land2.txt` (no creature command among its 39 kinds; it only
switches the two computer players on and builds their citadels at (2509.12, 1779.93) and (1006.36, 3677.02)). The
challenge script makes them: `LandControlAll` runs `LandControl1` (ip 155485) and **then** `LandControl2` (ip 155490),
which runs `EnterLand2` (ip 102763, the arrival scene), `EndEnterLand2` (ip 76137) and
`SetUpComputerPlayersAtStartOfLand2` (ip 75933, script id 268, address 74836):

| What | Khazar | Lethys |
|---|---|---|
| Marker (`CREATE` type 1, subtype 0) | (2540.93, 78.97, 1916.63), ip 74845 | (1050.88, 147.59, 3607.17), ip 74855 |
| `CREATURE_AUTOSCALE` factor | 1.2 | 1.5 |
| Script player | 2 (game PLAYER_TWO) | 3 (game PLAYER_THREE) |
| Runs (async) | `SetupKhazarCreature` (id 87, address 12414), ip 74890 | `SetupLethysCreature` (id 88, address 12779), ip 74894 |
| `LOAD_CREATURE` type | 7 = `CREATURE_TYPE_TORTOISE` | 4 = `CREATURE_TYPE_WOLF` |
| Mind file (data offset) | "KhazarCreature" (348) | "LethysCreature" (363) |
| `SET_CREATURE_NAME` text id | 5002 | 5001 |
| Alignment | = MyCreature's (ip 12768) | = -MyCreature's (`NEG`, ip 13133) |
| `CREATURE_FORCE_FRIENDS(1, it, MyCreature)` | yes (ip 12772) | no |

The species numbers are bw1-decomp `include/chlasm/CreatureEnum.h` (`CREATURE_TYPE`, Ape = 0). Both Setup scripts, in
order: `LOAD_CREATURE(type, file, player, GET_POSITION(marker))`; `It = CALL_PLAYER_CREATURE(player)`;
`SET_CREATURE_NAME`; `SET_CREATURE_DEV_STAGE(It, 13)`; `CREATURE_AUTOSCALE(1, It, factor)`; 45
`CREATURE_SET_KNOWS_ACTION(It, type, action, 1)` (type 0: actions 0-6; type 1: actions 1-6 and 10-41); STRENGTH (17)
and properties 26-33 set to 0.2; `RELEASE_FROM_SCRIPT(It)`; then a loop `while not THING_VALID(MyCreature):
MyCreature = CALL_PLAYER_CREATURE(1); SLEEP 5`; then the alignment (18) and, for Khazar, the friendship; and
`RELEASE_FROM_SCRIPT(It)`. `SetupLand5` runs Nemesis's twin, `SetupNemesisCreature` (ip 140952); `SetupLand3` runs
Lethys's again (ip 103147).

- **The mind files** `Scripts\CreatureMind\KhazarCreature`, `LethysCreature` and `NemesisCreature` are byte-identical
  (5268 bytes, version 25, species row 0, name "Matey"): the species comes from the script.
- **`LOAD_CREATURE`** (native 329, `GScript::LoadCreature` 0x6FD2F0) pops z, y (dropped) and x, and keeps the point as
  `MapCoords{ftol(x × 6553.6), ftol(z × 6553.6), altitude 0}`: the full fixed point, **not** the cell's middle that
  `LOAD_MY_CREATURE` uses. Then the player (`ftol`, `ConvertScriptPlayerToGamePlayer` 0x6EB9A0), the file name and the
  type (the raw integer). A player with a creature (+0xA4C) gets "Player has already his creature loaded, " and the
  native **carries on**. A player without a computer-player object (+0x944) gets "Player has not a tocomputer player
  field" and nothing; `GPlayer::Init` 0x649190 makes that object for every player it sets up (0x6491C0..0x6491FD).
  Else `fn_0065BA40(type, &coords, name)` on that object.
- **`fn_0065BA40`** opens `".\Scripts\CreatureMind\%s"` (0x9CF968); a file it cannot open: nothing. Else
  `Creature::Create(coords, &CreatureInfo[type] (0xC60460 + type × 0x394), owner = the computer player's +0x28)`
  0x474A20 (the type is not range checked), `fn_0064B4F0` (the player's +0xA4C = it, then `fn_0064B510`), the file
  name to +0x1228, `CreatureMental::LoadMindIntoPreexistingCreature` 0x4E8440 (the version to 0xCAB174, the species
  dword read and **ignored**, `CheckValidation`, the mind `fn_004E84A0`, then `fn_004C9FE0(phase + 12, 0, 0, 0, 0)`),
  +0x1108 = +0x110C = +0x1110 = +0x1114 = 1, `MoveToDevelopmentPhase(13, 0)`, and the file closed.
- **`Creature::Create`** 0x474A20 with a player and out of the land's bounds: NULL. With the owner's citadel (+0xA48)
  the home +0x1200 is its heart's (+0x30) special point 0 (vtable +0x1CC, edx = 0) in the fixed point, altitude 0; a
  citadel without a heart, or without the point: NULL (after construction). Without a citadel: the creation point.
  `ProcessState` then rewrites the home to point 15 every step ([The home follows the temple](#the-home-follows-the-temple)).
- **`SET_CREATURE_NAME`** (native 343, `GScript::SetCreatureName` 0x6F5BE0) pops the text id, then the object
  (`GetScriptGameThing`: no thing, nothing more). Not a creature: "Thing not creature!" and nothing. A creature:
  `wcscpy` of the help text database's entry into `Creature::name` (+0xE0, 64 UTF-16 characters, not bounded by the
  copy); the entry is the id's when 0 < id < the database's count (unsigned compare), else entry 0. In the Spanish
  data text 5001 is `HELP_TEXT_LETHYS_CREATURE_NAME_01` "Laetes" and 5002 `HELP_TEXT_KHAZAR_CREATURE_NAME_01`
  "Khalen"; the name replaces the mind file's ("Matey"), which `LOAD_CREATURE` read just before. The native is not
  Land 2's only: in `challenge.chl` it is also called by `SetupNemesisCreature` (5000), `CreateGuideCreature` (5003,
  the Guide; run by `MeetTheGuide`, `MoveTheGuideAround`, `TheStorm`, `GuideImpressTownLesson`,
  `GuideImpressTownNextLesson` and `CreatureDevGuideTeachesFight`), `CreatureGuardian` (5005; run by `LandControl1`),
  `Land4Ogre` (5005), `BlindWomanMain` (5006, the old woman's gremlin) and `TheBigFightMain` (5007).
- **`CREATURE_AUTOSCALE`** (native 452, `GScript::CreatureAutoscale` 0x6F6080) pops the size factor, the object
  (`GetScriptGameThing`) and the enable flag. No thing, or a thing that is not a creature (vtable +0x34, `IsCreature`):
  "Thing should be valid!". Any thing that is there, a creature or not (the second `IsCreature` call's result is not
  tested), then gets +0x115C = the enable flag (the raw dword) and +0x1160 = the factor. The fields are read only by
  `fn_0047DC50`, called from `Creature::ProcessState` at 0x472FB7, after the creature's spells (`ProcessSpells`,
  0x472EBB) and `ShrinkDownIfNearCitadel` (0x472F72) and only when the creature is not in a locked select (`Flags
  +0x24 & 0x10`, tested at 0x472F77: with it the turn goes another way and skips the step). It does nothing in a
  multiplayer game (`GGame::IsMultiplayerGame`), with +0x115C zero, or when the local player (`GGame+0x205A59`) has no
  creature (its player's +0xA4C). Else, in float: `v = (base · factor − own) · 0.5 + own`, where `own` is the
  creature's real scale (`[+0x160]+0x6C`) and `base` = `fn_004F5E60` on the local creature's `CreatureReceiveSpell`
  (+0x370): while a size spell is on it (`IsCreatureSubTypeSpellActive(0)`), its size before big (slot 2, +0x40) if
  big is on, else before small (slot 1, +0x28) if small is on; else its real scale. The result goes through
  `fn_004F5EA0` on the creature's own `CreatureReceiveSpell`: `v` if `v` compares below 2.0 or unordered (`fcom`
  against 0x8AB478, C0 tested: a NaN is kept), else 2.0; written to before big while big is on, else before small
  while small is on, else to the real scale. The growth comes after the step (0x473337). Land 2's scripts set it to
  1.2 (Khazar's creature) and 1.5 (Lethys's). Its other callers in `challenge.chl`: on Land 1
  `CreatureDevGuideTeachesFight` (the Guide on at 1.2, ip 25767, and off with 0, ip 26730) and
  `CreatureGuardianScale` (the Guardian at 1.2, ip 41638); then `SetupNemesisCreature` (its `CreatureSize` local, ip
  13168), `Land4Ogre` (1.1, ip 121180) and `TheBigFightMain` (1.0, ip 136614).
- **`CREATURE_SET_KNOWS_ACTION`** (native 71, `GScript::CreatureSetKnowsAction` 0x6F3960) pops whether it knows, the
  action, the kind (`CREATURE_ACTION_LEARNING_TYPE`: 0 a skill, 1 a miracle by its magic type), then the object
  (`GetScriptGameThing`). No thing: "No creature for script" and nothing. A thing that is not a creature (the dynamic
  cast to `Creature` fails): "No script for creature", and the call then goes on with a null creature (0x6F39F4),
  which the original does not guard. `Creature::ScriptSetKnowsAction` 0x4F7090 calls
  `CreatureActionsKnownAbout::SetKnowsAction` 0x4E27F0 on the mind's +0x1A9FC: knowing, `AddActionLearnt(creature,
  kind, action, notify 1)` 0x4E28C0, which appends the action to the kind's list (+8 + kind · 8, count at +0xC + kind ·
  8) unless it is in it, and, with a creature, shows the learning message (`fn_004C9FE0` code 6 for a skill, 7 for a
  miracle, the action as its index); not knowing, the action's node is unlinked from the list and freed (absent:
  nothing). The list takes any action number: no check against the tables. Then, knowing and kind 1 only, the mind's
  miracle sightings (+0x17D38 + action · 4) = `__ftol(NumTimesMagicActionMustBeSeenBeforeItIsLearnt(action))`
  (0x4F8CA0: `fild` of the row's times to see, `fmul` by the species' multiplier at `CreatureInfo + 0x37C`, 0 for an
  action of 42 or more; on the 24-bit x87 it is the float product, cut towards zero). The miracle's last-seen turn and
  a skill's watching are not touched; a miracle taken away keeps its sightings. Land 2's Setup scripts teach skills
  0-6 (the table has rows 0-5) and miracles 1-6 and 10-41.
- **`CREATURE_FORCE_FRIENDS`** (native 309, `GScript::CreatureForceFriends` 0x6F5770) pops one object (`a`, the last
  argument: `MyCreature` in `SetupKhazarCreature`), the other (`b`, `It`) and the enable flag, each object through
  `GetScriptGameThing`. Either missing: "Thing not found!" and nothing. Then "Thing not creature!" for `a` if it is not
  a creature (vtable +0x34, `IsCreature`), and again for `b`; either not a creature: nothing. Two creatures with the
  flag 0: nothing (no friend is taken away). Else `a`'s friend list (+0x3D0, a singly linked list of 8-byte nodes
  {next, creature}, its count at +0x3D4) is searched for `b`; absent, a new node is put at the list's **front** and the
  count goes up; then the same for `a` in `b`'s list. So each is in the other's list once, the newest friend first,
  and a creature made its own friend is in its list once. Readers (from the x86's references to +0x3D0, not followed
  further): `Creature::CanBeAttackedByCreature` 0x4E4280 answers 0 when either is in the other's list (after the same
  test on the list at +0x12A0); `CreatureMental::GetActivityObjectUsefulness` (0x4E9A26) gives a usefulness of 0 for
  an object in the list (the branch around it not read); `CreatureMental::RemoveBeliefsAboutDeletedObjects`
  (0x4D8898) unlinks a deleted friend (its `+0xA` bit 0) from its own creature's list. Its callers in `challenge.chl`:
  `SetupKhazarCreature` (ip 12772, after the alignment) and `OgreFamilyControl` (ip 122166 and 122170).

##### Creature properties from a script

`GET_PROPERTY` / `SET_PROPERTY` on a creature (natives 21 and 22). `GScript::GetProperty` 0x70DAE0 pops the object
(`GetScriptGameThing`), then the property; no thing: "Thing no longer valid" and a float 0; a script container
(vtable +0x3F8) with any property but PLAYER (21): a float 0; else `dynamic_cast<Creature>` and the jump table
0x70E78C on `property − 1`. `SET_PROPERTY` 0x70F380 pops the value, the object and the property; no thing: "Object no
longer valid"; a script container: the setter run on each of its members through the type's loop function; else
`fn_0070E820` (`CastCreature`, vtable +0xA4; jump table 0x70F2BC). The creature's body is `CreaturePhysical` at
+0x160, and every value is pushed as a float (VM type 2). The cases:

| Property | `GET` | `SET` |
|---|---|---|
| STRENGTH (17) | 0x70E081: physical +0x0C | 0x70EBA6: `CreaturePhysical::SetStrength` 0x4F0100 |
| ALIGNMENT (18) | 0x70E4F0: [+0x168]+0x08 | 0x70EF0C: [+0x168]+0x08, then `Update3DCreatureFromAttributes` 0x477210 and `UpdateMorphing` 0x618C40 on physical +0x58 |
| WARMTH (26) | 0x70E210: physical +0x10 | 0x70ED56: +0x10, kept to −1..1 |
| FATNESS (27) | 0x70E26C: +0x14 | 0x70EDBE: +0x14 |
| ENERGY (28) | 0x70E2C8: +0x1C | 0x70EDF5: +0x1C, kept to 0..1 |
| ITCHINESS (29) | 0x70E324: +0x20 | 0x70EE59: +0x20 |
| AMOUNT_OF_POO (30) | 0x70E380: +0x2C | 0x70EE7C: +0x2C |
| EXHAUSTION (31) | 0x70E3DC: +0x30 | 0x70EE9F: +0x30 |
| DEHYDRATION (32) | 0x70E438: +0x34 | 0x70EEC2: +0x34 |
| FIGHT_HEALTH (33) | 0x70E494: `GetCreature3D` 0x477850 +0x4AA8 | 0x70EEE5: `GetCreature3D` +0x4AA8 |
| SCALE (8) | 0x70DD4C: `GetScale` (vtable +0x120) = `Creature::GetScale` 0x47B190 → `CreaturePhysical::GetUserSize` 0x4EF4F0: the 3D body's +0x90 | 0x70EA76: `SetScale` (vtable +0x124) = `Creature::SetScale` 0x47B160: `LH3DCreature::SetSize` (vtable +8, 0x480530) with the value (0x47B173), then physical +0x6C = the value (0x47B180) |
| HEIGHT (19) | 0x70E567: a `PuzzleTotem` (`__RTDynamicCast`) gives its +0xE8, an int, as a float; a totem statue (vtable +0x1E4) its `GetWorshipSpeed` 0x738260; anything else `GetHeight` (vtable +0x42C) = `Creature::GetHeight` 0x477F50: the 3D body's +0x90 × 15 (0x8C2C40) | 0x70EA93: a `PuzzleTotem` fn_006DA6D0; a totem statue its town's `SetWorshipPercentage` 0x73C060 (the statue's own 0x738270 without a town), then +0x88 = `GetWorshipSpeed`; anything else `SetHeight` (vtable +0x144) = `Creature::SetHeight` 0x47DE80: s = value × [0x8C9D38] (0x3D888889, the float one fifteenth, multiplied) to physical +0x6C and to the 3D body's +0x90, within no limit |

- **The clamps** are the only ones: the value is stored, then `fcomp` with the lower limit (C0, so a NaN too) puts
  the lower limit (−1, 0x8AB678; 0 with `ebx`), else a value above 1 (C0 and C3 clear, 0x8AA390) puts 1. Strength,
  alignment, fatness, the other needs and the fight health are stored as given.
- **Not a creature**, properties 26-33: "Object not a creature" (0xC20964; `SET` at 0x70F287), and `GET` pushes a
  float 0. STRENGTH on anything else: `GET` gives a one-off spell seed's +0x6C (vtable +0xAC), a spell seed's +0x88
  (vtable +0x4C4), a particle container's +0x34 (vtable +0x4AC) or a spell's +0xE4 (vtable +0x400), else
  "Script-Did you want the strength of this" and its life (vtable +0x11C); `SET` the same fields (the particle
  container through fn_0063E3E0), else "Cannot set strength of this". ALIGNMENT on anything else: `GET` gives its
  player's `GetAlignmentValue` 0x64D6A0 (vtable +0x1C), a float 0 without one; `SET` gives a reward (vtable +0x3F4)
  +0x78, else "UNEXPECTED Alignment change".
- **`SetStrength`** 0x4F0100: +0x0C = the value; then the body's weak-strong morph (physical +0x58, +0xAC) =
  `fn_004F02B0(value, [[+0x54]+0x28]+0x1F4)` = (`CreatureInfo` row's +0x12C (0xC6058C + row · 0x394) · 0.2 + value ·
  0.8) · 2 − 1, −1 at or below −1 (or NaN), 1 at or above 1.
- **The fight health** (+0x4AA8 of the `LH3DCreature`) is kept outside fights. `LH3DCreature::Init` sets it to 1
  (0x480227, `ebp` = 0x3F800000 at 0x47FDBC); a fight's start sets it to (life + 1) · 0.5 (0x49BD8E, 0x49BE45);
  `ReduceLife` 0x47DD00 and `IncreaseLife` 0x47DE20 change it only while the creature has an opponent (+0x528C);
  the function at 0x480380 takes its argument off it (its callers not read); fn_004845F0 takes a quarter of what is
  lost off the life at the fight's end and leaves it as it is. A mind file of version 30 or later carries it
  (`fn_004E84A0` at 0x4E8BE3); `Creature::Save` / `Load` keep it.
- **SCALE and HEIGHT.** Both `GET`s read the 3D body's size +0x90, which `LH3DCreature::SetSize` writes kept to
  0.05..4 (in the pen the pen size, 0.22), not the user size physical +0x6C. `SetSize` also writes the matrix factor
  +0x94 = size × 15 / [+0x8C] (0x4805A1..0x4805B1) and, from the size as given, +0x4838..+0x4858, +0x498C / +0x4990
  and +0x5228, then walks the player's interface statuses for the creature's leash (0x4805B7..0x4806A9; not traced
  further). `SetHeight` writes only +0x6C and +0x90: the matrix factor and the rest wait for the next `SetSize`,
  which `ShrinkDownIfNearCitadel` makes every turn (0x4EFE69). The `SET` of SCALE is the only one through
  `SetSize`.
- In `challenge.chl` (the `PUSHI property; object; CALL 21/22` pairs): on Land 1, `CreatureDevSeeHome`
  (EXHAUSTION), `CreatureDevLearnToEat` (ENERGY), `GuideStayTired` and `GuideImpressTownNextLesson` (EXHAUSTION),
  `CreateGuideCreature` (STRENGTH), `CreatureDevGuideTeachesFight` (EXHAUSTION, FIGHT_HEALTH); on Land 2 the Setup
  scripts above (all ten), and later `CreatureCurse` / `CreatureCurse2`, `MakeCreatureAlignment`,
  `ReturnCreatureBackToNormal`, `HealCombatants`, `FreezeRayStatue` and others.

## openblack

Done for Land 1 and the land change; Land 2's natives are ported but no run reaches them. Porting decisions:
[Port decisions: the creature](../project/port-decisions-creature.md).

#### What openblack does with the player's creature

- **The profile's file is a setting:** `--creature-file <name>` (`EngineConfig::profileCreatureFile`), default
  `C4ba71b36.erc`, the game folder's first profile's creature, so every run loads the same one; empty for a profile
  without a creature. `CURRENT_PROFILE_HAS_CREATURE` answers whether that file exists.
- **`LOAD_MY_CREATURE`** (`ECS/PlayerCreature`): nothing when the player leads a creature already (the leash
  system's `PlayersCreature`); else the file through the mind cache, its species, size, alignment and strength
  (`CreatureMindFileBody`), and `CreatureArchetype::Create` at the middle of the point's cell on the ground. The
  fatness starts as the species does (which known field of the file holds it is not checked). The creature faces π, as
  the script-made creatures do (🔍 inferred). The fizz is not ported. Our creation's draws are our mind's (the 40
  initial desires), not the original's list above.
- **The slot** is `components::PlayerCreatureSlot` on the player's entity, or the player service's copy for a player
  without one (the neutral player always), written by `PlayerSystemInterface::SetPrimaryCreature` and read by
  `GetPrimaryCreature`. Every creature made with a player takes it (`PlayerSystem::AddCreature`, from
  `CreatureArchetype::Create`, as `Creature::Initialise` sets it), the newest winning, the neutral player's included;
  a creature of no player (`_COUNT`) takes none, as `GetPlayer` gives no player past 7. For a player who leads
  creatures (`creature_leash::CanLead`: not the neutral one) the leash follows the slot: the new creature becomes the
  one they lead (`Creature::leashable`, `LeashSystemInterface::ClaimOnArrival`), and the one before stops being it
  and loses its leash. `LeashSystemInterface::PlayersCreature` answers only for such a player, so the leash, the
  shortcuts and Creature Mode never take a neutral creature. The leash coming off the creature that loses the slot is
  openblack's (the original's interface leash status, `GLeashStatus` +0x14, is not read there). `CreateCreature`'s
  refusal has no openblack caller: the feature `CREATE_CREATURE` is not implemented (a log line), the map one throws
  as not implemented, and there is no `LoadFullyQualified` and no packet.
- **The removal:** `creature_removal::Remove` empties the slot as the destructor does: the owner of a creature that
  goes has no creature after, whichever one the slot held, the neutral player included, and no other creature of
  theirs takes its place (`WorldInterface::LosePlayersCreature`). It is the removal's last step, after
  `RemoveFromWorld`. Like the destructor's plain write it calls no `ReflectCreatureInTowns`, so the totems keep their
  icon until the next creature is set. When the slot held another creature of a player who leads creatures, that
  creature stops being the one they lead and its leash comes off: that is openblack's.
- **The land change** (`ecs::player_creature::SaveMyCreature`, called by `Game::ChangeLand` before the land is
  loaded):
  - the creature is the one the local player leads (`LeashSystemInterface::PlayersCreature`), the file the profile's
    (`--creature-file`) in `Scripts/CreatureMind`; nothing without a creature or a mind set up yet. Our creatures have
    no start data, so +0x1058 is always 0. The files are `player_creature::SaveTargetsOf`: with no profile file there
    is no mind file to write and the physique goes to `Physique`, as in the original, and the physique is written also
    when the mind could not be;
  - the mind file is `player_creature::ToMindFile`: what `CreatureMindSystem::SaveMind` gives (the desires, learning,
    opinions, sightings, known skills and miracles, stage, size, strength and alignment, as saved before any spell),
    with the body's fatness (`creature_mind_body::ToMindFile`); the previous fatness, age, energy, needs, tattoos and
    every section the mind does not model are kept from the file the creature was loaded from;
  - it is written through the file system service and the mind cache's entry is dropped
    (`resources::SaveCreatureMind`), so `LOAD_MY_CREATURE` reads it back from the disk;
  - the physique (`components/creaturemind` `PhysiqueFile`, `creature_mind_body::ToPhysique`): the species row, the
    size it is drawn at (`player_creature::PenSizeOf`, the value `ShrinkInPens` draws it with, from the creature's
    turn position, which has not moved since that turn's pen shrink; its own size outside a pen), strength, fatness and
    the alignment as saved, and the skin's two lists kept from the file it replaces (we do not model them). The size
    is the one the 3D body would keep: `creature_morph::ClampScale` (the one size clamp for the 3D body, also used on
    the drawn size before it is written) clamps as `SetSize` does, in the same order;
  - openblack keeps a one-time `<file>.bak` of the profile's mind file and of its physique file (when one exists)
    before it first writes over them, which the original does not (`resources::KeepBackupOnce`); if the backup cannot
    be made, that file is not written over;
  - the registry's reset then takes the creature, as the original's map clear does.
- **Skirmish:** loading "1- The Norse God" in openblack gives one creature, PLAYER_TWO's: in a skirmish the player has
  no creature of their own, in the original as in openblack.
- **`LOAD_CREATURE`** (`ECS/PlayerCreature`, `ScriptLoadCreature`): the pure `PlanScriptLoad` takes the species from
  the script's type as a row of the creature tables (`SpeciesFromRow`: 0 is the Giant Ape; a type out of 0-16 is
  refused, where the original indexes its table unchecked), the body from the file as `LOAD_MY_CREATURE` reads it but
  with the file's own species row ignored (`creature_mind_body::FromMindFile(file, species)`), and x and z through the
  fixed point (`map_coords::Quantise`), on the ground. A player who leads a creature already gets the original's error
  line and the load goes on. The file comes through the mind cache; one that is missing or that our reader refuses
  makes no creature. The creature is made by the same step as `LOAD_MY_CREATURE`'s (`CreatureArchetype::Create`, the
  same draws, facing π), then `SettleScriptLoaded`: `SetLeashable` (+0xA4C: it becomes the one its player leads,
  displacing any other, as every creature made for a player already is), `SetKnown` of the rope (+0x1110), evil and
  good (+0x1114) leashes, and stage 13. It is not held by the script (the native adds no script thing). The script
  player goes through `ScriptPlayerToGamePlayer`.
- **`SET_CREATURE_NAME`** (`ECS/PlayerCreature`, `SetName`): the help text of the id (`helptext::Get`, entry 0 out of
  range) becomes the mind's name (`Learnt::name`). Our mind takes up its file at its first turn, not as the creature is
  made, so a name given before then waits in `CreatureMindState::scriptName` and is put over the file's once the mind
  has taken it up (the original's order: the file first, then the name). No thing: nothing; anything else:
  "Thing not creature!".
- **`CREATURE_AUTOSCALE`** (`ECS/PlayerCreature`, `SetAutoscale` and `Autoscale`; the pure step
  `creature_size::AutoscaleStep`): the native puts `components::CreatureAutoscale {enabled, factor}` on a creature; no
  thing, or anything but a creature: "Thing should be valid!" and nothing (the original writes the two fields into
  whatever thing it is; a non-creature has no such fields here). Each creature turn, right after the creatures' spells
  step and the pen's shrink (`spell_creature::ProcessTurn`, `ShrinkInPens`; the original's order: the spells, the
  shrink, then the step, and the growth after it), `Autoscale` steps every creature that has it on: the local
  player's creature (`creature::LocalPlayer`, the leash system's `PlayersCreature`) gives its size before any size
  spell (`creature_spells::SizeBeforeSpells`), the step starts from the creature's own size now, and the result goes
  through `creature_spells::SetSizeBeforeSpells` (the size big, else small, puts back as it ends, else
  `Creature::size`). The creature the hand holds (`CreatureHandSystemInterface::GetCreature`, an ⚠️ approximate
  stand-in for the locked select) is skipped. openblack has no multiplayer game, so that test is always passed. The
  walk is on the const registry, so without the native no storage is made.
- **`CREATURE_SET_KNOWS_ACTION`** (`CreatureMindSystemInterface::SetKnowsAction`; the pure
  `creature_watching::SetKnows` and `TaughtSightings`): a skill or a miracle becomes known (`Knowledge::skillsKnown` /
  `miraclesKnown`) or no longer known; a miracle taught also gets its sightings set to the float product of its times
  to see and the species' multiplier (`TimesNeeded`, the same one a sighting is measured against), cut towards zero.
  Our mind sets itself up and takes up its file at its first turn, so what a script teaches before then (or while a
  file waits) is kept, in order, in `CreatureMindState::scriptKnows` and put over the file's once the mind has taken it
  up, as the name is; a mind saved meanwhile is saved with it. No thing: "No creature for script"; anything but a
  creature: "No script for creature" and nothing changed.
- **`CREATURE_FORCE_FRIENDS`** (`ECS/PlayerCreature`, `ForceFriends`; the pure `AddFriend`): the native's messages as
  the original's; two creatures with the flag on each get the other once in `components::CreatureFriends::friends`
  (made when first needed), at the front, `b` into `a`'s list first; off, nothing. Nothing reads the list yet, so it
  changes nothing else.
- **`GET_PROPERTY` / `SET_PROPERTY` on a creature** (`ECS/PlayerCreature`, `GetCreatureProperty` /
  `SetCreatureProperty`; the pure `ClampProperty`): strength, alignment and fatness are `Creature::strength`,
  `alignment` and `fatness`; warmth, energy, itchiness, amount of poo, exhaustion and dehydration are the body's needs
  (`creature_physiology::Needs`), read and written through `CreaturePhysiologySystemInterface::NeedsOf` / `SetNeeds`:
  before the body's first turn `NeedsOf` gives what the species starts it with, and `SetNeeds` marks the body started
  so that its first turn keeps a script's values (the original sets them as the creature is made). The fight health is
  the fighter's (`CreatureFighting::fighter.health`) in a fight, a script's set included, else
  `components::CreatureFightHealth` (made by a script's set outside a fight, or by `CreatureFightSystem::Leave` from
  the fighter's health as the creature leaves any fight, ended or not, as the original's one field stays), else 1.
  Warmth is kept to −1..1 and energy to 0..1 in the original's order (a NaN gives the lower limit); the rest as given.
  The body shows a new strength or alignment from the drawing's next refresh of its morph
  (`creature_morph::FromAttributes`, the same weighting as `SetStrength`), not in the native itself. No thing: "Object
  no longer valid" (`SET`) / "Thing no longer valid" (`GET`, 0); anything but a creature, 26-33: "Object not a
  creature" (and 0); STRENGTH and ALIGNMENT on anything else are not ported (logged once, `GET` gives 0). Only the six
  needs go through the physiology service: a creature whose body has no needs (no service, or no `CreatureNeeds`)
  logs an error for them and `GET` gives 0, which the original, where every creature has its body, never does.
- **SCALE and HEIGHT on a creature** (`CHLApi`; `player_creature::SetCreatureScale` / `SetCreatureHeight`, the pure
  `SizeForHeight`): `SET` SCALE draws the body (its `Transform`) at
  `CreatureArchetype::DrawnScale(species, creature_morph::ClampScale(v))` and then sets `Creature::size` = v, as
  given; `SET` HEIGHT sets `Creature::size` = h × `k_SizePerHeight` (the float one fifteenth, multiplied: 45 gives
  3.0000002, not 3) and leaves the `Transform` as it was (the original's drawn matrix waits for the next `SetSize`).
  The `GET`s on a creature are not ported (each logged once as GET_PROPERTY's unwritten case with its property's
  number, then counted; both give 0): the original reads the 3D body's size, which ours does not yet keep apart from
  `Creature::size` (see Pending). SCALE on a villager or an animal stays the `Transform`'s scale; `SET` SCALE on
  anything else is not ported ("SetProperty (Scale)"), `SET` HEIGHT on anything but a creature falls to the generic
  "SetProperty" line, as before. The fidelity, cycle and hand-demo logs have neither a "SetProperty (Scale)" nor a
  generic "SetProperty" line, so no run reaches either `SET` on a creature (among the scripts that would,
  `CreateGuideCreature` and `CreatureDevGuideTeachesFight` set the Guide's SCALE on Land 1).
- **`SET_FOCUS` on a creature** is ported (`CHLApi` SetFocus → `FocusByScript`, a container's creature member the same
  way), and so is its release (openblack `ReleasedByScript` after the flag is cleared). Another object's yaw snap is
  ported too ([a villager's speed and scripts](../beings/villager-speed-and-scripts.md)).
- **The natives:** `CALL_PLAYER_CREATURE` gives the creature the player leads; `SET_CREATURE_HOME` writes the leash's
  home (`LeashSystemInterface::SetHome`), x and z through the 16.16 fixed point, on the ground; `SET_CREATURE_DEV_STAGE`
  sets the mind's development phase, and the desires follow in the mind's next turn (the original at once);
  `DEV_FUNCTION` 2 is `SetKnown(Rope)`, 3 is `SetKnown(Evil)`, `SetKnown(Good)` and `SetLeashable`, where the original
  writes no slot; openblack's `SetLeashable` on the creature its player already leads changes nothing; the other
  values are not ported; `CREATURE_IN_DEV_SCRIPT` sets `Creature::inDevScript`.
- **The home:** each creature turn starts with `FollowTemplePens` (`ECS/PlayerCreature`, called from
  `ECS/CreatureLoop`): while its player's temple is built, every creature's home is the temple mesh's pen point
  (`TemplePenPoint`, special point 15), through the fixed point and on the ground. `TempleCreatureHome` (where a
  knocked-out creature is carried) takes the same point; it took the mesh's first point before.
- **The pen:** `ShrinkInPens`, right after `FollowTemplePens`, writes the drawn scale to
  `components::CreatureDrawPose::scale` and the pen size to `CreatureDrawPose::size` (`BetweenPenWalls`,
  `PenDrawnSize`), none outside a pen; `ecs::DrawnModel` draws a creature at it, its eyes, hair and footprints
  following ([Drawing between turns](drawing.md#drawing-between-turns)). `creature_pose::DrawnSize` is the one reader of the
  size it is drawn at (the pen size, else `Creature::size`). The locomotion takes its walk and run speeds from it and
  its stride, its step-or-walk choice and its steps' travel from the drawn scale (`creature_pose::DrawnScale`), as
  SetSize's +0x4844, +0x4848 and +0x94 ([Everything SetSize derives](#everything-setsize-derives)); `SpeedsFor` takes
  the size unclamped, as SetSize's f. The radius it keeps clear by stays its own size's. Also from `DrawnSize`, as the
  original reads +0x90: every animation's playback rate (`creature_layers::PlaybackRate`, 1.6 − 0.425 s: the body's
  layers in `PoseBody`, the turning, the object actions; ours keeps the float `ms × rate` where the original keeps an
  integer: `PoseBody`'s playback time and the object actions' `k_TurnMs × rate` a turn, so ours runs up to just under
  1 ms a frame ahead), the breath period (5·√s), the sound size key, the leash lengths, the creature's
  `object::GetScale` and `GetHeight` (s × 15) and through them every caller of those, the hand's touch (its capsules
  and its height), the follow camera's height, the hair's scale and the footprint's side. The duel too, as the
  original's attack choice and its fight action step read +0x90 and +0x94: its blows' reaches and heights are measured
  at the drawn scale, a step's length and an action's move take the drawn scale, a blow may land at most 7.5 of the
  attacker's drawn size deep, the opponent's height (which blows reach it, and how high on it one lands) is 15 of its
  drawn size (standing in for `fn_004867B0`), a blow lands within 0.2 × 15 of the attacker's drawn size of the
  opponent's body, drawn at its drawn scale, and the damage goes by the two drawn sizes (`DrawnSize` and `DrawnScale`
  in each). The head still looks from its own size's height (the original's look origin is not read).
  `Creature::size`, the Transform and the state hash's size keep the real size; their readers (the fight's arena, its
  places, the distance a fight is picked from, the faint's length, the leash's anger reach, the head's look height) do
  not see the pen size (the hand touches the drawn body, so it touches a smaller one there). The distance is taken from
  the creature's turn position, not its drawn one.
- **The "creature" part of the state hash:** each creature's owner, species, leashable, inDevScript, body, size,
  development phase and home.

### Test hooks

- Debug > Windows > Creature spawner (`Debug/CreatureSpawner*`, its pure parts in `CreatureSpawnerModel`): spawn, the
  creatures and the selected one's looks, audio, movement, hands, mind, learning, body, leash and fight; and at the
  top the player's creature (real and drawn size, phase, home, leash, desires and plan, needs, alignment, the hand),
  with "bring to the hand", "set stage" and the leashes. Its file dialogs, rope drawing, leash posts and editor
  hosting are not ported. Closed, it does nothing.
- `--creature-file <name>` picks the profile's creature.

## Pending

- **The player's creature** ([The player's creature](#the-players-creature)):
  - The fizz after `LOAD_MY_CREATURE` (sound 0x27 and the 3 s fade); its sound tag's own values are not read (queued
    fix F-15).
  - Which field of the mind file holds the fatness (the physique's unnamed floats), and whether the mind load
    (`fn_004E84A0`) sets the development phase.
  - `MoveToDevelopmentPhase`'s other steps (+0x194, the agenda cleared) and its immediate effect; its clear of
    +0x11B4 is read ([The tied lengths](leash-rope.md#the-tied-lengths-two-creatures-leashed-together-and-the-young-creatures-home)).
  - `DEV_FUNCTION` 1 and 4-12 (+0x10C0, which a version 24+ mind file sets, and +0x1108, the first of the four ints a
    version 27+ file sets, are read as above).
  - The slot: what the player's interface leash status (`GLeashStatus` +0x14) keeps when the slot changes under it,
    and so whether a creature that takes the slot later comes up with the leash on, is still open: the 19 callers of
    `GLeashStatus::SetOn` 0x5E6F70 are listed (ToBeDeleted 0x47502F, Faint 0x476FEF, SwapMinds 0x47BF14 / 0x47BF44,
    the packets 0x63D404 / 0x63D478, DetachObjectLeash 0x6F46AE and others), but the slot change's path is not traced.
  - The Living block: what GameOSFile+0x214 is used for (a checksum checked on load?) was not traced; a `Living*` that
    reaches +0x9C by another path cannot be ruled out by the static scan; CreatureSpells and GLeashStatus were covered
    only by the range and cast scans.
  - A land change ([A land change](#a-land-change-the-creature-saved-and-made-again)): the creature is deleted and
    made again from its file, as the original does; only reading the file back is left: `LOAD_MY_CREATURE` does not
    take the file's fatness (ERC_DESIGN E2) or its tattoos yet (queued as a fix), so the fatness saved is not read
    back and the live tattoo slots stay empty (the save keeps the file's tattoos for that reason); once it takes them,
    `ToMindFile` should write the live slots. The body's needs, age and energy are not written (their units in our
    model are not checked against the file's). When the profile names no file the original saves to the profile's
    string as it is; ours saves nothing.
  - Skirmish: a look at the retail front end (a skirmish started with a profile that has a creature) would close the
    last doubt.
  - The tutorial's own creatures (`CreaturesInGlade`), which our runs do not reach.
  - **`GET_PROPERTY` / `SET_PROPERTY` on a creature** ([Land 2's creatures](#land-2s-creatures)): not ported are
    STRENGTH on spell seeds, spells and particle containers, ALIGNMENT of a thing's player and of a reward, the "Did
    you want the strength" life fallback, and a script container (the original gives `GET` 0 and runs `SET` on each
    member; ours treats it as a thing that is not a creature). The fight health a version 30+ mind file carries is not
    read (ours starts at 1; the profile's version 33 files have it); queued as a fix. Which species field
    `SetStrength` reads (the row from `[[+0x54]+0x28]+0x1F4`, the field at +0x12C of the in-memory row) is taken to be
    our `CreatureArchetype::SpeciesStrength`, from the matching formula, not by offset. The 3D body takes a new
    alignment or strength at its next morph refresh, the original in the native. None of our runs reaches them
    (`challenge.chl` and the run logs). Every creature-property `GET` in the scripts `LandControl1` and `SetupLand1`
    run is followed by its `SET`, but those in `CreatureDevLearnToEat`, `CreatureDevGuideTeachesFight`,
    `HealCombatants` and `CreatureGuardianFight`; the fidelity, cycle and hand-demo logs (taken before this port, when
    every creature property still fell to `SET_PROPERTY`'s default) have no "SetProperty" line, so none of the former
    runs. Of the latter, `LandControl1` runs the first two only while `IsSkippingCreatureGuide` is 0 (ip 68042,
    68102; `CreatureDevelopmentSet` and `CreatureGuideSet`, which also run them, are run by no script); our runs'
    start-up answer 3 makes `SetupLand1` set it to 1 (`CAN_SKIP_CREATURE_TRAINING`, ip 22718-22723), so
    `CreatureDevSeeHome` also jumps past its EXHAUSTION `GET` and `SET` (ip 6096). `HealCombatants` is run only by
    `CreatureDevGuideTeachesFight`; `CreatureGuardianFight` only by `CreatureGuardian` (ip 42908), after its ALIGNMENT
    `GET` and `SET` (ip 42461-42465), which wait for `CreatureDevelopment_Fight` = 1, set only by
    `CreatureDevGuideTeachesFight`. So the one "GetProperty" line, logged at turn 9 right after the first "SetFocus
    (object)" one, is the `GET` of some other property, not identified (it is logged once, so later ones are not seen
    either). It is not `CreatureDevSeeHome`'s YPOS `GET` (ip 6037): that reads the highlight just made, which logs
    nothing, and a `SET` that logs (ip 6040) would follow it otherwise. A game started with another answer reaches
    `CreatureDevSeeHome`'s EXHAUSTION first, then `CreatureDevLearnToEat`'s ENERGY and the Guide's; not measured.
  - **SCALE and HEIGHT on a creature**
    ([What openblack does with the player's creature](#what-openblack-does-with-the-players-creature)): the `GET`s on
    a creature are not ported. The original's read the 3D body's size (+0x90: kept to 0.05..4 by `SetSize`, the pen
    size in the pen, but written unclamped by `SetHeight`), which ours does not keep apart from `Creature::size` until
    the drawn size is; then SCALE reads it as it is and HEIGHT × 15. `SET` HEIGHT's write of that size is left out for
    the same reason. After a `SET` HEIGHT the original draws the new size from the next turn's `SetSize`; ours from
    the next change of the drawn size, until it is refreshed every turn. In the pen a `SET` SCALE draws the original's
    body at the set size until that turn's pen shrink; ours keeps the turn's pen pose (`CreatureDrawPose::scale`).
    `SetSize`'s other fields (+0x4838..+0x4858, +0x498C / +0x4990, +0x5228 and the leash's interface statuses) are
    not traced to what reads them. Not ported either: HEIGHT on a puzzle totem and every other thing but a totem
    statue ([The village totem](../magic/village-totem.md#the-village-totem-the-grip-the-drag-and-the-let-go)), `GET` SCALE on
    anything but a villager or an animal, `SET` SCALE on anything but a villager, an animal or a creature.
  - **Land 2's creatures** ([Land 2's creatures](#land-2s-creatures)): no run reaches them. The map cycle only loads
    `Land2.txt` while the challenge script stays in `LandControl1`, so `LandControl2` and its `LOAD_CREATURE` never
    run (no "not implemented" line for it in the cycle's log; Land 2's turns hash an empty "creature" part). They need
    the land change through the challenge, which also brings the player's creature: without it both Setup scripts wait
    on `THING_VALID(MyCreature)` for ever and never set the alignment or the friendship. Not ported yet: the
    computer-player natives. What the version 25 files give through `fn_004E84A0` (size, strength, alignment) is the
    same open point as `LOAD_MY_CREATURE`'s. A file the original opens but our reader refuses would still make a
    creature there (with whatever mind `CheckValidation` leaves); ours makes none. Nor does ours ever refuse to make
    the creature where `Creature::Create` gives NULL (out of the land's bounds with a player, the owner's citadel
    without a heart, or the heart without point 0): `CreatureArchetype::Create` always makes one, so in those cases
    ours has a creature where the original has none (queued as a fix). The home: `Create` takes the heart's point 0
    and `ProcessState` point 15 at once, ours point 15 from the first creature turn (queued with the home, above). Who
    reads +0x1108; `fn_004C9FE0` after the mind load. The texts 5001 and 5002 were read in the Spanish data only. Ours
    takes up the mind file at the mind's first turn, so the stage `LOAD_CREATURE` and `SET_CREATURE_DEV_STAGE` set
    before it is then replaced by the file's (capped at 13), where the original moves to stage 13 after reading the
    file; the name (`SET_CREATURE_NAME`) is kept over the file's, the stage is not yet. The original writes the name
    into the creature at once, whatever its mind is doing; ours keeps a waiting name until the mind's turn, which only
    walks creatures that also have an animation and a transform, so a creature made without them (for instance the
    Guide or the Guardian, should our `CREATE` make them so) would keep the name waiting. A mind saved while the name
    waits is saved with it; a mind not set up yet is not saved at all.
  - `CREATURE_SET_KNOWS_ACTION`: (a) ours keeps what is known as one flag per row of the game's tables, so a kind other
    than 0 and 1, or an action past its table (Land 2's scripts teach skill 6, the skill table has rows 0-5), is
    dropped, where the original's list keeps it (and saves it in the mind file); what reads a skill 6 in the original
    is not read. A file's out-of-table entries are dropped the same way when ours takes a file up. (b) The original's
    list is in the order the actions were learnt; ours saves the file's own order, then the rest by number. (c) The
    learning message (`fn_004C9FE0` codes 6 and 7) is not shown: ours has no learning messages yet, for sightings
    either. (d) Ours does nothing for a thing that is not a creature, where the original goes on with a null creature.
    (e) What the script teaches waits in `scriptKnows` until the mind's first turn, and that turn (as for the name
    above) only walks creatures that also have an animation and a transform: a creature made without them never sets
    up what it knows, so the edits wait for ever and none is applied, where the original applies each at once. (a)-(e)
    are queued as fixes.
  - `CREATURE_AUTOSCALE`: ours steps right after the spells, as the original does (`ProcessSpells` 0x472EBB, then
    0x472FB7), so a size spell's easing and its end meet the step in the original's order. Easing reads the spell's
    `before`, and the spell's end writes `size = before`, so the other order would change every value while big or
    small eases or ends on an autoscaled creature, not only the first. Still open: (a) the original runs the whole
    turn creature by creature (`ProcessState`), so a rival processed before the local creature reads the local
    creature's size before that creature's own spells of the turn; ours runs every creature's spells first, then every
    step. The order of the creatures in the original's list is not read. (b) Ours runs the pen (`ShrinkInPens`), the
    physiology's growth and the other creature systems before the spells and the step, the original runs the pen after
    the spells and before the step, and grows the creature after the step (0x473337; see
    [Growth, size and age as time passes](body.md#growth-size-and-age-as-time-passes)). (c) The drawn size: the original
    draws the creature at the size `ShrinkDownIfNearCitadel` gave it before the step, so a step may show a turn later
    there. Ours rewrites the transform's scale only when the physiology changes the size (`TakeShape`), so a step
    shows at the next turn's growth at the earliest; whether some other call in the original sets the drawn size in
    between is not read. (d) Ours takes the hand's grip on the creature for the locked select's flag (`Flags +0x24 &
    0x10`, tested at 0x472F77; set by packet 0x1B, cleared by 0x1C, with the action states 3..6 in between:
    [The locked select on a creature](../hand/states.md#the-locked-select-on-a-creature); ours takes hold at the
    start through the creature hand's `Grab`); that the flag is set and cleared exactly with that grip is not checked.
  - `CREATURE_FORCE_FRIENDS`: (a) the list is stored only; its readers in the original (a friend cannot be attacked,
    `CanBeAttackedByCreature`; the activity choice in `GetActivityObjectUsefulness`) are not ported, and what the list
    at +0x12A0 that `CanBeAttackedByCreature` tests first holds is not read. (b) The original drops a deleted friend in
    `RemoveBeliefsAboutDeletedObjects`; ours keeps the entity of a destroyed creature in the list (nothing reads it).
    (c) Whether the list is saved with the game or the creature is not read; ours saves it nowhere.
- **The pen size's readers** ([Everything SetSize derives](#everything-setsize-derives)), each queued as a fix: the
  animation frame step kept as an integer (AnimTimeInc +0x48BC = ftol(ms × rate), `UpdateTime` 0x481E45); the duel's
  reaches measured at every attempt (+0x94, 0x489CE0), the back step's own stride (0x48A1E3) and the gap marched in 11
  points (0x489E12); the tied lengths on a tree (6 heights, at most 40, 0x5E6A0F) where ours branches on a mobile;
  the locomotion's stored target speed (0x47FA6E), unclamped run weight (0x48DA43), whole-ms times (0x48D9D1,
  0x48DA71), stride order (0x48D9BA..0x48DA6F) and slope radius (+0x5228, 0x491273). Still open: the press's band on
  the opponent (the hand's fight state is not read); the fighters' places, arrival, pick distance and faint length;
  the step's time (0x483E2A..0x483E47) against ours; the head look rate (+0x4858), the current-speed cap (+0x4838),
  the turning start (0x48122B), the head and feet scales (0x4EC450..0x4EC510) and the body-point kicks (0x482584) are
  not ported; whether the creature walks in its pen on Land 1's runs is not in the logs.
