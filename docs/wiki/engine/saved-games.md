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
| 14 | `GScript::GameThingClicked` 0x70AF82, per land. Profile D's `Saved Games\14\1` also holds `14.sav.tmpvor`, the same size (3902 bytes) as the profile's `vortex.txt`; its writer is not pinned **🔍 inferred** |
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
  GameThingWithPos part, `WriteInfo(info)`, the owner); the heart (+0x30); `WritePtrArray(+0x34, 6)`, its parts; a list
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
  (+0x30, +0x34, +0x38, +0x3C, +0x44) and a pointer +0x40.
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
- **The wonder** (0x80, exe only: `Wonder::Save` 0x779080): `Abode::Save` and +0xC4.
- **A script's highlight** (0x3F, `ScriptHighlight::Save` 0x70A760): `Fixed::Save` and +0x5C, +0x60, +0x7C, +0x80,
  +0x78, +0x84.
- **The magic food** (0x1E, `MagicFood::Save` 0x5FAB00): `PileFood::Save` and the player.
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
  Not written from openblack yet: every shield spell names a reaction, which has no writer (its rings have one); the info
  ids of the map shield (2083 for the physical one in both saves) and of its magic (2642) are not proved for the other
  rows.
  `LandscapeVortex::Save` (0x600000) is `MobileStatic::Save`, 12 bytes, four words and bytes with their count; the
  vortex into another land (0x1B, 0x600460) adds two pointers and words with their count. The vortex's save of the land
  (0x106, `VortexSave::Save` 0x76FB40) writes a byte, a word and two texts with their counts, and copies its land's
  file beside the world file.
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

1. The loading box; a buffer of 256 `PhysicsSaveInfo` (matrix, velocity, angular velocity: 0x48 bytes each).
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
  whether a load would begin), `IsAutoSaveValid` as the game's 0x564160, the safe write of a save's files as
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
  in); `PlanOf` takes from it the trees that stand on the map (not held, in the physics or flown by the particles) with
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
  every class of the save how many records it held and how many became objects. The big forests come in the same
  pass as the trees, in the same order (both go into the map cells): `BigForestTypeOfInfo` (369 down), and
  `MakeBigForestEntity` makes one through the big forest archetype's overload that takes its forest and its wood
  (`BigForestArchetype::Create(position, type, angle, scale, forest, wood)`; the old one makes its forest and wood and
  calls it), at its saved place, angle and size, with its saved creation number, life and record; each forest then
  points at the big forest its own record names. On the frozen Land 1 autosave: 1,346 trees, 581 of them in its 9
  forests, and its 3 big forests, each with its forest.
- **The towns** (`ecs::saved_world_load`): `PlanOf` takes them from the last of the game's town list (global list 31,
  newest first) to its first, and the players' town lists in order (`ownersTowns`); `Apply` makes them before the
  objects (`MakeTownEntity`: `TownArchetype::CreateWithoutDesireFlags`, as the desire flags have records of their own
  and the original's load makes none; the saved id as the town's key, or the next free key with the id kept as its
  script id when another town has it; its player, tribe, place, belief in the neutral player, area and whether it is
  uninhabitable; its record kept), gives each town its forest list in its saved order (the forest list's
  `TownLists`), and gives the towns their owner's list stamps in the players' saved order. Its other values
  (worship, influence, desires, belief in each player, its buildings, villagers and lists) stay in its record until
  their groups come. On the frozen Land 1 autosave: Land 1's six towns with their script's players and tribes.
- **The storage pits and the piles** (`ecs::saved_world_load`), in the same pass as the trees: the piles of wood and
  of food (`PotTypeOfInfo`: the pot rows run from 1876 down, the storage pit's food pile 1874 and its wood piles 1873
  to 1869 in every frozen save) are made through the pot archetype at their saved place and angle with their saved
  amount, set at once to that amount's height (`MakePileEntity`); a storage pit (`AbodeTypeOfInfo`: the abode rows
  run from 2394 down, Norse 2275 and Aztec 2355) comes after its piles, which were read inside it, and is made
  through the abode archetype's overload that takes its piles (`AbodeArchetype::Create(..., piles)`: it makes none
  and fills none) with its saved food and wood, of the town its record names (`MakeStoragePitEntity`); each town then
  takes the storage pit its own record names (`town_stores::SetStoragePit`). On the frozen Land 1 autosave: its two
  storage pits, each of its town with its food pile and its five wood piles.
- **The footpaths and their links** (`ecs::saved_world_load`), before the towns: the footpaths from the last of the
  game's footpath list (global list 28, newest first) to its first, each made as a new footpath at the list's head
  with its nodes in its saved order (their map places, raised by their heights above the land, and their flags) and
  whether it is in use (`MakeFootpathEntity`); then each footpath link with the footpaths it lists, in its saved order
  (`MakeFootpathLinkEntity`); each storage pit and big forest is given the link its record names as it is made
  (`footpaths::SetFootpathLink`). On the frozen Land 1 autosave: 84 footpaths with their 792 nodes and 59 links.
- **The abodes, creches and graveyards** (`ecs::saved_world_load`), in the same pass as the trees: the original's
  resolve of an abode (`Abode::CallVirtualFunctionsForCreation`, Abode.cpp:326: its chimney smoke and its sinking into
  the land; the graveyard's adds only a flag of its 3D object, 0x595DD0) does what the abode's creation does but not
  its MakeFunctional. openblack makes each through the abode archetype (`MakeAbodeEntity`) at its saved place, angle
  and size, of the town its record names, whole or being built as its saved build flags say, with its saved food and
  wood, then its build flags, percent built, creation number and life from the save; a graveyard takes its saved dead
  and shows no graves stage (the original keeps the stage on its 3D object, which the load makes afresh). Each is
  given the footpath link its record names. On the frozen Land 1 autosave: its 30 abodes, 2 creches and 2 graveyards,
  each of its town.
- **The towns' desire flags** (`ecs::saved_world_load`), in the same pass as the trees (they are objects in the map
  cells): each flag of a town made, through the desire flag archetype with its town, its desire (the record's kind),
  the store point of its slot, its mesh and its object scale, at its saved place; the place it was made at is the
  object's own coordinates (which the original sets once, as it is made), its height over the ground its place's
  altitude and its desire the saved one, with its creation number and record (`MakeDesireFlagEntity`); its town keeps
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
  0x74FB20 leaves it), its discipline, the food and wood it holds, its pregnancy, its home, town and mother, two
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
  from their records. One held, in flight, on a footpath or a data path, in a flock, a dance or a reaction, walking,
  or in a state that loads values is not made yet, with one log line; the dance states' two words (`SaveDance`
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
  records are kept, and one log line names such an icon) and the site's mana path (made with the first mote of prayer
  power).
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
  does not hold (the camera's flags and times, a group's speed and centres once it has moved, an untitled group's
  "Untitled N" name from the game's counter 0xBE8290, a footpath link openblack does not give it) is as its load kept
  it, else as the dance is made while it has not started, and the write fails, naming the dance, once it has (tests
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
  values, its sprites, nothing done to its object), links each to its group's next and root and gives it its firemen,
  and sets the two tags as the last fire read left them; a fire whose object was not made (one the load does not make,
  such as a held villager) is not made. The reactions have no records yet: a loaded fire makes its reaction again on its first turn, and
  a written one keeps the reaction its load kept, or none. The writer (`src/ECS/SavedWorld/WriteFires.cpp`) is a
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
    animal's counter, speed, angle, move state, circle turns and object, goal with its altitude, target (+0x60) and
    step from `AnimalBrain`. A goal keeps the kept map coordinates while it is their place. Kept: a villager's move
    state (openblack holds it as tags) and target, the step's height, the circle's entry distance, index and place
    (a circling walk with no kept circle fails, as does a step through with no turns to its rebuild).
  - The living thing (`Living::Save` 0x5EE630): the footpath (+0xC8, written twice), its node (the footpath's part,
    `PartId`), the states and turns since the change (+0x8C..+0x8E, +0x90), the reaction (a reaction's id) and its
    object (+0xBC), the birth turn, the status bits openblack keeps (a villager's 0x1, 0x30, 0x40, 0x400 with 0x2
    poisoned and 0x80 downed; an animal's 0x1, 0x30, 0x80; 0x8 the way along a footpath; the others as kept), the
    flock and the place in it (+0xD4), a child's creche walk (+0xC4), the specular colour (+0xD0: blue, green, red and
    0xFF, the only colour the heal chakra writes through `Living::SetSpecularColor` 0x417480, else 0), the dance group
    (the dance's part) and the place in it (+0xDC), and the reactions done, oldest first (the list's head, as
    `Living::SetReactionDoneWhen` 0x6E44A0 appends at the tail; each a u32 type, fn_005EF800, and its turn). Kept:
    +0x8F, +0x9C (the strength, written only by the constructors), the byte +0xA8 (only `Save` and `Load` touch it),
    the data path (a living thing that walks a track with none kept fails) and the script reminder. Tests
    `SavedWorldWriteLivingParts.*`, with the frozen Land 1 autosave's villagers and animals written back from entities
    holding what their records say.
- **The flocks are written by their writer** (`src/ECS/SavedWorld/WriteFlocks.cpp`; tests `FlockWriteTest.*`), in
  [the flock's `Save` order](#the-town-centre-the-flocks-and-the-building-sites) (xref notes #2844-#2850, checked
  against the 1,686 flocks of the 16 frozen saves): the container part, its place the domain's centre
  (`Flock::domainCentre`), its info (`Flock::info`: the default flock info `g_defaultFlockInfo` every maker passes, which no info table holds, so `GetInfoID` gives none, 0xFFFFFFFF, as every saved flock has it; `flocks::k_DefaultFlockInfo`), its player; the three words 0, 0xFFFFFFFF,
  0xFFFFFFFF; no shepherd (openblack has none), its town, its temple (the heart whose `CitadelHeart::flock` it is); its
  members from the head (`flocks::MembersFromHead`); `maxMembers`, `leaderTurns` (+0x4C), `domainRadius`,
  `flockDistance`, `altitude` (the flying height), no shepherd to follow, `calm` (+0x5C); `savedDomainCentre` twice
  (+0x60 and +0x6C, which only the constructors write, the same in every save); `followState`, `afterMove`,
  `followMode`; no saved thing; its script id, 0 for an animal's own flock (openblack's -1). The game's list, global
  21, newest first by `Flock::creationStamp`. openblack loads no flock yet: a loaded game's list is written as kept,
  and a flock made since stops the write; a flock's members have no writer yet either.
- **The reactions are written by their writer** (`src/ECS/SavedWorld/WriteReactions.cpp`; tests
  `Data/ReactionWriteWorld.*`: one made in play, field by field, and the frozen Land 1 autosave and a profile's save
  with 66 reactions written whole byte for byte): a source of no entity, `SourceKey{ReactionRecord, id}` by the
  reaction's id (`effects::reactions`), and the game's list of them, the newest first. Each: its thing part as its
  load kept it, its initiator, its followers from the oldest joiner (`effects::reactions::NextJoinOrder`, every class's
  follower), stealth, type, dance, start turn, whether it is active, its player (`SourceKey{k_PlayerType, number}`, 0
  for its null player) and its radius. The load makes each reaction of the game's list again, from its last to its
  first, without spreading it (`saved_world_load::MakeReaction`); an initiator or a dance the load made no entity of,
  and followers openblack has not put back on their living things, are named as the record kept them. Such a
  reaction has no initiator in play, so the next turn's pruning takes it away: the game keeps it only until then.
- **The forests, the trees and the big forests are written by their writers** (`src/ECS/SavedWorld/WriteForests.cpp`):
  a tree (`Tree::Save` 0x74C1B0) its parts, its turns to its next growth step, the size it grows to, its forest and its
  flags word (kept as loaded; a tree made since its wind slot in bits 2 to 5); a big forest its parts, its forest and
  its wood; a forest (`Forest::Save` 0x53AE40, a source keyed by its key, the game's list newest first) its container
  as loaded, whether it is scenic, its footpath link as loaded, its id, its two lists of trees, the grown and the
  growing, each nearest its centre first by the game's whole-number distance and a tree after those as near
  (`Forest::AddTree` 0x53A310, the ordered insert 0x53B310), a growing tree of a forest that is not scenic moving to
  the grown list once grown (`Forest::Process` 0x539DA0), its big forest, the turn of the last tree a forest planted
  and the id the next forest takes. A forest made in play is refused for now: the row its container writes is not
  read. The frozen Land 1 autosave, its forests, trees, big forests, rivers and mists made as the load makes them,
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
  (`MakeCitadelHeartEntity`); a citadel's first heart keeps the citadel (openblack's `CitadelWorship` on the heart)
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
  The steps from a screen are raffclar's reading of the exe, checked on synthetic texels only: the capture is not
  wired, and what fills the temporary picture in the original is not read.
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
  slot, `NowText` as `UpdateDateAndTime`, `TitleOf` the slot's help text with the land's number), the pictures and the
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
of the load's `Apply`; `HeadOf` is pure): the land's number; in GData the synchronised and the local seed, the turn and
the creation counter; in the ten words after the lists the influence multipliers, the visual time, the cycle as
`GGameInfo` keeps it (+0x48 the day's length, +0x4C the change, +0x50 the night: `SetVisualTimeCycle` 0x557620 stores
its three arguments there before it works out the rates) and VisualTimeScale; in the tail the snow lying (the grid,
the blocks' changes, the melting clock and band) and the "did you know" texts read with their counts. The game
flags, the real turn, the local player, the leash byte, the render clock, LastAutoSaveTurn and the rest of the tail
stay as the record has them until their owners are written. The players are the record's (a new game's: the eight of the
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

**Adding a class writer** (one area's lane, one commit per class group):

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
  and the towns (in the order they joined the player's list) from the entities; a loaded player's creature stays as
  its record names it (a load makes no creature yet), and one got since a load with none is named. A new game's
  computer player, statistics and interface are named as the objects of their classes of the player's number; the
  interface and the computer player have their writers ([below](#the-saves-in-play)), the statistics
  (`k_GameStatsType`) none yet, so a new game's save stops there and says so. A game object written for a new game names the
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
  sets to the turn.
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
- With no profile kept (a run of the checks) no slot exists: `saved_slot::SaveSlot` makes nothing, so the checks write
  no file; the autosave still keeps its turn and steps the run profile's count in memory.
- **A save that fails** never stops nor throws out of the turn: `saved_slot::SaveSlot` returns how it ended
  (`SlotSave`: written, no slot, not made, the world's temporary file not opened, a file that failed) with the reason,
  and what it or the world's writers throw is a save not made. A world that cannot be made whole (a game not loaded,
  an object of a class with no writer and no kept record, e.g. a live shield or flock spell) writes nothing
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
  autosave, slot 20 and the scripts' saves (openblack's own rule, 2026-10-27). Tests
  `SaveTriggersInPlay.AnAutosaveThatCannotWriteTheWorldKeepsTheOldSaveAndLogsOneLine`,
  `SaveTriggersInPlay.ASaveThePlayerAsksForThatCannotBeMadeIsToldOnScreen`,
  `SaveTriggers.WhichFailedSavesTheMessageBoxTells`.
- Not done: the memory-pressure quick save (the original's object heap), the quick load, the room's entry list,
  scroll and help script, `AutoSaved`, the two black frames, the save into a previous profile (openblack changes
  profile only at the start), game mode 4 and the box callback `sub_54C930`.

**The players' interfaces** (`src/ECS/SavedWorld/WritePlayerInterfaces.cpp`, `RegisterPlayerInterfaceWriters`): each
player's interface (0x64), its status (0x65) and its leash's status (0x102) are a source's objects whose id is the
player's number (every player has one of each, as in every frozen save). A load keeps each player's first interface,
its status and its leash's status by that number (`KeepPlayerInterfaces`, through `KeepLoadedSources`, which
`Game::LoadSavedGame` calls after `saved_world_load::Apply`). The three are written with the player's number (their own
words, the status's and the leash's player numbers) and with the references between them by
`context.Reference`; the leash's status says whether the leash is on in the player's hand, whether it works, its type
and what it is tied to, from the leash of the creature the player leads (`LeashSystemInterface::PlayersCreature`,
`components::CreatureLeash`), and keeps the rest as loaded while the player leads no creature. The hand, the throw,
the camera and the points stay as loaded: what the original keeps there at a save is 0 in every frozen save, and
openblack's load puts nothing in the hand; one made since the load is written as the constructors leave it (tests
`PlayerInterfaceWrite.*`, the frozen Land 1 autosave's 24 records by their writers and the file byte for byte).

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
does not put the steps back in a hand yet. What the mind has seen (0x67) is a source's objects by the player's number
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
the game's info or by the debug hook's test effect, stops the write with a message naming it) and the things it aims at. The game's list of containers is the live ones, the newest first, then those the load kept, as the
loaded file listed them. The container's strength is the one it gives its effect each turn (`Container::strength`).
Not written from openblack yet: the manager's collections and their atoms (none: the atom collections and atom cores are not written from openblack),
its word +0x78 (0, as made), +0xAD (1, as made), its targets' words (0), target points (none) and current target (0), and the
three bytes after the process info (0; the frozen saves have 0xCD or 0 there). A load does not make the containers
again: starting an effect again would draw synced random numbers and lose its age and atoms, so a loaded container stays
as the load kept it (tests `ParticleWrite.*`, the frozen Land 1 autosave byte for byte with the writers).

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
the original's word are not read), stops the write with a message naming it. No load makes the icons yet: they are
the town centres' and the worship sites' (tests `SpellIconWrite.*`).

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

**The spells** (`src/ECS/SavedWorld/WriteSpells.cpp`, `RegisterSpellWriters`): each spell is written by the record of
its class (`components::Spell::spellClass`): the plain spell (0x9), the storm and the tornado (0x10) and the healing
spell (0x12) with every spell's values only; the food and wood spells (0xA) with whether their first grain has landed
(+0xEC, `magic::SpellResourceData::firstDone`: `SpellResource::SpellEvent` 0x724D80 pays the first event's amount
while it is 0, then sets it); the forest spell (0xF) with TreeAdded (+0xF0, `SpellForestData::forestCreated`), its
MagicForest (+0xEC, by the forest's key, none once the forest has gone) and MaxObjects (+0xF4, `maxTrees`,
`SpellForest::SetMaxObjectsToCreate` 0x7256C0); the teleport (0x15) with its objects (`magic::spell_objects`, the
newest first: its stones, whose class, MagicTeleport 0x19, the reader does not read yet, so a teleport stops the
write); the water spell (0xC, `magic::water::SpellWaterData`). Every field above comes from the component (its places
in map coordinates through `map_coords::ToFixed`; its particle system the interface of its effect, `SourceKey` 0xA5,
whose manager names its file by the particle type of its spell's class and names the spell as its owner; the casting
hand's status the creator's player's, for a spell cast from the hand). The game's list of spells is the live ones
(`magic::Spells`, the newest first) then those the load kept. Unread for one made in play, written 0: the three bytes
after the process info (kept for a loaded spell). A spell with a reaction stops the write (the reactions are not
written from openblack yet), and so does a spell of a class with no writer that the list names (the shields, which
need the reactions' writer, the flying and ground flocks and the creature spell; `components::SpellClass`). No
load makes the spells yet (tests `SpellWrite.*`).

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

### Test hooks

None.

## Pending

- The records of the classes no save read so far holds: the reader stops at a class it does not know; each layout
  without C is read from the exe and proved by the sums on a save that holds it.
- The meaning of the computer player's and the action state's raw words and lists, and of the citadel's four lists
  (the reader keeps them as they are).
- The villager states 255 to 257 of the decompilation's table: the exe's table has 255 states, and the reader saves
  nothing for a state past it.
- The info id order: which tables register (only the root loaders call `SetInfoID`) and each table's row count fix
  every id; the ids seen (1743 for GPlayerInfo, 2104..2115 for the tree rows) check it.
- Which raw structs in the stream hold pointers or padding (they make two saves of the same state differ).
- The picture page arithmetic in the exe (slot / 2, slot / 8 is measured on the files), what fills the temporary
  picture and when, and the steps from the screen checked against a capture of the original.
- The writer of `14.sav.tmpvor`, and what slot 14 is for in play (`GScript::GameThingClicked`'s case).
- The help-text title of each slot (the table 0xC2864C) in the English game.
- Whether a task of the last script is written with its id as is (the library's task writer is not read).
- The save room's rule for the help after a save (raffclar's, in his slot rules) is left out until the room is ported
  and it is checked.
- Loading: of the world's objects only the climates, the weather things, the rivers and the banks of mist, the
  footpaths and their links, the abodes, creches and graveyards, the towns' desire flags, the citadels' hearts, the
  worship sites, the arenas, the dances, the towns (their first values and forest lists), the storage pits and the
  piles, the forests, the big forests, the trees, the rocks and other mobile statics and the villagers (those the
  villagers' item above makes) come back, and the computer players' switches and minds are put back; a desire flag's store and its rise, an abode's villagers and the town's links to its creche or
  graveyard beyond what the archetype sets, a heart's influence, town, flock, entrance and destruction (kept
  in its record), a citadel's parts and lists, a footpath node's followers, a footpath link kept with a
  building's place, the forests' and other buildings' links, a pile's saved rise (its height interpolator) and its
  flags and owner, a storage pit's other abode values (its villagers, drinking water, destruction mesh), whether it
  was being built, a tree's fire and town artifact, a tree held or in flight are not put back yet. openblack has no
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
