# Advisors and help

## Summary

The help in the original is three things: the two advisor spirits (the good and the evil one) that float in front
of the camera, gesture and speak; the help profile, which counts what the player does (camera moves, casting, the
hand) so the scripts can teach what has not been done yet; and the script timers and countdown the challenges use.
openblack has the spirits (their file, their clips, their movement, their faces, their draw and their anim sounds),
the help events and their counters, the field-of-view tests, the script timers and the countdown; the help profile's
saving with the player profile is not ported.

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the help.

### The advisor spirits

Two `HelpDude`s (0x5B8F00..0x5C2800) driven by `HelpDudeControl` (HelpSystem+0x10): dude 0 the good one
(`Data\HelpSprite\markgood.hd`, 97 bones), dude 1 the evil one (`markevil.hd`, 73 bones). A `.hd` holds a plain L3D0
mesh and 80 clips in CAnim format (not `.anm`: rotations are YXZ Euler angles, layers additive; checked against the
exe's functions in an emulator). What they say, and when, is on [the voices' page](../audio/voices.md#advisors-guidance-confirmations-and-night-voices),
which owns the advisors' voices.

- They float 7..9 units in front of the camera in "hover" coordinates (x and y over half the screen width, clamped
  ±0.75 / ±0.6), moving on cubic splines in time (1.0 s for eject, home and fly) and a potential field (rest zones at
  ±0.66, the partner, the mouse, the screen centre).
- Drawn after the scene in their own view (`RenderPass::FinishFrame3D`, depth cleared) while their in-world blend
  +0x35DC < 0.5, else in the world; alpha fades +3/s in, −2/s out; halo (good only) and puff from `smoke.raw`, the
  rainbow trail.
- No random idle gestures exist: gestures and emotions come from the voice samples' cue labels.
- The mouth: vowels 7/8/9 from the lip-sync key (`audio::lip_sync`, kept by `help::AdvisorVoices`).
- CHL 7, 8, 9, 10, 100, 137, 138, 139, 140, 165, 166, 167, 300, 301, 418 (below).

Objects and files:
- HelpDude: 0x37F0 bytes, vtable 0x900D04, ctor fn_005C17D0(isEvil, 0), then SetState(4). HelpDudeControl Init
  fn_005C2A40 builds both and puts them at their home point, state 0 (0x5C2CEA..0x5C2D16).
- The script-facing HelpSpirit holders are HelpSystem +0xC (good) and +8 (evil); help spirit type 1 → dude 0, any other
  → dude 1 (fn_005C5250 / fn_005C5260).
- A `.hd` is a "LiOnHeAd" file with one segment "helpdude" (`HelpDude::Load` 0x5C2194; names at 0x915D20 good, 0x915D24
  evil; lower case on the disc).
- Read order: bone count (+0x0C, replaced by the mesh's at 0x5C101F), anim count 80, rest height (+0x10: 63.423 good /
  79.154 evil), mesh name char[0x80] ("DATA\Yogi_Mesh.l3d" / "C:\dev\TESTBED\DATA\Demon_Mesh.l3d"), the 80 anim names, a
  has-data flag (0 skips mesh and clips), u32 mesh size then the L3D0, then LoadAnims 0x5C14E0.
- LoadAnims: a count (80), then per slot with a non-empty name a u32 record size (0 = no clip) and CAnim::ReadBinary
  (fn_005C1090 0x5C10CD..0x5C112E); an empty name has no record.
- After the clips: the face bones (+0x2ECC, seven u32: the first eye's three, 0 for none, the second eye's three, then
  the head), a word nothing reads (+0x2EE8, 1 in both), the pupils' texture centre +0x2EEC / +0x2EF0 and their scale
  +0x2EF4 (the offsets fn_005BF9D0 reads), the starting emotion (+0x2C28, 0 in both), u32 n + n bytes of face records
  (+0x2C38, n = 0x200: 8 records of 16 floats, one per emotion), near depth +0x35B8 (8.7333 both), model scale S +0x35B4
  (0.02552 / 0.02683), +0x35C0, the per-anim events, +0x35C4 (0.3467 / 0), +0x35C8 (0 / 0.3067), far depth +0x35BC
  (6.0267 / 5.64, × 1.2 in Init 0x5C2C9B), the 80 anim flags +0x2E7C, halo scale +0x34DC (0.4733 / 0) and halo offset +0x34E0 ((0, −0.28, 0) / 0).
- Per-anim events +0x333C[i] (16 bytes): count, the 12-byte sounds (`PlaySoundFX` 0x5C2800 when the anim's phase crosses
  each one), and +8 / +0xC the loop window of anim mode 4 (0x5BCE49..0x5BCE7A).
- Each sound is {+0 sound id, +4 flag, +8 phase}. Both files have sounds only on anim 58 (Yogi_KnockOnScreen: 155 at
  0.3405 and 0.3630; Demon_TapScreen: 155 at 0.2978, 0.3391, 0.6212, 0.6407, 0.6711, 0.7143, 0.7568) and on the evil
  one's anim 60 (Demon_HandGun: 154 at 0.4810). Every flag is 1.
- Anim flag & 0x20: skipped while clinging (0x5BCEDC).
- S scales the rows (0x5BE323), the partner zone, the bank lean and the trail; the halo is S × +0x34DC × the model scale ×
  100 (0x5C0AB8).
- +0x35C4 / +0x35C8 place the fingertip of the point arm: bone 0 × M + r0 × +0x35C4 × size + r2 × +0x35C8 × size
  (0x5BF1DC..0x5BF2FB) **⚠️ approximate**.

The CAnim clips (CAnim::ReadBinary 0x860860, CFrame::ReadBinary 0x860C30):
- CAnim is 0x38 bytes (`new` at 0x5C1108); the header is eleven u32 (0x860878..0x860965); +0x04 the looping flag (word &
  1); +0x10..+0x18 the root move of the whole clip; channel → bone tables at 0x8609B8 / 0x8609E3; each key holds the
  rotation channels then the position channels.
- The binary size is 0x2C + 4 per channel + 12 per channel and key (fn_00860BE0); the .hd record size before a clip is
  that + 4.
- `frameCount` keys evenly spaced over the duration: a looping clip wraps the last key to the first; a one-shot clip puts
  its last key at the duration (period = duration × frames / (frames − 1), integer maths, 0x860E1A / 0x861EF9). The key
  after the last is always key 0, even for a one-shot clip (0x860E5B / 0x861F33).
- A rotation channel is three Euler angles turned into a matrix by `LHMatrix::SetYXZMatrixOnly` 0x7FAC10 called as (y,
  x, z): M = Rz(z) Rx(x) Ry(y) with column vectors. The two keys' matrices are lerped element by element and each row is
  normalised (fn_007FB5C0).
- The angles rotate the bone in model axes from its rest pose: local = rest world(bone) × key × inverse rest
  world(parent) (row vectors, 0x8610B7..0x861456). A position channel is the bone's absolute translation relative to its
  parent (lerped, 0x861964).
- fn_00860E00 (Set): bones with no channel keep their matrix, or take the stand clip's key 0 (HelpDude +0x28) when called
  with fill = 1, which only the stand pose's Set does (0x5BB8BE); the other Sets (0x5BC8AE, 0x5BC982, 0x5BCAC3 and
  ApplyAnim's 0x5BBB9C) pass 0. It clamps a negative key to 0 (0x860E42); fn_00861EE0 does not.
- fn_00861EE0 (Add): the change from a reference key of the same clip to the sampled key, applied to the current local
  matrices (rotation in the parent's rest axes, translation added, 0x8621C9..0x8629AF). There are no blend weights:
  HelpDude passes the "weight" as the time into the clip (vowels 7..9 fn_005BF810, look 18 / 19 fn_005BF8C0).
- The mirrored path (a bone remap table, last argument) is never used: HelpDude always passes 0.
- `HelpDude::ApplyAnim` 0x5BB980(anim, phase, referencePhase, wrap): the phase wrapped to its fraction (0x5BB9A8) or
  clamped to 1 (0x5BBB26), then ≥ 0; ms = ftol(duration × phase), at most duration − 1; reference key = ftol((frames − 1)
  × referencePhase), at most duration − 1 (sic, 0x5BBA0C); the root move = phase × displacement, added through the rows
  (0x5BBA19..0x5BBAD6).
- The rest skeleton: `LH3DAnim::SetTransform` 0x83A1D0 into +0x28AC (each bone under its parent, the root under the
  identity), its inverses into +0x28B0 (LHMatrix::SetInverse 0x7FB290); SetTransform returns max − min of the world y,
  which the original writes over the .hd file's rest height +0x10 (0x5C1250). FinishAnimStack 0x5C0610 → 0x839F10
  composes the world bones for the draw.

Indices and states:
- Anim names at 0xBF029C (0..9 static, 10..17 copied from the emotion names 0xBF027C "Normal", "Pleased", "Displeased",
  "Sad"… "Furious", 18.. written at 0x5BAD6A). Emotion clips 10..17 (clips + 0x28); avoid 32..35 L / R / U / D; cling
  65..68 L / R / U / D (table 0x5BF5BC); look 18 / 19 (or 70 / 71).
- States +0x3490 / +0x3494 (`SetState` 0x5BD4A0, tables 0x5BD9A0 / 0x5BD9D0): 4 hover, 8 point, 0x40 avoid, 0x80
  fly-to-anim heading to 0x200 (play), 0x100 cling; a state | 0x20 is its outro; 0x29 / 0x2A fall back to 1 (0x5BD535).
- fn_005BBEF0 (playing an anim): 0x80 current or queued heading to 0x200, or 0x200 current or queued. PLAY_SPIRIT_ANIM
  fn_005BBF30: anim 0 while playing → SetState(4); else target, anim +0x350C, speed +0x3508, then SetState(0x80).
- Cling edge fn_005BBD20: |x| > 1.2820513 |y| ([0x900C64] = 0x3FA41A42, 1 / 0.78) → right (x = 1.04) or left (−1.04), else bottom (y = 0.78) or
  top (−0.78).
- Home anchors fn_005BD440: (W/2, 3H/2), (−W/2, H/2), (W/2, −H/2), (3W/2, H/2); the home point fn_005C2E90 is the
  nearest, or (3W/2, −H/2) / (−W/2, −H/2) when within 8 px. Home / vanish: fn_005C32A0 "at home" = control state 0 or 1.
- FLY_SPIRIT fn_005C3250: only out of home, FlyTo in 1 s, clamped. CLING_SPIRIT fn_005C4D40 does not eject. Both take x,
  y in 0..1 of the screen.

The spirit opcodes (each POPs the spirit, `ConvertScriptSpiritToHelpSpirit`; an unavailable object does nothing):
- `SPIRIT_EJECT` 7 0x710410 → `HelpSystem::SpiritEject` 0x5C6570(t, help script) → fn_005C4BD0: a help script's spirit
  appears (0x5C3400), any other is ejected (0x5C32C0). `SPIRIT_APPEAR` 300 0x710460 → SpiritEject(t, 1).
- `SPIRIT_HOME` 8 0x710490 → `HelpSystem::SpiritHome` 0x5C6670(spirit, arg) → fn_005C5200: spirit +0x58 / +0x5C = 0; arg
  ≠ 0 (a help script's spirit) → fn_005C3540, it vanishes (the dude's state +0xC = 1 at once, 0x5C357A); arg 0 →
  fn_005C3590, it flies home (fn_005C2E90 / fn_005BBDD0 first: the advisor flies off, inferred). `SPIRIT_DISAPPEAR` 301
  0x7104E0 → SpiritHome(t, 1).
- `SPIRIT_POINT_POS` 9 0x710510: POP in_world (the raw dword), the position, the spirit → `HelpSystem::SpiritPoint`
  0x5C6590 → fn_005C4EF0 (eject, then point mode 1 with side 8.0 / height 5.0).
- `SPIRIT_POINT_GAME_THING` 10 0x7105B0: POP in_world, the object, the spirit (converted before the test, 0x7105F1); no
  object → "Object no longer valid" (0x7105FD); else `HelpSystem::SpiritPoint` 0x5C6650 → fn_005C4FA0 (the thing's
  position + GetHeight vt +0x42C), re-sent every turn by 0x5C50C0.
- `LOOK_GAME_THING` 100 0x710630: POP the object, the spirit; no object → "Object no longer valid" (0x71066A); else
  fn_005C65F0 → fn_005C4E50.
- `HelpSpirit::Process` 0x5C5270 once a turn re-sends the point object (0x5C50C0) and the look object (0x5C5170); the look
  object goes through the point function (original bug).
- `STOP_POINTING` 137 0x710860 → fn_005C6630 → fn_005C5060; `STOP_LOOKING` 138 0x710890 → fn_005C6610 → fn_005C5090;
  `LOOK_AT_POSITION` 139 0x7108C0 → fn_005C65D0 → fn_005C4E00.
- `SPIRIT_PLAY_ANIM` 140 0x710940: POP the time, the anim (raw), y, x, the spirit; anim outside 0..0x50 → "Invalid enum"
  (0xC20B70), then "Invalid Y" / "Invalid X"; none stops it; `HelpSystem::SpiritPlayAnim` 0x5C6690(t, x, y, anim, time) →
  fn_005C4C80.
- `SPIRIT_PLAYED` 165 0x710A50: push `!HelpSystem::IsSpiritPlayingAnim` 0x5C66C0 (neg / sbb / inc; the negation of
  fn_005C4D10) as a bool.
- `CLING_SPIRIT` 166 0x710AA0 → `HelpSystem::SpiritCling` 0x5C66E0 → fn_005C4D40; `FLY_SPIRIT` 167 0x710B70 → `SpiritFly`
  0x5C6700 → fn_005C4DA0; both POP y, x, the spirit.
- `SPIRIT_SCREEN_POINT` 418 0x710CA0: POP y, x, the spirit; the pixel (ftol(W·x), ftol(H·y)) with W / H = [0xE85058] /
  [0xE8505A] (0x710D45..0x710D7B) → fn_005C65B0 → fn_005C4F50.
- The screen checks (0x7109CA..0x710A28 / 0x710AE8..0x710B4B): "Invalid Y" (0xC20B64) then "Invalid X" (0xC20B58) for a
  value below 0 (NaN too: `fcomp, test ah, 1`) or above 1; the opcode goes on.
- `SPIRIT_SPEAKS`: a text past 6974 becomes 0 (0x710C6E). 🔍 inferred the spirit conversion uses the local player's
  alignment.

Motion:
- Hover x / y (Sethoverx 0x5B96D0 / Sethovery 0x5B98D0) are ignored while the puff runs; a target under 0.001 s snaps.
- The hover channels step in place (0x5BDF6A..0x5BE1A9): x and y in the inline order of the terms; the depth's speed
  in the inline order too, but its value in the ordinary step's, ((((C c4 + b c3) + a c2) + t ss) + sv) (ours:
  `Zoomer::UpdateInline` for x and y, `UpdateInlineSpeed` for the depth).
- PointAt fn_005BC4A0: the world target snaps out of the world, else moves in 0.5 s; off screen when the projection
  fails, the depth is under 1.1 × near (0x5BC607) or y > H (0x5BC62C); the point arm side is LocalRand(2) (0x5BD784).
- `HelpDude::Feel` 0x5B9AD0: the 6 zones (HoverZone::Feel 0x5B9620, at +0x3380) minus 250 e², with the edge limit 0.45 in
  the wide screen, else 0.6; y is weighed × 4/3.
- Zones (fn_005BDAF0): the partner −0.5 with focus, else −5; the point {4, 0.2, 0.6} and {−1, 0.08, 0.16} at the point
  target; the mouse inner radius 0.16 and strength −1.5 × mouse speed − 2; the centre {−4, 0, 0.6}.
- `HelpDudeControl::Process` 0x5C3A30(dt, 0.4) from HelpSystem::Draw3D with g_delta_time × 0.001; the model scales 0.8
  e^(−0.5 (bias − 0.5)): 0.841 / 0.761 (0x5C3A9E..0x5C3AFF).
- Its per-dude dispatch (0x5C3B88..0x5C3CB3): an out dude gets Update1 (0x5C3C83) and later Update2; a dude at home not hovering gets `HelpDude::Update` 0x5BF5D0 (0x5C3CAC); a dude at home hovering (State +0x3490 = 4) only UpdateSaySentence (0x5C3CB3). Update1 always reaches the lip sync (ApplyLipSync 0x5BCD00, called at 0x5BEB05), with the same dt for both dudes, so a dude at home hovering has no lip sync that frame. Nothing runs while the window is minimised; the `ViewMode == 3` skip of Draw3D never happens in 1.20, as nothing writes 3 (audio.md, "Where the lip sync runs").
- Depth fn_005BD2A0: 2 × near when flagged, else lerp(+0x35B8, +0x35BC, smooth(+0x34D0)) × (1 + 0.3 k); smooth fn_005BD250
  = (1 − cos πt) / 2 clamped. Depth spacing fn_005BA010: the two dudes kept 0.9 apart, their squared distance against
  [0x900C58] = 0x3F4F5C2A (0.81000006, one step above the float nearest 0.81); the closeness is
  v = 1 − (1.11111104 d)² ([0x900C54] = 0x3F8E38E3, 1 / 0.9), stored over the y difference's slot (0x5BA0C0); the
  partner's depth goes to −0.5 v and the dude's own to max(v, zMin) (0x5BA0D8), each in 1 s.
- The HUD matrix (0x5BE2F5..0x5BE6E4): the camera's world axes × S × the model scale; pitch from the depth speed × 0.3 +
  the pitch offset; yaw = hover x × −0.8; roll toward the cling edge the short way at 3 rad/s.
- In the world (0x5BE702..0x5BE9B6): one shared bob [0xD15AA4] += dt × 0.3, y + sin(bob × 2.3); the ground is GetAltitude
  + 3; the rows grow by (3 sb + 1) × S × the model scale.
- The base pose clip runs at rate × 1.01 for the evil dude and × 0.99981135 ([0x900C68] = 0x3F7FF3A3) for the good one
  (0x5BCA99 area); the hover clip's sound uses the stable clip's time over the hover clip's duration (sic, 0x5BCA99).
- The bank lean: hover x speed / S × 0.025 × (1 − 0.8 stable), dead zone 0.01 (0x5BF124..0x5BF1CA).
- The point arm angle: atan2 in degrees × 0.57295777918682045 [0x900C80] (sic), × the point weight, + 0.5
  (0x5BF36B..0x5BF37D). The weight (the slot [esp+0x78] that 0x5BF373 multiplies by) is 1 without the arm (0x5BEB27),
  else the arm clip's phase: shown · t · 4 in the intro, shown in the hold, (1 − 4t) · shown in the outro.
- GoInvisible flicker windows: phase 0.13..0.25 and 0.55..0.7, or 0.16..0.25 (0x5BB94B..0x5BB969); [0xD15AAC] /
  [0xD15AA8] drive it with GetTickCount (0x5C0827).

Face and head:
- Before the first face update the current record +0x2E38 is the default one, {5, 10, 0.5, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0} (fn_005C2070, fn_005C2040 and 0x5C1E88..0x5C1E99): it gives the base pose's rate (rec[11]) on the frames before it. The same sixteen floats are what `HelpDudeEmotion`'s constructor 0x5C2090 stores (0x5C2092..0x5C20D1), so the record built before the first `UpdateFace` is this one.
- fn_005BD0B0: the face record lerped from neutral by the emotion weight (fn_005BAA40); the blink fn_005BAC10 (record 0 /
  1 the wait, 2 its length; −0.01 [0x8CEFC4] makes a double blink); the two lids (fn_005C0310, ms clamped to [0, dur −
  1], reference key 0); the emotion clip at +0x2C28 + 10.
- Eye bone scale (fn_005C0310, +0x2ECC..+0x2EE0 by face rec[3] / rec[4]): 1 except the evil Normal 1.01 and Afraid 1.47.
  The 3x3 of the local matrices (+0x28B4, 0x30 bytes a bone) of the bones +0x2ECC / +0x2ED0 / +0x2ED4 are multiplied by
  rec[3] and of +0x2ED8 / +0x2EDC / +0x2EE0 by rec[4], a bone of 0 skipped, before the two lids; not with flag 8.
  **🔍 inferred** the two scales are rec[3] and rec[4]: the face update passes the record from its fourth float.
- The pupils (fn_005BF9D0, passed the record from rec[3]): per eye e, across = clamp(rec[7 + e] + rec[5 + e], 0, 2)
  and down = clamp(rec[5 + e] − rec[7 + e], 0, 2), each × the .hd's pupil scale +0x2EF4; for the vertices of the bone
  +0x2ECC (eye 0) or +0x2ED8 (eye 1), u = (x × 10) × across + +0x2EEC and v = (y × 10) × down + +0x2EF0, z for y in
  the evil advisor (+0x2C24), written into the mesh's vertices, where they stay until the next face update. It runs
  even with flag 8. Ours moves the texture in the vertex program (`USE_ADVISOR_PUPILS`) with the values of the last
  face update.
- Emotion cross-fade dt × 3 (0x5BE207); the peak decays 0.3 / s (0x5BE2D0); SetEmotion fn_005BD210 stores max(peak, 0).
- CalcHeadPos 0x5BFE00: +0x34BC and +0x34C0 zeroed first (0x5BFE12..0x5BFE33), then the head bone's frame with its
  origin moved between the two eye bones, the target in it (a target behind, local y < 0, returns early at 0x5BFFB2,
  leaving the two at 0); +0x34BC = atan2(z, y), +0x34C0 = asin(x), each clamped to ±1.0472 and × 0.477465.
- An anim's root move (ApplyAnim 0x5BBA44..0x5BBAD6) is turned by the rows summed as ((z r2 + y r1) + x r0).
- The head follows at 4.5 × dt (evil) or 1.35 × dt (good), the step × 0.95 (fn_005BF620); the look noise amplitudes
  rec[13] / rec[14] are 0 in both files.
- Look target fn_005BC0A0: interest in a fast mouse (speed 4, distance 0.3, Random < 0.3; decay 0.5 / s, clamp [0, 3]);
  alternation timers Random(2, 4) / Random(3, 6); mode 1 the partner's model point, 2 the camera while talking or
  pointing.
- The mouse in hover space there is computed by integer division (idiv 0x5BC0CB / 0x5BC0FD): −1, 0 or 1 per axis (sic).
  The converted look position (0x5BC350) is then not used (sic): the point target is.

Voice tags (the voice samples' cue labels):
- Where the labels come from: `HelpDude::UpdateSaySentence` 0x5BB610, once `PlaySample` 0x5BB530 has started the line,
  passes the sample info's wave bytes and size (info+0x78 / +0x7C) to `BuildAudioTags` (0x5BB6A7..0x5BB6BD); with no
  bytes it leaves the line without tags and counts an error (0x5BB6DA..0x5BB6E3). The error count [0xC5836C] is cleared
  first (0x5BB6A1); a sentence with any error loses all its tags (0x5BB6EF..0x5BB716).
- `BuildAudioTags` reads the wave with the Windows multimedia I/O from memory ('MEM ', 0x42AEBF..0x42AED1): the "RIFF" of type "WAVE" from the start (0x42AEF4, an error "Error in WAVE chunk"), its "fmt " (0x42AF1E, "Error in FMT chunk"), 0x12 bytes of it with the sample rate at +4 (0x42AF5C..0x42AF7C); after it, inside the RIFF, the "cue " chunk (0x42AF93): the count and count × 24 bytes of points (0x42AFA5..0x42AFF7); then, from there to the end of the file (no parent), the LIST of type "adtl" (0x42B014). Then count times (0x42B27E..0x42B28D): the next "labl" in the list (0x42B052; once one is missing none follows), its id, its text one character at a time up to its NUL (sprintf "%s%c", 0x42B087..0x42B0EF), the first cue point with that id (0x42B0F1..0x42B114; a label without one is skipped), its time = the point's sample position (+4) / the sample rate (`fild` of the position as 64 bits, `fidiv`, 0x42B12E..0x42B148; the same float as dividing the two as floats for positions below 2^24), and the label's tags.
- `AudioTag::BuildAudioTags` 0x42AE70 scans each label for a '[' anywhere in it, copies from there, runs fn_0042A6D0
  until its NUL, steps on by what it used and scans again (0x42B15B..0x42B266), so every bracket of a label is read and
  the text outside them is skipped; ';' and '/' end a group (0x42A6F1..0x42A70F), an unclosed one counting as an error.
  **🔍 inferred** any error drops all the sentence's tags (the use of [0xC5836C] there is not read through), as
  `BuildSentenceTags` does.
- A label's time is written once, into the record at the sentence's tag count, before its brackets are read (0x42B132..0x42B148), and the parser fn_0042A6D0 never writes a record's time (its stores are +4, +8, +0xC and +0x10). The records come from `operator new(size, file, line)` 0x7DB790, which is calloc, also when they grow by 8 (0x42B1D8..0x42B21F, the old ones copied), so only a label's first tag has the label's time and the ones after it, in the same bracket or a later one of that label, have 0. The walker fires them in order and stops only at a tag whose time has not passed, so they fire together with their label's first tag, as if they had its time; the stop's flush (time < limit) does the same.
- A tag record is 20 bytes {time, who, action, index, value}: "[" then "<talker><type> <name>[digits]"; the talker by
  table 0x42AC04 / 0x42AC1C, the type by 0x42AC50 / 0x42AC64; trailing digits are the value (100 without).
- Type 3 names an anim, type 4 an emotion: strnicmp over min(strlen(name), letters) against the 80 anim names / 8
  emotions (0x42AB04..0x42ABEA); type 7 by its first letter (table 0x42AC94 / 0x42ACAC). An unknown name logs
  "unrecognised name in audio tag [%s]" and counts in [0xC5836C].
- The word is read with sscanf "%s " into the static buffer [0xC5716C]; an empty rest leaves the previous word in it, and the position still moves on by its length (0x42A989..0x42A9C0).
- fn_005BCBC0 fires every tag the sentence time has passed (strictly, 0x5BCBE1..0x5BCBF3); the time is the tick time
  while the play position is still −1 (Say's delay), so tags fire on it then; fn_0042ACC0 applies a tag.
- A sentence stopped (0x5BB840): only for the advisor speaking one, LHSampleStop, then fn_005BCBC0(time + 100, 0): the
  tags before the time + 100 ms all fire on that advisor itself, whoever they name (`AdvisorSpirit::FlushTags`, through
  `AdvisorSpiritController::StopSentence`).

Drawing:
- The model (fn_005C0700) is an LH3DObject of type 3 drawn through vt+0x100 in the frame (Draw3D 0x5C5B26, in-world blend
  ≥ 0.5) or vt+0x108 from the FinishFrame callback 0x5C2E10 (priority 100, after the Z reset quad of FinishFrame (d),
  before the 2D rectangles at 10000 and HelpText at 20000); fn_005C3920 decides by the blend (< 0.5 = overlay). Skinned
  with world bones from FinishAnimStack 0x5C0610 (97 / 73 bones).
- Its alpha is obj+0x4C's byte = +0x2C1C × 255 with the GoInvisible flicker (0x5C080F); 0 draws no model (0x5C0952);
  vt+0x48(a < 255) at 0x5C0952..0x5C0966. Light 0 is moved for the overlay (0x5C0732..0x5C080C / 0x5C0763..0x5C080C) and
  put back at 0x5C0EA7. The halo, then the puff (0x5C0986..0x5C0E95), smoke.raw mode 6.
- The overlay light: Get3DPointFromScreen((0, H/2), −10), moved toward the eye by 2 × blend while blend < 0.5
  (0x5C0732..0x5C080C).
- The in-world colour (0x5BE9B6..0x5BEA8F): w = ftol(255 × smooth(blend)); the diffuse is 255 + floor((c − 255) × w /
  256) per channel of the land's colour, keeping the land's alpha, and the specular (c × w) >> 8 keeping its alpha
  (0x5BE9C5..0x5BEA85), so white lerped to the land diffuse and the land specular scaled. **🔍 inferred** which of the
  two colours is the diffuse: one is pushed, the other is in edx (0x5BEA1B / 0x5BEA85).
- The body's light: as an animated object (vt+0x108 `fn_00813BA0` → `fn_0084C3C0`), min((N·L < 0 ? 0 : N·L × 1.3) + 0.4, 1) × 255 truncated on each channel of that colour, no ambient ([Model lighting](../rendering/model-lighting.md#model-lighting)).
- The puff: 16 particles of 0x20 bytes (+0x2C10), made on the first draw (0x5C0B24..0x5C0C06); six Random draws each (the
  CRT stream): grey 116..250, vx −0.8..0.8, vy −1.9..1.5, vz, size, spin; k = 2 − vy.
- StartPuff fn_005C1D20: the alpha target flips (alpha > 0.5 → 0, else 1), so every eject or home is a puff.
- Puff draw (0x5C0AE5..0x5C0E95): time unit ms × 0.0032; fade-in clamp(t × 4, 0, 1); alpha = ftol(255 − 32 age) × fade;
  ARGB = ((2a)/4) << 24 + grey & mask (evil 0x7F1F1F, good 0xFFFFFF); size ((sizeBase − k age fade × 0.5) + 1) × 0.12, at
  least 0.0001; angle spin × age + i; cell (age × 8) & 15; at hover + v × 0.1, depth −depth − 0.6; then vy −= (spin + 1) ×
  u × 0.3. It ends when no particle was drawn.
- The trail: 32 points (+0x28BC), all set to the hover at the call by fn_005BBCD0, which leaves the accumulator +0x344
  and the head +0x348 as they are. fn_005C3850 feeds fn_005B8F00 for both dudes every frame, home or not, with the hover
  x, y and depth and g_delta_time × 0.01: the accumulator takes the step and, past 0.2, gives back 0.2 and puts the
  hover at the head, one point a call at most (0x5B8F07..0x5B8F5B). Only a dude out of home with an in-world blend
  +0x35DC of 0 is drawn (0x5C38DF..0x5C3909): fn_005B90C0 gives 64 vertices per dude through the index list 0xD15788.
- The trail's start (fn_005C1C90, from the dude's constructor): the 32 points at the position it is given (0), the
  accumulator 5.0 (0x5C1CC3), so the first calls take a point each until it has run down, the head 0, and the 16
  sparks (+0x180, 0x1C bytes each: position, velocity, age) at 0 with spark i's age i (0x5C1CEA..0x5C1D0E).
- The sparks (0x5B8F7E..0x5B90A3), on every feed: age += dt; at 16 or more, five draws of the C runtime's Random
  (0x81D180) in this order, Random(−0.5, 0) for z, Random(±0.1) for y and Random(±0.1) for x (the position the fed
  point plus them), Random(±0.3) the y speed, Random(±0.1) the x speed, the z speed 0, and age −= 16; then the
  position += (dt × speed) × 0.1 and the y speed += dt × 0.021. **🔍 inferred** they are never drawn: no reader of them
  was found. Both advisors' sparks draw on the C runtime's stream every frame they start again, from the game's start.
- The strip (0x5B9200..0x5B92B9), for each point from the oldest: the tangent from the point before to the one after;
  all three of it 0 gives a width of 0 and no normalising, else it is made unit length by 1 / its length, the length
  summed as (z² + y²) + x²; the width is the length, + 0.2 only above 0.0001, at most 0.6; the half width is
  ((w × S) / near) × 150, the alpha 20 − ftol(−100 w), at most 64.
- In a frame (Init 0x5C2CAB / 0x5C2CD5): the trails are a callback before the Z reset (flag 0, priority 100), the
  overlay dude draw one after it (0x80000000 | 100), so the in-world dudes' draws (Draw3D) come first, then both
  trails, then the dudes near the screen.
- The rainbow trails: callback 0x5C2E30 (priority 100, after the Z-sorter's flush, before the Z reset) → fn_005C3850 →
  fn_005B90C0, Draw3DWorldTriangle with rainbow.raw in mode 15 (+4 = 5, +5 |= 1; 0x5C386B), ZFUNC LESS (2) during the
  call and 4 after.
- The trail's render states, mode 15's function 0x82E470 through the normal table: ALPHABLENDENABLE 1, SRCALPHA /
  INVSRCALPHA (0x82E4AA..0x82E4CC); ALPHATESTENABLE 1 (0x82E4D1) with the global ALPHAFUNC GREATEREQUAL (0x82CBA6);
  stage 0 COLOROP and **ALPHAOP MODULATE(TEXTURE, DIFFUSE)** (0x82E4EA..0x82E52A), so the alpha tested is the texture's
  alpha times the vertex alpha; ZWRITEENABLE 1 (0x82E53B..0x82E552); ALPHAREF the material's +4 = 5 (0x82E64A..0x82E664);
  CULLMODE none from the two-sided bit (Draw3DWorldTriangle 0x81C57B). The table is the normal one 0xC38728 during the
  callback (🔍 inferred: every writer of the global-alpha table 0xC387C8 into [0xECA618] puts the normal one back in the
  same function) and the forced-ALPHAREF switch [0xECA658] is off (its writers, the burning tree and ghost draws, reset
  it after drawing). The trail's vertex alpha is 20..64 (20 + trunc(100 w), at most 64), so a texel of rainbowa below about 18
  (at 64) to 57 (at 20) is dropped and writes no depth.

- LHMatrix::SetYXZMatrixOnly 0x7FAC10 (lh_matrix::YXZ, its cells and precisions); fn_00860E00 and fn_00861EE0 pass (rot.y, rot.x, rot.z) (in the code: `KeyRotation`)
- The lerp of the two keys' matrices, element by element ((m1 - m0) f + m0), rows normalised (0x8620AC..0x8621B8, 0x860FA9..0x8610B2) (in the code: `LerpedRotation`)
- 0x860878..0x860965: eleven u32 (in the code: `ReadCAnim`)
- fn_007FAFF0 (0x83A266): the bone's matrix times its parent's result (or the identity root) (in the code: `MakeRestSkeleton`)
- 0x860E42 (fn_00860E00 only) (in the code: `SampleKeys`)
- channel: the lerped key (0x8610B7..0x8612B9); no channel and the fill flag: the fill key of this bone, indexed by bone (0x86148C..0x8616B9). Rest world (bone) x key, then x inverse rest world (parent) if the bone is not bone 0 (0x8612BC / 0x8616BC). (in the code: `SetPose`)
- fn_007FACC0 (-z, -x, -y) = the transpose of the reference key's YXZ matrix (0x862086..0x8620A7) (in the code: `ApplyAdditive`)
- 0x8628DB..0x8629AF: the lerped key minus the reference key, added to the translation (in the code: `ApplyAdditive`)
- 0x5BB9A8..0x5BB9B9 (in the code: `ApplyAnimArguments`)
- CAnim: the clip format of the advisor spirits (Data\HelpSprite\*.hd, HelpDude::LoadAnims 0x5C14E0), read by CAnim::ReadBinary 0x860860 / CFrame::ReadBinary 0x860C30. It is not the .anm format of components/anm.  Semantics (runblack.exe; verified by running fn_00860E00 and fn_00861EE0 under an x86 emulator on both .hd files against this code's formulas, max error 4.5e-5): - A clip has `frameCount` evenly spaced keys over `durationMs`. A looping clip spreads them over the whole duration and wraps the last key to the first; a one-shot clip puts its last key at the duration (period = duration * frames / (frames - 1), integer maths, 0x860E1A / 0x861EF9). The next key after the last one is always key 0, even for a one-shot clip (0x860E5B / 0x861F33). - A rotation channel is three Euler angles in radians (x, y, z) turned into a matrix by LHMatrix::SetYXZMatrixOnly 0x7FAC10 (lh_matrix::YXZ) called as (y, x, z): with column vectors M = Rz(z) Rx(x) Ry(y). The two keys' matrices are lerped element by element and each row is normalised (fn_007FB5C0, lh_matrix::NormaliseRows: InverseSquareRoot 0x841170 with its 128-byte table). - The angles are not relative to the parent bone: they rotate the bone in model axes from its rest pose. The local matrix is rest world (bone) x key x inverse rest world (parent) in LH row-vector order (0x8610B7..0x861456). - A position channel is the bone's translation relative to its parent, absolute (lerped, 0x861964). - Layers are additive (fn_00861EE0): the change from a reference key of the same clip to the sampled key is applied to the current local matrices (rotation in the parent's rest axes, translation added). There are no blend weights: HelpDude passes the "weight" as the time into the clip (vowels 7..9 fn_005BF810, look 18/19 fn_005BF8C0). - The mirrored path of both functions (a bone remap table, last argument) is not ported: HelpDude always passes 0.
- fn_00860BE0: the bytes ReadBinary consumes (0x2C + 4 per channel + 12 per channel and key). The .hd record size that precedes a clip is this + 4 (HelpDude save 0x5C105D). (in the code: `CAnimBinarySize`)
- fn_00860E00 (HelpDude 0x5BB8E1 / 0x5BBBBB): writes the clip at `milliseconds` into the local matrices. Bones without a channel keep their matrix, or take `fill`'s key indexed by bone (the stand clip's key 0, HelpDude +0x28) when `fill` is given (the last argument 1 of 0x5BB8BE). (in the code: `SetPose`)
- FinishAnimStack 0x5C0610 -> 0x839F10: each local put under its parent's result, roots under `root` (world = parent * local here; L * W(parent) in the original's row vectors). (in the code: `ComposeWorld`)
- LHFile::OpenSegment 0x7BDEC0 "helpdude" (0x5C21A8) (in the code: `LoadHelpDudeFile`)
- 0x5C2219..0x5C2235: the names (at `out.animNames.resize(HelpDudeFile::k_AnimSlots);`) (in the code: `LoadHelpDudeFile`)
- HelpSystem::GetRandomBanterSet 0x5C81E0, on GRand::LocalRand (the local stream, not the synced one) (in the code: `GetRandomBanterSet`)
- HelpSystem::SetSetSent 0x5C8A80 (in the code: `SetSetSent`)
- HelpSystem::RunMessageSet 0x5C8B80 (set, no thing): false when StopRunningScripts refuses (in the code: `RunMessageSet`)
- HelpSystem::ProcessBanter 0x5C81C0 (in the code: `ProcessBanter`)
- 0x5C609C (at `return voice.sample != 0 && voice.bank != audio::SfxBank::None ? VoiceRoute::Narration : VoiceRoute::None;`) (in the code: `RouteOf`)
- fn_005CADC0 0x5CADE2: NeedsBiggerText 0x4079C0 (the language of Country.txt) -> H / 28, else H / 30. (pending) ReInitialiseText 0x5C7A80 after a resolution change (in the code: `HelpSystem::HelpSystem`)
- ??_GKMIcon 0x448C00, then the slot is 0 (in the code: `DeleteIcon`)
- 0x5C5E53..0x5C5E66 (in the code: `HelpSystem::ProcessIcons`)
- 0x6F7E11: number 0 (in the code: `HelpSystem::RunText`)
- 0x6F7CE8 / 0x6F7DC5: ScriptErrorMessage "Invalid text", then text 0 (in the code: `HelpSystem::RunTextWithNumber`)
- 0x6F7F29: number 0 (in the code: `HelpSystem::TempText`)
- UNICODE_sprintf(0xD96164, "*%s" 0xC0D420, CHAR2WCHAR(string)) (0x6F7E8D) TempText also reports "Development text being used in game!" (0xC0D3F8, 0x6F7E9E) and the string (0x6F7EE8) (ScriptErrorMessage / ScriptWarningMessage: debug output, not ported) (in the code: `HelpSystem::TempTextWithNumber`)
- fn_005CB010 (0x6FF700) (in the code: `HelpSystem::CloseDialogue`)
- HelpText fn_005CCED0 (0x5C6116); the TEXT_DRAW gate of fn_005C6E60 reads HelpTextData[+0x584].arg0, the text being made current here (inferred: TEMP_TEXT's id 0 reads entry 0) (in the code: `HelpSystem::AddText`)
- 0x5C61C0 (in the code: `HelpSystem::StartReadingTime`)
- 0x5C636D: the advisors (in the code: `HelpSystem::IsTextRead`)
- 0x5C640A: add 0x1C2 (in the code: `HelpSystem::IsTextRead`)
- 0x5C6454 (in the code: `HelpSystem::IsTextRead`)
- 0x5C6468..0x5C6475 (in the code: `HelpSystem::IsTextRead`)
- 0x5C64AD (in the code: `HelpSystem::IsTextRead`)
- 0x5C64C3: cmp +0x45DC, turn; sbb; neg (in the code: `HelpSystem::IsTextRead`)
- HelpText::Reset(1) 0x5CB020 (+0xB0 = 0 at 0x5CB053) (in the code: `HelpSystem::ClearAllText`)
- 0x5C6793 (in the code: `HelpSystem::DialogueControlRequest`)
- 0x5C67EA..0x5C67F1: HelpText (+0x14) fn_005CB010 (in the code: `HelpSystem::ClearDialogueControl`)
- 0x5C6807 (in the code: `HelpSystem::ReleaseDialogueControl`)
- 0x5C78C0..0x5C78D4 (in the code: `HelpSystem::ReleaseWideScreenOf`)
- 0x5C54F1 (in the code: `HelpSystem::ClearTextDisplayed`)
- fn_005C68C0: 1 s ([0x92A444]) while it waits for the click, 0.5 s ([0x915D1C]) otherwise (in the code: `HelpSystem::ShownLongEnough`)
- 0x5C8FF0 fn_005D7E40: the interface's hand state (GInterface +0x3AC; also at InterfaceActionProcess fn_005D1120's three exits). (pending) +0x578 = 0 (0x5C8FF7): its reader SendFOVObject 0x5C90B5 is not ported (at `if (Locator::handSystem::has_value())`) (in the code: `Process`)
- The text part of HelpSystem (runblack.exe, g_game+0x25005C): the current text and the five before it, the history, ClearAllText, IsTextRead, the click that ends a text, and the CHL functions RUN_TEXT, RUN_TEXT_WITH_NUMBER, TEMP_TEXT, TEMP_TEXT_WITH_NUMBER, TEXT_READ, GAME_CLEAR_DIALOGUE and GAME_CLOSE_DIALOGUE. The HelpText display (+0x14) is HelpTextDisplay, drawn by Renderer::DrawHelpText; OPENBLACK_TEXT_TRACE logs each text. The voices (milestone B7) plug in through the hooks and queries below (Game.cpp: audio::voices and audio::advisor); with Queries::voiceBankLoaded unset no text has a voice, so IsTextRead takes the reading-time branch, as the original does when the dialogue banks are not registered. Read in the disassembly of 0x5C5550..0x5C6E00, 0x5CB0F0..0x5CBF00 and 0x6F7C70..0x6F8280 (bwdis.py), cited at each step.
- fn_005CBEC0: the number of words of a text, the non-empty pieces its splitter fn_005CB590 returns. Words end at a blank (space, tab, CR, LF, 0xF8FE: fn_005CB0F0), at a '$' or '\' (fn_005CB190) or after 47 characters (0x5CB6E3); "$$" / "\\" (any two of them) is a literal character; '$' or '\' and a code (a digit or one of CDFMNP in either case: fn_005CB1D0 / fn_005CB220) with its digits are not words (fn_005CB2A0 / fn_005CB4E0; after C one more character is skipped, 0x5CB53A); any other character after them is the start of a word. (in the code: `CountWords`)
- HelpSystemInfo (info.dat, global 0xD16160)
- (GetTickCount() - [0xEA1C78]) * [0xEA1C80] + [0xEA1C7C], the scaled clock in milliseconds. Unset: 0. (in the code: `int32_t`)
- g_game+0x205A28 == 1 && (g_game+0x14 & 4): inside the citadel the times are in milliseconds (0x5C6463, 0x5C68E8). Unset: false. (in the code: `bool`)
- HelpSystem::SpiritHome 0x5C6670(spirit, arg) -> fn_005C5200 on the spirit (fn_005C68A0: 1 -> HelpSystem+0xC, any other -> +8): spirit+0x58 / +0x5C = 0 and HelpDudeControl (HelpSystem+0x10) fn_005C3540(dude) when arg != 0 (the dude's state +0xC = 1 at once, 0x5C357A), fn_005C3590(dude) when arg == 0 (fn_005C2E90 / fn_005BBDD0 first: inferred, the advisor flies off); dude = spirit+0x54 != 1 (fn_005C5250). Game.cpp: help::spirits. (in the code: `void`)
- The rest of HelpSystem::SetWideScreen 0x5C6AD0 when +0x45E8 changes: DialogBoxBase::HideAll (on), GInterface::SetActive(!(on && owner)) and the bars' timer +0x45F0 (wideScreenTime 0xD16174 * 1000 * the part already done, 0x5C6B3F..0x5C6B4E): openblack's bars are ScreenFade::SetWideScreen (in the code: `void`)
- fn_005C6E60's profile value TEXT_DRAW (+0x4604, fn_005C6CF0 0x5C6D4B: 1 when the profile has none; openblack has no profiles) (in the code: `GetTextDraw`)
- The profile's TEXT_TOPTOBOTTOM ([0xD16180], 0 when missing, 0x5C6D28..0x5C6D46) (in the code: `GetTextTopToBottom`)
- HelpSystem::SetCurrentControl 0x5C6780: +0x45CC = task (in the code: `SetCurrentControl`)
- fn_005C6800(task) (no 1.20 symbol; END_DIALOGUE 0x7107F9 and the task-stop callback 0x6EC6E9 call it): when the task has the dialogue, give it back, take the wide screen away and send both advisors home (in the code: `ReleaseDialogueControl`)
- fn_005C78C0(task) (the task-stop callback 0x6EC6FA): when the wide screen is on and this task owns it (+0x45EC), SetWideScreen(0, 0) (0x5C78C0..0x5C78DA) (in the code: `ReleaseWideScreenOf`)
- HelpSystem::SpiritHome 0x5C6670 (Hooks::spiritHome) (in the code: `SpiritHome`)
- HelpSystem::SetWideScreen 0x5C6AD0(on, owner): when +0x45E8 changes, +0x45E8 = on and +0x45EC = on ? owner : 0 (0x5C6B17..0x5C6B31), then Hooks::wideScreen (in the code: `SetWideScreen`)
- fn_005C6C90: HelpSystem+0x4610, the profile's READ_SPEED (fn_005C6CF0 0x5C6DC6; 0.5 without a profile, 0x5C6DEC) (in the code: `SetReadSpeed`)
- +0x45F8: the help switch (HelpSystem::Reset 0x5C55FC sets 1; SET_HELP_SYSTEM 0x6FC03D stores the popped value) (in the code: `SetHelpOn`)
- +0x45F4, fn_005C79C0 (its ResetFOV 0x5C9300 is not ported): the profile's HELP_LEVEL, 3 when the profile has none (fn_005C6CF0 0x5C6DB6; openblack has no profiles) (in the code: `SetHelpLevel`)
- The level GGuidance::PlayNow reads (0x71AF99..0x71AFB1): +0x45F8 ? +0x45F4 : 0 (in the code: `GetGuidanceLevel`)
- HelpSystem::TriggerCategory 0x5C8280(category): +0x2D8 + 4 category = the turn (Reset zeroes the 9, 0x5C55BE) (in the code: `TriggerCategory`)
- +0x560: the turn of the last help message started (RunMessage 0x5C8D0C; SetToZero 0x5C54A4 zeroes it) (in the code: `SetMessageTurn`)
- [0xD16180] (in the code: `_textTopToBottom`)
- +0x45E4 (IsTextRead moves it while the narration plays, 0x5C640F) (in the code: `_endMs`)
- +0x45CC (SetToZero 0x5C547B -> ClearDialogueControl) (in the code: `_dialogueOwner`)
- Row-vector v x rows (the LHMatrix 3x3 products of 0x5BA4DD, 0x5BBA19, 0x5BEF99) (in the code: `ByRows`)
- [0xC5716C], the static buffer of fn_0042A6D0's sscanf: it keeps its word from one call to the next (in the code: `TagWordBuffer`)
- fn_0042A6D0 on `s` from `pos`: one tag, the position after it (and the spaces after it) (in the code: `ParseOneTag`)
- sscanf(rest, "%s " (0x9CB580), [0xC5716C]) (0x42A989): after the space skip the rest is either empty, which gives EOF (-1) and leaves the buffer's previous word, or starts with a word; the error test at 0x42A9A7 (test edi, edi; jne) counts only a result of 0, which cannot happen here. Then the position moves on by the buffer's strlen (0x42A9B1..0x42A9C0), the previous word's length too (in the code: `ParseOneTag`)
- the same against the 8 emotions (0x42AB97..0x42ABEA) (in the code: `ParseOneTag`)
- +0x2C38: 8 records of 0x40 bytes (fn_005C2090 x 8, 0x5C2AA5; the 0x200 bytes read at 0x5C2307) (in the code: `DudeData::FromFile`)
- HoverZone::Feel 0x5B9620 (in the code: `HelpDude::Zone::Feel`)
- fn_005C17D0: everything 0 but the values below; the .hd's +0x2C28 (Load) is the starting emotion (in the code: `HelpDude::HelpDude`)
- fn_005BBBD0 (in the code: `HelpDude::SetPosition`)
- fn_005BBDD0 (in the code: `HelpDude::FlyTo`)
- 0x5B96D6 (in the code: `HelpDude::SetHoverX`)
- 0x5B98D6 (in the code: `HelpDude::SetHoverY`)
- test ah, 0x41 at 0x5BD226 (in the code: `HelpDude::SetEmotion`)
- fn_005BBD20 (in the code: `HelpDude::SnapClingEdge`)
- 0x3F851EB8 (in the code: `HelpDude::SnapClingEdge`)
- 0x3F47AE14 (in the code: `HelpDude::SnapClingEdge`)
- fn_005BBE70 (in the code: `HelpDude::Cling`)
- fn_005BBEF0 (in the code: `HelpDude::IsPlayingAnim`)
- fn_005BBF30 (in the code: `HelpDude::PlayAnim`)
- fn_005BBFA0 (in the code: `HelpDude::ScreenPoint`)
- fn_005BC4A0 (in the code: `HelpDude::PointAt`)
- fn_005BC6C0(pos) (in the code: `HelpDude::PointAt`)
- SetState 0x5BD4A0 (in the code: `HelpDude::SetState`)
- [0x8C79F0] (in the code: `HelpDude::SetState`)
- Random 0x81D180 (the CRT stream) six times: the grey first (0x5C1D4C / 0x5C0B6C), then +0, +4, +8, +0xC, +0x14 (in the code: `HelpDude::RandomisePuffParticle`)
- 0xBF4CCCCD / 0x3F4CCCCD (in the code: `HelpDude::RandomisePuffParticle`)
- 0xBFF33333 / 0x3FC00000 (in the code: `HelpDude::RandomisePuffParticle`)
- fn_005C1D20: the 16 particles drawn again with Random only once the draw has made them (+0x2C10, 0x5C1D2E); the first puff of a dude draws them when fn_005C0700 makes them (UpdateDraw) (in the code: `HelpDude::StartPuff`)
- fn_005BD2A0 (in the code: `HelpDude::HoverTo3D`)
- Convert3DToHover 0x5BD390 (in the code: `HelpDude::WorldToHover`)
- HelpDude::Feel 0x5B9AD0 (at `const float limit = _control.Frame().wideScreen ? 0.45f : 0.6f; // 0x3EE66667 / 0x3F19999A`) (in the code: `HelpDude::Feel`)
- [0x900C50] (in the code: `HelpDude::Feel`)
- [0x900C4C] (in the code: `HelpDude::Feel`)
- 0x5BB9D6 -> fn_005BB8F0 (in the code: `HelpDude::ApplyAnim`)
- UpdateHoverPos 0x5BA350 (in the code: `HelpDude::UpdateHoverPosition`)
- [0x900C5C] (in the code: `HelpDude::UpdateHoverPosition`)
- fn_005BA010 (in the code: `HelpDude::UpdateDepthSpacing`)
- fn_005BDAF0 (in the code: `HelpDude::UpdateZones`)
- [0x8CEFCC] / [0x8C79A8] (in the code: `HelpDude::UpdateZones`)
- 0x3E23D70A (in the code: `HelpDude::UpdateZones`)
- [0x900C6C], [0x8AB478] (in the code: `HelpDude::UpdateZones`)
- Update1 0x5BE2F5..0x5BE6E4: the HUD pose (in the code: `HelpDude::UpdateMatrix`)
- the yaw (0x5BE459..0x5BE4DE, [0x8AA3A8]): r0' = c r0 + s r2, r2' = c r2 - s r0 (RotateY) (in the code: `HelpDude::UpdateMatrix`)
- [0x900C8C] (in the code: `HelpDude::UpdateMatrix`)
- fn_005BC7D0 (in the code: `HelpDude::UpdateBasePose`)
- [0x8DE2D0] / [0x900C68] (in the code: `HelpDude::UpdateBasePose`)
- fn_005BD0B0 (in the code: `HelpDude::UpdateFace`)
- IsTalking 0x5BCD11 and [0xD15A9C] 0x5BCD1E, the times and CalcKey are audio::advisor's (LipSyncFrame); so is the StopSentence of a position < 0 past 0.5 s (0x5BCDE2..0x5BCDF7) (in the code: `HelpDude::UpdateAnimStack`)
- FlyToGimme 0x5BCC90 (in the code: `HelpDude::UpdateAnimStack`)
- fn_0042ACC0 (in the code: `HelpDude::FireTag`)
- fn_005BB8B0(0) (at `_layers.push_back({AnimLayer::Kind::Set, anim::Stand, 0});`) (in the code: `HelpDude::Update1`)
- [0x900C88] (in the code: `HelpDude::Update1`)
- fn_005BC0A0 (in the code: `HelpDude::UpdateLookTarget`)
- 0x5BC720: the partner's model point (in the code: `HelpDude::UpdateLookTarget`)
- fn_005BF620 (in the code: `HelpDude::UpdateHead`)
- 0x40900000 / 0x3FACCCCD (in the code: `HelpDude::UpdateHead`)
- fn_005C0140: the look layers; the noise amplitudes rec[13] / rec[14] are 0 in both files (in the code: `HelpDude::UpdateHead`)
- fn_005C00C0 0x5C00C0..0x5C0138: the constants are doubles (fmul / fadd qword 0x900CE0, 0x900CD8, 0x8AC3F8, 0x900CD0, 0x8C7C68, 0x900CC8, 0x8AB680, 0x900CC0, 0x900CB8, 0x900CB0, 0x900CA8, 0x8C9D40), each step rounded to 24 bits by the FPU's precision (fn_007DEE00); fsin / fcos are not rounded, the product after them is (in the code: `HelpDude::UpdateHead`)
- fn_005C0700 0x5C080F..0x5C0931: the alpha byte, then the fade (in the code: `HelpDude::UpdateDraw`)
- [0x900CFC], fst [esp+0x14] 0x5C0B0E (in the code: `HelpDude::UpdateDraw`)
- 0x5C0E38..0x5C0E52, after the sprite: vy -= (spin + 1) u 0.3 ([0x900CF4]) (in the code: `HelpDude::UpdateDraw`)
- 0x5C2CEA..0x5C2D16: both at their home point, state 0 (in the code: `HelpDudeControl::HelpDudeControl`)
- fn_005BD440 (in the code: `HelpDudeControl::Anchor`)
- fn_005C2E90 (in the code: `HelpDudeControl::HomePoint`)
- fn_005C3120 (in the code: `HelpDudeControl::HomePoint`)
- fn_005C3160 (in the code: `HelpDudeControl::HomePoint`)
- fn_005C32C0 (in the code: `HelpDudeControl::Eject`)
- fn_005C3400 (in the code: `HelpDudeControl::Appear`)
- fn_005C3590 (in the code: `HelpDudeControl::Home`)
- fn_005C3540 (in the code: `HelpDudeControl::Vanish`)
- fn_005C2FB0 (in the code: `HelpDudeControl::Cling`)
- fn_005C3250 (at `if (_state[dude] == ControlState::Home)`) (in the code: `HelpDudeControl::Fly`)
- fn_005C31B0 (in the code: `HelpDudeControl::PlayAnim`)
- fn_005C3960 (in the code: `HelpDudeControl::PointAtPosition`)
- fn_005C39B0 (in the code: `HelpDudeControl::PointAtPixel`)
- fn_005C39F0 (in the code: `HelpDudeControl::LookAt`)
- HelpDudeControl::Say 0x5C36D0 0x5C36E3..0x5C3708 with the FPU at 24 bits: fabs, fsub qword 0.94999998807907104 [0x915438] (0.95f as a double: the float subtraction), fcom 0, fadd 1.0f, fmul 250.0f, each rounded to a float (as audio::advisor::Say) (in the code: `HelpDudeControl::SayDelayMs`)
- fn_005C4BD0 (in the code: `HelpDudeControl::SpiritEject`)
- fn_005C5200 (in the code: `HelpDudeControl::SpiritHome`)
- fn_005C4EF0 (in the code: `HelpDudeControl::SpiritPointPosition`)
- fn_005C4FA0 (in the code: `HelpDudeControl::SpiritPointObject`)
- fn_005C4F50 (in the code: `HelpDudeControl::SpiritScreenPoint`)
- fn_005C5060 (in the code: `HelpDudeControl::SpiritStopPointing`)
- fn_005C4E00 (in the code: `HelpDudeControl::SpiritLookAtPosition`)
- fn_005C4E50 (in the code: `HelpDudeControl::SpiritLookObject`)
- fn_005C5090 (in the code: `HelpDudeControl::SpiritStopLooking`)
- fn_005C4C80 (in the code: `HelpDudeControl::SpiritPlayAnim`)
- fn_005C4D10 (in the code: `HelpDudeControl::SpiritPlayingAnim`)
- fn_005C4D40 (no eject) (in the code: `HelpDudeControl::SpiritCling`)
- fn_005C4DA0 (in the code: `HelpDudeControl::SpiritFly`)
- HelpSpirit::Process 0x5C5270, one spirit after the other: fn_005C9060 (HelpSystem::Process 0x5C9008) takes HelpSystem +8 (the evil one, dude 1) first, then +0xC (the good one, dude 0) (in the code: `HelpDudeControl::ProcessTurn`)
- push 0x3ECCCCCD at 0x5C5ADC (in the code: `HelpDudeControl::Update`)
- The anim indices HelpDude uses by number (the name table built at 0x5BAD10; spec_spirits.md section 2)
- 10..17 Normal..Furious (0x5BD1A5: clips + 0x28) (in the code: `FirstEmotion`)
- 32..35 L / R / U / D (0x5BA4CB, 0x5BED01) (in the code: `AvoidL`)
- 65..68 L / R / U / D (0x5BEECB table 0x5BF5BC) (in the code: `ClingL`)
- +0x35C4 (init 0x3DF5C28F at 0x5C1AEB) (in the code: `fingerR0`)
- Sethoverx 0x5B96D0: ignored while the puff runs; clamp to [-0.75, 0.75] (0x900C44 / 0x8AB274); then Zoomer::SetDestinationWithSpeedAndTime(target, 0, T) inline (T < 0.001 snaps) (in the code: `SetHoverX`)
- Sethovery 0x5B98D0: the same on y, clamp [-0.6, 0.6] (0x900C48 / 0x8C7BDC) (in the code: `SetHoverY`)
- fn_005BD210(emotion, peak): target +0x2C30, peak +0x2C34 = max(peak, 0), +0x3488 = 0 (in the code: `SetEmotion`)
- fn_005BBCD0: the 32 trail points reset to the hover (the trail is drawn by the renderer: a counter here) (in the code: `ResetTrail`)
- fn_005BBD20: not in 0x120; |x| > 1.28205 |y| -> right (x = 1.04) or left (-1.04), else bottom (y = 0.78) or top (-0.78) (0x900C64, 0x3F851EB8, 0x3F47AE14) (in the code: `SnapClingEdge`)
- fn_005BBEF0: 0x80 (current or queued) heading to 0x200, or 0x200 current or queued (in the code: `IsPlayingAnim`)
- fn_005BC4A0(pos, inWorld, side 8.0, height 5.0): world Zoomers (snap out of the world, else 0.5 s), +0x3670 / +0x3674 / +0x3678, the projection test, the hover target or (0, 1) off screen, SetState(8, 0), the look target (in the code: `PointAt`)
- fn_005C1D20: the puff starts (+0x2C18 = 1, +0x2C14 = 0), the 16 particles drawn again with Random if the draw has made them (+0x2C10, 0x5C1D2E), alpha target = alpha > 0.5 ? 0 : 1 (in the code: `StartPuff`)
- fn_005BC0A0(dt, engaged, partnerOut, talkOrPoint, lookPos): the look mode and target (in the code: `UpdateLookTarget`)
- fn_0042ACC0(tag, dude, resolve 1, apply 1) (in the code: `FireTag`)
- +0x2C14 (the draw side of the puff, 0x5C0AE5..0x5C0E95) (in the code: `PuffTime`)
- clamp(+0x2C14 x 4, 0, 1) of this frame's draw (0x5C0C2A..0x5C0C64) (in the code: `PuffFade`)
- fn_005BD2A0(hx, hy, k, flag): the hover point in 3D, at depth flag ? 2 near : lerp(+0x35B8, +0x35BC, smooth(+0x34D0)) (1 + 0.3 k) (in the code: `HoverTo3D`)
- ApplyAnim 0x5BB980(anim, phase, referencePhase, wrap): the layer and the root move added through the rows (in the code: `ApplyAnim`)
- fn_00861EE0 called directly at ms = clamp(ftol(dur * w), 0, dur - 1) with the key `referenceKey` (in the code: `AddAt`)
- fn_005BA010(zMin) (in the code: `UpdateDepthSpacing`)
- fn_005BDAF0(dt, focus) (in the code: `UpdateZones`)
- fn_005BC7D0(stable, rate, sfx) (in the code: `UpdateBasePose`)
- The model matrix of Update1 step 8 and the in-world pose of step 9 (0x5BE2F5..0x5BE9B6) (in the code: `UpdateMatrix`)
- The six Random draws of one puff particle in the exe's order (fn_005C1D20 0x5C1D3E..0x5C1DCD, the creation 0x5C0B62..0x5C0BEC): grey, vx, vy, vz, size, spin; the age 0 (in the code: `RandomisePuffParticle`)
- fn_005C5250 / fn_005C5260: type 1 -> dude 0, any other -> 1 (in the code: `DudeOf`)
- --- HelpDudeControl, by dude --- fn_005C2E90: the nearest of the four off-screen anchors, or (3W/2, -H/2) / (-W/2, -H/2) when within 8 px (in the code: `HomePoint`)
- fn_005C32A0: state 0 or 1 (in the code: `IsHome`)
- --- HelpSpirit (type 1 / 2) as the CHL calls them --- fn_005C4BD0 (CHL 7 SPIRIT_EJECT with isHelp, 300 SPIRIT_APPEAR with 1) (in the code: `SpiritEject`)
- fn_005C4EF0 (CHL 9 SPIRIT_POINT_POS) (in the code: `SpiritPointPosition`)
- fn_005C4FA0 (CHL 10 SPIRIT_POINT_GAME_THING): nothing for an unavailable object (in the code: `SpiritPointObject`)
- fn_005C4F50 (CHL 418 SPIRIT_SCREEN_POINT): `pixel` = (x W, y H) as the GScript computes it (in the code: `SpiritScreenPoint`)
- fn_005C5060 (CHL 137 STOP_POINTING) (in the code: `SpiritStopPointing`)
- fn_005C4E00 (CHL 139 LOOK_AT_POSITION) (in the code: `SpiritLookAtPosition`)
- fn_005C4E50 (CHL 100 LOOK_GAME_THING): nothing for an unavailable object (in the code: `SpiritLookObject`)
- fn_005C5090 (CHL 138 STOP_LOOKING) (in the code: `SpiritStopLooking`)
- fn_005C4C80 (CHL 140 PLAY_SPIRIT_ANIM, after the GScript's checks): x, y in 0..1 (in the code: `SpiritPlayAnim`)
- fn_005C4D10 (CHL 165 SPIRIT_PLAYED pushes the negation) (in the code: `SpiritPlayingAnim`)
- fn_005C4D40 (CHL 166 CLING_SPIRIT): x, y in 0..1 (in the code: `SpiritCling`)
- HelpDude::Load 0x5C2194 through LHFile, then LoadRestSkeleton (the L3D0's bones, LH3DAnim::SetTransform 0x83A1D0) (in the code: `LoadAssets`)
- fn_005C2A40: both dudes fn_005C17D0(0 / 1), both at home (0x5C2CEA..0x5C2D16). Without a file the dude has no clip and no mesh: the opcodes still run its states, nothing is drawn (in the code: `Runtime::Runtime`)
- Draw3D 0x5C5ACE: g_delta_time x 0.001 (0x5C5AD4) (in the code: `Runtime::Update`)
- the fill of fn_00860E00's last argument 1: the stand clip's key 0, HelpDude +0x28 (0x5BB8BE) (in the code: `Runtime::EvaluatePose`)
- CalcHeadPos 0x5BFE00(M, target, head): the head bone's frame (fn_00839FA0 of this frame's pose so far under M) with its origin moved to the middle of the two eye bones (+0x2ED8, +0x2ECC; 0x5BFEC8..0x5BFF3D), inverted (LHMatrix::SetInverse 0x7FB290); the target in it, nothing if behind (local y < 0, 0x5BFFA5), normalised (0x5BFFE5..0x5C000F); +0x34BC = atan2(z, y), +0x34C0 = asin(x), each clamped to +-1.0472 ([0x8C79E0] / [0x900CA4]) and x 0.477465 ([0x900CA0]) (in the code: `Runtime::HeadAngles`)
- Update1 0x5BF1DC..0x5BF2FB: (bone matrix 0 x M).pos + r0 (+0x35C4 x +0x10) + r2 (+0x35C8 x +0x10) (approximate operand mapping, spec_spirits_motion section 1.16) (in the code: `Runtime::Fingertip`)
- fn_005C3850 feeds fn_005B8F00 only for the dudes it draws (ctrl state != 0 and +0x35DC == 0) with (+0x3514, +0x3544, +0x3574) and g_delta_time x 0.01 (in the code: `Runtime::UpdateTrail`)
- 0x5BEA8F (in the code: `Runtime::WorldColour`)
- the order of the draw: Draw3D 0x5C5B26's dudes (the frame), then the FinishFrame callback 0x5C2E10's (the overlay); each dude is in one of the two, so the halo clock moves once per drawn halo as before (in the code: `Runtime::FillOverlay`)
- fn_005C3920 / Draw3D 0x5C5B26 (0x5C2CDE) (in the code: `Runtime::Draws`)
- fn_005B90C0: two vertices per point (at `std::array<TrailVertex, 2 * Trail::k_Points> v {};`) (in the code: `Runtime::TrailTriangles`)
- HelpSystem::Draw3D 0x5C59A0 -> HelpDudeControl::Process 0x5C3A30 (g_delta_time x 0.001, 0.4), once a frame; then the pose of each dude (FinishAnimStack 0x5C0610) and the trail ring (the puff particles are the logic's, HelpDude::PuffParticles) (in the code: `Update`)
- fn_005C3850 -> fn_005B90C0 for each dude out of home and not in the world (+0x35DC == 0): 64 vertices each, in the order of the index list 0xD15788 (three per triangle) (in the code: `TrailTriangles`)
- SetColorSpecular of Update1 step 9 (0x5BE9B6..0x5BEA8F): sb = smooth(+0x35DC), w = ftol(255 sb); colour = white lerped to the land diffuse by w (land_light::LerpBytes), specular = the land specular x w >> 8; sb 0: (white, 0). Pure: the Renderer applies it with its land light table (graphics::SpiritOverlay::landLightPoint) (in the code: `WorldColour`)
- CalcHeadPos 0x5BFE00 (in the code: `HeadAngles`)

#### The anim sounds

Read in the exe and run in the oracle (the call to `GAudio::SamplePlayAnimEffect` 0x42A4B0 stubbed, its arguments
captured); bw1-decomp's C (helpdude.cpp) agrees.
- Callers: `ApplyWingsHover` (hover, hover stable, wings), the lip sync's anim slots (`ApplyLipSync`, modes once and
  loop), and Update1's Avoid hop, scripted anim and cling anim. Update1 / Update get play_sound = 1 from both of the
  help spirits' calls (0x5C3C7E, 0x5C3CA7).
- No bank: the bank argument, or `GetSoundFXBank` 0x5BC7C0 (GGlobal audio +0x3AC, the in-game bank); none at all
  returns before anything, so the last phase +0x31FC[anim] is not touched (0x5C2807..0x5C2821).
- The crossing (0x5C2824..0x5C2935): last < 0 takes the phase; both made fractions with `__ftol`; last = the phase's fraction; end + 1 when it wrapped; nothing when last + 0.5 < end; then each sound in order: stop at time > end, play when time >= last, or end >= 1 and end − 1 > time.
- The record (0x5C2976..0x5C29DE): key {1, 1, 17, 1, sound id} (bw1-decomp: heavy impact, 1, hitter 17, target 1),
  action = (flag == 0) (`sete` 0x5C29BA: a set flag plays, a clear one goes to the stop variant), track 0, min 0, max 0,
  the owner the HelpDude itself, the distance `sqrt((dz² + dy²) + dx²)` from `LH3DTech::g_camera` to Transform
  +0x3374..+0x337C, at 24 bits. Oracle: camera (10, 20.5, 33.25), dude (1, 2, 3) → 36.582954 (0x421254F2).
- The point: the 3D callback (0x427200) finds no GameThing in a HelpDude and asks its `Get3DSoundPos` (vtable +0x10,
  0x5C20E0): the Transform's position. Not tracked, so only as the sound starts.
- The in-game bank's rows {*, *, 16 or 17, *, id}: 155 → 162 G_SpriteKnockGlass, 154 → 161 G_SpriteShootGun, 153 → 163
  G_SpriteSlapHead, 15 → 164 G_SpriteFart, one sample each (no draw of the audio library's random).

- +0x45E8 && +0x45EC: a script task holds the wide screen (IsDialogueControlled 0x5C674A, ProcessInterface 0x5C6A2E, GAudio::PlaySoundEffect userParam 1, ProcessAlignmentMusic 0x4279E9) (in the code: `IsScriptWideScreen`)
- [0x8AB274] / -[0x900C44] (in the code: `k_HoverClampX`)
- [0x8C7BDC] / -[0x900C48] (in the code: `k_HoverClampY`)
- [0xE852C0] / [0xE852C4] (in the code: `mouse`)
- fn_0081B450 (+ fn_0081B5F0 when forced) in pixels, nullopt on failure (Convert3DToHover 0x5BD390). Unset: (p.x, p.y) (in the code: `worldToPixel`)
- fn_005BCC20: every anim slot's mode +0x2F7C = 0 (in the code: `ClearAnims`)
- fn_005BBFA0(pixel): +0x35A4 = 0, +0x35AC/B0 = the pixel in hover space, SetState(8, 0) (in the code: `ScreenPoint`)
- Convert3DToHover 0x5BD390 (in the code: `WorldToHover`)
- UpdateHoverPos 0x5BA350(probability) (in the code: `UpdateHoverPosition`)
- fn_005C3960(dude, pos, inWorld, side, height): point mode 1 (in the code: `PointAtPosition`)
- fn_005C39B0(dude, pixel): point mode 2 (in the code: `PointAtPixel`)
- fn_005C39E0: point mode 0 (in the code: `StopPointing`)
- fn_005C39F0 (in the code: `LookAt`)
- fn_005C3A20 (in the code: `StopLooking`)
- fn_0081B450 (+ fn_0081B5F0 forced): 🔍 inferred the clip flag 0x20 is "behind the eye", the others off the screen; ⚠️ approximate the flag tests themselves are not read (in the code: `Runtime::MakeQueries`)
- 0x3F860A92 (in the code: `Runtime::HeadAngles`)

### Timers, help events and field of view

- Script timers 145..148: a ScriptTimer (script type 0x11) keeps the turn it was set and its length in turns; the time
  left is computed when asked and never below 0. It is the only class deleted when no script variable holds it
  (0x561300). The countdown (084/087/092/099/103/144) ticks once a turn (0x6EB6BA).
- GET_TOTAL_EVENTS 237: HelpProfile's 49 counters (g_game+0x250060), each at most once a turn, not while paused or
  while a script holds the wide screen; reset by HelpProfile::Process (0x5C4660, after HelpSystem::Process). Writers:
  the camera 25..30 (the player's camera model), casting 9..22, the hand 1..8, 5/6 (buildings),
  the creature (see Pending). API `help_profile::Trigger(Event)`.
- GAME_THING_FIELD_OF_VIEW 011 / POS_FIELD_OF_VIEW 012: `Graphics/RegionOnScreen` (sphere: LH3DBoundingBox::
  CheckRegionOnScreen 0x868C80; point: fn_0081F1D0); both false inside the temple. 011 (0x6F8130) in a multiplayer game
  pushes 1 without popping; no thing → "Object no longer valid" and 0. Details in
  [Opcodes](../camera/script-camera.md#opcodes).

Script timers (ScriptTimer.cpp):
- A `ScriptTimer` is vtable 0x8E0BAC, 0x30 bytes, made by fn_007115A0: a GameThingWithPos at (0, 0, 0) (SetToZero
  0x5705D0).
- +0x28 is the game turn it was set at (g_game +0x205A40). +0x2C is its length in turns.
- It is never processed: it follows the game turns, so it stops in pause and follows the game speed.
- GetScriptObjectType 0x711600 = 0x11, IsScriptTimer 0x5612F0 = 1, GetSaveType 0x561310 = 0x7D. On release:
  GScript::ReleaseScriptThingIntoTheGame 0x70F61E..0x70F670.
- Elapsed turns: fn_007116C0 = g_game +0x205A40 − +0x28 (unsigned).
- Time left: fn_00711670 = (+0x2C − elapsed, signed, 0 below 0) × [0xD01A38] × 0.001 [0x8AA3B0].
- Time since set: fn_007116D0 = elapsed × [0xD01A38] × 0.001. It keeps growing after the timer has run out.
- SetTurns 0x711610..0x711622 writes the current turn to +0x28 and the length to +0x2C. 0x711630(seconds) sets the
  length in seconds.
- `CREATE_TIMER` 146 0x6F1D20: POP the seconds; fn_007115A0; `AddScriptGameThing(t, 1)` and PUSH it. CREATE 027 with
  SCRIPT_OBJECT_TYPE_TIMER makes one too (fn_006F11A0 0x6F1253, the sub-type as the seconds, fild qword).
- In both cases fn_007115A0(seconds) does Base::new(0x30, "ScriptTimer.cpp", 13) and the caller adds it to the script.
  Out of memory gives 0 and "Thing not created".
- `SET_TIMER_TIME` 0x711280: POP the time, then the thing (none → "Object no longer valid"). A ScriptTimer starts again
  from this turn (0x711318..0x711324, 0x71131C; 0x711630); anything but a timer or a dispenser is "Invalid script
  thing".
- `GET_TIMER_TIME_REMAINING` 0x711370 (type check 0x7113EE): none → "Object no longer valid" and 0.0; not a timer →
  "Invalid script thing" and 0.0; else fn_00711670 (the seconds left, 0 once out). `GET_TIMER_TIME_SINCE_SET` 0x711410
  (type check 0x71148E): none or not a timer → the error and FLT_MAX ([0x95722C]); else fn_007116D0.
- `ScriptTimer::Save` 0x711700 / `Load` 0x7117B0 write +0x28 then +0x2C (4 bytes each), after GameThingWithPos's own
  block.

The countdown timer (GScript's, at g_game +0x250090):
- Its fields: +8 on, +0xC turns left, +0x10 shown.
- `GScript::InitialiseCountDownTimer` 0x6EB8B0 (START_COUNTDOWN_TIMER 084, 0x711150) sets on and shown first
  (0x6EB8B0..0x6EB8C8).
- Seconds ≤ 0 is the script error "Invalid time for timer" (0xC0C0A4), which does not stop the script. Below 0 counts as
  0 (0x6EB8DF..0x6EB8F0).
- turns = ftol(1000 / [0xD01A38] × seconds) (0x6EB8F8..0x6EB91E).
- REMOVE_COUNTDOWN_TIMER 087 0x711180 clears only +8 (0x71118B). The turns and +0x10 stay.
- `GetCountDownTimerRemainingTime` 0x6EB950 (GET_COUNTDOWN_TIMER 092, 0x7111A0) = turns left / (1000 / [0xD01A38]). Both
  are unsigned integer divisions, so the result is whole seconds. It reads +0xC even when the timer is off
  (0x6EB953..0x6EB977).
- A [0xD01A38] above 1000 would divide by 0 🔍 inferred.
- COUNTDOWN_TIMER_EXISTS 099 0x7111D0 pushes +8. HIDE 103 0x7111F0 / REVEAL 144 0x711210 write +0x10.
- fn_006EB930 (from GScript::Process 0x6EB6BA) does `dec [ecx+0xC]; jne` while the timer is on, and turns it off at 0. A
  0-turn countdown goes on below 0.
- The display (GGame::Process3dEngine 0x54E449..0x54E4B4) is drawn while on and shown (0x54E44F..0x54E45B).
- It formats "Time: %.1f" of the time left for (320, 90), size 24.0, and passes it to CreatureMentalEditor::DrawTextA
  0x4DF310, which is an empty function (`ret`) in v1.42: nothing is drawn, so the script countdown is never shown. Its
  other callers draw nothing either (`Town::DisplayHowImpressed` 0x7636B1 / 0x763719, fn_004E0020 0x4E01E7). (The
  arguments would be red below 10 s, green otherwise: below 10 s ([0x8AB414], 0x54E47B) 0xFF goes in the first colour
  argument, otherwise in the second.)

HelpProfile (g_game +0x250060, `HelpProfile::Create` 0x5C4500 from GGame::Init 0x54F4F3):
- One CameraHelpAccumulator (0x10C bytes) per HELP_EVENT_TYPE (names at 0xBF084C): +0 total count, +4 smoothed rate, +8
  ring head, +9 slots used (<= 64), +0xA triggered this turn, +0xC 64 trigger times in AccumulatedTime ms [0xC5AFD8].
- `CameraHelpAccumulator::Trigger` 0x449040(now): ++count, times[head] = now, head = (head + 1) & 63, used = min(used + 1,
  64).
- HelpProfile::Process 0x5C4660 (GGame::ProcessTurn 0x54E6A9, after GScript::Process and HelpSystem::Process): nothing
  while paused, in a script's wide screen or with [0xC4CCEE]; else ProcessSpecialTriggers, rate += (flag − rate) × 0.005
  [0x8C7674] and flag = 0 for the 49, AccumulatedTime += 100, and the same step for CameraHelp's 12 (fn_00449240).
- `HelpProfile::Trigger` 0x5C46E0(type) also counts 14..23 in GestureTotal (24), 1..42 in AllInterface (43), 9..11 in
  CastAll (11) and every type in AllEvents (48).
- `ProcessSpecialTriggers` 0x5C45A0 (only with the player's CameraModeNew3): the screen centre on the land → LookAtLand
  (44), and with heading distance < 15 [0x915448] and pitch > 0.55 [0x91544C] → LookAtLandTooClose (45); off the land with
  pitch < −0.3 [0x915450] → LookAtSky (46).
- `CameraHelp::CameraHelpCallback` 0x449140(reason, point, inputs): 0x3nn → Trigger(25 + nn) and each input bit into
  CameraHelp's input table (0xC5A320); 0x2nn → 0xC5A860 + nn; 0x1nn → 0xC5AC90 + nn; the point is not read. Only
  CameraHelp's debug page (fn_005C47A0) and its tooltips read those tables.
- Reasons: 0x100 CameraExclusion::InsideExclusion (0x45DF7D), 0x101 InsideInclusion (0x45DF2C), 0x102 (0x45C05D);
  0x200..0x203 from CameraModeNew3 (0x45C235, 0x45F77F, 0x45FC10, 0x45FE4A…).
- CameraModeNew3::Update's input mask (0x45C06A..0x45C0C5, names at 0x9CDDF0): 0x01 keyboard, 0x04 both mouse buttons,
  0x10 wheel down.
- The camera's events (0x45C38E..0x45C838): Zoom when the zoom delta ≠ 0; Rotate when |yaw| > 0.01 (the double [0x8C7A10]), then RotateCW (yaw > 0) or RotateCCW (yaw < 0); Pitch when the pitch delta ≠ 0. The original also needs the mode +0x8C to be 2 or 3 and, for the pitch, no auto-pitch (ported: the self-tilting camera's tilt is left out, [Player camera features (CameraHelp)](../camera/player-camera.md#player-camera-features-camerahelp)).
- GET_EVENTS_PER_SECOND 235 (0x70B7F0): POP the HELP_EVENT_TYPE; GetTriggerPerSecond 0x448FC0 = 0 with fewer than 2
  times, else (used × 1000 − 1000) / (now − oldest time); a negative span clamps the ring (fn_00448F90) and gives 0.
- GET_TIME_SINCE 236 (0x70B880): `CameraHelpAccumulator::GetTimeSinceLastUsed` 0x448F40 = 0 if never, else (now − newest
  time) × 0.001; a time in the future clamps the ring and gives 0.
- GET_TOTAL_EVENTS 237 (0x70B910): the count `fild [+8 + type × 0x10C]` (TeachRotate 25, TeachPitch 28, TeachZoom /
  TrackZoomUsage 29, DoubleClicking 30 / 31).
- The three readers take types 1..48 only (0x70B924..0x70B941); another gives the script error "Invalid event"
  (0xC20664) and 0.0.
- The counts belong to the player profile: Save 0x5C4820 / Load 0x5C4830 write and read the 49 accumulators (0x334C
  bytes) to "<user path>\helpstats.dat" (GGame::Save 0x554569, GGame::Load 0x554D1A, PlayerProfile::GetProfileByName
  0x66BC9A, WriteBackToRegistry 0x66BDFD). (not ported)
- `HelpProfile::SetToZero` 0x5C4770 (from its ctor): the 49 Reset (0x448F20: +0, +4, +8, +9, +0xA; the ring stays), then
  CameraHelp::ResetStats 0x4491E0; AccumulatedTime is a static and keeps going.

- CameraHelp's own tables: the inputs of the 0x3nn reasons (0xC5A320..0xC5A85C), the 0x2nn reasons (0xC5A860.. 0xC5AC90) and the 0x1nn reasons (0xC5AC90..0xC5AFB4). Only CameraHelp's debug page (fn_005C47A0) and its tooltips read them: kept so that the callback is whole (in the code: `k_InputCount`)
- HelpProfile::AccumulatedTime [0xC5AFD8] (in the code: `accumulatedTime`)
- 0x449040..0x449045 (in the code: `Accumulator::Trigger`)
- 0x448F41..0x448F4F: fld 0 (in the code: `Accumulator::TimeSinceLastUsed`)
- 0x449016..0x44902F: lea x5 x5 x5, [x8 - 0x3E8] = used x 1000 - 1000; fild; fidiv span (in the code: `Accumulator::TriggersPerSecond`)
- 0x5C475F (at `At(Event::AllEvents).Trigger(now);`) (in the code: `Trigger`)
- 0x45C38E..0x45C3A0: `fcomp 0; test ah, 0x40; jne` (in the code: `OnPlayerCameraMove`)
- 0x70B976: fild dword [+8 + n x 0x10C] (in the code: `TotalEvents`)
- CameraHelpAccumulator::Reset 0x448F20: +0, +4, +8, +9, +0xA to 0 (the ring is left as it is) (in the code: `Reset`)
- GetTimeSinceLastUsed 0x448F40: 0 if never; (now - times[(head - 1) & 63]) x 0.001; a time in the future (a profile loaded over a newer clock) brings every slot down to now (fn_00448F90) and gives 0 (in the code: `TimeSinceLastUsed`)
- HelpProfile::GetTriggerPerSecond 0x448FC0: 0 with fewer than 2 times; d = now - times[(head - used) & 63]; d < 0 clamps the ring (fn_00448F90) and gives 0, d == 0 gives 0, else (used x 1000 - 1000) / d (in the code: `TriggersPerSecond`)
- fn_00448F90: every time later than now becomes now (in the code: `ClampTimes`)
- CameraModeNew3::Update's input mask (the third argument, built at 0x45C06A..0x45C0C5), the names at 0x9CDDF0 (in the code: `CameraInput`)
- [0xC5B0B0] (in the code: `BothMouseButtons`)
- [0xC5B0EC] (in the code: `WheelDown`)
- What HelpProfile::ProcessSpecialTriggers 0x5C45A0 reads of the player's camera mode (CameraModeNew3, nothing for another mode): +0xC8 (the centre of the screen is on the land, inferred), +0x2E4 (the heading distance, inferred) and GCamera::CalculatePitch 0x443070
- The calls CameraModeNew3::Update makes for the player's rotate, pitch and zoom (0x45C38E..0x45C3AD, 0x45C6A8.. 0x45C838; spec_timers_events.md §2.3), from openblack's DefaultWorldCameraModel: `yaw` is its _rotateAroundDelta.y ([0xC5B0C8], turned into radians as yaw x pi / width), `pitch` its _rotateAroundDelta.x ([0xC5B0C4], x 0.002) and `zoom` its zoomDelta ([0xC5B0CC] x 0.0015 x the distance factor). Zoom when zoom != 0; Rotate when |yaw| > 0.01 (the double 0.01 [0x8C7A10]) and then RotateCW (yaw > 0) or RotateCCW (yaw < 0); Pitch when pitch != 0. ⚠️ approximate the original also needs the mode +0x8C to be 2 or 3 and, for the pitch, no auto-pitch (bl); the camera features (CameraHelp::EnabledFeatures 0x02 rotate, 0x01 pitch, 0x04 zoom) zero the deltas before, once openblack's camera reads them (pending, camera) (in the code: `OnPlayerCameraMove`)
- GET_TOTAL_EVENTS 237 (GScript::GetTotalEvents 0x70B910) and the other two readers: nullopt for a type outside 1..48 (`jle` / `cmp 0x31; jl`: the script error "Invalid event" (0xC20664), the script gets 0.0) (in the code: `TotalEvents`)
- GET_EVENTS_PER_SECOND 235 (0x70B7F0): GetTriggerPerSecond (in the code: `EventsPerSecond`)
- GET_TIME_SINCE 236 (0x70B880): GetTimeSinceLastUsed (in the code: `TimeSince`)
- HelpProfile::AccumulatedTime [0xC5AFD8] (in the code: `AccumulatedTime`)
- HelpProfile::SetToZero 0x5C4770 (from its ctor 0x5C4548 -> 0x5C4590): the 49 Reset, then CameraHelp::ResetStats 0x4491E0 (its 12). AccumulatedTime is a static and keeps going (in the code: `SetToZero`)
- GScript::GameCloseDialogue 0x6FF700 (CHL 412): HelpText fn_005CB010 (+0xB4 = 0, +0xAC = 1: display only, and HelpText::Reset clears both again) and ClearAllText (in the code: `CloseDialogue`)
- fn_005C5E50 (HelpSystem::Process 0x5C9001, once a turn): the KMIcons +0x24 / +0x28 go when HelpText has no current text (in the code: `ProcessIcons`)
- fn_005C82F0 (HelpSystem::Process 0x5C9020, once a turn): the thing gone (IsAvailable) or off the screen (fn_0081F1A0 on its LH3DObject +0x40) -> +0x20 = 0; else the bubble's life +0x98 = 3.0 (in the code: `ProcessBubble`)

### The Help key and the advisors' remarks

- The Help key (`GGame::DoAction` 0x63F989, action 0, gated with every action by the script's wide screen and its
  owner, helpSystem +0x45E8 / +0x45EC) calls `HelpSystem::HelpQuery` 0x5C98E0: the first object in the hand
  (`GInterfaceStatus::GetFirstObjectInCurrentHand` 0x5DC130), else the interface's picked thing (+0x3C8), goes to
  `GetHelpQueryOnGameThingWithPos` 0x5C94A0; with neither, the interface's hand position (+0x3B8, a MapCoords) goes to
  `GetHelpQueryAtPosition` 0x5C9780.
- About a thing (0x5C94A0): a villager (vt+0x2C8) under a script (+0x25 bit 4) says 6120 (0x17E8). A villager that is
  a disciple (+0xE0 bit 9) held at its job (`DiscipleInfos[type]` +0xC == 1) skips to its texts; else dead
  (`Living::IsDead` 0x417270) says 3036 (0xBDC), a child (`Villager::IsAChild` 0x55CB00) set 35, a vagrant
  (`Villager::IsVagrant` 0x7531A0) set 38, a villager not of the local player set 34. Another thing: a wonder
  (`Abode::IsWonder` 0x4061C0) not of the local player says set 97. Then its texts (vt+0x4D8 / vt+0x4DC,
  `GetQueryFirstEnumText`): a first of 0 is set 33 (fn_005C9490); a town centre (vt+0x1E0) or a totem statue
  (vt+0x1E4) says them through "HelpTown"; a town desire flag (vt+0x1E8) of a town pushes the need's text and
  min(GetRawDesire, 1) x 100 as floats, then says them through "HelpTownNeeds", and without a town says the need's
  text alone; anything else says them through "MultiHelpJustTalkWithText".
- A villager's texts (`Villager::GetQueryFirstEnumText` 0x753F20): its final state's query text when it has one, else
  for a disciple 3129 + its type for types 0..7, 2770 (0xAD2) for a trader and 3129 for a craftsman, else its info's.
- About the ground (0x5C9780): a MapCoords of 0, 0, 0 is no point (set 33). The point (x, z as metres, y the land
  height plus the altitude) with fish of a fish farm within 2 (fn_00824B10) says 2850 (0xB22); y <= 0.15 (a double)
  on a cell 0..0x1FF of a land block says 2949 (0xB85); else the cell's terrain material
  (`Terrain::GetMaterialInfo` 0x735330) says its texts, set 33 without.
- `RunMessageSet` 0x5C8B80: a set of mode 2 says the thing's texts (`GetStartAndEndTextForSet` 0x5C8AE0), 0 and 0
  without a thing.
- `HelpSpiritSay` 0x71D270 stamps lastPlayed[type] (+0xC + 4 type) and +0x2C, which is lastPlayed[8], the HelpSprites
  gate every remark asks (`CanPlaySpiritRemark`).
- `Town::UpdateAggressor` (bw1-decomp Town.cpp): harm applied by a creature of the local player that is not the town
  player's own creature calls `HelpSpritesCreatureAttackingThem`; any other harm whose caused player (the neutral one
  without) is the local player calls `HelpSpritesAttackingTown` (list 4, another player's town with people).
- `Abode::ApplyEffectsDueToPhysicalDestruction` (bw1-decomp Abode.cpp): a DestructionMesh with less than 0.4
  remaining and the local player's blow calls `HelpSpritesDestroyBuilding` 0x71D070: PlayNow(30), then the abode on
  screen (`IsObjectOnScreen` 0x81F1A0), then list 21.
- `HelpSpritesCheckMoonPhase` 0x71D1C0: see [the voices page](../audio/voices.md) (the wait is a cube).
- Notes, vanilla as it is (raffclar's vanilla-bugs 54..57): the advisor never coughs when a line is cut short; the
  remark count stops at 33 entries; the two alignment remark kinds are swapped; the moon says one-off 5 (3326).

### Notes from the code

**`src/Help/HelpMessageSets.cpp`**

- `ProcessConditions`: 0x5C86B4..0x5C86BE: +0x30, 18 dwords

## openblack

Status: the spirits, the help events, the field of view, the script timers and the countdown are **done**; the
creature's help events and the help profile's file are not (Pending). The step-by-step counterparts are under [Counterparts in the code](#counterparts-in-the-code).

- The spirits: `Help/HelpDudeFile` (the `.hd` file), `Help/SpiritAnimClip` (the CAnim clips, formerly
  `Help/CAnim`), `Help/Spirits` and `Help/SpiritsRuntime` (`AdvisorSpiritController`, `AdvisorSpirit`, the anim
  sounds), `Graphics/RendererSpirits` (their own view, `RenderPass::FinishFrame3D`), the shader
  `vs_object_advisor_instanced`.
- The mouth and the cues: `audio::lip_sync` (kept by `help::AdvisorVoices`), `Audio/WaveCueLabels`.
- The help events: `help_profile::Trigger(Event)`; the field of view: `Graphics/RegionOnScreen`.
- Port decisions for this area: [Port decisions](../project/port-decisions-audio-video-scripts.md#voices-and-help).

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The advisor spirits**

- `AdvisorSpiritController::Process`, the lip sync asked from `AdvisorSpirit::UpdateAnimStack`.
- `k_DefaultFaceRecord`
- `audio::ReadWaveCueLabels` (`Audio/WaveCueLabels`).
- `ParseAudioTags` gives the first tag the time and the others 0.
- the buffer is the parser's `lastWord` argument, which `help::AdvisorVoices` keeps for the whole run. The advisors' lines are the buffer's only live source: BuildAudioTags' one caller is UpdateSaySentence (0x5BB6BD), and the parser's other caller, 0x42A650 (a single label's tags), is never called.
- the help texts slot's `recording` reads the line's wave bytes once more (its bank's file for the dialogue banks, `banks::ReadWave`) for `audio::ReadWaveCueLabels`; `AdvisorVoices::KeepRecording` builds the tags (`BuildSentenceTags`) and its hooks give them to the speaker (`AdvisorSpiritController::SetSentenceTags`) and the stop's flush to it (`StopSentence`), once the advisors are loaded. In the 1.20 HelpSprites.sad, 620 of the 1923 waves carry labels.
- `vs_object_advisor_instanced` (`USE_ANIMATED_OBJECT_LIGHT`), `model_light::AnimatedIntensity`.
- `Renderer::DrawSpirits` draws each dude as the original's fn_005C0700: the instance matrix is the identity (the bones
  are already in the world), nothing at alpha 0, the blended path below 255, light 0 moved for the overlay and put
  back, then the halo and the puff (smoke.raw, mode 6) as world triangles. In the world it draws in
  `RenderPass::MainBlended`, as an overlay in `RenderPass::FinishFrame3D`.
- `Renderer::DrawSpiritTrails` draws the trails in `RenderPass::MainBlended` with a strict less depth test, the ZFUNC
  LESS of fn_005B90C0.
- The `.hd` file's tests (`test_help_dude_file.cpp`, `CheckFile`): the stand clip has a channel of each kind for every
  bone, and the rest height is the one `LH3DAnim::SetTransform` writes over the file's +0x10.
- `render_modes::materials::k_AdvisorTrail` (mode 15, two-sided, ALPHAREF 5), drawn by `Renderer::DrawSpiritTrails` through `WorldTextured` with `u_alphaTest` = `PrimitiveAlpha(15, normal, 5)`: x 5/255, y 2 (the modulated alpha), and the test put back off after the draw. The alpha is compared as a byte rounded to the nearest, as `fs_object` does.

**The anim sounds**

- `CrossedSounds` (Spirits.cpp).
- `AdvisorSpirit::Sound` crosses the anim's sounds as the update reaches the call and hands each one, with the dude's position then and its camera distance (`MakeAnimSound`), to `Queries::playAnimSound`; `spirits::Runtime` plays it with `audio::PlayAnimationEffect` on the in-game bank, owned by one audio object per dude whose point is the sound's. `Queries::soundBank` stands for the no-bank return. Tests: `Spirits.AnAnimSoundGoesToTheInGameBankAsTheOriginalSendsIt` (oracle values), `Spirits.AnAvoidHopPlaysItsSoundsAsItsPhasePassesThem`, `Spirits.WithoutTheInGameBankNoAnimSoundIsLookedAt`.

**Timers, help events and field of view**

- its Polar and ArcBall modes, ⚠️ approximate
- The script timers (`ECS/ScriptTimer`, `ecs::script_timer`): `ElapsedTurns` is fn_007116C0 (unsigned);
  `RemainingSeconds` is fn_00711670 (0 below 0, then turns × the frame ms × 0.001); `SinceSet` answers
  `GET_TIMER_TIME_SINCE_SET`, `std::nullopt` when the thing is not a timer.
- The countdown (`ecs::script_countdown`): its `RemainingSeconds` is `GetCountDownTimerRemainingTime` (whole seconds,
  read even when the timer is off), `SetShown` is `HIDE_COUNTDOWN_TIMER` / `REVEAL_COUNTDOWN_TIMER`.
- The camera's help events in the tests: a rotate of 0.005 stays below the 0.01 the original tests at 0x45C727, so it
  is no Rotate event (`test_timers_events.cpp`).
- `HelpSystem::Queries` holds what the help system asks the game; the tests give it an empty text database, so the
  display's TEXT_DRAW gate (fn_005C6E60) reads no entry (`test_help_system.cpp`).

**The Help key and the advisors' remarks**

- The Help key (`BindableActionMap::HELP`, F1 by default) is read by `input::ShortcutKeys::Update`, gated with the
  other keys (`ShortcutActionsFor`: a script's wide screen, inside the temple, the script's interface interaction),
  and answered by `help::help_key::Ask` (`Help/HelpKey`): the held thing (`HandGrabSystemInterface::GetHeld`), else
  the picked one, else the land under the cursor (`PickingSystemInterface::GetPick`).
- The answers are the pure rules of `help_query` (`Help/HelpQuery`, `test_help_query.cpp`); what they read of the
  world is `ecs::help_query_facts` (`ECS/HelpQueryFacts`); the ground's material is
  `ecs::terrain_material::AtCell`, shared with the forest miracle.
- `message_sets::RunMessageSet(..., thing)` says a thing's texts for a set of mode 2;
  `script_control::RunMessage(..., townNeed)` pushes a town need before its checks.
- The remarks: `town_emergency::UpdateAggressor` (its applier, `AttackRemarkFor`) calls
  `guidance::RemarkAttackingTown`, or `guidance::RemarkCreatureAttacking` for the local player's creature (the town
  player's own creature is the one they lead: owned and leashable); `abodes::OnPhysicalDamage` calls `guidance::RemarkBuildingDestroyed`.
- Debug: the consciences window's "Help key" (ask, the last answer) and "Guidance remarks" (each type's last turn,
  end the waits); the testbed's `advisors.help_key` and `advisors.guidance_alerts`, with `EndGuidanceWaits`.
- Unverified against v1.42 (raffclar's PR #401, ported).

### Test hooks

None of its own; the advisors' debug readouts are in [debugging](../project/debugging.md).

## Pending

- The creature's help events (GET_TOTAL_EVENTS' writers from the creature's code) are not ported.
- The help profile's file: Save 0x5C4820 / Load 0x5C4830 with the player profile (`helpstats.dat`) are not ported;
  openblack keeps no player profiles yet.
- A frame time [0xD01A38] above 1000 would divide by 0 in the countdown (🔍 inferred); unreachable in the game.
- The Help key's answers about a town desire flag, the temple entrance (2000), fireflies and the leash wait for those
  things in openblack.
- 🔍 Whether the Help key acts inside the temple: openblack gates every key there; raffclar's tree lets the Help key
  act. Not read in the exe yet.
