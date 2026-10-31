# The Hermit

A first-land silver scroll: a hermit on a hillside refuses to worship a god until he sees a huge enough creature. Bring
a big creature and he converts, tells of a miracle seed under a rock and the player gets a water miracle dispenser; damage
his hut instead and he buys the player off with a water miracle, then burns the village store in revenge; kill him and
the advisors are appalled.

**Land:** 1 · **Giver:** the hermit, at his hut on a hillside away from the village · **Script:** HermitMain · **Reward:** a water miracle dispenser (impressed), or a water miracle reward from the sky (hut damaged) · **Repeatable:** no

Sources: the land's challenge scripts (the original source text, checked against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack is judged on this tree: of the 77 script functions
the quest and the scripts it starts need, 10 only log "not implemented" in `src/CHLApi.cpp`, among them the creature's
actions, the villager's wandering place, the snapshot and reward commands; creating the hermit and the dispenser works
(`Create`). The land's control script starts this quest only in a game that skips the creature training (the start-up
box's fourth answer; see [../../scripts/land1_script.md](../../scripts/land1_script.md)), and it is not checked in game;
rows are pending unless the notes say otherwise. (The function counts above are from an earlier tree; at this tree the
regenerated native table shows no stub on these scripts, `GET_PROPERTY` reads health and the player's hand's hold,
`CREATURE_DO_ACTION` runs every row that has an executor, and `MOVE_GAME_THING` walks a creature under the script's
control.)
The land as a whole is in [../land_1.md](../land_1.md), the script's function coverage in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 19/89 done, 0 chosen, 31 partial, 0 inferred — 31%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script starts after the creature's trainer has taught it to eat, the slap-and-stroke lesson and the introduction to the leash (or straight after the creature is chosen if the training is skipped); the Immersion Mushrooms quest starts just before it | done | with the fourth tutorial answer `LandControl1` starts it straight away (training skipped), just after the mushrooms; with the other answers the land's control script never gets that far; quest_check --natural PASS, run l2b_exit (2026-10-30 20:57:10, refs/story/evidence/l2b_exit.log line 3961); its effects' own rows are done: [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Finds an object of a kind at a position" |
| Five seconds later a silver scroll appears over the hermit's hut and a bonfire is placed in front of it | done | done in its own system: interface/scrolls_and_signs.md "A land script puts a scroll up at a place or over a character…" (done); interface/scrolls_and_signs.md "Gold scrolls mark the story's challenges and silver ones the side challenges…" (done); physics/fire.md "A bonfire burns for ever and never hurts…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; CHL `CREATE` makes the bonfire and `CreateHighlight` the scroll (`src/ECS/ScriptHighlight.cpp`) |
| Clicking the scroll makes it active and the rest begins | done | done in its own system: interface/scrolls_and_signs.md "Tapping a scroll starts its challenge…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `GameThingClicked` and `SetActive` on a scroll work |
| Only then is a rock placed on the hill above the hut | done | done in its own system: nature/rocks_and_features.md "A rock a challenge script makes as a rock goes into the physics at once…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; CHL `CREATE` makes the rock |
| A bronze did-you-know scroll is put next to the rock: "If your Creature picks up rocks the exercise will make him stronger." | done | done in its own system: interface/scrolls_and_signs.md "A bronze scroll kind…" (done); interface/scrolls_and_signs.md ""Did you know" scrolls in the world give a tip when tapped…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `CreateHighlight` and `HighlightProperties` make did-you-know scrolls; bronze scrolls: ../../interface/scrolls_and_signs.md |
| The hut's health is set to full | done | the script reaches it; done in their own rows: scripts/challenge_natives_objects_and_world.md "Sets the health of an object but a pile, a tree or a crop field…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `SET_PROPERTY` health sets the hut's life (`SetProperty` in `src/CHLApi.cpp`, `SetHealthByScript`) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit (the hermit villager) appears at his hut; he can't be picked up, moved or hurt during the scene | done | the script reaches it; done in their own rows: physics/explosions.md "Indestructible things are spared" (done) and "Things in a script while a cut-scene holds the screen…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; CHL `CREATE` makes the villager and the three flags work (`SetIdPickupable`, `SetIdMoveable`, `SetIndestructable`) |
| A widescreen scene with the hermit's theme: he walks out of his hut, the camera following him over 7 seconds, then to his spot by the fire | done | done in its own system: camera/cinematics.md "The cinema bars slide in to a 16:9 picture…" (done); audio/music.md "The story's pieces…" (done); villager/held_and_scripted.md "A script moves a villager to a place…" (done); camera/script_camera.md "Make the camera's focus follow a thing…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; widescreen, music, `MoveGameThing` (villagers), `SetFocusFollow` and the camera moves work |
| The camera closes in on him and slowly tilts up over 35 seconds as he talks to it, gossiping: "As I see it, there ain't no need for me to bow before you, son." | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `SetFocus` (villager), his script animations (`SetScriptUlong`, `SetScriptState`) and `RunText` work; its line's strip and voice are done (story/advisors.md) |
| Unimpressed: "I ain't seen diddly from you which impresses me one itty bit." | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; as above; its line's strip and voice are done (story/advisors.md) |
| "Gods have huge Creatures. That's what my momma always told me." | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; as above; its line's strip and voice are done (story/advisors.md) |
| Very close up, unimpressed: "And until I see a big enough one, you don't mean nothing to me." | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; as above; its line's strip and voice are done (story/advisors.md) |
| The quest is recorded: title "The Hermit", 0 done, alignment 0, reminder "This Hermit still needs to be impressed by your Creature." (good advisor) | done | the script reaches it; done in their own rows: story/challenges_and_rewards.md "A started challenge is recorded with a title, a reminder and a picture…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `Snapshot` is real; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He wanders about his spot (within about 6) and the camera returns to where it was; the music stops | done | done in its own system: villager/held_and_scripted.md "A script makes a villager wander round a point…" (done); camera/script_camera.md "A script stores the camera's position and focus and restores them later…" (done); audio/music.md "Scripts start and stop a piece of music…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `SetScriptState` works and so do his wandering's place and radius (`SetScriptStatePos`, `SetScriptFloat` and state 199; checked against v1.42 on 2026-10-20); the camera return and `StopMusic` work |
| Both advisors pop out. Good: "Ah. Don't leave him as a lost soul. Show him your Creature." Evil: "Tchoh. Let him die alone. It's what he wants. The hick." Evil: "You don't have to justify yourself to him." Evil: "Come on. You've got better things to destroy." | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `SpiritEject`, `RunText`, `SpiritHome` work; its line's strip and voice are done (story/advisors.md) |
| Afterwards he can be picked up, moved and hurt | done | the script reaches it; its effects' own rows are done: physics/explosions.md "Indestructible things are spared" (done) and "Things in a script while a cut-scene holds the screen…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; the flags work; but see the death scene below |

## The hermit's wanderings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every second the script notes where he is (or that the creature is holding him) | done | his health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`: his own life), so the old reason (he read as dead after the introduction) is gone; `InCreatureHand` is real; quest_check PASS, run l1s_hermit (land1, 2026-10-31 07:14, refs/story/evidence/l1s_hermit.log line 11402); the script's own logic, on working natives |
| If he is more than 75 from his spot, not flying, not held and alive, and the dialogue is free, he walks back; he tries again every 10 seconds, and starts wandering again once within 15 | pending | not yet reached in play; `CreateTimer`, `MoveGameThing`, `IsDialogueReady` work, `InCreatureHand` and the in-hand read (`GetProperty` in `src/CHLApi.cpp`) are real |
| This stops once he has set off to take revenge after the hut (see below) | pending | not yet reached in play |

## Showing him the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 31 passes of its loop the script reads the creature's size and checks the hermit | done | the script reaches it; done in their own rows: scripts/challenge_natives_objects_and_world.md "Reads the scale of a villager, an animal or a creature…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `GET_PROPERTY` scale of a creature gives the size its body is drawn at (`GetProperty` in `src/CHLApi.cpp`, `ecs::object::GetScale`) |
| A creature larger than size 0.4, within 50 of him, while he is within 50 of his spot and not held, flying or in the creature's hand, impresses him | pending | not yet reached in play; the creature's scale (`GetProperty` in `src/CHLApi.cpp`), the held and flying reads (`GetProperty` in `src/CHLApi.cpp`, `GetProperty` in `src/CHLApi.cpp`) and `InCreatureHand` are real |
| Smaller than 0.2 and within 15: the first time, a widescreen scene with his theme: the creature's leash is let go and it is walked up to him; suspense music sting; unimpressed: "Call that a Creature? I could swat him flat!"; the creature looks sad | pending | not yet reached in play; `CREATURE_DO_ACTION` is real now (a row with an executor runs) and `MOVE_GAME_THING` walks the creature (`CreatureMindSystem::MoveForScript`) |
| The same size again, after the creature has been more than 50 away and come back within 15: "That durned critter ain't no bigger than the last time I seed him!" and the creature shows off | pending | not yet reached in play |
| Between 0.2 and 0.4 and within 15: the first time "Ha! Not the largest beast in the land, is he?" and the creature looks embarrassed | pending | not yet reached in play |
| Between 0.2 and 0.4 again, after it has been away more than 50, within 30: "That durned critter ain't no bigger than the last time I seed him!" and the creature looks angry | pending | not yet reached in play |
| Each kind of taunt is played at most twice; during them he can't be picked up, moved or hurt | partial | the flags work; the taunts are not yet reached in play; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Whether the player can move an object", "Makes an object impossible to destroy"; still open: no quest run's PASS is cited for it |
| A creature of exactly 0.2 or 0.4 gets no taunt | pending | not yet reached in play |

## Impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with his theme: the hut is made whole; the creature's leash is let go and it walks to stand before him; the hermit walks to his spot | partial 60% | the script reaches it; finished by creature/leash.md "Scripts can tie a creature's leash to an object, back to the hand, or take it off…" (partial 85%); scripts/challenge_natives_objects_and_world.md "Sets the health of an object but a pile, a tree or a crop field…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; quest_check PASS, run l1s_hermit (land1, 2026-10-31 07:14, refs/story/evidence/l1s_hermit.log line 62927) |
| Suspense sting; he faces the creature, prays three times: "Yes. Now that is one worthy Creature. I'll get worshipping!" | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; its line's strip and voice are done (story/advisors.md) |
| The camera turns to the creature, which does its summoning gesture | done | the script reaches it; done in its own row: creature/animation.md "Scripts can play a static or individual animation on a creature…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `CREATURE_DO_ACTION` is real now (a row with an executor runs) |
| If the seed under the rock hasn't been found: he points at the rock: "I done seen a firefly a-heading under that there rock at break of dawn." as the camera rises to show it | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; the scripted seed stands in for the real mechanic: fireflies hide in trees and rocks at dawn and leave a seed when lifted ([../../nature/fireflies.md](../../nature/fireflies.md)); its line's strip and voice are done (story/advisors.md) |
| "Now being as your Creature's so mighty, I be a-wondering whether he can move that rock." as the camera closes on the rock | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; its line's strip and voice are done (story/advisors.md) |
| "And see what the fiery little critter's doing under there." (waits for the player) | done | the script reaches it; quest_check PASS, story.s2_hermit natural run, 2026-10-27; its line's strip and voice are done (story/advisors.md) |
| The record becomes 60% done, alignment +0.4, reminder "Look under the rock where the Hermit saw the fireflies." | done | the script reaches it; done in their own rows: story/challenges_and_rewards.md "Its record is updated with how well it went…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `UpdateSnapshot` is real |
| If the seed was already found, the record goes straight to fully done, alignment 0, with that reminder | pending | never reached |
| A water miracle dispenser is given beside his spot (see Reward) | done | the script reaches it; done in their own rows: story/rewards.md "A miracle dispenser can be the reward instead of a chest…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27 |
| The player gains influence round the hermit (radius 4) and round his hut (radius 20) | done | done in its own system: worship/influence.md "Scripts make influence rings round places and objects…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; (`InfluenceObject` works) |
| The camera returns, the creature is given back and the music stops | done | done in its own system: camera/script_camera.md "A script stores the camera's position and focus and restores them later…" (done); creature/decision_making.md "A creature under a script's control does nothing of its own until released…" (done); audio/music.md "Scripts start and stop a piece of music…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27 |
| When the seed is found (or he dies) the record is updated to fully done with the reminder "This Hermit still needs to be impressed by your Creature." | pending | `UpdateSnapshot` is real |
| He then walks into his hut and is gone | pending | never reached |

## The seed under the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start, if the rock is in place, a watcher waits for it to be moved more than 5 from its spot, or picked up by the hand or the creature; this can happen before or after the hermit is impressed | done | the watcher runs from the start (`GetDistance`); a rock held by the hand or the creature is noticed now (the in-hand read, `GetProperty` in `src/CHLApi.cpp`, and `InCreatureHand`); finished by [challenge_natives_game_flow.md](../../scripts/challenge_natives_game_flow.md) "Gives the distance between two positions", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads whether a thing is in the player's hand", [challenge_natives_creature.md](../../scripts/challenge_natives_creature.md) "Whether a creature is holding an object in its hand"; quest_check PASS, run l1s_hermit (land1, 2026-10-31 07:14, refs/story/evidence/l1s_hermit.log line 10448); their own rows are done |
| A one-shot "strong" creature miracle seed then appears where the rock was, with a target effect for 10 seconds | partial | CHL `CREATE` makes the one-shot seed and `SpecialEffectPosition` works; not checked in game; seeds: ../../miracles/dispensers_and_seeds.md; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe", "Starts a special effect (sparkles", [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The Hermit's rock on Land 1"; still open: no quest run's PASS is cited for it |
| If the hermit is alive and within 50 of his spot: a widescreen scene; the creature (if within 30 of the rock) is sent to the bottom of the hill; good advisor: "Great! A Miracle Seed! Well, done, Mighty One." | partial | the camera and dialogue natives work, and `MoveGameThing` now walks the creature under the script's control (`CreatureMindSystem::MoveForScript`); not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Brings the black cinema bars in or out", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| The camera rises and turns to the hermit, who gossips: "Them fireflies turn theirselves into Miracle Seeds at dawn, I reckon." | partial | not checked in game; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Good advisor: "This is interesting news. We should watch out for fireflies and see where they hide at dawn." | partial | not checked in game; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Otherwise: the creature (if within 100) is sent down the hill; good advisor: "Golly. A Miracle Seed. How did that get there?"; evil: "The fireflies hide and turn themselves into Miracle Seeds at dawn. Duh!"; good: "Oh. Of course. Now that is worth knowing, Leader." | partial | not checked in game; `MoveGameThing` now walks the creature under the script's control (`CreatureMindSystem::MoveForScript`); finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |

## Damaging the hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any damage to the hut (its health below full) while he is alive and the dialogue is free sets off this branch, whoever caused it | pending | the hut's health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`: below 1 once damaged); not checked in play |
| He can't be picked up until he is put down; a second later he can be picked up, moved and hurt again | partial | the flags work; the branch is not yet reached in play; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Whether the player can move an object", "Makes an object impossible to destroy"; still open: no quest run's PASS is cited for it |
| A widescreen scene with his theme, once he has landed: the camera closes in as he despairs: "No! Not my cotton-picking hut! It's my only possession! Leave me alone!" | pending | not yet reached in play |
| A water miracle reward falls from the sky beside him | pending | never reached; `CreateReward` is real; see Reward |
| The record becomes 99% done with alignment -0.4 and the reminder "The Hermit is in a bad mood with you, Leader." (good advisor) | pending | never reached; `UpdateSnapshot` is real |
| He turns away towards his hut in despair; the music stops | pending | never reached |
| Evil advisor: "Let's kill him anyway. Go on, Boss." Good: "But you mustn't! Please!" | pending | never reached |
| Two seconds later he walks slowly (0.1) to his hut, despairs there and waits 10 seconds | pending | never reached |
| He then hurries (0.5) to the Norse village store | pending | never reached |
| When he is there, and the camera is within 100 of the store with the store on screen, a widescreen scene with his theme: the camera goes inside the store; evil advisor: "The Hermit is destroying our Village Store supplies!" while the good advisor points at the store | pending | never reached (`PosFieldOfView`, `SpiritPointGameThing` work) |
| He looks about, faces the camera and acts the arsonist; he walks out of the store | pending | never reached |
| Good advisor: "Well, you did flatten his hut. What do you expect?" | pending | never reached |
| The store is set on fire (burn speed 0.3) | partial 40% | `SetOnFire` works (the fire system); the scene is never reached; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Sets an object on fire at a burning speed"; still open: no quest run's PASS is cited for it |
| He dances mockingly round the fire: "Who's your Daddy? Who's your Daddy now? Oh yeah. Look at that!" | pending | never reached |
| He walks back to his hut and the camera returns | pending | never reached |
| Once he is at his hut and neither near the camera nor on screen, he fades away and the record is fully done with alignment -0.4 | pending | never reached |
| If he dies at any point in this branch, the death scene plays instead (see Killing him) | pending | never reached |

## Killing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he no longer exists or is dead before being impressed or offended, and the dialogue is free, the death scene plays | partial | his health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`), so the death scene no longer starts when the introduction ends; the scene runs on working natives; not checked in play; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of an object"; still open: no quest run's PASS is cited for it |
| Good advisor: "You killed him! You uncaring, horrid, mean god!" while the camera follows his body (or faces the creature if it was holding him, or looks at where he was last seen) | partial | `SetFocusFollow`, `SetPositionFollow`, `MoveCameraToFaceObject`, `RunText` work; not checked in game; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Keeps the camera looking at an object as it moves", "Keeps the camera's eye moving with an object", "Glides the camera in front of an object", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Once he has landed, five seconds of his theme | partial | the music command works; not checked in play; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music"; still open: no quest run's PASS is cited for it |
| The record is closed fully done with alignment -0.6 more than it had (so -0.6 straight away, -1.0 after damaging the hut, -0.2 after impressing him) | pending | `UpdateSnapshot` is real |
| Killing him after he was impressed but before the seed is found also plays the death scene | pending | never reached |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Impressed: a water miracle dispenser (Norse style, at an angle of 90) by his spot | done | the script reaches it; done in their own rows: story/rewards.md "A miracle dispenser can be the reward instead of a chest…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27; CHL `CREATE` makes spell dispensers and `SetMagicProperties`, `SetActive` work (`src/Magic/Script/CHLWorship.cpp`); dispensers: ../../miracles/dispensers_and_seeds.md |
| If it is the first dispenser of the game: the reward sting, the camera turns to it, a bronze scroll appears at it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), another goes beside it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), and the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | done | the script reaches it; done in their own rows: story/rewards.md "The first dispenser reward…"; story/rewards.md "A dispenser reward plays the reward sting and flies the camera…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27; `GetFirstHelp` and `GetLastHelp` are real for a spell dispenser; see [../rewards.md](../rewards.md) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | pending | never reached |
| Then the dispenser's own help lines are spoken | done | the script reaches it; done in their own rows: story/rewards.md "A miracle dispenser can be the reward instead of a chest…"; quest_check PASS, story.s2_hermit natural run, 2026-10-27 |
| Being given the dispenser marks the land as having a water miracle, so when Nemesis's storm wrecks the village the good advisor says "We'd better use our Water Miracle to put out the flames." | pending | the guide's storm has not yet been reached in a run (the guide is skipped when the creature training is); see [../creature_guide.md](../creature_guide.md) |
| Hut damaged: a water miracle reward falls from the sky beside him; when clicked, its help line is spoken | partial 60% | `CreateReward` makes the chest and `GetHelp` gives its help text (`src/CHLApi.cpp`); not checked in play against the original; see [../rewards.md](../rewards.md); finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Gives the player a reward (a miracle", [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Gives a help text by number"; still open: no quest run's PASS is cited for it |
| The strong creature seed under the rock, whichever way the quest goes | partial 60% | the seed appears when the rock is moved (CHL `CREATE` one-shot spell); not checked in game; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Create makes a one-shot miracle globe", [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "The Hermit's rock on Land 1"; still open: no quest run's PASS is cited for it |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit is gone after any ending (into his hut, faded away, or dead) | pending | the endings are not yet reached in play |
| After the hut branch the village store keeps burning until put out | partial | fire spreads and is put out by openblack's fire system; the scene is never reached; finished by [damage_and_repair.md](../../building/damage_and_repair.md) "Buildings catch fire"; still open: no quest run's PASS is cited for it |
| The dispenser stays by the hut and keeps recharging | partial | dispensers recharge (`src/Magic`); this one is never given; finished by [dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md) "It counts game turns only while active", "Once the globe is taken or moved off"; still open: no quest run's PASS is cited for it |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the seed is found before he is impressed, impressing him gives alignment 0, not +0.4 | pending | never reached; follows from the script |
| The dispenser's recharge time is set from the reward's "every so many seconds" value only when the game clock is past zero, which it always is; the hermit passes 0 | partial 60% | the script reaches it; finished by story/rewards.md "The shared dispenser-reward script means to set the refill time…" (partial 60%); quest_check PASS, story.s2_hermit natural run, 2026-10-27; undetermined: what a recharge time of 0 does to a dispenser |
| Any damage to the hut counts as the player's, even fire spreading from elsewhere | pending | the hut's health is read now (`GET_PROPERTY`, `GetProperty` in `src/CHLApi.cpp`); not checked in play |
| During the hut branch his "return home" walks are switched off for good | pending | never reached |
| Dropping him into the exit vortex while the quest is open counts as killing him: the death scene and the -0.6 play, though he is carried to the next land like any villager the vortex takes | pending | the script's death test is "gone or health at 0" (`refs/chl/thehermit.txt:704`); the vortex hand-off is [../portals.md](../portals.md); not reached in play |
| Only the creature's size is measured: the trees and bushes round his hut play no part, and a creature made bigger by a miracle counts while the miracle lasts | pending | the impress test reads only the creature's scale and the two distances (`refs/chl/thehermit.txt:517`-`:519`); the scale read is the drawn size (`GetProperty`, ../../scripts/challenge_natives_objects_and_world.md); a miracle-enlarged creature is not checked in the original |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit speaks with the "man" narrator voice | partial 60% | `RunText` plays the narrator's voice with the text (`src/Help/HelpSystem.cpp`); not checked in game; finished by [challenge_natives_advisors_and_text.md](../../scripts/challenge_natives_advisors_and_text.md) "Shows a line of text (and plays its speech) in the dialogue box"; still open: no quest run's PASS is cited for it |
| Clicking the active scroll again speaks its current reminder through the advisor who owns it | pending | the reminder needs `Snapshot`, which is real |
| Music: the hermit's theme for every scene | done | done in its own system: audio/music.md "The story's pieces…" (done); audio/music.md "Scripts start and stop a piece of music…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `StartMusic` plays it from its bank (`src/Audio`); the introduction's can play |
| Sounds: the mushroom suspense sting when the creature first faces him (impressed or first small-creature taunt) | done | done in its own system: audio/sound_effects.md "Scripts play, stop and ask about sounds…" (done); audio/sound_effects.md "The story's own sound effects bank…" (done); quest_check PASS, story.s2_hermit natural run, 2026-10-27; `PlaySoundEffect`, `StopSoundEffect` work; those scenes are |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the hermit, his hut, the rock and the record | pending | openblack's saved games (loaded only from the debug bar so far) put the scripts' machine back (`Game::LoadSavedGame`), but of the world's objects only the land's buildings, piles, forests and trees and a few others yet, not the quest's people, animals, creatures or scrolls ([saved games](../../../../engine/saved-games.md#pending)) |
