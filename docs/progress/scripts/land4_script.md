# Land 4 script

`Scripts/Land4.txt` builds the fourth land, the first land again under a rival god's curse: its towns, the Aztec temple feature, the volcanic rocks and the Norse gate. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 55/58 done, 1 partial — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`) |
| The story moves on to this land when Land 3 is finished | todo | the land-loading function does nothing (`LoadMap` in `src/CHLApi.cpp` is empty) |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land4.lnd` | done | `Game::LoadLandscape` |
| Is land number 4 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`) |
| Starts the camera looking at "1810.04,2611.21" | done | `FeatureScriptCommands::StartCameraPos` |
| Scales the towns' influence by 0.8 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Creates 5 climates and sets rain, temperature and wind 5, 5 and 5 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 8 towns (norse 3, aztec 2, japanese 1, celtic 1, indian 1); owners: neutral 7, player one 1 | done | `TownArchetype` (`CREATE_TOWN`) |
| Sets 9 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`) |
| Leaves 3 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp` |
| Gives towns 11 miracles to offer at their village centres (heal pu1 1, food 1, shield 1, wood 1, water pu1 1, fire 1, lightning bolt pu2 1, creature spell angry 1, teleport 1, nature 1, creature spell weak 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 7 houses (style C) (Aztec 6, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style A) (Norse 2, Aztec 2, Celtic 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 houses (style B) (Aztec 3, Japanese 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style D) (Aztec 2, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style F) (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 houses (style E) (Celtic 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 crèches (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 village centres (Aztec 1, Japanese 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`) |
| 25 planned buildings for towns to build later (houses (style A) 7, houses (style C) 5, houses (style F) 3, houses (style D) 3, workshops 2, houses (style B) 2, houses (style E) 1, village centres 1, graveyards 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`) |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 24 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 16 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 7 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 fisherman living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 shepherd living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 town leader living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 799 trees and bushes (Bush 315, Conifer 87, OakA 75, ConiferA 70, Oak 60, Birch 59, Olive 29, Beech 26, BushB 18, Hedge 15, Pine 8, BushA 7, Palm 7, PalmB 7, PalmA 6, PalmC 6, Cedar 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 3 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in |
| 9 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 8 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Spikey Pilar Volcanic" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 1 × feature "Aztec Suntemple Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 14 patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); eleven common miracles and stronger heal and food about 7% each, rarer stronger ones and nine creature miracles; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 fields (Wheat 12, Cereal 3, Corn 1) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 6 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`) |
| 8 pots (MagicWood 8) | done | `PotArchetype` (`CREATE_POT`) |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 29 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 26 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 26 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 20 × bat | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 7 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 5 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 5 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 15 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 118 × rocks and boulders (Boulder3Lime 17, LongrockVolcanic 12, Boulder2Sand 11, LongrockLimestone 10, Boulder2Lime 10, Boulder1Lime 8, Boulder1Sand 7, SharprockLimestone 5, Boulder1Volcanic 5, LongrockSandstone 4, FlatrockSandstone 4, Boulder3Volcanic 4, Boulder3Sand 3, RockLimestone 3, SquarerockLimestone 3, RockSandstone 3, SharprockSandstone 2, FlatrockLimestone 2, RockVolcanic 2, SharprockVolcanic 1, Boulder2Volcanic 1, SquarerockSandstone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 89 × fences (CeltFenceShort 62, CeltFenceTall 27) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 1 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`) |
| 16 lanterns (StreetLantern 8, CountryLantern 8) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |
| 2 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 23 streams through 388 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`) |
| 1 arenas | partial | an `Arena` is kept (`CREATE_ARENA`); the creature fights that use it are not wired into the game |
