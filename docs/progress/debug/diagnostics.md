# Diagnostics

openblack's ways of measuring and checking itself: the game's command-line switches for testing, logging, the
profiler, frame statistics, benchmarks, screenshots and crash reports.

**Progress: 15/19 done, 3 partial — 87%**

How the original does it, in our wiki: [openblack internals](../../bw1-notes/openblack-internals.md).

## Command line

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Point the game at the original's files, pick the land to start on, or the testbed | partial | `--game-path` and `--start-level` (`src/main.cpp`); no testbed in our tree |
| Window size, window mode, renderer backend, vertical sync, interface scale | done | `-W`, `-H`, `--window-mode`, `--backend-type`, `--vsync`, `--ui-scale` (`src/main.cpp`) |
| The original's detail level, 0 to 6 | done | `--detail-level` |
| Run a set number of frames and quit, for automated runs | done | `--num-frames-to-simulate` |
| A screenshot at a given frame, to a given path | done | `--screenshot-frame`, `--screenshot-path` |
| Run a testbed scenario or benchmark | todo | no `--scenario` or benchmark switches in our tree; see [testbed.md](testbed.md) |

## Logging

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Logs to a file, the terminal or the Android log | done | `--log-file` (a file, `stdout` or `logcat`) |
| A log level for each subsystem | done | `--log-level` |
| The log shown in the console window | done | `src/Debug/Console.cpp`, the editor's log panel (`src/Editor/Panels/LogPanel.cpp`) |

## Measuring

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A profiler of the frame's stages on the CPU and of the renderer's views on the GPU | done | `src/Profiler.cpp`, `src/Debug/Profiler.cpp` |
| Frame statistics logged every so many frames: average, 95th percentile and slowest frame, CPU and GPU, each stage | done | `--frame-stats`, `src/Debug/FrameStatsLog.cpp`, `src/Common/FrameStats.cpp`; tests `FrameStats.*` |
| Benchmarks summed up as mean, 95th percentile and slowest, written as JSON and CSV to compare runs | partial | `src/Debug/BenchmarkRecorder.cpp`; tests `BenchmarkRecorder.*`; nothing in the game runs a benchmark |
| Screenshots from the menu bar | done | "Capture" in `src/Debug/Gui.cpp` |
| A hash of the game's state each turn to check two runs stay the same | done | `OPENBLACK_STATE_HASH` writes one hash line per turn (`src/Debug/StateHash.cpp`); test `ReplayDeterminism.sameGameInTwoProcesses` |
| Memory use by system | partial | the process and graphics memory are logged with `OPENBLACK_PROFILE` (`src/Debug/MemoryStats.cpp`); not by system |

## Crashes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every kind of crash (failed assertions, runtime errors, abort, terminate, fatal signals, system exceptions, fatal renderer errors) is caught | done | `src/Common/CrashHandler.cpp` |
| A report with a stack trace goes to stderr, the log and a file in a crashes folder, with a minidump on Windows | done | `src/Common/CrashReport.cpp`, the minidump in `CrashHandler.cpp`; tests `CrashReport.*` |
| The game exits with a non-zero code instead of waiting on a dialog, breaking into a debugger first if one is attached | done | `src/Common/CrashHandler.cpp` |
| A switch keeps the system's own crash dialogs | done | `--crash-dialogs` (`CrashHandler.h`); test `CrashReport.FindsTheCrashDialogsSwitch` |
