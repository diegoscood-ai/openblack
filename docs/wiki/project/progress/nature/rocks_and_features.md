# Rocks and features

The fixed and loose scenery on the land: rocks and boulders of each land's stone, pillars and spikes, ruins and
monuments, gates, and the small things the land scripts scatter.

**Progress: 8/21 done, 0 chosen, 10 partial, 1 inferred — 75%**

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place rocks and boulders (flat, long, sharp, square and round) in chalk, limestone, sandstone and volcanic stone | partial 80% | Missing: the stored angles are raffclar's port, not checked against v1.42; `MobileStaticArchetype` (`src/ECS/Archetypes/MobileStaticArchetype.cpp`); each keeps the angles it is placed at (`ecs::placed_turn`, the `TurnAngles` component; ported from raffclar's stored-angles lane, 85ec4ceae and 1c38abad2, unverified against v1.42), see [../../../world/objects.md](../../../world/objects.md#angles-of-a-thing) |
| Rocks can be picked up by the hand and thrown | done | Scenes `physics.throw_one`, `objects.pickup_kinds`; `object_physics::RockValidForPlaceInHand` (2D radius up to 3.6, `src/ECS/ObjectPhysics.cpp`), thrown into the physics (`src/ECS/Physics/FromHand.cpp`); see ../hand/ |
| A thrown rock tumbles, lands and hurts what it hits | done | Scene `physics.rock_strikes_creature`; `src/ECS/Systems/Implementations/DynamicsSystem.cpp` and the impact reactions; see ../physics/ |
| A rock hit hard enough splits in two | done | `object_physics::SplitRock` from the impact and `object_physics::TapRock` from the hand (`HandSystem.cpp`) and `SmashRock` from the creature's blow; test `PhysicsObjects.OnlyTallRocksBreakWhenTapped`, scene `physics.rock_split`; detail in [rocks_splitting_and_heat.md](rocks_splitting_and_heat.md) |
| The creature picks up rocks, throws them and attacks with them | partial 50% | Missing: attacking with rocks is not checked; scene `objects.pickup_kinds`; The creature's object actions pick up and throw what it may lift (`CreatureObjectActionSystem.cpp`); attacking with rocks is not checked; see ../creature/ |
| Footpaths go round rocks | done | An obstacle entering the map cells sends the footpaths round it, by its 2D radius (`footpaths::RerouteFootpathsAroundObstacle` from `src/ECS/MapCells.cpp`); `FootpathObstaclesTest.ARockPutOnTheMapBendsTheFootpathRoundIt` in `test/test_footpath_obstacles.cpp`; see ../terrain/footpaths.md |
| Challenge scripts make rocks | done | CHL CREATE of a mobile static makes a rock (`src/CHLApi.cpp`); test `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind` |
| A rock a challenge script makes as a rock goes into the physics at once and settles on the land where it is made (Land 1's throwing stones) | done | `StartPhysics` in `src/CHLApi.cpp` for a row of the rock kind (`script_create::MakesARock`); test `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind`; see [../../../scripts/map-script-reader.md](../../../scripts/map-script-reader.md#the-class-each-mobile-type-makes) |
| A rock a challenge script makes as a mobile static keeps the script's height and falls from there (Land 4's meteors, the cannon's rocks) | done | `script_create::PutIntoPhysicsWhereItIs` from `src/CHLApi.cpp`; test `ParticleCarriedObjectsTest.AScriptsNewRockMadeAsAMobileStaticGoesIntoThePhysicsWhereItIs`; see [../../../scripts/map-script-reader.md](../../../scripts/map-script-reader.md#the-class-each-mobile-type-makes) |

## Features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place features: pillars and spikes in each stone, temples, statues, pyramids, the needle, the acropolis, the mine, the ark and its dock and wreck, the whale, craters, tombstones | partial 90% | No test; `FeatureArchetype` (`src/ECS/Archetypes/FeatureArchetype.cpp`) |
| Features with a physics shape are solid | pending | Features are not among the obstacles of `DynamicsSystem::InteractsWithPhysicsObjects` (`src/ECS/Systems/Implementations/DynamicsSystem.cpp`), so thrown things pass through them; our wiki counts multi-map fixed objects as obstacles ([physics.md](../../../physics/physics.md#manager)) |
| Planned features are drawn as far built as the land script's BUILT_PERCENTAGE says | partial 90% | No test; `src/ECS/FeatureBuild.cpp` (BUILT_PERCENTAGE: the Land 1 ark's dry dock drawn partly built) |
| Features catch fire and burn | done | Fire receivers by their info (combustion 2000, the ark's three at 250, `src/ECS/Fire/FireObjectTraits.cpp`); `FiresOnTheMap.AFeatureBurnsByItsOwnRow` and `FireTraitRows.realInfoDatFeatures` in `test/test_fire.cpp` |
| Features cast shadows | done | The static shadows of fixed objects (`RenderPass::StaticShadow`); scene `light.shadows` |
| Mushrooms and toadstools the creature can eat (magic mushrooms change it) | pending | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) and ../creature/ |

## Animated scenery

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place animated scenery: gates and land transitions | done | Scenes `objects.creature_gate_open`, `objects.creature_gate_close`; `AnimatedStaticArchetype` places them and they play their info's clip (`src/ECS/Archetypes/AnimatedStaticArchetype.cpp`) |
| Gates open when enough gate stones are laid | partial 80% | The plinth takes the stones and the plinth and gate open when the land's script says (scenes `objects.gate_stone_by_hand`, `objects.creature_gate_open`); missing: the script's own rule is not checked running, see ../story/ |
| Gate totems (ape, cow, tiger, blank) stand by the creature gates | inferred | Placed as scenery; whether the original does more with them is not checked |

## Loose objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fences, lanterns, barrels and other loose objects placed by the land scripts | partial 90% | No test; `MobileObjectArchetype`, `MobileStaticArchetype` |
| Loose objects can be picked up, thrown and broken | partial 60% | Missing: breaking them; Picked up and thrown into the physics (`src/ECS/Physics/FromHand.cpp`); a thrown fence goes into a wood store; breaking them is not ported; see ../physics/ |
| Some loose objects crush new buildings or block them | partial 50% | Missing: which objects block a building is not traced; A scaffold set down clears the things in its way (`DestroyThingsInWay` in `src/ECS/Scaffolds.cpp`); which objects block a building is not confirmed |
