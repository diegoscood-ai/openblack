# The Greedy Farmer

A silver scroll on land 2: a Celtic farmer asks the player to stop a gang of five hungry children from stealing his
herd of ten cows, which they lead off one at a time to a campfire hideout and slaughter. The quest ends
when the farmer dies, the herd is gone, the children are all dead, or a child is killed beside the gang and the rest
lose their nerve; three of those endings strengthen the local town's Lightning miracle. The land as a whole is in
[../land_2.md](../land_2.md), the land's control script in [../../scripts/land2_script.md](../../scripts/land2_script.md),
the challenge program in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Land:** 2 · **Giver:** a Celtic farmer at his house on the edge of the Celtic town (town id 4) · **Script:** GreedyFarmer · **Reward:** the Lightning miracle and its second power-up level put into the local town (not for the "all cows gone" ending) · **Repeatable:** no

Sources: the quest's original script source (`GreedyFarmer.txt`, ten scripts) and the land's trigger in
`LandControl2.txt`, both checked line by line against the PC game's compiled `challenge.chl`; the shared helpers it runs
(the scroll notify loop and the standard reminder); the game's English text table for every line quoted; the executable
(read-only) for what putting a miracle in a town does. openblack's state is judged on this tree: of the 62 script
functions this quest and its helpers call, none is a stub in `src/CHLApi.cpp` at this tree (the flock-member test,
the held-by-creature check, the health read and the snapshots are real), and `CREATE` makes the cows, children, farmer and
markers (`CreateScriptObject`), but the land's control script never runs (see [../land_2.md](../land_2.md) row 1). The
quest therefore never appears; every row below is pending unless the notes say otherwise.

**Progress: 0/143 done, 0 chosen, 4 partial, 0 inferred — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts a background check on the Celtic town with id 4 (the source comment on it reads "Celtic - Greedy Farmer") | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../land_2.md](../land_2.md) |
| The check waits 8 minutes, then looks whether the town belongs to the player; if not, it waits another 8 minutes and looks again, for ever, so the quest can only start on an 8-minute beat after the player has won the town | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; town ownership read is in the land script; the wait is the script VM's sleep |
| Once the town is the player's the quest starts in the background and the check stops; it starts only once | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nothing is made before that: the herd, the children and the farmer all appear only after the town is won | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A campfire (a bonfire object) is lit at the children's hideout, on low ground about 180 from the farmer's house as soon as the quest starts | pending | `CREATE` makes a bonfire (`CreateScriptObject` in `src/CHLApi.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Ten cows are made one at a time in the farmer's field, each within 2 of the field's centre, and each only at a moment when the field is not on screen, so the player never sees them pop in | pending | `CREATE` makes animals and `POS_FIELD_OF_VIEW` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The cows form a herd that keeps its members within an inner radius of 5 and an outer radius of 30, and are set to move as a herd; 2 seconds later they start grazing | pending | `FlockCreate`, `FlockAttach`, `ChangeInnerOuterProperties` are real; `SetScriptState` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each cow gets its own watcher script from the moment it is made (see "Saving cows") | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The script then waits until the hideout is off screen and makes five boys there, Celtic farmer children, each set to age 10, and puts them into a gang (a flock with inner radius 3, outer radius 5) | pending | `CREATE` makes villagers and `SET_PROPERTY` age works for villagers; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The children start stealing at once, before the scroll is even shown (see "The cattle thieves") | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A silver challenge scroll is then put up at the farmer's house | pending | `CreateHighlight` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| While the scroll waits, whenever the camera is within 100 of it and it is on screen, at most once every 30 seconds and only when no cut scene is running, the evil advisor steps out, points at the scroll and says "Hey. There's a job for you. Wanna do it?" | pending | `SpiritEject`, `SpiritPointPos`, `RunText` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scroll waits until the player clicks the scroll or the farmer's house itself; the scroll is then made active | pending | `GameThingClicked` is real; `SetActive` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest's main script waits on the scroll and does not watch for the endings meanwhile, so the children can steal the whole herd before the player ever clicks; the "all cows gone" ending then plays straight after the introduction | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: the notify helper is run as a blocking script before the main watch loop |
| The farmer himself is made at his house only when the scroll is clicked | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking starts a cut scene (widescreen, the camera and dialogue taken from the player, the game at normal speed) with the generic script music, number 2 | pending | `StartCameraControl`, `StartDialogue`, `StartGameSpeed`, `SetWidescreen`, `StartMusic` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The farmer is drawn in high detail for the scene | pending | `SetHighGraphicsDetail` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera's starting view is remembered so it can be given back | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The farmer walks from his house to his soap box, a spot beside it, while the camera glides over 3 seconds to a low view of the soap box | pending | `MoveCameraPosition`, `MoveCameraFocus` are real; `MoveGameThing` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scene waits until the farmer is within 3 of the soap box and the camera has arrived; he then turns to face the camera and plays his idle animation on a loop | pending | `SetFocus`, `SetScriptUlong`, `SetScriptState` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After 1 second the camera drifts over 5 seconds to a closer view of him | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The quest is recorded in the player's challenge log: a snapshot with the title "The Greedy Farmer", success 0 and alignment 0, the current camera view, and a reminder that replays the good advisor's line "Those hungry children are stealing the farmer's cattle Shall we get involved?" | pending | `Snapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer: "Excuse me powerful being. I need your help." | pending | `RunText`, `TextRead` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer: "Children have been stealing my cattle." He plays a disgruntled crowd animation once | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| 2 seconds into that line the camera cuts to the gang of children, 13 to the side and 9 above them, and follows them | pending | `SetFocusFollow`, `SetCameraPosition`, `SetCameraFocus` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer, over the shot of the children: "Stop it you little thieves!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After 3 seconds the camera cuts back to the close view of the farmer | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He goes back to his idle animation on a loop | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer: "Please, you must act to stop them." This line waits for the player to click on before going on | pending | `RunText` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera glides back to the player's starting view over 2 seconds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The farmer is set to wander around his soap box (with the script's wander values 6, 4 and 20) and goes back to normal detail; the music stops and the cut scene ends, keeping the dialogue | pending | `SetScriptUlong` partly ported; `SetScriptStatePos`, `SetScriptFloat` and the wander round a place (state 199) are real (checked against v1.42 on 2026-10-20); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Both advisors step out and the dialogue is cleared | pending | `GameClearDialogue` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor: "Kill someone. The rich farmer dude?" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor: "Think of the poor hungry children." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor: "I am. " | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the text table line ends in a space |
| The dialogue then ends; neither advisor is sent home here | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The herd

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The herd grazes the field: the script picks a random spot within 20 (each way) of where the herd first stood, retrying until the spot is at least 20 from the herd's current position, and sends the herd there | pending | `RANDOM` is real; `MoveGameThing` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| It then waits 300 seconds plus a random 0 to 300 before the next move; the random part is drawn afresh on every check of the wait (see quirks) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The grazing stops when the herd is empty or the quest has ended | pending | `IdSize` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The cattle thieves

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The gang is slowed to speed 0.4 and walks to where the herd is at that moment, waiting until it is within 5 of that spot | pending | `SET_PROPERTY` speed is ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cow is taken out of the herd and one boy of the gang is picked | pending | `FlockDetach`, `CallIn` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The cow is slowed to speed 0.4 and told to stand where it is; the boy walks up to it until he is within 5 | pending | `SET_PROPERTY` speed and `MOVE_GAME_THING` on animals and villagers are ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The cow is then put into the gang as its leader; cow and boy both walk to the gang and the script waits until both are within 10 of it | pending | `FlockAttach` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The cow is sent to the hideout at speed 0.4, cow and boy are set to move as a flock, and the gang walks to the hideout | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The script waits until the gang is within 10 of the hideout and the cow within 1 of it; a cow-slaughter sound plays at the cow, it is taken out of the gang and deleted with a fade | pending | `PlaySoundEffect` is real; `ObjectDelete` partly ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A count of slaughtered cows goes up by one (the count is never used; see cut parts) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The theft is abandoned at once if the cow or the boy stops existing, or if the cow is picked up by the player's hand or is in the player's creature's hand; the gang then goes back for another cow | pending | `GET_PROPERTY` in hand is real; `InCreatureHand` is real since 2026-10-21; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The gang keeps stealing one cow after another until the quest ends; if no cow or no boy can be had it simply tries again | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Saving cows

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever a cow is picked up by the hand or held by the player's creature, its watcher waits until it is let go, then puts it back into the herd at speed 0.4, so a dropped cow walks back to the herd | pending | `FlockAttach` and `GET_PROPERTY` in hand are real; `InCreatureHand` is real since 2026-10-21; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cow counts as stolen once it is in the gang | pending | `FlockMember` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A stolen cow that ends up back in the herd counts as saved: the good advisor steps out and says "You saved a cow. The Farmer will be pleased." and goes home | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The first cow saved adds 0.4 to the quest's alignment total; later saves only repeat the line | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cow's watcher stops when the cow's health reaches 0 or the quest ends | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## The angry farmer

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the farmer watches for the gang: when it comes within 40 of him he goes after it | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the camera is within 100 of him, he is on screen and no cut scene is starting, he shouts one of two lines picked at random: "Oi! Them's my cows!" or "Stop it you little thieves!" | pending | `RandomUlong`, `RunText` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He chases the gang until he is 20 or more from his soap box or within 10 of the gang | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Then, on the same camera conditions, he shouts "Come back with my cattle you swines!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He walks back to his soap box and waits until he is within 10 of it | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| He chases again only after the gang has been more than 40 from him | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The chasing stops when he dies, the quest ends or the herd is empty | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Endings: what is checked

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the main script watches the quest every 1.1 seconds | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Each boy has his own watcher, checking every 1.1 seconds; a dead boy (health 0 or less) still in the gang is taken out of it | pending | `FlockDetach` is real; `GET_PROPERTY` health is real (`src/CHLApi.cpp:1596`); `FlockMember` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| If the dead boy is not flying through the air and lies within 10 of the gang, the gang is marked as scared and he is remembered as the dead boy | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A boy killed more than 10 from the gang, or while flying, only leaves the gang; the next check can still find him near the gang once he has landed | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: the check repeats every 1.1 s for that boy until a scare is marked or the quest ends |
| The cow being led is counted in the gang, so the main script allows for it: if the gang holds a cow and nothing else, every boy is gone | pending | `CallIn`, `IdSize` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The first time the gang is smaller than five boys (plus the led cow), the evil advisor steps out: "Bit of killing. Nice one." waits 2 seconds and goes home; this is said once | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; it is also said when a boy leaves the gang for any other reason |
| The endings are tested in this order, the first that holds wins: the farmer's health is 0 or less; the herd is empty; the gang is empty or holds only a cow; the gang is scared | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: the order of the end conditions in the compiled watch loop |
| When an ending fires, any cut scene, game speed, dialogue and camera control running in the watch loop are ended at once, so the evil advisor's "Nice one" can be cut off | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: each end condition's handler closes widescreen, game speed, dialogue and camera control |

## Ending: the farmer is killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Good advisor steps out: "The farmer's dead. Outrageous." and goes home | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| 0.6 is taken off the alignment total (total −0.6, or −0.2 if a cow was saved first) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry is updated: success 1.0, that alignment, the same title and reminder | pending | `UpdateSnapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The reward is given (see Reward) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Who killed him does not matter (the player, the creature, a miracle or Khazar's or Lethys's side) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: the test is only his health |

## Ending: the herd is gone

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when the herd's size is 0, whether the children stole every cow or the player, the creature or anything else killed or removed them | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The farmer walks to his soap box; the script waits until he is within 3 of it, then until the camera is within 100 of the soap box with it on screen (it waits for ever if the player never looks) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cut scene glides the camera over 3 seconds to a view of the soap box | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The log entry is updated: success 1.0 with the alignment total unchanged (0, or +0.4 if a cow was saved) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer: "All my cows are gone. You're no god of mine!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After the line is read and 1 more second, the camera glides back to the player's view over 2 seconds and the scene ends | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| No reward is given for this ending | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: it is the only ending that does not set the reward flag |

## Ending: all the children are killed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when the gang is empty or holds only the cow it was leading; in practice when each boy dies away from the gang (a death beside it scares the others first) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The script waits until the camera is within 100 of the soap box and it is on screen | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cut scene cuts the camera straight to the soap box view; the farmer is sent walking to the soap box | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor steps out: "Way to go, Boss. You killed all the children." and goes home | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scene waits until the farmer is within 2 of the soap box, then 1 second; Farmer: "I wanted you to stop them stealing the cows, not kill them all!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera cuts back to the player's view and the scene ends | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| 1.0 is taken off the alignment total (total −1.0, or −0.6 if a cow was saved), and the log entry is updated with success 1.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The reward is given | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Ending: the children are scared off

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reached when a boy dies beside the gang (see the checks above); no camera condition, the scene starts wherever the player is looking | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A cut scene starts; each living boy other than the dead one walks to stand around the body (2 in front, 2 behind, 2 to either side, and one diagonally behind) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The camera glides over 6 seconds to a view 10 to the side and 10 above a boy of the gang | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; which boy is the one found in the gang as the scene starts |
| The gang turns to look at the body; the camera glides over 6 seconds to a closer view (8 to the side, 5 above) | pending | `SET_FOCUS` on a flock (a script container) is ported; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A boy: "I'm scared. I'm not stealing cows any more." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A boy: "Me neither. I'm off." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The dialogue box is closed and, a tenth of a second later, a new snapshot is taken (not an update): success 1.0, the alignment total unchanged (0, or +0.4 if a cow was saved) | pending | `GameCloseDialogue` is real; `Snapshot` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scene ends and the gang is broken up, the boys going their own way | pending | `FlockDisband` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The reward is given | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Alignment and success values

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The snapshot records the quest in the player's challenge log with its title, a success fraction, an alignment value, a camera view and a reminder script; later updates change the success and alignment of that entry | pending | `Snapshot` and `UpdateSnapshot` are real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Introduction: success 0.0, alignment 0.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The total starts at 0 and only one thing raises it: the first cow saved, +0.4 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Farmer killed: −0.6 → −0.6 (−0.2 with a saved cow), success 1.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Herd gone: no change → 0 (+0.4 with a saved cow), success 1.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Children all killed: −1.0 → −1.0 (−0.6 with a saved cow), success 1.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Children scared off: no change → 0 (+0.4 with a saved cow), success 1.0 | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Every ending counts as full success (1.0), including losing the whole herd | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whatever the ending, any cows left are handed over to the local town (the town within 100 of a point about 60 from the farmer's house) | pending | `FlockAttach` is real; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| For the farmer-killed, children-killed and scared endings, the Lightning miracle and its second power-up level are added to that town's miracles (the source comment calls it "a lightning bolt PU 1 for the LocalTown") | partial 60% | `SET_MAGIC_IN_OBJECT` puts a miracle in a town (`src/Magic/Script/CHLWorship.cpp`, `worship::town::AddMagicTypesHeld`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [../rewards.md](../rewards.md); re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| A cut scene flies the camera over 3 seconds to the town centre | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The evil advisor steps out: "The Lightning Bolt just got better." The dialogue then ends; the advisor is not sent home by the script and the camera is not moved back | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The local town is found by position, not by id; that it is the land script's town id 4 is inferred from the land script's comment and the positions, not checked | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The farmer, if alive, keeps wandering around his soap box; nothing removes him | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| After the scare the boys stay in the world as ordinary children; after the other endings the gang simply stops stealing | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the gang is broken up only in the scare ending |
| The campfire at the hideout stays | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Nothing in the land script reads this quest's result | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; evidence: no other script uses the quest's finished flag |

## Advisors' comments

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Evil advisor (scroll notify): "Hey. There's a job for you. Wanna do it?" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor (reminder): "Those hungry children are stealing the farmer's cattle Shall we get involved?" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; the line has no full stop between its two sentences in the text table |
| The intro exchange: evil "Kill someone. The rich farmer dude?", good "Think of the poor hungry children.", evil "I am. " | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor on a saved cow: "You saved a cow. The Farmer will be pleased." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor on the first child lost: "Bit of killing. Nice one." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Good advisor on the farmer's death: "The farmer's dead. Outrageous." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor on all the children dead: "Way to go, Boss. You killed all the children." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor on the reward: "The Lightning Bolt just got better." | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the generic script music number 2 and stops it at the end of the scene | partial 60% | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio/GameMusic.cpp`); the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| Each slaughtered cow plays the script cow-slaughter sound at the hideout | partial 60% | `PLAY_SOUND_EFFECT` works; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| No other music or sound is started by the quest | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cow in the player's creature's hand counts like one in the player's hand: the theft is dropped and the cow goes back to the herd when released | pending | `InCreatureHand` is real since 2026-10-21; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The creature killing the farmer, the children or the cows leads to the same endings as the player doing it; the script never asks who did it | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The grazing wait's random part is re-drawn on every check, so the wait ends as soon as the elapsed time beats a fresh draw; it ends a little after 5 minutes rather than anywhere from 5 to 10 | partial 60% | our VM's sleep (`Opcode21Sleep`, `components/ScriptLibrary/src/LHVM.cpp`) re-checks the condition each pass the same way; the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; re-measured 2026-10-21: the script runs on natives that work; a run has not reached it yet |
| The quest's alignment total is a global that is never reset; harmless because the quest runs once | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The farmer's first idle loop is given a count of −11 where the later one uses −1, apparently a typo; its effect is undetermined | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A "Nice one" comment is also triggered by a boy leaving the gang for reasons other than death | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The "herd gone" and "children killed" scenes wait for the camera to come to the soap box with no time limit, so the reward and the end can be put off for ever | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The scare ending writes a second, new snapshot instead of updating the first | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Source comments describe other lines than those used: "Gutted you killed the farmer", "Oi! Stop nicking me cows you little rascals!", "I'm too old for this shit!", "You stoopid fugger. I wanted you to stop them stealing the cows, not kill them!!", "The children are gone! Problem solved.", "The cows are all gone, dispute solved!" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; placeholder comments; the shipped text is quoted in the rows above |
| A comment on the herd-gone ending says it "should make the Farmer request you to click"; no such click is asked for | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The introduction's comment calls them children "from the village", but the boys are made at the hideout by the script, not taken from the town | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Cut start condition: the comments say the children would appear only when the town's storage pit ran alarmingly low, and the scroll only after at least one cow had been stolen; the storage pit is looked up and a food and a starving counter set up, but none is ever used, and the wait for the first stolen cow is commented out | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The hideout house is looked up but never used; the slaughtered-cow count is kept but never read; an unused local alignment total and a commented-out debug size exist | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A commented-out wander-around state for the children, and a commented-out condition that a cow only counts as saved once more than 50 from the gang | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| A stand-alone test challenge that just runs the quest exists in the sources but is not in the compiled game | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| Unused lines in the text table: good advisor "Someone is praying to you here."; evil advisor "We should punish someone."; good advisor "But those children are hungry!" (an alternative advisor exchange) | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; no script uses them |
| Unused boys' lines: "Well I'm still going to. I'm hungry."; "He's dead! We should stop this. It's gone too far."; "Let's get back to the Village."; and "I'm scared. I don't wanna do this any more." (the scare scene's comment quotes this one, but the script uses "I'm scared. I'm not stealing cows any more.") | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; suggests a cut ending where the farmer's death ends the thieving and one boy refuses to stop |
| Two filler lines under the quest's prefix: "Sending synchronised map file" and "NOT USED" | pending | the quest has not yet started in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
