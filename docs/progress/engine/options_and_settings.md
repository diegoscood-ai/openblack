# Options and settings

Where the game keeps what the player chose and who the player is: the install's settings in the Windows registry, the
graphics setup, the options set from the menu, the player profiles with their own folders, and the key bindings. The
options screens themselves are in [../interface/](../interface/).

**Progress: 7/24 done, 9 partial — 48%**

## Install and setup

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game finds its install folder through the registry | done | `src/main.cpp` and `src/FileSystem/DefaultFileSystem.cpp` read the GameDir registry value on Windows; `--game-path` elsewhere |
| The setup program's graphics choices (screen mode, resolution, device, vertical sync) are kept in the registry and used at start | partial 40% | openblack takes them from its command line (`-W`, `-H`, `--window-mode`, `--backend-type`, `--no-vsync` (on by default, as the original)) and does not read the registry's; re-measured 2026-10-21: taken from the command line, not the registry |
| A sound override setting in the registry changes how the audio starts (unconfirmed what it sets) | pending | not read; see ../audio/ |
| The game can be started straight into a land | done | `--start-level` in `src/main.cpp` (openblack's own switch) |

## Options

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The menu's options (help level, story text, tooltips, films) are read from the registry at start and written back when changed | pending | `MenuSettings` in `src/Gui/GameMenu.h` starts from defaults each run |
| Sound effect and music volumes set in the menu are applied and kept between runs | done | applied at once from the options box (`src/Game.cpp`, `EngineConfig::audioSampleMainVolume` and `audioMusicMainVolume`); re-measured 2026-10-21: kept between runs in Profiles/Setup.txt, as the original keeps them (joint170) |
| The detail level chosen in the menu takes effect the next time a land loads, and is kept | partial 60% | (`GameMenu.cpp`); `--detail-level` sets it at start; re-measured 2026-10-21: the Options page sets the level as it is left (joint165); it is not kept between runs |
| Each detail level turns parts of the drawing on and off (water tiling, clouds, fog, weather and the rest of the table) | partial 80% | water tiling, land reflection, clouds, fog and the hand shadow on objects follow the level (`src/Graphics/DetailLevel.h`); see ../rendering/; re-measured 2026-10-21: water tiling, reflection, clouds, fog, the hand shadow and the atmosphere's weather switch follow the level; the rain splash setting is not read |
| Autosave and push scrolling are options kept with the others | partial 30% | boxes in the menu (`MenuSettings::autoSave`, `pushScrolling`), not kept; autosave itself: see saving_and_loading.md; re-measured 2026-10-21: boxes in the menu; not kept, autosave itself pending |
| Left- or right-handed hand is an option of the player | partial 20% | a box in the menu (`MenuSettings::leftHandedHand`), not read by the hand and not kept; re-measured 2026-10-21: a box in the menu; the hand does not read it |

## Player profiles

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There can be several players, each with a name, a symbol and a creature, created, chosen and deleted on the Players page | partial 20% | openblack has one player named after `OPENBLACK_PLAYER_NAME` or the login (`src/Game.cpp`, `GameMenu::BuildPlayers`); re-measured 2026-10-21: one player only |
| The player chosen last is remembered and greeted next time | pending |  |
| Each player has a folder of their own for saved games, the current land's screenshots, help statistics and their creature | pending |  |
| Profile folder names are written so that upper and lower case survive on any file system (unconfirmed exact rule) | pending |  |
| The help the player has already seen is saved with their profile so it isn't shown again | pending | see ../interface/ |
| The player's symbol and creature name are kept with their profile | partial 30% | edited in the menu, not kept; re-measured 2026-10-21: edited, not kept |
| Whether the current profile already has a creature decides if the game starts at the creature choice | pending | see ../story/ |

## Key bindings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every game action has a default key or mouse binding | done | `src/Input/KeyBindings.cpp`; test `test/input/test_key_bindings.cpp` |
| The player can rebind an action and set the defaults back | done | `GameActionMap::SetKeyBinding`, `ResetKeyBindings` (`src/Input/GameActionMap.cpp`); the key bindings debug window (`src/Debug/KeyBindingsWindow.cpp`); see ../interface/ |
| Bindings are kept between runs | pending | held in memory only |

## openblack's own settings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Log file and per-part log levels | done | `--log-file`, `--log-level` in `src/main.cpp` |
| Screenshots of a chosen frame, and running a set number of frames then quitting | done | `--screenshot-frame`, `--num-frames-to-simulate` (`src/main.cpp`) |
| Frame statistics and benchmarks | partial 70% | `--frame-stats` and `--frame-stats-views` (`src/Debug/FrameStatsLog.cpp`); the benchmark recorder (`src/Debug/BenchmarkRecorder.cpp`) is only used by its test; see platforms_and_performance.md; re-measured 2026-10-21: the frame stats are written; the benchmark recorder is only used by its test |
| Interface scale for the debug windows | done | `--ui-scale` (`src/main.cpp`) |
