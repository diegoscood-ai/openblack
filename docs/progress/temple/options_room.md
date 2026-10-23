# Options room

The temple room where the game's options are set: walking in opens the options dialog, and closing it walks back out.

**Progress: 5/6 done, 0 chosen, 1 partial, 0 inferred — 88%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`; test `TempleCamera.PictureRoomsStartLookingAlongTheEndOfTheirPaths`) |
| Arriving, the options dialog opens | done | `temple::options_room::Update` (`src/Temple/OptionsRoomRules.h`), from `TempleInterior::UpdateOptionsAndFutureRooms`, opens `gui::GameMenu` at its Options page: once the camera has arrived and no dialog is up, in the room the player is in, once a visit to the room, as `GameOptionsRoom::Update` 0x78D860 ([the options inside the temple](../../../interface/temple-interior.md#the-options-inside-the-temple), read 2026-10-22); tests `OptionsRoom.WaitsForTheCameraAndForNoDialog`, `ArrivingOpensTheOptionsOnce` |
| Closing the options goes back to the main room | done | `temple::options_room::Update`: once the dialog is closed the temple goes to the main room, unless the player's page waits to restart the game (`gui::GameMenu::IsRestartPending`, the exe's [0xCD0B04]), and the next visit opens the options again, as `GameOptionsRoom::Update` 0x78D860 ([the options inside the temple](../../../interface/temple-interior.md#the-options-inside-the-temple), read 2026-10-22); tests `OptionsRoom.ClosingGoesBackAndTheNextVisitOpensThemAgain`, `ARestartWaitingKeepsThePlayerInTheRoom` |
| Under the dialog the room turns slowly by itself | done | `TempleCameraModel::SetDialogOpen`; see ../camera/temple_camera.md (`k_PictureDialogTurnSpeed`) |
| Going to another room puts the options away; leaving the temple leaves them up | done | going to another room closes the menu (`TempleInterior::UpdateOptionsAndFutureRooms`), as the exe's `Temple::GoToRoom` hides every dialog (0x793DD7); leaving the temple hides none in the exe (`GGame::LeaveInsideCitadel` 0x553A70 and the rooms' `CloseEngine`, read 2026-10-22, [the options inside the temple](../../../interface/temple-interior.md#the-options-inside-the-temple)), so `TempleInterior::Deactivate` leaves the menu as it is (the close it had is on [not in the original](../../not-in-the-original.md#templeoptions_room)) |
| The options themselves | partial 27% | `src/Gui/GameMenu.cpp`; the share is ../interface/options.md's score as it stands (2026-10-21) |
