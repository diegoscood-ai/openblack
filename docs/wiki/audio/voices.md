# Voices and texts

## Summary

The voices of the original: the text voice table (6974 texts with their bank and sample), the narration of RUN_TEXT and
the time a text stays to be read, the two advisors' sentences with their lip sync and cue labels, Guidance (the
remarks about towns, resources and the player's actions), the confirmations and the night's spooky voices. openblack
plays all of them on the 16 channels with the original's rules; the open approximations are listed in Pending.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Voices and texts

#### Text voice table

**Faithful.**
- The table is at 0x915D40, with identical copies at 0x942B38 (SAY), 0x957310 (GConfirmation) and 0x96BA30 (GGuidance).
  They are `{u32 id, u32 bank, u32 sample}` entries, 0x1B3E = **6974** texts (`HELP_TEXT_LAST`).
- The index is the position of the `ADD_TEXT` in `Scripts\InfoScript2.txt`. bw1-decomp's `HelpTextEnums.h` is from another
  version and drifts from 1009 onwards.
- **3477 texts have a voice**: 1922 in HelpSprites (6), 1328 in villagers (7) and 227 in Guidance (10). None in
  VillagersBanter or in SpellDialogue.
- **Where each text's voice goes**, over the whole table (checked 2026-10-29 from the frozen exe's table at 0x915D40,
  the rule of fn_005C5F90 0x5C5FD3..0x5C60DB and `Scripts\InfoScript2.txt`): 1092 texts go to the good advisor and 830 to
  the evil one (all 1922 HelpSprites texts have narrator 2 or 3, so none of them is narrated), 1555 are narrated (the
  villagers' and Guidance's, including the 2 villagers texts of narrator 2) and 3497 have no voice. Every spoken GUIDE
  (116) and MONK (24) text is in villagers, so they are narrated whatever their narrator's number. The table's copies
  at 0x900D40, 0x942B38, 0x957310 and 0x96BA30 are byte-identical to it. Both advisors play from HelpSprites
  (GAudio+0x3C0, set once at 0x5C58C8 through 0x5C3660). The bank paths (0x9CB3F8, registered once by 0x42A390 from
  `InitSFX`) and `scripts/InfoScript2.txt` (0x719280) are fixed: there is no per-land or per-language switch.
  openblack's table built from the data hashes to the exe's own table (FNV-1a 64 0xCD886474B759EFCC, test
  `VoiceTable.InstalledGame`), and its routes to the exe's rule (0xCF876EA5A63C093C, test
  `VoiceTable.InstalledGameRoutes`, with the synthetic twin `VoiceTable.InstalledGameRoutesSynthetic`).
- **It is rebuilt from the data**: the name of the text is the name of its sample's .wav. It is looked up first in
  villagers, then in HelpSprites and then in Guidance. There is a single duplicate: `HELP_TEXT_LAND_2_WORKSHOP_10` is in
  HelpSprites 801 and in villagers 399, and the exe gives villagers 399. That villagers goes first is deduced from that single case
  **🔍 inferred**.
- **Narrators** (InfoScript2): 0 NONE, 1 DEFAULT, 2 GOOD_SPIRIT, 3 EVIL_SPIRIT, 4 WOMAN, 5 MAN, 6 OGRE, 7 KHAZAR,
  8 LETHYS, 9 NEMESIS, 10 BOY, 11 BIG_VOICE, 12 TRAINER. `ADD_TEXT` also uses GUIDE and MONK without declaring them; their value is
  **🔍 inferred**, and openblack gives them −1.
- `HelpTextDatabase` 0xD17CA8 stores {+0 narrator, +4 arg0, +8 text} (fn_005CAD00). The conversion fn_007191F0 is applied
  to the text: `~` → 0xF8FE and `\n` → line break.
- **Fixed channel owners**:

  | owner | who |
  |---|---|
  | 0x270C | advisors (HelpDude) |
  | 0x270D | SAY with alt |
  | 0x270E | only stopped by STOP_SOUND_EFFECT(isSay); nobody plays it |
  | 0x270F | narration and SAY without alt |
  | the sample number | script PLAY_SOUND_EFFECT |

- In the original's say table every entry has id == index, so the id test of SaySoundEffect (0x70F910) never zeroes the bank.
- STOP_SOUND_EFFECT(isSay) takes the narrator from HelpTextDatabase[0 < id < count ? id : 0] (0x70FA8D..0x70FAAD), and
  reads the say table 0x942B38 without checking the id's bounds (0x70FAB3 / 0x70FAB9).

- +0x14 (0xD99BB4) (in the code: `lastBetter`)
- +0x18 (0xD99BB8) (in the code: `active`)
- +0x1C (0xD99BBC) (in the code: `lastSay`)

#### Narration and reading time

**Faithful except where marked.**
- **`RUN_TEXT`** (`GScript::RunText` 0x6F7D60): an id ≥ 6974 gives "Invalid text" and becomes 0. With `singleLine` or with the
  flag +0xB0, `ClearAllText` 0x5C5550. Then it goes to fn_005C5F90:
  - it shows the text and starts the reading time (fn_005C6100 → fn_005C61B0);
  - it stores it in the queue of 6 (+0x584..+0x598) and in the history of 1024 (fn_005C5EE0);
  - and it says the voice:
    - bank 6 with narrator 2 or 3 → the good or evil **advisor** (`HelpDudeControl::Say` 0x5C36D0, with a delay of
      0..500 ms and owner 0x270C);
    - another bank with a sample → `GAudio::PlaySoundEffect` 2D, owner 0x270F, +0x164 = 1.
- **`IsTextRead`** 0x5C64E0 → 0x5C6340:
  - With `withInteraction` it waits for the click.
  - If the text has a voice (fn_005C62F0: entry, bank, sample and the bank registered):
    - advisor: the text is read when none is speaking or went quiet less than 200 ms ago (fn_005BB730);
    - narration: while 0x270F is playing, end = now + 450 ms.
  - If it has no voice: inside the citadel, by ms; otherwise, by turns.
- **Reading time without voice** (fn_005C61B0): `s = (readDefaultWordGTTime·n + readDefaultAdjustGTTime) · msPerTurn ·
  0.001 · f(READ_SPEED)`.
  - The two constants are 5 and 8 in info.dat 0x4AEC4, in `HelpSystemInfo` 0xD1617C/0xD16178. The values come from
    pattern matching **🔍 inferred**.
  - `msPerTurn` = [0xD01A38] = 100 (0x54F4A5).
  - f(r) = r ≤ 0.5 ? 3 − 4r : (1 − 2(r − 0.5))·0.8 + 0.2 (fn_005C6CB0), with READ_SPEED = 0.5 by default.
  - n = words (fn_005CBEC0 over the splitter fn_005CB590).
- **Click** (`HelpSystem::ProcessInterface` 0x5C69B0, from `GInterface` 0x5D11C0, every frame): with unread text and
  (widescreen or the key [0xE85410]):
  - the key is `LHKeyboard::KeyState[0x9C]` (the array at 0xE85374; 0x9C = 0x1C + 0x80, `KB_ENTER` with the extended flag: the **keypad's Enter**), written 1 on its press and 0 on its release by `ProcessKeyboard` 0x7DCAF0, so it is read **held**, every frame; the click and the key are or-ed, and the key also passes the text's least time on the screen and the widescreen test.
  - the click is the left button's **release**, not its press: `GInterface` +0x39 bit 0x20, which the function reads,
    is set only by message 2, the left binding's release (0x471170, then 0x5D9900). openblack keeps the release seen
    in `ProcessEvents` (`help::dialogue_click::IsClick`, pinned by test_dialogue_click) and gives it at the next
    frame's interface pass. (pending) the testbed cannot click the dialogue yet: its pointer's buttons go to the hand
    directly, not through the game's mouse events;
  - it stops the spirits (fn_005C6720, unread);
  - it does `StopPlayingSoundEffect(0, 0x270F, 7)`: **only villagers**, with a 20 ms ramp (DLL 0x10012C50).
- `GAME_CLEAR_DIALOGUE` 0x6FF6F0 and `GAME_CLOSE_DIALOGUE` 0x6FF700 clear the text, but **do not stop the voice**.
- **There is no ducking** of the music by the voices: the music only changes with the options. This is faithful in the exe; what
  happens inside the DLL's thread is **🔍 inferred**.

#### Advisors, Guidance, confirmations and night voices

**Faithful in the original.**.
- **Advisors**:
  - `HelpDudeControl::Say` 0x5C36D0: v = |+0x3514| − 0.95 (0.95 at 0x915438); delay = v < 0 ? 0 : min((v + 1)·250,
    500) ms (+0x3514 = the advisor's floating position, **🔍 inferred**).
  - `HelpDude::SaySentence` 0x5BB340 → `UpdateSaySentence` 0x5BB610 (per frame) → `PlaySample` 0x5BB530
    (`LHSamplePlay` directly, without filters, with the PCM for the mouth).
  - Lip-sync: `ApplyLipSync` 0x5BCD00 and `AutoVoiceParams::CalcKey` 0x428850. CalcKey uses the FFT `AudioAnalyse::Analyse`
    0x428C60 + `four1` 0x428D50, which have not been dumped.
  - `IsTalking` 0x5BB760 and `StopSentence` 0x5BB840.
  - Spirit effects: `HelpDude::PlaySoundFX` 0x5C2800 (bank `GetSoundFXBank` 0x5BC7C0): `SamplePlayAnimEffect` with the
    key {1, 1, 17, 1, sound id}, action (flag == 0), track 0, the dude as owner and its camera distance; 155 plays
    G_SpriteKnockGlass 162 and 154 G_SpriteShootGun 161. Played in openblack (`help::spirits::Runtime`), details in
    [The anim sounds](../interface/advisors-and-help.md#the-anim-sounds).
- **GGuidance** (`Guidance.sad`, 0x71AB10..0x71D490): villager reactions and advisor comments.
  All read; the details are in
  [Guidance and night voices in openblack](#guidance-and-night-voices-in-openblack).
  - 33 types with an interval table 0x980190 {base, help level, always}.
  - `PlayNow` 0x71AF50: a non-"always" type stays silent on **Land 1 of a single-player game that is not the
    playground** (g_game+0x205A08 == 1, `IsMultiplayerGame` 0x552F80, +0x205A0C); then the help level
    (HelpSystem+0x45F8 ? +0x45F4 : 0) and turns since the last time > interval = base + rand(5·base·(1 − r³))
    (0x71AEE0, drawn on every call). `PlaySample` 0x71C6F0.
  - Types 9..30 (and 31, 32) make an advisor speak: `HelpSpiritSay` 0x71D270 starts the help script
    `MultiHelpJustTalkWithText(text, text)` (HelpSystem.txt), which waits for the dialogue, does RUN_TEXT and waits for it to be read.
  - `BeliefSFX` 0x437F40 → 0x71BF70 (thresholds 0.05/0.4/0.7, max 200).
  - Heartbeat: InGame 45 looping, 3D at the **citadel** if its heart (+0x30) is alive, max 500, pitch = 30 + 70·v
    smoothed 0.1 (0x71C460, already ✅ faithful: 0x8BF51C, 0x92B2C8, 0x8AB22C).
- **GConfirmation** (START_ANGLE_SOUND 285 / "START_ANGLE_SOUND" 348 = `StartPitchSound`):
  - Init 0x71A560, Start 0x71A610, Process 0x71A650.
  - HelpSprites 1686 (BETTER_14), 1673..1685 and 1704..1717.
  - It never says "no": it is a bug in the original and it is copied.
- **GSpookyVoices** 0x72E2A0..0x72E870: at night (real clock 20:45-20:59 or 23:00-05:59) it whispers the profile's name
  chosen by Soundex among the 100 `HELP_TEXT_SPOOKY_NAMES_*` (details in
  [Guidance and night voices in openblack](#guidance-and-night-voices-in-openblack)).
- The **creature does not speak**: `creature.sad` and the per-species banks are effects **🔍 inferred**.
- **Advisors, in detail.**
  - `HelpDudeControl::Init` comes from `HelpSystem::CallVirtualFunctionsForCreation` 0x5C5860 → fn_005C3660 →
    fn_005BB060. It sets the bank (+0x2EFC) and the sample count (+0x2F00, 0x5BB0C1).
  - HelpDudeControl lives as long as HelpSystem: `HelpSystem::Reset` does not touch it, and `Uninit` 0x5C5680 deletes it.
  - UpdateSaySentence is called from `HelpDude::Update1` 0x5BDE41 and from the control's loop 0x5C3CB3
    (0x5C3B05..0x5C3CBC). A sentence starts once GetTickCount ≥ its start tick (signed compare, 0x5BB62B).
  - Sentence globals: start tick 0xD15A98 (GetTickCount at PlaySample, 0x5BB55A); PCM 0xD15A94; frames 0xD15A8C =
    bytes / (2 · nChannels); rate 0xD15A88; duration 0xD15A90 = frames / rate (0x5BB5CC..0x5BB5FE). The previous PCM
    copy is freed first (0x5BB568); the spectrum buffer is 0xD14084 (2 × 0x200 floats).
  - A dude's play options hold the bank at +0x3680, the sample at +0x36A0 and keep-PCM at +0x37E0 (0x5BB39C..0x5BB3C0,
    0x5BB53A).
  - The last-talk tick is +0x37EC. TalkingOrJustStopped tests it with an unsigned compare < 200 ms
    (0x5BB749..0x5BB756). IsTalking refreshes it (0x5BB7A7) and calls StopSentence once the sentence is over (0x5BB7B6).
  - +0x2F0C holds the sample and its AudioTags (`BuildAudioTags` 0x42AE70 over the wave's tags, info+0x78 / +0x7C;
    0x5BB669..0x5BB71F).
  - StopSentence also calls fn_005BCBC0(duration + 100, 0) (0x5BB87B..0x5BB88E).
  - VoiceKey at HelpDude+0x2F60 (ctor fn_005C2D50): +0x00 offset in ms (0, 0x5C1EC8), +0x04 threshold 0.05, +0x08 level
    2.5, +0x0C rate 40, +0x10 the three bands; the band total is kept at +0x4C (0x42891E).
  - The key time +0x2F70 is the play position · 0.001 when that position is ≥ 0, and then the vowels fn_005BF810 run on
    +0x2F60 (0x5BCD6C..0x5BCDDB). Otherwise it is the tick time.
  - CalcKey's window: centre sample = (offset · 0.001 + dt + t) · rate (0x428860..0x428874); start = centre − window /
    2, clamped to 0..frames − window (0x428886..0x428892).
  - `BandLevel` fn_00428A80: from = ftol(low / rate · n), to = ftol(high / rate · n), each capped at n
    (0x428AAB..0x428AB5). The level is the sum of spectrum[from..to) divided by (to − from) (fidiv 0x428ADC).
  - `Amplitude` fn_005BB420(sample, window) = √Σ(s / 32768)² (3.0517578e-5, 0x8C5848) over `window` PCM samples,
    starting at ftol(t / duration · frames) with t = (GetTickCount − start) · 0.001. It gives 0 for sample 0, for t < 0
    or t ≥ the duration, without PCM, or when the sample is not playing on HelpSprites with owner 0x270C (0x5BB497).
  - Say's constants: 250 (0x8C7B2C) and 500 (0x8C78EC). It sets +0x74 + 4 · dude = 1 at 0x5C373B.
- **GConfirmation, in detail.**
  - A static at 0xD99BA0. Init 0x71A560 (from InitOneTimeOnly 0x54F03D) zeroes it, and `GGame::ProcessTurn` calls
    Process at 0x54E731 once per turn.
  - Fields: +0x08 the watched value (g_angle 0xC5E158 or g_pitch 0xC5E15C), +0x0C the range, +0x14 the turn of the last
    "better", +0x18 active, +0x1C the turn of the last sample.
  - START_ANGLE_SOUND (285, 0x70FFA0) watches the turn with range 7 (0x70FFB7; confirmed: xref immediates) and clears
    g_angle (0x70FFC1). CHL 348 (StartPitchSound 0x70FFE0) watches the tilt with range 5 (0x70FFF7; confirmed: xref
    immediates) and clears g_pitch (0x710001). The third argument both pass to Start (1) is not read by it (0x71A610).
  - Start 0x71A610 sets the value, the range and active, with +0x1C = +0x14 = 0. `GConfirmation::Stop` 0x71A640 clears active;
    `CleanGameForScriptReboot` calls it at 0x6EBD18, so a START_ANGLE_SOUND 285 / 348 still on goes off.
  - The camera feeds both values with fn_00454900 / fn_00454930: g = (a / b − g) · 0.4 + g. Here a is the frame's turn
    (or tilt) and b the camera's seconds for the frame (GetCameraTimeInc · 0.001); a = 0 on frames with no turn
    (0x46053F / 0x46055C).
  - Process 0x71A650: v = clamp(value / range, −1, 1), with a NaN giving −1; a = |v|. a == 1 and more than 100 turns
    since the last "better" (0x71A6BC) → BETTER_14 1686 (fn_0071A850; both turn stamps are set). a > 0, more than 20
    turns since the last sample (0x71A901) and a > LocalFloatRand(1) (drawn only at that point) → 1704 + LocalRand(14)
    (0x71A741..0x71A752).
  - It plays with Init's options: HelpSprites, volume 100 (+0x28), +0x2C 90, owner 0, 2D, no track; through 0x429E30
    (0x71A76B / 0x71A869).
- **GGuidance, in detail.**
  - Constants: 0.3 desire threshold 0x98012C, 200 desire distance 0x980130, 300 thing turns 0x980134, 600 forget turns
    0x980138; 0.55 / −0.55 alignment class 0x98014C / 0x980150, 200 attack 0x980158, aggression 1 0x98015C, 200 belief
    0x980164, 0.3 visibility 0x980168; 100 / 400 creature 0x98016C / 0x980170, 300 hand range 0x980174, 0.95 / 2 /
    0.75 / 0.4 alignment 0x980178..0x980184; heartbeat max distance 500 (`push 0x43FA0000`, 0x71C5D7), G_HeartBeat 45
    (0x71C600 / 0x71C63B); the strings "MultiHelpJustTalkWithText" 0xBF1988 and "…WithNoText" 0xC22254.
  - `Close` 0x71ABF0 is called from GInterfaceStatus::UnInit 0x5DD226. Init is also called from ResolveLoad 0x5DD194.
  - LastThings (+0x90 / +0x94, {thing, turn}, newest first): TimeSinceThingSeen fn_0071AE10 gives the turns since the
    thing was first seen. Past 600 it starts again (0x71AE65), and a new thing is added and gives 0
    (0x71AE99..0x71AEAA).
  - `DesireSample` fn_0071AA90: x = v − LocalFloatRand(v / 3). x ≥ 0.85 / 0.65 / 0.45 (0x980324 / 0x980320 / 0x98031C)
    picks the desire's text 0 / 1 / 2; otherwise 0.
  - `DesireScore` fn_0071B410 = 2 · t0 · s · v³ · (1 − d²) · (1 − t1³), with t0 = min(TimeSince(TownDesire) / 50, 1)
    and t1 = min(TimeSinceThingSeen / 300, 1) (fn_0071AE10 is called a second time for the value kept); d =
    min(distance / 200, 1) and v = min(value, 1); s = t0³ when the sample is the last desire's (+0x98), otherwise 1.
  - CheckWorshipSiteDesiresSFX draws both texts with the food value (`push ebp` twice, 0x71B33E..0x71B34F).
  - The town desire plays at volume 127, pitch 100, +0x2C 90.
  - `GetRandomSampleBasedOnValue` clamps the value to 0..1 first (0x71D320..0x71D358).
  - PlaySample's point is (x / 6553.6, GetAltitude + y, z / 6553.6) (0x71C75D..0x71C7C1). lastPlayed is written only
    for a type < 33. In 3D without a point nothing plays and lastPlayed is not updated (0x71C757).
  - The 22 HELP_SPRITES_GUIDANCE lists are objects at 0xD99BD8 + 0x98 · k.
  - GetGuidanceResourceType of each class: food (1) for Pot food 0x71BDEF and Animal 0x71BE10; wood (2) for Pot wood,
    Tree 0x71BE20 and DeadTree 0x71BE30.
  - `StrongestEffect` fn_0071BE40: the strongest of effects 0, 1, 2 and 4 that is above 0 (effect 0 scaled by 0.001
    (0x8AC418) and capped at 1); 7 when there is none.
  - Attacker samples (GetSampleForAttack vt +0xDC, 0x71BC20..0x71BD50): a plain thing gives 0; a spell of type 6
    (fn_0072B200) gives LIGHTNING_01..10 = 0x132E + LocalRand(10), other spells 0; a rock gives ROCKS_01..10 = 0x1342 +
    LocalRand(10); a creature gives a MONSTER text. The attacker is EffectValues +0x28; the caused player is
    `EffectValues::GetCausedPlayer` 0x525910.
  - HelpSpritesTownBeingAttacked reads that player's aggression at +0x9F8 + 0x80 · n.
  - `BeliefSFX` starts the strongest of the 8 beliefs (GBelief +0x8) at 0 (0x437F41..0x437F67). A believed-in player
    that is not below the strongest says nothing (0x437F84).
  - `BeliefSample` fn_0071BF70: good: ≥ 0.7 GOOD_AWE_01, ≥ 0.4 _02, > 0.05 _03; evil: the EVIL_AWE texts the same way;
    an alignment other than 1 / 2 is a coin toss, LocalRand(2) ? 2 : 1 (0x71BFB5); the last belief sample is +0x9C
    (0x71C154).
  - Who asks for it: `GBelief::AddToBelief` 0x437EB0, whenever belief is added with a thing (0x437F1E..0x437F2A), after the belief sprite: `BeliefSFX(player, thing +0x14, alignment)` with the town's current beliefs (GBelief +0x8, not the pending ones), and the sample's distance is from the local interface status's MapCoords (+0x14, the synced hand point) to the thing (0x71C065..0x71C073, `fn_0074CD50`, the same plane distance as `GetDistanceInMetres`). Its callers with a thing: a resource given to a town's store (the store as the thing, alignment 1) and a villager impressed by a miracle (the villager, the reaction's alignmentForSFX).
  - Heartbeat pulse: fn_0071C450 = +0xAC; fn_0071C430 = +0xB0 + (+0xAC − +0xB0) · g_game+0x205D64 (the turn's
    fraction).
  - fn_0071C3F0(v) sets the heartbeat override +0xB8 (0 = follow +0xA4). Callers: CitadelHeart fn_00465C70 (0x465DA3,
    0x466489).
  - HelpSpritesCheckMoonPhase constants: IsVisualNight 0x5575E0, π 0x8C36A0, 12000 0x9804C8, 600000 turns = 0x927C0.
  - DeathInVillageSFX is called from `Villager::VillagerDead` 0x7507B8 for a villager of the local player killed by
    another player (the death table's +0xC, 0x99A370). It plays 0x1658 (DEATH_IN_VILLAGE_06) + LocalRand(5) at
    volume 127.
  - Type 31 "MultiHelpJustTalkWithNoText" has no caller.
  - `HelpSystem::RunMessage` with first > last does nothing (0x5C8CEE).
  - `GRand::LocalFloatRand` returns a float (0x6DE5E3: the caller stores it).
  - The HelpSprites functions, with caller and list: TownBeingAttacked 0x71C870 → list 0; CreatureAttackingThem
    fn_0071C960 (Town::UpdateAggressor 0x73CAFA; needs [0xC221CC]) → list 2; LosingVillagers 0x71C990 (VillagerDead
    0x7508C5; the villager within 300 of the hand) → list 3; AttackingTown fn_0071C9F0 (0x73CB24; a town not of the
    local player, with people) → list 4; LowOnFood 0x71CA60 (Town::CalculateDesireForFood 0x747FA0; by value) →
    list 5; LowOnWood fn_0071CAF0 (0x7481BC) → list 6; InjuredPeople fn_0071CB80 (no caller) → list 7; LowOnPeople
    0x71CBE0 (VillagerDead 0x7508EF) → list 8; VillagerUnhappy 0x71CC40 (TownDesire::Process 0x745C8A; the value is not
    used) → list 9; LosingBelief fn_0071CCC0 (fn_00438340 0x4383C4) → list 10; OtherVillages fn_0071CD00 (no caller) →
    list 11; CreatureFight fn_0071CD40 (CameraModeNew3::StartFight 0x45A772 / 0x45A7B2) → list 12; GeneralBad
    fn_0071CD70 (fn_004383D0 0x43872B, the "Fight" action's running sub-state 0x503830 at 0x5039E7) and fn_0071CDF0 (0x438755, 0x50394F), which need the
    point on screen (fn_0081F1D0) → lists 13 / 14 (read 2026-10-31 from info.dat's HELP_SPRITES_GUIDANCE rows: list 14
    holds HELP_TEXT_BATTLE_ALERT_WINNING_EA_01.. and _GA_ texts, list 13 the BATTLE_ALERT_LOSING_EA ones with
    BATTLE_ALERT_EA_11 and _17: the winning and losing messages; no other code names those texts); KillingPeople fn_0071CE70 (VillagerDead 0x75078B; the villager's
    bounding box on screen, fn_0081F1A0) → list 15; WorshippersDying fn_0071CFE0 (VillagerDead 0x75087C, death
    reason 4) → list 18.
- **Night voices, in detail.**
  - `GSpookyVoices::Init` is called from `GGame::InitOneTimeOnly` 0x54F024. GetPlayerName 0x72E870 is called from
    `GGame::Init` 0x54FD10. fn_0072E280 (called from fn_0054EB40 0x54EC24) frees the options.
  - CRT calls: time() 0x7C79FD, localtime 0x7C789D, _isalpha 0x7C686F. SoundExCode indexes its jump table with
    (c | 0x60) − 0x61.
  - The countdown is reset to 100 and then decremented, so it stores 99 (0x72E3A2..0x72E3A8). The night test
    fn_0072E3B0 is at 0x72E3C5..0x72E3E9.
  - `Process` does nothing on lands 1 and 2 (0x72E31E / 0x72E327). `SoundexOverlap` tries the name's words in turn
    (0x72E6F3..0x72E727).

- 0xD14084: the spectrum buffer of ApplyLipSync / fn_005BCC40 (2 x 0x200 floats) (in the code: `spectrum`)
- HelpDude::PlaySample 0x5BB530: +0x164 = 1, LHSamplePlay (no GAudio filter); on a start the tick 0xD15A98, the PCM the DLL kept (info+0x80, +0x84 bytes, copied) and its frames = bytes / (2 * nChannels), rate and duration (in the code: `PlaySample`)
- +0x3680 bank, +0x36A0 sample (0x5BB39C..0x5BB3C0) (in the code: `PlaySample`)
- 0x428C61..0x428C93: the window's step 1 / (n * 32768) (0x8C49EC) as a float; the window itself on the FPU, at 24 bits (fn_007DEE00) so a float (in the code: `lip_sync::MagnitudeSpectrum`)
- The window of the lip-sync analysis: 0x200 samples (ApplyLipSync 0x5BCDA6) (in the code: `lip_sync::k_Window`)
- +0x04 (0x3D4CCCCD): a total band level below it is silence (in the code: `threshold`)
- +0x10: 200..400 Hz x 0.85, 400..700 Hz x 1.1, 700..10000 Hz x 10 (0x43480000, 0x43C80000, 0x3F59999A, 0x43C80000, 0x442F0000, 0x3F8CCCCD, 0x442F0000, 0x461C4000, 0x41200000) (in the code: `bands`)
- HelpDudeControl::Say fn_005C36D0(dude, sample, onlyIfSilent): v = |dude+0x3514| - 0.95 (double 0x915438 = 0.95f); the delay is 0 for v < 0, else min((v + 1) * 250, 500) ms (0x8C7B2C, 0x8C78EC); HelpDude::SaySentence; +0x74 + 4 dude = 1 (+0x3514 is the advisor's hover x, given once a frame by help::spirits through SetHover; 0 until then, 0x5C1A61) (in the code: `Say`)
- fn_00454930 (in the code: `FeedPitch`)
- 0x70FFC1 (in the code: `StartAngleSound`)
- fn_0071A730 -> fn_0071A8F0(a): the turns first, then the draw (in the code: `Step`)
- 0x71A700: fn_0071A7F0, 1689 + LocalRand(14), unreachable (a = |v|) (in the code: `Step`)
- GConfirmation (SoundConfirmation.cpp, the static at 0xD99BA0; research dev\docs\collected\documentacion\intro\spec_jc_specials.md section 11): the good spirit's "yes" while the player turns or tilts the camera the way a tutorial asks (TeachRotate / TeachPitch in HandDemos.txt). START_ANGLE_SOUND 285 (GScript::StartAngleSound 0x70FFA0) watches the turn, CHL 348 (also named START_ANGLE_SOUND, GScript::StartPitchSound 0x70FFE0) the tilt; GGame::ProcessTurn 0x54E731 runs Process once a turn. The camera feeds the two values (CameraModeNew3, fn_00454900 / fn_00454930).
- [0x963DA4] ROTATION_BETTER_14 ("Estupendo"), fn_0071A850 (in the code: `k_Better`)
- fn_00454900(a, b) / fn_00454930(a, b) (CameraModeNew3): g = (a / b - g) x 0.4 + g, each step a float; a is the turn (tilt) of the frame and b the camera's seconds of the frame ([esp+0xAC] = GetCameraTimeInc x 0.001), or a = 0 on the frames without one (0x46053F / 0x46055C). (pending) the seven call sites wait for the port of CameraModeNew3::Update (in the code: `FeedAngle`)
- 0x71B14F: the best distance starts at 200 (0x980130); 0x71B17B..0x71B1C2: a town with a storage pit and people (in the code: `CheckTownDesires`)
- 0x71B2AB: no citadel (GPlayer+0xA48) (in the code: `CheckWorshipSiteDesires`)
- 0x71B319..0x71B332: fcomp 1 (0x8AA390), test ah, 1: kept when below or unordered (a NaN too), else 1 (in the code: `CheckWorshipSiteDesires`)
- 0x71B3A2..0x71B3B7 (in the code: `CheckWorshipSiteDesires`)
- 0x71B3D2..0x71B3E1: the need wins whenever it scored, even below the food's (in the code: `CheckWorshipSiteDesires`)
- 0x71D276 (at `const auto script = type == Type::JustTalkNoText ? k_ScriptWithNoText : k_ScriptWithText;`) (in the code: `guidance::HelpSpiritSay`)
- 0x71B41A..0x71B465: t0 = min(TimeSince(0) / 50, 1), kept as a float (at `float t0 = static_cast<float>(TimeSinceLastPlayed(Type::TownDesire)) / k_DesireTurns;`) (in the code: `guidance::DesireScore`)
- 0x980180 / 0x980184 (in the code: `guidance::UpdateAlignmentRemarks`)
- fn_0071C810 (Villager::VillagerDead 0x7507B8) (in the code: `DeathInVillage`)
- HelpSpiritSay's "MultiHelpJustTalkWithNoText" (0x71D276; no caller found) (in the code: `JustTalkNoText`)
- The 22 lists of HELP_SPRITES_GUIDANCE (info.dat GHelpSpritesGuidance, the objects at 0xD99BD8 + 0x98 k): HELP_TEXT ids, the list ends at the first 0 (fn_0071D390, at most 0x22 = 34) (in the code: `SpriteList`)
- GGuidance::Close 0x71ABF0 (GInterfaceStatus::UnInit 0x5DD226): the options freed, the LastThings list emptied (in the code: `Close`)
- GGuidance::ProcessTownDesireSFX 0x71B020 (every 10 turns, PlayNow(TownDesire)): CheckTownDesiresSFX 0x71B130 and CheckWorshipSiteDesiresSFX 0x71B270 (GameQueries::desireTowns / worshipSites) choose a sample and its value; above 0.3 (0x98012C), x = value - LocalFloatRand(value / 2) and the sample plays 3D at the thing with maxDistance 200 x (0x980130), volume 127, pitch 100, +0x2C 90; +0x98 = the sample (in the code: `UpdateTownDesireRemarks`)
- fn_0071C960 (Town::UpdateAggressor 0x73CAFA): [0xC221CC] && HelpSpritesPlayNow(CreatureAttackingThem) -> list 2 (in the code: `RemarkCreatureAttacking`)
- [0xC221CC] (in the code: `State::enabled`, `SetEnabled`) has two writers (xref): `GGuidance::Init` sets it to 1 (0x71ADD5), and the script native `SET_CREATURE_IN_TEMPLE` (GScript 0x6FF8C0) stores the value it pops both into `CreatureRoom::CreatureInTemple` [0xD96270] (0x6FF8D1) and here (0x6FF8D6). Its readers are 0x71C930 (no caller found: CreatureBeingAttacked, list 1) and 0x71C960 above. openblack's native does the same.
- HelpSpritesLosingVillagers 0x71C990(villager) (Villager::VillagerDead 0x7508C5): the villager within 300 (0x980174) of the hand (GInterface+0x3B8, fn_00605CD0) -> list 3 (in the code: `WarnLosingVillagers`)
- fn_0071C9F0(town) (Town::UpdateAggressor 0x73CB24): a town not of the local player (g_game+0x205A59), with people -> list 4 (in the code: `RemarkAttackingTown`)
- fn_0071CAF0(town, value) (0x7481BC): the same with list 6 (LowOnWood) (in the code: `WarnLowOnWood`)
- fn_0071CB80(town) (no caller found): people, within 300 of the hand -> list 7 (in the code: `WarnInjuredPeople`)
- fn_0071CCC0(town) (fn_00438340 0x4383C4): people -> list 10 (in the code: `WarnLosingBelief`)
- fn_0071CD00(town) (no caller found): people -> list 11 (in the code: `RemarkOtherVillages`)
- fn_0071CD40(arena) (CameraModeNew3::StartFight 0x45A772 / 0x45A7B2) -> list 12 (in the code: `RemarkCreatureFight`)
- fn_0071CD70(MapCoords) (fn_004383D0 0x43872B, the fight sub-state 0x503830 at 0x5039E7: no opponent left) / fn_0071CDF0 (0x438755; 0x50394F: the opponent knocked out, not script-controlled); both only for the local player's creature. Ours: `CreatureFightSystem::Win` (good) and the duel whose opponent has gone (bad): the point on screen (fn_0081F1D0: GameQueries::pointOnScreen) -> list 13 / 14 (in the code: `RemarkGeneralBad`)
- fn_0071CE70(villager) (Villager::VillagerDead 0x75078B): the villager's object on screen (fn_0081F1A0: the bounding box, GameQueries::thingOnScreen) -> list 15 (in the code: `RemarkKillingPeople`)
- fn_0071CFE0 (Villager::VillagerDead 0x75087C, death reason 4 of a villager of the local player) -> list 18 (in the code: `WarnWorshippersDying`)
- 0x72E2A0..0x72E306 (at `state.spooky.bank = Bank(SfxBank::Guidance);`) (in the code: `spooky::Init`)
- time() 0x7C79FD (in the code: `UnixTime`)
- localtime 0x7C789D of UnixTime (in the code: `LocalTime`)
- GSpookyVoices::GetPlayerName 0x72E870 (GGame::Init 0x54FD10): the sample = GetName again (in the code: `UpdatePlayerName`)

#### The guidance across a land change

`GGuidance::Init` 0x71AC70 has two callers (xref): `GInterfaceStatus::Init` 0x5DD1CB (from `GInterface::Init`
0x5CE687, from `GPlayer::Init` 0x64929B: a new game) and `GInterfaceStatus::ResolveLoad` 0x5DD194 (a saved game's
load). A land change makes neither, so the guidance goes on with the interface status into the next land: every
type's last play (+0xC.., the last land's turns, now ahead of the turn the load puts back to 0, so `turn - last`,
unsigned, is past every interval), the remembered things (+0x90 with their count +0x94), the last samples, the heart
beat's value, pitch, phase, pulses and smoothed believers and share (+0x98..+0xC8), the one-off flags (+0xD0..+0xEC)
and the enabled switch [0xC221CC] (`SET_CREATURE_IN_TEMPLE`); and `Init`'s `LocalRand` draws (one or two a type,
0x71ACDD / 0x71ACFA) are not made. Of `GGame::ClearMap`'s call tree (0x552BB0, 723 functions eight calls deep) only
the status pass's `ProcessHeartBeatSFX` 0x71C190 touches the guidance ([clearing on a new
land](../hand/states.md#clearing-on-a-new-land)): the guidance calls of `Villager::VillagerDead` that
`GScript::DeleteAllScriptCreatedGameThings` could reach through `Villager::ReleaseFromScript` 0x7531D0 do not run,
since that returns at once while the map is cleared (g_game +0x14 & 0x8000, 0x7531D9); and the guidance's other
statics (the moon's countdown [0xC221D0], written only by `HelpSpritesCheckMoonPhase`) have no writer there.

The remembered things are `LastThings` nodes 0x71AEC0 {thing, turn}, the thing a pointer to the `GameThingWithPos`
seen, compared by address alone (fn_0071AE10). The clear frees the last land's things and leaves the nodes, so on the
new land a node matches a new thing only when the allocator gives the new thing a freed one's address: then the node's
turn is the last land's, and the new thing counts as seen that many turns ago when the new turn is no more than 600
past it (unsigned), else as just seen; no node is added either way. **Which new thing that is depends on the
original's memory allocator and cannot be reproduced** (see openblack, below).

## openblack

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**Narration and reading time**

- `GameActionInterface::IsKeyHeld(SDL_SCANCODE_KP_ENTER)` as the dialogue's `skipKey`, and `ProcessClick(false)` every frame (Game.cpp, the interface's frame pass);

**Advisors, Guidance, confirmations and night voices**

- [Voices on channels](#voices-on-channels-in-openblack) and [Guidance and night voices](#guidance-and-night-voices-in-openblack).
- `town_stores::AddToBelief` → `audio::guidance::PlayBeliefRemark`, the interface's point from `HandSystemInterface::GetSpellInfo` (the status's map point). Every call that reaches `PlayNow` draws the local stream twice (the interval), and once or twice more when it is time to speak.

### Voices and texts in openblack

- **`helptext`** (`src/Common/HelpText.{h,cpp}`):
  - `Entry {arg0, narrator, name, text}`: the name is needed by the voice table; the original does not store it.
  - `Parse` (UTF-16 with or without BOM, narrator names resolved with the file itself), `ConvertScriptText`
    fn_007191F0, `GetEntry`, `Count`.
  - The texts are read from the interface's text database (`gui::TextDatabase::AddHelpScript`, which parses with
    `Parse`) once the help texts' slot has it (`HelpTextSystemInterface::SetTexts`, set when the interface is made and
    taken away before it goes); else from the help text cache, loaded on first use. The patch texts (`GetPatch`) are
    always read from the cache.
  - `k_TextCount = 0x1B3E` and `k_NarratorGoodSpirit/EvilSpirit = 2/3`.
  - A narrator number is read like `_wtoi` **🔍 inferred**: the reader `LHScriptX` 0x7E7960 has not been read.
- **`audio::VoiceTable`** (`src/Audio/Services/Voices.{h,cpp}`): `Build` with the rule above and `Get`. `voices::BuildTable`
  is called in `Game::Initialize` with the wave names that `Game.cpp` reads from banks 7, 6 and 10. There are also
  `VoiceOwner` (0x270C..0x270F) and `TextVoice::HasVoice` (fn_005C62F0).
- **`help::HelpSystem`** (`src/Help/HelpSystem.{h,cpp}`, **the text part**, in `help::DialogueText`,
  `src/Help/DialogueText.{h,cpp}`, which the help system makes and forwards to and the help texts slot
  `HelpTextSystem` owns; `ProcessInterface` is `DialogueText::ProcessClick`):
  - `RunText` 0x6F7D60, `RunTextWithNumber` 0x6F7C70, `TempText` 0x6F7E40, `TempTextWithNumber` 0x6F7F50,
    `ClearDialogue` 0x6FF6F0, `CloseDialogue` 0x6FF700.
  - `SayText` fn_005C5F90, `AddText` fn_005C6100, `ClearAllText` 0x5C5550, `ClearTextDisplayed` 0x5C54E0, `Reset`
    0x5C5580.
  - `IsTextRead` 0x5C64E0, `ProcessInterface` 0x5C69B0.
  - `CountWords` fn_005CBEC0, `ReadSpeedFactor` fn_005C6CB0, `RouteOf` (the branches of 0x5C6025..0x5C60DB) and the history
    fn_005C5EE0/fn_005C5F50.
  - **Nothing is drawn**: `OPENBLACK_TEXT_TRACE` writes each text to the log.
  - The voices go through the hooks `sayVoice`, `stopVoicesOnClick` and `interruptAdvisors` and the queries `voiceBankLoaded`,
    `advisorsTalking` and `isPlaying`, which `Game.cpp` connects to `audio::voices` and `help::AdvisorVoices`
    ([below](#voices-on-channels-in-openblack)).
  - The click is the left button's release, given at the next frame's interface pass (see the click above).
  - The scaled clock 0xEA1C78..0xEA1C80 is real ms since startup **⚠️ approximate**.
  - `+0x460C` (`GInterface::SetActive`) is not ported.

- `HasVoice`: The part of fn_005C62F0 that reads the table (0x5C630A..0x5C631C): a bank and a sample

### Voices on channels in openblack

Read in the disassembly: 0x5BB060..0x5BB8A7, 0x5BCD00, 0x5C36D0..0x5C3842, 0x5C52C0/0x5C5290, 0x5C6020..0x5C60DB,
0x5C6A7E..0x5C6AB4, 0x5C6E20, 0x70F8E0, 0x70F9B0, 0x710280, 0x710350, 0x710C40, 0x428850..0x428EE1 (CalcKey,
fn_00428A80, Analyse, four1), the HelpDude init 0x5C1EA1..0x5C1F24 and, in the DLL, 0x10012BF0..0x10012F13 (LHSampleStop),
0x10014C00 (LHSampleGetPlayPosition) and 0x10015180 (LHSampleGetPercentageDone).

#### API

| function | original | what it does |
|---|---|---|
| `voices::RunTextVoice(route, voice)` | fn_005C5F90 0x5C6025..0x5C60DB | the route of `help::RouteOf` (HelpSprites with narrator 2/3, the rule kept only there): the good or evil advisor: `AdvisorVoices::Stop(1)`, `Stop(0)` and `AdvisorVoices::Say(0/1, sample, 0)`; the narration (any other voice with bank and sample): default options, owner 0x270F, +0x164 = 1, 2D, through `GAudio::PlaySoundEffect` (with its filters) |
| `voices::Say(text, withPos, alt, point)` | SaySoundEffect 0x70F8E0 | table 0x942B38; without a sample, nothing; owner alt ? 0x270D : 0x270F; 3D if withPos (the point only then); track 0; +0x164 = 1. No text and no advisor, even if it is HelpSprites |
| `voices::IsSaying(alt, text)` | 0x710280 | fn_0042A280(alt ? 0x270D : 0x270F, sample, bank) |
| `voices::CutByClick()` | 0x5C6AA4..0x5C6AAD | `StopPlayingSoundEffect(0, 0x270F, VILLAGERS)`: all villagers narration, with the 20 ms ramp |
| `voices::BankRegistered(bank)` | fn_005C62F0 0x5C631E | GAudio+0x3A8 + 4·bank ≠ 0 |
| `AdvisorVoices::Init(bank)` | fn_005C3660 → fn_005BB060 | the HelpSprites bank and its number of samples in both advisors |
| `AdvisorVoices::Say(advisor, sample, onlyIfSilent)` | HelpDudeControl::Say 0x5C36D0 | delay = v < 0 ? 0 : min((v + 1)·250, 500) ms with v = \|+0x3514\| − 0.95 (0x915438); SaySentence; +0x74 = 1 |
| `AdvisorVoices::SaySentence(advisor, sample, onlyIfSilent, delay)` | HelpDude::SaySentence 0x5BB340 | g_speaker = advisor; with a sentence playing, nothing if onlyIfSilent and it is speaking, otherwise StopSentence; sample in 1..n; start at GetTickCount + delay |
| `AdvisorVoices::UpdateSaySentence(advisor)` | UpdateSaySentence 0x5BB610, from Update1 0x5BDE41 and the control's loop 0x5C3CB3 | the line starts once its tick is reached (PlaySample 0x5BB530: `LHSamplePlay` directly, owner 0x270C, without GAudio's filters; copies the PCM) |
| `AdvisorVoices::ApplyLipSync(advisor, dt)` | ApplyLipSync 0x5BCD00, from Update1 0x5BEB05 (its only caller) | the sound part of the lip sync, run by the spirit's update (`AdvisorSpirit::UpdateAnimStack` through `Queries::lipSync`) with the control loop's dt; the dispatch below |
| `AdvisorVoices::IsTalking`, `TalkingOrJustStopped`, `PercentageDone`, `StopSentence`, `Stop` | 0x5BB760, 0x5BB730 (+200 ms), 0x5BB7C0, 0x5BB840, fn_005C3750 | as in the original, with g_speaker 0xD15AA0 and g_sentence 0xD15A9C as members of the class (globals in the original) |
| `AdvisorVoices::Interrupt(advisor, arg)` | fn_005C3780 (from fn_005C6720 → fn_005C4C20) | picks a HELP_TEXT_INTERRUPTION_* (0xE3F/0xE4C + rand 5, or 0xE44/0xE51 + rand 8 with arg 0 and LocalRand(2) = 0), Stop(advisor) and, outside the citadel, if PercentageDone < 0.9 (0x915440), says it |
| `AdvisorVoices::AnyTalking()` | 0x5C6372..0x5C63A0 | +0x74 && fn_005BB730(0) \|\| +0x78 && fn_005BB730(1): the advisor branch of IsTextRead |
| `AdvisorVoices::GetLipSyncKey`, `Amplitude`, `lip_sync::UpdateKey` (was `CalcKey`, then `ComputeLipSyncKey`), `MagnitudeSpectrum` (was `Analyse`), `Fft` (was `Four1`, then `FastFourierTransform`), `BandLevel` | +0x2F60, fn_005BB420, 0x428850, 0x428C60, 0x428D50, fn_00428A80 | the mouth, without drawing it (below) |
| `help::script_spirits::SpiritWhoTalks`, `help::script_spirits::HelpSpiritOf` (was `ConvertScriptSpiritToHelpSpirit`) | 0x5C6E20, 0x710350 | SPIRIT_SPEAKS |
| `sample_play::PlayPosition`, `PercentageDone`; `SampleOutput::StopRamped`, `PlayPositionMs` | 0x10014C00, 0x10015180, 0x10012C50 | position in ms, fraction done, the LHSampleStop ramp |
| `audio::SetBankSampleCount` / `BankSampleCount` | LHBankGetNumberOfSamples | the upper bound for SaySentence |

- `AdvisorVoices::Interrupt`: fn_005C3770
- `AdvisorVoices::Amplitude`: 0x5BB497: LHSampleIsPlaying(GAudio+0x3C0 = HelpSprites, 0x270C, sample) (at `if (!(_audio.isPlaying && _audio.isPlaying(static_cast<uint32_t>(sample))))`)
- `voices::RunTextVoice`: 0x5C609C..0x5C60A2 (at `if (route != help::VoiceRoute::Narration || voice.sample == 0 || voice.bank == SfxBank::None)`)
- `Say`: GScript::SaySoundEffect 0x70F8E0(text, withPosition, alt, LHPoint*) (GAME_PLAY_SAY_SOUND_EFFECT 0x70F9B0): text 0 for one >= 6974 (0x70F8EA); the say table 0x942B38: nothing without a sample (0x70F90A), the bank 0 when the entry's id is not the text (0x70F910, never in the original's table); a default LH_SamplePlayOptions with the bank, the sample, the owner alt ? 0x270D : 0x270F (0x70F931..0x70F938), is3D +0x08 = withPosition, the point +0x30 only when withPosition (0x70F970..0x70F986), track +0x0C = 0 (0x70F98F), +0x164 = 1; GAudio::PlaySoundEffect 0x429E30. No text and no advisor, even for HelpSprites.

#### How it works

- **RUN_TEXT** (HelpSystem::SayText): the `sayVoice` hook calls `RunTextVoice` with the route `RouteOf` gave for the
  text's narrator, or for the debug Consciences window's speaker override when one is set (test
  `VoicesTest.AForcedSpiritsRouteReachesTheVoice`).
  **TEXT_READ** with a voice: advisor → read when none is speaking or went quiet less than 200 ms ago; narration → +450 ms
  after the last time it sounded (the queries are connected).
- **Click** (`ProcessInterface`, with the script widescreen or the key): `interruptAdvisors` (the help texts' `InterruptAdvisors`:
  `AdvisorVoices::Interrupt` of the good advisor, then the evil one, both with arg 1) and then `CutByClick`. Only villagers is cut; the generic HelpSprites and Guidance narration
  continues. As in the original, the interruption sentence **is never said**: fn_005C3780 reads PercentageDone after Stop, which
  no longer leaves a speaker (it gives 1).
- **Advisors**: a single speaker (g_speaker). A SaySentence without onlyIfSilent with a sentence playing stops it and leaves the
  new one **without a speaker** (g_speaker is set at 0x5BB343 before the StopSentence at 0x5BB377, which clears it at 0x5BB89F):
  it is from the original, and the RUN_TEXT path does not go through there (it stops both first).
- **Lip-sync**: ApplyLipSync takes the time from `LHSampleGetPlayPosition` (ms) and calls lip_sync::UpdateKey(dt, t) on the PCM
  (window 0x200): MagnitudeSpectrum (triangular window with step 1/(n·32768), four1 from Numerical Recipes, modulus
  √((re²+im²)/n)) and the three AutoVoiceParams bands (200-400 Hz ×0.85, 400-700 ×1.1, 700-10000 ×10; threshold 0.05,
  level 2.5, rate 40·dt·0.18 downwards and double upwards, normalised if they add up to more than 1). If the sentence is no longer
  playing and t > 0.5 s, StopSentence. The wave's AudioTags (BuildAudioTags 0x42AE70) come from its cue labels
  (`audio::ReadWaveCueLabels`, read by the help texts slot when the line starts) and go to the advisor that says it;
  the pose, the mouth and the gestures are `help::spirits`' ([The advisor spirits](../interface/advisors-and-help.md#the-advisor-spirits)).
- **Lazy dialogue banks**: HelpSprites, villagers, VillagersBanter, SpellDialogue and Guidance are registered only
  with their headers (`PackFile::ReadAudioHeaders`) and each wave is read from the .sad when it is decoded, like
  `LHBankRegister(path, 0)` (about 107 MB that are no longer read at startup).
- **LHSampleStop with ramp**: all stops by sample, by owner or by channel (and the cutoffs of
  `LHSampleUpdate3DChannels`) go down to 0 in 20 ms, like the DLL; the game no longer waits those 20 ms (the fade goes on
  in the background). `StopAll` and the resets, abruptly.
- **CHL**: GAME_PLAY_SAY_SOUND_EFFECT (340), SAY_SOUND_EFFECT_PLAYING (458) and SPIRIT_SPEAKS (246) done.
  STOP_SOUND_EFFECT with isSay is `script_sound::StopSoundEffect`.
  - SPIRIT_SPEAKS' player, read (2026-10-28): `ConvertScriptSpiritToHelpSpirit` 0x710350 takes the alignment of the
    local player, `g_game` +0x18 + 0xA60 × `PlayerIndex` (+0x205A59), for the alignment spirits 3 and 4
    (0x71036D..0x710387, 0x7103A8..0x7103C2), and `LocalRand(100)` for 5 (> 50: the evil one). openblack's
    `ScriptHelpSpirit` (`src/CHLApi.cpp`) reads PLAYER_ONE, which is what its local player always is
    (`PlayerSystemInterface::GetLocalPlayer`). The text is 0 past 6973 (0x710C6E) and `GetSpiritWhoTalks` 0x5C6E20
    reads entry 0 for 0 or a text past the table's end, as `HelpTextSystem::GetNarrator` does (`k_TextCount` 0x1B3E).
    Nothing of SPIRIT_SPEAKS differs.

##### Where the lip sync runs

ApplyLipSync's only caller is `HelpDude::Update1` 0x5BEB05, reached by every Update1 (no
branch from 0x5BDDA0 jumps past it). The control's per-frame loop (`HelpDudeControl::Process` 0x5C3A30, first loop
0x5C3B05..0x5C3CBC, dude 0 then 1) dispatches each dude (0x5C3B88 / 0x5C3B8C):

- **out** (the out flag +0xC+4i ≠ 0, going home included): Update1 only, 0x5C3C83 `call 0x5bdda0`, so the lip sync;
  its Update2 runs in a second loop after both (0x5C3CD1..0x5C3D75);
- **at home, State (+0x3490) ≠ HOVER (4)**: `HelpDude::Update` 0x5BF5D0 (Update1 + Update2), 0x5C3CAC, so the lip
  sync too;
- **at home and hovering** (0x5C3C8E `cmp [ecx+0x3490], 4` / 0x5C3C95 `je`): only UpdateSaySentence 0x5C3CB3, so no
  lip sync for that dude that frame (no CalcKey, no 0.5 s stop, no ProcessAudioTags).

**dt:** both dudes get the same one, the loop's arg1 (0x5C3C75 / 0x5C3C9E `mov eax, [esp+0x2c]`), forwarded to
`ApplyLipSync(dt, 1)` (0x5BEAFD / 0x5BEB01). Draw3D makes it at 0x5C5AD6 / 0x5C5AE5: `(float)g_delta_time × 0.001f`
(a float multiply by [0x8AA3B0]), with g_delta_time the frame's real ms set once a frame by `LH3DRender::StartFrame`
(0x82F0E0; stored at 0x82F18D, forced to 1 when ≤ 0 at 0x82F193). **Place in the frame:** inside
`GGame::Process3dEngine` 0x54DA80 → `HelpSystem::Draw3D` 0x54E2ED, after the frame's game turn and before the dudes
are drawn by the same Draw3D (0x5C5B26), so the key a frame computes is drawn that frame. **Nothing runs while the
window is minimised**: `ProcessGraphicsEngine` is skipped then (`IsAppMinimized` 0x54D3B1 / 0x54D3B8). Draw3D is also
skipped when `ViewMode == 3` (0x54E2A4 / 0x54E2AA / 0x54E2AD), but **no 1.20 code ever writes 3** to ViewMode
(+0x205A28): its only five writes store 0 (GGame::Init 0x54FCA4, EndFallingSpellVideo 0x553A1C, LeaveInsideCitadel
0x553B1F), 2 (KickOffFallingSpellVideo 0x5539D5) or 1 (GoInsideCitadel 0x554004); there is no `lea` of the field, and
the draw switch before the test (0x54DD83) has no case 3. So the skip never happens in 1.20, and openblack has none.

openblack (`help::spirits`): `AdvisorSpiritController::Process` dispatches the same way: a dude whose control state is
not Home (Out, GoingHome, Clinging = the out flag ≠ 0) runs `UpdateMotion`; one at Home whose `State()` is not
`dude_state::k_Hover` runs `UpdateMotion` and `UpdateHead`; one at Home hovering only `updateSentence`. `UpdateMotion`
always reaches `UpdateAnimStack`, which asks `Queries::lipSync(dude, dt)` = `AdvisorVoices::ApplyLipSync` first, with
the controller's dt, `game_clock::FrameRealMs() × 0.001f` (`SpiritsRuntime`), the same for both dudes. The game's loop
holds while the window is minimised, so the advisors' update does not run then. The voices run nothing of their own
once a frame any more (the late call in the audio section, a frame behind, is gone).

#### Voices in game

Land 1 (`OPENBLACK_TEXT_TRACE`, `OPENBLACK_AUDIO_TRACE`, `OPENBLACK_SFX_TRACE`). The `FollowUs` intro speaks and progresses: the family's twelve voices
(`HELP_TEXT_DEFINITELY_NEWEST_INTRO_01..12`, villagers 57..68, 2D, owner 0x270F or 0x270D) with their
SAY_SOUND_EFFECT_PLAYING waits; then the RUN_TEXTs with voice "¡Has salvado a nuestro hijo!", "¡Gracias! ¡Gracias por tu
misericordia!" (woman, villagers 119/120), "¡Te alabamos!" (with click) and the advisors "Saludos.", "Somos tu
conciencia.", "Bueno.", "Y malo.", "Yin y Yang.", "Blanco y Negro.", "Como parte de ti, te guiaremos por este mundo."
(HelpSprites, owner 0x270C); then "Nuestra gente te rendirá culto.", "Ten la bondad de acompañarnos a nuestro
Pueblo." and "Te enseñaré cómo seguirlos.". The texts with a click are passed with `OPENBLACK_TEST_TEXT_CLICK=1` (the click, with
the widescreen, cuts the villagers voice, as in the original). It used to stop at HAS_CAMERA_ARRIVED (035);
since the script camera was ported it goes on into `Drag` and says text 5128 (HELP_TEXT_HAND_DEMO_DRAG_01, sample 497)
before stopping at the hand demo (HAND_DEMO_TRIGGER 336, pending; map-loading.md, "In game (Land 1)").

#### Audit: voices

Checked in the disassembly 0x5C36D0 (delay |+0x3514| − 0.95 double 0x915438, (v+1)·250, cap 500), 0x5C3750, 0x5C3780
(+0x7C = ebp, 0x900D48 = another identical copy of 0x915D40, 0.9 double 0x915440), 0x5C5290/0x5C52C0, 0x5BB340,
0x5BB530, 0x5BB610, 0x5BB730/0x5BB760/0x5BB7C0/0x5BB840, 0x5BCD00, 0x428850 (bands, the strongest band, limits
dt·rate·0.18 and double), 0x428A80, 0x428C60, four1 constants (0x8C49F8, 0x8AB260, 0x8C49F0), init 0x5C1EA1..0x5C1F24,
0x5C6025..0x5C60DB, 0x5C62F0, 0x5C6340, 0x70F8E0, 0x70F9B0, 0x710280, 0x710C40, 0x710350 (table 0x710400), 0x5C6E20,
0x5C6A7E..0x5C6AAD, 0x5C6720 → 0x5C68A0 → 0x5C4C20 → 0x5C5250, and in the DLL 0x10012BF0, 0x10012C50, 0x10012DF0,
0x1001439D/0x100143BC, 0x10014C00, 0x10015180: they add up. Fixed: `advisor::Reset` (now `AdvisorVoices::Reset`) claimed to be "HelpSystem / the map
change", without a source (HelpSystem::Reset 0x5C5580 does not touch HelpDudeControl): now it is only for the tests and
no game code calls it; removed the default argument of `PackFile::ReadBlocks`. Clarification ("the click cuts villagers
but not HelpSprites"): the click cuts villagers/0x270F with 0x42A210 and also **stops the advisor who is speaking**
(fn_005C3780 → fn_005C3750, without an interruption sentence in the original); the HelpSprites 0x270F narration (narrator ≠ 2/3)
is not cut. No leaks: the advisor only decodes PCM (no AL buffer), `ReadWave` closes its stream; the voices have no
owning entity and `ClearMap` → LHSampleStopAll cuts them; the music thread does not touch `sample_play`.

- `Interrupt`: HelpDudeControl fn_005C3780(dude, arg) (from fn_005C6720 -> fn_005C4C20: the click of HelpSystem::ProcessInterface 0x5C6A88 / 0x5C6A93 and fn_005C6800 0x5C6856 / 0x5C6861 with arg 1): an active dude that talks and was not interrupted yet (+0x7C) picks an "interruption" text (arg 0 and LocalRand(2) == 0: fn_005C5290, HELP_TEXT 0xE44 + rand 8 for the good one / 0xE51 + rand 8; else fn_005C52C0, 0xE3F + rand 5 / 0xE4C + rand 5), its sample from the voice table (0x900D48), Stop(dude), and outside the citadel, if PercentageDone < 0.9 (double 0x915440), Say(dude, sample, 0) and +0x7C. (As in W120: PercentageDone is read after the stop, which leaves no speaker, so it is 1 and the interruption is never said.)

### Guidance and night voices in openblack

Read in the disassembly: the whole of SoundGuidance.cpp 0x71AA90..0x71D480, of SpookyVoices.cpp 0x72E130..0x72E8B0,
the callers (0x54E711..0x54E729, 0x5DC50D, 0x7506C0..0x7508F4,
0x4141A0, 0x406640..0x406786, 0x66F4D8..0x66F509, 0x63A9A0..0x63A9E6), HelpSystem::RunMessage 0x5C8CE0 /
StopHelpScriptsForNewHelp 0x5C8C40 / TriggerCategory 0x5C8280 / Reset 0x5C5580 / fn_005C6CF0, GScript::StartScript
0x6EB710, HelpSystemOn 0x6FBFD0, SetHelpSystem 0x6FC020, GRand 0x6DE570 / 0x6DE590, _LHRand 0x7DB600, the moon
phase fn_0086A7F0 and the tables 0x980128..0x9804D0, 0x999434.

- GSpookyVoices (SpookyVoices.cpp, 0x72E130..0x72E8B0): at night by the computer's clock, now and then, a voice of Guidance.sad whispers the player's name, picked once among the 100 HELP_TEXT_SPOOKY_NAMES_* by a Soundex comparison with the profile's name.  The object is the static 0xDA0830: +0x8 the bank [0xDA0838], +0xC the options [0xDA083C], +0x10 the sample [0xDA0840], +0x14 a counter [0xDA0844], +0x18 a countdown [0xDA0848]. The GSpookyVoiceInfo of info.dat (5 entries, 0xDA0850) is never read by this code.
- `k_FirstName`: 0x999434..0x9995C4: the HELP_TEXT of the 100 names (HELP_TEXT_SPOOKY_NAMES_01..100 = 4586..4685)

#### GGuidance

`src/Audio/Services/Guidance.{h,cpp}`, namespace `audio::guidance`.

**Faithful.** A single GGuidance (the original has one per GInterfaceStatus, +0x30, and all callers use the one from
`MyInterfaceStatus`). What it reads from the game arrives through `GameQueries`; without a query, the neutral value of a
game without that system, and nothing sounds.

| function | original | what it does |
|---|---|---|
| `Init` | 0x71AC70 (GInterfaceStatus::Init 0x5DD1CB) | own options (Guidance bank, 2D, track 0, mode 2); lastPlayed[t] = turn − LocalRand(base) if it is > 0 (**with a second draw** for the value that is stored), otherwise 0; +0x98..+0xC8 = 0, +0xA8 = 30, the 7 "once" ones to 0, [0xC221CC] = 1. openblack calls it at the start of each land |
| `TimeSinceLastPlayed`, `Interval`, `PlayNow`, `CanPlaySpiritRemark` (was `HelpSpritesPlayNow`) | 0x71ADF0, 0x71AEE0, 0x71AF50, 0x71AFF0 | see above; `CanPlaySpiritRemark` = PlayNow(8) && PlayNow(t) (type 8 is never updated: it is a global gate) |
| `PlaySample` | 0x71C6F0 | **persistent** options: vol, pitch, +0x2C, sample (voice table 0x96BA38 if it is a text), owner = player no.; 3D: point, max, min = max·0.333 (0x8D8734), mask 0x180, track 0; **without a point it neither sounds nor marks**; 2D sets is3D 0 and leaves the rest as the last 3D left it (0x71C7CA); owner 0 is no owner; lastPlayed = turn |
| `SpiritSay` (was `HelpSpiritSay`) | 0x71D270 | RunMessage(text, text, "MultiHelpJustTalkWithText" or "…NoText" for 31) + TriggerCategory(8); lastPlayed[type] and +0x2C = turn **even if the script does not start**; +0x2C is lastPlayed[8] (the array starts at +0xC), the HelpSprites gate of every remark, so each remark restarts the common wait (in the code: `lastPlayed[HelpSprites]`) |
| `OneOff(k)` | fn_0071D0B0 | once per game (the flags are cleared only by `Init`), with probability (table 0x980440: 3308 0.02 · 3317 0.0002 · 3318 0.01 · 3321 0.01 · 3325 0.05 · 3326 0.1 · 3328 0.025) → SpiritSay(text, 32) |
| `GetRandomSample`, `RandomSampleForValue` (was `GetRandomSampleBasedOnValue`) | 0x71D300, 0x71D320 | HELP_SPRITES_GUIDANCE lists from info.dat (22 × 34; up to the first 0; 34 full ones count as 33) |
| `TimeSinceThingSeen`, `DesireSample`, `DesireScore` | fn_0071AE10, fn_0071AA90, fn_0071B410 | see below |
| `UpdateTownDesireRemarks` (was `ProcessTownDesireSFX`) | 0x71B020 (+0x71B130, +0x71B270) | every 10 turns: the town with a store and people nearest the camera (< 200) and its 17 desires {value +0x37C, type +0x380}; then the 6 worship sites of my citadel with worshippers (food, and +0x70 of the citadel) **override** the town if they score (the +0x70 one whenever it is not 0); value > 0.3 → x = v − rand(v/2), 3D at the town or the citadel, max 200·x; +0x98 = the sample |
| `UpdateHeartBeat` (was `ProcessHeartBeatSFX`), `HeartBeat`, `SetHeartBeatOverride`, `HeartBeatPulse`, `StopHeartBeat` | 0x71C190, 0x71C460, fn_0071C3F0, fn_0071C430/450, fn_0071C650 | every 10 turns v = Σ protection desire + ((+0xC8 + 0.001)/(q + 0.001) − 1) + ((+0xC4 + 0.001)/(p + 0.001) − 1) + Σ for each enemy creature whose nearest town is mine (d < 400) 1 − max(d − 100, 0)/400, clamped to 0..1; p, q smoothed 0.1; **every turn** (the `jne` at 0x71C1AD jumps to the call at 0x71C3C1) fn_0071C460(stored v): pitch, phase (+pitch·0.025·100 ms·0.001), pulse (1 − cos 2πφ)/2. The map clear's status pass runs it once more: `UpdateHeartBeatAtClearMap`, with `HeartBeatInputAtClearMap` (no towns, believers, creatures near a town or citadel heart; the belief share kept; [clearing on a new land](../hand/states.md#clearing-on-a-new-land)) |
| `RemarkOnMoonPhase` (was `HelpSpritesCheckMoonPhase`) | 0x71D1C0 (static) | countdown [0xC221D0]; at visual night, phase − π; at real night and \|·\| < 0.15 → OneOff(5) and 600000 turns; otherwise, ftol((phase − π)³·12000): the loop at 0x71D244..0x71D24E multiplies by the value twice (a cube), so before the full moon the wait is negative and the moon is looked at again the next turn (in the code: `MoonWait`) |
| `MoonPhase` | fn_0086A7F0 | 2π(1 − frac(days·0.03386318)), days = time()/86400 − 10962 (integer) |
| `ProcessGameTurn` | GGame::ProcessTurn 0x54E711..0x54E729 | GSpookyVoices::Process, the moon, the desires and the heartbeat of GInterfaceStatus::Process 0x5DC50D (order **⚠️ approximate**) |
| `PlayResourceDropRemark` (was `ResourceDropSFX`), `ResourceDropSample` | 0x71B570, 0x71B5F0 | the nearest town < 100; sum of its three values of the type ≥ 0.5 → PLEASED_x, < 0.25 → DISPLEASED_FOOD for food and **PLEASED** for wood and rain (the DISPLEASED ones for wood and rain are not used in the original); 3D, max 200 |
| `PlayTownAttackRemark` (was `TownAttackSFX`), `StrongestEffect`, `AttackerSample`, `WarnTownUnderAttack` (was `HelpSpritesTownBeingAttacked`) | 0x71B7C0, fn_0071BE40, 0x71BC20..0x71BD50, 0x71C870 | 10 ATTACK, + 10 FIRE if effect 0 dominates, + 10 times the attacker's sample (lightning / rock / creature); one at random; max 200·(min(+0xEC0·0.2, 1) + 1); and always the comment if the town is mine (PlayNow, not CanPlaySpiritRemark; aggression > 1) |
| `StartTotemRaiseSound`, `EndTotemRaiseSound` (were `StartRaiseTotemSFX`, `EndRaiseTotemSFX`) | 0x71BEB0, 0x71BED0 | the end plays nothing in the original (it computes the alignment class and returns) |
| `PlayDiscipleRemark` (was `MakeDiscipleSFX`), `DiscipleText`, `AlignmentClass` | 0x71BF10, fn_0071AB70, fn_0071C690 | 2D, vol 85; table 0x98040C; disciple 10 says GOOD/EVIL_LIVE_HERE depending on the class (±0.55) |
| `PlayBeliefRemark`, `PlayBeliefSample` (were `BeliefSFX`, `BeliefSample`), `BeliefVisibility` | 0x437F40, fn_0071BF70, fn_0071C0D0 | only if the player's belief is below the highest; x = (b + 0.0001)/(max + 0.0001) − rand(x/3); visibility t0·(1 − d²) > 0.3 |
| `PlayVillageDeathRemark` (was `DeathInVillageSFX`; its type is `Type::DeathInVillage`, 7) | fn_0071C810 | DEATH_IN_VILLAGE_06 + rand 5, 2D |
| the `Warn*` and `Remark*` functions (16, were `HelpSprites*`) | 0x71C930..0x71D070 | each with its condition (people, working store, < 300 from the hand GInterface+0x3B8, on screen) and its list (type − 9). Three are never used in v1.42 ✅: `WarnCreatureUnderAttack` sub_71C930 (list 1), `WarnInjuredPeople` sub_71CB80 (list 7) and `RemarkOtherVillages` sub_71CD00 (list 11) have no call or jump (xref) and no copy of their address anywhere in the file (a scan of every dword of runblack.exe, 2026-10-21) |
| `UpdateAlignmentRemarks` (was `HelpSpritesAlignmentProcess`) | 0x71CEB0 | accumulated +0xC0 = 0.95·+0xC0 + change; past 2·the player's maximum change: same sign and \|a\| > 0.75 → good: type 29 list 20, evil: type 28 list 19 **(sic: the Enums.h names give them swapped)**; opposite sign and \|a\| > 0.4 → 25/16 or 26/17 |

**Randomness**: LocalRand(n) ∈ [0, n) (0 for 0, without a draw) and LocalFloatRand(x) = x·LocalRand(0xFFFF)·(1/65535) like the
original; the generator is `game_random`, openblack's single local stream **⚠️ approximate**, except in the tests, which
use `SeededRandom` (was `LHRand`) with a seed.

**New queries** (`GameQueries.h`): `playgroundGame`, `multiplayerGame` (false), `helpLevel` (HelpSystem; 3 without it),
`localPlayerNumber` (PLAYER_ONE), `visualNight`, `handPosition` (the hand, **🔍 inferred** GInterface+0x3B8), `pointOnScreen`
(without it: false), `desireTowns`, `worshipSites`, `nearestTownAt` (assigned: `map_cells::GetNearestTown`), `townResourceNeeds`, `heartBeat` (without them: nothing; `heartBeat` is assigned, see [Chants and heartbeat](magic-and-weather-sounds.md#chants-and-heartbeat-in-openblack)), `helpRunMessage`,
`helpTriggerCategory`, `profileName`.

**HelpSystem**: `+0x45F8` switch (Reset 0x5C55FC = 1; **SET_HELP_SYSTEM 253** done), `+0x45F4`
level (3 without a profile, 0x5C6DB6), **HELP_SYSTEM_ON 200** done (switch && level ≠ 0), `TriggerCategory` (+0x2D8, 9;
Reset sets them to 0), `+0x560`. `script_control::RunMessage` / `StopHelpScriptsForNewHelp`: it does not start if a task that is not
a help task (type without 0x42) has the dialogue; otherwise, it stops the help scripts (0x4A), pushes the two numbers as
float and starts the script with types 0x7F. `chlapi::ScriptVm` comes out in `CHLApi.h` with `pushFloat` and `startScript`.

**Connected in openblack**: `ProcessGameTurn` on the turn (Game.cpp, before `audio::ProcessTurn`); `Init` on each
`LoadMap`; the info.dat lists at startup; `PlayResourceDropRemark` in `pot_resource::AddResourceToPos` (new pile from the
local hand: RESOURCE_TYPE 1 → 2, 0 → 1) and in `HandSystem::DepositInStore` (with the point and the GetGuidanceResourceType of the **receiver**, the store: StoragePit inherits the 0 of GameThing 0x71BDD0, so the original runs PlayNow and GetNearestTown and says nothing). Today none of this
sounds in Land 1: the non-"always" types stay silent in the campaign's Land 1, the towns have no desires or resource
values (neutral queries). The heartbeat does sound (the heart is the temple).

- `guidance::OneOff`: 0x71D0E5: push 0x20 (at `SpiritSay(oneOff.text, Type::OneOff);`)
- `guidance::AttackerSample`: 0x71BC47..0x71BC98: LIGHTNING_01..10
- `guidance::AttackerSample`: 0x71BCD0: ROCKS_01..10
- `guidance::AttackerSample`: 0x71BC20 / 0x71BC99
- `guidance::PlayTownAttackRemark`: 0x71B80D..0x71BB43: a list whose new nodes go first (fn_0071D470 / fn_0071D3B0)
- `guidance::PlayTownAttackRemark`: 0x71BB78..0x71BB8E: fn_0071D440(LocalRand(count))
- `guidance::HeartBeat`: 0x71C4BF..0x71C518: fn_0071C420 (x 0.025, 0x8D150C) x [0xD01A38] x 0.001, brought to <= 1
- `guidance::HeartBeat`: fn_00428740 0x71C63F (at `SetPitch(Bank(SfxBank::InGame), PlayerOwner(owner), k_HeartBeatSample, pitch);`)
- `guidance::UpdateAlignmentRemarks`: 0x71CED4..0x71CEEF: only past 2 (0x98017C) x the player's maximum change a turn
- `guidance::UpdateAlignmentRemarks`: 0x71CF62..0x71CF71: fn_0071D040 (type 29, list 20) when good, fn_0071D010 (type 28, list 19) when not (at `const auto type = good ? Type::VeryEvil : Type::VeryGood;`)
- `guidance::UpdateAlignmentRemarks`: fn_0071CF90(good): type 0x1A - good (25 / 26), list 0x11 - good (16 / 17) (at `const auto type = good ? Type::GoodBeingEvil : Type::EvilBeingGood;`)
- `OneOff`: fn_0071D0B0's remarks (0x71D0E5)
- `Init`: GGuidance::Init 0x71AC70 (GInterfaceStatus::Init 0x5DD1CB / ResolveLoad 0x5DD194): the options +0x8 (bank Guidance GAudio+0x3D0, 2D, track 0, mode 2, the ctor's defaults otherwise); for each type lastPlayed (+0xC + 4 t) = turn - LocalRand(base) if that is > 0, else 0 (0x71ACCF..0x71AD17: a positive first draw is drawn again for the value kept); the LastThings list +0x90 emptied; +0x98..+0xC8 = 0, +0xA8 = 30.0, the one-off flags +0xD0..+0xEC = 0; and [0xC221CC] = 1. As in the original, only a new game runs it (`guidance::StartLand(newGame)` from `Game::StartNewLand`, its turn counter back to 0; the testbed is a new game); a land change keeps the guidance ([across a land change](#the-guidance-across-a-land-change), 2026-10-22). Before, openblack ran it at every land.
- `PlaySample`: GGuidance::PlaySample 0x71C6F0(is3D, textOrSample, owner, type, volume, pitch, +0x2C, MapCoords*, maxDistance, isText): the persistent options get +0x28 volume, +0x48 pitch, +0x2C, the sample +0x24 (the voice table 0x96BA38 when isText) and the owner +0x20 (the player's number: GetPlayerNumber 0x64A790); 3D: +0x08 = 1 and, only with a point (none: nothing at all, 0x71C757), +0x30 = the point, +0x58 = maxDistance, +0x54 = maxDistance * 0.333 (0x8D8734), the mask +0x1C = 0x180 (the .sad keeps neither) and +0x0C = 0; 2D: +0x08 = 0 (the rest stays as the last 3D one left it). Then GAudio::PlaySoundEffect 0x429E30 and, for a type < 33, lastPlayed = turn. `point` is the MapCoords as a world point (x, GetAltitude + its height, z).
- `TimeSinceThingSeen`: The things GGuidance remembers (+0x90, LastThings 0x71AEC0 {thing, turn}): fn_0071AE10(thing) gives the turns since the thing was first seen, starting again past 600 (0x258); a new thing is added with the turn and gives 0. A land change (`StartLand(false)`, 2026-10-22) keeps the list, its count and its turns, and marks each entry as from an older land (`RememberedThing::olderLand`), which matches no thing: openblack's things are entity values, and a new land's registry gives its entities values the last land's had, so a kept entry would otherwise match whatever new thing came out with the same value. The Land Island window's "Kept by a land change" shows how many things are remembered and how many are from older lands, and whether the guidance is enabled. Test: `GuidanceTest.ALandChangeKeepsTheGuidanceAndOnlyANewGameStartsIt`
- `townResourceNeeds` (the town's three values of a RESOURCE_RAIN_TYPE): the original reads them in
  `GetResourceDropSample` (three town desire arrays of the desires 0, 1 and 10).

#### GSpookyVoices

`src/Audio/Services/SpookyVoices.{h,cpp}`, namespace `audio::spooky`.

**Faithful** except for the name. Static object 0xDA0830: bank +0x8, options +0xC, sample +0x10, counter +0x14, count-
down +0x18. The info.dat info `GSpookyVoiceInfo` (5 entries, 0xDA0850) is read by nobody.

- **Soundex** (`SoundexDigit`, was `SoundExCode`, 0x72E4E0, jump table 0x72E54C): a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3,
  l 4, m n 5, r 6 and **h w y the character itself** (entry 0x72E548 returns eax). Not a letter (`_isalpha`) = 0
  (🔍 inferred: the "C" locale, ASCII only; "é", "ñ" give 0).
- `GetNextSoundexCode` 0x72E5C0: skips 0 codes until the end or a space; after a letter with a code, if the next one
  has the same, it **returns code + 1 without skipping it** (a quirk of the original: "Curro" gives 7, 6, 0).
- `SoundsAlike` (was `PerformSoundexComparison`) 0x72E630: identical first character (case counts) and three equal codes;
  `SoundexOverlap` 0x72E6E0: any word of the name; `FindSoundAlikeName` (was `TrySoundex`) 0x72E7E0: the first of the 100 names (0x999434:
  4586..4685) that matches → the sample from the voice table 0x984D48 (another copy of 0x915D40). In the Spanish installation
  11 names resolve to an earlier one (Alfredo → Alberto, Manolo → Manuel, Jaime → Juan, María → Mario, Miriam →
  Mariano, Lucía and Luisa → Luis, Ángeles → Ángel, Julia → Julio, Rocío → Rosa, the second Alberto).
- `GetName` 0x72E740: profile name ([0xD4BF38] = PlayerProfile +0x200), then the network one ([0xD204D4]+0x70) and the
  `DefName` from the registry "Software\Microsoft\MS Setup (ACME)\User Info" (`RegistryRetrieveString`, 0x72E7AA),
  each tried with `TrySoundex` until one matches (0x72E740..0x72E7C8); no environment variable is read. openblack:
  `profileName` = `ecs::audio_queries::ProfileName`, the played profile's name (test
  `ProfileCreatureTest.TheNightVoicesTryTheNameOfTheProfilePlayed`); the network name is online and parked; the
  registry's is **not ported** (the Lionhead library's key root is not read). No name → sample 0 → it never sounds.
- `Init` 0x72E2A0 (GGame::InitOneTimeOnly, once), `UpdatePlayerName` 0x72E870 (GGame::Init, each land).
- `Process` 0x72E310: nothing on Lands 1 and 2 or without a sample; countdown of 100 calls; at night (fn_0072E3B0),
  r → ftol((1 − r³)·1000) < counter → `PlaySpookyVoice` (was `PlaySpooky`), otherwise OneOff(1); counter + 1. **Correction**: `PlaySpookyVoice` sets the
  counter to 0 (0x72E4D3), so after sounding it goes back to 1.
- `PlaySpookyVoice` 0x72E3F0: pitch 100·(1 + a³)^±1 with a = rand(0.65), +0x2C = rand(180), volume ·(1 + b³)^±1 with b = rand(0.8)
  **accumulated** (the options persist), 2D. +0x2C is the 2D pan: 0 is heard on the left, 90 ahead, 179 on the right
  ([banks and channels](banks-and-channels.md)); openblack plays the voice there (`options.pan`).

- `spooky::PlaySpookyVoice`: 0x72E3F6..0x72E42C in float steps (the FPU at 24 bits, fn_007DEE00) (at `const float a = guidance::LocalFloatRand(0.65f);`)
- `SoundExCode`: GSpookyVoices::SoundExCode 0x72E4E0(wchar_t*): not a letter (_isalpha 0x7C686F) 0; else by (c | 0x60) - 0x61 in the jump table 0x72E54C: a e i o u 0, b f p v 1, c g j k q s x z 2, d t 3, l 4, m n 5, r 6, and h w y the character itself (the table's 0x72E548 returns eax, which holds it). 🔍 inferred _isalpha in the "C" locale: ASCII letters.

#### Guidance and night voices in game

Land 1 at real night, `OPENBLACK_GUIDANCE_TRACE=1 OPENBLACK_PLAYER_NAME=Mario
OPENBLACK_TEST_GUIDANCE_SAY=80:3326` (and another run on turn 200):
`SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`; on turn 80/200 `HelpSpiritSay(3326, type 32)
MultiHelpJustTalkWithText not started`: the land's script (task 19/22, type Script) has the dialogue and
`StopHelpScriptsForNewHelp` does not take it away, as in the original. Neither the desires nor the night voices sound in Land 1
(muted by type and by land). No new errors.

#### Audit: Guidance and night voices

Checked in the disassembly: the tables 0x980190 (33 rows), 0x980328, 0x98040C, 0x980440 and the constants
0x98011C..0x980188; Init 0x71AC70 (double draw), Interval 0x71AEE0, PlayNow 0x71AF50, PlaySample 0x71C6F0,
ProcessTownDesireSFX 0x71B020, CheckWorshipSiteDesiresSFX 0x71B300..0x71B407 (the need wins even if it scores less),
ResourceDropSFX 0x71B570 / 0x71B5F0, fn_0071BF70, fn_0071C810, MakeDiscipleSFX 0x71BF10, fn_0071C460, 0x71C190,
0x71C990..0x71CAE0, 0x71CEB0..0x71D063, HelpSpiritSay 0x71D270, GetRandomSample 0x71D300..0x71D3A4, the moon 0x71D1C0 /
0x86A7F0, RunMessage 0x5C8CE0 / 0x5C8C40, CHL 200 / 253 (0x6FBFD0 / 0x6FC020), and the whole of GSpookyVoices (0x72E280..0x72E87F,
the jump table 0x72E54C). Two corrections:

- **The heartbeat runs every turn**: in `ProcessHeartBeatSFX` the `jne` at 0x71C1AD (turn not a multiple of 10) jumps to
  0x71C3B8, which calls fn_0071C460 with the stored +0xA4; only the value is recalculated every 10 turns. The first port returned without
  beating, so the pitch was smoothed and the phase advanced 10 times more slowly. Test
  `HeartBeatRunsEveryTurnTheValueEveryTen`.
- **`DepositInStore` says nothing**: in Object::DoDeleteObjectAndTakeResource 0x63A940 `this` (edi) is the receiver and
  esi the object (GetPos 0x63AA2B and ToBeDeleted 0x63AAB1 go to esi); ResourceDropSFX receives edi+0x14 and the
  GetGuidanceResourceType of edi (vt +0xE0), which for StoragePit is GameThing's 0x71BDD0 = 0. The first port passed the tree's
  point and "wood" (PLEASED_WOOD invented as soon as there is `townResourceNeeds`); now the store's point and `None`:
  PlayNow and the town search run and nothing sounds, as in the original.

In game (Land 3 at real night, `OPENBLACK_PLAYER_NAME=Mario OPENBLACK_TEST_GUIDANCE_SAY=90:3326`): `SpookyVoices: Init, name sample 95`, `Guidance: Init at turn 0`, and on turn 90
`HelpSpiritSay(3326, type 32) ... not started` (in Land 3 too the intro script has the dialogue). No new
errors.

- `ListCount`: fn_0071D390: the entries before the first 0; all 34 set count 33 (0x71D3A3)
- `GetRandomSample`: GHelpSpritesGuidance::GetRandomSample 0x71D300: list[LocalRand(count)] (fn_0071D390: the entries before the first 0; all 34 set counts 33)

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

### (Approximate) and 🔍 inferred: voices

The advisors' visual part (HelpDude: model, flight, mouth from the lip-sync key) is ported:
[The advisor spirits](../interface/advisors-and-help.md#the-advisor-spirits), with the gestures from the samples' cue labels.

- The advisor's delay reads +0x3514, the advisor's hover x: the flight is ported, and `help::spirits::Runtime` gives
  it once a frame (`AdvisorVoices::SetHover`, `src/Help/SpiritsRuntime.cpp`) to `AdvisorVoices::Say`'s
  `SayDelayMs`; until the first frame it is the init's 0 (0x5C1A61).
- **⚠️ approximate** QMixer's 20 ms ramp in four 5 ms gain steps, faded in the background instead of the game waiting the
  20 ms: the same fade, the cut instant may move by a few ms.
- **⚠️ approximate** the advisor's PCM is decoded again (the DLL converts it with ACM on start with +0x164);
  four1/Analyse/CalcKey with the x87 registers in double.
- **⚠️ approximate** only the Dialogue banks are lazy; the rest is read whole (the original registers them all
  that way).
- **🔍 inferred** spirit 1 (HelpSystem+0xC) is advisor 0 (fn_005C5250: +0x54 ≠ 1); LocalRand with openblack's
  generator.
- **🔍 not read** the narrator number of GUIDE and MONK, which `ADD_TEXT` uses without declaring them: the original's
  `ADD_TEXT` callback (0x7192E0, from `GSetup::LoadTextScripts` 0x719280) is not in the Ghidra export and has not been
  read; openblack gives them −1. It changes no voice (all their spoken texts are in villagers, so narrated whatever the
  number); only the text's colour and its history entry could differ.

### (Approximate) and 🔍 inferred: Guidance and night voices

- **⚠️ approximate**: x87 in double; the generator; the order of `ProcessGameTurn` relative
  to GInterfaceStatus::Process; the PlaySample points are world points (without the MapCoords rounding).
- The distances are `gutils::GetDistanceInMetres` (hypotenuse 0x74F680 with the 1/√ table of _FUN_0074f620
  on the points' 16.16 MapCoords) and [0xD01A38] is `game_clock::MsPerTurn()` (100, GGame::Init 0x54F4A5).
- **🔍 inferred**: GInterface+0x3B8 = the hand; ASCII `_isalpha`; the local player = PLAYER_ONE.
- **Not modelled**: the options' +0x2C (90, rand(180) in the night voices): it is stored, SamplePlay does not use it.
- **Cannot be reproduced**: after a land change, a remembered thing of the last land matches a new thing in the
  original when the allocator gives the new thing the freed one's address, so whether and which new thing counts as
  just seen without a new node depends on the original's memory allocator
  ([across a land change](#the-guidance-across-a-land-change)). openblack's kept entries match nothing (decision,
  2026-10-22): a new land's thing is always remembered anew (just seen), which differs from the original
  only where its allocator reused an address: there the original's thing can count as seen up to 600 turns ago (the
  desire remarks' score, `DesireScore`'s t1), and its list has one node fewer.

### From the audio engine's list

- The help text cache's load of InfoScript2.txt on first use stays as the fallback of `helptext`: the interface is not
  made without the j0 font or the front end atlas, so there is then no text database, and neither is there one in the
  unit tests, before the interface is made or after it goes. In a game with its data every reader comes after it is
  made (the voice table at the audio's start, then the help system, the natives and the frame's texts), so the
  database serves them all. The fallback can go once the text database exists whether or not the interface does.
- GConfirmation (START_ANGLE_SOUND 285, function 348): the core and the camera's feeds are in (`audio::confirmation`,
  `DefaultWorldCameraModel`); `Game::LoadMap` calls `audio::confirmation::Stop` where the original's script reboot does. What differs is the feeds' detail. `SmoothStrafeSpeedX` 0x454900 is the angle (Start
  with range 7) and `SmoothStrafeSpeedZ` 0x454930 the pitch (range 5), both g = (a / b − g) × 0.4f + g with b the
  camera's frame time. The original feeds them from seven sites of `CameraModeNew3::Update`, one per input:
  - X 0x45A311: the edge-scroll momentum turn, float(dz × π) / width, only when |dz| > (double)0.01f;
  - X 0x45C7D6: the middle mouse or the keys, turn × π / width, only when |turn| > (double)0.01f;
  - Z 0x45C8B1: 0.002f × the tilt, the automatic tilt included;
  - X 0x45D2C2: the hand ring's swept atan2 angle, wrapped to ±π;
  - Z 0x45DCDA: the hand's tilt drag, dy / height × 2.33333f × fov;
  - X 0x46053F and Z 0x46055C: 0 on a frame without a turn (a tilt).
  The second UpdateStrafe call (0x45CFA8, the hand at the screen edge) does not mark the frame as turned, so that frame
  feeds X twice (the turn, then 0). openblack sums every turn into one feed per frame, through float round trips,
  without the 0.01 threshold and without the edge-scroll momentum, and skips a frame with no time (the original
  divides by it). It is the camera's work; it moves the sound's timing only.
- The Guidance callers from other areas. Wired (the callers are in their areas' code, the remarks' own rules tested in
  `test/audio/test_guidance.cpp`):
  - `Villager::VillagerDead` 0x7506C0: KillingPeople, LosingVillagers, LowOnPeople and the village death remark from
    `src/ECS/Villager/VillagerDeath.cpp`;
  - `Town::UpdateAggressor` 0x73C9B0 (TownAttackSFX, the attack remarks): `town_emergency::UpdateAggressor`
    (`src/ECS/Town/TownEmergency.cpp`; tests `GuidanceTest.AttackingATownAndWreckingABuilding`,
    `GuidanceTownAttack.*`);
  - `Town::CalculateDesireForFood` 0x747FA0 / 0x7481BC (LowOnFood / LowOnWood) and `TownDesire::Process` 0x745C8A
    (VillagerUnhappy): `src/ECS/TownDesire.cpp`;
  - `Abode::ApplyEffectsDueToPhysicalDestruction` 0x406781 (DestroyBuilding: under 0.4 left by the local player's
    blow, on screen): `src/ECS/Abodes.cpp`;
  - the totem 0x738620 / 0x738666: `src/ECS/Systems/Implementations/VillageTotemSystem.cpp`;
  - `GBelief::AddToBelief` 0x437F2A: `town_stores::AddToBelief` → `guidance::PlayBeliefRemark`
    (`src/ECS/Town/TownStores.cpp`; test `GuidanceTest.ATownsPeopleAreAwedByAPlayerTheyBelieveInLessThanTheStrongest`);
  - the disciples' remark: `src/ECS/Villager/VillagerDisciple.cpp`.
  Not checked whether wired: the creature's sites (0x45A772, 0x5039E7; `guidance::WarnCreatureUnderAttack` has no
  caller in `src/`, as 0x71C930 has none found in the exe). Not wired (checked in `src/` on 2026-10-22): fn_0071D100 (other hands, multiplayer),
  GatheringBox/EndGameBox/network (OneOff 3 and others), fn_0064AF80 (multiplayer taunts), the registry `DefName`
  and the network name of GetName, and `fn_0081F1D0` (point on screen).
- The voice when dropping resources (ResourceDropSFX 0x71B570): connected (`src/ECS/ObjectDelivery.cpp`,
  `src/ECS/PotResource.cpp`; test `GuidanceTest.FoodDroppedOnATownThatWantsItIsPleased`), not tested in game (it
  requires dropping food or wood with the hand over a town on Land 2+).
- Text, not yet read: the values 8/5 from info.dat; GUIDE and MONK; `GRand::LocalRand(14)`: 0..13 or 0..14
  (0x6DE570); the advisors of `SpiritHome`; drawing the text (HelpText fn_005CCED0).
- Bugs in the original that are copied (to confirm with the user): the sacrificed child asks for ScriptSfx 179, which
  does not exist; GConfirmation never says "no"; MONSTER_09 is missing; STOP_SOUND_EFFECT(isSay) does not stop 0x270F;
  WELCOME_DANCE has no file; GET_MUSIC_ENUM_DISTANCE pushes twice.
- `Confirmation.h k_No` / `k_Yes`: "[0x963BA0] HELP_TEXT_ROTATION_YES_01; LocalRand(1718 [0x963C48] - 1704)" and
  "[0x963C54] ROTATION_NO_01; LocalRand(1703 [0x963CFC] - 1689): never reached". [GConfirmation](#advisors-guidance-confirmations-and-night-voices) above says "HelpSprites 1686
  (BETTER_14), 1673..1685 and 1704..1717". Not yet read: whether 1689..1702 / 1704..1717 are HELP_TEXT ids or
  HelpSprites samples, and where 1673..1685 comes from.
- `guidance::ResourceDropSample`: "return need < k_ResourceDispleased ? pick(0x1355) : 0; // the same
  (0x71B6E8..0x71B711)" and "pick(0x1358) (0x71B660..0x71B689)". The code names the blocks: 4949 (0x1355)
  PLEASED_WOOD_01..03 for wood and 4952 (0x1358) PLEASED_RAIN_01..03 for rain, each + LocalRand(3)
  (`src/Audio/Services/Guidance.cpp`); not checked against the exe which site is which.
- `Guidance.h Attacker`: "a creature MONSTER (each 10 + LocalRand(10))". `guidance::AttackerSample` uses 0x1338
  (MONSTER_01..10) as the base, beside 0x132E LIGHTNING and 0x1342 ROCKS; not checked against the exe.
- `guidance::LocalFloatRand`: "return 0.0f; // 0x6DE597..0x6DE5AD". The code returns 0 for an argument of 0
  (`src/Audio/Services/Guidance.cpp`); not checked against the exe.
