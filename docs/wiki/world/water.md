# Water

## Summary

Everything about water that is not the drawing of the sea: which map cell is water, the water queries (coast, river,
drinking water), the creature's `LandAvoid` mask, the water commands of the scripts, things that fall into the water,
sinking and drowning, and the rings on the surface. The sea, the coast and the rivers are drawn as described in
[Sea, coast and rivers](sea-coast-and-rivers.md); the sharks, the fish puzzle, the boat and the fixed scenery are on
[Sea life and fixed scenery](sea-life-and-scenery.md). What water sounds like is on
[World sounds](../audio/world-sounds.md#water-audio).

openblack ports the cell predicates, the queries, the mask, the impacts, sinking, drowning and the rings from the
original, with tests on synthetic cells and on the real Land 1 when it is present.

## The original

The sea, the coast and the rivers are drawn in [The frame and its draw stages](../rendering/frame.md); the reflections
and the underwater cuts in [Model lighting and colour](../rendering/model-lighting.md).

### Water cells (SeaCells)

All of the original's land/water is decided with five predicates on the **terrain cell** (8 bytes; byte +4 =
raw altitude, byte +6 = LND properties: 0x10 `hasWater`, 0x20 `coastLine`). Previously each module had its own rule
(flags, height ≤ 0, clipped edge); now there is a single module with the exact table, **including the map edge**:

| openblack function | Original | Rule | No cell (outside the map or no block) |
|---|---|---|---|
| `sea_cells::IsWater` | `MapCoords::IsWater` 0x6035B0 | `properties & 0x10` | **water** (1) |
| `sea_cells::IsLand` | `MapCoords::IsLand` 0x603720 | `!(properties & 0x10)` | 0 |
| `sea_cells::IsDryLand` | `MapCoords::IsDryLand` 0x603620 | **altitude ≥ 4**, does not look at the flags | 0 |
| `sea_cells::IsCoastal` | `MapCoords::IsCoastal` 0x6036A0 | `!(properties & 0x10) && (properties & 0x20)` | 0 |
| `sea_cells::InBounds` | `MapCoords::InBounds` 0x6042C0 | the cell falls inside the game map (unsigned comparison) | — |
| `sea_cells::CollideLandscape` | terrain part of `MapCell::Collide` 0x601BD0 | 0x10 outside the map (fn_00601E00), otherwise 1 water / 2 land | 0x10 / 1 |
| `sea_cells::GetSurfaceType` | `GSoundMap::GetSurfaceType` 0x71D8E0 | 6 with no cell, 7 if `!IsLand`, otherwise the material's `surfaceSound` (3 if it is not 1..8) | 6 |

- The cell of a point is `MapCoords` = `ftol(world·6553.6) >> 16` (10 units per cell, truncated; a negative x
  goes off the map through the unsigned high word, it is not clamped to 0). The impact against the water and the drop rule also use
  the cell **rounded to the nearest** (`fistp` in the original, `std::lrint`), half a cell of difference.
- Bits 0x04 (field), 0x08 (fixed) and 0x20 (tree) of `MapCell::Collide` come from the objects of the map cell,
  which openblack does not list yet: `CollideLandscape` only gives the terrain part.
- Consumers today: `CollisionSounds` (impact against the water), `AnimationSounds` (surface of the clips),
  `HandSystem::IsLand` (and with it the fish, the trees and the hand pots), `HandHolding` (gentle drop),
  `ecs::pot_resource::IsWater` (resources lost in the sea), `FishShoals::IsOkToCreateFishFarmAt`, `WaterQueries` and `LandAvoid`.
- **Fish farms**: `GFishFarmInfo::IsOkToCreateAtPos` 0x52D100 = `IsCoastal` **and** that there is not already a farm in that
  cell (`MapCoords::FindType(0x21)` 0x6045C0). No town, no depth, no distance. Today only the script creates
  farms, so the rule is ported (`IsOkToCreateFishFarmAt`) but unused.
- Unit test: `test/test_sea_cells.cpp` (hand-made two-block island + real `Land1.lnd` if installed:
  open sea 1464.2016; shore of altitude 1 at 1485.2015; dry land of altitude 44 at 1788.4/2710).
- The predicates span MapCoords 0x6035B0..0x603840. Each reads the landscape cell `g_ptr_blocks[g_index_block[x >> 4][z >> 4]] + ((x & 15) × 17 + (z & 15)) × 8`, with x, z the high words of the 16.16 MapCoords (world / 10). A cell off 0..511 or without a block is "no cell".
- `IsDryLand` is `cmp al, 4; sbb; inc` (0x603682). `GetSurfaceType` returns 7 for !IsLand at 0x71D966, and 3 for a material sound ≤ 0 or ≥ 9 at 0x71D97A.
- The fish farm object:
  - `FishFarm::ToBeDeleted` 0x52C690 → `DeleteDependancys` 0x52C6B0 (0x52C693). While the fishermen list is not empty, it calls the head villager's SetTopState(163) (vt +0x8E8). `ExitFishing` 0x75B880 then unlinks it, the farm still being available (GameThing +0xA bit 1 is set later, 0x56FB7B) (0x52C6B3..0x52C6D7).
  - `DeleteDependancys` then leaves the town's list +0x788 (0x52C6DA..0x52C741) and g_game +0x205C0C (0x52C75E..0x52C7A7), and calls RemoveMapObject (0x52C744..0x52C758).
  - `AddFisherman` fn_0052D250 does nothing for a null villager (0x52D256..0x52D25A). It puts a new 8-byte node {next, villager} at the head, +0x84++ (0x52D25C..0x52D27B), with no duplicate test (`EnterFishing` tests first, 0x75B85E) and no maximum.
  - `RemoveFisherman` unlinks every node of the villager (+0x84−− each, 0x52D2A6..0x52D2DD) and does not write the villager's +0x118.
  - The farm's score fn_0052D2F0 = ftol(fn_0052D240 (fld 1.0 [0x8AA390]) × (1 − min((float)((u64) +0x84 / (int) maxNoFishermanPerFishFarm (+0x120)), 1))) (0x52D2F3..0x52D34A): 1 with no fisherman, 0 with 1..4.
  - `FishingSpot` fn_0052C870: r = Get2DRadius (5), h = r × 0.5 (0x52C88A..0x52C8A6). It adds GameFloatRand(r) − h to x ("FishFarm.cpp" 0xB8, 0x52C8AA), then to z (0xB9, 0x52C8C2), each axis ftol((pos × 10 × 2^-16 + d) × 65536 / 10) (0x52C8CB..0x52C926); the altitude is copied.
  - `FishFarm::RemoveResource` 0x52CF20 (vt +0xA0): FOOD (type 0) → RemoveFood(n); any other type 0 (0x52CF20..0x52CF37).
  - `RemoveFood` 0x52CED0 with n above the food (0x52CED0..0x52CEE2) returns ftol(food) and sets food = 0 (0x52CEFC..0x52CF09). Otherwise food −= n and it returns n (0x52CEE4..0x52CEF0).
  - `FishFarm::GetTown` 0x52C450 = +0x8C. A town's fish farms (Town +0x788) run newest first (head insertion in the ctor, 0x52C407..0x52C418).
  - Fishing from the hand (`NetworkFriendlyStartLockedSelect` 0x52D770): the HandFood pot holds min(amountPickedUpInitially, GetFoodValue(3)) and goes into the hand at 0x52D840.

### Water queries

The original's positions are `MapCoords`: x and z in cell 16.16 (world × 6553.6 truncated, 10 world units
per cell) and y = height **above the ground**, not world altitude. The port keeps that detail inside the module and
exposes `glm::vec3` in world units.

The table root, `hypotenuse` and `GetDistanceInMetres` are not copied in
`WaterQueries.cpp`: they come from `src/Common/GUtilsDistance.{h,cpp}` (`openblack::gutils`), in float and not in double, see
[GUtils distances](../engine/coordinates-and-angles.md#gutils-distances).

| Function | Address | What it does |
|---|---|---|
| `GUtils::GetDistanceInMetres` | 0x74CD70 | xz distance of two `MapCoords`: `hypotenuse(int,int)` 0x74F680 (16.16) × 10/65536 |
| `hypotenuse` / inverse root | 0x74F680 / 0x74F620 | inverse root by a table of 1024 mantissas (`crt_xc_fn_atexitCleanupReg_Utils_0074F580`): ~10 bits, error ≈ 0.1 % |
| `GUtils::FindNearestCoastalTo` | 0x74E2E0 | square spiral of cells from the point until finding a cell **inside the game map** and `MapCoords::IsCoastal`, or until the spiral moves further away than the radius (max. 999999 steps) |
| `GUtils::Spiral` | 0x74D7E0 | the spiral step: table 0xDA59FC = (+1.0), (0,+1), (−1.0), (0,−1), with `dir/2` steps per stretch; it adds **to the high word**, so the cell fraction of the starting point is not lost (`operator+=` 0x605470) |
| `GStream::FindNearestPosTo` | 0x733D30 | the river point (`CREATE_STREAM_POINT`) closest in xz, **strictly** closer than the radius; the y of the result is the point's altitude minus the ground there |
| `GUtils::FindNearestDrinkingWater` | 0x74E3A0 | first the river; if there is one, it also looks for a coast that is no further **from the starting point** than that river (if it finds one, the coast wins) and returns "found" in any case; without a river, the coast within the radius |
| `Abode::FindNearestDrinkingWater` | 0x407020 | the above from the house into its cache: bit 0 of +0x7C = found, +0x80 = the position (**it only changes if it finds something**) |
| `Abode::GetNearestWaterPos` | 0x405FC0 | reads that cache (false if the bit is 0) |

The original's radii: **200** when creating the house (`Abode::Abode` 0x4013E0), **400** the shepherd when the house does not yet have
water (`Villager::ShepherdMoveFlockToWater` 0x768CC0), **500** the tiger (`Tiger::CalculeLairPos` 0x4214DD).

Details that matter:

- The spiral **stops as soon as it moves beyond the radius**, it does not cover the whole square: that is why it does not always return the
  strictly closest coast, but the first one in the spiral.
- `IsCoastal` (0x6036A0) is **land** on the coastline (`!hasWater && coastLine`), not water: the drinking water is a
  land cell next to the sea, and the drinker approaches it. The predicates are in `ECS/SeaCells`.
- `Tiger::CalculeLairPos` (0x421470) calls `FindNearestDrinkingWater(pos, 500)` **for each forest** but does not use the
  result to place the lair: the answer only sets the distance it compares with the forest's score
  (`fn_0053AD00`, `SigmoidThreshold` of trees and distance), and that distance is not read afterwards. The lair is still
  the best-scored forest, so `ECS/AnimalPredators.cpp` does not change (it only notes it).
- `GStream::FindNearestPosTo` walks the streams of [g_game +0x205C5C] newest first (the GStream ctor pushes at the head, 0x733A8E..0x733AA0), and each stream's points (+0x14, next +0xC) in script order. For two points at exactly the same distance the first one found wins.
- The coast spiral's 999999-step counter is set at 0x74E2FF.

- 0x74E2FF
- The streams of [g_game+0x205C5C] (newest first: the GStream ctor pushes at the head, 0x733A8E..0x733AA0) and their points (+0x14, next +0xC, script order). entt walks a storage from its last element to its first, so the view also gives the newest stream first (streams are only removed all together, with the map). The order only matters for two points at exactly the same distance: the first one found wins.

### The creature's `LandAvoid` mask

`LandAvoid` (0xD559B0) is 512×512 bytes `[z][x]`, one per terrain cell, built **once per landscape**
in `GLandscape::Open` (0x5E5541) with `ValidateLandAvoid` (0x6E7BA0) and `FloodAnalyse` (0x6E7FA0, `RoutePlan.cpp`):

1. For each cell, the altitudes of its **four corners** (`(x,z)`, `(x,z+1)`, `(x+1,z)`, `(x+1,z+1)`; 0 outside the
   map or with no block) × 0.67: if `max − min > 10` → **1** (too steep); if all four are 0 → **1** (deep
   sea); otherwise → **4** (walkable candidate).
2. The flood seed is the last 4 cell of the last run of 4 cells that ends in a 1 (by rows: the
   counter is reset on each row; a run that reaches the end of the row does not count). In Land1 it comes out as **(191, 362)**.
3. `FloodAnalyse` floods the 4 neighbours ((−1.0), (0,−1), (+1.0), (0,+1)): the reached 4s become **0** and the 1s
   neighbouring the flood become 5. At the end 1 → **2**, 4 → **2** (unreachable) and 5 → **1**.
4. The 0 cells whose terrain has the water bit (or has no block) become **6**: water that can be walked through.

The flood marks: 3 queued (FloodAnalyse 0x6E802C / 0x6E80CB), 5 for a 1 next to the flood (0x6E80D1), reset at
0x6E810D; the seed rule of step 2 is at 0x6E7EC4; a reached cell that is water, off the map or without a block is
walkable water (0x6E7F18).

Final values: **0** reachable land, **6** walkable water, **1** cell to avoid next to the reachable area, **2**
unreachable. `fn_00483890(pos, r)` (r = 7.1 in `fn_00483850`; 7.05 in `fn_00483870`, turning) accepts the position if its
cell is 0 or 6 and no cell centre `(10i+5, 10j+5)` of the neighbouring 3×3 that is not 0/6 lies closer than r.

The creature therefore **wades** as far as where all four corners of the cell have altitude 0 and does not swim (in
`ctrspec27.txt` there is no swimming animation). The Land1 lakes (altitude 1 with water) also come out as **6**: they can be
entered.

**Hook** `OPENBLACK_DUMP_LAND_AVOID=1` (or `=<file>.png`) dumps the mask when loading the landscape: one pixel per
cell, x to the right and z downwards, **green** 0, **blue** 6, **red** 1, **grey** 2 (magenta = impossible value), and
writes the seed and the counts to the log. Checked with the Land1 points: `(146,201)` open sea → red (deep
sea next to the reachable area), `(147-148,201)` shallow coast → blue, `(149,201)` and `(178,271)` land → green,
the lakes `(213,241)` and `(216,309)` → blue.

### Water in the scripts (CHL)

- **`GET_LAND_HEIGHT`** (`GScript::GetLandHeight` 0x6FB1F0): cell `(int)(x·0.1)`, `(int)(z·0.1)` (truncated towards 0, so
  −10 < x < 0 is still cell 0); outside 0..511, with no block or **altitude 0** it returns **−10.0**
  (0xC1200000, "it is in the sea"); otherwise, the interpolated height (`LH3DIsland::GetAltitude`). It is used by
  the challenges `Baywatch` (Land2) and `LostBrother` (Land1). Checked: Land1 open sea (1464, 2016) → −10, dry land
  (1788.4; 2710) → 28.917.
- **`GET_PROPERTY`** (`GScript::GetProperty` 0x70DAE0, jump table 0x70E78C on `property − 1`): for now the
  two water ones are there, the others are still unimplemented.
  - `FLYING` (5, 0x70DCF8) = bit 6 (0x40) of `Object+0x24` = **the object has a `PhysicsObject`** (also the
    obstacles at rest, just like `Object::IsActuallyInTheAir` 0x639410).
  - `DROWNING` (6, 0x70DD0A) = the virtual `IsDrowning` (vt +0x17C): `Villager` 0x756B30 = state **16** DROWNING;
    `Object` 0x63A780 = has a `PhysicsObject` **and** the centre of mass (po +0xCC) is below y = 0;
    `GameThingWithPos` 0x4052D0 = 0. It is used by the challenges
    `CreatureSavingPeopleDrowningMan` and `PiperSetFree` (Land1) and `ThrowBlokeMain` / `EndOfFlyingNutter` (Land3).
  - With a thing that no longer exists the original writes "Thing no longer valid" and pushes a **float 0**;
- **`SET_PROPERTY`** (0x70F380 → fn_0070E820, table 0x70F2BC): `FLYING`, `DROWNING` and `MOVING` go to the error case
  ("Cannot Set Property %d") and **change nothing**; ported that way.
- **Enum fixed**: `ObjectPropertyType` in `src/ScriptHeaders/ScriptEnums.h` merged two entries into
  `InHandGrabypeSpeed`, so from `IN_HAND_GRAB` (10) onwards everything was shifted by one value (`SPEED` 11 … `BUILT_PERCENTAGE`
  22, `ZPOS` 25). Now they are `InHandGrab` and `Speed` as in `bw1-decomp include/chlasm/ScriptEnums.h`. Nobody used the
  numbers before (checked with grep), so the fix breaks nothing.

### Impacts and objects that fall into the water

- **Impact against the water** (`AttemptToAddSoundEvent` 0x6465B7): `!IsDryLand` (altitude < 4) → **ring** at (x; 0.1; z)
  with growth 2R, rate 1/R, cell **0x3F**, 0xFFFFFFFF; also, if in the rounded cell there is no cell or the
  altitude is < 3 → collision type WATER, foam dust 0x28C8F0F4 and `fn_0074F2D0` (which **only** sets the
  global splash flag that scares the fish: there is no spell file effect, the 6 foam "particles" are the
  normal dust of `fn_00845C20`). Altitude 3 = shore: ring + brown dust with the ground sound.
  - The 6 particles (0x646776..0x646854): at (x, `GetAltitude`, z), velocity
    (rand(201) − 100)·0.02 per axis, size min(2R, 5), type 4 of the "liquid particles" (`fn_00845D30`, quota 0x400;
    `fn_00846010`: type 4 = 1 s without gravity, type 0 = 3 s, others 2 s; they are removed when the age **exceeds** the life,
    before moving). The colour first goes through `fn_004ED180`: k = clamp(ftol(snow at the point), 0, 255) with the
    `SnowCover` grid [0xEDC344] (128×128, 40 units per cell, bilinear, `fn_0086CA80`) and each channel
    c += floor((base − c)·k/256) towards the base colour of the light [0xFA26A4], alpha the same. Without snow k = 0 and the colour does not
    change;
- **Wave when bobbing in the water** (0x645A5E): same ring but **cell 0x30** (the one of the hand splash), not 0x3F.
- **Dropping over the water** (`Object::InitialisePhysicsFromHand` 0x636F00, fully ported; the algorithm is in
  [Water in impacts and when dropping](../physics/physics-impacts.md#water-in-impacts-and-when-dropping)): it only "lands" with `IsDryLand` or with the altitude of the
  rounded cell (fistp) > 1, so over the sea the object stays in physics and floats or sinks
  ([below](#sinking-drowning-and-being-deleted)); a hand pot dropped slowly loses its resource (next point).
  Buoyancy (density that rises 6.67e-5 per substep, drag ×100, deletion below −4R) is in
  [Engine](../physics/physics.md#engine). ✅ faithful
- **Resources that fall into the sea** (`Pot::AddResourceToPos` 0x66F270): outside the map it does nothing, and what is left after
  merging with piles/storehouses **is lost** if the cell is water (0x66F42D): neither a new pile nor a sound.

- UpdateRuleGravityWithFloor::AtomDataRipple (fn_006A1630, from ModifyAtomCollection 0x6A1CC2), the ripple of a particle that touches the floor, once the rule's own tests passed (alpha >= rule+0x6C, an ImpactSound, the speed against rule+0x60, ImpactSoundCondition, the ripple flag rule+0x71 = 1): only where MapCoords(position).IsWater(), and only when the particle is farther than minDistance (rule+0x40) in x, z from its last ripple, which is then moved there (lastRipple: the atom's AtomDataRipple +0x20, (0, 0, 0) when it is made). The ring: at the particle, growth 4 x Object::GetRadius of the atom, cell 0x30, colour 0xFFFFFFFF, angle 0, rate and aspect 1 (nothing when the pool is full). Returns true when a ring was added.

### Sinking, drowning and being deleted

**Faithful** except what is marked pending (the villager's complete death, see [Pending](#pending)).

- **`HasSunk`** (vt +0x7B8), asked on every substep from 0x645A01 when the body is awake, its centre is
  below `R/2` and its **density > 1**; if it says yes, the body stops (v = 0, L = 0) and `EndPhysics` runs
  as if it had come to rest (code 2):
  - `Object::HasSunk` 0x637470 → **no**: rocks, trees, pots, piles and pieces keep going down until `T.y < −4R`
    (code 4) and there they are **deleted** with their class's `ToBeDeleted(0)`. Measured times: rock immediately, villager ~4
    turns, offering pot ~64, animal ~75, normal object ~150, tree ~194, pot ~298, ball ~525.
    While going down they are drawn whole with their normal Draw, after the land: the part below y = 0 is hidden by the
    Z of the drawn cells (which write it even though they are transparent) and is visible over the open-sea 0x02 cells
    (which are not drawn). A tree that sinks next to those cells comes out **cut into rectangles**, also in the
    original ([Coast](sea-coast-and-rivers.md#coast)); the user remembers it that way (2026-10-01): a tree or a rock
    thrown into the sea looked cut off.
  - `Living::HasSunk` 0x5ED370 (animals) → `SetDying`, state LIVING_DEAD 15 and `ToBeDeleted(0)`: the animal disappears.
  - `Villager::HasSunk` 0x750AB0 → `stateCounter = GVillagerInfo::drowningTime` (**600** turns = 60 s) and state
    **DROWNING (16)**. Not available (FINAL 14 DYING, 0x750AB5) → not sunk. With the dead status bit (+0xB4 & 1, a
    corpse): state DYING 14 with `dyingTimeWithoutGraveyard` (`VillagerDrowning.cpp`, [Villagers: data and states](../beings/villager-states.md), Death).
  - Both `Living`s first notify the creature so that it learns from the player who dropped it
    (`ConsiderMakingCreatureMimicPlayer`, `DETECTED_PLAYER_ACTION_THROW_IN_THE_SEA` 0x15), after the availability test.

    the animal's waits for a last dropper of animals ([Mimicry and the town hooks](../creature/mimicry.md#mimicry-and-the-town-hooks)).
- **`Villager::EndPhysics` 0x5F0A60, water branch** (0x5F0BAF): every villager that **ends physics in a cell with the
  water bit** drowns, also on the shallow shore; there is no LANDED. With life > 0 → DROWNING with `drowningTime`
  (and `lastPlayerToInteract` = whoever threw it, pending); otherwise, `VillagerDead(6 PLAYER_INTERACTION_DROWN)`. This is reached
  through `HasSunk` (the normal case in the sea) or when stopping with contacts in a water cell of altitude ≥ 2; in Land1 **there is no
  water cell with altitude > 1** (checked by walking the map in `test_sea_cells`), so on that island
  it is always reached through `HasSunk`.
- **DROWNING state (16)**, `Villager::Drowning` 0x76A780, once per turn: `--stateCounter` and, at 0, death with
  reason 6. Default clip of the state **252 `P_DROWNING`** (2233 ms loop) and its sound events: 30 ms and 1590 ms
  swimming splash 157 (`editor.sad` 559-562), 257 ms drowning voice 134 (man 563-570, woman 571-578, the child does not
  scream) — they already play because `AnimationSounds` resolves surface 7 (water) of the clip.
  `EnterDrowning` 0x767410 / `ExitDrowning` 0x767420 return 1 (they do nothing).
  - **Runs every turn**: `GGame::ProcessTurn` → `Living::ProcessLiving` 0x5EC810 → `ProcessState` (vt +0x620,
    `Villager::ProcessState` 0x74FF70) → `CallState` 0x7521D0. `GVillagerInfo::processChecksEvery` (+0x2DC) only
    spaces out the periodic block of `CheckEveryTime` (0x750518: old age, hunger…), not the state function.
  - **Bit 0x4000 of `Flags` +0x24 = INDESTRUCTIBLE**: it is set and cleared by `SET_INDESTRUCTABLE` (`GScript::
    SetIndestructable` 0x6FDE20, objects that are not script containers), the puzzles (PuzzleGame, HanoiBlock,
    PuzzlePig) and `GameOSFile::LoadInstance`. `Drowning` 0x76A783 sets the counter to 10 before decrementing it: **an
    indestructible villager never drowns**.
  - **`lastPlayerToInteract` (+0x104)** = `PhysicsObject::GetPlayer` 0x647460 (the player of the `GInterfaceStatus` of
    po+0x24: the hand that dropped or threw it, inherited by what it hits; 0 without physics), set in the water branch of
    `EndPhysics`. It is only read by the `VillagerDead` of `Drowning` (the villager's `GetPlayerWhoLastDroppedMe` is the one from
    `GameThing`, which returns NULL, 0x4018B0).
  - **Rescuing with the hand**: picking it up puts it IN_HAND (the drowning function stops running) and dropping it on dry land
    takes it out of physics on the spot → `Villager::EndPhysics` without `IsWater` → LANDED. After LANDED it only goes back to the
    previous state (+0x8E) if it has the 0x400 flag (script-controlled) or if that state has +0x104 in its
    `GVillagerStateTableInfo` (file +0xF4): only `InScript` (4) and `In Script Dance` (5); DROWNING does not. It stays
    rescued.
- **Clips for dying in the water**: `DyingAnimation` 0x423770 → **283 `P_INTO_DEAD_DROWNED`** if `IsWater(Pos)`, and
  `DeadAnimation` 0x4237A0 → **249 `P_DEAD_DROWNED`** (`VillagerAnimate`); on land 253 and 243.
- Common **`ToBeDeleted`** (`ECS/ToBeDeleted`): the clean-up of each class before removing the entity — villager
  (`Villager::DeleteDependancys` 0x74FD60: house and homeless list), animal (`Animal::DeleteDependancys` 0x417BA0:
  the AI forgets it), and then out of physics and of the registry. It is used by the deletion at −4R (previously a direct
  `registry.Destroy`), the sinking of animals and the death of the drowned.
- **Scripts**: `GET_PROPERTY(DROWNING)` (and `FLYING`) now answer with `ecs::IsDrowning`; see
  [Water in the scripts](#water-in-the-scripts-chl).

### Water rings

- **Faithful** (`fn_005E5100`, after the land and before the models): per ring, age += (int)(game
  ms · rate), removed at 700; half width max(age·growth/700, 0.0001) (z × aspect, +0x28); alpha (int)((255 − 0.364286·
  (age % 700))·A) >> 8, RGB from the colour; rotation in Y, cell & 63 of the 8×8 sheet of `smoke.raw`/`smokea.raw`, mode 13
  (SRCALPHA/ONE, no Z); drifts with the wind if +0x1C. Hand splash: (x, 0.2, z), growth 7, random angle,
  cell 0x30, 0xB0 + light table[255]. Physics object in the water (0x6466D2): (x, 0.1, z), growth 2·radius, rate
  1/radius, cell 0x3F, white. The colour +0x34 is fixed **on creating** the
  ring (table[255] of that frame for the hand and the waterfall, table[200] for the rain): `WaterRingSystem::Add` resolves
  `seaLight` once and the drawing uses +0x34 as is, so a ring made at nightfall or during a lightning flash does not change.
  Particle rings (`Particles/PSysWaterRings`, called by the rules of
  [The PSys in the world: format, step, drawing and water rules](../physics/particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules)): explosion
  (`UR_Explosion` 0x67E347: in water or altitude < 4 three rings of growth 5, 7 and 10, cell 0x30, white, at
  (x, altitude, z); on dry land the scorch mark 0x251) and particle ripple (`fn_006A1630`: only in water and more than 2
  (rule+0x40) in xz from the last one, growth 4·atom radius, on the ground).
- The original's quota: 1024 rings of 0x38 bytes at 0xEAB7C8. They are created by the hand splash, the objects that fall
  into the water ([above](#impacts-and-objects-that-fall-into-the-water)), the wake of the [sharks](sea-life-and-scenery.md#sharks), the
  foot of the [waterfall](sea-life-and-scenery.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur), the fish of the
  [puzzle](sea-life-and-scenery.md#fish-puzzle), the rain and the particles; the swimmers (SuperVillagers) do not exist in openblack yet.
  The water that scares the fish: [Fish-farm fish shoals](sea-coast-and-rivers.md#fish-farm-fish-shoals).
- The ring count is at 0xEB9AB8 (next to the 1024 × 0x38 pool at 0xEAB7C8).
- The hand splash's random ring angle (CRT Random(0, 2π), 0x5D2057) is drawn before the ring pool is searched (0x5D205C), even when the pool is full.
- The pool is cleared only when the game is made: `GWater::InitialiseCircles` 0x54BA40 (zeroes the 1024 slots, sets +0x18 = 0.05 in each, creates `g_sprite_circle` 0xEB9AB4 and sets the count 0xEB9AB8 to 0), whose only caller is the `GGame` constructor 0x54B240 (0x54B663; the constructor is reached from `start_system` 0x6432CB and from the multiplayer lobby's restart 0x553377). `GGame::ClearMap` 0x552BB0 and `StartPlaygroundGame` 0x552F40 do not touch the pool or its count (no other write to 0xEB9AB8 but the adders' `++` and the drawing's `--` at 0x5E517A when a ring reaches its life), so the rings alive at a land change (LOAD_MAP) live on into the new land at their old positions and fade out there.

### Water in other pages

- Drawing of the sea (rows, undulation, drift, what is under the sea), the coast and its alpha, the rivers and the glow of the
  hand at night over the water: [Sea](sea-coast-and-rivers.md#sea),
  [Coast](sea-coast-and-rivers.md#coast), [Rivers](sea-coast-and-rivers.md#rivers), [Sky](sky.md#sky-sun-moon-and-clouds-original).
- Reflections of the hand, the objects, the creature and the boats; the underwater cuts (`DrawCutByPlane`); the fish
  shoals of the fish farms. Everything in the water zone that is drawn under the sea
  (`RendererBoat`, `RendererFishPlot`, `RendererCut`, the fish and the reflected moon) goes through `graphics::sea_pass`
  (plane, mirror, face and light in a single place):
  [The under-sea pass](sea-coast-and-rivers.md#the-under-sea-pass),
  [Object reflections and hand shadow on objects](../rendering/shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects),
  [DrawCutByPlane](../rendering/meshes-and-lod.md#cutting-by-the-water-plane),
  [fish shoals](sea-coast-and-rivers.md#fish-farm-fish-shoals).
- Creation of the fish farms from the script:
  [Fish farms](../scripts/map-script-reader.md#fish-farms).
- Buoyancy and sinking in the physics engine: [Engine](../physics/physics.md#engine); dropping from
  the hand: [Water in impacts and when dropping](../physics/physics-impacts.md#water-in-impacts-and-when-dropping).
- Shark routes (`WALK_PATH`, `camera.edt` tracks): [The script camera and camera tracks](../camera/script-camera.md).
- Particles over the water (bounce, ripples, explosion steam): [The PSys in the world: format, step, drawing and water rules](../physics/particles.md#the-psys-in-the-world-format-step-drawing-and-water-rules).
- Animals in the sea: [Hand, flight and death](../beings/animal-life.md#hand-flight-and-death); leaving something over the sea with the hand:
  [The hand: animation and placement](../hand/animation-and-placement.md).

### Notes from the code

These notes on the original fit no section above yet.

- 0x74E3D6: found, whatever the coast search says
- 0x6A176F..0x6A179B: minDistance^2 < dx^2 + dz^2
- 0x6A17CE

### Addresses

The code's comments describe what it does in plain words; the original's addresses and function names they used to quote are kept here, next to the openblack symbol each one corresponds to.

| Address or name | What it is | openblack |
|---|---|---|
| `0x750AC1..0x750AED` | Villager::HasSunk's creature-learning part (ConsiderMakingCreatureMimicPlayer, THROW_IN_THE_SEA 21, for the player who last dropped it) | `ecs::HasSunk` → `creature_mimic::ConsiderThrownInTheSea` ([Mimicry and the town hooks](../creature/mimicry.md#mimicry-and-the-town-hooks)) |
| `0x750AF5..0x750B0F, 0x750B1E` | Villager::HasSunk's dead branch: status bit +0xB4 & 1 (not Living::IsDead) -> SetTopState(14 DYING), counter = dyingTimeWithoutGraveyard (+0x290) | `ecs::HasSunk` |
| `0x5F0C9D` | end of Villager::EndPhysics' water branch (0x5F0BAF-0x5F0C9D) | `ecs::VillagerEndPhysicsInWater` |
| `0x5F0BC0..0x5F0C41` | Villager::EndPhysics in water at 0 life: Living::IsDead (vt +0xAF4) -> DYING with dyingTimeWithoutGraveyard, else counter 0 and VillagerDead with the physics' player or the neutral player (g_game +0x205A5B) | `ecs::VillagerEndPhysicsInWater` |
| `po +0xCC (y of the +0xC8 point)` | Object::IsDrowning reads the physics body's centre of mass y at PhysicsObject +0xCC; flag 0x40 of Object +0x24 = has a PhysicsObject | `ecs::IsDrowning` |
| `0xFA9314` | sea ripple sine table, 16 entries sin(i pi / 8), row r uses phase (frame + 2 (r + 1)) & 15 | `fs_water.sc RowUv` |
| `0xC39904 / 0xC39900 / 0xC39908` | sea row alpha constants: 255 up to view depth 7000, down to 80 at 14000 (rounded by fistp) | `fs_water.sc RowAlpha` |
| `[ebp-8] += [ebp-0x30] (in fn_00879930)` | sea rows' 1 / depth stepped per row, affine in screen y from the range vertices | `fs_water.sc RowUv (u_seaRows.z / .w)` |
| `0x84A390 / 0x84A3D0` | CRT initialisers fn_0084A380 / fn_0084A3C0 setting LH3D's user plane default (0, 1, 0, 0) | `sea_plane.sh u_objectClip` |
| `0x85113C..0x851149` | fn_00850FC0 (DrawUnderWater): code 0x800, "d > 0 out" on the mirrored point | `sea_plane.sh SeaPlaneDiscard, sea_pass::Kept` |


### Sources

- `bw1-decomp` (`src/Black/Object.cpp`, `include/chlasm/ScriptEnums.h`).

## openblack

Status: ✅ faithful except what [Pending](#pending) lists. Progress rows: [ocean](../project/progress/ocean/)
(the sea surface and reflections 100%, the shore 90%, things on the water 87%) and
[water physics](../project/progress/physics/water_physics.md) (96%).

- `src/ECS/SeaCells.*`: the cell predicates; `src/ECS/WaterQueries.*`: the queries (with `src/Common/GUtilsDistance.*`);
  `src/ECS/WaterRings.h` and `src/PSys/PSysWaterRings.cpp`: the rings;
- `src/ECS/VillagerDrowning.{h,cpp}` and `src/ECS/ToBeDeleted.{h,cpp}`: sinking, drowning and deletion (✅ faithful
  except the villager's complete death, see [Pending](#pending));
- water state lives in components and the systems are reached through Locator services; the cell predicates and the
  queries are pure functions tested with fakes (`test_sea_cells`, `test_water_queries`, `test_psys_water`; a test
  that needs the real `Land1.lnd` skips itself without it).

Where it is in the code:

- `src/ECS/WaterQueries.cpp`: `NearestStreamPos` walks the streams newest first (entt walks a storage from its last
  element, as the original's list is built at the head), so of two points at exactly the same distance the first found
  wins; `NearestDrinkingWater` returns found as soon as a stream point is found, whatever the coast search says.
- `src/ECS/FishFarms.{h,cpp}`: `IsFishFarm` (a valid entity with a `FishFarm`), `TownFishFarms` (the town's fish farms,
  newest first, by creation index from high to low; none for a null town), `FishermanCount` (the farm's fishermen,
  the bound of the fishing job's random draw) and `OnFishFarmDestroyed` (entt's destroy signal for `FishFarm`, which
  removes what depends on the farm while the component is still there).

Porting decisions taken from raffclar's tree (the moon in the sea and the reflections):
[Port decisions: engine and world](../project/port-decisions-engine-world.md#world).

#### The creature's `LandAvoid` mask

- In openblack: `land_avoid::Validate(island)` is called when loading the landscape (`Game::LoadLandscape`), `At(x, z)` gives the value and `IsPosValid(pos, radius)` is `fn_00483890`. Count in Land1: 12 949 cells 0, **1866** cells 6 (the ring of shallow water and the lakes), 3671 cells 1 and 243 658 cells 2.

#### Water in the scripts (CHL)

- openblack always returned the height: now `sea_cells::ScriptLandHeight` applies the full rule (`CHLApi.cpp`, `GET_LAND_HEIGHT` 151).
- In openblack it is `ecs::IsDrowning` (`ECS/VillagerDrowning`).
- openblack does the same (it often appears in the log because the scripts reference objects that openblack does not create yet).

#### Impacts and objects that fall into the water

- openblack `DynamicsSystem::AddPuff`
- openblack has no `SnowCover` grid here, so it is not applied.
- openblack: a single rule, `ecs::pot_resource::AddResourceToPos` (`ECS/PotResource.cpp`, exact to 0x66F270, see [The spell core](../magic/spell-core.md)); the hand (`HandResources.cpp`) no longer has its own rule and `pot_resource::IsWater` calls `ecs::sea_cells::IsWater` (the only copy of `MapCoords::IsWater` 0x6035B0).

#### Sinking, drowning and being deleted

- openblack publishes the villager's (`creature_mimic::ConsiderThrownInTheSea`, the player of `VillagerLastInteraction`)
- openblack used `po.body.inWater` (physics) instead of `IsWater(Pos)` (the cell).
- openblack: `Indestructible` component and `SET_INDESTRUCTABLE` in `CHLApi.cpp`.
- openblack does not yet have players to store it in.

#### Water rings

- openblack: `ecs/WaterRings`, `Renderer::DrawWaterRings`; hook `OPENBLACK_TEST_SPLASH="x,z"` (one splash per second; the rings only advance with the game running).
- openblack does the same: nothing calls `WaterRingSystem::Reset` when a land opens (`Game::LoadMap`).

### Test hooks

All in [Debug environment variables](../project/debugging.md#debug-environment-variables) (and the physics ones in
[Test hooks](../physics/physics.md#test-hooks)):

- `OPENBLACK_DUMP_LAND_AVOID=1` (or `=<file>.png`): the `LandAvoid` mask (colours [above](#the-creatures-landavoid-mask)).
- `OPENBLACK_TEST_SEA="x,z,type[,height]"` (+ `OPENBLACK_PHYSICS_TRACE=1`): an object in the water that floats, sinks or
  drowns; with `OPENBLACK_TEST_CUT=1` its part under the water is also drawn cut.
- `OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"`: the hand gently drops something at (x, z), e.g. a villager in the sea
  (1464; 2016).
- `OPENBLACK_TEST_SPLASH="x,z"`: one hand splash per second (rings, sound, fish scare).
- The sharks, the fish puzzle, the boat and the fixed scenery: [Sea life and fixed scenery](sea-life-and-scenery.md#test-hooks).
- `OPENBLACK_AUDIO_TRACE=1` (channels) and `OPENBLACK_ATMOS_TRACE=<n>` (ambience).
- Tests: `test_sea_cells`, `test_water_queries` and `test_psys_water`.

`test_water_queries` (real Land1, see [Tests and test data](../project/debugging.md#tests-and-test-data)):
mask consistent with the cell predicates, nearest coast (and that the spiral keeps the cell fraction),
nearest river point and its y, drinking water no further than the river, the house's cache and distance with the original's
inverse root.

- Shots of the water (and of land close to the camera) can differ between two runs by a few units in a few hundred
  pixels while the state hash and the random trace are identical. Explained: the GPU driver (Vulkan on this machine's
  integrated GPU); the same runs on Direct3D 12 are pixel-identical. The checks stay on Vulkan, and the scenario script
  reports off-by-1 water alone as exit 4 (docs/refactor/SCENARIOS.md).

## Pending


- Complete villager death (`VillagerDead` 0x7506C0) and the creature's mimicry when something dropped by the
  player sinks: in [Pending](#pending).
- The boat's deck light: the original copies the hull's +0x4C colour to each passenger; openblack lights each
  instance by its own position (queued fix).
- The fish shoals' flee time: the original's `fn_008248E0` deducts it with the unclamped dt and computes the push
  after subtracting it (0x82490D..0x82495A); openblack uses the clamped dt and computes the push before subtracting
  (queued fix).
- Foam of impacts against the water: the tint towards the base light according to the snow is `fn_004ED180` (the
  wiki named fn_0074F2D0, which is FishRush's emergency point). The sea's landing foam (0x28C8F0F4) goes through it in
  the same loop as the land's dust, and both are ported (physics.md, "The snow tint").
- Check that the sound ambience and its group by alignment now read the real values (`CameraWeather` →
  `weather::atmos::GetWeatherSmooth`, `atmos_banks::Alignment` → `Clouds::InfluentialPlayerAlignment`) and not 0.
- Screenshots still to be seen: the moon reflected in the sea; that the food pots and the orbs still animate their
  texture with the material's bit 0x10 gate (two frames need comparing); the rings of the rain
  (`water_drop_cb` 0x54EEA0) and of the water miracle (`SpellWater`) over the sea.
- `GET_PROPERTY` / `SET_PROPERTY`: only `FLYING` and `DROWNING`; the other properties are still unimplemented (they are not
  water-related).
- Villagers: drinking (`FindNearestDrinkingWater`, `CREATE_DRINK_WAYPOINT`), the shepherd who takes the flock
  to the water (`ShepherdMoveFlockToWater` 0x768CC0), `Villager::CreateDroppedResource` and reaction 9
  `REACT_TO_FLYING_OBJECT` when dropping with the hand. The complete death is in [Villagers: data and states](../beings/villager-states.md) (Death): `VillagerDead`
  0x7506C0 with reason 6, the corpse in the water (dying clip 283, dead clip 249), smoke and skeleton 0x1FF without a
  soul; out of the water the soul of `fn_00828790` (the child one `GVillagerInfo`+0x204 below age +0x138, the adult one
  GetMesh 0x74F880 = +0x214 StdDetail) plays P_DEAD1/2_GOTO_HEAVEN or _HELL, 244/245 or 247/248.
- `lastPlayerToInteract` (+0x104) and `GetPlayerWhoLastDroppedMe`: with a single player, the hand gives PLAYER_ONE
  🔍 inferred until there are several players.
- SuperVillager swimmers (`M_P_Swim2`, `DrawCutByPlane` and their ring every 1000 ms): they need the Land 1-2 scripts
  (Baywatch, FollowUs).
- `WALK_PATH` of the Livings (0x5EE100, `Living::MoveAlongPath` 0x5EE230): needs the villagers' paths (footpaths).
- Fish puzzle: the fisherman and the challenge scroll (`FishPuzzle.txt`); the rest of the `PuzzleGame` types.
- The water rings, the fish farm fish and the boat sprites already go through `graphics::billboard`
  and `graphics::frame_anim` (same cells and formulas, no blending between frames). The moon (sign of the
  tilt and of the phase, halo V 0xEDC304) is done, and the fish cell (8 + (ftol(frame) & 15),
  taken before the wrap, dt ≤ 0.1 s, `fn_008248E0`) too. The morphing of the ark and the dinosaur to the ground
  (`UpdateMelting` 0x5E3C55 / 0x5E3DBE) is `MorphWithTerrain` in `ECS/DesignedScenery`.
- Audio (**done**, [The audio engine](../audio/engine.md)): the sounds that did not go through the 16 channels
  (AnimationSounds, rocks, the camera whoosh, `G_RockPast` and the hand's `PlaySample`s) and the street lanterns
  as `SoundTags`; the audio does not read the ECS and `audio::GetSurfaceType` no longer exists (everyone uses
  `sea_cells::GetSurfaceType`).
- Was the sea still with the camera stopped? The code says yes (ambient wind 0, no drift).
- The horizon step: about 25 px of mirrored sky above the edge of the sea's 30000 square.
- The moon reflected in the sea and the warm patch of the hand at night over the water (120 × 120).
- A villager dropped gently on the shallow shore also drowns (60 s of the `P_DROWNING` clip), and can be rescued with the
  hand: was it like that?
- The Land 3 waterfall: only the water flows (material bit 0x10); were the rings at the foot visible?
- The beached ark of Land 4 carries the `G_WaterFlow` loop: did it sound like water?
- Calibrate by ear: the hand splash at 40 / 100 / 150 units and the sea, the coast and the loose waves.
- A fireball that falls into the sea: three thin rings and then white steam?
- The launch of the boat: density of the sand dust and volume of the creaks.

Everything else about water is done and faithful to the original.

**Can be done now (water area):**

**Blocked by other areas:**

**Done through the shared graphics and audio APIs:**

**Questions only the user can answer** (memory of the original):

Confirmed by the user (2026-10-01): a tree or a rock thrown into the sea looked cut off; the mottling of the shore
may come from the texture pack's `smallbump.raw`; and everything is preferred as in the original.
