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
game's text table. openblack never runs Land 3's control script (the land-loading command does nothing), and of what it
needs, snapshots and belief changes are still stubs in `src/CHLApi.cpp` (flocks, animals, villagers and dialogue work),
so every row is todo unless the notes say otherwise.

**Progress: 0/28 done, 0 partial — 0%**

## How it starts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started by the creature's prison when exactly two pillars are down and the Indian village is the player's; if the Indian village is won third, it never happens | todo | the prison's pillars follow town ownership, read through stubs (`GetTownWithId`), so it is never started |
| At the same moment, if the monk's quest is done, his Wonder gift is started | todo | follows from the same start; see [../silver_scrolls/the_shaolin.md](../silver_scrolls/the_shaolin.md) |
| It logs to its own story-log entry, separate from freeing the creature, and switches back to the creature's entry when it ends | todo | `Snapshot` is a stub |
| It waits 5 minutes, then waits until the Indian village is the player's (or gone) | todo | the town ownership read is a stub |
| If all three pillars are down by then, nothing happens and the entry is simply marked complete | todo | the pillars never fall and the log is a stub |

## The attack

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A pack is formed at a spot about 510 from the Indian village, towards the prison: wolves are made within 20 of it until there are 20, held loosely together (inner radius 5, outer 40) | todo | `FlockCreate`, `FlockAttach` and `CREATE` of animals are real (`ecs::script_containers`, `CreateScriptObject`); never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The pack sets off slowly (half speed) towards the Indian village | todo | `MoveGameThing` and the speed are handled for animals; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| A cut scene with one of the generic quest tunes: the camera watches from behind and follows the pack. Lethys: "You may have your few believers." Lethys: "But I can still command the beasts of the wilderness." | todo | `StartMusic`, the camera and `RunText` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera moves; the pack quickens a little (0.6) | todo | real camera natives and the speed; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## The monk helps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only if the monk's own quest has reached its end: the Tibetan town music plays, and after 4 seconds the monk appears near the pack's path facing the camera, playing his ambient animation five times | todo | `StartMusic` and `CREATE` of a villager are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Monk: "It seems you have need of me." Monk: "The wolves are possessed." | todo | `RunText` is real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The camera follows the pack again; wolves are taken from it until 8 are left. Each taken wolf, after 1 to 4 seconds, stops, a sparkle appears and it is replaced by a cow, which is let go | todo | `FlockDetach`, `SpecialEffectObject` and `ObjectDelete` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Monk: "But I think I can help you. I will try to destroy some of the wolves." Monk: "That is all." The monk disappears | todo | `RunText` and `ObjectDelete` are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Without the monk, the scene just lasts 12 seconds more and all 20 wolves come | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The story-log entry is started at nothing done, its reminder the good advisor's "The Village is coming under attack!" | todo | the log entry is a stub |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Kill every wolf of the pack before any of them gets within 25 of the spot at the edge of the village; the check is every 5 seconds | todo | `IdSize` and the distances are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| There is no time limit other than the wolves' walk | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |

## Failure: the wolves reach the village

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The pack is let loose; a cut scene flies the camera to the village | todo | real camera natives; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The story-log entry is marked complete (the source's own comment calls it "you have failed to save the town") | todo | the log is a stub |
| The good advisor steps out and points: "The wolves have reached the Village!" then "The Villagers are switching allegiance!" | todo | advisors and texts are real; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Lethys's belief in the Indian village is set to full and the player's to 0.3 | todo | `ObjectRelativeBelief` and `SetPlayerBelief` are stubs |
| This can lose the village, raising its prison pillar again | todo | the beliefs and the pillars are stubbed; see [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the last wolf of the pack is dead the entry is marked complete; there is no line and no reward | todo | the log is a stub |
| Either way the entry ends at complete with no alignment change, so the log cannot tell success from failure | todo | the log is a stub |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It cannot block the story: freeing the creature does not wait for it | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| Losing the village only means winning it back | todo | the town ownership reads are stubs |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The second line after the wolves arrive belongs to the evil advisor in the text table, but only the good advisor has stepped out to say it | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
| The wait for the village to be taken over has no pause in its loop | todo | the same script; never reached: Land 3 needs `LOAD_MAP`, and the prison only starts it when a pillar falls, which reads stubbed town ownership |
