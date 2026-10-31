# Land 4 script

`Scripts/Land4.txt` builds the fourth land, the first land again under a rival god's curse: its towns, the Aztec temple feature, the volcanic rocks and the Norse gate. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 55/58 done, 0 chosen, 1 partial, 0 inferred — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped; tests `LandScriptContents.realLand4Script`, `LandScriptContents.TheReaderTakesEachLineAndCountsWhatItsCommandsMake` |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`); test `LandScriptContents.realLand4Script` |
| The story moves on to this land when Land 3 is finished | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but play has not yet got here: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Starting on this land runs this land's challenges | pending | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land4.lnd` | done | `Game::LoadLandscape`; test `LandScriptContents.realLand4Script` |
| Is land number 4 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "1810.04,2611.21" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraLand1` |
| Scales the towns' influence by 0.8 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Creates 5 climates and sets rain, temperature and wind 5, 5 and 5 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 8 towns (norse 3, aztec 2, japanese 1, celtic 1, indian 1); owners: neutral 7, player one 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `LandScriptContents.realLand4Script`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 9 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Leaves 3 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp`; tests `LandScriptContents.realLand4Script`, `LandScriptTownCommandsTest.AnUninhabitableTownTakesNoVillager` |
| Gives towns 11 miracles to offer at their village centres (heal pu1 1, food 1, shield 1, wood 1, water pu1 1, fire 1, lightning bolt pu2 1, creature spell angry 1, teleport 1, nature 1, creature spell weak 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/; test `LandScriptContents.realLand4Script` |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 7 houses (style C) (Aztec 6, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 6 houses (style A) (Norse 2, Aztec 2, Celtic 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 5 houses (style B) (Aztec 3, Japanese 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 houses (style D) (Aztec 2, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style F) (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style E) (Celtic 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 crèches (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 graveyards (Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 village centres (Aztec 1, Japanese 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 25 planned buildings for towns to build later (houses (style A) 7, houses (style C) 5, houses (style F) 3, houses (style D) 3, workshops 2, houses (style B) 2, houses (style E) 1, village centres 1, graveyards 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites; tests `LandScriptContents.realLand4Script`, `LandScriptTownCommandsTest.CreatePlannedAbodePutsThePlanOnTheTownsList` |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`); test `CitadelPlanTest.PlanIsOnlyAPlan` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 24 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 16 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 7 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 fisherman living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 shepherd living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 town leader living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 799 trees and bushes (Bush 315, Conifer 87, OakA 75, ConiferA 70, Oak 60, Birch 59, Olive 29, Beech 26, BushB 18, Hedge 15, Pine 8, BushA 7, Palm 7, PalmB 7, PalmA 6, PalmC 6, Cedar 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `LandScriptContents.realLand4Script`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 3 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 9 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `LandScriptContents.realLand4Script` |
| 8 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand4Script` |
| 2 × feature "Spikey Pilar Volcanic" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand4Script` |
| 1 × feature "Aztec Suntemple Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand4Script` |
| 14 patches of mist | done | `MistArchetype` (`CREATE_MIST`); test `LandScriptContents.realLand4Script` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); eleven common miracles and stronger heal and food about 7% each, rarer stronger ones and nine creature miracles; see [../nature/fireflies.md](../nature/fireflies.md); test `WorshipTest.FireFlyRewardProbabilities` |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 fields (Wheat 12, Cereal 3, Corn 1) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `LandScriptContents.realLand4Script` |
| 6 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 8 pots (MagicWood 8) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 29 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 26 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 26 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 20 × bat | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 7 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 5 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 5 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 15 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `LandScriptContents.realLand4Script`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 118 × rocks and boulders (Boulder3Lime 17, LongrockVolcanic 12, Boulder2Sand 11, LongrockLimestone 10, Boulder2Lime 10, Boulder1Lime 8, Boulder1Sand 7, SharprockLimestone 5, Boulder1Volcanic 5, LongrockSandstone 4, FlatrockSandstone 4, Boulder3Volcanic 4, Boulder3Sand 3, RockLimestone 3, SquarerockLimestone 3, RockSandstone 3, SharprockSandstone 2, FlatrockLimestone 2, RockVolcanic 2, SharprockVolcanic 1, Boulder2Volcanic 1, SquarerockSandstone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand4Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 89 × fences (CeltFenceShort 62, CeltFenceTall 27) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand4Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 1 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand4Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`); test `LandScriptContents.realLand4Script` |
| 16 lanterns (StreetLantern 8, CountryLantern 8) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `LandScriptContents.realLand4Script`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |
| 2 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original; test `FireTraits.ABonfireHasAnEndlessSpotVisualAndNoFire` |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 23 streams through 388 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`); test `LandScriptContents.realLand4Script` |
| 1 arenas | partial 40% | an `Arena` is kept (`CREATE_ARENA`); openblack's creature fights (`CreatureFightSystem`) make an arena of their own and never take the land's, and the land's arena light is not drawn while a fight is on ([fights.md](../../../creature/fights.md#pending)) |
