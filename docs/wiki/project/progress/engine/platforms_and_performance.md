# Platforms and performance

What the game runs on and how it behaves as a program: the systems and graphics it needs, its threads, how it paces
frames and loads, and how it quits. openblack's own tools for measuring performance and its crash reports are not the
original's: they are listed in [openblack's debug controls](../../not-in-the-original.md#debugdiagnostics), and what was found
about them is on [Not in the original](../../not-in-the-original.md#engineplatforms_and_performance).

**Progress: 10/12 done, 0 chosen, 1 partial, 0 inferred — 88%**

## Systems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Runs on Windows | done | built and tested on Windows in CI (`.github/workflows/ci-vcpkg.yml`) |
| Runs on a Mac (the game had a Mac release) | done | built and tested on macOS in CI (`.github/workflows/ci-vcpkg.yml`) |
| Draws through the system's modern graphics interface rather than DirectX 7 | done | bgfx with Direct3D 12, Vulkan or Metal (`src/EngineConfig.h`, `--backend-type` through `launch_options::BackendNamed`, `src/LaunchOptions.h`); test `LaunchOptions.TheGameDrawsThroughVulkanUnlessTheCommandLineNamesAnotherInterface` (`test/test_launch_options.cpp`) |

## Threads

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world and the drawing share one main loop, a turn and a frame at a time | done | `Game::Update` (turns, then the frame) and `Game::Run`; test `GameFrameOrder.OpenblacksOrder` |
| Music streams on a thread of its own | done | the music's six tracks are fed by a thread of their own every 120 ms (`src/Audio/Engine/MusicStream.cpp`); see ../audio/; tests `MusicStream.*` (`test/audio/test_music_stream.cpp`) |
| The graphics are submitted from a render thread so the game doesn't wait on the card | done | bgfx is built multithreaded (`vcpkg.json` feature `multithreaded`), its render thread apart from the main thread |
| Network play runs its own threads | pending | see ../multiplayer/ |

## Frames

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Frames are drawn as fast as they can be, or held to the screen's refresh | done | `--no-vsync` (on by default, as the original); the turn rate is separate, see game_loop_and_clock.md |

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loading box with progress text shows while the game starts and while a land loads | done | the tips screen with its tip, picture, version and loading bar as the game starts and while any land or saved game loads, and a script's map's "please wait" banner (`gui::LoadingScreen`, `src/Gui/LoadingScreen.cpp`; `Game::BeginLoadingScreen`, `src/GameStartup.cpp`); tests `LoadingScreen.*` (`test/gui/test_loading_screen_rules.cpp`) |
| Meshes, animations, textures and sounds are loaded once and looked up by name | done | resource caches (`src/Resources`), loaded in `Game::Initialize`; tests `ResourceCacheOnce.*` (`test/test_resource_cache_once.cpp`) |
| The next land's resources are loaded when changing land, the last land's freed | partial 50% | `Game::LoadMap` clears the world; assets stay loaded for the whole run |

## Crashes and quitting

A save that crashed the game while loading is not offered again: that row is on saving_and_loading.md ("A save that failed to load twice").

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Closing the window or quitting from the menu ends the game cleanly | done | `Game::ProcessEvents`, the menu's quit question; tests `GameMenu.QuittingAsksFirst`, `NewGameShutDown.AGamesShutDownTakesEverySlotAGameMakesAndNoneAProcessMakes` |
