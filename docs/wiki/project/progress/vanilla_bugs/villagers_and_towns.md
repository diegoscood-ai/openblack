# Vanilla bugs: villagers and towns

Bugs in the original game's villager and town code and data: daily work, carried tools and their sounds, and the town
football pitch.

**Bugs: 14 (soft-locks and lost progress: 0)**

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The football score tally and the creature's "my side scored" test credit the side the villagers mourn for | The tally (seen only in the debug text) and the creature's reaction go to the wrong team, while the celebrations follow the villagers | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches; the progress file says to reproduce it as-is) |
| The belief a town gains when a match ends reads a match count that nothing in the football code sets | Undetermined: the belief reward may be fixed at its minimum | Read from the pitch code | [football.md](../town/football.md) | undecided (not in this tree yet: no football matches) |
| With football switched off, a pitch placed by a script never runs a match, but relaxing villagers can still go to it and wait | Villagers standing at an idle pitch (undetermined what ends their wait) | The town only updates its pitch while football is on; nothing stops villagers joining | [football.md](../town/football.md) | differs: no match is run, but relaxing villagers do not go to the pitch either (the relaxation pick counts no pitch) |
| A trader disciple fetching from its own store asks for the kind it already carries more of, not the kind its trade town wants | A trader carrying wood keeps fetching wood for a town that wants food, until its load runs out | The trader's pick-up reads its load through a call that also rewrites the kind it asked for (raffclar's vanilla-bugs row 2) | [../villager/disciples.md](../villager/disciples.md) | kept (unverified against v1.42); asking for the trade town's kind is the fix, for a "fix vanilla bugs" switch |
| A villager weighs a hunter by its speed in metres a second divided by the raw walking speed of the hunter's info row (a whole distance a turn), so the speed barely counts | Villagers take fright at every hunter with about half the table's urgency, whether it sleeps off its meal nearby or charges at them | Read in the exe (the villager's predator priority; [villager-reactions.md](../../../beings/villager-reactions.md#villagers-fleeing-from-predators)), with raffclar's reading of the info value (his vanilla-bugs row 65) | [../villager/reactions.md](../villager/reactions.md) | kept (the rule pinned to the exe's numbers in `test/test_predator_flee.cpp`) |

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
| A challenge script's desire boost checks only that the desire is at most 16, as a signed number, so a negative desire is written before the town's 17 boosts, over other fields of the town | nothing: no shipped script gives a negative desire (they give 0, 5, 6, 8, 9 and 16) | Read in runblack.exe v1.42: `GScript::SetTownDesireBoost` 0x6FE650 tests `0x10 < desire` signed and writes Town +0xD4 + 4 × desire ([the quests' natives](../../../scripts/script-commands.md#the-quests-natives-read-against-the-original)) | [town_desires.md](../town/town_desires.md) | not reproduced: a negative desire writes nothing (`town_desire::ScriptSetTownDesireBoost`) |
| No tree record gives fruit-tree logs | nothing visible: the fruit branch is never seen in a villager's hand | The tree records in `info.dat` | [tools_and_carried_items.md](../villager/tools_and_carried_items.md) | reproduced (no tree record gives them) |
| A town's disciple counts are moved as a villager's disciple changes, and only from its town at that moment, so a disciple that changes town leaves the old town's count one too high until the town's next turn counts them afresh | nothing lasting: the counts are right again at each town's turn | raffclar's reading of the disciple code (his deviation row 9, which counts them afresh every turn instead) | [../villager/disciples.md](../villager/disciples.md) | kept (unverified against v1.42); counting afresh every turn is the fix, for a "fix vanilla bugs" switch |
| A villager set to run from a fallen shield is given nothing to watch, so it gives the reaction up on the next turn | Villagers caught in the open when their spiritual shield breaks neither run nor look back, unless they were watching something already (another shield, a flying rock, a fire), which they then go on watching | raffclar's reading in Ghidra of the panic set-up (his vanilla-bugs row 62) | [../villager/reactions.md](../villager/reactions.md) | kept (unverified against v1.42) |
| Hiding from a fallen shield stores no state to go back to, and the hiding states drop a villager that watches nothing | A villager that was watching nothing gives the hide up at once and stands idle (the empty state) until a need moves it on; one that was hides 15 to 30 s and then stands idle the same way | the original's state table (ReactionValidate on rows 236 and 237) and raffclar's row 63; his tree has no validate there, so his villagers always hide | [../villager/reactions.md](../villager/reactions.md) | kept (unverified against v1.42); a candidate for the "fix vanilla bugs" switch |
