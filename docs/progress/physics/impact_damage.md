# Impact damage

What happens to people, animals, the creature, buildings, rocks and other things when a moving body strikes them or
they strike the ground: crush damage judged by how hard the blow was, buildings cracking where a rock hits, rocks
chipping and splitting, and the feedback (sounds, the thrower's credit, the creature copying the player). How bodies
move and collide is in [object_dynamics.md](object_dynamics.md) and [collisions.md](collisions.md), thrown villagers and
animals in flight in [thrown_living.md](thrown_living.md), and throwing itself in
[throwing_and_landing.md](throwing_and_landing.md).

openblack measures every blow the game's way (`EndTurn` in `src/ECS/Physics/PhysicsObjects.cpp`)
and hands it to each kind's impact reaction (`ReactToPhysicsImpact`): people and animals are hurt straight off their
life rather than through the effect path (`src/ECS/LivingPhysics.cpp`), the creature has no reaction yet, rocks wear
and split (`src/ECS/Rocks.cpp`), and resources go into stores. Rocks break buildings into pieces through `src/ECS/Physics/Buildings.cpp` and `src/ECS/Physics/FragMesh.cpp` (test `test/test_fragmesh_light.cpp`); over a broken building the damaged model is drawn with the partly built model over it (`src/ECS/Physics/PartialBuild.cpp`).

**Progress: 23/47 done, 7 partial — 56%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## How a blow is measured

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At the end of each game turn (after all physics steps), every body whose total force over the turn's touched steps (gravity included) is above a tiny limit gets an impact strength: that total, scaled by 0.05 | done | `EndTurn` in `src/ECS/Physics/PhysicsObjects.cpp` (the touched substeps' force sum times 0.05) |
| Each body also remembers which other body hit it that turn, and the player whose hand threw it (taking the hitter's when it has none) | done | `EndTurn` (`PhysicsObject::hitBy`, `byPlayer` taken from the hitter) |
| A blow is judged in multiples of the body's own weight under gravity ("how many g", about 0 for something lying still); a heavy body needs a harder blow to be hurt | done | `PhysicsObject::GLoad`, passed to each class's reaction in `ImpactInfo`. Our wiki differs: G is about 1, not 0, for a body lying on the ground ([physics.md](../../bw1-notes/physics.md#manager-physicsobjectgameturnupdate-0x644fc0)) |
| Every object with a model that is hit by moving things takes part in collisions while something moves near it, so a thrown rock can strike a standing villager or a house (standing trees are not hit) | done | The wake walk in `BeginTurn` makes resting proxies of what interacts (`src/ECS/Physics/PhysicsObjects.cpp`) |
| An impact plays a collision sound picked by both materials, at one of three loudness levels by how many g it was | done | `CollisionSounds::AttemptToAddSoundEvent` (`src/ECS/Physics/CollisionSounds.cpp`); see [throwing_and_landing.md](throwing_and_landing.md) |
| A thrown thing that can be used on what it hit (an animal or a tree into a store, food or wood into a store or a pile of the same; a toadstool poisons the store's food) is used on it instead of hurting it | partial | Trees and dead trees go into a storage pit (`HandPhysics.cpp`), fences into a wood store and pots into a store or pot of their type (`ReactToPhysicsImpact`, `held_apply`), animals into a food store (`ECS/LivingPhysics.cpp`); mushrooms and the toadstool's poison are not handled |

## People and animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal is hurt only by a blow above 2 g | done | `LivingReactToPhysicsImpact` (`src/ECS/LivingPhysics.cpp`) |
| The harm is the game's crush effect scaled by (g − 2) × 0.03, applied by what hit it with the thrower's player, through the body's own defences | partial | (g - 2) x 0.03 times the animal's crush defence, taken straight off the life (`HurtByImpact`); not through the effect path, so no thrower's player is credited |
| The same rule is the fall damage: a villager or animal thrown hard enough is hurt or killed by its own landing | done | The landing's blow goes through the same reaction (`ECS/LivingPhysics.cpp`) |
| A villager that cannot take effects (not available, at home, in a hand, or hiding in a building) is not hurt | partial | Villagers at home, held or hidden are out of the map cells and the physics, so nothing hits them; the impact itself does not test whether the villager takes effects |
| A villager whose life reaches nothing while standing dies as any villager dies, credited to the thrower's player; one hurt to nothing in flight dies when it lands | partial | `villager::DestroyedByEffect` with no player (the impact's player is not kept); one hurt to nothing in flight dies at its landing (`VillagerEndPhysics`) |
| An animal killed by a blow dies (it falls and lies; in flight it starts dying when it lands), it does not just vanish | done | `animal_ai::DestroyedByEffect` (dying; in flight its landing does it) |
| The harm moves the thrower's alignment by the crush it did, weighted by the victim's alignment kind | todo | The impact damage does not go through the effect path, so no alignment changes |
| Hurting a town's villager makes the town count the thrower's player as an attacker | todo | Not from impacts (no effect path) |
| A town keeps a count of its injured people: it rises when a villager's life falls below 0.7 and falls when it climbs back above | todo | No count of injured villagers in our tree |
| Villagers nearby react to someone crushed | todo | No crushed reaction from impacts |
| A villager or animal standing in the way is knocked into the air by any blow its resting body can't hold (the scripted 30 m/s launch happens only through the temple's redirect) | partial | The knock into flight is done (`Result::Pushed`, the class's `initialisePhysicsKnocked` in `ECS/LivingPhysics.cpp`); the temple's redirect launch is not ported |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature struck by a thrown body takes crushing harm of how many g the blow was for its mass, capped at 100, times 0.01 (a toy does nothing) | todo | The creature's physics class has no impact reaction (`src/ECS/CreaturePhysics.cpp`) |
| A thrown thing striking a creature while it fights takes none of its fight health or life and doesn't make it reel | todo | No creature impact reaction in our tree |
| A creature a script controls, belonging to the player, isn't hurt by blows | todo | No creature impact reaction in our tree |
| Blows worth less than 0.005 are ignored | todo | No creature impact reaction in our tree |
| A creature is never killed by blows: at no life it faints | todo | No creature impact reaction in our tree |
| When its own player hit it, the creature thinks less of its player by the harm done | todo | No creature impact reaction in our tree |
| Being hit feeds its fear and its anger from being damaged by the harm done (out of a fight, a second time through its cut-and-scar code) | todo | No creature impact reaction in our tree |
| A hit sways the creature's upper or lower body by the blow's sideways force (except in some of its animations) | todo | No sway in our tree |
| A creature's own throwing (at buildings, villagers, the camera, the sea) uses the same blows | done | What the creature lets go of flies through the physics (`CreatureObjectActionSystem.cpp` to `PhysicsObjects::AddObject` or `from_hand::InitialisePhysicsFromHand`) |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only rocks (and the bowling ball) damage buildings; anything else thrown at a house just bounces off | done | `Buildings::PhysicallyDestroysAbodes` (constants rows 3 and 20), `src/ECS/Physics/Buildings.cpp` |
| The blow is judged by momentum (speed × weight): up to 300 nothing happens | done | `Buildings::ReactToPhysicsImpact` (speed times mass of the hitter) |
| From 300 to 1000 a light knock sound plays, from 1000 to 2000 a heavier one, and the building is not hurt | done | `Buildings::ReactToPhysicsImpact` (the two rock-on-ground sounds) |
| Above 2000 a built building cracks: the part of its model within the rock's size plus 0.7 m of the line through the impact along the rock's motion breaks away | done | `FragMesh::Impact` (`src/ECS/Physics/FragMesh.cpp`: the cylinder of the rock's radius plus 0.7 along its motion) |
| The broken part flies off as its own piece, with 0.3 of the rock's velocity and a small random spin, and then lies as debris | done | `FragMesh::Impact` and `CreateFragment` in `Buildings.cpp` (0.3 of the velocity, the game's random spin) |
| The building's damaged model is kept, so later hits keep breaking it down; a building repaired past 0.2 life gets a fresh one | partial | The FragMesh is kept and drawn with the partly built model over it (`components::BuildingDamage`, `src/ECS/Physics/PartialBuild.cpp`); villagers do not repair, so it is never made afresh |
| The building loses life to match the share of its model knocked away (crush, through its defence), with a big crash sound | done | `abodes::OnPhysicalDamage` with the remaining share and the collapse sound |
| A building completely broken is destroyed (its ruins and rebuilding are in [../building/](../building/)) | done | `abodes::OnPhysicalDamage` (`src/ECS/Abodes.cpp`); see [../building/](../building/) |
| A building damaged this way empties: its people come out | done | `abodes::ReduceLife`: every inhabitant reacts as to a tap, the building stops working |
| Storage pits and the village centre take blows as other buildings do; scaffolds and totem statues ignore them | done | `Buildings::ReactToPhysicsImpact` for abodes and storage pits; scaffolds and totem statues have no reaction |
| A local player's throw that leaves less than 40% of an on-screen building shows the help sprites about destroying buildings | todo | Not in `Buildings.cpp` |
| A creature's blow on a house breaks it like a rock's | partial | The creature's knock-down action gives a home a blow through `abodes::OnPhysicalDamage` (`CreatureObjectActionSystem.cpp`, approximate: no breaking model); a creature thrower is not ported in `Buildings::ReactToPhysicsImpact` |
| Rocks thrown at an enemy temple are first redirected, with a plasma beam, to one of its owner's buildings (towns oldest first, each town's buildings newest first; life over 0.25 preferred) or else a homeless villager, the redirected hit carrying no speed; otherwise they hurt it by speed × weight × 0.000005, at most 0.2 a blow, and the harm passes to its heart | todo | No temple redirect, plasma beam or heart damage in our tree. See [../temple/](../temple/) |

## Trees, rocks and other things

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown things do not knock trees or dead trees down; only things meant to be used on them act | done | Standing trees are not obstacles (`InteractsWithPhysicsObjects`). Our wiki differs: dead trees are obstacles, and a resting obstacle hit hard enough is knocked into flight ([physics.md](../../bw1-notes/physics.md#manager-physicsobjectgameturnupdate-0x644fc0)) |
| A rock that lands harder than 4 g, if taller than 0.7 m and not hit by another rock, loses a little life, (g − 4) × 0.005 | done | `ReactToPhysicsImpact` in `src/ECS/Physics/PhysicsObjects.cpp` |
| A rock worn below 1% life splits into two smaller rocks (a burning rock's fire goes to both) | done | `Rocks::SplitInTwo` (`src/ECS/Rocks.cpp`: halves of 0.7935 its scale, its velocity, half its spin, `fire::CopyFire`) |
| Broken pieces, bonfires, scaffolds and totem statues have no reaction to blows | done | No reaction for them in `ReactToPhysicsImpact` |
| Rocks thrown at a physical shield cost it power and anger the town it guards | done | `map_shield::ReactToPhysicsImpact` (`src/Magic/Objects/MapShield.cpp`: pays by momentum, the town records the aggressor); see [../miracles/physical_shield.md](../miracles/physical_shield.md) |

## Feedback

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While a person or rock thrown from the hand (not a tree) feels any blow, the player's creature may learn "damage by throwing" from it | todo | Not from impacts of people or rocks; only the building branch asks the creature to copy (`creature_mimic::Consider` in `Buildings.cpp`) |
| When it was hit by something, it is also "damage by throwing at" | todo | The building branch is there and, as in the game, never fires; nothing for thrown people and rocks |
| Challenge scripts can ask what was last hit and by what, and clear it | todo | GET_HIT_OBJECT, GET_OBJECT_WHICH_HIT and CLEAR_HIT_OBJECT are stubs in `src/CHLApi.cpp` |
