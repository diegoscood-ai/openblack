# Thrown villagers, animals and the creature

Villagers and animals as physical bodies: how they fly when thrown or knocked, land in one of three poses, get up, or die
from the fall; and why the creature never flies. Picking them up and holding them is in [../hand/picking_up.md](../hand/picking_up.md) and [../hand/holding.md](../hand/holding.md), damage
from hits in [impact_damage.md](impact_damage.md), drowning in [water_physics.md](water_physics.md), and the creature's
falls in fights in [../creature/fighting.md](../creature/fighting.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack: villagers and animals fly in the game's physics with their own 12-point bodies
(`SetUpLivingBody`, `src/ECS/Physics/PhysicsObjects.cpp`). A villager thrown, dropped or knocked enters FLYING; on landing it takes the
game's posture and heading, plays its landing (or dies of the fall, or drowns on water) and decides afresh
(`src/ECS/LivingPhysics.cpp`); animals land, die of the fall and move their flock's home
(`src/ECS/LivingPhysics.cpp`, `src/ECS/AnimalAI.cpp`); the creature is struck but not hurt (a stand-in ball body,
`src/ECS/CreaturePhysics.cpp`). Tests: `test/test_living_landing.cpp`. The creature's catching, its hurt and its sway
aren't ported.

**Progress: 18/24 done, 4 partial — 83%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../bw1-notes/physics.md).

## Being let go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal put down gently on ground that is not too steep stands where it is put, without flying, and goes to its landed state | done | `from_hand::InitialisePhysicsFromHand` takes it out of the physics at once and its end of physics runs to LANDED (`src/ECS/LivingPhysics.cpp`) |
| A thrown villager or animal goes into its flying state and plays its kind's thrown animation (villagers: thrown, thrown dead, thrown by a vortex; each animal kind its own) | partial | FLYING with the thrown or thrown-dead clip (`src/ECS/VillagerAnimations.cpp`) and each animal kind's thrown clip (`src/ECS/AnimalAnimations.cpp`); the vortex clip is not used |
| A thrown villager drops what it was carrying, which flies on separately with the same speed and hits no other object | done | `villager::CreateDroppedResource` (from `FromHand.cpp` and `ECS/LivingPhysics.cpp`) with `PhysicsObjects::AddDroppedObject` and no object collisions |
| Animals taken from a flock leave it for a flock of their own; on landing, that flock's home moves to where they came down | done | `animal_ai::PlaceInHand` takes it off its flock; on landing the flock centres where it fell, predators recompute their lair (`src/ECS/AnimalAI.cpp`) |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Their bodies are simple 12-point shapes (head, waist and feet) with twice the air drag of objects | done | `SetUpLivingBody` in `src/ECS/Physics/PhysicsObjects.cpp` (drag x 2) |
| Villagers are heavy and grippy (density just under water's, the highest friction); animals are lighter and slide more | done | Constants rows 7 and 8 (`PhysicsObjects::ConstantsType`, `Data/PhysicsConstants.txt`) |
| They tumble only from what they hit and the hand's twist after release | done | `PhysicsBody` contacts and the release twist in `src/ECS/Physics/FromHand.cpp` |
| A villager or animal hit by a flying object while standing is knocked into flight | done | `Result::Pushed` and the class's `initialisePhysicsKnocked` (`src/ECS/LivingPhysics.cpp`) |

## Landing and getting up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| They land on their back, on their front or on their feet, by which way their body leans sideways at the end of the flight | done | `VillagerEndPhysics`, `AnimalEndPhysics` from the turn-start rows; tests `LivingLanding.VillagerPoseThresholds`, `AnimalLandTypeIsReversed` |
| On landing they are stood up facing the way they were heading, on the ground | done | `src/ECS/LivingPhysics.cpp` (the heading into `SetYAngle`); tests `LivingLanding.VillagerSetYAngle`, `ObjectYAngle` |
| They then play a landing animation (each animal kind its own) and get up to decide what to do next | done | The landed clips by land type (`VillagerLandedClip`, the animal kinds' clips), then they decide (`villager::Landed`, `AnimalAI.cpp`) |
| Nothing special happens to a breeder that lands: it decides afresh what to do | done | It decides afresh after its landing (`src/ECS/LivingPhysics.cpp`) |
| One that lands on water starts drowning instead | done | A villager in a water cell goes to DROWNING (`VillagerEndPhysics`, `src/ECS/VillagerDrowning.cpp`). Our wiki differs: animals have no drowning state; in the sea they sink and are deleted ([animals.md](../../bw1-notes/animals.md#hand-flight-and-death)) |

## Dying from a fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hits harder than 2 G hurt them; how much is in [impact_damage.md](impact_damage.md) | done | `LivingReactToPhysicsImpact` (`src/ECS/LivingPhysics.cpp`); see [impact_damage.md](impact_damage.md) |
| A villager whose life runs out in the air dies only when it lands, killed by the player who threw it | partial | It dies where it lands (`VillagerEndPhysics`); the impact damage does not keep the thrower's player |
| A villager's corpse that is thrown lands dead again | done | `VillagerEndPhysics` (an already dead one goes back to its dead state) |
| An animal whose life runs out in the air starts dying when it lands | done | `AnimalEndPhysics` to `animal_ai` dying |

## Reactions of others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby point at or run from things flying by (the game never uses its watch state for this) | todo | The flying-object reaction is spread (`animal_ai::SpreadFlyingObjectReaction`), but villagers have no handler for it |
| The creature watches flying objects and may learn from the player's throwing of people | todo | The catch hook of `PhysicsObjects::CheckAllCreaturesForCatching` is never set and no throw of people teaches the creature |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is never a physics body that moves: the hand can't throw it and hits never send it flying | done | `PhysicsObjects::CanBecomeAPhysicsObject` is false for a creature; its body is static (`src/ECS/CreaturePhysics.cpp`) |
| It is an obstacle: thrown things bounce off it and hurt it | partial | Thrown things bounce off a stand-in ball body (`creature_physics::SetUpBody`); they do not hurt it |
| Its falls in fights (recoils and being knocked out) are animations, not physics | partial | `src/ECS/Systems/Implementations/CreatureFightSystem.cpp`; see [../creature/fighting.md](../creature/fighting.md) |
| No physics path makes the creature fall over: its falls belong to its fights and animations | done | The creature's body is static: nothing in the physics moves it. See [../creature/fighting.md](../creature/fighting.md) |
| Miracle animals (wolves, bats, doves) have their own thrown and landed animations; the puzzle challenge's living pieces never fly | done | Each kind's clips (`src/ECS/AnimalAnimations.cpp`); animals whose info forbids it cannot be picked up (`animal_ai` `playerCanPickUp`) |
