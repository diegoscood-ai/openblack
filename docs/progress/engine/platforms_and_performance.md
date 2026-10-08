# Platforms and performance

What the game runs on and how it behaves as a program: the systems and graphics it needs, its threads, how it paces
frames and loads, how it uses memory, and what happens when it crashes. openblack's own tools for measuring
performance are counted here too; its debug windows are in [../debug/](../debug/).

**Progress: 17/23 done, 3 partial — 80%**

## Systems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Runs on Windows | done | built and tested on Windows in CI (`.github/workflows/ci-vcpkg.yml`) |
| Runs on a Mac (the game had a Mac release) | done | built and tested on macOS in CI (`.github/workflows/ci-vcpkg.yml`) |
| Runs on Linux | done | built and tested on Ubuntu in CI (openblack only) |
| Builds for Android, iOS, ARM Linux and ARM Windows | partial | cross-compiled in CI (`.github/workflows/ci-cross-compile.yml`); `src/FileSystem/AndroidFileSystem.cpp`; not known to be playable there (unconfirmed) |
| Draws through the system's modern graphics interface rather than DirectX 7 | done | bgfx with Direct3D 12, Vulkan or Metal (`src/EngineConfig.h`, `--backend-type`) |
| The disc must be in the drive and its copy protection pass | n/a | openblack reads an existing install |
| The minimum machine of 2001 (a 350 MHz processor, 64 MB of memory, an 8 MB 3D card) | n/a | modern machines only |

## Threads

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The world and the drawing share one main loop, a turn and a frame at a time | done | `Game::Update` (turns, then the frame) and `Game::Run` |
| The keyboard is read on its own thread and its keys handed to the loop under a lock | n/a | openblack reads keys from SDL's events on the main thread |
| Music streams on a thread of its own | done | the music's six tracks are fed by a thread of their own every 120 ms (`src/Audio/Engine/MusicStream.cpp`); see ../audio/ |
| The graphics are submitted from a render thread so the game doesn't wait on the card | done | bgfx is built multithreaded (`vcpkg.json` feature `multithreaded`), its render thread apart from the main thread |
| Network play runs its own threads | todo | see ../multiplayer/ |

## Frames

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Frames are drawn as fast as they can be, or held to the screen's refresh | done | `--vsync`; the turn rate is separate, see game_loop_and_clock.md |
| Average and slowest (95th percentile) frame times can be logged | done | `src/Common/FrameStats.cpp`, `--frame-stats`; test `test/test_frame_stats.cpp` |
| The cost of each part of a frame can be measured, with the GPU's time per render view | done | `src/Profiler.cpp`, `--frame-stats-views` |
| Crowds of creatures and villagers can be benchmarked and the results written out | partial | the benchmark recorder exists (`src/Debug/BenchmarkRecorder.cpp`, test `test/test_benchmark.cpp`) but nothing in the game starts it; no crowd scenarios in our tree |
| A fixed time per frame for repeatable runs | done | `OPENBLACK_FIXED_FRAME_MS` (`src/Debug/FixedClock.cpp`), also for the audio (`src/Audio/Device/FixedClockSampleOutput.cpp`) |

## Loading

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A loading box with progress text shows while the game starts and while a land loads | todo | the window stays blank until loading ends |
| Meshes, animations, textures and sounds are loaded once and looked up by name | done | resource caches (`src/Resources`), loaded in `Game::Initialize` |
| The next land's resources are loaded when changing land, the last land's freed | partial | `Game::LoadMap` clears the world; assets stay loaded for the whole run |

## Memory

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game manages its own memory heaps and writes a leak report to the install folder on exit | n/a | openblack uses the standard allocator and owns memory through RAII |
| Memory use (process and graphics memory) can be logged for profiling | done | `src/Debug/MemoryStats.cpp` (working set, peak, private bytes and the bgfx memory), logged with `OPENBLACK_PROFILE` |

## Crashes and quitting

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A crash writes a report with the call stack into a crashes folder, named after the time, then exits | done | `src/Common/CrashHandler.cpp`, `CrashReport.cpp`; test `CrashReport.NamesFilesAfterTheTime` |
| Aborts, assertions, uncaught exceptions and signals are each reported with an exit code as the shell expects | done | tests `CrashReport.AbortLikeCrashesExitWithThree`, `CrashReport.SignalsExitAsShellsReportThem` |
| The system's own crash dialogs can be asked for instead | done | `--crash-dialogs`; test `CrashReport.FindsTheCrashDialogsSwitch` |
| A save that crashed the game while loading is not loaded again | todo | see saving_and_loading.md |
| Closing the window or quitting from the menu ends the game cleanly | done | `Game::ProcessEvents`, the menu's quit question |
