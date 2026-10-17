# The land script reader

## Summary

A land is built from two kinds of script. The **land script** (`Land*.txt`, read by the LHScriptX reader) is a list
of one-line commands that make the towns, abodes, villagers, flocks, climates, waypoints and other simulation data of
the land. The **challenge scripts** (CHL, compiled into `challenge.chl`) make things later with `CREATE` and
`CREATE_WITH_ANGLE_AND_SCALE`. This page covers both creation paths: how a position becomes the map's integer
coordinates, which types each command makes and what it leaves out, and the town, villager, flock, fish-farm and
build-percentage rules.

openblack reads land scripts and runs the CHL creation commands with the original's quantisation and type switch.
The villagers, animals, birds, flocks, climates, arenas, waypoints, fish farms and the build percentage are ported;
a few CHL types (Reward, Creature, DeadTree, Store, Timer, Totem, Highlight, Scaffold) and a few land-script commands
(`LOAD_FEATURE_SCRIPT`, `CREATE_BASE_WITH_ANGLE`) are not. What the challenge scripts do afterwards with the things
they made is in [Script commands](script-commands.md); the objects these scripts place (lanterns, bonfires, dead
trees, gates) are in [Map script objects](map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates).

Progress, row by row: [the progress index](../project/progress/INDEX.md) (land loading and the map; land script
commands).

## The original

All of this is ✅ faithful, read in the v1.42 `runblack.exe` with the 1.20 symbols, except what is marked
**🔍 inferred**, **⚠️ approximate** or **pending**.

<a id="creation-from-chl-create-27--create_with_angle_and_scale-252"></a>
### Creation from CHL

CHL `CREATE` (native 27) and `CREATE_WITH_ANGLE_AND_SCALE` (native 252).

#### The natives

- **`GScript::CreateThing` 0x6F1B20** (`CREATE`) pops z, y, x (0x6F1B32, 0x6F1B48, 0x6F1B5C) into a point, then the
  subtype (0x6F1B76), then the type (0x6F1B88). A type outside 1..41 gives "Invalid type" (silent) and pushes **0**.
  Otherwise it calls `fn_006F11A0(&point, type, subtype, angle 0, scale 1.0)` (0x6F1BAE..0x6F1BBC), then
  `AddScriptGameThing`, pushes the id and calls `SetScriptNameOfCreate`.
- **`GScript::CreateWithAngleAndScale` 0x6F2E10** pops z, y, x, subtype, type, then the scale (0x6F2E9E) and the angle
  (0x6F2EA0), which it turns from degrees into radians (× 0.0174533 [0x92B20C], 0x6F2EAF), then the same
  `fn_006F11A0` (0x6F2EE3..0x6F2EF0).
- When nothing is created the script receives **0** ("Thing not created").
- After creating, both call `AddScriptGameThing(thing, 1)` (`CREATE` at 0x6F1BCD, `CREATE_WITH_ANGLE_AND_SCALE` at
  0x6F2F01): a thing the script created ([The script's things](script-commands.md#the-scripts-things-scriptmanage-slots)).

#### The switch and the position

`fn_006F11A0` (no symbol; it lies inside the mis-sized `GScript::CallNearInState` 0x6F1010 entry):

1. Before any test it makes the map coordinates of the script's point, once (`MapCoords(LHPoint)` 0x603160 at
   0x6F11AB..0x6F11B4 → `MapCoords::Set` 0x603340): x and z × 6553.6 (the float 0x45CCCCCD = 6553.60009765625, the
   product rounded to a float by the 24-bit FPU) truncated towards zero by `__ftol` 0x7A1400, and the altitude
   y − `GetAltitude` at the quantised x, z ([MapCoords](../engine/coordinates-and-angles.md#mapcoords)).
2. Subtype 5000 is valid only for types 17, 36, 26, 27, 1, 28, 25 and 39 (Timer, SpellDispenser, Whale, Ark, Marker,
   Ball, Poo, Scaffold); otherwise "Trying to create an invalid sub_type" and null (the test at 0x6F11BD,
   0x6F11BD..0x6F1207).
3. The altitude is set to 0 unless the type is 1 MARKER or 8 MOBILE_STATIC (0x6F1208..0x6F1212).
4. The jump table 0x6F1A70 on type − 1 (0x6F1226). Every case that has a position is handed that one MapCoords (each
   case's `lea`); no case reads the raw point.

`MobileStatic::PhysicsEditorCreate` 0x55D720 sets the altitude to 0 again, so **only a marker keeps the script's
height**.

| Type | Case | What it makes |
|---|---|---|
| 1 MARKER | 0x6F122D | `fn_0070D8D0`: a `ScriptMarker` with only the position, the MapCoords copied to +0x14..+0x1C (0x70D90C..0x70D91E), altitude kept |
| 3 FEATURE | 0x6F1272 | `fn_00527350`(MapCoords, info, angle, scale, 1.0, 0) |
| 4 VILLAGER, 5 CHILD | 0x6F12A4 / 0x6F12F6 | `Villager::Create` 0x74FBE0 (below) |
| 6 ANIMAL, 21 BIRD | 0x6F133B | `fn_00419C20` (below) |
| 7 REWARD | 0x6F13E4 | `Reward::Create` 0x6E58C0 |
| 8 MOBILE_STATIC, 33 ROCK | 0x6F141E | subtype 6 → GBaseOnly `fn_00609340` (no angle or scale); 7 and 59 (0x3B) → `GStreetLantern::Create` 0x7346E0; the rest `fn_00608770` (info 8 → Bonfire; a Rock when info +0x128 = 2, else MobileStatic) |
| 12 CREATURE | 0x6F149C | `Creature::Create` 0x474A20 |
| 13 DEAD_TREE | 0x6F14CB | `fn_00510BB0` |
| 15 WEATHER_THING | 0x6F151E | 0x7741F0(MapCoords, info, 100, 300, 100) |
| 16 STORE | 0x6F154C | `fn_005FA8B0` on a copy of the MapCoords, then `fn_0066D1A0`(MapCoords) |
| 17 TIMER | 0x6F1253..0x6F1263 | `fn_007115A0((float)(uint64)subtype)`: the subtype is the length in seconds; no position |
| 20 MOBILE_OBJECT, 25 POO, 27 ARK | 0x6F138D / 0x6F1735 / 0x6F176B | 0x607000 (Poo = MobileObject 5, Ark = 23) |
| 22 TREE | 0x6F14F5 | `Tree::Create` 0x749EE0 (no forest) |
| 23 VORTEX | 0x6F171B | 0x5FE6E0(MapCoords, subtype, 50.0) |
| 26 WHALE | 0x6F1747 | `Whale::Create` 0x774C50(pos, &GMobileObjectInfo[24], 0, angle, scale): the shark |
| 28 BALL | 0x6F178F | `Ball::Create` 0x435B90, then `GetNearestTownToPos`(MapCoords) ([the football](../world/objects.md#the-football-ball)) |
| 30 ONE_SHOT_SPELL | 0x6F1807 | `OneOffSpellSeed::Create` 0x72A2F0(MapCoords) |
| 31 ONE_SHOT_SPELL_IN_HAND | 0x6F1823 | `CreateSpellIntoHand` 0x72A730: no position |
| 32 PUZZLE_GAME | 0x6F184C | `fn_006D6680`(MapCoords, subtype, ftol(angle · 2048 · 0.159155), scale) ([Fish puzzle](../world/sea-life-and-scenery.md#fish-puzzle)) |
| 34 TOTEM | 0x6F187C | `fn_006DA550` |
| 36 SPELL_DISPENSER | 0x6F18B2 | `SpellDispenser::Create` 0x7228D0 |
| 37 HIGHLIGHT | 0x6F1241 | nothing ("Highlight has it's own unique create fun") |
| 39 SCAFFOLD | 0x6F13BF..0x6F13D5 | `Scaffold::Create`(pos, GScaffoldInfo 0xD959D0, 0, 0, angle, scale) (`fn_006E8460`) |
| 41 ANIMATED_STATIC | 0x6F18E8 | 0x421F50; `AnimatedStatic::CallVirtualFunctionsForCreation` 0x422300 places its mesh with `LH3DObject::SetPosition` 0x423140 (called at 0x4223A4, `__fastcall`, ecx = the 3D object +0x40): its map position × 1/6553.6 (0x8AA3A4), the altitude from `LH3DIsland::GetAltitudeAndSetColorSpecular` 0x803340 plus its height offset, its y angle (vt+0x508 `Object::GetYAngle`) and its scale (vt+0x120 `Object::GetScale`); `SetPosition` takes a fast path when the angle is 0 and the scale 1 (0x423141..0x423165) [V] |
| 2, 9, 10, 11, 14, 18, 19, 24, 29, 35, 38, 40 | 0x6F191D | "Invalid create type": Abode, Town, Dance, Flock, InfluenceRing, Citadel, WorshipSite, SpellSeed, Mist, Field, ComputerPlayer and TotemStatue make nothing |

So x and z are quantised for every type that has a position; only TIMER and ONE_SHOT_SPELL_IN_HAND have none.

#### The common step after the switch

0x6F1591..0x6F1A4D: a `dynamic_cast` to Object. If it is an Object and vt +0x7B0 returns 0, it skips to the script
name. Otherwise `PhysicsEditorCreate(type == 8)` (vt +0x108, 0x6F15D3), then, for an Object with a Game3DObject
(+0x40), `MoveMapObject` (vt +0x55C) on its own MapCoords, and the matrix rebuilt at
(x · 1/6553.6 [0x8AA3A4], `GetAltitude` 0x803090 + relY, z · 1/6553.6) (0x6F1614..0x6F163A) with only the Y angle and
the object's scale (0x6F1591..0x6F1A42): no X or Z tilt.

`PhysicsEditorCreate` per class: GameThingWithPos 0x401980, MobileStatic 0x55D720 and Bonfire 0x4397C0 set relY
(+0x1C) to 0; ScriptMarker (0x561030), LandscapeVortex, Reward, Whale, FieldCrop and ScriptHighlight do nothing;
MobileObject 0x607AB0 and Rock 0x6E79F0 run `InitialisePhysics`, `AdjustToGroundLevel` 0x7FCB80 and
`RaiseUntilNotIntersecting` 0x644800 (not traced further).

#### A marker returns its own vector

`MapCoords::Set` 0x603340 stores `y − GetAltitude`, `GET_POSITION` 0x6F88A0 returns `GetAltitude + relY` and
`ScriptMarker::PhysicsEditorCreate` 0x561030 does nothing, so a marker gives back exactly the vector it was created
with (y = 0 in CHL).

#### Villagers and children

Types 4 and 5 (table entries 0x6F12A4 and 0x6F12F6). The info is `GVillagerInfo` 0xDA6BE8 + subtype × 0x3A4, and the
villager is made with no town or house:

- villager: `Villager::Create` 0x74FBE0(pos, info, grownUpAge (info +0x138) + 1, 0) at 0x6F12CA, then
  `SetAge(grownUpAge + 1)` again (vt +0x8D4 = `Villager::SetAge` 0x7528C0) at 0x6F12E0, then
  `GScript::SetScriptState(v, 4 IN_SCRIPT)` 0x6F82E0 at 0x6F12E9;
- child: `Villager::Create`(pos, info, 10, 0) at 0x6F1315, `SetAge(10)` at 0x6F1325, `SetScriptState(v, 4)` at
  0x6F132E.

The second `SetAge` gives the same age (the birth turn does not move and no mesh changes, since the age crosses no
grownUpAge), but it runs `InitialiseScale` 0x74FB80 and `SetScaleForAge` 0x752A90 again (0x752A62, 0x752A6D): an
adult takes 0.9 and then draws `GameFloatRand(0.1)` twice (the second draw is always taken, as 0.9 < 0.95), a child
draws `GameFloatRand` once ([Creation](../beings/villager-states.md#creation)). These draws follow those of
`Villager::Create` itself. Then `SetScriptState` (no drowning test: only `IsAvailable` and `IsObjectInMap`) takes the
villager from the state it was born in (85 CREATED, or 16 DROWNING at sea) into IN_SCRIPT, with its exit and entry,
`SetAnim(1)` and +0x58 = 0 ([SET_SCRIPT_STATE](script-commands.md#set_script_state-017-set_script_ulong-020-played-064-and-the-script-states)).
Both happen inside the switch, before the common step at 0x6F1591.

The scripts that use it: the Land 1 intro's family (`FollowUs`: mother 4/49, father 4/53, son 4/52 at about
(1578, 2220), the three `CREATE` calls at file offsets 0xDBD38, 0xDBE00 and 0xDBEC8 of `challenge.chl`), and many
other challenges (75 `CREATE` of type 4 with a constant subtype in `challenge.chl`).

#### Animals and birds

Types 6 and 21 (0x6F133B): `fn_00419C20`(pos, info, 0, 0), then on its +0xB8 record +0x50 = 2 and +0x52 = 1, a call
to 0x417C20(250.0) and `SetScriptState(4)` at 0x6F1380 (**pending**: not read).

### Animals and flocks (CREATE_FLOCK, CREATE_NEW_ANIMAL)

- **CREATE_FLOCK** "NAANNN" (0x71634A): `Flock::Flock` 0x52F780(A1, the current player, id N0) → id at +0x8C,
  +0x60 / +0x6C = A1, +0x50 = 0x50, +0x52 = 0x1E, put at the front of the list g_game +0x205C44;
  `SetDomainCentrePos`(A2) → +0x14. The domain radius +0x50 = N3 (0 → 0x50). With `VERSION` ≥ 2.1 (0xD9957C; every
  land has 2.3; the test at 0x7163D2) the flock distance +0x52 = N4 and the town N5; before that the town is N4 and
  +0x52 stays 0x1E. With a town: +0x34 and the town's list +0xF08. A flock is invisible (simulation only).
- **CREATE_NEW_ANIMAL** (0x716543; CREATE_ANIMAL 0x71649F is the same with age 0) looks the flock up by +0x8C in that
  list (the newest with that id) and the town with `FindTownWithID`, then `fn_00419D10`(pos, info, town, flock, age):
  - with a flock: age 0 → `GameRand(20)` + 5; it creates the animal (`fn_00419E00`) and joins it (`fn_0052FA50`: the
    list +0x3C sorted by the animal's byte +0xD4, +0x48 members, `Living::SetFlock`); if the animal cannot be herded
    and the flock has a town, the flock leaves the town's list and +0x34 = 0; +0x88 = the largest number of members;
  - without a flock (`fn_00419C20`, also CHL `CREATE`): age 0 → **`GameRand(40)` + 5**; the animal gets its own flock
    (`Flock(Living*)` 0x52F950, at its position, with no script id, +0x50 = info.domainRadius (+0x25C),
    +0x52 = (int)info.flockDistance (+0x21C), no town);
  - the animal's town (`fn_00417C50`, +0xE0 and the town's list +0x984): only animals that can be herded keep it
    (`IsOkToBeShepherd`, vt +0xBA4 = 0x41D0E0 → 1).
- **Classes** (`fn_00419E00`, a jump on info.animalInfo +0x1F4, 27 cases): ground animals (lion, tiger, wolf,
  leopard, SpellWolf, PieceLion / Wolf / Villager; constructor 0x41FD30 or 0x416EB0), grazing animals (sheep,
  tortoise, cow, horse, pig, PieceSheep; constructor 0x41D0B0, can be herded) and flying animals (crow, dove,
  swallow, pigeon, seagull, bat, SpellDove and SpellBat; constructor `Dove` 0x41DCF0). Types 5 (goat), 7 (zebra),
  17–19 and above 26 (the puzzle horse, cow, tortoise and pig) **create nothing**.
- **Flying animals**: the `Dove` constructor 0x41DCF0 calls the Animal one (0x416EB0, which calls
  `Living::SetState` 0x5F2A80), resets fields (`fn_00417900`) and sets the altitude of its MapCoords (+0x1C) to info.altitudeNormal
  (+0x278: dove, pigeon and bat 20; crow, swallow and seagull 40); `Game3DObject::SetPosition` 0x63B680 places them at
  `GetAltitude + altitudeNormal`. `CallVirtualFunctionsForCreation` is 0x41F240 (the Animal one plus a call to the 3D
  object). `StandAnimation`: dove 8 (DOVE_FLAP), swallow 27 (SWALLOW_FLAP), seagull 22 (SEAGULL_TAKEOFF), bat 2
  (BAT_GLIDE). Their flight: [Birds](../beings/animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat). They count
  as Object in the creation counter; 81 of Land 1's 116 animals are birds.

### Map simulation data (data only, nothing is drawn)

#### Towns

- **SET_TOWN_UNINHABITABLE** (case 5, 0x715542): town +0x5F4 = 1.
- **SET_TOWN_CONGREGATION_POS** (case 6, 0x715580): `FindTownWithID` (none: nothing, 0x717E8A); town +0xF10 =
  `GetScriptPos`(N1) (0x7155A3..0x7155B9, all three dwords), the cache of `Town::GetCongregationPos` 0x7408B0. Every
  shipped `SET_TOWN_CONGREGATION_POS` has two fields.
- **CREATE_TOWN_CENTRE** (case 9, 0x71577C): the town or the nearest one (`fn_00552FF0`); `IsOkToCreateAtPos`
  0x404B10; `Abode::Create`; for a TownCentre, town +0x9A4 = the centre if it was empty, and
  `Town::SetWorshipPercentage`(N5 · 0.001) 0x73C060, which stores +0x5C0 **only if the town has a worship site**
  (otherwise 0) and passes it to the totem statue. Without a town, `TotemStatue::SetWorshipPercentage` 0x738270.
  Every land passes 0.
- **CREATE_PLANNED_ABODE** (case 8, 0x715629, shares code with CREATE_ABODE): the town or the nearest one, otherwise
  nothing; abode type 0x404 (TownCentre) → `PlannedTownCentre::Create` 0x7444D0, otherwise
  `PlannedAbode::Create` 0x405600 (pos, info, town, angle N4 · 0.001, scale N5 · 0.001; the food and wood are not used); both go on the
  town's planned list and are never drawn (`PlannedMultiMapFixed::Draw` 0x648930 is a `ret`).
- **CREATE_NEW_FEATURE** (case 75, 0x716973): with N5 ≠ 0 a PlannedFeature 0x527440, never drawn, which matters only
  to the town's building plans. No land uses it.
- **CREATE_STREET_LIGHT** (case 81, 0x717763) makes a GStreetLight (0x734E60), not decoded; no land uses it.
- **TOWN_DESIRE_BOOST** (command 86 of `GSetup::MapCommands`, 0x7179EC): `GGame::FindTownWithID` 0x552FA0 and
  `fn_747270` (the desire's name, `_stricmp`); both found → town +0xD4[d] = boost (0x717A26), with no re-sorting and
  no range check (Land2.txt: "Abodes" / "Civic_Buildings" −0.75).

#### Arenas, climates and waypoints

- **CREATE_ARENA** (case 69, 0x7175FA) → `fn_00424820` → GArena 0x4246F0 (pos, radius +0x30, list g_game +0x205C7C);
  its GLightSheet (a grey 180 light on the land) is drawn only while a fight is on (`GArena::Draw` 0x4249E0, +0x34).
- **Climate** (cases 60–63): `CREATE_WEATHER_CLIMATE`(id, info, pos, r1, r2) (case 60, 0x7171F5) →
  `fn_00771300`(pos, &GClimateInfo[N1], F3, F4, 0, id N0): id 0 = `GClimate(0)` 0x771020 (ignores the rest); otherwise
  GClimate 0x771170 (pos +0x14, the radii sorted into +0x20 / +0x24, id +0x28, info +0x2C; the first rain and
  temperature from the season's range), list g_game +0x205CF4
  ([GClimate](../world/weather.md#gclimate-the-climates)). `_RAIN`(id, F1, N2, N3, N4) (case 61, 0x717250 → 0x773200)
  → +0x34 {F1, N2, N3, (u8)N4}; `_TEMP`(id, F1, F2) (case 62, 0x7172A2 → 0x773290) → +0x44 / +0x48; `_WIND`(id, F1,
  F2, F3) (case 63, 0x7172E0 → 0x7732D0) → +0x4C... The climate is looked up by id (`fn_007731B0`, the newest); id 0
  uses the world climate g_game +0x250534, created on the fly; an unknown id does nothing. Land 1: zones 1 (2701,
  2567; −35 / −32 degrees, snow), 2 and 3.
- **CREATE_DRINK_WAYPOINT** (case 95, 0x716616) → 0x770BC0 (WayPoint.cpp, list g_game +0x205C74): an invisible point
  where the creature drinks. Land 1 has 47.
- **CREATE_STREAM_POINT** (0x717550): the point sits on the ground and is appended at the tail (`GStream::AddPoint` 0x733B90).
- **CREATE_FOOTPATH** (`fn_00717171`, 0x717639..0x71764D): `new GFootpath(0, 0)` (0x534EB0, which pushes it at the
  list's head). The script's footpath number is not stored.
- **CREATE_FOOTPATH_NODE** (0x71765F..0x7176AC): the node goes to each footpath of g_game +0x205C64 whose position
  from the list's head (`fn_00534F90`) equals the script's number (at most one; none → nothing). `GFootpath::AddPos` 0x534FC0 inserts `new GFootpathNode(MapCoords(pos), 1, 0)` at the list's head (0x7176A7).

#### Magic and land balance

- **FIRE_FLY_SPELL_REWARD_PROB** (case 88, 0x717998): `GMagicInfo::GetInfoFromText` 0x5FB3B0 compares, without case,
  with the names of the 42 magic effects (the first that matches; "NONE" is always 0; none → 42) → 0x52B630: out of
  range it does nothing; otherwise the table entry 0xCCFBAC[i] = p and the running sums at 0xCCFB04 are built again.
  It is not reset between lands.
- `GLandBalance::Values` 0xD1A280: 8 multipliers the island script sets with `SET_GLOBAL_LAND_BALANCE`. All are 1
  when a map loads (`GLandBalance::Init` 0x5E2890, from `GSetup::LoadMapFeatures`). Known uses: 4 = villager speed
  (`Villager::SetStateSpeed`), 5 = the wood value of trees (`Tree::GetWoodValue`), 7 = the belief speed constant.
- **SET_NIGHTTIME** → `GGameInfo::SetVisualTimeCycleFromMapEditor` 0x557BB0.

#### Globals

- `VERSION` (case 51, 0x716FF9) → 0xD9957C (0 again at each `LoadMapFeatures`, 0x718110); `SET_LAND_NUMBER` (case 82,
  0x7177A4) → g_game +0x205A08, read by `DesignedWaterFall` 0x5E3770 for the Land 3 / 4 scenery, by the influence and
  by the worship sites; `SET_TOWN_INFLUENCE_MULTIPLIER` (case 96, 0x717B5C) / `SET_PLAYER_INFLUENCE_MULTIPLIER`
  (case 97, 0x717B7B) → g_game +0x250078 / +0x25007C.
- **A land's load keeps the land number and both multipliers.** Their only writers are the GGame constructor (number
  0 at 0x54B7E4, multipliers 1.0 at 0x54B7AF / 0x54B7B5), `GGame::Init` (number 0 at 0x54F4ED, multipliers 1.0 at
  0x54F674 / 0x54F67A), the feature script's cases (0x7177B0, 0x717B68, 0x717B87) and the saved game's load
  (`GGame::Load` 0x5548C3, 0x554ABC, 0x554AF5). `GGame::Init` runs once, from `GGame::StartGame` 0x54C1A4 (called by
  `pc_main` 0x641EF4 alone); `LOAD_MAP`'s `GGame::LoadMap` 0x552F40 (`ClearMap`, `ResetState`, `LoadMapFeatures`)
  writes none of them, so the last land's values stay until the new land's script sets its own. Every story land,
  playground and `LandT.txt` sets all three, so the kept values are never read. Of these globals the land load only
  puts back the interact desire, g_game +0x205A50 = 0 (0x552F64).
- `GGame::LoadMap` 0x552F40 (every island but the first of the campaign) calls `Town::AsssignTownFeature` again
  (0x552F76), after `LoadMapFeatures` (0x552F6E).

### The map script reader (LHScriptX)

- Each command's argument types come from the type strings at 0xC20F20.. (CREATE_TOWN "NALNL", CREATE_ABODE
  "NALNNNN", CREATE_VILLAGER_POS "AALN").
- `LHScriptX<char>::Pram` +0x6000 holds 12 integer slots, one set for every script (a class static): `ScanLine`
  0x7E7540 stops at 12 (0x7E7715..0x7E77F9).
- `ScanLine` writes slot i only for an 'N' argument (`atol`, 0x7E77CC; 0x7E7734..0x7E77D7) or an int variable
  (0x7E77BE). Any other argument leaves the earlier command's value in that slot.
- `GetScriptPos` 0x718250 = `MapCoords(const char*)` 0x6031D0 → `MapCoords::Set` 0x603280: x = ftol(atof × 65536 / 10)
  (0x603299..0x6032AF), relative height 0 (0x6032B1), z from the second field (0x6032D5), and a third field, if there
  is one, goes to the height unscaled (0x6032E4..0x6032EF).
- Then `GetScriptPos` adds the offset [0xD99724] when it is set (`MapCoords::operator+=` 0x605410 at 0x71826F, in
  integers; 0x718261..0x71826F). Only the vortex reader sets it (`fn_0076FA50` at 0x76FA69, from
  `LandscapeVortexOut::ProcessContentsOfVortex` 0x5FE249), so a land's own script never has an offset.
- `GSetup::LoadMapFeatures` 0x7180B0 resets once per map, not per line (0x7180FE..0x71811A): no offset [0xD99724],
  the player override [0xC20D54] = −1, the tribe override [0xC20D58] = −1, VERSION [0xD9957C] = 0 (0x718110, so a land
  without VERSION does not keep the previous land's) and the last created object [0xD99384] = none.
- [0xD99384] = the object the last creation command made, written right after creating, even when null (0x715A9C,
  0x715B3F, 0x715D78). The vortex reader `fn_0076FA50` returns it.
- [0xC20D58], the tribe override (−1 = none, 0x7182A0..0x7182AE): CREATE_VILLAGER / _POS take that tribe's villager
  with the same number when there is one (0x715D37..0x715D5D). Only the vortex reader writes it (`fn_0076FA50` at
  0x76FA91).
- **LOAD_FEATURE_SCRIPT**: `MapCommandProcess` case 6 (jump table 0x715018 → 0x714DCB) calls
  `GSetup::LoadMapFeatures(path)` (the call at 0x714DD0; its argument is the script's string). It loads the file with
  `LHScriptX::Load` (0x718136), then assigns the town features (0x71813D), `GGame::Birthday`,
  `GPlayer::PostLoadCleanup` and `GStream::CreateAll` (0x718148..0x718152), as for the map's own script.
- The writing side (a thing written back as the line that makes it): [The writers](land-change-and-saving.md#the-writers).

#### The command callback and the last created object

- The reader runs one line through the land-script interpreter, whose command callback is fn_00715150 (+0x30 =
  0x715180). Every A position is read by `GSetup::GetScriptPos` 0x718250: `MapCoords(text)` 0x6031D0, then `+= *at`
  (`MapCoords::operator+=` 0x605410) when the offset [0xD99724] is set.
- The creation commands of the callback (switch at 0x71536E, jump table 0x717EBC) set the "last created" global
  [0xD99384]:

  | Commands | Writes |
  |---|---|
  | CREATE_TOWN | 0x715407 / 0x71541B |
  | CREATE_MIST | 0x715617 |
  | CREATE_ABODE, CREATE_PLANNED_ABODE | 0x7156EA..0x71576A |
  | CREATE_TOWN_CENTRE | 0x715858 / 0x715881 |
  | CREATE_TOWN_VILLAGER | 0x715A9C |
  | CREATE_SPECIAL_TOWN_VILLAGER | 0x715B3F |
  | CREATE_VILLAGER, CREATE_VILLAGER_POS | 0x715D78 / 0x715D8C |
  | CREATE_CITADEL | 0x715E7E |
  | CREATE_PLANNED_CITADEL | 0x715F3B |
  | CREATE_CREATURE_PEN | 0x716033..0x7160B6 |
  | CREATE_WORSHIP_SITE | 0x71613D |
  | CREATE_FOREST | 0x716193 |
  | CREATE_TREE | 0x716259 |
  | CREATE_NEW_TREE | 0x716319 |
  | CREATE_FLOCK | 0x7163C6 |
  | CREATE_ANIMAL | 0x716531 |
  | CREATE_FIELD | 0x716604 |
  | CREATE_DRINK_WAYPOINT | 0x71662D |
  | CREATE_FISH_FARM | 0x716710 |
  | CREATE_FEATURE | 0x7167D5 |
  | CREATE_FLOWERS | 0x71686F |
  | CREATE_SPELL_DISPENSER | 0x71692C |
  | CREATE_NEW_FEATURE | 0x716A3E |
  | CREATE_ANIMATED_STATIC | 0x716AB5 |
  | CREATE_POT | 0x716B46 |
  | CREATE_MOBILEOBJECT | 0x716C94 |
  | CREATE_SCAFFOLD | 0x716D34 |
  | CREATE_MOBILESTATIC | 0x716DAF |
  | CREATE_MOBILE_STATIC | 0x716E52 |
  | CREATE_DEAD_TREE | 0x716EF1 |
  | CREATE_CREATURE | 0x716F4A |
  | CREATE_AREA | 0x71704E |
  | CREATE_INFLUENCE_RING | 0x7171E3 |
  | CREATE_WEATHER_CLIMATE | 0x71723D |
  | CREATE_WEATHER_STORM | 0x71746F |
  | CREATE_WATERFALL | 0x7175E8 |
  | CREATE_ARENA | 0x71761B |
  | CREATE_BONFIRE | 0x7176DD |
  | CREATE_BASE | 0x71770E |
  | CREATE_STREET_LANTERN | 0x717751 |
  | CREATE_STREET_LIGHT | 0x717792 |

  The list comes from a byte scan of the stores `mov [0xD99384], reg` inside 0x715180..0x718000, mapped to the case
  starts of the jump table. The shared tail 0x717DA0 reads it.
- [0xD99384] is cleared by `GSetup::LoadMapFeatures` (0x71811A), by the text loader 0x7182E0 (0x718313; it sets the
  offset, the player and [0xC20D58] from its three arguments, then `LHScriptX::Load` 0x7E72C0), by the Out's ctor after
  it opens `vortex.txt`, and by the Out's contents when the last one is no longer available. fn_00718C10 (0x7190B0)
  sets the offset and the player for another loader.
- [0xD99380], the last created thing, is written by the 19 CREATE handlers of the same callback (0x715765..0x716EEC).
  Its only reader in all of .text is 0x7152F6, in the callback (`sub_715180`, the gap behind fn_00715150, whose
  address `GSetup::LoadMapFeatures` 0x718126 takes): it appends `[[0xD99380]+0x3C]` with `")%d\n"` [0xC2209C] (or
  `")\n"` when null) to a local echo of the command and its arguments (`"%.02f,"` / `"%d,"` / `"\"%s\","` at
  0xC220A4..0xC220B8), built in 0x7151B3..0x71534D and never output: a debug echo compiled without its output [V].

#### Towns and players

- `GGame::FindTownWithID` 0x552FA0 looks the town up by its script number (+0x5B4).
- CHL `GET_TOWN_WITH_ID` 368 (`GScript::GetTownWithId` 0x6F3060) writes the same loop inline: the popped float
  through `__ftol` (0x6F307A, towards zero), the players from `GetNextPlayerAndNeutral` 0x550980 and each one's town
  list (+0xA50, next +0x75C) compared with +0x5B4. Found: `AddScriptGameThing(town, 0)` 0x70D0F0 and its id pushed as
  an object (0x6F30DD..0x6F30F1); none: `ScriptErrorMessage` (a bare `ret`) and 0.
- CHL `BELIEF_FOR_PLAYER` 247 (`GScript::GetThingBeliefForPlayer` 0x6FCE60) pops the player, then the thing:
  `GetPlayer(ConvertScriptPlayerToGamePlayer(__ftol(p)))` (0x6EB9A0: 0 is the neutral slot, n is n − 1). No thing or
  no player: `ScriptErrorMessage` and 0.0. A town (vt +0x1B8) gives `Town::GetBeliefInPlayer` 0x73BAB0; anything else
  1.0 when its `GetPlayer` (vt +0x1C) is the player, else 0.0 (0x6FCF00..0x6FCF2B).
- The nearest town, `fn_00552FF0` (`ret 4`, one MapCoords, no id; called at 0x7156A6, 0x7157FF, 0x715AD6, 0x715B79,
  0x7168FE and 0x717E15): it walks the town list g_game +0x205C84 newest first (0x73964D), takes the first, then a
  later one only when `GUtils::GetDistanceInMetres` 0x74CD70 (through `fn_00605CD0`) is strictly smaller
  (`fcomp; test ah, 1` at 0x55301B). An exact tie goes to the newer town. Only x and z count.
- `GPlayer::GetPlayerFromText` 0x64B5E0: the player whose name matches without case (`_stricmp` 0x64B609), or else
  the neutral player (0x64B627..0x64B643).
- **SET_TOWN_BELIEF** (`fn_00715150`, 0x71542B): `GetPlayerFromText` (0x715432), `FindTownWithID` (0x715449; no town
  → nothing, 0x717E8A), then `Town::SetBeliefInPlayer` 0x73BA70(P, f) (0x715460): for the neutral player +0x5D8 = f,
  otherwise `SetBelief` 0x4387D0, capped. It overwrites the slot.
- **SET_TOWN_BELIEF_CAP** (0x715472): `GetPlayerFromText` (0x715479), `FindTownWithID` (0x715492),
  `GBelief::SetBeliefInPlayerCap` 0x438A00(P, f) (0x7154B5). The multiplayer case (0x7154BA..0x715528): if the land
  is `.\mpm_3p_1.txt` (`strcmp` with 0xD99648), `IsMultiplayerGame` 0x552F80 is true (0x7154FB), the town's +0x5B4 is 3
  and the player is not neutral, the cap is 2.0.
- **SET_TOWN_BALANCE_BELIEF_SCALE** (case 98): `FindTownWithID` (no town: 0x717BAE), then town +0x5DC = the scale
  (0x717BBA), which scales the town's pending belief when it is folded in.
- **SET_LOST_TOWN_SCALE** (case 104): [0xBF33F0] = the scale (0x717E85). `GLandBalance::Init` sets it back to 1.0
  (0x5E28A9). The town belief's fold reads it (`fn_004383D0` at 0x4384BA and 0x4386E4: the boredom and belief left in
  a player's towns when one is lost).
- **CREATE_TOWN_CENTRE_SPELL_ICON** (command 12) uses the same handler as CREATE_TOWN_SPELL (0x715B4A).
- **CREATE_SPELL_ICON** (command 13) does nothing in the original either (0x715B7C).
- **CREATE_INFLUENCE_RING** (case 59, 0x7171A3): `InfluenceRing::Create(pos, GGame::GetPlayer(player), radius,
  anti)`. `GetPlayer` does not check the index.
- **CREATE_WORSHIP_SITE** (command 19, 0x7160C8): only the player and the tribe are used. The site's position comes
  from its citadel slot.
- **CREATE_PLANNED_CITADEL** (0x715E91): it needs the town (`FindTownWithID`) and a player string that resolves
  (0x715EBE, else nothing). The `PlannedTownCitadelHeart` (constructor 0x467DD0) gets the info 0xC5E270 + N3 × 0x158,
  the angle N5 × 0.001 and the scale N6 × 0.001.
- **CREATE_MOBILESTATIC** "ANFF" (case 41, 0x716D46): `fn_00608770`(pos, info, 0, 0, F2, F3).
  **CREATE_MOBILE_STATIC** "ANFFFFF" (case 42, 0x716DC1): `fn_00608840`(pos with relY = F2, info, 0, 0, F3, F4, F5,
  F6).
- **CREATE_POT**: the handler's `Pot::Create` with its int 1 (0x716B39): the pot spreads its reaction at creation (for
  food, the hungry grazers come and eat).

#### Villagers from the map script

- **CREATE_TOWN_VILLAGER** (command 16, "NAAN", 0x715A4C): info = `GVillagerInfo::GetInfoFromText`(argument 2)
  (0x715A61), the 0x3A4-byte record at 0xDA6BE8 + 0x3A4 × info.
- **CREATE_SPECIAL_TOWN_VILLAGER** (command 17, "NANN", 0x715AF8): the info index is N2 itself ([ebp + 0x6008]),
  unchecked (0x715AF8..0x715B13).
- Both then do `Villager::Create` 0x74FBE0(`GetScriptPos`(A1), info, N3, 0) and set [0xD99384] (0x715A9C /
  0x715B3F). No villager → stop. Then the town from `FindTownWithID`(N0), else the town nearest the position
  (`fn_00552FF0`), else stop, and `Town::AddVillagerToTown` 0x73A090.
- **CREATE_VILLAGER** (command 15, "AAL", 0x715B9B with `cmp eax, 0xF`): A0 is also the abode's position (0x715BC9).
  The info is `GetInfoFromText` of the second argument, which is an 'A' position (0x715BAC), so it never names a
  villager and gives 0x54. The age is integer slot 2 ([ebp + 0x6008], 0x715BBC), which this command's 'L' does not
  write: what the last command with an 'N' third argument left. No shipped script uses it.
- **CREATE_VILLAGER_POS** (command 18, "AALN", also 0x715B9B, with `cmp eax, 0x12`): info = `GetInfoFromText`(argument
  2) (0x715BFF), age = N3 ([ebp + 0x600C]), the abode looked up at A0, the villager made at A1 (0x715C1B / 0x715C43).
- The abode lookup of CREATE_VILLAGER / _POS (0x715C5F..0x715D1B): every player, then the neutral one
  (`GetNextPlayerAndNeutral` 0x550980), their towns (GPlayer +0xA50, next +0x75C), their abodes (Town +0x754, next
  +0x9C, newest first). It takes the first abode whose door (vt +0x864, `MultiMapFixed::GetDoorPos` 0x52E370) is in the
  same map cell as the position (the high words compared, `cmp word [eax + 2], bx` at 0x715CAF / `cmp word [eax + 6],
  bp` at 0x715CC7).
- The abode's town comes from vt +0x48 (`Abode::GetTown` 0x401730; 0x715CFB..0x715D17). The abode itself counts only
  while it has room: info +0x174 MaxVillagers − the count +0xA4 == 0 → no abode (0x715D06..0x715D17).
- Then (0x715D1F..0x715E02): the info, or the override tribe's villager of the same number (+0x1FC) when
  `GVillagerInfo::Find` 0x752650 has one (0x715D51). `Villager::Create` 0x74FBE0(position, info, age, 0) with the
  position's height zeroed first (0x715D3F). [0xD99384] = the villager, even null; [0xD99380] gets it too (read only
  by a debug echo, [above](#the-command-callback-and-the-last-created-object)); leaving it out changes nothing. No villager → stop.
- Placement: the abode (`Abode::AddVillagerToAbode` 0x404060); else, in its town, `FindAbodeWithSpaceInTown`
  0x73B370(villager, 0) → `AddVillagerToAbode`, else `Town::AddVillagerToTown` 0x73A090; else the game's vagrants
  (g_game +0x205BFC, 0x715DC0).
- `GVillagerInfo::GetInfoFromText` 0x7519E0: for each of the 9 tribes (records 0xDA57B8, 0x1C apart; the name at
  [0xC22FDC + 4 × tribe]) whose name starts the text without case (`__strnicmp` 0x751A1B) and is followed by '_'
  (0x751A27; else the next tribe, 0x751A25 / 0x751A2B), it compares that tribe's 7 villager names with the rest
  (`__stricmp` 0x751A40, `GetTribeTextArray` 0x751AA0; none of the 7: the next tribe, 0x751A53..0x751A56). The result
  is tribe × 7 + villager (0x751A87..0x751A95). The 9 tribe records are in tribe order and each one's 7 villagers in
  VILLAGER_NUMBER order (🔍 inferred: they are filled at start-up).
- If no name matches it returns 0x54 (0x751A7D), one past the 84 records at [0xDA6BE8].
- `GVillagerInfo::Find` 0x752650: the **first** of the 84 records (0xDA6BE8, 0x3A4 apart) whose tribe (+0x1F4) and
  number (+0x1FC) match (0x752663..0x75266A, `je 0x752681`), else null (0x75267F).
- **🔍 inferred** The vagrants list g_game +0x205BFC starts empty on each map: the `GGame` constructor 0x54B3A0 zeroes
  it and each deleted villager leaves it (`DeleteDependancys` 0x74FE4B).

<a id="fish-farms-create_fish_farm--create_town_fish_farm"></a>
### Fish farms

`CREATE_FISH_FARM` and `CREATE_TOWN_FISH_FARM`:

- Case 31 (0x7166E1): 0x52C7B0(pos, GFishFarmInfo[N1] (0xCCFC78 + 0x128 · i; info.dat has only entry 0), no town).
  Case 32 (0x716722): without the town it creates nothing; otherwise the same with it. The constructor 0x52C360
  stores at +0x8C **always the nearest town** (`Town::GetNearestTownToPos` 0x73B170, any tribe), whatever the
  script's town.
- The fish shoal (`CallVirtualFunctionsForCreation` 0x52CC10): with [0xC37BF4] = 0 (the sea not flattened,
  `GetAltitude` 0x803090 uses the raw height), rings of radius 2, 4... below 50 and 32 directions; the first direction
  with a height of exactly 0 at two consecutive radii gives the centre. Nine of Land 1's farms (the lake of town 2 and
  others) get no shoal. The drawn shoal: [the sea pages](../world/sea-coast-and-rivers.md).

<a id="build-percentage-of-a-feature-built_percentage-chl-property-22"></a>
### Build percentage of a Feature

`BUILT_PERCENTAGE`, CHL property 22. **Faithful**.

- **The script** (Land 1): `TheMissionaries` creates `GArk = CREATE(3, 69 = ArkDryDock, (1881.083, 8.1316,
  3154.109))` and sets `BUILT_PERCENTAGE of GArk = 0.2` (the pattern `GET_PROPERTY; POPI 0; PUSHF v; SET_PROPERTY` is
  an assignment); `TheMissionariesBuildingBoat` adds 0.03 per hit (with `PLAY_SOUND_EFFECT(RANDOM_ULONG(92, 97))`) up
  to `ArkIncrement`.
- `GET_PROPERTY` 22 (0x70E1A9): `dynamic_cast<MultiMapFixed>` → `GetPercentBuilt` (vt +0x880 = 0x4014F0, +0x5C); not a
  MultiMapFixed → **1** (0x70E1ED). `SET_PROPERTY` 22 (0x70EC69): a MultiMapFixed → `fn_0052EDD0`: +0x5C = the value
  (0 if negative, **no cap**) and, if ≥ 1, `MultiMapFixed::Built` 0x52EBB0 (+0x5C = 1, +0x58 loses 0x02 and gains
  0x08, releases the building site +0x74, reaction 0xF if it has a town, `RequestChangeTexture`); then the town's list
  of buildings (0x70EC9B..0x70ECD4), which a Feature does not have. Anything but a MultiMapFixed jumps to 0x70F294:
  "Cannot Set Property %d" and nothing changes (also the common exit 0x70F2CC..0x70F2D4 of the other properties).
- **The first value**: `fn_00527350` → the `MultiMapFixed` constructor 0x52E1E0(pos, info, angle, scale, percentage,
  planned): planned → bit 0x02 and +0x5C = 0; otherwise +0x5C = percentage and bit 0x08. The script's `CREATE` passes
  1: built.
- **Drawing**: `Feature::Draw` 0x518690 = `MultiMapFixed::Draw` 0x518090: if `IsDrawBuilding` (vt +0x8A4),
  `DrawBuilding` 0x517F90. `Feature::IsDrawBuilding` 0x527790: **only for GFeatureInfo 69** (ArkDryDock) is it
  `!IsBuilt()` (0x422110: bit 0x02 clear and +0x5C ≥ 1); the other Features use `MultiMapFixed::IsDrawBuilding` 0x52F0C0 = "has a building site" (+0x74), which a Feature never has: they are always drawn whole. `DrawBuilding`:
  p = `GetPercentForDrawBuilding` 0x52EFD0 = min(`GetPercentBuilt`, `GetPercentRepairedFromWhenDamaged` 0x52F010, 1 if
  not built); with p = 0 nothing is drawn; otherwise vt +0x110 of the static object = `fn_00816AD0` (the half-built
  drawing of the houses: the main mesh cut at pos.y + 2 · ext.y · scale · p, with inner walls and a cap, and the
  scaffolding that rises (p < 0.2), whole or cut from above (p > 0.8)).

### Towns and citadel

Where the towns, the temple and the citadel are told:

- **Town data on load**: `SET_TOWN_UNINHABITABLE`, `CREATE_TOWN_CENTRE` (the centre and the worship percentage) and
  `CREATE_PLANNED_ABODE`, in [Map simulation data](#map-simulation-data-data-only-nothing-is-drawn). The totem of the
  town centre and the sinking of the abodes to their foundation: [Render](../project/internals.md#render).
- **The town's owner** (`Town` +0x2C, the player from `CREATE_TOWN` or the neutral one): it owns the temple that comes
  from `CREATE_PLANNED_CITADEL`, not the script's player
  ([Map script objects](map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)).
- **The temple**: on load there is only the plan (`PlannedTownCitadelHeart`); `CitadelHeart::Create` converts it and
  flattens the land (0x882730), and `CREATE_CITADEL` ignores the script's scale (same section).
- **Citadel and worship**: [The citadel and its six slots](../magic/temple-and-citadel.md#the-citadel-and-its-six-slots);
  its influence: [Influence](../magic/influence-and-alignment.md#influence).
- **Collision**: `IsOkToCreateAtPos` and the map collision grid (`MapCollide`), in
  [Map script objects](map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates).
- **Inside the citadel** (g_game +0x205A28 == 1): what plays in
  [The original GAudio](../audio/engine.md#original-gaudio-lhaudio-and-qmixer) and [Objects](../world/objects.md);
  the frame in [Other cases](../rendering/frame.md#other-cases-temple-video-and-2d).
- The town's trees (the scenic forest, the forest list Town +0x608):
  [Searches for trees](../beings/villager-jobs.md#searches-for-trees-and-forests-for-the-villagers).

### Sources

- runblack.exe (v1.42, with the 1.20 symbols): the map script's command cases, `GScript::CreateThing` 0x6F1B20 and its
  switch `fn_006F11A0`; the native table in bw1-decomp `src/Black/ScriptFunctions.cpp:74`
  (`{GScript::CreateThing, 5, 1, 0, "CREATE"}`) and `:299` (`{GScript::CreateWithAngleAndScale, 7, 1, 0,
  "CREATE_WITH_ANGLE_AND_SCALE"}`), and the type enum `include/chlasm/ScriptEnums.h:112` (SCRIPT_OBJECT_TYPE). No
  body is decompiled.

## openblack

Files: `src/LHScriptX/` (`Script`, `FeatureScriptCommands`, `MapScriptCommands`, `VillagerCommands`), `src/CHLApi.cpp`
(`CreateScriptObject`), `src/ECS/VillagerScriptRules.h`, the archetypes. Port decisions: [Port decisions: audio, video
and scripts](../project/port-decisions-audio-video-scripts.md).

- **CHL creation** (`CreateScriptObject`, `ScriptMarkerArchetype`): an unsupported type returns 0, as the original's
  "Thing not created" (openblack used to return entity 0, a valid entity). x and z are quantised the original's way
  (`map_coords::Quantise`) before the switch. Already ported: Vortex (`ecs::vortex::Create`), WeatherThing
  (`magic::script::CreateWeatherThing`), OneShotSpell, OneShotSpellInHand and SpellDispenser
  (`Magic/Script/CHLWorship.cpp`) and Whale (the shark). The Singing Stones circle is assembled at (2496.67, 2246.33)
  on the ground. An unimplemented CHL function is logged once.
- **Villagers made by CHL**: `villager_script_rules::CreateScriptVillager` (tested in `test_villager_script`) with
  `VillagerArchetype::Create`, the living action system's `VillagerSetAge` (`villager::SetAgeAndScale`) and
  `script_held::SetLivingScriptState`, so the second size draws are taken and the villager goes into IN_SCRIPT.
  Animals and birds are only created (the +0xB8 fields and `SetScriptState(4)` are pending).
- **Flocks and animals**: `components::Flock`, `Animal::flock / town`, `Town::flocks`, `RegistryContext::flocks`,
  `AnimalArchetype`; the birds' flight is ported (`AnimalClass::Flying` in `AnimalArchetype.cpp`).
- **Simulation data**: `Town::uninhabitable`, `Town::centre`, `Town::plannedAbodes`, `components::Arena`,
  `components::Climate`, `components::DrinkWaypoint`, the fire-fly table in `MapScriptGlobals`
  (`Locator::mapScriptSystem`, which also holds `VERSION`, the land number and the two influence multipliers);
  `town_desire::MapTownDesireBoost`; the lost-town scale through `SetLostTownScale` of `LandBalanceSystem`
  (`[0xBF33F0]`, read by the town belief's fold); `SET_NIGHTTIME` through
  `DayNightClock::SetCycleFromMapEditor`. `CREATE_STREET_LIGHT` makes a town lantern; `CREATE_NEW_FEATURE` with
  N5 ≠ 0 makes nothing. `ResetForLandLoad` in `Game::PrepareNewLand` puts back the interact desire, and
  `MapScriptGlobals`' start values stand for the game's start.
- **The reader** (`Script.cpp`): the 12 integer slots (`g_IntSlots`), the position offset, the last created thing and
  the tribe override (`Script::BeginMapFeatures` resets them once per map, with the vagrants list:
  `town_villagers::ClearVagrants`). openblack's command signatures stand for the exe's type strings
  (⚠️ approximate). Its parser makes a vector only of a two-field string, with y the terrain height, not a third
  field (⚠️ approximate: no shipped script has a three-field position). The player override [0xC20D54] is not
  ported: no command reads it yet.
- **Towns and players**: `town_queries::FindTownWithID`, `FindNearestTown` (`TownsNewestFirst`), `PlayerFromText`
  (not `GetPlayerName`, which throws); `GET_TOWN_WITH_ID` registers the town with `ScriptObjects().Register(town,
  false)` (`test/test_script_query_natives.cpp`); `BELIEF_FOR_PLAYER` reads the town's
  `town_belief::GetBeliefInPlayer`, a villager's `villager::GetPlayerOf` (its town's owner, none without a town) or a
  creature's owner, through `property_rules::BeliefForPlayer`. `SET_TOWN_BELIEF` now overwrites the slot (the old
  string map kept the first value). `CREATE_INFLUENCE_RING` adds a range check on the player. openblack plays no
  multiplayer land.
- **Villagers from the land script**: `villager_commands::VillagerInfoFromText`, `FindVillagerInfo` (the first match,
  not `GVillagerInfo::Find` of `InfoConstants.cpp`, which keeps the last match and throws), `FindAbodeAt`,
  `CreateVillagerInTown`, `CreateVillagerAtAbode`. `CREATE_VILLAGER` uses `k_NoVillagerInfo` (0x54) directly, as its
  text is a position; openblack makes no villager for it. **⚠️ approximate** `FindAbodeAt` takes the cell with
  `map_coords::FromMetres` (ftol(m × 6553.6f)), where the exe's `MapCoords::Set` is ftol(atof × 65536 / 10), which can
  differ by one unit at a cell edge; the door point is openblack's.
- **Fish farms**: the shoal search was already the original's (`GetUnflattenedHeightAt`), so the same nine of Land 1
  get no shoal.
- **Build percentage**: `components::Feature::percentBuilt`, `src/ECS/FeatureBuild.{h,cpp}` (`physics::PartialBuild`
  on the Feature's local mesh; no mesh at p = 0; the terrain footprint is kept), `GET / SET_PROPERTY` 22 in `CHLApi`.
  openblack's numbering is right: 22 = `BuiltPercentage`. At 0.2 the whole scaffolding shows and the ark is cut at one
  fifth; at 1, the ark stands on its props.
- **The temple** is created already built in `CitadelArchetype` (a difference still to fix, below); the flattening
  (flat up to 35 units, a blend up to 70) is in [Render](../project/internals.md#render). The temple entity holds the
  six worship slots (`components::Temple`). CitadelHeart and WorshipSite are not registered in the collision grid (`ecs::map_collide`) yet.

#### The command handlers

Each land-script command is a function of `FeatureScriptCommands` (the map's own commands are in
`MapScriptCommands`, where `MapScriptCommands::LoadFeatureScript` throws "not implemented"):

| Commands | Handlers |
|---|---|
| Towns | `FeatureScriptCommands::SetTownBelief`, `FeatureScriptCommands::SetTownBeliefCap`, `FeatureScriptCommands::SetTownCongregationPos`, `FeatureScriptCommands::SetTownBalanceBeliefScale`, `FeatureScriptCommands::TownDesireBoost`, `FeatureScriptCommands::CreatePlannedAbode`, `FeatureScriptCommands::CreatePlannedCitadel`, `FeatureScriptCommands::CreateWorshipSite`, `FeatureScriptCommands::CreateTownCentreSpellIcon`, `FeatureScriptCommands::CreateSpellIcon` (it logs that it does nothing) |
| Villagers | `FeatureScriptCommands::CreateVillager`, `FeatureScriptCommands::CreateTownVillager`, `FeatureScriptCommands::CreateSpecialTownVillager`, `FeatureScriptCommands::CreateVillagerPos` (each through `ValidVillagerInfo`) |
| Objects | `FeatureScriptCommands::CreatePot` (`PotArchetype::Create`, then `animal_ai::SetupPotReaction`), `FeatureScriptCommands::CreateMobileStatic` (`MobileStaticArchetype::CreateFromInfo`), `FeatureScriptCommands::CreateMobileUStatic` (`CreateWithXYZAngles`), `FeatureScriptCommands::CreateNewFeature`, `FeatureScriptCommands::CreateStreetLight` (`StreetLanternArchetype::Create` with `MobileStaticInfo::StreetLantern`), `FeatureScriptCommands::CreateInfluenceRing` (the range check against `PlayerNames::_COUNT`) |
| Weather and places | `FeatureScriptCommands::CreateWeatherClimate`, `FeatureScriptCommands::CreateWeatherClimateRain`, `FeatureScriptCommands::CreateWeatherClimateTemp`, `FeatureScriptCommands::CreateWeatherClimateWind` (to `magic::map_script`), `FeatureScriptCommands::CreateArena` and `FeatureScriptCommands::CreateDrinkWaypoint` (entities in `Locator::entitiesRegistry::value`), `FeatureScriptCommands::CreateStreamPoint`, `FeatureScriptCommands::CreateFootpath`, `FeatureScriptCommands::CreateFootpathNode` (`ecs::footpaths::Create`, `AtPositionFromHead`) |
| Globals | `FeatureScriptCommands::Version`, `FeatureScriptCommands::SetLandNumber`, `FeatureScriptCommands::SetTownInfluenceMultiplier`, `FeatureScriptCommands::SetPlayerInfluenceMultiplier`, `FeatureScriptCommands::FireFlySpellRewardProb`, `FeatureScriptCommands::SetLostTownScale`, `FeatureScriptCommands::SetNighttime` |

The helpers: `FindTown` and `TownKeyOf` (the script's town number as the town's key, `Town::id`, which
`AbodeArchetype` and `FieldArchetype` take; a number no town has gives `Abode::k_NoTown`, and they fall back on the
nearest town), `FindNearestTown`, `PlayerFromText`. The reader: `Script::RunCommand` (the integer slots,
`IntSlot`; the loop stops at 12, the limit 0xFBFD40), `Script::PositionOffset` / `SetPositionOffset`, `LastCreated`,
`SetTribeOverride` and `BeginMapFeatures`; `GetParameter` turns a position text into openblack's metres. The writer:
`CommandAsText` and `WriteCommand` ([In openblack](land-change-and-saving.md#in-openblack)). The villager names:
`VillagerInfoFromText`.

### Test hooks

- `OPENBLACK_TEST_BUILT_PERCENTAGE`: `BUILT_PERCENTAGE` of the Land 1 ark (the Land 1 script reaches `TheMissionaries`
  only after the creature is chosen).
- `OPENBLACK_LOG_ISOK=1`: one line per `IsOkToCreateAtPos` rejection.
- `OPENBLACK_DUMP_ENTITY_COUNTS`: the entity count by type after loading.
- The script command hooks (`OPENBLACK_SCRIPT_THING_TRACE`, `OPENBLACK_TEST_TEXT_CLICK`) are on
  [Script commands](script-commands.md#test-hooks); `OPENBLACK_TEST_SKIP_ANSWER` on
  [Land changes](land-change-and-saving.md#test-hooks).

## Pending

- **CREATE's height for a puzzle game and a weather thing**: the original's altitude is 0 before the switch
  (0x6F1212), so the script's y is dropped; openblack's `CreatePuzzleGame` and `CreateWeatherThing` add it to the
  land, and read the land at the raw x, z instead of the quantised one.
- **The scaffold's altitude**: openblack gives it `FromWorld(ground)`, an altitude h(raw) − h(quantised); the original
  passes altitude 0.
- **CREATE of a rock-class MOBILE_STATIC given a height**: the physics path after the switch (`InitialisePhysics`,
  `AdjustToGroundLevel`, `RaiseUntilNotIntersecting`) is not read, nor the virtual call vt +0x7B0 that decides whether
  the physics step runs: **🧪 verify with xref** (`xref.py show 0x6E79F0`, `xref.py vtcalls 0x7B0`).
- Whether openblack's `GetHeightAt` is bit-exact with `LH3DIsland::GetAltitude` 0x803090: **🧪 verify with the oracle**.
- CHL CREATE of an animal or a bird: the fields set on its +0xB8 record, the call 0x417C20(250.0) (not read) and the
  `SetScriptState(4)` after it ([Creation from CHL](#creation-from-chl-create-27--create_with_angle_and_scale-252)).
- CHL CREATE not ported: Reward, Creature, DeadTree, Store, Timer, Totem, Highlight and Scaffold.
- **The debug menu's Load Island** on a playground: the four creature flags `Creature` +0x1108..+0x1114 the skirmish
  start's packet sets to 1 (what they mean is not read), and the rest of `GScript::Reset(1)` (the scripts' globals are
  not cleared, `CleanGameForScriptReboot`, `HelpSystem::Reset`, `DeleteAllScriptCreatedGameThings`).
- `AnimatedStatic::Draw` 0x422770: which call in the rest of the draw poses the model's bones from the clip time it
  sets is not traced (openblack poses them as soon as the time is set).
- The gate stone plinth's stones (`AnimatedStatic::Draw`'s plinth case,
  [Map script objects](map-script-objects.md#map-script-objects-street-lanterns-bonfires-dead-trees-gates)):
  - the stacked stones are found by the cursor as the plinth (`SendObjectDrawCollision(plinth, 0, stone)`);
    openblack's `PickObjectAlongRay` skips every `PlinthStone` for now, so neither set is under the cursor
    (`PlinthStone::pickable` stays false);
  - `fn_005C9090`, called with the plinth and the stone's eye distance when a stacked stone is on screen, is not read;
  - the stones' light and haze: the stacked ones take the plinth's (+0x4C / +0x50), the sinking ones keep their own 3D
    object's, which are not traced; openblack lights each stone as any model;
  - the stacked stone's vt +0x34 (given its own position +0x38) is not read;
  - whether the plinth's model ([mesh +0x28] of an animated mesh, flag +4 bit 0x100) has its box made after
    `LH3DAnim::SetTransform` is the open question of the mesh box
    ([Object size](../engine/object-size-and-map-cells.md#object-size)); openblack takes the loaded box.
- `CREATE_BASE_WITH_ANGLE` is not implemented: the script line is skipped.
- `LOAD_FEATURE_SCRIPT` is not ported (it throws "not implemented").
- `CREATE_MIST` 263 and `SET_MIST_FADE` 264 of CHL (`CHLApi.cpp`) are not implemented.
- `GAbodeInfo::IsOkToCreateAtPos` 0x404B10 of `CREATE_TOWN_CENTRE`; register in the collision grid the fish farms,
  CitadelHeart (0x468FB0) and WorshipSite (0x77E490).
- A faithful citadel: the town desires, the building sites, `BUILD_BUILDING`, `CALL_NEAR`, the partial drawing
  (`DrawPartialyBuilt` 0x816AD0); until then the temple comes out built on load.
- `BELIEF_FOR_PLAYER` on a thing that is neither a town, a villager nor a creature: the other classes' vt +0x1C
  (Abode 0x405F70, Tree 0x55D8C0, GameThing 0x570130, Feature 0x5273B0, MobileStatic 0x6088B0, Rock 0x6E77A0, Spell
  0x55CDF0, Container 0x462A50) are not ported (the native logs a stub there); a slot with no player in it is not
  refused (`GetPlayer` gives none there): openblack reads the slot's belief, 0 for a player never in the land.
- `ADD_RESOURCE` on a villager: `Villager::AddResource` 0x7564D0 (Land 1's builders, listing lines 52628..52703).
- `GAME_THING_CLICKED` on a challenge scroll calls `SaveGameRoom::InstantSaveGame(14)` 0x792FB0: openblack has no
  save rooms.
- `SetScriptNameOfCreate` 0x56FA70 (the debug name) after FLOCK_CREATE, CREATE_TIMER and CREATE_HIGHLIGHT (0x6F1CEA,
  the name from `fn_006F6A10`).
- Whether the VM gives `type` as an int or as float bits (SET_SCAFFOLD_PROPERTIES): not verified.
- Which player `Flock::Flock` gets for FLOCK_CREATE or a FLOCK_ATTACH of two livings (g_game +0x18, 0xA60 apart).
- `k_NoVillagerInfo` (0x54, one past the 84 records of [0xDA6BE8]): what the exe reads there is not known (a queued
  fix).
- MOVE_GAME_THING on a flock: the original sets the destination of the flock's tail member and openblack the first
  member's; whether openblack's first member is the original's tail is not checked.
- The first rain and temperature of a climate, from the season's range, are not ported.
- `SET_TOWN_CONGREGATION_POS` with a three-field position does not reach the command in openblack (⚠️ approximate;
  no shipped script has one).
