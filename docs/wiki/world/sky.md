# Sky

## Summary

The original's sky is a dome blended from nine `.555` images (three alignments by day, dusk and night) following the
continuous sky type of the visual time (2 night, 1 dusk, 0 day), rebuilt 32 rows a frame when the type moves by more
than 0.03, drawn as two alignment layers tinted by the overcast and lightning. The sun (6 to 18 h, script time) and
the moon (its phase from the real clock, with an additive halo) are meshes in the sky; the sun's glare at the end of
the frame is hidden by the land and by what is drawn in front of its samples. Seventy clouds of `mist.l3d` drift with
the wind, coloured by the light table and the alignment, and stamp their shadows on the land. There are no stars.

openblack follows each of these with the original's numbers; the glare's depth test answers a frame or two late
(bgfx has no depth read back within the frame).

## The original

### Sky type

The original's convention: **2 night, 1 dusk, 0 day**, continuous. All ✅ faithful except what is marked. The
table names openblack's function (`openblack::sky_type::`) next to each original routine.

| Function | Original | What it does |
|---|---|---|
| `At(hour)` | `Time2SkyType` 0x86A1B0 | Folds around 12 only if hour > 12 (`test ah,0x41`); then strict `<` against A..D: 2, 2 − (h − A)/(B − A), 1, 1 − (h − C)/(D − C), 0. There is no division by zero: with A = B the ramp is unreachable. `DayNightClock::SkyType(hour)` (was `Time2SkyType`) forwards here with its thresholds |
| `SetThresholds(A,B,C,D)` | `SetDayNightTimes` 0x869FA0 | A → 0xFA26A0, B → 0xFA269C, C → 0xFA2698, D → 0xFA2694; called by `DayNightClock::SetCycle` (0x557620) |
| `SampleFrame(visual)`, `Frame()`, `FrameHour()` | `fn_0086A2C0`, [0xFA26BC], [0xFA26C4] | Normalises to [0, 24) with the loops at 0x86A2C4..0x86A308 and stores hour and type. `DrawSky` 0x5E2226 calls it once per frame with the visual time [0xBF3380]: in openblack, at the start of `Renderer::DrawScene` (once per frame, not on every pass) |
| `Jump(visual)` | `fn_0086A270` | `SampleFrame` and the whole dome at once. Called by `fn_005E22A0` 0x5E22CB (`ForceVisualTime` 0x5575D0): in openblack, `DayNightClock::SetScriptTime` (Reset/LoadMap, `SET_GAME_TIME`, the slider, `OPENBLACK_TIME_OF_DAY`, `config.timeOfDay`). `MOVE_GAME_TIME` does not jump |
| `IsVisualNight(T)` | 0x5575E0 | T > the double 1.2 ([0x8D8758] = 33 33 33 33 33 33 F3 3F): T = 1.2f is already night. Used by `DayNightClock::IsVisualNight`; `ChildAtCreche` 0x757CFF has the same comparison |
| `EveningRamp(visual, w, o)` | `fn_00557AE0` | u = 24 − visual, s = D + o; u < s → 1; !((D + w) + o > u) → 0; otherwise, 1 − (u − s)/w. For the Relaxation 0x7488C0 / Sleep 0x748960 desires (no caller yet, **pending**) |
| `LightColumn(T)` | 0x86985E..0x8698AD | (2 − T)·6.0f·2.5f = (2 − T)·15, column of the light table |
| `HazeFactor(T)` | 0x869D5F..0x869D7A | v = T > 1 ? 2 − T : T; v² |
| `DomeWeightOf`, `BlendTexel555`, `DomeBlend` | `fn_0086B7F0`, `fn_0086B9A0`, `fn_0086A330` | The dome, below |

Who calls what:

- They compute the type on the spot with the visual time, like the original: the model lighting
  (`model_light::UpdateFrameLight(…, sky_type::At(visual))`, like `fn_005E5830` 0x5E58D1..0x5E58DF; it gives the same as
  `Frame()` because `DrawSky` samples the same time right afterwards), `DayNightClock::ProcessTurn` (0x5E202B), the
  fireflies (`SkyType`, 0x52B7CA / 0x52B820) and `IsVisualNight` (night lights).
- They read the frame sample: the dome and `LandLightTable::Build(sky_type::Frame(), …)` (column
  `LightColumn`, haze `HazeFactor`; see [the frame and its draw stages](../rendering/frame.md)). Sound (`GSoundMap`, 0x71DDF1) must read
  `Frame()`, the T of the previous frame (ProcessTurn 0x54D830 and EndTurn 0x54D837 run back to back, without `DrawSky`):
  `audio::ProcessTurn` reads it this way (done).
- Constants: 12 [0x8CF1C0], 24 [0x8CA26C], 2.5 [0x8C581C], 255 [0x8AB270] (0x86B8A6 / 0x86B8C3); the ramp's fdivp is
  (h − A)/(B − A) (0x86A206..0x86A21A). The set-up thresholds 4.5 / 7 / 7.5 / 8.25 are pushed as 0x40900000 /
  0x40E00000 / 0x40F00000 / 0x41040000 (0x86A3B7..0x86A3E0).
- Precision: the FPU runs at 24 bits (`fn_007DEE00`, `and cw, 0xFCFF` at 0x7DEE0D), the same hypothesis as `SetCycle`. With
  it the `Time2SkyType` ramps already come out rounded to float, the `SampleFrame` loops run in float (h = −1e-7
  gives 24 → 0; with 53/64 bits it would stay at 23.9999999 and 24.0f would be stored) and the hysteresis subtraction is rounded to
  float before comparing it with the double 0.03f.
- NaN: `Time2SkyType` compares with `fcomp` / `test ah,1` / `je` (0x86A1DC..0x86A246), so «unordered» counts as «<»: a
  NaN time gives 2 (night). In `SampleFrame` the original gets stuck in an infinite loop with NaN.

**The dome** (`DomeBlend`, `SkySystem::UpdateDome`, `fs_sky.sc`):

- `fn_0086A3B0` creates 3 dynamic 256×256 textures (flags 0x104, format 4; [0xFA2738 + 4a]), one per alignment,
  and builds them whole. Sources: `Data\WeatherSystem\sky_<good|ntrl|evil>_<day|dusk|night>.555`, index
  3·time_of_day + alignment ([0xFA26E8]). It builds the whole dome at 0x86A595, and the dome's [0xFA26C0] is 0 again
  at 0x86A5AB.
- Each frame, `fn_0086A330` (after `SampleFrame`, 0x5E222B): if the dome is complete and |`Frame()` − built| > 0.03 (the
  double [0x99A168] = (double)0.03f, strict), it stores the new T ([0xFA26C0]) and goes back to row 0 ([0xFA26B8]);
  while rows are missing it blends 32 more with the stored T (the first block in the same frame). There are [0xEDD470] ?
  128 : 256 rows. [0xEDD470] depends on the detail level: `fn_00823AD0` copies it from the table [0x9A38E0 + 4·level] =
  1, 1, 0, 0, 0, 0, 0 (0x823C5B..0x823C69; `DetailLevel::skyNoBlend`), and `fn_0082A8E0` writes it again on its two
  exits (0x82AB1B / 0x82AB30). At levels 2..6 there are 256 rows in 8 frames, with blending. At levels 0 and 1
  `fn_00869670` is false: no blending, 128 rows and tint by T (below, not ported).
- `fn_0086B7F0`: T ≤ 1 → w = ftol(T·255) between day (255 − w) and dusk (w); otherwise, w = ftol((T − 1)·255) between dusk
  and night. `fn_0086B9A0` (555 path): per 5-bit channel `(c_inf·(255 − w)) >> 8 + (c_sup·w) >> 8`; the sum of
  weights is 255/256 (a channel of 31 comes out as 30; the callee masks 255 − w with 0xFF, 0x86B9F6..0x86BA03) and bit
  15 comes out as 0. Since each term is truncated separately, the dome
  comes out somewhat darker and with more banding than the previous linear blend: about −4/−5 per channel (out of 255) at noon and
  up to 40 % less in the dark channels at night (the night textures have values 1..4 out of 31). This is what
  the original does (tables 0xFA2554 / 0xFA2514, p = 255 − ftol(T·255) at 0x86B8E3): **do not «fix» it**.
- The texture (format 4) only receives the +0x138 mark when the block reaches the last row (0x86B95B..0x86B977); the
  unlock `fn_00838EB0` only uploads formats 1, 2 and 0x20 itself. The +0x138 mark means «dirty, upload on the next
  bind»: the `SetTexture` path of the LH3D texture (0x837EC6..0x837F74) checks it, locks the D3D surface (vtable +0x64,
  flags 0x821), converts the system copy with [+0x134] (0x837F19), unlocks, calls `IDirect3DDevice7::SetTexture` (+0x8C)
  and clears the mark (0x837F74); that is, in the `DrawSky` of the same frame. **The dome's draw** (`fn_0086B010`, the
  `jmp` at the end of `GLandAlignement::DrawSky` 0x5E2233; `3D/SkyDome`, `Renderer::UpdateSkyTint` / `SetSkyUniforms`,
  `fs_sky.sc`).

- `DrawSky` copies the overcast at the camera [0xD1A26C] to [0xFA2754] (0x5E2215) and the sky's alignment [0xBF3378]
  (0 good, 1 neutral, 2 evil) to [0xFA2758] (0x5E221B), and stores 1 − 0.5·[0xBF3378] in [0xEDD388] (0x5E21F1..0x5E2220:
  1 good, 0.5 neutral, 0 evil). The light table's build (`fn_00869850`, from `fn_0086A330` before the draw) clamps an
  overcast above 0 to 1 and writes it back (0x869DB1..0x869DDB), so the draw reads it ≤ 1; one ≤ 0 stays as it is.
- **The alignment pair** (0x86B0F6..0x86B3B5): X = [0xFA2758]; X < 0 → 0 and X > 2 → 2 are written back; X > 1: dome 1
  (neutral) and dome 2 (evil) with w = ftol((X − 1)·255); else dome 0 (good) and dome 1 with w = ftol(X·255). The dome
  numbers are the textures of `fn_0086A3B0`, 0 `good`, 1 `ntrl`, 2 `evil` (0x86A446..0x86A457, materials [0xFA2744]).
  `fn_0086B6B0(dome, alpha, colour, specular)` is called for the first dome with alpha 255 when 255 − w > 0 and then for
  the second with alpha w when w > 0; with 255 − w = 0 only the second, with alpha 255 (0x86B5B2..0x86B5EA).
- **The colours.** The colour starts white (ebp = −1, the blend path; the no-blend path tints it by T, not ported) and
  the specular black. With the Fog key [0xC37204] (0x86B2E0), haze = the light table's haze floats [0xFA26B4] (R),
  [0xFA26B0] (G), [0xFA26AC] (B) cut with `ftol` (0x86B2AD..0x86B2DE), o = ftol([0xFA2754]·255) (0x86B19A): colour
  c = 255 + floor((haze − 255)·o / 256), specular s = floor(haze·o / 256), each an 8-bit channel of a packed D3DCOLOR
  (0x86B2EC..0x86B3DF). With an overcast below 0 the products leave their bytes and only the low byte of each channel is
  kept (checked by emulating the 32-bit register arithmetic on 300 000 random inputs).
  Then, with bit 0 of [0xEDC360] (the "Weather" key, set once by `start_system` 0x6433C2), k = d² / 150 (the multiply by
  0x1B4E81B5, 0x86B3EE..0x86B406) with d = [0xEDC374], and each channel x of both x − ceil(x·k / 256)
  (0x86B408..0x86B4B8). Then, with a flash f = [0xFA2768] ≠ 0, c + floor((255 − c)·f / 256) and s + floor((255 − s)·(f /
  2) / 256) (0x86B4BA..0x86B5B0). `sky_dome::TintOf`; the shader draws each dome as clamp(texel·c + s)
  **(inferred: s taken as the specular D3D adds)**.
- **The darkness** d ([0xEDC374]) is set by `LH3DAtmos::Update3D` 0x8357A0, which runs before `DrawSky` in the frame, so
  from the [0xEDD388] of the frame before: 0, or ftol([0xEDD384]·255) when [0xEDD384] > 0 (0x8359F9..0x835A27), then the
  larger of that and ftol(([0xEDD388] − 0.6)·225) when [0xEDD388] > 0.6 (0x835A2D..0x835A5D; both constants floats, each
  step rounded to a float), held to 0..90 (0x835A63..0x835A78). [0xEDD384] only goes down by the time step
  (0x8359D2..0x8359EF) and starts at 0 (`fn_00835AD0` 0x835BD6, with [0xEDD388] = 0.5); nothing else writes it, so it
  stays 0. **So the dome is darkened on the good side**: from X < 0.8, up to d = 89 at X = 0 (k = 52, the colour 255 →
  203). `sky_dome::Darkness(X)` takes the light table's alignment of the frame, which is the sky's of the frame before.
  raffclar's `sky_dome::Darkness` was fed the alignment the other way round (evil darkened) and multiplied in doubles
  (`Darkness(0.38222218)` 46 instead of 47).
- **The sun and the moon through an overcast.** The moon's alpha m = min(200, ftol(0.5·y − 110)) becomes ftol(m /
  (8·[0xFA2754] + 1)) with the Fog key and an overcast > 0 (0x86B05B..0x86B094); the moon, its glow and its reflection
  are drawn only when m > 0 (0x86B09C..0x86B0A4, 0x86B61D..0x86B623). The sun the same with ftol(A) (`fn_0086C140`
  0x86C19D..0x86C1E7), and so is its glare, from ftol(glare·A·0.00392157) (`fn_0086BB60` 0x86BBF9..0x86BC4F).
- The initial dome is built with the current T (the original uses the time from `fn_0086A3B0` with its thresholds
  4.5 / 7 / 7.5 / 8.25); the last time jump of `Open` (0x5E1D9C; the first is `fn_005576F0` 0x557702) and the
  `SetScriptTime(12)` of `Reset` in openblack rebuild it whole immediately (the last jump wins), **🔍 inferred** with no
  visible effect. `fn_005E22A0` also calls `fn_005E1DE0` (0x5E22D3, reads [0xBF3378]) after the jump: it is not sky
  type and it is not in `SetScriptTime`.
- Not ported: the no-blend mode (`fn_00869670` false: copy of the day textures and tint of the sky colour by T,
  0x86B1C1..0x86B2A4), the 565 path ([0xEDD46C]) and the light table after `Jump`.
- Order in `GGame::Load`: the jump (0x554B6F) comes before `SetVisualTimeCycle` (0x554C9E), with the previous
  thresholds.

Tests: `test_sky_type` (default thresholds, `At` with Demon God and with NaN, normalisation (also
−1e-7 → 0 at 24 bits), 1.2 double, evening ramp, column and haze, weights, 555 blend, hysteresis and rows (also the
subtraction rounded to float), jump from `SetScriptTime`).
- The dome blends only when `fn_00869670` is true ([0xC38200] = 1 and [0xEDD470] = 0, detail levels 2..6); the source time-of-day index runs 0 _day, 1 _dusk, 2 _night (0xC3963C).

### The sun and its glare

The drawing is in [sky: sun, moon and clouds (original)](#sky-sun-moon-and-clouds-original); this is the alpha and the gates.

- **Sun** (`fn_0086C140`, from `fn_0086B010` 0x86B618, in the sky stage): nothing inside the citadel
  ([0xFA2760], 0x86C141). `fn_0086C020` sets the height and the alpha A [0xC395A8] from the script time t: 0 before
  3 h, (t − 3)·85 up to 6 h, 255 to 18 h, 255 − (t − 18)·85 to 21 h, 0 after (0x86C08C..0x86C12A). Nothing is drawn
  when A = 0 (0x86C159). Drawn alpha = `ftol(A)`, then, when the overcast at the camera w [0xFA2754] > 0 and the Fog
  key [0xC37204] is on, `ftol(a / (8w + 1))` (0x86C19D..0x86C1E7); colour (a << 24) | 0x957C63. Then the "sun drawn"
  flag [0xFA275C] = 1 (0x86C20D).
- **Glare** (`fn_0086BB60`, only from `LH3DRender::FinishFrame` 0x82F4C9: after the Z-sorter flush and the "before"
  callbacks, before the Z-reset quad and the 2D). Runs only if A ≠ 0, the flag is set and not in the citadel
  (0x86BB61..0x86BB8C); clears the flag (0x86BB92).
  - Target (`fn_0086BD00`): (1 − 0.2·h)·255, h = how many of 5 samples are hidden. Samples = sun + offset + d·near,
    d the unit camera → sun direction and near [0xE839E0]; offsets in world (x, y): (0, 0), (−500, −500),
    (−500, 500), (500, −500) and (500, 500), four distinct corners (stack 0x24..0x5C written 0x86BD06..0x86BD8D: the
    `fstp [4C]` stores the −500 the three `fst`s left on the stack, so [4C] is −500; an earlier reading took it for
    +500 and wrongly listed (500, 500) twice); y at least 10 (0x86BEF4). Hidden when
    `LH3DIsland::RayCast` from the camera hits (0x86BF1C), or, with the Z-buffer test ([0xC381EC] ∈ {3, 4, 6} and
    [0xC386D8]), when the sample is on screen (fn_008190D0 = 2) and the depth there is below 0xFDE8
    (0x86BF3F..0x86BFB4).
    - [0xC381EC] is the **detail level** ("detailidx"): its only writer is `fn_00823AD0` (0x823B32), called only from
      `start_system` (0x643026) with the registry's "detailidx" when below 5, else 4 (no value gives 4)
      (0x642FCB..0x64301F); the options dialog only reads it as its slider's default (0x51481A). So the test runs at
      detail 3 and 4, the default 4 included; 6 never happens there.
    - [0xC386D8] = the chosen Z buffer is 16 bits and the card is not a PowerVR KYRO ([0xECA5F0], from
      `CheckDescForPowerVRKYRO` in `LHScreen::DDrawInitialiseDevices`), set in `LH3DRender::OpenD3D`
      0x82CED8..0x82CEED ("ZBUFFER 16 bits"; `LH3DRender::Open` 0x82B542 logs "using z buffer for sun before check").
      Which Z format a machine gets is the first its driver lists that the device accepts (16 bits on most cards of
      the time).
    - The Z surface [0xECA63C] is locked once before the loop (vt+0x64, flags 0x811 = WAIT | READONLY | NOSYSLOCK)
      and unlocked after it (vt+0x80). `fn_008190D0` projects the sample: 0 when its depth is below the near plane
      (0x8190F9), 1 off screen unless 0 ≤ x < W and 0 ≤ y < H with x = ftol((X/z + 1)·W/2), y = ftol((1 − Y/z)·H/2)
      (cut towards 0), else 2. The 16-bit depth at that pixel, `surface + 2·((pitch·y)/2 + x)`, hides it below
      65000. The software transform writes 1 − near/z with no far plane ([Global state](../rendering/frame.md#global-state)), so that is
      anything drawn there nearer than about near / (1 − 65000/65535) ≈ 122.5 × near (how the driver rounds 1 −
      near/z to 16 bits is not known: about 0.1 % of that depth). What is in the Z buffer then: everything drawn
      before the glare in FinishFrame (the land, the objects, the Z-sorter's flush), before the Z-reset quad.
  - Ease: G [0xFA2778] += (target − G)·(`g_game_time_inc` × 0.01f), clamped to 0..255 (0x86BBA1..0x86BBF9).
  - Alpha: a = `ftol(G · A · (1/255))` (0x3B808081, left to right, 0x86BBF9..0x86BC0B), then the same overcast rule
    as the sun: `ftol(a / (8w + 1))` when w > 0 and Fog is on (0x86BC10..0x86BC4F). Both truncations go to int; no
    other clamp (255·255/255 gives 255).
  - Drawn as the sun mesh ×1.8, colour (a << 24) | 0xA06A35, ZFUNC ALWAYS and back to LESSEQUAL
    (0x86BC59..0x86BCEB).
### Sky: sun, moon and clouds (original)

- The time the sun and the moon use is the **script time** (fixed thresholds 3.5 / 7.5 / 8 / 8.5 h); the game's real
  clock is the visual time with the cycle's thresholds. See [day and night](day-and-night.md).
- The **dome** (the `sky_*.555` textures) follows the sky type of the visual time, with a hysteresis of 0.03 and 32
  rows per frame (`fn_0086A330` / `fn_0086B7F0`; `sky_type::DomeBlend`, `SkySystem::UpdateDome`). It is drawn (`fn_0086B010`)
  as two of the alignments' domes, the second over the first by an integer weight of 255, both in the frame's colours:
  towards the haze colour by the overcast (the Fog key), darker on the good side of the sky (the Weather key) and
  towards white in a flash (`3D/SkyDome`, `fs_sky.sc`). All the detail, in
  [sky type](#sky-type).
- **Sun** (`fn_0086C020` / `fn_0086C140`): `sun.l3d` (vertical quad 9928), `sun.raw`, mode 13 (additive SRCALPHA/ONE,
  no Z), at (−30000, y, −30000) rotated 3π/4, y = 7500·(clamp(min(T, 24−T), 6, 12) − 6)/6, colour 0x957C63, alpha
  0 → 255 between 3 and 6 h and 255 → 0 between 18 and 21 h (÷(1 + 8·clouds), the overcast held to 1, with the Fog
  key; `sky_dome::ThroughOvercast`, the moon's alpha too). **Glare** (`fn_0086BB60`, at the end of
  the frame): the same mesh ×1.8, colour 0xA06A35, without Z test, visibility from 5 samples occluded by the
  terrain or, at detail 3 and 4, by what the Z buffer holds near the camera, smoothed 1 %/ms (samples, alpha and gates in
  [the sun and its glare](#the-sun-and-its-glare)).
  - The alpha A is [0xC395A8]. The glare [0xFA2778] moves by (target − glare)·(`g_game_time_inc` × 0.01 [0x8C4B10]),
    clamped to 0..255 (0x86BBA1..0x86BBF9): the factor is not capped (`g_game_time_inc` ≤ 199) and the glare stays put
    while paused.
- **Moon** (`LH3DAtmos::UpdateGame` 0x8356E0): camera + (4000, 1100·cos θ − 150, 800·sin θ), θ = T·π/12; alpha m =
  min(200, 0.5·y − 110); colour = row 5 of `palette.raw` by alignment. Halo (`fn_0086A930`): 4000 quad on the basis of
  `fn_0086AC60` (+Z from the eye to the moon, X horizontal in the view), `atmos.raw` UV 0.25–0.49375 (v0 at the bottom
  left = 0.25), additive, colour (R/6, G/5, B/4, m). Mesh: `moon.l3d` (loaded without skins, texture `weather.raw` by
  code), that basis ×4, `fn_0086AFA0`(−0.1309) = Rz(+7.5°) in glm, RotateY(phase + π) = Ry(−(phase + π)) in glm, ×0.65
  (details in [objects that face the camera
  (billboards)](../rendering/meshes-and-lod.md#objects-that-face-the-camera-billboards)); the phase comes from the
  **real clock** (2π(1 − frac((days − 10962)/29.5306))) and regenerates the UVs. Normal culling (bit 0 = 0). **Z** (V):
  the moon writes Z in the sky and in the sea. `fn_0086A930` sets the object's alternate-table bit 0x80 (vt+0x48(1)), so
  both its draw (`fn_0080DB30`) and its `DrawUnderWater` (`fn_00811010`) take table 0xC387C8, where modes 4 and 5 go to
  0x82DD90; the primitive draws call it with edx = 0 (0x857363, 0x85BE72, 0x851772), and only a non-zero edx (creatures,
  override materials; the moon's override is −1 from 0x816530) would skip its states. A hit on the mode cache
  ([0xC38718] = 5, [0xC386F4] = 4) still reaches ZWRITEENABLE = 1 at 0x82DEE5; nothing sets a state after it. The depth
  test is LESSEQUAL (4), set before the sun and the moon (0x86B5ED..0x86B613) after the sky dome's ALWAYS (0x86B173).
  **Reflection** (`fn_0086B010` 0x86B61D; done): the first call draws halo + mesh + the mesh's `DrawUnderWater`
  (vt+0x118: the moon mirrored at y = 0, unlit); the second, with `pos.y = −pos.y` and `[0xFA2774]` = 1, only the halo.
  Same m and same colour, in the sky stage (the reflected land covers it). The tilt k = −1 of the second call is never
  seen: that call does not draw the mesh (`fn_0086A930` 0x86AC0F). The sun is **not** reflected (`fn_0086C140` is called
  only once).
  - **The order under the sea.** In one frame: the halo (`Draw3DWorldTriangle` 0x86AB92), the moon (vt+0x108 0x86AC19)
    and its `DrawUnderWater` (vt+0x118 0x86AC46) in the first call (0x86B649 / 0x86B65D), then the halo copy at (x, −y,
    z) in the second (0x86B67E..0x86B69C, 0x86B685, [0xFA2774] = 1; 0x86AC0F skips the moon). So in the sky the halo is
    under the moon, and under the sea the mirrored moon is under its halo. The halo is additive and the moon SRCALPHA /
    INVSRCALPHA, so where they overlap the order shows.
  - The tilt [0x9A3BF0] is applied with k = +1 at 0x86AEF3..0x86AF0B; `fn_0086AFA0` is 0x86AFAC..0x86B008 (r0' = c·r0
    − s·r1, r1' = c·r1 + s·r0).
  - The basis (0x86AC67..0x86AE4F) leaves an exactly null vector unnormalised (0x86AD26..0x86AD58,
    0x86ADAF..0x86ADD7).
  - Halo: corners at 0x86AA3C..0x86AB67 (half width 2000, the rows already × 4); UV constants 0.25 = 0x3E800000
    (0x86A981) and 0.49375 = 0x3EFCCCCD (0x86A996); the triangles 0xEDC310 are set at 0x86A933..0x86A974.
  - **The phase as last shown, and `GET_MOON_PERCENTAGE`.** The phase float [0xFA2724] is written only in `fn_0086AC60`
    (0x86AEDD), which runs only while the moon shows (`fn_0086B010` 0x86B61D: m > 0 after the overcast). Nothing else
    writes it, so it is 0 from start-up until the moon first shows, and it keeps its last value while the moon is down.
    `GET_MOON_PERCENTAGE` (native 108, `GScript::GetMoonPercentage` 0x711230, nothing in, one float out) pushes `phase <
    π ? 1 − phase·(1/π) : (phase − π)·(1/π)` (0x711231..0x711275; π 0x40490FDB at 0x8C36A0, 1/π 0x3EA2F983 at 0x8D0620,
    1.0 at 0x8AA390): 0 at the full moon, 1 at a new moon, and 1 before the moon first shows. No shipped script calls
    it: native 108 is called in none of the ten `.chl` files of the game, against 4 900 to 207 000 native calls in each
    (a scan for its call pattern, which does find natives 106 and 107; a pattern match, not a full parse). the native
    pushes `moon::ScriptPercentage` of it (`test_moon_percentage`).
  - **The date read.** The phase's `fn_0086A7F0` calls `time()` again only once `GetTickCount` has moved more than 2000
    ms since its last read, and keeps the date between reads (0x86A7F0..0x86A845). So the phase changes at most 2 s
    after the date's day changes.
- **Night-time hand glow on the water** (done; `3D/HandWaterGlow`, `Renderer::DrawHandWaterGlow`): `GLandscape::Draw`
  0x5E4D89, if k = `[0xD20184]` > 0.01 (k = clamp((120 − mean) × 1/15, 0, 1), the mean of the base colour's three bytes
  rounded down to an integer, the same as the hand's light on land, see [Night lights](day-and-night.md#night-lights)):
  colour = row 6 of `palette.raw` (`[0xFA26E0]`) moved a quarter of the way towards (255, 128, 64) per channel (integer:
  floor((3c + target)/4)), alpha clamp(ftol(k·190), 0, 190). `fn_005E3F70`: only if some cell of [(x − 70)/10, (x +
  70)/10] × [(z − 70)/10, (z + 70)/10] (lower limits clipped to 0..511, the upper ones not) has altitude < 5 or has no
  cell; horizontal quad at y = 0 of (x ± 60, z ± 60), UV (0.75; 0.375)–(0.796875; 0.421875) of `atmos.raw` (u with x, v
  with z), `LH3DAtmos::AdditiveMaterial` (mode 13, SRCALPHA/ONE; with the texture `data\textures\atmos.raw`
  (`fn_00835AD0` 0x835C20..0x835C4E creates `AtmosMaterial` mode 6 and `AdditiveMaterial` mode 13 with it)), ZFUNC
  ALWAYS, right before the sea. with the hand over high land nothing appears.
  - k > 0.01 is tested at 0x5E4D8F; the colour is made at 0x5E4DA0..0x5E4E57; the maximum alpha 190 is 0x8DAD54; a
    cell altitude byte below 5 counts as low (0x5E410A); the cell is ftol then a signed division by 10 (0x66666667,
    `sar 2`: both truncate towards 0).
  - The quad's corners go (x − 60, z − 60), (x + 60, z − 60), (x + 60, z + 60), (x − 60, z + 60) with the UVs of
    0x92B2F8 in that order (a 12×12 texel spot of the 256×256 atmos.raw) and the indices 0x92B2E0 = {0, 1, 2, 2, 3,
    0}; ZFUNC ALWAYS at 0x5E4281, no culling.
- **Clouds** (`CloudInSky::Open` 0x5E23F0, `fn_005E25C0`): 70 + 2 fixed ones, x ∈ ±8000 (wind at 70 u/s, angle 3π/4,
  around (1280, 1280)), y 300–500, z ±5000, size 13–50, k 2.5–5; edge alpha beyond ±6000.
  `mist.l3d` without skins: smoke material `smoke.raw` + `smokea.raw` (mode 6, two-sided, `fn_0080BBD0`). They are
  LH3DMist objects (+0x80 |= 2, effect branch), so they use the same `fn_007FA300`: the **billboard**
  mat3(right, −forward, up) from the "Map mist" section and the **non-uniform scale** s = size/(1 + (k − 1)
  (1 − |dy|/|d|)) with the size in row 0 (width on screen) and s in rows 1 and 2 (depth and height). Near the
  horizon |dy|/|d| ≈ 0.05–0.2, so each cloud is a **horizontal ellipse ~k times wider than it is tall** (2.5–5; the
  two fixed ones, size 300 and k 20, almost flat bands); they only look round right underneath. Animated 8×8 atlas
  (frame (counter/20) & 15, UV ((f&7)/8, (f>>3)/8 + 0.25)), zenith light with ambient 210/256; alignment colour ×
  table[255] · 186/256 + 35;
  alpha 0 on good land, 200 neutral, 255 evil.
  - The cloud count 70 is [0xBF33A8]; the wind angle is the constant [0x92B2A0] = 2.35619455575943 (3π/4).
  - The mist / cloud sphere radius for the screen test is the mesh's +0x30 (0x7FA7FE) × size (+0x88) × 0.55
    ([0x8D3E80], 0x7FA814).
  - **Placement** (`CloudInSky::Open` 0x5E2439..0x5E24F4): **six** CRT `Random` calls per cloud, interleaved: x
    (0x5E2443), y (0x5E245A), z (0x5E2473), then `LH3DObject::Create(7)` (0x5E2487) → `LH3DMist::LH3DMist` 0x7F9560,
    whose `Random(0, 16)` (0x7F95EE, `ftol & 15`) starts the mist counter +0x84, then size (0x5E24AA) and k
    (0x5E24CB). 70 clouds: 420 CRT draws per land open. Clouds 0 and 1 are drawn too (their values are overwritten
    later). Each cloud is on its own and uniform in the box: **the sky clouds are not in groups**. The groups that are seen come from chance (≈1500 u on average
    between 68 clouds, with streaks and gaps) and from perspective; the original's real groups are the storm clouds
    (`GWeather::DrawClouds` 0x83FC90: up to 16 balls per storm around its centre, darkened and with haze; ported,
    see [the flash and the clouds of the registered storms](../magic/storm-and-tornado.md#the-flash-and-the-clouds-of-the-registered-storms)).
  - `Random` 0x81D180 = min + (max − min)·(rand()·3.0518509e−05f) with the MSVC CRT's `rand()` (0x7C8837, s = s·214013 +
    2531011, (s >> 16) & 0x7FFF), not the synchronised GRand. **Correction (game_random)**: the `srand(time(NULL))` at
    0x577721 is in fn_005776E0, which is only reached when saving `creature.lhp`: `rand()` starts at seed 1 (`__initptd`
    0x7D2323) and is per thread; what changes the sky between sessions and lands is the `rand()` calls spent before
    **🔍 inferred**. `GLandscape::Open` → `GLandAlignement::Open` 0x5E1D10 → `CloudInSky::Open` rebuilds the 70 clouds on
    every land load. **When, in the load**: `GLandscape::Open` runs inline at the land script's `LOAD_LANDSCAPE` line
    (the feature script's command 0x716FB9..0x716FE7 and the map script's, `GSetup::MapCommandProcess` case 16
    0x714DDF..0x714E13, both copy the name to `GLandscape::Filename` first). In every land and playground script that is
    the third line, after `VERSION` and `SET_LAND_NUMBER`, so the sky's 420 draws come before every `CREATE_` line: the
    street lanterns' creation draws and the chimneys' come after them. In `GLandscape::Open` the call is at 0x5E5510,
    after the island, `sky.raw` and the sea material, before `ValidateLandAvoid` (0x5E5541); nothing else under
    `GLandscape::Open` draws from the CRT or GRand (its direct calls, 5 deep), and the rest of `GLandAlignement::Open`
    (0x5E1D15..0x5E1DAB) draws nothing. a drawn frame makes no layout draws. raffclar's tree has the mist draw, but
    `MakeLayout` draws all 70 × 5 layout numbers first and each `CloudArchetype::Create` then draws its `Random(0, 16)`:
    the same 420 draws, in a different order, so every cloud after the first gets different numbers.
  - Step (`fn_005E25C0`): x += inc·70·0.001; past 8000, t = x + 8000, x = t − ftol(t/16000)·16000 − 8000 (only x:
    each cloud comes back along the same line at the same height); edge = fistp((x ± 8000)·0.1275) (rounding), fixed
    ones 192; alpha = edge·A/255 in integers.
  - **Colour** (`fn_005E1DE0`, every frame from `GLandAlignement::DrawSky`): i = trunc(X), f = trunc((X − i)·256), each byte a + floor((b − a)·f/256)
    between 00FFFFFF / C8FFFFFF / FFAAA066 (0xBF339C), RGB·table[255] (c·t >> 8), then c + floor((8960 − 70c)/256) (255 →
    **220**: the original never paints them white). Per-vertex light (`fn_0084BA90`): I = fistp(255·N·L),
    f = 210 + (45·I >> 8) (210..**254**), diffuse (c·f) >> 8. The time of day only comes in through the table (rows 0-2
    of `palette.raw`) and the weather only through its cloudiness cap. Neutral noon ≈ (172..208, 179..217, 177..214),
    light grey that with α ≈ 0.7 over the blue looks whitish; at sunset salmon; evil: opaque ochre; good: none.
    **Land1 starts at 7.3 h script time** (almost full sunset) with the clock stopped: hence the brownish clouds; with
    `OPENBLACK_TIME_OF_DAY=12` they come out light grey as in the original.
    - The result goes to [0xBF3398]. The light product is per channel (R `imul` 0x5E1EE9, G 0x5E1F02, B 0x5E1F1E, each
      `sar 8`); the alpha byte (+0x1B, written at 0x5E1ECA) is the lerped one, untouched; the last step is c + (((35
      << 8) − 70c) >> 8) (0x5E1F28..0x5E1FB4).
  - **Sky alignment** [0xBF3378] (0 good, 1 neutral, 2 evil; it starts at 1 and loading a land does not touch it):
    target [0xBF337C] = (1 − clamp((v + 1)/2, 0, 1))·2 (`fn_005E2240`), with v = `GetAlignmentValue` of the player with
    the most influence at the interface's position (`fn_0064AC30` from `GPlayer::ProcessPlayers` 0x64A697, every turn;
    `DoCitadelMultiplayer` forces 0.5). `DrawSky` 0x5E2160 moves it 0.001 per ms (inc·0.01·0.1) and snaps it when it
    overshoots; it is used by the clouds, the light table and the sky.
  - **Animation**: each cloud is an LH3DMist with its own counter +0x84; `LH3DMist::AddDrawing` 0x7FA7F0 (vt+0x100)
    only sends it to the Z-sorter if its sphere (half diagonal of the mesh × size × 0.55) touches the screen, and only
    then does the counter advance in the Draw: the clouds drift out of phase with each other. **There is no blending
    between frames**: `fn_007FA300` computes frame = (counter·45)/900 in integers (0x7FA3F4..0x7FA41B, no fraction),
    sets a single UV offset (vt+0xE8 = 0x7F9B70: +0x68/+0x6C) and draws once (`fn_0080DB30`); mode 6 (`fn_0082DF10`)
    only configures stage 0 (MODULATE texture × diffuse). The change is abrupt every 20 counts (≈78 ms, 16 frames in
    ≈3.5 s); the same goes for the map's mist.
- **Cloud shadows**: `sclouds.raw` 40×40, one texel per cell from the cloud's corner, `lum = min(lum, max(48, 255 − (255
  − s)·α/255))`.
- **Skin ids 0xFF…** (`mist.l3d` 0xFFD0EBBE, `moon.l3d` 0xFF52BA30): they are never resolved; those meshes are loaded
  without skins (`LH3DMesh::Create(data, 1)`) and their texture is set by the code. Moreover their skin table starts at
  the end of the file (like `sun.l3d`).
- The clouds' colour (`fn_005E1DE0` → [0xBF3398]) is an integer lerp of good 0x00FFFFFF, neutral 0xC8FFFFFF and evil 0xFFAAA066 (0xBF339C) by the sky alignment; the full-light green [0xEDDD09] is the second byte of table[255].
## openblack

Status: **done** for the sky type, the dome, the sun and its glare, the moon and the clouds with their shadows. The
API is `openblack::sky_type::` (`src/3D/SkyType.h`); the dome is `3D/SkyDome` and `SkySystem`. Progress:
[sky](../project/progress/sky).

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Sky type**
  - openblack difference.
  - openblack copies it with `!(t >= x)` comparisons.
  - openblack carries on.
  - openblack blends 256 rows at all levels.
  - openblack uploads the 3 layers to the GPU only when the last row is finished, so the dome changes all at once
    after 8 frames, like the original.
  - openblack: `sky_dome::AlignmentPair(1 − SkyAlignment)`, its dome k is layer 2 − k, and `fs_sky.sc` mixes them by
    w / 255, with the second alone at w = 255.
  - openblack moves the sky's alignment after the table's build, in `UpdateClouds`
  - openblack: `sky_dome::ThroughOvercast` for the sun, the moon and the glare (see [The sun and its
    glare](#the-sun-and-its-glare)).
  - openblack rebuilds it every frame.
  - openblack does not load saved games.
- **Sky: sun, moon and clouds (original)**
  - openblack: Z written, `ZFunc::LessEqualInclusive`, in both passes.
  - In openblack, `DrawMoon(…, mirrored)` in the reflection pass: the halo with the mirrored camera's view and the
    mesh with the main one's (it comes out flipped, like `DrawUnderWater`, with the culling inverted).
  - openblack: `DrawMoon` draws the glow, then the moon in the main pass, and the moon, then the glow in the
    reflection pass (a sequential view).
  - openblack: `components::Moon::phase`, on the sky's moon entity (`SkySystemInterface::GetMoon`, kept from land to
    land) and written by `DrawMoon` once the moon shows.
  - openblack: `sky_frame::TakeMoonPhase` (`k_MoonDateReadInterval`, the date and the tick of the last read on
    `components::Moon`; `test_sky_frame`), called by `DrawMoon` once the moon shows.
  - In openblack it goes at the end of the reflection pass (a quad at y = 0 is its own mirror) and is seen through
    the sea.
  - openblack: `Clouds` (the same generator; fixed seed with `OPENBLACK_CLOUD_SEED=<n>`, applied right before the
    first layout; `Clouds::OnLandscapeOpened` from `InitializeLevel`).
  - openblack makes the original's 6 draws per cloud in its order (`Clouds::Layout`: x, y, z, the mist's start,
    size, k; the counter = int(Random(0, 16)) & 15), 420 per layout, and makes the layout at land open as the
    original: the script's `LOAD_LANDSCAPE` → `Game::LoadLandscape` → `InitializeLevel` →
    `Clouds::OnLandscapeOpened` → `CloudSystem::Reset`, once the island is loaded and before its walkable mask
    (`land_avoid::Validate`, the original's `ValidateLandAvoid` 0x5E5541 after 0x5E5510)
  - openblack: `SkyAlignment` (Renderer), target `Clouds::InfluentialPlayerAlignment()`
    (`ecs::effects::alignment::GetInterfaceAlignment()` × 2 − 1, or the debug slider, or
    `OPENBLACK_TEST_SKY_ALIGNMENT` from −1 evil to 1 good; `atmos_banks::Alignment()` uses the same value) and
    cloudiness `Clouds::WeatherOvercastAtCamera()` = byte 3 of `weather::atmos::GetWeatherSmooth(camera, 1)` × 0.01
    (0 without storms).
  - openblack: `Clouds::BuildShadowCap` → per-cell R8 texture used by `vs_terrain` and `vs_object` before the light
    table.
  - openblack's L3D parser now tolerates it.

### The sky in openblack

- The dome's time blend is done on the CPU, in `SkySystem::BlendDome`, on a copy of the 9 textures; `fs_sky.sc` no longer blends times,
  only alignments (layer 0 evil, 1 neutral, 2 good).
- Removed: `Sky::GetCurrentSkyType` with script time and invented thresholds 3.5 / 7.5 / 8 / 8.5 with `<=` (and later
  its forwarder 2 − `Frame()`), `Sky::SetDayNightTimes`, `u_skyAlphaThreshold.x` (fs_object does not read it), `u_sky` and its
  fallback ramp in `fs_water` (without `palette.raw` the sea now goes unlit, white, **🔍 inferred**: the original always has
  the table), and `u_skyAndBump.x` (it is 0; fs_terrain no longer reads it: the sky type reaches the land only through the
  light table and the haze of vs_terrain).
- **General fix**: `HashIdentifier(hashed_string)` hashed the number again; `Contains`/`Load` with
  `entt::hashed_string` found nothing (it was the cause of the "missing raw/* textures").

### The sun and its glare in openblack

- `Renderer::DrawSun`, `graphics::sun`: the sun's alpha is exact (`sky_dome::ThroughOvercast`), and so is
  the glare's (`sun::GlareAlpha`: `ThroughOvercast(G · A · (1.0f/255.0f), overcast, fog)`, the whole number over 255
  for the shader). The samples are the original's (`sun::k_GlareSamples`, `sun::GlareSamplePoints`, near = the drawn
  near clip, `Camera::GetDrawnNearClip`: the original's (h·0.05)·3.2 + 0.3 every frame, or a lens's own; the nudge in
  the original's float order, d = (sun − camera) · (near / sqrt((dz·dz + dy·dy) + dx·dx)), 0x86BD91..0x86BE30), each
  hidden when `LandIslandInterface::RayCast` from the camera hits (`sun::HiddenSamples`), or else, at the detail
  indices 3, 4 and 6 (`detail_level::GlareDepthTest` of `draw_list::DetailIndexOf`, so openblack's levels 3 to 6,
  since a level above 4 runs as 4; openblack's depth buffer is taken to be the 16-bit, non-KYRO case), when it is
  on screen (`sun::SamplePixel`, the original's rules) and something is drawn at its pixel nearer than
  `sun::GlareHidingDepth(near)` = near / (1 − 64999.5/65535), taking the 16 bits as rounded. The glare's ease,
  scale, colour and gates are the original's. Tests: `test_sun`, `test_detail_level`; testbed scenario
  `sky.sun_glare_behind_bank`.
  - **Known difference: the depth test's answer is a frame or two late.** The original locks and reads that frame's
    Z buffer on the CPU. bgfx gives no read back of the depth buffer within the frame (a read back of a texture is
    answered frames later too, and costs a copy of the whole target), so openblack asks the drawing instead
    (`Renderer::AskGlareSampleDrawn`): an occlusion query of a one-pixel square over the sample's pixel at the hiding
    depth, drawn after the scene with the depth test and writing nothing, which passes no pixel where something
    nearer is drawn. Its answer is read the next time the glare is worked out, while the sample is on screen, so a
    sample is counted hidden by the drawing a frame or two after the original would count it, and stays hidden as
    long after. With the glare easing by 1 % per ms this moves the glare by a frame or two of its ease, not where it
    settles. A query keeps its last answer while it is not asked (its sample off the screen, the test off), so one
    not asked the frame before is made afresh, and the sample is hidden only by the land until its first answer.
    Without occlusion queries (`BGFX_CAPS_OCCLUSION_QUERY`) only the land hides it.

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/Clouds.cpp`: `SeedCrtOnce`.
- `src/3D/Clouds.h`: `StampShadows`.
- `src/3D/Implementations/Sky.cpp`: `DayNightClock::ForceScriptTime`, `Sky::BlendDome`, `Sky::Sky`.
- `src/3D/SkyType.h`: `BlendRows555`.
- `src/Graphics/Renderer.cpp`: `Renderer::CollectClouds`.

### Test hooks

- `OPENBLACK_CLOUD_SEED=<n>`, `OPENBLACK_TIME_OF_DAY=<h>` and `OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>` (sky and clouds;
  [Test hooks](../rendering/frame.md#test-hooks)); the testbed scenario `sky.sun_glare_behind_bank`. The full list:
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending
### The sun and its glare, still open

- How the driver turns 1 − near/z into 16 bits (rounded or cut) is not known; openblack takes it rounded. Which
  passes write the Z buffer before the glare (the translucent objects, the sea, the particles) is not checked one by
  one against openblack's views.
- None of it shows in the day verification shot (Land 1, 12 h): the sun is 149° from the view direction.

