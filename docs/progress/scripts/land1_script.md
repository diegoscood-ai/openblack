# Land 1 script

`Scripts/Land1.txt` builds the first land of the story: the player's starting town, the neutral villages around it, the creature's gate with its three totems, the Pied Piper's cave and the magic mushrooms, scattered rocks and fences, and its climates and streams. This file counts what the script makes and whether openblack makes it; the challenges played there are the story domain's ([../story/](../story/)), and the scripts that run them are in [challenge_scripts.md](challenge_scripts.md).

**Progress: 63/67 done, 4 partial — 97%**

How the original does it, in our wiki: [The Land 1 intro and the tutorial's script side](../../bw1-notes/intro.md).

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`) |
| Loading the land starts the story's control script, which runs Land 1's challenges | partial | `Game.cpp` starts `LandControlAll` at a new game whatever land was loaded; of Land 1's challenge scripts many now call only working functions, see challenge_scripts.md |
| The land is where a new game starts (from the map script) | partial | openblack starts on `Land1.txt` by default (`--start-level`) without reading the map script |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land1.lnd` | done | `Game::LoadLandscape` |
| Is land number 1 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`) |
| Starts the camera looking at "1441.56,2081.76" | done | `FeatureScriptCommands::StartCameraPos` |
| Scales the towns' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (norse 2, celtic 2, japanese 1, aztec 1); owners: neutral 5, player one 1 | done | `TownArchetype` (`CREATE_TOWN`) |
| Sets 6 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`) |
| Leaves 4 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp` |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style A) (Norse 4, Celtic 3, Aztec 3, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 7 houses (style C) (Aztec 4, Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style E) (Norse 2, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style F) (Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style D) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 houses (style B) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 graveyards (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 storehouses (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 crèches (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 2 village centres (Norse 1, Aztec 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`) |
| 6 planned buildings for towns to build later (houses (style A) 3, wonders 2, houses (style D) 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`) |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 26 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 26 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 2 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 1 shepherd living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1395 trees and bushes (Bush 378, ConiferA 206, PalmB 194, Conifer 116, Pine 113, Palm 108, Birch 77, Beech 73, Oak 38, BushB 36, PalmA 16, OakA 11, Olive 10, PalmC 10, BushA 6, Burnt 3) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 3 dead trees | done | `DeadTreeArchetype` (`CREATE_DEAD_TREE`) with its life and three angles |
| 3 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 9 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Fat Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 2 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 17 patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); heal 77%, fireball, lightning, forest, food, wood and water about 4% each; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 19 fields (Wheat 18, Corn 1) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 13 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`) |
| 4 pots (MagicWood 4) | done | `PotArchetype` (`CREATE_POT`) |
| 47 drinking places | partial | a `DrinkWaypoint` is kept (`CREATE_DRINK_WAYPOINT`); nothing reads it yet |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 40 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 22 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 14 × swallow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 12 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 10 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 7 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 6 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 5 × bat | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 16 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 132 × rocks and boulders (Rock 70, Boulder3Lime 20, Boulder1Lime 19, LongrockLimestone 6, RockLimestone 5, SharprockLimestone 5, Boulder2Lime 5, Boulder1Sand 1, Boulder3Chalk 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 102 × fences (CeltFenceTall 64, CeltFenceShort 38) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 4 × toys (ToyDie 3, ToyCuddly 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 3 × gate totems (GateTotemBlank 1, GateTotemTiger 1, GateTotemApe 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 36 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 9 × Toadstool (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 4 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 2 × Champi (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 2 × Piper Cave Entrance (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`) |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`) |
| 1 × Gate Stone Plinth (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`) |
| 12 lanterns (StreetLantern 8, CountryLantern 4) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |
| 2 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 streams through 187 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`) |
| 3 arenas | partial | an `Arena` is kept (`CREATE_ARENA`); the creature fights that use it are not wired into the game |
