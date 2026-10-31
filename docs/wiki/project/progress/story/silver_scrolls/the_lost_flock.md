# The Lost Flock

A first-land silver scroll: a shepherd has dozed off and nine of his sheep have strayed all over the island. Each sheep
brought back to his field joins his flock; five back earns a large food reward, all ten back earns a sheep the player
can take as their creature. Killing the shepherd, or letting five sheep die in his field, ends it badly.

**Land:** 1 · **Giver:** the Norse shepherd, from the house beside his field next to the player's village · **Script:** TheLostFlock · **Reward:** a large food reward at five sheep; a sheep creature to swap to at ten · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on this tree: of the 83 script functions
the quest and the scripts it starts need (including the creature-swap script it hands over to), 13 only log "not
implemented" in `src/CHLApi.cpp`, among them the snapshot, reward, creature-action and swap commands; creating villagers
and animals works (`Create`). The land's control script starts this quest with the start-up box's second, third and
fourth answers (see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and it is not checked in game; rows
are pending unless the notes say otherwise. (The function counts above are from an earlier tree; at this tree the
regenerated native table shows no stub on these scripts, `GET_PROPERTY` reads health and the player's hand's hold,
`CREATE` makes creatures, and `MOVE_GAME_THING` and `CREATURE_DO_ACTION` drive a creature.)
The land as a whole is in [../land_1.md](../land_1.md), the script's function coverage in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 16/80 done, 0 chosen, 15 partial, 0 inferred — 29%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script is started early in the land, right after the opening (the family leading the player home and the temple), before the creature is chosen | partial 40% | with the second, third or fourth tutorial answer `LandControl1` starts it before the creature is chosen; with the first answer the land's control script never gets that far (the intro does not finish); finished by [land1_script.md](../../scripts/land1_script.md) "Loading the land starts the story's control script"; still open: no natural run's log is cited for this start gate |
| On starting, it places the nine lost sheep at once, so they are out in the world long before the scroll shows | done | done in its own system: animal/livestock.md "An animal belongs to the town its land script names…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; CHL `CREATE` makes the sheep (`AnimalArchetype`) |
| Each lost sheep stands still where it was placed (the script holds it) until the player brings it back; it does not wander off | partial 60% | CHL `CREATE` holds a script's animal in IN_SCRIPT, with a small domain and the nearest town, as the original does (`animal_script_rules::CreateScriptAnimal`, [map-script-reader.md](../../../../scripts/map-script-reader.md#animals-and-birds)); unverified against v1.42 in play, not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes an animal"; still open: no quest run's PASS is cited for it |
| The lost sheep are: one hidden in a flock of five pigs in another farmer's field, one behind the creature gates, one near the hermit's cottage, one at the sculptor's house, one in a beach gully, one at the throwing-stones range, one by the lost brother's spot, one on a peninsula near where the crowd gathers, and one on the hill above the town | done | done in its own system: animal/livestock.md "An animal belongs to the town its land script names…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; the nine places come from the script |
| The pigs' flock that hides the sheep keeps within 5 to 8 of its spot | done | the script reaches it; done in their own rows: animal/livestock.md "A flock keeps together round its leader…"; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `FlockCreate`, `ChangeInnerOuterProperties` and `FlockAttach` work (`src/ECS/ScriptContainers.cpp`), and so does `PopulateContainer`, which makes the five pigs |
| Every lost sheep is always drawn, however far away, so it can be spotted from a distance | done | done in its own system: scripts/challenge_natives_objects_and_world.md "Turns a cut-scene special effect on an object on or off…" (done); rendering/level_of_detail.md "How far the camera can see before the world is cut off…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `ThingJcSpecial` feature 19 does nothing because openblack fades no object with distance, so they are drawn anyway (`src/ECS/SuperVillager.cpp`) |
| Each lost sheep bleats with its own sample (nine different bleats) every 6 to 8 seconds while it is alive and more than 10 from the shepherd's field; it falls silent once there, once dead or once the quest is over | done | done in its own system: audio/sound_effects.md "Scripts play, stop and ask about sounds…" (done); audio/sound_effects.md "The story's own sound effects bank…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; the shared bleat script's natives work (`PlaySoundEffect`, `GetDistance`) |
| An empty flock is made at the shepherd's house, keeping its members within 5 to 12 | done | done in its own system: scripts/challenge_natives_objects_and_world.md "Creates an empty flock at a position…" (done); animal/livestock.md "Scripts make, move and read flocks…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `FlockCreate`, `ChangeInnerOuterProperties` work |
| The scroll waits until the creature's trainer has taught the leash and tied the creature to a house; only then does a silver scroll appear over the shepherd's house | partial 60% | with the fourth answer the land raises the flag at once (the lesson is skipped) and `CreateHighlight` makes the scroll (`src/ECS/ScriptHighlight.cpp`); not checked in game; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Creates a challenge marker", [the_creatures_learning.md](../gold_scrolls/the_creatures_learning.md) "The step ends when the creature is tied to that tree"; still open: no quest run's PASS is cited for it |
| Clicking the scroll makes it active (opened) and the introduction begins | done | done in its own system: interface/scrolls_and_signs.md "Tapping a scroll starts its challenge…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `GameThingClicked` and `SetActive` on a scroll work |
| Later, the guide's naps suggest unfinished silver scrolls: if the Explorers' scroll has been opened and isn't the outstanding last suggestion, and the flock's scroll hasn't been opened (or the flock was the last suggestion and isn't done), the good advisor says "You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it." and the camera sweeps up and over to the shepherd's field | pending | the creature guide has not yet appeared in a run (it is skipped when the creature training is); see [../creature_guide.md](../creature_guide.md) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with the first generic script theme | done | done in its own system: camera/cinematics.md "The cinema bars slide in to a 16:9 picture…" (done); audio/music.md "The story's pieces…" (done); audio/music.md "Scripts start and stop a piece of music…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; widescreen and script music work (`SetWidescreen`, `StartMusic`) |
| The shepherd (a Norse shepherd villager) appears at his house with one sheep, which is sent to his field | done | done in its own system: villager/life_cycle.md "Town villagers created straight into a town by script…" (done); villager/special_villagers.md "The special kinds are only made by challenge scripts…" (done); animal/livestock.md "Scripts make, move and read flocks…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; CHL `CREATE` makes the villager and the sheep, `MoveGameThing` sends it |
| He walks very slowly out to a spot in front of the house (speed 0.08) with a tired-eyed walk while the camera follows him | done | the script reaches it; done in their own rows: scripts/challenge_natives_objects_and_world.md "Sets the speed of a villager or an animal…"; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `SetProperty` speed, `MoveGameThing`, `OverrideStateAnimation` (villagers) and `FocusFollow` work |
| The quest is recorded: title "The Lost Flock", 0 done, alignment 0, reminder "There are still sheep missing!" (good advisor) | done | the script reaches it; done in their own rows: story/challenges_and_rewards.md "A started challenge is recorded with a title, a reminder and a picture…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `Snapshot` is real; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He faces the camera and yawns: "By the great gods, I have been asleep!" | done | the script reaches it; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `SetFocus` (villager), `RunText` and the shepherd's script animation (`SetScriptUlong`, `SetScriptState`) work; its line's strip and voice are done (story/advisors.md) |
| Throwing up his arms: "I was just counting my sheep when I dozed off." | done | the script reaches it; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; as above; its line's strip and voice are done (story/advisors.md) |
| Looking about: "Please find my straying sheep." | done | the script reaches it; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; as above; its line's strip and voice are done (story/advisors.md) |
| If the sheep on the hill above the town is still there, the camera cuts to it for about four seconds, drifting, and it bleats; then cuts back to the shepherd. Otherwise the camera simply turns back to him | done | done in its own system: camera/script_camera.md "Set the camera's position at once…" (done); camera/script_camera.md "Glide the camera's position to a point over a time…" (done); audio/sound_effects.md "Scripts play, stop and ask about sounds…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; the camera natives and `PlaySoundEffect` work |
| The shepherd stands in despair, weeping, then looks over to his field | done | done in its own system: villager/held_and_scripted.md "A script plays an animation on a villager…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; villager script animations work |
| The camera pulls back to look over the field as he walks to it at speed 0.15; the music stops | done | the script reaches it; done in their own rows: scripts/challenge_natives_objects_and_world.md "Sets the speed of a villager or an animal…"; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `MoveCameraPosition`, `MoveCameraFocus`, `HasCameraArrived`, `StopMusic` work |
| He becomes the leader of his flock with his one sheep, and wanders around the field (within about 6) | done | the script reaches it; done in their own rows: animal/livestock.md "A flock keeps together round its leader…"; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `FlockAttach` makes him leader; his wandering state has its place and radius (`SetScriptStatePos`, `SetScriptFloat` and state 199; checked against v1.42 on 2026-10-20) |
| Both advisors pop out. Evil: "Well, they were going to be slaughtered anyway. I say we waste the wandering mutton." Good: "But if we find and return them, we'll have a dyed-in-the-wool follower." Evil: "Wow. The saintly one made a joke." | done | the script reaches it; quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `SpiritEject`, `RunText`, `TextRead` work; its line's strip and voice are done (story/advisors.md) |
| Any loose sheep already within 50 of his flock when the scene ends (ones the player brought early) join the flock at once | done | done in its own system: scripts/challenge_natives_objects_and_world.md "Finds an object of a kind within a radius of a position…" (done); animal/livestock.md "Scripts make, move and read flocks…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; the shepherd's health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`: his own life), so the old reason (he read as dead when the introduction ended) is gone |

## Bringing the sheep back

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player brings the sheep back by any means (carrying them in the hand, throwing them, the creature carrying them); the scripts only count the shepherd's flock | pending | not yet reached in play; the hand and throwing work, and the flock counting natives (`IdSize`, `CallNear`) too |
| A returned sheep joins the shepherd's flock through the game's own flocking: an animal on its own looks about itself for a flock of the same kind to join, as long as the joined flock stays within the species' flock size | partial | lone animals look for a flock to join (`LookForFlocksInSpiral`, `src/ECS/AnimalAI.cpp`); not yet reached in play. Our wiki differs: a lone animal looks for a flock to merge with only after it lands (dropped or thrown), and a script-made flock takes it in whatever the size ([animals](../../../../beings/animal-life.md#flocks-age-and-smoke)); finished by [animal_behaviour.md](../../animal/animal_behaviour.md) "An animal picked up leaves its flock for one of its own"; still open: no quest run's PASS is cited for it |
| If the flock has only the shepherd left in it, the script itself adds any sheep within 50, checking every 0.7 seconds | pending | not yet reached in play |
| The second, third and fourth sheep in the flock each get "Many thanks, higher power. Keep them coming, please." (once each) | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 97901) |
| If more than three were already there when the introduction ended, these lines start from that count | done | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 97901); the script's own logic, on working natives |

## Five sheep: the first reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the flock reaches the shepherd and five sheep (four found), the script waits until the shepherd is not held by the hand or the creature, puts him back home if he is more than 50 away, and gathers the flock there | partial 60% | `InCreatureHand` is real and the in-hand read gives the player's hand's hold (`GetProperty` in `src/CHLApi.cpp`); quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 116273) |
| A widescreen scene: the shepherd walks to a spot by his house while the camera closes in over 5 seconds | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 116444) |
| He faces the camera and shifts about; the record becomes 99% done with alignment +0.4 and the reminder "The shepherd seems to think we have returned all his sheep." (good advisor) | partial 60% | `UpdateSnapshot` is real; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 124635) |
| "Thank you. Looks like you've returned my flock!" | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 124640) |
| He points, and a large food reward appears beside him (it appears on the ground, not falling from the sky, and belongs to no town) | partial 60% | `CreateReward` is real; the reward itself: [../rewards.md](../rewards.md); quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 132194) |
| "I can't offer much, but take this with my thanks." | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 134060) |
| He turns to the reward and the camera swings onto it | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 135859) |
| When the reward is clicked, its own help line is spoken if the dialogue is free | pending | never reached; `GetHelp` is real (`src/CHLApi.cpp`); see [../rewards.md](../rewards.md) |
| He goes back to wandering in his field; the silver scroll is removed | partial 60% | `ObjectDelete` works; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 139141) |
| For the guide's purposes the quest now counts as done, though it carries on | done | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 233463); the script's own logic, on working natives |

## More sheep, up to all ten

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first sheep after the reward: "Ooh. You found another sheep. I forgot there was more than five." | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 149675) |
| Each one after that cycles through "Good gracious. Another sheep. Superb work.", "You've found another sheep." and "Another sheep. Well I never. This gets better and better." | pending | never reached |
| A line is only said when the flock is bigger than the biggest it has been since the reward, so a sheep that dies and is replaced earns no new line | pending | never reached |

## Ten sheep: the sheep creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the flock is the shepherd and all ten sheep, the script waits until he is not held and puts him in his field if he is more than 50 from it | done | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 207844); the script's own logic, on working natives |
| A widescreen scene with the fourth epic script theme; the shepherd stands by his house as the camera moves in over 4 seconds | done | the music command works (`StartMusic`); the scene is reached; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Brings the black cinema bars in or out", "Glides the camera's eye to a position over a number of seconds"; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 207858); their own rows are done |
| "All my sheep are back! Thank you!" | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 207861) |
| A white success sparkle shows in the field for 3 seconds and a sheep creature appears in it | partial 60% | `SpecialEffectPosition` works and CHL `CREATE` makes creatures now; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 212513) |
| "Behold! A sheep. You may use him as your Creature." as he looks to the field and bows, then turns to the camera and prays | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 216661) |
| The record becomes fully done with alignment +0.6, keeping the reminder "There are still sheep missing!" | partial 60% | `UpdateSnapshot` is real; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 224564) |
| The camera pulls back over the field; the shepherd leaves his flock, the flock is let go, and he joins the Norse village as an ordinary villager | partial 60% | `FlockDetach`, `ReleaseFromScript` work; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 227565) |
| The sheep creature is then offered to swap for (see below) | partial 60% | `SwapCreature` is real; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 227705) |

## Swapping to the sheep

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The sheep creature can't be picked up and keeps looking at the camera, with a silver scroll just above its head | partial 60% | `CREATE` makes the sheep creature now; the swap: [../challenges_and_rewards.md](../challenges_and_rewards.md); quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 227713) |
| Whenever the camera is within 100 of it and it is on screen, the evil advisor pops out at most once a minute, points and says "Swap your Creature with this one if you want." | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 232844) |
| Clicking the scroll or the sheep with the player's creature more than 50 away: evil advisor: "You'll need to bring our Creature, Boss." and the scroll comes back | pending | never reached |
| With the creature within 50, the scroll goes, the creature's leash is let go and it walks to within 15 of the sheep (given at most 5 seconds); the two creatures look each other over | pending | never reached; `MoveGameThing` walks the creature under the script's control (`CreatureMindSystem::MoveForScript`) and `CREATURE_DO_ACTION` is real (examining by looking, row 20, has an executor) |
| Every 30 seconds the good advisor asks "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel." | pending | never reached |
| Clicking the sheep confirms; clicking the player's own creature, or waiting 110 seconds, cancels and the scroll comes back | pending | never reached |
| On confirming, a two-creature camera frames them, both sparkle, point at each other and swap with the creature-swap sound; the player's creature becomes the sheep | pending | never reached; `StartDualCamera` works, `SwapCreature` is real; pointing at an object (`CREATURE_DO_ACTION` row 163) has an executor now (`src/Creature/CreaturePlanActions.cpp`) |
| Evil advisor: "Great, Boss. If you want your old Creature back, just return here later." | pending | never reached |
| The old creature stays there, can't be picked up, and is offered in the same way, so the player can swap back and forth as often as they like | pending | never reached; the offer never closes: the script's own time limit (60 to 120 seconds) is made but never checked |
| The offer lasts until the land's scripts are stopped (leaving for the next land) | pending | never reached |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 2 seconds the script looks for a dead sheep within 30 of the field that is neither held nor flying; it is removed with a fade and counted | pending | never reached; `CallNearInState` (finding a dead sheep) is real |
| The first dead sheep: "Oh dear. One of my sheep has died. Sheep happens, I suppose." | pending | never reached |
| The second to fourth: "Oh drat. Another sheep dead." | pending | never reached |
| The fifth: "Too many sheep have died for me to make a living. I'm going to quit." then "You're a powerful being, Holy One, but you're not the best shepherd ever."; the record is closed fully done with alignment -0.6 and the quest ends | pending | never reached; `UpdateSnapshot` is real |
| Any dead sheep counts, including ones that died elsewhere and were dropped in the field | pending | `CallNearInState` is real (any dead sheep in the cells within 30 is found); never reached in play |
| If the shepherd dies (and is not in flight), the evil advisor pops out and points at him: "I never liked him anyway. He gave me the creeps."; the good advisor looks at him: "I thought nothing gave you the creeps."; evil: "You do, you big creep."; the record is closed fully done with alignment -1 | partial | the shepherd's health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`), not 0; not checked in play that this ending now waits for his death; `SpiritPointGameThing`, `LookGameThing`, `StopPointing` work, `UpdateSnapshot` is real; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of an object", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Makes an advisor point at an object", "Makes an advisor look at an object", "Stops an advisor pointing", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Updates the challenge log picture with what is going on"; still open: no quest run's PASS is cited for it |
| When the guide's storm begins, an unfinished, started flock quest is closed (fully done, alignment 0) and its script stopped | pending | the guide's storm has not yet been reached in a run (the guide is skipped when the creature training is, and a game without the skip has not yet been run that far) |
| A flock quest whose scroll was never clicked is simply left; the sheep stay where they are | partial 60% | nothing happens until the click; not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player has clicked an object"; still open: no quest run's PASS is cited for it |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After completion the shepherd lives on as one of the Norse village's people, and his ten sheep roam free | partial 60% | quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 227565) |
| The large food reward stays where it appeared until opened | pending | never reached; `CreateReward` is real |
| Nothing else in the land's later story depends on this quest beyond the guide's scroll suggestions | pending | the guide's suggestions have not yet been reached in a run |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every line of the shepherd is the "man" narrator voice; the advisor lines are the good and evil advisors' own voices | partial 60% | `RunText` shows the text and plays its narrator's voice (`src/Help/HelpSystem.cpp`); the lines after the introduction are never reached; not checked in game; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Clicking the active scroll again speaks its reminder through the advisor who owns it (the good advisor here) | pending | the shared reminder script needs `Snapshot`, which is real |
| Music: the first generic script theme for the introduction, the fourth epic theme for the all-sheep scene; the five-sheep scene has none | done | the music command plays these tracks (`StartMusic`, `src/Audio`); the introduction's can play, the all-sheep scene is reached; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music"; quest_check PASS, run l1s_lost_flock (land1, 2026-10-31 07:20, refs/story/evidence/l1s_lost_flock.log line 207853); its own row is done |
| Sounds: nine different sheep bleats for the lost sheep, and the creature-swap sound | partial 40% | the bleats play through `PlaySoundEffect`; the creature-swap sound is never reached; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks"; still open: no quest run's PASS is cited for it |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The five-sheep reward only fires when the flock is exactly the shepherd and five sheep; if two sheep join at the same moment and the count jumps past it, no reward or later line ever comes and the quest can't be finished (only the storm closes it) | pending | never reached; follows from the script |
| The ten-sheep ending likewise needs exactly the shepherd and ten sheep; one more sheep at the same moment skips it and only the "another sheep" lines play | pending | never reached |
| Since only ten sheep exist for the quest, one dead sheep makes the ten-sheep ending impossible unless some other sheep wanders into the flock | pending | never reached |
| Dead sheep are counted only within 30 of the field, so sheep killed elsewhere never count | done | done in its own system: scripts/challenge_natives_objects_and_world.md "Finds a living thing of a kind in a state within a radius…" (done); quest_check PASS, story.s5_lost_flock natural run, 2026-10-27; `CallNearInState` is real; in play |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the sheep, the shepherd, his flock count and the record | pending | openblack's saved games (loaded only from the debug bar so far) put the scripts' machine back (`Game::LoadSavedGame`), but of the world's objects only the land's buildings, piles, forests and trees and a few others yet, not the quest's people, animals, creatures or scrolls ([saved games](../../../../engine/saved-games.md#pending)) |
