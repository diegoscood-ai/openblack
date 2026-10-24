# Vortexes

## Summary

A vortex is a scripted object that swallows things and gives them back elsewhere. The In vortex pulls in the
villagers, animals and objects near it and writes them to a file in the save folder; the Out vortex of the next land
reads that file and throws each one out, then makes up to 30 new villagers. Both flatten the land under them as they
fade in. The tornado's carrying of objects shares the same particle machinery and is described here too; the tornado
as a miracle is on [Storm and tornado](../magic/storm-and-tornado.md).

openblack ports the vortex objects, their states and fades, the take, the hand-over, the land flattening, the effects,
the drawing and the crossing file `vortex.txt` between lands (its writers and its reader); only the vortex's own save
and load wait for saved games, as listed in [What is ported and what is pending](#what-is-ported-and-what-is-pending).

## The original

The vortex objects are `LandscapeVortex` and its three classes, in `MagicVortex.cpp` (0x5FD2D0..0x600460).

### The vortex objects

- Three classes, made by `LandscapeVortex::Create(pos, type, 50.0)` 0x5FE6E0 from the CHL `CREATE Vortex` (GScript
  0x6F171B): **In** (0x104 bytes, ctor 0x5FD7F0), **Out** (0x134, 0x5FDE20) and **Volcano** (0xEC, 0x5FD740). Any
  other type makes nothing.
- `CallVirtualFunctionsForCreation` 0x5FEE30 reads the type's **GVortexInfo** row (info.dat, table 0xD38248, stride
  0x40):
  - the initial state (info +0x20 -> +0xE4);
  - the mesh 0x230 scaled by `baseScale` (+0x28);
  - up to four particle systems: ObjectMover +0x18 (the one that pulls objects), PreLandscape +0x10 (on the ground),
    PostLandscape +0x14 and LightMap +0x1C;
  - for the Volcano only, a looping sound (0xAC).
  The `maxToCreate*` fields are not read: the Out's villager count is a literal 30 (fn_005FD480).
- CHL: `VORTEX_FADE_OUT` 257 (GScript 0x6FD8C0 -> StartFadeOut 0x5FFFD0) and `VORTEX_PARAMETERS` 328 (0x6FE090:
  SetTown 0x5FDFE0 and SetFlockParams 0x5FDFF0 of an Out: the town the arrivals join, where the arriving animals settle
  and the flock's two word fields).
- The global list [0xD38220] (linked through +0x88) is walked once a frame (fn_005FF310 from GLandscape::Draw
  0x5E4E91) and once a turn (fn_005FF330 from PSysGlobal::GameLoopEnd 0x68F5B9).
- **The vortex's player** ✅ (v1.42 exe, 2026-10-20): `MobileStatic::GetPlayer` 0x6088B0 asks what the object belongs
  to (+0x7C) and, with nothing there, gives the neutral player (g_game +0x205A5B). A vortex belongs to nothing: the base
  ctor 0x5FE830 passes 0 to the `MobileStatic` ctor 0x608710, which stores it at +0x7C (0x60873C), and nothing else in
  the vortex's code writes it. So the vortex is the neutral player's, and so are:
  - its ObjectMover, given `SetPlayer(GetPlayer())` as it is made (0x5FEFF2..0x5FF002; `GJPSysInterface::SetPlayer`
    0x55ED20 writes the effect's +0x18). Its rules (`UR_VortexAttract`, `AppearanceRuleTumble`) and its file's creators
    read no player, and the carrier it makes for each thing has its kill flag cleared (0x6D3BA7), so the carrier's
    deletion (0x6C9FC0) never uses a player either;
  - the animal Flock the Out makes (the `Flock` ctor's player, 0x5FE4FB..0x5FE516).
- A vortex cannot be picked up (ValidForPlaceInHand 0x5FF300 = 0) and is not an effect receiver. In the physics it is
  never a flying body. Only the In is hit by thrown objects: InteractsWithPhysicsObjects 0x5FD8C0 = 1, no raise, row
  13 of PhysicsConstants.

- 0x5FE2E2..0x5FE398: no file left: up to 30 new villagers (fn_005FD480(3)), one per even turn

### States and fades

- +0xE4 is `VortexStateType`: 0 Inactive, 1 Active, 2 FadeIn, 3 FadeOut. StartFadeIn fn_005FFFB0 and StartFadeOut
  set the state and +0xB4 = the turn.
- The seconds in the state, `e` (fn_005FF920): `(turns since +0xB4 + the turn fraction) × ms per turn × 0.001`.
- The presence `f` (fn_005FFAD0):
  - Inactive 0, Active 1;
  - FadeIn: 0 for 2 s, then `smooth((e − 2) / 5)`;
  - FadeOut: `smooth(1 − e / 5)` for 5 s, then 0;
  - with `smooth(x) = (3 − 2x)·x·x`.
- The fade and land curves are fn_005FFAD0, fn_005FFAA0 and fn_005FF350; with the retail spline (every y 0) their
  values are exact in float.
- Once a turn (fn_005FF4F0), after 7 s: a FadeIn becomes Active, and a FadeOut becomes Inactive and is deleted
  (`ToBeDeleted`).

### Taking objects in (the In vortex)

- **Once a turn**, only when Active (ProcessContentsOfVortex 0x5FDB60):
  - a spiral walk of up to 9999 cells from the vortex, within Create's 50 m (+0xE8) on every 3rd turn and 25 m on
    the others; the walk stops at the first cell farther than that;
  - every object of each cell that `CanBeSuckedIntoVortex` (vt +0x640) is taken in at rest;
  - a pile gives up to 1000 into a new pile, scaled `(rand(0.3)·0.4 + 0.3) × scale`, and that one is taken.
- **Which creature may be taken** is the creature's byte +0x37C, which only the scripts set, through property 36
  (CREATURE_CAN_GO_THROUGH_VORTEX). GET_PROPERTY (case at 0x70DDBD) answers 1.0 when the byte is set, else 0.0; a
  thing that is not a creature gives "Object not a creature", then "Cannot Get Property %d", and 0.0 (0x70E741).
  SET_PROPERTY's worker (case at 0x70ED24, through the table 0x70F2BC) stores 1 when the value compares unequal to
  0.0 (`fcomp`, `test ah, 0x40`), else 0: a value that is not a number compares unordered, which sets the same flag
  as equal, so it stores 0. A thing that is not a creature gives "Object not a creature" (0x70F287), then the
  "Cannot Set Property" message, and nothing is set. Ours: `script::property_rules::LetsThroughVortex`.
- **A thrown object that hits it** (ReactToPhysicsImpact 0x5FD8E0) is taken in with its velocity and its body matrix.
  This needs Active, `CanBeSuckedIntoVortex` and IN_PHYSICS.
- The take (fn_005FD9B0):
  - a creature only fizzes, `SetFizz(1, 3, true)`, and only when closer than `Get2DRadius × 0.7`;
  - anything else that `CanBecomeAPhysicsObject`, when the vortex has its ObjectMover particle system (+0x98,
    0x5FDA33), becomes a **VortexObjectInfo**: the object, "from the physics",
    velocity, matrix, the INDESTRUCTIBLE flag (+0x58, from +0x24 bit 14), the vortex;
  - the record is queued for the particle rule `UR_VortexAttract` 0x6D39C0. That rule turns each record into a
    particle carrying the object on a spiral into the vortex.
- What arrives and is not indestructible is **written to `vortex.txt`** in the save folder (VortexSave, the In's +0xEC,
  `obj->SaveObject` vt +0x82C, fn_0076FA00): see the next section.

- VortexObjectInfo (0x60 bytes, vtable 0x8F9E9C, made by LandscapeVortexIn's take fn_005FD9B0 0x5FDA3E..0x5FDA9D): what the vortex's particle rule (UR_VortexAttract 0x6D39C0) pulls in. documentacion/physics/vortex.md §6.

### Bringing them out (the Out vortex)

- An In and an Out are **not linked in memory**. The In writes what it swallows to `vortex.txt` (VortexSave mode 0),
  and the Out of the next land reads it back (mode 1, the land-script parser `LHScriptX::InitIfLevel` 0x7E7530).
  That is how the villagers and objects taken on one island come out on the next.
- Once a turn (ProcessContentsOfVortex 0x5FE030), on even turns:
  - it emits at a cell 16 m away at a random angle: the next object of the file, and when the file is empty
    **30 new villagers**;
  - the new villagers are of the town's tribe (type 7, Norse, without a town), half of them housewives and the rest a forester,
    fisherman, farmer, shepherd or leader; every 5th one is a child of `rand(9) + 1` years, the others `rand(6) + 16`.
  The statistics +0x118 / +0x120 / +0x124 count objects, food + wood and villagers ✅ (v1.42 exe, 2026-10-20): for each
  object of the file +0x118 += 1 (0x5FE275), +0x120 += `GetResource(WOOD)` (vt +0x98 with 1, 0x5FE27D..0x5FE287) then
  += `GetResource(FOOD)` (with 0, 0x5FE28F..0x5FE298), +0x124 += 1 when `IsVillager` (vt +0x2C8, 0x5FE2A3..0x5FE2AD);
  each new villager +0x124 (0x5FE303). In the vortex's code only the Out's `Save` 0x600630 and `Load` 0x600880 read
  them besides the 30-villager test on +0x124 (a scan of 0x5FD2D0..0x600B00), so the food and wood count is seen only by
  a saved game. openblack counts it the same way (`object_resources::GetResource`, Vortex.cpp).
- **The file's line and the object it hands over** 📼 1.20 only: the reader `fn_0076FA50` (from 0x5FE249)
  counts the read (+0x20), sets the position offset [0xD99724] and the tribe override [0xC20D58] = −1 and the player
  [0xC20D54] = the local player's number (0x76FA70..0x76FA9A: the byte +0xCD of the player g_game +0x205A59 names),
  then `LHScriptX::LoadOneLine` 0x7E7400. ✅ That player is never read: its only reader in .text is the getter
  0x7182B0, which nothing calls (no call, jump or pointer to it in the v1.42 exe, 2026-10-20), so it has no effect. On true it returns the land script's last created object
  [0xD99384] as it stands (0x76FAD3): it does **not** clear it first (its six readers and 54 writers in the exe's
  cross-reference index: no write in the reader). Every creation command writes it right after creating, null too: the
  villagers (0x715A9C, 0x715B3F, 0x715D78), the animals of CREATE_ANIMAL and CREATE_NEW_ANIMAL (0x716531, one block),
  the pot (0x716B46), the mobile object (0x716C94, not when `IsOkToCreateAtPos` refuses), the mobile statics of cases 41
  and 42 (0x716DAF, 0x716E52), the dead tree (0x716EF1), and the others of the dispatch. A line that makes nothing (a
  blank line, a pot of nothing, a thing refused on top of another, a miracle globe, whose one-shot commands never write
  it) hands over again what the line before made; ProcessContentsOfVortex 0x5FE161 counts it again and the hand-over
  refuses it as already thrown. `LoadOneLine` is false (the end, which deletes the reader) at the end of the file, also
  for a last line with no newline after it (that line still runs, but hands nothing over), and when `ScanLine` 0x7E7540
  returns 3 (0x7E749C). Pending: the other creation commands.
- **How the file is cut into lines** ✅ (v1.42 exe, `xref.py show` of `LoadOneLine` 0x7E7400 and `ScanLine` 0x7E7540,
  2026-10-18). `LoadOneLine` reads one byte at a time (`LHOSFile::Read` 0x7BC8E0, size 1) into the line buffer:
  - a newline ends the line and is not kept (0x7E743D); a carriage return before it stays in the line;
  - a newline with nothing before it is true and runs nothing (0x7E746E, then 0x7E7474 with no byte read jumps to the
    result, still 0): a blank line hands over again what the line before made;
  - a read error (result 3) or no byte read sets the result to 2 (0x7E7460..0x7E7464). With bytes read before it, the
    line still runs (the line count +4, `_LastLineParsed` 0xC341C0, `ScanLine` 0x7E7497) but `LoadOneLine` returns false:
    a last line with no newline is made, then the reader is deleted and nothing is handed over. With none (an empty end
    of the file) nothing runs and it is false;
  - `ScanLine` returns 3 (LH_ERROR) only at 0x7E78AE, reached from: an assignment to an unknown variable (0x7E75C2),
    with nothing after the name (0x7E75E7) or no `=` (0x7E75F2); a nesting count below zero (0x7E75A7); a string it
    cannot copy (0x7E766A); an argument naming an unknown variable (0x7E7773), or a variable of the wrong kind for the
    parameter's type (a float for `F` 0x7E778B, an empty string 0x7E779E, an integer for `N` 0x7E77B5); fewer arguments
    than the command's types (the next type `L`, `A`, `N` or `F`, 0x7E7808..0x7E7826); and 0x7E783A (the end of the line
    in the two-command case, not identified). The command callback's own result is never an error: `ProcessCommand`
    0x7E8240's value is dropped (0x7E7880) or only turned into text (0x7E7855). An unknown command word (`GetNextWord`
    above 0x1E, 0x7E7586): not settled;
  - the land-script loader `LHScriptX::LoadFile` 0x7E8170 loops on `LoadOneLine` the same way (0x7E81DB, 0x7E81F3), so a
    land script also stops at the first line `ScanLine` rejects.
- The hand-over (fn_005FE3B0):
  - a villager:
    - joins the Out's town, and becomes a disciple 12 (FROM_VORTEX) when there is also a script flock;
    - joins that flock (AddMember); when the flock is in a script, the villager becomes the script's too;
    - is kept alive while thrown (SetLife(1.0));
  - an animal is pointed at the Out's animal Flock (Living::SetFlock), made at SetFlockParams' position:
    - the flock is made at the first arrival, with SetFlockParams' two numbers as its flock distance and domain radius;
    - a new flock replaces it when its tail holds an animal of another kind;
    - SetFlock does not add a member, so the flock stays empty and every animal joins it, whatever its kind;
    - the animal also gets the Out's town (fn_00417C50);
  - anything but a Dove is then **flung** (fn_005FE5F0):
    - `v = (sin a·B, C, −cos a·B)`, with `B = (rand(5) + 8)·s` and `C = (rand(5) + 10)·s`;
    - spin `w = (−v.z, 0, v.x) / (height / 2)` in the body's axes (0x5FE691 reads v.z, 0x5FE6A3 v.x);
    - `InitialisePhysics` with the vortex as thrower: a real physics flight.

- InitIfLevel 0x7E7530, SetVariables(0xC20F00, 0x715180) 0x7E73D0, LoadOneLine 0x7E7400
- LandscapeVortexOut::ProcessContentsOfVortex 0x5FE030 (vortex.md §9, §13): the town, the thrown list, the emission on even turns (the file's objects, then 30 new villagers) and the hand-over with its fling. (pending) [0xD99384] (vortex.md §24)
- 0x5FE161..0x5FE174: the land script's last created object ([0xD99384]) is forgotten once it is not available
- 0x5FE228..0x5FE2E1 (§13, §24): with its VortexSave reader +0xF8, the next line of vortex.txt run as a land-script command at `at` (fn_0076FA50: every A position += at); the end of the file deletes the reader (0x5FE2C8..0x5FE2D5)

### The land under a vortex

- An In or an Out (not the Volcano) **flattens the 11 × 11 cells under it to their mean altitude** as it fades in
  (fn_005FF4F0, fn_005FF350):
  - the cells' original altitudes are saved the first time;
  - each turn the factor `q = 1 − (1 − f)²` grows;
  - every cell becomes `round(alt0 + q·b)`, with `b = mean − alt0` within 50 m, blending to 0 at 56 m.
- The vortex's spline (5 keys at 0, 10, 30, 50, 58 m) has every height 0 in the retail data, so it adds nothing.
- `q` only grows, because FadeOut takes it as 1: the land is **not restored** when the vortex goes.

### After a land change

- The only writer of a cell altitude, fn_00800DA0, has three callers: the vortex's flattening (0x5FF8F0 in
  fn_005FF4F0), a restore of the saved altitudes (0x5FF2C5 in 0x5FF230, which nothing calls: the land is never
  restored) and the temple's flattening (0x88293C in fn_00882730, the temple's InitTemple, 17 × 17 cells). None of
  them walks the map cells, sends a notification or moves another object; no random draw in any of them. The temple
  only re-takes its own melting (vt+0x1E8 `UpdateMelting` 0x8168F0, at 0x88296C) and bakes its outside (vt+0x208,
  0x8829AF).
- So there is **no re-seat step**: an object follows the new land only if its draw reads the ground.
  - **Follow at once**, drawn at `GetAltitude(MapCoords) + altitude` every frame: every Living (`Living::Draw`
    0x51AEC0 / `Villager::Draw` 0x51B940, `Animal::Draw` 0x51C310, `Dove::Draw`, `SpellWolf::Draw`, through
    fn_0051AF00 0x51AFBF / 0x51AFF1 / 0x51B0D9); the wood and food piles (`PileWood::Draw` 0x51BD8F, `PileFood::Draw`
    0x51C153: ground + sink offset); the spell icon 0x5197DA, `GBaseOnly` 0x609562, `MagicTeleport` 0x5FCCD3, the totem
    0x51AC3C and its statue 0x738B63, the dispenser 0x722A21, the town artifact 0x51C9C3, the scaffold 0x6EA69A, the
    fragment 0x76EC0F; physics objects in flight read the ground every turn (0x645CE7..).
  - **Stay** where they were, drawn at the y their 3D object was given when it was placed (`MultiMapFixed::Draw`
    0x518090, `MobileObject::Draw` 0x518150): every abode, Feature, BigForest, AnimatedStatic, WorshipSite,
    MobileStatic (rock, bonfire, dead or felled tree), pots and mobile objects at rest, trees (`Tree::Draw` 0x74AB00)
    and fields. The morphable meshes' per-vertex melting (`UpdateMelting`) is a snapshot taken at creation.
  - A tree is re-seated only as a side effect: when it grows (`Tree::Grow` 0x74A4CA..0x74A561, forest trees only) and
    when the cursor is over it (the hand's draw collision writes `GetWorldMatrix` into its matrix, 0x5D58DC..0x5D58FB).
- Where it follows: `y = GetAltitude(x, z) + altitude`, at the object's own point, with `altitude = y − GetAltitude`
  taken when the position was set (`MapCoords::Set` 0x603340).

### The objects a particle system carries (tornado and vortex)

`RenderParticleGameObject` (0x58 bytes, ctor 0x6C9E60) is what a particle holds. The physics part
(`physics::particle_carried_objects`, was `psys_objects`):

- **Attach** fn_006CA0E0: a flying object is not carried (the atom stays empty); otherwise **Take** fn_006CA060.
- **Take**: the class's `InitialisePhysics` with **add 0**. The object leaves the map cells and is flagged
  IN_PHYSICS without a body, and GameThing +0xA & 0x10 ("carried") is set. A villager or an animal enters FLYING; a
  villager first drops what it carries.
- While carried, the object is drawn at the particle's matrix (DrawAt 0x67B170).
- **Release** fn_006C9EF0: angles from the particle's matrix, position on the ground under it, the object's own
  class's `EndPhysics(NULL, true)` (vt +0x790; what each class does, and where the original crashes with no physics
  record: [Physics](../physics/physics.md), "The class's end of physics on Release"), the carried bit cleared. No flight and no
  impact.
- **The tornado** (UR_Tornado, miracles.md): when the particle dies, a living thing is released and dies
  (`DestroyedByEffect`), and anything else is deleted (the tornado destroys what it carries).
- **The vortex** (UR_VortexAttract): at the end of the spiral the object is released and, when the record says so,
  flung out (above). Otherwise the In's file takes it.
- IN_PHYSICS (+0x24 & 0x40) is set only by `Object::InitialisePhysics` 0x6374A5 and cleared by `RemoveObject` 0x646B56
  and `EndPhysics` 0x6375B9. A resting proxy body never has it.

### Drawing

Read in runblack.exe. Five things are drawn: the Z box (mesh 560), the ground decal (a hole and a
ring on the vortex's land block), the PreLandscape effect (before the land), the PostLandscape effect (in the
Z-sorter) and the LightMap effect. All of them are set up in `CallVirtualFunctionsForCreation` 0x5FEE30.

#### Creation

- Volcano only (fn_005FD410 == 2, 0x5FEE42): `SoundTag::Create(this, 0xAC, false, 2, -1, 0, 1, 1, 0)` (0x5FEE5A); 0xAC
  = 172, InGame.sad's G_Volcano_02.
- Position +0xD4 = (x, GetAltitude + MapCoords y, z) (0x5FEE6D..0x5FEE99); state +0xE4 = info +0x20 (0x5FEEAB).
- Unless the object is UNAVAILABLE (+0xA & 1, 0x5FEEBB), in this order:
  - the Z box +0xAC (0x5FEEC8..0x5FEFA5): a static `LH3DObject::Create(0)` with mesh MeshPack[0x230] = 560
    MSH_S_ZCHEATBOX at +0xD4 with **y = 0**, scaled by baseScale; the first primitive of its mesh gets material type
    18 (Z only, alpha test), which edits the shared mesh;
  - ObjectMover (info +0x18), only if +0x98 is empty: `GJPSysInterface::Create(NULL, type, +0xD4, (0,0,0), 1.0, NET 1)`
    -> +0x98, then SetPlayer(GetPlayer()) (0x5FEFB8..0x5FF002);
  - PreLandscape (info +0x10): `PSysInterface::Create(NULL, type, (x, GetAltitude(x, z), z), (0,0,0), 1.0, NET 0)` ->
    +0x9C (0x5FF017..0x5FF0A6);
  - PostLandscape (info +0x14): the same at +0xD4, NET 0 -> +0xA0 (0x5FF0B3..0x5FF0EA);
  - LightMap (info +0x1C): `GJPSysInterface::Create(..., +0xD4, (0,0,0), 1.0, NET 0)` -> +0x94 (0x5FF0F7..0x5FF12E).
- `PSysInterface::Create` 0x68E910 only forwards to `GJPSysInterface::Create` 0x68F2F0. Type 0 gives no effect
  (0x68F2F5). The effect's +0xAC = (NET == 1) (0x68F3AE): the ObjectMover is synchronised, the other three are local.

#### Once a turn

In this order:
1. the contents (vt +0x90C);
2. the LightMap +0x94 `Process_(info)` (vt +0x100, the turn's ms [0xD01A38]);
3. the ObjectMover +0x98 `Process_(info)`;
4. the state change and the land (above).

Both steps get the PSysProcessInfo {all 0, power +0x30 = 1.0, curl +0x34 = 0, enabled +0x38 = 1} (0x5FF512..0x5FF5EF).

#### Once a frame, before the land (fn_005FFBB0(1), from fn_005FF310 <- GLandscape::Draw)

This is stage 4h of [The frame and its draw stages](../rendering/frame.md): after the sea (4g), before the land (4i, fn_007FF610 at
0x5E4E96).
1. The Z box +0xAC is drawn at once, whatever the state (vt +0x104 = fn_00815980: on-screen and distance test, then
   DrawNow vt +0x108; 0x5FFBB6..0x5FFBC2).
2. `f` = fn_005FFAD0 (the fade). Only when f differs from the last value +0x8C (-1.0 at construction, fn_005FE9D0
   0x5FEA1F; an x87 equality test, 0x5FFBD3):
   - +0x8C = f;
   - if f != 0: the decal is made (fn_005FEA70; it does nothing while +0xA4 is set);
   - if f == 0 and the decal exists: it is released (fn_005FE8B0);
   - the Z material +0xA8's ALPHAREF byte (+4) = `min(ftol(f × 255), 235)`, an unsigned compare (0x5FFC1F..0x5FFC44).
3. If f != 0 and +0x9C (PreLandscape) exists (0x5FFC47..0x5FFC64):
   - with the argument set (always 1 here): `SetOrigin((x, GetAltitude(x, z) − ((0.3 − 2.5) × q + 2.5), z))`, q =
     fn_005FFAA0: ground − 2.5 + 2.2 q (0x5FFD3E..0x5FFD7B, vt +0x124). GetAltitude 0x803090 reads the LH3DMapCoords
     (ftol(x × 65536 × 0.1), ftol(z × 65536 × 0.1));
   - `Process_(info, g_game_time_inc)` (vt +0xFC, the frame's game ms) with info {all 0, power = **f**, curl 0,
     enabled 1} (0x5FFD81..0x5FFD95);
   - `Draw_(1.0, true)` (vt +0x104 -> fn_00679840: the Sorted path, fraction 1; 0x5FFDA3..0x5FFDAA). ZR_SurfRevol
     surfaces are drawn there and then (particles.md, 0x67CBA0), so the PreLandscape funnels are drawn **before the
     land**, after the Z box. At f == 0 the effect is neither stepped nor drawn.

#### The ground decal

- +0xA4 = `CreateMaterial(6, texture(fn_005FD4C0))`: mode 6 (SA/ISA, alpha = texture × diffuse, no Z write) with
  **S_VortexBaseMultiRing.raw** (In, Out) or **S_Volcano_Base.raw** (Volcano) (table 0xBF3F5C).
- +0xA8 = `CreateMaterial(0x12, texture(fn_005FD4D0))`: mode 18 (Z only, alpha test) with **S_VortexBaseAlphacopy.raw**
  (In, Out) or **S_Volcano_Base_Alpha.raw** (Volcano) (table 0xBF3F68).
- +0xB0 = the LandBlock holding the vortex's cell: `g_index_block[(ftol(x × 0.1) >> 4) × 32 + (ftol(z × 0.1) >> 4)]`,
  none when either cell is outside 0..511 (0x5FEAA5..0x5FEB08). Without a block the decal stops after the materials.
- The block's **+0x90C is its x corner and +0x910 its z corner** (block index × 160): LH3DIsland::Create stores
  [esp+0x18] × 160 at +0x90C (0x8043A5) and [esp+0x14] × 160 at +0x910 (0x8043B5), and files the block at
  `g_index_block[[esp+0x18] × 32 + [esp+0x14]]` (0x8042CE..0x8042D1, 0x8043C7), the same index order as the decal's
  (x first). So the decal pairs the land's u with z and its v with x, as openblack's land does (`vs_terrain`: u = local
  z / 160, v = local x / 160).
- The block gets (0x5FECF8..0x5FED94): +0x94C = 3, +0x950 = the Z material, +0x954 = the ring material, and the same UV
  matrix in +0x958 and +0x988 (fn_005FEDA0). The matrix is the identity × s, s = 1 / baseScale, with the translation
  `m9 = ((b910 − z) × 0.00625) × s + 0.5` and `m11 = ((b90c − x) × 0.00625) × s + 0.5`, in float steps
  (0x5FEB40..0x5FECF4). A vertex's (u, v) becomes (s u + m9, s v + m11): centred on the vortex, 160 × baseScale m
  across (44.8 m for the In and Out, 120 m for the Volcano).
- Release: both materials' +8 = 0, +0xA4 = +0xA8 = 0, the block's +0x94C = 0 (fn_005FE8B0). Several vortices on one
  block share its fields: the last one made there holds them, and the first one released clears the block's flags.
- **How the land draws it** (x87 fn_00874AA0; the SSE fn_007A1800 reads the same field at 0x7A2D37, 0x7A32D5,
  0x7A347F):
  - +0x94C bit 0 (0x87535D): first fn_00878F70, a Z-only pass of the block's triangles in the Z material, each vertex's
    (u, v) through the matrix and put back with its inverse afterwards (0x87907D). Then ZFUNC = EQUAL (render state
    0x17 = 3) for the block's own pass, and back to LESSEQUAL (4) after it (0x875BB1..0x875BDD).
  - +0x94C bit 1 (0x875BE8): fn_008790F0, the ring pass in the ring material. It rebuilds the block's vertices at its
    LOD (+0x930) with the diffuse = the light table entry ([0xEDD90C]) of the cell's byte 3 and the specular = the
    cell colour | 0xFF000000 (0x87915A..0x87917B), with no haze, and the UVs through the matrix +0x988. It is drawn
    under ZFUNC EQUAL when bit 0 is also set (0x875BED..0x875C3C).
  - Net effect (inferred from the states): the Z pass writes depth only where the mask's alpha >= ALPHAREF (mode 18's
    function writes ZWRITEENABLE 1 itself, 0x82E39D). The land under EQUAL is then drawn only there, so a **hole**
    opens where the mask is below `f × 255` (capped at 235). Through it shows what was drawn before: the sky, the sea,
    the Z box and the PreLandscape funnels. The ring is laid over the rest of the block in the land's light and does
    not fade with f.
  - It covers only the block holding the vortex's centre, so it is cut at that block's edge. The UVs clamp (no tiling:
    material +5 bit 2 and g_b_need_tilling, 0x87901D). The mask's border is alpha 255 and the ring's 0, so nothing
    shows outside.
  - Texture alpha (256 × 256, row 128 every 16 px): Alphacopy 255,255,255,190,180,87,85,35,64,37,78,72,183,178,255,255
    (min 16); Volcano_Base_Alpha 0 / 255 only (a hard disc); MultiRing / Volcano_Base 0,29,184,255...255,226,81. The
    game cuts them to ARGB4444 when it loads them (as openblack's texture loader does).
  - **The mirrored land** (fn_007FF4F0, stage 4c) calls the same block function (0x7FF5C0) on the same blocks (the
    transition blocks are copied whole, +0x94C included, 0x7FF593..0x7FF59F). Neither fn_00874AA0 nor its two decal
    passes read the mirror flag [0xFA92DC], set around the mirrored land (0x7FF535, 0x7FF5FD); only the vertex
    builders fn_00875C60 / fn_00876910 do. So the hole and the ring are drawn in the reflection too. There the land is
    drawn with ZWRITEENABLE 0 (0x5E48C5), but the Z pass's mode 18 turns it on again (0x82E39D); (not traced) whether
    anything in the block draw turns it off again for the mirrored blocks after it.

#### The Z-sorted part

- `SendInvisibleDrawCollision(this, +0xD4, 0.7 × Get2DRadius)` (the hand's collision); the ObjectMover +0x98
  `Draw_(true)` (vt +0x108); `AddDrawing` 0x5FFE10: `NewZObject(this, 0x5FFE70, |GetLHPoint(+0x14) − camera|²)`. The
  vortex's own mesh 559 MSH_S_VORTEXCYLINDER (`GetMesh` 0x5FEE20) is never drawn: it only gives the radius.
- **What queues it.** The object draw list fn_005E5CD0 (stage 4m) calls Draw (vt +0x610) for every available object of
  `GLandscape::DrawObjects` (0x5E6067..0x5E6086). Draw queues the Z object with no on-screen test of its own, and sets
  `g_b_last_on_screen` = 1 (0x5FFDFD), so the vortex is always "active". It is drawn again in the still-camera pass too
  (0x5E60E2..0x5E610F, which redraws only the active ones). How the list is built, rebuilt and walked is in
  [The object draw list (stage 4m)](../rendering/object-draw-list.md#the-object-draw-list-stage-4m). Two corrections to what this page said
  before: the turn trigger is "more than 10 turns since the last rebuild" (an unsigned `ja`, so every 11 turns with no
  other trigger), and the vt +0x74 skip on the object's +0x40 is GetDontDraw, not a footpath link (+0x40 is the
  Game3DObject).
  So the Post step and the light-map alpha run only for a vortex in a visible block (or the global list) at the last
  rebuild, which may be up to 10 turns old.
- The callback 0x5FFE70:
  - if +0xA0 (PostLandscape) and f != 0: `Process_(info {power = f, enabled 1}, g_game_time_inc)`, then
    `Draw_(1.0, true)`. Its origin stays at +0xD4;
  - then, if +0x94 (LightMap): the effect's alpha byte (PSys3D +0x6C) = `ftol(L × 255)` with L = fn_005FF980, then
    `Draw_(true)` (vt +0x108, the game's draw fraction g_game +0x205D64, 0x673700).
- fn_005FF980, the light map's L(e) (e = fn_005FF920, stored to a float at 0x5FF989):
  - Inactive 0; Active 0.6 ([0x92D2DC]);
  - FadeIn: `e / 2` while e < 2; then `(0.6 − 1) c + 1` with c = (e − 2) / 5, set to 0 when not above 0 (`test ah,
    0x41`) and to 1 when not below 1 (0x5FF9B7..0x5FFA1E);
  - FadeOut (any other state): `(1 − 0.6)(e / 5) + 0.6` while e < 5; then `1 − c` with c = (e − 5) / 2, kept to 0..1
    the same way (0x5FFA20..0x5FFA95).
  It is a flash to 1, then a rest at 0.6.
- The light map's stamp follows the effect's alpha: the effect's collect multiplies every atom's alpha by it
  (`Effect::CollectCollection`: alpha × global / 255 in float). The original scales the DrawData alpha byte by
  `(global × alpha) >> 8` when the global byte is not 0xFF (fn_00679920, 0x679BC2..0x679BDF, the byte set from
  PSys3D +0x6C by fn_00679860 0x679875). This is the generic particle collect: every effect with a global alpha
  rounds this way (pending, Particles).
- **Precision.** fn_005FF980, fn_005FFAA0, the height at 0x5FFD4E..0x5FFD73 and the two `× 255` run on the x87 under
  the game's 24-bit precision control ([The engine loop: turns and frames](../engine/loop.md)), so every step rounds as a float step.
  `test_vortex` pins the bit patterns, e.g. the height at ground 0,
  q 0.3 is 0xBFEB851E (in double it would be 0xBFEB851F). `ftol` truncates.

#### Deletion

Unlinks the vortex, deletes the Z box (vt +4) and the four effects at once (vt +4 with 1: no CloseDown), releases the
decal (fn_005FE8B0), then MobileStatic::ToBeDeleted. `ecs::ToBeDeleted` calls it for a `LandscapeVortex`, whether the
deletion is at once or deferred (`test_vortex` VortexDeletion). A vortex found unavailable in `ProcessAll` or the frame
updates loses them as well. A land change clears every effect (`particles::manager::Clear`).

#### The spell files (Data\Spells\ZSpellFiles\SF_*_txt.zzz, all present)

- **InBefore** (the In's and the Out's PreLandscape):
  - group 0 point (InitialScale 2, `SoundOfCreate SOUND_ACTION_VORTEX LOOPING SOFTRELEASE`);
  - then group 2 point (InitialScale 40, FollowOrigin);
  - then two funnels, `ZR_SurfRevol_AlphaFunnel` (TestFunnel, Scale 0.7, S_Teleport_Vortex_Texture.raw, additive, no
    Z, ClampToLandscape 1) and `ZR_SurfRevol_Solid` (Scale 0.8, S_Teleport_Vortex_Texture01.raw, colour 120,120,120,
    **Z write**, MaterialUseTextureAlpha 0).
- **InAfter** / **OutAfter** (PostLandscape):
  - group 0 point (OutAfter adds the same SOUND_ACTION_VORTEX loop);
  - then group 2 point (InitialScale 40, `SetScale` = StrengthFloatProvider × 1, ForceConstantHeight 15);
  - then `ZR_SurfRevol_Stars` (TestFunnelSpout, Scale 0.65, S_Teleport_Stars.raw, additive, `SetAtomAlpha` = strength
    × 255, SpeedV **+1 In / −1 Out**).
  The stars follow f through the power.
- **ZR_SurfRevol's material** (`ModifyAtomCollection` 0x686370): `CreateMaterial(6, texture)` (0x6863EC), then
  `GJUtils::SetMaterialProperties` 0x57E120 with the rule's {+0x3C UseAdditiveAlpha, +0x3D MaterialUpdateZBuffer,
  +0x3E MaterialSetDoubleSided, +0x40 MaterialUseTextureAlpha} (DefineProperties 0x6B2E80). The ctor's defaults are
  0, 0, 1, 1 (0x686231..0x68623F).
  - The rules, in order: 4 -> 6; UseTextureAlpha 0 -> 3; additive -> 13. With Z: 6 -> 5, 13 -> 12, 8 -> 3, 16 -> 9.
    Without Z: 5 -> 6, 12 -> 13, 3 and 2 -> 8, 9 -> 16.
  - So the Solid funnels (InBefore, VolcanoBefore: Z, UseTextureAlpha 0) are **mode 3**, and the alpha funnels and
    the stars mode 13.
  - Of the 132 spell files only those two Solid funnels set MaterialUseTextureAlpha 0. SF_TeleportVortex (6) and
    SF_SpellDispenserVortex (13) keep their modes.
  - The mode functions 3 (fn_0082D920) and 5 (fn_0082DD90) are the same code but for the mode cache's id: SA/ISA,
    no alpha test, ZWRITEENABLE 1, COLOROP and ALPHAOP MODULATE(TEXTURE, DIFFUSE), the material's texture. So mode 3
    draws as mode 5 does,
    and it still honours the texture's alpha.
- **LightMap**: one `ParticleLightMapCreator` atom, `Teleport_Vortex_LightMap.raw` (12 × 12 × 3, ShiftX 6.78761, ShiftZ
  8.60177).
- **ObjectMover**: `UR_VortexAttract` and `AppearanceRuleTumble` on group 0.
- **VolcanoBefore**: two fire funnels, a rock funnel, and two disc emitters (steam and fire sprites).

#### Random draws

| Where | Stream | When | Count |
|---|---|---|---|
| Effect creation (fn_00673070 from 0x68F3EC) | none: outside a step PSysRand is the 0 function | creation | 0 |
| PreLandscape / PostLandscape steps | **local** (NET 0) | every **frame** while f != 0, in the draw | one `PSysRand(0x100)` per atom made (AtomCore::Create 0x673816), all in the first steps: InBefore 4, InAfter / OutAfter 3; VolcanoBefore the same plus its emitters' sprites every frame |
| LightMap step | **local** | every turn | 1 when its atom is made; `LocalFloatRand(0)` × 3 at each draw makes no draw |
| ObjectMover step | **synchronised** (NET 1) | every turn | only for taken objects: one draw per new particle (its atom's random) |
| CRT rand() | none found in these paths | - | 0 |

The local seed is in the per-turn state hash, so a vortex's effects change the hash's random part.
- every Post afterwards, in list order. The original steps the Posts in Z-sorted order (by distance), interleaved with
  any other Z object whose callback steps an effect.

### The vortex classes in detail

The rest of the first full reading of `MagicVortex.cpp` (0x5FD360..0x600460), kept here with its addresses.

#### The functions

| Group | Functions |
|---|---|
| `GVortexInfo` (info.dat) | `GetBaseInfo` 0x5FD390, fn_005FD360, small accessors fn_005FD410..0x5FD500 |
| LandscapeVortex (base) | ctor 0x5FE7A0, dtor 0x5FE7F0, `LandscapeVortex::ToBeDeleted` 0x5FE8F0, fn_005FE8B0, fn_005FE9D0, fn_005FEA70, fn_005FEDA0, `Create3DObject` 0x5FEDF0, `GetMesh` 0x5FEE20, `CallVirtualFunctionsForCreation` 0x5FEE30, `ValidForPlaceInHand` 0x5FF300, fn_005FF310, fn_005FF330, fn_005FF350, fn_005FF4D0, fn_005FF4F0, fn_005FF920, fn_005FFAA0, fn_005FFAD0, fn_005FFBB0, `Draw` 0x5FFDC0, `AddDrawing` 0x5FFE10, `InteractsWithPhysicsObjects` 0x5FFF80, `CanBecomeAPhysicsObject` 0x5FFF90, `IsEffectReceiver` 0x5FFFA0, fn_005FFFB0 (`StartFadeIn`), `StartFadeOut` 0x5FFFD0, `GetScriptObjectType` 0x5FFFF0, `Save` 0x600000, `Load` 0x600210, `GetVortexInfo` 0x5FD570, `IsVillagerBeingThrownByThisVortex` 0x5FD6D0, the creature `Can*` queries 0x5FD620..0x5FD6B0 (all 0), `HandShouldFeelWithMeshIntersect` 0x5FD600, `PhysicsEditorCreate` 0x5FD610, fn_005FD5A0 |
| LandscapeVortexIn | dtor 0x5FD7B0, `ToBeDeleted` 0x5FD860, `ShouldPhysicsRaiseObjectUntilNotIntersectingThis` 0x5FD8D0 = 0, `GetPhysicsConstantsType` 0x5FD970 = 13, `CreatureMustAvoid` 0x5FD980, fn_005FD9B0 (the take), fn_005FDAC0, fn_005FDAF0, fn_005FDB50, `LandscapeVortexIn::ProcessContentsOfVortex` 0x5FDB60 |
| LandscapeVortexOut | ctor body 0x5FDD80 (its vtable stored at 0x5FDDAA, folded after the In's contents; it ends with fn_005FDEB0), dtor 0x5FDDE0, `ToBeDeleted` 0x5FDF10, `ProcessContentsOfVortex` 0x5FE030, fn_005FE3B0 (the hand-over), fn_005FE5F0 (the fling) |
| LandscapeVortexVolc | dtor 0x5FD700 |
| Others | `VortexObjectInfo` ctor 0x55D7F0, `UR_VortexAttract::ModifyAtomCollection` 0x6D39C0 (the spiral that pulls the taken objects in), `Creature::CanBeSuckedIntoVortex` 0x4F8A10, the CHL `CREATE` case, `GScript::VortexFadeOut` (VORTEX_FADE_OUT 257), `GScript::VortexParameters` (VORTEX_PARAMETERS 328), the PSys types 73..84 |

- The cubic spline +0xC8 is a GJArray of 5 × 0x3C bytes, made after the old one is freed with fn_005B39C0.
- The global list's second word [0xD38224] is a count (inc 0x5FEA59 at the insertion, fn_005FE9D0 0x5FEA54..0x5FEA60
  with the head [0xD38220]; dec 0x5FE908 / 0x5FE936 in LandscapeVortex::ToBeDeleted; read by nothing else); fn_005FD500 clears the head and it. The vtable store
  at 0x5FD5E8 belongs to the next folded function, so fn_005FD5A0 makes nothing.
- The emission cell of the Out uses 16 m ([0x8AB6E8] = 16, [0x92EE78] = −16) and the cell size 10 ([0x92D2A4]).
- The fade constants: 2 [0x92D2D8], 5 [0x92D2B0], 0.6 [0x92D2DC], 0.3 [0x92D2E0], 2.5 [0x92D2E4], and 1.0 [0x92D2E8]
  for the Z box's scale; the In's and Out's vtables 0x92DC44 / 0x92E560 hold `ProcessContentsOfVortex` at +0x90C.
- The land flattening: a half side of 5 cells (an 11 × 11 square), the 121 bytes of saved altitudes at +0xBC, the
  inner radius 50 [0x92D2C0];
- `Save` 0x600000 and `Load` 0x600210 are 0x210 and 0x250 bytes long (250 bytes in the old count, read as hex); the
  Out's thrown-villager list is freed node by node with operator delete 0x7AEE98.
- The Out's new villagers read the villager info table at 0xDA6BE8 + 0x3A4 × row.
- The Out's villager hand-over adds the villager to the script's flock with fn_0052FA50 (Flock add member: it clears
  the animal's old flock leader slot through fn_005ECFE0, then appends to the member list +0x3C); a villager in a
  script flock is registered with `GScript::AddScriptGameThing` 0x70D0F0. An animal of another kind is taken out of the
  Out's flock with `Flock::RemoveLivingFromFlock` 0x52FB50; a new flock is centred with `SetDomainCentrePos` 0x52FC20.

- A villager with a flock +0xF0, town or not (0x5FE434..0x5FE474, outside the town branch): fn_0052FA50 (the flock's add member) and, when the flock IsInScript (vt +0x448), the script hand-over (AddScriptGameThing 0x70D0F0 (thing, 0), IncrementScriptReference 0x70CF90, SetControlledByScript vt +0x440 (1)).
- An Animal (0x5FE49B..0x5FE578): the Out's animal Flock +0xEC is kept unless its member list's first living (+0x40, its +8) is of another kind (+0x28): then RemoveLivingFromFlock(flock, animal, 1) and a new one. A new one is `Flock(+0xFC, GContainerInfo::Definitions + 0x3C (the flock creators' info), GetPlayer() (vt +0x1C, the vortex's), 0xABA52)` (ctor 0x52F780), then SetDomainCentrePos(+0xFC), +0x52 (flock distance) = ftol(+0x108), +0x50 (domain radius) = ftol(+0x10C); then Living::SetFlock (not a member: the flock stays empty, so in play no second flock is ever made) and Animal::SetTown(+0xF4) 0x417C50.
- 0x5FE58C: a Dove (RTDynamicCast; pending: whether CitadelDove / SpellDove are Dove classes) ends here. Anything else goes to the front of +0x12C (count +0x130), is flung (fn_005FE5F0(obj, player, angle, 1.0): physics::psys_objects::Fling, below) and gets +0x25 |= 0x40 (0x5FE5DD, right after the fling, so at the release and not on landing; every kind but the dove, villagers and animals as well as objects): INDESTRUCTIBLE (bit 14 of +0x24), which the Out's ToBeDeleted 0x5FDF10 clears (& 0xBFFF, 0x5FDF3B..0x5FDF45) on everything in the list, whoever set it. It is the one bit that also keeps a thing from being eaten: the prey test fn_004196D0 refuses it (0x4196F4) and `Living::BeingEaten` 0x5EC4E0 puts it in LANDED instead of killing it ([Animals](../beings/animals.md)). The bit's other writers are `GameOSFile::LoadInstance` (0x559AA0, 0x559AED: a saved game), the `PuzzleMobileObject` ctor (0x561AAF), the puzzle game (0x6DA599..0x6DD3BB) and the script's SET_INDESTRUCTABLE; the land-script lines of `vortex.txt` do not carry it (it is given on the Out's side only).
- Fsin unrounded, x [0x8AB6E8]
- Fcos, x [0x92EE78]
- In 0x5FD860: its VortexSave writer goes; Out 0x5FDF10: its VortexSave reader and the thrown villagers' list
- ToBeDeleted 0x5FE8F0 (+ In 0x5FD860 / Out 0x5FDF10): the VortexSave writer / reader, the thrown list, the particle systems

#### The mesh and the Z box

- The main object's mesh is `GetMesh` 0x5FEE20 = 0x22F = 559 MSH_S_VORTEXCYLINDER. `Create3DObject` 0x5FEDF0 clears
  three bits of its 3D object (+4: 0x4000 fn_007F9DC0, 0x1000 fn_007F9880, 0x40 fn_008168A0). The vortex never draws it:
  it gives the radius (`Get2DRadius`) only.
- Both meshes are flagged `NoDraw` in AllMeshes.g3d. 559 has two submeshes (the second one `isPhysics`), a 14-vertex
  hexagonal prism, radius 20, y from −20 to 80. 560 is the four vertical walls of a box, x −90.5..89.5, z −90..90,
  y −400..0, with no top or bottom. The only `NoDraw` test in the LH3D code (0x7F0000..0x8A0000, `test ah, 0x20`) is
  0x80889C, in a load-time pass (fn_00808890 from `LH3DMesh::Create` 0x806602, all its buffers freed at
  0x809649..0x809678). `ComputeBoundingBox` 0x8081B0 still runs (0x806609), so the flag does not stop the draw
  🔍 inferred.
- `LandscapeVortex::Draw` 0x5FFDC0 is the Z-sorted part ([below](#the-z-sorted-part)).
- `UseSphere` (+0x5C of the funnel rule) is set by the ctor (0x686286) and has no reader in ZR_SurfRevol
  (0x685800..0x687000) (inferred unused).

#### The land-script round trip of `vortex.txt`

- The VortexSave object: vtable 0x99B3AC; its file is an LHReleasedOSFile (new 0x10C bytes, vtable 0x8C4D00) opened
  through `LHOSFile` 0x7BC650; the dtor fn_0076F9C0 closes it with `LHOSFile::Close` 0x7BC860.
- The writers are each class's land-script `SaveObject`: Villager 0x751AF0, Animal 0x41A130, Tree 0x74A9D0,
  MobileStatic 0x6088E0 (Rock), MobileObject 0x607270, Pot 0x66D550, Abode 0x405BB0 (the football too), Feature
  0x5275B0, DeadTree 0x511430, Scaffold 0x6EAF30, Creature 0x477F70, and about 40 more. Villager's starts with
  `CheckAndSetSaved` 0x56FEF0. Positions are written relative: with `at` set, Villager writes `position − at`
  (0x751B0B..0x751B21); the In passes its own position (fn_005FDAC0, 0x5FDAD4).
- The reader runs one line through the land-script interpreter (command callback fn_00715150, +0x30 = 0x715180;
  every A position through `GSetup::GetScriptPos` 0x718250 plus the offset [0xD99724]). The callback's creation
  commands, the "last created" global [0xD99384] they set and who clears it are on
  [The map script reader](../scripts/map-script-reader.md#the-command-callback-and-the-last-created-object).

- **The doves** ✅ (v1.42 exe, 2026-10-20): `Dove` and `SpellDove` save themselves with `Animal::SaveObject` 0x41A130
  (their vtables' +0x82C), which has no test of its own for a dove, so a dove the In takes is written as any animal.
  There is no `CitadelDove` class in the exe's vtables.
- **The reward chest and the ball** 🔍 not portable (v1.42 exe, 2026-10-20): both save themselves with
  `MobileObject::SaveObject` 0x607270 (`Reward`, `Ball` +0x82C), whose row is (+0x28 − `GMobileObjectInfo::InfoList`
  0xD38448) / 0x114 (0x607302..0x607319). Their info is of another table: a chest's `GRewardInfo` row (0xD50BF8 +
  type × 0x130, `Reward` ctor 0x6E5830 → 0x606E40) gives 363 and more, a ball's `GBallInfo` (0xC58498, `Ball` ctor
  0x4359B0) gives −3324, while the table has 32 rows (it ends at `g_MobileObjectCheckSum` 0xD3A6C8). The reader's
  `CREATE_MOBILEOBJECT` (0x716C03..0x716C94) takes `InfoList + row × 0x114` unchecked, so the next land reads another
  table's memory as a mobile object's row: what it makes cannot be determined. openblack writes no line for them.
- fn_0076FA50(MapCoords* at, Object** out): the next line as a land-script command at `at` (the Out's emission point): +0x20 += 1; [0xD99724] = at (every A position += at, GetScriptPos 0x718250); [0xC20D58] = -1 (no tribe override); InitIfLevel / SetVariables(0xC20F00, 0x715180) / LoadOneLine 0x7E7400; the object the line created ([0xD99384]; see the .cpp for openblack's clear before the line), or nullopt at the end of the file (*out = 0, return 0).

### Vortex save file

- 0x5FDE82: the name "vortex.txt" [0xBF40C8] in the save folder ("%s\%s", ctor 0x76F890)
- The reader (VortexSave ctor 0x76F890 with mode +0x1C = 1): the file's lines and how many were read (+0x20)
- The save folder (PathCreator::GetSaveGamePath 0x78EA60 of g_game +0x250310: "%s\%s" of the current user's path and a folder name). openblack has no save games: the folder is `SaveGame` under the game's folder, set as each land starts (`vortex_save::SaveFolderUnder`, `Game::LoadMap`); a run of the checks (`EngineConfig::checkRun`) never writes it, and its game folder holds no file, so there the Out's reader is empty, its first even turn makes nothing, then come its 30 villagers
- The ctor in read mode: "%s\%s" of the save folder and "vortex.txt" (0x5FDE82). Always a reader: without a folder or a file it is empty, its first ReadNext fails and the Out deletes it (§13), as the original's LHOSFile

### Vortex details

- fn_005FFAA0 on the vortex
- fn_005FD4E0: type != 2
- fn_005FF4D0 = 11, cdq / sar: 5
- fn_005FDB50: Get2DRadius x 0.7 [0x8AB238]
- 0x5FDB7F..0x5FDBAF: the whole radius on every 3rd turn, half otherwise
- 0x5FDBE5: off the map, straight to the next spiral step
- 0x5FDBF3..0x5FDC01: fn_00605CD0(walked, vortex) > R ends the walk. The walked MapCoords keeps the vortex's fractions, so the distance is exactly the whole-cell offset's: Hypotenuse(dx << 16, dz << 16) 0x74F680 in metres (ConvertWholeDistanceToMeters 0x74DCC0)
- 0x5FDC1E..0x5FDC33: only in the cell of its own MapCoords (ToMap equal), so an object over several cells is seen once
- Else (the same-cell test failed, or it cannot be sucked in, 0x5FDC33 / 0x5FDC42 both jump to 0x5FDC7D) a pile (IsPileResource vt +0x4CC), so a pile is split in every cell of it the walk meets: fn_0066F0D0(pile, 1000) (the tornado's split too: up to 1000 of its resource, a store pile's from its store, as a new pot of the hand's kind at its place, `object_resources::SplitOffPile`) scaled by (GameFloatRand(0.3) x 0.4 + 0.3) (one rounding, [esp+0x28]) x GetScale (vt +0x120, SetScale vt +0x124), taken the same way with no velocity
- 0x5FE3B8..0x5FE3CF: already being thrown -> nothing
- 0x5FE3F4..0x5FE418: out of its old town (vt +0x48 GetTown, Town::RemoveVillager 0x73E210), into the Out's (AddVillagerToTown 0x73A090); with a flock +0xF0 too: SetVillagerDisciple(0, 12, 0) 0x756000
- 0x5FE5D8 (the player argument is not read)
- 0x5FE077..0x5FE084: MapCoords::GetNearestTown(own pos, 200.0 [0x43480000])
- +0x12C (0x5FE08A..0x5FE12A): the gone ones leave the list; a villager in it stays alive (Villager::SetLife(1.0) vt +0x5B0, 0x5FE11E)
- 0x5FE130..0x5FE15B: while it still has something to emit (its file +0xF8, or fewer than 30 new villagers) the state's turn +0xB4 follows the game turn, so its fade does not run out while it emits
- 0x5FE17F: only on even turns
- 0x5FE18C..0x5FE223: the emission cell, 16 m away at a random angle; each axis' cell is ftol((cell x 10 +- 16 trig) / 10) and only the high word changes (the fraction of the vortex's own MapCoords is kept)
- 0x5FE252..0x5FE26F: the created thing cast to Object; none -> nothing more this turn (0x5FE39D). ⚠️ approximate an Object = a valid entity with a Transform
- The town's tribe (+0x5B8), 7 without a town (0x5FE317; Tribe::NORSE, the same enum: inferred, as TownDesire.h)
- fn_005FD420: GameFloatRand(1.0) < 0.5 -> the housewife (row tribe x 7), else GameRand(5) + 1 (forester .. leader)
- The base ctor 0x5FE830(pos, info, radius): MobileStatic with scale 1.0 (0x5FE832); +0xE0 = the info row, a constant per class (In 0x5FD7FC: 0, Out 1, Volcano 0x5FD74C: 2); +0xE8 = Create's 50.0 (0x42480000), the radius
- A creature (vt +0xA4): SetFizz(1.0, 3.0, 1) when closer than SuckRadius (0x5FD9C2..0x5FDA06), GetDistanceInMetres of the two MapCoords; SuckRadius fn_005FDB50 = Get2DRadius (vt +0x64, the vortex cylinder 559's) x 0.7; never queued
- [0xD38218] = 58 - 2 (0x5FD550)
- (pending) the spline S(clamp(r, 0, 58)) when its y are not 0 ([0x92D2C4..0x92D2D4])
- fn_005FD5A0: vt +0x914, the Out's 0x5FDFB0 looks in +0x12C, the others say no
- (pending) the 3D object +0xAC and the four particle systems (0x5FE94E..0x5FE9AD), through the particle engine
- fn_005FFAA0: q = 1 - (1 - s)^2, s = FadeValue (1 in FadeOut)
- fn_005FD5A0: any vortex throwing this villager (Villager::ThrownAnimation 0x423810)
- +0xE8: Create's 50.0 (the base ctor 0x5FE830), the In's suck radius (§8)

### Sources

- runblack.exe: MagicVortex.cpp 0x5FD2D0..0x600460, VortexSave.cpp 0x76F840..0x7700B0, PSysTornado.cpp
  (UR_VortexAttract 0x6D39C0, UR_Tornado 0x6D18B0), RenderParticleGameObject 0x6C9E60..0x6CA162.

## openblack

Status: **partial**. Progress rows: [tornado](../project/progress/miracles/tornado.md) (81%) and
[portals between lands](../project/progress/story/portals.md) (34%).

- `src/ECS/Vortex.{h,cpp}` and `src/ECS/Components/LandscapeVortex.h`: the vortex objects;
  `src/ECS/VortexSave.{h,cpp}`: the save file; `src/ECS/Physics/ParticleCarriedObjects.{h,cpp}` (was `PSysObjects`):
  what a particle system carries; `src/Particles/Rules/Storm.cpp`: the tornado's rule; `Graphics/RendererVortex.cpp`: the
  Z box, the ground decal and the funnels;
- the vortex state lives in components and its systems are reached through Locator services; the fades and the land
  curve are pure functions pinned by `test_vortex`.

Where it is in the code:

- `src/Enums.h`: `VortexType::In`, `VortexType::Out` and `VortexType::Volcano` (the three classes) and
  `VortexStateType::Active` (the state between the fade in and the fade out).
- `src/ECS/Vortex.cpp`:
  - `LandFactor`: the vortex's levelling amount for its state and seconds in it; `FlattenLand` levels the land with it,
    never under a `VortexType::Volcano`.
  - `ProcessIn`: the In's turn while it is `VortexStateType::Active`. It looks at the whole radius every third turn and
    half of it otherwise, ends the spiral walk past the radius, steps over cells off the map, and takes an object only
    in the cell of its own map position, so an object over several cells is seen once. Anything else that is a pile,
    in any of its cells, gives up to 1000 into a new pot (`object_resources::SplitOffPile`), taken the same way.
  - `TakeIn`: a creature sparkles away near the middle; anything else that can become a physics object, when the In
    has its object mover, becomes a `VortexObjectInfo` record and a target of the mover. `UR_VortexAttract`
    (`src/Particles/Rules/VortexAttract.cpp`) takes the records back (`vortex::TakeQueued`, the last first), starts
    carrying each thing (`particle_carried_objects::Take`; one that struck it in flight leaves the physics first),
    hands off what can be destroyed (`vortex::HandOff`: a villager leaves its town, an animal its town and flock, then its
    line is written), and draws each particle in (`vortex::crossing::Attract`); at the middle the particle goes, and
    the thing with it (its carrier's end, `ToBeDeleted`). The original's throw back out of what cannot be destroyed
    (0x6D3E43..0x6D3ED4) needs AtomData +0x38, which only AtomData's Load sets: in play it never runs, so here too
    that thing is gone at the middle (unwritten).
  - A creature may be sucked in when a script let it through (property 36, `Creature::mayGoThroughVortex`, +0x37C)
    and it is on the leash, or when a script controls it (+0x25 & 4, CONTROLLED_BY_SCRIPT; `Creature::CanBeSuckedIntoVortex`
    0x4F8A10); its route keeps clear of a way out of a land otherwise (`LandscapeVortexIn::CreatureMustAvoid` 0x5FD980:
    1 without a creature, else !CanBeSuckedIntoVortex; `vortex::CreatureMustAvoid`).
  - `ProcessOut`: the Out's turn. It finds its town (the nearest within 200 m), drops the gone villagers from its
    thrown list and keeps the others alive, and holds its fade while it still has something to emit. On even turns it
    emits at a cell 16 m away at a random angle: the next line of `vortex.txt` while the reader lasts, then up to 30
    new villagers, one a turn, of the town's tribe (Norse without a town), a housewife or another job at even odds.
  - `HandOver`: what the Out does with an emitted object: nothing if it is already being thrown; a villager leaves its
    old town for the Out's; an `AnimalInfo::Dove` ends there; anything else joins the thrown list and is flung.
  - `vortex::ReactToPhysicsImpact`: an In takes a hitter only when it is a `VortexType::In`, active, and the hitter
    can be sucked in and is in physics.
  - `vortex::IsVillagerBeingThrown` (declared as `IsVillagerBeingThrown` in `Vortex.h`): whether a `VortexType::Out`
    holds this villager in its thrown list; the villager's thrown animation asks it.
- `src/ECS/VortexSave.{h,cpp}`: `SetSaveFolder` sets the save folder, `SaveGame` under the game's folder, as each land
  starts; `SetWritable(!checkRun)` with it. `OpenReader` always returns a reader of `vortex.txt` in that folder; without
  a folder or a file it is empty, its first read fails and the Out drops it. `OpenWriter` (made by an In) empties the
  file and writes it; none, with no attempt to write, in a run of the checks. Each thing is written once, and a static's
  fellows lying just where it stood count as written with it (`Writer::written`, `vortex_save_lines::IdenticalStatics`).
- `src/ECS/VortexSaveLines.{h,cpp}`: each class's line (villagers, special villagers, animals, pots but a storage pit's,
  trees, mobile statics with their artefact line, mobile objects, dead trees, miracle globes), placed relative to the
  vortex, through the land-script writer (`lhscriptx::WriteCommand`). A villager's line names no town and an animal's no
  flock nor town, as it left them as it was taken (the original's too). Tests: `test_vortex_save_lines`.
- The Out's hand-over (`HandOver`): a villager's town, its FROM_VORTEX disciple and the script's flock and hold; an
  animal pointed at the animal flock and given the town; then the fling and `Indestructible`, cleared on everything
  thrown as the Out goes (`OnDeleted`). `Indestructible` is also what `script_held::CannotBeEaten` reads, so until
  then nothing hunts what came out, and one caught gets up instead of dying (test
  `VillagerCoreTest.WhatCannotBeDestroyedCannotBeEaten`). A villager thrown out flails (`villager_animation::ThrownClip`, clip 401).
- A vortex is filed in the map cells (a MultiMapFixed, as the original's MobileStatic class); the In interacts with
  physics objects (0x5FD8C0 = 1), its body is its cylinder (mesh 559), its constants type 13 (0x5FD970), its info the
  statics' vortex row. A raise never makes it a resting body (0x5FD8D0 = 0, the raise's choice of new resting bodies),
  but once something moving near it has put it in the physics, a raised object goes over it like over any listed
  body: the raising loop 0x644AE0..0x644C21 has no class test (`physics::turn::RaisedOver`, test
  `PhysicsTurn.ARaisedBodyGoesOverEveryBodyButTheVillagerAndPushedPairs`). Struck while active by a thing in flight that it
  could take, it takes it with the vortex's own body's velocity (+0x104) and matrix (fn_007FD140 of +0x28)
  (`LandscapeVortexIn::ReactToPhysicsImpact` 0x5FD8E0; `DynamicsSystem::ReactToPhysicsImpact`).
- To the scripts every vortex is type 23 VORTEX (`LandscapeVortex::GetScriptObjectType` 0x5FFFF0, vt +0x4E8) and its
  sub-type is its kind, In 0, Out 1, the volcano 2 (`GScript::GetSubType` 0x6F6D00 reads the vortex's type for
  `SCRIPT_OBJECT_TYPE_VORTEX`): `script_type::TypeOf` and `SubtypeOf` (`ECS/ScriptTypes.cpp`), which GAME_SUB_TYPE
  and the finders read. Before, openblack gave a vortex no script type at all. Test: `test_script_delete`.

Porting decisions taken from raffclar's tree (the vortex rules and the vortex service):
[Port decisions: engine and world](../project/port-decisions-engine-world.md#vortex).

#### The vortex objects

- openblack `InfoConstants::vortex`

#### Bringing them out (the Out vortex)

- openblack: `vortex_save::ReadNext` returns `Script::LastCreated()` after the line with no clear (was: cleared before each line); the pot, mobile object, mobile statics, dead tree and animal commands set it (`FeatureScriptCommands`), as the villagers' and the flowers' did.
- openblack cuts the file into its lines as the original reads them (`vortex_save::ReaderFrom`, tested in
  `test_vortex`): a blank line runs nothing and hands over again; a last line with no newline after it runs, then
  ends the file, handing nothing over (`Reader::lastLineEnded`). A carriage return before a newline is dropped.
- openblack skips a line it cannot read and goes on, where the original ends the file there (and a land script's
  loading too); which of openblack's reader errors are the original's is pending.
- Testbed: a run of the checks never writes the crossing file, so the testbed's `vortex crossing file` command gives a
  way into a land the text of one in place of the file; `story.vortex_throws_out_the_last_crossing` gives it a farmer,
  a cow, a blank line, a rock and a farmer on a last line with no newline, as openblack's writer writes them.

#### After a land change

- **openblack.** The Transform holds the absolute y, so the things that follow are moved when the land changes: `FlattenLand` (ECS/Vortex.cpp) and `FlattenLandUnderTemple` (CitadelArchetype.cpp) record the ground (`GetHeightAt`) under every villager, animal, creature and wood or food pile whose cell reads a corner of the flattened box, rewrite the altitudes, rebuild the land, then publish `events::LandAltitudesChanged` with those grounds; its handler (`ecs::land_reseat`) sets each one at `new ground + (y − old ground)` (a pile's sink base the same way), moves the places its drawing glides between by as much (`DrawPosition::turnStart`, a creature's `CreatureLocomotion` from and to, from raffclar's lane, unverified against v1.42), and leaves the thing exactly where it was when its ground did not change bitwise. Testbed: `story.vortex_levels_under_the_living`. Buildings, trees, features, fields, rocks and pots are not touched.

#### The objects a particle system carries (tornado and vortex)

- openblack's carried object has the `InPhysics` component as well as `CarriedByTornado` (see [Physics](../physics/physics.md), "In physics or not").

#### Drawing

- openblack has the three local effects, their steps and the decal's state (`ECS/Vortex.cpp`), the Z box and the decal's drawing in the main land, and the ground effect's funnels drawn before the land (`Graphics/RendererVortex.cpp`).

#### Creation

- openblack: `ecs::vortex::Create` through `audio::vortex_sound::CreationTag` (issue 22, [The audio engine](../audio/engine.md)).
- openblack (`vortex::Create`): the ObjectMover (Synced), Pre, Post and LightMap (Local) through `particles::manager::StartForSpell`, Pre and Post stepped per frame. The effects stand at (x, the land's height, z): the script's vectors have y = 0, so this is +0xD4. `TakeIn` uses the ObjectMover as the "+0x98 exists" gate (0x5FDA33) and hands each record to it (AddTarget_ vt +0x114, 0x5FDAB1), and `UR_VortexAttract` takes the object. The vortex's player (`ecs::vortex::k_Player`, the neutral player) is given to the ObjectMover as it is made, and to the animal flock the Out makes.

#### Once a turn

- openblack: `vortex::ProcessAll` steps the LightMap, then the ObjectMover, after the contents, with the turn's length.

#### Once a frame, before the land (fn_005FFBB0(1), from fn_005FF310 <- GLandscape::Draw)

- openblack, the box: `Renderer::DrawVortexBoxes` (`Graphics/RendererVortex.cpp`), called by `DrawPass` in the main view after the sea and before the land, for every `LandscapeVortex` (available or not: fn_005FF310 walks the whole list with no test). Mesh 560 from the mesh cache (`MeshId::SpellZCheatBox`), placed by `vortex_draw::ZBoxModel` (x, 0, z, the 3x3 scaled by baseScale, not turned; `test_vortex` ZBoxModel), drawn in mode 18 (`render_modes::State(ChromaDepthOnly)`: depth written, blend ZERO / ONE, so no colour) with the primitive's own culling. The mode's alpha test is left out: the material's ALPHAREF is 0, so every pixel passes. The "Celestial" program draws it (plain L3D geometry placed by one model matrix).
  - The mesh-level `NoDraw` flag of 560 does not stop the draw (inferred, see Creation above): the only test of it is the
    load-time pass at 0x80889C.
  - fn_00815980's distance test (0x815A23..0x815A47) skips an object farther than `radius × scale(+0x44) × 300` from the
    camera. 560's radius is at least 90 (half the box's width), so that is at least 7.5 km for an In or Out (20 km for
    the Volcano), more than a land's diagonal (about 7.2 km): 🔍 inferred never reached in play, so it is not ported. The
    on-screen test is bgfx's clipping.
  - Only in the main view: the mirrored land (stage 4c) comes before 4h and draws no box.
- openblack, the effects: `vortex::UpdateGroundEffects`, from `magic::Update` before every other per-frame effect step there (the fire graphics, the hand's effects, the shields, the teleport stones). It sets the decal's state (`NextDecal`, `GroundHoleThreshold`), then the PreLandscape's origin (the ground less `SwirlDepth`) and its step with power f, for every vortex. `vortex::UpdateOverLandEffects`, after the teleport stones and before the falling spell, does the Post steps and the LightMap alphas (below).
- openblack, the ground effect's draw (`Graphics/RendererVortex.cpp`):
  - `Renderer::SplitVortexParticles`, at the end of the frame's PSys collects (`CollectFrameParticles`, main view with
    the entities): for every `LandscapeVortex`, `vortex_draw::RoleOf` tells each collected effect apart
    (`test_vortex` EffectRoles). A vortex shows while its `decal.made`, which `NextDecal` sets exactly while f != 0.
    - The surfaces of a shown vortex's PreLandscape (a Sorted effect, as the original's `Draw_(1.0, true)`) leave the
      at-once list for `groundSurfaces`.
    - Its sprites, if any (the Volcano's steam and fire), stay in the Z-sorter: on the Sorted path the original
      queues them as Z objects too.
  - `Renderer::DrawVortexGroundEffects`, right after `DrawVortexBoxes` (main view, with the land and the entities),
    draws `groundSurfaces` in the walk's order through `DrawParticleSurface`. The solid funnel writes depth there, and
    the land is drawn over both funnels except in the decal's hole.
  - ⚠️ approximate With several vortices the original draws each vortex's box and then its funnels, one vortex after
    the other. openblack draws every box, then every funnel; the two differ only where one vortex's box hides another's
    funnels.
  - At f == 0 (0x5FFC47: no step and no draw) the PreLandscape effect is neither stepped (`UpdateGroundEffects`) nor
    drawn: its surfaces, sprites and chains are dropped from the frame.

#### The ground decal

- openblack (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`):
  - `CollectVortexDecals`, in the main view's land pass: every `LandscapeVortex` whose `decal.made` (C1's `NextDecal`)
    gives its block (`vortex_draw::DecalBlock`: the cells truncated, none off the 512 cells), its UVs
    (`vortex_draw::DecalUvTransform`, the float steps above), its `decal.alphaRef` and its textures from the resource
    cache (`raw/<mask>a`, `raw/<ring>`, `raw/<ring>a`; `vortex_draw::DecalTexturesFor`). One decal per block: the first
    vortex found holds it (approximate: the original keeps the last one made).
  - In `DrawPass`'s block loop, for that block only: `DrawVortexDecalMask` (program "VortexDecalMask" = `vs_terrain` +
    `fs_vortex_mask`: mode 18's state, depth written where `round(alpha × 255) >= ref`, discarded elsewhere), then the
    block in its usual state with the depth test EQUAL, then `DrawVortexDecalRing` ("VortexDecalRing" = `vs_terrain` +
    `fs_vortex_ring`: mode 6 under EQUAL, colour = ring × the land light × the pass's light scale + the cell colour,
    alpha = the ring's, `u_hazeBlock` 0 for no haze), then the block's projected shadows as before. Both programs use the
    land's own vertex program and the block's uniforms, so their depth is the block's to the bit. The textures are
    clamped.
  - 🔍 inferred the light table entry's alpha is 255 (openblack's land light texture has 255 there), and the ring pass has
    the land's SPECULARENABLE on.
  - The mirrored land in the reflection draws no decal yet (pending, see above).

#### The Z-sorted part

- openblack has no logic-side copy of that list or of the block culling (it is in the renderer), so it steps them every frame (pending).
- openblack: at f == 0 (0x5FFE89) the PostLandscape effect is neither stepped nor drawn (`SplitVortexParticles` drops it from the frame, as it does a hidden PreLandscape). Shown, its stars are drawn at once with the other Sorted effects' surfaces, before the Z-sorter's drain. The original draws them inside the vortex's Z object, at its place in the drain (pending).
- openblack computes them in float in the same order

#### Deletion

- openblack: `vortex::OnDeleted` deletes the effects (`particles::manager::Delete`) and clears the decal.

#### The spell files (Data\Spells\ZSpellFiles\SF_*_txt.zzz, all present)

- openblack: `Surface::useTextureAlpha` (Particles/Rules/SurfRevol) goes to `ModeFromProperties` in `DrawParticleSurface` (`test_vortex` FunnelMaterials: 3 and 5 give the same state).

#### Random draws

- openblack steps Pre and Post in the logic frame update (as the teleport stone's vortex):
  - every Pre first, before the other per-frame effect steps, as stage 4h comes before the object draw, the hand and the
    particles

#### The functions

- openblack writes each cell through `LandIslandInterface::SetCellAltitude` and rebuilds the altitudes.

#### The land-script round trip of `vortex.txt`

- The writers' pass over identical things ([Every writer](../scripts/land-change-and-saving.md#the-physics-classes):
  MobileStatic 0x608A20..0x608A9E, MobileObject 0x607366..0x6073E6): once a static thing's or a thing lying about's
  line is written, the others of its class and row lying on the land at the very same MapCoords (x, z and the altitude)
  count as written. openblack: `vortex_save_lines::IdenticalStatics` and `IdenticalMobileObjects` (not a scaffold,
  whose class and table are its own), marked in the writer's `written` set.
- A town in a line is named by the number the scripts find it by: the town's +0x5B4, which `GGame::FindTownWithID`
  0x552FA0 compares (0x552FBD) and the writers print (the scaffold's at 0x6EAF9A). openblack writes
  `Town::scriptId`, else `Town::id`, as `town_queries::ScriptIdOf` (so a town a scaffold founded is named by the
  founded towns' number).
- Every line's angles are the ones the thing keeps, as every writer of the original reads them (its stored angles, never worked out again from its matrix; [Every writer](../scripts/land-change-and-saving.md#the-physics-classes)). openblack: `placed_turn::AnglesOf` in `vortex_save_lines::LineFor` (since 2026-10-20; before, the angles were taken apart from the turn in x, y, z order, which for a thing that leans gives another turn once the next land reads the line, and wrapped an upright thing's angle into (-pi, pi]). A thing whose turn changed since it kept its angles is written with the angles of its turn now, as the game finds them.
- A dead tree's line names the player it belongs to (the exe's +0x90, `Rock::GetPlayer`). openblack: `DeadTree::owner` through `vortex_save_lines::OwnerOf` (since 2026-10-20; before, every dead tree crossed as the neutral player's).
- In openblack an A position is made by the land-script `GetParameter`: a string with one comma becomes `{x, GetHeightAt(x, z), z}`; the vortex's offset is added to x and z before the height is read.

### What is ported and what is pending

| Part | State |
|---|---|
| GVortexInfo, the three classes, Create (radius 50, scale 1), the state and fades, FadeIn -> Active / FadeOut -> delete at 7 s, the turn from GameLoopEnd | ported (Vortex.cpp, MagicLoop.cpp) |
| The land flattening (fn_005FF4F0 / fn_005FF350, the retail zero spline) | ported (`SetCellAltitude` + `CommitAltitudeChanges`, as the citadel) |
| VORTEX_FADE_OUT / VORTEX_PARAMETERS / CREATE Vortex | ported (CHLApi.cpp) |
| The In's take (spiral, ReactToPhysicsImpact, CanBeSuckedIntoVortex 0x639B60, the same-cell test, VortexObjectInfo, the pile split, the creature's sparkle) | ported (unverified against v1.42) |
| UR_VortexAttract (the spiral of the carried objects) | ported (`Particles/Rules/VortexAttract.cpp`, unverified against v1.42) |
| particle_carried_objects Take / Release / Fling, the carried bit | ported (ParticleCarriedObjects.cpp); the tornado's side is in Storm.cpp |
| The Out's emission (the cell 16 m away, the file's objects, 30 new villagers), the hand-over to its town, the thrown list kept alive, the fade held while it emits, the fling, the statistics (objects, their wood and food, villagers) | ported |
| The hand-over's flocks (the script flock, the animals' Flock), the disciple FROM_VORTEX, the script hand-over, the animal's town, INDESTRUCTIBLE | ported (unverified against v1.42) |
| VortexSave (`vortex.txt` between lands) | the writers: `lhscriptx::WriteCommand` and the physics classes' SaveObject ([Land changes, skipping the tutorial and saving](../scripts/land-change-and-saving.md)); the reader: one line at a time through the land-script interpreter with the position offset and the last created object (Script's map state); the folder `SaveGame` under the game's; the writers of villagers, animals, pots, trees, statics (artefacts), mobile objects, dead trees, globes, fragments (written as rocks) and scaffolds (`CREATE_SCAFFOLD`, and its reader) (`ECS/VortexSaveLines`), a town named by the scripts' number and a pile of identical things lying about written once; MAKE_LAST_OBJECT_ARTIFACT's reader ([Artefacts](../beings/towns.md#artefacts)); the reward chest and the ball have no line (undefined in the original, see "The land-script round trip of `vortex.txt`") |
| Drawing: the PreLandscape, PostLandscape and LightMap effects (made, stepped, deleted), the light map's L(e), the decal's change detector and alpha reference, the ground effect's height | ported (Vortex.cpp, MagicLoop.cpp; tested in `test_vortex`) |
| Drawing: the Z box (mesh 560 at (x, 0, z) × baseScale, mode 18, after the sea and before the land, main view) | ported (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`; tested in `test_vortex`) |
| Drawing: the decal's hole and ring on the vortex's block (mask, block under EQUAL, ring), in the main land | ported (`Graphics/RendererVortex.cpp`, `fs_vortex_mask`, `fs_vortex_ring`; tested in `test_vortex`) |
| Drawing: the PreLandscape funnels right after the boxes and before the land, Pre and Post undrawn while f == 0, the Solid funnel's mode 3 (MaterialUseTextureAlpha), the light map stamp following the effect's alpha | ported (`Graphics/RendererVortex.cpp`, `Graphics/VortexDraw.cpp`, `Particles/Rules/SurfRevol.cpp`, `Graphics/RendererRevolvedSurface.cpp`; tested in `test_vortex`) |
| Drawing: the decal in the mirrored land, the stars drawn at the vortex's Z object, the global alpha's rounding, the ObjectMover | pending (the renderer, the particle rules) |
| Save / Load of the vortex | not ported (openblack has no saved games) |
| The vortex that leaves a land | the scripts open it with CHL `CREATE` of an In at a marker (`challenge.chl`: Land 1 `LeaveThroughVortexL1` at (1700.23, 11.40, 2520.18), once `OpenVortexL1` is set or at once when the creature guide is skipped; Land 2 at (1061.11, 147.79, 3597.97); Land 4 at (2060.18, 11.39, 2591.70); Land 3's script is given its place when it starts). Opening it is ported, and `LandControlAll`'s `LOAD_MAP` changes the land (`events::LandChangeRequested`, `Game::ChangeLand`; see [Land changes](../scripts/land-change-and-saving.md)); a whole crossing in play is not checked against the original |
| Debug window | Debug > Windows > Vortices (`Debug/Vortices`, `VorticesModel`): the vortices on the land with fade out and delete, a new In / Out / Volcano at the hand or at a click, each land's leaving vortex opened at the script's place, and the crossing file's line the thing in the hand would be written as (`vortex::LineWhereItIs`) |

- CanBeSuckedIntoVortex (vt +0x640): Object 0x639B60 = CanBecomeAPhysicsObject (vt +0x7B0); only Creature overrides it (0x4F8A10: (+0x37C and its leash on) or CONTROLLED_BY_SCRIPT)

### Test hooks

- None of their own; `test_vortex` and `test_vortex_rules` pin the fades, the land curve and the rules.

## Pending

- **Verify with xref/oracle:** the 1 **🔍 inferred** and 1 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- The drawn vortex (the Z box, the ground decal and its ring, the funnels) follows the addresses above, but its look has
  not been compared with a shot of the retail game yet.
- The crossing (unverified against v1.42): the In's take, `UR_VortexAttract`, the writers and the Out's hand-over are
  ported from raffclar's lanes (PRs #349, #382, #389, #408) refitted onto ours; to measure against the exe: the
  spiral's roundings (the x87's precision is taken as 53 bits, as elsewhere), the atom's first matrix, and the In's
  struck-in-flight take with the vortex's own body.
- VortexSave: the reward chest and the ball (see "The land-script round trip of `vortex.txt`": undefined in the
  original, not ported). The reader: the
  last created object set by the creation commands other than the villagers, animals, pot, mobile object, mobile
  statics, dead tree and flowers; a line the interpreter cannot parse ends the file in the original (see "How the file
  is cut into lines"), and a land
  script's loading too, where openblack skips the line; which of openblack's reader errors are the original's, and
  whether an unknown command word is one.
- Drawing, the renderer's side:
  - the decal's hole and ring in the mirrored land (the original draws them there too, and its Z pass turns the
    mirrored land's Z writes back on; see "The ground decal");
  - several vortices on one block: the last one made should hold the decal (openblack: the first one found);
  - with several vortices, each one's box then its funnels, vortex by vortex (openblack: every box, then every
    funnel);
  - the PostLandscape stars drawn inside the vortex's Z object, at its place in the drain (openblack: at once with
    the other Sorted surfaces, before the drain);
  - the global alpha as `(global × alpha) >> 8` on the alpha byte (openblack: alpha × global / 255 in float, in the
    generic particle collect, so for every effect with a global alpha).
- The ObjectMover's own drawing (`Draw_(true)` in the Z-sorted part): the carried things are drawn as game objects at
  their particles' places (`particles::vortex_attract::UpdateCarriedObjects`); the 3D object flags the original sets on a
  carried thing (0x6D3B49..0x6D3B8E: vt +0x10, +0x10 = 10.0, vt +0x98) are not ported.
- A creature's route kept clear of a way out of a land needs the creature's obstacle planner (raffclar #303/#330).
- (not traced) the audio DLL's sample choice for SOUND_ACTION_VORTEX (the volcano's SoundTag 0xAC is G_Volcano_02, made since issue 22).
- Save / Load of the vortex (openblack has no saved games).
- After a land change (see above), 🎯 chosen differences of openblack:
  - a re-seated villager or animal whose own turn came before the change is drawn sliding to its new height over the
    rest of that turn (the draw goes from its position at the turn's start) instead of at once;
  - the other classes that follow the land in the original (spell icon, GBaseOnly, MagicTeleport, totem and statue,
    dispenser, town artifact, scaffold, fragment) are not re-seated;
  - openblack's morphable meshes take the melting on the GPU at every draw, so a field or a morphable building
    follows the new land, while the original's melting is frozen;
  - the tree's re-seat when it grows or is under the cursor is not ported.
- After a land change: the creature's drawn height (`Creature::Draw` 0x517910 and `LH3DCreature` not traced); whether
  footpaths are drawn in the retail game.

