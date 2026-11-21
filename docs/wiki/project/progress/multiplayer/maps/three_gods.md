# Three Gods

Three gods around a central mountain: you and two computer gods each start with a home town and three neutral towns
of your own tribe nearby, and a rich Tibetan town with a wonder sits in the middle. Each home lies under its own
climate, one of them freezing. Online the same land is the three-player map "King of the hill".

**Players:** 3 · **Landscape:** `Data/Landscape/Multi_Player/MPM_3P_1.lnd` · **Script:** `Scripts/Playgrounds/ThreeGods.txt` · **Mode:** both

**Progress: 27/40 done, 0 chosen, 5 partial, 0 inferred — 76%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#three-gods-loading) (its "Three Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat two gods to control this realm." (`HELP_TEXT_LAST_LINES_02`) rather than its file name | done | the skirmish box lists the playground folder's `.txt` lands and shows this one by HELP_TEXT_LAST_LINES_02 (`skirmish_box::TitleTextOf`, `gui::SkirmishBox`), as `SkirmishGameBox::InitControls` 0x544BD0; see ../skirmish.md; seen through the real input: the menu's Skirmish entry opens the box, which shows the 18 lands, and Start Game on the Three Gods row starts it (refs/realinput/skirmish_three_gods.log lines 210 to 213) |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | done | the skirmish box's Start Game ends the game and starts a new one on this land as a skirmish (`Game::EndGameForSkirmishBox`, `Game::AskForSkirmishLand`, `LoadMap` with `skirmish`), every script reset as at any game's start, as `GGame::StartPlaygroundGame` 0x555990; see ../skirmish.md; seen through the real input: the game ends for the box, the land is reset and ThreeGods.txt loads with its landscape MPM_3P_1 (refs/realinput/skirmish_three_gods.log lines 159, 216, 217 and 286) |
| No story script runs on a skirmish land | done | a skirmish reads the challenge program and starts only its auto-start script, `CreatureDevelopmentAutoRun`, as the original's skirmish start does (`GScript::Reset(1)` from `StartPlaygroundGame`, then `LoadBinary` of `Challenge.chl`; commit "Scripts: a skirmish's start reads the script program and starts its auto-start list", 2026-11-08), and no story script (`new_game_choice::RunsTheStory`) (`Game::LoadStartLand`, `ecs::IsSkirmishGame`); a land opened from the debug menu is not a skirmish and keeps the story's scripts, as a land the original opens another way; not yet seen through the real input; done 2026-11-05: as [skirmish.md](../skirmish.md)'s done row (no story script in a skirmish), seen through the real input on this land: after its start no script can take a challenge picture (refs/realinput/skirmish_three_gods.log line 300) |
| The camera starts looking at player three's home town, not yours (the script's start point is that town's centre) | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |
| The land's title "Three Gods" and its line "Battle three gods for this realm" are its start message (the line miscounts: there are two other gods) | done | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` fill the land information box with them as the land loads (`gui::LandInfoBox`, `land_message::HelpTextOf`; tests `LandInfoBox.*`, `LandMessage.*`); see ../skirmish.md |

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
| Each home town believes in its god at 1.5 and offers heal, teleport and water | partial 90% | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); measured 2026-11-05: as the original's script, the town's belief is set and its miracles are held at its village centre, its worship site added at load ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds": a town's icons are kept, a change of hands is what is missing, which a home town at the start doesn't need); missing: its icons seen in a run |
| Each home town has four to six buildings planned for its people to build | done | `CREATE_PLANNED_ABODE` (`FeatureScriptCommands::CreatePlannedAbode`): the town's planned list, read by the building sites |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two and three | partial 70% | `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on (`FeatureScriptCommands::ToggleComputerPlayer`, `ComputerPlayerSystem::SwitchOn`); the mind makes no decisions of its own (see ../skirmish.md); measured 2026-11-05: `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on, as the original's command; missing: what being on does, its mind's own decisions ([skirmish_opponents.md](../../rival_gods/skirmish_opponents.md), [ai.md](../../rival_gods/ai.md)) |
| Player two has a lion and player three a horse, both from the generic computer creature mind, standing on their temples | partial 90% | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it; measured 2026-11-05 as [playground_scripts.md](../../scripts/playground_scripts.md)'s rows for these creatures (partial 90%): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; missing: the file's fatness field and fight health, which are not read, and its god steering it, which is the mind's ([ai.md](../../rival_gods/ai.md)) |
| Both creatures are sized to match your own | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42) |
| No personalities are loaded, so both play with the default personality | pending | the minds keep their built-in personality (`ComputerMind`, `src/ECS/Components/ComputerMind.h`) but make no decisions of their own; see ../../rival_gods/ai.md |
| Every town's belief in either computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`, `src/ECS/Town/TownBelief.cpp`) |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each god has three neutral towns of its own tribe nearby, laid out alike: one offering shield and physical shield, one storm, flying flock, creature freeze and creature weakness, and a farther fishing town offering fire and lightning | partial 40% | towns, houses, villagers and fish farms are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the towns are built and hold their miracles, as the script; missing: the key part, a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%: a town changing player is not followed) |
| The Tibetan town in the middle has a wonder, a graveyard, nine houses, twelve fields and 20,000 food and 20,000 wood, believes in nobody at 1.5, and offers lightning, stronger lightning, beam explosion, storm and the strongest storm | partial 60% | built with its stores and belief; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the town is built with its stores, belief and miracles as the script; missing: a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%) |
| The Tibetan town has twelve more buildings planned, including a wonder | done | `CREATE_PLANNED_ABODE` (`FeatureScriptCommands::CreatePlannedAbode`): the town's planned list, read by the building sites |
| Six people stand in towns without a house | done | `CREATE_TOWN_VILLAGER` (`CreateVillagerInTown`, `src/LHScriptX/VillagerCommands.cpp`) |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 684 trees (beech, conifers, palms, oaks, birches, olives, pines) and six big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to 19 forests that foresters fell and replant | done | `CREATE_FOREST` (`ecs::CreateForest`): the forest the trees after it belong to |
| 43 fireflies | done | the land script's `CREATE_FIRE_FLY` hides 43 fireflies (`FeatureScriptCommands::CreateFireFly`, `PlaceFireFly`); seen through the real input in the skirmish (refs/realinput/skirmish_three_gods.log line 285, "43 in all"); every reward weight is zero on this map, so catching one gives nothing; see [../../nature/fireflies.md](../../nature/fireflies.md) |
| 374 animals in 71 flocks, mostly sheep (192), with pigs, pigeons, cows, gulls, crows and doves | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| 82 fenced wheat fields, 18 fish farms and one magic wood pot | done | `FieldArchetype`, `FishFarmArchetype`, `PotArchetype` |
| Limestone rocks and boulders and 48 street lanterns | done | `MobileStaticArchetype`, `StreetLanternArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | done | a skirmish ends by the skirmish's own rule: a god with no temple is out, and the game is decided once one god at most is left (`game_end::ProcessTurn`; the end box `end_game_box::Follow`); see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the land's script has no winning rule, and the skirmish's rules are done and seen in play on Two Gods ([skirmish.md](../skirmish.md) "A god among players one to four is out" and "When every other god is out", refs/story/evidence/skirmish_end_q1.log lines 38295 and 38298) |
| Losing while two or more gods are left shows "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`) with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | done | the end box opens with "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`), Watch Game and Leave Game while two or more gods are left (`end_game_box::Follow`, `gui::EndGameBox`); Leave Game ends the skirmish for the skirmish box; see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the skirmish's done row ([skirmish.md](../skirmish.md) "When the player is out and two or more gods are left", test `EndGameBox.TheLocalGodOutWhileTheGameGoesOnCanWatch`) |

## Online as "King of the hill"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "King of the hill - 3 players", described as "3 Player map designed to test your godly…" (`HELP_TEXT_DYK_RULES_10`), with a thumbnail | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | pending | our tree does not read online map files; online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| Online no god is a computer god and the script makes no creatures: each person brings their own | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| Online every town's belief is capped evenly for all three gods (2 in the three home towns, 1 elsewhere) | pending | our tree has no network play; see ../network_play.md; the caps themselves work (`ecs::town_belief::SetCap`); online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| Online the neutral Japanese storm town has a gathering point for worshippers | pending | our tree has no network play; see ../network_play.md; the gathering points themselves work (`SET_TOWN_CONGREGATION_POS`); online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| Online the camera always starts on your own temple | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
| The winning conditions offered for it start at 5,000 food, 5,000 wood, 1,600 belief and up to 12 towns taken | pending | see ../multiplayer_rules.md; online only: its online version, `Online Maps/mpm_3p_1.map` (`localmaps.txt` line 2), is played only in a network game; offline the land is the `Scripts/Playgrounds/ThreeGods.txt` skirmish |
