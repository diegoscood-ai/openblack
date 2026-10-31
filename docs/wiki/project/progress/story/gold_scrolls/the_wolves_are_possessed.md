# The Wolves Are Possessed

A gold story event on the third land: after the player has won two villages and stopped two of the creature's prison
pillars, Lethys sends a pack of twenty possessed wolves at the Indian village. If the player has finished the monk's
quest, the monk appears and turns most of the wolves into cows. The player must kill the rest before any reaches the
village, or the village's faith swings to Lethys. It is logged in the story log under the monk's line "The wolves are
possessed." (it has no title of its own) and has no scroll to click.

**Land:** 3 · **Giver:** Lethys (a cut scene; no scroll) · **Script:** FreeTheCreature (its wolf attack) · **Reward:** none; failing gives the Indian village to Lethys · **Repeatable:** no

Part of freeing the creature: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md).
The monk is the silver scroll [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md). The land:
[../land_3.md](../land_3.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table. openblack never runs Land 3's control script (the story reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked), and of what it
needs, flocks, animals, villagers, dialogue, snapshots and belief changes (`ObjectRelativeBelief`, `SetPlayerBelief`)
work in `src/CHLApi.cpp`, so every row is pending unless the notes say otherwise.

**Progress: 11/28 done, 0 chosen, 6 partial, 0 inferred — 52%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly two pillars are down and the Indian village is the player's; if the Indian village is won third, it never happens | pending | the prison's pillars follow town ownership, read through `GetTownWithId` (real); not checked in play |
| At the same moment, if the monk's quest is done, his Wonder gift is started | pending | follows from the same start; see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry when it ends | done | `Snapshot` is real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 167088); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture", [challenges_and_rewards.md](../challenges_and_rewards.md) "Its record is updated with how well it went (0 to 1) and the alignment it earned" |
| It waits 5 minutes, then waits until the Indian village is the player's (or gone) | pending | the town ownership read (`GET_PROPERTY` Player of a town) is real; not checked in play |
| If all three pillars are down by then, nothing happens and the entry is simply marked complete | pending | the pillars follow town ownership, read through `GetTownWithId` (real), and the challenge record is real (`Snapshot`, `UpdateSnapshot`); not checked in play |

## The attack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is formed at a spot about 510 from the Indian village, towards the prison: wolves are made within 20 of it until there are 20, held loosely together (inner radius 5, outer 40) | partial 60% | `FlockCreate`, `FlockAttach` and `CREATE` of animals are real (`ecs::script_containers`, `CreateScriptObject`); quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 143541); done in their own rows: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Creates an empty flock at a position", "Adds an animal or villager to a flock", [livestock.md](../../animal/livestock.md) "Scripts make, move and read flocks"; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Changes the inner and outer radius (and calm) of a shield, storm or influence" |
| The pack sets off slowly (half speed) towards the Indian village | partial 60% | `MoveGameThing` and the speed are handled for animals; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 143545); finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position, ending within a radius of it" |
| A cut scene with one of the generic quest tunes: the camera watches from behind and follows the pack. Lethys: "You may have your few believers." Lethys: "But I can still command the beasts of the wilderness." | done | `StartMusic`, the camera and `RunText` are real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 145741); its effects' own rows are done: [music.md](../../audio/music.md) "Scripts start and stop a piece of music", [script_camera.md](../../camera/script_camera.md) "Set the camera's position at once", [script_camera.md](../../camera/script_camera.md) "Make the camera's focus follow a thing, at once or gliding", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…"; the voices of the run's texts match the original's text voice table (`tools/checks/story/voice_table_check.py`: 6 of 6 in the saved run, 8 of 8 in the village-lost run) |
| The camera moves; the pack quickens a little (0.6) | partial 60% | real camera natives and the speed; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 148865); done in its own row: [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position, ending within a radius of it" |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the Tibetan town music plays, and after 4 seconds the monk appears near the pack's path facing the camera, playing his ambient animation five times | partial 60% | `StartMusic` and `CREATE` of a villager are real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 155477); done in their own rows: [music.md](../../audio/music.md) "Scripts start and stop a piece of music", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object to face a position", "Puts a villager, animal or other living thing into one of its script states" |
| Monk: "It seems you have need of me." Monk: "The wolves are possessed." | done | `RunText` is real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 156582); its effects' own rows are done: [advisors.md](../advisors.md) "The monk, the island keeper and the villagers of Lands 3 to 5's challenges", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…"; the voices of the run's texts match the original's text voice table (`tools/checks/story/voice_table_check.py`: 6 of 6 in the saved run, 8 of 8 in the village-lost run) |
| The camera follows the pack again; wolves are taken from it until 8 are left. Each taken wolf, after 1 to 4 seconds, stops, a sparkle appears and it is replaced by a cow, which is let go | partial 60% | `FlockDetach`, `SpecialEffectObject` and `ObjectDelete` are real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 158947); done in their own rows: [script_camera.md](../../camera/script_camera.md) "Make the camera's focus follow a thing, at once or gliding", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Takes a member out of a flock", "Starts a special effect (sparkles, smoke, explosions and so on) at a position for a time"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Hands an object back to the game after a script has controlled it" |
| Monk: "But I think I can help you. I will try to destroy some of the wolves." Monk: "That is all." The monk disappears | done | `RunText` and `ObjectDelete` are real; quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 163305); its effects' own rows are done: [advisors.md](../advisors.md) "The monk, the island keeper and the villagers of Lands 3 to 5's challenges", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…"; the voices of the run's texts match the original's text voice table (`tools/checks/story/voice_table_check.py`: 6 of 6 in the saved run, 8 of 8 in the village-lost run) |
| Without the monk, the scene just lasts 12 seconds more and all 20 wolves come | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The story-log entry is started at nothing done, its reminder the good advisor's "The Village is coming under attack!" | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 163355); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture", [script_camera.md](../../camera/script_camera.md) "Ending camera control gives the camera back to the player" |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Kill every wolf of the pack before any of them gets within 25 of the spot at the edge of the village; the check is every 5 seconds | pending | `IdSize` and the distances are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| There is no time limit other than the wolves' walk | done | the same script; quest_check PASS (absent: no timer native in the quest's window), run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 167088), the saved-village path; plain script: nothing outside the script moves |

## Failure: the wolves reach the village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack is let loose; a cut scene flies the camera to the village | partial 60% | real camera natives; quest_check PASS, run a2l3_2_q2 (joint295 base, 2026-10-30 22:26, refs/story/evidence/a2l3_wolves_lost.log line 174371); done in their own rows: [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Glide the camera's focus to a point over a time"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Hands an object back to the game after a script has controlled it" |
| The story-log entry is marked complete (the source's own comment calls it "you have failed to save the town") | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); quest_check PASS, run a2l3_2_q2 (joint295 base, 2026-10-30 22:26, refs/story/evidence/a2l3_wolves_lost.log line 175859); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture" |
| The good advisor steps out and points: "The wolves have reached the Village!" then "The Villagers are switching allegiance!" | done | advisors and texts are real; quest_check PASS, run a2l3_2_q2 (joint295 base, 2026-10-30 22:26, refs/story/evidence/a2l3_wolves_lost.log line 178151); its effects' own rows are done: [advisors.md](../advisors.md) "Ejected, an advisor flies out from its corner into the middle of the view in a second", [advisors.md](../advisors.md) "Scripts make one point at a place or an object in the world, and stop", [advisors.md](../advisors.md) "Every advisor line has its own recording", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…"; the voices of the run's texts match the original's text voice table (`tools/checks/story/voice_table_check.py`: 6 of 6 in the saved run, 8 of 8 in the village-lost run) |
| Lethys's belief in the Indian village is set to full and the player's to 0.3 | done | `ObjectRelativeBelief` and `SetPlayerBelief` are real; quest_check PASS, run a2l3_2_q2 (joint295 base, 2026-10-30 22:26, refs/story/evidence/a2l3_wolves_lost.log line 178162); its effects' own rows are done: [challenge_natives_towns_and_players.md](../../scripts/challenge_natives_towns_and_players.md) "Sets an object's belief in a player relative to others" |
| This can lose the village, raising its prison pillar again | pending | the belief natives are real and the pillars follow the town's owner (real reads); not checked in play; see [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the last wolf of the pack is dead the entry is marked complete; there is no line and no reward | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 167088); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "Its record is updated with how well it went (0 to 1) and the alignment it earned" |
| Either way the entry ends at complete with no alignment change, so the log cannot tell success from failure | done | the challenge record is real (`Snapshot`, `UpdateSnapshot`); quest_check PASS, run a2l3_3_q1 (joint295 base, 2026-10-31 00:08, refs/story/evidence/a2l3_wolves.log line 167088); its effects' own rows are done: [challenges_and_rewards.md](../challenges_and_rewards.md) "Its record is updated with how well it went (0 to 1) and the alignment it earned"; the village-lost run ends the same way |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It cannot block the story: freeing the creature does not wait for it | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Losing the village only means winning it back | pending | the town ownership reads are real; not checked in play |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second line after the wolves arrive belongs to the evil advisor in the text table, but only the good advisor has stepped out to say it | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The wait for the village to be taken over has no pause in its loop | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
