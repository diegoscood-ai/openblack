# Throwing and landing

The physics side of letting go of something: the speed it leaves the hand (or the creature's hand) with, whether it is
put down or thrown, its flight, and what each kind of object does when it lands. Grabbing, holding and the hand's states
are in [../hand/throwing.md](../hand/throwing.md). Damage done by what lands is in [impact_damage.md](impact_damage.md); burning objects that
start flying leave their fire group (see [fire.md](fire.md)). No craters or ground marks from landings exist in the game.

openblack: the hand picks things up and lets them go through the game's own release
(`src/ECS/Systems/Implementations/HandHolding.cpp`, `src/ECS/Physics/FromHand.cpp`) into the game's physics
(`src/ECS/Systems/Implementations/DynamicsSystem.cpp`, once `PhysicsObjects`), with landing sounds, dust and rings (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`).
The kinds' own landings are in `src/ECS/Systems/Implementations/HandPhysics.cpp` (trees replanted or
dead, piles), `src/ECS/Systems/Implementations/VillagerPhysics.cpp` and `src/ECS/VillagerDrowning.cpp` (postures, falls, drowning, stores), and the creature lets
go through the same release (`CreatureObjectActionSystem`). Artefacts are not ported ([../town/artefacts.md](../town/artefacts.md)) and reward chests do not
land through the physics (they fall by their own rule: [../story/rewards.md](../story/rewards.md)); scaffolds land through `src/ECS/Scaffolds.cpp` ([../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md)). Tests: `test/test_release_prediction.cpp`, `test/test_living_landing.cpp`.

**Progress: 35/45 done, 0 chosen, 8 partial, 1 inferred — 92%**

How the original does it, in our wiki: [Physics: thrown objects, collisions, damage and rocks that split](../../../physics/physics.md).

## Letting go from the hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| What is held follows the hand through a spring updated every 10 ms (pull 260 times the gap, damping 40 times the speed); the throw speed is the spring's speed at the moment of release | done | The hand's spring in `src/Hand/HandGrabRules.cpp` (`k_SpringStiffness` 260, `k_SpringDamping` 40, 10 ms steps); tests `HandGrab.TheSpringSettlesOnItsTargetAndIsCapped`, `HandHoldPose.TheHandTrailsADragOnItsSpringAndThrowsAtTheDragsSpeed` |
| The throw speed is capped at 124 m/s | done | `src/ECS/Physics/FromHand.cpp` (the capped velocity); test `ReleasePrediction.CapReleaseVelocity` |
| The hand gives what it lets go no spin; 180 ms later whatever it let go that still has a body gets a one-turn twist of 1.6 × its mass × its speed about the level axis across the hand's motion | done | The release twist in `src/ECS/Physics/FromHand.cpp` (1.6 x mass x speed into the external torque for one turn); tests `HandGrab.ATwistTurnsAboutTheLevelAxisAcrossTheHandsMotion`, `HandGrab.TheTwistIsCancelledWhenItsCountdownEndsOnNothing` |
| The object starts its flight from the pose it had in the hand | done | `from_hand::PredictRelease` and `from_hand::Throw` (`src/ECS/Physics/FromHand.cpp`); tests `ReleasePrediction.ObjectOriginIsTheBodysOrigin`, `HandHoldPose.AThrowLeavesFromWhereTheHandDrewTheObjectHoweverFastItMoves` |
| A release counts as a throw when its sideways speed is over 2 m/s (over 1 m/s from the creature); slower it is a drop | done | `from_hand::InitialisePhysicsFromHand` (squared sideways speed over 4, over 1 from a creature); test `HandGrab.AThrowIsFastAcrossTheGround` |
| On release the object is moved up out of the ground; a dropped one is also lowered onto it and, unless it is a tree, turned to lie along the slope | done | `Body::SettleOnLand(thrown, !tree)` from `from_hand::InitialisePhysicsFromHand`; test `PhysicsBodyPose.SettleOnLand` |
| On release the object is raised until it no longer overlaps anything under it | partial 90% | `DynamicsSystem::RaiseClearOfWhatIsUnder` (`Body::OverlapDepth`), run by the hand demos; no unit test pins its steps; see [collisions.md](collisions.md) |
| A drop that needed no raising, over dry land (or a land cell above the lowest heights), is put down where it is, without flying | done | `from_hand::InitialisePhysicsFromHand` (the landed test, `from_hand::MayLand`); tests `FromHandLanding.AThingRaisedOverWhatIsUnderItDoesNotLandUnlessAComputerPlayersVillager`, `HandGrab.APutDownStandsOnlyOnLandWithoutBeingRaised` |
| Villagers, animals and fences dropped on ground steeper than about 45 degrees are not put down but slide off in the physics | partial 90% | `from_hand::InitialisePhysicsFromHand` (land normal y under 0.7: no landing); no test or scenario drops one on a steep slope |
| When the hand is forced to let go, the object is put down where it is held, with no speed, and never replanted (a tree dies) | partial 90% | `from_hand::ForceDrop` (`src/ECS/Physics/FromHand.cpp`); no test covers it |
| Everything the hand throws or drops is credited to its player, for kills, drownings and impressing | done | `PhysicsEntry::throwerPlayer` rides on the body and passes on through hits: impact kills (see [impact_damage.md](impact_damage.md)), drownings (`VillagerDrowning.cpp`), the flying-object reaction; a computer hand's drop and a creature's let-go carry their own player; tests `PhysicsEndOfTurn.DamageByThrowingNeedsTheHandACreditAndALivingThingOrARock`, `ParticleCarriedObjectsTest.AStartFromTheHandKeepsItsThrowerAndItsPlayer`, `ComputerHandHolding.WhatTheHandLetsGoOfIsItsPlayersThrow` |
| A thrown (not put down) object makes nearby people and animals react (when thrown, and again while it flies whenever the turn's walk over the reactions comes to it): villagers look or point (reach 25 m), and any animal nearer than twice the object's speed runs off | partial 75% | `animal_ai::SpreadFlyingObjectReaction` from `FromHand.cpp`; villagers point or run (`villager_reactions::PointAtFlyingObject`, `living_rules::RespondToFlyingObject`), animals flee at 2 × speed (`AnimalFlee.cpp`); tests `PhysicsLiving.*`, scenario `physics.villagers_watch_throw`. Not settled: our wiki says only the predators within 25 m flee ([animals.md](../../../beings/animal-life.md#reactions)), the row says any animal |
| There is no size limit on throwing: rocks wider than 3.6 can't be picked up at all, and anything the hand holds can be thrown | done | `object_physics::RockValidForPlaceInHand` (`src/ECS/ObjectPhysics.cpp`); test `HandGrab.RocksWiderThanTheHandCanLiftStay` |
| A slow release of carried food or wood (speed squared at most 5) puts it straight down as a pile, or into a store close by | done | `from_hand::InitialisePhysicsFromHand` hand pot branch (the put-down effect, piles and stores, lost in water); test `HandGrab.APotOfAnyClassPutsItsResourceDownWhenLetGoGently` |

## Creature throws

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Thrown at a target, the flight time is the time it would take to fall the distance (square root of distance over gravity), and the release speed is the one that reaches the target in that time; a moving target is aimed ahead | done | `creature_throw::FlightTime` and the release velocity (`src/Creature/CreatureThrow.cpp`); tests `CreatureThrow.*` (`test/test_creature_interaction.cpp`) |
| A creature throws nothing at a target nearer than two thirds of its height | done | `creature_throw::FarEnoughToThrow`; test `CreatureThrow.ItThrowsAtNothingTooClose` |
| Tossed away, it leaves with 0.6 of the hand's own speed over the last 100 ms of the animation, turned with the creature and mirrored for the other hand | done | `src/Creature/CreatureThrow.cpp`; test `CreatureThrow.TossingKeepsSomeOfTheHandsSpeed` |
| Put down, it is placed at the hand's height above the ground with no speed, then let go through the same release as the hand's (settled, raised, landed test) with a spin of one about the vertical | partial 80% | `CreatureObjectActionSystem::ReleaseHeld` lets go through `from_hand::InitialisePhysicsFromHand` with no speed (test `CreaturePhysicsTest.WhatACreatureThrowsAtATargetIsPutDownWhereItIsWhenNoBodyCanBeMadeForIt`); missing: the spin of one about the vertical |
| What the creature throws then flies, bounces and rests in the same physics as hand throws | done | `CreatureObjectActionSystem::ReleaseHeld` to `DynamicsSystem::AddObject`; test `CreaturePhysicsTest.WhatACreatureThrowsAtATargetIsItsPlayersThrowWithTheCreatureAsItsThrower`, scenario `objects.kind_throws` |

## Landing, by kind of object

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Rocks and other fixed objects stay where they come to rest | done | The end of physics (`DynamicsSystem::EndPhysics`, `EndPhysicsAsObject`); test `PhysicsObjects.WhatThePhysicsLetsGoOfStandsOnTheMapsGrid`, scenario `physics.throw_one` |
| A rock or other artefact gently put down from the hand by a player within 50 m of a town's building or a worship site becomes that town's (or site's) artefact, and impresses it when it is worth it | inferred | ported from raffclar PR #278, not checked against v1.42: `object_physics::ConsiderArtefact` (from `PhysicsGameHooks::EndPhysics`); test `ArtefactRules.AnArtefactImpressesOnlyATownItDidntLastBelongTo`; see [../town/artefacts.md](../town/artefacts.md) |
| A tree put down gently on land, held upright (tilted less than about 0.2 radians) and not burning, is planted again with a planting sound, joining a forest nearby or starting one | done | `HandSystem::TreeEndPhysics` in `HandPhysics.cpp` (`Replant`, its smoke, forest, spot visual and alignment) and `TreeDropSound`; tests `TreeReplant.*`, `FromHandTree.ALandedTreeIsReplantedUnlessHeldTiltedOrNotToBeReplantedAndAlwaysFromACreature`, scenarios `physics.tree_replanted_good`, `physics.tree_replanted_evil` |
| A tree thrown, dropped tilted, dropped in water or burning falls and becomes a dead tree lying where it stops | done | `MakeDeadTree` from `HandSystem::TreeEndPhysics` (`HandPhysics.cpp`); the same entity keeps its fire; tests `HandGrab.PeopleFencesAndUprightTreesLeaveThePhysicsOnLanding`, `ParticleCarriedObjectsTest.AThrownTreeThatBecomesADeadTreeTellsTheVillagersWithItsThrowersPlayer` |
| Trees planted by the forest miracle that are thrown just stay where they fall, neither replanted nor dead | pending | The forest miracle's trees (`MagicTree`) take the tree class's end of physics (`HandSystem::TreeEndPhysics`), so they are replanted or die |
| A tree or dead tree that hits a wood store goes into it as wood | done | `HandSystem::TreeReactToImpact` in `HandPhysics.cpp` (`held_apply::TreeImpactTakes`, `resource_stores::DeleteObjectAndTakeResource`); test `HeldClassApply.ATreeIsGivenOnlyToAStoreOfWood`, scenario `examples.store_takes_each_class` |
| Thrown carried food or wood that comes to rest on land becomes a pile; in the water it stays a floating object | done | `HandSystem::PotEndPhysics` in `HandPhysics.cpp` (`PutDownHandPot` on land); tests `ParticleCarriedObjects.OnlyAHandPileOutOfTheWaterBecomesALandPile`, `ParticleCarriedObjectsTest.APotLetGoEndsItsPhysicsAsAPot` |
| Thrown food or wood that hits a store, or a pile of the same thing, goes into it | done | `HandSystem::PotReactToImpact` in `HandPhysics.cpp` (`held_apply::PotImpactTakes`, `resource_stores::DeleteObjectAndTakeResource`); test `HeldClassApply.AThrownPotGoesIntoWhatItHitsOnlyWhenBothAreAvailable` |
| Fences and the three mushrooms that hit a store taking their resource go into it | partial 50% | Fences go into a wood store (`DynamicsSystem::ReactToPhysicsImpact`, `held_apply::FenceImpactTakes`; test `HeldClassApply.AThrownFenceGoesIntoAWoodStoreUnlessIndestructible`); missing: the three mushrooms |
| An animal goes into any store that hits it or that it hits, as much as the animal is worth | done | `villager_physics::AnimalReactToImpact` (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`, any food store, `resource_stores`); tests `HeldClassApply.AThrownAnimalGoesIntoAFoodStoreOnlyWhenBothAreAvailable`, `HeldApplyTest.AnAnimalThrownIntoAPitIsItsFoodWithTheThrowersPlayer`, scenario `objects.cow_to_store` |
| A villager or animal lands lying on its back, its front or its feet by how it is tilted, then gets up | done | `VillagerEndPhysicsOfClass`, `AnimalEndPhysicsOfClass` (`src/ECS/Systems/Implementations/VillagerPhysics.cpp`); tests `LivingLanding.*`, scenario `physics.thrown_villager_lands`; see [thrown_living.md](thrown_living.md) |
| A scaffold put down gently snaps to a planned building of the town or starts building there; otherwise a little effect plays where it lands | partial 90% | The scaffold class's handlers in `src/ECS/Scaffolds.cpp` (`OnInitialisePhysicsFromHand`, `OnEndPhysics`: snapped into a workshop's slot, a gentle put-down builds, else the no-building puff); no unit test or scenario lands one; see [../building/workshop_and_scaffolds.md](../building/workshop_and_scaffolds.md) |
| Pieces broken off buildings land on the ground only and vanish after 100 turns per triangle | done | Fragments hit only the land and live 100 turns per triangle (`BuildingDamageSystem.cpp`, `turnsLeft`); scenario `physics.building_breaks` |

## Landing effects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing or hit plays a sound picked by what hit what: each object has a collision sound type, against the ground, the water or the other object's type | done | `DynamicsSystem::AttemptCollisionSound` (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`); test `PhysicsTurn.KnocksSoundWhenHitOrHardEnough`, scenario `physics.throw_many` |
| The sound is soft under 1.25 G, hard over 3 G and medium between (by the unscaled info weight); grain is never hard | partial 90% | `DynamicsSystem::AttemptCollisionSound` (grain never hard); no test pins the three levels |
| A sound plays only when the body hit another object or hit with more than half its weight, and the same pair stays quiet for 2 turns (at most 128 pairs at once) | done | The turn's end (hit or more than half its weight) and `turn::SoundPairs` (2 turns, 128 pairs, `src/Physics/TurnRules.cpp`); tests `PhysicsTurn.KnocksSoundWhenHitOrHardEnough`, `PhysicsTurn.APairStaysQuietForTwoTurns` |
| The sound follows the object it belongs to | done | Played in 3D with the object as the owner (`DynamicsSystem::AttemptCollisionSound`); scenario `physics.throw_many` |
| A rock hitting a building lets the building play its own sound | done | `BuildingDamageSystem::ReactToImpact` plays the knock or the collapse sound; scenario `physics.building_breaks` |
| Six dust puffs rise where it lands, as big as the object (at most 5 m), growing over an eighth of a second and gone after a second of game time; on snow they blend towards the land's colour by the snow's depth | done | Six puffs of size min(2 r, 5) (`DynamicsSystem::AttemptCollisionSound`), tinted by `turn::BlendColour` with the snow depth (`snow_dust::SnowAt`); test `PhysicsTurn.ADustInTheSnowTakesTheLandsLightByTheSnowsDepth`, scenario `physics.throw_in_snow` |
| A body that passes within 10 m of the camera faster than 20 m/s makes one of five whooshing sounds | done | `DynamicsSystem::Substep` (one of five fly-by sounds); test `PhysicsTurn.FastThingsWhooshPastTheCamera` |

## Thrown things as weapons

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A thrown object passes through its thrower for as long as it stays in the physics | done | The thrower skip in `DynamicsSystem::Substep`; test `PhysicsPairs.WhoTestsAgainstWhom` |
| Whatever a thrown object hits carries the thrower's credit on to what it hits next | done | The turn's end passes `throwerPlayer` on (`DynamicsSystem.cpp`); tests `ParticleCarriedObjectsTest.ADeadTreeThatLandsTellsTheVillagersWithItsThrowersPlayer`, `EffectSourceRules.ABlowIsAppliedByWhatStruckWhileTheThrowerIsBlamed` |
| A thrown object that hits the creature hurts it by how hard it hit for the creature's mass (capped at 100 G, a hundredth of that as damage, nothing under a two-hundredth) | done | `creature_struck::HurtCreature` (`src/ECS/CreatureStruck.cpp`) from `PhysicsGameHooks::ReactToImpact`, `living::CreatureCrush`; tests `CreatureStruck.TheCrushIsAHundredthOfTheKnockAndAtMostOne`, `CreatureStruck.AStrikeOfHalfItsWeightOrLessDoesNothing`, `CreatureCrush.*`, scenario `physics.rock_strikes_creature`; see [impact_damage.md](impact_damage.md) |
| A creature hit by something its own player threw likes the player less, and its fear and anger from being damaged rise | done | `creature_struck::HurtCreature`: the feedback only for its own player's throw, then fear and anger from damage; tests `CreatureStruck.OnlyItsOwnPlayersThrowGivesFeedback`, `CreatureStruck.ItTakesTheCrushItsPlayersFeedbackThenFearThenAnger` |
| Creatures try to catch objects flying towards them: at least 1 m/s across the ground, reachable within 5 s, and only 3% of the time when an unallied player threw it | done | `DynamicsSystem::CheckAllCreaturesForCatching` (`ECS/CreatureCatchWalk`); tests `CreatureCatchWalk.*`, `CreatureCatch.*`, scenario `physics.creature_catches` ([../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md)) |
| Thrown objects bounce off a physical shield and cost it prayer power by their momentum | done | The shield's heavy body in the physics and `map_shield::ReactToPhysicsImpact` (`src/Magic/Objects/MapShield.cpp`: momentum × `chantCostPerImpactMomentum` × 0.0001); test `Shield.realInfoDat`, scenario `miracles.shield_physical_throw` |
