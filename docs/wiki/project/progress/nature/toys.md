# Toys

The playthings lying about the lands: the toy ball (the "beach ball" of the Ogre's reward), the bowling ball, the
skittles, the dice and the cuddly toy (the tutorial's teddy bears). They are loose static objects the hand can pick up
and throw, each with its own physics, which the creature plays with when it wants to play and learns to play with by
watching the player. Toys mostly come from the land scripts and from reward chests. Where other files already cover a
rule in full, the row here links to them.

Each toy is a static object with one of five toy models, and the game tells toys apart by their model, not by their
row in the tables. The villagers' football also counts as a toy ball wherever the game asks whether something is a toy
or a ball. The toys' tables say a creature may not play with them, but the game never reads that column for statics:
any static may be played with.

Rows reachable only with a force-feedback device have the status `device`, counted apart and in no score (user, 2026-10-27; [progress/README.md](../README.md#status)).

**Progress: 36/64 done, 0 chosen, 8 partial, 9 inferred, 1 device-only (not scored) — 64%**

## The five toys

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five toys, each its own model: a toy ball, a toy bowling ball, a cuddly toy, a die and a skittle | done | Test `CreatureStruck.TheToysAreTheFiveToyModels`; `MobileStaticArchetype`; the five toy models in `src/3D/AllMeshes.cpp` |
| Anything with one of the five toy models counts as a toy, whatever its table row | done | Test `CreatureStruck.TheToysAreTheFiveToyModels`; `DynamicsSystem::ConstantsType` takes the toy row by the model (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`) |
| The villagers' football is a toy ball too: the creature may kick it about, play throwing games with it, it never hurts the creature, and the hand letting it go counts as playing with a toy | pending | The football exists (`components::Ball`, `BallArchetype`, from a script's CREATE since 2026-10-15) and the creature's planner takes it as a toy ball (`ActOnFactsOf` and `Target::ToyBall` in `CreatureMindLearning.cpp`); missing: its strike still hurts the creature and the hand letting it go does not count as playing with a toy, as `GamePhysicsHooksWorld::IsToy` takes only the toy statics; see [../story/minigames.md](../story/minigames.md) and [../town/football.md](../town/football.md) |
| Their table weights are: ball 100, skittle 250, cuddly toy 1000, die 1000, bowling ball 2000; the weight sets how loud a toy's knocks sound | partial 90% | The collision sound level uses the unscaled info weight (`AttemptCollisionSound`); the weights checked by test `ToysWorld.TheTableWeighsEachToy` (`test/test_toys_world.cpp`); missing: no test of the sound's level; see [../physics/throwing_and_landing.md](../physics/throwing_and_landing.md) |
| The ball and the bowling ball knock like solid stone, the die and the skittle like hollow wood, and the cuddly toy with the soft sound mushrooms make | done | Test `test/test_particle_carried_objects.cpp` (`ToyInfo.TheToysRowsInTheGamesInfoDat` on the game's info.dat: 14 / 20 / 18 / 18 / 14 and the magic mushroom's 20; its synthetic twin `ParticleCarriedObjectsTest.AToysKnockIsTheSoundKindOfItsRow`). The info's collision sound kind (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`) |
| The game's description texts name them "Toy ball", "Toy cuddly", "Toy skittle" and "Toy bowling ball"; the die has no text of its own and reuses the cuddly toy's, so it is described as "Toy cuddly" | pending | No object description texts in our tree |
| Over the hand, a toy feels like fur on a force-feedback mouse | device | No force feedback in our tree; only with a force-feedback device: the hand's feel of what it holds or is over is picked only by `GInterface::UpdateCollideImmersion` (0x5D8320), which returns at once while no force-feedback mouse was set up |
| Fire warms toys and makes them glow, but never hurts them | done | Test `test/test_fire_model.cpp` (`FireModel.AToyWarmsButIsNeverHurt`: no damage and no self-heating with no burn defence) and `test/test_particle_carried_objects.cpp` (`ToyInfo.TheToysRowsInTheGamesInfoDat`: burn defence 0, capacity 5000, combustion 1000 in every toy row). Data-driven; see [../physics/fire.md](../physics/fire.md) |

## Where toys are found

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tutorial land holds 27 toys in two groups of skittles with bowling balls, and a few loose ones: six giant skittles (about 2.6 times normal size) with three bowling balls and two balls; ten normal skittles with two bowling balls; two dice and two more balls about the land | done | `MobileStaticArchetype`; see [../scripts/landT_script.md](../scripts/landT_script.md); no test of the land's own lines, which are its script's data: their placement is tested (`LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo`) |
| Land 1 has a cuddly toy and three tiny dice (a tenth to a sixth of normal size) lying together | done | `MobileStaticArchetype`; see [../scripts/land1_script.md](../scripts/land1_script.md); no test of the land's own lines, which are its script's data: their placement is tested (`LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo`) |
| The Firestorm map and a three-player map each have three cuddly toys | done | see [../multiplayer/maps/firestorm.md](../multiplayer/maps/firestorm.md) and [../scripts/playground_scripts.md](../scripts/playground_scripts.md) |
| Challenge scripts can make a toy at a place with an angle and a size | done | Test `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind` (a mobile static by its row); CHL CREATE of a mobile static (`src/CHLApi.cpp`) |
| The tutorial's picking-up lesson makes five teddy bears on a hillside, makes any destroyed one again with a sparkle, and ends when all five are within 30 of the ditch | pending | see [../story/tutorial_island.md](../story/tutorial_island.md) |
| A toy reward chest holds a ball, a cuddly toy or a die; opened, the toy appears where the chest stood, at normal size | inferred | the reward native gives the chest (`src/CHLApi.cpp`; it matches the original ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original))); opened, the toy is made where it stands (`MobileStaticArchetype::Create`); not checked against v1.42; see [../story/rewards.md](../story/rewards.md) |
| Each toy chest has its good advisor line: "I say. A toy ball.", "Ah. How sweet. A cuddly thing.", "A die. Or is it a dice? No it's a die." | pending | see [../story/rewards.md](../story/rewards.md) |
| Two silver scrolls give the toy ball: Throwing Stones, and The Ogre, whose evil advisor calls it a beach ball | pending | see [../story/silver_scrolls/throwing_stones.md](../story/silver_scrolls/throwing_stones.md) and [../story/silver_scrolls/the_ogre.md](../story/silver_scrolls/the_ogre.md) |

## Sizes

Measured from the game's own models in its mesh pack, in the game's units (its metres: the units its land positions,
distances and the creature's height are given in). Sizes are width × height × depth of each model's most detailed
form at normal size; the people were measured standing in their model's resting pose. For comparison: a grown man
villager stands 1.60 to 1.66 tall (three Celtic men and the footballer measured) and a woman 1.44 (one measured). A creature stands 15 × its size: a new creature is size 0.20 to 0.29
(3 to 4.4 tall, the chimp smallest and the horse largest), size 1 is 15 tall, and a fully grown one at size 2 is 30
tall. At normal size the toy ball is about a seventh of a size-1 creature's height, as a beach ball is to a person,
and it is taller than a villager.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The toy ball is a sphere 2.15 across: 1.3 times a man's height, and half to two thirds of a new creature's height | done | The game's model at the script's size (`MobileStaticArchetype`); test `ToysWorld.EachToysModelIsItsOwnSize` (`test/test_toys_world.cpp`) |
| The bowling ball is a sphere 1.40 across, smaller than the toy ball, reaching a man's shoulder | done | Same; test `ToysWorld.EachToysModelIsItsOwnSize` |
| The cuddly toy is 2.87 wide, 3.81 tall and 1.85 deep: over twice a man's height, as tall as a new creature | done | Same; test `ToysWorld.EachToysModelIsItsOwnSize` (`test/test_toys_world.cpp`) measures the shipped model's box (the 3.70 once written here is no part of the original's model) |
| The die is a cube 2.78 on each side, 1.7 times a man's height | done | Same; test `ToysWorld.EachToysModelIsItsOwnSize` |
| The skittle is 1.23 by 1.26 across and 3.13 tall, nearly twice a man's height | done | Same; test `ToysWorld.EachToysModelIsItsOwnSize` (`test/test_toys_world.cpp`) measures the shipped model's box (1.18 and 3.11 once written here are not its sizes) |
| The villagers' football is a ball 0.37 across, under a quarter of a man's height; a second, identical ball model is also in the list | partial 90% | No test; the football is drawn with its info row's model at the script's size (`BallArchetype`, `src/ECS/Archetypes/BallArchetype.cpp`); see [../story/minigames.md](../story/minigames.md) |
| A land script's size multiplies the model's size | done | The scale passed to `MobileStaticArchetype::Create`; test `ToysWorld.ALandScriptsSizeMultipliesTheModelsSize` (`test/test_toys_world.cpp`), on synthetic and the game's data |
| The tutorial's giant skittles (size 2.62 to 2.68) stand 8.2 to 8.4 tall; its giant bowling balls (sizes 1.84 and 2.62) are 2.6 and 3.7 across and its giant ball (size 2.5) 5.4 across; the other skittles (3.1 tall), bowling balls (1.4), balls (2.15) and dice (2.78) are at normal size | done | `MobileStaticArchetype`; see [../scripts/landT_script.md](../scripts/landT_script.md); the sizes are the script's times the models', tested by `ToysWorld.ALandScriptsSizeMultipliesTheModelsSize` (at the giant skittles' 2.62) and `ToysWorld.EachToysModelIsItsOwnSize` |
| Land 1's cuddly toy (size 0.38) is 1.4 tall, villager height; its three dice (sizes 0.17, 0.17 and 0.11) are cubes 0.47 and 0.31 on a side (the models' boxes at those sizes) | done | `MobileStaticArchetype`; see [../scripts/land1_script.md](../scripts/land1_script.md); the sizes are the script's times the models', tested by `ToysWorld.ALandScriptsSizeMultipliesTheModelsSize` and `ToysWorld.EachToysModelIsItsOwnSize` |
| The tutorial's teddy bears and the Firestorm maps' cuddly toys are at normal size, 3.7 tall | partial 50% | The maps' toys are placed; missing: the tutorial's teddy lesson is not running ([../story/tutorial_island.md](../story/tutorial_island.md)) |
| A toy from a reward chest appears at normal size | pending | `CreateReward` is real; see [../story/rewards.md](../story/rewards.md) |
| The toys' table gives each toy a largest size of 3; whether the game ever enforces it was not traced (no shipped toy is placed bigger than 2.68) | inferred | Not traced: no use of it was found |
| The hand grips a toy by its own size: the ball, die and bowling ball are held from above with a grip as wide as three quarters of the toy's height, the cuddly toy and skittle from the side at the toy's flat radius (half its wider side), both times its placed size; a bigger toy is held in a wider grip | partial 50% | The hand holds what it lifts by the object's box (`HandHolding.cpp`); missing: the toys' own above or side grips are not checked |
| No toy is too big for the hand: only rocks wider than 3.6 are refused, so even the 8-tall giant skittles can be picked up | done | Tests `test/hand/test_hand_grab.cpp` (`HandGrab.PlaceInHandIsAnAllowlist`) and `test/test_particle_carried_objects.cpp` (`ToyInfo.TheToysRowsInTheGamesInfoDat`: no toy row is of the rock kind). Only rocks wider than 3.6 are refused (`object_physics::RockValidForPlaceInHand`); see [../hand/picking_up.md](../hand/picking_up.md) |

## Physics

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each toy has its own material in the physics; a toy with any other model would be as heavy as a rock | done | `DynamicsSystem::ConstantsType` (rows 14, 15, 16, 19, 20 by the model); see [../physics/object_dynamics.md](../physics/object_dynamics.md); tests `PhysicsMaterials.*` |
| The ball is very light (a tenth of water's density) and almost perfectly springy with hardly any damping, so it bounces high and long; air drag slows it quickly | done | Its row of `Data/PhysicsConstants.txt`; tests `PhysicsMaterials.*` (the file's rows) |
| The bowling ball is the heaviest (density 2.04, just over a rock's), grips poorly and keeps its spin, so it rolls a long way | done | Its row of `Data/PhysicsConstants.txt`; tests `PhysicsMaterials.*` (the file's rows) |
| The die lands dead (high damping), grips hard and keeps spinning, with no air drag | done | Its row of `Data/PhysicsConstants.txt`; tests `PhysicsMaterials.*` (the file's rows) |
| The cuddly toy is soft: a tenth of the springiness of the others, so it hardly bounces | done | Its row of `Data/PhysicsConstants.txt`; tests `PhysicsMaterials.*` (the file's rows) |
| The skittle grips well and stops turning quickly | done | Its row of `Data/PhysicsConstants.txt`; tests `PhysicsMaterials.*` (the file's rows) |
| Toys always take part in collisions, so a thrown thing knocks a standing skittle or die | done | A static is in the way by its kind, a toy always (`DynamicsSystem::InteractsWithPhysicsObjects`, [physics.md#a-static-is-in-the-way-only-by-its-kind](../../../physics/physics.md#a-static-is-in-the-way-only-by-its-kind)); test `ParticleCarriedObjectsTest.AMobileStaticIsInTheWayOfWhatFliesOnlyByItsKind` |
| A knocked skittle falls over and lies where it stops; nothing stands skittles up again or counts them | done | The body's last pose is kept (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`); test `ToysWorld.AKnockedSkittleLiesWhereItStops` (`test/test_toys_world.cpp`), on synthetic and the game's data |
| A die lands on whichever face the physics leaves up, or leaning on an edge where the physics' rest leaves it; nothing in the game reads the face | done | No face rule exists; test `ToysWorld.AThrownDieLiesAsThePhysicsLeavesIt` (`test/test_toys_world.cpp`), on synthetic and the game's data |
| In the sea the ball floats for over a minute, the die, cuddly toy and skittle for about 15 seconds, and the bowling ball sinks at once; a sunk toy is gone once it is four of its sizes under | done | Follows from the materials and the soaking rate; see [../physics/water_physics.md](../physics/water_physics.md); tests `test/test_physics_body_water.cpp` |
| Thrown, the bowling ball breaks buildings as a rock does; no other toy harms a building | done | Test `test/test_particle_carried_objects.cpp` (`ParticleCarriedObjectsTest.OfTheToysOnlyTheBowlingBallBreaksBuildings`: the toys' constants 14 / 16 / 15 / 19 / 20 by model, only 3 and 20 break buildings). `DynamicsSystem::PhysicallyDestroysAbodes` (rows 3 and 20) and the building's reaction (`src/ECS/Systems/Implementations/BuildingDamageSystem.cpp`); see [../physics/impact_damage.md](../physics/impact_damage.md) |
| The bowling ball costs a physical shield prayer power by its momentum, as a rock does | partial 90% | No test; `map_shield::ReactToPhysicsImpact` (`src/Magic/Objects/MapShield.cpp`) |
| A toy striking the creature never hurts it | done | `creature_struck::HurtCreature` lets a toy's strike pass; test `CreatureStruck.NothingStrikingItOrAToyDoesNothing`; see [../physics/impact_damage.md](../physics/impact_damage.md) |

## The hand

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand picks up any toy | done | see [../hand/picking_up.md](../hand/picking_up.md); tests `HandGrab.PlaceInHandIsAnAllowlist` (any mobile static not of the rock kind, whatever its size) |
| The cuddly toy and the skittle are held from the side; the ball, die and bowling ball from above | partial 50% | `HandHolding.cpp`; missing: the grip types are not checked per toy |
| Toys are thrown and put down like any loose object | done | `src/ECS/Physics/FromHand.cpp`; see [../hand/throwing.md](../hand/throwing.md); the let-go's rules are tested for every thing alike (`FromHandLanding.AThingPutDownStandsOnDryLandOrWhereTheNearestCellIsAboveAltitudeOne`, `FromHandSpin.TheTwistIsOnePointSixTimesTheMassAndSpeedAboutTheLevelAxisAcrossTheHandsMotion`, `FromHandSettle.AThingLetGoOfIsSettledUnlessACreatureThrowsIt`) |
| A toy the player's hand lets go of, thrown or put down (not one a creature threw), makes the player's creature consider copying "play with a toy" | done | `from_hand::InitialisePhysicsFromHand` reports it for the player whose hand let it go (`from_hand::ToyPlayer`), a computer player's hand for its own player; test `FromHandToy.AToyLetGoOfByAPlayersHandIsThatPlayersPlay` |
| A toy put down gently near a town becomes one of its artefacts; toys count 0.0001 and the bowling ball 0.0002 | inferred | ported from raffclar PR #278, not checked against v1.42: a toy that is a mobile static becomes one; see [../town/artefacts.md](../town/artefacts.md) |
| A toy thrown into a land's exit vortex (or lying within its reach) is taken in, as any thing that can fly is, and comes out of the next land's arrival vortex as the same toy; a toy that is an artefact stays an artefact of the same god with the same worth | partial 40% | The vortex takes in what can fly and writes the crossing file (`src/ECS/Vortex.cpp`, `VortexSave.cpp`); missing: going to the next land is not ported ([vortex.md](../../../world/vortex.md#what-is-ported-and-what-is-pending)); see [../story/portals.md](../story/portals.md#what-goes-in-kind-by-kind) |

## The creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Of the static objects, a creature can pick up only toys | done | `creature_object_actions::DecidedByKind` (`KindRule::Toy`); test `test/test_creature_interaction.cpp`; scene `objects.pickup_kinds` |
| It never eats a toy and never uses one as a weapon to hurt something by throwing | done | Test `CreatureEatingCanBeEaten.*` (`isToy`); The creature eats only food and throws only what it can pick up (`CreatureObjectActionSystem.cpp`) |
| Toys catch its eye: it is more than twice as keen to look at a toy as at any other static (0.9 against 0.4) | pending | No per-object looking interest in our tree |
| Kick a ball about (the toy ball or the football): two to four rounds of walking up to it, facing it and kicking it, each followed one time in four by pointing at it, else a short pause; half the time its happy animation at the end | inferred | plan action `KickBallAround` (`creature_mind::KickBallAround`, the kick `CreatureObjectActionSystem::Kick`: 45 times its shown size, half up and half ahead, tipped by the land's lean; silent, vanilla bug 68 kept; testbed `objects.kick_ball`; raffclar PR #420, unverified against v1.42) |
| Throw a toy at something: half the time its happy animation first, then picks up a toy, moves to a throwing spot its own height from the target, throws at it, and half the time is happy again | pending | The plan action throws any thing it can pick up; no toy |
| Throw a die: picks it up unless it already holds it, throws it at a spot 1.2 times its own height off along both map axes, watches it fly, then is happy two times in three and sad one time in three, whatever face comes up | inferred | plan action `ThrowDie` (`creature_mind::ThrowDie`, the step `WatchFlying`; testbed `objects.throw_die`; raffclar PR #420, unverified against v1.42) |
| Stroke a cuddly toy: picks it up and strokes it in its hand | inferred | plan action `StrokToy` (`creature_mind::StrokeToy`; testbed `objects.stroke_toy`; raffclar PR #420, unverified against v1.42) |
| Take a toy home: a toy more than 50 from its home is carried to a random spot within 5 of home and put down, or one time in five tossed aside | inferred | plan action `TakeToyHome` on a static toy (`Target::ToyAwayFromHome`, the home by `ecs::creature_home::HomeOf`; `creature_mind::TakeToyHome`; testbed `objects.take_toy_home`; raffclar PR #420, unverified against v1.42); see [../creature/home_and_pen.md](../creature/home_and_pen.md) |
| Play a throwing game with the player: picks up a toy and throws it at the hand, then is happy | inferred | plan action `PlayThrowingGameWithPlayer`, at the nearest of the local player's hands (`creature_mind::ThrowToPlayer`; testbed `objects.throw_to_player`; raffclar PR #420, unverified against v1.42) |
| Give a toy to a friend, and play throwing games with a friend using a toy | pending | see [../creature/friends_and_other_creatures.md](../creature/friends_and_other_creatures.md) |
| Any static, toys included, may be thrown into the sea for fun | pending | see [../creature/object_actions.md](../creature/object_actions.md) |
| These are the creature's ways to play: kicking a ball, throwing a toy at something, taking a toy home, stroking a toy and throwing a die are all on its list of things to do when it wants to play | inferred | All five are plan actions the planner weighs (`CreaturePlanActions.cpp`; raffclar PR #420, unverified against v1.42); see [../creature/desires.md](../creature/desires.md) |
| Copying the player playing with a toy: likely (0.9), needs no leash, first a look of noticing the playful deed, then one of: throw a die, stroke a toy, take a toy home, a throwing game with the player, throw it about, kick a ball; feeds the wish to play, about three times | partial 40% | The deed is in the creature's table (`creature_watching::Deed::PlayWithToy`, `src/Creature/CreatureDeeds.h`) and the hand reports it (row above); missing: the copy's own steps are not checked; see [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |
| Catching a toy thrown at it | done | as any thing that may be picked up ([../creature/feeding_and_thrown_things.md](../creature/feeding_and_thrown_things.md)); the football can't be, as in the game; tests `CreatureKindActions.ACatchIsOfAThingItCouldPickUp`, `CreatureObjectActions.OfOtherStaticsOnlyTheToysArePickedUp` |

## Villagers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers never go to play with a toy: the tables give toys the rocks' villager check with no wish to interact | done | Villagers do not seek toys in our tree; no unit test: there is no code path that could send them |
| A toy thrown over villagers is a flying object they watch and point at | pending | Villagers have no handler for the flying-object reaction; see [../villager/reactions.md](../villager/reactions.md) |

