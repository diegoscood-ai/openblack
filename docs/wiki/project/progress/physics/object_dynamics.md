# Object dynamics

How loose objects move once they are in the physics: falling under gravity, tumbling, sliding and rolling on slopes,
bouncing, coming to rest and waking again. The game runs its own rigid-body simulation (every mesh vertex is a contact
point with a spring and friction, probed 0.06 s ahead), not a general physics engine. Collisions are in
[collisions.md](collisions.md), water in [water_physics.md](water_physics.md), throwing and landing in
[throwing_and_landing.md](throwing_and_landing.md). Impact damage, fire and explosions are in
[impact_damage.md](impact_damage.md), [fire.md](fire.md) and [explosions.md](explosions.md). Tornadoes do not push
physics bodies: what a tornado lifts rides its funnel instead (see [../miracles/tornado.md](../miracles/tornado.md)). No
wind or other steady force acts on flying or resting objects: the only pushes are a villager shoving an object out of
its way and the hand's twist after a release, each lasting one turn.

openblack runs the game's own simulation: the bodies are `src/Physics/Body.cpp` (with
`DynamicsSystem::LoadConstants` reading `Data/PhysicsConstants.txt` through the resource caches),
and `DynamicsSystem` (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`, once `PhysicsObjects`) steps them once a
game turn and draws moving bodies between turns. Each kind's material, weight and shape are in `DynamicsSystem.cpp`
too, with the class hooks in `src/ECS/PhysicsGameHooks.cpp`. Tests: `test/test_physics_body_pose.cpp`,
`test/test_physics_body_water.cpp`, `test/test_physics_body_spin.cpp`, `test/test_physics_turn_rules.cpp`. No other physics library is linked.

**Progress: 38/47 done, 0 chosen, 7 partial, 2 inferred — 93%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## Simulation and timing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The physics runs in fixed steps of 5 ms, 20 of them per game turn of 0.1 s (200 a second), however fast the frames are | done | `DynamicsSystem::ProcessTurn` runs the turn's `physics::k_StepsPerTurn` substeps of `physics::k_StepSeconds` once a game turn (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`, `src/Physics/Body.h`); checked by the `physics.throw_one` and `physics.throw_many` scenarios |
| The physics stands still while the game is paused | done | No game turn runs while paused, so `ProcessTurn` is not called; `fidelity` and the physics scenarios run on the fixed turn |
| A flying object is drawn between where it was at the start and at the end of the last game turn, so its motion looks smooth at any frame rate | partial 90% | `DynamicsSystem::UpdateFrame` writes `components::PhysicsDrawPose` (lerped rows, normalised, with the quarter turn of villagers and animals); missing: an animated object's frame and the quarter turn of the other boned models (the reward chest, the dolphin, the shark, the ballista, some gates) |
| The game logic (AI, reactions) sees each flying object's position and angles at the end of the turn | done | `ProcessTurn` writes each moving body's pose into the `Transform` (`SyncTransform`): the thing takes the angles of its body and keeps them (`placed_turn::KeepAngles`); tests `PlacedTurn.*` (`test/test_placed_turn.cpp`); the original's side is checked in [The angles of a thing](../../../world/objects.md#the-angles-of-a-thing) |
| There is no limit on how many objects can be flying at once | done | `DynamicsSystem::MakeSureEndSlotIsFree` grows the list by 16; test `PhysicsHitter.AGrowthInTheLoopKeepsTheOldHitterAndNoCredit` |
| A flying object leaves the map cells while it flies and goes back into them when it comes to rest | done | Out of the cells when it starts (`AddObject`, the knocked branch of `Substep`), back in at its end (`EndPhysics`, the class's `CallsBackInMap`); tests `PhysicsObjects.WhatThePhysicsLetsGoOfStandsOnTheMapsGrid`, `ParticleCarriedObjectsTest.ATreeLetGoStaysInThePhysicsOutOfTheMap` |
| Objects a script marks as immovable never become physics objects | done | `object_flags::IsImmovable` refused in `DynamicsSystem::Add` (as `Object::InitialisePhysics`); set by SET_ID_MOVEABLE (`src/CHLApi.cpp`); tests `ParticleCarriedObjectsTest.AScriptsNewObjectThatCannotMoveStaysOutOfThePhysics`, `AnImmovableThingLetGoHangsWhereItWasLetGo` |
| Buildings, fields, forests and the creature never become flying objects; rocks, statics, mobile objects, the football, villagers, animals, trees, dead trees, food and wood, building pieces and one-shot miracle orbs can (pots only when their info allows, villagers only when reachable) | partial 90% | `DynamicsSystem::CanBecomeAPhysicsObject`; missing: villagers fly without the reachability test (`Villager::CanBecomeAPhysicsObject` is `IsReachable` in bw1-decomp) |
| A pot or a pile becomes a physics object only when its info row says so: the pots, the hand's wood and food and the things carried in the hand can; the storage pit's food pile, the wood piles, the food pile and the magic piles never do, whatever would move them (a script, a creature's kick, a storm, a vortex), and a scaffold clearing its way deletes them | done | `DynamicsSystem::CanBecomeAPhysicsObject` reads `GPotInfo::canBecomeAPhysicsObject` (test `ParticleCarriedObjectsTest.APotBecomesAPhysicsObjectOnlyWhenItsRowSaysSo`; [Pots and piles](../../../physics/physics.md#a-pile-flies-only-when-its-row-says-so)) |
| A creed (the creed and the eyes) never becomes a physics object, whatever would move it: the hand, a creature, a script, a storm or a vortex; let go by the hand it stays in the air where it was let go | done | `DynamicsSystem::CanBecomeAPhysicsObject` and `ConstantsType` (row 0) for a mobile object whose row is of the creed's kind; `from_hand::HangsWhereLetGo` (test `ParticleCarriedObjectsTest.ACreedNeverBecomesAPhysicsObjectAndHangsWhereTheHandLetsItGo`; [A creed never flies](../../../physics/physics.md#a-creed-never-flies)) |

## Bodies and shapes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body is built from the mesh's physics parts (or the LOD-0 parts when it has none): every vertex is a contact point and every triangle a face others can hit; morphing models (food piles, storage pits, the village centre, the temple heart) follow the land under each point | partial 80% | `CollectVertices` and `Body::Build` (the physics submeshes, else LOD 0, as [physics.md](../../../physics/physics.md#bodies)); the morphing part is inferred: neither our wiki nor bw1-decomp shows it, and our tree does not do it |
| Trees get a hand-made spindle body of 16 points along the trunk, with the centre of mass at 40% of the height while rooted (the spindle then reaches 0.2 of the height below the base) and at half the height once dead, and 0.3 of the drag | done | `shapes::TreeBody` (`src/Physics/BodyShapes.cpp`: 0.4 H rooted, 0.5 H dead, drag x 0.3), as `SetUpPhysObAsATree` in bw1-decomp; checked by the `physics.tree_replanted_good` / `_evil` scenarios |
| Villagers and animals get a hand-made body of 12 points on three levels (head, waist, feet), centre of mass at half height, with twice the air drag | done | `shapes::LivingBody` (`src/Physics/BodyShapes.cpp`: the turned box, half height, drag x 2); tests `LivingLanding.*` (`test/test_living_landing.cpp`), scenario `physics.thrown_villager_lands` |
| Pieces broken off buildings are thin slabs of their triangles, weighing 30 per unit of area, with twice the air drag; pieces thinner than 0.4 R² are deleted | done | The fragment body (a copy 0.45 behind, drag x 2, `src/Physics/BodyShapes.cpp`), `BodyMassOf` (30 x area) and `Mesh::IsSliver` (`src/Physics/DamageMesh.cpp`, deleted in `BuildingDamageSystem`); tests `DamageMeshPiece.AThinStripIsASliver`, `PhysicsWeight.AFragmentWeighsAsItsRockButItsBodyByItsArea` |
| How a body tumbles comes from its moment of inertia worked out from its points; the original's formula has one cross term wrong, which changes how lopsided objects spin | done | `Body::SetUpInertia` keeps the original's wrong cross term (`I[1][2] = -xz`, [physics.md](../../../physics/physics.md#manager)); checked by the `physics.throw_many` scenario |
| The body's radius is its farthest point from the centre of mass, times the object's scale | done | `Body::Build`; checked by the physics scenarios |

## Mass and material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An object's mass is its info weight times its scale cubed, and never below 0.01 | partial 90% | `DynamicsSystem::WeightOf` (at least 0.01, as `Object::GetWeight` in [physics.md](../../../physics/physics.md#bodies)); test `PhysicsWeight.AFragmentWeighsAsItsRockButItsBodyByItsArea`; unsettled: bw1-decomp's tree set-up takes its weight with no minimum and `Tree::SetUpPhysOb` (0x74B650) is not decompiled, so whether trees are clamped is not checked |
| Villagers weigh 82.5 at scale 1; buildings' bodies count as 2000 and never move; the temple heart's body 10000, the creature's 1000 whatever its size, the physical shield 50000 times its scale cubed, all static | done | `DynamicsSystem::BodyMassOf` / `WeightOf` and `PhysicsGameHooks::BodyMassOf` (`k_HeartMass`, `creature_physics::k_Mass`; Abode 0x402DD0, CitadelHeart 0x467B70, Creature 0x479B90, [physics.md](../../../physics/physics.md#bodies)); tests `CreaturePhysicsTest.OnceRegisteredThePhysicsBuildsAndWeighsACreatureAsOne`, `CreaturePhysicsRules.ACreaturesBodyIsAHeavyBallAsTallAsItThatFacesOut` |
| Some gates weigh 10000 as bodies | inferred | Written from expectation: no gate class has a `SetUpPhysOb` of its own among the exe's symbols (xref), and our wiki does not say it; to check |
| A building's (and a storage pit's) weight as an object, for a push, a catch or a lift, is its scale cubed times its abode's info weight; its body stays 2000 | done | Test `test/test_particle_carried_objects.cpp` (`ParticleCarriedObjectsTest.ABuildingWeighsItsScaleCubedTimesItsAbodesWeight`). `PhysicsGameHooks::ObjectWeightOf` (as [physics.md](../../../physics/physics.md#bodies); the weight item of #88) |
| Each kind of object takes one of 24 material rows from `Data/PhysicsConstants.txt`: density, contact stiffness, contact damping, friction, how much spin is kept each second, and air drag | done | `DynamicsSystem::LoadConstants` reads `Data/PhysicsConstants.txt` through the resource caches; test `PhysicsMaterials.TheFileGivesItsVersionAndRows` |
| Material values are clamped when loaded (density 0.05 to 3, stiffness 0 to 240, damping 0 to 10, friction 0 to 3, spin kept 0.2 to 1, drag 0 to 4); rows past the file's count copy the first one; without the file every row is zero (the game has no built-in values) | done | `DynamicsSystem::LoadConstants` and the materials loader (`src/Physics/Materials.cpp`); tests `PhysicsMaterials.EachColumnIsClampedToItsRange`, `TheRowsTheFileLacksAreCopiesOfRowZero`, `WithoutTheFileEveryMaterialIsZero` |
| Which row each kind uses: buildings, the temple heart and the creature row 0, any other movable object row 1, the football 2, rocks and the heavy statics 3, pots 4 (the food offering 5), trees and field crops 6, villagers 7, animals 8, one-shot miracle orbs 9, the physical shield 10, building pieces 11, poo 12, vortex 13, the toys 14/15/16/19/20, the scaffold 17, fences 18, the three mushrooms 21–23; wood in the hand row 1 | partial 95% | `DynamicsSystem::ConstantsType` (every class but one, [physics.md](../../../physics/physics.md#manager)); tests `ParticleCarriedObjectsTest.ALumpOfPooTakesItsOwnRowOfPhysicsConstants`, `AFootballIsAPhysicsObjectWithItsOwnInfoAndRow`; missing: the field crop (row 6), whose class is not in our tree |

## Gravity and flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every flying body falls under a gravity of 9.81 m/s² | done | `physics::k_Gravity` (`src/Physics/Body.h`); checked by the `physics.throw_one` scenario |
| Air drag grows with the square of the speed, scaled by 0.3 times the radius squared and the material's drag | done | `Body::SetUpInertia` (0.3 R squared times the material's drag); checked by the physics scenarios |
| No body flies faster than 124 m/s | done | `physics::k_MaxSpeed`; checked by the physics scenarios |
| No body spins faster than 3π radians a second (one and a half turns) | done | `physics::k_MaxSpin`; checked by the physics scenarios |
| Spin dies away: each second a body keeps its material's share of its spin | done | `Body::SetUpConstants` (the per-substep step of the share kept a second, `physics::SpinKeptPerStep`, raised with the runtime's power as the original); test `PhysicsBodySpin.TheSpinKeptPerStepGivesTheOriginalsBits` |
| Bodies tumble when they hit things off-centre: each contact point's force turns the body | done | `Body::ApplyContacts` (the torque of each contact); checked by the `physics.throw_many` scenario |
| Steady pushes on a body last one game turn and are cleared at its end; only a villager shoving an object and the hand's twist after a release set them | done | Cleared in `EndTurn`; the hand's twist after a release in `src/ECS/Physics/FromHand.cpp` ([physics.md](../../../physics/physics.md), GPacket case 0x40); the villager's shove is the last row below |

## Contacts, sliding and rolling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each contact point pushes back like a spring: the depth at first touch times the stiffness, plus damping on any deeper push | done | `Body::ApplyContacts`; checked by the physics scenarios |
| Each contact point holds with friction up to friction times the push; past that it slides, so objects stick on gentle slopes and slide down steep ones | done | `Body::ApplyContacts` (Coulomb anchors); checked by the physics scenarios |
| Round or many-pointed objects roll down slopes as friction at the contact turns into spin | done | `Body::ApplyContacts`; checked by the physics scenarios |
| How high something bounces comes from its material's stiffness and damping, not from a fixed bounce share | done | No bounce factor in `Body`; checked by the physics scenarios |
| Fast bodies test their contacts 0.06 s ahead, so they don't pass through thin things | done | `physics::k_LookAheadSeconds`; checked by the physics scenarios |

## Coming to rest, sleeping and waking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body touching something comes to rest when its speed is under 1 m/s, its spin under half that and its turning force under it; its velocity and spin are then cleared | done | `Body::Integrate` (`StepResult::Stopped`); checked by the physics scenarios |
| A body can't come to rest before it has been in the physics for its scale times its half height in seconds (a tall object flies longer before settling) | done | `Body::Initialise` (the rest counter starts at minus scale x half height x 1000); checked by the physics scenarios |
| From 15 s after that the rest test loosens steadily (the limit grows by 1 every 15 s), so nothing jitters for ever | done | `Body::Integrate` (past 15000 counts the limit is the counter / 15000); checked by the physics scenarios |
| At rest the object goes back into the map; one that comes to rest outside the map is deleted | done | `DynamicsSystem::EndPhysics` and the class's `CallsBackInMap`; test `PhysicsObjects.WhatThePhysicsLetsGoOfStandsOnTheMapsGrid` |
| Resting objects near a moving body join the physics as sleeping obstacles, and only those | done | The wake walk in `BeginTurn` (`AddProxy`); test `PhysicsRest.AtTheTurnsStartOnlyMovingBodiesAreAwake` |
| A moving body wakes the objects in the map cells within its radius plus 0.1 s of its sideways travel | done | `BeginTurn` (the box of the sideways speed x 0.1 plus the radius); checked by the `physics.throw_many` scenario |
| A sleeping obstacle that is hit hard enough is knocked into flight and becomes a physics object of its own | done | `StepResult::Knocked` in `Substep`, woken through `CanWakeKnockedProxy`; checked by the `physics.throw_many` scenario |
| Sleeping obstacles with nothing moving near them leave the physics at the start of the next turn (the physical shield always stays) | done | `BeginTurn` (the physical shields always stay); test `PhysicsRest.AtTheTurnsStartOnlyMovingBodiesAreAwake` |
| A body that falls more than four of its radii below sea level is deleted | done | `Body::Integrate` (`StepResult::Delete`, 0x645B22 in [physics.md](../../../physics/physics.md#manager)); checked by the `physics.sinking` scenario |

## Other things that move objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager clearing ground pushes the obstacle within 2 m with a force of its weight × 9.81, and kicks the football; what they push moves in the physics and villagers' bodies don't hit it | partial 40% | `DynamicsSystem::PushObject` (the force, the push flag) and the pair rule are there and a football (`Ball`) can fly; missing: nothing calls the push (the villager's clear-area state, `Villager::ArriveAtPushObject`, is not ported) and villagers do not kick the football |
| A forester's felled tree is put into the physics and topples; once it tips past about 11 degrees a tree taller than 10 m makes one falling sound | done | `ecs::FellTree` (`src/ECS/Trees.cpp`), from the forester (`src/ECS/Villager/VillagerForester.cpp`); the fall sound in `EndTurn`; tests `PhysicsTurn.AFelledTreeToppledOnceItsUpAxisTipsPastUpright`, `PhysicsTurn.OnlyAFelledTreeTallerThanTenMakesTheSoundOfATreeThatFalls`; the exe's rule (up row y < 0.98, height above 10, 0x64609F) in [physics.md](../../../physics/physics.md#manager) |
| Reward chests don't use the physics while falling: they appear 150 m up, fall at 50 m/s turning at π rad/s, and on landing play their thump, shake the camera and raise 27 dust sprites | inferred | `reward::FallHeight` and the landing (`src/ECS/Systems/Implementations/RewardSystem.cpp`); the height, speed and turn are not checked against the exe ([../story/rewards.md](../story/rewards.md) marks the height and the time unit unconfirmed) |
