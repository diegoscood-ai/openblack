# Rocks and features

The fixed and loose scenery on the land: rocks and boulders of each land's stone, pillars and spikes, ruins and
monuments, gates, and the small things the land scripts scatter.

**Progress: 14/21 done, 0 chosen, 5 partial, 1 inferred — 82%**

## Rocks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place rocks and boulders (flat, long, sharp, square and round) in chalk, limestone, sandstone and volcanic stone | partial 80% | `MobileStaticArchetype` (`src/ECS/Archetypes/MobileStaticArchetype.cpp`); each keeps the angles it is placed at (`ecs::placed_turn`, the `TurnAngles` component; ported from raffclar's stored-angles code, 85ec4ceae and 1c38abad2); missing: the stored angles are not checked against v1.42; see [../../../world/objects.md](../../../world/objects.md#the-angles-of-a-thing) |
| Rocks can be picked up by the hand and thrown | done | Scenes `physics.throw_one`, `objects.pickup_kinds`; `object_physics::RockValidForPlaceInHand` (2D radius up to 3.6, `src/ECS/ObjectPhysics.cpp`), thrown into the physics (`src/ECS/Physics/FromHand.cpp`); see ../hand/; test `HandPickUp.RocksWiderThanThreePointSixCantBeLifted` |
| A thrown rock tumbles, lands and hurts what it hits | done | Scene `physics.rock_strikes_creature`; `src/ECS/Systems/Implementations/DynamicsSystem.cpp` and the impact reactions; see ../physics/; test `LivingCrush.OnlyAKnockStrictlyAboveTwiceItsWeightHurts` (the hurt of a knock) |
| A rock hit hard enough splits in two | done | `object_physics::SplitRock` from the impact and `object_physics::TapRock` from the hand (`HandSystem.cpp`) and `SmashRock` from the creature's blow; test `PhysicsObjects.OnlyTallRocksBreakWhenTapped`, scene `physics.rock_split`; detail in [rocks_splitting_and_heat.md](rocks_splitting_and_heat.md) |
| The creature picks up rocks, throws them and attacks with them | partial 50% | The creature's object actions pick up and throw what it may lift (`CreatureObjectActionSystem.cpp`); scene `objects.pickup_kinds`; missing: attacking with rocks is not checked; see ../creature/ |
| Footpaths go round rocks | done | An obstacle entering the map cells sends the footpaths round it, by its 2D radius (`footpaths::RerouteFootpathsAroundObstacle` from `src/ECS/MapCells.cpp`); `FootpathObstaclesTest.ARockPutOnTheMapBendsTheFootpathRoundIt` in `test/test_footpath_obstacles.cpp`; see ../terrain/footpaths.md |
| Challenge scripts make rocks | done | CHL CREATE of a mobile static makes a rock (`src/CHLApi.cpp`); test `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind` |
| A rock a challenge script makes as a rock goes into the physics at once and settles on the land where it is made (Land 1's throwing stones) | done | `StartPhysics` in `src/CHLApi.cpp` for a row of the rock kind (`script_create::MakesARock`); test `ParticleCarriedObjectsTest.AScriptsNewMobileStaticIsARockByItsRowsKind`; see [../../../scripts/map-script-reader.md](../../../scripts/map-script-reader.md#the-class-each-mobile-type-makes) |
| A rock a challenge script makes as a mobile static keeps the script's height and falls from there (Land 4's meteors, the cannon's rocks) | done | `script_create::PutIntoPhysicsWhereItIs` from `src/CHLApi.cpp`; test `ParticleCarriedObjectsTest.AScriptsNewRockMadeAsAMobileStaticGoesIntoThePhysicsWhereItIs`; see [../../../scripts/map-script-reader.md](../../../scripts/map-script-reader.md#the-class-each-mobile-type-makes) |

## Features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place features: pillars and spikes in each stone, temples, statues, pyramids, the needle, the acropolis, the mine, the ark and its dock and wreck, the whale, craters, tombstones | done | CREATE_FEATURE and CREATE_NEW_FEATURE (`src/LHScriptX/FeatureScriptCommands.cpp`) to `FeatureArchetype` (`src/ECS/Archetypes/FeatureArchetype.cpp`); tests `LandScriptMeshCommandsTest.CreateFeatureMakesTheNumberedFeatureTurnedAndSizedInThousandths`, `LandScriptMeshCommandsTest.CreateNewFeatureMakesTheNamedFeatureTurnedAndSizedInThousandths` (`test/test_land_script_mesh_commands.cpp`) |
| Features with a physics shape are solid | partial 50% | A feature is in the way of what flies as a building is, while more than a tenth built and its life above a hundredth (`DynamicsSystem::InteractsWithPhysicsObjects`, `src/ECS/Systems/Implementations/DynamicsSystem.cpp`), as our wiki counts multi-map fixed objects as obstacles ([physics.md](../../../physics/physics.md#manager)); test `ParticleCarriedObjectsTest.AFeatureIsInTheWayOfWhatFliesWhileItStands`; its body weighs its own row's weight times its scale cubed, a rock pillar's 50000, so a thrown rock stops against it (`DynamicsSystem::ObjectWeight`, [physics.md](../../../physics/physics.md#bodies)); test `ParticleCarriedObjectsTest.AFeatureWeighsItsScaleCubedTimesItsRowsWeight`; test `ParticleCarriedObjectsTest.AFeatureIsInTheWayOfWhatFliesWhileItStands`. Fails in real play (hand-tested 2026-10-29, item 3): the target pillar in Throwing Stones lets the thrown rocks through |
| Planned features are drawn as far built as the land script's BUILT_PERCENTAGE says | done | `src/ECS/FeatureBuild.cpp` (BUILT_PERCENTAGE: the Land 1 ark's dry dock drawn partly built): the value kept, 0 below 0 or not a number, 1 from 1 up, as the original's SetPercentBuilt; tests `BuildingSitesTest.AFeaturesBuiltPercentageIsKeptBetweenNoneAndAll` and, for the partly built mesh, `DamageMeshPartialBuild.*` |
| Features catch fire and burn | done | Fire receivers by their info (combustion 2000, the ark's three at 250, `src/ECS/Fire/FireObjectTraits.cpp`); `FiresOnTheMap.AFeatureBurnsByItsOwnRow` and `FireTraitRows.realInfoDatFeatures` in `test/test_fire.cpp` |
| Features cast shadows | done | The static shadows of fixed objects, baked into each land block's texture at its prints step (`src/3D/BlockShadows.cpp`, `src/3D/BlockBake.cpp`); scene `light.shadows`; tests `BlockShadows.VerticesFallAlongTheSunOntoTheGround`, `BlockShadows.TrianglesFillTheMaskAsTheOriginal`, `BlockBake.TheShadowsDarkenWhatThePrintsLeft` |
| Mushrooms and toadstools the creature can eat (magic mushrooms change it) | pending | see [../resources/poison_and_mushrooms.md](../resources/poison_and_mushrooms.md) and ../creature/ |

## Animated scenery

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place animated scenery: gates and land transitions | done | Scenes `objects.creature_gate_open`, `objects.creature_gate_close`; `AnimatedStaticArchetype` places them and they play their info's clip (`src/ECS/Archetypes/AnimatedStaticArchetype.cpp`); tests `AnimatedStaticRules.OpenItPlaysOnToItsRestAndClosedBackToTheStart`, `AnimatedStaticSystemTest.ADrawnOpenGatePlaysOnToItsRestAndAnUndrawnOneKeepsItsPlace` (the clip) |
| Gates open when enough gate stones are laid | done | The plinth takes the stones and the plinth and gate open when the land's script says (scenes `objects.gate_stone_by_hand`, `objects.creature_gate_open`); the script's own rule runs in the quest run of Choose Your Creature played to the choice (refs/story/evidence/l1gold.log, 2026-10-30): the plinth reads 7 once the three stones are in (line 438323), then the glade's scene opens the plinth and the gate (lines 448072, 450865); see ../story/ |
| Gate totems (ape, cow, tiger, blank) stand by the creature gates | inferred | Placed as scenery; whether the original does more with them is not checked |

## Loose objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fences, lanterns, barrels and other loose objects placed by the land scripts | done | CREATE_MOBILEOBJECT, CREATE_MOBILESTATIC and CREATE_MOBILE_STATIC (`src/LHScriptX/FeatureScriptCommands.cpp`) to `MobileObjectArchetype`, `MobileStaticArchetype`; tests `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo`, `LandScriptObjectCommandsTest.CreateMobileStaticWithAnglesIsRaisedAndTurnedButMakesNoLantern`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` (`test/test_land_script_town_commands.cpp`) |
| Loose objects can be picked up, thrown and broken | partial 60% | Picked up and thrown into the physics (`src/ECS/Physics/FromHand.cpp`); a thrown fence goes into a wood store; missing: breaking them is not ported; see ../physics/ |
| Some loose objects crush new buildings or block them | partial 50% | A scaffold set down clears the things in its way (`DestroyThingsInWay` in `src/ECS/Scaffolds.cpp`); missing: which objects block a building is not traced |
