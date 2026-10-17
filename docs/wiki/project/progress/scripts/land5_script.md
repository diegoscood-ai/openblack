# Land 5 script

`Scripts/Land5.txt` builds the last land: the rival god's four towns with their worship sites and temple, the player's two towns, a fire dispenser, two water one-shot miracles, craters and the Pied Piper's caves. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 67/70 done, 1 partial — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`) |
| The story moves on to this land when Land 4 is finished | todo | the land-loading function does nothing (`LoadMap` in `src/CHLApi.cpp` is empty) |
| Starting on this land runs this land's challenges | todo | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land5.lnd` | done | `Game::LoadLandscape` |
| Is land number 5 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`) |
| Starts the camera looking at "1430.39,3421.05" | done | `FeatureScriptCommands::StartCameraPos` |
| Scales the towns' influence by 0.6 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/ |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 8 towns (norse 2, tibetan 2, aztec 1, greek 1, celtic 1, japanese 1); owners: player two 4, player one 2, neutral 2 | done | `TownArchetype` (`CREATE_TOWN`) |
| Sets 8 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`) |
| Leaves 1 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp` |
| Gives 1 towns a gathering place | done | the town's congregation position (`SET_TOWN_CONGREGATION_POS`), read by `src/ECS/Town/TownQueries.cpp` |
| Gives towns 51 miracles to offer at their village centres (lightning bolt 5, heal 4, heal pu1 4, lightning bolt pu2 4, fire 3, teleport 3, storm 3, beam explosion 3, water 2, water pu1 2, storm pu2 2, creature spell strong 2, fire pu2 2, nature 2, physical shield 2, beam explosion pu2 1, food 1, beam explosion pu1 1, shield 1, wood 1, ground flock 1, creature spell invisible 1, creature spell compassion 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/ |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 houses (style B) (Tibetan 12, Aztec 2, Greek 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 15 houses (style C) (Tibetan 5, Greek 4, Japanese 4, Aztec 2) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 11 houses (style A) (Greek 4, Aztec 2, Tibetan 2, Japanese 2, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 8 houses (style D) (Aztec 3, Greek 2, Japanese 2, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 6 houses (style F) (Tibetan 4, Aztec 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 storehouses (Tibetan 2, Aztec 1, Greek 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 4 houses (style E) (Japanese 2, Aztec 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 crèches (Aztec 1, Tibetan 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 3 workshops (Tibetan 2, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 wonders (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 1 graveyards (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`) |
| 5 village centres (Tibetan 2, Aztec 1, Greek 1, Japanese 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`) |
| 68 planned buildings for towns to build later (houses (style A) 13, houses (style D) 11, houses (style B) 8, houses (style C) 8, houses (style E) 6, wonders 6, houses (style F) 6, graveyards 4, village centres 2, crèches 2, totems 1, storehouses 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites |
| 1 temples of rival gods (player two 1) | done | `CitadelArchetype` (`CREATE_CITADEL`) |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`) |
| 4 worship sites (japanese for player two 1, greek for player two 1, tibetan for player two 1, aztec for player two 1) | done | `magic::script::CreateWorshipSite` (`CREATE_WORSHIP_SITE`): a built site at the player's temple |
| 1 miracle dispensers (fire pu2 1) | done | `magic::script::CreateSpellDispenser` (`CREATE_SPELL_DISPENSER`) |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 76 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 20 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 19 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 15 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 8 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |
| 7 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`) |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 804 trees and bushes (Bush 222, Oak 137, OakA 111, ConiferA 39, Olive 34, PalmB 34, Palm 34, Conifer 32, BushB 25, PalmA 23, Pine 22, Beech 20, Birch 19, PalmC 16, Cypress 12, CypressA 11, BushA 7, Cedar 6) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 5 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in |
| 8 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 5 × feature "Crater" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 10 patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); the widest table, with flocks about 3%, the tornado and the explosion miracle; see [../nature/fireflies.md](../nature/fireflies.md) |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 47 fields (Wheat 39, Corn 6, Cereal 2) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 13 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`) |
| 3 pots (MagicWood 3) | done | `PotArchetype` (`CREATE_POT`) |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 38 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 34 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 28 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 24 × bat | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 23 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 17 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 11 × swallow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 11 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 10 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 9 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 4 × tiger | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 28 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 119 × rocks and boulders (Boulder1Lime 25, Boulder3Lime 20, Boulder2Lime 20, LongrockLimestone 16, SharprockLimestone 10, FlatrockLimestone 6, Boulder1Chalk 4, SquarerockLimestone 4, FlatrockSandstone 3, Boulder2Sand 3, RockLimestone 2, RockSandstone 2, LongrockSandstone 1, Boulder3Sand 1, LongrockChalk 1, Rock 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 111 × fences (CeltFenceShort 68, CeltFenceTall 43) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 17 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 1 × MagicWood (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 1 × EgyptPotA (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 2 × Piper Cave Entrance (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`) |
| 21 lanterns (CountryLantern 14, StreetLantern 7) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |
| 1 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 19 streams through 422 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`) |

## Miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 one-shot miracles lying on the land (water 2) | done | `magic::script::CreateOneShotSpellPu` (`CREATE_ONE_SHOT_SPELL_PU`) |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial | the players are added to the player system; their thinking isn't (see ../multiplayer/) |
