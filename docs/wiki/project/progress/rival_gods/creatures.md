# The computer gods' creatures

Each computer god has a creature, loaded from a mind file the game ships, set up by script and then steered by its god:
fed, healed, taught, played with and sent to win, destroy or defend towns and to fight other creatures.

**Progress: 4/38 done, 0 chosen, 7 partial, 0 inferred — 19%**

See [../creature/](../creature/) for how any creature thinks and acts, especially
[../creature/decision_making.md](../creature/decision_making.md), [../creature/fighting.md](../creature/fighting.md) and
[../creature/saves_and_files.md](../creature/saves_and_files.md).

## The mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from three mind files that are byte for byte the same (an older file version), so they start alike; the script picks the species | partial 70% | the files can be read (`components/creaturemind`, `resources::CreatureMindLoader`); `LOAD_CREATURE`, the story's loading function, loads them |
| Skirmish gods' creatures come from a generic computer creature file in a newer version | done | `CREATE_CREATURE_FROM_FILE` loads it (`FeatureScriptCommands::CreateCreatureFromFile`); test `ScriptLoadCreatureTest.TheFileStepMakesTheSameCreatureForEveryCaller` |
| A creature loaded for a god belongs to that god and is placed where the script says | partial 70% | the land command does this (`CreatureArchetype`, owned by the script's player), turned a fixed 180 degrees and at the species' starting size (unconfirmed what the game uses) |

## How the story sets them up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each knows every everyday skill: building, using fields, totems and storehouses, fishing and dancing | partial 60% | `CREATURE_SET_KNOWS_ACTION` is real; quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127716); finished by [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Teaches a creature (or makes it forget) an action…" |
| Each knows fireball and lightning with both power-ups, heal and its power-up, teleport, forest, food and its power-up, both storms, tornado, both shields, wood, water and its power-up, both flocks, and all sixteen creature miracles | partial 60% | `CREATURE_SET_KNOWS_ACTION` is real; `LOAD_CREATURE` makes the creature now; quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127754); finished by [challenge_natives_creature.md](../scripts/challenge_natives_creature.md) "Teaches a creature (or makes it forget) an action…" |
| Strength, warmth, fatness, energy, itchiness, poo, exhaustion, thirst and fighting health are all set to 0.2 | done | `LOAD_CREATURE` makes the creature now (`LoadCreature` in `src/CHLApi.cpp`); quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127772); its effects' own rows are done: [challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) "Sets a creature's strength or alignment (33 and 21 calls)", [challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) "Sets a creature's body: warmth, fatness, energy…" |
| They are released into the world, then again after their alignment is set | partial 60% | `LOAD_CREATURE` makes the creature now (`LoadCreature` in `src/CHLApi.cpp`); quest_check --natural PASS, run a1q_impress (2026-10-28 22:03:10, refs/story/evidence/a1q_impress.log line 127778); finished by [challenge_natives_objects_and_world.md](../scripts/challenge_natives_objects_and_world.md) "Hands an object back to the game after a script…" |
| The story waits for the player's creature, checking every 5 seconds, before setting alignment | pending | the creature functions it waits on are real now; the story has not yet been run past Land 2's arrival |
| A creature that wins a fight is healed by 1% every 6 seconds until full or until it fights again | pending | done by the story scripts of lands play has not yet reached (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached) |

## Knowledge and size for skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god's creature learns every miracle its god has, and heal, food, wood and water always, as if seen enough to learn | done | the computer mind's start teaches it (`ComputerPlayerSystem`, `computer_player::MiraclesToLearn`; unverified against v1.42); tests `ComputerPlayerStart.TheCreatureLearnsThePlayersMiraclesAndHealFoodWoodAndWater`, `ComputerPlayerStart.EveryMiracleIsLearntWhenThePlayerCanCastEveryMiracle` |
| Its size is set from the person's creature: 0.9 to 1.2 times as tall | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42); test `ComputerPlayerStart.TheCreatureIsNinetyToOneHundredAndTwentyPercentOfThePersonsDrawnSize` |
| Its height for decisions ignores the shrinking near its temple | pending | no score rule reads this height yet (`src/ECS/ComputerPlayerScoresCreature.cpp`) |
| A computer god's creature keeps the speed it was given when it stops, where a person's goes back to walking | pending | computer players are marked (`PlayerMagic::playerType` 2), but the locomotion makes no difference for their creatures; see [../creature/locomotion.md](../creature/locomotion.md) |

## Looking after it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The god finds its creature and judges how hungry, starving, thirsty, tired, hot, cold, hurt or ill it is | partial 30% | the score rules (`computer_player::Score`, the creature by `PlayersCreature`, its desires by `ScoreWorld::CreatureDesire`, `src/ECS/ComputerPlayerScoresCreature.cpp`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#creatureinteract) |
| It feeds it from fields or fish farms, or casts food for it | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It makes it drink, sleep, go home, or move somewhere warmer or cooler (all off unless a script turns them on) | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It heals it when hurt, and restores it from illness | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It judges how near and free its creature is before using it | partial 30% | `computer_player::CreatureFreeTime` and `CreatureJobWorth` (`src/ECS/ComputerPlayerScores.h`), pinned to the oracle (`ComputerMindScores.*`); run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#creatureinteract) |

## Teaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It judges how important it is to teach an ordinary skill, aggressive, compassionate or impressive magic | partial 30% | the teaching score rules (`computer_player::Score`, `src/ECS/ComputerPlayerScoresCreature.cpp`), pinned to the oracle (`ComputerMindScores.*`); the lesson counts stay 0 until the mind's actions count them; run in the mind's turn (`computer_world::MakeScoreWorld`), few of the actions under it ported; see [../../../hand/computer-minds.md](../../../hand/computer-minds.md#creatureinteract) |
| It only teaches miracles its creature doesn't know, choosing among those marked known from the start | partial 30% | the filter is read (`MiracleRule::knownAtStart`, which makes nothing known; see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)); no god uses it |
| To teach a skill it leads the creature on the leash and does the action in front of it | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| To teach a miracle it casts it in front of the creature | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It teaches its creature to use the town's totem | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| It weighs the creature's own desire to learn a miracle | pending | the mind's turn runs (`computer_player::TakeTurn`, from `ComputerPlayerSystem::ProcessTurn`), but none of this desire's actions is ported yet (6 of the 59 are, `computer_hand::HandActions`) |
| It leashes its creature to things or to other creatures | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [../creature/leash.md](../creature/leash.md) |

## Playing with it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It throws food at it to play | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It strokes or slaps it (both off by default) | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [../creature/learning_from_feedback.md](../creature/learning_from_feedback.md) |
| It casts on it for a laugh (off by default) | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |

## Using it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It sends it to impress a town it wants to win (with a 3300 setting on that action, unconfirmed what it measures) | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It sends it to destroy a town | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It sends it to defend a town, or to help a town or a building site | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It sends it to attack another creature, if it is near enough | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| It uses it as a diversion against a player who attacks it | pending | no action of the mind does it yet: its score rule is written (`computer_player::Score`, `src/ECS/ComputerPlayerScores.h`) and runs in the mind's turn, but the action is not ported; see [ai.md](ai.md) |
| A computer god's creature fights with the computer's moves | partial 70% | any creature not the local player's fights by the computer's moves (`fight::ChooseMove`, `CreatureFightSystem`); computer players are marked (`PlayerMagic::playerType` 2) and their creatures fight as any other not the local player's; see [../creature/fighting.md](../creature/fighting.md) |

## Story scenes with them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A mirror creature of the player's species, opposite in alignment, angry and fully grown, is made from the player's creature for a fight (unconfirmed whether the game runs it) | pending | the script statement create creature from creature is ported (`CreatureCreateRelativeToCreature`, [a creature made from another](../../../creature/players-creature.md#a-creature-made-from-another)), but Land 5 is not reached in play; see [../story/land_5.md](../story/land_5.md) |
| While a script kills Khazar, both gods' creatures have all their desires turned off and are kept at full health | pending | `CREATURE_TURN_OFF_ALL_DESIRES` and the desire functions are real now; not reached in play |
| Scripts can turn every desire off and set one desire on, as for the mirror creature's anger at 0.5 | done | `CREATURE_TURN_OFF_ALL_DESIRES` (`CreatureTurnOffAllDesires`), `SET_CREATURE_ONLY_DESIRE`, `CREATURE_SET_DESIRE_VALUE`, `CREATURE_SET_DESIRE_ACTIVATED` and `CREATURE_SET_DESIRE_MAXIMUM` in `src/CHLApi.cpp`; test `ScriptGuideOgreNatives.TheOgresDesires` |
| Freed creatures on the third land, the player's and Lethys's, can both be leashed | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
