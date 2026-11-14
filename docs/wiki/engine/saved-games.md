# Saved games

## Summary

A saved game is a slot folder in the player's profile: `Profiles\<profile key>\Saved Games\<slot>` (slots 14 to 18
add a folder for the land). It holds the world file `<slot>.sav`, the script machine's state `Script_<slot>.sav`,
the small `SaveGame.inf` the save room shows, one `SavedCreature<n>` mind file per creature and copies of the
profile's map pictures; the slot's picture goes into texture pages shared by all slots in `Saved Games\Pictures`.

The profile key is `ProfileNameToKey` in `LHMultiplayerR.dll` (read 2026-10-24): each character of the name under
0x80 that `isalnum` takes is an underscore and itself, any other its four lower-case hexadecimal digits, and the key
stops after the character that takes it to 252 or more; the game's own folders (`_J_a_h_o_v_i_a`, `_f_f`) follow it.
openblack: `player_profiles::ProfileKey`, tests `PlayerProfiles.AProfilesFolderIsNamed*`.

The world file is not land-script lines. It is a little-endian binary stream written by each class's own binary
`Save` (and read by its `Load`): a load-attempts counter, the revision and the landscape file, then the game object
and, depth first, every object reachable from it. An object is written whole at its first reference with the next
1-based number, and by that number afterwards. Every object record carries the running checksum of everything
written before it. The game never checks the sum on load, but it lets a reader prove its own layout object by object
on the real saves. The land-script line writers are another thing: they serve the vortex's `vortex.txt` and the land
editor ([The writers](../scripts/land-change-and-saving.md#the-writers)).

openblack reads and writes the slot's creature mind files and the script machine's file byte for byte. Its
`components/gamesave` has the slots' rules, the info file, the picture pages and the world file's stream, with the
records of every class the saves read hold and a reader that walks the file from the game object, proving every record by its sum,
to the file's end on every frozen save, and writes back the same bytes (`apps/savetool`). The debug bar's Load Island /
Saved Games menu loads a slot into play: the land, the scripts, the seeds, the clock, the snow lying, the "did you know" texts read and, of
the world's objects, those listed in [openblack](#openblack). A world writer writes the world file from the entities,
each class by its own writer or from the record its load kept ([Writing the world file](#writing-the-world-file)); the
game saves in play as the original does ([The saves in play](#the-saves-in-play)), and a check never saves or loads:
its profile is not kept.

## The original

Read in the v1.42 `runblack.exe` (the 1.20 symbols through xref; bw1-decomp's C where it has the function:
`GameOSFile.cpp/.h`, `Game.cpp` `GGame::Save`/`Load`, `GameThing.cpp`, `Player.cpp`, `Object.cpp`, `Fixed.cpp`,
`GlobalGameList.cpp`, `SaveGameRoom.h`, `PathCreator.h`) and checked on the saves of the frozen copy's four
profiles: A and B with a Land 1 autosave each (slot 20), C with slots 15 and 16 of Land 1 and slot 20, and D with
slots 0, 14, 15 to 17 and 20 on Land 1, Land 2 and the multiplayer map MPM_3P_1.

### Where saves live

- `PathCreator` (constructor 0x78E8C0; the working folder is the game folder, `CheckAndRecreateSaveGamePaths`
  0x78EFC0): `GetCurrentUserPath` 0x78EA20 is `.\Profiles\<key>` ("%s\%s"), `GetSaveGamePath` 0x78EA60 that +
  `\Saved Games`, `GetAutoSavePath` 0x78EB20 that + the autosave's folder (slot 20), `GetSaveGamePicturesPath` 0x78EAA0
  `Saved Games\Pictures`, `GetCurrentGamePath` 0x78EAE0 the profile's `Current`. The names are at 0xC20C40 "Saved
  Games", 0xC285A8 "Current", 0xC285B0 "Pictures" and 0xC285BC "Profiles".
- `SaveGameRoom::InstantSaveGame(slot)` 0x792FB0 (cdecl, one argument) makes the slot's folder with "%s\%i"
  (0xBDFF24); for 14 ≤ slot ≤ 18 (0x7930BE..0x7930C6) it adds the land number (g_game +0x205A08) the same way, and the
  slot's `TempleSaveGame` id is slot + 100 × land (0x793119..0x793122). The world file is "%s\%d.sav" (0xC2893C).
- The creature mind files have their own rule: `Creature::Save` 0x4E4EA0 adds the land folder only for 15 ≤ slot ≤ 17
  (0x4E5047..0x4E504F), so the minds of slots 14 and 18 sit in the slot's own folder for every land (seen in the frozen
  profile D: `14\SavedCreature0..2` beside `14\1\14.sav`).
- The save room lists the slot folders of `Saved Games` (`SaveGameRoom::InitEngine` 0x78FD10, `DirFindFirst` /
  `DirFindNext`) and reads each in the folder of the land it belongs to: 15, 16 and 17 (the strings 0xC2887C, 0xC28880,
  ...) in the land's folder ("%s\%d" 0xC28874 with g_game +0x205A08), 14 (0xC28870) in the land before when the land is
  above 1 (0x78FF68), and 18 (0xC2886C) in the land after when 0 < land < 5 (0x78FFCB..0x78FFD2). The slot is `atoi` of
  the folder's name (0x79002E), and fn_00792A20 keeps a slot whose `%d.sav` and `SaveGame.inf` both open. So a load of
  slot 14 reads the save the previous land wrote, and a load of 18 the next land's.

### The slots and who writes them

The room's buttons and the slots' labels are on [The save game room](../interface/temple-interior.md#the-save-game-room).
The writers are the callers of `InstantSaveGame` 0x792FB0, with the argument pushed at each site:

| Slot | Written by |
|---|---|
| 0 to 13 | the save room's save buttons (`ComfirmForSaveGame` 0x792BD0 over a slot that has a save). The room's own save is the function the 1.20 symbols call `SaveGameRoom::LoadGame` 0x792C90: the same steps as `InstantSaveGame` (pictures, `SaveGame.inf`, `SaveAllGame` at 0x792ED3) for `CurrentSlot` [0xE05FC0]; on a failed write it drops the slot's entry (0x792EE4..) |
| 14 | `GScript::GameThingClicked` 0x70AF82, per land. Profile D's `Saved Games\14\1` also holds `14.sav.tmpvor`, the same size (3902 bytes) as the profile's `vortex.txt`: the copy of the crossing file the vortex's save makes beside the world file (`VortexSave::Save` 0x76FB40, the shields and vortexes below) |
| 15, 16, 17 | the autosave circle: `GameOSFile::AutoSave` 0x564050 takes slot = CircleSlot(land) % 3 + 15 (`SAVE_GAME_ROOM_CIRCLE_FIRST_SLOT` 15, `_COUNT` 3); the counter "CircleSlot(%d)" is kept in the current profile's registry values (`LHNetGet/SetCurrentProfileUlong`) and stepped after each save, per land |
| 18 | no `InstantSaveGame` call passes it: the room writes it when slot 14 is loaded, and 14 when 18 is (0x7936D4..0x793796) |
| 19 | the quick save: `GGame::DoAction` sends packet 0x61 (the quick load 0x62) outside a network game, and `GPacket::ProcessPacket` calls `InstantSaveGame(19)` at 0x63DC88 |
| 20 | `AutoSaveFilename` "20.sav" (`SAVE_GAME_ROOM_AUTO_SAVE_SLOT`): `CitadelHeart::Built` 0x4650EF when a temple heart is built outside a network game (in a new Land 1, 31 s after the start-up requester is answered: `InstantSaveGame(20)` returns to 0x4650F6); `GGame::Loop` 0x54D5BE / 0x54D5DB on quitting and before a profile change; and a box callback `sub_54C930` 0x54C970 (event 3, control 0x2712: it saves, then `GGame::LoadMap`). The frozen slot-20 saves are titled "Guardado Automático al Salir" (a Spanish install) |
| any | `GScript::InstantSaveForScript` 0x6FF8E0: the challenge script's SAVE_GAME_IN_SLOT, the slot popped as a float and truncated (`ftol`) |

### Start-up, the autosave and the load attempts

- `GGame::Init` (Game.cpp) loads the autosave when the start mode (GGame +0x25017C) is 0 and `IsAutoSaveValid`
  0x564160 holds (the file exists and opens; its counter and revision are read but not checked); mode 2 loads
  `LoadGameFilename`; a failed load falls through to a new game. `AutoLoad` 0x563FF0 sets `SaveGameRoom::CurrentSlot`
  to 20.
- `GGame::EndTurn` calls `AutoSave(false)` at 0x54EA43 when GameFlags has `GAME_FLAG_AUTO_SAVE_ENABLED` (0x10000, the
  options' autosave box; set in every frozen save) and g_game +0x205A10 is 0 (Game.cpp:2521). `AutoSave` 0x564050
  saves when no wide-screen help sequence runs, the game is not a network game, the land is not the Gods' Playground
  (`GAME_LAND_NUMBER_PLAYGROUND`, 6), and either it is forced or the turn − LastAutoSaveTurn > 6000
  (`AutoSaveInterval`, 0xBEC98C) and the game is not paused.

### The files of a slot, in the order they are written

`InstantSaveGame` 0x792FB0 first asks for a picture of the screen into the temporary picture (fn_00828E90 →
`ScriptedScreenShot::Process`, whether it is taken at once is not read **🔍 inferred**), sets `CurrentSlot`, makes the
title (the slot's help text of the table 0xC2864C: 0 for slots 0 to 13, 0x1A20 for 14, 0x1A22 for 15 to 17, 0x1A21 for
18, 0x1A05 for 19, 0x1A23 for 20, through `UNICODE_sprintf` with the land's number as a double), makes the slot's
folders, then, in the exe's order, copies the map pictures (0x79318C) before it puts the pictures into their pages
(0x793192), in this order (the file times of a save made by the original agree: the temporary
picture, the two picture pages, `SaveGame.inf`, `helpstats.dat` (the help profile's save inside `GGame::Save`),
`Script_20.sav`, then `20.sav`):

1. **The picture.** The temporary picture `Saved Games\Pictures\save_game_picture_temporary.raw` (0xC2890C, from the
   frame the game snapshots: "Request Snapshot" in the log) is copied (fn_007923D0) into the shared pages
   `save_game_picture_hires_%i.raw` (0xC288AC) and `save_game_picture_lores_%i.raw` (0xC288EC). The page is slot / 2 for
   the large pictures and slot / 8 for the small ones (slot 20 → hires_10, lores_2; 15 → hires_7, lores_1; 16 →
   hires_8), measured on the frozen profiles (the arithmetic of fn_00792650 / fn_00792700 is not read): in every
   profile each slot with a save has its two cells drawn, the cells of the slots without one are still 0xFF, and the
   exit save's cells (slot 20: the top half of hires_10, the cell at (0, 128) of lores_2) are the temporary picture's
   large picture and its small one, at (0, 128).
2. **The map pictures.** `GetCurrentGamePath` and fn_00784A30 (`CopyFileA`, "%s\%s", not failing if the file is there)
   copy the profile's `Current\screenshots_hires_<n>_map_0.raw` for n 0 to 10 (fn_007843F0), then
   `screenshots_lores_<n>_map_0.raw` for n 0 to 2 (fn_00784430), into the slot's folder; a copy that fails is not
   looked at. (In the code this step comes before step 1.)
3. **The info.** `TempleSaveGame::UpdateDateAndTime` 0x78F770 (`GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE)`
   and `GetTimeFormatW(LOCALE_USER_DEFAULT, 0)` of the time now, each read into 64 characters, the A versions when a W
   one fails; when the two lengths with their ends come to less than 62, `swprintf` "%s %s" (0xBEA62C) of the time and
   the date, else an empty text: e.g. "22:01:40 jueves, 12 de septiembre de 2024"), then
   `SaveGameInfo` 0x792910 writes `SaveGame.inf` ("%s\SaveGame.inf", 0xC2892C).
4. **The world.** `GameOSFile::SaveAllGame("<path>\<slot>.sav")` 0x558160 writes `<slot>.sav.tmp`; inside
   `GGame::Save`, `Creature::Save` writes each `SavedCreature<n>` and `GScript::Save` writes `Script_<slot>.sav.tmp`.
   When nothing failed it deletes the old `<slot>.sav` and renames the .tmp over it, then the same for the script file.
   A write error turns `WriteEnabled` off; the save is then reported (help text 0xD9C, `DoSaveProblemRequestor`
   0x54CA90) and the old files are kept.

#### SaveGame.inf

260 bytes:

| Offset | Size | What |
|---|---|---|
| 0 | 4 | the `TempleSaveGame` id % 100, that is the slot (0x792975..0x79298C) |
| 4 | 128 | the date and time, UTF-16, 64 characters (`TempleSaveGame` +0x104) |
| 0x84 | 128 | the title, UTF-16, 64 characters (`TempleSaveGame` +0; for slot 20 the help text of the table 0xC2864C) |

Each text is copied into a stack buffer and the whole 128 bytes are written, so the bytes after its terminating zero
are what the stack held (seen in the frozen files): two files of the same save compare only up to each zero.

#### The pictures

Every `.raw` picture of a save is 32,896 bytes: u32 0x8080 (the size of what follows), a `DDSURFACEDESC2` (124 bytes:
256 × 256, linear size 0x8000, pixel format "DXT1" at byte 84 of the file), then 32,768 bytes of DXT1 blocks. The same
layout holds for the `screenshots_*_map_*.raw` and the `save_game_picture_*.raw`: the 128 bytes before the blocks of
all 106 frozen pictures are the same words but the description's surface pointer (word 10, byte 0x28), the address of
the picture's texels in the game's memory, which varies from file to file. A large-picture page keeps two slots, one
above the other (slot & 1 the half); a small-picture page eight, in four rows of two (slot & 1 the column, (slot & 7)
/ 2 the row). A page the game makes new is all 0xFF. openblack's `components/rawimage` reads another `.raw` (plain RGB
or alpha rows), not this one.

#### The creatures' mind files

`Creature::Save` 0x4E4EA0 builds "%s\SavedCreature%d" (0xBDFF10) from `GetSaveGamePath`, `CurrentSlot`, the land for
slots 15 to 17 (above) and the creature's place in `Creature::CreatureList`, and calls `CreatureMental::SaveMind`;
`Creature::ResolveLoad` 0x4E6AB0 reads it back (0x4E6BB1). The files are creature mind files of version 33 (first u32
0x21), the layout of [The creature's mind](../creature/mind.md).

#### The script machine's file

`GScript::Save` 0x6EBEA0 makes "%sScript_%s" (0xBEC9D4) from the folder and name of the open `.sav.tmp` and calls
fn_006EB980 → `ScriptDLL` 0x6F6960 → the script library's SaveState (`[dll+0x64]`), which writes the whole LHVM image
with its state. The library is `Plug Ins\ScriptLibraryR.dll` (base 0x10000000; exports `SaveState` 0x10002550 →
0x10006FA0, `RestoreState` 0x10002560 → 0x100073E0, `SaveBinary` 0x10002530; xref does not index it, it was read
with capstone). SaveState: `fopen` "wb+" (0x10015092), the program (fn_100067D0), the main stack (fn_100072E0), the
globals (fn_10007050), the tasks (fn_10007130: the count of the list at 0x1003BD58, then fn_10007190 per task, in the
list's order), five u32 (0x1003BDB4, 0x1003BDC0, 0x1003BDB8, 0x1003BDBC, 0x1003BDC4; 0x10006FE5..0x1000702E;
openblack reads them as the ticks, the current line, the highest task number, the highest script id and the number of
instructions run; the third is the one the task constructor fn_10008870 steps for each new task's number), `fclose`.

- **A stack** (fn_100072E0): u32 count, u32 +0x104, u32 +0x108, then for each place its type (u32) and then its value
  (u32) (0x10007327..0x10007350: the type, 0x80 bytes past the value, is written first).
- **The globals** (fn_10007050): u32 count, then per variable a type byte, a u32 value and the name with its zero.
- **A task** (fn_10007190): its locals as the globals; nine u32, task +0x14 (its number, given by the task
  constructor), +0x18, +0x1C, +0x20, +0x24, +0x134, +0x148, +0x10 (its script's id, which the reader looks up) and
  +0x158 (openblack reads the others as the instruction, the previous instruction, the task waited for, the variables'
  offset, the handler index, the ticks and the script's type); four state bytes, +0x14C..+0x14F (openblack's in a
  handler, stopped, yielding and sleeping), each written 0xFF when it is not zero and 0 when it is (`neg`/`sbb`,
  0x1000719B..0x100071E7); its stack (fn_100072E0 on +0x28); and its handlers' addresses (fn_10007370: u32 count, then
  a u32 each). The script id is written as it is.
- **Reading** (RestoreState: `fopen` "rb", the program fn_10006780, the stack fn_10007A90, the globals fn_10007510, the
  tasks fn_10007830 with fn_10007920 per task, appended to the list in the file's order, and the five u32 in the same
  order) mirrors the writers: a stack place is read as its type and then its value (0x10007AE4..0x10007B13); a state
  byte is true when it is not zero (`setne`, 0x100079CF..0x10007A3A); a task's script is found by its id
  (fn_10006490 walks the script list for the script whose +0x28 is the id), and a task whose script is not there keeps
  "Not compiled" (0x100219E0) as its script's name and file (0x10007A52..0x10007A84).
- In every frozen file the 514 scripts have the ids 1 to 514 and the tasks' scripts go up to 510; the tasks are in
  rising number order; every stack, the main one and every task's, is empty, so the stack's order rests on the
  disassembly above; and every state byte is 0 or 0xFF.

`GScript::Save` then writes into the world file: the 511 entries of 0x14 bytes at 0xD9680C..0xD98FF8 first, each by
fn_0070D690 (the script's object handles, `BaseInfo`: `GetBase` 0x436B80, kept when the object `IsAvailable`
(vt +0x2C), else null): `WritePtr(object)`, then the entry's u32 +0xC and byte +0x10 (0x70D6C2..); then [0xD95C04],
`Dat_00D95C0C` (raw 4-byte writes that add to the sum by hand), GScript +8, +0xC, +0x10, +0x54 (12 bytes), +0x60
(12), +0x78, +0x7C, +0x80, +0x84, +0x3C, [0xD99000], +0xA8, +0xAC, the pointers +0x70 and +0x74, +0x98, +0x9C,
+0xA0, +0x88, fn_006EC800(+0xA4), +0x90 and +0x94. The script variables in `Script_<slot>.sav` hold handles into that
table, so a load must restore it to re-link them to the loaded objects.

A save made inside a script command keeps that command's task at the command. The temple autosave is one: the
script's `SET_PROPERTY` of the temple's built percentage (`ScriptProperty.cpp`, `SCRIPT_OBJECT_PROPERTY_TYPE_BUILT_PERCENTAGE`)
calls `MultiMapFixed::SetPercentBuilt` (Fixed.cpp:627), which calls `Built`, which reaches `CitadelHeart::Built`
(CitadelHeart.cpp:584) and `SaveGameRoom::InstantSaveGame(20)`. The script library's task loop moves a task's
instruction pointer (+0x18) only after the command's handler returns (`ScriptLibraryR.dll` 0x100082A2, after the call
at 0x10008273); the command handler (0x10009E70) does not move it, nor does SaveState (0x10002550, 0x10006FA0). So the
file holds the task at the command with its values already taken off its stack, and a load runs the command again on
an empty stack: three empty pops and a `SET_PROPERTY` of no object, as openblack's load logs ("VM error 1" three
times, "SET_PROPERTY: Object no longer valid"). **✅ verified** (disassembly, 2026-11-10); openblack does the same.

The table's places are 1 to 511 (`SCRIPT_MANAGE_TABLE_SIZE` 512, place 0 the null; `GScript::Save` writes
`VariableTable[1..511]`, ScriptManage.cpp). Each entry is `ScriptManage::Save` 0x70D690: `WritePtr` of the thing (null
when it is not `IsAvailable`), then `CreatedByScript` (u32) and `NoReferences` (u8). `ScriptManage::Load` reads them
back, `SetThing` the read object, and marks the entry invalid when it has no object but a count.

openblack (2026-11-10, held with the start from the automatic save): the scripts' machine names an object by its
entity, so a save gives the objects places. The objects the machine's values name (globals, each task's locals and
stack, the main stack) and those a script holds (`ScriptHeld` with a place) each take one, in entity order, and the
game object's tail writes the table: each object's reference, whether a script made it, and its count. The machine's
file is written with the places in place of the entities, and the running machine keeps its own values
(`ECS/SavedScriptObjects.h`, `LHVM::MapObjects`). A load puts each place's loaded object back in a script, held as
many times as its count, and turns the machine's places back into the loaded entities. More than 511 objects refuse
the save. **⚠️ partial**: the place numbers follow openblack's entity order, not the order in which the original hands
places out (`FindFreeScriptGameThingSlot` goes round the table from the last place it gave). A load reads a place
the same way whoever wrote it, so the original's saves are re-linked as openblack's are.

#### Other profile files

- The creature carried between lands is written by `GGame::ClearMap` 0x552BB0 to `.\Scripts\CreatureMind\
  <UserFilename>` (`SaveMind`) and `Physique<UserFilename>` (`Save3D`), in the game folder (Game.cpp). The frozen copy
  has two such pairs, `C<hex>.erc` and `PhysiqueC<hex>.erc`, with `.bak` copies; the name is not among the exe's
  strings, it comes with the profile **🔍 inferred**.
- `creature.lhp` (the profile's root, "%s\creature.lhp" 0x9D0450, fn_0047B440) is the online creature page's archive,
  not a save file ([Not in the original](../project/not-in-the-original.md)).

### The world file

#### The frame

| What | How |
|---|---|
| the load-attempts counter | u32, a raw write that the sum does not count: 1 while the file is written, set to 0 at the end (0x558160) |
| the revision | u32 length (with the zero) and the bytes, each byte one value: "$Revision: 183 $" |
| the landscape | the same: `GLandscape::Filename`, e.g. ".\DATA\LANDSCAPE\Land1.lnd" |
| the game object | `GGame::g_game->Save(file)` directly, with no reference before it: it is entry 1 of the pointer list, put there before |

#### The values and the running sum (GameOSFile.h)

- `WriteIt(T&)`: the value's bytes as they are in memory, `sizeof(T)`, little-endian, one write; the running sum
  (GameOSFile +0x214) grows by the value's first byte + sizeof(T) (e.g. `and eax, 0xff; add eax, 4` at 0x4E4EF1). A
  struct (MapCoords 12, LHPoint 12, LHMatrix 48, GAlignment 16, ...) is one value. `ReadIt` mirrors it. A bool is 4
  bytes (`bool32_t`) unless the field is a C++ `bool`.
- `WriteArray(p, n)` / `WriteCountedArray`: u32 n, then n values. A string: u32 length, then its characters.
- Lists (`LHListHead`, `LHLinkedList`, `LHOrderedLinkedList`): u32 count, then a pointer per element. `LHStack` and
  `LHDynamicStack`: u32 count, then the values from the top (index size − 1 down to 0). `GJVector`: u32 count, then each
  element by `WriteSafe`. `WriteSafe2DArray`: rows, columns, values. `GTPointer`: the validation turn, then the
  pointer. `Persistent*`: (file id, index) from `Persistent::GetSaveID`. `WriteInfo`: the info's id.
- `WritePtr` 0x561E10: null → u32 0. An object already in the pointer list → its 1-based number. A new one is added,
  its number (the new count) is written, and its `Save` follows at once (depth first). An object whose `GetSaveType`
  is 0 writes 0 (with a debug warning). `ReadPtr` 0x562180: 0 → null; count + 1 → `LoadInstance`, and the object's
  record follows; a smaller number → that entry; anything else is a "non-sequential" warning.

#### An object's record

- `GameThing::Save` 0x56FBE0 starts every record: u32 save type, `SaveExtraData` (u32: 0 by default, the player
  number for GPlayer and the player-owned singletons), u32 the running sum (`WriteCheckSum` 0x563F60: the sum of
  everything before this word, which then counts itself), u32 `destroyed`, u8 the GameThing flags.
- `GameThingWithPos::Save` 0x5703F0 adds Pos (MapCoords, one value) and its u16 flags.
- `Object::Save` 0x639B90 adds the info id, the coords (MapCoords), u32 the creation index, the life, scale and y angle
  (floats), the fire effect pointer, and, when the object is in the physics (flag 1 << 6) and not carried by particles
  (1 << 4), the physics matrix (48 bytes), the velocity (12) and the angular velocity (12).
- `Fixed::Save` 0x52E140 adds the town artifact pointer; `Tree::Save` 0x74C1B0 a u16 (+0x60), 4 bytes (+0x64, a float),
  the pointer +0x68 and a u16 (+0x5C).
- The info id: every info row registers itself when its table loads (`GBaseInfo::SetInfoID` 0x436BD0: its index is
  `InfoCount`++ [0xC58604], and it is pushed at the head of the list at g_game +0x205CEC; called from `load_variables`
  and each info table's `Load`/`LoadBinary`). `GetInfoID` 0x436C00 counts from the head, so the saved id is the number
  of rows − 1 − the row's load index; `GetInfoPtr` 0x436C30 walks back. Seen: 1743 for player 0's GPlayerInfo,
  2104..2115 for the trees (registered earlier, so higher). The tables' order is `load_variables` (bw1-decomp
  Balance.cpp:223: the magic infos, the magic effects, the spell seeds, the animals, the creatures, ..., the citadels,
  the abodes, the special villagers, the trees, misc, ..., the player info, ...).
- `LoadInstance` 0x558DC0 reads the type and the extra word and makes the object (`new <Class>`), except the
  singletons, which load into the object that exists: GGame (0x6A), GPlayer (0x63) by the extra player number,
  GPLAYER_INTERFACE (0x64), GPLAYER_INTERFACE_STATUS (0x65), GLEASH_STATUS (0x102), GCAMERA (0x77) and GAME_STATS
  (0x9F). Old types (spell wood, help orbs, spell icons, bookmarks, furniture, graph lines) and FieldContents rows only
  warn. 260 save types make an object; bw1-decomp has the C of 73 classes' `Save` and 72 classes' `Load`, the rest is
  in the exe only.
- Raw structs carry what was in memory: GAlignment's 16 bytes hold its vtable pointer (0x8CC7F0 in every save read)
  and `destroyed`. A writer has to put the exe's constant there, and two saves of the same state can differ wherever a
  raw struct holds a heap pointer or padding (not surveyed yet).

#### The game object, the top level

`GGame::Save` 0x554090 (Game.cpp:2679): the GameThing part; "$Revision: 756 $"; GameFlags (u32); LandNumber (u32);
`WritePtr(&players[i])` for the 8 players (the first one pulls in, depth first, its computer player, its stats, its
citadel, its creature, its towns and so most of the world); GData (RandSeed, InitialRandSeed, GameTurn, RealGameTurn,
NumCreatedObjects, FrameCount, InteractDesire, WorldPopulation); PlayerIndex (u8); the leash flags byte (evil + 2 ×
rope + 4 × good, added as raw bytes); +0x205A5C (u8); +0x205BA0; `GameLists.Save` (GlobalGameList.cpp:272: 44 lists in
this order: street lights, street lanterns, rewards, balls, forests, livings, spells, particle containers, dances,
reactions, mobile objects, villagers without a town, fields, fish farms, fire effects, fireflies, the puzzle game,
fireballs, map shields, mist, food piles, flocks, influence rings, weather things, script highlights, game things with a
position, streams, waypoints, footpaths, waterfalls, arenas, towns, building sites, multi-map fixed, animated statics,
objects, trees, big forests, climates, town centres, whales, town artifacts, fragments, the footpath finder);
+0x205D34; RenderTime; RenderTimeInc; the player and town influence multipliers; the visual time and GGameInfo +0x48,
+0x4C, +0x50, VisualTimeScale; `HelpSystem::Save` 0x5C6EB0; the help profile; `GScript::Save` 0x6EBEA0;
StartCameraCoords; the camera pointer (`GCamera::Save` 0x4430E0); the climate pointer (`GClimate::Save` 0x773320);
`CameraExclusion::SaveExclusionFile`; CameraHelp EnabledFeatures, AutoPitchParam1 and AutoPitchParam2;
`CreatureRoom::CreatureInTemple`; the snow cover (u32 0x4000 and 0x4000 floats, u32 0x400 and 0x400 floats, two more
values); `LH3DAtmos::ambient`; the start mode (+0x25017C); ScriptRebootRequested (1 byte); SkirmishGame; +0x205A10;
+0x205A14; LastAutoSaveTurn; +0x205A18; `Bookmark::SaveAll` 0x43A3F0; `GLandBalance::SaveAll` 0x5E2990; the hand
(`CHand::Save` 0x46EB10); `GAudio::Save` (the thing-music list, the script music's type and whether it plays, the
current town pointer, the alignment); `WorldRoom::RoomsAvailable`; `WorldRoom::SaveButtonConfig` 0x79F640;
`CreditsRoom::DYKSave` 0x78CE70; `ChallengeRoom::ChallengeSave` 0x784C70; the script creature curse (raw).

#### The first classes

What every save starts with, read field by field and proved by the sums on all 16 frozen saves and a new game's
save made by the original:

- **The players.** `GPlayer::Save` (Player.cpp:1997): the GameThing part (its own word is the player number); u8
  player_number; the alignment as one raw value of 16 bytes (its class word 0x8CC7F0, destroyed, the value and the
  change); the info id; `WriteArray` of TribalPower and MaxTribalPower (9 floats each); type; `WriteArray` of the name
  (32 UTF-16 characters, the zeros included); UserId, CheatOn, WindResistance, AllMagicEnabled, CheatField930,
  InfiniteInfluence (u32 each); the computer player; TotalPopulation; StartPos (one value of 20 bytes: MapCoords and
  two words); `WriteArray` of MagicTypeEnabled (ints) and MagicTypeEverBeenEnabled (C++ bools, a byte each), each of
  42 in every frozen save (bw1-decomp's CHL enum ends MAGIC_TYPE_LAST at 44, but the game writes 42); the game stats,
  the citadel and the creature; the towns and the teleports (`LHListHead`: count, then a pointer each); InfluencePower,
  AverageInfluencePower; `WriteArray` of DamageFromPlayer and of Allies (8 floats each); `WritePtrArray` of the 18
  interfaces (count, then a pointer each); HasLost; +0x93C. 644 bytes with no town, no teleport and the interfaces
  written before.
- **The computer player** (exe only: `GComputerPlayer::Save` 0x657640): the GameThingWithPos part; the player (+0x28,
  `GetPlayer` 0x55E3B0) and its action state (+0x34); three lists of pointers, each its count and a pointer each
  (+0x38 with its count +0x3C, +0x40 / +0x44, and the list +0x50 with its count +0x54, linked through +0x124,
  fn_00658F50); four raw words +0x58, +0x5C, +0x60, +0x64 and the global [0xC00B08], each adding its first byte + 4 by
  hand; `WriteArray(+0x68, 42)` of u32; a raw word +0x110; ints +0x1B8, +0x1BC, +0x1C0; a byte +0x1D0 (fn_00658DA0);
  `WritePtrArray(+0x1D8, 8)`, the eight attitudes; then, when +0x2C is set, its tree of desires by fn_00656400: each
  node's +0 (the suppression) and +4 (the personality) as raw words, then its children (the list +0x9C) the same way,
  with **no count**. The tree is the fixed one of 198 nodes ([Computer minds](../hand/computer-minds.md)), built by
  `GComputerPlayer::Init` 0x656E60 whenever +0x2C is 0, so a reader writes and reads 198 pairs in the tree's order
  (every frozen save has it). 1,880 bytes with empty lists and the attitudes written before.
  What the fields are (bw1-decomp's `PlayerComputer.h` and the names recorded with gx.py): the three lists are
  SpellCasts, Seen and Queue; +0x58 the player's speed (`speed`, written with the hand's state's by `SetSpeed`
  0x6583F0), +0x5C `seenRange`, +0x60 `maxSeenCount`, the constructor's three arguments (90, 30 and 50 from
  `GPlayer::Init`, Player.cpp:215); +0x64 `inGameInitDone` (1 once `InGameInit` 0x657060 has run, the start on the
  land; cleared at a land change); +0x68 the icons the mind counted by magic type (`spellLookedForCounts`, 42
  words, zeroed by the constructor); Active (+0x1B8, `GPlayer::ToggleComputerPlayer`), `paused` (+0x1BC,
  `GComputerPlayer::Pause` 0x658610) and `heldByScript` (+0x1C0); CreatureSizeMode (+0x1D0, the creature-like
  player's number, 0xFF from `Init`). The global [0xC00B08] starts at -1 in the exe's data and only the computer
  players' `Load` writes it (every frozen save has -1). The attitudes are by the other player's number, each made
  at 1.0 by the constructor 0x656D40.
- **The action state** (exe only: `PlayerActionState::Save` 0x651740): the GameThingWithPos part; its computer player
  (+0x6C); a counted list of pointers (+0x28, its count +0x2C: its sub-actions); eight raw words +0x30..+0x4C; a raw
  value of 12 bytes +0x50; an LHPoint +0x5C; a float +0x68; a pointer +0x74; a float +0x78; an LHPoint +0x7C. 119
  bytes with an empty list. Its constructor (fn_0064FFD0, from `GComputerPlayer::Init`) puts it in the middle of the
  cell (128, 128), +0x48 at 1, +0x4C the player's number and +0x68 the computer player's speed (the steps' speed:
  `SetSpeed` writes both); every frozen save's has +0x30 at 1.
- **The attitudes** (`GComputerAttitudeToPlayer::Save`, PlayerComputer.cpp): the GameThing part and the float
  Attitude: 21 bytes. Each computer player writes its eight in its own record.
- **The game statistics** (`GameStats::Save`, GameStats.cpp:632): the GameThing part (its own word is the player
  number); the player; NumPlayersThatHaveLeftTheGame (static); Position; LossReason; TownStats (one value of 40
  bytes); 25 words from TotalBirths to WoodUsed; InfluenceGraph and PopulationGraph (`EverlastingGraph<float, 500,
  50>`, one value of 2,012 bytes each); 11 words from NumGodsDefeated to +0x108C; the statics TotalLinesOfCodeExecuted,
  MaxFrameRate, MinFrameRate, NumBaseAllocations, StartTime and StatsVersion; the 36 cast counts; TotalChantsUsed,
  TotalNumSacrifices and the static NumTownsAtStart: 4,421 bytes.
- The base classes the later ones build on, from the C: `Fixed::Save` adds the town artifact; `MultiMapFixed::Save`
  (Fixed.cpp:805) a u8 FixedFlags, the float PercentBuilt, the footpath link and the building site; `Mobile::Save` a
  u16 (+0x54); `MobileObject::Save` (MobileObject.cpp:499) the object it holds, XAngle, ZAngle and its path;
  `MobileStatic::Save` (MobileStatic.cpp:48) the player and two words (+0x80, +0x84).

In every frozen save the walk from the game object goes: the game object, player 0, its computer player, the action
state (object 4), the eight attitudes, the statistics, and then the citadel (type 0x35) or, in the two Land 1
autosaves whose player 0 has no citadel (profiles A and B), the first town (type 0x28): 13
records, every stored sum equal.

#### The citadel, the footpaths and the town

What player 0's record pulls in next, depth first: its citadel and the citadel's heart, the footpaths of the heart's
link, then (through the heart, or through the player's towns when it has no citadel) its town, the town's storage pit
with the storage pit's footpaths and piles, and on into the town's own values:

- **The citadel** (exe only: `Citadel::Save` 0x463B00): `Container::Save` 0x46B920 (Container.cpp: the
  GameThingWithPos part, `WriteInfo(info)`, the owner); the heart (+0x30); `WritePtrArray(+0x34, 6)`, its worship sites by slot (`Citadel::AddWorshipSite` 0x463770 sets slot + 0xD, `RemoveWorshipSite` 0x4637C0 clears it); a list
  +0x4C whose count +0x50 is a raw word (linked through +0x84, fn_00464140) and a list +0x54 with a raw count +0x58
  (through +0x8C, fn_004641B0); two counted lists, +0x5C with its count +0x60 (through +0x44) and +0x64 / +0x68 (through
  +0x84); raw words +0x6C and +0x70; `WriteIt(+0x74)`.
- **The heart** (`CitadelHeart::Save`, CitadelHeart.cpp:719) on `CitadelPart::Save` 0x469830, which writes what its
  Load reads (CitadelPart.cpp: the MultiMapFixed parts, Influence, the citadel and +0x88): LastImpactTurn, the town,
  AlignmentFlock, SpecularColor, DrawColour, the byte DestructionStarted, DestructionTime, the plasma and magic
  containers, NextPlasmaTime, TransferContainer, TransferObject, LastTransferTurn and the entrance: 149 bytes with every
  reference written before.
- **The footpaths** (Footpath.cpp): `GFootpathLink::Save` its `LHLinkedList` of footpaths; `GFootpath::Save` its
  nodes (`LHListHead`) and the bool32 Active; `GFootpathNode::Save` its MapCoords, the u8 Flags and, outside the map's
  own footpath file, its followers (`LHLinkedList<Living*>`); `GFootpathLinkSave::Save` the MapCoords and the link.
- **The storage pit** (exe only: `StoragePit::Save` 0x7338D0): `Abode::Save` (Abode.cpp:1697: the MultiMapFixed parts,
  the u8 AbodeFlags, DrinkingWater, UnusedSavedValue, the town, the villagers, the four bytes AdultCount, PresentAtHome,
  ChildCount and index, `WriteCountedArray` of the two resources, and the bool32 for a destruction mesh, whose
  `WriteToFile` follows when it is set), then `WritePtrArray(+0xC4, 1)`, its food pile, and `WritePtrArray(+0xC8, 5)`.
- **The piles** (exe only): `Pot::Save` 0x66D730 adds to the MobileObject parts the raw ResourceType (+0x68), the
  ParentTown (+0x6C), the raw ResourceAmount (+0x70) and a raw byte of flags (+0x74); `PotStructure::Save` 0x66DA90 the
  building (+0x78), the player (+0x7C) and a raw byte (+0x80); `PileResource::Save` 0x66F620 (the wood pile, type 0x5A)
  the AltitudeZoomer as one raw value of 0x30 bytes (+0x84); `PileFood::Save` 0x66F5A0 then a pointer (+0xB8).
  âœ… What the fields are (read 2026-10-28; [what a pot keeps of its making](../world/objects.md#pots-and-piles)): the
  flags byte is Poisoned (bit 0), the reaction started (bit 1, `Pot::SetupReaction` 0x66D660 / `RemoveReaction`
  0x66D6A0), FromBuildingSite (bit 3, the last argument of `Pot::Create`) and SpeedUp (bit 4); ResourceType is the
  row's (GPotInfo +0x118, `Pot::Pot` 0x66CE60); +0x78 is ParentMultiMapFixed, the structure the pile is part of; +0x7C
  the player `GetPlayer` answered as it was made, and again as the load resolved it (`PotStructure::CallVirtualFunctionsForCreation` 0x66DB90); +0x80
  whether it was made to start its reaction; the AltitudeZoomer is the pile's sink (value, destination, destination
  speed, speed, a time only ever 0, time, duration, start value, start speed, c2, c3, c4:
  [the zoomer](matrices-and-zoomer.md#zoomer-lh3dlib)); the food pile's +0xB8 is its speed-up's spot visual (`PileFood::SetSpeedUp` 0x66E220). The classes and their
  numbers: the plain pot 0x58 (`Pot::Save` alone), the food pile 0x59 (`PileFood::Save`), the wood pile 0x5A
  (`PileResource::Save`, PileWood has no Save of its own), the magic food 0x1E (`MagicFood::Save` 0x5FAB00: the food
  pile's and its player +0xBC), the magic wood 0x1F (`MagicWood::Save` 0x600F20: the wood pile's and its player
  +0xB4) and the puzzle grain 0x107 (`PuzzleGrain::Save` 0x6DBEB0: the food pile's and its word +0xBC).
  `PotStructure` and `PileResource` are never saved as themselves (no `GetSaveType` of their own, made only inside
  the piles' constructors).
- **The town** (`Town::Save` 0x73ED30, in Town.cpp:3051's order): the Container part; MainStoragePit;
  `WriteSafe(TownDesire)` 0x563B30 (GameOSFile.cpp: 17 values in each of its arrays, 1,436 bytes); CameraView (one raw
  value of 0x18 bytes, +0x598); ID; tribe_type; the u8 player_number; 13 raw words +0x5C0..+0x5F4 (BalanceBeliefScale,
  +0x5DC, is written last); `WritePtrArray(+0x600, 2)`; the forests (count +0x60C, a pointer each); stats as one raw
  value of 0x118 bytes (+0x610); AreaMin; AreaMax; the creche; the graveyard; the workshop, abode, homeless, extra
  villager, spell icon, field, fish farm, building site and artifact lists; GBelief as one value of 0x1D0 bytes
  (0x742950); the villagers; NearestTown; the playthings and the animals; the worship site; the town centre; the
  planned list; PlayerTownInteract[8] as one value of 0x400 bytes (0x742AE0); the villagers on their way to worship;
  MagicTypesHeld as one value of 42 ints; FootballPitch; LastAttackingPlayer; LastAttackedTurn; five floats;
  `WriteSafe2DArray` of ResourceLastRemovedTurn (rows 8, columns 2, then the 16 words); the flocks;
  `WritePtrArray(+0x9B0, 17)`, the desire flags; the missionaries; BalanceBeliefScale; EmptyTownTimer.

With these the walk reads every frozen save, and the two saves made by the original, with every stored sum equal,
the stopping record's own included, and stops at: a villager (type 6) or a special villager (0x76) that follows a
footpath node (11 files), a forest (0x4E) in a town's forests (5 files, those that reach the town's own values), or
the flock (0x7E) of a citadel heart on the multiplayer map (4 files).

#### The forests and the trees

A town's forests list pulls in each forest, and each forest its two lists of trees and its big forest:

- **The forest** (exe only: `Forest::Save` 0x53AE40): the Container part; the raw IsScenic (+0x3C); a pointer +0x30
  (its footpath link); the raw id (+0x40); its two lists of trees (`LHOrderedLinkedList`, Trees0 at +0x48 and Trees1 at
  +0x50), each with its count as a raw word (+0x4C, +0x54) and then a pointer for each tree (the node's payload at +0,
  the next node at +4); BigForestObject (+0x38); and two raw words of the whole game, [0xCD04C8] and [0xBEA238].
- **The big forest** (`BigForest::Save`, BigForest.cpp:70): the MultiMapFixed parts, the forest and +0x84.
- **The tree** (exe only: `Tree::Save` 0x74C1B0): `Fixed::Save`; two raw bytes of +0x60 (an int16); four raw bytes of
  +0x64 (a float); the forest (+0x68); and two raw bytes of +0x5C, the low half of a uint32.

With them the five files that reach a town's own values read on through its forests (896 trees in them) to the
town's creche, its abodes or its fields.

#### The town's buildings

After its forests a town's record pulls in its creche, its graveyard, then its lists of abodes and fields:

- **The abode** and **the creche** (type 0x50) both save by `Abode::Save` 0x406A10 (Abode.cpp:1697, above): the
  creche's vtable keeps the abode's Save.
- **The graveyard** (`Graveyard::Save`, GraveYard.cpp:110): the abode's record and NumGraves.
- **The field** (exe only: `Field::Save` 0x529B10): the abode's record; `WriteInfo(+0x120)`, its kind; raw words +0xC8
  and +0xDC; the town (+0x118); a raw word +0x11C; a raw byte +0xCC; a raw word +0xD0; the Farmers (count +0xD8 as a
  raw word, then a pointer for each, the payload at +4 of each node); and one raw byte of +0xE0, a uint32.

With them the files that reach a town's own values read on to its first abode's villagers, and profile D's Land 2
autosave to an abode's standard building site (type 0x5C).

#### The villagers

The villagers come in as the followers of a footpath node or through an abode's villagers:

- **The walk round obstacles** (`MobileWallHug::Save`, MobileWallHug.cpp:1008), after `Mobile::Save`'s u16:
  TurnsUntilNextStateChange (int16), speed and GameAngle (u16), MoveState (u8); for the circling states LINEAR to
  EXIT_CIRCLE_CW (0x0C..0x12) the circle hug: TurnsToObj (u8), the entry distance (u32: EntryDistance, or the extended
  one when it is 0xFFFF), the object, its index and its MapCoords; for STEP_THROUGH (0x0B) TurnsUntilStepRebuild (int8);
  then the footpath, the goal (MapCoords), the target and the step (12 bytes).
- **The living thing** (exe only: `Living::Save` 0x5EE630): the footpath (+0xC8) and its node (+0xCC); the action as one
  raw value of 6 bytes (+0x8C: the top, final and third states, a byte and TurnsSinceStateChange); the reaction (+0x94);
  raw words +0x9C and +0xA0 (the birth turn); a raw byte +0xA8; the data path (+0xAC) and the script reminder (+0xB0);
  two raw bytes +0xB4 (the status); the flock (+0xB8), +0xBC and the footpath again (+0xC8); raw words +0xC4 and +0xD0
  (the specular colour); a raw byte +0xD4; the dance group (+0xD8); a raw byte +0xDC; then the reactions done, a raw
  count and for each its reaction (fn_005EF800) and its turn.
- **The villager** (`Villager::Save`, Villager.cpp:2583): Flags (u16), food, LastCheckTurn, the bytes FoodSpeedUp,
  CarriedObjectType and DiscipleType, the int16 ResourceHeld of food and of wood and is_pregnant, the home, the town,
  the mother, two zero words, then `SaveStateFunction`: the save of its top state (action byte 0) and of its final state
  (byte 1), each the SaveState of its entry in `VillagerStateTableEntry::StateTable` (0xD09198).
- **The table of the states** is filled at start-up by `_$E56` 0x59D030: each entry has nine member pointers of 16
  bytes (144 bytes, SaveState at +48) and the exe has 255 states. It differs from the decompilation's GStates.cpp
  (Lionhead's 1.20) in 64 states (state 85 saves nothing here, a football state there; 90 saves a dance's values), so
  the reader follows the exe's.
- **The states' saves** (Villager.cpp:2686..3010): building (the site and BuildPosIndex), dance (+0x118, +0x11C),
  worship (RequestingToGoHome), dead (DeathReason, a byte), in hand (DiscipleOnPickUp), fishing, foresting and inspecting
  (TargetThing), farming (FarmPos, a MapCoords, and InteractObject), football (three bytes and the ball), shepherd (two
  u16), in a script and at a script's position (ScriptWanderCentre, 8 bytes, ScriptWanderRadius, WanderArea.x and .z),
  fire (FirePos, 8 bytes, and the fire), nothing to do (a trader's values, for the trader disciple 9), trader
  (TradeResource and TradeTown), the inspection and the magic tree reactions (two words), clearing an area
  (ClearAreaPos, 8 bytes, and its radius), breeder (Partner and WanderArea, 8 bytes), footpath (the footpath and its
  node), hiding in a building (HideTurns and HidePos, 8 bytes) and the shield reaction (ReactionPos, 8 bytes); flying
  (0x5AC9A0), landing, the reactions and waiting for an animation (0x5AC9C0) write nothing.
- **The special villager** (`SpecialVillager::Save`, SpecialVillager.cpp:49): the villager's record and InfoIndex.

#### The town centre, the flocks and the building sites

What the villagers and the towns pull in next:

- **The town centre** (exe only: `TownCentre::Save` 0x744830): the abode's record, the totem statue (+0xCC) and
  `WritePtrArray(+0xD0, 6)`, its spell icons.
- **A script's reminder** (exe only: `DataForScriptRemind::Save` 0x5EF360, met as a living thing's script reminder):
  the GameThing part, a pointer +0x14, raw values of 12 (+0x18), 4 (+0x24) and 8 (+0x28) bytes, five raw words
  (+0x30, +0x34, +0x38, +0x3C, +0x44) and a pointer +0x40. Loaded (2026-11-11, `DataForScriptRemind::Load` 0x5EF570
  reads the same fields): each one a villager's living part names becomes its script memory
  (`saved_world_load::ScriptMemoryOf`, [the script memory](../scripts/script-commands.md#the-script-memory)): +0x14 the
  thing its walk followed, +0x18 the walk's destination and height, +0x28 the script's place in whole map x and z and
  +0x30 its number, +0x34 / +0x38 the pair of whole numbers, +0x3C the clip its state last put on and +0x44 the state to
  take up; a place, number and pair all 0 are what no script gave, as the villager's own script state values are read.
  Tests `SavedWorldLoad.AScriptsReminderGivesBackWhatItsVillagerKept` and
  `SavedWorldLoad.TheFrozenLandOneSavesScriptsRemindersArePlannedByTheirNumbers` (the 25 of `_J_a_h_o_v_i_a` slot 15).
  An animal's or a creature's reminder is read by nothing but a saved game. An animal's is loaded (2026-11-11) into
  the same script memory, kept only to be written again (openblack's animals make none in play yet: the original's
  `StateInScript` 0x5ED9A0 and `ExitInScript` 0x5ED9C0 make and keep one for an animal too); a creature's is not made. Written (2026-11-11,
  `src/ECS/SavedWorld/WriteVillagers.cpp`, `WriteScriptReminder`): a source keyed by the villager, one for each villager
  with a script memory, made back from the memory the same way, the word only cleared (+0x24) and the pointer only saved
  (+0x40) as the load kept them (0 for one made in play); the villager's living part names it, none without one. Test
  `Data/ForestWriteWorld.AVillagerMadeInPlayIsWrittenFromItsState` (its memory written and read back the same).
- **The mobile static** (`MobileStatic::Save`, MobileStatic.cpp:48): the MultiMapFixed parts, the player and two words.
- **The fish farm** (exe only: `FishFarm::Save` 0x52D3B0): the MultiMapFixed parts; its villagers (+0x80, the count +0x84
  as a raw word); the town (+0x8C); two raw floats (+0x90, +0x94).
- **The data path** (`DataPath::Save`, Living.cpp:49): TrackNo, EndPercentage, Reverse, Frame, FramesPerTurn and +0x2C.
- **The standard building site** (`StandardBuildingSite::Save`, BuildingSite.cpp:40, on the exe's `BuildingSite::Save`
  0x43C830): the GameThing part; RootBuilding; the BuildingWorkerList (its count +0x1C as a raw word), the ScaffoldList
  and a list +0x28 (their counts by `WriteIt`); a raw word +0x30; the building positions as one raw value of 0x600
  bytes (+0x34); raw words +0x634 and +0x638; ForcedDesire and life (floats); then the wood pile.
- **The flock** (exe only: `Flock::Save` 0x5305A0): the Container part; three raw words it works out as it writes them;
  the Shepherd, the town and the citadel heart (+0x30, +0x34, +0x38); its members (+0x3C, the count from +0x48 as a raw
  word); MaxMembers (+0x88); +0x4C; DomainRadius and FlockDistance (u16); FlyingHeight; FollowShepherd; +0x5C; the
  SavedDomainCentre and +0x6C (MapCoords); +0x78, +0x7C and +0x80 (raw words); SavedThing; +0x8C.

With them the walk reaches the town centre's totem statue (type 0x7F) in the town files, and further in the others:
a town's magic wood (0x1F), a building site's scaffold (0x5F), a workshop (0x52) circled by a villager, a reaction
(0x26), a pig (0x87) a villager aims at, or a flock's dove (0x8E) on the multiplayer map.

#### The workshops and the reactions

- **The totem statue** (exe only: `TotemStatue::Save` 0x738700): the MultiMapFixed parts, a pointer +0x7C and the raw
  words +0x80, +0x84 and +0x88.
- **The magic wood** (`MagicWood::Save`, MagicWood.cpp:40): a wood pile's record and the player.
- **The workshop** (exe only: `Workshop::Save` 0x779F40): the abode's record; pointers +0xD0 and +0xD4; raw words +0xC4
  and +0xC8; a counted list of three raw bytes (+0xDC); a list (+0xE0, its count +0xE4 as a raw word).
- **The scaffold** (exe only: `Scaffold::Save` 0x6EA240): the MobileObject parts; pointers +0x68, +0x6C and +0x70; a raw
  word +0x78; a pointer +0x7C; raw words +0x80 and +0x84; two raw bytes +0x88; a pointer +0x8C; a raw word.
- **The reaction** (exe only: `Reaction::Save` 0x6E4990): the GameThing part; a pointer +0x14; the living things
  reacting, an `LHDynamicStack` at +0x18 (its count +0x1C as a raw word, then each with `GetNthMember(count − 1)` down
  to 0, which counts from the head, where each joiner is pushed: the oldest joiner first. openblack keeps no such
  stack, only each follower's reaction; every follower is stamped with the order it joined in, across every class
  (`effects::reactions::NextJoinOrder`), for the list's order); raw words +0x20 and
  +0x24; a pointer +0x28; raw words +0x2C and +0x30; a pointer +0x38; a raw word +0x3C.

With them the walk reaches the towns' desire flags (type 0x4C) in the town files, and pigs (0x87), creatures
(0x69), sheep (0x83), doves (0x8E), fire (0x29) and other classes elsewhere.

#### The animals and the towns' desire flags

- **A town's desire flags** (exe only: `TownDesireFlags::Save` 0x746F10): the Object part, ParentTown (+0x64), then Slot,
  Desire, DesireType and Mesh as raw words (+0x54, +0x58, +0x5C, +0x60).
- **The animals** (exe only: `Animal::Save` 0x41B460): `Living::Save`, then a pointer (+0xE0); Hunger, Tiredness,
  BreedingUrge and +0xEC (u16; +0xE6 is not written); FoodPos (8 bytes); a pointer (+0xFC); PhysObPos, SleepPos and
  EnvironmentPos (4 bytes each); DeathReason; AngleZoomer (48 bytes). Then the animal's state save
  (`SaveStateFunction` 0x41BA90, through `CallSaveStateFunction` 0x41B9B0 on the top state, the byte at +0x8C): the
  exe's `Animal::StateTable` (0xD12108, 53 entries of 144 bytes, filled by `_$E59` 0x5AC9F0) has no save function in
  any of its 53 states, so no animal writes anything there.
- `Dove::Save` (0x41F110) is `Animal::Save`. The vtables give `Animal::Save` to the cow, sheep, goat, horse, zebra,
  pig, tortoise, lion, leopard, tiger and wolf (0x82-0x8C), the puzzle sheep, lion and villager (0x2D-0x2F) and the
  pieces (0x9A-0x9D), and `Dove::Save` to the dove, crow, swallow, pigeon, seagull, bat and vulture (0x8E-0x96). So
  all 25 write the same record. The spell dove and bat (0x90 and 0x95, `SpellDove::Save` 0x41FB40) add 48 raw bytes
  (+0x148) to the dove's record; the spell wolf (0x8D, `SpellWolf::Save` 0x4211D0) adds to the animal's record 12 bytes
  (+0x148), 16 bytes (+0x154), +0x164, 48 bytes (+0x168) and a pointer (+0x198). None of the eighteen saves holds one,
  so only the exe's writes and synthetic goldens stand for them.
  What they are (bw1-decomp `AnimalDove.h`, `AnimalWolf.h`): the dove's and bat's +0x148 is its fade, a `Zoomer`
  (`ScaleZoomer`); the wolf's +0x148 `FinalDestPos` (MapCoords), +0x154 `FinalDestPlane` (an `LHPlane`: the normal and
  its distance), +0x164 the corridor's half width, +0x168 `FadeAlpha` (a `Zoomer`) and +0x198 its `Player`.
  fn_00420F50 sets the plane to (normal.x, 0, normal.z, offset), the half width, and the final place to the destination
  it is given: the leader's T on the ground (altitude 0), a follower's the leader's goal (+0x80..+0x88). The writer
  (2026-11-11, `WriteAnimals.cpp` `SpellPartsOf`, test
  `Data/FlockLoadWorld.ASpellsDoveAndWolfAreWrittenWithTheirFadesTheWolfWithItsRunAndPlayer`) writes them from
  `magic::SpellFlockAnimal` (`fade`, `finalDestination`, `normal`, `offset`, `halfWidth`) and the animal's player, as
  raffclar's reader reads them (`ReadSpellDove`, `ReadSpellWolf`). The load makes them again (2026-11-11,
  `RestoreSpellAnimal` after `MakeAnimalEntity`, test
  `Data/FlockLoadWorld.ASpellsWolfIsMadeAgainWithItsFadeItsRunAndItsPlayerAndWrittenBackTheSame`) as every animal, its
  state, previous state and their memory and its walk's goal from its living and moving parts, then its fade's
  zoomer as saved (`SpellDove::SetDying` 0x41F5C0 and `SpellWolf::SetDying` 0x420CF0 only start the zoomer, so its
  dying state goes on from the saved zoomer and nothing starts it again), and a wolf's final destination (the brain's
  too), corridor, half width and player; its stored point is where its turns start; its flock spell names it as the
  spells are made.

With them the walk reaches a creature (0x69), a fire (0x29), a spell dispenser (0x10B), a player's sub-action (0xFA),
a town's class 0x61, the citadel heart's class 0x6E and a destruction mesh on an abode.

#### The planned buildings, the player's interface and the particles

- **The planned buildings** (`PlannedMultiMapFixed::Save`, PlannedMultiMapFixed.cpp:29; the exe writes the same at
  0x648990): the GameThingWithPos part, YAngle, scale, WasConstructed, +0x34 and CreationTurn (raw words), the
  FootpathLink and the info (`WriteInfo`, a raw word). `PlannedAbode::Save` (Abode.cpp:1174; also the planned town
  centre, 0x62) and `PlannedTownCitadelHeart::Save` (CitadelHeart.cpp:1494) add the town.
- **The spell dispenser** (`SpellDispenser::Save`, SpellDispenser.cpp:34): the abode's record, +0xCC (a pointer) and
  four raw words.
- **The citadel's entrance** (0x110) writes only `Object::Save`.
- **The one-off spell's seed** (exe only: `OneOffSpellSeed::Save` 0x72A930): the MobileObject parts and the raw words
  +0x78, +0x68 and +0x6C.
- **The player's interface** (0x64, `GInterface::Save` 0x5D0490): the GameThingWithPos part and its status (+0x39C).
- **The interface's status** (0x65, exe only: `GInterfaceStatus::Save` 0x5DCA50): the GameThingWithPos part; the
  constant 0xA2C2A as a raw word; player_number; +0x40; ThrowVelocity, ThrowAngularVelocity, HandPos and HandAngles as
  one raw value of 48 bytes; the hand (`GMagicHand::Save` 0x5FB2C0 through its vtable: a raw byte +0x14 and
  `WritePtrArray(+0x18, 1)`); HandHoldingSomething; +0x94 (8 bytes); +0x9C and +0xA0; +0xA4, CameraPos and CameraFoc
  (12 bytes each); then by `WriteIt` ReportedHandPos, +0xE8, HandSpeed, +0x104, +0x108, HandVelocity, +0x118, +0x11C;
  its interface (+0x2C, bw1-decomp's `iface`: the interface whose status it is) and LeashStatus (+0x12C); +0x34
  (fn_005DD250) and +0x38. Its own word (`SaveExtraData`, the interface's too) is its player's number. The
  constructor (0x5DB860, `SetToZero` 0x5DBA00) leaves the throw, the hand, the camera and the points at 0, +0x34 at
  -1, and makes its leash's status. In every frozen save each player's first interface (its `interfaces[0]`) is the
  one with a status, whose hand holds nothing and whose camera and hand points are 0, the local player's too.
- **The leash's status** (0x102, exe only: `GLeashStatus::Save` 0x5E7230; bw1-decomp's `LeashStatus.h`): the GameThing
  part (its own word the player's number); Activated (+0x14: the leash is on, `SetOn` 0x5E6F70), LeashWorks (+0x18),
  LeashType (+0x1C, `LEASH_TYPE`, the one picked: `DoLeashSelectionSystem` compares it with the selection);
  player_number (+0x30, a byte); AppliedForce (+0x20); ObjectAttachedTo (+0x24, a pointer: what the leash is tied to,
  the leash's start point in `GetLeashStartPoint`); +0x28 and LeashLength (+0x2C, the two lengths
  `CalculateLeashLengthFromCreature` writes); its InterfaceStatus (+0x38, a pointer); +0x34. A new one
  (`GLeashStatus::SetToZero` 0x5E6940) is off, working and the learning leash (2), the rest 0: the values of every
  frozen save's leash statuses of players with no creature.
- **What a computer player saw** (0x67, exe only: `GComputerSeen::Save` 0x656B80, PlayerComputer.cpp:47): the thing
  (+0x14: a forest, a tree or a seed) and the turn it was first seen (+0x18), an entry of the computer player's Seen
  list, written from its head, the newest first ([what the mind sees](../hand/computer-minds.md#what-the-mind-sees-round-its-hand)).
- **A player's sub-action** (0xFA, exe only: `PlayerSubAction::Save` 0x651D80): a raw word +0x14 (the step's kind, the
  `HandSubAction` numbers), then its arguments, the count +0x1C as a raw word and each from the list +0x18. **An
  argument** (0xFB, `PlayerSubActionArgument::Save` 0x651F80; bw1-decomp's names): Type (+0x14: 0 a thing, 1 a point,
  3 a number, 5 an amount); Thing (+0x18, a pointer); the count 64 as a raw word and the 64 bytes from +0x1C one by one
  (0 in the frozen arguments read whose thing comes before them); IntValue (+0x5C); a pointer +0x60 (0 there
  too); FloatValue (+0x64); PointValue (+0x68). The steps are the hand's state's list +0x28, the one under way first;
  in the frozen saves a pick-up has the thing and the speed (150), a drop at a place the place and the speed, a move to
  a place the place, a spell cast at an object the object, a number (14) and a second thing, in that order.
- **The particles' container** (0x6E, exe only: `GParticleContainer::Save` 0x63E5D0; bw1-decomp's
  `ParticleContainer.h`): the GameThingWithPos part (the map coordinates its effect is kept at); a byte +0x38, set at
  its making (fn_0063E0F0) to whether its spot visual's info has SingleZSort 1 (the info's +0x4C; openblack: its effect
  is drawn `DrawPath::Queued`); the thing it hangs on (+0x28, emptied by `Process` 0x63E280 once that is no longer
  available); its particle system +0x2C (`WriteSafe` of a `PSysBase*` is a `WritePtr`: its `GJPSysInterface`); +0x30 the
  turns it has left (-1 for ever: `Process` closes it at 0); +0x34 its particle strength (`SetParticleStrength`
  0x63E3E0, 1.0 from `SetToZero` 0x63E1B0), passed to its effect each turn. A new one goes at the head of the game's
  list of containers. **The particle interface** (0xA5, `GJPSysInterface::Save` 0x6CB060): `PSysBase::Save` is the
  GameThing part; then its manager (+0x14).

With them four saves read from the game object to its players: Land 1's two saves made by the original and two of the
frozen saves. The others stop at a creature (0x69), the particle manager (0xA4), a fire (0x29), a citadel's class 0x3C
or an abode's destruction mesh.

#### The creature

- **The creature** (0x69, exe only: `Creature::Save` 0x4E4EA0): `Living::Save`, then the creature's own values in
  raffclar's byte-exact layout (`CreatureRecord`, version 33): the version; saved in the temple; the index and the
  player (bytes); the 16 spell slots (spell, phase, turns left, found, hold turns, the miracle); the queued spells (a
  count, then spell, seconds, miracle); the mind's processing word (only after version 25); the largest and smallest
  sizes; two bytes; the fizz, its target, its rate and the freeze; a word; the three spell visuals and the two hands'
  particles; the target point, its particles and object, a byte, the arena; the pen (MapCoords); the fight health, the
  auto fight; the dance partners (a count, then each) and the dance; can die, strength, size, alignment, the animation
  pace, the auto scale and its share; a word that says whether the plan follows; the plan (desire, action, three
  objects, the agenda's seven words, the steps from the current one to the count, 44 bytes each, the agenda's two
  objects); the four creed powers (scale and power). With no lists and no plan the creature's own part is 569 bytes.
  The mind goes to its own file (`SaveMind`), not to the world file.
- **A fire** (0x29, exe only: `FireEffect::Save` 0x730B00): the GameThing part; a raw word +0x34; a byte from the
  game's data (0xDA09E4); a byte +0x30; a second byte from the game's data (0xDA09E5); +0x14 and +0x18; pointers +0x1C,
  +0x24, +0x20, +0x28; a byte +0x38; pointers +0x40, +0x44; a list (+0x48, its count +0x4C by `WriteIt`); the two
  bytes from the game's data again.
- **A rock** (0x6F, exe only: `Rock::Save` 0x6E77B0): `MobileStatic::Save`, raw words +0x88 and +0x8C, a pointer
  +0x90.
- **A spell's seed** (0x24, exe only: `SpellSeed::Save` 0x729CA0): `Object::Save`; +0x6C; a byte +0x54; the info
  (+0x58); pointers +0x5C, +0x68, +0x60, +0x64; bytes +0x70 and +0x71; raw words +0x74, +0x78, +0x7C, +0x84, +0x80,
  +0x8C, +0x88; then by `WriteIt` a bool +0x90 and a long +0x94, a raw word +0x98 and a bool.
  What the fields are (bw1-decomp's `SpellSeed.h` and the names recorded with gx.py): +0x6C the power-up level
  (`SetPowerUp` 0x729B30); +0x54 SpellSeedFlags; the seed's row; +0x5C the worship icon that charged it; +0x68 its
  creator (the constructor 0x7280A0 takes it from its interface status's vt +0x1C: in the frozen saves the player for
  a seed made in the hand, the worship icon for one an icon made); +0x60 its spell; +0x64 the interface status whose
  hand holds it (one in every frozen seed); +0x70 cast (read by `Cast`); +0x71 a creature learnt from it
  (`Spell::SetHasAlreadyBeenLearnedFrom` 0x7218F0); +0x74 and +0x78 its charge and its copy; +0x7C, +0x84, +0x80 the
  chants, age and object count kept from its spell (`StoreChantsAndAgeFromSpell` 0x728780); +0x8C its power
  (`GetPSysPower`); +0x88 its cast's multiplier (the constructor's last argument, read by `DoPreCastThings`); +0x90
  ready (DrawnInHand); +0x94 its turns in the hand; +0x98 the magic of its last cast (`DoPostCastThings`); +0x72 made
  by a one-shot orb (`OneOffSpellSeed::CreateSpellIntoHand` 0x72A730 sets it, `GetScriptObjectType` reads it). The
  common init (fn_00728140) leaves +0x6C at −1, +0x88 and +0x8C at 1.0 and the rest 0.

With them ten of the eighteen saves read from the game object to its players. The others stop at the particle
manager (0xA4, in four of the frozen saves), a worship site (0x3C), class 0x103, or an abode's destruction
mesh.

#### The worship sites and the dances

- **The worship site** (0x3C, `WorshipSite::Save`, Worship.cpp:1057; the exe's order at 0x77D2F0): `CitadelPart::Save`
  (the MultiMapFixed parts, a raw word, two pointers); the tribe's info; show_needs and the dance; the towns (a count
  by `WriteIt`, then each); the FoodPot; +0xBC, +0xC0, a byte +0xC4, NumWorshippers, +0xCC, CheatSpellSeed (raw); the
  worshippers and the icons (each a count, then each); ResourcesOnTheWay (`WriteArray`: a count, then each word);
  ChantStore, TotalSacrificeValue, TotalChantsAvailable, ChantsUsed, ChantsRequested, ChantDamage (floats);
  InfiniteChants, FreeMaintenance; Slot and IsSupplyingChants (bytes); the villagers going home (a count, then each);
  the totem; Overloaded; ManaPathPSys; OverloadPulsePhase and OverloadPulse.
- **Its needs** (0x3E, exe only: `ShowNeeds::Save` 0x719CB0): a pointer +0x14 and `WritePtrArray(+0x18, 3)`. **A visual
  of them** (0x3D, `ShowNeedsVisuals::Save` 0x71A230): `Object::Save`, +0x54, +0x58, a pointer +0x60, +0x5C.
- **The worship totem** (0x98, `WorshipTotem::Save`, WorshipTotem.cpp:292): `CitadelPart::Save`, the SpecularColor and
  the site (the exe writes the pointer at +0x100).
- **A town's artifact** (0x103, exe only: `TownArtifact::Save` 0x4265A0): the GameThing part; pointers +0x14, +0x18,
  +0x24; +0x28; pointers +0x2C, +0x38; +0x30 and +0x34.
- **The group behaviour** (`GroupBehaviour::Save`, GroupBehaviour.cpp:288): the GameThingWithPos part; Time and three
  unused words; the KeyFrames (a count, then each); NumGroups, EditorValue, EditorValue2, Length; the dance, fixed and
  proportion groups (each a count, then each); NumDancers; ProportionCounter (a byte); Denominator; Speed and
  LastSpeed; the DanceMembers (a count, then each); GroupAngle; UseDanceCamera, DanceCameraDisabled,
  DanceCameraCreated, MarkedForDeletion; CameraTravelTime; CameraNearTurns.
- **The dance** (0x27, the exe at 0x50C480; Dance.cpp:243 differs): the group behaviour, then Type, Unused,
  WorshipSpeed, the Target, six raw words (+0x100 to +0x114), 16 bytes (+0x118), a raw word +0x128 and the
  FootpathLink (+0xE8).
- **A key frame** (0x7A, exe only: `DanceKeyFrame::Save` 0x50F1C0): +0x14, its actions (the count +0x1C, then each from
  the list +0x18), +0x20. **A key action** (0x79, `DanceKeyAction::Save` 0x50E880): a count +0x18, then that many words
  from its list (from the last), +0x1C, 56 bytes +0x20.
- **A dance group** (0x7B, exe only: `DanceGroup::Save` 0x50D640): pointers +0x14, +0x18; its members (a count +0x20,
  then each); +0x24, +0x3C, +0x40, +0x44 (raw words); 8 bytes +0x48; +0x50, +0x54, +0x58; 16 bytes +0x5C; then by
  `WriteIt` a float +0x6C, a pointer +0x70, a byte +0x74, words +0x78, +0x7C, +0x80, +0x84, two MapCoords (+0x88,
  +0x94), the DANCE_TYPE +0xA0, +0xA4, 64 characters +0xA8, a DanceGroupProportion (12 bytes, +0xE8), the stack +0xF4
  (`WriteSafe` of an `LHStack<unsigned long, 32>`: a count, then each), and the villager type.

With them eleven of the eighteen saves read from the game object to its players. The others stop at the particle
manager (0xA4, in four of the frozen saves) or an abode's destruction mesh (three).

#### The particles, the spell icons and the destruction meshes

- **The particle manager** (0xA4, exe only: `PSysManager::Save` 0x6CB090): `PSysBase::Save` (the GameThing part); a
  long +0x14; a pointer +0x18; its systems (a count +0x20 by `WriteIt`, then each from the list +0x1C); the
  PSysProcessInfo (60 bytes, +0x24); an LHPoint +0x60; a byte +0x6C; a float +0x70; +0x74; three 12-byte values
  (+0x78, +0x84, +0x90); +0x9C, +0xA0, +0xA4; a pointer +0xA8; bytes +0xAC and +0xAD; a Persistent (+0xB4, two raw
  words from `Persistent::GetSaveID`); the SpellTargets (a count, then a word and a pointer each; a count, then 12
  bytes each; a word).
  What the fields are (bw1-decomp's `PSysManager.h`): +0x14 its atoms' count (`DeleteFromAtomList` takes one off);
  Player (+0x18); Collections; ProcessInfo (60 bytes: InterfacePosition, GesturePosition, CameraForwardUnitVector, a
  direction, Strength, GestureAngularVelocity, a byte, three bytes not read); Origin (+0x60); Alpha; Age; State (bit 1:
  closed down, `SetState` 0x672FF0 keeping CloseDownAge = Age); +0x78; InitialVelocity, InitialDirection and
  InitialSpeed (fn_00672EC0 from the start velocity: the direction its unit vector, (1, 0, 0) with a speed of 0 when it
  is no longer than 0.0001); Magnitude; CloseDownAge; OwnerSpell; +0xAC (whether the effect is synced,
  `GJPSysInterface::Create` 0x68F2F0) and +0xAD; its file (`Persistent::GetSaveID` 0x580C30: the file's id, the particle
  type it was loaded for, `PSysFileData[type]`, and -1); its SpellTargets (the objects, each a word and the thing; the
  points; CurrentTarget). A new one (fn_00672CE0, then fn_00672B50) has +0x14, Age, State, +0x78, InitialSpeed,
  CloseDownAge and OwnerSpell at 0, Alpha 255, +0xAC and +0xAD at 1, Strength 1.0 and the byte after it 1. An atom
  collection's persistents are its modifiers, each its file's id and its place in the file's list.
- **An atom collection** (0xA3, exe only: `AtomCollection::Save` 0x6CB680): the GameThing part; its Persistents (a count
  +0x20, then two words each); its systems (a count from +0x28, then each from +0x24); a float +0x2C; a pointer +0x30;
  +0x34; a byte +0x38; a pointer +0x3C; a second list of systems (a count from +0x44, then each from +0x40); a
  pointer +0x48; +0x4C; a byte +0x50.
- **The spell icons** (exe only: `SpellIcon::Save` 0x727940): `MultiMapFixed::Save`, the info, 12 bytes +0x100, +0x10C.
  A plain icon (0x81, `GAME_THING_TYPE_SPELL_ICON`) writes only that (`SpellIconRecord`); an arena's (0x42,
  `ArenaSpellIcon::Save` 0x4256E0, bw1-decomp Arena.cpp:64) is a `Fixed` (`Fixed::Save`), then a pointer +0x58 and the
  words +0x60, +0x64 and +0x6C (`ArenaSpellIconRecord`); the reader reads both (2026-11-11, test
  `SaveCreatureRecords.TheSpellIconsTheTeleportsStoneAndTheFootpathFinderHaveTheGamesLengths`; raffclar's reader has the
  same layouts, `ReadSpellIcon`, `ReadArenaSpellIcon`). openblack makes neither.
  A town's icon (0x55, `TownSpellIcon::Save` 0x748F70) adds the town; a town centre's (0x56, 0x748FF0) adds a pointer
  and three words from +0x11C with their count; a worship site's (0x78, `WorshipSpellIcon::Save` 0x77FF80) adds a u16
  +0x114, a pointer, +0x11C, +0x138, a u16 +0x13C, a byte +0x120, +0x124, a pointer, +0x134, and a list (the count
  +0x130, then each from +0x12C).
  What the fields are (bw1-decomp's `SpellIcon.h`, `TownSpellIcon.h` and `WorshipSpellIcon.cpp:626`): the info is the
  seed's row (`seed_info`, `WriteInfo`); +0x100 SpellCoords, the icon's map coordinates as it was made (the constructor
  0x725FF0 copies its place there; a raw 12 bytes, two whole numbers and the altitude); +0x10C SpecularColor (0 from
  `SetToZero` 0x7260F0; `SetSpecularColor` 0x55D380 is reached only by a citadel heart's destruction and the healing
  chakra's effect). A town centre's icon: the town (+0x114), its town centre (`Centre`, +0x118) and PowerUpEnabled[3]
  (4 bytes each). A worship site's: RemoveCountdown, Site, ScaleBeforeRemove, TurnChargingStarted, Slot, Charging,
  PowerUpType, ChargingStatus (the `GInterfaceStatus` of the hand that charges it), ChantStore and CreatedSpells (its
  seeds). Their own row: the worship sites' "Spell Icon" is 2396, the towns' "TownSpell Icon" 2395, in every frozen
  save. The seed rows run from 2578 (the storm) down by the seed's number: in profile D's slot 15 Land 2 save each
  town's centre icons are exactly the miracles Land2.txt's `CREATE_NEW_TOWN_SPELL` gives it under that rule (town 2's
  2573, 2571, 2551 and 2550 are physical shield, heal, itchy and teleport; town 5's 2575, 2574, 2573 and 2569 food,
  shield, physical shield and water; town 13's 2562 strong). A fixed object of several cells made not as a building
  site has its FixedFlags at 0x08 (constructed, `MultiMapFixed::MultiMapFixed` 0x52E1E0; 0x02 marks one under
  construction), as every frozen icon.
- **The wonder** (0x80, `Wonder::Save` 0x779080, `Wonder::Load` 0x7790F0): `Abode::Save` and its power +0xC4 (a
  float).
- **A script's highlight** (0x3F, `ScriptHighlight::Save` 0x70A760): `Fixed::Save` and +0x5C, +0x60, +0x7C, +0x80,
  +0x78, +0x84.
- **The magic food** (0x1E, `MagicFood::Save` 0x5FAB00, `MagicFood::Load` 0x5FAB40): `PileFood::Save` and the player.
- **A workshop's building site** (0x5E): `BuildingSite::Save` (0x43C830) alone, the standard site's values without its
  wood pile.
- **A dead or felled tree** (0x70, 0x71, exe only: `DeadTree::Save` 0x5115C0): `Rock::Save`, the info and +0x9C.
- **A building's destruction mesh** (after an abode's flag when it is set; `FragMesh::WriteToFile` 0x76D680): the count
  of its primitives (+4), +8, +0x10, +0x14, then each primitive (`FragPrimitive::WriteToFile` 0x76D980): its count of
  triangles (+4), +8, +0xC, the count again, and 88 bytes each.

With them all eighteen saves read every object from the game object to its players, with every stored sum equal, and
write back the same. What the game object writes after its players is not read yet.

#### The game object's global lists

After its eight players `GGame::Save` (0x554090) writes `WriteSafe(GData)` (0x563440: eight raw words, +8 to +0x24); a
byte +0x205A59; the curse byte of that player's creature (its +0x110C, +0x1110 and +0x1114, packed); a byte +0x205A5C;
a raw word +0x205BA0; then `GlobalGameLists::Save` (0x591AC0): 44 lists, each a count and then each object (eight by
`WriteIt` of the count and `WritePtr`, then the `WriteSafe` instances, from the dances to the footpath finders); then
ten raw words, the visual time among them. The help system (`HelpSystem::Save` 0x5C6EB0) follows, and is not read yet.

The lists meet the classes the players do not reach. From the exe:

- **The street lantern** (0xFE, 0x734C90): `Object::Save`, +0x58 and a byte from the game's data (0xDA0A10). **The
  street light** (0xFF), **the waypoint** (0xA1) and **the script marker** (0x7C) write only `GameThingWithPos::Save`.
- **The poo** (0x6B) and **the mobile object** (0x53) write `MobileObject::Save` alone; **the Hanoi block** (0x30,
  0x6DB9F0) adds 12 bytes and three pointers; **the reward** (0x46, 0x6E69C0) three pointers and ten raw words.
- **The particles:** `PSysBase::Save` is the GameThing part. `RenderParticle::Save` (0x6CBAD0) adds a byte; the point
  (0xA8) writes only that; the sprite (0xB4, 0x6CC630), the chain joint (0xB3, 0x6CC540), the mist (0xAB, 0x6CBB50),
  the animated textured 3D object (0xAE, 0x6CC160, on `Particle3DObj::Save` 0x6CBE80: a Persistent and a byte) and the
  game object (0xB2, 0x6CC440) add their values. The atom core (0xA2, 0x6CB3A0) writes three lists (a count by
  `WriteIt`, then each), an LHPoint, a float, 48 bytes (fn 0x6D06E0), three floats, an LHPoint, +0x8C (fn 0x6D0730),
  +0x90, a u16, two CalculateDrawPosInfo (56 bytes each), a float, +0x10C, a PSysAnimInfo (12 bytes), +0x11C, three
  pointers and a byte. The chain (0xB6, 0x6CC780) writes its joints as a count, then 28 bytes each. The sound (0x16,
  `PSysSound::Save` 0x6D14A0) writes a word, its PSysSoundAction (24 bytes), a word, a pointer and a byte.
- **The atoms' and collections' data:** `BaseAtomModifierData::Save` (0x6CB340) and `BaseCollectionModifierData::Save`
  (0x6CB370) write the GameThing part and a Persistent (two words from `Persistent::GetSaveID`). The beam's
  sub-collection (0xB9, 0x6CE1E0: a word, its points as a count and a word and two LHPoints each, two words), the
  emitter's collection (0xED, 0x6CFEE0), the hand's sprinkle (0xF5, 0x6D05E0), the atoms at a target (0xC4, 0x6CE340),
  the trail (0xF3, 0x6D02F0: its places as a count and 64 bytes each), the beam's atom (0xBF, 0x6CD3B0), the sound's
  atom (0xDC, 0x6CD770) and the vortex's attracted atom (0xC9, 0x6CEE70) add their values.

#### The particles' rules' data

The other rules' data are a fixed row of values after the base's Persistent, each written as one value (its first byte
and its size added to the running sum): a byte (B), a word (W, a float among them), a 12-byte LHPoint (P), a 48-byte
LHMatrix (M) or a pointer (R, `WriteSafe`, which may open a new object at once). raffclar's layouts
(`VanillaWorldClasses.inl`, his commit 3a5f4d369, the same at 2a6ac938c), each checked against the frozen exe's save
function by `xref.py savelayout` (2026-11-11, all 46 the same): 0xBB `UR_VolFXOnObject` WR; 0xBD `UR_Plasma` PPPPWWB;
0xBE `UR_BeliefSprite` PB; 0xC0 `UR_ManaPathNew` (0x6CCB10) PPPPPPPWWWWW; 0xC1 `UR_CreatureSpellGeneric`'s collection
BPPBWW, 0xC2 its atom WWWWW; 0xC3 `UR_OrientSpriteWithVelocity` BPWB; 0xC5 `UR_CreatureSpellCompassion` BWW; 0xC6
`UR_CreatureSpell` BWR; 0xC7 `UR_CreatureSpellItch` B; 0xC8 `UR_CreatureSpellFreeze` BBWW; 0xCB
`UR_SphereSurfaceTracer` BWWW; 0xCC `CheckShieldDeflections` BP; 0xCD `UR_ForestPath` BWWW; 0xCE `UR_Explosion`'s atom
B; 0xD0 `UR_Flocking` BP; 0xD1 `UR_CloudMoverNew` B; 0xD2 `UR_CloudGather`'s collection PWWBWWWWRRBBBB, 0xD3 its atom
WWWBWW; 0xD4 `UR_Tornado::FlyingAtomData` WWW; 0xD5 its debris collection WW; 0xD7 its collection BPPPPPWWBWB; 0xD9
`EmitterRuleLightningSprite` WBWWB; 0xDB `AttatchFireBallToAtom` BWR; 0xDD `RemoveSoundFromAtom` B; 0xDE
`UpdateRuleGravityWithFloor::AtomDataRipple` P; 0xDF `UR_BankedTurning` B; 0xE0 `UpdateRuleShieldSpark` WWWWM; 0xE1
`UR_VapourEndEffect` B; 0xE2 `AddSubCollectionsToAtom` B; 0xE3 `CreateNewBaseAtom` W; 0xE4 `ER_GlintsOnTarget`'s parent
atom BWWR, 0xE5 its atom W; 0xE6 `UR_OrientSpriteWithRandomAngle` BW; 0xE7 `UR_SideSpin` WB; 0xE8 `UR_InitialSpin` WB;
0xE9 `RemoveRuleAfterConditionTrue` BW; 0xEA `UR_OrientWithVelocity` PWB; 0xEB `UR_FollowTargets`'s atom WR, 0xF1 its
collection B; 0xEC `UR_AddDefensiveSphere` BR; 0xEF `ER_BurstFromParentAtom` BWW; 0xF0 `ER_EmitFromParentAtom` BWW;
0xF2 `UR_HealSpellChakra` WRBBBWW; 0xF4 `LightningForkFlicker` W; 0x10E `UR_LightSheetOnObject` W. The tornado's
flying collection (`UR_Tornado::FlyingCollectionData::GetSaveType` 0x5609D0) returns 0xD5 as its debris collection
does (0x560980), so no save holds 0xD6. The plasma's sub-collection (0xBC) is written as the beam's 0xB9
(2026-11-12, `UR_Plasma::SubCollectionData::Save`, bw1-decomp PSysSave.cpp:430, the same as
`UR_SimpleBeam::SubCollectionData::Save`: its spline, Index, then +0x30): `UR_Plasma::CreateArc` 0x6767F0 makes it as
it makes each beam's chain (only when ChainCreator is a chain creator) with Index the beam's number and +0x30 NumBeams;
each step its lay 0x676980 fills the spline as the beam's does (0x675D00): NumSplinePoints keys at shares i/(n-1)
through fn_00677160, the slopes by fn_006771C0. openblack's plasma keeps the number, the count and the last laid
key points on each beam (`beam::Ribbon`, noted as made as the chain is), written by the beam's codec under 0xBC
(`gamesave::PlasmaSubCollectionRecord`, `particle_codecs::PlasmaRibbonValues`, `BeamRibbonOf`; tests
`ParticleCodecs.APlasmasBeamDataIsWrittenAsTheSimpleBeamsUnderItsOwnClassAndReadBack`,
`SimpleBeamTest.APlasmasCarrierKeepsItsArcAsItsRulesData`), and a load puts it back as the beam's. The explosion's
collection data (0xCF, 2026-11-12, `UR_Explosion::CollectionData::Save`, bw1-decomp PSysSave.cpp:901; the fields as
[the beam explosion](../magic/lightning-explosion.md) names them): "not started" +0x20 and "target remains" +0x21 (bytes),
the centre +0x24, the ring +0x30, the targets +0x34 (a `GJVector<GTPointer<Object>>`: a count, then each turn and
object), exploded +0x48, deleted +0x4C, smoke +0x50 and beam +0x51 (bytes), the beam's container +0x54 (`SaveSafe`)
and stopped +0x52, in that order (raffclar's `ReadExplosionCollection` reads the same). openblack's rule keeps them
as `explosion::BlastData`, noted as made on the rule's first call (`gamesave::ExplosionCollectionRecord`,
`particle_codecs::ExplosionDataValues`, `ExplosionDataOf`, `explosion::DataOf`, `RestoreData`); a load gives the
targets the entities it made and the beam's container once the containers are made (`explosion::LinkLoadedFx`,
after the spells whose effects hold the blast; test
`ParticleCodecs.AnExplosionsDataIsWrittenAsTheOriginalSavesItAndReadBack`). The lightning's collection data (0xD8,
`UR_Lightning_CollectionData::Save` 0x6CE3B0, bw1-decomp PSysSave.cpp:744, `xref.py savelayout` and raffclar's
`ReadLightningCollection` agree) is read and written back as the game writes it (2026-11-12,
`gamesave::LightningCollectionRecord`): +0x20 and +0x24 (`SaveSafe`, the two lightnings linked by a collision), the
points +0x30, +0x3C and +0x48, a word +0x54, bytes +0x58..+0x5A, words +0x5C..+0x68, the targets +0x6C (a count, then
each the turn and object of a `GTPointer`, a point, three bytes and a word: 27 bytes), the forks +0x80 (a count, then
each its words with their count), a word +0x88, the held collections +0x8C (a count, then each a reference and its
owned byte), a word +0xA0, +0xA4 and +0xA8 (`SaveSafe`), words +0xAC and +0xB0, the point +0xB4 (test
`SaveWorldListRecords.EachRecordHasTheGamesLength`). openblack writes it from its lightning's data and loads it back (2026-11-12,
`WriteLightningData`, `lightning::SavedBoltOf`, `particle_codecs::LightningOf`, `lightning::RestoreBolt` from the
collection's load once its atoms are made, `LinkLoadedBolts` once every effect is; test
`ParticleWrite.ALightningIsLoadedFromTheFileItWroteAndWrittenAgainTheSame`), the fields as
[the lightning](../magic/fireball-and-lightning.md) reads them in the exe (the constructor 0x68FE20, `ActualModify`
0x6914C0, the search fn_00690F50, `UpdateForkStructure` 0x691BB0, the recursion fn_00691F30): +0x20 the bolt it is
linked to and +0x24 the one linked to it (their data's records); +0x30 the origin, +0x3C the last search's origin,
+0x48 the targets' centroid; +0x54 the heading (the camera's forward heading from the hand, else 0); +0x58 the first
step to come (1 from the constructor); +0x59 CastingFromHand; +0x5A 0 (nothing else writes it); +0x5C the age since
the last search; +0x60 the distance to the centroid (1.0 from the constructor); +0x64 the search's cap; +0x68 the cap
of picks; +0x6C the targets; +0x80 the forks' lists of targets with +0x88 the next free list; +0x8C the forks with
their left-off bytes and +0xA0 the next free fork; +0xA4 the fork picked at random; +0xA8 the bolt's own collection
(set at every step); +0xAC the forks' scale; +0xB0 the turn it was made; +0xB4 the clash point. Where nothing has set
them yet the original's data holds the particle allocator's fill: +0x88, +0xA0 and +0xB4 (openblack starts them at
it); +0xA4, a pointer, is written as none in openblack (the original would write whatever that fill points at).
- **The land:** the feature (0x74) writes `MultiMapFixed::Save` alone; the animated static (0x75, 0x422AA0) adds its
  frames (a count, then each word) and +0x7C; the bonfire (0x72, 0x439990) adds a word to `Rock::Save`; the mist
  (0x48, 0x606920) three words; the stream (0x47, 0x734380) its nodes (a count +0x18, then 16 bytes each) and two
  words; the influence ring (0x49, 0x5CDC60) the object it follows (its info's base, +0x28, null once that object is
  gone), its player (+0x34), its radius (+0x38, a float) and its anti word (+0x3C, any but 0 for an anti ring); the weather
  thing (0x43, 0x7745C0) 80 bytes, a word, an LHPoint, 80 bytes, two words, an LHPoint and a word.
- **The climate** (0xFC, 0x773320): the GameThing part; 12 bytes; four words; the info; three words from the game's data
  (0xDCB8B8, 0xDCB8C0, 0xDCB8BC); 16, 8 and 12 bytes; +0x58; a byte +0x64; its areas (a count, then an LHPoint, 80
  bytes from fn 0x773B60, two floats and an LHPoint each); seven words and a byte.
- **The spells:** `Spell::Save` (0x721930): the GameThingWithPos part; a word, a pointer, a word, a pointer; three
  12-byte values; six words; 60 bytes (fn 0x722070); a float; two bools; four pointers; a word (fn 0x7220C0); three
  floats; a MapCoords; two floats. `SpellWithObjects::Save` (0x7211D0) adds its objects (a count, then each). The water
  (0xC, 0x725420) adds a word; the shield (0x11, 0x72BC40) a list and two pointers.
  The other classes (exe only; their numbers in the game's table of save types): the plain spell (0x9, fireball,
  lightning bolt and the beam's explosion), the storm and the tornado (0x10) and the healing spell (0x12) write only
  `Spell::Save`; the food and wood spells (0xA, `SpellResource::Save` 0x725350) a byte (+0xEC); a spell with objects
  (0x14) and the teleport (0x15) `SpellWithObjects::Save` (the objects: the count +0xF0, then each from the list
  +0xEC); the flocks (`SpellFlock::Save` 0x7248A0: a spell with objects, a pointer +0xF4, words +0xF8 and +0xFC, 12
  bytes +0x100, a word +0x10C; on the ground 0xE, `SpellFlockGround::Save` 0x724BD0, nothing more; flying 0xD,
  `SpellFlockFlying::Save` 0x724AC0, words +0x114 and +0x118 and a byte +0x11C); the forest (0xF,
  `SpellForest::Save` 0x725C90: a byte +0xF0, a pointer +0xEC, a word +0xF4); a creature spell (0x13,
  `SpellCreature::Save` 0x722590: a pointer +0xEC, a byte +0xF0). The reader reads them all (`components/gamesave`
  `WorldListRecords.h`, test `SaveWorldListRecords`); no frozen save holds one (every frozen save reads to its end
  without them).
  What the fields are (bw1-decomp's `Spell.h` and the names recorded with gx.py): the words 0x12C859 and 0x2A1C9A
  around its particle system (+0xB0, `PSysInterface::Create` in `InitWithPos` 0x71FE50) and its reaction (+0x28); the
  movement's direction (+0x2C, (1, 0, 0) from `SetToZero` 0x71FCF0), SpellCastPos (+0xCC, map coordinates) and the
  direction (+0xD8, the process info's at `InitWithPos`); Life (the chants) and +0x3C (those it started with: `SetChants`
  writes both), closedDown, castByMyInterface (+0x44: whether the status `SetInterfaceStatus` 0x7201F0 is given is this
  machine's), +0x48 and +0x4C (cast by a creature, by a human player: the constructor 0x71FB40); the 60 bytes of the
  process info (+0x64); +0x50 the duration (`ProcessMaintainRequest` compares the age with it); +0x5C free and +0x5D
  (written only by `SetToZero`, 0); the creator, the player, the casting hand's interface status (+0xA8, from the seed it
  was cast from, `DoPostCastThings`) and the seed; the magic type; +0xB8 the age, +0xBC the magnitude, +0xE4 the strength
  multiplier; +0xC0 the place it was first cast at; +0x54 and +0x58 the mana path's counts at the cast place and at the
  spell's place (`CreateSpellPoint` 0x7213D0). The water spell's word (+0xEC) is the age it last made a ring at
  (`SpellWater::Process` 0x724ED0).
- **The shields and vortexes:** `MapShield::Save` (0x72C390) is `Fixed::Save`, a pointer and the info; the physical
  shield (0x17, 0x72C700) adds two words, 52 and 52 bytes, six words, a byte, a bool, a word and a pointer.
  What the fields are (the names recorded with gx.py, checked on profile D's slot 16 Land 2 save and the original's
  Land 2 save): the map shield's spell (+0x60) and its magic info (+0x64); the physical shield's +0x68 (1.0: only
  `SetToZero` 0x72CB70 writes it), its creation turn (+0x6C), its matrix's rows with its translation and scale (+0x74,
  52 bytes) and the last turn's (+0xA8), its start and final scale, its start and end spin, its angle, its bob
  (+0xDC..+0xF0), whether it is dying (+0xF8), its alpha (+0x70), its die time (+0xFC) and its effect (+0xF4,
  SF_PhysicalShieldFX). The shield spell's list after its objects is its anti-influence rings (+0x104, the count
  +0x108); then its struck reaction (+0xF4) and its shield reaction (+0xF8, REACT_TO_MAGIC_SHIELD, made at every cast).
  The physical shield's matrix is its scale on the diagonal turned about Y by its angle in `PhysicalShield::ProcessShield`
  0x72D190 (the cosine stored as a float, the sine unrounded, each cell a float; the last turn's is the matrix copied
  before the turn); its place's altitude is its height above the land (the same process), while its object's target
  keeps the place it was made at with altitude 0, its object's scale is its last rescale's and its heading 0 (both
  saves). The info ids of the physical shield (2083) and of its magic (2642) are the two saves'; the spiritual shield
  (`MagicShield`, class 0x18: `MagicShield::Save` 0x72C3F0 and `Load` 0x72C3D0, bw1-decomp SpellShield.cpp:111, are
  `MapShield::Save` 0x72C390 / `MapShield::Load`: a fixed object, its spell (+0x60) and its magic's info (+0x64), 79
  bytes with the part every object writes; gamesave `SpiritualShieldRecord`, test `SaveWorldListRecords`) is in no save
  read. `MapShield::Create` 0x72BE20 makes it of `GMapShieldInfo::Infos` row 0 (`MAP_SHIELD_INFO_MAGIC`; the physical
  shield row 1), and `MapShield::MapShield` 0x72C070 takes its magic's info from its spell (`SpellShield::GetMagicInfo`:
  the shield table's row 0, MAGIC_TYPE_SHIELD 19; the physical shield's row 1, 20). Each table's rows take their info ids
  from its first down (`GBaseInfo::SetInfoID`, one id per row in load order, `LoadIt` and `LoadMagic` in bw1-decomp
  Balance.cpp), so the spiritual shield's ids are the physical shield's plus one: 2084 and 2643 (worked out from the
  physical shield's saved ids; no save holds a spiritual shield to check them on).
  `LandscapeVortex::Save` (0x600000) is `MobileStatic::Save`, 12 bytes, four words and bytes with their count; the
  vortex into another land (0x1B, 0x600460) adds two pointers and words with their count. The vortex's save of the land
  (0x106, `VortexSave::Save` 0x76FB40) writes a byte, a word and two texts with their counts, and copies its land's
  file beside the world file.
  Read 2026-11-10 (bw1-decomp MagicVortex.cpp, the frozen profile D slot 14/1 save, Land 1's leave-through-vortex
  scroll): the In's record (0x1B) is the mobile static's parts of the vortex's row (1925, no owner, upright, built flag
  0x08, the thing's hidden-name mark 0x40, its place's script marks), then Centre (+0xD4), Type, State, Radius (50.0),
  AverageHeight (+0xC4) and the 121 heights it flattened (`OriginalHeights`), then its crossing file's save
  (`VortexSaver` +0xEC), its object mover's particle interface (+0x98) and `Saved` (five counts); the save there is
  mode 0 (a writer), 0 lines, the path of `vortex.txt` in the save folder and the world file's name with "vor". The
  Out (0x1C) and the volcano (0x1A) are classes of their own that no save read holds: the volcano writes the vortex's
  part only; the Out (`LandscapeVortexOut::Save` 0x600630, bw1-decomp MagicVortex.cpp:864) adds AnimalFlock (+0xEC),
  FlockToJoin (+0xF0), FlockCoords (+0xFC), FlockDistance (+0x108), DomainRadius (+0x10C), its crossing file's save
  (+0xF8, the reader), `Created` (+0x118, five counts: +0x118 the objects, +0x120 their wood and food, +0x124 the
  villagers, [the way out](../world/vortex.md)), the thrown things (+0x12C, a list) and TownToJoin (+0xF4). The
  crossing file's save's load (`VortexSave::ResolveLoad` 0x76FFE0) opens the file again: a reader passes over the
  lines it had read (its word, `DontProcessNumLines`), a writer seeks to its end. openblack
  (`src/ECS/SavedWorld/WriteVortices.cpp`, `saved_world_load::MakeVortexEntity`): the In is written from its
  component, its crossing file's save as a source keyed by its vortex (the path in the save folder); ⚠️ its object
  mover is written as none and its counts as 0 (openblack counts nothing an In takes). The copy of `vortex.txt` beside
  the world file (2026-11-14): `VortexSave::Save` 0x76FB40 names it "%svor" (0xC246E4) of the world file's own name,
  the `<slot>.sav.tmp` the save writes (so it keeps that name once the world file is renamed: profile D's slot 14/1
  holds `.\Profiles\<key>\Saved Games\14\1\14.sav.tmpvor`, 3902 bytes as its `vortex.txt`, and the record's two
  texts are `.\Profiles\<key>\Saved Games\vortex.txt` and that name), writes the crossing file's path and that name,
  then closes the crossing file and copies it there with `CopyFileA` (over a copy that is there) and opens it again
  where it was; `VortexSave::Load` 0x76FDC0 reads the two texts and copies the second over the first with `CopyFileA`
  as it reads the record, before any resolve opens the file. openblack: `SaveGameSystem::CrossingCopy` gives the
  name, the writer puts it in the record (`WorldSource::crossingCopy`, a loaded record's crossing file's path as kept),
  and `SaveGameSystem::Save` copies each crossing file the records name (`saved_world_write::CrossingFiles`) as the
  world file's temporary file is written, a missing one not copied; `Game::LoadSavedGame` copies each back
  (`vortex_save::PutBackCrossingFiles`) before the objects are made, none in a run of the checks, all through the
  file system (tests `SaveGameSystem.ASaveCopiesItsCrossingFilesBesideItsWorldFileAsItWritesIt`,
  `VortexDeletion.ACrossingFilesSaveNamesItsCopyBesideTheWorldFile`,
  `VortexSave.ALoadPutsEachCrossingFileBackFromItsCopyBesideTheWorldFile`). A load makes the In again as a script's CREATE does (its effects
  started again, its crossing file opened again, empty), then puts back its state, radius, heights and their mean, its
  marks, creation number and record. The original's vortex is an object that takes a creation number as it is made
  (2979 in that save: `LandscapeVortex`'s constructor, MagicVortex.cpp:703, makes a `MobileStatic`, whose object takes
  and raises the counter); openblack's `vortex::Create` takes it too since 2026-11-10 (test
  `VortexDeletion.AVortexTakesTheNextCreationNumber`; before, none). Test
  `VortexDeletion.AWayInIsWrittenAndALoadMakesItAgain`. The way out and the volcano (gamesave
  `LandscapeVortexOutRecord`, `LandscapeVortexVolcanoRecord`) are written from the component the same way (the Out's
  flocks, flock place (`FlockToJoin` = the script's flock, its lengths = `flockA` / `flockB`), counts (`statObjects`,
  `statResources`, `statVillagers`; the other two as kept or 0), thrown villagers and town; its crossing file's save
  while it still reads, mode 1 with its lines read), and a load makes them again with `vortex::Making::Loaded`: a way
  in's file opened to be written after what is there, a way out's reader passed over the lines it had read
  (`vortex_save::SkipLines`), none once it had read the whole file, and the script's last created object left as it
  is. The file read is `vortex.txt` as the load put it back from the copy beside the save (above). Tests
  `VortexDeletion.AWayOutAndAVolcanoAreWrittenAndALoadMakesThemAgain`, `VortexSave.ALoadedReaderPassesOverTheLinesItHadRead`.
- **The arena** (0x41, `GArena::Save` 0x4250D0): the GameThingWithPos part, three words, `WritePtrArray(2)` and a
  pointer: +0x28 (1.0 from the constructor 0x4246F0, written by nothing else but `GArena::Load` 0x4251D0), +0x30 the
  radius, +0x34 whether a fight is on in it (0 from the zeroed allocation, 1 from fn_00424AB0 as a fight takes it,
  0 again from fn_00424B60), the two fighters +0x38 (the creature that made it, fn_00424AB0 0x424ACE) and +0x3C (its
  opponent), and +0x44, an interface status, 0 from the constructor and **never set in play**: its only writers,
  fn_005DA4D0 / fn_005DA590 (0x5DA51A / 0x5DA5DA, from `GPacket::ProcessPacket` 0x63C90F / 0x63C927), store it only
  into the result of a `dynamic_cast` to `GArena` of fn_005D2C20's object, which is the player's creature
  (`GPlayer::creature` +0xA4C, 0x5D2C26) while it fights; a creature is never an arena, so the cast is always null
  (its readers fn_005D2C40 / fn_005D2E60 cast the same way). +0x2C (its light sheet)
  and +0x40 (1 for an arena made for a fight, 0 for one found) are not saved. Only the maker's creature +0x10A0 names
  the arena (0x507570); the opponent's stays 0. Land 1's three arenas (frozen autosave, objects 2003, 2005, 2007,
  each read inside its REACT_TO_FIGHT reaction) are Land1.txt's, with the words 1.0, the radius, 0, no fighters and 0.
- **The pot** (0x58, `Pot::Save` 0x66D730): the MobileObject parts, ResourceType, the town, the amount and a byte.
- **The puzzle game** (0x2C, 0x6D96C0): the GameThingWithPos part; four words; six words with their count;
  `WritePtrArray` of 6, 4 and 4; seven pointers; nine cells of a byte and three pointers; eight rows of nine pairs of
  pointers, each followed by two pointers and two words, and a ninth row of nine pairs; 18 pointers;
  `WritePtrArray(20)`; then words and pointers. No stored sum follows its record before the lists end: a first reading
  with one pair per row ended the three saves that hold one (in the Hanoi puzzle's blocks) about 97,000 bytes before
  the help system's own counts (18, 152, 9 and 152 words), which the nested rows put back in place.

A list's writer stops writing the whole file when a list holds more objects than its count; the reader takes the count
as written.

With them all eighteen saves read the game object up to its help system, with every stored sum equal, and write back
the same; in each the help system's counts stand where the read ends.

#### The end of the game object

After the ten words `GGame::Save` writes, to the end of the file:

- **The help system** (`HelpSystem::Save` 0x5C6EB0): four arrays of words, each with its count (written as the
  constants 18, 152, 9 and 152); +0x55C and +0x560; three pointers (two of them null); a u16 +0x56C; +0x570; a pointer
  +0x574; +0x578, +0x57C, +0x580; six words with their count (+0x584); +0x59C; `WriteArray(+0x5A0, 6)`; then by
  `WriteIt` three ints, a u16, a word (fn 0x5C7CF0), a word, a u16 and 17 words (+0x45D8 to +0x45C8, with the helpers
  0x5C7D40, 0x5C7D90, 0x5C7DE0 and 0x5C7E30 writing one word each); and 1,024 entries of 16 bytes from +0x5C4 with
  their count (fn 0x5C7E80, the count passed as the constant 0x400).
- **The help profile** (`HelpProfile::Save` 0x5C4820) writes to its own file (`LHSaveData`), nothing here.
- **The script** (`GScript::Save` 0x6EBEA0): its state goes to its own `Script_` file (fn 0x6EB980); here the 511
  objects of its table (0xD9680C to 0xD98FF8, 20 bytes apart; fn 0x70D690: the object, from its info's base, a word
  +0xC and a byte +0x10); five words from the game's data (0xD95C04 on); two 12-byte values; four words; then by
  `WriteIt` four words, two pointers, four words, a word (fn 0x6EC800) and two words.
- 12 bytes (+0x25009C) and two pointers (+0x2502C0, +0x250534).
- **The camera's exclusions** (`CameraExclusion::SaveExclusionFile` 0x455A10, written into the world file): a version
  word (1); UseExclusions, DrawForceField, UseMaxHeight, UseMaxDistance, MaxHeight and MaxDistance; the force field's
  points (their count, then 12 bytes each); the exclusions' count; the constant 40; then each exclusion's name
  (`WriteArray` of 40 characters: a count, then each).
- The camera help's EnabledFeatures, AutoPitchParam1 and AutoPitchParam2, and the creature in the temple; the snow cover
  (`WriteArray` of 16,384 and 1,024 floats, then a float and an int); the ambient weather (8 bytes); +0x25017C (fn
  0x556F30); ScriptRebootRequested; three ints, LastAutoSaveTurn and a word.
- **The bookmarks** (`Bookmark::SaveAll` 0x43A3F0): eleven bookmarks (`Bookmark::Save` 0x43A4E0: a byte, 12 bytes, a
  pointer, 12 and 12 bytes), FollowBookmark, CurrentBookmark and a word. **The land's balance** (`GLandBalance::SaveAll`
  0x5E2990): 32 bytes.
  - For the saved games' work (S1), where openblack keeps what the bookmarks save: each of bookmarks 0..7 is a
    `components::CameraBookmark` (the flag set = `position` has a value, Pos = `position`, Target = `target`,
    CameraOffset = `cameraOffset`, written twice by `Bookmark::Save`), bookmark 10 is `CameraBookmarkSystem`'s view
    before a jump, 8 (the town's, Tab) is not ported and 9 is never set; FollowBookmark, CurrentBookmark and
    ShowBookmarks are the system's followed, current and enabled ([the bookmarks](../camera/player-camera.md#the-bookmarks-markers-in-openblack)).
- **The hand** (`CHand::Save` 0x46EB10, written when the interface has one, as in every save read): a word, a pointer,
  four words, 12 bytes, two pointers and a byte.
- **The audio** (`GAudio::Save` 0x428310, through `GGlobal::Global`'s audio): its sounds (a count, then each object),
  two words, a pointer and a word.
- RoomsAvailable; the world room's buttons (`SaveButtonConfig` 0x79F640: five words); the credits room's "did you know"
  pages (`DYKSave` 0x78CE70: five arrays of words with their counts, each followed by a word); the challenge room's
  challenges (`ChallengeSave` 0x784C70: a count, then each (fn 0x7814E0): a u16, a name with its count, three words,
  two 12-byte values, four words, two arrays of words with their counts (fn 0x784FA0, 0x785050) and a word); and the
  script's curse of the creature (12 bytes).

The tail meets new classes through the script's table and the audio: **the camera** (0x77, `GCamera::Save` 0x4430E0:
the GameThingWithPos part, 12 bytes, four words, 8, 144, 144 and 48 bytes, a byte, then its mode's save number
(`GetSaveID`) and the mode's own `Save`: nothing for the plain modes (0), six words, two 12-byte values, a pointer and
a word for the follow and script modes (0x46), four words and a pointer for the heading mode (0x48), four words for the
free mode (0x65)); **the script's timer** (0x7D, `ScriptTimer::Save` 0x711700: the GameThingWithPos part and two
words); **an object of no other class** (0xF6: `Object::Save`); **a thing's music** (0x109, `ThingMusicInfo::Save`
0x429950: a word, a pointer, four words, 12 bytes); and two particle data, **the willow wisp's collection** (0xEE,
0x6CFFC0) and **the atom that fades out** (0xDA, 0x6CD660).

With them all eighteen saves, the frozen ones and the two made by the original, read to their last byte with every
stored sum equal, and write back byte for byte the same.

#### The classes a save meets

Without lengths in the stream, a reader can go only as far as the classes it knows. In the order a Land 1 save meets
them: GGame, GPlayer, GComputerPlayer, PlayerActionState, GComputerAttitudeToPlayer, GameStats, the citadel and its
heart, footpath links, footpaths and nodes, towns, storage pits, food and wood piles, forests, big forests, trees, the
creche, the graveyard, fields, abodes, villagers, special villagers, town centres, totem statues, fish farms, magic
wood, town desire flags, horses, flocks, planned abodes, the players' interfaces, their statuses and leash statuses,
cows, pigs, the citadel's entrance, street lanterns, seagulls, doves, footpath link saves, street lights, climates and
the camera; a Land 1 save also meets bats, swallows, tortoises, particle containers, bonfires, the PSys interface,
manager and atom collections, reactions, arenas, mobile objects, mist, script highlights, streams, waypoints, rocks,
features, mobile statics, dead trees and animated statics **🔍 inferred** (a census by the checksum chains: a
candidate header is kept when its sum grows along the file).

A Land 1 autosave (436,347 bytes) holds about 3,175 objects of about 60 types (1,346 trees, 792 footpath nodes, 132
rocks, 109 mobile statics, 84 footpaths, 64 computer attitudes, ...); a Land 2 save (1,010,636 bytes) about 8,639,
with 3 creatures, 619 dance key actions, 252 villagers, worship sites, citadels and scaffolds.

### What a load does

`LoadAllGame` 0x5587B0 (GameOSFile.cpp:377):

1. The loading box (`GameLoadingBox(false)`, which draws nothing on any load of play: [the loading box](#the-loading-box)); a buffer of 256 `PhysicsSaveInfo` (matrix, velocity, angular velocity: 0x48 bytes each).
2. It opens the file read-write and reads the attempts counter; at 2 (`GAME_OS_FILE_MAX_LOAD_ATTEMPTS`) the load is
   refused, else it writes the counter + 1 back. A save that crashed the game twice while loading is not offered again,
   and a load changes the file's date.
3. It opens the file again read-only and reads the revision and the landscape (neither is checked).
4. `script->Reset(1)`, `ClearMap()`, every player's computer player deleted, the dead list processed, then
   `landscape.Open(<landscape>)`.
5. `Loading` = true, `LoadInstance(&game)`: the whole graph is read; each object is `Load`ed as it is made and added to
   the file's GameThing list. `GGame::Load` (Game.cpp:2817) mirrors `Save`, except that it sets NeutralPlayerIndex to
   7; rebuilds the terrain map, the sound map and the gesture system after the GGameInfo values; clears
   `LH3DAtmos::ambient` after reading it; and sets LastAutoSaveTurn (and +0x205A1C) to the loaded GameTurn.
   `Object::Load` queues the physics state into the buffer (`PhysicsSaveInfo::ReadInfo`, at most 256).
6. `ResolveAllLoads` 0x561C60: for every loaded object, from the list's last node, `ResolveLoad` (Object's:
   `CallVirtualFunctionsForCreation`, then, for an object in the physics, a new physics object given the next buffered
   state; GGame's: the hand, RenderTime = GameTurn × 100, the time increments cleared, `GLandBalance::ReInit`, the help
   system); then every creature's interface `ResolveLoadForCreature`; then the script creature curse; then the pointer
   and object lists are emptied.
7. The player's creature gets its three leash switches from the saved byte; every Living in a move-to state from
   LINEAR to EXIT_CIRCLE_CW resolves its circle-hug info.

The order of step 6 (read in the disassembly): `LoadInstance` adds each object to the file's GameThing list after its `Load`
returns (`LHLinkedList::Add` 0x564460, at the head), and `ResolveAllLoads` takes the list's last node each time
(0x561C66..0x561C7A). So the links are put back in the order the objects' reads ended: an object read inside another
(at its first reference) before it, and the game object last. The global lists, a town's lists and a forest's lists
are read with `AddToEnd` / `AddToLast` (GameOSFile.h `ReadSafe`), so they keep the saved order.

A loaded tree (read in the disassembly): `LoadInstance` makes it with `Base::operator new` 0x4366F0, which clears the whole
object (0x4368C4 `rep stosd`), then the default constructor (`Fixed()`, `Tree::SetToZero` 0x749F60: +0x58, +0x5C's low
word and +0x68 only), and `Tree::Load` 0x74C2A0 reads +0x60, +0x64, the forest +0x68 and +0x5C's low word. Nothing
writes +0x5E: its bit 0 (growing) and bit 1 (the script's non-scenic flag), which the full constructor 0x749E00 sets,
stay clear, so a loaded tree never grows (even in a forest) and is not marked non-scenic. The resolve
(`Object::ResolveLoad` 0x63A020 → `Tree::CallVirtualFunctionsForCreation` 0x749F70) makes its 3D object, puts it in
the map cells (`Object::CallVirtualFunctionsForCreation` 0x636BE0, unless it is unavailable or flown by the particles),
sets its matrix from Pos, the ground, its scale and its angle, and sets only +0x5C's wind bits (2..5) from the angle;
no random number is drawn. A tree joins no forest list in its load: the forest's own record lists its trees.

A loaded big forest (read in the C and the disassembly): `BigForest::Load` (BigForest.cpp:59) reads the MultiMapFixed parts, its
forest and +0x84, the wood it holds as a float (`BigForest::GetWoodValue` 0x4390B0 is the life times +0x84); its
resolve (`BigForest::CallVirtualFunctionsForCreation` 0x439050) is the MultiMapFixed one (the map cells of its shape)
and its 3D object's flags, with no random number. Its forest is the forest's own record, and the forest points back at
it through its own record's big forest. Its row: every frozen save's big forests have the row number 369, the first of
the four big forest rows (Land 1's and Land 2's scripts make only row 0), so the rows run from 369 down as the tree rows
do from 2115.

A forest's two words of the whole game are the turn the last tree was planted by a forest ([0xCD04C8], read by
`Forest::Process` and `Tree::ApplyWaterSpell`) and the next forest id ([0xBEA238]); `Forest::OnClearMap` 0x53B2F0
sets them to 0 and 1, and each forest's load sets them again. The forest constructor 0x539BD0 takes the counter's id
when given 0 (and adds 1), and when given an id raises the counter to id + 1 only if the counter is below the id.

When a load's random draws count (read in the C and the disassembly, 2026-11-08): `GGame::Load` reads the eight
players, and through them depth first the creatures, towns, flocks and animals, **before** GData and its two seeds,
so every draw made inside an object's `Load` (a constructor's, `Creature::Initialise`'s) is overwritten by the seeds.
`ResolveAllLoads` runs after the whole read, so the draws of the resolves (a creature's `LoadMind`, an animal's first
clip) come after the seeds and move the stream, in the order the reads ended.

A loaded animal or bird (read in the C and the disassembly): made by its default constructor (no age draw);
`Animal::Load` 0x41B710 reads `Living::Load` 0x5EE960, the town, Hunger, Tiredness, BreedingUrge, +0xEC, FoodPos, the
pointer +0xFC, PhysObPos, SleepPos, EnvironmentPos, DeathReason and AngleZoomer, then `LoadStateFunction` 0x41BAA0 →
`CallLoadStateFunction` 0x41BA20, which calls the top state's load entry (+0x40 of its `Animal::StateTable` row). The
table's initializer 0x5AC9F0 writes 0 into the save (+0x30) and load (+0x40) entries of all 53 rows (its only
non-zero entries are at +0x0, +0x10, +0x20 and +0x60), so no animal state reads anything and, unlike a villager's, no
state counter is drawn again. Its resolve (`Object::ResolveLoad` 0x63A020 → `Animal::CallVirtualFunctionsForCreation`
0x41A040) **clears the AngleZoomer** it read, then `Living::`'s 0x5EC9B0: the 3D object, `InsertMapObject`, and
`SetAnim(1)`, the clip of the state it was loaded in: a bird in a state whose clip is its move clip draws there
(`Seagull::MoveAnimation` 0x41BFA0: `GameRand(2)`), as an eating animal draws its eat clip. The predator reaction
spread and the lair of a new animal are not made on a load.

A loaded creature (C: CreatureMentalSaveAndLoad.cpp:275 and :383, `Creature::ResolveLoad` 0x4E6AB0): `Creature::Load`
calls `Initialise` (its constructors' draws are overwritten by the seeds), which links the player to it
(`GPlayer::SetCreature`), makes its own REACT_TO_CREATURE reaction (at the head of the game's reactions list, which
`GlobalGameLists::Load` 0x592040 does not empty: the saved reactions are added after it, `AddToLast`) and puts it at
the head of `Creature::CreatureList`, so the list after a load is in the order the creatures' reads began, newest
first, not the saved order. For player 0 in a single-player game its species is the profile's mind file's
(`.\Scripts\CreatureMind\<profile file>`), not its record's row. Its resolve reads the mind from the profile's file for
`MyPlayer()`'s creature and from `<slot>[\<land>]\SavedCreature<index>` (0xBDFF10) for every other; only when that
file opens and is not empty does it `LoadMind` and go into the map (`Living::ResolveLoad`). After `ResolveAllLoads`,
`LoadAllGame` sets the local player's creature's three `LeashEnabled` from the saved byte: bit 0 evil, bit 1 rope,
bit 2 good.

The start-up load: `GGame::Init` tests the start mode (+0x25017C) at 0x54F6B3; at 0 it calls `IsAutoSaveValid`
0x564160 (0x54F6BB) and sets the mode to 1 (a new game) when there is no valid autosave; later, at mode 0, `AutoLoad`
0x563FF0 (0x54F777: slot 20, `LoadAllGame`), and a failed load sets the mode to 1. So the original loads the profile's
slot-20 autosave in its own start path. The start mode is the autosave's at `GGame::GGame` and as a restart picks
another profile (`GAME_MODE_RESTART`), a new game's for `-newgame` and Start a New Game (PCMain.cpp, Game.cpp:345-372).
openblack does the same (`startup::StartOf`, test `StartupRules.TheAutomaticSaveIsLoadedOnlyWhenAskedForAndReadable`):
`Game::LoadStartLand` loads slot 20 when the start asks for it (the program's start and the menu's Restart; not Start a
New Game nor a skirmish), no land was asked for on the command line, the player has a profile and
`SaveGameSystem::IsAutoSaveValid`; else, or when the load fails, a new game starts. A run that keeps no profile (every
check) has no autosave and starts a new game as before.

Checked against bw1-decomp (2026-11-13): no profile at all sets a new game and shows the new player's box
(PCMain.cpp:639); `IsAutoSaveValid` (GameOSFile.cpp:1942) is the profile's autosave path with `20.sav` existing and
opening (it reads the load count and the revision, and answers yes whatever they hold); `AutoLoad` (GameOSFile.cpp:1908)
is `LoadAllGame` of the same file with the save room's slot set to the autosave's. The player is not asked before the
load; after it, `GGame::Loop` (Game.cpp:1752, 1795) opens the main menu, once the in-loop logo has played, with the
cursor on and the start-up menu active, only when the start stayed the autosave's (a failed or missing one is a new
game's by then). openblack opens its main menu there too (`startup::OpensOnMainMenu`, `Game::StartRun`; test
`StartupRules.AGameStartedFromTheAutomaticSaveOpensOnTheMainMenu`). A start from the autosave takes back the scripts'
state once the scripts' machine is ready, in place of starting the story, and keeps the camera the save holds (the start
camera is made only for a new, skirmish or multiplayer start, Game.cpp:438).

8. The counter is set back to 0; the game timer restarts at GameTurn × GAME_MILLISECONDS_PER_TURN; the buffer is
   freed; the loading box closes.

### Checked on the real saves

| Check | Result |
|---|---|
| The frame, the game object and the first player's start, on profile D's slot 20 (Land 2) and profile A's (Land 1) | every field where the C puts it; the stored sums 0xD83 / 0xD82 (the game object) and 0x1387 / 0x1384 (player 0) equal the recomputed ones, and so does object 3's (the player's computer player, type 0x66) |
| GameFlags and LandNumber | 0x00810024 / 2 and 0x00010024 / 1, as the landscape files say |
| All 16 frozen `.sav` (9 on Land1.lnd, 3 on Land2.lnd, 4 on MULTI_PLAYER\MPM_3P_1.lnd) and a new game's save made by the original | the game object's, player 0's and object 3's sums all equal (51 of 51) |
| The Object → Fixed → Tree chain | after 697 of 765 (profile A, Land 1), 703 of 777 (profile C's slot 20) and 149 of 202 (profile D's slot 20) inline Tree records the next object's stored sum equals the running one; the others are followed by something other than a new object (a list's end) **🔍 inferred** |
| `SavedCreature*` (45 files) | all read by openblack's mind file reader and written back byte for byte |
| `Script_*.sav` (the 16 frozen ones and the two reference saves') | each read whole by openblack's LHVM file reader, state included, and written back byte for byte (test `LHVMSaveLayoutGameFiles.EveryScriptSaveReadsAndWritesBackByteForByte`): version 7, 398 global names and 399 global values, 514 scripts, 157,160 instructions, 0 to 36 tasks; together 214 tasks, 1,828 locals, 363 handler addresses and 211 true states, but every stack empty. The frozen copy's `CreatureIsle\Profiles` holds 11 more, Creature Isle's (version 8), another game's |
| `SaveGame.inf` and the pictures | the layouts above |

Two saves made by the original serve the local checks (local data in `_gamedata\original_saves`, never in a
repository): a new Land 1 game's slot 20, written by the game when the temple heart is built (the moment the checks'
new game reaches), and the frozen Land 1 autosave loaded and saved again after 105 s, where only what moved differs
(the running sums of every record after the first change, the animals, streams, a few bytes of villagers and flocks,
and 107 bytes from the camera's record on, where the ambient light that `GGame::Load` clears and LastAutoSaveTurn are)
and `Script_20.sav` is byte for byte the frozen one **🔍 inferred**: so the original's load and save seem to keep
everything that did not move.

## openblack

- **✅ faithful** the slot's creature mind files are read and written in the original's layout
  (`components/creaturemind`; all 45 mind files of the frozen saves write back byte for byte).
- **✅ faithful** the script machine's file is read and written in the library's layout (`components/ScriptLibrary`,
  `LHVMFile`, from raffclar's tree): the program, the main stack with each place's type and then its value, the
  globals, the tasks (their four states written 0xFF when true and read true when not zero, a task's script found by
  its id, "Not compiled" when there is none) and the five words. The 16 frozen script files and the two reference
  saves' write back byte for byte; synthetic goldens pin each part, the stacks' order included, which no real file
  shows (tests `LHVMSaveLayout.*`, `LHVMSaveState.*`). The game calls the machine's `SaveState` when it saves a
  slot (`src/ECS/SavedSlot.cpp`) and `RestoreState` when it loads one (`Game::LoadSavedGame`). `SaveState` writes the tasks in rising number order, which is the library's list order: a new
  task goes at the list's end (0x10006011..0x10006030, and a restored one at 0x100078C4..0x100078E3) and the numbers
  only grow (fn_10008870).
- The random streams can be saved and restored as the game data keeps them (test `GameRandom.SaveLoad`).
- The save service (`Locator::saveGame`: `SaveGameSystemInterface`, `Implementations/SaveGameSystem`): the current
  profile's slot folders (asked of the profile each time; none with no profile kept, so a run of the checks reads and
  writes nothing), each slot's summary (its `SaveGame.inf`, whether its world file is there, its load counter and
  whether a load would begin; the info read beside the world file, in one folder, as fn_00792A20 opens `%d.sav` and
  `SaveGame.inf` in the folder the room found, so the previous land's and the next land's come from the land before's
  and the land after's folders, test `SaveGameSystem.ThePreviousAndNextLandsSummariesReadTheirInfoBesideTheirWorldFile`;
  before 2026-11-10 the info was read from the land's own folder; that folder is the room's, `gamesave::RoomFolderOf`
  as `SaveGameRoom::InitEngine` 0x78FD10 lists and sub_7934C0 deletes: slot 14 in the land before's only above land 1
  (0x78FF68), 18 in the land after's only for 0 < land < 5 (0x78FFCB), else the slot's own folder, while a load,
  `LoadSlotFolder` as sub_793230 and fn_00793380, adds land − 1 or land + 1 whatever the land, so on Land 1 slot 14 is
  listed from `14` and loaded from `14\0`; test
  `SaveGameSystem.OnTheFirstLandThePreviousLandsSlotIsListedFromItsOwnFolderAndOnTheFifthTheNextLandsToo`; before
  2026-11-10 the summary took the load's folder), `IsAutoSaveValid` as the game's 0x564160, the safe write of a save's files as
  `SaveAllGame` 0x558160 does it (each to `.tmp`, then remove and rename), and the load counter as `LoadAllGame` keeps it
  (GameOSFile.cpp:394: refused at `GAME_OS_FILE_MAX_LOAD_ATTEMPTS` 2, a counter it cannot read standing for
  `GAME_OS_FILE_UNREAD_LOAD_ATTEMPTS` 1000; +1 written before the load, 0 after a load that ends). The game opens the
  world file read-write before anything (GameOSFile.cpp:387, `LH_FILE_MODE_READ_WRITE_CREATE`) and loads nothing when
  that fails, so a read-only world file is refused; openblack refuses a file whose counter it cannot write (test
  `SaveGameSystem.AWorldFileWhoseCounterCannotBeWrittenIsRefused`). The file system
  service writes a whole file, renames and removes a player's file (`FileSystemInterface::WriteFile`, `Rename`,
  `Remove`). `Save(slot, land, contents)` writes a slot as `SaveAllGame` 0x558160 and `GGame::Save` do, in their
  order: the slot's folders made (`CheckAndRecreateSaveGamePaths`), the world file into `<slot>.sav.tmp` (when it does
  not open nothing more is written), each creature's mind file `SavedCreature<n>` under its own name (from
  `Creature::Save`, inside the game object's save), the script machine's state into `Script_<slot>.sav.tmp` (the
  caller's `scriptState`, the machine's `SaveState`, which fails when its file does not open or is not written whole),
  then, when nothing failed, the old world file removed and the temporary one renamed, then the same for the script
  file, as the exe does (0x558160: `LHOSFile::Delete(filename)`, `LHOSFile::Rename(tmp, filename)`, then `Delete` and
  `Rename` of `Script_<slot>.sav`, neither result looked at), so a rename that fails loses the old file and leaves the
  save in its `.tmp`. A failed write (`WriteEnabled` off) renames nothing and removes nothing: the old world files
  stay byte for byte and the `.tmp` files stay as far as they were written. A write that throws on the way counts as
  a failed write and is not let out (test `SaveGameSystem.AFailedSaveKeepsTheOldFilesAndItsTemporaryFilesAsTheGameDoes`). The world file comes whole from the caller, its counter
  already 0 as the game leaves it at the end. A mind file that fails is reported and does not stop the save, and a
  script file that fails keeps the old files **🔍 inferred** (whether the original's mind and script writes turn
  `WriteEnabled` off is not read). The saves in play call it ([the saves in play](#the-saves-in-play); tests
  `SaveGameSystem.*`).
- **The load, so far** (`Game::LoadSavedGame`, from the debug bar's Load Island / Saved Games menu only, which lists
  the current profile's slots with a world file and lets a loadable one be loaded): the slot's world file through the
  service's `BeginLoad`; the world read (`gamesave::ReadWorld`) and its first values taken (`ecs::saved_game_load::
  StateOf`: the landscape, the land's number, the turn and the real turn, both seeds (GData +8 synchronised, +0xC
  local), the player, the render clock, the influence multipliers, the visual time, its speed and its cycle, named from
  `GGame::Load`); the scripts' restart and the map's clear through the land path; the saved land's file opened; the
  scripts' machine restored from `Script_<slot>.sav`; the land started; then the values put back in `GGame::Load`'s
  order (`Apply`: the seeds, the land's number and multipliers, `ForceVisualTime` with the sky, the speed, the cycle as
  `SetVisualTimeCycle(+0x48, +0x50, +0x4C)`), the turn set and the clock started again at the turn's time, and the
  counter set back by `EndLoad`. The world's objects it makes are in the items below.
- The objects' creation counter (GData's NumCreatedObjects, the fifth word of the game's data, which
  `Object::Object` (Object.cpp:155) takes and raises; the default constructor a load uses does not): `StateOf` reads
  it (`createdObjects`) and `Apply` puts it back with the seeds (`object_index::Restore`), after the loaded objects
  were made with their own saved numbers, so the next object made takes the saved number (tests
  `SavedGameLoad.TheCreationCounterTakesTheSavedNumber`, `...CounterIsPastEveryLoadedTreesNumber`).
- A tree can be made with the growth it is given (`TreeArchetype::Create` with a `TreeArchetype::Growth`), which draws
  no random number, as a loaded tree must not (a loaded tree is made by its default constructor and its saved values,
  never by the constructor that draws its first growth turn). A new tree's growth comes from `TreeArchetype::NewGrowth`,
  which draws as before (test `ScenicForestTest.ANewTreeDrawsItsFirstGrowthTurnOnlyWhenItGrows`).
- **The load's forests and trees** (`ecs::saved_world_load`, from `Game::LoadSavedGame` after the land's file): the
  reader keeps the order the records' reads ended (`WorldRead::loadOrder`, the order the original puts the links back
  in); `PlanOf` takes from it the trees that stand on the map (not held, in the physics or flown by the particles), and
  those a player's hand holds (below), with
  the type their row names (`TreeTypeOfInfo`: the 23 tree rows are numbers 2115 down to 2093 in every frozen save, the
  types proved at Land 1's script places), and the forests from the last of the game's forest list to its first, with
  the two words of the whole game as the last forest read leaves them. `Apply` makes each forest with its saved id (which
  another forest may share, each kept under its own key), its scenic flag and its
  record (`ForestData::saved`), sets the last planting turn and the next forest id (`ForestSystemInterface::
  SetNextId`), then makes the trees in their order, each in the forest its record names through a map from the saved
  forest's number to its key. `MakeTreeEntity` makes a tree through the tree archetype at its saved place, angle, size
  and maximum with its saved growth counter, not growing and not non-scenic (as the original's load leaves it), with its
  saved creation number and life (a `Life` only when not 1), and keeps its whole record on the entity
  (`components::LoadedRecord<gamesave::TreeRecord>`). The debug bar's Saved Games menu shows, under Last Load, of
  every class of the save how many records it held and how many became objects; the log has a `Load census: <class>
  <made> of <saved> made` line for each class made short (2026-11-10). The census is taken once the load's later steps
  have kept their objects too (2026-11-11, `saved_world_load::CountKept`, test
  `SavedWorldLoad.TheCensusCountsWhatTheLoadsLaterStepsKeep`). A way in's object mover made again from its records
  counts its parts as made, as the spells' and the containers' effects do (2026-11-13, test
  `SavedWorldLoad.AWayInsObjectMoverMadeAgainCountsItsPartsAsMade`): before, its collection, its carriers, their vortex
  data and the things they carry were logged as not made (the frozen 14/1: an atom collection, 3 atom cores, 3 vortex
  attract atom data, 3 particle game objects). The later steps' keeps: the players' values and `KeepLoadedSources`' players'
  interfaces and statuses, the camera and the computer players with their actions, attitudes and what their minds have
  seen, which their writers write; before, those classes were all logged as 0 made though the load puts them back
  (e.g. "computer player's sight 0 of 61"). A planned abode or planned town centre counts
  as made when its town takes it back as a plan or its made scaffold as its plan or candidate: openblack keeps them
  as values on those, which are the only records that name them (test
  `Data/TownWriteWorld.TheCensusCountsThePlansATownTakesBackAsMade`). A class with no count of its own counts the
  records its entities stand for: a worship site's totem, its need signs and its spell icons, which the site's making
  makes (test `SavedWorldLoad.TheCensusCountsAWorshipSitesTotemSignsAndIconsAsMade`; before, every one was counted
  unmade). A site's needs' record (0x3E) is kept by its number as the site's totem, which its writer names for it
  (`ShowNeedsRecord` through the totem), so the next save writes it under its loaded number and the census counts it
  (2026-11-11; before, it took a new number and was counted unmade). The music attached to things (0x109, the audio's
  list in the game's tail, `GAudio::Load` 0x428480's `ReadSafe(ThingMusicList)`, each read by `ThingMusicInfo::Load`
  0x429AE0) comes back into the music's list whole, head first, once every object is made
  (`src/ECS/SavedWorld/LoadThingMusic.cpp`, `GameMusic::RestoreThingMusic`): its music type, its thing, whether it is
  enabled, finished and started, and the place it plays at, which the record keeps as `MapCoords` (bw1-decomp
  ThingMusicInfo.h: `FixedPos`; x and z in 16.16 units and the height above the land, put back on the land as the
  music's player does); one whose thing was not made is left out (test
  `SavedWorldLoad.TheMusicAttachedToThingsComesBackInItsListsOrder`; before, a load left the list empty; the frozen
  saves' 12 hold no place). The rest of the audio's load (`GAudio::Load` 0x428480, bw1-decomp Audio.cpp): the alignment
  music playing (+0x1C) is unset and the last one (+0x24) none, the script music's type (+0x28) and whether it plays
  (+0x180) and the town the music last played for (+0x18C) come back from the tail's `audioWords` and `audioObject`
  (`GameMusic::RestoreFromSave`; tests `SavedWorldLoad.TheScriptMusicComesBackWithTheTownItNames`,
  `GameMusicTest.ALoadedGamesScriptMusicAndTownComeBackAndTheAlignmentMusicIsUnset`). The alignment at the camera
  (+0x190, `audioWord`) is loaded by the original but written again by `GPlayer::ProcessPlayers`' fn_0064AC30 at the
  end of every turn; openblack keeps no copy and takes it from the camera at each read (`GameQueries::cameraAlignment`),
  so it has nothing to restore. The big forests come in the same
  pass as the trees, in the same order (both go into the map cells): `BigForestTypeOfInfo` (369 down), and
  `MakeBigForestEntity` makes one through the big forest archetype's overload that takes its forest and its wood
  (`BigForestArchetype::Create(position, type, angle, scale, forest, wood)`; the old one makes its forest and wood and
  calls it), at its saved place, angle and size, with its saved creation number, life and record; each forest then
  points at the big forest its own record names. On the frozen Land 1 autosave: 1,346 trees, 581 of them in its 9
  forests, and its 3 big forests, each with its forest.
- **A thing in a hand** (read 2026-11-10): the original's load makes every object of the file, held ones too:
  `Object::ResolveLoad` 0x63A020 runs its creation (its 3D object, into the map), then takes one whose place is
  unavailable for a change of state (the place's bit 2) out of the map again and clears its in-map mark. openblack:
  `PlanOf` lists the objects every player's interface status names as its hand's (`MagicHandPart::held`,
  `WorldPlan::inHand`; a thing a script holds has the same marks and no status names it) and plans them despite their
  held marks: the map's objects, the villagers and the animals (`PlannedVillager::inHand`, `PlannedAnimal::inHand`);
  `Apply` makes them as any other and takes each off the map at its end (`map_cells::RemoveMapObject`), and the hand's
  load then finds it among the loaded objects (`saved_hand::Load`). Test
  `SavedWorldLoad.ThePlanTakesAThingInAHandButNotOneAScriptHolds`. Still left out: what a script holds, what flies
  or is in the physics, and the classes with no plan yet (a creature's is its own maker's).
- **The plain pots, the moving objects and the one-off spells' orbs** (read 2026-11-10): a plain pot (0x58, a pot
  row whose kind is a pot: the wheat, the branches, the fruit, the logs, a fish, the hand's wood and food) is planned
  as a pile of its row (`PotTypeOfInfo`), its resource, town, amount and flags its pile's, and made by the pile maker
  (a pot's scale follows its amount); a moving object (0x53) and an orb (0x25, `OneOffSpellSeed::Save` 0x72A930 writes
  +0x78 the power-up, +0x68 the seed and +0x6C the multiplier, the constructor fn_0072A3A0 storing them from its
  arguments in that order) by the moving object row their object names: the rows run from 1970 down (`k_FirstMobileObjectRow`;
  in the Land 2 save read, a poo, row 5, is 1965, an orb, row 25, 1945, the Hanoi blocks 1944 to 1940).
  `MakeMobileObjectEntity` makes a moving object through its archetype at its saved place, Y angle and size, leaning at
  its saved X and Z angles (`MobileObject::CallVirtualFunctionsForCreation` 0x607150 places it at all three), an orb
  through the orb's archetype with its seed's graphic; each into the map unless a hand holds it (0x607150 puts in the
  map only an object neither unavailable nor flown by the particles), its creation number, life and record kept. Test
  `SavedWorldLoad.ThePlanTakesPotsMovingObjectsAndOrbsByTheirRows`. Not yet: one in flight or in the physics, the
  classes of their own (Hanoi blocks, arrows and the rest; the footballs, [their own section](#the-footballs)). The poo (0x6B, 2026-11-10) is planned and made as a
  moving object of the poo's row (1965) from its record, which is `MobileObject::Save` alone (the poo has no `Load`,
  `Save` or resolve of its own), and written back by `WritePots.cpp` (`WritePoo`, the same parts) in the game's list of
  moving objects; a poo made in play (the creature's, a script's) is written the same way. Tests
  `SavedWorldLoad.TheFrozenLandTwoAutosavesPooIsPlannedAsMovingObjectsOfThePoosRow` (the 10 of the frozen Land 2 exit
  autosave) and `PotWrite.AMovingObjectAndAnOrbMadeInPlayAreWrittenAndListed`.
- **The moving objects and the orbs made in play** (2026-11-10): they had no writer, so a save left out every moving
  object and orb made since the game began (no record, nothing naming them, no log line), and a load came back with
  fewer of them: a Land 1 game in free play loaded with about 40 fewer moving things. `WritePots.cpp` now writes a moving
  object of the plain class (`MobileObject::Create`: every row but the poo's, the crop's and the creed's, which are
  classes of their own; and not a Hanoi puzzle's piece) as `MobileObjectRecord` (its place, its object of its row,
  1970 less the row, the moving thing's word, its x and z angles) and an orb as `OneOffSpellSeedRecord` (the same parts
  of the orb's row 1945, then its power-up, seed and multiplier), and the game's list of moving objects holds them with
  the pots, the newest first. Test `PotWrite.AMovingObjectAndAnOrbMadeInPlayAreWrittenAndListed`. Of the Land 1 load's
  missing moving things, 4 are bonfires (parked with the particles; since made by the load as a rock with its fire's container, `SavedWorldLoad.cpp`, read 2026-11-14).
- **A spot visual a script holds** (2026-11-10): the scripts' table names the object a script holds; for a spot
  visual the game's table holds its particle container (the object `CREATE` of a spot visual gives the script), which a
  save writes as the container's record. openblack's spot visual is an entity the script holds, with no class of its
  own, so a save while a script held one (Choose Your Creature's) failed; the table now names it by its container
  (`ScriptTableObject`, the containers' source of WriteParticles.cpp). Test
  `ParticleWrite.ASpotVisualAScriptHoldsIsNamedByItsContainer`. A load did not make containers again then; it does since (`saved_particles::MakeContainers`, read 2026-11-14).
- **A computer player a script holds** (2026-11-11): `CALL_COMPUTER_PLAYER` (`GScript::GetComputerPlayer` 0x6EE840)
  hands the script the player's `GComputerPlayer` (`AddScriptGameThing(player->ComputerPlayer, 0)`), so the scripts'
  table names the computer player's record (0x66). openblack keeps the computer player on the player's entity, which
  the table held, of no single class (the player's and the computer player's), so a save after a script had called
  for a computer player (Land 1's GiveScaffold and LearnWorshipping) was refused; the table now names it by the
  computer player's object of the player's number (`ScriptTableObject`). Test
  `ParticleWrite.AComputerPlayerAScriptHoldsIsNamedByItsComputerPlayersObject`.
- **What the physics moved** (2026-11-10): `Object::Save` writes, for an object in the physics (the place's bit 6)
  not flown by the particles, its `PhysicsObject`'s `PhysOb` Matrix (the body's unit rows and its centre of mass in
  the world), Velocity and AngularVelocity (the angular momentum the torque adds to, PhysOb.cpp:741); `Object::Load`
  buffers them (`PhysicsSaveInfo`) and `Object::ResolveLoad` 0x63A020 clears the flag, puts the object back into the
  physics standing still (`InitialisePhysics(0, 0, NULL, true, NULL)`) and sets the body's Matrix, Velocity and
  AngularVelocity from the buffer. openblack: the writer writes an object in the physics with its body
  (`PhysicsPartOf`: `Body::Axes`, `Centre`, `velocity`, `angularMomentum`; before, a body standing still at the
  world's origin); `PlanOf` takes each map object, moving object, pot, pile and dead tree the physics moved with its body
  (`WorldPlan::inPhysics`), and `Apply`, once everything is made, adds each to the physics standing still
  (`DynamicsSystemInterface::AddObject`) and puts its body back (`Body::RestorePose`, the velocity, the angular momentum,
  `SyncTurnStart`). Tests `SavedWorldLoad.ThePlanTakesAThingInThePhysicsWithItsBody`,
  `BodyPose.ARestoredPoseIsTheSavedRowsAndCentreWithThePointsPlacedByThem`. Not yet: a living thing in the physics
  (its maker still leaves it out) and the class's own start of the physics the original's `AddObject` runs.
- **The features** (0x74, 2026-11-10): the load had no plan for them, so a script's feature (CREATE of a feature,
  e.g. The Missionaries' at Land 1) was not made and the scripts' table named nothing for it: after a load
  `ChallengeHighlightNotify`'s `Location clicked` asked GAME_THING_CLICKED of nothing ("Object no longer valid", once
  a turn). `PlanOf` takes each feature that stands on the map or is in a hand by its row (2064 down,
  `FeatureTypeOfInfo`), and `MakeFeatureEntity` makes it through the feature's archetype at its saved place, angle and
  size, built as far as saved, with its marks, creation number, life and record. Test
  `ForestWriteWorld.ASavedFeatureIsMadeAgainFromItsRecord`.
- **The dead and felled trees and the singing stones' bases** (2026-11-10): the load had no plan for them, so a load
  lost them with no log line. `PlanOf` takes each dead (0x70) and felled tree that stands on the map or is in a hand,
  of its tree row and player (its record's `info` and object), and each object of no other class (0xF6) of the
  singing stone base's mobile static row; `MakeDeadTreeEntity` makes the tree through the dead tree's archetype on
  the land under its place raised by its altitude, at its life and X, Y and Z angles, with its wood multiplier, a
  felled one felled; `MakeStoneBaseEntity` the base as a mobile static of its row at its place and height with its
  hidden-name and out-of-play marks; each with its creation number and record. Tests
  `ForestWriteWorld.ASavedDeadAndFelledTreeAreMadeAgainFromTheirRecords`,
  `ForestWriteWorld.ASingingStonesBaseIsWrittenAsAnObjectOfNoOtherClass`. A bonfire on the map (2026-11-12; 0x72,
  `Bonfire::Load` 0x439A00: `Rock::Load`, then its word +0x94) is planned as a mobile static of the bonfire's row with
  its rock's player and its record whole, and made by `MakeMobileStaticEntity` with no spot visual started: its fire is
  the particle container of the game's list that hangs on it, made again with the containers once the bonfire is (test
  `SavedWorldWrite.ABonfireOnTheMapIsPlannedAsAMobileStaticWithItsPlayer`; before, a bonfire stayed as read, and so did
  its container with its effect's atoms).
- **The spells' seeds** (0x24, read 2026-11-10): `PlanOf` takes each seed of a seed row (2578 down) with its creator's
  player when its creator object is a player and the player of the interface status it names; `MakeSpellSeedEntity`,
  once every object is made, makes it through the seed's archetype at its saved place and puts back what its writer
  writes (`WriteSpellSeeds.cpp`, the inverse): its power-up, flags, icon, creator (a player, the worship icon made
  again, or none), charge and its copy, stored chants, age and object count, power, multiplier, ready, turns in the
  hand, last magic, the one-shot mark and the place it was made at; like every seed the game makes it is an
  interface's (`inInterface`). The spell cast from it is not made again (none). Test
  `SavedWorldLoad.ThePlanTakesASpellsSeedWithItsCreatorAndItsInterfacesPlayer`.
- **The towns** (`ecs::saved_world_load`): `PlanOf` takes them from the last of the game's town list (global list 31,
  newest first) to its first, and the players' town lists in order (`ownersTowns`); `Apply` makes them before the
  objects (`MakeTownEntity`: `TownArchetype::CreateWithoutDesireFlags`, as the desire flags have records of their own
  and the original's load makes none; the saved id as the town's key, or the next free key with the id kept as its
  script id when another town has it; its player, tribe, place, belief in the neutral player, area and whether it is
  uninhabitable; its two temporary pots (`TemporaryResourceStorePots`, `ReadPtrArray`), set once the pots are made
  (test `SavedWorldLoad.ALoadedTownTakesBackItsTemporaryPots`; before, both stayed empty, so an emptied one was never
  deleted by the town's process); its belief block after the block's two words, as `Town::Load` 0x73F450 reads `belief` whole, and its
  balance belief scale; its figures block after its two words, as `Town::Load` reads `stats` whole: the counts
  `TownStats` holds, the deaths (`TownDeaths`), the food and wood used and the injured (test
  `Data/TownWriteWorld.ALoadedTownTakesBackItsFiguresAndWritesThemAsItHoldsThem`; the town's process counts them again
  each turn, `town_stats::Compute`); the magic types it holds (`MagicTypesHeld`, `TownMagic::held`; test
  `Data/TownWriteWorld.ALoadedTownTakesBackTheMagicItHolds`; which of them its player may cast is the player's own
  record's); EmptyTownTimer, the countdown to its being empty (`Town::emptyCountdown`; test
  `Data/TownWriteWorld.ALoadedTownTakesBackItsEmptyCountdown`) (test `Data/TownWriteWorld.ALoadedTownTakesBackItsBeliefAndWritesItAsItStands`: a town loaded
  with no belief in any player went to the neutral player at its first fold, `TakeOverTown` then leaving its worship
  site, whose icons went with it, on the user's Land 2 autosave 19 of 20); its record kept), gives each town its forest list in its saved order (the forest list's
  `TownLists`), and gives the towns their owner's list stamps in the players' saved order. Once the storage pits
  are set, each town gets its plans back in its list's order (`Town::plannedAbodes`; test
  `Data/TownWriteWorld.ALoadedTownGetsItsPlansBack`): a planned abode's or a planned town centre's row, place, angle,
  size, whether it was a building, its creation turn and the footpath link it names; a planned citadel heart
  (`PlannedTownCitadelHeart::Load` 0x467FF0: `PlannedMultiMapFixed::Load` 0x648AF0's parts, its row read by `ReadInfo`,
  then its town) as a heart plan of the citadel heart's row (2411, `heartInfo` 0; one of a row the game's tables do not
  hold stays out), counted made (2026-11-11, test `Data/TownWriteWorld.ALoadedTownGetsItsPlannedCitadelHeartBack`; the
  frozen `_f_f` Land 1 save's, `...TheFrozenLandOnePlannedCitadelHeartIsOfTheHeartsRow`; before, it stayed out and the
  census counted it unmade in the three saves that hold one). Its other values
  (worship, influence, desires, belief in each player, its buildings, villagers and lists) stay in its record until
  their groups come. On the frozen Land 1 autosave: Land 1's six towns with their script's players and tribes.
- **The storage pits and the piles** (`ecs::saved_world_load`), in the same pass as the trees: the piles of wood and
  of food, and the magic wood (a wood pile's record and its player, `MagicWood::Save`; the player its record names
  becomes the pile's owner, test `PileLoad.ALoadedMagicWoodTakesBackItsPlayer`) (`PotTypeOfInfo`: the pot rows run from 1876 down, the storage pit's food pile 1874 and its wood piles 1873
  to 1869 in every frozen save) are made through the pot archetype at their saved place and angle with their saved
  amount, given their saved size and placed again with it (the object's load reads the scale; a new magic wood is made at
  0.7 and magic food at 0.3, and `Pot::CallVirtualFunctionsForCreation` 0x66D130's `SetSize` changes only a pot's size,
  by its amount; test `PileLoad.ALoadedPileTakesBackItsSavedSize`), set at that amount's height, then given back what
  their record keeps (`MakePileEntity`): the poison, the
  speed-up and the reaction-started bits of the flags (`gamesave::pot_flags`), and what only a save reads: the town the record names (in `Apply`, once the towns are
  made), the building-site bit and the +0x80 byte (whether it was made to start its reaction). The sink is not kept: `PileResource::Load` reads the 48 bytes of the AltitudeZoomer, but the resolve every loaded object gets (`Object::ResolveLoad`, Object.cpp:1910) runs the creation hook again, and `PileResource::CallVirtualFunctionsForCreation` 0x66E300 (Pot.cpp:767) puts the zoomer at minus the pile's height and calls `SetSize`, so every loaded pile starts buried and rises to the height of its amount over 1 s, saved settled or still rising; openblack buries it again and calls `PotArchetype::SetSize` with the rise (test `PileLoad.ALoadedPileRisesAgainFromBuried`), and the next save writes that fresh zoomer. The resolve also sets the pile's player (+0x7C) to what `GetPlayer` answers then (`PotStructure::GetPlayer`, Pot.cpp:1145): one part of a structure its structure's player at the load, any other the player it was saved with, the neutral player for none, a magic pile its own player (`MagicFood`/`MagicWood::GetPlayer`); a loose pile answers with it from then on. openblack's load does the same (`PlannedPile::pilePlayer`, `MakePileEntity`, then `Apply` once the structures are made: `Pot::playerWhenMade`, and `Pot::owner` for a pile part of no structure, which `object_resources::PlayerOfPile` answers), and the pots' writer writes `playerWhenMade` for a loaded pile too (test `PileLoad.ALoadedPileAnswersWithThePlayerTheLoadsResolveGivesIt`). In the frozen saves (census of 2026-11-12), Creature Isle's loose food piles are saved naming no player, so the next save names the neutral player for them, as the original's does; the only other loose food or wood pile, a wood pile of slot 15/2, is saved naming a player. The same resolve (`PotStructure::CallVirtualFunctionsForCreation`, Pot.cpp:562) starts the reaction again of a pile in play (its thing not unavailable) saved with its +0x80 byte set and its reaction-started bit clear (`Pot::SetupReaction`, Pot.cpp:403, acts only on a pile whose bit is clear), after every reaction the save kept, which the load read before any resolve (`GameOSFile::ResolveAllLoads`). openblack's load does the same: `PlanOf` lists those piles in the order their reads ended (`WorldPlan::pileReactions`) and `Apply`, once the reactions are made, starts each one's reaction (`Makers::pileReaction`, the game's `animal_ai::SetupPotReaction`; test `PileLoad.ALoadedPileMadeToReactWithNoReactionStartsItAgain`). None of the frozen saves holds such a pile (census of 2026-11-12 through `savetool --fields`, the 27 world files: of 744 food, wood and magic piles 738 have the byte clear, and the 6 with it set, Land 2's magic food in 15/2, 16/2 and 20, have their reaction started); openblack's own saves can, as a pile's reaction is taken away when it is picked up or emptied. A food pile saved sped
  up whose record names its sparkle (+0xB8, which `PileFood::Load` 0x66F5E0 reads back as the saved container) has
  it again: the load starts the PILEFOOD_SPEEDUP spot visual for ever on the pile (`pot_resource::SetSpeedUp`) and
  names it by the kept container's number (`WorldSource::loaded`, `SpotVisualEffect`), so the next save writes that
  container once, from the live effect (the containers' list skips a kept number now live); a storage pit (`AbodeTypeOfInfo`: the abode rows
  run from 2394 down, Norse 2275 and Aztec 2355) comes after its piles, which were read inside it, and is made
  through the abode archetype's overload that takes its piles (`AbodeArchetype::Create(..., piles)`: it makes none
  and fills none) with its saved food and wood, of the town its record names (`MakeStoragePitEntity`); each town then
  takes the storage pit its own record names (`town_stores::SetStoragePit`). On the frozen Land 1 autosave: its two
  storage pits, each of its town with its food pile and its five wood piles. They are written back from the entities
  ([Writing the world file](#writing-the-world-file)).
- **The magic food** ([the magic food](#the-magic-food)), after the other objects.
- **The footpaths and their links** (`ecs::saved_world_load`), before the towns: the footpaths from the last of the
  game's footpath list (global list 28, newest first) to its first, each made as a new footpath at the list's head
  with its nodes in its saved order (their map places, raised by their heights above the land, and their flags) and
  whether it is in use (`MakeFootpathEntity`); then each footpath link with the footpaths it lists, in its saved order
  (`MakeFootpathLinkEntity`); each storage pit and big forest is given the link its record names as it is made
  (`footpaths::SetFootpathLink`). On the frozen Land 1 autosave: 84 footpaths with their 792 nodes and 59 links.
  They are written back from the entities ([Writing the world file](#writing-the-world-file)).
- **The abodes, creches and graveyards** (`ecs::saved_world_load`), in the same pass as the trees: the original's
  resolve of an abode (`Abode::CallVirtualFunctionsForCreation`, Abode.cpp:326: its chimney smoke and its sinking into
  the land; the graveyard's adds only a flag of its 3D object, 0x595DD0) does what the abode's creation does but not
  its MakeFunctional. openblack makes each through the abode archetype (`MakeAbodeEntity`) at its saved place, angle
  and size, of the town its record names, whole or being built as its saved build flags say, with its saved food and
  wood, then its build flags, percent built, creation number and life from the save; a graveyard takes its saved dead
  and shows no graves stage (the original keeps the stage on its 3D object, which the load makes afresh). A field
  (`FieldRecord`) is made the same way as an abode of its field row, then takes its kind (1765 less its row), its
  town, its food, its growth, the crops sown and its turn offset from its record, and goes into the map's cells again
  as a field, as a field's making does; it draws no random number, and its farmers stay as the record names them
  (test `Data/AbodeWriteWorld.EachAbodeTheLoadMadeIsWrittenAsItsRecordWasRead`). Each is given the footpath link its
  record names. On the frozen Land 1 autosave: its 30 abodes, 2 creches and 2 graveyards,
  each of its town. They are written back from the entities ([Writing the world file](#writing-the-world-file)).
- **The fish farms** (`ecs::saved_world_load::MakeFishFarmEntity`, test
  `Data/AbodeWriteWorld.ASavedFishFarmIsMadeAndWrittenBackAsRead`), once the towns are made: each as a script makes
  it (`FishFarmArchetype::Create`: its info 1759 less its row, its shoal), then of the town its record names in place
  of the nearest one the making picks, with its stock (+0x94), its creation number and its life; its fishermen stay as
  the record names them, and it is written back as it was read.
- **The scripts' highlights** (`ecs::saved_world_load::MakeHighlightEntity`, test
  `Data/ForestWriteWorld.ASavedHighlightIsMadeAndWrittenBackAsRead`): the game's list of them (24) from its last to its
  first, so that the newest is made last, each as a script makes it (`script_highlight::Create`, at the head of the
  list and of its cell): of its row (2088 less the record's), at its place and height, with its angle, size and script
  id; then whether it is hidden or active, the height a script set and its did-you-know category from its words, the
  hidden name its thing's 0x40 says, and its creation number. It is written back as it was read.
- **The scripts' markers** (0x7C, `ecs::saved_world_load::MakeScriptMarkerEntity`, test
  `Data/ForestWriteWorld.ASavedScriptMarkerIsMadeAndWrittenBackAsRead`), 2026-11-10: the markers of the game's list of
  things with a place (list 25), from its last to its first, so that the newest is made last, each as a script makes
  it (`ScriptMarkerArchetype::Create`) at its place and height over the land, with the hidden name its thing's 0x40
  says. Its place in the scripts' object table puts it back in its script (a script made it). It is written back as it
  was read.
- **The influence rings** (0x49, `ecs::saved_world_load::MakeInfluenceRingEntity`, test
  `InfluenceRingWrite.ALoadedRingIsMadeAndListedOnceInItsLoadedPlace`), 2026-11-10: the game's list of rings from its
  last to its first, once every object is made, each as a script makes it (`influence::CreateRing`, at the head of the
  list) at its place, of the player its record names, with its radius and anti word, following the object its record
  names when that object was made. A ring of no player is not made. The list's writer names the loaded rings once, in
  their loaded places, behind the rings made since.
- **The animated statics** (0x75, `ecs::saved_world_load::MakeAnimatedStaticEntity`, test
  `Data/ForestWriteWorld.ASavedAnimatedStaticIsMadeAndWrittenBackAsRead`), 2026-11-10: in the objects' pass, each one
  that stands on the map, through the animated static archetype of its row (1986 less the record's) at its place,
  angle and size, with the open state its record's last word keeps (`AnimatedStatic::Load` 0x422BC0 reads the gate
  stone slots, then that word), its creation number, life, place's marks and footpath link. Its gate stones
  (2026-11-11, test `Data/ForestWriteWorld.ASavedPlinthsGateStonesAreLaidAgainAndWrittenBackAsRead`, and
  `Data/ForestWriteWorld.TheFrozenLandOneSavesPlinthsComeBackWithTheirGateStones` on the frozen `_s_i_s_i` saves 15/1,
  16/1 and 20, whose plinth keeps 366, 362, 364, open): each slot keeps the `MESH_LIST` number of the stone's model laid
  in it (+0x8C..+0x94, `GateStoneIndices`), 0 when empty, read as they stand with no link to any object (the laid stone
  object itself is deleted, `MobileStatic::ApplyThisToObject` 0x608C30). `CallVirtualFunctionsForCreation` 0x422300
  then calls `AddGateStone` 0x422D50 for each slot that is not 0, in slot order, which makes a 3D object in the first
  slot with none (+0x80..+0x88) and puts the model's number in that slot. openblack lays each again with
  `animated_static::AddGateStone`, so the slots and their worth (`CountGateStones` 0x422DF0: ape 362 1, tiger 366 2,
  cow 364 4) are the saved ones. Slots are only ever filled in order in play, so a saved slot of 0 before one that is
  not 0 never occurs; the original would leave such a record with the later stone's number in both slots (counted
  twice) and one 3D object, which openblack's one model per slot does not keep. **🔍** where its clip stands after the
  load is not read: openblack's starts at the clip's start and plays towards the state it has.
- **The towns' desire flags** (`ecs::saved_world_load`), in the same pass as the trees (they are objects in the map
  cells): each flag of a town made, through the desire flag archetype with its town, its desire (the record's kind),
  the store point of its slot, its mesh and its object scale, at its saved place; the place it was made at is the
  object's own coordinates (which the original sets once, as it is made), its height over the ground its place's
  altitude and its desire the saved one, with its creation number and record and its saved angle kept as its own
  (`placed_turn::KeepAngles`, as the archetypes keep theirs; its turn alone gives -0.0 back for a saved 0.0)
  (`MakeDesireFlagEntity`); its town keeps
  it by its desire (`TownDesireFlags`). The towns are made without flags (above), so a town has only its saved ones.
- **The snow lying** (`GGame::Save` 0x5546A4..0x55471C, the snow cover `g_snowcover` [0xFA2918], made for the save
  when there is none): the grid of 0x4000 depths (+0), the 0x400 blocks' changes (+0x10000), the melting clock (+0x11000)
  and band (+0x11004). `StateOf` reads them from the game object's tail and `Apply` puts them back
  (`SnowSystemInterface::Restore`; a grid of another size is left out).
- **The "did you know" texts read** (`CreditsRoom::DYKLoad` 0x78D150, from `GGame::Load` 0x55503A): `StateOf` reads
  the five lists and their counts from the game object's tail, and `Apply` puts them back after the snow
  (`did_you_know_read::Load`), as [the library's scrolls](../interface/temple-interior.md#the-librarys-scrolls) says.
- **The climates** (`GClimate::Save` 0x773320 read field by field): its place (+0x14), the radii (+0x20, +0x24), its
  id (+0x28), the next climate id [0xC2475C], its type (`WriteInfo`: the climate rows run from 327, the world's, down),
  the last day [0xDCB8B8], the season the climates use [0xDCB8C0] and the last season [0xDCB8BC], the rain (+0x34,
  16 bytes: desire, two day counts, flags), the temperature and its target (+0x44), the wind (+0x4C: x, z, angle), the
  storms' most (+0x58), the world flag (+0x64), its natural storms (the list at +0x5C), the storms' height (+0x68), the
  fall speed (+0x6C), the unread word (+0x70), the lightning ranges (+0x74..+0x80) and the lightning flag (+0x84).
  `MakeClimates` puts the climates back newest first, as the game's list (global list 38) holds them, with the counters
  as the last climate saved them (`weather::climate::Restore`). Each natural storm is written (`GClimate::Save`
  0x7735EC..0x773661) as a point, its descriptor's 80 bytes, its age (+0x58), speed (+0x68) and target (+0x5C), and read
  back (`GClimate::Load` 0x7739DF..0x773A48) by making a storm from the descriptor (`fn_0083F6F0`, at the head of the
  storms' list, speed 1) and reading its age, speed and target into it, then putting it at the head of the climate's
  list (`fn_00773B20`), so the climate's list comes back the other way round. openblack does the same
  (`StormDescriptorOf`, tests `SavedWorldLoad.AStormsBytesAreItsDescriptorInOrder`,
  `AClimatesNaturalStormsComeBackWithTheirAgeSpeedAndTarget`). On the frozen Land 1 autosave the four climates are
  Land1.txt's, each with the storms its record holds.
  A climate is written back by its writer (`src/ECS/SavedWorld/WriteWeather.cpp`, a source keyed by its id, the game's
  list newest first, `GClimate::Save` 0x773320): every value from the climate, the system's counters as they stand,
  and its natural storms in its list's order, each its place (the storm's +0x8), its descriptor, its age, speed and
  target; so a game loaded and saved again writes a climate's storms the other way round, as the original's does
  (tests `SavedWorldWrite.AClimateIsWrittenWithItsStormsInItsListsOrder`, `SavedWorldLoad.AStormsDescriptorWritesBackItsBytes`,
  and the frozen Land 1 autosave's climates in `TheFrozenLandOneAutosavesClimatesAreItsScripts`).
- **The things of the weather** (class 0x43, the game's list of weather things): a thing is written (`WeatherThing::Save`
  0x7745C0) as its place, its own storm descriptor's 80 bytes, a word saying whether it has a storm (1 or 0), its storm
  when it has one (written as a climate's natural storm is: its place, its descriptor, its age, speed and target), and
  whether the wind moves it; a thing whose storm is over writes the word 0 and nothing more. The load
  (`WeatherThing::Load` 0x7747E0) reads the storm only after the word 1. openblack reads the record the same way
  (`gamesave::WeatherStormPart`), makes each thing of the list at its saved place with its own descriptor
  (`MakeWeatherThingEntity`), makes its storm from the storm's saved descriptor at the head of the storms' list and
  puts back its age, speed and target, and keeps the things in the list's order (`weather::weather_thing::AddLoaded`).
  The writer (`src/ECS/SavedWorld/WriteWeather.cpp`) writes each thing back from its entity, with its storm while the
  storm is in the storms' list, and the list in the things' order (test
  `SavedWorldWrite.AWeatherThingComesBackWithItsStormAndIsWrittenAsItsRecord`; gamesave `SaveWorldListRecords`, the
  record's two lengths).
- **The rocks and the other mobile statics** (0x6F `Rock::Save` 0x6E77B0, 0x54 `MobileStatic::Save` 0x608650): an
  object of several cells' parts (its kind's row: the mobile static rows run from 1937, the first, down to the meteor's
  1877, just above the pots' 1876, as `load_variables` 0x42B460 loads the mobile static table right before the pots'
  and `GBaseInfo::SetInfoID` 0x436BD0 numbers every row from the last loaded), then the owner object (+0x7C, a
  reference; `MobileStatic::GetPlayer` 0x6088B0 is that object's player, or the neutral player), the X angle (+0x80)
  and the Z angle (+0x84), floats (`GetXAngle` 0x439610, `GetZAngle` 0x439620; the Y angle is the object's heading);
  a rock adds +0x88 (a word) and +0x8C (a float), made 0 and -1 by its constructor 0x6E6F70 and read by nothing else,
  and its player (+0x90, a reference: `Rock::GetPlayer` 0x6E77A0). The load makes each with its default constructor
  and `Object::ResolveLoad` 0x63A020, which places it through `MobileStatic::CallVirtualFunctionsForCreation` 0x609700
  at its coordinates' land point raised by its altitude, at its X, Y and Z angles and size, and puts it in the map.
  openblack makes each one that stands on the map (`MakeMobileStaticEntity`: the mobile static archetype, a rock of its
  saved player, a script's built percentage when it is not 0, its creation number, life and record kept), and writes
  both back (`src/ECS/SavedWorld/WriteMobileStatics.cpp`: every mobile static but a bonfire and a singing stone's base,
  a rock when its row's mobile type is a rock's, as `fn_00608770` makes one; one made in play is written made whole,
  flag 0x08, with no owner object, and a rock with 0 and -1 and its player). On the frozen Land 1 autosave every one of
  the 132 rocks and 109 mobile statics is a `CREATE_MOBILE_STATIC` of Land1.txt at its place, with the script's
  altitude, angles and size (tests `SavedWorldLoad.TheMobileStaticRowsRunDownFromTheFirstToTheMeteorsAboveThePots`,
  `ForestWriteWorld.ARockMadeInPlayIsWrittenFromItsState`,
  `ForestWriteWorld.TheFrozenLandOneAutosaveWithItsForestsAndRocksMadeWritesBackByteForByte`).
- **The villagers** (0x6 `Villager::Save` 0x754280, `Load` 0x754580; 0x76 `SpecialVillager::Save` 0x71F560 adds the
  row of its name in the special villagers' table, +0x130): after the living thing's parts, its flags word (0x1 awake,
  0x2 at a worship site, 0x4 at home, 0x8 a child, 0x10 on its way to worship, 0x20 in the hand, 0x40 counted out,
  0x80 football, 0x100 kept, 0x200 a disciple, 0x400 a disciple's follower, 0x800 an into or out-of clip plays, 0x1000
  an out-of clip plays first, 0x2000 has been to bed, 0xC000 its tree type), its food, the turn of its last check, its
  food speed-up, the object it carries (0 until `SetStateCarriedObject` 0x7501A0 first picks one, as `SetToZero`
  0x74FB20 leaves it), its discipline, the food and wood it holds, its pregnancy, its home, town and mother (openblack:
  each none once gone, 2026-11-11, test `Data/ForestWriteWorld.AVillagerNamesNoHomeTownOrMotherThatIsGone`, as
  `Abode::DeleteDependancys` → `RemoveAllVillagersFromAbode` puts the residents out and
  `Villager::FindChildrenAndOrphanThem` orphans a deleted mother's children), two
  zero words, then the values its top and final states save. Its row runs from 2247 (the first villager row) down, as
  every one of the frozen Land 1 autosave's 55 shows against Land1.txt; a special villager's row is its villager
  row. The constructor 0x74F950 draws its state counter, GameRand(500)+1, into the walk's turns to its next state.
  openblack writes both classes (`src/ECS/SavedWorld/WriteVillagers.cpp`: a special villager is one with a
  `NamedVillager`): the walk and living values through the shared parts (`WriteLivingParts.h`), its move state from
  its move state tag (`ecs::wall_hug::WalkStateOf`), else as kept, else 0 while one made in play has not moved yet;
  its own values from its components; its states' values none for a state that saves none, else as kept while it is
  in the states its load kept. The write fails, naming the villager, for one made in play that has moved with no move
  state tag (the game's state after arriving, 1 or 4, or none, is not held) or one in a state whose values openblack
  does not hold (test `ForestWriteWorld.AVillagerMadeInPlayIsWrittenFromItsState`). The game's load makes a villager
  with its default constructor (no random draw), reads its values raw (no state is entered, so no town modifier
  moves), and as its read ends draws its state counter again, GameRand(20)+1, when neither of its states loads values
  of its own (`Villager::LoadStateFunction` 0x7549B0: the state table's load entry is there exactly where its save
  entry is); its resolve (0x754870, through `Villager::CallVirtualFunctionsForCreation` 0x74FC70) gives it its 3D
  object, a child's meshes or a grown up's, puts it in the map and plays its first clip. Its home's, town's and the
  game's lists are read by their owners (`Abode::Load` 0x406D20: its villagers and counts; `Town::Load` 0x73F450: its
  homeless). openblack makes each villager in the order the reads ended (`MakeVillagerEntity` through
  `VillagerArchetype::Restore`, with no draw and its saved creation number), drawing the counter as the game does, made
  or not; then gives each its home, town and mother, each abode its villagers and counts, and each town its homeless,
  from their records. The reactions it did, with their turns, oldest first, are put back on it (`ReactionRecords`, at
  most three, as the animal's load does), and written back as loaded. Its walk is made as the record holds it
  (`MobileWallHug::Save` 0x60C420's order): the tag of its walk state (stepping through to its goal, with the turns to
  its next aim; the straight walks and the circling states with their way round; its last step), or, arrived or
  wandering, the state after its walk; the circle it heads for, with its turns and entry distance: of water or the land's edge, or, once every object is
  made, the thing's circle at the saved place among its circles (`thing_circles::CirclesOf`, its only one for -1); its speed, angle, goal and step; and, on a footpath, the footpath, its node there, its way along it (the
  status bit 0x8) and the footpath it takes next (`LivingFootpath`). The load keeps each loaded footpath's nodes by their numbers (their ids from 0, in the footpath's list order), so a walker written after a load names the node its footpath names. A villager in the footpath state (28, 29: `SaveFootPath` 0x755B20, the living part's footpath and node again) is made with them. A villager walking a script's track is made with its walk as its data path keeps it (2026-11-11: TrackNo, EndPercentage,
  the forward flag, Frame, FramesPerTurn and its speed into `LivingWalkPath`), one whose track `camera.edt` does not
  hold being left out. The walk's data path is kept by the id the data paths' writer names it by (2026-11-13,
  `DataPathIdOf`; test `SavedWorldLoad.AVillagersWalkAlongATrackIsKeptByItsDataPath`), so the census counts it made
  and a write keeps its thing part as read; before, the census logged it 0 of 1 (the frozen saves 14 to 16 of the
  census) and the writer wrote its thing part afresh. A villager walking to a thing (MOVE_TO_OBJECT, 2) is made walking to the thing its walk's
  target names, once every object is made (2026-11-11: the wall hug's target, `Villager::walkTarget`, which MOVE_TO_POS's
  exit clears; the save writes it while it holds one). One held, in flight, in a flock,
  reacting with no reaction, or with turns to a circle of nothing is not made yet (one in a
  reaction is made, and once the reactions are made again it follows its own, by the id it was made with, reacting to
  its other object, the reaction's followers joining in its list's order from its last; `LivingReaction`; one written
  after it stopped following it names no reaction and no other object, 2026-11-11, as `Living::StopReacting` 0x5F1140
  clears both, test `SavedWorldWriteLivingParts.ALoadedVillagerThatStoppedReactingNamesNoReaction`), with
  one log line naming which; the dance states' two words (`SaveDance`
  0x754AC0: +0x118 and +0x11C, raw, which no dance state reads or writes) are kept with the record and written back as
  loaded, so a villager in one of the seven dance states is made. A villager that enters a dance state in play: the
  original writes there whatever the shared slot last held (another state's partner, target or counter, possibly a
  pointer), which nothing reads back, as no dance state reads the two words and a load only copies them; openblack
  writes 0 and 0, the only values the exe gives the slot itself (the constructors 0x55C8A0 and 0x74F950, and
  `AddVillagerToWorshipSite`'s 0 at +0x118). A dead villager (states 13 to 15, `SaveDead`) saves its cause as one byte,
  which openblack holds (`Villager::deathReason`): loaded into it, and written from it. A villager fishing, felling or
  looking at a thing (`SaveFishing`, `SaveForesting`, `SaveInspectObject`: `TargetThing`) saves that thing; the load
  gives it back once every object is made (`Villager::targetThing`), and the writer writes the one it holds (tests
  `SavedWorldLoad.TheVillagerRowsRunDownFromTheFirstAndALoadedCounterIsDrawnWhenNoStateLoadsValues`,
  `ForestWriteWorld.TheFrozenLandOneAutosavesVillagersAreMadeAndWrittenBack`: its 55 villagers, each written back as
  its record but for the counter).
  **A villager's states' values made in play** (`HeldVillagerStateValues`, `src/ECS/SavedWorld/WriteVillagers.h`),
  from the offsets each state's save reads in the exe (`xref.py savelayout Villager::Save<State>`; the two shared
  slots are +0x10C, 12 bytes, and +0x118, 16 bytes): building (`SaveBuilding` 0x754A00: the site +0xFC, then
  BuildPosIndex +0x118: `Villager::buildingSite`, `buildPosIndex`); farming (`SaveFarming` 0x754E80: FarmPos +0x11C,
  12 bytes, then the field +0x118: `workPos`, `targetThing`, as `SetFarmerGotoField` 0x759C40 sets both); breeder
  (`SaveBreeder` 0x755A20: the partner +0x118, then WanderArea +0x11C, x and z: `targetThing`, which
  `SetupBreederDisciple` 0x769EE0 clears, and `wanderArea`); trader and a trader's nothing to do (`SaveTrader`
  0x7557A0, `SaveDiscipleNothingToDo` 0x755760 for disciple 9: TradeResource +0x118, then TradeTown +0x11C:
  `tradeResource`, which `CheckTrader` 0x769B80 writes over the cleared target, and `tradeTown`); in the hand
  (`SaveInHand` 0x754D80: +0x10C as a word, the disciple `EnterInHand` 0x76AFE0 writes there: `discipleOnPickUp`); on a
  footpath (`SaveFootPath` 0x755B20: +0xC8 and +0xCC, the living part's footpath and node again); football
  (`SaveFootball` 0x754F60: the bytes +0x118, +0x119, +0x11A, then the pitch +0x11C: the choose-again byte the
  substate assignment clears and each process sets, the substate's turns, the player's place among its position's
  players, [the buildings](../world/buildings.md); `footballChooseAgain`, `footballSubstateTurns`, `footballNumber`,
  `footballPitch`; the substate itself is the action's fourth byte, not saved here); shepherd (`SaveShepherd`
  0x755120: +0x118 and +0x11A, 16 bits each: `ShepherdWork::waitTurns` and `pending`, 0 and 0 before its first wait);
  the script states (`SaveInScript` 0x755260, `SaveScriptPos` 0x755470: +0x10C x and z, the place
  `SET_SCRIPT_STATE_POS` 0x6F84C0 gives as `MapCoords(point)`, +0x118 the number `SET_SCRIPT_FLOAT` 0x6F8640 gives,
  +0x11C and +0x120 the pair `SET_SCRIPT_ULONG` 0x6F8770 gives, which `SetupScriptWanderToPos` 0x768680 also writes:
  `ScriptStateArguments` with `map_coords::ToFixed`, and `ScriptAnimation`; 0 for what no script gave yet); the
  reactions: by a fire (`SaveFire` 0x755680: FirePos +0x10C, x and z, the place `SetupMoveAroundFire` 0x75A770 and
  `SetupOnFire` 0x75B170 keep and `FinishBeingOnFire` 0x75B3D0 goes back to, then the fire +0x114:
  `VillagerFireState::savedDestination` with `map_coords::ToFixed`, and its fire by its source key), hiding
  (`SaveHideInBuilding` 0x755E00: HideTurns +0x10C, then HidePos +0x110, which `SetupGoAndHideInNearbyBuilding`
  0x763CB0 sets to the building's place: `VillagerHiding::turns` and its building's place as that building's record
  writes it), a shield (`SaveShieldReaction` 0x755F40: ReactionPos +0x10C, the point `SetupReactToMagicShield`
  0x765C60 and the amazed state look at: `VillagerShieldState::lookAt`), an inspection (`SaveInspectionReaction`
  0x755B80: +0x10C the distance, 20.0 as `InspectCreatureReaction` 0x7678A0 sets it, +0x110 the turns before it walks
  closer: `VillagerInspection`) and a magic tree (`SaveMagicTreeReaction` 0x755CC0: +0x10C the next tree, +0x110 the
  trees seen: `MagicTreeWatch::next`, `looked`), each 0 before its state first set it. Clearing an area for a push
  (state 185, `SaveClearArea` 0x7558E0: +0x118, 8 bytes, and the radius +0x120) still fails the write: openblack never
  enters the state. The flying, landed, reaction and wait-for-a-clip states write nothing. Not yet exact: openblack
  keeps each use of a shared slot apart, so a value a state's path does not set itself (a trader set to nothing to do
  before its first trade, a builder fetching materials before it reaches the site, any state's values before anything
  set them, which openblack writes as 0) is written as openblack last set that use, where the original writes whatever
  another state left in the slot; and a place openblack keeps in metres (a fire's, a script's) goes back to whole map
  units through `map_coords::ToFixed`, which may be one unit off one that came from whole map units (the hand's
  disciple number, which the original leaves in a script place's x) (tests `VillagerStateValues.*`).
  A load puts back (2026-11-10) the farming state's work point (`Villager::workPos`, the 12 bytes as written) and its
  field once every object is made (`targetThing`), the building states' (39..41) place at the site (`buildPosIndex`)
  and the site once every object is made (`buildingSite`), and the script states' place, number and pair
  (`ScriptStateArguments`, `ScriptAnimation`, each made only when a script gave it something, as in play), and the
  shepherd's turns to wait and the state it goes on to (`ShepherdWork`), the breeder's place to wander about
  (`wanderArea`) and its partner once every object is made (`targetThing`), and a villager's by a fire (states 216 to
  220, 2026-11-11): the place it goes back to after the fire (`VillagerFireState::savedDestination`) and its fire once
  the fires are made (`fire`, by the fire's id), and a villager in the hand (state 24, 2026-11-12: its value of 4
  bytes holds the disciple it was at the pick-up, `Villager::discipleOnPickUp`, which it becomes again as it lands;
  census saves 2, 6 and 9 held one); a villager in a flock (a shepherd) is made
  too, its place in the flock's line kept and its flock given back once the flocks are made (one whose flock is not
  made is left without one), so farmers, shepherds with their flocks and villagers in a script's state are made by
  the load (a Land 1 free-play quick save had 17 farmers, 2 in a script's state and a shepherd not made, and with the
  shepherd his flock and its 7 animals); test `ForestWriteWorld.AFarmerAndAVillagerInAScriptsStateAreMadeAgainWithTheirStatesValues`.
- **The worship sites** (0x3C `WorshipSite::Save` 0x77D2F0, `Load` 0x77D700): a citadel part (the object of several
  cells' parts, its influence, its citadel and a reference always 0), its tribe's row (the tribe rows are read first:
  742 down; the site rows run from 2405 down), its needs' signs, its dance, its towns, its food pot, two words and a
  byte only the save and load touch, its worshippers' count, a word, the cheat spell seed, its worshippers, its spell
  icons, the resources on their way (food, then wood), its six chants (stored, sacrificed, available, used, requested,
  damage), the two cheats, its slot, whether an icon took chants, those requesting to go home (the load puts each before
  the first whose desire for life is less, `ReadSafe` 0x77E910 with `GetDesireForLife` 0x75BBA0), its totem, its strain,
  its mana path and the strain's pulse. Nothing in the game adds to those requesting to go home
  (`AddVillagerRequestingToGoHome` 0x77E0C0's only caller, `CheckRequestGoHome` 0x76C8D0, is never called). openblack
  makes each site (`MakeWorshipSiteEntity`, once the citadels' hearts, towns and piles are made) in its saved slot of its
  citadel (`worship::site::CreateInSlot`, the step of making a site that `Create` also takes), of its tribe, as built as
  saved, with its towns (each town's site set as `AddTown` sets it, without its footpaths), food pot, chants, cheats,
  strain and food on its way, and its totem and need signs made as the game makes them; its villagers and those requesting to go home (`InsertGoHomeRequest`) come once the villagers are
  made. A worshipper's word (`Villager::SaveWorship` 0x754C00, the state slot the dance states share) is kept with its
  record; one that enters a worship state in play writes 0, as nothing in the game sets it. The original's load sets
  the citadel's six sites from the citadel's own record (`Citadel::Save` 0x463B00, six pointers) and the site reads
  only its slot; openblack sets the slot's site from the site, and logs one line when the citadel's kept record names
  another there. `WorshipSite::ResolveLoad` 0x77DAF0 is only `Object::ResolveLoad`: no sound or part list is rebuilt,
  and the strain sound follows the sites' strain each turn. Its spell icons (0x78, `WorshipSpellIcon::Save`
  0x77FF80, listed in the site's record) are each made at its saved place with its seed (the seed rows run from 2578
  down) and its saved half, as the game makes one, at the head of the site's list, the saved list's last first so that
  they keep its order; then given their creation numbers, marks, remove timer, scale, charge start, charging, power-up
  and chant store, and their records. Not put back yet: an icon's charging hand and the seeds made from it (their
  records are kept, and one log line names such an icon). The site's mana path (the record's `manaPath`, the
  particles' interface of its effect) is made again from its saved particles once the containers are (`PlannedWorshipSite::manaPath`,
  `saved_particles::RestoreEffect`, adopted as the site's `manaPathEffect`; tests
  `SavedWorldLoad.AWorshipSitesManaPathIsPlannedWithItsParticles`, the frozen 17/3 save's site in
  `ForestWriteWorld.AFrozenSavesWorshipSiteIsMadeInItsSlotWithItsTownsAndWorshippers`: interface 5030, manager 5031); before,
  it was left as read and the site made a new one with its first mote of prayer power.
  The totem (0x98 `WorshipTotem::Load` 0x780F10: a citadel part, its specular colour and its site) and the three
  need signs (0x3D `ShowNeedsVisuals::Load` 0x71A320: an object, two words, their needs' record and the desire; the
  needs' record 0x3E lists them) are made as the game makes them and given their saved creation numbers, a sign its
  saved desire, and their records, so that they are written back as loaded; their needs' record is kept with the
  world read.
  The site's dance is the dance whose owner is the site, given by the dance's load (test
  `ForestWriteWorld.AFrozenSavesWorshipSiteIsMadeInItsSlotWithItsTownsAndWorshippers`).
- **The dances** (0x27 `Dance::Save` 0x50C480 after `GroupBehaviour::Save` 0x598240; its groups 0x7B
  `DanceGroup::Save` 0x50D640, its key frames 0x7A `DanceKeyFrame::Save` 0x50F1C0 and their actions 0x79
  `DanceKeyAction::Save` 0x50E880): the group behaviour's place, clock, key frames, group count, two editor values (30
  and 0 without a file), loop length, its groups, the fixed and the shared ones, its dancers, the round and its length,
  its two speeds, an empty member list, its file's facing, the camera's four flags, travel time and near turns; then the
  dance's type, worship speed, target, state (0 dancing, 1 waiting), duration, start turn, whether and since when it
  waits for its dancers, those on their way, the label "begin", a word the game always makes 0 and its footpath link.
  A group writes its dance twice, its dancers in the order they joined, its number, hidden flag, formation and shape,
  offset, size and angle, its move, spin, parent and place there, its fixed dancers, whether it moved, its speed,
  whether it has a last centre and its two centres, its dance type, its formation's places, its name, its proportion,
  its child groups and the sexes it takes (a loaded group's name with the bytes its text held past its end, while the
  name is the same). A key frame writes its time, its actions and its flags; an action the groups it is done to (in its
  file's order: the game's stack has the last read at its top, and the save writes it from its bottom, 0x50E880; a
  frozen save's two-group actions are so; the game does them from the top, the other way round, 0x5965C0), its type and its 56 bytes of
  arguments.
  The game's list of dances holds them newest first (`Dance::Dance` 0x50B6A0 puts each at its head). openblack writes a
  dance (`src/ECS/SavedWorld/WriteDances.cpp`) and its groups, key frames and actions as sources keyed by the dance
  and their numbers there (a living dancing in a group names it the same way), from the dance's state; what openblack
  does not hold (a footpath link openblack does not give it) is as its load kept it, else as made (the kept link, Dance +0xE8, cannot name a deleted link: only `Dance::Initialise` 0x50B7A0 and `Dance::AddFootpath` 0x50CA60 write it and the link is the dance's own; a group's members are written from the live groups, and the original takes a deleted dancer out at its own delete, `Living::ToBeDeleted` 0x5EC160 -> `RemoveFromDance` vt+0xB08, 2026-11-11); a group's name is
  its own: every `DanceGroup` the game makes (0x50CE40: `Dance::Initialise`'s first, `LoadDance`'s up to the file's
  count, a key frame's past the last, `LoadInstance`'s) takes "Untitled %d" from the counter 0xBE8290, 1 at the start
  and set back to 1 only by `GGame::Close` 0x54EDCB, a file's names then given to its own groups; openblack's
  `DanceSystem::NextUntitledNumber`, a load's groups taking theirs (a group with no name still stops the write); a group's speed, last-centre flag and two
  centres are its own, as measured each turn (see [Dances](../beings/dances.md#openblack)); the
  camera's four flags, travel time and near turns are its own (the hand's count, the call and the travel time, see
  [Dances](../beings/dances.md#the-dance-camera-actions-18-19); marked for deletion
  is written 0) (tests
  `ForestWriteWorld.ADanceMadeInPlayIsWrittenWithItsGroups`, and `AFrozenSavesDanceWritesBackItsRecordsAsSaved`: the
  blaze save's Ritual dance, its two groups, its key frames and their actions, each written back as saved).
  The load makes each dance of the game's list from its last to its first, once its dancers and what it is danced
  about are made (`MakeDanceEntity`): a dance of its row with the file its row names (`DanceSystem::FileOf`), its saved
  place, clock, loop, speeds, round, state and times and owner, and its groups as their records say, each dancer given
  its place (`components::Dancer`); its groups, key frames and actions keep their records by the dance and their
  numbers, so that what openblack does not hold is written as loaded. A dance whose dancers were not all made is not
  made (a dancer's dance state saves values, which the villager load does not make yet), with one log line.
- **The fires** (0x29, `FireEffect::Save` 0x730B00, `Load` 0x730DF0): the thing's part, the charring (+0x34), the
  process tag [0xDA09E4], the fire's own tag (+0x30), the create tag [0xDA09E5], the temperature (+0x14) and the one at
  the end of its last turn (+0x18), the object it burns (+0x1C, which points back at it through its object part's
  fire), its heater (+0x24), its player (+0x20) and its reaction (+0x28), its flags (+0x38), the next fire of its group
  (+0x40) and its root (+0x44), its firemen (a count and each villager) and the two tags again; the load reads all four
  tag bytes back into the two tags, so the last fire read leaves them. It is made with the default constructor
  0x72EA80 (in no list) and `ResolveLoad` 0x7310D0 only makes its sprites; the game's list of fires (global list 14)
  holds them newest first, as the constructor 0x72EB00 puts each new one at its head. openblack makes them once every
  object is made, from the list's last to its first (`MakeFires`, through `fire::Restore`: a new handle, the saved
  values, its sprites, nothing done to its object), links each group's made fires in the order its saved chain runs
  from its saved root (a root or a middle fire that was not made is passed over, the first made one becomes the root, so
  every fire's root and the chain from it agree: before, a fire whose saved root was not made became its own root while
  still on the old chain, and a later group merge made the chain a loop, the hang after loading `_J_a_h_o_v_i_a` 14/1,
  whose fire on a spell dispenser is not made; the original loads every fire with its raw next and root, 0x730DF0, so
  its chains are whole), gives each its firemen,
  and sets the two tags as the last fire read left them; a fire whose object was not made (one the load does not make,
  such as a held villager) is not made. A loaded fire takes back its reaction (+0x28, `ReadPtr` in `FireEffect::Load`): the reaction its record
  names, made again by the reactions' load (`GiveFiresTheirReactions`, 2026-11-11), so it does not make a second one
  on its first turn; a written one names its reaction while it lasts and none once it has ended, as the original's
  pointer goes with its reaction, and one its load did not make again (no reactions' service) as kept (test
  `Data/ForestWriteWorld.AFiresReactionIsWrittenWhileItLastsAndGivenBackByTheLoad`; before, the reaction the load kept
  was written whatever became of it, and a save after the reaction ended was refused: "reaction 36 is gone", named by
  a fire, on `_J_a_h_o_v_i_a` 14/1). The writer (`src/ECS/SavedWorld/WriteFires.cpp`) is a
  source keyed by each fire's handle, in the list's order; for a loaded game the list is the fires made since,
  newest first, then the loaded list with each fire still burning and each one the load did not make, as loaded: the
  load reads the list in its saved order, each at the end (`GlobalGameLists::Load` GlobalGameList.cpp:345,
  `GameOSFile::ReadSafe(LHListHead&)` GameOSFile.h:341 `AddToLast`), and a fire made in play goes to its head
  (0x72EB00: `next = GameLists.FireEffects.head; head = this`). The frozen Land 1 autosave holds no fire; a Land 1 save of a
  blaze (`_J_a_h_o_v_i_a`, 14/1) holds 13 in one group (tests
  `ForestWriteWorld.AFireIsWrittenFromItsStateAndMadeAgainFromItsRecord`,
  `ForestWriteWorld.AFrozenSaveOfABlazeWritesBackItsFiresByteForByte`).
  A fire whose next or root the load did not make keeps that link as loaded. Every object the load makes takes back
  the marks its saved place's flags give it (held by a script 0x200, under a script's control 0x400, immovable 0x1000,
  not to be picked up 0x2000, indestructible 0x4000), so that they are written as saved (the blaze save's script-held
  mobile static, 0xB703).
- **The rivers and the banks of mist** (`ecs::saved_world_load`): a river (`GStream::Save` 0x734380: its nodes,
  each a point, x, the ground's height and z, and a word of the game's memory, then its id and its last node's index)
  comes back with its id and points as saved (`MakeStreamEntity`), and once every river is back their landscape
  footprints are made (`ecs::CreateRiverFootprints`, as `GStream::ResolveLoad` 0x734640 makes each river's through
  `CreateRiver`). A bank of mist (`Mist::Save` 0x606920: its place, then +0x2C its size, +0x30 its colour and +0x34
  its edge-on shrink, the constructor's three values) is made through the mist archetype at its place and its
  altitude over the land (`MakeMistEntity`). On the frozen Land 1 autosave both are Land1.txt's.
  Both are written back by their class writers (`src/ECS/SavedWorld/WriteRiversAndMists.cpp`): a river's points
  from its entity, each followed by the word its record kept (the point's link to the next in the game's memory, which
  a load does not read; 0 for a river made since), its id (+0x1C) and its last point's index (+0x20, the count less
  one, `fn_00733C90`); a bank of mist's size, colour and shrink from its entity, and its place from its record while it
  stands where it was loaded (a place in metres does not give back the same map coordinates), else from where it
  stands. The frozen Land 1 autosave written back with them is the file the load kept, byte for byte (tests
  `RiverAndMistWriteTest.*`).
- **The parts every object writes first** (`src/ECS/SavedWorld/WriteObjectParts.h`, for every class's writer): the place
  (`GameThingWithPos::Save` 0x5703F0: the map coordinates, kept while the object stands where its load put it, and for
  one made in play those of where it stands with its height over the land under it, its target the same; and the
  flags word, made from openblack's state: 0x1 in the map, 0x2 of several cells, 0x4 in a god's hand, 0x10 in the
  hand's locked select, 0x40 in the physics, 0x100 in the landscape's draw list, 0x200 in a script, 0x400 controlled
  by a script, 0x1000 immovable, 0x2000 cannot be picked up, 0x4000 indestructible, 0x8000 of a type that counts as
  fixed; 0x80, on a structure, is kept as loaded, as openblack does not hold it; 0x8, 0x20 and 0x800 are never set on
  an object), the object's values (`Object::Save` 0x639B90: its row, its target place, its creation number, life,
  size, angle and fire), the fixed object's town artifact (`Fixed::Save` 0x52E140, as kept) and the object of several
  cells' flags, built percent, footpath link and building site (`MultiMapFixed::Save` 0x52F250).
- **The parts every moving and living object writes** (`src/ECS/SavedWorld/WriteLivingParts.h`, for the villagers',
  animals', creatures' and moving objects' writers; the villagers' writer uses them). Each value comes from what openblack holds, else
  the part the load kept, else the constructor's value; a value with none of the three stops the write naming the
  object. The made-in-play values are the constructors' and the zero-filled allocation's (`Base::operator new`
  0x4366F0, `rep stosd` 0x4368C4): `LivingConstructorParts(info)` gives `Living::Living` 0x5EBEC0's, with
  `MobileWallHug::SetToZero` 0x60F760, `LivingAction::LivingAction` 0x5ECC70 and `Living::SetToZero` 0x5EC030: the
  speed `SetSpeed(info's default speed)` (0x60FC50 keeps it within 0..0xFFFF), +0x9C the row's strength through
  `_ftol`, every other value 0; the birth turn is drawn (`SetAge` of a random age), so a living thing made in play
  whose birth turn openblack does not hold fails. A class whose constructor sets more (`Villager::Villager` 0x74F950:
  the counter GameRand(500) + 1) puts its own values in.
  - The moving thing's word (`Mobile::Save` 0x606C10, +0x54): nothing but `Mobile::Load` writes it (the field scans
    of Mobile, MobileWallHug, Living, Villager, Animal and Creature find no other access; 0 on the 47 villagers and
    116 animals of the frozen Land 1 autosave scanned): as kept, 0 when made.
  - A moving object's (`MobileObject::Save` 0x607C40): its object, x and z angles and path, which openblack does not
    hold: as kept; made in play, the object it is made with and 0, 0, no path (`MobileObject::MobileObject` 0x606E40;
    `SetXYZAngles` 0x607460 and `SetupMoveAlongPath` 0x6076C0 change them later, unread: written as made).
  - The walk round obstacles (`MobileWallHug::Save` 0x60C420): a villager's counter (`LivingAction`'s, the original's
    +0x58), speed (`WholeSpeed`), game angle, turns to its step rebuild, goal and step from `WallHug`, its circle's
    turns and object from `WallHugObjectReference`, its next footpath (+0x7C) from `LivingFootpath::next`; an
    animal's counter, speed, angle, move state, circle turns and object, goal with its altitude, target (+0x60),
    step and turns to its step rebuild from `AnimalBrain`. An animal's step-rebuild counter (+0x78) is set by the
    four ways into STEP_THROUGH and never counted down for an animal (`MoveTo` 0x60AF20 decrements it only when
    `IsAnimal` is false): 1 by `SetupMobileMoveToPos` 0x60AAD0 / 0x60ABC0 and `SetupMobileMoveToObject` 0x60ACD0,
    16 by `MoveTo`'s ARRIVED case falling through (0x60B022) and by the circle square sweep's straight-at-the-goal
    branch (0x614C40 / 0x6159F0); `MoveToObjectPos` leaves it. openblack's `AnimalBrain::turnsUntilStepRebuild`
    holds it, loaded from the record. A goal keeps the kept map coordinates while it is their place. A villager's move state
    is its walk's tag; with none, the state the original keeps after a walk (it changes `MoveState` +0x5E only in the
    walk set-ups and `MoveTo` 0x60AF20, whose FINAL_STEP case snaps to the goal and returns 10 leaving it 4, and
    nothing outside `MoveTo` reads it but `ForesterMoveToForest`'s mid-walk test): openblack keeps the state of the
    tags it drops (`wall_hug::DropWalkTags`, `WalkStateAfterWalk`, read only by saving), and a villager that never
    walked writes the 0 it was made with. Kept: a villager's target and the step's height. A villager's circle (its
    `WallHugObjectReference`) is written as `MobileWallHug::Save` resolves it (`CircleHugInfo::FetchObjectFromCircHugInfo`
    0x60A450): the entry distance as held; a thing's circle names the thing, with -1 for a thing of one circle (its
    collide data's own circle) or its place among the circles of the thing's row, and no place; water or the land's
    edge names no thing, with the index 1 and the circle's centre in map units with altitude 0
    (`ConvertLandscapePointToMapCoord` 0x6137F0). A villager in a circling state holding no circle (the LINEAR sweep
    `MoveToCircleHugLinearSquareSweep` clears the pointer first and sets TurnsToObj 0xFF when it finds none) writes
    TurnsToObj 0xFF, no object, the index 0 and `MapCoords()` (the NULL branch of `FetchObjectFromCircHugInfo`,
    `MobileWallHug.cpp`); its entry distance is openblack's, 0, as for a held circle (the orbit's own entry distance,
    0x60D9F0, is not wired yet); a circle a load kept stays as kept. An animal holding no circle writes the same with its own TurnsToObj and entry distance (`AnimalBrain::turnsToObj`,
    `hugGoalDistance`); an animal's held circle is written as a villager's (2026-11-11): its entry distance
    (`hugGoalDistance`), a thing's circle by its place among the thing's circles (-1 for one of one circle) with no
    place, water or the land's edge by the index 1 and its centre with no height; before, a circling animal with no kept
    circle stopped the save (the user's sheep). A step through with no turns to its rebuild still fails.
  - The living thing (`Living::Save` 0x5EE630): the footpath (+0xC8, written twice), its node (the footpath's part,
    `PartId`), the states and turns since the change (+0x8C..+0x8E, +0x90), the reaction (a reaction's id) and its
    object (+0xBC), the birth turn, the status bits openblack keeps (a villager's 0x1, 0x30, 0x40, 0x400 with 0x2
    poisoned and 0x80 downed; an animal's 0x1, 0x30, 0x80; 0x8 the way along a footpath; the others as kept), the
    flock and the place in it (+0xD4), a child's creche walk (+0xC4), the specular colour (+0xD0: blue, green, red and
    0xFF, the only colour the heal chakra writes through `Living::SetSpecularColor` 0x417480, else 0), the dance group
    (the dance's part) and the place in it (+0xDC), and the reactions done, oldest first (the list's head, as
    `Living::SetReactionDoneWhen` 0x6E44A0 appends at the tail; each a u32 type, fn_005EF800, and its turn). Kept:
    +0x8F, +0x9C (the strength, written only by the constructors), the byte +0xA8 (only `Save` and `Load` touch it),
    the data path (a living thing that walks a track names its own, which `WriteDataPaths.cpp` writes from its walk: TrackNo, EndPercentage the share it stops at, Reverse the forward flag +0x20, Frame and FramesPerTurn in the track's milliseconds, +0x2C its whole speed as a float, as `Living`'s 0x5EE100 sets them; before 2026-11-11 such a save stopped) and the script reminder. Tests
    `SavedWorldWriteLivingParts.*`, with the frozen Land 1 autosave's villagers and animals written back from entities
    holding what their records say. 🎯 Chosen difference (2026-11-11): a living whose footpath no longer holds its
    node (a re-plan deleted it under the living, as `GFootpath::StopGoingRoundObsticle`'s passes 2 and 3a do) is
    written on its footpath with no node, and the save is never refused for it; the original's `WritePtr` 0x561E10
    writes the freed node, which is undefined
    ([chosen differences](../project/reconstruction.md#what-is-deliberately-different-chosen-differences); test
    `SavedWorldWriteLivingParts.ALivingOnANodeItsFootpathNoLongerHoldsIsWrittenWithNoNode`).
  - **A creature's** (surveyed field by field in the exe: the writers of each `Living` and `MobileWallHug` field that
    run with a creature). A creature walks by its own body and the game drives no `Living` states on it (no creature
    code calls `SetState`, `SetCurrentAndDestinationState` or the state entries and exits on itself), so of the walk
    round obstacles only two values are its own: the turns its hand watch left (+0x58, `SetupReactToHandPickUp`,
    `CreatureReaction::turnsToStateChange`) and its game angle (+0x5C, `MobileWallHug::SetYAngle` from a script's angle:
    `ConvertAngle3DToGame(Creature::scriptYAngle)`); its speed is the row's from the constructor, and the move state,
    target, step, circle, step-through turns, footpath and goal are never written on a creature (the walk's functions
    run for villagers and animals only), so they are as made. Of the living values: the states as made; the reaction it
    follows (by the reactions' source key) and what it reacts to (`CreatureReaction`); its birth turn
    (`Creature::birthTurn`: the turn it is made on less the living part's drawn age, `CreatureArchetype::Create`, or as
    its load kept it); of the status bits,
    poisoned (`Poisoned`, from SET_POISONED or a spell's effect), the skeleton mark as kept (not held on a creature
    yet), the others as made (the dying, downed, landing and footpath bits are set by
    villagers' and animals' code only); the flock, data path, script reminder, creche walk, flock place and the byte
    +0xA8 as made (only constructors, `SetToZero` and the load write them on a creature); the specular colour, the
    dance group and the reactions done as for every living thing (test
    `SavedWorldWriteLivingParts.ACreatureIsWrittenFromWhatItHoldsAndElseAsMade`).
- **A loaded object whose entity changed class since is gone** (2026-11-12; `ClassWriters::StillOf`,
  `WriteContext::IsGone`): the original deletes a felled or dying tree and makes a dead tree, a new object, where
  openblack keeps the same entity. A kept list drops the loaded number (the game's trees' list among them), `UnlessGone` makes it none, and a record still
  naming it refuses the save ("is gone: it is no longer of that class"), never asking the tree's writer for a tree the
  entity no longer has (before, the game crashed saving four of the frozen saves; test
  `SavedWorldWrite.ALoadedObjectWhoseEntityChangedClassSinceIsGone`).
- **The game's spells list** (2026-11-12; `WriteSpells.cpp`): the live spells, the newest first, then the loaded
  file's spells the load kept as read; a spell the load made again and deleted since is out of it, as the original's
  spell leaves the list as it is deleted (before, an ended water or shield spell refused the save as gone in four of
  the frozen saves; test `SpellWrite.AKeptSpellDeletedSinceTheLoadLeavesTheGamesSpellsList`).
- **The flocks are written by their writer** (`src/ECS/SavedWorld/WriteFlocks.cpp`; tests `FlockWriteTest.*`), in
  [the flock's `Save` order](#the-town-centre-the-flocks-and-the-building-sites) (xref notes #2844-#2850, checked
  against the 1,686 flocks of the 16 frozen saves): the container part, its place the domain's centre
  (`Flock::domainCentre`), its info (`Flock::info`: the default flock info `g_defaultFlockInfo` every maker passes, which no info table holds, so `GetInfoID` gives none, 0xFFFFFFFF, as every saved flock has it; `flocks::k_DefaultFlockInfo`), its player; the three words 0, 0xFFFFFFFF,
  0xFFFFFFFF; no shepherd (openblack has none), its town, its temple (the heart whose `CitadelHeart::flock` it is); its
  members from the head (`flocks::MembersFromHead`); `maxMembers`, `leaderTurns` (+0x4C), `domainRadius`,
  `flockDistance`, `altitude` (the flying height), no shepherd to follow, `calm` (+0x5C); `savedDomainCentre` twice
  (+0x60 and +0x6C, which only the constructors write, the same in every save); `followState`, `afterMove`,
  `followMode`; no saved thing; its script id, 0 for an animal's own flock (openblack's -1). The game's list, global
  21, newest first by `Flock::creationStamp`. A loaded game's list is written as kept, its flocks made again by the
  load written by this writer, after the flocks made since, newest first, as the original adds each new flock at the
  head of the list (test `FlockWriteTest.AFlockMadeSinceALoadGoesAtTheHeadOfTheLoadedList`; before 2026-11-10 a flock
  made since stopped the write, refusing the quick save of two frozen Land 1 autosaves); a loaded flock destroyed since
  is out of it, as `Flock::DeleteDependancys` 0x52FE60 (from `Flock::ToBeDeleted` 0x52FFB0) unlinks the flock from
  the game's list (test `FlockWriteTest.ALoadedFlockDestroyedSinceIsOutOfTheLoadedList`; before 2026-11-10 the save
  was refused, the list naming an object that is gone). A flock the load made keeps, while it is where its
  load put it, its two places' map coordinates and its place's flags as loaded: the original holds them as map
  coordinates and never converts them, while openblack's flock holds metres, whose conversion back can move a
  coordinate by a unit or two (two of the frozen Land 2 autosave's flocks), and the place's flags are not held (one of
  its flocks has 0x600 there). Its animals are written by their writer
  (`src/ECS/SavedWorld/WriteAnimals.cpp`, `RegisterAnimalWriters`, one per class of animal record): an animal the load
  made, of the class its load kept, with its object and living parts from what openblack holds (`PositionPartOf`,
  `ObjectPartOf`, `LivingPartsOf`), its town, needs, sleeping cell, death reason and tilt (the bank zoomer,
  `saved_zoomer::BytesOf`; a loaded animal's is cleared, as `Animal::CallVirtualFunctionsForCreation` 0x41A040 clears
  it at the resolve) from its components, and its other values as kept. An animal made in play is written in the
  class its kind's factory makes (the cow 0x82 to the wolf 0x8C, the dove 0x8E to the vulture 0x96), with its
  constructor's values where openblack holds nothing (`Animal::SetToZero` 0x417900 clears them all): +0xEC and the
  physical object's place, which only the constructors write (`gx.py grep --writes`), are exact; the food place
  (`ReactToAnimalFoodNeeds`, the hunts' and `IsGrazingPos`), the thing it pounced on (+0xFC, `FinishPouncing`) and the
  surroundings' place (`SeekEnvironment`) are written in play by code whose values openblack does not hold yet, so an
  animal that has hunted, grazed or sought its surroundings writes its constructor's 0 there.
  An animal that has had no turn yet (a save as the game starts: the exit save at turn 0) is written with the values
  it was made with, as the original's animal holds them from its constructor (`Animal::Save`, bw1-decomp
  `src/Black/Animal.cpp`, writes the fields as held, with no first-turn check): openblack makes an animal's brain at
  its first turn, so the writers take the brain it would be made with (`animal_ai::BrainAsMade`: its row's speed, the
  heading 0, the birth turn of its age, a bird's height, its flock's centre as its sleeping place, DECIDE_WHAT_TO_DO,
  its needs 0) and its row's life, and make it no brain (2026-11-10; before, the save was refused, "has had no turn
  yet"; test `FlockLoadWorld.AnAnimalThatHasHadNoTurnYetIsWrittenWithTheValuesItWasMadeWith`). A predator made
  with no flock heads for its lair from its making, as the original's (`Animal::CreateFromType` 0x419C20 calls
  `CalculeLairPos`, whose `Flock::SetDomainCentrePos` 0x52FC20 writes the flock's tail's destination), so a turn-0 save
  writes that goal (2026-11-10; before, openblack gave it the goal at its first turn; test
  `FlockLoadWorld.ALonePredatorIsMadeHeadingForItsLair`).
- **The load's flocks, animals and birds** (`ecs::saved_world_load`, `MakeAnimalEntity`, `MakeFlockEntity`): the
  animals first, in the order their reads ended, each through `AnimalArchetype::Restore` (its saved place, y angle,
  scale, creation number, life and marks, no draw) with its brain as its record says (states, counters, move, needs,
  sleeping cell, status, birth turn; its age from it at the save's turn), its death reason, specular colour and the
  reactions it did (`ReactionRecords`: `Living::SetReactionDoneWhen` 0x6E44A0, their only adder, refreshes a type's turn
  or adds it at the tail after dropping the head once it holds three, so no save of the original holds more than
  three; `Living::Load` 0x5EE960 reads however many a file lists, and one with more, which openblack's three slots
  cannot hold, is not made); then
  each flock, only when everything it names (its members, shepherd, town and temple heart) is made, with every value
  the flock writer writes, its stamp from its place in the game's list, and its members in the line `Flock::Load`
  puts them in (`LoadedFlockLine`: each before the first from the head whose place is not lower; a member whose own
  read is still going on, having pulled its flock in, has place 0 then); an animal of a flock not made is taken away
  again. Every member of every frozen save's flocks has place 0, so a load puts each flock's members back the other way
  round, and so does the original: its own load and save of the frozen Land 1 autosave (`land1_autosave_resumed_120s`,
  a local reference save) has each of the 16 flocks of two or more members with its members, by their creation
  numbers, in the reverse order (test `FlockLoadOriginal.TheOriginalsResaveReversesTheFlocksTies`). A load and save of
  a frozen file is therefore not that file byte for byte: the flocks' member lists are reversed. Then each town's
  flocks as its record lists them, each animal's flock (its own record's) and town (the first pointer of its own
  part), predator and target, its 3D object and map cells (`AnimalArchetype::PlaceRestored`), and, once the reactions
  are made, the reaction it follows. Its state's clip is the resolves' that draw (`saved_world_load::Resolve`), which
  `Game::LoadSavedGame` makes after `saved_game_load::Apply` has put the seeds back, in the order the reads ended, as
  the original's `ResolveAllLoads` comes after the whole read: a bird flying or an animal eating draws its clip there.
  An animal the physics moved at the save (thrown or falling: its place's flag 0x40, neither unavailable nor flown by
  the particles, its body in its object part) is made and goes back into the physics with that body (2026-11-11), as
  `Object::Load` 0x639EB0 (bw1-decomp Object.cpp:1886) reads the body only for such a thing and `Object::ResolveLoad` (0x63A020,
  Object.cpp:1907, which `Animal::ResolveLoad` and `Living::ResolveLoad` call) clears the flag and starts its physics
  standing still through the class's `InitialisePhysics` (`Living::InitialisePhysics` 0x5EFE10: FLYING unless already
  so, its previous state stored unless in the hand), then sets the body's matrix, velocity and angular momentum: the
  plan's `inPhysics`, put back after every object is made as the map objects' are (test
  `FlockLoadWorld.AnAnimalInThePhysicsIsMadeInItWithItsBodyAndWrittenBackWithIt`; on the frozen Land 3 saves a pigeon
  on fire and a sheep in the physics come back with their flocks,
  `FlockLoadWorld.TheFrozenSavesAnimalsInThePhysicsAreMadeAndTheirFlocksWithThem`). One a particle system carries
  (flag 0x10: its record keeps no body, and `Object::ResolveLoad` starts no physics for it) is not made yet: the three
  cows of `_J_a_h_o_v_i_a`'s slot 14 (14/1) are such.
  Not made yet, with one log line naming which: a spell's animal, a puzzle game's piece, one held by a script or flown
  by the particles, on a footpath, on a data path, in a dance, walking round a circle of nothing (no
  thing, an index neither 0 nor 1) or round a thing the load has not made. An animal walking round a circle is made in
  that walk (2026-11-11), as `MobileWallHug::Load` 0x60C740 (bw1-decomp MobileWallHug.cpp:1031) reads it in the seven
  circling states and `CircleHugInfo::SetResolutionInfo` 0x60A5E0 keeps its object, index and place to resolve once the objects are back: its TurnsToObj
  (`AnimalBrain::turnsToObj`) and entry distance (`hugGoalDistance`, held whole: the original keeps one of 0xFFFF or
  more apart, the same value), and its circle: none for no object and the index 0; water or the land's edge for the
  index 1, the circle in the middle of the cell its saved centre is in with the blocker radius (as the villagers'
  load); a thing's at the saved place among the thing's circles, -1 its only one
  (`saved_world_load::AnimalThingCircleOf`, `RestoreAnimalCircle`). Written back, its walk is its record's (the
  writer's held circle, above). Before, every circling animal was refused, "walking round a thing" (56 in the 19
  census loads: 46 holding no circle, 8 water or the land's edge, 2 a thing: a tree and an abode), and with each its
  flock and the flock's other members. Tests
  `FlockLoadWorld.AnAnimalWalkingRoundACircleIsMadeInItsWalkAndWrittenBackAsItsRecordKeepsIt`,
  `FlockLoadWorld.AnAnimalWalkingRoundAThingNotMadeIsNotMade` and
  `FlockLoadWorld.TheFrozenSavesAnimalsWalkingRoundWaterOrNoCircleAreMadeInTheirWalk` (the 16 of `_J_a_h_o_v_i_a`
  slot 17's third save). Of the 148
  animals the 19 census loads refused for the four together on 2026-11-11, a reading of their records found 147 with
  a script's reminder (`Living` +0xB0, a `DataForScriptRemind` written inline after the animal's data path: doves,
  sheep and others an IN_SCRIPT state left one on, which `Living::ToBeDeleted` alone frees) and none on a footpath;
  the one left (a sheep of `_s_i_s_i` slot 20, whose reaction is written inline before them) was not read. An animal
  with a script's reminder is made (2026-11-11), its reminder its script memory (`ScriptMemoryOf`, as a villager's),
  written again by the reminders' writer keyed by the animal and named by its living part (`WriteAnimals.cpp`);
  before, it was refused, and with it its flock and the flock's other members. Tests
  `FlockLoadWorld.AnAnimalsScriptReminderIsKeptAndWrittenAgain`,
  `FlockLoadWorld.TheFrozenSavesAnimalsWithAScriptsReminderAreMade`. Tests `SavedWorldLoadAnimals.*` and `FlockLoadWorld.*` (the frozen Land 1 and Land 2 autosaves written
  back with their flocks by their writer: each as its record keeps it but for its members' line, the file as long).
- **The load's creatures** (`saved_world_load::MakeCreatureEntity`), after the animals: each made as a new creature of
  its row's species is (`CreatureArchetype::Create`, whose draws the seeds overwrite, as `Creature::Initialise`'s; it
  links its player, makes its own reaction and goes into the map), at its saved place, y angle and size with its saved
  alignment and strength; then its creation number, marks, whether it can die and go through a vortex, its sixteen
  spell slots (each naming the miracle the load made again from its saved number, none for one not made; `LinkSpellSlots`, test `CreatureWrite.ALoadedCreaturesSpellSlotsNameTheMiraclesMadeAgain`), freeze, fizz, its mind's stillness, size limits, the leash's target, its
  home (the pen), its fight health, autoscale and creed glows and the reactions it did (`ReactionRecords`), its record
  kept. The local player's creature is of the
  species of the profile's mind file, as `Creature::Load` reads it. In the resolves after the seeds
  (`saved_world_load::Resolve`, in the order the reads ended with the animals'), each creature reads its mind file
  (`CreatureMindFileOf`: the profile's for the local player's creature, the slot's `SavedCreature<index>` for the
  others) with the draws of its saved sources (`player_creature::DrawSavedSourcesOf`), its body (`RestoreBody`) and
  leashes from it, and the mind system takes the mind up (`LoadMind`); one whose file is not there is taken out of the
  map cells. Then the local player's creature knows the evil, rope and good leashes of the game's leash byte (bits 0, 1
  and 2). Not made yet, with one log line (`WhyCreatureNotMade`): a creature held, in flight, fighting, carrying out a
  kept plan other than a script's play or move, a forced look at the camera or death for good, or the plan a finished one
  leaves (a duel is made, below; a dance with villagers too), holding a miracle (+0x1078, or the two
  visuals +0x1084, +0x1088) or with a spell waiting. One with the beams of a cast from above at its hands (+0x107C,
  +0x1080) is made (2026-11-12): its beams, made with the spot visuals' containers, are given back to it
  (`CreatureCasting::beams`), so they close down and follow its hands each turn as in play once a cast has let go (the
  load of census save 1 refused its creature for its mirror hand's beam). A forced plan of looking at the camera (row 82, its one step 0x42 with the
  turns it looks for) or of lying dead for good (row 261, the faint 0x0E and the wait 0x18), its desire obeying its
  player, is taken up again once its mind is set up (2026-11-11, `ForcedPlanOf`, `LoadedForcedPlan`). Every kept
  agenda the load takes up is an order its mind waits on (`CreatureMindState::commanded`, 2026-11-12), as
  `Creature::ResolveLoad` (bw1-decomp `CreatureMentalSaveAndLoad.cpp`) sets `CommandState.Commanded` once it puts back
  the saved plan and agenda; `CreatureSubActionAgenda::Load` restarts the step it is at (its state 0, a new
  sub-action) and sets +0x20, the wait for the body's action before the agenda goes on, which `ProcessSubAction`
  clears; the action's own
  agenda at the step the plan is at, that step starting again, as a forced plan, with nothing else forcing does (the
  frozen Land 1 saves of the creature pen hold both). Sleeping at home (row 17, its player's order to go home: a yawn
  at random or not, the walk home 8, the face down the slope 0x44, the sleep 0x84 and the dazed wake 0x0A) is taken up
  the same way (2026-11-11), its agenda made with the yawn as the saved agenda's count has it, drawing nothing; and
  sitting down at a thing (row 81, four steps, the sit a played pose 4 last, with the seconds it is held for, the
  frozen Land 1 creature pen's candidates as the scripts sit them, in a save made in the temple's save game room; a
  creature a script holds sits by the script's sit, its seconds the saved ones, drawing nothing). A dance
  with villagers (2026-11-11; `ConstructSubActionsForDanceWithVillagers` 0x49C9A0, the rows that build with it and
  through `ConstructSubActionsForDanceImpressivelyWithVillagers` 0x49C960, such as 41 and 294) is written and read as
  its fourteen steps: STOP_MOVING 0x1D, GO_NEAR_OBJECT 0x3D (the villager, 5 heights), TURN_TO_FACE_OBJECT 6 (0.1),
  the main INDIVIDUAL_ACTION 0x0A (0x34), SET_DANCE 0x7E (the place, its row, 0, no spell), ADD_VILLAGERS_TO_DANCE 0x0C
  (the villager, the row's fewest dancers), STOP_LOOKING 0x30, the main MOVE_TO_POS 8 (the place, 0) and POINT_AT_POINT
  0x50 (the place, 1.0; the last main step, 8), 0x83, 0x19, 0x2C, 0x2E and DANCE 0x17 (the row's beats × 5). The place
  is the clear area's, altitude 0, so its point's height is the land's there (`MapCoords::GetLHPoint` 0x605C40).
  `DanceStepsOf` writes the steps left of openblack's `creature_dance::Agenda`; the load (`DanceOf`) makes that agenda
  again from the steps left and takes it up, at the step it was at, as the game's load takes up any kept plan
  (`Creature::ResolveLoad` 0x4E6AB0 sets the plan commanded). A script's move (its plan of following its player, every step
  left a walk 8 to within a distance of the script's point, the point at its height) is taken up again the same way,
  at the leg it was on (2026-11-11, `ScriptMoveOf`, `creature_mind::ScriptMoveLeg`; the creature guide's moves on
  Land 1). A script's focus (2026-11-11: `Creature::SetFocus` 0x4F6760 adds TURN_TO_FACE_POS 7 with the script's
  point, 0x4F6850 TURN_TO_FACE_OBJECT 6 with the thing and 2.0) is written as that one step of its plan of following
  its player, the point at the height the script gave it (`CreatureMindState::scriptFocus`), and taken up again the
  same way (`ScriptTurnOf`); the focus itself (the mind's look at the point) is not made again by the load. A creature
  that keeps villagers it called to dance with it (they stay on its list after the dance stops) is made with them, the
  ones the load made, the last called first, and names again the dance it made last as the dances are made
  (`components::CreatureDance`); its writer writes them and, as the original's save writes the plan of a creature
  with dancers, its plan; before 2026-11-10 the load left out every such creature and the dances it danced in (the
  user's Land 3 save in the temple: four creatures, two dances). A
  script's play (`ScriptPlayOf`: its animation and the plays left) is taken up again once the creature's mind is set
  up (`CreatureMindState::loadedScriptPlay`), the play it was at starting again from its beginning, as
  `CreatureSubActionAgenda::Load` (bw1-decomp `CreatureMentalSaveAndLoad.cpp`) sets the state back to 0 and the
  agenda's first word to 1.
- **The reactions are written by their writer** (`src/ECS/SavedWorld/WriteReactions.cpp`; tests
  `Data/ReactionWriteWorld.*`: one made in play, field by field, and the frozen Land 1 autosave and a profile's save
  with 66 reactions written whole byte for byte): a source of no entity, `SourceKey{ReactionRecord, id}` by the
  reaction's id (`effects::reactions`), and the game's list of them, the newest first. Each: its thing part as its
  load kept it, its initiator (a land's arena that started it named as the arena, `LandArenaKeyOf`), its followers
  from the oldest joiner (`effects::reactions::NextJoinOrder`, every class's
  follower), stealth, type, dance, start turn, whether it is active, its player (`SourceKey{k_PlayerType, number}`, 0
  for its null player) and its radius. The load makes each reaction of the game's list again, from its last to its
  first, without spreading it (`saved_world_load::MakeReaction`); an initiator or a dance the load made no entity of,
  and followers openblack has not put back on their living things, are named as the record kept them, without
  those gone since (2026-11-11, test `Data/ReactionWriteWorld.ALoadedReactionsKeptFollowersLeaveItAsTheyAreDeleted`:
  `Living::ToBeDeleted` 0x5EC0A0 calls `StopReacting` while it holds a reaction, and `Living::StopReacting` 0x5F1140
  takes it off the reaction's `ReactingLivings`; before, a save after such a follower died was refused, the dead
  villagers of reaction 394 on `_J_a_h_o_v_i_a` 14/1). An initiator or a dance the load made and the game has deleted
  since is written as none (2026-11-11, test `Data/ReactionWriteWorld.ALoadedReactionNamesNoInitiatorOrDanceGoneSince`):
  the reaction's dance is an `LHPTR` that goes to null with its dance, and a deleted building's reactions go with it
  (`MultiMapFixed::ToBeDeleted` 0x52E2B0 → `Reaction::RemoveAllReactionsInitiatedByObject`), which openblack's
  pruning does a turn later. Such a
  reaction has no initiator in play, so the next turn's pruning takes it away: the game keeps it only until then.
  The hand's pick-up reaction (2026-11-10): the original makes it with the player's interface status as its object
  (`Reaction::CreateReaction(status, 16, ...)`, [grab, hold and throw](../hand/grab-hold-and-throw.md)), so a save names
  the status there; openblack makes it with the hand entity, so the writer names a hand initiator as the first
  player's interface status (`SourceKey{InterfaceStatusRecord, PLAYER_ONE}`) and the load gives a reaction whose object
  is the first player's status the local hand again (`PlannedReaction::statusPlayer`). Before, every save after the
  first pick-up failed on the hand (an entity of no class). Test
  `Data/ReactionWriteWorld.AHandsPickUpReactionNamesItsPlayersInterfaceStatus`.
- **The forests, the trees and the big forests are written by their writers** (`src/ECS/SavedWorld/WriteForests.cpp`):
  a tree (`Tree::Save` 0x74C1B0) its parts, its turns to its next growth step, the size it grows to, its forest and its
  flags word (kept as loaded; a tree made since its wind slot in bits 2 to 5); a big forest its parts, its forest and
  its wood; a forest (`Forest::Save` 0x53AE40, a source keyed by its key, the game's list newest first) its container
  as loaded, whether it is scenic, its footpath link as loaded, its id, its two lists of trees, the grown and the
  growing, each nearest its centre first by the game's whole-number distance and a tree after those as near
  (`Forest::AddTree` 0x53A310, the ordered insert 0x53B310), a growing tree of a forest that is not scenic moving to
  the grown list once grown (`Forest::Process` 0x539DA0), its big forest, the turn of the last tree a forest planted
  and the id the next forest takes. A forest made in play (`ForestWriteWorld.AForestMadeInPlayIsWrittenAsTheGameMakesOne`)
  has its container at its centre in map units with no height and no place flags, of no row (0xFFFFFFFF) and no owner,
  and no footpath link, as all nine forests of the original's new Land 1 save have them. The frozen Land 1 autosave, its forests, trees, big forests, rivers and mists made as the load makes them,
  writes back byte for byte (tests `ForestWriteWorld.*`).
- **Checked against the original's own saves** (test `SavedWorldLoad.TheOriginalsReferenceSavesLoadAsTheirFilesSay`,
  gated on the reference saves of `_gamedata\original_saves`, found through `OPENBLACK_TEST_ORIGINAL_SAVES` or beside
  the game data's folder; `tools/original/save_ref.sh` makes them): each save the original wrote at a known moment is
  read to its end with every sum equal and loaded with fake makers; its land, land number, turn and clock are checked,
  of every class openblack loads as many are made as the file holds, the towns stand at their script's places by id,
  the storage pits and big forests at script places, and every tree at a script place has the script's type there.
- **The citadels' hearts** (`ecs::saved_world_load`), in the same pass as the trees: the original's resolve of a heart
  (`CitadelHeart::CallVirtualFunctionsForCreation`, CitadelHeart.cpp:1292) makes its temple's 3D side, its entrance
  when it has none, the map cells, the camera exclusion, the heart list, the collide data and the leashes, as the
  heart's creation does; it opens no worship site. openblack makes each heart through `CitadelArchetype::CreateHeart`
  with `openWorshipSites` false, of its citadel's player (the citadel's owner's number), at its saved place and
  angle, with its saved scale, life, build flags and percent built (its draw percent set from it) and creation number
  (`MakeCitadelHeartEntity`), its saved angle kept as its own (`placed_turn::KeepAngles`: the Land 1 temple save's
  heart keeps 36.0, which its turn gives back as -1.699) and its place's marks (in a script, script controlled,
  immovable, not to be picked up, indestructible) put back as a villager's are; a citadel's first heart keeps the citadel (openblack's `CitadelWorship` on the heart)
  with the citadel's record, and its other hearts are made of it. A heart is given the footpath link its record
  names. The first frozen Land 1 autosave holds no citadel record (why is not read: 🔍); a later Land 1
  autosave holds the player's temple, built (tests `SavedWorldLoad.*`, `SaveWorldReader.ReadsFromTheGameObjectDepthFirst...`).
- `components/gamesave` (std only; nothing in the game links it yet), from raffclar's component of the same name
  without the parts of his own world file: the world file's values, running sum and object numbers (`ObjectStream`,
  `SaveBytes`), the slots' folders and files with the room's folder walk (`SaveSlots`), the info file (`SaveGameInfo`,
  zeros where the game leaves its stack's bytes) and the world file's start (`VanillaSaveFile`). Tests in
  `test_gamesave`: synthetic goldens, among them the game object's running sums the original wrote for Land 1 and
  Land 2, and with the game's data every info file and world file's start of the frozen profiles, read, written back
  and their sums checked, and every mind file in its slot's folder.
- The pictures (`components/gamesave` `SavePictures`, `SavePictureMaker`, raffclar's): the 32,896-byte files read
  and written, the surface pointer kept as read so that a file writes back the same (0 in a new file), the pages and
  cells of each slot, and the game's steps from a screen to the temporary picture (the 256 × 128 sample, the smoothing,
  the small picture from even rows, five bits a colour, DXT1) and from it into a slot's cells. With the game's data
  every frozen picture writes back byte for byte, the compression gives back every temporary picture's blocks, and
  the slots' cells hold their saves' pictures (tests `SavedPicturesData.*`, `SaveScreenPicture.MatchesTheGameFiles`).
  The steps from a screen are raffclar's reading of the exe, checked on synthetic texels only.
- What fills the temporary picture (read 2026-11-11): `ScriptedScreenShot::Request` 0x828E90 (bw1-decomp
  `LH3DDivers.cpp`) with a shot of the temporary picture as both its picture and its thumbnails' file (Half 0,
  Thumbnail 4, so the small picture in the cell at (0, 128)), not full screen; it calls `Process` at once, which locks
  the primary surface and reads the frame on the screen there and then (32-bit: each texel the mean of its pixel and the
  one before it in its row, the row and column at `(long)(i * (side / 128 or 256))`, exact as the steps are powers of
  two of the screen's side). Its callers: `GGame::GoInsideCitadel` 0x553E10 (only coming in from the land, `CurrentRoom`
  7, before `ViewMode` = 1), `InstantSaveGame` 0x792FB0 (first, before `CurrentSlot` is set and the slot's cells are
  copied), `CreatureRoom::TakeScreenShot` and fn_00784690 (the challenge room's). The room's own save
  (`SaveCurrentSlot` 0x792CA0) takes none: it copies the picture taken as the player came in. openblack
  (`ecs::systems::SavePictureSystem`, `Locator::savePictures`): `TakePicture` as the temple is entered from the land
  (`TempleInterior::Activate`) and before every save by itself (`save_triggers`' `SaveIntoSlot`), of the frame on the
  screen (`AskShownFramePicture`; while a profile is kept every frame is kept aside for it, else the next frame drawn),
  sampled as `gamesave::SampleScreen` over the whole screen; once back (`Collect`, at the frame's end) the temporary
  picture is made (`MakeTemporaryPicture`) and the waiting slots' cells written (`PutSlotPictures`). The renderer gives
  the picture a frame or more later, so a save by itself first copies the old temporary picture, as `SaveSlot` does,
  and its cells are written again once the picture is back: the files end as the original's, written later. With no
  profile kept nothing is asked, kept or written. Tests `SavePictureSystem.*`.
- The world file's first records (`components/gamesave` `SavedRecords`): the part every object writes and the parts
  of an object, a fixed object, one of several cells, a moving thing, a moving object and a static that can move; the
  game object's head (up to its players), the players, the computer players with their trees of desires, their action
  states and attitudes, and the game statistics; each read and written in the game's layout, with synthetic goldens
  of their lengths and the places of their values. The fields whose meaning is not read yet are kept as they are, so
  a record writes back the same.
- The walk (`WorldReader`): `ReadWorld` reads a world file from the game object, each new object's record at its first
  reference, compares every stored sum with its own, and stops at the first class it does not know, with its number
  and its place; `WriteWorld` writes what was read again up to there. On every frozen save it reads the 13 records
  above with every sum equal and writes back the file's own bytes up to the stop (test
  `SavedGamesData.EveryWorldFileReadsFromTheGameObjectWithEverySumEqual`).
- The citadel, its heart, the footpaths, the storage pit, the piles of food and wood and the town (`TownRecords`):
  plain structs that list their values in the game's order (`SaveFields`: references, lists of references, counted
  values, values one by one, tables), read and written by one reader and one writer, with synthetic goldens of each
  record's length and places. A building's destruction mesh follows its flag. Every frozen save now reads further,
  with every stored sum equal, the stop's own included.
- The forests, the big forests and the trees (`ForestRecords`), listed the same way, with synthetic goldens.
- The abodes, the creche, the graveyard and the fields (`BuildingRecords`), the same way.
- The town centre, a script's reminder, the mobile static, the fish farm, the data path, the standard building site
  and the flock (`VillageRecords`), listed in order with synthetic goldens; with them the totem statue, the magic
  wood, the workshop, the scaffold and the reaction.
- The animals and the birds, one record for the 25 classes that write the same values, and a town's desire flags
  (`AnimalRecords`), with synthetic goldens.
- The planned buildings, the spell dispenser, the citadel's entrance and the one-off spell's seed (`PlannedRecords`);
  the player's interface, its status and leash, what a computer player saw, the sub-actions and their arguments, and
  the particles' container and interface (`InterfaceRecords`), with synthetic goldens.
- The creature (`CreatureRecords`), and a fire, a rock and a spell's seed (`EffectRecords`), with synthetic goldens.
- The worship site, its needs and their visuals, the worship totem, a town's artifact, and the dances with their key
  frames, key actions and groups (`WorshipRecords`), with synthetic goldens.
- The particle manager and its atom collections (`ParticleRecords`); the spell icons, the wonder, a script's highlight,
  the magic food, a workshop's building site and the dead and felled trees (`SpellIconRecords`); a building's
  destruction mesh (`SaveFields`), with synthetic goldens.
- The game object's data, bytes and global lists (`GameHead`), and 42 classes the lists meet (`WorldListRecords`),
  with synthetic goldens of each record's length.
- The game object's tail to the end of the file, the camera and its modes, and the classes the tail meets
  (`GameTailRecords`), with synthetic goldens.
- The villagers and the special villagers (`VillagerRecords`): the walk round obstacles and the living part read by
  their own code, as the game writes some of their values only in some states, and each state's values by the exe's
  table of the states, which the source keeps as read.
- `apps/savetool`: `savetool [--records] FILE|FOLDER...` prints each world file's start, its records by class, whether
  every stored sum is the reader's, where the read stopped and why, and whether the records write back the same.
- **A slot's save without the room** (`ecs::saved_slot::InstantSave(slot)`, as `InstantSaveGame`): the world file
  from the entities and the last load (nothing is written when it cannot be made whole), the info (`SaveGameInfo`: the
  slot, `NowText` as `UpdateDateAndTime`: the wide long date and time, the narrow short date and time where a wide
  call counts nothing (`saved_slot::ChooseLocaleText`), a text of 64 characters or more left unread but counted, and
  the label blank when the two counts come to 62 or more (`saved_slot::DateText`; test
  `SavedSlot.ASavesDateTakesTheShortDateWhereTheLongOneCannotBeReadAndCountsWhatIsNotRead`); both narrow texts are
  read into one buffer, as the original's, so a narrow call that writes nothing leaves the other's text there;
  `TitleOf` the slot's help text with the land's number), the pictures and the
  creatures' mind files, numbered in the game's list of creatures (newest first: `Creature::Initialise` puts a creature
  at the list's head, 0x4744C7..0x4744D2; openblack orders them by their creation numbers **🔍 inferred**), from each
  creature as it is now (`player_creature::ToMindFile`), and the script machine's `SaveState`; the save service's
  `Save` then writes, in the exe's order, the map pictures, the slot's picture cells, the info and the world's files.
  Off Windows the date is the C library's locale texts (`%X`, `%x`). Tests `SaveGameSystem.ASlotsSave*`,
  `SavedSlot.*`.
- The game saves in play through `saved_slot::SaveSlot`, which calls the world writer below
  ([The saves in play](#the-saves-in-play)); the land-script line writers are the vortex's
  (`src/ECS/VortexSave.cpp`), not a saved game's.
- The work goes into `components/gamesave` (the stream, the slot rules, the info file, the pictures and a record per
  class, each tested with synthetic bytes and, when the game's data is there, with the frozen saves), and a save service
  that turns records into entities and back. A check plays a profile that is not kept, so it never saves or loads.
- `gamesave::SavedRecord`, a record of any class the reader knows, is a struct over the variant of every class
  (`SavedRecordVariant`, over 150 classes) whose copies, moves, assignments, destruction and comparison are
  compiled once, in `SavedRecordCopy.cpp`, `SavedRecordMove.cpp` and `SavedRecordCompare.cpp` (apart, so that no one
  file makes them all, each with /bigobj): made in each file that copies or moves a record, each is a switch over every
  class with its copy inlined, which made such a test file take minutes and gigabytes to compile.

### Writing the world file

`ecs::saved_world_write::WriteWorld(writers, source)` (`src/ECS/SavedWorld/WorldWriter.h`) writes the world file as the
original's save walks its objects: the file's start, the game object, then each object's record at its first reference,
depth first, so the file numbers its objects in the walk's order as `WritePtr` does
([the values](#the-values-and-the-running-sum-gameosfileh)). What it writes each object from:

- **a class writer**, when the object's class has one: a function that makes the class's whole record from the entity;
- else **the record the entity's load kept** (`components::LoadedRecord<Record>`), per entity;
- a loaded object no entity stands for, **from the world read the load kept** (`WorldSource::kept`);
- the game object from the writer of its class (0x6A, called with `entt::null`), else from the kept world's first record.

**The game object** (`src/ECS/SavedWorld/WriteGame.cpp`, `RegisterGameObjectWriter`, in `GameWriters()`) starts from
the record its load kept (a new game: a new record) and puts in, where `GGame::Save` 0x554090 writes them
(Game.cpp:2679, the same order in the exe), the values openblack keeps (`saved_game_save::CurrentValues`, the inverse
of the load's `Apply`; `HeadOf` is pure): the land's number; in GData the synchronised and the local seed, the turn,
the creation counter and the frame count (FrameCount, the sixth word: the game loop's passes since the map's last
clear, `game_clock::FrameCount`, which `Apply` puts back; see
[the frame counter](../scripts/land-change-and-saving.md#in-openblack)); in the ten words after the lists the influence multipliers, the visual time, the cycle as
`GGameInfo` keeps it (+0x48 the day's length, +0x4C the change, +0x50 the night: `SetVisualTimeCycle` 0x557620 stores
its three arguments there before it works out the rates) and VisualTimeScale; in the tail the snow lying (the grid,
the blocks' changes, the melting clock and band) and the "did you know" texts read with their counts. The game
flags, the real turn, the local player, the leash byte, the render clock, LastAutoSaveTurn and the rest of the tail
stay as the record has them until their owners are written, but for two objects the original lets go of at their next turn: the help bubble's object (`helpObjects[1]`, HelpSystem +0x20, set 0 when not available by `HelpSystem::ProcessBubble` 0x5C82F0 each turn) and each bookmark's object (Bookmark +0x14, set 0 by `Bookmark::ValidateTarget` 0x439D80 from `Bookmark::Process` 0x439DA0; the bookmark stays set at its last place) are 0 once gone since the load (`context.UnlessGone`; 2026-11-11, test `SavedWorldWrite.TheGamesHelpBubbleAndBookmarksNameNoObjectGoneSinceTheLoad`). `helpObjects[0]`/`[2]` are the constant 0 `HelpSystem::Save` 0x5C6EB0 writes and `helpObject` is always 0 (its setter 0x5C9360 has no caller), so neither can name a gone object. The script part's two objects (`scriptRefs`) are the scripts' hit record, `GScript::Save` 0x6EBEA0 writing HitObject (+0x70, 0x6EC1E8) then ObjectWhichHit (+0x74, 0x6EC1F3), which `GScript::Load` 0x6EC270 reads back as loaded pointers (0x6EC576 / 0x6EC581) and `ValidateHitObjects` 0x6EB680 lets go of once no longer available at the scripts' next turn: the writer takes them from the physics' record as it is now (`DynamicsSystemInterface::GetHitObject` / `GetObjectWhichHit`, through `GameValues::hits`; 0 for none or a thing gone since; without a physics, as kept but `UnlessGone`), and the load puts them back (`LoadedScriptHits`, `RestoreHits`, with no availability test, as the original's load; Game.cpp at the load's end). Before, the record was written as the load kept it, so a thing gone since was still named and the save refused (the cow of the Land 1 save 14/1, drawn in by its vortex after the load), and a load left the record empty (2026-11-12, test `SavedWorldWrite.TheScriptsHitRecordIsWrittenAsThePhysicsKeepsItAndReadBack`). The players are the record's (a new game's: the eight of the
players' writer, [the players' records](#the-players-records); with none, the player entities by their numbers), and each global list is written by the list writer registered for it
(`ClassWriters::RegisterList(GlobalList::<List>, write)`: the objects in the game's order of the list, by
`context.Reference`), else as the load kept it; a new game needs every list's writer (tests
`SavedWorldWrite.TheGameObjects*`, `...AGameNotLoaded*`).

An entity, or a source's object, with neither a writer nor a kept record (one made in play whose class has no writer
yet) stops the write with a message naming it (`WorldWrite::failure`), never a silent gap. `WorldSource::loaded` gives the entity each loaded
object became, by its number in the loaded file, so that a kept record and a writer name the same object. With no
writers, a loaded game writes back its kept world's bytes (tests `SavedWorldWrite.*`).

A loaded object keeps its number in the loaded file as its handle (`WriteContext` reads the kept records' numbers
that way), and the stream numbers every handle again in the walk's order as it writes it, so a kept record's numbers
are valid as they are. An object made since the load must be referenced by the same key as its loaded twin (a player
by its source `SourceKey {k_PlayerType, number}`, through `PlayerObject`), so that both name one object.

A load keeps what the next save needs: `Game::LoadSavedGame` gives the save service (`KeepLoadedWorld`) the world it
read and the objects `saved_world_load::Apply` made of it (each entity by its number in the file and its record's
class: the footpaths and links, the rivers, the mists, the towns, the trees, big forests, piles, storage pits, abodes,
creches, graveyards, desire flags, the citadels' hearts and each citadel on its first heart, the arenas); a land that is not
loaded keeps nothing (`ForgetLoadedWorld`, as a new land is prepared). The forests and the climates are not entities:
their areas add them as sources' objects. **Checked on the frozen Land 1 autosave** (test
`SavedWorldWrite.TheFrozenLandOneAutosaveWritesBackByteForByte`, gated on the game's data): loaded with makers that
keep each entity's record as the game's do, then written back with only the game object's writer (from the values the
load puts back), every entity the load made from its kept record and every other object from the kept world, the file
is the original's byte for byte, the loads' counter 0 as the game's save leaves it.

**The arenas** (`src/ECS/SavedWorld/WriteArenas.cpp`, a source of class 0x41 and the game's list 30, `GArena::Save`
0x4250D0, [the arena](#the-game-objects-global-lists)): a land's arena, which the land
script makes (`components::Arena`, with the script's map units and its order) or a load makes from each arena of the
saved list (`MakeArenaEntity`, oldest first), is named `LandArenaId(entity)`; a creature fight's arena, kept on the
fight of the creature that made it, is named `FightArenaId(maker)`, and a creature's record names its arena by
`ArenaKeyOf(registry, creature)` (the maker only, as the original's +0x10A0). The list is the newest first: the
fights' arenas by the order they were made, then the land's. A land's arena is written at its map units, with its
place's flags and radius, and the rest as its load kept it (1.0, no fight, no fighters and +0x44 0 for one the script
made, as the constructor leaves it: openblack's fights never take a land's arena); a fight's arena at the two
creatures' middle in map units with height 0, 1.0, its radius, a fight on, the maker and the opponent, and +0x44 0,
which nothing in play changes (above); the arena found and
shrunk for a fight, and the free-place search, are [the fights' pending](../creature/fights.md#pending). Tests
`ArenaWriteTest.*`, with the frozen Land 1 autosave's three arenas written back byte for byte.

**The global lists of one kind of entity** (`src/ECS/SavedWorld/WriteGameLists.cpp`, `RegisterGameListWriters`). Every
list adds a new object at its head (`LHLinkedList::Add` and `LHListHead::AddToFirst` both link it first), so a list
made at creation runs newest first; `savetool --lists` shows it on every save read: in all 18 (the 16 frozen ones and
the original's `land1_new_answer3` and `land2_resumed_save60`) each list whose objects carry a creation number runs
strictly newest first by it, but for the villagers without a town (list 11), which a villager joins as it loses its
town. A new game's list is written from the entities of its kind, newest first by `ObjectCreationIndex`; a loaded
game's keeps its loaded order, without the objects gone since, behind those made since (all newer); an entity with
no creation number stops the write. Written so: the street lanterns, the rewards, the balls, the livings (villagers,
animals and creatures), the fields, the fish farms, the fireballs, the map shields, the scripts' highlights, the
animated statics, the trees, the big forests, the town centres, the whales (openblack's sharks), the fragments and the
puzzle games (16; a puzzle game is no object and keeps no creation number: one made since the load is placed by the
order the scripts made them, [the puzzle games](#the-puzzle-games)). The
street lights (list 0), the waterfalls (29) and the game's list of objects (35) are empty in every save read (both
the original's new-game saves included) and openblack makes none of their objects: empty, or as loaded. The
fireflies' list (15) names each firefly, the list's head first; each is written (2026-11-11,
`RegisterFireFlyWriters`, `FireFly::Save` 0x52B870) as a source keyed by its creation number: a map object (position
flags 1) of the firefly's row (2090) at where it is, its object part where it was made; its state (+0x5C), its home,
light and last turn's place as raw map coordinates (+0x60, +0x6C, +0x78), its drawn point (+0x84), its clock, drift,
progress, flight's length and two speeds (+0x90..+0xA4), its six phases (+0xA8..+0xBC) and its flags (+0xC0); then
the land's reward sums and weights (42 each) and whether the next nightfall tops them up, which every firefly writes
(test `SavedWorldWriteGameLists.TheFirefliesListNamesEachAndEachIsWrittenAsTheOriginalsRecord`). Before, a save with
any firefly refused. The load (2026-11-11, `FireFly::Load` 0x52BBC0, the same fields) makes each firefly the list
names, the list's head first, each at the list's tail as `GlobalGameLists::Load` puts them
(`saved_world_load::SavedFireFlyOf`, `ecs::RestoreFireFly`), kept by its creation number for the writer. At the
resolve its creation (`FireFly::CallVirtualFunctionsForCreation` 0x52A510) sets its home and last turn's place to
where it is and makes its sprite: the firefly's resolve is the object's (its vtable slot +0xF8 is
`Object::ResolveLoad` 0x63A020, xref's vtslots), which first calls vt+0x658, the firefly's creation, with its
position. The reward table and the top-up flag are the last firefly read's (each one's read
sets them). Test `SavedWorldLoad.TheFrozenLandTwoAutosavesFirefliesAreReadAndWrittenBackAsTheirRecords` (the 43 of the
frozen Land 2 exit autosave, read and written back field for field). The towns'
artifacts (41; one in the 18 saves, a Land 1 instant save's) are the `TownArtifact` things `Town::AddArtifact`
0x73FDE0 and `WorshipSite::AddArtifact` 0x77DB3B make, each added at the head of the list (0x425DF0); openblack keeps
them on the artefact's object (`TownArtefact`, its entity standing for both): every artefact is in the list, in a town,
a site or neither (the original's stays on its object, +0x54, until `ToBeDeleted` 0x425E70), newest first by the order
each was made (`TownArtefact::made`), in front of the loaded list. The footpaths, the towns and the footpath finders, which
are not objects and have no creation number, are added at the head as they are made too (bw1-decomp `Footpath.cpp`
`GFootpath` 191, `Town.cpp` 151, `Footpath.cpp` 1372): written newest first by the stamp each kind's making keeps
(`Footpath::creationStamp`, `Town::creationStamp`, `FootpathFinder::creationStamp`). A footpath finder (0x5,
`GFootpathFinder::Save` 0x539150, `WriteFootpaths.cpp`, test
`SavedWorldWriteFootpaths.AFootpathFinderIsWrittenWithItsEndsFootpathAndNodes`; 2026-11-11) is the thing's part, then
From, To, its footpath and its StartNode and DestNode, the footpath's head and last node as `GFootpathFinder::GFootpathFinder`
0x538EC0 sets them (bw1-decomp `Footpath.cpp` 1360: to's arrive place added, then from's, at the head); its planner and
plan are not saved (the load's `Init` sets the plan up again). openblack keeps the two nodes by their ids
(`FootpathFinder::startNode`, `destNode`); a thing gone is written as none. Before, a save while a worship site's
footpath was being planned was refused, the finder being of no class. The banks of mist (list 19) the
same: `Mist::Mist` 0x606270 links each at the head of the game's list of mist, and openblack stamps each as
`MistArchetype::Create` makes it (`Mist::creationStamp`). The streams (list 26) too: the map script's CREATE_STREAM (0x717538, its
constructor 0x733AC0) links each at the head of the game's list of streams; openblack stamps each as CREATE_STREAM or a
load makes it (`Stream::creationStamp`). The villagers without a town (list 11), which a villager joins at the head as it loses its town
(the map script's CREATE_VILLAGER with no abode or town, a child's birth `ChildBorn` 0x762220, `Villager::TownDeleted`
0x750B50, `ReleaseFromScript` 0x7531D0; `MakeHomelessNoStateChange` 0x761240 takes it out), are written as openblack's town state keeps them (`town_villagers::Vagrants`, the head first),
then the loaded list's villagers still without a town, which a load does not put in that list. The objects of several map
cells (list 33) join the head of the game's list of them in `MultiMapFixed::MultiMapFixed` 0x52E1E0, whatever their
class (its makers: the abodes, which take in the fields, the stores, the creches, the graveyards and the town centres,
the citadels' pieces, the big forests, the mobile statics with the rocks, dead trees and bonfires, the fish farms, the
features, the animated statics, the totem statues, the spell icons, the flowers, the football pitches and the worship
site upgrades; the original's new Land 1 save holds 344 of 16 classes there): written newest first by their creation
numbers from the entities with `Abode`, `CitadelHeart`, `WorshipSite`, `WorshipTotem`, `BigForest`, `MobileStatic`,
`DeadTree`, `FishFarm`, `Feature`, `AnimatedStatic`, `VillageTotem`, `SpellIcon` or `Flowers`. An object a list names whose class has no writer yet
still stops the write, naming it (tests `SavedWorldWriteGameLists.*`).

**The pots and piles** (`src/ECS/SavedWorld/WritePots.cpp`, `RegisterPotWriters`): a pot entity is of the class its
load's record is (a food or a wood pile), else of the class its row's making picks (`Pot::Create` 0x66CF10: a plain pot
0x58, a food pile 0x59 or the magic food 0x1E for row 10, a wood pile 0x5A or the magic wood 0x1F for row 9). Each is
written with the moving object's parts (its object naming its row, 1876 less the row; as made: carrying nothing, on no
path, its x and z angles as it holds them; a pile on the land at the height its sink is going to, as `SetSize` leaves
`Pos.altitude`), the row's resource, `Pot::town`, the amount and the flags (`gamesave::pot_flags`: the poison, the
reaction started, the building site's mark and the speed-up from the pot, the other bits as kept); a pile then its
structure (`resource_stores::StructureOfPile`), the player it answered with as it was made (as kept for a loaded pile,
else `Pot::playerWhenMade`: its store's or its worship site's player as it was made, a magic pile's maker, else the
neutral player), `reactsWhenMade` and its sink's 48 bytes (`saved_zoomer::BytesOf`); a food pile and the magic food their
speed-up's spot visual (`SpotVisualObject`; a loaded pile's is made again by the load), the magic piles
their owner. Tests `PotWrite.*` (`test/test_saved_world_write_pots.cpp`), each record read back as written.
The game's lists of food piles (20) and of mobile objects (10) are the pots made since the load, the newest first by
their creation numbers (each pot's making puts it at its list's head: `PileFood::PileFood` 0x66E070, `MobileObject`'s
constructor through `LHLinkedList::Add`), then each list as the load kept it, less what has gone since; the food
piles' list holds the food piles and the magic food, the mobile objects' the orbs and the scaffolds too. The mobile
objects of other classes (toys) stay as the load kept them until their writers come (the footballs have theirs, [the footballs](#the-footballs)); the Hanoi puzzle's blocks
made since the load join them by their creation numbers ([the puzzle games](#the-puzzle-games)). Not written: the puzzle grain (openblack has no puzzle grain
and the reader no record of class 0x107).

**Adding a class writer** (one area's branch, one commit per class group):

1. Write it in the area's own file, `src/ECS/SavedWorld/Write<Area>.cpp`, beside the load's `saved_world_load` pair of
   the class, as `void Register<Area>Writers(ClassWriters& writers)`, declared in `src/ECS/SavedWorld/GameWriters.h`,
   and add its call to `GameWriters()` in `GameWriters.cpp` (one line each).
2. For each class: `writers.Register(Record::k_Type, claims, write)`. `claims(registry, entity)` says whether an entity
   is of the class (from its components; exactly one class may claim an entity). `write(context, entity)` returns the
   whole `gamesave::SavedRecord`, its fields in the original's order (read the exe's `Save` of the class, never guess),
   each value it models from the entity and every field whose meaning is not read kept from
   `context.KeptRecordOf(entity, Record::k_Type)` (zero for an object made in play, as its constructor leaves it).
3. A class whose objects are not entities (the forests, the climates) registers a source instead:
   `writers.RegisterSource(Record::k_Type, keys, write)`. `keys(registry)` lists the objects' ids (unique in the class,
   e.g. a forest's key, a climate's id) in the game's order of the class; `write(context, id)` returns the record of
   one. They are named by `context.Reference(SourceKey {.type = Record::k_Type, .id = id})` and listed by
   `context.Keys(Record::k_Type)`, numbered in the same walk as the entities. A class has one entity writer or one
   source, never both. A record written inside another (a climate's natural storms) needs no key. A part of an
   entity that is an object of its own in the file (a footpath's node) is a source's object whose id is
   `PartId(owner, index)` (the owner in the high 32 bits; `PartOf(id)` gives back the owner and the index), and its
   writer finds its loaded record by `context.KeptRecordOf(SourceKey)` (none for one made since the load).
4. A class that the game keeps in a global list registers that list's writer too, in the same file.
5. A reference to another object is always `context.Reference(entity)` (or `context.Reference(entity, type)` for an
   entity that stands for more than one object of the file, such as a citadel's first heart and its citadel): a
   writer never makes an object's number. The record's `ThingPart::checksum` need not be set: the stream writes its own
   sum. A raw pointer the record keeps that is not a reference (a river node's word of the game's memory) is written
   as the load kept it, and 0 for an object made in play, as the original's new objects have it.
6. A loaded object keeps its number of the loaded file: the area's load adds each object it makes to
   `WorldSource::loaded` (number → `LoadedObject {.entity, .type}`, or `{.type, .id}` for a source's object), so that
   the kept records that name it and its writer agree. With no writer the loaded object is written from the record
   its load kept (`LoadedRecord<>` for an entity, the kept world for a source's object).
7. Its test, in `test/test_saved_world_write.cpp` or the area's test file: the class's record written from an entity
   made the way the load makes it equals the record the load read (a synthetic record, and with the game's data the
   frozen Land 1 autosave's records of the class), and the whole file written from the loaded game still equals the
   original's file byte for byte.
**What is written from the entities** (each tested record by record against the read and, with the game's data, by
the frozen Land 1 autosave written whole from its loaded footpaths, byte for byte; tests `SavedWorldWriteFootpaths.*`):

- **The footpaths, their nodes and the footpath links** (`src/ECS/SavedWorld/WriteFootpaths.cpp`), each record in the
  order its class's `Save` writes it (Footpath.cpp, [the footpaths](#the-citadel-the-footpaths-and-the-town)): a
  footpath (a `Footpath` entity) its nodes from the head, and whether it is in use (the saved word stays while it
  says the same); a node, a source's object that is a part of its footpath (`RegisterSource`, its id
  `PartId(footpath, node id)`, the nodes listed in their footpaths' list order, each footpath's from its head), its
  map place and its flags from the footpath's node, and its followers from its record in the kept world
  (`context.KeptRecordOf(SourceKey)`), as the load does not put them on the node, without those gone since
  (2026-11-11: `Villager::DeleteDependancys` 0x74FD60 takes a villager off its node's followers unless it walks the
  path; test `SavedWorldWriteFootpaths.EachFootpathNodeAndLinkTheLoadMadeIsWrittenAsItsRecordWasRead`; before, a save
  after such a follower died was refused, villager 1050834 of node 388 on `_J_a_h_o_v_i_a` 14/1) (a node made in play
  writes the livings on it); a link (a `FootpathLink` entity) the footpaths it lists, in its order. Each one's
  thing part (its extra word, destroyed word and flags) comes from the record its load kept, and is zero for one made
  in play. A footpath link kept with a building's place (0x4) is not loaded and is written from the kept world.
- **The abodes, the creches, the graveyards and the storage pits**
  (`src/ECS/SavedWorld/WriteAbodes.cpp`; tests `Data/AbodeWriteWorld.*`: each record as the load read it, synthetic
  and the frozen Land 1 autosave's, and that file written whole with these writers byte for byte, the piles as their
  loads kept them), each in the order
  its class's `Save` writes it ([the storage pit and the piles](#the-citadel-the-footpaths-and-the-town), [the town's
  buildings](#the-towns-buildings)). An abode (0x7), a creche (0x50), a graveyard (0x51) and a storage pit (0x8): the
  parts of a building of several cells (the shared parts, its row from `Abode::info`), then its town (`Abode::townId`),
  its food and its wood; the villagers from its record without those gone since, and the adults, those at home and
  the children as it holds them (2026-11-11: the load puts them back and a death takes the villager out, as
  `Abode::RemoveDeletedVillagerFromAbode` 0x404220 from `Villager::DeleteDependancys` removes it from `villagers` and
  lowers `AdultCount` or `ChildCount`; test `Data/AbodeWriteWorld.ALoadedAbodeIsWrittenWithoutTheVillagersGoneSinceItsLoad`)
  (an abode made in play writes those it holds); a loaded field's farmers likewise without those gone since (test
  `Data/AbodeWriteWorld.ALoadedFieldIsWrittenWithoutTheFarmersGoneSinceItsLoad`: `Villager::ExitFarming` removes its
  farmer, run by the dying villager's SET_DYING); its flags and drinking water from its record, and
  for one made in play from what it holds (`Abode::AbodeFlags`, Abode.h: bit 0x1 drinking water found from the
  `DrinkingWater` search, 0x2 counted by its town's statistics, 0x4 not to be planned again, 0x10 a villager of it in
  a hand; the water in map units with no height, as in every record the original's new Land 1 save holds); its fixed
  flags from `Abode::buildFlags` (0x8 built, as the original's Land 1 fields and town centres have it); the word after
  the water, the byte after the counts (an index, 11 to 33 in the original's Land 1 save, not read yet: 0 for one made
  in play) and the destruction mesh from its record (openblack keeps no destruction mesh to write). **A field** (0x32,
  `Field::Save` 0x529B10): the abode's record, its kind's row (`WriteInfo(+0x120)`: 1765 less the kind, Land 1's kinds
  0 and 2 being rows 1765 and 1763 in the original's save), +0xC8 (only `Field::Load` writes it: as kept, else 0), its
  food (+0xDC, `Field::food`), its town (+0x118), its turn offset (+0x11C), its crops sown (+0xCC), its growth (+0xD0),
  its farmers (newest first) and a byte of +0xE0 (only the load writes it: as kept, else 0). A graveyard adds its dead (`Graveyard::dead`), a storage pit its food pile as an array
  of one and its five wood piles as an array of five (`StoragePit`). An entity is of the class its load kept the
  record of; one made in play of the class its row's kind makes (the abode maker's switch: a plain abode for every
  kind it does not hand to a class of its own). The storage pits' wood piles (0x5A) and food piles (0x59) are the
  pots' and piles' writer's (below). Not yet tested: the frozen Land 1 autosave written whole with the pots' writer
  and these together, byte for byte.
- **The towns' desire flags** (0x4C, `src/ECS/SavedWorld/WriteTownDesireFlags.cpp`; tests `Data/DesireFlagWriteWorld.*`:
  each record as the load read it, synthetic and the frozen Land 1 autosave's 42, that file written whole byte for
  byte, and a flag made in play), in [their `Save`'s order](#the-animals-and-the-towns-desire-flags): the place, its
  altitude the flag's height over the ground (`TownDesireFlag::overGround`); the object's values with the first prayer
  site's row (1856, the row `TownDesireFlags::Create` makes every flag with: `GPrayerSiteInfo::Infos[0]`, and the
  number all 42 flags of the frozen autosave carry), where it was made as its target place (`madeAt`) and its own
  scale (`objectScale`, the Transform drawing it at 1); its town; its slot (`point` less the first flag's point, 12);
  the desire it flies for (`storedDesire`); its desire; its mesh, the one its load kept or else its desire's row's
  worship site mesh, as the flag is made with.
- **The citadels' hearts** (0x3A, `src/ECS/SavedWorld/WriteCitadelHearts.cpp`; tests `Data/HeartWriteWorld.*`: the
  heart of a profile's Land 1 save with its temple built, written as read, and that file written whole byte for byte):
  the parts of a building of several cells with the heart's row (2411); the turn of the last blow that harmed it
  (`CitadelHeart::breakingBlowTurn`); its destruction: started and its clock (`Temple::destroying`, `destructionClock`),
  its plasma beam and its glow (`destructionBeamSource`, `destructionGlow`), the next plasma's time
  (`destructionBeamClock`); its draw colour, its fade in the top byte over white (`Temple::fadeAlpha`); its beam's
  source, target and turn (`beamSource`, `beamTarget`, `beamTurn`). Its influence (the info's constant, never written
  after the heart is made), its citadel, the word after it (never set), its town, its alignment flock, its specular
  colour (only the renderer holds it, frame by frame) and its entrance come from the record its load kept. The load
  puts the values openblack holds back on the heart (`MakeCitadelHeartEntity`); the beams' objects are not made again.
  A heart made in play is written with those values as a new heart starts them: its part's influence (the heart row's
  `influence`, 125, set by `CitadelPart::CitadelPart` 0x4693F0 from the row), its citadel (`CitadelHeart::citadel`), the
  word after it 0, its town (`CitadelHeart::town`), its alignment flock (`flock`), its specular colour 0 and its
  entrance; its place's altitude 0. All four citadels of the original's own saves (the new Land 1 game, the Land 2
  game: 0 in every heart's specular colour, the word and the altitude) bear these out (test
  `Data/HeartWriteWorld.TheOriginalsNewGameCitadelHoldsWhatANewHeartStartsWith`). Open (2026-11-11): a loaded heart's
  alignment flock (`CitadelHeart::AlignmentFlock`, a raw `Flock*`) is written as kept; only the heart's own
  `RemoveAlignmentFlock` clears it in what was read, so whether a flock deleted another way leaves it named is not
  determined.
- **The citadels** (0x35, the same file), named only through their first heart (`context.Reference(heart,
  CitadelRecord::k_Type)`; the writer claims no entity): a loaded one as its load kept it; one made in play in
  [`Citadel::Save`'s order](#the-citadel-the-footpaths-and-the-town): at its heart's place with altitude 0 and no place
  flags, the citadel's row (2412), its player; its heart; its six worship sites by slot (`CitadelWorship::sites`); its
  pieces, the list +0x4C that `CitadelPart::CitadelPart` 0x4693F0 adds each piece to at its head (its hearts, worship
  sites and their altars, newest first by creation number: the Land 2 save's citadels hold altar, site, heart and
  altar, site, altar, site, heart); the list +0x54, which nothing fills; the list +0x5C, its planned pieces
  (`PlannedCitadelPart::PlannedCitadelPart` 0x469610), which openblack does not make; the list +0x64, which nothing
  outside the citadel fills (all three empty in all four citadels of the original's saves); +0x6C its influence
  (`CitadelInfluence::reach`, set by `CitadelHeart::CitadelHeart` 0x4649B0: 750 in the new Land 1 save, 450 on Land 2);
  +0x70 the worship strain's sound fraction (`CitadelWorship::strainSoundFraction`, `SetWorshipStrainSoundFrac`
  0x463850); +0x74 whether it may not make worship sites (`CitadelWorship::cannotCreateSites`; read by
  `FindOrCreateWorshipSite` 0x4631D0). Tests `Data/HeartWriteWorld.AHeartMadeInPlayIsWrittenWithItsCitadelAndItsEntrance`.
- **The citadels' entrances** (0x110, the same file; an entity with `CitadelEntrance`): only `Object::Save`'s parts, as
  the heart makes it: its heart's place with altitude 0 and the flag of counting as fixed (0x8000), no row
  (0xFFFFFFFF), its place as its target, the creation number after its heart's (openblack makes it without one: the
  number the heart's making skips), whole, at scale 1, turned 0, no fire and no body; all three entrances of the
  original's saves so. A loaded heart's entrance stays the kept world's record, which the heart's kept record names.
  The load's heart maker makes the entrance with the heart (`CitadelArchetype::CreateHeart`); since 2026-11-10 the load
  also lists it in the loaded table under the number the heart's record names (`CitadelHeartRecord::entrance`), so
  the scripts' table and the next save find it by that number, and the load's census counts it (test
  `Data/HeartWriteWorld.ALandOneSavesHeartIsWrittenAsReadAndTheFileAsItWas`).
- **The worship sites, their altars, what the altars show and the need signs** (0x3C, 0x98, 0x3E and 0x3D,
  `src/ECS/SavedWorld/WriteWorshipSites.cpp`; tests `Data/WorshipWriteWorld.*`). A site, an altar or a sign the load
  made is written as its load kept it ([the worship sites](#the-worship-sites-and-the-dances)), a site without the
  worshippers gone since and its count `NumWorshippers` down by one for each (2026-11-11: `WorshipSite::RemoveVillagerFromWorshipCount`
  0x77D0A0 from `Villager::RemoveVillagerFromWorshipSite` 0x76C440, which `Town::RemoveVillager` calls), its dance and its
  totem none once gone (`LHPTR`s; test `Data/WorshipWriteWorld.ALoadedSiteIsWrittenWithoutTheWorshippersGoneSinceItsLoad`;
  its food pot, a raw pointer, and its villagers asking to go home stay as kept, as nothing seen takes them out), a site with the spell icons
  it holds now, head first: `WorshipSpellIcon::ToBeDeleted` 0x77F230 takes an icon out of its site's list
  (`WorshipSite::RemoveSpellIcon` 0x77C450) and the save writes the list as it is (`WriteIt` +0xE4, then each icon), so
  an icon deleted since the load is not named (test `Data/WorshipWriteWorld.ALoadedSiteNamesOnlyTheSpellIconsItHoldsNow`;
  the user's Land 2 autosave was refused for the 19 icons its sites lost after the load); one made in play from the entity, as
  `WorshipSite::Save` 0x77D2F0, `WorshipTotem::Save` 0x780F90, `ShowNeeds::Save` 0x719CB0 and `ShowNeedsVisuals::Save`
  0x71A230 write it. The site (`WorshipSite`): a citadel part of its tribe's site row (2405 down by the tribe) at its
  place with altitude 0, the row's `influence` (`CitadelPart::CitadelPart` 0x4693F0), its citadel (through its heart,
  `CitadelRecord::k_Type`) and the word after it 0; its tribe's row (742 down); what its altar shows (named through the
  altar, `ShowNeedsRecord::k_Type`: no entity of its own); its dance, towns and food pot; +0xBC, the turn its dance was
  last checked (only `ValidateDance` 0x77B9A0 sets it, which nothing calls), +0xC0 and the byte +0xC4 (only the save and
  load touch them) and +0xCC (only `SetToZero` 0x77AE60 clears it) 0, as the zero-filled new site has them; the
  villagers at the site (`villagersAtSite`) and their list; its spell icons; the food on its way then the wood (never
  added: 0); its six chants, the two cheats, its slot, `iconTookChants`, those asking to go home, its altar, its strain,
  its mana path (the particles' interface of `manaPathEffect`) and the strain's pulse. The cheat's next spell seed
  (+0xD0, moved only by the developer's `Citadel::Cheat` 0x463510) is 0. The altar (`WorshipTotem`): a citadel part of
  its site's row (`WorshipTotem::WorshipTotem` passes the site's info) at its own place with altitude 0, the same
  influence and citadel, its specular colour 0 (set only by the heart's destruction and the heal chakra's effect through
  `SetSpecularColor`'s slot, which openblack does not hold) and its site. What the altar shows: the altar and its three
  signs in their order. A sign (`ShowNeedsVisuals`): `Object::Save`'s parts of its row (1855 down: hunger, rest, people
  hiding, a workshop's wood) at its owner's place and height (an altar's 0, a workshop's its own: the Land 2 save's workshop signs keep their workshops' heights) and only the flag of counting as fixed, its place as
  its target, whole, at scale 1, turned 0; +0x54 0 and its own scale +0x58 1.0
  (`ShowNeedsVisuals::CallVirtualFunctionsForCreation` 0x719E00), what it belongs to (an altar's sign the altar's
  record of needs, a workshop's the workshop) and +0x5C its desire. All four sites of the original's Land 2 save, their
  altars and their twelve signs bear these out (`Data/WorshipWriteWorld.TheOriginalsLandTwoSitesHoldWhatANewSiteStartsWith`).
- **The scripts' timers** (0x7D, `src/ECS/SavedWorld/WriteMarkers.cpp`, as `ScriptTimer::Save` 0x711700 writes one:
  a thing with a place, then the turn it was set on and the turns it was set for): no game list holds a timer, only
  the scripts' object table names it. One made in play is a thing with a hidden name (0x40) at no place, with its
  script's marks (in a script, controlled by it), then `ScriptTimer::timer`'s `setTurn` and `turns`; one a load made is
  written as kept, with how it is set now. The load makes each (`MakeScriptTimerEntity`, in the order the reads ended)
  set as saved, with its marks and its record, so the scripts' object table names it again. Every timer of the
  `_J_a_h_o_v_i_a` profile's Land 1 save (16/1) is so (tests `Data/ForestWriteWorld.AScriptTimerIsMadeAndWrittenBackAsReadAndOneMadeInPlayFromItsState`,
  `Data/ForestWriteWorld.TheProfilesLandOneTimersHaveNoPlaceAndTheirScriptsMarks`).
- **The workshops** (0x52, `src/ECS/SavedWorld/WriteAbodes.cpp`; test `Data/AbodeWriteWorld.AWorkshopMadeInPlayIsWrittenFromItsState`), as
  `Workshop::Save` 0x779F40 writes it: the abode's record; +0xD0 its wood pile (`Workshop::woodPile`, set by
  `CreatePileWood`), +0xD4 its sign of the wood it wants (`showNeeds`); +0xC4 the turns left for the scaffold being made
  (`countdown`), +0xC8 the scaffolds it owns (`owned`, the count `GetSpaceInStore` 0x779AF0 takes off); its three
  scaffold slots; its scaffolds, the head of its list first, each by its number (one holding a scaffold was refused
  until the scaffolds' writer came, 2026-11-10). Every workshop of the original's Land 2 save names a magic wood and a sign of the workshop's
  wood row at the workshop's place that belongs to the workshop
  (`Data/WorshipWriteWorld.TheOriginalsLandTwoWorkshopsNameTheirPileAndTheirSign`).
  The load (2026-11-11, `src/ECS/SavedWorld/LoadWorkshops.cpp`; test
  `Data/AbodeWriteWorld.ASavedWorkshopIsMadeWithItsSignAndWrittenBackAsRead`), as `Workshop::Load` 0x77A130 reads it:
  the abode's load makes the workshop of its row without the sign and the wood pile a new one makes
  (`AbodeArchetype::Create`'s `ownParts` false); then its turns left (+0xC4), the scaffolds it owns (+0xC8) and its
  slots; once the piles and the scaffolds are made, its wood pile and its sign as its record names them, the sign made
  from its own record (0x3D: made as the game makes one at its saved place, for the workshop, of the row it names, then
  its creation number, its desire and its record as saved), and its scaffolds in its record's order, a null one skipped
  as the load's `ReadPtr` loop skips it; and each town's workshop list from the town's record (before, a loaded town's
  list was empty). Before, a loaded game made no workshop (29 in 8 of the original's saves).
- **The spell dispensers** (0x10B, `src/ECS/SavedWorld/WriteAbodes.cpp`; test
  `Data/AbodeWriteWorld.ASpellDispenserMadeInPlayIsWrittenFromItsState`, 2026-11-10), as `SpellDispenser::Save`
  0x722D50 writes one (bw1-decomp SpellDispenser.cpp:39): the abode's record; +0xCC the orb it made (`SpellDispenser::orb`,
  0 once it is gone), then the raw words +0xC4 the turns counted (`timer.tick`), +0xC8 the period (`timer.period`),
  +0xD0 whether it is active (`timer.active`) and +0xD4 its miracle (`magicType`); the fields as
  [the dispenser's turn](../magic/worship.md#the-dispenser-and-its-turn) reads them. Before, no writer claimed a
  dispenser, so a quick save of a new Land 3 game (which has one) was refused. Its load (`SpellDispenser::Load`
  0x722E80, bw1-decomp SpellDispenser.cpp:25: the abode's, then +0xCC by `ReadPtr` and the four words; no resolve of
  its own, so the object's resolve runs its `CallVirtualFunctionsForCreation` 0x7227D0, which starts its effect 0x90 at
  the land under it unless the dispenser is not available, bit 0 of its thing flags at +0xA: one in a hand or held by a
  script) makes it as an abode of its row (`MakeAbodeEntity`) with the four
  values (`worship::dispenser::Restore`, its effect as the creation starts it) and gives it its orb once the orbs are
  made (test `Data/AbodeWriteWorld.ASavedSpellDispenserIsMadeWithItsValuesAndWrittenBack`); the effect only for one
  saved available, and in play only for an available one (test
  `Data/AbodeWriteWorld.ASavedSpellDispenserStartsItsEffectOnlyWhenItWasSavedAvailable`). Before, a loaded dispenser
  was not made, nor the fire on it, which left the original's Land 1 save in slot 14 of `_J_a_h_o_v_i_a` with a fire
  group the load could not keep whole.
- **The scaffolds** (0x5F, `src/ECS/SavedWorld/WritePots.cpp`; test
  `PotWrite.AScaffoldMadeInPlayIsWrittenWithItsTwoPlansAndListed`), as `Scaffold::Save` 0x6EA240 writes one: a moving
  object's parts of the scaffold's row (1938 in every scaffold of the original's saves: the game's one
  `GScaffoldInfo`), carrying (+0x58) its workshop (`Scaffold::owner`); +0x68 its building site (`site`), +0x6C its plan
  and +0x70 its candidate (`plan`, `candidate`), +0x78 the cross-fade's timer (`fade`), +0x7C its holder (`holder`),
  +0x80 and +0x84 the phantom's angle and scale (`phantomAngle`, `phantomScale`), +0x88 its flags (`flags`), +0x8C its
  town (`town`) and +0x90 the only building it may offer (`limit`, -1 for any). Its two plans are its own planned
  abodes (0x61), made by `Scaffold::Create` (bw1-decomp Scaffold.cpp:59, `PlannedAbode::CreateNoInit(coords, NULL,
  NULL, 0.0f, 1.0f)`) in no town's list: each is written nested in the scaffold, at its place (`ScaffoldPlan::position`),
  of no row (-1) unless it offers a building, with its angle and scale, the turn it was made (the
  `PlannedMultiMapFixed` constructor's +0x3C, kept now as `ScaffoldPlan::creationTurn`), never built, on no footpath and
  in no town. The planned abodes' source tells a scaffold's plan (`PartId(scaffold, 0 or 1)`) from a town's.
  `Scaffold::TryToBuildPlannedBuilding` 0x6E9080 sets both pointers to null once the scaffold joins a site
  (`SetPlannedAbode(0)`, `SecondPlanned = 0`), so a site's scaffold names no plan, as every one of the `_J_a_h_o_v_i_a`
  profile's Land 2 save (20) does: six on sites, six in workshops with their two plans each (savetool `--records`). A
  plan deleted while still named is refused: the original's pointer would name an object that is gone. The scaffolds
  are in the game's list of moving objects (10), the newest first with the others; the plain moving objects' writer no
  longer claims them.
  The load (2026-11-10, `MakeMobileObjectEntity`; tests
  `Data/AbodeWriteWorld.ASavedScaffoldIsMadeBeforeItsSiteWithItsPlansAndWrittenBackAsRead`,
  `Data/AbodeWriteWorld.TheFrozenLandTwoAutosavesScaffoldsAreMadeAndWrittenAsRead`) makes each scaffold with the moving
  objects, before the building sites, as `Scaffold::Load` 0x6EA3D0 reads it: made as a new one is
  (`scaffolds::Create`), then its plan and candidate from their records (place, angle, scale, whether available, turn
  made, the building offered), the timer, the phantom's angle and scale, the flags and the building it may offer as
  saved, and the placed turn 1 (Load's last line). Once the sites are made it takes its site, workshop, town and
  holder as its record names them, then its phantom as `Scaffold::ResolveLoad` 0x6EA560 sets it: unless the object is
  unavailable (GameThing flags bit 0), its site's building's mesh (`SetPhantomMeshAndPosition` 0x6E8890), else, with a
  plan offering a building, that building's. A load makes no workshop yet, so a loaded workshop's scaffold has no owner
  and is written naming the workshop its record names, which the load kept.
- **The town centres' spell icons' creation numbers**: the original's icon takes the counter's number as it is made
  (`Object::Object`); openblack counted those numbers for the town (`object_index::OnTownCentre`, `AddTownSpell`) and
  now gives each icon, as it is made, the next of its town's (`object_index::AssignTownIcon`), so the game's list of
  icons is ordered by them (test `ObjectCreationCounter.*`). An icon made once its town's counted numbers are all taken
  (a town given a magic in play, by a reward or a challenge's script, through `Town::AddMagicTypesHeld` →
  `TownCentre::AddSpell` 0x744050, or one made since a load) takes the counter's next number as it is made (2026-11-11,
  test `ObjectCreationCounter.AnIconBeyondItsTownsCountedOnesTakesTheCountersNextNumber`); before, it had none and every
  save after it was refused ("has no order of its making", the game's list 33 of objects of several cells).
- **A loaded town's buildings destroyed since its load** (2026-11-11, `WriteTown`, test
  `Data/TownWriteWorld.ALoadedTownIsWrittenWithoutTheBuildingsDestroyedSinceItsLoad`): its abodes, building sites and
  workshops are written in their saved order without them, and a creche or graveyard gone as the one the town holds
  now, as the original's building leaves its town as it is deleted (`Abode::ToBeDeleted` 0x402C60 →
  `Town::RemoveStructureFromTown` 0x739A60: `AbodeList.Remove`, an unbuilt one's building sites deleted,
  `RemoveWorkshop`; `Creche::DeleteDependancys`: the town's creche becomes another functional one, or none). Before,
  the town wrote the lists its load kept, and a save after a blaze burnt a town's buildings was refused (town 31 of
  `_J_a_h_o_v_i_a` 14/1: 2 building sites, a creche, a spell dispenser and an abode named while gone). Its animals, spell icons, artifacts, flocks and fish farms
  gone since have left their lists too (2026-11-11, test `Data/TownWriteWorld.ALoadedTownIsWrittenWithoutItsOtherThingsGoneSinceItsLoad`:
  `Animal::DeleteDependancys` → `AnimalList.Remove`, `Town::RemoveSpellIcon`, `Town::RemoveArtifact`, a flock off
  `FlockList`, `FishFarm::DeleteDependancys` off the town's farms; its fields list stays as kept, as no field unlink
  of its own was found). A main storage pit or town centre gone since is written as the one the town holds now:
  `StoragePit::DeleteDependancys` 0x732CD0 and `TownCentre::DeleteDependancys` 0x743BE0 (vt +0x910, called by their
  `ToBeDeleted`) hand the town's link (+0x9A4 for the centre) to another functional one of its `AbodeList`, or none;
  a worship site gone since is none (`WorshipSite::ToBeDeleted` 0x77AA60 sets each of its towns' `worship_site` to
  null); its football pitch's link (+0xEA4) stays as kept, as `Football::DeleteDependancys` 0x531430 leaves it; its
  villagers and missionaries lists stay as kept (no unlink found: `Town::RemoveVillagerFromList968` has
  no callers); its playthings are written as it holds them since the load puts them back (2026-11-14,
  [the footballs](#the-footballs)): a ball leaving its town takes itself off the list (0x436050, called from
  0x435B10, → `Town::RemovePlaything` 0x73B800), and a football taken since the load is in it. Its villagers gone since have left it
  too (2026-11-11, test `Data/TownWriteWorld.ALoadedTownIsWrittenWithoutTheVillagersGoneSinceItsLoad`:
  `Villager::DeleteDependancys` 0x74FD60 → `Town::RemoveVillager` 0x73E210, out of `HomelessList` and of
  `VillagersOnWayToWorshipSite`, whose count `NumVillagersOnWayToWorshipSite` goes down with each; before, villagers
  1050824 and 1050827 of town 31 on `_J_a_h_o_v_i_a` 14/1 were named while gone). Its plans are
  written as they stand (2026-11-11), so the rebuild plan a destroyed building leaves (`MoveAbodeToPlannedAbodes`
  0x404520, openblack `abodes::MoveAbodeToPlannedAbodes` → `plans::CreateFromBuilding`) is in the save and a plan built
  since is not; each loaded plan is kept by its town and its place in its list, written from its record with the
  values it holds now, its place in whole map units as saved while it stands there (test
  `Data/TownWriteWorld.ALoadedTownGetsItsPlansBack`). Before, a loaded town wrote the plans its load kept.
- **The towns' planned citadel hearts** (0x39, `src/ECS/SavedWorld/WriteTowns.cpp`, as `PlannedTownCitadelHeart::Save`
  0x467FC0 writes one: a plan's parts and its town): of the citadel heart's row (2411; the game's tables hold one heart
  row, and a plan of another is refused), like the other plans (test `Data/TownWriteWorld.APlannedCitadelHeartIsWrittenOfTheHeartsRow`).
  The planned heart of the frozen `_f_f` profile's Land 1 save is of that row
  (`Data/TownWriteWorld.TheFrozenLandOnePlannedCitadelHeartIsOfTheHeartsRow`).
- **The towns** (0x28, `src/ECS/SavedWorld/WriteTowns.cpp`; tests `Data/TownWriteWorld.*`: a synthetic town written as
  read, the frozen Land 1 autosave's towns written as read and that file written whole byte for byte, a town made in
  play, and the original's own new Land 1 save's six towns holding what a new town starts with), in
  [`Town::Save`'s order](#the-citadel-the-footpaths-and-the-town). A loaded town is written from the record its load
  kept, with the values the load puts back (`MakeTownEntity`) as they stand: the belief in the neutral player,
  whether it may be lived in, its area's two corners (their altitudes as kept), its belief block after its two words
  (as kept), the balance belief scale, its figures block (its two words and the counts openblack does not keep as
  kept, the rest as the town holds it, not recounted from its buildings as a town made in play's is), the magic types
  it holds, the countdown to its being empty and its two temporary pots (one deleted since is not named; test
  `Data/TownWriteWorld.ALoadedTownWritesItsTemporaryPotsAsTheyStand`). A town made in play is written from
  its components; a value openblack does not hold is the one a new town starts with (`Town::Town` 0x739350: its memory
  cleared by `Base::operator new` 0x4366F0, then `SetToZero`, the belief's and the desire's `Init`, and its own
  stores):
  - the container's part: its place and flags, the town's row (2081, the only row; all six towns of the original's
    new Land 1 save) and its owner; the main storage pit (`Town::storagePit`);
  - the desires (`TownDesire`, the 17 values of each array): +0x90 `boostA`, +0xD4 `boost`, +0x118 `desire`, the town,
    +0x164 `population`, `raw`, +0x1AC `amount`, +0x1F0 `desired`, the two sorts (`sorted`, `sortedRaw`; the first
    word the float's bits), `alignmentTurns`, `doingNowAtStart`, `doingNowCountAtStart`, `doingNow`, `doingNowCount`;
    the base, the modifier, the change and the last process turn are 0 (no writer in the exe; 0 in all six towns);
  - its id (`town_queries::ScriptIdOf`), tribe, player number; worship percentage, count and those on the way
    (`TownMagic`); its influence (`TownInfluence::radius`, the value `Town::UpdateInfluence` 0x73E1F0 stores); the
    belief in the neutral player; BuildingRequested = `requestedPlanThisTurn` (cleared at the town's process,
    0x747390); DesireDirty and DesireDirtyTimer = `buildPulse` and `buildPulsePrevious` (set by `AddBuildingSite`
    0x73B96D and `VillagerDead` 0x73E497, stepped by the process at 0x747530); CannotBuildWorshipSite =
    `forbidWorshipSite`; Uninhabitable; the two temporary pots; the forests (the forests' service's town list, head
    first); the area; the creche, the graveyard, the workshops, the abodes (`town_stats::AbodesOf`), the homeless, the
    spell icons, the fields, the fish farms, the building sites, the playthings, the animals, the worship site, the
    town centre, the villagers on the way to worship, the magic held, the football pitch; the last attacker and its
    turn, the two aggressor scales (`protectionMultiplier`, `mercyMultiplier`) and the two interaction totals (`mercy`
    the owner's, `protection` the others'), the worship percentage kept through an emergency; the resources' last
    taking by player; the flocks (the original adds each at the head: openblack's list reversed); the desire flags by
    desire; the missionaries; the balance belief scale (`TownBelief::beliefScale`); EmptyTownTimer = `emptyCountdown`;
  - the figures block (0x118 bytes): the word 0x99A00C, Base's 0, then `TownStats` (bw1-decomp TownStats.h; its 8
    players and 10 death causes proved by `TownStats::VillagerDead` 0x749780, which adds to +0xA4 + player x 4 and
    +0x7C + cause x 4): the adults, the children, NumAbodes (`abodesWithPlaces`), NumCivicBuildings,
    NumPlannedCivicBuildings (`civicPlans`), MaxVillagersInAbodes (`adultPlaces`), RecentDeaths, MaxChildrenInAbodes
    (`childPlaces`), VillagerSpaceLeftInAbodes (`freeAdultPlaces`), the males and females, the deaths (`TownDeaths`),
    the disciples, FoodReqiredForDinner, FoodUsed (`Town::foodUsed`), WoodUsed (`woodUsedForBuilding`), TotalFood and
    TotalWood (`foodCarried`, `woodCarried`), TotalBuildingSiteCost (`woodAtSites`), NumWounded (`Town::injured`),
    NumAbodesOfNumber; the counts as they stand (`town_stats::AsTheyStand`);
  - the belief block (0x1D0 bytes): the word 0x999FEC, Base's 0, then `TownBelief` in its order;
  - each player's dealings (`PlayerTownInteract`, 0x80 bytes each): the aggression and the last attack's turn
    (`town_aggression::Record`), the trust 1.0, no damage done, the last attack's block (the word 0x8D8BA8, Base's 0,
    its burn, crush and hit), the turn of the last artefact belief gift (`TownArtefactGifts`).

  Its plans (`Town::plannedAbodes`, in their list's order) are a source each (`PartId(town, index)`): a planned abode
  (0x61), a planned town centre (0x62), in `PlannedAbode::Save` 0x405830's order (`PlannedMultiMapFixed::Save`
  0x648990, then the town): where it is to stand in whole map units with height 0 and no place flags, its angle
  (`yAngleRadians`), its scale, whether it rebuilds a building (`wasBuilt`), the word +0x34 (0), the turn it was planned
  (`creationTurn`), its footpath link, its abode row (`AbodeInfoRowOf`) and its town; the original's own new Land 1
  save's six plans have no flags, no height and the word 0 (test
  `Data/TownWriteWorld.TheOriginalsNewGamePlansHoldWhatThePlansWriterGives`). A planned citadel heart (0x39) stops the
  write (its row from the script's heart info is not read yet), and a town's artifacts are those that joined it, the newest
  first (`TownArtefact::joined`). **The towns' artifacts** (0x103, `src/ECS/SavedWorld/WriteTownArtifacts.cpp`) in
  `TownArtifact::Save` 0x4265A0's order: the object (+0x14), its town (+0x18, kept after the artefact leaves it, cleared
  only once the town goes, `TownArtifact::Process` 0x425FB0), its player (+0x24), its worth (+0x28, `value`), its
  dance (+0x2C), its magic effect (+0x38), whether its dance started (+0x30, `dancing`) and the turn it did (+0x34,
  `danceStarted`); its object's fixed part names it (`FixedPartOf`). The site it joined (+0x1C) is not saved. A load
  makes it again (2026-11-13; `TownArtifact::Load`, bw1-decomp Artifact.cpp:26, reads the same; raffclar's tree has no
  artefact load): planned from the game's list of artifacts, the newest first, with whether its town's list holds it
  and its places in that list and in the game's (`WorldPlan::artifacts`); once its object, its town and its dance are
  made, it goes back on its object with its town, owner, worth, dance and its dance's start, its site none
  (`ArtefactSystemInterface::Restore`); once the containers are made it takes its glow's container again, and
  `TownArtifact::ResolveLoad` 0x4267E0's `CreateParticleEffectIfNecessary` 0x425F20 is done (`ResolveLoad`): a glow and
  the owner's symbol only when it has no glow and is worth it, so one that loads its glow has no symbol, as the
  original's (test `SavedWorldLoad.ATownsArtifactIsPlannedAndPutBackOnItsObject`). Before, a loaded artefact was not
  made (the frozen 14/1's 0 of 1). **Not exact** for a town made in play (openblack does not hold
  these; each is written as a new town starts with it): the camera view (0; `Town::SetTownArea` 0x73AAF0 sets it);
  AverageGameTurnsToTravelToWorshipSite (0; `SetAverageGameTurnsToTravelToWorshipSite` 0x73AA50, nonzero once the
  player has a citadel); Raining (0; the climate at the town's making); the extra villagers (none); NearestTown (none;
  `UpdateNearestTowns` 0x73AEE0); in the figures block NumPlannedBuildings, NumBuildingSites, NumCivicBuildingSites,
  NumPlannedWonders, NumWonderSites, TotalSpaceLeftInAbodes, NumChildrenAdded, NumAbodesAdded, NumWonders,
  ChildSpaceLeftInAbodes, LastAbodeAddedTurn, AverageFoodUsed and AverageWoodUsed (0), and FoodUsed and WoodUsed past
  a birthday (`TownStats::ProcessBirthday` 0x749800 clears them; openblack's never clear); TotalFood and TotalWood,
  which the original moves only as a villager joins or leaves, against openblack's sums as they stand; in each
  player's dealings field_0x0, field_0xc, the trust (`ReduceTrust`, `Process`), the damage done
  (`UpdateDamageDoneToVillager`), and the last attack's other kinds, scale, applier, place and player; the area of a
  town that never had an abode (openblack's empty area is 0x7FFFFFFF and 0, the original's 0 and 0). Exact though not
  held: Promiscuity (no writer), and the town's own villager list (its adder has no caller; none in the six towns).
- **The creature's drinking waypoints and the scripts' markers** (0xA1 and 0x7C, `src/ECS/SavedWorld/WriteMarkers.cpp`,
  tests `SavedWorldWriteMarkers.*`), each only `GameThingWithPos::Save`'s place. A waypoint (`DrinkWaypoint`) at the
  place the land's script gave CREATE_DRINK_WAYPOINT (`CreateDrinkWayPoint` 0x770BC0: `GSetup::GetScriptPos` 0x718250,
  whole map units, height 0), no place flags; the game's list of waypoints (list 27) takes each at its head, written
  newest first by the order the land made them (`DrinkWaypoint::number`). A marker (`ScriptMarker`, made only by a
  script: `ScriptMarker::Create` 0x70D8D0 at `MapCoords(LHPoint)` 0x603160, the height over the land) where it
  stands, its place's marks from its components (`PositionFlagsOf`: held by a script and under its control, 0x600) and
  the thing's 0x40, its name hidden (`GAME_THING_FLAG_NAME_HIDDEN`, GameThing.h), which `GScript::AddScriptGameThing`
  0x70D0F0 sets as the script takes the new marker; the game's list of things with a place (list 25, `game_thing_with_pos`, which only markers join) takes
  each at its head, written newest first by `ScriptMarker::creationStamp`. The original's own new Land 1 save bears
  these out: its 47 waypoints with no height and no flags, its 7 markers with 0x40 and 0x600 (test
  `SavedWorldWriteMarkers.TheOriginalsNewGameWaypointsAndMarkersHoldWhatTheWritersGive`). The load makes the markers
  again (`MakeScriptMarkerEntity`) and, since 2026-11-10, the waypoints (`MakeWaypointEntity`): the original's load
  reads each waypoint's place alone and puts the game's list of waypoints back as saved, so openblack makes them from
  that list's last to its first, each `DrinkWaypoint` numbered after the ones made so far (the order the land made
  them, which the creature's search for water reads) and keeping its record, which the writer writes back as read
  (nothing moves a waypoint). Before it a loaded game had no waypoints, so a creature that wanted a drink went to the
  coast instead (Land 1's quick save: 47). Test `SavedWorldWriteMarkers.ASavedWaypointIsMadeInItsOrderAndWrittenBackAsRead`.
- **The town centres and their totem statues** (0x60 and 0x7F, `src/ECS/SavedWorld/WriteAbodes.cpp`, test
  `Data/AbodeWriteWorld.ATownCentreAndItsTotemMadeInPlayAreWrittenFromTheirState`). A town centre (`TownCentre::Save`
  0x744830): the abode's record, its totem statue (+0xCC, the `VillageTotem` plinth whose town centre it is) and its six
  spell icons by slot (`WritePtrArray(+0xD0, 6)`, `TownCentreIcons`). A totem statue (`TotemStatue::Save` 0x738700): the
  parts of a building of several cells of its tribe's row (378 less the tribe: the original's new Land 1 save has the
  Norse town's at 371 and the Aztec's at 376), built (0x8, `VillageTotem::percentBuilt`), its town centre (+0x7C, the
  constructor's), +0x80 the worship share it rises to (`SetWorshipPercentage` 0x738270: `TotemWorship::percentage`),
  +0x84 (only the load writes it: as kept, else 0) and +0x88 the share the hand holds it at (the locked select
  0x7385E0 / 0x7386A0: `VillageTotem::held`).
  The town centre's load (2026-11-10, `MakeAbodeEntity`, test
  `Data/AbodeWriteWorld.ASavedTownCentreIsMadeWithoutItsTotemAndIconsAndWrittenBackAsRead`): `TownCentre::Load`
  0x744880 reads the abode, the totem statue's pointer and the six icons' pointers; its resolve
  (`TownCentre::ResolveLoad` 0x7448C0) is `Object::ResolveLoad` (`TownCentre::CallVirtualFunctionsForCreation`
  0x743CF0: the abode's, then its 3D objects), then `CreatePSys` 0x69BC10 when it is functional. Nothing there makes a
  totem, an icon or a worship site: each has its own record. openblack makes the centre through the abode maker
  without `AbodeArchetype::MakeTownCentreFunctional` (`townCentreParts` false), makes its belief symbols' effect
  (`town_belief::MakeEffect`, the local particle stream) when it is functional, and gives each town the centre its
  record names (`TownRecord::townCentre`). Before it a loaded game had no town centres (Land 1's quick save: 2).
  The totem statue's load (2026-11-10, `MakeTotemStatueEntity`, test
  `Data/AbodeWriteWorld.ASavedTotemStatueIsMadeOnItsTownCentreAndWrittenBackAsRead`): `TotemStatue::Load` 0x738800
  reads the MultiMapFixed parts, its town centre, +0x80 into the share's Zoomer as its current, start and destination
  values (no speed, no time: it stands there), +0x84 and +0x88; its resolve (`TotemStatue::ResolveLoad` 0x738940) is
  `Object::ResolveLoad` (`TotemStatue::CallVirtualFunctionsForCreation` 0x737D60: its 3D object at Pos with the land's
  altitude under it, its angle and scale, the map cells from its special point 0, then +0x88 (the hand's share), +0x90
  and +0x94 set to the Zoomer's current value and +0xD4 to 0, no random number), then `SetPlayersCreature`. Its rise is
  drawn on top of Pos (`TotemStatue::Draw` 0x738960: the current value × 8), so Pos is the plinth at rest. openblack
  makes the plinth and its icon (`VillageTotemArchetype::CreateAt`, the in-play maker's own parts, given to no player)
  at Pos, of the tribe its row names (378 less the tribe), of the town centre it names (which has it as its totem when
  the centre's record names it), built as saved, `TotemWorship` at the saved share and standing, and the hand's share
  (`VillageTotem::held`) the same; `VillageTotemSystem::WorldLoaded`, after the load, gives each totem its player's
  creature's icon. A save written after a load so writes +0x88 as +0x80, as the original's would. Its MultiMapFixed
  parts' footpath link comes back with it as every building's does (the load's `giveLink`; test
  `Data/AbodeWriteWorld.ASavedTotemStatueKeepsItsFootpathLink`); before, the loaded totem had none, so the next save
  left its link out (a Land 1 quick save's 59 links written back as 57, the Norse and Aztec totems' two).
  The original's totem is in the map's cells (0x8003 in the frozen Land 1 autosave's two: in the map, of several
  cells, counted as fixed); openblack's, made in play or by the load, is not (no `InsertMapObject`), so its place's
  flags are written without the first bit: a difference of the in-play maker, still to fix as its own commit.
  The town centre's spell icons' load (2026-11-10, `MakeTownCentreSpellIconEntity`, test
  `Data/AbodeWriteWorld.ASavedTownCentresSpellIconsAreMadeInTheirSlotsAndWrittenBackAsRead`):
  `TownCentreSpellIcon::Load` 0x7490E0 reads `TownSpellIcon::Load` 0x748FB0 (`SpellIcon::Load` 0x727A00: the
  MultiMapFixed parts, the seed's row, SpellCoords and SpecularColor; then the town), its town centre, the count and
  `PowerUpEnabled`; its resolve (`TownCentreSpellIcon::CallVirtualFunctionsForCreation` 0x748D20) makes its 3D object at
  Pos with its angle and scale, its seed's graphic (`SpellIcon::Create3DSpellObject` 0x726210: 1 m above its special
  point 0, of its player's seed) and the graphic's power-up band (`UpdateGraphicWithPULevels`). Nothing there adds it
  to its town's list (the town's record lists its icons) or makes a worship site's icon (the site's records hold
  them). openblack makes each at its saved place, angle and size, of the seed its row names (2578 less the seed), its
  town and its town centre, in the slot the centre's record names it in (`TownCentreIcons`), with its power-up levels
  and its graphic showing the highest; its player its town's; each town's `TownMagic::spellIcons` in its record's order
  (`TownRecord::spellIcons`, the newest first). An icon whose record names no town centre (all 28 of the frozen slot-20 Land 2
  autosave's: the saved `Centre` is 0) is put in the slot its centre's record names it in, as `TownCentre::Load` 0x744880
  reads the centre's six icons, still naming no centre (`TownCentreSpellIcon::ToBeDeleted` 0x748C80 only takes an icon
  with a centre out of its array); before, such a centre's slots stayed empty (test
  `Data/AbodeWriteWorld.ATownCentresIconsThatNameNoCentreAreInTheSlotsItsRecordNames`).
- **The game statistics** (0x9F, `src/ECS/SavedWorld/WriteGameStats.cpp`, test
  `SavedWorldWriteGameLists.APlayersStatisticsAreItsFiguresInTheGamesOrder`): a source of the players' numbers, as the
  players name them; a new game's each its player's number as its own word, its player, then the figures in
  `GameStats::Save`'s order from `game_stats::Serialize`; a loaded player's as the load kept them.
- **A singing stone's base** (0xF6, `GBaseOnly`, `src/ECS/SavedWorld/WriteMobileStatics.cpp`, test
  `Data/ForestWriteWorld.ASingingStonesBaseIsWrittenAsAnObjectOfNoOtherClass`): a script's CHL CREATE of mobile static
  row 6 makes a `GBaseOnly` (`GBaseOnly::Create` 0x609340: `Object::Object(coords, info)` alone), whose record is
  `Object::Save`'s parts only: its row, its place with the height the script gave, its target where it was made, and
  the thing's flags (`GAME_THING_FLAGS`: 0x1 unavailable, 0x40 its name hidden, from `Unavailable` and `NameHidden`).
- **The features, the animated statics and the scripts' highlights** (0x74, 0x75 and 0x3F,
  `src/ECS/SavedWorld/WriteLandFeatures.cpp`, test
  `Data/ForestWriteWorld.AFeatureAnAnimatedStaticAndAHighlightMadeInPlayAreWrittenFromTheirState`). Their rows count down
  from 2064 (features), 1986 (animated statics) and 2088 (highlights), as the original's new Land 1 save has its Aztec
  statues, lime pillars, Norse gate, gate stone plinth, piper's caves and did-you-know signs. A feature: the parts of a
  building of several cells, made whole (0x8), built as far as a script set it. An animated static adds its three gate
  stones' slots (`AnimatedStatic::Save` 0x422AA0: a count of 3, then each) and +0x7C, the open state
  (`SetOpenState` 0x422CC0). Each slot is the `MESH_LIST` number of the stone's model laid in it, 0 when empty
  (`AddGateStone` 0x422D50 sets it, `CountGateStones` 0x422DF0 reads it), so a plinth holding gate stones writes their
  models (2026-11-11; before, it refused the save). A highlight
  (`ScriptHighlight::Save` 0x70A760): `Fixed::Save` with the thing's mark 0x40 every one of the original's carries,
  then +0x5C hidden (only the load writes it), +0x60 active (`SetActivated`), +0x7C and +0x80 whether a script set its
  height and that height (`SetDrawHeight` 0x709C40), +0x78 the script id and +0x84 the did-you-know category
  (`SetScriptId` 0x709A20).
- **The fish farms** (0x5B, `src/ECS/SavedWorld/WriteFishFarms.cpp`, test
  `Data/AbodeWriteWorld.AFishFarmMadeInPlayIsWrittenFromItsState`): the parts of a building of several cells (row 1759
  less the farm's info, the row of all 13 in the original's new Land 1 save; fixed flags 0x8 and built 1.0 as
  `FishFarm::FishFarm` 0x52C360 makes it), its fishermen (newest first), its town, +0x90 (1.0 from the constructor,
  nothing else writes it) and its stock +0x94 (`FishFarm::food`, which `FishFarm::Process` 0x52D130 changes).
  A loaded farm writes the fishermen its record listed without those gone since (2026-11-11, test
  `Data/AbodeWriteWorld.ASavedFishFarmIsMadeAndWrittenBackAsRead`: `Villager::ExitFishing` → `FishFarm::RemoveFisherman`,
  run by the SET_DYING of `Villager::DeleteDependancys` 0x74FD60).
- **The street lanterns** (0xFE, `src/ECS/SavedWorld/WriteStreetLanterns.cpp`, tests `SavedWorldWriteStreetLanterns`):
  `Object::Save`'s parts of the row it was made with (`StreetLantern::info`; the target its own place, as
  `Object::Object` sets it), +0x58 (`GStreetLantern::Create` 0x7346E0: 0 for the street lantern's row 7, 1 for any
  other, so `StreetLantern::country`; the original's new Land 1 save: 8 of row 1930 with 0, 4 of row 1878 with 1), and
  the byte of whether the lanterns are heard (`g_lanternSoundsActive`, `lantern_sounds::IsOn`), no thing flags. The
  load (2026-11-10, `MakeStreetLanternEntity`, test `StreetLanternWriteTest.ASavedLanternIsMadeAgainAndWrittenBackAsRead`)
  makes each lantern that stands on the map again as `GStreetLantern::Load` 0x734BE0 and its creation do: `Object::Load`,
  +0x58 from the file (`StreetLanternArchetype::CreateLoaded`: country as saved, with no look for another mobile static
  within 0.5 m, which only `Create` makes), the byte into `g_lanternSoundsActive` (`lantern_sounds::SetOn`, the last
  lantern read holds), then `CallVirtualFunctionsForCreation` 0x734810's mesh on the land under its place, its light
  (`EclairagePublic::Create` with +0x58) and its sound tag; its creation number, life and record kept, which the writer
  reads. `GameOSFile::ResolveAllLoads` (bw1-decomp `GameOSFile.cpp:1403`) resolves the loaded things from the list's
  last node, the first read (`Add` puts each at the head), so the original makes each lantern's light, with its six
  C runtime draws, in the file's order among the other objects' resolves; openblack makes the lanterns after the
  features. The C runtime's stream is not saved, so that order changes nothing a file holds or a game turn reads
  (the lights' flicker only). Before it a loaded game had no lanterns (Land 1's quick save: 12).
- **The music attached to things** (0x109, `src/ECS/SavedWorld/WriteThingMusic.cpp`, 2026-11-11, test
  `SavedWorldWrite.TheMusicAttachedToThingsIsWrittenInTheAudiosListHeadFirst`): the audio's list in the game's tail
  (`GAudio::Save` 0x428310's `WriteSafe(ThingMusicList)`: its count, then each info), head first, each by
  `ThingMusicInfo::Save` 0x429950 (bw1-decomp AudioMusicThing.cpp:278): `GameThing::Save`'s part (as the load kept it,
  else the class's own), the music type, the thing, Enabled, Finished, Started, UseFixedPos and FixedPos, the map
  coordinates `SET_MUSIC_PLAY_POSITION` 0x70FE60 makes of its point (`MapCoords(LHPoint)`, kept as they are in
  `ThingMusicInfo::playCoords`). An info never given one writes 12 zero bytes, as the original's do: the constructor
  `ThingMusicInfo(type, thing)` sets no FixedPos, and every such record the saves hold has it 0 (all 13, UseFixedPos 0:
  the 11 of the frozen profiles' saves, `_J_a_h_o_v_i_a` 14/1, 15/2 two, 16/1, 16/2 two, 17/1, 20 two, `_s_i_s_i` 16/1
  and 20, and the 2 of `_gamedata/original_saves`, read with `savetool --fields` and as raw bytes, 2026-11-11). A loaded
  info writes its record's bytes back. Each info has a key (`ThingMusicInfo::key`): a loaded one its number in the file, kept by the load
  (test `SavedWorldLoad.TheMusicAttachedToThingsKeepsItsNumbersForTheNextSave`), so a save names it as the loaded
  object it was; a new one the next of the list's own. Without the game's music (no `game_music::Get()`, the tests')
  the list is written as the load kept it. Before, a save wrote the list the load kept, without the music attached
  since.
- **The rest of the audio's part of the tail** (2026-11-11, `GAudio::Save` 0x428310 after its list, bw1-decomp
  Audio.cpp:1117; test `SavedWorldWrite.TheScriptMusicItsTownAndTheAlignmentGoWhereTheAudiosLoadReadsThem`): the
  script music's type (+0x28) and whether it plays (+0x180) in `audioWords`, the town the music last played for
  (+0x18C, `WritePtr`) in `audioObject` by its number (0 for none; openblack's own choice of 0 for a town destroyed
  since, which the original's pointer would not be told apart from), and the alignment (+0x190) in `audioWord`, as the
  alignment music reads it (`GameMusic::GetCameraAlignment`, the value `GPlayer::ProcessPlayers` writes there each
  turn), from `saved_game_save::CurrentValues`' `music` (`HeadOf` puts the words, the game object's writer the town).
  Without the game's music they stay as the load kept them. Before, they were always the loaded record's (0 for a
  new game).
- **The dead and felled trees and the bonfires** (0x70, 0x71 and 0x72, `src/ECS/SavedWorld/WriteMobileStatics.cpp`,
  test `Data/ForestWriteWorld.ADeadTreeAFelledTreeAndABonfireMadeInPlayAreWrittenFromTheirState`), each a rock's
  record (`Rock::Save`: the mobile static's parts, the two words a new rock is made with, 0 and -1.0, and its player):
  a dead tree (`DeadTree`, a felled one with `FelledTree`) of the dead tree's mobile static row (`DeadTree::DeadTree`
  0x510880 makes its rock of `GMobileStaticInfo::Infos` +0x384, row 3), then the row of the tree it was (`WriteInfo`
  +0x94) and its wood multiplier (+0x9C, `woodValueMultiplier`, 1.0 from `SetToZero` 0x510A80); a bonfire (a
  `MobileStatic` of the bonfire's row, `Bonfire::Bonfire` 0x4395C0: +0x960, row 8) then +0x94, which nothing writes (0).
- **The teleports' stones** (0x19 `MagicTeleport::Save` 0x5FBFA0, `src/ECS/SavedWorld/WriteMobileStatics.cpp`, test
  `Data/ForestWriteWorld.ATeleportsStoneMadeInPlayIsWrittenWithItsReactionAndPlayer`; 2026-11-11): the mobile static's
  parts of the teleport's row (`MagicTeleport::MagicTeleport` 0x5FC130 makes its `MobileStatic` of
  `GMobileStaticInfo::Infos` +0xF3C, row 13, owner 0 at +0x7C), made whole as every mobile static is, at a hundredth of
  its scale (`MagicTeleport::Create` 0x5FC1F0: `SetScale(GetScale()·0.01)`; openblack keeps the stone at 1.0, as it has
  no mesh), then its reaction (+0x94, none once it has gone), its spell (+0x9C) and its player (+0xA0). The travellers
  (+0x8C) and its pool's particles (+0x98) are not saved. The stones join the game's list of objects of several cells
  (list 33) by their creation numbers, and each player's teleports list (`GPlayer` +0xA58, the newest at its head) names
  its stones, the newest first (`saved_players::ObjectsOf`), then those its loaded list names that the load did not
  make, as read (one made and gone since leaves the list). Before, a save with a teleport's stone up was refused: its
  stone was of no class, and so were the spell and the reaction that name it. The load makes the stones again
  (2026-11-14, `saved_spells::PlanTeleportStones` / `MakeTeleportStones` / `LinkTeleportStones`,
  `magic::teleport::Restore`; test `SpellLoad.TeleportStonesAndTheirSpellAreMadeAgainInTheirOrderAndWrittenBackAsRead`):
  `MagicTeleport::Load` 0x5FBF50 (bw1-decomp MagicTeleport.cpp:87) reads the mobile static's parts, then its reaction,
  its spell and its player by `ReadPtr`; `GameOSFile::LoadInstance` makes it with `MagicTeleport::MagicTeleport`
  0x5FBFF0, which clears its link, its travellers (+0x8C), its reaction, its spell, its player and its pool (+0x98,
  fn_005FC1E0); its resolve is `Object::ResolveLoad` 0x63A020 (MagicTeleport's vtable +0xF8), whose first call is
  `MagicTeleport::CallVirtualFunctionsForCreation` 0x5FC260: the mobile static's creation (into its cells) and its pool
  started again (`PSysInterface::Create` of PT 73 at the world position, with the stone's player); no reaction is made
  there (only `MagicTeleport::Create` makes one). So a loaded stone is planned from list 33 with the player its record
  names and its place in that player's list (the player's record, `GPlayer` +0xA58), made from each list's last to its
  first so that `StonesOf` keeps the list's order, at its place, X, Y and Z angles and creation number, in its cells, with
  its pool started again and no travellers, its record kept (its scale written as read); once the spells are made it
  takes the spell its record names, and once the reactions are made the reaction it spreads, by the id that reaction was
  made again with (the reaction names it as its initiator). raffclar's tree has no stone load.
- **The building sites** (a standard site 0x5C, a workshop's 0x5E; `src/ECS/SavedWorld/WriteBuildingSites.cpp`, tests
  `SavedWorldWriteBuildingSites.*`), in `BuildingSite::Save` 0x43C830's order: the building (`BuildingSite::root`), the
  builders and the scaffolds, the head first; the list +0x28 and the word +0x30, which only the constructors write
  (empty and 0); the ring of 128 building positions as their x, y and z in metres (`ring`); +0x634 the builders' count
  (`builderCount`, `AddBuilder` 0x43BE40 and `RemoveBuilder`); +0x638 whether it repairs (`isRepairSite`, the
  constructor 0x43B7E0 takes it from the building's flags); +0x63C the forced desire (`desireBoost`); +0x640 the life
  the repair is measured by (`repairBase`); a standard site then its wood pile (`StandardBuildingSite::Save`); a
  citadel's site (0x5D, `CitadelBuildingSite::GetSaveType` 0x43D190; `CitadelBuildingSite::Save` 0x43D5E0) then its six
  wood piles as an array of references (`WritePtrArray(+0x644, 6)`, `CitadelBuildingSite::piles` by slot), a record
  the reader now reads too. The game's list of building sites
  (list 32) takes each at its head as the constructor makes it: written newest first by `BuildingSite::creationStamp`,
  which `building_sites::Create` gives.
  The load (2026-11-10, `MakeBuildingSiteEntity`, test
  `SavedWorldWriteBuildingSites.ASavedSiteIsMadeOnItsBuildingAndWrittenBackAsRead`) makes each standard site again as
  `StandardBuildingSite::Load` 0x43D890 and `BuildingSite::Load` 0x43CAD0 read it: the building, the builders' list
  (each added at the end, in the file's order), the scaffolds' list (`fn_0043DCB0`, at the end too), the second list
  of builders, the word, the 128 positions read as they are, the builders' count, the repair flag, the forced desire,
  the life, then the wood pile; there is no `ResolveLoad` of its own, so only the pointers come back, and nothing
  computes the ring again. openblack makes it once its building and pile are made, links the building back to it
  (`Abode::buildingSite`, or `CitadelPartBuild::buildingSite`) and redraws the building partly built, gives it its
  builders once the villagers are made, and puts the town's sites back in the order the town's record lists them
  (`TownRecord::buildingSites`). Its scaffolds (2026-11-10), made already as moving objects, go in its list at the
  end in the file's order; the second list and the word are not read: a site holding either logs one line. A builder's
  site, which its own record names, is found again now that the site is made. Before it a loaded game had no building
  sites, so its unfinished buildings had none and their builders none to go to.
  The workshops' and the citadels' sites (2026-11-10, same maker, test
  `SavedWorldWriteBuildingSites.ASavedWorkshopsAndCitadelsSitesAreMadeAndWrittenBackAsRead`): a workshop's site has no
  load of its own (`BuildingSite::Load` alone, and no wood pile: `WorkshopBuildingSite::GetPileWood` is the
  workshop's), a citadel's reads `BuildingSite::Load` then its six piles (`CitadelBuildingSite::Load` 0x43D620,
  `ReadPtrArray`); neither has a resolve of its own. openblack makes them as the standard site, a workshop's of
  `BuildingSite::Kind::Workshop` with no pile, a citadel's with `CitadelBuildingSite::piles` by their slots, all in the
  order their reads ended.

### The players' records

`GPlayer::Save` 0x64C4C0 and `GPlayer::Load` 0x64C8C0 (Player.cpp:1997 and 2036) write and read the same fields in the
same order (above, [the first classes](#the-first-classes)); `GPlayer::ResolveLoad` is empty. A new game's players are
set up by `GGame::SetupPlayers` 0x550410 (Game.cpp:1028): the neutral player as type 3 (`PLAYER_TYPE_NEUTRAL`), number 7,
named with text 0x1A6C ("PEPE NEUTRAL" in the frozen copy's InfoScript2.txt); the local player by `InitReal` as the
human (type 1) with the profile's name; each other player below 7 by `Init(PLAYER_TYPE_0, i, "Player[i + 1]", i)`
(the string 0xBEC8DC). `Init` makes each player's computer player, statistics and its interface in the interfaces'
place of its number. Every frozen and reference save (19) holds row 1743 as the players' info and type 3 for the
neutral player; players 1 and 2 are type 2 (computer) in the 4 Land 2 saves.

openblack (`src/ECS/SavedPlayers.h`, `src/ECS/SavedWorld/WritePlayers.cpp`):

- **The load** (`saved_players::Load`, from `Game::LoadSavedGame` after the world's objects): each player record the
  game object names (`ListPlayers`) puts back what openblack models of it (`ValuesOf`, `Apply`): the alignment and its
  change this turn, the tribal powers and their most, the type, the "all magic" cheat, the total population, how many
  holders enable each magic type and whether it was ever enabled, the influence power and its average, the harm from
  each player and the alliances (one side only: `alliances::Restore`). They go where openblack keeps them: the player's
  entity, else the player service. The rest stays in the record: the name, the user id, the cheat words, the start
  position, the computer player, the statistics, the creature, the teleports, the interfaces, HasLost and the last word.
  Each player is listed in the loaded objects as the players' source object of its number.
- **The writer** (`RegisterPlayerWriters`, a source of the players' class with the ids 0 to 7): each player from the
  record its load kept, or a new game's record (`NewRecord`, as `SetupPlayers` makes it: its number, the alignment's
  class word 0x8CC7F0, row 1743, tribal powers 1, its type and name, the lists of 9, 9, 32, 42, 42, 8, 8 and 18), with
  the values put where the save writes them (`RecordOf`); the citadel (the player's temple not marked for deletion)
  and the towns (in the order they joined the player's list) from the entities, and its teleports' stones, the newest
  first, in front of those its load kept ([the stones](#writing-the-world-file)); a loaded player's creature stays as
  its record names it (a load makes no creature yet), and one got since a load with none is named. A new game's
  computer player, statistics and interface are named as the objects of their classes of the player's number; the
  interface, the computer player and the statistics (`k_GameStatsType`, from `game_stats::Serialize`) have their
  writers ([below](#the-saves-in-play)). Any object that names a new game's player (`PlayerReference`: a town's
  artefact, a citadel, a town, a fire, a pot, a rock) names this source's object of its number, as the game object does, not the player's entity.
  Nobody's player (`PlayerNames::_COUNT`: a creature a script makes for no player, as Land 1's guide is, and the old
  body a creature swap leaves, and what such a creature makes) is named as no object (0), as the game's null player
  pointer is written (`GameOSFile::WritePtr` 0x561E10 writes 0 for null; test `SavedPlayers.NobodysPlayerIsNamedAsNoObject`).
  Both hold a null player, not the neutral one: the script's CREATE of a creature calls `Creature::Create(coords, info,
  NULL, ...)` (`GScript::CreateScriptThing` 0x6F11A0, bw1-decomp ScriptCreateOrCall.cpp:107-111), and
  `Creature::SwapMinds` 0x47BEE0 gives each creature the other's owner (Creature.cpp:3787-3788), so the body swapped
  with such a creature is left with none;
  before 2026-11-11 a new game's save was refused, "there is no player 8" (the user's Land 1 play of joint589). A game object written for a new game names the
  eight players of this source. Checked on the frozen Land 1 autosave: written with the players' writer from the
  values its load puts back, the file is still the original's, byte for byte (tests `SavedPlayers.*`,
  `SavedWorldWrite.TheFrozenLandOneAutosaveWritesBackByteForByte`).
- Differences found, not fixed here: openblack's neutral player has type 0 until a load sets it (the original's is 3
  from the start); a new game's user id, start position and the cheat words are written 0 (the original takes the
  user id from the network player, and its start position for a new game is not read **🔍**); a loaded computer
  player's mind is not switched on from its record.

### The saves in play

The original's callers of `InstantSaveGame` 0x792FB0 ([the slots](#the-slots-and-who-writes-them)), read in the C:

- **The autosave** (`GameOSFile::AutoSave` 0x564050, GameOSFile.cpp:1880): from `GGame::EndTurn` 0x54EA43 when the
  game flags have `GAME_FLAG_AUTO_SAVE_ENABLED` and the game is not over (+0x205A10, set as the GameOver script starts),
  before the game over check (Game.cpp:2521). It saves when no script holds the wide screen, outside a network game,
  off the Gods' Playground (land 6), and when forced or when `GameTurn - LastAutoSaveTurn > AutoSaveInterval` (6000,
  unsigned) with the game not paused. It sets `LastAutoSaveTurn` to the turn (and `AutoSaved`, which only the main
  menu's Continue reads, from `IsAutoSaveValid`), reads the profile's "CircleSlot(<land>)" (0 when there is none),
  saves into `count % 3 + 15` and writes the count + 1 back, whether the save was written or not. `LastAutoSaveTurn`
  is 0 as the program starts (`InitStaticsValues` 0x54A780), is kept from land to land while each land's turn starts
  at 0 again (`GData::Reset` 0x510750), so a new land autosaves at its first turn once an autosave was made, and a load
  sets it to the loaded turn (Game.cpp:2921). `GGame::Save` writes it, and +0x205A18 after it.
- **The quick save**: `GGame::DoAction` 0x63F940, action 0x1D: outside a network game, no script's wide screen, the
  game not over, and |+0x205A18 − turn| > 3 (signed): +0x205A18 = the turn, then `SetPacket(0x61)`; its packet saves
  into slot 19 (`GPacket::ProcessPacket` 0x63DC88). `GGame::ProcessTurn` (Game.cpp:2452) also sends it when the
  object heap runs low after an hour of play; the quick load (action 0x1E, packet 0x62) checks +0x205A1C, which a load
  sets to the turn ([the quick load](#the-quick-load)).
- **Slot 20**: `CitadelHeart::Built` (CitadelHeart.cpp:567) when the local player's temple is built, outside a
  skirmish and a network game; and `GGame::Loop` as its play loop ends (Game.cpp:1943): not once the game is over, in
  a network game, a skirmish or game mode 4, and not on land 6 (when quitting it first draws two black frames; when
  the profile was changed at the front end it saves into the previous profile).
- **The scripts**: `GScript::InstantSaveForScript` 0x6FF8E0 pops the slot as a number and truncates it (`_ftol`).
- **Slot 14**: `GScript::GameThingClicked` 0x70AEB0 (read in the v1.42 exe 2026-10-27), when the thing found clicked is
  a scroll (vt `IsScriptHighlight`), not a did-you-know, of the challenges 0x38, 0x3B, 0x3C or 0x3D (fn_0070AC50, its
  +0x78): the interface's last tap +0x460 / +0x464 = 0, `InstantSaveGame(14)` (0x70AF82), then `RememberTapped` with
  the scroll again. No network or skirmish test of its own (a multiplayer game pushes true before the pop).
- **The save game room**: `SaveGameRoom::SaveCurrentSlot` 0x792CA0 (inside the range the 1.20 symbols give
  `LoadGame`; called by `DialogBoxSaveMessage::ControlCallback` and the 14 / 18 swap 0x7936B0): for `CurrentSlot`, the
  name the player typed (0xE05F28) as the title, a new room entry for a slot without a save, the picture cells, the
  date, `SaveGameInfo`, `SaveAllGame`; a failed write drops the new entry; a written one adds 1 to the profile's saves
  (`PlayerProfile::Profile` +0x80C), rewrites the room's scroll and, for a new slot, starts
  "CitadelSaveGameRoomLoadHelp".

openblack (`src/ECS/SaveTriggers.h`; the rules pure, tests `SaveTriggers.*`):

- `save_triggers::AutoSave` as the original's, with `LastAutoSaveTurn` kept by the save service (0 as it is made, the
  loaded turn after a load) and the land's count of autosaves kept with the profile (`circle_slot` lines of the
  profiles' file, `PlayerProfileSystemInterface::CircleSlot`); the turn's step `AutoSave` before `GameOver`
  ([the turn](loop.md#the-turn-call-by-call)) asks for it when the profile's autosave option is on (openblack keeps the
  options' autosave box with the profile, not in the game flags) and the game is not over.
- The Quick Save key (Ctrl+S) as action 0x1D, through packet 0x61 (`game_packets::Type::QuickSave`) into slot 19; the
  last quick save's turn is kept by the save service, written into the game object's tail and put back by a load.
- Slot 20 when the temple is built (`worship::citadel`'s built heart) and as the game's frames end (the menu's Quit and
  Restart, the window closed).
- SAVE_GAME_IN_SLOT (`CHLApi.cpp`).
- Slot 14 from `GAME_THING_CLICKED` on such a challenge's scroll, between the click cleared and given again
  (`save_triggers::ChallengeScrollTapped`; test `SaveTriggersInPlay.AChallengesScrollTappedSavesIntoSlotFourteen`).
- `saved_slot::RoomSave(slot, name)`: the room's save with the name given, counted in the profile's statistics when
  written. The room is not ported, so only the debug bar reaches it.
- **Debug control**: Load Island > Saved Games > Save (only with a profile kept): Autosave now (forced), Quick save, and
  Into slot N (slots 0 to 13 by the room's save, the others by the instant save). Nothing saves until it is clicked.
- The Quick Load key (Ctrl+L) as action 0x1E, through packet 0x62 (`game_packets::Type::QuickLoad`): slot 19 loaded
  through `Game::LoadSavedGame` at the start of the frame after the turn that applied it ([the quick
  load](#the-quick-load)).
- With no profile kept (a run of the checks) no slot exists: `saved_slot::SaveSlot` makes nothing, so the checks write
  no file; the autosave still keeps its turn and steps the run profile's count in memory.
- **A save that fails** never stops nor throws out of the turn: `saved_slot::SaveSlot` returns how it ended
  (`SlotSave`: written, no slot, not made, the world's temporary file not opened, a file that failed) with the reason,
  and what it or the world's writers throw is a save not made. A world that cannot be made whole (a game not loaded,
  an object of a class with no writer and no kept record, e.g. a live spiritual shield or flock spell) writes nothing
  at all: the world is made before anything is opened or written, so there is no folder, picture cell, info or
  temporary file, and the slot's old save is untouched. That is not a difference: the game never meets a world it
  cannot write. Each save in play logs one line
  of how it ended. The game tells a save whose writing failed (`WriteEnabled` off) with its message box, whoever asked
  for the save: `SaveAllGame` calls `DoSaveProblemRequestor` 0x54CA90 with help text 0xD9C
  (HELP_TEXT_DIALOG_ERRORHASOCCURED, read through `HelpTextDatabase` +0xA350), which makes the game's own `SetupBox`
  (0xCD3AF0, made in Game.cpp's statics, also used by `DoOKGameRequestor`) the active box and shows
  `SetupBox::MessageBoxA` style 0, the text and one OK; its answer (0x54C8A0, event 3) clears `WantKey` and gives the
  active box back to the dialog shown before (fn_005136B0). The box does not pause the game. A temporary world file
  that does not open returns at once, with no message. openblack: `save_triggers::ToldOnScreen`; the event
  `events::SaveProblem` makes the game's interface show `GameInterface::ShowGameMessage` (the text over a 500 by 200
  box in the middle, the OK under it, as the menu's notices; it takes the mouse until the OK is clicked). The keys do not
  reach the game: with an active box `SetupBox::UpdateWantKey` 0x407EF0 sets `WantKey` (the game's box is no
  `DialogBoxBase`), and a message box holds its box (`SetOnHold` 0x4081A0), which clears `CanEscape`; `GGame::ProcessKey`
  then gives the key to `SetupBox::Key` 0x408F80 and takes it (0x63F0F0, the `WantKey` branch), Escape included.
  openblack gives the box every key pressed (Escape does nothing), before the temple's and the cave's Escape. A save openblack could not make, which the game never meets, is told on screen only
  when the player asked for it (the quick save, the room, the debug bar's Into slot N), and only logged for the
  autosave, slot 20 and the scripts' saves (2026-10-27): to the player a save openblack cannot make is a failed save,
  and the original's answer to a failed save is that box, so it is the same rule, not a new one. Tests
  `SaveTriggersInPlay.AnAutosaveThatCannotWriteTheWorldKeepsTheOldSaveAndLogsOneLine`,
  `SaveTriggersInPlay.ASaveThePlayerAsksForThatCannotBeMadeIsToldOnScreen`,
  `SaveTriggers.WhichFailedSavesTheMessageBoxTells`.
- ✅ **The profile the save on leaving goes to** (bw1-decomp `src/Black/Game.cpp:1943-1967`, the frozen exe's
  0x54D54A..0x54D5E5): the Players page's activation keeps the profile played (`FrontEnd::PreviousProfile`,
  `frontend.cpp:1116`, 0x53E9CB), cleared after each play loop's end. With none kept, or the mode not the restart (2)
  and the profile played the kept one (`_wcsicmp`), slot 20 is saved as usual; else, when the kept one still exists and
  is not the one played, `SetCurrentProfile` takes it up, slot 20 is saved, and the one played is taken up again; else
  nothing is saved. openblack: `Game` keeps the profile played as the Players page shows and whether the start again
  is that page's (its Back and the restart question, not Start Game, whose mode is 1), and
  `save_triggers::Exiting` saves where `ExitSaveProfileOf` says, the kept profile selected for the save with its
  controls, push scrolling and creature's name taken up, then the one played again (test
  `SaveTriggers.LeavingTheGameSavesIntoTheProfilePlayedAsThePlayersPageWasOpened`). The save's picture, which comes
  back a frame or more later, goes into the pictures' folder of the profile played as it was asked, as the game reads
  its screen at once (`SavePictureSystem`; test
  `SavePictureSystem.APictureGoesToTheProfilePlayedAsItWasAskedForThoughAnotherIsPlayedOnceBack`). The activation's exception, the
  page activated again as the new player's box closes, keeps the profile kept: openblack's Players page does not show
  again as that box closes, so the same.
- Not done: the memory-pressure quick save and quick load (the original's object heap), the room's entry list,
  scroll and help script, `AutoSaved`, the two black frames, game mode 4 and the box callback `sub_54C930`.

**The players' interfaces** (`src/ECS/SavedWorld/WritePlayerInterfaces.cpp`, `RegisterPlayerInterfaceWriters`): each
player's interface (0x64), its status (0x65) and its leash's status (0x102) are a source's objects whose id is the
player's number (every player has one of each, as in every frozen save). A load keeps each player's first interface,
its status and its leash's status by that number (`KeepPlayerInterfaces`, through `KeepLoadedSources`, which
`Game::LoadSavedGame` calls after `saved_world_load::Apply`). The three are written with the player's number (their own
words, the status's and the leash's player numbers) and with the references between them by
`context.Reference`; the leash's status says whether the leash is on in the player's hand, whether it works, its type,
what it is tied to, the turn of its last tie and, while it is on, its two lengths, from the leash of the creature the
player leads (`LeashSystemInterface::PlayersCreature`, `components::CreatureLeash`, `saved_leash::WriteLive`), and
keeps the rest as loaded while the player leads no creature. Player one's status, the hand's, writes the hand's
values from the live hand (`HandSystemInterface::GetSavedStatus`, `saved_hand::WriteLive`; the field list below); the
other players' statuses stay as loaded (what the original keeps there is 0 in every frozen save), but a kept hand
holding an object gone since the load holds nothing: held 0, the hand's byte (HasObjectInHand) 0 and
HandHoldingSomething 0, as `GInterfaceStatus::Process` 0x5DC4E0 checks every player's hand each input turn
(`GMagicHand::Validate` 0x5FB130 -> `RemoveFromHand` 0x5FB0B0, then `TidyHands` 0x5DC6F0) and the save's
`CleanUpForSerialisation` 0x5FB2A0 zeroes the emptied slot; and a kept leash status tied to an object gone since is
tied to nothing (`GLeashStatus::Validate` 0x5E6B90, the same pass, sets ObjectAttachedTo 0 and nothing else;
2026-11-11, test `PlayerInterfaceWrite.AKeptHandAndLeashLetGoOfAnObjectGoneSinceTheLoad`). One made since the
load is written as the constructors leave it (tests `PlayerInterfaceWrite.*`, the frozen Land 1 autosave's 24 records
by their writers and the file byte for byte, `HandStatusLoad.*`).

**The hand's status comes back** (`src/ECS/SavedWorld/LoadPlayerInterface.cpp`, `saved_hand::Load`, from
`Game::LoadSavedGame` after the leash). The original's load (`GInterfaceStatus::Load` 0x5DCDC0, exe only: bw1-decomp
has no body) reads the fields into a status the load makes, and `GInterfaceStatus::ResolveLoad` 0x5DD190 only starts
its guidance (`GGuidance::Init`); `GInterface::Load` reads only the link to its status, and `GInterface::ResolveLoad`
0x5D0510 clears the interface's collide. Then `GameOSFile::ResolveAllLoads` 0x561D1D calls
`GInterface::ResolveLoadForCreature` 0x5D0560: `InterfaceActionForceToNormal` 0x5D1260 (the action ended, openblack's
`EndAction`), fn_005E7660 with ecx the status's leash status (the leash's `SetOn` again, as the leash's paragraph
above says: `saved_leash::SwitchAgain`, run after the hand's restore) and the status's creature lock bit (+0x24 &
0x20) cleared. openblack puts player one's status back into the hand
(`HandSystemInterface::RestoreSavedStatus`): the values below, the rest as a new status leaves them (the still move
+0xF4..+0xFC, which the file does not keep, the locked object and the creature lock, the pick-up's record), then the
action ended with no seed's unlock (the new interface has no action to leave). Each field, in the file's order:
- the GameThingWithPos part: its position, the hand's synced place (`GetSyncedPlace`, `HandSyncedPlace`).
- 0xA2C2A: a constant, read and dropped. player_number (+0x28): the status's player.
- +0x40: the second word of the interface's locked select (+0x3C, not saved, is its object): no counterpart, kept;
  the load ends any locked select.
- ThrowVelocity, ThrowAngularVelocity, HandPos, HandAngles (+0x44, 48 bytes): the throw's data the ThrowData packet
  sets, put back as the packet's handler sets them.
- the hand (`GMagicHand::Load` 0x5FB320: HasObjectInHand, and `ReadPtrArray` of one object) and HandHoldingSomething
  (+0x90, the count of hands holding, 0 or 1; `TidyHands` 0x5DC6F0): the object in the hand, the entity the load made
  of it, with no pick-up call (its own record says it is held). `GMagicHand::Save` 0x5FB2C0 clears the slot of an
  empty hand first (`CleanUpForSerialisation` 0x5FB2A0). The plan takes every object a status's hand names despite
  its held marks (`WorldPlan::inHand`): map objects, trees, villagers, animals, pots (`PlannedPile`), moving objects,
  orbs and scaffolds (`madeHere`), and the spells' seeds, which are planned wherever they are; each is made, then taken
  off the map. An object the load still does not make leaves the hand empty, with one log line naming it. A seed in the
  hand keeps its type and its creator's player for the last seed type it leaves when it goes out of the hand (openblack
  caches them at a pick-up; the original reads them from the seed).
  **The drawn hand** (`CHand::Load` 0x46E930, from `GGame::Load`, the game object's tail: `handWord`, `handObject`,
  `handWords`, `handPoint`, `handObjects`, `handByte`) reads CurrentState (the hand state's index: 1
  normal, 4 holding, 8 grain), HeldObject (+0x4904, `ReadPtr`), holdType, heldObjectSortingKept, heldObjectShadowKept,
  holdLowering (a float), position (12 bytes), spellInHandParticles (`ReadSafe`), spellInHand (`ReadPtr`) and
  spellInHandFlag (a byte). `CHand::ResolveLoad` 0x46ECF0 (from `GGame::ResolveLoad` 0x555080, before the land balance
  and the help): a holding state with no object becomes normal; holding, the pick-up base place is the object's (x and
  z in world units, its altitude above the ground plus the ground's), the held object's and the tug's velocities are
  zeroed and the holding state's Enter runs; the grain state's Enter runs for state 8. openblack
  (`saved_hand::DrawnHandOf`, `HandSystem::RestoreSavedDrawnHand`, after the status): when the record is in the
  holding state and names the status's held object, which the load made, the drawn hand holds it with the saved hold
  type and lowering and the saved sorting flag (the object drawn sorted while held, as at a pick-up); its radius,
  height and rootedness are the object's, which every drawn frame and turn ask again. No pick-up call. Not put back:
  the pick-up base place and the velocities (openblack's hold has no such fields), the shadow flag, the spell-in-hand
  seed (the hand's sprinkle collection, 0xF5, is a particle collection's data, read with the particles),
  and a drawn hand naming another object than the status's (left empty: the load's ForceToNormal lets go of a drawn
  object the status's hand does not hold; the order of the two resolves is not read). The spell-in-hand particles
  (2026-11-12, `handObjects[0]`, `ReadSafe`) are made again from their records once the seeds are, with their seed
  (`handObjects[1]`) and the bone flag (`handByte`), and taken in as the hand's in-hand effect
  (`hand_fx::RestoreInHandEffect`, the load's `Makers::handEffect`; test
  `MiracleFxSystemTest.ASavedGamesInHandEffectIsTakenInAsLoadedForItsSeed`); the save writes them from the live in-hand
  effect (`hand_fx::InHandEffectOf`, its file the particle type the seed's level gave it), none without one. The save writes HeldObject and
  spellInHand from the live hand (`RegisterGameObjectWriter`, src/ECS/SavedWorld/WriteGame.cpp, with the hand
  service): `CHand::Save` 0x46EB10 writes `WritePtr(+0x4904)` and `WritePtr(+0x4950)` unchecked, and both are what the
  local interface's drawn hand has now. HeldObject is the drawn hand's object (`HandSystemInterface::GetRenderHandObject`):
  0 once it is thrown (`CHand::ThrowObject` 0x46DDD0 sets +0x4904 to 0, also for an object that is no longer available,
  from `CHand::GameTurnUpdate` 0x46E4E0). spellInHand is the seed whose effect plays in the hand
  (`HandSystemInterface::GetSpellInHand`, `hand_fx::InHandEffectSeed`): set by fn_0046E7B0 (from
  `SpellSeed::SetPowerUp` 0x729B30, for the local interface's seed: as it comes into the hand and at each power-up)
  only when the level's in-hand particles (`particleTypeInHand`, fn_007285E0) were made, and set to 0 as the seed goes
  out of the hand (`ReleaseInHandEffect` fn_0046E890, from `SpellSeed::InterfaceSetOutMagicHand`), as the effect ends
  or the map is cleared (`DeleteEffectInHand` fn_0046E780, from `DrawSpellInHand` 0x46E680 and `OnClearMap`), and when
  the seed is no longer available (fn_0046E640, from `CHand::GameTurnUpdate`). A seed in the hand is both. Each is 0
  when the hand holds nothing or the thing is gone (openblack destroys at once; the original writes a dying object
  until the next turn's pass drops it). The rest of the hand part is written from the drawn hand too
  (`HandSystemInterface::GetDrawnHandNow`, `saved_hand::WriteDrawnHand`, the inverse of `DrawnHandOf`): CurrentState
  is the drawn hand's state (`_renderHandState`, the same index: 1 normal, 4 holding, 8 grain); while it holds an
  object, holdType, heldObjectSortingKept and holdLowering are the hold's (`ThrowObject` leaves the three as they were,
  so with nothing held they stay as the load kept them, the values a load reads only in the holding state); position
  (+0x78) is the hand's place without the turning pose's lift (`_smoothedPosition`; +0x78 does not take the lift
  either), kept as loaded until the hand is first placed; spellInHandFlag (+0x4954, fn_00728620: the seed's info says
  the in-hand effect is drawn at the hand's bone; set by fn_0046E7B0 with the effect and never cleared) is written
  while an effect plays in the hand, else kept as loaded. Kept as loaded, with no live counterpart:
  heldObjectShadowKept (+0x4914) and spellInHandParticles (+0x494C, `WriteSafe`); also the byte after a seed has come
  and gone since the load (the original keeps that seed's flag, openblack the loaded one). Tests
  `HandStatusLoad.TheGamesRecordNamesTheDrawnHandsObjectAndSeedFromTheLiveHand` and
  `HandStatusLoad.TheGamesRecordsDrawnHandIsWrittenFromTheDrawnHandAndReadsBackAsIt` (a game loaded with an empty
  hand, the hand then picking an object up, writes a record `DrawnHandOf` reads back as holding it).
- +0x94 (8 bytes): no counterpart, kept.
- +0x9C: the turns the last unlocked apply was held (the 0x1A handler 0x5DA8A0).
- +0xA0: the turns the hand stayed within one unit (fn_005DBC60, `UpdateTurnMovement`).
- +0xA4: the synced hand; CameraPos (+0xB0) and CameraFoc (+0xBC): the camera the sync gave.
- ReportedHandPos (+0xC8): the synced hand at the last turn's end; +0xE8: that turn's move; HandSpeed (+0x100), +0x104
  (the heading), +0x108 (the turning rate), HandVelocity (+0x10C) and +0x118 (the sideways acceleration): the turn's
  movement as fn_005DBC60 leaves it.
- +0x11C: the ring index the sync handlers copy (not ported): kept.
- iface (+0x2C) and LeashStatus (+0x12C): the links, written by their writers.
- LastSpellGained (+0x34, read by fn_005DD2A0): the last seed type the repeat gesture asks for again, the interface's
  (`gestures::State().lastSeedType`) and the player's spell icons' copy (`worship::player::SetLastSeedType`).
- +0x38: no known reader ([the spell core](../magic/spell-core.md)): kept.
Nothing of the tricons is saved: they are the camera's helper icons, which fade in from the hand's state. The debug
bar's Saved Games menu shows what the last load put back (the object in the hand, made or not, and the last seed type).
Tests `HandStatusLoad.*`.

**The leash's status comes back** (`src/ECS/SavedWorld/LoadLeashStatus.cpp`, `saved_leash::Load`, from
`Game::LoadSavedGame` once the resolves are done, so the creatures know their leashes again first). The original's
load (`GLeashStatus::Load` 0x5E7430, exe only; `ResolveLoad` 0x5E7650 is empty) reads the fields with no `SetOn`, no
moods and no sound; the `SetOn` comes later, at the load's end (below). Each field, in the file's order, and where openblack puts it on the leash of the
creature the player leads:
- Activated (+0x14): the leash is on, `CreatureLeash::worn`, held by the status's player; off takes any leash off.
- LeashWorks (+0x18): `CreatureLeash::leashWorks`, and the worn leash's `works`.
- LeashType (+0x1C): `CreatureLeash::selected`, and the worn leash's `type`.
- player_number (+0x30): the holder (the status's own word, kept by `KeepPlayerInterfaces`).
- AppliedForce (+0x20): no counterpart; kept in the record (0 in every frozen save).
- ObjectAttachedTo (+0x24): the worn leash's `tiedTo`, the entity the load made of that object; a status tied to an
  object the load did not make is not put back (one log line).
- +0x28 and LeashLength (+0x2C), the two lengths `CalculateLeashLengthFromCreature` 0x5E6980 and
  `UpdateLeashLengthWhenAttachedToObject` 0x5E69E0 write: the worn rope's `slackLength` and `maxLength` (the frozen
  saves' 43 / 122 are `leash::InHand` of a size-30 creature, 90 / 180 the tied lengths). With the leash off they have
  no counterpart and stay as loaded.
- InterfaceStatus (+0x38): the link to the status, written by its writer.
- +0x34: the game turn of the last tie (written by `SetOnObject` 0x5E6BD0 at 0x5E6D17 from GameTurn):
  `CreatureLeash::lastTiedTurn`, and the worn leash's `tiedTurn` when it is tied.
The rope is laid afresh from its ends on the next frame. **At the load's end** `GameOSFile::ResolveAllLoads` 0x561D1D
calls, for every creature with a player, its player's leader interface's `ResolveLoadForCreature` 0x5D0560, which calls
fn_005E7660 on the status's leash status (0x5D0568..0x5D0574): `SetOn(the player's +0xA4C creature, Activated)`
(0x5E7663..0x5E767D). openblack does it after the hand's status is back (`saved_leash::SwitchAgain`,
`LeashSystemInterface::SwitchAtLoad`, the switch the tie uses, `LeashSystem::Switch`): on, for a creature that knows
the learning leash, a worn leash stays as the load put it back (held or tied) and takes its mood and its owner's
temple pick again, then its two lengths again over the saved ones, and one not worn goes on as its owner picked it;
off, or without the learning leash, it comes off (`TakeOff`, or `ReleaseUnleashed` with none worn). ✅ The lengths:
`SetOn`'s on path (taken when `on` and the creature's `LeashEnabled[2]`) sets Activated, lays the rope from the
hand's start point (0x5E6FBE / 0x5E6FEE), picks the type at the creature's player's temple (fn_005E6B00 at 0x5E7001)
and then, with no test, calls `CalculateLeashLengthFromCreature` 0x5E6980 at 0x5E7009. That writes +0x28 and
+0x2C from the creature's size (×15, then ×0.7 + 22 and ×3 + 32) when ObjectAttachedTo is 0, else calls
`UpdateLeashLengthWhenAttachedToObject` 0x5E69E0: tied to a tree, 6 × the creature's height (40 and 20 when that is
over 40) and half that; tied to anything else, 1.5 × the distance in metres between the two (0x5E6A6A..0x5E6A7A),
held to 180..360, and half that. So the saved lengths of a worn leash, tied or not, are replaced at the load's end
by the ones its put-on takes from where the creature and its tie stand; openblack's `LeashSystem::Switch` takes
them again (`LengthsOf`), a tie to a thing the load did not keep keeping the saved ones. The haptics are not
ported. Tests
`LeashStatusLoad.TheLoadsEndSwitchesEachLeashAsItsStatusSays`,
`LeashSystemTest.TheLoadsSwitchKeepsAWornLeashAndTakesOneOffAsItsStatusSays`. In the 16 frozen world files 128 statuses read; 9 are on
(one a rival's, tied, with +0x28 = 8 as raw bits and length 0, put back as they are). Tests `LeashStatusLoad.*`.

**The camera comes back** (`src/ECS/SavedWorld/LoadCamera.cpp`, `saved_camera::Load`, after `KeepLoadedSources`; its
writer `src/ECS/SavedWorld/WriteCamera.cpp`, a source of one object kept by `KeepCamera`). `GCamera::Load` 0x4433A0
(exe only, Ghidra's C and the disassembly) reads, after the GameThingWithPos part, pos (+0x5C), time (+0x68, the
mode's seconds), TimeDelta (+0x6C), scriptSlomoControl (+0x70), CameraDirty (+0x74), Weather (+0x80, 8 bytes), the
focus zoomers (CameraHeadingZoomer +0x88, 144 bytes), the position zoomers (CameraOriginZoomer +0x118), the field of
view zoomer (+0x1A8) and the byte +0x78; then `ClearCameraStack` 0x442FE0 deletes every mode and makes a new
`CameraModeNew3`, whose constructor (CameraModeNew3.cpp:1215) reads the loaded zoomers (`Reinitialise`) and whose push
zeroes time; then the saved mode's number and, through fn_00441650, a follow (0x46), heading (0x48) or free (0x65)
mode pushed with its own `Load`. openblack: the camera's origin and focus `Zoomer3` and the script camera's field of
view zoomer take the record's, `script_camera::ClearModes` (the mode's seconds 0) and the player's model starts again
(`CameraModel::Restart`, which also counts on the debug window's restarts) with `PlayerCameraRestarted`. The other
values are rewritten by `GCamera::Update` every frame (TimeDelta 0x441FCA, CameraDirty 0x442660, Weather 0x4426BF,
pos), +0x70 and +0x78 have no reader but the constructor and Update's ground test, so they are kept in the record. A
save writes the zoomers, the field of view and the mode's seconds from the camera, the rest as loaded; inside the
citadel, where openblack's camera shows the rooms, the zoomers are the island camera's the citadel keeps
(`TempleInteriorInterface::GetOutsideCamera`, as the original's rooms never move GCamera's; test
`SavedCamera.InsideTheCitadelASaveWritesTheIslandsCameraTheCitadelKeeps`). Tests `SavedCamera.*`.

**The camera's mode in a save, and a cut scene saved mid-way** (read 2026-11-10). `GCamera::Save` 0x4430E0 ends with
the current mode's save number (vt+0x38 of `modes[ModeCurrentIndex]`) and that mode's `Save` (vt+0x40). Only three
classes have a number of their own: `CameraModeFollow::GetSaveID` 0x44B990 = 0x46, `CameraModeFollowHeading` 0x44D900 =
0x48, `CameraModeFree` 0x44DFF0 = 0x65; every other mode, the player's `CameraModeNew3`, the citadel's, the creature
interaction's, the dual camera (`CameraModeTwoObjects`) and the path modes among them, has `CameraMode::GetSaveID`
0x44A330 = 0. `CameraModeScript` (the script's mode, START_CAMERA_CONTROL) inherits from `CameraModeFollow` and its
vtable's +0x38 and +0x40 are the follow's (0x44B990, `CameraModeFollow::Save` 0x44D5F0): a save made while a script
has the camera writes 0x46 and the follow's own values, in `CameraModeFollow::Load` 0x44D3E0's order: Heading +0xC,
Pitch +0x10, ViewingDistance +0x14, ZoomTimeScale +0x18, RelativeHeading ("behind") +0x1C, UseOffsets +0x20, FocusOffset
+0x24 and OriginOffset +0x30 (12 bytes each), the Target pointer +0x8 (the thing the position follows) and
AutoReposition +0x44. The script mode's own values (+0x48 alive, +0x4C the focus's thing, its computer player follows
+0x50 / +0x54, the track +0x58 and its time +0x5C) are not saved. A load (`GCamera::Load` 0x4433A0) clears the stack
to a new `CameraModeNew3` and, for 0x46, fn_00441650 makes a plain `CameraModeFollow(camera, 0, 1.0, 0, 0)` (pushed by
its constructor) and reads those values into it: never a `CameraModeScript`. So after a load **the cut scene's camera
does not go on**: the script's SET/MOVE_CAMERA_POSITION, FOCUS and the rest find no `CameraModeScript` (their
`__RTDynamicCast`, e.g. `SetCameraPos` 0x6EC8F0, does nothing else; the 1.20 non-Creature-Isle branch of
ScriptCamera.cpp:103), and END_CAMERA_CONTROL's `ActualEndCameraControl` 0x6ECD70 only warns about the mode and puts
the field of view back. A follow of nothing (a cut scene's camera that follows no thing) fails `IsStillValid` 0x44B960
(Target null) and the turn's `CheckStackedModesForValidity` 0x441D40 takes it off and restarts the player's mode; a
follow of a thing stays, a player's follow of it. The script's mode is made with UseOffsets 0 (ctor 0x44B800 at
0x44B940) and AutoReposition 0 (ctor 0x461180 at 0x4611B5); nothing writes its offsets, so its saved offsets are the
heap's bytes (undetermined). openblack: `saved_camera::FollowPartOf` writes the script mode when it is the current
mode outside the citadel (`script_camera::ScriptModeCurrent`): its heading, pitch, distance, time factor, "behind", 0
for UseOffsets and 0 for both offsets (unread with UseOffsets 0), the position's thing and 0; any other current mode
writes 0. While the script mode or a dual camera is current the zoomers written are the script camera's (it moves the
camera's zoomers then; before 2026-11-10 the player's were written). A load makes the player's camera of a plain mode
and of a follow of nothing (`LoadedModeOf`; ⚠️ the frames before the first turn, when the original's follow of
nothing is still current and the zoomers go on with any move under way, are not modelled). Tests
`SavedCamera.TheScriptsModeIsSavedAsAFollowOfItsView`, `ALoadMakesThePlayersCameraOfAPlainModeOrAFollowOfNothing`,
`ASaveInACutSceneWritesTheScriptsModeAndTheZoomersItMoves`.

**Creature Mode, the heading and the free modes in a save** (read 2026-11-10). Creature Mode is a plain
`CameraModeFollow` of the creature, made by the creature key (`GGame::DoAction` 0x63FB4B: `CameraModeFollow(camera,
creature, 1.0, 0, offsets)`, offsets 1 when the camera is less than 80 [0x8D060C] across from the creature) and by
the double click on a creature (`CameraModeNew3::Update` 0x45E34B: `(camera, creature, 1.0, 0, 0)`). Its constructor
0x44B800 takes the offsets' flag only to choose its starting distance and then turns it off (0x44B940) and sets
AutoReposition 1 (0x44B85A); nothing else writes AutoReposition but the script mode's constructor (0x4611B5, 0) and
`Load` (the field scan of +0x44: the writes at 0x44CCBD / 0x44CD28 in `Update` are to a zoomer, `edi`, not the mode),
and nothing writes the offsets (+0x24, +0x30) of a follow made without them. So Creature Mode saves as 0x46 with its
heading, pitch and distance (openblack's `creature_follow::View` yaw, pitch, distance: the same
`GetHeadingAndPitchFromPoints` heading and pitch, `CreatureCameraModel`), 1.0, 0, 0, the heap's offsets, the creature
and 1. The load makes a plain follow of the creature again with those values, current over the player's
`CameraModeNew3`: Creature Mode on the creature. openblack: the writer writes Creature Mode's follow
(`saved_camera::CreatureModeFollow`) while it has the camera (`CreatureModeSystemInterface::GetView`, outside the
citadel, no script mode, dual camera or creature interaction current), the offsets as 0; once the load's objects and
creatures are back (`saved_camera::LoadFollow`, from `Game::LoadSavedGame` after the hand), a follow with those values
(`IsCreatureModeFollow`) of a loaded creature locks Creature Mode onto it with the saved view
(`CreatureModeSystemInterface::Restore`: `Enter`, the view set, no double click's button held). A follow of a thing the
load did not make follows nothing and the first turn takes it off: the player's camera, with one log line. A script's
mode saved in a cut scene while it followed a thing comes back in the original as the player's plain follow of that
thing with the script's values (time factor 0.2, "behind" 1, AutoReposition 0), creature or not: openblack makes it
the same way, with Creature Mode's follow (2026-11-10, below). Not made again (one log line, the player's camera): a
heading (0x48) or a free mode (0x65): `CameraModeFollowHeading`'s constructor 0x44D8A0 is called
only by fn_00441650 and by its `Create` 0x44D840, which nothing calls or points to, and `CameraModeFree`'s 0x44DF90
only by fn_00441650 (xref callers and refs), so no game in play has one to save, and only a file that had one makes
one (see [Not in the original](../project/not-in-the-original.md)). Tests `SavedCamera.AFollowReadsBackFromItsModePart`,
`CreatureModesFollowIsItsViewAtThePlainPaceWithTheClearView`, `ASaveInCreatureModeWritesItsFollowOfTheCreature`,
`ALoadedCreatureModeFollowLocksOntoTheCreatureAgain`, `CreatureModeSystem.ARestoreLocksOntoTheCreatureWithTheSavedView`.

**The player's follow of any thing** (read 2026-11-10, bw1-decomp `CameraModeFollow.cpp`, matched). The plain follow
is one class whatever it follows: each frame (`Update` 0x44C160) the distance kept in 2..1500, the ease T = the
follow's pace × ZoomTimeScale, the focus to the thing's point (a flock's position with half its leader's height, else
its 3D object's position, else its map coords, plus half its height), the position from the distance, the pitch (at
least π/13) and the heading, less the thing's game angle turned to radians when RelativeHeading is set and the thing
is a `MobileWallHug`; Ctrl and Shift look for a clear view only with RelativeHeading 0 and AutoReposition 1; outside
the camera fence the position goes onto it and the player's mode is made. `ProcessKeyMovement` is the same keys for
every follow: Shift / Ctrl and the cursor keys turn, tilt and draw it, the wheel zooms, the cursor keys alone or a grip
of the land make the player's mode. It ends when its thing is no longer available (`IsStillValid` 0x44B960, the turn's
check). openblack: Creature Mode's `CreatureModeSystem` and `CreatureCameraModel` are that follow, for a creature as
before and now for any thing a load gives it (`CreatureModeSystemInterface::Restore` with the saved
`creature_follow::Style`: time factor, behind, clear view). A thing that is not a creature is read as the script
camera's follow reads it (`script_camera::Thing`, `FollowPoint`, its wall hugger's angle for a heading behind it) and
stays followed while it is there and available; the keys, the grip, the fence and the temple end it as they end
Creature Mode. The save writes the follow with its own style. ⚠️ openblack checks the thing every frame, the original
once a turn (as Creature Mode already does). Tests `CreatureCameraModel.AFollowsStyleSetsItsPaceAndKeepsItsHeadingBehindTheThing`,
`CreatureCameraModelTest.CtrlAndShiftLeaveAFollowBehindItsThingOrWithoutTheClearViewAsItIs`,
`CreatureModeSystemTest.ARestoreFollowsAThingThatIsNotACreature`, `SavedCamera.ALoadedCreatureModeFollowLocksOntoTheCreatureAgain`.

What else a cut scene keeps in a save is the game tail's: the script's camera owner, CameraControlTask (GScript +0xA8:
START_CAMERA_CONTROL 0x6ECCDF writes it; `GScript::Save` writes it and `Load` 0x6EC270 reads it back at 0x6EC563, the
seventh of the script's eight words, `GameTail::scriptSecondWords[6]`), with the drawing flags +0x78, +0x7C, +0x80
(`scriptSecondWords[0..2]`); and the help system's wide screen: `HelpSystem::Load` 0x5C73E0 reads WideScreen +0x45E8
(0x5C77F9), its owner WideScreenControl +0x45EC (0x5C7807) and the bars' timer +0x45F0 (0x5C7815, a float), the sixth
to eighth of the help's last seventeen words (`GameTail::helpLastWords[5..7]`; the order of the seventeen: +0x45D8,
+0x45E0, +0x45DC, +0x45E4, +0x460C, +0x45E8, +0x45EC, +0x45F0, +0x45F4, +0x4604, +0x4608, +0x45FC, +0x4600, +0x45F8,
+0x1C, +0x45C4, +0x45C8, after +0x45CC, +0x45D0 and the half +0x45D4). The load reads them into place without
`SetWideScreen`, so the bars, the refusal of every action (`GGame::DoAction` 0x63F952) and the owner's later
SET_WIDESCREEN / END_CAMERA_CONTROL come back as saved. openblack (2026-11-10): a save writes the camera's owner from
the script control (`ScriptControlSystemInterface::GetCameraOwner`), the wide screen and its owner from the help
system and the bars' timer from the cinema bars (`saved_game_save::CurrentValues`, `HeadOf`); a load reads them
(`saved_game_load::StateOf`) and puts them back as they are (`Apply`: `RestoreCameraOwner`,
`HelpSystem::RestoreWideScreen`, `CinemaBars::Restore` at the saved timer, with the sounds' copy of the script's
wide screen), without the rest of a wide screen's change, so the owner's END_CAMERA_CONTROL gives the camera back and
the bars show where the save left them (tests
`SavedWorldWrite.TheScriptsCameraOwnerAndTheWideScreenGoWhereTheLoadReadsThem`,
`ScriptControlSystem.ALoadPutsTheCameraOwnerBack`, `HelpSystem.ALoadPutsTheWideScreenAndItsOwnerBackWithoutTheHook`,
`CinemaBars.ALoadPutsThemAtTheirSavedTimerWithNoSlideStarted`). ⚠️ the drawing flags (`scriptSecondWords[0..2]`) and
the game speed's owner (+0xAC, `scriptSecondWords[7]`) are still neither written nor put back. 🔍 whether the load
sets the interface inactive again (`SetWideScreen`'s `GInterface::SetActive`) is not read; openblack leaves it as it
was.

**A new game's camera** (`saved_camera::NewRecord`, `WriteNewCamera`): the game object of a game not loaded names the
camera as the first of its tail's two objects (`WriteGame.cpp`), numbered at its place in the walk as any object; the
camera is a GameThing, with no creation number (its own word, destroyed word and flags are 0 in every frozen save,
`GameThing::GameThing`). Its fields, from what the original holds at a save (`GCamera::Save` 0x4430E0 writes them as
they are) and openblack's source:
- the position part: `UpdateGameThingWithPosData` 0x442EF0, the last call of every `GCamera::Update` (0x4426EB): the
  drawn camera's x and z × 6553.6 truncated, its y less its own cell's altitude × 0.67 (y off the map or with no
  block), flags 0 (`GameThingWithPos::SetToZero`; 0 in every frozen save). openblack: `Camera::GetOrigin`, as the
  audio's `CameraMapCoords`, and `LandIslandInterface::GetCellAltitude`. openblack's drawn player camera has the
  nudge and the 1 m ground clearance since 2026-11-10 (`script_camera::HoldDrawn` in `ApplyShake`, see [the game
  camera](../camera/game-camera.md)).
- pos (+0x5C): `LH3DIsland::RayCast` from the position zoomer (nudged x − 1, y + 1 when within 0.001² of the focus,
  0x4421D5..0x442228) through the focus (0x442406), y = `GetAltitude` there (0x442423), outside the citadel; on a miss
  it keeps the last one. openblack: `LandIslandInterface::RayCast`, `GetHeightAt`; a miss refuses the save (the point
  last looked at is not held).
- time (+0x68): `script_camera::State::modeSeconds`. TimeDelta (+0x6C): `GetCameraTimeInc` 0x555820 (the frame's
  whole wall ms, `g_delta_time`) × 0.001f [0x8AC418] as a float, at most 0.1f [0x8AB22C] (0x441F95..0x441FCA):
  `script_camera::State::frameMs` (`game_clock::CameraFrameMs`, kept by `Game.cpp` each frame) through
  `script_camera::FrameSecondsOf` (test `SavedCamera.TheFrameTimeIsTheFramesWholeMillisecondsAtMostATenth`, the frozen
  saves' 1, 15, 16 and 31 ms).
- scriptSlomoControl (+0x70): 1 while a script task holds the game's speed (`GScript::StartGameSpeed` 0x6FA9E0 and
  `ActualEndGameSpeed` 0x6FAA60 set it with GameSpeedTask): `!ScriptControlSystemInterface::MaySetGameSpeed(0)`.
- CameraDirty (+0x74): 1 when the camera given to `UpdateCamera` is not where `g_camera` was drawn the frame before,
  each axis by `fcomp` (a NaN reads as equal; 0x442627..0x4426B0): `script_camera::State::drawnMoved`, set in
  `ApplyShake` from the zoomers' value and the last drawn camera.
- Weather (+0x80): `LH3DAtmos::GetWeatherSmooth`(the drawn camera, 1) (0x4426BA): `weather::atmos::GetWeatherSmooth`.
- the focus, position and field of view zoomers: as for a loaded camera; +0x78: 0 (constructor 0x4418A2, written
  only by the load); the mode: 0, the player's `CameraModeNew3` (`CameraMode::GetSaveID` 0x44A330).
A new game's camera is not named (the file as before) without a camera of the world or a land. **Inside the
citadel** (read 2026-11-10 in the disassembly): `InnerCamera::PreDraw` 0x796920 copies the room camera's own zoomers
(ZoomerPos, ZoomerFoc) into its current position and focus and calls `LH3DTech::UpdateCamera`, after `GCamera::Update`
in the frame, so the `g_camera` the next `GCamera::Update` reads is the room camera of the frame before. That
update's citadel branch (0x442337..0x442399, `g_game` +0x205A28 = 1) copies `g_camera`'s position and focus into its
drawn camera, skips the ray (so pos keeps the point of the last update outside), then goes on to the 1 m hold over the
land (0x44242B..0x4424AB: `GetAltitude` at the drawn x, z, +1, less y; added to y and the focus's y when above 0) and
skips `UpdateCamera` (0x442602 → 0x442673): CameraDirty compares `g_camera`'s position with that held one, Weather is
`GetWeatherSmooth` at the held one and the position part (`UpdateGameThingWithPosData`) is `g_camera`'s, not held. The
rooms are in the island's coordinates as they are drawn: the room meshes sit at the origin, unturned and at scale 1
(`TempleInterior::AddRoomParts`, `_templePosition` 0), and the room cameras' paths are the original's
`Data/Citadel/engine/<room>.cam` in those coordinates, so openblack's drawn temple camera (`TempleCameraModel`, its
interpolation of no time) is the room camera's place itself; how faithfully it moves is the temple camera's own
fidelity ([temple camera](../project/progress/camera/temple_camera.md)). openblack: the camera drawn the frame before
(`script_camera::State::drawnBefore`, kept in `ApplyShake`), held by `saved_camera::HeldOverLand`; pos the ray of the
zoomers the citadel kept going in. Tests
`SavedCamera.ANewGamesCameraIsWrittenFromWhatTheGameHolds`, `SavedCamera.WithNoCameraOfTheWorldANewGamesCameraIsNotWritten`,
`SavedCamera.InsideTheCitadelTheDrawnCameraIsHeldOneMetreOverTheLand`,
`SavedCamera.ANewGamesCameraSavedInsideTheCitadelIsTheKeptIslandCameraAndTheRoomsDrawnCamera`.

**The computer players** (`src/ECS/SavedWorld/WriteComputerPlayers.cpp`, `RegisterComputerPlayerWriters`): each
player's computer player (0x66) and its hand's state (0xF9) are a source's objects by the player's number, and its
eight attitudes (0x10D) by the player's number × 8 + the other player's. A load (`LoadComputerPlayers`, through
`KeepLoadedSources`) keeps them by those ids and puts back, through `ComputerPlayerSystemInterface::Restore`, the
switches (switched on, started on the land, paused, the creature-like player: on the player's entity, none for a
player whose switches are a new one's) and the mind (the icons counted, each node's suppression and personality and
the attitudes, on the player's entity's mind or kept for it, as a mind kept from the land before). The writers put
in the switches from the player's `components::ComputerPlayer`, whether a script holds the hand and the player's and
the steps' speeds from its `components::ComputerHand`, and the mind's values from `SavedMind` (the entity's or the
kept one); the references between them and the player by `context.Reference`. Their places, lists, sub-actions and
the other words stay as loaded; one made since is written as the original makes every player's (tests
`ComputerPlayerWrite.*`, the frozen Land 1 autosave's 80 records by their writers and the file byte for byte).
The hand's steps (0xFA) and their arguments (0xFB) are a source's objects by the player's number and their places
(the step's in the steps, the argument's in the step): written from the player's `components::ComputerHand` when it
has one (each step's kind, each argument's kind, thing by `context.Reference`, number, amount and point; the 64
bytes and the word +0x60 as loaded, zero for one made in play), else as loaded; a load keeps them by those ids and
(2026-11-11, `HandOfRecord`) puts the hand back on the player's entity when its state has steps: the hand's point
(+0x50, the twelve bytes), its step's point (+0x5C), the steps' speed (+0x68) and the player's (the computer player's
+0x58), its map place, +0x30 (the first step to be looked at for a move in front), +0x34 (one put there), the stage
+0x38, a wait's turns +0x44, a gesture's progress +0x78 and origin +0x7C, whether a script holds it, and each step
with its arguments, a thing as the load made it again. Not put back: +0x3C / +0x40 (the time estimate, read by
nothing in play) and +0x74 (a cast's thing, read only by the save), which the writer keeps as loaded; what the hand
holds and the queued actions; busy (+0x1F8, not saved), so a loaded hand is ready once its steps are done. What the mind has seen (0x67) is a source's objects by the player's number
and its place in the mind's list: a load puts each back in the mind (`ComputerMind::seen`: a forest by its key, a tree
or a seed as the entity the load made of it, else the turn alone) and the writer writes the computer player's Seen
list and each entry from the mind, one whose thing openblack does not hold as loaded (tests
`ComputerPlayerWrite.AHandsStepsAndTheirArgumentsAreWrittenFromTheHand`, `...WhatTheMindHasSeen...`, and profile D's
slot 15 Land 2 save: its 6 steps and 9 arguments by their writers, the file byte for byte).

**The particles** (`src/ECS/SavedWorld/WriteParticles.cpp`, `RegisterParticleWriters`): each live spot visual's
container (0x6E), its effect's particle interface (0xA5) and manager (0xA4) are a source's objects named by the
container's effect's number, written from the particle engine (`ParticleSystemInterface::GetState`): the container's
map coordinates, whether its effect is drawn as one, the thing it hangs on (by `context.Reference`, while it is still
there), its interface and its turns left; the manager's atom count, player, process info, origin, alpha, age,
closing-down bit, start velocity with its direction and speed, magnitude, closing-down age, synced flag, its file (the
container's spot visual's particle type, `Container::particleType`, as `GParticleContainer::Create` 0x63E4B0 makes
the effect for the spot visual info's type, fn_0063E410; a container whose type openblack does not know, made without
the game's info or by the debug hook's test effect, stops the write with a message naming it; a spell's effect by
its class's particle type; a physical shield's effect, which neither holds, by SF_PhysicalShieldFX's, 67: the
saves' shield managers name {67, -1}; a worship site's mana path, which neither holds either, by SF_ManaPathNew's, 22,
as `WorshipSite::CreateManaPathSprite` 0x77B2C0 makes it for `PARTICLE_TYPE_MANA_PATH`, test
`ParticleWrite.AWorshipSitesManaPathNamesItsFileByTheManaPathsParticleType`: before, a save once a site's first mote of
prayer power had flown was refused) and the things it aims at. The game's list of containers is the live ones, the newest first, then those the load kept, as the
loaded file listed them, but for a loaded one made again and gone since (its turns run out, or its owner lets its
visual go; before, every save after the frozen Land 1 save 14/1's load was refused for 37 of them), which has left the list as the
original's `GParticleContainer::ToBeDeleted` 0x63E1D0 unlinks a container from `GameLists.ParticleContainers` (test
`ParticleWrite.ALoadedContainerGoneSinceTheLoadLeavesTheGamesList`). The container's strength is the one it gives its effect each turn (`Container::strength`).
The manager's collections (its roots, the newest first), each collection (0xA3) and each atom (0xA2) are written
(`src/ECS/SavedWorld/ParticleCodecs.cpp`, a source's objects named by the effect's number and their place in a walk
of the effect): a collection's rules still on it by their places in the file ({the file's particle type, the
object's index}, `Modifier::fileIndex`), its birth, group, flags (0x01 its own coordinates, cleared under an ancestor
that passes its frame down, 0x02 drawn interpolated; the other six bits unset, 0xCD's), its manager, parent atom,
its atoms, the newest first, the word 0xFFFFFFFE for a group whose atoms pass their frame down (0 otherwise: all
516 saved collections) and its alpha byte; an atom's sub-collections, the newest first, velocity, birth, turn (no
translation, as all 1953 saved atoms), base scale, scale, stretch, place, ARGB colour, specular, flags (0x01 drawn on
its first update, 0x02 / 0x04 stepped once / twice, 0x08 deflected, 0x10 in use, the other bits 0xCD's), its two
drawn matrices with their three unset bytes, its last and current frames, its animation (its rate, its creator's
frame count, playing, the creator's looping: byte 8 follows PlayAnim though bw1-decomp names it Loop, as
`AR_SetAnimPlay` sets it; then two bytes the creators copy from their stack, FF FF or 1E 20 in the saves, written
0xCD from openblack), gravity, collection, its point particle (0xA8) or its sprite (0xB4, `Particle3DSprite::Save` and
`ParticleSpriteCreator::CreateParticle3DSprite` 0x6AA0D0: drawn, the creator's ScaleAlpha, the creator, its
FileOffset, its flags (0x01 CentreAtBase, 0x02 stretched, StretchVertically not 1, 0x04 IgnoreRotation, 0x08
UseLandscapeColor; the high nibble unset), then the stretch of its last two steps, which `GameUpdate` 0x6C9E40
copies only while it is stretched, 1 until then) or its mist (0xAB, `RenderParticleMist::Save`,
`ParticleMistCreator::CreateParticleMist` 0x6AA610 and `RenderParticleMist::DrawAt` 0x67A670: drawn, the creator, its
InitialScale, TakeRatioFromMatrix, a half all four saved mists hold as 0, then the place (x, 0, z) and the alpha / 255
of its last land light map stamp, which the draw sets only for a creator with a light map, the lightning storms':
unset 0xCD bytes and 1 until then; openblack keeps them on the atom as `mist_atoms::SubmitFrame` stamps) or its
animated textured mesh (0xAE, `Particle3DObjAnimTextured::Save` and `ParticleMeshCreatorAnimTextured::CreateParticle`
0x6A8DA0: drawn, the creator, its flags (0x01 NeverClip, which this creator reads in the plain mesh creator's
CastHumanShadow place, 0x02 DrawWithLandscapeColor; the high five bits unset), TextureHeight, TextureWidth, SlideU,
SlideV, a byte both saved ones hold as 0, its frame count, 1000 for a sliding texture) and its random byte. Its sounds,
the newest first (0x16, `PSysSound::Save` 0x6D14A0 / `Load` 0x6D13A0: the count of the particles' sounds alive, the
global the sound's constructor and destructor step, fn_006D0EC0 / fn_006D0FC0, `audio::spell_sounds::Count`; its
PSysSoundAction, 24 bytes: the action, four slots, the flags byte and three zero bytes, `particles::SoundAction`; its
delay; its atom; whether it has been released). A collection's rules' data,
the newest first (the original makes each on the rule's first use, `GetModifierData`, at the list's head;
`Collection::dataMade`): the emitters' (0xED, `EmitterRule::ShouldEmit`: the rule by its place in the file, the next
emission time, the count emitted, whether the first update is to come); the trail's (0xF3,
`UR_Trail::CollectionData::Save` 0x6D02F0: FirstUpdate, then the ring's filled count twice, as the count and as its
size, `GameOSFile::WriteSafe(GJCircularArray)`, and each place from the newest, `Entry(i)` = Head - i: position,
scale, the 3x4 matrix of its turn; openblack's ring `fireball_rules::TrailPlaces`); the hand's sprinkle's (0xF5:
FirstUpdate and its age, `UR_HandSprinkle::ModifyAtomCollection` adding a step each call, PSysModifiers.cpp:826); the
atoms at a target's (0xC4: the game turn it took its target at, and the target, the physical shield in the saves).
An atom's rules' data, the newest first: its sound rule's (0xDC, `AddSoundToAtom::ModifyAtomCore`, PSysModifiers.cpp:104:
whether it has started its sound). An atom's draw offset (atom core field 20, 2026-11-11): one that follows the hand
(`DrawOffsetLT` 0xA6, `DrawOffsetLT::Save` 0x6CB910: its reference count, Amount +0x18, RefPos +0x1C) from
`Atom::HandDrawOffset`; a decaying one (`DrawOffsetDecay` 0xA7, `DrawOffsetDecay::Save` 0x6CB9C0: its reference count,
StartOffset +0x20, CurrentOffset +0x2C, Age +0x38, DecayRate +0x3C, 0.5 from fn_006C7760) from
`Atom::DecayingDrawOffset`, written once for the atoms that share it (`DecayingDrawOffset::id`; the trails that take
their ball's) with their count; the original steps it after each frame's draw (fn_006C7990 in `PSysGlobal::DrawLoop`),
openblack before the next frame's, so the record takes the last frame's ms on its age and offset, and a loaded one has
none still to take (tests `ParticleCodecs.A*DrawOffset*`,
`ParticleWrite.TheAtomsDrawOffsetsAreWrittenOnceForTheAtomsSharingThemAndLoadedTheSame`). A chain (2026-11-11) keeps
its joints as the frame's draw last left them, as the original's (`ParticleChainJoint::DrawAt` 0x679E80, called from
the atom draw fn_00679920, writes the joint `JointIndex` of its chain: the drawn matrix's translation +0x24..+0x2C
with the draw offset added, its scale +0x30, the DrawData colour, the atom's RGB under the drawn alpha byte through
the effect's, its specular, and 1, and sets the chain's +0x40; `JitterEnabled` is only ever 0, ctor 0x55F090, so no
jitter is drawn): `Collection::chainJoints` / `chainDrawn`, one joint for each chain joint made in the collection
(`Atom::chainJoint`, numbered as made, as `CreateRuleMakeChain` 0x69FD10, `UR_GesturingRecognised` and `UR_Plasma`
number them), all zero until drawn (the `Chain` ctor 0x6C8830) and kept when its atom is gone; taken once a frame as
the particles are collected for the frame's draw (`manager::KeepDrawnJoints`), beside the draw, so identical in play
(test `SimpleBeamTest.EachChainKeepsItsJointsAsTheFrameDrawsThem`). The chain is written (0xB6, `Chain::Save` 0x6CC780,
named by its collection, collection field 8, and by each joint's particle): its creator (+0x14, `CreateChain` 0x6AA880),
+0x18 never written (0xCD's), FrameHeight +0x1C, FrameWidth +0x20, FrameOfHead +0x24, FrameOfTail +0x28, +0x2C (4,
the ctor's, never written after), the repeats +0x30 (NumTexturesForWholeChain, joints - 1 for -1, or a rule's:
`Collection::chainTextures`), FileOffset +0x34, UseDynamicLighting +0x38, the v-scroll +0x3C, the joints (a count,
then 28 bytes each: place, scale, colour, specular, drawn, three bytes all 120 saved joints hold as 0), the v-scroll
rate +0x4C and the drawn byte +0x40; a joint's particle (0xB3, `ParticleChainJoint::Save` 0x6CC540): drawn, its
chain, its `JointIndex`, its jitter 5 and its switch 0 (`CreateChainParticle` 0x6AA900). The six chains of the frozen
slot 0 save (the magic beam, SF_SimpleBeam, three to each of its two effects) hold the joints of the frame drawn at a fraction of
about 0.76 between their atoms' two drawn matrices, every one of the 120 (tests
`ParticleCodecs.AChainIsWrittenAsTheOriginalSavesItAndReadBack`, `ParticleCodecs.AChainJointIsWrittenAsTheOriginalSavesIt`,
`ParticleWrite.AChainIsWrittenWithItsJointsAsTheFramesDrawLeftThem`). The load (`Chain::Load` 0x695C10,
`ParticleChainJoint::Load` 0x6959D0, PSysLoad.cpp) puts the chain back on its collection: its creator, its joints,
its drawn byte, its v-scroll and its rate, and its repeats as the collection's own when they are not what its creator
gives; each joint's atom takes its chain's creator and its number (test
`ParticleWrite.AChainIsLoadedFromTheFileItWroteAndWrittenAgainTheSame`). The simple beam (2026-11-11) keeps what its
two data records hold (`particles::beam`, Particles/BeamData.h): on its atom (`UR_SimpleBeam::AtomData`, ctor
0x55F110) the object it follows, the game turn that object was last found available (its `GTPointer`'s
GameTurnValidated, stamped as it is taken and at each step it `IsAvailable`; unset 0xCD's for a point, as both saved
ones) and the two ends; on each beam's collection (`SubCollectionData`, ctor 0x55F160) its number, NumBeams and the key points
it was last laid through (the spline +0x20 is built from them each step by fn_006771C0 -> fn_00677490); the rule notes
its data as made on the atom and on each beam's collection (test `SimpleBeamTest.ABeamKeepsWhatASaveWrites`). Both
are written: the atom's (0xBF, `UR_SimpleBeam::AtomData::Save` 0x6CD3B0: the rule, the turn +0x20, the object +0x24,
the end +0x28 and the start +0x34) and each beam's (0xB9, `SubCollectionData::Save` 0x6CE1E0: the rule, the spline's
BCType +0x28, 1 for zero end slopes, its elements, a count then each key's t = i x (1 / (N - 1)), its point and its
second derivatives, one curve an axis by `key_points::Make`, the scalar build fn_005B3760 that the vector one
fn_00677490 is per component, then the number +0x2C and NumBeams +0x30), in its collection's rules' data. The first
beam of the frozen slot 0 save (a point beam, its turn unset) is given back bit for bit from its key points, the
second derivatives included (tests `ParticleCodecs.TheSimpleBeamsDataIsWrittenAsTheOriginalSavesItAndReadBack`,
`ParticleWrite.TheSimpleBeamsDataIsWrittenOnItsAtomAndOnEachBeam`). The load (`AtomData::Load` 0x696850,
`SubCollectionData::Load` 0x697670) puts them back: the atom's turn, ends and object (by the load's entities), each
beam's number, count of beams and key points, noted as made; the next step lays the beam afresh from them as in play
(test `ParticleWrite.ASimpleBeamIsLoadedFromTheFileItWroteAndWrittenAgainTheSame`). With the game's data, the
frozen slot 0 save's two magic beams are made again from their records and their six chains, 120 joints, six beams
(the spline's second derivatives recomputed from the key points) and two beam atoms are written back as the file holds
them (integration test `ParticleWrite.TheFrozenSlotZeroMagicBeamsGiveBackTheirChainsAndBeamsBitForBit`, skipped
without the data). The joints are taken for every effect drawn, on screen or not, as the original's: every container
is drawn each frame (`GParticleContainer::DrawParticleContainers` 0x63E0D0), each collection's atoms all reach DrawAt
(fn_006798B0) and the Z-sorter culls nothing (`LH3DZSorter::NewZObject`, its capacity aside); no joint is kept on a frame that draws no world (`GGame::Process3dEngine`, Game.cpp:2061: the full screen film's
`videoOnly` skips the world's draw, where openblack's PreDraw returns before the collect, and only the world view mode, `ViewMode` 0, draws the world: the
citadel's (1, `GoInsideCitadel` 0x553E10) draws the temple and the falling spell video's (2,
`KickOffFallingSpellVideo` 0x5539A0) the film, so openblack keeps joints only while `game_clock::SequenceMode()`,
its port of `ViewMode` with the same writers, is 0); the beam's turn is
`GGame::data.GameTurn` (0x6763C0, fn_00677110 for the take), openblack's `game_clock::Turn()`. The rules' data of the classes with a fixed row of values ([the particles' rules' data](#the-particles-rules-data))
are read and written by one record, `gamesave::RuleDataRecord` (`RuleDataRecords.h`): the class, the GameThing part,
the rule's Persistent, the row's values but its references as their bytes, and its references; each class's row is
its layout (`RuleDataLayoutOf`), with typed reads and writes of a value by its place (`Byte`, `Word`, `Float`,
`PointAt`, `Matrix`, `Reference`); `test_gamesave`'s `EachClass/RuleDataRecordLayout.*` pins each class's bytes and
running sum. Of them, written from openblack: the mana path's data on each spark (0xC0, `mana_path::AtomData` from
`Particles/Rules/ManaPath.h`, its path and progress, noted as made as the spark is; `particle_codecs::ManaPathDataValues`,
read back by `ManaPathDataOf`; the end at +0x20 is the path's `to`, the start +0x2C its `from`, the side +0x38, the
spline's four coefficients from the cube's, the length +0x74, the weave's size +0x78, its frequency +0x7C, the phase
+0x80 and the progress +0x84, as `ModifyAtomCollection` 0x69ACD0 sets them; test
`ParticleCodecs.AManaPathSparksDataIsWrittenAsTheOriginalSavesItAndReadBack`); a load puts it back on its spark, noted as
made (`mana_path::RestoreData` from `LoadParticles.cpp`, as `UR_ManaPathNew::AtomData::Load` 0x695FC0 reads it), so
the next step reads the progress the save kept to let an arrived spark go, and steps the others along their saved
paths (test `ParticleWrite.AManaPathsSparksAreLoadedFromTheFileTheyWroteAndWrittenAgainTheSame`); the glowing shell's
data on each atom (0xBB, `volume_glow::AtomData`: the game turn its object was last found there, then the object;
`particle_codecs::VolumeGlowDataValues`, `VolumeGlowDataOf`), written and loaded back the same way; the plasma's on
each carrier atom (0xBD, its `PlasmaCommand` as `ModifyAtomCollection` 0x676530 copies the `UR_PlasmaInf`'s fields
+0x14..+0x4C to +0x20..+0x58: start, end, start and end normals, width (the life), speed, alpha; `plasma::DataOf` and
`RestoreData` from `Particles/PlasmaData.h`, `particle_codecs::PlasmaDataValues`, `PlasmaDataOf`; tests
`SimpleBeamTest.APlasmasCarrierKeepsItsArcAsItsRulesData`, `ParticleCodecs.APlasmasArcIsWrittenAsTheOriginalSavesItAndReadBack`).
The plasma's beams' own data (0xBC, a list) is not written yet. The belief sprite's data on each sprite (0xBE: its start
+0x20 and start alpha +0x2C, as `ModifyAtomCollection` 0x69B750 sets them; `belief_sprite::AtomData` from
`Particles/Rules/Belief.h`, `particle_codecs::BeliefSpriteDataValues`, `BeliefSpriteDataOf`; tests
`BeliefSpriteRule.ASpriteKeepsItsStartAndStartAlphaAsItsRulesData`,
`ParticleCodecs.ABeliefSpritesDataIsWrittenAsTheOriginalSavesItAndReadBack`) is written and loaded back the same way. The
creature spell wisps' data (`UR_CreatureSpellGeneric`, MAC 0x678D50): on its collection (0xC1, made as soon as the rule
has its creator) the first update's byte +0x38, where the creature's 3D body stood (+0x78 of `GetCreature3D`, kept at
+0x20) and the gesture point (+0x2C) as the wisps began, the byte again, the wisps due +0x3C (a float) and made +0x40
(an int); on each wisp (0xC2) theta, phi, their rates and its size (+0x20..+0x30). openblack kept only the two points'
x and z before; it keeps them whole now (`creature_spell::WispsData`, `WispData` from `Particles/CreatureSpellWispData.h`,
the due and made still in the rule's slot; `particle_codecs::CreatureSpellWispsValues`, `CreatureSpellWispValues` and
their reads; tests `CreatureSpellParticlesTest.TheWispsKeepWhereTheyBeganAndEachWispItsTurnAsTheirRulesData`,
`ParticleCodecs.TheCreatureSpellWispsDataIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back. The
orient-with-velocity sprite's (0xC3, `UR_OrientSpriteWithVelocity` 0x69A790: FirstUpdate +0x20, then its
`GJLowPassFilterVariableStep` +0x24: the value, the time constant +0x30 and Initialised +0x34, all set on the first
step, Initialised to 1): `orient_velocity::AtomData` from `Particles/Rules/Orient.h`,
`particle_codecs::OrientWithVelocityValues` and `OrientWithVelocityOf` (test
`ParticleCodecs.AnOrientWithVelocitySpritesDataIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back. The
creature spells' collection data, each made on the rule's call once it has its creator: the first rule's (0xC6,
`UR_CreatureSpell::ModifyAtomCollection` 0x677F90: FirstUpdate +0x20, then the creature's GTPointer, its turn +0x24
stamped with `GGame::data.GameTurn` as the creature is taken and at each step it is available (vt+0x2C answering 1), the
creature +0x28, cleared once it is not); the hearts' (0xC5, 0x678A50) and the steam's (0xC8, 0x678610, its byte written
twice): FirstUpdate +0x20, cleared on the call that makes the data, the atoms due +0x24 (a float) and made +0x28 (an
int); the itch's (0xC7, 0x6781A0): FirstUpdate +0x20, cleared once its flies are made. `creature_spell::CreatureHeld`
and `AtomsMade` from `Particles/CreatureSpellData.h`, `particle_codecs::CreatureSpellValues`,
`CreatureSpellAtomsMadeValues`, `CreatureSpellItchValues` and their reads (tests
`CreatureSpellParticlesTest.TheFirstRuleKeepsItsCreatureAndTheTurnItWasLastFoundAvailable`,
`ParticleCodecs.TheCreatureSpellsCollectionDataIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back. The
first rule's atom's particle, the creature it stands for (`RenderParticleCreatureRef`, class 0xAF; Save 0x6CC2E0 and
Load 0x695770: the render particle's enabled byte, then `WriteSafe` of its creature's GTPointer, the turn word then
the creature) is written from the atom's creature particle and loaded back with its creature
(`gamesave::ParticleCreatureRefRecord`, `particle_codecs::CreatureRefValues` and `CreatureRefOf`,
`creature_spell::RestoreCreatureRef`; tests `SaveWorldListRecords.ACreatureSpellsFirstParticleKeepsItsCreatureAndTheTurnItWasMadeSureOf`,
`ParticleCodecs.ACreatureSpellsFirstParticleIsWrittenAsTheOriginalSavesItAndReadBack`); no frozen save holds one, and
a round trip in play, with a creature spell in flight, is still to run. The sphere surface tracer's (0xCB,
`UR_SphereSurfaceTracer::ModifyAtomCore` 0x6A32B0) and the forest path's (0xCD, `UR_ForestPath::ModifyAtomCollection`
0x6A3770) data on each atom: FirstUpdate +0x20, cleared on the first step that draws the three angles +0x24, +0x28,
+0x2C (`PSysFloatRand(2π)` each, the third read by neither); `sphere_angles::AtomData` from
`Particles/SphereAnglesData.h` (the tracer's kept in the atom's vec4 data, the angles then 1 once drawn),
`particle_codecs::SphereAnglesValues`, `SphereAnglesOf` (test
`ParticleCodecs.TheSphereAnglesDataIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back. The flocking
rule's (0xD0, `UR_Flocking::ModifyAtomCollection` 0x683580: made once the collection has atoms; FirstUpdate +0x20, 1
from its constructor and written by nothing, then the flock's velocity +0x24, damped and pulled each step):
`flocking::FlockVelocityOf` and `RestoreFlockVelocity` (the slot's state), `particle_codecs::FlockingValues`,
`FlockingOf` (test `ParticleCodecs.AFlocksVelocityIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back.
No atom data of the explosion's class (0xCE) is made in play: its only constructor (0x560D40, the one place the exe
holds its vtable 0x8FE7EC) is called only by `GameOSFile::LoadInstance`, so openblack writes none. The cloud mover's
(0xD1, `UR_CloudMoverNew::ModifyAtomCollection` 0x6D41C0: made on its first call, FirstUpdate +0x20 cleared as that call
puts every atom at the effect's origin): `storm::CloudMoverFirstOf`, `RestoreCloudMoverFirst`,
`particle_codecs::CloudMoverValues`, `CloudMoverOf` (test `ParticleCodecs.ACloudMoversDataIsWrittenAsTheOriginalSavesItAndReadBack`),
written and loaded back. The cloud gather's data on each cloud (0xD3, made as each cloud is,
`UR_CloudGather::ModifyAtomCollection` 0x6D4A70 and its init fn_006D4880: Radius +0x20, Theta +0x24, +0x28 written
by nothing after its constructor's 0, SpecActive +0x2C, SpecStartAge +0x30 set at a strike, HeightOffset +0x34):
`storm::CloudData`, `CloudOf`, `RestoreCloud`, `particle_codecs::CloudValues`, `CloudOf` (test
`ParticleCodecs.ACloudsDataIsWrittenAsTheOriginalSavesItAndReadBack`), written and loaded back. Its collection's
(0xD2, made after the float providers' check): the heading +0x48, the clouds due +0x20 (a float) and made +0x24 (an int),
FirstUpdate +0x28, the rate +0x2C, the next strike +0x30 and the strike's end +0x34 (collection ages), the spin +0x38,
the lightning collection +0x3C and the cloud it hangs from +0x40 (cleared as the strike ends, +0x44 then set), and the
lightning, rain and storm switches +0x45..+0x47; the storm itself (+0x54) is not saved, so after a load it is made
again on the next step, as openblack's gather makes it when it has none. `storm::CloudGatherValues`, `CloudGatherOf`,
`LightningOf`, `LightningCloudOf`, `DetachedLightningOf`, `RestoreCloudGather`; a lightning collection that hangs from no
cloud is among the effect's parts its writer numbers (after the walk from the roots) and its load makes it from the
reference alone, once every part is made (`Restorer::Finish`); `particle_codecs::CloudGatherValues`, `CloudGatherOf`
(tests `ParticleCodecs.ACloudGathersDataIsWrittenAsTheOriginalSavesItAndReadBack` and
`ParticleWrite.ACloudGatherIsLoadedFromTheFileItWroteAndWrittenAgainTheSame`, its lightning let go of its cloud). The tornado's data (2026-11-12, `UR_Tornado`): on its collection (0xD7, `CollectionData` 0x74, made on the rule's first call 0x6D18B0 and saved by 0x6CFC40) FirstUpdate +0x20, the base +0x24, the top +0x30, their velocities +0x3C and +0x48 (`UpdateBaseAndTopPoints` 0x6D1C60), a point +0x54 only its constructor 0x5608A0 sets (0, 0, 0), the TornadoScale +0x60 and the strength +0x64 (0x6D1924), the alpha +0x68 (the byte its debris and flying collections take, `UpdateDebrisAtoms` 0x6D2AF0 and `UpdateFlyingAtoms` 0x6D31F0, and the pretend objects' test above 0xFA, fn_006D2E70), the fade +0x6C and the close-down +0x70; on each debris and flying collection (0xD5 for both, saved by 0x6CF9F0 / 0x6CFAA0, made on the first update of each, 0x6D2AF0 / 0x6D2E70) the atoms made +0x20 and due +0x24, floats; on each flying atom (0xD4, 0x2C bytes, saved by 0x6CFB50) its state +0x20, the height fraction it rises to +0x24 and its spin's scale +0x28. The debris and flying collections and the flying atoms have not the tornado among their own rules: the writer numbers that data past their rules' places (`MadeRuleIndex`) and the load finds the rule by its place in the file. `storm::TornadoData`, `TornadoEmitterData`, `TornadoFlyingData`, `TornadoOf`, `TornadoEmitterOf`, `TornadoFlyingOf` and their `Restore`s; `particle_codecs::TornadoValues`, `TornadoEmitterValues`, `TornadoFlyingValues` and their reads (test `ParticleCodecs.ATornadosDataIsWrittenAsTheOriginalSavesItAndReadBack`). Before, a tornado's data was left out of the file. The game objects a tornado carries (the atoms' creators) are not written yet. The lightning sprite emitter's data on its collection (0xD9, 2026-11-12, `EmitterRuleLightningSprite`, 0x34 bytes made after its creator's check, `ModifyAtomCollection` 0x6A6910, saved by 0x6D0490): the collection age its flash began at +0x20, which its constructor leaves at the particle heap's fill 0xCDCDCDCD until the first flash (openblack's `storm_flash::State::strikeAge` starts at that value, read only once a flash has set it), whether the flash lights the cloud +0x24, the next sprite's due time +0x28, the sprites made +0x2C and FirstUpdate +0x30 (`storm_flash::StateOf`, `RestoreState`, `particle_codecs::LightningSpriteValues`, `LightningSpriteOf`, test `ParticleCodecs.ALightningSpriteEmittersDataIsWrittenAsTheOriginalSavesItAndReadBack`). The sound removal's data on each atom (0xDD, 2026-11-12, `RemoveSoundFromAtom::AtomData`, 0x24 bytes made on the rule's first call on the atom, `ModifyAtomCore` 0x69DDD0, saved by 0x6CD7D0): SoundRemoved +0x20, set once it has stopped the atom's sound of its action (`sound_rules::SoundRemovedOf`, `RestoreSoundRemoved`, the atom's data x; `particle_codecs::SoundRemovalValues`, `SoundRemovalOf`; test `ParticleCodecs.ASoundRemovalsDataIsWrittenAsTheOriginalSavesItAndReadBack`). The sub-collections rule's data on each atom (0xE2, 2026-11-12, `AddSubCollectionsToAtom::AtomData`, 0x24 bytes made on the rule's first call on the atom, `ModifyAtomCore` 0x69F220, bw1-decomp PSysModifiers.cpp:504, saved by 0x6CDAA0): Added +0x20, set as it puts its NextGroups under the atom (`fireball_rules::SubCollectionsAddedOf`, `RestoreSubCollectionsAdded`, the atom's data x; `particle_codecs::SubCollectionsValues`, `SubCollectionsOf`; test `ParticleCodecs.ASubCollectionsRulesDataIsWrittenAsTheOriginalSavesItAndReadBack`). The fireball's rule's data on each atom (0xDB, 2026-11-12, `AttatchFireBallToAtom::AtomData`, 0x2C bytes made on the rule's first call on the atom, `ModifyAtomCollection` 0x682FD0): FirstUpdate +0x20, cleared as the ball is made (`MagicFireBall::Create`), then its `GTPointer<MagicFireBall>`: the turn +0x24, stamped with `GGame::data.GameTurn` as the ball is taken and at each step it is still valid (vt+0x2C answering 1; else the pointer is cleared and the turn stays), and the ball +0x28. openblack keeps the turn on the ball's link from its atom (`magic::CurrentTurn` at each step its ball is still there; `FollowAtom` was passed 0 before), the first update in the atom's data y and the ball by its key in x (`fireball_rules::BallData`, `BallDataOf`, `RestoreBallData`, which gives a loaded ball a key of its own among the balls and its `atomKey`; `particle_codecs::FireBallDataValues`, `FireBallDataOf`; the writer names the ball by its class 0x20; test `ParticleCodecs.AFireballRulesDataIsWrittenAsTheOriginalSavesItAndReadBack`). The fade-out rule's data on each atom (0xDA, 2026-11-12, `AR_FadeOutOnceConditionTrue::AtomData`, 0x30 bytes made on the rule's first call on the atom, `ModifyAtomCore` 0x6A7CF0, bw1-decomp PSysModifiers.cpp:3117, saved by 0x6CD660, read by 0x696B00): ConditionMet +0x20 (a byte), then StartTime +0x24, StartAlpha +0x28 and StartScale +0x2C, floats, set as the condition first holds (its constructor sets only ConditionMet, so the three floats are unset until then; openblack's are 0) (`fade_out::DataOf`, `RestoreData`, the atom's data x to w; `particle_codecs::FadeOutDataValues`, `FadeOutDataOf`; test `ParticleCodecs.AFadeOutRulesDataIsWrittenAsTheOriginalSavesItAndReadBack`). The willow wisp's data on its collection (0xEE, 2026-11-12, `UR_WillowWisp::CollectionData`, made on the rule's first call, `ModifyAtomCollection` 0x6A6D20, bw1-decomp PSysModifiers.cpp:2879, saved by 0x6CFFC0, read by 0x6994B0): LastParentPos +0x20 (12 bytes), NumToEmit +0x2C and NumEmitted +0x30 (floats both, the constructor 0x55FE40 sets them 0) and FirstUpdate +0x34 (`particle_codecs::WillowWispDataValues`, `ApplyWillowWispData`, the slot's state xyz, extra x and y and first; test `ParticleCodecs.AWillowWispsDataIsWrittenAsTheOriginalSavesItAndReadBack`). Not
written from openblack
yet: the other rules' data of collections and atoms, the plain meshes and light maps (their atoms name no particle),
its word +0x78 (0, as made), +0xAD (1, as made), its targets' words (0), target points (none) and current target (0), and the
three bytes after the process info (0; the frozen saves have 0xCD or 0 there). An atom's two drawn matrices (the
atom core's draw infos, +0x98 the last step's and +0xD0 this step's, though bw1-decomp names them the other way) are
taken each step as the original's atom step makes them (fn_00673EA0 with fn_00673DB0: the turn's rows times
Scale x BaseScale, the second row times StretchHeight too, at LocalPos; through the frame of the nearest ancestor
whose collection passes it down, `LHMatrix::PreMultiply`, the local matrix first; the scale times the frame's; the
alpha the colour's times the collection's byte / 255 unless it is 255; on the first step the last one is this one):
`particles::saved_draw` (`Atom::savedDraw`), beside the DrawState the drawing uses. Checked on every one of the 1953
saved atoms of the 19 world files read (`savetool --fields`, bit for bit: 1544 with no frame, 409 in one); tests
`ParticleSavedDraw.*` pin two of them. The load's side (`src/ECS/SavedWorld/LoadParticles.cpp`,
`saved_particles::PlanEffect` and `RestoreEffect`) makes an effect again from its interface, manager, collections,
atoms, particles, rules' data and sounds (`audio::spell_sounds::RestoreSound`, nothing played), as the original's
loads (bw1-decomp PSysLoad.cpp) put them back and draw no random number: the atoms keep their random bytes and their
two saved drawn matrices until their next step, a point atom takes the one point creator its group's rules name, the
effect's next process is a single step (test `ParticleWrite.AnEffectWithItsAtomsIsWrittenLoadedAndWrittenAgainTheSame`:
written, made again and written again byte for byte). The containers of the game's list are made again with it
(`saved_particles::PlanContainers` / `MakeContainers`, once every thing they can hang on is made; the oldest first, so
that the newest is at the head): each its effect made again from its records and taken into the engine as it is
(`particles::manager::Adopt`), its object (the script's handle) at its place, the thing it hangs on, its turns, its
strength, its spot visual's particle type and its draw path (`manager::RestoreContainer`); no effect is started again,
so no random number is drawn. A food pile's sparkle is its container (`Pot::speedUpVisual`): the pile's own restart of
it at the load is gone. A container whose effect cannot be made stays as the load kept it, with one log line, and so does one whose thing
the load did not make (2026-11-12; a creature casting at the save is kept as read): made, it would close at once with
nothing to hang on, and the thing's kept record would name a container gone (the frozen save 0/0's creature; test
`ParticleWrite.AContainerWhoseThingTheLoadDidNotMakeIsKeptAsReadWithIt`). A loaded
effect's manager writes back what openblack does not hold as its record had it: the point +0x78 (0 in 227 of the 231
saved managers), the byte +0xAD (0 in 221) and its targets' points and word. Still unread for an effect made in play:
those values (written 0 and 1) and its targets' own words.
A vortex's object mover (2026-11-11) is written as its In's particle interface (the In's second reference, written
none before) while it runs, its file by its vortex row's particle type. Each thing it carries writes the rule's data
on its particle (0xC9, `UR_VortexAttract::AtomData::Save` 0x6CEE70: the rule, the start height, the taken velocity,
whether it can't be destroyed, whether its 3D object was paper and disappearing and its importance before the take,
and the out vortex) and its particle (0xB2, `RenderParticleGameObject::Save` 0x6CC440: drawn, the turn the thing was
last found available on and the thing (none once it is let go), the effect's player, the thing's matrix, which is the
atom's drawn matrix of this step, and the byte the take clears); what the take found of the thing's 3D object comes from
its kind and state (docs/wiki/world/vortex.md), a loaded one's as its record had them (test `ParticleCodecs.AVortexsCarriedThingIsWrittenAsTheOriginalSavesItAndReadBack`, pinned to the
Land 1 save 14/1's records 3274, 3277 and 3278). The load (2026-11-11) makes a way in's object mover again from its
records, as `LandscapeVortexIn::Load` (0x600550) reads it back rather than starting one: `vortex::Create(.., Loaded,
mover)` adopts it, its interface and manager records kept so that a write gives back what openblack does not hold as
read (as for the other loaded managers); each atom whose particle is a game object (0xB2) gets its carrier back with its
turn, its byte, the player its record names (resolved by the plan) and its thing (by the load's entities), and its vortex attract data (0xC9) with its out vortex; once the effect
is made, each carrier takes its thing, carried as its record's own flags say (out of the map cells, in the physics with
no body, marked carried; `particle_carried_objects::RestoreCarried`, no take). An animal a mover carries is made despite
its carried marks (flown by the particles and in the physics). A mover that cannot be made leaves the vortex starting
its own, with one log line.

**The spell icons** (`src/ECS/SavedWorld/WriteSpellIcons.cpp`, `RegisterSpellIconWriters`): a town centre's icon
(0x56) and a worship site's (0x78) are written from their entities (`components::SpellIcon` with
`TownCentreSpellIcon` or `WorshipSpellIcon`), with the parts of a fixed object of several cells (`MultiMapFixedFieldsOf`,
their own row 2396 less the icon table's index), the seed's row (`SpellSeedInfoRow`: 2578 less the seed's number), and
each class's own values from its component; the status of the hand that charges a worship icon is the charging
player's interface status (`SourceKey` 0x65), and its seeds by `context.Reference`. A loaded icon keeps its
SpellCoords, its SpecularColor and the parts' values openblack does not model as its load kept them; one made in play
has its SpellCoords at the place it stands (it is made there, and nothing moves an icon), SpecularColor 0 and its
FixedFlags 0x08. Unread for one made in play, written 0: its place's and its object part's altitude (the parts'
writer keeps them, 0 for one made since). An icon of no seed, or with an openblack specular colour (whose bytes in
the original's word are not read), stops the write with a message naming it. The loads make both: the worship sites' with their sites, the town
centres' after their centres (tests `SpellIconWrite.*`).

**The spell seeds** (`src/ECS/SavedWorld/WriteSpellSeeds.cpp`, `RegisterSpellSeedWriters`): a seed (0x24) is written
from its entity (`components::SpellSeed`): the object's parts of its seed's row (`ObjectPartOf`), then every field
above from the component (its creator: the player through `PlayerObject` or the worship icon; its holder's interface
status, the creator's player's, `SourceKey` 0x65). The object part's map coordinates are the place its maker made it
at (`SpellSeed::madeAt`; kept for a loaded seed), as the original's makers give it: a seed made into the hand from the
hand's place (the one-shot orb's, `OneOffSpellSeed::CreateSpellIntoHand` 0x72A75E) at the hand's reported position
× 6553.6, truncated, at altitude 0 (`seed::MadeAtHand`); a caught fireball's at the fireball's place (fn_00682EA0); a
fully charged icon's seed at the icon's place (`WorshipSpellIcon::PutFullyChargedPowerUpSeedInHand` 0x77F8F0). These
are the only makers of seeds in the exe (fn_00728300 and `SpellSeed::Create` 0x7282A0 and their callers): the
packets', the rewards' and the dispensers' make one-shot orbs, not seeds. A seed of no seed type, or of a creator that is a
creature or another thing (whose objects in the record are not read), stops the write with a message naming it. No
load makes the seeds yet (tests `SpellSeedWrite.*`).

**The fireballs' balls** (`src/ECS/SavedWorld/WriteFireBalls.cpp`, `RegisterFireBallWriters`; 2026-11-12): a ball (0x20,
`MagicFireBall::Save` 0x6832B0: `Object::Save`, `WriteSafe` of the manager +0x5C, the byte +0x58) is written from its
entity (`components::MagicFireBall`): the object's parts of its row, 316 less its row of `GMagicFireBallInfo`'s three
(the rows after the climates run 320 for the spell system, 319-317 the vortices, 316-314 the fireball's), its target
the place it was made at (`MagicFireBall::madeAt`, set by `fireball::Create` as `AttatchFireBallToAtom::ModifyAtomCollection`
0x682FD0 gives `MagicFireBall::Create` the atom's x and z × 6553.6, truncated, at altitude 0, which `Object::Object`
keeps), the manager of the effect whose atom carries it (a `SourceKey` of the managers' class by the effect's id; 0 once
the effect has gone, as `WriteSafe` writes a gone object) and whether rain cools it (+0x58, 1 unless a script cast it,
`MagicFireBall::Create`). Test `SpellSeedWrite.AFireballsBallIsWrittenFromItsEntity`. A load (`MagicFireBall::Load` 0x683320 reads
the same) plans the balls as the game's list of fireballs holds them, the newest first, a ball of no fireball's row
left out (`WorldPlan::fireBalls`), and makes them from the list's last to its first before the fires, which name them
(`Makers::fireBall`, `MakeFireBallEntity`, `fireball::Restore`: its place, size, row, the place it was made at,
whether rain cools it, its creation number and life, its record kept; no fire of its own, the fire is made from its
own record); once the effects are made again, each ball takes the effect its manager names and the player that
effect's spell casts for (`fireball::RestoreEffect`), as `fireball::Create` does. A ball whose effect is not made
again goes at the next turn, as a ball whose effect has gone does
(test `SavedWorldLoad.TheFireballsBallsArePlannedAndMadeAsTheirListHoldsThem`).

**The spells** (`src/ECS/SavedWorld/WriteSpells.cpp`, `RegisterSpellWriters`): each spell is written by the record of
its class (`components::Spell::spellClass`): the plain spell (0x9), the storm and the tornado (0x10) and the healing
spell (0x12) with every spell's values only; the food and wood spells (0xA) with whether their first grain has landed
(+0xEC, `magic::SpellResourceData::firstDone`: `SpellResource::SpellEvent` 0x724D80 pays the first event's amount
while it is 0, then sets it); the forest spell (0xF) with TreeAdded (+0xF0, `SpellForestData::forestCreated`), its
MagicForest (+0xEC, by the forest's key, none once the forest has gone) and MaxObjects (+0xF4, `maxTrees`,
`SpellForest::SetMaxObjectsToCreate` 0x7256C0); a creature spell (0x13, `SpellCreature::Save` 0x722590) with CreatureCastOn (+0xEC, `magic::SpellCreatureData::creature`, none once that creature is unavailable, as `SpellCreature::Process` 0x722380 clears it then; openblack's turn does not clear it, so the writer tests it) and the hand's pour flag (+0xF0, `handPoured`, set once by fn_00722400); the teleport (0x15) with its objects (`magic::spell_objects`, the
newest first: its stones, MagicTeleport 0x19, [written with the mobile statics](#writing-the-world-file); the load makes
it again, 2026-11-14: `SpellWithObjects::Load` 0x7210F0, its load (vtable +0xE8), reads every spell's values and its
objects, each added at the list's tail, so the list keeps the order it was saved in, and its constructor is empty, so a
loaded teleport spell is its values, its stones made again in its list's order and its record kept, as `MakeSpells`
makes it; it has no particle type, so no effect); the water spell (0xC, `magic::water::SpellWaterData`). Every field above comes from the component (its places
in map coordinates through `map_coords::ToFixed`; its particle system the interface of its effect, `SourceKey` 0xA5,
whose manager names its file by the particle type of its spell's class and names the spell as its owner; the casting
hand's status the creator's player's, for a spell cast from the hand). The game's list of spells is the live ones
(`magic::Spells`, the newest first) then those the load kept. Unread for one made in play, written 0: the three bytes
after the process info (kept for a loaded spell). A spell's reaction (read after its particle system, `Spell::Load`
0x721CD0) is named by its id (`SourceKey{ReactionRecord, id}`, as the reactions' writer keys it); one no longer in the
game's list stops the write. The shield spell (0x11, `SpellShield::Save` 0x72BC40) writes its objects (its world shield), its anti-influence rings
(`magic::SpellShieldData::rings`, the newest first) and its struck and shield reactions by their ids; its physical
shield (0x17) is written from `components::MagicShield` (its two matrices as it keeps them, `MagicShield::matrix` and
`previousMatrix`, as the original's process makes them; test
`SpellWrite.APhysicalShieldWritesItsValuesAndItsMatricesAsTheGameKeepsThem`, the matrix of profile D's slot 20
Land 2 shield bit for bit). The spiritual shield's world object (0x18, 2026-11-14,
`src/ECS/SavedWorld/WriteSpells.cpp` `WriteSpiritualShield`; test
`SpellLoad.ASpiritualShieldIsWrittenMadeAgainInItsListsOrderAndWrittenBackAsRead`) is written from
`components::MagicShield` as `MapShield::Save` writes it: the fixed object's parts of its row (2084) at its place on
the land (its target there, altitude 0), its object's scale (0.017 of its radius, `fn_0072C250`) and heading 0, its
spell and its magic's info (2643); before, a save with one up stopped as an object of no class. The load makes it again
with the physical shields (`saved_spells::PlanSpiritualShields`, `MakePhysicalShields`, `map_shield::RestoreSpiritual`),
both kinds from the game's list of map shields' last to its first, so the newest keeps the newest serial: its place,
its scale, its creation number and its record; `GameOSFile::LoadInstance` makes it with `MagicShield::MagicShield`
0x72C1F0 (the map shield's zeroing constructor) and its resolve is `Object::ResolveLoad` (MagicShield's vtable +0xF8),
whose creation puts it back into its cells; the shield spell made again names itself as its spell, as for the
physical shield. The flock spells (0xD flying, 0xE on the ground; 2026-11-11, `FlockOwn`, test
`SpellWrite.TheFlockSpellsWriteTheirFlockTheirCountsAndWhereTheyLastLetAnimalsOut`) write their animals (their
objects, the newest first), their Flock (+0xF4, `components::FlockSpell::flock`, none once gone), the animals made
(+0xF8 `created`), the emit count (+0xFC `emitted`), where they last let animals out (+0x100, MapCoords x and z and the
height above the land, `lastSpawn`, `lastSpawnHeight`) and +0x10C 0; the flying one adds +0x114, +0x118 and the byte
+0x11C, 0: only `SetToZero` (0x723210, 0x7239E0) writes +0x10C and those three (`xref.py field`). raffclar's reader
reads the same layout (`ReadSpellFlock`, `ReadSpellFlockFlying`). Their animals (SpellDove 0x90, SpellBat 0x95, SpellWolf 0x8D) are
written with the animals ([the spell animals](#writing-the-world-file)). A spell of a class with no writer that the list names stops the write
(`components::SpellClass`). The load makes the flock spells again too (2026-11-11, `RestoreFlock`, test
`SpellLoad.AFlockSpellIsMadeAgainWithItsFlockItsCountsAndItsAnimalsAndWrittenBackAsRead`), as `SpellFlock::Load`
0x724780 reads them: their flock, the animals made, the emit count and the last spawn point, and their animals the load
made, the newest first, each naming its spell; the flying one's cast effect (+0x110) is not saved, so it has none. The
creature spells too (2026-11-11, test `SpellLoad.ACreatureSpellIsMadeAgainWithItsCreatureAndOneOnACreatureNotMadeIsKept`),
as `SpellCreature::Load` 0x722520 reads them: CreatureCastOn (+0xEC) and the hand's pour (+0xF0) into
`magic::SpellCreatureData`; one cast on a creature the load did not make is kept as read, not made, so that it is
written back naming its creature. The plain spells too (0x9, 2026-11-12, the fireball's, the lightning bolt's and the
beam's explosion's; test `SpellLoad.APlainSpellIsMadeAgainAndWrittenBackAsRead`): `Spell::Save` is all they write and
openblack's plain spell keeps nothing of its own, so the spell's values and its effect made again are all of it; a
thrown fireball's effect comes back with them, and its balls take it (the fireballs' balls, below). The healing spells
too (0x12, 2026-11-14; test `SpellLoad.AHealingSpellIsMadeAgainAndWrittenBackAsRead`): `SpellHeal`'s load (vtable +0xE8)
is `Spell::Load` 0x721CD0 and `GameOSFile::LoadInstance` makes one with its empty constructor, so a loaded healing
spell is the spell's values and its effect made again, as openblack's keeps nothing of its own (its targets are its
effect's). The storm and tornado spells too (0x10, 2026-11-14; test
`SpellLoad.StormSpellsAreMadeAgainJoinTheStormSpellsAndAreWrittenBackAsRead`): their load (vtable +0xE8) is
`Spell::Load` too, and `GameOSFile::LoadInstance` makes each with `SpellStormAndTornado::SpellStormAndTornado`
0x72D900, which clears the whirlwind's effect (+0xEC) and the water reaction (+0xF0) (fn_0072DA10) and puts the spell
at the head of the storm spells (0xDA07F8, count 0xDA07FC); neither is saved, so a loaded storm has no whirlwind and no
water reaction, and the loaded storms join that list in their objects' order (`spell_storm::AddLoaded`), the newest
first. The forest spells too (0xF, 2026-11-14; test
`SpellLoad.AForestSpellIsMadeAgainWithItsForestAndOneNamingAForestNotMadeIsKept`), as `SpellForest::Load` 0x725D50 reads
them after `Spell::Load`: TreeAdded (+0xF0, `SpellForestData::forestCreated`), MagicForest (+0xEC, by `ReadPtr`: the
key of the forest the load made of that record, `SavedWorld/LoadSpells.h` `SpellLinks::forestOf`) and MaxObjects
(+0xF4, `maxTrees`); `GameOSFile::LoadInstance` makes one with its zeroing constructor (`SetToZero`, TreeAdded only),
so nothing else is kept. One naming a forest the load did not make is kept as read, not made, so that it is written
back naming its forest. The
load makes the water spells again (2026-11-11,
`saved_spells::PlanSpells` / `MakeSpells`, src/ECS/SavedWorld/LoadSpells.cpp): every spell class's ResolveLoad is
`GameThing::ResolveLoad` 0x405250 (empty), so a loaded spell is its saved values and its place in the game's list,
nothing started again: each from the list's last to its first (`magic::RestoreSpell`, pushed at the front), its
effect made again from its records (`RestoreEffect`, adopted as its own with the spell as its sink), its last ring's
age, its seed's link, and its record kept; made before the reactions, which then find it as their initiator, and
given its reaction by the id the reaction was made again with. A loaded spell writes its places as it read them while
they stand (metres to map coordinates and back is not always the same coordinates), and the list writes a kept spell
made again once. The water spell's putting-out-a-fire reaction (`SpellWater` +0xF0,
`SpellWaterData::puttingOutFireReaction`, made by `Object::ApplyWaterSpell` 0x63A8E0) is not saved: `SpellWater::Save`
writes only the last ring's age (+0xEC, 0x725448), and `GameOSFile::LoadInstance` calls `SpellWater::SetToZero`
0x724EC0 (0x559123) before the load, which clears it, so a loaded water spell has none, as here; the reaction's own
record, when the save holds one, is made again with the spell as its initiator. The shield spells are made again the same way (2026-11-11), each after its
physical shields (0x17, `saved_spells::MakePhysicalShields`, from the game's list of map shields, the newest with
the newest serial: `map_shield::RestorePhysical`, its values as `PhysicalShield::Load` 0x72C410 reads them, its two
matrices as saved, its effect made again with the shield as its target and stepped by it), which it keeps as its
objects and names as their spell; its anti-influence rings; its struck and shield reactions by the ids they were
made again with. Its town (+0xFC, `SpellShieldData::town`, set by `SpellShield::InitWithPos` 0x72B5F0) is not saved:
the load constructs the spell with `SpellShield::SpellShield` 0x72B3F0, whose fn_0072B3C0 puts it at the head of the
shield spells and clears +0xF4..+0xFC, so a loaded one has none, and the loaded shield spells join that list in their
objects' order (`spell_shield::AddLoaded`). Tests `SpellWrite.*`,
`SpellLoad.*`, `SpellLoadFrozen.*` (slot 20's water spell 7382, its effect 7383/7384 and its reaction 7392; its
shield spell 7393 with its physical shield 7395, effect 7396/7397, rings 7526/7527 and reaction 7528).

**The influence rings** (`src/ECS/SavedWorld/WriteInfluenceRings.cpp`, `RegisterInfluenceRingWriters`): a ring (0x49)
is written from its entity (`components::InfluenceRing`), as `InfluenceRing::Save` 0x5CDC60 writes it: its place as map
coordinates (`map_coords::FromWorld`, as `MapCoords(LHPoint)` 0x603160 makes a script's: the height above the ground),
the object it follows while that object exists (0 for a fixed ring, and once the object is gone), its player (the
players' source, `PlayerObject`), its radius and its anti word, which the ring keeps as its maker gave it (the scripts'
popped word, 1 for a shield's). The game's list of rings is the live ones (`influence::Rings`, the newest first, as
`InfluenceRing::Create` pushes them) then those the load kept. Read for one made in play: the thing's words are a new
one's (`GameThing::GameThing` 0x56FA10 leaves `destroyed` and the flags byte 0, and a listed ring is never deleted: its
`ToBeDeleted` 0x5CD8A0 unlinks it first), and the place's flags word is `SetToZero` 0x5705D0's 0 (no ring function
writes it). The place's height is never read by a ring (only written and copied from its object). No load makes the
rings yet: a loaded ring is written back from the kept world (tests `InfluenceRingWrite.*`).

**The creature's own values** (`src/ECS/SavedWorld/WriteCreatures.cpp`, `CreatureFieldsOf`), as `Creature::Save` 0x4E4EA0 writes them after `Living::Save`: the version 33; saved in the
temple (`ViewMode` 1, set only by `GoInsideCitadel`: `game_clock::IsInsideCitadel`); its place in the game's list of
creatures (0xFF when absent) and its player; the 16 slots (`TPerSpellData`: the type, 16 as the constructor 0x4F5240
leaves it, for nothing else writes it; the phase, the turns left, the value found, the hold and the miracle, from
`CreatureSpells`) and those waiting (the type, the seconds as given, the miracle: openblack keeps a waiting spell's
turns, not its seconds, so a creature with spells waiting stops the write); the mind's word (0 while a freeze
holds the mind, `CreatureMindState::paused`: its other writers, packets 55 and 56, are the developer's lesson chooser's
and not reached); the largest and smallest sizes (`CreatureSizeLimits`, else the defaults); the vortex byte; gone for
good, then the fizz, its target and its rate (`CreatureFizz`), all three 0 while the invisible slot is off, as the save
sets them on the creature too (`ZeroFizzForSave`, for the save to call: saves only, which no check reaches); the freeze
(+0x12BC, the freeze's ease); IgnoredByVillagers (+0x12C0, read by `Villager::ReactToCreaturePriority` but written only
by the constructor and the load: 0); the leash's target (the point, the thing, the flag); the home (the temple's pen
while there is one, as `ProcessState` 0x472E6F sets it each turn, else the home the leash keeps,
`CreatureLeash::home`, each as loaded while it is still the place the loaded map coordinates name); the fight health out of a
fight (`CreatureFightHealth`, else the constructor's 1.0); the auto fight out of a fight (0: openblack keeps it only in a
fight); can die, strength, size; the alignment, for the local player's creature while a task of "CreatureCurse" runs
the global "OriginalAlignmentOfMyCreature" (the script library's `GetFirstRunningTaskId` 0x100083B0: a task whose
script's name matches letter for letter, any state, answering the highest task id; `GetGlobalVariableValue`
0x10005190: the first global of the name, its four bytes as a float, else 0.0); the animation pace (1 − the freeze, as
the freeze's ease 0x4F54C3 writes it); the auto scale and its share (`CreatureAutoscale`); the plan's flag (a script's
control, a fight or dancers) and, for a creature a script holds, its plan (`ScriptHeldPlanOf`): a script's play of an
animation keeps the plan `PrepareCreatureForScriptedAction` 0x4F6A90 makes (desire 24, action 22, the act-on object
the creature itself) and the agenda `ScriptPlayIndividualAnimation` 0x4F6E30 / `ScriptPlayStaticAnimation` 0x4F6F10
build (sub-action 10 with the clip, or 4 with the clip and 10.0, once per play, then 0x85), and a script's move
(`MOVE_GAME_THING`, `ScriptMoveToPos` 0x4F6B60: its two MoveToPos 8 of the script's point, its height kept with the
walk, `creature_mind::MoveForScript`, within a sixth of the way and then within its height), from the step it is at: state
1 and the agenda's first word 0 once the step is under way (stage 0 of 10 starts the clip, 0x501C90; stages 1 and 2
wait for the body, 0x501CD0), else state 0 and 1, as `ClearSubActionAgenda` 0x4FF5C0 leaves them; the main step
0xFFFFFFFF; its own plan of sleeping on the spot that found no clear place (row 313, `OwnSleepPlanOf`, 2026-11-12), the game's plain sleep `ConstructSubActionsForSleep` (bw1-decomp `CreatureAction.cpp`): its desire and row, a yawn 10 (57) or not, the walk home 8 (its point on the land there, within its height or 5), the face down the slope 0x44, the sleep 0x84 as the main step and the wake 10 (63); its own plan of fishing and eating the catch (row 155, `FishAndEatPlanOf`, 2026-11-12), `ConstructSubActionsForFishAndEat` 0x4AB350's four sub-actions, the eat the main one (+0x1C = 3): the walk 8 to the shoal (its centre's height, or 0 for the place `SpiralCheckForValidPoint` finds, within its height, 15 at least), CreateFishFromSea 0x5C, PickupCreatedObject 0x37 and EatCreatedObject 0x80; openblack's one fishing step is the game's 0x5C until it starts and its 0x37 once started (census save 8, 16/3, held such a creature); its own plan of looking over a miracle's place (row 157, `ExaminePlacePlanOf`, 2026-11-12), `ConstructSubActionsForExaminePos`'s agenda: pointing first (one draw in two) TurnToFacePos 7 and PointAtPoint 0x50 (1.0), then the walk 8 within five heights, TurnToFacePos 7 and the puzzled Wait 0x18 (2.1 s in turns) as the main step; else the walk, the turn and PointAtPoint (2.0) as the main step; every point the place on the land (census save 8 at frame 5000); one that found a clear place is not written yet (its point's height not read); a creature it holds with no plan and nothing left keeps the plan a finished one leaves (desire 40, the
agenda cleared). Not reproduced, written 0: the words of a step its argument does not set (`AddSubAction` 0x4FF240
sets only the action, the belief, the argument's words and +0x2C, so the point, +0x20, +0x24, +0x28 and +0x30 keep
what the slot held before) and the agenda's +0x18, which only the constructor's memory, the save and the load touch.
A sit-down a script forces (`CREATURE_DO_ACTION` row 81 SitDown, `ScriptDoAction` 0x4F7000: desire 24, the row,
the activity, act-on and use objects) keeps `ConstructSubActionsForSitDown` 0x4A4AB0's four sub-actions, the main
one the fourth (+0x1C = 3): TurnToFaceObject 6 (the object's belief at +0x34, 0.1 at +0x48), FaceDownSlope 0x44 and
StaticAction 4 (clip 38 at +0x38, the drawn seconds at +0x48; the argument classes' `SetArgumentOfSubActionAgenda`
0x4FF690..0x4FF860 put an object at +0x34, an integer at +0x38, a point at +0x3C and a float at +0x48 of the
sub-action), from the step it is at, with the walk to the thing first: MoveToPos 8 of the thing's point
(`GetLHPoint` 0x605C40: x and z × 1/6553.6, the land's height there plus its own altitude; openblack keeps the
thing's height as the plan is made, `Movement::pointHeight`) within `Get2DRadius`. One of other steps stops the write. So are the other rows the first two lands' scripts force, each with its builder's
sub-actions and main one: LookForever 189 and LookButDontApproach 188 (0x4B0D50, 0x4B0970: TurnToFaceObject 6 of
the object for 2.0, then the main Wait 0x18 of `ConvertRealWorldSecondsToGameTicks` turns at +0x38, 86400 or the
drawn seconds), PointAtObject 163 (0x4AEB40: the main 0x51 of the object for 5.0), LookAtCamera 82 (0x4A4ED0: the
main 0x42 of 5 seconds of turns) and PointAtCamera 168 (0x4AF760: TurnToFaceCamera 0x1F, then the main 0x50 at the
camera's place for 1.0), WaveAtPlayer 90 (0x4A5E00: TurnToFaceCamera 0x1F, then the main IndividualAction 0xA of
clip 0x48) and BeIdle 161 (0x4AE8E0: twice a Wait 0x18 of `1 + GameFloatRand(1.0)` seconds in turns and an
IndividualAction 0xA of clip 0x39, each `AddMainSubAction`, so the main one the fourth). The creature guide's, which
runs on every land: BeFrightenedOnTheSpot 174 (0x4B0250: 0xA of clip 0x3D, Wait of 3 s, the main 0xA again),
WaveAtObject 91 (0x4A5EB0: GoNearObject 0x3D of the object within 3.5 × its height, TurnToFaceObject for 1.4, the main
0xA of clip 0x48), SmileAtFriend 60 (0x4A2190: GoNearObject within 3 × its height, the main TurnToFaceObject for
4 + `GameFloatRand(4.0)`, then at random 0xA of clip 0x37) and HoldObject 64 (0x4A2BC0: PickUp 0 of the object unless
it carries one, the main TurnToFaceCamera 0x1F, HeldObjectAction 3 of `GameRand(4)` + 100 while it has done so less
than twice for the kind, a Wait of 40 + `GameFloatRand(10.0)` s) and ExamineByLooking 20 (0x49BA70: GoNearObject, TurnToFaceObject for
2.0, Wait of 1.1 s, the main ObserveObject 0x3E of `CreatureBelief::GetType` 0x4D63F0, the belief's type attribute,
`GetCreatureBeliefType` clamped below 24: openblack's `Belief::kind`) and SleepAtHome 17 (`ConstructSubActionsForSleep`
0x49B1C0: 0xA of clip 0x39 at random, MoveToPos 8 home within min(5, its height), FaceDownSlope, the main Sleep 0x84,
0xA of clip 0x3F; the walk's point is `ConvertMapCoordToLandscapePoint` 0x613750 of HomePos, the land's height there plus
HomePos.altitude, which every writer of +0x1208 sets to 0 (the constructors, `Create` 0x474AF6, `ProcessState`
0x472E9F): openblack keeps the land's height at its home with the plan, `Situation::homeAltitude`). The later lands' Hurl 15 (`ConstructSubActionsForHurl` 0x49ADF0: 0xA of clip 0x35 at
random, PickUp of the object used unless it carries one, MoveToThrowPos 0x10 of the target within its height, the
main Throw 5 of the target), DiePermanently 217 (0x4AC3B0) and DeadForever 261 (0x4BAAF0: the main Faint 0xE of clip
0x6A, then a Wait of 999999 or 864000 s in turns; `Creature::Save` writes DeadForever's agenda from its first step,
setting its current one to 0), LookAtHand 8 (0x49BA10: the main LookAtHand 0x12) and PutDown 67 (0x4A3720: Discard 1
of clip 0x61, no main step, 0xFFFFFFFF as `ClearSubActionAgenda` leaves it) and SleepByObject 78 (0x4A4610:
MoveToPos to the clear place by the thing, 0xA of clip 0x39 on `GameRand(2)`, FaceDownSlope, the main Sleep, 0xA of
clip 0x3F; still walking it stops the write), Poo 19 and Fart 177 (`ConstructSubActionsForPoo` 0x49ABB0, the fart's
0x4B04C0 calling it: 0xA of clip 0x42 on `GameRand(3)`, GoNearObject 0x3D of the thing within 0,
TurnToFaceAwayFromObject 0x16 of it, the main StaticAction 4 of clip 0x20 for 4.0, then Poo 0xF with no argument;
`creature_mind::PooBy`) and EatFromFoodPile 158 (0x4AE450: MoveToPos 8 to the pile's point, its height
`GetAltitude` of its MapCoords plus its altitude +0x1C, within (both 2D radii) × 1.2, TurnToFaceObject for 0.1,
CreatePickUpThenRemove 0x8A of the pile and 0x11, the main EatCreatedObject 0x80; openblack keeps the pile's height
with the walk, `Situation::objectAltitude`), EatAlive 11 (`ConstructSubActionsForEatAlive`: unless it carries a thing
it can eat, 0xA of clip 0x36 on `GameRand(2)` and PickUp 0 of the thing, then the main Eat 2 of the thing) and
StompAndEat 13 (`ConstructSubActionsForStompAndEat`: unless it carries a thing it can eat, Stomp 0x11 and PickUp 0 of
the thing, then the main Eat 2 of it) and RaiseTotemPole 56 (0x4A1BD0: GoNearObject 0x3D within 12.0, TurnToFaceObject
for 0.1, 0xA of clip 0x34, the main RaiseTotem 0x31 of the pole; `creature_town_help::MoveTotem`) and four casts,
CastFireballPU1 96 (`ConstructSubActionsForCastFireball`: MoveToPos to a clear place, still walking it stops the
write), CastLightningBoltPU1 99, CastHealSpellPU1 102 and CastMakeCreatureItchy 120 (the lightning, helpful and playful
constructs above in [casting](../creature/casting.md#the-constructs)): 0xA at random, GoNearObject 0x3D,
GetAwayFromObject 0x87 (object and distance), TurnToFaceObject for 0.1, Gesture 0x58 of the miracle's gesture when it
has one, and the main CastSpellAtObject 0x27 that `AddCastSpellSubAction` 0x4FF4B0 adds (the object, clip 0x29, 3.0
and the magic type; `creature_mind::CastAt`, `CastFireballAt`); the main step is
the one openblack marks A knocked-out creature a script holds keeps the Die action 9 every faint forces (`Creature::Faint` 0x476FA0,
`ConstructSubActionsForDeath` 0x49B770; `KnockedOutPlanOf`, from `CreatureKnockedOut`): desire 24, the thing it is done
to (itself for a faint), the main FAINT 0xE of clip 0x6A, WAIT 0x18 of the lie's turns left, TRANSPORT_HOME 0x70 for a
player's creature, WAIT of 3 s in turns, WaitForSpellsToWearOff 0x8B and RestToGetBetter 0x82 when it was decided.
One stage a turn (stage table 0xBE0BE8: FAINT 0x5022D0 / 0x502320 / none, WAIT none / 0x503500 / none): falling, step
0 at stage 1; the turn it is seen lying, stage 2; from the next, the WAIT at stage 1 counting down; the turn its count
is out, the carry home at stage 0, not started (first word 1). Carried home or later it stops the write. A creature in its duel (2026-11-11) keeps its fight's plan as
`ConstructSubActionsForFight` 0x49BCD0 lays it and its orders leave it: the creature that started it (it made the
arena) at the main step, FIGHT 0x1C (stage 1, under way), of the eighteen (its twelve own and six ORDER_CREATURE 0x4A,
each with the partner +0x54 and the ordered step +0x58), with the ORDER of RESPOND_AFTER_FIGHT 0x48 to the opponent,
its own RESPOND_AFTER_FIGHT and REMOBILISE_OBJECT 0x4D ahead; the opponent as the order's stage 0x5062F0 left it,
plan desire 26 / action 124 (ObeyCreature) about the starter, its agenda cleared and then the ordered FIGHT and
WAIT_FOREVER 0x4E on the starter; both agendas' partner the other, as RequestPartner 0x503A90 sets it and
`ClearSubActionAgenda` leaves it (+0xC40 is not cleared); the arena only the starter names; the fight health its
fighter's; who fights for it 0, 1 or 2 (`AutoFightOf`). The 3D body's fight is not saved: a loaded duel starts again at
FIGHT's start. Before or after its duel the save still stops (`DuelPlanOf`). The load (2026-11-11, `DuelOf`, the
pairing after the creatures are made): the original's load restores the agenda but not the 3D body's fight, so FIGHT's
running stage 0x503830, with no body action under way, returns 2 at once (the player's creature also ends the camera's
fight watch, `CameraModeNew3::EndFightSoon`): no winner, no won-fight reaction, no `EndFighting`; the starter orders
RESPOND_AFTER_FIGHT, and each plays its response, 0x37 or 0x38 by the two lives (0x5061B0), and is let go. openblack
makes both creatures at `CreatureFighting::Stage::Respond` with nothing paid (`ended`), each fighting the other, the
starter with its arena's place and radius from the arena's record, their saved fight health and who fights for them
(`AutoFightControlOf`); one whose opponent is not made fights no more. The starter's plan is its saved one, the fight (row 21) with its opponent by the desire the save kept, taken up once its mind is set up (`CreatureMindState::loadedFightDesire`, 2026-11-12), as the original's load restores the plan with the agenda; so a save made while the loaded duel's response is under way writes the starter's fight again (`DuelPlanOf`), where before it stopped ("a fight its mind's plan is not of", the census's save 10). The arena's record is made as the list's other
arenas are.
(`Step::main`). Any other row names its row as the write stops. Any other plan a script holds a creature on still stops the write; the four creed powers (`CreatureHandGlows`, 0 and 0 without a glow, fn_004F8AE0). The
miracle it holds as it casts (+0x1078, `CreatureCasting::held`), two visuals nothing sets in play (+0x1084, +0x1088:
only the constructor, the load and `DestroySpell` / `ProcessSpells`'s clears write them: 0 as made), the beams at its
hands of a cast from above (+0x107C the mirror hand's, +0x1080 the right hand's, `InitialUpdateSpellInfo` 0x4F8590;
`CreatureCasting::beams`) and the leash's mark (+0x11E4) are written by their spot visuals' containers' keys while
those are there (`SpotVisualObject`), else 0, as `ProcessSpells` and `CreatureReceiveSpell::Process` let them go. Not
written yet, and the write stops naming the creature: a fight (its arena and status), a dance, and a plan (tests
`CreatureWrite.*`, `CreatureWriteCurse.*`).

**The creature's class writer** (0x69, `RegisterCreatureWriters`, claiming every `components::Creature`): the record
its load kept, else a new one, with:
- the thing's flags as kept, 0 for a creature made in play: openblack does not hold yet the 0x40 a script's taking
  sets (`GAME_THING_FLAG_NAME_HIDDEN` by name, but its one writer is `GScript::AddScriptGameThing` 0x70D0F0,
  `or byte [thing+0xA], 0x40` at 0x70D144, whenever the command takes the thing, and nothing clears it);
- the place (`PositionPartOf`); a creature in the physics stops the write, naming it (its body there is not written);
- the object (`ObjectPartOf`) with: the info id, 2517 less the species' row (the creature rows, as
  [the load](#openblack) numbers them); its place, `coords` (+0x2C), where it stood as its last turn began
  (`Living::ProcessLiving` 0x5EC810 copies Pos into it for every living thing, creatures too, before its turn;
  `components::DrawPosition::turnStart`), as kept while that is the loaded place, else where it stands; its scale (+0x50) the start size of its species within the body's limits (`ClampScale(startScale)`): only
  `SetJustScale` writes +0x50 and a creature's only caller is `Object::CallVirtualFunctionsForCreation` 0x636BE0's
  `SetXYZAnglesAndScale(..., GetScale())` from `Creature::Create` 0x474A20, where `GetScale` is the 3D body's size
  `CreaturePhysical::CreaturePhysical` 0x4EF2C0 has just set from the row (+0x1FC); `Creature::SetScale` 0x47B160
  changes only the body. Its angle (+0x4C) 0 as made: a creature's turns are its body's (0 in all 46 frozen
  creatures); as kept for a loaded one;
- the moving thing's, the walk's and the living thing's parts (`LivingPartsOf`, made as `LivingConstructorParts` of
  its species' row), then its own values above. A kept flock (Living +0xB8) gone since the load is none: a deleted
  flock takes itself off every member at its delete (`Flock::ToBeDeleted` -> `Flock::DeleteDependancys` 0x52FE60 ->
  `Living::SetFlock(0)`; 2026-11-11, test `CreatureWrite.ALoadedCreatureNamesNoFlockGoneSinceTheLoad`). The other
  kept references are left: the script reminder (+0xB0) is the creature's own (deleted only in
  `Living::ToBeDeleted` 0x5EC0A0); the walk's target (MobileWallHug +0x60) is never cleared (only
  `SetupMobileMoveToObject` 0x60ACD0, the constructor and `SetToZero` write it); the circle's object is let go at the
  object's delete (`CircleHugStateInfoT::OnDeletionOfNewcollideObj` 0x60A680), but with a new sweep from the walker's
  place and a new move state, which a kept record cannot give; the visuals +0x1084 (the creature's own, cleared by
  `DestroySpell` 0x4F81F0 as it deletes it) and +0x1088 (cleared by `ProcessSpells` 0x4F8B80 once its container is
  not available) are particle sources, not entities a delete can be seen on. A creature with no row of its species (no game tables) stops the
  write, naming it. Tests `CreatureWrite.TheClassWriterWritesACreatureMadeInPlayWhole`,
  `CreatureWrite.ACreatureWithNoRowOfItsSpeciesStopsTheWrite`, and `FlockLoadWorld.*` (the frozen Land 2 autosave's
  creatures made by the load and written back, each as its record keeps it).
- A loaded creature keeps its kept scale, angle and place. `Object::ResolveLoad`
  0x63A020 calls `CallVirtualFunctionsForCreation` too, so the original takes the body's size again at the resolve;
  what that size is then is not read 🔍 (the frozen saves hold 2.0 for some creatures and 0.22..0.27 for others, the
  same creature 0.26 in one save and 2.0 in the next).

### The saving banner

✅ The original: `GameOSFile::SaveAllGame` 0x558160 (GameOSFile.cpp:339), the last step of `InstantSaveGame` 0x792FB0
after the pictures and SaveGame.inf, first calls `GameSavingBox(false)` 0x557FC0 (0x55816D), and `GameSavingBox(true)`
(0x55868B) once the file is written; `GameLoadingBox` 0x557F90 does the same for the loads with text 6775. Both call
`LoadSaveBox` 0x557D10 with help text 6810 (`HELP_TEXT_DIALOG_ADDITION_120`, "Guardando..." in the frozen
InfoScript2.txt; the size check against 0x1A9A at 0x557FC0). With `finished` set it draws nothing. Otherwise, only once
the game is initialised: `g_game_time_inc` and `TimeInc` set to 0, `LH3DRender::StartFrame` 0x82F0E0 (no clear),
`DrawAlpha` 0xFF, then over the last frame left in the back buffer:

- unless `SaveGameRoom::CurrentSlot` is 15 to 17 (the autosaves' circle; `cmp 0xF` / `cmp 0x11` at 0x557DB0), the whole
  screen darkened: `DrawBox(-1, -1, w + 1, h + 1)` 0x78000000 at its four corners;
- the band of the script map's "please wait" banner
  ([the loading screen](../video/playback.md#startup-and-loading-screen)): top `h/2 - 30`, bottom `h/2 + 30`, edge
  `ftol(w × 0.4)`; shadows 15 high above and below (0x9F000000 fading to 0), the ends
  0xFF202020 to 0xFF404040 and the middle 0xFF404040, `DrawLine` 0xF0F0F0F0 along its top and bottom;
- the text with `DrawTextWrap` 0x411750 at (101, 201, 701, 401, start 401) in black, then at (100, 200, 700, 400, start
  400) in white, big text (35), centred both ways;
- `LH3DRender::FinishFrame` and `LHScreen::Flip`. The game then writes the file in the same call, so the banner stays on
  the screen until the next frame after the save is drawn.

openblack: `saved_slot`'s save sends `events::GameSaving` with the slot once a slot folder is there (a profile is kept;
the checks keep none, so they never show it), before the world file is made. `Game::PresentSavingBanner`
(`src/GameStartup.cpp`) then, between the game's frames only, draws `LoadingScreen::DrawSaving` (the "please wait"
banner's drawing, `loading::SavingDarkens` and `SavingDarkening` in `src/Gui/LoadingScreenRules.h`, tested in
`test_loading_screen_rules`) through `RendererInterface::PresentOverShownFrame`: the frame kept on the screen (every
frame is kept while a profile is kept, for the saves' pictures) copied to the window again, the banner over it in the
Cursor view, and the frame handed over at once, not counted, so the pictures of the frame on the screen are still of
the game's frame. Debug: the debug bar's Save menu, "Show the saving banner" (at the next frame's start). Differences
left: the saves the debug bar asks write while it draws, and show no banner; with the frame not kept (the multisampling
switch, a renderer without read-backs) no banner shows.

The banner's clearing of the frame's time (read 2026-11-13, bw1-decomp `GameOSFile.cpp` `LoadSaveBox`): before it
draws, `LH3DTech::g_game_time_inc` and `GGame::TimeInc` go to 0. They stay 0 only until the loop's frame clock sets
them again (`GGame::Loop`, step 3 of [the original loop](loop.md#the-original-loop)), so only what reads them between
a save in a turn and that frame's clock sees it. Every save in play runs in a turn (step 2): the quick save in the
turn's input packets (`GPacket::ProcessPacket` 0x63DC88), the autosaves and the script saves in `ProcessGameCode`.
The readers a game turn reaches by direct calls (`tools/xref/xref.db`'s calls from `ProcessOneGameTurn` 0x54D620 to
every reader of [0xEA9EC0], 2026-11-13) are the interface's power-up pass and what it calls, `ProcessPowerUpSystem`
0x5CF300 (the gesture cooldown, 0x5CF42F, and its +0x194 clock, 0x5CF48B), fn_005CFAE0 0x5CFAF8 and
`DoLeashSelectionSystem` 0x5D0235 (the leash selection's clock +0x374), and the banner's own frame
(`LH3DRender::StartFrame` / `FinishFrame`'s per-frame counters, which openblack's banner, drawn over the kept frame, has
no counterpart of). The power-up pass comes in `GInterface::Process` after the turn's packets, so only a quick save's
turn gives it no time; an autosave or a script's save comes later in the turn, after the pass, and changes nothing.
`TimeInc`'s readers (`CameraModeNew3::Update`, `CHand::GetRequiredState`, `Tree::PreDraw`) and `GetCameraTimeInc` run
in the frame, after the clock. openblack: `Game::PresentSavingBanner` first calls `hand_casting::OnSavingBanner`,
whose `TurnTime` gives the turn's power-up pass (`gestures::ProcessPowerUpSystem`, the gestures and the leash
selection) no time until the next frame's (`hand_casting::Update`); test
`HandCastingTurnTime.ASavingBannerLeavesTheTurnsPowerUpPassNoFrameTimeUntilTheNextFrame`. Debug readout (the game log at debug): "Saving banner: the turn's power-up time cleared until the next frame's" as the banner draws, and "Power-up pass after the saving banner: the gestures and the leash selection read 0 s" at that turn's pass. Readers through virtual
calls were not followed one by one; the over-approximation (every slot of each offset) adds only draw code.

### The loading box

✅ The original never shows a "Loading..." box on any load of play (read 2026-11-12 in bw1-decomp and the v1.42
Ghidra export). `GameLoadingBox` 0x557F90 (GameOSFile.cpp:299) is the saving banner's drawing with
`HELP_TEXT_DIALOG_ADDITION_85`, but `LoadSaveBox` 0x557D10 draws only when `GGame::g_game->Initialised` is set and
`GGame::Initialising` is clear, and every load a player makes runs inside `GGame::Init`:

- `GameOSFile::LoadAllGame` 0x5587B0, which calls the box first, has three callers: `GGame::Init` (start type
  `GAME_START_TYPE_LOAD_GAME`, `LoadGameFilename`), `GameOSFile::AutoLoad` 0x563FF0 (called only by `GGame::Init`, the
  start's autosave, slot 20) and fn_00636120, a network game's debug load of `NetDebug%d.sav` from a packet.
- `GGame::Init` 0x54F3B0 sets `Initialising` to 1 near its start (Ghidra's FUN_0054f421 line 30) and back to 0 only on
  its two returns (lines 113 and 424), both after the two loads (lines 139 and 148).
- The quick load (packet 0x62) and the save game room's load (sub_54C9F0) both go through fn_00793380, which only sets
  `LoadGameFilename` and game mode 4; the play loop ends and `PCMain` starts the game again with
  `GAME_START_TYPE_LOAD_GAME`, so the load is `GGame::Init`'s, under the tips' loading screen.
- The box's other caller, `GGame::ProcessGeorgeKey` 0x641080, a developer's key handler, has no caller in the exe.

So only the network debug load could show it. raffclar's tree draws the box before each load (his commit 46109a7d5,
`SaveGameSystem::Load` → `Game::ShowSavingBox`); it is not taken
([not in the original](../project/not-in-the-original.md#enginesaving_and_loading)). openblack shows none, as the
original: its loads (`Game::LoadSavedGame`: the quick load, the room's load, the debug bar's) draw no box.

What a saved game's load shows instead is `GGame::Init`'s tips screen (bw1-decomp `Game.cpp:276`): a new tip and its
picture when none is up (`StartTipOfTheDayText`, `MakeTipVideo`; the last load's end cleared it), then the redraw points
`RenderLoadingFrame` before the players, the data tables, the map's initialisation and the map script, one in each
object's `GameThing::Load` while the file is read, and six after the load, up to `LoadingFrameEnabled` = 0; no fade-in
(that is only the first tip's). openblack's `Game::LoadSavedGame` shows the same screen
(`loading::LookOf(LoadKind::SavedGame)`, test `LoadingScreen.ASavedGamesLoadShowsTheTipsScreen`), drawn again after the
screen starts, after the map's clear, after the landscape, after the objects, after the scripts and before the end;
each redraw waits its 150 ms, as the original's. Before, it started the screen but drew no frame of it, so the last
frame of play stayed frozen on screen while the save loaded.

### The quick load

The original (v1.42 exe; bw1-decomp `PCInput.cpp:791`, `Game.cpp:2909`):

- **The key**: `GGame::DoAction` 0x63F940, action 0x1E (`BINDABLE_ACTION_QUICK_LOAD`): outside a network game, the game
  not over (+0x205A10), and |`InstantLoadTurn` (+0x205A1C) − `GameTurn`| > `INSTANT_SAVE_MINIMUM_TURNS` (3; the exe
  compares the absolute signed difference with 3, `Game.h:42`): `SetPacket(0x62)`. Unlike the quick save, there is no
  wide-screen test, and the key sets no turn of its own.
- **`InstantLoadTurn`** is 0 from `GGame::ClearVariables` 0x54BF20 (called only by the game object's constructor) and is
  set only by `GGame::Load` 0x554830, to the loaded game's turn (Game.cpp:2909); no save writes it. So in a game that
  has loaded nothing the key does nothing in its first 3 turns, and after any load for 3 turns either side of the
  loaded turn.
- **The packet**: a single player's packet is applied by the next game turn (`ProcessOneGameTurn` →
  `ProcessGameInputs`); a paused single player game runs no turn (`LocalTimerSaysDoATurn` returns false while
  `GAME_FLAG_PAUSED`), and inside the temple the world is paused, so a key pressed then waits for the world's next
  turn. `GPacket::ProcessPacket` 0x63C420 case 0x62 (0x63DCB6): `MemoryState` = 0, then `push 0x13` and call
  fn_00793380: slot **19**, the quick save's.
- **fn_00793380(slot)**: `SaveGameRoom::CurrentSlot` = slot, the slot's folder (`%s\%i`, with the land's subfolder for
  15 to 17, the land before for 14 and the land after for 18), the map pictures copied from the slot into the current
  game's folder, `LoadGameFilename` = `<folder>\19.sav`, and only when that file exists (`LHOSFile::Exists` returns 0)
  `GameMode` = 4 (`GAME_MODE_LOAD_GAME`). With no file nothing else happens and the game goes on.
- **When it loads**: game mode 4 ends the play loop at the end of that iteration (the turn's game code, the draw and
  the flip run first); the end-of-loop save into slot 20 is skipped in game mode 4 (Game.cpp:1928), and `PCMain` starts
  the game again with `GAME_START_TYPE_LOAD_GAME` (PCMain.cpp:687), which loads `LoadGameFilename`.
- `GGame::ProcessTurn` (Game.cpp:2449) also sends packet 0x62 after the low-memory quick save (`MemoryState` 2), under
  the same turn rule; it is not ported (openblack has no object heap).

openblack: `save_triggers::QuickLoadRefused` (the rule, pure; test `SaveTriggers.TheQuickLoadWaitsMoreThanThreeTurnsFromTheLastLoad`)
and `save_triggers::QuickLoadKey`, called by `Game` as the key goes down beside the quick save's; the turn of the last
load is kept by the save service (`SaveGameSystemInterface::LastLoadTurn`, 0 as it is made, set by
`Game::LoadSavedGame` to the loaded turn). The packet's handler publishes `events::QuickLoadAsked` (test
`SaveTriggersInPlay.TheQuickLoadKeySendsAPacketThatTheTurnTurnsIntoTheLoad`), and the game loads slot 19 through
`Game::LoadSavedGame` at the start of the next frame's input, so the frame that applied the packet ends with its world,
as the original's loop does, and no input is read between. A second press while a quick load still waits for its turn asks for nothing more
(`QuickLoadRefusal::AlreadyAsked`; test `SaveTriggersInPlay.TwoQuickLoadPressesBeforeTheTurnGiveOneLoad`), as the
original's `SetPacket` writes the player's one packet again. A slot with no loadable save loads nothing and the game goes
on. Each press and load logs one "Quick load:" line. **Debug readout**: Load Island > Saved Games shows the key's last
outcome (refused and why, asked for, loaded, or could not be loaded), only once the key was pressed. Not checked:
whether the original's key is read at all inside the temple or with the menu open (openblack reads it as it reads the
quick save's).

### The puzzle games

The original (v1.42, `PuzzleGame::Load` 0x6D9D40 and `PuzzleGame::Save` 0x6D96C0, `HanoiBlock::Load` 0x6DB960 and
`HanoiBlock::Save` 0x6DB9F0; the layout above, under the classes the players do not reach). A load makes the
object with its constructor of no arguments (the count of calm turns +0x40C = 5 there; the start a script's CREATE
gives, the tree puzzle's pulls and the Hanoi puzzle's pieces, is not run) and reads, after the GameThingWithPos part:

| Record field (`PuzzleGameRecord`) | Puzzle member | What it is |
|---|---|---|
| `words[0]` | +0x48 | the type |
| `words[1]`, `words[3]` | +0x57C, +0x580 | 0 in every save read |
| `words[2]` | +0x584 | the play has begun (`GetPuzzleGameStatus` 0x6DC070: 4 in progress, else 5) |
| `values` (count 6) | +0x3CC..+0x3E0 | the totem puzzles' kept heights |
| `first` (6), `second` (4), `third` (4) | +0x398, +0xC4, +0xD4 | the totems or bells, the lion maze's trees, the lanterns (a tree puzzle's four, (−1, −1), (−1, 3), (3, −1), (3, 3)) |
| `objects[0..2]`, `objects[3..6]` | +0x3B0..+0x3B8, +0x3BC..+0x3C8 | the Hanoi puzzle's bases and rings, the largest first |
| `cells[k]` (k = 3 × across + along) | byte +0x4C + k, +0x58 / +0x7C / +0xA0 + 4k | a tree puzzle's spot: its count, its tree, its mist and its bat |
| `rows[r]`, `lastPairs` | +0xFC / +0x240 + 36x + 4y, +0x410 / +0x430 + 4r, +0x450 / +0x470 + 4r | a maze's walls, then two objects and two words a row |
| `pieces` (18), `fourth` (20) | +0x4E4, +0x494 | |
| `secondWords` | +0x490, +0x388 | a word; the size (float) |
| `half` | +0x38C | the game angle |
| `thirdWords` | +0x390, +0x394, +0x3E4..+0x3F0 | the grid's cosine and sine steps (the Hanoi puzzle's doubled at its start), four words |
| `others` | +0x3F4, +0x3F8, +0x3FC, +0x384 | the maze's wolf, sheep and villager, and one more |
| `lastWord` | +0x3C | played (`Process` and `IsComplete` set it) |
| `halves` | +0x44, +0x46 | the maze's two walks |

The count of calm turns (+0x40C) is not saved. A Hanoi block (0x30) is a moving object with its rest place
(+0x68, MapCoords, which the reader keeps as three floats), the block above (+0x74), the block below (+0x78) and its
puzzle (+0x7C). The four Land 2 saves (the frozen `_J_a_h_o_v_i_a` 15/2, 16/2 and 20, and `land2_resumed_save60`) each
hold two puzzle games in the game's list 16, the Hanoi puzzle (type 3) and the first tree puzzle (type 1), at the
same places, neither begun (+0x584 = 0; neither can be played without being begun), and the Hanoi puzzle's seven
blocks in the moving objects' list (10); in 15/2 the rings stand on the first base, at 0, 4, 7 and 9 over the ground.
The puzzle's words +0x57C, +0x580 and its kept heights, which no constructor sets for these types, are 0 there; a new
puzzle game is written with them 0.

openblack (`src/ECS/SavedWorldLoad.cpp`, `src/ECS/SavedWorld/WritePuzzleGames.cpp`, the blocks in
`src/ECS/SavedWorld/WritePots.cpp`; `test/test_saved_puzzle_games.cpp`):

- **Planned**: the blocks that stand on the map or are in a hand, in the order their reads ended
  (`WorldPlan::hanoiBlocks`), and the puzzle games of the game's list from its last to its first
  (`WorldPlan::puzzleGames`), so that the newest is made, and processed each turn, last.
- **Made**: each block with the moving objects, as a moving object of its row with its record and its rest place;
  where the puzzle put it is where it stands when it was saved at its rest place, else its rest place
  (`MakeHanoiBlockEntity`). Each puzzle game once every object is made (`MakePuzzleGameEntity`): its type, place, size,
  angle, steps, played and begun marks, its place's marks and hidden name as saved, the count of calm turns 5; the Hanoi
  puzzle its bases and rings, each block given the blocks above and below it and its puzzle from its record, and the
  last state of each piece; a tree puzzle its spots' counts, trees, mist and bats, its lanterns and begun mark, each
  spot's place where its tree stands when the tree was saved on it. It joins the puzzles' turn
  (`TrackPuzzleGame`), and the scripts' table names it by its number, so PLAYED and GET_OBJECT_STATE answer as saved.
  The loaded rings keep their shadow on the land's texture: the start that switches it off is not run by a load.
- **Made by the load**: the types whose state the record holds whole: the two tree puzzles (1, 2), the Hanoi puzzle
  (3) and the types that have nothing to play (the two mazes 4 and 5, the second fish puzzle 15, none and past the
  last). The others (the totems 6 to 9, the Theseus and tilt mazes 10 to 13, the fish puzzle 14, the immersion puzzles,
  the lion and sheep, the chess) are not made, with one log line: no save read holds one.
- **Written**: a loaded puzzle game from its record with the type, place, marks, size, angle, steps, played and begun
  marks, and the Hanoi or tree puzzle's objects and counts it holds now (a thing gone, or not made by the load, is
  none); one made since as a new one holds the rest (every word 0, the arrays whole and empty, the steps from its angle
  and size). One made since of a type the load does not make stops the write. The list of puzzle games puts those
  made since first, newest first by the order the scripts made them (`PuzzleGame::creationStamp`). A block from its
  moving object's parts, its rest place, the blocks above and below it and its puzzle; one made since that stands
  where its puzzle put it is written at its rest place, as the game's.

### The wonders

The original (`Wonder::Load` 0x7790F0, `Wonder::Save` 0x779080): a wonder is an abode with its power (+0xC4, a float),
read and written after the abode's part. A load makes it with its constructor of no arguments (`SetToZero`), so its
making's `SetPower` and `AddToPlayer` (`Wonder::Create` 0x778E80, after the constructor 0x778E00; the built wonder's
power added to its player's most tribal power) do not run: the power comes from the record, and the player's most tribal
power from the player's own record (`GPlayer::Load`). The four frozen saves of land number 3 (`_J_a_h_o_v_i_a` 0, 15/3,
16/3 and 17/3) hold three wonders each; they are games on the three-player multiplayer map (`MULTI_PLAYER\MPM_3P_1.lnd`),
not the story's Land 3 (`Land3.lnd`, which `Scripts/Land3.txt` loads), and their script states hold no task.

openblack (`src/ECS/SavedWorldLoad.cpp`, the writer in `src/ECS/SavedWorld/WriteAbodes.cpp`;
`test/test_saved_world_write_abodes.cpp`):

- **Planned**: the wonders that stand on the map, in the order their reads ended (`WorldPlan::wonders`), with the
  abode row their record names.
- **Made**: after the plan's other buildings, before the building sites and the villagers, as an abode of its row
  (`MakeWonderEntity` through `MakeAbodeEntity`), of its town, built or being built as saved, with its footpath link,
  its villagers and counts as its record lists them, its power as saved and its record kept. Every player's most
  tribal power is left as it was before the making (the archetype's making adds a built wonder's power, as the
  original's `Create`); the players' load then puts back what their records hold. One of a row that is not a wonder's
  is not made, with one log line. The census counts the made ones.
- **Written**: as an abode is, of its kept record (a loaded one) or its state (one made since), then its power as it is
  now (`ecs::wonders::GetPower`). Before, no writer claimed a wonder.

### The magic food

The original (`MagicFood::Load` 0x5FAB40, `MagicFood::Save` 0x5FAB00): a pile of magic food (0x1E) is a food pile,
read and written as one (`PileFood::Load` / `PileFood::Save`: the pot, the pile's values and its sparkle's
container), then its player (+0xBC, a reference). The frozen Land 2 saves (`_J_a_h_o_v_i_a` 15/2, 16/2 and 20, and
`land2_resumed_save60`) hold nine between them (one, two, three and three).

openblack (`src/ECS/SavedWorldLoad.cpp`, the writer in `src/ECS/SavedWorld/WritePots.cpp`;
`test/test_saved_world_write_abodes.cpp`):

- **Planned**: the piles of magic food that stand on the map, are in a hand or in the physics, in the order their
  reads ended (`WorldPlan::magicFoods`), with the pot row their record names and the player its player record names.
- **Made**: after the other objects, as a food pile of its row is (`MakeMagicFoodEntity` through `MakePileEntity`):
  its size (`Object::Load`'s scale; the making gives a new one 0.3), with its placement and its sink's height, and its
  amount, poison, speed-up, reaction marks and sink as saved, its sparkle again when it was saved sped up with one
  (named by the kept container's number), the town it was made for, the player as its owner, and its record kept (not
  a food pile's, so the writer writes it as magic food). The census counts the made ones.
- **Written**: by the pots' writer, as before (its class is that of its kept record, else its row's).

### The reward chests

The original (`Reward::Load` 0x6E6C70, `Reward::Save` 0x6E69C0, the same order; `Reward::ResolveLoad` 0x6E6F00 is only
`Object::ResolveLoad`): a reward chest (0x46) is a mobile object (`MobileObject::Load`), then three references and ten
raw words. The game holds it in its rewards list (`GameLists` list 2) and in its mobile objects list (list 10). Its
info id is its row of the reward table: the table is read right after the fireballs' three rows (`load_variables`,
bw1-decomp Balance.cpp:521), so its row 0 is id 313 and row r is 313 - r (61 rows, 313 to 253). The record's fields
(read with `xref.py field Reward`, runblack.exe v1.42, 2026-11-13):

| Record | Field | Meaning | Set by |
|---|---|---|---|
| reference 0 | +0x68 | the miracle its town learnt from it, spinning above it while open | `Reward::InterfaceTap` 0x6E61EF; `ToBeDeleted` 0x6E5C14 deletes it |
| reference 1 | +0x6C | its player | `Reward::Reward` 0x6E587D; read by `GetPlayer` 0x6E5CB0 |
| reference 2 | +0x74 | its town, none for none | `Reward::Create` 0x6E5913 |
| word 0 | +0x78 | the alignment the scripts' ALIGNMENT property gives it (bw1-decomp ScriptProperty.cpp:793); no method reads it | 0 at `Reward::Reward` 0x6E586F |
| word 1 | +0x7C | tapped open | `InterfaceTap` 0x6E5D9C; read by `ProcessDrawing` 0x6E665F |
| word 2 | +0x80 | nothing reads it | 0 at `Reward::Create` 0x6E592E |
| word 3 | +0x84 | its state: 0 falling, 1 landed, 2 opened | `Create`, the landing fn_006E63C0 0x6E6443, `InterfaceTap` 0x6E5D9F |
| word 4 | +0x88 | it has landed: "in the map at once" at the making, 1 at the landing, which it gates | `Create` 0x6E5934, fn_006E63C0 0x6E63C9 / 0x6E643D |
| word 5 | +0x8C | it goes into the map's cells at the next turn | fn_006E63C0 0x6E6437; `Reward::Process` 0x6E68F3 clears it and adds it to the map |
| word 6 | +0x90 | its own clock, a float in seconds: each drawn frame adds the game time's step x 0.001 | `ProcessDrawing` 0x6E666A |
| word 7 | +0x98 | the last drawn frame (`LH3DRender::g_frame`), so it is drawn once a frame | -1 at `Create` 0x6E5940; `ProcessDrawing` 0x6E64D8 |
| word 8 | +0x9C | `Create`'s first float: 0 from the natives, the scale from the generic create; nothing reads it | `Create` 0x6E594A |
| word 9 | +0xA4 | its dust's time left, whole milliseconds; 2500 from the sky, 0 on the land | `Create` 0x6E5967 / 0x6E5991; fn_006E67A0 |

Not saved: the turns an opened chest has left (+0x70, 200 from `Reward::Reward` 0x6E58AF, counted down by `Process`
0x6E6956 only while open) and the dust sprites (+0xA0). The load makes the chest with the bare constructor
(`LoadInstance` 0x559E0E, `Reward::Reward()` 0x6E5620 on `Base::operator new`'s zeroed block), so a loaded chest has
no turns left: once opened, the next turn's `Process` takes it below 0 and deletes it. `Object::ResolveLoad` 0x63A02E
then calls `Reward::CallVirtualFunctionsForCreation` 0x6E59B0 (vt +0x658), the step `Reward::Create` calls before it
sets the chest's values (0x6E591B, so there they are all 0): the closed model, its 27 dust sprites with their five
random draws each on the C runtime's stream (0x6E5ABB..0x6E5BC1, unless the thing's +0xA bit 0), the opened model
when its state is 2 (fn_006E5710 0x6E5BD5), and, when it has landed (+0x88), +0x8C cleared and the chest added to
the map at once unless its place is unavailable (+0x24 bit 2, 0x6E5BDA..0x6E5BF8). Save 9 of the census
(`_J_a_h_o_v_i_a` 17/1) holds one: number 2002, id 255 (row 58, the toy ball), player 2's, no town, landed and not
opened, its clock 17.28 s, no dust time.

openblack (`src/ECS/SavedWorldLoad.cpp`, `RewardSystem::Restore`; `test/test_saved_game_load.cpp`,
`test/test_reward_tap.cpp`):

- **Planned**: the chests of the rewards list, made the oldest first (`WorldPlan::rewards`), of their row of the table
  and with the player their record names. One in the physics, held or flown by the particles is not planned.
- **Made**: after the spells' seeds (`MakeRewardEntity`), at its place, angle and size, with its state, opened mark,
  clock and dust time as saved, its town, its creation number, life, marks and record kept (not the miracle above
  an opened one: its class, 0x23 `SpellSeedGraphic`, is not read by openblack's reader, and openblack's is a drawn
  graphic with no record); as the original's load, its dust sprites made again with their random draws, the opened model when open,
  in the map's cells at once when landed, and no turns left, so an opened one goes at the next turn. A new chest has
  its 200 turns from its making, not from its opening (the same count for a chest made in play). The C runtime's
  draws of the dust come at the chest's making, after the spells' seeds; in the original they come in the
  resolve's order of the loaded objects. Against the other loaded objects that draw on that stream (the mists and
  the citadels, `MistArchetype`, `CitadelArchetype`) that order is not checked yet; save 9 holds 17 mists and a
  citadel.
- **Written** (`WriteReward` in `src/ECS/SavedWorld/WritePots.cpp`; `test/test_saved_world_write_pots.cpp`): as a
  moving object of its row (313 less the row), in the game's lists of rewards and of moving objects; while it falls,
  at the place it was given at and its y angle as given (the fall turns only its model). Its player and town, then
  its words: its opened mark, state, landing mark (its state not falling), the mark for the map's cells, its clock
  and its dust time from openblack; the scripts' alignment, the two unread words and the last drawn frame as its
  record kept them, else a new chest's (0, 0, -1, 0). openblack does not keep the alignment the scripts set on a
  chest, nor its drawn frames (the guard only stops a second drawing in a frame). An opened chest whose town's
  miracle spins above it is refused, as openblack's miracle is a drawn graphic with no record (the original writes
  its 0x23 object): a save is refused for the 200 turns such a chest stays open.

### The footballs

The original (`Ball::Save` 0x436110, `Ball::Load` 0x436260, the same order; read with `xref.py savelayout` and
`xref.py field Ball`, runblack.exe v1.42, 2026-11-14; bw1-decomp has the class's layout, `src/Black/Ball.h`, but not
these two): a football (0x44) is a mobile object (`MobileObject::Save`), then two points of 12 bytes, four references, a
place of 12 bytes and a word, 141 bytes in all. raffclar's reader (`ReadBall`, `VanillaWorldClasses.inl`) reads the
same layout without naming it. Its info id is the ball table's one row, the table read right after the two map shields'
(`load_variables`, bw1-decomp Balance.cpp:410): 2082 (the physical shield is 2083 in the saves read, the features start
at 2064 after the town's row and the 16 jobs). The game holds it in its balls list (`GameLists` list 3, each new ball at
the head: `Ball::Ball` 0x435A14..0x435A2F) and in its mobile objects list (list 10). No save read holds one (none of the
16 frozen saves, which `savetool --records` reads to their ends).

| Record | Field | Meaning | Set by |
|---|---|---|---|
| point 0 | +0x6C | its point in the world as its pitch's turn last took it | `Ball::Ball` 0x435A50 (its place); `Football::ProcessBall` 0x531E73 |
| point 1 | +0x78 | that point the turn before, for the goal test | `Ball::Ball` 0x435A7D; `ProcessBall` 0x531F74 |
| reference 0 | +0x94 | a pitch's game | 0 at `Ball::Ball` 0x4359E4 and `SetToZero` 0x435C0E only |
| reference 1 | +0x98 | a villager, the one `GetPlayer` 0x435F60 asks | 0 at `Ball::Ball` 0x4359EA and `SetToZero` 0x435C02 only |
| reference 2 | +0x9C | a second villager | 0 at `Ball::Ball` 0x4359F0 only |
| reference 3 | +0xA0 | the town a villager's pick-up gave it to, none for none | 0 at `Ball::Ball` 0x4359F6; `Ball::AddToTown` 0x436029 |
| place | +0xA4 | a place | 0 at `Ball::Ball` 0x4359FC..0x435A08 only |
| word | +0xB0 | a word | 0 at `SetToZero` 0x435C1A only |

("only": no other write in the ball's functions, `xref.py field Ball`, nor where Ghidra's C of any function names a
ball's field, `gx.py grep --writes Ball+<offset>`.) Not saved: whether a town has taken it (+0x90, set by `AddToTown` 0x436030 and read
by `IsBallFree` 0x4360E0), the kick's destination (+0x84) and the kick's word (+0xB4, `KickBallAtDestination` 0x435DB2).
The load makes the ball with the bare constructor and `SetToZero` (`LoadInstance` 0x559D47, 0x435C00: +0x90, +0xB4 and
the references 0), so a loaded ball keeps its town but is free again: a villager can run after it and pick it up, and
the pick-up, finding it has a town, leaves it with that town (`AddToTown` 0x436003). Its call to come and play is not
made by the load (the constructor that makes it, 0x435A97, is not run): it is the reaction's own record.

openblack (`gamesave::BallRecord`, `components/gamesave/include/WorldListRecords.h`; `test/gamesave/test_world_list_records.cpp`):

- **Read** as above, its fields named; test `SaveWorldListRecords.ABallWritesItsPointsThenItsTownAmongItsReferences`
  (a synthetic record: no save read holds one).
- **Planned** (`src/ECS/SavedWorldLoad.cpp`, `WorldPlan::balls`): the footballs of the balls list, made the oldest
  first. One in the physics, held or flown by the particles is not planned, as the other moving objects.
- **Made**: after the reward chests, once the towns are made (`MakeBallEntity`, `BallArchetype::CreateLoaded`): at its
  place, angles and size, its two points as saved (`components::Ball::point`, `previousPoint`), its town the entity the
  load made of the town its record names, free (`components::Ball::owned` false, as the original's load leaves it), no
  kick; its creation number, life, marks and record kept, and no reaction made: the reaction's record makes it again
  before the reactions' load. Tests `SavedWorldLoad.TheFootballsArePlannedAndMadeWithTheTownsTheLoadMade`,
  `PotWrite.ASavedFootballIsMadeAgainWithItsTownAndWrittenBackAsRead`.
- **Written** (`WriteBall` in `src/ECS/SavedWorld/WritePots.cpp`): as a moving object of row 2082, in the game's lists of
  balls and of moving objects; its two points, no pitch's game and no villagers, its town (none for none), and the place
  and word as its record kept them, else 0. Whether the town has taken it is not written, as the original. Tests
  `PotWrite.AFootballIsWrittenWithItsPointsAndItsTown` (one made in play, read back as written),
  `PotWrite.ASavedFootballIsMadeAgainWithItsTownAndWrittenBackAsRead`.
- **A town's things to play with** (2026-11-14): the town's record lists them (`TownRecord::playthings`, read in
  `Town::Load` 0x73F450 after NearestTown, a count then a reference each, added at the list's end, so in the saved
  order, the newest first). In the original only footballs are ever in it (`Town::AddPlaythingIfNoneThere` 0x73B7A0 has
  two callers, `Ball::AddToTown` and the script's CREATE of a ball), and every reference of the list is an object the
  load has read already or reads there and then, so the list comes back whole. openblack's load puts the list back once
  the footballs are made (`src/ECS/SavedWorldLoad.cpp`, `components::Town::playthings`), each as the entity the load
  made of it, in the saved order; one the load did not make (a football held or in the physics, not planned) is left
  out. A loaded town writes the list as it holds it (`WriteTowns.cpp`), so a football taken since the load is in it.
  Test `Data/TownWriteWorld.ALoadedTownGetsItsPlaythingsBackAndWritesThemAsRead` (two footballs, back newest first and
  written as read).

### Test hooks

None. A debug readout (2026-11-11): the listing of a save's refusals (`ListRefusals`, the debug bar's list and the
"Save refusal:" log lines after a refused save) names, for each refused object, the object whose record first named it
(`Refusal::namedBy`, its class and number), and groups the lines by that class too, so an object the game destroyed
while a record still names it shows what holds it (tests `SavedWorldWriteRefusals.TheLinesTellWhoNamedTheObjectRefused`
and `...TheListingGivesEveryRefusalTheWriteStopsAtTheFirst`). The write itself and every file are unchanged. When the
game object's record is the namer, the readout adds the part of it that names the object (2026-11-12,
`GameRecordPartsNaming`: a player's place, a global list by its place, a slot of the scripts' object table, a bookmark,
or the tail's help, script, hand and audio objects; test `SavedWorldWriteRefusals.TheGameRecordsPartThatNamesAnObjectIsNamed`),
as for the cow named by the game after the Land 1 save 14/1's load.

## Pending

- **A duel's maker whose plan is not the fight** (census c613 of 2026-11-13, `refs/story/evidence/census_original_saves/c613_20261113/c61310.log`; parked 2026-11-13 with this evidence, read again with bw1-decomp 73eb10c8). Found 2026-11-13 with a trace run of c613's own exe (`c613t10.log`): 4847's sod-off made the fight's agenda, then it took up eating while the fight it had set up went on ([fights.md](../creature/fights.md#pending), "A new plan ends the fight it is in", fixed there); and the original's maker of a sod-off fight keeps the sod-off for its plan (`ConstructSubActionsForTellCreatureToSodOff` builds the fight's sub-actions one time in two), so `DuelPlanOf` now takes the maker's plan when it is the fight (21) or the sod-off (256) and writes that action with the fight's steps (test `CreatureWriteScriptPlan.ACreatureInItsDuelKeepsItsFightsPlan`). The readings below led there. A second run of the same save with the game log at trace (`refs/story/evidence/census_original_saves/amigo_20261113/amcT10_save10_trace.log`, 2026-11-13, an exe of the day) shows the save holds no duel: that run had no fight at all, and all four of its saves were written. So c613's duel started in play, with creature 4847 as the maker of its arena while its plan was HUNGER / EAT_ALIVE (desire 4, action 11). In openblack a fight starts only from the `FightCreature` agenda (its plan the Fight action, 21; `StartFightWith` in `CreatureMindSystem.cpp`), and the mind leaves a fighter alone (`CreatureMindSystem::ProcessTurn`: no thinking with `CreatureFighting` on it), so that plan cannot have come after the start through the mind's own turn. What remains: the start of c613's duel, which its info-level log does not show; a run of c613's own exe with `-L game=trace` names it. The trace run also shows the path below at work (line 488: "Creature 4847 stops looking 4850 over to act on desire 2"). The text below is the first reading, kept for its references: later its mind adopts HUNGER / EAT_ALIVE (desire 4, action 11) on creature 4850, most likely its opponent, and marks it as its target (`LeashSystem::MarkTargetAt`, logged at `LeashSystem.cpp:1903`). That is not a leash order: the target list was filled by `CreatureMindSystem::ForceDesireOn`, from the watch of another creature (`CreatureMindLearning.cpp:1378`) or an agenda's desire step (`CreatureMindSystem.cpp:1914`); the log does not say which, as their lines are at trace level. The fight system keeps the duel at its Duel stage, so every later save refuses (`saved_world_write::DuelPlanOf`: "a fight its mind's plan is not of (desire 4, action 11)"). In the original a creature's plan leaves its fight only through what stops it: `Creature::StopWhatIAmDoing` (Creature.cpp) ends the duel there and then (the opponent's `FightCreature` cleared and its action finished unsuccessfully, a fight's strength for both, the arena made for the fight deleted, `FightStarted` false), as `ForceActOnObject` 0x4C5FF0 does on a leash order (its agenda cleared, then `FinishActionUnsuccessfully`; the fight sub-action has no abort, so the end is the partner and `FightCreature` code). openblack's leash order already does that (`LeashSystem::PullAwayFromAction`, `StopWhatItDoes`, `CreatureFightSystem::CallOff`), but `ForceDesireOn` and the planning before it do not ask whether the creature duels. Still to read: whether the original's watch and desire steps can run at all while it fights (its reactions refuse a creature with a partner, `ReactToCreaturePriority` 0x4F2BF0), then either keep the mind from planning over a duel or end the duel as `StopWhatIAmDoing` does.
- The pots and piles: the puzzle grain (0x107) has no record in the reader and no entity in openblack.
- The puzzle games of the types no save read holds (the totems, the Theseus and tilt mazes, the fish puzzle, the
  immersion puzzles, the lion and sheep, the chess) are neither made by a load nor written when made since: their
  state's fields wait for a save that holds one.
- The records of the classes no save read so far holds: the reader stops at a class it does not know; each layout
  without C is read from the exe and proved by the sums on a save that holds it.
- The meaning of the computer player's and the action state's raw words and lists, and of the citadel's four lists
  (the reader keeps them as they are).
- The villager states 255 to 257 of the decompilation's table: the exe's table has 255 states, and the reader saves
  nothing for a state past it.
- The info id order: which tables register (only the root loaders call `SetInfoID`) and each table's row count fix
  every id; the ids seen (1743 for GPlayerInfo, 2104..2115 for the tree rows) check it.
- A loaded creature's object scale: the size its body has at `Object::ResolveLoad`'s `CallVirtualFunctionsForCreation`
  (the frozen saves' 2.0), needed once a load makes creatures.
- Which raw structs in the stream hold pointers or padding (they make two saves of the same state differ).
- The file names' page arithmetic (fn_00790980 / fn_00790940 are not read; the room's textures take slot / 2 and slot /
  8, fn_00792780), and the steps from the screen checked against a capture of the original.
- The picture of the exit save made as the game quits: raffclar reads the screen made black first; not read here.
- What slot 14 is for in play (`GScript::GameThingClicked`'s case).
- The help-text title of each slot (the table 0xC2864C) in the English game.
- Whether a task of the last script is written with its id as is (the library's task writer is not read).
- The save room's rule for the help after a save (raffclar's, in his slot rules) is left out until the room is ported
  and it is checked.
- Loading: of the world's objects only the climates, the weather things, the rivers and the banks of mist, the
  footpaths and their links, the abodes, creches and graveyards, the towns' desire flags, the citadels' hearts, the
  worship sites, the arenas, the dances, the towns (their first values and forest lists), the storage pits and the
  piles, the magic food, the forests, the big forests, the trees, the rocks and other mobile statics, the villagers (those the
  villagers' item above makes), the wonders, the Hanoi puzzle's blocks and the puzzle games of the types
  [the puzzle games](#the-puzzle-games) lists come back, and the computer players' switches and minds are put back; a desire flag's store and its rise, an abode's villagers and the town's links to its creche or
  graveyard beyond what the archetype sets, a heart's influence, town, flock, entrance and destruction (kept
  in its record), a citadel's parts and lists, a footpath node's followers, a footpath link kept with a
  building's place, the forests' and other buildings' links, a pile's owner, a storage pit's other abode values (its villagers, drinking water, destruction mesh), whether it
  was being built, a tree's fire and town artifact, a tree held by a script or in flight are not put back yet. openblack has no
  list of every tree: where it walks every tree it walks the
  registry, so the game's tree list (kept in its saved order by the load) is not reproduced apart from the order the
  trees are made in, which is the map cells' order.
- The weather things' and the climates' natural storms' load and write are checked only against their records' layout
  and against themselves: only one frozen save holds a weather thing (with no storm) and every save read has its
  climates' storm lists empty, so a storm's saved values wait for a reference save the original makes during a storm
  (`tools/original/save_ref.sh`).
- Writing: every class without a writer of its own is written from its kept record until its area's writer comes, so
  a game not loaded cannot be saved yet (its save says why and writes nothing). The save room itself is not ported
  (only the debug bar reaches its save), nor the rest of [The saves in play](#the-saves-in-play)'s "Not done".
- **The writers on an unlanded branch**: `save/s1r` (tip 9534be79a, 61 commits on joint290) holds the next writers
  and the creature parts they read. Its first step, the 13 commits up to the influence rings' writer (every test
  group, fidelity and cycle identical), has landed: the players' interfaces, statuses and leashes, the computer
  players and their hands, the spot visuals' containers, the spell icons, the spells and seeds and the influence
  rings' writer (the sections above); so has the reactions' writer and load. Still on the branch, not landed:
  - the rings put back on a load, the flocks and the creature spell, the creatures' minds numbered by the game's list,
    and a creature written whole by its class's writer (its own values, its moving and living parts, its plan, agenda
    and dance partners for a script-controlled creature), with the creature parts they read (its home, its birth
    turn, its skeleton mark, its watch turns, who fights for it, a waiting creature spell's seconds, where it stood
    as its turn began);
  - the pots and piles: written from their entities (a plain pot, a food or wood pile and the magic food and wood), a
    pot's own town, whether a building site made it and whether it starts its reaction as it is made, a loaded pile's
    poison, speed-up, reaction and sink, and the game's lists of food piles and mobile objects;
  - the footpaths, the abodes, creches, graveyards and storage pits, the towns' desire flags and the citadels' hearts
    written from the entities (a loaded heart takes its own values back);
  - the leash's last tie turn, a computer hand's first step's stage, its move in front and its wait, the miracles a
    computer mind has cast, and a loaded game's computer hand reported where the save left it;
  - the creature's mind file: who saved it, its anger leaving its own player's things alone, and a decision tree's
    examples kept newest first (a load keeping a tree's first sixteen);
  - the behaviour steps of the creature it carries (the clear place, the confined area, the home and the walk's look
    on the map, a creature dead for good fainting then lying, a step's face pulled as it begins, a temple being built
    giving its creature its home and pen, a creature miracle keeping its creature).
  The two tests that failed on the branch's earlier base
  (`SavedWorldWriteLivingParts.EveryFrozenCreatureThatKeepsItsPlanIsControlledByAScript` and
  `CreatureLoopTest.TurnMakesNoStorage`) are put right on it; its behaviour steps are measured one by one against
  joint290 before they land.
  The pot's own town (the original's `ParentTown` +0x6C, kept by `Pot::Pot` 0x66CE60 and read only by `Pot::Save`
  0x66D730 / `Pot::Load` 0x66D820) is on the branch (`Pot::town`, `fromBuildingSite`, `reactsWhenMade`, set at
  openblack's creation sites as the 23 callers of `Pot::Create` 0x66CF10 pass them, [objects.md](../world/objects.md)).
  The computer hand's take ([computer-hands.md](../hand/computer-hands.md)) has landed, so on the branch's base both
  are present, but `MakeHandPot` (`ComputerPlayerSystem.cpp:398`) does not yet take
  `town = town_queries::GetTown(at)`, the container's town, as the original's take out of a container passes it
  (sub_655170 0x6552AD); the hand's straight handful from a field or a fish farm needs the same
  ([picking.md](../hand/picking.md#pending)).
