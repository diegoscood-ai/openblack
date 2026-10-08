# Creature Guide

The gold-scroll story of the first land's second half, shown under the scroll title "The Creature's Learning": a huge
creature, once Nemesis's, wanders a valley, asks to meet the player's creature, befriends it, teaches the player to
impress the Aztec village with a food miracle (and the creature to dance), teaches the creature to fight, and is killed
by Nemesis's storm on a mountain top as it is about to tell the secret of the Creed. The storm then turns on the
player's village and, a couple of minutes later, the exit vortex shows itself. The land as a whole is in
[land_1.md](./land_1.md), the vortex in [portals.md](./portals.md), the script program in
[../scripts/challenge_scripts.md](../scripts/challenge_scripts.md).

Sources: the land's challenge scripts (the original source text, checked line by line against the PC game's compiled
`challenge.chl`), the game's text table and the executable. openblack's state is judged on this tree: of the 104 script
functions the guide's scripts and the scripts they start need, 23 still only log "not implemented" in `src/CHLApi.cpp`
(among them the creature's actions, desires and fights and the challenge log), a script cannot make the guide
(`CreateScriptObject` makes no creatures), and the land's control script never gets that far (it stops at Choose Your
Creature's gate stones, or skips the guide when the creature training is skipped), so the guide never appears; every row
below is todo unless the notes say otherwise.

**Progress: 0/121 done, 2 partial — 1%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide's story is a gold scroll (a story highlight, not a silver reward scroll), titled "The Creature's Learning", the same title as the trainer's lessons before it | todo | `CreateHighlight` is real (`src/ECS/ScriptHighlight`); `Snapshot` and `UpdateSnapshot` are stubs; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The land's control script runs the guide's chapters one after another: the meeting, the request to bring the creature, the food lesson, the wait while the village is won, the fight lesson and the storm; each waits for the last to finish | todo | `LandControl1` runs them in order, but a no-skip game stops at Choose Your Creature's gate stones (see [../scripts/land1_script.md](../scripts/land1_script.md)) |
| The meeting starts only after the trainer's last lesson (tying the leash to a house) is done | todo | the trainer's lessons are never reached; see [gold_scrolls/the_creatures_learning.md](gold_scrolls/the_creatures_learning.md) |
| The Pied Piper silver scroll is started only after the food lesson; the Ogre (guardian) silver scroll is started after the fight lesson and waits for it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); see [the_pied_piper.md](silver_scrolls/the_pied_piper.md) and [the_ogre.md](silver_scrolls/the_ogre.md) |
| The exit-vortex script is started with the storm and waits until the storm opens the way | todo | the storm needs the guide; with the guide skipped the exit vortex opens at once instead; see [portals.md](./portals.md) |
| A new game that skips the creature training (or keeps the old creature, patch 1.1) never makes the guide; the land waits for its leave flag and the vortex opens at once | partial | `CanSkipCreatureTraining` and `IsKeepingOldCreature` are real (the SkipBox, `src/Gui/SkipBox`): with the third and fourth answers no guide script runs, `LandControl1` waits for the leave flag and `LeaveThroughVortexL1` opens the vortex at once; not checked in game. See [../../bw1-notes/map-loading.md](../../bw1-notes/map-loading.md#skipping-the-tutorial-skipbox-and-can_skip_tutorial) |
| The scroll's progress mark and its reminder line are updated at every step: 0.5 at the meeting, 0.6 after making friends, 0.7 at the food lesson, 0.75 while the village is won and at the fight, 0.8 to 0.85 after it, 0.9 to 0.99 through the storm and 1 as the guide dies | todo | `UpdateSnapshot` is a stub; the challenge log is not drawn |
| Each reminder is spoken by whichever advisor owns the line, which steps out to say it | partial | the shared reminder calls real natives (`SpiritEject`, `RunText`); its texts come from the stubbed log |

## The guide itself

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide's species depends on the player's creature: a sheep for a cow, a lion for a tiger, a bear for anything else (the ape, or a creature swapped at the breeder) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `IsOfType` is a stub |
| It is made at a fixed spot near the Aztec village, twice normal size, with a top speed of 40.5 and full strength | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); a creature's scale, speed and strength are not handled by `SetProperty` |
| Its name over it is "Guide" | todo | `SetCreatureName` is a stub |
| Until the creatures meet, it runs from objects (a flee reaction), dropped at the meeting | todo | `CreateReaction` and `RemoveReactionOfType` are stubs |
| Its voice is the guide's own narrator; while it speaks, creature sounds are switched off, so the talk is "telepathic" | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `SetCreatureSound` is real; see ../scripts/info_scripts.md |
| Every chapter remakes the guide at its starting spot if it no longer exists, so killing it before the storm only brings a new one | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Nothing in the scripts reacts to the player hurting, throwing or attacking the guide or its village; its lessons simply go on (undetermined: how the creature itself reacts to being hit by the player) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## Wandering the valley

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide is made as soon as the creature is chosen and starts wandering, long before the meeting | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| It first walks to the head of a canyon, then loops: walks between two canyon spots, plays a sad animation, or stares at the player's creature for 10 seconds, picked at random | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); creature walks by script are not done |
| The wandering stops only when the meeting begins (the loop's own end state is never reached) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## The messenger

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The meeting puts the guide at the canyon head and waits two minutes, or until the player's creature comes within 75 of it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `CreateTimer` is real |
| A man then walks slowly up near the temple, faces it and waves for attention in a loop, under a gold scroll; he cannot be moved, picked up or killed meanwhile | todo | `CREATE` makes the man (a villager), his walk and the three flags are real (`SetIdMoveable`, `SetIdPickupable`, `SetIndestructable`); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| Clicking his scroll closes in on him (high detail) as he stands scared stiff: "Holy One! There's a huge Creature in the valley beyond!" | todo | `MoveCameraToFaceObject`, `SetHighGraphicsDetail` and `RunText` are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| He turns round on the spot, points into the valley and talks: "We're all terrified!" | todo | villager animations and texts are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The good advisor: "This must be the Creature that Sable spoke of. Let's investigate." The camera then pulls back over the temple | todo | advisors, texts and the camera are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| He is then let go to live in the village beyond the pass, movable and mortal again | todo | `ReleaseFromScript` and the flags are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| While he walks up, the guide stops wandering and walks to a spot in the pass | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## First sight

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide keeps turning to the camera; once it is on screen with the camera within 90, a scene starts with the second epic theme, closing to 60 from it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `GameThingFieldOfView` is real |
| Good advisor: "Look at the size of that!"; evil: "Ah. Okay, we're dead. We're all dead."; good: "What is it? Where's it from?" and "It doesn't seem to be aggressive, though."; evil: "If it is, we're all dead. Even us spirits. Dead. You too, Boss." | todo | advisors and texts are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The camera closes to 40; good: "Can you hear that? This giant is communicating. It's telepathic!"; the guide: "You have a Creature. Bring him to me."; evil: "No way. He'll eat him!"; good: "But this giant looks friendly. Let's bring our Creature over with the Leash." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); the camera distance is measured from it |
| The guide plays its sad animation and the scroll's reminder becomes "You have a Creature. Bring him to me." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## Bringing the creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must lead the creature to within 45 of the guide, normally with the leash; nothing else is asked | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `SetCreatureDevStage` is real |
| While it is further, the guide says "Please. Use the Leash to bring your Creature to me." when on screen and the camera is within 150, at most every 20 seconds | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The scripts look only at distance: a sleeping creature, or one in its pen, is fine once it is within 45 (no special handling found) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Unused quirk: "Yes" and "No" help sounds were meant to play as the creature came closer (within 30, within 20) or went further, but that loop only runs while the creature is already beyond 45, which it never is by then, so they never play (and the "further" test compares with a distance never set) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## Making friends (and the guide's past)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene with the guide's theme starts with the camera low behind the player's creature, which looks up at the guide | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); the camera natives are real |
| Good advisor: "I do hope they'll be friends." | todo | `RunText` is real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The leash is taken off; the creature looks the guide over, and the guide picks the player's creature up in its hand as the camera rises to its face | todo | `CreatureDoAction` is a stub and the guide is never made |
| The guide looks at the camera, the creature waves at it, the guide smiles back | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide: "Another Creature. Is it true? Are you real?" / "I have been alone for aeons. I thought I would die without ever seeing another god." / "Or another Creature." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) (the guide's lines are spoken through it) |
| The history: "I once belonged to Nemesis, the most powerful of gods." / "But his power kept growing, and soon I wasn't bold enough for him." / "So I was cast out. Banished here. And I've been alone ever since." (playing its sad animation) / "Until now." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Good advisor: "This being is astonishing. But what a sad tale." | todo | `RunText` is real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The guide: "In return for your friendship, and that of your Creature, I will teach you both." / "You will both learn the ways of the gods." (with its feeling-nice animation) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| It points at the pass to the Aztec village and the camera follows: "When you are ready for the first lesson, meet me at the Village through this pass." That line becomes the reminder | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Both creatures are left wanting only to be friends; the creature's own development script is switched off; the guide walks to the village and waits there | todo | `SetCreatureOnlyDesire` is a stub; `CreatureInDevScript` is real |

## The food lesson (impressing the Aztec village)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until this lesson starts, the Aztec village's belief in the player is set back to nothing every 10 seconds, so it cannot be won early | todo | `BeliefForPlayer` and `SetPlayerBelief` are stubs |
| A gold scroll stands just beside the guide at the village; the lesson starts when it is clicked (no time limit) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Starting it ends the belief reset, empties the village store of food (three times over the lesson) and keeps the village topped up: whenever it has 15 people or fewer, six more (three men, three women) come out of three of its houses every 3 seconds | todo | `RemoveResource` is a stub |
| The creature's stage moves to "the guide teaches it miracles", the stage after which it may cast powered-up miracles | todo | `SetCreatureDevStage` is real, but nothing in our creature's casting reads a powered-up stage |
| With the guide's theme the camera faces the guide; if more than five minutes passed since the lesson became available: "Now you are ready for the lesson." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| "This is the Aztec Village. I will teach you the first secret here." / "These people do not yet believe in you as a god." / "To grow in power you and your Creature can impress them until they believe in you." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The camera swings to the store: "Look at the Village Store. The flag shows that they need food." / "If you give them some, they'll think it's a Miracle and will believe in you more." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| "Click on the Signpost to find out more about Desire flags." — a sparkle marks a new did-you-know signpost by the village centre about what impresses villagers | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `DidYouKnow` and `SpecialEffectObject` are real |
| The guide plays its casting gesture and a powered food one-shot miracle appears by the village: "This is a Food Miracle you can cast once."; a second signpost appears: "If you give your Creature a Miracle Seed he'll instantly cast it." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `CREATE` of a one-shot miracle is real |
| "You can pick up such one-shot Miracles with the Action Button." / "And activate them by tapping them with the same button. Like this." — a recorded hand demonstration taps the miracle and casts it on the store; if the demonstration's tap misses, the miracle is put straight into the player's hand | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `PlayHandDemo` is real |
| "Things like this really impress the Villagers." / "If your Creature is on the Rope Leash he'll learn this Miracle as you cast it." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); learning by watching: [../creature/learning_by_observation.md](../creature/learning_by_observation.md) |
| The guide makes a new food miracle (casting gesture, 2.5 seconds, the seed drops in from 50 up): "Now you try. Tap on the Miracle and click the Action Button on the Aztec Village Store." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Picking the miracle up instead of tapping it: "You picked up the Miracle instead of tapping it. Drop it and try again." (said once) | todo | `GetObjectHeld` is a stub |
| Holding a tapped miracle: "Well done. Now click the Action Button over the Village Store." again every 30 seconds while it is held | todo | the held read is a stub |
| Letting it go: if the store now has any food the lesson is passed; if not, "Hmm. Try again." and a new miracle if the old one is gone | todo | the store read (`GetResource`) is a stub |
| An untapped miracle taken more than 50 from its spot fades away and a new one is made there | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| On success six more food miracles appear for practice; the creature looks at the guide and learns to dance (the creature's own way of impressing villagers) | todo | `CreatureSetKnowsAction` is a stub |
| "Good. Not only have you fed them, but now they believe in you more." / "I have created more Food Miracles for you to practice with." / "At the Village Centre you can see your symbol. When it reaches the stone hand there the Village will believe in you." / "Holding your hand over it shows how much belief the people have in you." / "I need sleep. Wake me when the Village believes in you, and you are ready to continue your education," | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## Winning the village, and the guide's naps

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide lies down asleep under a gold scroll; the reminder is "You need to take over this Village by impressing it." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The next step waits, with no time limit, until the Aztec village belongs to the player, however it was won (the player, the creature, miracles) | todo | the town's owner (property 21) is not handled by `GetProperty`; see [../worship/belief.md](../worship/belief.md) |
| Then a scene with the fourth epic theme, then the guide's theme, views the village; evil advisor: "The Aztecs will worship you now. Cool." | todo | follows from the town owner read, not handled |
| The good advisor suggests one thing, in this order: a silver scroll not yet tried (the Missionaries, then the Lost Flock, then the Pied Piper, flying the camera high over the land to it): "You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it."; else, if the home village's store has under 10,000 food: "It is unwise to neglect your followers. I see the Village is short of food."; else, if the creature's hunger is over 50: "You shouldn't neglect your Creature. He's rather hungry." | todo | follows from the town owner read, not handled |
| The guide sleeps again and is kept fully tired every second | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| It sleeps five minutes; if the suggested silver scroll is finished, it wakes once two and a half minutes have passed | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Clicking it while it sleeps (village won): the populating stops; "I am tired. Let me rest before we continue." and the advisor's suggestion again, moving on to the next silver scroll if the last one was done | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Clicking it with the village lost again: evil advisor: "The Aztec Village still needs to be further impressed. Use the Creature to help you." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## The fight lesson

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature's stage moves to "learning to fight"; the guide walks to a clearing near the village and sits, a gold scroll beside it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide: "I'm refreshed and ready to start teaching your little Creature more." and becomes the reminder | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Until the scroll or the guide is clicked, the guide beckons and points at the camera; the player's creature points out the scroll every 40 seconds (every 12 when the camera is away from it); with the camera within 100 and the scroll in view the good advisor says "The Guide wishes to attract our attention." at most every 30 seconds | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| If the creature is further than 40, the evil advisor: "Bring our Creature to the big guy. I smell combat!", and the lesson waits for it; reminder: "Your Creature needs to fight before he can move to the next stage." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| With the guide's theme, the leash is switched off and taken away (twice, three seconds apart) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| "When I was Nemesis' Creature he often sent me into battle. I foresee the same for you." / "First, I will become your size to make things easier." / "Be prepared to get bruised. Don't worry. In time these will heal." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide shrinks: it is set to scale itself automatically to a size of 1.2 while the camera pans down with it (undetermined how the game applies that size over time) | todo | `CreatureAutoscale` is a stub |
| The creature is walked to its place opposite, both stare at each other and a two-shot camera holds them | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); `StartDualCamera` is real (`src/Camera/ScriptCamera`) |
| The guide taunts: "An arena will appear every time there is a fight." as the player's creature shies on the spot; then "You should know that your Creature will fight without help from you."; the creature looks happy and then at the camera | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide starts a fight with the creature, and starts it again every 15 seconds until the creature is fighting (this wait is written out twice) | todo | duels exist in our tree (`CreatureFightSystem`, see [../creature/fighting.md](../creature/fighting.md)), but the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) and `IsFighting` is a stub |
| For the drills both creatures stop fighting by themselves and both are put back to full fight health whenever either drops below half | todo | `SetCreatureAutoFighting` is a stub; `SetAutoFighting` exists in `CreatureFightSystem` |
| Attacking: "Firstly, let's learn to attack." / "Click on a part of my body, using the Action Button, to strike there." — passed when the guide reels from a blow, repeated every 15 seconds | todo | striking by clicking works (`CreatureFightSystem::Press`), but there is no guide to strike and `GetCreatureFightAction` is a stub |
| Queuing: "A good hit. You can queue up multiple attacks as well." / "Try queuing up several attacks on me now." — passed when more than two blows are queued, repeated every 10 seconds | todo | the queue works (`CreatureFightSystem::QueueMove`), but there is no guide and `CreatureFightQueueHits` is a stub |
| Blocking: the guide queues a block; "That's right. Now you must learn to block attacks." / "Click the Action Button on your Creature to make him block." — passed when the creature blocks, repeated every 10 seconds | todo | blocking by clicking works, but there is no guide and `SetCreatureQueueFightMove` is a stub |
| The guide swings a middle blow and three seconds later says "Ah. Sorry about that." if it is reeling itself, otherwise "That's it. A good block, there."; then "You'll stop blocking if you do another command." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Dodging: "Let us concentrate on dodging." / "Click the Action Button anywhere on the ground to move the Creature there." — passed when the creature steps, repeated every 10 seconds with a reworded line | todo | stepping by clicking works, but there is no guide and the check is a stub |
| "That's it. You have learnt well." / "Let us try some friendly combat to test these skills." — the healing stops, both fight by themselves again, both at full fight health | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| During the bout, every 5 seconds: good advisor "Well struck!" when the creature is hitting, "Ooh! That must have hurt!" when it reels, evil advisor "Go on. Hit him!" when it is idle | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The bout is stopped (both sit) when the player's creature drops below 0.3 fight health; the guide's own health is never really read, the script reading the player's creature's twice | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| After the bout the guide is healed and rested and grows straight back to twice size; the Ogre's silver scroll is unlocked and the leash comes back | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); the Ogre: [the_ogre.md](silver_scrolls/the_ogre.md) |
| The guide: "That was an important lesson, friend." then, because of the same slip, always "You show great potential. But work on your technique." (its "I'm sorry I had to do that." / "But it was for your own good." for a lost bout is never reached) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide walks to the creature; in a two-shot it is taught the powered healing miracle and casts it on the creature, whose life is set full | todo | `CreatureSetKnowsAction` is a stub; healing itself: [../miracles/](../miracles/) |
| "I have a secret for you. But I warn you, this knowledge is dangerous." / "But come only when you are ready. What I will tell you is forbidden knowledge, dangerous even to gods." / "Why don't you try out what you've learnt while I sleep?" | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide is left wanting only to sleep; the creature's development script is switched off again | todo | `SetCreatureOnlyDesire` is a stub |

## The last lesson (the storm)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guide walks back to the fight clearing and sleeps for five minutes under a gold scroll | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Clicking it then: "I still need rest. We will talk later."; and if the Ogre's scroll hasn't been started, the camera shows the Ogre's place: "Why don't you try out what you've learnt while I sleep?" | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| After five minutes it wakes and walks to a spot below the mountain; the beckoning, pointing and good advisor's notice start again until it is clicked | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| If the creature is further than 40: "Please. Use the Leash to bring your Creature to me.", waiting for it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| With the guide's theme the creature is walked up beside it and they look at each other: "Come, little one. The most important lesson of all awaits." (summoning animation) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| As the camera creeps in over 30 seconds: "It's the key to this world." / "It's the very secret of godliness." — the guide slows to 8 and walks up the mountain as the camera follows: "And it's the way you'll triumph." / "Follow me to the mountain. I will tell you everything there." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The player must bring the creature up the mountain; while the guide waits at the top and the creature is further than 35 and the guide is on screen, it keeps beckoning | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| When the creature is within 100 of the top, a rain storm forms some way off and drifts towards it: full rain, overcast 0.8, dark cloud (150 clouds, shade 1.5, at height 250), sheet lightning only, inner and outer reach 150 and 500, not blown by the wind, lasting practically for ever | todo | `CREATE` of a weather thing and the `Change*Properties` natives are real (`src/Magic/Script/CHLWeather.cpp`); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| With Nemesis's theme the creature is stopped and the camera circles it: the guide: "I must tell you the secret of the Creed. It's the source of godly power." Nemesis: "Creature. Speak no more." The guide, frightened, turns to the storm: "Nemesis! My old master has returned!" then "Quickly. Come close, my friend, and listen well." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| With the creature within 40 of the top, the storm rushes onto the guide (speed 40): Nemesis: "Silence, Traitor! You must not pass on the secret of the Creeds!" (the guide sad); the guide: "Nemesis. He is so determined to be the only god." / "He wants to destroy all other gods and their Creatures. He wants ultimate power!"; Nemesis: "Silence! You will die for this." | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| The guide: "Find the three parts of the Creed which are aligned! This is the only defence against Nemesis." / "Now run! Get away from here!" / "Leave! You can't help me! Seek an ally to help you find the Creeds!" (angry, three times) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| "Now run! Get away from here!" is said once more if the guide is on screen with the creature still beside it; five seconds later the guide dies | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## The guide's death

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With Nemesis's theme the creature is held looking at the guide while the camera dollies round it | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Six lightning bolts (the powered-up lightning miracle) strike the guide from points 60 above the mountain, curling, 0.5 to 3 seconds apart, while it rages | todo | `SpellAtThing` is real (`src/Magic/Script/CHLSpells.cpp`), but the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| Evil advisor, clinging to the screen's edge: "Uh oh. This don't look good." | todo | `ClingSpirit` and `RunText` are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| An explosion miracle falls from the storm onto the guide, and the storm starts forking lightning (one to three, then up to ten forks) | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); the weather natives are real |
| The guide is killed for good (it doesn't faint or get carried home); its body stays on the mountain top | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`); the dead-for-ever state is a creature action, a stub |
| Evil advisor: "He's toast."; the camera ends looking between the guide's dead feet | todo | `RunText` and the camera are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The player's creature becomes fully mature | todo | `SetCreatureDevStage` is real on our player's creature; maturity itself: [../creature/development_phases.md](../creature/development_phases.md); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The Lost Flock and the Pied Piper silver scrolls, if started and unfinished, are closed and their scripts stopped | todo | `StopScript` and `StopScriptsInFiles` are real; see [the_lost_flock.md](silver_scrolls/the_lost_flock.md) and [the_pied_piper.md](silver_scrolls/the_pied_piper.md); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| No reward is given for the guide's lessons: what the creature keeps is the dance, any miracle it watched, the fighting practice and its maturity | todo | the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |

## After the storm

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The storm heads for the player's Norse village at speed 30 | todo | the storm's moves by script were not checked; the guide is never made: `CREATE` of a creature is not done (`CreateScriptObject`) |
| 40 seconds later, with Nemesis's theme, the camera pans round the village under the storm: evil advisor: "Your Village is being trashed by Nemesis' storm!" | todo | real camera, music and advisor natives; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| With a water miracle available, good advisor: "We'd better use our Water Miracle to put out the flames."; otherwise "The entire place is getting wrecked!" and "Yes, we must see if there's anything we can do for them."; evil advisor: "It does look like they need help." | todo | advisors and texts are real; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| A minute later the storm moves on; 30 seconds after that the way out opens: a yellow beam marks where the vortex will be and the good advisor asks "This is hopeless. Wait. What's this golden light?" while the evil advisor points at it | todo | `SpecialEffectPosition`, `SpiritEject` and `SpiritPointPos` are real; the beam and the vortex scenes: [portals.md](./portals.md); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| The storm then keeps circling the village between five places, moving every minute, for as long as it exists | todo | the storm's moves by script were not checked; never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |
| Music: the guide's theme for its scenes, Nemesis's theme for the storm, the second and fourth epic themes for first sight and the won village | todo | `StartMusic` plays these tracks (`src/Audio/Services/GameMusic`); never reached: a no-skip game stops at Choose Your Creature's gate stones, and the guide is never made (`CREATE` of a creature is not done) |

## Saves, other modes and unused material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A saved game keeps where the guide's story is, the guide, its scroll and the storm | todo | openblack has no saved games |
| The guide exists only in the first land of the story; not in skirmish, multiplayer or the Creature Isle | n/a | nothing to do beyond the story itself |
| The development stages "meeting its guide", "the guide explains the history" and "helping a town" are never set by any shipped script (the guide's chapters jump from making friends to miracles, impressing, fighting and full maturity) | n/a | data quirk |
| An older, unshipped set of lessons: a horse guide explaining its past on the mountain (Nemesis's temple, its loneliness), teaching miracles with fireball and heal seeds, helping and impressing a town, a friendship scene where it shrinks itself with a miracle | n/a | in the source but not in the program |
| An unshipped storm brewing scene and older stand-alone storm, meeting and fight scripts | n/a | not in the program |
| A shipped but never started script for taking over the land's villages, where the advisors guess the big creature wants them to be the Aztec village's god | n/a | only started by an unshipped control script |
| Test scripts that run the whole guide story or the storm alone | n/a | not started by the game |
| A cut line in a later land says the Creed lies in the body of the one the player knew as the Guide | n/a | commented out of the fourth land's script |
