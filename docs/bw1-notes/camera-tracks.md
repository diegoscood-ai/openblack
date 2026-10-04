# Camera tracks (`Data\camera.edt`) and `WALK_PATH`

Code: `src/3D/CameraTracks.{h,cpp}` (reader and evaluation), `src/ECS/MobileWalkPaths.{h,cpp}` +
`ECS/Components/MobileWalkPath.h` (the `DataPath` of a MobileObject), `src/CHLApi.cpp` (`WALK_PATH`,
`GET_WALK_PATH_PERCENTAGE`, `CONVERT_CAMERA_POSITION/FOCUS`). RE scripts: `dev\documentacion\agua\re\edt_dump.py`
(dumps the tracks), `emu_walkpath.py` (Unicorn: the original chain over a track), `cmp_trace.py` (compares with the
openblack trace).

## The file

`GCameraEditor::CreateSegFile` 0x445530 opens `.\Data\camera.edt` once (the global `LHFile` 0xC59CF4) and
`LHFile::GetSegment` 0x7BDDD0 reads segments by name. Format: `"LiOnHeAd"` and then segments with a 32-byte name
(zero-padded), a u32 size and the data. On the original disc (150536 bytes): `EDITOR` (58084 bytes, editor names;
the game does not read it), `Cam0`..`Cam555` (32 bytes) and `Track0`..`Track52`. Creature Isle ships its own.

- **`Cam%d`** (`GCameraEditor::LoadCameraFromHD` 0x446FE0 copies the 32 bytes as they are): position (3 floats), focus
  (3 floats), and two floats that are 0 and −1 in all 556. `CONVERT_CAMERA_POSITION` 0x6ED200 pushes floats 0..2,
  `CONVERT_CAMERA_FOCUS` 0x6ED270 floats 3..5. If the segment does not exist the original leaves the stack buffer
  uninitialised; openblack returns (0, 0, 0).
- **`Track%d`** (`ScriptedCamera::Create` 0x447060 → 12-byte `ScriptedCamera`): a u32 (0 in all of them; goes to +0) and
  two `LH3DWay`, each preceded by its u16 size; they are copied to new memory, `LH3DWay::AdjustPtr` 0x844570
  rebuilds the pointers and each one gets an `LH3DWay::Running` (0x843ED0): +4 the one for the camera **position**,
  +8 the one for the **focus**. If it is missing: `"Cannot load track No %d"` and it returns 0.

### `LH3DWay` (0x24 + 44·n bytes)

| Offset | Contents |
|---|---|
| +0x00 | u16 size; +0x02 u16 = 0x63 in all of them |
| +0x04 | n points |
| +0x08 | float 0.26 in all of them (nothing below reads it) |
| +0x0C | recomputed by `AdjustPtr`: Σ, over the segments with distinct times, dt·(v0 − a·t0 + a/2) with t0 = time[i]·0.001, dt = time[i+1]·0.001 − t0, a = (v[i+1] − v0)/dt; 0.1 if it comes out 0. Used by the `WALK_PATH` of Living (below), not by that of MobileObjects |
| +0x10 | duration in ms = ftol(last time) (12212 in Track20, 12884 in Track21) |
| +0x14..+0x20 | pointers (from the editor; `AdjustPtr` rebuilds them from +0x24) |
| +0x24 | n points (vec3), then n pairs of handles (vec3 ×2: the two inner handles of segment i→i+1), n times in ms, n speeds (units/s) |

Each segment i is a cubic Bezier (P[i], handle[i].a, handle[i].b, P[i+1]).

### `LH3DWay::Running` (0x20C bytes)

+0 current segment, +0x04..+0x200 table of 128 floats, +0x204 last parameter t, +0x208 the way.

- `fn_00843F00` (table of segment s): total length of the Bezier in 127 steps (t = i·(1/127), i = 0..127, from P[s]);
  then table[i] = 127·accumulated length(i)/total for i = 1..126, table[0] = 0, table[127] = **1.0** (+0x200).
- `fn_00844280(muestra, &out)`: segment = the first i with time[i+1] ≥ sample (a new one rebuilds the table);
  sample ≥ duration → last point (touches neither segment nor t); sample ≤ 0 → first point and t = 0. Otherwise:
  L = |P[s+1] − P[s]| (the chord, `fn_008433E0`), v0 = speed[s], dt = (time[s+1] − time[s])·0.001,
  τ = (sample − time[s])·0.001, a = 2(L − dt·v0)/dt², **u = τ(v0 + aτ/2)/L** (constant acceleration that covers
  the chord exactly at time[s+1]). Then k = ftol(127u), lo = table[k], hi = table[k+1]; if u == lo, t = lo; if
  not, the loops that search for the entry move a pointer but do **not** change lo or hi, and t = w·lo + (1 − w)·hi with
  w = (hi − u)/(hi − lo), which is u again (with the rounding of the table's scale, up to ~4·10⁻⁴ in t with the FPU
  at 24 bits). So the table does not reparametrise anything. Outside the table (u < 0 or u > 1, which only happens if v0 > 2L/dt;
  in the shark tracks all speeds are 10) the original reads other fields of the object: openblack
  uses t = u. out = the segment's Bezier at t; t is left at +0x204.
- `fn_008439C0(tramo, t, &out)` is the bare Bezier (the one used by `WALK_PATH`); it sums x in a different order from y and z
  (((B2·3ut² + B1·3u²t) + B3·t³) + B0·u³), openblack copies it.

## `WALK_PATH` (177) of a MobileObject

`GScript::WalkPath` 0x6FBB50 pops (object, forward, track, from, to). If the object is Living it goes to 0x5EE100 (below);
if it is a MobileObject (`__RTDynamicCast`) to `fn_006076C0(pista, desde, hasta, adelante)`; otherwise, "Thing is invalid for move
path". `fn_006076C0`: adds the object once to the list g_game+0x205CD4 (= `GlobalGameLists` +0x130) and gives it a
new `DataPath` (0x30) at +0x64: +0x14 = `ScriptedCamera::Create(pista)`, +0x1C = to, +0x20 = forward,
+0x28 = **100** (step), +0x2C = 1, **+0x24 = from × duration**.

Every turn, in `GGame::ProcessTurn`: `Whale::ProcessAll` 0x54E5C7 (start of turn = Pos), then
`GlobalGameLists::Process` 0x5913ED calls `MobileObject::MoveAlongPath` 0x607790 (vt+0x52C) of each object in the
list, and afterwards `GScript::Process` 0x54E693 (the scripts: the `WALK_PATH` takes effect from the next turn):

1. sample = ftol(+0x24) if forward, ftol(duration − +0x24) otherwise; clamped to 0..duration.
2. `fn_00844280` on the **position** Running (its point is discarded) and `fn_008439C0` on the **focus** way with the
   segment and t it left: **the object follows the focus curve, with the timing of the position curve**.
3. `fn_00607990`: if +0x24 / duration < to: +0x24 += 100 (capped at the duration) and
   `SetPos(MapCoords(ftol(x·6553,6), ftol(z·6553,6)), y relativa 0)` + `Game3DObject::SetPosition(coords, 0, 0, 0, 1)`
   (height = GetAltitude + 0; it also sets rotation 0 and scale 1 on the 3D object, which the shark's `Draw` sets again
   every frame). Otherwise it leaves the list **without moving** that turn (the DataPath stays).

Units: the sample is the track time in ms and the step of 100 is one 100 ms turn, so the track is
traversed in real time (Track20: 123 turns; Track21: 129). Backwards it goes through the samples from the end; `desde` and
`hasta` are always compared with +0x24 / duration, so `WALK_PATH(o, 0, 21, 0,25, 0,75)` starts at sample
9663 and stops when +0x24 reaches 9721 (0.75 of the duration). There is no loop: when it finishes, the object stays where it is.
A second `WALK_PATH` on the same object does not add it to the list again and replaces the DataPath (the old one is lost).
`ToBeDeleted` 0x606F4A removes it from the list (in openblack the component dies with the entity).

`GET_WALK_PATH_PERCENTAGE` (179, 0x6FBC50): **1.0** for anything that is not Living; for a Living
`Living::GetWalkPathPercentage` 0x5EE520 = +0xAC→+0x24 / duration. No original script calls it.

### Living (not ported)

0x5EE100 (misassigned symbol `Animal::DebugText`): deletes the old DataPath at +0xAC, creates one with the track, +0x18 =
track, +0x1C = to, +0x20 = forward, **+0x28 = duration / (focus length (+0x0C) / (speed(+0x5A)/655·0.1))**,
+0x2C = speed, +0x24 = from × duration, and calls vt+0x8FC, Living::SetAnim (not AddFootpath: corrected by session Personas, 2026-10-04). It is moved by
`Living::MoveAlongPath` 0x5EE230. Used by `FollowUs` (Father, track 22) and `BlindWomanJourney` (track 7).

## The sharks of Land 1 (`FollowUs`, challenge.chl)

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
`OPENBLACK_TEST_SHARK="pista,cámara[,adelante[,desde[,hasta]]]"` a single one at that camera's focus. Capture
`_audit/agua/paths_sharks1.png`.

`RUN_CAMERA_PATH` (119, 0x6ED7F0) uses the same track from the script camera (`CameraModeScript`, `fn_00461A80`):
the reader exists (`LoadCameraTrack`, `CameraWayRunner`), the camera does not.
