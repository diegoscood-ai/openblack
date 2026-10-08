# Library

The temple's library (the credits room): seven scrolls of the game's help, the history of what was said, and the
people who made the game.

**Progress: 3/11 done, 5 partial — 50%**

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The credits: the people who made the game | done | `src/3D/TempleScrolls.cpp` (the library's first scroll, `Content::LibraryStaff`) |
| The creature: what the player has been told about creatures | partial | `Content::LibraryCreature` in `src/3D/TempleScrolls.cpp`; the help the player was shown is made up (`TempleScrolls::Facts::Mock`, `libraryHelp`) |
| Miracles | partial | `Content::LibraryMiracles`; the help shown is made up (`TempleScrolls::Facts::Mock`) |
| Navigation: moving the hand and camera | partial | `Content::LibraryControl`; the help shown is made up (`TempleScrolls::Facts::Mock`), and the keys and buttons after each control are not written (TODO in `src/3D/TempleScroll.cpp`) |
| Village life | partial | `Content::LibraryVillageLife`; the help shown is made up (`TempleScrolls::Facts::Mock`) |
| Miscellaneous help | partial | the did-you-know scroll; the help shown is made up (`TempleScrolls::Facts::Mock`) |
| The history of the story so far: up to five entries of what was said | todo | TODO in `src/3D/TempleScrolls.cpp` |
| Each "did you know" read in the world is added to its scroll, once | todo | nothing records what was read; needs the bronze scrolls (../interface/scrolls_and_signs.md) |
| What has been seen is kept with the saved game | todo | our tree has no saved game |
| Signs over the seven scrolls | done | `src/3D/TempleSigns.cpp` |
| The room's camera turns about where its path ends | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |
