# The Spiritual Healer

A second-land village event, not a silver scroll: once the player wins the Greek village with town id 9 (the one the
control script calls Town5), a priest appears there who keeps the villagers young but feeds on their children, taking one
about every half minute. The player can leave him, or pick him up and kill or banish him, which kills the whole village.
The game's text table has no title for it; "The Spiritual Healer" is a descriptive title taken from the script's name.

**Land:** 2 · **Giver:** the healer, a priest made at a house in the village with town id 9 · **Script:** SpiritualHealer · **Reward:** none · **Repeatable:** no

Sources: the event's script source (`SpiritualHealer.txt`, with the land control script `LandControl2.txt`), checked
line by line against the PC game's compiled `challenge.chl`; the game's text table; the land file `Land2.txt` for which
town sits where; and the executable for how stopping scripts and setting a town's properties work. It is not a scroll:
neither the source nor the compiled code creates a highlight, takes or updates a snapshot, or names a scroll title, and
no HELP_TEXT_TITLE line exists for it, so it never appears in the challenge log. openblack is judged on this tree: of
the 48 script functions the event's scripts need, one is still a stub in `src/CHLApi.cpp` at this tree, the beam
effect (`ADD_SPOT_VISUAL_TARGET_OBJECT`); the town look-up is real; creating villagers works (`Create`). The land's control script never runs (see
[../land_2.md](../land_2.md) and [../../scripts/land2_script.md](../../scripts/land2_script.md)), so none of it happens;
rows are pending unless the notes say otherwise. Function coverage is in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/55 done, 0 chosen, 3 partial, 0 inferred — 3%**

## What it is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It has no reward and no alignment set by the script; whatever alignment the player gains or loses comes from the deaths and acts themselves | pending | undetermined how much the game's own rules move alignment for these deaths; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher on the village with town id 9 when the land is set up | pending | `LandControl2` has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)) |
| Every 1.3 seconds the watcher checks whether the village belongs to the player; the moment it does, the event starts and the watcher stops | pending | reading a town's owner (`GetProperty` player) is real (`src/CHLApi.cpp:1818`); not reached in play |
| The healer is made as a priest villager at a house by the village centre; he joins no village and has no special protection: he can be picked up, thrown and killed | pending | `Create` makes villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Two background scripts start with him: his walk round the village and his keeping of the villagers' age | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The healer's rounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He walks a loop of five spots round the village (first, second, fourth, third, fifth), stopping at each to look for something or stand idle, then starts again | pending | `MoveGameThing` and `Played` work for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each walk ends when he is within 5 of the spot or after 30 seconds; each animation ends when played or after 30 seconds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| At each stop he waits while he is in the player's hand or the creature's | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| The rounds end only when he dies | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Keeping the village young

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 minutes every villager of the village is set to age 16 | pending | setting a property on a town (`SetProperty` age on a container) is not ported; in the game it applies to each of its villagers |
| At the same time, unless he is in the player's or the creature's hand, a sparkle of success shows on him for 3 seconds | pending | `SpecialEffectPosition` works; the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| At the same time, if the camera is within 100 of him and he is on screen, he calls out "While my heart beats you will all never age!" | pending | `GamePlaySaySoundEffect` and `GameThingFieldOfView` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| This keeps going until he dies; banishing him does not stop it | pending | see the quirks below |
| Undetermined whether age 16 makes the village's children grow up, or what the game counts as a child for the healer's choice | pending | not settled by the scripts |

## Taking the children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 30 seconds he looks for a child of the village; if there is one, he takes it | pending | `Call` works; undetermined which child is picked; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He stops his rounds and stands where he is; the child leaves the village | pending | `StopScript` and `FlockDetach` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He faces the child and beckons, and a magic beam runs from him to the child (lasting up to 20 seconds) | pending | `SpecialEffectObject` works, `AddSpotVisualTargetObject` is a stub (`src/CHLApi.cpp:6811`) |
| The child walks to him, until within 1.5 or for 30 seconds, and faces him | pending | `MoveGameThing` and `SetFocus` work for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| For the first two children he takes, if the camera is within 150, he is on screen and no scene is playing: the good advisor pops out, "Oh, I don't think I want to see this!", and vanishes; the evil advisor pops out: "I do. Let me have a look." | pending | `SpiritEject`, `SpiritDisappear` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The count of children taken goes up for each of the first two whether or not the player saw the exchange, so it is never heard after the second child | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The beam ends; he blows a raspberry at the child and, if the camera is within 150 and he is on screen, says "I take this life to feed the life of the many!" | pending | a life-draining priest animation is written in but commented out twice; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| When his animation ends the child dies, is let go from the script, and he looks pleased | pending | the health property (`SetProperty`) is real (`src/CHLApi.cpp:2029`); `ReleaseFromScript` works |
| After 6 seconds he resumes his rounds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Picking him up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The moment he is in the player's hand or the creature's (or dies), his rounds and his child-taking are stopped | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play; `StopScript` would stop neither here (see the quirks) |
| If he is alive he says "Put me down! The people need me for their eternal youthfulness." | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| Nothing more happens until he is put down or thrown | pending | the held checks are real (`GetProperty` in hand, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not reached in play |
| If thrown, a scene follows him through the air, camera position and focus both following him, until he lands | pending | `SetPositionFollow` and `SetFocusFollow` work; needs the held checks |
| Landing within 200 of the village centre, alive: "Kill me and you kill them all.", and his rounds start again | pending | `GetDistance` works; needs the held checks |
| Landing (or being put down) more than 200 from the village, alive, for the first time: "Banish me and they die." His rounds are not restarted; he resumes taking children from where he is | pending | the children walk to him, so later ones may walk out to him; the next child restarts his rounds |
| The second time he ends up more than 200 away (the two need not be in a row), he is banished for good (below) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If he is dead, he has been killed (below) | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Undetermined what happens if he is sacrificed or otherwise removed: the script reads his health and then checks he still exists, but reading the health of a thing that is gone is an error in the game | pending | reading the health of a thing that is gone is an error in the game |

## Killing or banishing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His rounds and child-taking are stopped (as written, see quirks) | pending | as written, see quirks |
| If the camera is within 100 of him and he is on screen, a scene starts: the camera is put just above him (5 up, 5 off each way) and pulls out to 20 over 6 seconds | partial 40% | setting the camera and its glide work (`SetCameraPosition`, `SetCameraFocus`, `MoveCameraPosition`); the scene never runs; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| Killed: he cries "Argh!" | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| Banished: he says "It's all over. I'm off. I don't care what happens to the people." and joins the Greek village with town id 2 (the one near Khazar the control script calls Town4 and, wrongly, Norse), then is let go to live as one of its villagers where he stands | pending | `GetTownWithId` is real; not checked in play; `FlockAttach`, `ReleaseFromScript` work |
| The screen fades to black over 2 seconds, the camera is cut to look over the healer's village, and fades back in over 2 seconds | partial 40% | the fades and camera cuts work (`SetFade`, `SetFadeIn`, `SetCameraPosition`, `SetCameraFocus`); the scene never runs; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| Every villager of the village dies on screen | pending | the health property on a town is real (`SetProperty` sets each member, `SetHealthByScript`, `src/CHLApi.cpp:1923`); not reached in play |
| Killed only: his voice says, after his death, "Kill me and you kill them all." | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |
| The good advisor: "Well, Leader, you stopped those nasty sacrifices." The scene waits until it is read and 10 seconds have passed | pending | `SpiritEject` and `RunText` work; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the camera was not near him or he was off screen, there is no scene: the village's villagers simply all die | pending | the health property on a town is real (`SetProperty` sets each member, `SetHealthByScript`, `src/CHLApi.cpp:1923`); not reached in play |
| The event then ends | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 3 minutes after the end, four Norse villagers appear at the healer's house: a farmer, two housewives and a forester, as the seed of a new village | pending | `Create` makes villagers; the script does not add them to any village (undetermined whether they join the village by themselves); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Leaving him alone, the village keeps its age-16 adults and loses a child about every half minute forever; nothing ever ends the event | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All the healer's lines are the "man" narrator voice; most are spoken as sounds at him rather than in the dialogue box ("While my heart beats...", "I take this life...", "Put me down!...", "Banish me...", "Kill me..." on landing); only the end scene's lines use the box | pending | `GamePlaySaySoundEffect` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Advisors appear only for the child-taking exchange (first two children) and the good advisor's closing line | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature picking the healer up counts the same as the player's hand: his child-taking stops, he complains, and where it drops or throws him decides whether he is safe, banished or killed | pending | `InCreatureHand` is real since 2026-10-21 |
| The creature killing him in any other way also ends the event with the "killed" scene and the village's death | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp:1596`); not reached in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The stop lines name both scripts in one string ("Walkabout, KillChild"); the game splits the name at spaces, commas and tabs and stops every script named, so both stop | pending | our `StopScript` (`src/CHLApi.cpp`) compares the whole string as one name and would stop neither; the Missionaries, the Lethys vortex and the land-four meteorites use the same form |
| The main loop's check for him being picked up is only made between children, not while he is taking one (by the script machine's rule, a script waiting on a called script doesn't run its own checks), so a child already being taken still dies even with the healer in the hand | partial 60% | our `LHVM` (`components/ScriptLibrary/src/LHVM.cpp`) follows the same rule; not separately confirmed in the original; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| Banishing him for good stops his rounds but not his age-keeping, so far away in his new village he keeps calling out "While my heart beats you will all never age!" every 3 minutes when the camera is near, and keeps setting the old village's (new) villagers to 16 | pending | the age script ends only on his death |
| If he is banished for good while off camera, he is neither added to the other village nor let go from the script | pending | those lines are inside the scene's branch |
| The first banishment is only a warning, and does not send him back: he stays where he landed until the next child restarts his rounds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A comment says the landing lines are "not entry is in camera mode follow"; the "Banish me and they die." line waits for a text to be read though it is played as a sound | pending | undetermined how long that wait lasts |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the healer, his banishment count and the count of children taken | pending | openblack has no saved games |
