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
quest and the swap script it starts call, 6 only log "not implemented" in `src/CHLApi.cpp`, a script cannot make a
creature (`CreateScriptObject` makes villagers, animals and puzzle games, but no creatures), and Land 4's control script
never runs (the land-loading command does nothing), so the boy never exists and every row below is todo unless the notes
say otherwise.

**Progress: 0/43 done, 6 partial — 7%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background right after the land's opening, with no other condition | todo | `LandControl4` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); see [../../scripts/land4_script.md](../../scripts/land4_script.md) |
| A Japanese farmer is made a few paces from his spot on the shore and walks to it; he is made a boy by setting his age to 11 | todo | `Create` makes villagers, `MoveGameThing` and the age property work for villagers; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| A flock of five tortoises is made beside him (gathering within 5, roaming up to 10) | todo | `FlockCreate` and `ChangeInnerOuterProperties` work, `PopulateContainer` is a stub |
| A silver scroll appears on the shore next to him | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out every 30 seconds or more, points at it and says "Look. Something for you to do here." | todo | the shared notify script's natives work; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| If the boy dies before the scroll is clicked, the quest ends quietly: the scroll goes and nothing is said | todo | the health property (`GetProperty`) is not ported |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll starts a cinematic (letterbox); the camera glides over three to four seconds to look down on the boy, who faces it | partial | the letterbox, the camera glides and `SetFocus` on villagers work (`SetWidescreen`, `MoveCameraPosition`, `MoveCameraFocus`); the scene never runs |
| Boy: "I want to be a fisherman like my dad, instead of a turtle farmer." | todo | `RunText`, `TextRead` work; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The fish puzzle is made out in the bay | partial | `CreateWithAngleAndScale` makes puzzle game 14 and its bait, net and two shoals (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`; hook `OPENBLACK_TEST_FISH_PUZZLE`), faithful per our wiki ([water](../../../bw1-notes/water.md#fish-puzzle)); the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Boy: "My dad said I'd never be any good. Maybe he's right." | todo | `RunText` works; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The camera cuts to look down on the puzzle | partial | `SetCameraPosition`, `SetCameraFocus` work; the scene never runs |
| Boy: "When you tap the water, the fish will swim away from your hand." | todo | `RunText` works; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Boy: "When they are all in the net, I'll be able to catch them and make my old man proud." | todo | `RunText` works; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The dialogue box is closed and the scroll is entered in the challenge log as "The Fish Puzzle", not yet done, with the good advisor's reminder "Try to help this person catch all the fish, by scaring them into the net."; the cinematic ends | todo | `GameCloseDialogue` works, `Snapshot` is a stub |

## Playing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player taps the water to herd the fish away from the hand and into the boy's net; there is no time limit and no limit on tries | partial | the herding works: the hand's splash scares the shoals and 30 fish in the net for 0.5 s close it (`ecs::UpdateFishShoals`, `src/ECS/FishShoals.cpp`; [water](../../../bw1-notes/water.md#the-net-the-bait-and-the-shoals)); reached only through the test hook, as Land 4 never loads |
| The quest waits until the puzzle reports it has been played to the end | partial | `Played` on puzzle 14 answers once the net has closed (`IsPuzzleGamePlayed`, `src/ECS/PuzzleGames.cpp`), which settles "played" for this puzzle ([water](../../../bw1-notes/water.md#the-script-side-puzzlegame-14-and-played)); the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| If the boy dies while the puzzle is being played, the good advisor says once: "Oh no. You've killed the fisherman. I despair." The puzzle goes on | todo | the health property is not ported |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the puzzle is done the scroll is marked done in the challenge log (success 1, alignment unchanged) whether or not the boy is alive | todo | `UpdateSnapshot` is a stub |
| If the boy lives: a cinematic with the reward sting; the camera glides to the boy, who faces it | todo | `PlaySoundEffect` and the camera natives work; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Boy: "Yahoo! I caught the lot. My father will be so impressed." | todo | `RunText` works; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| A turtle (tortoise) creature is made beside him, made to stand idle and face the camera | todo | `Create` makes no creatures; `CreatureDoAction` is a stub |
| Boy: "Here. Have my favourite turtle as thanks for all your help." as he and the camera turn to the turtle | todo | no turtle |
| A second later he walks off home to the Japanese village at speed 0.6; the cinematic ends | todo | `MoveGameThing` and the speed property work for villagers; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Evil advisor, pointing at the boy: "Hey, Boss. If anyone's a good fishermen around here it's you!" | todo | `SpiritEject`, `SpiritPointPos`, `RunText` work; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Good advisor: "Well, yes, but he's happy and that's what counts." | todo | the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| Good advisor: "We don't need recognition for our fishing abilities, do we?" | todo | the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The silver scroll goes | todo | `ObjectDelete` works; the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| If the boy is killed on his walk home (before he is within 20 of the village), the evil advisor says once: "Heh. From fisherman hero to corpse in one easy move. Neat, Boss." | todo | the health property is not ported |
| If the boy was already dead when the puzzle ended, the scroll goes and the evil advisor says: "Oh boy. You caught them all. Sweet.", "Let's go see the fisherm... Oh. We killed him. I forgot. Heh." and "Still, we rock at fishing. Any fish-gods out there better watch out!"; there is no turtle | todo | the health property is not ported |

## The reward: swapping to the turtle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The turtle waits with a silver scroll above its head, cannot be picked up, and keeps turning to look at the camera | todo | no turtle; the shared swap script, see [creature_swaps.md](creature_swaps.md); `CreatureDoAction` is a stub |
| When the camera is within 100 of it and it is on screen, the evil advisor points at it every 60 seconds: "Swap your Creature with this one if you want." | todo | no turtle |
| Clicking the scroll or the turtle with the player's creature more than 50 away: "You'll need to bring our Creature, Boss." | todo | no turtle |
| With the creature close, it is taken off its leash and walked over to the turtle (up to 5 seconds), and the two look each other over | partial | taking the leash off works (`DetachObjectLeash`); the walk (`CreatureDoAction`) is a stub and there is no turtle |
| Every 30 seconds the good advisor asks: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel."; clicking the turtle confirms, clicking the player's own creature or waiting 110 seconds cancels | todo | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining`, `ClearClickedObject` work; no turtle |
| Confirming: a cinematic with a two-creature camera; sparkles on both, the creatures point at each other, the swap is made with its sound, and three seconds after both animations end it closes | todo | `StartDualCamera` works, `SwapCreature` is a stub; how the swap carries the mind over: see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." | todo | `SwapCreature` stub |
| The old creature now waits in the turtle's place to be swapped back, and so on for ever: the offer never ends | todo | the swap script's timer is made but never checked |

## Aftermath, sound and music

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The boy's five tortoises stay on the shore | todo | `PopulateContainer` stub (no tortoises) |
| No music is started; the only sound is the reward sting on success | todo | the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The quest never changes alignment; the alignment changes the source once had for killing the boy are commented out | todo | the quest never starts: no Land 4 (`LOAD_MAP` is empty) |

## Quirks and unused parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The source notes that two of the boy's instruction lines were meant to be spoken by another voice; in the text table both are the boy's own lines, so they play as the boy | todo | the quest never starts: no Land 4 (`LOAD_MAP` is empty) |
| The scroll is marked done before the game checks whether the boy lived, so killing him still completes the scroll | todo | `UpdateSnapshot` stub |
| The script only waits for the boy to reach home; it never removes the puzzle | todo | undetermined whether the puzzle removes itself once played (ours keeps it: `ProcessPuzzleGame` stops once played) |
| A developer test script for the creature's creed also starts this quest; it is a developer test, not reached in the shipped game | n/a | developer test only |
