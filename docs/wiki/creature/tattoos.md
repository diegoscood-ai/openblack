# The tattoo editor

## Summary

In the temple's Creature Cave a copy of the player's creature rests by the fire, asleep or sitting. Clicking on it
zooms the camera in and opens the tattoo editor: the player turns the view around the creature, drags symbols onto
eight places on its body, and the skin is rewritten with each drop. While the editor is open the copy turns to face
the eye and looks at the pointer; left alone it shows off, scratches or yawns; closed, it turns back to the fire and
rests again.

openblack makes, poses and draws the copy with the original's draws on the local stream, opens and closes the editor
with the camera's zoom, finds the places on the posed body and drops the symbols as the original does. The mesh pick
that finds the creature under the mouse and `SET_CREATURE_IN_TEMPLE` come from raffclar's port and are not yet checked
against the original. Still to port: the copy's hair and the snore.

## The original

### The tattoo editor in the Creature Cave

Read in `runblack.exe` (`CreatureRoom::InitEngine` 0x7878E0, `CreatureRoom::Draw` 0x786840,
`CreatureRoom::DrawAdditional` 0x788630, `CreatureRoomCamera::UpdateMain` 0x78A3E0, its zoom fn_00789FA0,
`TattooEditor::SetC3D` 0x543200, `UpdateC3D` 0x542C50 and `ControlCallback` 0x543440).

#### Where the creature stands

`InitEngine` (0x787B4E..0x787F2D) makes the room's copy of the player's creature only when the player has one, it has
a 3D body and `CreatureRoom::CreatureInTemple` [0xD96270] is set (script `SET_CREATURE_IN_TEMPLE`, default 1, GScript
0x6FF8D1, saved). Without the copy the zoom goes to points 44 / 49 and no editor opens (0x786C99). The copy stands on
the line from movement.l3d's first place [0xC5E424] to its third [0xC5E484] (`CPController::Init` 0x4699C0 copies
the mesh's first three 4×3 matrices to 0xC5E400, 0xC5E430, 0xC5E460; the second's point [0xC5E454] is the fire). The
copy takes the creature's own size (the vfunc at +8 with physical +0x6C: `SetSize`, so +0x90 is the size kept between
0.05 and 4).

- **The coin.** Then a coin, `LocalRand(2)` (0x787C21), always drawn, counts only below a size of 1.25 (0x8CF998):
  t = (2 − size) × 0.5, with the coin t = (t − 0.5) × 2, held to [0, 1] (0x787C52..0x787C9E); the place is first +
  (third − first) × t. The coin is kept at room +0x100 and cleared when the size is 1.25 or more (0x787C38..0x787C48,
  `fcomp; test ah,1`: kept only below), so a large creature never sleeps.
- **The rest.** It starts static action 0x1D (sleep) with the coin, or 0x26 (sit) and `SetLookPoint(fire)`, each with a
  **second `LocalRand(2)`** as the action's flag (0x787D41..0x787D92; the start action's flag at 0x787D4D / 0x787D6B),
  and then turns to face the fire: `angle_correct` 0x7FAAF0 of `heading_from_direction_vector` 0x7FAA50 (fire −
  place), at body +0x84 (0x787D97..0x787DDB). `heading_from_direction_vector` is fn_007FAA50 / `GetYAngle` of
  [Coordinates and angles](../engine/coordinates-and-angles.md) (x² + z² ≤ 1e-6 [0x9A2BAC] → 0, else
  `atan360`(−z, x), `atan360` being `ArcTanOctant` 0x7FA990) and `angle_correct` is `WrapAngle` (> π [0x8C36A0] →
  − 2π [0x8AB210]; < −π [0x8C79A4] → + 2π). So the room's start draws **two** `LocalRand(2)`, the coin then the flag,
  and between them what the copy's first moment of time draws.
- **The copy's object.** Its vtable is `CreatureCitadel`'s 0x99EBD0 (written at 0x787BDD, over the `LH3DCreature`
  one). Its vt+0x28 (0x787D10, the place pushed at 0x787CC3) is 0x7865D0: it returns [0xC5E428], the first place's y,
  kept at room +0x108; no draw. Its vt+0x14 (0x787D3E, argument 1) is `LH3DCreature::UpdateTime` 0x481DF0, one
  millisecond of the copy's time, before its rest action starts. The copy constructor 0x47F490 runs `Init` 0x47FC20
  (body state +0x4994 = 0, the blink timer +0x5468 = 1000, state +0x5470 = 0), `LoadBinary`, `SetSize`, and copies the
  three morph targets, the evil-good one +0x9C held to [−1, 1] (one that is not a number as −1, 0x47F67F..0x47F6B3).
  Its `SetSize` 0x480530 holds the size the same way as `creature_morph::ClampScale` (not above 0.05, or not a number:
  0.05; else below 4 kept, else 4; 0x480563..0x480599), and InitEngine's own `SetSize` (vt+8 with physical +0x6C,
  0x787C1C) comes before the coin, whose clearing reads that held size (+0x90).
- **The first moment.** In that millisecond: `StateSwitch` skips state 0 (the jump table starts at state 2,
  0x4851AD), `StateAction` state 0 is `DoStandingAction` 0x48D250 (no draw), the blink timer goes to 999 (its
  `LocalRand` at 0x482028 only at 0 or below in state 2), and `AddEvilGoodSparkles` 0x481FE5 **draws** for a creature
  whose +0x9C is over 0.2 (three sparkles of six `LocalRand`: 101, 101, 101, the bones, 6, 2) or under −0.2 (three of
  five: no kind), while [0xC64204] is clear or the copy (see [The evil and good sparkles](drawing.md#the-evil-and-good-sparkles)).
  Nothing else it calls one level down draws (the `GameRand` callers in `StateSwitch` and `StateAction` are in other
  states).
- **The second moment.** Then the flag's `StartStaticAction` 0x4843A0 (`StateSet(2)` 0x484EC0, case 0x484FE5: no
  draw) and the heading, and **a second `UpdateTime(1)`** (vt+0x14 at 0x787DEE), now in state 2: `StateSwitch`
  0x485461 only advances the start animation, `StateAction` 0x486A61 only `AdvanceCyclic`, and the sparkles draw again
  as in the first. Then vt+0x10 (0x787DF9) is `MorphTexture` 0x48D540 (no draw; it redraws the tattoo and the blood and bruise marks into the skin) and vt+0x18 (0x787E04)
  `PrepareForDrawing` 0x4ED320, which plays the sound events queued in that moment (`PlayASoundEffect` 0x483290:
  actions 3, 4 and 5, the footsteps and the stamp, make 3, 6 or 12 dust puffs through fn_004ED250, three
  `LocalRand(201)` each, 0x483485..0x4834A4; the table at 0x483600). An event is queued by `Morphable::CheckSounds`
  0x617970 from the animations advanced in that moment, when its time is at or after the start and before the end of
  the stretch played (1 ms or less here); in the second moment only the rest's start animation N - 1 advances, and in
  the first nothing that checks sounds. No species has any event on those animations: in every `Data/CTR/*.cbn` (the
  17 species, read with openblack's `morph::MorphFile`), animation 28 (sleep's start) and 37 (sit's start) have no
  extra data at all, where 29 (the sleep) has its `SnoreIn` (action 8; at 2037 ms for the ape, 70 ms for the horse).
  So nothing is played and nothing drawn there.
- **The draws, in sum.** The room's start therefore draws, before the smoke: 2 for a neutral creature, 38 for a good
  one, 32 for an evil one. The smoke comes after the copy (`LH3DSmoke::Create` 0x787F49, then six `LocalFloatRand` for
  each smoke read, 0x787F9E..0x78802E); the CRT `Random` draws at 0x7879BD..0x787A3A come before it, on the C
  library's stream.
- **Once a visit.** InitEngine runs once a visit whether or not it makes a copy, but every draw comes after the test
  for a creature with a body, and the world is paused inside the temple. A land change never meets the room: the rooms
  close (`CloseEngine` for all seven, fn_00793D00) only from `LeaveInsideCitadel`, `DoCitadelMultiplayer` and
  `~Temple` (from `GGame::Close` at exit), and every caller of `GGame::LoadMap` 0x552F40 is outside the citadel or
  leaves it first: sub_54C930 (the setup box's answer 0x2712, which loads `scripts\landT.txt`) calls
  `LeaveInsideCitadel` when g_game +0x205A28 is 1 (inside, 0x54C962..0x54C96B) before its `InstantSaveGame`,
  `StopAllTasks` and `LoadMap`; `StartPlaygroundGame` is called only from `GGame::Init`; sub_641570 has no direct
  caller (the start); `GScript::LoadMap` is the `LOAD_MAP` native, run by scripts.

#### How the copy is posed and drawn

`CreatureRoom::DrawAdditional` 0x788630 (vt+4, called by `TempleRoom::Draw` at 0x79A213 each frame the room is drawn)
does it all only when its argument is false, the room's own pass (0x7886E5; with true, the main room's reflection, it
goes straight to `TempleRoom::DrawAdditional`). In order, after the name scrolls: while the copy sits (room +0x100 =
0), `LocalRand(1000) == 23` (0x7887AB..0x7887C3) starts concurrent action 0xCA (`StartConcurrentAction` 0x484360: only
when none plays, +0x5738 = 0, and the species has animation 0xCA = 202, the yawn; +0x5738 = 0xCA, its time +0x5734 =
0). Then `UpdateTime(min(g_delta_time, 200))` (vt+0x14, 0x7887D5..0x7887EF: the frame's real ms, not the game's),
`PrepareForDrawing` (vt+0x18), the colour, the sun, `DrawNow`, the sun put back.

- **Inside each `UpdateTime`** 0x481DF0, in this order: `ProcessBreath` (0x481DFC, no draw), `AddEvilGoodSparkles`
  (0x481FE5, the draws of the room's start, every call), the blink timer +0x5468 less the step +0x48B4
  (0x481FF1..0x482058: at 0 or below open, closing 200, opening 200, then open again with `LocalRand(+0x546C) +
  (+0x546C >> 1)`, the local stream; `Init` starts it at 1000, interval 5000, open), the body's state
  (`DoStaticAction` 0x4875D0 draws only for actions 0x20 and 0x23, not for the sleep or the sit; `StateSwitch` 0x48256A
  and `StateAction` 0x482571 come after `OperateFace` 0x4824B1 and the concurrent action, then `UpdateLook` 0x482578),
  and the concurrent action (0x4824B6..0x482514): its time goes on by the animation step +0x48BC, halved while +0x5730
  is set, and it ends at the animation's length.
- **In the frame**: `Temple::Draw` calls each drawn room's `PreDraw` (vt+0x8, 0x79465E), which runs `InitEngine` while
  the room's engine is not up (0x799F90), then its `Draw` (vt+0xC, 0x794666). So the first frame the room is drawn
  makes the copy (its two 1 ms moments) and its smokes, and then, in the same frame, draws the copy with its yawn
  chance and a full moment of the frame. `CreatureRoom::Draw` 0x786840 calls `TempleRoom::Draw` first (0x786874, the
  copy's `DrawAdditional` at 0x79A213) and only then moves its smokes on: 16 at +0x480, each with three
  `LocalFloatRand` (0x78690D..0x78693B) and fn_007F8E00 (the smoke's own step, on the CRT `Random`),
  0x786903..0x786984, then 2 at +0x4C0 (0x786986..0x78699C) and the four objects at +0x4C8 (vt+0x108). `DrawNow`'s own
  three `LocalRand(201)` (0x48E554..0x48E59A) are drawn only when +0x5460 is 3, and nothing in 0x47F000..0x492000
  writes 3 there (`Init` and `StateSet` write 1 or 2). `StartStaticAction` 0x4843A0 plays N - 1, N and N + 1 (each
  checked with `GetAnim`), so 0x1D is 28, 29, 30 and 0x26 is 37, 38, 39; `SetLookPoint` 0x482C90 writes the look target
  +0x48C0 (unless [0xC64210] is 0 and the creature's +0x24 has 0x10, not read further). The copy's `Init` 0x47FC20
  clears the look target, the head's turn (+0x4860..+0x486C), the face (+0x49AC..+0x49B8), the action and the
  concurrent action.
- **The colour** is 0xFFE0E0E0 through `ApplyCitadelColoring` 0x7867B0: each byte, the alpha too, times
  `Temple::CitadelColour` [0xE05FE8]'s over 256, and the specular out-argument set to `Temple::CitadelSpecular`
  [0xE05FE4]. Then fn_0084A4B0 (0x78883D) with the copy's position (+0x78) fades it with depth: the depth is the
  position through the third column of `g_current_matrix`, stored as a float; nearer than [0xF03150] takes entry 0,
  farther than [0xF0314C] entry 1023, else entry `ftol((d - near) * [0xF03154])` held to 0..1023; each of red, green
  and blue times the entry's byte, over 256. The table is built by fn_0084A5B0, called only from `Temple::Draw`
  (0x79461C) with near and far from the active room (`Temple` +0x1C, its +0xE4 / +0xE8; 30 and 80 with none) and 100:
  [0xF03154] = 1024 / (far - near) as a float, and entry i is 0xFF000000 plus three times the byte (255 v) >> 8 with
  v = 256 - ftol(156 i / 1024). `TempleRoom`'s constructor sets 60 and 200 (0x798971), and `CreatureRoom::Draw` sets
  40 and 150 at its start (0x786849): no other writer of +0xE4 / +0xE8 in the rooms' functions. The colour goes to the
  body's 3D object (Morphable +0x482C) by `SetColorSpecular` (vt+0x2C, 0x7F9770: the colour in edx, the specular
  pushed 0, 0x78885B), after vt+0xA0 with 1 (0x7F9910: sets bit 0x10000 of its +4, which a bare object starts with):
  so the copy is drawn with **no specular**, the temple's specular computed and dropped. The table's second colour
  (entry +4, the temple's light times 0.4 times i / 1024, rounded) is returned by fn_0084A4B0 and dropped by
  `DrawAdditional`. `CreatureRoom::Draw` 0x786B43..0x786B59 colours the belts and medals through the same two routines.
- **The sun** (`SetSun` 0x81E1F0, the engine's one model light) is moved to the fire [0xC5E454] for `DrawNow`
  (0x788886) and put back after it (0x78891D).
- **The fade's arithmetic** (fn_0084A4B0 0x84A4B0..0x84A545, at the game's 24-bit precision): the depth is
  `((m[0x20] z + m[0x14] y) + m[8] x) + m[0x2C]`, stored as a float; `d < near` (unordered too, so a depth that is no
  number) takes entry 0, `!(d <= far)` entry 1023, else `ftol(float(float(d - near) * [0xF03154]))` held to 0..1023.
  Run natively through the oracle (fn_0084A5B0 then fn_0084A4B0, 923 depths in each of 40/150, 60/200 and 30/80,
  random ones and each step's start and the floats either side, and a NaN), every share equals that float model; a
  double product lands one step early just before 22 of 751 step starts (49.8828125: step 92, share 241). Since a
  NaN never reaches the `ftol`, its 0x80000000 does not matter.
- **Where in the frame it is drawn**: `TempleRoom::Draw` draws the room's own mesh first (`InnerRoom::Draw` 0x799FF7,
  through the mesh draw fn_00809E50, which never queues into the Z-sorter: neither it nor its callees call
  `LH3DZSorter::NewZObject` 0x83F310), then `DrawAdditional` (0x79A213), then `DrawGlow` (0x79A21E). After it
  `CreatureRoom::Draw` moves on and draws its smokes (0x786903..0x78699C; `LH3DSmoke::AddDrawing` queues them in the
  Z-sorter, 0x7F8D8E) and then the belts and medals (0x786B43..0x786B7D). So the copy comes after the room's own
  translucent primitives (its waterfall, its glow meshes) and before its glows, smokes and belts and medals.
- **The copy's hair.** The copy constructor's `LoadBinary` 0x47F5F8 runs `Morphable::ReadBinary` 0x617AE0, whose
  `AddHairGroup` and `HairGroup::ReadBinary` 0x617C1B..0x617C23 give the copy hair groups of its own, drawn by its
  body's draw; by which time they swing is not read.

#### The editor's view

In the same place `InitEngine` sets the view the editor turns: r = size × 15 held to [1, 20] (0x8C2C40, 0x8C7658), the
point looked at [0xDE0F48] = place + (0, 0.5 r, 0) and the eye [0xDE0F58] = place + (−1.5 r, 0.75 r, 0) (0x900C6C,
0x8AB274). The globals are written only there and by the editor's turning (`ControlCallback` 0x5434F3..0x5436DC, the
main menu's own view (0, 20, −30) / (0, 7, 0) at 0xCD04F0 / 0xCD04E0 when the editor has no cave), so the view is kept
between openings. A room's `InitEngine` runs when its engine is not up (+0xE0, from `TriggerIntroCamera`, `PreDraw`
and `Update`); its close (vfunc +0x2C, `CloseEngine` 0x798AF0 clearing +0xE0) runs for all seven rooms when the player
leaves the temple (fn_00793D00) and not on `Temple::GoToRoom`, so the place, the coin and the view are made once a
visit to the temple.

- **The view's lowest tip.** As the editor turns the view (`ControlCallback` 0x5434F3..0x5436DC), the eye's height
  above the point looked at is held at no less than the ground distance between them × 0.125 (the float at
  [0x8AB620], read at 0x5435D0). The editor's turn steps by the frame's whole milliseconds (0x54354E).

#### How a click finds it

`UpdateMain` (0x78A932..0x78AA81) looks for targets 0x31..0x35, from 0x31 (the creature) only while the player has a
creature. The creature's point is the room mesh's point 49 (0x31), projected and 10 pixels lower, but its reach is
(0, 0) (0x99EC04), so it takes only that pixel; it is found by the pick `DrawAdditional` makes as it draws the copy
(0x7888B2 `DrawNow` with the selection point at the mouse, then `CheckRegionOnScreen` and the mesh's vfunc +0x1E0):
flag [0xDE0F8C] set (0x788983) makes the creature the target at once, ahead of the others (0x78A9AA).

- **The pick.** After `DrawNow` (0x7888B7..0x788996) the selection pixel `g_selected_px/py` is set to the mouse
  (`special_jc_x/y`), `g_last_selected_box` cleared, `CheckRegionOnScreen` 0x868C80 run on meshes[CurrentMesh]'s box
  (+0x14) with the body's 3D object (+0x482C), the pixel put back; the flag is set when `g_b_last_on_screen` and
  `g_last_selected_box` (the camera within the sphere, or the mouse strictly inside its circle, 0x868F03..0x868F40)
  and the body object's vt+0x1E0 (type 3, vtable 0x9A3068: fn_00811950) holds the mouse pixel, and then
  `Get3DPointFromScreen`(mouse, `g_last_distance`, the box centre's depth) goes to [0xE360C8], which
  `HandStateCitadel::Update` (0x5B0D14) and `CreatureRoomCamera::UpdateMain` (0x78A657) read. fn_00811950 takes the
  object's LOD (vt+0x1DC = fn_007F9C60, +4 bits 21..23), its matrix times `g_world_to_clipping`, its posed vertices
  (+0x80, 0x30 bytes each) and, for each sub-mesh whose status bits (+0 >> 29) meet [0xC37D94] and whose LOD
  (+0 >> 4 & 0x3F) is not above the object's, each primitive through fn_00855440, the first that holds the pixel
  winning.
- **Read 2026-10-22.** fn_00811950 does not set [0xC37D94]: it is what the body's own draw just left (fn_0080DB30
  0x80DCB9, vt+0x1D4 = 0x7F9C30, the object's +4 & 7), which openblack takes as `status == 0 && (lodMask & 1)`.
  `CheckRegionOnScreen` 0x868C80 is `screen_pick::CursorOverSphere` step for step: the radius is the box's +0x1C ×
  the object's scale +0x44, the centre the box's middle through the object's matrix plus its position, off the screen
  when its depth + the radius is nearer than the near plane, over it when the camera is inside the sphere, else the
  screen circle (r = R × near / depth × width / 2 / `g_meter_width_screen_on_2`) must reach the screen and hold the
  mouse strictly. Not read: the box's +0x1C against openblack's half diagonal, the near plane over
  `g_meter_width_screen_on_2` against openblack's x scale, the body's +4 & 7, and fn_00855440 against
  `screen_pick::FirstHit`. The point at [0xE360C8] is the temple's cursor point, also written by `TempleRoom::Draw`.

#### The zoom, the opening and the closing

- **The zoom.** A click on a target with the zoom at 0 calls fn_00789FA0 with its number + 1: for the creature the eye
  is the room mesh's point 44 and the point looked at point 49, as for the belts and medals, the zoom's target is 1
  (+0x470) and the camera's state +0x4AC is 1 (0x78A0A7, 0x78A0B1). The zoom comes in by the frame's seconds and goes
  back by twice that (0x78AEDA..0x78AF76), as for every target.
- **It opens as the zoom starts and the camera follows it.** Every frame `CreatureRoom::Draw` (0x786C86..0x786D8E),
  while the state is 1 and the copy exists, puts the editor's eye and point looked at into the camera's (+0x49C,
  +0x490), and, unless a network game holds the creature (g_game +0x599C), opens the dialog, `SetC3D(copy)` and
  `UpdateC3D`. So the editor opens in the frame the zoom starts, and the camera zooms onto the editor's view and
  follows it as it turns.
- **It closes and zooms back out.** While the state is 1 a press sets the zoom's target to 0 but keeps the state
  (0x78AA9E), the movement keys clear it (0x78AC2C..0x78ACFD; they are read every frame), and once the dialog is no
  longer shown the state goes back to −1 (0x78AAD5..0x78AAED). With the state not 1, `Draw` (0x786EFE..0x786F42) sets
  the zoom's target to 0, clears the state, `SetC3D(0)` and hides the dialog if it is the active box: closing the
  editor zooms out, and the keys close it as it is. `SetC3D` and the close also call the copy's `MorphTexture`
  (0x786F13). `Temple::GoToRoom` hides every dialog (0x793DD7). Cancel (`ControlCallback` 0x543AB6) copies back the
  tattoos kept as the editor opened (`SetC3D` 0x5432FA keeps two copies of creature +0x1170); OK closes, or in a
  network game after a change shows a message box first.

#### The copy's turns and looks

`CreatureRoom::Draw` 0x786C86..0x786F75, after the room and its copy are drawn and the smokes and belts moved on. With
the camera's state 1 and the copy:

- **The editor's first frame** with it (its +0x320C still null, so once an opening; 0x786CB5..): `EndStaticAction`
  0x4848C0 (state 3, the loop, goes to 4, the end; state 2, the start, sets +0x5238 to end after it) and a turn is
  wanted ([0xDE0F78] = 1) to `heading_from_direction_vector`(the editor's eye [0xDE0F58] − its point looked at
  [0xDE0F48]), stored as a float at [0xDE0F7C], with no `angle_correct`. Then the camera takes the editor's view, the
  dialog, `SetC3D`, `UpdateC3D` (the places, and the idle actions below) and fn_00542A90 (in the cave only the marker:
  its own drawing of a creature is for the editor's main-menu use, +0x3210, which `SetC3D` clears).
- **The look** (0x786DB9..): `Get3DPointFromScreen` 0x81B370 of the mouse at a depth of 3.0 (0x786DA7: the
  camera-space point ((x − half width) near half width / half width, (half height − y) near half height / half height,
  near) scaled by 3 / near, through the camera's rotation, the eye added); while `SetupThing::MouseCaptured`
  [0xC4CD0C] and [0xC4CD14] are both 0 the start of the press [0xDE0F80] is cleared and the look target +0x48C0
  zeroed (no look); else the press's start is taken from `GetTickCount` the first such frame and, for as long as
  `GetTickCount` − it is below 1500 (0x786E04, unsigned), `SetLookPoint`(the point), then the look zeroed.
  `MouseCaptured` is set each frame a box that wants the mouse is up to whether any of the left, right and middle
  buttons is held in the raw mask (`InterfaceActionProcessFirstGUIFeatures` 0x5D2B27: [0xE85304] & 0x43); [0xC4CD14]
  is written only by `DoCitadelDraw` 0x555502 (the multiplayer citadel loop, ORing the right button in), by
  `InputReset` (0) and by two routines that put back what they read, so in a game on one machine it stays 0.
- **Closed** (the state not 1, or no copy) and the copy there (0x786E2C..): the target is
  `heading_from_direction_vector`(fire [0xC5E454] − the copy's place, room +0x104), stored at [0xDE0F7C]; when
  |target − heading +0x84| is below 0.01 (the double 0x8C7A10, 0.01f widened; a difference that is not a number
  counts as below) `LocalRand(2)` is drawn **every such frame** and `StartStaticAction` 0x4843A0 (0x1D with the coin,
  else 0x26 and `SetLookPoint`(fire)) with it as the flag, which starts only while no body action plays and the
  species has the three animations; else `EndStaticAction` and a turn is wanted. Either way +0x4A94 is zeroed (not
  read further).
- **Last**, every frame with the copy: a turn wanted starts when no body action plays (`IsPerformingBodyAction`
  0x4842D0, state +0x4994 ≠ 0), `StartTurningAction` 0x481160 with [0xDE0F7C], and is no longer wanted. The three
  globals [0xDE0F78], [0xDE0F7C], [0xDE0F80] are written only here (xref) and kept for the whole game.
- **The turn**: `StartTurningAction` (not while a body action plays) is `InitialiseTurning` 0x48B790 and
  `StateSet(0x23)` (+0x47D0 = 0, 0x4850E3). `InitialiseTurning` keeps the target at +0x4840, zeroes the flag +0x522C,
  and takes the angle `angle_correct`(target − heading): above 0 the spins 6, 7 and 8 (side 1), else 3, 4 and 5 (side
  2, the angle negated); when the species lacks one of the three no blend is made. Below a quarter turn (1.5707964f, a
  number that is not counts as below) the first two blended by angle × 0.63661975f (0x8CF14C), else the last two by
  (angle − 1.5707964f) × 0.63661975f, every operation a float: `CAnim::CAnim(a, frame, b, frame, w)` 0x85E9D0, whose
  length is `fistp`(float(float(len b − len a) × w) + len a), to the nearest; the blend merges the two spins'
  keyframes into one animation. Each `UpdateTime`: `StateSwitch` (state 0x23, 0x485CBD) ends it first when
  |heading − target| is below 1e-4 (the double 0x8C79D8; not a number counts as below): the heading takes the target,
  `StateSet(0)`, the blend freed; then `StateAction` (0x487495) runs `DoTheTurningBusiness` 0x48B970: with the blend,
  its time +0x47D0 goes on by the animation step +0x48BC and, at its length or beyond, the heading takes the target
  and the time and blend go; without one, the heading becomes `angle_correct`(heading + `angle_correct`(target −
  heading) × time / the stand's length), each a float, and the time goes on, the heading taking the target at the
  stand's length. The step +0x48BC is `ftol`((1.6 − (size × 0.5) × 0.85) × ms) (0x481E45..0x481E66, every constant a
  float, +0x4A90 being 0 for the copy). While turning a body action plays (state 0x23), so nothing else starts. The
  spins' sound events come from `CheckSounds` on animation 4.

#### The places on the body

`SetC3D` (0x543286..0x5432D7) builds, for each of the eight places the species has, a `MeshIntersect` on the
creature's mesh (fn_00866640 with the mesh, the creature's +0x47F0 and the place's data; see the next item). Each frame
`UpdateC3D` (0x542DC7..0x542FED) asks each for `GetWorldPositionAndInwardNormal` 0x867400 under the frame's pose,
counts it while the inward normal and the way from the camera to it are less than 0.2 apart past side on (cosine above
−0.2, 0x8CF130), projects it (`ProjectPoint` 0x8190D0) and keeps the nearest to the mouse, in screen pixels, dropped
beyond 64 pixels (0x1000 squared); the point is kept in dialog units (`unadjust`). The way from the eye (`g_camera`
0xEA1DB8) and the normal are each normalised unless all three of their parts are 0 (0x542E1C..0x542EE6), the normal
stored as floats. `ProjectPoint` returns 0 only when the depth (the third row of `g_world_to_clipping` 0xEA9E40) is
below the near plane [0xE839E0], a NaN included (0x8190F9); otherwise it writes x = ftol((X/z + 1)·[0xE839F0]) and
y = ftol((1 − Y/z)·[0xE839F4]), cut towards zero, and returns 1 when they are off the screen or 2 on it, held there to
`g_MaxScreen` (width − 1, height − 1, so it changes nothing). Only 0 drops the place (0x542F29). `UpdateC3D` comes
after `DrawAdditional` and the smokes (0x786D83).

- **The place's triangle.** fn_00866640 (0x866640..0x866A84) finds the triangle a place is on. The place's skin is
  the mesh's skin list (+0x3C) at (place byte 2 >> 6) & 3. It walks the sub-meshes (+0xC count, +0x10), their
  primitives (+4, +8) and, only for a primitive whose material skin (+8) is that skin, its triangles (+0x18 count,
  +0x1C, three u16 vertex indices each, counted in the primitive), in order. For each: du1, dv1 = uv1 − uv0 and du2,
  dv2 = uv2 − uv0, stored as floats (vertex stride 0x20, uv at +0xC/+0x10); det = du2·dv1 − dv2·du1, and the triangle
  is passed over when |det| <= 1e-4 (the double at 0x8C79D8, 1e-4f widened); t = (u/256 − u0, v/256 − v0), a =
  (tv·du2 − tu·dv2)/det (+0x20) and b = (tu·dv1 − tv·du1)/det (+0x24), worked out on the x87 and stored as floats, det
  divided by as the float stored. The **first** triangle with 0 <= a <= 1, 0 <= b <= 1 and a + b <= 1 wins (all
  inclusive); it keeps the primitive (+0), the sub-mesh (+4), the three vertex indices (+8..+0x10) and each vertex's
  vertex group (+0x14..+0x1C, counted in the primitive, from the groups' vertex counts +0x20/+0x24; 0 for a mesh
  without bones, mesh flag +4 & 0x100). It returns false when no triangle holds the point. The positions it
  transforms on the way (into 0xF963C8) are not kept: the triangle found does not depend on the pose. The kept vertex
  group's bone is the one `GetWorldPositionAndInwardNormal` moves the vertex by (groups +0x24, bone at +2).

#### Dropping a symbol

The symbols are `SetupPicture`s. `SetupPicture::MouseUp` 0x40F840, with its bool argument set, a draggable picture
(+0x280) being dragged (+0x281) calls the box's callback with message 11 (0x40F887), then clears the drag.
`SetupBox::DrawAll` gives the bool 1 when the left button goes up (`PrevLeftButton` set, the button now up:
0x408D24..0x408D3F, to the focus control +0x70) and 0 only when the control under the mouse changes while it is held
(0x408E93), which leaves the drag on: every left release of a drag reaches the callback, wherever it lands.
`ControlCallback`'s case 11 (0x543AE3) does nothing while the room has no copy of the creature (+0x320C); otherwise it
sets +0x30A8 to −1 and, with a place under the mouse (+0x3214 ≥ 0), puts the symbol on there (the same design on the
same place only takes the new colour) and sets the changed flag (+0x10 = 1, 0x543C24, also when no slot is free). With
no place it skips both (to 0x543C28). Then, every time, it copies the eight slots into the player's creature +0x1170
(0x543C60), calls fn_0047D980 and `PlayerSymbol::RebuildLocalTattoo` fn_005DEA60 (0x5DEA60), and the vfunc +0x10 of
the copy and of the creature's body: the skin is written again on every drop.

#### Left alone

The copy plays one of three idle actions (0x41, 0x3C, concurrent 0xCA) every 20 to 40 seconds (0x4E20 +
`LocalRand(0x4E20)` ms), up to ten times, then static action 0x26; a click ends any action and starts the wait again
(0x542C69..0x542DA5). `UpdateC3D` does it while the editor has the copy (+0x320C), so each frame of the open branch,
after that frame's first-frame `EndStaticAction` and before the look.

- The wait [0xCD06E4] goes down by `g_delta_time` [0xC38134] (the frame's ms, not held to 200); at 0 or below the count
  [0xCD06E0] goes up and, below 10, `LocalRand(3)` picks 0: `StartIndividualAction(0x41)`, 1:
  `StartIndividualAction(0x3C)`, 2: `StartConcurrentAction(0xCA)`; from 10 on, while no body action plays,
  `StartStaticAction(0x26)` (the one-argument form 0x484310); then the wait is `LocalRand(0x4E20)` + 0x4E20.
- The two globals are also written by `TattooEditor::InitControls` 0x5433E0 (vt+0x20), which the dialog's callback
  runs on message 6, the box's activation (0x5439F3, from `DialogBoxBase::Show` 0x5135F0 when the box is not the
  active one yet, which `CreatureRoom::Draw` calls at 0x786D68 every frame of state 1, after the first-frame
  `EndStaticAction` and before `SetC3D`): the wait `LocalRand(0x4E20)` + 0x4E20 and the count 0 (0x5433FD..0x54340C),
  and the changed flag +0x10 cleared. So each opening draws the wait first, and no action comes before it has run out.
- The one-argument `StartIndividualAction` 0x484B00 and `StartStaticAction` 0x484310 draw their flag `LocalRand(2)`
  first (the copy's +0x48D0 and +0x4834 are 0: `Init` 0x47FD29 and the copy constructor 0x47F6B9; with +0x4834 set
  they would draw `GameRand`), the static one's flag kept only while +0x4A90 is 0 (it is for the copy), then the
  two-argument forms: `StartIndividualAction` 0x484B80 refuses while a body action plays or without the animation,
  else state 0xA plays it (+0x49A0); `StartConcurrentAction` refuses while one plays or without the animation, and
  draws nothing.
- Then, while the raw left button is held ([0xE85304] & 1, 0x542D66), every frame: the wait is drawn again, the count
  goes to 0, and while a body action plays `EndAnyActions(1, 1)` 0x48F710 (`EndDanceAction`, `EndFighting`,
  `EndStaticAction`, `StopPointing`, `StopMoving`: of the copy's states only the rest's start and loop end). 0x41 is
  animation 65 (the show-off, `k_Impress`), 0x3C 60 (the scratch), 0xCA 202 (the yawn).

### Sources

- The functions named above read in the exe with the xref index; the fade's arithmetic run with the oracle; the
  species' `Data/CTR/*.cbn` read with openblack's `morph::MorphFile`.

## openblack

**Done**: the copy's place, rest, draws, pose, draw and colour, the editor's view, zoom, opening and closing, the
copy's turns, looks and idle actions, the places on the posed body, and the drop; the mesh pick of the creature
(`cave_copy_pick`) and `SET_CREATURE_IN_TEMPLE` are in from raffclar's port, unverified against v1.42 (see
[Pending](#pending)). **Not ported**: the copy's hair, the snore. The depth fade on the belts and medals is drawn with
the rest of the temple's ([the depth fog](../interface/temple-interior.md#the-depth-fog-and-the-temples-light)). The port
decisions taken from raffclar's code are in [Port decisions: the creature](../project/port-decisions-creature.md).

### The copy in openblack

- **Made.** `ecs::archetypes::CreatureCopyArchetype` makes the copy (`components::CreatureCopy`: the creature it
  copies, its species, size, heading, the coin as `sleeping` and the second toss as `restFlag`; the creature's
  `CreatureMorph`, `CreatureAnimation`, `CreatureEyes` and a `Transform` at the place, `AngleY(heading)`, the
  creature's scale; no `Creature` component, and no `Mesh` until it is drawn) as the creature's room is drawn while
  there is no copy, just before the room's effects start, and destroys it with the rooms on leaving. The draws are
  `creature_tattoo_editor::DrawCopyRest` (the coin, a moment's sparkle draws by `Creature::alignment` and the
  skeleton's bone count, the flag, a second moment; the sparkles' values are unused, as they are not shown yet), the
  heading `CopyHeading` (`affine::GetYAngle`, `affine::WrapAngle`); tests
  `TattooEditor.TheCopysRestTossesTheCoinThenTheRestsFlag`, `TheCopysMomentsDrawTheirSparklesAroundTheRestsFlag`,
  `TheCopyFacesTheFireAsTheOriginal` with the exe's bits, and `CreatureCopyArchetypeTest`. [0xC64204] has no openblack
  counterpart (taken as clear). The effects' smoke takes its numbers from its own generator, not the local stream
  (Pending); since they draw nothing on the local stream, the copy's draws are the room's first, as in the original.
- **Made once a visit**, as the original: the room starts the first time it is drawn in a visit
  (`TempleInterior::_creatureCaveStarted`, cleared by `Deactivate`; the original's `PreDraw` runs `InitEngine` while the
  room's engine is not up, 0x799F90, and `CloseEngine` clears it only when the player leaves the temple, fn_00793D00),
  and only then is the copy made, when the player has a creature with a body and `CreatureInTemple` [0xD96270] is set
  (read at 0x787B61, after the creature tests at 0x787B4E..0x787B5B); without a copy then there is none for the visit
  (`creature_tattoo_editor::MakesCopy`, test
  `TattooEditor.TheCopyIsMadeOnlyAsTheRoomStartsAndOnlyWhenTheCreatureIsShown`). `Game::PrepareNewLand`, where every
  land load starts (the debug menu's, `LOAD_MAP`'s, the testbed's), first takes the player out of the temple
  (`TempleInterior::Deactivate`) when they are inside, so the copy goes with the rooms before the registry is reset.
  `CreatureInTemple` is `CreatureCaveSystem::IsCreatureShown`, 1 from the start, set by `SET_CREATURE_IN_TEMPLE`
  (raffclar #349, unverified against v1.42), read as the room starts. The copy's `Transform` turned by `AngleY(heading)`
  takes openblack's creature convention (the body's yaw is minus openblack's heading, inferred, see the leash's
  Pending).
- **Posed.** `CreatureAnimationSystem::UpdateCopies`, called by the temple's creature-room update each frame the room
  is drawn, with `creature_tattoo_editor::CopyFrameMs(game_clock::FrameRealMs())`. The first time it runs a
  millisecond, starts the rest (`creature_layers::PlaySequence` of `CopyRestAnimations`: 28, 29, 30 or 37, 38, 39),
  looks at the fire when sitting (`CreatureAnimation::lookAt` and `CreatureEyes::lookAt`; none asleep) and runs
  another millisecond, with no draws (the room's start drew them, `DrawCopyRest`). Each frame then: `CopyStartsYawn`
  (the draw of 1000 while sitting; the yawn is the gesture layer's 202 when the species has it), and a moment of the
  frame's ms: `DrawCopyMoment` (the sparkles' draws), the blink (`creature_eyes::AdvanceBlink` on the local stream,
  the step taken as the ms), the breath and `AdvanceLayers` at the copy's size; then `SampleBody` into its bones (no
  turn poses) and its eyes placed on that pose. `CreatureCopyArchetype` makes the copy's play state afresh (the body
  standing, no face, gesture, wobble, blend slots or look, the head straight, full speed, the blink from its start).
  openblack's smokes take their numbers from their own generator (Pending), and the copy is posed before they move
  on, as in the original.
- **Drawn.** `RenderingSystemTemple` gives the copy a row in its creature's mesh while the room is drawn, in
  `RenderContext::entityInstances`, with `creature_tattoo_editor::CopyColour` (the temple's light byte by byte, then
  `temple_fade::Apply` at `temple_fade::Depth`, both beside `TempleLight` in 3D/TempleLight.h, with the frame's
  `TempleLight::fade`) as its set colour and 0 as its specular. The temple's `CitadelColour` is the colour openblack's
  `TempleLight::multiply` holds (its alpha 0xFF; `Temple::Draw` 0x7943ED..0x794531), and `TempleLight::Colour` gives
  the same red, green and blue as `ApplyCitadelColoring` (each `(c l) >> 8`). `TempleInterior` sets that fade each
  frame before the rooms are drawn, `temple_fade::Of`: the creature's room's 40 / 150 once that room has been drawn in
  the game (it is never unset, as the original's room keeps its fields), a room's 60 / 200 before; openblack's temple
  always has a room, so the 30 / 80 of no room is only in the function and its test. `creature_draw::Bodies` finds it
  by `components::CreatureCopy` (its species, its creature's painted skins), and `Renderer::DrawTemplePass` draws it
  in the room's own pass only, after the rooms' translucent meshes and before the belts and medals, with the creature
  body's program, its pose, then its eyes, under `model_light::ScopedLight(fire)`; the temple's parts loop leaves that
  row out.
- **Tests**: `TattooEditor.TheCopyMovesOnByTheFramesRealTimeHeldTo200`, `TheCopysRestIsTheSleepOrTheSitAroundItsLoop`,
  `TheCopyYawnsOneDrawnFrameInAThousandOnlyWhileSittingAndNotInTheReflection`,
  `EachMomentOfTheCopyDrawsItsSparklesAsTheRoomsStartDoes`, `TempleFade.*` (the oracle's shares, the first frame and
  no room, the depth's row), `TheCopysColourIsItsGreyInTheTemplesLightThenFaded`,
  `CreatureDrawBodies.TheCreatureRoomsCopyIsDrawnAsItsCreaturesSpeciesWithItsCreaturesSkins`. Under the checks' fixed
  step (`OPENBLACK_FIXED_FRAME_MS`) `TimeSystem::TickCount` is the fixed ticks, advanced by the step each frame before
  `UpdateRealClock`, so `FrameRealMs` is the step and the copy's draws repeat run to run; the hand inside the citadel
  takes the same `FrameRealMs`. (🔍 inferred) that the step +0x48B4 of the copy's `UpdateTime(ms)` is the ms (the
  room's start's one millisecond leaves the timer at 999), and that the room the player is in stands for `Temple`
  +0x1C.

### The editor in openblack

- **The place and the view.** `creature_tattoo_editor::CavePlace` and `CaveView` (`Creature/TattooEditor.h`, tests
  `TattooEditor.TheCreatureStandsAlongTheCavesPlacesByItsSize`, `TheCaveLooksAtItsCreatureFromBesideAndAbove`); the
  zoom, the follow and the closing in `TempleCameraModel` (`OpenTattooEditor`, `UpdateCaveTargets`). The place, the
  coin and the view are made the first time the editor opens in a visit, not as the room is first drawn, since the
  room has no creature to draw yet (Pending). A new land closes the editor as it is, without putting anything back,
  before the registry is cleared, and the editor writes a skin only while its creature is still in the registry (test
  `TattooEditorSystem.ACreatureGoneTakesNoTattoos`).
- **The lowest tip** is `creature_tattoo_editor::Turn`'s `k_LowestTip` (raffclar's tree had 0.25), test
  `TattooEditor.TheViewStaysBetweenAnEighthAndAsHighAsItIsFar` (1.25 at a distance of 10).
- **Closing.** `TempleCameraModel::ZoomedToAfterPress` keeps the creature zoomed to after a press (test
  `TempleCamera.APressLeavesTheCreatureZoomedTo`); since the movement keys are read every frame, openblack's dialog
  takes only Enter and Escape of the keys and lets the others through.
- **The turns and looks.** `creature_tattoo_editor::CopyStep` with the room's `CopyLife` (the turn wanted and its way,
  the press's start, whether the editor had the copy last frame), kept by `TempleInterior` for the whole game, is run
  by the temple's creature-room update each frame the room is drawn, after the places are hovered: the editor's eye
  and point looked at are `TattooEditorSystem::GetView`'s, the button held is `GameActionInterface::IsMouseButtonHeld`
  of any of the three (the window's own state, so also while the dialog takes the mouse's events; none in the test
  runs), the point 3 units in is `screen_point::PixelPoint` with the lens's near clip, the ticks
  `game_clock::TickCount`. `EndStaticAction` is `creature_layers::EndLoop`, `StartStaticAction` `PlaySequence` of
  `CopyRestAnimations` (refused while the body plays or turns, or without the animations), `IsPerformingBodyAction` a
  body that plays or a turn asked for or made, and the look `CreatureAnimation::lookAt` with the eyes' `lookAt`. A
  turn asked for (`CreatureCopy::turnRequested`) starts as `CreatureAnimationSystem::UpdateCopies` next moves the copy
  on (nothing reads the copy in between), as `StartCopyTurn` by the lengths of the species' animations as the body is
  drawn, and each moment of its time runs `StepCopyTurn` by `CopyAnimationStep`; while it turns with a blend its pose
  is the two spins in `CreatureAnimation::slots` at the turn's time, weighted 1 − w and w, and its `Transform` turns by
  its heading. Tests: `TattooEditor.TheCopysTurnPlaysTheSpinsOfItsSideBlendedByTheAngle`,
  `TheCopyKeepsItsHeadingUntilItsSpinsEndThenFacesTheTurnsWay`, `ATurnWithinATenThousandthIsOverAtItsFirstMoment`,
  `WithoutSpinsTheCopysHeadingComesRoundOverItsStandsLength`, `TheCopysAnimationsMoveOnMoreSlowlyTheBiggerItIs`,
  `TheEditorsFirstFrameGetsTheCopyUpToTurnToTheEditorsEye`,
  `WithTheEditorOpenTheCopyLooksAtThePointerForTheFirstSecondAndAHalfOfAPress`,
  `ClosedAndFacingTheFireTheCopyRestsAgainDrawingItsFlagEveryFrame`, `AsleepTheCopyRestsAgainWithoutLookingIntoTheFire`,
  `ClosedAndNotFacingTheFireTheCopyGetsUpAndTurnsToIt`, `ATurnToTheEyeNotStartedWhenTheEditorClosesGoesToTheFire`.
- **Not shared with the land turn.** `creature_locomotion::PairFor` / `PairDuration` (the land creatures' turn) divide
  by float(pi / 2) where the original multiplies by 0.63661975f (0x8CF14C; e.g. angle 0x39561F77 gives 0.00013000000
  against 0.000129999986), hold the angle to pi and the weight to 0..1, send a turn that is not a number to the second
  pair and one of exactly 0 to the other side, and keep the blended length unrounded where the original's `fistp`
  rounds it; nor does locomotion end a turn by the 1e-4 heading test (Pending for the land turn).
- **Stand-ins**: the blended pose (the original's `CAnim` blend is not read past its length; openblack samples both
  spins at the turn's time and sums them by weight, `skeletal_animation::WeightedSum`); the spins' sound events (not
  played for the copy); the look target zeroed is openblack's no look (as `GetLookDirection` reads an all-0 target).
  (🔍 inferred) openblack's end of a loop on its next moment stands for `StateSet(4)` at once, as for every body.
- **The places.** `creature::PosedTattooPoints` (each place's triangle by `PosedTriangle`, its point and normal by
  `creature_eyes::PointOnTriangle`, test `CreatureTattooPoints.APlaceIsPosedAsTheBodyIsDrawn`), then
  `creature_tattoo_editor::ProjectToPixels` and `SitesOnScreen` give the list `TattooEditorSystem::Hover` takes (tests
  `TattooEditor.APointIsProjectedToPixelsFromTheTopLeft`,
  `APointIsProjectedOffTheScreenButNotFromNearerThanTheNearPlane`, `APlaceOnScreenFacesTheCameraByItsInwardNormal`,
  `OnlyAPlaceNearerThanTheNearPlaneIsLeftOut`, `APlaceTurnedALittlePastSideOnStillCounts`). The temple's creature-room
  update calls them each frame the room is drawn with the editor open, after the copy is posed and its effects move
  on: the copy's `CreatureRig::tattooPoints` posed by its `CreatureAnimation::boneMatrices` at `ecs::DrawnBodyModel`,
  seen from `Camera::GetEye` through `GetViewProjectionMatrix` with the lens's near clip (`GetDrawnNearClip`) and the
  window's size, and the mouse at `GameActionInterface::GetMousePosition`; no places without a copy (tests
  `TattooEditorSystem.ThePlaceOnTheCopyUnderThePointerIsFoundOnItsPose`, `WithoutACopyThePointerIsOverNoPlace`). So a
  dropped symbol lands on the place under the pointer. (🔍 inferred) openblack's view-projection stands for
  `g_world_to_clipping`: the projection is openblack's, not bit for bit the original's, so a pixel on a boundary may
  differ.
- **The triangle.** `creature_tattoo_editor::FindSiteTriangle` (`Creature/TattooEditor.h`), on the primitives given in
  the mesh's order, each vertex with its bone; tests `TattooEditor.APlaceIsFoundWhereItsTriangleHoldsItOnTheSkin`,
  `APlaceOnATrianglesEdgeOrCornerIsOnIt`, `ATriangleFlatOnTheSkinHoldsNoPlace`, `APlaceIsFoundOnlyOnItsOwnSkin`,
  `TheFirstTriangleHoldingAPlaceIsTaken`, `APlaceNoTriangleHoldsIsNotFound`. openblack keeps the bone itself. Since the
  search does not depend on the pose, openblack makes it once, as the species loads, where `SetC3D` makes it each time
  the editor opens: `CreatureRig::tattooPoints`, by `creature::FindTattooPoints` on the base mesh's primitives (every
  submesh's in turn), with the other meshes' vertices at the same indices as for the eyes (tests
  `CreatureTattooPoints.EachUsedPlaceKeepsItsTriangleInEveryMesh`, `AMeshWithoutTheVertexTakesTheBases`).
- **The drop.** openblack's dialog releases on the left button alone, as the original. `DraggedSymbol` calls its drop
  on every release of a drag, `TattooEditorSystem::Drop` writes the skin whenever the editor is open, and
  `creature_tattoo_editor::Drop` sets `changed` only with a place.
- **Left alone.** The same in `creature_tattoo_editor::CopyStep`, in the room's `CopyLife` (`idleMs`, `idles`), with
  the frame's `game_clock::FrameRealMs` and the left button held (`IsMouseButtonHeld`): the actions are
  `creature_layers::PlayOnce` (65, 60) and the yawn `PlayGesture` (202), each only when the rig has the animation, the
  sit `PlaySequence` 37, 38, 39, `EndAnyActions` openblack's `EndLoop`, and the activation's draw made on the editor's
  first frame with the copy. Tests `TattooEditor.EachOpeningOfTheEditorStartsTheIdleWaitAgain`,
  `WhenTheWaitRunsOutTheCopyShowsOffScratchesOrYawns`, `FromTheTenthIdleActionTheCopySitsDownWhenItIsFree`,
  `TheLeftButtonHeldStartsTheIdleWaitAgainAndEndsTheCopysRest`. (🔍 inferred) what the actions' flag +0x522C changes
  is not read (it is dropped, as for the rest); `StopMoving`'s write of +0x4990 is not ported (the copy never moves).
  Not ported: a second activation within one opening (the network game's warning box shown over the editor and gone
  again would activate the editor's box anew and draw the wait again; openblack's warning is part of its dialog).

### Test hooks

None of its own; the temple's debug readouts cover the cave.

## Pending

- **The tattoo editor in the cave** ([The tattoo editor in the Creature Cave](#the-tattoo-editor-in-the-creature-cave)):
  the copy of the creature is made, posed and drawn, resting, with its yawn while it sits, and the editor's view is
  taken from it. The pick that finds the creature under the mouse (`DrawAdditional`'s flag [0xDE0F8C], [How a click
  finds it](#how-a-click-finds-it)) is `cave_copy_pick` (raffclar #349, refitted onto our copy, unverified against
  v1.42): asked by the room's camera each frame, from the copy as last posed, rather than during the draw; the body's
  sphere from its mesh's box, then the triangles of each sub-mesh of the first level of detail and stage, posed by
  each corner's bone and shaped towards the species' meshes on the CPU. Still to read: fn_00855440 itself against
  openblack's `screen_pick::FirstHit` and 0x868C80 against `screen_pick::CursorOverSphere` (both imported whole, not
  yet checked against the original), the value of [0xC37D94] and the body's LOD there (openblack takes the first
  level and stage 0), whether the seams' blend counts, and what the two readers of the point do with it (openblack
  keeps only the flag).
- **Not read for the copy**: who sets +0x5730 (the concurrent action's half step); by which time the copy's own hair
  swings (openblack's copy has no `CreatureHair` yet, so no hair is drawn: not ported); how the eyes close in the
  sleep (openblack's copy's eyes keep the creature's mode); what `SetLookPoint`'s gate ([0xC64210], creature +0x24 &
  0x10) is; what the rest's flag +0x522C changes; what bit 0x10000 of the body's +4 does.
- **Not ported**: the sleep's `SnoreIn` sound event, which the copy's `PrepareForDrawing` plays; the smoke's six
  `LocalFloatRand` each (0x787F9E..0x78802E), which openblack's effects take from their own generator.
- **`SET_CREATURE_IN_TEMPLE`** ([0xD96270], default 1, GScript 0x6FF8D1, saved) is honoured (raffclar #349,
  unverified against v1.42), and like the original's it also sets the guidance flag [0xC221CC] that the creature
  fighting remarks wait on (0x6FF8D6; [audio/voices.md](../audio/voices.md)). It is not saved with the game yet (new
  port row). Without the copy the zoom goes to 44 / 49 and no editor opens (0x786C99). Still open: the body places'
  Hover from the drawn mesh.
- **The editor's arithmetic**: the x87 intermediates in `CavePlace` / `CaveView`, and the whole-millisecond frame steps
  for the editor's turn (0x54354E), are not compared bit for bit.
- **`MorphTexture`** on `SetC3D` and on the close (0x786F13), and `PlayerSymbol::RebuildLocalTattoo` fn_005DEA60,
  partly read: `LH3DCreature::MorphTexture` 0x48D540 (vt+0x10 of LH3DCreature, CreatureFalling and CreatureCitadel)
  runs `Morphable::MorphTexture`, then `GTattoo::Draw` with the player's leader interface status and the blood and
  bruise marks (`TextureRef::DrawCutAroundIntersectionPoint`, `ColourIntersectionPoint`, `BloodColours`,
  `BloodColourStrength`, `BruiseHealTimes`); `RebuildLocalTattoo` 0x5DEA60 (8 callers) writes the profile's symbol
  through a temp file and rebuilds the texture (fn_00838AF0 / fn_00838EB0, `YouLostYourVRAM`), multiplayer-aware.
  Their order and numbers are still to read.
- Whether a `LOAD_MAP` script runs while the citadel's pause holds the game is not read.
- [?] whether a press reaches the room's camera while the dialog has the mouse (in the original it would zoom out with
  the editor still open; openblack's dialog takes every mouse event).
