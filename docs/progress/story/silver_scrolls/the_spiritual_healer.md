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
the 48 script functions the event's scripts need, 3 only log "not implemented" in `src/CHLApi.cpp`, among them the town
look-up and a special effect; creating villagers works (`Create`). The land's control script never runs (see
[../land_2.md](../land_2.md) and [../../scripts/land2_script.md](../../scripts/land2_script.md)), so none of it happens;
rows are todo unless the notes say otherwise. Function coverage is in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/55 done, 3 partial — 3%**

## What it is

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is a background event, not a silver scroll: no scroll stands anywhere, nothing has to be clicked, and no record, reminder or alignment is written to the challenge log | n/a | checked in the source and the compiled program: no highlight, snapshot or title |
| It has no reward and no alignment set by the script; whatever alignment the player gains or loses comes from the deaths and acts themselves | todo | undetermined how much the game's own rules move alignment for these deaths; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a watcher on the village with town id 9 when the land is set up | todo | `LandControl2` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)) |
| Every 1.3 seconds the watcher checks whether the village belongs to the player; the moment it does, the event starts and the watcher stops | todo | reading a town's owner (`GetProperty` player) is not ported |
| The village is Greek: the land's map script makes town id 9 a Greek village; the control script's comment calls it Norse, and the villagers made at the end are Norse ones | n/a | naming only |
| The healer is made as a priest villager at a house by the village centre; he joins no village and has no special protection: he can be picked up, thrown and killed | todo | `Create` makes villagers; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| Two background scripts start with him: his walk round the village and his keeping of the villagers' age | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## The healer's rounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| He walks a loop of five spots round the village (first, second, fourth, third, fifth), stopping at each to look for something or stand idle, then starts again | todo | `MoveGameThing` and `Played` work for villagers; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| Each walk ends when he is within 5 of the spot or after 30 seconds; each animation ends when played or after 30 seconds | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| At each stop he waits while he is in the player's hand or the creature's | todo | `InCreatureHand` is a stub and the in-hand property is not ported |
| The rounds end only when he dies | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## Keeping the village young

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3 minutes every villager of the village is set to age 16 | todo | setting a property on a town (`SetProperty` age on a container) is not ported; in the game it applies to each of its villagers |
| At the same time, unless he is in the player's or the creature's hand, a sparkle of success shows on him for 3 seconds | todo | `SpecialEffectPosition` works; needs the held checks (stubs) |
| At the same time, if the camera is within 100 of him and he is on screen, he calls out "While my heart beats you will all never age!" | todo | `GamePlaySaySoundEffect` and `GameThingFieldOfView` work; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| This keeps going until he dies; banishing him does not stop it | todo | see the quirks below |
| Undetermined whether age 16 makes the village's children grow up, or what the game counts as a child for the healer's choice | todo | not settled by the scripts |

## Taking the children

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 30 seconds he looks for a child of the village; if there is one, he takes it | todo | `Call` works; undetermined which child is picked; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| He stops his rounds and stands where he is; the child leaves the village | todo | `StopScript` and `FlockDetach` work; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| He faces the child and beckons, and a magic beam runs from him to the child (lasting up to 20 seconds) | todo | `SpecialEffectObject` works, `AddSpotVisualTargetObject` is a stub |
| The child walks to him, until within 1.5 or for 30 seconds, and faces him | todo | `MoveGameThing` and `SetFocus` work for villagers; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| For the first two children he takes, if the camera is within 150, he is on screen and no scene is playing: the good advisor pops out, "Oh, I don't think I want to see this!", and vanishes; the evil advisor pops out: "I do. Let me have a look." | todo | `SpiritEject`, `SpiritDisappear` work; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| The count of children taken goes up for each of the first two whether or not the player saw the exchange, so it is never heard after the second child | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| The beam ends; he blows a raspberry at the child and, if the camera is within 150 and he is on screen, says "I take this life to feed the life of the many!" | todo | a life-draining priest animation is written in but commented out twice; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| When his animation ends the child dies, is let go from the script, and he looks pleased | todo | the health property (`SetProperty`) is not ported; `ReleaseFromScript` works |
| After 6 seconds he resumes his rounds | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## Picking him up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The moment he is in the player's hand or the creature's (or dies), his rounds and his child-taking are stopped | todo | needs the held checks (stubs); `StopScript` would stop neither here (see the quirks) |
| If he is alive he says "Put me down! The people need me for their eternal youthfulness." | todo | needs the held checks (stubs) |
| Nothing more happens until he is put down or thrown | todo | needs the held checks (stubs) |
| If thrown, a scene follows him through the air, camera position and focus both following him, until he lands | todo | `SetPositionFollow` and `SetFocusFollow` work; needs the held checks |
| Landing within 200 of the village centre, alive: "Kill me and you kill them all.", and his rounds start again | todo | `GetDistance` works; needs the held checks |
| Landing (or being put down) more than 200 from the village, alive, for the first time: "Banish me and they die." His rounds are not restarted; he resumes taking children from where he is | todo | the children walk to him, so later ones may walk out to him; the next child restarts his rounds |
| The second time he ends up more than 200 away (the two need not be in a row), he is banished for good (below) | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| If he is dead, he has been killed (below) | todo | the health read is not ported |
| Undetermined what happens if he is sacrificed or otherwise removed: the script reads his health and then checks he still exists, but reading the health of a thing that is gone is an error in the game | todo | reading the health of a thing that is gone is an error in the game |

## Killing or banishing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| His rounds and child-taking are stopped (as written, see quirks) | todo | as written, see quirks |
| If the camera is within 100 of him and he is on screen, a scene starts: the camera is put just above him (5 up, 5 off each way) and pulls out to 20 over 6 seconds | partial | setting the camera and its glide work (`SetCameraPosition`, `SetCameraFocus`, `MoveCameraPosition`); the scene never runs |
| Killed: he cries "Argh!" | todo | needs the health read |
| Banished: he says "It's all over. I'm off. I don't care what happens to the people." and joins the Greek village with town id 2 (the one near Khazar the control script calls Town4 and, wrongly, Norse), then is let go to live as one of its villagers where he stands | todo | `GetTownWithId` is a stub; `FlockAttach`, `ReleaseFromScript` work |
| The screen fades to black over 2 seconds, the camera is cut to look over the healer's village, and fades back in over 2 seconds | partial | the fades and camera cuts work (`SetFade`, `SetFadeIn`, `SetCameraPosition`, `SetCameraFocus`); the scene never runs |
| Every villager of the village dies on screen | todo | the health property on a town is not ported |
| Killed only: his voice says, after his death, "Kill me and you kill them all." | todo | needs the health read |
| The good advisor: "Well, Leader, you stopped those nasty sacrifices." The scene waits until it is read and 10 seconds have passed | todo | `SpiritEject` and `RunText` work; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| If the camera was not near him or he was off screen, there is no scene: the village's villagers simply all die | todo | the health property on a town is not ported |
| The event then ends | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| 3 minutes after the end, four Norse villagers appear at the healer's house: a farmer, two housewives and a forester, as the seed of a new village | todo | `Create` makes villagers; the script does not add them to any village (undetermined whether they join the village by themselves); the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| Leaving him alone, the village keeps its age-16 adults and loses a child about every half minute forever; nothing ever ends the event | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All the healer's lines are the "man" narrator voice; most are spoken as sounds at him rather than in the dialogue box ("While my heart beats...", "I take this life...", "Put me down!...", "Banish me...", "Kill me..." on landing); only the end scene's lines use the box | todo | `GamePlaySaySoundEffect` works; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| Advisors appear only for the child-taking exchange (first two children) and the good advisor's closing line | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| No music is played | n/a |  |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature picking the healer up counts the same as the player's hand: his child-taking stops, he complains, and where it drops or throws him decides whether he is safe, banished or killed | todo | `InCreatureHand` is a stub |
| The creature killing him in any other way also ends the event with the "killed" scene and the village's death | todo | needs the health read |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The stop lines name both scripts in one string ("Walkabout, KillChild"); the game splits the name at spaces, commas and tabs and stops every script named, so both stop | todo | our `StopScript` (`src/CHLApi.cpp`) compares the whole string as one name and would stop neither; the Missionaries, the Lethys vortex and the land-four meteorites use the same form |
| The main loop's check for him being picked up is only made between children, not while he is taking one (by the script machine's rule, a script waiting on a called script doesn't run its own checks), so a child already being taken still dies even with the healer in the hand | partial | our `LHVM` (`components/ScriptLibrary/src/LHVM.cpp`) follows the same rule; not separately confirmed in the original; the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| Banishing him for good stops his rounds but not his age-keeping, so far away in his new village he keeps calling out "While my heart beats you will all never age!" every 3 minutes when the camera is near, and keeps setting the old village's (new) villagers to 16 | todo | the age script ends only on his death |
| If he is banished for good while off camera, he is neither added to the other village nor let go from the script | todo | those lines are inside the scene's branch |
| The first banishment is only a warning, and does not send him back: he stays where he landed until the next child restarts his rounds | todo | the quest never starts: no Land 2 (`LOAD_MAP` is empty) |
| The script reads its village by id itself and ignores the village the watcher passes it | n/a |  |
| A comment says the landing lines are "not entry is in camera mode follow"; the "Banish me and they die." line waits for a text to be read though it is played as a sound | todo | undetermined how long that wait lasts |
| His spoken lines read slightly differently from the developers' comments (e.g. "You kill me, you kill them all.") | n/a | comments only |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text table holds only the lines numbered 15 to 24 for him, all used; lines 1 to 14 are missing from the table and from every script, so an earlier version's lines were cut entirely | n/a |  |
| There is no scroll title for it in the text table; the nearest unused-looking title, "The Rejuvenator", belongs to the ape-swap scroll | n/a | checked: that title is used by the ape-swap script |
| A developers' test script starts the event on its own | n/a | not started by the game |
| A saved game keeps the healer, his banishment count and the count of children taken | todo | openblack has no saved games |
