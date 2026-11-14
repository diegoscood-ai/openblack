# Food miracle

The horn of plenty: held at the side of the hand and poured with the right button, it showers grain that lands as food
piles or goes straight into a storage pit. Its extreme version makes speed-up food. Grain on villagers' tables is in
[../resources/](../resources/).

**Progress: 30/32 done, 0 chosen, 2 partial, 0 inferred — 99%**

How the original does it, in our wiki: [Miracles one by one](../../../magic/food-wood-water-heal.md).

## Casting and paying

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The horn is held at the side of the hand and pours while the right button is held | done | The seed's in-hand cast (`src/ECS/Systems/Implementations/HandSpellSeed.cpp`, `src/Magic/Hand/HandCasting.cpp`): one apply per turn while held, released on letting go. Scenario `miracles.hand_held_miracles` |
| The horn holds the miracle's cost to create (7000 prayer, 10000 extreme) and is never refilled; the pour ends when it is spent | done | A seed without a worship icon lives off its first charge and nothing refills it (`src/Magic/Core/Chants.cpp`), as the original ([page](../../../magic/worship.md#structure-and-data)); tests `SpellChants.lightningPlayerLivesOnItsInitialChants` (the same rule), `FoodWood.realInfoDat` |
| Each grain pays 7 per unit: the first brings 200 food, each later one 18 (20 extreme) | done | `src/Magic/Spells/SpellResource.cpp` (ResourceEvent); test `FoodWood.resourceEventCosts`, with the real rows `FoodWood.realInfoDat` |
| A grain becomes food only on dry land and while the miracle has strength; over water it is paid for and lost | done | `src/Magic/Spells/SpellResource.cpp` (paid first, the strength read after paying; off the map, on water or with no strength nothing lands; a closed miracle neither pays nor drops); test `test/test_food_miracle.cpp` `FoodMiracleSpell.AGrainIsPaidForWhereverItLandsButIsFoodOnlyOnDryLandWhileTheMiracleHasStrength` |
| The amount is multiplied by the tribal power | done | `src/Magic/Spells/SpellResource.cpp` (amount times the tribal power, truncated); the tribal power rises with the player's wonders ([prayer_cost.md](prayer_cost.md)); test `test/test_food_miracle.cpp` `FoodMiracleSpell.TheFoodAGrainBringsIsItsAmountTimesTheTribalPowerCutDownToAWholeUnit` |
| Letting go keeps the rest in the horn; pressing again needs enough for a first grain | done | `src/Magic/Spells/SpellResource.cpp` (HasEnoughChantsForResourceRecast); test `FoodWood.resourceEventCosts` |

## The hand while pouring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hand rises and tips the horn over 4 s along a natural spline through its key points, peaking at 1.61 of the key (about 16 m and 1.73 rad), then starts over | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (a natural spline through four key points, height 10 m and tilt 1.07 rad at 1, looping over 4 s), as the original ([page](../../../magic/food-wood-water-heal.md#food-and-wood)); tests `FoodWood.grainSplineKeyPoints`, `FoodWood.grainRaiseLoopsOverTotalTime` |
| The tip is a roll about the line to the camera, applied at once | done | `src/ECS/Systems/Implementations/HandPlacement.cpp` (the grain's tilt is the roll of the hand's sway, `magic::hand_hold::HeldUp`: minus the tilt about the hand to camera axis); tests `test/test_food_miracle.cpp` `FoodMiracle.TheTipRollsTheHandBackAboutTheLineToTheCameraByThePoursTilt`, `HandHoldPose.TheHandRollsBackAboutTheLineToTheCamera`; scenario `miracles.hand_held_miracles` |
| The hand is pinned where the pour began | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (ClampedPosition, `magic::PinnedHand`), used by `HandPlacement.cpp`; test `test/test_food_miracle.cpp` `FoodMiracle.TheHandIsPinnedWhereThePourBeganForTheWholePour` |
| Stopping eases the hand back over one game turn | done | `src/ECS/Systems/Implementations/HandGrain.cpp` (height and tilt blended from the last turn's by the turn fraction, `magic::PourPoseAt`); test `test/test_food_miracle.cpp` `FoodMiracle.StoppingEasesTheHandBackToRestOverOneGameTurn` |
| The pour stops whenever the local player's miracle ends | done | `src/Magic/Core/Spell.cpp` (the first close-down of a spell this computer cast stops the hand's raise); test `test/test_food_miracle.cpp` `FoodMiracleSpell.ThePourStopsWhenAMiracleThisComputerCastClosesDown` |

## Grain in the air

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Grain streams from the horn, more as the hand moves, falls, lands and fades | done | `src/Particles/Rules/Sprinkle.cpp` (18 grains a second even from a still hand, more as it moves; each landing grain is a spell event); test `FoodWood.sprinkleEmitsAtTheRateEvenWhenStill` |
| The pouring sound loops and fades out softly when let go | done | The food's file keeps its sound on while the miracle is enabled (`StartStopSoundOnCondition`, `src/Particles/Rules/Sound.cpp`) and lets it go softly once its miracle is let go (`src/Audio/Services/SpellSounds.cpp`); the spells bank plays it as a sample that loops for ever; tests `PhysicalShieldMiracleEffect.APouringSoundStopsWhenTheEffectIsNoLongerEnabledAndRingsOut` and, with the game's data, `SpellSounds.realDataThePouringSoundsLoopUntilLetGoSoftly` |

## Landing and piles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A landing grain looks outwards in a spiral of nine cells for something to take it | done | `src/ECS/PotResource.cpp` (AddResourceToPos: nine cells along the spiral, fixed list then mobile list); test `test/test_food_miracle.cpp` `FoodMiracleDrop.AGrainLooksForATakerInItsOwnCellThenTheEightAroundItAndNoFurther`; scenario `miracles.food_wood` |
| A storage pit within 1.2 times its radius takes it into its own food pile | done | `src/ECS/PotResource.cpp`, `src/ECS/StoragePitStore.cpp`, as the original ([page](../../../magic/food-wood-water-heal.md#food-and-wood)); scenario `miracles.food_speedup_and_stores`; tests `TakeResourceTest.APutDownReachesAStoragePitByTheObjectsMultiplierAndAPileByThePotsOne` (a storage pit's multiplier is 1.2, a pile's 2), `FoodWood.aPutDownOfAnotherTypeOrOutOfReachIsLeftForAPile` (the reach: 1.2 times the 2D radius, in and out), `FoodWood.aPutDownGoesIntoEveryStoreOfItsType`, `TakeResourceTest.APoisonedPutDownPoisonsThePit` (the pit takes it into its food pile) |
| A worship site's food pot within twice its radius takes it | done | The site's food pot (`src/Worship/WorshipSite.cpp`, a store's food pile) is taken like any food pot (`src/ECS/PotResource.cpp`), as the original ([page](../../../magic/food-wood-water-heal.md#food-and-wood)); worship sites only exist after Land 1; test `test/test_food_miracle.cpp` `FoodMiracleDrop.AWorshipSitesFoodPotTakesFoodWithinTwiceItsRadius` |
| Existing food piles in reach take it in turn, never beyond full | done | `src/ECS/PotResource.cpp` (the first pile that qualifies, not the nearest; clipped at its most only for non-magic piles, as the original's rule); test `test/test_food_miracle.cpp` `FoodMiracleDrop.PilesInReachTakeItInTurnAndOnlyAMagicPileTakesMoreThanItsMost`; scenario `miracles.food_wood` |
| Where nothing takes it and the land is dry, a new food pile appears, with no puff | done | `src/ECS/PotResource.cpp`, `src/Magic/Objects/MagicFood.cpp`. Scenario `miracles.food_wood` |
| A new pile rises out of the ground over one second and is drawn only once above it | done | `src/ECS/Archetypes/PotArchetype.cpp` (the pile's sink, a one-second zoomer from buried to its proportion raised); test `test/test_food_miracle.cpp` `FoodMiraclePile.ANewPileRisesOutOfTheGroundOverOneSecond` |
| A food pile grows quickly at first and never past full size | done | `src/ECS/ObjectMetrics.cpp` (PileFoodProportionRaised); test `FoodWood.pileHelpers` |
| The grain texture flows while the pile is still sinking | done | `src/ECS/Archetypes/PotArchetype.cpp` (the grain's UV scroll while the pile sinks, 0.25 (1 - clamp(offset / height + 1)), only the store's and the magic food piles; that the original's call is a texture offset is inferred, [objects](../../../world/objects.md)); test `test/test_food_miracle.cpp` `FoodMiraclePile.TheGrainTextureFlowsWhileTheFoodPileStillSinks` |
| Piles sit on the land and follow it | partial 80% | Set on the land when made and morphed with it (`src/ECS/Archetypes/PotArchetype.cpp`); not re-checked in the game |
| A thud plays as food lands: small ones under 200 food, big ones above, heard only within their range | done | `src/ECS/PotResource.cpp` (PileSoundSample, PlayPileSound); test `FoodWood.pileHelpers` |
| A magic food pile receives no projected shadow; a magic wood pile does | done | (added) `src/ECS/Systems/Implementations/RenderingSystem.cpp` (ReceivesDynamicShadow, over `ecs::shadow_receiver::Receives`), `src/Graphics/ShadowList.cpp`; see `src/Magic/Objects/MagicFood.cpp`; tests `test/test_food_miracle.cpp` `FoodMiracle.AMagicFoodPileReceivesNoProjectedShadowButAMagicWoodPileDoes`, `ShadowReceiver.TheFoodInTheHandAndTheMagicFoodDoNotReceive` |
| The local player hears the advisor remark on the drop | done | `src/ECS/PotResource.cpp` calls `audio::guidance::PlayResourceDropRemark` (`src/Audio/Services/Guidance.cpp`, the town's needs from `src/ECS/AudioQueries.cpp`), for a new pile, only when the dropper is the local player's interface (`dropper.isMyInterface`); tests `test/audio/test_guidance.cpp` `GuidanceTest.ResourceDropSamples` (the remark by the town's needs), `GuidanceTest.FoodDroppedOnATownThatWantsItIsPleased` (a town within 100 m, heard within 200 m), `ResourceStoreDropper.AGiverIsItsPlayersInterfaceAndMineOnlyForTheLocalPlayer` (only the local player's drop is its interface) |

## Extreme food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The extreme horn starts with more prayer and drops 20 per grain | done | FOOD_PU1's row (`src/Magic/MagicTables.cpp`); test `FoodWood.realInfoDat` |
| A new pile from extreme food is speed-up food and sparkles for as long as it lasts | done | `src/Magic/Spells/SpellResource.cpp` (speed-up, not poisoned), `src/ECS/PotResource.cpp` (SetSpeedUp: the speed-up sparkles for good) Scenario `miracles.food_speedup_and_stores` |
| Villagers who eat speed-up food work four times as fast for 2550 turns | partial 75% | 3 of 4 parts: taking food from a sped-up pile starts the villager's speed-up at 255 (`villager::GetResourceFrom`; test `VillagerResourcesTest.TakingFromASpedUpPileSpeedsTheVillagerUp`); it goes down by one every tenth turn, so it lasts 2550 turns (`villager::ProcessFoodSpeedup`, `src/ECS/Villager/VillagerCore.cpp`; test `VillagerResourcesTest.AVillagersFoodSpeedUpLastsTwoThousandFiveHundredAndFiftyTurns`); while it lasts the villager's pace is multiplied by its row's food power-up increase (`src/ECS/VillagerSpeed.cpp`; test `VillagerSpeed.WoundedLoadedNeededAndSpedUp`); a reaction to a speed-up pile is weighted up (`src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`). Not checked: that info.dat's villager rows give four as that increase |

## What it does to people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers nearby react to a new food pile, impressed by how much their town wants food | done | The grain's look-at-nice-miracle reaction is handled (`k_LookAtNiceSpell`, `src/ECS/Systems/Implementations/VillagerReactions.cpp`) and weighted by the town's want of food (`MiracleImpressiveValue`); tests in `test/test_villager_reactions.cpp`. The miracle's new pile also starts its own food reaction as it is made, as the original's pile maker does (`pot_resource::AddResourceToPos`, `src/ECS/PotResource.cpp`; test `TakeResourceTest.ANewPileThatAPutDownMakesStartsItsRowsReactionAsItIsMade`, [wiki](../../../magic/food-wood-water-heal.md#food-and-wood)) |
| Villagers take food from the piles | done | Villagers reacting to a food pile take food from it (`src/ECS/Systems/Implementations/VillagerResourceReactions.cpp`), and a miracle's pile starts that reaction as it is made (`pot_resource::AddResourceToPos`; test `TakeResourceTest.ANewPileThatAPutDownMakesStartsItsRowsReactionAsItIsMade`, [wiki](../../../magic/food-wood-water-heal.md#food-and-wood)) |
| The creature can learn from the player feeding a storage pit, a worship site or a building site | done | The storage-pit deed is passed on (`StoragePitTakesPutDownResource` to `object_resources::ReportResourceDeed`, `src/ECS/PotResource.cpp`, `src/ECS/ObjectResources.cpp`), and so are a worship site's and a building site's: the food a cast puts down there gives the put-down's deed (`pot_resource::GiveTo`, test `CreatureMimicSiteTest.AResourceGivenByHandShowsTheDeedOfWhatTookIt`), and the grain that reaches it tells the world the player's miracle reached it (`spell_event::SpellEvent` to `MagicWorld::PlayerAffected`, test `FoodMiracleSpell.AGrainOrLogThatReachesAThingTellsThePlayersMiracleReachedIt`), whose deed is the miracle's for what it reached (`magic::MiracleDeed`, test `CreatureMimicSiteTest.FoodAndWoodShowWhatTheyReached`) |
| Food piles are saved with the game | done | written: a magic food pile by its class's writer (`src/ECS/SavedWorld/WritePots.cpp`; test `PotWrite.MagicPilesAndAPlainPotAreOfTheirOwnClasses`), and the game's list of food piles names every one made in play (test `PotWrite.TheGamesListsOfPotsAreTheNewOnesNewestFirstThenTheKeptOnes`); a load makes a magic food pile again, and the in-play round trip of 2026-11-14 (refs/story/evidence/save_rt_1114/README.md) wrote 3 magic food piles into its quick save, which the load made again (no "not made" line for them, refs/story/evidence/save_rt_1114/rtip1B.log lines 268-269) and played on 240 turns, each back at its place on the turn after the save (refs/story/evidence/save_rt_1114/rtip1A_entities_turn17986.txt lines 1293-1295, refs/story/evidence/save_rt_1114/rtip1B_entities_turn17986.txt lines 1029-1031) |
