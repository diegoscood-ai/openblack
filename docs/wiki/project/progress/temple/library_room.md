# Library

The temple's library (the credits room): seven scrolls of the game's help, the history of what was said, and the
people who made the game.

**Progress: 9/11 done, 1 partial — 86%**

## The scrolls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The credits: the people who made the game | done | `src/3D/TempleScrolls.cpp` (the library's first scroll, `Content::LibraryStaff`) |
| The creature: what the player has been told about creatures | done | `Content::LibraryCreature` in `src/3D/TempleScrolls.cpp`: the creature tips read, in reading order (`libraryHelp`, from `did_you_know_read::In`, gathered in `TempleInterior`); test `TempleScrolls.TheLibrarysHelpScrollsListTheTipsRead`; unverified against v1.42 |
| Miracles | done | `Content::LibraryMiracles`: the miracle tips read, in reading order; unverified against v1.42 |
| Navigation: moving the hand and camera | partial | `Content::LibraryControl`: the navigation tips read, in reading order (unverified against v1.42); the keys and buttons after each control are not written (TODO in `src/3D/TempleScroll.cpp`) |
| Village life | done | `Content::LibraryVillageLife`: the village life tips read, in reading order; unverified against v1.42 |
| Miscellaneous help | done | the did-you-know scroll (`Content::LibraryDidYouKnow`): the other tips read, in reading order; unverified against v1.42 |
| The history of the story so far: up to five entries of what was said | done | `Content::LibraryHistory` in `src/3D/TempleScrolls.cpp`, read from the dialogue's history (`help::StoryHistoryView`); the Previous Line, Latest Line and Next Line buttons (`gui::StoryHistoryBox`), newest line again on entering the library; tests `TempleScrolls.TheStoryHistoryShowsFiveLinesFromItsFirstLine`, `StoryHistory.*`; scenario `temple.library_history`; unverified against v1.42 |
| Each "did you know" read in the world is added to its scroll, once | done | the script highlights' read lists (`did_you_know_read`, a repeat ignored) feed the scrolls; test `ScriptHighlight.TheLibraryListsTheTipsOfACategoryInTheOrderTheyWereRead`; unverified against v1.42 |
| What has been seen is kept with the saved game | todo | our tree has no saved game |
| Signs over the seven scrolls | done | `src/3D/TempleSigns.cpp` |
| The room's camera turns about where its path ends | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |
