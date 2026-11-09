# Food

What the villagers live on. Food comes from fields, fish farms, livestock and the food miracle, is kept in each town's
store, and is eaten by villagers, worshippers and the creature. The player can give food by hand.

**Progress: 12/20 done, 0 chosen, 7 partial, 0 inferred — 87%**

How the original does it, in our wiki: [Objects and resources](../../../world/objects.md).

## Where food comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fields' crops grow over the turns, faster on good land and by the weather, until ripe | done | `fields::GrowthStep` in `src/ECS/Fields.cpp` (the land's alignment and the rain); test `test/test_fields.cpp` |
| Farmers sow fields, several times before the crop grows | done | `FindBestField`, `FARMER_PLANTS_CROP` (`src/ECS/Villager/VillagerFarmer.cpp`); test `test/test_villager_farming.cpp`; see `../villager/` |
| Farmers harvest a ripe field and carry the crop to the store | done | Test `test/test_villager_farming.cpp`; `FARMER_DIGS_UP_CROP` and the harvest carried home (`src/ECS/Villager/VillagerFarmer.cpp`) |
| A field's food runs out as it is harvested, and it is sown again | done | the field's food and activity, sow then harvest (`fields::GetFieldActivity`, `fields::RemoveFood`, `src/ECS/Fields.cpp`; the farmer's sowing and digging up, `src/ECS/Villager/VillagerFarmer.cpp`); tests `FieldsTest.RemoveFoodRipeRunsOutOnEqualCost`, `RemoveFoodUnripeRunsOutKeepsTheCrop`, `VillagerFarmingTest.FarmerPlantsCrop`, `FarmerDigsUpCrop` |
| The water miracle and rain speed a field's growth | done | Test `test/test_fields.cpp` (`GrowthStep` in the rain); scene `miracles.water_fields`; rain in `fields::GrowthStep`, the water miracle through `ApplyWaterSpellToField` (`src/Magic/Spells/SpellWater.cpp`) |
| Fishermen fish at the town's fish farms and bring fish to the store | done | `src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/FishFarms.cpp`; tests `VillagerFarmingTest.FishermanArrives`, `FishingTrip`, `FishFarm.AFishermanCatchesAQuarterOfWhatHeCarriesLessOutOfSeason`, `AFishermanTakesHisFoodToTheStoreOnceHisRoomIsLessThanHisCatch`; the same as [fields and fish farms](../building/fields_and_fish_farms.md); see [fish](../animal/fish.md) |
| Shepherds take livestock to be slaughtered for food | pending | the shepherd's states are not ported; see [livestock](../animal/livestock.md) |

## Eating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry and fetch food from the store to eat | done | `ChangeStateToFindFoodToEat` (`src/ECS/Villager/VillagerFood.cpp`); test `test/test_villager_food.cpp`; see `../villager/` |
| Villagers with no food starve and die | done | Test `VillagerFoodTest.HungerDamage`; starving in `src/ECS/Villager/VillagerFood.cpp` (death reason Starving) |
| Worshippers eat at the worship site | partial 80% | A hungry worshipper is let go to eat when the site's pot holds a meal (`CheckAllowedToRestAtWorshipSite`, `src/ECS/Systems/Implementations/VillagerWorship.cpp`); test `WorshipperTest.AHungryWorshipperEatsOnlyWithAMealInThePot`; missing: the meal taken from the pot is not tested; see `../worship/worship_sites.md` |
| The creature eats from the store, the fields and the fish farms, taking the town's food | partial 80% | From the store and the fields (scenes `mind.eats_from_store`, `mind.eats_from_field`; `CreatureObjectActionSystem::FoodValueOf`); missing: the fish farms are not checked; see `../creature/` |
| Food from a powered-up food miracle makes those who eat it faster for a while | partial 50% | The pile sparkles (`pot_resource::SetSpeedUp`) and the speed rule and its count-down exist (`src/ECS/VillagerSpeed.cpp`, `VillagerCore.cpp`); missing: eating never sets the villager's `foodSpeedUp` |
| Poisoned food poisons those who eat it; the heal miracle cures them | done | Test `GameMagicWorldObjects.TheHealCuresPoisonAndNothingElse`, scene `villagers.poisoned_pause`; poisoned food harms the villagers who eat it (`src/ECS/Villager/VillagerFood.cpp`, `src/ECS/Components/Poisoned.h`) and the heal miracle cures it (`src/Magic/Core/SpellEvent.cpp`); see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned village store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| How much food each thing is worth (fish, grain, a handful from a field, animals) | partial 60% | Tests `StoreRules.OnlyMeatAndVegetableAnimalsAreFood`, `AMushroomIsWorthItsFoodValue`; missing: the amounts not audited as a whole against the exe; the info tables' amounts are used where the jobs and fields are ported (`src/ECS/Fields.cpp`, `VillagerFisherman.cpp`, `CreatureObjectActionSystem::FoodValueOf`); not audited as a whole |

## A town's food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting food | partial 90% | No test; `AbodeArchetype` (CREATE_ABODE's food amount) |
| The store's food shows as one pile that rises with what it holds | done | see [stores and piles](stores_and_piles.md) |
| A town short of food raises its food desire flag | partial 90% | No test named here; the town's food desire reads its stores (`src/ECS/TownDesire.cpp`); see `../town/` |
| Giving food to a town that wants it impresses the town | partial 90% | No test; `DoResourceAdding` (`src/ECS/ObjectResources.cpp`) adds belief through `town_stores::AddToBelief` by how much the desire drops |
| Food poured on a store teaches the creature to feed the town | done | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`, `src/ECS/StoragePitStore.cpp`) and passed to `creature_mimic::Consider` (`ReportResourceDeed`, `src/ECS/ObjectResources.cpp`); tests `TakeResourceTest.FoodOfAnotherPlayerDrawsOnceForTheStealDeed`, `OwnFoodAndAnyWoodDrawNothing` |
| Scripts read, add and take a town's food | done | Tests `TakeResourceTest.*`; GET_RESOURCE, ADD_RESOURCE and REMOVE_RESOURCE work through `object_resources` (`src/CHLApi.cpp`, `src/ECS/ObjectResources.cpp`): every kind answers and gives its own resource, a villager given resources carries them, and a worship site, a building site, a dead tree, a rock, a big forest and a fish farm read and give theirs as the original's do |

The food miracle is in `../miracles/food.md`.
