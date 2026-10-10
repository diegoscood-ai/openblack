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

openblack runs the game's own simulation: the bodies are `src/ECS/Physics/PhysicsBody.cpp` (with
`PhysicsObjects::LoadConstants` reading `Data/PhysicsConstants.txt` through the resource caches),
and `PhysicsObjects` (`src/ECS/Physics/PhysicsObjects.cpp`) steps them once a game turn and draws moving
bodies between turns. Each kind's material, weight and shape are in `PhysicsObjects.cpp` too. Tests:
`test/test_physics_body_pose.cpp`, `test/test_physics_body_water.cpp`. Bullet is still linked but never stepped:
it only answers the ray casts of the hand, the camera and the console (`DynamicsSystem`).

**Progress: 36/43 done, 6 partial — 91%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Simulation and timing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The physics runs in fixed steps of 5 ms, 20 of them per game turn of 0.1 s (200 a second), however fast the frames are | done | `PhysicsObjects::GameTurnUpdate` runs the turn's 20 substeps of `PhysicsBody::k_Dt` once a game turn (`src/ECS/Physics/PhysicsObjects.cpp`) |
| The physics stands still while the game is paused | done | No game turn runs while paused, so `GameTurnUpdate` is not called |
| A flying object is drawn between where it was at the start and at the end of the last game turn, so its motion looks smooth at any frame rate | done | `PhysicsObjects::UpdateFrame` writes `components::PhysicsDrawPose` (lerped rows, normalised, with the quarter turn of boned models) |
| The game logic (AI, reactions) sees each flying object's position and angles at the end of the turn | done | `GameTurnUpdate` writes each moving body's pose into the `Transform` (`SyncTransform`) |
| There is no limit on how many objects can be flying at once | done | `MakeSureEndSlotIsFree` grows the list by 16 |
| A flying object leaves the map cells while it flies and goes back into them when it comes to rest | done | Out of the cells when it starts (`AddObject`, the knocked branch of `Substep`), back in through `PhysicsObjects::BackInMap` |
| Objects a script marks as immovable never become physics objects | done | `object_flags::IsImmovable` refused in `AddObject`; set by SET_ID_MOVEABLE (`src/CHLApi.cpp`) |
| Buildings, fields, forests and the creature never become flying objects; rocks, statics, mobile objects, villagers, animals, trees, dead trees, food and wood, building pieces and one-shot miracle orbs can (pots only when their info allows, villagers only when reachable) | partial | `PhysicsObjects::CanBecomeAPhysicsObject`; pots fly without their info being asked and villagers without a reachability test |

## Bodies and shapes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body is built from the mesh's physics parts (or the LOD-0 parts when it has none): every vertex is a contact point and every triangle a face others can hit; morphing models (food piles, storage pits, the village centre, the temple heart) follow the land under each point | partial | `CollectVertices` and `PhysicsBody::Build` (the physics submeshes, else LOD 0); morphing models do not follow the land under each point |
| Trees get a hand-made spindle body of 16 points along the trunk, with the centre of mass at 40% of the height while rooted (the spindle then reaches 0.2 of the height below the base) and at half the height once dead, and 0.3 of the drag | done | `SetUpTreeBody` in `src/ECS/Physics/PhysicsObjects.cpp` (0.4 H rooted, 0.5 H dead, drag x 0.3) |
| Villagers and animals get a hand-made body of 12 points on three levels (head, waist, feet), centre of mass at half height, with twice the air drag | done | `SetUpLivingBody` (the turned box, half height, drag x 2); tests `LivingLanding.*` (`test/test_living_landing.cpp`) |
| Pieces broken off buildings are thin slabs of their triangles, weighing 30 per unit of area, with twice the air drag; pieces thinner than 0.4 R² are deleted | done | `SetUpBody` fragment branch (a copy 0.45 behind, 30 x area, drag x 2) and `CreateFragment` in `src/ECS/Physics/Buildings.cpp` (slivers deleted) |
| How a body tumbles comes from its moment of inertia worked out from its points; the original's formula has one cross term wrong, which changes how lopsided objects spin | done | `PhysicsBody::ComputeMomentOfInertia` keeps the original's wrong cross term |
| The body's radius is its farthest point from the centre of mass, times the object's scale | done | `PhysicsBody::Build` |

## Mass and material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An object's mass is its info weight times its scale cubed, and never below 0.01 (trees excepted) | done | `PhysicsObjects::Weight` (at least 0.01). Our wiki differs: the minimum of 0.01 applies to every object, trees included ([physics.md](../../bw1-notes/physics.md#bodies)) |
| Villagers weigh 82.5 at scale 1; buildings count as 2000 and never move; the temple heart and the gates 10000, the creature 1000, the physical shield 50000 times its scale cubed, all static | partial | `PhysicsObjects::Weight`: villagers from info.dat (82.5), buildings 2000, the physical shield from its info at the collision scale, the creature a stand-in 1000 (`creature_physics::k_Mass`); no temple heart or gates in the physics |
| Each kind of object takes one of 24 material rows from `Data/PhysicsConstants.txt`: density, contact stiffness, contact damping, friction, how much spin is kept each second, and air drag | done | `PhysicsObjects::LoadConstants` reads `Data/PhysicsConstants.txt` through the resource caches |
| Material values are clamped when loaded (density 0.05 to 3, stiffness 0 to 240, damping 0 to 10, friction 0 to 3, spin kept 0.2 to 1, drag 0 to 4); rows past the file's count copy the first one; without the file every row is zero (the game has no built-in values) | partial | `PhysicsObjects::LoadConstants` clamps each column and copies row 0 past the file's count; without the file a built-in table is used, not zeros |
| Which row each kind uses: buildings, the temple heart and the creature row 0, any other movable object row 1, the football 2, rocks and the heavy statics 3, pots 4 (the food offering 5), trees 6, villagers 7, animals 8, one-shot miracle orbs 9, the physical shield 10, building pieces 11, poo 12, vortex 13, the toys 14/15/16/19/20, the scaffold 17, fences 18, the three mushrooms 21–23; wood in the hand row 1 | partial | `PhysicsObjects::ConstantsType` maps buildings, the creature, rocks, pots, trees, villagers, animals, orbs, the shield (10), fragments, the scaffold, fences, toys and mushrooms; the football, poo and vortex rows have no class in our tree |

## Gravity and flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every flying body falls under a gravity of 9.81 m/s² | done | `PhysicsBody::k_Gravity` |
| Air drag grows with the square of the speed, scaled by 0.3 times the radius squared and the material's drag | done | `PhysicsBody` (0.3 R squared times the material's drag) |
| No body flies faster than 124 m/s | done | `PhysicsBody::k_MaxSpeed` |
| No body spins faster than 3π radians a second (one and a half turns) | done | `PhysicsBody::k_MaxOmega` |
| Spin dies away: each second a body keeps its material's share of its spin | done | `PhysicsBody::SetUpConstants` (the per-substep step of the share kept a second) |
| Bodies tumble when they hit things off-centre: each contact point's force turns the body | done | `PhysicsBody::ContactForces` (the torque of each contact) |
| Steady pushes on a body last one game turn and are cleared at its end; only a villager shoving an object and the hand's twist after a release set them | done | Cleared in `EndTurn`; the hand's twist after a release is in `src/ECS/Physics/FromHand.cpp`; `PhysicsObjects::PushObject` exists but nothing calls it |

## Contacts, sliding and rolling

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each contact point pushes back like a spring: the depth at first touch times the stiffness, plus damping on any deeper push | done | `PhysicsBody::ContactForces` |
| Each contact point holds with friction up to friction times the push; past that it slides, so objects stick on gentle slopes and slide down steep ones | done | `PhysicsBody::ContactForces` (Coulomb anchors) |
| Round or many-pointed objects roll down slopes as friction at the contact turns into spin | done | `PhysicsBody::ContactForces` |
| How high something bounces comes from its material's stiffness and damping, not from a fixed bounce share | done | No bounce factor in `PhysicsBody` |
| Fast bodies test their contacts 0.06 s ahead, so they don't pass through thin things | done | `PhysicsBody::k_LookAhead` |

## Coming to rest, sleeping and waking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body touching something comes to rest when its speed is under 1 m/s, its spin under half that and its turning force under it; its velocity and spin are then cleared | done | `PhysicsBody::Integrate` (`Result::Stopped`) |
| A body can't come to rest before it has been in the physics for its scale times its half height in seconds (a tall object flies longer before settling) | done | `PhysicsBody::Initialise` (the rest counter starts at minus scale x half height x 1000) |
| From 15 s after that the rest test loosens steadily (the limit grows by 1 every 15 s), so nothing jitters for ever | done | `PhysicsBody::Integrate` |
| At rest the object goes back into the map; one that comes to rest outside the map is deleted | done | `EndPhysics` and `PhysicsObjects::BackInMap` |
| Resting objects near a moving body join the physics as sleeping obstacles, and only those | done | The wake walk in `BeginTurn` (`AddProxy`) |
| A moving body wakes the objects in the map cells within its radius plus 0.1 s of its sideways travel | done | `BeginTurn` (the box of the speed x 0.1 plus the radius) |
| A sleeping obstacle that is hit hard enough is knocked into flight and becomes a physics object of its own | done | `Result::Pushed` in `Substep` |
| Sleeping obstacles with nothing moving near them leave the physics at the start of the next turn (the physical shield always stays) | done | `BeginTurn` (the physical shields always stay) |
| A body that falls more than four of its radii below sea level is deleted | done | `PhysicsBody::Integrate` (`Result::Delete`) |

## Other things that move objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager clearing ground pushes the obstacle within 2 m with a force of its weight × 9.81, and kicks the football; what they push moves in the physics and villagers' bodies don't hit it | partial | `PhysicsObjects::PushObject` and the pair rule are there, but nothing calls the push and there is no football |
| A forester's felled tree is put into the physics and topples; once it tips past about 11 degrees a tree taller than 10 m makes one falling sound | done | `ecs::FellTree` (`src/ECS/Trees.cpp`), from the forester (`src/ECS/Villager/VillagerForester.cpp`); the fall sound in `EndTurn` (felled-tree kind) |
| Reward chests don't use the physics while falling: they appear 150 m up, fall at 50 m/s turning at π rad/s, and on landing play their thump, shake the camera and raise 27 dust sprites | todo | No reward chests (`CreateReward` in `src/CHLApi.cpp` is a stub) |
