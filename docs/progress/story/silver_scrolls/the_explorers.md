# The Explorers

A silver scroll of the first land: three sailors (the script and the text call them missionaries) camp on a beach
beside an unfinished ark and want to sail away. Each time they need something they sing a verse about it, to an
accordion: wood to finish the boat, then grain, then meat. Once they have everything they sail off and leave a Water
miracle dispenser behind. Killing them, or leaving them waiting for three and a half hours, ends the challenge with an
evil mark. If they do sail, they come back on the fifth land ("The Explorers Again", see [../land_5.md](../land_5.md)).

**Land:** 1 · **Giver:** three sailors at a campfire beside an ark in dry dock, on a beach the good advisor places
"behind those huge Gates" · **Script:** TheMissionaries · **Reward:** a Water miracle dispenser on the beach ·
**Repeatable:** no

Sources: the challenge's script source (`TheMissionaries.txt`, checked against the PC game's compiled `challenge.chl`),
the shared helper scripts it runs (the scroll notice, the dispenser reward), the game's text table and the executable.
openblack's state is judged on this tree: of the 84 script functions the challenge and the helper scripts it runs need,
9 still only log "not implemented" in `src/CHLApi.cpp` (among them the held-object checks, the store's resources and the
challenge log). On top of that, the land's control script starts this challenge only in a game that skips to the
creature choice (the start-up box's fourth answer; see [../../scripts/land1_script.md](../../scripts/land1_script.md)).
Every row is todo unless the notes say otherwise. The script program as a whole is in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

**Progress: 0/89 done, 29 partial — 16%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the challenge in the background once the creature has been chosen (or straight away when a new game skips to the creature choice), alongside the creature breeder | partial | with the fourth tutorial answer `LandControl1` starts it with the creature breeder; with the other answers the land's control script never gets that far |
| The challenge script also waits until the creature choice is finished before putting up the camp and the scroll, which matters only when the opening is skipped | partial | with the fourth answer the land sets the creature-choice flag when it loads the profile's creature, so the camp goes up; not checked in game |
| The ark is a dry-dock building made at the start, already a fifth built, and it can't be destroyed, hurt by fire or set alight | partial | CHL `CREATE` makes the dry dock feature, the three flags work and `SetProperty` sets its built amount (`src/ECS/FeatureBuild.cpp`); not checked in game |
| A ring of the player's influence, radius 150, is put round the ark, so the hand can reach the beach; it is never taken away, not even when the ark sails | partial | `InfluencePosition` works (`src/Magic/Script/CHLInfluence.cpp`); not checked in game |
| A campfire (a bonfire object) is placed between the sailors' three seats, can't be moved or picked up and is set burning | partial | CHL `CREATE`, `SetIdMoveable`, `SetIdPickupable` and `SetOnFire` work; not checked in game |
| The guide's first lesson suggests this scroll first when the player hasn't opened it (or was already sent here and hasn't finished it): "You have not investigated many Silver Reward Scrolls, Leader. Try a few - it'll be worth it.", flying the camera to the beach | todo | the creature guide never runs in our tree; see [../creature_guide.md](../creature_guide.md) |
| When the challenge ends by the time running out, the main script is stopped before it can mark itself finished, so the guide would keep suggesting it | todo | the timeout is never reached (see the deaths below) |
| If the ark sails, a flag is set that the fifth land reads to bring the sailors back ("The Explorers Again"); killing them or letting the time run out loses that challenge | todo | the fifth land is never reached (`LOAD_MAP` is empty): [../land_5.md](../land_5.md) |

## The scroll and the advisors' hints

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver scroll appears 16 above the ark | partial | `CreateHighlight` (`src/ECS/ScriptHighlight.cpp`); not checked in game |
| While it isn't clicked, every 30 seconds that the camera is within 100 of the scroll with the scroll on screen and the widescreen free, the evil advisor comes out, points at it in the world and says "A Silver Reward Scroll. Let's see what it's all about." | partial | the shared notice script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`, `GameThingFieldOfView`); not checked in game |
| Clicking the scroll or the ark itself starts the challenge, and the scroll is switched to its active look | partial | `GameThingClicked` and `SetActive` on a scroll work; not checked in game |
| A second hint runs alongside until the scroll is clicked: when the camera comes within 100 of a point near the gates, the good advisor comes out, points at the scroll and says "There's a Silver Reward Scroll on the beach behind those huge Gates."; after that it waits 250 seconds before it can say it again | partial | the hint's natives work (`CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining`, the advisor ones); not checked in game |

## The sailors arrive and sing the first verse

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| On the click, three sailors come out of the ark's door: two plain sailors and an accordion player, who carries the challenge's background accordion tune with him | partial | CHL `CREATE` makes the villagers and `AttachMusic` works (`src/Audio`); not checked in game |
| A three-and-a-half-hour time limit starts from the click | partial | `CreateTimer` works; the limit is never reached (see the deaths below) |
| The first verse starts at once, as long as no sailor is in the hand or the creature's hand (the verse scene always checks this and is skipped otherwise) | partial | the verse runs; `GetObjectHeld` and `InCreatureHand` are stubs answering no, so a held sailor is not noticed |
| The idle and building animations are stopped; the sailors walk at a slow 0.4 to their three seats round the fire (a sailor more than 15 away who isn't flying is put straight onto his seat; a flying one is waited for until he lands) and they are drawn in high detail | partial | `MoveGameThing` (villagers), `SetPosition`, `SetHighGraphicsDetail` work; not checked in game |
| The camera glides high over the beach, the first verse's music starts, then the camera drops low to the fire | partial | the camera moves and the verse's music (`StartMusic`, `src/Audio`) work; not checked in game |
| The challenge is recorded with its title "The Explorers", no success and no alignment yet, and its reminder is this first verse scene, so clicking the scroll again replays the song | todo | `Snapshot` is a stub |
| The accordion tune is moved to the third seat's sailor, who plays the accordion for the rest of the challenge; the second sits down and sways to the song; the first beckons, looks lost, then gossips | partial | `DetachMusic`, `AttachMusic` and the villager animation commands work; not checked in game |
| The camera cuts between close shots of the singers every 3 seconds, then the second sailor stands and despairs, and the camera sweeps along the ark and back to the fire | partial | the camera natives work; not checked in game |
| The scene ends only when the song's words have finished; then the first two sailors go back to idling | partial | not checked in game |
| The idle loop, at random: walk to the fire and prod it, whittle a stick, shrug, look puzzled or despair, then sit down, sway to a sailing song for 4 to 8 loops and stand up; it ends when the sailor dies | todo | (inferred) cut short: `GetProperty` has no health and answers 0, so the sailors are taken for dead at the first check after the first verse (see the deaths below) |
| After the first verse, the evil advisor: "How dare they leave? After all we've done for them?"; the good advisor: "What have we done for them, exactly? Perhaps we should help them out. It might be nice." | partial | `StartDialogue`, `RunText`, `TextRead` work; not checked in game |

## The three verses (words on screen)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Each verse's twelve lines are shown one at a time, each when the music reaches that line, or after 10 seconds if the music gives no cue; the last waits until read | partial | `LastMusicLine` works; the word highlight that follows the song is not drawn (`src/Help`); not checked in game |
| Verse one (wood): "Ooooh, we've got this notion" / "That we'd quite like to sail the ocean" / "So we're buildin' a big boat to leave here for good." / "We're not keen on sinkin'" / "So we're all sittin' here a thinkin'" / "Cos we built it too big and we've run out of wood." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave til we get some more wood." / "Oooh, we're not keen on sinkin'" / "so that's why we're sittin' thinkin'" / "cos we simply can't leave til we get some more wood." | partial | sung in the sailors' (man's) voice; the first verse plays; not checked in game |
| Verse two (grain): "Ooooh, the boat is now finished" / "But there's still somethin' on the wish-list" / "To keep us all goin' through the wind and the rain." / "There's no food on the table" / "And we can't sail unless we're able" / "So we ain't goin' nowhere 'til we get some grain." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave until we get some grain." / "Therrrre's no food on the table" / "And we can't sail unless we're able" / "So we ain't goin' nowhere 'til we get some grain." | todo | never reached (inferred, see the deaths below) |
| Verse three (meat): "Ooooh, We're not complainin'" / "but there's still one more thing remaining" / "Cos bread is quite borin' if that's all you eat." / "We need some flavour" / "So do us all a little favour" / "Cos, we ain't goin' nowhere 'til we get some meat." / "eidle eidle eee" / "eidle eidle eee" / "we simply can't leave until we've got some meat." / "Sooooo, Do us a favour" / "find somethin' with a little flavour" / "Cos we're goin' nowhere 'til we've got some meat." | todo | never reached (inferred) |
| The songs are shown as known dialogue, so they don't wait for the player to click on through them | partial | not checked in game |

## Wood: finishing the boat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Gifts are only looked for while all three sailors are alive | todo | never reached (inferred): the sailors count as dead |
| Every 3 seconds the script looks for a wood store (a pile of wood dropped by the hand) within 40 of the ark | todo | never reached (inferred); `CallNear` works |
| If one is there and no sailor is in either hand, its wood is counted and the pile fades away | todo | never reached; `GetResource` is a stub |
| Each 1,500 wood builds a whole ark's worth; the ark starts a fifth built, so 1,200 wood in all finishes it; the target is capped at fully built | todo | never reached |
| The building scene: the sailors walk at 0.4 to three spots along the ark and hammer, look overworked, saw, carve or swing a sledgehammer at random (1 to 5 loops, then a 4 to 10 second pause) until the ark is fully built | todo | never reached |
| A sailor says "Thank you for this wood."; the camera glides to the ark, cuts, then pulls back over it | todo | never reached |
| The ark grows on screen by 0.03 every half second, with a random small woodpile sound each step, until it reaches the new target | todo | never reached (`SetProperty` built amount and `PlaySoundEffect` work) |
| If the ark still isn't finished: "Can we have some more wood please?" | todo | never reached |
| The sailors stay at work by the ark until it is finished, not by the fire | todo | never reached |
| A tree dropped within 40 of the ark (checked every 3.4 seconds) brings the good advisor out once in the whole challenge: "A tree is no good. These people require prepared wood." | todo | never reached (`SpiritAppear`, `SpiritDisappear` work) |

## Verse two: grain

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the ark is fully built, verse two plays (if no sailor is held) and grain becomes the next need | todo | never reached; the cap at fully built was not traced |
| Sailors that are flying are waited for, the others are put on their seats; all are drawn in high detail | todo | never reached |
| The camera glides to the start of a recorded camera path and follows it while the second verse's music plays | partial | the music, `RunCameraPath` and the camera conversions work; never reached (inferred) |
| The first sailor sits and sways, the accordion keeps playing; at the fourth line the second sailor is put on the ark's deck, walks along it, faces the camera and looks unimpressed, then is put back beside the ark and walks back to his seat to gossip | todo | never reached |
| The challenge is recorded again: success 0.4, alignment 0.2, with verse two as the reminder | todo | never reached; `Snapshot` is a stub |
| The camera closes in on the fire with cuts every 3 seconds; when the words end, the first two sailors go back to idling | todo | never reached |
| Every 3 seconds the script looks for a food store within 40 of the ark; its food is added up and the pile fades away | todo | never reached; `GetResource` is a stub |
| While the player answers, the interface is limited to moving the hand | partial | the interaction level works (`SetInterfaceInteraction`); never reached (inferred) |
| Under 300 food in all: "That's good grain. Please can we have more?"; 300 or more: "We're grateful for the grain, Holy One." | todo | never reached |

## Verse three: meat

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| At 300 food, verse three plays (if no sailor is held) and meat becomes the last need | todo | never reached |
| The sailors are put back on their seats as in the first verse; the third verse's music starts and the words follow | partial | the music works (`StartMusic`); never reached (inferred) |
| The first sits and sways, the accordion plays, the second gossips; the record is updated to success 0.5, alignment 0.1, with verse three as the reminder | todo | never reached; `UpdateSnapshot` is a stub |
| The camera glides round the fire with cuts every 3 seconds, then cuts on the seventh, ninth and tenth lines of the song, ending in two quick moves | todo | never reached |
| Every 3 seconds the script looks within 40 of the ark for a cow, a sheep, a horse or a pig, in that order, and takes one per check, fading it away | todo | never reached (`CallNear`, `ObjectDelete` work) |
| A cow: "Great. Cattle to eat."; a sheep: "A sheep. Lovely. Sheep have many uses. And the voyage is long."; a pig: "A pig. Yeah, we'll have that."; after the first animal: "We need more meat, though." | todo | never reached |
| Two animals are needed; the scroll is then taken away and a sailor says "Thanks for all this meat." | todo | never reached |
| The interface is limited to moving the hand while they talk | partial | the interaction level works; never reached (inferred) |
| Quirk: a horse gets no line of its own; as the first animal it is answered "Thanks for all this meat." and then "We need more meat, though." | todo | never reached |

## Extra hands: villagers, a woman and a galley boy

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every 3.3 seconds, while the ark is on screen and the camera within 200, the script looks for a villager within 40 of the ark who isn't held by a script | todo | never reached (`GameThingFieldOfView`, `CallNear` work) |
| Before the boat is built, with no sailor dead: "Some more company is nice but we need to get the boat finished." | todo | never reached |
| Once the boat is built, a man is taken aboard: "Hurrah! Another shipmate!"; he walks to the ark's door and vanishes, lost to his village | todo | never reached; `SexIsMale` is a stub |
| A woman is taken too: "This young lady will keep us all in check during the voyage." / "Go inside. You have your own cabin." | todo | never reached; `SexIsMale` is a stub |
| While a sailor is dead, a man is taken as a replacement: "Great! Another sailor. Just what we need!" / "Go inside and pick out a uniform, mate." | todo | never reached |
| While a sailor is dead, a woman is turned away: "It'd be lovely to have you along, but we need a man to steer the big heavy rudder." | todo | never reached |
| After a refusal the script waits until that villager is more than 40 from the ark, or 20 seconds | todo | never reached |
| Each man taken while a sailor is dead brings a new sailor out of the door, who walks to the dead one's seat and starts idling; the death count goes back down | todo | never reached |
| Once the boat is built, the first child brought within 40 (checked every 3.1 seconds) becomes the galley boy: "Great! Someone to scrub the decks! Get inside!" | todo | never reached |
| The galley boy is put on the ark's deck scrubbing it for good, facing the camera, which closes on him and pulls back with a lens change over 6 seconds, then returns to where it was | todo | never reached (`SetCameraLens`, `MoveCameraLens` work) |
| Only one galley boy is taken; he leaves with the ark | todo | never reached |

## Hurting the sailors

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Every pass of the main loop checks each sailor for having died or vanished, whatever killed him (hand, creature, miracle) | todo | `ThingValid` works but `GetProperty` has no health and answers 0, so (inferred) every sailor counts as dead at the first check |
| The first seat's sailor dying: "Oh my god. You killed Kenneth!"; the others: "You killed my shipmate! Outrageous!" (not said for the last of the three) | partial | (inferred) the lines play, but wrongly, right after the first verse (see above); not checked in game |
| A death records success at the dead share (a third per sailor) and alignment -0.2, then at once 0.3 and -0.4 if the second sailor still lives, or -0.6 if only the third does | todo | `UpdateSnapshot` is a stub |
| If the accordion player dies, the tune moves: a surviving sailor is swapped for an accordion player in the same seat, who plays on | todo | not checked: with every sailor taken for dead at once (inferred) |
| With a sailor dead, the reminder becomes "They haven't got enough shipmates." (good advisor) and no gifts are accepted until a man replaces him | todo | the reminder needs `Snapshot`, a stub |
| All three dead: the evil advisor: "You killed all the explorers!" / "But I can't get their stupid song out of my brain."; the record is set to success 1, alignment -1, and the challenge ends with no reward | partial | (inferred) this ending plays, wrongly, right after the first verse; the record is a stub (`UpdateSnapshot`); the ark, the campfire and the scroll stay |

## Running out of time

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After three and a half hours the sailors walk to the ark's door; the record is set to success 1, alignment -0.6 | todo | never reached (the deaths end it first, inferred) |
| The good advisor's voice: "The Missionaries have given up waiting for you, Leader."; the sailors fade away | todo | never reached |
| Every script of the challenge is stopped; the ark, the campfire and the scroll stay | todo | never reached; `StopScriptsInFilesExcluding` works |

## Setting sail

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When wood, grain and meat are all given, all three sailors are alive, no verse is playing and no sailor is held, the campfire and the galley boy are taken away and the departure starts | todo | never reached |
| The sailors and the ark are removed and a hand-made film of the ark sailing away starts (its own two boat animations from the game's misc data) | todo | never reached; `PlayJcSpecial` does play the boat film (`src/CHLApi.cpp`, water.md) |
| A heave sound, then two cheers 3.5 seconds apart as the camera glides along the boat | todo | never reached (`PlaySoundEffect` works) |
| The camera turns to the sea and a sailor says "Thank you, highness. We won't forget you, you know." as the third epic theme starts | partial | the music works (`StartMusic`); never reached |
| The dialogue box closes; a cheer, a close shot of the boat, then shots far out to sea to the west | todo | never reached (`GameCloseDialogue` works) |
| The record is set to success 1, alignment 0.2, with a picture taken | todo | never reached; `UpdateSnapshotPicture` is a stub |
| Back over the beach, the good advisor: "I just hate goodbyes."; the evil advisor: "I just hate good." | todo | never reached |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A Water miracle dispenser is built on the beach by the old camp, facing angle 0, and switched on | partial | CHL `CREATE` makes spell dispensers and `SetMagicProperties`, `SetActive` work (`src/Magic/Script/CHLWorship.cpp`); never reached; see ../../miracles/dispensers_and_seeds.md and ../../miracles/water.md |
| Its refill time is left at the dispenser's own (the script asks for 0 seconds, which the engine ignores for a non-timer object) | todo | never reached |
| The reward sting plays and the camera glides to a view 21.5 to the side and 14 up from the dispenser over 4 seconds | todo | never reached; see [../rewards.md](../rewards.md) |
| If it is the player's first dispenser: the evil advisor: "This pedestal is a Miracle Dispenser. It charges up and generates one-shot Miracles."; a signpost is put beside it ("A Miracle Dispenser gives out one-shot Miraculous Wonders when it's fully charged."), "Click on the signpost for more info.", and a "did you know" scroll about miracles hidden around the land is placed ("That there are Miracles hidden all over Eden. Keep your eyes peeled.") | todo | never reached; shared with every dispenser reward ([../rewards.md](../rewards.md)) |
| Otherwise: "Nice. Another of those cool Miracle Dispensers." | todo | never reached |
| The dispenser's own help lines are then spoken | todo | never reached; `GetFirstHelp`, `GetLastHelp` are stubs |
| A storm reward falling from the sky was the original reward and is left commented out | n/a | not in the compiled scripts |

## Unused and cut material

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| An advisor line "It looks like the men are getting ready to set sail." (good advisor) is in the text table but no script says it | n/a | not used by the shipped scripts |
| A sailor's line "How are we going to sail the ship now we don't have a full crew ?" is in the text table but no script says it | n/a | not used by the shipped scripts |
| The quest's text calls the sailors missionaries throughout, though the scroll's title is "The Explorers" | n/a | naming only |
