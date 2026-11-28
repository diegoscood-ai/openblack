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

**Progress: 13/13 done, 0 chosen, 0 partial, 0 inferred — 100%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| It is started in the middle of Lethys's creed scene, right after "It is in a land you once knew…" (`HELP_TEXT_L3_LETHYS_GIVES_CREED_05`) | done | the quest's script calls only real natives (`LeaveThroughVortexL3`, no stubs); quest_check --natural PASS, run a2l3c_2 (land3 control, 2026-11-06 15:03:52, refs/story/evidence/a2l3_control.log line 925864), a run started at the land's control script itself (quest=LandControl3, as loading the land does, timeline quest.land_control_3), spec tools/checks/story/quests/leave_through_the_vortex_land_3.txt; capped at 60%: the run reaches it through testbed shortcuts (the three villages and Lethys's last town won by belief given to their towns, as the player's winning would give it). See [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md); natural: quest_check --natural PASS on spec tools/checks/story/quests/leave_through_the_vortex_land_3.txt in the user's game of 2026-11-15 (refs/story/played/u_20261115_205033.log line 3073806, task FreeTheCreature; a save of their own play loaded, no testbed or quest line), so the shortcut runs' cap is lifted |
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
| The land is marked finished; its control script returns and every other script of the land is stopped | done | read from the script (2026-11-24): the exit script sets the land-finished flag (refs/chl/LeaveThroughVortexL3.txt:105758), the control script's wait on it ends and it returns (refs/chl/LandControl3.txt:113247-113251), and the story's top script stops every script outside its own file (refs/chl/LandControlAll.txt:155497-155498); openblack runs the script as shipped |
| The story then loads Land 4 and its control script | done | `LOAD_MAP` works; testbed run, the chain started at Land 3 by the story= key (2026-11-27, refs/story/evidence/land3_control_j778/land3_control.log, its Testbed line 1763): the exit scene ends (line 964046), LandControlAll stops every other script (line 964161) and loads Land 4 (lines 964162, 965006), and Land 4's control script runs its set-up and arrival (SetupLand4 from line 965500, BeginLand4 from line 965532); before that, not reached in play: in the user's game of 2026-11-15 (refs/story/played/u_20261115_205033.log) this quest plays its exit scene to its end (task LeaveThroughVortexL3 from line 3070567: the camera and the fade, then FinishedLand3 set), but that game's Land 3 was itself loaded from the debug bar (line 3693), whose Load Island starts LandControl3 alone, so no LandControlAll loads Land 4; about 1100 turns later Land 4 is loaded from the debug bar too (line 3100156); the testbed story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log reach Land 3 (see [../../scripts/land3_script.md](../../scripts/land3_script.md)) |
| The script sends no people through the vortex: it only plays the camera dive | done | read from the script (2026-11-24): the exit script only makes the vortex and its highlight, waits for the click and plays the camera dive and fade before setting the finished flag; it sends nobody into the vortex (refs/chl/LeaveThroughVortexL3.txt:105672-105767); openblack runs the script as shipped |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If Lethys's last town had not been taken, Lethys counts as spared, which the advisors remark on when Land 4 begins | done | read from the script (2026-11-24): `LethysKilled` is a global of the story (declared in refs/chl/StandardReminder.txt), set to 0 as the prison script starts (refs/chl/FreeTheCreature.txt:109502) and to 1 only as its last statement, after Lethys's end scene (:110307); leaving through the vortex first stops that script, so it stays 0; Land 4's arrival reads it and has the advisors say 5279 and 5280 when it is 0, 5281 otherwise (refs/chl/beginland4.txt:123937-123962). A land change keeps the scripts' globals: `LOAD_MAP` restarts no script (test `LandLoadScripts.OnlyANewGameOrAPlaygroundsStartRestartsTheScripts`, [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Loads the next land", done). The spared lines are seen in quest run a2l4c_3 (refs/story/evidence/a2l4_control.log lines 190256 and 196921, task BeginLand4) |

## Soft-locks and what comes next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll waits for ever; nothing removes it or closes the vortex, so the land cannot be lost by delay | done | the same script runs; quest_check --natural PASS, run a2l3_vortex3 (2026-10-30 19:53:02, refs/story/evidence/a2l3_vortex3.log line 3489); now done too: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object" |
| Next: the fourth land's arrival | done | testbed run, the act made by the timeline (2026-11-28): refs/story/evidence/land3_control_j778/land3_control.log loads land4.txt at line 964162 and Land 4's control script starts at turn 0 at line 965006; finished by [../land_4.md](../land_4.md) "The land loads and its control script begins" and "The player comes through the vortex and finds the land ruined", both done; not checked against the user's games of 2026-11-15 that play Land 3's quests by their own scripts (refs/story/played/u_20261115_195059.log and u_20261115_205033.log, which enter Land 3 by loading saved games and, in the second, from the debug bar's Load Island at line 3693; and the testbed story runs refs/story/evidence/slotpeak_l1_to_l2.log and l2b_exit.log); see [../land_4.md](../land_4.md) |
