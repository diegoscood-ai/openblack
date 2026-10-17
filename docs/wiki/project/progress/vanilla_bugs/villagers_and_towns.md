# Vanilla bugs: villagers and towns

Bugs in the original game's villager and town code and data: daily work, carried tools and their sounds, and the town
football pitch.

**Bugs: 8 (soft-locks and lost progress: 0)**

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The football score tally and the creature's "my side scored" test credit the side the villagers mourn for | The tally (seen only in the debug text) and the creature's reaction go to the wrong team, while the celebrations follow the villagers | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches; the progress file says to reproduce it as-is) |
| The belief a town gains when a match ends reads a match count that nothing in the football code sets | Undetermined: the belief reward may be fixed at its minimum | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches) |
| With football switched off, a pitch placed by a script never runs a match, but relaxing villagers can still go to it and wait | Villagers standing at an idle pitch (undetermined what ends their wait) | The town only updates its pitch while football is on; nothing stops villagers joining | [football.md](../town/football.md) | differs: no match is run, but relaxing villagers do not go to the pitch either (the relaxation pick counts no pitch) |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Both the hammering clip and the sledgehammer clip carry the sledgehammer sound | Hammering sounds like a sledgehammer; the separate hammer sound is never used by a villager | The clip sound events in `Data/SmallSounds.SAS` | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | reproduced (the clips' sound events, `src/Audio/Services/AnimationSounds.cpp`) |
| The digging sounds belong to two farmer clips that the farming states never play | Sowing and digging up crops are silent | Clip sound events against the farming states' clips | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | reproduced (the farming states play the silent clips) |
| When the football leaves the pitch, the game compares the two sides' first-listed players and sends the one further from the ball to fetch it (may be intended) | The player furthest away walks the long way to the ball while everyone else stands paused | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches) |
| A football spectator who leaves the match keeps their place in the crowd's wave until they are gone | A gap where the leaving spectator's turn in the wave comes round | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches) |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| No tree record gives fruit-tree logs | nothing visible: the fruit branch is never seen in a villager's hand | The tree records in `info.dat` | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | reproduced (no tree record gives them) |
| A villager set to run from a fallen shield is given nothing to watch, so it gives the reaction up on the next turn | Villagers caught in the open when their spiritual shield breaks neither run nor look back, unless they were amazed by another shield still standing | raffclar's reading in Ghidra of the panic set-up (his vanilla-bugs row 62) | [../villager/reactions.md](../villager/reactions.md) | kept (unverified against v1.42) |
| Hiding from a fallen shield stores no state to go back to, and the hiding states drop a villager that watches nothing | A villager that was not amazed by another shield gives the hide up at once and stands idle (the empty state) until a need moves it on; one that was hides 15 to 30 s and then stands idle the same way | the original's state table (ReactionValidate on rows 236 and 237) and raffclar's row 63; his tree has no validate there, so his villagers always hide | [../villager/reactions.md](../villager/reactions.md) | kept (unverified against v1.42); a candidate for the "fix vanilla bugs" switch |
