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
rows are todo unless the notes say otherwise. The land as a whole is in [../land_1.md](../land_1.md), the script's
function coverage in [../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/88 done, 34 partial — 19%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's script starts after the creature's trainer has taught it to eat, the slap-and-stroke lesson and the introduction to the leash (or straight after the creature is chosen if the training is skipped); the Immersion Mushrooms quest starts just before it | partial | with the fourth tutorial answer `LandControl1` starts it straight away (training skipped), just after the mushrooms; with the other answers the land's control script never gets that far |
| Five seconds later a silver scroll appears over the hermit's hut and a bonfire is placed in front of it | partial | CHL `CREATE` makes the bonfire and `CreateHighlight` the scroll (`src/ECS/ScriptHighlight.cpp`); not checked in game |
| Until the scroll or the hut is clicked, whenever the camera is within 100 of the scroll and it is on screen, the evil advisor pops out at most every 30 seconds, points at it and says "We got ourselves a task down here!" | partial | the shared notice script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`, `PosFieldOfView`, `DllGettime`); not checked in game |
| Clicking the scroll makes it active and the rest begins | partial | `GameThingClicked` and `SetActive` on a scroll work; not checked in game |
| Only then is a rock placed on the hill above the hut | partial | CHL `CREATE` makes the rock; not checked in game |
| A bronze did-you-know scroll is put next to the rock: "If your Creature picks up rocks the exercise will make him stronger." | partial | `CreateHighlight` and `HighlightProperties` make did-you-know scrolls; not checked in game; bronze scrolls: ../../interface/scrolls_and_signs.md |
| The hut's health is set to full | todo | `SetProperty` has no health |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit (the hermit villager) appears at his hut; he can't be picked up, moved or hurt during the scene | partial | CHL `CREATE` makes the villager and the three flags work (`SetIdPickupable`, `SetIdMoveable`, `SetIndestructable`); not checked in game |
| A widescreen scene with the hermit's theme: he walks out of his hut, the camera following him over 7 seconds, then to his spot by the fire | partial | widescreen, music, `MoveGameThing` (villagers), `SetFocusFollow` and the camera moves work; not checked in game |
| The camera closes in on him and slowly tilts up over 35 seconds as he talks to it, gossiping: "As I see it, there ain't no need for me to bow before you, son." | partial | `SetFocus` (villager), his script animations (`SetScriptUlong`, `SetScriptState`) and `RunText` work; not checked in game |
| Unimpressed: "I ain't seen diddly from you which impresses me one itty bit." | partial | as above; not checked in game |
| "Gods have huge Creatures. That's what my momma always told me." | partial | as above; not checked in game |
| Very close up, unimpressed: "And until I see a big enough one, you don't mean nothing to me." | partial | as above; not checked in game |
| The quest is recorded: title "The Hermit", 0 done, alignment 0, reminder "This Hermit still needs to be impressed by your Creature." (good advisor) | todo | `Snapshot` is a stub; how records are kept: [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| He wanders about his spot (within about 6) and the camera returns to where it was; the music stops | partial | `SetScriptState` works but his wandering lacks its place and radius (`SetScriptStatePos`, `SetScriptFloat` are stubs); the camera return and `StopMusic` work |
| Both advisors pop out. Good: "Ah. Don't leave him as a lost soul. Show him your Creature." Evil: "Tchoh. Let him die alone. It's what he wants. The hick." Evil: "You don't have to justify yourself to him." Evil: "Come on. You've got better things to destroy." | partial | `SpiritEject`, `RunText`, `SpiritHome` work; not checked in game |
| Afterwards he can be picked up, moved and hurt | partial | the flags work; but see the death scene below |

## The hermit's wanderings

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every second the script notes where he is (or that the creature is holding him) | todo | (inferred) never reached: right after the introduction the script takes the hermit for dead, since `GetProperty` has no health and answers 0, and the death scene runs (see Killing him); `InCreatureHand` is a stub too |
| If he is more than 75 from his spot, not flying, not held and alive, and the dialogue is free, he walks back; he tries again every 10 seconds, and starts wandering again once within 15 | todo | never reached (inferred, see above); `CreateTimer`, `MoveGameThing`, `IsDialogueReady` work, `InCreatureHand` is a stub |
| This stops once he has set off to take revenge after the hut (see below) | todo | never reached (inferred) |

## Showing him the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 31 passes of its loop the script reads the creature's size and checks the hermit | todo | never reached (inferred); also `GetProperty` has no scale for a creature (it answers 0) |
| A creature larger than size 0.4, within 50 of him, while he is within 50 of his spot and not held, flying or in the creature's hand, impresses him | todo | never reached (inferred); with `GetProperty` answering 0 for the creature's scale no creature could impress him |
| Smaller than 0.2 and within 15: the first time, a widescreen scene with his theme: the creature's leash is let go and it is walked up to him; suspense music sting; unimpressed: "Call that a Creature? I could swat him flat!"; the creature looks sad | todo | never reached (inferred); `CreatureDoAction` is a stub |
| The same size again, after the creature has been more than 50 away and come back within 15: "That durned critter ain't no bigger than the last time I seed him!" and the creature shows off | todo | never reached (inferred) |
| Between 0.2 and 0.4 and within 15: the first time "Ha! Not the largest beast in the land, is he?" and the creature looks embarrassed | todo | never reached (inferred) |
| Between 0.2 and 0.4 again, after it has been away more than 50, within 30: "That durned critter ain't no bigger than the last time I seed him!" and the creature looks angry | todo | never reached (inferred) |
| Each kind of taunt is played at most twice; during them he can't be picked up, moved or hurt | partial | the flags work; the taunts are never reached (inferred) |
| A creature of exactly 0.2 or 0.4 gets no taunt | todo | never reached (inferred) |

## Impressed

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A widescreen scene with his theme: the hut is made whole; the creature's leash is let go and it walks to stand before him; the hermit walks to his spot | todo | never reached |
| Suspense sting; he faces the creature, prays three times: "Yes. Now that is one worthy Creature. I'll get worshipping!" | todo | never reached |
| The camera turns to the creature, which does its summoning gesture | todo | never reached; `CreatureDoAction` is a stub |
| If the seed under the rock hasn't been found: he points at the rock: "I done seen a firefly a-heading under that there rock at break of dawn." as the camera rises to show it | todo | never reached; the scripted seed stands in for the real mechanic: fireflies hide in trees and rocks at dawn and leave a seed when lifted ([../../nature/fireflies.md](../../nature/fireflies.md)) |
| "Now being as your Creature's so mighty, I be a-wondering whether he can move that rock." as the camera closes on the rock | todo | never reached |
| "And see what the fiery little critter's doing under there." (waits for the player) | todo | never reached |
| The record becomes 60% done, alignment +0.4, reminder "Look under the rock where the Hermit saw the fireflies." | todo | never reached; `UpdateSnapshot` is a stub |
| If the seed was already found, the record goes straight to fully done, alignment 0, with that reminder | todo | never reached |
| A water miracle dispenser is given beside his spot (see Reward) | todo | never reached |
| The player gains influence round the hermit (radius 4) and round his hut (radius 20) | todo | never reached (`InfluenceObject` works) |
| The camera returns, the creature is given back and the music stops | todo | never reached |
| When the seed is found (or he dies) the record is updated to fully done with the reminder "This Hermit still needs to be impressed by your Creature." | todo | `UpdateSnapshot` is a stub |
| He then walks into his hut and is gone | todo | never reached |

## The seed under the rock

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start, if the rock is in place, a watcher waits for it to be moved more than 5 from its spot, or picked up by the hand or the creature; this can happen before or after the hermit is impressed | partial | the watcher runs from the start (`GetDistance`); a rock held by the hand or the creature is not noticed (`GetProperty` has no in-hand, `InCreatureHand` is a stub); not checked in game |
| A one-shot "strong" creature miracle seed then appears where the rock was, with a target effect for 10 seconds | partial | CHL `CREATE` makes the one-shot seed and `SpecialEffectPosition` works; not checked in game; seeds: ../../miracles/dispensers_and_seeds.md |
| If the hermit is alive and within 50 of his spot: a widescreen scene; the creature (if within 30 of the rock) is sent to the bottom of the hill; good advisor: "Great! A Miracle Seed! Well, done, Mighty One." | partial | the camera and dialogue natives work, but `MoveGameThing` does not move the creature; not checked in game |
| The camera rises and turns to the hermit, who gossips: "Them fireflies turn theirselves into Miracle Seeds at dawn, I reckon." | partial | not checked in game |
| Good advisor: "This is interesting news. We should watch out for fireflies and see where they hide at dawn." | partial | not checked in game |
| Otherwise: the creature (if within 100) is sent down the hill; good advisor: "Golly. A Miracle Seed. How did that get there?"; evil: "The fireflies hide and turn themselves into Miracle Seeds at dawn. Duh!"; good: "Oh. Of course. Now that is worth knowing, Leader." | partial | not checked in game; `MoveGameThing` does not move the creature |

## Damaging the hut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Any damage to the hut (its health below full) while he is alive and the dialogue is free sets off this branch, whoever caused it | todo | `GetProperty` has no health; the death scene comes first (inferred) |
| He can't be picked up until he is put down; a second later he can be picked up, moved and hurt again | partial | the flags work; the branch is never reached (inferred) |
| A widescreen scene with his theme, once he has landed: the camera closes in as he despairs: "No! Not my cotton-picking hut! It's my only possession! Leave me alone!" | todo | never reached (inferred) |
| A water miracle reward falls from the sky beside him | todo | never reached; `CreateReward` is a stub; see Reward |
| The record becomes 99% done with alignment -0.4 and the reminder "The Hermit is in a bad mood with you, Leader." (good advisor) | todo | never reached; `UpdateSnapshot` is a stub |
| He turns away towards his hut in despair; the music stops | todo | never reached |
| Evil advisor: "Let's kill him anyway. Go on, Boss." Good: "But you mustn't! Please!" | todo | never reached |
| Two seconds later he walks slowly (0.1) to his hut, despairs there and waits 10 seconds | todo | never reached |
| He then hurries (0.5) to the Norse village store | todo | never reached |
| When he is there, and the camera is within 100 of the store with the store on screen, a widescreen scene with his theme: the camera goes inside the store; evil advisor: "The Hermit is destroying our Village Store supplies!" while the good advisor points at the store | todo | never reached (`PosFieldOfView`, `SpiritPointGameThing` work) |
| He looks about, faces the camera and acts the arsonist; he walks out of the store | todo | never reached |
| Good advisor: "Well, you did flatten his hut. What do you expect?" | todo | never reached |
| The store is set on fire (burn speed 0.3) | partial | `SetOnFire` works (the fire system); the scene is never reached |
| He dances mockingly round the fire: "Who's your Daddy? Who's your Daddy now? Oh yeah. Look at that!" | todo | never reached |
| He walks back to his hut and the camera returns | todo | never reached |
| Once he is at his hut and neither near the camera nor on screen, he fades away and the record is fully done with alignment -0.4 | todo | never reached |
| If he dies at any point in this branch, the death scene plays instead (see Killing him) | todo | never reached |

## Killing him

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If he no longer exists or is dead before being impressed or offended, and the dialogue is free, the death scene plays | partial | (inferred) it plays, but wrongly: `GetProperty` has no health and answers 0, so the death scene starts as soon as the introduction ends and the dialogue is free |
| Good advisor: "You killed him! You uncaring, horrid, mean god!" while the camera follows his body (or faces the creature if it was holding him, or looks at where he was last seen) | partial | (inferred) plays wrongly right after the introduction (see above); `SetFocusFollow`, `SetPositionFollow`, `MoveCameraToFaceObject`, `RunText` work; not checked in game |
| Once he has landed, five seconds of his theme | partial | the music command works; plays in the wrong death scene (inferred) |
| The record is closed fully done with alignment -0.6 more than it had (so -0.6 straight away, -1.0 after damaging the hut, -0.2 after impressing him) | todo | `UpdateSnapshot` is a stub |
| Killing him after he was impressed but before the seed is found also plays the death scene | todo | never reached |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Impressed: a water miracle dispenser (Norse style, at an angle of 90) by his spot | partial | CHL `CREATE` makes spell dispensers and `SetMagicProperties`, `SetActive` work (`src/Magic/Script/CHLWorship.cpp`); never reached; dispensers: ../../miracles/dispensers_and_seeds.md |
| If it is the first dispenser of the game: the reward sting, the camera turns to it, a bronze scroll appears at it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), another goes beside it ("That there are Miracles hidden all over Eden. Keep your eyes peeled."), and the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles." then "Click on the signpost for more info." | todo | never reached; `GetFirstHelp`, `GetLastHelp` are stubs; see [../rewards.md](../rewards.md) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | todo | never reached |
| Then the dispenser's own help lines are spoken | todo | never reached |
| Being given the dispenser marks the land as having a water miracle, so when Nemesis's storm wrecks the village the good advisor says "We'd better use our Water Miracle to put out the flames." | todo | the guide's storm never runs in our tree; see [../creature_guide.md](../creature_guide.md) |
| Hut damaged: a water miracle reward falls from the sky beside him; when clicked, its help line is spoken | todo | `CreateReward`, `GetHelp` stubs; see [../rewards.md](../rewards.md) |
| The strong creature seed under the rock, whichever way the quest goes | partial | the seed appears when the rock is moved (CHL `CREATE` one-shot spell); not checked in game |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit is gone after any ending (into his hut, faded away, or dead) | todo | the endings are never reached; the wrong death scene does not remove him (inferred) |
| After the hut branch the village store keeps burning until put out | partial | fire spreads and is put out by openblack's fire system; the scene is never reached |
| The dispenser stays by the hut and keeps recharging | partial | dispensers recharge (`src/Magic`); this one is never given |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| If the seed is found before he is impressed, impressing him gives alignment 0, not +0.4 | todo | never reached; follows from the script |
| The dispenser's recharge time is set from the reward's "every so many seconds" value only when the game clock is past zero, which it always is; the hermit passes 0 | todo | never reached; undetermined: what a recharge time of 0 does to a dispenser |
| Any damage to the hut counts as the player's, even fire spreading from elsewhere | todo | `GetProperty` has no health |
| The hut branch's comments speak of the hermit giving a gesture; the shipped game gives a water miracle | n/a | comment only |
| During the hut branch his "return home" walks are switched off for good | todo | never reached |

## Advisors, music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The hermit speaks with the "man" narrator voice | partial | `RunText` plays the narrator's voice with the text (`src/Help/HelpSystem.cpp`); not checked in game |
| Clicking the active scroll again speaks its current reminder through the advisor who owns it | todo | the reminder needs `Snapshot`, a stub |
| Music: the hermit's theme for every scene | partial | `StartMusic` plays it from its bank (`src/Audio`); the introduction's can play |
| Sounds: the mushroom suspense sting when the creature first faces him (impressed or first small-creature taunt) | partial | `PlaySoundEffect`, `StopSoundEffect` work; those scenes are never reached (inferred) |

## Other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps the hermit, his hut, the rock and the record | todo | openblack has no saved games |
| A test script starts the quest on its own; an unshipped control script had it switched off | n/a | not started by the game |
| The hermit villager is also used by the fifth land's script and an unshipped waving scene, outside this quest | n/a | see ../land_5.md |
