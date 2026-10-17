# Film playback

## Summary

How the original shows its films: the full-screen player for `INTRO.bik` and `fall.bik` (started by a script or by
the falling spell, paced by a 16 ms timer, faded out at the end, the game paused under the opaque part, skipped with
ESC), the falling spell's own film and fade, and the start-up screens (the logos, the pre-intro, the tips screen and
the loading screen). No film has a sound track: what is heard comes from the audio engine. openblack has all of these
on the original's schedule; ESC still skips in cases where the original's other key paths take the key first.
The decoder and the player class are on [Bink video](bink.md); what the falling spell draws over its film is on
[the falling spell](../magic/falling-spell.md).

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the films.

### The full-screen video

The one for INTRO.bik and fall.bik. Fields of `g_game` (0xD0195C): **+0x250188** the `LHVideoPlayer*` (≠ NULL = there is
a video; it is what the audio reads), **+0x25018C** the frame at which the fade starts, **+0x250190** end frame,
**+0x250194** alpha (float), +0x250530 "is the intro". Globals: `VideoFramesReady` 0xD01988, `VideoFinished` 0xD0198C,
0xD01990 "video music started", 0xD01994 its bank, 0xD01998 the timer, `VideoPreviousPause` 0xD0199C, 0xD019A0 the
previous widescreen state, the byte 0xD01984 "cannot be skipped", `FallingSpellVideo` 0xCD3B10, `VideoLetterboxScale`
0xBEC16C = 1.0.

- +0x48, width * height * 2 bytes, zeroed (0x7C64EE) (in the code: `framebuffer`)

#### Starting: PlayFullScreenMovie

**Faithful.** 0x54D920 (`ecx` = g_game, `ret 8`; the 2nd argument is an **audio bank name**, not the `const Rect&` of
symbols.txt, and both callers pass NULL):

1. `ClearTipVideo` 0x54D923; +0x250530 = 0; `DeleteVideo` of the previous one (0x54D939); alpha = 1.0 (0x54D946).
2. `[0xCD3B20]+0x1C = −1` 0x54D963: the GAudio (the same `ecx` as `LHBankRegister` and `StartScriptMusic`), +0x1C =
   `GameMusic::_alignmentType`, "the music that is playing" forgotten.
3. `VideoPreviousPause = (flags >> 2) & 1` 0x54D95C and `PauseGame(1)` 0x54D96A.
4. The bank: frees the previous one (fn_00428640) and registers the new one (fn_00428620 → `LHBankRegister`, null with NULL).
5. `fn_0054AB20(file, 5)` 0x54D9A5: with `[0xD46AB8] ≠ 0` it first tries `%c:\name.ext` on the CD; `VideoFinished = 0`,
   0xD01990 = 0, `VideoFramesReady = 0`; `new(0x68)` → +0x250188 **even if the file does not open**; `Open(path, 1, 0)`;
   **end = frames** (0x54AC4E) and **fade = frames − fps·5** (0x54AC70); a *pre-roll* of up to 10 `BinkWait`/
   `BinkService`/`Sleep(0)` that decodes nothing.
6. `fn_0054AB00` 0x54D9AD: `timeSetEvent(16 ms, resolution 5, 0x54AAE0, periodic)`.
7. `0xD019A0 = HelpSystem+0x45E8` 0x54D9BE and, if there was no widescreen, `HelpSystem::SetWideScreen(1, 0)` 0x54D9E4.
8. `HelpSystem` fn_005C6C40 0x54D9EF, **always** (also if widescreen was already on): +0x45F0 = −FLT_MAX
   (0x5C6C40), so `GetWideScreenPercentage` 0x5C6B60 = |t·0.001/wideScreenTime| clamped to [0, 1] gives 1 immediately:
   the bars appear fully in the first frame and stay that way (while paused 0 is added, fn_005C6BB0). When it ends,
   `SetWideScreen(0)` leaves +0x45F0 = (1 − 1)·2000 = 0 and the bars go away over 2 s of game clock.

- Addresses of fn_0054AB20: `VideoFinished = 0` at 0x54ABD0, 0xD01990 = 0 at 0x54ABD6, `VideoFramesReady = 0` at
  0x54ABE2, `DeleteVideo(false)` at 0x54ABE8..0x54ABF1, `new(0x68)` and +0x250188 at 0x54AC05..0x54AC23,
  `LHVideoPlayer::Open(path, 16 bits, one texture false)` at 0x54AC3B, end at 0x54AC4B..0x54AC4E, fade at
  0x54AC60..0x54AC70, pre-roll at 0x54AC76..0x54ACD8.
- The 5 s comes from `push 5` at 0x54D9A2. The 16:9 constant is [0x8C7A50] = 0.5625, read at 0x54DBF7.
  `VideoLetterboxScale` [0xBEC16C] is read only at 0x54DBFF.

If the file does not open: fps 1 and 0 frames → end 0 and the next `Process3dEngine` deletes it (0 ≥ 0).
If one video replaces another, `VideoPreviousPause` and 0xD019A0 are taken **already paused and with widescreen**: when
the second ends the game stays paused (faithful by reading, not seen in the game).

- 0x54ABD0 (0xD01990, the film's music started, 0x54ABD6: not ported) (in the code: `VideoPlayer::Open`)
- 0x54DD56..0x54DD74: +0x250188 != NULL && +0x250194 == 1.0f, and not FallingSpellVideo (in the code: `VideoPlayer::CoversScreen`)
- Process3dEngine 0x54DC11..0x54DC1B (`neg; sbb; and 0xFFFFFF51; add 0xFF`): the vertex alpha of the falling spell's film is 0x50, any other 0xFF (in the code: `k_OpaqueAlpha`)
- GGame::VideoLetterboxScale [0xBEC16C] = 1.0f, read only at 0x54DBFF (in the code: `k_LetterboxScale`)

#### Schedule: 5 s fade and the intro at 60 s

**Faithful.** By default the video lasts until its last frame with a 5 s fade. `StartAVISequence(1)` 0x68F450, if there
is a player, overrides the schedule: **fade = fps·58** (`fps*7`, `+fps*28`, `*2`, 0x68F4A1..0x68F4AF) and **end = fps·60**
(`imul 0x3C` 0x68F4C3), +0x250530 = 1, and `SetupScreenFadeBackToNormal(0)` 0x68F4E9 (only if there is a player, which
there always is). INTRO.bik: fade from 1392 to 1440; **frames 1440..1600 are never seen**. fall.bik: **no fade of its own**
(`FallingSpell::Init` sets fade = end, 0x5262C4..0x5262CF); it only fades when the spell ends (48 frames).
`SET_AVI_SEQUENCE(2)`: `KickOffFallingSpellVideo` and `SetupScreenFadeBackToNormal(0)`.
`StartAVISequence(1)` does nothing without a film (0x68F49C); it writes the fade at 0x68F4AF, the end at 0x68F4C6 and
+0x250530 = 1 at 0x68F4D1.

- FallingSpell +0x0C: the film ms of the last update (100 after Init, 0x52615F) (in the code: `LastMs`)

#### Pacing

**Faithful.** The 16 ms timer (callback 0x54AAE0, checks the id 0xD01998) calls `VideoPoll(1, 1)` 0x54AA40: if
`frame > end` (0x54AA6A `jle`) `DeleteVideo`; otherwise, the pending `BinkNextFrame`, and if `BinkWait` says it is time,
**one** `DecodeNextFrame`, `fn_0054A9B0` (the bank music from frame 3 onwards; never with a NULL bank) and
`++VideoFramesReady`. That is, Bink keeps the time and the timer decodes at most one frame every 16 ms, on
another thread, with `BINKNOSKIP`.

- `VideoPoll` 0x54AA40: past the end → `DeleteVideo(true)` (0x54AA61..0x54AA6E); no HBINK, or `BinkWait` says not yet →
  nothing (0x54AA94..0x54AAA9); otherwise `DecodeNextFrame` (0x54AAB7) and `++VideoFramesReady` (0x54AAC1).

- 0x54AA61..0x54AA6A `jle` (in the code: `VideoPlayer::Poll`)

#### Every frame: Process3dEngine

**Faithful.** `GGame::Process3dEngine` 0x54DA80, after `LH3DRender::StartFrame` 0x54DAB5 and inside the video section:

1. `BinkService` 0x54DAD7; if `VideoFinished`, `FinishedVideo` 0x54DAE7. Without a video, straight to step 6.
2. **alpha = 1.0 every frame** (0x54DB05), before looking at anything.
3. `frame ≥ end` (0x54DB10 `jl`) → alpha = 0, `DeleteVideo`, go to step 6.
4. `frame > fade` (0x54DB2D `jle`) → if the pause state is not `VideoPreviousPause`, `PauseGame(VideoPreviousPause)`
   0x54DB42 (the game runs again during the fade) and
   **alpha = 1 − (f − start) / (|end − start| + 1)** (0x54DB4A..0x54DB7F: integers, `fild`/`fidiv`/`fsubr 1.0`, FPU at 24
   bits).
5. If alpha is exactly 1.0 (bitwise comparison), it is not the spell and there is no new frame: up to **1000 ×
   (`VideoPoll(0, 0)` + `Good_sleep_us(500)`)** = 0.5 s of waiting (0x54DB9E..0x54DBD1). Then, if there is still a video:
   - bars: `ftol((H − W·0.5625) · VideoLetterboxScale) / 2` (0x54DBEB..0x54DC0F, `cdq; sub; sar`, signed,
     **not clamped to 0**: on screens wider than 16:9 they come out negative and the image overflows above and below;
     the cinema bars' own height, `ScreenFade::LetterboxHeight` fn_0081E8B0, is the same formula on the bars' fraction
     [0xEB9950] and does not clamp either (`cdq; sub; sar` at 0x81E8CF, no test); openblack's
     `CinemaBars::BarHeight` clamps it at 0, so the hand demos' cursor (`hand_demo::CursorPixel`) does not use it);
   - colour: `0x00FFFFFF | ftol(base · alpha) << 24`, base **0xFF**, or **0x50** with `FallingSpellVideo`
     (0x54DC11..0x54DC4D, `__ftol` truncates);
   - `DrawToScreen(color, 0, bars, W + 1, H − 2·bars + 1)` 0x54DC6D (W, H = `[0xE839E4]`/`[0xE839E8]`),
     `VideoFramesReady = 0`, the statistics string.
6. **The 3D world is not drawn** if there is a video, alpha == 1.0 and it is not the spell (0x54DD5E..0x54DD7D → 0x54E2A4).
   What comes afterwards is done: `GScript::ProcessFade` / `Temple::UpdateFade` and `HelpSystem::Draw3D` (cinema bars and
   texts) 0x54E2D7..0x54E2ED. During the fade (alpha < 1) the world is drawn and the video is blended on top.

- 0x54E2AD (in the code: `TempleFade::Runs`)
- 0x54DD83..0x54DD95: switch (+0x205A28), case 2 at 0x54DD9B (in the code: `FallingSpellVideo::ProcessFrame`)

#### End: DeleteVideo and FinishedVideo

**Faithful.** `DeleteVideo(bool)` 0x54A940 (the `bool` is unused): destroys the player, +0x250188 = 0,
**`++VideoFinished`**, 0xD01984 = 0, `timeKillEvent`, 0xD01998 = 0, 0xD01990 = 0. It is called by `Process3dEngine` (end),
`VideoPoll` (past the end), the skip, `PlayFullScreenMovie` and `fn_0054AB20`.
`FinishedVideo` 0x54D8D0, on the next frame: `PauseGame(VideoPreviousPause)`; if `HelpSystem+0x45E8 ≠ 0xD019A0`,
`SetWideScreen(current == 0, 0)` 0x54D902; 0xD01984 = 0; `VideoFinished = 0`. Since `fn_0054AB20` sets `VideoFinished = 0`
after its `DeleteVideo`, a replaced video does not call `FinishedVideo`.

- A film with no frames (frame 0 ≥ end 0): alpha 0 and `DeleteVideo` (0x54DB0E).
- `DeleteVideo` addresses: the dtor (`BinkClose`) at 0x54A959, +0x250188 = 0 at 0x54A969, `++VideoFinished` at
  0x54A977, 0xD01984 = 0 at 0x54A97D, `timeKillEvent` and 0xD01990 = 0 at 0x54A983..0x54A99A.
- `FinishedVideo` addresses: `PauseGame` at 0x54D8D9, reads HelpSystem +0x45E8 at 0x54D8E4, `SetWideScreen` at
  0x54D8F2..0x54D902, 0xD01984 = 0 at 0x54D907, `VideoFinished = 0` at 0x54D90E.

- 0x54A959 the dtor (BinkClose), 0x54A969 +0x250188 = 0 (in the code: `VideoPlayer::Stop`)
- 0x54D8D9 (in the code: `VideoPlayer::FinishedVideo`)

#### Pause and widescreen

**Faithful.** `PauseGame` 0x54AE20: nothing if it is already in that state or in multiplayer; it toggles bit 2 of `flags`
and stops or starts the game clock (`GameClock` +0x205D68). During the opaque video the game is **paused** (there are no
turns; the turn in which the script starts the film ends paused, so its `EndTurn` cuts the ambience with
`AtmosProcess(0)`, [Master volumes, focus and reset](../audio/banks-and-channels.md#master-volumes-focus-and-reset)); during the fade the previous pause state returns. Widescreen goes with
owner 0 (`+0x45EC = 0`).

### Skipping with ESC

**Faithful.** `GGame::ProcessKey` 0x63EF20, key `LH_KEY 1` (ESC; index 0 of the table 0x63F6B8 → 0x63F6A0 → 0x63F3B9):

- With `[0x9A161C]` (0x10) or `[0x9A161E]` (0x20) in the modifiers → nothing (0x63F3C6 / 0x63F3D6). **🔍 inferred** Shift
  and Ctrl: the same function translates them to DIK 0x2A / 0x1D at 0x63F2C6..0x63F2EC (and `[0x9A1620]` 0x40 → 0x38 = Alt,
  which the skip does not look at). They are constants (nobody writes them).
- With a video and without the byte 0xD01984 → `fn_0054DA00` 0x63F3F5 and `GAudio::StartScriptMusic(0)` 0x63F402 (the
  script music is released; `StartScriptMusic` is 0x428230, called at 0x63F3FA..0x63F402). Without a video, the normal
  ESC (0x63F4D7).
- Before the table, ProcessKey has other ESC paths that exit without getting here (0x63EF7A..0x63F2A4) 📼 1.20 only: the box of [g_game +0x205A10] (when not in wide screen) and, in land mode 6, the tutorial consume
  ESC; an active `SetupBox` ([0xC4CCED]) or dialogue takes the key; inside the citadel (+0x205A28 == 1) the citadel's
  handler 0x794830 takes it. None of them reaches the skip at 0x63F3B9, so a film is not skipped from inside the
  citadel or with a box up. openblack skips in every case (`Game.cpp`, Pending).
- The normal ESC (the menu) also calls fn_006EC670 (a help task's dialogue is stopped first) and has a 300 ms debounce in
  fn_00540020: event 7 [0x5400F2] records in [0xBEA294] = timeGetTime when the menu was last closed; its initial value
  is 0xFFFFFFFF, so before the first close there is no delay.

`fn_0054DA00`: with `FallingSpellVideo` → `EndFallingSpellVideo` 0x553A10 and nothing else. Otherwise, with a video: if it
is already in the fade (`frame > start`) → `DeleteVideo` right away; otherwise, **start = frame** and
**end = min(frame + 48, frames)** (0x54DA47 `add 0x30`, signed `jle`; 2 s at 24 fps) and `PauseGame(VideoPreviousPause)`
0x54DA63. The byte 0xD01984 is set to 1 by `pc_main` at 0x641E49 only when
`GetNumberOfProfiles` 0x66B9C0 (called at 0x641E30) returns 0, after `JustDoNewProfileBox` 0x53CE70 📼 1.20 only, and
cleared by `DeleteVideo`/`FinishedVideo`: **the first video of a new profile cannot be skipped**.

- 0x553A1A (in the code: `FallingSpellVideo::End`)
- 0x63F3E6 -> 0x63F4D7: the key's other use (in the code: `VideoPlayer::EscapeKey`)

### The falling spell (fall.bik)

**Faithful** (ported in `src/Video/FallingSpellVideo.{h,cpp}`). Read in full: `KickOffFallingSpellVideo` 0x5539A0,
`EndFallingSpellVideo` 0x553A10, `FallingSpell::Init` 0x526060, `Close` 0x5264A0, the unnamed update 0x526E00
(called by `fn_00553A60` 0x553A6A), `Draw` 0x5267D0, the callback 0x526480 → 0x526530 and `Temple::UpdateFade` 0x794280.

- **Startup.** `SET_AVI_SEQUENCE(on, 2)` → `StartAVISequence` (0x68F459 loads g_game, 0x68F45F calls) →
  `KickOffFallingSpellVideo` and always
  `SetupScreenFadeBackToNormal(0)` 0x68F471. KickOff **does nothing** if the local player has no creature
  (0x5539A5..0x5539C0: `g_game + 0xA64 + 0xA60·[+0x205A59]` = `players[PlayerIndex].creature`, `GPlayer` of 0xA60 bytes
  from +0x18 with `creature` at +0xA4C). If it has one: `EndFallingSpellVideo` of the previous one (0x5539C4),
  `+0x205A28 = 2`, `new(0x40)` (Game.cpp line 0x1ADD), ctor fn_00527240 (+0 = +4 = 0), `FallingSpellVideo` = the object,
  `Init`. `SET_AVI_SEQUENCE(off, 2)` → `StopAVISequence` 0x68F4F7 → `EndFallingSpellVideo` (without an object, nothing).
  - g_game +0x205A28 = 2 is written at 0x5539D5. Value 1 is the citadel's (`GoInsideCitadel` 0x554004);
    `EndFallingSpellVideo` writes 0 at 0x553A1C.
  - KickOff: `new(0x40)` at 0x5539DF, ctor fn_00527240 at 0x5539ED, `FallingSpellVideo` = the object at 0x5539F4, `Init`
    at 0x5539F9. A failed `new` jumps to 0x553A00.
  - The original reads `FallingSpellVideo->+0x20` with no null test. Mode 2 without the object only happens after a
    failed `new(0x40)`, which crashes in `Init`.
- **Init.** Loads the camera path `data\spells\fall\fall.cm2`, creates a `CreatureFalling` (0x57B8 bytes, vtable
  0x8D8BD8) of the player's creature with its hand glows, `PlayFullScreenMovie("data\spells\fall\fall.bik", NULL)`
  0x5261FC (**pause and widescreen like the intro**: the game does stop) and, with a player: fps ≤ 0 → 0x18, the
  camera at the path point for ms 0 (×0.8), and **+0x25018C = +0x250190** (0x5262C4..0x5262CF): **the video has no
  fade of its own** (the 5 s of `fn_0054AB20` are cancelled). Then +0 = 1, +0x10 the saved model light [0xEA9E90] (not
  the camera's position: fn_0081E1F0 writes it into the D3D light's position, 0xEC7F8C, then `SetLight(0)` through the
  device's vt+0x48), an `LH3DSprite` and 16 sparks (+0x34/+0x38), `+0x1C = +0x20 = +0x24 = 0`, a `LightBurst` (+0x3C)
  that is initialised twice, +0x28 = 0, +0x30 = 1.0, +0x2C = 0 and the end-of-frame callback 0x526480 (the light
  flashes, from state 2 onwards).
  - `Init` 0x526060 calls `Close` first if +0x00 is set (0x526068..0x526070; never on a new object).
  - The `fall.cm2` path (`LHFileLength` / `LHLoadData` / fn_0086D4A0, 0x52607F..0x5260B6) goes to +0x08, the
    `CreatureFalling` (`new(0x57B8)` 0x5260D1, ctor 0x52610E) to +0x04, its hand glows at 0x526186..0x5261EB, and
    +0x0C = 100 at 0x52615F.
  - After `PlayFullScreenMovie` (0x5261F0..0x5261FC) the part inside the video section is 0x526201..0x5262D5: the player
    check at 0x52620B..0x526213 (+0x250188 != NULL), fps ≤ 0 → 0x18 at 0x526219..0x52621E, and the camera at the path
    point of frame × 1000 / fps, points × 0.8 (0x526225..0x5262BF).
  - The end of `Init`: +0x00 = 1 (0x5262DA), +0x10 saved (0x5262E0..0x526311), the sprite (0x52631E) and the 16 sprite
    records (`new(0x200)` 0x526335), +0x1C, +0x20 and +0x24 = 0 (0x526425 / 0x526428 / 0x52642B), the light bursts
    (`new(0x400)` 0x52642E), and `RegisterFinishFrameCallback(0, 0x526480, this)` at 0x526466.
  - `Draw` puts the model light at (0, 0, 1000), and `Close` puts the saved one back (fn_0081E1F0 0x5264F4).
- **Every frame (mode 2).** `Process3dEngine` 0x54DD83: with `+0x205A28 == 2` (case 2, 0x54DD9B..0x54DE02) **the land is
  not drawn** (case 0 is 0x54DE57): `LH3DAtmos::Update3D`, `g_mode_cleaning = 0`, the update 0x526E00, and if there is
  no video or **`+0x20 == 4`** → `EndFallingSpellVideo` 0x54DDD6; otherwise `FallingSpell::Draw` (the video **first**, with
  `LHVideoPlayer::thedraw(0)` 0x52689F and base alpha 0x50, then the falling creature `DrawNow` 0x526A42 with its
  time-based tint, and the sparks) and the liquid particles. Other readers of +0x205A28: `GCamera::Update` 0x44233C..,
  fn_00516CB0, fn_00517080, `AddPlayerSparkles` 0x55264D, fn_005739F0, `Process3dEngine` 0x54E3D2 / 0x54E4BC
  (the weather `Render2D` is skipped in mode 2) **(not ported)**.
  - Case 2 addresses: the switch on +0x205A28 at 0x54DD83..0x54DD95, `LH3DAtmos::Update3D` at 0x54DDAB,
    `g_mode_cleaning = 0` at 0x54DDB5, fn_00553A60 (the update 0x526E00 when `FallingSpellVideo` != NULL) at 0x54DDBB, the
    end test at 0x54DDC0..0x54DDD2, `FallingSpell::Draw` at 0x54DDE0, the liquid particles at 0x54DDF5 / 0x54DDFD.
- **The update 0x526E00.** Without a video: `++(+0x20)` and nothing else. With a video, `t = frame·1000/fps` (0x526E61,
  integers): the creature advances `t − (+0xC)` (+0xC = 100 after Init), the camera along the path and `ChangeFov(π/4)`.
  Sounds by `+0x24` (each `if` after the previous one: a single call can pass several): **> 17 450 ms** 151 ScreenRumble
  (ScriptSfx); **> 19 450** 56 S_LasersbeamExplode_02 (Spells) and stops 172 (InGame, owner 1); **> 31 650** 166 G_Creed_01.
  State `+0x20`: **0 → 1 at > 13 450 ms** (+0x1C = 1; 168 G_CitadelExplode_01 and 172 G_Volcano_02 owner 1);
  **1 → 2 at > 37 750** (30 S_HealChakra; 166 owner 2 with pitch 0x85); **2 → 3 at > 43 900**: temple fade to white
  (`[0xE06024] = 0xFFFFFF`, target `[0xE06020] = 1.0`, current `[0xC2A150] = 0`, `[0xE06028] = 0`), stops 166 (owners 0 and
  2), **`LHMusicStop(1)` 0x5271B0** (all music fades out) and 168; **3 → 4** when `[0xE06028] ≠ 0` (the fade reached
  white): target 0, current 1.0, `[0xE06028] = 0` (comes back from white). Banks: +0x3AC InGame, +0x3B4 Spells,
  +0x3BC ScriptSfx (0x526F6E..0x5271E1).
  - It returns at once without +0x00 (0x526E15); without a film, `++(+0x20)` is at 0x526E21..0x526E29.
  - Film time t = frame × 1000 / fps in 32-bit integers: frame × 1000 wraps (`lea` × 3 = 125, `shl 3`), then
    `cdq; idiv` (0x526E61..0x526E71).
  - The creature advances by ms − +0x0C (vt +0x14, 0x526E74..0x526E8B), then +0x0C = ms (0x526EA6). The camera along the
    path with `ChangeFov(0x3F490FDB = π/4)` is at 0x526E8E..0x526F1C.
  - Thresholds: sounds 17450 (0x526F32, test 0x526F21..0x526F3A, state set 0x526F40), 19450 (0x526F7F,
    0x526F79..0x526F8D), 31650 (0x526FE8, 0x526FE2..0x526FF6); states 13450 (0x527034, set 0x52704A), 37750 (0x5270B3,
    set 0x5270C1), 43900 (0x52713A, test 0x527130..0x527142).
  - The eleven sound calls, by their name in openblack's `FallingSpellSound::Cue` (listed with their addresses under
    [Counterparts in the code](#counterparts-in-the-code)).
  - The white fade setup is at 0x527148..0x527170 ([0xE06024] = 0xFFFFFF at 0x527152, target at 0x52715C, current at
    0x527166, [0xE06028] at 0x527170). State 3 is set at 0x5271A3; `LHMusicStop(1)` is `push 1` at 0x5271AE.
  - State 3 → 4: the test is at 0x5271EC..0x5271FA, the state set at 0x5271FC, the fade back from white at
    0x527203..0x527221.
  - The sounds are stopped with `LHSampleStop(bank, owner, sample)` through GGlobal +0x14 (the import 0x8A97A4, called
    directly).
- **`Temple::UpdateFade`** (0x54E2DE, every frame unless target == current and the mode is not 1; mode 3 never):
  advances `g_delta_time · 0.001` per frame (1.0 per second, real clock, also while paused). Going up, when it overshoots:
  `++[0xE06028]`, current = target and **target = 0** (it comes back down by itself); going down, when it overshoots:
  current = target, `++[0xE06028]` and with target ≤ 0 the colour goes to 0. It writes `(alpha << 24) + rgb` (alpha 0xFF
  above 1, otherwise `ftol(current·255)`) into `[0xFA51D8]` with fn_0053CE60: **the same screen-fade colour as the
  script**. The FPU runs at 24 bits (fn_007DEE00, called after `FinishFrame` 0x54E426 / 0x54E4D1 and in `EndTurn`): every
  step rounds to `float` and `float`s are compared; a step that lands **exactly** on the target does not count
  (target == current: `Process3dEngine` stops calling it and the fade stays) **(inferred: that nothing raises the
  precision in between)**. At startup, `DoLogo` (0x5FA0E1..0x5FA0FF, first pass of `GGame::Loop`) sets current = target
  = 0 and `[0xE06028] = 0`: at rest.
  - `Process3dEngine` 0x54E2A4..0x54E2DE: mode 3 runs neither fade (`cmp ecx, 3; je 0x54E3C7` at 0x54E2AD). If target !=
    current (0x54E2B3..0x54E2C4) or the mode is 1 (0x54E2C9), it calls `Temple::UpdateFade`; otherwise
    `GScript::ProcessFade(1)` (0x54E2D7).
  - Constants: g_delta_time × 0.001 is `fmul [0x8AA3B0]` (0x794293); alpha × 255 is `fmul [0x8AB270]` (0x794346).
  - Going up (0x7942E6..0x794316): add the step and store; only strictly past the target (0x7942F2..0x7942FD) does
    `++[0xE06028]`, current = target (0x79430B) and target = 0 (0x794316).
  - Going down (0x7942A0..0x7942DE): subtract the step; strictly below the target (0x7942AC..0x7942B7) current = target,
    `++[0xE06028]` (0x7942CF), and with target ≤ 0 the colour [0xE06024] = 0 (0x7942DC..0x7942DE).
  - The colour (0x794326..0x79435D): alpha 0xFF above 1.0, else ftol(current × 255) (the product rounded to float,
    `__ftol` truncates); value = (alpha << 24) + (rgb & 0xFFFFFF); fn_0053CE60 writes it at 0x794361.
  - The .data value of current [0xC2A150] is 1.0f. `DoLogo` (`GGame::Loop`'s first pass, 0x54D060, single player) sets
    current = target = 0, [0xE06024] = 0xFF000000 (0 once masked, 0x794355) and [0xE06028] = 0. `OnNewGame` also sets
    current = target = 0 (0x553984 / 0x55398E), only together with its black (rendering.md).
- **The end.** `EndFallingSpellVideo`: `+0x205A28 = 0`, `Close` (removes the callback, deletes the creature and the path,
  restores the **model light** [0xEA9E90] (not the camera; fn_0081E1F0 0x5264F4), frees sparks and flashes), `delete`, `FallingSpellVideo = NULL` and `fn_0054DA00`. Without
  `FallingSpellVideo` that skip is the **normal** one: a 48-frame fade from the current one **with base 0xFF** (the first
  frame, alpha 1.0, covers the world) and the pause restored. In time: at 43.9 s the white rises over 1 s, at ~44.9 s the
  spell ends, the video fades out over 2 s and the white goes down over 1 s. ESC (`fn_0054DA00` 0x54DA0C) does the same
  earlier.
  - `Close` 0x5264A0 only runs with +0x00 set (0x5264AD): `RemoveFinishFrameCallback` (0x5264B6), +0x00 = 0,
    [0xC64204] = 0, the creature deleted, the path freed (fn_0086D4D0), the model light restored, the sprites freed.
  - `EndFallingSpellVideo`: `Close` at 0x553A2D, the dtor fn_00527250 (which calls `Close` again only with +0x00, now 0)
    at 0x553A3E, `delete` at 0x553A44, `FallingSpellVideo = NULL` at 0x553A4E, then fn_0054DA00 at 0x553A58.

- 0x79428D fild, 0x794293 fmul (in the code: `TempleFade::Update`)
- the player is made anyway and ends on the next frame (0x54AC05) (in the code: `FallingSpellVideo::GameHooks`)
- 0x5539BE..0x5539C0 `je 0x553A0E`: no creature, nothing at all (in the code: `FallingSpellVideo::KickOff`)
- 0x5539D5 (in the code: `FallingSpellVideo::Start`)
- 0x526068..0x526070: Close if +0x00 (never on a new object) 0x526075..0x5261EB (not ported): fall.cm2, the CreatureFalling, its hand glows; +0x08 the path, +0x04 the creature (in the code: `FallingSpellVideo::Init`)
- 0x5264AD: only with +0x00. 0x5264B6 RemoveFinishFrameCallback, +0x00 = 0, [0xC64204] = 0, the creature deleted, the path freed (fn_0086D4D0), the camera put back (fn_0081E1F0 with +0x10), the sprites (not ported) (in the code: `FallingSpellVideo::Close`)
- 0x526E15 (+0x00) (in the code: `FallingSpellVideo::Update`)
- The falling spell's film of runblack.exe (fall.bik): GGame::KickOffFallingSpellVideo 0x5539A0 / EndFallingSpellVideo 0x553A10, the FallingSpell object (new(0x40), Init 0x526060, Close 0x5264A0, its update 0x526E00, Draw 0x5267D0) and the Temple fade it drives (Temple::UpdateFade 0x794280).  - CHL 203 SET_AVI_SEQUENCE(on, 2): PSysGlobal::StartAVISequence 0x68F459 -> KickOff (only when the local player has a creature), StopAVISequence 0x68F4F7 -> End. - KickOff: the previous one ended, g_game +0x205A28 = 2, FallingSpellVideo = the object, Init: PlayFullScreenMovie ("data\spells\fall\fall.bik", NULL) 0x5261FC (the game paused, the wide screen on, as the intro) and the fade start moved to the end (0x5262C4..0x5262CF): the film has no fade of its own. - Process3dEngine case 2 (0x54DD9B..0x54DE02), every frame: the land is not drawn; fn_00553A60 runs the update 0x526E00 (the state +0x20 and the sounds by the film's ms), then without a film or at state 4 EndFallingSpellVideo, else FallingSpell::Draw (the film with base alpha 0x50 first, LHVideoPlayer::thedraw 0x52689F, then the falling creature and its sprites) and the liquid particles. - State: 0 -> 1 at 13.45 s, 1 -> 2 at 37.75 s, 2 -> 3 at 43.9 s (a 1 s white Temple fade up, LHMusicStop(1)), 3 -> 4 when that fade is up (the fade goes back down from white): EndFallingSpellVideo, whose fn_0054DA00 is then the normal skip (no FallingSpellVideo any more): a 48-frame fade of the film with base alpha 0xFF, the pause given back. ESC (fn_0054DA00 0x54DA0C) ends it the same way.  Ported in src/Magic (FallingSpell, Graphics/RendererFallingSpell.cpp): the fall.cm2 path (computed, not yet applied to the camera), the 16 screen-anchored smoke puffs, the one LightBurst (initialised twice) and the model light Init saves, Draw puts at (0, 0, 1000) and Close puts back (fn_0081E1F0 0x5264F4: the light, not the camera). Still not ported: Init's fall.cm2 camera path (LHFileLength / LHLoadData / fn_0086D4A0 0x52607F..0x5260B6), the CreatureFalling copy of the player's creature (new(0x57B8) 0x5260D1, ctor 0x52610E, vtable 0x8D8BD8) and its hand glows (0x526186..0x5261EB), the camera along the path with ChangeFov(pi / 4) (0x526259..0x5262BF, 0x526EA9..0x526F1C), the 16 sprites (new(0x200) 0x526335) and the light bursts (new(0x400) 0x52642E, the finish frame callback 0x526480 -> 0x526530), everything FallingSpell::Draw draws besides the film (the creature parts), LH3DAtmos::Update3D (0x54DDAB) and the other readers of +0x205A28 (GCamera::Update 0x44233C.., fn_00516CB0, fn_00517080, AddPlayerSparkles 0x55264D, fn_005739F0, Process3dEngine 0x54E3D2 / 0x54E4BC). The sounds and the music stop are events (Hooks::sound / musicStop); GameHooks() plays them through Audio.h (PlaySoundEffect / StopSoundEffect / MusicStop). (aproximado) In mode 2 the original draws the film inside Process3dEngine (thedraw 0x52689F), so FinishFrame's bars and fade go over it; openblack draws bars, film, fade (Renderer::DrawFinishFrameOverlays): the same picture while the bars are at 100 % (the film's letterbox is their height). The frame EndFallingSpellVideo runs, the original still shows the film with base alpha 0x50 (DrawToScreen kept its colour at 0x54DC6D, drawn by FinishFrame's callback); openblack takes the colour when it draws, 0xFF (under the white fade, then 1 - g_delta_time / 1000 opaque).
- [0xC2A150] Temple::Dat_00C2A150, the current value. The exe's .data has 1.0f, but DoLogo (GGame::Loop's first pass, 0x54D060, single player) sets current = target = 0, [0xE06024] = 0xFF000000 and [0xE06028] = 0 (0x5FA0E1..0x5FA0FF), and OnNewGame current = target = 0 (0x553984): at rest, as here (in the code: `current`)
- fn_0053CE60 0x794361: [0xFA51D8] = the Temple fade's colour (ScreenFade::SetColour) (in the code: `void`)
- Process3dEngine, after the film's service (0x54DAB5..0x54DD76, VideoPlayer::Process): case 2 0x54DD83..0x54DDDB (the update 0x526E00 through fn_00553A60, then EndFallingSpellVideo without a film or at state 4), then 0x54E2A4..0x54E2DE Temple::UpdateFade when it runs, `realMs` = g_delta_time (game_clock::FrameRealMs) (in the code: `ProcessFrame`)
- 0x68F49C (in the code: `VideoPlayer::SetSchedule`)
- 0x68F49C (in the code: `VideoPlayer::ScheduleIntro`)
- 0x54DA00..0x54DA12: GGame::EndFallingSpellVideo (milestone V6) (in the code: `VideoPlayer::Skip`)
- GGame::FallingSpellVideo 0xCD3B10 != NULL (KickOffFallingSpellVideo 0x5539A0 / EndFallingSpellVideo 0x553A4E; FallingSpellVideo.h) (in the code: `SetFallingSpellVideo`)

### Startup and loading screen

**Faithful by reading.** The logos, the pre-intro and the tips screen are in openblack (`Game::PlayLogoScreens`,
`PlayPreIntro`, `StartTipOfTheDay` and the loading screen's `BeginLoadingScreen` / `RenderLoadingFrame` /
`EndLoadingScreen`, src/GameStartup.cpp; their rules in src/Gui/StartupRules.h and src/Gui/LoadingScreenRules.h).

- `PlayLogoScreens` 0x642950, called by `pc_main` at 0x641D5D on every start, before the audio exists (0x641D79): the
  path is `.\data\logo.bik` (0xBFEBAC), or `%c:\logo.bik` (0xBFEBBC) on the CD when [0xD46AB8] is set and that file
  is there. No audio.
  - Two passes, `[esp+0x40]` = 1 and 2 (0x6429CA, 0x642D4B..0x642D57): each makes a new `LHVideoPlayer` (`new(0x68)`
    0x6429ED), `Open(path, 1, 0)` 0x642A11, frame −1, `BinkGoto(hbink, pass, 0)` 0x642A2C (Bink frames count from 1:
    our frames 0 and 1), `DecodeNextFrame` 0x642A34, `UploadToTextures` 0x642A40, and at the end `Close` and `delete`
    (0x642D32..0x642D43).
  - The fade is a `Zoomer` on the stack, the shared one (`SetDestinationWithSpeedAndTime` 0x407D60): to 1.0 at speed 0
    in 0.7 s (0x642A54..0x642AC4). Each frame (`StartFrame` 0x82F0E0): time += `[0xC38134]` (the frame's ms) × 0.001
    (0x8AA3B0), then the curve while time < duration, otherwise the destination (0x642AE8..0x642B95, the sums of
    `Zoomer::Update`: the start speed is 0 in every move, so its term adds nothing). Alpha = `ftol(value × 255)`
    (0x8AB270, 0x7A1400) kept within 0 and 255 (0x642BA8..0x642BBB), drawn this frame whatever is pressed below.
  - Pressed (fn_00642930: `[0xE85474]` = 1, a key, or `[0xE85350] & 3`, the mouse buttons): in phase 0 or 1 the
    Zoomer is put at 1 (0x642BCF..0x642C2C) and fades out at once; in phase 2 it is put at 0 (0x642C5F..0x642CBC) and the
    still ends. Each press clears the input (fn_005FA000, 0x642C34). Otherwise, arrived (time = duration, 0x642C39):
    phase 1 holds, to 2.0 in 1.7 s (0x642CC6), phase 2 fades out, to 0 in 1.1 s (0x642C52); phase 3 ends the still
    (0x642D27). Opaque from 1 to 2 and back to 1, so the fade out shows from 1 s into it.
  - Drawn with `DrawToScreen` 0x845740 (colour `alpha << 24 | 0xFFFFFF`, x −1, y −1, the screen's width and height,
    0x642CDB..0x642D03), then `EndFrame` and `Flip` 0x7DE090.
  - The input is cleared before the passes (0x6429C5) and after them (0x642D5D), then the FPU is reset (0x642D62,
    fn_007DEE00: `fninit`, 24-bit precision).
- `PlayPreIntroVideo` 0x6426F0: its own loop (does not use `GGame`): `frame = −1`, `BinkSetSoundOnOff(0)` 0x6427AB,
  `trailer.sad` via `LHMusicPlay` 0x642806, cursor hidden; on each pass it waits for `BinkWait` (up to 50000 × 0.5 ms),
  decodes, `DrawToScreen(0xFFFFFFFF, 0, 0, W, H)` (no bars), `Flip`; continues while `frame < frames − 1` and
  `[0xE85474] ≠ 1` (inferred: a key). When it ends `BinkSetSoundOnOff(0)` again, `LHMusicStop(0)` 0x642907.
  - Called by `pc_main` at 0x641D94 when `[0xD46ABD]` is set or there is no profile (fn_0066B9C0, 0x641D8B), after
    the audio (0x641D79) and before `StartTipOfTheDayText` (0x641D9F). The path is `.\data\pre_intro.bik` (0xBFEB80) or
    `%c:\pre_intro.bik` (0xBFEB98) on the CD.
  - The music: `LHBankRegister("audio\music\intro	railer.sad")` 0x6427CE, then `LHMusicPlay` 0x642806 with the
    options' bank, volume 0x7F, start 1, no sync, no fade, pitch 0x64 (0x6427DE..0x6427FA). The bank is not released
    afterwards. `SetCursor` 0x642815 before the loop; at the end `LHResetFPU` 0x64290D and `InputReset` 0x642912.
  - Each pass: `StartFrame`, `BinkNextFrame` when one is due (0x642834), the wait (`BinkWait` 0x642845, then
    `Good_sleep_us(500)` 0x64285E, at most 50000 times, 0x642850), `UnpackNextFrame` 0x64286A, `DrawToScreen`
    0x6428A4, `FinishFrame`, `Flip`. Only a key ends it early: the mouse buttons do not.
- `MakeTipVideo` 0x5F3CE0: `Open(".\data\tips.bik", 1, 1)` (one texture), `BinkGoto(tip + 1)`, one image.
  `ClearTipVideo` 0x5F3D90 deletes it (also from `PlayFullScreenMovie`). Called by `GGame::Init` 0x54F452,
  `DrawLoading` 0x5F3DE4 (a tip whose picture has gone) and `StartTipOfTheDayText` 0x5F4CC6.
- `StartTipOfTheDayText` 0x5F4C90, from `pc_main` 0x641D9F (after the pre-intro) and `GGame::Init` 0x54F44D (each
  game): the tip is `(GetTickCount() >> 4) % 34 + 1`, 29 and 32 giving the next one (`tip % 34 + 1`), and tip 0 with
  no profile (0x5F4C9A..0x5F4D13); its text is the tip's entry of the text table (`tip + 0x141F`), copied and its `$`
  and `\` codes blanked up to a space, the hidden-word mark 0xF8FE or the end (0x5F4D42..0x5F4D99). The first time
  (`[0xBF3BAD]`, cleared at 0x5F4E1C) it fades in: `DrawLoading(min(fade, 1), 0)`, fade += the frame's ms × 0.001,
  `Flip`, while fade < 1.0 (0x5F4DC3..0x5F4E1A).
- `DrawLoading(fade, progress)` 0x5F3DC0: nothing at fade ≤ 0; the sharp picture's share `clamp((fade − 0.5) × 2, 0,
  1)` and the alpha `min(fade × 2, 1) × 255` (0x5F3DF6..0x5F3F14); the layout from the screen's W and H: the text
  W × 0.125 to W × 0.875, from W × 0.6 down (by the width) to H × 0.95, the picture at (W × 0.18, H × 0.05), W × 0.64 by
  H × 0.64 (0x5F3E6D..0x5F3F05, the constants 0x8AB620, 0x8AB618, 0x8C7BDC, 0x8CF000, 0x8C49E8, 0x8AC3F4, 0x8CA28C).
  The bar's body is a `DrawBevBox` 0x5F4637 of the front end's buttons; the version comes from the registry
  (`LHRegistrySetCurrentKey` 0x5F442B).
- `RenderLoadingFrame(bool)` 0x5F4E40, the loading's redraw point (twelve calls in `GGame::InitOneTimeOnly`, seven in
  `GGame::Init`, and the map script's): with a tip shown (`[0xD318D8]`), `GetTickCount`; the first call notes the time
  and starts the loading time at 1; under 150 ms since the last redraw nothing (0x5F4E96); in the tips mode
  (`[0xD01980]` = 1) a wait over 350 ms counts as 350 (0x5F4EB3), otherwise in full; then, unless the window is
  minimised (0x7DED50), `DrawLoading(1.0, time / [0xBF3BA8])`, or in the other mode the "please wait" banner once the
  time is past `[0xBF3BB0]` (0x5F4F1C).
- `fn_0054AD00` (blocking playback with `VideoTimerSection`) has no callers.

- 0x8450B8 (in the code: `VideoPlayer::DecodeNextFrame`)

### The detail change message

When the player changes the detail level on the Options page, the game shows a message before it leaves the page.
`DialogBoxOptions::ControlCallback` tells HELP_TEXT_DIALOG_VIDEOCHANGE when the detail differs from
the one the page opened with, updates the shown detail and keeps the page to go to in [0xCC62B8]; the message's answer
(event 3, 0x514208..0x514229) then goes there. Choosing another options tab (0x8FC..0x900) does the same first
(0x5140CC..0x514125). The message is `SetupBox::MessageBoxA` style 0 (0x4115E8): the text and one OK button.
**Faithful.**

### The audio of each video

**Faithful.** No `.bik` makes sound: all sound comes from LHaudio.

| video | audio |
|---|---|
| logo | none (GAudio does not exist yet) |
| pre_intro | `audio\music\intro\trailer.sad` via LHMusic (vol 127, no fade), cut off abruptly with `LHMusicStop(0)` |
| tips | none |
| intro | the game paused → ambience off (`AtmosProcess(0)`); the music that was playing continues (in Land 1, `intro.sad` from `START_MUSIC(54)`, if it has not finished); during the fade `ProcessMusic` exits through "nothing" without stopping the track and there is no `LHAtmosProcess(1)` (0x427DF8 / 0x4270B1 read +0x250188) |
| fall | the same, plus the `FallingSpell::Draw` effects timed to the video (`t = frame·1000/fps`) and `LHMusicStop(1)` at 43.9 s |
| ESC | `StartScriptMusic(0)`: the script music is released |

## openblack

Status: **done**, except ESC's other key paths (Pending). The full-screen player is `video::Get()`
(`src/Video/VideoPlayer.{h,cpp}`), the falling spell's film `src/Video/FallingSpellVideo.{h,cpp}`, the start-up screens
`src/GameStartup.cpp` with their rules in `src/Gui/StartupRules.h` and `src/Gui/LoadingScreenRules.h`. The step-by-step counterparts are under [Counterparts in the code](#counterparts-in-the-code).

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The falling spell (fall.bik)**

- Rumble 0x526F6E, LaserExplode 0x526FBB, VolcanoStop 0x526FD6, Creed 0x527024, CitadelExplode 0x527074, Volcano 0x5270A2, HealChakra 0x5270EF, CreedHigh 0x527125, CreedStop 0x527181, CreedHighStop 0x52719D, CitadelExplodeEnd 0x5271E1.

**The detail change message**

- `src/Gui/GameMenu.h`: `Tell` shows a message with one OK button; `TellVideoChange` tells VIDEOCHANGE when the detail
  changed and answers false otherwise; `SelectOptionsTab` asks it first on the Options page; `_pageAfterTell` is the
  page the answer goes to (or Back's hide).

**Startup and loading screen**

- the same rules on our `Zoomer`, over our decoder through the 16-bit copy, drawn in the player's tiles (`video::PictureTiles`, half a texel inside each 256-texel tile). Only the keys and mouse buttons are taken from the window's queue while it shows, as the cleared input; a close of the window stops the logos and is left for the game, which then ends at its first frame. `--skip-logos` (a developer's option, not the original's) leaves them out.
- `--pre-intro` plays it (a developer's option, not the original's), and so would a first run, but openblack keeps no profiles yet and counts the player as having one. Each frame waits for its time by the film's rate (`video::FramesDue`, 0.5 ms at a time, at most 50000 times), is decoded by our decoder through the 16-bit copy and drawn in the player's tiles over the whole screen. The trailer's bank is registered for it and let go after the cut; nothing else plays it. The cursor is not changed.
- the same rules (raffclar's `loading::` rules, his `LoadingClock`), the picture through our decoder and the 16-bit copy (`video::ReadStill`), its blur from that copy (`loading::BlurPicture` on `graphics::rgb16`). The tips screen fades in once after the start-up screens, and shows while each land loads (`Game::LoadMap`, the testbed), drawn again at the redraw points: after the reset, after each command of the map script, before the land starts. A script's `LOAD_MAP` shows the "please wait" banner. The tip goes when the land has loaded, so a film never finds it (the player's `ClearTipVideo` is not needed). The bar's body is one texel's colour of the buttons' atlas rather than the bevelled box, the version is always V1.00, and the cursor is not changed.

### Test hooks

The video hooks and tests (`OPENBLACK_TEST_VIDEO`, `test_video_player`, `test_falling_spell_video`) are listed on
[Bink video](bink.md#test-hooks).

## Pending

- ESC: the original's box, tutorial, SetupBox and citadel paths take the key before the skip (0x63EF7A..0x63F2A4,
  [Skipping with ESC](#skipping-with-esc)); openblack skips in every case. Queued as a fix.
