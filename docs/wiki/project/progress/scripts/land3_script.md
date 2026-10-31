# Land 3 script

`Scripts/Land3.txt` builds the third land, where the player's creature is taken from them: the rival god's two towns, their temple and worship sites, the creature's prison pillars, the weeping stones and a wood dispenser. Counts below are from the script; the challenges are the story domain's ([../story/](../story/)).

**Progress: 67/70 done, 0 chosen, 1 partial, 0 inferred — 97%**

## Loading the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script loads from its first line to its last in openblack | done | checked by reading it the way `Script.cpp` does: every line is a known command with the right arguments, so no line is skipped; tests `LandScriptContents.realLand3Script`, `LandScriptContents.TheReaderTakesEachLineAndCountsWhatItsCommandsMake` |
| The land's footpath file is loaded with it | done | `Game::LoadMap` (the land's `.fot`); test `LandScriptContents.realLand3Script` |
| The story moves on to this land when Land 2 is finished | pending | `LOAD_MAP` changes the land (`Game::ChangeLand`), but play has not yet got here: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Starting on this land runs this land's challenges | pending | the land control script always begins with Land 1's challenges, whatever land is loaded |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `Land3.lnd` | done | `Game::LoadLandscape`; test `LandScriptContents.realLand3Script` |
| Is land number 3 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "3237.21,3157.85" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraLand1` |
| Scales the towns' influence by 0.5 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Sets 3 land balance numbers (4 = 1.25, 5 = 1.21951, 6 = 2.03252) | done | `land_balance::Set` keeps them all, and each is read: 4 by the villagers' speed, 5 by the trees' wood, 6 by how fast artefacts gain worth (see [land_script_commands.md](land_script_commands.md#land-set-up)); test `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople` |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`); test `DayNightClock.ALandsCycleIsClamped` |
| Creates 4 climates and sets rain, temperature and wind 4, 4 and 4 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |

## Towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 6 towns (celtic 2, tibetan 1, egyptian 1, indian 1, japanese 1); owners: neutral 3, player two 2, player one 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `LandScriptContents.realLand3Script`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 6 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Caps belief 4 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`); test `TownBeliefTest.Scripts` |
| Leaves 1 towns uninhabitable (ruins and empty villages) | done | the town's uninhabitable flag (`SET_TOWN_UNINHABITABLE`), read by `src/ECS/Town/AbodeVillagers.cpp`; tests `LandScriptContents.realLand3Script`, `LandScriptTownCommandsTest.AnUninhabitableTownTakesNoVillager` |
| Gives 2 towns a gathering place | done | the town's congregation position (`SET_TOWN_CONGREGATION_POS`), read by `src/ECS/Town/TownQueries.cpp`; test `VillagerRepairTest.GotoCongregateInTownAfterEmergency` |
| Gives towns 23 miracles to offer at their village centres (fire 3, lightning bolt 3, heal 2, food 2, storm 2, water 2, teleport 2, shield 2, physical shield 2, fire pu1 1, nature 1, water pu1 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/; test `LandScriptContents.realLand3Script` |

## Buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 11 houses (style F) (Indian 5, Egyptian 3, Japanese 2, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 10 houses (style A) (Egyptian 4, Indian 2, Japanese 2, Tibetan 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 10 houses (style D) (Egyptian 5, Tibetan 2, Japanese 2, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 6 houses (style C) (Tibetan 3, Egyptian 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 storehouses (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 houses (style B) (Japanese 3, Egyptian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 crèches (Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style E) (Tibetan 1, Indian 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 village centres (Tibetan 1, Egyptian 1, Indian 1, Japanese 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 70 planned buildings for towns to build later (houses (style F) 21, houses (style B) 11, houses (style C) 11, houses (style A) 9, houses (style D) 9, houses (style E) 4, crèches 2, village centres 1, workshops 1, wonders 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites; tests `LandScriptContents.realLand3Script`, `LandScriptTownCommandsTest.CreatePlannedAbodePutsThePlanOnTheTownsList` |
| 1 temples of rival gods (player two 1) | done | `CitadelArchetype` (`CREATE_CITADEL`); test `CitadelPlanTest.CreateCitadelIsBuilt` |
| The player's planned temple (built when the player arrives) | done | `CitadelArchetype::CreatePlan` (`CREATE_PLANNED_CITADEL`); test `CitadelPlanTest.PlanIsOnlyAPlan` |
| 2 worship sites (egyptian for player two 1, tibetan for player two 1) | done | `magic::script::CreateWorshipSite` (`CREATE_WORSHIP_SITE`): a built site at the player's temple; tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.AWorshipSiteNeedsThePlayersTemple` |
| 1 miracle dispensers (wood 1) | done | `magic::script::CreateSpellDispenser` (`CREATE_SPELL_DISPENSER`); test `LandScriptContents.realLand3Script` |

## People

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 68 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 46 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 10 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 9 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 7 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 2 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 villagers placed in towns without a house (housewives 1) | done | `CreateVillagerInTown` (`CREATE_TOWN_VILLAGER`); test `ScriptVillagerCommands.InfoFromText` |

## Nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 1397 trees and bushes (PalmB 454, Palm 279, BushB 132, Conifer 76, PalmA 71, PalmC 69, Beech 54, Cedar 52, Bush 39, HedgeA 39, Pine 38, BushA 33, Olive 26, Hedge 15, ConiferA 8, Copse 6, Cypress 5, Birch 1) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 1 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 16 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `LandScriptContents.realLand3Script` |
| 20 × feature "Spikey Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 6 × feature "Pilar2 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 1 × feature "Prison Pillar 1" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 1 × feature "Prison Pillar 2" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 1 × feature "Prison Pillar 3" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 1 × feature "Tibetan Large Pillar Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`); test `LandScriptContents.realLand3Script` |
| 70 patches of mist | done | `MistArchetype` (`CREATE_MIST`); test `LandScriptContents.realLand3Script` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); eleven common miracles about 9% each, flocks under 1%, no creature miracles; see [../nature/fireflies.md](../nature/fireflies.md); test `WorshipTest.FireFlyRewardProbabilities` |

## Food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 18 fields (Wheat 14, WheatWithFence 4) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `LandScriptContents.realLand3Script` |
| 7 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 2 pots (MagicWood 2) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |

## Animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 27 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 9 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 8 × tortoise | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 8 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 6 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 6 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 5 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 15 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `LandScriptContents.realLand3Script`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 106 × rocks and boulders (Rock 83, Boulder1Sand 7, Boulder3Lime 3, RockLimestone 3, Boulder2Lime 3, SharprockLimestone 2, Boulder1Lime 2, FlatrockLimestone 2, LongrockLimestone 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 87 × fences (CeltFenceShort 49, CeltFenceTall 38) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 8 × WeepingStoneReward | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 13 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 7 × Champi (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 6 × Toadstool (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 1 lanterns (StreetLantern 1) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `LandScriptContents.realLand3Script`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |

## Water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2 streams through 18 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`); test `LandScriptContents.realLand3Script` |

## Rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial 60% | `TOGGLE_COMPUTER_PLAYER` adds each player, marks it a computer player and switches its computer mind on (`ComputerPlayerSystem::SwitchOn`), which starts on the land, runs the scripts' hand actions and weighs its tree each turn (`ComputerPlayerSystem::ProcessTurn`), but 57 of its 59 actions are not ported yet (see [../rival_gods/ai.md](../rival_gods/ai.md)) |
