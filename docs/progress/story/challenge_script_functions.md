# Challenge script language and functions

The story is written in the game's challenge script language and compiled into one file the game runs. Every script
command and native function, one row each, is in ../scripts/ (challenge_natives_*.md); the machine that runs the
scripts is in ../engine/script_vm.md. This file only says how far the story's needs are met.

**Progress: 6/12 done, 6 partial — 75%**

## What the story needs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game's own compiled challenge file is loaded and the story's top script started | done | `Game.cpp` loads `Scripts/Quests/challenge.chl` (`LHVM::LoadBinary`) and starts `LandControlAll`; see ../engine/script_vm.md |
| The language compiles and decompiles exactly as the game's | done | `components/lhvmcompiler`, `components/lhvmdecompiler`; tests in `test/lhvm/` (`test_chl_roundtrip.cpp`, `test_lhvm_decompiler.cpp`); see ../debug/script_debugger.md |
| Script kinds: story scripts, help scripts, challenge help, temple help and specials, multiplayer help | partial | `Game.cpp` runs every kind each world turn (`LookIn(ScriptType::All)`) and only the temple help and temple special kinds inside the temple; when the original runs each kind is not fully checked; see ../engine/script_vm.md |
| Camera commands for cut scenes | done | every camera native is real in `src/CHLApi.cpp` (set, move, follow, face, dual, paths, lens, shake, zones, widescreen); checked in the Land 1 intro; see [script-camera.md](../../bw1-notes/script-camera.md#opcodes) |
| Dialogue, text and the advisors | done | `RunText`, `TempText`, `TextRead`, `StartDialogue`, `EndDialogue` and every advisor native are real (`src/Help`); checked in the Land 1 intro; see [advisors.md](advisors.md) |
| Scrolls, challenge records and rewards | partial | scrolls are real (`CreateHighlight`, `HighlightProperties`, `src/ECS/ScriptHighlight.cpp`); the challenge records (`Snapshot`, `UpdateSnapshot`) and rewards (`CreateReward`, `CreateRewardInTown`) are stubs |
| Music and sound | done | music and sound natives are real (`StartMusic`, `StopMusic`, `AttachMusic`, `PlaySoundEffect`, sound tags, the alignment music); see [audio.md](../../bw1-notes/audio.md) |
| Towns, belief, influence, players and computer gods | partial | influence natives, `GetNearestTownOfPlayer`, `SetTownDesireBoost` and `BuildBuilding` are real; `GetTownWithId`, the belief natives (`BeliefForPlayer`, `SetPlayerBelief`) and every computer player native are stubs |
| The creature and the leash | partial | the leash natives, `LoadMyCreature`, `CallPlayerCreature`, `SetCreatureHome`, `SetCreatureDevStage` and two `DevFunction` values are real; every creature teaching, desire and action native is a stub |
| Miracles, fire and weather | partial | the spell, fire and weather natives are real (`src/Magic/Script/CHL*.cpp`); mist, `SetMagicRadius`, `IsAffectedBySpell` and poison are stubs |
| Time and timers | done | game time, timers and the countdown are real (`SetGameTime`, `MoveGameTime`, `CreateTimer`, `SetTimerTime`, `StartCountdownTimer`, `src/ECS/ScriptTimer.cpp`) |
| Overall: about 62 of the 464 native functions do something, so no land's story runs past its first steps | partial | 241 of the 464 natives are real and 13 more work in part (`src/CHLApi.cpp`); Land 1 runs the intro, then stops at the creature gate (the plinth reading `ObjectInfoBits` is a stub), and no other land loads (`LoadMap` is empty) |
