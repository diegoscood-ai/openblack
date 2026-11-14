# The skirmish

## Summary

A skirmish is a game on one machine against the computer gods of a land of the `Scripts/Playgrounds` folder, started
from the skirmish box. The game marks it as a skirmish (the byte g_game +0x205A0C), which changes the start (no story
script, the land information box), the menu (Leave Skirmish Game) and the end of the game (a god with no temple is
out, and the game ends when one god at most is left). openblack takes raffclar's pieces of it (his pull requests
numbers 943, 944, 945 and 1009) where they match the original, refitted onto its own systems. Status by row:
[skirmish](../project/progress/multiplayer/skirmish.md).

## The original

### The skirmish game

- **Set** only by the start mode 4 (the skirmish box's Start Game): `GGame::Init` runs
  `GGame::StartPlaygroundGame` 0x555990 (the scripts reset, then `LoadMap` of the land) and then writes 1 to +0x205A0C
  (0x54F75E). Every game's start clears it first (0x54F6CE, and `GGame::ClearVariables` 0x54BF20). A save keeps it
  (`GGame::Save` 0x554090, `GGame::Load` 0x554830). A land opened another way is not a skirmish.
- **Read by** `GPlayer::ProcessPlayers` 0x649A20 (the end of the game, below), `GameStats::PlayerLostTheGame`
  0x56A6D0, `GGame::EndTurn` 0x54E960 (no story game-over script in a skirmish), `GGame::Loop` 0x54CF20,
  `GGame::ProcessKey` 0x63EF20, `MainMenu::InitControls` 0x53FBA0, `GComputerPlayer::InGameInit` 0x657060, the
  temple rooms' `IsAvailable`, `GAudio::PlayFightMusic` 0x427590, `GGuidance::PlayNow` 0x71AF50 and
  `Villager::SetStateSpeed` 0x753760.

### The skirmish box

- **Reached** by the menu's Start Skirmish Game: the game mode 5 ends the game (`GGame::Close`), and `pc_main`
  0x641C60 sets the start mode 4 and shows the box outside a game (`FrontEnd::JustDoSkirmishGameBox` 0x544F80) before
  the next `GGame::StartGame`.
- **How it runs:** `FrontEnd::JustDoABox` 0x544F90 is a loop of its own between the two games, not a box of the game's
  frame (the game's boxes are made active with `SetupBox::SetCurrentActiveBox` and drawn in the frame, see
  [../hand/states.md](../hand/states.md)). Each pass starts and finishes a frame, draws the plasma backdrop, hands the
  keyboard's last key to the active box and takes the mouse from the same input system the game's frames read; its
  black starts at 999 ms and goes down by `LH3DTech::g_delta_time` (the frame's time) while the box is up and up again
  after, and the loop ends once the box is gone and the count has passed 999.
- **Its controls** (`SkirmishGameBox::Init` 0x544820, the big text size): the title text at 40, 30, 720 by 40
  (HELP_TEXT_DIALOG_ADDITION_26); Back, a left arrow of 40 at 30, 530; Start Game (HELP_TEXT_DIALOG_LOBBYSTARTGAME), a
  right arrow of 40 at 730, 530; the list at 100, 110, 600 by 380.
- **Its callback** (0x544DF0, named `SkirmishGameBox_Callback`): message 0 tells the Start button whether nothing is
  picked (the list's selection +0x248 is -1); Back sets the start to the automatic save's (`GAME_START_TYPE_AUTOSAVE`,
  0, bw1-decomp `frontend.cpp`) and hides the box, so the next start reloads the automatic save, or starts a new game
  when it can't be read (`GGame` 345-357); Start Game builds
  `.\scripts\playgrounds\<name>.txt` from the stem at the selection, with no check of the list's end (raffclar's
  vanilla bug 495), and hides the box; the draw message 0xD draws the band under the title in 0xE0E0E0 at half the
  box's alpha.
- **Start Game** then starts the game in mode 4: `GGame::StartPlaygroundGame` 0x555990 and the skirmish byte (above).

### The land information box

- **Made** in `FrontEnd::Init` 0x53B4D0 as a `StartGameBox` of 780 by 580 (the global 0xCD0B14), its controls in
  `StartGameBox::Init` 0x5445B0: the title text at 40, 30, 720 by 40; Go! (HELP_TEXT_DIALOG_ADDITION_62), a button at
  320, 530, 160 by 40; the list at 100, 110, 600 by 380 with its +0x291 at 0 (the skirmish box's list has 1: the
  list's selection shown or not); Back, a left arrow of 40 at 30, 530.
- **Filled** by the land script's `START_GAME_MESSAGE` (command 99 of the table 0xC20F00, its case at 0x717BCD in the
  command callback 0x715180) and `ADD_GAME_MESSAGE_LINE` (command 100, 0x717C9C): the number 0 takes the script's
  string, 2 and any other number help text 0x1B36, 3 0x1B37, 4 0x1B38; the start message hides every box (`DialogBoxBase::HideAll` at 0x717C4C), then copies the title, empties
  the list and shows the box; a line goes at the list's end. openblack since 2026-11-07 (`gui::ApplyLandMessage`, which puts every
  box away through `GameInterface::HideAllDialogs` first ([advisors and help](advisors-and-help.md)); test `LandInfoBox.TheStartMessagePutsEveryDialogAwayBeforeTheTitle`).
- **Its callback** (0x544770): Go! hides the box; Back hides it and sets the game mode 5, which ends the game for the
  skirmish box again; the draw message draws the title's band as the skirmish box's.

### The skirmish's creature

`GGame::Init` sends the creature packet 0x4E at a skirmish's start (0x54FD24..0x54FDD1): when the local player has a
citadel and its leader interface status is the local one, at the cell of the citadel's creature home. Its handler
(0x63DA4F) loads the profile's creature there, knowing every leash and at phase 13. See
[the player's creature](../creature/players-creature.md).

### The end of the game

`GPlayer::ProcessPlayers` 0x649A20, after every player's `Process`, in a network game or a skirmish and while the game
is not over (the byte `GameOver`):

1. In a skirmish there are no winning conditions and no time limit (the time-limit test is skipped when +0x205A0C is
   set), so the game goes straight to its losers.
2. Every 10 turns each active player's share of the influence power is added to its score history (not ported).
3. For each active player (`GGame::GetNextActivePlayer` 0x5508D0: the slots 0..6 of a type other than 0) numbered
   under 4, with no citadel and not yet lost (+0x938): it has lost (+0x938 = 1), and its creature, if any, is deleted
   (`ToBeDeleted(0)`). For the local player the creature's mind is first saved into memory (`SaveMindToMemory`), which
   only the end box's online upload and `CreatureDatabase::Send` read.
4. For each active player that has lost and is not finished (+0x93C): `GameStats::PlayerLostTheGame(player, 0)`
   0x56A6D0 (below); then, for the local player, the end box as eliminated (result 1, Watch Game) while
   `GetNumberOfPlayersThatHaveLost` 0x555130 is under `GetNoPlayers` 0x550A80 less 1, else the game is over and the end
   box shows lost (result 3); for another player, the box with OK "%s is out of the game." (`HELP_TEXT_WIN_GAME_03`) (help text 0x108E, through
   `DoOKGameRequestor` 0x54CB10) while the counts leave two or more gods, unless the end box shows, else the game is
   over and the end box shows won (result 2), whether or not the local player lost before. The counts compare with no
   sign (`GetNoPlayers - 1U <= lost`). Then the player is finished (+0x93C = 1) and its computer mind stops (its
   `Active` +0x1B8 = 0).
5. `GetNoPlayers` counts the active players of a type other than 0 numbered under 4; `GetNumberOfPlayersThatHaveLost`
   those of type 1 or 2 that have lost.

`GameStats::PlayerLostTheGame` 0x56A6D0 (v1.42 as 1.20): every other active human player not lost counts one more
god defeated (`NumGodsDefeated`, statistics +0x1064) if the loser is human or the game is a skirmish; a human loser
takes the next place (`Position` = ++`NumPlayersThatHaveLeftTheGame`) and its `LossReason`; when the lost count is
then exactly `GetNoPlayers` less 1, the last human seen still in takes the place after.

Once `GameOver` is set, `GGame::ProcessOneGameTurn` 0x54D620 skips `ProcessGameCode` (the turn's start, its body and
its end): only the inputs and the steps after the code run, and the turn count stays.

### Leaving

- **The menu in a skirmish** (`MainMenu::InitControls` 0x53FBA0, run each time the menu shows): the second button
  reads help text 0x1ACB (HELP_TEXT_DIALOG_ADDITION_170, Leave Skirmish Game) in place of 0x1ACA, Join Online Game is
  hidden (`SetupControl::Hide`, the third virtual, with the skirmish byte), and the buttons are 80 high and 100 apart
  from 150 down (70, 80 and 145 otherwise; in a network game too), a hidden button taking no place.
- **Leave Skirmish Game** asks HELP_TEXT_DIALOG_ADDITION_167 ("This will leave the current Skirmish game. Do you wish
  to do this?"); Yes ends the game with the game mode 5, and the skirmish box shows again.

### The end box

- **Made** in `FrontEnd::Init` 0x53B4D0 (`FrontEnd::EndGameDialog`, 780 by 500). `EndGameBox::Init` 0x56E160 makes its
  seven tabs, its texts and three arrows: two at 20, 530 (Back to Game and Watch Game) and one at 740, 530 (Leave Game);
  the title's text is 49 high.
- **Its title** (`EndGameBox::InitControls` 0x56E740) by the result: 1 (out, the game goes on) and 3 (out, decided)
  help text 0x1ABA "You have lost the game." (`HELP_TEXT_DIALOG_ADDITION_152`); 2 (won) 0x108F "Congratulations! You've won the game!" (`HELP_TEXT_WIN_GAME_04`), or for a local
  player numbered 4 and up "%s - %s Won" with 0x1A0E and the last god's name; 0 the question to leave (network). Result
  1 also gives the advisors' one-off guidance (`GGuidance::HelpSpritesSayOneOffGuidanceIfNecessary`).
- **Its callback** (0x56EC00): message 7 clears its showing mark (raffclar's withdrawn vanilla bug 470); the tabs 0x8FD
  and 0x8FE open the winning conditions' box and the Game Statistics box over it; the draw message 0xD draws the
  title's band in the local player's colour from the statistics package, at half alpha, which for a looker-on reads
  past its four players (raffclar's vanilla bug 471).
- **The box with OK** for another god out is the game's own message box (`DoOKGameRequestor` 0x54CB10) with help text
  0x108E "%s is out of the game." (`HELP_TEXT_WIN_GAME_03`), opened only while the end box does not show itself.


## openblack

- ✅ **The skirmish flag:** `RegistryContext::skirmish`, cleared with the map, read through `ecs::IsSkirmishGame`
  (`src/ECS/SkirmishGame.h`) by the original's readers that openblack has: the saves (`save_triggers::TempleBuilt`
  and `Exiting` skip the automatic save; a save keeps the flag as the game tail's first word, 1 or 0, as
  `GGame::Save` 0x554090 writes the 32-bit flag after the scripts' reboot byte, and a load puts it back, any value
  but 0 a skirmish, as `GGame::Load` 0x554830 reads it: `saved_game_save::HeadOf`, `saved_game_load::StateOf` and
  `PutBackSkirmish`, test `SaveTriggers.ASaveRoundTripKeepsTheSkirmish`), the temple's game over (`GameTempleDestructionWorld::PlaygroundGame`, so the
  story's game-over script does not start, as `GGame::EndTurn` 0x54E960), the computer gods' start
  (`computer_player::World::Skirmish`), the villagers' belief speed scale (`VillagerSpeed`'s `multiplayer`, as
  `Villager::SetStateSpeed` 0x753760), the guidance's playground query (`GameQueries::playgroundGame`, as
  `GGuidance::PlayNow` 0x71AF50 lets every advisor type speak on land 1 in a skirmish), F2's question
  (`input::playground_key::KeyAsks`, as `GGame::ProcessKey` 0x63EF20) and the fight music (the big fight on any land,
  as `GAudio::PlayFightMusic` 0x427590; test `GameMusicTest.ASkirmishPlaysTheBigFightOnAnyLand`).
- ⚠️ **The skirmish box:** `gui::SkirmishBox` and its rules `gui::skirmish_box` (`src/Gui/SkirmishBox*.cpp`, raffclar's
  box refitted onto openblack's dialogs). As the original, Start Skirmish Game ends the game at once
  (`Game::EndGameForSkirmishBox`), and the next start shows the box over the front end's backdrop before any land
  (`Game::AskForSkirmishLand` from `LoadStartLand`, `GameInterface::ShowSkirmishBox`): Start Game loads the land as a
  skirmish (`LoadMap` with `skirmish`: the challenge program is read and only its auto-start script starts, as the original's `Reset(1)` reads it, so no story script and no skip-tutorial question; 2026-11-08), Back
  starts the story. The list reads the folder each time, every `.txt` name in the order Windows gives them
  (`skirmish_box::FolderOrderLess`), each by its name without the last four characters, Two, Three and Four Gods by
  HELP_TEXT_LAST_LINES_01..03 (help texts 0x1B36..0x1B38). As the original's loop, the box runs its own frames
  between the two games, and takes the input as the game's frames take it: the window's events through the test runs'
  filters (`input::DropRealEvent`, `input::replay::TakeRealEvent`), then a recording's events due at the box's frame
  (`input::replay::DueEvents`); its frames and fade go by the fixed step of a run of the checks (`fixed_clock`), and
  in the runs that read no real pointer its pointer is where its own events put it (`input::PointerAfter`). Tests
  `SkirmishBox.*`, `SkirmishBoxRules.*`, `RealInput.theBoxPointerFollowsOnlyMotionAndButtons`. Not yet seen through
  the real input.

- **The land information box:** `gui::LandInfoBox` (`src/Gui/LandInfoBox.cpp`, raffclar's #1009 refitted), filled
  through the event `LandMessageGiven` from the two script commands (`FeatureScriptCommands`) and
  `land_message::HelpTextOf` (`src/LandMessage.h`); its Back opens the skirmish box (`Game::OpenSkirmishBoxOverGame`,
  the game paused under it). Tests `LandInfoBox.*`, `LandMessage.*`.
- **The skirmish's creature:** `player_creature::BringIntoSkirmish` at the skirmish's first turn, before its packets
  (`Game::PlayTurnStep`).
- ✅ **The skirmish box's Back:** the next start asks for the automatic save (`startup::AutoSaveAskedAfterSkirmishBox`,
  `Game::AskForSkirmishLand`): it is reloaded with the main menu over it, as a game started from it opens, or a new
  game starts when it can't be read. The log says `The skirmish box: Back to the story, from the automatic save`.
  Test `StartupRules.TheSkirmishBoxsBackReloadsTheAutomaticSaveWithTheMenuOverIt`.
- ✅ **The land information box:** `gui::LandInfoBox` (`src/Gui/LandInfoBox.cpp`, raffclar's box refitted), filled
  through the event `LandMessageGiven` from the two script commands (`FeatureScriptCommands`) and
  `land_message::HelpTextOf` (`src/LandMessage.h`, the numbers as the commands' handlers read them); Go! and Escape
  close it, its Back ends the skirmish for the skirmish box (`Game::EndGameForSkirmishBox`); the game goes on under it.
  The log says `The land information box shows: <heading>` as it shows, `The land information box: Go! (or Escape)
  closes it` and `The land information box: Back ends the skirmish`. Tests `LandInfoBox.*`, `LandMessage.*`.
- ⚠️ **The skirmish's creature:** `player_creature::BringIntoSkirmish` at the skirmish's first turn, before its packets
  (`Game::PlayTurnStep`), as the start's packet. Not yet seen through the real input.
- ⚠️ **The start camera:** a game's start (`Game::LoadStartLand`, the skirmish's and a new game's, not a land change or
  a saved game) on a land whose script leaves `START_CAMERA_POS` at the origin, or gives none
  (`RegistryContext::startCameraPlace`, all zero), puts the camera over the player's temple as over a script's start
  place and flies it to the temple as the temple key's double tap flies (`start_camera::StartsAtTemple`,
  `ShortcutKeys::StartAtTemple`, `ZoomToPlaces::StartAtTemple`), as `GGame::Init` makes `GCamera(citadel->Pos)` and
  calls `CameraModeNew3::ZoomToCitadel(x, z, CitadelDistance, CitadelPitch, 0)` for the start modes 1, 3 and 4
  (bw1-decomp `src/Black/Game.cpp:433`). Of the shipped lands only Four Gods, `LandT.txt` and `comp.txt` leave it at
  the origin. Tests `StartCamera.*`, `ZoomToPlaces.AGameStartFliesToTheTempleFromAFreshCamera`. Not yet seen in a run.
- ⚠️ **The end of the game:** `game_end::ProcessTurn` and `game_end::CountOut` (`src/ECS/GameEndRules.cpp`, pure,
  raffclar's rules without the network game's conditions), run by `GameEndSystem::ProcessTurn`
  (`Locator::gameEndSystem`) from `magic::ProcessTurnStart` after the players' turns, before the interface alignment.
  A player's standing is the `GameStanding` component on its entity (lost, finished). The creature goes through
  `creature_removal::RemoveFromGame`, the computer mind through `ComputerPlayerSystem::SwitchOn(false)`, the
  statistics through `game_stats::GodDefeated` and `TakeNextPlace`. Tests `GameEnd.*`, `GameEndSystem.*`. Not yet
  seen through the real input.
- ⚠️ **The end box and the stop:** `end_game_box::Follow` (`src/Gui/EndGameBoxRules.cpp`, raffclar's rules without
  the network game's question and conditions) opens the boxes from the turn's events in `Game::FollowGameEnd`:
  `gui::EndGameBox` (`GameInterface::OpenEndGameBox`, which closes the menu and the message box) or the message box
  with OK. Leave Game ends the skirmish for the skirmish box; Watch Game closes the box. Once the game is decided,
  `Game::GameLogicLoop` plays only the turn's input steps (`game_turn::PlaysOnceDecided`): the turn count stays, as
  the original's. The loop still counts each such turn for its frame (`game_clock::CountTurnWithoutGameCode`), as the
  original's `ProcessNetworkPackets` counts every turn it plays whether `ProcessOneGameTurn` runs the game's code or
  not, so a decided skirmish plays one turn a frame; before 2026-11-04 the count lived in the turn's start, which a
  decided turn leaves out, and the frame's loop never ended (test `GameClock.ATurnWithoutTheGameCodeEndsTheFramesLoopAndKeepsTheTurn`). raffclar's own stop keeps the clock's step for his network game; ours leaves it out, as the
  original does. Tests `EndGameBox.*`, `GameTurnOrder.ADecidedSkirmishDoesOnlyThePlayersInputs`. Not yet seen
  through the real input.

- ⚠️ **Leaving:** `GameMenu::SetSkirmish` (the label, the hidden button and the layout), kept in step with
  `ecs::IsSkirmishGame` by `Game::HandleInterfaceAction`; Leave Skirmish Game asks with `GameMenu::Ask`, and Yes
  (`Action::LeaveSkirmish`) ends the skirmish under the skirmish box. Tests `GameMenu.InASkirmish*`,
  `GameMenu.OutOfASkirmishTheSecondButtonStartsASkirmishAtOnce` (raffclar's menu refitted, without its network
  game's question and conditions box). Not yet seen through the real input.

## Pending

- The skirmish's creature: the leader interface status test of the start's packet (openblack has one interface).
- 🔍 The start camera looks at the temple's own position, as `GCamera(citadel->Pos)`; the zoom's help callback
  (`CameraHelpCallback(0x307)`) is not sent, as the temple key's is not.
- The lists' scroll bar shows always; raffclar's shows it only while the lines do not fit, not checked against the
  original (the lists' +0x291 is their selection's showing, not the scroll bar).
- The `SKIRMISH` launch switch (openblack has no such switches).

- 🔍 The temple rooms' `IsAvailable` and `GGame::Loop`'s other reads of the skirmish flag.
- The end box: the Multiplayer tab's box of the winning conditions, the advisors' one-off guidance as the lost box
  opens, the box's other texts; the score history every 10 turns that its graphs read.
- The score history every 10 turns; the local creature's mind kept in memory (online only).
- The `SKIRMISH` launch switch (openblack has no such switches).
