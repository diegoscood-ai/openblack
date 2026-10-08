# Puzzles and games

The puzzle objects and small games the lands' challenges are built on: the tree puzzles, the beach temple rings, the
Theseus maze, the lion maze, the fish herding, the shaking mushrooms, throwing stones, the singing stones, the man who
wants to be thrown, the shield stones, and the villagers' football with its ball. Each row is a rule of the game
itself; the story around each challenge (who asks, the films, the alignment) is in the land files,
[land_1.md](land_1.md) to [land_5.md](land_5.md), and what the rewards do is in [rewards.md](rewards.md). The fourth land's
bell memory game belongs to its gold scroll, [The Totem Puzzle](gold_scrolls/the_totem_puzzle.md).

Only the games compiled into Black & White's own challenge file count. The engine and the script sources also hold
puzzle kinds and games no Black & White land uses; they are listed at the end as n/a.

**Progress: 1/89 done, 11 partial — 7%**

## Puzzle objects

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A challenge script places a puzzle of a given kind at a place, with an angle and a size | done | `CREATE` of a puzzle game makes one with its kind, place, angle and size (`ecs::CreatePuzzleGame`, `src/ECS/PuzzleGames.cpp`); the kinds are in `src/ScriptHeaders/ScriptEnums.h` |
| A script can ask a puzzle whether it is not yet begun, in progress, won, lost, won the good way or won the evil way | todo | only `PLAYED` is answered, for the fish puzzle; the other states are not |
| A puzzle checks every game turn whether it is solved; once solved it stays solved and the script's "played" test becomes true | partial | `ProcessPuzzleGamesTurn` runs every turn (`Game.cpp`) and the played test stays true once true, but only the fish puzzle has a solve test |
| A puzzle's pieces (trees, rings, animals, markers) are the puzzle's own: if one is destroyed it is put back | partial | the parts are tracked and removed with their puzzle; putting a destroyed piece back is not done |
| Puzzles are saved and loaded with the game, pieces and all | todo | no save system |
| Each puzzle has a "did you know" signpost beside it with its rules, removed when the puzzle is solved | todo | the rules signposts come from the land scripts' did-you-know calls, never reached; see ../interface/scrolls_and_signs.md |

## Tree puzzles (lands 2 and 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine trees stand on a three-by-three grid; each spot has a state that shows as the kind of tree growing there | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Pulling a tree off its spot changes the state of that spot and of the four spots beside it (not the corners); each changed spot gets a new tree of its new kind, which starts small | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The land 2 puzzle has two states per spot; the land 3 puzzle has three | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| New trees grow back by a twentieth of their full size each step until full (the step is unconfirmed) | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree; see ../nature/trees.md |
| The puzzle is solved when all nine trees are fully grown and all nine spots are in the same state; the trees then stay as ordinary trees | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Land 2's prize is a flying-flock miracle dispenser that refills every 7 minutes; land 3's (which appears only after the player owns the nearby Indian town) is a flying-flock seed falling from the sky into that town, with its street lamps faded away | todo | never reached: `LOAD_MAP` is an empty native; see [land_2.md](land_2.md), [land_3.md](land_3.md) |

## Beach temple rings (land 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A beach temple made of four rings of different sizes stands on one of three bases | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Rings are moved by hand; a dropped ring settles onto the stack under it if one is within 10 m | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A ring can rest on a base or on a larger ring, never on a smaller one | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A ring stacked on another sits higher by the height of the ring below (4, 3, 2 or 1 m from the largest down) | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| A base moved off its place is put back | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when all four rings are stacked largest to smallest on the base up the beach, away from the tide | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The prize: the temple heals every living thing within 10 m of it, renewed every 20 s, for good | todo | never reached: `LOAD_MAP` is an empty native; see [land_2.md](land_2.md) and [silver_scrolls/the_beach_temple_puzzle.md](silver_scrolls/the_beach_temple_puzzle.md) |

## Theseus maze (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A walled grid holds a hero and a monster; the player clicks the ground of a square next to the hero, or the hero's own square to wait, and a marker shows the clicked square | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The hero steps one square that way at 4 m/s unless a wall is in the way | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Then the monster takes up to two steps towards the hero, each time trying across first and then along, and only where no wall blocks it | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| If the monster reaches the hero's square, or either of them is killed, the maze starts again | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when the hero walks out of the maze's exit | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Land 4 has two mazes, the second appearing when the first is solved; the prize is a "strong" creature-spell seed falling from the sky into the player's home town | todo | never reached: `LOAD_MAP` is an empty native; see [land_4.md](land_4.md) |

## Lion maze (land 5)

The engine calls this puzzle the lion maze, but on land 5 the animal in it is a wolf (Stanley) and its target a sheep;
the quest around it is [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A wolf stands in a walled grid; four bells round it each send the wolf one way, and a fifth, at a corner, starts the maze again | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree; see [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |
| Tapping a direction makes the wolf turn and then slide that way, square after square, until a wall stops it, walking at 4 m/s; each tap flashes the bell and plays its sound | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| If the wolf or the sheep is killed or lost, the maze starts again | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Solved when the wolf stops on the sheep's square; both vanish in a sparkle | todo | only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| The prize: a lion creature to swap for, and the fireball's second and third levels for the player's home town; the script only gives it while the wolf's owner lives, but he is made indestructible and can never die, so the prize always comes | todo | never reached: `LOAD_MAP` is an empty native; see [land_5.md](land_5.md) and [silver_scrolls/stanley_the_wolf.md](silver_scrolls/stanley_the_wolf.md) |

## Fish herding (land 4)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two shoals of 15 fish each are made in the sea, 11 m across, swimming at 7 (the unit is unconfirmed) | todo | ported: two shoals of 15 at the bait, a net of seven floats (`CreateFishPuzzle`, `components::FishBait`), checked with a test hook ([water.md](../../bw1-notes/water.md#fish-puzzle)); Land 4 itself is never reached |
| The fish swim away from the hand, so the player herds them by tapping the water | todo | the puzzle shoals are fish-farm shoals, which our tree scares away from the hand; Land 4 is never reached ([water.md](../../bw1-notes/water.md#fish-puzzle)) |
| Solved when the shoal reaches where the boy fishes (the exact test is unconfirmed) | todo | ported: solved when 30 fish are inside the net together for half a second (`IsPuzzleGamePlayed`); Land 4 is never reached. Our wiki differs: the test is 30 fish within 11 of the bait at once for 0.5 s, not the shoal reaching the boy ([water.md](../../bw1-notes/water.md#fish-puzzle)) |
| The prize, while the boy lives: a tortoise creature to swap for; five tortoises wander with him | todo | never reached: `LOAD_MAP` is an empty native; the swap needs creature natives (stubs); see [land_4.md](land_4.md) and [silver_scrolls/the_fish_puzzle.md](silver_scrolls/the_fish_puzzle.md) |

## Shaking mushrooms (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The challenge only exists when a force-feedback mouse is plugged in; otherwise nothing happens and the land's mushrooms stay as they are. With the mouse, the mushrooms already standing near the puzzle are removed first | partial | with no force-feedback mouse nothing happens, as here: `ImmersionExists` answers no, so the challenge never starts; see [silver_scrolls/the_immersion_mushrooms.md](silver_scrolls/the_immersion_mushrooms.md) |
| 18 mushrooms are made by a hut, scattered in a square round a cauldron in the same layout every game; each shakes in the hand through the mouse, and the ninth shakes most | todo | never started (`ImmersionExists` is a stub); only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Dropping the ninth mushroom in the cauldron wins, any other loses | todo | never started; only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |
| Won: a level 2 heal is cast on the hut and the prize is a compassion creature-spell dispenser | todo | never started |
| Lost: the hut catches fire and an explosion is cast on it (50 m across, for 30 s); the blast throws the man through the air to land where the camera is | todo | never started; see [land_1.md](land_1.md) |

## Throwing stones (land 1)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A boulder sits on a pillar as the target; knocking it off its place (more than 2 m) wins | partial | `ThrowingStones` calls real natives except `GameThingHit` and the log; it is started after the temple scene, not checked in game; see [silver_scrolls/throwing_stones.md](silver_scrolls/throwing_stones.md) |
| A pile of five rocks (half to nine tenths of full size) refills while the quest runs: a rock that is destroyed comes back, and one left more than 40 m from the pile is put back at half size; once the quest is won the pile stops refilling | partial | `CREATE` of rocks, `ObjectDelete` and the distances are real; not checked in game |
| A villager watches each throw: if the rock passes within 12 m of the pillar he cheers, otherwise he despairs | partial | `CREATE` of a villager, the distance reads and villager animations are real; not checked in game; see ../villager/ |
| He ducks when a rock in the hand comes within 10 m of him | todo | the held read (property 9) is not handled by `GetProperty` |
| If his hut falls below three quarters of its health he storms off to a friend's house | todo | a building's health (property 1) is not handled by `GetProperty` |
| Hitting the pillar while the boulder stays on it earns half marks and a word of praise | todo | the pillar hit needs `GameThingHit`, a stub |
| The prize is a toy ball falling from the sky by the target | todo | the toy ball reward: `CREATE` of a ball and the rewards are not done; see [rewards.md](rewards.md) |
| Afterwards a boulder comes back on the pillar; each time it is knocked off it turns into a water seed, six times in all | partial | `CREATE` of rocks and one-shot seeds is real; not checked in game |

## Singing stones (lands 1 and 2)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 1's circle has eight singing stones, each with its own note, and three wrong stones with a sour note; tapping a stone plays its note | partial | `SingingStoneCircle` is started by `LandControl1` at once and calls real natives (sounds through `PlaySoundEffect`, music in `src/Audio/Services/GameMusic`); not checked in game; see [silver_scrolls/the_singing_stones.md](silver_scrolls/the_singing_stones.md) |
| The circle is complete when the five loose stones are back in their own holes; they are then fixed in place and the circle plays its tune | partial | the hole checks use real distance and flag natives; not checked in game |
| Putting wrong stones in the holes upsets the hippy who lives by the circle | partial | real villager and text natives; not checked in game |
| The circle's prize is a food miracle dispenser | todo | the dispenser comes through the reward script (`CreateReward` is a stub); see [land_1.md](land_1.md) |
| Land 2's stones remember the last 14 taps and listen for three tunes | todo | never reached: `LOAD_MAP` is an empty native; see [silver_scrolls/the_singing_stones_land_2.md](silver_scrolls/the_singing_stones_land_2.md) |
| The first tune makes night fall, with a flock of 10 bats and a mist | todo | never reached: `LOAD_MAP` is an empty native; `CREATE_MIST` is a stub |
| Another tune raises the dead within 10 m of the circle for 5 minutes: dead villagers come back as skeletons of the same age, joining the town with id 11 (the Indian town by Lethys's snow edge) whichever town they came from, and dead animals come back as they were | todo | never reached: `LOAD_MAP` is an empty native; `SetSkeleton` is a stub |
| Each tune played adds a third to the challenge's success | todo | never reached: `LOAD_MAP` is an empty native; the log is a stub; see [land_2.md](land_2.md) |

## The man who wants to be thrown (land 3)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A man by a campfire can't be hurt or burnt and keeps asking to be picked up and thrown | todo | never reached: `LOAD_MAP` is an empty native |
| Throwing him slows the game to a third of its speed (0.32) for 3 s, the camera following him from 10 m, then the speed eases back to normal over 3 s | todo | never reached: `LOAD_MAP` is an empty native; see ../engine/game_loop_and_clock.md |
| After landing he walks back to his place and boasts it didn't hurt | todo | never reached: `LOAD_MAP` is an empty native |
| Landing in the sea, he is made again at his place once out of view | todo | never reached: `LOAD_MAP` is an empty native |
| If the creature eats him it is made to poo, and he comes out of it again | todo | never reached: `LOAD_MAP` is an empty native; creature actions by script are stubs; see ../creature/physiology.md |
| Set alight, he begs for the fire to be put out | todo | never reached: `LOAD_MAP` is an empty native |

## Shield stones (land 5)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Three stones, each with a chanting villager, beam power to a point 80 m above a town and a vertical beam holds a shield over it | todo | never reached: `LOAD_MAP` is an empty native; see [gold_scrolls/nemesis_shielded_village.md](gold_scrolls/nemesis_shielded_village.md) and ../miracles/spiritual_shield.md |
| The shield's radius is 35 m for each stone still beaming | todo | `SetMagicRadius` is a stub |
| A stone beams while it is within 1 m of its place and its chanter lives at his spot; moving or breaking it stops it | todo | never reached: `LOAD_MAP` is an empty native |
| A rock, tree or fireball flying within 40 m of a chanter startles him away for 45 s | todo | never reached: `LOAD_MAP` is an empty native |
| A beaming stone strikes the player's creature with lightning when it is within 100 m, unless it is invisible | todo | never reached: `LOAD_MAP` is an empty native |
| It starts when the town has any belief in the player or the creature comes within 300 m | todo | `BeliefForPlayer` is a stub; see [land_5.md](land_5.md) |

## Land 4's totems

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two totems start lowered; raising both to full height frees the skeleton village | todo | `CREATE` of a totem is not done; see [land_4.md](land_4.md), ../town/artefacts.md and [gold_scrolls/undead_village.md](gold_scrolls/undead_village.md#raising-the-totems) |
| A bronze "Did you know" totem puzzle north-west of the Japanese village uses the engine's third totem layout: six totems each turning through five positions, where changing one moves up to five others, until all are fully raised; a ring of influence (radius 30) surrounds it, and solving it gives a shield miracle dispenser | todo | `CREATE` of the puzzle game is real, but only the fish puzzle's rules are ported (`src/ECS/PuzzleGames.cpp`, `src/ECS/FishPuzzle.cpp`); this kind has no rules in our tree |

## Football

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A town's football pitch is a building with goals, corner flags and a centre circle, in two sizes, and comes with a ball | partial | the pitch is placed (`src/ECS/Archetypes/AbodeArchetype.cpp`); its ball and parts are not; see ../building/civic_buildings.md |
| Ten places: per side two attackers, two defenders and a goalkeeper | todo | no football in our tree; see ../villager/play_and_gossip.md |
| A match waits for at least three players on each side, then for the ball to be put on the centre spot and most players to be in place, and kicks off | todo | no football in our tree |
| Players who wait too long without enough players are sent away | todo | no football in our tree |
| A match goes back to waiting when a side has no players, or both sides have only one | todo | no football in our tree |
| A match stops when the town no longer most wants playtime or relaxation, once it has run 1800 turns | todo | no football in our tree; see ../town/ |
| The score is kept, home against away | todo | no football in our tree |
| After a goal play stops for 70 turns, then restarts from the centre | todo | no football in our tree |
| When the ball goes dead the nearest player fetches it for the restart while the others take their places | todo | no football in our tree |
| The ball is removed when it goes more than 100 m from the pitch or into water, or is carried too far away in a hand (a new one is then made, unconfirmed) | todo | no football in our tree |
| Attackers shoot at goal at 12 to 16 m/s, lob near goal at 3 to 4 m/s aiming within 2 m of it, and dribble with taps of 6 to 7 m/s a fifth of the way on | todo | no football in our tree |
| Defenders clear and save at 8 to 11 m/s aiming within 5 m, and mark players | todo | no football in our tree |
| The goalkeeper saves with kicks of 9 to 10 m/s and goes for a loose ball one time in five; outfield players go two times in three | todo | no football in our tree |
| Each player picks what to do by weighted chance (pass 0.8 to 1, save 0.7 to 1, clear 0.6 to 1, mark 0 to 1) | todo | no football in our tree |
| Spectators do a Mexican wave, one more joining each turn | todo | no football in our tree |
| The player or creature can take the ball and throw it; the creature learns from scoring and catching | todo | no football in our tree; see ../creature/town_actions.md and ../creature/learning_by_observation.md |
| A football match impresses those watching (it has its own impressiveness kind) | todo | no football in our tree; see ../worship/ |

## The ball

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A ball is a toy the creature can pick up, kick and throw | todo | `CREATE` of a ball is not done (`CreateScriptObject` leaves it to its default case); see ../creature/object_actions.md and [../nature/toys.md](../nature/toys.md) |
| A ball has its own bouncy physics | todo | see ../physics/object_dynamics.md |
| Creatures and villagers react to a ball near them and go to play | todo | see ../creature/reactions.md |

## Not used by Black & White's lands

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Two maze puzzles | n/a | an engine puzzle kind no land uses |
| Three of the four totem puzzle layouts: six totems each turning through five positions, where turning one moves the others by set amounts | n/a | no land uses them |
| A second lion maze, a second fish puzzle and a second mushroom puzzle | n/a | engine puzzle kinds no land uses |
| Lions and sheep: a take-away game against the computer, which plays the winning move when there is one | n/a | an engine puzzle kind no land uses |
| Chess | n/a | the kind exists but has no rules in the game |
| Cow bowling, bowling, whack-a-villager, catching villagers, a race, Simon says, the cup final, a spitting totem, the big whale, raiding lions | n/a | script sources not compiled into the game's challenges; the cup final: [../town/football.md](../town/football.md) |
| Creature Isle's own games | n/a | see [creature_isle.md](creature_isle.md) |
