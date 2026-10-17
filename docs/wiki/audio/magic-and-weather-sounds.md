# Magic and weather sounds

## Summary

The sounds of the miracles and the weather: the particle atoms' loops and hits (teleport pool, tornado, shields,
lightning, fireballs), the weather's ambience and thunder, the alignment music, the worship chants and the creature's
heartbeat. openblack plays the miracles' sounds on the channels as the original does, with the weather and the
alignment read from their systems; the open approximations are in Pending.

## The original

The addresses are the v1.42 exe's and `LHaudiodllR.dll`'s (0x1000xxxx), read with the 1.20 symbols; the patch did not
change the audio. The engine every sound goes through is on [the audio engine](engine.md).

### Sound of the particles

The loops and hits of the miracles (teleport pool, tornado, shield, lightning bolts, fireballs...) are played by
the PSys atoms. What follows is verified in the exe and in `LHaudiodllR.dll`.

- **`SOUND_ACTION` property** (`SoundActionProperty::ReadProperty` 0x585A70, `src/Particles/SoundAction`):
  `<SOUND_*|NO_SOUND> LOOPING b ONLYONE b SOFTRELEASE b USESURFACE b`. The name is looked up in `Data\SoundAction.h`, which
  the game reads on the fly (fn_00585590, `LHParseFile::FindEnumVal` 0x7BE530); unknown or NO_SOUND → -1. It stores
  LOOPING (bit 0), SOFTRELEASE (bit 2) and USESURFACE (bit 3); ONLYONE is read and discarded. The code sets 0x22 (bit 1
  "delay by distance", bit 5 "ground height") on the thunderclaps.
- **PSysSoundAction** (0x18): action, then the slots passed to the bank (surface +4, size +8, alignment
  +0xC), FadeStep +0x10 and the flags +0x14. Default values (inline ctor, e.g. CreateRuleAnAtom 0x69F350):
  surface 1, **size 2**, alignment 2, FadeStep 0. Correction to the report: without SoundRadiusFP the size is 2, not 0
  (the result is the same in the "*" rows).
- **`AtomCore::StartSound` 0x6745D0**: global position of the atom (with 0x20, terrain height); with USESURFACE,
  `GSoundMap::GetSurfaceType`; alignment of the owning player (`GetDiscreteAlignmentValue` 0..6 → 1,1,2,2,2,3,3; still
  not linked to the player: the slot stays, no row of spells.sad looks at it). It creates a `PSysSound` (0x40, ctor
  fn_006D0F70) at the start of the atom's list (+0x2C) and in the global one 0xD4EE70. With bit 1 it does not play: +0x34 =
  distance to the camera / **347**. Otherwise, `GAudio::SamplePlayAnimEffect` 0x42A4B0 (mode 0) with the attributes
  {size, alignment, 1, surface, action} over spells.sad.
- **`GSoundMap::GetSurfaceType` 0x71D8E0** (`src/Audio/SoundMap`): 6 outside the 512-cell grid or without a block;
  7 if the cell is not land (`MapCoords::IsLand` 0x603720: bit 0x10 of the cell properties, `hasWater`); otherwise,
  the material's `surfaceSound` (1..8, other → 3). The animation sounds now use this same function (before,
  "height ≤ 0 → 7").
- **Bank** (`LHSamplePlayAnimEffect` 0x100146F0 of the DLL): `LHFindAttribRow` (most exact row, `src/Audio/AnimEffectBank`,
  alias of the core's `AnimEffectTable`: its tables are copied from those of the registered bank,
  shared with the animation sounds), a random sample from the list and **it does not play if the camera is farther away
  than the sample's maxDist** (S_TeleportPool: 170). The sample's playback mode (+0x274, bit 0x400) 2 =
  does nothing if it is already playing for that object; thus the loop can be "re-emitted" every turn without duplicating.
  The mode argument: **0 play, 1 `LHSampleStop`, 2 `LHSampleReleaseLoop`**.
  `GAudio::SamplePlayAnimEffect` also filters by help/cinema modes and the inside of the citadel: not ported.
- **Lifetime of the PSysSound** (fn_006D11A0, from `PSysGlobal::GameLoopEnd` 0x68F5B0 once per turn, with the turn's
  duration):
  - live atom, LOOPING: if the camera is closer than **1200**, it is re-emitted (mode 0).
  - live atom, delayed: if +0x34 > 0 the turn is subtracted from it; when it passes 0 (strictly negative) it plays. A delay
    that lands exactly on 0 never plays (it is kept).
  - dead atom (StopSound 0x674500 sets +0x38 = 0; the atom's destructor does the same): if it is still playing, with
    FadeStep it lowers the volume `vol − FadeStep` (0..127, no less than 0) every turn, and once (+0x3C) sends mode 2 with
    SOFTRELEASE (the loop finishes its pass) or 1 without it (cut). When it is no longer playing, it is deleted.
  - `PSysSound::Get3DSoundPos` 0x6D1000: the drawn position of the atom (+0xF4), with 0x20 at ground level.
  - The cull constant lbl_0093A460 is the 1200 (0x6D131E); with no channel left the sound is deleted through vt +4
    (0x6D12B5). The camera distance is fn_00442D50.
- `PSysSound`: vtable 0x93A468; the destructor 0x6D0FC0 takes it out of the global list (fn_006D1110).
  `AtomCore::GetSoundOfAction` 0x674550 gives the newest sound of that action on the atom; `AtomCore::StopAllSounds`
  0x6747E0; NO_SOUND plays nothing (0x6745D8).
- **Rules** (`Rules/Sound.cpp`): `StartStopSoundOnCondition` 0x69DC40 (SoundCondition true or none → plays if the
  atom does not already have that action; false → stops it with FadeStep), `AddSoundToAtom` 0x69DCA0 (once per atom on
  reaching Delay and with the condition; StopOtherSoundsFirst; then, with DoCameraShake +0x44, the camera shake below),
  `RemoveSoundFromAtom` 0x69DDD0 (once: stops that action with FadeStep).
- **The sound rule's camera shake** [V]: `AddSoundToAtom::DefineProperties` 0x6AB600 reads DoCameraShake +0x44 (bool, 0x6AB75B), CameraShakeRadius +0x48 (0..100, 0x6AB772) and CameraShakeDuration +0x4C (0..10, 0x6AB789); the ranges are the editor's only. In `ModifyAtomCore` 0x69DCA0, after the sound starts and the atom is marked done, with +0x44 set (0x69DD63): the atom's position through `AtomCore::LocalToGlobal` 0x673D00 (0x69DD6A..0x69DD8F), ms = `__ftol`(Duration x 1000.0f [0x8AB228]) (0x69DD94..0x69DD9F), **cut towards zero** where `StartCameraShake` 0x68F400 rounds, and `LH3DCameraChecker::Create(Radius, point, 1.0f, ms, yOnly = 1)` 0x821050 (0x69DD9D..0x69DDB9): up and down only, strength 1. In the data only `SF_BeamExplosionFX`'s `AddSoundToAtom_First` sets the flag (Delay 0.35, Radius 200, Duration 0.7: 700 ms); `SF_BeamExplosionCitadel` has two with it off.
- **Who plays on creation**: CreateRuleAnAtom (SoundOfCreate, size classes from `CreateRuleAnAtom::ModifyAtomCollection`
  0x69F410; with SoundRadiusFP: < Small 200 → 3, < Medium 500 → 2,
  otherwise 1), CreateRuleSphere (only the first atom), EmitterRuleConical and UR_WillowWisp (SoundEmission on each atom).
  For the fireball: `SizeFromThrow` (> 0.6 → 1, > 0.3 → 2, otherwise 3) and `SizeFromImpactSpeed` of
  UpdateRuleGravityWithFloor fn_006A1630 (< Medium → 3, < Large → 2, otherwise 1; before that it requires alpha ≥
  MinAlpha, |v| ≥ Small and that the atom does not already have that action).
- Test: `OPENBLACK_PSYS_SOUND_TRACE=1 OPENBLACK_TEST_PSYS="SF_TeleportVortex,1478,2129,0,1,5"` in Land1 (initial
  camera at 157 from the point, within the 170 of S_TeleportPool): "start SOUND_SPELL_TELEPORT_POOL (76) ... ->
  spells.sad/39 (S_TeleportPool.wav) looping", at 5 s "release loop" and half a second later "deleted" (it finishes its
  pass). `test_spell_sounds` checks the enum, the flags, the sizes and, with `OPENBLACK_GAME_PATH`, the real
  rows of spells.sad.
- Not ported: the owner's alignment, the game-state filters, the finite repetitions
  (loop > 0 is played once) and the DLL's global distance cap (+0x44 of the audio system).

- PSysSound::Get3DSoundPos 0x6D1000: with an atom, its position (+0xF4) with the land's height under it when SnapToGround (+0x30 & 0x20: LH3DIsland::GetAltitude 0x6D1068); it answers 1 even without an atom, leaving the point as it was (0x6D107E), so the channel keeps the last one (openblack, Milagros') the original reads +0xF4 without asking whether the atom was drawn: that field is only written when it is (Atom::Draw), so a never drawn atom would give it a stale point; `drawn` stands for that. (in the code: `PSysSoundPosition`)
- AtomCore::CalculateGlobalPos 0x6745FA, the land's height with SnapToGround (0x6745FF..0x674648) (in the code: `spell_sounds::StartSound`)
- AtomCore::StopAllSounds 0x6747E0 (in the code: `StopAllSounds`)
- AtomCore::GetSoundOfAction 0x674550: the newest sound of that action on the atom, or null (in the code: `GetSoundOfAction`)
- fn_006D11A0 (PSysGlobal::GameLoopEnd 0x68F5B0 -> fn_006D0EA0), once per game turn after the effects stepped: a live looping sound is re-issued within 1200 of the camera (the bank does nothing if it is already playing), a Delayed one starts when its delay runs out; once the atom is gone the sound fades by FadeStep per turn, is released once (loop released with SOFTRELEASE, else stopped) and is deleted when it no longer plays. (in the code: `ProcessTurn`)

## openblack

### Counterparts in the code

What openblack does at each step of the original described above, by section.

**Sound of the particles**

- `AddSoundToAtom` (Rules/Sound.cpp) → `ExplosionSystemInterface::AddShake(..., verticalOnly)` → `camera_shake::StartEffectShake`.
- `audio::spell_sounds::ProcessTurn` goes in slot 11, inside `magic::ProcessSpellParticlesEndOfLoop` (was `ProcessPSysGameLoopEnd`) (`MagicLoop.cpp`), which `Game::GameLogicLoop` calls at 0x54E688, after the particle system's `ProcessTurn` (slot 9), the fireflies and the physics, and before `GScript::Process`. Thus the sound sees this turn's atoms, as in the original, and in the original's order. The bank loader in `Game.cpp` stopped reading a .sad at the first empty sample: spells.sad has an empty one at 31, so 32..88 were missing (teleport pool, lightning bolts, fireworks...). Now it skips it and continues.

### The miracles on channels in openblack

The magic code's files keep their logic intact. Read in the disassembly: 0x6745D0 (`AtomCore::StartSound`), 0x6D0F70..0x6D13A0
(`PSysSound`: ctor, destructor, `Get3DSoundPos` 0x6D1000, the loop fn_006D11A0), 0x72EBE0 (`FireEffect::ToBeDeleted`),
0x72EDC0/0x72EDE0/0x72EE70 (fire), 0x730760 (`ProcessList`), 0x5D2730/0x5D27B0 (gesture loop), 0x5CEC50
(`GInterface::Get3DSoundPos`), 0x6882F0/0x688560..0x68866A (recognised gesture), 0x683184..0x68327F (passing
fireball), 0x68CE90 / 0x68DE69 (`PHandFX`), 0x72A640 (`OneOffSpellSeed::InterfaceTap`), 0x5EC340 (`Living::MoveByTeleport`),
0x726490 (`PlayTapSound`), 0x77F4E0 (`PlayFullyChargedSoundFX`), 0x729C40, 0x7314E0/0x731AB0 (steam) and, in the DLL,
0x10014010 (`LHSampleIsPlaying(bank, owner, &info)`).

#### New API

| function | original | use |
|---|---|---|
| `NewObjectId()` | (openblack) the original compares owner pointers | a number for the `Owner::Object` of each ParticleSound (was `PSysSound`), FireEffect, PHandFX, gesture data, FireGraphic |
| `PlayingChannel(owner, bank)` | `LHSampleIsPlaying(bank, owner, LH_SampleInfo**)` 0x10014010: the first channel of the bank and owner (any sample), if it is in use; nothing with the audio off | ParticleSound 0x6D120A |
| `Volume(channel)` | `LH_SampleInfo` +0x38 | the ParticleSound fade 0x6D1223 |

#### Each site

| site | original | now |
|---|---|---|
| `spell_sounds::StartSound` | 0x6745D0: global position (+ ground with SnapToGround), surface with USESURFACE (`ecs::sea_cells::GetSurfaceType`), Delayed → delay distance/347 (0x6747AE); otherwise, `SamplePlayAnimEffect(this, dist, {size, alignment, 1, surface, action}, 0, spells, track 1, 0, 0)` | `Owner::Object` registered with `RegisterObject` (its `Get3DSoundPos` 0x6D1000: the atom with ground if SnapToGround; without an atom, the last point) |
| `spell_sounds::ProcessTurn` | fn_006D11A0: without an atom, `LHSampleIsPlaying` directly (none → it is deleted), fade `max(vol − FadeStep, 0)` with `LHSampleSetVolume` on **that** channel, and once `SamplePlayAnimEffect(this, 0, key, 1 + SoftRelease, …)` (1 stop, 2 release the loop); with an atom, Looping or expired Delayed → again within 1200 of the camera | `PlayingChannel`, `Volume`/`SetVolume`, `PlayAnimationEffect` with `AnimAction::Stop`/`Release` |
| `FireSound` | fn_0072EDE0 per turn and slot: options InGame, sample 2, owner the fire, 3D, track 1, at `Get3DSoundPos` 0x72EE70; the .sad gives it loop −1 and mode 2 (FLAGS 0x7E0); when the fire starts and stops it: [The fire's sound](../world/fire.md#the-fires-sound) | `PlaySoundEffect(PlayOptions)` per turn; `StopSoundEffect(2, owner, InGame)`; `ToBeDeleted` empties the fire's first slot (and, in openblack, the other one if it had it) |
| `HandSystem::BeginApplyOnRelease` / `EndApplyOnRelease` | `SoundTag::Create(GInterface, 3, track 0, mode 2, −1, 0, 3D, IN_GAME, 0)` 0x5D275E; `SoundTag::Remove(this, 3, IN_GAME)` 0x5D27C8 | `tags::Create`/`tags::Remove` with the left hand's entity (⚠️ approximate: its `Transform` instead of the newest point of the mouse buffer of `GInterface::Get3DSoundPos` 0x5CEC50); on release, the loop finishes its pass (`ToBeDeleted` 0x71ECB0) |
| `Gesture` (recognised) | fn_006882F0 (the record's state is `MyInterface`'s): 2D `PlaySoundEffect(0, 0x24, 3, 0, 0, 0, IN_GAME)`; another interface: 3D options, track 0, owner the atom's data, at the record's +0x3C | the same (`RecognisedGesture::fromInterface` / `handPosition`) |
| `Fireball::FlyBySound` | 0x683184: < 40 from the camera now and > 40 before (strict, 0x68322B), speed² > 400; `PlaySoundEffect(0, 0x40 + GetTickCount() % 5, 2, 0, 0, 0, IN_GAME)` | the same with `TickCount()` (before, a counter of its own and `>=` in the previous step) |
| `hand_fx::RemoveHandSpellVisuals` (was `DoRemoveFromHandVisual`) | options InGame, 0x77, owner the PHandFX, 2D (0x68CEE1) | `PlayOptions`, fixed `Owner::Object` |
| `hand_fx::AddSpellToHandVisuals` | `PlaySoundEffect(0, 0x23, 3, 0, 0, 0, IN_GAME)` 0x68DE7D | the same |
| `seed::SetPowerUp` (fn_00729C40) | PU 0/1/2 → `PlaySoundEffect(0, 10/11/12, 2, 0, 0, 0, SpellDialogue)` | the same; with the script widescreen it is removed by the userParam 1 filter, as in the original |
| `FinishCast` (was `DoPostCastThings`), tribal power | 0x72930A: for a tribe whose tribal power is above 1, after the tribal power column, `PlaySoundEffect(0, 0x1B + tribe, 2, 0, 0, 0, SpellDialogue)`, for my interface's seed and another's alike (never in the vanilla game, [The hand](../magic/hand-casting.md#the-hand)) | the same |
| `PlayFullyChargedSoundFX` | 0x77F4E0: table 0x77F5F8 by seed type (> 0x1D → 8), `PlaySoundEffect(0, voz, 2, 0, 0, 0, SpellDialogue)` | the same |
| `PlayTapSound` | fn_00726490: options InGame, 0x2A, no owner, 2D, pitch +0x48 from the table {100, 115, 130, 145, 155, 175, 190} with the index in 0..5 | the same; the mask +0x1C stays at 0 (the .sad of 42, FLAGS 0x402, does not have the pitch bit 0x1, so the options' pitch applies without a mask) |
| `one_off::InterfaceTap` | options InGame, 0x6D, owner the orb, 3D, track 0, at the +0xC8 of the interface state (0x72A6F4) | the same (⚠️ approximate: the left hand's interaction point instead of +0xC8) |
| `teleport::MoveByTeleport` | `SoundTag::Create(MapCoords&, 0x27/0x26, track 0, mode 2, 0, 0, 3D, IN_GAME, 0)` 0x5EC358/0x5EC372: departure at the being's MapCoords, arrival at the argument's | `tags::CreateAtMapCoords` (before, a loose emitter with the arrival at height 0) |
| `FireGraphic` (steam) | fn_00731AB0 → fn_007314E0: options InGame, 0x35, owner the FireGraphic, 3D, track 0, at its +0x98 | the same (🔍 inferred: +0x98 = the position of the burning object; its writer has not been read) |
| "Audio Player" debug panel | — | "Sound" plays the sample in 2D through `PlaySoundEffect`; "Music" starts a MUSIC_TYPE like START_MUSIC (`ScriptStartMusic`) and stops it like STOP_MUSIC; the "Emitters" tab and its floating volumes disappear (the effects master is in "Channels") |

- `spell_sounds::StartSound`: 0x674752: the Delayed flag waits distance / 347 s (0x6747AA)

#### Audible changes (because of the original)

- Everything from the miracles takes one of the 16 channels, with priority, stealing and QMixer's law; it passes GAudio's filters
  (script widescreen, SET_GAME_SOUND, citadel, interface states).
- The ParticleSound fade lowers the channel's 0..127 volume (before, a fraction of the emitter).
- The fire's crackle follows the object (track 1); when removed, the gesture loop finishes its pass instead of being cut.
- The recognised gesture of another interface, the steam and the teleport sound in 3D at their point.
- The passing fireball chooses its sample with `GetTickCount() % 5`.

#### In game

Land 1: with a lightning bolt and a test fire, S_HandLightning_Final plays on a channel with `owner object 1` and
G_Fire_01 on its channel every turn without restarting (mode 2); the fireball plays S_Fireball_S_01 and
S_FireballHitSolid_L_01, and when the atom dies it is stopped and deleted (three fires take turns on the two slots); an
armed FIRE seed plays G_HandGesture_02 in 3D on the tag's channel while pressed and ends on release; the power-up band
G_SpellPowerUpBand plays in 2D (the PU 10 voice is filtered by the intro widescreen, as in the original); a tapped FIRE
icon plays G_ClickOnSpell_01 with pitch 100 and the voice HELP_TEXT_ANNOUNCER_VOICE_FIREBALL_01 when it charges.

#### Audit: miracles

Reviewed against the disassembly: 0x726490 (the pitch table {0x64, 0x73, 0x82, 0x91, 0x9B, 0xAF,
0xBE} and the 0..5 clamp of 0x7264B7..0x726513, the pitch at +0x48 after the `push`), 0x6882F0 and 0x6885BB..0x68865B (2D with
mode 3 / 3D options with the atom's data as owner and track 0), 0x68CE90..0x68CEE1 and 0x68DE69..0x68DE7D, 0x72A660..0x72A6F4
(owner the orb, 3D, track 0, the point +0xC8, and the `ToBeDeleted` right after), 0x5EC340..0x5EC372 (the nine arguments
of `SoundTag::Create`), 0x72EDC0/0x72EDE0 (bit 0x20 before the trigger, track 1) and 0x72EC79..0x72ECE4 (only the first
slot, and the recalculated maximum), 0x5D274D..0x5D275E and 0x5D27C3..0x5D27C8, 0x6D1000 and 0x6D11A0..0x6D137A (the key
{+0x24, +0x28, 1, +0x20, +0x1C} in both places, the action `1 + SoftRelease`, the fade `max(+0x38 − FadeStep, 0)`,
the strict `< 1200²`), 0x674740..0x6747B4 (the distance/347 delay), 0x683184..0x68327F, 0x7314E0/0x731B0F,
0x77F4E0..0x77F5EE (`> 0x1D` → 8) and, in the DLL, 0x10014010 (the first channel of the bank and owner, +0x8C == 1, nothing with
+0x14 at 0). They all match. The `.sad` files confirm what the code claims: 42 has FLAGS 0x402 (without the pitch bit
0x1, so the mask +0x1C at 0 is correct) and voices 10/11/12 of SpellDialogue have userParam 1 (they are removed by the
widescreen, as the code says). There are no new ECS component dependencies in `src/Audio`, no
`CreateEmitter`/`PlayEmitter`/`AudioEmitter` outside, and each `RegisterObject` is undone: ParticleSound in `ProcessTurn`/`Clear`
(the `shared_ptr` stays alive in `g_Sounds` until the `UnregisterObject`), FireEffect in `sound::Free` (which `ToBeDeleted`
always calls, before `g_Pool` frees it) and in `sound::Clear` (before `g_Pool.clear()`); the new map goes through
the particle system's `Reset` and `magic::OnLoadMap` → `ecs::fire::Clear` before `audio::ClearMap`.

Fixed in the audit:

- There were no tests. `test_sample_play` has three new ones (15 in total): `OwnerChannelAndTheFadeOfAPSysSound`
  (the first channel of the bank and owner, another bank or owner → none, the fade 127 → 97 → 0 with its gain, and when
  the wave ends there is no channel any more: the ParticleSound is deleted), `OwnerChannelNeedsActive` (0x10014018) and
  `TapSoundKeepsTheOptionsPitch`, which with the real 42 checks the **pitch 175 of placement 5** (what could not be seen in game) and that with the `.sad`'s bit 0x1 the bank's pitch would win.
- `ParticleSoundPosition` (was `PSysSoundPosition`) asks for `atom->drawn`, which 0x6D1000 does not look at: the comment now marks it as openblack's (the +0xF4
  is only written when drawing the atom).
- `Audio.h`: an `Owner::Object` that only plays channels without track does not need `RegisterObject` (PHandFX, steam, gesture).
- `ECS/Trees.cpp` (no owner): the growth of the watered tree drew the sample with `Locator::rng` although its comment
  cited `GetTickCount() % 9`; now it uses `audio::TickCount()` like 0x74C4B3..0x74C4C0.

In game (Land 1): with `OPENBLACK_TEST_FIRE` and the camera next to it, G_Fire_01 stays on **one** channel
(index 2) for 72 consecutive turns without restarting (mode 2) with `owner object 1`; the only handle change was at the beginning, with
the intro camera grazing the sample's maximum of 80: the fire takes one channel.

### Weather and alignment in openblack

With the weather (`src/ECS/Weather`) and the alignment (`src/ECS/Effects/Alignment`) in the base, the ambience and
the music read the real values. Only through those modules' public APIs and through `GameQueries` (the audio does not include the
ECS: `src/ECS/AudioQueries.cpp` registers it).

- **Weather** (`GameQueries::weatherSmooth`):
  `GCamera::Update` fills GCamera+0x80 with `LH3DAtmos::GetWeatherSmooth(camera position, 1)` 0x835180 and
  `GSoundMap` reads it (temperature, rain, snow, overcast, wind x/z: weatherFade, RAIN, WIND). openblack:
  `audio::CameraWeather()` asks for `weather::atmos::GetWeatherSmooth(camera origin, true)` when
  computing the sound map. `GetWeatherSmooth` has no state (bilinear between cells, the grid cache per
  frame), so asking for it again on the audio turn gives the same as GCamera+0x80 **(approximate: the original
  takes it in the frame's GCamera::Update, openblack on the audio turn with the camera at that moment)**.
- **GAudio+0x190** (`GameQueries::cameraAlignment`, `ecs::audio_queries`): `fn_0064AC30` (at the end of
  `GPlayer::ProcessPlayers` 0x64A697, once per turn) calls `fn_005E2240(x)` with x = clamp((alignment of the
  most influential player at the camera + 1) / 2, 0, 1) = `ecs::effects::alignment::GetInterfaceAlignment()`.
  fn_005E2240 (0x5E2240..0x5E2291, in float steps because of the 24-bit FPU): x < 0 → 0 (`fcom [0x8AA398]; test ah, 1`),
  x > 1 → 1 (`test ah, 0x41`), s = (1 − x) + (1 − x) (to [0xBF337C], the sky), +0x190 = 2 (0x8AB478) − s − 1. The sky
  uses the same x, so openblack's sky settings (`OPENBLACK_TEST_SKY_ALIGNMENT`, the debug
  slider) also reach the audio: if `Clouds::InfluentialPlayerAlignment()` is not 2x − 1, x = (value + 1) / 2.
  The audio no longer includes `3D/Clouds.h`: `atmos_banks::Alignment()` reads the query (0 without it, `GAudio::Reset`
  0x426CC2). The audio turn goes after the magic turn (Game.cpp), like `GGame::EndTurn` after `ProcessPlayers`.
- **Ambience group** (`ProcessAtmosBanks` 0x428FE0, 0x428FFA..0x42901E): for each bank,
  `LHAtmosSetGroup(bank, +0x190 > −0.59999999999999998 (double 0x8C4A08) ? 1 : 2)`; the loops and one-shots of another
  group fade out or do not start. Trace: `(openblack) Atmos group g (alignment a)` when it changes
  (`OPENBLACK_ATMOS_TRACE`).
- **Alignment music** (`ProcessAlignmentMusic` 0x4279C0, `fn_00427460`): `GetDiscreteAlignmentValue` 0x414730
  of +0x190 (0..6), the table 0x9C99F0 (0,0,1,1,1,2,2) and GENERIC_EVIL / NEUTRAL / GOOD = index + 1 (0x427579). Trace:
  `(openblack) alignment music type t (GAudio+0x190 a, discrete d)` on change (`OPENBLACK_MUSIC_TRACE`).
- **Town and tribe** (`ECS/AudioQueries.cpp`, `NearestMusicTown` / `KeptMusicTown`, both through `MusicTownOf`, which
  reads a town as fn_00427460 does and gives nothing once the entity is no longer a town, as `GameThing::IsAvailable`
  at 0x4274AF; the camera's point is `CameraMapCoords`): fn_00427460 receives
  GetCamera()+0x14 (0x427A3B..0x427A46) = the MapCoords of `Camera::GetOrigin`, the same point as
  `GameQueries::camera` (GCamera::UpdateGameThingWithPosData 0x442EF0 takes them from LH3DTech::g_camera, 0x442EF3..0x442F35); its +8, the height that 0x4274C4 compares, is y − the altitude byte of the camera's cell × 0.67,
  without interpolation, or y outside the map / without a block (0x442F38..0x442FCE; before, openblack used the interpolated GetHeightAt);
  `nearestTown` = fn_00602160 (`map_cells::GetNearestTownWithCentre`, strict <, only those
  of +0x9A4) with townTriggerOffDistance; the tribe, the town's `Tribe` component (Town +0x5B8, 0x42753D /
  0x42755A); the distance, `gutils::GetDistanceInMetres` 0x74CD70 (0x4274EC, 0x427522); `town` = the town of
  GAudio+0x18C while it is valid (IsAvailable 0x4274AF). The logic (≤ 300 and height < 400, 0x4274C4..0x427535) was already
  in `GameMusic::AlignmentMusicType`. The other test of fn_00602160, fn_00741020 (0x6021B3), is
  `map_cells::TownHasCentre` (a TownCentre among the buildings +0x754, IsTownCentre vt+0x1E0 0x55DB70, or a planned
  building +0x9A8 with GetAbodeNumber 0x401260 == 0xC; vt+0x44 is GetAbodeNumber, not GetComputerSeen): a town without
  CREATE_TOWN_CENTRE gives tribe music.
- **Guidance**: `PlayResourceDropRemark` 0x71B570 looks for the town with `nearestTownAt` = `map_cells::GetNearestTown`
  0x6020E0(100, 0x98013C) at the point's MapCoords; its three values (`townResourceNeeds`, GetResourceDropSample
  0x71B5F0: Town +0xC4/+0x108/+0x19C… = TownDesire +0x90/+0xD4/+0x168 of desires 0, 1 and 10) come from
  `ecs::town_desire::GetField` (Raw + Boost + BoostA, in that order and in float). `desireTowns`: the towns from
  `map_cells::ForEachTown` with `GetSortedRawDesires` (+0x378: value +0x37C, type +0x380) and `GetRawDesire` 0x73E420.
  `worshipSites`: PLAYER_ONE's citadel (`worship::citadel::Of`), its six slots (`WorshipSitesOf`), +0x70
  (`StrainSoundFraction`; the cap at 0x71B319 lets a NaN through, test ah, 1), DancerCount > 0 and
  `worship::site::CalculateDesireForFood` 0x77C310. `heartBeat`: see [Chants and heartbeat](#chants-and-heartbeat-in-openblack).
- Temple check: in Land 2 the human's citadel starts without worship sites, so it is
  tested as player 1: `-s Land2.txt OPENBLACK_TEST_WORSHIP_PLAYER=1 OPENBLACK_TEST_WORSHIP="1,0.5"
  OPENBLACK_CAMERA_FLY="2540,150,1740,2540,100,1800" OPENBLACK_GUIDANCE_TRACE=1` → site 148 with dancers and
  food 1.0 → `text 4993 3D -> Guidance.sad 24` and `4992 -> 23` (NEEDWORSHIPPERS_FOOD).
- Desires check: in Land 1 nothing sounds, as in the original (PlayNow 0x71AF6F..0x71AF8F:
  TownDesire is not "always" on land 1 without multiplayer). `-s Land2.txt OPENBLACK_GUIDANCE_TRACE=1
  OPENBLACK_CAMERA_FLY="2187,120,2200,2187,20,2260"` (store of town 94 at 59.9) → `desire sample 4982 value
  0.346` → `type 0 text 4982 3D -> Guidance.sad 143` (DESIRE_EXPAND), and the value drops on repetition (t0 of DesireScore).
- Check (Land 1, `OPENBLACK_TEST_ALIGNMENT_MUSIC=1 OPENBLACK_TEST_TEXT_CLICK=1 OPENBLACK_MUSIC_TRACE=1`,
  `-n 60000`): `OPENBLACK_CAMERA_LOCK="1850,90,2620,1865,30,2650"` (town 0, NORSE, with a centre) → `alignment music
  type 23`, `MUSIC_TYPE_NORSE_TOWN_NEUTRAL`, **celt_neutral.sad** plays (22..24 point to the Celtic strings);
  `"2440,90,2560,2450,30,2580"` (town 4, AZTEC) → type 8, `AZTEC_TOWN_NEUTRAL`, **aztc_neutral.sad**.

**In-game check** (Land 1):
- `OPENBLACK_TEST_WEATHER="1818,2628,100,100"` with `OPENBLACK_CAMERA_LOCK="1775,60,2595,1830,45,2650"` and
  `OPENBLACK_ATMOS_TRACE=50`: `ATMOS_TYPE_RAIN Vol=1.000 Sent=127`, the `rainconst.wav` loop
  from rain.sad starts and `thunder_*.wav` one-shots sound; WIND 0 (6 m/s wind in the test storm).
- `OPENBLACK_TEST_SKY_ALIGNMENT=-1`: `Atmos group 2 (alignment -1.000)`; without it, group 1 with 0;
  with 0.8, group 1.
- With `OPENBLACK_TEST_ALIGNMENT_MUSIC=1` and `OPENBLACK_TEST_TEXT_CLICK=1` (the Land 1 script does
  `ENABLE_DISABLE_ALIGNMENT_MUSIC(0)` at the start), when intro.sad ends: with −1, `alignment music type 1 (… −1.000,
  discrete 0)`, `MUSIC_TYPE_GENERIC_EVIL` and evil.sad plays; with 0.8, `type 3 (… discrete 6)`, `GENERIC_GOOD`, good.sad.
  Without the hook, `Music Playing=NONE` after the intro (the script has it switched off), the same as before.

**Audit.** Reviewed in the disassembly: fn_005E2240 0x5E2240..0x5E2299, fn_0064AC30
0x64AC30..0x64ACAB, the call 0x64A697, ProcessAtmosBanks 0x428FFA..0x42901E (`push 2` at 0x429015), GAudio::Reset
0x426CC2, fn_00427460 0x427466 / 0x427579, ProcessAlignmentMusic 0x4279C0..0x427A46, ENABLE_DISABLE_ALIGNMENT_MUSIC
0x710120 (g_game+0x250090 +0x94), GetDiscreteAlignmentValue 0x414730 and fn_00426C80; the callers of
ProcessAudioGameTurn 0x427080 (GGame::EndTurn 0x54E9A6, after ProcessTurn, and Temple::ProcessGameTurn 0x794A5A) and of
GAudio::Reset (GGame::Init 0x54F474, ClearMap 0x552D98). Everything adds up. Fixed: the clamp of fn_005E2240 with a NaN
(the `fcom` leaves C0 as unordered: `test ah, 1` takes it to 0, +0x190 = −1; openblack took it to 1); the formula is now
in `ecs::audio_queries::AudioAlignmentValue(x)` (was `GAudioAlignment`) and the group threshold in `atmos_banks::GroupFor(alignment)`, with new
tests (`AudioLaws.AlignmentCurve`, `AudioLaws.AtmosGroupByAlignment`: the float −0.6 is below the double
−0.59999999999999998, group 2). No new AL sources or buffers, no ECS components in `src/Audio`, the query is
read only on the game thread (neither the music thread nor its callbacks use it). In game
(with `-l stdout`): `Atmos group 2 (alignment -1.000)` / `Atmos group 1 (alignment
0.800)`; with −1 and the music hook, `alignment music type 1 (… discrete 0)`, GENERIC_EVIL and evil.sad. Notes: in the
citadel the original runs the audio turn from Temple::ProcessGameTurn without fn_0064AC30 (+0x190 keeps the
last value); `GetDiscreteAlignmentValue` with a NaN (0x414756 lets it through to `__ftol`) is not matched (it does not
happen: +0x190 is never NaN).

### Chants and heartbeat in openblack

The audio does not include the ECS: the game
answers through `GameQueries` (`src/ECS/AudioQueries.cpp`).

**Chants** (`GAudio::ProcessChantMusic` 0x427790, branch 6 of `ProcessMusic` 0x427E4D). Only `ProcessMusic` calls it;
worship does not call the audio (the dance, `Dance::ProcessDances` 0x50BB60, does not touch the music).
- Game side (`GameQueries::chantSite`, `ChantSite` in AudioQueries.cpp; it gives nothing where the original leaves at
  0x4277C3 and 0x4277E8):
  `map_cells::GetNearestCitadel` 0x602200 (150, 0x43160000) at the camera's MapCoords;
  `worship::citadel::FindNearestWorshipSite` = fn_004639A0 (100, 0x42C80000): of the six slots in order, the one that has
  dancers (fn_0077B960) with the dance centre (fn_0077CD90 = `GetSpecialPos(8)`) nearest,
  `GetDistanceInMetres` < best (strict). It returns the tribe (fn_0077C2E0: +0x8C → +0x10), the dancers
  (Dance +0x90), the centre as `GetLHPoint` and the `GetAltitude` of the centre and of the camera.
- Audio side (`GameMusic::ProcessChantMusic`): camera height = `GetAltitude(camera)` + MapCoords+8;
  |height − ground at the centre| < 100 (0x427855; a NaN passes, `test ah, 1`); type = `ChantMusicType` (fn_00427430:
  table 0x9C9A30 `{28,28,30,32,34,36,38,40,42}`, the African one with the Celtic one, + 1 with more than 8 dancers (`jbe`,
  unsigned), 5 from tribe 9); without a bank, nothing; options: vol 127, start GAudio+0x18[group − 1] read before
  `SavePositions` (0x4278FF / 0x42792D), sync 1, **no fade**, 3D, pitch 100, the position; `LHMusicPlay` every turn and
  `Set3DPosition` if it got a channel; "Music Playing=…"; then `ProcessMusic` sets NONE (0x427E95).
- `_vox` is played alone (the whole chant with voices; confirmed by the user): it is another bank, not a layer on top.
- **🔍 inferred**: every openblack worship site has its dance (it is created with it), so +0xA0 is never null; a
  centre without a mesh point stays at MapCoords 0, like the original's zeroed MapCoords.
- **⚠️ approximate**: without a camera there is no chant (the original always has one).
- Tests: `ChantMusic.Table`, `GameMusicTest.ChantMusicAtTheDance` (test_game_music.cpp).

**Heartbeat** (`GameQueries::heartBeat`, `HeartBeat()` in AudioQueries.cpp; the calculation was already in `guidance`).
- `protectionDesire`: Σ `Town::GetRawDesire(3)` 0x73E420 of PLAYER_ONE's towns (`map_cells::TownsOf`, the
  order of the list +0xA50), summed in float.
- `believers` = `magic::players::BelieverFraction` 0x64B680: men + women (TownStats
  +0x54/+0x58 = Town +0x664/+0x668) of their towns divided by `WorldPopulation` (g_game+0x205A54: +1 in the Villager ctor
  0x74FAFF, −1 in SetDying 0x76A552 once, with the flag +0xE0 & 0x40, or in the dtor 0x74FBD4 when that flag is not
  set); 0 if either is 0. openblack counts the Villager entities that are not counted out
  (`magic::players::WorldPopulation`, the flag `k_FlagCountedOut` set by the death), which is the same count: every
  villager is made and deleted through those two paths (test `VillagerDeathTest.WorldPopulation`). Its other reader is
  `TempleRoom::AddText` (0x79B747).
- `beliefShare` = `influence::InfluencePowerRatio` = fn_0064B700: **Σ +0x8C of the active players and the neutral one
  divided by one's own +0x8C** (`fdivr` 0x64B74F; 0 if one's own is 0). The query said the opposite (one's own divided by the
  sum); the comment was fixed, Guidance does not change. **🔍 inferred**: active (+0x8E0 ≠ 0,
  GetNextActivePlayerAndNeutral 0x550930) = the player has an entity (`magic::players::EntityOf`).
- GPlayer +0x8C = `influence::CalculateInfluencePower` 0x64AD00 (from GPlayer::Process 0x64971D, after the
  alignment; in openblack `influence::CalculateInfluencePowers` in slot 3 of `magic::ProcessTurnStart`):
  `Citadel::GetInfluence` (with heart) + Σ Town +0x5C8 + Σ radius +0x38 of the player's rings (also the anti ones),
  in float. +0x90 and the GameStats history have no reader: not ported. **⚠️ approximate**: it is stored with the land
  (`InfluenceGlobals::power`), not in the GPlayer.
- `citadelHeart`: the citadel (+0xA48) with a built and living heart (`worship::citadel::HasLivingHeart`; 0x71C574..
  0x71C5A6) → its position. **🔍 inferred**: openblack's heart is the temple itself, built with it; its life is
  `ecs::life::LifeOf` of the temple (1).
- Nearby enemy creatures (0x71C2A6..0x71C379): for each player slot and the neutral one (`GetNextPlayerAndNeutral`
  0x550980), its creature +0xA4C when its interface status is not the guidance's (+0xCC), the nearest town by whole
  cells `GetNearestTown` 0x601F90 (no excluded town, tribe −1) and, when that town's player has the guidance's
  interface status, the u32 distance (then 1 − max(d − 100, 0)/400 for d < 400, [0x98016C] = 100, [0x980170] = 400).
  openblack: `PlayerCreaturesNearTowns` (the primary creature of each `PlayerNames` slot, `map_cells::GetNearestTownCells`,
  the town's owner) and the pure `ecs::audio_queries::EnemyCreatureDistances` (another player than the local one, the
  neutral one included, near a town of the local player). Test: `HeartBeatEnemies.OtherPlayersCreaturesNearTheLocalPlayersTowns`.
- Test: `InfluenceTest.influencePowerAndRatio` (test_influence.cpp).

### Test hooks

The audio hooks and traces are listed together on [the audio engine](engine.md#test-hooks).

## Pending

### (Approximate) and 🔍 inferred: miracles

- (Approximate) the gesture loop uses the left hand's entity instead of `GInterface`; the one-off orb sounds at
  the hand's interaction point instead of `GInterfaceStatus` +0xC8.
- (Inferred) `FireGraphic` +0x98 = position of the object.
- (Approximate, from the magic code, unchanged) `FireSound::Consider` does not stop the slot's sound when it is the same fire
  (fn_0072EDC0 at 0x72F82C); the owner's alignment in the PSysSound key (0x674678) is not filled in.

- Sound: the owner's alignment, the game-state filters, the finite repetitions
  and the global distance cap (see [Sound of the particles](#sound-of-the-particles)).

### From the audio engine's list

- The heartbeat: not tested in game.
