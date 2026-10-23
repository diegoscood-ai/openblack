# Leave Through the Vortex (Land 3)

The third land's last gold scroll: when Lethys hands over the creed he opens a vortex in the lowland and a gold scroll
hangs over it. Clicking the scroll takes the player through to the fourth land, leaving Lethys alive unless his last
town has already been taken. The scroll has no title in the game's text and makes no story-log entry of its own.

**Land:** 3 · **Giver:** Lethys opens the vortex; the evil advisor points out the scroll · **Script:** LeaveThroughVortexL3 · **Reward:** the way to Land 4 · **Repeatable:** no

The quest that opens it: [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md).
The vortex itself, its look and how people are carried through: [../portals.md](../portals.md). The land:
[../land_3.md](../land_3.md); the next land's arrival: [../land_4.md](../land_4.md).

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table. openblack never runs Land 3's control script, though none of the 28 commands this quest needs
(vortices, highlights, dialogue and cut scenes among them) is a stub in `src/CHLApi.cpp`; rows are pending unless the notes
say otherwise.

**Progress: 0/14 done, 0 chosen, 1 partial, 0 inferred — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started in the middle of Lethys's creed scene, right after "It is in a land you once knew. Use this Vortex." | pending | the quest's script calls only real natives (`LeaveThroughVortexL3`, no stubs); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real. See [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| An entry vortex is made in the lowland, about 300 from the prison | pending | `CREATE` of a vortex is ported (`ecs::vortex::Create` from `CreateScriptObject`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real. See [../portals.md](../portals.md) |
| A gold scroll is made over it, 20 above the ground | pending | `CreateHighlight` and the height through `SetProperty` (YPos) are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| It has its own challenge record but takes no story-log snapshot | partial 50% | the same script runs (no snapshot call); the challenge log itself is not drawn in our tree; re-measured 2026-10-21: as the row above: the same script |

## The reminder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever the camera is within 100 of the scroll and the scroll is on screen, at most every 30 seconds and only when no cut scene is running, the evil advisor steps out, points at it and says "Let's go, Boss. Hit the Scroll and we're out of here." | pending | the shared reminder `ChallengeHighlightNotify` calls only real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The reminder stops when the scroll or the vortex is clicked, and the scroll is switched active | pending | `GameThingClicked` is real and `SetActive` handles highlights; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: the camera flies high above the vortex looking down into it | pending | `MoveCameraPosition`, `MoveCameraFocus` are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The screen fades to black over 8 seconds while the camera dives into the vortex | pending | `SetFade` and the camera flight are real; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The land is marked finished; its control script returns and every other script of the land is stopped | pending | `StopAllScriptsExcluding` works and the flag is plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The story then loads Land 4 and its control script | pending | `LOAD_MAP` works, but play has not yet got here: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real (see [../../scripts/land3_script.md](../../scripts/land3_script.md)) |
| The script sends no people through the vortex: it only plays the camera dive | pending | nothing crosses between lands in our tree; see [../portals.md](../portals.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If Lethys's last town had not been taken, Lethys counts as spared, which the advisors remark on when Land 4 begins | pending | the remark is in Land 4's arrival, which is never reached; how the spared flag is set was not traced here |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll waits for ever; nothing removes it or closes the vortex, so the land cannot be lost by delay | pending | the same script runs; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Next: the fourth land's arrival | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../land_4.md](../land_4.md) |
