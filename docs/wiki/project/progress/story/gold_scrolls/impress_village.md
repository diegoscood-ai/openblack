# Impress Village

Khazar's second lesson on the second land, logged as the gold scroll "Impress Village": twenty minutes after the
worship lesson Khazar lays a scroll by the village and teaches casting by gesture (a spiral to open the miracle
selection, then the miracle's own gesture), then takes the player to the nearest Norse village, empties its store and
has the player fill it with food while he gives it wood. When that village is won, Khazar comes back for a lesson on
influence, which is not a scroll of its own but is covered here. The land as a whole is in [../land_2.md](../land_2.md),
the land's script in [../../scripts/land2_script.md](../../scripts/land2_script.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** Khazar (the friendly god's hand), at a scroll west of the player's village · **Script:** LearnGestures (then LearnInfluence) · **Reward:** none; the store-filling wins the Norse village over, and winning it brings the lesson on influence and the land's last scroll · **Repeatable:** no

Sources: the quest's script sources (`LearnGestures.txt`, `LearnInfluence.txt`), the land's control script
(`LandControl2.txt`, its expansion and influence watchers), the shared Land 2 scroll notifier (`SetupLand2.txt`) and the
hand demo scripts (`HandDemos.txt`), checked against the compiled scripts in the shipped `challenge.chl`; the game's
English text table (`InfoScript2.txt`) and hand demo recordings (`Data/HandDemo`). openblack's state is judged on this
tree: none of the script functions the two scripts call only logs "not implemented" in `src/CHLApi.cpp` (Khazar's hand
and actions go to his computer player, `Locator::computerPlayerSystem`; the store is read with `GET_RESOURCE`; the
challenge log is real). Land 2's story has not yet been run past its arrival since Khazar's first wait
(`COMPUTER_PLAYER_READY`) became real (see [../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest
has not appeared in a run; every row is pending unless the notes say otherwise.

**Progress: 0/66 done, 0 chosen, 5 partial, 0 inferred — 5%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When Khazar's worship lesson is done ([worship_site.md](worship_site.md)), the control script starts an expansion watcher alongside The Workshop | pending | not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The watcher sets a 20-minute timer and checks every 3 seconds; once the time is up and the player still owns the home village, the quest starts, once | pending | `CreateTimer` and `GetTimerTimeRemaining` are real (`src/ECS/ScriptTimer`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Starting it also marks that the gestures lesson has been triggered, which changes Khazar's idle remarks from "build more" ones to ones that include "take over the land" | pending | Khazar's remarks need his computer player; the computer-player natives are real (`src/CHLApi.cpp`, `Locator::computerPlayerSystem`); not yet reached in play |
| The quest's village is the nearest Norse village to the player (the one the land script also uses for the idol scroll), and its store is the storage pit within 20 of a fixed spot in it | pending | `GetTownWithId` is real; not checked in play |
| Its target is twice the cost of a grain miracle, used to keep the worship site able to pay for one | pending | `GetManaForSpell` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Norse village's store is emptied of all food and wood straight away, before the scroll appears | partial 60% | `RemoveResource` empties the store (`src/CHLApi.cpp`, `ecs::object_resources`); not checked in play against the original; see [../../town/storehouse.md](../../town/storehouse.md); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Khazar's hand flies (speed 200, fixed height) to a spot west of the player's village; a gold scroll appears there on the ground; 2 seconds later his hand is let go | pending | `MoveComputerPlayerPosition`, `ComputerPlayerReady` and `CreateHighlight` are real; not checked in play |
| The good advisor nags it while the camera is within 100 and it is on screen, at most every 30 seconds: "Look. One of Khazar's Scrolls. Let's click on it." | pending | the notifier calls real natives; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The quest waits for the scroll (or its spot) to be clicked | pending | `GameThingClicked` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Khazar's introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's own computer player is paused for the lesson | pending | `ENABLE_DISABLE_COMPUTER_PLAYER` switches his computer mind off and on (`src/CHLApi.cpp:7024`, `ComputerPlayerSystem::SwitchOn`); not yet reached in play |
| A widescreen film with Khazar's music; the camera pulls up over the village and his hand flies in (speed 300), then comes to 20 in front of the camera | pending | `SetWidescreen`, `StartMusic` and the camera are real, and so are Khazar's hand moves (`MOVE_COMPUTER_PLAYER_POSITION`, `SET_COMPUTER_PLAYER_POSITION`, `src/CHLApi.cpp:7011`, `:7050`); not yet reached in play |
| Khazar: "You have become well established." · "Our combined strength is becoming greater." · "As you near Lethys it will become more essential to have your Miracle skills in top form." (the camera drifts and his hand moves slowly aside) · "For this you must experience the benefit of gestures." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The interface is limited to moving the hand and the film ends | pending | `SetInterfaceInteraction` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## The gesture hand demo

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any miracle the player is charging is cancelled; a second film with Khazar's music turns the camera west, Khazar's hand drifts in | pending | `ClearPlayerSpellCharging` and the camera are real; Khazar's hand is real (the computer player's hand natives); not checked in play; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "You can charge Miracles without having to return to your Temple to select them" | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The worship site's power is raised to twice a grain miracle's cost if it has less | pending | `GetMana` and `GameSetMana` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The "gestures" hand demo plays, pausing at its marks: "Move your hand in a spiral like this." · "Now select the Miracle of your choice. " · "Grain is selected like this." | pending | `PlayHandDemo`, `HandDemoTrigger` are real (`Data/HandDemo/Gestures.hnd`); see [../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The dialogue closes and the log entry "Impress Village" is recorded at 0; tapping the entry in the log plays this whole gesture demo again, instead of an advisor's reminder | pending | `Snapshot` is real |
| When the demo ends, Khazar (one line, the player may act): "All your Gestures are summarised at the bottom right of the screen." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The music stops and the film ends into dialogue; the player's counts of spirals drawn and miracles picked by gesture so far are noted | pending | `StopMusic` and `GetTotalEvents` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar (one line): "You try it now." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Practising the gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze info sign on gestures appears nearby (power-ups, the spiral, leash and creature miracle gestures, repeating the last miracle) | pending | the info sign script is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The player has 5 minutes to draw the spiral and pick a miracle by its gesture; both counts must have gone up | pending | `GetTotalEvents` and `CreateTimer` are real; see [../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Every second while waiting, any miracle being charged is cancelled and the worship site's power is topped up to twice a grain miracle's cost, so the player can always afford the miracle | pending | `ClearPlayerSpellCharging`, `GetMana`, `GameSetMana` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| When the 5 minutes run out, Khazar (one line): "Well, it doesn't matter if you can't do it now." and the lesson moves on without the praise | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The interface goes back to normal | pending | `SetInterfaceInteraction` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If the player did it, a film: 1.5 seconds, then Khazar: "Superb. " | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Going to the Norse village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The village's store is emptied of food and wood again and the scroll moves to the Norse village | partial 60% | `RemoveResource` empties the store (`src/CHLApi.cpp`, `ecs::object_resources`); not checked in play against the original; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| If the village still exists: Khazar: "You should practice your gestures. Go and impress this town over here." as the camera flies there and his hand goes to above the store | pending | Khazar's hand moves (`MOVE_COMPUTER_PLAYER_POSITION`), the camera and text are real; not yet reached in play |
| The log entry is recorded at 0.5 with the good advisor's reminder "You need to fill up the Village Store with food." | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Khazar: "They look like they're in a bad way. No food or wood." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If the store exists: Khazar: "Why don't you create some food for their Village Store, while I supply them with wood." (the player may act), then "Make sure you have people worshipping at your Worship Site." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| His hand is let go and the film ends | pending | `ReleaseComputerPlayer` is real |
| If Khazar's temple has a wood miracle, it is filled with enough power for six wood miracles and he is told to cast wood on the village four times | pending | `QueueComputerPlayerAction` is real; `GetSpellIconInTemple` is real |

## Filling the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 seconds the store's food is read and the log entry is updated to 0.5 plus half the share of 2,800 food it holds | partial 60% | the store's food is read with `GetResource` (`src/CHLApi.cpp`) and `UpdateSnapshot` is real; not checked in play against the original; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The player fills it by casting grain on the store or dropping food in it (any way of adding food counts) | partial 60% | the store is read with `GetResource` (`src/CHLApi.cpp`), so any food added counts; not checked in play against the original; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| When it holds more than 2,800 food (the wood is not checked; a wood check is commented out) Khazar is let go and, if he is still alive, a film: his hand is put by the store and the camera sweeps round it | pending | the store read (`GET_RESOURCE`, `src/CHLApi.cpp:4417`) is real; not yet reached in play |
| If the village is not yet the player's, Khazar: "Excellent, we have filled the Village Store and the people are easily impressed." then "The town needs more impressing before it becomes yours. But I have matters to attend to and shall return later."; this is remembered for the influence lesson | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If it already is the player's, only the first line, as a one-line dialogue | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The step also ends, without the film, if the village or its store no longer exists | pending | the store read (`GET_RESOURCE`, `src/CHLApi.cpp:4417`) is real; not yet reached in play |
| The log entry "Impress Village" is recorded at 1 (complete), now with the evil advisor's reminder "You need to impress this Village." | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The quest then waits until the village is the player's (or no longer exists), and only then marks the gestures lesson done | pending | the town ownership read (`GetTownWithId`, a town's player) is real; not checked in play; see [../../town/belief_and_conversion.md](../../town/belief_and_conversion.md) |

## Khazar's lesson on influence

Not a scroll: no highlight and no log entry. The control script's influence watcher, started with The Workshop, waits
for the gestures lesson to be done and the Norse village to be the player's, then runs it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The influence watcher checks for both conditions over and over without any pause | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If Khazar is alive: a widescreen film with Khazar's music; his hand flies (speed 300) towards the Indian village | pending | `SetWidescreen` and `StartMusic` are real, but Khazar's hand is real (the computer player's hand natives); not checked in play |
| If the store was filled before the village was won: the camera flies a recorded track and Khazar says "Congratulations." then "You are expanding well. Lethys had better beware."; otherwise the camera is put on a fixed view | pending | `RunCameraPath` and `RunText` are real; see [../../camera/camera_paths.md](../../camera/camera_paths.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "Time is not kind, you need to gain influence in the direction of our common enemy." | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If the Indian village (the one where The Sea and The Plague happen) is not yet the player's, the camera turns to it and Khazar: "I strongly encourage you to gain this town."; either way, Khazar: "Before I leave to defend my realm I must tell you one more thing." | pending | real camera and text natives; the town ownership read (`GET_PROPERTY` player, `src/CHLApi.cpp:1818`) is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The screen fades to black over 2 seconds, the camera is put high above the edge of the player's influence and fades back in | pending | `SetFade`, `SetFadeIn`, `SetCameraPosition` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| Khazar: "You will notice you cannot do anything, even cast Miracles outside of your influence." · "However, there is a way using the power of belief." as the camera drops to the edge | pending | `RunText` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The "Influence" hand demo plays, pausing once: "You can reach outside the edge of your realm for short periods of time" · "But your influence is soon lost until your hand is returned to your realm." | pending | `PlayHandDemo` is real (`Data/HandDemo/Influence.hnd`), and virtual influence (`SET_VIRTUAL_INFLUENCE`, `src/CHLApi.cpp:6168`) is real; not yet reached in play; see [../../worship/influence.md](../../worship/influence.md) |
| Fade to black, the camera is put back on the view before the demo, fade in, the music stops and the film ends; Khazar is let go | pending | fades, camera and `StopMusic` are real; letting Khazar go is real (`RELEASE_COMPUTER_PLAYER`, `src/CHLApi.cpp:7834`); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| A bronze info sign on influence appears (the faded hand, virtual influence, what can be done outside influence) | pending | the info sign script is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If Khazar is already dead, only the info sign appears | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| When the lesson ends, the watcher starts the land's last scroll ([destroy_it.md](destroy_it.md)) unless Khazar's death has already started it | pending | plain script flags; see [destroy_it.md](destroy_it.md); not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Failing and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't be failed: the gesture practice gives up after 5 minutes and the store has no time limit | pending | the same script; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| If the player loses the home village before the 20 minutes are up, the quest waits until it is theirs again | pending | the town ownership read (`GetTownWithId`) is real; not checked in play |
| If Khazar is dead when the scroll is clicked, his lessons are skipped and the quest at once marks itself done without logging anything; the scroll is never removed | pending | Khazar's death needs computer-player natives (real now; not checked in play) |
| If the Norse village is destroyed, the store step ends and the lesson is marked done, but the influence lesson never comes, since it waits for that village to be won | pending | the store and town reads are real (`GET_RESOURCE`, `GET_TOWN_WITH_ID`); not yet reached in play |
| Winning the Norse village before the scroll is clicked does not skip the lesson; the store must still be filled | pending | the store read (`GET_RESOURCE`, `src/CHLApi.cpp:4417`) is real; not yet reached in play |

## Advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is nagged by the good advisor; the log reminders are the good advisor's (fill the store), then the evil advisor's (impress the village) | partial 60% | the notifier and reminders call real natives; the challenge record is real (`Snapshot`, `UpdateSnapshot`); re-measured 2026-10-21: the script calls only natives that work; not checked in a run yet |
| Khazar's theme plays through each of his films | pending | `StartMusic` and `StopMusic` are real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part; it can help fill the store, since only the store's food is read | pending | follows from the store read (`GET_RESOURCE`, real); not yet reached in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the 5 minutes run out, both noted counts are pushed on by one each second until neither matches; if the player had done just one of the two steps, a count can match again and "Well, it doesn't matter…" is said twice | pending | `GetTotalEvents` is real; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, nor has the quest's own wait for Khazar's hand |
| The log entry is updated before the 2,800 check, so it can show just over complete for a moment | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Khazar's wood miracle is given power for six casts but he is told to cast four | pending | `QueueComputerPlayerAction` is real |
| Khazar's computer player is paused at the start of the lesson and no Land 2 script un-pauses it; whether letting his hand go lifts the pause is undetermined | pending | `ENABLE_DISABLE_COMPUTER_PLAYER` switches his computer mind off and on (`src/CHLApi.cpp:7024`, `ComputerPlayerSystem::SwitchOn`); not yet reached in play |

