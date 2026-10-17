# The temple interior

## Summary

Inside the temple (the citadel) the original draws its own rooms in place of the land: the main room with its
reflection and pool, the challenge room with its list of scrolls, the creature cave and the save-game room, each
with a camera that flies between fixed views, and its own sounds (doors, scroll squeaks, wooshes, buttons, the
heart's sparks and the worship strain). The game runs a 100 ms temple turn there even while paused. openblack draws
the rooms on its own renderer, keeps the challenge records the scripts write, and plays the interior's sounds;
the hand's placement inside, the save-game room's reflection and the picture of a challenge are not ported.
The temple's outside and its slots are on [the temple and the citadel](../magic/temple-and-citadel.md).

## The original

The addresses are the v1.42 exe's, read with the 1.20 symbols; the patch did not change the interior.

### The temple interior

The original draws the citadel in case 1 of `GGame::Process3dEngine` (`DoCitadelDraw` 0x555410; the frame's order is in [Other cases: temple, video and 2D](../rendering/frame.md#other-cases-temple-video-and-2d)).

- **No sun and no glare inside ✅ faithful.** `LH3DSky::g_b_we_are_inside_citadel` [0xFA2760] is 1 only around `GLandAlignement::DrawSky` (0x555439..0x55544D; the same in `Process3dEngine`). DrawSky → fn_0086A330 → fn_0086B010 still draws the dome and the moon, which do not read the flag, but the sun (fn_0086C140, called at 0x86B618) returns at once while it is set (0x86C141..0x86C148). The "sun drawn" flag [0xFA275C] is only set after the sun is drawn (0x86C20D), and the glare (fn_0086BB60, from `LH3DRender::FinishFrame` 0x82F4C9) runs only if it is set (0x86BB78..0x86BB7F) and clears it (0x86BB92). So inside the citadel there is no glare at all, and its eased visibility [0xFA2778] keeps its value through the visit (the easing at 0x86BBA1 is skipped). `GScript::SetSunDraw` also writes [0xFA2760] (0x708EC1); what the script passes has not been read.
- **The main room's reflection, under its floor and its pool (faithful, relative to the room).** `WorldRoom::Draw`
  0x79E280: with CitadelReflections [0xC381E0] (written at 0x8238E9) and CitadelLightmaps [0xC381E4] (0x82390B),
  fn_00795310(1, …) (0x79E2B5) draws the room mirrored, then its vt+4(1) and fn_007954A0(1), then the floor
  (fn_00795430(0xFF), 0x79E2D9). The mirror multiplies row 1 of the room object's matrix (+0x18: +0x20..+0x28, its
  local y axis) by −1 [0x8AB678] (0x795338..0x795364) and undoes it after the draw (0x7953A7..0x7953CE); the position
  is not touched. fn_007954A0 does the same around fn_0083D8D0 (0x7954C1..0x795525). The room is reflected in its own
  plane y = 0, through its position. openblack's rooms stand at the temple's position (0, 0, 0), turned only about y,
  so the world's y = 0 of `Camera::Reflect` (`DrawTempleReflection`) is that plane, and the pool's reflection
  (`DrawTemplePool`) shows that same mirrored room. Where the original puts the interior in the world has not been
  read ([The game camera](../camera/game-camera.md)); the plane relative to the room does not depend on it.
- `PictureRoomBase::Draw` 0x78F1F0 (the base of the save-game room) mirrors its room the same way when its +0x124 is
  set (0x78F21F, behind the same two flags) and then draws the floor at alpha 0xD0 (0x78F23A) instead of 0xFF.
  openblack mirrors only the main room ([Pending](../rendering/frame.md#pending)).

#### The challenge list

The challenge room's pictures, frames and buttons, the world room's challenge markers and the challenge scroll all read one list that only three script functions fill: SNAPSHOT (057), UPDATE_SNAPSHOT (238) and UPDATE_SNAPSHOT_PICTURE (351). Read in the v1.42 exe; bw1-decomp's `ScriptFunctions.cpp` has the same three bodies.

**The pops ✅ faithful.** Every value is a raw 32-bit `ScriptDLL::POP` (0x6F6BC0), its type kept. Top of the stack
first:

| Function | Pops, in order |
|---|---|
| SNAPSHOT `GScript::SnapShot` 0x6F97B0 | challenge, count n, n values with their types (the first popped kept first), the reminder script (a string offset, `STRING` 0x6F6BD0), title text, alignment, success, focus z, y, x, position z, y, x, quest (data +0x2C) |
| UPDATE_SNAPSHOT `GScript::UpdateSnapShot` 0x6F9640 | challenge, count n, n values, reminder script, title, alignment, success |
| UPDATE_SNAPSHOT_PICTURE `GScript::UpdateSnapShotDetails` 0x6F94A0 | challenge, take picture, title, alignment, success, focus z, y, x, position z, y, x (11, its fixed count) |

- The alignment is clamped to -1..1 and the success to 0..1, each with one `fcomp` against the low bound then one
  against 1 (0x6F9861..0x6F98DF): a NaN takes the low bound (C0 is set for unordered), so NaN alignment = -1, NaN
  success = 0.
- n above 12 (SNAPSHOT, `jbe` 0x6F97EF) or 12 and above (UPDATE_SNAPSHOT, `jb` 0x6F9688) prints "Paul - number of
  stack objects is too big." and still pops all n into a 12-slot array (an overflow in the original). The shipped
  `challenge.chl` passes n = 0, 1 or 2 at every one of its 247 SNAPSHOT and UPDATE_SNAPSHOT calls.
- SNAPSHOT passes take picture = 1, UPDATE_SNAPSHOT 0, UPDATE_SNAPSHOT_PICTURE what it popped
  (`Temple::UpdateChallenge` 0x794970 forwards to the room). UPDATE_SNAPSHOT_PICTURE passes no reminder script (NULL)
  and n = 0. Fields a function does not pop are left as the stack held them: UPDATE_SNAPSHOT's camera and quest, and
  UPDATE_SNAPSHOT_PICTURE's quest.

**The list ✅ faithful.** A singly linked list, head [0xDD0BD8], count [0xDD0BDC], of 0xD8-byte records
(`TempleChallenge`). Record: +0 challenge (u16), +2 reminder script (66 chars, `strcpy`), +0x44 title, +0x48
success, +0x4C alignment, +0x50 position, +0x5C focus, +0x68 turn it started, +0x6C turn it was done, +0x70 n,
+0x74 the values, +0xA4 their types, +0xD4 quest. No limit on the count (the room shows the first 21).

`ChallengeRoom::UpdateChallenge` 0x784780, in this order:
1. Look for the record whose u16 challenge equals the given one's low 16 bits (0x7847AB), from the head.
2. **None:** the picture of slot = count is taken (fn_00784690, 0x7847BE, whatever take picture says); a record is
   made (fn_00781230): challenge, then the common write below, +0x68 = the game turn [g_game+0x205A40], +0xD4 = 1, at
   once replaced by the given quest (0x784925); it is appended at the tail and the count goes up. The alignment is not
   touched.
3. **Found (slot k):** the old values of type 4 (object) lose their script reference (fn_00784750 →
   `GScript::DecrementScriptReference`); the given alignment, clamped again, goes to
   `GAlignment::ScriptUpdate(alignment, the local player [g_game+0x205A59], challenge)` (0x78488C); if the record's
   success is already ≥ 1 (`test ah,1` 0x7848A0; NaN is not), the new success is forced to 1. With take picture: the
   picture of slot k is taken and the common write gets the given camera; without, it keeps the record's position and
   focus (`GetPosition` 0x7813B0, fn_007813D0). The quest is not touched.
4. The common write (fn_007812A0): the reminder script (NULL → empty), title, success, alignment, position, focus, n,
   the values and types; each value of type 4 takes a script reference (fn_00784720 →
   `GScript::IncrementScriptReference`); if the success is exactly 1 (fn_00781440, `fcomp qword 1.0`), +0x6C = this
   turn, so a done record's turn moves on with each later snapshot.

So an UPDATE_SNAPSHOT_PICTURE of a known challenge empties its reminder script and its values.

`GAlignment::ScriptUpdate` 0x414360: change = `GetUpdatedChangeThisTurn`(alignment × GPlayerInfo +0x14, the script scale) (`ScaleChange`.

**Who reads it.** The room's draw callbacks (fn_00782260, fn_00782490, fn_00782530, fn_00782560, fn_007825D0) and its
choice (fn_00782410) walk to slot k; `ChallengeRoom::MakeScrollText` 0x784159; `WorldRoom::DrawChallenges` 0x79E03B
through `GetNumOfChallenges` 0x784F30 and `GetChallengeAtPosition` 0x784F40; sub_7849A0 counts the records whose
success is exactly 1 and sub_7849D0 gives slot k's position (both through the temple, fn_007949B0 and sub_7949C0);
`ChallengeRoom::StartScript(challenge)` 0x784C10, from `ScriptHighlight::InterfaceTap` 0x70ADAD through
`Temple::StartScript` 0x794A20, runs a record's reminder with `TempleChallenge::StartScript(0)` 0x781460 (its values
pushed from the last to the first, then the script by name). The save and load (`ChallengeSave` 0x784C70,
`ChallengeLoad` 0x784D10) keep it, and `ChallengeClear` 0x784DE0 (from `GGame::ClearMap` 0x552DD2 and
`GScript::CleanGameForScriptReboot` 0x6EBC00) lets go of every record's references and frees them.

**Which lands call them.** All 260 calls are in `Scripts\Quests\challenge.chl` (137 SNAPSHOT, 110 UPDATE_SNAPSHOT, 13
UPDATE_SNAPSHOT_PICTURE, for 49 challenges; every challenge an update names also has a SNAPSHOT);
Land 1's `Introduction` takes challenge 12's snapshot after its last camera arrival (instruction 88064, n = 0). None
runs inside the 900-frame Land 1 check or the cycle: openblack logs the first call of every native not written yet,
and the reference runs (`obs2_fixed.log`, `cyc900_obs2.log`) and the hand demos log none.

### The challenge room's woosh, the heart's sparks and the worship strain

Read in the v1.42 exe (bw1-decomp has no bodies for these: `ChallengeRoom.cpp` is empty), with xref and
the oracle (each function run with stubbed callees, its globals and the arguments of its sound call captured).

**The challenge room's woosh** (fn_00782410, 0x782410..0x78248B). `ChallengeRoom`'s constructor 0x781A60 loads its
controls with `TempleRoom::LoadOptionData(file, 0x6A, 0xC25168, 0xC25310, 0xC254B8)` (0x781AA6): 106 callbacks. Of
them, fn_00782630..fn_007828B0 (step 0x20, 21 of them) call fn_007821F0(data, k) and fn_00783B30..fn_00783DB0 (21)
call fn_00782410(data, k) straight, k = 0..20.
- fn_007821F0 (the click): the challenge room camera's blend, `[[[g_game+0x59B4]+0x1C]+0xC4]+0x450` (`fld dword`,
  `fcomp qword 1.0`, `test ah, 0x40`: exactly 1 or unordered), `ChallengeRoomCamera::Init` 0x785332 sets it to 0 and
  `UpdateMain` 0x785CA4..0x785E21 moves it toward 1. At 1 (or NaN) it calls fn_00782410(data, k) and returns.
  Otherwise `Temple::SetCameraToLookAtSubMesh(data+4, 0, 0, 25.0)` 0x78223B, [0xC28604] = k, fn_00784640(k),
  [0xE01EE8] = 1, and no sound of its own (oracle: blend 1.0 and NaN choose; 0.5, 0 and 0x3F7FFFFF turn the camera,
  with submesh `data+4`, 0, 0, 25.0).
- fn_00782410 (the choice): the challenge is node k of the list [0xDD0BD8] (count [0xDD0BDC], node +0 next, +4 the
  `TempleChallenge`); k ≤ 0 gives the first node (oracle: −1 → the first); k ≥ count, an empty list or a null list
  give a null challenge, whose `GetSuccess` (0x7813F0, `fld [ecx+0x48]`) would then read address 0x48 **(not settled:
  no click reaches it unless a control exists for a slot without a challenge)**. [0xDD0BF0] = the challenge in every
  case. If `GetSuccess` is exactly 1.0 or unordered (`fcomp qword 1.0`, `test ah, 0x40`, `jne`) nothing more.
  Otherwise [0xDD0BEC] = 1, fn_00799190 (the temple fade: [0xE06024] = 0xFFFFFF, [0xE06020] = 1.0, [0xC2A150] = 0,
  [0xE06028] = 0), then `PlaySoundEffect(0, 46 + (GetTickCount() & 3), 3, 0, 0, 0, InGame)` 0x782486, the same call
  as `InnerCamera::FocusOnSubMesh` 0x7957A6 (both through 0x429D60, bank [GAudio + 0x3AC]). No random draw: the clock
  picks the woosh. Oracle: success 0.5 → chosen, fade globals as above, woosh 0x31 (tick 0x12345677); 1.0 and NaN →
  nothing; 0 and 0x3F7FFFFF → chosen.
- `ChallengeRoom::Update` 0x782170 then, once [0xDD0BEC] and the fade's end [0xE06028] are both set:
  `LeaveInsideCitadel` 0x78218D, [0xDD0BEC] = 0 and `TempleChallenge::StartScript([0xDD0BF0], 1)` 0x7821A4.

**The heart's sparks.** Both use the heart's beam state (+0xB0 the last target, +0xB4 the last turn) and their own
cyclic counter, and play through the options variant 0x429E30: bank InGame (GAudio + 0x3AC), is3D 1, track 0,
owner the heart, sample 206 + c, the point the heart's `MapCoords::GetLHPoint` (x and z from 16.16 times 10/65536,
y the land's altitude plus its height above the land), everything else the `LH_SamplePlayOptions` defaults (volume
127, pitch 100, loops 0, mode 3). The oracle captured the options for both (offsets +0x04, +0x08, +0x0C, +0x20,
+0x24, +0x30..+0x38).
- fn_004686B0 (the beam at what takes a blow; the only callers are `CitadelHeart::GetActualObjectToEffect` 0x468D1C
  and 0x468D90): nothing without its flag (second argument, a byte). A new target sets +0xB0 and +0xB4 = 0; a null
  target then does nothing. Due when turn > +0xB4 + ftol(1000 / [0xD01A38] × 2.0) (20 turns), +0xB4 = turn; the
  spot visual of +0xAC is made if missing (fn_004684A0, `CreateSpotVisualWithSpecifiedDuration`, type 0x15); without
  it nothing more; then the spark 206 + [0xC5E3E4] (0..4, 0x468815) and a `UR_PlasmaInf` from the heart to the target.
  Oracle: new target at turn 20 no, 21 yes; same target from 100: 120 no, 121 yes; flag 0 or a null target: nothing.
  This is `physics::temple_heart::BeamAtTargetDue` (and `BeamInterval`); `BeamAtItselfDue` is fn_004684F0, which has
  no sound.
- `CitadelHeart::ProcessTransferedDamageEffect` 0x468940 (from `CitadelHeart::Process` 0x4665B1, every turn, after
  fn_00465C70 and fn_004688F0, which forgets an unavailable spot visual and target): life = `GetLife` (vt +0x11C).
  Life ≤ 0 or unordered → share 0; life ≥ 1 → nothing; otherwise share = life. Then nothing unless
  `GetActualObjectToEffect(GetPlayer(), false)` is the heart itself (no building or homeless villager takes its
  blows; with the flag false neither spark function does anything). Due when turn > +0xB4 + ftol(((3.0 − 0.4) × share +
  0.4) × (1000 / [0xD01A38])), each step rounded to a float (24-bit FPU); +0xB4 = turn (the target is kept); the spot
  visual as above, none → nothing more; the spark 206 + [0xC5E3E8] (0..4, 0x468B32) only when share ≤ 0.6 (`fcomp
  0.6`, `test ah, 0x41`, je skips it above); then a `UR_PlasmaInf` between two points of fn_00465C30 whatever the share.
  Oracle (36 lives, 10 at the edges where float and double steps differ by a turn): 0 / −1 / NaN → 4 turns, 0.25 → 10,
  0.5 → 17, 0.6 → 19 with the sound, 0x3F19999B → 19 without, 0x3F7FFFFF → 29; the table is pinned in
  `TempleHeartWounds.TheOriginalsWaitAndSoundForEachLife`.
- No random draw in either; the pitch deviation of the .sad (if any) is the audio library's own `rand`.

**The worship strain.** `Citadel::SetWorshipStrainSoundFrac` 0x463850 plays nothing itself: from
`Citadel::ProcessSpellIcons` 0x46396C with the largest site strain (+0x114, `test ah, 0x41`: strictly larger), only for
the local player's citadel, it moves Citadel +0x70 by [0xD01A38] × 0.001 toward it and clamps. The sound is the
guidance's worship site remark, which reads +0x70 (see Guidance above). In openblack this is already ported and
running: `worship::citadel::ProcessSpellIcons` (`src/Worship/Citadel.cpp`), `audio_queries::WorshipSites`
(`src/ECS/AudioQueries.cpp`) and `CheckWorshipSiteDesires` (`src/Audio/Services/Guidance.cpp`). Re-read against the
exe: identical (the NaN branches differ but cannot be reached: the target starts at 0 and only takes larger values).

### The challenge room's controls and its camera's blend

Read in the v1.42 exe (bw1-decomp has headers only for `ChallengeRoom`, its camera, `TempleRoom` and
`InnerCamera`), with xref and the oracle.

**The controls.** `ChallengeRoom`'s 106 controls (`LoadOptionData`, the table at 0xC25168 / 0xC25310 / 0xC254B8):
- 0..20 `LH_Picture_Challenge_01..21` (pictures): the click is fn_007821F0(data, k), the draw state fn_00782260, shown
  for k < count.
- 21..41 `LH_FRAME_CHALLENGE_01..21` (bronze frames): shown for k ≥ count (fn_00782530).
- 42..62 `LH_FRAME_CHALLENGE_SILVER_01..21`: shown for k < count and the challenge's +0xD4 (its type as found) 0
  (fn_00782560); 63..83 `LH_FRAME_CHALLENGE_GOLD_01..21`: k < count and +0xD4 ≠ 0 (fn_007825D0).
- 84..104 `LH_Button_Challenge_01..21`: the click is the choice fn_00782410(data, k) straight; shown for k < count and
  a success not exactly 1 or unordered (fn_00782490; the compare is unsigned, 0x78249A).
- 105 `LH_Scroll_Challenge`, the room's scroll.
- count is [0xDD0BDC], the list's length; slot k is the (k + 1)th challenge found (a new one is appended at the tail,
  `UpdateChallenge` 0x78494E). The room's mesh has the names in other cases (`LH_PICTURE_challenge_01`,
  `LH_BUTTON_challenge_01`, `LH_SCROLL_CHALLENGE`), so the lookup ignores case **(inferred from the data)**.
- Pictures and buttons act when the press is let go over them (`TempleRoom::UpdateMouse` 0x79A3A0; the picture's
  vt+4 0x78E190, the button's 0x78DC40), as the main room's buttons do.

**The camera's blend** (`InnerCamera::ZoomProgress`, `ChallengeRoomCamera` +0x450). `Init` 0x785332 sets 0. Each
frame `TempleRoom::UpdateMouse` passes dt = `g_delta_time` × 0.001f (0x79A507..0x79A519) through
`ChallengeRoomCamera::Update` 0x7854D0 and `InnerCamera::Update` 0x796B60 to `UpdateMain` 0x7859D0 (vt+0x1C at
0x796DA8), unscaled. There, with the camera's state +0x124:
- not 4 (0x785CA4..0x785CE4): over 1 it becomes 1 (strictly over: an unordered one is kept), then blend − dt × 0.7f
  (0x8AB238); not over 0 (or unordered) it becomes 0;
- 4, looking at a submesh (0x785DE8..0x785E21): blend + dt × 0.75f (0x8AB274); over 1 it becomes 1 (an unordered one
  stays unordered).
- The game's floating point runs at 24 bits (control word 0x007F from `LHResetFPU` 0x7DEE00; `LH3DRender::StartFrame`
  sets 53 bits only for its own timing, 0x82F12A..0x82F144, and resets at 0x82F275), so each step is single float
  arithmetic. The oracle ran the two blocks' bytes for blends 0, 0.5, 0.99, 1 and 1.2 at dt 0.016f, 0.033f and 0, and a
  random sweep of 4000 cases, all matching single float arithmetic (53-bit arithmetic differs in 7.5 % of them).
- What the camera looks at does not matter: a look at the scroll raises it too, so a click on any picture after it
  chooses at once.

## openblack

Status: **partial**. The draw, the challenge records, the sounds and the temple turn are done; what is missing is in
Pending. The drawing is raffclar's, re-applied on our renderer (`Renderer::DrawTemplePass`,
`Graphics/RendererTemple.cpp`).

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**The temple interior**

- `Renderer::DrawTemplePass` (`Graphics/RendererTemple.cpp`), raffclar's drawing re-applied on our renderer.
- the temple pass draws the dome and the moon, and neither the sun nor its glare, as the original.

**The challenge list**

- raffclar's `script::challenge_snapshots` (`ScriptHeaders/ScriptChallengeSnapshots`, the pops) and `ecs::challenge_records` (`ECS/ChallengeRecords`, the list, records as `components::ChallengeRecord` entities), pinned to the exe by `test_challenge_records_original` (oracle runs of `ChallengeRoom::UpdateChallenge` 0x784780 and `GAlignment::ScriptUpdate` 0x414360). The three natives fill it (`KeepChallengeRecord` in `CHLApi.cpp`): they pop every value, keep the record, hold its things through the script objects service and move the local player's alignment at once (`effects::alignment::AddClamped`); the picture is not taken yet. The scripts' restart empties it as `ChallengeClear` does, after the intro's things are released (`GScript::CleanGameForScriptReboot` 0x6EBC00; openblack: `challenge_records::Clear` in `Game::PrepareNewLand`).
- ×(1 − |A|/2) the way the player leans, ×(1 + |A|/2) the other way), the alignment history gets it (`CAlignmentHistory::Add` 0x415110), and `CrudeUpdate` 0x4146B0 adds it to the alignment at once, clamped to -1..1, not through the turn's pending change. At the game's 24-bit x87 precision every step rounds to float (oracle, 9 cases in `test_challenge_records_original`).

**The challenge room's woosh, the heart's sparks and the worship strain**

- **openblack.** `temple_challenge_pick` (`src/3D/TempleChallengePick.h`: `ClickOn`, `CanChoose`, `ChosenWoosh`), `temple_sounds::HeartSpark` (`src/3D/TempleSounds.h`) and `physics::temple_heart::WoundShare`, `WoundBeamInterval`, `BeamAtWoundsDue`, `WoundBeamSounds` (`src/Physics/TempleHeart.h`), with their tests (`test_temple_sounds`, `test_temple_heart`), pinned to the oracle. Not connected yet: openblack's challenge room has no challenge controls, list, fade or script start, and nobody calls `temple_heart` (no `GetActualObjectToEffect`, no spot visual, no plasma).

**The challenge room's controls and its camera's blend**

- **openblack.** `temple_challenge_room` (`src/3D/TempleChallengeRoom.h`: `ControlOf`, `Visible`, `BlendStep`), pinned to the oracle's answers (`test_temple_challenge_room`). The blend is the room camera's zoom toward the submesh it looks at (`TempleCameraModel`, which the scrolls already use: the same rates and the same clamps, so the same values; it steps through `BlendStep`). `TempleChallengeControls` shows the controls of the challenges found (`ecs::challenge_records::InOrder`, a quest's frame gold), hides the others from the draw and the pick, and turns a let-go over the same picture or button into its click; a picture's click before the blend is 1 turns the camera to it (`FocusOnSubMesh` 0x795570: position = frame × (middle + (0, 0, 25)), look-at = frame × middle, in the room mesh's space; the scrolls pass (−30, 0, 0) and a save-game slot (0, 0, 20)). The choice is not connected yet.

### The citadel interior in openblack

`g_game+0x205A28` (0x4282F0, the symbol says `HelpSystem::GetWideScreenControl`) is set to 1 by `GGame::GoInsideCitadel`
0x554004 and to 0 by `GGame::LeaveInsideCitadel` 0x553B1F. In openblack the citadel is the temple interior
(`Locator::temple`, `TempleInteriorInterface::Active`, opened and closed by `ENTER_EXIT_CITADEL` and the debug
window), as `StartCameraControl` (CHLApi.cpp) already reads it. `Game.cpp` registers `GameQueries::insideCitadel` with
that: `SetInsideCitadel` is that query. What the audio does inside (all disassembled again):

- **Filters** (now with data): `GAudio::PlaySoundEffect` 0x429F6D and `SamplePlayAnimEffect`
  0x42A554 only let the userParam 2 samples sound (`cmp di/bp, 2`); the 3D cutoff is measured from
  `LH3DTech::g_camera` (0x429EB1), which in openblack is the same camera (the temple one inside). The ambience is turned off
  (fn_00429100, `atmos_banks::SetTargets`) and the advisor does not interrupt (0x5C3810).
- **Music** (`GameMusic::ProcessCitadelMusic`, `ProcessCitadelMusic` 0x427B60, first branch of `ProcessMusic`
  0x427E2C): inside, on the first turn `LHSampleStopAll` (`sample_play::StopAll`, the 16 channels; the latch is the
  global [0xC56164], which `GAudio::Reset` does not touch and which goes back to 0 on leaving, 0x427C7F); type = 44 + fn_00426C80(
  `GetDiscreteAlignmentValue` 0x414730 of `GPlayer::GetAlignmentValue` 0x64D6A0 of the local player g_game+0x205A59),
  CITADEL_EVIL / NEUTRAL / GOOD, all three in `citadel.sad` (group 4); without a bank it returns 0 and `ProcessMusic` continues
  (0x427BDF). Options: bank, volume 127, start `pos[group − 1]` read (0x427BF6) **before** `fn_004281C0` saves the positions (0x427C28; the alignment saves first), sync 1, fade 1, 2D, pitch 100;
  `LHMusicPlay` **every turn** (the engine re-triggers the same bank, 0x1000DFD4), "Music Playing=%s" and
  returns 1 → `ProcessMusic` sets "Music Playing=NONE", +0x180 = 0, +0x1C = −1 (on leaving the alignment music starts
  again). The alignment is the new query `GameQueries::localPlayerAlignment` (`ecs::audio_queries`:
  `ecs::effects::alignment::Get(PLAYER_ONE)`; 0 without it), not GAudio+0x190. It replaces the `citadelMusic` query.
- **Leaving** (`audio::LeaveCitadel`, from `TempleInterior::Deactivate`): `LeaveInsideCitadel` 0x553B25 → Temple
  `fn_00793D00` (if its engine was started, Temple+0x24): `StopPlayingSoundEffect(2 G_Fire_01, owner 0, InGame)`
  0x793D48 and `(12 G_WaterFlow, 0, InGame)` 0x793D59. Entering has no audio call of its own (0x553E10..0x55405E;
  `Temple::InitEngine` 0x793C60 does not touch the audio).
- **fn_00427200** (the game's 3D function that LHaudio calls at 0x1001438C, 0x1001487B and 0x10014B91): each coordinate
  with |v| > 5000 becomes 0 (`fabs; fcomp qword 5000.0` 0x8C49E0, `test ah, 0x41`: equal, less and NaN stay), in the
  channel's stored point (+0x50, 0x427222..0x42726D, after copying it for the default case) and in the point
  it returns (0x427349..0x42738E); the distance it returns (0x427399..0x427400) is that of the point **without** the cap plus
  the offset to `g_camera`. `audio::GuardSoundPoint`, in `sample_play::UpdateChannels` (LHSampleUpdate3DChannels
  0x10014310: it stops the channel if that distance is not less than the maximum +0x6C, 0x100143AF; otherwise,
  LHSampleSet3DPosition with the capped point) and in `audio::OwnerSoundPoint` (the start of the anim-effects).
- **HelpSystem** `ReadSpeedFactor` fn_005C6CB0: with the game thread's FPU at 24 bits (fn_007DEE00) each step
  rounds to float (the qword constants 0.5, 1, 0.80000000000000004 and 0.20000000000000001 whole, the dwords 4 and 3);
  it returns float; a NaN goes to the first branch (`fcom`, `test ah, 0x41`). E.g.: 0.1 → 2.5999999 (not 2.59999999404);
  0.7 → 0.68000001.

**API**: `audio::LeaveCitadel()` and `audio::ProcessCitadelTurn()` (Audio.h), `audio::GuardSoundPoint(p)` (AudioSystem.h),
`GameQueries::insideCitadel` (now registered), `GameQueries::localPlayerAlignment` (new), removed
`GameQueries::citadelMusic`; `GameMusic::GetCitadelSamplesStopped()` ([0xC56164], for the tests).

**Tests**: `GameMusicTest.CitadelMusicInsideTheCitadel`, `GameMusicTest.CitadelMusicWithoutItsBank`,
`GameSfx.GuardSoundPoint`, `SamplePlayTest.TrackedPointGuardedAt5000`, `HelpSystem.ReadSpeedFactor` (values at 24 bits),
`AudioManager.EachTurnReachesTheServiceAsItsOwnCall`, `PausedTurnTimer.*` (test_game_clock).

**The world pauses inside the citadel.** In the original:
- Entering (`GoInsideCitadel`), the pause flag is kept (g_game+0x2502C4 = bit 2 of g_game+0x14, 0x553F61..0x553F72) and,
  in a single player game, `PauseGame(1)` (0x553F83). Leaving (`LeaveInsideCitadel`), after the sequence mode 0
  (0x553B1F) and fn_00793D00, `PauseGame(g_game+0x2502C4)` (0x553B44..0x553B4B) gives the kept flag back. `PauseGame`
  stops the game timer and starts it again from now, so the world goes on from the turn it stopped at, without catching
  up.
- While paused, the world's turns wait (`LocalTimerSaysDoATurn` says no). In sequence mode 1 (or with the paused
  cinematic), `ProcessNetworkPackets` (0x54CC66..0x54CCFF) has a turn every 100 ms of `GetTickCount`: the static
  [0xCD3AE8] is set to now the first time (0x54CC98), then a turn is due once now − it > 100 (0x54CCA9, `jbe`), which
  moves it on by 100 (0x54CCB0), or to now when it is still more than 200 behind (0x54CCC1..0x54CCCA). It is never
  reset. In sequence mode 1 that turn is `Temple::ProcessGameTurn` 0x794A30 (0x54CCFF).
- `Temple::ProcessGameTurn`, only while paused (0x794A35): `GDebug::ClearMessages(-1)` (0x794A42),
  `ScriptDLL::LookIn(0x18)` (TempleHelp | TempleSpecial, 0x794A4F), `ProcessAudioGameTurn` (0x794A5A) and
  `HelpSystem::Process` (0x794A6F: the hand state, the icons, the spirits, the tooltips, the bubble, the conditions and
  the banter). The paused end of turn (`AtmosProcess(0)`) goes on in between.
- The tooltip's icon is kept while paused inside the citadel (fn_005C9D00 0x5C9D4A, `cmp g_game+0x205A28, 1`), and the
  helpers' frame step (fn_005557E0 0x5557E5) is the real frame time in sequence mode 1.

openblack:
- `TempleInterior::Activate` keeps `game_clock::IsPaused()` and calls `game_clock::Pause(true)` before the sequence mode
  1; `Deactivate` gives the kept flag back after the sequence mode 0. `game_clock::Pause` is `PauseGame`'s: the timer
  stops and starts again from now, the turn number stays, and there is no burst of turns on leaving.
- `Game::Update`, while paused in sequence mode 1: `game_clock::PausedTurnTimer` (the rule of [0xCD3AE8], on
  `game_clock::TickCount()`, so the fixed step makes it deterministic), then `Game::ProcessTempleTurn`:
  `LookIn(TempleHelp | TempleSpecial)`, `audio::ProcessCitadelTurn()` (ProcessAudioGameTurn alone: no sound map, no
  sound tags, no turn 5 gate), the room's tooltip (`TempleInteriorInterface::ProcessTurn`, its Submit), `help::Process`
  and the `OPENBLACK_AUDIO_TEST_CITADEL` hook. The tooltips age once a temple turn: the temple interior's own 100 ms
  tooltip turn on the frame time (raffclar's) is gone, and `help::Process` no longer runs from the world's turns inside.
  Not ported: `GDebug::ClearMessages` and the paused cinematic. Entering from the frame's key pass ends no turn
  paused, so the ambience is not cut there; inside, only the temple's own turn drives the audio.
- `help::tooltips::ProcessTurn` keeps the icon while paused inside the citadel; the spirits' frame step reads
  `game_clock::IsInsideCitadel()` (the sequence mode) rather than the temple being active.
- **🔍 inferred** The escape menu, opened inside the temple by its options room, does not pause or unpause the game:
  the temple holds the pause. The original's options room has not been read.
- What is audible: the citadel's music and filters on the temple's 100 ms turns, and nothing of the world, which is
  paused (no world turn: no sound map, tags, villagers or animals), as in the original.

#### Audit: citadel interior

Reviewed against the disassembly: the whole of fn_00427200 (0x427209..0x42726D the uncapped copy and the cap of +0x50,
0x4272AD / 0x4272E1 the flag 0, 0x427321..0x42738E the returned capped point, 0x427399..0x427400 the uncapped
distance + offset), LHSampleUpdate3DChannels 0x10014310 (flag 0 → LHSampleStop and +0x18 = 0; `fcomp` +0x6C,
`test ah, 1`: a NaN does not stop; LHSampleSet3DPosition with the capped point), LHSampleSet3DPosition 0x10013AC0 (the QMixer
source at point + offset 0x10013E76..0x10013E99, +0x50 = the point 0x10013EBA), LHSamplePlayAnimEffect
0x10014B91 (the returned point goes into the options), ProcessCitadelMusic 0x427B60..0x427C8F (options +0x00/+0x04/+0x14/
+0x1C/+0x20/+0x24/+0x28; the bank it checks, GAudio+0xDC + 4·index, is that of type 44 + index), 0x4282F0
(`== 1`), ProcessMusic 0x427DF0..0x427EBB, 0x426C80, 0x64D6A0, 0x429F6D, 0x42A554, Temple fn_00793D00
(0x793D3C..0x793D59), StopPlayingSoundEffect 0x42A210, LeaveInsideCitadel 0x553B1F / 0x553B25, GoInsideCitadel
0x553E10..0x55405E (its calls: none for audio; fn_00463A50 → fn_00469E70 is for the citadel, not for sound),
fn_005C6CB0 and fn_005C61B0. Everything matches the code; nothing to fix.

- `ReadSpeedFactor`: the team's rule (step in double, result to float) could round twice in the qword `fmul`
  and `fadd`; checked **exhaustively** for the 2^23 floats of the second branch (b = 2 − 2r is exact; the
  double rounding only differs if the double falls on a float midpoint: none does) and the first branch is a
  single rounding (3 − 4r is exact in double): ✅ faithful for every READ_SPEED.
- No new AL sources or buffers, no ECS dependencies in `src/Audio`, no new music thread (`LHMusicPlay`
  from the game turn, like the other branches); the latch [0xC56164] is global as in the original.
- In game, measured before the world paused inside the citadel (to measure again: inside, the world's samples no longer
  start at all, and the hook's 80 turns inside are the temple's) (Land 1, `OPENBLACK_AUDIO_TEST_CITADEL="100,180"`, traces AUDIO/SFX/MUSIC/ATMOS): on entering `LHSampleStopAll` stops the
  tree creaks on channels 0, 2 and 4; inside no sample starts (all `filtered (inside the citadel…)`),
  citadel.sad (CITADEL_NEUTRAL) rises 4 → 127 while intro.sad goes down; on leaving `SFX: stop InGame.sad/2` and `/12`
  owner 0, `MUSIC_TYPE_SCRIPT_INTRO` comes back from chunk 1 abruptly (fn_00427CA0 passes fade 0, 0x427D41) and
  citadel.sad fades out, the ambience loops start again and the creaks sound again.

#### The temple interior's sounds

All in InGame.sad, all user parameter 2 (they pass the citadel's filter), all 2D and without an owner but where said.
The code: `3D/TempleSounds.h`, `TempleDoors`, `TempleToggles::ClickOf`, `TempleScrolls`, `TempleInterior::HoldControl`.

| Event | Sample | Call | Mode, loops | Where |
|---|---|---|---|---|
| A doorway starts to swing open (its swing goes from ≤ 0 to > 0) | 60 G_CitadelDoorOpen_01 | `PlaySoundEffect(0, 60, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794DAE (fn_00794D30) |
| A doorway turns round to close / is shut at once | 61 G_CitadelDoorClose_02 | `PlaySoundEffect(0, 61, 2, 0, 0, 0, InGame)` | 2, 0 | 0x794FED (fn_00794FB0), 0x794F8D (`InnerRoom::FastCloseDoor`) |
| A scroll starts to turn | 54 + GetTickCount() % 6, G_ScrollSqueak_01..06 | `PlaySoundEffect(0, 54 + t % 6, 2, 0, 0, 0, InGame)` | 2, 0 | 0x78429E (challenge), 0x7894C3 (creature), 0x78B64C, 0x79201E (save game) |
| The camera starts to look at a scroll (InnerCamera state ≠ 4) | 46 + (GetTickCount() & 3), G_Woosh_01..04 | `PlaySoundEffect(0, 46 + (t & 3), 3, 0, 0, 0, InGame)` | 3, 0 | 0x7957A6 (`InnerCamera::FocusOnSubMesh`) |
| A map button turned off / on | 63 G_CitadelButtonDown_01 / 62 G_CitadelButtonUp_01 | options: bank InGame, is3D 0, track 1 (the default), owner 0, mode 3, pitch below | 3, 0 | the ten callbacks at 0xC2A7A8 (0x79E940..0x79EDA0) |

- The scrolls' squeak: the flag (0xDD0BE8 in the challenge room, 0xDE0FA4 in the creature room) is cleared when the mouse
  does not move **and** when the clamped position does not change (0x7842AB), set after a squeak; a squeak only while
  it is clear. The pick is the clock, not a random stream: no random draw.
- The buttons, by their flag and tooltip: Temples (0xC2A4EC, 0xE91) 100 / 100; Creatures (0xC2A4F8, 0xE8F) down 110,
  up: 110 is written to the options' **owner** (+0x20) and the pitch stays 100 (0x79EAD5), an original slip, kept
  (`Owner::Key(110)`); Miracle activity (0xC2A4F0, 0xE93) 95 / 95; Influence (0xC2A4F4, 0xE92) 105 / 105, its on
  callback also sets `WorldRoom::ShowInfluence` and calls fn_0079DCD0; Challenges (0xC2A4FC, 0xE90) 108 / 108. The
  .sad has no pitch override for 62/63, so the caller's pitch plays (with deviation 0).
- From the .sad: 54..59 deviation 10 and mode 2 (0x400), 60/61 mode 2, 62/63 mode 3, 46..49 pitch 90 (0x1) deviation 10.

### Test hooks

- `OPENBLACK_AUDIO_TEST_CITADEL="<enter>[,<leave>]"`: enters the temple interior at hook turn `<enter>` and leaves
  it at `<leave>`, like `ENTER_EXIT_CITADEL(1)` / `(0)`; listed with the other audio hooks in
  [the audio engine's test hooks](../audio/engine.md#test-hooks).

## Pending

- fn_00427200 0x427227..0x42726D sets to 0 each coordinate of the channel's point (+0x50/54/58) whose absolute value
  exceeds 5000 before asking for the owner's position (ported as `audio::GuardSoundPoint`, see
  [the citadel interior](#the-citadel-interior-in-openblack)). LHSamplePlay compares |x| with 5000 (0x100114D1 …) for a
  log warning.

- The flag g+0x14 bit 0x400000 («always do a turn», **🔍 inferred**), the second `ProcessNetworkPackets` after drawing
  if g+0x205D58 (0x54D3C9) and the cinematic while paused (0x54CCCF..0x54CCEB): they do not exist in openblack. The
  temple's own 100 ms turn while paused (0x54CC88..0x54CCFF) is `game_clock::PausedTurnTimer` and
  `Game::ProcessTempleTurn` ([The citadel interior in openblack](#the-citadel-interior-in-openblack)).

- [The temple interior](#the-temple-interior), what is missing:
  - The hand's placement inside the temple is not ported: openblack's hand there is placed by raffclar's code. In the
    original, `TempleRoom::Draw` 0x799FE0 casts the cursor into the room (`InnerCamera::RayCast` 0x795980, called at
    0x79A199) and on a hit writes the point to [0xE360C8..0xE360D0] and InnerCamera+0x440 to [0xE360B8..0xE360C0],
    then copies [0xE360C8] to [0xE360D8] (0x79A1EB..0x79A208) **(inferred: the hand's point)**; the hand's state
    there is `HandStateCitadel` (`Enter` 0x5B0C10, `Update` 0x5B0D00). Neither has been read.
  - The save-game room: the original draws its reflection (`PictureRoomBase::Draw` 0x78F21F) and its floor at alpha
    0xD0; openblack draws neither (queued fix).
  - CitadelReflections and CitadelLightmaps: the original draws the main room's reflection only with both options
    on; openblack always draws it, which is the same only at the default settings (queued fix).
  - [The challenge list](#the-challenge-list): an UPDATE_SNAPSHOT or UPDATE_SNAPSHOT_PICTURE of a challenge with no
    record makes one whose unpopped fields (UPDATE_SNAPSHOT's camera, both functions' quest) are whatever the native's
    stack frame held: not determinable. openblack's record then has no camera and is no quest; the shipped scripts
    always take a SNAPSHOT of a challenge they update, so only the order at run time could reach it. The alignment
    history (`CAlignmentHistory::Add`) is not kept, as elsewhere; the picture itself (fn_00784690, the
    `screenshots_lores/hires` files) is its own row.
  - The Creature Cave's mist domes are drawn through openblack's land mist, lit by the land under them
    **⚠️ approximate**: how the original lights the cave's mists inside the citadel is not yet read.

### From the audio engine's list

- The citadel interior: the doors, buttons, scrolls and the scroll woosh play since step T6 of M4 (see "The temple
  interior's sounds" below). The creature room plays too (`CreatureRoom::DrawAdditional` 0x78869A 175
  G_FireCreatureCave at the fire's point [0xC5E454] and 0x7886E0 177 G_WaterCreatureCave 3D at (160, −45, −30), both
  options with track 0 and owner 0, every frame the room is drawn; its vfunc 11 0x7871D5 / 0x7871EC stops them): in
  openblack `TempleInterior::UpdateCreatureCave` plays both every frame the room is drawn and stops them once it is
  not. Still silent (rules read and ported unconnected, see "The challenge room's woosh, the heart's sparks and the
  worship strain" below): the woosh of `ChallengeRoom` 0x782486 (fn_00782410, a challenge chosen, 46 + tick & 3,
  mode 3, 2D: waits for the challenge room's controls, list, fade and script start) and the heart sparks 206 + c
  (0x468815, 0x468B32: wait for `CitadelHeart::GetActualObjectToEffect` 0x468C30, its spot visual and plasma, and a
  caller for `physics::temple_heart`). The worship strain (`Citadel::SetWorshipStrainSoundFrac` 0x463850) is not
  silent: it plays no sound itself and is already ported and connected (worship site remarks). Who plays 2 / 12 with owner 0 inside the temple (what fn_00793D00
  stops) is not in the inventory **(inferred: nobody in the original; the stop stays the same)**.
- If another island is loaded with the temple open, openblack does not call `TempleInterior::Deactivate` and
  `insideCitadel` would stay at 1.
