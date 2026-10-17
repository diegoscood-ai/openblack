# Land script objects

## Summary

The small objects a land's map script makes: street lanterns, bonfires, mobile statics and rocks, dead trees, pots,
flowers, animated statics (the Norse gate and its lamps, the gate stone plinth, the Piper cave entrance), the planned
citadel, and the test that keeps trees and mobile objects from being made on top of fixed objects. Also the CHL side:
deleting a thing by script (OBJECT_DELETE) and the reward chests.

openblack makes every one of these with its own archetype, at the original's place in the script and with the
original's parameters. The collision test that rejects a creation is ported; the citadel is still made already built
(a difference that needs the town desires and building sites first), and part of OBJECT_DELETE's modes are not
ported.

## The original

### Map script objects (street lanterns, bonfires, dead trees, gates)

#### The parameters and the infos

- In the script's argument block, the integer of parameter i is at +0x6000 + 4i and the float at +0x6030 + 4i.
- `GMobileStaticInfo` takes 300 bytes in memory (0xD3A6D8 + 300·i: MS[6] = 0xD3ADE0, MS[7] = 0xD3AF0C, MS[8] =
  0xD3B038) and 284 in `info.dat`: in memory the info.dat record starts at +0x10 (the object type is at info +0x10
  and the clip of an AnimatedStatic at +0x128, which is +0x118 in openblack's `GAnimatedStaticInfo`). The 61
  MobileStatic infos have object type 0x1C (MOBILE_STATIC). The tables: [info.dat](../formats/info-dat.md).

#### Street lanterns

- **CREATE_STREET_LANTERN** (case 80, 0x717720) → `GStreetLantern::Create` 0x7346E0(pos, &MS[N1]). It creates
  nothing if in the map cell of the position (`MapCoords::FindType` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0;
  cells of 10 units) there is an object of type 0x1C less than 0.5 m away in x/z (`GUtils::GetDistanceInMetres`
  0x74CD70): any thing made with a MobileStatic info, so rocks, bonfires, street lanterns and dead trees.
  - `FindType(0x1C, prev)` at 0x7346EC / 0x734716 walks the cell's fixed list from its head (type 28 counts as
    fixed); the distance at 0x7346FC is `GetDistanceInMetres` 0x74CD70 (the table hypotenuse 0x74F680 on the two
    MapCoords), `fcomp 0.5; test ah, 1`. A multi-cell object is found in every cell of its NewCollideDescriptor.
- +0x58 = (info ≠ MS[7]). `CallVirtualFunctionsForCreation` 0x734810: mesh 148 (MSH_B_CAMPFIRE) if +0x58, otherwise
  398 (MSH_O_TOWNLIGHT); `SetPosition((x, GetAltitude + y, z), angle 0, scale 1)` (no 180° turn), the light
  `fn_00823240`(that point, +0x58) at +0x5C and, **in both classes** (it does not check +0x58; an older note said
  "only in the village one"), the sound 0x93 at +0x60 (`fn_0071E8C0` = `SoundTag::Create`), unless the object carries
  the UNAVAILABLE mark (+0xA & 1). The sound: [Day and night](../world/day-and-night.md).
- Land1: 8 of type 7 and 4 of type 59 (field lanterns with the bonfire mesh, **not** bonfires). The CHL CREATE with 7
  or 59 goes through the same place.
- CREATE_STREET_LIGHT (case 81, 0x717763) creates a `GStreetLight` (0x734E60), not decoded. No land uses it.

#### Bonfires, mobile statics and dead trees

- **CREATE_BONFIRE** "AFFF" (case 73, 0x7176AE) → `fn_00439850`(pos, F1 temperature, F2 Y angle, F3 scale) → ctor
  0x4395C0: `Rock`(pos, MS[8], angle, scale) and `CreateSpotVisualWithSpecifiedDuration`(pos, 25 SF_Bonfire, 1.0,
  −1 = forever, the bonfire). The temperature is not used on creation (Land1 has 24.7). No lantern light.
- **MobileStatic**: `CREATE_MOBILESTATIC` "ANFF" (case 41, 0x716D46) → `fn_00608770`(pos, info, 0, 0, F2 angle, F3
  scale): MS[8] → `Bonfire::Create` with temperature 100; info +0x128 = 2 → `Rock`; MS[6] → nothing (0x6087E1); the
  rest `MobileStatic`.
- `CREATE_MOBILE_STATIC` "ANFFFFF" (case 42, 0x716DC1) → `fn_00608840`(pos with relY = F2, info, 0, 0, F3, F4, F5,
  F6): MS[6] → GBaseOnly `fn_00609340`; MS[7] (the street lantern) → nothing (0x608869); the rest `fn_00608770`(…,
  F4, F6); then `SetXYZAnglesAndScale`(F3, F4, F5, F6) on what was created (also the base and the bonfire). So
  lanterns only come from CREATE_STREET_LANTERN.
- **CREATE_DEAD_TREE** "ALNFFFF" (case 43, 0x716E64) → `fn_00510BB0`(pos, GTreeInfo[N2], player, F3, F4, F5, F6, 0):
  ctor 0x510A30 = `Rock`(MS[3], angle 0, scale 1) + `SetLife`(F3); with 0xCC5F10 = 0, `GetDeadTreeMesh` 0x510C60 is
  the normal mesh of the type; then `SetXYZAnglesAndScale`(F4, F5, F6, 1), the MobileStatic matrix (x = F4, y = F5,
  z = F6). Land1: 3 (types 12, 4 and 4, life 1, small angles).

#### Pots, features and flowers

- **CREATE_POT** (case 38): `IsOkToCreateAtPos` and, if the quantity N3 ≤ 0 (0x716B19), nothing (this removes the 4
  empty wood piles of Land1). It calls `Pot::Create` with its int argument 1 (0x716B39): the pot sends its reaction
  when it is created (for food, hungry grazers come to eat).
- **CREATE_NEW_FEATURE** (case 75): with N5 ≠ 0 it creates a `PlannedFeature` 0x527440 (not drawn); no land uses it.
- **CREATE_FLOWERS** "ANFF" (0x71682E..0x716880) → `Flowers::Create` 0x5279E0(GetScriptPos(A0), &GFlowersInfo[N1]
  (0xCC9748 + 0x128 × N1, unchecked), F2 angle, F3 scale): the floats as they are, with no × 0.001 (CREATE_FEATURE
  scales its integers, 0x716797). The result goes to [0xD99380] and [0xD99384] (the last created object). The object
  (0x7C bytes) is a `MultiMapFixed` (ctor 0x52E1E0 with percent 1.0, not planned) with the Flowers vtable 0x8D91C8;
  against Feature's 0x8C2320 it overrides only the destructor, `GetDebugText` (0x5279A0), `GetSaveType` (0x527990 =
  0x10A; Feature's 0x74), `CallVirtualFunctionsForCreation` (0x527A30) and `Get3DType` (0x527980 = 1). Every other
  slot, the script type and the fire's included, is Feature's. No shipped land creates flowers.
- **Names**: features `fn_00527740` and animated statics `fn_00422600` compare with `_stricmp` (if there is none,
  they return the number of infos, 0x4C / 0x10); `GAbodeInfo::GetInfoFromText` 0x405A70 walks the 9 tribes, compares
  the prefix with `_strnicmp`, requires '_' and the description with `_stricmp` (16 per tribe); otherwise, −1. The
  original uses the result without checking it.

#### Animated statics

- `CallVirtualFunctionsForCreation` 0x422300 sets the clip from info +0x128 (read at 0x42241F..0x422446; Norse Gate
  191, Gate Stone Plinth 195, Piper Cave Entrance 189). With mesh 212 (Norse Gate, `fn_004230D0`) it creates 2
  `Game3DObject` with mesh 398 at (∓15, 30, 0) of the gate's matrix (rows with scale + translation), angle 0 and
  scale 1, each with the light `fn_00823240`(its position, 0). The lamps are built at 0x42246B..0x422571 and are not
  map objects.
- **AnimatedStatic::Draw** 0x422770 (the land's object draw list calls it for each object it draws; the symbol reads
  `ChessPion::Draw`, a shared body) steps the clip first thing, before the rest of its draw:
  - it reads its 3D object's clip time (vt +0x18C, 0x42277E);
  - open (+0x7C == 1, 0x422786): time + `g_game_time_inc` [0xEA9EC0] (the frame's game ms), and above the resting
    place of the clip vt +0x184 gives (`fn_00422750` 0x422750: (frames (+0x38) − 2) × play time (+0x20) / frames,
    `imul` / `idiv`, signed) it is the resting place (`jle`, 0x4227A8);
  - anything else: time − `g_game_time_inc`, and below 0 it is 0 (`jns`, 0x4227B0..0x4227B8);
  - the time is set back (vt +0x188, 0x4227C1); then come the lighting (`fn_00801C90`, 0x4227D6) and the haze
    (`fn_007FEB30`, 0x4227E0) and the rest of the draw, which draws the model at that time. A frame the list does
    not draw it, its clip does not move.
  - then the type switch (0x4227E7..0x422815: the info's index, (info − 0xC54D30) / 300, through the byte table
    0x422A90 and the jump table 0x422A80): the Norse Gate (0) draws itself (`Game3DObject::AddForDrawing` 0x63B5D0)
    and, when it is on screen, its two lamps with its light and haze (+0x4C / +0x50 copied, vt +0x100); the gate
    stone plinth (1) is below; the cave entrance (2) and the phone box (15) only draw themselves (0x422A6C); the
    chess pieces (3..14) draw nothing.

#### The gate stone plinth

The plinth's case of the draw (0x42286F..0x422A6B): it draws itself (`AddForDrawing(model, plinth)`), keeps
`g_b_last_on_screen` [0xEA1AF0] as its own (saved at 0x422879, set back at 0x422A62), then its three stone slots
(+0x80, +0x84, +0x88: the 3D objects `AddGateStone` 0x422D50 made, empty slots skipped but counted):

- While it is moving (`IsMoving` 0x422CD0, 0x422884), each stone sinks: with t = the plinth's clip time (vt +0x18C,
  `fild`) / (the clip's play time [clip +0x20] − 3, an integer, then `fild`; 0x4228B4..0x4228E7), h the stone's half
  height and H the plinth's (each model's `GetMesh` vt +0xF8, then [mesh +0x28], 0x4228EB / 0x422928), the stone
  takes the plinth's whole 3x4 matrix (12 dwords from +0x14, `rep movsd` 0x422913) and its y [+0x3C] gets
  ((2H + (slot · 2h − (7.15 · 2h) · t)) − 0.2) added (0x4228F1..0x422945), then it is drawn with
  `AddForDrawing(stone, NULL)` (0x42294B), with no object to stand for under the cursor.
- When it is closed (+0x7C == 0, 0x422965; a closing plinth draws both sets), each stone stands stacked: the plinth's
  matrix again, y + ((2H + 2 · (slot · h)) − 0.2) (0x422989..0x4229D1), its position set (vt +0x34 with its +0x38,
  0x4229DD), the plinth's light and haze (+0x4C / +0x50, 0x4229E0..0x4229F5), drawn (vt +0x100, 0x4229FD); and when
  it is on screen, `fn_005C9090`(plinth, `g_last_distance` [0xEA1AF4]) on [[0xD0195C] +0x25005C], and with
  `g_last_selected_box` [0xEA1AD0] `SendObjectDrawCollision(plinth, 0, stone)` 0x5D56C0 on `GGame::MyInterface`
  0x555850 (0x422A03..0x422A41): the cursor finds the stone as the plinth.
- The two constants, read in runblack.exe's data: [0x8C19BC] = 0x40E4CCCD = **7.15f** (`fld` 0x42290D) and
  [0x8AB244] = 0x3E4CCCCD = **0.2f** (`fsub` 0x42293C and 0x4229C8). [mesh +0x28] is the half extent in y of the
  model's box, unscaled (`LH3DMesh::ComputeBoundingBox`, see [Object size](../engine/object-size-and-map-cells.md#object-size));
  the 2 · h the lifts use is the model's whole height, and the size the matrix carries is not applied to it.

#### The planned citadel

- **CREATE_PLANNED_CITADEL** (case 20): town and player mandatory; `fn_00467DD0` (PlannedTownCitadelHeart in the
  town) and stores the position at 0xC5E258. When the map loads **there is no temple**, only the plan
  (GameThingWithPos 0x4C, no mesh, no cell, no creation index, no flattening; `Draw` 0x648930 = `ret`). Info
  "Citadel Heart" (info.dat 0x115C0): wood 5, timeToBuild 150, desireToBeBuilt 1.0, mesh 564 BuildingDummyCitadel;
  abode type of the plan 0x804 (civic).
- The real temple comes from `PlannedTownCitadelHeart::CreatePlannedNoFixedCheck` 0x467EF0 (vtable +0x504: the
  player's `Citadel` if it has none, `fn_00462B10`, and `CitadelHeart::Create` 0x464E20), which is called by
  `Town::AddBuildingSiteNoFixedCheck` 0x73B8A0 from `Town::RequestBestPlanned`, `Town::ForceBuildingOfPlannedAtPos`
  0x73E560 (`GScript::BuildBuilding` 0x6FAB30 of CHL, and 0x641774 after `StartPlaygroundGame` with 0xC5E258) and
  `Scaffold::TryToBuildPlannedBuilding`. `GGame::Birthday` → `GPlayer::Birthday` → `Town::Birthday` only rebuilds
  statistics.
- The conversion 0x467EF0 (argument `float life`): the player is the **owner of the town** (`Town+0x2C`, the one from
  CREATE_TOWN or the neutral one), not the script's (that one is only validated). `CitadelHeart::Create`(pos, info,
  citadel, plan angle, plan scale, life, 1): the 1 marks "under construction" (MultiMapFixed 0x52E1E0, +0x58 bit 1,
  +0x5C = 0). `CallVirtualFunctionsForCreation` 0x4675A0 creates the LH3D type 8 at **scale 1** with y =
  altitude(origin) + alt and calls 0x882730 (mesh B_FIRST_TEMPLE, % built, **flattens the land**, then the outside's
  first blend: [The temple's outside](../magic/temple-and-citadel.md#the-temples-outside-alignment-and-size)): the
  flattening happens on conversion. Worship sites (`fn_00464F50`) only if life ≥ 1. Then heart+0x94 = town,
  `PostCreatePlanned` 0x648C50 and the plan is deleted.
- `AddBuildingSiteNoFixedCheck` always passes life 0.0 and creates a `CitadelBuildingSite` (0x468DC0 → 0x43D1E0); the
  villagers finish it (`CitadelHeart::Built` 0x465000). `Draw` 0x882A40 uses `DrawPartialyBuilt` 0x816AD0 while the
  **percent built** (+0x5C, passed as +0x9C) is below 1.0, not the life, and with the inner-wall offsets [0xC392AC] /
  [0xC392B0] set to 1.0 (both cull modes) for that draw, then restored to 0.2 / 0.35 (0x882A7B..0x882AA7).
- Triggers: **Land 1** = CHL `FollowUs`: `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)` (the plan position from
  Land1.txt:95; `GetPlannedAtPos` 0x73E4C0 takes the nearest plan less than the radius of mesh 564 × scale + 1 m
  away), then `CALL_NEAR(Citadel 18)` + `SET_PROPERTY(22, 0.375)`; `PreventCitadelCompletion` limits it to 0.9 and
  `CheckCitadel` waits for 1. `BUILD_BUILDING` (`GScript::BuildBuilding` 0x6FAB30) pops the desire first, then z,
  y, x (0x6FAB3F..0x6FAB81), and calls `Town::ForceBuildingOfPlannedAtPos(MapCoords(LHPoint(x, y, z)) 0x603160,
  desire × 5.0 [0x8AB6E4])`. **Lands 2-5** = the AI: `Villager::CheckSatisfyCivicBuildings` 0x758E90 (town desire
  FOR_CIVIC_BUILDING 6, 0x748330) → `RequestBestPlanned` 0x73A650 → `GetBestPlanned` 0x73A140 (mask 4).
- **CREATE_CITADEL** (`Citadel::CreateCitadel` 0x463240) passes (angle, 1.0, 1.0, 0) to `CitadelHeart::Create`: the
  script's scale is **ignored** (Kapa's Land1 Playground passes 0, other maps 300 or 4121) and it comes out built.
- The citadel's script side: [Towns and citadel](map-script-reader.md#towns-and-citadel).

#### IsOkToCreateAtPos

- `IsOkToCreateAtPos` 0x638C40 fails if `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe`
  0x601D10 gives bit 8 and the cell is not water. Bit 8 comes from a `NewCollide::Obj` circle of radius 0.5
  (0x82AD90) against the `GetCollideData` (vtable +0x858) of each fixed object in the cell's list +4
  (`Obj::Collide` 0x829140); the other bits come from `MapCell::Collide` 0x601BD0 (bit 0x10 of the land block,
  outside the map).
- **Who calls it**: only CREATE_TREE (27, 0x716235), CREATE_NEW_TREE (28, 0x7162EE), CREATE_POT (38, 0x716B0C,
  before looking at the quantity) and CREATE_MOBILEOBJECT (40, 0x716C71). If it fails, it creates nothing, writes
  nothing and the script continues. Angle and scale are not used. The CHL handlers do not call it.
  CREATE_TOWN_CENTRE uses another one (`GAbodeInfo::IsOkToCreateAtPos` 0x404B10; in Land1-5 it rejects none).
  Abodes, fields, features, mobile statics, bonfires and dead trees are created without checking anything.
- **The test**: a circle of 0.5 at the script's (x, z) (the height does not count, `MapCoords(char*)` leaves y = 0)
  against the objects of **its cell**; a 2D test `dx² + dz² <= (ra + rb)²` and then the children. With water in the
  cell (bit 0x10, `hasWater`; `MapCoords::IsWater` 0x6035B0) it is always created; outside the map (or in an empty
  block) too.
- **Shapes**: a tree = a circle of 0.3 at its position, only in its cell (0x74C5F0). MultiMapFixed (abode, centre,
  field 594, feature, animated static, mobile static, rock, bonfire, dead tree, dispenser) = `NewCollide(LH3DObject)`
  0x829390 from the mesh's bbox: the bbox centre rotated with `x' = x·cos a − z·sin a`, `z' = x·sin a + z·cos a`;
  half-axes `max(1, scale·half)` in x and z; if long/short > 1.4, an outer circle `sqrt(ex²+ez²)` with
  `int(long/short)+1` children of the short radius in a row along the long axis (0x82ADD0 / 0x828F40); otherwise one
  circle of `max(ex, ez)`. It is put into every cell whose circle (cell centre, 7.1) it touches. The bbox (0x8081B0)
  passes the boned meshes (flag 0x100) through `LH3DAnim::SetTransform`. Without collide data: BigForest, pots,
  mobile objects, villagers, animals, street lanterns and planned buildings.
- Not decoded: the collide data of fish farms, CitadelHeart (0x468FB0) and WorshipSite (0x77E490).

### Deleting by script (OBJECT_DELETE)

`GScript::DeleteObject` 0x6F92D0 (native 48, its table entry written at 0x700AC4 by the name "OBJECT_DELETE") [V]:

- It pops the mode, then the object's id. A thing no longer there (`GetScriptGameThing` 0x70D220 null): it returns at
  once (0x6F9307), with no `RemoveScriptGameThing`. `GetScriptGameThing` gives null for id 0 or id >= 0x200 (with an
  error), for an empty slot (a warning), and for a thing that is not available (`call [vt+0x2C]`, `IsAvailable`, at
  0x70D28B, `je 0x70D2DB`): a thing marked for deletion, or a dying villager. So every native that resolves its object
  through it does nothing on such a thing. Then `IsInScript` (vt+0x448) and, unless flag +0x25 & 4, vt+0x440 with the
  running script's id (0x70D2B7..0x70D2D1) [V].
- A puzzle game first, whatever the mode (the virtual `IsPuzzleGame`, vt +0x498; only `PuzzleGame` answers 1,
  0x6F9311 / 0x561B40): `RemoveScriptGameThing`, then its reset `fn_006D6CC0(this, 0)`, and it returns. It is never
  deleted here.
- Then by the mode:
  - **0**: no class test. The virtual `ToBeDeleted(0)` (vt +0x0C). For a creature that is `Creature::ToBeDeleted`
    0x474F00, its whole removal (it saves the mind, turns the leash off, deletes the things it owns, leaves
    `CreatureList`, removes its reactions, then `Living::ToBeDeleted`).
  - **1**: only an Object (the virtual `IsObject`, vt +0x460: 1 for Object, 0 for a bare `GameThingWithPos`, a town
    or a puzzle game). A creature (vt +0x34 `IsCreature`, then a dynamic cast from Object to Creature, 0x6F9389) gets
    `Creature::SetFizz(1.0, 2.0, true)` and is not deleted (0x6F93A3). Any other Object gets `GoolooGooloo` (a 500 ms
    ghost of its model), then `ToBeDeleted(0)` (0x6F93B9..0x6F93C7).
  - **2**: only an Object, a creature included: its mesh is broken up (`fn_00681230` → `fn_00681260(object, point,
    15.0, 3.0, 0)`, 0x68123A), then the virtual `ToBeDeleted(0)`; for a creature, `Creature::ToBeDeleted` again
    (0x6F93EF..0x6F93FD).
  - **3**: only the citadel heart (`IsObject`, then a dynamic cast to `CitadelHeart`, 0x6F942D): with its byte +0xB8
    set, broken up with `fn_00681260(heart, point, 80.0, 3.0, 0)`; clear, `CitadelHeart::DestructionSequenceStart`.
    Nothing is deleted.
  - A thing the mode's class test refuses, and any mode past 3: nothing but the step below.
- Last, on every path but the gone thing: `RemoveScriptGameThing` 0x70D1A0, which disbands a script container
  (`IsScriptContainer`, vt +0x3F8, then `PUSH(id, 4)` and `DisbandId`) after the mode's action (0x70D1FA..0x70D212;
  0x6F947F). Despite its name it frees no script slot: its whole body is the two range / reference checks ("Jonty -
  Out of script range", "Script removing object with more than one reference left!", both through the empty
  `ScriptErrorMessage`) and the disband. The slot table (0xD967F8, 20-byte records, the reference count at +0x10) is
  written only by FindFreeScriptGameThingSlot, AddScriptGameThing, Increment/DecrementScriptReference, fn_0070D080
  and fn_0070D2F0. So no path of OBJECT_DELETE frees a slot (see
  [The script's things](script-commands.md#the-scripts-things-scriptmanage-slots)).
- The order matters only for mode 0 on a container: `ToBeDeleted` (vt +0x0C) runs first, and `Flock::ToBeDeleted`
  0x52FFB0 starts with `DeleteDependancys` 0x52FE60, which takes the flock off its player's list (+0xF08) and the
  game's list (+0x205C44), clears its shepherd's flock (+0x30) and empties its members (`Living::SetFlock(0)` on each
  whose flock it is, else the node dropped), before `GameThingWithPos::ToBeDeleted` marks it. The disband after it
  finds no member left (or no thing: `GetScriptGameThing` refuses a marked thing, vt +0x2C at 0x70D28B), so no member loses a reference or gets IN_SCRIPT through it. `Town::ToBeDeleted` 0x739970 and
  `Dance::ToBeDeleted` 0x50B970 ([Dances](../beings/dances.md)) are the other containers' own parts. The only shipped
  mode-0 delete of a container is losttreasure's `delete WolfPack` (a flock).
- **Which classes are Objects.** The classes under GameThingWithPos but not under Object answer 0 at vt +0x460
  (`IsObject@GameThingWithPos` 0x405550; `IsObject@Object` 0x4029F0 answers 1, and no class under Object overrides
  it: 52 vtables point to the first, 134 to the second). Those a script can hold are Town (0x999AE4), Flock
  (0x8DD108), Dance (0x8D3968), ScriptMarker (0x8E10B0), ScriptTimer (0x8E0BAC), Mist (0x8EB640), InfluenceRing
  (0x8EA3CC), WeatherThing (0x8EBC48), PuzzleGame (0x8F2338), Spell and its subclasses (0x9805B0...),
  GParticleContainer (0x8E2A14, SPECIAL_EFFECT_POSITION / _OBJECT), GComputerPlayer (0x8E41D8) and GArena (0x8C2C98),
  found through the callers of `AddScriptGameThing` 0x70D0F0. The CITADEL script type names the CitadelHeart, an
  Object, which is why mode 3 casts to it.

### Reward chests (CREATE_REWARD 239, CREATE_REWARD_IN_TOWN 240)

- **The natives** (`GScript` 0x6F2B30, 0x6F2C70): pop whether it falls from the sky, the point, the town (240), then
  the type, 1..60 ("Invalid type" and 0 otherwise; a town that is not one, "Thing not valid" and 0). Both call
  `Reward::Create(MapCoords, &GRewardInfo[type] (0xD50BF8 + type × 0x130), the local player, in the map at once = not
  from the sky, angle 0, scale 1, town)`. 240 names the town, not the chest, as the script's creation (0x6F2DE3).
- **Creation:** 27 dust sprites made with every chest, 5 draws each on the C runtime's stream (`Random` 0x81D180,
  see [Random numbers](../engine/random.md#the-c-runtime)): z (±2), y (0..2), x (±2), the cell
  `Random(0, 15) & 63` and the shade `Random(0.25, 1)` × (212, 180, 140). A chest put on the land has no dust time;
  one from the sky 2500 ms.
- **The fall** (drawn each world frame, fn_006E6770 from 0x5E6180): height 150 − 50t above the land, turned π t,
  placed by its clock before the clock runs on. On reaching the land: sample 174 of the in-game bank (ingame.sad), no
  owner, 2D, not tracked (fn_006E63C0 → `GAudio::PlaySoundEffect` 0x429E30), and a camera shake within 200 of the
  world's origin (0, 0, 0), 1.0, 400 ms, every axis (`LH3DCameraChecker::Create` 0x821050). It goes into the map's
  cells at the next turn's `Reward::ProcessList` 0x6E6890 (after the dead list, 0x54E70C). Weight 789; a physics
  object only once landed; the hand never takes it.
- **The dust** (fn_006E67A0, at the end of `Reward::ProcessDrawing` 0x6E675C): while landed and its time lasts, each
  sprite `LH3DSprite::AddDrawing` 0x840C70 (screen-plane, z-sorted by (x² + y²) + z², the smoke's material:
  smoke.raw, mode 6, two-sided), +0xC (the half width) = max((1 − left) × 1.8 + 0.7, 0.0001), alpha = ftol(255 ×
  left), left = whole ms × 0.0004; then the time −= the frame's whole ms (not below 0). Drawn on the landing frame,
  then from the next turn, once the landscape draws the chest.

### Sources

- runblack.exe read with the xref index (the script command switch, the classes' vtables), the shipped land scripts
  and CHL, and a Python simulation of the collision test (`sim.py`).

## openblack

- **Street lanterns**: `StreetLanternArchetype`, `components::StreetLantern` / `LanternLight`; `MobileStaticWithinHalfMetre`
  (StreetLanternArchetype.cpp) is `GStreetLantern::Create`'s test for a mobile static less than 0.5 m away; `night_lights` sets the
  lights by `LanternLight` (before, by the mesh, and the real bonfires came out with lantern light).
- **Bonfires**: `BonfireArchetype`. Land1's bonfire temperature (24.7) was once taken as the angle; it is the
  temperature, unused on creation.
- **Mobile statics**: `MobileStaticArchetype::CreateFromInfo` (`MobileStaticInfo::SingingStoneBase`, MS[6], makes
  nothing, as at 0x6087E1) and `MobileStaticArchetype::CreateWithXYZAngles` (the street lantern makes nothing, as at
  0x608869).
- **Dead trees**: `DeadTreeArchetype` (`components::DeadTree`, no Tree or forest, can be picked up); openblack once
  made a live burnt tree.
- **Flowers**: `FeatureScriptCommands::CreateFlowers`, `FlowersArchetype`; a row past the two of info.dat throws
  (`std::array::at`) where the original reads past the table.
- **Names** that are not found: openblack logs the failure and skips the command (a deliberate robustness
  difference; it used to throw), where the original uses the result unchecked.
- **The Norse Gate's lamps**: `CreateNorseGateLamps` (AnimatedStaticArchetype.cpp) makes the two lamps of
  `fn_004230D0`, not map objects; it takes the gate's `Transform` by value, since making the lamps can move the
  storage.
- **Animated statics**: the archetype gives every animated static `AnimatedStaticPose` (place 0, the resting place
  from its clip's `GetPlayTime` and keyframe count, `animated_static::OpenRestingPlace`) and a `SkeletalAnimation` of
  its clip at time 0, speed 0. The object draw list's consumer (`UpdateObjectDrawList` in `Game.cpp`) calls
  `AnimatedStaticSystem::Draw(object, FrameGameMs)` before the on-screen test of each object it draws:
  `animated_static::StepClip` moves the place, the clip's time follows it, and the bones are posed again with
  `graphics::ComputePoseInto`, as `ecs::UpdateAnimations` poses them (it ran earlier in the frame at the old time), so
  the frame draws the new pose. The gate's route circles (`AnimatedStaticSystem::RouteCircles`, see
  [Villager states](../beings/villager-states.md)) read the same place.
- **The plinth's stones**: `AnimatedStaticSystem::Draw` places a plinth's stones after its clip step
  (`animated_static::PlinthStoneDraws`, the formula above in the same order, with `ecs::object::MeshHalfHeight` of the
  plinth's model and of each stone's, and the clip's play time the archetype keeps in `AnimatedStaticPose::playTime`).
  Each drawn stone is an entity of its own (`PlinthStone` {plinth, not pickable}, `Mesh` of its stone, `Transform` =
  the plinth's rotation and size at its position raised by the lift), made, moved and unmade so that exactly the drawn
  ones exist, kept in `AnimatedStaticPose::stones` in the order they are drawn. The stones are not map objects, so the
  renderer's draw gate never leaves them out; instead, as the original draws them only from the plinth's Draw,
  `AnimatedStaticSystem::EndDrawnFrame` (once a frame, after the object draw list, beside
  `script_highlight::EndDrawnFrame`) takes away the stones of every plinth whose Draw did not run that frame (and of a
  plinth that is gone). A plinth that holds no stones makes nothing. Files: `src/Scenery/AnimatedStaticRules.{h,cpp}`,
  `src/ECS/Systems/Implementations/AnimatedStaticSystem.cpp`; tests `test/test_animated_static.cpp`.
- **The citadel**: CREATE_CITADEL draws at scale 1 (it used to use the script's: in Kapa's Land1 Playground the
  temple was invisible). CREATE_PLANNED_CITADEL requires a valid town and player (otherwise, nothing); the temple
  belongs to the town's owner (`Town::owner`) and is drawn at scale 1. It is still created already built (and
  flattening) on load, so that it does not disappear from Land 1-5 (see Pending).
- **IsOkToCreateAtPos**: `ECS/MapCollide.h/.cpp` (`openblack::ecs::map_collide`): a grid of cells emptied on
  LOAD_LANDSCAPE and filled with the script's parameters (the mesh from the created object's `Mesh`, the Y angle and
  scale from the script, not the Transform). The bbox of a boned mesh goes through the pose, like the original's. The
  water test is `ecs::sea_cells::IsWater` (`ECS/SeaCells.h`, the cell of the 16.16 MapCoords), the same as for
  physics, the hand and the AI (since 2026-10-01; before, it read the cell by hand). Not registered yet: fish farms,
  CitadelHeart and WorshipSite. `GAbodeInfo::IsOkToCreateAtPos` is not ported (it rejects nothing in Land1-5).
  - **Result** (checked with `OPENBLACK_DUMP_ENTITY_COUNTS`): Land1 1395 → 1351 trees (44 rejections: the 43 of
    `sim.py` − 2 under the Piper Cave Entrance + 3 under the dead trees), mobile objects 51; Land2 921 → 915 (+1
    pot); Land3 1397 → 1373 and 26 → 20 mobile objects; Land4 799 → 764 and 1 → 0 (+1 pot); Land5 804 → 782 and
    19 → 13; LandT 341 → 316. The differences from `sim.py`: the Piper Cave Entrance is a boned mesh and `sim.py` used
    the untransformed vertices; `sim.py` did not model the dead trees (they are Rocks with the type's normal mesh,
    created with angle 0 and scale 1, and the script's angles are almost 0). Screenshot: Land1 next to the Boulder1
    Lime (2120, 2494), now without the trees on top.
- **OBJECT_DELETE**: `script_type::ScriptDeleteOf` (`ECS/ScriptTypes.h`) makes the mode's choice, and
  `script_type::IsObject` the class test of modes 1 to 3. openblack tells the non-Object classes by their components:
  Town, Flock, ScriptTimer, Mist, InfluenceRing, WeatherThing, PuzzleGame and Spell. A creature in modes 0 and 2 goes
  through `ecs::creature_removal::RemoveFromGame`, its whole removal; mode 1 on a creature is not deleted: it gets
  `SetFizz(1.0, 2.0, true)` through `Locator::creatureFizzSystem`, and leaves the game the same way once it has fizzed
  right out; another object is marked to be deleted, and a thing the mode refuses stays. Like the original, it frees
  no script slot (the slot lives on the entity, the ScriptHeld component): a gone thing returns before anything in
  both. Code in `src/CHLApi.cpp`; tests `test/test_script_delete.cpp`. openblack differs: the lookup does not refuse a
  thing that is not available, only an invalid entity (`src/CHLApi.cpp:2248`), a candidate fix row (it differs only for
  a thing deleted earlier in the same turn; the other natives' shared lookup is to be checked the same way).
- **Reward chests**: `components::Reward`, `ecs::reward` (`src/ECS/RewardRules.{h,cpp}`), the `rewardSystem` slot
  (`RewardSystemInterface`, `Implementations/RewardSystem`); the dust is drawn with the smoke sprites
  (`Renderer::CollectBoatSprites`) from the reward system's frame list. Tests `test/test_reward.cpp`,
  `test/test_script_reward_natives.cpp`.

### Test hooks

- `OPENBLACK_LOG_ISOK=1`: one log line per rejection of `IsOkToCreateAtPos` (command, position, what blocks it).
- `OPENBLACK_DUMP_ENTITY_COUNTS=<frame>`: the entity counts per kind (Land1: 1351 trees and 51 mobile objects at
  600).

## Pending

- **The citadel** (a known difference, to be fixed): with no town desires, building sites,
  BUILD_BUILDING / SET_PROPERTY 22 / CALL_NEAR or partial drawing, openblack creates the temple already built (and
  flattening) on load. Making it faithful needs all of the above ported.
- **The plinth's stones under the cursor**: `PickObjectAlongRay` does not find the stones yet; the original sends
  them as the plinth (`SendObjectDrawCollision`).
- **CREATE_STREET_LIGHT** (`GStreetLight` 0x734E60): not decoded (no land uses it). Verify with xref
  (`xref.py show 0x734E60`).
- **The collide data** of CitadelHeart (0x468FB0) and WorshipSite (0x77E490) is decoded
  ([Temple and citadel](../magic/temple-and-citadel.md#the-hearts-and-the-worship-sites-collide-shapes)); openblack
  differs: one child radius for both (`src/ECS/MapCollide.h:26-33`), a fix row. The fish farms': not decoded, not
  registered.
- **OBJECT_DELETE**:
  - a script marker, a special effect's particle container and a dance are not Objects, but `script_type::IsObject`
    does not tell them apart (the marker has its `ScriptMarker` tag, which only `script_type::TypeOf` reads; the
    particle container has no component to tell it by; the dance, made by DANCE_CREATE, has its `components::Dance`,
    which `IsObject` does not list), so modes 1 to 3 still act on them (queued fix F-19); the computer player and the
    arena have no openblack form yet;
  - openblack disbands a container before the puzzle test and the mode's action, where the original does it after,
    inside RemoveScriptGameThing (0x70D1FA..0x70D212) (queued fix F-18). Only mode 0 on a container differs: openblack
    destroys the container at once and ports no Flock / Town / Dance `ToBeDeleted`, so the disband cannot simply move
    after it (the flock would be gone and its members left linked to it). It needs those class deletions in
    `ecs::ToBeDeleted` first; then the disband moves after the mode and finds the flock empty, as the original does;
  - mode 1's ghost, the break-up and mode 3 are not ported (queued fix F-20); its creature fizzle is;
  - openblack differs: OBJECT_DELETE's lookup does not test `IsAvailable` (`src/CHLApi.cpp:2248`), a candidate fix
    row.
