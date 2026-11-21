# Two Gods

The smallest of the game's own battle lands: you and one computer god face each other across a ridged, hilly land,
each with a home town and a second empty village, with six neutral towns between you. Online the same land is the
two-player map "Bombardment".

**Players:** 2 · **Landscape:** `Data/Landscape/Multi_Player/MPM_2P_1.lnd` · **Script:** `Scripts/Playgrounds/TwoGods.txt` · **Mode:** both

**Progress: 28/43 done, 0 chosen, 6 partial, 0 inferred — 76%**

What the script builds, object by object, is in
[playground_scripts.md](../../scripts/playground_scripts.md#two-gods-loading) (its "Two Gods" sections); this file
covers the land as a player meets it. How skirmish is set up is in [../skirmish.md](../skirmish.md); the computer gods
themselves are in [../../rival_gods/skirmish_opponents.md](../../rival_gods/skirmish_opponents.md).

## Choosing and starting it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The skirmish box lists every script in the playground folder; this one appears under the game's own line "Defeat another god to control this realm." (`HELP_TEXT_LAST_LINES_01`) rather than its file name | partial 90% | the skirmish box lists the playground folder's `.txt` lands and shows this one by HELP_TEXT_LAST_LINES_01 (`skirmish_box::TitleTextOf`, `gui::SkirmishBox`), as `SkirmishGameBox::InitControls` 0x544BD0; see ../skirmish.md; seen in part through the real input (pinned window, recorded mouse): the box lists the folder's 18 lands and its last row starts this land (refs/realinput/skirmish_menu_path.log lines 208 and 210); the line it is shown by stays to see on a shot; measured 2026-11-05: as the original's list (`SkirmishGameBox::InitControls` 0x544BD0 cuts each name by four characters and shows TwoGods by the first "Defeat" help text); missing: the line seen on a shot |
| Starting it clears the current land, resets the scripts and loads this script and its landscape | done | the skirmish box's Start Game ends the game and starts a new one on this land as a skirmish (`Game::EndGameForSkirmishBox`, `Game::AskForSkirmishLand`, `LoadMap` with `skirmish`), every script reset as at any game's start, as `GGame::StartPlaygroundGame` 0x555990; see ../skirmish.md; seen through the real input (pinned window, recorded mouse): refs/realinput/skirmish_menu_path.log line 211 (the skirmish starts on this land), line 214 (its land loads) and line 249 (no script that can take a challenge picture runs) |
| No story script runs on a skirmish land | done | a skirmish reads the challenge program and starts only its auto-start script, `CreatureDevelopmentAutoRun`, as the original's skirmish start does (`GScript::Reset(1)` from `StartPlaygroundGame`, then `LoadBinary` of `Challenge.chl`; commit "Scripts: a skirmish's start reads the script program and starts its auto-start list", 2026-11-08), and no story script (`new_game_choice::RunsTheStory`) (`Game::LoadStartLand`, `ecs::IsSkirmishGame`); a land opened from the debug menu is not a skirmish and keeps the story's scripts, as a land the original opens another way; seen through the real input (pinned window, recorded mouse): refs/realinput/skirmish_menu_path.log line 249, no script that can take a challenge picture runs after the skirmish's start |
| The camera starts on your home town's village centre | done | `START_CAMERA_POS` (`FeatureScriptCommands::StartCameraPos`) |
| The land's title "Two Gods" and its line "Battle another god for this realm" are its start message | done | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` fill the land information box with them as the land loads (`gui::LandInfoBox`, `land_message::HelpTextOf`; tests `LandInfoBox.*`, `LandMessage.*`); see ../skirmish.md |

## The land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A compact land about 1,600 by 1,100 paces, the smallest of the battle lands, of steep ridges and ramps (hills up to three quarters of the highest possible), with a sixth of it at sea level and the sea along one long side | done | `Game::LoadLandscape` |
| No climate is set, so the land keeps the game's default weather | pending | with no climate our weather queries give no rain and no wind (`src/ECS/Weather/Weather.h`); what the game's weather does on such a land is not determined |
| A day of 1,700 seconds with short nights (8.3%) and dawns and dusks (7%), the standard day | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`) |
| The music follows the tribe of the nearest town: Celtic around both home towns, Norse, African and Greek between | done | `src/Audio/GameMusic.cpp`; see ../../audio/music.md |
| Towns project 0.4 of their normal influence and gods their full influence | done | `SET_TOWN_INFLUENCE_MULTIPLIER` and `SET_PLAYER_INFLUENCE_MULTIPLIER`, kept in the map script globals and read by `src/ECS/Influence` |

## Your start (player one)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Your temple stands at one end of the land | done | `CitadelArchetype` (`CREATE_CITADEL`) |
| Your Celtic home town: eight houses, a crèche, a storehouse, a workshop and a village centre, six fields and 34 villagers | done | towns, buildings and villagers as in [playground_scripts.md](../../scripts/playground_scripts.md) |
| Its storehouse starts with 8,000 food and 3,000 wood | done | `AbodeArchetype` keeps the amounts |
| It believes in you fully and offers heal, teleport, water and a stronger water | partial 90% | belief set (`ecs::town_belief::SetBeliefInPlayer`) and the town holds its miracles (`magic::script::CreateNewTownSpell`); how far they reach its god's worship site icons is in [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md); measured 2026-11-05: as the original's script, the town's belief is set and its miracles are held at its village centre, its worship site added at load ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds": a town's icons are kept, a change of hands is what is missing, which a home town at the start doesn't need); missing: its icons seen in a run |
| A second town of yours, about 300 paces from the first: an Indian village centre with no houses, offering fire, nature, storm, a stronger storm and physical shield | partial 80% | the centre and its belief are made and the town holds its miracles (`CreateNewTownSpell`); see the row above; measured 2026-11-05: as the original's script, the town's belief is set and its miracles are held at its village centre, its worship site added at load ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds": a town's icons are kept, a change of hands is what is missing, which a home town at the start doesn't need); missing: its icons seen in a run; and whether this empty village gets its worship site before it has people is not read |
| Two worship sites by your temple, Indian and Celtic | done | `CREATE_WORSHIP_SITE` (`magic::script::CreateWorshipSite`): a built site at the player's temple |
| Water and flying flock one-shot miracles lying near your temple | done | `CREATE_ONE_SHOT_SPELL_PU` (`magic::script::CreateOneShotSpellPu`) |

## The computer god (player two)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A computer god is switched on for player two | partial 70% | `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on (`FeatureScriptCommands::ToggleComputerPlayer`, `ComputerPlayerSystem::SwitchOn`); the mind makes no decisions of its own (see ../skirmish.md); measured 2026-11-05: `TOGGLE_COMPUTER_PLAYER` adds the player and switches its mind on, as the original's command; missing: what being on does, its mind's own decisions ([skirmish_opponents.md](../../rival_gods/skirmish_opponents.md), [ai.md](../../rival_gods/ai.md)) |
| Its temple, Celtic home town and empty Indian village mirror yours at the far end of the land | done | as above |
| Its home town starts with the same stores (8,000 food, 3,000 wood) and the same water miracles | done | `AbodeArchetype` keeps the amounts; the town holds the same miracles (`CreateNewTownSpell`) |
| Its Indian village lacks the physical shield yours offers, the land's one difference between the two gods | done | each town holds the miracles its script gives it (`CreateNewTownSpell`) |
| It has a leopard from the generic computer creature mind, standing beside its home town | partial 90% | `CREATE_CREATURE_FROM_FILE` (`FeatureScriptCommands::CreateCreatureFromFile`, `CreatureArchetype`) with the mind loaded; it faces a fixed way and takes our starting size; nothing of its god steers it; measured 2026-11-05 as [playground_scripts.md](../../scripts/playground_scripts.md)'s rows for these creatures (partial 90%): the file's body, on the ground facing 0, the leashes known and stage 13, as the original; missing: the file's fatness field and fight health, which are not read, and its god steering it, which is the mind's ([ai.md](../../rival_gods/ai.md)) |
| The leopard is sized to match your own creature | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42) |
| No personality is loaded for it, so it plays with the computer god's default personality | pending | the mind keeps its built-in personality (`ComputerMind`, `src/ECS/Components/ComputerMind.h`) but makes no decisions of its own; see ../../rival_gods/ai.md |
| Every town's belief in the computer god is capped (at 1, or 2 in its own home town) while yours is not, a handicap in your favour | done | `SET_TOWN_BELIEF_CAP` (`ecs::town_belief::SetCap`, `src/ECS/Town/TownBelief.cpp`) |

## Neutral towns and the middle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Six neutral towns with half belief in nobody: nearer you an African town (7 houses) and a Norse fishing town (6, with fish farms); in the middle a Greek town (7); nearer the computer god two Norse towns (5, and 6 with fish farms) | done | towns, houses, villagers and fish farms (`FishFarmArchetype`) are built; see [playground_scripts.md](../../scripts/playground_scripts.md#two-gods-towns) |
| The neutral towns offer storms, flocks, fire, lightning, wood and shield miracles to whoever wins them | partial 40% | the towns hold their miracles (`magic::script::CreateNewTownSpell`), but a town changing hands does not yet pass them on to the winner's worship site ([../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md)); measured 2026-11-05: the towns are built and hold their miracles, as the script; missing: the key part, a town won passing its miracles on to the winner's worship site ([dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "Converting a town adds", partial 50%: a town changing player is not followed) |
| An Egyptian building site between the two gods: twelve scaffolds and a beam explosion dispenser, with no people | done | the dispenser is made (`CREATE_SPELL_DISPENSER` (`magic::script::CreateSpellDispenser`)), and the scaffolds of the site's town (`FeatureScriptCommands::CreateScaffold`; test `ScaffoldsTest.ALandFilesScaffoldIsMadeOfTheNumberedTownAtItsPlaceAngleAndSize`) |
| One beam explosion one-shot lying near the computer god's side | done | `CREATE_ONE_SHOT_SPELL_PU` (`magic::script::CreateOneShotSpellPu`) |
| Three pots of magic wood | done | `PotArchetype` |

## Nature and features

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 308 trees (mostly conifers and palms) and three big forests | done | `TreeArchetype`, `BigForestArchetype` |
| The trees belong to four forests that foresters fell and replant | done | `CREATE_FOREST` (`ecs::CreateForest`): the forest the trees after it belong to |
| 151 animals in 22 flocks: sheep, cows, pigs, horses, gulls, pigeons, crows and eight wolves | done | `CREATE_NEW_ANIMAL` (`AnimalArchetype`) and `CREATE_FLOCK`; how they behave is the animals' domain |
| Rocks, street lanterns and Champi mushrooms to pick up | done | `MobileStaticArchetype`, `StreetLanternArchetype`, `MobileObjectArchetype` |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land has no winning rule of its own; the skirmish's own rule applies | done | a skirmish ends by the skirmish's own rule: a god with no temple is out, and the game is decided once one god at most is left (`game_end::ProcessTurn`; the end box `end_game_box::Follow`); see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the land's script has no winning rule, and the skirmish's rules are done and seen in play on Two Gods ([skirmish.md](../skirmish.md) "A god among players one to four is out" and "When every other god is out", refs/story/evidence/skirmish_end_q1.log lines 38295 and 38298) |
| Losing while two or more gods are left shows "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`) with Watch Game (keep watching the other gods) and Leave Game (leave the skirmish) | done | the end box opens with "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`), Watch Game and Leave Game while two or more gods are left (`end_game_box::Follow`, `gui::EndGameBox`); Leave Game ends the skirmish for the skirmish box; see ../skirmish.md; not yet seen through the real input; done 2026-11-05: the skirmish's done row ([skirmish.md](../skirmish.md) "When the player is out and two or more gods are left", test `EndGameBox.TheLocalGodOutWhileTheGameGoesOnCanWatch`); on this two-god land the player's loss always leaves one god, so the end box offers Leave Game only |

## Online as "Bombardment"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Offered online as "Bombardment - 2 players", described as "Bombardment action for 2 players. Use the ramps…" (`HELP_TEXT_DYK_RULES_02`), with a thumbnail | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| The online map file holds its own script and this same landscape; the game unpacks both into the game folder, loads them and deletes them | pending | our tree does not read online map files; online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| Online neither god is a computer god and no creatures are made by the script: each person brings their own | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| Online every town's belief is capped evenly for both gods (2 in the two home towns, 1 elsewhere), unlike the one-sided caps of the skirmish | pending | our tree has no network play; see ../network_play.md; the caps themselves work (`ecs::town_belief::SetCap`); online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| Online the two empty Indian villages and the Egyptian building site have gathering points for worshippers | pending | our tree has no network play; see ../network_play.md; the gathering points themselves work (`SET_TOWN_CONGREGATION_POS`); online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| Online the camera always starts on your own temple | pending | our tree has no network play; see ../network_play.md; online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
| The winning conditions offered for it start at 9,000 food, 4,000 wood, 2,100 belief and up to 8 towns taken | pending | see ../multiplayer_rules.md; online only: its online version, `Online Maps/mpm_2p_1.map` (`localmaps.txt` line 1), is played only in a network game; offline the land is the `Scripts/Playgrounds/TwoGods.txt` skirmish |
