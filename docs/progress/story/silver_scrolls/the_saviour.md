# The Saviour

A first-land silver scroll: a freak wave has swept a fisherman's wife's husband and four other men into the sea off
the player's village. The men drown one by one over five minutes, out where the hand can't reach, so the player must
send the creature to wade out and carry them ashore. Saving all five, or killing all five, earns a strength creature
miracle dispenser; the scroll's alignment follows how many were saved, drowned or killed.

**Land:** 1 · **Giver:** a fisherman's wife on the shore by the player's village · **Script:** CreatureSavingPeople · **Reward:** a "Strengthen" creature miracle dispenser on the shore, only for saving all five with the wife alive or for killing all five · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on this tree: of the 66 script functions
the quest and the scripts it starts need, 6 only log "not implemented" in `src/CHLApi.cpp`, among them the help reads,
the held-by-creature check and the snapshot commands; creating villagers works (`Create`), and so does the game-time
read (`DllGettime`). The land's control script starts this quest only in a game that skips the creature training (the
start-up box's fourth answer; see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and it is not checked
in game; rows are pending unless the notes say otherwise. (The function counts above are from an earlier tree; at this
tree the regenerated native table shows no stub on these scripts, and `GET_PROPERTY` reads health and the player's
hand's hold.) The land as a whole is in [../land_1.md](../land_1.md), the script's function coverage in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md), dispensers in [../rewards.md](../rewards.md).

**Progress: 0/51 done, 0 chosen, 27 partial, 0 inferred — 21%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest once the trainer's lesson of tying the leash to a house is done, before the guide is met (or straight after setup in a game that skips the creature training) | partial 40% | only with the fourth tutorial answer, which skips the creature training: `LandControl1` starts it straight after `HermitMain`; with the other answers the land control script never gets there; re-measured 2026-10-21: the natives it calls work; the land's control script does not get there in a run |
| A woman villager (the fisherman's wife) is made on the shore by the player's village; she can't be picked up, moved or killed for now | partial 60% | `CREATE` makes the villager (`VillagerArchetype`); `SET_ID_PICKUPABLE`, `SET_ID_MOVEABLE`, `SET_INDESTRUCTABLE` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| An anti-influence circle of radius 30 is put over the sea where the men will be, so the hand can't act there | partial 60% | `INFLUENCE_POSITION` works (`src/Magic/Script/CHLInfluence.cpp`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Until the scroll is clicked, the wife faces the camera and waves for attention every 2 seconds | partial 60% | `SET_FOCUS`, `SET_SCRIPT_ULONG` and `SET_SCRIPT_STATE` on a villager work (`Alert`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| A silver challenge scroll stands on the shore beside her | partial 60% | `CREATE_HIGHLIGHT` works (`src/ECS/ScriptHighlight.cpp`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| While the camera is within 100 of the scroll and it is on screen, at most every 30 seconds the good advisor steps out, points at it: "Look. Something for you to do here." | partial 60% | the shared notice script runs on working natives (`DLL_GETTIME`, `GAME_THING_FIELD_OF_VIEW`, the advisors, `RUN_TEXT`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Clicking the scroll or the shore spot starts the quest; a 5-minute timer is started | partial 60% | `GAME_THING_CLICKED` and `CREATE_TIMER` (`ecs::script_timer`) work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The film starts; the wife is always drawn and in high detail | partial 60% | `THING_JC_SPECIAL` (our tree fades nothing with distance) and `SET_HIGH_GRAPHICS_DETAIL` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Five Norse men are made out in the sea, each with the drowning animation looping and always drawn, however far | partial 60% | `CREATE` makes the men and `SET_SCRIPT_ULONG` loops the drowning clip on a villager; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Generic script music 3 plays | partial 60% | `START_MUSIC` works (`src/Audio`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The evil advisor appears: "What's all the fuss?" as the camera glides (3 s) to look from the shore over the sea | partial 60% | `SPIRIT_APPEAR`, `SPIRIT_DISAPPEAR`, the camera moves, `SET_FOCUS` and the clips on a villager, `RUN_TEXT` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The evil advisor disappears; the camera turns further out to sea while the wife looks at the first man and despairs | partial 60% | `SPIRIT_APPEAR`, `SPIRIT_DISAPPEAR`, the camera moves, `SET_FOCUS` and the clips on a villager, `RUN_TEXT` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The wife faces the camera and despairs again: "A freak wave has just struck! My husband and four others are drowning! Please help them!" | partial 60% | `SPIRIT_APPEAR`, `SPIRIT_DISAPPEAR`, the camera moves, `SET_FOCUS` and the clips on a villager, `RUN_TEXT` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| She walks slowly (half speed) to a spot further along the shore | partial 60% | `MOVE_GAME_THING` and `SET_PROPERTY` speed on a villager work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Two seconds later the scroll is entered in the challenge list at 0% with the reminder "My gosh! There are still people to save!" | pending | `SNAPSHOT` keeps the record now |
| The camera comes in low over the beach; the evil advisor: "Er, let's see. Who do we know who's tall enough to wade out to them?" | partial 60% | the camera, the advisors, the clips and the text work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The good advisor steps out; once the wife has reached her spot she faces the sea and despairs in a loop: good advisor: "Don't muck around. Let's get our Creature out there as fast as possible." | partial 60% | the camera, the advisors, the clips and the text work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The wife goes back to normal detail and can now be picked up, moved and killed; the music stops | partial 60% | the flags, `SET_HIGH_GRAPHICS_DETAIL` and `STOP_MUSIC` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The 5-minute timer is set again from full as the film ends | partial 60% | `SET_TIMER_TIME` works; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## Saving the men

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The men drown one after another, the last-placed first: at about 79, 119, 199, 249 and 299 seconds into the five minutes | partial | the timer and each man's script run on working natives; each man's health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`); not checked in play |
| A man drowns while he is still within 5 of where he started (and alive), or is anywhere in the drowning state, when his time comes: he plays the drowned death and is removed | partial | the drowning test (`GET_PROPERTY` drowning, `ecs::IsDrowning`), the death clip and `OBJECT_DELETE` work; the "alive" test reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| A man counts as out of the water once he is more than 5 from his start, not drowning, and neither in the hand, flying through the air, nor in the creature's hand | pending | the in-hand read (`src/CHLApi.cpp:1679`), the flying read (`src/CHLApi.cpp:1671`) and `IN_CREATURE_HAND` (`src/CHLApi.cpp:3658`) are real; not checked in play |
| A man brought out alive joins the village and is handed back to the game | pending | health is read now (`src/CHLApi.cpp:1596`), so a man brought out alive no longer counts as killed; `GET_TOWN_WITH_ID` is real, so `FLOCK_ATTACH` has its village; not checked in play |
| If a saved man is killed before the quest ends, the evil advisor says "I like it, Boss. One less human to deal with." | pending | health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`); not checked in play |
| A man who leaves the water dead, or vanishes, without having drowned (dropped, thrown, eaten) is killed by the player: the evil advisor says "I like it, Boss. One less human to deal with." | pending | health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`); not checked in play |
| The quest ends when no man is left in the water, or when the timer runs out | partial | the timer end works (`GET_TIMER_TIME_REMAINING`); the count of men left reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| The hand can't reach the men for the anti-influence, so the creature, tall enough to wade out, is the way to save them (the advisors say so) | partial | the anti-influence works; the creature wading out and carrying people is not wired in our tree |

## The ending

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A second after the end the count is made: saved men, drowned men, and the rest as murdered | pending | the tally reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| The scroll's alignment is (saved − murdered) × 0.2 − drowned × 0.1, e.g. +1 for all saved, −0.5 for all drowned, −1 for all killed; the scroll goes to 80% with that alignment | pending | `UPDATE_SNAPSHOT` is real, and the tally reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| If the last man to leave the water drowned, the scroll waits to be clicked again, with the good advisor's notice "Your godly attention is required here, Leader." | pending | `UPDATE_SNAPSHOT` is real, and the tally reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| The closing film: if the wife has been killed, a man (her brother) is made on the shore with a woman witness beside him, and he takes her place | pending | the wife's health is read now (`src/CHLApi.cpp:1596`), so a living wife no longer reads as killed; not checked in play |
| The camera turns to face the wife (or brother) from 10 away; they and the witness face it | partial | `MOVE_CAMERA_TO_FACE_OBJECT` and `SET_FOCUS` on a villager work; not checked in game |
| If the wife was killed, her brother first: "My sister asked for your help and you killed her!" despairing | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| All five saved and the wife alive: "Oh, I'm so grateful. We'll get some extra worshipping done." then the evil advisor: "Huh. Yeah, you're grateful now." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| All five saved but the wife killed: the witness despairs: "At least some survived. We're a little thankful for that." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| Some saved: "At least some survived. We're a little thankful for that."; and if any were killed, the witness (if there is one) shakes her fist: "You killed those that didn't drown. I'm shocked beyond words. Almost." then despairs: "We'll remember that. Such acts do not go un-noticed." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| None saved, some killed: "You killed those that didn't drown. I'm shocked beyond words. Almost." / "We'll remember that. Such acts do not go un-noticed.", then both advisors come out: evil "I feel gutted." / good "Give it a rest." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| None saved, none killed (all drowned): "At times like this I wish we had a choice of gods to worship." then the advisors' "I feel gutted." / "Give it a rest." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| All five killed: "I saw that! It was murder!" then the advisors' "I feel gutted." / "Give it a rest." | pending | the branch follows health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| The scroll is completed (100%) with the same alignment, and removed | pending | `UPDATE_SNAPSHOT` is real; removing the scroll (`OBJECT_DELETE`) works |
| The wife (or her brother) joins the village and is handed back to the game; the anti-influence over the sea is removed | pending | `GET_TOWN_WITH_ID` is real, so the village is found; not checked in play; the anti-influence removal works |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Only all five saved with the wife alive, or all five killed, earn the reward; any mixed result, or all saved with the wife dead, earns nothing | pending | the tally reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| A "Strengthen" creature miracle dispenser is built on the shore by the scroll, turned half round, recharging every 5 minutes | partial | `CREATE_WITH_ANGLE_AND_SCALE` makes dispensers, `SET_MAGIC_PROPERTIES` and `SET_TIMER_TIME` work (`src/Magic/Script/CHLWorship.cpp`); its condition reads health, real now (`src/CHLApi.cpp:1596`); not checked in play |
| A film with the reward sting: the camera glides to the dispenser; the evil advisor steps out | partial | the shared dispenser script runs on working natives (`PLAY_SOUND_EFFECT`, camera, `CREATE_HIGHLIGHT`, advisors); not checked in game |
| If it is the first dispenser given by a film in the game: a did-you-know scroll ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") is placed by it, a signpost appears next to it telling "A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."; the evil advisor points: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." pointing at the signpost | partial | the shared dispenser script runs on working natives (`PLAY_SOUND_EFFECT`, camera, `CREATE_HIGHLIGHT`, advisors); not checked in game |
| Otherwise the evil advisor points: "Nice. Another of those cool Miracle Dispensers." | partial | the shared dispenser script runs on working natives (`PLAY_SOUND_EFFECT`, camera, `CREATE_HIGHLIGHT`, advisors); not checked in game |
| Then the dispenser's own help lines for its miracle are spoken by whichever advisors own them | pending | `GET_FIRST_HELP` and `GET_LAST_HELP` give a spell dispenser's help text; not checked in play |

## Quirks and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved man who is killed before the quest ends still counts as saved in the tally | pending | the tally reads health, real now (`src/CHLApi.cpp:1596`); the creature carrying men is not wired |
| The fist-shaking and despairing witness only exists when the wife was killed, so in other endings those animations don't play | pending | the tally reads health, real now (`src/CHLApi.cpp:1596`); the creature carrying men is not wired |
| A man still held by the creature when the five minutes run out isn't counted as saved, so he counts as murdered | pending | the tally reads health, real now (`src/CHLApi.cpp:1596`); the creature carrying men is not wired |
| A saved game keeps the men, the wife, the timer and the scroll | pending | our tree has no saved games |
