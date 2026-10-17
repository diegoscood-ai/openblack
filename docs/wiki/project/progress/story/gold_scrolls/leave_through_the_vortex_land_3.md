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
(vortices, highlights, dialogue and cut scenes among them) is a stub in `src/CHLApi.cpp`; rows are todo unless the notes
say otherwise.

**Progress: 0/14 done, 1 partial — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started in the middle of Lethys's creed scene, right after "It is in a land you once knew. Use this Vortex." | todo | the quest's script calls only real natives (`LeaveThroughVortexL3`, no stubs); never reached: Land 3 needs `LOAD_MAP`, an empty native. See [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| An entry vortex is made in the lowland, about 300 from the prison | todo | `CREATE` of a vortex is ported (`ecs::vortex::Create` from `CreateScriptObject`); never reached: Land 3 needs `LOAD_MAP`, an empty native. See [../portals.md](../portals.md) |
| A gold scroll is made over it, 20 above the ground | todo | `CreateHighlight` and the height through `SetProperty` (YPos) are real; never reached: Land 3 needs `LOAD_MAP`, an empty native |
| It has its own challenge record but takes no story-log snapshot | partial | the same script runs (no snapshot call); the challenge log itself is not drawn in our tree |

## The reminder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Whenever the camera is within 100 of the scroll and the scroll is on screen, at most every 30 seconds and only when no cut scene is running, the evil advisor steps out, points at it and says "Let's go, Boss. Hit the Scroll and we're out of here." | todo | the shared reminder `ChallengeHighlightNotify` calls only real natives (`SpiritEject`, `SpiritPointPos`, `RunText`); never reached: Land 3 needs `LOAD_MAP`, an empty native |
| The reminder stops when the scroll or the vortex is clicked, and the scroll is switched active | todo | `GameThingClicked` is real and `SetActive` handles highlights; never reached: Land 3 needs `LOAD_MAP`, an empty native |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: the camera flies high above the vortex looking down into it | todo | `MoveCameraPosition`, `MoveCameraFocus` are real; never reached: Land 3 needs `LOAD_MAP`, an empty native |
| The screen fades to black over 8 seconds while the camera dives into the vortex | todo | `SetFade` and the camera flight are real; never reached: Land 3 needs `LOAD_MAP`, an empty native |
| The land is marked finished; its control script returns and every other script of the land is stopped | todo | `StopAllScriptsExcluding` works and the flag is plain script; never reached: Land 3 needs `LOAD_MAP`, an empty native |
| The story then loads Land 4 and its control script | todo | the land-loading command `LOAD_MAP` does nothing (see [../../scripts/land3_script.md](../../scripts/land3_script.md)) |
| The script sends no people through the vortex: it only plays the camera dive | todo | nothing crosses between lands in our tree; see [../portals.md](../portals.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If Lethys's last town had not been taken, Lethys counts as spared, which the advisors remark on when Land 4 begins | todo | the remark is in Land 4's arrival, which is never reached; how the spared flag is set was not traced here |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll waits for ever; nothing removes it or closes the vortex, so the land cannot be lost by delay | todo | the same script runs; never reached: Land 3 needs `LOAD_MAP`, an empty native |
| Next: the fourth land's arrival | todo | `LOAD_MAP` is empty; see [../land_4.md](../land_4.md) |
