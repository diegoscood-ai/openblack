# Death Comes To Those That Wait

A fan-made four-corner land by "Fat Omen", reworked from Four Gods: you start rich, with nine powerful dispensers in an
empty corner, but seven computer creatures begin together a short walk from your temple, among 22 neutral towns of
every tribe, most with wonders. Its start line warns "Make your moves fast now or never!".

**Players:** 4 (two computer gods, plus creatures for four players with no temple) · **Landscape:** `Data/Landscape/Multi_Player/DCTTTW.lnd` · **Script:** `Scripts/Playgrounds/Death Comes To Those That Wait.txt` · **Mode:** skirmish

**Progress: 14/33 done, 11 partial — 59%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#death-comes-to-those-that-wait-loading) (its "Death Comes
To Those That Wait" sections); this file covers the land as a player meets it. How skirmish is set up is in
[../skirmish.md](../skirmish.md); the computer gods are in
[../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

openblack reads this script to its end, skipping its misspelt lines as the game does, so the land is built; the rows
still todo are mostly about how a skirmish starts and ends.

## Where it comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A fan's land, signed "By Fat Omen" in its title, built on Four Gods: the same land size, the same corner temples and home towns for players three and four, and the same spot for the computer creatures | n/a | history of the file; neither file is among those the game installs |

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it by its file name, "Death Comes To Those That Wait" | partial | the debug menu lists it by its start message title "Death Comes To Those That Wait -- By Fat Omen" with the warning as a tooltip (`src/Level.cpp`); no skirmish box |
| The whole script loads, the game skipping lines it does not know | partial | `Script::Load` logs and skips each unknown or misspelt line and goes on, as the game does, but it also skips the line missing its closing bracket, which the game reads (not checked against the file, which is not in our game folder) |
| No story script runs on a skirmish land | todo | our tree starts the story's control script (`LandControlAll`) on the start land whatever it is, and leaves the story's scripts running when a land is opened from the debug menu |
| The script gives no start point, so the camera starts zoomed onto your own temple | todo | `FeatureScriptCommands::StartCameraPos` looks at the point given, here the land's corner (0, 0), off the land |
| Its start message is the title and "Make your moves fast now or never!" | todo | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty; only the debug land list reads them, for its title and tooltip (`src/Level.cpp`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A land the size of Four Gods' (about 1,900 by 1,750 paces) but raised and reshaped: hilly almost everywhere, mountains reaching the highest possible height, and only an eighth at sea level | done | `Game::LoadLandscape` |
| One climate over the whole land: a mild 12 degrees, no rain and no wind | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`magic::map_script`, `src/Magic/Script/MapScriptWeather.cpp`); see ../../weather/ |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%) | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`) |
| Six land balance settings, three more than the game's own lands set | partial | `land_balance::Set` keeps all six; only numbers 4 (villagers' speed) and 5 (trees) are read (`src/LandBalance.h`) |
| Towns project half their normal influence and gods their full influence | done | `SET_TOWN_INFLUENCE_MULTIPLIER` and `SET_PLAYER_INFLUENCE_MULTIPLIER`, kept in the map script globals and read by `src/ECS/Influence` |
| The music follows the nearest town's tribe, here every one of the game's nine tribes | done | `src/Audio/GameMusic.cpp`, by the nearest town's tribe (not checked against the file, which is not in our game folder) |

## Your start (player one)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Your temple in a corner, beside an Aztec home town of nine houses, a graveyard, a crèche, a storehouse, a workshop, two fields and about 14 villagers | done | towns, buildings and villagers as in [playground_scripts.md](../../scripts/playground_scripts.md) |
| A vast store: 5,000,000 food and 5,000,000 wood in the storehouse and 1,275,000 more wood in the workshop | done | `AbodeArchetype` keeps the amounts |
| Your town offers stronger heal, teleport, stronger food, stronger water, creature itchiness and wood | partial | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Nine Indian miracle dispensers belonging to your town but standing in the empty far corner where Four Gods' player two lived: the strongest beam explosion, lightning, storm and fire, flying and ground flocks, stronger heal, nature and stronger food | done | `CREATE_SPELL_DISPENSER` (`magic::script::CreateSpellDispenser`) |
| 119 special villagers meant for every town are written with a misspelt command, so the game makes none of them | done | the reader logs the unknown command and skips each line, so none is made, as in the game |

## The creatures and computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Players three and four are computer gods with Celtic towns and temples in their corners, as in Four Gods | partial | the towns and temples are made (`TownArchetype`, `CitadelArchetype`); `TOGGLE_COMPUTER_PLAYER` adds the players, but no mind thinks for them |
| Player three has a tortoise and player four a zebra from the generic computer creature mind | partial | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it |
| Players two, five, six and seven get a horse each from the same mind but no temple, no town and no computer god | partial | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it; how the game drives a creature whose player has no god is not determined |
| A fifth horse is given to "player eight", a name the game does not have (it knows players one to seven and neutral) | todo | our tree knows players one to seven and neutral too (`k_PlayerNamesStrs`, `src/Enums.h`); `CreateCreatureFromFile` does not check the name, so this creature gets a player number out of range; what the game does is not determined |
| All seven creatures start on one spot about 500 paces from your temple: the danger the title warns of | partial | made where the script puts them; nothing of their gods steers them |
| Each creature is sized to match your own | todo | `SET_COMPUTER_PLAYER_CREATURE_LIKE` is empty |
| Belief in players two, three and four is capped in every town (at 1, or at a half in one Egyptian town), yours not | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`) |

## Neutral towns

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 22 neutral towns of all nine tribes (Norse, Greek, Indian, Japanese, African, Celtic, Egyptian, Tibetan, Aztec), twelve of them with a wonder | done | `TownArchetype` and the buildings, wonders included (`AbodeArchetype`) (not checked against the file, which is not in our game folder) |
| Two towns share the number 15 (a Greek town and the Tibetan town in the middle) | todo | what the game does is not determined; our tree's lookup (`ecs::town_queries::FindTownWithID`) takes the first town with that number, the players' towns before the neutral ones |
| The Tibetan town in the middle has a graveyard, a football pitch, 21 houses, about 18 people, 20,000 food and 20,000 wood, and twelve more buildings planned (the Greek town given the same number has its own wonder) | partial | built with its stores, belief and planned buildings (`CreatePlannedAbode`); its pitch never plays: football is always off in multiplayer ([../../town/football.md](../../town/football.md#the-add-on-and-its-switch)) |
| The towns offer 102 miracles between them, from lightning and shields to the strongest storms and explosions | partial | the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 2,163 trees in 40 forests, nine big forests and 86 fireflies | partial | trees, forests and big forests are made (`TreeArchetype`, `ecs::CreateForest`, `BigForestArchetype`); `CREATE_FIRE_FLY` is empty; every map gets the nightly top-up instead; see [../../nature/fireflies.md](../../nature/fireflies.md) |
| 1,219 animals in 159 flocks, among them 15 tigers, 9 tortoises, 2 lions, bats and swallows as well as farm animals, wolves and birds | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| 61 fields and 40 fish farms | done | `FieldArchetype`, `FishFarmArchetype` |
| 79 lanterns, twelve of them singing stone bases | done | `StreetLanternArchetype` (`CREATE_STREET_LANTERN`), the singing stone bases included |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | todo | not determined what ends a skirmish, and our tree ends none; see ../skirmish.md |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | todo | no end of game in our tree; see ../skirmish.md and [../../story/losing_and_game_over.md](../../story/losing_and_game_over.md) |
