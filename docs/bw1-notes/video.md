# Bink videos (.bik)

How runblack.exe v1.42 (W120) plays its five Bink videos and what openblack has: the files and when each one is shown,
the `LHVideoPlayer` class and its 16-bit copy, the `GGame` full-screen video (pacing, pause, widescreen, fade, ESC, the
3D world not drawn), the falling spell, startup and the loading screen, the audio of each video, and the openblack
player (`src/Video/`, milestones V1-V3) with the V4..V8 plan.

- [The five videos](#the-five-videos)
- [binkw32.dll and the container](#binkw32dll-and-the-container)
- [LHVideoPlayer](#lhvideoplayer)
- [The 16-bit copy (555 / 565)](#the-16-bit-copy-555--565)
- [The full-screen video](#the-full-screen-video)
  - [Starting: PlayFullScreenMovie](#starting-playfullscreenmovie)
  - [Schedule: 5 s fade and the intro at 60 s](#schedule-5-s-fade-and-the-intro-at-60-s)
  - [Pacing](#pacing)
  - [Every frame: Process3dEngine](#every-frame-process3dengine)
  - [End: DeleteVideo and FinishedVideo](#end-deletevideo-and-finishedvideo)
  - [Pause and widescreen](#pause-and-widescreen)
- [Skipping with ESC](#skipping-with-esc)
- [The falling spell (fall.bik)](#the-falling-spell-fallbik)
- [Startup and loading screen](#startup-and-loading-screen)
- [The audio of each video](#the-audio-of-each-video)
- [openblack](#openblack)
- [Plan V3..V8](#plan-v3v8)
- [Pending](#pending) · [Test hooks](#test-hooks) · [Sources](#sources)

## The five videos

**Faithful.** All of them are Bink 1 revision `i` (`BIKi`) and **none has an audio track**. There are no `.smk` files nor
other `.bik` files in the install (not in `CreatureIsle\` either).

| file | px | fps | frames | who plays it and when |
|---|---|---|---|---|
| `Data\logo.bik` | 768x512 | 15 | 2 | `PlayLogoScreens` 0x642950 from `pc_main` 0x641D5D, always at startup: two **still images** (`BinkGoto` 0x642A2C), before GAudio exists |
| `Data\pre_intro.bik` | 768x512 | 25 | 2515 (100.6 s) | `PlayPreIntroVideo` 0x6426F0 from `pc_main` 0x641D94, only with no profiles (or with `[0xD46ABD]`, (inferred) a command-line option): its own modal loop |
| `Data\INTRO.bik` | 640x360 | 24 | 1601 (66.7 s) | opcode 203 `SET_AVI_SEQUENCE(1)` → `StartAVISequence(1)` 0x68F450 → `PlayFullScreenMovie("data\intro.bik", NULL)` 0x68F489; in Land 1 it is requested by `FollowUs` (`rt_chl_code.txt` 51023) |
| `Data\Spells\fall\fall.bik` | 640x360 | 24 | 1200 (50.0 s) | `SET_AVI_SEQUENCE(2)` → `KickOffFallingSpellVideo` 0x5539A0 → `FallingSpell::Init` 0x526060 → `PlayFullScreenMovie` 0x5261FC |
| `Data\tips.bik` | 768x512 | 15 | 35 | `MakeTipVideo` 0x5F3CE0, loading screen: **35 tip images**, one per frame (`BinkGoto(tip + 1)` 0x5F3D5F) |

INTRO.bik, pre_intro.bik and fall.bik have **a single keyframe** (frame 0): they can only be decoded from the start, which
is why the original never seeks in them; logo and tips are all keyframes. Literals: 0xBFEBAC/0xBFEBBC (logo),
0xBFEB80/0xBFEB98 (pre_intro), 0xC0426C (intro), 0xBE9C38 (fall), 0xBF3BB4 (tips); `%c:\%s.%s` 0xBEC348 is the CD path.

## binkw32.dll and the container

**Faithful.** The install's `binkw32.dll`: RAD **Bink 1.0w**, 2000-12-03, **32-bit** i386 (openblack is x64: it cannot be
loaded; it only serves as an oracle outside the repo). Imports used (IAT 0x8A9934..0x8A9964): `BinkOpen` 0x844E8C,
`BinkGetSummary` 0x844EB6, `BinkDoFrame` 0x8450FE, `BinkCopyToBuffer` 0x845146, `BinkNextFrame`, `BinkWait`,
`BinkService` (0x54DAD7, fn_0054AB20, `FallingSpell::Draw` 0x526DE8), `BinkGoto` (tips, logo), `BinkGetRealtime`
0x54DC8D (only the `GGame::VideoStatistics` string 0xCD3618), `BinkSetSoundOnOff` (pre-intro, twice with 0) and
`BinkClose`. `BinkSetSoundSystem`/`BinkOpenDirectSound` are only in fn_00844C60, which **nobody calls**; `BinkSetVolume`,
`BinkPause` and `BinkBuffer*` are not imported.

`BinkOpen(nombre, 0x08080000)` 0x844E86: `0x00080000` = `BINKNOSKIP` (does not skip frames when running late);
`0x08000000` = `BINKNOTHREADEDIO` **(inferred)**, with no observable effect.

The container (public Bink 1 format, FFmpeg `libavformat/bink.c`; the exe does not read it): 44-byte header (`BIK` +
revision, size − 8, frames, largest frame, frames, width, height, fps as a fraction, video flags, audio tracks), 12
bytes per track, `frames + 1` offsets (bit 0 = keyframe, the last one = file size) and one packet per frame (per
track a 32-bit size and its bytes, then the video). Checked on all five files (`bik_frames.py`).

## LHVideoPlayer

**Faithful.** A 0x68-byte class (0x844C60..0x845B30), `new(0x68)` at the four places that use it. Fields: +0x00 width,
+0x04 height, **+0x08 integer fps** (`FileFrameRate / FileFrameRateDiv`, unsigned `div` 0x844EC2; 1 if `BinkOpen` fails,
0x844EA1), +0x0C frames, +0x10 current frame (0 after `Open`; −1 in tips/logo/pre-intro), +0x14 open, +0x18 16-bit
(is 1 in all four uses), **+0x1C the HBINK**, +0x20 "a `BinkNextFrame` is due", +0x24..+0x3C the texture mosaic,
**+0x48 the framebuffer** of `ancho*alto*2` zeroed bytes, +0x4C..+0x64 what `DrawToScreen` stores.

- `Open` fn_00844E70: `Close`, `BinkOpen`, `BinkGetSummary`, the mosaic of **256x256 textures** (640x360 and 768x512 →
  3x2 = 6; `CreateTexture` flags 0x104 in 16 bits) and `[0xEF7514] = this`. With the 3rd argument (only `MakeTipVideo`)
  and a small image, a single texture.
- `DecodeNextFrame` fn_008450B0: without an HBINK nothing; `frame > frames` → only `++frame` (0x8450C4 `jg`); otherwise,
  `BinkNextFrame` if +0x20, `BinkDoFrame` and, if it returns 0, `BinkCopyToBuffer` + `UploadToTextures` fn_00845420 and
  +0x20 = 1; always `++frame` 0x845164.
- `DrawToScreen` 0x8456C0 only stores the parameters; the drawing is done by the `thedraw` callback 0x844E30 →
  fn_00845740, one quad per tile with the given vertex colour.
- The mosaic materials: `CreateMaterial(modo 6, textura)` 0x844FC6 (SRCALPHA / INVSRCALPHA, colour and alpha
  MODULATE, no Z write), `+5 &= ~4` 0x844FD7 (no wrapping: `SetD3DTillingOff` 0x8459B1) and `+5 |= 1` 0x844FE4
  (two-sided: CULLMODE 1 = NONE, 0x8459E4).
- fn_00845740 (read in `documentacion\psys\lh3d.asm`): `w`/`h` 0 → the screen (`[0xE85058]`/`[0xE8505A]`, 0x845798..0x8457B5);
  scale `sx = w / ancho`, `sy = h / alto` (0x8457B9..0x8457D3); per tile `x0 = x + tx·256·sx`, `x1 = x0 + n·sx` (same
  in y), with `n` = 256 except for the last column/row, `ancho & 0xFF` / `alto & 0xFF` (0x84583A..0x845870); u, v from
  1/512 to `n/256 − 1/512` (0x845891..0x84596C: half a texel inwards); the colour on all four vertices
  (0x8458C1..0x8458DB); ZFUNC (0x845A16) = **ALWAYS** if the 2nd `bool` is 0 (otherwise LESSEQUAL), ZWRITEENABLE
  (0x845A49) = the 1st `bool`; `DrawAndClip2D` FVF 0x1C4 0x845A7D; at the end ZFUNC goes back to 4 (0x845ADC).
  `Process3dEngine` passes both `bool`s as 0 (0x54DC56 / 0x54DC58).
- `EnterVideoSection`/`LeaveVideoSection` 0x844C80/0x844CA0: the critical section 0xEF74F8 between the timer thread
  and the game thread.

## The 16-bit copy (555 / 565)

**Faithful.** `BinkCopyToBuffer(bink, +0x48, ancho*2, alto, 0, 0, flags)` 0x845146 with `flags = [0xEDD46C] ? 10 : 9`
(0x845119..0x845126): 9 = `BINKSURFACE555`, 10 = `BINKSURFACE565` (7 = 4444 is never used). `[0xEDD46C]` is set to 1 by
`fn_0085D7F0` 0x85D930 only if the chosen 16-bit format has the green mask 0x7E0, i.e. if the card does not
offer X1R5G5B5 → **555 is the normal path**.

- Oracle (`dev\_scratch\asistente\video\golden`, the user's DLL with the same call sequence): the 555 is
  exactly `rgb32 >> 3` and the 565 exactly `R >> 3, G >> 2, B >> 3` of the `BINKSURFACE32` of the same frame, in every
  pixel (frame 0 of all five videos for the 565; patch12 audit).
- The textures go back to 8 bits when sampled: **(inferred)** by bit replication (`(n << 3) | (n >> 2)`); this is
  done by the D3D7 driver, not the exe, and the oracle does not measure it (its PNGs assume it).
- The same `[0xEDD46C]` switch decides the truncation of the alpha-less `.raw` files in fn_00837400 (`cmp` 0x8376B9):
  the 555 branch 0x837765..0x83779F does `((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3)`; its own 565 branch
  0x8376E3..0x83771E does `(G & 0xF8) << 3`: **the low bit of green is always 0** (it is not Bink's 565).

openblack: `src/Graphics/Rgb16.h` (`graphics::rgb16`): `Pack555` (555 branch and BINKSURFACE555), `Pack565` (BINKSURFACE565,
`G >> 2`), `PackRaw565` (the 565 branch of the .raw files), `Expand5/6` (replication, inferred), `Quantize/Expand` over spans.
`test_rgb16` emulates both branches of fn_00837400 instruction by instruction.

## The full-screen video

The one for INTRO.bik and fall.bik. Fields of `g_game` (0xD0195C): **+0x250188** the `LHVideoPlayer*` (≠ NULL = there is
a video; it is what the audio reads), **+0x25018C** the frame at which the fade starts, **+0x250190** end frame,
**+0x250194** alpha (float), +0x250530 "is the intro". Globals: `VideoFramesReady` 0xD01988, `VideoFinished` 0xD0198C,
0xD01990 "video music started", 0xD01994 its bank, 0xD01998 the timer, `VideoPreviousPause` 0xD0199C, 0xD019A0 the
previous widescreen state, the byte 0xD01984 "cannot be skipped", `FallingSpellVideo` 0xCD3B10, `VideoLetterboxScale`
0xBEC16C = 1.0.

### Starting: PlayFullScreenMovie

**Faithful.** 0x54D920 (`ecx` = g_game, `ret 8`; the 2nd argument is an **audio bank name**, not the `const Rect&` of
symbols.txt, and both callers pass NULL):

1. `ClearTipVideo` 0x54D923; +0x250530 = 0; `DeleteVideo` of the previous one (0x54D939); alpha = 1.0 (0x54D946).
2. `[0xCD3B20]+0x1C = −1` 0x54D963: the GAudio (the same `ecx` as `LHBankRegister` and `StartScriptMusic`), +0x1C =
   `GameMusic::_alignmentType`, "the music that is playing" forgotten.
3. `VideoPreviousPause = (flags >> 2) & 1` 0x54D95C and `PauseGame(1)` 0x54D96A.
4. The bank: frees the previous one (fn_00428640) and registers the new one (fn_00428620 → `LHBankRegister`, null with NULL).
5. `fn_0054AB20(file, 5)` 0x54D9A5: with `[0xD46AB8] ≠ 0` it first tries `%c:\nombre.ext` on the CD; `VideoFinished = 0`,
   0xD01990 = 0, `VideoFramesReady = 0`; `new(0x68)` → +0x250188 **even if the file does not open**; `Open(path, 1, 0)`;
   **end = frames** (0x54AC4E) and **fade = frames − fps·5** (0x54AC70); a *pre-roll* of up to 10 `BinkWait`/
   `BinkService`/`Sleep(0)` that decodes nothing.
6. `fn_0054AB00` 0x54D9AD: `timeSetEvent(16 ms, resolución 5, 0x54AAE0, periódico)`.
7. `0xD019A0 = HelpSystem+0x45E8` 0x54D9BE and, if there was no widescreen, `HelpSystem::SetWideScreen(1, 0)` 0x54D9E4.
8. `HelpSystem` fn_005C6C40 0x54D9EF, **always** (also if widescreen was already on): +0x45F0 = −FLT_MAX
   (0x5C6C40), so `GetWideScreenPercentage` 0x5C6B60 = |t·0.001/wideScreenTime| clamped to [0, 1] gives 1 immediately:
   the bars appear fully in the first frame and stay that way (while paused 0 is added, fn_005C6BB0). When it ends,
   `SetWideScreen(0)` leaves +0x45F0 = (1 − 1)·2000 = 0 and the bars go away over 2 s of game clock.

If the file does not open: fps 1, frames 0 → end 0 and the next `Process3dEngine` deletes it (0 ≥ 0).
If one video replaces another, `VideoPreviousPause` and 0xD019A0 are taken **already paused and with widescreen**: when
the second ends the game stays paused (faithful by reading, not seen in the game).

### Schedule: 5 s fade and the intro at 60 s

**Faithful.** By default the video lasts until its last frame with a 5 s fade. `StartAVISequence(1)` 0x68F450, if there
is a player, overrides the schedule: **fade = fps·58** (`fps*7`, `+fps*28`, `*2`, 0x68F4A1..0x68F4AF) and **end = fps·60**
(`imul 0x3C` 0x68F4C3), +0x250530 = 1, and `SetupScreenFadeBackToNormal(0)` 0x68F4E9 (only if there is a player, which
there always is). INTRO.bik: fade from 1392 to 1440; **frames 1440..1600 are never seen**. fall.bik: **no fade of its own**
(`FallingSpell::Init` sets fade = end, 0x5262C4..0x5262CF); it only fades when the spell ends (48 frames).
`SET_AVI_SEQUENCE(2)`: `KickOffFallingSpellVideo` and `SetupScreenFadeBackToNormal(0)`.

### Pacing

**Faithful.** The 16 ms timer (callback 0x54AAE0, checks the id 0xD01998) calls `VideoPoll(1, 1)` 0x54AA40: if
`frame > fin` (0x54AA6A `jle`) `DeleteVideo`; otherwise, the pending `BinkNextFrame`, and if `BinkWait` says it is time,
**one** `DecodeNextFrame`, `fn_0054A9B0` (the bank music from frame 3 onwards; never with a NULL bank) and
`++VideoFramesReady`. That is, Bink keeps the time and the timer decodes at most one frame every 16 ms, on
another thread, with `BINKNOSKIP`.

### Every frame: Process3dEngine

**Faithful.** `GGame::Process3dEngine` 0x54DA80, after `LH3DRender::StartFrame` 0x54DAB5 and inside the video section:

1. `BinkService` 0x54DAD7; if `VideoFinished`, `FinishedVideo` 0x54DAE7. Without a video, straight to step 6.
2. **alpha = 1.0 every frame** (0x54DB05), before looking at anything.
3. `frame ≥ fin` (0x54DB10 `jl`) → alpha = 0, `DeleteVideo`, go to step 6.
4. `frame > fundido` (0x54DB2D `jle`) → if the pause state is not `VideoPreviousPause`, `PauseGame(VideoPreviousPause)`
   0x54DB42 (the game runs again during the fade) and
   **alpha = 1 − (f − start) / (|end − start| + 1)** (0x54DB4A..0x54DB7F: integers, `fild`/`fidiv`/`fsubr 1.0`, FPU at 24
   bits).
5. If alpha is exactly 1.0 (bitwise comparison), it is not the spell and there is no new frame: up to **1000 ×
   (`VideoPoll(0, 0)` + `Good_sleep_us(500)`)** = 0.5 s of waiting (0x54DB9E..0x54DBD1). Then, if there is still a video:
   - bars: `ftol((H − W·0.5625) · VideoLetterboxScale) / 2` (0x54DBEB..0x54DC0F, `cdq; sub; sar`, signed,
     **not clamped to 0**: on screens wider than 16:9 they come out negative and the image overflows above and below;
     `ScreenFade::LetterboxHeight` fn_0081E8B0 does clamp, it is not the same one);
   - colour: `0x00FFFFFF | ftol(base · alpha) << 24`, base **0xFF**, or **0x50** with `FallingSpellVideo`
     (0x54DC11..0x54DC4D, `__ftol` truncates);
   - `DrawToScreen(color, 0, barras, W + 1, H − 2·barras + 1)` 0x54DC6D (W, H = `[0xE839E4]`/`[0xE839E8]`),
     `VideoFramesReady = 0`, the statistics string.
6. **The 3D world is not drawn** if there is a video, alpha == 1.0 and it is not the spell (0x54DD5E..0x54DD7D → 0x54E2A4).
   What comes afterwards is done: `GScript::ProcessFade` / `Temple::UpdateFade` and `HelpSystem::Draw3D` (cinema bars and
   texts) 0x54E2D7..0x54E2ED. During the fade (alpha < 1) the world is drawn and the video is blended on top.

### End: DeleteVideo and FinishedVideo

**Faithful.** `DeleteVideo(bool)` 0x54A940 (the `bool` is unused): destroys the player, +0x250188 = 0,
**`++VideoFinished`**, 0xD01984 = 0, `timeKillEvent`, 0xD01998 = 0, 0xD01990 = 0. It is called by `Process3dEngine` (end),
`VideoPoll` (past the end), the skip, `PlayFullScreenMovie` and `fn_0054AB20`.
`FinishedVideo` 0x54D8D0, on the next frame: `PauseGame(VideoPreviousPause)`; if `HelpSystem+0x45E8 ≠ 0xD019A0`,
`SetWideScreen(actual == 0, 0)` 0x54D902; 0xD01984 = 0; `VideoFinished = 0`. Since `fn_0054AB20` sets `VideoFinished = 0`
after its `DeleteVideo`, a replaced video does not call `FinishedVideo`.

### Pause and widescreen

**Faithful.** `PauseGame` 0x54AE20: nothing if it is already in that state or in multiplayer; it toggles bit 2 of `flags`
and stops or starts the game clock (`GameClock` +0x205D68). During the opaque video the game is **paused** (there are no
turns; in `EndTurn` only `AtmosProcess(0)`); during the fade the previous pause state returns. Widescreen goes with
owner 0 (`+0x45EC = 0`).

## Skipping with ESC

**Faithful.** `GGame::ProcessKey` 0x63EF20, key `LH_KEY 1` (ESC; index 0 of the table 0x63F6B8 → 0x63F6A0 → 0x63F3B9):

- With `[0x9A161C]` (0x10) or `[0x9A161E]` (0x20) in the modifiers → nothing (0x63F3C6 / 0x63F3D6). **(inferred)** Shift
  and Ctrl: the same function translates them to DIK 0x2A / 0x1D at 0x63F2C6..0x63F2EC (and `[0x9A1620]` 0x40 → 0x38 = Alt,
  which the skip does not look at). They are constants (nobody writes them).
- With a video and without the byte 0xD01984 → `fn_0054DA00` 0x63F3F5 and `GAudio::StartScriptMusic(0)` 0x63F402 (the
  script music is released). Without a video, the normal ESC (0x63F4D7).
- Before the table, ProcessKey has other ESC paths that exit without getting here (0x63EF7A..0x63F2A4): the box of
  `+0x205A10`, in land 6 the fade back and the tutorial, an active `SetupBox` or dialogue, inside the
  citadel. **Pending** a full read; none of them exists in openblack.

`fn_0054DA00`: with `FallingSpellVideo` → `EndFallingSpellVideo` 0x553A10 and nothing else. Otherwise, with a video: if it
is already in the fade (`frame > ini`) → `DeleteVideo` right away; otherwise, **start = frame** and
**end = min(frame + 48, frames)** (0x54DA47 `add 0x30`, signed `jle`; 2 s at 24 fps) and `PauseGame(VideoPreviousPause)`
0x54DA63. The byte 0xD01984 is set by `pc_main` 0x641E49 after the first-profile box and cleared by
`DeleteVideo`/`FinishedVideo`: **the first video of a new profile cannot be skipped** (inferred: what is seen in the game).

## The falling spell (fall.bik)

**Faithful (V6, `src/Video/FallingSpellVideo.{h,cpp}`).** Read in full: `KickOffFallingSpellVideo` 0x5539A0,
`EndFallingSpellVideo` 0x553A10, `FallingSpell::Init` 0x526060, `Close` 0x5264A0, the unnamed update 0x526E00
(called by `fn_00553A60` 0x553A6A), `Draw` 0x5267D0, the callback 0x526480 → 0x526530 and `Temple::UpdateFade` 0x794280.

- **Startup.** `SET_AVI_SEQUENCE(on, 2)` → `StartAVISequence` 0x68F45F → `KickOffFallingSpellVideo` and always
  `SetupScreenFadeBackToNormal(0)` 0x68F471. KickOff **does nothing** if the local player has no creature
  (0x5539A5..0x5539C0: `g_game + 0xA64 + 0xA60·[+0x205A59]` = `players[PlayerIndex].creature`, `GPlayer` of 0xA60 bytes
  from +0x18 with `creature` at +0xA4C). If it has one: `EndFallingSpellVideo` of the previous one (0x5539C4),
  `+0x205A28 = 2`, `new(0x40)` (Game.cpp line 0x1ADD), ctor fn_00527240 (+0 = +4 = 0), `FallingSpellVideo` = the object,
  `Init`. `SET_AVI_SEQUENCE(off, 2)` → `StopAVISequence` 0x68F4F7 → `EndFallingSpellVideo` (without an object, nothing).
- **Init.** Loads the camera path `data\spells\fall\fall.cm2`, creates a `CreatureFalling` (0x57B8 bytes, vtable
  0x8D8BD8) of the player's creature with its hand glows, `PlayFullScreenMovie("data\spells\fall\fall.bik",
  NULL)` 0x5261FC (**pause and widescreen like the intro**: the game does stop) and, with a player: fps ≤ 0 → 0x18,
  the camera at the path point for ms 0 (×0.8), and **+0x25018C = +0x250190** (0x5262C4..0x5262CF): **the video has no
  fade of its own** (the 5 s of `fn_0054AB20` are cancelled). Then +0 = 1, +0x10 the saved camera position, an
  `LH3DSprite` and 16 sparks (+0x34/+0x38), `+0x1C = +0x20 = +0x24 = 0`, a `LightBurst` (+0x3C) that is initialised twice,
  +0x28 = 0, +0x30 = 1.0, +0x2C = 0 and the end-of-frame callback 0x526480 (the light flashes, from state 2 onwards).
- **Every frame (mode 2).** `Process3dEngine` 0x54DD83: with `+0x205A28 == 2` (case 2, 0x54DD9B..0x54DE02) **the land is
  not drawn** (case 0 is 0x54DE57): `LH3DAtmos::Update3D`, `g_mode_cleaning = 0`, the update 0x526E00, and if there is
  no video or **`+0x20 == 4`** → `EndFallingSpellVideo` 0x54DDD6; otherwise `FallingSpell::Draw` (the video **first**, with
  `LHVideoPlayer::thedraw(0)` 0x52689F and base alpha 0x50, then the falling creature `DrawNow` 0x526A42 with its
  time-based tint, and the sparks) and the liquid particles. Other readers of +0x205A28: `GCamera::Update` 0x44233C..,
  fn_00516CB0, fn_00517080, `AddPlayerSparkles` 0x55264D, fn_005739F0, `Process3dEngine` 0x54E3D2 / 0x54E4BC
  (the weather `Render2D` is skipped in mode 2) **(not ported)**.
- **The update 0x526E00.** Without a video: `++(+0x20)` and nothing else. With a video, `t = frame·1000/fps` (0x526E61,
  integers): the creature advances `t − (+0xC)` (+0xC = 100 after Init), the camera along the path and `ChangeFov(π/4)`.
  Sounds by `+0x24` (each `if` after the previous one: a single call can pass several): **> 17 450 ms** 151 ScreenRumble
  (ScriptSfx); **> 19 450** 56 S_LasersbeamExplode_02 (Spells) and stops 172 (InGame, owner 1); **> 31 650** 166 G_Creed_01.
  State `+0x20`: **0 → 1 at > 13 450 ms** (+0x1C = 1; 168 G_CitadelExplode_01 and 172 G_Volcano_02 owner 1);
  **1 → 2 at > 37 750** (30 S_HealChakra; 166 owner 2 with pitch 0x85); **2 → 3 at > 43 900**: temple fade to white
  (`[0xE06024] = 0xFFFFFF`, target `[0xE06020] = 1.0`, current `[0xC2A150] = 0`, `[0xE06028] = 0`), stops 166 (owners 0 and
  2), **`LHMusicStop(1)` 0x5271B0** (all music fades out) and 168; **3 → 4** when `[0xE06028] ≠ 0` (the fade reached
  white): target 0, current 1.0, `[0xE06028] = 0` (comes back from white). Banks: +0x3AC InGame, +0x3B4 Spells,
  +0x3BC ScriptSfx (`sfx_inventory.md` 0x526F6E..0x5271E1).
- **`Temple::UpdateFade`** (0x54E2DE, every frame unless target == current and the mode is not 1; mode 3 never):
  advances `g_delta_time · 0,001` per frame (1.0 per second, real clock, also while paused). Going up, when it overshoots:
  `++[0xE06028]`, current = target and **target = 0** (it comes back down by itself); going down, when it overshoots:
  current = target, `++[0xE06028]` and with target ≤ 0 the colour goes to 0. It writes `(alpha << 24) + rgb` (alpha 0xFF
  above 1, otherwise `ftol(actual·255)`) into `[0xFA51D8]` with fn_0053CE60: **the same screen-fade colour as the
  script**. The FPU runs at 24 bits (fn_007DEE00, called after `FinishFrame` 0x54E426 / 0x54E4D1 and in `EndTurn`): every
  step rounds to `float` and `float`s are compared; a step that lands **exactly** on the target does not count
  (target == current: `Process3dEngine` stops calling it and the fade stays) **(inferred: that nothing raises the
  precision in between)**. At startup, `DoLogo` (0x5FA0E1..0x5FA0FF, first pass of `GGame::Loop`) sets current = target
  = 0 and `[0xE06028] = 0`: at rest.
- **The end.** `EndFallingSpellVideo`: `+0x205A28 = 0`, `Close` (removes the callback, deletes the creature and the path,
  restores the **model light** [0xEA9E90] (not the camera; fn_0081E1F0 0x5264F4), frees sparks and flashes), `delete`, `FallingSpellVideo = NULL` and `fn_0054DA00`. Without
  `FallingSpellVideo` that skip is the **normal** one: a 48-frame fade from the current one **with base 0xFF** (the first
  frame, alpha 1.0, covers the world) and the pause restored. In time: at 43.9 s the white rises over 1 s, at ~44.9 s the
  spell ends, the video fades out over 2 s and the white goes down over 1 s. ESC (`fn_0054DA00` 0x54DA0C) does the same
  earlier.

## Startup and loading screen

**Faithful by reading; pending in openblack (there is no front end nor loading screen).**

- `PlayLogoScreens` 0x642950: `Open(logo.bik, 1, 0)`, two passes with `BinkGoto`, `DecodeNextFrame`,
  `UploadToTextures` and a fade with a `Zoomer`; no audio.
- `PlayPreIntroVideo` 0x6426F0: its own loop (does not use `GGame`): `frame = −1`, `BinkSetSoundOnOff(0)` 0x6427AB,
  `trailer.sad` via `LHMusicPlay` 0x642806, cursor hidden; on each pass it waits for `BinkWait` (up to 50000 × 0.5 ms),
  decodes, `DrawToScreen(0xFFFFFFFF, 0, 0, W, H)` (no bars), `Flip`; continues while `frame < frames − 1` and
  `[0xE85474] ≠ 1` (inferred: a key). When it ends `BinkSetSoundOnOff(0)` again, `LHMusicStop(0)` 0x642907.
- `MakeTipVideo` 0x5F3CE0: `Open(".\data\tips.bik", 1, 1)` (one texture), `BinkGoto(consejo + 1)`, one image.
  `ClearTipVideo` 0x5F3D90 deletes it (also from `PlayFullScreenMovie`).
- `fn_0054AD00` (blocking playback with `VideoTimerSection`) has no callers.

## The audio of each video

**Faithful** (session *audio*, `video_audio.md`). No `.bik` makes sound: all sound comes from LHaudio.

| video | audio |
|---|---|
| logo | none (GAudio does not exist yet) |
| pre_intro | `audio\music\intro\trailer.sad` via LHMusic (vol 127, no fade), cut off abruptly with `LHMusicStop(0)` |
| tips | none |
| intro | the game paused → ambience off (`AtmosProcess(0)`); the music that was playing continues (in Land 1, `intro.sad` from `START_MUSIC(54)`, if it has not finished); during the fade `ProcessMusic` exits through "nothing" without stopping the track and there is no `LHAtmosProcess(1)` (0x427DF8 / 0x4270B1 read +0x250188) |
| fall | the same, plus the `FallingSpell::Draw` effects timed to the video (`t = frame·1000/fps`) and `LHMusicStop(1)` at 43.9 s |
| ESC | `StartScriptMusic(0)`: the script music is released |

## The decoder (V5): FFmpeg + the binkw32 colours

**Faithful (checked bit by bit).** The image comes out identical to that of the game's `binkw32.dll` 1.0w in the 55
golden frames of the five videos (0,1,2,10,100 and the last of INTRO/pre_intro/fall; all of logo and tips): 19 673 088
identical 555 texels, the 565 of frame 0 of all five as well, and the RGBA8 equal to the oracle's `BINKSURFACE32`; also
via `BinkGoto` (tips/logo in shuffled order). Test: `dev\_scratch\asistente\video\ffmpeg\check\` and its `README.md`.

- **Decoder:** libavcodec's `bink` (FFmpeg 7.1.2) fed with the packets from `BikFile`: `codec_tag` =
  `BIK` + revision, the image size and the 4 bytes of video flags from the header as *extradata* (the same that
  `libavformat/bink.c` would give; libavformat is not used). It outputs YUV 4:2:0 and its planes match RAD's in all the
  golden frames.
- **The BinkCopyToBuffer colours** (inside the DLL, not the exe; reconstructed from the golden frames):
  chroma **not interpolated** (one U/V per 2x2 block) and four 16.16 tables truncated towards zero separately, added to
  a *floored* luma and then clamped to 0..255:
  `y' = max(0, 76309·(Y−16) >> 16)` (Y < 16 → 0; Y > 235 is not clamped);
  `R = y' + trunc(104597·(V−128)/65536)`; `G = y' + trunc(−25675·(U−128)/65536) + trunc(−53279·(V−128)/65536)`;
  `B = y' + trunc(132202·(U−128)/65536)`. These are the usual limited-range BT.601 constants **except for B's**:
  the classic 132201 fails at U = 70 (RAD gives −117). R, G and B are determined by (Y, U, V) in the 449 685 distinct
  cases of the golden frames and the model gets all of them right.
- **(approximate)** The data pins each constant only to an interval (Y 76305..76309, Rv 104579..104605, Bu
  132202..132221, Gu 25674..25683, Gv 53248..53302); within them the tables only change at extreme chroma values. In the
  five complete videos U goes from 16 to 212 and V from 40 to 219: the only doubtful case that appears is **V = 219** (10
  chroma samples in the whole of pre_intro.bik), where G could be 1 less (`k_GreenFromV` in `src/Video/BinkYuv.h`).
- **Trimmed FFmpeg:** `vcpkg-overlay-ports/ffmpeg` (vcpkg's 7.1.2#3 port with
  `--disable-everything --disable-network --enable-decoder=bink --enable-demuxer=bink --enable-protocol=file`, without
  hardware acceleration nor Media Foundation) and in `vcpkg.json` only the `avcodec` *feature*. On Windows x64:
  `avcodec-61.dll` + `avutil-59.dll` (~1.2 MB), copied next to the exe by vcpkg's *applocal* step (like `lua.dll`).
  First build ~25 min (almost all of it the `configure` in msys).
- **Licence:** without `gpl`/`version3`/`nonfree` FFmpeg is **LGPL-2.1-or-later** (`libavcodec/bink.c` included),
  compatible with openblack's GPL-3; the overlay aborts if any of those *features* is requested.

## openblack

Milestones V1 and V2 (draft patch12, session *asistente*; inert until V4: nobody calls `Play` yet).

- `src/Video/BikFile.{h,cpp}` (**V1**, faithful to the format): reads and validates the container like `bik_frames.py`
  (`BIK` signature, non-zero image and fps, tables inside, increasing offsets with the last = size, audio sizes); `Fps()` =
  the integer division of 0x844EC2; `FrameData`/`VideoData`/`AudioData`, keyframes.
- `src/Video/VideoDecoder.h`: `IVideoDecoder` (`Open`, `DecodeNext(i)` → RGBA8; empty = the frame failed and the previous
  image stays) and `NullVideoDecoder` (opaque black: for the tests, and as a fallback if the decoder rejects a
  valid movie, which is then shown in black with its pause, fade and skip).
- `src/Video/FfmpegDecoder.{h,cpp}` (**V5**, faithful: see above) and `src/Video/BinkYuv.h` (the binkw32 colour
  tables): the default decoder of `GameHooks()`. `DecodeNext(i)` in order decodes one packet; another `i`
  (`BinkGoto`) goes back to the last *key frame* <= i and decodes from there.
- `src/Video/VideoPlayer.{h,cpp}` (**V2**): `video::VideoPlayer` with the original's fields and their address. `Play` =
  `PlayFullScreenMovie` + `fn_0054AB20` (the player exists even if it does not open); `SetSchedule`/`ScheduleIntro` = 58·fps /
  60·fps; `Process(realMs)` = `Process3dEngine` 0x54DAB5..0x54DD76 + `VideoPoll` + `DecodeNextFrame`; `Skip` =
  fn_0054DA00; `EscapeKey(shift, ctrl)` = ProcessKey 0x63F3B9..0x63F402; `Stop` = `DeleteVideo`; `FinishedVideo`
  private; `IsPlaying()` atomic (for the audio, from any thread); `CoversScreen()` = 0x54DD5E; `GetFrame()` (RGBA8
  as sampled, the 16-bit buffer, vertex colour, `serial`). Pure functions `FadeAlpha`, `VertexColour`,
  `FullScreenRect` (unclamped bars), `FramesDue`. `video::Get()` / `video::IsPlaying()` with `GameHooks()`:
  `game_clock::Pause/IsPaused`, `help::Get()->SetWideScreen(on, 0)` (which moves the `ScreenFade` bars and notifies the
  audio with owner 0) and `game_music::ScriptStopMusic`.
- `src/Video/FallingSpellVideo.{h,cpp}` (**V6**): `video::FallingSpellVideo` (the `FallingSpell` as far as the video is
  concerned, and +0x205A28): `KickOff` (0x5539A0, with the `hasCreature` hook: **(inferred)** a `Creature` entity of
  `PLAYER_ONE`), `Start` (what comes after the check; used by `OPENBLACK_TEST_VIDEO=fall`), `End` (0x553A10 →
  `VideoPlayer::Skip`), `ProcessFrame(realMs)` (case 2 + `Temple::UpdateFade`), `Update` (0x526E00), `HidesWorld()` (mode 2),
  `State()`, `SoundState()`; pure `TempleFade` (0x794280, in `float` like the 24-bit FPU). Hooks (`GameHooks()`):
  `sound(FallingSpellSound)` → `audio::PlaySoundEffect(PlayOptions)` 2D (owner `Owner::None()` / `Key(1)` / `Key(2)`,
  pitch 133 only at 0x527119) or `audio::StopSoundEffect(muestra, dueño, banco)`; `musicStop(1)` → `audio::MusicStop(1)`;
  `setScreenFadeColour` → `ScreenFade::SetColour` (new, fn_0053CE60). `VideoPlayer::GameHooks().endFallingSpellVideo` → `End()`. Opcode 203
  (`CHLApi.cpp`): sequence 2 with `on` → `KickOff()` + `FadeBackToNormal(0)`, without `on` → `End()`, after the `FreeStart()`
  like V4. `Game.cpp`: `GetFallingSpell().ProcessFrame(FrameRealMs())` right after `video::Get().Process`.
  `Renderer::DrawScene`: with `HidesWorld()` the same path as `CoversScreen()` (only the video and the final layers).
- `src/Game.cpp`: `video::Get().Process(game_clock::FrameRealMs())` after `UpdateRealClock()` (after the turns,
  like `Process3dEngine` after the turn loop); in `ProcessEvents`, ESC with a video → `EscapeKey` (without a video it exits
  openblack as always: openblack's ESC is not the original's).
- `src/Graphics/Renderer.cpp` (**V3**, session *sistemas*): `Renderer::DrawVideoOverlay` in `RenderPass::ScreenOverlay`,
  after the hand message, in the order of `LH3DRender::FinishFrame` 0x82F460 (`Renderer::DrawFinishFrameOverlays`):
  first the bars if pct ≠ 0 (0x82F652..0x82F6DD, fn_0081E590 twice, height (int)((h − w·0.5625)·pct)/2 from
  fn_0081E8B0), then the callbacks with bit 0x80000000 (0x82F6E5..0x82F718), among them `thedraw` 0x844E30
  (registered with 1 at 0x54B62D; `RegisterFinishFrameCallback` 0x82F2C0 sets the bit), and finally the script fade
  fn_0086FEE0 (0x82F753), which paints the bars again on top of its colour. The video covers the bars exactly at their
  edge: with pct = 1 the `FullScreenRect` rectangle fits exactly between them. One RGBA8 `clamp` texture of the video's
  size, recreated if the size changes and uploaded with `updateTexture2D` only when `serial` changes (`UploadToTextures`
  0x84514E). The fn_00845740 quads tile by tile over `video::FullScreenRect` (0x54DBEB..0x54DC6D) with the **same
  texels** as each 256x256 tile (half a texel inwards, no wrapping: the filter does not reach the neighbouring tile), the
  colour `Frame::colour` (0x54DC11..0x54DC4D) as vertex colour, and the `render_modes::State` of the two-sided mode 6
  material with ZFUNC ALWAYS and no Z. Program `WorldQuad` (vs_blob + fs_world_quad: colour = texture × diffuse,
  alpha = `s_alpha.r` × diffuse) with a white 1x1 R8 texture as `s_alpha`: **no new shader**.
  `Renderer::DrawScene`: with `video::Get().CoversScreen()` (0x54DD5E..0x54DD7D → 0x54E2A4) nothing of the world is drawn
  (no shadows, no reflection, no sky, no hand message); only the video and `DrawScreenOverlay` (the script fade
  and the bars, 0x54E2D7..0x54E2ED), in the same order bars, video, fade. Checked in the game: with `fall` the world comes out × 0.686 = 1 − 0x50/255 in all
  the measured pixels; with `intro` the screen is the null decoder's black (without the clear blue).

Differences:

- **(approximate)** Pacing: there is no 16 ms thread nor `BinkWait`; `Process` decodes all the frames that are due by the
  wall clock since the first `Process` (frame i at i·den·1000/num ms). Same frames at the same times; after a
  long stall openblack decodes the backlog in one go (only the last one is shown) where the original would go one by one
  every 16 ms.
- **(approximate)** The wait of up to 0.5 s (0x54DB85..0x54DBD1) does not block: the last image stays.
- Faithful since V5: the conversion to 16 bits starts from the decoder's RGBA8 and gives the same 555/565 as the DLL's
  YUV→555 (checked against the golden frames).
- **(inferred)** One skip per key press: SDL key repeats are ignored.
- V6: the 16 sparks (puffs of smoke.raw), the `LightBurst` and the `fall.cm2` path (computed, not yet applied to the
  camera) are ported by milagros2 (miracles.md, "The falling spell"). Not ported: the `CreatureFalling`, the camera with
  `ChangeFov(π/4)`, the creature's tint and the `SetScalePowerTime` calls of `Draw`, `LH3DAtmos::Update3D`/`Render2D`, the other
  readers of +0x205A28 and the rewriting of +0x1C by `Draw`. What is seen in openblack in mode 2: the video at 31 % over
  **(inferred)** openblack's clear colour (what lies underneath in the original has not been read: no clear has been
  identified in `StartFrame`).
- **(approximate)** V6: the fps ≤ 0 → 0x18 of `Init`/0x526E4E is not written into the player (it is used when computing
  `t`; `BikFile` does not open fps 0).
- **(approximate)** V6: in mode 2 the original draws the video inside `Process3dEngine` (`thedraw` 0x52689F) and the
  bands and the fade of `FinishFrame` go on top; openblack now draws the original's order in mode 2: video, puffs, flashes, bands, fade. Same image with the bands
  at 100 % (the video's *letterbox* is its height). On the frame in which `EndFallingSpellVideo` runs the original still
  draws the video with base 0x50 (the colour was stored by `DrawToScreen` 0x54DC6D); openblack, with 0xFF (under the
  almost opaque white).
- Not ported: the sound bank (both callers pass NULL), `ClearTipVideo`, the CD path, the statistics
  string, the 256x256 mosaic (a single texture with the same texels per tile), and `GAudio+0x1C = −1` (audio has no
  way to do it; `ProcessMusic` repeats it during the fade).
- fn_005C6C40 is `ScreenFade::SnapWideScreen`, via the `snapWideScreen` hook of `VideoPlayer::Hooks`, which `Play` always
  calls, like the original (0x54D9EF, after the jump at 0x54D9D2): also if the script already had widescreen on.
- **(inferred)** V3: the alpha of the 16-bit textures is 1 (`CreateTexture` flags 0x104; an A1R5G5B5 with Bink's bit 15
  at 0 would not be visible); the driver's bilinear filter; the pixels with bgfx's centre,
  without D3D7's half pixel (like the other `ScreenOverlay` rectangles).
- **(approximate)** V3: while the video covers the screen openblack clears to 0x274659 as always (the original does not
  clear); it is only visible outside the video rectangle, on screens that are not 16:9 and before the bars arrive.

## Plan V3..V8

| milestone | what | owner / blocker |
|---|---|---|
| V3 | **Done** (session *sistemas*): `Renderer::DrawVideoOverlay`, the world not drawn with `CoversScreen()`, `OPENBLACK_TEST_VIDEO`; no new shader (`WorldQuad`) | — |
| V4 (done, session asistente) | Opcode 203 `SetAviSequence` (`CHLApi.cpp`): sequence 1 → `video::Get().Play(data\intro.bik)` + `ScheduleIntro()` before the `FadeBackToNormal(0)`; the `FreeStart()` of the `game.skip-intro` mod stays (without video) | shared `CHLApi.cpp` |
| V5 | **Done**: trimmed FFmpeg (`--enable-decoder=bink`, LGPL only) behind `IVideoDecoder`, bit for bit equal to binkw32 in the golden frames | — |
| V6 | **Done** (session *asistente*): `fall.bik` with `FallingSpellVideo` (mode 2, no fade of its own, states and sounds by the video time, the temple white, end at state 4), opcode 203 sequence 2, ESC; without the falling creature or the camera; sounds and `LHMusicStop(1)` via `Audio.h` | the creature, the camera and the flashes: *milagros* |
| V7 | `tips.bik` on the loading screen | blocked: there is no loading screen |
| V8 | `logo.bik` and `pre_intro.bik` at startup, with `trailer.sad` | blocked: there is no front end nor profiles; audio from *audio* |


**V4 (done):** `SET_AVI_SEQUENCE(on, 1)` calls `video::Get().Play(FindPath("Data/intro.bik"))` and `ScheduleIntro()` (58/60 s)
and removes the fade (0x68F477..0x68F4E9); a map load for the movie to play (`GGame::ClearVariables` 0x54BF28). With
the "free start" of the `game.skip-intro` mod there is no movie. **Pending a test in the game:** in Land 1 without the mod,
FollowUs does not reach 203 yet (in 4 min of play it stops before, with unported opcodes such as DANCE_CREATE); the movie
can already be seen with `OPENBLACK_TEST_VIDEO=intro`.

## Pending

- V4..V8 (table above); `parity.md` ("Vídeo Bink") is "in progress" since V3.
- `OPENBLACK_VIDEO_TRACE` does not exist yet.
- *audio*: connect `GameQueries::videoPlaying` to `video::IsPlaying()` (`MakeMusicQueries` in `Game.cpp` and the queries
  of `AudioSystem`); a way to forget `GameMusic::_alignmentType` for 0x54D963.
- ProcessKey 0x63EF7A..0x63F2A4: the other ESC paths, not fully read.
- Check in the game that the first video of a new profile cannot be skipped, and with `OPENBLACK_TEST_VIDEO=fall` the
  white at 43.9 s, the end at ~44.9 s and the opaque 48-frame fade.
- Hear in the game the 11 sounds and the music stop at 43.9 s (`OPENBLACK_TEST_VIDEO=fall`).
- *milagros*: the `CreatureFalling`, the `fall.cm2` camera, the sparks and the flashes of `FallingSpell::Draw`.

## Test hooks

- `test_bik_file` (6): synthetic files (format, audio, 13 rejections) and the five real `.bik` files with
  `OPENBLACK_TEST_GAME_PATH` (or `OPENBLACK_TEST_BW_ROOT`); without the folder 2 are skipped.
- `test_video_player` (19): fake hooks and clock, synthetic `.bik` files: pause and widescreen saved and restored,
  file that does not open, pacing, fade and end, intro schedule (1392/1440), skip (48 frames, cap, second
  skip), ESC with Shift/Ctrl/byte 0xD01984, spell, replaced movie, 555 and 565, null decoder, failed
  frames.
- `test_falling_spell_video` (15): `FallingSpellFilmMs`, `TempleFade` (rises, turns around by itself, goes down, colour to 0,
  exactly on the target, above 1, when it runs), without a creature nothing, KickOff (mode 2, pause, 0x50, fade = end), no
  fade of its own past frame 1080, states and the 11 sounds with their addresses, the white and the end at state 4 (26
  frames of 40 ms, the normal skip of 48), ESC, StopAVISequence, short video, video that does not open, KickOff twice.
- `test_rgb16` (5): the 555 and 565 branches of fn_00837400 emulated, expansion, truncations, spans.
- `test_ffmpeg_decoder` (7): the colour tables and the non-interpolated chroma; with the game folder, the CRC-32 of the
  555 of logo.bik (frames 0 and 1, and back to 0), tips.bik by seeking (34, 20, 34) and INTRO.bik frame 10 in order,
  against the DLL's golden `.bin` files (only the CRCs are in the test).
- Game: `OPENBLACK_TEST_VIDEO=<intro|fall|ruta>` (Game.cpp, when loading the map): `intro` = `Data\intro.bik` +
  `ScheduleIntro()` (60 s), `fall` = `video::GetFallingSpell().Start()` (KickOff without the creature check:
  mode 2, alpha 0x50 without world, states, white and end; V6), otherwise the given path. V3 shots: `dev\documentacion\unify\shots\video\intro_mid.png`
  (frame 1800, the video covers the screen) and `fall_mid.png` (frame 1500, the world × 0.686).

## Sources

- `dev\documentacion\video\original.md` (reading of the original) and `dev\documentacion\video\plan.md` (decision and plan V0..V8).
- `dev\documentacion\audio\video_audio.md` (session *audio*: the audio of each video).
- `dev\_scratch\asistente\video\golden\README.md` (the DLL's golden frames; 555 = flag 9 at 0x845146) and
  `oracle\` (the 32-bit exe that extracts them).
- `dev\_scratch\asistente\video\patch12\README.md` and `audit.md` (V1-V2 and their audit).
- `dev\_scratch\asistente\video\ffmpeg\README.md` (V5: the overlay, sizes, the reconstruction of the colours and
  the comparison with the golden frames) and `patch5\README.md` (how it is applied).
