# Fish and fish farms

The fish in the sea: fish farms, the patches of sea a town's fishermen work, and the fish they bring home. Boats and the
sea itself are in `../ocean/`.

**Progress: 10/12 done, 0 chosen, 1 partial, 0 inferred — 91%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../../beings/animal-life.md), [Skeletal animation (villagers and animals)](../../../beings/animation.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place fish farms for towns | done | CREATE_FISH_FARM and CREATE_TOWN_FISH_FARM (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/FishFarmArchetype.cpp`), the place searched on the unflattened sea; testbed `hand.fish_farm`; [map script reader](../../../scripts/map-script-reader.md#fish-farms) |
| A fish farm belongs to the town nearest it, whatever the script's town, and to that town's player | partial 90% | the nearest town of any tribe (`FishFarmArchetype::Create`), and the farm's town and owner (`fish_farms::TownOf`, `PlayerOf`, `src/ECS/FishFarms.cpp`); no test pins them |
| A fish farm's food grows again every so many turns, up to full | done | every 16th turn each farm gets 1 food back, up to 1400 (`ecs::ProcessFishFarmsTurn`, `src/ECS/FishShoals.cpp`); `FishFarm.AFishComesBackOnEverySixteenthTurnUpToFull` |
| Fishermen go to a farm, up to its number of fishermen, and fish | done | the fishermen's list and spots, and the fishing states (`src/ECS/FishFarms.cpp`, `src/ECS/Villager/VillagerFisherman.cpp`); `FishFarm.AFishermansSpotIsTheFarmMovedByMetres`, `FishFarm.AFishermanCatchesAQuarterOfWhatHeCarriesLessOutOfSeason`; see `../villager/` |
| A fisherman brings his catch to the store as food | done | the catch carried to the drop-off (`src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/Villager/VillagerResources.cpp`); `FishFarm.AFishermanTakesHisFoodToTheStoreOnceHisRoomIsLessThanHisCatch`; see `../resources/food.md` |
| A town sends a fisherman to the nearest farm that has none | done | a farm's score counts its fishermen, not its fish (`fish_farms::Score`, `src/ECS/FishFarms.cpp`); `FishFarmFinder.ATownSendsAFishermanToTheNearestFarmWithoutOne`, `FishFarmScore.OneOnlyWithoutFishermen`; see `../town/` |
| Fish farms show where fish are (the shoal and its nets) | done | the shoal of each farm (`src/ECS/FishShoals.cpp`); the nets are the fish puzzle's (`src/ECS/FishPuzzle.cpp`); `FishShoal.*`, `FishShoalBits.AFishsSwimStepGivesTheOriginalsBits`, `FishPuzzleBits.TheNetsRingGivesTheOriginalsBits` |
| A shoal's fish dart away from a splash: the hand in the sea or something falling in | done | `ecs::SplashWater` (`src/ECS/FishShoals.cpp`); `FishShoal.AScareSendsTheNearFishAwayAndTheShoalElsewhere`, `FishShoalBits.AShoalsScareGivesTheOriginalsTargetAndHeading`; testbed `hand.fish_scare` |
| The creature fishes, eating fish or bringing them to the town | pending | see `../creature/` |
| The hand can take fish (a fish in the hand) and give it as food | done | the hand scoops fish from a farm with fish and gives them as food (`src/ECS/Systems/Implementations/HandFish.cpp`, `src/ECS/HeldApply.cpp`); `FishFarm.TheHandsFirstHandfulGoesByAFullFarmNotTheFishInIt`, `FishFarmStock.TakingFishTakesNoMoreThanItHas`; testbed `hand.fish_scoop`; see ../hand/multi_pickup.md |
| Fish can be put by the worship site to feed the worshippers | done | a handful of food given to the worship site goes into its store (`worship::site::DeleteObjectAndTakeResource`, `src/Worship/WorshipSite.cpp`); `ResourceStoresTest.AWorshipSiteTakesFood`; see `../worship/worship_sites.md` |
| A fire or a miracle damages a fish farm | done | a fire heats a farm by the game's fish farm row and its burn does no harm (burn multiplier 0); a miracle's crush and hit take its life, and the last of it deletes the farm, its fishermen sent away, its town still attacked (`fire::traits::InfoOf`, `src/ECS/Fire/FireObjectTraits.cpp`; `src/ECS/Effects/EffectValues.cpp`); `RepairsTest.AMiracleWearsAFishFarmDownAndItsLastLifeDeletesItStillAttackingItsTown`; [fish farms](../../../world/sea-life-and-scenery.md#fish-farms) |
