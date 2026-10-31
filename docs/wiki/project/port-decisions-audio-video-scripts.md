# Port decisions: audio, video and scripts

## Summary

How the sound, film and script code of openblack was fitted onto raffclar's architecture: for each piece, whose
names, interfaces and layout were taken, whose bodies stayed, and what of his was left out and why. The rule is the
one in the project's CLAUDE.md: his architecture is the base; where both trees do the same job his version is taken
only once it is shown to behave as the original, bit for bit; otherwise ours stays behind his names. Each decision
below is one porting row; the behaviour it reproduces is on the page of that system
([the audio engine](../audio/engine.md), [voices](../audio/voices.md), [Bink](../video/bink.md),
[script audio](../audio/script-audio.md)).

## The original

Nothing of the original lives here: each decision points to the page that holds the behaviour it keeps.

## openblack

The decisions, grouped by area. "His" is raffclar's tree (the port target, his `master` at `0222f9f5`, and the newer
pieces taken from it); "ours" is the tree the checks measure.

### Sound

- **Audio service:** his `Locator::audio` slot and `AudioManagerInterface` (with a virtual destructor) front
  our engine. `AudioManager` is the engine, `AudioManagerNoOp` only a base for test fakes, never a device
  fallback. The free functions of `Audio/Audio.h` are thin wrappers over the slot (`Audio/AudioApi.cpp`); a test
  injects a fake through the slot with `RestoreService`. The slot is reset after the registry.

- **Sound tags:** his `soundTagSystem` slot and the names of his `SoundTagSystemInterface` and `SoundTagSystem`
  front our port of the original's tags. Only the names are his: the signatures are ours (`ProcessTurn()` without the
  camera, `SetActive` and the rest on a `TagId`, two `CreatePointSound` overloads with the original's arguments, plus
  `Create`, `CreateAtMarker`, `Remove`, `Delete`, `Exists`, `Clear`, `TagSoundPoint` and `Point`), so a caller of his
  would have to be adapted to them; the two his tree has (the weather's thunder and the street lanterns) are ours
  already, through `audio::tags::Thunder` and `audio::lantern_sounds`. His tags are entities with an audio
  emitter; ours stay one list inside the system, as the record `SoundTagSystem::Tag` of the implementation,
  because the original walks its tags newest first with ids as channel owners, and a land without tags must get no new
  storage. His component name `components::SoundTag` is left free for a later port of his code. The functions of
  `audio::tags` hand their calls to the slot, which is emplaced and reset next to `audioState`, where the tags' state
  lived before. The ambient point tag (the thunder's) is a `CreatePointSound` overload that takes an `AtmosType`. The
  tests inject the system through the slot and call it on their own reference.

- **Thunder:** his `lightning::k_ThunderClaps` and `k_FirstThunderClap` (eleven claps of the rain's bank, from the
  second) are taken into our `3D/Lightning.h`. The rest stays ours. The clap is `lightning::ThunderClap(ticks)`, picked
  from the wall clock's milliseconds as the original does, where his draws a C runtime number, which would add a draw
  to the random trace at every clap. The sound is `audio::tags::Thunder`, a delayed point tag in the rain's ambient
  bank. It is our storms' sheet lightning callback, which `audio::Init` sets and `audio::Shutdown` clears, so his
  thunder code inside the weather's update is not taken. The ticks come from `audio::TickCount()`, the game clock,
  which is fixed in the deterministic runs. His `ThunderSound(clap)` (a hashed sound id) is left out: our tags
  address the bank by sample number. His strikes near the camera are left out too.

- **Hearing range:** his `Audio/HearingRange.h` names, `SoundEffectStartRange` (the bank's maximum distance, the
  play's when the bank gives none, 9999 by default) and `AnimEffectStartRange` (no fallback), are the arithmetic our
  `PlaySoundEffectOptions` and `anim_effects::Play` already did. His `IsWithinStartRange` (`distance <= range`) is the
  animation effects' test, which takes the caller's distance as the original does; the global limit of 800 beside it
  stays, his has none. The sound effects' test is ours, `IsWithinStartRangeSq(distanceSq, range)`: the original
  compares the squared camera distance with the squared range and culls only when it is more, so a sound on the edge
  and a distance that is not a number both start. His square root and `<=` would round differently at the edge and
  cull a NaN. His three `test_hearing_range` cases hold for the original's rule and are taken as he wrote them,
  beside our case for the squared test.

- **Sound effect rules:** his `Audio/ScriptSoundEffect.h` names are taken for what our `FilterReason` already did:
  `SoundEffectUse` (the user parameter, compared whole), `SoundEffectConditions` and `SoundEffectHeard`, with his
  `CurrentSoundEffectConditions` in `Audio/Game/AudioSystem.h`, built from our audio system's state and queries. His
  `ScriptSoundBank` is an alias of our `SfxBank` (the same numbers), so there is one bank enum; a registered bank is
  turned into its type in `AudioSystem.cpp`. `FilterReason` asks `SoundEffectHeard`, then each condition alone for its
  trace text. His `ScriptSoundBankFile`, `BankOfFile` and `InCreatureFightControls` are not taken: the first two
  belong to his script sound effects, which ours has in `Audio/Services/ScriptSound`, and the third guesses the fight
  states from the fight, the camera and the hand. `creatureFightControl` means the interface action states CREATURE
  FIGHT, FIGHT_APPLY and FIGHT_SELECT: the first is set while the local player directs its creature's fight
  (`ecs::audio_queries::LocalPlayerDirectsFight`, since 2026-10-21), the other two are not ported
  ([the audio engine's list](../audio/creature-and-interface-sounds.md#from-the-audio-engines-list)).
  His `GameSoundEffects` wrappers (`PlayGameSoundEffect`, `StartGameSoundEffect`, `GameSoundEffectHeard`) and the
  call sites moved onto them are not taken: our `audio::PlaySoundEffect` already filters every sound effect as the
  original's does, so a second filtered wrapper would only add a name.
  His behaviour is not taken where it differs from the original's, which ours already does:
  - the sound effects' cull: his culls any placed sound, where the original culls only a 3D one with a sample number
    (his square root and `<=`, above);
  - the animation effects: his have no limit of 800;
  - a delayed point sound: his starts anywhere when its bank gives no distance, the original never; his `<=` where the
    original's is a strict `<`; his seconds are turns × 0.1, the original's turns × the turn's milliseconds × 0.001;
  - the camera's woosh: his plays on every flight, the original's only when the camera moves more than 150 away.

- **Sampler defaults:** his `Graphics/ShaderSamplers` and `Graphics/SamplerDefaults` as he wrote them, his
  `ShaderProgram::Submit` on every draw and his `ShaderManager` reflection, `DiscardBindings` and `FrameEnded`; our
  `ShaderProgram` keeps its sampler flags, its uniform arrays and its warn-once log.

### Voices and help

- **Help speech:** his `helpSpeechSystem` slot and `HelpSpeechSystemInterface` front our help texts' voices
  (`audio::voices`), with his `SpeechVoice` (the first voice is our narration owner, the second the extra one) and
  `SpeechBank` names in `Audio/HelpSpeech.h`. `HelpSpeechSystem` keeps nothing: `Say`, `IsSaying` and `Stop` call
  `voices::Say`, `voices::IsSaying` and `voices::Stop`, the last being what the click's cut always did, now with the
  voice and the bank as arguments. GAME_PLAY_SAY_SOUND_EFFECT and SAY_SOUND_EFFECT_PLAYING go through the slot. Not
  taken: his `HelpSpeechTable` and `SpeechVoices` (our `VoiceTable` and the channels' owners do their job, measured
  against the original's table), so neither `SetTable`, `GetTable` nor `GetLineCount`, which read or count what only
  those two hold. His `GetSpokenTextCount` is taken: it counts the texts with a voice in our table
  (`VoiceTable::GetSpokenCount`, his table's name for it), 0 until the table is built.

- **Advisor voices:** his `help::AdvisorVoices` (`Help/AdvisorVoices`), held by the `helpTextSystem` slot behind his
  `GetVoices`, is the class of our advisors' voices: the free functions of `audio::advisor` that kept their state in
  the audio state became its methods, with our bodies (the 0.5 s stop of a sentence no longer playing, the spectrum
  lip sync, `Interrupt`'s arg path and its draws from the audio's local random stream, in the same order), and that
  state became its members. His names taken: `IsActive`, `GetSpeaker`, `GetSentence`, `GetLipSyncKey`. Ours kept: our
  signatures (`Say` reads the hover x that the spirits give it through `SetHover` once a frame, so the advisors' path
  still goes through `help::spirits`), `Interrupt(advisor, arg)`, `Init` (its bank now given by the slot's
  `InitVoices`), `SentenceTime`, `Amplitude` and the tests' `Reset`; the lip sync runs in the spirits' update (N2-M-4,
  below). The voices now
  live as long as the slot (made when the game's services are, gone with them) instead of in the audio state; both
  are made before the help system starts and go after it, so nothing reads them outside either. The game,
  the spirits' runtime, the voices' RUN_TEXT route and the debug windows reach them through the slot, stopping with a
  message when it is missing. `test_voices` injects a help texts slot and calls its voices; the help texts' loader
  test's fake slot holds its own.

- **Lip sync and the voices' seams (N2-M-4):** his names over ours, with our bodies. The lip-sync analysis moves from
  `Audio/Services/Advisor` to his `Audio/LipSync` (`audio::lip_sync`): `FastFourierTransform`, `Analyse`,
  `ComputeLipSyncKey`, `AutoVoiceParams` and `VoiceKey` become his `Fft`, `MagnitudeSpectrum`, `UpdateKey`, `Settings`
  and `Key`, with his `Band`, `Levels` and `k_Window`; the maths is ours, in our order of operations (the two trees
  give the same floats; where they are written differently, as `BandLevel`'s mean in double or the FFT's rounding
  steps, ours stays). `UpdateKey` keeps our guards and adds his check that the window lies inside the samples, which
  only matters where ours would have read past them. `Key` has no time (nothing read it). His `Help/SpiritVoice.h`
  holds `AudioTag`, `TagLabel`, `LipSyncFrame` and the tag parser's declarations, moved out of `Help/Spirits.h` with
  our signatures (the `lastWord` argument came with the cue labels' row, below). `AdvisorVoices` takes his `Audio` and
  `Hooks` seams and his `Recording`, `SayDelayMs`, `Say(advisor, line, onlyIfSilent, hoverX)`,
  `ApplyLipSync(advisor, dt)` returning the frame, `KeepRecording` and the `stopTags` flush at the line's length +
  100 ms; the help texts slot wires the seams to our engine (`HelpTextSystem::VoiceAudio`, the bank given by
  `InitVoices`). **Our play position is kept**: the mixer's position of the advisors' bank and owner
  (`sample_play::PlayPosition`), as the original reads it, not his emitter's progress times the line's length. Also
  ours: the line count read once at `Init` (his reads it at every line) and `Interrupt(advisor, arg)` with its text
  pick (two more seams, `textLine` and `insideCitadel`). The lip sync runs in the spirits'
  update, as his ([the lip sync in the spirits' update](port-decisions-hand-camera-interface.md#the-lip-sync-in-the-spirits-update)),
  and the help texts slot sets the hooks (`HelpTextSystem::VoiceHooks`: `setTags`, `stopTags`) since the cue labels'
  row (N2-M-5, below). His `test_lip_sync` and
  `test_advisor_voices` are taken whole, with our two lip-sync tests moved beside them; the voices' tests read the line
  count after `Init` and pass `Interrupt`'s argument (a delayed line now starts with `UpdateSaySentence`).

- **Wave cue labels (N2-M-5):** his `Audio/WaveCueLabels` (`audio::ReadWaveCueLabels`) and `test_wave_cue_labels` are
  taken as he wrote them (only his tree reads the labels). The tag parser takes his `lastWord` argument
  (`ParseAudioTags(label, time, lastWord, errors)`, `BuildSentenceTags(labels, lastWord)`): the word buffer that was
  ours in `Locator::scriptState` becomes his `AdvisorVoices::_tagWord`. Both live for the whole run, as the original's
  static buffer does, so the parser is a pure function of its arguments.

### Films and the screen

- **Sun glare:** his `ThroughOvercast` gives the glare's alpha too (`sun::GlareAlpha`), with the original's five
  samples hidden by our land ray cast (`sun::GlareSamplePoints`, `sun::HiddenSamples`); a behaviour change of its own.
  His sample list is the original's and is taken (`sun::k_GlareSamples`: the middle and four distinct corners), and
  so is his depth test of what is drawn in front of a sample, through occlusion queries (`_glareQueries`,
  `Renderer::AskGlareSampleDrawn`, `sun::GlareHidingDepth`), each a behaviour change of its own. Ours stays: the land
  ray first and the query only for a sample it does not hide and that is on screen (`sun::SamplePixel`), the drawn
  near clip for the nudge (his takes the camera's), our alpha, the test only at the detail levels that run it
  (`detail_level::GlareDepthTest` of `draw_list::DetailIndexOf`), a query not asked the frame before made afresh
  (his trusts its old answer), and no glare in the citadel (his draws it in the temple with a depth test). The
  query's answer comes a frame or two late, where the original reads that frame's depth buffer: a chosen
  difference (the user, 2026-10-21), since bgfx has no read back of the depth buffer within the frame
  ([the sun and its glare in openblack](../world/sky.md#the-sun-and-its-glare-in-openblack)).

- **Bink tests:** his `test_bink_file` and `test_bink_robustness` cases run on our `video::BikFile` and
  `video::BinkDecoder` (`test/video/test_video_bink_file.cpp`, `test_video_bink_robustness.cpp`), with his synthetic
  files in `test/bink/BinkTestFiles.h`. Our decoder
  takes a container rather than loose packets, so each fuzz run puts its damaged packets, in his order, into one
  synthetic file, each behind an audio packet of 0 or 1 byte so that any size is a valid frame. Left out where ours has
  no counterpart (`KeyFrameAtOrBefore`, `HasPicture`) or differs: his decoder refuses impossible picture sizes and ours
  does not, so his size refusals and his malformed-container fuzz are not run.

- **Bink decoder's place:** our decoder sits in his layout, `components/bink/include` and `components/bink/src`,
  with his file names where the file does the same job (our `BikFile` is `BinkFile.{h,cpp}`; the others already had
  his names). It is our code: his decoder is not taken. Our class and namespace names stay (`video::BikFile`,
  `video::BinkDecoder`, `video::bink`, `video::bink_yuv`); his are `bink::BinkFile` and `bink::Decoder` with other
  interfaces, so a rename would not bring the two together. So `BinkFile.h` declares `video::BikFile`, and its test
  suites are still `BikFile.*`. It is not a library of its own as his `bink` target is,
  because ours uses game headers: the container reads through the file system and the Locator, the decoder implements
  the player's `IVideoDecoder` (`src/Video/VideoDecoder.h`) and the colours are the game's 16-bit ones
  (`Graphics/Rgb16.h`). So it is compiled into `openblack_lib`, which adds `components/bink/include` to its include
  folders, and is included as `<BinkFile.h>`, `<BinkDecoder.h>` and so on, as he does. `OPENBLACK_BINK_DECODER` (on by
  default) still leaves out only the decoder's sources; the container is always built. The player, the falling spell's
  film and `VideoDecoder.h` stay in `src/Video`. Before his `components/bink/CMakeLists.txt` or a link to his `bink`
  target is brought in, the container's reading and the `IVideoDecoder` link must move out of the component.

- **The story's black opening:** his `CinematicDirectorSystem::StartStory`, as he wrote it (the script fade black at
  once), on our director. It is given where our `LoadStartLand` starts `LandControlAll`, under the original's
  conditions, a pure `new_game_choice::OpensOnBlack(started, landNumber, held)`: the story's script started, the start
  land is Land 1 (the number its script gives with `SET_LAND_NUMBER`), and the game was not held paused for the
  start-of-game question; with it the temple's fade is put at rest, as the original does. No other start land opens
  on black. His
  condition (the question not asked, as his asks only a returning player) is not taken: ours asks at every new game
  with the pause, as the original, so only a Land 1 game that shows no box (no window, or no in-game interface) opens
  on black. One started with `--skip-opening`'s answer does not either, as that answer stands for the question answered
  while the game waited.

### Scripts

- **Script natives with logic:** `CHLApi.cpp` only pops, turns ids into entities and pushes; the body is a free
  function over the injected service interfaces and the registry (`ECS/PlayerCreature`), tested with a recording fake.
  What it decides from the popped values is a pure plan (`PlanLoad`, `PlanScriptLoad`: species, body and point, or
  none), and two natives that make the same thing share the step that makes it (`LOAD_MY_CREATURE` and
  `LOAD_CREATURE` create through one helper), so they cannot drift apart.

- **A villager in a script's hands:** what the scripts do to a villager is his `villager_script`
  (`Implementations/VillagerScript`), under his names, with our bodies: `CanBeDirected` (available and in the map;
  the callers that refuse a drowning villager test that themselves), `SetScriptState`, `MoveTo` (MOVE_GAME_THING's
  walk, taken out of `CHLApi`), `SetScriptAnimation`, `HasPlayedScriptAnimation`, `ScriptClip`, the IN_SCRIPT and
  SCRIPT_PLAY_ANIM state functions (`InScript`, `ScriptPlayAnim`, their entries and exits, with our codes: an exit's
  1 lets the villager go, where his `bool` refuses) and the release (`ReleaseFromScript`, which his tree has not).
  The walk and the state helpers they use stay our villager's (`ECS/Villager/VillagerScript`: `SetupMoveToPos`,
  `AreWeThere`, `StorePreviousState`, `IsAvailable`, `IsObjectInMap`, `PlayAnimThenSetState`, WAIT_FOR_ANIMATION).

- **The script villagers' rules:** his `ECS/VillagerScriptRules.h` (`villager_script_rules`) holds our rules under his
  names: `ScriptDistance` (GET_DISTANCE: the game's distance, 0 under half a metre), `StepScriptClip`,
  `HasPlayedScriptClip` and `ScriptLetsGo`, which `villager_script` and the native call. `ScriptLetsGo` is the
  original's: the exit functions of the state it works towards and of the next compared (`HaveSameExitFunction`), and
  a state that is not a final one lets it go, where his lists the states held by a script and has no not-final
  clause. Not taken: his `KeepsWalking` (ours stops every walk but MOVE_TO_POS's) and his script speed conversion
  (ours is `SetVillagerSpeedInMetres`). His `test_villager_script` comes with our `ScriptLetsGo` cases and one more
  for a state that is not final; his speed and walking cases are left out.

- **Script debugger tests:** our debugger's five fixture cases (`DebuggerTest`: running freely, breakpoints, stepping,
  continuing, holding and changing variables) were his, word for word, so they live where he keeps them, at the end of
  `test/test_editor_scripts.cpp`, with the fixture. Our `UpdatingTheProgramKeepsTasksAndValues` uses the same fixture,
  so it moved there too, after his cases. `test/test_lhvm_debugger.cpp` keeps what he does not have: the idle debugger
  (`DebuggerIdle`) and writing a program out (`LhvmProgram`). Both files stay in the `test_ui_script` group. All of a
  gtest suite's tests must use one fixture class in the group executable, so the suite cannot be split across the two
  files.

- **Script spirits:** his `Help/ScriptSpirits` names (`help::script_spirits`: `DiscreteAlignment`, `HelpSpiritOf`,
  `SpiritWhoTalks`, `IsScreenFraction`, `k_GoodSpirit`, `k_EvilSpirit`) and his `test_script_spirits` front our
  bodies: the help system's spirit rules, the music's discrete alignment (`audio::DiscreteAlignment` now forwards to
  it) and the natives' screen check. `DiscreteAlignment` takes his steps, just short of seven: the original's
  constant is 6.9999995, not 7 as we had read it (2026-10-19, oracle; see `docs/wiki/audio/music.md`, "The discrete
  alignment"); his tests give the same numbers with either. A NaN now passes the cap as in his and the original's, and
  converts to the integer indefinite as the game's `__ftol` does. `CHLApi`'s spirit natives call
  his `ScriptHelpSpirit` and `CheckScreenFractions`; ours keeps the opcode in the invalid-fraction message and the
  local random stream of the audio code for the random spirit.

- **The script timers, ours behind his component:** the timer component is his `components::ScriptTimer` in
  `ECS/Components/ScriptTimer.h`, wrapping his `script::timers::Timer` (`setTurn`, `turns`) from
  `ScriptHeaders/ScriptTimers.h`. Only his struct is taken: his functions there (`TurnsFor`, `Set`, `SecondsRemaining`,
  `SecondsSinceSet`) work out the turns of some seconds in double precision and with a fixed 100 ms turn, where ours
  keep the game's single precision and its ms per turn, as the original does, so they are not taken. Our functions stay loose in
  `ECS/ScriptTimer.h` (`ecs::script_timer`, `ecs::script_countdown`) and read the component's timer.

- **The scripts' source in the editor, his code:** `Editor/Scripts/Decompiler.cpp` is his, on the Challenge Language
  decompiler and compiler (`lhvmdecompiler`, `lhvmcompiler`, now linked into `openblack_lib`), so the Scripts panel shows
  a script's source and compiles it back into the running program. `ScriptModel` reads numbers through his
  `lhvm::chl::ParseFloat`, the same test as our own copy of it, which was removed. His two tests of it
  (`EditorRecompilesAScriptIntoTheRunningProgram`, `EditorSourceView`) are in his files.

- **His challenge records, taken from his newer tree (#20).** `script::challenge_snapshots`
  (`ScriptHeaders/ScriptChallengeSnapshots.{h,cpp}`, the three snapshot natives' pops behind a `Stack` of two
  functions), `ecs::challenge_records` (`ECS/ChallengeRecords.{h,cpp}`) and `components::ChallengeRecord` are his, with
  his tests (`test_script_challenge_snapshots`, `test_challenge_records`, in the `test_ui_script` group). They are
  newer than the fixed target and taken now because they close the challenge list's row; every rule was checked
  against the original's record keeping run in the oracle (`test_challenge_records_original`). One change:
  `AlignmentChange` damps the change with our `effects::alignment::ScaleChange`, the same rule as his
  `disciple::DampedAlignment`, which our tree does not have. Records are entities with his component, and the reminder
  arguments' things are held through his `References` pair of functions, which the natives fill from the script
  objects service.

### Test hooks

None of its own: the hooks of each system are on its page
([audio](../audio/engine.md#test-hooks), [video](../video/bink.md#test-hooks)).

## Pending

- Each decision's open part is kept in the Pending list of the page of its system.
