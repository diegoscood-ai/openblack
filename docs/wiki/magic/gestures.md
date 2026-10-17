# Gestures

## Summary

Miracles are chosen and powered up by drawing shapes with the hand. The original samples the mouse (with no button
held) every 28 ms onto the land under the cursor, keeps the last 80 samples, finds their corners on the fly and
compares the sequence of turns, the first direction and the aspect against 81 templates read from `Gestures.jty`.
A recognised gesture gives a packet with a point and a size (the circle of the storm and the shields). Once a pass,
`ProcessPowerUpSystem` decides which gestures are looked for: the power-ups of a charging seed, the selection
spirals, the scribble that cancels, the repeat, and the creature's leash picker.

openblack ports the sampler, the recogniser, the packet and the pass's order bit for bit; the HUD gesture icons, the
immersion, the creature's fight gestures and four of the pass's five early outs are not ported. The gesture's particle rules are on [Gesture rules](../physics/particle-rules.md#gesture-rules).

## The original

### Gestures: the buffer and the recogniser

#### The samples

- The sample is given by the mouse **with no button**. It is CMouse message 0, every 28 ms of mouse events
  (`fn_005CEAD0`), with the terrain point under the cursor, or that of the last sample if it is outside. There are no
  samples while paused nor during the 0.4 s following a recognition. If the camera changed position in the frame
  (`GCamera::IsMoving`), **the buffer is cleared** on every `ProcessPowerUpSystem`.
- The terrain point is the interface's land under the message's integer pixel: fn_005E5620 at 0x5CEB0F (depth 0, the
  sea always, no depth out: 0x5CEB00 `push 0`; then `KeptInReach`). It is dropped as off the land when all three of
  its fabs are not above (double)1e-4f (0x5CEB18..0x5CEB4F, `test ah, 0x41`: a NaN counts as not above); then
  fn_0057C8F0 (0x5CEB86) reuses the newest sample's point, only when there is one (0x5CEB81, 0x57C8F8). Callers: the
  message 0 handler 0x5D99E0 (after fn_005CECF0 stores the pixel at GInterface +0x428 and fn_005D5980 finds the
  interface's land at the same pixel) and the reseed fn_005D2770 at 0x5D27A6, with +0x428.
- Mouse sampling: `GGame::MouseHandler` 0x54FFE0 adds the events' ms into [0xD019CC] (0x55001B) and sets it to 0,
  does not subtract (0x55006C); then `CMouse::ProcessPosition` 0x61A110. `GCamera::IsMoving` is +0x74, set every frame
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
  moving are CameraModeNew3 in the arena, Dance and Follow.
- Order of each call:
  1. the clearing by the camera, the 0.4 s wait and the expiry of the pending circle (5 s);
  2. **the circle**: with the action pressed (m_Buttons 0x200) and a seed with `sizingGesture` (CIRCLE: storm,
     shield and physical shield), the circle stores position and size;
  3. one else-if chain (0x5CF5A2..0x5CF697); a stage that returns true ends the pass:
     - the creature's fight: with the local creature fighting (`Creature::IsFighting` 0x47B1C0) and the interface's
       action state +0x44 == 0x10, both pickers close (+0x371 = +0x364 = 0) and its gestures run (fn_005CFDE0);
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
  changes, and the temple's pick is set). For the local player it also wipes the gesture buffer (g_game +0x25006C is
  the `GestureSystem`; 0x5E6B59..0x5E6B81, as `SetOn` does at 0x5E7102) 📼 1.20 only.
- Before any pick the hand's current leash is the rope (`GLeashStatus` +0x1C is 2 at init, 0x5E6950).

#### Seeds, the help and the addresses

- **One-off seeds** have no icon, so with them there are no power-up gestures (only with those of a worship icon).
  SCRIBBLE does shake them off.
- The help (`HelpProfile::Trigger` 0xE..0x17): help events 14..22 with their call addresses; 19
  GestureCreatureSpecial (the creature's fight gesture, fn_005CFDE0 0x5CFF0B).
- The immersion: force feedback 3, 8, 9, 10. The HUD gesture icons: `DisplayGesture` fn_0068ABA0, `S_Gesture0/1.raw`
  (not read).
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

## openblack

Status: **done** for the sampler, the buffer, the recogniser, the packet, the pass's order, the leash picker and the
selection; not ported: the HUD gesture icons, the immersion, the creature's fight gestures (help event 19) and four
early outs. Port decisions: [port decisions: magic](../project/port-decisions-magic.md).

- **Input** (`Magic/Gestures/GestureInput.cpp`): `gestures::FeedSample` takes `sampling::InterfaceLandUnderPixel` at
  the integer cursor pixel, and keeps the point when any |c| > 1e-4f. openblack pauses the world inside the temple, so
  `FeedSample` skips as well.
- **The line under a pixel**: `Camera::SetPixelNearClip`, set from the object draw list's near clip right after
  Game.cpp computes it (the same value), and `sampling::LineThroughPixel` (the eye `Camera::GetEye`, the point
  `Camera::PixelNearPoint`). On that line `sampling::InterfaceLandUnderPixel` is fn_005E5620
  (`PickingSystem::LandUnderPixel` with the sea), and `sampling::LandOrPointAlongPixel` is fn_00800C30;
  `hand_land_point::AlongMouseRay`. openblack's land lines start at the drawn view's eye (`Camera::GetEye`), as its
  mouse ray did; the points a length along the line keep the camera's position plus the mouse ray (`PointAlongPixel`
  in `GestureInput.cpp`), as before.
- **The recogniser** (`Magic/Gestures`): `GestureBuffer`, `GestureMatch`, `GestureTemplates`. openblack refuses a
  `Gestures.jty` with a template of more than 80 keypoints (Pending).
- **The packet**: `PacketFromResult` with `Projection::screenToLand` = `sampling::InterfaceLandUnderPixel` at the
  centre's truncated pixel and `Projection::alongPixel` = `hand_land_point::AlongMouseRay` on the line through the
  edge pixel. With no land openblack keeps the camera's position (distance 0, size 0).
- **The pass** (`Magic/Gestures/PowerUpSystem.cpp`): of the early outs only the citadel one is ported
  (`PowerUpPassGoesOn`, `ClearBuffer`). `FirstStage` picks the branch and closes the pickers; openblack's `fighting`
  stays false. `ScribbleCancelFor` picks the branch from the hand's status and the leash of the local player's
  creature (`LeashHand`, read from the leash system once a pass); the leash branch pushes
  `game_packets::Type::CreatureLeashClick`, whose handler works the leash key (`PressKey(Leash)`), which takes an
  untied leash off. `LeashSystemInterface::Shake` stays the debug windows' and the hand demos' way in. The square
  spiral is `LeashStartWanted` and `LeashSelection::Start`.
- **The leash picker** is `LeashSelection` in `InterfaceGestures` (open = +0x371, pending = +0x370, timer = +0x374):
  `LeashSelection::Active`; `LeashSelection::Stage` returns what the pass does, which pushes
  `game_packets::Type::LeashType` for a pick; `GestureSystemInterface::IsLeashPickerOpen` reads the flag. One log line
  for each open, close and pick ("Gestures: leash picker ..."). openblack reads the leash system's `Picked` and takes
  the rope while it reports none. `creature_hand_packets::ApplyLeashClick` calls `LeashSelection::LeashClickArrived`
  first; the 0x65 handler is `temple_leash::ApplyLeashType`.
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
  total); 19 (GestureCreatureSpecial) is not raised yet. The `LookingFor` table is filled in (lf types 2 and 4..6),
  not drawn.
#### Where the code is

- `GestureBuffer`: `WrapDifference` (fn_00578890), `AddSampleAtLastWorld` (fn_0057C8F0), `KeypointIndices`
  (fn_005788D0), `PrevNonZero` (fn_0057BE10), `PrevCorner` (fn_0057C1A0), `PrevAnchor` (fn_0057BE70).
- `GestureMatch`: `KeyBox` (fn_00578D70), `ComputeAspect`, `DirectionMatches` (fn_0057A0E0), `BuildFromSystem`;
  `GestureTemplates`: `LoadTemplates` (false if the data is short); `GestureShapes`: `Path::At` (fn_00687E60).
- `GestureInput`: `ScreenToLand` (fn_005E5620), `CameraMoving` (`GCamera::IsMoving`); `GestureDebugHooks`:
  `TemplateStroke` (the template tool's z × width / height).
- `PowerUpSystem`: `SelectionOpenAndHandReady` (fn_005CF1C0), `ReseedBuffer` (fn_005D2770),
  `SetupPowerUpGesturesInner` (fn_005CEE80), `PowerUpLevelGesture` (fn_005CF270).

### Test hooks

- `OPENBLACK_GESTURE_TRACE=1`: the help events also go to the log.
- `test_gestures`, `realData`: each of the 14 miracle gestures recognises its stroke and none of the other 13.

## Pending

- **`Gestures.jty`**: the original's loader 0x579AF0 keeps a template of more than 80 keypoints (its count is a byte)
  and clears the whole list on a short read; openblack refuses such a file. Queued as a fix; the game's file has at
  most 26 keypoints a template.
- **The early outs** of `ProcessPowerUpSystem` other than the citadel's are not ported: the interface status's +0x24
  bit 0x20, an inactive interface, the status's +0x3C and a box holding the mouse. In a multiplayer game (no pause) the
  original samples in the temple with the 0.2 near clip; openblack has none.
- **The creature's fight gestures** (fn_005CFDE0) and help event 19 are not ported.
- **The leash type's packet 0x65** wipes the local player's gesture buffer (0x5E6B59..0x5E6B81) 📼 1.20 only; openblack's
  handler does not.
- **The gestures' eye while the falling spell draws its own view** is not read (which `g_camera` the original has
  then).
- **The HUD gesture icons** (lf types 2 and 4..6 are filled in, not drawn) and the immersion are not ported.
- **Still not found**: who sets bit 0x02 of m_Buttons and who reads the 40 s cap of the repeat (the address of
  m_Buttons is not located). Verify with `xref.py readers` on those fields.
