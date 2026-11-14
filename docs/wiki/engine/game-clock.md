# The game clock

## Summary

The original keeps game time with one timer that the speed factor scales at its source and that stops while the game
is paused. A turn is due whenever the timer reaches the next multiple of 100 game ms; at most one turn runs per frame
in single player, and more than 2 s of lag is dropped. After the turns, each frame computes the frame clock: whole game
ms since the previous frame, a visual clock and the fraction of the turn, which every system that moves per frame reads.
A second, wall-clock timer counts real ms since the engine started. The speed is 0.5, 1 or 2 from the keys, any value
from a script.

openblack ports this as one module, `game_clock` (`src/GameClock.{h,cpp}`), ✅ faithful: the game drives it from
`LoadMap` and `Game::Update`, and every reader that used to keep its own clock now reads it. The day and night cycle
is a different clock, on [Day and night](../world/day-and-night.md#clock).

## The original

### Game clock

The original **does not have a "give me the time" function**: `GGame::Loop` 0x54CF20 computes the clock once per
iteration and leaves it in GGame fields that hundreds of readers read inline (379 references to the turn, 157 to
`g_game_time_inc`, 29 to the fraction). Where it runs in the loop: [The original loop](loop.md#the-original-loop).

#### The game timer

An `LHTimer` at g_game +0x205D68 (+0x100 base, +0x104 accumulated ms, +0x108 speed factor, 0 = stopped, +0x10C saved
factor).

- `MSeconds` 0x43EB70 = `ftol((GetTickCount − base) · factor + accumulated)`.
- `Stop` 0x43E9C0 accumulates and sets the factor to 0.
- `SetSpeedUpFactor` 0x43EBC0 rebases if it is running and, if it is stopped, only saves the factor.
- To start it (start of Loop 0x54CF93, `ResetLocalGameTimer` 0x54C690, unpausing) the factor is set to 1e-5
  (0x3727C5AC) and then `SetSpeedUpFactor(saved)`: that way it rebases and **the stopped time does not count**.
- All in float (FPU at 24 bits, [Floating point](floating-point.md#the-x87-arithmetic)).
- The GGame ctor starts the game timer at speed 1 [0x54B58A].
- The loop's start (bw1-decomp `GGame::Loop`, read 2026-11-09): with a start kind other than 0 the timer is taken from 0
  and started (0x54CF6B..0x54CF93); the first pass's logo (`DoLogo` 0x5FA070, single player, its sample 160 started
  before and stopped after) runs on that timer, then `ResetLocalGameTimer` 0x54C570 (0x54D1F7) sets it to the turn
  times 100 from now, so the logo's time is not counted. openblack: `TimeSystem::StartGameClock` ends with
  `ResetTimerToTurn`, after the land's load, and again after the in-loop logo (`Game::PlayInLoopLogo`).
- The in-loop logo is one the player sees: [The in-loop logo](#the-in-loop-logo).

#### The in-loop logo

Read 2026-11-10 in bw1-decomp's matched `logo.cpp` (`DoLogo` :564, `RunAlexFX` :195) and against the frozen exe
(`RunAlexFX` 0x5F89F0, 0x1600 bytes, `DoLogo` 0x5FA070; the v1.42 bytes follow the C at every step read).

- **Where.** `GGame::Loop` 0x54CF20 (`Game.cpp:1779-1793`): after `DoNetworkStart` and "Starting game loop...", on the
  process's first pass only (a function static), the in-game bank's sample 160 (`LH_SAMPLE_LOGO`, 2D, no owner) is
  started; outside network games `DoLogo` runs and the sample is stopped after it (in a network game the sample starts
  and is never stopped). Then the start menu's `Show` (an autosave start), `InputReset`, and later `ResetLocalGameTimer`
  (0x54D1F7), so the logo's time does not count.
- **The start order.** `pc_main`: the launch's logo stills (`PlayLogoScreens`, `logo.bik`), the audio, the pre-intro
  (`pre_intro.bik`, no profile or `-PREINTROVIDEO`), the tips, `InitOneTimeOnly`, the new player's box (no profile),
  then `StartGame`, which loads the land and enters `GGame::Loop`: the in-loop logo plays there, after the land's load
  and before the first turn. `INTRO.bik` is asked for by the Land 1 script (`FollowUs`, opcode 203,
  [Bink](../video/bink.md)), in the turns, so after the logo.
- **`DoLogo`.** `RunAlexFX(INIT)`, `ClearKey`, then `RunAlexFX(UPDATE)` while no key is current (`keyboard.CurrentKey`,
  set by a key's down, cleared by any key's up), neither the left nor the right button is held (`mouse.Buttons`) and
  the effect is not finished; `RunAlexFX(RELEASE)`; empty frames (cleaning mode SCREEN: black) while the left or right
  button is still held; `InputReset` (keys, the character ring, the buttons' pressed bits, the mouse's moves and wheel,
  `ControlMap::MouseDelta`, the key buffer, the setup screens' clicks); the temple fade's level and target 0, its
  colour 0xFF000000, its count 0.
- **INIT.** The camera at (0, 0, 100) looking at the origin. `gatheringtext.raw` (with its alpha) in a mode 6 material
  and a mode 13 one (both `CreateMaterial`, no flags). The picture: when `(GetTickCount() / 1000) & 3` is 0 (the
  machine's clock in seconds, no random stream), `%TEMP%\userlogo.raw` is read (`GetTempPathA` and the name): 4096 bytes
  read one at a time, each grey written into its pixel's red, green and blue (rows of y, 0x5F8B55..0x5F8B89), then 128
  UTF-16 characters of a name (`fread(.., 2, 0x80, ..)` 0x5F8B98), character 128 set to 0. The file is used only when
  the name's first character is not 0; otherwise the name is two blanks and `data\lhlogo.raw` (0x3000 bytes, RGB) is
  read over the picture. 4096 sprites (`LH3DSprite::Create(4096, 1)`), for each column x, each row y: depth =
  `Random(70, 130)` (the C runtime's stream), the pixel ((W >> 1) + (x - 32) W / 240, (H >> 1) + (y - 32) W / 240)
  (whole-number divisions, the width both ways) at that depth (`Get3DPointFromScreen` with the game's near clip), the
  colour 0xFF000000 + the pixel's RGB, the mode 13 material when green ≥ 0x80 else mode 6, cell 5, angle
  `Random(0, 2π)`, size depth / `Random(200, 320)` (at least 0.0001). Then `qsort` (0x7C7E64, the runtime's quicksort,
  not stable) by the blue byte (`compfunc` 0x5F89B0); each sprite's size and place are kept as its own and its home.
  `data\D_SKY.l3d` as a static object with dynamic lighting off. One empty frame; the mouse's move cleared.
- **UPDATE**, each frame (`g_delta_time` the frame's whole milliseconds): the near clip 0.1, cleaning mode ZBUFFER (the
  colour is never cleared while it plays: each frame goes over the last). Time: 0.001 at the first frame, then
  += dt × speed × 0.0011 (0x5F904E). Fade: 255 − t (1/12) 255 below 12, 255 − (33 − t) 127.5 above 31, cut and kept
  to 0..255, into `LH3DFade::g_color` (black). zoom = CosineEase(clamp((t − 15)(1/11), 0, 1)); distance =
  a + (100 − a) zoom with a = clamp((15 − t)(1/15), 0, 1) 15 + 10; eye = (sin(0.352532 t + 2), cos(3 − 0.26793 t),
  sin(0.15161 t + 7)) × distance (the constants doubles; the first sine scaled while on the FPU, the others stored
  first, 0x5F91B5..0x5F9229), then eye += ((0, 0, 100) − eye) zoom; flash = clamp(1 − (t − 30) 0.5, −2, 1): below 1
  the focus is −eye (1 − flash), and eye ×= flash. The sky: white moved s / 256 of the way to black (each byte
  ((−255 s) >> 8) + 255, s = clamp((t − 22) 50, 0, 190), 0x5F94CA..0x5F9539), alpha 160 + 95 s / 256, set with
  specular 0 and drawn through the global alpha table (its one primitive, mode 2, becomes mode 3; its skin, with the
  mesh's flag 0x10000, is X1R5G5B5: opaque). The sprites' alpha clamp((flash + 1) 255, 0, 255); finished once
  flash ≤ −2 (t ≥ 36: about 33 s at speed 1). Size scale 1 + 1.54 zoom¹⁶. The mouse: its move smoothed by a tenth
  (whole pixels, cut), the wheel (±240) scales the speed by exp(wheel / 1500) (the FPU's inline exponential,
  0x5F9696..0x5F96AA), kept to 0.1..1; when the smoothed move is not nothing, each sprite on screen (`ProjectPoint`
  0x819390 gives 2) clamps the move to ±20 and, within a tenth of the width both ways, is pushed by
  4 (1 − distance / that width) of it at its own depth, its velocity += (new − old) dt 100. Then each sprite: velocity
  −= (place − home) dt 10.01, ×= exp(−dt), place += velocity dt, its alpha, its size, a new angle `Random(0, 2π)`, drawn
  at once in that order. The name (j0, size H / 20, centred at 0.71428573 H) white at clamp(t − 22, or past 33
  1 − (t − 33), 0..1); while that alpha is above 0, the trademark sign "TM" (the default picture only, at
  (W 0.64, H 0.32), size W / 70, grey 200 at the sprites' alpha), the Pentium 4 boxes (below) and the force-feedback
  mouse's box (not ported: haptics).
- **The Pentium 4 boxes 🎯 chosen difference (user, 2026-11-10): never shown in openblack** ([chosen differences](../project/reconstruction.md#what-is-deliberately-different-chosen-differences)). With `LH3DP3::IsPentium4` and `g_b_p4_improvement` (1, 0x643064), two boxes of `p4.raw`
  and `p4t.raw` at the bottom left. `IsPentium4` comes from the Intel compiler's CPU indicator
  (`___intel_cpu_indicator_init` 0x8A2610): CPUID's base family 15 with SSE2 gives 0x200. A modern Intel processor
  (family 6) gives 0x80 and no boxes; AMD processors since the K8 report base family 15 and draw them. 🔍 not ported
  yet.
- **The C runtime's draws.** 3 a sprite as it starts (12288), 1 a sprite each frame, from the stream every other CRT
  user shares ([Random numbers](random.md#the-c-runtime)).
- **openblack**: `in_loop_logo` (`src/Gui/InLoopLogo.h`: the rules, tested with fakes), `graphics::InLoopLogoDraw`
  (`src/Graphics/InLoopLogoDraw.h`: the sky, the stars, the name, the trademark and the fade into a picture of its own
  whose colour is kept from frame to frame, copied to the window) and `Game::PlayInLoopLogo`
  (`src/GameInLoopLogo.cpp`), from `Game::StartRun` after the land's load and before the first frame, once a run of
  the program. Tests `InLoopLogo.*` (`test/gui/test_in_loop_logo.cpp`; the quicksort's order pinned to the exe's own
  on 230 sprites, run on the oracle). `--skip-logos` (every run of the checks) and `--skip-in-loop-logo` leave it out;
  the debug bar's Start-up and Loading menu shows whether it played and plays it again. A recording made while it
  played keeps its frames (`input::recording::Kind::Logo`); a replay shows no logo but takes the same CRT draws.
  The sines, the cosines and the exponential are worked as the FPU works them (`Common/X87Precision.h`). Not ported: the Pentium 4 boxes (the user's 2026-11-10 chosen difference) and the force-feedback box (haptics are ignored).

#### When there is a turn

`LocalTimerSaysDoATurn` 0x54C4A0, called from `ProcessNetworkPackets` 0x54CD45:

- It is due when `MSeconds ≥ turn · 100`, with the 100 hard-coded (0x54C4F0). The comparison is **absolute**, so the
  leftover of one turn carries over to the next.
- When paused (single player), never (0x54C528).
- With more than 2000 ms (0x7D0) of lag it calls `ResetLocalGameTimer` 0x54C570, which sets the timer to `turn · 100`
  from now (0x54C615). The answer of that call is that of the earlier sample (`setge` 0x54C567).
- At most **1 turn per frame** in single player (10 in network play): `neg; sbb; and 9; inc` at 0x54CD0F..0x54CD18.
  The loop asks the timer first and then the cap (0x54CD52), so after the last turn of the frame the timer is queried
  once more. `++NetworkTurnsThisFrame` is at 0x54CD93 (at 0x54CE58 is the call to `ProcessOneGameTurn`).

#### The turn number

The turn g_game +0x205A40 goes up **when the turn starts** and only when not paused (`GGame::StartTurn` 0x54E4FD..0x54E507), before `ProcessTurn` 0x54E5C0 and `EndTurn` 0x54E960. During the turn the whole game already
reads the new number.

#### The frame clock

`GGame::Loop` 0x54D2A8..0x54D3A6, after the turns and before drawing:

- Not paused: `Δ = MSeconds − previous sample`; with the same turn `remainder += Δ`; with a new turn
  `remainder += Δ − 100` (0x54D316); then the remainder is clamped to 0..99 (0x54D325..0x54D337).
- `visual = turn · 100 + remainder` (0x54D343); if it is lower than the previous one, that lower value is stored
  (0x54D350: the clock **does go backwards**) and `remainder = 0` (0x54D356); what is never negative is the dt, which
  is 0 that frame.
- `g_game_time_inc` [0xEA9EC0] = g+0x250540 = g+0x205D48 = `visual − previous` (0x54D366 / 0x54D374 / 0x54D380):
  **whole ms**, at most 199, which follow the speed. The visual clock is g+0x25053C.
- The **fraction** g+0x205D64 = `remainder · 0.01` [0x8C4B10] (0x54D392): it goes from 0 to 0.99 and lags one turn
  behind.
- When paused only the dt is 0 (0x54D39A); **the fraction is preserved**.
- `NetworkTurnsThisFrame` goes back to 0 after drawing (0x54D3C3).
- Readers of [0xEA9EC0] that do `fild g_game_time_inc`: `GLandAlignement::DrawSky` 0x5E2160, fn_005E25C0 0x5E25FD,
  fn_00823460 0x8234B6, fn_00823570 0x82359F, fn_007F8E00 0x7F8F25 (the smoke clips to 100 **s**) and fn_007FA300
  0x7FA3BE; `Field::Draw` reads it at 0x5286D7 (its Zoomer at +0xE8 advances by `g_game_time_inc · 0.001f`, the
  constant 0x8AC418 = 0x3A83126F, so fields grow in game seconds); `Tree::PreDraw` 0x74A7C0 reads the same value from
  g+0x205D48 (0x74A7D6, 0x74A81B), adding it into [0xDA579C] (reset at 0x74A879); the sea at 0x879963 and 0x87A130, and the sea's frame counter
  [0xFA938C] (`(frame + 1) & 15`) advances only if `g_game_time_inc != 0` (0x879B0A / 0x879B41).

#### The wall clock

`g_delta_time` [0xC38134] is another `LHTimer` (`LH3DTech::g_timer` 0xEA1B78), read in `LH3DRender::StartFrame` 0x82F14E: ms of the frame, 1 if it comes out ≤ 0 (0x82F195), and it does not stop when paused.

- Its static constructor (fn_008189F0, in the `__xc_a` table at 0x9C7D60) leaves it **stopped** (speed 0, saved 1).
- `LH3DTech::RenderInitialization` 0x818C61..0x818CA3 (called by `LH3DRender::Open` at 0x82B540) starts it: factor
  1e-5, `elapsed = MSeconds` (≈ 0), base = `GetTickCount`, factor = the saved one (1). It therefore counts the ms
  **since the engine starts**, not since the machine starts. The help texts read it at 0x5C6250.
- `SetSpeed` 0x5537F0 also sets [0xD00DA8] = 0 in both branches (0x5538AD, 0x5538C8); that address is only written
  in the whole exe (also in `GNetwork::ProcessOnePacket` 0x634B40), nobody reads it **(inferred: no indexed reader has
  been seen)**.

#### The system clock's seconds

The time played is not on either timer: `GGame::Init` ends with `time()` into `GGame +0x59AC` (0x54FD15), and
`GGame::Close` 0x54ECA9..0x54ECC7 adds `time()` − +0x59AC to the profile's `timestarted` (+0x808) before
`WriteBackToRegistry`; the save game room shows the same sum live
([the save game room](../interface/temple-interior.md#the-save-game-room)). Whole seconds of the C runtime's
`time()`, as a 32-bit count.

#### The turn length and the conversions

- [0xD01A38] is the turn ms that the logic reads: set to 100 by `GGame::Init` 0x54F4A5, changed by
  `SET_GAME_TICK_TIME` 0x714DBE. The scheduler and the frame clock do not read it: they use the literal 100 (0x54C4F0,
  0x54D316, 0x54D343, 0x54C615, 0x5550A3, 0x553810).
- The turn seconds passed by hand: `push 0x3DCCCCCD` (0.1f) in `ProcessTurn` 0x54E5D1, 0x54E6C3, 0x54E775.
- Readers of [0xD01A38] every turn: the fireflies (`FireFly::Process` fn_0052AF90, 0x52AF93: `fild [0xD01A38];
  fmul 0,001`), the hand wind of the chimney smoke (fn_005DBC60 0x5DBD4E: `1000 / [0xD01A38]`), the spell sounds'
  turn seconds (`spell_sounds` fn_006D11A0 0x6D11AB..0x6D11C5) and the hand grain's
  (`CHand::GameTurnUpdate` 0x46E4E3..0x46E4FB).
- Seconds to turns: `ftol(1000 / [0xD01A38] · s)` (an integer division) in `NumGameTicksPerSecond` 0x711630 and inline
  at 0x70CCDE, 0x711338, 0x5C61F6 and `GetTicksToChangeOver` 0x66CD00. With [0xD01A38] = 0 the original would fail at
  the `div` (0x711635, unchecked). The name of 0x711630 is misleading: it is actually the `SetTime` of a script timer
  (it stores the turn in +0x28 and the turns in +0x2C, fn_00711610); the conversion is the same as that of the other
  four.
- `GetCameraTimeInc` 0x555820: the game dt when playing back the recorded interface, otherwise the wall one.
  fn_005557E0: the wall dt in the temple, the game dt outside; ≤ 0 → 0, cap 500.
- `ResolveLoad` 0x555080 (loading a game): dt and fraction to 0, `visual = turn · 100` (the Loop statics are not
  touched).
- Loop 0x54CF6B..0x54D003 and 0x54D1F7: the start of a game, timer from 0, dt and fraction to 0,
  `ResetLocalGameTimer`.

#### Pause

`PauseGame` 0x54AE20 (0x54AE7C..0x54AEE7) sets the flag g+0x14 bit 2 and stops or restarts the timer. While paused
inside the citadel, `ProcessNetworkPackets` 0x54CC88..0x54CCCA runs the temple's turn every 100 ms of the ticks (the
static [0xCD3AE8]). Others that unpause with `PauseGame(0)`: `Continue` 0x53FF00 of the Escape menu (hides the menu
and unpauses; the menu keeps the game paused while it is open) and the SkipBox callback, after applying the answer
[0x54458B]. g_game+0x205A28 (bw1-decomp's `GGame::ViewMode`, the sequence mode: 1 = inside the citadel, 2 = the falling spell, see
[The falling spell](../video/playback.md#the-falling-spell-fallbik)) is set to 0 by `GGame::Init` [0x54FCA4]. Its
only writers (every `+ 0x205A28]` operand of .text, 2026-10-22; no `lea` of it, and the `GLandscape` at g_game
+0x205A20 never writes its +8): `GGame::Init` 0 (0x54FCA4), `KickOffFallingSpellVideo` 2 (0x5539D5),
`EndFallingSpellVideo` 0 (0x553A1C), `LeaveInsideCitadel` 0 (0x553B1F) and `GoInsideCitadel` 1 (0x554004). So a land
change (`ClearMap`, `GGame::LoadMap`) keeps it. openblack: `Game::PrepareNewLand` sets none only at a new game
(`land_load::Resets::newGame`, #206); the temple's exit before it leaves the
citadel's mode as `LeaveInsideCitadel` does.

#### How the speed acts

- The speed is **one factor that multiplies the game timer at its source**: 1 normal, 2 twice as fast, 0.5 half.
  `GGame::SetSpeed` 0x5537F0 stores the requested value at g+0x205B74 (0x553800, what the indicator and the keys
  read) and gives it to the timer: running (factor ≠ 0, 0x553835, `SetSpeedUpFactor` inline), it **rebases**
  (elapsed = `MSeconds`, base = `GetTickCount`, factor = speed, 0x553859..0x553894), so the time already gone keeps the
  old speed and only the time to come runs at the new one; stopped (paused), it only keeps it as the saved factor
  (0x5538C2) for when the timer starts again. In a network game it also sets the idle period to `ftol(100 / speed)` ms
  (0x553810..0x553828).
- `MSeconds` is the only place the factor acts, and three readers take the same scaled sample: the turn timer
  (`LocalTimerSaysDoATurn`, above), the frame clock (`GGame::Loop` 0x54D2B2..0x54D2E2) and the restarts (`StartGame`
  0x54C1F9..0x54C2C4, `ResetLocalGameTimer` 0x54C570). Nothing divides by the speed in single player: there is no
  `dt · speed` and no `fraction · speed`; the turn, `g_game_time_inc` and the fraction all follow it because they all
  read that clock.
- **The turn stays 100 game ms** (the scheduler's literal 100 and [0xD01A38] = 100): a faster game has more turns per
  wall second (10 · speed), never longer or shorter turns. With **at most one turn per frame** (0x54CD0F..0x54CD18),
  once `frame wall ms · speed > 100` the turns cannot keep up: one turn a frame, the remainder held at 99,
  `g_game_time_inc` about 100 a frame, and the lag grows until the **2 s lag reset** (`ResetLocalGameTimer` from
  0x54C553) drops it. At 33 ms frames speed 2 (66 ms a frame) keeps up; 4 (132) would not.
- **The script**: `SET_GAMESPEED` 0x6FAAE0 passes the popped float straight to `SetSpeed` (0x6FAB1B..0x6FAB29), any
  value, only for the task that took it with `START_GAME_SPEED`; `END_GAME_SPEED` (`ActualEndGameSpeed` 0x6FAA60,
  `SetSpeed(1.0)` at 0x6FAA8E) and a stopped owner task set 1 ([The Land 1 intro](../scripts/land1-intro.md)).
- **The hand demos** (the interface's playback) set speed 1: at 0x5DACDE (with the wide screen), at `StartPlayBack`
  0x5DADEE and on every record read in fn_005DAEE0 (0x5DAFC4).
- **Who writes the speed, and nobody else**: the GGame ctor (timer at 1, 0x54B58A), `DoNetworkStart` 0x54CEDD (the
  stored value only, 1.0), `SetSpeed`'s six callers (three of the playback, the keys, `ActualEndGameSpeed`,
  `SET_GAMESPEED`), and the restarts that keep the saved factor (`StartGame`, called once from `pc_main` at 0x641EF4;
  `ResetLocalGameTimer`; `LoadAllGame` 0x558C0C..0x558D1B; the network packets of `GNetwork::ProcessOnePacket`
  0x634AA7, 0x634E04, 0x634FF0, network games only).
- **A new land keeps the speed**: `LOAD_MAP` → `GGame::LoadMap` 0x552F40 → `ClearMap` 0x552BB0 / `GGame::ResetState` 0x5557A0
  touch neither the stored speed nor the timer factor (no reference to g+0x205B74, g+0x205D68 or its factor
  outside the list above). Only a script that owns the speed and is stopped on the way (`LandControlAll`'s
  `STOP_ALL_SCRIPTS_IN_FILES_EXCLUDING` before each `LOAD_MAP`) gives it back to 1, as any stopped owner does.

#### The speed keys

`GGame::ProcessBindableKeys` 0x63F830, called from `GGame::ProcessKey` at 0x63F30E:

- ProcessKey passes one modifier with the key, by priority Shift (0x2A), then Ctrl (0x1D), then Alt (0x38, either
  side) (0x63F2C6..0x63F303).
- A key and modifier bound to an action go to `DoAction` first (fn_00470A00: the key and the modifier must both match,
  left and right alike, `CompareKeys` 0x46F360; a binding without a modifier does not match Alt and the key). The
  default bindings (`ControlMap::LoadDefaults` 0x46F890) have nothing on 1 or 2.
- Otherwise **Alt+1** (`KB_1` with `KB_LALT` 0x38 or `KB_RALT` 0xB8) sets `SetSpeed(s · 0.5)`, or 0.5 if that is below
  0.5 (0x63F880..0x63F8B5), and **Alt+2** (`KB_2`) sets `SetSpeed(s + s)`, or 2 if that is above 2
  (0x63F8C9..0x63F8F0), both through `SetSpeed` (0x63F8FB). From the keyboard the speed is therefore 0.5, 1 or 2.
  Either returns 1, so ProcessKey stops: no bookmark.
- Key-downs and auto-repeats both reach it (the key buffer drops a repeat only within the same frame, 0x5E1C12). No
  pause or wide-screen gate; inside the citadel the keys go to the citadel's own handler (0x63F295) and never get here.
- The bookmark keys: only 1..7 reach the bookmark case (the key pass's byte table at 0x63F6FC gives 3, nothing, to
  KB_8..KB_EQUAL: [the player camera's bookmarks](../camera/player-camera.md#the-bookmarks-markers-in-openblack)); Ctrl
  (without Alt) sets one, Shift toggles its selection (fn_0043A390), and Alt with 3..8 does nothing
  (0x63F46B..0x63F61F). The player profile's own bindings (`ControlMap::LoadCurrentProfileKeys` 0x4713D0) were not
  read.

#### The speed indicator

fn_005CBF10 (unnamed in the symbols; it starts right after fn_005CBEC0) is called once a frame by HelpText's
FinishFrame callback fn_005CCAB0 (0x5CCC79), right after the dialogue texts, so never while a film is open (the
callback leaves early then). It writes the game speed's words near the top of the screen:

- **The state** (0x5CBF10..0x5CBF76): paused (g+0x14 bit 2) is 0 whatever the speed; else g+0x205B74 below 1 is 1
  (slow), above 1 is 2 (fast), anything else −1 (none).
- **The fade** [0xD17CB4] and **the words shown** [0xBF1A28] (0x5CBF78..0x5CBFF0): while the state is the one shown
  and not −1 the fade goes up `g_delta_time · 0.003`, else down `g_delta_time · 0.002` (`g_delta_time` is whole real
  milliseconds, read as signed by `fild`). Above 0 it is clamped to 1; at 0 or below it is set to 0 and the words
  shown become the state. These two and the tumble's phase are statics, never reset.
- **The gates, in order** (0x5CBFF6..0x5CC064). Nothing, and no reset, when the words shown are −1 or there is no
  game (g+0x250538 = 0). Then a **reset** (words shown = state, fade 0, at 0x5CC736) when the sequence mode g+0x205A28
  is not 0 (0x5CC00C); when `GetCamera()+0x70` is not 0 and the state is not paused (0x5CC01A..0x5CC028; the camera
  flag set and cleared together with the game speed's owner); when [0xD00DCC] is not 0 (0x5CC02E); when the video
  player g+0x250188 is not 0 (0x5CC040); and when `SetupBox::GetCurrentActiveBox()` has a box whose +0x94 is not 0
  (0x5CC04E..0x5CC064). +0x94 is `SetupBox::BackgroundStyle`
  ([KMIcon](../interface/tooltips.md#kmicon-the-button-or-key-drawing)).
- **[0xD00DCC]** makes `ProcessFrameInputs` call `EditorProcessInputs` (0x54C35E) and `ProcessGraphicsEngine` draw
  `BMan_Display`, `DrawMouseCross` and `GCameraEditor::FinalDraw` (0x54D893..0x54D8A6): the developers' editor mode.
  It is bw1-decomp's `GGlobal::EditorMode`: the constructor (0x590F5A) and both run-time writes (0x54CCEB, 0x54E9E7)
  clear it, and the only setter (0x590F80, which also sets the object [0xD00E04]) is never called, so in v1.42 it is
  always 0 and the editor branches never run [V].
- **The text** (0x5CC06A..0x5CC114): t = fn_005BD250(fade), the eased fade (1 − cos(fade · π)) / 2 clamped to [0, 1];
  nothing when t is 0. Help text 0x127B + the words shown (4731 `HELP_TEXT_GAME_PAUSED`, 4732 `_SLOWMO`, 4733
  `_FASTMO`) up to its first null (`_wcslen`), spaces counted; depth (t + 1) · 15, so 15 when faint and 30 at full.
- **The layout** (0x5CC16D..0x5CC3FD): the pitch is the `acos` (0x5CC350) of the angle between the unit rays from the
  eye to the screen's middle and to (W / 2, H / 20), both from `Get3DPointFromScreen` at depth 10; letter i of n is at
  column trunc((W / 2 − (n − 1) · s / 2) + i · s) (`__ftol` 0x5CC3D2) with s = (W / 2) / n / depth · 30, so the word
  spans half the screen at depth 30; each letter's origin is `Get3DPointFromScreen` of (column, H / 20) at the depth.
- **The tumble**: the phase goes on `g_delta_time · 0.0015` only on frames that draw, wrapping once past 4π; letter i
  is i / 3 behind. Below 2π it is a turn about the letter's horizontal axis, else about its vertical axis after taking
  2π off, folded into [−π / 2, π / 2); then a turn about x by the pitch, then by π. The turn's sine and cosine are
  stored to float, the pitch's kept at full precision; the half turn uses the double of float π.
- **The draw** (0x5CC65F..0x5CC719): each letter by `GatheringText::DrawTextRawOriented` in font j0, size 2, stretch
  1, offset (−w / 2, −1, 0) with w from `GetStringWidth` at size 2, white at alpha trunc(t · 255) (`__ftol` 0x5CC6DA),
  across axis 0 and down axis 1; after the end of the frame's Z reset, depth tested LESSEQUAL, no depth write.

#### Clocks of other systems

- **TownBelief** (read; openblack is faithful, see [openblack](#openblack)): `TownCentre::DrawAll` 0x7447F0, from `Process3dEngine` 0x54E032 on
  **every drawn frame (also when paused)**, calls `ProcessPSys` 0x69BCC0 → `GJPSysInterface::Process_` 0x673690,
  which passes as ms **[0xD01A38]** (the ms of one turn, 100) and not those of the frame → fn_00673300 → fn_00673340:
  dt [0xD4E0EC] = ms · 0.001 = 0.1 s (0x673402..0x67340C). So the original advances phase, angles and fight 0.1 s per
  frame (it depends on the fps). Also: `fmod 2π` of phase, a1 and a2 (0x69C4B7 / 0x69C4F0 / 0x69C50B, the double
  [0x8D45D8]); the order of DrawAll (list g_game +0x205CFC, the ctor 0x743AC3 inserts at the head: newest first); the
  centre's state is made in MakeFunctional 0x743F18 / ResolveLoad 0x7448D8, and DrawAll checks `IsAvailable`
  (0x744808). The glows (`PlayerSymbolSprite::Draw` 0x69D7E0) add `g_game_time_inc · 0.002` (0x69D855), once per
  frame like Draw_(1) 0x69BF19.
- **An effect's first step is double**: an effect's first `Process_` steps twice (+0xAD = 1 at init in fn_00672B50
  0x672BF7, used by fn_00673300), and `CreatePSys` already calls it (0x69BC95).
- **Per-turn physics**: `PhysicsObject::GameTurnUpdate` runs once a game turn with its 20 substeps and the bodies are
  drawn interpolated with the turn fraction (fn_00646FE0 / fn_007FCE80; [Physics: manager](../physics/physics.md#manager)).
- **Own fractions read the game's**: HandGrain (0x5B2D41 / 0x5B2D61), the PSys (0x67370D) and the fireflies (0x52ADF6)
  read the turn fraction; the shields' draw (`PhysicalShield::DrawShield` 0x72CEEC / 0x72CF01) reads g_game +0x205D64.
- **Turn readers in the effects**: the fire's steam and smoke bursts (fn_00731AB0 0x731AF8 / 0x731B27, fn_00731E50
  0x731E7B / 0x731EAE) and the spell seed's graphic (fn_00727350 0x72736E) read the turn number.
- **The help texts**: the reading seconds (0x5C6211..0x5C6225) and `ShownLongEnough` (fn_005C68C0) are computed in
  float (FPU at 24 bits); `ReadSpeedFactor` is fn_005C6CB0; the end time is `seconds · 1000 + now`
  (0x5C6279..0x5C629B, `fmul; fiadd` at 24 bits).

#### The fireflies' turn

The fireflies run on the game turn and draw on the synchronised stream ([Night lights](../world/day-and-night.md#night-lights)
holds when they come out and how they look). Each night they may spawn once: the flag `[0xBE9DA0]` is set again every
morning.

- **The spawn** (fn_0052B200): (max − count) attempts (0x52B215..0x52B223, the max read again each time). Each
  `GameRand(2)` (0x52B235): non-zero takes the `GameRand(count)` (0x52B25F) tree of the game's tree list, and an empty
  list ends the spawn (0x52B24E); zero takes the `GameRand(count)` (0x52B2CC) object of the multi-map fixed list, then
  the first rock from there (`IsRock` vt +0x1F0 at 0x52B304, 0x52B2FF..0x52B312); an empty list ends the spawn
  (0x52B2BF) and no rock from there makes nothing this attempt. A tree or rock is fn_0052B1A0 (`IsRock` or
  `IsAnyKindOfTree`). `IsRock` is 1 for a rock, the bonfire and a fragment (`Rock::IsRock` 0x439730 in the three
  vtables) and 0 for a dead and a felled tree (0x5109D0), which are trees by `IsAnyKindOfTree` (vt +0x478). So the
  fireflies start at, perch on and wake from bonfires too; Land 1 makes two (`CREATE_BONFIRE`).
- **The making**: `FireFly::Create` 0x52A200 → the constructor 0x52A280 → fn_0052A380 makes eight `GameFloatRand` in
  this order: the two speeds `GameFloatRand(0.8) + 0.6`, then six `GameFloatRand(2π)` phases, two of which (+0xB0 and
  +0xBC) are drawn and never read. The new firefly goes to the **head** of the list (0x52A2C8..0x52A2DD).
- **Waking** (fn_0052B350): only the list head, and only when it is resting. Its tree or rock must still be there, an
  object of its cell's fixed list (`GetFirstObjectFixed` 0x52B388) that is fn_0052B1A0 with its own map coordinates,
  else it is deleted (0x52B3FB). The match is `MapCoords::operator==` 0x605660: x and z equal and the altitudes
  equal (`fcompp`, C3; two NaN altitudes would count as equal), which openblack's `MapCoords ==` matches on every
  number. The same walk (the cell's next by vt +0x53C) deletes it at once when it meets another `FireFly`
  (`__RTDynamicCast` to FireFly 0xBE9DC0, 0x52B3B1..0x52B3D9) with its map coordinates; that never happens, as a
  firefly is never in the fixed list: `FireFly` derives from `Object` (0x52A28D), whose constructor clears +0x24
  (0x636577) and nothing of the firefly sets bit 15 again, so `Object::InsertMapObjectToCell` 0x636830 puts it at the
  head of the mobile list (0x636834). It then flies (fn_0052A950) to the abode or lantern found by fn_0052A670, raised by
  its height + 2 (fn_0052A5D0, 0x52A605..0x52A60B), or with none 15 m along x at altitude 4 (fn_0052A630, 0x52A917);
  it is awake and goes to the tail.
- **Going to rest** (fn_0052B4C0): only the list head, and only when it is awake. It flies (fn_0052AA10) to the tree
  or rock found by fn_0052A7A0 (fn_0052A8D0, 0x52A8D5), or with none 15 m along x at altitude 0; it rests from then on
  and goes to the tail. Moving the head to the tail is 0x52B43C..0x52B4B3 and 0x52B51C..0x52B591.
- **The search** (fn_0052A670 for the lights, fn_0052A7A0 for the perches; the 300 m radius at 0x52A5D6 and 0x52A8D5):
  a spiral over `ftol(ceil(2r / 10))²` cells (0x52A673..0x52A6B4, the ceiling on the double) from the start's cell. Each
  cell in bounds is searched only when `GameRand(2) != 0` (0x52A6ED / 0x52A81D); in its walk of the cell (0x52A701,
  0x52A75E) an object that passes is taken when it is nearer than the best so far (or the distance is unordered) or
  there is none yet, and then `GameRand(3) == 0` (0x52A747 / 0x52A877) ends that cell. The radius only sizes the
  spiral: nothing is cut by distance.
- **The draw**: `FireFly::Draw` fn_0052ABC0 reads the turn fraction (0x52ADF6, above).

### Sources

- `runblack.exe` (disassembly and the xref index); bw1-decomp `src/Black/Game.cpp` for the loop.

## openblack

**Faithful and ported** in `src/GameClock.{h,cpp}`, namespace `openblack::game_clock`; tests in
`test/test_game_clock.cpp` (and `test/test_speed_indicator.cpp`). The game starts it in `LoadMap`, decides the turns
in `Update` and computes the frame clock right afterwards. Before, the clock was written about 37 times in the code,
with different fidelities.
`Game::Game` starts the clock (`InitializeClock`) as the GGame ctor does (0x54B58A, speed 1); it stays paused until a
map is loaded. `OPENBLACK_FIXED_FRAME_MS` gives the same ms every frame (`Debug/FixedClock.h`).

| API (`game_clock`) | Original | What it does |
|---|---|---|
| `k_MsPerTurn` = 100, `MsPerTurn()`, `SetMsPerTurn()` | [0xD01A38] | the turn ms that the logic reads |
| `k_SchedulerMsPerTurn` = 100 | the literal 100 | the 100 of the scheduler and of the frame clock (does not read [0xD01A38]) |
| `k_TurnSeconds` = 0.1f | the `push 0x3DCCCCCD` | the turn seconds passed by hand |
| `k_MaxLagMs`, `k_MaxTurnsPerFrame` | 0x54C553, 0x54CD0F | 2000 ms; 1 turn per frame |
| `Timer` (`ElapsedMs`, `Stop`, `SetSpeed`, `Start`) | LHTimer 0x43EB70 / 0x43E9C0 / 0x43EBC0; start 0x54CF93 | the timer |
| `Turn()`, `SetTurn()` | g+0x205A40 | the turn |
| `IsTurnScheduled()`, `TurnDue()`, `StartTurn()`, `ResetLocalTimer()` | 0x54C4A0, 0x54CD45, 0x54CD93 + 0x54E507, 0x54C570 | the scheduler |
| `Start(paused)` | Loop 0x54CF6B..0x54D003 and 0x54D1F7 | timer from 0, dt and fraction to 0, `ResetLocalGameTimer` |
| `OnLoad()` | `ResolveLoad` 0x555080 | dt and fraction to 0, `visual = turn · 100` |
| `Pause(bool)`, `IsPaused()` | `PauseGame` 0x54AE20 | the flag and the timer stopped / restarted |
| `PausedTurnTimer::Due(now)` | the static [0xCD3AE8] | the temple's turn every 100 ms while paused inside the citadel |
| `SetSpeed(v)`, `Speed()` | `GGame::SetSpeed` 0x5537F0 | the speed factor; the time already elapsed keeps the previous speed |
| `UpdateFrameClock()` | `GGame::Loop` 0x54D2A8..0x54D3A6, 0x54D3C3 | remainder, visual clock, dt and fraction |
| `FrameGameMs()`, `FrameGameSeconds()` | [0xEA9EC0]; `· 0.001` [0x8AA3B0] | whole game ms of the frame |
| `TurnFraction()` | g+0x205D64 | the turn fraction |
| `VisualMs()` | g+0x25053C | the visual clock |
| `StartEngineTimer()` | `RenderInitialization` 0x818C61..0x818CA3 (`Reset()` leaves it stopped like fn_008189F0) | starts the wall clock from ≈ 0; called by `InitializeEngine` (Locator.cpp) when creating the renderer |
| `UpdateRealClock()`, `FrameRealMs()`, `EngineMs()` | `StartFrame` 0x82F14E..0x82F195; `g_timer` 0xEA1C78..0xEA1C80 | the wall clock |
| `CameraFrameMs(playingBack)` | `GetCameraTimeInc` 0x555820 | game dt when playing back the recorded interface, otherwise the wall one |
| `ClampedFrameMs(inTemple)` | fn_005557E0 | wall in the temple, game outside; ≤ 0 → 0, cap 500 |
| `TimeSystemInterface::GetWallTime()`, `machine_clock::WallTime()` | `time()` at 0x54FD15 and 0x54ECA9 | the system clock's seconds since 1970 for the time played; with `OPENBLACK_FIXED_FRAME_MS` the deterministic date (1 January 2001, 12:00 UTC) moved on by the fixed step's ticks, which no land's restart takes back, so the checks read the same; tests `RunSeed.TheWallTimeIsTheWallClocksWithoutAFixedStep`, `WithAFixedStepTheWallTimeMovesOnByTheSteps` |
| `TicksForSeconds(s)` | `NumGameTicksPerSecond` 0x711630 and the inline copies | seconds → turns; with a turn length of 0 openblack gives 0 **🔍 inferred** where the original would fail |
| `TickCount()` | the audio's tick count | one copy for `audio::TickCount` and `MusicStream` |

[0xD00DA8] is not ported (nobody reads it).

- **Speed.** `Game::SetGameSpeed(m)` still receives the turn-duration multiplier (2 slow, 0.5 fast) and calls
  `SetSpeed(1 / m)`. A new land keeps the speed, as in the original (`Game::StartNewLand` does not set it; the
  testbed's `examples.speed_across_land` shows it).
- **The speed keys**: `input::SpeedForKey` (src/Input/SpeedKeys.h) with `SlowerGameSpeed` and `FasterGameSpeed`,
  called at the start of `Game::ProcessEvents`' key-down, before the bookmark keys, and not inside the temple.
- **The speed indicator**: `help::SpeedIndicator` (`Help/SpeedIndicator.h`), filled in `FillOverlayFrame` and drawn by
  `Renderer::DrawSpeedIndicator` in the screen overlay right after the help text. The script owning the speed is the
  script control's owner (the camera flag agrees with it without a saved game); the editor mode is always off, as
  openblack has none. The active box's gate is `GameInterface::IsBoxActive` (the menu open or the SkipBox shown):
  +0x94 is `SetupBox::BackgroundStyle`, not 0 for every box openblack has.
- **The fireflies** (`src/ECS/FireFlies.cpp`, [above](#the-fireflies-turn)): `ecs::ProcessFireFliesTurn` runs
  their turn and `ecs::UpdateFireFlies` draws them with `TurnFraction()`. Its helpers, one per step of the original:
  `Spawn`, `Create`, `WakeOne`, `SleepOne`, `StartFlight` (the flight out or home), `HeadToTail`, `SpiralSearch`
  (with `k_SearchRadius` 300 m for both searches), `Aside` (the 15 m fallback), and `TreeList` /
  `MultiMapFixedList` (the game's two lists, newest first). fn_0052B1A0 is `ecs::IsFireFlyPerch` and the spawn's
  first rock `ecs::FireFlyRockFrom`, both through `object_physics::IsRock` (a rock, the bonfire, a fragment) since
  they asked a rock's own test, which left the bonfire and fragments out (tests
  `FireFliesTest.AFireFlyPerchesOnARockTheBonfireAFragmentOrAnyTree` and
  `FireFliesTest.AFireFlyNotAtATreeStartsAtTheFirstRockOfTheFixedListFromItsDraw`).
- **Fields**: openblack differs: `FieldSystem::Update` gets the wall frame time instead of the game ms · 0.001
  (`Game::PlayFrameStep`); see [Pending](#pending).
- **TownBelief**: `town_belief::Step()` (Game.cpp, next to the other frame steps) does the step and the local GRand
  draws once per frame with `MsPerTurn() · 0.001`, and `Collect` only reads the state; the glows advance in the same
  `Step`. Before, the step ran in `Collect`, which runs when drawing and goes **twice per frame** (CollectSorted and
  CollectQueued): twice as fast and twice as many draws. The draw order is by creation index (the original's list is
  newest first), leaving out unavailable centres (`ecs::IsAvailable`, as the original's draw). **⚠️ approximate**
  openblack creates the centre's state the first frame it sees it, not when the centre becomes functional or is loaded.
- **The animals' clips** (`ecs::UpdateAnimalAnimations`, `AnimalGroundSpeed` in `ECS/AnimalAnimations.h`): while an
  animal moved during the last turn its clip plays by its whole speed in map units a turn, otherwise by the frame's
  game ms, as the original ([the clips advance by the ground](../beings/animation.md#the-clips-advance-by-the-ground));
  the former `movedLastTurn · 10` step is gone.

### What was fixed

Before the port each system had its own clock:

- **The turn leftover.** `GameLogicLoop` did a turn if 100 ms had passed since the previous one and set the mark at
  the frame of the turn: at 30 fps turns lasted 133 ms (25 % slower). Now 3 s at 33 ms per frame give 30 turns.
- **The turn goes up at the start.** `_turnCount` went up at the end; during the turn one less was read than in the
  original.
- **The pause.** It did not stop the clock: when unpausing it did a turn in the same frame and the fraction jumped to
  0.99. The fraction was 0 while paused; now it freezes.
- **The frame order.** The turns go before the frame clock and before everything that moves per frame (villagers,
  animals, sharks, ship, rings, fish, fireflies, widescreen, magic), as in `GGame::Loop`. The ten copies of
  `_paused ? 0 : dt / mult` in Game.cpp read `FrameGameMs()` / `FrameGameSeconds()`: whole ms, 0 when paused, ≤ 199.
- **The sea.** `RendererSea.cpp` took as dt the time since startup (`desc.time`, which is never rebased) and
  `ScrollRows` accumulated it again: the sea scrolled faster and faster. Now it reads `g_game_time_inc`'s counterpart,
  and the sea frame counter advances only when the frame's game ms are not 0.
- **Own fractions.** HandGrain, the PSys and the fireflies used the wall clock or their own, without speed or pause
  and with a cap of 1. Now they read `TurnFraction()`; so does `MapShield.cpp`'s `DrawShields` (`g_LastTurn` and its
  wall clock deleted).
- **Own clocks.** HandFish and HandResources (`ProcessInInteract` once per turn) counted turns with the hand's real
  dt; now they count the game's turns. The fragments of broken buildings (`Fragment::ProcessTimer`, called by the turn
  at 0x54E768) run from `Game::GameLogicLoop` and not from the physics with a `static` of the turn (which was not reset
  when loading a map), and the physics dt is the game one. MissionaryBoat (was PetitNavire; `g_carry`) and the sharks
  (`s_Clock`) reconstructed the whole ms: they now arrive whole.
- **Conversions.** The dispensers, the end of the help texts and the ramp for picking up from a pile (before in float
  and without truncating) use `TicksForSeconds`. Chants and help read `MsPerTurn()`. `SET_GAME_TICK_TIME` (it used to
  throw an exception) writes the turn length and nothing else. The fireflies and the chimney smoke's hand wind read
  `MsPerTurn()` every turn (before, the constant `k_MsPerTurn`, which `SET_GAME_TICK_TIME` did not reach). `Chants.h`
  only has the default value; `Spell.cpp` already fills it with `MsPerTurn()`. In `MagicLoop.cpp` the turn seconds of
  `spell_sounds::ProcessTurn` and `hand_grain::GameTurnUpdate` are `MsPerTurn() · 0.001f`. `atmos::UpdateGame`
  (WeatherLoop.cpp) uses `k_TurnSeconds` instead of a hand-written 0.1f.
- **The magic's turn length** (`magic::k_TurnMs`, the constant 100, gone): the spell's age and its effect's step
  (`Spell::ProcessMaintainRequest` 0x7204D3 and the PSys `GJPSysInterface::Process` 0x673690 read [0xD01A38]), the
  storm's cast effect, the seed's wait in the hand (`SpellSeed::ProcessInHand` 0x72999F), the flocks' emit total
  (`SpellFlockFlying/Ground::Process` 0x723C2F / 0x724311), the dove's and wolf's fade (`SetDying` 0x41F5EB /
  0x420D1B, `ProcessFadeOut` 0x41F4C3 / 0x420BF3), the shields (`PhysicalShield::DrawShield` 0x72D033,
  `ProcessShield` 0x72D193) and the seed graphics' effect step read `MsPerTurn()`; the pure rules
  (`seed::ReadyAfterTurnsInHand`, `spell_flock::NextEmitted`, `StartFade`, `ProcessFade`) take it as a parameter
  (tests `SeedInHand.TheWaitFollowsTheGamesTurnLength`, `SpellFlock.TheFadeFollowsTheGamesTurnLength`). Identical
  while the turn is 100 ms, which no shipped map changes.
- **The night lights' once-a-second check** that forgot lights whose lantern was gone took the frame's game ms; it is
  not in the original (no periodic check: a light goes when its lantern is freed) and was removed with
  [the lantern's light going when it is freed](../world/day-and-night.md#night-lights).
- **The copies of the turn** (`reactions::Turn`, `magic::CurrentTurn`, the fireball's one, which nobody read, the
  weather and its loop, the trees, the animals, the villagers and the chimney smoke, which did not check whether there
  was a Game; `HandSpellSeed.cpp`'s `CurrentTurn()`; `FireGraphic.cpp`'s `g_Turn`, deleted, `SetTurn` remaining for
  the trace; `SpellSeedGraphic.cpp`) read `Turn()`. `magic::k_TurnMs`, `Chants.h`, `DayNightClock.cpp`,
  `FireFlies.cpp`, `ChimneySmoke.cpp` and `HelpSystem` take their constants from the clock.
- **Frame game ms.** `RendererMists.cpp`, `RendererSmoke.cpp`, `Renderer::UpdateClouds` (clouds, sky alignment and
  `night_lights::Update`), `CollectChimneySmoke` and `CollectMists` read `FrameGameMs()` instead of their wall-clock
  `static lastTime` with a 100 ms cap and the speed divided. The sun glare (`Graphics/Renderer.cpp`, `DrawClock`)
  eases by the frame's game ms, as the original's `g_game_time_inc` × 0.01, so it stays put in pause.
  The one-off globes (`MiracleFxSystem::UpdateGlobes`, `one_off::UpdateFrames`) and the chains' scroll
  (`chain_atoms::AdvanceScroll`) take `FrameGameMs()` as it is, as `OneOffSpellSeed::UpdateFrame` 0x72A570 loads
  `g_game_time_inc` at 0x72A571 and `ChainScroll` (fn_0067B3F0) does; before, `magic::Update` gave them
  `FrameGameSeconds() · 1000`, which is not the whole ms for 103 of the 200 frame lengths (10 ms came back as
  10.000001, the bubble's phase one bit further: test
  `MiracleFxSystemTest.TheGlobesStepByTheFramesWholeGameMillisecondsAsTheyAre`).
  The fields' sink steps by `FrameGameMs()` × 0.001f, as `Field::Draw` steps its Zoomer (+0xE8) by
  `g_game_time_inc` × [0x8AC418] at 0x5286D7; before, it took the wall clock's frame time, so it moved in pause and
  ignored the game speed. It is now the field's own draw (`FieldSystem::Draw`, from the
  [object draw list](../rendering/object-draw-list.md#how-openblack-runs-the-list)), as in
  [the field's draw](../world/objects.md#the-fields-draw-in-openblack) (test
  `FieldSystem.ADrawnCropStartsItsEaseAgainAndStepsByTheFramesWholeGameMilliseconds`).
- **The help texts clock** (`queries.nowMs`) was an approximate clock of its own; now it is `EngineMs()`. The reading
  seconds and `ShownLongEnough` were computed in double; now in float.
- **The wall clock was not started.** `engineTimer` stayed with base 0 and speed 1: `EngineMs()` and
  `UpdateRealClock()` gave the ms of `steady_clock` since its epoch (machine startup). In float, with more than 4.6 h
  of the machine switched on (2^24 ms) the ms went in steps of 2 (after days, in steps of 32: the texts clock and
  `FrameRealMs` were quantised), and with more than 24.8 days the conversion to int32 overflowed. Now `Reset()` leaves
  it stopped and `StartEngineTimer()` starts it from ≈ 0.
- **From audio**: `Audio/Services/SoundTags.cpp` had a local `k_MsPerTurn` 🔍 inferred: it is the turn length, now
  `game_clock::MsPerTurn()`; the duplicate copy of `audio::TickCount` / `MusicStream` is `game_clock::TickCount()`.

Visible changes, checked with screenshots: at 30 fps the game runs 25 % faster (turns last 100 ms); the first turn is
played in the first frame; when unpausing there is no jump; with the pause on the villagers and animals stay where they
were (before they went back to the position at the start of the turn); the sea scrolls at a constant speed; the piles
collected from fish farms, fields and heaps follow the game speed and stop when paused; the grain in the hand, the
fireflies and the particle effects interpolate with the game fraction (they stop when paused and follow the speed);
the belief symbols of the towns stop when paused.

### Test hooks

`OPENBLACK_FIXED_FRAME_MS` drives every clock with a fixed frame time ([Proving two runs are the same
game](loop.md#proving-two-runs-are-the-same-game)); the state hash's `clock=` part covers the turn, pause, speed and
ms per turn. The testbed's `examples.speed_across_land` shows the speed kept across a land change.

## Pending

- **Other constant turn lengths**, each to be read at its call site before it moves: `hand_grab::k_TurnMs`
  (`HandGrabRules.h`, the grab wait's turns), `scaffold_ghost`'s `k_TurnMs` (the part of the turn of the visual
  clock, which is the scheduler's literal 100), `DiscipleMarks.cpp`'s (the scheduler's `k_TurnDuration`),
  `CreatureObjectActionSystem.cpp`'s and `VillagerClips.cpp` / `VillagerAnimations.cpp`'s `k_MsPerTurn`. All are
  100 ms, so nothing differs while no map sets the turn length (none does).
- Trees need no fix for the frame time: openblack's `UpdateTrees` only uses its dt for a debug trace.
- **The player camera** (`Camera::Update`) and the camera paths' `Update` run with the frame's real dt (µs); the
  original chooses with `GetCameraTimeInc` 0x555820 (`CameraFrameMs()`, whole wall-clock ms). The script camera
  (`script_camera::UpdateCamera`), the paths' `HandlePlayerControl`, the creature camera and the hand's steps already
  read `CameraFrameMs()` (`Game.cpp`, `Camera/CreatureCameraModel.cpp`, `HandSystem.cpp`).
- **A one-off visual's length** (`manager::SpotVisualTurns` in `ParticleManager.cpp`, `seconds · 1000 / turn length`, now with `MsPerTurn()`): it
  has not been read in which order the original rounds when creating a one-off visual effect; the formula is left.
- openblack differs: every effect's first Process steps twice in the original, but openblack does it only for the
  town-centre effect (`town_belief::Step` in `src/Particles/TownBelief.cpp`), a fix row (see
  [Particles](../physics/particles.md#pending)).
- **`Magic/Objects/ShieldDebugHooks.h`** (the `OPENBLACK_TEST_SHIELD_FRAMES` comment) says that the PSys does not follow the speed: that is no longer so (its
  fraction is the game's); the note is to be corrected.
- **The bookmark keys**: openblack's follow the original's (`input::ActionOfBookmarkKey`, the keys 1 to 7:
  [the player camera's bookmarks](../camera/player-camera.md#the-bookmarks-markers-in-openblack)). The profile's own
  bindings (`ControlMap::LoadCurrentProfileKeys` 0x4713D0) are not read.
- **The testbed's scenario clock** (`ScenarioSeconds`) rebuilds the game time as `wall frame · speed` instead of
  reading `FrameGameSeconds()`: the same on average while `frame ms · speed ≤ 100`, ahead of the game's turns beyond
  that (its x4 at 33 ms frames).
