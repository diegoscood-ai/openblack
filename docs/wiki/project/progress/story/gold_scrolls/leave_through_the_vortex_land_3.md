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

**Progress: 0/13 done, 0 chosen, 1 partial, 0 inferred — 0%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started in the middle of Lethys's creed scene, right after "It is in a land you once knew. Use this Vortex." | pending | the quest's script calls only real natives (`LeaveThroughVortexL3`, no stubs); not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real. See [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md) |
| An entry vortex is made in the lowland, about 300 from the prison | done | `CREATE` of a vortex is ported (`ecs::vortex::Create` from `CreateScriptObject`); quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 3444); now done too: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a vortex" |
| A gold scroll is made over it, 20 above the ground | done | `CreateHighlight` and the height through `SetProperty` (YPos) are real; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 3448); its effects' own rows are done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Creates a challenge marker (the bronze, silver or…", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Sets the height above the land of a highlight, of…" |
| It has its own challenge record but takes no story-log snapshot | done | the same script runs (no snapshot call); the challenge record is real (`Snapshot`, `UpdateSnapshot`) and the temple's challenge room lists the challenges (temple/challenge_room.md); quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 3446); its effects' own rows are done: [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Creates a challenge marker (the bronze, silver or…", [challenges_and_rewards.md](../challenges_and_rewards.md) "A started challenge is recorded with a title, a reminder and a picture" |

## The reminder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reminder stops when the scroll or the vortex is clicked, and the scroll is switched active | done | `GameThingClicked` is real and `SetActive` handles highlights; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 4898); now done too: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object (a dispenser, a gate and so on) on or off" |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene: the camera flies high above the vortex looking down into it | done | `MoveCameraPosition`, `MoveCameraFocus` are real; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 6922); done in their own rows: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object (a dispenser, a gate and so on) on or off"; [cutscenes.md](../cutscenes.md) "A cut scene takes the camera and brings the cinema bars in", [cutscenes.md](../cutscenes.md) "The camera glides to a place and focus over a time" |
| The screen fades to black over 8 seconds while the camera dives into the vortex | done | `SetFade` and the camera flight are real; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 9264); its effects' own rows are done: [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Fades the screen to a colour over a time", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a…", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's point of view to a position…", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Whether the fade has finished", [cutscenes.md](../cutscenes.md) "The picture fades to a colour and back between shots", [cutscenes.md](../cutscenes.md) "The camera glides to a place and focus over a time" |
| The land is marked finished; its control script returns and every other script of the land is stopped | pending | `StopAllScriptsExcluding` works and the flag is plain script; not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The story then loads Land 4 and its control script | pending | `LOAD_MAP` works, but play has not yet got here: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real (see [../../scripts/land3_script.md](../../scripts/land3_script.md)) |
| The script sends no people through the vortex: it only plays the camera dive | pending | the same script runs, with no sending; what the vortex carries is written to the crossing file and read back by the next land's Out (`vortex_save`), not checked in play; see [../portals.md](../portals.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If Lethys's last town had not been taken, Lethys counts as spared, which the advisors remark on when Land 4 begins | pending | the remark is in Land 4's arrival, which is never reached; how the spared flag is set was not traced here |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll waits for ever; nothing removes it or closes the vortex, so the land cannot be lost by delay | done | the same script runs; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 3489); now done too: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object" |
| Next: the fourth land's arrival | pending | not yet reached in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real; see [../land_4.md](../land_4.md) |
