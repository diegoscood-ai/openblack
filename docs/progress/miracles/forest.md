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
| It can't be cast at an object | partial 80% | `src/Magic/CastRules.cpp` (CanCastOn: no for the forest) No test covers it |
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
| While the caster can't afford all the trees, every tree withers each turn and goes at nothing | partial 80% | `src/Magic/Spells/SpellForest.cpp` (Process: fewer wanted than there are, every tree shrinks 0.05 a turn) No test covers it |
| When the miracle ends every tree withers away within about two seconds; magic trees never stay | done | `src/Magic/Spells/SpellForest.cpp` (CloseDown drops the creator, so no tree is wanted and all shrink 0.05 a turn) Scenario `miracles.forest_withers` |
| A player's forest has no time limit and lasts while it is paid for; a one-shot forest lasts two minutes | done | The timers from the NATURE row (`src/Magic/MagicTables.cpp`, 120 and none), aged in `src/Magic/Core/Spell.cpp`, as the original ([page](../../../magic/forest.md#forest)); test `SpellForest.realInfoDat` |

## Effects on the world

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers look at the nice miracle when it lands | done | The look-at-nice-miracle reaction is started (`src/Magic/Core/SpellEvent.cpp`) and handled by the villagers (`k_LookAtNiceSpell`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`); tests in `test/test_villager_reactions.cpp`; scenario `miracles.reactions_nice` |
| Villagers stop to look at the magic trees | pending | Each magic tree starts its reaction (`src/Magic/Objects/MagicTree.cpp`), but villagers have no handler for the magic tree's reaction (`src/ECS/Systems/Implementations/VillagerReactions.cpp`) |
| Creatures take no notice of the magic trees' reaction | partial 80% | The creature's reaction handler has no case for the magic tree's reaction (`src/ECS/Systems/Implementations/CreatureReactions.cpp`); no test covers it |
| A burning magic tree stops drawing looks, and draws them again once put out | partial 80% | `src/Magic/Objects/MagicTree.cpp` (the fire start removes the tree's reaction, the fire end makes it again) No test covers it |
| A magic tree gives a quarter of an ordinary tree's wood, scaled by tribal power | partial 80% | `src/Magic/Objects/MagicTree.cpp` (0.25 x tribal power), read by `src/ECS/Trees.cpp` (wood value, kept by the felled tree) Test `SpellForest.realInfoDat` reads the quarter; `PlayerMagic::tribalPower` is never set in our tree (see [prayer_cost.md](prayer_cost.md)); no test of the felled wood |
| Shields don't stop the planting; it does no damage | partial 80% | The forest event pays and plants with no shield test; its effect values do no damage; no test or check covers it |

## The caster's camera

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only the caster's camera flies the forest's camera path, placed at the forest | done | The forest's camera particle (`src/Particles/Rules/Forest.cpp`) takes the caster's camera through the camera path system; tests `ForestCamera.AnotherPlayersEffectNeverAsksForTheCamera`, `ForestCamera.SFForestsCameraTakesTheCameraForItsAnimationsLengthAfterFourSeconds` (`test/test_particle_forest_camera.cpp`) |
| It waits, then glides onto the path and follows it slowed down | done | `src/Particles/Rules/Forest.cpp`; tests `ForestCamera.ItAsksForTheCameraAtItsFirstStepAtTheParticleThenFollowsAndPlaysAfterThePause`, `ForestCameraRules.ItPlaysFromALaterStepOnceOlderThanThePauseNotAtIt` |
| The flight lasts one play of its animation and ends with the miracle | done | `src/Particles/Rules/Forest.cpp`; tests `ForestCamera.TheLastDrawnFrameLetsTheCameraGoOnceAndTheRemovalDoesNotAgain`, `ForestCamera.TheParticleRemovedBeforeItsLastFrameLetsTheCameraGo` |
| A movement key (only on a frame where it would move the camera) or gripping the land gives the camera back | partial 80% | The camera path system gives the camera back on a moving key; scenario `miracles.forest_camera_taken_back`; gripping the land and the frame rule have no test |
| The hand stays shown during the flight | partial 60% | The flight is ported (rows above); whether the hand stays shown is not tested or checked |

## Effects (FX)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A trail of rising player-coloured blobs follows the falling seed | partial 80% | Particle data of the forest effect; scenario `miracles.forest_grass`; not compared with the original |
| An animated light lies on the land for the life of the effect | partial 80% | `src/Particles/Creators/LightMap.cpp` (the forest's light map) No test covers it |
| At 3.5 s sparkles burst and keep rising, with spinning vortices | partial 80% | Particle data, hung from the camera particle; scenario `miracles.forest_grass`; no test |
| At 5.5 s five flocks of ten circle the forest on a squashed sphere | done | `src/Particles/Rules/Forest.cpp` (the forest path and good or evil creator), `src/Particles/Rules/Flock.cpp` (flocking); test `SpellForest.butterflyFlap`. Scenarios `miracles.forest_grass`, `miracles.forest_bats` |
| Bats for an evil caster, butterflies otherwise | done | `src/Particles/Rules/Forest.cpp` (good or evil creator: butterflies or bats, flapping clips from `src/Particles/Creators/Mesh.cpp`) Scenario `miracles.forest_bats` |
| The flocks fade in over 4 s and out at 12 to 14 s | partial 80% | Particle data; no test or check covers the timing |

## Audio

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A sound with the seed, cut when it lands | partial 80% | The particle sound rules (`src/Particles/Rules/Sound.cpp`) No test covers it |
| A sound with the sparkles at 3.5 s | partial 80% | Particle data; no test covers it |
| A sound with the flocks at 5.5 s | partial 80% | Particle data; no test covers it |

## Creatures and saving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A creature can cast a forest | partial 60% | The creature's plan casts a forest by a villager (scenario `miracles.creature_casts_forest`); not yet checked against the original (issue #172) |
| The forest and its trees are kept in a saved game | pending | No game save system |
