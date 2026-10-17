# Sea life and fixed scenery

## Summary

The living things and fixed pieces of each land's sea and coast: the sharks of Land 1 that follow their tracks, the
fish puzzle with its net, bait and shoals, the missionaries' boat of the intro, the waterfall of Land 3 and the beached
ark and dinosaur of Land 4.

openblack ports the sharks (their tracks checked segment by segment against the original), the fish puzzle and its
script side, the boat and the fixed scenery, with the differences listed in [Pending](#pending).

## The original

### The sharks of Land 1

`Shark1Pos = CREATE(Marker, CONVERT_CAMERA_FOCUS(221))`, `Shark2Pos = ...(230)`; `Shark1 = CREATE(Whale, 5000,
GET_POSITION(Shark1Pos))`, `Shark2` likewise; `WALK_PATH(Shark1, 1, 21, 0, 1)`, `WALK_PATH(Shark2, 1, 20, 0, 1)`.
Cam221/Cam230 are the start cameras of those tracks: their focus is the first focus point of Track21/Track20
((1312.57, 0.74, 2010.79) and (1318.92, 0.74, 2000.23)). The foci are at y ≈ 0.4..0.9 (ignored: relative y 0), at
10 units/s, towards (1427, 2056) and (1430, 2042).

**Verified**: the openblack trace (`OPENBLACK_WALK_PATH_TRACE=1`) gives the same segments, t and points as the
Unicorn emulation of 0x844570/0x843ED0/0x844280/0x8439C0 with the FPU at 24 bits over the
71 compared samples of each track, and the backwards traversal 0.25→0.75 of Track21 ends at the same sample.
With the FPU at 64 bits t changes by up to 4·10⁻⁴ (a few millimetres). The original runs at 24 bits, and sets it itself:
`fn_007DEE00` does `fninit` and clears bits 8–9 (PC = 00, single precision) of the control word (`and 0xFCFF`,
0x7DEE0D); it is called at startup (`pc_main` 0x641C6F via `fn_007DEDD0`), in every `GGame::EndTurn` (0x54E964, 0x54E974,
0x54E984), in `Process3dEngine` (0x54E426, 0x54E4D1), when opening the landscape and when loading. The CRT's
`__setdefaultprecision` (53 bits) only runs before all of that.

Hook: `OPENBLACK_TEST_SHARK=1` creates the two `FollowUs` sharks and their `WALK_PATH`;
`OPENBLACK_TEST_SHARK="track,camera[,forward[,from[,to]]]"` a single one at that camera's focus.

`RUN_CAMERA_PATH` (119, 0x6ED7F0) uses the same track from the script camera (`CameraModeScript`, `fn_00461A80`):
the reader (`LoadCameraTrack`, `CameraWayRunner`) is what the script camera's `RunPath` uses
([The script mode](../camera/script-camera.md#the-script-mode)).

### Sharks

- **Faithful** (class `Whale`, Whale.cpp; `src/ECS/Sharks.{h,cpp}`,
  `ECS/Archetypes/SharkArchetype.*`, `ECS/Components/Shark.h`). `CREATE(Whale = 26, 5000, pos)` → `Whale::Create`
  0x774C50 (`GMobileObjectInfo[24]`; mesh 370 from info.dat is not used) → `CallVirtualFunctionsForCreation` 0x774CA0:
  **scale ×2**, mesh 31 `MSH_SHARK_BONED`, clip 129 `ANM_SHARK_BONED_SWIM` looping, +0x6C (heading) = 0. No AI: per
  turn `Whale::Process` 0x775280 only copies Pos into +0x2C (it is moved by the script's `WALK_PATH` along the focus of the
  tracks `Track21`/`Track20` of `camera.edt`: [The script camera and camera tracks](../camera/script-camera.md), `ECS/MobileWalkPaths.*`). Per frame `fn_00774E30` (from `GLandscape::Draw` 0x5E4B26, before the sea):
  clip time += ms; heading = `LH3DMath::GetYAngle` 0x841290 = atan2(dz, dx) in [0, 2π) of Pos − +0x2C (the
  previous one if it did not move); position interpolated with the turn fraction (each end at GetAltitude + relY);
  bottom part in 0xFF303070; wake `fn_00775170`; the top part is `Whale::Draw` 0x774E10 (table[255], default
  plane). No shadow, no reflection, cannot be picked up.
  Wake: a **global** timer 0xDCB984 for all sharks (each one adds the frame's ms to it): if
  it exceeds 50, `%= 50` and a ring at (p.x, 0, p.z), growth 10, aspect 0.5, rate 0.5, cell 0x31, 0x90FFFFFF,
  angle = heading, without writing +0x1C. p = the position of `EBone.matrices[0]` times the matrix of bone `EBone.bones[0]`
  (0x77507D, `fn_007FAE60`; in mesh 31 bone 0 and (−0.079, −0.003, 0.304)): `L3DMesh::GetEBonePoint0`.
  The timer adds integer ms (`g_game+0x250540`, 0x775265: the difference between two integer readings of the game
  clock, `GGame::Loop` 0x54D374); Space of `[0xC37D9C]`: the one just filled by the shark's own `DrawCutByPlane`
  (`fn_00811C70` → `fn_00839980`/`fn_00839BC0` blend the clip's frames and `fn_00839F10` multiplies each bone
  by its parent and the root by the object's world matrix, +0x14): **world space**, no camera. Hook `OPENBLACK_TEST_SHARK=1` (the two sharks of
  `FollowUs` with their `WALK_PATH`).
  At noon: light fin and tail above the water, dark blue body through the sea,
  white rings coming off the back towards the tail.
- `Whale` is 0x74 bytes, vtable 0x8FEBEC, kept in the list g_game +0x205D04. The ctor copies Pos into +0x2C (`MobileObject::MobileObject` 0x606E40); +0x6C = 0 after the creation.
- `fn_00774E30`: the heading is stored with fstp dword (0x774EEF..0x774EF4); the interpolated position starts at 0x774EFB; the wake uses the first EBone point of its mesh (0x775049..0x7750A3).

- LH3DMath::GetYAngle 0x841290 (0x774EEF), stored with fstp dword (0x774EF4)
- 0x774EFB..: between the start and the end of the turn, each end at GetAltitude + relY
- The wake: the translation of [0xC37D9C][bones[0]] x EBone matrices[0] (0x77507D-0x7750A3, fn_007FAE60). The array is the one the shark's own DrawCutByPlane (fn_00811C70) just filled: fn_00839980 / fn_00839BC0 blend the clip's frames and fn_00839F10 multiplies each bone by its parent, the root by the object's world matrix (+0x14: SetPosition's position, heading and scale), so it is world space; no camera matrix is involved. Here: model x this frame's pose (model space bones) x the point, the same product.
- The ctor copies Pos into +0x2C (MobileObject::MobileObject 0x606E40); +0x6C = 0 after the creation

### Fish puzzle

Land 4, `PuzzleGame` 14: 30 fish have to be in the net at the same time for 0.5 s. **Faithful**; the fisherman and the
scroll are missing. The normal fish shoals (fish farms: drawing, scaring, fishing, stock) are in
[Fish-farm fish shoals](sea-coast-and-rivers.md#fish-farm-fish-shoals); their creation from the script, in
[Fish farms](../scripts/map-script-reader.md#fish-farms).

#### The net, the bait and the shoals

- **Faithful** (Land 4, `PuzzleGame` 14, `fn_006D7480` branch 0x6D7FCD): bait `{pos, radius 11, need 30,
  500 ms}` + net `FishPlot` (ctor 0x829A30: `Data\MISC\Fishplot.l3d`, a static float with dynamic lighting, drawn
  at 7 points `pos + 11·(cos(i·2π/7), 0, sin(i·2π/7))`, phase 0, closure 1) + 2 shoals of 15 (range 7) at `pos + (±12, 0,
  12)` with `+0x5C = bait`. `fn_00824B90` every frame: `inside = 0` on all baits; per shoal, `n =
  fn_00824DA0` (0 and the shoal no longer moves or is drawn if the bait is `done`; otherwise, the visible fish with
  `dx² + dz² < r²` after moving, 0x824AB8), and if it has a bait: the net under the water (`fn_00829BC0`), `inside += n` and, with
  `inside ≥ 30` and not `done`, `timer += g_game_time_inc`; at 500 ms `done = 1`, the net closes and **each of the 15
  fish of each shoal of that bait** releases a ring (its position, growth 2, rate 1, +0x24/+0x28 = 1, cell
  0x30, white; +0x1C unwritten). At the end, a bait with `inside < 30` resets the timer to 0 (0x824D2E): the 30
  have to be inside at the same time for 0.5 s in a row. `fn_00829BC0` (dt = ms·0.001): if it is closing and `k ≠ 0`, `k =
  max(k − 2·dt, 0)`, radius `1 + 10·k` and the points are repositioned; `phase += 2·dt`; `SetClipPlane(0, −1, 0, 0)`, per
  float `SetPosition((x, y + 0.5·cos(i² + phase), z), 0, 1)` + `DrawCutByPlane` (vt+0x11C, fn_0080C050), and
  `SetClipPlane(0, 1, 0, 0)`. Since it is called **once per shoal**, with the two shoals the phase advances 4/s and the net
  closes in 0.25 s, not in 0.5 (this is replicated). Above the water `fn_00829B50` (from `fn_00824D60`, 0x5E6296): the same
  floats with the default plane. The puzzle shoals are not fished (`fn_00824B10` skips `+0x5C ≠ 0`).
  - The net is drawn once per frame (the original draws it twice, once per shoal, with the
    phase of each call: it is not noticeable). The colour of the cut is the default `+0x4C`, 0xFFFFFFFF (ctor of
    `LH3DMeshedObject` 0x8164F7): the `FishPlot` ctor does not call `SetColour` (vt+0x2C); `UseDynamicLighting`
    (vt+0x58, `fn_008168C0`) only sets bit 0x20 of +4, and the only ones that write +0x4C are the drawings of vt+0x100,
    +0x110, +0x130 and +0x154, which the net does not go through (`fn_00829B50`/`fn_00829BC0` only call vt+0x20 and
    vt+0x11C). The script side (`CREATE_WITH_ANGLE_AND_SCALE` 32/14 and `PLAYED`) is done: see
    [below](#the-script-side). The fisherman and the scroll are missing.
    Hook `OPENBLACK_TEST_FISH_PUZZLE` ([Test hooks](#test-hooks)).
- The net mesh is loaded once with `LH3DMesh::CreateFromHD("Data\MISC\Fishplot.l3d", 0)` (0x829A63). The 7 floats are 2π/7 apart ([0x9A3964] = 0.897598), at the centre's y (the ctor 0x829A30 and fn_00829BC0 0x829C1F..).
- The shoals' centres are written at 0x6D8089..0x6D8232: +12 in x and z for the first, then x − 24.
- The shoal fish (fn_00824740, also used by the fish farms) draw their randoms in this order:
  1. the size (0x82475E, at least 1e-4 [0x8BF518], 0x824763..0x824774);
  2. the sprite's cell, ftol(r) & 0x3F into +0x28 (0x824797);
  3. the sprite's angle +0x14 (0x8247DB), later overwritten with the heading at every fn_008248E0 (0x824AA1);
  4. the animation phase +0x1C (0x8247F7);
  5. z, y, x in that order (0x824809, 0x824819, 0x82482C), then the centre is added (0x824846..0x824877);
  6. the heading (0x82488D, 0x82489F);
  7. the turn rate ((r + 1) × speed) × [0x900C90] (0x3F20D97C, about π/5; 0x8248B1..0x8248CE).

  The flee time +0x20 starts at 0 (0x8248BC).
- Each frame (`fn_00824B90`): nothing when d² > 90000 (0x824E16). When d² > 40000 the alpha byte is the low byte of ftol((1 − (d² − 40000) × 0.0001) × 255) (0x824E31..0x824E6F), which reaches 0 at d² = 50000 and then wraps.
- The shoal timer (0x824E73..0x824F2C, before the splash): timer −= g_game_time_inc × 0.001 (whole milliseconds). Below 0 the shoal gets a new target: CRT Random(−[+0x58], [+0x58]) for z (0x824EA3), then for x (0x824EB9), y the centre's, and timer = 0.5 × the distance from the previous target (0x824F11).
- On a splash the shoal darts 2 units in a random direction (CRT Random(0, 2π), 0x824F41). The fish within 8 units of the splash flee from it for 2 s, and only then is the shoal's timer set to 2 (0x82503B).
- A bait counts the fish inside its radius in (x, z) at 0x824AB8..0x824AF5.

- fn_00824740 for each of the 15 fish: size 0.8..1.2, within +-5 (x, z) and -1..0 (y) of the centre, a random heading, speed 0.5..1.5 and turn rate speed x [0x900C90] (bit_cast 0x3F20D97C, about pi / 5) x (1 +- 0.1); the target is the centre
- LH3DMesh::CreateFromHD("Data\MISC\Fishplot.l3d", 0) (0x829A63), once into the mesh manager as "misc/Fishplot"
- The 7 points round the centre (the ctor 0x829A30 and fn_00829BC0 0x829C1F..): y is the centre's

#### The script side

- `CREATE_WITH_ANGLE_AND_SCALE(32, 14, PuzzlePos, …)` → `fn_006D6680` (puzzlegame.cpp, 0x588 bytes, list
  g_game+0x205D14). It does nothing until `GlobalGameLists::Process` 0x591449 calls `fn_006D7480` every turn:
  0x6D74C3 nothing if +0x3C; if the "played" test (`fn_006D66E0`) gives 1, +0x3C = 1 and out; otherwise, the type's step. The
  type 14 (0x6D7FCD) creates the first time the bait at `ConvertToLHPoint(+0x14)` 0x6041C0 (altitude + the script's y), its net
  and the two shoals (see [above](#the-net-the-bait-and-the-shoals)).
- `PLAYED` (64) = `GScript::Played` 0x6F9DC0: creature → its plan (0x6F9DF4); Living → `IsScriptAnimationComplete` or its
  state; weather → +0x78 == 0; **PuzzleGame** (vt+0x498) → `fn_006D66E0`, switch 0x6D6C94 on type − 1: type 14
  (0x6D6A32) gives 0 without a bait, 1 if `bait+0x18` (done) —and sets +0x3C = 1—, 0 otherwise. Lost object or "Thing not living"
  → 1. The `done` is set by `fn_00824B90` when the 30 fish have been inside radius 11 for 500 ms (the shoals only count
  when less than 300 from the camera, as they are drawn).
- `PuzzleGame::ToBeDeleted` 0x6D6FF0: deletes the bait with its `FishPlot` (fn_00829B20) and the two shoals (out of the list
  0xEB99F4, with their 15 fish).

- The CHL PLAYED of a PuzzleGame (GScript::Played 0x6F9DC0 0x6F9F0B -> fn_006D66E0, switch 0x6D6C94 on type - 1): type 14 (0x6D6A32) is 1 once its bait is done (bait +0x18, set when the 30 fish stayed 500 ms in the net), and that sets +0x3C too; 0 while the bait is not made yet. The other types are not ported: 0.

### The missionaries' boat (PetitNavire)

`PLAY_JC_SPECIAL(6)` in Land 1 (`TheMissionaries`): the launch of the missionaries' ark and its voyage. **Faithful**,
with the differences stated at the end. The reflection of the hull is drawn like the other reflections
([Object reflections and hand shadow on objects](../rendering/shadows-and-reflections.md#object-reflections-and-hand-shadow-on-objects)); the build
percentage of the ark in the dry dock (`BUILT_PERCENTAGE`) is in
[Build percentage of a Feature](../scripts/map-script-reader.md#build-percentage-of-a-feature).

The GScript function of a CHL opcode is in the table 0xC0DB98 + 0x90·opcode.

- **CHL**: `PLAY_JC_SPECIAL` (326) = `GScript::PlayJCSpecial` 0x708ED0, table 0x708F74 on the integer value (0..15):
  0, 1, 2, 4, 5, 6 → `fn_005DF9C0(n)`; 3 → a 0x2C ScriptGFX object (0x828DB0); 14/15 → [0x9CD384] = 1/0. In
  `fn_005DF9C0` case 6 (0x5DFBF8) is `new PetitNavire(0)` (0x68 bytes). `IS_PLAYING_JC_SPECIAL` (327) 0x708FC0 pops
  a **float** (ftol) and returns 1 except with 13, which gives [0xD19C94] (only set by the hand intro, `fn_005DF640`
  0x5DF807; not ported → 0).
- **A single boat** [0xD19CB4]; the ctor 0x5E1020 frees whatever is there (`fn_005E13C0`: releases Boat1/Boat2 and the objects and
  sets the global to 0). Constants (initialisers `crt_xc_fn_JCMisc_005DFED0/005DFF00`): dry dock [0xD19A08] =
  (1881.0833; 8.1316; 3154.1094), exit at sea [0xD199F8] = (1456.54; 0; 3263.06).
- **Objects**: hull MSH_O_ARK (339) static, `SetPosition(dock, 0, 1)`; sailor MSH_P_NORS_SAILOR (504) animated with
  ANM_P_PUSH_OBJECT (346). Animations +0x08..+0x20 = 346, 332 OVERWORKED1, 235 CROWD_WON_2, 333 OVERWORKED2, 406
  TITANIC, 378 SITTING_SWINGING_LEGS, 359 SCRUBBS. Mode 0: its own dynamic shadow (`fn_008745A0`, holder+4 = 1 and
  ShadowInfo+0xC = **0**: it also falls on objects). Mode 1: cow MSH_A_COW_1 (16) with ANM_A_COW_EAT_2 (36), grain
  pile MSH_S_GRAIN_PILE (533) and `LH3DSprite::Create(5)` with the smoke material [0xEA1ABC] (`smoke.raw`, **mode 6**,
  0x80BC7D), +0x14 = 3.92699 (5π/4), flag 0x40 (flat in XZ), cell 0x31; phases +0x50[i] = i·1200 ms.
- **Hull tracks**: `Data\MISC\boat1.anm` ("beach04", 158 frames, 15833 ms, flags 0x501 = looping) and
  `boat2.anm` ("beach_sailing", 44 frames, 4466 ms, 0x501). A single matrix per frame with **determinant −1**;
  `fn_0083AC70` interpolates the 12 floats (like `LH3DAnim::GetPose`) and composes it with the parent matrix M (`fn_007FAFF0`:
  track·M). Then `RotateY(π/2)` 0x5198F0 and `fn_007FAE60(diag(−1, 1, 1))`, which **multiply in front** (local
  space): hull = mirror·RotY(π/2)·track(t)·M, determinant +1. In glm: `M · track · eulerAngleY(−π/2) · scale(−1, 1, 1)`.
- **PreDraw 0x5DFF20** (from `GLandscape::Draw` 0x5E490F, before the sea), with dt = integer `g_game_time_inc`:
  - Mode 0: +0x34 += dt; +0x64 = +0x24; if `!+0x48 || +0x34 > 3000`, +0x24 += dt. If +0x24 > 15833 − 400 it is deleted,
    does `new PetitNavire(1)` and **returns**: that frame the new boat has no PreDraw and its PostDraw draws it once
    at the dry dock without rotating. Otherwise: M = Translate(dry dock); hull y += `GetAltitude(hull.xz)` − `GetAltitude(dock.xz)`
    (0x5E00EB..0x5E0154); `fn_00874850` (shadow); +0x4C = 0xFF303070 and `DrawUnderWater` (vt+0x118, the reflection);
    `fn_00801C90` gives it back the land light.
  - Mode 1: +0x24 = (+0x24 + dt) % 4466 (looping; otherwise, min(…, dur − 1)); +0x34 += dt; at 60000 ms it is deleted. M =
    RotY(π/4) (0x92B210) at (1456.54; 0; 3263.06) + (−k, 0, −k)·0.005·+0x34, with k = `InverseSquareRoot(2)` 0x841170
    (table 0xEEA394 plus one Newton step = 0.70710659): **5 u/s** towards −x −z, 300 units in total. Hull as
    above (no height correction or shadow), 0xFF303070 and `DrawUnderWater`.
  - **One drawing per frame**: there are no "two parts" or two drawings per frame. 0x5E0100-0x5E0190 (mode 0) and
    0x5E0380-0x5E03EE (mode 1) are **mutually exclusive** branches (0x5E0195: `cmp +0x30, 1`); **both** set up the mirror
    diag(−1, 1, 1) (0x5E00C1-0x5E00D9 and 0x5E0350-0x5E03B6) and the π/2 is the hull's `RotateY`. Each frame there is a
    single `DrawUnderWater`, in 0xFF303070.
- **PostDraw 0x5E03F0** (from `fn_005E5CD0` 0x5E6250, after `fn_00824140`):
  - Mode 0, 2D sounds (`GAudio::PlaySoundEffect` 0x429E30, bank GGlobal+0x3BC = `Scriptsfx.sad`, options +0xBC = 2,
    that is +0x50 = mode 2, owner 0, is3D 0; in openblack through `sample_play`, one of the 16 channels)
    when the hull time crosses the threshold (+0x64 < threshold < +0x24): 100 → 62 `MissionaryBoatCreak_01`, 1500 → 61
    `MissionaryBoatSlide_01`, 3900 → 60 `MissionaryBoatSplash_01`.
  - Mode 0, every 200 ms (+0x38 += ftol(dt); > 200 → action and +0x38 = 0): if 3900 < t < 6500, 2 `SmokyStuff::Create`
    (hull + (r2 − 10, **7**, r1), mode 0, size 7, 0xFEFFFFFF); if 1130 < t ≤ 3900, 2 × (hull + (r2, 0, r1), 0, 5,
    0xFFB88C38, sand), with r1 = Random(−20, 20) and r2 = Random(−2, 2) in that order (corrects the report: the ±20 goes in z and
    the −10 / +7 in x / y). Then the hull (vt+0x100).
  - Mode 0, sailors (the same object drawn 5 times): if t > 850: if +0x48, +0x34 = 0; animation +0x0C + (i % 3)·4
    and +0x48 = 0. Position (dry dock.x + {5.2; 5.3; 5.5; 5; 5}[i], ground, dry dock.z + (i − 2.5)·3 + {1; −0.7; 0; 0.4; −0.2}[i]
    + 10) (0xBF2B1C, 0xBF2B44), angle −π/2, frame ({5, 500, 1500, 455, 2000}[i] + +0x34) % duration (0xBF2B30),
    colour = land light at its place.
  - Mode 1, wake (0x5E0785): phase = (phase + dt) % 6000, t = phase/6000; position = hull·(0, 0, 100t − 15) with
    **y = 0.2**; half width +0xC = 30t + 10, aspect +0x10 = 0.5; alpha = ftol((1 − f)·255) with f = (t − u)/(1 − u)
    if t ≥ u (otherwise, u) and u = [0xD19CB8]: **nobody writes that float** (only reads at 0x5E086E..0x5E0893, no
    initialiser), so u = 0 and f = t; it is only drawn if f > 0.2 (double 0.2 at 0x8C7C68).
  - Mode 1, deck (0x5E08E7..0x5E1015, emulated): the matrix of each one is L·hull (`fn_007FAFF0` and copy to
    obj+0x14), L = RotY(a)·scale + t in the hull's frame; colour = the hull's +0x4C. Cow (a = −1,
    t = (−1.778; 10.78; 1.83), frame +0x34 % dur) and again (a = −0.7, t = (−1.778; 10.78; 3.83), +0x34 + 1255);
    grain (scale 0.26, t = (−5.708; 10.854; 3.199)); the sailor object with MSH_P_NORS_F_A_1 (498) and TITANIC at
    (−0.14; 13.213; −19.657) (+0x34), with 504 and TITANIC at (−0.14; 13.213; −18.9) (+0x34 + 500), SITTING rotated π at
    (−6.25; 11.424; −7.227) (+0x34) and at (−5.25; 11.424; −7.227) (+0x34 + 2345), SCRUBBS rotated π at
    (5.881; 10.741; 0.174) (+0x34).
- **SmokyStuff** (0xCC bytes, list 0xEB99CC): `Create` 0x823C90(pos, mode, size, colour) = 15 mode-6 smoke sprites
  that face the camera; each one at pos + (c, b, a) with a, b, c = Random(−size, size), rotation Random(0, 2π), cell 0x10,
  velocity norm(e, size, d)·Random(0.3; 1)·size (mode 0). `fn_00824140` (0x5E619C, dt = ms·0.001) → `fn_00823F70`:
  life −= dt/3 (mode 0), nothing if life ≤ 0; colour (life·100)<<24 | 0x808080 with the argument's rgb; rotation ±5·life + v.x;
  half width ((1 − life)·2 + 1)·size/2; pos += v·dt; cell (int)(life·15); it is freed with life < 0. `Random` 0x81D180
  = a + (b − a)·rand()/32768 (stdcall). Billboard of `LH3DSprite::Draw` 0x84071D: local x → (cos, −sin) on screen,
  local y → (sin, cos).

### Fixed scenery per land: Land 3 waterfall, Land 4 ark and dinosaur

**Faithful** (done), with the differences stated in the openblack point.

- `GWaterfall` (`CREATE_WATERFALL`, Land command 68, 0x7175D1; ctor 0x734130, vtable 0x8EC14C) is an
  **empty** object: `CallVirtualFunctionsForCreation` 0x7341B0 = `ret 4`, it neither draws nor sounds, and no land uses it.
- `DesignedWaterFall` 0x5E3770 (LandFeature.cpp), every frame from fn_005E5CD0 (0x5E5CDA), according to the land
  number (`SET_LAND_NUMBER`, g_game+0x205A08). When the number changes (last land at 0xBF34E4, 74 at startup) it deletes
  the two objects (0xD1A31C / 0xD1A324), the `ScriptMarker` 0xD1A32C, its `SoundTag` 0xD1A330 (ToBeDeleted: the loop
  finishes its pass) and releases the meshes. They are not game objects: loose `LH3DObject`s (no shadow, no cell).
  A new land deletes what the last one had at 0x5E37AB. The meshes (0xD1A320 / 0xD1A328) stay loaded in the mesh
  manager instead of `LH3DMesh::Release`.
  - **Land 3**: `Data\MISC\waterfall3.l3d` (`LH3DObject::Create(0)`, static) at (3059.23; **0**; 3145.33), angle
    4.7, scale 1, dynamic lighting, +0x10 = 1. Every frame `V = V − 0.5·dt` and `V −= (int)V` (it stays in −1..0) with
    `SetUVOffset(0, V)` (vt+0xE8 fn_007F9B70: +0x68/+0x6C and flag 0x400); dt = `g_game_time_inc`·0.001. Every 0.7 s
    of game time (timer 0xD1A338, reset even if the quota is full) a ring at (3018.8; 0.2; 3130.15):
    growth 30, +0x24 = 1, aspect 1, rate 0.3, cell 0x30, angle 0, colour `0x80 << 24 | table[255].rgb`; the
    drift field +0x1C is not written. `SoundTag::Create(marker, 12 G_WaterFlow, false, mode 2, loops −1, 0,
    3D, InGame, 0)` (0x5E3921).
  - Land 3's addresses: the position 0xD1A2C0 with the angle 0xBF34E8 and the scale 0xBF34EC; the waterfall's texture
    V offset is at 0xD1A334; the ring period 0.7 s is 0xBF34F0; from 0x5E3A32 its point, growth (0xBF34F4), rate
    (0xBF34F8) and alpha (0xBF34FC); the ring is made at 0x5E39BA.
  - **Land 4**: `Data\MISC\arche.l3d` (Create(1)) at (3538, altitude, 2129), angle 8.9728, scale 1.1, +0x10 = 10,
    with the same `G_WaterFlow` SoundTag at (3538, 0, 2129); `Data\MISC\dinosaur.l3d` at (2690, altitude, 2590)
    (≈ 138.7: high ground), angle 1.57, scale 1, +0x10 = 10, with its terrain footprint (fn_0081E9E0, the buildings'
    footprint list). `dinosaur.l3d` is the only one of the three with `ContainsLandscapeFeature` (0x8000);
    `waterfall3.l3d` has a footprint block but without that flag and nobody stamps it.
  - Land 4's addresses: 0xD1A2B0 with 0xBF3500 / 0xBF3504 and 0xD1A2A0 with 0xBF3508 / 0xBF350C; both are morphable
    objects (`Create(ecx = 1)` at 0x5E3B08 / 0x5E3CF8).
  - The marker is created with `MapCoords(LHPoint)` (0x603340 stores y − altitude) and `Get3DSoundPos` 0x56FE20 adds the
    altitude back: the sound comes out at exactly y = 0. `G_WaterFlow` (InGame 12; the bank describes it as "Citadel
    waterfall", but it is only used by this waterfall and the ark): volume 50, loop, min 60, **max 110**, scale 6, mode 2.
- **What scrolls (done)**: nobody reads the 0x400 flag. The static `Draw` 0x80DB30 reads U and V with vt+0x8 /
  vt+0xC (0x80DE86) and leaves them at 0xECA62C / 0xECA630 (0xECA628 = 1 if they are not both 0); the default triangle
  submission (`[0xC386EC]` = `LH3DRender::DrawTriangle` 0x82F810) adds them to the UVs **unless byte +5 of the material
  has bit 0x10** (0x82F8CC). In `waterfall3.l3d` the rock (submesh 0, `Textured`, +5 = 0x14) has it and the water
  (submesh 1, `TexturedChroma`, 0x04) does not: **only the water flows**. (Other paths, `fn_0082FD70` from `fn_00812170` /
  `fn_00817930` and `fn_00884750`, always add it; the static one does not go through them.) In `AllMeshes.g3d` only the
  `S_PHILE*` meshes have that bit, no food pile.
- **Rings at the foot**: they are drawn with `LH3DSprite::Draw` in mode 13 (`fn_0082ECD0`: SRCALPHA/ONE blending, no alpha
  test, ZWRITEENABLE 0 and without touching ZFUNC): with Z test against the land, as in openblack. The rings at the foot end up almost buried: the ground is at 0 at the point and
  rises to 1.7 within 7 units, so with the Z test only a faint glow is visible.

- 0x5E39BA: a ring at the foot once the timer passes 0.7 s (the timer restarts even when the pool is full)
- A morphable object (Create(ecx = 1) 0x5E3B08): UpdateMelting at 0x5E3C55
- A morphable object too (Create(ecx = 1) 0x5E3CF8): UpdateMelting at 0x5E3DBE

### Fish shoal details

- GFishFarmInfo::IsOkToCreateAtPos 0x52D100: MapCoords::IsCoastal (0x52D107), then FindType(0x21 FISH_FARM, 0) (0x52D116) on the point's map cell: a fish farm is only in its own cell (FishFarm::InsertMapObject 0x52CA10, GetNextPos 0x52C940 gives only its +0x14)

### Fish farms

- Entt's on_destroy<FishFarm> (Registry::Destroy from ecs::ToBeDeleted, Remove, Reset): FishFarm::ToBeDeleted 0x52C690 -> DeleteDependancys 0x52C693. The component is still there during the signal
- 0x52C88A..0x52C8A6: r = Get2DRadius (vt +0x64: 5.0, 0x52C470); h = r x 0.5
- 0x52C490: the position +0x14
- 0x52C450: +0x8C
- The ctor's head insertion (0x52C407..0x52C418): newest first
- fn_0052D250 AddFisherman: a null villager -> nothing; else at the head (+0x84++), no duplicate test (EnterFishing 0x75B85E tests first: HasFisherman), and the villager's TargetThing (+0x118) = the farm (0x52D281; (to do, the villagers' area) villager::SetTargetThing). No maximum (maxNoFishermanPerFishFarm is only in Score)
- FishFarm::RemoveFisherman 0x52D290: every node of the villager out (+0x84-- each); TargetThing NOT written
- Is the villager in the list (EnterFishing's test 0x75B85E)
- +0x84 (Fishing 0x75B6CB's GameRand(n))
- FishFarm::GetArrivePos 0x52C490 (vt +0x104): the position +0x14
- FishFarm::GetTown 0x52C450: +0x8C (FishFarm::town), entt::null without one
- Town +0x788: the town's fish farms, newest first (head insertion in the ctor 0x52C407..0x52C418): the FishFarm entities with that town, by creation index from high to low. A null town gives none
- "in g_game +0x205C0C": a valid entity with a FishFarm
- FishFarm::DeleteDependancys 0x52C6B0 (FishFarm::ToBeDeleted 0x52C693; run by an on_destroy<FishFarm> listener that AddFisherman connects, so ecs::ToBeDeleted / Registry::Destroy reach it): while the list is not empty, the head's SetTopState(163) (vt +0x8E8, fields::ReleaseWorker) relying on ExitFishing 0x75B880 to unlink it (0x52C6B3..0x52C6D7); a pass that does not shrink the list ends the loop (no hook registered, or the villager side did not unlink it). The town list +0x788 and the global list need nothing (the components); RemoveMapObject is ecs::ToBeDeleted's generic part
- With the sea flattening off ([0xC37BF4] = 0 during the search, 0x52CCD4)

### Shark details

- 0x774E87: the heading of the move (pos - prevPos) * 10 / 65536 with y = 0; the last one while it stands still

### Boat details

- 0x8C7B48 / 0x3FC90FDB
- 0x8CF1B8, units per ms
- 0x5DFF6B: len(Boat1) - 400 turns it to mode 1
- 0xBF2B10..: the ScriptSFX samples at these clip times, 2D (LH_SamplePlayOptions +0xBC = 2)

## openblack

Status: **partial**. Progress rows: [things on the water](../project/progress/ocean/things_on_the_water.md) (87%),
[fish](../project/progress/animal/fish.md) (73%), [fields and fish farms](../project/progress/building/fields_and_fish_farms.md)
(87%).

- `src/ECS/Sharks.cpp` (with `Archetypes/SharkArchetype.cpp`), `src/ECS/FishShoals.cpp`, `src/ECS/FishPuzzle.cpp`,
  `src/ECS/FishFarms.cpp` (with `Archetypes/FishFarmArchetype.cpp`), `src/ECS/PuzzleGames.h`,
  `src/ECS/MissionaryBoat.cpp` (the boat) and `src/ECS/DesignedScenery.cpp` (the fixed scenery);
- the script commands they answer (`PLAYED`, the puzzle games) are on
  [Script commands](../scripts/script-commands.md).

#### Sharks

- openblack does the same (clock in double and integer ms per frame, without losing time at high fps).
- openblack: model × pose (bones in model space) × point, the same product.
- `SharkArchetype::Create`: the shark keeps its creation position, with its second field 0, as the constructor does.

#### The net, the bait and the shoals

- openblack: `components::FishBait` / `FishPlot` and `FishShoal::bait` (FishFarm.h), `ecs/FishPuzzle` (`CreateFishPuzzle`: the shoals are `FishFarm`s with a full stock and no `Transform`), the rule in `ecs::UpdateFishShoals`, `Renderer::DrawFishPlots` (`RendererFishPlot.cpp`; own instances, cut mode of `vs_object`): the bottom part in the reflection pass, mirrored, after the fish; the top part in the main pass after the rings.
- `InitFishShoal` (FishShoals.h): sets up the 15 fish of a shoal round its centre (size, place, heading, speed and turn rate), all aiming at the centre.
- `LoadFishPlotMesh` (FishPuzzle.cpp): loads `Data\MISC\Fishplot.l3d` once as `misc/Fishplot`; `PlaceFloats` puts the net's 7 floats round its centre, at the centre's height.

#### The script side

- openblack: everything that is not a villager, animal, creature or puzzle gives 1 with "Thing not living"; the Living and the creature are still not ported)
- openblack: `components::PuzzleGame`, `src/ECS/PuzzleGames.{h,cpp}` (creation, turn, `PLAYED`, clean-up of the deleted ones), `CreateScriptObject` type 32 and `PLAYED` in `CHLApi`. Only type 14; the other puzzles (Hanoi, mazes, totems, chess...) are not ported and their `PLAYED` gives 0.
- `IsPuzzleGamePlayed` (PuzzleGames.h): the `PLAYED` answer, 1 once the fish puzzle's bait is done.

#### The missionaries' boat (PetitNavire)

- openblack: `src/ECS/MissionaryBoat.{h,cpp}` (was `PetitNavire`; state, entities, PreDraw and PostDraw in `Update` with the integer time and the remainder kept), `src/ECS/DisappearSmoke.{h,cpp}` (was `SmokyStuff`; the same module as the smoke of the corpse in [Animals: species and the turn](../beings/animals.md), `Object::CreateSmokyStuff` 0x63A810), `components::DynamicShadow` (the hull shadow goes into `graphics::shadow_list` with the fixed sun, `useSun`, and also falls on objects), `Renderer::DrawBoatReflection` / `CollectBoatSprites` / `DrawBoatSprite` (`RendererBoat.cpp`; the reflection in 0x303070 is `Renderer::DrawUnderWater(view, hull, sea_pass::UnderWater(0xFF303070, 0))`, mode 2 of `vs_object` with the packed rgb; the sprites, one by one in the common transparent queue, LH3DSprite::AddDrawing 0x840CB3). Remaining differences: the deck takes the land light of its own place (not the hull's); the hull shadow comes out with the visibility of the normal camera (the original reads the mirror's, 🔍 inferred); the mode ≠ 0 of `SmokyStuff::Create` (0x823DA7) has no calls here and is not ported.
- The boat's crew table in `MissionaryBoat.cpp` lists each passenger's mesh, pose, angle, scale, place on deck and start time: two `MeshId::AnimalCow1`, one `MeshId::SpellGrainPile`, one `MeshId::PersonNorseFemaleA1` and four `MeshId::PersonNorseSailor`, in the original's order.

#### Fixed scenery per land: Land 3 waterfall, Land 4 ark and dinosaur

- openblack: `ECS/DesignedScenery` (`designed_scenery::Update` every frame after the rings, `OnLoadMap` before the registry reset): `Transform` + `Mesh` entities (meshes `misc/<file>` loaded on use), `UvScroll` on the waterfall, a water ring with `seaLight`. The dinosaur's footprint comes out by itself in the footprint pass (mesh with `ContainsLandscapeFeature`). Sound: `Audio/SoundTags` (see [Water audio](../audio/world-sounds.md#water-audio)). The objects' +0x10 (float 1 / 10) is the LOD distance factor k of `fn_00815A70` (vt+0x100): D = min((k + 1)·[0xC37EA0]·[0xC3813C], 100000) (0xC3813C is moved by `LevelOfDetail`), LOD 1/2/3/4 below 23.33·D / 66.67·D / 86.67·D / beyond (`g_last_distance` 0xEA1AF4) and each submesh is drawn if bits 29..31 of its flags contain the LOD: all three meshes have them all (0xE0000800), so k changes nothing on screen and is not stored. Differences: the dynamic lighting is the same as for all objects; when loading a map the registry is cleared, so the loop is cut abruptly instead of finishing its pass.
- openblack: `L3DSubMesh::Primitive::uvOffset` and `u_window.w` in `vs_object` (the difference of two frames: only the sheet of water changes, and the palms because of the wind).
- `ProcessWaterfall` (DesignedScenery.cpp): adds a ring at the foot of the waterfall each time its timer passes 0.7 s; `ProcessLand4` gives the ark and the dinosaur `MorphWithTerrain`, so they melt into the land like the original's morphable objects.

#### Fish farms

- `FishFarmArchetype::Create`: searches for the farm's place on the unflattened sea height, as the original does with the sea flattening off.
- openblack guard

### Test hooks

- `OPENBLACK_TEST_SHARK=1` (or `="track,camera[,forward[,from[,to]]]"`) and `OPENBLACK_WALK_PATH_TRACE=1`: the sharks.
- `OPENBLACK_TEST_FISH_PUZZLE="x,z[,inside]"` (with `OPENBLACK_HAND_TRACE=1`): the fish puzzle.
- `OPENBLACK_TEST_JC_SPECIAL="6[,mode[,frames[,ms]]]"` and `OPENBLACK_BOAT_TRACE=1`: the missionaries' boat.
- `OPENBLACK_SCENERY_TRACE=1` and `OPENBLACK_SOUND_TAG_TRACE=1`: the fixed scenery of Land 3 and Land 4 and its sound
  tags.

## Pending

