# Profiles

Each person who plays has a player profile: their name, symbol, creature's name and tattoo, their saved games, their
controls and what help they have seen. The Players page of the options picks, makes and edits them.

**Progress: 1/17 done, 4 partial — 18%**

How the original does it, in our wiki: [The hand and the interface](../../bw1-notes/hand-and-interface.md).

## The Players page

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lists the players, the current one picked | partial | the list is there (`GameMenu::BuildPlayers`) with only one name, `OPENBLACK_PLAYER_NAME` or the computer's login |
| Shows the player's name, symbol and creature's name | done | `GameMenu::BuildPlayers` (`src/Gui/GameMenu.cpp`) |
| The creature's name can be typed in | partial | the edit box works (`EditBox`); the name isn't given to the creature or kept |
| The player picks one of sixteen symbols, its picture ringed as it comes up | partial | `SymbolPicture` in `src/Gui/Controls.cpp`; the symbol chosen isn't used by the game |
| Create New Player asks for a name and makes a profile | todo | `GameMenu::Action::CreatePlayer` logs "This part of the menu is not available yet" |
| Delete Player removes a profile, asking first | todo | `Action::DeletePlayer` logs "This part of the menu is not available yet" |
| Edit Tattoo opens the tattoo editor for the creature | todo | `Action::EditTattoo` logs "This part of the menu is not available yet"; see ../creature/ for tattoos |
| Start Game starts a new game for the player from the first land | todo | `Action::StartNewGame` logs "This part of the menu is not available yet" |
| Picking another player during a game asks to restart | todo |  |

## What a profile keeps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Profiles are kept on disk, one folder each, and the last one played is picked at start | todo | there are no profiles on disk |
| Each profile has its own saved games | todo | see ../engine/ for saving |
| Each profile has its own key bindings | todo | see [key_bindings.md](key_bindings.md) |
| The player's symbol is shown in the world (on the temple, the creature's pen and spells) | partial | the town belief symbols and the creature's marks use the players' symbols (`src/Particles/TownBelief.cpp`, the tattoo atlas in `Game.cpp`), with the game's own symbols, not a profile's |
| The tattoo and its colours are kept with the profile | todo |  |
| What help the player has been given, and how often, is kept so it isn't repeated in the next game | todo | the help record (`src/Help/HelpProfile.cpp`) starts at 0 every run; see [help_system.md](help_system.md) |
| A picture of the creature is kept with the profile | todo |  |
| A web page about the creature is written from the profile for sharing | todo | (unconfirmed whether this is part of the shipping game or only of the creature upload tool) |
| Creatures can be uploaded and downloaded online | n/a | the service no longer exists |
