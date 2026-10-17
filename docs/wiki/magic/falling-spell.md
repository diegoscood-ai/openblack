# The falling spell

## Summary

The falling spell is the film the game plays when a creature is first given a miracle (`fall.bik`, CHL 203
`SET_AVI_SEQUENCE(on, 2)`). Over the film the original draws more than the video: 16 large orange smoke puffs fixed
to the screen, a burst of coloured light rays around the creature, and a copy of the player's creature seen through a
camera path read from `fall.cm2`, tinted as the film goes on and with its hands glowing.

openblack draws the puffs, the bursts and the path camera bit for bit, and makes and draws the creature's copy in its
tint (body and eyes); the copy's hair, its queued animation sounds and the hand glows are still to be drawn. The film
itself (its states, sounds, the temple white and the end) is on
[the falling spell film](../video/playback.md#the-falling-spell-fallbik).

## The original

### The falling spell: what FallingSpell draws

The `fall.bik` video, its states, sounds, the temple white and the end are in
[video playback](../video/playback.md#the-falling-spell-fallbik). Here is the rest of `FallingSpell`
(fallingspell.cpp, 0x525CB0..0x527290, fully read: `LightBurst::Init` 0x525D30, `LightBurst::Draw` 0x525DF0,
`FallingSpell::Init` 0x526060, the callback 0x526480 → 0x526530, `Close` 0x5264A0, `Draw` 0x5267D0, the
update 0x526E00). Note the name clash: 0x5539A0 is `GGame::KickOffFallingSpellVideo`, which starts the film. What
is the original's behaviour; openblack's port of it is under openblack.

#### The 16 puffs

- **The 16 "sparks"** are not sparks: they are 16 large puffs of `smoke.raw` (material [0xEA1ABC], mode 6) in
  orange, anchored to the screen. `LH3DSprite::Create(0x10, 1)` 0x52631E (+0x34) and 16 records of 0x20 bytes
  (new(0x200) 0x526335, +0x38): `x`, `y` = Random(−0.1; 1.1) (screen fraction), depth Random(5, 25),
  size Random(4, 8), spin Random(−2, 2), shrink 2 − y, colour (min(2v, 255), v, v/3) with v = ftol(Random(16, 100))
  (0x526352..0x526400).
- They are drawn only with `+0x1C` set: the update sets it **once** to 1 on moving to state 1, 13.45 s (0x527047);
  `Draw` rewrites it with "some was seen" (0x526DCA).
- Each `Draw` (0x526BFD..0x526DCA, g_delta_time): alpha = ftol(255 − 32·age) with the **previous** age, age +=
  0.0013·dt; without alpha it is not drawn; half width ((size − shrink·age·0.5) + 1)·0.75 (min. 1e−4), angle
  spin·age + i, cell ftol(age·8) & 15 (rows 0-1 of the 8×8 sheet), position
  `Get3DPointFromScreen((ftol(W·x), ftol(H·y)), depth)` 0x81B370 and then y += (spin + 1)(1 − y)·0.0013·dt·0.1;
  `AddDrawing` 0x840C70 (Z-sorter, far to near).
- They are drawn with `LH3DSprite::Draw` 0x840530 in mode A (flag 0x40 clear; `billboard::Screen`) and FOV π/4
  (`ChangeFov` 0x526EB3 before each `Draw`): in pixels, half width = size·(W/2)/(tan(π/8)·depth), hundreds of pixels.
  The Z-sorter key is |pos − g_camera|², and a puff at or before the near plane [0xE839E0] is not drawn. They are
  visible from 13.45 s to about 19.6 s (255/32 of age).

#### The light bursts

- `LightBurst` is **one** object of 0x400 bytes at +0x3C, whose `Init` is called **twice** (0x52643E and 0x52644C);
  they are not "two LightBursts". It holds 64 rays: R = LocalFloatRand(1) + 0.5, ×1.4 with LocalRand(16) < 2,
  ×0.714286 with LocalRand(16) < 1; phase Random(0, 2π); rate Random(2, 20), negative if Random(0, 1) < 0.5; sway
  Random(−0.8; 0.8).
- `LightBurst` (0x400 bytes) is four arrays of 0x40 floats at +0x000, +0x100, +0x200 and +0x300 (Init 0x525D34
  `mov edi, 0x40`); `Draw` writes 0x80 vertices, the centre and rim point of each spoke (0x526042).
- The end-of-frame callback 0x526530 runs **from state 2** (37.75 s): centre = the midpoint of two points of the
  creature (fn_004813D0, 0x48F180) projected (fn_008190D0; nothing if it ends up behind the near plane, **without
  advancing**: no step without the centre, 0x5265F7); d = 0.7·dt s: +0x2C += 0.1·d (alpha ftol(255·+0x2C), cap 255),
  +0x28 += 0.25·d and, past 0.5, +0x30 += 5·d.
- Four `LightBurst::Draw`: radius 10·g³, 25·g, 50·g, 75·g² (g = +0x30), a5 = +0x2C, −+0x2C, −2·+0x2C, 2·+0x2C,
  colours (R,G,B) (0x20,0xFF,0x40), (0xFF,0x40,0xFF), (0x40,0x80,0xFF), (0xFF,0x40,0x40) with that alpha.
- Each one is a fan of 64 triangles (2i, 2i+1, 2((i+1)&63)+1), radius (R + W·sin(a5·B + P))·radius·(4s²k + 1 − k),
  s = |sin((i+1)·a6²·π/4)|, k = clamp(1 − a6², 0, 1), angle i·π/32 + a5 (sin a x, cos a y), centre with the colour
  and edge with only the alpha; the last spoke closes on the first rim point (0x525EE6, `and ecx, 0x3F`); UV
  u = 0.5 + frac(i/64)/4, v = 0.25 + frac(0.4·a5)/4 (centre) or 0.25 + frac(|1 + 0.4·a5|)/4 (edge; equal with
  a5 ≥ 0) of `atmos.raw` with `LH3DAtmos::AdditiveMaterial` [0xEDC364] (mode 13), `Draw3DWorldTriangle` 0x526047.
  With a6 = 0 the fan is a point: it grows with +0x28.
- `LightBurst::Draw` 0x525DF0 (ecx = the burst, 7 stack arguments, `ret 0x1C`): the depth only places the points for
  the Z sort, because `Get3DPointFromScreen` (0x525E31 / 0x525FA1) projects back onto the same pixels. The UV helpers
  0x525CD0 / 0x525D00 are frac(|x|)/4 + 0.25 or + 0.5, with frac = x − ftol(x).
- The bursts are placed at the near depth [0xE839E0] × 1.1 (0x52666C..0x526676).

#### The drawing order in mode 2

`FallingSpell::Draw` draws the video itself (`thedraw(0)` 0x52689F, which clears the player's flag +0x64: the 0x8000
callback of `FinishFrame` no longer paints it, 0x844E3A..0x844E49), then `FinishFrame`: the Z-sorter (the puffs), the
callback 0x526480 (the bursts), the bands and the fade.

#### The model light

`Init` stores [0xEA9E90] in +0x10 (0x5262E0..0x526311), `Draw` sets it to (0, 0, 1000) (fn_0081E1F0, for the
creature), `Close` restores the stored one (0x5264E8..0x5264F4); `FallingSpell::Draw` keeps the model light and puts
it back (0x526873..0x526895). This corrects an older note of the video page: what `Close` restores is not the camera,
it is this light (fn_0081E1F0 = `model_light::SetLight`).

#### The camera path, fall.cm2

- It belongs to the spell, not to the video: `LHLoadData` 0x52609C + copy fn_0086D4A0: header {size, duration
  48 333 ms, 1449 keys} and keys of 0x48 bytes (position, focus and 12 matrix floats). The path comes from
  `LHFileLength` / `LHLoadData("data\spells\fall\fall.cm2")` (string 0xBE9C78, 0x52607A..0x5260A2).
- fn_0086D760(ms): key (n−1)·t/dur, fraction **(t − ftol(t/step)·ftol(step))/step** with step = dur/(n−1) = 33.379:
  the remainder uses the integer part of the step, so the fraction exceeds 1 from about 2.9 s onwards and the
  original's camera **extrapolates** from the key (ported the same); a·(1−f) + b·f for position, focus and matrix.
  Past the duration, t = duration − 1 (unsigned `jbe`, 0x86D76E..0x86D77A); the key (n − 1)·t/duration is unsigned
  and wrapped into n; the next key is the same one at the end (0x86D7E2..0x86D7F5). Its arguments: ecx = the path,
  edx = a matrix that transforms position and focus when not null (the update passes 0), then t and the three
  outputs, `ret 0x10`.
- The update (0x526E9B..0x526F1C) and `Init` (0x526259..0x5262BF) give position and focus ×0.8 and the matrix to
  fn_00819F50, and `ChangeFov(π/4)` (only the update; `Init` does not change the FOV).
- `ChangeFov` 0x8195B0 takes the horizontal angle: [0xC3812C] = tan(fov/2) × near, [0xC38130] = that / aspect
  (0x4424C6..0x4424E2).
- **fn_00819F50** (0x819F50..0x81A74B, called only by `Init` 0x5262BF and the update 0x526F1C; ecx = position,
  edx = focus, stack = matrix, `ret 4`) is **`LH3DTech::UpdateCamera` 0x819920 with the path's rotation**: the same
  beginning (debug camera [0xEA9EC8] / [0xEA9ECC], the shake fn_008210C0, g_camera 0xEA1DB8 and its focus
  0xEA1DC4, the look-at of `UpdateWorldToCamera` 0x81A10D) and the same ending (matrix B 0xEA1C98, g_world_to_clipping
  0xEA9E40 scaled by [0xE83A00] / [0xE83A04], its inverse, the octant [0xEA9EBC], the plane [0xF03128] → [0xF03118],
  the Y rotation matrix 0xEA1D88 from `GetYAngle` + π/2, the light [0xEA9E90] → [0xEA9E80], `SetD3DMatrix` and
  `SetTransform(VIEW)`, the inverse 0xEA1CF8). What differs: from the first nine floats a0..a8 of the matrix
  (0x81A075..0x81A0F5) it makes (a0, a3, −a6, a1, a4, −a7, a2, a5, −a8), normalises each row of three (fn_007FB5C0,
  with the table-based `InverseSquareRoot` 0x841170) and **copies it over the look-at** at 0xEA1D28..0xEA1D48; the
  translation 0xEA1D4C..0xEA1D54 = −(column · position) (0x81A112..0x81A22F). So the camera's right, up and forward
  are rows 0, 1 and −2 of the path matrix; the focus does not rotate it (it only counts for the octant, the Y
  rotation and the shake); a9..a11 are not read.
- **Who sets the game camera.** `GCamera::Update` (0x44233C..0x4423F4) with `g_game+0x205A28` = 1 or 2 takes
  g_camera (that of the previous frame, here the path's) as its drawn camera, and with ≠ 0 it skips `ChangeFov`
  (0x4424F0) and `UpdateCamera` (0x4425F6..0x442602); GCamera's zoomers carry on with their own thing. `Close` does
  not touch the camera: **nothing restores it**, the first frame in mode 0 draws GCamera's again (`ChangeFov`
  0x4425D3 with its FOV, `UpdateCamera` 0x442622). `GetNearClipping` 0x4424AF runs in all modes from GCamera's
  camera.

#### Init and Close

- `Init`: it first closes an open spell (0x52606A..0x526070). `LH3DSprite::Create(0x10, 1)` applies `SetToZero`
  0x8404F0 and the flag 0x80. The burst values are +0x28 = 0, +0x30 = 1.0 and +0x2C = 0, then
  `RegisterFinishFrameCallback(0, 0, 0x526480, this)` (0x526459..0x526463).
- `Close`: nothing when it is not open (0x5264A7). Otherwise `RemoveFinishFrameCallback` and +0x00 = 0; the path is
  freed (`fn_0086D4D0`), then the records, the sprites and the burst. `Draw` does nothing when +0x00 is 0 or when
  there is no film (0x5267F4).

#### The copy of the creature

Read from the exe and pinned with the oracle, as rules of the film time:

- `Init` (0x5260D1..0x5261EB): `new(0x57B8)` and the copy constructor 0x47F490 of the local player's
  `GetCreature3D` (players[+0x205A59].creature), at the position (0, 0, 0) with no extra argument; vtable
  `CreatureFalling` 0x8D8BD8 (its only own slot, vt+0x28 `GetAltitude` 0x526490, returns 0). `SparklingCreature`
  [0xC64204] = the copy, its position +0x78 = (0, 0, 0), heading +0x84 = `angle_correct(0)`, `UpdateTime(1)`
  (vt+0x14), +0x0C = 100, +0x4A90 (`IsAnimationTimeModified`) = 1, `ResetLook` 0x4831B0,
  `StartIndividualAction(0xD7, 0)` 0x484B80 (no draw: `IsPerformingBodyAction`, `GetAnim(0xD7, 1)`, +0x49A0 = 0xD7,
  +0x522C = 0, `StateSet(10)`), then `HandGlows` (fn_0068E230, at copy +0x57B0), glows 0 and 2 made (fn_0068E300) and
  `SetScalePowerTime(type, 1, 1, 1)`.
- The update (0x526E74..0x526EA6): d = t − +0x0C (32-bit); `UpdateTime(d)` only when d > 0; +0x0C = t always.
- `Draw` (0x5268A4..0x526BFB), with t = frame·1000/fps read again (fps ≤ 0 → 24): first the copy's
  `PrepareForDrawing` (vt+0x18) every frame. **Hidden** when 19 550 ≤ t ≤ 27 350 (`SparklingCreature` = copy +
  0x57B8, then straight to the puffs at 0x526BFD: no tint, no `DrawNow`, **no glow call and no draw**). Otherwise
  `SparklingCreature` = the copy and the **level** v = 255t/19 450 (t < 19 450) or 255(t − 19 450)/7900
  (t < 27 350), else 0, in 32-bit signed integers truncated toward zero, held to 0..255. Then **three `LocalRand`**
  in this order: 32, 8, 4, giving the flicker (0x20 + r32, 8 + r8, r4). The body (`DynamicShadow` +0x482C, the
  complex body object) gets vt+0x2C with **edx = the diffuse colour and the stack = the specular** (the same slot
  `PrepareForDrawing` calls with 0xFFFFFFFF / 0 and `MiniMap::DrawMarker` with `ApplyCitadelColoring`'s pair):
  diffuse channels 255 − ⌈k·v/256⌉ for k = 127, 191, 255 (white at v = 0, (0x80, 0x40, 0x00) at 255; the bit
  operations at 0x5269E6..0x526A36 checked for every v); specular channels (flicker·v) >> 8 (0x5269A0..0x5269E0);
  both alpha 0xFF. Then `DrawNow` 0x526A42. So the flicker is the **specular**, rising from black, and the diffuse
  darkens from white; the tint drops back to white / black at 19 450 ms and after 27 350 ms.
- The glows (0x526A47..0x526BFB), only when drawn: x = t − 16 350 (16 350 ≤ t < 17 350), 1000 − (t − 17 350)/2
  (17 350 ≤ t < 19 350, the halving truncated toward zero), t − 31 650 (31 650 ≤ t < 32 650), else 0, for glow 0;
  the same with 17 200, 18 200..20 200 and 36 500 for glow 2. x = 0 makes **no call**; otherwise x·0.001 held to
  0..1 and `SetScalePowerTime(type, 1.0, 2x + 1, 1.0)`. `SetScalePowerTime` 0x68E340: a third argument of 0 removes
  the glow, else it is made if missing, glow +0 = the third argument, glow +4 = the second; the fourth is not read.
- **The copy's `PrepareForDrawing` and `SparklingCreature`.** `Draw` calls the copy's vt+0x18 at 0x5268A9, right
  after `LHVideoPlayer::thedraw` and before t is read, so on every frame, hidden or not. vt+0x18 of `CreatureFalling`
  is `LH3DCreature::PrepareForDrawing` 0x4ED320 itself (the creature's): it plays and drops the queued sound effects
  whose fraction is below f, then blends the drawn bones +0x47F0 = f·+0x5178 + (1 − f)·+0x5174, f = min(the game's
  turn fraction, 2) ([the creature](../creature/overview.md), "The drawn body"), resets the body's colour and
  specular (vt+0x2C with 0xFFFFFFFF / 0) and prepares the hair. No random number. The film pauses the game and the
  turn fraction is kept in a pause, so the copy is drawn between the last two poses its own `UpdateTime` calls gave
  it (each one poses through `UpdateBuffers`, and so does `ResetLook` at `Init`), at the fraction the pause froze.
  Its eyes are placed in `DrawNow` (only on drawn frames), their look moving on by its last `UpdateTime`'s
  milliseconds (`IntTimeInc`).
- `SparklingCreature` [0xC64204]: the copy at `Init` (0x526124), then each `Draw` writes it, copy + 0x57B8 in the
  hidden window (0x5268FF: an address that is no creature) and the copy otherwise (0x526912); `Close` sets 0.
  `AddEvilGoodSparkles` 0x481410 returns at once unless it is 0 or the creature itself, so the copy makes no sparkles
  (and draws none of their local numbers) in the `UpdateTime` that follows a hidden `Draw`, the next frame's.
- The debug keys 0xE85376..0xE85379 turn the copy (0, π/2, π, 3π/2).
#### More addresses from the code

- The burst's constants: × 0.714286 [0x8D8BCC] (0x525DCA..0x525DE1), the angle i π / 32 [0x8C7C94]
  (0x525F5C..0x525F92), 75 g² [0x8D8C04] (0x52676B..0x5267B7), g_delta_time × 0.0013 [0x8D8C08]
  (0x526C0A..0x526C20).
- The path's lerp: a's twelve × (1 − f), fn_0086DAC0 (0x86DAC0) b × f, fn_0086DA50 (0x86DA50) the sum.
- `Close` frees the sprites with `LH3DSprite::Release` (0x5264F9..0x52651F).

### Sources

- The exe (fallingspell.cpp's range, read whole) with the xref index; the copy's tint, glows and the `fall.cm2`
  sampler with the oracle.

## openblack

Status: **partial**. The puffs, the bursts, the model light, the drawing order and the path camera are ported and
faithful; the creature's copy is made and drawn over the video in its tint (body and eyes, no land light, no haze,
the eyelids skin × tint / 255); its hair, its queued animation sounds and the hand glows are not drawn yet. The tint is an
`ObjectColour`-like diffuse and specular pair.

- `Magic/Objects/FallingSpell`, drawn by `Graphics/RendererFallingSpell.cpp`. Casting: CHL 203
  `SET_AVI_SEQUENCE(on, 2)`; in tests `OPENBLACK_TEST_VIDEO=fall`.
- **Mode 2 order**: `Renderer::DrawFinishFrameOverlays` draws the video, the puffs, the bursts, the bands and the fade
  in the original's order with `HidesWorld()`.
- **The camera**: `FallingSpell::Hooks::applyCamera` on each update with the path's camera and empty in `Close` (the
  equivalent of mode 0): the FOV with `SetProjectionMatrixPerspective` (π/4 horizontal, like `cameraXFov`; on
  closing the configuration's `cameraXFov`) and the view `falling_spell::WorldToCamera` with `Camera::SetDrawnView`,
  which `GetViewMatrix` returns while it is set without touching the zoomers. Without land or a creature in mode 2
  nothing is seen.
- **The `fall.cm2` sampler bit for bit**: fn_0086D760 run by the oracle on two synthetic paths (7 keys over 200 ms,
  and fall.cm2's 1449 keys over 48 333 ms) at 24 film times, including past the duration and 0xFFFFFFFF:
  `CameraPath::At` gives the same 18 floats' bits (test `CameraPathMatchesTheOriginalBits`).
- **The copy's rules** as pure functions: `Magic/Objects/FallingSpellCreature.h`, tests `FallingSpell.CreatureTint*`,
  `GlowPowerMatchesTheOriginal`, `CreatureAdvance`. The oracle ran `Draw` with a fake game, video player, copy and
  body (the body's vt+0x2C recording edx and the stack); 20 cases of film time and local seed are in the tests.
- **The copy's drawing** (issue #23): `FallingSpell::Draw` calls, after the light, `Hooks::prepareCreature`
  (`CreatureAnimationSystemInterface::PrepareFallingCopy`: the drawn bones from the copy's two poses in its
  `CreatureTurnPose` at `game_clock::TurnFraction`, kept in pause), then `Hooks::setCreatureSparkling`
  (`FallingCreature::sparkling` = not `CreatureHidden(t)`), then on shown frames `Hooks::placeCreatureEyes`
  (`PlaceFallingCopyEyes`, the look by `FallingCreature::lastStepMs`). The copy is posed (sampled and
  `creature_turn_pose::Advance`) by each step of its time and once more after its look reset at the start, before
  the falling action begins. Tests: `FallingSpell.EachDrawPreparesTheCreature...`,
  `CreatureAnimationSystemFallingCopy.ItIsPosedStandingTwice...`.
- **Approximate**: `Get3DPointFromScreen` without going through the near plane (it cancels out; the original rounds
  with it); the Z-sorter key computed in camera space; `Init` runs on the first frame of the video and not in the
  turn of the CHL 203 (the Random/LocalRand come out a bit later in their sequences); a `KickOff` over another one is
  only detected when the previous one had already had its puffs; exact 1/√ and not the `InverseSquareRoot` table;
  `Init` and the first update in the same frame. Puffs and bursts are drawn in pixels in the `ScreenOverlay` view
  with ZFUNC ALWAYS: in mode 2 nothing writes Z before and the reset quad of `FinishFrame` (d) leaves z = 1, so its
  LESSEQUAL always passes.
- **Inferred**: the mode 2 near plane [0xE839E0] is that of openblack's camera (the depths 5..25 pass it).
#### Where the code is

- `Magic/Objects/FallingSpell`: `BurstIndices` (the fan's index list), `SparkQuads` (the puffs queued this frame),
  `BurstV` (the UV helper), `CameraPath::Parse` (the 0x48-byte keys), `FallingSpell::FinishFrame` (the bursts, from
  state 2 only), `LoadFallPath`; the camera goes through `Hooks::applyCamera`.

### Test hooks

- `test_falling_spell` (15): indices and geometry of the fan, ranges of `LightBurst::Init`, the light stored and
  restored, `Init` of the puffs, nothing without `+0x1C`, position/size/angle/cell/alpha on screen and far-to-near
  order, the fade and the rewritten `+0x1C`, bursts only from state 2 and with a centre (alpha, +0x28, +0x30), the
  fraction of fn_0086D760 and its drift (t = 48 000: f = 16.36), the camera ×0.8 and the real `fall.cm2`, the camera
  applied on each update and restored in `Close`, and `WorldToCamera` like fn_00819F50.
- `OPENBLACK_TEST_FALL_LOG=1`: one line per second of video (state, puffs, bursts).
- `OPENBLACK_TEST_FALL_BURST_AT=fx,fy`: a centre at that screen fraction instead of the creature.

## Pending

- **The copy's hair, its queued animation sounds and its hand glows 0 and 2** are still to be drawn (queued as a
  fix). The three `LocalRand` of each drawn frame must be made with the tint, or the local stream is three draws
  short each drawn frame of the film.
- **The camera's drawn view**: `Camera::SetDrawnView` is not in the camera yet; until it is, only the FOV changes.
- **The centre of the bursts** comes from the creature's two points: without a creature there is no centre, and the
  bursts are neither drawn nor advanced.
- **The debug keys** 0xE85376..0xE85379 that turn the copy are not ported.
- **Not ported**: the debug camera and the shake on the path camera, and the readers of g_camera in mode 2
  (`GetWeatherSmooth` 0x4426BA, GCamera +0x74).
- **Inferred**: the near plane is still openblack's (`GetNearClipping` runs in all modes from GCamera's camera).
- **Not settled**: `Draw` reads the film frame again after the update did; the original's poll timer may move it in
  between, so the draw's t can be one frame past the update's; openblack uses the update's ms for both. Verify with
  the oracle by running the update and `Draw` with a moving fake poll timer.
- What action 0xD7 is in each species' animation set is not named here. Known: `StartIndividualAction` 0x484B80 only
  stores the action at +0x49A0 and the flag at +0x522C (state 10) after `GetAnim(action, 1)`, so the clip comes from
  the species' animation tables, which are data. Still open: reading those tables per species.
