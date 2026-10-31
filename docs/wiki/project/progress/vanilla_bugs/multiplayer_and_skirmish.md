# Vanilla bugs: multiplayer and skirmish

Bugs in the original game's skirmish and network play and in the shipped skirmish land scripts
(`Scripts/Playgrounds/`).

**Bugs: 4 (soft-locks and lost progress: 1)**

## Soft-locks and lost progress

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Start Skirmish Game ends the current game at once without the confirmation the text table has for it ("Starting a Skirmish game end the current game. Do you wish to do this?") | The game in progress is thrown away without a question | Version 1.2 never shows the line | [skirmish.md](../multiplayer/skirmish.md) | undecided (not in this tree yet: the menu's Start Skirmish Game button does nothing, so the game goes on) |

## Wrong outcomes

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| The online winning conditions switch off the building goals (houses, wonders, new towns, converts) by default, with a developers' note that a bug was still to be fixed | Those goals are not offered for online games | The game's condition list | [firestorm.md](../multiplayer/maps/firestorm.md) | undecided (not in this tree yet: no online play) |

## Missing or wrong feedback

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| A god the computer takes over is named with a random line from the first 99 of the text table, which are villagers' grumbles, not names (unconfirmed that players saw it) | A rival god named something like "Work, work, work. I'm sick of it." | The naming reads the text table by index | [skirmish_opponents.md](../rival_gods/skirmish_opponents.md) | undecided (not in this tree yet: no network play) |

## Harmless

| Bug | What the player sees | Evidence | Found in | openblack |
|-----|----------------------|----------|----------|-----------|
| Each clan in the clan list carries a yes/no flag that the game stores but never reads | nothing visible | Checked in the Windows and Mac versions | [clans.md](../../not-in-the-original.md#multiplayerclans) | undecided (not in this codebase: the clan list is the online service's, bwgame-service) |
