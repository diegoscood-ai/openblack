# Challenge natives: game flow, time and scripts

The challenge scripts' functions for running the story: timers, the time of day, random numbers, stopping scripts, loading the next land, game speed, the temple, the real clock, keys and profile choices. The game has 464 of these functions in all; the language statement each comes from is shown in italics, and "called" counts are calls in the shipped `challenge.chl`. How the virtual machine runs them is in [../engine/script_vm.md](../engine/script_vm.md); what each challenge is about is in [../story/](../story/).

**Progress: 33/62 done, 10 partial — 61%**

## Used by the shipped scripts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gives the distance between two positions: *get distance from ‹p0› to ‹p1›* (called 712 times in 200 scripts) | done | `GetDistance` in `src/CHLApi.cpp`: distance over x and z only through `gutils::GetDistance`, nought under half a metre |
| Gives a random number between two numbers, from the game's own random numbers: *number from ‹min› to ‹max›* (called 162 times in 51 scripts) | done | `Random` in `src/CHLApi.cpp`: the game's synced random float (`game_random::GameFloatRand`) over max minus min plus one, truncated; `test/test_game_random.cpp` |
| Gives the time the game has been running: *time* (called 51 times in 20 scripts) | done | `DllGettime` calls `LHVM::PushElaspedTime`: the VM's tick count times 0.1, as the script library does |
| Gives a random whole number between two numbers: *constant from ‹min› to ‹max›* (called 40 times in 22 scripts) | done | `RandomUlong` in `src/CHLApi.cpp`: the game's synced `game_random::GameRand` over max minus min plus one, plus min |
| Changes how fast the game runs: *set game speed to ‹speed›* (called 6 times in 3 scripts) | done | `SetGamespeed`: `help::script_control::SetGameSpeed` (`src/Help/ScriptControl.cpp`) sets the clock's speed only for the task that holds the game speed |
| Sets the hour of the day: *set game time ‹time›* (called 31 times in 13 scripts) | done | `SetGameTime`: `Game::SetTime` forces the day-night clock |
| Gives the hour of the day: *get game time* (called 27 times in 8 scripts) | done | `GetGameTime`: `DayNightClock::GetScriptTime` |
| Puts the game at cinema speed for a cut scene (part of the cinema block): *begin cinema (game speed part)* (called 328 times in 181 scripts) | done | `StartGameSpeed`: `help::script_control::StartGameSpeed` (`src/Help/ScriptControl.cpp`): the task takes the game speed when nobody holds it. Our wiki differs: the original only takes control of the game speed here and does not change it; the speed changes through set game speed ([intro](../../bw1-notes/intro.md#flow)) |
| Puts the game back to its normal speed after a cut scene: *end cinema (game speed part)* (called 450 times in 238 scripts) | done | `EndGameSpeed`: `help::script_control::EndGameSpeed` gives the speed back to normal for the holding task (or nobody); a stopped task also gives it back (`OnTaskStopped`) |
| Gives a position at a distance and angle from one place towards another: *get target from ‹from› to ‹to› distance ‹distance› angle ‹angle›* (called 10 times in 7 scripts) | todo | `GetTargetRelativePos` in `src/CHLApi.cpp` logs "not implemented" and pushes a zero position |
| Sets a timer's time: *set ‹timer› time to ‹time› seconds* (called 161 times in 68 scripts) | done | `SetTimerTime`: `ecs::script_timer::SetTime` (`src/ECS/ScriptTimer.cpp`) restarts the timer from this turn; a spell dispenser takes it as its period |
| Creates a timer running for a number of seconds: *create timer for ‹timeout› seconds* (called 137 times in 84 scripts) | done | `CreateTimer`: `ecs::script_timer::Create` makes a timer thing held by the script (`ecs::script_held::AddScriptThing`) |
| Gives the time a timer has left: *get ‹timer› time remaining* (called 154 times in 79 scripts) | done | `GetTimerTimeRemaining`: `ecs::script_timer::Remaining`, nought once out or for a thing that is not a timer; `test/test_timers_events.cpp` |
| Gives the time since a timer was set: *get ‹timer› time since set* (called 6 times in 6 scripts) | done | `GetTimerTimeSinceSet`: `ecs::script_timer::SinceSet`, the largest float for a missing or wrong thing |
| Loads the next land (its land script) for the story: *load map ‹path›* (called 4 times in 1 script) | todo | `LoadMap` in `src/CHLApi.cpp` is empty: Land 1 never goes on to Land 2 (a port is designed but parked, see [map-loading](../../bw1-notes/map-loading.md#pending)) |
| Stops every script except those from the named source files: *stop all scripts excluding files ‹source filenames›* (called 6 times in 2 scripts) | partial | `StopAllScriptsInFilesExcluding` stops by source file through `LHVM::StopScripts`; the list is split on spaces only and compared with case, where the original also splits on commas and tabs and ignores case ([map-loading](../../bw1-notes/map-loading.md#pending)) |
| Stops a script by name: *stop script ‹script name›* (called 69 times in 31 scripts) | partial | `StopScript` stops by name through `LHVM::StopScripts`, comparing the whole text with each script's name, with case; the splitting of the original is not checked here |
| Stops every script from the named source files: *stop scripts in files ‹source filenames›* (called once in 1 script) | partial | `StopScriptsInFiles` stops by source file through `LHVM::StopScripts`; split on spaces only and compared with case (the original also splits on commas and tabs and ignores case) |
| Runs one of the developers' functions by number: *run ‹func› developer function* (called 23 times in 15 scripts) | partial | `DevFunction` in `src/CHLApi.cpp`: functions 2 and 3 (rope and other leashes known) through `ecs::player_creature::DevFunction` (`src/ECS/PlayerCreature.cpp`); the others log "not implemented"; the creature itself is still dormant in game |
| Whether the player's mouse has a wheel: *player has mouse wheel* (called once in 1 script) | todo | `HasMouseWheel` logs "not implemented" and pushes false |
| Gives how many times a kind of event has happened: *get ‹type› total event* (called 23 times in 7 scripts) | done | `GetTotalEvents`: `help_profile::TotalEvents` (`src/Help/HelpProfile.cpp`), the help accumulators fed by the camera and hand |
| Stops or starts the day going by: *enable/disable game time* (called 36 times in 13 scripts) | done | `GameTimeOnOff`: `DayNightClock::SetRunning` |
| Moves the time of day to an hour over a time: *move game time ‹hour of the day› time ‹duration›* (called 5 times in 5 scripts) | done | `MoveGameTime`: `DayNightClock::MoveScriptTime` |
| Stops the scripts of the named files except those named: *stop scripts in files ‹source filenames› excluding ‹script names›* (called once in 1 script) | partial | `StopScriptsInFilesExcluding` stops by file and name through `LHVM::StopScripts`; lists split on spaces only and compared with case |
| Whether the player is inside the temple: *inside temple* (called 3 times in 1 script) | done | `InsideTemple`: `game_clock::IsInsideCitadel` |
| Lets the player into the temple or not: *enable/disable temple* (called 2 times in 2 scripts) | done | `SetInterfaceCitadel`: `worship::citadel::SetInterfaceCitadel`, read by the temple entrance's tap check (`src/Worship/Citadel.cpp`) |
| Runs a line of land script: *run map script line ‹command›* (called 2 times in 2 scripts) | todo | `MapScriptFunction` in `src/CHLApi.cpp` logs "not implemented" |
| Whether a key is held down: *key ‹key› down* (called 13 times in 8 scripts) | todo | `KeyDown` logs "not implemented" and pushes false |
| Starts an immersion (force feedback) effect: *start immersion ‹effect›* (called once in 1 script) | todo | `StartImmersion` logs "not implemented" (no force feedback) |
| Gives the position of a player's temple: *player ‹player› temple position* (called 2 times in 2 scripts) | todo | `GetTemplePosition` logs "not implemented" and pushes a zero position |
| Gives a miracle's icon in the temple: *get spell icon ‹spell› in ‹temple›* (called once in 1 script) | done | `GetSpellIconInTemple`: `magic::script::GetSpellIconInTemple` (`src/Magic/Script/CHLWorship.cpp`) |
| Gives the position of a player's temple entrance: *player ‹player› temple entrance position radius ‹radius› height ‹height›* (called 2 times in 1 script) | todo | `GetTempleEntrancePosition` logs "not implemented" and pushes a zero position |
| Whether the player chose to skip the tutorial: *can skip tutorial* (called once in 1 script) | done | `CanSkipTutorial`: the map script globals' skip flags, set from the start-up requester's answer in `src/Game.cpp` |
| Whether the player chose to skip the creature's training: *can skip creature training* (called once in 1 script) | done | `CanSkipCreatureTraining`: the same skip flags |
| Whether the player is keeping their old creature: *is keeping old creature* (called once in 1 script) | done | `IsKeepingOldCreature`: the same skip flags |
| Whether the current profile has a creature: *current profile has creature* (called once in 1 script) | done | `CurrentProfileHasCreature`: `ecs::player_creature::ProfileHasCreature`, whether the profile's mind file is in `Scripts/CreatureMind` |

## Not used by the shipped scripts

The game has these but no shipped script calls them; mods and fan-made challenges can.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Starts the countdown clock shown on screen: *start the countdown clock (no statement used)* (not called by the shipped scripts) | partial | `StartCountdownTimer`: `ecs::script_countdown::Start` (`src/ECS/ScriptTimer.cpp`), counted each turn in `src/Game.cpp`; the clock is never drawn on screen |
| Removes the countdown clock: *remove the countdown clock (no statement used)* (not called by the shipped scripts) | done | `RemoveCountdownTimer`: `ecs::script_countdown::Remove` |
| Gives the time left on the countdown clock: *time left on the countdown clock (no statement used)* (not called by the shipped scripts) | done | `GetCountdownTimer`: `ecs::script_countdown::RemainingSeconds`, whole seconds |
| Whether the countdown clock exists: *countdown clock exists (no statement used)* (not called by the shipped scripts) | done | `CountdownTimerExists`: `ecs::script_countdown::Exists` |
| Hides the countdown clock: *hide the countdown clock (no statement used)* (not called by the shipped scripts) | partial | `HideCountdownTimer`: `ecs::script_countdown::SetShown(false)`; nothing draws the clock yet |
| Gives how full the moon is: *get moon percentage* (not called by the shipped scripts) | todo | `GetMoonPercentage` logs "not implemented" and pushes nought |
| Keeps an object from being deleted while a script holds it: *keeping an object (emitted by the compiler, not written)* (not called by the shipped scripts) | done | `AddReference`: `ecs::script_held::IncrementReference` (`src/ECS/ScriptHeld.cpp`): the thing takes a slot in the scripts' table at its first reference, is in a script, and is controlled if a script made it or already controlled it |
| Lets go of an object the script was keeping: *letting an object go (emitted by the compiler, not written)* (not called by the shipped scripts) | done | `RemoveReference`: `ecs::script_held::DecrementReference`, the count never goes below nought; unreferenced slots are released each turn (`ecs::script_held::Process`) |
| Gives the real time on the player's computer clock: *get real time* (not called by the shipped scripts) | todo | `GetRealTime` logs "not implemented" and pushes nought |
| Gives the real day of the week: *get real day* (not called by the shipped scripts) | todo | `GetRealDay115` logs "not implemented" and pushes nought |
| Gives the real day of the month: *get real day* (not called by the shipped scripts) | todo | `GetRealDay116` logs "not implemented" and pushes nought |
| Gives the real month: *get real month* (not called by the shipped scripts) | todo | `GetRealMonth` logs "not implemented" and pushes nought |
| Gives the real year: *get real year* (not called by the shipped scripts) | todo | `GetRealYear` logs "not implemented" and pushes nought |
| Shows the countdown clock again: *show the countdown clock (no statement used)* (not called by the shipped scripts) | partial | `RevealCountdownTimer`: `ecs::script_countdown::SetShown(true)`; nothing draws the clock yet |
| Stops every script except those named: *stop all scripts excluding ‹script names›* (not called by the shipped scripts) | partial | `StopAllScriptsExcluding` stops by name through `LHVM::StopScripts`; split on spaces only and compared with case |
| Gives the number of buttons on the player's mouse: *number of mouse buttons* (not called by the shipped scripts) | todo | `NumMouseButtons` logs "not implemented" and pushes nought |
| Gives how often a kind of event happens a second: *get ‹type› event per seconds* (not called by the shipped scripts) | done | `GetEventsPerSecond`: `help_profile::EventsPerSecond` (`src/Help/HelpProfile.cpp`) |
| Gives the time since a kind of event last happened: *get time since ‹type› event* (not called by the shipped scripts) | done | `GetTimeSince`: `help_profile::TimeSince` |
| Turns the intro building sequence on or off (unconfirmed): *enable/disable intro building* (not called by the shipped scripts) | todo | `SetIntroBuilding` logs "not implemented" |
| Takes the player into the temple or back out: *enter/exit temple* (not called by the shipped scripts) | partial | `EnterExitCitadel`: `TempleInterior::Activate` or `Deactivate`; the original's walk in and out is not ported |
| Gives the square root of a number: *square root ‹value›* (not called by the shipped scripts) | done | `SquareRoot`: square root, nought for nought or less |
| Sets the length of a day and how much of it is night and dawn and dusk: *set game time properties duration ‹duration› percentage night ‹percentage night› percentage dawn dusk ‹percentage change›* (not called by the shipped scripts) | done | `SetGameTimeProperties`: `DayNightClock::SetCycle` |
| Puts the day's length back to normal: *reset game time properties* (not called by the shipped scripts) | done | `ResetGameTimeProperties`: the clock's default cycle |
| Stops an immersion effect: *stop immersion ‹effect›* (not called by the shipped scripts) | todo | `StopImmersion` logs "not implemented" |
| Stops all immersion effects: *stop all immersion* (not called by the shipped scripts) | todo | `StopAllImmersion` logs "not implemented" |
| Saves the game in a slot: *save game in slot ‹slot›* (not called by the shipped scripts) | todo | `SaveGameInSlot` logs "not implemented" (no save rooms) |
