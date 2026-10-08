# Collisions

What flying objects hit and how: the land, other loose objects, buildings, trees, people and the creature, and the edges
of the map. Contacts are springs with friction at every point of a body (see [object_dynamics.md](object_dynamics.md)).
What the hits do to life and buildings is in [impact_damage.md](impact_damage.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack tests the game's own way: each body's points against the land (`PhysicsBody::GroundAndWater`)
and against other bodies' faces (`PhysicsBody::CollideVertices`, `src/ECS/Physics/PhysicsBody.cpp`), with the pair
rules and the turn in `src/ECS/Physics/PhysicsObjects.cpp`; the creature is a stand-in ball, not its bones' ellipsoids.
Bullet is still linked but never stepped: it only answers ray casts (`DynamicsSystem`). Tests:
`test/test_physics_body_pose.cpp`, `test/test_physics_body_water.cpp`.

**Progress: 30/33 done, 3 partial — 95%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Against the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every point of a body is tested against the land height under it; the push is along the flat normal of the land triangle there | done | `PhysicsBody::GroundAndWater` (land height per vertex, flat triangle normal through `land_normal`), `src/ECS/Physics/PhysicsBody.cpp` |
| Against the land a body uses its own material's stiffness, damping and friction | done | `PhysicsBody::SetUpConstants` from the class's row of `Data/PhysicsConstants.txt` (`PhysicsObjects::LoadConstants`, `ConstantsType`) |
| A thrown object is only ever pushed up out of the land when it leaves the hand, never pulled down to it | done | `PhysicsBody::AdjustToGroundLevel(noPullDown, ...)` called by `physics::from_hand::InitialisePhysicsFromHand` (`src/ECS/Physics/FromHand.cpp`); test `PhysicsBodyPose.AdjustToGroundLevel` |
| A body over the sea stops touching the land while any of its points is under the water | done | `PhysicsBody::GroundAndWater` sea branch (buoyancy, no contact with the bottom); tests `PhysicsBodyWaterTest.NearestCellDecides`, `NoCellIsWater` |

## Object against object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two bodies are tested only when they are closer than the sum of their radii; then each point of one is tested against the faces of the other | done | `Substep` in `src/ECS/Physics/PhysicsObjects.cpp` (sum of radii), then `PhysicsBody::CollideVertices` |
| Between two bodies the softer stiffness and damping are used, and the friction is 0.3 times the smaller friction (things slide off each other easily) | done | `PhysicsBody::CollideVertices` / `ContactForces` |
| What pushes one body pushes the other back as hard, and turns it | done | `PhysicsBody::ContactForces` puts the force on both bodies; no unit test in our tree |
| Two resting bodies don't collide with each other, except in the step either was just put into the physics | done | `Substep` skips two resting bodies unless either is `justSetUp` |
| A thrown object and its thrower pass through each other for as long as the object stays in the physics list | done | `Substep` thrower test; `ForgetThrower` when the object rests or leaves |
| Villagers' bodies pass through things a villager pushed or kicked, and those through villagers | done | The pair skip in `Substep` (`PhysicsObject::k_PushedByLiving`), set on a forester's felled tree (`ecs::FellTree`, `src/ECS/Trees.cpp`); `PhysicsObjects::PushObject` has no caller yet, so villagers push nothing |
| What a released villager was carrying flies without hitting other objects | done | `villager::CreateDroppedResource` (from `FromHand.cpp` and `ECS/LivingPhysics.cpp`) makes the log through `VillagerStores::MakeDroppedLog` and `PhysicsObjects::AddDroppedObject` with `k_NoObjectCollision` |
| Contacts are tested 0.06 s ahead along the body's motion, so fast objects stop at thin walls such as the physical shield | done | `PhysicsBody::k_LookAhead` and the predicted vertex length in `CollideVertices`; checked against a physical shield ([physics.md](../../bw1-notes/physics.md#the-physical-shield-test)); no unit test |
| Each body remembers what it last hit in the turn; what reacts to a hit knows what hit it, and a body thrown by nobody takes the credit of whoever threw what hit it | done | `EndTurn` in `PhysicsObjects.cpp` (`PhysicsBody::lastHit`, `PhysicsObject::hitBy`, `byPlayer` passed on) |
| How hard a hit was is the mean of the body's total force (gravity included) over the touched steps of the turn; divided by its weight it gives the G of the hit (about 0 for something lying still) | done | `EndTurn` (`impact = the length of forceSum x 0.05`), `PhysicsObject::GLoad`. Our wiki differs: G is about 1, not 0, for a body lying on the ground ([physics.md](../../bw1-notes/physics.md#manager-physicsobjectgameturnupdate-0x644fc0)) |

## What can be hit

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks, statics, mobile objects, villagers, animals, dead trees and carried food or wood are obstacles to flying bodies | done | `PhysicsObjects::InteractsWithPhysicsObjects` (mobile statics and objects, villagers, animals, dead trees, the hand's food and wood pots) |
| Standing trees and forests are not obstacles: thrown things pass through them | done | `PhysicsObjects::InteractsWithPhysicsObjects` (`Tree`, `BigForest` excluded) |
| Fields are never hit | done | `PhysicsObjects::InteractsWithPhysicsObjects` (`Field` and field abodes excluded) |
| Food and wood piles are hit, unless they are part of a storage pit | partial | `PhysicsObjects::InteractsWithPhysicsObjects`: only the hand's food and wood pots are hit; piles are never hit. Our wiki differs: piles do not interact with flying bodies at all ([physics.md](../../bw1-notes/physics.md#manager-physicsobjectgameturnupdate-0x644fc0)) |
| Buildings are obstacles only while more than 10% built and still standing (life above 0.01); ruins are not. The village centre always is; worship sites, graveyards, fish farms, football pitches, lanterns, bonfires and totem statues never are | partial | `PhysicsObjects::InteractsWithPhysicsObjects` tests only the building's life (above 0.01); the more-than-10%-built test and the per-type exceptions are not there |
| A building is a heavy obstacle that never moves; a hit damages it instead | done | Weight 2000, not dynamic (`PhysicsObjects::Weight`, `CanBecomeAPhysicsObject`); the hit goes to `physics::Buildings::ReactToPhysicsImpact` (`src/ECS/Physics/Buildings.cpp`, `FragMesh.cpp`); see [impact_damage.md](impact_damage.md) |
| Pieces broken off buildings hit only the land | done | Fragment bodies have no faces and are not obstacles (`SetUpBody` fragment branch, `InteractsWithPhysicsObjects`), `src/ECS/Physics/Buildings.cpp` |
| The creature is an obstacle that is never moved by the physics: thrown things hit one ellipsoid per bone | partial | `creature_physics::SetUpBody` (`src/ECS/CreaturePhysics.cpp`): a static body, but a ball of six points as tall as the creature, not one shape per bone; the weight is a stand-in |
| A physical shield is always in the physics as a very heavy static dome (weight 50000 × its scale cubed) that thrown things bounce off | done | `BeginTurn` keeps every physical shield (`magic::map_shield::Shields`), weight from its info at the collision scale (`src/Magic/Objects/MapShield.cpp`) |
| A spiritual shield and the miracle vortices are not obstacles | done | `map_shield::InteractsWithPhysicsObjects` (magic shield no); vortices are not among the obstacle classes |

## Knock-back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A resting object hit by a flying body is knocked into flight (rocks, villagers, animals, pots) and flies on its own | done | `PhysicsBody::Result::Pushed` in `Substep`, woken through the class's `initialisePhysicsKnocked` (`ECS/LivingPhysics.cpp`) or `CanWakeKnockedProxy` |
| A rock hit hard may split in two, the halves keeping its speed and half its spin | done | `ReactToPhysicsImpact` in `PhysicsObjects.cpp` calls `Rocks::SplitInTwo` with the velocity and half the angular momentum (`src/ECS/Rocks.cpp`) |
| Whoever threw the first object is credited for what the objects it knocked do | done | `EndTurn` passes `byPlayer` on from the hitter |

## Stuck and embedded objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A released object that overlaps things under it is raised, step by step by the deepest overlap through their tops, until nothing pushes it | done | `PhysicsObjects::RaiseUntilNotIntersecting` with `PhysicsBody::PenetrationAlong` |
| A villager is not raised over things a villager pushed, nor those over a villager | done | `RaiseUntilNotIntersecting` skips `k_PushedByLiving` bodies (a felled tree; nothing else sets it yet) |
| The miracle vortex and map shields never raise objects | done | `RaiseUntilNotIntersecting` only takes what `InteractsWithPhysicsObjects` (false for a vortex and the magic shield) |

## Map edges

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Looking up map cells for a body near or past the edge uses the nearest edge cells of the 512 by 512 map | done | `BoxCell` clamps each corner to the 512 by 512 cells (`PhysicsObjects.cpp`) |
| An object that comes to rest outside the map is deleted | done | `PhysicsObjects::BackInMap` from `EndPhysics` |
| A body that falls more than four radii under sea level (off the land into the deep) is deleted | done | `PhysicsBody::Integrate` returns `Result::Delete`, then `ToBeDeleted` (or `Buildings::DestroyFragment`) in `Substep` |
