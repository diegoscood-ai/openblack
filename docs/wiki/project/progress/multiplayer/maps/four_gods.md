# Four Gods

Four gods in the four corners of a mountain-ringed land: you and three computer gods each start with the same small
Celtic town, with neutral Japanese and Indian towns between you and a Tibetan wonder town in the middle. Online the
same land is the four-player map "The four corners of Eden".

**Players:** 4 · **Landscape:** `Data/Landscape/Multi_Player/MPM_4P_1.lnd` · **Script:** `Scripts/Playgrounds/FourGods.txt` · **Mode:** both

**Progress: 21/35 done, 0 chosen, 7 partial, 0 inferred — 74%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#four-gods-loading) (its "Four Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists it under the game's own line "Defeat three gods to control this realm." rather than its file name | done | the skirmish box lists the playground folder's `.txt` lands and shows this one by HELP_TEXT_LAST_LINES_03 (`skirmish_box::TitleTextOf`, `gui::SkirmishBox`), as `SkirmishGameBox::InitControls` 0x544BD0; see ../skirmish.md; seen through the real input: the box shows the 18 lands and Start Game on the Four Gods row starts it, its line "Defeat three gods to control this realm." shown (refs/realinput/skirmish_four_gods.log lines 210, 212 and 218) |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | done | the skirmish box's Start Game ends the game and starts a new one on this land as a skirmish (`Game::EndGameForSkirmishBox`, `Game::AskForSkirmishLand`, `LoadMap` with `skirmish`), every script reset as at any game's start, as `GGame::StartPlaygroundGame` 0x555990; see ../skirmish.md; seen through the real input: the game ends for the box, the land is reset and FourGods.txt loads with its landscape MPM_4P_1 (refs/realinput/skirmish_four_gods.log lines 159, 216, 217 and 243) |
| No story script runs on a skirmish land | done | a skirmish loads no challenge program, so no story script starts (`Game::LoadStartLand`, `ecs::IsSkirmishGame`); a land opened from the debug menu is not a skirmish and keeps the story's scripts, as a land the original opens another way; not yet seen through the real input; done 2026-11-05: as [skirmish.md](../skirmish.md)'s done row (no challenge program in a skirmish), seen through the real input on this land: after its start no script can take a challenge picture (refs/realinput/skirmish_four_gods.log line 261) |
| The script gives no start point, so the camera starts zoomed onto your own temple | done | a game's start (the skirmish box's Start Game, or a new game) on a land whose script leaves `START_CAMERA_POS` at the origin, or gives none, puts the camera over the player's temple and flies it to the temple as the temple key's double tap flies (`start_camera::StartsAtTemple`, `ShortcutKeys::StartAtTemple`, `ZoomToPlaces::StartAtTemple`, from `Game::LoadStartLand`), as `GGame::Init` makes the camera at the citadel and calls `CameraModeNew3::ZoomToCitadel` with the citadel's distance and pitch for start modes 1, 3 and 4 (bw1-decomp `src/Black/Game.cpp:433`); raffclar's start camera refitted; tests `StartCamera.*`, `ZoomToPlaces.AGameStartFliesToTheTempleFromAFreshCamera`; seen through the real input (refs/realinput/skirmish_four_gods.log line 252, "The game starts with the camera at the player's temple") |
| The land's title "Four Gods" and its line "Battle four other gods for this realm" are its start message (the line miscounts: there are three other gods) | done | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` fill the land information box with them as the land loads (`gui::LandInfoBox`, `land_message::HelpTextOf`; tests `LandInfoBox.*`, `LandMessage.*`); see ../skirmish.md |

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
| Each home town believes in its god fully and offers heal, teleport, food, water and creature freeze | partial 90% | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); measured 2026-11-05: as the original's script, the town's belief is set and its miracles are held at its village centre, its worship site added at load ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds": a town's icons are kept, a change of hands is what is missing, which a home town at the start doesn't need); missing: its icons seen in a run |

## The computer gods

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Computer gods are switched on for players two, three and four | partial 70% | `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on (`FeatureScriptCommands::ToggleComputerPlayer`, `ComputerPlayerSystem::SwitchOn`); the mind makes no decisions of its own (see ../skirmish.md); measured 2026-11-05: `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on, as the original's command; missing: what being on does, its mind's own decisions ([skirmish_opponents.md](../../rival_gods/skirmish_opponents.md), [ai.md](../../rival_gods/ai.md)) |
| Player two has a horse, player three a tortoise and player four a zebra, all from the generic computer creature mind | partial 90% | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it; measured 2026-11-05 as [playground_scripts.md](../../scripts/playground_scripts.md)'s rows for these creatures (partial 90%): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; missing: the file's fatness field and fight health, which are not read, and its god steering it, which is the mind's ([ai.md](../../rival_gods/ai.md)) |
| All three creatures start bunched together on one edge of the land, between your temple and player two's, far from players three and four, whose creatures must cross the land to reach home | partial 90% | made where the script puts them; nothing sends them home; measured 2026-11-05: they are made where the script puts them, as the original's command; missing: seen in a run, and whether anything later calls them home is the minds' ([ai.md](../../rival_gods/ai.md)) |
| The creatures are sized to match your own | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42) |
| No personalities are loaded, so all three play with the default personality | pending | the minds keep their built-in personality (`ComputerMind`, `src/ECS/Components/ComputerMind.h`) but make no decisions of their own; see ../../rival_gods/ai.md |
| Every town's belief in any computer god is capped at 1, their own home towns included, while yours is not, a handicap in your favour | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`, `src/ECS/Town/TownBelief.cpp`) |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Four Japanese towns, one midway along each edge between two corners, each offering shield and physical shield | partial 40% | towns, houses and villagers are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the towns are built and hold their miracles, as the script; missing: the key part, a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%: a town changing player is not followed) |
| Four Indian fishing towns in an inner ring, each offering lightning, creature strength and creature invisibility | partial 40% | towns, houses, villagers and fish farms are built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the towns are built and hold their miracles, as the script; missing: the key part, a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%: a town changing player is not followed) |
| The Tibetan town in the middle has a wonder, a graveyard, fifteen houses and about 24 people, and offers lightning, the strongest lightning, beam explosion and the strongest beam explosion | partial 60% | built; the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the town is built with its stores, belief and miracles as the script; missing: a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%) |

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
| The land has no winning rule of its own; the skirmish's own rule applies | done | a skirmish ends by the skirmish's own rule: a god with no temple is out, and the game is decided once one god at most is left (`game_end::ProcessTurn`; the end box `end_game_box::Follow`); see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the land's script has no winning rule, and the skirmish's rules are done and seen in play on Two Gods ([skirmish.md](../skirmish.md) "A god among players one to four is out" and "When every other god is out", refs/story/evidence/skirmish_end_q1.log lines 38295 and 38298) |
| Losing while two or more gods are left shows "You have lost the game." with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | done | the end box opens with "You have lost the game.", Watch Game and Leave Game while two or more gods are left (`end_game_box::Follow`, `gui::EndGameBox`); Leave Game ends the skirmish for the skirmish box; see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the skirmish's done row ([skirmish.md](../skirmish.md) "When the player is out and two or more gods are left", test `EndGameBox.TheLocalGodOutWhileTheGameGoesOnCanWatch`) |

## Online as "The four corners of Eden"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "The four corners of Eden - 4 players", described as "Tactical action for 4 players. You've got to be quick and clever in this battle for belief...", with a thumbnail | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
| The online map file holds its own script and this same landscape; the game unpacks both, loads them and deletes them | pending | our tree does not read online map files; online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
| Online no god is a computer god and the script makes no creatures: each person brings their own | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
| Online every town's belief is capped evenly for all four gods (2 in the four home towns, 1 elsewhere) | pending | our tree has no network play; see ../network_play.md; the caps themselves work (`ecs::town_belief::SetCap`); online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
| Online the camera always starts on your own temple | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
| The winning conditions offered for it start at 1,000 food, 1,000 wood, 1,100 belief and up to 12 towns taken | pending | see ../multiplayer_rules.md; online only: its online version, `Online Maps/mpm_4p_1.map` (`localmaps.txt` line 3), is played only in a network game; offline the land is the `Scripts/Playgrounds/FourGods.txt` skirmish |
