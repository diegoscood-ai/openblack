# Four Gods

Four gods in the four corners of a mountain-ringed land: you and three computer gods each start with the same small
Celtic town, with neutral Japanese and Indian towns between you and a Tibetan wonder town in the middle. Online the
same land is the four-player map "The four corners of Eden".

**Players:** 4 · **Landscape:** `Data/Landscape/Multi_Player/MPM_4P_1.lnd` · **Script:** `Scripts/Playgrounds/FourGods.txt` · **Mode:** both

**Progress: 13/35 done, 9 partial — 50%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#four-gods-loading) (its "Four Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat three gods to control this realm." rather than its file name | partial | the debug menu's "Playground Islands" lists it by its start message title, or by its file name when it has none (`src/Level.cpp`, `src/Debug/Gui.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it from the debug menu; see the next row |
| No story script runs on a skirmish land | todo | our tree starts the story's control script (`LandControlAll`) on the start land whatever it is, and leaves the story's scripts running when a land is opened from the debug menu |
| The script gives no start point, so the camera starts zoomed onto your own temple | todo | `FeatureScriptCommands::StartCameraPos` looks at the point given, here the land's corner (0, 0), off the land |
| The land's title "Four Gods" and its line "Battle four other gods for this realm" are its start message (the line miscounts: there are three other gods) | todo | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty; only the debug land list reads them, for its title and tooltip (`src/Level.cpp`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land about 1,900 by 1,750 paces with its highest peaks in the four corners, where the temples stand, and lower ground and water between; about a fifth is at sea level | done | `Game::LoadLandscape` |
| One climate over the whole land: a mild 12 degrees, no rain and no wind | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`magic::map_script`, `src/Magic/Script/MapScriptWeather.cpp`); see ../../weather/ |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%), the standard day | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`) |
| The music follows the nearest town's tribe: Celtic in every corner, Japanese and Indian between, Tibetan in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project half their normal influence and gods their full influence | done | `SET_TOWN_INFLUENCE_MULTIPLIER` and `SET_PLAYER_INFLUENCE_MULTIPLIER`, kept in the map script globals and read by `src/ECS/Influence` |

## The four starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each god has a temple in a corner and a Celtic home town beside it: nine or ten houses, a graveyard, a crèche, a storehouse, a workshop, two fields and about 14 villagers | done | towns, buildings and villagers as in [playground_scripts.md](../../scripts/playground_scripts.md) |
| Each home store starts with 500 food and 500 wood | done | `AbodeArchetype` keeps the amounts |
| Your workshop alone starts with 12,750 wood, a head start over the other three (online too, for whoever plays player one) | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god fully and offers heal, teleport, food, water and creature freeze | partial | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two, three and four | partial | `TOGGLE_COMPUTER_PLAYER` adds the player (`FeatureScriptCommands::ToggleComputerPlayer`, `PlayerArchetype`); no mind thinks for it (see ../skirmish.md) |
| Player two has a horse, player three a tortoise and player four a zebra, all from the generic computer creature mind | partial | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it |
| All three creatures start bunched together on one edge of the land, between your temple and player two's, far from players three and four, whose creatures must cross the land to reach home | partial | made where the script puts them; nothing sends them home |
| The creatures are sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| No personalities are loaded, so all three play with the default personality | todo | no computer god mind in our tree; see ../../rival_gods/ai.md |
| Every town's belief in any computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`, `src/ECS/Town/TownBelief.cpp`) |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Four Japanese towns, one midway along each edge between two corners, each offering shield and physical shield | partial | towns, houses and villagers are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |
| Four Indian fishing towns in an inner ring, each offering lightning, creature strength and creature invisibility | partial | towns, houses, villagers and fish farms are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |
| The Tibetan town in the middle has a wonder, a graveyard, fifteen houses and about 24 people, and offers lightning, the strongest lightning, beam explosion and the strongest beam explosion | partial | built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 664 trees (mostly conifers and palms) and nine big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 21 forests that foresters fell and replant | done | `CREATE_FOREST` (`ecs::CreateForest`): the forest the trees after it belong to |
| 218 animals in 28 flocks: cows, pigs, sheep, pigeons, gulls, doves and 16 wolves | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| 30 fields and 16 fish farms | done | `FieldArchetype`, `FishFarmArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish, and our tree ends none; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | no end of game in our tree; see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |

## Online as "The four corners of Eden"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "The four corners of Eden - 4 players", described as "Tactical action for 4 players. You've got to be quick and clever in this battle for belief...", with a thumbnail | todo | our tree has no network play; see ../network_play.md |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | todo | our tree does not read online map files |
| Online no god is a computer god and the script makes no creatures: each person brings their own | todo | our tree has no network play; see ../network_play.md |
| Online every town's belief is capped evenly for all four gods (2 in the four home towns, 1 elsewhere) | todo | our tree has no network play; see ../network_play.md; the caps themselves work (`ecs::town_belief::SetCap`) |
| Online the camera always starts on your own temple | todo | our tree has no network play; see ../network_play.md |
| The winning conditions offered for it start at 1,000 food, 1,000 wood, 1,100 belief and up to 12 towns taken | todo | see ../multiplayer_rules.md |
