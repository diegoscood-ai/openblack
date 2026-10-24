# The hand: animation and placement

## Summary

The god's hand is a boned mesh animated from `hh.HBN`: a pose for each thing it does (wiggling, ready to pick up,
stroking, gripping the land, holding), with lean layers that follow the mouse's lag. Each drawn frame the original
works out the hand's state (hidden, normal, holding, camera, creature, citadel...), blends 0.13 s into a new state's
pose, and places the hand on the mouse ray: one hand length short of the land, or on the object under the cursor by a
ray test of that object's mesh, with its distance eased by a zoomer. The hand turns to face away from the camera and
leans its up to the land or the surface it feels.

openblack draws the original's hand mesh with its clips, the navigation poses, the two swapped pose buffers and the
0.13 s blend, the original's frame order, the mouse ray and the hand's own mesh test. Left: the mirrored left hand,
the CAMERA state's own blends, the distance zoomer's restart on entering the normal state, and the hand entity's
start position. What is under the cursor is on [Picking](picking.md#object-under-the-cursor); the states and the
interface on [Hand and interface states](states.md#interface-action-states); grabbing, holding and throwing on
[Grabbing, holding and throwing](grab-hold-and-throw.md#holding-spring-and-throwing).

## The original

### Animation and mesh

- Animations `Data\CTR\hh.HBN` (Lionhead pack, morph block at 0x2C). Specification `Data\hndspec5.txt` (69 nodes, 26
  present). C nodes = central poses; L*_lr / L*_fb = directional layers −1..+1.
- Mesh `Data\CreatureMesh\Hand_Boned_Base2.l3d`: 22 bones (0 palm, 8–19 fingers, tips 10/13/16/19, index = 19, 20/21
  thumb). The model origin **is the grip point** in the holding states. Its material is `AlphaTextured`, with an alpha
  gradient in the bottom rows of its skin: the wrist fades out (once drawn with a hard white edge;
  [Textures](../rendering/textures.md)).
- Scale: `3.2 * handScale / 555.294`; `CHand::SetDistanceFromView` (0x46C0D0): d<10 → (d/10)^0.8; d>150 →
  ×(d/150)·(1 − 0.3·(d−150)/1650) (the 2, 10 and 150 at 0x8CBEA4, 0x8CBEA0, 0x8CBEA8). The power is `__CIpow`
  0x7C7AE0 at 0x46C140 of d / 10 (`fdiv`) and the double 0.800000011920929 (0x8CBF28, the float 0.8), stored as a
  float. Beyond 150 the order is (d − 150) / (1800 − 150) × 0.3 (0x8AB23C) taken from 1 (`fsubr`), times d / 150,
  times the scale (0x46C168..0x46C19E), each step at float precision.
- 555.294 in the scale `3.2 * handScale / 555.294` is the bind joint Y extent of Hand_Boned_Base2, so the hand is 3.2
  units long; fn_0046C040 (`GetBaseHandSize`) returns that 3.2.
- `CHand::SetDistanceFromView` clamps the distance to [2 ([0x8CBEA4]), CHand +0x4838] (0x46C0E4) before the scale.
- One CHand per player (`CHand::CHand` 0x46BA80); +0x484C = 1 after LoadBinary's `ToggleLeftRight` (0x46D56F), so the
  hand is mirrored; the left-handed mode is `ToggleLeftRight` 0x46C260 on the same hand.
- CHand +0x4840 is the texture set: 1 in the ctor (0x46BBC9) and in `OnClearMap` (0x46E91C).

#### The hand's clip

`HandStateNormal`'s clip is ObtainRequiredHandPosition's tmpInt (0x5B71D0..0x5B736D, in hndspec5.txt's order):

- 0 Cwiggle at the entry (0x5B608E).
- With an object under the cursor (+0x3C8; none if it is a LandscapeVortex, 0x5B6096..0x5B60AF), not the held one
  (CHand +0x4904, 0x5B60B1) and with a G3D (+0x40, 0x5B60F7): 0xC Ccan_pickup when the interface hand state +0x3AC is
  9, else 0x18 Cstroke (0x5B60FE..0x5B6129).
- A SpellIcon under the hand sets 0xC on its own, later (0x5B627C).
- 0xF Chold_fingers with the hover collide's leash (+0x3D8 = the hover collide's +0x10; the action collide's is
  +0x410), that is the hand over the leash **rope**, not a leash held in the hand, or action state 0x15 (the leash
  pull); it needs a pick of the rope. CHand +0x4874's 0x2D (0x5B69BB..0x5B6AA3): the camera's edge turn sets +0x4874
  with its ring cursor (0x45D259) and PrepareForDrawing clears it after the state's update (0x46C92C). The hover's
  Crotate / Cpitch: [The navigation poses](#the-navigation-poses).
- The L lean poses (0x5B72AD..0x5B742A): the _lr pose at ftol((lag + 80) / 160 × its duration), lag = clamp(+0x48B8 −
  +0x485C, −80, 80) (smoothed mouse x minus mouse x; the ±80 read from 0x900B88 / 0x900B8C, after the normal state's
  vtable), only when |lag| > 0.0001 [0x8C79D8].
- The _fb pose the same with clamp(+0x4860 − +0x48BC, −80, 80) (mouse y minus smoothed y), only when |+0x48BC| (the
  smoothed y itself, 0x5B73BF) > 0.0001; each pose is the difference from its middle frame ((count − 1) >> 1,
  fn_00861EE0).
- In HandStateCamera (Update 0x5B09B4 / 0x5B0A80) the sideways lag is the other way round (+0x485C − +0x48B8).
- In HandStateHolding the same lean is clamped by the ±80 at 0x900B20 / 0x900B24.

#### The navigation poses

- **The camera state** (`HandStateCamera::Update` 0x5B03E0; Enter 0x5B02F0 zeroes the pose +0x14, the grip flag +0x10
  and the clock +0x8, and sets the distance Zoomer +0xA8 to +0x4854):
  - First `SetDistanceFromView(|+0x78 − g_camera|)`, the sum (x² + z²) + y² (0x5B0478..0x5B04B5).
  - While +0x10 is 0 (not gripping yet): the Zoomer's destination is that distance, 0 speed, 0.4 s, stepped inline
    (the value summed ((((C c4) + b c3) + t s0) + a c2) + v0, 0x5B0511..0x5B0594); the distance is it or the land's
    (fn_0046DF60(0)), the smaller (a land distance not below the Zoomer's keeps the Zoomer, 0x5B0596..0x5B05B3);
    `SetDistanceFromView` takes it and fn_0074CAF0 puts +0x78 along the cursor's line (0x5B05BB..0x5B05CD).
  - The pose (0x5B05D2..0x5B0741): 0x2A Cgrip when the mode's HandStatus +0x1AC is 3 (the pan) or the clear view
    [0xC5B100] is above 0.01; else from the mode's hints +0x90: & 1 → 0x2D Crotate with EnabledFeatures & 2, else the
    wiggle; & 8 → 0x33 Czoom with & 4, else the wiggle; & 2 → 0x30 Cpitch with & 1; else the wiggle (0).
  - Crotate's up: the way from +0x78 to the camera's focus (g_camera +0xC), its y raised by 0.3 [0x8AB23C] of its
    length (√((y² + x²) + z²)), normalised by the same sum with the raised y unless x, the raised y and z all compare
    equal to 0 (0x5B0622..0x5B06E8); its lift is fn_0046C040 (3.2) × +0x4834 × 0.33 [0x8CA268] (0x5B06FE..0x5B070D),
    added to y in the matrix only (0x5B093A..0x5B0944), not to +0x78. Every other pose takes the last normal up
    g_D13F60.
  - A change of pose (0x5B074B..0x5B07BC): to Cgrip, +0x10 = 1 and, with no object, bubble or leash in the interface
    (+0x3C8 / +0x3D0 / +0x3D8), `StartStateBlend` and +0x78 = g_D13F30 (the land under the cursor); from Cgrip,
    +0x10 = 0; then +0x14 = the pose and `StartStateBlend` again.
  - The clock: (+0x8 + g_delta_time) mod the pose's length (0x5B07C1..0x5B07E0), so it runs on through pose changes;
    the lean layers are the pose's own (GetAnim(pose + 1) / (pose + 2), 0x5B09AC / 0x5B0A70).
  - The up Zoomers (+0x18 / +0x48 / +0x78) are snapped to the up unless +0xC is set (0.8 s eases,
    0x5B07E5..0x5B0826); no code here sets +0xC, so they snap, and +0xC is zeroed (0x5B086D). The up is normalised
    ((z² + y²) + x², 0x5B08B2..0x5B0927) and fn_0046E160 builds the matrix from it and the lifted place.
- **The hover** (ObtainRequiredHandPosition with its hints gate, the normal state's call pushing 1 at 0x5B71A7): with
  no object under the cursor that has a 3D object (+0x3C8; a LandscapeVortex or the held object count as none; one
  without a G3D goes on here too), no screen object (+0x3D0) and no leash (+0x3D8 or action state 0x15), the hints are
  the CameraModeNew3's +0x90, ORed during a playback with the recorded flags [0xC5B0F4] (2 → 1, 1 → 2, 4 → 8, and
  +0x4858 = 1, 0x5B68B2..0x5B68CB). & 1 with feature 2: Crotate, the up's target the way from the aim point
  (fn_0074CAF0 at fn_0046DF60's distance, 0x5B600A) to the focus as above (0x5B68E7..0x5B69B9), kept over the land
  normal (0x5B6F80); else & 2 with feature 1: Cpitch; the up is the land normal in both other cases (0x5B6AE2). The
  target is normalised again and taken only when the mouse's x moved (0x5B6FC9..0x5B70A7).
- **Mouse smoothing**: smooth += dt·vel, clamped to between -80 px and 80 px of the mouse; vel = (vel + (mouse −
  smooth)·20·dt)·0.03^dt.
- The mouse is LHMouse's position in the game's pixels ([0xE852C0] / [0xE852C4], ints), copied by PrepareForDrawing
  (0x46C6C5..0x46C6D8) into +0x485C / +0x4860, so the lean depends on the resolution.
- The HOLDING poses come from the jump table 0x5B56A8 (the grip heights from 0x5B568C).
- MAGIC (0x5B50B0, a spell seed until it is ready): GetAnim(0, 0) = Cwiggle held at half its length.
- **The state blend.** The original blends at a change of CHand's state (PrepareForDrawing 0x46C73F..0x46C768: Exit,
  Enter, `CHand::StartStateBlend` fn_0046C500), 0.13 s ([0x8CBE9C], fdiv 0x46C970), linear, and at the state Updates'
  own calls of the blend start below (NORMAL's change of pose among them); the animator itself never blends.
  StartStateBlend makes the current pose the blend source (+0x47F0 ↔ +0x49AC) and sets the timer +0x49B0 = 0.
- **Two pose buffers, swapped.** `StartStateBlend` swaps the two buffer pointers (+0x47F0 drawn ↔ +0x49AC faded from,
  0x46C500..0x46C51C); nothing is copied, so the next pose is written over the older buffer. Two changes with no pose
  written between them fade from the pose kept at the change before, not from the last one drawn. INVISIBLE's Enter
  copies the drawn buffer into +0x49AC before the change's swap (0x46BEA7..0x46BEC5), so the hand shown again fades
  from where it was hidden. The +0x49AC buffer is allocated uninitialised by `CHand::LoadBinary` (0x46D5B9, stored
  0x46D5C3).
- The state's Update is handed the drawn buffer as it was before the Update ran (loaded at 0x46C913, pushed 0x46C91B).
  The other callers of the swap: NORMAL at a change of its pose (0x5B7233, reloading +0x47F0 at 0x5B723B), CITADEL
  (0x5B1119, reload 0x5B1121; its Enter zeroes the pose it tests, 0x5B0CA1, so (🔍 inferred) it never fires),
  CREATURE at its clip changes (0x5B1BF3, 0x5B214F; it writes into +0x47F0 itself, 0x5B2A1F), CAMERA at its changes of
  pose (0x5B07BC, and before it 0x5B0788 for the grip with no object, screen object or leash): CAMERA writes its pose
  into the buffer it was handed (`FillBuffer` at 0x5B099C with [esp+0x80] read at 0x5B0970), which after one swap is
  the one faded from. Also the held object replaced by another (fn_0046DE40 0x46DECB / 0x46DEE7, from
  `CHand::SetHeldObject` fn_0046E590) and the land grip's start (`StartLandscapeGrip` fn_005D1AB0 0x5D1E71, after
  `Morphable::SetPos`).
- NORMAL's change of pose (`HandStateNormal::Update` 0x5B71A0): its pose is the one `HandStateHolding::Update` gives
  (0x5B71CB); its Enter sets the last pose +0xC to −1 (0x5B5DD0), so the first Update only notes it
  (0x5B71F4..0x5B7200); later, another pose than +0xC is noted and swaps (0x5B7229..0x5B7233), the pose then written
  into the reloaded drawn buffer (0x5B723B, `FillBuffer` 0x5B7295). The clip's clock is not reset there: +8 = (+8 +
  g_delta_time) mod the new pose's length (0x5B71E7..0x5B71FB).
- While the blend runs, every float of the world bone matrices +0x47F0 = prev + (cur − prev)·t from +0x49AC
  (0x46C96A..0x46C9A4, the fstp at 0x46C991..0x46C9A1).
- The place blend: g_C5E670 is the hand's place when its state changed (fn_0046C500 0x46C522 = g_C5E680,
  fn_0046E160's last); g_C5E660 = g_C5E670 + (g_C5E680 − g_C5E670)·t (0x46C9A6..0x46CA9A).
- In HOLDING (+0x4878 == 4) the held object's G3D (+0x40 +0x38) is put at g_C5E660 + the hold offset g_D13F20 during
  the blend, so it is drawn from the blended place, except when [0xD47824] is set and +0x4904 == [0xD47828]
  (0x46CA3A..0x46CA4E, not identified).
- The blend timer +0x49B0 advances by dt = `GetCameraTimeInc` 0x555820 × 0.001 (g_delta_time inside the citadel,
  `LH3DTech::g_delta_time`, 0x46C5C9..0x46C608).

#### Hidden hand and CHand's states

`CHand::GetRequiredState` 0x46CD10:

- CITADEL 0xA when g_game +0x205A28 == 1 (0x46CD1C); it comes first, so inside the citadel the hand stays drawn.
- INVISIBLE 0 sets +0x4844 = 1 (0x46CD38..0x46CD5C) for the interface's modes 1 / 2 / 12 / 25; the rest goes through
  the jump table 0x46CF48 / 0x46CF5C. CAMERA 2 when the camera mode is CameraModeNew3 with +0x8C & 1
  (0x46CEFA..0x46CF3B).
- INVISIBLE → `HandStateInvisible::Enter` 0x46BE90 → `CHand::Hide(0)` 0x46C2E0: the hand's LH3DObject +0xAC = 1 (not
  drawn), +0x483C set; `HandStateInvisible::Exit` 0x46BED0 → `Show(0)` 0x46C1B0, the bones kept for the blend back;
  `HandStateInvisible::Update` 0x46BEE0 does nothing.
- `CHand::Hide` / `Show` also switch the system cursor: hidden with no request (+0x4848) and no custom cursor (+0x49A0),
  `LHMouse::SetCursor(0, 1, 0)` and `FrontEnd::SetCursorOff`; shown, `FrontEnd::SetCursorOn` when +0x4844 is 0.
- Only INVISIBLE hides; an inactive interface or a box with the mouse alone only stops PrepareForDrawing (no state
  Update, no pose; step 6 of [the frame order](#the-frame-order)).
- The hidden hand (+0xAC = 1) feeds no tree bend: `GLandscape::Draw` skips the held object's slot 1 of the bend table
  while the hand's LH3DObject is not drawn (0x5E495A..0x5E4962), see [Drawing](../world/trees.md#drawing).
- GetRequiredState reads the turn's +0x3AC, and the Enter's Hide comes before that frame's draw.
- `OperateInteractionCamera` 0x46CB60 (called at 0x46CEA0) and fn_0046CF80 (0x46C738) act only on CREATURE (state 7,
  packets 0x59 / 0x5F). See [The hand on a creature](states.md#the-hand-on-a-creature).

#### The frame order

`CHand::PrepareForDrawing` 0x46C550: `Process3dEngine` 0x54DA80 calls it (virtual +0x18, 0x54DEAA) after
`GInterface::PreDrawProcess` 0x5CE9E0 and before `GLandscape::Draw`. The interface's frame pass
(`ProcessFrameUpdates` 0x5CEDB0 = fn_005D9A20 → `InterfaceActionProcess` fn_005D1120: the press, the box rule, the
interface hand state) runs at the end of the frame, after the draw: in `GGame::Loop` 0x54CF20 the frame calls
`ProcessGraphicsEngine` 0x54D850 at 0x54D3BE (which calls `Process3dEngine` at 0x54D88E), then `ProcessFrameInputs`
0x54C340 at 0x54D3E9, then `FlipScreen` 0x54D800 at 0x54D3F5. So a press is handled after that frame's hand
preparation, and the state it asks for is entered at the next frame's. In order:

1. The system mouse's hide / show (fn_0046C2E0 0x46C5A5, `CHand::Show` 0x46C5BE); `GetCameraTimeInc` 0x46C5C9 into
   +0x4900 and dt (× 0.001, 0x46C5FC); `SetTextureSet` fn_0046BF60 0x46C617; the alignment into +0x9C and
   `Morphable::UpdateMorphing` 0x46C673.
2. +0x47BC (the fixed-position animation): fn_0046D760 and return (0x46C678..0x46C68E).
3. The mouse into +0x485C / +0x4860, the last one into +0x4864 / +0x4868 (0x46C68F..0x46C6D8); the heading +0x84 from
   the camera → mouse ray (0x46C6F0..0x46C725; the angle helper 0x7FAA50, whose PDB name `GUtils::AngleXZ` is wrong).
4. `CHand::GetRequiredState` 0x46CD10 (0x46C730), which sends the release impulse, packet 0x40 (0x46CE88); then
   fn_0046CF80 (0x46C738), which keeps CREATURE 7 while it waits (packets 0x59 / 0x5F).
5. The state change, when the state differs from +0x4878: the old state's Exit (vt +8, 0x46C752), +0x4878 = the new
   one (0x46C755), its Enter (vt +0, 0x46C764), the state blend start fn_0046C500 (0x46C768).
6. Return when hidden (+0x483C, 0x46C76D), INVISIBLE (the new state 0, 0x46C779), a box has the mouse ([0xC4CCEE]
   ≠ 0, 0x46C781) or the interface is inactive (GInterface vt +0x40C returns 0, 0x46C78D..0x46C7A4): each jumps to
   the epilogue 0x46CB56, so nothing of steps 7 and 8 runs. [0xC4CCEE] is what the last frame's interface pass left
   (the pass comes after the draw). Steps 1 to 5 still run under a box: the morph, the camera time, the mouse into
   +0x485C and the heading, the required state with its release impulse (packet 0x40) and the CREATURE wait, and a
   state change with its Exit, Enter and blend start. Skipped: the mouse smoothing, the state's Update (so the hand's
   land point g_C5E640, the distance zoomer and the holding place are not taken), the blend, the copy of the bones
   and the matrix into the hand's 3D object and the custom cursor's draw. The 3D object's not-drawn flag (+0xAC) is
   not touched, so the hand stays drawn, in the pose and at the place of the last frame that reached step 8, unless
   it is INVISIBLE (whose Enter hid it).
7. The mouse smoothing (0x46C7AA..0x46C906), then the state's Update (vt +0xC, 0x46C921): NORMAL, HOLDING and CAMERA
   find the hand's land point there (fn_0046DF60).
8. The 0.13 s blend (0x46C932..0x46CAB8), the hand's 3D object (0x46CAC2..0x46CB3D), fn_0046DAF0 (the custom cursor's
   draw) while +0x49A0 (0x46CB51).

So the land point always sees the state the same frame's change left, and so does the frame's interface pass; the
change itself reads what the last frame's pass and this frame's turns left.

### Where the hand is placed

`ObtainRequiredHandPosition` 0x5B5E70 (a cdecl free function of 9 arguments; the 1.20 symbols swap its name with
`HandStateHolding::Update` 0x5B3C70):

1. Object under the cursor (see [Picking](picking.md#object-under-the-cursor)). If there is none, the terrain.
2. With an object:
   - villagers (Living that are not the creature): distance = |centre − camera| − 2D radius, corrected for height;
   - the rest: the point where the ray intersects the mesh ([The hand's mesh test](#the-hands-mesh-test));
   - if you are carrying something: the search ray aims at the terrain point + 0.6·grip height, and the final point is
     moved towards the camera by half the 2D radius of the held object.
3. The distance to the camera is smoothed with the Zoomer 0xD13F80: **0.1 s** when approaching ([0xBF01D8]),
   **0.28 s** when moving away ([0xBF01D0]); **2.0 s** ([0xBF01D4]) when giving to the creature or while the zoomer's
   speed (0xD13F8C) is above −1 in one camera mode. Never further than the terrain under the cursor.
4. `HandStateNormal` puts the hand exactly there (with no further smoothing).

- The action point GInterface +0x3F0 (zeroed unless an action is in progress, then the collided object's Pos or the
  land under the cursor): see [The action point](picking.md#the-action-point).
- The Zoomer target is at least 1; with a screen object under the hand (+0x3D0) and no leash (+0x3D8 == 0) the target
  is its depth +0x3D4 (0x5B6AA5..0x5B6ABF).
- The empty hand stops 3.2 (fn_0046C040, 0x46E0D3) × handScale (+0x4834) × Size1 (+0x90) short of the land, not over
  the sea (land y < 0.1, [0x8AB22C] at 0x46E02E); every branch is clamped to [2, CHand +0x4838] (0x46E0EA..0x46E147).
  (🔍 inferred) Size1 is 1 for the hand.
- With no land on the line, fn_0046DF60 takes the hand's position +0x78 (the place the last frame left: written after
  the call by every state that makes it) and its distance |camera − +0x78|, never shortened; while HOLDING with the
  spring on (holding +0x104) it takes fn_0074CAF0 instead, the point the reach (+0x4838) along the line from the eye
  through the near point, which counts as found and is shortened.
- With no object under the cursor ORHP's point is fn_0074CAF0(pixel, dist) (0x5B600A..0x5B6015), the view distance
  along the line from the eye through the pixel's near point, so the zoomer's target is that distance, not the land's
  own; [0xD13F70] (a fixed hand position) instead while the one-shot flag [0xD1404C] is set (the creature fight's
  sparkles).
- +0x78..+0x80 is `Morphable::position` (CHand : Morphable : DrawingObject). The one CHand of the process is made by
  `GInterface::SetupHand` (0x5CE985..0x5CE99B) with LHPoint(10, 10, 10), which `Morphable::MorphInit` 0x617310 copies
  into it (0x61737A..0x61738A); `OnClearMap` 0x46E8C0 keeps it across lands, `CHand::Load` 0x46E930 reads it from a
  saved game (0x46EA7E). No frame draws it before a state's Update writes it (the 3D object's matrix is built after
  ORHP, 0x5B725F).
- The distance zoomer 0xD13F80: ORHP's very first call (the static flag 0xBF01CC, 1 in `.data`) zeroes it
  (0x5B5EAA..0x5B5F18). Every `HandStateNormal::Enter` 0x5B5D00 (the normal state's vtable 0x900BA0, slot 0) also sets
  the zoomer to +0x4854, the hand's last `SetDistanceFromView` value (10.0 from the constructor, 0x46BBB7).
- Holding, `HandStateHolding::Update` first calls fn_0046DF60(0) (0x5B3DAE) and `SetDistanceFromView` with it
  (0x5B3DBF), then ORHP with subtractHand = (+0x490C == 2, the MAGIC hold, 0x5B3F57) or +0x49B4 (the creature to give
  the held object to) set; so the subtraction uses this frame's scale.
- **The hand's land under the cursor** is fn_0046DF60's own (0x46DF8B): fn_00800C30 (size 0x160) with shift on and
  depth float(near + 1). It takes `LH3DTech::Get3DPointFromScreen` of the integer pixel at depth 0 (the near point P),
  d = P − camera; a depth ≤ 0 or NaN becomes the near clip; unless d is all zero, each axis × 1 / sqrt((dz² + dy²) +
  dx²), then each × depth; both the camera and P move forward by d (0x800CD3..0x800D67), and `RayCast` fn_00802550 (the
  land or the sea within 7500 of the real `g_camera`) runs along that line, so land nearer the eye than near + 1 along
  the line is not found. y = GetAltitude at ftol((x·65536)·0.1), the hand's quantised point. The game thread's FPU runs
  at 24 bits, so each step is a float operation. The distance from the eye is summed z, y, x (0x46E09F..0x46E0C5); the
  hand size is subtracted when the argument is set and y ≥ 0.1, ordered (a NaN height keeps the full distance,
  0x46E026..0x46E069); then 2 at or below 2 (a NaN too, 0x46E116) and the reach above it (a NaN reach keeps the
  distance, 0x46E123..0x46E132). Callers: ORHP 0x5B5E70 at 0x5B5FC7 (argument 1 from Normal, so `HandStateNormal`
  reaches it through ORHP at 0x5B71CB; from Holding, a MAGIC hold or +0x49B4 set), `HandStateHolding` 0x5B3DAE (0) and
  `HandStateCamera` 0x5B0438 (0); the citadel, play-animation and tug states' Updates too. The objects under the cursor
  come from the interface's collide (+0x3C8 / +0x3D0 / +0x3D8), not from this line.
- **The interface's land under the cursor** (fn_005D5980 → fn_005E5620, from `PreDrawProcess` 0x5CEAA6, the land the
  objects are weighed against and +0x3F0's land point) is another line: `Get3DPointFromScreen` at depth 0 (0x5E562C),
  `RayCast` fn_00802550 from `g_camera` to that near point, not moved on (0x5E5660), else the sea's level wherever the
  line goes down, with no reach (fn_005E55D0, 0x5E5678); y = GetAltitude at ftol((x·65536)·0.1) (0x5E56A3..0x5E56EB);
  then `KeptInReach` fn_00442B00, within 7680 of the map's middle (0x5E5726 / 0x5E572D).
- The gestures take the same function at their own pixels: the gesture sample (fn_005CEAD0 at 0x5CEB0F) and the sizing
  gesture's packet (fn_0057A5E0 at 0x57A710); the recognised gesture's trail takes fn_00800C30 with the shift off
  instead (fn_00689F20 at 0x689F72). Their near plane is the one the last `GCamera::Update` wrote
  ([Gestures](../magic/gestures.md#gestures-the-buffer-and-the-recogniser)).
- Block 6 (0x5B622A..0x5B629C): a SpellIcon under the hand gives its MapCoords and altitude as the point, the depth
  target |camera − point| − 3.2 × the hand size ([esp+0x7C], 0x5B6298; used at 0x5B6B08..0x5B6B59), and the flag
  esp+0x50 (0x5B6251) makes the up the land normal (0x5B6FA1..0x5B6FC4).
- The up target over an object the hand feels (0x5B6808..0x5B6848): 0.25 [0x8AB3D4]·d − n + (0, 0.5, 0), d =
  norm(camera − hit), n the hit face's normal as the hand's mesh test gives it, turned away from the eye (see
  [The hand's mesh test](#the-hands-mesh-test)), with the hand higher than 1.6 hand sizes above the ground.
- `HandShouldFeelWithMeshIntersect` vt 0x59C: Object 0x4025B0 = 1; Tree 0x55D9E0, DeadTree 0x5109C0, TotemStatue
  0x561120, LandscapeVortex 0x5FD600, SpellSeed 0x727F60, MapShield 0x72C010 = 0. An AnimatedStatic takes its mesh from
  fn_00422250 and is not lifted to the surface.
- Over a felt object the land normal is used instead when the hand is lower over the land than half its size (3.2 ×
  +0x4834 × 0.5, 0x5B6DC8..0x5B6DD9 / 0x5B6F8C..0x5B6F9F); the result is normalised (0x5B6FFC..0x5B7038).
- The raised direction while holding (0x5B6384..0x5B643D): the camera is taken from the cursor's ground point first,
  the lift is added to the y after (0x5B63C8), then each axis × 1 / sqrt((y² + x²) + z²) unless all three are 0.

#### The mouse ray

The original's mouse ray is a direction only, from the eye: a user that needs a point starts it at `g_camera`
(0xEA1DB8), as `HandStateTug::Update` does (0x5B8070, the reconstruction's `hand.cpp` 1671:
`hand->position = cam + dir * t`); the hand's heading in `CHand::PrepareForDrawing` takes only the direction.

- fn_0074CBD0 (0x74CBD0, size 0xD0) builds it: `LH3DTech::Get3DPointFromScreen` (0x81B370) with depth 0 gives the
  mouse's point on the near plane in world space (0x74CBF6); the eye is taken away (0x74CBFB..0x74CC24) and the vector
  normalised in float as 1 / sqrt(z² + y² + x²) times each axis (0x74CC58..0x74CC85). It is left as it is only when all
  three axes are exactly 0 (0x74CC2D..0x74CC56), which a point on the near plane never is.
- `Get3DPointFromScreen` (0x81B370, size 0xE0), mouse in integer pixels: x = (mouse.x − W/2) · [0xC3812C] / (W/2),
  y = (H/2 − mouse.y) · [0xC38130] / (H/2), z = the near plane [0xE839E0]; with a depth d ≠ 0 all three are scaled by
  d / near (0x81B3B0..0x81B3BA). The point is R · (x, y, z) + `g_camera`, R the camera's axes at [0xEA1D28..0xEA1D48],
  each row summed z, y, x (0x81B3BE..0x81B43F). Nothing clamps the mouse: the screen's edges are at ±1 (W/2 and H/2
  off the centre), and a point beyond them extends the same way.
- [0xC3812C] = near · tan(fov / 2) and [0xC38130] = [0xC3812C] / aspect, the aspect [0xE839EC] = W / H
  (`UpdateViewPort` 0x819030): written by `ChangeFov` (0x8195B0, 0x819647..0x819663) and again by `GCamera::Update`
  with the new near plane (0x4424BA..0x4424E2). The field of view [0xEA1DD0] is the horizontal one (70°, 0x3F9C61AA,
  passed to `ChangeFov` by `UpdateViewPort` at 0x819045).
- So the direction is normalize(R · (ndcX · tan(fov / 2), ndcY · tan(fov / 2) / aspect, 1)): the near plane only
  scales the point, and the ray does not depend on it beyond rounding.
- Because the hand is `origin + mouseDir × distance`, wherever its distance is clamped (at least 1, [2, reach]) or the
  hand's distance zoomer lags, the start point matters; where the distance follows the surface the point is the same.

#### Holding and grain place

- g_C5E650: the required hand position of the last HOLDING / GRAIN frame (`HandStateHolding::Update` 0x5B3EFC /
  0x5B3FC9): the cursor's ground point g_C5E640 while a pile is locked (0x5B3EFC), else ORHP's output before
  AdjustRequiredPos, GetExtraHeight and the hold height are added (0x5B3FC9).
- g_C5E640: the cursor's ground point, written by fn_0046DF60 (0x46E057; also kept at 0xD13F30), which the state
  Updates call (Holding 0x5B3DAE / 0x5B5FC7, Camera 0x5B0438) and HandStateNormal through ORHP (0x5B71CB). It is the
  hand's land point (the no-land fallback included), written only in frames that reach a state's Update (not hidden).
- The pile lock's GetAltitude (0x5B3F3B) reads +0x78's x and z. Packet 0x40 (GetRequiredState, 0x46CE3D) sends
  g_C5E640 − g_C5E650 before this frame's state change and Update: the throw velocity.
- The frame's last `SetDistanceFromView` in HOLDING / GRAIN is at 0x5B499B, with |camera − CHand +0x78|.
- The tug's grip follows the drawn lean (HandStateTug's objectMatrix +0xEC, 0x5B81C5..0x5B822C).

#### The hand's mesh test

Over an object with a mesh (not a Living other than the creature, not a SpellIcon, not one it lifts to the surface)
`ObtainRequiredHandPosition` places the hand with a ray test of the object's mesh of its own, not with the cursor's
pick: the call at 0x5B6529 to `fn_00865020` (size 0xA00, `ret 0x20`), a method of a `MeshIntersect` (ecx, 0x28 bytes
at esp+0x8C). Its arguments: the mesh, its matrices (the 3D object's at +0x14; a creature's `TransformedMatrices`),
the ray's start (`g_camera`, esp+0x60), its direction (esp+0x54, unit: the mouse direction, or while holding the
raised one, normalised at 0x5B6335..0x5B6367 / 0x5B6384..0x5B643D), a byte choosing the sub-mesh filter (0 here), the
out point (esp+0x3C), the out normal (esp+0x10) and a byte that lets t be 0 or less (0 here).

- **Sub-meshes** (0x86505D..0x865091): with the filter byte 0 only those with flag 0x20000000, bit 0 of `lodMask` (the
  first level of detail); with 1 only those with 0x2000 (`isPhysics`). Nothing else is read: status, window and
  physics sub-meshes are tested when they carry the first level. `LH3DSubMesh::Create` 0x87FA00 first gives a sub-mesh
  with no level and bit 11 clear every level (|= 0xE0000800, 0x87FA06..0x87FA1F). In AllMeshes.g3d no physics sub-mesh
  carries the first level, no sub-mesh takes the load default, the status sub-mesh 0 of every abode, temple and wonder
  carries it (190 sub-meshes), and 86 meshes have drawn sub-meshes of the lower levels only.
- **World space**: each primitive's vertices are moved first (0x8650C8..0x86525E, into a buffer at 0xF963C0): a mesh
  with bones (mesh flag 0x100) takes each vertex group's matrix (matrices + bone × 0x30), any other the one matrix,
  x' = ((z·m[6] + y·m[3]) + x·m[0]) + m[9] (rows of three, the position last). Distances, the epsilon and the normal
  are in world units, with the object's scale.
- **No bounding volume**: every triangle of every primitive is tested (0x865260..0x8659C5).
- **The plane**: n = (V1 − V0) × (V2 − V0), made unit unless it is zero (0x8652B7..0x865405); denom = n · dir; the
  plane is missed when |denom| ≤ 0.005 ([0x9A3BC4], 0x86544C..0x86546E), on either side: **no back-face cull**.
  t = (n · V0 − n · origin) / denom (0x8654A0), missed when t ≤ 0 (0x8654AC..0x8654BD); P = origin + t · dir.
- **Inside**: for each edge V0→V1, V1→V2, V2→V0, whether (edge × (P − its start)) · n > 0 (0x865520..0x865737); the
  hit needs three or none (0x865739..0x865742). With n from the same triangle an inside point gives three; a point on
  an edge gives 0 for that edge and misses.
- **Nearest**: the first hit is kept and a later one replaces it only when its t is strictly smaller
  (0x865748..0x86575B); a tie keeps the first in order (sub-meshes, primitives, triangles).
- **Out**, with both out pointers (0x865761..0x86580B): P (also kept at 0xFA23C0) and the unit n turned along the ray,
  n when denom > 0 else −n (0x8657A1..0x865805): normal · dir > 0, away from the eye whatever the winding. Returns 1 on
  a hit, else 0. Without both pointers (0x86580E..0x8659A8) it fills the `MeshIntersect` instead (the primitive and
  sub-mesh, the three vertices and their bones, two barycentric coordinates in [0, 1] with the 0.005 guards, a hit
  outside them dropped); the hand does not take that branch.
- **After the call** (0x5B652E..0x5B66F6): up = norm(normal) (0x5B6536..0x5B65A5); toCam = camera − P;
  s = −(toCam.z · dir.z + toCam.x · dir.x + toCam.y · dir.y) (0x5B65A9..0x5B65ED). Holding ([esi+0x4904]): the point
  is camera + mouseDir · s on the mouse direction (0x5B65F5..0x5B668F). s < 1: the point is camera + dir and toCam =
  −dir (0x5B6693..0x5B66F6). Otherwise the point stays P.
- In AllMeshes.g3d (V1 − V0) × (V2 − V0) points along the vertex normals for 83% of the drawn triangles (97381 against
  19579), so on a face the eye sees the test's normal is mostly the face's inward one, and the up target 0.25·d − n +
  (0, 0.5, 0) leans out of the surface.

### Addresses

The hand's functions, fields and globals as the decompiled reconstruction of the pickup path lays them out
(`decomp_pickup`, `hand.cpp` and `NOTES_hand.md`; each checked in the disassembly). Where the 1.20 symbols give no
name, the name is the reconstruction's own.

| Address | Function | What it does |
|---|---|---|
| 0x46BA80 | `CHand::CHand` | one hand per player; the tug state is made here (`HandStateTug::HandStateTug`) |
| 0x46BF20 | `CHand::SetMaxDistanceFromUser` | the reach +0x4838, at most 1800 |
| 0x46BF50 | `CHand::GetMaxDistanceFromUser` | returns +0x4838 |
| 0x46BF60 | `CHand::SetTextureSet` (fn_0046BF60) | a new set +0x4840 re-morphs the texture (vt +0x10); called with GInterface +0x48 |
| 0x46BFB0 | `CHand::GetMaxForce` | (+0xAC + 1) × 600000; called by `HandStateTug::Update` (0x5B8282, 0x5B82C1, 0x5B8417) |
| 0x46C040 | `CHand::GetBaseHandSize` | 3.2 |
| 0x46C050 | `CHand::StartFixedPosAnimation` | from `Abode::InterfaceTap` (animation 0x39); fn_0046D760 plays it |
| 0x46C0D0 | `CHand::SetDistanceFromView` | the scale above |
| 0x46C500 | `CHand::StartStateBlend` (fn_0046C500) | the buffer swap and the blend start; called by `CHand::PrepareForDrawing` and the Creature, Normal and camera Updates, and fn_005D1AB0 |
| 0x46C550 | `CHand::PrepareForDrawing` | the frame order above |
| 0x46D080 | `CHand::GetScaleMatrix` (fn_0046D080) | the hand's scale S = 3.2 / +0x8C × +0x4834 × +0x90, which fn_0046D9D0 puts in front of the held object's matrix |
| 0x46DAF0 | fn_0046DAF0 | the custom cursor's draw |
| 0x46DB70 | `CHand::GetViewDistanceTo` | a static distance from the view to an object; called by `HandStateMultiPickUp::Enter` and `HandStateTotem::Enter` |
| 0x46DE40 | `CHand::SwapHeldObject` (fn_0046DE40) | the held object replaced; called by `GInterface::PlaceObjectInMagicHand` (0x5DA87F) and fn_005DC330 (0x5DC457) |
| 0x46DF60 | `CHand::UpdatePointUnderMouse` / `CHand::GetDistanceFromView` (fn_0046DF60) | the hand's land under the cursor above; called by the camera, citadel, normal, play-animation and tug Updates and `HandStateHolding::Update` (0x5B3DAE) |
| 0x46E160 | `CHand::BuildHandMatrix` (fn_0046E160) | the hand's matrix from its place and up ([Where the hand goes](picking.md#where-the-hand-goes)) |
| 0x46E2F0 | fn_0046E2F0 | the held object's matrix with CHand +0x484C |
| 0x46E4B0 | `CHand::GetCreatureToGiveTo` | only in HOLDING (`HandStateHolding::CanCurrentlyGiveToCreature` 0x5B3C60, called at 0x46E4C2: the give timer [0xD13F3C] above 0) |
| 0x46E4E0 | `CHand::GameTurnUpdate` | each turn: the tug's held object (`HandStateTug::UpdateHeldObject` 0x5B8E60), the grain state's turn (`HandStateGrain::GameTurnUpdate` fn_005B2D70, with the turn's ms × 0.001), the dead spell's particles dropped (fn_0046E640) |
| 0x46E5D0 | fn_0046E5D0 | returns +0x489C, the grain state |
| 0x46E5E0 | `HandState::DrawTheHeldObject` | the default, nothing; 0x5B02D0 `HandState::Exit` the same |
| 0x5B3310 | `HandStateGrain::Update` | calls `HandStateHolding::Update` at 0x5B3361 |
| 0x5B57D0 | `HandStateMultiPickUp::Update` | pitch 1.3744469 (0x3FAFEDE0, about 7π/16); the hand put at the land under the object it is over plus its height + 3 ([0xBF01A0]); `HandStateMultiPickUp::DrawTheHeldObject` 0x5B5BD0; fn_005B5B90 0x5B5B90 copies a 3 × 3 matrix |
| 0x5B5D00 | `HandStateNormal::Enter` | the normal state's vtable 0x900BA0, slot 0 |
| 0x5B8E60 | `HandStateTug::UpdateHeldObject` | the tug's held object each turn; `HandStateTug::DrawTheHeldObject` draws it |
| 0x74BD20 | `Tree::SpawnUprootEffect` (fn_0074BD20, name inferred) | the uproot's dirt and smoke; only caller `HandStateTug::Update` (0x5B847E). It makes a 12-byte ground mark (fn_008251F0 → fn_00825240): mesh 0x251 (TreeRootsPile) at the tree's G3D object's place, scaled (box +0x24 + +0x2C) × scale × 0.3, life 15000 ms, then `SmokyStuff::Create(pos, 1, 1.0f, -1)` (0x8252EB), the white dust (2026-10-24, corrects the earlier "16-particle emitter") |

**The holding state's vtable** 0x900B40, 8 slots: +0x00 `HandStateHolding::Enter` 0x5B3AF0, +0x04
`HandStateHolding::DrawTheHeldObject` 0x5B56D0, +0x08 `HandState::Exit` 0x5B02D0, +0x0C `HandStateHolding::Update`
0x5B3C70, +0x10 `HandStateHolding::AllowCameraTricons` 0x5B3A70, +0x14 `HandStateHolding::GetExtraRoll` 0x5B3AC0
(0.0), +0x18 `HandStateHolding::GetExtraHeight` 0x5B3AD0 (0.0), +0x1C `HandStateHolding::AdjustRequiredPos` 0x5B3AE0
(nothing). The names of slots 5..7 are the reconstruction's. `HandStateGrain` (vtable 0x900B00) overrides slots 0, 2,
3, 5, 6 and 7 (its roll 0x5B2D50, height 0x5B2D30, adjust 0x5B32A0) and shares 1 and 4: it derives from
`HandStateHolding`. The holding state's own fields: +0x04 the hand, +0x08 the frame passed to `CAnim::fn_00860E00`
(the pose, through `CAnim::EvalFrame`), four Zoomers at +0x14, +0x44, +0x74 and +0xA4 (the last starting at the hand's
+0x4854, 10.0), the roll Zoomer +0xD4, the spring flag +0x104 and the spring's velocity +0x108
(`HandStateHolding::handVelocity`, `HandStateHolding::Update` adding a × 0.01, 0x900B60). The hold heights: ABOVE 0.2
(0x900B34), and the lowering held at no less than 1.9 (0x900B38, a strict compare). The tug's vtable is 0x900C1C; its
constants 0x900C08 = 0.13, 0x900C0C = 1000, 0x900C10 = 1000, 0x900C14 = 4000, with 9.81, 1.3 and 0.3; its 2D distance
from the camera is fn_006E8130. The FINGERS hold (4) has no working case in the holding state: it reads a stale local
(no object uses it: no `GetHoldType` override returns 4. Object 0x638BF0 and Poo give 1, `Animal::GetHoldType` 0x419310
and the Villager 0x750310 give 7, MobileObject 0x607120 and FieldCrop give 6, Tree 0x74B600 and DeadTree give 5,
MobileStatic 0x6091B0 gives 6 or 1, and SpellSeed 0x728680 gives 2 until it is ready, then its info's `holdType`
(+0x158), which is only 1, 2 or 6 in the 30 spell seed rows of info.dat. So the missing case is unreachable). The bones are posed onto the hand's matrix by
`LH3DTech::fn_00839F10` (misnamed "GetCameraPosition" in the symbols).

**CHand's fields** (`Morphable` first): +0x84 the heading, +0x9C the alignment (−1..1), +0xAC a float the force reads
(`GetMaxForce`), +0x47BC / +0x47C0 / +0x47D0 the fixed-position animation (animation, `CAnim`, time), +0x47F0 the
drawn bone matrices, +0x49AC the faded-from ones, +0x482C the hand's 3D object, +0x4834 handScale, +0x4838 the reach
(≤ 1800), +0x483C hidden, +0x4840 the texture set, +0x4844 forced invisible (interface mode 25), +0x4848 hidden by
request, +0x484C the left hand (mirrored), +0x4850 the interface status, +0x4854 the view distance (2..reach), +0x4858
the mouse moved in x, +0x485C / +0x4860 the mouse and +0x4864 / +0x4868 the last, +0x486C / +0x4870 a forced mouse
position used while the byte +0x4874 is set, +0x4878 the state and +0x487C the 11 states (+0x4888 tug, +0x488C
holding, +0x4894 multi pick-up, +0x489C grain), +0x48B4 the smoothing started, +0x48B8 / +0x48BC the smoothed mouse,
+0x48C0 / +0x48C4 its velocity, +0x48F8 the throw delay (180 ms while holding), +0x48FC the last object released (sent
in packet 0x40), +0x4900 the camera time, +0x4904 the held object, +0x4908 a grab still to be tugged free, +0x490C the
hold type, +0x4910 / +0x4914 the held 3D object's sorting and shadow kept, +0x491C the fixed-position animation
running, +0x4920..+0x4928 the tug's angular velocity, +0x492C..+0x4934 the object's base position at pick-up,
+0x4938..+0x4940 the held object's velocity, +0x4944 the hold lowering = max(the multiplier × height, 3.2 × scale ×
0.3), +0x494C / +0x4950 / +0x4954 the spell-in-hand particles, spell and flag, +0x499C / +0x49A0 the custom cursor's
surface and mode, +0x49B0 the blend timer, +0x49B4 / +0x49B8 / +0x49BC the creature to give to and its 3D object.

**The globals**: g_C5E640 the point under the mouse this frame (written by fn_0046DF60 and `HandStateHolding::Update`),
g_C5E650 the same of the last holding frame (their difference the throw velocity), g_C5E660 the blended draw position,
g_C5E670 the blend's source, g_C5E680 the target (fn_0046E160), g_D13F20 the held object's offset (`Holding::Update`),
g_D13F30 the land point under the mouse (also mirrored to 0xC5E640), 0xD13F4C the held aim's y offset, g_D13F3C the
give-to-creature timer (1100 when set; the holding state tests > 300; the point on the creature to give to is fn_00480810 0x480810 of its 3D body), 0xD13F60 (g_D13F60) the last hand up, g_D13F70 the fixed
hand position used once while [0xD1404C] is set, g_D13F80 the distance zoomer (0x30 bytes: value, destination, speed
at +0xC = 0xD13F8C...), g_BF01CC the zoomers' first-reset flag, g_BF01D0 / g_BF01D4 / g_BF01D8 the zoomer's 0.28 /
2.0 / 0.1 s, [0xE852C0] / [0xE852C4] LHMouse's x, y. The grain state's constructor (0x5B2C78), the vortex (0x5FF146),
`UR_ChangeScale`, `UR_MoveAtom` and fn_0069FEF0 share one helper with the hand.

### Sources

- The exe read with the xref index; the decompiled reconstruction of the pickup path (`decomp_pickup`: `hand.cpp`,
  `NOTES_hand.md`), each function checked in the disassembly; `hh.HBN`, `hndspec5.txt`, `Hand_Boned_Base2.l3d` and
  AllMeshes.g3d read from the frozen game data.

## openblack

The hand's state lives in ECS components and the `HandSystemInterface` service, never in globals; its assets load through the resource caches. **Done**: the hand's mesh, clips, lean and navigation poses, the two pose buffers and the blend, the frame order, the
placement on the mouse ray, the hand's land under the cursor, the hand's mesh test and the morph. **Not ported**: the
mirrored left hand, the CAMERA state's own blends and swaps, the system cursor, the hand demos' playback flags. The
port decisions taken from raffclar's code are in
[Port decisions: the hand, the camera and the interface](../project/port-decisions-hand-camera-interface.md).

### The hand's scale in openblack

- `hand_land_point::ScaleForViewDistance` keeps the distance as `ClampedViewDistance` does and takes the power as the
  game's runtime works it out (`gutils::x87::Pow`, see
  [Floating point](../engine/floating-point.md#the-runtimes-arctangent-arccosine-power-and-logarithm)), then the
  far fade in the original's order: 10 000 of 10 000 scales equal `CHand::SetDistanceFromView` run whole in the
  oracle (8000 distances stepped through the floats from 2 to 10, 2000 up to 2048 at seven reaches;
  `test_hand_scale`).

### The poses in openblack

- `src/3D/HandNavigationPose.h` (raffclar's names: `hand_navigation_pose::WhileDragging`, `WhileHovering`,
  `UpTowardsFocus`, `RotateLift`), used by `HandSystem::NavigationPose` / `HoverPose` and the camera branch of
  `HandSystem::Place`; tests in `test/hand/test_hand_navigation_pose.cpp`. raffclar's version leaned the up by 0.4 and
  gave (0, 1, 0) at the focus; the exe's 0.3 and its zero are taken.
- A grip the camera does not drag (a hand demo's, a drag given up, another camera's) keeps Cgrip; no hover pose while a
  script camera, a miracle's camera path or a hand demo has the camera (the hints are not worked out then, and the
  playback flags are not ported). The state blends at a change of pose wait for the CAMERA state (Pending).
- The hand over the leash rope (0xF Chold_fingers) needs a pick of the rope and is not ported; openblack's hand does
  not read the camera's ring cursor (+0x4874).
- **The buffers** (`HandPoseBuffers`, `HandStatePose`): the state change swaps the buffers, after INVISIBLE's copy;
  the normal state swaps at its change of pose, the empty hand dragging the land (which stands for the CAMERA state)
  entering it afresh. openblack has no kept pose until the first change and blends nothing then. The CREATURE state
  swaps and starts the blend at each change of its cycle to Cslap or Ctickle, twice when one step makes both, and
  not when the caress turns back to Cwiggle (`creature_feedback::NextCycle`'s `blendStarts`, read by
  `HandSystem::Update`; tests `CreatureTouch.ChangingToTheSlapOrTheCaressStartsTheHandsBlendAndBackToTheWiggleDoesNot`,
  `CreatureHandSystemTest.OnlyTheChangesToTheCaressOrTheSlapStartTheHandsBlend`), as the original's
  ([the clip](states.md#the-creature-states-update)). The CAMERA state's swaps come with that state; the held object's
  replacement and the land grip's start are not ported as swaps; the animator restarts a clip's clock at a change of
  clip, where NORMAL's runs on. The CREATURE state shows its cycle at its own clock (`HandStateClock`): zeroed at its
  Enter, on by the frame's real ms (g_delta_time's) and wrapped at the length of the cycle shown, so a change of cycle
  does not restart it, as the original's +8; the CAMERA and play-anim states keep their own as well.
- **The blend**: `HandSystem::BlendHandWorld` blends the bone matrices in world space, as the original does. openblack
  draws the hand as its model times model-space bone matrices, so each is taken to world space (model × bone), blended,
  and taken back with the inverse of the model (⚠️ approximate: the original keeps world matrices only).
- **The hidden hand**: `UpdateTreeBends` reads the hand's `NotDrawn` after the hand's frame update, so from the same
  frame as the original ([The original loop](../engine/loop.md#the-original-loop)). The system cursor
  (`FrontEnd::SetCursorOn` when +0x4844 is 0) is not ported.

### The frame order in openblack

`HandSystem::PrepareState` (the required state, the CREATURE wait and its packets, the change's Enter, the blend start
and the hide) runs first in `Game::Update`'s hand, before the box rule (`BoxTakesInput`), `ResolveCursorPoint` (the
interface collide and the hand's land point) and `HandSystem::Update` (the interface's frame pass, then the state's
update and the blend), then `Place`. The release impulse (packet 0x40, `GetRequiredState`) is sent first in
`PrepareState`, before the state change, every frame (inside the citadel nothing, 0x46CD1C), hidden, gripping or over
the sky alike.

The stop of step 6 is `hand_detail::DrawPreparationStops` (the INVISIBLE state, a box with the mouse, the interface
inactive), taken by `PrepareState` after the state change into `_preparationStopped`; the box is
`interface_active::BoxHasMouse`, what the last frame's pass found, as the original's [0xC4CCEE]. While it holds,
`HandSystem::Update` skips the mouse smoothing and returns before the state's update (the totem's slide, the creature
hand, the play-anim and camera clocks, the normal state's pose change), the animator and the blend, and `Place`
returns: the hand keeps its last pose and place, and stays drawn unless INVISIBLE. The interface's part of `Update`
(the presses, the hover) runs on, as the original's pass does after the draw. Tests `HandDrawPreparation.*`
(`test/hand/test_hand_held_draw.cpp`); the Game Detail Overlay's "Hand Preparation" line shows when it is stopped. The
test-hook hand demos keep the box rule off (`hand_demo::BoxRuleReachesHand`), so no box stops the hand in them.

### The placement in openblack

- **The land point**: `HandLandPoint` with `_smoothedPosition` (before the hand's first placement (10, 10, 10),
  `k_HandStartPosition`) and `CursorLand::nearPoint`; with no object under the cursor,
  `hand_land_point::AlongMouseRay` in `ResolveCursorPoint`. The fight sparkles' fixed position is not ported.
- **The start position**: the hand entity is still made at (0, 0, 0) on every land, so what reads its Transform before
  the first placement differs; nor is the position read from a saved game (Pending).
- **The distance zoomer**: `_handDistanceStarted`, so that the zoomer's first start is at 0; its later starts jump to
  the target as before. The restart on every `HandStateNormal::Enter` is not ported (Pending).
- **Holding**: `hand_land_point::ScaleForViewDistance` before the subtracted call. The giving to the creature is
  ported ([Grabbing, holding and throwing](grab-hold-and-throw.md#applying-a-held-object-in-openblack)), but its terms
  in the placement are not: the subtraction while a creature waits to be given the held object (+0x49B4), the
  creature's push on the hit point and the zoomer's 2.0 s while giving (`HandSystem::ResolveCursorPoint` keeps
  the hold without them). In `HandSystem::Place` the hold height's base is 0.2 for
  `HoldType::Above` and 3.2 × the hand's scale for `HoldType::Magic` (a spell seed before it is ready).
- **The hand's land under the cursor**: `HandLandUnderCursor` (Game.cpp) with
  `PickingSystemInterface::LandFrom2DPoint(cursor, true, near + 1)` (`screen_point::PixelPoint`,
  `ecs::hand_land_point::ShiftedAlongView` and `land_line::LandOrSeaAlong`), the distance by
  `hand_land_point::HandLandPoint`, passed to `ResolveCursorPoint` as `CursorLand::hand`.
  Measured against the exe at the map's bounds (the oracle on `LH3DIsland::RayCast` 0x802550 with a fake flat land of 20 height units in `g_index_block` / `g_ptr_blocks`, the camera at the line's start): from (−500, 67, 505) towards (−400, 60.3, 505) it gives x 300.00003 (0x43960001); from (−8000, 500, 2560) towards (−7990, 499.5, 2560) x 1732.6891 (0x44D8960D); from (5000, 20, 2560) towards (5010, 19.9, 2560) the land is not met before the edge and the sea gives x 6999.992 (0x45DABFF0); from (5000, 100, 2560) the sea at 15000 is out of reach (returns 0); a rising line returns 0. There is no other bound on the hand: past these it stays along the mouse ray at its clamped distance. Test `LandLine.TheHandsRayStopsAtTheMapAsTheOriginalsDoes`.
- **The interface's land under the cursor**: `PickingSystem::LandUnderPixel(eye, near point, true)`
  (`land_line::UnderPixel` and `KeptInReach`), passed as `CursorLand::interface`.
- **The gestures**: `sampling::InterfaceLandUnderPixel` and, for the trail, `sampling::LandOrPointAlongPixel`; the near
  plane is kept in `Camera::SetPixelNearClip`.
- **The feel**: `hand_feel` (`Hand/HandFeel.h`): `FeelOf` (what the hand does with the thing picked), `FeelDirection`
  (the raised direction), `RestAtPosition` (before a villager or an animal), `RestOnModel` (on a felt model) and
  `TurnsToLand` (the land normal instead), called from `HandSystem::ResolveCursorPoint` and `UpdateNormalUp`.
- **The mouse ray**: `Camera::RayFromEye` (the eye, and that direction from the view's axes, `_xFov` and `_aspect`,
  summed and normalised in the original's order). The hand's mouse ray (Game.cpp, to
  `HandSystem::ResolveCursorPoint`, the creature's hand and the tug), the gestures' points along the line while the
  falling spell draws its own view (`GestureInput`) and the temple camera's mouse use it. The camera's land under a
  pixel is the island ray from the pixel instead
  ([The land under a pixel](../camera/player-camera.md#player-camera-the-land-under-a-pixel)).
  `Camera::DeprojectScreenToWorld` started the ray at NDC depth 0 of glm's −1..1 projection, 2·near·far / (near + far)
  ≈ 2·near in front of the eye, with its direction through the inverse view-projection; it stays only for the
  editor's picks off the land. Because the hand is `origin + mouseDir × distance`, the eye start moves it by about
  2·near along the ray wherever its distance is clamped or the zoomer lags: that is the original's placement.
- **Holding and grain place**: `_heldGroundPoint` from `_cursorLandPoint` in the normal, camera, holding and grain
  frames, and `_requiredHandPosition` from the ground point while a pile is locked, else the hand's point before its
  lifts.
- **The hand's mesh test**: `ecs::hand_mesh_ray` (src/ECS/HandMeshRay.h): `Tested`, `ToWorld`, `Triangle`, `Nearest`
  and `AlongRay`, the products worked in double and the stored values rounded to floats; the sub-mesh levels are
  loaded as the original's (`l3d::LoadedFlags` in `L3DSubMesh::Load`). `HandSystem::PickObjectAlongRay` runs it for the
  object it picks by its mesh (and the temple's entrance), with the matrix it is drawn with, into `CursorHit::surface`;
  `ResolveCursorPoint` places the hand at its point (s by `AlongRay` when holding) and takes its normal for the up
  target, and a miss keeps the land's point and up. The pick (`L3DMesh::RayIntersect`) is unchanged; until 2026-10-09
  its hit also placed the hand, which differed: the lower levels of detail were tested, the temple heart's status shell
  was not, the normal was the winding's (so the up target leant into most surfaces), there was no 0.005 epsilon, points
  on edges hit, and the test was in the mesh's space. Tests: test/hand/test_hand_mesh_ray.cpp.
- **The hand is drawn unmirrored**: the mirrored CHand (+0x484C) and its left-handed mode are not ported (Pending).

### Test hooks

- `OPENBLACK_TEST_HAND_ALIGNMENT=<-1..1>`: the hand's good / evil morph ([Good and evil](picking.md#good-and-evil)).
- `OPENBLACK_MOUSE_AT="fx,fy"`: the cursor fixed at a fraction of the window, so the hand appears in captures.

## Pending

- **The distance zoomer while the draw preparation is stopped** ([The frame order](#the-frame-order), step 6): the
  original's zoomer and land point are taken in the state's Update, which does not run while hidden, under a box or
  with the interface inactive; openblack's `ResolveCursorPoint` advances `_handDistance` every frame. To be fixed as a
  measured commit of its own (it also moves the hidden and inactive frames).
- **The mouse ray's rounding** ([The mouse ray](#the-mouse-ray)): `Camera::RayFromEye` takes the screen point in 0..1
  (the cursor over the window size), not integer pixels over W/2, and does not round the point through the near plane
  and `g_camera` as fn_0074CBD0 does (the eye added in `Get3DPointFromScreen` and taken away again): its direction can
  differ from the original's in the last bits. While the falling spell draws a view of its own, the ray starts at that
  view's eye; whether `g_camera` is that eye then has not been read.
- **The distance zoomer's start on entering the normal state** ([Where the hand is placed](#where-the-hand-is-placed)):
  every `HandStateNormal::Enter` 0x5B5D00 sets the zoomer 0xD13F80 to +0x4854, the hand's last view distance (10.0
  from the constructor, 0x46BBB7). openblack starts it again only when `_handDistanceValid` is false, which the land
  grip sets (`ResolveCursorPoint`), and then jumps it to its target; the original's trigger is the change into the
  normal state, from any state, not the grip. Not ported until the writers of +0x4854 are: `SetDistanceFromView` in
  ORHP (0x5B5FDD, 0x5B6D67) and in the holding state's update (0x5B3DBF), the camera state's (0x5B04B5, 0x5B05BB), and
  the grain state's after its placement, whose address has not been read (queued fix F-80). openblack keeps the value
  where the scale is taken (`_viewDistance`); the camera state's distance Zoomer starts from it.
- **The CAMERA state** (`CHand::GetRequiredState`, CAMERA 2 when the camera mode is CameraModeNew3 with +0x8C & 1,
  0x46CEFA..0x46CF3B): with it, the state blends at the camera state's changes of pose (fn_0046C500 at 0x5B0788 /
  0x5B07BC) and the object, bubble or leash case at the change to Cgrip.
- **The empty hand's up over an object** (0.25 toCam − hitNormal + (0, 0.5, 0), with
  `HandShouldFeelWithMeshIntersect` vt +0x59C and the hand higher than 1.6 hand sizes above the ground): the empty
  hand's up at ORHP's tail; the rotate tricon's up is in ([The navigation poses](#the-navigation-poses)).
- **The held object's matrix**: the frame's last `SetDistanceFromView` at 0x5B499B with |camera − CHand +0x78| in
  HOLDING / GRAIN; the exception [0xD47824] && +0x4904 == [0xD47828] (0x46CA3A..0x46CA4E) of the held object's blended
  place in PrepareForDrawing is not identified. (The held object's matrix itself is on
  [Grabbing, holding and throwing](grab-hold-and-throw.md#pending).)
- **The giving's terms in the placement** ([Where the hand is placed](#where-the-hand-is-placed)): ORHP's subtraction
  while +0x49B4 (the creature to give the held object to) is set, the creature's push on the hit point and the
  zoomer's 2.0 s ([0xBF01D4]) while giving are not ported, though the giving itself is.
- **The camera's edge rotate** writes the ring cursor to CHand +0x486C / +0x4870 with +0x4874 = 1 (0x45D24A..0x45D259);
  what the hand does with it is not read, and openblack's hand does not read the camera's warp (`GetCursorWarp`).
- **The mirrored hand**: openblack draws the hand unmirrored; the mirrored CHand (+0x484C) and its left-handed mode are
  not ported.
- **The hand's start position**: the hand entity is made at (0, 0, 0) on every land; the original's CHand starts at
  (10, 10, 10) and keeps its place across lands.
- **The barn's "Did you know?" sign on Land 1** was not under the cursor at any screen point tried while checking the
  sign tap, where the sign by the fence was. Whether its pick (its mesh, its height or the land under it) differs from
  the original's has not been looked into.
- **Tooltip items** of the hand are in [Tooltips, Pending](../interface/tooltips.md#pending).
