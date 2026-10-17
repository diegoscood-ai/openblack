# Land 3 script

`Scripts/Land3.txt` builds the third land, where the player's creature is taken from them: the rival god's two towns, their temple and worship sites, the creature's prison pillars, the weeping stones and a wood dispenser. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 66/70 done, 2 partial — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`) |
| The story moves on to this land when Land 2 is finished | todo | the land-loading function does nothing (`LoadMap` in `src/CHLApi.cpp` is empty) |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land3.lnd` | done | `Game::LoadLandscape` |
| Is land number 3 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`) |
| Starts the camera looking at "3237.21,3157.85" | done | `FeatureScriptCommands::StartCameraPos` |
| Scales the towns' influence by 0.5 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Sets 3 land balance numbers (4 = 1.25, 5 = 1.21951, 6 = 2.03252) | partial | `land_balance::Set` keeps them all; only numbers 4 (villagers' speed) and 5 (trees) are read |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`) |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (celtic 2, tibetan 1, egyptian 1, indian 1, japanese 1); owners: neutral 3, player two 2, player one 1 | done | `TownArchetype` (`CREATE_TOWN`) |
| Sets 6 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`) |
| Caps belief 4 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`) |
| Leaves 1 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp` |
| Gives 2 towns a gathering place | done | the town's congregation position (`SET_TOWN_CONGREGATION_POS`), read by `src/ECS/Town/TownQueries.cpp` |
| Gives towns 23 miracles to offer at their village centres (fire 3, lightning bolt 3, heal 2, food 2, storm 2, water 2, teleport 2, shield 2, physical shield 2, fire pu1 1, nature 1, water pu1 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style F) (Indian 5, Egyptian 3, Japanese 2, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style A) (Egyptian 4, Indian 2, Japanese 2, Tibetan 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 10 houses (style D) (Egyptian 5, Tibetan 2, Japanese 2, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style C) (Tibetan 3, Egyptian 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 storehouses (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style B) (Japanese 3, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 crèches (Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style E) (Tibetan 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 village centres (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`) |
| 70 planned buildings for towns to build later (houses (style F) 21, houses (style B) 11, houses (style C) 11, houses (style A) 9, houses (style D) 9, houses (style E) 4, crèches 2, village centres 1, workshops 1, wonders 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites |
| 1 temples of rival gods (player two 1) | done | `CitadelArchetype` (`CREATE_CITADEL`) |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`) |
| 2 worship sites (egyptian for player two 1, tibetan for player two 1) | done | `magic::script::CreateWorshipSite` (`CREATE_WORSHIP_SITE`): a built site at the player's temple |
| 1 miracle dispensers (wood 1) | done | `magic::script::CreateSpellDispenser` (`CREATE_SPELL_DISPENSER`) |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 68 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 46 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 10 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 9 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 7 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 2 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 villagers placed in towns without a house (housewives 1) | done | `CreateVillagerInTown` (`CREATE_TOWN_VILLAGER`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1397 trees and bushes (PalmB 454, Palm 279, BushB 132, Conifer 76, PalmA 71, PalmC 69, Beech 54, Cedar 52, Bush 39, HedgeA 39, Pine 38, BushA 33, Olive 26, Hedge 15, ConiferA 8, Copse 6, Cypress 5, Birch 1) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 1 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in |
| 16 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 20 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 6 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 1" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 2" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Prison Pillar 3" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Tibetan Large Pillar Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 70 patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); eleven common miracles about 9% each, flocks under 1%, no creature miracles; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 18 fields (Wheat 14, WheatWithFence 4) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 7 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`) |
| 2 pots (MagicWood 2) | done | `PotArchetype` (`CREATE_POT`) |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 27 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 9 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 8 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 8 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 6 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 6 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 5 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 15 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 106 × rocks and boulders (Rock 83, Boulder1Sand 7, Boulder3Lime 3, RockLimestone 3, Boulder2Lime 3, SharprockLimestone 2, Boulder1Lime 2, FlatrockLimestone 2, LongrockLimestone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 87 × fences (CeltFenceShort 49, CeltFenceTall 38) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 8 × WeepingStoneReward | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 13 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 7 × Champi (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 6 × Toadstool (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 1 lanterns (StreetLantern 1) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 streams through 18 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`) |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial | the players are added to the player system; their thinking isn't (see ../multiplayer/) |
