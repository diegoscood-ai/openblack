# Puzzles and games

The puzzle objects and small games the lands' challenges are built on: the tree puzzles, the beach temple rings, the
Theseus maze, the lion maze, the fish herding, the shaking mushrooms, throwing stones, the singing stones, the man who
wants to be thrown, the shield stones, and the villagers' football with its ball. Each row is a rule of the game
itself; the story around each challenge (who asks, the films, the alignment) is in the land files,
[land_1.md](land_1.md) to [land_5.md](land_5.md), and what the rewards do is in [rewards.md](rewards.md). The fourth land's
bell memory game belongs to its gold scroll, [The Totem Puzzle](gold_scrolls/the_totem_puzzle.md).

Only the games compiled into Black & White's own challenge file count. The engine and the script sources also hold
puzzle kinds and games no Black & White land uses; they are listed at the end as n/a.

**Progress: 2/89 done, 0 chosen, 11 partial, 0 inferred — 9%**

## Puzzle objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script places a puzzle of a given kind at a place, with an angle and a size | done | `CREATE` of a puzzle game makes one with its kind, place, angle and size (`ecs::CreatePuzzleGame`, `src/ECS/PuzzleGames.cpp`); the kinds are in `src/ScriptHeaders/ScriptEnums.h` |
| A script can ask a puzzle whether it is not yet begun, in progress, won, lost, won the good way or won the evil way | partial 40% | `GetObjectState` answers it (`ecs::PuzzleGameStatusOf`): not yet begun, in progress or won for the fish puzzle, won from the start for the mazes and the second fish puzzle; the other puzzles are not ported and stay "not yet begun", and the immersion and lion and sheep outcomes are not given; re-measured 2026-10-21: answered for the fish puzzle and the mazes; the other puzzles stay not begun |
| A puzzle checks every game turn whether it is solved; once solved it stays solved and the script's "played" test becomes true | partial 50% | `ProcessPuzzleGamesTurn` runs every turn (`Game.cpp`) and the played test stays true once true, but only the fish puzzle has a solve test; the mazes and the second fish puzzle are solved from the start (`ecs::CompletesAtOnce`), as in the original; re-measured 2026-10-21: checked each turn; only the fish puzzle has a solve test |
| A puzzle's pieces (trees, rings, animals, markers) are the puzzle's own: if one is destroyed it is put back | partial 40% | the parts are tracked and removed with their puzzle; putting a destroyed piece back is not done; re-measured 2026-10-21: tracked and removed with the puzzle; a destroyed piece is not put back |
| Puzzles are saved and loaded with the game, pieces and all | pending | no save system |
| Each puzzle has a "did you know" signpost beside it with its rules, removed when the puzzle is solved | pending | the rules signposts come from the land scripts' did-you-know calls, never reached; see ../interface/scrolls_and_signs.md |

## Tree puzzles (lands 2 and 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine trees stand on a three-by-three grid; each spot has a state that shows as the kind of tree growing there | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Pulling a tree off its spot changes the state of that spot and of the four spots beside it (not the corners); each changed spot gets a new tree of its new kind, which starts small | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The land 2 puzzle has two states per spot; the land 3 puzzle has three | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| New trees grow back by a twentieth of their full size each step until full (the step is unconfirmed) | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree; see ../nature/trees.md |
| The puzzle is solved when all nine trees are fully grown and all nine spots are in the same state; the trees then stay as ordinary trees | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Land 2's prize is a flying-flock miracle dispenser that refills every 7 minutes; land 3's (which appears only after the player owns the nearby Indian town) is a flying-flock seed falling from the sky into that town, with its street lamps faded away | pending | Land 2's puzzle is not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; Land 3's is not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [land_2.md](land_2.md), [land_3.md](land_3.md) |

## Beach temple rings (land 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A beach temple made of four rings of different sizes stands on one of three bases | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Rings are moved by hand; a dropped ring settles onto the stack under it if one is within 10 m | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A ring can rest on a base or on a larger ring, never on a smaller one | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A ring stacked on another sits higher by the height of the ring below (4, 3, 2 or 1 m from the largest down) | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A base moved off its place is put back | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when all four rings are stacked largest to smallest on the base up the beach, away from the tide | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The prize: the temple heals every living thing within 10 m of it, renewed every 20 s, for good | pending | not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; see [land_2.md](land_2.md) and [silver_scrolls/the_beach_temple_puzzle.md](silver_scrolls/the_beach_temple_puzzle.md) |

## Theseus maze (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A walled grid holds a hero and a monster; the player clicks the ground of a square next to the hero, or the hero's own square to wait, and a marker shows the clicked square | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The hero steps one square that way at 4 m/s unless a wall is in the way | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Then the monster takes up to two steps towards the hero, each time trying across first and then along, and only where no wall blocks it | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| If the monster reaches the hero's square, or either of them is killed, the maze starts again | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when the hero walks out of the maze's exit | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Land 4 has two mazes, the second appearing when the first is solved; the prize is a "strong" creature-spell seed falling from the sky into the player's home town | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [land_4.md](land_4.md) |

## Lion maze (land 5)

The engine calls this puzzle the lion maze, but on land 5 the animal in it is a wolf (Stanley) and its target a sheep;
the quest around it is [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wolf stands in a walled grid; four bells round it each send the wolf one way, and a fifth, at a corner, starts the maze again | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree; see [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Tapping a direction makes the wolf turn and then slide that way, square after square, until a wall stops it, walking at 4 m/s; each tap flashes the bell and plays its sound | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| If the wolf or the sheep is killed or lost, the maze starts again | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when the wolf stops on the sheep's square; both vanish in a sparkle | pending | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The prize: a lion creature to swap for, and the fireball's second and third levels for the player's home town; the script only gives it while the wolf's owner lives, but he is made indestructible and can never die, so the prize always comes | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [land_5.md](land_5.md) and [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |

## Fish herding (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two shoals of 15 fish each are made in the sea, 11 m across, swimming at 7 (the unit is unconfirmed) | pending | ported: two shoals of 15 at the bait, a net of seven floats (`CreateFishPuzzle`, `components::FishBait`), checked with a test hook ([water.md](../../../world/sea-life-and-scenery.md#fish-puzzle)); Land 4 itself is never reached |
| The fish swim away from the hand, so the player herds them by tapping the water | pending | the puzzle shoals are fish-farm shoals, which our tree scares away from the hand; Land 4 is never reached ([water.md](../../../world/sea-life-and-scenery.md#fish-puzzle)) |
| Solved when the shoal reaches where the boy fishes (the exact test is unconfirmed) | pending | ported: solved when 30 fish are inside the net together for half a second (`IsPuzzleGamePlayed`); Land 4 is never reached. Our wiki differs: the test is 30 fish within 11 of the bait at once for 0.5 s, not the shoal reaching the boy ([water.md](../../../world/sea-life-and-scenery.md#fish-puzzle)) |
| The prize, while the boy lives: a tortoise creature to swap for; five tortoises wander with him | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; `SwapCreature` is real; see [land_4.md](land_4.md) and [silver_scrolls/the_fish_puzzle.md](silver_scrolls/the_fish_puzzle.md) |

## Shaking mushrooms (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge only exists when a force-feedback mouse is plugged in; otherwise nothing happens and the land's mushrooms stay as they are. With the mouse, the mushrooms already standing near the puzzle are removed first | done | with no force-feedback mouse nothing happens, as here: `ImmersionExists` answers no, so the challenge never starts; see [silver_scrolls/the_immersion_mushrooms.md](silver_scrolls/the_immersion_mushrooms.md); re-measured 2026-10-21: with no force-feedback mouse nothing happens, as the original |
| 18 mushrooms are made by a hut, scattered in a square round a cauldron in the same layout every game; each shakes in the hand through the mouse, and the ninth shakes most | pending | never started (`ImmersionExists` is real and answers no, as the original without a force-feedback mouse); only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Dropping the ninth mushroom in the cauldron wins, any other loses | pending | never started; only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Won: a level 2 heal is cast on the hut and the prize is a compassion creature-spell dispenser | pending | never started |
| Lost: the hut catches fire and an explosion is cast on it (50 m across, for 30 s); the blast throws the man through the air to land where the camera is | pending | never started; see [land_1.md](land_1.md) |

## Throwing stones (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A boulder sits on a pillar as the target; knocking it off its place (more than 2 m) wins | partial 60% | `ThrowingStones` calls only real natives (`GameThingHit` too); it is started after the temple scene, not checked in game; see [silver_scrolls/throwing_stones.md](silver_scrolls/throwing_stones.md); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| A pile of five rocks (half to nine tenths of full size) refills while the quest runs: a rock that is destroyed comes back, and one left more than 40 m from the pile is put back at half size; once the quest is won the pile stops refilling | partial 60% | `CREATE` of rocks, `ObjectDelete` and the distances are real; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| A villager watches each throw: if the rock passes within 12 m of the pillar he cheers, otherwise he despairs | partial 60% | `CREATE` of a villager, the distance reads and villager animations are real; not checked in game; see ../villager/; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| He ducks when a rock in the hand comes within 10 m of him | pending | the held read (property 9) is real (`GetProperty` IN_HAND); not seen in game |
| If his hut falls below three quarters of its health he storms off to a friend's house | pending | a building's health (property 1) is real in `GetProperty`; not seen in game |
| Hitting the pillar while the boulder stays on it earns half marks and a word of praise | pending | `GameThingHit` is real; not seen in game |
| The prize is a toy ball falling from the sky by the target | pending | `CREATE` of a ball (`BallArchetype::CreateForNearestTown`) and the reward chests are real; not seen in game; see [rewards.md](rewards.md) |
| Afterwards a boulder comes back on the pillar; each time it is knocked off it turns into a water seed, six times in all | partial 60% | `CREATE` of rocks and one-shot seeds is real; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## Singing stones (lands 1 and 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1's circle has eight singing stones, each with its own note, and three wrong stones with a sour note; tapping a stone plays its note | partial 60% | `SingingStoneCircle` is started by `LandControl1` at once and calls real natives (sounds through `PlaySoundEffect`, music in `src/Audio/GameMusic`); not checked in game; see [silver_scrolls/the_singing_stones.md](silver_scrolls/the_singing_stones.md); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The circle is complete when the five loose stones are back in their own holes; they are then fixed in place and the circle plays its tune | partial 60% | the hole checks use real distance and flag natives; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Putting wrong stones in the holes upsets the hippy who lives by the circle | partial 60% | real villager and text natives; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The circle's prize is a food miracle dispenser | pending | the dispenser comes through the reward script (`CreateReward` is real); see [land_1.md](land_1.md) |
| Land 2's stones remember the last 14 taps and listen for three tunes | pending | not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; see [silver_scrolls/the_singing_stones_land_2.md](silver_scrolls/the_singing_stones_land_2.md) |
| The first tune makes night fall, with a flock of 10 bats and a mist | pending | not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; `CREATE_MIST` is real |
| Another tune raises the dead within 10 m of the circle for 5 minutes: dead villagers come back as skeletons of the same age, joining the town with id 11 (the Indian town by Lethys's snow edge) whichever town they came from, and dead animals come back as they were | pending | not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; `SetSkeleton` is real |
| Each tune played adds a third to the challenge's success | pending | not reached by any check: it starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; the challenge record is real (`Snapshot`, `UpdateSnapshot`); see [land_2.md](land_2.md) |

## The man who wants to be thrown (land 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A man by a campfire can't be hurt or burnt and keeps asking to be picked up and thrown | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Throwing him slows the game to a third of its speed (0.32) for 3 s, the camera following him from 10 m, then the speed eases back to normal over 3 s | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see ../engine/game_loop_and_clock.md |
| After landing he walks back to his place and boasts it didn't hurt | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Landing in the sea, he is made again at his place once out of view | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the creature eats him it is made to poo, and he comes out of it again | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; creature actions by script go through the creature's mind; see ../creature/physiology.md |
| Set alight, he begs for the fire to be put out | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Shield stones (land 5)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three stones, each with a chanting villager, beam power to a point 80 m above a town and a vertical beam holds a shield over it | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md) and ../miracles/spiritual_shield.md |
| The shield's radius is 35 m for each stone still beaming | pending | `SetMagicRadius` is a stub |
| A stone beams while it is within 1 m of its place and its chanter lives at his spot; moving or breaking it stops it | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A rock, tree or fireball flying within 40 m of a chanter startles him away for 45 s | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A beaming stone strikes the player's creature with lightning when it is within 100 m, unless it is invisible | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It starts when the town has any belief in the player or the creature comes within 300 m | pending | `BeliefForPlayer` is real (towns); not checked in play; see [land_5.md](land_5.md) |

## Land 4's totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two totems start lowered; raising both to full height frees the skeleton village | pending | `CREATE` of a totem makes a puzzle totem ([the puzzle totem](../../../magic/puzzle-totem.md)); its height, the creature's pull and the hand's drag are not ported; see [land_4.md](land_4.md), ../town/artefacts.md and [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md#raising-the-totems) |
| A bronze "Did you know" totem puzzle north-west of the Japanese village uses the engine's third totem layout: six totems each turning through five positions, where changing one moves up to five others, until all are fully raised; a ring of influence (radius 30) surrounds it, and solving it gives a shield miracle dispenser | pending | `CREATE` of the puzzle game is real, but only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town's football pitch is a building with goals, corner flags and a centre circle, in two sizes, and comes with a ball | partial 40% | the pitch is placed (`src/ECS/Archetypes/AbodeArchetype.cpp`); its ball and parts are not; see ../building/civic_buildings.md; re-measured 2026-10-21: the pitch is placed; its ball and parts are not |
| Ten places: per side two attackers, two defenders and a goalkeeper | pending | no football in our tree; see ../villager/play_and_gossip.md |
| A match waits for at least three players on each side, then for the ball to be put on the centre spot and most players to be in place, and kicks off | pending | no football in our tree |
| Players who wait too long without enough players are sent away | pending | no football in our tree |
| A match goes back to waiting when a side has no players, or both sides have only one | pending | no football in our tree |
| A match stops when the town no longer most wants playtime or relaxation, once it has run 1800 turns | pending | no football in our tree; see ../town/ |
| The score is kept, home against away | pending | no football in our tree |
| After a goal play stops for 70 turns, then restarts from the centre | pending | no football in our tree |
| When the ball goes dead the nearest player fetches it for the restart while the others take their places | pending | no football in our tree |
| The ball is removed when it goes more than 100 m from the pitch or into water, or is carried too far away in a hand (a new one is then made, unconfirmed) | pending | no football in our tree |
| Attackers shoot at goal at 12 to 16 m/s, lob near goal at 3 to 4 m/s aiming within 2 m of it, and dribble with taps of 6 to 7 m/s a fifth of the way on | pending | no football in our tree |
| Defenders clear and save at 8 to 11 m/s aiming within 5 m, and mark players | pending | no football in our tree |
| The goalkeeper saves with kicks of 9 to 10 m/s and goes for a loose ball one time in five; outfield players go two times in three | pending | no football in our tree |
| Each player picks what to do by weighted chance (pass 0.8 to 1, save 0.7 to 1, clear 0.6 to 1, mark 0 to 1) | pending | no football in our tree |
| Spectators do a Mexican wave, one more joining each turn | pending | no football in our tree |
| The player or creature can take the ball and throw it; the creature learns from scoring and catching | pending | no football in our tree; see ../creature/town_actions.md and ../creature/learning_by_observation.md |
| A football match impresses those watching (it has its own impressiveness kind) | pending | no football in our tree; see ../worship/ |

## The ball

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ball is a toy the creature can pick up, kick and throw | pending | `CREATE` of a ball is real (`BallArchetype::CreateForNearestTown`: the nearest town takes it); the creature's play with it is not checked; see ../creature/object_actions.md and [../nature/toys.md](../nature/toys.md) |
| A ball has its own bouncy physics | pending | see ../physics/object_dynamics.md |
| Creatures and villagers react to a ball near them and go to play | pending | see ../creature/reactions.md |

