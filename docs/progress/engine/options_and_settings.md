# Options and settings

Where the game keeps what the player chose and who the player is: the install's settings in the Windows registry, the
graphics setup, the options set from the menu, the player profiles with their own folders, and the key bindings. The
options screens themselves are in [../interface/](../interface/).

**Progress: 7/24 done, 9 partial — 48%**

## Install and setup

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game finds its install folder through the registry | done | `src/main.cpp` and `src/FileSystem/DefaultFileSystem.cpp` read the GameDir registry value on Windows; `--game-path` elsewhere |
| The setup program's graphics choices (screen mode, resolution, device, vertical sync) are kept in the registry and used at start | partial | openblack takes them from its command line (`-W`, `-H`, `--window-mode`, `--backend-type`, `--no-vsync` (on by default, as the original)) and does not read the registry's |
| A sound override setting in the registry changes how the audio starts (unconfirmed what it sets) | todo | not read; see ../audio/ |
| The game can be started straight into a land | done | `--start-level` in `src/main.cpp` (openblack's own switch) |

## Options

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu's options (help level, story text, tooltips, films) are read from the registry at start and written back when changed | todo | `MenuSettings` in `src/Gui/GameMenu.h` starts from defaults each run |
| Sound effect and music volumes set in the menu are applied and kept between runs | partial | applied at once from the options box (`src/Game.cpp`, `EngineConfig::audioSampleMainVolume` and `audioMusicMainVolume`); not kept |
| The detail level chosen in the menu takes effect the next time a land loads, and is kept | partial | the menu steps through the levels but changes nothing yet (`GameMenu.cpp`); `--detail-level` sets it at start |
| Each detail level turns parts of the drawing on and off (water tiling, clouds, fog, weather and the rest of the table) | partial | water tiling, land reflection, clouds, fog and the hand shadow on objects follow the level (`src/Graphics/DetailLevel.h`); the weather and rain splash settings are not read yet; see ../rendering/ |
| Autosave and push scrolling are options kept with the others | partial | boxes in the menu (`MenuSettings::autoSave`, `pushScrolling`), not kept; autosave itself: see saving_and_loading.md |
| Left- or right-handed hand is an option of the player | partial | a box in the menu (`MenuSettings::leftHandedHand`), not read by the hand and not kept |

## Player profiles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There can be several players, each with a name, a symbol and a creature, created, chosen and deleted on the Players page | partial | openblack has one player named after `OPENBLACK_PLAYER_NAME` or the login (`src/Game.cpp`, `GameMenu::BuildPlayers`) |
| The player chosen last is remembered and greeted next time | todo |  |
| Each player has a folder of their own for saved games, the current land's screenshots, help statistics and their creature | todo |  |
| Profile folder names are written so that upper and lower case survive on any file system (unconfirmed exact rule) | todo |  |
| The help the player has already seen is saved with their profile so it isn't shown again | todo | see ../interface/ |
| The player's symbol and creature name are kept with their profile | partial | edited in the menu, not kept |
| Whether the current profile already has a creature decides if the game starts at the creature choice | todo | see ../story/ |

## Key bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game action has a default key or mouse binding | done | `src/Input/KeyBindings.cpp`; test `test/test_key_bindings.cpp` |
| The player can rebind an action and set the defaults back | done | `GameActionMap::SetKeyBinding`, `ResetKeyBindings` (`src/Input/GameActionMap.cpp`); the key bindings debug window (`src/Debug/KeyBindingsWindow.cpp`); see ../interface/ |
| Bindings are kept between runs | todo | held in memory only |

## openblack's own settings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Log file and per-part log levels | done | `--log-file`, `--log-level` in `src/main.cpp` |
| Screenshots of a chosen frame, and running a set number of frames then quitting | done | `--screenshot-frame`, `--num-frames-to-simulate` (`src/main.cpp`) |
| Frame statistics and benchmarks | partial | `--frame-stats` and `--frame-stats-views` (`src/Debug/FrameStatsLog.cpp`); the benchmark recorder (`src/Debug/BenchmarkRecorder.cpp`) is only used by its test; see platforms_and_performance.md |
| Interface scale for the debug windows | done | `--ui-scale` (`src/main.cpp`) |
