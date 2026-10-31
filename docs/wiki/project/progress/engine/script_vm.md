# Script virtual machine

The game's story, challenges and tutorial are written in Black & White's own script language, compiled into one program
(`challenge.chl`) that a small virtual machine runs alongside the world, a step each game turn, calling into the game
through several hundred native functions. Lands themselves are set up by simpler line-by-line land scripts. What the
scripts do, land by land, belongs to [../story/](../story/).

**Progress: 17/35 done, 0 chosen, 16 partial, 0 inferred — 83%**

## The compiled program

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The game loads its compiled script program (code, constant data, global variables, scripts and their parameters) at start | done | `components/ScriptLibrary` (`LHVMFile`), `challenge.chl` loaded in `src/Game.cpp`; tests `LhvmProgram.WrittenProgramsReadBack`, `ChlRoundTrip.ChallengeChlWithoutHeaders` |
| The scripts marked to start on their own are started when the program loads | done | `LHVM::LoadBinary` starts the auto-start list; tests `LhvmAutostart.TheScriptsTheAutoStartListNamesRunOnceTheProgramIsLoaded`, `LhvmAutostart.ANumberWithNoScriptIsAnError` (`test/lhvm/test_lhvm_autostart.cpp`) |
| The land control script is started for the game | done | `src/Game.cpp` starts `LandControlAll` by name; test `IslandLoad.LandOneStartsTheWholeStorysControlScriptAsANewGame` |
| The challenge script sources compile to exactly the program the game ships | done | `components/lhvmcompiler`; tests `ChlRoundTrip.ChallengeChlWithoutHeaders`, `ChlRoundTrip.ChallengeChlWithHeaders` |

## Running scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every running script gets a step each game turn, after the living, the miracles, the particles and the physics, and before the help, the time of day, the weather things and the climates | done | `LHVM::LookIn` from `Game::GameLogicLoop` (`Step::Scripts`); test `GameTurnOrder.OpenblacksOrder` |
| A script runs until it waits, sleeps or yields, and carries on from there next turn | partial 80% | `LHVM::CpuLoop`; runs the story's scripts, not audited instruction by instruction |
| Arithmetic, comparisons, logic and jumps work on integers, floats, positions and objects | partial 80% | opcode handlers in `components/ScriptLibrary/src/LHVM.cpp`; not audited against the game |
| Converting between number types truncates as the game does | done | tests `LhvmCast.ToIntTruncatesTheFloat`, `LhvmCast.ToFloatReadsTheBitsAsAnUnsignedInteger` |
| Taking a value off the stack into a variable releases the object the variable held before | done | `LHVM::RemoveReference` on the old value (`components/ScriptLibrary/src/LHVM.cpp`); test `LhvmAutostart.PoppingIntoAVariableReleasesTheObjectItHeld` (`test/lhvm/test_lhvm_autostart.cpp`) |
| A script's elapsed time is counted in tenths of a second as the game counts it | done | the tick count times 0.1f (`components/ScriptLibrary/src/LHVM.cpp`); test `LhvmTicks.TheElapsedTimeIsTheTurnsCountedTimesATenthAsAFloat` (`test/lhvm/test_lhvm_ticks.cpp`) |
| Native calls the game doesn't implement drop their arguments so later values don't shift | done | test `LhvmNativeStack.ANativeBoundWithAnArgumentItDoesNotPopHasItDropped` |
| A script can start another and either wait for it to finish or carry on alongside it | partial 80% | `Opcode24Call` (sync and async); not audited |
| Exception handlers (a block's "when"/"until" conditions) are checked each turn before the scripts' normal code, and can break out of their block | partial 80% | `LHVM::LookIn` runs the exception handlers first; not audited |
| Global variables are shared by all scripts; each script has its own locals and parameters | partial 90% | `LHVM`; not audited |
| Objects a script holds are reference counted so the game doesn't delete them under it | partial 70% | `AddReference` and `RemoveReference` in the VM; the game objects' side not checked |
| A script can stop itself, all scripts, scripts of a type, or scripts by name | partial 80% | `StopTask`, `StopAllTasks`, `StopTasksOfType`, `StopScripts` exist and are reached from `src/CHLApi.cpp`; few callers |
| Only some kinds of script run in some situations (for example help scripts versus challenge scripts) | partial 60% | the temple's turn runs only the temple help and special scripts (`src/Game.cpp`); the world's turn runs every kind |
| Scripts can ask for their own task number and the kind of script they are | partial 60% | the task number is given to the help script control (`src/CHLApi.cpp`); the script type accessors are not all used |
| The whole script program can be rebooted (every script stopped and the auto-start ones started again), as the game does when asked to | pending | `LHVM::Reboot` runs when the program loads; nothing else requests it |
| A script error is reported with its script and line, and the game carries on | partial 60% | the VM's error callback logs in `src/Game.cpp`; the game's own handling unconfirmed |

## Native functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The native function table matches the game's, by name, order, arguments and results | done | `src/CHLApi.cpp`; test `ChlLanguage.NativeTableMatchesTheGameBindings` |
| Each native function does what the game's does | partial 75% | of the 465 bound natives, 60 only log "not implemented" and 11 more log it for some of their cases (counted from the `NotImplemented` calls in `src/CHLApi.cpp`, 2026-10-22); the rest do their work, not all audited against the exe; which ones each challenge needs: see ../story/ |
| Positions, objects, floats and strings are taken off and put on the stack as the game passes them | partial 80% | stack helpers in `src/CHLApi.cpp`; test `test/audio/test_chlapi_stack.cpp` for a few natives; not audited as a whole |

## Land scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land scripts are read line by line, each line a command with numbers, strings and quoted positions | done | `src/LHScriptX/Lexer.cpp`, `Script.cpp`; tests `LoadScene.load_celtic_abode_f`, `ScriptWriter.TheReaderTakesEveryWrittenCommand` |
| An unreadable line doesn't end the game: the land is kept as far as it got | partial 50% | `Game::LoadMap` stops at the first bad line (unconfirmed whether the game skips just the line) |
| Version, land number, landscape, starting camera position and influence multipliers | partial 90% | version, land number, landscape, starting camera position and the town and player influence multipliers done (`src/LHScriptX/FeatureScriptCommands.cpp`); one town's own multiplier (SET_A_TOWNS_INFLUENCE_MULTIPLIER, in the playgrounds' scripts only) has no handler: the command is not in `FeatureScriptCommands`' table |
| Towns, town centres, abodes, citadels and town belief | done | towns, centres, abodes and planned abodes, citadels, belief and its cap, uninhabitable towns, congregation points (`FeatureScriptCommands.cpp`); tests `LandScriptTownCommandsTest.*` (`test/test_land_script_town_commands.cpp`) |
| Villagers | done | villagers by position, town villagers and special town villagers (`FeatureScriptCommands.cpp`); tests `ScriptVillagerCommands.InfoFromText`, `ScriptVillagerCommandsAbodes.FindAbodeAt` |
| Trees, forests, big forests, flowers, features, fields, pots, mobile objects and statics, street lights and lanterns, bonfires, mists | done | trees, dead trees, forests, big forests, flowers, features, fields, fish farms, pots, mobile objects and statics, street lights and lanterns, bonfires, mists done; scaffolds are made (`ecs::scaffolds::Create`); tests `LandScriptMeshCommandsTest.*` (`test/test_land_script_mesh_commands.cpp`), `LandScriptObjectCommandsTest.*` (`test/test_land_script_town_commands.cpp`), `LandScriptContents.realLand1Script` .. `realLandTScript`. Walls, pitches, temporary pots and furniture are still empty, but no land script the game ships (the story lands, the playgrounds or the others in `Scripts`) uses those commands, so they are not progress ([Not in the original](../../not-in-the-original.md)); re-measured 2026-10-28 (was partial 70%) |
| Streams and footpaths | done | streams, stream points, footpaths, nodes and linking a footpath to the last created object done (`FeatureScriptCommands::LinkFootpath`); a waterfall is an empty object, as in the original (no land makes one); tests `FootpathsTest.LinkFootpathGivesTheFootpathToTheLastCreatedFixedObject`, `FootpathsTest.LinkFootpathAfterAThingThatHoldsNoLinkOrNothingDoesNothing`, `LandScriptContents.realTheShippedScriptsUseTheLandscapeMistStreamAndRainCommandsAsCounted`. Paths (`CreatePath`) log "not implemented", but no land script the game ships uses CREATE_PATH, so it is not progress; re-measured 2026-10-28 (was partial 60%) |
| Weather climates | done | climates with rain, temperature and wind, and storms (`FeatureScriptCommands.cpp`); tests `WeatherTest.ClimatesAddTemperatureAndWind`, `WeatherTest.TheWeatherSystemSetsUpClimatesAsTheLandScriptDid` |
| Animals, flocks, creature pens, worship sites, spell icons and dispensers, one-shot spells, fireflies, influence rings, arenas | partial 85% | animals, flocks, worship sites, spell icons and dispensers, one-shot spells, influence rings and arenas done (tests `LandScriptObjectCommandsTest.*`, `LandScriptTownSpellCommandsTest.*`, `LandScriptMagicObjectsTest.*`); a planned worship site makes nothing, as in the original (`FeatureScriptCommands::CreatePlannedWorshipSite`); creature pens are empty, but no land script the game ships uses CREATE_CREATURE_PEN, so they are not progress; what is missing: fireflies (`CreateFireFly` is empty, and CREATE_FIRE_FLY has 152 lines in the shipped scripts, all outside the story lands), see [../nature/fireflies.md](../nature/fireflies.md); re-measured 2026-10-28 (was partial 70%) |
| Creatures from mind files, and computer players switched on | done | creatures from files and the computer player switch; CREATE_CREATURE makes a creature for a player who has none (`ecs::player_creature::MakeForLandScript`); the computer players' personalities and creature likes reach their minds (`FeatureScriptCommands::LoadComputerPlayerPersonality`, `SetComputerPlayerPersonality`, `SetComputerPlayerCreatureLike`), 2026-10-22; tests `ScriptLoadCreatureTest.TheLandScriptsCreateCreatureMakesANewCreatureOfTheTypeForThePlayer`, `PlayerSystem.aComputerPlayerTheLandScriptMakesIsListed`, `PlayerSystem.theLandScriptsPersonalityKeywordsReachTheComputerPlayers` |
| Land balance, night time and the game's opening messages | partial 80% | global land balance, night time, the lost-town scale and the last created object made an artefact (`FeatureScriptCommands::MakeLastObjectArtifact`; test `test/test_artefact_rules.cpp`) done; per-player balance (`SetLandBalance`) is empty, but no land script the game ships uses SET_LAND_BALANCE, so it is not progress; what is missing: the opening messages (`StartGameMessage`, `AddGameMessageLine` are empty; the playgrounds' scripts use them, 16 and 65 lines); re-measured 2026-10-28 (was partial 65%) |
| Map scripts: the commands that set up a game rather than a land (players, date, turn length, which land and scripts to load, language) | pending | `src/LHScriptX/MapScriptCommands.cpp` throws for each and is never run |
