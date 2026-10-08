# Firestorm

A hot, dry three-god land thick with fire miracles: each god starts with a large town, eighteen shrine-like empty
villages each hold one miracle dispenser in a ring round a rich Norse wonder town, and over two hundred one-shot
miracles, a fifth of them fire, lie on the ground. It is a copy of one of Lionhead's later online maps, which were only
ever offered from the online servers; its script sets up no computer gods.

**Players:** 3 · **Landscape:** `Data/Landscape/mpm_3p_2.lnd` · **Script:** `Scripts/Playgrounds/Firestorm.txt` · **Mode:** both

**Progress: 20/31 done, 6 partial — 74%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#firestorm-loading) (its "Firestorm" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Where it comes from

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script is the developers' three-player online map with only its landscape path and some villagers' ages changed; the game's own online condition list still names that map (`mpm_3p_2`) | n/a | history of the file; the original download is lost (see bwgame-service's notes on lost maps) |
| Neither the script nor its landscape is among the files the game installs; the skirmish box shows any script placed in its folder | n/a | how it reached this install is not part of the game |

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it by its file name, "Firestorm", and it has no start message | partial | the debug menu's "Playground Islands" lists it by its start message title, or by its file name when it has none (`src/Level.cpp`, `src/Debug/Gui.cpp`); no skirmish box |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | partial | `Game::LoadMap` loads it from the debug menu; see the next row |
| No story script runs on a skirmish land | todo | our tree starts the story's control script (`LandControlAll`) on the start land whatever it is, and leaves the story's scripts running when a land is opened from the debug menu |
| The camera starts at the script's start point, which lies off the land's edge over the sea rather than at your temple | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A low land about 1,450 by 1,600 paces: a third of it at sea level, the rest low ground and a central upland, crossed by four streams | done | `Game::LoadLandscape`; the streams from `CREATE_STREAM` and `CREATE_STREAM_POINT` |
| One hot climate over the whole land: about 25 degrees, with a long dry spell set and a steady wind | done | `CREATE_WEATHER_CLIMATE` and its rain, temperature and wind (`magic::map_script`, `src/Magic/Script/MapScriptWeather.cpp`); see ../../weather/ |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%) | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`) |
| Five patches of mist | done | `MistArchetype` (`CREATE_MIST`) |
| 79 standing features: Egyptian needles, Aztec statues and limestone pillars | done | `FeatureArchetype` (`CREATE_NEW_FEATURE`) |
| The music follows the nearest town's tribe: Celtic, Aztec and Japanese at the three homes, Norse in the middle | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project almost no influence (0.01 of normal) and gods 0.8 of theirs, so each god can act only around its temple and must win towns by belief | done | `SET_TOWN_INFLUENCE_MULTIPLIER` and `SET_PLAYER_INFLUENCE_MULTIPLIER`, kept in the map script globals and read by `src/ECS/Influence` |

## The three starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| You (player one) have a temple and a big Celtic town of 18 houses, a storehouse, four fields and about 45 villagers | done | towns, buildings and villagers as in [playground_scripts.md](../../scripts/playground_scripts.md) |
| Player two has much the same with an Aztec town beside its temple, player three with a Japanese town | done | as above |
| Each home store starts with 10,000 food and 10,000 wood | done | `AbodeArchetype` keeps the amounts |
| Each home town believes in its god at 2 and offers fire, the strongest fire, shield, physical shield, water, creature invisibility and creature itchiness | partial | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) |
| Each home town has its own five dispensers: a stronger fire, two fires, heal and shield | done | `CREATE_SPELL_DISPENSER` (`magic::script::CreateSpellDispenser`) |
| Each home town has a gathering point for worshippers | done | `SET_TOWN_CONGREGATION_POS`: the town's congregation position, read by `src/ECS/Town/TownQueries.cpp` |
| About 75 one-shot miracles lie round each home: fire, stronger fire, heal, shield, storm, lightning, teleport and creature miracles | done | `CREATE_ONE_SHOT_SPELL_PU` (`magic::script::CreateOneShotSpellPu`) |

## The other two gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script switches on no computer gods and makes no creatures, so in a skirmish players two and three keep their temples and towns but nobody plays them | partial | as in the game as far as found: the temples and towns are made and nothing plays them; whether the game's temple command itself makes them computer gods is not determined |
| In a network game, a temple nobody joins gets a fully grown creature of a random kind | todo | our tree has no network play; see ../../rival_gods/skirmish_opponents.md |

## The ring of shrines and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eighteen empty Celtic villages, with no houses or people, form a ring round the middle; each half-believes in nobody, has a gathering point and holds one dispenser (heal, creature bigness, weakness, anger, invisibility, compassion, strength, storm, lightning, teleport or fire) | done | the villages are made as towns with their belief, gathering points (`SET_TOWN_CONGREGATION_POS`) and dispensers (`CREATE_SPELL_DISPENSER` (`magic::script::CreateSpellDispenser`)) |
| The Norse town in the middle has a wonder, ten houses, about 26 people and 20,000 food and 20,000 wood, believes in nobody at a quarter, and offers heal, a stronger heal, shield, physical shield, water, creature freeze and creature strength | partial | built with its stores and belief; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)) |
| 18 pots of magic wood and 28 drinking places for animals | partial | the pots are made; each drinking place is kept as a `DrinkWaypoint` (`CREATE_DRINK_WAYPOINT`), but nothing reads it yet |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 304 trees (oaks, pines, palms, birches, olives, cedars, bushes) and nine big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to ten forests that foresters fell and replant | done | `CREATE_FOREST` (`ecs::CreateForest`): the forest the trees after it belong to |
| 70 animals in 12 flocks: cows and horses only | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| 16 fields | done | `FieldArchetype` |
| 149 rocks and boulders, three cuddly toys, magic mushrooms and toadstools to pick up, 32 lanterns and a street light | done | `MobileStaticArchetype`, `StreetLanternArchetype`, `MobileObjectArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With no computer gods playing, a skirmish here has no opponent to beat; the skirmish's own rule decides when it ends | todo | not determined what ends a skirmish, and our tree ends none; see ../skirmish.md |
| Online its winning conditions offered no building goals (houses, wonders, new towns and converts were switched off by default, with a note that a bug was still to be fixed), at most 50 villagers born, 11,000 to 30,000 food or wood, 2,100 to 20,000 belief and at most 2 towns taken | todo | the game's condition list; our tree has no winning conditions; see ../multiplayer_rules.md |

## Online

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Online this map was downloaded from the servers like the three shipped ones | n/a | the servers and this map's online file are gone; bwgame-service could offer the copy |
| Online every temple belongs to a person, and the camera always starts on your own temple | todo | our tree has no network play; see ../network_play.md |
