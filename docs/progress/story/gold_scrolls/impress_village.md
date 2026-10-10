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
tree: of the 71 script functions the two scripts and the scripts they run need, 13 still only log "not implemented" in
`src/CHLApi.cpp` (among them Khazar's hand and actions, the town store and the challenge log). openblack never runs Land
2's control script at all (see [../../scripts/land2_script.md](../../scripts/land2_script.md)), so the quest never
appears; every row is todo unless the notes say otherwise.

**Progress: 0/66 done, 1 partial — 1%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When Khazar's worship lesson is done ([worship_site.md](worship_site.md)), the control script starts an expansion watcher alongside The Workshop | todo | never reached: Land 2 needs `LOAD_MAP`, an empty native |
| The watcher sets a 20-minute timer and checks every 3 seconds; once the time is up and the player still owns the home village, the quest starts, once | todo | `CreateTimer` and `GetTimerTimeRemaining` are real (`src/ECS/ScriptTimer`); never reached: Land 2 needs `LOAD_MAP`, an empty native |
| Starting it also marks that the gestures lesson has been triggered, which changes Khazar's idle remarks from "build more" ones to ones that include "take over the land" | todo | Khazar's remarks need his computer player (all computer-player natives are stubs) |
| The quest's village is the nearest Norse village to the player (the one the land script also uses for the idol scroll), and its store is the storage pit within 20 of a fixed spot in it | todo | `GetTownWithId` is a stub |
| Its target is twice the cost of a grain miracle, used to keep the worship site able to pay for one | todo | `GetManaForSpell` is real; never reached: Land 2 needs `LOAD_MAP`, an empty native |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Norse village's store is emptied of all food and wood straight away, before the scroll appears | todo | `RemoveResource` is a stub; see [../../town/storehouse.md](../../town/storehouse.md) |
| Khazar's hand flies (speed 200, fixed height) to a spot west of the player's village; a gold scroll appears there on the ground; 2 seconds later his hand is let go | todo | `MoveComputerPlayerPosition` and `ComputerPlayerReady` are stubs (the wait for his hand never ends); `CreateHighlight` is real |
| The good advisor nags it while the camera is within 100 and it is on screen, at most every 30 seconds: "Look. One of Khazar's Scrolls. Let's click on it." | todo | the notifier calls real natives; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The quest waits for the scroll (or its spot) to be clicked | todo | `GameThingClicked` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Khazar's introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's own computer player is paused for the lesson | todo | `EnableDisableComputerPlayer` is a stub |
| A widescreen film with Khazar's music; the camera pulls up over the village and his hand flies in (speed 300), then comes to 20 in front of the camera | todo | `SetWidescreen`, `StartMusic` and the camera are real, but Khazar's hand moves are stubs |
| Khazar: "You have become well established." · "Our combined strength is becoming greater." · "As you near Lethys it will become more essential to have your Miracle skills in top form." (the camera drifts and his hand moves slowly aside) · "For this you must experience the benefit of gestures." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The interface is limited to moving the hand and the film ends | todo | `SetInterfaceInteraction` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## The gesture hand demo

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any miracle the player is charging is cancelled; a second film with Khazar's music turns the camera west, Khazar's hand drifts in | todo | `ClearPlayerSpellCharging` and the camera are real; Khazar's hand is a stub; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Khazar: "You can charge Miracles without having to return to your Temple to select them" | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The worship site's power is raised to twice a grain miracle's cost if it has less | todo | `GetMana` and `GameSetMana` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The "gestures" hand demo plays, pausing at its marks: "Move your hand in a spiral like this." · "Now select the Miracle of your choice. " · "Grain is selected like this." | todo | `PlayHandDemo`, `HandDemoTrigger` are real (`Data/HandDemo/Gestures.hnd`); see [../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md); never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The dialogue closes and the log entry "Impress Village" is recorded at 0; tapping the entry in the log plays this whole gesture demo again, instead of an advisor's reminder | todo | `Snapshot` is a stub |
| When the demo ends, Khazar (one line, the player may act): "All your Gestures are summarised at the bottom right of the screen." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The music stops and the film ends into dialogue; the player's counts of spirals drawn and miracles picked by gesture so far are noted | todo | `StopMusic` and `GetTotalEvents` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Khazar (one line): "You try it now." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Practising the gesture

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A bronze info sign on gestures appears nearby (power-ups, the spiral, leash and creature miracle gestures, repeating the last miracle) | todo | the info sign script is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The player has 5 minutes to draw the spiral and pick a miracle by its gesture; both counts must have gone up | todo | `GetTotalEvents` and `CreateTimer` are real; see [../../gesture/miracle_gestures.md](../../gesture/miracle_gestures.md); never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Every second while waiting, any miracle being charged is cancelled and the worship site's power is topped up to twice a grain miracle's cost, so the player can always afford the miracle | todo | `ClearPlayerSpellCharging`, `GetMana`, `GameSetMana` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| When the 5 minutes run out, Khazar (one line): "Well, it doesn't matter if you can't do it now." and the lesson moves on without the praise | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The interface goes back to normal | todo | `SetInterfaceInteraction` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If the player did it, a film: 1.5 seconds, then Khazar: "Superb. " | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Going to the Norse village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The village's store is emptied of food and wood again and the scroll moves to the Norse village | todo | `RemoveResource` is a stub |
| If the village still exists: Khazar: "You should practice your gestures. Go and impress this town over here." as the camera flies there and his hand goes to above the store | todo | Khazar's hand moves are stubs; the camera and text are real |
| The log entry is recorded at 0.5 with the good advisor's reminder "You need to fill up the Village Store with food." | todo | the log entry is a stub |
| Khazar: "They look like they're in a bad way. No food or wood." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If the store exists: Khazar: "Why don't you create some food for their Village Store, while I supply them with wood." (the player may act), then "Make sure you have people worshipping at your Worship Site." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| His hand is let go and the film ends | todo | `ReleaseComputerPlayer` is a stub |
| If Khazar's temple has a wood miracle, it is filled with enough power for six wood miracles and he is told to cast wood on the village four times | todo | `QueueComputerPlayerAction` is a stub; `GetSpellIconInTemple` is real |

## Filling the store

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 seconds the store's food is read and the log entry is updated to 0.5 plus half the share of 2,800 food it holds | todo | `GetResource` and `UpdateSnapshot` are stubs |
| The player fills it by casting grain on the store or dropping food in it (any way of adding food counts) | todo | the store read `GetResource` is a stub |
| When it holds more than 2,800 food (the wood is not checked; a wood check is commented out) Khazar is let go and, if he is still alive, a film: his hand is put by the store and the camera sweeps round it | todo | the store read is a stub |
| If the village is not yet the player's, Khazar: "Excellent, we have filled the Village Store and the people are easily impressed." then "The town needs more impressing before it becomes yours. But I have matters to attend to and shall return later."; this is remembered for the influence lesson | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If it already is the player's, only the first line, as a one-line dialogue | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The step also ends, without the film, if the village or its store no longer exists | todo | the store read is a stub |
| The log entry "Impress Village" is recorded at 1 (complete), now with the evil advisor's reminder "You need to impress this Village." | todo | the log entry is a stub |
| The quest then waits until the village is the player's (or no longer exists), and only then marks the gestures lesson done | todo | the town ownership needs `GetTownWithId` (a stub); see [../../town/belief_and_conversion.md](../../town/belief_and_conversion.md) |

## Khazar's lesson on influence

Not a scroll: no highlight and no log entry. The control script's influence watcher, started with The Workshop, waits
for the gestures lesson to be done and the Norse village to be the player's, then runs it.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The influence watcher checks for both conditions over and over without any pause | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If Khazar is alive: a widescreen film with Khazar's music; his hand flies (speed 300) towards the Indian village | todo | `SetWidescreen` and `StartMusic` are real, but Khazar's hand is a stub |
| If the store was filled before the village was won: the camera flies a recorded track and Khazar says "Congratulations." then "You are expanding well. Lethys had better beware."; otherwise the camera is put on a fixed view | todo | `RunCameraPath` and `RunText` are real; see [../../camera/camera_paths.md](../../camera/camera_paths.md); never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Khazar: "Time is not kind, you need to gain influence in the direction of our common enemy." | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If the Indian village (the one where The Sea and The Plague happen) is not yet the player's, the camera turns to it and Khazar: "I strongly encourage you to gain this town."; either way, Khazar: "Before I leave to defend my realm I must tell you one more thing." | todo | real camera and text natives; the town ownership read is a stub; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The screen fades to black over 2 seconds, the camera is put high above the edge of the player's influence and fades back in | todo | `SetFade`, `SetFadeIn`, `SetCameraPosition` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| Khazar: "You will notice you cannot do anything, even cast Miracles outside of your influence." · "However, there is a way using the power of belief." as the camera drops to the edge | todo | `RunText` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The "Influence" hand demo plays, pausing once: "You can reach outside the edge of your realm for short periods of time" · "But your influence is soon lost until your hand is returned to your realm." | todo | `PlayHandDemo` is real (`Data/HandDemo/Influence.hnd`), but virtual influence (`SetVirtualInfluence`) is a stub; see [../../worship/influence.md](../../worship/influence.md) |
| Fade to black, the camera is put back on the view before the demo, fade in, the music stops and the film ends; Khazar is let go | todo | fades, camera and `StopMusic` are real; letting Khazar go is a stub; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| A bronze info sign on influence appears (the faded hand, virtual influence, what can be done outside influence) | todo | the info sign script is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If Khazar is already dead, only the info sign appears | todo | Khazar's death needs computer-player natives (stubs) |
| When the lesson ends, the watcher starts the land's last scroll ([destroy_it.md](destroy_it.md)) unless Khazar's death has already started it | todo | plain script flags; see [destroy_it.md](destroy_it.md); never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Failing and the story

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest can't be failed: the gesture practice gives up after 5 minutes and the store has no time limit | todo | the same script; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| If the player loses the home village before the 20 minutes are up, the quest waits until it is theirs again | todo | the town ownership read (`GetTownWithId`) is a stub |
| If Khazar is dead when the scroll is clicked, his lessons are skipped and the quest at once marks itself done without logging anything; the scroll is never removed | todo | Khazar's death needs computer-player natives (stubs) |
| If the Norse village is destroyed, the store step ends and the lesson is marked done, but the influence lesson never comes, since it waits for that village to be won | todo | the store and town reads are stubs |
| Winning the Norse village before the scroll is clicked does not skip the lesson; the store must still be filled | todo | the store read is a stub |

## Advisors and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is nagged by the good advisor; the log reminders are the good advisor's (fill the store), then the evil advisor's (impress the village) | partial | the notifier and reminders call real natives; the log entries are stubs |
| Khazar's theme plays through each of his films | todo | `StartMusic` and `StopMusic` are real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part; it can help fill the store, since only the store's food is read | todo | follows from the store read, a stub |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the 5 minutes run out, both noted counts are pushed on by one each second until neither matches; if the player had done just one of the two steps, a count can match again and "Well, it doesn't matter…" is said twice | todo | `GetTotalEvents` is real; never reached (Land 2 needs `LOAD_MAP`), and the quest would hang first at its wait for Khazar's hand (`ComputerPlayerReady` is a stub) |
| The log entry is updated before the 2,800 check, so it can show just over complete for a moment | todo | the log entry is a stub |
| Khazar's wood miracle is given power for six casts but he is told to cast four | todo | `QueueComputerPlayerAction` is a stub |
| Khazar's computer player is paused at the start of the lesson and no Land 2 script un-pauses it; whether letting his hand go lifts the pause is undetermined | todo | `EnableDisableComputerPlayer` is a stub |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two spare influence demo scripts exist in the hand demo file; one has its demo commented out, the other plays a second influence recording (`influence2.hnd`); nothing in the game runs them | n/a | never in the shipped game |
| An earlier place for the 0 log entry, after the gesture demo, is commented out | n/a | commented out |
| A wood check on the store (2,800 wood as well as food) is commented out | n/a | commented out |
| A note to clear the repeat-miracle icon and remove the leash "for confusion" is left as a to-do | n/a | a to-do note in the source |
