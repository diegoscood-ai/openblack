# Creature and interface sounds

## Summary

The creature's sounds (the sound events of its animations, its voice banks per species, the queue and its key)
and the interface's and the hand's (the knock on a building, the clicks, picking up a villager, the hand crossing the
land and the water). openblack plays the interface and hand sounds at the original's sites, and the creature's
sounds through raffclar's creature audio system with the original's keys, banks, gate and per-turn queue, the
Acknowledge of the leash's orders, the hair groups, the creed glow and the side effects after the sound; what is left
(the animations played by weight in whole ms and the Refusal call, both waiting for the creature's own ports) is in
Pending.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### The creature's sounds

Read in the v1.42 exe (bw1-decomp's `LH3DCreature::PlayASoundEffect` and `PlayQueuedSoundEffects` first;
`Morphable::CheckSounds`, `AddSoundEvent`, `AdvanceCyclic`, `AdvanceSimple` and `PrepareForDrawing` have no C body and
were read in the disassembly). Every number below was also run with the oracle (its scripts `t_add.py`,
`t_check.py`, `t_adv.py`, `t_draw.py`, `t_play.py`); `test_creature_sound_queue` pins them.

**The moments.** Each animation of a `.cbn` lists its moments in its extra data (`Morphable::ReadExtraDataBinary`
0x617EE0): kind (read only when the file's version is not 0), time in ms, value, mode and a "more" flag (five
`GetSegmentData` of 4 bytes, 0x617FB4..0x618003). A node is made only when the version is above 4 (0x618008) and is
put at the **head** of the animation's list (Morphable +0x4314 + 4·animation; node +0 kind, +4 time, +8 value,
+0xC mode, +0x10 next; 0x61802B..0x61804D), so the list runs from the last listed to the first. Every shipped
`Data\CTR\*.cbn` names its species bank: Bape, Bchimp, Bgorilla, Bmandrill `bape`; bbear, BPolarBear `bbear`; Bcow
`bcow`; Bgreek `bgreek`; Bhorse, bzebra `bhorse`; Blion, bleopard, btiger `btiger`; brhino `brhino`; bsheep
`bsheep`; btort `btort`; bwolf `bwolf` (a string search of the files).

**Queued once a creature turn.** `Creature::ProcessState` calls `LH3DCreature::UpdateTime` (vt +0x14, 0x473939) with
`ftol(creature+0x160 → +0x48 · [0xD01A38])` ms. UpdateTime first plays **every** sound still queued
(`PlayQueuedSoundEffects` 0x4818D0 from 0x4824AA, in queue order, then the count +0x47B4 = 0), then `OperateFace`
(0x4824B1), then advances the layers (`StateSwitch`, `StateAction`, the `Do…Action`s, `AdvanceMovementAnimations`,
`UpdateMotion`, `DoTheTurningBusiness`), each through:
- `AdvanceCyclic` 0x617A10 (anim, t, Δ): Δ = 0 → t, nothing checked; else CheckSounds(t, t + Δ); when t + Δ ≥ the
  duration (`*GetAnim(anim, 0)`), n = t + Δ − duration and, if n > 0, CheckSounds(t − duration, n); the result is n,
  held at duration − 1 (0x617A6A). So t + Δ equal to the duration gives 0, and the moment at 0 waits for the next
  call.
- `AdvanceSimple` 0x617A80: n = min(duration, t + Δ), CheckSounds(t, n), returns n (the end itself, not − 1).
- `CheckSounds` 0x617970 (anim, old, new): nothing when old = new; for each node with old ≤ time < new: kind 1 writes
  `*(Morphable+0x4804)[value] = (mode == 0)` (a hair group shown or hidden); kinds 0 and 2 call
  `AddSoundEvent(fraction, value, mode, kind == 2)` with fraction = fild(time − old) / fild(new − old)
  (0x6179CE..0x6179ED); other kinds nothing.
- `AddSoundEvent` 0x6178C0: nothing when 16 are queued (+0x47B4, 0x6178C9); else inserted **before the first entry
  whose fraction it is not above** (`fcomp`, `test ah, 0x41`, 0x6178E4..0x6178EF): among equal fractions the newest
  goes first. Entry (16 bytes at +0x46B4): +0 fraction, +4 action, +8 mode, +0xC generic. With the node list
  reversed, the moments of one CheckSounds at one time come out in the order the file lists them; those of a later
  CheckSounds at the same fraction go before them.

**Played in the frames.** `PrepareForDrawing` 0x4ED320 (vt +0x18, every drawn frame): f = g_game +0x205D64 (the
turn fraction, `game_clock::TurnFraction`), taken as 2.0 when it is ≥ 2.0 (the double at 0x8AC410, 0x4ED33A); it
plays the queued sounds in order while entry.fraction < f (0x4ED39C..0x4ED3A9), then moves the rest down
(0x4ED3ED..0x4ED434). Oracle, a queue at 0, 0.25, 0.5, 0.5, 0.99, 1, 1.5: f 0 plays none, 0.25 one, 0.5 two, 0.99
four, 1 five, 1.25 six, 2 and 5 all.

**One sound** (`PlayASoundEffect` 0x483290, thiscall (action, mode, generic)):
- MapCoords (ftol(x · 6553.6), 0, ftol(z · 6553.6)) of the 3D position +0x78 / +0x80 (0x4832A2..0x4832C6).
- No camera (`GGame::GetCamera` null, 0x4832D5): nothing at all, the dust and the footprint included.
- Distance = √((dz² + dy²) + dx²) from `LH3DTech::g_camera` (0xEA1DB8) to +0x78, summed depth first
  (0x4832FB..0x48332B; oracle: (0.1, 0.2, 0.3) to (0.7, −0.4, 1.9) gives 0x3FE7D15E, where width first would give
  0x3FE7D15F).
- Key: [clamp(ftol((2 − Size +0x90) · 1.5 + 1), 1, 3), clamp(ftol((1 − EvilGood +0x9C) · 1.5 + 1), 1, 3), sound
  object +0x4830, `GSoundMap::GetSurfaceType` 0x71D8E0, action] (0x483337..0x4833D6). Oracle: size 0.6666667 → 3,
  0.67 → 2, 1.3333334 → 2, 1.34 → 1; EvilGood −0.3333334 → 3, 0.3333334 → 2, 0.34 → 1. +0x9C is clamp(alignment,
  −1, 1), set once a turn ([The creature](../creature/overview.md)).
- The surface (`GSoundMap::GetSurfaceType` 0x71D8E0, every surface sound's): 6 off the map or in no block, 7 on
  water (`IsLand`), else +0x10 of the row `Terrain::GetMaterialInfo` 0x735330 gives (≤ 0 or ≥ 9 → 3). That row is
  `GetTerrainMaterial` 0x735380's: 27, snow, where `GClimate::GetSnow` 0x7715B0 (byte 6 of the cell's weather, the
  snow lying there) is 27 or more [0x999A10], else the island's material (0 → 1); a material past 43 reads row 0
  (0x735344). So a step on deep lying snow sounds of snow. openblack: `ecs::sea_cells::GetSurfaceType` through
  `terrain_material::AtCell` (test `WeatherTest.AStepOnDeepLyingSnowSoundsOfSnow`).
- Bank: generic → GAudio +0x3B8 (type 4, `creature.sad`); otherwise the species bank +0x5288, registered by
  `LoadBinary` from `sprintf("audio\sfx\creature\%s.sad", +0x5774)` (the .cbn's name as written; 0x4EBD81..0x4EBDD4).
- Gate (0x4833EA..0x483425): generic, or GScript +0x84 (`SET_CREATURE_SOUND`) ≠ 0, or this is the local player's
  creature (+0x4834 == g_game +0xA64 + 0xA60 · [g_game +0x205A59]). So `SET_CREATURE_SOUND 0` silences only the
  species-bank sounds of other players' creatures; the shared-bank ones (steps, blows, snores, eating) are always
  heard.
- `GAudio::SamplePlayAnimEffect` 0x42A4B0 (owner +0x4, distance, key, mode, bank, track 1, min 0, max 0)
  (0x48342B..0x483441); with a sample started and no owner, `SampleSet3DPosition` at +0x78 (0x483451..0x483469).
  The sample is picked in the bank's list with the audio library's own `Rand` (see "The anim-effects in the core"):
  no game stream is drawn for the sound.
- After it, heard or not (table 0x483600, jumps 0x4835EC): 3 → `fn_004ED250(3)`, 4 → 6, 5 → 12 dust puffs (three
  `LocalRand(201)` each, [Physics](../physics/physics.md)); 95..109 → fn_0068E6E0 (the creed glow on the hands,
  `HandGlows::SetScalePowerTime`); then for 3..5 the wet footprint of the lower foot (`LH3DWetFeet::Add` through
  fn_0081F360, size 4 · Size, species type from +0x4834 → +0x28 → +0x1F4) and, when that foot is below 1, the
  `FishRush` emergency point (0x4835B9..0x4835DB).
- Volume and pitch: none set here; the sample's own .sad values and the core's 800 / max-distance gates apply.

**Direct calls**, not queued: `Creature::CantFindRoute` 0x47543E plays 56 Refusal (mode 0, species bank) after
`FinishActionUnsuccessfully`; `Creature::ForceActOnObject` 0x4C6283 and `ForceActOnMapCoord` 0x4C6834 / 0x4C6BB1
play 55 Acknowledge (mode 0, species bank) each time, after an in-game bank sample through `PlaySoundEffect` 0x429E30
when the leash is the local interface's (0x4C6223..0x4C626A), then `StartConcurrentAction(200)`.

## openblack

### The creature's sounds in openblack

raffclar's port (`ecs::systems::CreatureAudioSystem`,
`src/ECS/Systems/Implementations/CreatureAudioSystem.cpp`, pure parts in `src/Creature/CreatureAudio.{h,cpp}`):
- Same keys, banks, gate and side effects: `creature_audio::Keys` / `SizeKey` / `AlignmentKey`, `IsHeard`,
  `creature_dust::PuffsFor`, `IsFootstep` (checked against the oracle in `test_creature_sound_queue`).
- **Direct calls**: `CreatureAudioSystemInterface::PlayNow(creature, action)` sounds a species-bank sound at once,
  with the camera check and the gate of `PlayASoundEffect`; the leash's ground order uses it for 55 Acknowledge
  ([Sent to a point of the land](../creature/leash.md#sent-to-a-point-of-the-land)) and so does its act on a thing,
  `ForceActOnObject`'s (`LeashSystem::ActOn`, so a field order is heard twice). `CantFindRoute`'s 56 Refusal is played
  by the locomotion's give-up (below).
- **Timing as the original** (with the animation's per-turn advance). At the creature's turn,
  `ecs::creature_loop::ProcessTurn` first plays every sound still queued (`CreatureAudioSystem::PlayQueued`, the
  original's `PlayQueuedSoundEffects` at the start of UpdateTime), then the animations move on once in whole ms
  (`CreatureAnimationSystem::AdvanceTurn`, [The creature](../creature/overview.md)), which gives the stretches each layer passed;
  `CreatureAudioSystem::QueueTurn` runs `CheckMoments` over each (`creature_audio::QueueTurn`): the face, the gesture
  and a blow's wobble, then the action, so a later layer's moments at one fraction go before an earlier one's, as
  `AddSoundEvent` puts them. Each frame `CreatureAudioSystem::Update(turnFraction)` plays those whose fraction is below
  min(turn fraction, 2), as `PrepareForDrawing`, each with its print and its dust. Nothing is fired per frame any more.
  The queue is `components::CreatureAudio::queued` (16 at most, the newest first among equal fractions); the creature
  debug window's Sounds part lists it.
- Animations played by weight (the walk, the run, the object actions; in ours they replace the action in the pose)
  still come from their own per-turn tracks in float ms: the heaviest one's moments are taken from where the same
  animation stood at the end of the turn before to where the turn leaves it (`LayerEvents`, the stretch wrapping round
  a loop's end as `AdvanceCyclic` does), and one just brought in passes none until the next turn. The original advances
  these in whole ms inside the same UpdateTime (`AdvanceMovementAnimations`, the `Do…Action`s, `UpdateMotion`); they
  join the whole-ms advance with their own ports.
- Distance: `creature_audio::CameraDistance(listener, position)`, summed depth first as the original does (the
  former `glm::distance` summed width first, an ulp apart in the cases tried; `test_creature_audio_system`).
  Listener = `audio::ListenerPoint` (the camera's point), position = the Transform, not the 3D object's +0x78.
- The creed moments 95..109: after the sound, heard or not, `CreatureAnimationSystem::PlayCreedEvent`
  ([the Creed glows](../creature/drawing.md#the-creed-glows-in-openblack)).
- The hair groups: a moment of kind 1 shows (mode 0) or hides a tuft as the creature's turn passes it, by its place
  among the file's groups (`ShowHairGroup` in `CreatureAudioSystem::QueueTurn`, kept in `CreatureHair::hidden`; the
  rig keeps each group's `fileIndex`, since groups with no strands are not loaded), and a hidden tuft is neither stepped
  nor drawn (`CreatureHairSystem::Update`, `creature_draw::BuildHair`), as the original's group step and draw skip a
  group whose first word is 0 (`fn_00847C90` 0x847CA2, `fn_00847CE0` 0x847CE3; `HairGroup::Init` sets it to 1 and the
  file never writes it). The moments one stretch passes are taken from the last listed to the first, so of two at
  different times the earlier-listed one wins. Tests `CreatureAudioSystemTest.AMomentShowsOrHidesATuftOfHairAsTheTurnPassesIt`,
  `CreatureHairSystemTest.AHiddenTuftWaitsWhereItWasAndMovesOnOnceShown`, `CreatureDrawHair.AHiddenTuftIsNotDrawn`.
- The Refusal direct call. `Creature::CantFindRoute` 0x4753F0 (the belief's navigation-blocked mark, the action's
  turns-since-done count to 0, `FinishActionUnsuccessfully("Cannot find route", 1, 1)`, then `PlayASoundEffect(56, 0,
  0)` 0x47543E) is reached from `LH3DCreature::FollowerCallbackFunction` 0x47F1F0 on the follower's plan ends 0 and 3
  (both from the follower's failure, `RouteFollower::OnFailed`: 0 the destination inside an obstacle, 3 a failed
  plan), and from `UpdateMotion` 0x4910E4 / `ProcessMoveStraight` 0x491630 when a move restarted from a place the
  creature cannot stand fails again (`StartMovingToPoint` ≠ 4). openblack: `CantFindRoute` in
  `CreatureLocomotionSystem.cpp` since 2026-10-28, at the end of the creature's locomotion turn, when the planning
  reads ends 0 or 3 (`creature_locomotion::PlanningOutcome`) or the restart after straying is refused: the mind's
  `AbandonAction` (what it was doing given up as a failure, as the freeze's `FinishActionUnsuccessfully(…, 1, 1)`),
  then `PlayNow(Refusal)`. Not ported with it: the navigation-blocked mark (openblack keeps no failed-belief memory,
  [mind](../creature/mind.md)) and the turns-since-done count (not kept either). Test
  `CreatureLocomotionSystemTest.WithNoWayThereItGivesUpWhatItWasDoingAndRefuses`. The version gate
  on the moments is never reached with the shipped files: `Morphable::ReadBinary` passes the first number of the
  block (0x617AF8, kept in [esp+0x58]), which is 21 in every shipped `.cbn` (their binary version is 6), above 4, so
  every moment is kept with its kind, as our loader reads them. Without a camera (no listener
  point) a queued sound does nothing at all, its print and its dust included, as the original's (0x4832D5;
  `test_creature_audio_system`). Ours only: the debug mute (`_muted`), and `VoiceBankStem`'s species table for a rig with no bank
  name (never reached with the shipped files) with its lower case and `.sad` strip.
- Side effects in the original's order (`PlayQueuedSound`): the sound, then, heard or not, the dust of a step or the
  creed glow, then a step's print; the print's lower foot, when below a height of 1 (0x4835A5..0x4835DB, the foot's
  y against 1.0), is also the fish's emergency point, `ecs::SplashWater` at the foot (`FootprintSystem::Step`,
  `creature_footprints::Splashes`), which every shoal in sight flees from at its next update, as the hand's splash
  (tests `FootprintSystemTest.AFootSetDownInTheWaterSplashesTheFish`,
  `FootprintSystemTest.AFootSetDownAtAHeightOfOneMakesNoSplash`, `CreatureAudioSystemTest.AFootstepsPrintIsLaidAfterItsSound`).
  The dust draws the local stream and the sound pick the audio library's, so the order of those two moves no draw.

### Interface and hand in openblack

Read in the disassembly: `Abode::InterfaceTap` 0x406830..0x406966 (with `Abode::GetAbodeType` 0x4061F0 and `Abode::InterfaceValidToTap` 0x406820),
`fn_004082F0` / `fn_00408340` (the SetupBox click), `GGame::Loop` 0x54D009..0x54D078 (the Logo),
`fn_0x005e5cd0` 0x5E61A1..0x5E6230, `fn_00827820`, `fn_00827210`, `InfluenceCircle::Draw` 0x826C90 / `Reset` 0x826C50 /
`Add` 0x826FA0, `GGame::Update3DInfluence` 0x5552A0, `GInterface::StartGrab` 0x5D1740 and `SendTap` 0x5D38A0.

#### What sounds now

| site | original | openblack |
|---|---|---|
| Click on a control of the game's own menus | `fn_004082F0` (from the SetupBox loop `fn_00408340`, codes 0xA/0xC at 0x408AA6, 0x408BBE, 0x408D6F): `G_MenuButton` InGame **159** 2D mode 3 + immersion 0x2C | `MenuClick` (declared in `src/Debug/Window.h`, defined in `src/Debug/Gui.cpp`): wraps each `ImGui::MenuItem`, `Button`, `Checkbox` and `Selectable` of the menu bar (openblack's own dialogs), plays the sample when the control reports a click and returns what it was given; the in-game menus and the skip box play it through `PlayButtonSound` (below). The immersion is not ported (there is no force feedback) |
| Knocking on the door | `Abode::InterfaceTap` 0x406830: only if `GetAbodeType() & 2` (LivingQuarters: houses A..F and mill), `G_KnockRoofMulti` **110 + [0xC4CC7C]** (0..8 rotating; confirmed: xref immediates), 3D with track 0, owner the house, at the hand's point (status +0xC8) | `AbodeKnockSystem::Tap` (`src/ECS/Systems/Implementations/AbodeKnockSystem.cpp`, `Locator::abodeKnockSystem`), reached from `HandSystem::Update` when the action button is pressed over a house: a house does not fit in the hand (`Object::ValidForPlaceInHand` 0x402870 = 0), so `StartGrab` taps it immediately, and the tap requires the hand inside the influence (`InterfaceMustBeInInfluenceForInteraction` 0x4028A0 = 1). It uses the counter `audio::Counter::KnockRoof` |
| Crossing an influence ring | `fn_0x005e5cd0` 0x5E61B0 per frame if not paused: `fn_00827820` checks the hand's point against the circles of `GGame::Update3DInfluence` (one per citadel and per town with influence) and, for each player whose "the hand is inside" changed, uses `fn_008277B0` to look for one of their circles whose edge the hand crossed between the previous point ([0xEA9EF0], which each call rewrites) and the current one; if there is one, it makes the ripple and sets [0xEB9A6C]; then `G_HandThroughInfluence_01` InGame **52** 3D (track 0, no owner) at the hand + immersion 6 | `influence::ProcessHandCrossing` (`src/ECS/Influence/InfluenceCircles.cpp`), called from `InfluenceSystem::Update` (`src/ECS/Systems/Implementations/InfluenceSystem.cpp`) with the hand's point if the game is not paused. The `.sad` gives it mode 1 (new channel per crossing), volume 40 and min/max 100/300: entering and leaving sound the same, and a circle that appears, grows or shrinks under a still hand does not sound (no edge is crossed). `influence::HandCrossedInfluence` is `fn_00827820` without the sound |
| Screams when picking up a villager | `GInterface::GenericPickup` 0x5D28C5..0x5D295D | already there (`HandHolding.cpp`): two tags, `G_PickUpObject` 10 and 180/187/194 + `GetRandomSample(7)` |

| Conquering a town | `G_TakeOverTown_01` InGame **205** (fn_00649810, from `SetTownEmpty` 0x7410DA) | `town_belief::TakeOverTown` (`src/ECS/Town/TownBelief.cpp`): the local player winning a town from another hears it, 2D and untracked (`HearsTakeOver`); tests `TownBeliefRules.OnlyTheLocalPlayerWinningATownHearsIt`, `TownBeliefTest.TakeOverTownDoesTheRest`. Ported from raffclar's PR #236, not checked against v1.42 yet |
| Command acknowledged | `G_AcknowledgeCommand` InGame **1** at 0x5D2E2C, 0x5D3044, 0x5D4162 (the leash's orders) | `AcknowledgeOrder` in `src/ECS/Systems/Implementations/LeashSystem.cpp`, from `LeashSystem::ActOnPoint` and `LeashSystem::ActOn`: sample 1 for the local player's creature, then the creature's own Acknowledge 55 through `CreatureAudioSystemInterface::PlayNow`; tests `LeashSystemTest.TakingAnOrderOnTheLandItAcknowledgesIt`, `LeashSystemTest.SentOntoAFieldItAcknowledgesTheOrderAndAgainAsItIsToldToActOnTheField`, `LeashSystemTest.ToldToActOnAThingItAcknowledgesTheOrderEveryTime` ([the leash](../creature/leash.md#sent-to-a-point-of-the-land)) |
| Chest and scrolls | `Reward::InterfaceTap` 0x6E5D00 (173 `G_ClickSignpost`, 40 `G_RewardSting`, 41 `G_OpenChest`, all in the in-game bank and 2D; confirmed: xref immediates); `ScriptHighlight::SetActivated` 0x70A630 / `InterfaceTap` 0x70AC70 (134 `G_ClickOnScroll`, 173) | `RewardSystem::Tap` (ported from raffclar's reward-chest lane, unverified against v1.42) and `script_highlight` (`src/ECS/ScriptHighlight.cpp`): SetActivated plays 134 through the options variant with the in-game bank, the highlight as owner, 2D (options +0x08 = 0) and the rest at the constructor's defaults, except for a "did you know" sign (0x70A671); the tap's 173 too. Test `ScriptHighlight.ActivationPlaysTheClickOnScrollButNotForADidYouKnow` |
| Camera out of bounds | `G_OutOfBounds` InGame **43**: `CameraModeNew3::Update` (fn_0045FA00) pushes a camera point outside the fence back (InsideInclusion 0x45FA31), then immersion 0x2A and `PlaySoundEffect(0, camera point, 43, mode 2, 0, 0, 3D, InGame)` (0x45FC5F) | `DefaultWorldCameraModel::KeepInsideFence` (`src/Camera/DefaultWorldCameraModel.cpp`, the rule in `camera_zones::PushInsideFence`): the camera put back plays 43 at the camera, mode 2, 3D, with the shake; no force feedback. Test `CameraZones.ACameraOutsideTheFenceIsPutBackPastWhereItCrossed` (it pins the sample 43) ([player camera](../camera/player-camera.md#what-the-player-camera-does-with-the-zones)) |

`PlayButtonSound` (`src/Gui/GameInterface.cpp`) is the original's `fn_004082F0`: `G_MenuButton` (159), mode 3, 2D, the
in-game bank; the force feedback (`StartImmersion(0x2C, 0x80000000)`) is not ported. The skip box calls it too, on every left
release, over a control or not (🔍 inferred, as the menu's; see Pending).

- G_HandThroughInfluence: `GGame::Update3DInfluence` makes the circles (on a turn that is a multiple of 10, with the
  dirty byte set) only while the player's border is shown; without it a crossing makes no ripple and no sound
  (0x8278E0..0x8278EC).

`Abode::GetAbodeType` 0x4061F0 reads the house's `GAbodeInfo` +0x120; openblack stores the `AbodeNumber` and the mesh, so
`abodes::TypeOf` looks up the record by those two (like `influence::AbodeInfoOf`) — **🔍 inferred**: all records
for the same house number carry the same LivingQuarters bit.

The rest of the real `Abode::InterfaceTap` is in `AbodeKnockSystem::Tap`: [0xC4CC6C] = the tapped town
(0x40683F), `HowManyPeople::KnockKnock` 0x829690 ([the hand](../hand/animation-and-placement.md), "The population
read-out"), `Villager::SetStateWhenTappedOnAbode` 0x752B80 of the inhabitants (+0xA0) and the hand's animation 0x39
(`CHand::StartFixedPosAnimation` 0x46C050, `Ctap_house`). Each inhabitant reacts before the type test, so every abode's tap reaches them.

#### Audit: interface and hand

Checked in the disassembly: 0x406820 (`mov eax, 1`), 0x406830..0x40694A (Abode vt +0x8C4 = `GetAbodeType`
0x4061F0, `test al, 2`, 0x6E + [0xC4CC7C] wrapping to 0 at 9, +0x20 owner, +0x08 = 1, +0x0C = 0, point status
+0xC8), 0x4082F0 (0x429DA0 with owner 0, 0x9F, mode 3, loops 0, +0x10 0, 2D, bank +0x3AC; immersion 0x2C), 0x5E61A6
(pause), 0x5E61B0..0x5E621E (0x34 = 52, +0x20 = 0, 3D, track 0, sound only if [0xEB9A6C]), 0x827820, 0x8277B0,
0x827210 (strict), 0x5552A0, 0x5508A0 (stops at the type-3 player), 0x5D1740 (Abode vt +0x6FC = 0x402870,
+0x714 = 0x4028A0), 0x5D38A0 (+0x740 = 0x406820, packet 0x20), 0x828A50, 0x883120.

- **Fixed**: the crossing sounded every time a player's "inside" bit changed, also when a circle appeared or
  grew under a still hand; the original requires `fn_008277B0` (a circle of the player with the previous point [0xEA9EF0]
  and the current one on different sides of the border). Now that previous point is stored and the circle is searched for.
- **Fixed**: the comment and the wiki said that `InfluenceCircle::Draw` sets the latch [0xEB9A1C]; it only reads it.
- **Fixed**: the knock and crossing tests copied the call instead of exercising the code; now they call
  `AbodeKnockSystem::Tap` (house and town centre, which stays silent) and `influence::ProcessHandCrossing` on the registry
  (entering, staying, leaving, growing circle, neutral town), and the ten-tap one does not depend on the starting value
  of the static counter.
- Unchanged: the menu click (asked for in the game's own menus), no new ECS dependency in
  `src/Audio`, no new AL source/buffer, no music thread. `audio::OnThingDeleted` is called only as a creature leaves the game (`src/ECS/CreatureRemoval.cpp`); other channels follow `ownerPosition`, which with entt's versioned ids gives nullopt for a deleted entity.
- In game (`OPENBLACK_TEST_KNOCK="10,0,6,0.4"`, `OPENBLACK_SFX_TRACE=1`): the ten
  taps give 110..118 and 110, 3D track 0, owner the house (cut by the maximum 150 with the camera at 220).

#### Tests and game

- `test/audio/test_ui_sfx.cpp` (11 tests, each read of InGame.sad with a synthetic twin; the ones that read InGame.sad find the game through the test helper
  `GameRoot` and `OPENBLACK_TEST_BW_ROOT`, and skip themselves without it; the 3D cull measures from a camera at the
  origin, as `GAudio::PlaySoundEffect` measures from the camera at 0x429E6F): the real user parameters from InGame.sad of the interface samples
  (0 in 42/134/159/160/173/205/40/41/119, 1 in 1/52/110..118, 2 in 46, 4 in 129) and the mode 1, volume 40 and
  min/max 100/300 of 52; **tapping a house ten times gives 110..118 and goes back to 110**, 3D at the hand's point with
  max 150; inside the citadel neither the `MenuButton` (parameter 0) nor the knock (1) sound but the woosh (2) does; 52
  takes a new channel per crossing (mode 1 from the `.sad`) and is cut beyond 300 from the camera.
- In game (Land 1, hook `OPENBLACK_TEST_KNOCK="10,0,6,0.4"` with
  `OPENBLACK_SFX_TRACE=1`): the ten taps give `InGame.sad/110, 111, 112, 113, 114, 115, 116, 117, 118, 110`, 3D with
  track 0, owner the house and at its point. During the **whole** Land 1 intro the script has the widescreen on (the
  narration continues at 75 s), so GAudio filters them by user parameter 1 — as the
  original would; the audible start is in the test.
- Picking up a villager **sounds twice** (`OPENBLACK_TEST_PICK_VILLAGER="0,6"` with
  `OPENBLACK_TEST_VIEW_VILLAGER="0,5,0"`): `InGame.sad/10 G_PickUpObject` → channel 113 and `InGame.sad/187 G_PickUpMan_01`
  → channel 132, the two 3D point tags on the villager (with the camera far away they are cut by their maximums 180 and 160).
- The influence crossing triggers by itself: two `InGame.sad/52` with **mode 1**, 3D, track 0 and no owner
  while the intro camera drags the hand across the edge of a town's circle (filtered by the
  widescreen, just like the original). The menu click cannot be tested without moving the mouse.

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

### (Approximate) and 🔍 inferred: interface and hand

- **(Approximate)** with the hand off the land `InfluenceSystem::Update` does not call `ProcessHandCrossing` (there is no point); the
  original keeps passing the last [0xE9A100].
- **(Approximate)** the hand's point for the crossing comes from `HandSystem::GetPlayerHandPositions()[0]`; the original uses
  [0xE9A100], which `GLandscape::Draw` 0x5E4395 fills from the hand of `MyInterface()`.
- **(Approximate)** the menu click: openblack has no SetupBox; its in-game menus (`src/Gui/Dialog.cpp`, `GameMenu`) sound when a control acts on a release (`PlayButtonSound`, like the original's code 0xA), and the debug bar's ImGui menus through `MenuClick`.
- **(Inferred)** all records of an `AbodeNumber` carry the same `ABODE_TYPE` (above).
- The "there is already state from the previous frame" latch ([0xEB9A68]) is process-static, as in the original (nothing
  clears it between maps).

- The creature ([above](#the-creatures-sounds)): still to do are the whole-ms per-turn advance of the animations
  played by weight (the walk, the object actions, the fight's moves; their sounds are queued per turn from their float
  tracks meanwhile), which waits for those animations' own port in the creature's turn, and the Refusal 56 direct call,
  which waits for the creature's route give-up (both in [the openblack section](#the-creatures-sounds-in-openblack);
  progress/audio/sound_effects.md, #164). Done and moved out of here: the per-turn queue, the Acknowledge of the
  leash's orders, the side effects after the sound, the hair groups, the creed glow 95..109, the nearby enemy creatures
  of the heartbeat (0x71C2A6..0x71C379, [Chants and heartbeat](magic-and-weather-sounds.md#chants-and-heartbeat-in-openblack))
  and the fight music ([Fight music](music.md#fight-music)). `SET_CREATURE_SOUND` 0 mutes only other players'
  creatures' species-bank sounds, as his scope does; his species fallback table is never reached (every shipped .cbn
  names its bank).

### Interface and hand sounds with no place in openblack yet

- **Logo** InGame 160: `GGame::Loop` 0x54D011 starts it once ([0xBEC27C]) and **stops** it on returning from
  `DoLogo` 0x5FA070 (0x54D073). openblack does not have that logo sequence, so adding it would sound different (it
  would sound without the logo); it is left for when it exists.
- **`G_ClickOnSpell_01` 42** from the arena (`ArenaSpellIcon` fn_00425660): openblack has no arena. The leash post's
  (`LeashObj::InterfaceTap` 0x464490) is there: `worship::temple_leash::InterfaceTap` (`src/Worship/TempleLeash.cpp`)
  plays it, 2D and of no owner, for the local interface when a post is picked (`leash_posts::Tap`,
  `k_TapSample`). The worship icon one (0x726430 → fn_00726490, with its pitch) is there too
  (`Worship/WorshipSpellIcon.cpp`).
- **Virtual influence** `G_VirtualInfluence_04` 129 (`GVirtualInfluence::Draw` 0x76D000, `Start` fn_0076CED0,
  `SetSoundFraction` 0x76CF90, `Stop` fn_0076CF60; 2D loop with pitch = fraction·100): the strength is ported, and
  so are the hum's rules as pure functions (`virtual_influence::HumPlays` and `HumPitchPercent`,
  `src/Common/VirtualInfluence.cpp`), but nothing calls them: the draw, and with it the hum and the mana path sprites,
  is not called yet.
- The menu click: `RussClickNoise` 0x4082F0 (`PlaySoundEffect(0, 159, 3, 2D, InGame)` and a force-feedback effect)
  has three callers, all in `SetupBox::DrawAll`: a double click with a callback and a focused control (0x408AAA,
  message 0xC = DOUBLE_CLICK, left or right, 0x61A2F3 / 0x61A322); a whole click between two frames (0x408BED); a
  release over the focused control (0x408D82, message 0xA = MOUSE_UP), sliders included. The keyboard never clicks.
  openblack (Dialog.cpp, GameInterface.cpp) has the release and the whole click; still missing: the double click,
  static texts that take the hit (only SetupStaticTextNoHit refuses it), the first control of an overlap rather
  than the last (unless a later one is on top), and the skip box's click on a release away from any control.

### From the audio engine's list

- The interface action states FIGHT_APPLY 0x16 and FIGHT_SELECT 0x17: `GameQueries::interfaceState` gives 0x10 in
  the creature fight state (`ecs::audio_queries::LocalPlayerDirectsFight`, see [Fight music](music.md#fight-music)),
  so the samples of userParam 4 (such as `G_VirtualInfluence_04`) are quietened there. The fight's presses and their
  states 0x16 and 0x17 are the hand's since 2026-10-29 ([the player's presses](../creature/fights.md#the-players-presses));
  `interfaceState` still gives 0x10 for those two as well, where the original gives 0x16 or 0x17.
- The original's menu (autosave 20 and boxes on top of the world) if it is ported.
- The pause: not settled, which key pass of the original takes a given key (the frame-start pass or the turn's
  ProcessGameInputs), and so whether the player's own pause key or `GoInsideCitadel` cuts the ambience, depends on when
  in the frame it comes. openblack takes both in the frame's pass (no cut); the end-of-turn rule itself is done (see
  "Pause" above).
- The scream when picking up a villager: alive = `LifeOf > 0` (a valid entity is taken as available) **🔍 inferred**.
- The collision distance comes from the `GameQueries` camera (`LH3DTech::g_camera`) **🔍 inferred**.
- The `SetFlight` woosh measures from the current origin of the previous frame's camera (FlyToPosFoc reads the
  camera's position, +0x118) **🔍 inferred**.
