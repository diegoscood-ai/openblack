# The computer gods' creatures

Each computer god has a creature, loaded from a mind file the game ships, set up by script and then steered by its god:
fed, healed, taught, played with and sent to win, destroy or defend towns and to fight other creatures.

**Progress: 5/47 done, 0 chosen, 5 partial, 0 inferred — 17%**

See [../creature/](../creature/) for how any creature thinks and acts, especially
[../creature/decision_making.md](../creature/decision_making.md), [../creature/fighting.md](../creature/fighting.md) and
[../creature/saves_and_files.md](../creature/saves_and_files.md).

## The mind files

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's, Lethys's and Nemesis's creatures come from three mind files that are byte for byte the same (an older file version), so they start alike; the script picks the species | partial 70% | the files can be read (`components/creaturemind`, `resources::CreatureMindLoader`); `LOAD_CREATURE`, the story's loading function, loads them; re-measured 2026-10-21: the files are read and LOAD_CREATURE loads them (src/CHLApi.cpp, ScriptLoadCreature) |
| Skirmish gods' creatures come from a generic computer creature file in a newer version | done | `CREATE_CREATURE_FROM_FILE` loads it (`FeatureScriptCommands::CreateCreatureFromFile`) |
| One skirmish land uses Nemesis's file for all its gods | done | the same command, with Nemesis's file |
| A creature loaded for a god belongs to that god and is placed where the script says | partial 70% | the land command does this (`CreatureArchetype`, owned by the script's player), turned a fixed 180 degrees and at the species' starting size (unconfirmed what the game uses); re-measured 2026-10-21: owned and placed; the facing is fixed and the size is ours |

## How the story sets them up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Khazar's is a tortoise at 1.2 times size, Lethys's a wolf at 1.5, Nemesis's a lion at 2 | partial 60% | the species come from `LOAD_CREATURE` (`ecs::player_creature::ScriptLoadCreature`); the sizes through `CREATURE_AUTOSCALE` are not confirmed in a run; re-measured 2026-10-21: loaded as the script asks; the size is not seen in a run yet |
| Each is named from the game's text | pending | `SET_CREATURE_NAME` is real |
| Each is made fully grown and scaled automatically by the factor given | pending | `SET_CREATURE_DEV_STAGE` works (`ecs::player_creature::SetDevelopmentStage`) and so does `CREATURE_AUTOSCALE`; the creature's loading is not checked in play |
| Each knows every everyday skill: building, using fields, totems and storehouses, fishing and dancing | pending | `CREATURE_SET_KNOWS_ACTION` is real |
| Each knows fireball and lightning with both power-ups, heal and its power-up, teleport, forest, food and its power-up, both storms, tornado, both shields, wood, water and its power-up, both flocks, and all sixteen creature miracles | pending | `CREATURE_SET_KNOWS_ACTION` is real, and the creature is never loaded |
| Strength, warmth, fatness, energy, itchiness, poo, exhaustion, thirst and fighting health are all set to 0.2 | pending | `LOAD_CREATURE` makes the creature now (`LoadCreature` in `src/CHLApi.cpp`), but the story has not yet been run past Land 2's arrival |
| They are released into the world, then again after their alignment is set | pending | `LOAD_CREATURE` makes the creature now (`LoadCreature` in `src/CHLApi.cpp`), but the story has not yet been run past Land 2's arrival |
| Khazar's takes the player's creature's alignment and is made its friend; the others take the opposite | pending | `CREATURE_FORCE_FRIENDS` is real |
| The story waits for the player's creature, checking every 5 seconds, before setting alignment | pending | the creature functions it waits on are real now; the story has not yet been run past Land 2's arrival |
| A creature that wins a fight is healed by 1% every 6 seconds until full or until it fights again | pending | done by the story scripts of lands play has not yet reached (Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real, and no later land is reached), and the creature is never loaded in play |

## Knowledge and size for skirmish

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A god's creature learns every miracle its god has, and heal, food, wood and water always, as if seen enough to learn | done | the computer mind's start teaches it (`ComputerPlayerSystem`, `computer_player::MiraclesToLearn`; unverified against v1.42) |
| Its size is set from the person's creature: 0.9 to 1.2 times as tall | done | `SET_COMPUTER_PLAYER_CREATURE_LIKE` names it; the computer mind's start sizes the creature (`ComputerPlayerSystem`, `computer_player::SizeLike`; unverified against v1.42) |
| Its height for decisions ignores the shrinking near its temple | pending | no computer god mind in our tree |
| A computer god's creature keeps the speed it was given when it stops, where a person's goes back to walking | pending | no computer players in our tree; see [../creature/locomotion.md](../creature/locomotion.md) |

## Looking after it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The god finds its creature and judges how hungry, starving, thirsty, tired, hot, cold, hurt or ill it is | pending | no computer god mind in our tree; see [ai.md](ai.md) |
| It feeds it from fields or fish farms, or casts food for it | pending | no computer god mind in our tree |
| It makes it drink, sleep, go home, or move somewhere warmer or cooler (all off unless a script turns them on) | pending | no computer god mind in our tree |
| It heals it when hurt, and restores it from illness | pending | no computer god mind in our tree |
| It judges how near and free its creature is before using it | pending | no computer god mind in our tree |

## Teaching it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It judges how important it is to teach an ordinary skill, aggressive, compassionate or impressive magic | pending | no computer god mind in our tree |
| It only teaches miracles its creature doesn't know, choosing among those marked known from the start | partial 30% | the filter is read (`MiracleRule::knownAtStart`, which makes nothing known; see [../creature/learning_by_observation.md](../creature/learning_by_observation.md)); no god uses it; re-measured 2026-10-21: the filter is read; no god uses it |
| To teach a skill it leads the creature on the leash and does the action in front of it | pending | no computer god mind in our tree |
| To teach a miracle it casts it in front of the creature | pending | no computer god mind in our tree |
| It teaches its creature to use the town's totem | pending | no computer god mind in our tree |
| It weighs the creature's own desire to learn a miracle | pending | no computer god mind in our tree |
| It leashes its creature to things or to other creatures | pending | no computer god mind in our tree; see [../creature/leash.md](../creature/leash.md) |

## Playing with it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It throws food at it to play | pending | no computer god mind in our tree |
| It strokes or slaps it (both off by default) | pending | no computer god mind in our tree; see [../creature/learning_from_feedback.md](../creature/learning_from_feedback.md) |
| It casts on it for a laugh (off by default) | pending | no computer god mind in our tree |

## Using it

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It sends it to impress a town it wants to win (with a 3300 setting on that action, unconfirmed what it measures) | pending | no computer god mind in our tree |
| It sends it to destroy a town | pending | no computer god mind in our tree |
| It sends it to defend a town, or to help a town or a building site | pending | no computer god mind in our tree |
| It sends it to attack another creature, if it is near enough | pending | no computer god mind in our tree |
| It uses it as a diversion against a player who attacks it | pending | no computer god mind in our tree |
| Scripts force a god's creature to help a town (Nemesis's battle plan does) | pending | `FORCE_COMPUTER_PLAYER_ACTION` is real |
| A computer god's creature fights with the computer's moves | partial 70% | any creature not the local player's fights by the computer's moves (`fight::ChooseMove`, `CreatureFightSystem`); there are no computer players; see [../creature/fighting.md](../creature/fighting.md); re-measured 2026-10-21: the computer's moves are used for any creature not the local player's; there are no computer players |

## Story scenes with them

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Lethys's creature torments the captive player's creature on the third land | pending | the creature is never loaded; see [lethys.md](lethys.md) |
| Lethys's creature is made strong and carries Khazar's off on the second land | pending | Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real; see [khazar.md](khazar.md) |
| Nemesis's lion is swapped for a double of the player's species for the final fight | pending | `SWAP_CREATURE` and `LOAD_CREATURE` are real; see [nemesis.md](nemesis.md) |
| A mirror creature of the player's species, opposite in alignment, angry and fully grown, is made from the player's creature for a fight (unconfirmed whether the game runs it) | pending | `CREATURE_CREATE_RELATIVE_TO_CREATURE` (the script statement create creature from creature) is real now (`CreatureCreateRelativeToCreature`), but Land 5 is not reached in play; see [../story/land_5.md](../story/land_5.md) |
| While a script kills Khazar, both gods' creatures have all their desires turned off and are kept at full health | pending | `CREATURE_TURN_OFF_ALL_DESIRES` and the desire functions are real now; not reached in play |
| Scripts can turn every desire off and set one desire on, as for the mirror creature's anger at 0.5 | done | `CREATURE_TURN_OFF_ALL_DESIRES` (`CreatureTurnOffAllDesires`), `SET_CREATURE_ONLY_DESIRE`, `CREATURE_SET_DESIRE_VALUE`, `CREATURE_SET_DESIRE_ACTIVATED` and `CREATURE_SET_DESIRE_MAXIMUM` in `src/CHLApi.cpp` |
| Freed creatures on the third land, the player's and Lethys's, can both be leashed | pending | the story has not yet reached Land 3 in play: it has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
