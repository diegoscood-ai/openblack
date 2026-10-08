# Land 2 script

`Scripts/Land2.txt` builds the second land: the largest, with fifteen towns, the rival god's temple and worship site, a second rival's town, the Greek and Indian villages, many fields and planned buildings. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 65/69 done, 2 partial — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`) |
| The story moves on to this land when Land 1 is finished (its control script loads it) | todo | the land-loading function does nothing (`LoadMap` in `src/CHLApi.cpp` is empty) |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land2.lnd` | done | `Game::LoadLandscape` |
| Is land number 2 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`) |
| Starts the camera looking at "3393.78,3261.76" | done | `FeatureScriptCommands::StartCameraPos` |
| Scales the towns' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Sets 6 land balance numbers (0 = 2, 1 = 2, 2 = 2, 3 = 2, 4 = 1.5, 5 = 2) | partial | `land_balance::Set` keeps them all; only numbers 4 (villagers' speed) and 5 (trees) are read |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`) |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 15 towns (celtic 5, norse 4, indian 4, greek 2); owners: neutral 9, player three 3, player two 2, player one 1 | done | `TownArchetype` (`CREATE_TOWN`); the quests' towns: id 2 is Greek and Khazar's at the start, id 9 Greek, id 12 Indian (the land's control script calls them Norse in its comments; the map script decides) |
| Sets 11 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`) |
| Caps belief 20 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`) |
| Leaves 4 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp` |
| Gives 1 towns a gathering place | done | the town's congregation position (`SET_TOWN_CONGREGATION_POS`), read by `src/ECS/Town/TownQueries.cpp` |
| Changes towns' desires 8 times (abodes 4, civic buildings 4) | done | `ecs::town_desire::MapTownDesireBoost` (`TOWN_DESIRE_BOOST`) |
| Gives towns 22 miracles to offer at their village centres (heal 3, fire 2, food 2, teleport 2, physical shield 2, lightning bolt 2, shield 2, nature 1, wood 1, creature spell itchy 1, water 1, creature spell angry 1, storm 1, creature spell strong 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 25 houses (style A) (Indian 14, Celtic 8, Greek 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 houses (style F) (Indian 6, Celtic 2, Norse 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 storehouses (Indian 4, Celtic 3, Greek 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 9 houses (style C) (Celtic 4, Norse 2, Indian 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 8 houses (style B) (Norse 3, Greek 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style D) (Greek 2, Celtic 2, Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 crèches (Celtic 2, Norse 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style E) (Norse 2, Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 graveyards (Greek 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 workshops (Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 village centres (Indian 4, Celtic 3, Greek 2, Norse 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`) |
| 89 planned buildings for towns to build later (houses (style A) 32, houses (style F) 18, houses (style C) 8, houses (style E) 7, workshops 6, crèches 6, houses (style B) 4, wonders 4, graveyards 2, houses (style D) 2) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites |
| 2 temples of rival gods (player two 1, player three 1) | done | `CitadelArchetype` (`CREATE_CITADEL`) |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`) |
| 2 worship sites (norse for player two 1, celtic for player three 1) | done | `magic::script::CreateWorshipSite` (`CREATE_WORSHIP_SITE`): a built site at the player's temple |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 115 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 77 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 13 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 11 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 6 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 4 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 villagers placed in towns without a house (foresters 1) | done | `CreateVillagerInTown` (`CREATE_TOWN_VILLAGER`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 921 trees and bushes (ConiferA 264, Bush 187, Pine 178, Conifer 104, Oak 43, BushB 43, Beech 29, Birch 21, OakA 18, Palm 10, BushA 6, PalmB 5, PalmC 5, PalmA 4, Olive 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 10 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 7 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Pier" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); eleven common miracles about 8% each, flocks and nine creature miracles under 1%; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 60 fields (Wheat 34, WheatWithFence 22, Corn 4) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 30 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`) |
| 5 pots (MagicWood 5) | done | `PotArchetype` (`CREATE_POT`) |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 111 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 79 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 50 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 47 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 41 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 33 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 19 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 18 × pigeon | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 16 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 3 × tiger | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 3 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 2 × lion | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 45 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 378 × rocks and boulders (RockLimestone 239, Boulder3Lime 32, Boulder2Lime 31, Boulder1Lime 28, SharprockLimestone 15, SquarerockLimestone 11, FlatrockLimestone 8, LongrockLimestone 7, Boulder3Sand 4, Boulder2Sand 2, LongrockSandstone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 265 × fences (CeltFenceTall 181, CeltFenceShort 84) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 39 lanterns (StreetLantern 36, CountryLantern 3) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |
| 1 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 2 computer players (player two, player three) | partial | the players are added to the player system; their thinking isn't (see ../multiplayer/) |
