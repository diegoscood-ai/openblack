# Tutorial land script

`Scripts/LandT.txt` builds the tutorial land: two of the player's towns, a neutral town, toys (skittles, a bowling ball, balls, dice), weeping stones and a pier. The game loads it by its own path and starts the tutorial's control script by name rather than through the story's control script (both names sit in the game; how it is chosen from the menu is unconfirmed). Counts below are from the script; what the tutorial teaches is the story domain's ([../story/](../story/)).

**Progress: 43/45 done, 0 chosen, 0 partial, 0 inferred — 96%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped; tests `LandScriptContents.realLandTScript`, `LandScriptContents.TheReaderTakesEachLineAndCountsWhatItsCommandsMake` |
| The tutorial land has no footpath file, as in the game | done | nothing to load; test `LandScriptContents.realLandTScript` |
| Loading the tutorial land starts the tutorial's own control script | pending | openblack always starts the story's control script (Land 1's challenges) |
| The tutorial can be chosen from the game's menu | pending | only through `--start-level LandT.txt` or the debug land menu |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `LandT.lnd` | done | `Game::LoadLandscape`; test `LandScriptContents.realLandTScript` |
| Is land number 6 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "0.00,0.00" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraLand1` |
| Scales the towns' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 3 towns (japanese 1, celtic 1, indian 1); owners: player one 2, neutral 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `LandScriptContents.realLandTScript`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 2 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 wonders (Japanese 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style F) (Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style A) (Indian 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 houses (style C) (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 workshops (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 storehouses (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 graveyards (Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 5 planned buildings for towns to build later (houses (style F) 5) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites; tests `LandScriptContents.realLandTScript`, `LandScriptTownCommandsTest.CreatePlannedAbodePutsThePlanOnTheTownsList` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 4 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 town leader living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 shepherd living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 forester living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 farmer living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 341 trees and bushes (Oak 101, OakA 69, Bush 51, BushA 20, BushB 15, Beech 14, PalmC 13, ConiferA 12, Palm 12, Conifer 7, Birch 7, Pine 6, PalmB 5, PalmA 5, Olive 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `LandScriptContents.realLandTScript`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 4 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `LandScriptContents.realLandTScript` |
| 1 × feature "Spikey Pilar Sand" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLandTScript` |
| 1 × feature "Pier" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLandTScript` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); every weight is zero, so a firefly caught here gives nothing; see [../nature/fireflies.md](../nature/fireflies.md); test `WorshipTest.FireFlyRewardProbabilities` |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 fields (Wheat 2) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `LandScriptContents.realLandTScript` |
| 5 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 2 pots (MagicWood 2) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 148 × swallow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 129 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 92 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 71 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 26 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 22 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 19 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `LandScriptContents.realLandTScript`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 76 × rocks and boulders (Boulder3Sand 12, Boulder2Sand 9, RockSandstone 8, Boulder3Lime 7, FlatrockSandstone 5, Boulder1Lime 5, FlatrockLimestone 5, LongrockSandstone 4, RockLimestone 4, Boulder2Lime 4, Boulder1Sand 3, SharprockLimestone 3, SharprockSandstone 2, SquarerockSandstone 2, LongrockLimestone 2, SquarerockLimestone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLandTScript`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 27 × toys (ToySkittle 16, ToyBowlingBall 5, ToyBall 4, ToyDie 2) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLandTScript`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 2 × WeepingStone | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLandTScript`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 6 lanterns (StreetLantern 4, CountryLantern 2) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `LandScriptContents.realLandTScript`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |
| 1 bonfires | done | `BonfireArchetype` (`CREATE_BONFIRE`); the temperature is not used on creation, as in the original; test `FireTraits.ABonfireHasAnEndlessSpotVisualAndNoFire` |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1 streams through 37 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`); test `LandScriptContents.realLandTScript` |
