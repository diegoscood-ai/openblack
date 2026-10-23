# The real clock and calendar

Everything in the game that reads the date and time of the player's computer, rather than the game's own clock: voices
that whisper the player's name late at night, the real moon's phase, script functions that read the real date and time,
the smiley footprints of the first of April, the time played, the dates on saved games, and the times the online
features send and compare. The game's own day and night, and its in-game calendar, are owned by
[../sky/day_night_cycle.md](../sky/day_night_cycle.md) and [../engine/game_loop_and_clock.md](../engine/game_loop_and_clock.md).
All of the game's reads of the clock were traced; the dates and hours below are the only ones it acts on.

**Progress: 32/46 done, 0 chosen, 4 partial, 1 inferred — 74%**

## Night whispers: when

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On real nights a ghostly voice now and then whispers the player's own name | done | `audio::spooky` (`src/Audio/Services/SpookyVoices.cpp`), run once a game turn from the guidance (`src/Audio/Services/Guidance.cpp`); tests in `test/audio/test_spooky_voices.cpp`; the advisors' summary rows are in ../story/advisors.md ("Spooky voices") |
| A real night is from 20:45 to 20:59, and from 23:00 to 05:59, by the computer's local clock | done | `spooky::NightNow`; test `SpookySoundex.NightByTheClock` |
| The clock is read every 100 game turns, about every 10 seconds of play | done | a countdown of 100 calls of `spooky::Process`, which runs only with the game's turns; test `SpookyTest.CountdownAndCounter` |
| Never on the first two lands | done | `spooky::Process` does nothing on lands 1 and 2 |
| Never when no name could be matched (see below) | done | no sample without a matching name (`spooky::GetName` gives 0), so it never sounds |
| There is no option to turn it off | done | our tree has no option either |
| Nothing stops it in a multiplayer game | done | nothing in `spooky::Process` checks for a multiplayer game, as in the game |

## Night whispers: how likely

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each real-night check counts up by one; the count starts at 0 and goes back to 0 after each whisper | done | the counter in `spooky::Process`, back to 0 in `PlaySpookyVoice`; test `SpookyTest.CountdownAndCounter` |
| A check whispers when (1 − r³) × 1000, cut to a whole number, is below the count, with r a local random number from 0 to 1 | done | `spooky::Process`, with the local random stream (`game_random::LocalFloatRand`) |
| So it is rare at first and grows: about 0.3% a check after 10 checks (under 2 minutes), 2% after 60 (10 minutes), 6.4% after 180 (30 minutes), 14% after 360 (an hour), and certain after 1000 | done | follows from the rule above |
| A check that does not whisper may instead have the evil advisor say, once per run of the game, "Night's falling. Beware. The foul are abroad." with a chance of 0.02% | done | every night check that does not whisper calls `guidance::OneOff(1)`, which says the line once per run at its chance (`src/Audio/Services/Guidance.cpp`); see ../story/advisors.md |

## Night whispers: whose name

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The name is looked up as the game starts up, and again as each game starts | done | `spooky::Init` at the game's one-time start and `spooky::UpdatePlayerName` at each land (`Game.cpp`) |
| The game first tries the player's profile name | partial 40% | our tree has no profiles: the name comes from `OPENBLACK_PLAYER_NAME` (`Game.cpp`), a stand-in (see ../interface/profiles.md); re-measured 2026-10-21: the name is matched, but from a stand-in, not the player's profile |
| If that does not match, it tries a second name the game holds for the player (unconfirmed which: most likely the online player's name) | pending | the network name is not ported (no network login in our tree) |
| Each word of the name is tried in turn | done | `spooky::SoundexOverlap` (words split at spaces) |
| A word matches a recorded name when their first letters are the same exactly (case counts) and their next three sound codes match | done | `spooky::SoundsAlike` and `GetNextSoundexCode`; tests `SpookySoundex.LetterCodes`, `NextCodeKeepsTheW120Quirk`, `ComparisonAndOverlap`. Our wiki differs: h, w and y give the letter itself as their code rather than none, and a letter with the same code as the one before is not skipped but gives that code plus one ([audio](../../../audio/voices.md#guidance-and-night-voices-in-openblack)) |
| There are 100 recorded names: James, John, Peter, Pete, Matthew, Mark, Simon, Paul, Timothy, Tim, Jon, Andrew, Andy, Jamie, Will, William, Oliver, Ollie, Richard, Rich, Dan, Ben, Neil, Rob, Robert, Russell, Jeremy, Jez, Gary, Mike, Chris, Christopher, Dave, David, Aaron, Wayne, Ian, Phil, Julian, Ed, Tom, Charlie, Jack, Steven, Steve, Liam, Jason, Nick, Nicholas, Thomas, Darren, Kevin, Lee, Terry, Adrian, Dominic, Alex, Matt, Nathan, Adam, Alan, Graham, Angus, Abdul, Achmed, Mohammed, Habib, Gareth, Jo, Joe, Janice, Claire, Sophie, Vicky, Mel, Rachel, Emma, Gemma, Alison, Caroline, Jade, Kylie, Lucy, Sam, Debbie, Jasmine, Lisa, Louise, Emily, Amanda, Lizzie, Daisy, Katie, Sarah, Cat, Amy, Naomi, Helen, Karen, Rebecca | done | the 100 `HELP_TEXT_SPOOKY_NAMES_*` texts (`spooky::k_FirstName`), the first match winning (`spooky::FindSoundAlikeName`); tests `SpookySoundex.TheInstallationsNames` and its synthetic twin |
| The whisper is that name's recording, from the advisors' sound bank | done | the name's sample from the voice table, played from the Guidance bank (`spooky::Init`, `PlaySpookyVoice`) |
| The game's info table also lists five named voices: Tblamb, Revans, Aevans, Bill and Pmolyneux | done | read into `InfoConstants::spookyVoice` (`src/InfoConstants.h`) and, as in the game, used by nothing ([audio](../../../audio/voices.md#guidance-and-night-voices-in-openblack)) |

## Night whispers: how it sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The volume is the normal one times or divided by 1 + a³, a a local random number up to 0.65, so 78% to 127% | done | `spooky::PlaySpookyVoice`; test `SpookyTest.PlaySpookyVoiceBuildsTheVolumeUp`. Our wiki differs: the volume is scaled by the random number up to 0.8 and the pitch by the one up to 0.65, and the volume change builds up from one whisper to the next because the sound options are kept ([audio](../../../audio/voices.md#guidance-and-night-voices-in-openblack)) |
| The pitch is the sample's own times or divided by 1 + b³, b up to 0.8, so 0.66 to 1.51 times | done | `spooky::PlaySpookyVoice`. Our wiki differs: the pitch is the one scaled by the random number up to 0.65, the volume the one up to 0.8 ([audio](../../../audio/voices.md#guidance-and-night-voices-in-openblack)) |
| A third random number from 0 to 179 is set on the sound (unconfirmed: most likely where it is heard from) | partial 50% | drawn and kept (`spooky::State::field2C`), but our sample player does not use it; re-measured 2026-10-21: the number is drawn as the original (so the random stream matches); its use on the sound is not ported, and where it is heard from is inferred |
| It is played as a plain sound, not placed in the world (unconfirmed) | inferred | played 2D (`spooky::PlaySpookyVoice`); re-measured 2026-10-21: that the original plays it 2D is not confirmed in the exe |

## The real moon

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The moon in the sky shows the real moon's phase, from the computer's clock | done | see ../sky/moon.md: `moon::Phase` (`src/Graphics/Moon.cpp`); test `Moon.PhaseFollowsTheRealMoon` |
| The clock is read at most once every 2 seconds for it | done | our tree reads it every frame, which gives the same phase (../sky/moon.md) |
| Scripts can ask how full the moon is | done | `GET_MOON_PERCENTAGE` (`GetMoonPercentage` in `src/CHLApi.cpp`) gives `moon::ScriptPercentage` of the phase the moon last showed with: 0 at the full moon, 1 at a new moon; see ../sky/moon.md |
| The advisors remark on the moon's phase | done | the guidance's moon remark (`RemarkOnMoonPhase`, `src/Audio/Services/Guidance.cpp`) uses the real phase (`moon::Phase` of the computer's clock) at a real night; see ../interface/help_system.md |

## Script functions

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| "get real time" gives the local time as an hour with a fraction: hours + minutes / 60 + seconds / 3600 | done | `GET_REAL_TIME` (`GetRealTime` in `src/CHLApi.cpp`) through `script::real_time::Time` (`src/ScriptHeaders/ScriptRealTime.cpp`), each step rounded to a float; test `ScriptRealTime.AnswersAsTheOriginalForTheSameLocalTime` |
| "get real day" gives the day of the month, 1 to 31 | done | the first `GET_REAL_DAY` (`GetRealDay115`) through `script::real_time::MonthDay`; test `test/test_script_real_time.cpp` |
| "get real weekday" gives the day of the week, 1 to 7 with Sunday as 1 | done | the second `GET_REAL_DAY` of the pair (`GetRealDay116`) through `script::real_time::WeekDay`, 1 on Sunday; test `test/test_script_real_time.cpp` |
| "get real month" gives the month, 1 to 12 | done | `GET_REAL_MONTH` (`GetRealMonth`) through `script::real_time::Month`; test `test/test_script_real_time.cpp` |
| "get real year" gives the full year, e.g. 2026 | done | `GET_REAL_YEAR` (`GetRealYear`) through `script::real_time::Year`; test `test/test_script_real_time.cpp` |
| In a multiplayer game each of them reports "This is not multiplayer friendly yet!" as a script error, but still gives the value | pending | no multiplayer game in our tree |

## Special days

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the first of April every creature leaves a smiley face instead of its own footprint | done | `FootprintSystem::IsAprilFools` (`src/ECS/Systems/Implementations/FootprintSystem.cpp`), `creature_footprints::IsAprilFools`; tests `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize`, `FootprintSystemDate.AprilFoolsComesFromTheInjectedDate`; see ../terrain/land_marks.md |
| The day is the computer's local date, from midnight to midnight, read again for every print, so prints change mid-walk at midnight | partial 70% | our tree reads the local date once, when the footprints are reset at each land (`FootprintSystem::Reset`), not for every print, so the prints do not change at midnight; re-measured 2026-10-21: the date picks the prints as the original; it is read once per land, not for every print |
| The smiley keeps the size the species' own print would have | done | test `CreatureFootprints.AprilFoolsSmileyKeepsTheSpeciesSize` |
| The smiley is the eighth picture in the row of footprint pictures | done | `creature_footprints::k_SmileyCell` (`src/Creature/CreatureFootprints.h`) |
| It applies to every creature on the land, the player's, the enemies' and in multiplayer games, with no option to turn it off | done | as the game; our tree adds a debug override in the creature spawner (`src/Debug/CreatureSpawnerFootprints.cpp`); re-measured 2026-10-21: as the original; the debug override is a debug control |

## Time played and saved games

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The real time is noted when a game starts | partial 30% | our tree counts from when its engine started, on the game's own clock (`game_clock::EngineMs`, `src/3D/Implementations/TempleInterior.cpp`), not the real one; re-measured 2026-10-21: a time is noted, but the engine's clock, not the real one |
| When the game is closed, the time since then is added to the total time played, which is kept in the player profile | pending | no total in our tree (no profiles) |
| The save game room's scroll shows the total time played, including the current session, as hours, minutes and seconds | pending | `src/3D/TempleScrolls.cpp` shows only this session's time |
| Each saved game is labelled with the time and date it was saved, in the user's own Windows format: the time, a space, then the long date (the short date if the long one is not available) | pending | `TempleScrolls` shows a date field, but our saved games are made-up placeholders |
| If the time and date together are 62 characters or more, the label is left blank | pending | no saved games in our tree |
| The real time is noted as each land's statistics start (unconfirmed what for) | pending | it is saved with the statistics and never shown; perhaps the online record's time played (unconfirmed) ([../interface/statistics_counted.md](../interface/statistics_counted.md)) |
| The music notes the real time each mood was last chosen (unconfirmed what for) | pending | see ../audio/music.md (unconfirmed) |

## Other uses of the clock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature upload file is given a random number from a generator seeded with the clock | pending | no `creature.lhp` in our tree; see online_services.md |
