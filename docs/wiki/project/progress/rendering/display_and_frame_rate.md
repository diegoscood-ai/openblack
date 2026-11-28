# Display and frame rate

The game's window and screen: resolution, full screen or windowed, the graphics device, picture settings, vertical sync
and recovering when the device is lost. The game turn rate is in [../engine/](../engine/); frame statistics and the
debug wireframe are openblack's debug controls ([the list](../../not-in-the-original.md#openblacks-debug-controls-kept-exempt-from-removal)).

**Progress: 4/7 done, 0 chosen, 3 partial, 0 inferred — 76%**

How the original does it, in our wiki: [Graphics engine parity: original versus openblack](../../../rendering/frame.md), [World rendering: original versus openblack](../../../rendering/frame.md).

## Window and device

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player picks the screen resolution | partial 40% | `-W` and `-H` on the command line (0 by default: 85 % of the desktop, centred; `src/main.cpp`); no in-game list of modes, read 2026-11-22: the original has no in-game choice either; it takes the screen's width, height, depth and full screen at start from the setup program's registry (`start_system`, bw1-decomp `PCMain.cpp`: `RegistryRetrieve*` on `BWSETUP_REGISTRY_KEY`, the screen's size and depth and "FullScreen"), so what is left is the setup program's, the same question as [options_and_settings.md](../engine/options_and_settings.md) "The setup program's graphics choices" |
| Full screen or in a window | done | `--window-mode` windowed, fullscreen or borderless (`launch_options::DisplayModeNamed`, `src/LaunchOptions.h`, from `src/main.cpp`); test `LaunchOptions.TheWindowIsWindowedFullScreenOrBorderlessByItsName` (`test/test_launch_options.cpp`) |
| The picture is rebuilt at the new size when the window changes size | done | `Renderer::Reset` and the main view's size, called on the window's size change from `src/Game.cpp` (its SDL window event); no unit test: a window event passed straight to the renderer, with no logic of its own |
| The game behaves sensibly while minimised | done | on minimise and restore the audio is switched off and on (`audio::OnFocus` from `src/Game.cpp`) and the whole game waits until the restore, as the original (`src/Windowing/WindowAway.h`); switching to another window without minimising changes nothing; tests `WindowAway.aMinimiseHoldsUntilTheRestore`, `WindowAway.switchingAwayWithoutMinimisingDoesNotHold`, `WindowAway.theLoopWaitsOnlyWhileAwayAndRunning` |
| The player picks the graphics device | partial 40% | `--backend-type` picks the backend; no choice of adapter, read 2026-11-22: the original has no in-game choice either; it takes the DirectDraw and Direct3D devices at start from the setup program's registry (`start_system`, bw1-decomp `PCMain.cpp`: `RegistryRetrieve*` on `BWSETUP_REGISTRY_KEY`, "DDrawGuid" and "D3DeviceGuid"), so what is left is the setup program's, the same question as [options_and_settings.md](../engine/options_and_settings.md) "The setup program's graphics choices" |
| Losing the graphics device (switching away, a driver reset) is recovered from, textures loaded again | partial 50% | left to bgfx; re-measured 2026-10-21: bgfx resets the device and keeps every texture across it. The original (read 2026-11-23): `LHScreen::RunInMSWindow` restores its surfaces (`LHSurface::RestoreAll` 0x7E6960) and a texture whose video memory is gone is reloaded from its file as it is next used (`LH3DTexture::YouLostYourVRAM` 0x838430 sets `ReloadPending` for a named texture), so the same textures show; read 2026-11-28 (bw1-decomp `LH3DTexture::Activate`): every texture's video copy is let go with its surface (`fn_0085DB10` calls `YouLostYourVRAM`), and on its next use a system-memory texture (the block textures) is copied up again from its own copy, a named one (the players' symbols and tattoos) is made again by its maker's callback, a file one is read again, so the same textures show; openblack differs where the driver loses the device itself (Vulkan's lost device, [the frame](../../../rendering/frame.md)): the game crashes on its next frame, which bgfx does not expose |

## Picture and frame rate

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Vertical sync | done | `--no-vsync` (on by default, as the original) (`src/main.cpp`, `EngineConfig`); the original's only frame cap is its full-screen flip's vertical sync (`VSync` missing or non-zero), with no sleep or minimum frame time in its loop, read 2026-10-22 ([the frame rate's cap](../../../engine/loop.md#the-frame-rates-cap)); tests `ResetFlags.StartupFlagsAreTheVsyncBitAlone`, `ResetFlags.SwitchingKeepsTheOtherFlags` |
