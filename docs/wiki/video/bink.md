# Bink video

## Summary

The original plays five Bink 1 films (`logo`, `pre_intro`, `tips`, `INTRO` and `fall`), none with sound, through RAD's
`binkw32.dll` 1.0w and its own small player class, `LHVideoPlayer`, which copies each frame to a 16-bit texture
(555 or 565) and draws it as a screen rectangle. openblack plays the same films with its own Bink 1 decoder, which
gives the DLL's picture bit for bit on every frame of the five films, and its own player on the same schedule; the
films are connected and on by default (user, 2026-10-07), and the debug bar's Video menu plays any of them.

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols, and `binkw32.dll` 1.0w's; the patch did not change
the video code. How the films are scheduled, paced, paused, skipped and ended is on
[Film playback](playback.md), with the falling spell and the start-up screens.

Progress: [video](../project/progress/video/) (what our tree does of it, row by row).

### The five videos

**Faithful.** All of them are Bink 1 revision `i` (`BIKi`) and **none has an audio track**. There are no `.smk` files nor
other `.bik` files in the install (not in `CreatureIsle\` either).

| file | px | fps | frames | who plays it and when |
|---|---|---|---|---|
| `Data\logo.bik` | 768x512 | 15 | 2 | `PlayLogoScreens` 0x642950 from `pc_main` 0x641D5D, always at startup: two **still images** (`BinkGoto` 0x642A2C), before GAudio exists |
| `Data\pre_intro.bik` | 768x512 | 25 | 2515 (100.6 s) | `PlayPreIntroVideo` 0x6426F0 from `pc_main` 0x641D94, only with no profiles (or with `[0xD46ABD]`, 🔍 inferred a command-line option): its own modal loop |
| `Data\INTRO.bik` | 640x360 | 24 | 1601 (66.7 s) | opcode 203 `SET_AVI_SEQUENCE(1)` → `StartAVISequence(1)` 0x68F450 → `PlayFullScreenMovie("data\intro.bik", NULL)` 0x68F489; in Land 1 it is requested by `FollowUs` |
| `Data\Spells\fall\fall.bik` | 640x360 | 24 | 1200 (50.0 s) | `SET_AVI_SEQUENCE(2)` → `KickOffFallingSpellVideo` 0x5539A0 → `FallingSpell::Init` 0x526060 → `PlayFullScreenMovie` 0x5261FC |
| `Data\tips.bik` | 768x512 | 15 | 35 | `MakeTipVideo` 0x5F3CE0, loading screen: **35 tip images**, one per frame (`BinkGoto(tip + 1)` 0x5F3D5F) |

INTRO.bik, pre_intro.bik and fall.bik have **a single keyframe** (frame 0): they can only be decoded from the start, which
is why the original never seeks in them; logo and tips are all keyframes. Literals: 0xBFEBAC/0xBFEBBC (logo),
0xBFEB80/0xBFEB98 (pre_intro), 0xC0426C (intro), 0xBE9C38 (fall), 0xBF3BB4 (tips); `%c:\%s.%s` 0xBEC348 is the CD path.

### binkw32.dll and the container

**Faithful.** The install's `binkw32.dll`: RAD **Bink 1.0w**, 2000-12-03, **32-bit** i386 (openblack is x64: it cannot be
loaded; it only serves as an oracle outside the repo). Imports used (IAT 0x8A9934..0x8A9964): `BinkOpen` 0x844E8C,
`BinkGetSummary` 0x844EB6, `BinkDoFrame` 0x8450FE, `BinkCopyToBuffer` 0x845146, `BinkNextFrame`, `BinkWait`,
`BinkService` (0x54DAD7, fn_0054AB20, `FallingSpell::Draw` 0x526DE8), `BinkGoto` (tips, logo), `BinkGetRealtime`
0x54DC8D (only the `GGame::VideoStatistics` string 0xCD3618), `BinkSetSoundOnOff` (pre-intro, twice with 0) and
`BinkClose`. `BinkSetSoundSystem`/`BinkOpenDirectSound` are only in fn_00844C60, which **nobody calls**; `BinkSetVolume`,
`BinkPause` and `BinkBuffer*` are not imported.

`BinkOpen(name, 0x08080000)` 0x844E86: `0x00080000` = `BINKNOSKIP` (does not skip frames when running late);
`0x08000000` = `BINKNOTHREADEDIO` **🔍 inferred**, with no observable effect.

The container (public Bink 1 format, FFmpeg `libavformat/bink.c`; the exe does not read it): 44-byte header (`BIK` +
revision, size − 8, frames, largest frame, frames, width, height, fps as a fraction, video flags, audio tracks), 12
bytes per track, `frames + 1` offsets (bit 0 = keyframe, the last one = file size) and one packet per frame (per
track a 32-bit size and its bytes, then the video). Checked on all five files.

- BinkDoFrame != 0 (0x845104): the player keeps the picture it had (in the code: `FfmpegDecoder::DecodeNext`)
- BinkDoFrame of frame `index` (0, 1, 2, ... in order: Bink 1 frames are deltas of the one before): the picture as RGBA8, width * height * 4 bytes, valid up to the next call. Empty: the frame failed (BinkDoFrame != 0, 0x845104), and the player keeps the picture it had (in the code: `DecodeNext`)

### LHVideoPlayer

**Faithful.** A 0x68-byte class (0x844C60..0x845B30), `new(0x68)` at the four places that use it. Fields: +0x00 width,
+0x04 height, **+0x08 integer fps** (`FileFrameRate / FileFrameRateDiv`, unsigned `div` 0x844EC2; 1 if `BinkOpen` fails,
0x844EA1), +0x0C frames, +0x10 current frame (0 after `Open`; −1 in tips/logo/pre-intro), +0x14 open, +0x18 16-bit
(is 1 in all four uses), **+0x1C the HBINK**, +0x20 "a `BinkNextFrame` is due", +0x24..+0x3C the texture mosaic,
**+0x48 the framebuffer** of `width*height*2` zeroed bytes, +0x4C..+0x64 what `DrawToScreen` stores.

- `Open` fn_00844E70: `Close`, `BinkOpen`, `BinkGetSummary`, the mosaic of **256x256 textures** (640x360 and 768x512 →
  3x2 = 6; `CreateTexture` flags 0x104 in 16 bits) and `[0xEF7514] = this`. With the 3rd argument (only `MakeTipVideo`)
  and a small image, a single texture.
  - When `BinkOpen` fails, +0x1C stays NULL (0x844E92..0x844EA8). `BinkGetSummary` (0x844EB6..0x844EE1) fills +0 width,
    +4 height, +8 fps (`div` 0x844EBC..0x844EC2), +0xC frames and +0x10 = 0.
- The ctor is fn_00844D00. The framebuffer +0x48 is zeroed at 0x7C64EE, with pitch width × 2 (0x845141).
- `DecodeNextFrame` fn_008450B0: without an HBINK nothing; `frame > frames` → only `++frame` (0x8450C4 `jg`); otherwise,
  `BinkNextFrame` if +0x20, `BinkDoFrame` and, if it returns 0, `BinkCopyToBuffer` + `UploadToTextures` fn_00845420 and
  +0x20 = 1; always `++frame` 0x845164.
  - With no HBINK it returns at 0x8450B8. `BinkNextFrame` is at 0x8450E3 and `BinkDoFrame` at 0x8450FE (0 = decoded); a
    non-zero result (0x845104) leaves the previous picture.
- `DrawToScreen` 0x8456C0 only stores the parameters; the drawing is done by the `thedraw` callback 0x844E30 →
  fn_00845740, one quad per tile with the given vertex colour.
- The mosaic materials: `CreateMaterial(modo 6, textura)` 0x844FC6 (SRCALPHA / INVSRCALPHA, colour and alpha
  MODULATE, no Z write), `+5 &= ~4` 0x844FD7 (no wrapping: `SetD3DTillingOff` 0x8459B1) and `+5 |= 1` 0x844FE4
  (two-sided: CULLMODE 1 = NONE, 0x8459E4).
- fn_00845740: `w`/`h` 0 → the screen (`[0xE85058]`/`[0xE8505A]`, 0x845798..0x8457B5);
  scale `sx = w / width`, `sy = h / height` (0x8457B9..0x8457D3); per tile `x0 = x + tx·256·sx`, `x1 = x0 + n·sx` (same
  in y), with `n` = 256 except for the last column/row, `width & 0xFF` / `height & 0xFF` (0x84583A..0x845870); u, v from
  1/512 to `n/256 − 1/512` (0x845891..0x84596C: half a texel inwards); the colour on all four vertices
  (0x8458C1..0x8458DB); ZFUNC (0x845A16) = **ALWAYS** if the 2nd `bool` is 0 (otherwise LESSEQUAL), ZWRITEENABLE
  (0x845A49) = the 1st `bool`; `DrawAndClip2D` FVF 0x1C4 0x845A7D; at the end ZFUNC goes back to 4 (0x845ADC).
  `Process3dEngine` passes both `bool`s as 0 (0x54DC56 / 0x54DC58).
- `EnterVideoSection`/`LeaveVideoSection` 0x844C80/0x844CA0: the critical section 0xEF74F8 between the timer thread
  and the game thread.

### The 16-bit copy

**Faithful.** `BinkCopyToBuffer(bink, +0x48, width*2, height, 0, 0, flags)` 0x845146 with `flags = [0xEDD46C] ? 10 : 9`
(0x845119..0x845126): 9 = `BINKSURFACE555`, 10 = `BINKSURFACE565` (7 = 4444 is never used). `[0xEDD46C]` is set to 1 by
`fn_0085D7F0` 0x85D930 only if the chosen 16-bit format has the green mask 0x7E0, i.e. if the card does not
offer X1R5G5B5 → **555 is the normal path**.

- Oracle (the user's DLL run outside the repo with the same call sequence): the 555 is
  exactly `rgb32 >> 3` and the 565 exactly `R >> 3, G >> 2, B >> 3` of the `BINKSURFACE32` of the same frame, in every
  pixel (frame 0 of all five videos for the 565).
- The textures go back to 8 bits when sampled: **🔍 inferred** by bit replication (`(n << 3) | (n >> 2)`); this is
  done by the D3D7 driver, not the exe, and the oracle does not measure it (its PNGs assume it).
- The same `[0xEDD46C]` switch decides the truncation of the alpha-less `.raw` files in fn_00837400 (`cmp` 0x8376B9):
  the 555 branch 0x837765..0x83779F does `((R & 0xF8) << 7) | ((G & 0xF8) << 2) | (B >> 3)`; its own 565 branch
  0x8376E3..0x83771E does `(G & 0xF8) << 3`: **the low bit of green is always 0** (it is not Bink's 565).

## openblack

> **Code rules.** File loading (the `.bik` files included) goes through the resource caches; state lives in ECS
> components or Locator services, never in new globals; the decoder and the player own their resources with RAII and
> standard types (`std::unique_ptr`, `std::optional`, `std::span`); the pure functions (fade, vertex colour, rectangles,
> the temple fade) are unit tested in `test/` with fake hooks and clocks; comments describe behaviour in plain
> English, with no decompiled names or addresses (those belong here). See [the conventions](../project/refactor-conventions.md#coding-conventions-summary).

- `components/bink/{include,src}/BinkFile.{h,cpp}` (class `video::BikFile`, faithful to the format): reads and
  validates the container (`BIK` signature, non-zero image and fps, tables inside, increasing offsets with the last = size, audio sizes); `Fps()` =
  the integer division of 0x844EC2; `FrameData`/`VideoData`/`AudioData`, keyframes.
- `src/Video/VideoDecoder.h`: `IVideoDecoder` (`Open`, `DecodeNext(i)` → RGBA8; empty = the frame failed and the previous
  image stays) and `NullVideoDecoder` (opaque black: for the tests, and as a fallback if the decoder rejects a
  valid movie, which is then shown in black with its pause, fade and skip).
- `components/bink`: `BinkDecoder.{h,cpp}` with `BinkBitReader.h`, `BinkTables.h`, `BinkHuffman`, `BinkBundles` and
  `BinkBlocks` (faithful: see above), and `BinkYuv.h` (the binkw32 colour tables); compiled into `openblack_lib`.
  Built with `OPENBLACK_BINK_DECODER` (the default) and made by `GameHooks()` while the films are on. `DecodeNext(i)` in order decodes one packet; another `i`
  (`BinkGoto`) goes back to the last *key frame* <= i and decodes from there.
- `src/Debug/VideoViewer.{h,cpp}` and `VideoPlayback.h` (only with `OPENBLACK_BINK_DECODER`): the debug GUI's Video window
  (*Debug → Windows*: the "Game films" switch, the five films, and the player).
- `src/Video/VideoPlayer.{h,cpp}`: `video::VideoPlayer` with the original's fields and their address. `Play` =
  `PlayFullScreenMovie` + `fn_0054AB20` (the player exists even if it does not open); `SetSchedule`/`ScheduleIntro` = 58·fps /
  60·fps; `Process(realMs)` = `Process3dEngine` 0x54DAB5..0x54DD76 + `VideoPoll` + `DecodeNextFrame`; `Skip` =
  fn_0054DA00; `EscapeKey(shift, ctrl)` = ProcessKey 0x63F3B9..0x63F402; `Stop` = `DeleteVideo`; `FinishedVideo`
  private; `IsPlaying()` atomic (for the audio, from any thread); `CoversScreen()` = 0x54DD5E; `GetFrame()` (RGBA8
  as sampled, the 16-bit buffer, vertex colour, `serial`). Pure functions `FadeAlpha`, `VertexColour`,
  `FullScreenRect` (unclamped bars), `FramesDue`. `video::Get()` / `video::IsPlaying()` with `GameHooks()`:
  `game_clock::Pause/IsPaused`, `help::Get()->SetWideScreen(on, 0)` (which moves the `CinemaBars` and notifies the
  audio with owner 0) and `game_music::ScriptStopMusic`.
- `src/Video/FallingSpellVideo.{h,cpp}`: `video::FallingSpellVideo` (the `FallingSpell` as far as the video is
  concerned, and +0x205A28): `KickOff` (0x5539A0, with the `hasCreature` hook: **🔍 inferred** a `Creature` entity of
  `PLAYER_ONE`), `Start` (what comes after the check; used by `OPENBLACK_TEST_VIDEO=fall`), `End` (0x553A10 →
  `VideoPlayer::Skip`), `ProcessFrame(realMs)` (case 2 + `Temple::UpdateFade`), `Update` (0x526E00), `HidesWorld()` (mode 2),
  `State()`, `SoundState()`; pure `gui::ScreenFade` (`Gui/ScreenFade`, 0x794280, in `float` like the 24-bit FPU). Hooks (`GameHooks()`):
  `sound(FallingSpellSound)` → `audio::PlaySoundEffect(PlayOptions)` 2D (owner `Owner::None()` / `Key(1)` / `Key(2)`,
  pitch 133 only at 0x527119) or `audio::StopSoundEffect(sample, owner, bank)`; `musicStop(1)` → `audio::MusicStop(1)`;
  `setScreenFadeColour` → `ScriptFade::SetColour` (new, fn_0053CE60). `VideoPlayer::GameHooks().endFallingSpellVideo` → `End()`. Opcode 203
  (`CHLApi.cpp`): sequence 2 with `on` → `KickOff()` + `FadeBackToNormal(0)`, without `on` → `End()`. `Game.cpp`: `GetFallingSpell().ProcessFrame(FrameRealMs())` right after `video::Get().Process`.
  `Renderer::DrawScene`: with `HidesWorld()` the same path as `CoversScreen()` (only the video and the final layers).
- `Temple::UpdateFade` 0x794280 reaches its target only **past** it, in both directions (read with capstone): current
  [0xC2A150], target [0xE06020], rgb [0xE06024], done [0xE06028]. `fld current; fcomp target; test ah,1; jne 0x7942E6`
  goes up only while current < target (equal goes down). Up: `fadd; fst current; fcomp target; test ah,0x41; jne
  0x794320` (≤ skips), so only current > target gives `done++; current = target; target = 0` (0x7942FF). Down:
  `fsubr; fst current; fcomp target; test ah,1; je 0x794320` (≥ skips), so only current < target gives `current =
  target; done++` (0x7942B9), then the colour goes back to black only when target ≤ 0 (0x7942BE..0x7942DC). A fade
  that lands exactly on its target holds there: no turn, no reset, no colour change. The alpha is 0xFF only above 1.0
  (`fcomp [1.0]; test ah,0x41; jne` at 0x794326), else current × 255 truncated, sent to fn_0053CE60.
  `TEST(ScreenFade, LandingOnTheTarget)` pins it.
- `src/Game.cpp`: `video::Get().Process(game_clock::FrameRealMs())` after `UpdateRealClock()` (after the turns,
  like `Process3dEngine` after the turn loop); in `ProcessEvents`, ESC with a video → `EscapeKey` (without a video it exits
  openblack as always: openblack's ESC is not the original's).
- `src/Graphics/Renderer.cpp`: `Renderer::DrawVideoOverlay` in `RenderPass::ScreenOverlay`,
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
  material with ZFUNC ALWAYS and no Z. Program `WorldTextured` (vs_blob + fs_world_textured: colour = texture × diffuse,
  alpha = `s_alpha.r` × diffuse) with a white 1x1 R8 texture as `s_alpha`: **no new shader**.
  `Renderer::DrawScene`: with `video::Get().CoversScreen()` (0x54DD5E..0x54DD7D → 0x54E2A4) nothing of the world is drawn
  (no shadows, no reflection, no sky, no hand message); only the video and `DrawScreenOverlay` (the script fade
  and the bars, 0x54E2D7..0x54E2ED), in the same order bars, video, fade. Checked in the game: with `fall` the world comes out × 0.686 = 1 − 0x50/255 in all
  the measured pixels; with `intro` the screen is the null decoder's black (without the clear blue).

Differences:

- **⚠️ approximate** Pacing: there is no 16 ms thread nor `BinkWait`; `Process` decodes all the frames that are due by the
  wall clock since the first `Process` (frame i at i·den·1000/num ms). Same frames at the same times; after a
  long stall openblack decodes the backlog in one go (only the last one is shown) where the original would go one by one
  every 16 ms.
- **⚠️ approximate** The wait of up to 0.5 s (0x54DB85..0x54DBD1) does not block: the last image stays.
- Faithful: the conversion to 16 bits starts from the decoder's RGBA8 and gives the same 555/565 as the DLL's
  YUV→555 (checked against the golden frames).
- **🔍 inferred** One skip per key press: SDL key repeats are ignored.
- The 16 sparks (puffs of smoke.raw), the `LightBurst` and the `fall.cm2` path (computed, not yet applied to the
  camera) are ported ([The spell core](../magic/spell-core.md), "The falling spell"). Not ported: the `CreatureFalling`, the camera with
  `ChangeFov(π/4)`, the creature's tint and the `SetScalePowerTime` calls of `Draw`, `LH3DAtmos::Update3D`/`Render2D`, the other
  readers of +0x205A28 and the rewriting of +0x1C by `Draw`. What is seen in openblack in mode 2: the video at 31 % over
  **🔍 inferred** openblack's clear colour (what lies underneath in the original has not been read: no clear has been
  identified in `StartFrame`).
- **⚠️ approximate** The fps ≤ 0 → 0x18 of `Init`/0x526E4E is not written into the player (it is used when computing
  `t`; `BikFile` does not open fps 0).
- **⚠️ approximate** In mode 2 the original draws the video inside `Process3dEngine` (`thedraw` 0x52689F) and the
  bands and the fade of `FinishFrame` go on top; openblack now draws the original's order in mode 2: video, the falling
  creature, puffs, flashes, bands, fade (the video in the main view, the creature in the blended view after it through
  the fall's camera, the rest in the screen overlay: the puffs and flashes always lie in front of the creature, so the
  overlay's depth test ALWAYS gives the same pixels). Same image with the bands
  at 100 % (the video's *letterbox* is its height). On the frame in which `EndFallingSpellVideo` runs the original still
  draws the video with base 0x50 (the colour was stored by `DrawToScreen` 0x54DC6D); openblack, with 0xFF (under the
  almost opaque white).
- Not ported: the sound bank (both callers pass NULL), `ClearTipVideo`, the CD path, the statistics
  string, the 256x256 mosaic (a single texture with the same texels per tile), and `GAudio+0x1C = −1` (audio has no
  way to do it; `ProcessMusic` repeats it during the fade).
- fn_005C6C40 is `CinemaBars::Snap`, via the `snapWideScreen` hook of `VideoPlayer::Hooks`, which `Play` always
  calls, like the original (0x54D9EF, after the jump at 0x54D9D2): also if the script already had widescreen on.
- **🔍 inferred** The alpha of the 16-bit textures is 1 (`CreateTexture` flags 0x104; an A1R5G5B5 with Bink's bit 15
  at 0 would not be visible); the driver's bilinear filter; the pixels with bgfx's centre,
  without D3D7's half pixel (like the other `ScreenOverlay` rectangles).
- **⚠️ approximate** While the video covers the screen openblack clears to 0x274659 as always (the original does not
  clear); it is only visible outside the video rectangle, on screens that are not 16:9 and before the bars arrive.

**Opcode 203, sequence 1:** `SET_AVI_SEQUENCE(on, 1)` calls `video::Get().Play(FindPath("Data/intro.bik"))` and
`ScheduleIntro()` (58/60 s) and removes the fade (0x68F477..0x68F4E9); a map load for the movie to play
(`GGame::ClearVariables` 0x54BF28). **Checked in the game:** in Land 1, FollowUs reaches 203 and `INTRO.bik` plays (on
screen at frame ~19900); the intro then goes on to `Drag` (map-loading.md, "In game (Land 1)"). The movie can also be
seen alone with `OPENBLACK_TEST_VIDEO=intro`.

**The world is not prepared under a film.** While a full-screen film covers the screen or hides the world (the
intro's opaque part, the falling spell's mode 2), openblack skips the renderer's per-frame draw preparation, since nothing
of the world is drawn; its dirty flags add up over the film and the first frame after it prepares from that total.
That frame's draw data is what it would have been on every land with a villager or an animal (they mark the draw
dirty every frame); on a land with neither, it also takes the values that change without a mark (wind sway, tree
brightness), which without the skip would have stayed as at the last preparation. No real land is like that.

### The decoder and the binkw32 colours

**Faithful (checked bit by bit).** openblack's own Bink 1 decoder (`video::BinkDecoder`, `components/bink`) gives the
same picture as the game's `binkw32.dll` 1.0w on **every frame of the five videos** (5353 frames: logo 2, tips 35,
INTRO 1601, fall 1200, pre_intro 2515), both the 555 surface (`BinkCopyToBuffer` flags 9) and the 32-bit one (flags
3), in order and, for tips/logo, by `BinkGoto` in any order. Earlier, FFmpeg's libavcodec `bink` decoder had been
checked the same way on 55 golden frames; it was removed once ours matched everywhere.

- **The check** (outside the repo: the DLL and the frames are the game's): a 32-bit host loads the install's
  `binkw32.dll` with LoadLibrary and decodes each film with the game's own calls (`BinkOpen(name, 0x08080000)`,
  `BinkDoFrame`, `BinkNextFrame`, `BinkCopyToBuffer`), printing the zlib CRC-32 of each frame's 555 and BGR bytes; a 64-bit
  harness decodes the same films with `BinkDecoder` and compares. The repo keeps only a handful of those CRCs
  (`test/BinkGolden.h`, used by `test_video_bink_decoder`).
- **The format as the game's files use it:** revision `i` only, no alpha, no audio. Each video packet starts with a
  32-bit field the picture does not use, then the planes **Y, V, U** (from revision `h` the second chroma plane comes
  first), each on a 32-bit boundary, bits read from the lowest of each byte. Planes are 4:2:0 and padded to whole 8x8
  blocks (chroma blocks are counted on the luma's 16x16 grid: 640x360 → chroma 320x184). Per plane nine bundles
  (block types, sub-block types, colours, patterns, x and y motion, intra and inter DC, run lengths), each with one of
  16 fixed Huffman trees and its own order of the 16 symbols, read in chunks before each row of blocks; ten block types
  (skip, scaled 16x16, motion, run, residue, intra DCT, fill, inter DCT, pattern, raw).
- **Integer details that make it exact:** pixel stores wrap modulo 256 (an IDCT put or a residue add is never
  clamped); the IDCT multiplies wrap at 32 bits and the row pass rounds `(x + 127) >> 8`; a motion vector is checked
  only against the plane's block area, so a negative x in the first column reads the end of the row above; before the
  first frame there is no reference, and the picture being decoded is its own; a frame that fails keeps the previous
  picture as the reference.
- **The BinkCopyToBuffer colours** (inside the DLL, not the exe; reconstructed from the golden frames):
  chroma **not interpolated** (one U/V per 2x2 block) and four 16.16 tables truncated towards zero separately, added to
  a *floored* luma and then clamped to 0..255:
  `y' = max(0, 76309·(Y−16) >> 16)` (Y < 16 → 0; Y > 235 is not clamped);
  `R = y' + trunc(104597·(V−128)/65536)`; `G = y' + trunc(−25675·(U−128)/65536) + trunc(−53279·(V−128)/65536)`;
  `B = y' + trunc(132202·(U−128)/65536)`. These are the usual limited-range BT.601 constants **except for B's**:
  the classic 132201 fails at U = 70 (RAD gives −117). R, G and B are determined by (Y, U, V) in the 449 685 distinct
  cases of the golden frames and the model gets all of them right.
- **⚠️ approximate** The data pins each constant only to an interval (Y 76305..76309, Rv 104579..104605, Bu
  132202..132221, Gu 25674..25683, Gv 53248..53302); within them the tables only change at extreme chroma values. In the
  five complete videos U goes from 16 to 212 and V from 40 to 219: the only doubtful case that appears is **V = 219** (10
  chroma samples in the whole of pre_intro.bik), where G could be 1 less (`k_GreenFromV` in
  `components/bink/include/BinkYuv.h`).

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The 16-bit copy**

- `src/Graphics/Rgb16.h` (`graphics::rgb16`): `Pack555` (555 branch and BINKSURFACE555), `Pack565` (BINKSURFACE565, `G >> 2`), `PackRaw565` (the 565 branch of the .raw files), `Expand5/6` (replication, inferred), `Quantize/Expand` over spans. `test_rgb16` emulates both branches of fn_00837400 instruction by instruction.

**The decoder and the binkw32 colours**

- **openblack:** CMake option `OPENBLACK_BINK_DECODER`, **on by default**: `VideoPlayer::GameHooks()` makes a `BinkDecoder`, so the game's films play (the intro, the falling spell's film). The debug GUI's **Video** menu has a **Game films** switch (on by default, `ecs::systems::VideoSystemInterface::FilmsEnabled`, read when a film starts): off, the hooks make no decoder and the film is skipped exactly as a build without the option skips it. The same menu plays any of the five films in a window (play, pause, step, restart, the frame and time against the player's pacing). The format's constant tables and the order of its DCT coefficients and its transform follow FFmpeg's decoder (LGPL-2.1-or-later, credited in `BinkTables.h` and `BinkBlocks.cpp`, used under GPL-3).

### Test hooks

- `test_bik_file` (6): synthetic files (format, audio, 13 rejections) and the five real `.bik` files with
  `OPENBLACK_TEST_GAME_PATH` (or `OPENBLACK_TEST_BW_ROOT`); without the folder 2 are skipped.
- `test_video_player` (19): fake hooks and clock, synthetic `.bik` files: pause and widescreen saved and restored,
  file that does not open, pacing, fade and end, intro schedule (1392/1440), skip (48 frames, cap, second
  skip), ESC with Shift/Ctrl/byte 0xD01984, spell, replaced movie, 555 and 565, null decoder, failed
  frames.
- `test_falling_spell_video` (15): `FallingSpellFilmMs`, `ScreenFade` (rises, turns around by itself, goes down, colour to 0,
  exactly on the target, above 1, when it runs), without a creature nothing, KickOff (mode 2, pause, 0x50, fade = end), no
  fade of its own past frame 1080, states and the 11 sounds with their addresses, the white and the end at state 4 (26
  frames of 40 ms, the normal skip of 48), ESC, StopAVISequence, short video, video that does not open, KickOff twice.
- `test_rgb16` (5): the 555 and 565 branches of fn_00837400 emulated, expansion, truncations, spans.
- `test_video_bink_yuv` (2): the colour tables and the non-interpolated chroma (`BinkYuv.h`, in every build).
- With `OPENBLACK_BINK_DECODER`: `test_bink_bitstream` (18), the bit order, every tree's codes, tree headers and each bundle's chunks;
  `test_bink_blocks` (13), coefficients, residues, the transform and the block operations, from hand-written
  bitstreams; `test_video_bink_decoder` (11), synthetic 16x16 films (fill, skip, raw, motion, a damaged frame, seeking) and,
  with `OPENBLACK_GAME_PATH`, the CRC-32 of the 555 of INTRO, fall and pre_intro frames in order and of logo and tips in
  any order, against the DLL's (only the CRCs are in `BinkGolden.h`); `test_video_playback` (7), the Video window's
  pacing with fake times.
- Game: `OPENBLACK_TEST_VIDEO=<intro|fall|path>` (Game.cpp, when loading the map): `intro` = `Data\intro.bik` +
  `ScheduleIntro()` (60 s), `fall` = `video::GetFallingSpell().Start()` (KickOff without the creature check:
  mode 2, alpha 0x50 without world, states, white and end), otherwise the given path. At frame 1800 of `intro` the
  video covers the screen; at frame 1500 of `fall` the world comes out × 0.686.

## Pending

- The tips screen's bar body as the original's bevelled box (`DrawBevBox` 0x5F4637), the version from the registry and
  the cursor it sets (`SetCursor` 0x5F3F30): not read yet.
- The pre-intro of a first run: openblack keeps no player profiles yet.
- `OPENBLACK_VIDEO_TRACE` does not exist yet (tooling).
- The music's alignment is forgotten for the video at 0x54D963 (`GAudio+0x1C = −1`); `GameMusic::_alignmentType` has
  no such reset. Queued as a fix.
- With `OPENBLACK_TEST_VIDEO=fall`, the white at 43.9 s, the end at ~44.9 s and the opaque 48-frame fade are still to
  be seen in a game run. The first video of a new profile is read as not skippable; openblack has no profiles, so
  nothing sets `SetNoSkip`.
- `SET_AVI_SEQUENCE` pauses the game for both films (checked: `VideoPlayer::Play` calls `game_clock::Pause`
  and gives the pause back at the film's end, as `PlayFullScreenMovie`). In mode 2 the original still runs
  `LH3DAtmos::Update3D` every frame and skips the weather's `Render2D` (0x54E3D2 / 0x54E4BC); not ported.
