# Display and frame rate

The game's window and screen: resolution, full screen or windowed, the graphics device, picture settings, vertical sync
and recovering when the device is lost. The game turn rate is in [../engine/](../engine/); frame statistics and the
debug wireframe are in [../debug/](../debug/).

**Progress: 4/8 done, 4 partial — 75%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## Window and device

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player picks the screen resolution | partial 40% | `-W` and `-H` on the command line (0 by default: 85 % of the desktop, centred; `src/main.cpp`); no in-game list of modes; re-measured 2026-10-21: picked on the command line only; the in-game list of modes is missing |
| Full screen or in a window | done | `--window-mode` windowed, fullscreen or borderless (`src/main.cpp`) |
| The picture is rebuilt at the new size when the window changes size | done | `Renderer::Reset` called on a window resize from `src/Game.cpp` |
| The game behaves sensibly while minimised | done | on minimise and restore the audio is switched off and on (`audio::OnFocus` from `src/Game.cpp`) and the whole game waits until the restore, as the original (`src/Windowing/WindowAway.h`); switching to another window without minimising changes nothing |
| The player picks the graphics device | partial 40% | `--backend-type` picks the backend; no choice of adapter; re-measured 2026-10-21: the backend is picked; the adapter choice is missing |
| Losing the graphics device (switching away, a driver reset) is recovered from, textures loaded again | partial 50% | left to bgfx; re-measured 2026-10-21: bgfx resets the device; reloading the textures as the original is not checked |

## Picture and frame rate

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Taking a screenshot | inferred | `--screenshot-frame` and `--screenshot-path` take one at a frame; no key in game (unconfirmed the game has one); re-measured 2026-10-21: whether the original has a screenshot key is not confirmed; the command-line shot is a test tool |
| Vertical sync | done | `--no-vsync` (on by default, as the original) (`src/main.cpp`, `EngineConfig`); the original's only frame cap is its full-screen flip's vertical sync (`VSync` missing or non-zero), with no sleep or minimum frame time in its loop, read 2026-10-22 ([the frame rate's cap](../../../engine/loop.md#the-frame-rates-cap)) |
