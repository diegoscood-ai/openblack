# Skirmish

A game against computer gods on one of the skirmish lands, started from the in-game menu (or a launch switch) without
a network. The lands are the land scripts in `Scripts/Playgrounds`. The setup box only picks the land: everything else
(opponents, their creatures, towns, miracles) comes from that land's script. Skirmish has been in the game since 1.00
(the box is in the 1.00 program). Patch 1.2's winning conditions and time limit apply only to network games. It is not
the Gods' Playground, the tutorial island the story reaches with F2 (see
[../story/tutorial_island.md](../story/tutorial_island.md)).

**Progress: 5/57 done, 14 partial — 21%**

See [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) for the computer gods,
[multiplayer_rules.md](multiplayer_rules.md) for the network rules and
[../scripts/playground_scripts.md](../scripts/playground_scripts.md) for what each land script builds.

## Reaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu Escape brings up has a Start Skirmish Game button, second of five | partial | the button is there, second of five (`GameMenu::BuildMain`, `src/Gui/GameMenu.cpp`), but `Game::HandleInterfaceAction` only logs "This part of the menu is not available yet"; see [../interface/main_menu.md](../interface/main_menu.md) |
| Start Skirmish Game ends the current game at once, without asking, and opens the skirmish box | todo | no skirmish box in our tree |
| Leaving a story game this way quick-saves the story to its reserved slot, as any exit from the story does (except from the Gods' Playground island) | todo | no saved games in our tree; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The Start Skirmish Game button is greyed out during a network game | todo | no network game in our tree |
| During a skirmish the button reads Leave Skirmish Game, and Join Online Game is greyed out | todo | the labels are noted in `GameMenu::BuildMain`, not done |
| During a skirmish (and a network game) the menu's five buttons are spaced further apart and start lower | todo | the menu always uses the story's button layout (`GetButtonRect`, `src/Gui/GameMenu.cpp`) |
| The `SKIRMISH` launch switch opens the skirmish box straight away; `NEWGAME` overrides it, and it overrides `MULTIPLAYER` | todo | our tree has none of these switches; `--start-level` loads a land directly (`src/main.cpp`); see [../easter_eggs/launch_switches_and_files.md](../easter_eggs/launch_switches_and_files.md) |
| With no player profile yet, the new-player box comes first and a new story game starts instead of the skirmish | todo | no player profiles in our tree; see [../interface/profiles.md](../interface/profiles.md) |

## The skirmish box

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A box titled "Choose a map for skirmish game." with a list of lands, Back on the left and Start Game on the right | todo | no skirmish box in our tree |
| The list holds every `.txt` file in `Scripts/Playgrounds`, in the order the folder gives them, so an added land shows too | partial | our tree reads the same folder into its levels (`Game.cpp`, as `Level::LandType::Skirmish`) and lists them, sorted and titled by each land's start message, in the debug menu's "Playground Islands" (`src/Debug/Gui.cpp`) |
| A land is shown by its file name without `.txt`. The exceptions are Two, Three and Four Gods, which read "Defeat another god to control this realm.", "Defeat two gods to control this realm." and "Defeat three gods to control this realm." | todo | the debug menu titles each land by its start message (`src/Level.cpp`), not by its file name; no skirmish box. `construct.txt` (not a game file) would show as "construct"; see [maps/construct.md](maps/construct.md) |
| Start Game is greyed out until a land is picked | todo | no skirmish box in our tree |
| There are no other options: no opponent count, difficulty, teams, colours, tribes, creature or time limit | todo | no skirmish box in our tree; all of these come from the land script; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| Start Game loads `Scripts/Playgrounds/<name>.txt` | partial | the debug menu loads `Scripts/Playgrounds/<name>.txt` through `Game::LoadMap`; no skirmish box in our tree |
| Back closes the box and returns to the story: its automatic save is reloaded and the main menu shown | todo | no skirmish box and no saved games in our tree |

## Starting a skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The usual loading screen is shown while the land loads | todo | our tree has no loading screen (noted in `src/Video/VideoPlayer.cpp`) |
| Every script is reset first. The story's new-game steps (its control script, the challenge and save-room files, the skip-tutorial questions) are not run | todo | `Game::Run` starts the story's control script (`LandControlAll`) and the skip-tutorial question on any land; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| The land script is read and builds the land, its towns, temples, creatures and computer gods | partial | the debug menu or `--start-level` loads it through `Game::LoadMap`; an unknown or misspelt command is skipped and the rest loads ([../scripts/land_script_commands.md](../scripts/land_script_commands.md)); the computer gods get no mind |
| A land whose start camera is left at the origin (Four Gods, Death Comes To Those That Wait) starts with the camera zoomed onto the player's temple | todo | `FeatureScriptCommands::StartCameraPos` always looks at the point given |
| When the land script reaches its start message, the land info box pops up. Its heading is the land's title, or for a number from 2 to 4 the matching "Defeat ... to control this realm." line; the description lines follow | todo | `START_GAME_MESSAGE` and `ADD_GAME_MESSAGE_LINE` are empty; only the debug land list reads them, for its title and tooltip (`src/Level.cpp`). Island Wars and Firestorm have no start message, so they show no box |
| The player's own creature is loaded from their profile's creature file and placed at their temple's creature spot | todo | no profiles; `LOAD_MY_CREATURE` can load a mind file given with `--creature-file`, but nothing does it at a skirmish start; see [../creature/saves_and_files.md](../creature/saves_and_files.md) |
| The player starts with the alignment kept in their profile (limited to fully good or fully evil) | todo | no profiles; see [../interface/profiles.md](../interface/profiles.md) |
| The game's random numbers start from the same fixed seed in every game, so the same moves play out the same way (unconfirmed whether anything reseeds it later) | done | `Game::LoadMap` resets both seeds (`game_random::Reset`) for a land of the Playgrounds folder, as the original's skirmish start does ([engine-math](../../bw1-notes/engine-math.md#random-numbers-game_random)) |
| The player's statistics start afresh | todo | no statistics reset at a land start; see [../interface/statistics.md](../interface/statistics.md) |

## The lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| [Two Gods](maps/two_gods.md): two temples, the player and one computer god | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods |
| [Three Gods](maps/three_gods.md): three temples, two computer gods | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods |
| [Four Gods](maps/four_gods.md): four temples, three computer gods (its description says "Battle four other gods") | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods |
| [Island Wars](maps/island_wars.md): four temples, three computer gods with creatures from Nemesis's mind file | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods (the file is not in our game folder) |
| [Firestorm](maps/firestorm.md): three temples, but no computer god is switched on, so the other two temples stand idle (as far as found) | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods; like the game, nothing plays its other temples (the file is not in our game folder) |
| [Death Comes To Those That Wait](maps/death_comes_to_those_that_wait.md), "By Fat Omen": temples for players one, three and four, two computer gods, and creatures for the temple-less players two and five to eight | partial | loads from the debug menu's "Playground Islands" (`Game::LoadMap`, `src/Debug/Gui.cpp`) with its towns, temples and creatures; no computer god mind drives its gods; the misspelt lines are skipped as in the game, but the line missing its bracket is skipped where the game reads it |
| [Construct](maps/construct.md): openblack's own test land (added in 2021), not one of the game's, with only the player's temple; copied into the playground folder, the box would list it like any other land | partial | loads from the debug menu or with `-s Playgrounds/construct.txt` like the others; see [../easter_eggs/unused_content.md](../easter_eggs/unused_content.md) |
| Each land's towns belong to their players, with their tribe | done | towns load with their owner and tribe (`TownArchetype`, `CREATE_TOWN`); see [../scripts/playground_scripts.md](../scripts/playground_scripts.md) |
| Each land sets its players' and towns' influence multipliers | done | `SET_PLAYER_INFLUENCE_MULTIPLIER` and `SET_TOWN_INFLUENCE_MULTIPLIER` are kept in the map script globals and read by `src/ECS/Influence` |
| Each land sets its own day length | done | `SET_NIGHTTIME` (`DayNightClock::SetCycleFromLand`); see [../scripts/land_script_commands.md](../scripts/land_script_commands.md) |

## Playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The only miracles on offer are those the land gives: town miracles, dispensers, one-shot seeds and firefly rewards | partial | the land script's town miracles, dispensers and one-shot seeds are made ([../scripts/playground_scripts.md](../scripts/playground_scripts.md)); fireflies come from the nightly top-up and the land's reward table ([../nature/fireflies.md](../nature/fireflies.md)); but the story's control script also runs on the land |
| No challenge or story script runs, so there are no scrolls, rewards or story advisor lines | todo | the story's control script (`LandControlAll`) starts on every land (`Game.cpp`) |
| Whether the advisors still give their general hints on a skirmish land | todo | undetermined in the game; our guidance treats every land but Land 1 alike (`guidance::PlayNow`, `src/Audio/Services/Guidance.cpp`; nothing sets its playground query) |
| The story's game-over script (when the temple's heart is destroyed) doesn't run in a skirmish | todo | the story's scripts run on a skirmish land too |
| A skirmish runs on one machine, so pausing, the menu and game speed work as in the story | partial | pausing and the in-game menu work on any land (`game_clock::Pause`, `src/Gui/GameMenu.cpp`); see [../interface/main_menu.md](../interface/main_menu.md) |
| The automatic save still runs (it skips only network games and the Gods' Playground island). A save records that it is a skirmish, so a loaded one carries on as a skirmish | todo | no saved games in our tree; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| About once a minute the player's alignment is written back to their profile, so a skirmish changes the alignment the story and the next skirmish start from | todo | no profiles; see [../interface/profiles.md](../interface/profiles.md) |
| Every ten turns each player's share of a running total is recorded for the statistics | todo | see [../interface/statistics.md](../interface/statistics.md) |
| There are no winning conditions and no time limit: patch 1.2 sets them up only for network games, and the time-limit check is skipped in a skirmish | done | our tree has no winning conditions or time limit on any land, which matches a skirmish; see [multiplayer_rules.md](multiplayer_rules.md) |
| Music | partial | the music engine plays on a skirmish land as on any other (`src/Audio/GameMusic.cpp`); what the original plays there is undetermined |

## Winning and losing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god among players one to four is out of the game once it has no temple (how a temple is lost is unconfirmed here) | todo | no rule in our tree takes a god out of the game or ends a skirmish; how a temple is damaged and destroyed: [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| A god that is out stops thinking and its creature is taken off the land | todo | no rule in our tree takes a god out of the game or ends a skirmish; see [../rival_gods/skirmish_opponents.md](../rival_gods/skirmish_opponents.md) |
| When another god is out, "<name> is out of the game." is shown | todo | no rule in our tree takes a god out of the game or ends a skirmish |
| When every other god is out, the game ends and the end box says "Congratulations! You've won the game!" | todo | no rule in our tree takes a god out of the game or ends a skirmish |
| When the player is out and only one god is left, the game ends and the end box says "You have lost the game." | todo | no rule in our tree takes a god out of the game or ends a skirmish |
| When the player is out and two or more gods are left, the end box says "You have lost the game." with Watch Game and Leave Game; Watch Game closes it and play goes on, so the player can watch | todo | no rule in our tree takes a god out of the game or ends a skirmish; see [../story/losing_and_game_over.md](../story/losing_and_game_over.md) |
| The end box is edged in the player's colour and has tabs for the statistics and (unconfirmed) the winning conditions | todo | no end box; see [../interface/statistics.md](../interface/statistics.md) |
| Nothing is uploaded and no points are given (that is only for internet games) | todo | no rule in our tree takes a god out of the game or ends a skirmish; our tree has no online code; see [online_services.md](online_services.md) |
| Whether a skirmish can end without a winner (Firestorm's idle temples, players with no temple) | todo | undetermined |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Leave Skirmish Game asks "This will leave the current Skirmish game. Do you wish to do this?" | todo | the menu has no Leave Skirmish Game button |
| Yes ends the skirmish and opens the skirmish box again, for another land or Back to the story | todo | no way to leave a skirmish in our tree |
| Leaving a skirmish, or quitting the game from one, skips the quick-save the story makes on exit | todo | no saves in our tree; see [../engine/saving_and_loading.md](../engine/saving_and_loading.md) |
| The player's creature is not saved back to their profile at the end of a skirmish (nothing found does it) | todo | no skirmish end and no profiles in our tree |
