# Water physics

What happens to things that fall into the sea: the splash, floating or sinking, bobbing and drifting, soaking up water
until they go under, and villagers and animals drowning. The sea itself is in [../ocean/](../ocean/); what a drowning
does to a villager's family and town is in [../villager/](../villager/).

openblack floats and sinks bodies in the game's own physics: the sea test, buoyancy, water drag and
soaking are in `PhysicsBody::GroundAndWater` (`src/ECS/Physics/PhysicsBody.cpp`), `src/ECS/Physics/CollisionSounds.cpp`
makes the water rings (`ecs::AddWaterRing`), the foam puffs and the water sound of a splash, and `Substep` makes the
bobbing rings. A sunk villager drowns for its type's drowning time and dies (`VillagerEndPhysicsInWater`,
`VillagerDrowningState`, `src/ECS/VillagerDrowning.cpp`), a sunk animal dies and is gone (`ecs::HasSunk`), and a tree
that ends in the water becomes a dead tree (the tree's end of physics in `HandPhysics.cpp`). Tests:
`test/test_physics_body_water.cpp`.

**Progress: 25/27 done, 2 partial — 96%**

How the original does it, in our wiki: [Water in the game](../../bw1-notes/water.md), [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Falling into the water

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A body is in the water when the land under its centre is at sea level, its centre is less than one radius above the sea, and the map cell under its first point has no land | done | `PhysicsBody::GroundAndWater` and `PhysicsBody::IsLandUnder` (`src/ECS/Physics/PhysicsBody.cpp`); tests `PhysicsBodyWaterTest.NearestCellDecides`, `NoCellIsWater` |
| Only the sea at height 0 counts as water for the physics: raised lakes are not water to it | done | `PhysicsBody::GroundAndWater` (the surface is height 0) |
| Hitting deep water plays the water collision sound, six foam puffs and a ring, and makes fish within 8 m dart away | done | `CollisionSounds::AttemptToAddSoundEvent` (water sound, foam, ring) and `ecs::SplashWater`, which scares the fish shoals (`src/ECS/FishShoals.cpp`) |
| Hitting the water makes a ring that grows to twice the body's radius, faster for small bodies | done | `CollisionSounds.cpp` (ring at y 0.1, growth 2 R, rate 1 / R) into `ecs::AddWaterRing` |
| Six foam-coloured puffs rise where it hits the water | done | `CollisionSounds.cpp` and `src/ECS/Physics/Dust.cpp` (the foam colour) |
| Over the shallow shore it makes both a ring and dust, with the ground's sound | done | `CollisionSounds.cpp` (dry land, shore and deep tests) |

## Floating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The water lifts a body by how deep its centre is (as a share of its size) times its weight, divided by its density: lighter than water floats, heavier sinks | done | `PhysicsBody::GroundAndWater` (buoyancy fraction x m g / density) |
| The water slows a body a hundred times more than the air does, by how much of it is under | done | `PhysicsBody::GroundAndWater` (drag x 100) |
| The water's push acts at the centre; only its turning effect is shared among the points under the surface, so floating things tilt and settle | done | `PhysicsBody::GroundAndWater` |
| A floating body bobs; whenever its vertical speed turns round while its centre is below half its radius above the sea, it makes a small ring | done | `Substep` in `src/ECS/Physics/PhysicsObjects.cpp` (`AddRipple`) |
| Floating things drift only with their own speed: there are no currents or wind | done | No current or wind force in `PhysicsBody` |
| While under the sea a body never touches the sea floor | done | `PhysicsBody::GroundAndWater` (no contact with the bottom) |

## Soaking and sinking

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Things soak up water: their density rises a little every step they are in it (about 0.013 a second), so floating things sink in the end | done | `PhysicsBody::GroundAndWater` (density rises each submerged substep) |
| How long things float: wood in the hand about 15 s, trees about 20 s, carried food about 30 s, the lightest toys over a minute; animals go under in a few seconds and villagers almost at once; rocks sink straight away | done | Follows from the materials and the soaking rate (`Data/PhysicsConstants.txt`, `PhysicsBody`); see [../nature/toys.md](../nature/toys.md) |
| Something has sunk when it is denser than water and its centre is less than half its radius above the sea | done | `Substep` (centre below half its radius and density over 1, then the class's `hasSunk` or `ecs::HasSunk`) |
| Ordinary objects that sink keep going down and are deleted deep under the sea | done | `ecs::HasSunk` is false for them (`src/ECS/VillagerDrowning.cpp`); deleted below minus four radii |
| An animal that sinks dies and is gone (the creature may learn "throw in the sea" from it) | partial | `ecs::HasSunk` sets it dying and deletes it; the creature does not learn from an animal yet (only from a villager) |
| Carried food or wood that comes to rest in the water stays a floating object rather than becoming a pile | done | The pot class's end of physics makes a pile only on land (`HandPhysics.cpp`) |
| A tree that ends in the water is not planted again but becomes a dead tree | done | The tree class's end of physics (`HandPhysics.cpp`: replanted only when landed on land) |

## Drowning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager that sinks stops where it is and starts drowning (one already dead goes to dying instead) | done | `ecs::HasSunk` and `VillagerEndPhysicsInWater` (`src/ECS/VillagerDrowning.cpp`) |
| A villager that lands on a water cell, the shallow shore included, drowns too; one already dead just dies | done | `VillagerEndPhysicsInWater` from the villager's end of physics (the cell's water bit) |
| A drowning villager struggles for its type's drowning time (600 turns, 60 s, for every villager type in info.dat), then dies drowned | done | `VillagerDrowningState` (the info's drowning time, 600 turns) |
| The drowning is credited to the player who dropped or threw the villager, else to the last player who interacted with it | done | `VillagerDeadDrowned` with the last player to interact (`RememberLastPlayerToInteract`) |
| An indestructible villager never finishes drowning (its count is reset to 10 each turn) | done | `VillagerDrowningState` holds the counter at 10 for `Indestructible` (SET_INDESTRUCTABLE in `src/CHLApi.cpp`) |
| A drowning villager plays its drowning animation, with swimming splashes and a scream | done | The drowning clip and its sound events (`src/ECS/VillagerAnimations.cpp`, the clip's sounds) |
| Scripts can ask whether something is drowning: a villager in the drowning state, or an object in the physics with its centre under the sea | done | `ecs::IsDrowning` for GET_PROPERTY (`src/CHLApi.cpp`) |
| Throwing a villager or animal into the sea is something the creature can learn from | partial | A villager: `creature_mimic::ConsiderThrownInTheSea` from `ecs::HasSunk`; an animal: not yet |
