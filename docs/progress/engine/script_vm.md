# Script virtual machine

The game's story, challenges and tutorial are written in Black & White's own script language, compiled into one program
(`challenge.chl`) that a small virtual machine runs alongside the world, a step each game turn, calling into the game
through several hundred native functions. Lands themselves are set up by simpler line-by-line land scripts. What the
scripts do, land by land, belongs to [../story/](../story/).

**Progress: 15/36 done, 0 chosen, 19 partial, 0 inferred — 78%**

## The compiled program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game loads its compiled script program (code, constant data, global variables, scripts and their parameters) at start | done | `components/ScriptLibrary` (`LHVMFile`), `challenge.chl` loaded in `src/Game.cpp` |
| The scripts marked to start on their own are started when the program loads | done | `LHVM::LoadBinary` starts the auto-start list |
| The land control script is started for the game | done | `src/Game.cpp` starts `LandControlAll` by name |
| The challenge script sources compile to exactly the program the game ships | done | `components/lhvmcompiler`; tests `ChlRoundTrip.ChallengeChlWithoutHeaders`, `ChlRoundTrip.ChallengeChlWithHeaders` |

## Running scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every running script gets a step each game turn, after the living and before the weather and the miracles | done | `LHVM::LookIn` from `Game::GameLogicLoop` |
| A script runs until it waits, sleeps or yields, and carries on from there next turn | partial 80% | `LHVM::CpuLoop`; runs the story's scripts, not audited instruction by instruction; re-measured 2026-10-21: runs the story's scripts; not audited instruction by instruction against the exe |
| Arithmetic, comparisons, logic and jumps work on integers, floats, positions and objects | partial 80% | opcode handlers in `components/ScriptLibrary/src/LHVM.cpp`; not audited against the game; re-measured 2026-10-21: runs the story's scripts; not audited instruction by instruction against the exe |
| Converting between number types truncates as the game does | done | tests `LhvmCast.ToIntTruncatesTheFloat`, `LhvmCast.ToFloatReadsTheBitsAsAnUnsignedInteger` |
| Taking a value off the stack into a variable releases the object the variable held before | done | `LHVM::RemoveReference` on the old value (`components/ScriptLibrary/src/LHVM.cpp`) |
| A script's elapsed time is counted in tenths of a second as the game counts it | done | the tick count times 0.1f (`components/ScriptLibrary/src/LHVM.cpp`) |
| Native calls the game doesn't implement drop their arguments so later values don't shift | done | test `LhvmNativeStack.ANativeBoundWithAnArgumentItDoesNotPopHasItDropped` |
| A script can start another and either wait for it to finish or carry on alongside it | partial 80% | `Opcode24Call` (sync and async); not audited; re-measured 2026-10-21: runs the story's scripts; not audited instruction by instruction against the exe |
| Exception handlers (a block's "when"/"until" conditions) are checked each turn before the scripts' normal code, and can break out of their block | partial 80% | `LHVM::LookIn` runs the exception handlers first; not audited; re-measured 2026-10-21: runs the story's scripts; not audited instruction by instruction against the exe |
| Global variables are shared by all scripts; each script has its own locals and parameters | partial 90% | `LHVM`; not audited; re-measured 2026-10-21: as the original's model; not audited |
| Objects a script holds are reference counted so the game doesn't delete them under it | partial 70% | `AddReference` and `RemoveReference` in the VM; the game objects' side not checked; re-measured 2026-10-21: the VM counts; the game objects' side is not checked |
| A script can stop itself, all scripts, scripts of a type, or scripts by name | partial 80% | `StopTask`, `StopAllTasks`, `StopTasksOfType`, `StopScripts` exist and are reached from `src/CHLApi.cpp`; few callers; re-measured 2026-10-21: all four exist and are reached; few callers |
| Only some kinds of script run in some situations (for example help scripts versus challenge scripts) | partial 60% | the temple's turn runs only the temple help and special scripts (`src/Game.cpp`); the world's turn runs every kind; re-measured 2026-10-21: the temple's turn runs only its kinds; the world's runs every kind |
| Scripts can ask for their own task number and the kind of script they are | partial 60% | the task number is given to the help script control (`src/CHLApi.cpp`); the script type accessors are not all used; re-measured 2026-10-21: the task number is given; the type accessors are not all used |
| The whole script program can be rebooted (every script stopped and the auto-start ones started again), as the game does when asked to | pending | `LHVM::Reboot` runs when the program loads; nothing else requests it |
| Scripts are stopped and their state kept when the game is saved, and restored on load | partial 40% | the VM writes and reads its state (`SaveState`, `RestoreState`); no save games (see saving_and_loading.md); re-measured 2026-10-21: the VM's state is written and read; no save games |
| A script error is reported with its script and line, and the game carries on | partial 60% | the VM's error callback logs in `src/Game.cpp`; the game's own handling unconfirmed; re-measured 2026-10-21: logged; the original's own handling is not confirmed |

## Native functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The native function table matches the game's, by name, order, arguments and results | done | `src/CHLApi.cpp`; test `ChlLanguage.NativeTableMatchesTheGameBindings` |
| Each native function does what the game's does | partial 75% | of the 465 bound natives, 60 only log "not implemented" and 11 more log it for some of their cases (counted from the `NotImplemented` calls in `src/CHLApi.cpp`, 2026-10-22); the rest do their work, not all audited against the exe; which ones each challenge needs: see ../story/; re-measured 2026-10-21: about 97 of the 465 bound natives still log 'not implemented' (2026-10-21); the rest are not all audited against the exe |
| Positions, objects, floats and strings are taken off and put on the stack as the game passes them | partial 80% | stack helpers in `src/CHLApi.cpp`; test `test/audio/test_chlapi_stack.cpp` for a few natives; not audited as a whole; re-measured 2026-10-21: stack helpers; a few natives tested |

## Land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts are read line by line, each line a command with numbers, strings and quoted positions | done | `src/LHScriptX/Lexer.cpp`, `Script.cpp` |
| An unreadable line doesn't end the game: the land is kept as far as it got | partial 50% | `Game::LoadMap` stops at the first bad line (unconfirmed whether the game skips just the line); re-measured 2026-10-21: the land stops at the first bad line; whether the original skips only that line is not confirmed |
| Version, land number, landscape, starting camera position and influence multipliers | partial 90% | version, land number, landscape, starting camera position and the town and player influence multipliers done (`src/LHScriptX/FeatureScriptCommands.cpp`); one town's own multiplier logs "not implemented"; re-measured 2026-10-21: one town's own multiplier is missing |
| Towns, town centres, abodes, citadels and town belief | done | towns, centres, abodes and planned abodes, citadels, belief and its cap, uninhabitable towns, congregation points (`FeatureScriptCommands.cpp`) |
| Villagers | done | villagers by position, town villagers and special town villagers (`FeatureScriptCommands.cpp`) |
| Trees, forests, big forests, flowers, features, fields, pots, mobile objects and statics, street lights and lanterns, bonfires, mists | partial 70% | trees, dead trees, forests, big forests, flowers, features, fields, fish farms, pots, mobile objects and statics, street lights and lanterns, bonfires, mists done; scaffolds are made (`ecs::scaffolds::Create`); walls, pitches, temporary pots and furniture are empty; re-measured 2026-10-21: most kinds done; walls, pitches, temporary pots and the rest of the note are not |
| Streams and footpaths | partial 60% | streams, stream points, footpaths, nodes and linking a footpath to the last created object done (`FeatureScriptCommands::LinkFootpath`); a waterfall is an empty object, as in the original (no land makes one); paths (`CreatePath`) log "not implemented"; re-measured 2026-10-21: paths and linked footpaths and waterfalls are missing |
| Weather climates | done | climates with rain, temperature and wind, and storms (`FeatureScriptCommands.cpp`) |
| Animals, flocks, creature pens, worship sites, spell icons and dispensers, one-shot spells, fireflies, influence rings, arenas | partial 70% | animals, flocks, worship sites, spell icons and dispensers, one-shot spells, influence rings and arenas done; creature pens, planned worship sites and fireflies are empty; fireflies: [../nature/fireflies.md](../nature/fireflies.md); re-measured 2026-10-21: creature pens, planned worship sites and fireflies are empty |
| Creatures from mind files, and computer players switched on | done | creatures from files and the computer player switch; CREATE_CREATURE makes a creature for a player who has none (`ecs::player_creature::MakeForLandScript`); the computer players' personalities and creature likes reach their minds (`FeatureScriptCommands::LoadComputerPlayerPersonality`, `SetComputerPlayerPersonality`, `SetComputerPlayerCreatureLike`), 2026-10-22; re-measured 2026-10-21: CREATE_CREATURE, personalities and creature likes are missing |
| Land balance, night time and the game's opening messages | partial 50% | global land balance, night time and the lost-town scale done; per-player balance, opening messages and artefacts are empty; re-measured 2026-10-21: per-player balance, opening messages and artefacts are empty |
| Map scripts: the commands that set up a game rather than a land (players, date, turn length, which land and scripts to load, language) | pending | `src/LHScriptX/MapScriptCommands.cpp` throws for each and is never run |
