# Thrown villagers, animals and the creature

Villagers and animals as physical bodies: how they fly when thrown or knocked, land in one of three poses, get up, or die
from the fall; and why the creature never flies. Picking them up and holding them is in [../hand/picking_up.md](../hand/picking_up.md) and [../hand/holding.md](../hand/holding.md), damage
from hits in [impact_damage.md](impact_damage.md), drowning in [water_physics.md](water_physics.md), and the creature's
falls in fights in [../creature/fighting.md](../creature/fighting.md).

What a villager carries, and the log it lets fall when it is launched, is in [../villager/tools_and_carried_items.md](../villager/tools_and_carried_items.md).

openblack: villagers and animals fly in the game's physics with their own 12-point bodies
(`physics::shapes::LivingBody`, `src/Physics/BodyShapes.cpp`). A villager thrown, dropped or knocked enters FLYING; on landing it takes the
game's posture and heading, plays its landing (or dies of the fall, or drowns on water) and decides afresh
(`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); animals land, die of the fall and move their flock's home
(`src/ECS/Systems/Implementations/VillagerPhysics.cpp`, `src/ECS/AnimalAI.cpp`); the creature is struck and hurt (`src/ECS/CreatureStruck.cpp`) on a stand-in ball body (
`src/ECS/CreaturePhysics.cpp`). Tests: `test/test_living_landing.cpp`. The creature catches what flies at it
([../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md)).

**Progress: 20/24 done, 0 chosen, 4 partial, 0 inferred — 97%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## Being let go

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A villager or animal put down gently on ground that is not too steep stands where it is put, without flying, and goes to its landed state | done | `from_hand::InitialisePhysicsFromHand` takes it out of the physics at once and its end of physics runs to LANDED (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); tests `HandGrab.PeopleFencesAndUprightTreesLeaveThePhysicsOnLanding`, `LivingLanding.AVillagerLetGoWithNoBodyEndsItsPhysicsAtOnce` |
| A thrown villager or animal goes into its flying state and plays its kind's thrown animation (villagers: thrown, thrown dead, thrown by a vortex; each animal kind its own) | done | FLYING with `villager_animation::ThrownClip` (thrown, thrown dead, or thrown by a vortex through `vortex::IsVillagerBeingThrown`, `VillagerAnimate.cpp`) and each animal kind's thrown clip (`src/ECS/AnimalAnimations.cpp`); test `VillagerAnimation.AVillagerAVortexThrowsOutFlails` |
| A thrown villager drops what it was carrying, which flies on separately with the same speed and hits no other object | done | `villager::CreateDroppedResource` (from `FromHand.cpp` and `ECS/Systems/Implementations/VillagerPhysics.cpp`) with `DynamicsSystem::AddDroppedObject` and no object collisions; test `VillagerResourcesTest.CreateDroppedResource` |
| Animals taken from a flock leave it for a flock of their own; on landing, that flock's home moves to where they came down | done | Test `test/test_animal_system.cpp` (`AnimalSystemTest.AnAnimalTakenFromItsFlockGetsItsOwnWhoseHomeMovesWhereItLands`). `animal_ai::PlaceInHand` takes it off its flock; on landing the flock centres where it fell, predators recompute their lair (`src/ECS/AnimalAI.cpp`) |

## Flying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Their bodies are simple 12-point shapes (head, waist and feet) with twice the air drag of objects | done | `physics::shapes::LivingBody` (`src/Physics/BodyShapes.cpp`, drag x 2) from the body set-up in `DynamicsSystem.cpp`; scenario `physics.thrown_villager_lands`, `fidelity`; test `PhysicsBodyRules.ALivingBodyIsTwelvePointsCentredAtHalfHeightWithTwiceTheDrag` |
| Villagers are heavy and grippy (density just under water's, the highest friction); animals are lighter and slide more | done | Constants rows 7 and 8 (`DynamicsSystem::ConstantsType`, `Data/PhysicsConstants.txt`); tests `PhysicsMaterials.*` |
| They tumble only from what they hit and the hand's twist after release | done | `Body` contacts and the release twist in `src/ECS/Physics/FromHand.cpp`; test `FromHandSpin.TheTwistIsOnePointSixTimesTheMassAndSpeedAboutTheLevelAxisAcrossTheHandsMotion` |
| A villager or animal hit by a flying object while standing is knocked into flight | done | The knocked resting proxy (`DynamicsSystem::CanWakeKnockedProxy`) and `VillagerInitialisePhysicsKnocked` (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); scenario `physics.creature_hit` (a cow hit by rocks); tests `VillagerCoreTest.KnockedVillagerFlies`, `VillagerCoreTest.KnockedAnimalFlies` |

## Landing and getting up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| They land on their back, on their front or on their feet, by which way their body leans sideways at the end of the flight | done | `VillagerEndPhysicsOfClass`, `AnimalEndPhysicsOfClass` from the turn-start rows; tests `LivingLanding.VillagerPoseThresholds`, `AnimalLandTypeIsReversed` |
| On landing they are stood up facing the way they were heading, on the ground | done | `src/ECS/Systems/Implementations/VillagerPhysics.cpp` (the heading into `SetYAngle`); tests `LivingLanding.VillagerSetYAngle`, `ObjectYAngle` |
| They then play a landing animation (each animal kind its own) and get up to decide what to do next | done | The landed clips by land type (`VillagerLandedClip`, the animal kinds' clips), then they decide (`villager::Landed`, `AnimalAI.cpp`); tests `LivingLanding.LandedClips`, `LivingLanding.LandedGoesBackWhereTheStateOrAScriptAsks` |
| Nothing special happens to a breeder that lands: it decides afresh what to do | done | It decides afresh after its landing (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); test `LivingLanding.LandedGoesBackWhereTheStateOrAScriptAsks` |
| One that lands on water starts drowning instead | done | A villager in a water cell goes to DROWNING (`VillagerEndPhysicsOfClass`, `src/ECS/VillagerDrowning.cpp`); animals have no drowning state, in the sea they sink and die ([animals.md](../../../beings/animal-life.md#hand-flight-and-death)); test `VillagerDeathTest.EndPhysicsInWater`; scenario `physics.sinking` |

## Dying from a fall

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Hits harder than 2 G hurt them; how much is in [impact_damage.md](impact_damage.md) | done | `LivingReactToPhysicsImpact` (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); tests `LivingCrush.OnlyAKnockStrictlyAboveTwiceItsWeightHurts`, `LivingImpactTest.AtTwiceItsWeightAnImpactTakesNothing`; see [impact_damage.md](impact_damage.md) |
| A villager whose life runs out in the air dies only when it lands, killed by the player who threw it | done | It dies where it lands, killed by the thrower's player (`VillagerSharedLanding`, from `VillagerEndPhysicsOfClass`); scenario `physics.thrown_villager_lands`; test `VillagerDeathTest.AThrownCorpseLandsDeadAgainWithItsCounterKept` (it dies as it lands) |
| A villager's corpse that is thrown lands dead again | done | Test `test/test_villager_death.cpp` (`VillagerDeathTest.AThrownCorpseLandsDeadAgainWithItsCounterKept`). `VillagerEndPhysicsOfClass` (an already dead one goes back to its dead state) |
| An animal whose life runs out in the air starts dying when it lands | done | `animal_ai::EndPhysics` runs the animal's own SetDying (a species' fade, else the common one), a corpse lying dead again; test `AnimalBehaviourTest.AnAnimalThatLandsDeadDiesThroughItsOwnSetDying` ([animal-life.md](../../../beings/animal-life.md)) |

## Reactions of others

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby point at or run from things flying by (the game never uses its watch state for this) | done | The flying-object reaction (`animal_ai::SpreadFlyingObjectReaction`) taken by villagers (`villager_reactions::ReactToFlyingObjectPriority`, `PointAtFlyingObject`: run when nearer than 2 s of its flight, else point); tests `PhysicsLiving.AShelteredVillagerSeesOnlyWhatFliesInsideItsShield`, `PhysicsLiving.ADancerNoticesOnlyWhatFliesAtIt`, `VillagerAnimation.LookingUpAtFlyingThings`, scenario `physics.villagers_watch_throw` |
| The creature watches flying objects and may learn from the player's throwing of people | done | it watches a thrown thing it sees (`LookAtFlyingObject`, [../creature/reactions.md](../creature/reactions.md)), catches what flies at it ([../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md)), and the deeds DAMAGE_BY_THROWING and THROW_IN_THE_SEA report to it (`TurnRules.cpp`, `creature_mimic::ConsiderThrownInTheSea`; [mimicry](../../../creature/mimicry.md)); test `PhysicsEndOfTurn.DamageByThrowingNeedsTheHandACreditAndALivingThingOrARock` |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature is never a physics body that moves: the hand can't throw it and hits never send it flying | done | `DynamicsSystem::CanBecomeAPhysicsObject` is false for a creature; its body is static (`src/ECS/CreaturePhysics.cpp`); test `CreaturePhysicsTest.ACreatureStandsAsItsBodyWhereItIs` |
| It is an obstacle: thrown things bounce off it and hurt it | partial 60% | They hurt it (`creature_struck::HurtCreature`, tests `CreatureStruck.*`, scenario `physics.rock_strikes_creature`); missing: they bounce off a stand-in ball body (`creature_physics::SetUpBody`, test `CreaturePhysicsRules.ACreaturesBodyIsAHeavyBallAsTallAsItThatFacesOut`), not one ellipsoid per bone (#43) |
| Its falls in fights (recoils and being knocked out) are animations, not physics | done | Recoils and the knock-out are fight states and clips in `src/ECS/Systems/Implementations/CreatureFightSystem.cpp` (`CreatureKnockedOut`), never the physics; test `CreatureFight.RecoilsByHeightAndDirection`; the fights' own gaps are in [../creature/fighting.md](../creature/fighting.md) |
| No physics path makes the creature fall over: its falls belong to its fights and animations | done | The creature's body is static: nothing in the physics moves it (test `CreaturePhysicsTest.ACreatureStandsAsItsBodyWhereItIs`). See [../creature/fighting.md](../creature/fighting.md) |
| Miracle animals (wolves, bats, doves) have their own thrown and landed animations; the puzzle challenge's living pieces never fly | done | Each kind's clips (`src/ECS/AnimalAnimations.cpp`); animals whose info forbids it cannot be picked up (`animal_ai` `playerCanPickUp`); tests `AnimalSystemTest.TheHandPicksUpWhatTheSpeciesAllows` |
