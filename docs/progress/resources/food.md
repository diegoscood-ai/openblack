# Food

What the villagers live on. Food comes from fields, fish farms, livestock and the food miracle, is kept in each town's
store, and is eaten by villagers, worshippers and the creature. The player can give food by hand.

**Progress: 13/20 done, 5 partial — 78%**

How the original does it, in our wiki: [Objects and resources](../../bw1-notes/objects-and-resources.md).

## Where food comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Fields' crops grow over the turns, faster on good land and by the weather, until ripe | done | `fields::GrowthStep` in `src/ECS/Fields.cpp` (the land's alignment and the rain); test `test/test_fields.cpp` |
| Farmers sow fields, several times before the crop grows | done | `FindBestField`, `FARMER_PLANTS_CROP` (`src/ECS/Villager/VillagerFarmer.cpp`); test `test/test_villager_farming.cpp`; see `../villager/` |
| Farmers harvest a ripe field and carry the crop to the store | done | `FARMER_DIGS_UP_CROP` and the harvest carried home (`src/ECS/Villager/VillagerFarmer.cpp`) |
| A field's food runs out as it is harvested, and it is sown again | done | the field's food and activity, sow then harvest (`fields::GetFieldActivity`, `src/ECS/Fields.cpp`) |
| The water miracle and rain speed a field's growth | done | rain in `fields::GrowthStep`, the water miracle through `ApplyWaterSpellToField` (`src/Magic/Spells/SpellWater.cpp`) |
| Fishermen fish at the town's fish farms and bring fish to the store | done | `src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/FishFarms.cpp`; see [fish](../animal/fish.md) |
| Shepherds take livestock to be slaughtered for food | todo | the shepherd's states are not ported; see [livestock](../animal/livestock.md) |

## Eating

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Villagers get hungry and fetch food from the store to eat | done | `ChangeStateToFindFoodToEat` (`src/ECS/Villager/VillagerFood.cpp`); test `test/test_villager_food.cpp`; see `../villager/` |
| Villagers with no food starve and die | done | starving in `src/ECS/Villager/VillagerFood.cpp` (death reason Starving) |
| Worshippers eat at the worship site | todo | `CheckAllowedToRestAtWorshipSite` never lets them (`src/ECS/Systems/Implementations/VillagerWorship.cpp`); see `../worship/worship_sites.md` |
| The creature eats from the store, the fields and the fish farms, taking the town's food | partial | it only eats things with a food value (`CreatureObjectActionSystem::FoodValueOf`); see `../creature/` |
| Food from a powered-up food miracle makes those who eat it faster for a while | partial | the pile sparkles and the speed rule exists (`src/ECS/VillagerSpeed.cpp`), but eating never sets it |
| Poisoned food poisons those who eat it; the heal miracle cures them | done | poisoned food harms the villagers who eat it (`src/ECS/Villager/VillagerFood.cpp`, `src/ECS/Components/Poisoned.h`) and the heal miracle cures it (`src/Magic/Core/SpellEvent.cpp`); see [poison and mushrooms](poison_and_mushrooms.md); Land 2's poisoned village store: [the_plague.md](../story/silver_scrolls/the_plague.md) |
| How much food each thing is worth (fish, grain, a handful from a field, animals) | partial | the info tables' amounts are used where the jobs and fields are ported (`src/ECS/Fields.cpp`, `VillagerFisherman.cpp`, `CreatureObjectActionSystem::FoodValueOf`); not audited as a whole |

## A town's food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land script gives each store its starting food | done | `AbodeArchetype` (CREATE_ABODE's food amount) |
| The store's food shows as one pile that rises with what it holds | done | see [stores and piles](stores_and_piles.md) |
| A town short of food raises its food desire flag | done | the town's food desire reads its stores (`src/ECS/Town/TownDesire.cpp`); see `../town/` |
| Giving food to a town that wants it impresses the town | done | `DoResourceAdding` (`src/ECS/ObjectResources.cpp`) adds belief through `town_stores::AddToBelief` by how much the desire drops |
| Food poured on a store teaches the creature to feed the town | partial | the deed is worked out (`StoragePitStore::DoCreatureMimicAfterAddingResource`, `src/ECS/StoragePitStore.cpp`) but not passed on to the creature |
| Scripts read, add and take a town's food | partial | ADD_RESOURCE works (`src/CHLApi.cpp`); GET_RESOURCE and REMOVE_RESOURCE are stubs |

The food miracle is in `../miracles/food.md`.
