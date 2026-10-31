# Land 1 script

`Scripts/Land1.txt` builds the first land of the story: the player's starting town, the neutral villages around it, the creature's gate with its three totems, the Pied Piper's cave and the magic mushrooms, scattered rocks and fences, and its climates and streams. This file counts what the script makes and whether openblack makes it; the challenges played there are the story domain's ([../story/](../story/)), and the scripts that run them are in [challenge_scripts.md](challenge_scripts.md).

**Progress: 64/66 done, 0 chosen, 2 partial, 0 inferred — 99%**

How the original does it, in our wiki: [The Land 1 intro and the tutorial's script side](../../../scripts/land1-intro.md).

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped; tests `LandScriptContents.realLand1Script`, `LandScriptContents.TheReaderTakesEachLineAndCountsWhatItsCommandsMake` |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`); test `LandScriptContents.realLand1Script` |
| Loading the land starts the story's control script, which runs Land 1's challenges | partial 70% | `Game.cpp` starts `LandControlAll` at a new game on Land 1 with the program read, as the original; of Land 1's challenge scripts many now call only working functions, see challenge_scripts.md |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land1.lnd` | done | `Game::LoadLandscape`; test `LandScriptContents.realLand1Script` |
| Is land number 1 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "1441.56,2081.76" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraLand1` |
| Scales the towns' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (norse 2, celtic 2, japanese 1, aztec 1); owners: neutral 5, player one 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `LandScriptContents.realLand1Script`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 6 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Leaves 4 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp`; tests `LandScriptContents.realLand1Script`, `LandScriptTownCommandsTest.AnUninhabitableTownTakesNoVillager` |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style A) (Norse 4, Celtic 3, Aztec 3, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 7 houses (style C) (Aztec 4, Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 houses (style E) (Norse 2, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 houses (style F) (Norse 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 houses (style D) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 houses (style B) (Aztec 2, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 graveyards (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 storehouses (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 crèches (Norse 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 village centres (Norse 1, Aztec 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 6 planned buildings for towns to build later (houses (style A) 3, wonders 2, houses (style D) 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites; tests `LandScriptContents.realLand1Script`, `LandScriptTownCommandsTest.CreatePlannedAbodePutsThePlanOnTheTownsList` |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`); test `CitadelPlanTest.PlanIsOnlyAPlan` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 26 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 26 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 2 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 shepherd living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1395 trees and bushes (Bush 378, ConiferA 206, PalmB 194, Conifer 116, Pine 113, Palm 108, Birch 77, Beech 73, Oak 38, BushB 36, PalmA 16, OakA 11, Olive 10, PalmC 10, BushA 6, Burnt 3) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 3 dead trees | done | `DeadTreeArchetype` (`CREATE_DEAD_TREE`) with its life and three angles; test `LandScriptSave.ALandScriptDeadTreeIsMadeForThePlayerItNames` |
| 3 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `LandScriptContents.realLand1Script` |
| 9 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand1Script` |
| 3 × feature "Fat Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand1Script` |
| 2 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand1Script` |
| 2 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand1Script` |
| 2 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand1Script` |
| 17 patches of mist | done | `MistArchetype` (`CREATE_MIST`); tests `LandScriptContents.realLand1Script`, `SavedWorldLoad.TheFrozenLandOneAutosavesRiversAndMistsAreTheirScripts` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); heal 77%, fireball, lightning, forest, food, wood and water about 4% each; see [../nature/fireflies.md](../nature/fireflies.md); test `WorshipTest.FireFlyRewardProbabilities` |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 19 fields (Wheat 18, Corn 1) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `LandScriptContents.realLand1Script` |
| 13 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 4 pots (MagicWood 4) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |
| 47 drinking places | done | a `DrinkWaypoint` is kept in the order made (`CREATE_DRINK_WAYPOINT`); the creature drinks at the nearest one under 1000 (`mind_detail::DrinkPlaceOf`, `creature_drink::NearestWaterPoint`; test `CreatureDrink.*`; unverified against v1.42) |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 40 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 22 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 14 × swallow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 12 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 10 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 7 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 6 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 5 × bat | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `LandScriptContents.realLand1Script`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 132 × rocks and boulders (Rock 70, Boulder3Lime 20, Boulder1Lime 19, LongrockLimestone 6, RockLimestone 5, SharprockLimestone 5, Boulder2Lime 5, Boulder1Sand 1, Boulder3Chalk 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 102 × fences (CeltFenceTall 64, CeltFenceShort 38) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 4 × toys (ToyDie 3, ToyCuddly 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 3 × gate totems (GateTotemBlank 1, GateTotemTiger 1, GateTotemApe 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 36 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 9 × Toadstool (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 4 × EgyptBarrel (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 2 × Champi (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 2 × Piper Cave Entrance (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`); test `LandScriptContents.realLand1Script` |
| 1 × Norse Gate (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`); test `LandScriptContents.realLand1Script` |
| 1 × Gate Stone Plinth (animated) | done | `AnimatedStaticArchetype` (`CREATE_ANIMATED_STATIC`); test `LandScriptContents.realLand1Script` |
| 12 lanterns (StreetLantern 8, CountryLantern 4) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `LandScriptContents.realLand1Script`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |
| 2 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original; test `FireTraits.ABonfireHasAnEndlessSpotVisualAndNoFire` |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 streams through 187 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`); tests `LandScriptContents.realLand1Script`, `WaterQueries.drinking_water`, `SavedWorldLoad.TheFrozenLandOneAutosavesRiversAndMistsAreTheirScripts` |
| 3 arenas | partial 40% | an `Arena` is kept (`CREATE_ARENA`); openblack's creature fights (`CreatureFightSystem`) make an arena of their own and never take the land's, and the land's arena light is not drawn while a fight is on ([fights.md](../../../creature/fights.md#pending)) |
