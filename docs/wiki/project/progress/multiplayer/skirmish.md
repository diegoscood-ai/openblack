# Skirmish

A game against computer gods on one of the skirmish lands, started from the in-game menu (or a launch switch) without
a network. The lands are the land scripts in `Scripts/Playgrounds`. The setup box only picks the land: everything else
(opponents, their creatures, towns, miracles) comes from that land's script. Skirmish has been in the game since 1.00
(the box is in the 1.00 program). Patch 1.2's winning conditions and time limit apply only to network games. It is not
the Gods' Playground, the tutorial island the story reaches with F2 (see
[../story/tutorial_island.md](../story/tutorial_island.md)).

**Progress: 6/54 done, 0 chosen, 12 partial, 0 inferred — 11%**

See [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) for the computer gods,
[multiplayer_rules.md](multiplayer_rules.md) for the network rules and
[../scripts/playground_scripts.md](../scripts/playground_scripts.md) for what each land script builds.

On an unlanded branch, `fix/skirmish` (tip 439647ddc, 8 commits on joint290), each its own commit and none landed:
a god with no temple is out and its mind stops; the end box, with the world stopping once a skirmish is decided; the
skirmish box's Start Skirmish Game listing the playground lands and starting one; the land information box and the
skirmish's creature from the profile; Leave Skirmish Game asking first; the game's readers of a skirmish asking one
flag; a creature's removal not making a hand that holds it drop it; and the skirmish's start camera read from the
original. The rows below say what joint290 does, and change with each commit as it lands.

## Reaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu Escape brings up has a Start Skirmish Game button, second of five | partial | the button is there, second of five (`GameMenu::BuildMain`, `src/Gui/GameMenu.cpp`), but `Game::HandleInterfaceAction` only logs "This part of the menu is not available yet"; see [../interface/main_menu.md](../interface/main_menu.md) |
| Start Skirmish Game ends the current game at once, without asking, and opens the skirmish box | pending | no skirmish box in our tree |
| Leaving a story game this way quick-saves the story to its reserved slot, as any exit from the story does (except from the Gods' Playground island) | pending | our tree cannot save a game yet; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The Start Skirmish Game button is greyed out during a network game | pending | no network game in our tree |
| During a skirmish the button reads Leave Skirmish Game, and Join Online Game is greyed out | pending | the labels are noted in `GameMenu::BuildMain`, not done |
| During a skirmish (and a network game) the menu's five buttons are spaced further apart and start lower | pending | the menu always uses the story's button layout (`GetButtonRect`, `src/Gui/GameMenu.cpp`) |
| The `SKIRMISH` launch switch opens the skirmish box straight away; `NEWGAME` overrides it, and it overrides `MULTIPLAYER` | pending | our tree has none of these switches; `--start-level` loads a land directly (`src/main.cpp`); see [../easter_eggs/launch_switches_and_files.md](../easter_eggs/launch_switches_and_files.md) |
| With no player profile yet, the new-player box comes first and a new story game starts instead of the skirmish | pending | profiles and the new player's box exist (`src/Common/PlayerProfiles.h`, `Game::AskForAnotherPlayer`), but there is no skirmish switch or box to put it before; see [../interface/profiles.md](../interface/profiles.md) |

## The skirmish box

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A box titled "Choose a map for skirmish game." with a list of lands, Back on the left and Start Game on the right | pending | no skirmish box in our tree |
| The list holds every `.txt` file in `Scripts/Playgrounds`, in the order the folder gives them, so an added land shows too | partial | our tree reads the same folder into its levels (`Game.cpp`, as `Level::LandType::Skirmish`) and lists them, sorted and titled by each land's start message, in the debug menu's "Playground Islands" (`src/Debug/Gui.cpp`) |
| A land is shown by its file name without `.txt`. The exceptions are Two, Three and Four Gods, which read "Defeat another god to control this realm.", "Defeat two gods to control this realm." and "Defeat three gods to control this realm." | pending | the debug menu titles each land by its start message (`src/Level.cpp`), not by its file name; no skirmish box |
| Start Game is greyed out until a land is picked | pending | no skirmish box in our tree |
| There are no other options: no opponent count, difficulty, teams, colours, tribes, creature or time limit | pending | no skirmish box in our tree; all of these come from the land script; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| Start Game loads `Scripts/Playgrounds/<name>.txt` | partial | the debug menu loads `Scripts/Playgrounds/<name>.txt` through `Game::LoadMap`; no skirmish box in our tree |
| Back closes the box and returns to the story: its automatic save is reloaded and the main menu shown | pending | no skirmish box in our tree; a saved game loads only from the debug bar (`Game::LoadSavedGame`; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md)) |

## Starting a skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The usual loading screen is shown while the land loads | done | `Game::LoadMap` shows the tips screen while any land loads, a Playgrounds land included (`Game::BeginLoadingScreen`, `gui::LoadingScreen`); tests `LoadingScreen.*`; see [../video/bink_videos.md](../video/bink_videos.md#the-loading-screen) |
| Every script is reset first. The story's new-game steps (its control script, the challenge and save-room files, the skip-tutorial questions) are not run | pending | `Game::Run` starts the story's control script (`LandControlAll`) only on Land 1 with the program read, as the original (test `NewGameChoice.TheStoryStartsOnlyWithItsProgramReadAndOnLand1`), but still asks the skip-tutorial question on any land; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| The land script is read and builds the land, its towns, temples, creatures and computer gods | partial | the debug menu or `--start-level` loads it through `Game::LoadMap`; an unknown or misspelt command is skipped and the rest loads ([../scripts/land_script_commands.md](../scripts/land_script_commands.md)); `TOGGLE_COMPUTER_PLAYER` switches a computer god's mind on (`ComputerPlayerSystem::SwitchOn`), but the mind makes no decisions of its own: its turn runs only the actions scripts queue (`ComputerPlayerSystem::ProcessTurn`) |
| A land whose start camera is left at the origin (Four Gods) starts with the camera zoomed onto the player's temple | pending | `FeatureScriptCommands::StartCameraPos` always looks at the point given |
| When the land script reaches its start message, the land info box pops up. Its heading is the land's title, or for a number from 2 to 4 the matching "Defeat ... to control this realm." line; the description lines follow | pending | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty; only the debug land list reads them, for its title and tooltip (`src/Level.cpp`). Firestorm has no start message, so it shows no box |
| The player's own creature is loaded from their profile's creature file and placed at their temple's creature spot | pending | a profile keeps its creature file (`Profile::creatureFile`, `src/Common/PlayerProfiles.h`; `--creature-file` for the checks) and `player_creature::LoadMyCreature` loads it, but nothing does it at a skirmish start; see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The player starts with the alignment kept in their profile (limited to fully good or fully evil) | pending | our profiles keep no alignment (`src/Common/PlayerProfiles.h`); see [../interface/profiles.md](../interface/profiles.md) |
| The game's random numbers start from the same fixed seed in every game, so the same moves play out the same way (unconfirmed whether anything reseeds it later) | done | `Game::LoadMap` resets both seeds (`game_random::Reset`) for a land of the Playgrounds folder, as the original's skirmish start does ([engine-math](../../../engine/random.md#random-numbers)) |
| The player's statistics start afresh | partial | the map's clear puts every player's game statistics back to their start on every land load, a Playgrounds land included, keeping the two histories (`game_stats::ClearAll` from `Game::LoadMap`; `test/test_game_stats.cpp`); whether a skirmish start also clears the histories is not checked; see [../interface/statistics.md](../interface/statistics.md) |

## The lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| [Two Gods](maps/two_gods.md): two temples, the player and one computer god | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; the computer gods' minds make no decisions of their own (`ComputerPlayerSystem::ProcessTurn` runs only what scripts queue) |
| [Three Gods](maps/three_gods.md): three temples, two computer gods | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; the computer gods' minds make no decisions of their own (`ComputerPlayerSystem::ProcessTurn` runs only what scripts queue) |
| [Four Gods](maps/four_gods.md): four temples, three computer gods (its description says "Battle four other gods") | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; the computer gods' minds make no decisions of their own (`ComputerPlayerSystem::ProcessTurn` runs only what scripts queue) |
| [Firestorm](maps/firestorm.md): three temples, but no computer god is switched on, so the other two temples stand idle (as far as found) | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; the computer gods' minds make no decisions of their own (`ComputerPlayerSystem::ProcessTurn` runs only what scripts queue); like the game, nothing plays its other temples (its script is the game's online map `Online Maps/mpm_3p_2.map`) |
| Each land's towns belong to their players, with their tribe | done | towns load with their owner and tribe (`TownArchetype`, `CREATE_TOWN`); see [../scripts/playground_scripts.md](../scripts/playground_scripts.md) |
| Each land sets its players' and towns' influence multipliers | done | `SET_PLAYER_INFLUENCE_MULTIPLIER` and `SET_TOWN_INFLUENCE_MULTIPLIER` are kept in the map script globals and read by `src/ECS/Influence` |
| Each land sets its own day length | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`); see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |

## Playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only miracles on offer are those the land gives: town miracles, dispensers, one-shot seeds and firefly rewards | partial | the land script's town miracles, dispensers and one-shot seeds are made ([../scripts/playground_scripts.md](../scripts/playground_scripts.md)); fireflies come from the nightly top-up and the land's reward table ([../nature/fireflies.md](../nature/fireflies.md)); but the story's control script also runs on the land |
| No challenge or story script runs, so there are no scrolls, rewards or story advisor lines | pending | the story's control script (`LandControlAll`) no longer starts on a skirmish land (`Game.cpp`, only on Land 1, as the original), but a land opened from the debug menu keeps the story's scripts running |
| Whether the advisors still give their general hints on a skirmish land | pending | undetermined in the game; our guidance treats every land but Land 1 alike (`guidance::PlayNow`, `src/Audio/Services/Guidance.cpp`; nothing sets its playground query) |
| The story's game-over script (when the temple's heart is destroyed) doesn't run in a skirmish | pending | the story's scripts run on a skirmish land too |
| A skirmish runs on one machine, so pausing, the menu and game speed work as in the story | partial | pausing and the in-game menu work on any land (`game_clock::Pause`, `src/Gui/GameMenu.cpp`); see [../interface/main_menu.md](../interface/main_menu.md) |
| The automatic save still runs (it skips only network games and the Gods' Playground island). A save records that it is a skirmish, so a loaded one carries on as a skirmish | pending | our tree has no autosave and cannot save a game yet; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| About once a minute the player's alignment is written back to their profile, so a skirmish changes the alignment the story and the next skirmish start from | pending | our profiles keep no alignment; see [../interface/profiles.md](../interface/profiles.md) |
| Every ten turns each player's share of a running total is recorded for the statistics | pending | see [../interface/statistics.md](../interface/statistics.md) |
| There are no winning conditions and no time limit: patch 1.2 sets them up only for network games, and the time-limit check is skipped in a skirmish | done | our tree has no winning conditions or time limit on any land, which matches a skirmish; see [multiplayer_rules.md](multiplayer_rules.md) |
| Music | partial | the music engine plays on a skirmish land as on any other (`src/Audio/GameMusic.cpp`); what the original plays there is undetermined |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god among players one to four is out of the game once it has no temple (how a temple is lost is unconfirmed here) | pending | no rule in our tree takes a god out of the game or ends a skirmish; how a temple is damaged and destroyed: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A god that is out stops thinking and its creature is taken off the land | pending | no rule in our tree takes a god out of the game or ends a skirmish; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| When another god is out, "<name> is out of the game." is shown | pending | no rule in our tree takes a god out of the game or ends a skirmish |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" | pending | no rule in our tree takes a god out of the game or ends a skirmish |
| When the player is out and only one god is left, the game ends and the end box says "You have lost the game." | pending | no rule in our tree takes a god out of the game or ends a skirmish |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game; Watch Game closes it and play goes on, so the player can watch | pending | no rule in our tree takes a god out of the game or ends a skirmish; see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| The end box is edged in the player's colour and has tabs for the statistics and (unconfirmed) the winning conditions | pending | no end box; see [../interface/statistics.md](../interface/statistics.md) |
| Nothing is uploaded and no points are given (that is only for internet games) | pending | no rule in our tree takes a god out of the game or ends a skirmish; our tree has no online code; see [online_services.md](online_services.md) |
| Whether a skirmish can end without a winner (Firestorm's idle temples) | pending | undetermined |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Leave Skirmish Game asks "This will leave the current Skirmish game. Do you wish to do this?" | pending | the menu has no Leave Skirmish Game button |
| Yes ends the skirmish and opens the skirmish box again, for another land or Back to the story | pending | no way to leave a skirmish in our tree |
| Leaving a skirmish, or quitting the game from one, skips the quick-save the story makes on exit | pending | our tree cannot save a game yet; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The player's creature is not saved back to their profile at the end of a skirmish (nothing found does it) | pending | no skirmish end in our tree |
