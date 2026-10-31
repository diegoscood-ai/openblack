# The Temple Is Finished (the first gold scroll)

The story step between the opening's walk to the village and Choose Your Creature: once the villagers finish the
temple, the advisors show the player its entrance, make them go in and come out again, then fly the camera to the
creatures' gates and show what a gold story scroll looks like. It has no title of its own in the game's text table and
no scroll the player can click: the gold scroll it shows is a stand-in, removed when the scene ends, at exactly the
place where [Choose Your Creature](choose_your_creature.md)'s real scroll appears next.

**Land:** 1 · **Giver:** the two advisors, at the new temple · **Script:** CitadelGuide · **Reward:** none (it
opens the way to the gold scrolls) · **Repeatable:** no

Sources: the land's control script and the scene's script (the original source text, which matches the shipped
`challenge.chl`), the game's text table and the executable. openblack is judged on this tree: the land's control script
reaches this scene, and none of the 41 commands it needs only logs "not implemented" in `src/CHLApi.cpp`
([../../scripts/land1_script.md](../../scripts/land1_script.md)). Checked in a run on 2026-10-22 (Land 1, SkipBox answer 0,
deterministic fixed-step runs): after the town's builders finish the temple, the scene plays through in the same launch
as the tutorial: texts 1998 and 2000 on the flight, the player goes in through the entrance and out with Escape, then
texts 5212, 5199 and 5232 on the way out, and Choose Your Creature starts. The steps are not compared with the original
one by one, and the reminders and the camera pull-back were not exercised. A quest run of the scene on its own
(2026-10-28) shows the 20-second reminder, the pull-back and the way in, and stops inside the temple, where the
reminder to leave never comes, as in the original ([The quest run](#the-quest-run),
[The reminder to leave never comes in the original either](#the-reminder-to-leave-never-comes-in-the-original-either),
[Pending](#pending)).

**Progress: 9/34 done, 0 chosen, 25 partial, 0 inferred — 67%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script runs the scene straight after the opening walk ("follow us", [../tutorial.md](../tutorial.md)), which ends only when the villagers have finished the temple | partial 60% | `LandControl1` runs `CitadelGuide` after `FollowUs`; in the run of 2026-10-22 the whole of `FollowUs` plays, its builders finish the temple on their own and `CheckCitadel` sets `CitadelBuiltL1` to 1 (about 4.5 min of play after the pick-up demo in one run, about 1.5 min in another), then `CitadelGuide` runs. See [../../scripts/land1_script.md](../../scripts/land1_script.md); finished by [land1_script.md](../../scripts/land1_script.md) "Loading the land starts the story's control script"; still open: a start-gate row closes only from a natural run's PASS, none cited |
| A new game that skips to choosing the creature never plays it: the temple is built at once at its spot and marked finished instead | done | `LandControl1`'s skip branch: `BuildBuilding`, `CallNear` and `SetProperty` (built percentage) are real, and the test runs (fourth SkipBox answer) start with the temple built. See [land-change-and-saving.md](../../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial); test `CitadelPlanTest.SetPropertyBuiltPercentageAndBuilt` |
| Before the scene, two did-you-know scrolls are placed: one by the village on zooming, rotating and double clicking, one by the temple door on the Space bar's safe camera position | done | the shared `DidYouKnow` script calls only real natives (`CreateHighlight` is real, `src/ECS/ScriptHighlight`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |
| The control script waits for the whole scene (including the visit inside the temple) before it places the fish-farm did-you-know, starts Throwing Stones and The Lost Flock, and runs Choose Your Creature | partial 60% | plain script order in `LandControl1`; in the run of 2026-10-22 Choose Your Creature starts only once the player is out of the temple and the scene is over. See [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [choose_your_creature.md](choose_your_creature.md); the fish-farm did-you-know and the two silver starts were not looked at in that run; finished by [land1_script.md](../../scripts/land1_script.md) "Loading the land starts the story's control script"; still open: a start-gate row closes only from a natural run's PASS, none cited |
| The scene is logged nowhere: no challenge record, no reminder, nothing in the scroll log | partial 60% | the same script runs, so nothing is logged either; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; undetermined: the row is the script's own lack of a record, which no system row holds; only a quest run's PASS closes it, none cited |
| The scene is a "temple" kind of script: while the player is inside the temple, only temple scripts may take the camera, so this scene can still run its own camera there | done | inside the temple only the temple scripts run (`Game::ProcessTempleTurn`) and only they get the camera (`src/Help/ScriptControl.cpp`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |

## The temple flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema (widescreen bars, the player's control taken) begins with the first epic theme | done | `SetWidescreen`, `StartCameraControl`, `StartMusic` are real (`src/CHLApi.cpp`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |
| The camera flies to the temple over 6 seconds, then sinks slowly down its face over 23 seconds | done | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` are real (`src/Camera/ScriptCamera`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |
| Both advisors step out; the good advisor: "At last. The people have finished our Temple." | done | `SpiritEject` and `RunText` are real (advisors and texts in `src/Help`); plays in the run of 2026-10-22 (the scene runs to its end); finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Brings the good or evil advisor out onto the screen", "Shows a line of text (and plays its speech) in the dialogue box"; quest_check PASS, run l1t_tutorial_temple (land1, 2026-10-31 09:52, refs/story/evidence/l1t_tutorial_temple.log line 564138) |
| The camera moves to the entrance over 4 seconds; 2 seconds in, the evil advisor: "This is the entrance. Click the Action Button on it to take you inside." (with the action button's picture) | done | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", "Whether the camera has finished a glide", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Brings the good or evil advisor out onto the screen", "Shows a line of text (and plays its speech) in the dialogue box"; quest_check PASS, run l1t_tutorial_temple (land1, 2026-10-31 09:52, refs/story/evidence/l1t_tutorial_temple.log line 571175) |
| The evil advisor stops pointing and the cinema ends when the camera arrives | done | `StopPointing` and the camera release are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |

## Going in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple is let in for the first time ("enable temple"); until now it could not be entered | done | `SetInterfaceCitadel` is real: the entrance can be tapped only while its value is not 0 (`src/Worship/Citadel.cpp` `EntranceValidToTap`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original Quest run 2026-10-28: passes, every effect's own row done. |
| Every 20 seconds until the player goes in, the evil advisor steps out and repeats "This is the entrance. Click the Action Button on it to take you inside." | done | `CreateTimer` and the timer reads are real (`src/ECS/ScriptTimer`); the scene plays through in the run of 2026-10-22, but this step was not exercised in it; the quest run of 2026-10-28 shows it (the timer set again at turn 651, see [The quest run](#the-quest-run)); finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Creates a timer running for a number of seconds", "Sets a timer's time", "Gives the time a timer has left", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Brings the good or evil advisor out onto the screen", "Shows a line of text (and plays its speech) in the dialogue box"; quest_check PASS, run l1t_tutorial_temple (land1, 2026-10-31 09:52, refs/story/evidence/l1t_tutorial_temple.log line 606120) |
| Whenever the camera strays more than 100 from the door, or the door is out of view, the camera is pulled back to the entrance over 3 seconds and the good advisor says "No, click on the door." — so the player cannot wander off until they go in | partial 60% | `InsideTemple`, the near and in-view questions and the camera moves are real; the scene plays through in the run of 2026-10-22, but this step was not exercised in it; the quest run of 2026-10-28 shows the pull-back with 1322 (a shortcut row: its timeline makes the camera stray); finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Whether the player is inside the temple", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Whether an object is in the camera's view", [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Gives the distance between two positions", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Going in through the door itself works as in the game | done | the temple entrance is tapped to go in (`src/Worship/Citadel.cpp`, `src/3D/Implementations/TempleInterior.cpp`); tests `CitadelPlanTest.APressOnTheEntranceTapsItUnlessTheScriptLockedIt` and `CitadelPlanTest.InterfaceCitadelGatesTheEntrance` (`test/test_citadel_plan.cpp`); the natural run of 2026-10-22 goes in through it (see above) and the quest run of 2026-10-28 too (`INSIDE_TEMPLE` true at turn 711) |

## Inside the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the player leaves within those first 5 seconds the reminder never runs (quirk) | partial | the same script runs, so the same quirk; the scene plays through in the run of 2026-10-22, but this step was not exercised in it, and in the quest run of 2026-10-28 the reminder never comes (see [Pending](#pending)); undetermined: in the quest run of 2026-10-28 the reminder never comes at all, so whether openblack keeps the quirk can't be told until that stop inside the temple is settled |
| Escape leaves the temple as in the game | done | Escape inside the temple goes back to its main room and out (`Game.cpp`, `TempleInterior::Escape`); tests `TempleKeys.EscapeInTheMainRoomLeaves` and `TempleKeys.EscapeInAnyOtherRoomGoesBackToTheMainRoom` (`test/temple/test_temple_room_access.cpp`); the natural run of 2026-10-22 leaves with Escape (see above) |
| The scene waits until the player is outside and no other dialogue is running | partial 60% | `IsDialogueReady` and `InsideTemple` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Whether the dialogue box is free for this script", [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Whether the player is inside the temple"; still open: no quest run's PASS is cited for it |

## Coming out: the gold scroll shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cinema begins with the neutral theme; the camera jumps to the entrance and pulls back over 3 seconds, then climbs the temple over 8 | partial 60% | `SetCameraPosition`, `SetCameraFocus`, `StartMusic` and the camera moves are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", "Takes the camera from the player at the start of a cut scene", [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Puts the game at cinema speed for a cut scene"; still open: no quest run's PASS is cited for it |
| The camera turns to look at the village (5 seconds), then over it (7 seconds) | partial 60% | real camera natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's point of view to a position over a number of seconds", "Whether the camera has finished a glide"; still open: no quest run's PASS is cited for it |
| The good advisor appears: "Now we have a Temple, let's explore. I'm dying to know what these Signposts are." and, 2 seconds in, points at a signpost in the village | partial 60% | `SpiritAppear` and `SpiritPointGameThing` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Makes an advisor appear", "Makes an advisor point at an object", "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Scrolls are made drawable ("enable highlight draw") and a gold scroll is made 14 above the creatures' gates — the same spot and height as Choose Your Creature's scroll | partial 60% | `SetDrawHighlight`, `CreateHighlight` and `HighlightProperties` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Whether challenge markers are drawn", "Creates a challenge marker", "Sets the text and category of a challenge marker"; still open: no quest run's PASS is cited for it |
| The camera drifts into the village (8 seconds); the evil advisor appears: "No, there's so much we gotta do. I say we hunt out the Gold Story Scrolls." | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Makes an advisor appear", "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| The camera turns to the scroll (8 seconds); evil advisor: "We'll need these if we're gonna progress through this world." | partial 60% | real camera and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| The camera pans slowly up the scroll (14 seconds); evil advisor: "A Gold Story Scroll looks like this.", pointing at it | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Makes an advisor point at an object", "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| The good advisor steps out, points at the scroll: "Clicking on Gold Story Scrolls with the Action Button will lead you through your Quest in Eden." | partial 60% | real advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Brings the good or evil advisor out onto the screen", "Makes an advisor point at an object", "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| The camera pulls back to a view ready for exploring (6 seconds); the evil advisor steps out and the good one stops pointing: "You can activate the Scroll or you can explore. It's up to you, Leader." | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Brings the good or evil advisor out onto the screen", "Stops an advisor pointing", "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Both advisors go home, the music stops and the cinema ends when the camera arrives | partial 60% | `SpiritHome`, `StopMusic` and the camera release are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Sends the good or evil advisor back off the screen", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Stops the script's music", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Gives the camera back to the player at the end of a cut scene", [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Puts the game back to its normal speed after a cut scene", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Whether the camera has finished a glide"; still open: no quest run's PASS is cited for it |
| The stand-in scroll is deleted; it can never be clicked | partial 60% | `ObjectDelete` deletes a highlight (mode 0); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Delete with no mode: a villager"; still open: no quest run's PASS is cited for it |

## Aftermath, failure and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing stays changed in the world except that the temple can now be entered | partial 60% | the same script runs; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Lets the player into the temple or not"; still open: no quest run's PASS is cited for it |
| The scene cannot fail; the only wait the player controls is going into the temple and out again, which the advisors and the camera pull keep pushing them to do (no soft lock while the temple stands) | partial 60% | the same script runs; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Whether the player is inside the temple", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Next in the story: Choose Your Creature's scroll appears where the stand-in was | partial 60% | in the run of 2026-10-22 `ChooseYourCreature` starts when the scene ends, and its first gate stone is laid; the scroll's place is not compared with the stand-in's; see [choose_your_creature.md](choose_your_creature.md); finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Creates a challenge marker"; still open: no quest run's PASS is cited for it |
| Music: the first epic theme for the flight, the neutral theme for the way out | partial 60% | `StartMusic` plays these tracks (`src/Audio/GameMusic`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music"; still open: no quest run's PASS is cited for it |

## The quest run

The scene is played on its own, without the land's control script, by a quest run
([../../../scenarios.md](../../../scenarios.md#playing-a-timeline-on-a-land), `tools/checks/story/README.md`):

```sh
OB_SESSION=<session> sh tools/checks/story/quest_run.sh <label> <exe folder> "SetupLand1/CitadelGuide" \
    quest.the_temple_is_finished 20000 tools/checks/story/quests/the_temple_is_finished.txt
```

- The land entry starts `SetupLand1`, then `CitadelGuide`. The timeline `quest.the_temple_is_finished`
  (`src/Debug/TestbedQuestTempleFinished.cpp`) has the temple built at its plan at once, as the control script's skip
  branch does, lets the flight play, waits for the reminder to go in, looks away over the village to be pulled back to
  the door, puts the pointer on the doorway and presses the Action button, waits inside for the reminder to leave
  (2009), presses Escape and lets the way out play.
- `tools/checks/story/quests/the_temple_is_finished.txt` holds the rows' patterns, each from the script's source;
  the pull-back row is marked shortcut, as the timeline makes the camera stray on purpose.

**The run of 2026-10-28** (Land 1, 20000 frames): 10 rows pass, 16 fail, 3 gate, 2 shortcut and 3 have no pattern,
of 34.
- The flight plays with texts 1998 (turn 299) and 2000 (turn 362), the cinema ends and the temple is let in (turn
  404); the reminder to go in repeats every 20 seconds (its timer set again at turn 651); looking away brings the
  camera back to the door with 1322.
- The press on the doorway takes the player inside (`INSIDE_TEMPLE` true at turn 711). The pointer is aimed at the
  doorway on the screen, not at the script's door marker: the marker is on the ground at the doorway's sill, and a
  press aimed at it hits the land in front of the door, as in an earlier run
  ([../../../../hand/picking.md](../../../../hand/picking.md#the-temples-entrance)).
- **It stops inside the temple.** Five seconds in, the scene sets its timer to 60 seconds and waits, while the player
  is inside, for it to run out before saying "Press the Escape key to leave the Temple." (the script,
  `refs/chl/CitadelGuide.txt:30059`..`30082`). The timer never runs down: `GET_TIMER_TIME_REMAINING` gives 60.000004
  at turn 711 to the end of the run, as the world is paused inside and its turn number stays at 711. Text 2009 is
  never said; the timeline waits for it before pressing Escape, so the player never leaves, and every row from the
  reminder on fails, "Escape leaves the temple" (done, tested) included, which was never tried.

## The reminder to leave never comes in the original either

✅ read 2026-10-28, from the matched `ScriptTimer` (bw1-decomp `cafa502d` / `250209ba`) and the frozen exe. In a
single-player game the original never says 2009 while the player is inside: openblack's stop inside the temple is the
original's behaviour, a quirk of the script.

- **A script timer counts game turns, nothing else.** `SET_TIMER_TIME` (`GScript::SetTimerTime` 0x711280, its call
  at 0x71131F) and `CREATE_TIMER` (`ScriptTimer::Create` 0x7115A0, its call at 0x7115F0) give the seconds to
  `ScriptTimer::SetTime(float)` 0x711630 (bw1-decomp `src/Black/ScriptTimer.cpp:43`): length = ftol(1000 / ms per
  turn × seconds) turns, start = the game turn `GGame::data.GameTurn` (g_game +0x205A40, `:39`).
  `GET_TIMER_TIME_REMAINING` (`GScript::GetTimeRemaining` 0x711370, its call at 0x7113F0 the only one) reads
  `GetTimeInSecondsRemaining` 0x711670 (`:48`): (length − turns since set, 0 below 0) × ms per turn × 0.001, the
  turns since set being the game turn less the start
  (`GetTurnsSinceSet` 0x7116C0, `:55`; 0x7116C5 reads g_game +0x205A40). No real time, tick count or paused flag
  enters; 60 s is 600 turns, and 600 × 100 × 0.001f is the 60.000004 the run reads.
- **Inside the temple the game turn stands still.** `GGame::GoInsideCitadel` 0x553E10 keeps the pause flag and, when
  the game is not multiplayer, calls `PauseGame(1)` (0x553F83); only `LeaveInsideCitadel` gives the kept flag back
  (0x553B4B). The one write of the game turn is `GGame::StartTurn`'s `inc` at 0x54E507, which the paused flag
  (g_game +0x14 bit 2, `test al, 4` at 0x54E4FD) jumps over; and while paused `LocalTimerSaysDoATurn` gives no world
  turn at all ([../../../../engine/game-clock.md](../../../../engine/game-clock.md#the-turn-number)). Of the other
  `PauseGame` callers, none that can run during this scene inside the temple unpauses: the Escape menu's pause is not
  reached inside, and the two `PauseGame` calls in `GGame::ProcessKey` (0x63F0E8, 0x63F1D3) pass 1
  ([../../../../interface/temple-interior.md](../../../../interface/temple-interior.md#the-citadel-interior-in-openblack)).
- **The scene still runs inside, on the temple's own turn.** It is a temple special script, and the temple's turn
  (`Temple::ProcessGameTurn` 0x794A30, every 100 ms of the tick count while paused) runs the scripts with
  `LookIn(TempleHelp | TempleSpecial)`. So the loop `while inside temple` goes round, but the timer's turn count stays
  where it was set, and `get ClickDoorTimer time remaining <= 0` is never true until the player leaves, when the loop
  ends.
- **`wait 5 seconds` does pass inside.** The script DLL's `SLEEP` (opcode 21, `Plug Ins\ScriptLibraryR.dll`
  0x10009DE0) is done once ftol(seconds × 10.0 [0x1001F13C]) is below the task's own tick count (+0x148, `jb` at
  0x10009E0B); that count goes up once per `LookIn` pass that runs the task's type (0x1000818F..0x10008212,
  [../../../../engine/loop.md](../../../../engine/loop.md)), which inside the temple is the temple's 100 ms turn. So
  the wait ends after about 50 temple turns (about 5 real seconds), and the timer is then set at the turn the player
  went in. openblack's `LHVM::Opcode21Sleep` and its `LookIn` count the same way (in the run: the timer is set at turn
  711, the turn the player went in).
- **openblack does the same.** Its timer is set and read from `game_clock::Turn()` (`src/ECS/ScriptTimer.h`), and
  `TempleInterior::Activate` pauses the clock as `GoInsideCitadel` does, so the turn stays at the one the player went
  in. No difference.
- In a multiplayer game `GoInsideCitadel` does not pause, the turns go on inside and the reminder would come after a
  minute; the scene is Land 1's single-player tutorial, so this is not reached.

## Pending

- **The timeline's way out**: Escape is pressed after a fixed time inside instead of after 2009, so that the way out
  (texts 5212 to 5232, the stand-in scroll deleted) can be checked.

