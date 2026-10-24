# Forest

A seed miracle: thrown onto the land it plants eighteen magic trees in a spiral, which grow while the caster pays for
them and wither away when the miracle ends. Sparkles, a light on the land and a flock of butterflies (or bats, for an
evil god) follow, and the caster's camera takes a short flight round the new forest. There is no power-up.

Given by gold scroll: [The Workshop](../story/gold_scrolls/the_workshop.md).

**Progress: 20/41 done, 0 chosen, 19 partial, 0 inferred — 85%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/forest.md).

## Casting and cost

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cast with the forest gesture from its seed | done | Cast from its seed on press (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`); the seed comes from a worship icon or a one-shot orb. See `../gesture/` and `dispensers_and_seeds.md`. Scenarios `miracles.forest_grass`, `miracles.forest_withers` |
| Only on land within influence, in a cell no building covers | partial 80% | `src/Magic/Spells/SpellForest.cpp` (CanCastAt: in the map, on land, no abode of the cell covers it, the cell's newest fixed object is no multi-cell building), with the cast rule's influence test (`src/Magic/CastRules.cpp`) No test covers it |
| Not right beside an abode (measured from the abode's middle and its size) | done | `src/Magic/Spells/SpellForest.cpp` (NoAbodeCovers: an abode's 2D radius against the distance to its middle; fields never block) Scenario `miracles.forest_beside_hut`; no unit test |
| It can't be cast at an object | done | `src/Magic/CastRules.cpp` (CanCastOn: no for the forest, whatever the object and its place, as the original's check on an object); test `test/test_forest_miracle.cpp` `ForestMiracleCastRules.ItIsNeverCastAtAnObject` |
| Cost 13000 to cast, then 5 a turn plus 1 for each tree | done | `src/Magic/Spells/SpellForest.cpp` (CalculateCostToMaintain: cost per turn plus one per tree); test `SpellForest.costToMaintain` |
| No power-up exists | done | The NATURE row has no power-up (`src/Magic/MagicTables.cpp`) Test `SpellForest.realInfoDat` (with the game's data) |
| It can be recast only while it has trees left to make | done | `src/Magic/Spells/SpellForest.cpp` (HasEnoughChantsAndLifeForRecast: trees still to make); test `SpellForest.maxObjectsToCreate` |

## Planting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The seed falls, spinning, and plants every tree at once when it lands | partial 80% | The falling seed atom's land collision event (`src/Particles/ParticleEffect.cpp`) makes `src/Magic/Spells/SpellForest.cpp` (SpellEvent) plant the whole forest at once. Scenario `miracles.forest_grass`. The seed's spin while falling is inferred from the code, not seen ([wiki Pending](../../../magic/forest.md#pending)) |
| Eighteen trees on a spiral from 2 to 11 m round the cast point | done | `src/Magic/Spells/SpellForest.cpp` (SpiralOffset); tests `SpellForest.spiral`, `SpellForest.mapCoords` |
| A spot a building covers is skipped, never retried; other trees don't block | done | `src/Magic/Spells/SpellForest.cpp` (ValidPlaceForTree: a spot is tested once; a tree is no multi-cell building) Test `ForestSystemTest.plantOncePerSpell`; scenario `miracles.forest_beside_hut` |
| The kind of tree comes from the ground at the centre: broadleaves, palms on sand or by water, conifers on snow | done | `src/Magic/Spells/SpellForest.cpp` (RandomTreeType, TerrainMaterial: the cast point's material, snow, and no block counts as deep water, so palms); test `SpellForest.realInfoDat`. Scenarios `miracles.forest_grass`, `miracles.forest_sand`, `miracles.forest_snow` |
| Each tree faces a random way; trees further out grow smaller (down to half size) | done | `src/Magic/Spells/SpellForest.cpp` (CreateTree: a random angle, target scale 1 - 0.5 x distance / 11); test `SpellForest.targetScale` |
| Trees grow a little each turn up to their size | done | `src/Magic/Spells/SpellForest.cpp` (Process grows every tree 0.01 a turn while it can afford them) Test `SpellForest.treesWanted`; scenario `miracles.forest_grass` |
| They also grow as other trees do, faster with rain and good land | done | `src/ECS/Trees.cpp` (TreeGrowthAmount with the rain and the land's alignment, also for the miracle's trees); test `SpellForest.treeGrowthAmount` |
| The forest belongs to the caster | partial 80% | Each magic tree keeps the caster's player (`src/Magic/Objects/MagicTree.cpp`); the forest itself keeps none, as the original never reads it. That the original never reads the forest's own player is inferred ([wiki Pending](../../../magic/forest.md#pending)) |
| A forest cast from a worship icon leaves its seed on the ground at the centre, rising with the tallest tree; the hand can pick it up and cast the trees left elsewhere | done | (added) `src/Magic/Core/SpellSeed.cpp` (DrawSpells, FollowsSpell, ValidForPlaceInHand), `src/Magic/Spells/SpellForest.cpp` (AdjustSpellSeedPos); tests `SpellForest.seedFollowsSpell`, `SpellForest.adjustSpellSeedPos` |

## Upkeep and withering

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the caster can't afford all the trees, every tree withers each turn and goes at nothing | done | `src/Magic/Spells/SpellForest.cpp` (Process: fewer wanted than there are, every tree shrinks 0.05 a turn, `ShrinkAllTrees` in `src/ECS/Trees.cpp` takes a tree at nothing or less, the last tree takes the forest and the next turn deletes the spell); tests `test/test_forest_miracle.cpp` `ForestMiracleTrees.UnpaidEveryTreeWithersEachTurnUntilTheForestAndThenTheSpellGo`, `ForestMiracleTrees.AllTheTreesWitherByTheDecaySpeedEachTurnAndGoAtNothing` |
| When the miracle ends every tree withers away within about two seconds; magic trees never stay | done | `src/Magic/Spells/SpellForest.cpp` (CloseDown drops the creator, so no tree is wanted and all shrink 0.05 a turn) Scenario `miracles.forest_withers` |
| A player's forest has no time limit and lasts while it is paid for; a one-shot forest lasts two minutes | done | The timers from the NATURE row (`src/Magic/MagicTables.cpp`, 120 and none), aged in `src/Magic/Core/Spell.cpp`, as the original ([page](../../../magic/forest.md#forest)); test `SpellForest.realInfoDat` |

## Effects on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle when it lands | done | The look-at-nice-miracle reaction is started (`src/Magic/Core/SpellEvent.cpp`) and handled by the villagers (`k_LookAtNiceSpell`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`); tests in `test/test_villager_reactions.cpp`; scenario `miracles.reactions_nice` |
| Villagers stop to look at the magic trees | pending | Each magic tree starts its reaction (`src/Magic/Objects/MagicTree.cpp`), but villagers have no handler for the magic tree's reaction (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Creatures take no notice of the magic trees' reaction | partial 80% | The creature's reaction handler has no case for the magic tree's reaction (`src/ECS/Systems/Implementations/CreatureReactions.cpp`); no test covers it. Not yet established: the original has a creature setup for this reaction, which for a tree returns before the creature follows it ([creature reactions](../../../creature/mimicry.md)), and whether its start-up reaction table hands the reaction to creatures (with the record a creature then keeps of it) was not read |
| A burning magic tree stops drawing looks, and draws them again once put out | done | `src/Magic/Objects/MagicTree.cpp` (the fire start removes the tree's reaction, the fire end makes it again for the tree's player, not stamped with the turn; an ordinary tree makes none); test `test/test_forest_miracle.cpp` `ForestMiracleTrees.FireTakesAMagicTreesLookAwayAndPuttingItOutMakesItAgain` |
| A magic tree gives a quarter of an ordinary tree's wood, scaled by tribal power | done | `src/Magic/Spells/SpellForest.cpp` (`spell_forest::TreeWoodMultiplier`: the row's 0.25 x the tribal power, [page](../../../magic/forest.md)), `src/Magic/Objects/MagicTree.cpp`, read by `src/ECS/Trees.cpp` (wood value, kept by the felled tree); the tribal power rises with the player's wonders ([prayer_cost.md](prayer_cost.md)); tests `SpellForest.realInfoDat` (the quarter), `SpellForest.treeWoodMultiplier`, `test/test_forest_miracle.cpp` `ForestMiracleTrees.AMagicTreesWoodIsItsMultiplierTimesAnOrdinaryTreesWood`, `test/test_take_resource.cpp` `TakeResourceTest.AForestMiraclesTreeGivesWoodByItsCastersTribalPower` (the felled wood) |
| Shields don't stop the planting; it does no damage | partial 80% | The forest event pays and plants with no shield test; its effect values do no damage; no test or check covers it |

## The caster's camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the caster's camera flies the forest's camera path, placed at the forest | done | The forest's camera particle (`src/Particles/Rules/Forest.cpp`) takes the caster's camera through the camera path system; tests `ForestCamera.AnotherPlayersEffectNeverAsksForTheCamera`, `ForestCamera.SFForestsCameraTakesTheCameraForItsAnimationsLengthAfterFourSeconds` (`test/test_particle_forest_camera.cpp`) |
| It waits, then glides onto the path and follows it slowed down | done | `src/Particles/Rules/Forest.cpp`; tests `ForestCamera.ItAsksForTheCameraAtItsFirstStepAtTheParticleThenFollowsAndPlaysAfterThePause`, `ForestCameraRules.ItPlaysFromALaterStepOnceOlderThanThePauseNotAtIt` |
| The flight lasts one play of its animation and ends with the miracle | done | `src/Particles/Rules/Forest.cpp`; tests `ForestCamera.TheLastDrawnFrameLetsTheCameraGoOnceAndTheRemovalDoesNotAgain`, `ForestCamera.TheParticleRemovedBeforeItsLastFrameLetsTheCameraGo` |
| A movement key (only on a frame where it would move the camera) or gripping the land gives the camera back | done | The camera path system gives the camera back on a key that moves it 400 units a second of the camera's frame time (whole milliseconds, at most a tenth of a second) by a whole unit, or on a land grip (`src/Camera/CameraPathControl.h`); scenario `miracles.forest_camera_taken_back`; tests `CameraPathControl.AMovementKeyTakesTheCameraBackOnlyWhenItWouldMoveIt`, `CameraPathControl.GrippingTheLandAlwaysTakesTheCameraBack` (`test/test_near_clipping.cpp`), `CameraPathSystemTest.AMovementKeyThatMovesTheCameraOrALandGripTakesItBack` (`test/camera/test_camera_path_system.cpp`) |
| The hand stays shown during the flight | partial 60% | The flight is ported (rows above); whether the hand stays shown is not tested or checked |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A trail of rising player-coloured blobs follows the falling seed | partial 80% | Particle data of the forest effect; scenario `miracles.forest_grass`; not compared with the original |
| An animated light lies on the land for the life of the effect | done | `src/Particles/Creators/LightMap.cpp` (the forest's light map: one atom from 3.5 s, the nine frames of Forest_LMap.raw at 15 a second, looped; no rule of its group takes it away); tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.AnAnimatedLightLiesOnTheLandFromThreeAndAHalfSecondsForTheEffectsLife`, `ForestMiracleEffects.realDataSFForest` |
| At 3.5 s sparkles burst and keep rising, with spinning vortices | done | Particle data, hung from the camera particle (SF_Forest: at 3.5 s the sparkle root, a burst of ten and up to six vortices turning 8.22566 rad a second; every sparkle leaves within 1.002 rad of straight up); scenario `miracles.forest_grass`; tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.AtThreeAndAHalfSecondsTheSparklesBurstWithTheirSoundAndRiseBesideSpinningVortices`, `ForestMiracleEffects.realDataSFForest` |
| At 5.5 s five flocks of ten circle the forest on a squashed sphere | done | `src/Particles/Rules/Forest.cpp` (the forest path and good or evil creator), `src/Particles/Rules/Flock.cpp` (flocking); test `SpellForest.butterflyFlap`. Scenarios `miracles.forest_grass`, `miracles.forest_bats` |
| Bats for an evil caster, butterflies otherwise | done | `src/Particles/Rules/Forest.cpp` (good or evil creator: butterflies or bats, flapping clips from `src/Particles/Creators/Mesh.cpp`) Scenario `miracles.forest_bats` |
| The flocks fade in over 4 s and out at 12 to 14 s | done | Particle data (SF_Forest: each butterfly from 0 to 255 over its first 4 s and back to 0 from 12 to 14 s; the flocks go at 16 s); tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.TheFlocksFadeInOverFourSecondsAndOutFromTwelveToFourteen`, `ForestMiracleEffects.realDataSFForest` |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound with the seed, cut when it lands | done | SOUND_SPELL_FOREST_1 with the seed (`src/Particles/ParticleEffect.cpp`, the create rule's sound); the seed meets the land on the 41st turn and the next sound turn stops its sound, which has no soft release (`src/Audio/Services/SpellSounds.cpp`); tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.TheSeedsSoundStartsWithItAndIsCutWhenItLandsOnTheFortyFirstTurn`, `ForestMiracleEffects.realDataSFForest` |
| A sound with the sparkles at 3.5 s | done | Particle data (SF_Forest: SOUND_SPELL_FOREST_4 with the sparkle root made at 3.5 s, played once by `src/Particles/ParticleEffect.cpp`); tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.AtThreeAndAHalfSecondsTheSparklesBurstWithTheirSoundAndRiseBesideSpinningVortices`, `ForestMiracleEffects.realDataSFForest` |
| A sound with the flocks at 5.5 s | done | Particle data (SF_Forest: SOUND_SPELL_FOREST_3 with the flocks made at 5.5 s, played once, with the first flock only, as the original's sphere rule plays it; `src/Particles/ParticleEffect.cpp`); tests `test/test_forest_miracle.cpp` `ForestMiracleEffects.AtFiveAndAHalfSecondsTheFiveFlocksComeWithOneSound`, `ForestMiracleEffects.realDataSFForest` |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast a forest | partial 60% | The creature's plan casts a forest by a villager (scenario `miracles.creature_casts_forest`); not yet checked against the original (issue #172) |
| The forest and its trees are kept in a saved game | pending | No game save system |
