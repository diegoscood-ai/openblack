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

openblack reads and writes the slot's creature mind files byte for byte and reads the script machine's file. Its
`components/gamesave` has the slots' rules, the info file, the picture pages and the world file's stream, with the
records of its first classes and a reader that walks the file from the game object, proving every record by its sum,
as far as the classes it knows. Nothing saves or loads in play yet, and a check never would: its profile is not kept.
openblack reads and writes the slot's creature mind files and the script machine's file byte for byte; it has no slot,
no save service and no reader of the world file yet. A check never saves or loads: its profile is not kept.

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

`InstantSaveGame` 0x792FB0, in this order (the file times of a save made by the original agree: the temporary
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
2. **The map pictures.** `GetCurrentGamePath` and fn_00784A30 (`CopyFileA`, "%s\%s") copy the profile's
   `Current\screenshots_*_map_*.raw` into the slot's folder.
3. **The info.** `TempleSaveGame::UpdateDateAndTime` 0x78F770 (`time`, `localtime`, `GetDateFormat` /
   `GetTimeFormat`: the text follows the Windows locale, e.g. "22:01:40 jueves, 12 de septiembre de 2024"), then
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
- **The action state** (exe only: `PlayerActionState::Save` 0x651740): the GameThingWithPos part; its computer player
  (+0x6C); a counted list of pointers (+0x28, its count +0x2C); eight raw words +0x30..+0x4C; a raw value of 12 bytes
  +0x50; an LHPoint +0x5C; a float +0x68; a pointer +0x74; a float +0x78; an LHPoint +0x7C. 119 bytes with an empty
  list.
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
  reacting, an `LHDynamicStack` at +0x18 (its count +0x1C as a raw word, then each from the top); raw words +0x20 and
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
  the guidance (+0x2C) and LeashStatus (+0x12C); two raw words.
- **The leash's status** (0x102, exe only: `GLeashStatus::Save` 0x5E7230): the GameThing part; raw words +0x14, +0x18,
  +0x1C; a byte +0x30; +0x20; a pointer +0x24; +0x28 and +0x2C; a pointer +0x38; +0x34.
- **What a computer player saw** (0x67, exe only: `GComputerSeen::Save` 0x656B80): a pointer +0x14 and a raw word +0x18.
- **A player's sub-action** (0xFA, exe only: `PlayerSubAction::Save` 0x651D80): a raw word +0x14, then its arguments,
  the count +0x1C as a raw word and each from the list +0x18. **An argument** (0xFB, `PlayerSubActionArgument::Save`
  0x651F80): a raw word +0x14; a pointer +0x18; the count 64 as a raw word and the 64 bytes from +0x1C one by one;
  +0x5C; a pointer +0x60; +0x64; 12 bytes +0x68.
- **The particles' container** (0x6E, exe only: `GParticleContainer::Save` 0x63E5D0): the GameThingWithPos part, a byte
  +0x38, a pointer +0x28, its particle system +0x2C (`WriteSafe` of a `PSysBase*` is a `WritePtr`), +0x30 and +0x34.
  **The particle interface** (0xA5, `GJPSysInterface::Save` 0x6CB060): `PSysBase::Save` is the GameThing part; then
  the system (+0x14).

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
- **An atom collection** (0xA3, exe only: `AtomCollection::Save` 0x6CB680): the GameThing part; its Persistents (a count
  +0x20, then two words each); its systems (a count from +0x28, then each from +0x24); a float +0x2C; a pointer +0x30;
  +0x34; a byte +0x38; a pointer +0x3C; a second list of systems (a count from +0x44, then each from +0x40); a
  pointer +0x48; +0x4C; a byte +0x50.
- **The spell icons** (exe only: `SpellIcon::Save` 0x727940): `MultiMapFixed::Save`, the info, 12 bytes +0x100, +0x10C.
  A town's icon (0x55, `TownSpellIcon::Save` 0x748F70) adds the town; a town centre's (0x56, 0x748FF0) adds a pointer
  and three words from +0x11C with their count; a worship site's (0x78, `WorshipSpellIcon::Save` 0x77FF80) adds a u16
  +0x114, a pointer, +0x11C, +0x138, a u16 +0x13C, a byte +0x120, +0x124, a pointer, +0x134, and a list (the count
  +0x130, then each from +0x12C).
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
  words; the influence ring (0x49, 0x5CDC60) its owner (from its info's base), a pointer and two words; the weather
  thing (0x43, 0x7745C0) 80 bytes, a word, an LHPoint, 80 bytes, two words, an LHPoint and a word.
- **The climate** (0xFC, 0x773320): the GameThing part; 12 bytes; four words; the info; three words from the game's data
  (0xDCB8B8, 0xDCB8C0, 0xDCB8BC); 16, 8 and 12 bytes; +0x58; a byte +0x64; its areas (a count, then an LHPoint, 80
  bytes from fn 0x773B60, two floats and an LHPoint each); seven words and a byte.
- **The spells:** `Spell::Save` (0x721930): the GameThingWithPos part; a word, a pointer, a word, a pointer; three
  12-byte values; six words; 60 bytes (fn 0x722070); a float; two bools; four pointers; a word (fn 0x7220C0); three
  floats; a MapCoords; two floats. `SpellWithObjects::Save` (0x7211D0) adds its objects (a count, then each). The water
  (0xC, 0x725420) adds a word; the shield (0x11, 0x72BC40) a list and two pointers.
- **The shields and vortexes:** `MapShield::Save` (0x72C390) is `Fixed::Save`, a pointer and the info; the physical
  shield (0x17, 0x72C700) adds two words, 52 and 52 bytes, six words, a byte, a bool, a word and a pointer.
  `LandscapeVortex::Save` (0x600000) is `MobileStatic::Save`, 12 bytes, four words and bytes with their count; the
  vortex into another land (0x1B, 0x600460) adds two pointers and words with their count. The vortex's save of the land
  (0x106, `VortexSave::Save` 0x76FB40) writes a byte, a word and two texts with their counts, and copies its land's
  file beside the world file.
- **The arena** (0x41, 0x4250D0): the GameThingWithPos part, three words, `WritePtrArray(2)` and a pointer.
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

The order of step 6 ✅ (disassembly): `LoadInstance` adds each object to the file's GameThing list after its `Load`
returns (`LHLinkedList::Add` 0x564460, at the head), and `ResolveAllLoads` takes the list's last node each time
(0x561C66..0x561C7A). So the links are put back in the order the objects' reads ended: an object read inside another
(at its first reference) before it, and the game object last. The global lists, a town's lists and a forest's lists
are read with `AddToEnd` / `AddToLast` (GameOSFile.h `ReadSafe`), so they keep the saved order.

A loaded tree ✅ (disassembly): `LoadInstance` makes it with `Base::operator new` 0x4366F0, which clears the whole
object (0x4368C4 `rep stosd`), then the default constructor (`Fixed()`, `Tree::SetToZero` 0x749F60: +0x58, +0x5C's low
word and +0x68 only), and `Tree::Load` 0x74C2A0 reads +0x60, +0x64, the forest +0x68 and +0x5C's low word. Nothing
writes +0x5E: its bit 0 (growing) and bit 1 (the script's non-scenic flag), which the full constructor 0x749E00 sets,
stay clear, so a loaded tree never grows (even in a forest) and is not marked non-scenic. The resolve
(`Object::ResolveLoad` 0x63A020 → `Tree::CallVirtualFunctionsForCreation` 0x749F70) makes its 3D object, puts it in
the map cells (`Object::CallVirtualFunctionsForCreation` 0x636BE0, unless it is unavailable or flown by the particles),
sets its matrix from Pos, the ground, its scale and its angle, and sets only +0x5C's wind bits (2..5) from the angle;
no random number is drawn. A tree joins no forest list in its load: the forest's own record lists its trees.

A loaded big forest ✅ (C and disassembly): `BigForest::Load` (BigForest.cpp:59) reads the MultiMapFixed parts, its
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
  shows (tests `LHVMSaveLayout.*`, `LHVMSaveState.*`). Nothing in the game calls the machine's `SaveState` or
  `RestoreState` yet. `SaveState` writes the tasks in rising number order, which is the library's list order: a new
  task goes at the list's end (0x10006011..0x10006030, and a restored one at 0x100078C4..0x100078E3) and the numbers
  only grow (fn_10008870).
- The random streams can be saved and restored as the game data keeps them (test `GameRandom.SaveLoad`).
- The save service (`Locator::saveGame`: `SaveGameSystemInterface`, `Implementations/SaveGameSystem`): the current
  profile's slot folders (asked of the profile each time; none with no profile kept, so a run of the checks reads and
  writes nothing), each slot's summary (its `SaveGame.inf`, whether its world file is there, its load counter and
  whether a load would begin), `IsAutoSaveValid` as the game's 0x564160, the safe write of a save's files as
  `SaveAllGame` 0x558160 does it (each to `.tmp`, then remove and rename), and the load counter as `LoadAllGame` keeps it
  (GameOSFile.cpp:394: refused at `GAME_OS_FILE_MAX_LOAD_ATTEMPTS` 2, a counter it cannot read standing for
  `GAME_OS_FILE_UNREAD_LOAD_ATTEMPTS` 1000; +1 written before the load, 0 after a load that ends). The file system
  service writes a whole file, renames and removes a player's file (`FileSystemInterface::WriteFile`, `Rename`,
  `Remove`). Nothing in play calls the service yet (tests `SaveGameSystem.*`).
- **The load, so far** (`Game::LoadSavedGame`, from the debug bar's Load Island / Saved Games menu only, which lists
  the current profile's slots with a world file and lets a loadable one be loaded): the slot's world file through the
  service's `BeginLoad`; the world read (`gamesave::ReadWorld`) and its first values taken (`ecs::saved_game_load::
  StateOf`: the landscape, the land's number, the turn and the real turn, both seeds (GData +8 synchronised, +0xC
  local), the player, the render clock, the influence multipliers, the visual time, its speed and its cycle, named from
  `GGame::Load`); the scripts' restart and the map's clear through the land path; the saved land's file opened; the
  scripts' machine restored from `Script_<slot>.sav`; the land started; then the values put back in `GGame::Load`'s
  order (`Apply`: the seeds, the land's number and multipliers, `ForceVisualTime` with the sky, the speed, the cycle as
  `SetVisualTimeCycle(+0x48, +0x50, +0x4C)`), the turn set and the clock started again at the turn's time, and the
  counter set back by `EndLoad`. The world's objects are not loaded yet.
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
  forests, and its 3 big forests, each with its forest (tests `SavedWorldLoad.*`, `SaveWorldReader.ReadsFromTheGameObjectDepthFirst...`).
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
- No save service and nothing loaded into play yet; the land-script line writers are the vortex's
  (`src/ECS/VortexSave.cpp`), not a saved game's.
- The work goes into `components/gamesave` (the stream, the slot rules, the info file, the pictures and a record per
  class, each tested with synthetic bytes and, when the game's data is there, with the frozen saves), and a save service
  that turns records into entities and back. A check plays a profile that is not kept, so it never saves or loads.

### Test hooks

None.

## Pending

- Every class's record after the first ones: the citadel and its heart, the footpaths and the towns come next in every
  frozen save; the layouts of the classes without C (about 190 of 260; Land 1 needs about 60 classes, Land 2 more),
  each read from the exe and proved by the sums on the frozen saves.
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
- Loading: of the world's objects only the forests, the big forests and the trees come back; the towns' forest lists,
  a tree's fire and town artifact, a tree held or in flight, and the object creation counter (GData's
  NumCreatedObjects) are not put back yet. openblack has no list of every tree: where it walks every tree it walks the
  registry, so the game's tree list (kept in its saved order by the load) is not reproduced apart from the order the
  trees are made in, which is the map cells' order.
- Writing and the save moments in openblack: nothing is ported yet.
