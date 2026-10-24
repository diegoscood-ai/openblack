# Physics impacts

## Summary

What happens when a flying thing hits: a building struck hard enough breaks into pieces that fly in the physics and
leave a damaged model behind; every impact plays its collision sound, raises dust or a splash and can wake what it
touches; a thing that falls into the water splashes and rings; and a rock hit hard enough splits in two.

openblack ports the building breaks (the damage meshes, the pieces, the rebuild on a blow), the sounds, the dust, the
water impacts and the rock split from the original; the engine and the per-turn manager they rely on are on
[Physics](physics.md).

## The original

The breaking of buildings is `Abode::ReactToPhysicsImpact` 0x406240 with the pieces of `FragMesh` (from 0x7F6F00); the
rock split is `Rock::SplitInTwo` 0x6E7560.

### Buildings that break

- Only rocks (row 3) and the row-20 toy break buildings, with `p = |v|·masa` of the hitter:
  **p > 2000** breaks; 1000–2000 and 300–1000 only make sound (`editor.sad` 431–436 and 437–442).
- On the first hit the building is copied into triangles in world coordinates (LOD 0 submeshes). An **infinite
  cylinder** through the rock's position, in the direction of `0.3·v` and with radius `R + 0.7`, decides:
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
  each open edge, lit per face and two-sided on the CPU every frame as the original does (`fn_007F7ED0`; see
  [Model lighting](../rendering/model-lighting.md#model-lighting)); its own generated mesh (with the partly built part
  over it) stays for the reflections. That mesh is the building's `components::DrawMesh` (the drawing-only model, the
  same one a building site's partly built model uses); the `Mesh` component stays the intact model, so the
  physics body, the map cells, the sizes and the static shadow keep it. A hit that leaves the whole building
  (remaining 1) takes the DrawMesh away and hands the draw back to `abodes::RedrawConstruction`. If the same rock
  hits it again, they stop colliding (it passes through on the third contact).
- **Pieces**: body = its distinct vertices and a copy of each one 0.45 behind (no faces: nothing collides with them),
  around its origin; the original's ×2 (0x76F2DB) goes to the **drag**, not to the inertia; the rest counter uses
  the half height of the rock mesh from info.dat. They are created inside `FragMesh::Impact`, before reading what remains
  (so a hit that only splits triangles also releases pieces). A building forgets the rock that hit it when the rock
  stops or is picked up, or when its body is rebuilt (fn_646D60, Abode::SetUpPhysOb); see the next bullet.
- **Who forgets a building-breaker** (the pairing of a rock and a building it struck: FragMesh +0, the last hitter,
  and the entry's +0x1C, the pass-through partner). Only for an object that `PhysicallyDestroysAbodes` (vt +0x7A8),
  only on entries of the physics list whose object is an Abode (`__RTDynamicCast` to `Abode`: StoragePit, TownCentre
  and the other subclasses too, with no availability test), and for each such entry: its FragMesh's last hitter is
  cleared if it is the object, then its +0x1C is cleared if it is the object (0x646D7B..0x646DBF). It is done at
  two places:
  - on stop: fn_00646D60, whose only caller is `GameTurnUpdate` 0x645D37, before the stop's `SetXYZAngles` and
    `EndPhysics`;
  - in `RemoveObject` 0x646A00, the same test inlined over the other entries (0x646CE7..0x646D2F); `RemoveObject`
    does nothing during the turn (see the Manager section).

  No other code clears the pairing: `AddObject` taking out a resting entry does not, an object that breaks no
  building is never forgotten (a piece keeps its building as its partner), and an abode with no entry in the list
  keeps its last hitter. So a rock that passed through a building, taken by the hand and thrown again, hits it again.
  Tests:
  `test_physics_forget_hitter`.
- `CreateFragment` 0x76EB20 + `Fragment::SetUpPhysOb` 0x76EC50 make a flying piece; its life of 100 turns per triangle
  is set in the Fragment ctor (0x76E9FB).
- `FragPrimitive::Impact` 0x76E2D0: right after `CreateFragment` (0x76E619) three `GameRand(201)` are drawn and dropped
  (0x76E7B1..0x76E7F8), even if the split left the piece no triangles; then the spin `(GameRand(201) − 100)·0.01` per
  axis, z first and x last (0x76E83B / 0x76E864 / 0x76E88D). `SplitUnconnectedGroups` 0x76DC30 draws its spin the same
  way ×0.02 (0x76E120..0x76E172).
- **The order of a break** (`FragMesh::Impact` 0x7F7D40: each primitive's `FragPrimitive::Impact` 0x76E2D0, then the
  building's own `SplitUnconnectedGroups(true)` 0x76DC30 once, 0x7F7D83 / 0x7F7E16). For each broken primitive:
  1. `CreateFragment` fn_0076EB20 (0x76E619) makes the piece and at once calls `SplitUnconnectedGroups(false, parent)`
     on the piece's own mesh (0x76EBBC). Each loose part becomes a Fragment there and then, through `CreateFragment`
     again (0x76E0E1), so its own loose parts come first (recursively). Then its spin ×0.02 (0x76E120 / 0x76E149 /
     0x76E172; 0.02 is 0x8CF178, no dropped draws here) and `InitialisePhysics` (0x76E1BA).
  2. The piece is centred again on what is left (0x76E61E..0x76E7AB).
  3. Three `GameRand(201)` dropped (0x76E7C0 / 0x76E7DC / 0x76E7F8).
  4. Its spin ×0.01 (0x76E83B / 0x76E864 / 0x76E88D; 0.01 is 0x8C5840), z, y, x.
  5. `InitialisePhysics` (vt +0x784, 0x76E8D9), then the entry's flags |= 0x10 (0x76E8E5).

  `InitialisePhysics` adds the body, then runs `Fragment::SetUpPhysOb` 0x76EC50. That walks the triangles of the piece's
  **first primitive** and their corners. The first corner found is stored with **no dust** (0x76EDC5..0x76EE01). Every
  later corner not yet listed (exact equality, 0x76EE12..0x76EE4B) gets a puff: `(GameRand(201) − 100)·0.02` × 3, drawn
  z, y, x (0x76EEC5..0x76EF53), added through fn_00845C20 (0x76F028). Then the mass (`30·area`, at least 0.01,
  0x76F069..0x76F086) and the hull, and **only then** the sliver test, `0.2·R > area / (2R)` → `ToBeDeleted`
  (0x76F29D..0x76F2C3). So a sliver draws its dust and has its body before it goes. Since a child's `InitialisePhysics`
  runs inside its parent's `CreateFragment`, **children come before their parent** in the physics list. The synced
  draws of one broken primitive are therefore: for each loose part (recursively) its children's, its spin, then
  3 × (distinct corners − 1) for its dust; then the 3 dropped draws; then the piece's spin; then its dust. The
  building's falling parts follow, each like a loose part.
- `FragMesh::Impact` is at 0x7F7D40; the back offset 0.45 is 0x8C7C78, the shared-side tolerance 0.01 is the double at
  0x8C7A10.
- A fragment forgets a parent that is no longer available (`Fragment::ProcessTimer` 0x76EAF3..0x76EB06);
  `Fragment::EndPhysics` 0x76F440..0x76F455 only touches the parent if it is available.
- A burning broken building draws its FragMesh with the fire's charring grey (fn_00730570) and glow
  (`GetFireEffectCharingColor` 0x730480), both with alpha 0xFF; otherwise 0xFFFFFFFF / 0 (`Abode::Draw`
  0x5160A6..0x5160E9). A fragment's FragMesh keeps the ctor's 0xFFFFFFFF / 0 (fn_007F6EE0 0x7F6EE5): nothing sets it
  again.
- `FragMesh::Draw` fn_007F7960 → fn_007F7ED0 per primitive in its own material; it sends batches every 256 vertices or
  triangles (0x7F869D..0x7F86D8). The tint flag 0xC371B4 is 1 in .data and nothing writes it.
- Repair draw: `GetPercentRepairedFromWhenDamaged` 0x52F010 = (life − site +0x640) / (1 − site +0x640), with the repair
  site `Abode::ReduceLife` makes at the hit (0x405E7A); `MultiMapFixed::RemoveDamage` (vt +0x8B8, from `Repaired`
  0x52EC70) removes the DestructionMesh and the whole model is drawn again.
- **Rebuild on a blow** (`Abode::ReactToPhysicsImpact`, only for p > 2000 and `IsBuilt`, vt +0x890 at 0x4062E8). The
  draw share is `GetPercentForDrawBuilding` (vt +0x898 = `MultiMapFixed` 0x52EFD0): the smaller of how far it is
  built and `GetPercentRepairedFromWhenDamaged` 0x52F010. That is 1 when not built; with a FragMesh (vt +0x8B4) and a
  site (+0x74), (life − s) / (1 − s) with s = site +0x640 (0 when either side is 0); otherwise life · 0.98
  (0x8CF3FC).
  - No FragMesh (+0x90 = 0): a new one from the 3D object (0x40638F..0x4063B8).
  - A FragMesh and **0.2 ≤ share ≠ 1**: it is deleted and a new one made from the intact model (0x40630E..0x40635F),
    before the blow strikes. The new FragMesh has no last hitter (FragMesh +0), and the blow is **not** recorded as it
    (the jump past 0x406363), so no pass-through pairing starts on that blow.
  - Otherwise the same-hitter test. Its 1400 test (0x40636C) is always true on this path (p > 2000); the other test,
    the entry's object being the abode itself (0x40637D), is false only for a blow passed on by a citadel.
  - The sites: `Abode::ReduceLife` 0x405D90, once the new life is below 1, adds one with `Town::AddBuildingSite` when it
    has none and has a town (0x405E5E..0x405E7A), then, with a site and built, sets site +0x640 = 1.1·life − 0.1
    (0x405E86..0x405EA7). Right after a hit the share is therefore 0.1(1 − L) / 1.1(1 − L) = **1/11**, below 0.2: a
    town's building is rebuilt only once its villagers have raised its life (`MultiMapFixed::Repaired` 0x52EC70 later
    deletes the site and calls `RemoveDamage`). A building with **no town** never gets a site; its share
    is life · 0.98, so every later blow that finds it at life ≥ 0.204 rebuilds it from the intact model.
    The new damage mesh is baked onto the landscape when the building followed it before its first break (openblack
    takes `MorphWithTerrain` off a broken building). Tests: `test_damage_mesh_rebuild`.
- The player of a hit is `PhysicsObject::GetPlayer` 0x647460 of the **hitter's** body (`[arg +0x20]`, 0x406261); with a
  player and FROM_HAND on the **abode's own** body (`test byte [arg +0x1D8], 4` with arg the abode's entry, 0x406273),
  `ConsiderMakingCreatureMimicPlayer(hitter's status +0x24, DAMAGE_BY_THROWING_AT 16, the abode, 0)` (0x406286). It runs
  **before** the `PhysicallyDestroysAbodes` test (vt +0x7A8, 0x406291), so any hitter counts. The FROM_HAND bit is not
  inherited from the hitter (the turn copies only the status +0x24 into a hit body, 0x64629C..0x6462A7) and a resting
  proxy starts with flags 1 | (vt +0x7B4 & 1) << 7 (`AddProxy` fn_00644DF0, 0x644EC2..0x644EE6), so for a building the
  test is never met in practice. `StoragePit` 0x733730 and `TownCentre` 0x744380 only forward to the Abode's. A creature
  thrower is tested at 0x4064BA..0x4064DA.
- **Pieces**: row 11, mass `30·área`, they only collide with the ground, cannot be picked up, last 100 turns per triangle
  (no fade). Very thin ones (area < 0.4 R²) are deleted as they are created, after their body and their dust. A large one (area > 9) that falls while the
  building is still standing stays as rubble of the building.
- **A piece's end of physics** (`Fragment::EndPhysics` 0x76F3F0, read in the v1.42 exe): its flying-object reaction
  (9) is removed (0x76F3FA), then `Fixed::EndPhysics` (0x76F40E: back in the map cells, or `ToBeDeleted` outside
  them; its town-artifact part is not ported for any fixed object). With a FragMesh (+0x94), an area (+0xA0) above 9.0
  (0x76F42F, the float at 0x8FFEC8), a parent (+0xA4) that is available (vt +0x2C) and has a FragMesh (+0x90): the
  parent's primitive list grows by one, the piece's **first** primitive goes last (0x76F473..0x76F585), each corner
  of its triangles (stride 0x58, three corners of 0x14 at +4) through the record's matrix, the body's rows at
  +0xA4..+0xC4 and its centre at +0xC8..+0xD0 (body +0x7C / +0xA0, not the object's origin):
  x = ((v.x r0.x + v.z r2.x) + v.y r1.x) + c.x, y = ((v.z r2.y + v.y r1.y) + v.x r0.y) + c.y,
  z = ((v.x r0.z + v.z r2.z) + v.y r1.z) + c.z (0x76F4CB..0x76F542); the piece's FragMesh keeps no primitive
  (0x76F591) and the piece gets `ToBeDeleted(0)` (0x76F59E). Otherwise its parent is cleared (0x76F5AF) and its
  FragMesh +0x24 too (0x76F5BF, the snow hold, see below). With no FragMesh nothing is cleared. It returns what
  `Fixed::EndPhysics` returned (0x76F5A1), the piece, also when it merged and is marked for deletion; openblack
  destroys a merged piece at once and keeps no resting body for it, as for the other objects it destroys at once
  (row F5 below).
- Quirk of the original: the rubble counts again as building triangles, and if after a hit the count
  reaches 1 the FragMesh is deleted and the house is drawn whole. It is kept like this on purpose, as in the original.
- A building that is not built (`IsBuilt` vt +0x890 = 0 at 0x4062E8, a building site) gets no FragMesh: the hit goes
  straight to `ApplyEffectsDueToPhysicalDestruction` 0x406640 (`abodes::OnPhysicalDamage`, see
  [Life and damage](../world/buildings.md#life-and-damage), where the
  effect preset × defence crush 0.2 takes 0.2 off the percent built), with the player of what hit it
  (`PhysicsObject::GetPlayer` 0x647460: the hand's status +0x24, inherited) and whether a creature threw it (0x4064BA;
  not ported, no creature).
- Pending: creature hits (`ConsiderMakingCreatureMimicPlayer` 0x406286). The rebuild on a blow is done (above); no
  testbed scenario shows it yet: every abode the testbed places belongs to a town and no scenario has villagers repair a
  broken one, so its share stays 1/11 between blows.

- Abode::Draw 0x515F70: the FragMesh, then the intact model partly built at GetPercentForDrawBuilding. (pending, with the repairs of V11) the FragMesh made anew from the intact model when the percent is in [0.2, 1) at the next hit (Abode::ReactToPhysicsImpact 0x4062FA..0x40633C) DrawBuilding 0x517F90: pct = GetPercentForDrawBuilding (vt +0x898, MultiMapFixed 0x52EFD0) -> GetPercentRepairedFromWhenDamaged 0x52F010: with the DestructionMesh and the repair site Abode::ReduceLife made at the hit (0x405E7A), (life - site +0x640) / (1 - site +0x640) (session buildings' abodes, V11)
- 0x406261: player = hb->GetPlayer (PhysicsObject::GetPlayer 0x647460: the GInterfaceStatus +0x24 of the hand that threw it, inherited by what it hit)
- (not ported, no creature) 0x406273..0x406286: with the player and the proxy's FROM_HAND flag, ConsiderMakingCreatureMimicPlayer(status, DAMAGE_BY_THROWING_AT 16, this, 0) (not ported, no creature) 0x4064BA..0x4064DA: the thrower (po +0x1C, else hb +0x1C) is a Creature -> byCreature
- Fragment::EndPhysics 0x76F440..0x76F455: the parent (+0xA4) must be IsAvailable (vt +0x2C at 0x76F450)
- Fragment::ProcessTimer 0x76EAF3..0x76EB06: a parent (+0xA4) no longer IsAvailable (vt +0x2C at 0x76EAFF) is forgotten
- Abode::Draw 0x516080..0x5160E9: the DestructionMesh +0x90 (only while it is what the building draws)
- 0x5160A6..0x5160D8: with a fire (+0x44) +0x10 = fn_00730570 (the charring grey) and +0x14 = GetFireEffectCharingColor 0x730480 (the glow), both with alpha 0xFF (0x7305DE, 0x73055A); else 0xFFFFFFFF / 0. The same pair as the other burning objects (RenderingSystem's Burning)
- 0x5160DB..0x5160E9: no matrix (the triangles are in the world), the position of the LH3DObject (+0x40 + 0x38)
- Fragment::Draw 0x76EC00 (GetWorldMatrix vt+0x63C, then its translation) and PhysicsObject::DrawAll 0x646E57.. 0x646E77 (the LH3DObject's matrix +0x14 and translation +0x38): the same matrix; the FragMesh +0x94 keeps the ctor's 0xFFFFFFFF / 0 (fn_007F6EE0 0x7F6EE5..0x7F6EEC), nothing sets it again. While it flies that matrix is the pose between its last two turns (fn_00646FE0 -> fn_007FCE80, components::PhysicsDrawPose), the scale Transform's
- This frame's draws of every FragMesh (FragMesh::Draw fn_007F7960, pass 0): a broken building's from Abode::Draw (0x5160A6..0x5160E9: no matrix, its position, the fire's charring grey fn_00730570 and glow 0x730480 or 0xFFFFFFFF / 0) and a fragment's from Fragment::Draw 0x76EC00 / PhysicsObject::DrawAll 0x646E77 (its world matrix and translation, the pair of the FragMesh ctor fn_007F6EE0: 0xFFFFFFFF / 0) = SnapshotFragMeshes + AppendFragMeshes(out, draws), in one thread `turnTime`: the turn and its fraction the fire glow is computed at (SnapshotFragMeshes)
- 0x76EEC5, 0x76EEEE, 0x76EF1A (ViscousLiquid.cpp line 0x27D): z (stored 0x76EF4A), y (0x76EF35), x (0x76EF53)
- The same on the synced stream, a fragment's puffs (Fragment::SetUpPhysOb 0x76EEC5..0x76EF53): GameRand(201).
- [0x8C7C78] = 0x3EE66666 (fn_007F7ED0 0x7F83BE)
- (GameRand(201) - 100) x scale on each axis, z drawn first and x last: the LHPoint's arguments are evaluated right to left (Impact 0x76E83B / 0x76E864 / 0x76E88D x 0.01 into +8 / +4 / +0 of the spin, SplitUnconnectedGroups 0x76E120 / 0x76E149 / 0x76E172 x 0.02 likewise)
- 0x7F7F64..0x7F7F86: [0xC371B4] (1 in .data, never written) and not the pair [0xC371B0] = -1 / [0xE9A0F8] = 0
- Fragment::SetUpPhysOb (0x76EC50): the triangles' distinct vertices (exact match), each with the normal of the first triangle it was found in.

### Sounds, dust and the look of impacts

- **Collision sound** (`AttemptToAddSoundEvent` 0x6464F0), once per turn on each awake body with something that
  hit it or `F > 0.5·m·g`: collision type of each side (info `collideSound`; piece = BUSH; DeadTree mesh 406 =
  HOLLOW_WOOD; no object = GROUND, or WATER in the sea), level by `g = impact / (unscaled info weight · 9.81)`
  (3 if < 1.25, 1 if > 3, otherwise 2), and `GAudio::SamplePlayAnimEffect(object, |g_camera − point|, {level, 0, A, B, 75},
  0, editor.sad, track = A ≠ 0x16)` (0x646919): the sample is chosen by the `editor.sad` animation table in the audio
  core ([The world's callers on the channels](../audio/script-audio.md#the-worlds-callers-on-the-channels)), in 3D on the object, which is the owner
  of the channel. A pair does not sound again until two turns later. A rock against a building: the building sounds.
- **Buildings**: medium hit {2, 0, 0x16, 0x10, 75} and weak {3, …} (`G_Rock_V_Ground_M/S`, 0x406610), collapse
  {1, 0, 0x16, 9, 75} (`G_Crash_Abode`, 0x40671D), in 3D with the building as owner. (Corrects 431–447, which came from reading
  the bank table with a one-column offset.)
- Collision codes: tables A (hitter) 0xBFEE34 and B (hit) 0xBFEDB0, one entry per SOUND_COLLISION_TYPE; the key is
  built at 0x64686C..0x64688A. The pair list is at 0xD47208, at most 128 pairs.
- **Dust**: on falling to the ground, 6 puffs from `data\blobs.raw` (rows 2–3), colour 0x50806040, size `min(2R, 5)`,
  ±2 m/s, life 1 s of game time, they grow in 0.125 s and shrink down to 0; in the sea colour 0x28C8F0F4 + splash + ring;
  in shallow cells, ring + dust. Each building piece releases one per distinct vertex but the first (0x80706050, size 2;
  see "The order of a break" above).
- **The snow tint** of the dust and the foam, `fn_004ED180(argb, point)`, called for each landing puff
  (`AttemptToAddSoundEvent` 0x64683A) and for a creature's sound-effect dust (`LH3DCreature::PlayASoundEffect` through
  fn_004ED250, 0x483489/94/9F): with the snow cover there (`[0xEDC344]`), k = `__ftol` of the snow's depth at the point
  (fn_0086CB80, the bilinear grid) kept within 0..255, then each channel c + ⌊(base − c)·k / 256⌋ towards the land's
  light of the moment `[0xFA26A4]` (argb's alpha kept). Measured with the oracle on 16 colours and depths, both ways.

- **A creature's step dust**: `LH3DCreature::PlayASoundEffect` (0x48346E..0x4834A9) maps the sound's action 3..109
  through the byte table 0x483600: a light step (3) makes 3 puffs, a step (4) 6, a stamp (5) 12 (`fn_004ED250(n)`), and
  95..109 go to fn_0068E6E0 (another row); this whether or not the sound plays (only no camera skips it, 0x4832D5).
  Each puff (0x4ED277..0x4ED308, and run with the oracle's argument capture: seed 0x12345, size 2 gives (−6.2, −6.7, 8.8), (−8.3, 1.5, 3.5), (−7.8, 3.2, 5.6)): three `LocalRand(201) − 100`, each × 0.05 × the drawn size (+0x90), into the velocity
  as (third, second, first) (z from the first draw), size 4 × the drawn size, colour 0x40806040 tinted by the snow
  (fn_004ED180), `AddLiquidParticle(position +0x78, velocity, colour, size, 4)`, read from the stack.
- **Dust velocities**: a ground impact's puffs take `(LocalRand(201) − 100)·0.02` (0x8CF178) on each axis, drawn z, y,
  x (0x6467D1 / 0x6467ED / 0x646809); a building piece's puffs use the synced `GameRand(201)` instead
  (`Fragment::SetUpPhysOb` 0x76EEC5..0x76EF53). The puff's sprite variant is CRT `rand() % 16` (fn_00845FA0
  0x845FDE).
- **Whoosh** (G_ROCKPAST, `InGame.sad` 69–73): a body that enters the camera's 10 m sphere at more than 20 m/s:
  `PlaySoundEffect(0, 69 + GetTickCount() % 5, modo 2, 0, 0, 2D, InGame)` (0x645C12).
- **Half-built hit building**: over its FragMesh the intact model is drawn clipped at
  `pos.y + pct·height` (pct = `(life − s)/(1 − s)`, s = 1.1·life − 0.1 on hit: 1/11), with an inner wall at 0.35 (0.2 if
  the material is two-sided), a cap on the cut and the scaffolding (the submesh with the highest status) coming out of the ground; nothing
  if the cut ends up below 0.2. Without repair by the villagers, it stays like that.
- **Half-built draw** (`DrawPartialyBuilt` fn_00816AD0, LH3DStaticObject vt +0x110, called by
  `MultiMapFixed::DrawBuilding` 0x517F90 and `LH3DCitadel` 0x882A40): pct clamped to [0, 1] (0x816AD6); cut plane d =
  half·scale·pct·2 + y (0x816D6F..0x816D9B), half being the mesh box's size.y (+0x28: half the y extent over every
  sub-mesh, `LH3DMesh::ComputeBoundingBox` 0x8081B0; [Buildings](../world/buildings.md) has the wonder's numbers); a primitive draws nothing when the plane is under 0.2 (0x8AB244) above the
  origin (fn_0085C7F0 0x85C82F). `DrawBuilding` draws nothing at pct 0 (0x517FE0).
- Only the status-0 sub-meshes and the scaffold (S = max(highest status, 1), 0x816EA8) are drawn; any other status is
  skipped (0x816F54).
- Scaffold: below pct 0.2 it rises, its matrix moved along its up row by −2·half·(1 − 5·pct) (0x816F8B..0x816FEA,
  0x8DAD58 = 5, 0x8C7CE0 = −2); above 0.8 (0x8C4A04) it is cut from the top at d = (1 − (pct − 0.8)·5)·half·scale·2 + y
  (0x81708D..0x8170CB); in between it stands whole. The scaffold draws with no inner walls (0xC392B4 = 0, 0x816F60).
- Inner walls: offset 0xC392AC = 0.2 when the material is two-sided (+5 bit 0), otherwise 0xC392B0 = 0.35; the temple's
  `LH3DCitadel` draw 0x882A40 sets both to 1.0 around its call (0x882A7B / 0x882A85) and also draws at pct 0 (the
  scaffold then lowered by the whole height).
- Every partial-build pass is two-sided (`g_NoBackfaceCull` = 1 at 0x85CD61, CULLMODE NONE at 0x85CE91..0x85CEB1).
- The cap (fn_00820B20) is drawn only if the outer pass cut something, the inner pass cut as many times, and LH3DObject
  +8 bit 0x200 is clear (0x85CF18..0x85CF43); no cap after an overflow of the segment list (at most 1000 segments,
  fn_00820EC0). No writer of the 0x200 bit was found in the exe.
- The cap's colour (0x820B46..0x820B93): [0xC37D8C], which `DrawPartialyBuilt` sets from the object's +0x4C (0x816D52;
  +0x50 into [0xE9FE2C], 0x816D58), times 3 and shifted: each of R, G, B is (c·3) >> 2, the integer form
  `(c·192) >> 8` of `ScaleRgbShift8KeepAlpha`, and the alpha is kept. No model light, no haze: for a building drawn
  partly built +0x4C is the bilinear land light (`MultiMapFixed::DrawBuilding` 0x517FB2, so it follows the time of
  day), times the charring grey when it burns (0x517FD4). The walls' corners are that colour × the light factor
  (0x85C932..0x85C97E) with the specular [0xE9FE2C] (0x85C980), and each cap corner copies the specular of its cut
  point (+0x14, 0x820CFE), i.e. the object's. The texture and render mode are the primitive's; CULLMODE NONE when not
  already set (0x820E6B..0x820E8B). The exe, asked for colours: 0xFFFFFFFF → 0xFFBFBFBF, 0x80FF8001 → 0x80BF6000,
  0x12345678 → 0x1227405A, 0xFFFEFDFC → 0xFFBEBDBD.
- No snow on the partly built draw: the snow texture [0xEDD394] is read only by the whole draws (fn_0080DB30 0x80E432,
  fn_0080E550 0x80E71C), the boned draw (0x8130F5), `FragMesh::Draw` and the atmos, never by fn_00816AD0, fn_0085C7F0,
  fn_0085C0E0 or the cap fn_00820B20; the snow slots (vt +0x124 `DrawSnow` fn_0080F0A0, +0x128 fn_0080EF20) are called
  only from the whole draw (0x80E443, when obj+0x58 >= 5), and `MultiMapFixed::Draw` 0x518090 sends a building being
  drawn partly built only to `DrawBuilding` (0x5180A6), the citadel 0x882A40 to fn_00816AD0 while +0x9C < 1.0. So the
  cut outer walls, the inner walls and the cap of a building site, a repaired house's rebuilt part, an unfinished
  worship site, ArkDryDock or temple carry no snow.
- Cut points (fn_0081D790) lie on the world plane (0, −1, 0, h); the corner distances are clamped to ≥ 0.0001
  (0x8BF518) below and ≤ −0.0001 (0x8D8738) above, and every attribute is interpolated, including the lit colour:
  t = dA / (dA − dB) (0x81D9BE..0x81D9C8), each attribute t × above + (1 − t) × below (0x81D9DB..0x81DA2A).
- The segment list (fn_00820EC0: `cmp word [0xEB9994], 0x3E8; jbe`) is reset per primitive (0x85C809, 0x85CF4F) and
  shared by the outer and inner passes: it stores while it holds at most 1000 (up to 1001), so with both passes cutting
  n segments the cap shows for 1 ≤ n ≤ 500. There is no other limit. One segment per triangle with a corner below and
  one above (a corner at the height counts above, 0x85C99D); a triangle with all corners above gives none (0x81D536).
  The segment's ends by the corners above (fn_0081D4F0), as (corner below, corner above) of each end: a, b above →
  (c, a), (c, b); a, c → (b, a), (b, c); a → (b, a), (c, a); b, c → (a, b), (a, c); b → (a, b), (c, b);
  c → (a, c), (b, c). The inner pass pushes the corners in before it cuts them (fn_0085C0E0). The cap's triangles per
  segment i of N stored: (2i, 2i+1, N+2i) and (N+2i+1, 2i+1, N+2i) (0x820D54..0x820E61).
- Inner walls and cap belong to a primitive only when its outer pass drew something (0x85CE04..0x85CE08): a triangle
  with any corner below the cut, not only a whole one.
- **Shadows**: pieces do not cast any; the broken building keeps the static shadow of its intact model.
- **Pending** (they depend on systems that do not exist yet): snow on the FragMesh (weather storms, snow map). The charring/glow of a burning house's FragMesh goes in `BuildingDamageSystem::AppendDamageMeshes`.

- 0x6467D1, 0x6467ED, 0x646809: z ([ebp-0x38]), y ([ebp-0x3C]), x ([ebp-0x40]) in that order
- fn_00845FA0 0x845FDE: a kind other than 0 (all of openblack's are kind 4) takes CRT rand() % 16 (signed; rand is never negative), after the 0x400 test of fn_00845D30 (0x845D42)
- The velocity of a ground impact's puffs (AttemptToAddSoundEvent 0x6467D1..0x646833): (LocalRand(201) - 100) x 0.02 on each axis, +-2 units per second, drawn z, y, x.
- fn_00820EC0 0x820EC0: stored while count <= 0x3E8, else [0xEB99A0] = 1
- 0x81D9BE..0x81D9C6
- One clipped draw (fn_0085C7F0's or fn_0085C0E0's list) and the cut segments of the primitive (0xE637E0, count [0xEB9994], overflow [0xEB99A0]; both passes of a primitive share them)
- fn_0081D1B0 -> fn_00820EC0
- 0x81D536: all above
- A model vertex in the world: (x - k nx, y + delta, z - k nz) through the matrix (fn_0085C7F0 0x85C86F..0x85C8DD, fn_0085C0E0 0x85C156..0x85C1C9)
- fn_0085C7F0: one primitive cut at the plane `planeD`, then (when `innerWalls`, [0xC392B4]) fn_0085C0E0's inner pass and fn_00820B20's cap
- The world plane the cut points are put on (fn_0081D790)
- 0x85C809 / 0x85C810: count and overflow reset per primitive
- 0x85CD61 g_NoBackfaceCull = 1 and 0x85CE91..0x85CEB1 D3DRENDERSTATE_CULLMODE = NONE: every pass is two-sided
- [0xC392B4] = 0 while the scaffold draws (0x816F60)
- fn_0085C0E0: k = [0xC392AC] when material +5 bit 0 (0x85C0EB..0x85C111), else [0xC392B0]; the stream is rewound first (0x85CEFE..0x85CF0C) so the same deltas are used
- 0x85CF18..0x85CF43: the cap only if the outer pass cut something, the inner one cut as many times and obj+8 & 0x200 is clear
- fn_00820B20: nothing after an overflow (0x820B20..0x820B45)
- 0x820D54..0x820E61: per segment i, (o0, o1, i0) and (i1, o1, i0); the uvs of the cut points, no culling. ⚠️ approximate the original's colour is 0.75 x the object diffuse (+0x4C), unlit (0x820B46..0x820B93); here the normal is the world up and the lighting is the renderer's
- 0x816EA8..0x816EB5: S = max(fn_0080AD60 = the highest status, 1)
- [0xC392AC] / [0xC392B0], the inner wall offset for both cull modes, in model units. Unset: the defaults (PartialBuild::k_InnerOffsetTwoSided / k_InnerOffsetCulled). LH3DCitadel's draw 0x882A40 sets both to PartialBuild::k_TempleInnerOffset around its call (0x882A7B / 0x882A85) and restores them (0x882A9D / 0x882AA7)
- [0xC392AC] (0x3E4CCCCD), material +5 bit 0 set
- [0xC392B0] (0x3EB33333)

#### The snow tint


### Water in impacts and when dropping

- In [Water](../world/water.md): the [water cells (SeaCells)](../world/water.md#water-cells-seacells), the
  [impact against the water, the wave when bobbing and the resources that fall into the sea](../world/water.md#impacts-and-objects-that-fall-into-the-water)
  and [sinking, drowning and being deleted](../world/water.md#sinking-drowning-and-being-deleted) (`HasSunk`, state DROWNING 16,
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
  3. it lands if it was not thrown, **it did not have to be raised** (the body's y did not change; exception: a villager, `IsVillager` vt +0x2C8,
     let go of through the interface of a computer player, its player's kind +0x8E0 == 2, 0x637130..0x63716D: a
     computer player's hand's, or its creature's; openblack: `from_hand::MayLand`) and `IsDryLand || the altitude of the rounded cell (fistp) > 1`. `Living` and `Fence` also need
     normal.y ≥ 0.7 (0x6372A6). `IsFence` 0x609110 = `MobileStatic` with mesh 0x38 (American fence) or 0x51/0x52
     (Celtic fences). A thrown fence that hits a store of wood becomes wood (`MobileStatic::ReactToPhysicsImpact`
     0x608FC0; [Static objects (MobileStatic, rocks)](../world/objects.md#static-objects-mobilestatic-rocks)).
  4. Lands → LANDED flag (8). `Living`, `Fence` or tree (not on fire) on `IsLand` leave physics **on the
     spot** with `RemoveObject(obj, 1, 1)`: they take the body's pose and their `EndPhysics` runs (villager: LANDED or
     drowns; animal: LANDED; tree: replanted, see objects). A tree **tilted** in the hand (|x| or |z| of
     `GetYXZ` > 0.2) or with `dont_replant` stays in physics **without** LANDED, unless it has a thrower: a tree a creature
     lets go of goes straight to the `RemoveObject` whatever its tilt (the thrower test 0x637362..0x637367, before the
     tilt and `dont_replant` tests 0x637390..0x6373BE). openblack: `from_hand::LandedTreeStaysInPhysics`. Everything else (rocks, statues, thrown
     pots…) **stays in physics with LANDED**, resting and aligned, until it stops by itself (~1 s for a rock on flat ground).
  5. Does not land (the sea, outside the map, raised on top of something, thrown) → keeps flying or falls;
     `Villager::CreateDroppedResource` 0x750940 (drops the log it carries: pending, openblack does not carry wood),
     `Reaction::CreateReaction(obj, 9 REACT_TO_FLYING_OBJECT)` (ported: the animals flee from it, the villagers run from it
     or point at it, [A thing thrown from the hand, reaction 9](../beings/villager-reactions.md#a-thing-thrown-from-the-hand-reaction-9)) and
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
    It skips the object's own thrower (po +0x1C, 0x644987), objects without a 3D object (0x6449D5), and anything whose
    `ShouldPhysicsRaiseObjectUntilNotIntersectingThis` (vt +0x7A4, 0x6377D0) is 0, for example LandscapeVortexIn
    0x5FD8D0 or every map shield 0x72C1E0 (MapShield, MagicShield and PhysicalShield share it). That test only picks
    the new resting bodies: the raising loop (0x644AE0..0x644C21) goes over every body already in the physics list
    with no class test (only itself and the villager / pushed-by-a-Living pairs are skipped). A physical shield is in
    the list from its creation (`PhysicalShield::CallVirtualFunctionsForCreation` 0x72CD30 calls AddProxy fn_00644DF0)
    and never leaves it (0x72CAF0), so a raised object does go over a physical shield. In the original it repeats with
    no round limit.
  - At 0x637231 the "altitude > 1" test of step 3 is `cmp [cell + 4], 1; jbe` on the rounded cell; the tilt test of
    step 4 reads `LHMatrix::GetYXZ` 0x7FAB30 of the 3D object's matrix (0x637390).
  - `Object::InitialisePhysicsFromHand` ends any release prediction of this object first (0x636F69..0x636F89); the
    flying-object reaction (0x637412) uses the thrower's player (`GInterfaceStatus::GetPlayer`, 0x637405).
  - **Release spin** (`GPacket::ProcessPacket` case 0x40, 0x63CDF7..0x63CEB9; sent by `CHand::GetRequiredState`
    0x46CE01..0x46CE8D about 180 ms after the release, with the hand's displacement since then): if the object has a
    body, its external torque (PhysOb +0x128) += (−m·dz·1.6·|v|, 0, m·dx·1.6·|v|), with m = PhysOb +0x134 and 1.6
    (0x931338). `ZeroForces` copies it every substep and `GameTurnUpdate` clears it at the end of the turn.
  - **The villager's dropped log** (`Villager::CreateDroppedResource` 0x750940; called at 0x6373F4 with the hand's
    angular velocity, which is 0): `InitialisePhysics(v, w, thrower 0, add 1)`, then, with a body, po +0x90 = the
    angular momentum when given, flag 0x10 (no collision with objects, 0x750A6D), `AdjustToGroundLevel(false, true)`
    and `RaiseUntilNotIntersecting` (0x750A54..0x750A89). The log's mesh comes from the `CarriedObject::Init` table
    0xC5E19C (one AllMeshes index per CARRIED_OBJECT, 0 = none; read at 0x750993).
  - `RemoveObject(obj, 1, 1)` 0x646A00: angles and position of the body, `EndPhysics`, and if LANDED and `IsLand`,
    `DropSfx` (vt+0x794: 0 except `Tree::DropSfx` 0x74BC60 = G_PlantTree_01 + tick % 3, which openblack plays when
    replanting); `Tree::EndPhysics` 0x74B830 only replants with LANDED on land and without fire, otherwise, dead tree.
    `Tree::EndPhysics` searches the cells (0x74B9C0) before it inserts the tree, so a replanted tree does not find
    itself.
  - Hook: `OPENBLACK_HAND_TEST_DROP` (types 4 tree, 5 animal added). Checked on Land1 (1788.4; 2710):
    rock → in physics with LANDED and at rest after 0.9 s; tree → replanted; villager and animal → out of physics
    on the spot; pot → pile; villager in the sea (1464; 2016) → sinks and DROWNING 600 turns; rock over a
    building at (1780.4; 2713.3) → raised to y 38.3, without landing.

### Rocks that split

- A rock with 2D radius > 3.6 cannot be picked up: **clicking on it hits it immediately**. A rock that can be
  picked up is hit with a short click (< 225 ms). Condition: height > 0.7. No hit counter.
- Two rocks of the same type come out, scale × 0.7935 (∛½: half the volume), only the Y angle, at
  `Pos ± (cos a, 0, sin a)·0.7935·R2D` with random `a`; the original is deleted and the halves enter physics (they fall or
  keep flying with its velocity). Sound G_RockTap_01..04 (130 + counter 0xD559AC) in 3D at the hand's point, with the rock as owner (0x6E751D).
- Strong impacts also split them (see damage).
- `Rock::ValidForPlaceInHand` 0x6E7030 holds the "2D radius > 3.6 cannot be lifted" rule.
- The first draw is `GameFloatRand(2π)` at 0x6E7576; the factor 0.7935 is 0x942110. The offset is `GetPosFromAngle(a,
  R2D·0.7935)` (0x6E75B1), and the halves are made at pos ± o with `MapCoords` operators (0x6E76A9 / 0x6E76CE). Both
  keep the rock's altitude, so each half sits at the ground at its own point plus that altitude.
- Each half in turn (0x6E76F3..0x6E7788): made through the mobile static maker (fn_00608770; a half it could not make
  is skipped), `SetLife(1.0)`, `InitialisePhysics` with the rock's velocity, its angular momentum (+0x90) set, and
  then the rock's fire copied to it (`CopyFromObjectToObject` 0x7308F0, [Fire](../world/fire.md#fire)). After
  both, `ToBeDeleted(0)` on the rock (0x6E7794), which deletes its fire with it. As `InitialisePhysics` takes a
  burning object out of its blaze (`FireEffect::StartedMoving`), the order matters: each half starts to fly before it
  has a fire, so both halves burn on in the rock's blaze.
- The halves' `SetLife(1.0)`:

- 0x942110, about the cube root of 1/2: each half has half the volume
- Rock::SplitInTwo 0x6E7560 draws first thing: GameFloatRand(2pi) at 0x6E7576
- 0x6E75B1: o = GetPosFromAngle(a, R2D x 0.7935); 0x6E76A9 / 0x6E76CE: the halves at pos + o and pos - o (MapCoords::operator+ 0x605520 / operator- 0x6055C0 on this +0x14, `lea edi, [esi + 0x14]` 0x6E76A3): both keep the rock's altitude (o's is 0), so each half is the ground at its own point plus that altitude (GetLHPoint)
- Rock::SplitInTwo: the halves are made (fn_00608770 at 0x6E7717) at the rock's kept Y angle, `GetYAngle` vt +0x508
  (0x6E7702), with no lean; openblack takes `placed_turn::AnglesOf(rock).y`
  ([The angles of a thing](../world/objects.md#angles-of-a-thing))
- Rock::SplitInTwo: the fire passes on to both halves (fn_007308F0, ECS/Fire), then the old rock goes
- A creature's blow on a rock (`LH3DCreature::DoBlendedAction`, v1.42 0x48C371..0x48C443, after the pick-up and the
  building branches): `IsRock` (vt +0x1F0) and a cast to `Rock`; then `LH_SamplePlayOptions` with the in-game bank
  (Global +0x3AC), sample `0x8B + [0xC6421C]` (139 + a counter of its own, 0..3, set back to 0 at 4, 0x48C3D1..0x48C3EF),
  the attached object the creature (`esi`), positional 1, looping 0, the position the rock's `MapCoords` point
  (`GetLHPoint` 0x48C3B6); `PlaySoundEffect` 0x48C433, then `Rock::SplitInTwo` 0x48C43A. The track flag is left as
  the options' constructor sets it. bw1-decomp `src/Black/Creature3D.cpp` (DoBlendedAction) agrees. LHSample.h names
  139..142 `LH_SAMPLE_G_ROCKTAP_01_C2`..`04_C2`. The branch after it (a living thing, cast to `Rock`) never plays.
- <a id="what-a-split-makes"></a>What a split makes ✅. `Rock::SplitInTwo` serves every class made on a Rock: a rock,
  the bonfire (row 8, made by the maker's own branch 0x439850), a fragment (its constructor 0x76E9D0 makes a Rock of
  row 2), a dead or felled tree (row 3). Each half is made through the mobile static maker fn_00608770 with the split
  thing's own info (`[esi+0x28]`, 0x6E76FC), its `GetPlayer` (vt +0x1C, 0x6E7710, `Rock::GetPlayer` 0x6E77A0 in all
  of them: the player kept at +0x90, none for a fragment) and its Y angle. The maker gives row 8 a bonfire with no
  player (0x608792: position, 100, angle, scale; the bonfire's spot visual starts with it), then a row of the rock
  kind (+0x128 == 2) a Rock with that player; of the other rows, row 6 nothing (0x6087E1; a half it can't make is
  skipped, 0x6E7723) and the rest a plain mobile static with no player. So a bonfire's halves are two bonfires, a fragment's two rocks of row 2 of the
  neutral player. A bonfire's half gets no body (`CanBecomeAPhysicsObject` 0x4397A0 = 0, see
  [Physics](physics.md#a-bonfire-never-flies)). The two ways a creature splits a thing: its blow (above), whose `IsRock`
  test lets a rock, the bonfire and a fragment through, and the shatter step of breaking a rock
  (CREATURE_SUB_STATE_ACTIONS_SHATTER_ROCK, 0x508090), a `dynamic_cast<Rock*>` of the object acted on and
  `SplitInTwo` (returns 2), else 3 (the agenda fails).

### Building impact details

- CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548
- Entt's on_destroy<BuildingDamage> (Registry::Destroy, Remove, Reset): the FragMesh's generated model out of the mesh cache at the real destruction of a deleted building (dead list: the zombie's body leaves at the next BeginTurn, 0x645018). Connected once per registry, from abodes::ConnectDrawMeshListener (idempotent).

### Building pieces

- 0x7F7ED9..0x7F7F18: [0xEA9E90] - position, axis by axis, each stored as a float
- 0x7F7F1B..0x7F7F37: (y y + z z) + x x into InverseSquareRoot 0x841170; 0x7F7F3C..0x7F7F5A: each axis times it
- 0x7F7F8C..0x7F7FF1
- 0x7F7FEE..0x7F806E
- 0x7F8074..0x7F8085: fn_007FEB30(position, specular, &colour) returns the new specular
- 0x7F80BC..0x7F8113: the LHMatrix rows are glm's columns
- 0x7F8118..0x7F81A1 / 0x7F81DA..0x7F8234: a = v2 - v0, b = v1 - v0, n = b x a
- 0x7F81A7..0x7F81CA / 0x7F823C..0x7F825F: (x x + y y) + z z into InverseSquareRoot; 0x7F826A..0x7F8280
- 0x7F8283..0x7F829F: (l.z n.z + l.y n.y) + l.x n.x, x 255 in Intensity; 0x7F82A8..0x7F8363 the two sides
- 0x7F83B1..0x7F8498: the back copy, v - 0.45 n with 0.45 n stored first
- 0x7F8587..0x7F859F: (f0, f1, f2) and (b2, b1, b0)
- 0x7F8621..0x7F8698: the walls of the open edges
- Pass 0: the FragPrimitive's material through the current (normal) table, 0x7F86BE
- FragMesh (0x7F6F00..0x7F8A20): a building copied into world-space triangles that a rock can knock pieces out of. Every LOD 0 primitive keeps its material; the triangles know their edge neighbours and a size class (how many times they may still be halved).

### Partly built buildings

- fn_0081D790(ecx = the corner below, edx = the corner above): the point on the world plane (0, -1, 0, h) ([0xF03128..0xF03134]) of the edge. The distances are clamped to the side the 0x800 flags gave each corner, so a corner the (local y) flag test put on the wrong side of the world plane gives t = 0 or 1, not a point off the edge.
- 0x81D8E5..0x81D8F4
- 0x81D9A9..0x81D9B8
- fn_0081D4F0(a, b, c): a triangle with a corner flagged 0x800 (above), in the original's corner order. Every cut records its segment (n0, n1) before the triangles are emitted.
- 0x81D53C: c below
- 0x81D5B8: b below
- 0x81D5F5: b and c below
- 0x81D67E: a below
- 0x81D6BB: a and c below
- 0x81D71B: a and b below
- 0x81D778 (only the frustum bits were set)
- The object being drawn: [0xEA1AE8] and its world matrix 0xEA9E10
- 0x85C7F6..0x85C83A: rel = planeD - obj.y; under 0.2 nothing, and the stream does not move (literal: the next primitives read this one's deltas)
- 0x85C87A..0x85C884
- 0x85C938..0x85C9AD / 0x85CBA8..0x85CC1A: flagged 0x800 when rel <= the model y (+ the melting delta). Literal: the unscaled model y against a height scaled by obj +0x44 (only differs for scale != 1)
- 0x85CDC5: no flag, into the list as it is
- 0x85CDB9
- 0x85CE06: an empty list draws nothing, no inner pass and no cap
- [0xEB9998] (0x85CEEC)
- [0xEB999C]
- fn_0084BA90 (one primitive) / fn_0087FB60 -> fn_00859530 (every primitive of the sub-mesh, with the melting stream): the normal, uncut draw with the material's own culling
- 0x817001 / 0x81722F fn_0087FB60: literal, every primitive of the sub-mesh once per primitive, each call moving the stream by the whole sub-mesh (with n primitives the copies after the first and the sub-meshes after it read the wrong deltas)

### Rocks

- CleanupWhenDeleted 0x6377F0: RemoveMapObject vt +0x548

## openblack

Status: ✅ faithful except what [Pending](#pending) lists. Progress rows:
[impact damage](../project/progress/physics/impact_damage.md) (93%),
[damage and repair](../project/progress/building/damage_and_repair.md) (92%),
[rocks splitting and heat](../project/progress/nature/rocks_splitting_and_heat.md) (74%).

- `src/ECS/Systems/Implementations/BuildingDamageSystem.*` and `src/Physics/DamageMesh.*`: the breaks and the damage
  models; the components `BuildingDamage` and `Fragment` (`src/ECS/Components/Fragment.h`); the generated meshes in
  `src/3D/L3DMeshGenerated.cpp`, cached as "<name>/<n>" (`ResourcesInterface::NextGeneratedMeshId`, one count per
  family) and taken away with the game that made them (`ForgetGeneratedMeshes`, from a game's shutdown, whose
  entities go without the destroy signals that erase them at a land change), so the next game numbers its own from 0
  as a launch does; a fragment's mesh goes with the fragment however it goes, a land change's clear too
  (`OnFragmentDestroyed`, as a damaged building's); the original has no such ids, its damage meshes belong to their
  abode;
- `src/ECS/Systems/Implementations/DynamicsSystem.cpp`: the collision sounds and the dust; `src/ECS/Physics/PartialBuild.cpp`:
  a partly built building's draw; `src/ECS/ObjectPhysics.cpp`: the split and the tap.

Where it is in the code (helpers named in the old notes):

- `BuildingDamageSystemInterface.h` (was `Buildings`): `ConnectDamageListener` (a destroyed building's generated
  model leaves the mesh cache, connected once per registry), `DestroyFragment` (a fragment out of the map cells),
  `ProcessTurn` (a fragment whose parent is no longer available forgets it); the impact itself is
  `BuildingDamageSystem::ReactToImpact` (below).
- `src/Physics/DamageMesh.*` (was `FragMesh`): `RandomSpin` (a piece's spin, z drawn first and x last),
  `UniqueVertices` (a piece's distinct vertices, each with the normal of its first triangle), and the light of the
  draw: `FrameLight`, `LightDirection` and `ObjectLight` of `damage::Mesh`; a flying piece is a `Piece`.
- `src/ECS/Physics/PartialBuild.cpp`: `WholePrimitive` (the uncut draw), `ClippedPrimitive` (one primitive cut at
  the plane, then the inner walls and the cap), `ClipTriangle` (a triangle with a corner above, in the original's
  corner order), `CutPoint` (the point of an edge on the plane, clamped to the edge).
- `DynamicsSystem::AddPuff` (was `Dust`): a dust kind other than 0 takes CRT rand() % 16 as its seed.
- `object_physics::SplitRock` (was `Rocks`): the split in two (factor 0.7935, the cube root of one half).

Porting decisions taken from raffclar's tree: [Port decisions: engine and world](../project/port-decisions-engine-world.md#physics).

#### Buildings that break

- openblack: `DynamicsSystem::ForgetAsBuildingHitter` (the stop, `RemoveObject` and `RemoveObjectWithEndPhysics`, the test taken before the end of physics), `BuildingDamageSystem::ForgetHitter(hitter, building)`.
- openblack: `DamageMesh::Impact` and `SplitUnconnectedGroups` hand each piece to `BuildingDamageSystem::CreateFragment` as they make it (the body, then `DustCorners`, then `IsSliver`).
- openblack: `damage::RebuildsOnBlow` and `damage::ModelForBlow` (`src/Physics/DamageMesh.*`), called by `BuildingDamageSystem::ReactToImpact` with `abodes::GetPercentForDrawBuilding` and the town's real building sites.
- openblack: `BuildingDamageSystem::ReactToImpact` tests the building's own `PhysicsObject::flags` the same way (`creature_mimic::ShouldMimicBuildingHit`), before its own hitter test.
- openblack: the class hooks' Fragment (`PhysicsGameHooks`, which puts it back in the map itself) calls `BuildingDamageSystem::PieceAtRest(fragment, body rows, body centre)`, the merge is `damage::Mesh::Merge` / `StuckCorner`; tests `test_damage_mesh_rebuild` (DamageMeshMerge), `test_particle_carried_objects` (AFragmentEndsItsPhysicsAsAFragment).
- openblack's `abode_queries::IsBuilt` reads the building's own state (not under construction and percent built ≥ 1).
- openblack's `PhysicsEntry::throwerPlayer`, the player who threw it

#### Sounds, dust and the look of impacts

- openblack: `physics::turn::BlendColour`, the world's `SnowAt` / `LandColour` (the snow system's depth, the land light table's colour), for the landing dust; testbed `physics.throw_in_snow`.
- openblack: `creature_dust` (PuffsFor, Make), thrown from the creature audio's fired steps before their hearing gate.

#### Rocks that split

- openblack does the same (`object_physics::SplitRock`): per half `AddObject` then `CopyFire`, then the rock through `ecs::ToBeDeleted`.
- openblack gives them no `Life`, which reads as full life (1.0) and is made at 1.0 when first worn (`KnockRock`).
- A creature's blow on a rock is `object_physics::SmashRock`, from the knock-down's moment
  (`CreatureObjectActionSystem::Moment`): the creature's rock tap of `audio::Counter::CreatureRockTap`, owned by the
  creature, at the rock, then `SplitRockAsItGoes` (the halves fly on as a flying rock flew, as `KnockRock`'s split).
  Unverified against v1.42 in play.
- [What a split makes](#what-a-split-makes): the blow and the shatter step (`CreatureMindSystem`'s shatter command)
  ask `object_physics::IsRock`, so the bonfire and a fragment break too; before, both took only a rock, and the shatter
  of a bonfire gave the agenda up. `object_physics::HalvesOf` gives the row and the player of the halves (test
  `TakeResourceTest.ASplitThingsHalvesAreMadeFromItsRowWithItsPlayer`), and `SplitRock` makes them through
  `MobileStaticArchetype::CreateFromInfo` as the maker does (a bonfire's with its flames and no body; a rock's as
  before). A fence knocked down stays (test `CreatureObjectActionSystemTest.AStaticThatIsNotARockKnockedDownIsLeftAsItIs`).
  In openblack a fragment is never the target of a blow (`CanDestroy` takes trees, homes and mobile objects and
  statics) nor picked up by a creature, so its split is not reached yet.

#### Partly built buildings

- The cut in the code: the cut's rule is `graphics::partial_build_cap` (CutEdgesOf, CutShare, SegmentList, Capped, k_CapQuad,
  CapColour), used by `ecs::physics::PartialBuild`, which puts the cap's triangles in a primitive of their own with
  `L3DSubMesh::Primitive::partialBuildCap`; the renderer passes it as `u_window.y` and `vs_object` gives those corners
  `ScaleShift8(colour, 192)` instead of the model light. Every primitive it makes is marked
  `L3DSubMesh::Primitive::partialBuild`, never merged with a whole one and given no snow (`TakesSnow`). Tests `test_partial_build_cap` (the cut shares pinned to the
  exe's fn_0081D790 results, the cap colours to fn_00820B20's, `TakesSnow`);
  testbed scenario `examples.abode_partly_built` (a Celtic hut being built, at 0, 10, 50 and 85 %).

### Test hooks

- `OPENBLACK_TEST_HIT_ABODE="speed[,scale[,index[,n]]]"`: n rocks thrown against a house, one every 1.5 s;
  `OPENBLACK_TEST_FRAGMESH=1`: which buildings can be broken (their damage mesh) and how many triangles they have.
- `OPENBLACK_HAND_TEST_SPLIT="x,z,scale,rounds"`: a rock split by the hand's taps.
- `OPENBLACK_HAND_TEST_DROP="x,z,seconds[,type]"`: a gentle drop (into the water too).
- The engine's own hooks (`OPENBLACK_TEST_PHYSICS`, `OPENBLACK_PHYSICS_TRACE`) are on [Physics](physics.md#test-hooks).

## Pending

- **Verify with xref/oracle:** the 0 **🔍 inferred** and 1 **⚠️ approximate** marks in The original above. Each is settled by reading the function named next to it (`python dev/tools/xref/xref.py show <function>`) and, where a number is in doubt, by an oracle run of that function (`dev/tools/oracle/GUIDE.md`).

- The shatter step's cast also takes a dead or a felled tree (Rock-derived, `IsRock` 0), whose halves the maker makes
  from row 3 of the mobile statics. openblack's shatter takes only what answers it is a rock: the breaking-a-rock
  plan never picks a dead tree (its target is a rock or a bonfire), and row 3's kind and the halves' player for a
  dead tree have not been read ([What a split makes](#what-a-split-makes)).
