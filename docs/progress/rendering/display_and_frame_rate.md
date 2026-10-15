# Display and frame rate

The game's window and screen: resolution, full screen or windowed, the graphics device, picture settings, vertical sync
and recovering when the device is lost. The game turn rate is in [../engine/](../engine/); frame statistics and the
debug wireframe are in [../debug/](../debug/).

**Progress: 4/8 done, 4 partial — 75%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../bw1-notes/parity.md), [World rendering: original versus openblack](../../bw1-notes/rendering.md).

## Window and device

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player picks the screen resolution | partial | `-W` and `-H` on the command line (0 by default: 85 % of the desktop, centred; `src/main.cpp`); no in-game list of modes |
| Full screen or in a window | done | `--window-mode` windowed, fullscreen or borderless (`src/main.cpp`) |
| 16 or 32 bit colour | n/a | modern displays are always 32 bit |
| The picture is rebuilt at the new size when the window changes size | done | `Renderer::Reset` called on a window resize from `src/Game.cpp` |
| The game behaves sensibly while minimised | done | on minimise and restore the audio is switched off and on (`audio::OnFocus` from `src/Game.cpp`) and the whole game waits until the restore, as the original (`src/Windowing/WindowAway.h`); switching to another window without minimising changes nothing |
| The player picks the graphics device | partial | `--backend-type` picks the backend; no choice of adapter |
| Losing the graphics device (switching away, a driver reset) is recovered from, textures loaded again | partial | left to bgfx |
| Anti-aliasing | n/a | the game has none; openblack's off-screen targets multisample where the format allows (`src/Graphics/FrameBuffer.cpp`) |

## Picture and frame rate

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gamma or brightness setting | n/a | no gamma or brightness setting in openblack. Our wiki differs: the original has no gamma, post-processing or fog setting in its frame ([parity](../../bw1-notes/parity.md)) |
| Taking a screenshot | partial | `--screenshot-frame` and `--screenshot-path` take one at a frame; no key in game (unconfirmed the game has one) |
| Vertical sync | done | `--no-vsync` (on by default, as the original) (`src/main.cpp`, `EngineConfig`) |
