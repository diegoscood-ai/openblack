# Land changes, skipping the tutorial and saving

## Summary

Three moments when the scripts touch the whole land. At a new game the original always asks, in a requester (the
SkipBox), how much of the tutorial to skip, and the challenge script reads the answer. When a land is finished the
script calls `LOAD_MAP`: the old land is cleared (the player's creature saved to its file first), the scripts and
their variables are kept, and the new land's script runs inside the same call. And the original can write any object
back as the land-script line that makes it again: the vortex carries what it swallowed to the next land that way,
and the land editor saves with the same writers.

openblack shows the SkipBox and sets the same answer bits, changes land inside the script's call as the original
does, and has the line writer and the writers of the physics classes, the dead tree and the one-shot spell seed. The
other classes' writers, saved games and the vortex's `vortex.txt` are not ported. The vortex's own round trip of
`vortex.txt` is on [Vortexes](../world/vortex.md#the-land-script-round-trip-of-vortextxt).

## The original

**Faithful**, read in the v1.42 `runblack.exe` (the 1.20 symbols, and the patch's own bytes where it changed code),
except what is marked.

<a id="skipping-the-tutorial-skipbox-and-can_skip_tutorial"></a>
### Skipping the tutorial (SkipBox and CAN_SKIP_TUTORIAL)

#### The requester

- `GGame::OnNewGame` 0x553900: if there is a script and it is Land 1 it starts `LandControlAll` and, always, calls
  `DoYesNoSkipTutorialRequestersIfNecessary` 0x54CBD0 (0x55395B). This clears bits 23, 24 and 25 of g_game +0x14
  (0x54CBD5 / 0x54CBE1 / 0x54CBED); the v1.42 patch nulls out the test that came before showing the box with 25 NOPs
  (0x54CBF4..0x54CC0C), so it **always** does `PauseGame(1)` and `SkipBox::Show` (vt +0x0C,
  `DialogBoxBase::Show` 0x5135F0) on the SkipBox made by `FrontEnd::Init` (0x53B844, the pointer at 0xCD0634, the
  callback 0x544480 set at 0x53BB9D).
- `SkipBox::Init` 0x5441C0: a title, a button (id 0xB, text 0xA24 from `HelpTextDatabase`) and four checkboxes (ids
  0x3C..0x3F) with the texts 5..8 of the text table at 0xD17CA0 (not read); the selected one is at +0x20, 0 by default
  (0x544206; also set to 0 at 0x5442C2). `SkipBox::CanESCOut` 0x53BD60 returns 0: it does not close with Esc.
- The callback 0x544480: when button 0xB is pressed, by the checkbox (jump table 0x5445A0): 0 clears 23, 24 and 25;
  1 sets 23 and clears 24 and 25; 2 sets 23 and 24 and clears 25; 3 sets all three. Then `PauseGame(0)` and
  `DialogBoxBase::Hide`.
- The scripts read it with `CAN_SKIP_TUTORIAL` (460, `GScript::CanSkipTutorial` 0x6FFEF0, bit 23),
  `CAN_SKIP_CREATURE_TRAINING` (461, 0x6FFF10, bit 24) and `IS_KEEPING_OLD_CREATURE` (462, 0x6FFF30, bit 25), which
  push the bit as a boolean (VMType 6). Nothing else writes those bits (no `or` / `and` with 0x800000, 0x1000000 or
  0x2000000 in the whole `.text`).

#### What the script does with the answer

Read in the decompiled listing of `challenge.chl` (pseudo-assembly: the natives appear as `CALL <n>`; extents:
`SetupLand1` 25357-25622, `LandControl1` 74956-75239, `LandControlAll` 171106-171139, `CreaturesInGlade`
43862-46096, `CreatureDevSeeHome` 6932-7814, `FollowUs` 49528-53196):

- Only `SetupLand1` calls them (25398-25455): `CAN_SKIP_TUTORIAL` → the global `IsSkippingToCreatureSelect` = 1;
  `CAN_SKIP_CREATURE_TRAINING` → `IsSkippingCreatureGuide` = 1; `IsKeepingOldCreature` =
  `IS_KEEPING_OLD_CREATURE and CURRENT_PROFILE_HAS_CREATURE` (25432-25435) and, when it is set, it **forces** the other
  two to 1 (25440-25447). Neither `SetupLand1` nor `LandControlAll` has music, camera, dialogue or fade before
  `LandControl1` (the `SET_FADE_IN(3.0)` at listing line 171120, ip 155496, comes after
  `LOAD_MAP("scripts/land3.txt")`: it is the change to Land 3, not Land 2).
- `LandControl1`: with `IsSkippingToCreatureSelect` neither `FollowUs` (74982, the intro), `CitadelGuide` (74983) nor
  `ChooseYourCreature` (75037) runs. Instead it does `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)`, takes the created
  object (the citadel, 🔍 inferred from the type) with `CALL_NEAR(18, 5000, that point, 5)` and sets its
  `BUILT_PERCENTAGE` (`SET_PROPERTY(22, citadel, 1.0)`, 74988-75017), and deletes the three gate stones `GateKey1`,
  `GateKey2` and `QuarryRock` (75042-75083), which `ChooseYourCreature` would have made.
- `CreaturesInGlade` (75088), which takes the camera to choose a creature (and, when skipping, skips the guardian and
  does a 2 s `SET_FADE_IN` after 2 s), is skipped only with `IsKeepingOldCreature`: then the block 75093-75125 runs
  (`SET_GAME_TIME(15.4)`, `GAME_TIME_ON_OFF(1)`, `SET_CAMERA_ZONE(3436)`, `SET_OPEN_CLOSE` of the creature gates,
  `ChooseYourCreatureFinished` = 1, `LOAD_MY_CREATURE(1850, 1300)`).
- With `IsSkippingCreatureGuide` none of `MoveTheGuideAround`, `CreatureDevLearnToEat`, `CreatureDevPunishment`,
  `CreatureDevLeashIntro`, `CreatureDevLeashAttachToHouse`, `MeetTheGuide`, `GuideAsksToMeetYourCreature`,
  `GuideImpressTown*`, `CreatureDevGuideTeachesFight` or `TheStorm` runs (it waits for `LeaveLandNow`).
- **`CreatureDevSeeHome` (75141) always runs**, with every answer. Before anything, unconditionally,
  `SET_GAME_TIME(4.59)` + `GAME_TIME_ON_OFF(1)` (7000-7003): dawn breaks. With `IsSkippingCreatureGuide` it takes the
  branch 7056-7085: `loop { START_CAMERA_CONTROL }`, `loop { START_DIALOGUE }`, `START_GAME_SPEED`,
  `SET_WIDESCREEN(1)`, `SET_CAMERA_POSITION(1891.039, 31.693, 2520.674)`, `SET_CAMERA_FOCUS(1899.053, 30.312,
  2518.680)`, `SET_WIDESCREEN(0)`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`, `SET_FADE_IN(2.0)`, with no
  `SLEEP` between, so the lock lasts one turn and the camera **snaps** (does not glide) onto the village.
- **The player's creature on the skip path** (challenge.chl instruction pointers, read in the bytecode): `LandControl1`
  loads it at ip 68025 (`LOAD_MY_CREATURE(1850, 0, 1300)`, turn 7) and runs `CreatureDevSeeHome` at ip 68037. There,
  after the dawn and an asynchronous did-you-know, `MyCreature = CALL_PLAYER_CREATURE(1)`; the skip jumps over
  `SET_CREATURE_DEV_STAGE(0)` (ip 6043) to the camera block (6053-6080, above), then `SET_POSITION(MyCreature,
  HomePos)`, `SET_CREATURE_HOME(MyCreature, HomePos)` and `SET_FOCUS(MyCreature, GET_CAMERA_POSITION)` (turn 8), then
  `SET_FADE_IN(2)` and a looped `SLEEP 2` until the script ends. HomePos is the script's local marker made at ip 5951
  at (1896.5, 29.48, 2520.06). At turn 30, `LandControl1` ip 68047-68063: `MyCreature = CALL_PLAYER_CREATURE(1)`,
  `SET_CREATURE_DEV_STAGE(MyCreature, 13)`, `DEV_FUNCTION(2)` (the rope leash), `DEV_FUNCTION(3)` (the other leashes),
  `CREATURE_IN_DEV_SCRIPT(false, MyCreature)`, `RELEASE_FROM_SCRIPT(MyCreature)`. What each native does:
  [The player's creature](../creature/players-creature.md#the-players-creature).
- `CreaturesInGlade` (answers 2 and 3, not 4) holds the start: `START_CAMERA_CONTROL` 44028, `START_DIALOGUE` 44031,
  `SET_WIDESCREEN(1)` 44035, `SET_FADE(0,0,0,2.0)` 44295 (the skip branch), `SET_FADE_IN(2.0)` 44326,
  `SET_CAMERA_POSITION/FOCUS` 44303/44310 (1753.3, 49.5, 2811.1), `MOVE_CAMERA_*` 44339-44403, `HAS_CAMERA_ARRIVED`
  44404/44415/44422, `START_MUSIC(63)` 44419, `RUN_CAMERA_PATH(13)` 44421, and it lets go only at 44684-44691.
- `START_MUSIC(54)`, the intro music, appears **only once in the whole challenge.chl**: line 50114, inside
  `FollowUs`. With any skip answer it never plays. The others in Land 1: 67 in `ChooseYourCreature` (42493) and in
  `CreatureDevSeeHome` without skipping (7117), 63 and 65 in `CreaturesInGlade` (44419, 45644/45813/45985), 67 and 69
  in `TheStorm`. `SET_AVI_SEQUENCE` (51024), `CAMERA_PROPERTIES` (50444), `SET_FOCUS_AND_POSITION_FOLLOW` (50439) and
  `SET_INTERFACE_INTERACTION` of Land 1 are all inside `FollowUs`: with the skip they never run.
- The 13 background scripts (`SingingStoneCircle`, `ThrowingStones`, `TheLostFlock`, `TheMissionaries`,
  `MagicMushroom`, `HermitMain`, `CreatureSavingPeople`, `PiedPiper`, `CreatureGuardian`, `LeaveThroughVortexL1`…)
  wait on a `ChallengeHighlightNotify` / `QuestHighlightNotify` / `SingingStonesNotify` before touching the camera, so
  none gets in the way at the start. With `IsSkippingCreatureGuide`, `LeaveThroughVortexL1` opens the exit vortex
  from the start (53845-53876).
- There is no other way in the original to skip the intro: `FollowUs` does not call `KEY_DOWN` or check Esc; the
  click only moves the texts on.

<a id="changing-land-load_map"></a>
### Changing land (LOAD_MAP)

- **The command.** `LOAD_MAP` (native 152, `GScript::LoadMap` 0x6FB320) pops the path, sets [0xBF3BAC] = 1,
  [0xD318C8] = 0 and `LoadingFrameEnabled` = 2, draws the loading frame (`RenderLoadingFrame(1)` 0x5F4E40) and calls
  `GGame::LoadMap(path)` 0x552F40 (the call at 0x6FB36A) inside the command: `ClearMap` 0x552BB0 (0x552F4F), the
  landscape and island released, g_game +0x205A50 = 0, `GSetup::LoadMapFeatures(path)` 0x7180B0 (0x552F6E) and
  `Town::AsssignTownFeature` (0x552F76). The calling script goes on in the same step, on the new land. The other
  callers of `GGame::LoadMap`: 0x54C990, `GGame::StartPlaygroundGame` 0x555990 (at 0x5559A7, the skirmish start from
  `ResetAndStartPlaygroundGame`) and 0x641752. The seeds this resets:
  [The seeds](../engine/random.md#the-seeds).
- **What `ClearMap` keeps, saves and resets**, in its order:
  - it **saves** the local player's creature (0x552C3A..0x552D54,
    [A land change](../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again));
  - the camera exclusions, the physics objects, the hand, the music and the audio;
  - `ValidateScriptVariables` 0x6EB6F0 (0x552DA9; its first part, `fn_006EB680` 0x6EB683..0x6EB6A6, clears the
    script's `HitObject` (+0x70) and `ObjectWhichHit` (+0x74), the last hit's record, when their thing is no longer
    available, `IsAvailable` vt+0x2C, `GameThing::IsAvailable` 0x401810; bw1-decomp `Script.h`) [V], then `GlobalGameLists::ClearMap` 0x591520 (0x552DBF), which first
    deletes every creature (the creature list g_game +0x205BBC, `ToBeDeleted(0)` at 0x59154E): the creature's
    destructor `fn_00475170` clears its player's creature (+0xA4C, 0x475279), so after a land change **the player has
    no creature** and the next land's `LOAD_MY_CREATURE` makes it again from the file just saved;
  - the players are kept (`GPlayer::OnEndOfClearMap` 0x64CD00, from 0x552DE1: nine float pairs at +0x68 / +0xB8 back
    to 1.0), and so are their alignment and the scripts, which are not restarted (`ValidateScriptVariables` again at
    0x552E0E);
  - `GGame::ResetState` 0x5557A0 (0x552E62; `GData::Reset` 0x510750: the turn and both seeds) and
    `GameStats::ClearAll`.
- **Land 1's way out.** `LandControl1` runs `LeaveThroughVortexL1` at ip 68105 and, on the skip path, waits on
  `LeaveLandNow` (68114-68117). With `IsSkippingCreatureGuide` the script (ip 48110-48809) jumps to 48701:
  `VortexOpen = 1`, the vortex (`CREATE` 23 at 48713) and its highlight (`CREATE_HIGHLIGHT` 48721, property 24 = 20),
  then `RUN QuestHighlightNotify(highlight, vortex, 2, 3401, 3, 4460, 30 × 60)` (48745). **That script loops until the
  player clicks the highlight or the vortex** (`GAME_THING_CLICKED` of either, its exception handler at ip 1989-1994),
  giving a reminder every 1800 s of script time (ip 1936-1988). Only then the cinematic: the camera taken, a 6 s move
  above the vortex and `HAS_CAMERA_ARRIVED` (48771-48777), an 8 s dive (48781) with `SLEEP 5` (48783),
  `SET_FADE(0, 0, 0, 2)` and `SLEEP 2` (48789-48791), `HAS_CAMERA_ARRIVED` (48793),
  `STOP_SCRIPTS_IN_FILES("CreatureGuide.txt")` and `LeaveLandNow = 1` (48796-48800). `LandControlAll`
  (155483-155513) then calls `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING("LandControlAll.txt")`,
  `LOAD_MAP("scripts/land2.txt")` and `RUN LandControl2`; the later lands the same (only before Land 3 a
  `SET_FADE_IN(3.0)`). No hand or creature has to go into the vortex and no film plays, but the click is needed: the
  land changes at the earliest about 14 s (140 turns) after it.
- **STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING** (154, `GScript::StopScriptsExcludingFiles` 0x6F0C00) keeps the list at GScript +0x2C and stops every task whose
  source file is not in it (`StopScripts` with the filter 0x6F0A60): the names are split with `strtok` on space, comma
  and tab (0xC0C710) and compared with `_stricmp`.

#### A variable holding a thing of the land before

The original keeps the challenge and its variables: `GScript::LoadMap` 0x6FB320 calls only `RenderLoadingFrame` and
`GGame::LoadMap`, and the script things' table (0xD967F8, 511 slots of 0x14 bytes from slot 1) is emptied only by
`fn_0070CEB0`, whose one caller is `GScript::Reset` 0x6EB2D0 (the call at 0x6EB396), not reached from `LOAD_MAP`. A variable holds a
slot number; the slot keeps the thing as a `BaseInfo` {+4 the unique id, +8 the heap offset} (`BaseInfo::Set`
0x436BB0 → `Base::SetInfo` 0x436B20). The unique id comes from the object heap's counter
(`UniqueKeyHeap::UseUniqueId` 0x7E1900: +0xBC, then +1), a deleted object's key is set to 0
(`UniqueKeyHeap::Delete` 0x7E18B0), and `BaseInfo::GetBase` 0x436B80 gives the object only while the id at its
address is the same (`IsUniqueIdAtAddressTheSame` 0x7E19F0). So once the land's things are deleted
(`GlobalGameLists::ClearMap`) the variable's slot gives no thing (`GetScriptGameThing` 0x70D220 warns and returns 0),
and never a new land's thing, even one made at the same address. A slot is handed out again only when it has no
reference and an empty `BaseInfo` (`FindFreeScriptGameThingSlot` 0x70CEE0: +0x10, +4 and +8 all 0), so a slot a
variable still holds is never given to a new thing; the slots without references are freed by `fn_0070D480` at
`ClearMap`'s `ValidateScriptVariables`. The heap's counter goes back to 1 only in
`UniqueKeyHeap::SetToZero` 0x7E1780, called through the heap's table at 0x553358 (`sub_5532F0`, which has no caller), never on a land change.
The slots themselves: [The script's things](script-commands.md#the-scripts-things-scriptmanage-slots).

### The writers

The original can write an object back as the land-script line that makes it again. Each class has its own
`SaveObject` (vt +0x82C). The vortex uses it to carry what an In vortex swallows to the next land (`vortex.txt`,
[The vortex](../world/vortex.md)); the land editor's save uses the same writers. The line format is in
[The line format](../formats/land-and-script-files.md#the-line-format); the reading side is
[The map script reader](map-script-reader.md#the-map-script-reader-lhscriptx).

#### The command text

- `GSetup::GetCommandAsText` 0x715130 → `LHScriptCommandX::GetCommandAsText` 0x7E7FB0 gives the printf format of a
  command: the name and "(" (`strcpy`, `strcat`, 0x7E7FBE..0x7E7FD5), then per type letter (the 12 letters,
  letter − 'A' through the byte table 0x7E8078 and the jump table 0x7E8064, 0x7E7FE1..0x7E802A) A `"%s, "` (0xC34DDC),
  L `"\"%s\", "` (0xC34DE4), N `"%d, "` (0xBE83AC), F `"%f, "` (0xC34DD4); the last ", " is cut when anything was added
  (0x7E8035 / 0x7E8042), then `")\n"` (0xC22098, 0x7E8047). The names come from the table 0xC20F00 (16 bytes per
  SCRIPT_FEATURE_COMMANDS entry, the name first).
- `MapCoords::ConvertToText` 0x602880: `sprintf "\"%0.2f,%0.2f\""` (0xBF4198, the quotes in the literal) of x, then z,
  each (double)(fixed × 10.0f × 2^-16) on the x87 (`fild`, `fmul [0x930050]`, `fmul [0x8AC41C]`; with the FPU at 24
  bits, as `fn_007DEE00` sets it (`and cw, 0xFCFF` at 0x7DEE0D), the first product is rounded to a float and the
  second is exact).
- The exe's CRT `sprintf` 0x7C57D2 rounds the digits of `%0.Nf` **half up** (0.625 → "0.63", 2^-7 → "0.007813"), where
  the UCRT and glibc round half to even.

#### Every writer

- Every writer first calls `CheckAndSetSaved` 0x56FEF0: an object whose GameThing +0xC already equals
  GlobalSaveCount [0xD99710] has been saved in this pass and is skipped. The count goes up (`inc word`) in
  `SaveAllMap` 0x7183AE, `fn_00718C10` (at 0x718C93, and near 0x719154) and the VortexSave constructor 0x76F84E.
- The position written is `MapCoords − at` when an `at` is given (the vortex's In, `MapCoords::operator-` 0x6055C0).
- A writer that returns 0 (`xor eax, eax`) does not save the object (VortexSave's write fails for it); one that
  returns 1 without writing leaves it to its parent, or nothing is needed.

#### The physics classes

| Writer | Used by | Skips when | Line |
|---|---|---|---|
| MobileStatic 0x6088E0 | MobileStatic, Rock, Fragment | +0x7C set | `CREATE_MOBILE_STATIC(pos, info, altitude, x, y, z, scale)` |
| MobileObject 0x607270 | MobileObject, Ball | +0x58 set | `CREATE_MOBILEOBJECT(pos, info, ftol(y × 1000), ftol(scale × 1000))` |
| Pot 0x66D550 | Pot, the piles | it is part of a structure (a storage pit's or a workshop's pile, 0x66DA00) | `CREATE_POT(pos, info, resource type, amount)` |

- MobileStatic and MobileObject then mark every identical object (the same class, info and MapCoords: x, z and the
  altitude) as saved, so a stack is written once: MobileStatic 0x608A20..0x608A9E, MobileObject 0x607366..0x6073E6
  (`FindType(MapCoords, OBJECT_TYPE)` 0x6045C0 over the cell). MobileObject's pass goes through `FindTypeOnMap`
  (0x601668) filtered on the info's object type (+0x10): MOBILE_OBJECT 20 only, so a Ball (type 15) never marks its
  twins.
- **MobileStatic** `SaveObject` 0x6088E0: `CheckAndSetSaved` at 0x6088F1; the position 0x6088FE..0x608929; +0x7C (an
  Object*: what it belongs to) checked at 0x60892D; `GetScale` (vt +0x120), `GetZAngle` (+0x50C), `GetYAngle`
  (+0x508), `GetXAngle` (+0x504), each as a double (0x608947..0x608977): the stored angles as they are. The info's
  index = (+0x28 − 0xD3A6D8) / 0x12C and the altitude +8 as the vertical offset (0x60897D..0x6089A5). An
  `IsTownArtifact` (vt +0x800) object writes `TownArtifact::SaveObject` 0x4269C0's own line instead
  (0x6089F2..0x608A1B).
- **MobileObject** `SaveObject` 0x607270: `CheckAndSetSaved` at 0x60727B; the position 0x60728C..0x6072BB; +0x58 (what
  holds it) checked at 0x6072BF; `ftol(GetYAngle × 1000.0f [0x8AB228])`, `ftol(GetScale × 1000.0f)`, the info's index
  (+0x28 − 0xD38448) / 0x114 (0x6072CF..0x607301).
- **Pot** `SaveObject` 0x66D550: `CheckAndSetSaved` at 0x66D55A; the position 0x66D569..0x66D59B; `IsPartOfStructure`
  (vt +0x860) at 0x66D59F → nothing (0x66D632). A plain Pot's (0x55D4B0) is 0; a PotStructure's (0x66DA00) is its
  +0x78 structure when that `IsAvailable` (vt +0x2C); 0x66DA00 also clears +0x78 when its owner is not available. The
  info's index (+0x28 − 0xD4C660) / 0x144, `GetResourceType` (vt +0x690), the amount +0x70 (0x66D5AD..0x66D5D7).
- **A fragment is written as a rock**: its constructor gives every fragment GMobileStaticInfo row 2 (0xD3A930), Rock
  (`Fragment::Fragment`, 0x76E9E4). It comes back on the next land as a plain rock.

#### Other writers

- **DeadTree** `SaveObject` 0x511430 (`DeadTree::SaveObject`): `CREATE_DEAD_TREE` (43, "ALNFFFF"). `CheckAndSetSaved` at 0x511439; the position
  0x511446..0x511478; the player `GetPlayer` (vt +0x1C), else the local player (g_game +0x205A5B), its name from the
  table 0xBFF094 (`GetPlayerNumber` 0x64A790) (0x51147C..0x5114BB); `GetZAngle`, `GetYAngle`, `GetXAngle`, `GetLife`
  (vt +0x11C), each a double (0x5114C4..0x5114F4); the tree info's index (+0x94 − 0xDA3AD8) / 0x140
  (0x5114FA..0x511512).
- **OneOffSpellSeed** `SaveObject` 0x72AB80 (`OneOffSpellSeed::SaveObject`): `CREATE_ONE_SHOT_SPELL_PU` (84, "AL"; not 83). `CheckAndSetSaved` at
  0x72AB8A; the position 0x72AB99..0x72ABCA. The name (0x72ABCE..0x72ABDA) is `GetMagicInfo` 0x72A610 (the seed's info,
  `GetMagicInfoFromPULevel` 0x72AFE0 at its power-up level +0x78: the base one when that level has no magic type) →
  `GetMagicInfoText` 0x5FB3F0 = `GetMagicEffectInfo` 0x5FB680 of that GMagicInfo's magicType (+0x10, the table
  0xD37D10) + 0x34 (debugString), as `GetInfoFromText` reads it.
- Scaffold (0x6EAF30), Bonfire (0x4398A0), Villager (0x751AF0), Abode, Tree, Feature, Creature and some forty other
  classes have their own writers.

#### The footpath file

The footpaths are also saved in their own file (`.fot`): `GFootpath::Save` 0x535E00 writes the nodes, then +0x20;
`GFootpathNode::Save` 0x538D70 writes its MapCoords +0x18, then the byte +0x14 (the flags); a node is 0x30 bytes
(constructor 0x534CF0(coords, a, b)). `GFootpath::LoadAll`
makes a `new GFootpath` per saved footpath (the constructor 0x534EB0 pushes it at the list's head), and
`GFootpathLink::LoadAll` (0x536D9E) resolves each link save as it is read (0x536FA0): to the MultiMapFixed or the
planned abode at its point, else deleted.

### Sources

- runblack.exe (v1.42, with the 1.20 symbols): `GSetup::GetCommandAsText` 0x715130 / 0x7E7FB0,
  `MapCoords::ConvertToText` 0x602880, the `SaveObject` writers above, `GGame::OnNewGame`, the SkipBox and
  `GScript::LoadMap`; the decompiled listing and the bytecode of `challenge.chl`.

## openblack

### In openblack

- **The SkipBox**: `Game::Run`, after starting `LandControlAll`, clears the three bits (the start-of-game answer's
  service, `Locator::tutorialSkipSystem`), pauses the game and shows the SkipBox (`Gui/SkipBox`, with the texts of the
  patch database and entry 0xA24 for the button). Its answer sets the bits (0 none, 1 bit 23, 2 bits 23 and 24, 3 the
  three) and unpauses the game. openblack has no profiles: `CURRENT_PROFILE_HAS_CREATURE` (CHL 463, through
  `Locator::playerProfileSystem`) answers whether the profile's creature file (the `--creature-file` setting,
  [The player's creature](../creature/players-creature.md#the-players-creature)) exists, as the original answers for
  the profile's. Before the script camera was ported, `MOVE_CAMERA_*` and `HAS_CAMERA_ARRIVED` were stubs and
  `CreaturesInGlade` **stuck there for ever with the camera, the dialogue and the widescreen held**.
- **LOAD_MAP** publishes `events::LandChangeRequested` with the path; its handler, `Game::ChangeLand`, loads the land
  at once through `Game::LoadMap` (the game folder and the path), inside the script's call as the original does:
  nothing iterates the registry above the VM's step, and `LoadMap` does not touch the VM. The calling script and the
  rest of the turn go on on the new land. **⚠️ approximate** The turn number the state hash is given at that turn's end
  is the old land's.
  - Before the load, `ChangeLand` saves the local player's creature to the profile's mind file and its physique
    beside it (`ecs::player_creature::SaveMyCreature`,
    [A land change](../creature/players-creature.md#a-land-change-the-creature-saved-and-made-again)), as `ClearMap`
    does first; then the registry's reset takes the creature with the land, and the next land's `LOAD_MY_CREATURE`
    makes it again from the file. The map-cycle test hook saves nothing; the debug menu saves only with "Take your
    creature along" (below).
  - Then `ChangeLand` releases the script things no variable holds (`ecs::script_held::Process`, the original's
    `fn_0070D480`), on the old land and before the seeds go back, as `ValidateScriptVariables` does before
    `GlobalGameLists::ClearMap` and `ResetState`. Its second call (0x552E0E) has nothing left to release here: the
    registry's reset takes every `ScriptHeld` with its entity. The scripts' variables are not cleared, in neither
    game. `fn_006EB680`, which drops the script's `HitObject` (+0x70) and `ObjectWhichHit` (+0x74) when their thing is
    not available, is not ported (a port row). The map-cycle test hook is a separate path into `Game::LoadMap`, unchanged.
  - **A variable holding a thing of the land before**: a variable holds the entity's number (index and version).
    `Registry::Reset` empties the registry with entt's `clear`, which gives every entity a new version as it goes; a
    new land's entity may take an old index but not the old number, so `Registry::Valid` refuses the old one and the
    natives say the thing is gone, as the original's. Pinned by `LandLoadThings` in `test_land_load_globals`.
    **⚠️ approximate** entt's version has 12 bits: after 4095 deletions of entities at the same index the number comes
    back, and a variable still holding the first would find the latest; the original's 32-bit id does not wrap in a
    game.
  - `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING`: `chlapi::InScriptFileList`, the same split and compare.
- **The debug menu's Load Island.** Without "Take your creature along" it only loads the island (`Game::LoadMap`).
  With it, it goes through `Game::ChangeLand` and does what the game does at that point:
  - a story land (`Land1.txt` … `Land5.txt`): every script task is stopped first, as `LandControlAll`'s
    `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING` does, and `LandControlAll` too (it waits on the old land's control script and
    would load the next land on top). After the load `LandControlK` is started; for Land 1 `LandControlAll`, as at a
    new game's start. The land's own script then brings the creature back where its story does (Land 2 in
    `Land2VortexEntry`, Land 3 in `SetupLand3`, Lands 4 and 5 in `VortexEntry`; Land 1 only with
    `IsKeepingOldCreature`);
  - a playground (a script in `Playgrounds`): every task is stopped, as `GScript::Reset(1)` reboots the scripts at the
    skirmish start; after the load the profile's creature is made at the first player's temple pen point and set to
    development stage 13, as the skirmish start's packet does (`Citadel::GetCreatureHomePos`, then `Creature::Load`
    and `MoveToDevelopmentPhase(13)`). Without a built temple, or when the creature cannot be made, one log line says
    so and no creature is made;
  - any other script: only `ChangeLand`.
  - Limits: the globals the skipped lands would have set (`ArkSailed`, `LethysKilled`) stay 0, so the story goes on as
    if those events never happened; no `LandControlAll` runs after a story land K ≥ 2, so leaving it loads nothing;
    Land 2's creature comes only after its arrival cinematic and its camera and dialogue waits. On a playground the
    four creature flags the skirmish start's packet sets to 1 (`Creature` +0x1108..+0x1114) are not set, and stopping
    the tasks is not the full script reboot of `GScript::Reset(1)`: the scripts' globals keep their values. If the
    load fails after the tasks are stopped, the old land stays with its scripts stopped and one log line says so.
    Automated runs never use the menu.
- **Our runs.** With the start-up answer 3 the vortex opens at about turn 31, but no run clicks the highlight
  (`OPENBLACK_IGNORE_REAL_INPUT=1`, a fixed mouse), so `LOAD_MAP` is never reached in fidelity's 140 turns or the
  cycle's Land 1 segment; even with a click at once it would come at about turn 172.
- **The writers**: `lhscriptx::WriteCommand(name, {values})` (`LHScriptX/ScriptWriter`): the command table copied from
  the exe, the format of 0x7E7FB0 (`lhscriptx::CommandAsText`), `ConvertToText`'s position text (`PositionText`) and
  the half-up rounding (`FixedHalfUp`: the exact decimal expansion of the double, then rounded half away from zero).
  It gives nothing for an unknown command, a wrong number of values or a value of the wrong type for its letter. A
  test checks that every command it can write has a reader with the same parameters.
- `ecs::land_script_save` (`ECS/LandScriptSave`): the save pass (`BeginSavePass` raises the save count), a writer per
  class registered by its owner, and the physics, dead tree and one-shot seed writers above (`SaveMobileStatic`,
  `SaveMobileObject`, `SavePot`, `SaveDeadTree`, `SaveOneOffSpellSeed`). A pot is skipped when it is a storage pit's
  pile (`StoragePitStore::OwnerOf`) or a workshop's (`workshops::WorkshopOfPile`). The static's own altitude is written,
  without openblack's ground drop of statics.
  - **⚠️ approximate** openblack keeps no object angles, so the MobileStatic and dead-tree angles come from the
    rotation (`GetYXZ` 0x7FAB30), equal to the stored ones to rounding but wrapped (4 rad comes out −2.28), where the
    exe writes the stored angles as they are.
  - **⚠️ approximate** the identical-object pass walks the cell in openblack's own order, not the exe's `FindType`
    chain; the result (all of them marked) is the same.
  - Every openblack MobileObject is of type 20; a Ball is not ported.
  - The MobileStatic's +0x7C and the MobileObject's +0x58 tests have no counterpart, and the town artifact's own line
    is not written.
  - openblack's DeadTree has no player: it writes PLAYER_ONE, the local player of a single-player game.
- **Footpaths**: `FotFile::Load` (`src/Serializer/FotFile.cpp`) makes one footpath per saved one
  (`ecs::footpaths::Create`), its nodes from their saved MapCoords and flags, `active` from the saved +0x20, and
  resolves the links as it reads them.

### Test hooks

- `OPENBLACK_TEST_SKIP_ANSWER=<answer>,<frame>`: answers the SkipBox at that frame, as its OK would.

## Pending

- **Changing land**:
  - not ported: `ValidateScriptVariables` 0x6EB6F0's first part, `fn_006EB680` (the hit pair cleared), a port row;
    whether openblack keeps a hit-object pair at all is not checked;
  - ⚠️ approximate a variable's dead thing can come back as another after entt's 12-bit version wraps (4095 deletions
    at the same index, [above](#changing-land-load_map)); the original's 32-bit id does not;
  - the loading frame and its tip image, `vortex.txt` (the objects taken by an In vortex), the tribute and prayer
    power, and whether the day and night clock is set back (openblack sets it to noon) are not ported or not checked.
- **The SkipBox's checkbox texts** (5..8 of the table at 0xD17CA0) are not read.
- The writers of the other classes (each class's owner): the full list of `SaveObject` writers and the commands they
  write is in the old land-script writers report, to be merged by each class's page.
- The saved-game path, since openblack has no saved games.
- A building site's pile is linked through +0x74: whether its +0x78 is set too is not read. 0x66DA00 also clears
  +0x78 when its owner is not available (not ported, minor).
- The MobileStatic writer's +0x7C test and the town artifact's line (`TownArtifact::SaveObject` 0x4269C0), the
  MobileObject writer's +0x58 test and the dead tree's player are not ported.
