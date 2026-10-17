# Fights

## Summary

Two creatures fight in an arena. The original has no fight camera mode: the player's own camera keeps a fight state,
and when the fight starts it either watches it (flies to the arena and orbits the fighters) or only plays the spirits'
remark. The watched fight shows a panel of the two fighters' health and stamina; at its end the camera lingers 3 s.
Inside a fight, a miracle's blow goes to the fight health (taken at 0.1 while blocking) and makes the creature reel;
a faint, from a fight or from anything else (a destroying effect, the tornado), ends the fight.

openblack has the fight start's watch rule, the arenas list, the camera's watch and linger through the agreed
`camera::FightWatch`, the panel, blocking, the reel and fainting. Neither Land 1 nor Land 2 ever has two creatures,
so no verification run reaches a fight. This page owns the fight camera; the player camera's other modes are in
[The player camera](../camera/player-camera.md).

## The original

The `LH3DCreature` classes have no bodies in bw1-decomp; the camera's field names are those of
`src/Black/CameraModeNew3.h`.

### Fights: the camera

- **There is no fight camera mode.** The player's camera `CameraModeNew3` has a fight state (`HasFight` +0x44, the
  arena +0x48, `FightDistance` +0x58, `FightTimeLeft` +0x5C, `FightStatus` +0x64).
- **The creature's fight start** (an unnamed function at 0x5036C0, inside the wrongly sized symbol
  `SubStatePerformAddVillagersToDance`): once `LH3DCreature` fn 0x484410 has taken the arena's position and radius,
  it makes reaction 0x13, counts the fight (+0x178), and asks the camera: `GGame::GetCamera` 0x5037D8, its current mode
  `dynamic_cast` to `CameraModeNew3` (0x5037FB). Only the player's own mode calls `StartFight(GArena*)` 0x45A4D0
  (0x50380A); any other mode (a script's) does nothing, not even the remark. There is no owner test there.
- **`StartFight` 0x45A4D0:** nothing when the arena or either fighter (+0x38, +0x3C) is null (0x45A4DD..0x45A4F3).
  It sets `HasFight` and the arena, then a local "watch" flag = 1, cleared when `WantToQuitFight`
  (`WantToQuitFight(camera position, screen centre +0xF0, 1.0)` 0x45A390) is true (0x45A539). It **watches** when
  (either fighter's `GetPlayer` (vt 0x1C) is the local player's, and either fighter has `Flags` +0x24 & 0x400,
  controlled by script) or the watch flag is still set, that is the camera already looks at the arena
  (0x45A53D..0x45A5AC). Watching sets up the camera's fight state and flies to the arena (0x45A5B2..0x45A71D).
  **Not watching** (0x45A72A..0x45A7B7) clears `HasFight` and the arena and, when `MyInterfaceStatus` is set and a
  fighter is the local player's (the first fighter tested first), plays the spirits' `CreatureFight` remark
  fn_0071CD40 (help list 12, [audio](../audio/engine.md)) for it; fn_0071CD40 does not use its creature argument. The camera
  does not move.
- **The start, read whole** (0x5036C0) 📼 1.20 only: run by a creature in its Fight activity; it waits (returns 0) while its 3D
  body performs a body action (`IsPerformingBodyAction` 0x4842D0: +0x4994 ≠ 0), takes the arena from its own +0x10A0
  or else from the plan's object's (0x503737..0x503746), and calls the 3D start 0x484410 with the opponent's 3D, the
  arena's position (+0x14 / 6553.6, y = land + +0x1C) and its radius (vt+0x60). 0x484410 refuses while a body action
  runs, when the body lacks animation 0x7A, 0x7B or 0x7C, or while +0x51EC is set; else it stores the opponent
  (+0x528C) and the arena (+0x4A98..+0x4AA4) and enters state 0x16. Only then come the reaction, the count and the
  camera. Whether the opponent runs this sub-state too, and so asks the camera twice, depends on how its activity is
  set through mind +0x1BE8 (the joint-activity partner, see
  [the size spells](body.md#the-size-spells-the-start-delay-and-the-targets)); not traced.
- **The arenas the camera asks for.** Besides the arena `StartFight` is given, the camera reads the game's list of
  arenas (head g_game +0x205C7C, next +0x48, count +0x205C80) when it lingers over one or is double clicked onto one
  (0x45BB64.., 0x45DFC5..), and each frame asks its own arena `IsAvailable` (vt +0x2C, 0x45A9B1..0x45A9CA). Of an
  arena it reads the fighters (+0x38 the creature that made it, +0x3C its opponent, set by fn_00424AB0 at
  0x424AC7..0x424ACE), `GetPos` and `GetRadius` (vt +0x60, 0x424780: +0x30). A new arena (fn_00424820, made by
  fn_00424E70 from the creature's fight start at 0x507566 and from `GScript::GetArena` 0x6F4C36) is pushed at the
  list's head, so the list runs newest first. On the creature's path the arena's position is a MapCoords of the two
  creatures' middle, (FtoL(midX × 6553.6), FtoL(midZ × 6553.6)) with **altitude 0** (0x5074B1..0x50750C): `GetPos`
  gives the middle quantised to a map position, and the land's height there.
- **The arenas' list** 📼 1.20 only: only fn_00424E70 makes arenas (from the creature at 0x507566 and `GScript::GetArena`
  0x6F4C36). The creature's path 0x507480 does nothing when the partner (mind +0x1BE8) already has an arena
  (+0x10A0); else it takes the middle of the two 3D positions as MapCoords (altitude 0), calls `SetRequiredSpeed` on
  both, and d = [[3D+0x5198]+0x6403C] × 10 (the field not identified). fn_00424E70(pos, a, b, d): r = fn_00424FB0
  (below); fn_00424E00 gives the nearest listed arena strictly closer than d, whose radius then becomes min(its
  radius, r) (0x424EA1..0x424ECD; the smaller, not the larger as once written) with +0x40 = 0; else, for t = 1.0,
  0.9 … while t > 0.59, a free-place search (0x4DFC80, a cell grid round pos; creature +0x400 = 250, +0x3FC = 2·t·r)
  and, where it finds a place, a new arena of radius t·r pushed at the list's head with +0x40 = 1; none found gives no
  arena and the sub-state returns 1. fn_00424AB0 then stores the fighters, sets +0x34 = 1 and plays sample 0xB0 at
  the arena. The maker's `StopWhatIAmDoing` (0x475BEB..0x475C07) deletes an arena it made (+0x40:
  `GArena::ToBeDeleted` 0x424960 unlinks it and, through `GameThing::ToBeDeleted` 0x56FB70, sets +0xA |= 1, so
  `IsAvailable` turns false), or clears a reused or script arena's fighters and +0x34 (fn_00424B60).
- **The radius and the range** 📼 1.20 only: fn_00424FB0 gives 15 × max(3D +0x90 of a, of b) × 2.6 ([0x9C95F8]), within
  [0, 60]. The 3D's arena radius +0x4AA4 has three readers: `CheckFightQueue` 0x489546 takes the queue only while
  the opponent is within 0.8 R ([0x8CF09C]) of the middle; `StartFightAction` 0x4846E0 (state 0x17) refuses the steps
  0x8A, 0x8B and 0x8C (back, right, left) while the creature itself stands beyond 0.8 R and [0xC64214] is 0 (no
  writer found); the block recoil (0xC6, 0x486F27) moves the body only to a place strictly inside 0.8 R.
- **The end of a fight.** fn_0045A800 starts the camera's 3 s linger (if `FightStatus` is 0: `FightTimeLeft` = 3000
  when ≤ 0, `FightStatus` = 1; so a second call is harmless). The creature side calls it, after the same
  `dynamic_cast` to `CameraModeNew3`, from:
  - **fn_004845F0** (0x48466D), the `LH3DCreature`'s end of a fight: it clears the opponent (+0x528C), calls the
    linger with no owner test when the fight state +0x4994 is 0x16, 0x17, 0x18, 0x1A, 0x1B, 0x1D, 0x20, 0x21 or 0x22
    (start, stance, action, the block's start, block and end, the three cast states; not finish, the block recoil,
    faint or get up), and takes `(1 − fight health +0x4AA8) × 0.25` off the creature's life (vt 0x5B0);
  - **the "Fight" action's running sub-state 0x503830** (its sub-states are set up at 0x4F9FC4..0x4FA03A: 0x503600,
    the start 0x5036C0, this one, 0x503A50), at 0x5038A6, reached three ways, each time only for the local player's
    creature: (a) its body action is over (0x503842; exactly "the 3D body no longer performs a body action",
    0x50383D..0x503844); (b) the opponent's life (vt 0x11C, the plan's object's) is gone, at 0 or below, and it is not
    script-controlled (Flags +0x25 & 4 clear): the `GeneralGood` remark fn_0071CDF0, reaction 0x28, a win (+0x17C),
    fn_004845F0, then the linger (0x503A1F); (c) its `LH3DCreature` has no opponent any more (+0x528C = 0,
    0x5039B6): the `GeneralBad` remark fn_0071CD70, fn_004845F0, then the linger.
  `EndFightNow` is never called by the fight.
- **The fight music** 📼 1.20 only: `GInterface::ResetActionState` sets interface state 0x10 when vt+0x40C holds, the camera
  mode is a `CameraModeNew3` with `HasFight` (+0x44, read at 0x5D2A3A), fn_005D2990 holds (g_game+0x250538 set and
  the interface player's creature fighting, `IsFighting` 0x47B1C0: 3D +0x528C ≠ 0) and the hand holds nothing
  (`GetFirstObjectInCurrentHand` 0x5DC130). fn_00427660 plays the fight music when the local player has a creature
  (+0xA64) and either that state is 0x10 or an arena in use (+0x34) with that creature as a fighter (fn_00425060) lies
  under 100 ([0x8AB41C]) from the camera's point [0xEA1DB8]
  ([GameMusic::ProcessMusic and its sources](../audio/music.md#gamemusic-processmusic-and-its-sources)).
- **Can it happen on Land 1 or Land 2?** No. A fight needs two creatures, and on those lands the only creature is the
  local player's, made by `LOAD_MY_CREATURE` (never a second one); the other makers are the playground scripts'
  `CREATE_CREATURE_FROM_FILE` and the debug spawner.

#### Player camera: watching a fight

`CameraModeNew3` keeps a fight of its own (bw1-decomp names, `CameraModeNew3.h`): `HasFight` +0x44, the `GArena` +0x48
(fighters at +0x38 / +0x3C), `Yaw0` +0x50, `Pitch0` +0x54, `FightDistance` +0x58, `FightTimeLeft` +0x5C (int ms),
`TimeInArena` +0x60 (int ms) and `FightStatus` +0x64 (0 on, 1 lingering, 2 ended). The addresses come from the
disassembly of the fight camera's functions.

- **`StartFight(GArena*)` 0x45A4D0:** returns at once without the arena or either fighter (0x45A4DD..0x45A4F3). Then
  HasFight = 1, the arena, and the in-view test `WantToQuitFight(GetPos, +0xF0, 1.0)` (0x45A530). It watches when a
  fighter's `GetPlayer` (vt +0x1C) is the local player and either has Flags +0x24 & 0x400 (controlled by a script), or
  when the arena is in view (0x45A53D..0x45A5AC). Watching (0x45A5B2..0x45A71D): FightDistance = 5.5 [0x9CE6C4],
  Yaw0 = π/2 (0x3FC90FDB), Pitch0 = 0.408407 (0x3ED11ABA), FightStatus = FightTimeLeft = 0, +0x4C..+0x4E = 0,
  +0x2D0 = 1 (the first frame), the turn zoomer +0x210 zeroed field by field and the focus Zoomer3d +0x240 / +0x270 /
  +0x2A0 `SetPosition(0)`, +0x78 = 0, then `FlyToPosFoc(pos, foc, 0)` (vt +0x50, 0x45A71D) to the view fn_00458E50
  builds (below), with P the arena's centre (`GetPos` vt +0x100: x and z × 1/6553.6 [0x8AA3A4], `GetAltitude` + its
  altitude; the quantised MapCoords summed at 0x45A646..0x45A68B) and Q = (P.x + r, P.y + r × 0.5, P.z) with
  r = `GetRadius` (+0x30). The creature's arenas are made with altitude 0 at the quantised middle of the two fighters
  (0x5074B1..0x50750C), so P.y is the land there. Not watching (0x45A72A..0x45A7B7): HasFight = arena = 0, and the
  `CreatureFight` help sprite fn_0071CD40 for the local player's fighter; the camera does not fly.
- **The fly-to view, fn_00458E50(from, focus, &pos, &foc) 0x458E50** (cdecl; its callers are StartFight 0x45A706 and
  `CameraModeFollow::Update` 0x44C867): foc = focus; heading and pitch from `GetHeadingAndPitchFromPoints(from,
  focus)` 0x4428D0; d = |focus − from| (3D, float subtractions, (dz² + dy²) + dx², a float); d < 50 [0x8C6CA4] →
  (50 − d) × 0.8 [0x8C4A04] + d, then d > 100 [0x8AB41C] → (100 − d) × 0.1 [0x8AB22C] + d (not a number passes both
  unchanged); heading = `FindBestAngle(heading, d, focus, &pitch, null)`; pos =
  `SetPointFromPointDistanceHeadingAndPitch(focus, d, heading, pitch)` 0x442810. It touches no zoomer. No random draw
  in its tree (`__ftol` is the only runtime call).
- **`CameraModeNew3::FindBestAngle(heading, distance, point, &pitch, &score)` 0x458F40** (static, cdecl): for each of
  32 headings heading + i × π/16 [0x8AB504] (a float), the score is the sum over j = 3..7 of point.y − the land's
  altitude at `SetPoint(point, j × distance × 0.125, that heading, 0)` (map coords (m × 65536) × 0.1, truncated), plus
  50 × cos(i × π/16) (50 a double, `fcos` of the float i × π/16); the first strict maximum wins (from −1e20,
  0xE0AD78EC; ties keep the lower i). The same loop computes `SetPoint(point, distance × 0.3, heading + i × π/16, 0)`
  and its altitude and does not use them. Then the pitch: pitch × 0.2 [0x8AB244] + (normal pitch × 0.5) × 0.2
  [0x8C7C68, a double] + 3π/25 (0.37699114390179034 [0x8C7C60], a double), where the normal pitch is
  `GetHeadingAndPitchFromPoints(point + n, point)`'s with n = `GetNormal` at the point's map coords (1.5393804 on level
  land); ≤ π/8 [0x8C6CA0] (or unordered) → π/8, else ≥ π/3 [0x8C79E0] → π/3. Returns heading + best × π/16; `score`
  gets the best score when not null. On level land the arena's view keeps the heading (3π/2, back towards the centre)
  and gets the pitch 0.2 × atan(−0.5) + 0.1539 + 0.3770 ≈ 0.4382. The double click's flight calls it too (0x45E22F).
- **The hand-over:** the fly-to runs once, in StartFight; the per-frame orbit never calls it. The orbit runs from the
  next `Update` and writes the frame's origin and focus, but while +0x100 is set the tail gives the zoomers the
  flight's legs instead (see [Player camera: flights to a place](../camera/player-camera.md#player-camera-flights-to-a-place)).
  After about 0.75 s the last leg is set and +0x100 cleared (that frame skips the orbit's place too); from the next
  frame on the orbit's place replaces the last leg. So the view of the arena is an approach, not reached and held. The
  orbit's zoomer time [esp+0xB8] (0 each frame at 0x45E359, set at 0x45E3C3..0x45E408) is 2.5 [0x8C581C] × (GCamera
  +0x68 > 2 [0x8C7BC0] ? 0.8 [0x9CE6C0] : 2 [0x8C7BBC] + (0.8 − 2) × +0x68 / 2): 2 s once the mode has run 2 s, 5 s
  right after a mode change. It is set only when the orbit runs, after its skips and its HasFight, arena and fighters
  tests (0x45E350..0x45E3A4). Not zero, it replaces the tail's time for the origin and the focus
  (0x460229..0x46024A, after the self tilt's 1 s at 0x460216); the flight's legs (0x460390..) keep their own times.
  - The mode seconds, GCamera +0x68 (see [GCamera and its zoomers](../camera/game-camera.md#gcamera-and-its-zoomers)):
    `GCamera::Update` adds the frame's camera seconds (`GGame::GetCameraTimeInc` × 0.001, at most 0.1,
    0x441F95..0x441FC1, also kept at +0x6C) at 0x441FCD, before the current mode's Update; `SwitchToViewMode` sets it
    to 0 for any mode pushed (0x441D30: the script mode, a dual camera, a follow camera), and `PopViewMode` for any
    mode popped (0x441C86, back to the mode below; 0x441CB9, the last one popped and a new CameraModeNew3 made);
    fn_0044BB30 sets it to 2 (0x44C141). StartFight does not touch it: the player's mode goes on, so a fight started
    2 s or more after the last mode change eases in over 2 s.
- **Its callers:** the creature's fight start 0x50380A (when the camera's mode `dynamic_cast`s to CameraModeNew3);
  lingering in `Update` 0x45BB64..0x45BCA1 (not fighting, the screen-centre ray on the land +0xC8, and for an arena
  of the list g_game +0x205C7C, linked by +0x48, both the camera and its focus strictly within its radius across the
  land, with both fighters: TimeInArena += g_delta_time, StartFight above 1000 ms; TimeInArena is only reset while
  HasFight, 0x45A980); and the double click's flight onto an arena 0x45DFC5..0x45E086 (feature 0x10, not the give-up
  flight, not fighting, the target within an arena's radius).
- **`WantToQuitFight(camera, focus, k)` 0x45A390:** false without HasFight, the arena or +0xC8. With r = GetRadius
  (vt +0x60) × k and the across-the-land distances (dz² + dx², the arena's altitude read and dropped): true when
  r × 3.2 [0x8C7C8C] < |arena − focus| and r × 4.2 [0x8C7C88] < |arena − camera|, or r × 6 [0x8AB35C] < |arena −
  camera|. StartFight passes `LH3DCamera::GetPos` and the screen-centre hit +0xF0 with k = 1.
- **`Update` 0x45A971..0x45A9CA, first:** with HasFight, TimeInArena = 0; FightStatus 1 counts FightTimeLeft down by
  the frame's game ms g_game +0x205D48 and turns 2 below 0 (`jns`); FightStatus 2 → `EndFightNow(1)`; else an arena
  gone or not `IsAvailable` (vt +0x2C) → fn_0045A800(1) and arena = 0. Then (0x45AA7A..0x45AAB7) with HasFight, an
  arena and mouse mode 0 or 2: +0x12C = the focus and +0x4C..+0x4E = 0.
- **Dragging away 0x45BAFF..0x45BB5F:** after the screen-centre ray, when 1.5 × 3 [0x9CE60C × 0x8C2C50] < +0x78,
  `WantToQuitFight(camera, +0xF0, 0.75)` and the mouse mode +0x8C & 1 (the previous frame's: +0x8C is set later, at
  0x45BDE9): `EndFightNow(0)` and `UpdateClickParams(camera, focus, 1)`. +0x78 is the time since the camera's last
  flight began: += the camera's seconds every frame (0x4601A7..0x4601B7), 0 at StartFight, 0 / 0.75 / 0.75 / 0.01 for
  the double click's 100 / 60 / 30-15 / give-up flights (0x45E13D..0x45E1DD), 0 at a flight's last leg (0x4603EF).
- **The turn and tilt (0x45C6A8..0x45C8A9, 0x45D2B2, 0x45DCCF):** Yaw0 += turn × π / w (with feature 2) and the edge
  rotate's angle; Pitch0 −= tilt × 0.002 (with feature 1, or with a local flag, 🔍 inferred the self tilt), and +=
  the pitch drag's step (the other way from the camera's own pitch).
- **The keyboard skip [esp+0xCF] (0x45E36F):** 0 each frame (0x45B930). With feature 8 (`GetCameraFeatures` & 8,
  0x45C943..0x45C948) a keyboard move input [0xC5B0D4] or [0xC5B0D0] not 0 first reports Drag (`CameraHelpCallback`
  0x308 with the input mask [esp+0xB8], 0x45C94E..0x45C986, in any mouse mode); then, in mouse mode 2 (+0x8C,
  0x45C98E: camera input, nothing gripped), the move is added to the frame's origin [esp+0x14], focus [esp+0x3C] and
  the click focus (0x45C9A1..0x45CAA7), and with either input still not 0 the flag is set (0x45CAD0). That block is
  the only one in `CameraModeNew3::Update` that applies the keyboard moves; the other uses of [0xC5B0D0] / [0xC5B0D4]
  there test them (0x45AD12..0x45AD38, 0x45B86F..0x45B882) or clear them (0x45ADAF, 0x45AE68, 0x45BE2C, 0x45CC08).
  So in mode 2 a keyboard move skips the orbit that frame and the move stands; the watch goes on (no keyboard input
  ends it: the drag away needs mode & 1).
- **Watching, each frame (0x45E37E..0x45E8D6):** skipped while +0x8C is 1 (the land alone dragged), or 2 with
  [esp+0xCF] (0x45E36F, above); needs HasFight, the arena and both fighters. With A = +0x38 and B = +0x3C, their
  LH3DCreature (+0x160 → +0x58) positions +0x78 and radii +0x5228 (`Creature::GetRadius` 0x4792C0; set by
  `LH3DCreature::SetSize` 0x48064C):
  - the middle (A + B) × 0.5, its y raised by (fn_004867B0(B) + fn_004867B0(A)) × 0.25;
  - d = |A − B| across the land; with a zoom ([esp+0xC8] = the zoom delta, [0xC5B0CC] × 0.0015 × the zoom scale) ≠ 0,
    FightDistance = ((((d + rB × FD) + rA) + zoom × 0.3) − d − rA) / rB (0x45E47D..0x45E51F);
  - FightDistance > 40 [0x9CE6C8] → `EndFightNow(0)` (0x45E538), and the frame goes on; then FD ≤ 0 → 0, FD < 40
    or 40; Pitch0 ≤ 0.241661 [0x8C78E0] → it, < 1.32914 [0x8C7C98] or it;
  - the distance (d + rB × FD) + rA (0x45E5A9..0x45E5EE);
  - the heading of B − A by fn_007FAA50 when |x| or |z| > 0.01 (the double 0x8C7A10), else the turn zoomer's value;
  - the first frame (+0x2D0): the turn zoomer and the focus Zoomer3d set to the heading and the middle, +0x2D0 = 0;
    otherwise the turn's destination value + W(W(heading) − W(value)) (W = fn_0045A8C0: as it is within [−π, π], else
    (t − ftol(t)) × 2π with t = a × 0.159155 [0x8C7C90], then twice: > π − 2π, < −π + 2π) and the focus' the middle,
    both speed 0 over 5 s (`SetDestinationWithSpeedAndTime`); both step with the camera seconds (the turn inline, the
    Zoomer's own update);
  - the origin `SetPointFromPointDistanceHeadingAndPitch(focus, distance, Yaw0 − turn, Pitch0)` 0x45E89C; +0x12C =
    the focus; the mode switch [esp+0xB4] = 3 (no mode case runs); RotateAroundPoint +0x88 = 0. The tail (the self
    tilt's height 0x45F10B with feature 0x40) still runs after it.
- **Ending:** `EndFightNow(int)` 0x45A830: with [0x9CE6BC] (= 1) or the argument, HasFight = arena = FightTimeLeft =
  0, FightStatus = 2. fn_0045A800(int) 0x45A800: with [0x9CE6BC] or the argument, and only while FightStatus is 0,
  FightTimeLeft = 3000 when ≤ 0, FightStatus = 1; a second call does nothing. Its callers: `Validate` 0x45A8AF,
  `Update` 0x45A9C5, fn_004845F0 0x48466D and 0x5038A6 (the fight's end, above).
- **Features (`GetCameraFeatures` 0x456260, vt +0x58):** HasFight ? EnabledFeatures & ~0x10 : EnabledFeatures. Of its
  17 calls in `Update` only 0x45DDA3 tests 0x10: the double click's flight and the give-up flight are off while a
  fight is watched, the 3 s linger included. EnabledFeatures itself does not change.
- **Tricons (`UpdateTricons` 0x459398..0x45939F):** with HasFight, +0x90 &= ~2, the Pitch zone; the classifier then
  cannot start an edge pitch drag. The pitch icon still shows from +0x94 & 2 (the tilt input, or HandStatus 5 / 6;
  0x45934B..0x45936E, 0x45943C..0x45946C).

#### The fight's panel

- **Where it is drawn** [V]. fn_005CCAB0, the help text's frame callback (symbols.txt wrongly folds it into
  fn_005CBEC0), is called only from fn_005CD020 (at 0x5CD037), registered at 0x5CAD75 (0x5CAD74..0x5CAD81) with
  `RegisterFinishFrameCallback(20000, after = 1, 0x5CD020)`, so it runs inside `LH3DRender::FinishFrame` 0x82F460 in
  the "after" callbacks. The whole callback is skipped when g_game+0x250188 is set (0x5CCAD4 → 0x5CCE41) or the
  HelpSystem (g_game+0x25005C) is null (0x5CD035). It calls fn_00456270 at 0x5CCCD5 (0x5CCC7E..0x5CCCD5) when the game
  runs (g_game+0x250538, set to 1 in `GGame::Init` 0x54FCDC and to 0 in `Close` 0x54ECE4), the camera's current mode
  (`[cam+0x58]` index into `cam+0x28`) is a `CameraModeNew3` (a `dynamic_cast` at 0x5CCCC0, so no panel under a
  script camera mode) and its `HasFight` (+0x44) is set (loaded at 0x5CCCCC).
- **fn_00456270, the panel's draw** [V]: it needs the mode's arena (+0x48), the arena's two fighters (+0x38 and +0x3C)
  and each one's `LH3DCreature` (`[creature+0x160]+0x58`) (0x456270..0x4562A5), and calls
  `fn_00516CB0(h0, s0, name0, h1, s1, name1, 0xFF)` (0x4562A7..0x4562D6) with each fighter's fight health
  (LH3DCreature +0x4AA8), stamina (+0x4AAC, as `CastSpellOnObject` reads it) and name (Creature +0xE0).
- **fn_00516CB0** [V]: draws nothing in the sequence mode (g_game+0x205A28), under an active SetupBox (+0x94) or with
  alpha below 1 (0x516CB5..0x516CDD); sets `SetupThing::DrawAlpha`; then one see-through box (`DrawBox` 0x5F000000,
  0x516DD0) and a loop of two rows (0x516E46..0x51705E), fighter 0 first and the second row 3.5 units below it
  (0x516E3A, 0x517049); per fighter the health and the stamina clamped to [0, 1], two bars (fn_00516B00) and the name
  twice (`DrawText`, a black shadow 2 pixels down and right, then white, 0x516FB0..0x517038). The health bar's colour
  runs red → yellow → green around 0.5 (0x516F27..0x516F94); which bar is which colour on screen 📼 1.20 only.
- **The layout** 📼 1.20 only: its unit is u = the screen height (word [0xE8505A]) / 70, a whole number (0xEA0EA0EB, sar 6);
  33·u and 9·u are formed (0x516D6C..0x516D86); fn_00516B00 draws each bar's frame (0x413C20) and its fill to
  ftol((x1 − x0 − 6) · v + x0 + 3), kept within [x0, x1], with black one-pixel lines. The exact pixel layout was not
  transcribed.
- **Which fight** [V]. Only the one the camera watches: `HasFight` is set by the camera's `StartFight` 0x45A4D0
  (0x45A50A) and cleared there at once when it does not watch (0x45A72A); nothing searches the duels. So the local
  creature's own fight shows no panel unless it is watched.
- **The linger** [V]. The 3 s linger (fn_0045A800) only sets the time left and the status (0x45A818..0x45A826);
  `HasFight` and the arena are cleared by `EndFightNow` when the time runs out (0x45A983..0x45A9AA; it clears them at
  0x45A842..0x45A84B), or the arena alone earlier when it is no longer available (`IsAvailable`, vt +0x2C = GameThing
  0x401810, `!(byte +0xA & 1)`; 0x45A9B1..0x45A9CA). So the panel stays up through the linger while the arena is
  there.
- **The rows** [V]. The first row is the arena's +0x38, the creature that made the arena (the maker 0x507483, only when
  the opponent has no arena +0x10A0 of its own, calls fn_00424E70 then fn_00424AB0(maker, opponent) at
  0x507579..0x50757D, which stores +0x38 and +0x3C at 0x424AC7..0x424ACE), the second its opponent: not "the local
  player's creature first".

##### The frame order around the panel

`FinishFrame` 0x82F460 [V unless marked]: the Z-sorter's flush fn_0082F280 (0x82F480); the "before" callbacks
(0x82F49C..0x82F4C7); the sun glare fn_0086BB60 (0x82F4C9); a Z-reset full-screen quad (0x82F5F6..0x82F614); the
**cinema bars**, two rectangles drawn at once by fn_0081E590 (it draws through DrawAndClip2D, it does not queue) when
[0xEB9950] is set (0x82F652..0x82F6DD); the "after" callbacks (0x82F6E5..0x82F718), in ascending priority 📼 1.20 only, among
them the help text's 0x5CD020 (20000): fn_005CCAB0 draws the dialogue box and texts (fn_005CC760 at 0x5CCC72,
fn_005CBF10 at 0x5CCC79), then **the fight's panel** (0x5CCCD5), fn_0050B2B0, the multiplayer block and the KMIcons
(0x5CCE41), then **the status panel** fn_005178D0 (0x5CD03C); then the video callback (0x8000) and later ones;
fn_00836200 (0x82F74E); the **screen fade** fn_0086FEE0 (0x82F753), only when the fade alpha byte [0xFA51DB] is set,
a full-screen quad with **the bars drawn again** over it when [0xEB9950] is set (0x870130..0x8701AB); EndScene
(0x82F7D3). So both panels are drawn after the bars and after the dialogue and help text, and before the fade; under
a fade the fade covers them and the bars are drawn again on top. Whether the SetupThing text
(`GatheringText::DrawTextRaw`) is drawn at once like `DrawBox` / `DrawQuad` was not checked [L: at once].

##### The creature's status panel, drawn after it

Who sets it and when it shows is on [the creature's overview](overview.md#audio-status-panel-and-the-follow-camera);
the reading also corrects an older note: `Creature::DisplayStatistics` 0x476070 (`DisplayStatistics`) is not the status panel but a
developer text dump through `GDebug::SetMessage(0xCD3B24, 2, ...)` ("Age: %d Life: %.2f ..." at 0x4765E5), its first
block gated by [0xCD3C68] (0x47607C), called only from `Creature::ProcessState` 0x473952 for [0xC64144], the first
creature initialised (set at 0x4744F1, cleared at 0x4751E6), and written only when GDebug's channel 2 is on
(0x511DB1). The real panel is fn_00517080 (bw1-decomp's `DrawCreatureStats` 0x517265 is a mis-sized label inside it),
drawn by fn_005178D0 as `fn_00517080([0xCC62EC], [0xCC62E8], [0xCC62E4], [0xCC62E0], [0xCC630C])`, alpha and the
layout flag cleared after it every frame (0x5178D0..0x517905). [V unless marked]:

- **The values.** `Creature::Draw` 0x517910 (0x51791D..0x5179D7) writes them for the creature with Flags +0x24 & 0x10
  that is `MyInterface()->GetInteractObject()` (0x5CED10: the interface's +0x400 only when it has Flags & 0x10 and
  `ValidAsInterfaceTarget`, vt 0x6F0): alpha 0xFF, the layout flag [0xCC6310] = 1, damage 1 − `GetLife` (0x517958),
  hunger 1 − physical +0x1C, tiredness physical +0x30, and the reward [0xCC62E0] = `HandStateCreature` +0xD8 of the
  creature's player's render hand (`GetPlayer()->GetRenderHand()`, 0x51799D), only while that hand's state
  `dynamic_cast`s to `HandStateCreature` (0x51799D..0x5179D7, the only RTTI user of it, refs to 0xBE8ED8), else the
  last value stays. Flags & 0x10 is set by fn_005DA950 (vt 0x190, then `NetworkFriendlyStartLockedSelect` vt 0x6D0,
  Creature 0x476E70; `Flags |= 0x10` at 0x5DA99C) and cleared by fn_005DAA10 (`NetworkFriendlyEndLockedSelect` vt
  0x6EC; 0x5DAA5A), both called only from `GPacket::ProcessPacket` (0x63CBC9, 0x63CBE5); no other write of 0x10 to
  +0x24 exists (a byte-pattern scan). `CameraModeFollow::Update` (0x44C1E5..0x44C23F) writes the three values with
  the flag 0 and the reward untouched when `GetFocusThing()` (vt 0x48 = 0x44B980) is a creature (vt 0x34); its
  constructors are called from `GGame::DoAction` 0x63FB4B, `CameraModeNew3::Update` 0x45E34B,
  `GatheringBox::InitControls` 0x573131 / 0x573342, fn_00441650 (from `GCamera::Load`) and fn_00461180; which is the
  player's "follow my creature" is not known. The four values are zeroed only by fn_0044A850, the
  `CameraModeCtrInteract` constructor (vtable 0x8C7740), which `CHand::OperateInteractionCamera` 0x46CC2A builds right
  after `SwitchToViewMode` (0x44A8C9..0x44A8F2). If both writers run in a frame, the later one's flag wins [L: the
  camera first]. In openblack's terms 📼 1.20 only the flag is set by the action press on the creature (only the player's own
  or an allied player's, `StartTapOrLockedSelect`) and stays set through action states 3..6, the button held and the
  let-go until the end packet has run: the hand pressed and held on the creature (openblack's `ApplyStartLockedSelect`
  and the creature hand's `Grab`), not hover, and not the leash (action state 21).
- **The reward, `HandStateCreature` +0xD8.** `Enter` 0x5B13F0 sets it to 0 (0x5B1431, with +0xCC, +0xD0, +0xD4,
  +0xDC, +0xE0, +0x110, +0x118, +0x11C); `Exit` 0x5B2A80 only calls `StopImmersion`. A slap (after
  `ForceIndividualAction` 0x5B1E51 and packet 0x27): k = 1 or 2 (1 when [esp+0x20] < [esp+0x54], else 2,
  0x5B1D1D..0x5B1D46; k = 1 also picks action +6 and immersion 1; the two compared values are not known), then
  `if (r > 0.25) k *= 2; r = r − (float)k × 0.1f; if (−1 > r) r = −1` (0.1f at [0x900AA4], 0x5B1ED4..0x5B1F19). A
  stroke (after `ForceIndividualAction` 0x5B2218 and packet 0x27), only when the stroke counter +0xCC ≥
  `ftol((1.0f − r) × 0.5f × 2500.0f)` (2500 at [0x900AD0], 0x5B2154..0x5B2177), the stroked bone (nearest of 9)
  differs from the last one +0x108, +0xD0 ≥ 2000 and the body action is done or past 0.8: `r = 0.1f + r`, at most 1,
  and +0xDC = 0 (0x5B2243..0x5B2266). +0xCC and +0xD0 are timers set to 0 and 1000 after each slap or stroke (that
  they count ms 📼 1.20 only). So r runs within −1..1 since the CREATURE state was entered, and is kept after the let-go until
  the next `Enter`; it is the creature's "player feedback" the hand sends on leaving state 7 (fn_0046CF80 reads
  `[CHand+0x4898]+0xD8` at 0x46CFF6 / 0x46D009 and clamps it, packet 0x59).
- **Hidden** in the sequence mode, while a script holds the widescreen (HelpSystem +0x45E8 and +0x45EC,
  0x517099..0x5170B1), under an active SetupBox (+0x94), or with alpha below 1. `HelpSystem::SetWideScreen(on, b)`
  0x5C6AD0 writes +0x45E8 = on and +0x45EC = b while on and calls `GInterface::SetActive(!(on && b))`; the same pair is
  `HelpSystem::IsDialogueControlled` 0x5C6740 without its +0x45CC test; the script side `GScript::SetWideScreen` reads
  +0x45EC at 0x6F7C16 (its arguments not read).
- **The layout of fn_00517080(damage, hunger, tiredness, reward, alpha).** W = word [0xE85058], H = word [0xE8505A];
  every division is a truncating integer one. Alpha is kept to 255 and `SetupThing::DrawAlpha` restored at the end;
  damage, hunger and tiredness are clamped with `!(v > 0) → 0, !(v < 1) → 1`, the reward the same with −1. With the
  flag f: x0 = W/40, x1 = W/4 + W/40, y0 = f ? H/4 : H/40 (0x517234..0x517263), scale = f ? 1.0f : 0.75f,
  y1 = ftol((float)(H/3) × scale + y0); `SetupThing::unadjust` takes both corners into the centred 800×600 setup
  space (at W ≥ 800 and H ≥ 600 it subtracts (W − 800)/2 and (H − 600)/2; smaller screens scale by 850/W and 650/H,
  not read whole). The box is `DrawBox(x0, y0, x1, y1, 0x5F000000, 0, 0, 0x5F000000, 0, 1)`, black at alpha 0x5F on
  the left fading right, of a fixed width (a value from the label width at [esp+0x1C], 0x5174CC, is never read).
  size = `unadjustsize(H/16)`, the text size size/2; the first row at y0 + (2·size)/3, the rows size apart, and
  size/3 more before the reward row. The label column labelW is the widest of the four labels by
  `GetTextWidth(text, (float)(size/2), 0, 1.0f)` plus (float)(size/2), worked out once while the static float
  [0xCC6314] is 0 and never reset. Rows: 3 + (f ≠ 0): the reward row only with f = 1, and then always. Per row:
  L = ftol(x0 + labelW), R = ftol((float)L + labelW); the label black at (L − size/4 + 2, y + 2), then white at
  (L − size/4, y), `DrawText(…, 800, 2, label, size/2, colour)`, right-justified 📼 1.20 only; the value black at
  (R + size/4 + 2, y + 2), white at (R + size/4, y), left-justified 📼 1.20 only; the bar `fn_00516B00(colour, L, y, R,
  y + size/2, fill, row == 3)`. Labels (help texts, 12-byte entries with the text at +8; ADDITION_n = id − 6690 📼 1.20 only):
  row 0 0x1A52 (6738), `swprintf("%d%%", ftol(damage × 100))`; row 1 0x1A66 (6758), hunger; row 2 0x1AAB (6827),
  tiredness; row 3 0x1A98 (6808), fill = r × 0.5 + 0.5, its value text 0x1A39 (6713) with `ftol(r × −100)` when
  r < −0.01f, else 0x1A63 (6755) with `ftol(r × 100)` when r > 0.01f, else 0x1A85 (6789) and " 0%" (0xBE8ECC). Bar
  colours: rows 0..2 0xFFFFFF00 (yellow); row 3 0xFFFF0000 (red) below 0.5, else 0xFF00FF00 (green).
- **fn_00516B00(colour, x0, y0, x1, y1, fill, fromMiddle)**: `DrawBevBox(x0, y0, x1, y1, 1, 0x10, −1, −1)`; dim =
  each channel less half of it, alpha 0xFF; end = ftol((float)(x1 − x0 − 6) × fill + (x0 + 3)), clamped to [x0, x1];
  from the middle mid = (x0 + x1)/2, `DrawBox(mid, y0 + 3, end, y1 − 3, dim, colour, colour, dim)` and a black
  one-pixel line at mid, else `DrawBox(x0 + 3, y0 + 3, end, y1 − 3, dim, colour, colour, dim)`; then two inner
  shadows 3 px wide, black to clear: top (x0 + 3, y0 + 3, x1 − 3, y0 + 6) and left (x0 + 3, y0 + 3, x0 + 6, y1 − 3).

### Fights: miracles, a tornado and fainting

The `LH3DCreature` fight states (+0x4994) are those of openblack's `creature_fight::State` from `Start` = 0x16 on, in
the same order (`Stance` 0x17, `Action` 0x18, `BlockStart` 0x1A, `Block` 0x1B, `BlockRecoil` 0x1C, `BlockEnd` 0x1D,
`Faint` 0x1E, `CastStart` 0x20, `Cast` 0x21, `CastEnd` 0x22): every test below fits that order (🔍 inferred from the
fit; no table names them). The faint's states are confirmed by their writers: 0x1E the fall, 0x24 lying fainted
(openblack's `Lying`, last in its list), 0x1F getting up (openblack's `GetUp`); 0x23 is a turn on the spot, not a
fight state ([Struck by a thrown thing](hits-and-catch.md#struck-by-a-thrown-thing)). The cast states (CastStart
0x4852A8, Cast 0x485575, CastEnd 0x4853C9), the fight's spells and the blow that ends an opponent's cast are on
[the fight's cast](casting.md#the-fights-cast).

- **Blocking** fn_0048A580: state 0x1B; 0x1A once its time (+0x47D0) is past half of animation 0x8E (142, start
  block); 0x1D while before that half.
- **A miracle's blow in a fight**, in `Creature::ApplyEffect` 0x478C80: a harmless effect from anything but the
  creature itself or its own player does nothing to a fighter (0x478CA9..0x478CC6); harm from itself does nothing
  (0x478CD5). Fighting, unless [0xC6414C] or [0xC6414D] (not named) is set: blocking, the effect's numbers are taken at
  0.1 (0x478D24); then fn_00484790(184) (0x478D3F); a heal goes to the fight health +0x4AA8 (clamped to 0..1) and
  `TimeWarpHeal(heal × 9600)`, harm comes off it, and at 0 or below the creature faints, its opponent (+0x528C) being
  made to want to show off (desire 0x18, action 0x27, or 0x13 when its `LH3DCreature` +0x9C, read as the alignment,
  is below −0.5) and counting a win (+0x17C).
- **The reel** fn_00484790(animation): nothing while fn_00489CB0 holds (the fight animation +0x49A0 in 179..193, the
  reels, or state 0x1C or 0x1E); blocking, animation 198 and state 0x1C; in state 0x17, 0x18, 0x20, 0x21 or 0x22 the
  animation given and state 0x18 (jump table 0x48481C / 0x484824); otherwise nothing.
- **Fainting** `Creature::Faint` 0x476FA0: nothing if fainted already (+0x384); the spells on it brought to their end
  (fn_004F4B60 on `CreatureReceiveSpell` +0x370: each of the 16 slots' +0x10 = 0, and +0x8 = 0 for one in phase 3,
  holding; true when none is left), `EndAnyAnimsRapidly`, `StopMoving`, the leash let go (`GLeashStatus::SetOn`), a
  forced plan (desire 0x18, action 9), mind +0x1C14 = 1, fainted = 1, fn_004C9FE0(0x25, 3, 0, 0, 0). There is no fight
  code in it. Its callers: `DestroyedByEffect` 0x476F7A, `ApplyEffect` (0x478EF2, 0x478F06), fn_0047CF00 (three),
  `ReduceLife` 0x47DDF5, and fn_00477060, a jump to it that the tornado's pick-up fn_006D21B0 calls for a creature
  (0x6D2464).
- **A faint in a fight** 📼 1.20 only: the forced plan ends the fight. `ForceActivityAndForceAction` 0x4C44D2 →
  `FinishActionUnsuccessfully` ("stopping because new action forcing me", [0x9D9744]) → `FinishActionGeneric`
  0x475880 → `StopWhatIAmDoing` → fn_0048F710 (0x475B76) → fn_004845F0 (+0x528C = 0, the linger in the fight states,
  the life loss); then, with mind +0x1BE8 set, it clears it and +0x20D30 and stops the partner ("Stopping because my
  partner told me to", [0x9CFA7C], 0x4758A6..0x4758C1), whose own stop ends its side the same turn. No win (+0x17C),
  remark or reaction 0x28.
- **The two creatures' spells.** The sub-state 0x508C10 waits until both creatures' spells are over; which activity
  runs it is not decoded (its table at [0xBE5A28]).
- **A creature's defence and the pen**: the defence divisor (`Creature` 0x478C00, (clamp(size, 0.001, 2) × 1.5 + 1))
  takes LH3DCreature +0x90, the size it is drawn at, so in its temple's pen the pen size.

## openblack

The fight camera, the panel, blocking, the reel and fainting are done; the fight's miracle side (`ApplyEffect`,
inventory R22) is not wired yet. Porting decisions: [Port decisions: the creature](../project/port-decisions-creature.md).

### The fight start and the camera

The camera's side of the watch (the orbit, `camera::FightWatch` in `src/Camera/FightOrbit`) is on
[the player camera: watching a fight, the camera's side](../camera/player-camera.md#openblack); this section is the
fight system's side.

- `creature_fight::ViewAtFightStart` is the rule; `CreatureFightSystem::StartFight` asks it through the agreed
  `camera::FightWatch` that the player's camera model gives (`CameraModel::GetFightWatch`, null for every other model)
  and either calls `FightWatch::StartFight` with the two fighters (the one that made the arena first), the arena's
  middle on the ground and its radius, or plays `audio::guidance::RemarkCreatureFight`. Not while a script camera mode
  is on or the script has the screen (`interface_active`), which stands for the failed `dynamic_cast`. The fight
  system no longer moves the camera itself: the flight to the arena and the following of the fighters are the
  camera's (0x45A5B2.., `Update` 0x45E37E..0x45E8D6). The debug window's camera switch keeps only the watching off.
  openblack asks once a fight.
- **The arenas:** `CreatureFightSystemInterface::GetArenas` lists the fights now on, newest first, each as an
  `ArenaView` (maker, opponent, middle, radius), from the `CreatureFighting` of the creature that made the arena
  (`madeArena`), read through the const registry. The view's middle is `map_coords::Quantise` of the float middle
  `MakeArena` keeps (ToFixed then ToMetres), the position as the original's arena stores it; its height is left to the
  reader (the land under it, as the altitude is 0); `ArenaOf` gives the arena a creature made or was called into. The
  order comes from `creature_fight::Arena::serial`, counted by the fight system as each arena is made. Nothing calls
  them yet (the camera will); the count is only written where an arena is made, and no part of the turn state hash
  reads `CreatureFighting`'s contents, so the verification runs are identical. openblack makes an arena per fight at
  the float middle, with no reuse, no shrink and no sound.
- **The radius and the range:** fn_00424FB0's constants are openblack's, on the drawn size where openblack passes
  `Creature::size`. `CheckFightQueue`'s 0.8 R is openblack's `TakeOrder`; openblack starts every step and clamps the
  movement to the range (`ClampToRange`).
- **The end:** fn_004845F0's life loss is openblack's `EndFightFor` (`creature_fight::LifeAfterFight`), so
  `EndFightFor` tells the camera (`FightWatch::EndFight`) when `creature_fight::CameraLingersOnEnd` holds for the
  fighter's state. Of the running sub-state's three ends only (c) is matched: a duel whose opponent has gone or left
  the duel (`ProcessDuels`) calls `EndFightFor` and then, for the local player's creature, `EndFight`.
- **The fight music:** `GameQueries::creatureFightMusic` is assigned (`CreatureFightMusic`, the arena branch only;
  [GameMusic::ProcessMusic and its sources](../audio/music.md#gamemusic-processmusic-and-its-sources)); the 0x10 branch waits
  for the interface states (F-86).
- **No fight on Land 1 or Land 2:** the player's creature is made by `player_creature::LoadMyCreature`, never a second
  one, so the verification runs are identical.

### The panel

`CreatureFightSystem::GetPanel` gives the panel of the fight the player's camera watches
(`camera::FightWatch::IsWatchingFight`, lingering too, and `GetWatchedFighter`), while the fight's arena is there
(`ArenaOf`): its maker first, then its opponent. `Game::UpdateHandInterface` hands it to the interface each frame
(none in the temple), which draws it with `creature_fight_hud`'s layout. `creature_fight_hud::Compute` uses screen
shares and a red-yellow-green blend, not the original's whole pixels.

### Miracles, blocking and fainting

- `IsBlocking`, `Recoil` and `ForceFaint` are on the fight interface. Blocking is `creature_fight::IsBlocking`, asked
  through `CreatureFightSystem::IsBlocking`; the reel is `CreatureFightSystem::Recoil` (animation 184, `k_RecoilMid`,
  as `ApplyEffect` passes).
- `ForceFaint` is `Creature::Faint` from outside a fight's loss: fainted already, nothing; a fight it is in is left
  with no winner (`Leave`: no life loss, no linger; the opponent ends only at its next `ProcessDuels`), then it faints
  as the knocked-out do. Every faint now brings the spells on it to their end (`creature_spells::TryFinishAll`,
  fn_004F4B60). `ForceFaint` is called by an effect that destroys a creature that can die (`effect_values`,
  `DestroyedByEffect` 0x476F7A) and by the tornado's pick-up for a creature it reaches
  (`TornadoSystem::CatchCreature`, fn_00477060). On Land 1 and Land 2 no creature has a spell on it, so a faint there
  is as before.
- Nothing calls `IsBlocking` or `Recoil` yet; the miracle's side (`ApplyEffect`, R22) calls them in its own commit.
  `IsCameraOnFight` answers from the player's camera (`FightWatch::IsWatchingFight`, the original's `HasFight`); its
  reader is the camera's port.
- `CreatureKnockedOut::Stage::WaitingForSpells` and `CreatureKnockedOut::rest`, two additions of the imported code,
  are not taken.
- The defence divisor takes `creature_pose::DrawnSize`, the size the creature is drawn at.

### Test hooks

- The debug window's camera switch turns the watching of fights off.
- The testbed's `miracles.hand_grips_casting_creature` shows the held creature's status panel; `hand.status_panel`
  cannot yet (its `HandStroke`, `HandSlap` and `HandLetGo` commands need `CreatureHandSystemInterface::Stroke`, `Slap`
  and `IsHeldByCommand`).

## Pending

- **The body's size outside the turn**:
  - a script's height (`Creature::SetHeight` 0x47DE80: +0x90 = h/15 at once, unclamped) and `SET_POSITION`
    (`ForceMoveMapObjectWithoutWalking` 0x479EE3: the body's own size until the next turn) reach the body in the
    original; queued as a fix;
  - the breath period: `SetSize` 0x480628..0x48063C writes 5·√size as current and target each turn, ours eases;
    queued as a fix;
  - `SpellCreator` and the script GET SCALE / GET HEIGHT read the body's size (0x4F8260): they wait on the drawn size
    being kept apart from `Creature::size`;
  - readers read now as the drawn size (+0x90): the arena radius ([Fights: the camera](#fights-the-camera)), the
    leash's heights and the anger reach (`GetHeight` 0x477F50), the look point
    ([What a creature looks at](mind.md#what-a-creature-looks-at)), the collar
    ([The leash's collar end](leash-rope.md#the-leashs-collar-end)), each queued with its own fix. Still not traced to +0x6C or
    +0x90: the fight's places and faint time, the beliefs, the reactions, the throws, poo and puke, and the eyes.
- **The fight camera** ([Fights: the camera](#fights-the-camera)):
  - which moment of our fight 0x5036C0 is: it runs once the 3D start 0x484410 took the arena; whether the opponent
    also runs it (and so asks the camera twice) depends on mind +0x1BE8 (the partner pointer RequestPartner sets), not traced;
  - the end: which of our ends is (a) or (b) needs our fight states mapped to the 3D states 0x16..0x22; the remarks of
    (b) and (c), reaction 0x28 and fn_0048F710's call of fn_004845F0 are not ported;
  - the fight music: the interface state 0x10 branch waits for the interface states (F-86). Which interface state index the
    handler of 0x5D4B16 (at [0xD180A8]) belongs to is not computed;
  - the fight's panel: fn_00516CB0 / fn_00516B00 in whole pixels (u = screen height / 70); queued as a fix. How long
    the panel stays in the linger depends on when an arena is deleted after a fight (`GArena::ToBeDeleted` 0x424960 is
    reached only virtually, as GArena vt+0xC, and none of the 527 `call [reg+0xC]` sites is in an arena or fight
    function: the arena owner's delete path is still to trace);
  - the status panel's whole-pixel layout (H/16, size/2, the rows, the bars' ends): queued as a fix; which of the
    follow camera's makers is Creature Mode's (`GGame::DoAction` 0x63FB4B, `CameraModeNew3::Update` 0x45E34B and
    others) is not known;
  - the arena list: an arena near the place within d is reused at the smaller radius, a new one shrinks until a free
    place is found, and a made arena is deleted at the end (fn_00424E70, `GArena::ToBeDeleted` 0x424960); queued as a
    fix. The script's arenas (`GET_ARENA`, `CREATE_ARENA`) are not in `GetArenas`; d's field ([3D+0x5198]+0x6403C)
    is not identified;
  - the arena's middle: the original's sums read the quantised MapCoords (0x5074B1..0x50750C, `StartFight`
    0x45A646..0x45A68B); queued as a fix. How the camera turns `GetPos` back into metres is not read;
  - the range: openblack starts every step and clamps the movement (`ClampToRange`) where the original refuses the
    steps beyond 0.8 R; queued with the arena list.
- **Fainting and the fight's additions**
  ([Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting)):
  - a faint by other causes ends both fighters' fights in the same turn with no winner (the forced plan's
    `FinishActionGeneric` → `StopWhatIAmDoing` → fn_004845F0, and the partner's stop); queued as a fix;
  - which activity runs the sub-state 0x508C10 (its table at [0xBE5A28]) is not decoded;
  - the flags [0xC6414C] / [0xC6414D] and `ApplyEffect`'s fight effects go with the miracle's side (R22).

### Watching a fight, the camera's side, not ported

- the arena's own altitude for the arenas a script makes (`GScript::GetArena` 0x6F4C36, not read): openblack's flight
  takes the land's height at the centre, which is right for the creature's arenas (altitude 0);
- the arena's centre is quantised for the flight only: the in-view test and the drag away read the centre the fight
  side gives (the fighters' float middle), where the original reads `GetPos` (the quantised one) everywhere;
- the zone test of `FlyToPosFoc` (`InsideInclusion`) on the fight's flight, as for every openblack flight;
- the two starts of the camera's own, lingering over an arena for 1000 ms and the double click onto one: openblack has
  no query of the arenas (the list g_game +0x205C7C); TimeInArena is kept and reset as the original does, unread;
- the middle's raise by a quarter of fn_004867B0 for each fighter (fn_004813B0's point.y less LH3DCreature vt +0x28 of
  it; neither is read);
- the keyboard move's own needs: the original applies it only with feature 8 and in mouse mode 2
  (0x45C943..0x45CAA7), and reports Drag 0x308 for it with feature 8 (0x45C986); openblack's `TiltZoom` applies it
  without either test (the orbit, when it runs, still replaces it), and no Drag goes to the camera help for it;
- g_game +0x250538 (fn_005D2990, the interface's fight state 0x10 for the fight music and sound filter), and the other
  readers of HasFight: 0x5D4B16, in sub_5D4AC0, the fight state's interface action (its pointer stored at [0xD180A8]
  by the InterfaceAction table's init, 0x5D0DA9): it leaves the state through `ResetActionState` (0x5D4BF5) as soon as
  the fight camera or fn_005D2990 fails, and its presses (+0x39 & 1 or & 2) clear the gesture buffer (g_game +0x25006C)
  and start the fight gestures 0x23 / 0x22 on the creature under the hand (sub_5D3EF0), else sub_5D2C40 / sub_5D2E60
  [V]; `HandStateHolding::Update` 0x5B5F61, which notes a fight camera in a local flag [V]; the draw fn_00456270 from 0x5CCCCC;
- the flags +0x4C..+0x4E (set by the turn, the tilt and the edge rotate; cleared each frame in modes 0 / 2), with no
  reader found;
- the mode seconds are not set to 0 by openblack's camera models outside the script camera (Creature Mode's, the
  temple's); which of the original's mode switches those models stand for is not read, so the orbit's ease time after
  them is not known to match;
- the arenas' radii in play, not measured; `GArena::IsAvailable` (vt +0x2C) is approximated by the fighters being
  there;
- ⚠️ approximate the in-view test reads the camera's current origin without the shake, where the original reads
  `LH3DCamera::GetPos`;
- the creature's own radius (LH3DCreature +0x5228) and so the orbit's distance: `ecs::object::GetRadius` stands in (its
  creature branch is not ported);
- a fighter's radius of 0 (no loaded mesh in openblack; the original's creatures always have a radius from
  `LH3DCreature::SetSize`), which openblack takes as the original does, IEEE floats under /fp:precise with no trapping
  (a `static_assert` on `is_iec559` beside the division). The original, at rB = 0, divides in floats without a crash,
  and only in frames with a zoom (the division is skipped when [esp+0xC8] == 0); without a zoom it watches on at the
  distance d + rA. With a zoom out the numerator is about 0.3 z > 0, so FD = +inf and FD > 40 ends the watch on that
  same frame (0x45E538), then FD is clamped to 40; with a zoom in FD = −inf, which does not end it (FD ≤ 0 → 0); and
  when the numerator rounds to exactly 0 (0.3 z lost against d + rA) FD = 0 / 0 = NaN, which compares false with 40
  (fcomp unordered, so no end) and then clamps to 0. rA = 0 is never divided by and the original watches on. Ours
  matches it: the end is the same `FD > 40`, and the clamps make the original's two comparisons in its order (not
  above 0 → 0, then below 40 or 40; not above the lower pitch → it, then below the upper or it), so not a number goes
  to 0 as there. Tests: `WorldCameraFight.ASecondFighterWithNoRadius*`,
  `FightOrbit.TheDistanceCountsTheSecondFightersRadii`.
