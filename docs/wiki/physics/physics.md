# Physics

## Summary

Things the hand throws or drops, rocks, trees, pots, villagers, animals and building pieces, fly in the game's own
physics: each body is a cloud of points with springs and friction against the land and against other bodies, stepped
20 times per game turn by one manager that also decides what woke, what hit what and what each class does when it
lands. There is no rigid-body library: the original's engine is small and self-contained.

openblack runs the same engine and manager, ported from the original and pinned by tests, behind its own system and
component names; each class's part is a hook owned by that class's code. Impacts on buildings, dust, splashes and
split rocks are on [Physics impacts](physics-impacts.md); water itself is on [Water](../world/water.md).

## The original

The engine is `PhysOb` (0x7FB730..0x7FE7B0); the manager is `PhysicsObject::GameTurnUpdate` 0x644FC0. The lines and
picks under the cursor are the game's own tests ([The hand: animation and placement](../hand/animation-and-placement.md),
[The game camera](../camera/game-camera.md)).

### Engine

- **Penalty contacts** on a cloud of vertices (point masses): each vertex below the ground or inside
  another body receives a normal spring and a Coulomb friction anchor. No impulses and no restitution coefficient: the
  bounce comes from the spring and the damping of probing the vertices **0.06 s ahead** (`x + v·0.06`).
- **Penetration against another body** (fn_007FDD60, 0x7FDC90..0x7FDD0E): the ray from the centre T to the predicted
  vertex hits a face of the other body at `hit = T + s·dir`; `pen = predLen − dot(hit − T, dir) / predLen` with
  `predLen = max(|world − T|, 0.001)` (vertex +0x20, read by the `fdiv` at 0x7FDCF5 and the `fsubr` at 0x7FDD01), so
  `pen = (1 − s)·predLen`, the depth of the predicted vertex past the face. It is positive from the first substep the
  0.06 s look-ahead crosses the surface: that is what stops a fast body before it tunnels.
- **dt 0.005 s, 20 substeps per 0.1 s turn**, semi-implicit Euler. Gravity 9.81; max. speed 124 (the same as the
  hand throw); max. ω 3π. No linear damping; the angular one keeps `d4` per second.
- Constants: dt 0.005 (0x8C7674), gravity 9.81 (0x8CF02C), look-ahead 0.06 (0x8CA27C), max. speed 124 (0xC371FC, the
  same float caps the hand throw), max. ω 3π (0x9A2BB0).
- One substep is `ZeroForces` 0x7FD200, `GroundAndWater` 0x7FD4D0, `ContactForces` 0x7FDE40 and the integration
  fn_007FE260; `SetUpPos` 0x7FC760 builds the body's vertices from the 3D object's matrix: it copies the 12 dwords at
  LH3DObject +0x14 (0x7FC76B..0x7FC779), which an object in the map has from `Object::GetWorldMatrix` 0x638200 and a
  held one from the hand's state (where the hand drew it).
- The rotation step is only taken when the angle is above 1e-5 (0x99A100); the axis is the step divided by the angle
  (0x7FE706..0x7FE748).
- `AdjustToGroundLevel` 0x7FCB80 reads the normal at the centre's cell (`GetNormal` 0x803630) and starts its
  lowest-vertex search at 1000 (0x7FCB8E); at its end, and at the end of `SetUpPos`, the current matrix is copied into
  the turn-start one (0x7FCE56..0x7FCE6B, 0x7FC94C..0x7FC98F), so a body that was just placed is drawn where it is.
- The backward ray of `fn_007FC310` starts with best = −10000 (0xC61C4000) and keeps the hit with t < 0 nearest to 0.
- `AddObject` 0x6443A0 takes the spin in the body's axes: `t = M·w` with the inertia tensor (PhysOb +0x8), then L = the
  body rows times t (0x6445DB..0x644691). It sets L before `Object::InitialisePhysics` reaches 0x63756A.
- The original accumulates torque as F×r and rotates the rows the other way round; the two inversions cancel out. The port uses r×F and
  columns (same motion). The inertia tensor keeps the original's bug (`I[1][2] = −xz`).
- **Ground**: `GetAltitude` per vertex and the **flat normal of the exact triangle** (`LH3DIsland::GetNormal` 0x803630,
  raw heights without the flattening at the sea edge), with the original's two tables (T1 0xE9B2D8, T2 0xE9A2D8, built by
  fn_00803890): `land_normal::OfCell`, checked branch by branch.
- **Sea**: if the ground under the centre is < 0.0001, the centre is below the radius and the cell has no land:
  buoyancy `frac·m·g/density`, drag ×100, and **no contact with the bottom**. Density rises by 6.67e-5 per
  submerged substep (it soaks up water; the rate is 0x9A2BC8); above 1 it sinks. Below −4R (0x9A2BD0) the object is
  deleted.
- **Corpses sink**: at each turn start an object with `GetLife() < 0.01` gets +0.01 density (`GameTurnUpdate`
  0x644FEE).
- **Rest**: threshold 1 (4 if already at rest), grows after 15000 counts (0x8AB418; growth factor 6.66667e-5,
  0x9A2BCC); the counter starts at −scale·half height·1000 (0x7FB7D6) and adds 5 per substep.
- `Data\PhysicsConstants.txt`: version 3, 24 rows × 6 (density, contact k/mass, penetration k/mass, µ,
  angular fraction kept per second, drag), each column clamped to its range. Rows per class: 0 houses,
  3 rocks, 4/5 pots, 6 trees, 7 villagers, 8 animals, 14–20 toys, 21–23 mushrooms. It is loaded by
  `EditorPhysics::Load` 0x5249D0 into `EditorPhysics::PhysicsConstants` 0xCC63E0; the column minima are at 0x8D8B10 and
  the maxima at 0x8D8B28.
  The table is in the zero-filled data (no initial values in the file image) and `Load` returns at once when `fopen`
  fails (0x5249EE → 0x524B41), so without the file every material is zero; the only other writers are the physics
  editor's sliders (0x5249A0).

- F, g (0x5D4F34..0x5D501C): l2 = (z z + y y) + x x stays on the stack; l2 > 124 x 124 (an fmul of [0xC371FC] by itself; test ah, 0x41: NaN fails) and not all three 0: k = 124 / sqrt(l2), v = (k x, k y, k z); at PC24 the fsqrt and the fdiv round to float. h (0x5D5020..0x5D5032): bl = the UNCAPPED l2 > 1 ([0x8AA390])
- J: fn_00644F20 0x644F20: n = min(n, 15); history[i] = +0x7C, then 20 x ZeroForces 0x7FD200, fn_007FD4D0, fn_007FDE40, fn_007FE260 (its result ignored); history[n] = +0x7C
- 0x7FC767..0x7FC779: +0x7C = the object's matrix, its rows scaled (the unit rows times the scale, or as given)
- 0x7FCB9C..0x7FCBE2: GetNormal 0x803630 at the centre's MapCoords, ftol(x 65536 0.1) (LandIsland::GetNormalAt's ToFixed: the same cell)
- 0x7FCB8E: 0x447A0000
- 0x9A2BC8: waterlogged
- fn_007FC310: best starts at -10000 (0xC61C4000); the plane normal is the raw cross product of the world vertices, so the -0.0001 (qword 0x9A2BC0) threshold is on the unnormalised one; t < 0 and nearest to 0 wins. The mesh-collide branch of fn_007FDD60 (other +0x168, fn_008683C0) is not used by openblack's bodies.
- 0x7FDCF5 / 0x7FDD01: fdiv and fsubr read the vertex's +0x20, the predicted length (ZeroForces 0x7FD321, at least 0.001), not +0x1C: pen = predLen - ((hit - centre) . dir) / predLen, so the 0.06 s look-ahead stops fast bodies at thin walls (the physical shield, documentacion/physics/shield_physics.md)
- 0x6445DB..0x6445FE
- 0x644633..0x644691
- 0x9A2BD0
- Rest: at least 1 (4 once resting), growing after 15000 counts (0x8AB418, 0x9A2BCC)
- 0x99A100
- fn_007FE260 0x7FE706..0x7FE748: inv = 1 / angle (fdiv 0x7FE710), axis = step inv, M = fn_007FB180(axis, angle) (lh_matrix::AxisAngle = glm::rotate(-angle)) and the rows times M (fn_0046D9D0 0x7FE748: r_k' = r_k M, glm's M * R). The original's torque is F x r (0x7FE0F9..0x7FE11F, 0x7FD7FD..0x7FD823), openblack's r x F, so its omega and axis are the opposite of openblack's: the original's axis is -(step inv)
- PhysicsObject::AddObject 0x6445DB..0x644691: w is in the body's axes and the original's sign; t_x = ((M3 wy + M6 wz) + M0 wx) + 0, t_y = ((M1 wx + M4 wy) + M7 wz) + 0, t_z = ((M2 wx + M5 wy) + M8 wz) + 0 (M = the inertia tensor PhysOb +0x8, its translation zeroed by SetUpMoi), L_i = ((t_z r2_i + t_y r1_i) + t_x r0_i) with the rows of +0x7C, stored negated (this port's sign).
- fn_007FDD60 (PhysicsObject::RaiseUntilNotIntersecting 0x644B79 / 0x644B8A): the largest dot(q - hit, direction) over this body's vertices q whose ray back along -direction meets a face of `other` (fn_007FC310), 0 if none. With direction (0, -1, 0) it is how far this body must go up for its vertices to leave `other` through its top.
- Object::GetWorldMatrix 0x638200's rows for a yaw a (+0x4C) and a scale S (+0x50), no quarter turn: a != 0 and S != 1: (fl(c S), 0, fl(s S)), (0, S, 0), (-fl(s S), 0, fl(c S)) with c, s = fcos, fsin of a unrounded (0x63827F..0x638318); a != 0 and S == 1: c unrounded, s stored as a float (0x638323..0x6383B3); a == 0: diag(S) (0x6383C3) or the identity (0x638406). The "0 +" forms make a -0 a +0. glm: column k = the original's row k.
- GameTurnUpdate 0x644FEE: GetLife() < 0.01 -> sink factor +0.01 a turn (corpses sink)
- Every column clamped to its own range (0x8D8B10 minima, 0x8D8B28 maxima)
- (order) AddObject below already runs the class's initialisePhysics and CheckAllCreaturesForCatching, and L is set after them; the original sets L inside AddObject (0x6445DB..0x644691) before InitialisePhysics' 0x63756A. The same while those hooks do not read L (none does)
- Rep movsd, 12 dwords PhysOb +0x7C -> +0xAC (SetUpPos 0x7FC94C..0x7FC98F, AdjustToGroundLevel 0x7FCE56..0x7FCE6B, GameTurnUpdate 0x645187..0x64519B): the three rows and the translation
- The copy at the end of AdjustToGroundLevel (0x7FCE6B)
- C.y += best, fn_007FD140 (the object's origin) and PhysOb::SetUpPos 0x7FC760
- SetUpPos copies the matrix into the turn-start one (0x7FC94C..0x7FC98F)
- 0x639622..0x639628: GetWeight vt +0x638, fmul [0x8CF02C]. ⚠️ approximate Weight() is not GetWeight for Abode / StoragePit / Fragment, and DeadTree's 0x511590 is not ported
- A game turn has started since the body was added: until then it is drawn where it is. Before the first turn the two matrices are equal anyway: SetUpPos 0x7FC760 (0x7FC94C..0x7FC98F), AdjustToGroundLevel 0x7FCB80 (0x7FCE56..0x7FCE6B) and so RaiseUntilNotIntersecting when it raises the body (SetUpPos at 0x644BE7) all copy the 12 floats of the current matrix into the turn-start one (PhysicsObjects::SyncTurnStart)
- Loads Data\PhysicsConstants.txt (EditorPhysics::Load 0x5249D0).
- AddObject (0x6443A0) with the angular velocity as the original takes it (PhysOb::SetAngularVelocityFromBody, 0x6445DB..0x644691), bit-exact; the world-omega overload above goes through R^T.
- The turn-start matrix becomes the current one (the `rep movsd` of 12 dwords PhysOb +0x7C -> +0xAC at the end of SetUpPos 0x7FC98F and of AdjustToGroundLevel 0x7FCE6B): turnStartRotation / turnStartCentre = the body's. Called by Add, AddProxy, BeginTurn (0x645187..0x64519B), RaiseUntilNotIntersecting after each raise, and by every caller of PhysOb::AdjustToGroundLevel right after it.
- What a villager drops when released without landing (Villager::CreateDroppedResource 0x750A05..0x750A89): Object::InitialisePhysics(velocity, angularVelocity, thrower 0, add 1, status 0) (vt +0x784 -> AddObject), then with a body: po+0x90 = angularMomentum when given (PhysOb +0x68, L in world space), flag 0x10 (NoObjectCollision), PhysOb::AdjustToGroundLevel(false, true) 0x7FCB80 (SyncTurnStart) and RaiseUntilNotIntersecting 0x644800.
- RaiseUntilNotIntersecting (0x644800): resting bodies are made for the objects of the map cells under the body's square (C +- R) that InteractsWithPhysicsObjects (ShouldPhysicsRaiseObjectUntilNotIntersectingThis 0x6377D0), then the body goes up by max(fn_007FDD60 both ways) until no overlapping body pushes it more than 0.001. Each raise is fn_007FD140 + PhysOb::SetUpPos (0x644BE0 / 0x644BE7), so the turn-start matrix follows (SyncTurnStart).

### Bodies

- Mass = `scale³ · info.weight` (Object::GetWeight 0x638480), minimum 0.01. Houses 2000, static.
  The body's mass and the object's weight are two values: a class's SetUpPhysOb passes its own mass to
  SetUpConstants (Abode 0x402DD0 pushes 2000.0; Fragment 0x76EC50 30 × area, at least 0.01; a creature 1000), while
  GetWeight (vt +0x638, read by the catch, PushObject's CalculateForceAppliedBy 0x639622, the tug, the kick and the
  pick-up tests) stays `scale³ · info.weight`: a building by its abode's info, a fragment by the third mobile static
  (its Rock info, 0x76E9E4). Ported as DynamicsSystem::BodyMassOf and WeightOf.
- By default, the vertices and triangles of the `isPhysics` submeshes (bit 13) or, if there are none, those of LOD 0.
  **The user's modified `AllMeshes.g3d` has no physics submeshes in the rocks**, so the drawing mesh is used
  (40–109 vertices; the original art has 12–24).
- Trees (`SetUpPhysObAsATree` 0x63A230): 16 points in rings along the trunk and 24 faces; centre of mass at
  0.4 H (alive) or 0.5 H (dead). Villagers and animals (0x5EFF40 / 0x5F04E0): box of 12 points and 20 faces, centre at
  half height, drag ×2. The box's 12 points are turned by π/2 about Y with cos stored as the float −4.371139e-8; the
  radius is the length of the turned vertex (0x5F01BD..0x5F0218 / 0x5F0763..0x5F07BE).
- Creature: it never flies (`CanBecomeAPhysicsObject` 0x479D10 = 0) but thrown objects hit it
  (`InteractsWithPhysicsObjects` 0x479D20 = 1). Its body (`Creature::SetUpPhysOb` 0x479B90) is the LH3DCreature's
  bounding sphere, mass 1000, not dynamic, one vertex per skeleton element. The mass is pushed as a constant
  (1000.0, 0x447A0000 at 0x479BD5, into `SetUpConstants` 0x7FB810, +0x134) and nothing writes it again: the turn
  update of `LH3DCreature::UpdateTime` (0x482BC4..0x482BF5) refreshes only the body's centre (+0xA0) and radius
  (+0x150). Its weight as an object (the catching, the lift share, the pick-up test) is another value,
  `Creature::GetWeight` 0x47CD60 = `LH3DCreature::GetMass` 0x47FA80, which follows the size.
- The physical shield gets a heavy body: GMapShieldInfo 0xDA05D0 (magic) / 0xDA06D8 (physical), weight 50000.
- Animated meshes (boned, mesh flag 0x100, `LH3DObject::IsAnimated` vt +0x1AC): at draw time fn_007FCE80 turns the
  rows by π/2 about Y after normalising them (0x7FD013..0x7FD082): r0' = c·r0 + s·r2, r2' = c·r2 − s·r0, row 1
  unchanged.
- Fragment body (`Fragment::SetUpPhysOb` 0x76EC50): the triangles' distinct vertices (exact match), each with the normal
  of the first triangle it was found in. The ×2 is applied at 0x76F2DB to PhysOb +0x14C (the drag). The rest counter
  uses the half height of the mesh the Fragment's ctor builds as a Rock (`Rock(coords, &MS[2] = 0xD3A930 …)` 0x76E9E4,
  scale 1 at 0x76EA32).
- One-shot orb: `OneOffSpellSeed::Create` 0x72A3BE makes a MobileObject with the GMobileObjectInfo at 0xD39F3C, the one
  after WHALE (0xD39E28, stride 0x114), index 25 **🔍 inferred**.

- The same origin for another pose of the body (fn_007FCE80 0x7FD097..0x7FD117: T - R s com)
- OneOffSpellSeed::Create 0x72A3BE: MobileObject(pos, 0xD39F3C, ...), the GMobileObjectInfo after WHALE's 0xD39E28 (stride 0x114): ONE_OFF_SPELL_SEED 25 (inferred: WHALE's address is not checked either)
- GMapShieldInfo 0xDA05D0 / 0xDA06D8 (weight 50000: Object::GetWeight gives the physical shield a heavy body)
- Villager::SetUpPhysOb (0x5EFF40) / Animal::SetUpPhysOb (0x5F04E0): 12 points on three levels (head, waist, feet) turned by pi/2 about Y, 20 faces, centre of mass at half height, drag x 2. The draw's quarter turn of an animated mesh (LH3DObject::IsAnimated, vt +0x1AC: the boned meshes, mesh flag 0x100): fn_007FCE80 0x7FD013..0x7FD082 after NormaliseRows, with c = fcos, s = fsin of (double)(float)(pi / 2) [0x8C7B48] left on the stack (not rounded), each product and sum rounded to float (PC24): r0' = c r0 + s r2, r2' = c r2 - s r0, row 1 unchanged. Villager / Animal bodies are in the original's axes (no turn, livingaxes.md §1-§2); their Transform is the drawn rotation, so it is the body's rows turned.
- Villager 0x5F01BD..0x5F0218 / Animal 0x5F0763..0x5F07BE: RotY((float)pi / 2) with c stored as a float, -4.371139e-8f (0xB33BBD2E), and s = 1.0f: x' = fl(-z + fl(c x)), z' = fl(x + fl(c z)); the radius is sqrt((z z + y y) + x x) of the turned vertex
- Creature::SetUpPhysOb 0x479B90 (vt +0x78C): its own body, from its skeleton (pending, creature area)
- Fragment::SetUpPhysOb (0x76EC50): the distinct vertices and a copy of each 0.45 behind along its normal (a slab), no faces (nothing is hit by a fragment), about the fragment's origin; mass 30 x area; drag x 2 (0x76F2DB doubles PhysOb+0x14C, the drag)
- Initialise takes the half height (mesh+0x28) of the LH3DObject Fragment's ctor builds as a Rock: Rock(coords, &MS[2] = 0xD3A930 = 0xD3A6D8 + 2 x 300...) 0x76E9E4, scale 1 0x76EA32, then MobileStatic::CallVirtualFunctionsForCreation 0x76EB61 with MobileStatic::GetMesh = info +0x120 (0x6084E0).
- Creature::InteractsWithPhysicsObjects 0x479D20 (mov al, 1)
- A one-shot orb is a MobileObject too (0x72A3BE)
- The class's GetWeight (Creature 0x47CD60), unclamped
- The draw's quarter turn of an animated mesh (fn_007FCE80 0x7FD013..0x7FD082): a Villager's or an Animal's drawn rotation (openblack's Transform) from the original's rows (its bodies since livingaxes). documentacion/physics/ livingaxes.md

### Manager

- **Once a game turn**, from `GGame::ProcessTurn` 0x54E67E (after `FireFly::ProcessAll`, before `GScript::Process`;
  not while paused): the turn's start, its **20 substeps in one go** (0x64576F..0x64604D, `cmp eax, 0x14` at 0x646046)
  and its end. The game logic sees the end-of-turn pose. (The second call in `Process3dEngine` 0x54DAB0 is dead: nothing writes [0xD46A74].)
- **Drawn between turns** (fn_00646FE0 from `GLandscape::Draw` 0x5E49DC, every frame, each awake body):
  fn_007FCE80 lerps the 12 floats of the turn-start matrix (PhysOb +0xAC, copied from +0x7C at 0x645187) to the end
  one cell by cell with the turn fraction (g_game +0x205D64, `game_clock::TurnFraction`), normalises each row
  (fn_007FB5C0, `affine::NormaliseRows` (was `lh_matrix`), with LH3DMath's `InverseSquareRoot` 0x841170 and its table, ours `gutils::ModelInverseSquareRoot`), and takes
  `T − R·s·com` as the origin. `PhysicsDrawPose` carries it to the drawing (instances, blob shadows, a villager's carried prop, the
  trees' bending sources); a body at rest is drawn at its Transform. (pending) the −Radius < T.y filter of the drawing (0x647017), the
  π/2 turn of animated meshes (vt +0x1AC, 0x7FD009) and vt +0x184, the flames of a burning flying object
  (FireGraphic), the roots of a flying tree (they follow its Transform), dynamic shadow receivers (RendererShadows).
- In the original a body is posed between turns only while −Radius < T.y (0x647017..0x64702A); a body sunk deeper than
  its radius keeps the pose it was last drawn at.
- **Turn start** (0x645013..0x645180): an entry whose object is no longer available (`IsAvailable` vt +0x2C at
  0x645018) gets `PhysOb::DeInitialise` 0x7FB730 and is removed; the last entry moves into its slot. This is how the
  body of an object marked for deletion leaves (the dead list itself is in [The engine loop: turns and frames](../engine/loop.md)). Then the
  end matrix (+0x7C) becomes the turn-start one (+0xAC) (0x645187..0x64519B).
- **Every removal moves the last entry into the gap**, and the slot is looked at again:
  - the turn start's unavailable entry (0x645023..0x645173, `dec esi` at 0x645179);
  - the entry still asleep after the wake walk (DeInitialise 0x6455CB, the copy 0x645603..0x645743, `dec ebp` at
    0x64574F);
  - `AddObject` 0x6443A0 on an object resting in the list (DeInitialise 0x644438, the copy 0x64443D..0x64458A);
  - `RemoveObject` 0x646A00 (DeInitialise 0x646B82, the copy 0x646B98..0x646CD8, back one slot at 0x646CDE).

  The entries are copied by value, so a pointer to an entry (another entry's hitBy, or lastHit to its body) names a
  **slot**, not an entry: after a move it names the entry moved into the slot, and a pointer to the old last slot
  names what is left there. Nobody else's hitBy is fixed up at a removal (0x64508F, 0x645667, 0x6444A1, 0x646C00).

- **Nothing leaves the list during the turn's steps and its end.** The flag 0xD4782C is set right after the turn start
  (0x645765) and cleared after the end loop (0x64646B). While it is set `RemoveObject` returns at once
  (0x646A00..0x646A12). So:
  - a body that comes to rest keeps its entry as a resting body, and its object is the one `EndPhysics` returns, even
    one it deleted (0x645EB5..0x645F0E);
  - a body below −4R gets `ToBeDeleted(0)` only if its object is still available (0x645D15..0x645D2E), and is still
    stepped and hit;
  - a sunk animal is deleted by its `HasSunk` (vt +0x7B8 at 0x645A19, no availability test), which stops the body
    (0x645A23..0x645A3E); it then rests as above;
  - an object deleted by a reaction, or taken out by any other caller, keeps its entry.

  They all go at the next turn start, once their object is no longer available. Only `AddObject`, which takes a
  resting body out itself, changes the list during the turn.

- **Wake walk** (0x6452A6..0x64556A): for each moving body, cells are walked x outer, z inner; a cell is walked only
  once per turn for all the bodies (list of walked cells, at most 0x200, 0x64547B). In each cell the fixed list comes
  first, then the mobile list. An object already in physics is woken (|= 1, 0x6454F7); one with a 3D object gets a
  resting proxy at once (`AddProxy` fn_00644DF0, 0x64551D).
- Box corners (`RaiseUntilNotIntersecting` 0x644877, `GameTurnUpdate` 0x645322): x·6553.6 (0x8AC400) then `__ftol`;
  off the map fn_00604250 clamps each corner to 0..511 ([g_game+0x59C8] − 1).
- Physical shields always stay in the list (`GetAlwaysRemainsInPhysicsInternalSystem`, PhysicalShield 0x72CAF0 = 1),
  whether or not something moves near them.
- `PhysicsObject` +0x1A4: 1 villager (`AddObject` 0x64476D, `AddProxy` 0x644E67), 2 felled tree (`FelledTree::Create`
  0x511883), 3 felled tree that has toppled, else 0.
- **Felled tree falling sound** (0x64609F..0x6460F8): a kind-2 body whose up row has y < 0.98 (0x8CF3FC), and whose
  height is above 10 (0x8AB414), plays once `SoundTag::Create(MapCoords, GetRandomSample(31, 1), track 0, mode 3, 3D,
  InGame)` 0x71EB60; then it becomes kind 3.
- Deletion below −4R (code 4, 0x645B22): `ToBeDeleted(0)` is called only if the object is still available
  (0x645D15..0x645D2B).
- An object `Object::InitialisePhysics` 0x637480 puts in flight leaves the map cells (`IsObjectInMap` vt +0x178 at
  0x6374AC, `RemoveMapObject` vt +0x548 at 0x6374BA); a burning object leaves its fire group
  (`FireEffect::StartedMoving(0)` 0x637589); `CheckAllCreaturesForCatching` 0x47CBD0 runs at 0x63756A. It walks
  the creature list; for each it calls fn_0047C960 first (0x47CBF3) and skips the creature when that returns 0, then
  skips one with Flags & 0x10 (0x47CC00), one in the hand's locked select.
- A knocked resting proxy is woken through `InitialisePhysics` (vt +0x784 at 0x645FB3) only when that call starts
  (Started == 1, 0x645FB9).
- **The list** grows by 16 slots when it is full (`MakeSureEndSlotIsFree` 0x644C40, no upper limit). The wake pass
  stops when the allocated slots are used up (0x6453E2 / 0x645411 / 0x6454B7): no more proxies or wake-ups that turn.
  A growth allocates a new array (0x644C84) whose every slot is built by `PhysicsObject::PhysicsObject` 0x644330 (it
  sets only the vtable, +0x4 and +0x1A8..+0x1D4: the object +0x18, hitBy +0x20, status +0x24 and the body are left
  as the allocator gives them), copies the `count` listed entries (0x644CE0), frees the old array (0x644CF9), and
  rebases every listed entry's non-null hitBy by slot index, `new + ((hitBy - old) / 0x1DC) x 0x1DC`
  (0x644D0D..0x644D56). lastHit (+0xA0) is not rebased, so after a growth no lastHit matches a listed body.
  `AddObject` checks the slot (0x6443C9) after `CanBecomeAPhysicsObject` (0x6443AD) and the flag test +0x25 & 0x10
  (0x6443BC), before it looks for the object in the list, and again after taking a resting entry out (0x644590),
  which never grows it. After a resting entry is taken out, the original always appends the new entry (0x64458D /
  0x644590): its Add cannot fail at that point.
- **Pair skip** (0x64583E..0x645866): a villager's body (+0x1A4 == 1) does not hit what a Living pushed (flag 2:
  `Object::PushObject` 0x6396BA, `Ball::KickBallAtDestination` 0x435D99, `FelledTree::Create` 0x51186B). Nothing in
  openblack sets flag 2 yet (pending, with those three).
- **Pushing** (`Object::PushObject(Living*)` 0x639640, vt +0x81C, from `Villager::CheckForClearArea` 0x75917E): the
  object's body (found with `SearchForPhysicsObject` 0x646950, or made by `InitialisePhysics(0, 0, …)`) gets flag 2 and
  external force += F·d/|d|, d = body centre − the object's MapCoords point, F = `CalculateForceAppliedBy` 0x639620 =
  GetWeight × 9.81 (the pusher is not read). Nothing if d is exactly 0. An object a particle system carries has no
  body, so it gets no force (0x639678). It returns 0.
- Each turn, each moving body looks at the box `|v.xz|·0.1 + R` and adds as **obstacles at rest** the objects
  that interact: rocks and statics, mobile objects, villagers, animals, dead trees, hand pots, houses and
  storehouses. **Standing trees do not interact** (thrown things pass through them); neither do fields, forests nor piles.
  Who interacts: physical shield 1 (0x72D600), magic shield 0, creature 1, fields never
  (`Field::InteractsWithPhysicsObjects` 0x528020); buildings only while standing (MultiMapFixed: `GetPercentBuilt >
  0.1 && life > 0.01`).
- Object–object collision: ray from the centre of A to each vertex against the triangles of B; the forces go to both. An
  obstacle that does not manage to stay still (threshold 4) starts flying: that is how a villager is knocked down or a rock pushed.
- At the end of the turn: `impact = |ΣF|·0.05` (mean force), G = impact / (m·g) (≈1 when resting), and
  `ReactToPhysicsImpact` on both bodies. The damage is attributed to the player who threw what hits.
- **The end of the turn, entry by entry** (`GameTurnUpdate` 0x646053..0x6464C4). The in-turn flag 0xD4782C is set
  before the substeps (0x645765) and cleared after this loop (0x64646B); while it is set `RemoveObject` 0x646A00 returns
  at once (0x646A00..0x646A12), so no entry leaves the list during the loop. The count is read again at every entry
  (0x646447) and the entry is taken again from the list base after the reaction (0x646363): the entries a reaction adds
  (pieces, rock halves) are walked too, with no force, so only steps 1-3 touch them. For each entry, in list order:
  1. its pushes +0x144..+0x158 are cleared (0x64607B..0x646099);
  2. the felled tree's sound (above, 0x64609F..0x6460F8);
  3. a resting proxy (+0x19C) is moved to its object's MapCoords point (0x646102..0x646214);
  4. when |ΣF|² ≤ 0.0001 (0x8BF518) the rest is skipped (0x646239..0x646244, the jump to 0x646441): the impact +0x8
     and hitBy +0x20 are left as they were, and nothing reads either in that turn (their readers are steps 7-10 and
     the reactions). Otherwise the listed slots 0..count−1 are searched
     (0x64627D..0x6462C6) for the one whose body address is lastHit (0x646296); on a match hitBy is that slot, and
     **only then** its status +0x24 is copied when this entry has none (0x64629C..0x6462B2). With no match hitBy keeps
     its value of earlier turns, and no credit is taken. There is no clear of hitBy at the turn start, which clears
     only ΣF and lastHit (0x6451A1..0x6451AA). Since lastHit is set only by the substeps' pair loop on listed bodies
     (0x7FDD25 / 0x7FDD2F, from 0x6458D2), it names a listed slot when the loop starts, and the only cases a reaction
     earlier in the loop makes are:
     - **a growth** (a reaction's `AddObject` with the list full): no later lastHit of the loop matches, so every
       later entry that touched a body keeps its old hitBy (a slot rebased by index, from the last turn whose lookup
       ran for it, or 0 from `AddObject` / `AddProxy`) and takes no credit. That slot is always listed when it is
       read: it was below the capacity, which equals the count at the growth, and the count never falls during the
       loop (`RemoveObject` returns at once, and `AddObject` always appends after it takes a resting entry out);
     - **an `AddObject` on an object resting in the list**: the last entry moves into its slot k and the new entry
       goes to the last slot n−1. A later lastHit of slot k's body matches the moved entry (and takes its credit), one
       of slot n−1's body the new entry; any hitBy names the slot's new occupant, the entry's own too, read again
       after its reaction through the list base (0x646363, 0x6463C3).

  7. the collision sound (0x6462CD..0x646352): not a resting proxy, and hitBy or impact > 4.905·mass; the level from
     impact / (info +0xAC · 9.81): under 1.25 → 3, over 3 → 1, else 2; then `AttemptToAddSoundEvent` 0x6464F0 with
     `GetGameObjectWhoHitMe` 0x644F00;
  8. `ReactToPhysicsImpact(entry, false)` (vt +0x7AC, 0x64635D), for every entry past step 4, resting or not, hit or
     not. There is no availability test anywhere in the loop;
  9. deed 0xF, DAMAGE_BY_THROWING (0x64636B..0x6463BE): FROM_HAND (+0x1D8 & 4), a status, an object that `IsLiving`
     or `IsRock` and not `IsAnyKindOfTree`: `ConsiderMakingCreatureMimicPlayer(status, 0xF, object, 0)` 0x4EA900 on the
     status's player;
  10. when hitBy is set: `GScript::SetHitObject(object, GetGameObjectWhoHitMe)` 0x6EC7B0 on g_game +0x250090
      (0x6463C3..0x6463E2), which keeps each only if `IsAvailable`, else 0 (+0x70 the thing hit, +0x74 what hit it);
      then deed 0x10, DAMAGE_BY_THROWING_AT, on the same object under the same tests (0x6463E7..0x64643A).

  After the loop the sound-pair list (0xD47208) ages: each count goes down by one, and a pair at 0 is removed by
  shifting the rest down, so the order is kept (0x646462..0x6464C4).

  The hit record is overwritten by every later hit in the same loop: the last entry in list order wins.
  `SetHitObject` has two callers, this loop and `CLEAR_HIT_OBJECT` 0x6FBE00. The record is read by `GET_HIT_OBJECT`
  0x6FBD80 (+0x70), `GET_OBJECT_WHICH_HIT` 0x6FBDC0 (+0x74) and `GAME_THING_HIT` 0x6FBE20, which pushes
  (+0x70 == object), and false with "Object no longer valid" when the object is gone. The base game's `challenge.chl`
  calls `CLEAR_HIT_OBJECT` 5 times and `GAME_THING_HIT` 5 times (ChooseYourCreature, ThrowingStones, the Land 2 shield
  challenge, the idol pyre), and never the two getters.

  The four natives (runblack.exe, `GScript` statics):
  - `GET_HIT_OBJECT` 0x6FBD80 / `GET_OBJECT_WHICH_HIT` 0x6FBDC0: g_game +0x250090 → +0x70 / +0x74; non-null →
    `AddScriptGameThing(thing, 0)` 0x70D0F0 and `PUSH(id, 4)` (an object); null → `PUSH(0, 4)`. No availability test
    at read time.
  - `CLEAR_HIT_OBJECT` 0x6FBE00: `SetHitObject(0, 0)` 0x6EC7B0.
  - `GAME_THING_HIT` 0x6FBE20: `POP`, `GetScriptGameThing` 0x70D220; null → `ScriptErrorMessage("Object no longer
    valid")` (0xC0D428; `ScriptErrorMessage` 0x6F62B0 is a bare `ret` in the shipped game, so nothing is printed) and
    `PUSH(0, 6)`; else `PUSH(+0x70 == thing, 6)` (a bool, `sete`). Plain pointer equality, no other rule.

- **Damage**: villagers and animals, if G > 2, lose `(G−2)·0.03` of life (× defenceMultiplierCrush; 1 for villagers).
  Rocks (not hit by another rock), if G > 4 and height > 0.7: life −(G−4)·0.005, and they split below 0.01.
  Tree or dead tree hitting a storehouse: it becomes wood. A pot or a pile (the hand's pots included) that hits
  something (`Pot::ReactToPhysicsImpact` 0x66DC90): the hit (`GetGameObjectWhoHitMe` 0x644F00) and the pot available,
  and the hit `IsResourceStore` of the pot's type or `IsPot` of the same type → the hit's
  `DeleteObjectAndTakeResource(pot, po +0x24)`; when it took nothing (or no such hit), `MobileObject`'s reaction
  0x607A20. Houses: see
  below.
- Life: `Object::GetLife` (vt +0x11C, 0x402600) reads Object +0x48.
- **Living damage** (`Living::ReactToPhysicsImpact` 0x5ED3E0, the slot of Villager and Living; Animal's 0x41BC10
  calls it after its store test). The whole of `Object::ApplyEffect`, as for any effect:
  1. G = impact (+0x8) / (mass (+0x15C) × 9.81) (0x5ED454..0x5ED46E); G ≤ 2, or unordered, does nothing (`fcom 2;
     test ah, 0x41; jne`): strictly above 2;
  2. the share s = float((G − 2) × 0.03) (0x5ED470..0x5ED47E);
  3. `EffectValues(3, GetGameObjectWhoHitMe(entry), PhysicsObject::GetPlayer(entry))` (0x5ED482..0x5ED496): the
     crush preset g_EffectInfo[3] (crush 1.0), applied by what struck it (entry +0x20 → +0x18; none for a fall onto
     the land, 0x644F00), with the thrower's player at +0x3C (entry +0x24's status → `GetPlayer`, else none, 0x647460);
  4. its 7 numbers × s (`operator*=` 0x525720, 0x5ED4A4);
  5. `IsEffectReceiver` (0x5ED4B2), then `ApplyEffect(ev, 0)` (vt +0x5CC, 0x5ED4C7; `Object::ApplyEffect` 0x637980
     for Villager, Animal and Living), in the order of [The spell core](../magic/spell-core.md)'s `Object::ApplyEffect`: the
     life (crush × defenceMultiplierCrush; 1 for villagers), the kill (`DestroyedByEffect` vt +0x5F8, 0x637A8A;
     `Villager::DestroyedByEffect` 0x7502D0 → `VillagerDead(2)` does nothing while it flies (0x7506C3): the landing
     kills it), REACT_TO_OBJECT_CRUSHED above a crush of 0.01 (the effect's own number, so G above about 2.33), the attack on its
     town (`GetCausedPlayer` 0x525910: the thrower first, else the striker's own player), and the alignment of
     `EffectValues::GetPlayer` 0x5254C0: the striker's own player when something struck it (none when it has none:
     a villager of no town, a standing tree), else the thrower's, else the **neutral** player (0x5254D5..0x5254ED).
     A creature that struck it moves its own alignment instead (0x637B82..0x637BA3).
  - No GameRand, GameFloatRand or C runtime draw on the way. The crushed reaction's spread draws only for a fire's
    reaction (+0x20 == 1, 0x6E3F18), and its start for villagers and animals is `Living::SetupReactToObjectCrushed`
    0x4170F0, `ret 8`. Only the town attack's guidance remark (`GGuidance::TownAttackSFX` 0x71B7C0, not ported) draws
    LocalRand.
  - The thrower is the local player when its hand threw it (PhysicsEntry `byPlayer`).
- `Rock::ReactToPhysicsImpact` 0x6E7930 holds the "not hit by another rock" test.
- Hits on a resting proxy clear a building's last hitter: `Abode::SetUpPhysOb` 0x402DD0.
- **End of a body's physics** (EndPhysics vt +0x790): `Object::EndPhysics` 0x6375A0 clears IN_PHYSICS, sets the map
  coordinates from Pos and, with insert and the object available (0x637617), puts it back in the map cells when
  `MapCoords::InBounds` 0x6042C0, else deletes it (`ToBeDeleted(0)`). It always returns the object. Villager
  (0x5F0B81) and Animal (0x5F0E01) call it in the middle of theirs, after SetYAngle and before their landing, water
  and death work (`PhysicsObjects::BackInMap` from `ECS/Systems/Implementations/VillagerPhysics.cpp`); the other classes after their part. A
  tree that becomes a DeadTree never calls it: the DeadTree goes in the cells with no InBounds test (0x74BC0A).
- **Where a stopped object is placed** (`GameTurnUpdate` 0x645D33..0x645F33, entry `esi`, body `esi + 0x28`):
  after the building-hitter forget (fn_00646D60) and `SetXYZAngles(GetYXZ(body + 0x7C))` (0x645D51..0x645D6A), vt
  +0x2C8 (0x645D76) is `IsVillager`: `GameThingWithPos::IsVillager` 0x401D10 (`xor eax, eax`) in the Object, Living,
  Animal and Abode vtables, `Villager::IsVillager` 0x55CAB0 (`mov eax, 1`) in Villager's.
  - A villager (0x645D80..0x645DF8) takes the body's centre (body + 0xA0 / 0xA4 / 0xA8): MapCoords x =
    `__ftol(c.x · 6553.6f)`, z = `__ftol(c.z · 6553.6f)` (truncated), altitude = c.y − the land's height there
    (0x645DAB..0x645E94). Its `EndPhysics` (vt +0x790, 0x645EB5) then sets the altitude to 0 (0x5F0BA8): it stands on
    the land at the centre's x, z.
  - Anything else (0x645DFD..0x645E89) takes the same arithmetic on the origin of the body's object pose
    (fn_007FD140: the rows scaled by body + 4, origin = centre − rows · (body + 0xF8..0x100)).
  - `RemoveObject` 0x646A00 has no such test: it always takes the pose's origin (0x646A84).
  - Entry + 0x1A0, freed and cleared after `EndPhysics` (fn_008745E0, 0x645ED8..0x645EEA), is a drawing cache
    (set only by fn_007FCE80 from the drawn-pose code fn_00646FE0): no game state.
  - `RemoveObject` writes the place before its EndPhysics test (0x646B06), so it is written with or without the end of
    physics: x, z = `__ftol(p · 6553.6f)` into the object (0x646A9D..0x646AC2), then the altitude from a second
    MapCoords of `__ftol((p · 65536) · 0.1f)` (0x646ABC..0x646AF1, the same values: 65536 · 0.1f is 6553.6f exactly)
    through `LH3DIsland::GetAltitude` 0x803090, altitude = y − the land's height (0x646AFE..0x646B08).
  - The object takes the body's pose (`SyncTransform`),
    then stands at `physics::objects::PlacedOnTheMap(land, p)`: p is the pose's origin, or at the turn's stop a
    villager's body centre; x and z through `map_coords::ToFixed`, the height the land's there plus y − the land's
    (`map_coords::FromWorld` / `ToWorld`), and a Fixed's bounding centre with it; a villager's landing then grounds it.
    `RemoveObjectWithEndPhysics` passes no stop, so it takes the pose's origin. The plain `RemoveObject` (no end of
    physics) does not write the pose or the place yet: fix row F26 (the testbed's fix queue).
- When a body stops in `GameTurnUpdate` (0x645EB5..0x645F08), the object `EndPhysics` returned gets
  `SetXYZAngles(GetYXZ(body))`. A DeadTree returned by `Tree::EndPhysics` is a MobileStatic (0x608CE0), so it keeps x,
  y and z.
- Orientation after physics (`SetXYZAngles`, vt +0x514; `GetYXZ` 0x7FAB30): an Object (Villager, Animal, Tree,
  Feature, 0x638D00) takes only the Y angle and stands upright (0x638DBA..0x638E6D), so a tree replanted from the hand
  stands straight; a MobileStatic (DeadTree, Fragment, statics, 0x608CE0) or MobileObject (Pot, Scaffold,
  OneOffSpellSeed, 0x607460) takes YXZ(y, x, z) (`SetYXZMatrixOnly` 0x7FAC10).
- The landed yaw is taken by `RemoveObject` 0x646A50 / `GameTurnUpdate` 0x645D51 before the class's `EndPhysics` as
  `GetYXZ`'s yaw, atan2(−fwd.x, fwd.z), and set again after it (0x646B72 / 0x645F08).
- **In physics or not.** The original's flag IN_PHYSICS (`GameThingWithPos +0x24 & 0x40`) is set by
  `Object::InitialisePhysicsFromHand` (0x636F2F) and cleared by `Object::EndPhysics` (0x6375A0) and `RemoveObject`
  (0x646B56). A body at rest keeps its entry in the list as the resting proxy (0x645EE0) with the flag already
  cleared, so the flag is `PhysicsObjects::IsFlying(entity)` (an entry that is not resting), not
  `PhysicsObjects::Find(entity) != nullptr`.
  - The dynamics system
    sets it again from its list whenever an entry is added, starts to move (a knocked resting proxy), comes to rest or
    goes, so it is on exactly the objects with a moving body; the carry adds it in `Take` and removes it in `Release`
    (see below). A reader that only asks "in the physics?" reads the tag: the press of the hand (in physics and not
    carried, the 225 ms grab path, 0x5D529F), the carry's own refusal (0x637487), `PushObject` (0x63965A) and the
    knocked proxy's start. A reader that needs the body (velocity, pose) still looks up the entry.
  - The tag is also on a thing left hanging where a hand let go of it (below, "An immovable thing let go hangs"). It
    has no entry, so nothing in the list sets the tag again for it: `SyncInPhysics` only runs on an object whose entry
    is added, starts to move, stops or goes, and on a carried one. That is the original's rule as well: IN_PHYSICS is
    cleared only by `EndPhysics` and by a `RemoveObject` that finds an entry (0x646B56).
  - Other writers of the flags word (+0x24 as a whole), found in a sweep of every word write to `[reg+0x24]` from a
    register in runblack.exe: the `Object` constructors (0x63648B, 0x636577) and `GameThingWithPos::SetToZero`
    (0x5705D2) zero it at creation, and the read-modify-writes of the other bits (SetInScript 0x639B3A,
    SetControlledByScript 0x40225A, SetMoveable 0x6FB43C, SetPickupable 0x6FB4AC, SetIndestructable 0x6FDEC7,
    InitialiseIsFixedForMapList 0x63A663, RemoveDraggingCreatureByLeash 0x636E84, PuzzleGame 0x6D8A2E/0x6D8A58) keep
    bit 0x40 as it is. No other writer of IN_PHYSICS was found.
- **End of flight**: a thrown tree ends up as a DeadTree in the pose it was left in; a hand pot on land
  becomes a pile; villagers and animals get up; a villager that ends up in a cell with water goes to DROWNING (60 s)
  and a sunken animal is deleted (see [Sinking, drowning and being deleted](../world/water.md#sinking-drowning-and-being-deleted)).
  - `Villager::EndPhysics` 0x5F0A60 takes its landing pose from the **turn-start** rows (po+0xD4..): with a = right.y,
    a < −0.5 (0x8CEFCC) → posture 1, yaw = GetYAngle(up) (no π, no wrap), creature desire ANGER 0.5; a > 0.5
    (0x8AA3B4) → posture 2, yaw = Wrap(GetYAngle(up) + π), ANGER 0.5; else posture 0, yaw = Wrap(GetYAngle(fwd) + π),
    COMPASSION 0.1. With a player (`GetPlayer` 0x647460) it calls `MakeCreatureEmpathiseWithPlayer` (0x5F0B53).
  - Then `SetYAngle` 0x60DAC0 (+0x4C and GameAngle +0x5C) at 0x5F0B61; if it was not put down gently (no LANDED),
    +0xE0 &= ~0x20 (0x5F0B67..0x5F0B70); then `Object::EndPhysics` 0x5F0B81, the land type into status bits 4-5 of
    +0xB4 and altitude +0x1C = 0 (0x5F0B88..0x5F0BA8).
  - On land with life ≤ 0 (0x5F0CA0..0x5F0CEC): an already dead one goes to state 15 with its counter kept, otherwise
    `VillagerDead(5 PLAYER_INTERACTION, player, 0, 1)`; alive, +0x104 = `GetPlayer()` (0x5F0C83), then
    `SetTopState(LANDED)` 0x5F0D31 → `Villager::Landed` 0x7606E0.
  - `Animal::EndPhysics` 0x5F0D80: land type from the turn-start right.y, the villager's mapping reversed (a > 0.5 → 1,
    a < −0.5 → 2, else 0, 0x5F0D93..0x5F0DC7); yaw = Wrap(GetYAngle(the **current** fwd row) + π)
    (0x5F0DC9..0x5F0DF3); then `Object::EndPhysics` 0x5F0E01, land type, altitude 0 and its life/state work.
  - Without a body (`Living::InitialisePhysicsFromHand` 0x5EFDF8, `EndPhysics(NULL, 1)`): the villager jumps to
    0x5F0B79 and the animal to 0x5F0DF9 with land type 3, with no pose, no empathy and no `SetYAngle`; with no
    PhysicsObject the drowning death uses the neutral player g_game +0x205A5B (0x5F0C17).
  - A put-down villager or animal never enters FLYING: `Living::InitialisePhysicsFromHand` sets FLYING (0x5EFDEB) only
    when the body exists, is not LANDED and still belongs to this object (0x5EFDD3..0x5EFDE0).

- K, l (0x5D5083..0x5D5105): fn_007FD140's translation, Velocity, PhysOb +0x68 and the +0x7C rows for GetYXZ
- GPacket::ProcessPacket case 0x40 (0x63CDF7..0x63CEB9; sent by CHand::GetRequiredState 0x46CE01..0x46CE8D about 180 ms after a release, handDelta = the hand's displacement since): with a body (SearchForPhysicsObject 0x646950), |v| = sqrt((vz vz + vy vy) + vx vx) of PhysOb +0xDC, m = PhysOb +0x134 (po +0x15C); the external torque PhysOb +0x128 (po +0x150..+0x158) += (-(((m dz) 1.6) |v|), 0, ((m dx) 1.6) |v|) with 1.6 [0x931338], in the original's sign (its torque is F x r: stored negated here). ZeroForces copies it every substep and GameTurnUpdate zeroes it at the turn's end. Nothing without a body.
- fn_007FD140 0x7FD140: the object's origin from the body, T - (s R) com in the original's order (0x7FD15E.. 0x7FD1ED): each row times the scale first, then t0 = ((r2.x cz) + (r1.x cy)) + (cx r0.x), t1 = ((r2.y cz) + (r0.y cx)) + (r1.y cy), t2 = ((r2.z cz) + (r0.z cx)) + (r1.z cy), float products and sums.
- The allocated slots of the list (0xD4781C): MakeSureEndSlotIsFree 0x644C40 grows it by 16 when it is full, never shrinks it, has no upper limit; DeleteAll 0x6442B0 sets it to 0
- Tree::EndPhysics 0x74BBD9..0x74BC3A: the DeadTree from fn_00510B70 goes in the cells with no InBounds test (InsertMapObject vt +0x544 at 0x74BC0A) and is returned; Object::EndPhysics is not called
- A corner of the physics' cell boxes (RaiseUntilNotIntersecting 0x644877, GameTurnUpdate 0x645322): x 6553.6 [0x8AC400], __ftol, and off the map (MapCoords::InBounds 0x6042C0) fn_00604250: each signed high word clamped to 0 .. [g_game+0x59C8] - 1 (511)
- PhysicsObject::MakeSureEndSlotIsFree 0x644C40: count == capacity -> capacity + 16 (0x644C54)
- 0x645013..0x645026: the object not IsAvailable (vt +0x2C at 0x645018) -> PhysOb::DeInitialise 0x7FB730 and the entry goes (the last one moves into its slot, 0x64502B..0x645180)
- 0x645187..0x64519B: the end matrix (PhysOb +0x7C) becomes the turn-start one (+0xAC)
- PhysicsObject::GameTurnUpdate 0x6452A6..0x64556A: each moving body (+0x19C clear) wakes what is in the map cells of its box C -/+ (fn_006E8160 |v.xz| x 0.1 [0x8AB22C] + R [+0x178]) (BoxCell), x outer ([esp+0x1C]) and z inner ([esp+0x2C]); a cell is walked once for all the bodies (the list of walked cells, 0x64544B..0x645491, keeps at most 0x200). The inline iterator (0x645495..0x6454A3: the fixed list, then the mobile one, map_cells:: ForEachInCell): what InteractsWithPhysicsObjects (vt +0x79C, 0x6454C7) is woken when it is in the physics already (|= 1, 0x6454F7), else with a 3D object (+0x40) it gets a resting proxy (AddProxy fn_00644DF0, 0x64551D) at once. The walk stops when the list's allocated slots are used up (count >= capacity [0xD4781C]: before each x column 0x6453E2, each z cell 0x645411 and each object 0x6454B7): AddProxy does not grow it here, so the condition stays true for the rest of the walk and for every later body of the turn
- 0x64547B
- GetAlwaysRemainsInPhysicsInternalSystem (PhysicalShield 0x72CAF0: 1): the physical shields stay in the system at their Object scale, whether or not something moves near them (Magic/Objects/MapShield)
- 0x64609F..0x6460F8: a felled tree (kind 2) that has toppled (its row 1's y, PhysOb +0x8C, < 0.98 [0x8CF3FC]) sounds once when taller than 10 [0x8AB414] (GetHeight vt +0x42C): SoundTag::Create(its MapCoords +0x14, GetRandomSample(31, 1), track 0, mode 3, loops 0, 0, is3D 1, InGame, delay 0) 0x71EB60; then kind 3
- PhysicalShield 0x72D600: 1, MagicShield 0
- Fields are never hit (Field::InteractsWithPhysicsObjects 0x528020); buildings only while standing (MultiMapFixed: GetPercentBuilt > 0.1 && life > 0.01)
- (inferred, not read) out of the physics without EndPhysics: back in the map cells at once while it exists, not at the next map_cells::Sync. Nothing for a resting proxy (it never left) or one held out (a tornado's) the original's only insert is Object::EndPhysics' (0x646B20 -> 0x637617 test byte [esi+0xA], 1): never for an UNAVAILABLE (marked) object
- 0x644877..0x644919: the cells of the two corners (C.x - R, C.z - R) and (C.x + R, C.z + R) (BoxCell: x 6553.6, __ftol, clamped into the map by fn_00604250 when off it), the low and high words of the two
- X outer, z inner (0x64491F..0x64493D), JustMapXZ::InBounds 0x644951, ToMap 0x644962 and the inline iterator: the fixed list, then the mobile one (0x644967..0x644975, map_cells::ForEachInCell; a MultiMapFixed in every cell of its NewCollideDescriptor). AddProxy (fn_00644DF0, 0x6449F4) puts each in the physics list at once, so a later cell of the same object skips it ("not in the physics list yet"): here the candidates list does that
- Object::SetXYZAngles 0x638DBA..0x638E6D: the yaw only
- MobileStatic 0x608CE0 / MobileObject 0x607460 -> SetYXZMatrixOnly 0x7FAC10
- CalculateForceAppliedBy 0x639620 does not read the Living
- 0x639678: carried by a PSys (+0xA & 0x10, taken with no body but IN_PHYSICS) goes to InitialisePhysics, which gives no body to an object already IN_PHYSICS (0x637487 -> 0x637502): 🔍 inferred nothing then, so no force
- 0x63965A: IN_PHYSICS (+0x24 & 0x40) with a body
- SearchForPhysicsObject 0x646950
- 0x6396C6..0x63970F: the object's MapCoords (+0x14) as a point: y = GetAltitude 0x803090 + its altitude, x / z times [0x8AA3A4]; d = T (po +0xC8..+0xD0) - it
- fn_00646FE0 (GLandscape::Draw 0x5E49DC), every frame for each awake entry: fn_007FCE80(PhysOb, g3d, the turn fraction g_game +0x205D64) lerps the 12 floats of the turn-start matrix (+0xAC) to the end one (+0x7C) cell by cell (0x7FCED2..0x7FCFFD), normalises each row (fn_007FB5C0 0x7FD000), scales them and takes T - R s com as the origin. A body at rest has no entry of its own pose: it is drawn at its Transform (the end of the turn). (pending) the turn of animated meshes by pi/2 (vt +0x1AC, 0x7FD009; documentacion/physics/livingaxes.md) and vt +0x184 (0x647052, an animated object's frame through vt +0x18C / +0x188 and fn_005167D0)
- (dead list plan) an UNAVAILABLE (marked) object is not posed: fn_00646FE0 itself has no IsAvailable test (its vt +0x2C at 0x6470ED is DeadTree's LH3DObject [0xCC5F04]), but the original's body of a marked object goes at the next turn start (0x645018) and the object is out of the cells (CleanupWhenDeleted 0x6377F0), so it is not drawn
- 0x647017..0x64702A: fld [+0x150] (Radius); fchs; fcomp [+0xA4] (T.y, the current centre); test ah, 1: posed only when -Radius < T.y, so a body sunk deeper than its radius keeps the pose it was last drawn at
- Object::PushObject 0x6396BA / Ball::KickBallAtDestination 0x435D9B: pushed by a Living (a villager is not raised over it by RaiseUntilNotIntersecting)
- +0x1A4: 1 a villager (AddObject 0x64476D, AddProxy 0x644E67), 2 a felled tree (FelledTree::Create 0x511883), 3 a felled tree that has toppled (GameTurnUpdate 0x6460F8), else 0
- A knocked resting proxy is woken through Object::InitialisePhysics (vt +0x784 at 0x645FB3) only when it starts (Started == 1, 0x645FB9): it can become a physics object and no particle system carries it (0x637487)
- Object::PushObject(Living*) 0x639640 (vt +0x81C, no override; Villager::CheckForClearArea 0x75917E): its body (SearchForPhysicsObject 0x646950 when IN_PHYSICS and not carried by a PSys, else InitialisePhysics(0, 0, 0, add 1, 0) vt +0x784) gets flag 2 (PushedByLiving, 0x6396BA) and external force += F d / |d|, d = T - the object's MapCoords point, F = CalculateForceAppliedBy 0x639620 = GetWeight x 9.81 [0x8CF02C] (the pusher is not read). Returns 0.0 (0x6397A6; the only caller pops it).
- The object's rotation from a body's (or an atom's) rows as the original's SetXYZAngles(GetYXZ(rows)) (vt +0x514, GetYXZ 0x7FAB30): an Object (Villager, Animal, Tree, Feature: 0x638D00) takes only the Y angle and stands upright; a MobileStatic (DeadTree, Fragment, the statics: 0x608CE0) or MobileObject (Pot, Scaffold, OneOffSpellSeed: 0x607460) takes YXZ(y, x, z) (0x63B680 -> 0x7FAC10); anything else keeps the rows (pending: Football and the Ball have no component yet). explosions.md §2.6
- The creature area's CheckAllCreaturesForCatching (pending, creature area: the CreatureList walk, the eligibility fn_0047C960, 0.8 x weight, CanBePickedUp, the reach test fn_0047C9D0 and the 0xED plan)

### Who owns what: class hooks and the hand's API

The original keeps each class's part of the physics in its virtuals (`InitialisePhysics`, `ReactToPhysicsImpact`
vt +0x7AC, `EndPhysics` vt +0x790, `HasSunk` vt +0x7B8). `ReactToImpact` gets an
`ImpactInfo` (G of the turn, who hit it, the thrower, whether the hand threw it). A default hook keeps the physics' own
code for the class. Tree, DeadTree and Pot are the hand's code (`HandPhysics.cpp`); Villager and Animal are the
villagers' and animals' code (`ECS/Systems/Implementations/VillagerPhysics.cpp`). A Creature struck by a body takes
`Creature::ReactToPhysicsImpact` 0x479990 (`ecs::creature_struck::HurtCreature`, `ECS/CreatureStruck.cpp`): the
crush by how hard it was struck for its `GetMass`, the feedback, fear and anger
([Struck by a thrown thing](../creature/hits-and-catch.md#struck-by-a-thrown-thing)).

- Scaffold's virtuals: before `Object::InitialisePhysicsFromHand`, `DestroyThingsInWay` when flag 0x800
  (0x6EA8A3..0x6EA8AD); after it, flag 0x400 = LANDED && !dont_replant (0x6EA8D2..0x6EA903), not called when no body
  was made (0x6EA8D4); `Scaffold::CanBecomeAPhysicsObject` 0x6EA910 (not in a workshop slot, no site);
  `Scaffold::EndPhysics` calls `Object::EndPhysics` itself in the middle of its work (0x6E87B6).
- `Villager::ThrowObjectFromHand` 0x756AE0..0x756B1C runs before Object's: with dont_replant, a villager of no player
  or of the hand's player gets `SetVillagerDisciple(0, 0, 0)` (`GPlayer::IsMemberOfThisPlayer` 0x64D750).
- `Villager::InitialisePhysics` 0x5EFEF0 runs on every `InitialisePhysics` (not from the hand): `CreateDroppedResource`,
  then `Living::InitialisePhysics` 0x5EFE10 (FLYING; `StorePreviousState` unless IN_HAND 0x18, and `SetTopState(FLYING)`
  must return 1, otherwise a particle system's take is refused).
- `Object::DropSfx` 0x63A7B0 is empty. `Fixed::EndPhysics` reaches `Object::EndPhysics` through 0x52E054 / 0x52E0CB.
- The class's `EndPhysics` 0x6375A0 inserts into the map cells with `InsertMapObject` vt +0x544 at 0x63762C; with NULL
  it also removes the flying-object reaction (9) and calls `ConsiderCreatureMimickingWhenObjectLands`
  (0x63763D..0x63764A).
- An object a particle system carries ([The objects a particle system carries (tornado and vortex)](../world/vortex.md#the-objects-a-particle-system-carries-tornado-and-vortex)):
  the take (fn_006CA060) takes the object out of the map cells and sets GameThing +0xA |= 0x10; `Take` is refused when
  the object is IN_PHYSICS (so a second take is refused, 0x637487) or IMMOVABLE, after the class's part (0x637480). On
  `Release` (0x6C9F3B..0x6C9F54), a villager, animal or tree lands upright with the atom's yaw, while a static, mobile
  object or pot keeps the atom's tilt. `Tree::EndPhysics` returns at once for a carried tree (+0xA & 0x10, 0x74B837 /
  0x74B84B), so the tree is not put back in the map cells and stays IN_PHYSICS. `Release` removes both, but a tree keeps
  `InPhysics` (no body, out of the map), as the original.
- **The class's end of physics on `Release`**: the object's own `EndPhysics(NULL, true)` (vt +0x790, called at
  0x6C9F99 while the carried bit is still set; the bit is cleared after it, 0x6C9FA2). Read in the v1.42 exe. The
  tornado lets go only of a living thing this way (the atom's destructor 0x6C9FC0 tests `IsLiving` vt +0x3C4, then
  calls `DestroyedByEffect`; anything else it carried gets `ToBeDeleted`); the vortex (`UR_VortexAttract`, 0x6D3E73)
  lets go of whatever it carried. With no physics record, per class:
  - Villager 0x5F0A60, Animal 0x5F0D80 (and their subclasses): the living's landing with no body.
  - Tree 0x74B830 (MagicTree too): returns at once while carried (above).
  - Object 0x6375A0 (mobile objects, balls, poo, the creature...) and PuzzleGrain 0x561A40: IN_PHYSICS cleared, back
    in the map or deleted outside it, the flying-object reaction (9) removed, `ConsiderCreatureMimickingWhenObjectLands`.
  - Fixed 0x52DF50 (abodes, features, fields, flowers, mobile statics, totems, worship sites... Rock 0x6E7000,
    Bonfire and FelledTree 0x511970 only call it): with insert and `CanBecomeArtifact` (vt +0xCC, 0x63A7D0: the info's
    artifact multiplier > 0 and not in a script) it asks `PhysicsObject::GetPlayer` (0x647460, from 0x52DF78), whose
    first read is the record's +0x24: with no record that reads address 0x24, and **the original crashes**. Otherwise
    the Object's end.
  - Pot 0x66DBD0 (pots, piles, PotStructure, MagicFood, MagicWood): with insert, pot row 11 or 12 (HandWood, HandFood)
    and not on water (`MapCoords::IsWater`), `SetupReaction` 0x66D660, then the record's interface status (+0x24,
    read at 0x66DC21) for `AddResourceToPos`: with no record **the original crashes**. Otherwise the Object's end.
    With a body that status is the hand's when a hand threw the pile or threw what struck it (copied into a struck
    body at the turn's end, 0x64629C..0x6462B2), and NULL for a pile knocked by anything else: then the land pile is
    put down by no interface, as with no record.
  - DeadTree 0x5113F0: Rock's end, then `CreateReaction(this, 12, GetPlayer(record), 0)`: `GetPlayer` with no record
    (0x51140A) **crashes**. With a body the same: the object's end, then reaction 12 with the record's player (its
    interface status +0x24, the hand's when a hand threw it or threw what struck it, inherited at the turn's end;
    none otherwise); FelledTree's own end 0x511970 is Fixed's only.
  - Scaffold 0x6E8780: the creature-mimic part is skipped (the record is tested, 0x6E8793), the Object's end, the town's
    `CheckScaffoldSnapToPoint`, then the record's FROM_HAND flag (+0x1D8, 0x6E87D8): with no record **it crashes**.
  - Fragment 0x76F3F0: reaction 9 removed, Fixed's end, then for a fragment that sticks to a building (+0x94 set,
    +0xA0 > 9.0, +0xA4 an available object with a +0x90 mesh) the record's matrix (+0xA4..+0xD0, from 0x76F4DA): with
    no record **it crashes**; otherwise +0xA4 and its FragMesh's +0x24 are cleared. In practice a carried fragment
    has no parent any more: the take is refused while it flies, and its first end of physics either merged it or
    cleared its parent.

- `IMMOVABLE` is GameThingWithPos +0x24 & 0x1000 (SET_ID_MOVEABLE); `GameThingWithPos::IsCannotBePickedUp` 0x401A10
  (vt 0x180) is only overridden by HanoiBlock 0x6DE440.

A class's `dropSfx` (DropSfx vt +0x794) is played by `RemoveObjectWithEndPhysics` (RemoveObject 0x646B2E..0x646B48) for
the object EndPhysics returned, when LANDED and on land; only the tree has one (`Tree::DropSfx` 0x74BC60, registered by
`HandPhysics.cpp`), and a body that comes to rest in GameTurnUpdate (0x645EB5) never plays it.

The hand calls `physics::from_hand::Throw(object, spring velocity, dont_replant)` (`Object::ThrowObjectFromHand`
0x6385E0, after it took the object out of the hand) and `ForceDrop` (`GInterface::ForceDropHeld` 0x5D4350); what the
throw needs from the hand (putting a hand pot down, wood stores, dead trees, roots) comes through
`from_hand::SetHandHooks`. A building that is deleted calls `physics::Buildings::OnBuildingDeleted` first.
A building site gets its partly built mesh from `PartialBuild::BuildMesh(building, intact mesh,
percent, tag)` (fn_816AD0 in the building's own space; Feature::Redraw uses it too) and frees it with
`PartialBuild::EraseMesh`.

**Rows of PhysicsConstants** (`GetPhysicsConstantsType` vt +0x788): a fence (`IsFence` 0x609110) is row 18, tested
after the three "rock" tests and before the toys (`MobileStatic` 0x609270); anything else with no override takes
`Object` 0x6376A0 = `CanBecomeAPhysicsObject ? 1 : 0`. Not ported (their classes are not in openblack): Ball 2, Poo 12,
LandscapeVortexIn 13, Scaffold 17, Creature 0, CitadelHeart 0, FieldCrop 6.

**The release prediction** (GInterface state 12 0x5D4E65..0x5D513D, `physics::from_hand::PredictRelease`): at the
release the hand builds a body that is not in the list (PredictionPhysOb 0xD47088) from the held object at its pose,
sets its angular momentum from the hand's angular velocity h (`L = h·M8`, the inertia tensor with no body rotation,
0x5D4EB3..0x5D4F2E; stored negated, because the original's torque is F×r and this port's r×F), caps the hand's velocity
at 124 (the "fast" flag is the uncapped `|v|² > 1`), runs `AdjustToGroundLevel(1, !(tree || fast))` and then
`min(N, 15)` turns of 20 substeps with no other body and no game logic (fn_00644F20), N = min(ping / 100, 5). The 0x4D
packet carries the velocity, L, the origin (fn_007FD140) and the angles after those turns. Until the throw is applied the
held object is drawn along the recorded poses (fn_00646FE0 0x6471D7..0x647446, `PhysicsDrawPose`); the prediction ends
at InitialisePhysicsFromHand (0x636F69), at a turn start when the object is no longer available (0x6451DA) and at
DeleteAll. `from_hand::Throw` takes the packet's L (0x637013..0x637031). In single player N is mostly 0 or 1.
The body starts where the hand drew the object (the 3D object's matrix, see `SetUpPos` above), not at its turn place:

(pending) the caller sets the packet's pose before Throw, and the hand skips drawing the held object while it is the
prediction's.

Release prediction details: `PhysOb::Initialise` 0x7FB780 on the held object, `Object::SetUpPhysOb` 0x6376B0
(SetUpConstants only if CanBecomeAPhysicsObject and not IMMOVABLE); fn_00644F20 stores the history at 0xD46D80 (N at
[0xD47080], turns since at [0xD47084], incremented in `GameTurnUpdate` 0x646053..0x64605B); the drawn origin uses the
scale [0xD4708C] and the com [0xD47180..0xD47188]. The "fast" test is the uncapped |v|² > 1 (0x8AA390).

**A thrown object always has a body.** `PhysicsObject::AddObject` 0x6443A0 only fails for a class that cannot become a
physics object, an IMMOVABLE one (flag 0x1000) or one already flying; the list never refuses. The angle the hand gives
(`ThrowAngularVelocity`, CHand +0x48D4) is the prediction body's L after `min(ping / 100, 5)` steps against the ground
(fn_00644F20): 0 in a single player game (inferred: ping 0), as in openblack. ⚠️ approximate an object
openblack cannot make a body for (no mesh; the original would crash in `PhysOb::Initialise`) is put on the ground
(`physics::from_hand::PlaceWithoutBody`); When `AddObject` refuses a
villager or animal (one a script made IMMOVABLE but still lets be picked up), `Living::InitialisePhysicsFromHand`
0x5EFDF8 ends its physics at once with `EndPhysics(NULL, 1)` (vt +0x790): `PlaceWithoutBody` calls
`villager_physics::EndPhysicsWithoutBody` for both, the hand's and a creature's let go alike. Anything else the
refusal leaves as `Object::InitialisePhysicsFromHand` 0x636F00 made it: IN_PHYSICS and out of the map, in the air where
it was let go (it returns 0 when `AddObject` does, after `RemoveMapObject`; bw1-decomp `Object.cpp`);

**An immovable thing let go hangs (a bug of the original, kept).** `Object::InitialisePhysicsFromHand` 0x636F00 returns
0 at once for an object already IN_PHYSICS (0x636F0A); otherwise it sets IN_PHYSICS (0x636F2F..0x636F3E), takes the
object out of the map cells and calls `AddObject`. When `AddObject` refuses (an IMMOVABLE object, `SET_ID_MOVEABLE`
false, 0x6443BC), it returns 0 with nothing undone. `Living::InitialisePhysicsFromHand` 0x5EFD80 ends a villager's or an
animal's physics at once then (`EndPhysics(NULL, true)`, 0x5EFDF8..0x5EFE00), so they land; `Pot` 0x66DFC3 and
`Scaffold` 0x6EA8CD chain to the Object's and do nothing more. So a thing that is not living, made immovable by a
script and let go by the player's hand (or a creature's, `DoDiscardAction` calls the same function through vt 0x780)
stays where it was let go, in the air, IN_PHYSICS with no entry, out of the map, for the rest of the session: only
`EndPhysics` and a `RemoveObject` that finds an entry clear the flag. a villager or an
animal is put down as before, and an object refused only because openblack has no mesh for it keeps
`PlaceWithoutBody`. The map's own syncing (`map_cells::Sync`) never files an `InPhysics` object newly, so only the
explicit calls move it in or out. Afterwards:
- the hand's press on it takes the 225 ms grab path (IN_PHYSICS without PSYS_FLYING, 0x5D529F), its tooltip is
  "Catch" (0xE80, 0x5D6DA3) and the pick sound of a tree is played (0x5D2899);
- the pick fails: `fn_005DC330` (the object entering the hand) calls `RemoveObject(obj, 0, 0)` for an IN_PHYSICS object
  (0x5DC337..0x5DC346), and with no entry returns 0 (0x5DC35A → 0x5DC3CF). `PlaceObjectInMagicHand`'s failure path
  (0x5DA7C9 → 0x5DA833) ends the action (fn_005D1260) and, while the object is available (vt 0x2C), files it back in
  the map (vt 0x544 `InsertMapObject`, 0x5DA849). It stays IN_PHYSICS and in the air, and every later pick fails the
  same way.

- Object::InitialisePhysicsFromHand 0x636F00 (bw1-decomp src/Black/Object.cpp:447), from the hand (thrower NULL). Living 0x5EFD80 / Villager 0x5EFE90 add FLYING and the disciple around it: FLYING (the class's initialisePhysics) at the end, only when the object does not land (0x5EFDD7..0x5EFDEB).
- The class's part before Object's (Scaffold::InitialisePhysicsFromHand 0x6EA8A3..0x6EA8AD: DestroyThingsInWay)
- A, b (0x5D4E65..0x5D4E82): the old prediction ends (DeInitialise) and the new one is active with the held object before the body is built
- C, d (0x5D4E8C..0x5D4EAD): PhysOb::Initialise(held +0x40, GetScale) and held->SetUpPhysOb(&PredictionPhysOb) (Object::SetUpPhysOb 0x6376B0: SetUpConstants with CanBecomeAPhysicsObject && !(+0x25 & 0x10, IMMOVABLE), SetUpPos from the held pose)
- GInterface state 12 at the release, the physics part (0x5D4E8C..0x5D5105): a body that is not in the list, built by the held object's SetUpPhysOb at its Transform (PhysOb::Initialise 0x7FB780 + vt +0x78C), L from the hand's angular velocity (step e), the hand's velocity capped at 124 (step g), AdjustToGroundLevel(1, !(tree || l2 > 1)) (steps h, i) and fn_00644F20(turns) (step j: min(turns, 15) turns of 20 substeps ZeroForces, GroundAndWater, ContactForces, Integrate, its result ignored; no other body, no game logic). nullopt when no body could be built. turns is the caller's min(ping / 100, 5) (0x5D5061..0x5D507B). GInterface state 12 0x5D4F5F..0x5D5032 (steps g, h): l2 = (z z + y y) + x x of the hand's velocity; above 124 x 124 and not all three 0 it is scaled to 124 (k = 124 / sqrt(l2), float at PC24); fast = the uncapped l2 > 1
- The class's part first (Villager 0x5EFEF0: the dropped resource 0x5EFF01, then Living::InitialisePhysics 0x5EFE10: FLYING; ClassHandlers::initialisePhysicsWithoutBody, the villagers' area), which then calls Object's (0x5EFE78); false refuses the take
- 0x6C9F3B..0x6C9F54: SetXYZAngles(GetYXZ(+0x24)) per class (F14, explosions.md §2.6): a villager, animal or tree lands upright with the atom's yaw, a static / mobile object / pot keeps the atom's tilt. The atom's matrix is the object's GetWorldMatrix (fn_006CA0E0 0x6CA13F: the original's rows, no quarter turn);
- fn_00646FE0 0x6473B4..0x647446, the prediction's drawn origin: the normalised rows (glm's columns) times the scale [0xD4708C], t_i = ((cz m(6+i)) + (cy m(3+i))) + (cx m(i)) with the body's com [0xD47180..0xD47188] (cy in the middle for every i, unlike fn_007FD140), origin = centre - t
- PredictionActive [0xD47824], PredictionObject [0xD47828], PredictionPhysOb 0xD47088, the history 0xD46D80 (N + 1 entries, N at [0xD47080]) and the turns since [0xD47084]
- 0x6451C9..0x645203, after the loop over the list (its jl at 0x6451C3): a prediction whose object is no longer IsAvailable (vt +0x2C at 0x6451DA) ends; else its +0xAC = +0x7C and PhysOb +0x78 = 0 ([0xD47100], 0x645203), not ported: the prediction's body is never stepped again and its drawing uses the history
- 0x609297..0x6092AA: IsFence (vt +0x3CC, MobileStatic::IsFence 0x609110) -> row 18, before the toys
- Object::GetPhysicsConstantsType 0x6376A0 with MobileStatic::CanBecomeAPhysicsObject 0x609320 = 1
- Scaffold::CanBecomeAPhysicsObject 0x6EA910 (the class's handler: not in a workshop slot, no site), else MobileObject's 0x607AA0 (mov eax, 1)
- 0x646B2E..0x646B48: LANDED and the returned object's Pos IsLand -> its DropSfx (vt +0x794); the GameTurnUpdate stop (0x645EB5..0x645F33) never calls it
- 0x6471D7..0x64744F: while PredictionActive and its object has an LH3DObject (+0x40, 0x6471E4..0x6471EF), the object's matrix (+0x14) = history[k] lerped to history[k + 1] by the turn fraction, all 12 floats ((h1 - h0) f + h0, 0x647262..0x647387) with k = [0xD47084] < N (signed, 0x64725A), else history[N] (rep movsd 0x64738E..0x6473A4); NormaliseRows fn_007FB5C0 (0x6473A8), then PhysOb::DrawOrigin (0x6473B4..0x647446). (pending) the Get3DType == 2 branch (0x6471F7..0x647236, an animated mesh's frame through vt +0x18C / +0x184 / +0x188 and fn_005167D0); the draw itself (fn_00874850 0x64744F with the [0xD47200] list) is the renderer's, from PhysicsDrawPose. (pending, Mano) the hand skips drawing the held object while it is the prediction's (CHand:: PrepareForDrawing 0x46CA3A, HandStateHolding_vfunc1 0x5B56D0). Valid, a mesh and a history stand for the LH3DObject test
- 0x5D4E65..0x5D4E73
- Living::InitialisePhysics / InitialisePhysicsFromHand (0x5EFD80): the object flies (FLYING). AddObject calls it once the body is built (fromHand: AddObject's own flag, the debug throws; (approximate, debug path) for those the original's vt +0x784 InitialisePhysics would run with no hand, Scaffold 0x6EA840's ScaffoldMoved included, but they come with fromHand). From the hand (from_hand::InitialisePhysicsFromHand, through InitialisePhysicsOfClass) it comes last, after the whole of Object::InitialisePhysicsFromHand 0x636F00 (AddObject, AdjustToGroundLevel, RaiseUntilNotIntersecting, the LANDED test, RemoveObject or CreateDroppedResource and the flying-object reaction) and only when the object did not land: Living 0x5EFDD7..0x5EFDEB sets FLYING only when the po is not LANDED and po->object is still this one (a put-down villager or animal never enters FLYING).
- PhysicsObject::PredictionActive [0xD47824] / PredictionObject [0xD47828]: GInterface state 12 0x5D4E65..0x5D4E82, the old prediction ends (PredictionPhysOb.DeInitialise) and the new one is active with the held object
- PredictionPhysOb 0xD47088 and fn_00644F20's history (0xD46D80, N at [0xD47080]) of from_hand::PredictRelease, for the drawing; the turn counter [0xD47084] = 0 (0x644FB0)
- Object::InitialisePhysicsFromHand 0x636F69..0x636F89: when the prediction is this object's, it ends (active 0, DeInitialise).

### The physical shield test

Before and after the penetration fix (`len` → `predLen`, see [Engine](#engine)), traced with
`OPENBLACK_PHYSICS_TRACE=1`: a PhysicalShield of radius 40 and a rock (mass 6.75, radius 0.37) thrown at it at
35.5 m/s.

| turn after the throw | before (len) | after (predLen) |
|---|---|---|
| +23 | 33.7 m/s, 21.5 m from the centre, 0 contacts | 33.7 m/s, 21.5 m, 0 contacts |
| +24 | 33.8 m/s, 38 contacts, G 0.12 | 25.7 m/s, 15 contacts, G 8.3 |
| +25 | 33.9 m/s, through the shield | 8.3 m/s, 5 contacts, G 20.4 |
| +26..+32 | lands inside the dome | slides along the outside (14.3..17.7 m, 7.0..5.2 m/s) |

### Collision sounds

- 0x6468AB..0x646919: GGame::GetCamera, then |LH3DTech::g_camera - point| as the distance (fsqrt 0x64690D)
- 0x6466AA / 0x6466B3: PhysicsObject +0x178 = PhysOb +0x150, the body radius (max |vertex - CoM| x scale); rate 1 / r and growth 2 r with no guard
- 0x64686C..0x64688A: the key {level, 0, A (0xBFEE34), B (0xBFEDB0), 75 COLLIDE}

### Release from the hand

- TODO(villager-jobs): GGuidance::MakeDiscipleSFX when the villager's disciple changed (0x6372E8). (to do, creature) ConsiderCreatureMimickingWhenObjectLands (0x637306).
- I (0x5D5034..0x5D505C): AdjustToGroundLevel(1, (IsAnyKindOfTree vt +0x478 || bl) ? 0 : 1)
- 0x63CE07..0x63CE09
- 0x63CE57..0x63CE67
- 0x63CE3B, 0x63CE4D, 0x63CE79, 0x63CE7D
- 0x63CE34, 0x63CE43, 0x63CE73
- += into PhysOb +0x128 (0x63CE87..0x63CEB9), with this port's torque sign
- What GInterface state 12 predicts at the release (0x5D4E8C..0x5D512B), for the 0x4D packet.

### Rigid body details

- 0x7FB84D..0x7FB85B: __CIpow(d4, (double)0.005f [0x9A2BB8]) on the x87 stack, then fstp float. (approximate, last bit) the CRT's x87 pow 0x7C7AE0 at PC24 is not emulated: here a double pow rounded once to float
- 0x7FC77B..0x7FC7E2: the centre of mass offset from the scaled rows, in the original's order
- 0x7FC7E6..0x7FC944: each row times 1 / sqrt((z z + y y) + x x) (fsqrt, fdivr 1.0), unless it is all zero
- 0x7FC959..: the translation += t
- 0x7FC9A4..0x7FC9FC: world_i = ((r2_i z + r1_i y) + r0_i x) + T_i with the normalised rows, then the contact anchor = world (0x7FC9FF..0x7FCA0D)
- 0x7FCD51..0x7FCDC3: ((r2 z + r1 y) + r0 x) + T for x, ((r1 y + r0 x) + r2 z) + T for y and z
- 0x7FCE01..0x7FCE06
- 0x644601..0x644616
- 0x644619..0x644630
- Out = PhysOb +0x7C, rows (glm's columns) times [this + 4] (0x7FD15E..0x7FD16A), then out9..11 -= t
- 0x7FD16C..0x7FD180
- 0x7FD18C..0x7FD1AB
- 0x7FD1AF..0x7FD1CE
- 0x7FD1D2..0x7FD1ED
- The value SetAngularMomentumFromHand stores, for a tensor `inertia` ([row][col] as the original's PhysOb +0x8): -(((I00 hx + I20 hz) + I10 hy) + 0, ((I21 hz + I01 hx) + I11 hy) + 0, ((I22 hz + I02 hx) + I12 hy) + 0); the + 0 is the fadd of the zeroed translation [0xD470B4..0xD470BC] (0x5D4ED2 / 0x5D4EFD / 0x5D4F28), which makes a -0 a +0

### Manager details

- JustMapXZ::InBounds 0x5E1860 (0x645435), then the walked list
- Creature::GetPhysicsConstantsType 0x479B80
- Scaffold::GetPhysicsConstantsType 0x6E8770 (mov eax, 0x11)
- MobileObject::GetPhysicsConstantsType 0x6079F0: Champi, MagicMushroom, Toadstool
- 0x750A54..0x750A66: po+0x90
- 0x750A6D: or [po+0x1D8], 0x10
- 0x750A78..0x750A7F
- 0x750A89
- Obj != po->thrower (+0x1C, 0x644987) && ShouldPhysicsRaiseObjectUntilNotIntersectingThis (vt +0x7A4, = InteractsWithPhysicsObjects, 0 for LandscapeVortexIn / MapShield) && not in the physics list yet && it has a Game3dObject (+0x40, 0x6449D5)
- 0x644AB4: go up by the first push over 0.001 and start again, until nothing pushes. The original has no limit; the 1000 rounds only guard openblack against a body that could never leave another.
- Vt +0x784 (0x63968B..0x6396AA): both points 0
- 0x6396B4..0x6396C0
- 0x639713..0x639740: all three exactly 0 adds nothing; else d x F / sqrt((dz dz + dy dy) + dx dx) (0x639742..0x639774)
- 0x63977A..0x6397A0: PhysOb +0x11C (po +0x144..+0x14C)
- 0x5D4E78..0x5D4E82
- DeleteAll 0x6442F4
- DeleteAll 0x6442B0
- (pending, creature area) the walk of Creature::CreatureList [0xC5FCF8] and each creature's tests and plan
- CreateDroppedResource's part after InitialisePhysics (0x750A54..0x750A89), shared by both AddDroppedObject
- PredictionActive && PredictionObject == object: what CHand::PrepareForDrawing 0x46CA3A and HandStateHolding_vfunc1 0x5B56D0 test to skip drawing the held object in the hand (its PhysicsDrawPose is the prediction's then)

### Sources

- Disassembly of `runblack.exe`.
- `bw1-decomp` `src/Black/Object.cpp:447` (`Object::InitialisePhysicsFromHand`, matched).

## openblack

Status: ✅ faithful for the engine and everything on this page except what [Pending](#pending) lists. Progress rows:
[collisions](../project/progress/physics/collisions.md) (95%), [object dynamics](../project/progress/physics/object_dynamics.md)
(91%), [throwing and landing](../project/progress/physics/throwing_and_landing.md) (83%),
[thrown living things](../project/progress/physics/thrown_living.md) (83%).

- `src/Physics/Body.cpp` (was `PhysOb`): the rigid body, its points against the land (`Body::TouchGround`) and other
  bodies' faces (`Body::TouchFaces`); `src/Physics/TurnRules.*`: the end-of-turn steps as pure functions;
- `src/ECS/Systems/Implementations/DynamicsSystem.cpp` (`DynamicsSystemInterface`): the per-turn manager, reached as a
  Locator service, with `src/ECS/Physics/Hitter.cpp` (the hit record) and `src/ECS/PhysicsGameHooks.cpp` (the class
  hooks); `src/ECS/Physics/FromHand.cpp` (`FromHand`): what a released object does; `src/ECS/ObjectPhysics.*` and `src/ECS/Components/Life.h`;
- the kinds' own parts: `HandPhysics.cpp` (trees, dead trees, piles), `VillagerPhysics.cpp`, `src/ECS/VillagerDrowning.cpp`,
  `src/ECS/AnimalAI.cpp`, `src/ECS/CreaturePhysics.cpp` (a stand-in ball body for the creature);
- a body's state lives in components (`Life`, `BuildingDamage`, `Fragment`, `PhysicsDrawPose`); the formulas
  (contacts, damage, the cylinder cut) are tested with fakes: `test/test_physics_body_pose.cpp`,
  `test/test_physics_body_water.cpp`, `test/test_release_prediction.cpp`, `test/test_living_landing.cpp`.

Where it is in the code (helpers named in the old notes; `PhysicsObjects` is now `DynamicsSystem`, `PhysicsObject`
is `PhysicsEntry`, `PhysOb` is `Body`):

- `DynamicsSystemInterface.h`, the manager:
  - `LoadConstants`: reads `Data\PhysicsConstants.txt`, every column clamped to its own range; `ConstantsType`: the
    class's row of it (0 for the creature, 17 for a scaffold, the mushrooms by type);
  - `CanBecomeAPhysicsObject`, `InteractsWithPhysicsObjects` (a physical shield yes, a magic shield and a field no,
    a building only while standing), `WeightOf` (the class's weight, unclamped);
  - `InitialisePhysicsOfClass`, `ThrowFromHandOfClass`: the class's part before the object's;
  - `FinishDroppedObject`: the end of a dropped resource's set-up (its angular momentum, no object collision, on the
    ground);
  - `PushObject`: the force of a push, weight × 9.81, none for an object a particle system carries;
  - `CanWakeKnockedProxy`: a knocked resting proxy is woken only when it can become a physics object and no particle
    system carries it;
  - `RemoveObject` (out of the physics, back in the map cells at once) and `RemoveObjectWithEndPhysics` (a landed
    object on land plays its drop sound);
  - `RotationFromRows`: the object's rotation from a body's rows (the yaw only for an Object, YXZ for a mobile one);
    `DrawQuarterTurn`: the quarter turn of a villager's or an animal's drawn rotation;
  - `BeginPrediction`, `SetPrediction`, `EndPredictionOf`, `IsPredictionObject`: the throw's prediction, kept for the
    drawing until the throw (the held object is not drawn in the hand while it is the prediction's);
  - `UpdateFrame`: every frame each awake body's pose between the turn's two matrices, only while its centre is
    above minus its radius; `CheckAllCreaturesForCatching`: the creatures' catch (pending);
  - `PlayAnimEffect` (was `CollisionSounds`): a collision sound, its distance from the camera.
- `DynamicsSystem.cpp`: `WorldMatrixRows`, an object's world rows for its yaw and scale, with the original's float
  roundings.
- `PhysicsEntry.h`: the flags `k_FromHand`, `k_PushedByLiving`, `k_NoObjectCollision`.
- `Physics/Body.h`: `ObjectOrigin` (`Body::ObjectOrigin`: the origin T - (s R) com in the original's order), `HandAngularMomentum`,
  `RayBehindVsFaces` (the nearest face behind a point, best starting at -10000).
- `VillagerPhysics.cpp`: `VillagerEndPhysics` and `AnimalEndPhysics` (the landing: pose, land type, yaw, then the
  state, `VillagerStates::Landed` for a villager that lives); `Physics/LivingRules.h`: `VillagerLandingPoseOf`,
  `AnimalLandType` (1 above 0.5, 2 below -0.5 or unordered, else 0), `AnimalLandingYaw` (the current forward row's
  yaw plus pi, wrapped).
- `PhysicsGameHooks.h`: `EndPhysicsWithoutBody` (above); `RegisterPhysicsHandlers` (`HandSystem::RegisterPhysicsHandlers`): the class handlers of
  the hand's kinds.
- `ObjectFlags.h`: `IsImmovable`, the flag 0x1000 (such an object gets no physics); `ObjectPhysics.h`: `IsRock` (a
  rock is not hurt by another rock).
- `MagicShield.h`: the kind `Physical` of `MagicShield` (the heavy shield body, weight 50000).

Porting decisions taken from raffclar's tree (the physics body, the class hooks, the World seams):
[Port decisions: engine and world](../project/port-decisions-engine-world.md#physics).

#### Engine

- openblack once divided by +0x1C (`len`, the unpredicted length), and a rock at 33 m/s went through a physical shield.
- openblack reads it through the resources (`PhysicsMaterialsLoader`, under `physics::k_MaterialsId`, `physics::ParseMaterials`) and, without the file, makes every material zero as the game does (`physics::k_NoMaterials`).

#### Bodies

- openblack: the body's mass, `WeightOf`, stays `creature_physics::k_Mass`; the lift share takes `ecs::creature_physics::Weight` ([The creature](../creature/overview.md), "Carrying", 0x4EF541..0x4EF5DA), the pick-up test the same mass (`physics::living::CreatureMass` in `CreatureObjectActionSystem::PassesPickUpTest`, `GetWeight` read at `CanBePickedUpByCreatureIgnoringWhetherItIsInTheMap` 0x4E3960, [Animals: species and the turn](../beings/animals.md)), and the catch takes `ecs::creature_physics::Weight` too ([The creature's catch](../creature/hits-and-catch.md#the-creatures-catch)).
- openblack guard: the piece's own half height without info.dat

#### Manager

- openblack: `PhysicsObjects::GameTurnUpdate` from `Game::GameLogicLoop`.
- openblack keeps the hitter as a slot of the list (`PhysicsEntry::hitBySlot`, read through `ecs::physics::hitter::HitterEntry`), and takes lastHit as a slot when the end loop starts (`lastHitSlot`), so both name the slot as the original's do.
- openblack does the same (`DynamicsSystem::ProcessTurn`, `_inTurnUpdate`), with these differences:
  - an object deleted now is destroyed at once, where the original's is only marked until the dead list frees it;
  - so for an entry whose object is already gone there is no `EndPhysics`, no `HasSunk`, no restart when knocked and
    no reaction, and such an object is passed to reactions as no hitter;
  - `Find` and `ForEachEntry` pass over such an entry.
- openblack: `turn::CountedImpact`, and `FindHitter` only after the test; 5. impact = √|ΣF|²·0.05 (0x64624A..0x646266); 6. hitBy (+0x20): 0 when lastHit (+0xA0) is null (0x6462CA).
- openblack: `turn::LookUpHitter` (None / Keep / Slot), with the list's growths (`physics::State::growths`) compared with their number when the loop starts;
- openblack: `physics::turn::EndOfTurn` (`src/Physics/TurnRules.*`) runs the steps in this order over `DynamicsSystem::EndTurnSteps`. The record is `DynamicsSystemInterface::RecordHit`, `GetHitObject` and `GetObjectWhichHit`, read back as kept (no second availability test). The deeds go through `creature_mimic::Consider` with the local player's hand. Where openblack still differs is in Pending below.
- openblack (`src/CHLApi.cpp`): `GET_HIT_OBJECT` / `GET_OBJECT_WHICH_HIT` push `GetHitObject` / `GetObjectWhichHit`, each added as a found script thing (`ScriptObjects().Register(thing, false)`), or 0; `CLEAR_HIT_OBJECT` is `RecordHit(null, null)`; `GAME_THING_HIT` pushes whether the popped thing is `GetHitObject`, and false with "Object no longer valid" (kept as openblack's own diagnostic) when the thing is gone. raffclar's tree wires the first three without adding the thing to the script table and with an availability test at read time; ours follows the exe. Tests: `test/test_script_hit_natives.cpp` (group test_ui_script).
- openblack: the Pot class's `reactToImpact` (`HandPhysics.cpp`, `held_apply::PotImpactTakes`).
- openblack: `villager_physics::ImpactEffectOf` builds the effect (`physics::living::LivingCrush` the share, `effects::EffectPlayer` the player) and `effects::ApplyEffect` takes it, from `LivingReactToPhysicsImpact` (`VillagerPhysics.cpp`).
- openblack: `DynamicsSystem::EndPhysicsOfClass(po, turnStop)`.
- openblack keeps it as the `InPhysics` component (`ECS/Components/Physics.h`, raffclar's name).

#### Who owns what: class hooks and the hand's API

- openblack keeps that split with the virtual class `PhysicsClassHooks` (`DynamicsSystemInterface::SetClassHooks`, set once where the game starts): the game's `ecs::PhysicsGameHooks` picks the class (`DynamicsSystem::ClassOf`: Villager, Animal, Tree, DeadTree, Pot, Rock, Fragment, Building, Shield, Creature, Scaffold, Other) and calls the code that owns it.
- openblack's carried object has `InPhysics` and `CarriedByTornado` at once, as the original has both bits
- openblack: `PotEndPhysics(pot, entry.byPlayer)`; test `test_particle_carried_objects` (APotThatLandsIsPutDownByTheInterfaceThatThrewIt).
- openblack: `PhysicsGameHooks::EndPhysics` (DeadTree, which puts it back in the map itself) and `DeadTreeEndPhysicsReaction(object, entry.byPlayer)`; test `test_particle_carried_objects` (ADeadTreeThatLandsTellsTheVillagersWithItsThrowersPlayer).
- openblack does not crash: where the original reads the missing record, it goes on as for a record that names no interface, no player and no flags (what `GetPlayer` itself returns for a record with no interface status). So a fixed object is not taken as a town artifact (the Object's end); a hand's pile of wood or food on land becomes a land pile put down by no interface (no player for a new pile, no creature deed, no drop sound), and in the water it ends as any object; a dead tree tells the villagers "wood here" with no player (a felled tree does not); a scaffold has the snap and its own state (no holder, placed now, dropped-gently cleared), but no build and no puff, as it is not from the hand. The code: `PhysicsClassHooks::EndPhysicsWithoutBody` (villager, animal, tree, any object) and `PhysicsGameHooks::EndPhysicsWithoutBody` (pot, dead tree, scaffold, fragment). A fragment has its own end with no body: its flying-object reactions go, the object's end, and a piece that still sticks to its building does so through the object's own matrix (its rows and place), the nearest openblack has to the missing record's.
- openblack's `PredictRelease` takes the hand's drawn pose, sets it on the Transform for the setup and puts the turn's place back after (issue #134, hand-and-interface.md).
- openblack's old ballistic flight for it is gone.
- openblack still puts it on the ground (pending below).
- openblack does the same (`from_hand::LetGoWithoutBody`: `InPhysics`, out of the map, the Transform left at the hand's pose)
- openblack: `from_hand::TakeOutOfPhysicsForHand`, from `HandSystem::PickUp` after a pile has swapped in the hand's own pile (a pile's `InterfaceSetInMagicHand` 0x66EF80 puts a new hand pile in the hand, which is not in the physics, so a hanging pile can still be scooped from);
  - a tornado or a vortex cannot take it (`InitialisePhysics` refuses IN_PHYSICS, 0x637487), a push gives it no force;
  - in the original a save and a load end it (`ResolveLoad` 0x63A020 clears the flag and its InitialisePhysics refuses the
    immovable object); openblack saves no physics state.
- openblack's atom carries a Living's DRAWN rotation (its Transform), so OriginalRows takes the quarter turn off first, and the result gets it back (livingaxes: fn_0051AF00 draws it on the ground)
- openblack guards

### Test hooks

`OPENBLACK_TEST_PHYSICS="x,z,height,vx,vy,vz[,scale[,n]]"`, `OPENBLACK_TEST_HIT_VILLAGER="speed[,scale[,index]]"`,
`OPENBLACK_TEST_THROW_TREE="x,z,vx,vy,vz"`, `OPENBLACK_TEST_HIT_ABODE="speed[,scale[,index[,n]]]"` (n rocks against a house, one every 1.5 s), `OPENBLACK_HAND_TEST_SPLIT="x,z,scale,rounds"`,
`OPENBLACK_PHYSICS_TRACE=1` (position, velocity, contacts, G, density and radius of each body per turn),
`OPENBLACK_TEST_SEA="x,z,type[,height]"` (type = `villager|animal|tree|pot|rock`: creates it at that height above the
point and puts it into physics with no velocity; writes the cell, the density and `GET_LAND_HEIGHT` there and on the reference
land, and traces the counter of the drowning villager every 100 turns),
`OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"` (the hand holds a rock 0, a pot of 300 food 1 or of wood 2,
the first villager 3, the first tree 4 or the first animal 5, and **drops it gently** at (x, z) after those seconds of game time; the log says whether it ended up in physics).

## Pending

- **Verify with xref/oracle:** the 2 **🔍 inferred** and 4 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- The readers of IN_PHYSICS a hanging thing (see "An immovable thing let go hangs") reaches outside the hand still read
  a moving body (`IsFlying`) or an entry (`Find`): the scripts' GET_PROPERTY Flying (0x70DCFA, the bit itself) and
  SET_POSITION (0x6F8A92, refused "Object is flying"), the villagers' food reaction (0x5F1732, a hanging pot is
  ignored), the fire's list (0x72F729), `Rock::SplitInTwo` (0x6E75E5, a hanging rock is not split), the scaffold's
  tap and validity (0x6E9D8D, 0x6E991A), the forester (0x75FB0D, 0x764777), the land following of a pile
  (`LandReseat`), the creature's choices (0x4884E1, 0x501115) and the release from a script (0x70F6EC). Each also
  changes its answer for a carried object: a behaviour commit of its own.
- The original crashes when the hand taps a hanging scaffold: `fn_006E9EA0` (from `Scaffold::InterfaceTap` 0x6E9DEB)
  looks up the physics entry of an IN_PHYSICS scaffold and reads `[eax+0x90]` (0x6EA06E) before testing it (0x6EA07D).
  openblack's split (`scaffolds::Split`) reads a body only when it finds one, so it does not crash.
- Can a rock burn at all? A fire set on a rock (the testbed's fires fixture) stays at T 24.7 against its combustion
  point of 500 and never takes, so no rock splits while lit and the split's fire order (halves take the fire after they
  start to fly, as the original) shows in no case reachable today. The rock's combustion values are not read yet. Fix row F12 (the testbed's fix queue).
- The end of the turn on an entry whose object openblack has already destroyed (see the Manager section). The entry
  is walked and keeps its place, and it is still found as another entry's hitter. But:
  - it has no reaction and no deeds;
  - a body it hit gets its collision sound with no sound type for it (the original reads the marked object's);
  - a reaction is given no hitter for it. Fix row F5 (the testbed's fix queue).
- A kept hitter slot at or past the count, read after a growth in the end loop: the original would read the slot's
  leftover bytes (object +0x18, status +0x24, the body), which after a growth are whatever the allocator left (the
  constructor 0x644330 does not set them). Not reachable (step 6 of the end of the turn): openblack's
  `hitter::HitterEntry` stops the game with a message if it is ever read. Fix row F2a, F2b (the testbed's fix queue).
- Our Add can fail at the body set-up after it has taken a resting entry out, so the count falls inside the end loop;
  the original always appends (0x64458D / 0x644590). Fix row F2b: make the append unconditional, measured on the
  physics scenarios.
- Whether a growth of the list inside the end loop happens in the physics scenarios (building breaks, many throws):
  not measured; the count and the capacity at each end loop can be logged with the physics trace.
- The end of the turn's snapshot of what hit the body, taken before its reaction (`_reacted`): the original reads slot
  i again after the reaction (0x646363), which is another entry when the reaction's `AddObject` moved the last entry
  into slot i. Fix row F2c (the testbed's fix queue).
- LocalRand in the reactions: a Living's `ApplyEffect` path draws LocalRand only in the town attack's guidance remark
  (`Town::UpdateAggressor` 0x73CA76 → `GGuidance::TownAttackSFX` 0x71B7C0: `PlayNow(2)`, then with the help level at
  1 or more `LocalFloatRand(1.0)` 0x71AEE8 and `LocalRand` 0x71AF34, and one more `LocalRand` 0x71BB78 for the sample
  when it plays). openblack's `town_emergency::AttackTown` does not call the remark yet, so it draws none. The felled
  tree's sound (LocalRand) and the collision dust (LocalRand) now come entry by entry, as in the original. Fix row F11 (the testbed's fix queue).
- The hit natives read back a recorded object that was destroyed after the hit as its stale id (a later native finds
  it gone); the original keeps a raw pointer there, so what it gives in that case depends on where it clears +0x70 /
  +0x74 (below). Fix row F13 (the testbed's fix queue).
- The deeds 0xF and 0x10 at the end of the turn: openblack's credit is a flag of the local player's hand, so the player
  is always the local one. The object of an entry whose reaction already destroyed it gets no deed (the original's is
  only marked). Which classes answer `IsAnyKindOfTree` is not checked; openblack tests the Tree component. Fix row F8 (the testbed's fix queue).
- The hit record is cleared with the simulation (a new land), as in raffclar's tree; where the original clears GScript
  +0x70 / +0x74 besides `CLEAR_HIT_OBJECT` is not checked.
- (not verified) the angular damping step `pow(d4, 0.005)` of `PhysOb::SetUpConstants` 0x7FB810: openblack takes the
  float overload of `std::pow`; the original's CRT pow under the game's 24-bit FPU precision (fn_007DEE00, 0x7DEE0D) may
  differ in the last bit. Fix row F16 (the testbed's fix queue).
- (not verified) `AttemptCollisionSound`, verbatim: "0x6466AA / 0x6466B3: PhysicsObject +0x178 =
  PhysOb +0x150, the body radius (max |vertex - CoM| x scale); rate 1 / r and growth 2 r with no guard". Not clear what
  "rate" and "growth" refer to (perhaps the dust puff's growth).
- Snow on the FragMesh (needs the weather: snow storms and the 128×128 snow map). Fix row F15 (the testbed's fix queue).
  What it reads (`FragMesh::Draw` 0x7F79A3..0x7F7A13): +0x1C the snow drawn (0..255, drawn above 5), +0x20 no snow,
  +0x24 the snow held: while set +0x1C is not read again from the snow map (`__ftol(depth) − 20`, kept within
  0..255), and every frame it moves at most `g_game_time_inc >> 4` towards that. A piece starts with its building's
  +0x1C and +0x24 = 1 (`FragPrimitive::Impact` 0x76E4AD..0x76E4B9), and its end of physics clears +0x24 when it does
  not stick (0x76F5BF): a flying piece keeps its building's snow, a landed one takes the snow where it lies.
- A building piece too thin to fly (the sliver test of `Fragment::SetUpPhysOb`) is marked `ToBeDeleted` with its
  body already in the list, and both keep that body until the next turn start, which moves the last body into its
  place. openblack's fragment entity is destroyed at once, so the body stays with no object. Whether the rest of the
  original's turn (the end loop, a later piece of the same break) does anything with the sliver body is not checked.
- Entries kept until the next turn start whose object openblack destroyed at once (see the Manager section): the
  original still runs that object's `EndPhysics`, `HasSunk`, knocked restart and reaction, and uses it as a hitter
  (its collision sound type, a building's damage, a reaction's hitter). openblack can do none of these while the dead
  list's deferral is off. Fix row F5 (the testbed's fix queue).
- The snow size of a building piece's dust, `2·(1 + snow/255)` (0x76EF57..0x76F012): openblack has no snow map, so it
  is always 2. Fix row F15 (the testbed's fix queue).
- The plain `RemoveObject` (without the end of physics): the exe sets the object's angles and its place through
  MapCoords from the body's pose there too (0x646A41..0x646B08); openblack takes the entry out and leaves the object's
  Transform as the last turn's end left it. Fix row F26 (the testbed's fix queue).
- Villagers and animals on landing: the original's three postures (see the end-of-flight bullet above and
  [Villagers: data and states](../beings/villager-states.md)); whether openblack draws all three is not verified.
- From dropping: the disciple sound
  (`MakeDiscipleSFX`) and `SetVillagerDisciple`, reaction 9, the villager's log and the creature stuff (catching,
  imitating, toys). The mesh branch of fn_007FDD60 (fn_008683C0) is not needed: no openblack body uses it.
  The `Tree`+0x5C & 2 flag of `Tree::EndPhysics` (0x74B882: only `Fixed::EndPhysics`, neither replanting nor dead
  tree) is set only by the `MagicTree` constructor (0x5FCF8D, `or byte [esi+0x5C], 2`; found by a sweep of the whole
  `.text`): they are the trees of the forest miracle, which this tree does not have.
- The terrain normal without the original's quantisation. Fix row F6 (the testbed's fix queue).
- Tooltip "Golpear para Romper" ("Hit to Break", 0xEF7) and checking the player's influence before hitting a rock. Fix row F14 (the testbed's fix queue).
- (unclear state) the invented splash cool-down: it is to be removed, as made up and not in the original (fidelity
  pass of the physics). No page describes it and no cool-down was found in `src`; it may have been removed already
  with the water port. Check before closing it. Fix row F25 (the testbed's fix queue).

