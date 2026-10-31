# Day and night

## Summary

The original keeps two clocks in hours: the visual time, which advances by turns (one day every 1700 s of game time by
default, with the cycle's night and change fractions), and the script time, the visual time mapped onto the fixed
hours 3.5 / 7.5 / 8 / 8.5, which the scripts, the sun, the moon and the street lanterns use. At night (the light
table's base mean below 120) houses with someone at home light their windows, the hand and the street lanterns stamp
light onto the land, the lanterns loop their sound, and fireflies come out.

openblack reproduces both clocks turn for turn and every night light. A new game opens at noon and a land change keeps
the clock; Land 1's script then sets 15.4 h and, at about turn 9, 4.59 h with the clock running, so the verification
runs turn dark there (see [Night lights](#night-lights)).

## The original

### Clock

There are **two clocks** in hours 0..24:

- **Visual time** (`GLandAlignement::VisualTime` 0xBF3380). It advances linearly, one day every `duration` seconds of
  game time (1700 s by default, about 28 min). It is moved by `GLandAlignement::UpdateTime` 0x5E1FE0 once per
  turn, called from `GGame::ProcessTurn` 0x54E6AE with `(scale · 0.1, 0.1)` (`GGame::ProcessTurn` passes the turn length [0xD01A3C] × 0.1, 0x54E6AE..0x54E6C3). With the game paused it does not advance and with
  the game sped up it goes faster (it goes by turns).
- **Script time**: the visual time passed through a piecewise linear function (`fn_00869FD0`; `fn_0086A160` visual →
  script, `fn_0086A110` script → visual). It maps the cycle thresholds to the fixed hours 3.5 / 7.5 / 8 / 8.5
  (0xC395B0..BC), mirrored around 12. It is used by `GET_GAME_TIME` / `SET_GAME_TIME`, the sun (`fn_0086C020`), the moon
  (`LH3DAtmos::UpdateGame`) and the street lantern lights (`fn_0086C220`). It is **not** the time of the sky type: that
  one runs on the visual time (see [Sky type](sky.md#sky-type)).

**Cycle** (`GGameInfo::SetVisualTimeCycle` 0x557620, `(duration, night, change)`; fractions of the whole day):

- Speed: `n = ftol(duration · 0.41666666f)` ([0x8DF8F0] = 0x3ED55555) and `10/n` hours per second, the same by day
  and by night (0xBF338C and 0xBF3390 hold the same value; 0 for n = 0, 0x557645..0x557685). The product stays in the x87 register before `__ftol`: with the
  FPU at 24 bits (`fn_007DEE00`, `and cw, 0xFCFF` at 0x7DEE0D) it is the float product that openblack computes; with 53 bits
  durations that are multiples of 2.4 (1200, 2400) would give one `n` less.
- Thresholds (`LH3DSky::SetDayNightTimes` 0x869FA0 → 0xFA26A0..94): N = 12·night, E = 12·change + N (stored as
  float), c = min((E − N)·0.25, N), and `SetDayNightTimes(N − c, c + N, E − c, E + c)` (0x55768F..0x5576E4; ✅ faithful,
  openblack does it in this order from `DayNightClock::SetCycle`, which also calls `sky_type::SetThresholds`).
  By default (1700; 0.083; 0.07) they are **0.786 / 1.206 / 1.626 / 2.046 h**
  from midnight. That is, there is full night only between 23.2 and 0.8 visual hours and it is day between 2 and
  22 h: the night lasts about 2 real minutes and each transition another 30 s.
- `LH3DSky::Time2SkyType` 0x86A1B0 over the visual time: 2 at night, 1 at dusk, 0 by day. `IsVisualNight` is > 1.2
  (the double). All the detail, in [Sky type](sky.md#sky-type).
- `fn_0086A3B0` sets (4.5; 7; 7.5; 8.25) when opening the sky, but `GLandAlignement::Open` overwrites it immediately with the
  default cycle.
- `ResetGameTimeProperties` 0x711520: 1700 s, 8.3 % night, 7 % change, the floats at 0x8DF8E4..0x8DF8EC in that
  order (confirmed: xref constants). `SetGameTimeProperties` 0x7114B0 pops the change share first, the night share
  second and the duration last (confirmed: xref disassembly). `SetVisualTimeScale` 0x557610
  (GAME_TIME_ON_OFF). Target hour 0xBF3384 (12), step 0xBF3388 (2.5 h per second of game time), MOVE_GAME_TIME in
  progress 0xD1A268.
- In the campaign the thresholds are **always** the default ones: `challenge.chl` never calls 407 or 408, Land1,
  Land4, Land5 and LandT have no `SET_NIGHTTIME` and Land2/Land3 repeat (1700; 0.083; 0.07). The playgrounds do
  change them: SandBox Creature Training and ThreeGods (1000; 0.23; 0.17) → 2.25 / 3.27 / 4.29 / 5.31; Demon God
  (1143; 1; 0) → 12 / 12 / 12 / 12, type 2 always except at exactly 12 o'clock (0); Ultimate Sandbox (3400; 0.4; 0.01) →
  4.77 / 4.83 / 4.89 / 4.95.

**Advance per turn** (`UpdateTime(a, b)`, a = scale · 0.1 and b = 0.1):

1. If there is no `MOVE_GAME_TIME` in progress: step 2.5 h/s and target = visual + speed · a, where speed =
   lerp(day, night, sky type / 2).
2. The visual time approaches the target by the short way (±12 h), at most step · b per turn, and wraps around at 24.
3. `SetTime` 0x5E22E0 normalises the target and sets it to 0 outside ±1000. With seconds ≠ 0, step = |difference| /
   seconds. `MOVE_GAME_TIME` finishes on the turn after arriving.

Checked: 20 000 turns plus a `MOVE_GAME_TIME(3, 30)` give in the port the same values as the emulation of the
original, down to the sixth decimal.

**Startup and scripts**

- `GLandAlignement::Open` 0x5E1D10: scale 1, default cycle and time 12, **only once per game**: it returns at once
  while [0xD1A264] is set (0x5E1D15..0x5E1D20), sets it (0x5E1D33), and only `fn_005E1DB0` (0x5E1DC1), from
  `GLandscape::Close` 0x5E5280 (called only by `GGame::Close`, 0x54EE1C) and `DoCitadelMultiplayer`, clears it. Its
  callers are `GLandscape::Open` 0x5E52E0 (0x5E5510; the map command `LOAD_LANDSCAPE` 0x714E13, a saved game's load
  0x558AC9) and `DoCitadelMultiplayer`. So a new game (after the last game's `GGame::Close`, or the first) opens at
  noon, while a land change (`LOAD_MAP`: `GGame::LoadMap` 0x552F40 releases the landscape with
  `LH3DLandscape::Release`, never `GLandscape::Close`) keeps the clock as it was: the visual time 0xBF3380, the target
  and step, the scale, the cycle and a `MOVE_GAME_TIME` going on. No other writer of the visual time
  (`UpdateTime`, `fn_005E22A0` from `ForceVisualTime`: `GGame::Load`, `fn_005576F0`, `Open`, `SET_GAME_TIME`) nor of
  the cycle (`SetVisualTimeCycle`: `GGame::Load`, `fn_005576F0`, the map editor's `SET_NIGHTTIME`, `Open`, the two
  game time property natives) nor of the scale (`SetVisualTimeScale`: `GGame::Load`, `fn_005576F0`, `Open`,
  `GAME_TIME_ON_OFF`) is reached from `ClearMap` 0x552BB0.
- Island script: `SET_NIGHTTIME(1700, 0.083, 0.07)` in Land2/Land3 reaches `SetVisualTimeCycleFromMapEditor` 0x557BB0,
  which limits night ≤ 1 and change ≤ 1 − night.
- CHL: `SET_GAME_TIME` 112, `GET_GAME_TIME` 113, `GAME_TIME_ON_OFF` 288 (scale 1/0), `MOVE_GAME_TIME` 289 (hour,
  seconds), `SET_GAME_TIME_PROPERTIES` 407, `RESET_GAME_TIME_PROPERTIES` 408.
- `SET_GAME_TIME` 0x710E20: ForceVisualTime (script → visual time); `GET_GAME_TIME` 0x710E60: the visual time mapped
  back to script time. `GAME_TIME_ON_OFF` 0x710E90: `SetVisualTimeScale(on ? 1 : 0)`; `MOVE_GAME_TIME` 0x710EC0: the
  visual time slides to the hour in `duration` seconds of game time. `SET_GAME_TIME_PROPERTIES` 0x7114B0:
  `SetVisualTimeCycle(duration, percentage night, percentage change)`.
- Land1 starts with `FollowUs`: `SET_GAME_TIME(7.3)` and `GAME_TIME_ON_OFF(0)`, a dawn with the clock stopped; later
  it does `MOVE_GAME_TIME(12, 460)`. `LandControl1` sets 15.4 h and turns the clock on. Other sequences set their
  time, for example `VillageWavingSequence` 12/16 h and the Land4 meteorite one 16.5 h.
- With the C runtime's 53-bit default (`__setdefaultprecision` 0x7CC96C) durations that are multiples of 2.4 would truncate one lower; the game runs at 24 bits.
- The fixed script-time thresholds 3.5 / 7.5 / 8 / 8.5 are the floats at 0xC395B0..0xC395BC.
### Night lights

- **Windows** (`Abode::Draw` 0x515F70): the submeshes with the L3D `isWindow` mark are shown unlit, with a flat grey
  224..252 that flickers slightly. It only happens if someone is at home (Abode +0xB6) and it is visual night. Each
  house has its own time offset: the fractional part of |x + z|·0.1 + y. "Someone at home" is `Abode::presentAtHome`
  (+0xB6, read first at 0x515F78, then `IsVisualNight` 0x5575E0), which the villagers raise as they arrive home and
  lower as they leave (`RenderingSystem`).
  - The colour (0x515F9E..0x516066): f = |z + x|·0.1 + y, t = (f − ftol(f)) + VisualTime [0xBF3380] (the offset cut
    towards nought, so a negative f gives a negative offset); the strength is ftol((t − 20.5)·8192) if t > 20.5, else
    ftol((3 − t)·8192) if t < 3, else nothing; 0 or less is no colour (0x51601E). The grey is
    (windowFlicker[ftol(t·1000) & 7] << 2) & 0x1F | 0xE0, the table [0x8D86D0] = {0, 7, 3, 5, 4, 2, 6, 1}, times the
    strength >> 8 below 256 (0x516044), packed 0xFFgggggg into the mesh object's vt+0x30; 0 otherwise.
- **Hand light and street lantern light** (done; `night_lights::Update`, called from `Renderer::UpdateClouds`):
  - Mechanism (`fn_008229B0`): an 8-bit image, remapped on load to min(47, v·48/255 + 0.5), is stamped onto the
    luminosity of the cells with a bilinear filter that the original shifts by one texel. v = r·trunc(I·255)/255. If v ≤
    lum·((G255·48) >> 8) >> 8 the cell does not change (G255 is the green of table[255]). Otherwise, v replaces the
    normal land (≥ 48) or the maximum with another light is kept. The range 0..47 is the warm ramp of the light table.
    the land shader then takes table[luminosity] as for any cell.
  - Only when the mean of the table's base is below 120.
  - Hand: `light_hand.raw` (12×12) from the position of the hand model − 55. No light if the hand is hidden. The
    position is CHand +0x78 (`fn_005E5830` 0x5E5843..0x5E5877), wherever it is, over the sky too; hidden is its 3D
    object's +0xAC (0x5E59C2..0x5E59D0), with no other test.
  - The hand light's strength (the same value is the hand glow on the water): the mean is an **integer**, the sum of the
    three bytes of the table's base colour divided by 3 and rounded down, and the strength is (120 − mean) × 1/15, a
    product by the float 1.0f/15.0f rather than a quotient by 15; ≤ 0 gives 0 and ≥ 1 gives 1. So it moves in
    fifteenths, one per whole value of the mean from 119 (1/15) to 105 (full). before, it kept the mean's fraction and
    divided by 15 (a sum of 316 gave 0.9778 instead of 1, the sum 332 gave 0.6222 instead of 0.6667). Only a mean from
    105 to 119 tells them apart. Tests: `test_hand_light`; scenario `hand.night_light` (16.6 h to 17.5 h with the clock
    stopped, a neutral player: the mean falls from 122 to 102).
  - Street lanterns: `CREATE_STREET_LANTERN` (type 7 = MSH_O_TOWNLIGHT, flames at +5) and bonfires (MSH_B_CAMPFIRE, +1).
    Stamps `village_diffuse.raw` (14×14) from pos − 50, with ±0.5 of jitter every 30 ms. Strength I/255 from
    `fn_0086C220` by script time: it turns on from 16.5 to 17.5 and turns off from 6 to 7 (on at 16.5 0xC395C4, off at
    7 0xC395C8, ramp 1 0xC395CC).
  - Lantern details: the jitter clock +0x10 starts at Random(0, 30) (0x8232FB); a flame's size Random(−0.1, 0.1) + 1,
    at least 1e-4 (0x823355..0x823382); the flames start at cell 0 (0x823397); new lights go at the head of
    [0xEB99B8] (0x8232D5..0x8232E7); every 30 ms (0x8234B6..0x8234FD, 1/30 [0x8CF3F8]) Random(−0.5, 0.5) for +0x14
    (0x823502) and +0x18 (0x823514) and the glow's size Random(−0.1, 0.1) + 3 (0x823526..0x823546); fn_007349E0(dark)
    plays the lanterns' loop only while dark (both branches of 0x5E5921); fn_00823690 stamps village_diffuse.raw.
  - Creation (`fn_00823240`) is **not gated by darkness**: 6 CRT draws in this order: the clock `Random(0, 30)`
    (0x8232FB), then for each of the 3 sprites (2 flames, 1 glow) the flame size `Random(−0.1, 0.1)` (0x823368, flames
    only) and the start `ftol(Random(0, 31))` (0x8233EE). Callers: `GStreetLantern::CallVirtualFunctionsForCreation`
    0x734905, `AnimatedStatic::CallVirtualFunctionsForCreation` 0x422565 (the Norse gate's lamps) and
    `CitadelHeart::DrawLantern` 0x4666E2 (2 lights, lazily, on the first draw with +0x9C > 0.07).
  - The darkness gate: `fn_005E5830` runs once per drawn frame from `GLandscape::Draw` 0x5E488E. With the mean of the R,
    G, B of [0xFA26A4] < 120 (0x5E5921) it calls, in order, the jitter `fn_00823460`, `fn_0086D360`, the village alpha
    [0xD20184] = clamp((120 − mean) × 1/15), `fn_007349E0(1)`, the hand light `fn_008229B0` (if the hand is shown) and
    `fn_00823690`; otherwise (0x5E5A20) [0xEB99BC] = 0, `fn_0086D360` and `fn_007349E0(0)`. 0x5E592A is the only caller
    of the jitter, so its 3 CRT draws per light per tick (newest first) are **not made while it is light**. The mean
    (0x5E58E4..0x5E591F): byte 0 `and 0xff`, byte 1 `mov dl, ah`, byte 2 `mov al, [0xFA26A6]`, summed as integers, then
    `imul 0x55555556` and the sign fix, a signed sum / 3 truncated (floor, the sum being ≥ 0). The gate `cmp esi, 120;
    jge` uses that integer mean; int mean < 120 ⇔ sum < 360 ⇔ the fractional mean < 120, so `night_lights::IsDark`,
    which keeps the fraction, gives the same answer. The strength (0x5E594B..0x5E599D): `fild (120 − mean)` × [0x8C9D38]
    (bytes 89 88 88 3D, 1.0f/15.0f); ≤ 0 → 0, < 1 as is, else 1.0, stored in [0xD20184]. 120 − mean is exact, so the x87
    product rounds once to a float, the float product openblack takes. The time of day only drives the brightness
    (`fn_0086C220`), not the gate. Before the gate every call sets [0xD20184] = 0 (0x5E5903), so while it is light the
    village alpha is 0; it is also zeroed when a land opens (`GLandscape::Open` 0x5E52F5). Its readers: the hand light's
    stamp (0x5E59E8, the dark branch only), the hand's glow on the water (0x5E4D89 / 0x5E4DA5: shown above 0.01, alpha
    clamp(k × 190, 0, 190)) and `ClearLight` (0x5E57B1, after each 3D frame, which takes the hand's stamp off when k >
    0); The flames' clock [0xEB99C4] (written only at 0x8235AD / 0x8235BA) and the start table (only at 0x8233F8) are
    never reset, not at a new land either; `night_lights::Clear` keeps them too, so in a run of several games in one
    process the flames' phase carries over from one to the next.
  - raffclar's tree: 6 creation draws in this order at creation, and the jitter only when dark. Not checked: whether
    his `LandLightTable::GetLandColour` equals [0xFA26A4] bit for bit, and his registry order against "newest first".
  - The list keeps creation order and the jitter (`night_lights::Flicker`) walks it from its end, newest first. The
    jitter runs only while `IsDark` (the mean of the table's base colour < 120), first in the dark branch as in the
    original, before the stamps; while it is light the clocks stand still and nothing is drawn. A new light draws only
    for itself. A light leaves the list (`fn_00823420`, unlink and free, no draws) only in three places: the street
    lantern's destructor (`GStreetLantern::~GStreetLantern` 0x7347E3, when the dead list frees it, so a lantern marked
    for deletion still flickers until then; `GStreetLantern::ToBeDeleted` 0x734AB0 leaves the light alone), the heart's
    `ToBeDeleted` (0x464C94, the temple's lamps) and `ClearAllStuff` (0x82AEDF). There is no periodic check. openblack
    forgets a street lantern's light when its entity is destroyed (`on_destroy<StreetLantern>`, VillageLightSystem); a
    Norse Gate's lamps keep theirs until the map is cleared, as the gate's `ToBeDeleted` (0x4225A0) leaves them. The
    sprites are made by the next update (no draws). Tests: `test_village_light_system`.
  - The temple's lanterns (`CitadelHeart::DrawLantern` 0x4665C0..0x466727, the last call of `CitadelHeart::DrawNow`
    0x46734B): the heart's `CitadelEntrance` keeps two lamp objects (+0x58, +0x5C) and their lights (+0x60, +0x64).
    Every drawn world frame, for every heart newest first (fn_00467360 from `GLandscape::Draw` 0x5E4EA0, after the night
    lights' stamp at 0x5E488E and before the object list; never inside the temple, whatever the camera sees), the
    heart's draw percent (its 3D object's +0x9C) is tested: at most 0.07f ([0x8C9D30] = 0x3D8F5C29, strictly greater
    passes, 0x4665D4..0x4665E8) nothing is made or drawn. On the first frame that passes with no lamp 0 yet, for lamp 0
    then lamp 1: a static object with MeshPack[398] (MSH_O_TOWNLIGHT), placed by its point in the heart's frame, x =
    −8.85, z = −10.34 then x = −2.3, z = −13.4 (0x46665C..0x46666C, 0x466642..0x466652): per component ((z·forward) +
    (x·right)) + position, floats in that order (0x466674..0x4666C1), with no turn and scale 1 (`SetPosition(P, 0,
    1.0)`), then its light `fn_00823240(point, 0)`, a town light with its 6 draws, before the next lamp is made. Every
    frame that passes, both lamps are drawn (AddDrawing 0x466701..0x46671D) with no collision, so the cursor never finds
    them; they make no sound. The heart's deletion (`CitadelHeart::ToBeDeleted` 0x464C50..0x464CA9) releases each lamp
    and unlinks its light at once (`fn_00823420`, no draw). the lamps are `NotDrawn` while the temple is drawn 7 % built
    or less. While the temple is destroyed, `CitadelHeart::DrawNow` 0x467262..0x46727E returns before the heart's
    own draw, its fire effect, its collision and the three calls that end it (DrawLantern among them) once the heart's
    destruction flag +0xB8 is set and its clock +0xBC is above [0xC5E254] (21.5 + 0.3 as floats, 21.8f, written at
    start-up by 0x465B30): from then on the lamps are neither made nor drawn, but they stay until the heart's
    deletion. Ours: `temple_destruction::HeartDrawn`, read by `citadel_lanterns::Draw` (the lamps only; the heart's
    own draw, `TempleDestructionSystem` through the same `HeartDrawn`, test `test_temple_destruction`). The hearts are taken in the order of their storage, newest first while none has been deleted. The new
    lights' sprites are made by the next night-lights update, so on the frame the lamps are made their flames show one
    frame later than the original's, while it is dark. The cursor's pick skips every lamp that is only a model with a
    light (a temple's and a Norse gate's, which the gate's draw also draws with no collision, 0x422855). Tests:
    `test_citadel_lanterns`; scenario `examples.temple_lanterns`.
  - Darkness in the verification runs (base colour from `palette.raw`, alignment 0): Land 1's script sets
    `SET_GAME_TIME(15.4)` then `SET_GAME_TIME(4.59)` at about turn 9; 4.59 h is sky type ≈ 1.73, base mean ≈ 81, so
    the fidelity, cycle (first Land 1) and hand-demo runs are light (12 h, then 15.4 h) up to about turn 9 and dark
    from there to the end (the lanterns' loop plays from turn 9 to turn 140 in a traced run). A new game opens at 12 h
    (`DayNightClock::Reset`, sky type 0, base mean ≥ 182 for any alignment, ≥ 158 under a full overcast); a land
    change keeps the clock (2026-10-22), so Land 2 and the second Land 1 of the cycle run open at the time, cycle and
    switch the land before left. The pinned `OPENBLACK_TIME_OF_DAY` runs are light at 12 (vis day,
    temple shots) and dark at 22 (vis night: sky type 2, base mean 68).
  - Additive sprites: two `S_Fire` flames (frames 0..31 backwards in 700 ms with an integer global clock, which only
    runs with alpha ≠ 0; flame i starts at the global table 0xC383BC, which every new light rewrites with
    ftol(Random(0, 31)) (fn_00823240 0x8233E4..0x8233F8), so they all run in phase with the last one created;
    fn_00823570, `frame_anim::LanternCell`; half-size 1 ± 0.1) and a halo
    `smoke` cell 56 ((flags & ~7) | 0x38, 0x8233BC..0x8233EB; half-size 3 ± 0.1). Colour 0xF38421 with alpha trunc(I/2).
  - Approximate: the sprite shader only uses the alpha mask, without the texture's RGB.
  - The additive quad of the hand over the water (±60, `atmos.raw`) is done (`src/Graphics/HandWaterGlow.*`, see
    [sky: sun, moon and clouds (original)](sky.md#sky-sun-moon-and-clouds-original)). The two lights of the Norse gate
    (MSH_O_TOWNLIGHT at (±15, 30, 0) of the gate) are done (see [the land script reader](../scripts/map-script-reader.md)).
- **Street lantern sound** (done; `src/Audio/Services/LanternSounds.*`, one `audio::tags` per lantern,
  [SoundTag](../audio/world-sounds.md#soundtag)):
  - **Creation**: `GStreetLantern::CallVirtualFunctionsForCreation` 0x734810 creates a `SoundTag` (`fn_0071E8C0`,
    +0x60) with offset (0, `Object::GetHeight` 0x638120, 0), sample 0x93, 3D, mode 2, loops −1, bank 1
    (`Audio\SFX\Game\InGame.sad`) and then `SetActive([0xDA0A10])`. **Both lantern classes** have it (it does not check
    +0x58); the object with the UNAVAILABLE mark (+0xA & 1) does not have it.
  - **Sample**: 0x93 = LH_SAMPLE_G_LANTERN_01 (147) = `G_Lantern_01.wav`, 22050 Hz, ~4.1 s, priority 200, override
    marks 0x7C0: loops −1, minDist 3, maxDist 5, scale 4, mode 2. No volume or pitch mark, so
    volume 127 (gain 1) and pitch 100 % with the usual ±15 % deviation. Mode 2 = one channel per lantern: if it is already
    playing, it does nothing.
  - **Day/night switch**: `fn_007349E0(on)` stores [0xDA0A10] and does `SoundTag::SetActive` on every lantern. Only
    caller `fn_005E5830` (`GLandscape::Draw`, every frame): 1 when the mean of the light table's base is
    **< 120** (the same test as the hand and the village lights, with no time test), 0 otherwise. `SetActive(0)` calls
    `StopPlayingSoundEffect`: abrupt cut.
  - **Per turn**: `SoundTag::ProcessSoundTags` 0x71E5F0 from `GGame::EndTurn`; each active tag calls
    `GAudio::PlaySoundEffect` 0x42A100 with (x, altitude + y, z) plus the offset. 0x429E30 **does not start** the sample
    if the camera is more than maxDist (5 units) from that point; once started, the loop continues wherever the camera goes.
    If the lantern stops being available, `CreateSoundTagForDeadObject` calls `LHSampleReleaseLoop`: the current pass
    finishes and the tag is deleted.
  - The emitter is created with `PlayType::Repeat` and released by setting `PlayType::Once` on it (=
    `LHSampleReleaseLoop`: `AudioManager::Update` removes its `AL_LOOPING` and destroys it when the pass finishes).
  - **Not ported** (unverified): QMixer's attenuation curve for {min 3, max 5, scale 4} (`QSWaveMixSetDistanceMapping`,
    it is in the external DLL). a general audio change could set `AL_REFERENCE_DISTANCE` / `AL_MAX_DISTANCE` per source
    from the `.sad`, but only after deciphering QMixer.
  - Because of the 5 units in the data, in normal play it can only be heard with the camera almost on top of the lantern.
- **Fireflies** (done; `src/ECS/FireFlies.*`):
  - When: with visual time > 12 and sky type > 1 up to 50 appear, each on a coin toss in a random tree or at the
    first rock (a bonfire or a fragment counting as one) from a random place in the fixed objects' list
    (`ecs::FireFlyRockFrom`, `ecs::IsFireFlyPerch`; tests `FireFliesTest.*` in `test/test_fire_fly_perch.cpp`). Each
    turn one flies to the closest house or lantern less than 300 m away, at +(height + 2). In the morning
    each turn one goes back to a tree or a rock.
  - Flight: it lasts distance / (3·speed), at least 0.5 s (0x52A9C4..0x52A9F8: `fcom` 0.5, and 0.5 when not above it), with smoothstep easing (`fire_flies::FlightSeconds`).
  - Drawing: orbit of radius 8 plus a jitter of radius 1, both times the drift (fn_0052ABC0, its switch at 0x52AE88):
    0 asleep, 1 hovering, p × 5.0 [0x8DAD58] over the first fifth (p < 0.2 [0x8AB244]) of a flight out, and
    1 − (p − 0.8) × [0x8DAD5C] over the last fifth (p > 0.8 [0x8C4A04]) of a flight home, where [0x8DAD5C] is
    0x40A00001, a hair over 5 (0x52ACBE): openblack used 5.0 until `fire_flies::DriftAmplitude` took the original's
    float (tests `FireFlies.*` in `test/test_fire_fly_drift.cpp`). Sprite 37 of `S_SpriteSheet3`, half-size 0.3, alpha 190, which
    fades out between 100 and 300 m. The sprite's texture is `.\data\textures\S_SpriteSheet3.raw` itself (the string at
    0xBE9DE4, loaded once into [0xCCFC60] by `FireFly::CallVirtualFunctionsForCreation` 0x52A557..0x52A575, render mode
    0xD), its colours with the sheet's alpha file, and `FireFly::Draw` sets the colour 0xFFFFFF with that alpha
    (0x52AB6B..0x52AB7F; the alpha at 0x52AB11..0x52AB66: not drawn past a squared distance of 90000, 190 up to 10000, else `ftol((1 − (d² − 10000) / (90000 − 10000)) · 190)`, `fire_flies::Alpha`): the firefly is the cell's own colour, a light blue (its alpha-weighted bytes 31, 111, 213; 93,
    215, 241 at its brightest).
  - Waking at nightfall (fn_0052B350): the firefly comes out only if a tree or rock of its cell has map coordinates
    equal to its own (`MapCoords::operator==` 0x605660 at 0x52B3A4: x, z and the altitude, `fcompp` at 0x605678);
    openblack compared x and z only until `WakeOne` used the whole compare. A firefly whose spot has lost its tree is
    removed at nightfall (test `FireFlies.WithNoTreeOrRockToGoHomeToItRests15MetresAsideOnTheGroundAndGoesAtNightfall`).
  - Caught in the hand (fn_0052B600 → fn_0052B5A0): the object's own map coordinates are looked up in its cell for a
    FireFly whose map coordinates equal them, x, z and the altitude (0x52B5BD); openblack compared the firefly's x and
    z with the object's point within a fixed-point unit until `TakeFireFlyAt` took the object's map coordinates.
  - The search for a light or a perch walks the spiral of cells, each searched on a coin toss, keeping the nearest
    found and ending a cell on a one-in-three draw (`SpiralSearch` in `src/ECS/FireFlies.cpp`; tests
    `FireFlies.ComingOutItHoversTwoMetresAboveTheNearestLightItFinds`, `FireFlies.GoingHomeItHidesInTheNearestTreeOrRock`).
- **Sound**: `GSoundMap` blends the day and night ambience according to max(0, sky type − 1) (done, see
  [sounds](../audio/world-sounds.md#sounds), "Ambience"); the
  sounds of the houses are chosen by the sky type (pending).
- Gameplay only, left out for now: lions and wolves go to their den (22 / 23 h), children leave the
  crèche, the food desire and the creature's desires.

### The computer's clock in the scripts

Five natives read the player's computer clock, each from `localtime(time())` (the CRT's `_time` 0x7C79FD and
`_localtime` 0x7C789D), and push one float. In a multiplayer game each first reports "This is not multiplayer friendly"
(`ScriptErrorMessage`) and then answers all the same. None of them is called by the shipped story script.

| Native | exe | Pushed |
|---|---|---|
| GET_REAL_TIME 114 | `GScript::GetRealTime` 0x710F20 | `(float(min·60 + sec) · 1/3600) + hour`: the int converted (fild), times the float 1/3600 (0x3991A2B4 at 0x957238), plus the hour (fiadd), each step rounded to a float |
| GET_REAL_DAY 115 | `GetRealDay` 0x710FA0 | `tm_mday`, 1..31 |
| GET_REAL_DAY 116 | `GetRealWeekDay` 0x711000 | `tm_wday + 1.0`, 1 (Sunday)..7 |
| GET_REAL_MONTH 117 | `GetRealMonth` 0x711070 | `tm_mon + 1.0` |
| GET_REAL_YEAR 118 | `GetRealYear` 0x7110E0 | `tm_year + 1900.0` (0x95723C) |

The rounding was settled with the exe oracle (`_time`, `_localtime` and `ScriptDLL::PUSH` stubbed): for every one of
the 86400 seconds of a day the pushed bits are the float-by-float result above; working in double instead differs in
2737 of them (the first at 1:01:14: 0x3F82A190, not 0x3F82A191).

## openblack

Status: **done** for the clock (`DayNightClock`), the night lights (windows, the hand light, the street lanterns and
the temple's lanterns), the lanterns' sound and the fireflies; of the gameplay effects of the night only the creature's
tiredness and sleep are ported ([Pending](#pending)). Progress: [sky](../project/progress/sky), [weather](../project/progress/weather). The weather and the climate
are in [Weather](weather.md#weather-and-climate).

- Land 1's intro runs its clock commands (`SET_GAME_TIME(15.4)`, then `SET_GAME_TIME(4.59)` and `GAME_TIME_ON_OFF(1)`
  in `CreatureDevSeeHome`, [what the script does with the answer](../scripts/land-change-and-saving.md#what-the-script-does-with-the-answer)); the World menu and the hooks below
  move the clock at any other time.
- The computer's clock in the scripts: `script::real_time` (`src/ScriptHeaders/ScriptRealTime.h`), used by the five
  natives; test `test_script_real_time` (the oracle's bits). openblack has no multiplayer game, so there is no warning.
  The Moon debug window shows what the scripts would read now.
- **VM fix**: unimplemented CHL functions left their arguments on the stack, and those of the following ones were
  shifted. For example, `SET_PROPERTY` left a 1.0 that `SET_GAME_TIME` then read. Now `LHVM::Opcode05Sys`
  removes `stackIn` values when the function has not popped any, below whatever it has pushed.

### Notes on the original's sections

What openblack does at each point of [The original](#the-original), by section:

- **Clock**
  - openblack passes this time to the sun and the moon (`SkySystem::SetTime` / `GetTime`).
  - openblack calls `DayNightClock::Reset` in `Game::PrepareNewLand`, before the island script, only at a new game
    (`land_load::Resets::newGame`: the game's first land, a playground's start, the testbed); a land change keeps the
    clock (2026-10-22; before, every land opened at noon). The original's reset comes with the island script's
    `LOAD_LANDSCAPE`, before any of its time commands.
- **Night lights**
  - openblack: `night_lights::WindowColour` (`src/3D/NightLights.*`; test
    `VillageLights.WindowsOfAHomeWithPeopleInLightUpAtNight`), which gives the colour +0x54 0xFFgggggg or 0
    with the windows off, and goes in the z of the fifth column of the instance (`argb_colour::PackInstanceWindow`,
    [the object colour fields in the
    instance](../rendering/model-lighting.md#the-object-colour-fields-in-the-instance)).
  - openblack writes the result straight into the luminosity (byte 3) of this frame's land cells, with no minimum
    against the loaded luminosity (`VillageLightSystem::Update`, `land_light::SetLuminosity`)
  - openblack: the hand's Transform, skipped only while the hand has `NotDrawn` (it used to skip a hand exactly at
    (0, 0, 0) and stamp a hidden one).
  - openblack: `HandLight::GetStrength`, both overloads (the packed colour and the colour as 0..1 fractions, each
    channel rounded to its byte)
  - openblack's are the same.
  - openblack (as the original): the light is made with its lantern, `VillageLightSystemInterface::AddLight` →
    `night_lights::MakeLight` (the 6 draws in the original's order, into the shared start table), from
    `StreetLanternArchetype::Create` (every street lantern: map script, CHL, a town's) and from the Norse gate's two
    lamps in `AnimatedStaticArchetype::Create` (−15 then +15).
  - openblack (as the original, `AnimatedStatic` draw 0x422770: the gate's AddForDrawing 0x422821, then, only when
    its on-screen flag is set, the lamps' draws 0x422855): the Norse gate's two lamp models are drawn only on a frame
    whose object draw list drew the gate and found it on the screen. Each lamp keeps its gate
    (`components::GateLamp`), the list records the draws that found their object on the screen
    (`ObjectDrawListSystemInterface::OnScreenThisFrame`) and `draw_gate::FillFrame` leaves out a lamp whose gate is not among
    them (`draw_gate::GateLampHidden`, reason `GateNotOnScreen`). The lamps' lights, flames and glow are not tied to the
    gate, as the original's are not. Before it draws them the gate's draw copies its own colour (+0x4C, from `GetColorAndSpecular`
    0x4227D6 at the gate's position) and its fogged specular (+0x50, `GetFogValue` 0x4227E0) into each lamp
    (0x42283B..0x42284E): openblack lights and hazes a lamp at its gate's place and gives it that pair as its set colour
    (`draw_gate::LampLightOrigin`, `RenderingSystem`; test `DrawGate.AGatesLampIsLitAtItsGatesPlace`; readout: the
    Land Island window's "Gate lamps' light"). Test: `DrawGate.AGatesLampIsHiddenUnlessItsGatesDrawFoundTheGateOnTheScreen`;
    debug readout: the Land Island window's "Object draw list", its "Left out of the last frame" table (with its
    switch "Draw only what the list draws").
  - openblack: `worship::citadel_lanterns` (`src/Worship/CitadelLanterns.*`, lamps on `components::CitadelEntrance`,
    marked `CitadelLamp`), from `Renderer::PreDraw` after the clouds (outside the temple), and from the heart's
    deletion with `VillageLightSystemInterface::RemoveLight`
  - openblack: `audio::lantern_sounds::SetOn` from `night_lights::Update` (the same mean < 120), `ProcessTurn` in
    the turn block of `Game.cpp` and `Clear` together with `night_lights::Clear` on map load (before
    `Registry::Reset`, which would delete the emitter entities without releasing the OpenAL source and the loop
    would keep playing).
  - openblack sets each source's {min, max, scale} from the `.sad` as in all 3D sounds: OpenAL's inverse distance
    clamped, reference = min, rolloff = scale, muted beyond max (`device::SetSourceDistance`,
    `src/Audio/Device/Device.cpp`; [world sounds](../audio/world-sounds.md)).
  - openblack used to draw it from the alpha file alone, white.
  - **Home of the script villagers**: CREATE_VILLAGER / _POS give the villager the abode whose door is in the
    script position's map cell while it has room, else an abode with room in that abode's town, else the town, else
    the vagrants, as the original (`villager_commands::FindAbodeAt`, `CreateVillagerAtAbode` in
    `src/LHScriptX/FeatureScriptCommands.cpp`, 1d7c9a5f0; [Villagers from the map
    script](../scripts/map-script-reader.md#villagers-from-the-map-script)).
  - Checked in game (`OPENBLACK_LANTERN_SOUND_TRACE=1`): with `OPENBLACK_TIME_OF_DAY=22` and the camera at the Land1
    village lantern `2493,2534` it starts at 1.7 units and does not repeat (mode 2); at the field lantern
    `1365,2571` it starts the same way (both classes); at noon it never starts; when changing map
    (`OPENBLACK_TEST_MAP_CYCLE`) the cut of `Clear` does not leave the loop loose. The night → day transition
    **could not be triggered in game** at the time: early in Land 1 the clock is stopped at 7.3 (above the darkness
    threshold, which is between 7.2 and 7.3), and `OPENBLACK_TIME_OF_DAY` pins it without a transition; the abrupt cut
    was verified through the same code path (`Clear`). Correction (2026-10-22, the Land 1 play-through logs): the
    script does not set 7.3 every turn. `SET_GAME_TIME(7.3)` comes once with the clock off, then `MOVE_GAME_TIME(12,
    460)`, `SET_GAME_TIME(12)` and `SET_GAME_TIME(15.4)` with the clock on, so a night to day transition can be seen
    by playing on past the temple; still to do in a run.

### Code map

The source files of this page and the symbols their notes describe:

- `src/3D/NightLights.h`: `WindowColour`.

### Test hooks

- `OPENBLACK_TIME_OF_DAY=<script hour>` fixes the time on every turn (`SetScriptTime`, with the sky jump).
- `OPENBLACK_TEST_MOVE_TIME="hour,seconds"` does a `MOVE_GAME_TIME` on load; the script may overwrite it.
- `OPENBLACK_CLOCK_TRACE=1` writes the clock to the log every 50 turns: visual time, script time, sky type computed
  now, the frame's one (`sky_type::Frame()`) and the dome's one (`Dome().Built()`).
- The World menu shows the script time (slider), the visual time, the frame's sky type and the dome's.
- `OPENBLACK_LANTERN_SOUND_TRACE=1`, the tests `test_hand_light` and `test_citadel_lanterns`, and the scenarios
  `hand.night_light` and `examples.temple_lanterns` (see [Night lights](#night-lights)). The full list:
  [Debug environment variables](../project/debugging.md#debug-environment-variables).

## Pending

- The sounds of the houses chosen by the sky type (see [Night lights](#night-lights), "Sound").
- The gameplay of the night (the lions' and wolves' dens, the crèche, the food desire). The creature's night is
  ported: its tiredness-from-night source and its sleep read `DayNightClock::IsVisualNight`
  (`creature_physiology::SourceValue` and `SleepTurn`, `src/Creature/CreaturePhysiology.cpp`; tests
  `CreatureMindSystemTest.TheNightIsTheDayNightClocks`, `CreaturePhysiologySystemTest.TheNightKeepsARestedCreatureAsleep`);
  whether its other desires read the night is not checked.
- The lanterns' sound: that OpenAL's clamped inverse distance (`device::SetSourceDistance`) is QMixer's attenuation
  curve for {min 3, max 5, scale 4} is not checked against QMixer (see [Night lights](#night-lights), "Street lantern
  sound").

The lanterns' darkness gate, creation draws, redraw, the Norse gate's lamps drawn only with their gate and lit and
hazed at their gate's place (2026-10-22, test `DrawGate.AGatesLampIsLitAtItsGatesPlace`) are as the original.
