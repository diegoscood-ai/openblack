# Challenge room

The room of the challenges: a scroll listing every challenge found and done, and a picture of each, from which a
challenge can be replayed.

**Progress: 1/9 done, 2 partial — 22%**

## The room

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The room's camera comes in along its path and turns about the player | done | see ../camera/temple_camera.md (`src/Camera/TempleCameraModel.cpp`) |
| The scroll lists the challenges discovered and completed | partial | `src/3D/TempleScrolls.cpp`: the facts are made up (`TempleScrolls::Facts::Mock`, from `GatherScrollFacts` in `src/3D/Implementations/TempleInterior.cpp`); `src/Game/GameStats.cpp` exists but does not feed the scroll, and the lands' scripts give no titles yet |
| Each challenge has a picture taken when it was recorded | todo | `SNAPSHOT` is a stub in `src/CHLApi.cpp`; see ../story/challenges_and_rewards.md |
| Pictures show low detail, and high detail when looked at | todo | no pictures are made |
| Each challenge's picture shows its title and how well it went | todo | no pictures are made |
| A challenge can be replayed from its picture | todo | TODO in `src/3D/TempleToolTips.cpp` |
| The count of challenges and completed challenges | partial | made up (`TempleScrolls::Facts::Mock`): `challengesDiscovered` and `challengesCompleted` are not counted |
| Tooltips for the pictures and the replay button | todo | TODO in `src/3D/TempleToolTips.cpp` |
| The room's records are kept with the saved game | todo | our tree has no saved game; see ../engine/saving_and_loading.md |
