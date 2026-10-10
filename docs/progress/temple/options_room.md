# Options room

The temple room where the game's options are set: walking in opens the options dialog, and closing it walks back out.

**Progress: 5/6 done, 1 partial — 92%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |
| Arriving, the options dialog opens | done | `TempleInterior::UpdateOptionsAndFutureRooms` opens `gui::GameMenu` at its Options page |
| Closing the options goes back to the main room | done | `TempleInterior::UpdateOptionsAndFutureRooms` |
| Under the dialog the room turns slowly by itself | done | `TempleCameraModel::SetDialogOpen`; see ../camera/temple_camera.md |
| The options don't go out of the temple with the player | done | `TempleInterior::Deactivate` closes the menu, and so does going to another room |
| The options themselves | partial | `src/Gui/GameMenu.cpp`; see ../interface/options.md |
