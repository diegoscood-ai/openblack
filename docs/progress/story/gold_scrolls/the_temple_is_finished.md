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
one by one, and the reminders and the camera pull-back were not exercised.

**Progress: 3/34 done, 0 chosen, 31 partial, 0 inferred — 64%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script runs the scene straight after the opening walk ("follow us", [../tutorial.md](../tutorial.md)), which ends only when the villagers have finished the temple | partial 60% | `LandControl1` runs `CitadelGuide` after `FollowUs`; in the run of 2026-10-22 the whole of `FollowUs` plays, its builders finish the temple on their own and `CheckCitadel` sets `CitadelBuiltL1` to 1 (about 4.5 min of play after the pick-up demo in one run, about 1.5 min in another), then `CitadelGuide` runs. See [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| A new game that skips to choosing the creature never plays it: the temple is built at once at its spot and marked finished instead | done | `LandControl1`'s skip branch: `BuildBuilding`, `CallNear` and `SetProperty` (built percentage) are real, and the test runs (fourth SkipBox answer) start with the temple built. See [land-change-and-saving.md](../../../../scripts/land-change-and-saving.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| Before the scene, two did-you-know scrolls are placed: one by the village on zooming, rotating and double clicking, one by the temple door on the Space bar's safe camera position | partial 60% | the shared `DidYouKnow` script calls only real natives (`CreateHighlight` is real, `src/ECS/ScriptHighlight`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The control script waits for the whole scene (including the visit inside the temple) before it places the fish-farm did-you-know, starts Throwing Stones and The Lost Flock, and runs Choose Your Creature | partial 60% | plain script order in `LandControl1`; in the run of 2026-10-22 Choose Your Creature starts only once the player is out of the temple and the scene is over. See [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [choose_your_creature.md](choose_your_creature.md); the fish-farm did-you-know and the two silver starts were not looked at in that run |
| The scene is logged nowhere: no challenge record, no reminder, nothing in the scroll log | partial 60% | the same script runs, so nothing is logged either; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The scene is a "temple" kind of script: while the player is inside the temple, only temple scripts may take the camera, so this scene can still run its own camera there | partial 60% | inside the temple only the temple scripts run (`Game::ProcessTempleTurn`) and only they get the camera (`src/Help/ScriptControl.cpp`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |

## The temple flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema (widescreen bars, the player's control taken) begins with the first epic theme | partial 60% | `SetWidescreen`, `StartCameraControl`, `StartMusic` are real (`src/CHLApi.cpp`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera flies to the temple over 6 seconds, then sinks slowly down its face over 23 seconds | partial 60% | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` are real (`src/Camera/ScriptCamera`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| Both advisors step out; the good advisor: "At last. The people have finished our Temple." | partial 60% | `SpiritEject` and `RunText` are real (advisors and texts in `src/Help`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera moves to the entrance over 4 seconds; 2 seconds in, the evil advisor: "This is the entrance. Click the Action Button on it to take you inside." (with the action button's picture) | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The evil advisor stops pointing and the cinema ends when the camera arrives | partial 60% | `StopPointing` and the camera release are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |

## Going in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple is let in for the first time ("enable temple"); until now it could not be entered | partial 60% | `SetInterfaceCitadel` is real: the entrance can be tapped only while its value is not 0 (`src/Worship/Citadel.cpp` `EntranceValidToTap`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| Every 20 seconds until the player goes in, the evil advisor steps out and repeats "This is the entrance. Click the Action Button on it to take you inside." | partial 60% | `CreateTimer` and the timer reads are real (`src/ECS/ScriptTimer`); the scene plays through in the run of 2026-10-22, but this step was not exercised in it |
| Whenever the camera strays more than 100 from the door, or the door is out of view, the camera is pulled back to the entrance over 3 seconds and the good advisor says "No, click on the door." — so the player cannot wander off until they go in | partial 60% | `InsideTemple`, the near and in-view questions and the camera moves are real; the scene plays through in the run of 2026-10-22, but this step was not exercised in it |
| Going in through the door itself works as in the game | done | the temple entrance is tapped to go in (`src/Worship/Citadel.cpp`, `src/3D/Implementations/TempleInterior.cpp`); tests in `test/temple/` |

## Inside the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 5 seconds after the player goes in, a timer starts: after a minute inside, then every 30 seconds, the good advisor says "Press the Escape key to leave the Temple." | partial 60% | `InsideTemple` (`game_clock::IsInsideCitadel`) and the timers are real; the scene plays through in the run of 2026-10-22, but this step was not exercised in it |
| If the player leaves within those first 5 seconds the reminder never runs (quirk) | partial 60% | the same script runs, so the same quirk; the scene plays through in the run of 2026-10-22, but this step was not exercised in it |
| Escape leaves the temple as in the game | done | Escape inside the temple goes back to its main room and out (`Game.cpp`, `TempleInterior::Escape`) |
| The scene waits until the player is outside and no other dialogue is running | partial 60% | `IsDialogueReady` and `InsideTemple` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |

## Coming out: the gold scroll shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cinema begins with the neutral theme; the camera jumps to the entrance and pulls back over 3 seconds, then climbs the temple over 8 | partial 60% | `SetCameraPosition`, `SetCameraFocus`, `StartMusic` and the camera moves are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera turns to look at the village (5 seconds), then over it (7 seconds) | partial 60% | real camera natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The good advisor appears: "Now we have a Temple, let's explore. I'm dying to know what these Signposts are." and, 2 seconds in, points at a signpost in the village | partial 60% | `SpiritAppear` and `SpiritPointGameThing` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| Scrolls are made drawable ("enable highlight draw") and a gold scroll is made 14 above the creatures' gates — the same spot and height as Choose Your Creature's scroll | partial 60% | `SetDrawHighlight`, `CreateHighlight` and `HighlightProperties` are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera drifts into the village (8 seconds); the evil advisor appears: "No, there's so much we gotta do. I say we hunt out the Gold Story Scrolls." | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera turns to the scroll (8 seconds); evil advisor: "We'll need these if we're gonna progress through this world." | partial 60% | real camera and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera pans slowly up the scroll (14 seconds); evil advisor: "A Gold Story Scroll looks like this.", pointing at it | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The good advisor steps out, points at the scroll: "Clicking on Gold Story Scrolls with the Action Button will lead you through your Quest in Eden." | partial 60% | real advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The camera pulls back to a view ready for exploring (6 seconds); the evil advisor steps out and the good one stops pointing: "You can activate the Scroll or you can explore. It's up to you, Leader." | partial 60% | real camera, advisor and text natives; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| Both advisors go home, the music stops and the cinema ends when the camera arrives | partial 60% | `SpiritHome`, `StopMusic` and the camera release are real; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The stand-in scroll is deleted; it can never be clicked | partial 60% | `ObjectDelete` deletes a highlight (mode 0); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |

## Aftermath, failure and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing stays changed in the world except that the temple can now be entered | partial 60% | the same script runs; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| The scene cannot fail; the only wait the player controls is going into the temple and out again, which the advisors and the camera pull keep pushing them to do (no soft lock while the temple stands) | partial 60% | the same script runs; plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |
| Next in the story: Choose Your Creature's scroll appears where the stand-in was | partial 60% | in the run of 2026-10-22 `ChooseYourCreature` starts when the scene ends, and its first gate stone is laid; the scroll's place is not compared with the stand-in's; see [choose_your_creature.md](choose_your_creature.md) |
| Music: the first epic theme for the flight, the neutral theme for the way out | partial 60% | `StartMusic` plays these tracks (`src/Audio/GameMusic`); plays in the run of 2026-10-22 (the scene runs to its end); not compared with the original |

