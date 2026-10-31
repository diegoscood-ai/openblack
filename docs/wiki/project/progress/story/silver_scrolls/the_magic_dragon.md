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
begins with Land 1's control script, and the story reaches it only through the earlier lands' exits (`LOAD_MAP` changes land now, not yet checked), so Land 5's control script never runs
and this quest never starts. Of the 70 commands it needs, none is a stub in `src/CHLApi.cpp` at this tree (the help reads,
the challenge record and the health read and set are real); creating villagers, highlights, effects, timers and dispensers works
(`CreateScriptObject`). Every row is pending unless the notes say otherwise.

**Progress: 0/66 done, 0 chosen, 15 partial, 0 inferred — 10%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 5's control script starts the quest in the background right after the land's opening (Nemesis's curse on the creature), together with the land's other first quests | pending | `LandControl5` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| Nothing has to be done first; the camp is there from the start of the land | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest's title is "The Magic Dragon" (title text 113) | pending | `Snapshot`, `UpdateSnapshot` and `UpdateSnapshotPicture` are real |
| No other script waits for it or reads its outcome | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The crusaders come back in the game's ending: its first credits shot ("Black & White", "Designed and Created by Lionhead Studios Ltd.") shows three crusaders sitting and talking at this cave's exit, whether or not the quest was done | pending | the ending sequence in the land's control script; see [../ending.md](../ending.md); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The cave's entrance and exit and the five blowhole craters along the hill between them are made indestructible | partial 40% | `SetIndestructable` works and `CallNear` finds the features; the script never runs; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Finds an object of a kind within a radius of a position", "Makes an object impossible to destroy", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| A small cauldron (a fifth of normal size) is put in the camp | pending | `CreateWithAngleAndScale` makes mobile objects; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Five crusaders (the land's special crusader villagers) are made round the camp and put in one group that keeps within 10 to 25 of its centre | pending | `Create`, `FlockCreate` and `FlockAttach` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Each crusader is indestructible, cannot be picked up, cannot be hurt by fire and cannot be set on fire | partial 40% | the flag commands work (`SetIndestructable`, `SetIdPickupable`, `SetHurtByFire`, `SetSetOnFire`), but the crusaders are never made; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Makes an object impossible to destroy", "Whether fire hurts an object", "Whether an object can be set on fire", "Create makes a villager", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| All five sit down for good and face the camp; the group's life is set to 0.6 (worn out from "our last adventure") | pending | the health property is real (`SetProperty`, `src/CHLApi.cpp`; a group sets each member); the crusaders don't exist in play yet |
| A wood pile (a wood store holding 9000 wood) is the pyre, just outside the cave mouth; for now it is indestructible, cannot be picked up, cannot burn and cannot be set on fire | partial 60% | CHL `Create` of a store makes it as the original: a magic wood pile of one unit, nobody's (`CreateScriptObject` in `src/CHLApi.cpp`, `pot_resource::CreateScriptStore`; [the generic create of a store](../../../../scripts/map-script-reader.md#the-generic-create-of-a-store)), then `AddResource` fills it; the flags work; not yet reached in play, and no run's PASS closes it; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Makes an object impossible to destroy", "Whether fire hurts an object", "Whether an object can be set on fire", "Create makes a store", [challenge_natives_towns_and_players.md](../../scripts/challenge_natives_towns_and_players.md) "Puts food or wood into a store", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| Five small helper scripts, one per crusader, wait for the charge into the cave | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## How it appears (the advisor nag)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A silver challenge scroll hangs 20 above the camp | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest waits for this: it goes on only when the scroll, or the camp spot, is clicked; the scroll then takes its opened look | pending | `GameThingClicked` and `PositionClicked` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The introduction: "heal us"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader is drawn in high detail; the camera swings in to the camp (position over 3 seconds, focus over 2), then creeps closer over 20 seconds | partial 60% | `StartMusic` works; the camera and high detail natives work too, but the scene never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Draws an object in full detail however far away it is", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| One crusader whittles a stick throughout; the leader gets up and looks exhausted; two others get up, one sinking back to sit twice, one once | pending | the villagers' script animations work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll is recorded with progress 0 and the reminder "These brave followers are in need of healing, Leader." (spoken by the good advisor when asked for) | pending | `Snapshot` is real |
| The leader faces the camera (man's voice): "Holiest One, we worship and adore you." | pending | `RunText` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The leader: "In your name we'll fight the dragon in this cave, once we've recovered from our last adventure." The camera cuts to look into the cave mouth, a dragon's roar sounds from it, and the camera drifts over 6 seconds | partial 40% | the cut, the camera moves and `PlaySoundEffect` work; the scene never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", "Glides the camera's eye to a position over a number of seconds", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The leader turns; the camera cuts back to the camp: "If you'd be so good as to heal us, the dragon would stand no chance." while he looks exhausted twice | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The music stops and the scene ends; the leader and the two others sit back down, a quarter of a second apart | partial 40% | `StopMusic` works; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Stops the script's music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Puts a villager, animal or other living thing into one of its script states", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |

## Healing the crusaders

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player must bring all five crusaders to full life (each at 1 or more); the heal miracle is the way to do it | partial 20% | the heal miracle heals people (`src/Magic/Spells/SpellHeal.cpp`, [../../miracles/heal.md](../../miracles/heal.md)), and the health read is real (`GetProperty` in `src/CHLApi.cpp`), but the crusaders don't exist in play yet; finished by [heal.md](../../miracles/heal.md) "Each person is healed the moment their glow appears", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Reads the health of an object", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| There is no time limit and nothing can go wrong: they cannot die, burn or be carried off | pending | the health read is real; the quest is not reached in play |

## The second scene: "light the pyre"

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts with the creature guide's theme; the leader stands up in high detail and faces the camera, which swings round (position over 3 seconds, focus over 2) and then drifts over 30 seconds | partial 40% | `StartMusic` works; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object to face a position", "Draws an object in full detail however far away it is", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The leader: "Thank you, Holiest. We're nearly ready to take on the pesky dragon." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera's focus turns to the cave mouth over 6 seconds; the leader walks to the pyre, faces the cave, and the camera cuts to look at the mouth, drifting over 8 seconds | partial 40% | the cut and the camera moves work; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes a living thing walk to a position", "Turns an object to face a position", [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", "Glides the camera's point of view to a position over a number of seconds", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The leader prods the pyre like a campfire; the scroll's progress becomes 0.33 with the reminder "Light the pyre and the caped crusaders can get on with it." | pending | `UpdateSnapshot` is real |
| The leader: "Our plan is to fill the cave with smoke from this pyre." He turns back to the camp | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| From now on the pyre can burn: it loses its indestructibility and can be hurt by fire and set alight | partial 60% | `SetIndestructable`, `SetHurtByFire`, `SetSetOnFire` work and the pyre (a store) is made; not yet reached in play; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes an object impossible to destroy", "Whether fire hurts an object", "Whether an object can be set on fire", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The crusaders liven up (gesturing, gossiping, looking impressed); the leader: "The dragon will be unable to see and we'll storm in and dispatch him." and dances | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves back (2 seconds); the leader walks towards it: "There's one problem. We have no means of lighting the pyre." shrugging twice, then "Can you help us with this?" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The leader walks back to his place, the music stops, the scene ends and all five sit down one after another | partial 40% | `StopMusic` works; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Puts a villager, animal or other living thing into one of its script states", "Makes a living thing walk to a position", [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Stops the script's music", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |

## Lighting the pyre

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest waits until there is fire within 10 of the cave's doorway; the pyre is about 3 from it, so setting it alight (a fireball, a burning object thrown on it) is enough, and any other fire that close counts too | partial 40% | the test works (`IsFireNear`, `src/Magic/Script/CHLFire.cpp`), but the pyre is never made and the script never runs; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether there is fire within a radius of a position", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| No time limit; nothing reminds the player beyond the scroll's reminder | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Smoking out the dragon (the charge)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts (no music): thick smoke (eight times normal size) pours from the pyre for 30 seconds; 3 seconds later the camera moves in above it (3 seconds), holds 4 seconds | pending | `SpecialEffectPosition` works; the scale property sets any thing with a place (`SetProperty` in `src/CHLApi.cpp`, `ecs::living::SetThingScale`); whether that makes the effect's smoke bigger is not checked; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera rises high above the cave mouth and pans along the hill from the first blowhole to the fifth over 28 seconds while smoke bursts out of each blowhole in turn, 3 seconds apart (each eight times size, for 30 seconds) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The pyre is emptied and removed | pending | `AddResource` and `ObjectDelete` work and the store is made; not yet reached in play |
| The camera cuts to look across the camp to the cave and pulls back slowly over 25 seconds; the scroll's progress becomes 0.66 with the reminder "Let's listen at the blowholes to follow their valiant progress." | pending | `UpdateSnapshot` is real |
| The charge: the leader walks to the rally point at the mouth, looks into the cave and back, points and cries "Charge!" | pending | `GamePlaySaySoundEffect` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The other four get up (three of them stand from sitting first) and gather round the rally point; once all five are there the leader shouts "Run forward quickly!" and all run into the cave at 0.7 speed (one a second later), fading away as they reach the doorway | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scene ends once the last crusader has gone | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Listening at the blowholes

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After 10 seconds the fight is followed blowhole by blowhole, strictly in order from the cave mouth's end: each one plays only when the camera is near it (within 50 for the first, within 30 for the others) and the blowhole is on screen | pending | `CallNear`, `GetDistance` and `PosFieldOfView` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The voices are heard as spoken sound lines in crusaders' voices rather than as dialogue boxes | pending | undetermined whether their subtitles show; `GamePlaySaySoundEffect` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| First blowhole: smoke (10 seconds); "Cough! Cough!", 2 seconds later "Who's' that?" (the stray apostrophe is in the game's text), 4 seconds later "Get off! That's me!", 2 seconds later "Can you see him?" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Second: smoke; "Cough! Cough!", then "What's that golden shining, oh Alan, why didn't you go before?", 4 seconds later the dragon roars, then "Go for his head, Barry!" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Third: smoke; "Tally-Ho!", 2 seconds later "At 'em, lads!", then the dragon roars | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Fourth: smoke; the dragon-fight sound, then "Ralph's dead!" and "Albert's copped it. Flee!" and the camera shakes (within 50, amplitude 0.5, 2 seconds) | pending | `ShakeCamera` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The dragon is never seen: there is no dragon in the cave or anywhere, only its roars and the fight sound | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| There is no limit on how long the player takes; nothing reminds them which blowhole is next beyond the scroll's reminder | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Out of the exit: the treasure

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Reaching the fifth blowhole starts a scene: the camera moves to look over it and the gate (3 seconds); smoke, a roar, the camera shakes (within 100, amplitude 0.25, 1 second); 4 seconds later "We've got the treasure, Scarper to the exit, lads!" | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera moves to the cave's exit (4 seconds); smoke (half the pyre's size) bursts from it for 20 seconds | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Three crusaders (two did not make it) come out of the exit in high detail, indestructible, unpickable and unhurt by fire (this time not protected from catching fire), at life 0.4, and run out with the running-while-on-fire animation at speeds 0.7, 0.6 and 0.55; one shouts "Run forward quickly!" | pending | `Create` makes villagers and the flags work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| They stop in front of the exit: the leader faces the camera and pants, one pants, one sits down | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A sparkle marks the spot and a flying-flock miracle dispenser appears there, refilling every 5 minutes | partial 40% | spell dispensers are made by `CreateWithAngleAndScale` (`src/Worship/SpellDispenser.cpp`) and the flying flock miracle exists (`src/Magic/Spells/SpellFlock.cpp`); see [../../miracles/flocks.md](../../miracles/flocks.md); the script never runs; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Starts a special effect (sparkles", "Creates an object of a kind at a position with an angle and a scale", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| The player gains an influence ring of radius 50 round the spot, which is never taken away | pending | `InfluencePosition` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The leader: "Victory! We bring you the treasure we found inside the cave.", "Take this with our compliments.", "We will leave it here as a holy shrine to you. It might be useful, too." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The two standing crusaders sit down; the scroll's progress becomes 1 (complete) and the scroll is removed | pending | `UpdateSnapshot` is real |
| The dispenser's own presentation runs too: the reward sting, the camera flies to a view 21.5 to its side and 14 up (4 seconds), and the evil advisor, pointing, says "Nice. Another of those cool Miracle Dispensers." (or, if it were the first dispenser of the game, explains dispensers and puts up a signpost), then the dispenser's own help lines | pending | the shared dispenser-reward script; undetermined exactly how the two scenes are ordered; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The three survivors wait up to 2 minutes for the player to heal them to full life | pending | `CreateTimer`, `SetTimerTime`, `GetTimerTimeRemaining` work; the health read is real; the quest is not reached in play |
| Healing them or not makes no difference: the script notes which happened but never uses it (no alignment change, no extra reward) | pending | quirk; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| They then walk back into the cave's exit one after another and fade away | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| What stays: the dispenser and the influence ring at the exit; the cave features stay indestructible; the cauldron stays at the old camp | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Script quirks and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| One of the four followers (the second) is never told to stand up before walking to the rally point, unlike the other three | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The two camp scenes play the creature guide's theme and stop it at their end; the smoke, blowhole and treasure parts start no music | partial 40% | `StartMusic` and `StopMusic` work, but the script never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", "Stops the script's music", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |
| Sounds: the dragon's roar (four times), the dragon-fight sound, the reward sting (from the dispenser) | partial 40% | `PlaySoundEffect` works (`src/Audio`); the script never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Plays a sound effect from one of the sound banks", [land5_script.md](../../scripts/land5_script.md) "The story moves on to this land when Land 4 is finished"; still open: no quest run's PASS is cited for it |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature has no part to play; it could cast heal or set the pyre alight on the player's behalf, which the script cannot tell apart from the player doing it | pending | the script checks only the crusaders' life and fire near the cave; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
