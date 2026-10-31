# Leave through the vortex (Land 2)

The way out of Land 2 when the player missed following Lethys through his vortex: once Lethys holds no towns and his
temple is all but destroyed, the temple blows up, the vortex hidden under it opens again, the advisors say they must
follow the creature, and a gold scroll over the vortex takes the player through to Land 3. The game gives this scroll
no title and records no challenge log entry for it. The vortex itself (how it looks, sounds and what crosses) is in
[../portals.md](../portals.md#opening-the-exit-per-land); the land as a whole in [../land_2.md](../land_2.md).

**Land:** 2 · **Giver:** the good advisor, at a gold scroll over the vortex where Lethys's temple stood · **Script:** LeaveThroughVortexL2 · **Reward:** none (the way to Land 3) · **Repeatable:** no

Sources: the quest's script source (`LeaveThroughVortexL2.txt`) and the land's control script (`LandControl2.txt`),
checked against the compiled form in the shipped `challenge.chl`; the shared scroll-notify script; the land's map script
(`Land2.txt`); the game's English text table. openblack's state is judged on this tree: none of the 35 script functions
this quest needs only logs "not implemented" in `src/CHLApi.cpp` (`tools/checks/story/script_coverage.py`, 2026-10-30).
Land 2's control script runs when the story comes from Land 1 (the story's top script, `LandControlAll`, which also
loads Land 3 once the control script returns); a quest run starts this quest directly, without either, so only a story
run can show the land change. Every row below is pending unless the notes say otherwise.

**Quest runs:** the rows that pass in this page's quest runs of 2026-10-28 cite them, and are done where their effects' own rows are done; the runs, and what the other rows wait on, are under [Pending](#pending).

**Progress: 13/27 done, 0 chosen, 3 partial, 0 inferred — 53%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script checks every 9 seconds, while the land isn't already being left: Lethys must hold no towns at all and his temple must be at a tenth of its health or less | done | `LandControl2` reads the town totals through `GetPlayerTownTotal` (real) and `GetTownWithId` (real), and a temple's health through `GetProperty` (real for any object); quest_check --natural PASS, run l2b_exit (2026-10-30 20:57:10, refs/story/evidence/l2b_exit.log line 655841); done in their own rows: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of the citadel a script finds" |
| It runs once (an "exit vortex done" flag), and the control script waits for it | done | plain script logic, run by our VM; quest_check --natural PASS, story run l2b_exit (l2b d9ae25798, 2026-10-30, refs/story/evidence/l2b_exit.log line 656857) |
| In normal play it comes only after Khazar's death and Lethys's theft of the creature, when the player didn't follow him in time | done | the control script's order: Khazar's death, then the theft, then this quest; see [lethys_has_taken_our_creature.md](lethys_has_taken_our_creature.md) and [nemesis_no.md](nemesis_no.md#script-quirks); quest_check --natural PASS, story run l2b_exit (l2b d9ae25798, 2026-10-30, refs/story/evidence/l2b_exit.log line 655841) |
| Setting the "exit vortex done" flag ends the land's last gold scroll's updates and its wait | done | plain script flags, run by our VM. See [destroy_it.md](destroy_it.md); quest_check --natural PASS, story run l2b_exit (l2b d9ae25798, 2026-10-30, refs/story/evidence/l2b_exit.log line 655940) |
| Not determined what the temple-health check reads if the temple has already been destroyed outright | pending | undetermined in the original; `GetProperty` reads the health of a thing that still exists |

## The temple falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film starts with the camera close in front of Lethys's temple, drifting slowly towards it over 12 seconds | done | `SetCameraPosition`, `MoveCameraPosition` and `StartCameraControl` are real (`src/Camera/ScriptCamera`); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 5091); its effects' own rows are done: [script_camera.md](../../camera/script_camera.md) "A script starts camera control…", [script_camera.md](../../camera/script_camera.md) "Set the camera's position at once", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Glide the camera's focus to a point over a time" |
| Lethys's temple is blown up | done | `ObjectDelete` of a citadel heart is real (`DeleteByScript`); never reached in play; see [../losing_and_game_over.md](../losing_and_game_over.md#how-a-temple-is-destroyed); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 5092); its effects' own rows are done: [losing_and_game_over.md](../losing_and_game_over.md) "A script can destroy a temple the same way", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Finds an object of a kind within a radius of a position" |
| When the camera arrives, it cuts to a high view behind the temple site looking at the vortex spot, and sinks over 10 seconds | done | real camera natives (`SetCameraPosition`, `MoveCameraPosition`, `HasCameraArrived`); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 12051); its effects' own rows are done: [script_camera.md](../../camera/script_camera.md) ""camera ready": whether the camera has arrived", [script_camera.md](../../camera/script_camera.md) "Set the camera's position at once", [script_camera.md](../../camera/script_camera.md) "Set the camera's focus at once", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time" |
| The vortex opens in front of where the temple stood; the camera flies over 8 seconds to look out from it over the land | partial 60% | `CREATE` of a vortex is ported (`ecs::vortex`). See [../portals.md](../portals.md); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 18556); done in their own rows: [portals.md](../portals.md) "A vortex fades in over 7 seconds and is then fully open", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Glide the camera's focus to a point over a time"; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a vortex"; [portals.md](../portals.md) "A light map paints a glow on the ground around the vortex" |
| Both advisors step out; good: "Look! The Vortex has opened again!"; good: "I see! Lethys' Temple was hiding a Vortex. Let's go through!"; good: "We've got to get our Creature back."; evil: "Yeah. The poor guy. I really miss him."; good, clickable through: "We've no choice. We must follow him through the Vortex." | done | `SpiritEject` and `RunText` are real (advisors and dialogue texts in `src/Help`); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 34879); its effects' own rows are done: [advisors.md](../advisors.md) "Ejected, an advisor flies out from its corner into the middle of the view in a second", [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…", [advisors.md](../advisors.md) "The advisors' lines on Land 2's gold scrolls have their own recordings from the advisors' bank" |
| Both go home and the film ends | done | `SpiritHome` and the camera release are real; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 35677); its effects' own rows are done: [advisors.md](../advisors.md) "Sent home, it flies back to its corner in a second", [script_camera.md](../../camera/script_camera.md) "Ending camera control gives the camera back to the player" |

## The scroll

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A gold scroll appears 20 above the vortex | done | `CreateHighlight` and the height through `SetProperty` (YPos) are real; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 35685); its effects' own rows are done: [hand_in_scripts.md](../../hand/hand_in_scripts.md) "Scripts put a highlight", [scrolls_and_signs.md](../../interface/scrolls_and_signs.md) "A scroll can be set to draw at a height and turned on or off by scripts" |
| The player can wait as long as they like; the land goes on meanwhile | done | plain script wait; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 35850); plain script: nothing outside the script moves |
| Clicking the scroll or the vortex switches the scroll on and leaves | done | `GameThingClicked` is real and `SetActive` handles highlights; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 38348); done in their own rows: [scrolls_and_signs.md](../../interface/scrolls_and_signs.md) "A scroll can be set to draw at a height and turned on or off by scripts", [script_camera.md](../../camera/script_camera.md) "A script starts camera control…", [clicking_and_activating.md](../../hand/clicking_and_activating.md) "Tapping a challenge's marker" |

## Leaving

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A film: the camera flies to the start of the exit camera path over 4 seconds, runs the path, and after 2 seconds the screen fades to black over 2 seconds | done | `MoveCameraPosition`, `RunCameraPath` and `SetFade` are real. See [../portals.md](../portals.md#going-through); quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 42756); its effects' own rows are done: [script_camera.md](../../camera/script_camera.md) "Set or glide the camera to a named position from the game's camera library", [script_camera.md](../../camera/script_camera.md) "Glide the camera's position to a point over a time", [script_camera.md](../../camera/script_camera.md) "Run a recorded camera track by name", [cinematics.md](../../camera/cinematics.md) "Fade the picture to a colour over whole seconds…" |
| The vortex is marked closed and the land is marked as being left; the land's control loop ends | done | plain script flags; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 48369); its effects' own rows are done: [script_camera.md](../../camera/script_camera.md) "Ending camera control gives the camera back to the player" |
| The story's top script stops every other script and loads Land 3, which begins with the arrival through the vortex | done | `StopAllScriptsExcluding` and `LOAD_MAP` are real; Land 3 begins with its arrival (`GetThroughVortexL2`, `VortexEntry`): [so_you_couldnt_bear_to_be_without_your_creature.md](so_you_couldnt_bear_to_be_without_your_creature.md); quest_check --natural PASS, story run l2b_exit (l2b d9ae25798, 2026-10-30, refs/story/evidence/l2b_exit.log line 712572) |
| No villagers are sent through by this script; who follows the player is the vortex's business | pending | the In vortex writes what it takes to the crossing file and the next land's Out reads it back (`vortex_save`), and `LOAD_MAP` changes the land; not checked in play; see [../portals.md](../portals.md#what-crosses-and-what-is-left) |

## Success, failure and aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No challenge log entry is recorded for this scroll | done | the same script runs, so no entry either; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 49127); plain script: nothing outside the script moves |
| It can't fail; it waits for the click for ever | done | the same script runs; absent quest_check PASS, nothing matched from the log's start to the run's end, run a1q_vortex (2026-10-28 23:02:58, refs/story/evidence/a1q_vortex.log line 41280); played: the vortex clicked |
| It can soft-lock only before it starts: if Lethys keeps a town or his temple can't be brought below a tenth of its health, the land never ends | pending | depends on the town totals (`GetPlayerTownTotal`) and the temple's health read (`GET_PROPERTY` Health), and on Lethys's computer player, all real natives now; not checked in play |
| What it unlocks: Land 3 | done | `LOAD_MAP` of Land 3 from the story's top script; quest_check --natural PASS, story run l2b_exit (l2b d9ae25798, 2026-10-30, refs/story/evidence/l2b_exit.log line 712572) |

## Advisors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The good advisor does most of the talking and gives the scroll; the evil advisor has one line | done | advisors and texts are real; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 34879); its effects' own rows are done: [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…", [advisors.md](../advisors.md) "The advisors' lines on Land 2's gold scrolls have their own recordings from the advisors' bank" |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script plays no music or sound of its own | done | the same script runs, with no sound calls; the vortex's own sounds are made with it (`audio::vortex_sound`, from `src/ECS/Vortex.cpp`); absent quest_check PASS, nothing matched from the log's start to the run's end, run a1q_vortex (2026-10-28 23:02:58, refs/story/evidence/a1q_vortex.log line 41280); played: the vortex clicked |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll is never deleted; the land ends with it | pending | the same script runs; not yet reached in play: Land 2's story has not yet been run past its arrival since Khazar's first wait (`COMPUTER_PLAYER_READY`) became real |
| The vortex is 12 away from the one Lethys opened, though a source comment asks for them to be kept the same | done | the same script runs, so the same place; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 18554); its effects' own rows are done: [portals.md](../portals.md) "A vortex fades in over 7 seconds and is then fully open" |
| A source comment on the second line describes a different line ("Now that Lethys belief in this land is no longer protecting the vortex") from the one spoken | done | the same script runs, so the same line is said; quest_check PASS, run a1q_vortex (joint280, 2026-10-28 23:02:09, refs/story/evidence/a1q_vortex.log line 26338); its effects' own rows are done: [advisors.md](../advisors.md) "The line shows in a strip above the bottom of the screen…", [advisors.md](../advisors.md) "The advisors' lines on Land 2's gold scrolls have their own recordings from the advisors' bank" |

## Pending

The quest runs, made with the deployed exe (`_logs/deployed`: until 22:39 on 2026-10-28 the build deployed at 18:26, whose game is joint279's since joint279 changed only tests and docs; joint280's from then), read with `tools/checks/story/quest_check.py` against the quest's spec:

 | Run | Started | Exe | What it plays | quest_check |
 |-----|---------|-----|---------------|-------------|
 | `refs/story/evidence/a1q_vortex.log` | 2026-10-28 23:02:09 | joint280 | the land left through the vortex | 15 pass, 0 fail, 6 gate, 6 no spec |
 | `refs/story/evidence/l2b_exit.log` | 2026-10-30 20:57 | l2b d9ae25798 | the story from Land 1's skip branch: Land 2's control script under `LandControlAll`, Lethys's towns won and his temple's heart set to 0.05 by shortcuts, Khazar's death, the theft with the chance missed, the exit and the load of Land 3 | 21 pass, 0 fail, 6 no spec (`--natural`) |

- Rows that pass but wait on another row, which would finish them:
  - [clicking_and_activating.md](../../hand/clicking_and_activating.md) "Tapping a challenge's marker" (partial 80%)
  - [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a vortex" (partial 80%)
  - [portals.md](../portals.md) "A light map paints a glow on the ground around the vortex" (partial 50%)
- The start-gate rows close only from a natural run of the land, which reaches the quest the way the story does. The
  story run for them: `story_chain.sh` with the SkipBox answered 3 and the chapters
  `story.land1_skip_to_land2/story.land2_khazar_dies/story.land2_lethys_temple_falls/story.land2_leave_through_the_vortex`
  ([scenarios.md](../../../scenarios.md)), read with `--natural` (the spec's window opens at the quest's first line).
  Two of its steps are shortcuts for the player's play: Lethys's three towns won by a gift of belief, and his temple's
  heart set below a tenth of its life (with no towns left, the heart takes the player's blows itself:
  [losing_and_game_over.md](../losing_and_game_over.md) "The heart passes the harm on…"), so the row on the control
  script's check of Lethys's towns and temple, which the run reaches only through them, stays pending on it
  (refs/story/evidence/l2b_exit.log, 2026-10-30).
