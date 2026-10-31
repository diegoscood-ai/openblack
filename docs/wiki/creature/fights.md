# Fights

## Summary

Two creatures fight in an arena. The original has no fight camera mode: the player's own camera keeps a fight state,
and when the fight starts it either watches it (flies to the arena and orbits the fighters) or only plays the spirits'
remark. The watched fight shows a panel of the two fighters' health and stamina; at its end the camera lingers 3 s.
Inside a fight, a miracle's blow goes to the fight health (taken at 0.1 while blocking) and makes the creature reel;
a faint, from a fight or from anything else (a destroying effect, the tornado), ends the fight.

openblack has the fight start's watch rule, the arenas list, the camera's watch and linger through the agreed
`camera::FightWatch`, the panel, blocking, the reel, fainting, the scripts' fight natives and the player's presses in a
fight. Land 1's creature lessons make the Guide's creature and fight it with the player's (`CreatureDevGuideTeachesFight`);
the verification runs end long before that, so none reaches a fight. This page owns the fight camera; the player
camera's other modes are in [The player camera](../camera/player-camera.md).

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
  both, and d = [[3D+0x5198]+0x6403C] × 10 (the route follower's target speed). fn_00424E70(pos, a, b, d): r = fn_00424FB0
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
    0x5039B6): the `GeneralBad` remark fn_0071CD70, fn_004845F0, then the linger. Ours: the good remark in
    `CreatureFightSystem::Win` for the local player's winner not controlled by a script, the bad one where a duel ends
    because its opponent has gone; reaction 0x28 is not made yet.
  `EndFightNow` is never called by the fight.
- **No time limit** ✅ v1.42 by xref: the running sub-state 0x503830 returns 0 (goes on) while the body's opponent
  +0x528C is set and the opponent's life is above 0 (or it is script-held); it counts no time. `EndFightSoon`
  0x45A800 is the camera's linger only (all four callers pass 1). So a fight nobody directs, both sides fought by the
  computer, ends only when a landed blow takes a fight health to 0 or less (`DoFightActionAction` 0x48A5E0: loss
  0.05 high, 0.03 middle and low, × the force, × 0.1 blocked, through `ReduceFightHealth` 0x480380), the winner's
  show-off plan and the loser's `Faint`, or when something outside stops it.
- **The fight music** 📼 1.20 only: `GInterface::ResetActionState` sets interface state 0x10 when vt+0x40C holds, the camera
  mode is a `CameraModeNew3` with `HasFight` (+0x44, read at 0x5D2A3A), fn_005D2990 holds (g_game+0x250538 set and
  the interface player's creature fighting, `IsFighting` 0x47B1C0: 3D +0x528C ≠ 0) and the hand holds nothing
  (`GetFirstObjectInCurrentHand` 0x5DC130). fn_00427660 plays the fight music when the local player has a creature
  (+0xA64) and either that state is 0x10 or an arena in use (+0x34) with that creature as a fighter (fn_00425060) lies
  under 100 ([0x8AB41C]) from the camera's point [0xEA1DB8]
  ([GameMusic::ProcessMusic and its sources](../audio/music.md#gamemusic-processmusic-and-its-sources)).
- **Can it happen on Land 1 or Land 2?** Yes, on Land 1. Its creature lessons make the Guide's creature
  (`CreateGuideCreature`, refs/chl/CreatureGuide.txt:23404, `create 12 …`), and `CreatureDevGuideTeachesFight`
  (:25492, run from LandControl1.txt:68102) forces a fight between it and the player's creature, turns both creatures'
  own fighting off (:26001, :26004) and waits for the player's own moves: the Guide reeling from a blow (:26093), more
  than two blows queued (:26215), a block (:26336) and a step (:26502). Only the player's presses
  ([The player's presses](#fights-the-players-presses)) get the lesson past them. The other makers of a second
  creature are the playground scripts' `CREATE_CREATURE_FROM_FILE` and the debug spawner.

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
  HasFight, 0x45A980; openblack: `DefaultWorldCameraModel::Update` with `fight_orbit::ArenaUnderView` over
  `CreatureFightSystemInterface::GetArenas`, the time in `_fight.overArenaMs`, then `StartFight`); and the double
  click's flight onto an arena 0x45DFC5..0x45E086 (feature 0x10; not the give-up flight [esp+0x4B], not fighting
  +0x44; for each arena of the list g_game +0x205C7C, linked by +0x48, its map position (vt +0x100, x and z × the map
  scale; the altitude `GetAltitude` 0x803090 read and dropped) against the target's x and z: d = √(dx² + dz²) as a
  float, and the first arena whose radius (vt +0x60) is greater than d, whoever is in it, gets `StartFight(arena)`
  0x45A4D0 and the flag [esp+0xC3] (the follow of the object double clicked, `CameraModeFollow` 0x45E321) cleared;
  the flight to the point goes on after it, 0x45E08B; openblack: `UpdateModeFlying` with `fight_orbit::ArenaAtPoint`,
  before its own test of the screen-centre hit; the follow itself is not ported).
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
  ends it: the drag away needs mode & 1). The flag reads only the move inputs, not the turn [0xC5B0C8], tilt
  [0xC5B0C4] or zoom [0xC5B0CC]; and the move inputs are written only in `ProcessKeyMovement`'s branches without a
  modifier (0x4583E1, 0x458456, 0x4584BC, 0x458530; the edge scroll writes none), while during a watch
  `IsRotateKeyModifierActive` 0x457F30 is on (HasFight and the arena) unless ZOOM_ON, which zooms and turns. So with
  the watch on when the keys are read, a movement key tilts or turns (or zooms) and never moves the camera over the
  land: the skip is reached only when the keys were read without the watch and the next `Update` runs it (the keys
  are read after the update, `GGame::Loop`: a watch that starts between). Checked 2026-10-24.
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
  +0x45EC at 0x6F7C16 (its arguments not read). The panel does not read the interface's active flag: `SetActive` has
  four other callers (`DialogBoxBase::HideAll` 0x51373D, `InterfaceActionValidateObjectsForStates` 0x5D1050,
  `StartPlayBack` 0x5DAE3D, `EndPlayBack` 0x5DB41C), so "interface inactive" and "a script's wide screen" differ, as
  during a hand demo under a script's bars.
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

### Fights: a fight a script forces

✅ v1.42 by xref, with bw1-decomp `CreatureAction.cpp` for the builder's body. A script's `CREATURE_DO_ACTION`
(creature, 21, the other creature) forces the action row 21 `Fight` (`CREATURE_FIGHT`), as every forced action is
([An action forced by a script](script-control.md#an-action-forced-by-a-script-creature_do_action)).

- **The builder** `CreatureAgenda::ConstructSubActionsForFight` 0x49BCD0 (its address is in the action table, at
  0x492436; its one direct caller is `ConstructSubActionsForTellCreatureToSodOff` 0x4B947C). As it builds, it sets:
  creature +0x3B8 = 1 and `FightStarted` = 1 (neither reader read); the lean: the mind's learning +0x17D04, first set to
  −alignment when the creature has never learnt the fight (learning +0x15C40[21] = 0), copied to the 3D body's
  +0x4AB0; then, with the other thing a creature (else `FinishActionUnsuccessfully("no other creature")`), for each of
  the two the fight health +0x4AA8 = (life + 1) / 2, the stamina +0x4AAC = energy − exhaustion kept in 0..1, and who
  fights for it +0x579C = 1 for a human player's creature, 2 for any other.
- **Its sub-actions**, in order: REQUEST_PARTNER (the other, request 2) with `LookAtObjectArgument` and the angry face
  `SetFaceForActionAnger`; DEMOBILISE_OBJECT (the other); the other ordered to TURN_TO_FACE_OBJECT it, and it turns to
  face the other (0.1); both play an individual action, 0xDE or 0x46 by a draw of 2 each; CREATE_ARENA; POINT_AT_ARENA;
  both MOVE_TO_ARENA, looking at the partner; WAIT_FOR_OBJECT (the other); both TURN_TO_FACE_OBJECT again; then the
  other ordered to FIGHT, and the main sub-action FIGHT (the other); both RESPOND_AFTER_FIGHT; REMOBILISE_OBJECT.
- **The FIGHT sub-state's start** sub_5036C0 (registered by `crt_xc_fn_CreatureSubAction_004F8D70`, 0x4FA01F) calls
  `LH3DCreature::StartFighting` 0x484410 at 0x5037A6, its only caller, which sets the opponent +0x528C: from there
  `IS_FIGHTING` is true. The same start tells the player's camera (`CameraModeNew3::StartFight`) and makes a reaction
  (`Reaction::CreateReaction`).
- **No desire picks it by itself here**: the anger that picks a fight goes through the fights' own start
  ([The fight start and the camera](#the-fight-start-and-the-camera)).

### Fights: the player's presses

✅ v1.42 by xref and the oracle (2026-10-29): Ghidra's C for the interface (bw1-decomp has none of it) and bw1-decomp
`Creature3D.cpp` for the 3D body's side.

- **The fight state.** `GInterface::ResetActionState` 0x5D29C0 puts an idle interface in action state 0x10 while the
  player's camera mode is a `CameraModeNew3` that `HasFight`, the interface's player's creature fights (fn_005D2990:
  `Creature::IsFighting`) and the hand holds nothing (see the fight music above). Its process sub_5D4AC0 (the table's
  row 16 at [0xD180A8], set by the table's init 0x5D0890) leaves it through `ResetActionState` as soon as the camera or
  the duel fails; otherwise a press of the grab button (+0x39 & 1, message 1) or else of the action button (+0x39 & 2,
  message 3) clears the gesture buffer and goes to the fight, and to nothing else: no land grip, no pick-up, no tap, no
  leash. A double click is the idle state's, so it is not taken either.
- **On a thing.** With an object under the hand (the action collide's +0x400, valid as an interface target, vt+0x6F0)
  the press goes to sub_5D3EF0 (the object, 0x23 for the grab button, 0x22 for the action button) and ends there
  whatever happens: nothing for a thing that is not a creature, nor for a creature other than the player's own
  (player +0xA64) or its opponent (its mind's +0x1BE8), nor while its 3D body's twelve marks are taken (+0x4B30 ≥ 12).
  The line from the camera through the cursor must meet the pressed creature's current mesh
  (`GetNearestIntersection` 0x865020). On the player's creature the move is 3, a block; on its opponent a blow by how
  far up the hit is over its head height, (hit y − its y) / `GetHeadHeight` 0x4867B0: below 0.33 (float [0x8CA268]) 2,
  low; above 0.66 ([0x8CF128]) 0, high; else 1. Then, only while no apply waits (+0x444 = 0): the mark
  (`AddFightIntersection`), the packet (fn_00550BE0: the creature, the move), the press's render time (+0x454) and turn
  (+0x458), action state 0x17 (grab) or 0x16 (action), and a sound tag at the creature's MapCoords (`SoundTag::Create`
  sample 1, no track, mode 3, no loops, 2D, InGame).
- **On the ground.** With nothing under the hand, `SendFightSelectPoint` 0x5D2C40 (grab) or `SendFightApplyPoint`
  0x5D2E60 (action) at the interface's action point (+0x3F0): nothing while an apply waits or with no fighting creature;
  `FailApply` when the point is off the map or the creature no longer fights; nothing outside the arena (the 3D body's
  centre +0x4A98 and radius +0x4AA4: a radius at or within the distance refuses) or with more than eleven marks taken;
  else the mark, packet 0x0F or 0x10 with the point, the apply's turn (+0x444, fn_005D36A0) and the sound tag at the
  point. The arena part of these functions casts the creature to a `GArena`, which always fails (+0x44 is never
  written).
- **The let go.** States 0x17 and 0x16 (0x5D5650, 0x5D5670) wait for their own button's release (+0x39 & 0x20, & 0x40),
  then sub_5D4650, also the states' end: packet 0x24 with min(render time − the press's, (turn − the press's + 1) × 100),
  `ValidateLastFightIntersection` and `ResetActionState`. The render time is the visual clock g_game +0x25053C.
- **The turn** (`GPacket::ProcessPacket`, jump table 0x63DDCC): 0x22 fn_005DAAD0 and 0x23 fn_005DAB50, while the thing
  pressed on is interactable and the player has a creature, `AddToFightQueue(move, add / replace)` on it; 0x24
  fn_005DAB20 `ValidateFightEvent(held)`; 0x0F / 0x10 (`Pkt_FightSelectPoint` 0x5DA4D0, `Pkt_FightApplyPoint` 0x5DA590)
  note the apply's handled turn (+0x448), then with the creature there, available and fighting
  `SelectFightThisToMapCoord` 0x4C6460 / `ApplyFightThisToMapCoord` 0x4C64D0, `FightClickGround(point, replace / add)`;
  else the spot visual 0xB (a failed command) at the point.
- **The step** (`FightClickGround` 0x489B90, bw1-decomp Creature3D.cpp:4831): the way to the point turned by −Heading
  (the cosine stored as a float, the sine kept unrounded for the first axis): forward when −z′ > |x′|, back when
  z′ > |x′|, right when −x′ > |z′|, else left, each `AddToFightQueue(0x8000 | the step's animation)`. Oracle
  2026-10-29: 22 cases, pinned in `CreatureFight.AGroundPressStepsAsTheOriginal`.

### Fights: the arena's ring

✅ v1.42 by xref and the oracle (2026-10-29).

- **When.** `GGame::Draw` runs fn_00424D10, which calls `GArena::Draw` 0x4249E0 for every arena of the game's list,
  each frame. An arena whose fight is on (+0x34, set by fn_00424AB0 as its fighters are stored, cleared by fn_00424B60)
  makes its sheet the first time (+0x38 → `new(0x84)`, the game's `operator new` 0x7DB790, which is `calloc`: the
  memory starts cleared), and draws it every frame it is on; with the fight off nothing is drawn, the sheet kept.
- **What.** `GLightSheet::GLightSheet` 0x4244D0 (the arena's MapCoords as metres, its radius +0x30, the colour
  0xFFFFFFFF, the height radius × 0.16666667, the spread 1.01 (0x3F8147AE) and 32 points; pushed at 0x424A16..0x424A5C).
  `LightSheet::Init` 0x83E710 sets the shift every 0.03 s (+0x18, 0x3CF5C28F), the slide speed 1.0, the default
  spread 1.1 and height 10, which the arena's then replace (+0x10, +0x14). The points (fn_00424550): for i of 32,
  angle = i × 6.2831855 / 31 at float precision, x = cos stored as a float × radius + the middle's x, z = sin × radius
  + the middle's z, so the last point closes on the first; each on the land at the MapCoords
  ((x × 65536) × 0.1, (z × 65536) × 0.1) truncated (`GetAltitude`). Oracle 2026-10-29: four arenas, 128 points, all
  matched. The arena's earlier note "a grey 180 light" was wrong: the colour is white.
- **Each frame** (`GLightSheet::Draw` 0x424650): the colour masked to 0xFFFFFF, the strength fed in (+0x38) the
  arena's +0x80, 1.0 from the constructor, the sheet's `Update` by the frame's game time (`g_game_time_inc` × 0.001),
  and its Z object at its middle (+0x4, zero until it is first built).

### Fights: the marks of the presses

✅ v1.42 by xref (2026-10-29), bw1-decomp `Creature3D.cpp` for the 3D body's side and the frozen exe's
`DrawFightSparkles` 0x48DD70 for the colours.

- **The list** (the 3D body of the creature that presses, +0x4B30 its count, at most 12): `AddFightIntersection`
  0x48A420 (a mesh hit: kind 0 on the opponent, 1 on its own skin; the press now held, `LastFightIntersectionPending`)
  and 0x48A490 (the ground: kind 2, the point (+0x4E34) and the point in the creature's frame (+0x4DA4)), each with a
  held time (+0x4B64) and a frame (+0x4B94) of 0. A body press marks only while fewer than 12 show and no apply waits;
  a ground press only while 11 or fewer show, and only for the local player's creature.
- **What takes them away.** `AddToFightQueue` 0x489A20 with replace: only the newest stays, now first. `ShuntFightQueue`
  0x489930, as each move is taken: the oldest goes. `ReceivedFightImpact3d` 0x48B6E0, a hit taking the waiting blow:
  for the local player's creature, the newest goes and its press is no longer held. `ValidateLastFightIntersection`
  0x48A570, as the press is let go: no longer held.
- **Each turn** of the creature (Creature3D.cpp:1440..1460): the held mark counts the creature's turn milliseconds, up
  to 1200; for the local player's creature with an opponent, each ground mark's point is its frame point turned and
  moved by the creature's matrix (z, then y, then x, then the place).
- **Each frame** (`DrawFightSparkles`, from `GGame::Process3dEngine`, with an opponent): a mark on the opponent is its
  spot on the opponent's mesh as posed now (`GetWorldPositionAndInwardNormal`), its frame stepped (frame + held × 8 /
  1200) & 15; on its own skin, its spot there, frame 0; on the ground, the turn's point plus (the frame point by the
  matrix now − it) × the turn fraction. The share (12 − i) / 12, against the doubles 0.33 and 0.66 ([0x8CF1B0],
  [0x8CF198]): below a third red = share × 3.0303… × 255, else below two thirds red 255 and green = (share − 0.33) ×
  3.0303… × 255, else red and green 255 and blue = (share − 0.66) × 3.0303… × 255, each truncated; the colour is
  ((red | 0xFFFFA000) << 8 | green) << 8 | blue, so the first's blue of 262 spills into the green. Drawn half way to
  the camera, the creature's drawn size (+0x90), by `DrawASparkle` 0x8465A0: the liquid particles' sprite, the
  additive material of data\blobs.raw (mode 0xD, made by 0x845DF0), its cell the frame.

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
  (0x478CD5). Fighting, unless [0xC6414C] (set around a fight blow's own effect, `ReceivedFightImpact` 0x479068) or
  [0xC6414D] (set around a thing striking it in the physics, `ReactToPhysicsImpact` 0x479AF4) is set: blocking, the
  effect's numbers are taken at 0.1 (0x478D24), but the harm was read before and is taken whole; then fn_00484790(184)
  (0x478D3F); a heal goes to the fight health +0x4AA8 (clamped to 0..1) and
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

### Fights: the move queue and the lean

- **The lean a move teaches** ✅ `CheckFightQueue` 0x489540 (v1.42), as the move is taken from the front of the queue
  (+0x4AC4) in the stance (state 0x17): the lean is the mind's learning +0x17D04 (bw1-decomp `field_0x1522c`), kept
  in −1..1. A step (the 0x8000 bit) is taken by `StartFightAction(value & 0x3FFF)` and teaches **every** creature
  towards defence, × 0.98 − 0.02 (0x48964E..0x48967E, [0x8CF17C] 0.98, [0x8CF178] 0.02), with no test of the player;
  a miracle (the 0x4000 bit, 0x4896DA..) teaches nothing; a blow (0, 1, 2), a block (3) and the special move (4) teach
  only the human player's creature (`MyPlayer() == GetPlayer()`): a block towards defence (0x4897AB..0x4897C9), the
  rest towards attack (0x489856..0x489874).

### Fights: the blows' force and the computer

- **A blow's force** ✅ `DoFightActionAction` 0x48A5E0 (v1.42 0x48B064..0x48B0CA; bw1-decomp `Creature3D.cpp`
  `DoFightActionAction`): (W + 3) / (W' + 3) ([0x8C2C50] 3), times size +0x90 / size' +0x90, times the speed +0x49A4,
  in that order; at or below 0.04 ([0x8CF190]) it is 0.04, not below 2 ([0x8AB478]) it is 2; then `fadd st0, st0`
  doubles it only when the animation +0x49A0 is 130 (`cmp 0x82` at 0x48B0BE). W is the weak-strong target 3D +0xAC,
  clamp(`ConvertStrengthTo3D` 0x4F02B0 (strength, species), −1, 1) = 2 × (0.8 × strength + 0.2 × the species'
  strength) − 1 ([body](body.md)); its only readers are `GetMass` and these two loads. The oracle's 0x4F02B0 for a
  species of 0.6: strength 0.37 → 0xBE2C0830, 0 → 0xBF428F5C, 1 → 0x3F570A3E, 0.9 → 0x3F2E147A.
- **The reach of the first special move** ✅ the same function (0x48A8F4): the swing's puff (2 × size × the scale)
  and the radius a blow lands within (scale × size × 0.2 × 15) take the scale 2 only for animation 130, else 1. The
  second special move, 136, is neither doubled nor reaches further.
- **The computer's wait before it takes over** ✅ +0x4AB8, whole milliseconds, while +0x579C is 1 (the player with
  the computer waiting). `StateAction` 0x486A00 takes the creature's own turn milliseconds (+0x48B4, bw1-decomp
  `IntTimeInc`) off it in four states: the block (0x486AE0, 143), the reel in a block (0x486E37, 198), the stance
  (0x487293, before `DoFightMainAction`) and an action, a blow, a step or a reel (0x4873D5, before
  `DoFightActionAction`). Only the block and the stance hand over (`jns`: below 0, +0x579C = 2); in an action or a
  reel in a block it goes on below 0, and the computer takes over as soon as the creature is back in its stance or its
  block. The block's start and end, the casts and the faint do not count. 15000 ([0x8CF0AC] through `__ftol`) is
  written by `AddToFightQueue` 0x489A43 (also turning 2 into 1) and by `CheckFightQueue` 0x4895AE, which the stance
  and the block call only when +0x579C ≠ 2: there, with the opponent within `ArenaEdgeFraction` of the arena's radius
  of its middle and a move queued, before it looks whether the move can be made (a blow still waiting for its charge
  stays queued and still renews the wait). `StartFighting` 0x4844FD writes the 3000 of the start. With +0x579C = 2
  the stance chooses its own moves and the block ends at random (`GameRand(8) == 1`); neither reads the queue.
- **What the computer sees of its opponent** ✅ `DoFightMainAction` 0x489180 (v1.42): a jump table at 0x4894F4 on the
  opponent's state − 0x17. The stance (0x17) and the end of a cast (0x22) give the lot to attack (16, 8 or 1); an
  action (0x18), the start of a cast (0x20) and a cast (0x21) read the opponent's animation +0x49A0: 125..136 a blow
  (0x48934F), 137 a step in (0x4893E1), any other an attack at once (0x489488), which is what a cast's own animation
  gives; the block's start, the block and its end (0x1A, 0x1B, 0x1D) the block's choice (0x48942E); the reel in a
  block (0x1C) an attack at once; the others nothing. The lot to strike back, `GameRand(5)` (0x489377, 0x4893FE,
  0x48943F), is drawn only by the style 2 (`cmp ecx, 2` before each) and only against a blow, a step in or a block.

### Fights: the script natives

✅ Read in the v1.42 exe (bw1-decomp has only their declarations, `src/Black/Script.h`). Each pops as listed, looks the
thing up with `GetScriptGameThing` 0x70D220 and tests it with vt +0x34 (is a creature): a thing that is not one prints
"Thing not creature!" ([0xC0CF98]) and the native does nothing (pushing 0). Only IS_FIGHTING prints "Thing not
found!" ([0xC0CFAC]) for no thing; the others say nothing of it. The per-creature fields are `LH3DCreature`'s
(`Creature::GetCreature3D` 0x477850).

- **IS_FIGHTING 229** `GScript::IsFighting` 0x6F4D40: pops the thing, pushes `Creature::IsFighting` 0x47B1C0 as a
  bool: the fight opponent +0x528C (bw1-decomp `FightCreature`) is set. `StartFighting` 0x484410 sets it as the duel
  begins (state 0x16, the start, after the walk to the places); the fight's end fn_004845F0 clears it. So the
  scripts' "fighting" is the duel only.
- **SET_CREATURE_AUTO_FIGHTING 390** 0x6F5CF0: pops the creature, then the bool; `SetAutoFightStatus` 0x47D2F0 writes
  +0x579C = 2 for on, 0 for off. +0x579C is who fights for the creature (bw1-decomp's `FIGHT_CONTROL_AI` names): 0
  nobody, 1 the player with the computer waiting (+0x4AB8 counts down in the stance, the block, its reel and the
  actions, by bw1-decomp; below 0 it becomes 2), 2 the computer. The creature fight action sets 1 for a human player's creature and 2 for any other as a
  fight begins (bw1-decomp `CreatureAction.cpp`), whatever a script set before. `AddToFightQueue` 0x489A20 turns 2
  into 1 and leaves 0 alone; with 2, `ShuntFightQueue` takes nothing off the queue and the stance does not read it.
- **IS_AUTO_FIGHTING 391** 0x6F5D70: pushes +0x579C ≠ 0 (fn_0047D310) as a bool. It reads the field the last fight
  left, in or out of a fight.
- **SET_CREATURE_QUEUE_FIGHT_MOVE 392** 0x6F5DF0, **_SPELL 393** 0x6F5E60, **_STEP 394** 0x6F5ED0: pop the number,
  then the creature; fn_0047D320 / fn_0047D340 / fn_0047D360 call `AddToFightQueue(number, false)`, the spell with
  | 0x4000 and the step with | 0x8000, at the back of the queue. The queue's numbers (`CheckFightQueue` 0x489540,
  `AttemptAttack` 0x489CE0): 0, 1, 2 a blow high, in the middle, low; 3 a block; 4 the special move; 0x8000 | an
  animation played by `StartFightAction(value & 0x3FFF)`; 0x4000 | a miracle. A blow is queued waiting for its charge
  (+0x4AF4 = −1, +0x4B24 set), which only `ValidateFightEvent` (the hand let go) gives; a hit takes it away
  (`ReceivedFightImpact3d`). So a blow a script queues for another god's creature is never made, and blocks the
  moves behind it until the creature is hit.
- **GET_CREATURE_FIGHT_ACTION 395** 0x6F5F40: pushes as an integer `Creature::GetCurrentFightAction` 0x47D380 of the
  animation +0x49A0 (bw1-decomp `CurrentAnim`), by the byte table 0x47D410 for 106..193 and else 10 (bw1-decomp
  `FIGHT_ACTION` names): 106 → 9 fainting; 110..112 → 4 casting; 122..124 → 0 nothing; 125..129 and 131..135 → 3
  hitting; 130, 136 → 5 the special move; 137..140 → 2 stepping; 142..144 → 1 blocking; 179..183, 184..188, 189..193
  → 6, 7, 8 reeling high, in the middle, low; every other (0, the power-up 108, the reel in a block 198) → 10.
  +0x49A0 is not the animation playing: `StateSet` 0x484EC0 writes 0 entering the stance (state 0x17, 0x4850B3) and
  standing (state 0, 0x484F20); `StartFightAction` 0x4846E0 and the reel `StartFightRecoilAction` 0x484790 write the
  action's animation (the reel in a block 198); `AttemptAttack(3)` writes 143 as the block starts (the end of a block
  keeps it); `CheckFightQueue` writes 111 as a miracle's cast starts; the faint writes 106, getting up 107; the start
  and the finish keep what was there. So in its stance a fighter answers 10, not 0: the guide's bout test
  `GET_CREATURE_FIGHT_ACTION(MyCreature) == 0` ("Go on. Hit him!", CreatureGuide.txt 26681) never holds.
- **CREATURE_FIGHT_QUEUE_HITS 396** 0x6F5FC0: pushes as a float fn_0047D470, the count of queue entries (+0x4AC0
  of them at +0x4AC4) that are 0, 1 or 2.
- **SET_FIGHT_EXIT 420** `GScript::SetFightExit` 0x6F6060 (no shipped script calls it): pops one value and writes it
  as it is to `CameraModeNew3::AutoEndFight` [0x9CE6BC] (1 from the start; its only writer). `EndFightNow` 0x45A830 and
  `EndFightSoon` 0x45A800 act only with it or their argument set, and every call passes 1 but two `EndFightNow(0)`: the
  drag away (0x45BB4C, [Dragging away](#fights-the-camera)) and the zoom out past 40 (0x45E538). So with it 0 the
  player cannot leave a watched fight by either; the zoom still stops at 40, the drag still re-grips the camera
  (`UpdateClickParams`), and the fight's end, its arena gone or a fighter gone still end the watch. Not saved, and kept
  across lands.

## openblack

The fight camera, the panel, blocking, the reel, fainting and the fight's miracle side (`ApplyEffect`, inventory R22)
are done. Porting decisions: [Port decisions: the creature](../project/port-decisions-creature.md).

### The fight start and the camera

The camera's side of the watch (`camera::FightWatch` in `src/Camera/FightWatch.h`, the orbit in `src/Camera/FightOrbit`) is on
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
  order comes from `creature_fight::Arena::serial`, counted by the fight system as each arena is made. `GetArenas` is
  read by the fight music's arena branch (`CreatureFightMusic` in `src/ECS/AudioQueries.cpp`), and `ArenaOf` by the
  panel and by the villagers' reactions (a creature in an arena stops a villager's reaction to it,
  `VillagerCreature.cpp`); the count is only written where an arena is made, and no part of the turn state hash
  reads `CreatureFighting`'s contents, so the verification runs are identical. openblack makes an arena per fight at
  the float middle, with no reuse, no shrink and no sound.
- **The radius and the range:** fn_00424FB0's constants are openblack's, on the drawn size where openblack passes
  `Creature::size`. `CheckFightQueue`'s 0.8 R is openblack's `TakeOrder`; openblack starts every step and clamps the
  movement to the range (`ClampToRange`).
- **The end:** fn_004845F0's life loss is openblack's `EndFightFor` (`creature_fight::LifeAfterFight`), so
  `EndFightFor` tells the camera (`FightWatch::EndFight`) when `creature_fight::CameraLingersOnEnd` holds for the
  fighter's state. Of the running sub-state's three ends only (c) is matched: a duel whose opponent has gone or left
  the duel (`ProcessDuels`) calls `EndFightFor` and then, for the local player's creature, `EndFight`.
- **The fight music:** `GameQueries::creatureFightMusic` is assigned (`CreatureFightMusic`: the interface's creature
  fight state 0x10 as the predicate `ecs::audio_queries::LocalPlayerDirectsFight`, then the arena branch;
  [Fight music](../audio/music.md#fight-music)). The gesture pass reads the same state (`gestures::CurrentFight`,
  `FightGestures::controlsUp`): while the local player's creature duels with the fight controls up, the gesture stage
  is the fight's, and holding the move button wipes the path. In that state the fight's miracles are looked for: one
  drawn whose line from the creature to its opponent meets the opponent's skin sends packet 0x5A, which queues the
  miracle in the creature's fight at the next turn
  ([gestures](../magic/gestures.md#what-is-looked-for-and-when)); before them the special move's gesture (the star),
  which sends packet 0x22 with move 4 when the same line meets the opponent. Its presses are ([The player's presses](#the-players-presses)).
- **Fights on Land 1:** only from the Guide's fight lesson on, which no verification run reaches, so the runs are
  identical.

### The player's presses

The interface's fight state is the hand's (`HandSystem::InFightState`, `HandFightPress.cpp`, the rules in
`ecs::hand_fight`, `src/ECS/HandFight.h`): its action state number is 0x10 while the player's camera watches the local
creature's duel and the hand holds nothing (`ecs::audio_queries::LocalPlayerDirectsFight`) and nothing else holds the
hand, and 0x17 / 0x16 while a press on a body is held. In it the grab button's press (the left or the middle button)
and else the action button's go to the fight, paused or not, and nothing else of the hand takes them, the double click
and the land grip among them; each wipes the gesture's path.
- With an object kept by the interface the press is a body's: only the player's creature or its opponent, where the line
  through the cursor meets its skin as drawn (`CreatureFightSystemInterface::BodyHit`: every triangle of the creature's
  own mesh the game's mesh test looks at, posed by the drawn bones and shaped as the body is, from either side, the
  nearest ahead, `creature_skin_ray::Nearest` after `MeshIntersect::GetNearestIntersection`, a triangle whose spot
  can't be worked out through its edges' x left out as there; the band by the hit's height over the body's foot,
  shared by its head bone's height above the land, `GetHeadHeight`, the species' head action point in the turn's pose),
  while fewer than twelve moves wait
  (`QueuedMoves`) and no apply waits (the hand's apply gate): `game_packets::Type::FightMoveReplace` / `FightMoveAdd`
  with the move as the scripts number it (`hand_fight::BodyMove`), held until its own button is let go (or the
  interface stops acting, or the map's clear), when `FightCharge` carries the held time (`hand_fight::HeldMs`, on the
  visual clock).
- With none it is the ground's, at the hand's action point: `FailApply` off the map or with the creature not duelling,
  nothing outside the arena (`hand_fight::InsideArena`, strict, in the FPU's float steps) or with twelve moves waiting,
  else `FightSelectPoint` / `FightApplyPoint` with the point's map position, which shuts the apply gate until its turn.
- Each press plays sample 1 of the game's sounds where it lands.
- Each press leaves its mark (`creature_fight_marks`, on the fighter's `CreatureFighting::marks`; the hand calls
  `MarkOnSkin` / `MarkOnGround` as it presses and `LetGoOfMark` as it lets go), at most twelve, which a replace, each
  move taken (`CheckQueue`) and a hit on the waiting blow take away as the original's; each turn the held mark counts
  the turn and the ground marks follow the creature (`TurnMarks`); each frame the fight system works out their
  sparkles (`UpdateSparkles`, `Sparkles`), which the renderer draws as one-frame puffs of the blobs sheet, added
  (`DynamicsSystemInterface::AddedSprite`). The colours are pinned by `FightMarks.EachSparkleIsColouredByItsPlace`.
- At the turn (`ecs::creature_fight_packets`, registered with the hand's packets): a ground press steps the sender's
  creature (`CreatureFightSystem::GroundPress`, the step by `creature_fight::GroundStep`, bit for bit with
  `FightClickGround`) or, with the creature gone or not duelling, shows the failed command's spot visual; a body press
  queues its move while the body is there (`QueueMove`); the charge goes to the blow (`ReleaseCharge`).
- Refitted from raffclar's 83e1943cb, 42e099769 and 98b0b5ea9 (his Move and Action buttons, the presses only while
  the fight is shown, every press of the empty hand to the fight), wired at the original's interface state instead of
  his `Game.cpp`. The dormant `CreatureFightSystem::Press` and its `CreatureFightPress` component, which nothing in the
  game called, are gone, and with them the old ground rule (the longer axis, within 1.5 radii), which was not the
  original's.
- Tests: `HandFight.*`, `HandFightPacketsTest.*` (`test/creature/test_hand_fight.cpp`),
  `CreatureFight.AGroundPressStepsAsTheOriginal`, `CreatureFightTurnsTest.AGroundPressQueuesTheStepTowardsThePoint`,
  `CreatureFightTurnsTest.ALineOfSightMeetsTheBodyWhereItsBonesStand`. The Land Island window's "Hand's action state"
  shows 16, 22 and 23 in a fight. Testbed `combat.player_clicks`: neither tiger fights by itself, as in Land 1's
  lesson, and the scenario's mouse (the pointer put on a creature, `PointerTo` with `atCreature`, and its buttons through
  the hand's own input) clicks the opponent's middle with the action button, the player's tiger, then the opponent's
  head with the grab button. Testbed `combat.fight_miracle_gesture`: the same fight, the player's tiger knowing the
  lightning, whose gesture (the fork up) the scenario's mouse draws with no button held, through the gesture pass's fight
  stage ([Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting) for what the lightning
  then does to the opponent); six seconds later it draws the special move's star.

### The arena's ring

`CreatureFightSystem::UpdateRings`, from the fight system's frame update: each arena now in use
(`GetArenas`, keyed by its number `ArenaView::serial`) is given a ring the frame it is first seen, a
`particles::LightSheet` started on `creature_arena_ring::Points` (bit for bit with the original's, each point on the
land at its map position), white, `creature_arena_ring::Height` high, its strength moving one point every 0.03 s, fed
in at 1.0, spread 1.01, its middle at the origin until first built; the particles move it on and draw it each frame
(`ParticleSystem::UpdateFrame`, `AddLightSheets`) while the fight system keeps it, and an arena no longer in use lets it
go. Refitted from raffclar's 885337343 and fa3fef479 (his `creature_arena` ring), with the original's white where his
was grey. The script's arenas (`CREATE_ARENA`) have no fights yet, so they draw none. Tests:
`CreatureArenaRing.ThePointsAreTheOriginals` (36 oracle points), `CreatureArenaRing.ItStandsASixthOfTheRadiusHighInWhiteLight`,
`CreatureFightTurnsTest.AnArenaStandsInARingOfLightWhileItsFightIsOn`. Testbed: every `combat.*` scenario shows it.

### A fight a script forces

- `Fight` is an executor only ever forced (`creature_plan_actions::ForcedFor`, `Build::Fight`, target a creature), so
  the planner never weighs it. Its agenda (`creature_mind::FightCreature`) follows the builder's shape:
  - the request to the other to fight (`RequestPartner`, `RequestKind::Fight`, with an angry face): the other agrees
    only when it is free for it, under a script's control as the asker is, and of a height from half to twice its own
    (`creature_friendship::AnswerOf`);
  - the other stood still in its mind (`Demobilise`);
  - the main step `Step::Kind::Fight`: as it starts, the mind asks `CreatureFightSystem::StartFight(creature, other)`.
    One that does not start gives up the rest of the agenda. A fight that starts takes both creatures to their
    places, they taunt each other, duel and respond (the `CreatureFighting` stages stand in for the sub-actions
    from TURN_TO_FACE to RESPOND_AFTER_FIGHT). The mind leaves the body alone while it is fighting or knocked out, and
    the step is done on the first turn it thinks again;
  - the other let go (`Remobilise`).
- The builder's settings are made by `StartFight` for both creatures: the fight health, the stamina, who fights for
  it (`fight::Control::Player` for the local player's creature, `Computer` for any other) and the lean
  (`fight::FirstTendency` of the alignment for a creature that has not fought before, else what its record keeps).
- The guide's lesson (CreatureGuide.txt, CreatureDevGuideTeachesFight): `CREATURE_DO_ACTION(Guide, 21, MyCreature)`
  puts the guide under the script's control, and MyCreature is under it already (its own forced actions just before).
  So the request finds both alike, and the two must be of a height within half to twice each other.
- Tests: `CreatureFriendship.AFightIsAskedForThenFought`,
  `CreatureFightStep.TheFightStartsAsTheStepStartsAndIsDoneWhenTheMindThinksAgain`,
  `CreatureFightStep.AFightWithNobodyGivesTheAgendaUp`, `CreatureKindActions.AFightIsOnlyForcedAndOfAnotherCreature`,
  `CreatureMindSystemTest.AFightAScriptForcesAsksTheOtherFirstThenStartsTheDuel`,
  `CreatureMindSystemTest.AForcedFightThatCannotStartGivesTheRestUp`.
- A forced fight nobody directs ends as the original's, with a knock-out: Land 4's quest run (run ev292_qrp3_q7,
  2026-10-29, `refs/story/evidence/qrp3_q7.log` line 2249199) has Sleg's fight, started at turn 6092, end at turn
  9132 with the player's creature at fight health 0 and Sleg at 0.934, and the script go on to its losing branch.
- The verification runs stay identical: none reaches a forced fight (Land 1's Guide is made and fought only in its
  creature lessons, far past the runs' end), and the planner never picks it.

### The panel

`CreatureFightSystem::GetPanel` gives the panel of the fight the player's camera watches
(`camera::FightWatch::IsWatchingFight`, lingering too, and `GetWatchedFighter`), while the fight's arena is there
(`ArenaOf`): its maker first, then its opponent. `Game::UpdateHandInterface` hands it to the interface each frame
(none in the temple), which draws it with `creature_fight_hud`'s layout. `creature_fight_hud::Compute` uses screen
shares and a red-yellow-green blend, not the original's whole pixels.

### Asking first and calling a fight off

- **Asking first** (his #396). `CreatureFightSystem::StartFight`, after the busy and too-weak answers, asks the other
  creature with `creature_fight::AgreesToFight`: from half to twice the asker's height (`DrawnHeightOf`, the drawn
  size × the height at size one), not while it lies knocked out, and only when both or neither are under a script's
  control (`script_held::IsControlledByScript`). Otherwise it answers `StartResult::Refused` and nothing starts. Every
  starter goes through it: the leash tie to a creature, an angry creature, a script, the debug tools. The player is not
  told why, as in the original (vanilla bug). `creature_friendship::AnswerOf` (the port of his #616, the same request rule for the
  mind's joint actions) covers the same fight answer; whichever lands second keeps one copy.
- **Calling a fight off** (his #396). `CreatureFightSystem::CallOff`: duelling, both sides end their fight with no
  winner (`EndFightFor(self, std::nullopt)`: the life loss and the camera's linger as any end, no `FoughtFight`), go to
  `Respond` with nothing to play, clear their queues and wind down (`Fighter::stopping`,
  `creature_fight::StopTransition`): the stance finishes, a block or the cast pose ends at once, a start, a blow, a
  step, a block's start or reel and a cast's start play out first, and a cast started is never cast. No blow lands
  once the duel is over. Still setting up, each side simply leaves. The trigger is `LeashSystem::PullAwayFromAction`
  (a leash tie that takes the creature elsewhere), the game's unsuccessful end of the action.
- The creature window's fight panel has a "Call off" button beside "Abort fight"; the testbed has `combat.big_vs_small`
  and `combat.too_small`.

### Miracles, blocking and fainting

- `IsBlocking`, `Recoil` and `ForceFaint` are on the fight interface. Blocking is `creature_fight::IsBlocking`, asked
  through `CreatureFightSystem::IsBlocking`; the reel is `CreatureFightSystem::Recoil` (animation 184, `k_RecoilMid`,
  as `ApplyEffect` passes).
- `ForceFaint` is `Creature::Faint` from outside a fight's loss: fainted already, nothing; a fight it is in is left
  with no winner (`Leave`: no life loss, no linger; the opponent ends only at its next `ProcessDuels`), then it faints
  as the knocked-out do. Every faint now brings the spells on it to their end (`creature_spells::TryFinishAll`,
  fn_004F4B60). `ForceFaint` is called by an effect that destroys a creature that can die (`src/ECS/Effects/EffectValues.cpp`,
  `DestroyedByEffect` 0x476F7A) and by the tornado's pick-up for a creature it reaches
  (`TornadoSystem::CatchCreature`, fn_00477060). On Land 1 and Land 2 no creature has a spell on it, so a faint there
  is as before.
- **A miracle's effect on a fighter** (`CreatureApplyEffect` in `src/ECS/Effects/EffectValues.cpp`): with an opponent
  (`HasOpponent`: the duel, its end not paid) and unless a thing struck it in the physics (`EffectValues::blow`, set
  from the source's `blow` by the miracles' `ApplyEffect`, as [0xC6414D]; the fight's blows take no effect in
  openblack, so [0xC6414C] has nothing to guard): blocking (`IsBlocking`), the numbers × 0.1
  (`creature_fight::k_BlockedMiracleShare`), the harm kept as read before; `Recoil`; the heal mends the marks
  (`creature_marks::k_HealEffectCounts`); `TakeMiracle(heal, harm)` (`creature_fight::TakeMiracle`: the heal on, kept
  to 0..1, the harm off, at none or less `KnockOut`, which gives its opponent the win as a blow's knock-out does: the
  winner shows off or, evil below −0.5, poos, counts a win, both fights end, the loser faints). Then the heal mends the
  marks again, and the fight is asked again: knocked out, the object's effect follows (its life), as the original's
  second `IsFighting`. Tests `CreatureSpellHooksTest.InAFightAMiracleReelsTheFighterAndGoesToItsFightHealth`,
  `CreatureFightTurnsTest.AMiracleThatTakesTheLastFightHealthLosesTheFight`,
  `CreatureFight.AMiraclesHealThenItsHarmGoToTheFightHealth`. The fight's spells (`CastFightSpell`, from the player's
  gesture's packet 0x5A or the scripts' SET_CREATURE_QUEUE_FIGHT_SPELL) reach it through the miracle's own effect. The
  computer never casts in a fight by itself: the fight's spell (`LH3DCreature` +0x4B28) is written only by
  `CheckFightQueue` 0x4896EC from a queue entry with 0x4000, and `AddToFightQueue` 0x489A20 is called only by the
  scripts' natives (fn_0047D320 / 340 / 360), the ground press (`FightClickGround`) and packets 0x22, 0x23 and 0x5A;
  `DoFightMainAction` 0x489180 chooses blows, blocks and steps only. Not ported: the
  600-turn opinion of a creature applier.
  `IsCameraOnFight` answers from the player's camera (`FightWatch::IsWatchingFight`, the original's `HasFight`); its
  reader is the camera's port.
- A faint is got up only once the creature lies, as the original's creature update resurrects only a creature
  lying (`IsActuallyDead` 0x472F0C in `Creature::ProcessState`, and the faint sub-action's next stage waits for it at
  0x502320): the fight's get-up (`ProcessKnockedOut`) waits for the fall to end, and so does the mind's faint step
  (`creature_mind::Think`). A script taking the creature (`PrepareForScript`, the original's `StopWhatIAmDoing` →
  `Resurrect` 0x475966) still cuts a fall to the get-up. The fall can outlast the waits: the faint clip (106) lasts
  1266 to 2364 ms for the creatures (3366 ms for `Bgreek`), and its step is the turn's ms × (1.6 − size × 0.425), so
  at 100 ms turns it outlasts the mind's 10 s from size 3.21 (2364 ms; 3.42 for 1476 ms, 3.46 for 1266 ms) and the
  fight's shortest wait, 8 + 4 × size + 3 s for a neutral creature, from size 3.53; above size 3.74 the step is 0 or
  less and the fall never ends (`k_MaxSize` is 4).
- `CreatureKnockedOut::Stage::WaitingForSpells` and `CreatureKnockedOut::rest`, two additions of the imported code,
  are not taken.
- The defence divisor takes `creature_pose::DrawnSize`, the size the creature is drawn at.

### The move queue and the lean

- `creature_fight::LearnTendency` nudges the lean as [the original](#fights-the-move-queue-and-the-lean) does by the
  move's kind: a block or a step (`Move::Kind::Animation`) towards defence, a blow or the special move towards attack,
  and a miracle not at all (test `CreatureFight.AStepTeachesDefenceAndAMiracleTeachesNothing`). openblack still
  nudges as the move is queued (`PlayerMove`), where the original nudges as it takes the move from the queue, so a move
  that never leaves the queue still teaches.
- The help record counts the local player's creature's moves (2026-10-19, raffclar PR #525, unverified against
  v1.42): `creature_fight::HelpEventOf` gives a block, an attack (a blow, also while it waits to be let go, or the
  special move), a miracle or a step, and `CountsHelpEvent` counts it each time the creature looks at the front of its
  queue from its stance with its opponent in range; a move that ends a block counts nothing. The scripts read these
  counts; the record's file is read and written by `components/helpstats`, but no profile keeps it yet
  ([Advisors and help](../interface/advisors-and-help.md)).
- A fighter faces its opponent outright every turn of its fight action, as the original's `DoFightMainAction`
  0x489180 (bw1-decomp `Creature3D.cpp`, `DoFightMainAction`: `Heading = angle_correct(heading towards FightCreature)`). openblack
  had turned it at an invented 3 radians a second (`k_FaceTurnRate`), behaviour the original does not have; it was
  removed (`CreatureFightSystem::StepFighters`, `FaceHeading`). The heading is `heading_from_direction_vector`
  0x7FAA50 of the way to the opponent (`atan360(−z, x)`, 0 when x² + z² ≤ 1e-6), kept as a float and wrapped once by
  `angle_correct` 0x7FAAF0; openblack has both bit for bit since 2026-10-28 (`creature_locomotion::HeadingFromDirection`,
  minus the yaw `affine::GetYAngle` gives, and `affine::WrapAngle`), so a fighter right on top of its opponent faces
  heading 0. Before, it took `std::atan2` and an fmod wrap that rounds an angle already in range, and kept its heading
  when the way had no length. Oracle 2026-10-28: (3, 4) gives 0x401FE0BC, (−7, −24) 0xBE914D76, (0.0005, 0.0005) 0.
  Tests `CreatureLocomotion.AHeadingFromADirectionIsTheOriginals`,
  `CreatureFightTurnsTest.AFighterInItsStanceFacesItsOpponentOutright`. The original faces first, from the
  opponent's position as it is then (`FightCreature->GetPos()`), after the stance's clock and before
  `DoFightPullApart` 0x48B4B0 and the queue's moves; openblack faces in the same place of the fighter's turn since
  2026-10-28 (before, after the action's carry and the blow, which never run in the states that face, so the turn is
  the same). The pull-apart itself is not ported.

### The blows' force and the computer

- `creature_fight::DamageFactor` is [the original's](#fights-the-blows-force-and-the-computer) (2026-10-28, raffclar's
  #994 refitted): the shown strengths, each plus 3 (`k_StrengthShift`), divided first, then times the drawn sizes'
  ratio, then times the speed, kept in 0.04..2 and doubled for the first special move. The shown strength is the
  body's weak-strong target (`creature_pose::TargetMorph(...).weakStrong`, `creature_morph::FromAttributes`), which
  the drawn shape also takes. `Fighter::special`, which doubles the force and the reach, holds only for animation 130.
  Tests: `CreatureFight.DamageFollowsTheFormula`, `CreatureFight.StatesFollowOneAnother`,
  `CreatureMorph.TheShownStrengthIsTheOriginalsToTheBit` (the oracle's numbers),
  `CreatureFightTurnsTest.InItsPenABlowLandsAndHurtsByTheSizeItIsDrawnAt`.
- The computer's wait is the original's (2026-10-28, raffclar's #994 refitted): `creature_fight::CountComputerWait`
  takes the turn's milliseconds off `Fighter::computerWaitMs` of a player's fighter in its stance, an action, its block
  and a reel in its block, and gives the fighter to the computer only in the stance or the block, below 0.
  `TakeOrder` makes the wait 15 s again whenever, in range, in the stance or the block, a move is queued, also a blow
  still waiting for its charge. The duel turn reads the queue only for a fighter the computer does not fight for.
  The turn's milliseconds are the game turn's (see Pending).
  Tests: `CreatureFight.TheComputerTakesOverOnlyInTheStanceOrTheBlock`,
  `CreatureFight.AQueuedMoveInRangeMakesTheComputerWaitAgain`.
- The computer's choices draw as the original's (2026-10-28, raffclar's #994 refitted): `creature_fight::OpponentOf`
  gives an opponent that starts a cast or casts as `Opponent::OtherAction`, attacked at once (it was a blow's), and
  `ChooseMove` draws the most aggressive tier's lot to strike back only against a blow, a step in or a block, where it
  drew it before every choice. Tests: `CreatureFight.ComputerChoosesByWhatTheOpponentDoes`,
  `CreatureFight.OpponentStatesAsTheComputerSeesThem`.

### The script natives

The natives of [Fights: the script natives](#fights-the-script-natives) are in `src/CHLApi.cpp`, through
`CreatureFightSystemInterface` (`Locator::creatureFightSystem`):

- IS_FIGHTING asks `IsDuelling`: the `CreatureFighting` stage `Duel`, from `BeginDuel` (the original's
  `StartFighting`) until `EndFightFor` moves it on. While the creature walks to its place, taunts or waits it is in a
  fight (`IsFighting`) but not fighting for the scripts.
- SET_CREATURE_AUTO_FIGHTING sets `Fighter::control`: on `Control::Computer`, off `Control::None` (the original's 0:
  the computer never takes over, and a move queued for it leaves it so). Only a creature in a fight is changed, as a
  fight's start sets the control anyway. A move queued for a creature the computer fought for gives it to the player
  with the 15 s wait (`PlayerMove`), as `AddToFightQueue` turns 2 into 1.
- SET_CREATURE_QUEUE_FIGHT_MOVE / _SPELL / _STEP decode the number with `creature_fight::ScriptMove` and queue it at
  the back through `QueueScriptMove` (`creature_fight::ScriptQueueMove`): only a step teaches another god's creature,
  as the original's lean rule. A number the original would try as a blow at no band (5 and up) is not queued.
- GET_CREATURE_FIGHT_ACTION gives `creature_fight::FightActionOf` (the table 0x47D410) of `Fighter::actionAnimation`,
  which `Enter` keeps by `ActionAnimationOn` as the original keeps +0x49A0: none in the stance or out of the fight; an
  action's, a reel in a block's, a faint's or getting up's own animation; the block's 143 as a block starts and the
  cast's 111 as a cast starts; else what it was. Out of a fight, a creature knocked out answers 9 until it gets up and
  any other 10. Tests: `CreatureFight.TheFightActionIsTheGamesNumberForItsAnimation`, `WhatAFighterCountsAsDoing`,
  `CreatureFightTurnsTest.WhatAScriptIsToldACreatureDoes`.
- CREATURE_FIGHT_QUEUE_HITS counts the blows at a band in the queue (`creature_fight::QueuedBlows`), as a float.
- IS_AUTO_FIGHTING stays a stub: the original answers from the field the last fight left, which openblack does not
  keep once a fight is over.
- SET_FIGHT_EXIT sets `player_camera::State::fightExit` (`src/Camera/PlayerCameraScript.h`, kept in
  `Locator::scriptState` for the game's life, as the original's static); the player camera's drag away and zoom out
  past the furthest end the watch only while it is on, the re-grip and the clamp at 40 either way. Tests
  `WorldCameraFight.ZoomingOutPast40KeepsTheWatchWhileTheScriptsKeepThePlayerInTheFight`,
  `DraggingAwayKeepsTheWatchWhileTheScriptsKeepThePlayerInTheFight`.
- A script's fight (CREATURE_DO_ACTION 21, as the guide's lesson starts its fight) is the forced `Fight` agenda of
  [A fight a script forces](#a-fight-a-script-forces), so the natives answer for it as for any fight once its duel
  begins.
- Unchanged by these natives, the verification runs stay identical: no verification run reaches a fight, and the
  only caller of `SetAutoFighting` with off is the debug window's switch.

### Test hooks

- The debug window's camera switch turns the watching of fights off.
- The debug spawner's fight panel shows who fights for each fighter (player, computer or nobody) and its script
  action, and its "Fights by itself" switch turned off leaves nobody fighting for it, as SET_CREATURE_AUTO_FIGHTING
  off does.
- The testbed's `miracles.hand_grips_casting_creature` and `hand.status_panel` show the held creature's status panel
  (the latter strokes, slaps and lets go through `CreatureHandSystemInterface::Stroke`, `Slap` and
  `IsHeldByCommand`).

## Pending

- **The arena's ring** ([Fights: the arena's ring](#fights-the-arenas-ring)): openblack makes the ring in the fight's
  frame update, before the particles move it on; the original makes it in the arena's draw, then moves it on at once,
  so on its first frame it has moved one frame further. Its arenas are the fight's own (their middle and radius as
  [the arenas' list](#fights-the-camera) notes).
- **The player's presses** ([The player's presses](#fights-the-players-presses)), each its own fix:
  - the skin a press meets is the body as drawn this frame (`BonesDrawn`), where the original's matrix buffer +0x47F0
    may be a turn's pose: which buffer +0x47F0 holds at the press is not read;
  - the marks: a hit takes the waiting blow's mark whenever openblack's queue loses a waiting blow, where the original
    needs its release flag (+0x4B24, a pending fix of the queue); the sparkle's half size is the creature's drawn size,
    as the puffs take an `LH3DSprite`'s size; the hand's move to the mark of a held press (the hand's fixed point
    [0xD13F70]) is not ported; the special move's and the fight miracles' marks wait for the fight's gestures;
  - the arena the ground press is tested against is the fight's (`ArenaOf`, the quantised middle), not the 3D body's
    own copy (+0x4A98, the arena's MapCoords / 6553.6);
  - the hand's tooltip over a fighter (Block, Attack, Manoeuvre) and the fight's own gestures are not ported.

- **The pull-apart** (`DoFightPullApart` 0x48B4B0, bw1-decomp `Creature3D.cpp`): each stance turn, after facing
  and unless the opponent is in a blow's animation, two fighters closer than the sum of their forward extents
  (`GetForwardExtent`: the furthest bone ahead along the heading) are each pushed apart by half the overlap along the
  fighter's heading, each only when it stays within the arena's edge fraction. Not ported.

- **The fights' stops of a walk**: the fight's start, the faint, the taunt and the duel each stand a walking creature
  at once in openblack. The original's forced fight brakes, then turns and plays an action before it walks to the
  arena; its faint resets the route as the faint starts; its taunt's turn is asked for again every turn; its duel waits
  for a standing body. None is a braking stop openblack can take on its own yet, so they stay instant after the
  creature walk series (the braking stop at the action end and its callers,
  [routes](../beings/routes.md#the-creatures-walk-on-the-follower)).
- **A fight a script forces** ([Fights: a fight a script forces](#fights-a-fight-a-script-forces)):
  - creature +0x3B8 and `FightStarted`, which the builder sets, have no reader read and nothing in openblack;
  - the lean is first −alignment when the creature has never learnt the fight (learning +0x15C40[21]); openblack's,
    when it has never fought (`CreatureFightRecord::foughtBefore`);
  - the original sets the fight health, the stamina and who fights for it as the agenda is built, before the walk;
    openblack as the fight starts, the same turn as the request in openblack's agenda;
  - the original's walk to the arena, taunts, waits and response are sub-actions of the agenda (each with its look and
    face); openblack's are the fight's own stages, for a fight however started;
  - the reaction the FIGHT sub-state's start makes is not ported.

- **Calling a fight off, the rest of the original** (read 2026-10-18):
  - the creature's stop (`Creature::StopWhatIAmDoing` 0x4758D0, from `FinishActionGeneric` 0x47588F): when its 3D body
    has an opponent (3D +0x528C), the opponent's creature takes `ModifyStrength` by a value of the creature action
    table (`[g_CreatureActionInfos + 0x1660]`, 0x475B37..0x475B44; which row and field is not read), loses its opponent
    (+0x528C = 0) and finishes its action unsuccessfully (0x475B5E); then the creature's own `EndAnyActions` 0x48F710
    ends its fight (`EndFighting` 0x4845F0). The strength change is not ported;
  - `EndFighting` 0x4845F0 returns at once, with no camera linger and no life loss, unless the fight state is one of
    0x16, 0x17, 0x18, 0x1A, 0x1B, 0x1D, 0x20, 0x21, 0x22 (start, stance, action, the block's start, block and end, the
    three cast states); ours takes the life loss whatever the state (`EndFightFor`), so a loser fainting (state 0x1E)
    may lose life where the original's path does not. Which path takes the loser's life in the original is not read:
    to read before changing it;
  - the original's other ways to stop what a creature does (a script's forced action, other leash pulls) also end a
    fight there; ours calls a fight off only from the leash's pull-away, and ends the creature's own side (with the
    state test above, `CameraLingersOnEnd`) when the hand takes hold of it (`CreatureFightSystem::EndOwnFight`, test
    `CreatureFightTurnsTest.EndOwnFightEndsOnlyItsSideAndOnlyInTheMidstOfTheFight`);
  - his "a creature on its way to a fight takes no leash orders" waits for leash orders, which ours does not have.
- **Asking first, the exact test**: the fight's request answer (0x4F62D0, from the request-partner sub-action 0x503A50,
  the fight action's first step at `ConstructSubActionsForFight`) refuses outside 0.5 ([0x8AA3B4]) to 2.0
  ([0x8AB478]) of the ratio of the two `GetHeight`s (vt+0x42C: asked / asker), then asks the availability (0x4F6270):
  refused when the asked creature's mind action (+0xF60) is 9 (dead) or 0x105 (dead for good), or when the script
  control bit (0x400 of +0x24) differs; already partners (+0x1BE8) answers "partner", a creature with another partner
  refuses, else it agrees. Ours tests "lies knocked out" for dead (already answered `Busy` before), and has no partners
  in the fight's request.
  - The heights are the drawn size: `GetHeight` 0x477F50 is the 3D body's +0x90 × 15, so a test that changes only
    `Creature::size` after making a creature leaves its drawn size as it was made (`CreatureDrawPose::size`), and the
    fixture gives both.
  - The script bit is `ControlledByScript` (bw1-decomp `GameThingWithPos` Flags 1 << 10, the word at +0x24, compared
    by `xor` at 0x4F6294..0x4F62A3), the same bit the fight camera's watch reads (0x45A4D0). A fight that starts through
    the request, and so the camera's "a script controls a fighter" case, has both fighters or neither under a script;
    the camera's test controls both.

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
    ([The leash's collar end](leash-rope.md#the-leashs-collar-end)), each queued with its own fix. The beliefs about
    another creature are now traced to the drawn size too (on its way: "harder than me" by the drawn height and the
    other creature's height in four bands of its drawn size,
    [beliefs about other creatures](mind.md#beliefs-about-other-creatures-and-buildings-in-openblack)). Still not traced to +0x6C or +0x90: the
    fight's places and faint time, the reactions, the throws, poo and puke, and the eyes.
- **The fight camera** ([Fights: the camera](#fights-the-camera)):
  - which moment of our fight 0x5036C0 is: it runs once the 3D start 0x484410 took the arena; whether the opponent
    also runs it (and so asks the camera twice) depends on mind +0x1BE8 (the partner pointer RequestPartner sets), not traced;
  - the end: which of our ends is (a) or (b) needs our fight states mapped to the 3D states 0x16..0x22; the remarks of
    (b) and (c), reaction 0x28 and fn_0048F710's call of fn_004845F0 are not ported;
  - the fight music: the 0x10 state is a predicate of its entry conditions, the same one the hand's fight state
    follows, so the two agree.
    Which interface state index the handler of 0x5D4B16 (at [0xD180A8]) belongs to is not computed;
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
    is the route follower's target speed;
  - the arena's middle: the original's sums read the quantised MapCoords (0x5074B1..0x50750C, `StartFight`
    0x45A646..0x45A68B); queued as a fix. How the camera turns `GetPos` back into metres is not read;
  - the range: openblack starts every step and clamps the movement (`ClampToRange`) where the original refuses the
    steps beyond 0.8 R; queued with the arena list.
- **Fainting and the fight's additions**
  ([Fights: miracles, a tornado and fainting](#fights-miracles-a-tornado-and-fainting)):
  - a faint by other causes ends both fighters' fights in the same turn with no winner (the forced plan's
    `FinishActionGeneric` → `StopWhatIAmDoing` → fn_004845F0, and the partner's stop); queued as a fix;
  - which activity runs the sub-state 0x508C10 (its table at [0xBE5A28]) is not decoded;
  - the winner of a miracle's knock-out: the original forces its plan (desire 0x18, action 0x27 or 0x13) and sets
    mind +0x1C14 and +0x10B0 in `ApplyEffect` itself (0x478E50..0x478EDB); openblack gives the win as a blow's
    (`KnockOut`, then `Win`), whose show-off and poo are its own stages; whether the two match turn for turn is not
    measured.
- **The queue, the blows and the stamina against v1.42** (the re-measure of 2026-10-21; the rows and their evidence are
  in [the fighting tracker](../project/progress/creature/fighting.md)). On its way, each its own measured commit:
  - a full queue also clears the release flag;
  - a hit takes away only the last queued blow, and only while the release flag holds;
  - the stamina's regain per creature turn is the exe's own float constant, not openblack's 1/600;
  - the fight health may go below 0 after the fight;
  - out cold uses the drawn size.
  Still to check before a fix: whether a landed blow costs real life force × 0.5 × 0.05.
- **The lean** ([Fights: the move queue and the lean](#fights-the-move-queue-and-the-lean)): openblack nudges it as a
  move is queued, the original as the move is taken from the queue; the original's computer also chooses by the lean
  the fight started with (LH3DCreature +0x4AB0), where openblack's reads the lean as it is nudged. Queued as a fix.
- **The script natives** ([Fights: the script natives](#fights-the-script-natives)): IS_AUTO_FIGHTING waits for who
  fights for a creature to be kept after its fight (+0x579C outlives the fight).
- **The computer's wait** ([Fights: the blows' force and the computer](#fights-the-blows-force-and-the-computer)): the
  original counts it by the creature's own turn milliseconds (+0x48B4, which `LH3DCreature::UpdateTime` 0x481DF0 is
  given by its caller through the vtable, not read; raffclar's tree takes them slower for a frozen creature, not
  checked here); openblack counts the game turn's.

### Watching a fight, the camera's side, not ported

- the arena's own altitude for the arenas a script makes (`GScript::GetArena` 0x6F4C36, not read): openblack's flight
  takes the land's height at the centre, which is right for the creature's arenas (altitude 0);
- the arena's centre is quantised for the flight only: the in-view test and the drag away read the centre the fight
  side gives (the fighters' float middle), where the original reads `GetPos` (the quantised one) everywhere;
- the zone test of `FlyToPosFoc` (`InsideInclusion`) on the fight's flight, as for every openblack flight;
- the two starts of the camera's own are ported (35171c8ab, 712707382): lingering over an arena for more than 1000 ms
  (`fight_orbit::ArenaUnderView`, `k_ArenaLingerMs`) and the double click onto one (`fight_orbit::ArenaAtPoint`),
  over `CreatureFightSystemInterface::GetArenas` (tests `FightOrbit.TheArenaLingeredOverTakesInWhereTheCameraIsAndWhereItLooks`,
  `FightOrbit.TheArenaDoubleClickedOntoIsTheFirstWhoseRadiusReachesThePoint`); the follow of the object double clicked
  is not;
- the middle's raise by a quarter of fn_004867B0 for each fighter. Read 2026-10-24: fn_004867B0 is
  `LH3DCreature::GetHeadHeight` (0x4867B0..0x4867E3), the y of `GetHeadPos` 0x4813B0 less what the creature's vt +0x28
  gives for that point (`fsubr`, a float); `GetHeadPos` is the translation (+0x24) of bone [+0x51B8] (48 bytes a bone)
  in `GetSafeBuffer` 0x4842B0's pose. Porting it needs the creature's posed head bone in the camera's orbit, and
  vt +0x28 is not read;
- the keyboard move's own needs: the original applies it only with feature 8 and in mouse mode 2
  (0x45C943..0x45CAA7), and reports Drag 0x308 for it with feature 8 (0x45C986); openblack's `TiltZoom` applies it
  with feature 8 and reports Drag for it (the orbit, when it runs, still replaces it);
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
- the creature's own radius (LH3DCreature +0x5228) and so the orbit's distance: `ecs::object::GetRadius`, whose
  creature branch gives the body's radius at the drawn size ([The creature's radius](body.md#the-creatures-radius));
  a creature made without its body's measures keeps the generic mesh-box radius;
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
  to 0 as there. Tests: `WorldCameraFight.ASecondFighterWithNoRadius…` (three),
  `FightOrbit.TheDistanceCountsTheSecondFightersRadii`.
