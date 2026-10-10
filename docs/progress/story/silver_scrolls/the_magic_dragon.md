# The Magic Dragon

A silver scroll on the fifth land: five worn-out crusaders camped by a cave want to fight the dragon inside. The player
heals them, then lights their pyre so its smoke fills the cave, then follows the unseen fight by listening at the
cave's five blowholes. Three crusaders stagger out of the far exit with the dragon's treasure: a flying-flock miracle
dispenser. The land as a whole is in [../land_5.md](../land_5.md), dispensers in
[../rewards.md](../rewards.md) and [../../miracles/dispensers_and_seeds.md](../../miracles/dispensers_and_seeds.md).

**Land:** 5 · **Giver:** the crusaders' leader, at their camp by the cave mouth near the Tibetan town · **Script:**
Crusaders · **Reward:** a flying-flock miracle dispenser (doves or bats) by the cave's exit, refilling every 5 minutes,
and an influence ring round it · **Repeatable:** no

Sources: the quest's script source (checked against the decompiled shipped `challenge.chl`, where it is compiled), the
land's control script, the shared notify, reminder and dispenser-reward scripts, the game's text table
(`InfoScript2.txt`). openblack's state is judged on this tree: openblack starts the story's top script, which always
begins with Land 1's control script, and the land-loading function does nothing, so Land 5's control script never runs
and this quest never starts. Of the 70 commands it needs, only 3 log "not implemented" in `src/CHLApi.cpp` (the help
reads and the challenge record); creating villagers, highlights, effects, timers and dispensers works
(`CreateScriptObject`). Every row is todo unless the notes say otherwise.

**Progress: 0/67 done, 15 partial — 11%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 5's control script starts the quest in the background right after the land's opening (Nemesis's curse on the creature), together with the land's other first quests | todo | `LandControl5` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| Nothing has to be done first; the camp is there from the start of the land | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The quest's title is "The Magic Dragon" (title text 113) | todo | `Snapshot`, `UpdateSnapshot` and `UpdateSnapshotPicture` are stubs |
| No other script waits for it or reads its outcome | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The crusaders come back in the game's ending: its first credits shot ("Black & White", "Designed and Created by Lionhead Studios Ltd.") shows three crusaders sitting and talking at this cave's exit, whether or not the quest was done | todo | the ending sequence in the land's control script; see [../ending.md](../ending.md); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cave's entrance and exit and the five blowhole craters along the hill between them are made indestructible | partial | `SetIndestructable` works and `CallNear` finds the features; the script never runs |
| A small cauldron (a fifth of normal size) is put in the camp | todo | `CreateWithAngleAndScale` makes mobile objects; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Five crusaders (the land's special crusader villagers) are made round the camp and put in one group that keeps within 10 to 25 of its centre | todo | `Create`, `FlockCreate` and `FlockAttach` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Each crusader is indestructible, cannot be picked up, cannot be hurt by fire and cannot be set on fire | partial | the flag commands work (`SetIndestructable`, `SetIdPickupable`, `SetHurtByFire`, `SetSetOnFire`), but the crusaders are never made |
| All five sit down for good and face the camp; the group's life is set to 0.6 (worn out from "our last adventure") | todo | the health property (`SetProperty`) is not ported |
| A wood pile (a wood store holding 9000 wood) is the pyre, just outside the cave mouth; for now it is indestructible, cannot be picked up, cannot burn and cannot be set on fire | partial | the flags work; CHL `Create` does not make stores (`CreateScriptObject` in `src/CHLApi.cpp`) |
| Five small helper scripts, one per crusader, wait for the charge into the cave | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## How it appears (the advisor nag)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver challenge scroll hangs 20 above the camp | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| While it waits, whenever the camera is within 100 of it and it is on screen, the evil advisor steps out, points at it and says "Hey. There's a job for you. Wanna do it?", at most once every 30 seconds and only when no other scene is playing | todo | the shared notify script's natives work (`SpiritEject`, `SpiritPointGameThing`, `RunText`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The quest waits for this: it goes on only when the scroll, or the camp spot, is clicked; the scroll then takes its opened look | todo | `GameThingClicked` and `PositionClicked` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The introduction: "heal us"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader is drawn in high detail; the camera swings in to the camp (position over 3 seconds, focus over 2), then creeps closer over 20 seconds | partial | `StartMusic` works; the camera and high detail natives work too, but the scene never runs |
| One crusader whittles a stick throughout; the leader gets up and looks exhausted; two others get up, one sinking back to sit twice, one once | todo | the villagers' script animations work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The scroll is recorded with progress 0 and the reminder "These brave followers are in need of healing, Leader." (spoken by the good advisor when asked for) | todo | `Snapshot` is a stub |
| The leader faces the camera (man's voice): "Holiest One, we worship and adore you." | todo | `RunText` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The leader: "In your name we'll fight the dragon in this cave, once we've recovered from our last adventure." The camera cuts to look into the cave mouth, a dragon's roar sounds from it, and the camera drifts over 6 seconds | partial | the cut, the camera moves and `PlaySoundEffect` work; the scene never runs |
| The leader turns; the camera cuts back to the camp: "If you'd be so good as to heal us, the dragon would stand no chance." while he looks exhausted twice | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The music stops and the scene ends; the leader and the two others sit back down, a quarter of a second apart | partial | `StopMusic` works |

## Healing the crusaders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must bring all five crusaders to full life (each at 1 or more); the heal miracle is the way to do it | partial | the heal miracle heals people (`src/Magic/Spells/SpellHeal.cpp`, [../../miracles/heal.md](../../miracles/heal.md)), but the crusaders don't exist and the health read is not ported |
| There is no time limit and nothing can go wrong: they cannot die, burn or be carried off | todo | needs the health read |

## The second scene: "light the pyre"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader stands up in high detail and faces the camera, which swings round (position over 3 seconds, focus over 2) and then drifts over 30 seconds | partial | `StartMusic` works |
| The leader: "Thank you, Holiest. We're nearly ready to take on the pesky dragon." | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera's focus turns to the cave mouth over 6 seconds; the leader walks to the pyre, faces the cave, and the camera cuts to look at the mouth, drifting over 8 seconds | partial | the cut and the camera moves work |
| The leader prods the pyre like a campfire; the scroll's progress becomes 0.33 with the reminder "Light the pyre and the caped crusaders can get on with it." | todo | `UpdateSnapshot` is a stub |
| The leader: "Our plan is to fill the cave with smoke from this pyre." He turns back to the camp | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| From now on the pyre can burn: it loses its indestructibility and can be hurt by fire and set alight | partial | `SetIndestructable`, `SetHurtByFire`, `SetSetOnFire` work, but the pyre (a store) isn't made |
| The crusaders liven up (gesturing, gossiping, looking impressed); the leader: "The dragon will be unable to see and we'll storm in and dispatch him." and dances | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera moves back (2 seconds); the leader walks towards it: "There's one problem. We have no means of lighting the pyre." shrugging twice, then "Can you help us with this?" | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The leader walks back to his place, the music stops, the scene ends and all five sit down one after another | partial | `StopMusic` works |

## Lighting the pyre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits until there is fire within 10 of the cave's doorway; the pyre is about 3 from it, so setting it alight (a fireball, a burning object thrown on it) is enough, and any other fire that close counts too | partial | the test works (`IsFireNear`, `src/Magic/Script/CHLFire.cpp`), but the pyre is never made and the script never runs |
| No time limit; nothing reminds the player beyond the scroll's reminder | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Smoking out the dragon (the charge)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts (no music): thick smoke (eight times normal size) pours from the pyre for 30 seconds; 3 seconds later the camera moves in above it (3 seconds), holds 4 seconds | todo | `SpecialEffectPosition` works; the scale property works only for villagers and animals; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera rises high above the cave mouth and pans along the hill from the first blowhole to the fifth over 28 seconds while smoke bursts out of each blowhole in turn, 3 seconds apart (each eight times size, for 30 seconds) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The pyre is emptied and removed | todo | `AddResource` and `ObjectDelete` work; no store is made |
| The camera cuts to look across the camp to the cave and pulls back slowly over 25 seconds; the scroll's progress becomes 0.66 with the reminder "Let's listen at the blowholes to follow their valiant progress." | todo | `UpdateSnapshot` is a stub |
| The charge: the leader walks to the rally point at the mouth, looks into the cave and back, points and cries "Charge!" | todo | `GamePlaySaySoundEffect` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The other four get up (three of them stand from sitting first) and gather round the rally point; once all five are there the leader shouts "Run forward quickly!" and all run into the cave at 0.7 speed (one a second later), fading away as they reach the doorway | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The scene ends once the last crusader has gone | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Listening at the blowholes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 10 seconds the fight is followed blowhole by blowhole, strictly in order from the cave mouth's end: each one plays only when the camera is near it (within 50 for the first, within 30 for the others) and the blowhole is on screen | todo | `CallNear`, `GetDistance` and `PosFieldOfView` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The voices are heard as spoken sound lines in crusaders' voices rather than as dialogue boxes | todo | undetermined whether their subtitles show; `GamePlaySaySoundEffect` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| First blowhole: smoke (10 seconds); "Cough! Cough!", 2 seconds later "Who's' that?" (the stray apostrophe is in the game's text), 4 seconds later "Get off! That's me!", 2 seconds later "Can you see him?" | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Second: smoke; "Cough! Cough!", then "What's that golden shining, oh Alan, why didn't you go before?", 4 seconds later the dragon roars, then "Go for his head, Barry!" | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Third: smoke; "Tally-Ho!", 2 seconds later "At 'em, lads!", then the dragon roars | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Fourth: smoke; the dragon-fight sound, then "Ralph's dead!" and "Albert's copped it. Flee!" and the camera shakes (within 50, amplitude 0.5, 2 seconds) | todo | `ShakeCamera` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The dragon is never seen: there is no dragon in the cave or anywhere, only its roars and the fight sound | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| There is no limit on how long the player takes; nothing reminds them which blowhole is next beyond the scroll's reminder | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Out of the exit: the treasure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reaching the fifth blowhole starts a scene: the camera moves to look over it and the gate (3 seconds); smoke, a roar, the camera shakes (within 100, amplitude 0.25, 1 second); 4 seconds later "We've got the treasure, Scarper to the exit, lads!" | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera moves to the cave's exit (4 seconds); smoke (half the pyre's size) bursts from it for 20 seconds | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Three crusaders (two did not make it) come out of the exit in high detail, indestructible, unpickable and unhurt by fire (this time not protected from catching fire), at life 0.4, and run out with the running-while-on-fire animation at speeds 0.7, 0.6 and 0.55; one shouts "Run forward quickly!" | todo | `Create` makes villagers and the flags work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| They stop in front of the exit: the leader faces the camera and pants, one pants, one sits down | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A sparkle marks the spot and a flying-flock miracle dispenser appears there, refilling every 5 minutes | partial | spell dispensers are made by `CreateWithAngleAndScale` (`src/Worship/SpellDispenser.cpp`) and the flying flock miracle exists (`src/Magic/Spells/SpellFlock.cpp`); see [../../miracles/flocks.md](../../miracles/flocks.md); the script never runs |
| The player gains an influence ring of radius 50 round the spot, which is never taken away | todo | `InfluencePosition` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The leader: "Victory! We bring you the treasure we found inside the cave.", "Take this with our compliments.", "We will leave it here as a holy shrine to you. It might be useful, too." | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The two standing crusaders sit down; the scroll's progress becomes 1 (complete) and the scroll is removed | todo | `UpdateSnapshot` is a stub |
| The dispenser's own presentation runs too: the reward sting, the camera flies to a view 21.5 to its side and 14 up (4 seconds), and the evil advisor, pointing, says "Nice. Another of those cool Miracle Dispensers." (or, if it were the first dispenser of the game, explains dispensers and puts up a signpost), then the dispenser's own help lines | todo | the shared dispenser-reward script; undetermined exactly how the two scenes are ordered; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three survivors wait up to 2 minutes for the player to heal them to full life | todo | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining` work; needs the health read (not ported) |
| Healing them or not makes no difference: the script notes which happened but never uses it (no alignment change, no extra reward) | todo | quirk; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| They then walk back into the cave's exit one after another and fade away | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| What stays: the dispenser and the influence ring at the exit; the cave features stay indestructible; the cauldron stays at the old camp | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Script quirks and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One of the four followers (the second) is never told to stand up before walking to the rally point, unlike the other three | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Cut: each crusader was to idle by looking for something and looking exhausted in turn through each stage; the code is commented out (two followers keep an empty wait loop from it) | n/a | not in the compiled script |
| Cut: a base-camp object, a wait for the camera to be away before making the camp, a dragon murmur and a camera shake at the fourth blowhole in the leader's own spot are commented out | n/a | not in the compiled script |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The two camp scenes play the creature guide's theme and stop it at their end; the smoke, blowhole and treasure parts start no music | partial | `StartMusic` and `StopMusic` work, but the script never runs |
| Sounds: the dragon's roar (four times), the dragon-fight sound, the reward sting (from the dispenser) | partial | `PlaySoundEffect` works (`src/Audio`); the script never runs |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part to play; it could cast heal or set the pyre alight on the player's behalf, which the script cannot tell apart from the player doing it | todo | the script checks only the crusaders' life and fire near the cave; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
