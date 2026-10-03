# Physics: thrown objects, collisions, damage and rocks that split

Code: `src/ECS/Physics/` (`PhysOb` = rigid body, `PhysicsObjects` = per-turn manager, `FromHand` = what a released
object does), `src/ECS/Rocks.*`, `src/ECS/Components/Life.h`, and the hand part in `HandPhysics.cpp`. Full reports with pseudo-C++ and
addresses in `C:\Users\diewgarc\dev\documentacion\physics\` (`physob.md`, `physicsobject.md`, `physob_bodies.md`,
`rock_split.md`). Bullet is not used for this (openblack only uses it for ray casting).

- [Engine (PhysOb)](#engine-physob-0x7fb7300x7fe7b0)
- [Bodies](#bodies)
- [Manager](#manager-physicsobjectgameturnupdate-0x644fc0)
- [Buildings that break](#buildings-that-break-abodereacttophysicsimpact-0x406240-fragmesh-0x7f6f00)
- [Sounds, dust and the look of impacts](#sounds-dust-and-the-look-of-impacts)
- [Water in impacts and when dropping](#water-in-impacts-and-when-dropping)
- [Rocks that split](#rocks-that-split-rocksplitintwo-0x6e7560)
- [Who owns what: class handlers and the hand's API](#who-owns-what-class-handlers-and-the-hands-api)
- [Pending](#pending), [Test hooks](#test-hooks), [Sources](#sources)

Status: the engine and what is described here is **faithful** (ported from the original) except what is marked and what is in
[Pending](#pending). Everything about water that is not physics (which cell is water, the impact against the water, sinking and
drowning) is in [water.md](water.md).

## Engine (PhysOb, 0x7FB730..0x7FE7B0)

- **Penalty contacts** on a cloud of vertices (point masses): each vertex below the ground or inside
  another body receives a normal spring and a Coulomb friction anchor. No impulses and no restitution coefficient: the
  bounce comes from the spring and the damping of probing the vertices **0.06 s ahead** (`x + v·0,06`).
- **dt 0.005 s, 20 substeps per 0.1 s turn**, semi-implicit Euler. Gravity 9.81; max. speed 124 (the same as the
  hand throw); max. ω 3π. No linear damping; the angular one keeps `d4` per second.
- The original accumulates torque as F×r and rotates the rows the other way round; the two inversions cancel out. The port uses r×F and
  columns (same motion). The inertia tensor keeps the original's bug (`I[1][2] = −xz`).
- **Ground**: `GetAltitude` per vertex and the **flat normal of the exact triangle** (`LH3DIsland::GetNormal` 0x803630,
  raw heights without the flattening at the sea edge), with the original's two tables (T1 0xE9B2D8, T2 0xE9A2D8, built by
  fn_00803890): `land_normal::OfCell`, checked branch by branch (`documentacion/physics/step2_misc.md` §4).
- **Sea**: if the ground under the centre is < 0,0001, the centre is below the radius and the cell has no land:
  buoyancy `frac·m·g/density`, drag ×100, and **no contact with the bottom**. Density rises by 6,67e-5 per
  submerged substep (it soaks up water); above 1 it sinks. Below −4R the object is deleted.
- **Rest**: threshold 1 (4 if already at rest), grows after 15000 counts; the counter starts at −scale·half height·1000
  (0x7FB7D6) and adds 5 per substep.
- `Data\PhysicsConstants.txt`: version 3, 24 rows × 6 (density, contact k/mass, penetration k/mass, µ,
  angular fraction kept per second, drag), each column clamped to its range. Rows per class: 0 houses,
  3 rocks, 4/5 pots, 6 trees, 7 villagers, 8 animals, 14–20 toys, 21–23 mushrooms (table in `physob.md`).

## Bodies

- Mass = `escala³ · info.weight` (Object::GetWeight 0x638480), minimum 0.01. Houses 2000, static.
- By default, the vertices and triangles of the `isPhysics` submeshes (bit 13) or, if there are none, those of LOD 0.
  **The user's modified `AllMeshes.g3d` has no physics submeshes in the rocks**, so the drawing mesh is used
  (40–109 vertices; the original art has 12–24).
- Trees (`SetUpPhysObAsATree` 0x63A230): 16 points in rings along the trunk and 24 faces; centre of mass at
  0.4 H (alive) or 0.5 H (dead). Villagers and animals (0x5EFF40 / 0x5F04E0): box of 12 points and 20 faces, centre at
  half height, drag ×2.

## Manager (PhysicsObject::GameTurnUpdate 0x644FC0)

- **Once a game turn**, from `GGame::ProcessTurn` 0x54E67E (after `FireFly::ProcessAll`, before `GScript::Process`;
  not while paused): the turn's start, its **20 substeps in one go** (0x64576F..0x64604D, `cmp eax, 0x14` at 0x646046)
  and its end. The game logic sees the end-of-turn pose. openblack: `PhysicsObjects::GameTurnUpdate` from
  `Game::GameLogicLoop`. (The second call in `Process3dEngine` 0x54DAB0 is dead: nothing writes [0xD46A74].)
- **Drawn between turns** (fn_00646FE0 from `GLandscape::Draw` 0x5E49DC, every frame, each awake body):
  fn_007FCE80 lerps the 12 floats of the turn-start matrix (PhysOb +0xAC, copied from +0x7C at 0x645187) to the end
  one cell by cell with the turn fraction (g_game +0x205D64, `game_clock::TurnFraction`), normalises each row
  (fn_007FB5C0, `lh_matrix::NormaliseRows`; (approximate) exact 1 / sqrt, not the table of 0x841170), and takes
  `T − R·s·com` as the origin. `PhysicsDrawPose` carries it to the drawing (instances, blob shadows, a villager's carried prop, the
  trees' bending sources); a body at rest is drawn at its Transform. (pending) the −Radius < T.y filter of the drawing (0x647017), the
  π/2 turn of animated meshes (vt +0x1AC, 0x7FD009) and vt +0x184, the flames of a burning flying object
  (FireGraphic), the roots of a flying tree (they follow its Transform), dynamic shadow receivers (RendererShadows).
- **The list** grows by 16 slots when it is full (`MakeSureEndSlotIsFree` 0x644C40, no upper limit). The wake pass
  stops when the allocated slots are used up (0x6453E2 / 0x645411 / 0x6454B7): no more proxies or wake-ups that turn.
- **Pair skip** (0x64583E..0x645866): a villager's body (+0x1A4 == 1) does not hit what a Living pushed (flag 2:
  `Object::PushObject` 0x6396BA, `Ball::KickBallAtDestination` 0x435D99, `FelledTree::Create` 0x51186B). Nothing in
  openblack sets flag 2 yet (pending, with those three).
- Each turn, each moving body looks at the box `|v.xz|·0,1 + R` and adds as **obstacles at rest** the objects
  that interact: rocks and statics, mobile objects, villagers, animals, dead trees, hand pots, houses and
  storehouses. **Standing trees do not interact** (thrown things pass through them); neither do fields, forests nor piles.
- Object–object collision: ray from the centre of A to each vertex against the triangles of B; the forces go to both. An
  obstacle that does not manage to stay still (threshold 4) starts flying: that is how a villager is knocked down or a rock pushed.
- At the end of the turn: `impacto = |ΣF|·0,05` (mean force), G = impact / (m·g) (≈1 when resting), and
  `ReactToPhysicsImpact` on both bodies. The damage is attributed to the player who threw what hits.
- **Damage**: villagers and animals, if G > 2, lose `(G−2)·0,03` of life (× defenceMultiplierCrush; 1 for villagers).
  Rocks (not hit by another rock), if G > 4 and height > 0.7: life −(G−4)·0,005, and they split below 0.01.
  Tree or dead tree hitting a storehouse: it becomes wood. Houses: see below.
- **End of flight**: a thrown tree ends up as a DeadTree in the pose it was left in; a hand pot on land
  becomes a pile; villagers and animals get up; a villager that ends up in a cell with water goes to DROWNING (60 s)
  and a sunken animal is deleted (see [water.md](water.md#sinking-drowning-and-being-deleted)).

## Buildings that break (Abode::ReactToPhysicsImpact 0x406240, FragMesh 0x7F6F00..)

Code: `src/ECS/Physics/Buildings.*`, `FragMesh.*`, components `BuildingDamage` and `Fragment`
(`src/ECS/Components/Fragment.h`), meshes generated in `src/3D/L3DMeshGenerated.cpp`. Reports:
`documentacion/physics/fragmesh.md` and `abode_damage.md`.

- Only rocks (row 3) and the row-20 toy break buildings, with `p = |v|·masa` of the hitter:
  **p > 2000** breaks; 1000–2000 and 300–1000 only make sound (`editor.sad` 431–436 and 437–442).
- On the first hit the building is copied into triangles in world coordinates (LOD 0 submeshes). An **infinite
  cylinder** through the rock's position, in the direction of `0,3·v` and with radius `R + 0,7`, decides:
  - triangles with all 3 vertices inside break;
  - those with 1–2 are split through the midpoint of the longest side (up to 3 times depending on their size);
  - small ones go by majority.
- The broken ones of each primitive fly as one piece with the impact velocity (spin ±1). Its loose parts and everything
  left of the building that does not touch the ground (y < ground + 0.1) fall as still pieces (spin ±2). Loose
  triangles disappear. Groups by shared sides (tolerance 0.01).
- The **building's life** becomes the fraction of triangles remaining; below 0.75 it stops working (pending:
  the villagers coming out and the village emergency); at 0 it disappears: its villagers are left homeless and a storehouse
  loses its piles. The collapse sound plays (`editor.sad` 443–447).
- The damaged building is drawn with its FragMesh: each triangle flat, with a back face 0.45 behind and a wall on
  each open edge (its own generated mesh). Its physics body keeps the intact mesh; if the same rock
  hits it again, they stop colliding (it passes through on the third contact).
- **Pieces**: body = its distinct vertices and a copy of each one 0.45 behind (no faces: nothing collides with them),
  around its origin; the original's ×2 (0x76F2DB) goes to the **drag**, not to the inertia; the rest counter uses
  the half height of the rock mesh from info.dat. They are created inside `FragMesh::Impact`, before reading what remains
  (so a hit that only splits triangles also releases pieces). A building forgets the rock that hit it when the rock
  stops or is picked up, or when its body is rebuilt (fn_646D60, Abode::SetUpPhysOb).
- **Pieces**: row 11, mass `30·área`, they only collide with the ground, cannot be picked up, last 100 turns per triangle
  (no fade). Very thin ones (area < 0.4 R²) are deleted when created. A large one (area > 9) that falls while the
  building is still standing stays as rubble of the building.
- Quirk of the original: the rubble counts again as building triangles, and if after a hit the count
  reaches 1 the FragMesh is deleted and the house is drawn whole. It is left like this on purpose (the user prefers it as in the original, 2026-09-29).
- Pending: repair by villagers (the
  "half-built" drawing over the rubble), creature hits, village alignment and aggressor, buildings under
  construction (−0.2 per hit).

## Sounds, dust and the look of impacts

Code: `src/ECS/Physics/CollisionSounds.*`, `Dust.*`, `PartialBuild.*`. Reports `documentacion/physics/collision_sounds.md`
(full table in `snd/full_matrix.md`) and `building_visuals.md`.

- **Collision sound** (`AttemptToAddSoundEvent` 0x6464F0), once per turn on each awake body with something that
  hit it or `F > 0,5·m·g`: collision type of each side (info `collideSound`; piece = BUSH; DeadTree mesh 406 =
  HOLLOW_WOOD; no object = GROUND, or WATER in the sea), level by `g = impacto / (peso de info sin escalar · 9,81)`
  (3 if < 1,25, 1 if > 3, otherwise 2), and `GAudio::SamplePlayAnimEffect(objeto, |g_camera − punto|, {nivel, 0, A, B, 75},
  0, editor.sad, track = A ≠ 0x16)` (0x646919): the sample is chosen by the `editor.sad` animation table in the audio
  core (B4, [audio.md](audio.md#b4-the-worlds-callers-on-the-channels)), in 3D on the object, which is the owner
  of the channel. A pair does not sound again until two turns later. A rock against a building: the building sounds.
- **Buildings**: medium hit {2, 0, 0x16, 0x10, 75} and weak {3, …} (`G_Rock_V_Ground_M/S`, 0x406610), collapse
  {1, 0, 0x16, 9, 75} (`G_Crash_Abode`, 0x40671D), in 3D with the building as owner. (Corrects 431–447, which came from reading
  the bank table with a one-column offset.)
- **Dust**: on falling to the ground, 6 puffs from `data\blobs.raw` (rows 2–3), colour 0x50806040, size `min(2R, 5)`,
  ±2 m/s, life 1 s of game time, they grow in 0.125 s and shrink down to 0; in the sea colour 0x28C8F0F4 + splash + ring;
  in shallow cells, ring + dust. Each building piece releases one per vertex (0x80706050, size 2).
- **Whoosh** (G_ROCKPAST, `InGame.sad` 69–73): a body that enters the camera's 10 m sphere at more than 20 m/s:
  `PlaySoundEffect(0, 69 + GetTickCount() % 5, modo 2, 0, 0, 2D, InGame)` (0x645C12).
- **Half-built hit building**: over its FragMesh the intact model is drawn clipped at
  `pos.y + pct·alto` (pct = `(vida − s)/(1 − s)`, s = 1.1·life − 0.1 on hit: 1/11), with an inner wall at 0.35 (0.2 if
  the material is two-sided), a cap on the cut and the scaffolding (the submesh with the highest status) coming out of the ground; nothing
  if the cut ends up below 0.2. Without repair by the villagers, it stays like that.
- **Shadows**: pieces do not cast any; the broken building keeps the static shadow of its intact model.
- **Pending** (they depend on systems that do not exist yet): snow on the FragMesh (weather storms, snow map)
  and charring/glow from fire; the 0.75 colour of the cap.

## Water in impacts and when dropping

- In [water.md](water.md): the [water cells (SeaCells)](water.md#water-cells-seacells), the
  [impact against the water, the wave when bobbing and the resources that fall into the sea](water.md#impacts-and-objects-that-fall-into-the-water)
  and [sinking, drowning and being deleted](water.md#sinking-drowning-and-being-deleted) (`HasSunk`, state DROWNING 16,
  `ToBeDeleted`). What remains here is dropping from the hand, which decides whether the object lands or stays in physics (also
  over water).
- **Dropping (gently or throwing): `Object::InitialisePhysicsFromHand` 0x636F00** (matched code in bw1-decomp
  `src/Black/Object.cpp:447`), fully ported in `physics::from_hand::InitialisePhysicsFromHand` (`src/ECS/Physics/FromHand.cpp`). Every
  drop goes through here: packet 0x12 calls `ApplyThisToMapCoord` (tree over a wood store → the store
  keeps it, 0x74BFD0) and then `ThrowObjectFromHand(status, 0)` 0x6385E0 with the **spring velocity** (not zero):
  1. `PhysicsObject::AddObject(obj, v, 0, NULL, status)`; `lanzado = v.x² + v.z² > 4` (> 1 if a creature throws it).
  2. `AdjustToGroundLevel(lanzado, !IsAnyKindOfTree)` 0x7FCB80 (not thrown: lowers the body until its lowest
     vertex touches the ground, aligned to the normal except for trees; thrown: only pulls it out of the ground), `ZeroForces`,
     **`RaiseUntilNotIntersecting`** 0x644800 and the FROM_HAND flag (4).
  3. `aterriza` (lands) if it was not thrown, **it did not have to be raised** (the body's y did not change; exception: a villager of a computer
     player) and `IsDryLand || altitud de la celda redondeada (fistp) > 1`. `Living` and `Fence` also need
     normal.y ≥ 0.7 (0x6372A6). `IsFence` 0x609110 = `MobileStatic` with mesh 0x38 (American fence) or 0x51/0x52
     (Celtic fences).
  4. Lands → LANDED flag (8). `Living`, `Fence` or tree (not on fire) on `IsLand` leave physics **on the
     spot** with `RemoveObject(obj, 1, 1)`: they take the body's pose and their `EndPhysics` runs (villager: LANDED or
     drowns; animal: LANDED; tree: replanted, see objects). A tree **tilted** in the hand (|x| or |z| of
     `GetYXZ` > 0.2) or with `dont_replant` stays in physics **without** LANDED. Everything else (rocks, statues, thrown
     pots…) **stays in physics with LANDED**, resting and aligned, until it stops by itself (~1 s for a rock on flat ground).
  5. Does not land (the sea, outside the map, raised on top of something, thrown) → keeps flying or falls;
     `Villager::CreateDroppedResource` 0x750940 (drops the log it carries: pending, openblack does not carry wood),
     `Reaction::CreateReaction(obj, 9 REACT_TO_FLYING_OBJECT)` (pending, no reactions) and
     `Creature::CheckAllCreaturesForCatching` (pending, creature).
  - **Hand pot** (`Pot::InitialisePhysicsFromHand` 0x66DF00): with |v|² ≤ 5 (all three axes) there is no physics:
    `StartMultiPutdown` (particles), `Pot::AddResourceToPos` (merges with piles and storehouses, **is lost in the
    water**), `GoolooGooloo` and `ToBeDeleted`; faster, it flies like any object.
  - `RaiseUntilNotIntersecting` 0x644800: adds as bodies at rest (fn_00644DF0) the objects in the cells under
    the square C ± R that `InteractsWithPhysicsObjects`, and raises the body by what
    max(fn_007FDD60(it, other, (0,−1.0)), fn_007FDD60(other, it, (0,1,0))) says with each body whose spheres overlap,
    repeating while any pushes > 0.001. fn_007FDD60 = the largest `dot(q − hit, dir)` of the vertices q whose
    backward ray (fn_007FC310, t < 0 without limit, unnormalised normal with threshold −0.0001) finds a face
    of the other. A villager is not raised onto something pushed by a `Living` (flag 2, `Object::PushObject` 0x6396BA).
  - `RemoveObject(obj, 1, 1)` 0x646A00: angles and position of the body, `EndPhysics`, and if LANDED and `IsLand`,
    `DropSfx` (vt+0x794: 0 except `Tree::DropSfx` 0x74BC60 = G_PlantTree_01 + tick % 3, which openblack plays when
    replanting); `Tree::EndPhysics` 0x74B830 only replants with LANDED on land and without fire, otherwise, dead tree.
  - Hook: `OPENBLACK_HAND_TEST_DROP` (types 4 tree, 5 animal added). Checked on Land1 (1788.4; 2710):
    rock → in physics with LANDED and at rest after 0.9 s; tree → replanted; villager and animal → out of physics
    on the spot; pot → pile; villager in the sea (1464; 2016) → sinks and DROWNING 600 turns; rock over a
    building at (1780.4; 2713.3) → raised to y 38,3, without landing.

## Rocks that split (Rock::SplitInTwo 0x6E7560)

- A rock with 2D radius > 3.6 cannot be picked up: **clicking on it hits it immediately**. A rock that can be
  picked up is hit with a short click (< 225 ms). Condition: height > 0.7. No hit counter.
- Two rocks of the same type come out, scale × 0.7935 (∛½: half the volume), only the Y angle, at
  `Pos ± (cos a, 0, sin a)·0,7935·R2D` with random `a`; the original is deleted and the halves enter physics (they fall or
  keep flying with its velocity). Sound G_RockTap_01..04 (130 + counter 0xD559AC) in 3D at the hand's point, with the rock as owner (0x6E751D).
- Strong impacts also split them (see damage).

## Who owns what: class handlers and the hand's API

The original keeps each class's part of the physics in its virtuals (`InitialisePhysics`, `ReactToPhysicsImpact`
vt +0x7AC, `EndPhysics` vt +0x790, `HasSunk` vt +0x7B8). openblack keeps that split with
`PhysicsObjects::SetClassHandlers(PhysicsClass, ClassHandlers)`: one entry per class (`PhysicsObjects::ClassOf`:
Villager, Animal, Tree, DeadTree, Pot, Rock, Fragment, Building, Shield, Other), set by the system that owns the class.
`reactToImpact` gets an `ImpactInfo` (G of the turn, who hit it, the thrower, whether the hand threw it). An empty handler
keeps the physics' own code for the class. The hand registers Tree, DeadTree and Pot (`HandPhysics.cpp`); the villagers'
and animals' parts are still inside `PhysicsObjects.cpp` until session Personas moves them.

The hand calls `physics::from_hand::Throw(object, spring velocity, dont_replant)` (`Object::ThrowObjectFromHand`
0x6385E0, after it took the object out of the hand) and `ForceDrop` (`GInterface::ForceDropHeld` 0x5D4350); what the
throw needs from the hand (putting a hand pot down, wood stores, dead trees, roots) comes through
`from_hand::SetHandHooks`. A building that is deleted calls `physics::Buildings::OnBuildingDeleted` first.

**Rows of PhysicsConstants** (`GetPhysicsConstantsType` vt +0x788): a fence (`IsFence` 0x609110) is row 18, tested
after the three "rock" tests and before the toys (`MobileStatic` 0x609270); anything else with no override takes
`Object` 0x6376A0 = `CanBecomeAPhysicsObject ? 1 : 0`. Not ported (their classes are not in openblack): Ball 2, Poo 12,
LandscapeVortexIn 13, Scaffold 17, Creature 0, CitadelHeart 0, FieldCrop 6 (`step2_misc.md` §3.2).

**A thrown object always has a body.** `PhysicsObject::AddObject` 0x6443A0 only fails for a class that cannot become a
physics object, an IMMOVABLE one (flag 0x1000) or one already flying; the list never refuses. The angle the hand gives
(`ThrowAngularVelocity`, CHand +0x48D4) is the prediction body's L after `min(ping / 100, 5)` steps against the ground
(fn_00644F20): 0 in a single player game (inferred: ping 0), as in openblack (`step2_throw.md`). (approximate) an object
openblack cannot make a body for (no mesh; the original would crash in `PhysOb::Initialise`) is put on the ground
(`physics::from_hand::PlaceWithoutBody`); openblack's old ballistic flight for it is gone.

## Pending

- Snow on the FragMesh (needs the weather: snow storms and the 128×128 snow map) and charring/glow from
  fire (needs the fire system); the 0.75 colour of the cap of the "half-built".
- Buildings: repair by villagers (building site, wood), creature hits, village alignment and aggressor,
  the inhabitants coming out when dropping below 0,75, half-built buildings (−0.2 per hit).
- Villagers and animals on landing: the original's three postures and the corpses (today they get up or disappear).
- Complete villager death (`VillagerDead` 0x7506C0) and the creature's mimicry when something dropped by the
  player sinks: in [water.md](water.md#pending).
- From dropping: the disciple sound
  (`MakeDiscipleSFX`) and `SetVillagerDisciple`, reaction 9, the villager's log and the creature stuff (catching,
  imitating, toys). The mesh branch of fn_007FDD60 (fn_008683C0) is not needed: no openblack body uses it.
  The `Tree`+0x5C & 2 flag of `Tree::EndPhysics` (0x74B882: only `Fixed::EndPhysics`, neither replanting nor dead
  tree) is set only by the `MagicTree` constructor (0x5FCF8D, `or byte [esi+0x5C], 2`; sweep of the whole
  `.text` in `documentacion/agua/re/scan_tree5c.py`): they are the trees of the forest miracle, which this tree does not have.
- The terrain normal without the original's quantisation.
- Tooltip "Golpear para Romper" ("Hit to Break", 0xEF7) and checking the player's influence before hitting a rock.
- **better physics** mod (requested by the user, disabled by default): building pieces with a collision mesh that
  can be grabbed; the engine stays faithful to the original.

## Test hooks

`OPENBLACK_TEST_PHYSICS="x,z,altura,vx,vy,vz[,escala[,n]]"`, `OPENBLACK_TEST_HIT_VILLAGER="velocidad[,escala[,índice]]"`,
`OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz"`, `OPENBLACK_TEST_HIT_ABODE="velocidad[,escala[,índice[,n]]]"` (n rocks against a house, one every 1.5 s), `OPENBLACK_HAND_TEST_SPLIT="x,z,escala,rondas"`,
`OPENBLACK_PHYSICS_TRACE=1` (position, velocity, contacts, G, density and radius of each body per turn),
`OPENBLACK_TEST_SEA="x,z,tipo[,altura]"` (type = `villager|animal|tree|pot|rock`: creates it at that height above the
point and puts it into physics with no velocity; writes the cell, the density and `GET_LAND_HEIGHT` there and on the reference
land, and traces the counter of the drowning villager every 100 turns),
`OPENBLACK_HAND_TEST_DROP="x,z,segundos[,tipo]"` (the hand holds a rock 0, a pot of 300 food 1 or of wood 2,
the first villager 3, the first tree 4 or the first animal 5, and **drops it gently** at (x, z) after those seconds of game time; the log says whether it ended up in physics).

## Sources

- `C:\Users\diewgarc\dev\documentacion\physics\`: `physob.md`, `physicsobject.md`, `physob_bodies.md`, `rock_split.md`,
  `fragmesh.md`, `abode_damage.md`, `collision_sounds.md` (full table in `snd/full_matrix.md`) and
  `building_visuals.md`.
- `bw1-decomp` `src/Black/Object.cpp:447` (`Object::InitialisePhysicsFromHand`, matched).
- `documentacion/agua/re/scan_tree5c.py` (sweep of the `Tree`+0x5C flag).
