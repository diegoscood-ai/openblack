# Casting from the hand

## Summary

A miracle is cast from the hand that holds its seed. How depends on the seed's cast type: some arm on press and cast
on release after a sizing gesture (storm, fire, shields, flocks), some cast on press at the hand's point (forest,
heal, teleport, destroying lightning) and some apply every turn while held (food, wood, water, lightning). The hand
shows the miracle with its own particle effect, coloured power-up bands that fly to the wrist, the gesture trail, the
recognised gesture's sparkles and light sheet, and, for a tribe with tribal power, its name spinning round the hand.

openblack ports the casting paths, the hand's effect and bands, the utility effects and the light sheet faithfully.
The hand's glow is computed but not drawn. The colour pulse of the hand is written in the original but never reaches a
drawn frame, so openblack has nothing to draw for it.

## The original

### Casting from the hand, gestures and hand effects

What is below was read in the exe; what was not is stated. The gestures themselves are on
[Gestures](gestures.md#gestures-the-buffer-and-the-recogniser).

#### Casting from the hand

- `ActionPressedHolding` 0x5D1560 with a seed: over a valid object (in the influence) it applies to the object;
  otherwise, to the ground under the hand, which must be in the player's influence. Depending on the `castType`:
  - **HAND_GESTURE** (storm, fire, shields, flocks): it arms on press if it can be cast there
    (`ValidToApplyThisToMapCoord` 0x728720 = ready and `CanCast`); otherwise, `FailApply`. Arming
    (`BeginApplyOnRelease` fn_005D2730) resets the buffer with the current sample and starts the IN_GAME 3 loop
    `G_HandGesture_02`. It casts on release (states 8/9, 0x5D48D0).
  - **HAND_POSITION** (forest, heal, teleport, destroying lightning): it casts on press (`DropOnMapCoord`
    fn_005D1850).
  - **IN_HAND** (food, wood, water, lightning): state 10/11. While it is held, one apply per turn (0x5D4C10 /
    0x5D4D00); on release, `ApplyUnlockProcess` 0x728EB0.
- `SendApplyToMapCoord` 0x5D3340:
  - one packet per turn (`m_ApplySentTurn`);
  - with a pending circle, the point and the gesture are those of the circle;
  - **fn_00729AF0: a seed with `sizingGesture` needs that gesture in the packet**; otherwise, `FailApply`;
  - a power-up level being charged goes with the cast;
  - then `SpellSeed::ApplyThisToMapCoord` 0x728E20 (the magnitude is the size of the gesture) and the result
    (fn_005DA100): the seed stays in the hand if the spell is kept in it; otherwise it leaves (0x16) with the visual
    SUCEED_CAST (3).
- **The hand opens as the seed leaves.** `GInterface::ApplyChangesToHand` 0x5DA100 (the apply handlers' tail: the map
  point fn_005DA400 0x5DA4B6 after vt 0x728, the object fn_005DA1A0 0x5DA2A2) takes 0x16 to fn_005CED60 (0x5DA145)
  and 0x17 to fn_005DA0C0, which calls it first (0x5DA0C0). fn_005CED60 takes the first object out of the status's
  hand (fn_005DC1E0, 0x5CED6A) and then, for my interface with the status's hand empty (+0x90 == 0, 0x5CED8A),
  `CHand::ThrowObject` 0x46DDD0 (0x5CEDA5): CHand +0x48FC (lastReleased) = the seed, +0x4904 = 0. With nothing held
  the hand's required state is NORMAL, so at its next draw it blends (0.13 s) back to the normal state's clip (Cwiggle
  over the land). 1, 3, 5 and 0x18 do not reach ThrowObject (0x18 only fn_005DC1E0, 0x5DA185). There is no throw clip
  in `hndspec5.txt`: the opening is the change of state.
- **A seed a worship site or a dispenser takes**: the hand opens in the same turn, through the status's hand check
  rather than the apply's result. The worship totem's site (`ApplyToWorshipSite` 0x7289C0, from 0x728B77) adds the
  seed's chants to the site (+0xF0) and marks the seed `ToBeDeleted(0)` (vt 0xC, 0x7289E5); a spell icon (0x728B81..)
  does the same (0x728BE6), and a dispenser (fn_00728C80) makes its one-off seed, marks the seed (0x728CDC) and shows
  spot visual 0x1B. They return 3, 3 and 1, which `ApplyChangesToHand` leaves alone (0x5DA119, and 1 only
  fn_006E47C0). `GameThing::ToBeDeleted` 0x56FB70 sets +0xA bit 0, so `IsInteractable` (vt 0x190, 0x5701B0 →
  `IsAvailable` 0x401810) is 0. `GGame::ProcessGameInputs` runs the turn's packets (0x54C3F8) and then
  `GInterface::Process` (0x54C40D) → fn_005D2250 → fn_005CEBB0 (0x5D262F, every player's status) →
  `GInterfaceStatus::Process` → `ValidateHands` 0x5DC610 (0x5DC5E0): `GMagicHand::Validate` 0x5FB130 finds the seed
  not interactable and takes it out (`RemoveFromHand` 0x5FB0B0), `TidyHands`, and for my interface's status with
  CHand +0x4904 set, `CHand::ThrowObject` (0x5DC68C). The spell icon calls `ValidateHands` itself right after marking
  the seed (0x728BF4), so the hand lets go before `PutFullyChargedPowerUpSeedInHand` (0x728C35) puts the icon's own
  seed in it.
- **The apply to an object**, `SpellSeed::ApplyThisToObject` 0x728D10: `CanCast(object, player)` 0x729190
  (0x728D31); then a spell dispenser (`IsSpellDispenser`, fn_00728C80), a worship site (fn_00728A50,
  `ApplyToWorshipSite` 0x728B30) or a magic fireball (`ValidToApplyToMagicFireBall` 0x728A20, `ApplyToMagicFireBall`
  0x728AE0) take it; otherwise the object cast fn_00729690 (0x728DC7), then `IsSpellKeptInHand` (0x728DDA) and the
  SUCEED_CAST spot visual (0x728DFF). fn_00729690 is `SpellSeed::Cast` 0x729520 step for step (`GetMagicInfo`,
  `IsSpellCastInHand`, fn_0071FA10, `DoPreCastThings`, fn_00729110) but takes the object instead of a point, makes
  the spell with fn_005FB520 (0x7297A6; `Cast` calls fn_005FB490, `CastAtPos`, at 0x729636) and gives
  `DoPostCastThings` the object's position (object + 0x14). fn_005FB520 casts on the object (`InitWithObject`) when
  the seed's `castOnObject` is 1, else at its position ([Life cycle](spell-core.md#life-cycle)).
- `FailApply` fn_005D18F0: visual 4 (`SF_FailedApply`) at the point and `G_SpellCastFailure`: packet 0x2B (point, 4)
  (0x5D1914..0x5D1928) shows the fail spot visual at the next turn's start, the sample (0x25) plays at once.
- **The seed's own tests**: `SpellSeed::ApplyOnlyAfterRecSystem` 0x7286B0 is castType HAND_GESTURE;
  `SpellSeed::ValidForLockedApplyProcess` 0x728750 is `IsSpellCastInHand` 0x729820, castType IN_HAND;
  `SpellSeed::IsSpellKeptInHand` 0x729840; fn_00729AC0 gives the sizing gesture (CIRCLE for the storm and the
  shields); `SpellSeed::ValidToApplyThisToObject` 0x7286D0 is ready, the target not highlighted by the script (vt
  0x48C, 0 for `GameThingWithPos` 0x4023A0) and `CanCast(object)`; `SpellSeed::IsG3DObjectDrawnInHand` 0x728600 is
  `isSpellSeedDrawnInHand` == 1. A press over an object takes `GetCreatureToGiveTo`, then the action collide's object;
  `ValidAsInterfaceTarget` (Object 0x402840) is 1, giving to a creature needs a creature, and
  `InterfaceMustBeInInfluenceForInteraction` is 1 (0x4028A0). `IsSpellSeedReturnPoint` is 0 for an Object (0x402730);
  worship sites and dispensers answer 1.
- **What the apply packets carry**: `GInterfaceStatus::UpdateSpellInfo` 0x5DC8F0 builds the spell info from the
  synced status (packets 0x15 / 0x16 / 0x17): +0x00 the MapCoords +0x14 as a world point (0x5DC915..0x5DC945), +0x0C
  the hand +0xC8 (0x5DC8FC), +0x18 normalize(+0xBC − +0xB0) (0x5DC948), +0x24 the velocity +0x10C (0x5DC9F5).
  `SendApplyToObject` 0x5D30D0 (for every seed target, a return point too) and `SendApplyToMapCoord` both wait while
  an apply packet is unanswered (+0x444, `m_ApplySentTurn`, 0x5D3135 / 0x5D337C), test the gesture (fn_00729AF0,
  0x5D3162) and send packet 0x4D with CHand's throw block as it is (0x5D326B..0x5D3289 / 0x5D3565..0x5D3576, 0x5D35C8:
  the velocity +0x48C8, h sent negated, HandPos +0x48E0 and the angles +0x48EC, which in state 8 are `GetYXZ` of the
  held seed's matrix, 0x5B55A4..0x5B55BD). In the holding state `ObtainRequiredHandPosition` writes +0x48E0 as the
  held object's G3D translation +0x38 + 0.2 [0x8AB244] × the state's spring velocity +0x108 (0x5B553F..0x5B55A1;
  the held object +0x4904 → +0x40, 0x5B3CBA); without one, the hand's position. Then the object send adds 0x11 with
  the gesture; the map send a 0x16 (0x5D360D: the apply MapCoords and +0x48E0 into +0x14 / +0xA4, fn_005D2250's
  last-sent values untouched) with its ping index (0x5D35DF) and the 0x12 (0x5D362D). A hand demo's record is copied
  in the same frame before its message (fn_005DAEE0 0x5DB0A5..0x5DB0B6).
- **The map apply's handler**, GInterface 0x5DA400 (packet 0x12): the held seed `IsAvailable`,
  `ValidToApplyThisToMapCoord`, `ApplyThisToMapCoord` 0x728E20, whose `CreateSpotVisual(pos, 3, 1.0, spell)` (0x728E8B)
  passes the effect's strength 1.0; the duration is the entry's own (+0x44, 0x63E55F). The IN_HAND repeat (0x5D4C10 /
  0x5D4D00) applies again once a turn on the land only when the point is `InBounds` (0x5D4C82) and
  `ValidToApplyThisToMapCoord` (vt 0x724, 0x5D4C97), with no `FailApply` otherwise.
- **A forced drop**, `GInterface::ForceDropHeld` 0x5D4350: packet 0x4D with zero velocity (0x5D4361..0x5D4389) and
  the held object's matrix position and YXZ angles (0x5D4393..0x5D43C3); with nothing held (0x5D4391 → 0x5D43CA) the
  status's +0x44.. is copied and the velocity and angular part zeroed (0x5D43EF..0x5D4417), so HandPos and the angles
  survive; then 0x1D with no fields (0x5D442C). Its handler 0x5DA8F0: the hand holds (+0x90), its first object
  `IsAvailable` (vt 0x2C, 0x5DA90F), then its `ThrowObjectFromHand(status, 1)` (vt 0x758) and `ApplyChangesToHand`
  (0x5DA934). For a seed, `SpellSeed::ThrowObjectFromHand` 0x72ACD0 (forced) → `ApplyToWorshipSite` 0x729A80: the
  charge goes back to the icon's site, and the seed is deleted either way (3).
- **A seed leaving the hand**, `InterfaceSetOutMagicHand` 0x728940: the status's last seed type (fn_005DCA20) and the
  icon's `CancelCharge` 0x77F9A0; for the local hand, fn_0046E890 (the in-hand effect), `PHandFX` `SetPULevel(0, 0)`
  and `StopTribalPowerRing`. A seed may be shaken from the hand: `Object::ValidToShakeFromHand` 0x636AA0 is 1, and
  SpellSeed does not override it.
- Hold parameters (0x728640..0x728680): MAGIC until the seed is ready (`Cwiggle` at half length) and then its
  `holdType`; radius `holdRadius × scale`, plus `holdLoweringMultiplier`. The seed's mesh is only drawn in the hand
  with `isSpellSeedDrawnInHand`: fire, lightning, heal and storm are only their effect in the hand.
- **Feeding a fireball in flight** with a fire seed in the hand is the start of `ProcessPowerUpSystem`
  (0x5CF360..0x5CF3AA), with the cursor over the ball and no button: a hover, not a press
  ([the feed](fireball-and-lightning.md#fireball-magic_type-1-3-seed-2-fire)).

#### The hand

- **Effect in the hand** (CHand fn_0046E7B0 / `DrawSpellInHand` 0x46E680):
  - it is the level's `particleTypeInHand` (fn_007285E0), and `SetPowerUp` creates it again;
  - it is advanced every frame by `max(1, g_game_time_inc)` ms, strength = that of the seed's PSys
    (`SpellSeed::GetPSysPower` 0x7298F0), magnitude = the hand's scale, and only with the seed ready;
  - `UR_FollowLocalHand` 0x69A6A0 and `UR_FollowCastPosn` 0x69FE30 carry it to the hand;
  - effects that advance per frame are drawn where the last step left them, without per-turn interpolation;
  - its end is fn_0046E780; ReleaseInHandEffect is CHand fn_0046E890.
- **PHandFX** (ctor 0x68CB10, `Draw` 0x68D0C0, `Band::Draw` 0x68D6D0): `Power_Up_Band.L3d` bands at scale 10 on the
  root bone, at 10 + 40·index, spinning at (1 + 0.2·index)·12 rad/s.
  - **Matrix** (`Band::Draw` 0x68D8BB..0x68D9EA): the local one is 10·I with the translation (0, 0, +0x18 +
    index·+0x1C) (0x68D900..0x68D909), that is, along the **root bone's own Z axis** (the forearm). Only once it has
    arrived (f ≥ 1, 0x68D90D) each row rotates its (x, y) by the angle +0x20 around that Z (0x68D922..0x68D9DB:
    (x, y) → (c x + s y, c y − s x), c stored as a float at 0x68D929, s on the stack). Afterwards fn_007FAFF0
    0x68D9EA = local × bone (rows; in glm bone · local). The bone is the first 0x30 bytes of the matrix pointed to by
    CHand +0x47F0 (copied at 0x68D0F2..0x68D100; `PrepareForDrawing` 0x46CAE5 copies the same one into the matrix of
    the hand object). Result: a bracelet that goes around the wrist and spins about the forearm's axis.
  - Permanent: `SetPULevel(pu + 1, 1)` from `SpellSeed::SetPowerUp` 0x729BFC..0x729BFE (pu = POWER_UP_TYPE: −1 no
    power-up, 0 = PU1, 1 = PU2), so 0 / 1 / 2 rings (maximum 5); they start at 2.4 s; alpha 20→130 in 0.85 s, with
    matrix lerp. They fly from in front of the camera to the root bone of the hand (the wrist).
  - Colour (`Band::Draw` 0x68D849..0x68D8B1, on every draw): +0x4C = `GetPlayerColour` 0x64D800 of the local player
    (g_game +0x205A59) with the band's alpha; +0x50 = per-channel lerp of the ctor's colours +0x34 / +0x38
    (fn_0068CA30, args 8 and 9), 0 in all callers. A single draw per band (0x68DD46 vt+0x104). The user's
    recollection: a translucent red ring reaches the wrist when picking up a miracle (the exe confirms it: player 1's
    red).
  - Temporary: 5 on gaining a level, 0.1 s between them; alpha 20→120, with slerp.
  - Charge ones: duration lerp(3.5; 1; c), one every lerp(6; 0.3; c) s.
  - They arrive flying from 4 m in front of the camera, at half scale (the matrix 0xEA1CF8..0xEA1D27 is the
    camera-to-world matrix: the inverse of `g_world_to_camera` 0xEA1D28, set by `LHMatrix::SetInverse` at each camera
    update, 0x819F2B in `UpdateCamera` and 0x81A743 in fn_00819F50, identity at `RenderInitialization`
    0x818E15..0x818E83; its +0x24..+0x2C, 0xEA1D1C..0xEA1D24, is the camera's position; read by `PHandFX::Band::Draw`
    0x68DA3C, `StartTribalPowerRing` 0x68DF2B and fn_008296D0).
  - `AddSpellToHandVisuals` plays `G_SpellPowerUpBand`; the shake, `G_ShakeHand_01` and a band that leaves.
- `PHandFX` loads the same `Power_Up_Band.L3d` as the icon bands (`fn_0068CC70` 0x68CC7D..0x68CC99), with
  MaterialProperties {additive 1, Z 0, two-sided 1, change 1, alpha 1}, so the band becomes mode 13 (SRCALPHA / ONE,
  no Z write, fn_0082ECD0).
- Band: 0x48 bytes, vtable 0x936B1C, ctor fn_0068CA30; only `LH3DObject::Create` 0x68CA98 and vt+0xF4 (no vt+0x78),
  with +4 = 0x10009 (0x816537): it neither receives nor casts a projected shadow.
- Bands: permanent fn_0068CCC0 (index count + 1, at the head); temporary fn_0068CD30 (index count); charge
  fn_0068CDA0 (alpha lerp(3, 5, c) → lerp(15, 50, c)).
- fn_0068D000 removes the newest; vt 0 RemoveAllPermBands 0x68D060; vt 8 AddSpellToHandVisuals 0x68DE20; vt 0xC
  SetPULevel 0x68DDA0; delayed start fn_0068DE90.
- `PHandFX::DrawHandFX` 0x68DD60 (vtable slot 0x10 of PHandFX, no direct caller) → `PHandFX::Draw` 0x68D0C0 every
  frame with g_game_time_inc × 0.001 (0 while g_game +0x14 bit 2, the pause, is set). Interpolation:
  fn_0044CF90 / fn_0044E9F0.
- DoRemoveFromHandVisual: the band returns with alpha 5 → 50 reversed over 1 s (its sound 0x77 is in the audio
  pages).
- **The hand glow**: a second pass with additive `S_Hand_Flow`, player colour, alpha 0.8, 8×4 atlas at −20 frames/s,
  with two textures (colour and `S_Hand_Flowa`). The pass itself is read below; when in the frame it runs is not
  traced (see Pending).
- **The glow pass, read** (`PHandFX::Draw` 0x68D0C0, 0x68D264..0x68D60D). It is drawn by `PHandFX::Draw` itself, as
  part of the hand's effects:
  - only while the glow's alpha (PHandFX +0x54) is above 0.01 (0x68D264..0x68D272);
  - its material is made once, `CreateMaterial(13, fn_0057DBE0("S_Hand_Flow.raw"))`, mode 13 (additive, no depth
    write), kept at +0x68 with the texture at +0x64 (0x68D278..0x68D295);
  - the frame advances by the frame's seconds times the rate (+0x5C) and wraps through twice the cell count (+0x60,
    fmod); the cell is the frame rounded (`fistp`) modulo the count; its column is the cell modulo 8 and its row the
    cell over 8, each times 0.125 (0x68D298..0x68D374);
  - the hand's own 3D object (CHand +0x482C) is drawn again: its mesh (vt +0xF8) has every vertex's UV saved in a
    static buffer, then set to u × 0.125 + column, v × 0.125 + row (0x68D3BE..0x68D4D3);
  - the object's colour (+0x4C) is set to the alpha truncated to a byte (`__ftol`, 0.8 × 255 = 204) over the player's
    colour, `GetPlayerColour` & 0xFFFFFF (0x68D4D6..0x68D508);
  - the material is made current, tiling on or off by its flag, and the cull mode (D3DRS 22) none for a two-sided
    material, counter-clockwise otherwise (0x68D50B..0x68D569);
  - the object is drawn (vt +0x108, 0x68D578), then its colour (0x68D58E) and its UVs (0x68D591..0x68D60D) are put
    back.

  So the glow is the hand's own object, posed and morphed as the hand is, lit as the hand's draw lights it, in the
  flow's cell, additive at 204 of 255 in the player's colour, writing no depth.

#### The tribal power column

The tribe's name spinning round the hand. It only exists for a tribe whose tribal power is above 1
(`GetTribalPowerTribe` 0x5FB710 through `SpellSeed::GetTribalPowerTribe` 0x729AD0: the seed's player and its magic
info). No tribe gains any in the vanilla game (every `TribalPower[t]` stays 1), so it never shows there.

- **Start**: `SpellSeed::InterfaceSetInMagicHand` 0x7288EE → `PHandFX::StartTribalPowerRing` 0x68DF20. It first
  removes the old ring (vt +0x1C), then `PowerSpinRunner::Create` 0x66F730 with the tribe's text, the camera matrix's
  position 0xEA1D1C (the matrix 0xEA1CF8) as the first sample, the local player's colour (g_game +0x205A59,
  `GetPlayerColour` 0x64D800) | 0xFF000000, and held = 1 (+0x9BC). The ring pointer is PHandFX +8.
- **The text**: fn_0068DEB0 returns help text 0x4D8 + tribe (tribe 0..8, else none).
- **Cast**: `SpellSeed::DoPostCastThings` 0x729260, first of all: when the seed is my interface's
  (`GetInterfaceStatus` == `MyInterface` +0x39C), the hand's FX object (`MyInterface` +0x3A0 → +0x4948) vt +0x20 =
  `PHandFX::ReleaseOrCreateTribalPowerRing` 0x68DFC0; otherwise `CreateTribalPowerColumn` 0x68DEF0 at the cast's
  MapCoords (x, z × 1/6553.6, **y = the altitude field as it is**, not added to the land height) in the seed player's
  colour. Then, either way, `PlaySoundEffect` sample 0x1B + tribe, mode 2, not 3D, bank 9 (SpellDialogue).
- `ReleaseOrCreateTribalPowerRing`: with a ring, held = 0, age = 0, its position (+0x9C0) = the hand's position
  (`MyInterface` +0x3A0 → +0x78), and PHandFX +8 = 0; without one, `CreateTribalPowerColumn` there in the local
  player's colour.
- **`PowerSpinRunner::Update` 0x66F890** (age +0x9B8 += dt): held, the ring's centre is the hand matrix's position −
  its z row × 0.5, its axis the z row; under 1 s of age it flies in from `g_camera` 0xEA1DB8: centre += (camera −
  centre)(1 − age²), axis += (unit(focus − camera) − z)(1 − age²); trail +0x94 = max(1 − 0.5 age, 0); hang axis +0x90
  = 2; radius sin(0.1 age) × 0.5 + 1.3 (doubles), angle = age, alpha 255; `PowerSpin::Update` 0x66FD80 with 20 × dt
  samples. Released: hang axis 1, trail 0.8, alpha 255 up to 3 s then 255 − (age − 3) × 256 (≥ 0), position + (0,
  age³, 0), radius age × the same swell, angle 1.4 age², axis (0, −1, 0); it deletes itself once age > 4.8 s.
- `PowerSpin::Init` 0x66FCB0: the text copied (wcscpy), +0x88 its length, hang axis 2, trail 0.2, one sample of the
  point with axis (0, −1, 0), radius, angle and alpha 0. `Draw` 0x66FE80 lays each letter where the ring was `i ×
  trail` samples back (64 kept), turned back by `i × 2π / length`, a letter 0.6 × the radius high.
- **The caller** (bw1-decomp `src/Black/Game.cpp` 2355..2365, `GGame` frame draw): in the 3D part of the frame, when
  g_game +0x205A28 ≠ 3, after `ProcessFade` / `Temple::UpdateFade` and `help_system->Draw3D()` (and `ClearLight` when
  the land was drawn), it walks the list `PowerSpinRunner::First` (0xD4DE74, linked at +0x9B4). Each runner gets
  `Update(MyInterface()->hand->DynamicShadow->matrix, (int)g_game_time_inc × 0.001)`: the hand's 3D object's matrix
  (the Morphable field bw1-decomp names `DynamicShadow`) and the game step in seconds. A runner that is still alive,
  while `LH3DRender::g_started_frame`, is queued in the Z-sorter as **its own Z object** with `PowerSpin::Draw` as the
  callback, keyed `GetValueForZSorter(Position)` (x² + y² + z² from `g_camera`, Position +0x9A8 written by
  `PowerSpin::Update`; 🔍 inferred its `pos` argument, the ring's centre). Then `GCameraEditor::Draw3DPart`, and after
  it `InfluenceCircle::Draw` and `FinishFrame` (the drain). `GGame::ClearMap` and `GGame::Close` delete every runner.

#### Utility effects

- **The trail** (PT 48 `SF_GestureChain`) is active when the game is waiting for a gesture: icon seed charging, seed
  with m_Held & 8, selection open with the hand free, or seed with a circle. It goes in the hand, with magnitude
  `hand scale × f(distance)` ({0, 50, 500, 1500} → {0.2; 1; 1; 1.5}). Its rules `ZR_ChainGesture` 0x68A080
  (emission fn_0068A330) and `CreateRuleMakeChain` 0x69FD10, and the ribbon is drawn with the `ParticleChainCreator`.
  Colour: fn_00671110 creates it with `PSysInterface::Create` and does `SetPlayer` (vt 0x20) on it with the local
  player (g_game +0x205A59, 0x671172..0x671197); `ParticleChainCreator0` of `SF_GestureChain` has `UsePlayerColor 1`
  (white 255 × the player's colour, alpha 10), so the trail comes out in the player's colour (red for player 1).
  fn_00671260 does the same with PT 35 (0x6712CD..0x6712EA); the selection (fn_006711D0) gets no player.
- **The selection** (PT 28 `SF_SpellSelection`), while it is open.
- **The recognised gesture** (`fn_00689790` from `Success(1)`; PT 35 `SF_Gesture`; `UR_GesturingRecognised`
  0x6884F0 / 0x688910):
  - The record (0x48 bytes, list 0xD4EB10) carries the stroke (the terrain points of the whole buffer) and the ideal
    shape of the gesture (`PathSymbol<n>.cam`, or the circle's) placed over the pixel box of what was matched:
    - the box keeps the centre and divides its half sizes by those of the shape (fn_0068C140);
    - each point goes to the terrain under its pixel, at its altitude, or at 400 m along the ray (fn_00689F20);
      - fn_00689F20 is called at 0x689AE3 from fn_00689790 (which symbols.txt wrongly names
        `GJMesh::GetBoundingBox`), itself from `Success` fn_005CE420 at 0x5CE42C. The pixel is (ftol(((box[2] −
        box[0]) + 1)·p.x + box[0]), ftol(((box[3] − box[1]) + 1)·p.z + box[1])) (0x689F28..0x689F57). The land is
        fn_00800C30 at 0x689F72 with the shift off and depth 0.0 (0x689F60 / 0x689F62): `RayCast` from `g_camera` to
        the pixel's near point, the land, else the sea within 7500; no sea beyond it and no `KeptInReach`. On a hit y
        = `GetAltitude` at ftol((x·65536)·0.1), ftol((z·65536)·0.1) (0x689F7B..0x689FC2); else fn_0074CAF0(pixel,
        out, 400.0f) (0x689FCD).
    - if on the ground the shape comes out **more than twice as deep as it is wide** (camera axes in the
      horizontal), it is squashed vertically ×0.75 and this is repeated, 15 times at most;
    - the ideal one is resampled to as many points as the stroke has.
  - The rule takes one record per step: one atom (PCreator) and **IN_GAME 36 `G_SpellGestureRecognise`**. In its
    subcollection it places `NumAtoms` (234) sprites in the player's colour, with scale × (length of the ideal / 100):
    - the ideal approaches the camera until that scale rises (at most to half distance);
    - each sprite goes from the stroke to the ideal (t over `TimeToIdeal`, blended with smoothstep by `InterpGain`)
      and lights up from the ends (alpha `t × MaxAlpha`);
    - it jitters with shuffled-phase value noise, which fades out after `DispersalTime`. The noise is
      `Noise::VSNoise1To1` 0x590BB0: Ebert's lattice, permutation table 0xBEFDBC and Catmull-Rom spline 0x590010;
    - the collection pulses from `CollectionAlphaPulse` to 0 between 2.4 and 4.5 s, and the atom dies at `DieAge`
      (7 s);
    - LH3D's `LightSheet` (50 points on the ideal, height scale × 9, strength 1 − (2f − 1)² fed in at its first
      point; [the light sheet](#the-light-sheet-as-read-from-the-exe)) starts on the record's first pass in the
      colour of the record's player (AtomData +0xC0), moves on every frame, and goes with its atom at `DieAge`;
    - the colour pulse of the hand, which no frame shows ([the hand's pulse](#the-hands-pulse)).
  - The noise lattice: 256 × `1 − GameFloatRand(2)` (fn_00590DF0) from seed 0, the same values in every game. It is
    filled from `PSysGlobal::InitializeOneTimeOnly` 0x68F75C (fsubr [0x8AA390] at 0x590E10); its 256 draws run before
    `GGame::Init` re-seeds (0x54F4AF), so they do not move Init's 0x88F89F stream. `Noise::VLattice` 0x590EB0 reads a
    value through the permutation table 0xBEFDBC.
- PSysUtilityPSys 0xD4E0E8 (0x40 bytes, made by `PSysGlobal::InitializeOneTimeOnly` 0x68F779): +0 the trail (48), +4
  its "active", +8 the recognised sparkles (35), +0xC SF_OnFire (37), +0x10 the exploded meshes' SF_ExplodeObject,
  +0x14 SF_LightningStrike (61), +0x18 SF_ManaPathNew (22), +0x1C SF_BeliefSprite (24), +0x20 the selection (28),
  +0x24 its "active".
- fn_006721B0 (from `PSysGlobal::GameLoopEnd` 0x68F5C9, after the exploded meshes' slot +0x10), once a turn: the
  slots +0x0C, +0x18, +0x1C, +0x08 and +0x14, in that order, each made when missing (`PSysInterface::Create(NULL,
  type, 0, 0, 1.0, NET)`), stepped with an empty ProcessInfo (power 1, enabled) and the turn's ms ([0xD01A38], vt
  0xFC `Process_`), and dropped when it returns 5 (made again next turn). The per-slot functions: fn_006717F0,
  fn_00671740, fn_00671B40, fn_00671CD0, fn_00672100, fn_006719E0.
- The slot gates [0xC029FC] (+0x0C), [0xC02A00] (+0x18, +0x1C) and [0xC02A04] (+0x08) are 1 in .data and never
  written; NET_GAME_TYPE is 1 for +0x0C (0x6716EE) and +0x14 (0x67198E), 0 for the others.
- The trail's magnitude table is 0xC0213C / 0xC0214C. Its condition is fn_00671DA0 (misnamed
  `GInterface::InterfaceActionHandObjectApplyToPos`, every frame): a seed in the hand with m_Held & 8 (fn_005CF200),
  the selection open with the hand ready (fn_005CF1C0), a circle-sized seed in the hand (fn_00729AC0 == 4),
  `m_Buttons` bit 0x02 (no setter found) and byte [0xD17D10] (unknown).
- SF_Gesture (35) is stepped once a turn (fn_00672100) and drawn every frame with the turn's fraction by `Draw_(1)`
  (fn_00671DA0 0x671DD3); the original draws it only while MyInterface +0x3A0 != 0.
- The gesture records: list 0xD4EB10, count 0xD4EB18; the rule takes the newest one per step; the pixel box of the
  matched samples is 0xD4EB00; the gesture's shape is fn_0068C650.
- The camera's forward (0xEA1DD4) projected on the ground, (1, 0, 0) when it looks straight down; the 400 m point
  along the ray is fn_0074CAF0. The squash constants: 0.75 [0xC02650] at 0x689C20, 15 [0xC0264C] at 0x689C26, ratio 2
  [0x8AB478] at 0x689C37.
- **A faked gesture** (`PSysGlobal::FakeGestureOnLandscape` 0x68F510 → `UR_GesturingRecognised::FakeGestureOnLandscape`
  0x689440, read in the frozen v1.42 exe): its callers are `Reward::InterfaceTap` (0x6E6126, 0x6E632C), `PLAY_GESTURE`
  (`GScript::PlayGesture` 0x6FC090) and the computer hands' gesture step (sub_654B50). Nothing for gesture 0. The
  shape (fn_0068C650) laid point by point (fn_0068C2F0): x = shape x × size + corner x, z = shape z × size + corner z,
  and y the land's altitude at the map point `LH3DMapCoords(x, z)` (0x4427B0: ftol((m × 65536) × 0.1)) by
  `LH3DIsland::GetAltitude` 0x803090. The record (0x48 bytes): its status is the one given, else `MyInterface`'s
  (0x689575), its point +0x3C the corner; the stroke and the ideal both grow to 0x50 = 80 points (fn_0068C6E0 /
  fn_0068C780) and take, for i in 0..79, the point at i × (1 / 79) through the laid points: the last one when that is
  1.0, else at = (n − 1) × along, index = ftol(at), laid[index] + (laid[index + 1] − laid[index]) × (at − index)
  (0x689622..0x6896F6); then the record goes to the end of the list (0xD4EB10). Every step is on the game thread at
  24 bits ([floating point](../engine/floating-point.md#the-games-fpu-runs-at-24-bits)), so each product and sum rounds
  to a float.
- `UR_GesturingRecognised`: AtomData 0xC8 bytes (fn_00688140); the atom data is the channel owner of another
  interface's recognition sound (0x688643); [0xD4EB60] is never written, so the scale is always the ideal's length /
  100. In the sparkle group (none in SF_Gesture) the atom runs along the ideal once every 2 s and fades out over its
  last 2 s (age / 2 [0xC02648] at 0x68872D, < 2 at 0x68877E, × 127.5 [0x92B6EC] at 0x68878B). The wiggle: x and z by
  the ideal's box, y upwards only, phase offsets +0.3 [0x8AB23C] (0x6892D3) and +0.7 [0x8AB238] (0x689308). The pulse
  start and length are fn_0068DE90 / fn_0068DEA0; the LightSheet is fn_0083E710 with +0x18 = 0.03, its shift period:
  the light sheet's strength moves one point along it every 0.03 s.
- `ZR_ChainGesture` (AtomData 0x48, fn_0068A290): while the effect is enabled one head atom at the gesture position
  trails a chain whose NextGroups joints are a shift register, a new joint every MinEmitDist of movement (at most
  count / DieAge joints per second); when the effect stops being enabled the head stops emitting and goes 5 s later.
  `CreateRuleMakeChain` makes NumAtoms joints of PCreator, once.

#### The light sheet, as read from the exe

Read in runblack.exe, names from bw1-decomp `config/BW1W120/symbols.txt` (bw1-decomp's `LightSheet.h` is only the
layout; the bodies are not decompiled). It is LH3D's general sheet, not the gesture's own: its owners are the
recognised gesture's AtomData (+0x2C), the arena's `GLightSheet` (drawn during a fight, `GLightSheet::Draw`
0x4246C6), the spell icon's charge (`SpellIcon::TChargingData::Draw` 0x7267A0, its tail 0x726B95), the worship totem
(`WorshipTotem::TChargingData::Draw` 0x780EC0) and the camera's force field (`CameraModeNew3::ForceField`
[0xC5E150], drawn directly at 0x54E198 with `ForceFieldMaterial` [0xC5B0FC]). Lightning does not use it.

- **Layout**: +0 count n, +4..+0xC middle, +0x10 spread, +0x14 height, +0x18 shift period, +0x1C slide speed, +0x20
  points (12 bytes each), +0x24 strengths, +0x28 heights (the wave), +0x2C colour, +0x30 time, +0x34 slide, +0x38
  strength fed in, +0x3C time since the last shift, +0x40 vertex colours (diffuse and specular, 8 bytes), +0x44
  positions, +0x48 indices, +0x4C UVs, +0x50 vertex count 3n, +0x54 index count 12(n − 1), +0x58 material, +0x5C
  force-field mode, +0x60 force-field target, +0x64 force-field accumulator.
- **Constructor** 0x83E690: zeroes only the pointers (+0x20, +0x24, +0x28, +0x40, +0x44, +0x48, +0x4C).
- **`Init(n)`** 0x83E710: allocates the arrays, zeroes the points, strengths, heights, time, slide, fed strength and
  shift timer, builds the index list, and sets the defaults +0x10 = 1.1 (0x3F8CCCCD), +0x14 = 10, +0x18 = 0.03
  ([0x936774]), +0x1C = 1.0, +0x5C = +0x64 = 0. It never writes the middle (+4..+0xC).
- **The gesture's sheet** (`UR_GesturingRecognised::ModifyAtomCollection` 0x6884F0, once per atom, flag AtomData
  +0x28, 0x68880C..0x6888A1): `Init(50)`; point i at the ideal's parameter i × float 1/49 ([0x93689C], fn_00687E30 on
  record +0x1C); colour +0x2C = AtomData +0xC0 & 0xFFFFFF, where +0xC0 is `GPlayer::GetPlayerColour` of the record's
  object's player (0x688B2D..0x688B42, in ModifySubCollection); height +0x14 = AtomData +0xA4 (the ideal's length ×
  0.01, 0x688973..0x68897E; 1.0 from the constructor) × `LightSheetHeightScale` (0x688895 `fmul [ecx+0x2c]`); +0x18 =
  0.03 again. +0x18 is the **shift period** in seconds, not a width.
- **Every turn** (same function, 0x6888A4..0x6888F0): f = clamp(age / `LightSheetDieAge`, 0, 1) (0x6888AF `fdiv
  [edx+0x40]`), and the fed strength +0x38 = 1 − (2f − 1)². It is a **strength**, not an alpha: it goes in at point 0
  and travels along the sheet.
- **The properties** (`UR_GesturingRecognised::DefineProperties` 0x6B0560, `AddFloatProperty` 0x584520): +0x2C
  `LightSheetHeightScale` (0x6B058A, editor range 0..10), +0x3C `DieAge` (0x6B05E4, 0..5), +0x40 `LightSheetDieAge`
  (0x6B05FA, 0..5). The ranges are editor limits, not clamps. SF_Gesture's `UR_GesturingRecognised0` sets 9, 7 and 4.
  The atom dies after `DieAge` (0x688682 `fcomp [ebp+0x3c]`), and the sheet is freed with its AtomData (fn_00688200,
  list unlink, fn_0083E6B0).
- **Every frame** (`PSysGlobal::DrawLoop` 0x68F5E0, gated by [0xC029F4] = 1, → fn_006883C0 at 0x68F611, which walks
  the AtomData list [0xD4EB28] (next at +0x24, newest first) → fn_00688320 per AtomData): dt = `g_game_time_inc` ×
  0.001, or 0 when the game is paused (g_game +0x14 & 4); then `Update(dt)`, `GlobalTextures::GetLightSheetMaterial`
  0x57D440 and, when `g_started_frame` [0xECA644] is set, one `LH3DZSorter::NewZObject` 0x83F310 (callback 0x4246E0,
  flag 0) at 0x6883AB.
- **`Update(dt)`** 0x83E4F0: time += dt; time since the shift += dt; slide −= dt × speed (+0x1C). In gesture mode
  (+0x5C = 0), while the time since the shift is above 0.03 (the first test on the unrounded x87 sum): subtract 0.03,
  move the strengths one point along (memmove) and put the fed strength at point 0. Then height[i] = (cos(i ×
  18.849556f / (n − 1) − time × 3.0f) × 0.30000001192092896 + 0.800000011920929) × height (+0x14): the depth and the
  middle of the wave are float 0.3f and 0.8f stored as doubles ([0x900C70], [0x8CBF28], 0x83E599 / 0x83E59F). It
  writes no sheet field at +4..+0xC. The wave loop has no guard of its own: n = 0 skips it (0x83E56B), and n = 1
  divides by n − 1 = 0 (`fidiv` 0x83E584, masked: height[0] becomes NaN, never drawn since `DoTheDrawing` stops at
  n < 2). The per-point heights are read only by `DoTheDrawing`, which returns first below two points, so the NaN is
  never seen.
- **Material** (`GetLightSheetMaterial` 0x57D440): `.\data\textures\S_LightSheetStars.raw` through fn_0057DBE0 (flags
  0x41, so its `a.raw` alpha companion is loaded, [the frame](../rendering/frame.md)), `CreateMaterial(13)`, +5 |= 1
  (two-sided) | 4 (wrap).
- **The sort key** (0x688367..0x68839D): (m − cam)·(m − cam), with m the middle (+4..+0xC) and cam [0xEA1DB8..C0].
  The middle is the one written by the previous `DoTheDrawing`, so the key is one frame late. The key's three squares
  are summed (x² + y²) + z² (dz, dy, dx loaded, dx² + dy² at 0x688395, + dz² at 0x68839B), kept in x87 registers and
  rounded to a float once at 0x68839D. `NewZObject` inserts the entry before the first node whose key is less than
  the new one: the list runs from the largest key to the smallest (far to near), a new entry goes after existing
  equal keys, and entries past 0x800 are dropped.
- **The first frame's middle**: the AtomData (0xC8 bytes, 0x688575 / 0x6886DC, `PSysBase::operator new` 0x6755B0 →
  fn_006756D0) comes from a 5-bucket pool (block sizes 0x20..0x200; 0xC8 goes to the 0x100 bucket [0xD4E14C]) whose
  every success path ends at 0x6758B6 with `mov eax,0xCDCDCDCD; rep stosd; rep stosb` over the whole block, new and
  reused alike. Neither constructor nor `Init` nor `Update` writes the middle, so before the first draw each axis
  holds 0xCDCDCDCD = −431602080.0f, and the first key is about 5.59e17: on that frame the sheet sorts as the farthest
  object. From the second frame the key is the previous frame's mean. Both creation paths reach `Init` in the same
  `ModifyAtomCollection` call: the new atom is put at the head of the collection's list (+0x40, fn_00674BD0 from
  `CommonInitNewAtom` at 0x674C7A), the loop at 0x6886A6 walks that list from the head, finds the AtomData just made
  (+0x1C = the rule, 0x688155) and reaches 0x68880C (directly at 0x688703, or after the sub-collection loop at
  0x688806), where +0x28 = 0 sends it to `Init(50)` at 0x688822; no frame is drawn inside the call. The flush
  fn_0082F280 walks the Z list from its head [0xEE9D38] along +0x14 (0x82F290..0x82F2AD), so the largest key draws
  first: the first frame's sheet is drawn before (behind) every other sorted object.
- **`DoTheDrawing`** 0x83E8C0 (from the Z-sorter callback 0x4246E0 at 0x4246E4, inside `LH3DRender::FinishFrame`
  0x82F460's flush fn_0082F280 at 0x82F480), gesture mode, one pass:
  - n < 2: nothing. middle = Σ points × (1/n) (0x83E908 on, zero then sum). slide −= trunc(slide). The 0.02
    early-out and the slide × 0.2 are force-field mode only.
  - Per point, three vertices: v0 = P; v2 = middle + (P − middle) × spread (+0x10), then v2.y += height[i] ×
    strength[i]; v1 = P + (v2 − P) × 0.25.
  - light = min(255, ftol(height × strength × 50)) (unsigned compare, so a negative value gives 255). Cs = each
    channel of the colour +0x2C × light >> 8 (masks 0xFF0000FF / 0xFF0000 / 0xFF00), alpha bits 0.
  - (diffuse, specular): v0 (0xFF000000, 0); v1 (Cs | 0xFF000000, ((Cs & 0xFEFEFE) | 0x40000000) >> 1, half the
    colour with specular alpha 0x20); v2 (0xFF000000, 0).
  - UVs: u = 0.1 × the running arc length (|P_i − P_(i+1) mod n| added after each point; the "bounce" test is
    direction > 1, a dead branch, so u only grows); v = 1 − slide, 0.75 − slide, −slide.
  - Triangles per segment (base 3i): (0,1,3) (1,4,3) (1,2,4) (2,5,4).
  - Drawn by the indexed world-triangle draw fn_0081C780: transform by g_world_to_clipping, cull none for a two-sided
    material (bit 0 of material +5, otherwise CCW), each vertex's specular copied into D3DTLVERTEX +0x14
    (0x81C9BB..0x81C9C4), no haze, then one `LH3DRender::DrawTriangle` 0x82F810 for the whole list (skipped when it
    is empty, 0x81CB73).
- **Clipping**: per triangle, all clip codes 0 is kept directly (facing test only when not two-sided, 0x81CBC7), a & b
  & c ≠ 0 is rejected, anything else goes to fn_0081A760 with plane mask 0x20 (0x81CA91, 0x81CB52). fn_0081A760 clips
  the triangle against the frustum planes one bit at a time (all three outside: dropped, 0x81A7BB; one or two: two new
  vertices by fn_0081DD90 and one or two triangles, the second by a recursive call), drops it if it faces away while
  `g_NoBackfaceCull` [0xEA9EB8] is 0, and appends it to `g_TriangleList` 0xEA5DE0 (0x81AA7E..0x81AA9E). It never
  rejects a whole sheet.
- **Specular**: D3DRENDERSTATE_SPECULARENABLE (0x1D) is written by an immediate only in the render-state defaults
  fn_0082C8F0: 0 when [0xEA9E9C] is set (0x82CC37), otherwise 1 (0x82CC50). [0xEA9E9C] is set by
  `LHScreen::DDrawInitialiseDevices` (0x7DD41B..0x7DD443) for a 3dfx Voodoo or Banshee, and at 0x7DD539 when the
  device record's +0xF4 is non-zero. So the specular is **on** on every other device. Mode 13 (fn_0082ECD0, entry 13
  of 0xC38728 and of 0xC387C8) sets alpha blend on, SRCALPHA / ONE, alpha test off, colour and alpha = texture ×
  diffuse, Z write off; it touches neither specular nor fog. The colour is one pass, texture × diffuse + specular
  clamped by the fixed function (🔍 inferred from the D3D7 documentation: the exe has no clamp of its own), and the
  blend weights the whole of it, specular included, by the star texture's alpha (× the diffuse alpha 0xFF).
- **The blend in practice** (rechecked in the v1.42 exe): mode 13's states are, in order, ALPHABLENDENABLE 1
  (0x82ED06), SRCBLEND 5 SRCALPHA (0x82ED2B), DESTBLEND 2 ONE (0x82ED52), ALPHATESTENABLE 0 (0x82ED69), stage 0
  COLOROP 4 MODULATE (0x82ED9D), COLORARG1 2 TEXTURE (0x82EDC9), COLORARG2 0 DIFFUSE (0x82EDE5), ALPHAOP 4 MODULATE
  (0x82EDF0), ALPHAARG1 2 TEXTURE (0x82EDFB), ALPHAARG2 0 DIFFUSE (0x82EE06), ZWRITEENABLE 0 (0x82EE24); ZFUNC,
  specular and fog untouched. `S_LightSheetStarsa.raw` (65536 bytes) is 0xFF in every byte, so after the 4444 cut the
  texture alpha is 255 everywhere, and every vertex's diffuse alpha is 0xFF: the sheet is a plain additive (ONE /
  ONE) draw of clamp(texture x diffuse + specular). The specular's weighting by the stars' alpha has no visible
  effect with this texture. The draw's vertex copy (0x81C9B9..0x81C9D0) and `DrawTriangle` 0x82F810 (FVF 0x1C4,
  XYZRHW | DIFFUSE | SPECULAR | TEX1) change no colour; the only per-draw change is the UV offset of [0xECA628] for
  materials without +5 bit 0x10. The specular's device rule: `DDEnumCallback` 0x7DD5C0 sets a device record's +0xF4
  to +0xEC (1 only for a device enumerated with a GUID, that is a secondary, add-on card, 0x7DD658 / 0x7DD6AB) OR
  `CheckDescForVoodoo` (0x7DD782..0x7DD790); `DDrawInitialiseDevices` copies it to [0xEA9E9C] at 0x7DD539. On a
  primary display adapter that is not a 3dfx card the specular is on.
- **The specular alpha 0x20** is the vertex's fog factor. Nothing in .text sets FOGENABLE (0x1C) and the alpha stage
  does not read the specular, so it is ignored.
- **Not in the water's reflection**: `DoTheDrawing` has two call sites, the callback 0x4246E0 and the force field
  0x54E198. The callback is pushed only by the owners above, none of them in `GLandscape::Draw` 0x5E42E0, and
  `PSysGlobal::DrawLoop`'s one caller is 0x54E019 in `GGame::Process3dEngine`, after `GLandscape::Draw` (0x54DEDE,
  where the reflection is drawn) has returned. The sheet is drawn once a frame, from the flush in `FinishFrame`.
- **The destructor** of the record's atom data (fn_00688200, 0x688231) also calls the hand object's vt +0x2C with 0
  (interface +0x3A0 → +0x482C): the hand's pulse stop, as dead as the pulse ([the hand's pulse](#the-hands-pulse)).
- The force-field mode is +0x5C..+0x64 with `PulseForceField` 0x83E610 (the camera's force field only).
#### The hand's pulse

- `UR_GesturingRecognised::ModifySubCollection` 0x688910, each step of the record's atom: past the pulse start
  (age − fn_0068DE90, [0x936AD8]) the collection's alpha byte +0x50 = ftol(clamp(1 − that / fn_0068DEA0, 0, 1) × rule
  +0x70) (0x688F3A..0x688FA1); then, when the record has an interface status (fn_006882F0) whose `CHand` (interface
  +0x3A0) has its 3D object (+0x482C), each byte of the record's player colour (atom data +0xC0) × ftol(clamp(1 −
  that / rule +0x7C, 0, 1) × 255) >> 8, alpha too, goes to that object's vt +0x2C (0x688FCC..0x689083) with its own
  +0x4C kept.
- vt +0x2C of the hand's object (`LH3DObject::Create(3)`, vtable 0x9A3068) is `SetColorSpecular` 0x7F9770: +0x4C
  the colour, +0x50 the **specular**. The hand's draw (vt +0x108, fn_00813BA0) hands +0x50 to `g_specular` (0x813E34)
  and to the hair (0x81401A), so a written pulse would add the player's colour to the hand.
- **It never reaches a drawn frame.** The atom's rules run only in the PSys step (`GJPSysInterface::Process` →
  fn_00673300 → fn_00673340 → fn_006750E0 → fn_00675AD0, the rule's vt +0x18), and SF_Gesture is stepped once a turn
  from `PSysGlobal::GameLoopEnd` in `GGame::ProcessTurn`, inside `ProcessGameCode`. Every frame,
  `ProcessGraphicsEngine` → `Process3dEngine` then calls `CHand::AddDrawing` (0x54DF4D), which first rewrites the
  object's +0x4C and +0x50 with `LH3DIsland::GetColorAndSpecular` at the hand (0x46D117; the specular is written on
  both its paths, 0x8020F0 and 0x802109) and only then queues the hand's Z object; the under-sea pass sets 0x65A0A0A0
  and 0 itself (`SetColorSpecular` before `DrawUnderWater`). No game code runs between that rewrite and the flush, so
  the hand is always drawn with the land's specular, and the pulse and its stop change no pixel.

#### More addresses from the code

- `PHandFX::DrawHandFX` 0x68DD60 and `DrawSpellInHand` 0x46E680 are called from `CHand::Draw` with g_game_time_inc in
  ms. `PHandFX::Band` is the 0x48-byte band (vtable 0x936B1C; ctor fn_0068CA30).

## openblack

Status: **done** for the casting paths, the effect in the hand, the bands, the tribal power column, the utility
effects and the light sheet; **partial** for the hand glow (computed, not drawn); not ported: the light sheet's
force-field mode and its other owners. The colour pulse of the hand is not drawn in the original
([the hand's pulse](#the-hands-pulse)), so openblack, which does not write it, draws the same hand. Port decisions:
[port decisions: magic](../project/port-decisions-magic.md).

- **The seed leaving the hand** (2026-10-15, issue #57): `HandSystem::HandleSeedApplyResult` calls
  `RenderHandRelease` after the seed leaves (`hand_seed::HandLetsGoAfterApply`). Before, only the status's hand let
  go; the drawn hand kept the seed until it was no longer available, so a seed that follows its spell (the forest's)
  left the hand closed on nothing while the forest grew, and a seed deleted once cast let go a turn late.
  `ApplySeedToObject`'s return-point branch and `HandleSeedApplyResult`'s used-up or deleted branch also call
  `RenderHandRelease` as the seed leaves the hand. Before, the drawn hand kept the seed until its turn update or the
  next frame's pose found it gone, and an icon's swap let go of it without a throw.
- **The apply to an object** casts with `seed::CastOnObject`, the twin of `seed::Cast` ending in `CastAtObject`;
  until 2026-10-10 it cast at the object's position, so a creature spell applied to a creature never reached it. The
  magic fireball: `CanCastOnObject` answers a ball with `seed::ValidToApplyToMagicFireBall` before the castOnObject
  rule, and `ApplySeedToObject` takes it in (`fireball::DeleteAndPutIntoSpellSeed`, result 1: the seed kept).
- **Feeding a fireball**: `gestures::FeedsFireBall` at the start of the pass calls the hand's
  `HandStatus::applyHeldToCursorObject` (`SendSeedApplyToObject`), which leaves the hand's `_applyObjectState`
  (action state 18).
- **The effect in the hand**: `Magic/Hand/HandMagicFX.cpp`; `UR_FollowLocalHand` and `UR_FollowCastPosn` are in
  `Particles/Rules/HandFollow.cpp`; effects that advance per frame use `manager::SetPerFrame`.
- **PHandFX's bands**: `affine::TurnRows(2)` (was `lh_matrix`) turns the rows. openblack had the band along Y and
  spinning about Y (the ring hung below the hand and turned edge-on); corrected (`DrawBand`). The colour is
  `components::ObjectColour`.
- **The hand glow** is computed (`hand_fx::GetGlow`) but **not drawn**: it needs the object program to take a UV
  transform (u × 0.125 + column, v × 0.125 + row) and the flow's texture with its alpha in place of the hand's skin,
  in mode 13 with the object colour at 204 over the player's colour (see "The glow pass, read"). The renderer has the
  hook, `Renderer::DrawHandGlow`, inside the hand's own Z object right after the
  shadows over it, in the world view only; nothing feeds it yet (`graphics::HandGlowOf` turns a `hand_fx::Glow` into
  its draw).
- **The tribal power column**: `Magic/TribalPowerSpin`, run by `hand_fx` behind the miracle FX system
  (`Locator::miracleFxSystem`), drawn by `Renderer::DrawTribalPower` with the temple's `Text3D` text. openblack looks
  the text up by name, `HELP_TEXT_TRIBEPOWER_01` ("Celtic Power") to `_09`; in the Spanish `InfoScript2.txt` those are
  ADD_TEXT lines 1244..1252 (0x4DC..), so the index counts from another start (not checked). `FinishCast` (the
  original's `DoPostCastThings`) calls the miracle FX system's `ReleaseTribalPowerRing` with the hand's position for
  my interface, else its `TribalPowerColumn` at the cast position. Its `UpdateHand` (`hand_fx::Update`) steps the
  ring and the columns with `HandSystemInterface::GetHandMatrix` and the same step; the renderer queues each one in
  the frame's single Z queue (`ZObject::tribalPower`) keyed at `Runner::Position()` (added to his `TribalPowerSpin`
  for this) with `zsort::SumOrder::XYZ`, before the influence circles; the drain draws it with `Text3D`. The miracle
  FX system's `Reset` (`hand_fx::Reset`) deletes every runner at a new game; a land change keeps them, as the
  original keeps its `PHandFX`, and its map clear deletes only the effect in the hand (`hand_fx::OnClearMap`;
  [what a land change keeps](../scripts/land-change-and-saving.md#what-a-land-change-keeps), 2026-10-22).
  - **Approximate**: the step runs in `hand_fx::Update` (the magic loop's frame step), not in the frame's 3D part,
    and is not skipped in sequence mode 3; inside the temple openblack draws the temple pass instead of the world, so
    the names are not drawn there; ties between equal keys follow openblack's arrival order (ring, then columns,
    after the intro light).
- **The utility effects**: `Particles/Utility.cpp`; the trail's rules in `Particles/Rules/Gesture.cpp`, the ribbon in
  `Graphics/RendererChain.cpp`; the noise in `Particles/Noise.cpp` (game_random). The recognised gesture's land points
  are `sampling::LandOrPointAlongPixel(pixel, 400)` (`PickingSystem::LandOrSeaAlong` on the line unmoved, the height
  at the quantised point, else `hand_land_point::AlongMouseRay`). The record is built by the pure
  `particles::utility::RecognisedTrail` (the buffer, the match, the shape lookup, the camera's forward and the land along a
  pixel's line; tests `GestureTrail.*` in `test/test_gesture_effects.cpp`: two samples, a gesture and two land points,
  the squash's layings, the 400 m), and `GestureRecognised` hands it on; two records in one turn each become an atom
  of their own, the newest first (`GestureEffects.GesturesInQuickSuccessionEachLeaveTheirOwnTrailTheNewestFirst`).
  A faked gesture is `particles::utility::ShowGestureOnLand` over the pure `magic::gestures::LandGestureTrail`: the height
  asked at the map point each laid point falls in, every step in float (tests `RewardOpening.AGestureShownOnTheLand*`
  in `test/test_reward_opening.cpp`).
  The record carries the player of the interface that
  recognised it (`RecognisedGesture::player`). Not ported in the trail's condition: `m_Buttons` bit 0x02 and byte
  [0xD17D10].
- **The light sheet**: `LightSheet` starts its middle at the same 0xCDCDCDCD pattern. The particle service lists the
  sheets (oldest first) and moves them every frame. The draw (`particles::draw::AddLightSheets`,
  `Renderer::CollectFrameParticles`, the `Surface` case of the Z queue's drain, `Renderer::DrawParticleSurface` with
  the `ParticleSurface` program, `Graphics/RendererRevolvedSurface.cpp`): once a frame, for the main view only, the
  sheets are walked newest first, each one's key point read before it is built, then built. Each sheet is one entry of
  the single Z queue keyed (m − cam)·(m − cam) as above, so equal keys keep the game's order, and is drained far to
  near with the rest. The draw is one pass: S_LightSheetStars and its alpha file (ARGB4444 as loaded, linear, no
  mipmaps, wrapping), colour = min(texture × diffuse + specular, 1), alpha = texture alpha × diffuse alpha, mode 13
  (SRCALPHA / ONE, Z test LESSEQUAL, no Z write), no culling, the vertices in the world: the same in every state read.
  Not in the water's reflection. openblack's port skips the wave below two points and keeps time, slide and shift
  running as the exe does.
#### Where the code is

- `Magic/Hand/HandMagicFX`: `LoadBandMesh` (the band's material properties), `HandFxOwner` (the PHandFX's own number
  as a sound channel owner: one hand FX), `MakeBand` (its colour for `PlayerNames::PLAYER_ONE`, inferred the local
  player), `AddPermanentBand` (fn_0068CCC0), `AddTemporaryBand` (fn_0068CD30), `AddChargeBand` (fn_0068CDA0),
  `RemoveAllPermBands`, `ReleaseInHandEffect` (CHand fn_0046E890).
- `ECS/Systems/Implementations/HandSpellSeed.cpp`, the seed in the hand:
  - the seed's tests `ApplyOnlyAfterRecSystem`, `ValidForLockedApplyProcess`, `IsSpellKeptInHand`, `SizingGesture`,
    `ValidToApplyThisToObject` (the script highlight test not ported, approximate) and `IsG3DObjectDrawnInHand`;
  - `HandSystem::GetSpellInfo`: the spell info from the synced status, as `UpdateSpellInfo`;
  - `HandSystem::PushSeedThrowData`: the 0x4D packet's throw block (a hand demo's record copied first);
    `HandSystem::LiveHandThrowData`: the live hand's velocity and HandPos (the held seed's drawn matrix, `HandDrawPose`);
  - `HandSystem::SendSeedApplyToObject` and `HandSystem::SendSeedApplyToMapCoord`: the sends, one waiting apply at a
    time; `HandSystem::ApplySeedToMapCoord`: the 0x12 handler and its success spot visual;
  - `HandSystem::SeedActionPressed`: the press with a seed; `HandSystem::UpdateSeedAction` with
    `SeedAction::LockedApplyMap`: the IN_HAND apply once a turn;
  - `HandSystem::FailApply`: the fail spot visual packet and the failure sample;
  - `HandSystem::ForceDropHeld` (the 0x4D and 0x1D packets) and `HandSystem::ApplyForceDropHeld` (their handler, the
    seed's charge back to its worship site);
  - `HandSystem::SeedLeftHand`: the last seed type, the icon's cancelled charge and the in-hand effect released;
    `HandSystem::UpdateSeedInHand`: the status's `validToShake` while a seed is held.

### Test hooks

- Hooks, in [debug environment variables](../project/debugging.md#debug-environment-variables):
  `OPENBLACK_TEST_CAST`, `OPENBLACK_TEST_CAST_PATH`, `OPENBLACK_TEST_THROW_VEL`, `OPENBLACK_TEST_SHOT_PATH`,
  `OPENBLACK_TEST_GESTURE` and `OPENBLACK_GESTURE_TRACE`.
- **For other systems**:
  - `OPENBLACK_TEST_SEED` + `OPENBLACK_TEST_CAST` casts through the real hand path;
  - `_CAST_PATH` drags the hand during the first press (food, wood, water);
  - `_THROW_VEL` is the hand velocity the spell receives (the fireball).
- `test_gestures`:
  - octants and rounding of .5; corners of a square; the clearing after the still samples;
  - synthetic templates (it recognises its own and not the others; the mirror, only with `allowReverse`);
  - the selection with a fake icon: SPIRAL opens and FORK_RIGHT requests seed 4;
  - with `OPENBLACK_GAME_PATH`, `Gestures.jty` (81 × 1628), the player's 14 gestures, and CIRCLE and STAR mirrored
    with `reversed`.
- `test_gesture_file`: the file is read by the `gestures` component (`GestureFile`, from raffclar's tree), which
  writes it back byte for byte. The recogniser's list is pinned, field for field, to the parse it replaced, on the
  real file and on a synthetic twin. A template with more than 80 keypoints is refused by default and kept when the
  recogniser asks (`GestureFile::Open` with `keepLongTemplates`, as the original keeps it;
  `GestureFile.RefusesTooManyPointsUnlessAskedToKeepThem`, [Gestures, the templates](gestures.md#the-templates)). The
  game's file has at most 26.

## Pending

- **Casting from the hand**: the help's event 19, the immersion, the HUD gesture icons and the hand glow are not
  ported.
- **The hand's glow**: the pass itself is read ("The glow pass, read": the hand's own object drawn again inside
  `PHandFX::Draw`, mode 13, its cull mode by the material's two-sided flag, not by the hand's mirroring). Still open:
  when in the frame `PHandFX::Draw` runs, since the caller of `PHandFX::DrawHandFX` 0x68DD60 (vt slot 0x10, which
  calls `PHandFX::Draw` with the frame time) is not found among the 191 sites that use the slot, and so whether the
  pass falls inside the hand's own Z object, as a Z object of its own or at once; and which program draws the hand
  object (vt +0x108). openblack's hook (`Renderer::DrawHandGlow`, fed nothing yet) sits inside the hand's Z object
  right after the shadows over it, in the world view only, until this is known. The draw itself is a port row (the UV
  transform and the flow texture in the object program).
- **SF_Gesture's draw gate**: the original draws it only while MyInterface +0x3A0 != 0 (pending in openblack).
- **The light sheet**: openblack draws it. The visual check is pending until a run recognises a gesture: no check run
  recognises one yet (the hand check's Gestures demo raises no recognition), so the drawing is covered only by its
  unit tests. Issue #139 (the sheet looks less blended than the original's in a live test) is open: every render
  state, the texture, its alpha (255 everywhere), the vertex colours and the strength curve were read again in the exe
  and match ([the blend in practice](#the-light-sheet-as-read-from-the-exe)), so no draw difference is known. What is
  left needs a capture of the original and of openblack at the same moment of the same recognised gesture: the inputs
  (the ideal's place and length, so the height; the frame step that moves the strength along the sheet), the frame's
  colour depth on the original's machine (a 16-bit back buffer would quantise the additive sum), and the 234 sprites
  drawn with the sheet.
  - Not reproduced: the game builds a sheet only when its Z object is drained, so a sheet dropped from a full queue
    (2048 entries) keeps its middle and slide for that frame, while openblack builds every sheet when it collects the
    frame; openblack also collects the frame only for the main view with its entities, where the game queues the
    sheet whenever `g_started_frame` is set. openblack sums the key in float, the same only while the game's FPU runs
    in single precision (not measured). A sheet with fewer than two points, or not yet started, gets no Z object in
    openblack; the game queues one anyway and draws nothing (DoTheDrawing returns), which changes only how full the
    queue is.
  - Still open in the original: the matrix in effect at the flush (taken as the normal camera); the register-state
    `SetRenderState` in `GameWindowProc` 0x7DC54E (could touch specular or fog; not traced) and the device record's
    +0xF4 that also turns the specular off; the specular's clamp, known only from the D3D7 documentation; the x87
    80-bit intermediates (the cos argument, the v1 difference, middle × 1/n), which a double port can miss by an ulp,
    not measured; the constructor's defaults of the three properties (SF_Gesture sets all of them).
  - Not ported: the force-field mode, the other owners (arena, spell icon, worship totem; recheck the n = 1 wave when
    they are ported).
- **The tribal power column**: the step runs in `hand_fx::Update`, not in the frame's 3D part, and is not skipped in
  sequence mode 3; it is not drawn inside the temple. It never shows in the vanilla game. The text index of the tribe
  names against the Spanish file is not checked.
- **Inferred**: the tribal ring's Z key is its `pos` argument (the band's flight matrix 0xEA1CF8 is settled: the
  camera-to-world matrix, above).
