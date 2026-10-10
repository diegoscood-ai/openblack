# Throwing and landing

The physics side of letting go of something: the speed it leaves the hand (or the creature's hand) with, whether it is
put down or thrown, its flight, and what each kind of object does when it lands. Grabbing, holding and the hand's states
are in [../hand/throwing.md](../hand/throwing.md). Damage done by what lands is in [impact_damage.md](impact_damage.md); burning objects that
start flying leave their fire group (see [fire.md](fire.md)). No craters or ground marks from landings exist in the game.

openblack: the hand picks things up and lets them go through the game's own release
(`src/ECS/Systems/Implementations/HandHolding.cpp`, `src/ECS/Physics/FromHand.cpp`) into the game's physics
(`src/ECS/Physics/PhysicsObjects.cpp`), with landing sounds, dust and rings (`src/ECS/Physics/CollisionSounds.cpp`).
The kinds' own landings are in `src/ECS/Systems/Implementations/HandPhysics.cpp` (trees replanted or
dead, piles), `src/ECS/LivingPhysics.cpp` and `src/ECS/VillagerDrowning.cpp` (postures, falls, drowning, stores), and the creature lets
go through the same release (`CreatureObjectActionSystem`). Artefacts are not ported ([../town/artefacts.md](../town/artefacts.md)) and there are no reward
chests (`CreateReward` is a stub); scaffolds land through `src/ECS/Scaffolds.cpp` ([../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md)). Tests: `test/test_release_prediction.cpp`, `test/test_living_landing.cpp`.

**Progress: 35/45 done, 5 partial — 83%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Letting go from the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is held follows the hand through a spring updated every 10 ms (pull 260 times the gap, damping 40 times the speed); the throw speed is the spring's speed at the moment of release | done | The hand's spring in `src/ECS/Systems/Implementations/HandPlacement.cpp` (260 and 40, 10 ms steps); the throw speed is the spring's |
| The throw speed is capped at 124 m/s | done | `src/ECS/Physics/FromHand.cpp` (the capped velocity); test `ReleasePrediction.CapReleaseVelocity` |
| The hand gives what it lets go no spin; 180 ms later whatever it let go that still has a body gets a one-turn twist of 1.6 × its mass × its speed about the level axis across the hand's motion | done | The release twist in `src/ECS/Physics/FromHand.cpp` (1.6 x mass x speed into the external torque for one turn) |
| The object starts its flight from the pose it had in the hand | done | `from_hand::PredictRelease` and `from_hand::Throw` (`src/ECS/Physics/FromHand.cpp`) |
| A release counts as a throw when its sideways speed is over 2 m/s (over 1 m/s from the creature); slower it is a drop | done | `from_hand::InitialisePhysicsFromHand` (squared sideways speed over 4, over 1 from a creature) |
| On release the object is moved up out of the ground; a dropped one is also lowered onto it and, unless it is a tree, turned to lie along the slope | done | `PhysicsBody::AdjustToGroundLevel(thrown, !tree)` from `from_hand::InitialisePhysicsFromHand` |
| On release the object is raised until it no longer overlaps anything under it | done | `PhysicsObjects::RaiseUntilNotIntersecting`; see [collisions.md](collisions.md) |
| A drop that needed no raising, over dry land (or a land cell above the lowest heights), is put down where it is, without flying | done | `from_hand::InitialisePhysicsFromHand` (the landed test) |
| Villagers, animals and fences dropped on ground steeper than about 45 degrees are not put down but slide off in the physics | done | `from_hand::InitialisePhysicsFromHand` (normal y at least 0.7) |
| When the hand is forced to let go, the object is put down where it is held, with no speed, and never replanted (a tree dies) | done | `from_hand::ForceDrop` (`src/ECS/Physics/FromHand.cpp`) |
| Everything the hand throws or drops is credited to its player, for kills, drownings and impressing | partial | `PhysicsObject::byPlayer` rides on the body and passes on through hits; the impact damage of villagers and animals does not use it (see [impact_damage.md](impact_damage.md)) |
| A thrown (not put down) object makes nearby people and animals react once: villagers look or point (reach 25 m), and any animal nearer than twice the object's speed runs off | partial | `animal_ai::SpreadFlyingObjectReaction` from `FromHand.cpp`: animals flee it; villagers do not react to it. Our wiki differs: only the predators within 25 m flee a thrown object ([animals.md](../../bw1-notes/animals.md#reactions)) |
| There is no size limit on throwing: rocks wider than 3.6 can't be picked up at all, and anything the hand holds can be thrown | done | `Rocks::ValidForPlaceInHand` (`src/ECS/Rocks.cpp`) |
| A slow release of carried food or wood (speed squared at most 5) puts it straight down as a pile, or into a store close by | done | `from_hand::InitialisePhysicsFromHand` hand pot branch (the put-down effect, piles and stores, lost in water) |

## Creature throws

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown at a target, the flight time is the time it would take to fall the distance (square root of distance over gravity), and the release speed is the one that reaches the target in that time; a moving target is aimed ahead | done | `creature_throw::FlightTime` and the release velocity (`src/Creature/CreatureThrow.cpp`); tests `CreatureThrow.*` (`test/creature/test_creature_throw.cpp`) |
| A creature throws nothing at a target nearer than two thirds of its height | done | `creature_throw::FarEnoughToThrow`; test `CreatureThrow.ItThrowsAtNothingTooClose` |
| Tossed away, it leaves with 0.6 of the hand's own speed over the last 100 ms of the animation, turned with the creature and mirrored for the other hand | done | `src/Creature/CreatureThrow.cpp`; test `CreatureThrow.TossingKeepsSomeOfTheHandsSpeed` |
| Put down, it is placed at the hand's height above the ground with no speed, then let go through the same release as the hand's (settled, raised, landed test) with a spin of one about the vertical | partial | `CreatureObjectActionSystem::ReleaseHeld` lets go through `from_hand::InitialisePhysicsFromHand` with no speed; the spin of one about the vertical is not given |
| What the creature throws then flies, bounces and rests in the same physics as hand throws | done | `CreatureObjectActionSystem::ReleaseHeld` to `PhysicsObjects::AddObject` |

## Landing, by kind of object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks and other fixed objects stay where they come to rest | done | The end of physics (`EndPhysics`, `BackInMap`) |
| A rock or other artefact gently put down from the hand by a player within 50 m of a town's building or a worship site becomes that town's (or site's) artefact, and impresses it when it is worth it | todo | No town artefacts in our tree (`src/ECS/Town/TownProcess.cpp` TODO). See [../town/artefacts.md](../town/artefacts.md) |
| A tree put down gently on land, held upright (tilted less than about 0.2 radians) and not burning, is planted again with a planting sound, joining a forest nearby or starting one | done | The tree class's end of physics in `HandPhysics.cpp` (`Replant`, its smoke, forest, spot visual and alignment) and its drop sound |
| A tree thrown, dropped tilted, dropped in water or burning falls and becomes a dead tree lying where it stops | done | `MakeDeadTree` from the tree's end of physics (`HandPhysics.cpp`); the same entity keeps its fire |
| Trees planted by the forest miracle that are thrown just stay where they fall, neither replanted nor dead | todo | The forest miracle's trees are trees like any other in the physics (`HandPhysics.cpp`), so they are replanted or die |
| A tree or dead tree that hits a wood store goes into it as wood | done | The tree and dead tree impact in `HandPhysics.cpp` (`DepositInStore`) |
| Thrown carried food or wood that comes to rest on land becomes a pile; in the water it stays a floating object | done | The pot class's end of physics in `HandPhysics.cpp` (`PutDownHandPot` on land) |
| Thrown food or wood that hits a store, or a pile of the same thing, goes into it | done | The pot class's impact in `HandPhysics.cpp` (`held_apply::PotImpactTakes`, `resource_stores::DeleteObjectAndTakeResource`) |
| Fences and the three mushrooms that hit a store taking their resource go into it | partial | Fences go into a wood store (`ReactToPhysicsImpact` in `PhysicsObjects.cpp`); mushrooms do not |
| An animal goes into any store that hits it or that it hits, as much as the animal is worth | done | `AnimalReactToPhysicsImpact` (`src/ECS/LivingPhysics.cpp`, any food store, `resource_stores`) |
| A villager or animal lands lying on its back, its front or its feet by how it is tilted, then gets up | done | `VillagerEndPhysics`, `AnimalEndPhysics` (`src/ECS/LivingPhysics.cpp`); tests `LivingLanding.*`; see [thrown_living.md](thrown_living.md) |
| A scaffold put down gently snaps to a planned building of the town or starts building there; otherwise a little effect plays where it lands | done | The scaffold class's handlers in `src/ECS/Scaffolds.cpp` (snapped into a workshop's slot, a gentle put-down builds, else the no-building puff); see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Pieces broken off buildings land on the ground only and vanish after 100 turns per triangle | done | Fragments hit only the land and live 100 turns per triangle (`src/ECS/Physics/Buildings.cpp`) |

## Landing effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing or hit plays a sound picked by what hit what: each object has a collision sound type, against the ground, the water or the other object's type | done | `CollisionSounds::AttemptToAddSoundEvent` (`src/ECS/Physics/CollisionSounds.cpp`) |
| The sound is soft under 1.25 G, hard over 3 G and medium between (by the unscaled info weight); grain is never hard | done | `CollisionSounds::AttemptToAddSoundEvent` (grain never hard) |
| A sound plays only when the body hit another object or hit with more than half its weight, and the same pair stays quiet for 2 turns (at most 128 pairs at once) | done | `EndTurn` (hit or more than half its weight) and the pair list of `CollisionSounds` (2 turns, 128 pairs) |
| The sound follows the object it belongs to | done | Played in 3D with the object as the owner (`CollisionSounds.cpp`) |
| A rock hitting a building lets the building play its own sound | done | `Buildings::ReactToPhysicsImpact` plays the knock or the collapse sound |
| Six dust puffs rise where it lands, as big as the object (at most 5 m), growing over an eighth of a second and gone after a second of game time; on snow they blend towards the land's colour by the snow's depth | partial | Six puffs (`CollisionSounds.cpp`, `src/ECS/Physics/Dust.cpp`); the snow tint is not applied |
| A body that passes within 10 m of the camera faster than 20 m/s makes one of five whooshing sounds | done | `Substep` in `PhysicsObjects.cpp` (one of five fly-by sounds) |

## Thrown things as weapons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A thrown object passes through its thrower for as long as it stays in the physics | done | The thrower skip in `Substep` |
| Whatever a thrown object hits carries the thrower's credit on to what it hits next | done | `EndTurn` passes `byPlayer` on |
| A thrown object that hits the creature hurts it by how hard it hit for the creature's mass (capped at 100 G, a hundredth of that as damage, nothing under a two-hundredth) | todo | The creature's physics class has no impact reaction (`src/ECS/CreaturePhysics.cpp`); see [impact_damage.md](impact_damage.md) |
| A creature hit by something its own player threw likes the player less, and its fear and anger from being damaged rise | todo | No creature impact reaction in our tree |
| Creatures try to catch objects flying towards them: at least 1 m/s across the ground, reachable within 5 s, and only 3% of the time when an unallied player threw it | todo | `PhysicsObjects::CheckAllCreaturesForCatching` is called, but its creature hook is never set |
| Thrown objects bounce off a physical shield and cost it prayer power by their momentum | done | The shield's heavy body in the physics and `map_shield::ReactToPhysicsImpact` (`src/Magic/Objects/MapShield.cpp`) |
