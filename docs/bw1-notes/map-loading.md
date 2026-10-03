# Map loading and script functions

What the map script (`Land*.txt`, LHScriptX) and the CHL CREATEs do when loading a land: object creation,
mists, flocks and animals, simulation data (towns, climate, arenas), fish farms, script objects (street lanterns,
bonfires, dead trees, gates, planned citadel), `IsOkToCreateAtPos` and `BUILT_PERCENTAGE`. All **faithful** (read
in runblack.exe) except what is marked **(inferred)**, *deviation* or **pending**.

- [Creation from CHL](#creation-from-chl-create-27--create_with_angle_and_scale-252)
- [Map mist (CREATE_MIST)](#map-mist-create_mist)
  - [Drawing (LH3DMist)](#drawing-lh3dmist-fn_007fa300)
- [Animals and flocks](#animals-and-flocks-create_flock-create_new_animal)
- [Map simulation data](#map-simulation-data-data-only-nothing-is-drawn)
- [Fish farms](#fish-farms-create_fish_farm--create_town_fish_farm)
- [Build percentage of a Feature](#build-percentage-of-a-feature-built_percentage-chl-property-22)
- [Map script objects](#map-script-objects-street-lanterns-bonfires-dead-trees-gates)
- [Towns and citadel](#towns-and-citadel)
- [Script commands that move things](#script-commands-that-move-things-move_game_thing-033-and-friends)
- [Pending](#pending) · [Test hooks](#test-hooks) · [Sources](#sources)

## Creation from CHL (CREATE 27 / CREATE_WITH_ANGLE_AND_SCALE 252)

Disassembly in `documentacion\mapa\chl_creatething_6F11A0.txt`.
- `GScript::CreateThing` 0x6F1B20 and `CreateWithAngleAndScale` 0x6F2E10 (angle in degrees, ×0.0174533) only accept
  types 1..41 and call the switch `fn_006F11A0` (table 0x6F1A70). If nothing is created, the script receives **0**
  ("Thing not created"). openblack returned entity 0 (a valid entity) for everything unsupported.
- Subtype 5000 is only valid for Timer, SpellDispenser, Whale, Ark, Marker, Ball, Poo and Scaffold.
- **Marker** (`fn_0070D8D0`): a `ScriptMarker` with only the position. `MapCoords::Set` 0x603340 stores
  `y − GetAltitude`, `GET_POSITION` 0x6F88A0 returns `GetAltitude + relY` and `ScriptMarker::PhysicsEditorCreate`
  0x561030 does nothing, so the marker returns exactly the vector it was created with (y = 0 in CHL).
- The other objects stay on the ground: `PhysicsEditorCreate` (GameThingWithPos 0x401980, MobileStatic 0x55D720,
  Bonfire 0x4397C0) sets relY = 0; after creating, 0x6F1591-0x6F1A42 rebuilds the matrix at `GetAltitude(pos) + relY` with
  only the Y angle and the object's scale (no X/Z tilt).
- Cases: Feature `fn_00527350`(angle, scale); Villager `Villager::Create` 0x74FBE0 with age grownUpAge + 1,
  VillagerChild with age 10 (no town or house); Animal and Bird `fn_00419C20`; MobileStatic and Rock: subtype 6 →
  GBaseOnly `fn_00609340` (no angle or scale), 7 and 59 → `GStreetLantern::Create`, the rest `fn_00608770` (info 8 →
  Bonfire, rocks with info +0x128 = 2); MobileObject `0x607000`, Poo = MobileObject 5, Ark = 23; Tree
  `Tree::Create` 0x749EE0 (no forest); AnimatedStatic `0x421F50`. Abode, Town, Dance, Flock, InfluenceRing, Citadel,
  WorshipSite, SpellSeed, Mist, Field, ComputerPlayer and TotemStatue give "Invalid create type" in the original too.
  PuzzleGame (type 32, 0x6F184C): `fn_006D6680(pos, subtipo, ftol(ángulo·2048·0,159155), escala)` (see «Fish
  puzzle» in [water.md](water.md#fish-puzzle)). Pending in openblack: Reward, Creature, DeadTree, Store,
  Timer, Vortex, Ball, Totem, Highlight and Scaffold. Already ported: WeatherThing (`magic::script::CreateWeatherThing`),
  OneShotSpell, OneShotSpellInHand and SpellDispenser (`Magic/Script/CHLWorship.cpp`) and Whale (the shark).
- openblack: `CreateScriptObject` (CHLApi.cpp), `MarkerArchetype`. The Singing Stones circle is now assembled at
  (2496,67, 2246.33) on the ground. Unimplemented CHL functions are logged only once per function.

## Map mist (CREATE_MIST)

- `CREATE_MIST` "AFNFF" (0x7155C9) → `Mist::Create` 0x6063D0(pos with relY = F1, size F3, colour N2, k F4) →
  `CallVirtualFunctionsForCreation` 0x606420: `LH3DObject::Create(7)` (LH3DMist) at `GetAltitude(x, z) + F1`,
  +0x88 = F3, +0x90 = N2 >> 24 (alpha), flag +0x80 bit 1; only if F4 ≠ 1, +0x8C = F4 and bit 2. The constructor of
  LH3DMist 0x7F9560 sets +0x88 = 1, +0x8C = 3, +0x90 = 0x80 and the counter +0x84 = Random(0, 16) & 15.
- `Mist::SetFade` 0x606800(initial size, final size, initial alpha, final alpha, seconds): sets the initial size and
  alpha right away and `fn_00606880` adds one step per 0.1 s turn for seconds × 10 turns (alpha limited to
  0..255); `Get2DRadius` 0x606660 = scale × the largest x/z half-extent of the mesh. It is used by `CREATE_MIST` 263 and
  `SET_MIST_FADE` 264 of CHL (one call of each in challenge.chl; not done yet).
- Land1 has 17 (swamp, the piper's cave...). openblack: `MistArchetype`, `components::Mist`,
  `Renderer::CollectMists` / `DrawMist` (drawing further below).

### Drawing (LH3DMist, `fn_007FA300`)


- **Other mists** (API `mists::Submit(const MistDesc&)`, `src/Graphics/Mists.h`): whoever has their own
  LH3DMist objects (the storm puffs of `GWeather::DrawClouds` 0x83FC90) sends them every frame with position,
  size, ARGB colour, effect/normal branch, k and its counter; `CollectMists` clips them with the same sphere and they go into the
  same back-to-front list as the map ones and the blended models (`_frameMists`, `DrawMist(índice)`).

- Same mesh `mist.l3d` (dome of radius 20, base at the origin), smoke material 0xEA1ABC (`fn_0080BBD0`, mode 6:
  SRCALPHA/INVSRCALPHA blending, colour and alpha = texture × diffuse, no Z write, two-sided) and 8×8 atlas as the
  clouds. Creation (`CallVirtualFunctionsForCreation` 0x606420): +0x80 |= 1 always; if F4 ≠ 1, +0x8C = F4 and
  +0x80 |= 2 ("effect" branch). Colour N2 at +0x4C (ARGB), +0x50 (specular) = 0.
- **Rotation** (key): 0x7FA38F copies into the object's matrix the one at 0xEA1C98, which `UpdateCamera` 0x819A62 builds in two
  steps. First it permutes the columns of the world→camera A = 0xEA1D28: row i = (A[3i], −A[3i+2], A[3i+1]),
  translation 0. And **then** (0x819AC5 `mov ecx, 0xEA1C98`, 0x819AF3 `call fn_007FB3F0`; the other copy of
  `UpdateCamera`, 0x81A1AD, does the same at 0x81A265) it **inverts it in place**: `fn_007FB3F0` is the inverse of
  the 4×3 matrix (cofactors / determinant, translation = −t·M⁻¹). Being orthonormal, the inverse is the transpose, so
  the final rows are right, −forward and up. A uses row vectors and the object's matrix is applied the same way
  (x' = m0 x + m3 y + m6 z, `fn_0084BA90`), so row k is the image of local axis k: in glm
  **mat3(right, −forward, up)**, that is, a **billboard**. Local X = screen right, local Y (the axis of
  the dome) towards the camera, local Z = up, so the dome is always seen face-on, like a smoke disc, and
  never edge-on (checked by emulating 0x819690 + the permutation + 0x7FB3F0 with several cameras,
  `documentacion\mapa\emu_inv.py`). Its centre is on the ground, so the Z test cuts off the lower half of the disc (also
  in the original).
- **Effect branch** (bit 2; in Land1 they all have k = 1, in Land4/Land5 k = 3.78 / 2.64): s = size/(1 + (k − 1)
  (1 − |dy|/|d|)); 0x7FA4DC..0x7FA539 scale row 0 (local X) by the size and rows 1 and 2 (Y, Z) by s: **non-uniform
  scale**. Light at (0, 500000, 0), ambient 0xD2, no land light, atlas V + 0.25 (0x7FA44D: rows 2-3).
- **Normal branch** (0x7FA5B0): the 9 cells × size. `fn_00801C90` gives the light (table[lum] bilinear over the 4 cells)
  and leaves at +0x50 the bilinear RGB of those cells (the first dword read as D3DCOLOR: red = blue byte). `fn_007FEB30`
  applies the haze: light × (256 − trunc((256 − k) t)) >> 8 and specular += round(haze colour × t). Then each
  channel = floor(N2 × light / 255), alpha = alpha of N2, and the model lighting (light at (−500000, 500000, −500000),
  ambient 90). **Without** the + 0.25 of the atlas (0x7FA675: rows 0-1 of `smokea.raw`, peaks 171-197; rows 2-3 reach
  228-248).
- Per-vertex light (`fn_0084BA90`): I = round(255 · n_local · L_local), L_local = normalize(M⁻¹ (Lpos − pos)); with
  non-uniform scale it is not the light of the rotated normal.
- Counter +0x84 += ftol(g_game_time_inc · 0.255), modulo 900 only if it goes past 900; frame (counter/20) & 15. It only
  advances inside Draw, that is, with the mist on screen.
- Order: `LH3DMist::AddDrawing` 0x7FA7F0 discards with `CheckRegionOnScreen` (radius = mesh radius × size ×
  0.55) and sends the mist to the `LH3DZSorter` (key |pos − camera|², callback 0x7FA980), together with the transparent
  models and the sprites.
- openblack: `Renderer::CollectMists` / `DrawMist` (RendererMists.cpp) enter the back-to-front list of the
  main pass (`DrawPass`, `ZObject::mist` of the `graphics::zsorter` queue); without entities the mists go into the
  same queue as the clouds (there is no `DrawMists` any more). `vs_cloud`
  receives `u_cloudLight` (L_local) and `fs_cloud` adds `u_cloudSpecular` (0 in the clouds and in the effect branch).
  Deviation: the counter keeps the fraction (like `Clouds.cpp`), because without vsync openblack goes over 250 fps and
  the original's truncated step would be 0. The alpha texture `smokea.raw` is cut to 4 bits on load, as in the
  original (ARGB4444, `a.raw` 0x8375C1: 228 → 238/255; see
  [rendering.md](rendering.md#argb4444-textures)). The `graphics.smooth-smoke` mod (disabled by default) leaves it
  with its 8 bits, which is how it looked before.

## Animals and flocks (CREATE_FLOCK, CREATE_NEW_ANIMAL)

Disassembly in `documentacion\mapa\all_cases.txt` (cases 24, 25 and 49).
- **CREATE_FLOCK** "NAANNN" (0x71634A): `Flock::Flock` 0x52F780(A1, the current player, id N0) → id at +0x8C, +0x60/+0x6C
  = A1, +0x50 = 0x50, +0x52 = 0x1E, in the list g_game+0x205C44 (inserted at the front); `SetDomainCentrePos`(A2) → +0x14.
  Domain radius +0x50 = N3 (0 → 0x50). With `VERSION` ≥ 2.1 (0xD9957C; all lands have 2.3): flock
  distance +0x52 = N4 and town N5; before that, town N4 and +0x52 stays at 0x1E. With a town: +0x34 and the town's list
  +0xF08. Invisible (simulation only).
- **CREATE_NEW_ANIMAL** (0x716543; CREATE_ANIMAL 0x71649F the same with age 0): looks for the flock by +0x8C in that list
  (the newest with that id) and the town with `FindTownWithID` → `fn_00419D10`(pos, info, town, flock, age).
  - With a flock: age 0 → GameRand(20) + 5; creates the animal (`fn_00419E00`) and joins it (`fn_0052FA50`: list +0x3C
    sorted by the creature's byte +0xD4, +0x48 members, `Living::SetFlock`); if the animal cannot be herded and the
    flock has a town, the flock leaves the town's list and +0x34 = 0; +0x88 = maximum number of members.
  - Without a flock (`fn_00419C20`, also the CHL CREATE): age 0 → **GameRand(40)** + 5; the animal receives its own
    flock (`Flock(Living*)` 0x52F950, at its position, without a script id, +0x50 = info.domainRadius (+0x25C),
    +0x52 = (int)info.flockDistance (+0x21C), no town).
  - Town of the animal (`fn_00417C50`, +0xE0 and the town's list +0x984): only those that can be herded keep it
    (`IsOkToBeShepherd`, vtable +0xBA4 = 0x41D0E0 → 1); the others, none.
- **Classes** (`fn_00419E00`, jump on info.animalInfo +0x1F4, 27 cases): ground animals (lion, tiger, wolf, leopard,
  SpellWolf, PieceLion/Wolf/Villager; ctor 0x41FD30 or 0x416EB0), grazing animals (sheep, tortoise, cow, horse, pig,
  PieceSheep; ctor 0x41D0B0, can be herded) and flying animals (crow, dove, swallow, pigeon, seagull, bat,
  SpellDove and SpellBat; ctor `Dove` 0x41DCF0). Types 5 (goat), 7 (zebra), 17-19 and > 26 (puzzle horse, cow, tortoise and
  pig) **create nothing**.
- **Flying animals**: the `Dove` ctor 0x41DCF0 calls the Animal one (0x416EB0, which calls `Living::SetState` 0x5F2A80),
  resets fields (`fn_00417900`) and sets the altitude of its MapCoords (+0x1C) = info.altitudeNormal (+0x278: dove,
  pigeon and bat 20, crow, swallow and seagull 40); `Game3DObject::SetPosition` 0x63B680 places them at
  `GetAltitude + altitudeNormal`. `CallVirtualFunctionsForCreation` is 0x41F240 (the Animal one plus a call to the
  3D object). `StandAnimation`: dove 8 (DOVE_FLAP), swallow 27 (SWALLOW_FLAP), seagull 22 (SEAGULL_TAKEOFF),
  bat 2 (BAT_GLIDE). Their flight is already decoded and ported (commit e3a9d81f, «Birds fly like the original»;
  `AnimalClass::Flying` in `AnimalArchetype.cpp`, details in
  [animals.md](animals.md#birds-crow-dove-swallow-rock-dove-seagull-bat)).
  *Previously* openblack did not create them (81 of the 116 animals of Land1); they count as Object in the creation counter.
- openblack: `components::Flock`, `Animal::flock/town`, `Town::flocks`, `RegistryContext::flocks`, `AnimalArchetype`.

## Map simulation data (data only, nothing is drawn)

- **SET_TOWN_UNINHABITABLE** (case 5, 0x715542): town +0x5F4 = 1 (`Town::uninhabitable`).
- **CREATE_TOWN_CENTRE** (case 9, 0x71577C): town or the nearest one (`fn_00552FF0`); `IsOkToCreateAtPos` 0x404B10;
  `Abode::Create`; if it is a TownCentre: town +0x9A4 = the centre if it was empty (`Town::centre`) and
  `Town::SetWorshipPercentage`(N5·0.001) 0x73C060, which stores +0x5C0 **only if the town has a worship site** (otherwise,
  0) and passes it to the totem statue. Without a town, `TotemStatue::SetWorshipPercentage` 0x738270. All lands pass 0.
- **CREATE_PLANNED_ABODE** (case 8, shares code with CREATE_ABODE): town or the nearest one, otherwise nothing; abode
  type 0x404 (TownCentre) → `PlannedTownCentre::Create` 0x7444D0, otherwise `PlannedAbode::Create` 0x405600 (pos, info,
  town, angle N4·0,001, scale N5·0.001; food and wood are not used); invisible (`PlannedMultiMapFixed::Draw`
  0x648930 = `ret`). openblack: `Town::plannedAbodes`.
- **CREATE_ARENA** (case 69) → `fn_00424820` → GArena 0x4246F0 (pos, radius +0x30, list g_game+0x205C7C); its
  GLightSheet is only drawn during a fight. openblack: `components::Arena`.
- **Climate** (cases 60-63): `CREATE_WEATHER_CLIMATE`(id, info, pos, r1, r2) → `fn_00771300`: id 0 = `GClimate(0)`
  0x771020 (ignores the rest); otherwise, GClimate 0x771170 (pos +0x14, sorted radii +0x20/+0x24, id +0x28, info +0x2C;
  initial rain and temperature from the season's range, not ported), list g_game+0x205CF4 (GClimate in
  [day-night-weather.md](day-night-weather.md#gclimate-the-climates-climatecpp)). `_RAIN`(id, F1, N2, N3,
  N4) → +0x34 {F1, N2, N3, (u8)N4}; `_TEMP`(id, F1, F2) → +0x44/+0x48; `_WIND`(id, F1, F2, F3) → +0x4C..; the climate is
  looked up by id (`fn_007731B0`, the newest); id 0 uses the world climate g_game+0x250534, created on the fly; an unknown
  id does nothing. Land1: zones 1 (2701, 2567; −35/−32 degrees, snow), 2 and 3. openblack:
  `components::Climate`.
- **CREATE_DRINK_WAYPOINT** (case 95) → 0x770BC0 (WayPoint.cpp, list g_game+0x205C74): point where the creature drinks.
  Land1 has 47. openblack: `components::DrinkWaypoint`.
- **FIRE_FLY_SPELL_REWARD_PROB** (case 88): `GMagicInfo::GetInfoFromText` 0x5FB3B0 compares case-insensitively with the name
  of the 42 magic effects (the first that matches; "NONE" is always 0; if there is none, 42) → 0x52B630: out of
  range it does nothing; otherwise, table 0xCCFBAC[i] = p and rebuilds the cumulative sums at 0xCCFB04. It is not reset between
  lands.
- **Globals**: `VERSION` → 0xD9957C; `SET_LAND_NUMBER` → g_game+0x205A08 (0 in the GGame ctor; openblack does not reset it when loading a map; it is read by
  `DesignedWaterFall` 0x5E3770 for the Land 3/4 scenery, see rendering.md);
  `SET_TOWN_INFLUENCE_MULTIPLIER` / `SET_PLAYER_INFLUENCE_MULTIPLIER` → g_game+0x250078 / +0x25007C, which
  `GGame::Init` 0x54F66F sets to 1 before the script. openblack: `Game::GetMapScriptGlobals`.

## Fish farms (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM)

- Case 31 (0x7166E1): 0x52C7B0(pos, GFishFarmInfo[N1] (0xCCFC78 + 0x128·i; info.dat only has entry 0), no town).
  Case 32 (0x716722): without the town it creates nothing; otherwise, the same with it. The ctor 0x52C360 stores at +0x8C **always
  the nearest town** (`Town::GetNearestTownToPos` 0x73B170, any tribe), whatever the script's one is.
- Fish shoal (`CallVirtualFunctionsForCreation` 0x52CC10, reviewed): with [0xC37BF4] = 0 (without flattening the sea,
  `GetAltitude` 0x803090 uses the raw height), rings of radius 2, 4... < 50 and 32 directions; the first direction with
  height exactly 0 at two consecutive radii gives the centre. openblack already did it the same way (`GetUnflattenedHeightAt`), so
  the 9 of Land1 without a shoal (those of the lake of town 2 and others) come out the same as in the original with the same
  data; the search was not changed.

## Build percentage of a Feature (`BUILT_PERCENTAGE`, CHL property 22)

Status: **faithful** and ported.

- **Script** (Land 1): `TheMissionaries` creates `GArk = CREATE(3, 69 = ArkDryDock, (1881,083; 8,1316; 3154,109))` and sets
  `BUILT_PERCENTAGE of GArk = 0,2` (the pattern `GET_PROPERTY; POPI 0; PUSHF v; SET_PROPERTY` is an assignment);
  `TheMissionariesBuildingBoat` adds 0.03 per hit (with `PLAY_SOUND_EFFECT(RANDOM_ULONG(92, 97))`) up to
  `ArkIncrement`. openblack's numbering is the right one: 22 = `BuiltPercentage`.
- `GET_PROPERTY` 22 (0x70E1A9): `dynamic_cast<MultiMapFixed>` → `GetPercentBuilt` (vt+0x880 = 0x4014F0, +0x5C); if it is not
  a MultiMapFixed, **1**. `SET_PROPERTY` 22 (0x70EC69): MultiMapFixed → `fn_0052EDD0`: +0x5C = value (0 if
  negative, **no cap**) and, if ≥ 1, `MultiMapFixed::Built` 0x52EBB0 (+0x5C = 1, +0x58 loses 0x02 and gains 0x08, releases
  the building site +0x74, reaction 0xF if it has a town, `RequestChangeTexture`); then the town's list of buildings
  (0x70EC9B..0x70ECD4), which a Feature does not have.
- **Initial value**: `fn_00527350` → `MultiMapFixed` ctor 0x52E1E0(pos, info, angle, scale, percentage,
  planned): planned → bit 0x02 and +0x5C = 0; otherwise, +0x5C = percentage and bit 0x08. The script's `CREATE` passes 1:
  built.
- **Drawing**: `Feature::Draw` 0x518690 = `MultiMapFixed::Draw` 0x518090: if `IsDrawBuilding` (vt+0x8A4), `DrawBuilding`
  0x517F90. `Feature::IsDrawBuilding` 0x527790: **only for GFeatureInfo 69** (ArkDryDock) is it `!IsBuilt()` (0x422110:
  bit 0x02 clear and +0x5C ≥ 1); the other Features use `MultiMapFixed::IsDrawBuilding` 0x52F0C0 = "has a building site"
  (+0x74), which a Feature never has: they are always drawn whole. `DrawBuilding`: p = `GetPercentForDrawBuilding`
  0x52EFD0 = min(GetPercentBuilt, GetPercentRepairedFromWhenDamaged 0x52F010 = 1 if it is not built); with p = 0
  nothing is drawn; otherwise, vt+0x110 of the static object = `fn_00816AD0` (the half-built drawing of the houses: main
  mesh cut at pos.y + 2·ext.y·scale·p with inner walls and cap, and the scaffolding that rises (p < 0.2), whole or
  cut from above (p > 0.8)).
- openblack: `components::Feature::percentBuilt`, `src/ECS/FeatureBuild.{h,cpp}` (`physics::PartialBuild` applied to the
  Feature's local mesh; no mesh with p = 0; the terrain footprint is kept), `GET/SET_PROPERTY` 22 in `CHLApi`.
  The Land 1 script does not reach `TheMissionaries` yet (it comes after choosing a creature): it is tested with
  `OPENBLACK_TEST_BUILT_PERCENTAGE`. At 0.2 the whole scaffolding is visible and the ark cut at one fifth; at 1, the ark on
  its props.

## Map script objects (street lanterns, bonfires, dead trees, gates)

Disassembly in `documentacion\mapa\all_cases.txt`, `d_streetlantern.txt`, `d_deadtree_isok.txt` and `d_animstatic_cvffc.txt`.
- **Parameters**: in the script's argument block, the integer of parameter i is at +0x6000 + 4i and the float at
  +0x6030 + 4i. `GMobileStaticInfo` takes 300 bytes in memory (0xD3A6D8 + 300·i: MS[6] = 0xD3ADE0, MS[7] = 0xD3AF0C,
  MS[8] = 0xD3B038) and 284 in `info.dat`: in memory the info.dat record starts at +0x10 (the object type is at
  info +0x10 and the clip of an AnimatedStatic at +0x128, which is +0x118 in openblack's `GAnimatedStaticInfo`). The 61
  MobileStatic infos have object type 0x1C (MOBILE_STATIC).
- **CREATE_STREET_LANTERN** (case 80, 0x717720) → `GStreetLantern::Create` 0x7346E0(pos, &MS[N1]): creates nothing if in the
  map cell of the position (`MapCoords::FindType` 0x6045C0 → `MapCell::FindTypeOnMap` 0x6015E0; cells of 10
  units) there is an object of type 0x1C less than 0.5 m away in x/z (`GUtils::GetDistanceInMetres` 0x74CD70); any
  thing made with a MobileStatic info: rocks, bonfires, street lanterns, dead trees. +0x58 = (info ≠ MS[7]).
  `CallVirtualFunctionsForCreation` 0x734810: mesh 148 (MSH_B_CAMPFIRE) if +0x58, otherwise 398 (MSH_O_TOWNLIGHT);
  `SetPosition((x, GetAltitude + y, z), ángulo 0, escala 1)` (no 180° turn), the light `fn_00823240`(that point,
  +0x58) at +0x5C and, **in both classes** (it does not check +0x58, corrected: this note used to say "only in the village one"), the
  sound 0x93 at +0x60 (`fn_0071E8C0` = `SoundTag::Create`), unless the object carries the UNAVAILABLE mark (+0xA & 1);
  details of the sound in [day-night-weather.md](day-night-weather.md). Land1: 8 of type 7 and 4 of type 59
  (field lanterns with the bonfire mesh, **not** bonfires). The CHL CREATE with 7 or 59 goes through the same place.
  openblack: `StreetLanternArchetype`, `components::StreetLantern` / `LanternLight`; `night_lights` sets the lights
  according to `LanternLight` (previously by the mesh, and the real bonfires came out with lantern light).
- **CREATE_BONFIRE** "AFFF" (case 73, 0x7176AE) → `fn_00439850`(pos, F1 temperature, F2 Y angle, F3 scale) → ctor
  0x4395C0: `Rock`(pos, MS[8], angle, scale) and `CreateSpotVisualWithSpecifiedDuration`(pos, 25 SF_Bonfire, 1,0, −1 =
  forever, the bonfire); the temperature is not used on creation (Land1 has 24,7, which openblack took as the angle). No
  lantern light. openblack: `BonfireArchetype`.
- **MobileStatic**: `CREATE_MOBILESTATIC` "ANFF" (case 41) → `fn_00608770`(pos, info, 0, 0, F2 angle, F3 scale):
  MS[8] → `Bonfire::Create` with temperature 100; info +0x128 = 2 → `Rock`; MS[6] → nothing; the rest `MobileStatic`.
  `CREATE_MOBILE_STATIC` "ANFFFFF" (case 42) → `fn_00608840`(pos with relY = F2, info, 0, 0, F3, F4, F5, F6): MS[6] →
  GBaseOnly `fn_00609340`; MS[7] → nothing; the rest `fn_00608770`(…, F4, F6); then `SetXYZAnglesAndScale`(F3, F4, F5,
  F6) on what was created (also the base and the bonfire). openblack: `MobileStaticArchetype::CreateFromInfo` /
  `CreateWithXYZAngles`.
- **CREATE_DEAD_TREE** "ALNFFFF" (case 43, 0x716E64) → `fn_00510BB0`(pos, GTreeInfo[N2], player, F3, F4, F5, F6, 0):
  ctor 0x510A30 = `Rock`(MS[3], angle 0, scale 1) + `SetLife`(F3); with 0xCC5F10 = 0, `GetDeadTreeMesh` 0x510C60 is the
  normal mesh of the type; then `SetXYZAnglesAndScale`(F4, F5, F6, 1), the MobileStatic matrix (x = F4, y = F5,
  z = F6). Land1: 3 (types 12, 4 and 4, life 1, small angles). openblack made a live burnt tree; now
  `DeadTreeArchetype` (`components::DeadTree`, no Tree or forest, can be picked up).
- **CREATE_POT** (case 38): `IsOkToCreateAtPos` and, if the quantity N3 ≤ 0 (0x716B19), nothing. Removes the 4 empty wood
  piles of Land1.
- **CREATE_NEW_FEATURE** (case 75): with N5 ≠ 0 it creates a `PlannedFeature` 0x527440 (not drawn); no land uses it.
- **Names**: features `fn_00527740` and animated statics `fn_00422600` compare with `_stricmp` (if there is none, they return the
  number of infos, 0x4C / 0x10); `GAbodeInfo::GetInfoFromText` 0x405A70 walks the 9 tribes, compares the prefix with
  `_strnicmp`, requires '_' and the description with `_stricmp` (16 per tribe); otherwise, −1. The original uses the result without
  checking it; openblack logs the failure and skips the command (deliberate robustness deviation; it used to throw).
- **AnimatedStatic** (`CallVirtualFunctionsForCreation` 0x422300): sets the clip from info +0x128 (Norse Gate 191, Gate
  Stone Plinth 195, Piper Cave Entrance 189); `Draw` 0x422770 advances or rewinds it depending on whether it is open, limited to its
  duration: closed is t = 0 (openblack: `SkeletalAnimation` stopped at 0). With mesh 212 (Norse Gate, `fn_004230D0`)
  it creates 2 `Game3DObject` with mesh 398 at (∓15, 30, 0) of the gate's matrix (rows with scale + translation),
  angle 0 and scale 1, each with the light `fn_00823240`(its position, 0).
- **CREATE_PLANNED_CITADEL** (case 20): town and player mandatory; `fn_00467DD0` (PlannedTownCitadelHeart in the
  town) and stores the position at 0xC5E258. The real temple comes from `PlannedTownCitadelHeart::CreatePlannedNoFixedCheck`
  0x467EF0 (vtable +0x504: the player's `Citadel` if it has none, `fn_00462B10`, and `CitadelHeart::Create` 0x464E20), which
  is called by `Town::AddBuildingSiteNoFixedCheck` 0x73B8A0 from `Town::RequestBestPlanned`, `Town::ForceBuildingOfPlannedAtPos`
  0x73E560 (`GScript::BuildBuilding` 0x6FAB30 of CHL, and 0x641774 after `StartPlaygroundGame` with 0xC5E258) and
  `Scaffold::TryToBuildPlannedBuilding`. `GGame::Birthday` → `GPlayer::Birthday` → `Town::Birthday` only rebuilds
  statistics. When the map loads **there is no temple**, only the plan (GameThingWithPos 0x4C, no mesh, no cell, no
  creation index, no flattening; `Draw` 0x648930 = `ret`). Info "Citadel Heart" (info.dat 0x115C0): wood 5,
  timeToBuild 150, desireToBeBuilt 1,0, mesh 564 BuildingDummyCitadel; abode type of the plan 0x804 (civic).
  - Conversion 0x467EF0 (arg `float life`): the player is the **owner of the town** (`Town+0x2C`, the one from CREATE_TOWN or the
    neutral one), not the script's (that one is only validated). `CitadelHeart::Create`(pos, info, citadel, plan angle, plan
    scale, life, 1): the 1 marks "under construction" (MultiMapFixed 0x52E1E0, +0x58 bit 1, +0x5C = 0).
    `CallVirtualFunctionsForCreation` 0x4675A0 creates the LH3D type 8 at **scale 1** with y = altitude(origin) + alt and calls
    0x882730 (mesh B_FIRST_TEMPLE, % built, **flattens the land**): the flattening happens on conversion. Worship sites
    (`fn_00464F50`) only if life ≥ 1. Then heart+0x94 = town, `PostCreatePlanned` 0x648C50 and the plan is deleted.
  - `AddBuildingSiteNoFixedCheck` always passes life 0.0 and creates a `CitadelBuildingSite` (0x468DC0 → 0x43D1E0); it is
    finished by the villagers (`CitadelHeart::Built` 0x465000). With life < 1 `Draw` 0x882A40 uses `DrawPartialyBuilt`
    0x816AD0 (not decoded).
  - Triggers: **Land 1** = CHL `FollowUs`: `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)` (the plan pos from
    Land1.txt:95; `GetPlannedAtPos` 0x73E4C0 takes the nearest plan less than the radius of mesh 564 × scale + 1 m away),
    then `CALL_NEAR(Citadel 18)` + `SET_PROPERTY(22, 0.375)`; `PreventCitadelCompletion` limits it to 0.9 and
    `CheckCitadel` waits for 1. **Lands 2-5** = AI: `Villager::CheckSatisfyCivicBuildings` 0x758E90 (town desire
    FOR_CIVIC_BUILDING 6, 0x748330) → `RequestBestPlanned` 0x73A650 → `GetBestPlanned` 0x73A140 (mask 4).
  - **CREATE_CITADEL** (`Citadel::CreateCitadel` 0x463240) passes (angle, 1,0, 1,0, 0) to `CitadelHeart::Create`: the
    script's scale is **ignored** (Kapa's Land1 Playground passes 0, other maps 300 or 4121) and it comes out built.
  - openblack: CREATE_CITADEL draws at scale 1 (it used to use the script's: in Kapa's Land1 Playground the temple was
    invisible). CREATE_PLANNED_CITADEL requires a valid town and player (otherwise, nothing), the temple belongs to the town's owner
    (`Town::owner`) and is drawn at scale 1. **Pending deviation**: since there are no town desires, building
    sites, BUILD_BUILDING/SET_PROPERTY 22/CALL_NEAR or partial drawing, the temple is still created already built
    (and flattening) on load, so that it does not disappear from Land 1-5. Making it faithful requires porting all of the above.
- **IsOkToCreateAtPos** 0x638C40: fails if `MapCoords::CollideCollideWithFixe` 0x604FE0 → `MapCell::CollideWithFixe`
  0x601D10 gives bit 8 and the cell is not water. Bit 8 comes from a `NewCollide::Obj` circle of radius 0.5 (0x82AD90)
  against the `GetCollideData` (vtable +0x858) of each fixed object in the cell's list +4 (`Obj::Collide` 0x829140);
  the other bits come from `MapCell::Collide` 0x601BD0 (bit 0x10 of the land block, outside the map). Full
  report: `documentacion\mapa\flecos_isok.md` (simulation `isok\sim.py`).
  - **Who calls it**: only CREATE_TREE (27, 0x716235), CREATE_NEW_TREE (28, 0x7162EE), CREATE_POT (38, 0x716B0C, before
    looking at the quantity) and CREATE_MOBILEOBJECT (40, 0x716C71). If it fails, it creates nothing, writes nothing and the script continues.
    Angle and scale are not used. The CHL handlers do not call it. CREATE_TOWN_CENTRE uses another one (`GAbodeInfo::IsOkToCreateAtPos`
    0x404B10, not ported: in Land1-5 it rejects none). Abodes, fields, features, mobile statics, bonfires and dead
    trees are created without checking anything.
  - **The test**: circle of 0.5 at the script's (x, z) (the height does not count, `MapCoords(char*)` leaves y = 0) against the
    objects of **its cell**; 2D test `dx² + dz² <= (ra + rb)²` and then the children. With water in the cell (bit 0x10,
    `hasWater`) it is always created; outside the map (or in an empty block) too. In openblack that test is
    `ecs::sea_cells::IsWater` (`MapCoords::IsWater` 0x6035B0, `ECS/SeaCells.h`, the cell of the 16.16 MapCoords), the
    same as for physics, the hand and the AI (2026-10-01; previously it read the cell by hand). Land1 still has 1351 trees and 51
    mobile objects (`OPENBLACK_DUMP_ENTITY_COUNTS=600`, `_scratch\mapa\int_counts.log`).
  - **Shapes**: tree = circle of 0.3 at its position, only in its cell (0x74C5F0). MultiMapFixed (abode, centre,
    field 594, feature, animated static, mobile static, rock, bonfire, dead tree, dispenser) = `NewCollide(LH3DObject)`
    0x829390 from the mesh's bbox: bbox centre rotated with `x' = x·cos a − z·sin a`, `z' = x·sin a + z·cos a`;
    half-axes `max(1, escala·mitad)` in x and z; if long/short > 1,4, outer circle `sqrt(ex²+ez²)` with
    `int(largo/corto)+1` children of the short radius in a row along the long axis (0x82ADD0 / 0x828F40); otherwise, one circle of
    `max(ex, ez)`. It is put into every cell whose circle (cell centre, 7.1) it touches. The bbox (0x8081B0) passes the
    boned meshes (flag 0x100) through `LH3DAnim::SetTransform`, like openblack's. Without collide data: BigForest,
    pots, mobile objects, villagers, animals, street lanterns and planned buildings.
  - **openblack** (`ECS/MapCollide.h/.cpp`, `openblack::ecs::map_collide`): grid of cells that is emptied on
    LOAD_LANDSCAPE and filled with the script's parameters (mesh from the created object's `Mesh`, Y angle and scale from the
    script, not the Transform). Not registered yet: fish farms, CitadelHeart (0x468FB0) and WorshipSite (0x77E490), not
    decoded. `OPENBLACK_LOG_ISOK=1` writes one line per rejection (command, position, what blocks it).
  - **Result** (checked with `OPENBLACK_DUMP_ENTITY_COUNTS`): Land1 1395 → 1351 trees (44 rejections: the 43 of
    `sim.py` − 2 under the Piper Cave Entrance + 3 under the dead trees), mobile objects 51; Land2 921 → 915 (+1
    pot); Land3 1397 → 1373 and 26 → 20 mobile objects; Land4 799 → 764 and 1 → 0 (+1 pot); Land5 804 → 782 and
    19 → 13; LandT 341 → 316. Differences from `sim.py`: the Piper Cave Entrance is a boned mesh and `sim.py` used
    the untransformed vertices; `sim.py` did not model the dead trees (they are Rocks with the type's normal mesh,
    created with angle 0 and scale 1, and the script's angles are almost 0). Screenshot: Land1 next to the Boulder1 Lime
    (2120, 2494), now without the trees on top.

## Towns and citadel

Summary of what the wiki already says about towns, temple and citadel, with links (no text has been moved):

- **Town data on load**: `SET_TOWN_UNINHABITABLE`, `CREATE_TOWN_CENTRE` (centre and worship percentage) and
  `CREATE_PLANNED_ABODE`, in [Map simulation data](#map-simulation-data-data-only-nothing-is-drawn).
  The totem of the town centre (`components::TotemStatue`) and the sinking of the abodes down to their foundation, in
  [openblack-internals.md](openblack-internals.md#render).
- **`Town::owner`** (`Town+0x2C`, the player from `CREATE_TOWN` or the neutral one): it is the owner of the temple that comes from
  `CREATE_PLANNED_CITADEL`, not the script's player
  ([Map script objects](#map-script-objects-street-lanterns-bonfires-dead-trees-gates)).
- **Temple**: on load there is only the plan (`PlannedTownCitadelHeart`); `CitadelHeart::Create` converts it and flattens the
  land (0x882730), and `CREATE_CITADEL` ignores the script's scale (same section). openblack creates it already built in
  `CitadelArchetype` (pending *deviation*); the flattening (flat up to 35 units, blend up to 70) is in
  [openblack-internals.md](openblack-internals.md#render).
- **Citadel and worship**: the six worship site slots in the temple entity (`components::Temple`) in
  [magic.md](magic.md#the-citadel-and-its-six-slots-worshipcitadelcpp); its influence and the difference inherited from the
  already-built temple in [magic.md](magic.md#influence-m1i-srcecsinfluence).
- **Collision (`MapCollide`)**: `IsOkToCreateAtPos` and the `ecs::map_collide` grid, in
  [Map script objects](#map-script-objects-street-lanterns-bonfires-dead-trees-gates); CitadelHeart and
  WorshipSite are not yet registered in it.
- **Inside the citadel** (`g_game+0x205A28 == 1`): what plays in [audio.md](audio.md#original-gaudio-lhaudio-and-qmixer)
  and [objects-and-resources.md](objects-and-resources.md#sounds-report-documentacionsoundnotestxt); the frame in
  [original-frame.md](original-frame.md#other-cases-temple-video-and-2d); graphical parity in [parity.md](parity.md).
- Town trees (scenic forest, list of forests Town +0x608):
  [trees.md](trees.md#searches-for-trees-and-forests-for-the-villagers-report-documentaciontrees2villager_queriesmd).

## Script commands that move things (MOVE_GAME_THING 033 and friends)

The **Land 1** intro (CHL `FollowUs`, `Scripts\Quests\challenge.chl`; code in `documentacion\mapa\rt_chl_code.txt`
from line 49528) creates the family (mother = VILLAGER 49, father = 53, son = 52, CREATE 027), moves it with
`MOVE_GAME_THING(cosa, punto, 0.0)` and waits with `GET_DISTANCE(GET_POSITION(cosa), punto) == 0` (`FollowUs_loop_4` and
`_loop_6`) or `< 1` (`_loop_5`, `_loop_77..79`). Then it makes them act with `SET_SCRIPT_ULONG(cosa, clip, veces)` +
`SET_SCRIPT_STATE(cosa, 200)` and waits for `PLAYED(cosa)` (`_loop_8`, 11, 17, 45, 48, 49, 74, 80..82). All of this was a stub.

### MOVE_GAME_THING (GScript::MoveGameThing 0x6F8E80)

It pops radius, z, y, x and the thing (0x6F8E91..0x6F8EE9); `GetScriptGameThing` 0x70D220 (otherwise: "Thing no longer valid"
0xC0C258). `MapCoords(pos)` 0x603160 (x / z; y stays relative to the land). By type, in this order:

| test (vtable) | what it does | openblack |
|---|---|---|
| IsCreature (+0x34) | `dynamic_cast<Creature*>` ("no creature for script" 0xC0D598), IsObjectInMap (+0x178) → fn_004F6B60(pos, radius): `PrepareCreatureForScriptedAction` 0x4F6A90 and subactions (`AddSubAction` 0x4FF240). Only here is the radius used | **pending** (there is no creature AI) |
| IsLiving (+0x3C4) | IsObjectInMap and !IsDrowning (+0x17C), otherwise nothing; `AreWeThere(coords, 0.0)` (+0x85C, 0x60AD60) == 0 → `Living::SetupMoveToPos(coords, 4 IN_SCRIPT)` 0x5F2830 (0x6F8FCA); if already there → `GScript::SetScriptState(cosa, 4)` 0x6F82E0 (0x6F8FD7) | villagers exact; animals **(approximate)**, below |
| IsFlock (+0x3EC) | `Flock::SetDomainCentrePos` 0x52FC20: destination (+0x80) of the first member and centre (+0x14) of the flock | `AnimalBrain::goal` of the first one and `Flock::domainCentre` |
| IsWeather (+0x3FC) | fn_00774550: +0x78 (its system) +0x5C = pos | there are no weather things |
| IsComputerPlayer (+0x4B8) | fn_00658510(pos, 60.0) | there are no CPU players |
| rest | "Jonty - Thing must be living to move it!" (0xC0D56C) and `SetPos(coords)` (+0xFC, 0x401940) | only the `Transform` is moved **(approximate)** |

- **Living::SetupMoveToPos** 0x5F2830 (pos, final): movement state = byte of GLivingInfo +0x124 (1 MOVE_TO_POS
  for all villagers), or 3 MOVE_ON_STRUCTURE if GameThingWithPos +0x24 & 0x80 (only set by `Living::MoveOnStructure`;
  openblack does not have it: never); `SetCurrentAndDestinationState(movimiento, final)` (+0x8DC) and, only if it gives 1,
  `MobileWallHug::SetupMobileMoveToPos(pos)` 0x60AAD0: destination +0x80 = pos, `InitStepsXZ` 0x60BFA0, out of the
  detour lists (fn_00611AC0 / 00611610 / 00612BB0 / 00610590, +0x76 = 0), and `AreWeThere(0)` → +0x5E = 1 ARRIVED; otherwise,
  `CircleHugInfo::Reset`, +0x78 = 1, +0x5E = 0xB **STEP_THROUGH** (straight, without going around: it is not the LINEAR of
  `SetupMoveToWithHug` 0x5F2890). openblack: `ecs::villager::SetupMoveToPos` (`src/ECS/Villager/VillagerScript.*`), with
  the PathfindingSystem's `MoveStateStepThroughTag` or `MoveStateArrivedTag` mark.
- **MobileWallHug::AreWeThere(pos, r)** 0x60AD60: `dx² + dz² < (velocidad u16 +0x5A + r)²` (strict, in MapCoords);
  `AreWeThere(r)` 0x60AD40 = `AreWeThere(GetDestPos() (+0x860), r)`. openblack in metres with `WallHug::speed`
  **(approximate: floats instead of 16.16 integers)**.
- Animals **(approximate)**: `animal_ai::MoveTo(…, IN_SCRIPT)` (Living::SetupMoveToPos), the destination's altitude above
  the land (+0x88) is taken as 0 and, if already there, `animal_ai::SetState(IN_SCRIPT)` instead of SetScriptState.

### What the script queries afterwards

- **GET_POSITION** (GScript::GetPosition 0x6F88A0): flock → Pos of the first member or `GetFlockPos` 0x530570 (the
  centre +0x14); **a MobileWallHug that is not a creature (+0x408, +0x34) returns its destination (+0x80) if
  `AreWeThere(0)`** (0x6F8977..0x6F89AF), otherwise its Pos. Height = `GetAltitude` + the +8 of those MapCoords; openblack gives the
  land height at the destination **(approximate: WallHug only stores x / z; GET_DISTANCE does not look at y)**.
- **GET_DISTANCE** (GScript::GetDistance 0x6F8CA0): `GUtils::GetDistance(LHPoint, LHPoint)` 0x74CDE0 =
  `hypotenuse(dx, dz)` 0x74F6C0: **only x and z**; 0 if |dx| and |dz| ≤ 0.0001 (0x8BF518), otherwise `1 / InvSqrt(dx²+dz²)` with the
  root approximated by the table `_FUN_0074f620` (table 0xDA5A10, the same as `AnimalLairs.cpp`); **below 0.5
  (0x8AA3B4) it gives 0**. Previously openblack measured in 3D and without the cut-off, so `== 0` was never met.

### SET_SCRIPT_STATE 017, SET_SCRIPT_ULONG 020, PLAYED 064 and the script states

- **SET_SCRIPT_STATE** 0x6F8370: state (first pop) and thing ("Object no longer valid" 0xC0D428). Script container
  (+0x3F8: g_game +0x250090 +0x24 and the loop function from table 0xC0C73C) **pending**; `dynamic_cast<Living*>` and
  !IsDrowning → `GScript::SetScriptState(living, estado)` 0x6F82E0; otherwise, "Object not living for set state" 0xC0D440.
- **GScript::SetScriptState** 0x6F82E0, not a creature: IsAvailable (+0x2C; Villager 0x751D50: not being deleted and final ≠ 14
  DYING) and IsObjectInMap (+0x178: +0x24 & 1; openblack: not in the hand, **(approximate)**) → `StorePreviousState`
  (+0x8EC, 0x763470), `CallExitStateFunction(estado)` (+0x904) and `CallEntryStateFunction(estado)` (+0x90C) without looking at
  the result, `Living::SetAnim(1)` (+0x8FC, 0x5ECB80 → SetAnim(GetAnimId(), 1) 0x5ECBA0: the state's clip from 0) and
  +0x58 = 0. The creature branch (fn_0047B140 / 004F6E30 / 004F6F10) **pending**.
- **SET_SCRIPT_ULONG** 0x6F8770: times (first pop), clip, thing. Villager: +0x120 = times, +0x11C = clip
  (`Villager::scriptAnimLoops` / `scriptAnim`); creature +0x1290 / +0x128C **pending**; otherwise, "setting the state of
  something neither a creature nor a villager" (0xC0D484).
- **PLAYED** 0x6F9DC0 on a villager: `Villager::IsScriptAnimationComplete` 0x7689D0: TOP 23 WAIT_FOR_ANIMATION → 0; TOP
  200 → times == 0; otherwise 1. Another Living: GetFinalState == 4 (0x6F9EC4) **pending**.
- Row **4 IN_SCRIPT**: state `StateInScript` 0x5ED9A0 (creates DataForScriptRemind if there is none; 1), entry
  `EnterInScript` 0x5ED7E0 (vt +0x940: 1 if `IsStateEntryFunctionSameAs(final, next)` 0x7524D0 or there is no script
  memory), exit `ExitInScript` 0x5ED9C0 (vt +0x914: `CircleHugInfo::Reset`; `IsScriptState(next)` (+0x960, file
  0x18) → 1; otherwise it stores the memory and `ExitNoChangeState(next)` 0x768780 = 1 if next is interruptible by script
  (file 0x1C), IN_HAND (`IsStateForInterface` 0x417070) or `IsStateExitFunctionSameAs`).
- Row **200 SCRIPT_PLAY_ANIM**: `ScriptPlayAnim` 0x768970: with times > 0, one less and `PlayAnimThenSetState(veces ?
  200 : 4)`; entry `EnterPlayAnim` 0x768840 (like EnterInScript), exit `ExitPlayAnim` 0x7689C0 = ExitInScript; clip
  `ScriptAnimation` 0x768A00 = +0x11C (`AnimFn::Script` of `VillagerAnimations.cpp`).
- **Living::PlayAnimThenSetState** 0x5ECAC0: `CallExitStateFunction(s)` and, if 1, `CallEntryStateFunction(23, s)`: TOP 23
  and FINAL s, the clip does not change. Row **23 WAIT_FOR_ANIMATION**: `WaitForAnimation` 0x5EC990: `IsReadyForNewAnimation(1)`
  → `SetTopStateToFinal` and 0, otherwise 1.
- **Not ported**: `DataForScriptRemind` (Living +0xB0, `Create` 0x5EF190, `KeepThatInMind` 0x5EF1D0, fn_005EF2A0), with
  which a villager taken out of a script state remembers its walk and resumes it on returning. Without it, the branches of
  EnterInScript / EnterPlayAnim that resume it are never taken **(inferred)**.
- **(approximate)** In the original the walk only advances from the MOVE_TO_POS function (`Living::MoveToPos` 0x5EC270 →
  `MobileWallHug::MoveTo` 0x60AF20); openblack's PathfindingSystem moves every entity with a mark, whatever its
  state. That is why `SetScriptState` removes the marks if the villager is no longer in MOVE_TO_POS: without that the father kept
  walking (and overshot the destination) while acting in 200.

### Virtual machine CAST

In the original `ScriptLibraryR.dll` (`Plug Ins`), opcode 23 INTCAST (0x10008EE0, table 0x100090E4 by type − 1)
**converts**: CASTI reads the bits as a float and does `__ftol` (0x1001568C, truncates; the low word of the 64-bit integer),
CASTF reads the bits as an unsigned 32-bit integer (fild qword with the high part 0); CASTV, CASTO and CASTB only change
the type and type 5 does nothing. openblack only changed the type, so `PUSHF 1.0 CASTI` reached SET_SCRIPT_ULONG
as 0x3F800000 times. Fixed in `components/ScriptLibrary/src/LHVM.cpp` (`Opcode23Cast`).

### In game (Land 1)

With `OPENBLACK_TEST_TEXT_CLICK=1` (the texts with interaction 1 wait for a click, as in the original) `FollowUs` passes
`_loop_4` at ~15 s (the family reaches the kiss markers), walks to the beach, the son runs into the sea, the camera jumps
with the script's SET_CAMERA_POSITION calls, the texts appear (`¡Has salvado a nuestro hijo!` … `Te enseñaré cómo seguirlos.`)
and the family passes `_loop_77..79` (reaches `StartPath`).

**State on 2026-10-03 (session Intro, run `_audit\Intro\base_100k`, `--mod game.skip-intro=off`,
`OPENBLACK_TEST_TEXT_CLICK=1`):** the script camera (035 `HAS_CAMERA_ARRIVED`, 003 / 004, see
[script-camera.md](script-camera.md)) and `SET_AVI_SEQUENCE` (203, `INTRO.bik`, see [video.md](video.md)) are ported, so
`FollowUs` now runs through the film (~frame 20000), the rescue, the advisors' texts 4431..4437 and 5189..5192 and
`RUN Drag` (52154). In `Drag` (12627..12722), after `PLAY_HAND_DEMO(279, 1, 0)` (12663, 266) the script waits
`HAND_DEMO_TRIGGER` (336) three times (12672 / 12682 / 12692) and then `IS_PLAYING_HAND_DEMO` (267, 12699; it pushes
`!IsPlayBack(0)` 0x6FDB9A, so it waits for the demo to end).

**State on 2026-10-04 (session Intro, run `_audit\Intro\drag_140k`):** with the hand demo (`Input/HandDemo.h`, session
Mano) and the opcodes 266 / 267 / 336 wired, `drag.hnd` plays its 590 records, text 5219 follows and the hand-over at
52172..52175 runs (`SET_WIDESCREEN 0`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`): the bars go, the camera
and the dialogue are released, about 4 min of game after the start. The dialogue texts are drawn (HelpText display,
`Help/HelpTextDisplay.h`, Renderer::DrawHelpText; not verified yet against the original's look). Still missing: the
advisors (pending: HelpDude), the welcome crowd and dance, the villagers' focus and override animations (session
Personas), the intro's JC specials and the high-detail family models.

### Skipping the tutorial (SkipBox and CAN_SKIP_TUTORIAL)

**Faithful** (runblack.exe v1.42, read with `bwdis.py`):

- `GGame::OnNewGame` 0x553900: if there is a script and it is Land 1 it starts `LandControlAll` and, always, calls
  `DoYesNoSkipTutorialRequestersIfNecessary` 0x54CBD0 (0x55395B). This clears bits 23, 24 and 25 of `g_game+0x14`
  (0x54CBD5 / 0x54CBE1 / 0x54CBED); the condition that used to come before showing the box is nulled out with 25 NOPs
  (0x54CBF4..0x54CC0C), so it **always** does `PauseGame(1)` and `SkipBox::Show` (vt +0x0C, `DialogBoxBase::Show`
  0x5135F0) on the SkipBox from `FrontEnd::Init` (0x53B844, pointer at 0xCD0634, callback 0x544480 at 0x53BB9D).
- `SkipBox::Init` 0x5441C0: a title, a button (id 0xB, text 0xA24 from `HelpTextDatabase`) and four checkboxes (ids
  0x3C..0x3F) with texts 5..8 of the text table at 0xD17CA0 (not read); the selected one is at `+0x20`, 0 by
  default (0x544206). `SkipBox::CanESCOut` 0x53BD60 returns 0: it does not close with ESC.
- Callback 0x544480: when button 0xB is pressed, depending on the checkbox (jump table 0x5445A0): 0 clears 23, 24 and 25;
  1 sets 23 and clears 24 and 25; 2 sets 23 and 24 and clears 25; 3 sets all three. Then `PauseGame(0)` and
  `DialogBoxBase::Hide`.
- The scripts read it with `CAN_SKIP_TUTORIAL` (460, `GScript::CanSkipTutorial` 0x6FFEF0, bit 23),
  `CAN_SKIP_CREATURE_TRAINING` (461, 0x6FFF10, bit 24) and `IS_KEEPING_OLD_CREATURE` (462, 0x6FFF30, bit 25), which
  push the bit as a boolean (VMType 6). Nobody else writes those bits (search for `or`/`and` with 0x800000,
  0x1000000, 0x2000000 in the whole `.text`).
- challenge.chl: only `SetupLand1` calls them. `CAN_SKIP_TUTORIAL` → global `IsSkippingToCreatureSelect` = 1;
  `CAN_SKIP_CREATURE_TRAINING` → `IsSkippingCreatureGuide` = 1; `IS_KEEPING_OLD_CREATURE` and
  `CURRENT_PROFILE_HAS_CREATURE` → `IsKeepingOldCreature` = 1 and the other two as well.
- `LandControl1`: with `IsSkippingToCreatureSelect` neither `FollowUs` (the intro) nor `CitadelGuide` run: it does
  `BUILD_BUILDING((1915.05, 0, 2508.89), 1.0)`, takes with `CALL_NEAR(18, 5000, ese punto, 5)` the created object (the
  citadel, (inferred) from the type) and sets its `BUILT_PERCENTAGE` (property 22) = 1; `ChooseYourCreature` does not run either: it creates the three gate stones (`GateKey1`, `GateKey2`,
  `QuarryRock`) and runs `CreaturesInGlade` (which takes the camera to choose a creature and, in that case, skips the
  guardian and does a 2 s `SET_FADE_IN` after 2 s). With `IsKeepingOldCreature` it instead sets the time (`SET_GAME_TIME 15.4`,
  `GAME_TIME_ON_OFF 1`), opens the creature gate (`SET_OPEN_CLOSE`), sets `ChooseYourCreatureFinished` and
  loads the profile's creature (`LOAD_MY_CREATURE`). With `IsSkippingCreatureGuide` none of `MoveTheGuideAround`,
  `CreatureDevLearnToEat`, `CreatureDevPunishment`, `CreatureDevLeashIntro`, `CreatureDevLeashAttachToHouse`,
  `MeetTheGuide`, `GuideAsksToMeetYourCreature`, `GuideImpressTown*`, `CreatureDevGuideTeachesFight` or `TheStorm` run
  (it waits for `LeaveLandNow`).
- There is no other original way to skip the intro: `FollowUs` does not call `KEY_DOWN` or check ESC; the click only advances the
  texts.

**What runs with each answer** (read in `dev\documentacion\mapa\rt_chl_code.txt`, which is pseudo-assembly: the natives
appear as `CALL <n>`; extents: `SetupLand1` 25357-25622, `LandControl1` 74956-75239, `LandControlAll`
171106-171139, `CreaturesInGlade` 43862-46096, `CreatureDevSeeHome` 6932-7814, `FollowUs` 49528-53196):

- `SetupLand1` 25398-25455: the three globals are independent, but `IsKeepingOldCreature` =
  `IS_KEEPING_OLD_CREATURE and CURRENT_PROFILE_HAS_CREATURE` (25432-25435) and, when it is set, it **forces** the other two
  to 1 (25440-25447). Neither `SetupLand1` nor `LandControlAll` have music, camera, dialogue or fade before
  `LandControl1` (the `SET_FADE_IN(3.0)` at 171120 is already the transition to Land 2).
- `LandControl1`: with `IsSkippingToCreatureSelect` neither `FollowUs` (74982), `CitadelGuide` (74983) nor
  `ChooseYourCreature` (75037) run, and instead it does `BUILD_BUILDING` + `CALL_NEAR(18, 5000, …)` +
  `SET_PROPERTY(22, ciudadela, 1.0)` (74988-75017) and deletes `GateKey1`, `GateKey2` and `QuarryRock` (75042-75083).
  `CreaturesInGlade` (75088) is only skipped with `IsKeepingOldCreature`: then the block 75093-75125 runs
  (`SET_GAME_TIME(15.4)`, `GAME_TIME_ON_OFF(1)`, `SET_CAMERA_ZONE(3436)`, `SET_OPEN_CLOSE` of the creature
  gates, `ChooseYourCreatureFinished = 1`, `LOAD_MY_CREATURE(1850, 1300)`).
- **`CreatureDevSeeHome` (75141) always runs**, with all three answers. With `IsSkippingCreatureGuide` it takes the branch
  7056-7085: `loop { START_CAMERA_CONTROL }`, `loop { START_DIALOGUE }`, `START_GAME_SPEED`, `SET_WIDESCREEN(1)`,
  `SET_CAMERA_POSITION(1891.039, 31.693, 2520.674)`, `SET_CAMERA_FOCUS(1899.053, 30.312, 2518.680)`,
  `SET_WIDESCREEN(0)`, `END_GAME_SPEED`, `END_CAMERA_CONTROL`, `END_DIALOGUE`, `SET_FADE_IN(2.0)`; with no `SLEEP` in between,
  so the lock lasts one turn and the camera **snaps** (does not glide) onto the village. Before that, unconditionally,
  `SET_GAME_TIME(4.59)` + `GAME_TIME_ON_OFF(1)` (7000-7003): dawn breaks.
- `CreaturesInGlade` (answers 2 and 3, not 4) does hijack the beginning: `START_CAMERA_CONTROL` 44028,
  `START_DIALOGUE` 44031, `SET_WIDESCREEN(1)` 44035, `SET_FADE(0,0,0,2.0)` 44295 (skip branch), `SET_FADE_IN(2.0)`
  44326, `SET_CAMERA_POSITION/FOCUS` 44303/44310 (1753.3, 49.5, 2811.1), `MOVE_CAMERA_*` 44339-44403,
  `HAS_CAMERA_ARRIVED` 44404/44415/44422, `START_MUSIC(63)` 44419, `RUN_CAMERA_PATH(13)` 44421, and it does not let go until
  44684-44691. In openblack `MOVE_CAMERA_*` and `HAS_CAMERA_ARRIVED` are stubs, so the script **got stuck there
  forever with the camera, the dialogue and the widescreen held**.
- `START_MUSIC(54)`, the intro music, appears **only once in the whole challenge.chl**: line 50114, inside
  `FollowUs`. With any skip answer it never plays. Others in Land 1: 67 in `ChooseYourCreature` (42493) and in
  `CreatureDevSeeHome` without skipping (7117), 63 and 65 in `CreaturesInGlade` (44419, 45644/45813/45985), 67 and 69 in
  `TheStorm`. `SET_AVI_SEQUENCE` (51024), `CAMERA_PROPERTIES` (50444), `SET_FOCUS_AND_POSITION_FOLLOW` (50439) and
  `SET_INTERFACE_INTERACTION` of Land 1 are all inside `FollowUs`: with the skip they are never executed.
- The 13 background scripts (`SingingStoneCircle`, `ThrowingStones`, `TheLostFlock`, `TheMissionaries`, `MagicMushroom`,
  `HermitMain`, `CreatureSavingPeople`, `PiedPiper`, `CreatureGuardian`, `LeaveThroughVortexL1`…) wait on a
  `ChallengeHighlightNotify` / `QuestHighlightNotify` / `SingingStonesNotify` before touching the camera, so none of them
  gets in the way at the start. With `IsSkippingCreatureGuide`, `LeaveThroughVortexL1` opens the exit vortex from the beginning
  (53845-53876).

**openblack:** no SkipBox. `Game::Run`, after starting `LandControlAll`, clears the three bits (`TutorialSkipFlags` of
`Game`) and sets those of the answer of the [game.skip-intro](mod-library.md#gameskip-intro) mod (without the mod, none: the
default answer). The mod is **enabled by default** with the fourth answer, so neither `FollowUs`
nor `CreaturesInGlade` run; for that answer to be valid, the mod also answers `CURRENT_PROFILE_HAS_CREATURE` (CHL 463),
which openblack cannot know because it has no profiles. The only thing left of the beginning, the camera and dialogue turn of
`CreatureDevSeeHome`, is eaten by the mod's `free start` option (not from the original): see
[mod-library.md](mod-library.md#gameskip-intro).

## Pending

- SkipBox: openblack does not show it (replaced by the `game.skip-intro` mod); its texts are missing (table 0xD17CA0).
- CHL CREATE not ported: Reward, Creature, DeadTree, Store, Timer, Vortex, Ball, Totem, Highlight and Scaffold.
- `CREATE_MIST` 263 and `SET_MIST_FADE` 264 of CHL (`CHLApi.cpp`, not implemented).
- Initial rain and temperature of `GClimate` (season range): recorded here as not ported; check against
  [day-night-weather.md](day-night-weather.md#gclimate-the-climates-climatecpp).
- `GAbodeInfo::IsOkToCreateAtPos` 0x404B10 of `CREATE_TOWN_CENTRE`; register in `MapCollide` the fish farms,
  CitadelHeart (0x468FB0) and WorshipSite (0x77E490).
- Faithful citadel: town desires, building sites, `BUILD_BUILDING`, `CALL_NEAR`, partial drawing
  (`DrawPartialyBuilt` 0x816AD0); until then the temple comes out built on load.
- `SET_LAND_NUMBER` is not reset when loading a map in openblack.

## Test hooks

- `OPENBLACK_TEST_BUILT_PERCENTAGE`: `BUILT_PERCENTAGE` of the Land 1 ark.
- `OPENBLACK_LOG_ISOK=1`: one line per `IsOkToCreateAtPos` rejection.
- `OPENBLACK_DUMP_ENTITY_COUNTS`: entity count by type after loading (result of `IsOkToCreateAtPos`).
- `OPENBLACK_SCRIPT_THING_TRACE=1`: one line per MOVE_GAME_THING, SET_SCRIPT_STATE, SET_SCRIPT_ULONG and per true PLAYED
  (to follow `FollowUs`).
- `OPENBLACK_TEST_TEXT_CLICK=1`: every turn, if a text is waiting for the click (RUN_TEXT with interaction 1), it does the left
  click (`HelpSystem::ProcessInterface(true)`, which ignores it until 1 s of text).

## Sources

- `C:\Users\diewgarc\dev\documentacion\mapa\`: `chl_creatething_6F11A0.txt`, `all_cases.txt`, `d_streetlantern.txt`,
  `d_deadtree_isok.txt`, `d_animstatic_cvffc.txt`, `flecos_isok.md`, `isok\sim.py`.
- Script commands: `documentacion\miracles\all.asm` (GScript::MoveGameThing 0x6F8E80, SetScriptState 0x6F8370,
  SetScriptUlong 0x6F8770, Played 0x6F9DC0, GetPosition 0x6F88A0, GetDistance 0x6F8CA0, HasCameraArrived 0x6ED170),
  `bwdis.py` on the cited Living / Villager / MobileWallHug functions, and `_scratch\mapa\sldis.py` on
  `Plug Ins\ScriptLibraryR.dll` (INTCAST).
