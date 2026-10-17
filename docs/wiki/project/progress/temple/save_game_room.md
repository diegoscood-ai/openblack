# Save game room

The room where the game is saved and loaded: a scroll of the saves, a picture for each, and statistics of saving.

**Progress: 1/10 done, 3 partial — 25%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |
| The scroll lists the saved games with their date and land | partial | `src/3D/TempleScrolls.cpp`: the saved games are made up (`TempleScrolls::Facts::Mock`); our tree has no saved games |
| The time played since the game started | partial | counted from when the game's engine started (`game_clock::EngineMs`, in `GatherScrollFacts` in `src/3D/Implementations/TempleInterior.cpp`) |
| How many times the game has been saved and loaded | partial | made up (`TempleScrolls::Facts::Mock`): nothing is saved or loaded |
| Saving into a slot asks to confirm and takes a picture of the land | todo | see ../engine/saving_and_loading.md |
| Loading a slot asks to confirm and loads the game | todo | see ../engine/saving_and_loading.md |
| Deleting a save asks to confirm | todo |  |
| Each save's picture in low and high detail | todo |  |
| Quick save and quick load from the keys | todo | `QUICK_SAVE` and `QUICK_LOAD` are bindable actions (`src/Input/GameActionMap.cpp`) that do nothing; see ../engine/saving_and_loading.md |
| Scripts save the game into a slot | todo | `SAVE_GAME_IN_SLOT` is a stub in `src/CHLApi.cpp` |
