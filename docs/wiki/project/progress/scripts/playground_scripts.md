# Playground scripts

This page covers the land scripts of three skirmish lands in `Scripts/Playgrounds/`, Two, Three and Four Gods: a player against one to three rival gods (the computer players) on lands made for two, three or four gods, and Firestorm, a copy of the game's online map `Online Maps/mpm_3p_2.map` (an 8-byte header and a land script), whose sections are parked with online ([README.md](../README.md#how-the-dashboard-groups-the-rows)). Each section counts what its script builds and whether openblack builds it. Rival gods' play is the multiplayer and skirmish domain's ([../multiplayer/](../multiplayer/)); openblack opens these lands only through the debug land menu or `--start-level`, and the story's control script still starts on them.

**Progress: 210/225 done, 0 chosen, 14 partial, 0 inferred — 96%**

## Two Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/TwoGods.txt`, 1,000 lines, landscape `MPM_2P_1.lnd`, two players) loads from first line to last | done | every line is a known command with the right arguments when read the way `Script.cpp` does; test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Its footpath file `twogods.fot` is loaded with it | done | `Game::LoadMap`; test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| It can be chosen from the game's skirmish menu, with its title and description | partial 30% | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Two Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_2P_1.lnd` | done | `Game::LoadLandscape`; test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Is land number 0 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "2185.72,2315.78" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraTwoGods` |
| Scales the towns' influence by 0.4 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Sets 3 land balance numbers (4 = 2.75, 5 = 4, 6 = 20) | done | `land_balance::Set` keeps them all, and each is read: 4 by the villagers' speed, 5 by the trees' wood, 6 by how fast artefacts gain worth (see [land_script_commands.md](land_script_commands.md#land-set-up)); test `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople` |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`); test `DayNightClock.ALandsCycleIsClamped` |
| Titles the land "Two Gods" with 1 description line(s) | partial 20% | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |

## Two Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 10 towns (norse 3, celtic 2, indian 2, african 1, greek 1, egyptian 1); owners: neutral 6, player one 2, player two 2 | done | `TownArchetype` (`CREATE_TOWN`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 9 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Caps belief 10 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`); test `TownBeliefTest.Scripts` |
| Gives towns 39 miracles to offer at their village centres (storm 6, fire 4, storm pu1 4, flying flock 3, ground flock 3, heal 2, teleport 2, water 2, water pu1 2, nature 2, physical shield 2, storm pu2 2, lightning bolt 2, wood 2, shield 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/; test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |

## Two Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 14 houses (style F) (Celtic 8, Norse 3, African 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 houses (style A) (Norse 7, Celtic 4, African 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 10 houses (style B) (Norse 5, Celtic 2, Greek 2, African 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 7 storehouses (Norse 3, Celtic 2, African 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 houses (style D) (Norse 2, Celtic 1, African 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 crèches (Celtic 2, African 1, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 houses (style E) (African 2, Celtic 1, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 workshops (Celtic 2, Greek 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 2 houses (style C) (Greek 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 9 village centres (Norse 3, Celtic 2, Indian 2, African 1, Greek 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 2 temples of rival gods (player one 1, player two 1) | done | `CitadelArchetype` (`CREATE_CITADEL`); test `CitadelPlanTest.CreateCitadelIsBuilt` |
| 4 worship sites (indian for player one 1, celtic for player one 1, indian for player two 1, celtic for player two 1) | done | `magic::script::CreateWorshipSite` (`CREATE_WORSHIP_SITE`): a built site at the player's temple; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.AWorshipSiteNeedsThePlayersTemple` |
| 1 miracle dispensers (beam explosion 1) | done | `magic::script::CreateSpellDispenser` (`CREATE_SPELL_DISPENSER`); test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| 12 scaffolds | done | `FeatureScriptCommands::CreateScaffold` (`CREATE_SCAFFOLD`): each the numbered town's scaffold at its place, angle and size (test `ScaffoldsTest.ALandFilesScaffoldIsMadeOfTheNumberedTownAtItsPlaceAngleAndSize`) |

## Two Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 54 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 47 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 25 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 7 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 6 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 5 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 3 villagers placed in towns without a house (town leaders 1, farmers 1, foresters 1) | done | `CreateVillagerInTown` (`CREATE_TOWN_VILLAGER`); test `ScriptVillagerCommands.InfoFromText` |

## Two Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 308 trees and bushes (ConiferA 141, PalmB 57, Bush 43, Palm 25, Birch 22, CypressA 9, PalmA 7, CopseA 4) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 4 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 3 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); shared with Four Gods: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table); test `WorshipTest.FireFlyRewardProbabilities` |

## Two Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 24 fields (Wheat 13, Cereal 7, Corn 4) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |
| 6 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 3 pots (MagicWood 3) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |

## Two Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 51 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 20 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 × pigeon | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 8 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 8 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 22 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Two Gods: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 39 × rocks and boulders (Rock 39) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 12 × Champi (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.CreateMobileObjectIsNotMadeOnTopOfAFixedObject` |
| 31 lanterns (StreetLantern 31) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |

## Two Gods: miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 9 one-shot miracles lying on the land (water 4, flying flock 4, beam explosion 1) | done | `magic::script::CreateOneShotSpellPu` (`CREATE_ONE_SHOT_SPELL_PU`); test `PlaygroundScriptContents.TwoGodsLoadsWholeAndMakesWhatItsRowsCount` |

## Two Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 1 computer players (player two) | partial 60% | `TOGGLE_COMPUTER_PLAYER` adds each player, marks it a computer player and switches its computer mind on (`ComputerPlayerSystem::SwitchOn`), which starts on the land, runs the scripts' hand actions and weighs its tree each turn (`ComputerPlayerSystem::ProcessTurn`), but 57 of its 59 actions are not ported yet (see [../rival_gods/ai.md](../rival_gods/ai.md)) |
| 1 rival creatures from mind files (ComputerControlledCreature as Leopard 1) | partial 90% | `ecs::player_creature::MakeFromMindFile` (`CREATE_CREATURE_FROM_FILE`): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; left: the file's fatness field and its fight health are not read (see [land_script_commands.md](land_script_commands.md), creature_mind_scripts.md) |
| Sets 1 rival creatures' liking for the player | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42); test `ComputerPlayerStart.TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize` |

## Three Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/ThreeGods.txt`, 2,063 lines, landscape `MPM_3P_1.lnd`, three players) loads from first line to last | done | every line is a known command with the right arguments when read the way `Script.cpp` does; test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Its footpath file `threegods.fot` is loaded with it | done | `Game::LoadMap`; test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |
| It can be chosen from the game's skirmish menu, with its title and description | partial 30% | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Three Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_3P_1.lnd` | done | `Game::LoadLandscape`; test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Is land number 0 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "2816.90,2740.75" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraThreeGods` |
| Scales the towns' influence by 0.4 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 0.7 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Sets 3 land balance numbers (4 = 2.75, 5 = 4, 6 = 20) | done | `land_balance::Set` keeps them all, and each is read: 4 by the villagers' speed, 5 by the trees' wood, 6 by how fast artefacts gain worth (see [land_script_commands.md](land_script_commands.md#land-set-up)); test `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople` |
| Sets a day of 1000 with 23% night and 17% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`); test `DayNightClock.ALandsCycleIsClamped` |
| Titles the land "Three Gods" with 1 description line(s) | partial 20% | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| Creates 3 climates and sets rain, temperature and wind 3, 3 and 3 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |
| Creates 2 storms that are there from the start | done | `magic::map_script::CreateWeatherStorm` (`CREATE_WEATHER_STORM`); test `WeatherTest.ScriptStormBytes` |

## Three Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 13 towns (norse 4, japanese 4, celtic 4, tibetan 1); owners: neutral 10, player one 1, player two 1, player three 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 13 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Caps belief 26 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`); test `TownBeliefTest.Scripts` |
| Gives towns 42 miracles to offer at their village centres (lightning bolt 4, lightning bolt pu1 4, storm 4, physical shield 4, heal 3, teleport 3, water 3, shield 3, flying flock 3, creature spell freeze 3, creature spell weak 3, fire 3, beam explosion 1, storm pu2 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/; test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |

## Three Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 houses (style B) (Norse 6, Celtic 5, Japanese 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 16 houses (style F) (Norse 6, Japanese 6, Celtic 3, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 15 houses (style E) (Celtic 6, Japanese 4, Tibetan 3, Norse 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 15 houses (style D) (Japanese 6, Celtic 5, Norse 4) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 crèches (Norse 4, Japanese 4, Celtic 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 houses (style C) (Japanese 5, Tibetan 3, Norse 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 12 storehouses (Norse 4, Celtic 4, Japanese 3, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 10 houses (style A) (Norse 6, Celtic 2, Tibetan 1, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 3 workshops (Norse 1, Japanese 1, Celtic 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 wonders (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 graveyards (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 village centres (Norse 4, Japanese 4, Celtic 4, Tibetan 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 82 planned buildings for towns to build later (houses (style A) 17, graveyards 12, houses (style B) 11, houses (style E) 11, houses (style F) 9, houses (style C) 9, workshops 8, houses (style D) 3, storehouses 1, wonders 1) | done | `FeatureScriptCommands::CreatePlannedAbode` (`CREATE_PLANNED_ABODE`): the town's planned list, never drawn, read by the building sites; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptTownCommandsTest.CreatePlannedAbodePutsThePlanOnTheTownsList` |
| 3 temples of rival gods (player one 1, player two 1, player three 1) | done | `CitadelArchetype` (`CREATE_CITADEL`); test `CitadelPlanTest.CreateCitadelIsBuilt` |

## Three Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 133 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 96 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 22 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 12 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 2 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 6 villagers placed in towns without a house (foresters 6) | done | `CreateVillagerInTown` (`CREATE_TOWN_VILLAGER`); test `ScriptVillagerCommands.InfoFromText` |

## Three Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 684 trees and bushes (Beech 128, ConiferA 123, Palm 116, OakA 100, Birch 64, Olive 48, PalmA 43, Pine 36, Conifer 20, Cypress 6) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 19 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 6 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |
| 43 fireflies | pending | `CREATE_FIRE_FLY` is empty; every map also gets the nightly top-up: [../nature/fireflies.md](../nature/fireflies.md#where-they-come-from) |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); every chance is zero, so its fireflies give nothing: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table); test `WorshipTest.FireFlyRewardProbabilities` |

## Three Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 82 fields (WheatWithFence 82) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount` |
| 18 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |
| 1 pots (MagicWood 1) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |

## Three Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 192 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 54 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 42 × pigeon | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 36 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 26 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 17 × crow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 7 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 71 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Three Gods: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 35 × rocks and boulders (Boulder1Lime 13, Boulder2Lime 7, SharprockLimestone 5, FlatrockLimestone 3, SquarerockLimestone 3, RockLimestone 3, Boulder3Lime 1) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`); tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.CreateMobileStaticTakesItsYAngleAndScaleAndMakesABonfireForTheBonfireInfo` |
| 48 lanterns (StreetLantern 48) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`); tests `PlaygroundScriptContents.ThreeGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.AStreetLanternIsATownLightOnlyForTheStreetLanternInfo` |

## Three Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 2 computer players (player two, player three) | partial 60% | `TOGGLE_COMPUTER_PLAYER` adds each player, marks it a computer player and switches its computer mind on (`ComputerPlayerSystem::SwitchOn`), which starts on the land, runs the scripts' hand actions and weighs its tree each turn (`ComputerPlayerSystem::ProcessTurn`), but 57 of its 59 actions are not ported yet (see [../rival_gods/ai.md](../rival_gods/ai.md)) |
| 2 rival creatures from mind files (COMPUTERCONTROLLEDCREATURE as Lion 1, COMPUTERCONTROLLEDCREATURE as Horse 1) | partial 90% | `ecs::player_creature::MakeFromMindFile` (`CREATE_CREATURE_FROM_FILE`): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; left: the file's fatness field and its fight health are not read (see [land_script_commands.md](land_script_commands.md), creature_mind_scripts.md) |
| Sets 2 rival creatures' liking for the player | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42); test `ComputerPlayerStart.TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize` |

## Four Gods: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (`Playgrounds/FourGods.txt`, 1,550 lines, landscape `MPM_4P_1.lnd`, four players) loads from first line to last | done | every line is a known command with the right arguments when read the way `Script.cpp` does; test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Its footpath file `fourgods.fot` is loaded with it | done | `Game::LoadMap`; test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |
| It can be chosen from the game's skirmish menu, with its title and description | partial 30% | listed in the debug land menu with its description as a tooltip; no skirmish menu |

## Four Gods: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `MPM_4P_1.lnd` | done | `Game::LoadLandscape`; test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Is land number 0 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "0.00,0.00" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraZero` |
| Scales the towns' influence by 0.5 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 1 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Sets 3 land balance numbers (4 = 2, 5 = 2, 6 = 10) | done | `land_balance::Set` keeps them all, and each is read: 4 by the villagers' speed, 5 by the trees' wood, 6 by how fast artefacts gain worth (see [land_script_commands.md](land_script_commands.md#land-set-up)); test `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople` |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`); test `DayNightClock.ALandsCycleIsClamped` |
| Titles the land "Four Gods" with 1 description line(s) | partial 20% | shown only as the debug land menu's tooltip; in the game the land info box pops up with the title and line when the script reaches it ([../multiplayer/skirmish.md](../multiplayer/skirmish.md)) |
| Creates 1 climates and sets rain, temperature and wind 1, 1 and 1 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |

## Four Gods: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 13 towns (celtic 4, japanese 4, indian 4, tibetan 1); owners: neutral 9, player one 1, player two 1, player three 1, player four 1 | done | `TownArchetype` (`CREATE_TOWN`); tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptTownCommandsTest.CreateTownGivesTheTownItsOwnerAndTribe` |
| Sets 4 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Caps belief 39 times | done | `ecs::town_belief::SetCap` (`SET_TOWN_BELIEF_CAP`); test `TownBeliefTest.Scripts` |
| Gives towns 44 miracles to offer at their village centres (lightning bolt 5, heal 4, teleport 4, food 4, water 4, creature spell freeze 4, shield 4, physical shield 4, creature spell strong 4, creature spell invisible 4, lightning bolt pu2 1, beam explosion 1, beam explosion pu2 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/; test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |

## Four Gods: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 38 houses (style A) (Indian 17, Celtic 11, Japanese 7, Tibetan 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 25 houses (style F) (Indian 11, Celtic 6, Japanese 6, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 19 houses (style E) (Japanese 8, Celtic 5, Indian 4, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 18 houses (style B) (Celtic 9, Japanese 6, Tibetan 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 storehouses (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 crèches (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 11 houses (style D) (Japanese 6, Tibetan 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 9 houses (style C) (Celtic 4, Japanese 3, Tibetan 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 9 workshops (Celtic 4, Japanese 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 5 graveyards (Celtic 4, Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 wonders (Tibetan 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 13 village centres (Celtic 4, Japanese 4, Indian 4, Tibetan 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 4 temples of rival gods (player one 1, player two 1, player three 1, player four 1) | done | `CitadelArchetype` (`CREATE_CITADEL`); test `CitadelPlanTest.CreateCitadelIsBuilt` |

## Four Gods: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 55 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 51 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 39 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 16 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 15 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 12 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 1 trader living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |

## Four Gods: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 664 trees and bushes (ConiferA 320, PalmB 200, Palm 77, PalmA 36, Conifer 17, Pine 14) | done | `TreeArchetype` (`CREATE_NEW_TREE`); tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `LandScriptObjectCommandsTest.NoTreeIsMadeOnTopOfAFixedObject` |
| 21 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 9 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`); test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |
| Sets 42 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); shared with Two Gods: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table); test `WorshipTest.FireFlyRewardProbabilities` |

## Four Gods: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 30 fields (Wheat 14, Corn 10, Cereal 6) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`); test `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount` |
| 16 fish farms | done | `FishFarmArchetype` (`CREATE_TOWN_FISH_FARM`); test `FishFarmTown.AFarmBelongsToTheTownNearestItAndToThatTownsPlayer` |

## Four Gods: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 50 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 50 × pig | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 34 × sheep | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 24 × pigeon | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 24 × seagull | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 20 × dove | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 16 × wolf | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain; tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsAnimalJoinsTheFlockOfItsIdOrGetsOneOfItsOwn` |
| 28 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`); tests `PlaygroundScriptContents.FourGodsLoadsWholeAndMakesWhatItsRowsCount`, `AnimalBehaviourTest.TheLandScriptsFlockTakesItsDistanceAndTownByTheScriptsVersion` |

## Four Gods: rival gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Switches on 3 computer players (player two, player three, player four) | partial 60% | `TOGGLE_COMPUTER_PLAYER` adds each player, marks it a computer player and switches its computer mind on (`ComputerPlayerSystem::SwitchOn`), which starts on the land, runs the scripts' hand actions and weighs its tree each turn (`ComputerPlayerSystem::ProcessTurn`), but 57 of its 59 actions are not ported yet (see [../rival_gods/ai.md](../rival_gods/ai.md)) |
| 3 rival creatures from mind files (ComputerControlledCreature as Horse 1, ComputerControlledCreature as Tortoise 1, ComputerControlledCreature as Zebra 1) | partial 90% | `ecs::player_creature::MakeFromMindFile` (`CREATE_CREATURE_FROM_FILE`): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; left: the file's fatness field and its fight health are not read (see [land_script_commands.md](land_script_commands.md), creature_mind_scripts.md) |
| Sets 3 rival creatures' liking for the player | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42); test `ComputerPlayerStart.TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize` |

## Firestorm: loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script (the game's online map `Online Maps/mpm_3p_2.map`, an 8-byte header and a land script; 1,540 lines, landscape `mpm_3p_2.lnd`, three players) loads from first line to last | done | every line was a known command with the right arguments in raffclar's check, made on a copy named `Playgrounds/Firestorm.txt`, which is in no folder of the game; our reader skips a bad line rather than stopping |
| It has no footpath file, as in the game | done | nothing to load |
| It can be chosen from the game's skirmish menu | partial 30% | listed in the debug land menu only |

## Firestorm: set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Loads the landscape `mpm_3p_2.lnd` | done | `Game::LoadLandscape` |
| Is land number 0 of the story | done | kept in the map script globals (`SET_LAND_NUMBER`); test `LandLoadGlobals.ALandsLoadKeepsTheLastLandsNumberAndMultipliers` |
| Starts the camera looking at "1441.56,2081.76" | done | `FeatureScriptCommands::StartCameraPos`; test `SetCameraPos.setCameraLand1` |
| Scales the towns' influence by 0.01 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.townRadiusAndPlayer` |
| Scales the players' influence by 0.8 | done | kept in the map script globals and read by `src/ECS/Influence`; test `InfluenceTest.citadelStoryInfluence` |
| Sets 3 land balance numbers (4 = 2.5, 5 = 3.01471, 6 = 5) | done | `land_balance::Set` keeps them all, and each is read: 4 by the villagers' speed, 5 by the trees' wood, 6 by how fast artefacts gain worth (see [land_script_commands.md](land_script_commands.md#land-set-up)); test `ArtefactSystemTest.EveryThousandTurnsASitesArtefactsGainWorthByItsDancersShareOfItsPlayersPeople` |
| Sets a day of 1700 with 8.3% night and 7% dawn and dusk | done | `DayNightClock::SetCycleFromLand` (`SET_NIGHTTIME`); test `DayNightClock.ALandsCycleIsClamped` |
| Creates 1 climates and sets rain, temperature and wind 1, 1 and 1 times | done | `magic::map_script::CreateWeatherClimate` and its rain, temperature and wind (`src/Magic/Script/MapScriptWeather.cpp`); see ../weather/; test `WeatherTest.ClimatesAddTemperatureAndWind` |

## Firestorm: towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Creates 22 towns (celtic 19, aztec 1, japanese 1, norse 1); owners: neutral 19, player one 1, player two 1, player three 1 | done | `TownArchetype` (`CREATE_TOWN`) |
| Sets 22 towns' belief in a player | done | `ecs::town_belief::SetBeliefInPlayer` (`SET_TOWN_BELIEF`); test `TownBeliefTest.Scripts` |
| Gives 21 towns a gathering place | done | the town's congregation position (`SET_TOWN_CONGREGATION_POS`), read by `src/ECS/Town/TownQueries.cpp`; test `VillagerRepairTest.GotoCongregateInTownAfterEmergency` |
| Gives towns 28 miracles to offer at their village centres (shield 4, physical shield 4, water 4, fire 3, fire pu2 3, creature spell invisible 3, creature spell itchy 3, heal 1, heal pu1 1, creature spell freeze 1, creature spell strong 1) | done | `magic::script::CreateNewTownSpell` (`CREATE_NEW_TOWN_SPELL`): the town holds the miracle; see ../miracles/ |

## Firestorm: buildings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 15 houses (style A) (Celtic 7, Aztec 4, Japanese 4) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 14 houses (style B) (Aztec 4, Norse 4, Celtic 3, Japanese 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 12 houses (style D) (Aztec 5, Japanese 4, Celtic 3) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 11 houses (style C) (Norse 6, Celtic 2, Aztec 2, Japanese 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 9 houses (style E) (Japanese 4, Aztec 3, Celtic 2) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 storehouses (Celtic 1, Aztec 1, Japanese 1, Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 houses (style F) (Japanese 2, Celtic 1, Aztec 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 1 wonders (Norse 1) | done | `AbodeArchetype` (`CREATE_ABODE`); test `TestFixed.celticAbodeBoundingCircle` |
| 4 village centres (Celtic 1, Aztec 1, Japanese 1, Norse 1) | done | `AbodeArchetype` made the town's centre, with its worship share (`CREATE_TOWN_CENTRE`); test `TestFixed.celticTownCentreBoundingCircle` |
| 3 temples of rival gods (player one 1, player two 1, player three 1) | done | `CitadelArchetype` (`CREATE_CITADEL`); test `CitadelPlanTest.CreateCitadelIsBuilt` |
| 33 miracle dispensers (fire 7, heal 5, fire pu1 3, shield 3, creature spell big 2, creature spell weak 2, storm 2, lightning bolt 2, teleport 2, creature spell angry 2, creature spell invisible 1, creature spell compassion 1, creature spell strong 1) | done | `magic::script::CreateSpellDispenser` (`CREATE_SPELL_DISPENSER`) |

## Firestorm: people

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 82 housewives living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 31 fishermen living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 24 farmers living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 16 shepherds living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 15 foresters living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| 3 town leaders living in their houses | done | `VillagerArchetype` through `CreateVillagerAtAbode` (`CREATE_VILLAGER_POS`); test `ScriptVillagerCommandsAbodes.FindAbodeAt` |

## Firestorm: nature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 304 trees and bushes (OakA 65, Oak 57, Bush 43, Pine 40, PalmC 23, Palm 18, Birch 14, PalmA 12, Olive 10, Cedar 8, PalmB 4, Beech 4, BushB 3, BushA 3) | done | `TreeArchetype` (`CREATE_NEW_TREE`) |
| 10 forests that trees belong to (foresters fell and replant them) | done | `ecs::CreateForest` (`CREATE_FOREST`): the forest the trees after it are looked up in; test `ScenicForestTest.ForestIdsFollowTheCounterAndMayRepeat` |
| 9 big forest models | done | `BigForestArchetype` (`CREATE_NEW_BIG_FOREST`) |
| 49 × feature "Egyptian Needle Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 24 × feature "Aztec Statue Feature" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar3 Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 3 × feature "Pilar Lime" | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| 5 patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| Sets 44 firefly miracle reward chances | done | kept in the map globals' reward table (`FIRE_FLY_SPELL_REWARD_PROB`); every chance is zero, so its fireflies give nothing: [../nature/fireflies.md](../nature/fireflies.md#the-lands-reward-table); test `WorshipTest.FireFlyRewardProbabilities` |

## Firestorm: food

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 16 fields (Wheat 6, Corn 6, WheatWithFence 4) | done | `FieldArchetype` (`CREATE_NEW_TOWN_FIELD`) |
| 18 pots (MagicWood 18) | done | `PotArchetype` (`CREATE_POT`); test `TakeResourceTest.ALandScriptsPotIsMadeOnlyWithAnAmountAndOffAnyFixedObjectUnlessInTheWater` |
| 28 drinking places | done | a `DrinkWaypoint` is kept in the order made (`CREATE_DRINK_WAYPOINT`); the creature drinks at the nearest one under 1000 (`mind_detail::DrinkPlaceOf`; unverified against v1.42); test `CreatureDrink.TheNearestWaterPointMustBeNearerThanAThousand` |

## Firestorm: animals

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 45 × cow | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 25 × horse | done | `AnimalArchetype` (`CREATE_NEW_ANIMAL`), with its flock and town; how it behaves is the animals' domain |
| 12 flocks | done | `FeatureScriptCommands::CreateFlock` (`CREATE_FLOCK`) |

## Firestorm: objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 149 × rocks and boulders (Boulder2Lime 36, Boulder3Lime 28, Boulder1Lime 27, FlatrockLimestone 19, LongrockLimestone 14, SquarerockLimestone 6, RockLimestone 6, Rock 5, Boulder2Volcanic 3, SharprockLimestone 3, Boulder1Chalk 2) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 3 × toys (ToyCuddly 3) | done | `MobileStaticArchetype` (`CREATE_MOBILE_STATIC`) |
| 27 × MagicMushroom (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 6 × Toadstool (can be picked up) | done | `MobileObjectArchetype` (`CREATE_MOBILEOBJECT`) |
| 32 lanterns (CountryLantern 18, StreetLantern 14) | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`) |
| 1 street light | partial 50% | `StreetLanternArchetype` as a town lantern (`CREATE_STREET_LIGHT`); the street light itself is not decoded |

## Firestorm: water and paths

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 4 streams through 71 points | done | the streams and their points are kept and the rivers built from them after the last line (`ecs::CreateRiverFootprints`) |

## Firestorm: miracles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 225 one-shot miracles lying on the land (fire 49, heal 32, fire pu1 21, shield 18, lightning bolt 14, storm 14, creature spell weak 14, teleport 14, creature spell big 14, creature spell angry 14, creature spell invisible 7, creature spell strong 7, creature spell compassion 7) | done | `magic::script::CreateOneShotSpellPu` (`CREATE_ONE_SHOT_SPELL_PU`) |
