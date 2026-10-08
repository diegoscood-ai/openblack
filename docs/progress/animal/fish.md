# Fish and fish farms

The fish in the sea: fish farms, the patches of sea a town's fishermen work, and the fish they bring home. Boats and the
sea itself are in `../ocean/`.

**Progress: 9/13 done, 1 partial — 73%**

How the original does it, in our wiki: [Animals: AI, states and clips](../../bw1-notes/animals.md), [Skeletal animation (villagers and animals)](../../bw1-notes/animation.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land scripts place fish farms for towns | done | CREATE_FISH_FARM and CREATE_TOWN_FISH_FARM (`src/LHScriptX/FeatureScriptCommands.cpp`, `src/ECS/Archetypes/FishFarmArchetype.cpp`) |
| A fish farm belongs to its town and its player | done | the farm's town and its owner (`fish_farms::TownOf`, `PlayerOf`, `src/ECS/FishFarms.cpp`) |
| A fish farm's food grows again every so many turns, up to full | done | every 16th turn each farm gets 1 food back, up to 1400 (`ecs::ProcessFishFarmsTurn`, `src/ECS/FishShoals.cpp`) |
| Fishermen go to a farm, up to its number of fishermen, and fish | done | the fishermen's list and spots, and the fishing states (`src/ECS/FishFarms.cpp`, `src/ECS/Villager/VillagerFisherman.cpp`); see `../villager/` |
| A fisherman brings his catch to the store as food | done | the catch carried to the drop-off (`src/ECS/Villager/VillagerFisherman.cpp`, `src/ECS/Villager/VillagerResources.cpp`); see `../resources/food.md` |
| A town's desire to fish follows how full its farms are | partial | a farm's score follows its fishermen, not its fish (`fish_farms::Score`, `src/ECS/FishFarms.cpp`); see `../town/` |
| Fish farms show where fish are (the shoal and its nets) | done | the shoal of each farm (`src/ECS/FishShoals.cpp`); the nets are the fish puzzle's (`src/ECS/FishPuzzle.cpp`) |
| Fish rush and splash about the sea (unconfirmed) | done | the shoals flee a splash: the hand in the sea or an object falling in (`ecs::SplashWater`, `src/ECS/FishShoals.cpp`) |
| The creature fishes, eating fish or bringing them to the town | todo | see `../creature/` |
| The hand can take fish (a fish in the hand) and give it as food | done | the hand scoops fish from a farm with fish and gives them as food (`src/ECS/Systems/Implementations/HandFish.cpp`, `src/ECS/HeldApply.cpp`); see ../hand/multi_pickup.md |
| Fish can be put by the worship site to feed the worshippers | done | a handful of food given to the worship site goes into its store (`worship::site::DeleteObjectAndTakeResource`, `src/Worship/WorshipSite.cpp`); see `../worship/worship_sites.md` |
| Stoning a fish farm or burning it damages it (unconfirmed) | todo |  |
| A fish farm is drawn and repaired as the town's (unconfirmed) | todo |  |
