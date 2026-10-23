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

**Progress: 0/88 done, 0 chosen, 35 partial, 0 inferred — 10%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script starts after the creature's trainer has taught it to eat, the slap-and-stroke lesson and the introduction to the leash (or straight after the creature is chosen if the training is skipped); the Immersion Mushrooms quest starts just before it | partial | with the fourth tutorial answer `LandControl1` starts it straight away (training skipped), just after the mushrooms; with the other answers the land's control script never gets that far |
| Five seconds later a silver scroll appears over the hermit's hut and a bonfire is placed in front of it | partial | CHL `CREATE` makes the bonfire and `CreateHighlight` the scroll (`src/ECS/ScriptHighlight.cpp`); not checked in game |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll and it is on screen, the evil advisor pops out at most every 30 seconds, points at it and says "We got ourselves a task down here!" | partial | the shared notice script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`, `PosFieldOfView`, `DllGettime`); not checked in game |
| Clicking the scroll makes it active and the rest begins | partial | `GameThingClicked` and `SetActive` on a scroll work; not checked in game |
| Only then is a rock placed on the hill above the hut | partial | CHL `CREATE` makes the rock; not checked in game |
| A bronze did-you-know scroll is put next to the rock: "If your Creature picks up rocks the exercise will make him stronger." | partial | `CreateHighlight` and `HighlightProperties` make did-you-know scrolls; not checked in game; bronze scrolls: ../../interface/scrolls_and_signs.md |
| The hut's health is set to full | pending | `SET_PROPERTY` health sets the hut's life (`src/CHLApi.cpp:2029`, `SetHealthByScript`); not checked in play |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit (the hermit villager) appears at his hut; he can't be picked up, moved or hurt during the scene | partial 60% | CHL `CREATE` makes the villager and the three flags work (`SetIdPickupable`, `SetIdMoveable`, `SetIndestructable`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| A widescreen scene with the hermit's theme: he walks out of his hut, the camera following him over 7 seconds, then to his spot by the fire | partial 60% | widescreen, music, `MoveGameThing` (villagers), `SetFocusFollow` and the camera moves work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The camera closes in on him and slowly tilts up over 35 seconds as he talks to it, gossiping: "As I see it, there ain't no need for me to bow before you, son." | partial 60% | `SetFocus` (villager), his script animations (`SetScriptUlong`, `SetScriptState`) and `RunText` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Unimpressed: "I ain't seen diddly from you which impresses me one itty bit." | partial 60% | as above; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| "Gods have huge Creatures. That's what my momma always told me." | partial 60% | as above; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Very close up, unimpressed: "And until I see a big enough one, you don't mean nothing to me." | partial 60% | as above; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The quest is recorded: title "The Hermit", 0 done, alignment 0, reminder "This Hermit still needs to be impressed by your Creature." (good advisor) | pending | `Snapshot` is real; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He wanders about his spot (within about 6) and the camera returns to where it was; the music stops | partial 90% | `SetScriptState` works and so do his wandering's place and radius (`SetScriptStatePos`, `SetScriptFloat` and state 199; checked against v1.42 on 2026-10-20); the camera return and `StopMusic` work; re-measured 2026-10-21: the wandering is checked against v1.42; the camera return and the music stop work |
| Both advisors pop out. Good: "Ah. Don't leave him as a lost soul. Show him your Creature." Evil: "Tchoh. Let him die alone. It's what he wants. The hick." Evil: "You don't have to justify yourself to him." Evil: "Come on. You've got better things to destroy." | partial 60% | `SpiritEject`, `RunText`, `SpiritHome` work; not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Afterwards he can be picked up, moved and hurt | partial 70% | the flags work; but see the death scene below; re-measured 2026-10-21: the flags work; see the death scene |

## The hermit's wanderings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every second the script notes where he is (or that the creature is holding him) | pending | not yet reached in play; his health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`: his own life), so the old reason (he read as dead after the introduction) is gone; `InCreatureHand` is real |
| If he is more than 75 from his spot, not flying, not held and alive, and the dialogue is free, he walks back; he tries again every 10 seconds, and starts wandering again once within 15 | pending | not yet reached in play; `CreateTimer`, `MoveGameThing`, `IsDialogueReady` work, `InCreatureHand` and the in-hand read (`src/CHLApi.cpp:1679`) are real |
| This stops once he has set off to take revenge after the hut (see below) | pending | not yet reached in play |

## Showing him the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 31 passes of its loop the script reads the creature's size and checks the hermit | pending | not yet reached in play; `GET_PROPERTY` scale of a creature gives the size its body is drawn at (`src/CHLApi.cpp:1623`, `ecs::object::GetScale`) |
| A creature larger than size 0.4, within 50 of him, while he is within 50 of his spot and not held, flying or in the creature's hand, impresses him | pending | not yet reached in play; the creature's scale (`src/CHLApi.cpp:1623`), the held and flying reads (`src/CHLApi.cpp:1679`, `src/CHLApi.cpp:1671`) and `InCreatureHand` are real |
| Smaller than 0.2 and within 15: the first time, a widescreen scene with his theme: the creature's leash is let go and it is walked up to him; suspense music sting; unimpressed: "Call that a Creature? I could swat him flat!"; the creature looks sad | pending | not yet reached in play; `CREATURE_DO_ACTION` is real now (a row with an executor runs) and `MOVE_GAME_THING` walks the creature (`CreatureMindSystem::MoveForScript`) |
| The same size again, after the creature has been more than 50 away and come back within 15: "That durned critter ain't no bigger than the last time I seed him!" and the creature shows off | pending | not yet reached in play |
| Between 0.2 and 0.4 and within 15: the first time "Ha! Not the largest beast in the land, is he?" and the creature looks embarrassed | pending | not yet reached in play |
| Between 0.2 and 0.4 again, after it has been away more than 50, within 30: "That durned critter ain't no bigger than the last time I seed him!" and the creature looks angry | pending | not yet reached in play |
| Each kind of taunt is played at most twice; during them he can't be picked up, moved or hurt | partial | the flags work; the taunts are not yet reached in play |
| A creature of exactly 0.2 or 0.4 gets no taunt | pending | not yet reached in play |

## Impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with his theme: the hut is made whole; the creature's leash is let go and it walks to stand before him; the hermit walks to his spot | pending | never reached |
| Suspense sting; he faces the creature, prays three times: "Yes. Now that is one worthy Creature. I'll get worshipping!" | pending | never reached |
| The camera turns to the creature, which does its summoning gesture | pending | never reached; `CREATURE_DO_ACTION` is real now (a row with an executor runs) |
| If the seed under the rock hasn't been found: he points at the rock: "I done seen a firefly a-heading under that there rock at break of dawn." as the camera rises to show it | pending | never reached; the scripted seed stands in for the real mechanic: fireflies hide in trees and rocks at dawn and leave a seed when lifted ([../../nature/fireflies.md](../../nature/fireflies.md)) |
| "Now being as your Creature's so mighty, I be a-wondering whether he can move that rock." as the camera closes on the rock | pending | never reached |
| "And see what the fiery little critter's doing under there." (waits for the player) | pending | never reached |
| The record becomes 60% done, alignment +0.4, reminder "Look under the rock where the Hermit saw the fireflies." | pending | never reached; `UpdateSnapshot` is real |
| If the seed was already found, the record goes straight to fully done, alignment 0, with that reminder | pending | never reached |
| A water miracle dispenser is given beside his spot (see Reward) | pending | never reached |
| The player gains influence round the hermit (radius 4) and round his hut (radius 20) | pending | never reached (`InfluenceObject` works) |
| The camera returns, the creature is given back and the music stops | pending | never reached |
| When the seed is found (or he dies) the record is updated to fully done with the reminder "This Hermit still needs to be impressed by your Creature." | pending | `UpdateSnapshot` is real |
| He then walks into his hut and is gone | pending | never reached |

## The seed under the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start, if the rock is in place, a watcher waits for it to be moved more than 5 from its spot, or picked up by the hand or the creature; this can happen before or after the hermit is impressed | partial | the watcher runs from the start (`GetDistance`); a rock held by the hand or the creature is noticed now (the in-hand read, `src/CHLApi.cpp:1679`, and `InCreatureHand`); not checked in game |
| A one-shot "strong" creature miracle seed then appears where the rock was, with a target effect for 10 seconds | partial | CHL `CREATE` makes the one-shot seed and `SpecialEffectPosition` works; not checked in game; seeds: ../../miracles/dispensers_and_seeds.md |
| If the hermit is alive and within 50 of his spot: a widescreen scene; the creature (if within 30 of the rock) is sent to the bottom of the hill; good advisor: "Great! A Miracle Seed! Well, done, Mighty One." | partial | the camera and dialogue natives work, and `MoveGameThing` now walks the creature under the script's control (`CreatureMindSystem::MoveForScript`); not checked in game |
| The camera rises and turns to the hermit, who gossips: "Them fireflies turn theirselves into Miracle Seeds at dawn, I reckon." | partial | not checked in game |
| Good advisor: "This is interesting news. We should watch out for fireflies and see where they hide at dawn." | partial | not checked in game |
| Otherwise: the creature (if within 100) is sent down the hill; good advisor: "Golly. A Miracle Seed. How did that get there?"; evil: "The fireflies hide and turn themselves into Miracle Seeds at dawn. Duh!"; good: "Oh. Of course. Now that is worth knowing, Leader." | partial | not checked in game; `MoveGameThing` now walks the creature under the script's control (`CreatureMindSystem::MoveForScript`) |

## Damaging the hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any damage to the hut (its health below full) while he is alive and the dialogue is free sets off this branch, whoever caused it | pending | the hut's health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`: below 1 once damaged); not checked in play |
| He can't be picked up until he is put down; a second later he can be picked up, moved and hurt again | partial | the flags work; the branch is not yet reached in play |
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
| The store is set on fire (burn speed 0.3) | partial 40% | `SetOnFire` works (the fire system); the scene is never reached; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| He dances mockingly round the fire: "Who's your Daddy? Who's your Daddy now? Oh yeah. Look at that!" | pending | never reached |
| He walks back to his hut and the camera returns | pending | never reached |
| Once he is at his hut and neither near the camera nor on screen, he fades away and the record is fully done with alignment -0.4 | pending | never reached |
| If he dies at any point in this branch, the death scene plays instead (see Killing him) | pending | never reached |

## Killing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he no longer exists or is dead before being impressed or offended, and the dialogue is free, the death scene plays | partial | his health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`), so the death scene no longer starts when the introduction ends; the scene runs on working natives; not checked in play |
| Good advisor: "You killed him! You uncaring, horrid, mean god!" while the camera follows his body (or faces the creature if it was holding him, or looks at where he was last seen) | partial | `SetFocusFollow`, `SetPositionFollow`, `MoveCameraToFaceObject`, `RunText` work; not checked in game |
| Once he has landed, five seconds of his theme | partial | the music command works; not checked in play |
| The record is closed fully done with alignment -0.6 more than it had (so -0.6 straight away, -1.0 after damaging the hut, -0.2 after impressing him) | pending | `UpdateSnapshot` is real |
| Killing him after he was impressed but before the seed is found also plays the death scene | pending | never reached |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Impressed: a water miracle dispenser (Norse style, at an angle of 90) by his spot | partial 40% | CHL `CREATE` makes spell dispensers and `SetMagicProperties`, `SetActive` work (`src/Magic/Script/CHLWorship.cpp`); never reached; dispensers: ../../miracles/dispensers_and_seeds.md; re-measured 2026-10-21: the natives it calls work; the scene is never reached in a run |
| If it is the first dispenser of the game: the reward sting, the camera turns to it, a bronze scroll appears at it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), another goes beside it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), and the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | pending | never reached; `GetFirstHelp` and `GetLastHelp` are real for a spell dispenser; see [../rewards.md](../rewards.md) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | pending | never reached |
| Then the dispenser's own help lines are spoken | pending | never reached |
| Being given the dispenser marks the land as having a water miracle, so when Nemesis's storm wrecks the village the good advisor says "We'd better use our Water Miracle to put out the flames." | pending | the guide's storm never runs in our tree; see [../creature_guide.md](../creature_guide.md) |
| Hut damaged: a water miracle reward falls from the sky beside him; when clicked, its help line is spoken | partial 60% | `CreateReward` makes the chest and `GetHelp` gives its help text (`src/CHLApi.cpp`); not checked in play against the original; see [../rewards.md](../rewards.md); re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| The strong creature seed under the rock, whichever way the quest goes | partial 60% | the seed appears when the rock is moved (CHL `CREATE` one-shot spell); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit is gone after any ending (into his hut, faded away, or dead) | pending | the endings are not yet reached in play |
| After the hut branch the village store keeps burning until put out | partial | fire spreads and is put out by openblack's fire system; the scene is never reached |
| The dispenser stays by the hut and keeps recharging | partial | dispensers recharge (`src/Magic`); this one is never given |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the seed is found before he is impressed, impressing him gives alignment 0, not +0.4 | pending | never reached; follows from the script |
| The dispenser's recharge time is set from the reward's "every so many seconds" value only when the game clock is past zero, which it always is; the hermit passes 0 | pending | never reached; undetermined: what a recharge time of 0 does to a dispenser |
| Any damage to the hut counts as the player's, even fire spreading from elsewhere | pending | the hut's health is read now (`GET_PROPERTY`, `src/CHLApi.cpp:1596`); not checked in play |
| During the hut branch his "return home" walks are switched off for good | pending | never reached |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit speaks with the "man" narrator voice | partial 60% | `RunText` plays the narrator's voice with the text (`src/Help/HelpSystem.cpp`); not checked in game; re-measured 2026-10-21: the script runs on natives that work; not checked in a run yet |
| Clicking the active scroll again speaks its current reminder through the advisor who owns it | pending | the reminder needs `Snapshot`, which is real |
| Music: the hermit's theme for every scene | partial | `StartMusic` plays it from its bank (`src/Audio`); the introduction's can play |
| Sounds: the mushroom suspense sting when the creature first faces him (impressed or first small-creature taunt) | partial | `PlaySoundEffect`, `StopSoundEffect` work; those scenes are not yet reached in play |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the hermit, his hut, the rock and the record | pending | openblack has no saved games |
