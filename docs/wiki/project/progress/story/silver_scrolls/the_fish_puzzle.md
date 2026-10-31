# The Fish Puzzle

A silver scroll on the fourth land. A boy turtle farmer on the shore wants to be a fisherman like his father; the player
herds a shoal of fish into his net by tapping the water, and he gives the player his favourite turtle, a creature to
swap for.

**Land:** 4 · **Giver:** a boy turtle farmer on a shore some way from the Japanese village, his home · **Script:** FishPuzzle · **Reward:** a turtle (tortoise) creature to swap for, offered for as long as the land lasts · **Repeatable:** no (the swap itself can be made back and forth)

The land as a whole is in [../land_4.md](../land_4.md), the fish-herding game itself (the engine's puzzle) in
[../minigames.md](../minigames.md#fish-herding-land-4), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

Sources: the land's challenge script source (checked against the PC game's compiled `challenge.chl`), the game's text
table (`Scripts/InfoScript2.txt`) and the executable. openblack is judged on this tree: of the 59 script functions the
quest and the swap script it starts call, none is a stub in `src/CHLApi.cpp` (`CREATURE_DO_ACTION` runs every action these scripts
force, the swap's point-at-each-other, row 163, included), a script can make a
creature (`CreateScriptObject`, `src/CHLApi.cpp`), and Land 4's control script
never runs (the story reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked), so the boy never exists and every row below is pending unless the notes
say otherwise.

**Progress: 0/42 done, 0 chosen, 6 partial, 0 inferred — 8%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background right after the land's opening, with no other condition | pending | `LandControl4` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); see [../../scripts/land4_script.md](../../scripts/land4_script.md) |
| A Japanese farmer is made a few paces from his spot on the shore and walks to it; he is made a boy by setting his age to 11 | pending | `Create` makes villagers, `MoveGameThing` and the age property work for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A flock of five tortoises is made beside him (gathering within 5, roaming up to 10) | pending | `FlockCreate` and `ChangeInnerOuterProperties` work, `PopulateContainer` is real |
| A silver scroll appears on the shore next to him | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the boy dies before the scroll is clicked, the quest ends quietly: the scroll goes and nothing is said | pending | the health property is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cinematic (letterbox); the camera glides over three to four seconds to look down on the boy, who faces it | partial 40% | the letterbox, the camera glides and `SetFocus` on villagers work (`SetWidescreen`, `MoveCameraPosition`, `MoveCameraFocus`); the scene never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Brings the black cinema bars in or out", "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object to face a position", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| Boy: "I want to be a fisherman like my dad, instead of a turtle farmer." | pending | `RunText`, `TextRead` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The fish puzzle is made out in the bay | partial 60% | `CreateWithAngleAndScale` makes puzzle game 14 and its bait, net and two shoals (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`; hook `OPENBLACK_TEST_FISH_PUZZLE`), faithful per our wiki ([water](../../../../world/sea-life-and-scenery.md#fish-puzzle)); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Creates an object of a kind at a position with an angle and a scale", "Create makes a puzzle game", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| Boy: "My dad said I'd never be any good. Maybe he's right." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera cuts to look down on the puzzle | partial 40% | `SetCameraPosition`, `SetCameraFocus` work; the scene never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| Boy: "When you tap the water, the fish will swim away from your hand." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Boy: "When they are all in the net, I'll be able to catch them and make my old man proud." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The dialogue box is closed and the scroll is entered in the challenge log as "The Fish Puzzle", not yet done, with the good advisor's reminder "Try to help this person catch all the fish, by scaring them into the net."; the cinematic ends | pending | `GameCloseDialogue` works, `Snapshot` is real |

## Playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player taps the water to herd the fish away from the hand and into the boy's net; there is no time limit and no limit on tries | partial 80% | the herding works: the hand's splash scares the shoals and 30 fish in the net for 0.5 s close it (`ecs::UpdateFishShoals`, `src/ECS/FishShoals.cpp`; [water](../../../../world/sea-life-and-scenery.md#the-net-the-bait-and-the-shoals)); reached only through the test hook, as Land 4 never loads; finished by [fish.md](../../animal/fish.md) "A shoal's fish dart away from a splash", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| The quest waits until the puzzle reports it has been played to the end | partial 60% | `Played` on puzzle 14 answers once the net has closed (`IsPuzzleGamePlayed`, `src/ECS/PuzzleGames.cpp`), which settles "played" for this puzzle ([water](../../../../world/sea-life-and-scenery.md#the-script-side)); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether a living thing has finished the animation or state the script gave it", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| If the boy dies while the puzzle is being played, the good advisor says once: "Oh no. You've killed the fisherman. I despair." The puzzle goes on | pending | the health property is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the puzzle is done the scroll is marked done in the challenge log (success 1, alignment unchanged) whether or not the boy is alive | pending | `UpdateSnapshot` is real |
| If the boy lives: a cinematic with the reward sting; the camera glides to the boy, who faces it | pending | `PlaySoundEffect` and the camera natives work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Boy: "Yahoo! I caught the lot. My father will be so impressed." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A turtle (tortoise) creature is made beside him, made to stand idle and face the camera | pending | `Create` makes creatures now; not checked in play; standing idle (action 161 BeIdle) has an executor (`CreatureDoAction`, `src/Creature/CreaturePlanActions.cpp`) |
| Boy: "Here. Have my favourite turtle as thanks for all your help." as he and the camera turn to the turtle | pending | no turtle |
| A second later he walks off home to the Japanese village at speed 0.6; the cinematic ends | pending | `MoveGameThing` and the speed property work for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Evil advisor, pointing at the boy: "Hey, Boss. If anyone's a good fishermen around here it's you!" | pending | `SpiritEject`, `SpiritPointPos`, `RunText` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Good advisor: "Well, yes, but he's happy and that's what counts." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Good advisor: "We don't need recognition for our fishing abilities, do we?" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The silver scroll goes | pending | `ObjectDelete` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the boy is killed on his walk home (before he is within 20 of the village), the evil advisor says once: "Heh. From fisherman hero to corpse in one easy move. Neat, Boss." | pending | the health property is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |
| If the boy was already dead when the puzzle ended, the scroll goes and the evil advisor says: "Oh boy. You caught them all. Sweet.", "Let's go see the fisherm... Oh. We killed him. I forgot. Heh." and "Still, we rock at fishing. Any fish-gods out there better watch out!"; there is no turtle | pending | the health property is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |

## The reward: swapping to the turtle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The turtle waits with a silver scroll above its head, cannot be picked up, and keeps turning to look at the camera | pending | no turtle; the shared swap script, see [creature_swaps.md](creature_swaps.md); looking at the camera (action 82 LookAtCamera) has an executor |
| When the camera is within 100 of it and it is on screen, the evil advisor points at it every 60 seconds: "Swap your Creature with this one if you want." | pending | no turtle |
| Clicking the scroll or the turtle with the player's creature more than 50 away: "You'll need to bring our Creature, Boss." | pending | no turtle |
| With the creature close, it is taken off its leash and walked over to the turtle (up to 5 seconds), and the two look each other over | partial 40% | taking the leash off works (`DetachObjectLeash`); the walk is `MoveGameThing`, which walks a creature under the script's control (`src/CHLApi.cpp`), and the look-over (action 20 ExamineByLooking) has an executor; there is no turtle in play yet; finished by [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Takes a creature's leash off", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", [land4_script.md](../../scripts/land4_script.md) "The story moves on to this land when Land 3 is finished"; still open: no quest run's PASS is cited for it |
| Every 30 seconds the good advisor asks: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel."; clicking the turtle confirms, clicking the player's own creature or waiting 110 seconds cancels | pending | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining`, `ClearClickedObject` work; no turtle |
| Confirming: a cinematic with a two-creature camera; sparkles on both, the creatures point at each other, the swap is made with its sound, and three seconds after both animations end it closes | pending | `StartDualCamera` works, `SwapCreature` is real; pointing at each other (action 163 PointAtObject) has an executor now in `src/Creature/CreaturePlanActions.cpp`; no turtle in play yet; how the swap carries the mind over: see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." | pending | `SwapCreature` is real (`src/CHLApi.cpp`); `RunText` works |
| The old creature now waits in the turtle's place to be swapped back, and so on for ever: the offer never ends | pending | the swap script's timer is made but never checked |

## Aftermath, sound and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The boy's five tortoises stay on the shore | pending | `PopulateContainer` is real (`src/CHLApi.cpp`); not reached in play |
| No music is started; the only sound is the reward sting on success | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest never changes alignment; the alignment changes the source once had for killing the boy are commented out | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The source notes that two of the boy's instruction lines were meant to be spoken by another voice; in the text table both are the boy's own lines, so they play as the boy | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll is marked done before the game checks whether the boy lived, so killing him still completes the scroll | pending | `UpdateSnapshot` is real |
| The script only waits for the boy to reach home; it never removes the puzzle | pending | undetermined whether the puzzle removes itself once played (ours keeps it: `ProcessPuzzleGame` stops once played) |
