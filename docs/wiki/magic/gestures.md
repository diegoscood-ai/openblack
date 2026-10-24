# Gestures

## Summary

Miracles are chosen and powered up by drawing shapes with the hand. The original samples the mouse (with no button
held) every 28 ms onto the land under the cursor, keeps the last 80 samples, finds their corners on the fly and
compares the sequence of turns, the first direction and the aspect against 81 templates read from `Gestures.jty`.
A recognised gesture gives a packet with a point and a size (the circle of the storm and the shields). Once a pass,
`ProcessPowerUpSystem` decides which gestures are looked for: the power-ups of a charging seed, the selection
spirals, the scribble that cancels, the repeat, and the creature's leash picker.

openblack ports the sampler, the recogniser, the packet, the pass's order and the gesture icons bit for bit, with
their leash and bubble models and the seeds' graphics inside them (the freeze phial's frozen look, the mists and the
mesh of a holder effect and the glints on a phial are not drawn there yet); the immersion and the creature's fight
gestures are not ported. The gesture's particle rules are on [Gesture rules](../physics/particle-rules.md#gesture-rules).

## The original

### Gestures: the buffer and the recogniser

#### The samples

- The sample is given by the mouse **with no button**. It is CMouse message 0, every 28 ms of mouse events
  (`fn_005CEAD0`), with the terrain point under the cursor, or that of the last sample if it is outside. There are no
  samples while paused nor during the 0.4 s following a recognition. If the camera changed position in the frame
  (`GCamera::IsMoving`), **the buffer is cleared** on every `ProcessPowerUpSystem`, unless a camera shake is going on
  or the camera mode lets the player gesture while it moves ([step 1 of the pass](#what-is-looked-for-and-when)).
- The terrain point is the interface's land under the message's integer pixel: fn_005E5620 at 0x5CEB0F (depth 0, the
  sea always, no depth out: 0x5CEB00 `push 0`; then `KeptInReach`). It is dropped as off the land when all three of
  its fabs are not above (double)1e-4f (0x5CEB18..0x5CEB4F, `test ah, 0x41`: a NaN counts as not above); then
  fn_0057C8F0 (0x5CEB86) reuses the newest sample's point, only when there is one (0x5CEB81, 0x57C8F8). Callers: the
  message 0 handler 0x5D99E0 (after fn_005CECF0 stores the pixel at GInterface +0x428 and fn_005D5980 finds the
  interface's land at the same pixel) and the reseed fn_005D2770 at 0x5D27A6, with +0x428.
- Mouse sampling: `GGame::MouseHandler` 0x54FFE0 adds the events' ms into [0xD019CC] (0x55001B) and sets it to 0,
  does not subtract (0x55006C); then `CMouse::ProcessPosition` 0x61A110.
- **Where the events and their ms come from** (read in the frozen v1.42 exe). `FinishInitialisation` stores the
  handler at 0x550141 in `TheSystem` +0x1C8, which is the `LHMouse` at +0x1C4 (its +4). `LHMouse::Draw` 0x7E57C0
  calls it through that slot (`call [esi + 4]`, 0x7E57FD and 0x7E587C) with (+0x150, the event, +0x60, +0x164), so
  the handler's ms is `LHMouse` +0x164 = `TheSystem` +0x328. Its only writer is `MouseMoveHandler` 0x7DC920
  (0x7DC94C): the message time it is given minus the one before ([TheSystem +0x7080]; the first gives 0, 0x7DC92C);
  it then calls `Draw(1, 4)` (0x7DC997), event 4 being the handler's move. `GameWindowProc` 0x7DBF30 calls it with
  `GetMessageTime()` for two messages: WM_MOUSEMOVE (0x7DC5A6, which first `SetEvent`s [TheSystem +0x708C],
  0x7DC5B3) and 0x8009 (0x7DC575, unless [+0x70B8]); both skip while [+0x70BA] is set ("turn off mouse move", fn_0054A710 from
  `SetTurnOffMouseMove` 0x54A770: on while the camera's grab or turn holds the mouse, `CameraModeNew3::Update`
  0x45AE96, and every interface pass back to whether a playback runs, `GInterface::Process` 0x5CEC3E). 0x8009 is posted by
  `MouseInputThreadProc` 0x7DBD10: it waits on that event for 40 ms (`WaitForSingleObject(…, 0x28)`, 0x7DBD29); a move
  just waits again, and a wait that runs out posts 0x8009 with the async buttons and the cursor's position (in the
  client area when [+0x74], 0x7DBE88), only while the window is active (`_gWindowActive`, WM_ACTIVATEAPP 0x7DC08C),
  outside an assert dialog and with [+0x70BA] and [+0x70B8] clear; else it sleeps 300 ms (0x7DBD73) and looks
  again, posting at once when it may. So a still mouse gives a message every 40 ms (more by the system's timer), each
  over 28 ms on its own, so each is a sample: 70 still samples wipe the path after about three seconds. The handler
  takes event 4 only with ms above 0 (0x55000D), and sends the position only with `GGame::IsAvailable` (vt +0x2C,
  0x550059) and no playback (`IsPlayBack`, 0x55003B); during a playback the sum grows and is not reset. The exact ms
  of each message are the system clock's (`GetMessageTime`) and its timers' rounding, which cannot be known. `GCamera::IsMoving` is +0x74, set every frame
  at 0x442660. Land under a pixel: fn_005E5620 (the sample and the packet); the trail's: fn_00800C30 with the shift
  off. Yaw: `GCamera::CalculateRotationAngleY` 0x441E60 (fn_00441E60), the heading between the camera's two points; the packet size
  rotates the offsets by (cos, −sin) of it.
- **The line under a pixel** that the gestures' land functions take: from `g_camera` [0xEA1DB8] to the pixel's
  `Get3DPointFromScreen` at depth 0, on the near plane [0xE839E0] that the most recent `GCamera::Update` wrote
  (0x4424B4, `GetNearClipping` over the camera drawn the frame before), 3.5 from `RenderInitialization` (0x818F99)
  before the first. So a turn's gestures see the frame before's near clip and the frame's gestures this frame's.
  fn_00800C30 with the shift off at depth 0 is the land, else the sea within 7500 of `g_camera`, with y =
  `GetAltitude` at ftol((x·65536)·0.1), else fn_0074CAF0 (the point that far along the line, eye + d·(length /
  sqrt((dz² + dy²) + dx²))).
- **Inside the temple** no gesture finds its land in a single player game: `GoInsideCitadel` pauses the game
  (`PauseGame(1)` at 0x553F83, unless it is a multiplayer game), so the sample skips at once (g_game +0x14 & 4,
  0x5CEADB), and `ProcessPowerUpSystem` empties the buffer and returns first in the temple (0x5CF318, 0x5CFAA6), with
  every packet (0x5CF559) and every `Success` (the trail) below it. So the temple's 0.2 near clip
  (`InnerCamera::PreDraw` 0x79693C, written after `GCamera::Update`) never reaches them there. In a multiplayer game
  (no pause) the original samples in the temple with the 0.2 near clip.

#### The buffer and the corners

- `GestureSystem::AddSample` 0x57BBC0: 80 samples in a ring.
  - A still sample is compared with the one from **two messages before** (0x57BC3A: head − 2, because the head has
    not advanced yet). 70 in a row like this clear the buffer (0x57BC90), and that sample starts it again (it enters
    the emptied buffer at 0x57BCBF): it is the 72nd still sample.
  - `ProcessNewSample` 0x57C3F0 finds the corners on the fly:
    - corner = turn ≥ π/8·¾ (`FindCorner` 0x57BFE0);
    - merge or rejection by length (`MergeOrReject` 0x57C200; `LongEnough` fn_0057C630: 12 px, or between 4 and 12 if
      the recent box measures less than 50 px);
    - heading and octant of the exit (`UpdateHeading` 0x57C710). The octant rounds an exact .5 down (fn_00578700).
- GestureSystem at g_game +0x25006C (0xC98 bytes); GestureSystemResult at +0x250070; the template list at +0x250064,
  read from ".\Data\Gestures.jty" (0xBEC8A4) in `GGame::LoadFiles`.
- T_corner [0xD064FC] = π/8·¾ (initialiser 0x57BB00); T1 [0xD064C0]; T2 [0xD064BC]; aspect classes [0x900110] /
  [0x900114].
- fn_00578890: wrap(b − a) (> π → −2π, <= −π → +2π). fn_0057A150 is another wrap (|x| > π → x ∓ 2π).
- fn_00578730: octant of the heading + π/2 (0 = up on screen).
- fn_0057BF60: turn = wrap(Atan2P(c − b) − Atan2P(b − a)) on screen.
- fn_0057C590 / fn_0057C5C0: "far" = 4 px or more on either axis.
- fn_0057C820: the corner's turn relative to the previous one (the start keeps its own).
- fn_0057C8F0: sample with the land point of the most recent one if the cursor is off the land.
- Boxes: fn_005789D0 (samples s..b−1; those at (0,0,0) are skipped after the first) and fn_00578D70 (key points s..e).
- Key point indices (flags & 0xB, or the last one): fn_005788D0.
- Backward search: fn_0057BE10 (with flags), fn_0057C1A0 (corner), fn_0057BE70 (with flags or a far position).
- Aspect fn_00578E40 = (W + 1) / max(1, (H + 1)·ratio), with ratio = screen width/height (0xE85058 / 0xE8505A).

#### The templates

- `Gestures.jty` (`GestureSystemDataList::Load` 0x579AF0): 81 templates of 0x65C bytes 📼 1.20 only. It reads the count and
  then that many records only, so bytes after the last record are ignored; each record (0x5790A0) reads 0x640 bytes
  and 7 u32 into overlapping byte fields, the keypoint count being the low byte of the u32 at +0x648, with no test
  against 80; any short read clears the whole list. The game's file has at most 26 keypoints a template.
- Template record (fn_005790A0, 0x65C bytes): 80 samples, then u32 count, gesture, positionMode, checkDirection,
  allowReverse, checkAspect (4 bytes; only the low byte is kept) and f32 aspect.
- **A template of more than 80 keypoints** (read in the frozen v1.42 exe; only a changed file has one). The list is
  one `new` array (0x579B74, the count before it) of 0x65C-byte `GestureSystemData` objects: the table pointer
  0x008DF7E0 at +0 (0x579BB4; the exe has no relocations, so it never moves), 0 at +4 (0x579BA6), the samples from +8
  (each 0x14 bytes, read raw), then the fields as `Serialise` leaves them: it reads the count's u32 at +0x648, the
  gesture's at +0x649, the position mode's at +0x64A, check direction at +0x650, allow reverse at +0x654, check
  aspect at +0x658 and the aspect last at +0x64C, so +0x648 is the count's low byte, +0x649 the gesture's,
  +0x64A..+0x64B the position mode's lower half and +0x64C..+0x64F the aspect. The matcher reads a template's turns
  (+0xC of a keypoint) for every keypoint from 1 to count − 2, and only keypoint 0's direction: keypoint 80's turn is
  the record's own allow reverse word (+0x654), and keypoint 81 + k's is the next object's sample k's y; a turn never
  falls on a table pointer for a count up to 255 (keypoint 162's is the zero word two objects on). Past the last
  object the turns are heap memory the game never wrote, which cannot be known.
- ComputeAspect 0x578EA0: a template stores maxX / max(maxZ, 0.1). The template tool multiplied z by width/height
  (0x578F30). BuildFromSystem 0x578C20 takes the key points and the newest sample, and then calls ComputeAspect.
- GestureSystemData (vtable 0x8DF7E0), SetToZero 0x578BE0, Append fn_00578D30 (no bounds check).
- Ideal shapes: fn_0068C650 / fn_0068C340 (per-gesture cache 0xD4E760); points (x + 100)/200, (100 − z)/200. GJPath
  0x687BE0..0x687E60: fn_00687CB0 (re-measures), fn_00687C80 (length), fn_00687E30 / fn_00687E60 (point at t).

#### The match

- `MatchGesture` 0x579F10 → `Match` 0x57A050: first `MatchForward` 0x57A1A0 and, if the template allows it,
  `MatchMirror` 0x57A3E0 (negated turns, error without wrapping). Only three things are compared:
  - the sequence of turns (a turn smaller than T1 = 21π/128 can be absorbed; maximum error T2 = 3π/16);
  - the first direction (fn_0057A0E0: the direction at idx against the template's first, mirrored 8 − d; 0 stays 0);
  - the aspect class (0.15 / 4, fn_00579FA0).
- fn_00579AB0: the gesture's first template gives the position mode.
- The player's miracle gestures (SPIRAL selection) are **14**, not 12: FORK_DOWN, CYRILLIC_L, VERTICAL_SCRIBBLE,
  S_SHAPE, FORK_RIGHT, FORK_LEFT, FORK_UP, HEART, THREE, W_SHAPE, SQUARE_SPIRAL, INVERSE_SQUARE_SPIRAL, HOUSE and
  STAR. Each one recognises its stroke and none of the other 13.

#### The packet

Packet (fn_0057A5E0), in mode 2 (the one of all the templates):

- the point is the terrain point under the centre of the box of the matched samples;
- the size is 1.05 × the half width of that box in the world, at that distance. It is the `size` of the circle of the
  storm and of the shields (the magnitude of the cast).
- The land is fn_005E5620 at 0x57A710 (depth 0, the sea always, no depth out: 0x57A6FC `push 0`; then
  `KeptInReach`) at the pixel (ftol(cx), ftol(cy)) of the box's centre (0x57A6DD..0x57A6F3). Its result is not
  checked (0x57A715): with no land the point is a local the function never set, so the original's value there is
  undefined. The distance is fn_00442D50 from `g_camera`, sqrt((dz² + dy²) + dx²); the edge point is fn_0074CAF0 at
  that distance (0x57A761), on the ray through (centre.x + width/2, centre.y). Centre: `LHRegionF::CentreCoord`
  0x7DEBC0. Called from `ProcessPowerUpSystem` at 0x5CF559.

### What is looked for and when

- `GInterface::ProcessPowerUpSystem` 0x5CF300 runs at the end of each `InterfaceActionProcess`: once per frame (`ProcessFrameInputs`) and again per turn
  (`GInterface::Process`, with the time of the last frame). Still not found: who sets bit 0x02 of m_Buttons and who
  reads the 40 s cap of the repeat.
- Its early outs come first (0x5CF310..0x5CF35A), each to the same exit (0x5CFAA6..0x5CFADC), which wipes the gesture
  buffer (g_game +0x25006C: its 0x320 dwords from +8, the bytes +0xC88 and +0xC90 and the dword +0xC8C) and returns:
  inside the citadel (g_game +0x205A28 == 1), the interface status's +0x24 bit 0x20, an inactive interface (vt +0x40C
  == 0), the status's +0x3C set, and a box holding the mouse ([0xC4CCEE]). The cameras that allow gesturing while
  moving are CameraModeNew3 in the arena, Dance and Follow. What the four other tests are (read in the frozen v1.42
  exe):
  - status +0x24 bit 0x20 is the creature locked for the hand: `Creature::NetworkFriendlyStartLockedSelect` sets it
    (0x476E79), `Creature::NetworkFriendlyEndLockedSelect` clears it (0x476EA0); the only other writer is a saved
    game's load (`GInterface::ResolveLoadForCreature` 0x5D057F), and `IsHandReadyForObject` 0x5DC89C is its other
    reader. A creature gone while locked never gets its end, so the bit stays set
    ([each turn](../hand/states.md#each-turn));
  - status +0x3C is the interface's locked object: fn_005DC110 (+0x3C = the object, +0x40 = 0) from the
    StartLockedSelect handler (0x1B, fn_005DA950 0x5DA997, once the object passed its checks: not null, interactable,
    not locked already; the object's own start's result is ignored) for any kind of object, cleared by the
    EndLockedSelect handler (0x1C, fn_005DAA10 0x5DAA55, unless the object is null), by `GInterfaceStatus::Process`
    0x5DC57F (with `EndAction` for the local interface) when the object is no longer available (vt +0x2C), when it is
    not a creature (vt +0x34) and the hand has no influence, or when its `ProcessInInteract` (vt +0x808: piles, fields
    and fish farms; 1 for everything else, 0x639B10) is not 1, and by `SetToZero` 0x5DBA7D;
  - vt +0x40C is `GInterface::IsActive` 0x5CE2E0 (!(+0x28 & 1), the script's wide screen);
  - [0xC4CCEE] is the frame's box flag, cleared and set by `InterfaceActionProcessFirstGUIFeatures` 0x5D2A70 in every
    frame's interface pass ([A box takes the mouse](../hand/states.md#a-box-takes-the-mouse-the-escape-menu-a-dialog));
    the turn's pass reads what the last frame left.
- Order of each call:
  1. the clearing by the camera, the 0.4 s wait and the expiry of the pending circle (5 s). The camera's test
     (0x5CF3AF..0x5CF40C, read in the frozen v1.42 exe): with the camera moving (`GCamera` vt+0x174) the buffer is
     wiped, unless a camera shake is in the list (`LH3DCameraChecker::g_first` [0xEB99A8] not null) with the shakes
     switched on ([0xC383B8], 1 and never written; the shakes' own update fn_008210C0 and fn_00821270 read it too),
     or the current camera mode lets the player gesture while it moves (fn_005CF2D0: the mode stack's current mode's
     `CanPlayerGestureWhenCameraMoving`, vt+4: 1 for `CameraModeDance` 0x44ABD0 and `CameraModeFollow` 0x44B9B0, the
     player's `CameraModeNew3` 0x45A860 only with its +0x44 and +0x48 both set, 0 for every other mode);
  2. **the circle**: with the action pressed (m_Buttons 0x200) and a seed with `sizingGesture` (CIRCLE: storm,
     shield and physical shield), the circle stores position and size;
  3. one else-if chain (0x5CF5A2..0x5CF697); a stage that returns true ends the pass:
     - the creature's fight: with the local creature fighting (`Creature::IsFighting` 0x47B1C0, its only call in the
       pass, 0x5CF5AA, kept in a local flag that steps 5, 6 and 7 read at 0x5CF7D0, 0x5CF8D1 and 0x5CF996) and the
       interface's action state +0x44 == 0x10: with the move button held (`ControlMap::IsActionPerformed(1)`,
       BINDABLE_ACTION_MOVE, 0x5CF5CB) the gesture buffer is wiped as by the early outs and the pass ends with no HUD
       icons (0x5CF62D → 0x5CFAC6); else both pickers close (+0x371 = +0x364 = 0) and its gestures run
       (fn_005CFDE0): true ends the pass, false goes on at step 4 (0x5CF69D). With the state not 0x10 the chain goes
       on below, still with the flag set;
       - **the fight's gestures** (fn_005CFDE0, read in the frozen v1.42 exe): nothing without the local player's
         creature (player +0xA64), with its 3D body's fight queue full (+0x4B30 ≥ 12) or with no opponent (mind
         +0x1BE8). The special moves: a count of 1 (fn_0047C610 returns 1) whose gesture is
         `creatureSpecialMoveGesture` (fn_0047C620 returns [0xDA0588], GSpellSystemInfo +0x28; 8 STAR in the frozen
         info.dat), for every creature, with no knowledge test; lf type 1; recognised: Success(1), help event 19
         (0x5CFF0B) and fn_005D3DD0, which casts a line from its own body's point (`LH3DCreature` 0x4813D0) to the
         opponent's and, when it meets the opponent's current mesh (`MeshIntersect::GetNearestIntersection`
         0x865020), adds that hit as the fight's intersection (`AddFightIntersection(0, hit)` 0x48A420,
         `_LastFightIntersectionPending` = 0) and sends packet 0x22 (the creature, move 4: the special); true either
         way. Then the miracles: for each spell seed whose +0x18C flag is set (our `unknown0x17C`, set for FIRE,
         LIGHTNING_BOLT, HEAL, WEAK and STRONG) and whose first magic (+0x124, `magicTypes[0]`) the creature knows
         (`CreatureActionsKnownAbout::KnowsAction(1, magic)` 0x4E2890), lf type 0xB with the seed and the seed's
         gesture (+0x104); recognised, the same line to the opponent: a hit gives Success(1), the fight's
         intersection and packet 0x5A (the creature, the magic) and true; a miss returns false, so the rest of the
         pass runs on the recognised path. No help event for the miracles;
     - with an **icon** seed that is charging (`HoldingChargingSeed` fn_005CEF50), **both pickers close** (+0x371 and
       +0x364 = 0, 0x5CF610) and the seed's power-up gestures run (fn_005D0000); if it already has a power-up,
       SCRIBBLE removes it (packet 0x6A);
     - else the stage of the open selection with the hand ready (fn_005CF1C0, `SelectionStage` 0x5CFAE0, cap
       `selectionSystemTimeOut` = 30 s), which first **closes the leash picker** (+0x371 = 0, 0x5CF668);
     - else, while the leash picker is active (`LeashSelectionActive` 0x5CEFB0), its stage
       (`DoLeashSelectionSystem` 0x5D01F0, see the leash picker below);
  4. **SCRIBBLE cancels**, skipped while a charging icon seed is in the hand (fn_005CEF50, 0x5CF69F). One else-if
     chain on the hand's state, not on the recognition: the first branch whose condition holds is the only one that
     looks for the scribble (lf type 0), and a scribble it does not recognise leaves the rest of the pass to run:
     - **an object in the hand** (interface status +0x90, 0x5CF6B2): `GetFirstObjectInCurrentHand`, in the influence
       (+0x48) and `ValidToShakeFromHand` (vt +0x710) == 1, else nothing waits for the scribble (not the leash nor
       the icon). Recognised: the hand effect's remove-from-hand visual (`+0x3A0` → +0x4948, vt +4), Success(0)
       (fn_005CE420), `ForceDropHeld` (fn_005D4350); a seed goes back to its worship site or is deleted;
     - **else the hand's leash on and not tied** (0x5CF725..0x5CF733: the interface status's `GLeashStatus` +0x12C,
       +0x14 != 0 and +0x24 == 0), with no influence test: recognised, the same remove-from-hand visual, Success(0)
       and **packet 0x5F** (`SetPacket`, 0x5CF769), the click on the creature's packet, so the leash comes off at the
       next game turn through the toggle (fn_005D06E0 → fn_005E7140 → `SetOn(creature, 0)`). A tied leash falls
       through to the icon;
     - **else** an icon charging for the hand (fn_0064BAB0, 0x5CF785): it cancels the charge that started last
       (`GPlayer::CancelSpellCharging` fn_0064BCC0: the charging icon with the largest start turn +0x138, a turn-0 start
       never; packet 0x1E, 0x5CF7BE);
  5. with the hand free, SPIRAL / INVERSE_SPIRAL open the selection if there is a requestable icon of that category
     (`OpenSelection` 0x5CF010);
  6. R_SHAPE repeats the last miracle (packet 0x26), if the player can;
  7. **the square spiral** (0x5CF98E..0x5CFA89): with the local player's creature, not fighting, the leash picker not
     active (0x5CF9A4) and the selection not open with the hand ready (fn_005CF1C0); and either the creature's leash
     on this interface status (`GetInterfaceStatusLeashOn` 0x4CF060 == status +0x39C) and more than one leash known,
     or no leash on at all and at least one known (fn_004CF040 counts the creature's known flags +0x110C, +0x1110,
     +0x1114: evil, rope, good); and `leashSelectionStart` ([0xDA0590]) != 0. Then lf type 2 with the leash of
     `GLeashStatus` +0x1C. Recognised: Success(1) and the gesture cleared; with no leash on, **packet 0x5F** (the
     leash goes on at the next turn) and, with more than one known, the pending flag +0x370 = 1 and the picker opens;
     with the leash on and more than one known, +0x370 = 0 and the picker opens. Opening (fn_005CEF80) sets the timer
     +0x374 = 0, **closes the miracle selection** (+0x364 = 0) and sets +0x371. No help event.

#### The leash picker

- `LeashSelectionActive` 0x5CEFB0 keeps +0x371 only while the local player has a creature (player +0xA64) and either
  a leash-on is pending (+0x370) or the creature's leash is on this interface status; otherwise it clears it. A tied
  leash is still on (+0x14 stays 1), so it keeps the picker.
- `DoLeashSelectionSystem` 0x5D01F0: with no creature, +0x371 = 0 and the pass ends. Unless the game is paused
  (g_game +0x59A4, the flag the cooldown and the circle's timer test), the timer adds the frame's game time;
  **strictly greater** than `leashSelectionSystemTimeOut` ([0xDA058C]) closes the picker, but this pass still looks
  for its gestures. With the creature's leash not on this status it returns false and the rest of the pass runs. Then
  lf[SCRIBBLE] type 0: a scribble gives the remove-from-hand visual, Success(0), +0x371 = 0 and the gesture (+0x1A4)
  cleared, and ends the pass, so step 4 does not shake the leash off.
- **The secret script.** Then (0x5D02C0) on land 1 (g_game +0x205A08 == 1), not multiplayer (`IsMultiplayerGame`
  0x552F80) and not a playground game (g_game +0x205A0C == 0, the flag `GGame::Init` sets after
  `ResetAndStartPlaygroundGame`), a SQUARE_WAVE (23, fn_005CF220 with no lf entry written for it) wipes the buffer
  (0x5D02F3: g_game +0x25006C's 0x320 dwords from +8, the bytes +0xC88 and +0xC90 and the dword +0xC8C, as the early
  outs) and starts the script "HiddenScript" ([0xBF1BF8], `GScript::StartScript` 0x6EB710: nothing without a script
  runtime, else the runtime's start by name with the script types 0x7F, 0x60 in a multiplayer game), with no
  `Success` and the gesture +0x1A4 left as it is, and the pass goes on: the leashes below are looked for in the wiped
  buffer. The script is `LandControlAll`'s `HiddenScript` in `Scripts\Quests\challenge.chl` (the phone box secret).
- Last, for leash 1..3 (evil, rope, good): not the current one (`GLeashStatus` +0x1C), known (creature +0x110C + 4
  (i - 1)) and `leashSelectionGestures[i]` ([0xDA0594] + 4 i) != 0: lf type i + 3 and lf leash i; recognised,
  Success(1), the gesture cleared, **packet 0x65** with the leash (`SetPacket(0x65, i, 0)`, 0x5D03B4), +0x371 = 0, and
  the pass ends.
- info.dat (`Scripts\info.dat`, `GSpellSystemInfo`, read from the file): `leashSelectionSystemTimeOut` = 25.0,
  `leashSelectionStart` = 15 (SQUARE_SPIRAL), `leashSelectionGestures` = {17 E, 7 VERTICAL_SCRIBBLE, 17 E, 13 HEART}:
  the evil leash is the vertical scribble, the rope the E and the good leash the heart (entry 0 unused).
- The packet 0x5F handler (`GPacket::ProcessPacket` 0x63D3B5) clears +0x370 before the leash toggle. The 0x65
  handler is the temple's leash pick (refused under the compassion and anger spells, else the creature's leash
  changes, and the temple's pick is set). Unless refused, it calls fn_005E6AE0 (0x63DD1F) on the interface status's
  `GLeashStatus` (+0x12C): +0x1C = the leash, then fn_005E6B00(leash, player): the player's +0xA48 → +0x30 pick
  (fn_004648E0), the creature's moods (`UpdateMoodsFromLeash`, with a creature) and, **for the local player**
  (`players[g_game +0x205A59]`, 0x5E6B37..0x5E6B57), the gesture buffer wiped (g_game +0x25006C is the
  `GestureSystem`; 0x5E6B59..0x5E6B81, read in the frozen v1.42 exe). fn_005E6AE0 is also what the compassion and
  anger creature spells' starts call on a leash that is on (0x4F5A04 with GOOD, 0x4F5B24 with EVIL, for the creature's
  player: [the leash](../creature/leash.md#the-timed-dominant-desire)). The same fn_005E6B00 runs when `SetOn` puts the
  leash on (0x5E7001, with the creature's player, vt +0x1C = Creature +0x1070, after the rope and before the lengths;
  a null player wipes nothing), and `SetOn` wipes it again when it takes the leash off (0x5E70D9..0x5E7129, last,
  after `StopImmersion(7)`, when the status's own player, `GLeashStatus::GetPlayer` 0x5E7630 = player +0x30, is the
  local one). The off path runs on any `SetOn(creature, 0)` and on `SetOn(creature, 1)` when the creature does not know
  the rope (+0x1110), with no test that a leash was on; most callers ask `GetInterfaceStatusLeashOn` first, but
  `Creature::ToBeDeleted` (0x47502F: `SetOn(creature, 0)` on each of the creature's player's interface statuses,
  `GetNextInterfaceStatus` 0x64AAC0, when it has a player) and `DetachObjectLeash` (0x6F46AE) do not, so a creature of
  the local player that is deleted, or detached by a script, wipes the path with no leash on. The two computer-player
  callers test the tie first: 0x653C81 is the start of the hand step `AttachLeashToObject` (sub_653BF0, row 12 of
  the step table, stage 0 at 0xBFF940), which runs the off path only when `GLeashStatus` +0x24 is set (0x653C45), and
  0x658A8C is fn_006589F0 (from `GComputerPlayer::NewProcess` 0x6574B1), which needs the tied object (fn_0065D2E0 =
  +0x24) and an EVIL leash (+0x1C = 1); a tied leash is on (the tie sets +0x24 and then `SetOn(1)`, whose off path
  clears it), and their status is a computer player's, never the local one, so neither wipes the path. The script's
  `AttachObjectLeashToHand` (fn_005E6EA0, 0x5E6EBA) and the tie (0x5E6D1A) call `SetOn(1)` already on or not. The
  toggle's untie (fn_005E7140, tied) does not call `SetOn`.
- Before any pick the hand's current leash is the rope (`GLeashStatus` +0x1C is 2 at init, 0x5E6950).

#### The gesture icons

Read in the frozen v1.42 exe.

- **The list.** At the end of the pass, only when the pass runs to its end (every stage that ends it returns before),
  fn_0068ABA0 (0x5CFA93) takes the `LookingFor` table (24 entries of type, seed, leash, power-up). A table equal to the
  copy it kept (0xD4E940, 0x180 bytes) changes nothing; any other clears the list (fn_0068ADD0), keeps the new copy and,
  for each gesture whose type is not 0, makes a `DisplayGesture` (0x5C bytes, ctor 0x68AF90) inserted before the first
  one whose display order (+0x34) is not above its type: the highest type first and, of equal types, the later
  gesture first. Its init fn_0068B380: gesture +0xC, seed +0x10, leash +0x14, power-up +0x18, order +0x34, the shape
  fn_0068C650 +0x1C, the symbol's sprite +0x24 (half size 0.2 [0xC02674], cell gesture % 16, four cells a row) in the
  material fn_0057D5D0 +0x28 (S_Gesture<gesture / 16>, mode 6, two-sided and tiling, made once a page, the same as the
  bubbles' $g symbols), the sparkles' period +0x38 = fn_0068C110 × 2.0 [0xC02660] (the shape's length over the
  circle's), the phase +0x3C and its seconds +0x40 at 0. The ctor: the sparkle's sprite +0x20 (cell 0x28 of eight a
  row), the material [0xD4EB6C] (S_SpriteSheet2, mode 13, made once), the symbol's colour +0x2C = the local player's
  `GetPlayerColour` with alpha [0xC02668] 0xFF, the sparkle's +0x30 = 0x96FFFFFF ([0xC02669]). Then fn_0068AB70 puts
  each one in the slot of its index: fn_0068B0A0 → 0x68AED0, for the screen's size then: x from
  W·(1 − ((0.005 + 0.035)·slot + 0.035 + 0.015)) to W·(1 − ((0.005 + 0.035)·slot + 0.015)), y from H − 0.035·W to H
  (every step a float). `PSysGlobal::OnClearMap` clears the list and the copy (fn_0068AA10), and with them the
  models' objects (0x68AA16..0x68AA58): the four leash objects of 0xD4E7C8 (each deleted through its vt +4 and its
  pointer set to 0, so the next draw of that leash makes it again with its matrix the identity) and the 42 seed
  graphics of 0xD4E898 (`ToBeDeleted(0)`, vt +0xC, and set to 0). The bubble (0xD4EB74) is not cleared.
- **The fade.** `PSysGlobal::DrawLoop` (0x68F61B, the particles' stage of the world's frame, so not in the temple)
  calls fn_0068AA60 once a frame. Its alpha [0xC02710] starts at 1.0 and nothing else writes it. With a help system
  (g_game +0x25005C): while the alpha is above 0 and fn_005C5760 says a help text is drawn, it goes down by
  `g_game_time_inc` × 2.0 [0xC02718] × 0.001, and the frame it reaches 0 draws nothing; else, while it is below 1 and
  none is drawn, it goes up by `g_game_time_inc` × 2.0 [0xC02714] × 0.001, to 1. Then, while the alpha is not 0, each
  icon is one Z object at key 0 (`NewZObject`, sub_68B480). fn_005C5760 reads the HelpText (HelpSystem +0x14) +0xB8:
  its draw (sub_5CCAB0, the FinishFrame callback sub_5CD020) clears it first (0x5CCABB) and sets it when its text loop
  draws the newest text (0x5CC974: the TEXT_DRAW gate fn_005C6E60, the display not hidden +0xAC, the newest entry used);
  with a film open (+0x250188) the draw stops right after clearing it. Its only other writer is HelpText's init. So the
  icons read the help text drawn the frame before.
- **One icon's draw**, sub_68B480: nothing for a shape with no points (fn_0068C330). First the 3D model inside the
  icon, sub_68B7E0 (seed, leash; below). Then with
  ZFUNC 8 and ZWRITEENABLE 0 the symbol at the middle of the box (sub_68B420: the pixel a + (b − a)·(u, v), each cut
  to an integer, `Get3DPointFromScreen` at depth 10.0), its colour each byte of +0x2C × ftol(alpha × 255) >> 8. Then
  in [0xD4EB6C]: seconds += dt and phase += dt / period (dt = `g_game_time_inc` × 0.001); a phase past 1 goes back by
  1 − 0.1, then fmod 1; a sparkle (sub_68BFC0) at the phase and, while it is past 0.9, another at phase − 0.9. A
  sparkle: the shape's point at t (sub_68C1D0: by the points' index, (count − 1)·t, straight between two; the first at
  or below 0, the last at or past 1) through sub_68B420 again, half size [0xD4EAFC] = 0.05 × 2, the RGB of +0x30 ×
  round((1 − (2t − 1)²) × alpha × 255) >> 8 with its alpha byte kept. ZFUNC back to 4.
- **The model inside the icon**, sub_68B7E0(seed, leash), read in the frozen v1.42 exe and run in the oracle (the
  numbers below are its bits):
  - Order 9 (the inverse spiral) takes seed 22 (0x68B7E9). The leash comes first: leash ≠ −1 is the leash's model,
    else seed ≠ −1 the seed's graphic, else for order 8 (the spiral) the bubble, else nothing.
  - **The depth reset** first, for every icon: [0xC02720] is 1 and never written, so sub_68A6C0 always runs. A
    rectangle of four pretransformed vertices (0xC02680, stride 0x20): (x0, y0), (x1, y0), (x1, 2y0 − y1),
    (x0, 2y0 − y1) from the box at +0x44..+0x54 (the box above the icon's, as big), z [0xC0270C] = 0.999, rhw
    0.001, colour [0xD4EB64] (never written: 0), in a material of mode 18 with no texture, two-sided (made once at
    0xD4EB68), ZFUNC ALWAYS around it (render state 0x17), `DrawAndClip2D` with the triangles (0, 1, 3), (1, 2, 3)
    (0xC02700). Mode 18 writes only the depth where its alpha test passes (ALPHAREF 0: always), so the box's depth
    is reset to nearly far and the model shows over the land there.
  - **The camera**: `SetSun` with the sun's own value (no change), then sub_68B740([0xC02724] = 10): it keeps
    `g_camera` (0xD4EAE0..0xD4EAF8) and `UpdateCamera`s to the position (0, 0.2 × 10, 0) = (0, 2, 0) ([0xC0271C] =
    0.2) and the focus (0, 0, 10); sub_68B7D0 puts the camera back after the model, after `SetSun` again. The lens
    is the frame's (the projection is not touched).
  - **The screen offset**: [0xFA9398] = 1 and the offset [0xFA939C] = (x1 + x0) × 0.5 − W / 2, [0xFA93A0] =
    (y0 − y1) × 0.5 + y0 − H / 2, every step a float, W / 2 and H / 2 whole (0x68B846..0x68B89B; e.g. the box
    (972.8, 732.16)..(1008.64, 768) of a 1024 × 768 screen gives 478.72, 330.24): the model drawn at the middle of
    the screen lands at the middle of the box above the icon. `LH3DRender::DrawTriangle` and `DrawTriangleMT`
    (0x82F811, 0x82F921) add it to every vertex while the flag is set, an x below 0 becoming 0 and one not below W
    becoming W − 1, y alike with H; it is cleared at 0x68BF1F.
  - **The leash**: one `LH3DObject` (static) per leash number, made at its first draw and kept until the map is
    cleared in the table at 0xD4E7C8 (0x34 bytes a leash: the object, then a matrix made the identity), its mesh
    sub_4645A0(leash),
    one of the three copies of `Data\MISC\leash.l3d` with `data\textures\leash.raw` that fn_004645D0 makes (the
    same as the temple posts' collars; a leash out of 1..3 takes the first), and `SetAnimatedUV(0, v)` with v = 0.375
    for leash 1, else (leash − 1) × 0.125 (the posts' bands). Each draw turns the kept matrix's rows, with l the leash,
    t = ms × 0.001 and k = (1 + 0.05 l) × 1.0 ([0xC0272C]): rows 1 and 2 by (1 − 0.05 l) × 1.0 × 0.65823 ([0x936ACC])
    × t, rows 2 and 0 by t × k, rows 0 and 1 by (1 − 0.06 l) × k × 0.79543 ([0x936AC8]) × t, each a' = c a − s b,
    b' = c b + s a, the cosine stored as a float, the sine kept wide (0x68BA02..0x68BBCE). Its matrix is then the
    identity times 0.15 ([0xC02728]) at (0, 0, 10) premultiplied by the kept one (`PreMultiply`: 0.15 × its rows, the
    place kept), its scale 0.15, its colour 0x00FFFFFF with the alpha byte ftol(alpha × 255) (0x68BC76), drawn with
    `Draw` (vt +0x108, no cut). E.g. leash 2 drawn four times at 33 ms: first row 0x3F7BFDF6 0xBDC08D90 0x3E18AA93.
  - **The bubble** (order 8, neither): one object made once at 0xD4EB74 with the shared mesh
    `.\data\spells\meshes\O_Bibble_up.l3d` and its material properties {1, 1, 0, 1, 1} (those of the one-shot
    orb), `SetNeedSorting(1)`, no dynamic shadow nor shadow on texture; its matrix the identity scaled 0.1
    ([0xC02734]), its rows 1 and 2 turned by the float quarter turn 0x3FC90FDB (sub_68BF40, sine and cosine kept
    wide), at (0, 0, 10), its scale 0.1, the same colour, drawn with `DrawWithClipping` (vt +0x104, 0x815980): kept
    on the screen (it is at the view's middle), then, its flag bit 3 being set as every new object's (Flags 0x10009,
    0x816537), left out when (half diagonal × 0.1 × 300)² < 104, its squared distance from the camera, compared as
    "not less" (a NaN is left out).
  - Neither object has dynamic lighting nor its own alpha set (bits 5 and 7 of the new object's flags clear), so its
    vertices take the colour as it is (`LH3DPrimitive::Draw`, [Model lighting](../rendering/model-lighting.md)) and
    its primitives keep their modes: the alpha byte fades only the modes that blend with the vertex alpha.
  - **The seed** (a seed with no leash), read in the frozen v1.42 exe and run in the oracle: the `SpellSeedGraphic`
    of the seed's magic at its power-up level (`GSpellSeedInfo::GetMagicTypeFromPULevel` 0x72AFC0 with +0x18: −1 →
    the row's +0x124, else +0x128 + 4 × level), one per magic type in the table of 42 at 0xD4E898 (a level whose magic
    is 0 shares entry 0 with every other such level), made at its first draw with `SpellSeedGraphic::Create`
    0x726F60(MapCoords(0, 0, 0), seed, the local player `g_game` + 0x18 + 0xA60 × [+0x205A59], 1.0, power-up)
    (0x68BCD9..0x68BD1D), `SetAutoUpdate(0)` (0x68BD2F), its +0x58 = 0.5 ([0xC02888]) and its +0x70 = 120
    ([0xC02884]); its power-up stays the one it was made with. It is one of the world's list of seed graphics
    (0xD9D3D0), so every 30 turns `fn_00727440` gives the flying flock's graphic its bat or dove again, and it is never
    stepped by the turn (auto update 0). Each draw: the identity scaled 0.12 ([0xC02730], all nine cells and the zero
    x, y of the place, not the 10) at (0, 0, 10) goes to `DrawUpdateAtPos`(matrix, 0.12) 0x727630, then
    `DrawSpellGraphic`(nullptr, draw now 1, not selectable 0, alpha byte) 0x68BE0B (pushed 0x68BE06..0x68BE0A: alpha,
    0, 1, 0).
    - `DrawUpdateAtPos`: +0x54 = 0.12; `fn_007270E0`(MapCoords of the matrix's place, &+0x14, &effect point):
      `MapCoords::Set` 0x603340 keeps x and z as `ftol(× 6553.6)` (z 65536) and the height above the land
      `y − GetAltitude` (the land under (0, 10), 0 off the land's blocks, 0x803090); +0x64 (the bands' point) =
      `GetLHPoint` (x, z × 10/65536, the land's height + that height, 0x605C40); the effect point the same with
      `holderHeight` (row +0x164) × 0.12 added, the mesh's MapCoords height `meshHeight` (+0x160) × 0.12 + that height.
      Oracle with the land's height 0, 13.7, −2.25: the mesh's height above it 0xBE3851EB / 0xC15E147B / 0x40047AE1,
      the bands' point (0, 0, 10), the effect (0, 0x3D1374BD, 10) for heights −1.5 and 0.3. Then `fn_007274D0` moves
      the holder effect there (vt +0x124), magnitude 0.12 (vt +0x11C), and steps it by `g_game_time_inc` with the
      zeroed process info, power 1, active (vt +0xFC).
    - `DrawSpellGraphic` with draw now: the mesh (only with `useMesh`, `IsSpellG3DObjectDrawn` 0x727690) turns as in
      the world (+0x3C += 2 × dt, fmod 2π, + 2π below 0), its size `scale` (+0x144) × 0.12. Its 3D object is made with
      `SetDynamicLighting(1)` (0x727238) and its colour is the new object's 0xFFFFFFFF (`LH3DMeshedObject` 0x8164F7):
      the draw now path never puts the land's light on it.
      - A player's seed (not a creature spell, 0x51A0B3): colour = rgb | alpha << 24, `SetDrawWithGlobalAlpha(alpha ≠
        0xFF)`, `SetPosition`(the mesh point, the angle, the size) and `DrawWithClipping` at once (vt +0x104,
        0x51A14A). Oracle at 33 ms, scale 0.8, alpha 0xBF: first row 0x3DC42E11 0 0x3BCF77A4, colour 0xBFFFFFFF.
      - A creature spell phial: its 8 × 4 frames (as on a worship icon), its pulse phase +0x38 += (0.35 for FREEZE,
        else 0.5) × dt, fmod 1, the pulse (sin(phase × 2π) + 1) × 0.5, `SetPosition`; FAT (5) scales rows 0 and 2 by
        1 + 1.5 pulse (0x519D1E), THIN (6) row 0 by 1 − 0.8 pulse and row 2 by 1 − 0.7 pulse (0x519CD2); then the jump
        table 0x51A808 by the receive type: FREEZE (0) 0x519DE4 the land's light at its place (`fn_00801C90`,
        `fn_007FEB30`) and `DrawFroz`(pulse, 0x354F8D, `GetFrozMaterial(0)`) (vt +0x15C), whatever draw now is; SMALL
        (1) 0x519F29 drawn at (1 − phase) × the size with the alpha, then at its size with alpha (alpha × 80) >> 8 and
        its own alpha on; BIG (2) 0x519E33 drawn with the alpha, then every cell × (phase + 1), the scale × (pulse + 1),
        alpha (ftol(255 − 255 phase) × alpha) >> 8, its own alpha on; INVISIBLE (7) 0x519D7D the land's light at its
        place (`fn_00801C90`, then the haze `fn_007FEB30`: nothing this close to the eye), alpha ftol(alpha × pulse),
        its own alpha on; every other type (3, 4, 5, 6 and past 7) 0x51A05B its colour as it is (0xFFFFFFFF: the
        hungry phial of the inverse spiral is drawn opaque) with its own alpha on unless the alpha is 0xFF. Each
        `DrawWithClipping` at once. Oracle (33 ms frames, alpha 0xBF): pulse phase 0x3C872B02, 0x3D072B02; FAT first
        row 0x3E33454E, THIN 0x3D5B2C9D … 0x3D70D28F, BIG's second draw 0x3DC76ABB, scale 0x3E188AE8, colour
        0xBAFFFFFF, SMALL's 0x3DC0F167 then colour 0x3BFFFFFF, INVISIBLE alpha 0x69 then 0x73.
      - `DrawWithClipping` (0x815980) keeps a model on the screen (it is at the view's middle) and leaves it out when
        (half diagonal × its scale × 300)² is short of its squared distance from the models' camera.
    - **The holder effect**: `SetAlpha`(alpha) (vt +0x12C), then with draw now `Draw_`(t, 0) at once (vt +0x104,
      0x51A291), t = 1 as the graphic is not auto-updated (0x51A27E): drawn now with the models' camera and moved on
      the screen with the model (a sprite atom: `Particle3DSprite::DrawAt` 0x67B0DF → `LH3DSprite::Draw` →
      `LH3DRender::DrawTriangle` 0x840C57). No
      other draw takes it: `PSysGlobal::DrawLoop`'s managers (fn_00672210) are the utility lists, and the world's orbs
      and icons queue their own holders from `DrawSpellGraphic`. So the effect made at (0, height, 0) of the world and
      moved to (0, ·, 10) is only ever seen inside the icon. It is made with NET 0 (`PSysInterface::Create` at
      0x7272FF), so its creation and every step of it draw from the local random stream: that is what can be told from
      outside the icon. Its target is the graphic (vt +0x114, 0x727313): a phial whose holder puts glints on its target
      (holder 92, `SF_CreatureSpellFreezeOnHolder`: seeds 12, 17, 18 and 22 to 26) has them on the icon's phial.
    - **The bands** (made at creation when the power-up is not −1): as the world's, with draw now each level's two
      draws both at once (0x51A780, 0x51A7A3), turned towards the models' eye (0, 2, 0) (`ConvertToCameraFacingMatrix`
      reads `g_camera`, set by the icon's camera), size 0.2 × 0.5 × 0.12 = 0x3C449BA6, colour the local player's with
      alpha (120 × alpha) >> 8 (0x59 at 0xBF, 0x77 at 0xFF), specular 0x141414. Oracle at 33 ms, power-up 0: the
      angles 0x3D072B02 and 0x3EAE0760, first row 0xBBE32F84 0xBB5CAE41 0xBC16B0C4.

#### Seeds, the help and the addresses

- **One-off seeds** have no icon, so with them there are no power-up gestures (only with those of a worship icon).
  SCRIBBLE does shake them off.
- The help (`HelpProfile::Trigger` 0xE..0x17): help events 14..22 with their call addresses; 19
  GestureCreatureSpecial (the creature's fight gesture, fn_005CFDE0 0x5CFF0B).
- The immersion: force feedback 3, 8, 9, 10. The gesture icons (the list 0xD4EAC8, count 0xD4EAD0): see
  [the gesture icons](#the-gesture-icons).
- The creature's own gesture chain (`SF_CreatureGestureChain`, PT 31): started only by sub_4F4B90 (the creature's
  received-spell object +0x370, its +0x198), from the Gesture sub-action's start sub_506D70 (sub-action 88, its
  name at 0xBE5FAC) and from the dance editor's sub-action (DancingDuringEditor, sub_504660). The cast adds the
  Gesture sub-action only for a miracle whose `GMagicInfo` +0x2C is set, which no creature cast has
  ([not in the original](../project/not-in-the-original.md)).
- ProcessPowerUpSystem addresses:
  - wait [0xBF1AF8] = 0.4 s; fn_005CE420 = Success (sparks fn_00689790 and immersion 3); fn_005D2770 = empty and
    reseed;
  - fn_005CF220 = recognise; SetupPowerUpGestures 0x5CEE30 / fn_005CEE80; fn_005CF270 = the level's gesture;
  - fn_005CF1C0 = selection open only with the hand ready; fn_005D3880 = the seed in the hand; Object 0x636AA0 =
    ValidToShakeFromHand (1); `IsHandReadyForObject` 0x5DC890; fn_00729AC0 = sizingGesture; GInterfaceStatus
    fn_005DCA20 / fn_005DCA40 = R's seed.
  - Icons: the walk fn_005CF040 (player +0xA48 → sites +0x34 → list +0xE0, link +0x110);
    `FindBestSpellIconForSpellSeed` 0x64BF40; `RequestSpell` 0x77FB40; CanRepeat fn_0064BD90; MaxChargeFraction
    fn_0064BB10 / fn_0077FE80; the 0x1E cancel `GPlayer::CancelSpellCharging` 0x64BCC0 (see step 4 above);
    PowerUpAvailable fn_0077FBF0; SetPowerUpCharge 0x77FC30; handlers 0x25 0x5DABA0, 0x26 0x5DABF0.

### Selection by gesture

The icons seen by the hand's selection system are those of the interface's player, in the order of the lists of its
six sites (`GPlayer` 0x64BAB0..0x64BF40). `GPlayer::FindBestSpellIconForSpellSeed` 0x64BF40 chooses, among the valid
icons of that seed, the one of the site with the most chants available.

- `FindBestSpellIconForSpellSeed` starts at best −1 (0x64BF64) and needs a strictly larger value (0x64BFBD), so the
  first site wins a tie.
- `fn_005CF040` walks the six sites' icons in list order and keeps the category's icons that are
  `ValidForRequestSpell(status, −1, 1)`. Packet 0x25 (0x5DABA0) checks `IconValidForRequest`, then requests from the
  best icon. Packet 0x26 (0x5DABF0 → `fn_0064BD50`) uses the interface's last seed type (`fn_005DCA40`).
#### More addresses from the code

- Recognition: the buffer's keypoints against the templates, 0x578C20..0x57A880.
- `GestureSystem::UpdateHeading` (0x57C710) gives the corner's turn from the one before it.
- 70 samples on the same pixel wipe the buffer (0x57BC90).
- The leash selection's test is fn_005CEFB0 (`LeashSelectionActive`); `OpenSelection` is fn_005CF010 / fn_005CF040.

#### A land change

A land change keeps the gestures' state (read 2026-10-22, #206). The gesture buffer is the GGame's (g_game +0x25006C):
every `+ 0x25006C]` operand of .text is in the GGame constructor, `GGame::Load`, `GGame::ToBeDeleted`, the power-up
pass `ProcessPowerUpSystem` and its stages (fn_005CF220, fn_005D0000, `DoLeashSelectionSystem`, fn_005D2770,
fn_005D30D0, sub_5D3340, sub_5D4AC0), the interface's own fn_005CE420 / fn_005CEAD0 / `Get3DSoundPos`, the hand's
remove (fn_005DC1E0, which wipes it when the local interface takes something out of its hand) and place
(fn_005DC330) and `GLeashStatus::SetOn`; none is a callee of `ClearMap`. The selection, the leash picker, the circle
and the cooldown are the GInterface's (+0x364, +0x371, ...), which the map's clear does not make again: only
`GInterface::SetToZero` / `Init`, at a new game. The interface's last seed type (the status's +0x34, which the R
repeats) is written only by `GInterfaceStatus::SetToZero` (−1, 0x5DBAB6, a new game) and `fn_005DCA20` (0x5DCA24),
from a seed's `InterfaceSetInMagicHand` (0x728881) and `InterfaceSetOutMagicHand` (0x728950).

## openblack

Status: **done** for the sampler, the buffer, the recogniser, the packet, the pass's order, the leash picker and the
selection, the pass's five early outs and the gesture icons with their leash and bubble models and the seeds'
graphics inside them (but the freeze phial's look, the holder effects' mists and meshes and the glints on a phial:
Pending); not ported: the immersion and the creature's fight gestures (help event 19). Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Input** (`Magic/Gestures/GestureInput.cpp`): `gestures::FeedSample` takes `sampling::InterfaceLandUnderPixel` at
  the integer cursor pixel, and keeps the point when any |c| > 1e-4f. The messages are `mouse_messages::Frame`
  (`Magic/Gestures/GestureMouseMessages.h`): the input thread runs through the frame (a message at each 40 ms wait
  that runs out while the window has the input focus, `WindowingInterface::HasInputFocus`, else 300 ms sleeps), then a
  frame in which the cursor moved ends with the move's message; each gives the ms since the one before (0 for the
  first). `mouse_messages::Handle` is the handler (above 0 ms; past 28 ms back to 0 and a sample). Frame-sized: a move
  stands at the end of its frame and the frame's time stands for the system's clock. A check run
  (`OPENBLACK_IGNORE_REAL_INPUT`) counts the window as active. Not ported: the stop while the camera's grab or turn holds the
  mouse (the camera moves then, which wipes the path every pass) and the playback's (openblack has no playback of the
  interface's recordings). The gesture window shows the messages' clock. openblack pauses the world inside the temple, so
  `FeedSample` skips as well.
- **The line under a pixel**: `Camera::SetPixelNearClip`, set from the object draw list's near clip right after
  Game.cpp computes it (the same value), and `sampling::LineThroughPixel` (the eye `Camera::GetEye`, the point
  `Camera::PixelNearPoint`). On that line `sampling::InterfaceLandUnderPixel` is fn_005E5620
  (`PickingSystem::LandUnderPixel` with the sea), and `sampling::LandOrPointAlongPixel` is fn_00800C30;
  `hand_land_point::AlongMouseRay`. openblack's land lines start at the drawn view's eye (`Camera::GetEye`), as its
  mouse ray did; the points a length along the line keep the camera's position plus the mouse ray (`PointAlongPixel`
  in `GestureInput.cpp`), as before.
- **The recogniser** (`Magic/Gestures`): `GestureBuffer`, `GestureMatch`, `GestureTemplates`. A template of more
  than 80 keypoints is kept (`GestureFile::Open` with `keepLongTemplates`), with `GestureData::laterTurns` read from
  the list's image as above (`LoadTemplates`); the matcher reads every turn through `TemplateTurn`, and a template
  whose turns run past the list's end, which cannot be known, is not taken (a deliberate stand-in: the game reads heap
  memory there). The gesture window counts such templates.
- **The packet**: `PacketFromResult` with `Projection::screenToLand` = `sampling::InterfaceLandUnderPixel` at the
  centre's truncated pixel and `Projection::alongPixel` = `hand_land_point::AlongMouseRay` on the line through the
  edge pixel. With no land openblack keeps the camera's position (distance 0, size 0).
- **The pass** (`Magic/Gestures/PowerUpSystem.cpp`): the five early outs are `PowerUpPassGoesOn` over
  `CurrentPassState`, then `ClearBuffer`: the citadel (`game_clock::IsInsideCitadel`), the locked select
  (`HandSystemInterface::HoldsLockedSelect`: the interface's creature-lock flag `_statusCreatureLocked`, which a
  creature gone while locked leaves set, or the hand's `_statusLockedObject`, set
  by the StartLockedSelect handler past its checks and cleared by the EndLockedSelect one, by the turn's pass when the
  object is gone or, when it is not a creature, the hand has no influence (for a creature or a totem the action ends
  with it, `hand_creature::StepLockedObject`) or a pick-up ends, and by `ResetTurnState`), the inactive interface
  (`interface_active::IsActive`) and
  the box (`interface_active::BoxHasMouse`, written by Game.cpp's frame pass from `input::BoxTakesInput`, with the same
  guard as the hand's box rule: not while a demo the test hook started plays). The gesture window shows which of them
  wipes the path. `FirstStage` picks the branch and closes the pickers. The fight is `CurrentFight`: the local
  player's creature (the leash system's) duelling (`CreatureFightSystemInterface::IsDuelling`, the scripts'
  IS_FIGHTING), the fight controls up as the interface's fight state 0x10 that the music reads
  (`ecs::audio_queries::LocalPlayerDirectsFight`), and the move button (`BindableActionMap::MOVE`); with the controls
  up the move button gives `GestureStage::FightMoveHeld` (`ClearBuffer`, the pass ends) and else `GestureStage::Fight`
  closes both pickers and the pass goes on at the scribble, since the fight's own gestures are not ported; the
  fighting flag keeps steps 5 to 7 off. The gesture window shows the fight and its controls.
  A moving camera (`sampling::CameraMoving`) wipes the path through `CameraMoveWipesPath`, unless a camera shake is
  going on (`camera_shake::Checkers`, whose shakes are always on); openblack has none of the camera modes that let the
  player gesture while the camera moves, so that input is false. The gesture window says when a shake keeps the path.
  `ScribbleCancelFor` picks the branch from the hand's status and the leash of the local player's
  creature (`LeashHand`, read from the leash system once a pass); the leash branch pushes
  `game_packets::Type::CreatureLeashClick`, whose handler works the leash key (`PressKey(Leash)`), which takes an
  untied leash off. `LeashSystemInterface::Shake` stays the debug windows' and the hand demos' way in. The square
  spiral is `LeashStartWanted` and `LeashSelection::Start`.
- **The leash picker** is `LeashSelection` in `InterfaceGestures` (open = +0x371, pending = +0x370, timer = +0x374):
  `LeashSelection::Active`; `LeashSelection::Stage` returns what the pass does, which pushes
  `game_packets::Type::LeashType` for a pick; `GestureSystemInterface::IsLeashPickerOpen` reads the flag. One log line
  for each open, close and pick ("Gestures: leash picker ..."). openblack reads the leash system's `Picked` and takes
  the rope while it reports none. `creature_hand_packets::ApplyLeashClick` calls `LeashSelection::LeashClickArrived`
  first; the 0x65 handler is `temple_leash::ApplyLeashType`, which, unless refused, wipes the local player's gesture
  path (`GestureSystemInterface::ForgetPath`); its readout is the gesture window's points recorded, with the picker's
  "picked leash" log line. The leash system does the same as the leash goes on (`LeashSystem::PutOnUnchecked`, and
  `TieTo` tying a leash already on), for the creature's owner, and as it comes off (`TakeOff`, a leash being on), for
  its holder; the untie to the hand and a change of type alone do not (the nice and nasty spells' change of a worn
  leash does, through `temple_leash::TakeLeashType`, as the original's spells reach fn_005E6B00)
  (`LeashSystemTest.TheLeashGoingOnOrComingOffWipesTheLocalPlayersGesturePath`). With no leash on, the off path is
  `LeashSystem::ReleaseUnleashed`, which wipes it for the creature's owner (`DETACH_OBJECT_LEASH` on a creature with
  none, and every creature taken out of the game, `creature_removal`'s `TakeOffLeash` step;
  `LeashSystemTest.LetGoWithNoLeashOnItStillWipesTheLocalOwnersGesturePath`); the creature spawner's "Detach (script)"
  runs the script's command. The script's leash to the hand (`LeashSystem::PutInHand`) puts the leash on again
  whatever it wears, so it wipes the path too
  (`LeashSystemTest.TheScriptsLeashToTheHandPutsTheLeashOnAgainWhateverItWears`).
- **The secret script**: the three tests are `SecretScriptLand`, with the land number `MapScriptGlobals::landNumber`
  (SET_LAND_NUMBER) and no multiplayer game nor playground game flag (both false); `LeashSelection::Stage` sets
  `Outcome::startSecretScript` and asks the recogniser nothing more that pass (an empty buffer matches no template);
  the pass then `ClearBuffer`s and starts it through `Locator::vm` (`LHVM::StartScript`,
  `help::script_control::k_SinglePlayerScriptTypes`), with one log line.
- **API for worship** (`PowerUpSystem.h`): `gestures::SetIconProvider(IconProvider*)`. Without a provider the
  selection never opens; `Worship/GestureIconProvider.cpp` registers its own. The provider answers:
  - `AnyRequestableIconOfCategory` (fn_0064BE40), `ForEachRequestableIcon` (the walk of OpenSelection) and
    `IconValidForRequest` (fn_0064BEC0);
  - `RequestSpell` (packet 0x25), `CanRepeat` / `RepeatLast` (0x26) and `CancelMostChargedIcon` (0x1E; despite its
    name it cancels the charge that started last, as the original);
  - `AnyIconChargingForHand` / `MaxChargeFraction` (the PHandFX charge bands);
  - `PowerUpAvailable` / `SetPowerUpCharge` (0x6A).
- **The help**: the gestures' help events are counted in `src/Help/HelpProfile.cpp` (each also into the gesture
  total); 19 (GestureCreatureSpecial) is not raised yet.
- **The gesture icons** (`Magic/Gestures/GestureIcons.h`): the pass's end hands its table to
  `GestureIconList::Update` (in the miracles' service's hand magic state, `IconState`, cleared by `gestures::Reset`
  with the map; the alpha stays), with the window's size, the local player's colour and `ShapeOf`'s lengths. Once a
  frame `FillGestureIcons` (Game.cpp, after `FillOverlayFrame`, not inside the temple) runs `StepIcons` (the fade,
  `StepIconsAlpha`, with whether the dialogue text was drawn the frame before: the help text's frame active, its
  TEXT_DRAW gate and its newest text; then each icon's `StepIconSparkles`), and lays each symbol and sparkle as a
  `billboard::Sprite` at its pixel, `screen_point::PixelPoint` at depth 10 with the pixel near clip.
  `Renderer::DrawGestureIcon` (`Graphics/RendererGestureIcons.cpp`) draws each as a Z object at key 0 of the world
  view: the symbol in `materials::k_GestureSymbol`, then the sparkles in `materials::k_GestureSparkle`, ZFUNC ALWAYS
  and no Z write. Before the symbol, `DrawGestureIconDepthBox` resets the depth of the box above the icon
  (`IconModelDepthBox`, mode 18 `ChromaDepthOnly`, ZFUNC ALWAYS), its corners put back on their pixels through the
  view's own matrices (`screen_point::WorldAtDepth`) at 1000 along the view (openblack: the original's depth is
  0.999; only the icon's model is tested against it before the frame's end, so the value is not seen), and
  `DrawGestureIconModel` draws the model (`Magic/Gestures/GestureIconModels.h`: `IconModelKindOf`,
  `StepIconModel`, the leashes' spins `IconLeashSpins` in the miracles' service's hand magic state, cleared with the
  map by `ResetIconModels` (from `gestures::Reset`),
  `StepLeashSpin`, `LeashModelMatrix`, `LeashModelBand`, `BubbleModelMatrix`, `BubbleModelDrawn`, `IconModelAlpha`,
  `IconModelScreenOffset`; tests `GestureIconModels.*` with the original's bits) with the program
  `ObjectIconInstanced` (`vs_object_icon_instanced.sc`: vs_object with `USE_DRAW_CAMERA`, its view and projection
  `u_drawViewProj` = the world's projection times `glm::lookAt(k_IconModelEye, k_IconModelFocus)`, and every vertex
  moved by `u_drawScreenOffset` and held on the screen as `DrawTriangle` does): the leash's collar mesh
  (`shared_assets::k_LeashCollarMesh`, its band as the texture's v offset) or the one-shot orb's bubble mesh, white
  set as its colour, unlit, its alpha byte as the vertex alpha, the normal mode table. The gesture window lists the
  icons, their alpha, the model each shows and the seeds' graphics.
- **The seeds' graphics** (`Magic/Gestures/GestureIconSeeds.h`, tests `GestureIconSeeds.*` with the original's bits):
  `DrawIconSeed`, from `FillGestureIcons` for an icon of `IconModelKind::SeedGraphic`, keeps one `IconSeedGraphic`
  per magic type (`MagicTypeForPowerUpLevel`) in the miracles' service's hand magic state, made at its first draw
  for the local player with its power-up and the seed's mesh (`seed_graphic::MeshIdOf`, the flying flock's bat or
  dove, made again every 30 turns by `RefreshIconSeedMeshes` from `seed_graphic::ProcessTurn`), its holder effect
  started at the map's corner with magnitude 1 (`StartForSpell`, the local stream, its player the local one), stepped
  only by its draws (`SetPerFrame`) and left out of every draw list of the world (`SetDrawn(false)`); all of them go
  with the map (`ResetIconSeeds`, from `ResetIconModels`). Each draw: `IconSeedPlacesAt` puts the point, the mesh and
  the effect through the map position with the land's height under (0, 10); the holder effect is moved there,
  magnitude 0.12, and stepped by the frame (`ProcessForSpell`, power 1, active); `StepIconSeed` turns the mesh, steps
  a phial's frames (`frame_anim::SpellIconFrame`) and pulse and gives its draws (`affine::PlacementMatrix`, the FAT
  and THIN squashes, the BIG and SMALL second draws, the INVISIBLE phial in the land's light: no colour set, so
  `vs_object` takes the land's light at its place), each kept or left out by `IconModelInReach`, then the bands
  (`seed_graphic::BandRotation`, `billboard::BandToEye` towards `k_IconModelEye`, `k_IconSeedBandAlpha` 120); the
  effect's alpha is the icons' and its sprites are collected as it was stepped (t = 1). `Renderer::DrawGestureIcon`
  draws the meshes (`DrawGestureIconModel`: lit by the models' light, their colour set, their own alpha through the
  global alpha table when the original turns it on), then the holder effect's sprites (`DrawGestureIconEffect`:
  `DrawParticleSprites` facing the models' camera, every corner through `IconPointInWorld`, which takes it to the
  screen with the models' camera, moves and holds it there as `vs_object`'s `USE_DRAW_CAMERA` does and puts it back
  into the world at its depth), then the bands (their own additive material, unlit). The freeze phial's frozen look,
  the holder effect's mists and meshes and the glints on its target are not drawn there yet (Pending).
#### Where the code is

- `GestureBuffer`: `WrapDifference` (fn_00578890), `AddSampleAtLastWorld` (fn_0057C8F0), `KeypointIndices`
  (fn_005788D0), `PrevNonZero` (fn_0057BE10), `PrevCorner` (fn_0057C1A0), `PrevAnchor` (fn_0057BE70).
- `GestureMatch`: `KeyBox` (fn_00578D70), `ComputeAspect`, `DirectionMatches` (fn_0057A0E0), `BuildFromSystem`;
  `GestureTemplates`: `LoadTemplates` (false if the data is short); `GestureShapes`: `Path::At` (fn_00687E60).
- `GestureInput`: `ScreenToLand` (fn_005E5620), `CameraMoving` (`GCamera::IsMoving`); `GestureDebugHooks`:
  `TemplateStroke` (the template tool's z × width / height).
- `PowerUpSystem`: `SelectionOpenAndHandReady` (fn_005CF1C0), `ReseedBuffer` (fn_005D2770),
  `SetupPowerUpGesturesInner` (fn_005CEE80), `PowerUpLevelGesture` (fn_005CF270), `PowerUpStage` (the recognitions of
  fn_005D0000, its effects left to the pass), `RepeatLookedFor` / `RepeatStage` (step 6); the sized seed's cast rule is
  `ecs::hand_seed::SizingGestureAllowsCast`.

- **A land change** keeps the gestures' state (`gestures::State`: the buffer, the selection, the leash picker, the circle, the cooldown) and every player's last seed type (`worship::player::LastSeedType`): only a new game resets them (`magic::OnNewGame` from `Game::PrepareNewLand` with `land_load::Resets::newGame`, 2026-10-22, #206; before, every load did, from the gesture system's new-land pass and `worship::OnLoadMap`). The Land Island window's "Kept by a land change" shows the last seed type.

### Test hooks

- `OPENBLACK_GESTURE_TRACE=1`: the help events also go to the log.
- `test_gestures`, `realData`: each of the 14 miracle gestures recognises its stroke and none of the other 13.
- `test_gestures`, the rest of the pass: `PowerUpSystem.*` (the citadel, each early out on its own, the scribble on a
  held leash, a moving camera wiping the path unless it shakes), `LeashPicker.*` (the picker, the fight with its
  controls up, the secret script's square wave), `PowerUpStage.*`, `RepeatStage.*`, `Selection.*`,
  `GesturePacket.*`, and the long templates
  (`GestureTemplatesLoader.aTemplateOfMoreThanEightyKeypointsReadsOnIntoTheList`,
  `Gestures.aLongTemplateMatchesOnItsLaterTurnsAndOnePastTheListNever`).
- `test_gesture_mouse_messages`: `GestureMouseMessages.*`, the input thread's 40 ms messages, the 300 ms looks of an
  inactive window, a move's time and the handler's 28 ms.

## Pending

- **`Gestures.jty`**: a changed file's template of more than 80 keypoints whose turns run past the list's last
  object reads heap memory in the original, which cannot be known; openblack never takes it. The game's file has at
  most 26 keypoints a template.
- **The temple in a multiplayer game** (no pause): the original samples in the temple with the 0.2 near clip;
  openblack has none.
- **The creature's fight gestures** (fn_005CFDE0: the special move, the fight miracles) and help event 19 are not
  ported: they need the player's fight controls, which openblack lacks (the interface's fight action, packets 0x22 and
  0x5A, the fight's intersection on the opponent's mesh; see
  [fights.md](../creature/fights.md) and the fighting tracker's player controls).
- **The gestures' eye while the falling spell draws its own view** is not read (which `g_camera` the original has
  then).
- **Inside a seed's gesture icon**, what the seed's graphic draws is ported except:
  - the freeze phial's frozen look (`DrawFroz` with the pulse and `GetFrozMaterial(0)`): openblack has no frozen
    draw of a model (the frozen creature is a tint, `creature_spells::FrozenTint`), so the icon shows no freeze phial;
  - the holder effect's mists (the storm's cloud, the water's whole effect) and mesh atoms (the shield's sphere): only
    its sprites are drawn with the models' camera; the mists' program and the effects' mesh instances are drawn
    with the world's;
  - the glints on the phial (holder 92 of seeds 12, 17, 18 and 22 to 26 aims them at the graphic): openblack's glint
    targets are entities with a mesh, and the icon's graphic is not one;
  - (approximate) the bands' turn towards the eye is the world's bands' `billboard::BandToEye`, within a few float
    steps of `ConvertToCameraFacingMatrix` (oracle: 0xBBE32F84 against 0xBBE32F88).
  The depth reset of the box above the icon is placed with `screen_point::WorldAtDepth`, which with openblack's
  left-handed view puts its points behind the eye (clip w = −depth), so the reset is clipped; the help's
  did-you-know bubble and the key-or-mouse panels that use it are clipped the same way. The immersion is not ported
  either.
- **Still not found**: who sets bit 0x02 of m_Buttons and who reads the 40 s cap of the repeat (the address of
  m_Buttons is not located). Verify with `xref.py readers` on those fields.
