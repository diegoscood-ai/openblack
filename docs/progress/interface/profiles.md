# Profiles

Each person who plays has a player profile: their name, symbol, creature's name and tattoo, their saved games, their
controls and what help they have seen. The Players page of the options picks, makes and edits them.

**Progress: 1/17 done, 0 chosen, 10 partial, 1 inferred — 36%**

How the original does it, in our wiki: [Screen, text and read-outs](../../../interface/screen-and-readouts.md).

## The Players page

The exe's Players page (the profile editor) is a list of the players, an edit box for the creature's name, the
symbol's picture, five big buttons and four lines of text, under the options' tabs (read 2026-10-21); how its buttons
behave has not been read yet.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists the players, the current one picked | partial 60% | every kept profile, by name, the one played picked (`GameMenu::SetPlayers`, `Game::RefreshPlayersPage`; test `GameMenu.PlayersPickAProfileAndLeavingStartsTheGameAgain`); picking another plays as it at once and makes Back the Restart arrow, which asks first (Escape and the other tabs ask too): the list is the exe's, the picking and asking are not read against it |
| Shows the player's name, symbol and creature's name | done | `GameMenu::BuildPlayers` (`src/Gui/GameMenu.cpp`); test `GameMenu.PlayersNameTheirCreatureAndPickASymbol`; the exe's page has the creature's name box and the symbol's picture |
| The creature's name can be typed in | partial 60% | the edit box (a caret, a selection, the first click selecting all) keeps the name with the profile as the page is left, and the creature in play is renamed (`Game::NameCreatureAfterProfile`, `player_creature::SetName`; tests `GameMenu.PlayersEditsAreKeptAsThePageIsLeft`, `TextEdit.*`); the exe's page has the edit box; when the name is kept is not read |
| The player picks one of sixteen symbols, its picture ringed as it comes up | partial 50% | `SymbolPicture` in `src/Gui/Controls.cpp` (test `SymbolPicture.RingComesUpAndGoes`); the symbol is kept with the profile but the game's symbols in the world don't use it yet |
| Create New Player asks for a name and makes a profile | partial 50% | the new player's box over the game, on the swirling backdrop, with a Cancel (`Game::AskForAnotherPlayer`; tests `NewPlayerBox.*`, `GameMenu.PlayersStartANewGameOrMakeAPlayerAfterAsking`); the profile made is played (also the debug bar's Debug > Players); the exe's new profile box is not read against ours |
| Delete Player removes a profile, asking first | partial 50% | asks by name (HELP_TEXT_DIALOG_AERYOUSUREDELETE; test `GameMenu.PlayersAreDeletedAfterAskingByName`); deleting the one the game was started as plays the first left and must restart; with none left the new player's box comes up without Cancel. The creature's mind file stays. Not read against the exe |
| Edit Tattoo opens the tattoo editor for the creature | partial 30% | the tattoo editor itself is ported and opens in the creature's cave (`gui::OpenTattooEditorOnCreature`, see [../creature/creature_tattoos.md](../creature/creature_tattoos.md)); the Players page's button stays hidden: nothing calls `GameMenu::SetTattooAvailable` yet (test `GameMenu.PlayersSquareButtonsSpreadOutWithEditTattooOrWithout`) |
| Start Game starts a new game for the player from the first land | partial 50% | asks first (HELP_TEXT_DIALOG_ADDITION_20), then the game ends and a new one starts from the first land as the profile played (`Game::RestartForProfile`; test `GameMenu.PlayersStartANewGameOrMakeAPlayerAfterAsking`); not read against the exe |
| Picking another player during a game asks to restart | partial 50% | Restart asks (HELP_TEXT_DIALOG_RESTART_WARNING) or, when the profile started as was deleted, tells (HELP_TEXT_DIALOG_RESTART); test `GameMenu.PlayersPickAProfileAndLeavingStartsTheGameAgain`; a new game then starts from the first land, not from the profile's last automatic save (there are none yet); not read against the exe |

## What a profile keeps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Profiles are kept on disk, one folder each, and the last one played is picked at start | partial 80% | kept in `Profiles/Profiles.txt` in the program's working directory, the last one played picked at start (`PlayerProfileSystem`, `src/Common/PlayerProfiles.*`; a text file in place of the registry, [deviations.md](../../deviations.md)); each profile's folder is named by the game's key for its name, its creature's mind and physique are kept there (tests `PlayerProfiles.*`, `PlayerProfileSystem.*`); the exe picks the current profile named in the registry at start; the creature's file name rule is the exe's ([the player's creature](../../../creature/players-creature.md)); a run of the checks plays one profile made from the command line and keeps none |
| Each profile has its own saved games | pending | see ../engine/ for saving |
| Each profile has its own key bindings | pending | see [key_bindings.md](key_bindings.md) |
| The player's symbol is shown in the world (on the temple, the creature's pen and spells) | partial 30% | the town belief symbols and the creature's marks use the players' symbols (`src/Particles/TownBelief.cpp`, the tattoo atlas in `Game.cpp`), with the game's own symbols, not a profile's; the exe writes the profile's symbol into the local tattoo ([tattoos](../../../creature/tattoos.md)) |
| The tattoo and its colours are kept with the profile | pending | the exe keeps the creature's look in the profile's folder ([the player's creature](../../../creature/players-creature.md)) |
| What help the player has been given, and how often, is kept so it isn't repeated in the next game | pending | the help record (`src/Help/HelpProfile.cpp`) starts at 0 every run; see [help_system.md](help_system.md) |
| A picture of the creature is kept with the profile | inferred | (not found in the exe yet) |
| A web page about the creature is written from the profile for sharing | pending | the shipping exe writes an html page into the profile's folder with the creature's saved values ([the player's creature](../../../creature/players-creature.md)); see ../creature/lessons_and_help.md |
