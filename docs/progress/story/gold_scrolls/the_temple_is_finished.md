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
reaches this scene, but the game is checked only up to the intro's hand-over
([../../scripts/land1_script.md](../../scripts/land1_script.md)), and none of the 41 commands it needs only logs "not
implemented" in `src/CHLApi.cpp`.

**Progress: 3/34 done, 30 partial — 53%**

## Where it sits in the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script runs the scene straight after the opening walk ("follow us", [../tutorial.md](../tutorial.md)), which ends only when the villagers have finished the temple | partial | `LandControl1` runs `CitadelGuide` after `FollowUs`; the intro is checked in game up to the hand-over, the rest of `FollowUs` (the walk and the citadel) is not. See [../../scripts/land1_script.md](../../scripts/land1_script.md) |
| A new game that skips to choosing the creature never plays it: the temple is built at once at its spot and marked finished instead | done | `LandControl1`'s skip branch: `BuildBuilding`, `CallNear` and `SetProperty` (built percentage) are real, and the test runs (fourth SkipBox answer) start with the temple built. See [../../../bw1-notes/map-loading.md](../../../bw1-notes/map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| Before the scene, two did-you-know scrolls are placed: one by the village on zooming, rotating and double clicking, one by the temple door on the Space bar's safe camera position | partial | the shared `DidYouKnow` script calls only real natives (`CreateHighlight` is real, `src/ECS/ScriptHighlight`); not checked in game past the intro's hand-over |
| The control script waits for the whole scene (including the visit inside the temple) before it places the fish-farm did-you-know, starts Throwing Stones and The Lost Flock, and runs Choose Your Creature | partial | plain script order in `LandControl1`; not checked in game past the intro's hand-over. See [../silver_scrolls/throwing_stones.md](../silver_scrolls/throwing_stones.md), [../silver_scrolls/the_lost_flock.md](../silver_scrolls/the_lost_flock.md), [choose_your_creature.md](choose_your_creature.md) |
| The scene is logged nowhere: no challenge record, no reminder, nothing in the scroll log | partial | the same script runs, so nothing is logged either; not checked in game past the intro's hand-over |
| The scene is a "temple" kind of script: while the player is inside the temple, only temple scripts may take the camera, so this scene can still run its own camera there | partial | inside the temple only the temple scripts run (`Game::ProcessTempleTurn`) and only they get the camera (`src/Help/ScriptControl.cpp`); not checked in game past the intro's hand-over |

## The temple flight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema (widescreen bars, the player's control taken) begins with the first epic theme | partial | `SetWidescreen`, `StartCameraControl`, `StartMusic` are real (`src/CHLApi.cpp`); not checked in game past the intro's hand-over |
| The camera flies to the temple over 6 seconds, then sinks slowly down its face over 23 seconds | partial | `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived` are real (`src/Camera/ScriptCamera`); not checked in game past the intro's hand-over |
| Both advisors step out; the good advisor: "At last. The people have finished our Temple." | partial | `SpiritEject` and `RunText` are real (advisors and texts in `src/Help`); not checked in game past the intro's hand-over |
| A second line, "It's beautiful and it will be extremely useful later.", is in the text table but commented out of the scene | n/a | cut line |
| The camera moves to the entrance over 4 seconds; 2 seconds in, the evil advisor: "This is the entrance. Click the Action Button on it to take you inside." (with the action button's picture) | partial | real camera, advisor and text natives; not checked in game past the intro's hand-over |
| The evil advisor stops pointing and the cinema ends when the camera arrives | partial | `StopPointing` and the camera release are real; not checked in game past the intro's hand-over |

## Going in

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple is let in for the first time ("enable temple"); until now it could not be entered | partial | `SetInterfaceCitadel` is real: the entrance can be tapped only while its value is not 0 (`src/Worship/Citadel.cpp` `EntranceValidToTap`); not checked in game past the intro's hand-over |
| Every 20 seconds until the player goes in, the evil advisor steps out and repeats "This is the entrance. Click the Action Button on it to take you inside." | partial | `CreateTimer` and the timer reads are real (`src/ECS/ScriptTimer`); not checked in game past the intro's hand-over |
| Whenever the camera strays more than 100 from the door, or the door is out of view, the camera is pulled back to the entrance over 3 seconds and the good advisor says "No, click on the door." — so the player cannot wander off until they go in | partial | `InsideTemple`, the near and in-view questions and the camera moves are real; not checked in game past the intro's hand-over |
| Going in through the door itself works as in the game | done | the temple entrance is tapped to go in (`src/Worship/Citadel.cpp`, `src/3D/Implementations/TempleInterior.cpp`); tests in `test/temple/` |

## Inside the temple

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 5 seconds after the player goes in, a timer starts: after a minute inside, then every 30 seconds, the good advisor says "Press the Escape key to leave the Temple." | partial | `InsideTemple` (`game_clock::IsInsideCitadel`) and the timers are real; not checked in game past the intro's hand-over |
| If the player leaves within those first 5 seconds the reminder never runs (quirk) | partial | the same script runs, so the same quirk; not checked in game past the intro's hand-over |
| Escape leaves the temple as in the game | done | Escape inside the temple goes back to its main room and out (`Game.cpp`, `TempleInterior::Escape`) |
| The scene waits until the player is outside and no other dialogue is running | partial | `IsDialogueReady` and `InsideTemple` are real; not checked in game past the intro's hand-over |

## Coming out: the gold scroll shown

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second cinema begins with the neutral theme; the camera jumps to the entrance and pulls back over 3 seconds, then climbs the temple over 8 | partial | `SetCameraPosition`, `SetCameraFocus`, `StartMusic` and the camera moves are real; not checked in game past the intro's hand-over |
| The camera turns to look at the village (5 seconds), then over it (7 seconds) | partial | real camera natives; not checked in game past the intro's hand-over |
| The good advisor appears: "Now we have a Temple, let's explore. I'm dying to know what these Signposts are." and, 2 seconds in, points at a signpost in the village | partial | `SpiritAppear` and `SpiritPointGameThing` are real; not checked in game past the intro's hand-over |
| Scrolls are made drawable ("enable highlight draw") and a gold scroll is made 14 above the creatures' gates — the same spot and height as Choose Your Creature's scroll | partial | `SetDrawHighlight`, `CreateHighlight` and `HighlightProperties` are real; not checked in game past the intro's hand-over |
| The camera drifts into the village (8 seconds); the evil advisor appears: "No, there's so much we gotta do. I say we hunt out the Gold Story Scrolls." | partial | real camera, advisor and text natives; not checked in game past the intro's hand-over |
| The camera turns to the scroll (8 seconds); evil advisor: "We'll need these if we're gonna progress through this world." | partial | real camera and text natives; not checked in game past the intro's hand-over |
| The camera pans slowly up the scroll (14 seconds); evil advisor: "A Gold Story Scroll looks like this.", pointing at it | partial | real camera, advisor and text natives; not checked in game past the intro's hand-over |
| The good advisor steps out, points at the scroll: "Clicking on Gold Story Scrolls with the Action Button will lead you through your Quest in Eden." | partial | real advisor and text natives; not checked in game past the intro's hand-over |
| The camera pulls back to a view ready for exploring (6 seconds); the evil advisor steps out and the good one stops pointing: "You can activate the Scroll or you can explore. It's up to you, Leader." | partial | real camera, advisor and text natives; not checked in game past the intro's hand-over |
| Both advisors go home, the music stops and the cinema ends when the camera arrives | partial | `SpiritHome`, `StopMusic` and the camera release are real; not checked in game past the intro's hand-over |
| The stand-in scroll is deleted; it can never be clicked | partial | `ObjectDelete` deletes a highlight (mode 0); not checked in game past the intro's hand-over |

## Aftermath, failure and story order

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing stays changed in the world except that the temple can now be entered | partial | the same script runs; not checked in game past the intro's hand-over |
| The scene cannot fail; the only wait the player controls is going into the temple and out again, which the advisors and the camera pull keep pushing them to do (no soft lock while the temple stands) | partial | the same script runs; not checked in game past the intro's hand-over |
| Next in the story: Choose Your Creature's scroll appears where the stand-in was | todo | Choose Your Creature then waits for ever at the creature gates (`ObjectInfoBits` is a stub); see [choose_your_creature.md](choose_your_creature.md) |
| Music: the first epic theme for the flight, the neutral theme for the way out | partial | `StartMusic` plays these tracks (`src/Audio/Services/GameMusic`); not checked in game past the intro's hand-over |
| No creature is involved (it hasn't been chosen yet) | n/a | no creature is involved |

## Unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Commented-out camera moves straight onto the gold scroll and back to the temple | n/a | cut from the scene |
| An older draft of the temple's opening (rotating the camera, carrying wood and food to the builders, the kinds of scroll) | n/a | not in the shipped program: [../silver_scrolls/see_the_citadel.md](../silver_scrolls/see_the_citadel.md) |
