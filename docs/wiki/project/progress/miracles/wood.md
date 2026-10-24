# Wood miracle

Held at the side of the hand like the horn, the wood miracle pours logs for four seconds; they land as wood piles or
go into a storage pit, a workshop or a building site. There is no extreme version. Wood used for building is in
[../resources/](../resources/).

**Progress: 10/23 done, 0 chosen, 12 partial, 0 inferred — 81%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/food-wood-water-heal.md).

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The wood is held at the side of the hand and pours while the right button is held | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Hand/HandCasting.cpp`) Scenario `miracles.hand_held_miracles` |
| A pour lasts 4 s (the player's timer) | done | The player's timer from the WOOD row (`src/Magic/MagicTables.cpp`), aged in `src/Magic/Core/Spell.cpp`. Test `FoodWood.realInfoDat` (with the game's data) |
| It holds a finite pool that is never refilled | done | A seed without a worship icon lives off its first charge (`src/Magic/Core/Chants.cpp`) As the original ([page](../../../magic/worship.md#structure-and-data)); test `SpellChants.lightningPlayerLivesOnItsInitialChants` (the same rule) |
| Each drop pays 3 per unit: the first brings 500 wood, each later one 20 | done | `src/Magic/Spells/SpellResource.cpp` (ResourceEvent); tests `FoodWood.resourceEventCosts`, `FoodWood.realInfoDat` |
| Wood lands only on dry land; the amount is multiplied by the tribal power | done | `src/Magic/Spells/SpellResource.cpp` (dry land only; amount times the tribal power, truncated); the tribal power rises with the player's wonders ([prayer_cost.md](prayer_cost.md)); tests `test/test_wood_miracle.cpp` `WoodMiracleSpell.ALogIsPaidForWhereverItLandsButIsWoodOnlyOnDryLand`, `WoodMiracleSpell.TheWoodALogBringsIsItsAmountTimesTheTribalPower` |
| Pressing again needs enough prayer for a first drop | done | `src/Magic/Spells/SpellResource.cpp` (HasEnoughChantsForResourceRecast) Test `FoodWood.resourceEventCosts` |

## The hand and the logs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips over 4 s exactly as for food, rolling about the line to the camera | done | `src/ECS/Systems/Implementations/HandGrain.cpp`, `HandPlacement.cpp` (the same raise as food); test `FoodWood.grainRaiseLoopsOverTotalTime` |
| Stopping eases the hand back over one game turn | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (blended from the last turn's by the turn fraction); test `test/test_wood_miracle.cpp` `WoodMiracle.StoppingEasesTheHandBackToRestOverOneGameTurn` |
| Logs fall from the hand tumbling about the axis across their movement | done | `src/Particles/Rules/Sprinkle.cpp` (the tumble appearance rule: `sprinkle::TumbleAngle`, `sprinkle::TumblesAboutZ`, [page](../../../magic/food-wood-water-heal.md#food-and-wood)), logs drawn by `src/Particles/Creators/Mesh.cpp`; test `test/test_food_wood.cpp` `FoodWood.aLogTumblesByItsSpeedAboutTheAxisAcrossItsMovement`; scenario `miracles.hand_held_miracles` |
| The pouring sound loops and fades out softly when let go | partial 80% | The particle sound rules (`src/Particles/Rules/Sound.cpp`, `src/Audio/Services/SpellSounds.cpp`); no test covers the loop's fade |

## Where the wood goes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing log looks outwards in a spiral of nine cells for something to take it | done | `src/ECS/PotResource.cpp` (AddResourceToPos); test `test/test_wood_miracle.cpp` `WoodMiracleDrop.ALogLooksForATakerInItsOwnCellThenTheEightAroundItAndNoFurther`; scenario `miracles.food_wood` |
| A storage pit takes it into its wood piles in order | done | `src/ECS/PotResource.cpp`, `src/ECS/StoragePitStore.cpp`. Scenario `miracles.food_speedup_and_stores` |
| A workshop, building site or scaffold takes it | partial 50% | Building sites and workshops keep their wood in a wood pile (`src/ECS/Town/BuildingSites.cpp`), which takes a log like any pile; they are not stores of their own in `src/ECS/PotResource.cpp`, and a pit under construction does not pass its wood on. Whether the original counts a workshop or a building site as a wood store of its own is not read; issue #59 (the wood of a store under construction) |
| Existing wood piles take it, never beyond full | done | `src/ECS/PotResource.cpp` (the first pile that qualifies); tests `test/test_wood_miracle.cpp` `WoodMiracleDrop.WoodPilesInReachTakeItInTurnAndOnlyAMagicPileTakesMoreThanItsMost`, `WoodMiracleDrop.AWoodPileTakesWoodWithinTwiceItsRadius`; scenario `miracles.food_wood` |
| Where nothing takes it a new wood pile appears and rises out of the ground over a second | done | `src/Magic/Objects/MagicWood.cpp`, `src/ECS/Archetypes/PotArchetype.cpp` (the one-second rise) Scenario `miracles.food_wood` |
| A wood pile grows evenly with what it holds, never past full size | done | `src/ECS/ObjectMetrics.cpp` (the wood pile's proportion raised) Test `FoodWood.pileHelpers` |
| A wood pile never rises above the ground and follows the land | partial 80% | Set on the land when made (`src/ECS/Archetypes/PotArchetype.cpp`); not re-checked in the game |
| Thuds play as wood lands, small under 200 and big above, heard only within their range | done | `src/ECS/PotResource.cpp` (PileSoundSample); test `FoodWood.pileHelpers` |
| The local player hears the advisor remark on the drop | done | `src/ECS/PotResource.cpp` calls `audio::guidance::PlayResourceDropRemark` (`src/Audio/Services/Guidance.cpp`), for a new pile; test `test/audio/test_guidance.cpp` `GuidanceTest.ResourceDropSamples` |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new wood pile, impressed by how much their town wants wood | partial 70% | The log's look-at-nice-miracle reaction is handled (`k_LookAtNiceSpell`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`) and weighted by the town's want of wood (`MiracleImpressiveValue`); tests in `test/test_villager_reactions.cpp`. Whether a miracle's new pile itself starts a wood reaction in the original is not read (`src/ECS/PotResource.cpp` starts none) |
| Villagers fetch the wood for building | partial 50% | Villagers fetch wood for building sites (`src/ECS/Systems/Implementations/VillagerBuildingSites.cpp`) and pick up wood they react to (`VillagerResourceReactions.cpp`), but a miracle's pile starts no wood reaction |
| The creature can learn from the player putting wood in a storage pit or by a building site | partial 60% | The storage-pit deed is passed on (`StoragePitTakesPutDownResource` to `object_resources::ReportResourceDeed`, `src/ECS/PotResource.cpp`, `src/ECS/ObjectResources.cpp`); no building-site deed for a cast; no test covers the miracle's case |
| Wood piles are saved with the game | pending | No game save system |
