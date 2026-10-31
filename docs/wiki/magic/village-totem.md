# The village totem

## Summary

Each town centre has a totem: a plinth with the player's creature's icon on top. The player sets how many of the
town's villagers go to worship by gripping the totem with the hand and dragging it up or down the screen; while held
the icon rises with the share, a see-through copy shows the share the town has now, and a tooltip gives the
percentage. Letting go sends the new share to the town. The grip needs a built totem, a built temple heart and a
built worship site for the town. The totem turns to face its town's worship site and changes its icon when the
player's creature changes.

openblack ports the press, the hand's state and pose, the drag, the let go, the drawing with the see-through copy, the
icon, the script properties and the hand's size in the state; the immersion, the creature's desire update at the grip
and the grey floating number are not ported. The hand's other states are on [the hand's states](../hand/states.md).

## The original

The addresses below are those of the v1.42 exe, read with the 1.20 symbols.

### The locked select on a village totem (hand state 5)

The totem's own side (who may grip it, the share, packet 0x29, the drawing) is in
[the grip, the drag and the let go](#the-village-totem-the-grip-the-drag-and-the-let-go).

- **The press.** There is no branch of its own: the totem is step 4's object of
  [the action press](../hand/states.md#the-action-press), like piles, fields, fish farms and creatures, and which one
  starts is decided by the object under the hand, not by its type. It needs `ValidForLockedSelectProcess` 0x738500,
  `IsCannotBePickedUp` clear (Flags bit 0x2000), Flags & 0x10 clear (nobody holds it) and the hand in the influence
  (TotemStatue keeps Object's `InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1). It is not valid to tap (vt
  +0x740 = Object 0x4196B0 = 0), so `StartTapOrLockedSelect` starts it at once unless the leash is in the hand (the
  225 ms wait). A totem that fails goes on to step 5, where it is neither placeable nor tappable.
- **The hand state.** IN LOCKED SELECT gives cursor state 14 for an object that is not a creature (0x5D80C0); with
  nothing in the hand `CHand::GetRequiredState` 0x46CD10 turns 14 into hand state 5, `HandStateTotem` (jump table
  0x46CF48 / 0x46CF5C, 0x46CEDB). Its vtable 0x900BE4: Enter 0x5B7690, Exit 0x5B7790, Update 0x5B77A0, which
  `CHand::PrepareForDrawing` 0x46C550 calls every drawn frame (0x46C921).
- **Enter** 0x5B7690: the object is the interface's +0x3C8; the clip frame (+8) = 0. The grip height is `GetHeight` ×
  0.7 ([0x900BDC]) + the y of its 3D object (+0x40 → +0x3C), or its +0x1C + the land with no 3D object; it goes into
  the state's +0xA8 and the hand's y (+0x7C). The hand's x and z (+0x78 / +0x80) are the totem's MapCoords × 10 /
  65536, and they are not changed afterwards: the hand stays at the totem. `SetDistanceFromView(fn_0046DB70)` and
  fn_0074CAF0(+0x485C, &hand point, +0x4854) (not read). `LHMouse::PinImage(0x8000, 0x8000)` pins the cursor image
  where the mouse is.
- **The mouse's move.** `PrepareForDrawing` (0x46C68F..0x46C6D8) keeps the last sample (+0x485C / +0x4860 into
  +0x4864 / +0x4868) and takes the mouse's position (LHMouse +0xBC / +0xC0); CHand +0x486C / +0x4870 replace it only
  when +0x4874 is set, which only the camera does. So dy = +0x4868 − +0x4860 is the move **up the screen, in pixels,
  since the last drawn frame**.
- **Update** 0x5B77A0, every drawn frame:
  - when dy is not 0: slide = dy × `GetHeight` × 3 ([0x8C2C50]) ÷ the screen's height (LHSys screen +0xA) (fild,
    fmul, fmul, fidiv); going down, it stops at the land: only a slide below 0 is tested (0x5B7820..0x5B782E), and
    when −slide > hand y − the land under the hand (hand y is the last frame's grip height), slide = −(hand y − land)
    (0x5B7830..0x5B788C). A slide up is never held back, even with the hand under the land (a sunk puzzle totem's
    grip). The land under the hand is `GetAltitude` at ftol(the hand's x and z × 65536 × 0.1): Enter's point, the
    held object's own MapCoords, which nothing moves while the state runs. Then, while the totem has Flags & 0x10
    and `IsReadyForNetworkUnfriendlyLockedSelect` (Object 0x4027E0 = 1), `NetworkUnfriendlyLockedSelect(&slide)`: the
    held share moves by slide × 0.1;
  - the grip height again (`GetHeight` × 0.7 + the icon's y) into the hand's y and +0xA8;
  - the hand point is projected (`ProjectPoint` 0x5B7C10 with g_world_to_clipping); when it is on the screen, CHand
    +0x486C / +0x4870 = its point (ftol) and `PinImage(x, y)` (rounded). **The "warp" only moves the cursor's image**:
    `PinImage` 0x7E5550 sets LHMouse +0xCC = 1 and the image's point +0xC4 / +0xC8, which `UpdateDrawRegions`
    0x7E55C0 draws instead of the mouse's; the mouse itself is not moved. Exit 0x5B7790 sets LHMouse +0xCC
    ([0xE852D0]) = 0. `ProjectPoint` flags only a point nearer than the near plane (the depth against
    g_info_transform +0); x = (x ÷ depth + 1) × half the width and y = half the height − half the height × y ÷ depth,
    each kept from 0 to `g_max_screen` (the width − 1 and the height − 1, `LH3DTech::UpdateViewPort` 0x8190AB..0x8190C5)
    before the rounding. **The pinned image is not seen**: while the hand is shown `CHand::Show` 0x46C1B0 sets the
    mouse's image to none (`SetCursor(0, 1, 0)` 0x46C215, so `UpdateDrawRegions` returns at its first test, +0x14 &
    ~7 = 0) unless CHand +0x49A0 is set, and +0x49A0 is written only by the constructor (0x46BC20, 0) and by
    sub_46C380, which nothing calls or points to (xref and a whole-text scan, 2026-10-22);
  - **tilt**: a = 7π/16 ([0x900BF8] = 1.3744469); when the hand's y is below 2.5 ([0x900BD8], the world height, not
    the height above the land), a = atan((2.5 − y) ÷ 3.2) + 7π/16 (3.2 is fn_0046C040, [0x8C7C8C]; `fld1`,
    `fpatan`). The hand's matrix is M · R, M from fn_0046D080 and, in row vectors, R = Rx(a) · Ry(b) with b = CHand
    +0x84 + π: rows (cos b, 0, sin b), (−sin a sin b, cos a, sin a cos b), (−cos a sin b, −sin a, cos a cos b). The
    position is the hand point;
  - **the clip**: `Morphable::GetAnim(9, 0)`, Chold_side (the clip `HandStateHolding` takes for TREE, SIDE and
    VILLAGER, 0x5B53BA). closure = min(1, `GetHoldRadius` ÷ (3.2 × 0.5)), with `GetHoldRadius` = `GetHeight` × 0.75
    because the totem keeps Object's hold type ABOVE (0x638BF0 = 1, 0x638C00). frame = ftol((clip[0] >> 1) × (closure
    × 0.5)), into +8, posed with fn_00860E00(clip, matrix, ..., frame, ..., 0, 1). clip[0] is the clip's length in
    ms (read 2026-10-24): `CAnim::ReadBinary` 0x860860 stores the record's first u32 at +0 (0x860889) and the second's
    bit 0 at +4, the looping flag (0x8608A5); `CAnim::FillBuffer` (fn_00860E00) takes +0 as the period of the time it is
    given (0x860E16, or +0 × count ÷ (count − 1) for a one-shot clip, 0x860E1A) and the key as count × time ÷ period
    (0x860E2F); the hand's clips come through the same reader (`Morphable::GetSetAnim` 0x6198D1). openblack's
    `GetDurationMs` is that same first field (`morph::AnimationHeader::duration`; test
    `SkeletalFromMorph.TheHandsClipFieldsReadTheSameBytes`), so the frame is the original's;
  - `GetHeight` (Object 0x638120) is 2 × the 3D object's mesh +0x28 × the scale (+0x50), of the +0x40 object: the
    icon. Only `Creature` 0x477F50 and `MagicFireBall` 0x682D30 override it (and `GameThingWithPos` 0x405500 below
    `Object`), so the icon's is this one; +0x28 is the mesh box's half height (max − min) × 0.5 from
    `LH3DMesh::ComputeBoundingBox` 0x8081B0 ([the mesh box](../engine/object-size-and-map-cells.md#the-mesh-box)).
- The share is applied only at the end of the locked select (packet 0x29, below).
- **The tilt's numbers as the FPU works them** (0x5B7963..0x5B7A2F): the 2.5 − y is stored as a float, divided by 3.2
  (`fn_0046C040` returns the float 3.2), its arctangent taken with `fpatan`, and the double 1.3744468688964844 (7π/16
  as a float, [0x900B80]) added; the sum is negated (a = −tilt) before `fcos` / `fsin`. The rows are worked with
  explicit zeros: (cos b − 0, 0, 0 + sin b), (0 − sin t sin b, cos t, sin t cos b + 0), (0 − cos t sin b, sin(−t), cos
  t cos b + 0), t the tilt. b = H + π is the float H plus the float π ([0x8C36A0]); cos b and sin b are stored as
  floats for row x and for cos b in rows y and z, but rows y and z multiply sin t and cos t by the extended sin b still
  on the FPU stack, each product rounded once to a float when it is stored. H is CHand +0x84, `Morphable::Heading`, a
  float that starts at 0; its only writer is `CHand::PrepareForDrawing` (0x46C725), once per frame before the hand
  state's update: if |ray.x| > 0.01 or |ray.z| > 0.01, H = (float) `GetYAngle(ray)`, ray the unit camera → mouse
  direction ([the mouse ray](../hand/animation-and-placement.md#the-mouse-ray)); otherwise H is kept. `fn_0046D080` (the
  M) is the hand's uniform scale matrix, 3.2 ÷ +0x8C × +0x4834 × +0x90 (stored at +0x94), its x negated when +0x484C is
  set. The closure is `GetHoldRadius` (vt +0x590) ÷ (3.2 × 0.5), above 1 taken as 1; the frame is the clip's first
  field shifted right (`sar`, signed) as an integer, times closure × 0.5, through `ftol`.

### The village totem: the grip, the drag and the let go

TotemStatue fields: +0x7C the town centre, +0x80 the share last set (`SetWorshipPercentage` 0x738270 stores it first,
0x738280), +0x88 the share the hand holds, +0x40 the object's 3D object (the icon), +0x8C the second 3D object, +0x90
/ +0x94 / +0x98 the last rise drawn (× 8), +0x9C the Zoomer's current value (+0xA0 its destination), +0xD4 "no tooltip
at the next re-place". The hand's side (the press, hand state 5 and its pose) is in
[the locked select](#the-locked-select-on-a-village-totem-hand-state-5).

- **Who may grip it**, `TotemStatue::ValidForLockedSelectProcess` 0x738500 (vt +0x6CC), in this order and nothing
  else:
  1. the status's player (`GInterfaceStatus::GetPlayer` 0x5DBC20) is the totem's (`TotemStatue::GetPlayer` 0x738490:
     the town's player; with no town (0x7384AC) the player at g_game +0x205A5B, the neutral player's slot (7))
     (0x73850A..0x738518). That byte is written only as 7 (0x550458, 0x554988). A totem with no town can never be
     gripped anyway: the last test calls vt +0x48 (`GetTown`, 0x738552) and fails on a null town (0x738557), before it
     reads the town's worship site (vt +0x30C);
  2. the totem `IsFunctional` (vt +0xD4, MultiMapFixed 0x52EF70): available (byte +0xA bit 0 clear), `IsBuilt`
     (0x5610B0: +0x58 bit 1 clear and built >= 1) and its life above 0.75 (`GetPercentRepairedForNonFunctional`
     0x52EFC0) (0x73851E);
  3. the player's citadel (GPlayer +0xA48) has a heart (+0x30) and the heart `IsBuilt` (CitadelPart 0x464AD0)
     (0x73852C..0x73854C);
  4. the totem's town has a worship site (`Town::GetWorshipSite` 0x73C940) and the site `IsBuilt` (0x77BDD0)
     (0x738552..0x738573).
- **The grip**: `NetworkUnfriendlyStartLockedSelect` 0x7385E0 (vt +0x6D4, on the pressing machine): +0x88 = the share
  set (+0x80), `StartImmersion(0xF, 0x80000000)`, `GGuidance::StartRaiseTotemSFX(status, +0x88)` (stores it at the
  guidance's +0xBC). Then packet 0x1B, whose handler sets Flags & 0x10 and calls `NetworkFriendlyStartLockedSelect`
  0x738590 (vt +0x6D0) on every machine: if the gripping player has a creature (GPlayer +0xA4C),
  `Creature::UpdateDesiresAfterObservingPlayerAction(CREATURE_DESIRE_TO_IMPRESS)` 0x477D80 (only when
  `CanCreatureSeePlayerHand`); then `Reaction::CreateReaction(totem, 0x11 REACT_TO_HAND_USING_TOTEM, that player, 1)`
  0x6E3D70.
  - `UpdateDesiresAfterObservingPlayerAction` 0x477D80: when `CanCreatureSeePlayerHand` 0x477800,
    `UpdateDesiresAfterObservingAction` 0x477DA0, which calls `CreatureDesires::DivideIncreaseTimeTimeBy(desire, x)`
    0x4DC3D0 on the creature's desires (+0x164 + 8), x = fn_00477DD0(desire) = the float at 0xC67EF0 + desire × 0x1C0 ×
    the creature info's +0x290. The desire pushed is 0, `CREATURE_DESIRE_TO_IMPRESS`.
  - `CreateReaction`'s last argument (an int): when it is not 0 the reaction's +0x2C is the game turn (g_game
    +0x205A40), stored after the spread (0x6E3DE2..0x6E3DFA).
- **The drag** is not a packet. `HandStateTotem::Update` (every drawn frame) calls `NetworkUnfriendlyLockedSelect`
  0x7386A0 (vt +0x6DC) with the slide of the hand: while Flags & 0x10, **+0x88 = clamp(slide × 0.1 + +0x88; 0; 1)**
  and nothing else. The share set, the villagers and the town are not touched while the hand holds it. The slide is
  `dy × GetHeight × 3 ÷ screen height`, dy the mouse's move up the screen in pixels since the last frame.
- **The let go**: the locked select's end (action state 3's end 0x5D4870, or state 5's 0x5D2210, both followed by
  packet 0x1C) calls `NetworkUnfriendlyEndLockedSelect` 0x738630 (vt +0x6E8; the symbol's name, not `EndLockedSelect`):
  `SetPacket(0x29, the totem's index, +0x88)` 0x5514D0, `EndRaiseTotemSFX(status, +0x88)` 0x71BED0 (guidance sound
  3, `PlayNow(3)`, only when the share let go is above the one gripped; then fn_0071C690, the player's alignment class
  (1 above [0x98014C], 2 below [0x980150], else 0), whose result is dropped: nothing is played. `StartRaiseTotemSFX`
  0x71BEB0 only stores the share at the guidance's +0xBC) and `StopImmersion(0xF)`. So there is one 0x29
  per locked select, at its end (state 3's end skips it, and packet 0x1C too, when the totem is unavailable, +0xA &
  1). `NetworkFriendlyEndLockedSelect` 0x738690 only returns 1.
- **The turn's pass** (`GInterfaceStatus::Process` 0x5DC4E0, [each turn](../hand/states.md#each-turn)) also ends the
  locked select, and the hand's action with it: a totem that is gone ends it with nothing sent, and a totem held
  where the player's influence at the hand's synced point is not above 0 (0x5DC540 → 0x5DC558) is let go, its end
  sending the share it was held at (0x29) and 0x1C.
- **Effects**: `TotemStatue`'s `IsEffectReceiver` (vt 0x774, 0x561130) says no, so no effect (a blast, a fireball's
  heat, a cast's targets) reaches a village totem ([the blast](lightning-explosion.md#ur_explosion)).
- **Packet 0x29**, handler 0x63D1F8 (ProcessPacket table 0x63DDCC):
  - with a town: when the share > 0 and the town has a player, that player's
    `MakeCreatureEmpathiseWithPlayer(CREATURE_DESIRE_TO_IMPRESS, 0.5, the totem's MapCoords)` 0x4C80B0 (only if the
    creature `CanSeePos` it); then `Town::SetWorshipPercentage(share)`, which sends the villagers;
  - without a town: `TotemStatue::SetWorshipPercentage(share)`;
  - then +0x88 = `GetWorshipSpeed` (+0x80), and a ValueSpinner "%3.0f" of share × 100 at the totem (its MapCoords, the
    land + its +0x1C), colour bytes 80 80 80 FF (grey), Init 0x833A00: 5 s of life, rising 10 per second
    (`UpdatePosition` 0x4382D0). It is made on every machine.
- **AddToPlayer** 0x738130 (vt +0x8DC): with a town that has a worship site, the y angle is
  `GUtils::Get3DAngleFromXZ(totem, site)` + π/2 ([0x8C78D8]) through `SetYAngle` (Object 0x639260); then, with a
  town, `SetPlayersCreature` 0x7381C0 and `Town::SetWorshipPercentage(0)`; without a town only
  `TotemStatue::SetWorshipPercentage(0)`.
  - `SetPlayersCreature`: when the 3D object +0x40 exists, it is taken out of the map, given the mesh
    `GPlayer::GetPlayerTotemForTown` 0x64B650 (the player's creature's info +0x344 `totemIcon`, the record's +0x334
    in info.dat, which the game keeps 0x10 bytes behind the file; or 0xFA = 250 with no creature; an index outside
    the mesh pack gives 0), put back in the map; +0x90 = +0x94 = FLT_MAX (the next draw re-places it) and +0xD4 = 1
    (that re-place shows no tooltip). Only the icon is re-meshed.
  - Called by `TotemStatue::Create` 0x737D25 (the creation) and by `TownCentre::AddToPlayer` 0x7443A0, which the
    town's player setter fn_0073A8F0 runs on each of the town's buildings: from the Town constructor (0x739597) and
    when the town changes player (fn_00649810, which first sets the town's share to 0 at 0x6498D8).
    `SetPlayersCreature` alone also runs from `ResolveLoad` 0x73894A and whenever a player's creature is set
    (fn_0064B4F0 → fn_0064B510 → each town centre's fn_00743BD0; from `Creature::Initialise`, `SwapMinds` and
    fn_0065BA40).
  - fn_0064B4F0 is the player's creature setter: GPlayer +0xA4C = the creature, then fn_0064B510 walks the player's
    towns (+0xA50, next +0x75C) and, for each town's centre (+0x9A4) with a totem (+0xCC, fn_00743BD0),
    `SetPlayersCreature`. `GetPlayerTotemForTown` 0x64B650 reads the creature's own info (+0x28) at +0x344, the
    record's +0x334 in info.dat.
  - The creation's order: `TownCentre::CreateTotemIfNecessary` 0x743DA0 (from `TownCentre::MakeFunctional` 0x743E9C
    and from `Town::SetWorshipSite` 0x73D06C; it needs the centre's town (+0x98) available, no totem yet (+0xCC) and
    the centre built) stores the new totem in +0xCC only after `TotemStatue::Create` returns (0x743DD7). So at the
    creation `AddToPlayer`'s `Town::SetWorshipPercentage(0)` finds no totem (`Town::GetTotemStatue` reads the centre's
    +0xCC) and only sets the town's share. And `SetWorshipSite` makes no second totem: a worship site linked to the
    town after its totem was made does not turn it. A totem turns to its site only at its creation and when its town
    changes player; the lands' scripts make their town centres before `CREATE_WORSHIP_SITE`, so their totems keep the
    centre's angle until then.
  - `SetYAngle` (Object 0x639260) does nothing when the angle is the one it has (+0x4C); otherwise `SetXYZAngles`
    0x638D00 takes it out of the map, stores it and re-places the 3D object. The draw places both 3D objects (+0x8C
    the plinth, +0x40 the icon) with the object's y angle (`GetYAngle` 0x402500 = +0x4C) and scale.
- **Drawing**, `TotemStatue::Draw` 0x738960 (every frame, only while `IsBuilt` and life > 0):
  - first = +0x88 × 8 and second = the Zoomer's current × 8, swapped while Flags & 0x10: held, the totem stands at the
    held share and the other at the risen one; not held, it stands at the risen one and the other at +0x88, which is
    then the share set, where it is going;
  - when the two differ: both 3D objects (+0x8C, and +0x40 2.729 above it) are placed at the first rise and drawn with
    byte +0x4F = 0x80 between vt +0x48(1) and vt +0x48(0) (0x738B15..0x738C7A): **a second, see-through totem** at
    alpha 0x80;
  - the totem itself is re-placed when its rise changed since the last draw (+0x90) or a second one was drawn; then,
    if +0xD4 is 0 and the totem is the local player's (g_game +0x205A59): `ToolTips::ForceToolTips(0xEEB, +0x88 ×
    100)` (0x738CAC..0x738CF2); +0xD4 goes back to 0. So the tooltip shows on every frame the height changes: while
    held and slid, while it rises after the release and after a script change, always with +0x88; and on every frame a
    second totem shows, held still too. `ForceToolTips` 0x5C9C60 is `SubmitToolTips(text, -1, 0, 1, 0)` with the value
    stored for the text's number. +0xD4 is set to 1 only by `SetPlayersCreature` 0x7381C0 (with +0x90 = +0x94 =
    FLT_MAX, so the next draw places it again), and `TotemStatue::Create` 0x737CC0 calls it through `AddToPlayer`
    right after the creation's `CallVirtualFunctionsForCreation` sets +0xD4 to 0: a new totem's first re-place is quiet
    too;
  - the second totem: `SetDrawWithGlobalAlpha` (LH3DObject Flags1 0x80) and the colour's alpha byte 0x80, then
    `Game3DObject::AddForDrawing` with no game object (so the hand never picks it and the help's view never sees it);
    the totem meshes are not Z-sorted, so it is drawn at once, in the global-alpha render mode (mode 1: source alpha
    over inverse source alpha, Z write on), before the solid totem. It keeps the colour the solid had on its last draw:
    the second totem's draw (0x738B15..0x738C8F) writes only its position and the alpha 0x80, and the solid's draw
    (0x738E15..0x738E9E) then sets its own +0x4C / +0x50 by `GetColorAndSpecular` 0x801C90 at its origin (the colour's
    alpha 0xFF, 0x801F7B) and `GetFogValue` 0x7FEB30 at that origin's depth;
  - the whole draw, the Zoomer's advance apart, needs the totem built and alive (0x738A80..0x738AA3: +0x58 & 2 clear
    and +0x5C >= 1, life +0x48 > 0; a NaN fails): an undrawn totem shows no plinth or icon and gives the hand no hit.
    In the shipped game only a script's HEALTH or BUILT_PERCENTAGE can make it fail.
- **Script**: HEIGHT get on a totem is `GetWorshipSpeed` 0x738260 = `fld [this + 0x80]`, the share set (0..1), not a
  height; HEIGHT set is the town's `SetWorshipPercentage` (the totem's own without a town), then +0x88 =
  `GetWorshipSpeed`, with no clamp ([the creature](../creature/overview.md)'s property table). `GET_TOTEM_STATUE`
  (`GScript::GetTotemStatue` 0x6F3390, 1 in, 1 out) pops an object and, when it is a **town centre** (vt +0x1E0
  `IsTownCentre`) with a totem (+0xCC), pushes `AddScriptGameThing(totem, 0)` as an object (type 4); otherwise it
  pushes 0 (confirmed: xref immediates). There is no error message, and it takes the centre, not the town.
  - `Town::SetWorshipPercentage` 0x73C060 itself, with a worship site and a totem, also sets the totem's +0x88 =
    `GetWorshipSpeed` after the totem's share (0x73C0A1..0x73C0BE), so a script's `HEIGHT` set and the let go store it
    twice.
  - The base scripts' users: `challenge.chl`'s `LearnWorshipping` (`GET_TOTEM_STATUE` of the Norse town centre, then
    `SET_PROPERTY(HEIGHT, totem, 0.5)` in its lesson) and `CheckDyingWorshippers` (a `GET_PROPERTY(HEIGHT)` of that
    totem in its loop), both run by `LandControl2`; `LookAfterTotem` / `LookAfterTotems`, run by `LandControl4`, read
    and set the `HEIGHT` of the two objects `SetupUndeadVillage` creates as the script's type 34 (`TOTEM`, the puzzle
    totems, +0xE8: [the puzzle totem](puzzle-totem.md)), not of village totems. The `Land*.txt` files use none of them.
  - `HEALTH` and `BUILT_PERCENTAGE` on a totem (v1.42 exe). The `SET_PROPERTY` worker 0x70E820 (table 0x70F2BC by
    property - 1) and the `GET_PROPERTY` native 0x70DAE0 (table 0x70E78C) treat the totem as any `Object` /
    `MultiMapFixed`; `TotemStatue` overrides none of the slots they call:
    - `HEALTH` set (0x70E867): cast to `Object`, vt +0x5B0 = `Object::SetLife` 0x63A140: life (+0x48) = the value, **no
      clamp** (negative and above 1 are kept). It refuses a value <= 0.01, or no number (`fcomp`, `test ah,0x41`), only
      when Flags +0x24 has 0x4000 (indestructible), or 0x200 (in a script) while the help system's widescreen (+0x45E8)
      and its control (+0x45EC) are both on. Nothing else: no draw, no death, no town.
    - `HEALTH` get (0x70DB85): vt +0x11C = `Object::GetLife` 0x402600 = +0x48, as stored.
    - `BUILT_PERCENTAGE` set (0x70EC69): cast to `MultiMapFixed`, `SetPercentBuilt` 0x52EDD0: +0x5C = the value; below
      0 **or no number** -> 0 (`fcomp 0.0; test ah,1`, so a NaN is 0, not kept as the 1.20 C reads); then if +0x5C >=
      1, vt +0x8A8 = `MultiMapFixed::Built` 0x52EBB0 (the totem's slot): the building site +0x74 deleted if any; the
      new-building reaction only for a civic object (vt +0x8C0 = `MultiMapFixed::IsCivic` 0x422040 = 0 for a totem:
      none); with a 3D object (+0x40) and not a field (vt +0x210 = 0): `SetShadowOnTexture(1)` (vt +0x80 of the 3D
      object) and `RequestChangeTexture` 0x5E2FF0; then +0x58 &= ~2 (not under construction), |= 8 (constructed),
      +0x5C = 1.0. A value of 1 or more on a totem already built runs `Built` again.
    - After it the worker takes the object's town (vt +0x48 = `TotemStatue::GetTown` 0x738480, the town of its town
      centre +0x7C) and, with a town, when the object is not in its building list (`Town::IsInBuildingList` 0x73CD20,
      the sites at +0x790) and the value is below 1 or no number, `Town::AddBuildingSite` 0x73B8E0: **a totem set below
      whole gets a building site in its town** (its villagers come to build it, with `MultiMapFixed::BuildBy`).
    - `BUILT_PERCENTAGE` get (0x70E1A9): cast to `MultiMapFixed`, vt +0x880 = `GetPercentBuilt` 0x4014F0 = +0x5C; not
      a `MultiMapFixed`: 1.0.
    - The totem's `IsBuilt` (vt +0x890 = 0x5610B0): +0x58 & 2 clear and +0x5C >= 1 (no number: not built). A totem is
      made built (percent 1.0, +0x58 & 2 clear) and nothing sets +0x58 & 2 on it afterwards, so only the percent
      decides.
    - Users: none of the shipped scripts sets either on a totem (the two `GET_TOTEM_STATUE` sites of `challenge.chl`
      set only `HEIGHT` on the totem; the `Land*.txt` files name no totem).
- **When the creature goes**: the creature's destructor empties its player's +0xA4C with a plain write (0x475279), not
  through `GPlayer::SetCreature`, so `ReflectCreatureInTowns` 0x64B510 is not called there; the rest of the destructor
  is not read for it.
- The town's player setter fn_0073A8F0 also runs the other buildings' `AddToPlayer`, around fn_0073A9C0 and
  fn_0073A7D0.

## openblack

Status: **done** for the press, the hand state and pose, the grip test, the drag, the let go, the drawing and the
script properties, and the hand's size in the state (2026-10-24); not ported: the immersion, the creature's desire
update at the grip and the grey ValueSpinner. Port decisions (raffclar's village totem, his rules and where his grip
test and closure differ): [port decisions: magic](../project/port-decisions-magic.md).

- **The press** (2026-10-11): `hand_press::Branch::Totem`, in the chain with the Field and Hovered branches (the
  object under the hand): after `Hovered` and ScreenObject, before `TapOnly`, Creature and FishFarm, with the fact
  `cursorIsTotem` (the plinth or icon under the cursor, `VillageTotemSystem::TotemOf`); its tests are
  `ValidForLockedSelect`, `object_flags::IsCannotBePickedUp`, `IsGripped` and `InInfluence`, and it keeps the fish farm
  out as a creature does. `SendStartLockedSelect` / `SendEndLockedSelect` call the totem's network-unfriendly start and
  end, and the 0x1B / 0x1C handlers set and clear `gripped`. The turn's pass (`HandTurn.cpp`,
  `hand_creature::StepLockedObject`) lets go of a held totem that is gone, or whose hand's synced point has no
  influence, and ends the select and then the hand's action (2026-10-22): a totem held outside the influence is
  dropped with the share it was held at; for a totem that is gone the select's end sends neither the share nor the end
  packet. Tests: `test_hand_creature`. `InterfaceHandState` gives 18 while the start waits and 14
  once the totem is held, so the required hand state is 5; `HandSystem::Update` then calls `VillageTotemSystem::Slide`
  with the frame's mouse move up the screen (the game cursor's delta, y down, negated) against the window's height.
- **The pose** (2026-10-11): `HandSystem::Place` puts the hand in hand state 5 at
  `VillageTotemSystemInterface::GetHandHold` (x and z the totem's MapCoords in metres, y `GripY` of the icon's y and
  height, every frame; the hold radius `ecs::object::GetHoldRadius(icon, true)`), even with no land under the pointer,
  with the rotation `village_totem::HoldRotation(HandTiltAt(y), H)`; the up Zoomers are not stepped there, and the
  return to the normal state snaps them as its Enter. `HandSystem::Update` plays Chold_side at
  `GripTimeMs(GripClosure(radius), the clip's length)`, the field the holding state's `HoldTimeMs` reads. H is
  `HandSystem::_handHeading`, set in `UpdateHeading` as the original's +0x84
  (`static_cast<float>(affine::GetYAngle(ray))` under the same 0.01 gate); `HoldRotation` works b = H + π in float, cos
  b and sin b rounded to floats, and the extended sin b (a double) in rows y and z, each product rounded once. The M is
  openblack's `Transform` scale (`hand_detail::HandModelScale`). The ray itself is the original's, to the bit
  ([the hand's mouse ray](../hand/animation-and-placement.md#the-mouse-ray)). **Not ported**: CHand
  +0x486C / +0x4870 (the hand's screen point, read only while the camera sets +0x4874).
- **The cursor pin** (2026-10-22, fork issue #163): `HandSystem::PrepareState` pins the cursor's image at the mouse
  (`GameActionInterface::GetMousePosition`) as the hand enters state 5 and lets it go as it leaves it;
  `HandSystem::Update` moves it each drawn frame onto the hand's hold point through the camera's view and projection,
  `village_totem::CursorPin` keeping it on the screen and rounding it half to even, while the point is in front of the
  eye (⚠️ approximate: the original's test is the near plane, ours the eye). `HandSystemInterface::GetCursorPin` gives
  it. As in the original nothing draws a cursor image while the hand is shown (`Sdl2WindowingSystem` switches the
  system cursor off), so the pin shows only in the readout; test
  `VillageTotemHandHold.TheCursorsImageIsPinnedOnTheHandKeptOnTheScreen`. raffclar's `GameActionMap::PinCursor` (a pin
  where the mouse is, for his scoop) is not taken: the original pins on the hand, and only for the totem.
- **The grip test** (2026-10-11): the four tests are `village_totem::ValidForLockedSelect` over `GripFacts`, read by
  `VillageTotemSystem::ValidForLockedSelect` (the town's player `worship::town::OwnerOf`; `IsFunctional` as
  `IsAvailable`, `abodes::IsBuilt` and `GetPercentRepairedForNonFunctional` below the life; the heart of
  `citadel::Of(player)`; the town's `TownMagic::worshipSite`). openblack's
  `VillageTotemSystem::SetPlayersCreature` takes the neutral player for a totem with no town, as the original's `GetPlayer` does.
  `ValidForLockedSelect` refuses a town-less totem at the player test, the original at the town test: the same result
  (the "(pending)" comment at that test is out of date).
- **The grip, the drag and the let go** (2026-10-11): +0x88 and Flags & 0x10 are `components::VillageTotem`'s `held`
  and `gripped`; `StartLockedSelect` is 0x7385E0, `Slide` 0x7386A0 with `village_totem::HandSlide` and `SlideShare`,
  `EndLockedSelect` 0x738630 (packet 0x29 is `game_packets::Type::TotemShare`, the share in `data[0]`) and `LetGo` the
  0x29 handler. `VillageTotemSystem::FriendlyStartLockedSelect`, from the start packet's handler
  (`HandSystem::ApplyStartLockedSelect`) before `SetGripped`, makes the reaction through
  `ecs::effects::reactions::CreateReaction(totem, ReactToHandUsingTotem, player, stamp = true)`. The desire update is
  not ported: openblack's creature mind has no counterpart of `DivideIncreaseTimeTimeBy`. `LetGo`, with a town, a
  share above 0 and a town player other than the neutral one, makes that player's creature empathise
  (`CreatureMindSystemInterface::EmpathiseWithPlayer(owner, ToImpress, 0.5, the totem's place)`, only a creature that
  sees the place) before the town's share is set, as the 0x29 handler. The guidance's
  `StartTotemRaiseSound` / `EndTotemRaiseSound` stand for `StartRaiseTotemSFX` / `EndRaiseTotemSFX` (the share the hand
  holds); like the original they play nothing (the alignment class, a pure read, is not taken). The grip height's
  `ecs::object::GetHeight` is, for the icon, `ObjectHeight`: 2 × the mesh box's half height × the scale field, as
  `Object::GetHeight`.
- **AddToPlayer and the icon** (2026-10-11): `VillageTotemSystem::AddToPlayer` and `SetPlayersCreature`, with the
  rules `village_totem::FacingSite` (on the totem's and the site's MapCoords) and `village_totem::IconMesh` (the pack's
  626 meshes; 250 is `BuildingSpellHand`). They are called by `VillageTotemArchetype::Create` (the creation, while the
  component's `onCentre` is still false, which stands for the centre's +0xCC not yet set), by
  `town_belief::TakeOverTown` through `TownChangedPlayer` (the totems of the town's centres), and by
  `CreatureArchetype::Create`, `LOAD_CREATURE` and `CreatureMindSystem::SwapMinds` through `PlayersCreatureSet` (the
  totems of the player's towns' centres; the swap's for each player as it is given the other creature, the first
  creature's player first, none for a creature of no player, as `Creature::SwapMinds`; test
  `CreatureMindSystemTest.SwappedMindsSetEachPlayersTotemsAsThePlayerIsGivenTheOtherCreature`), and, for every totem,
  by `VillageTotemSystem::WorldLoaded` once a saved game's objects and players are made (`Game::LoadSavedGame`), as
  `ResolveLoad` 0x73894A; test `VillageTotemScriptTest.ALoadedWorldPutsEveryTotemsIconBack`. openblack's load makes
  no creature yet ([saving and loading](../engine/saved-games.md)), so a loaded totem wears the hand, as at its
  creation. The player's creature is openblack's primary creature (`PlayerSystemInterface::GetPrimaryCreature`), the
  one the player leads. The turn and the re-mesh refile the totem in the map cells
  (`map_cells::OnAnglesOrScaleChanged`), as the original's remove and insert.
- **The drawing**: the draw's first rise is `village_totem::ShownShares(...).solid` in
  `worship::percentage::UpdateTotems`. (2026-10-15) Every frame, past the built and alive test, the second totem's
  plinth and icon are two `SeeThrough` entities (`components::Alpha` 128/255, which the renderer draws unsorted in the
  global-alpha mode, as the original), made and moved after the walk over the totems and destroyed once the shares
  stand together, skipped by the hand's pick and deleted with the totem; the re-place (`village_totem::Replaces`)
  forces `helptext::k_ToolTipWorshipShare` with the held share as a percentage (`ShowsShareToolTip`: the local player's
  totem, not on the quiet re-place), and `SetPlayersCreature` sets `quietOnce` and `k_NeverPlaced`. The second totem is
  drawn among the objects just before the totem's own plinth and icon, as the original (`components::DrawnWith` and
  `graphics::object_order`, see [blended models drawn at once](../rendering/object-draw-list.md#blended-models-drawn-at-once-their-place-among-the-objects)).
  (2026-10-16) `RenderingSystem` keeps each plinth's and icon's `haze::LitAndHazed` of the frame for the next frame's
  copy, drawn as a set colour (the vertex light at its own matrix, no land light, no haze); the View menu's "Totem Copy
  Takes Solid Light" switch. Before the solid's first draw the copy is lit at its own place (the original's first
  colour is not read). (2026-10-16) `village_totem::IsDrawn`, and `UpdateTotems` puts `components::NotDrawn` on the
  plinth and the icon while it fails (taken off once it passes); the hand's pick skips an undrawn totem.
- **The script** (2026-10-11): `CHLApi`'s `GET_PROPERTY` / `SET_PROPERTY` `HEIGHT` on a totem go through
  `VillageTotemSystemInterface::GetShare` and `SetShareFromScript` (the totem test before the creature's height, as the
  original's order), and `GET_TOTEM_STATUE` through `TotemOfTownCentre` (`abodes::TypeOf` is the town centre test).
  `worship::percentage::SetWorshipPercentage` sets `held` as the original's town setter. (2026-10-16)
  `components::VillageTotem::percentBuilt` (1 at creation) is +0x5C; the under-construction bit is not kept, since it
  is never set on a totem. `SET_PROPERTY` `HEALTH` on a totem goes through
  `VillageTotemSystemInterface::SetLifeFromScript` (`life::SetLife`, as given), `BUILT_PERCENTAGE` through
  `SetBuiltPercentageFromScript` with `village_totem::PercentBuiltSetTo` (none below none or for no number, 1 from 1
  up), and the get through `GetBuiltPercentage`, before the Features' default of 1. `HEALTH` get was already the
  totem's `life::LifeOf`. `abodes::IsBuilt` of a totem is `village_totem::IsBuilt(percentBuilt)`, which the grip's
  `IsFunctional` reads.
- **Effects**: `effects::IsEffectReceiver` (`ECS/Effects/EffectValues.cpp`) answers no for a village totem, its plinth
  and its icon, as `TotemStatue`'s (the totem building's rule, a separate class, is tested by
  `RepairsTest.NoEffectReachesATeleportStoneASeedATotemOrAnEmptyPot`; no test pins the village totem's).

### Test hooks

- The View menu's "Totem Copy Takes Solid Light" switch.
- The Game Details overlay's "Cursor Image Pinned" line: where the cursor's image is pinned while the hand holds a
  totem, or "no".
- `test_village_totem`: the two property rules, and `VillageTotemSystem` on a registry of its own.
- The testbed's hand lines (`objects.village_totem_drag`) end in "holding the totem at share N" while the hand
  holds a village totem, N the share it holds it at. Their ", held" is the camera's cursor freeze, not the totem's
  hold.

## Pending

- **The hold** (hand state 5): not ported: `StartTapOrLockedSelect`'s 225 ms wait with the leash in the hand. (The
  hand's size in the state, Enter's `SetDistanceFromView(fn_0046DB70(totem))`, is taken:
  [the hand's scale](../hand/animation-and-placement.md#the-hands-scale-in-openblack).) fn_0074CAF0 in
  Enter; and the
  order of the totem's draw (which moves the icon) against `CHand::PrepareForDrawing` (openblack takes the icon's y as
  the placement finds it). H is only as exact as openblack's mouse ray, which comes from `Camera::RayFromEye` and
  `glm::normalize`, not from `Get3DPointFromScreen` less the eye normalised in float (queued fix F-94).
- **The grip and the let go**, not ported:
  - `StartImmersion(0xF)` at the grip and `StopImmersion(0xF)` at the end: openblack has no immersion effects yet;
  - the creature's `UpdateDesiresAfterObservingPlayerAction` at the grip (the desires' `DivideIncreaseTimeTimeBy`,
    table 0xC67EF0) is queued as a fix; the reaction 0x11 is ported;
  - the 0x29 handler's grey ValueSpinner (the floating numbers are in
    [the floating numbers](../world/buildings.md#the-floating-numbers-valuespinner); this one is not made yet);
  - `HEIGHT` on everything but a totem statue, a creature and a puzzle totem (`GetHeight` / `SetHeight`; the puzzle
    totem's is on [the puzzle totem](puzzle-totem.md));
- **Not read yet or open**:
  - whether the original's hand, and so its synced point (status +0xC8), stays at the totem while the hand holds it
    (openblack's does: the synced hand is the hand's place, at the totem's hold), which decides whether a slide can
    take the point out of the influence;
  - the vt +0x48 calls around the see-through draw (what they switch);
  - how the living take the reaction 0x11 (REACT_TO_HAND_USING_TOTEM): openblack's villagers and animals have no
    handling of their own for it;
  - a script's `BUILT_PERCENTAGE` below 1 on a totem: the original then gives it a building site in its town;
    openblack keeps only the percent (the abodes' setter skips the same building-list part). And `Built`'s
    `SetShadowOnTexture(1)` / `RequestChangeTexture` on the totem's 3D object, which may put a rebuilt totem in the
    baked land shadows ([the frame](../rendering/frame.md)): not ported;
  - `HEALTH` set on every object but a totem (openblack: only the totem), and `Object::SetLife`'s refusal of a life
    at or below 0.01 for the indestructible and in-script flags (`life::SetLife` has neither);
  - the second totem's light: openblack's copy takes the solid's last light (see openblack); the first frame's colour
    is not read;
  - whether anything re-meshes the totems when the player's creature goes (see the original); openblack re-meshes
    nothing as the creature goes;
  - the other buildings' `AddToPlayer` in the town's player setter fn_0073A8F0 (only the totem's is ported), and the
    order against fn_0073A9C0 and fn_0073A7D0 there.
