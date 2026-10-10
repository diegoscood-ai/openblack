# Three Gods

Three gods around a central mountain: you and two computer gods each start with a home town and three neutral towns
of your own tribe nearby, and a rich Tibetan town with a wonder sits in the middle. Each home lies under its own
climate, one of them freezing. Online the same land is the three-player map "King of the hill".

**Players:** 3 · **Landscape:** `Data/Landscape/Multi_Player/MPM_3P_1.lnd` · **Script:** `Scripts/Playgrounds/ThreeGods.txt` · **Mode:** both

**Progress: 19/40 done, 7 partial — 56%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#three-gods-loading) (its "Three Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat two gods to control this realm." rather than its file name | partial | the debug menu's "Playground Islands" lists it by its start message title, or by its file name when it has none (`src/Level.cpp`, `src/Debug/Gui.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it from the debug menu; see the next row |
| No story script runs on a skirmish land | todo | our tree starts the story's control script (`LandControlAll`) on the start land whatever it is, and leaves the story's scripts running when a land is opened from the debug menu |
| The camera starts looking at player three's home town, not yours (the script's start point is that town's centre) | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |
| The land's title "Three Gods" and its line "Battle three gods for this realm" are its start message (the line miscounts: there are two other gods) | todo | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty; only the debug land list reads them, for its title and tooltip (`src/Level.cpp`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land about 1,900 by 1,750 paces built around a central mountain that reaches the highest height a land can have, with a fifth of it at sea level round the edges | done | `Game::LoadLandscape` |
| Three local climates: your home lies in a freezing, windy one (about -35 degrees), player three's in a cold, windy one (about 7 degrees), both set to be raining from the start, and player two's in a warm one (about 24 degrees) that starts dry | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`magic::map_script`, `src/Magic/Script/MapScriptWeather.cpp`); see ../../weather/ |
| Two storms are already blowing at the start, one in your cold climate and one in player three's | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`magic::map_script`, `src/Magic/Script/MapScriptWeather.cpp`); see ../../weather/ |
| Short days and long nights: a day of 1,000 seconds with 23% night and 17% dawn and dusk | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`) |
| The music follows the nearest town's tribe: Norse around you, Japanese around player two, Celtic around player three, Tibetan in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project 0.4 of their normal influence and gods 0.7 of theirs, so all three start further apart in reach | done | `SET_TOWN_INFLUENCE_MULTIPLIER` and `SET_PLAYER_INFLUENCE_MULTIPLIER`, kept in the map script globals and read by `src/ECS/Influence` |

## The three starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| You (player one) start with a temple and a Norse home town: seven houses, a crèche, a storehouse, a workshop, six fields and about 20 villagers | done | towns, buildings and villagers as in [playground_scripts.md](../../scripts/playground_scripts.md) |
| Player two starts the same with a Japanese town, player three with a Celtic town | done | as above |
| Each home town's store starts with about 3,600 to 4,000 food and wood, and no worship sites, dispensers or one-shot miracles are placed: everything else is won from towns | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god at 1.5 and offers heal, teleport and water | partial | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Each home town has four to six buildings planned for its people to build | done | `CREATE_PLANNED_ABODE` (`FeatureScriptCommands::CreatePlannedAbode`): the town's planned list, read by the building sites |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two and three | partial | `TOGGLE_COMPUTER_PLAYER` adds the player (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`); no mind thinks for it (see ../skirmish.md) |
| Player two has a lion and player three a horse, both from the generic computer creature mind, standing on their temples | partial | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it |
| Both creatures are sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| No personalities are loaded, so both play with the default personality | todo | no computer god mind in our tree; see ../../rival_gods/ai.md |
| Every town's belief in either computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`, `src/ECS/Town/TownBelief.cpp`) |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each god has three neutral towns of its own tribe nearby, laid out alike: one offering shield and physical shield, one storm, flying flock, creature freeze and creature weakness, and a farther fishing town offering fire and lightning | partial | towns, houses, villagers and fish farms are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |
| The Tibetan town in the middle has a wonder, a graveyard, nine houses, twelve fields and 20,000 food and 20,000 wood, believes in nobody at 1.5, and offers lightning, stronger lightning, beam explosion, storm and the strongest storm | partial | built with its stores and belief; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |
| The Tibetan town has twelve more buildings planned, including a wonder | done | `CREATE_PLANNED_ABODE` (`FeatureScriptCommands::CreatePlannedAbode`): the town's planned list, read by the building sites |
| Six people stand in towns without a house | done | `CREATE_TOWN_VILLAGER` (`CreateVillagerInTown`, `src/LHScriptX/VillagerCommands.cpp`) |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 684 trees (beech, conifers, palms, oaks, birches, olives, pines) and six big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 19 forests that foresters fell and replant | done | `CREATE_FOREST` (`ecs::CreateForest`): the forest the trees after it belong to |
| 43 fireflies | todo | `CREATE_FIRE_FLY` is empty; every map gets the nightly top-up instead; see [../../nature/fireflies.md](../../nature/fireflies.md); every reward weight is zero on this map, so catching one gives nothing |
| 374 animals in 71 flocks, mostly sheep (192), with pigs, pigeons, cows, gulls, crows and doves | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| 82 fenced wheat fields, 18 fish farms and one magic wood pot | done | `FieldArchetype`, `FishFarmArchetype`, `PotArchetype` |
| Limestone rocks and boulders and 48 street lanterns | done | `MobileStaticArchetype`, `StreetLanternArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish, and our tree ends none; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | no end of game in our tree; see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online as "King of the hill"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "King of the hill - 3 players", described as "3 Player map designed to test your godly powers. Try to take over the neutral towns to build up your holy powerbase, and destroy the other deities!", with a thumbnail | todo | our tree has no network play; see ../network_play.md |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | todo | our tree does not read online map files |
| Online no god is a computer god and the script makes no creatures: each person brings their own | todo | our tree has no network play; see ../network_play.md |
| Online every town's belief is capped evenly for all three gods (2 in the three home towns, 1 elsewhere) | todo | our tree has no network play; see ../network_play.md; the caps themselves work (`ecs::town_belief::SetCap`) |
| Online the neutral Japanese storm town has a gathering point for worshippers | todo | our tree has no network play; see ../network_play.md; the gathering points themselves work (`SET_TOWN_CONGREGATION_POS`) |
| Online the camera always starts on your own temple | todo | our tree has no network play; see ../network_play.md |
| The winning conditions offered for it start at 5,000 food, 5,000 wood, 1,600 belief and up to 12 towns taken | todo | see ../multiplayer_rules.md |
