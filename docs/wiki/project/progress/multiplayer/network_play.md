# Network play

Playing other people over a local network or the internet: finding and joining a game in a lobby, chatting, and keeping
every player's game in step. How a network game is won and lost, and its end box, are in
[../story/losing_and_game_over.md](../story/losing_and_game_over.md); the end box's Statistics tab and the figures sent
online are in [../interface/statistics.md](../interface/statistics.md).

**Progress: 0/19 done, 1 partial — 3%**

## Finding a game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Join Online Game on the menu | todo | the menu's Join Online Game button is there (`GameMenu::BuildMain`, `src/Gui/GameMenu.cpp`) but `Game::HandleInterfaceAction` only logs "This part of the menu is not available yet" |
| Multiplayer setup: choose a network and a lobby | todo | no network code in our tree |
| Games on the local network are found and joined | todo | no network code in our tree |
| Internet games are listed in the online lobby (GameSpy) | n/a | the original's lobby servers are gone |
| Logging in to the internet game with a name and password | n/a | the original's servers are gone |
| A channel can be created, joined, left and locked against newcomers (patch 1.1) | todo | no network code in our tree |
| The host can kick or ban a player from the channel | todo | no network code in our tree |
| Players see each other's ping | todo | no network code in our tree |

## The lobby

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Public chat and private messages between players | todo | no network code in our tree |
| Players pick their colours | todo | no network code in our tree |
| The host picks the island; players who lack it are told | todo | no network code in our tree |
| Missing islands are sent to players who lack them | todo | no network code in our tree |
| Teams: players change team, ask and invite others into theirs | todo | no network code in our tree; see [multiplayer_rules.md](multiplayer_rules.md) |
| Starting: every player says ready, the files are sent, the game starts together; the start can be cancelled | todo | no network code in our tree |

## In the game

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every player's commands are sent to all, and every game runs the same turns | todo | no network code in our tree |
| The game's random numbers are split into a shared stream and a local one | partial | `game_random` (`src/Common/GameRandom.h`, `Locator::gameRandom`) keeps the synced and the local seed and every caller picks its stream, as the original; nothing is synced over a network |
| Checksums detect a game out of step and the players wait to resync | todo | no network code in our tree; the per-turn state hash of the test runs is a verification tool, not a network checksum |
| The host moves to another player if the host leaves | todo | no network code in our tree |
| Players can talk to each other in the game | todo | no network code in our tree (unconfirmed whether the game sends voice or text) |
| Multiplayer help scripts run during network games | todo | the multiplayer help scripts are listed by the script editor (`src/Editor/Scripts/ScriptModel.cpp`) but nothing starts them; see ../engine/script_vm.md |
| Belief grows at the multiplayer rate | todo | the multiplayer belief speed scale is read into the info tables (`beliefSpeedScaleMultiPlayer`, `src/InfoConstants.h`) but nothing uses it, and nothing marks a game as multiplayer |
