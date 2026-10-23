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

**Progress: 0/28 done, 0 chosen, 0 partial, 0 inferred — 0%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly two pillars are down and the Indian village is the player's; if the Indian village is won third, it never happens | pending | the prison's pillars follow town ownership, read through `GetTownWithId` (real); not checked in play |
| At the same moment, if the monk's quest is done, his Wonder gift is started | pending | follows from the same start; see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry when it ends | pending | `Snapshot` is real |
| It waits 5 minutes, then waits until the Indian village is the player's (or gone) | pending | the town ownership read (`GET_PROPERTY` Player of a town) is real; not checked in play |
| If all three pillars are down by then, nothing happens and the entry is simply marked complete | pending | the pillars never fall and the challenge record is real (`Snapshot`, `UpdateSnapshot`) |

## The attack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is formed at a spot about 510 from the Indian village, towards the prison: wolves are made within 20 of it until there are 20, held loosely together (inner radius 5, outer 40) | pending | `FlockCreate`, `FlockAttach` and `CREATE` of animals are real (`ecs::script_containers`, `CreateScriptObject`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The pack sets off slowly (half speed) towards the Indian village | pending | `MoveGameThing` and the speed are handled for animals; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| A cut scene with one of the generic quest tunes: the camera watches from behind and follows the pack. Lethys: "You may have your few believers." Lethys: "But I can still command the beasts of the wilderness." | pending | `StartMusic`, the camera and `RunText` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera moves; the pack quickens a little (0.6) | pending | real camera natives and the speed; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the Tibetan town music plays, and after 4 seconds the monk appears near the pack's path facing the camera, playing his ambient animation five times | pending | `StartMusic` and `CREATE` of a villager are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Monk: "It seems you have need of me." Monk: "The wolves are possessed." | pending | `RunText` is real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The camera follows the pack again; wolves are taken from it until 8 are left. Each taken wolf, after 1 to 4 seconds, stops, a sparkle appears and it is replaced by a cow, which is let go | pending | `FlockDetach`, `SpecialEffectObject` and `ObjectDelete` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Monk: "But I think I can help you. I will try to destroy some of the wolves." Monk: "That is all." The monk disappears | pending | `RunText` and `ObjectDelete` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Without the monk, the scene just lasts 12 seconds more and all 20 wolves come | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The story-log entry is started at nothing done, its reminder the good advisor's "The Village is coming under attack!" | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Kill every wolf of the pack before any of them gets within 25 of the spot at the edge of the village; the check is every 5 seconds | pending | `IdSize` and the distances are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| There is no time limit other than the wolves' walk | pending | the same script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |

## Failure: the wolves reach the village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack is let loose; a cut scene flies the camera to the village | pending | real camera natives; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| The story-log entry is marked complete (the source's own comment calls it "you have failed to save the town") | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| The good advisor steps out and points: "The wolves have reached the Village!" then "The Villagers are switching allegiance!" | pending | advisors and texts are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; the prison starts it when a pillar falls (the town ownership it reads is real) |
| Lethys's belief in the Indian village is set to full and the player's to 0.3 | pending | `ObjectRelativeBelief` and `SetPlayerBelief` are real; not checked in play |
| This can lose the village, raising its prison pillar again | pending | the belief natives are real and the pillars follow the town's owner (real reads); not checked in play; see [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the last wolf of the pack is dead the entry is marked complete; there is no line and no reward | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |
| Either way the entry ends at complete with no alignment change, so the log cannot tell success from failure | pending | the challenge record is real (`Snapshot`, `UpdateSnapshot`) |

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
