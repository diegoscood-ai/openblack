# The Shaolin

A silver scroll on the third land. A guru leaves his mountain temple to meditate in a secret place and asks the player
not to follow him. The player has to follow him anyway, unseen, all the way down the mountain to his hidden spot. If
the player succeeds, the guru later comes back, while the creature is being freed, to start a Wonder for the player's
people.

**Land:** 3 · **Giver:** the guru (a Shaolin monk) at the mountain temple above the Japanese village; the scroll itself stands by the Japanese village · **Script:** Shaolin · **Reward:** a Wonder begun for the player about 20 minutes into the creature's rescue (only if this scroll was done first) · **Repeatable:** no (failed attempts restart until it is done)

The land as a whole is in [../land_3.md](../land_3.md), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

Sources: the land's challenge script source (checked against the PC game's compiled `challenge.chl`), the game's text
table (`Scripts/InfoScript2.txt`) and the executable. openblack is judged on this tree: of the 53 script functions the
quest and the scripts it starts call, none is a stub in `src/CHLApi.cpp` since 2026-10-28 (`GAME_THING_CAN_VIEW_CAMERA`,
the seen check, was the last; the in-hand read is real), but Land 3's control script
never runs (the story reaches it only through the earlier lands' exits; `LOAD_MAP` changes land now, not yet checked), so the guru never exists and every row below is pending unless the
notes say otherwise. The reward scene calls 28 (none missing; `BuildBuilding` works).

**Progress: 0/61 done, 0 chosen, 5 partial, 0 inferred — 3%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background as soon as the land begins, alongside the creature's rescue, the swap to an ape and the tree puzzle | pending | `LandControl3` has not yet run in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real ([map-loading](../../../../scripts/map-script-reader.md#pending)); see [../../scripts/land3_script.md](../../scripts/land3_script.md) |
| The guru is made at once, at his temple on the mountain, as a special "Shaolin" villager | pending | `Create` makes villagers (`CreateScriptObject` in `src/CHLApi.cpp`); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He cannot be picked up by the hand, hurt by fire or set alight for the whole quest | partial 30% | the three flag commands work (`SetIdPickupable`, `SetHurtByFire`, `SetSetOnFire` in `src/CHLApi.cpp`), but there is no guru to set them on; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Whether fire hurts an object", "Whether an object can be set on fire", [land3_script.md](../../scripts/land3_script.md) "The story moves on to this land when Land 2 is finished"; still open: no quest run's PASS is cited for it |
| He walks slowly to his standing place just outside the temple and waits there | pending | `MoveGameThing` works for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll does not appear until the player has some belief in the Japanese village (at least 0.02), checked every 5 seconds; the land's notes call this "waits until the player starts to take over the Japanese town" | pending | `BeliefForPlayer` is real (towns); not checked in play |
| A silver scroll then appears beside the Japanese village, at the foot of the mountain, not at the temple | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Clicking the scroll (or its spot on the ground) starts the quest | pending | `GameThingClicked` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's good Tibetan music is attached to the temple | partial 40% | `AttachMusic` works (`src/Audio`); the script never runs; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Fixes a piece of music to an object so it is heard near it", [land3_script.md](../../scripts/land3_script.md) "The story moves on to this land when Land 2 is finished"; still open: no quest run's PASS is cited for it |
| A cinematic begins (letterbox): the camera rises from above the Japanese village and sweeps up the mountain to the temple over 4 to 6 seconds, then closes in on the guru, who is drawn in high detail and faces it | partial 40% | the letterbox, the camera glides, `SetFocus` on villagers and `SetHighGraphicsDetail` work; the cinematic never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Brings the black cinema bars in or out", "Glides the camera's eye to a position over a number of seconds", "Glides the camera's point of view to a position over a number of seconds", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Turns an object to face a position", "Draws an object in full detail however far away it is", [land3_script.md](../../scripts/land3_script.md) "The story moves on to this land when Land 2 is finished"; still open: no quest run's PASS is cited for it |
| He plays a standing ambient animation three times | pending | `OverrideStateAnimation` works for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll is entered in the player's challenge log as "The Shaolin", not yet done, with the good advisor's reminder "We should follow this guru carefully, Leader" | pending | `Snapshot` is real |
| Guru: "Ah, I see a god. Greetings." | pending | `RunText` and `TextRead` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "I am a guru and a yogi." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "I am off to meditate and increase my powers." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "I have a secret place for this. Please don't follow me there." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "That is all." (his sign-off after every speech) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera follows him from behind and above as he takes his first steps away from the temple, then the cinematic ends | pending | `SetFocusFollow` and `SetPositionFollow` work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Then, in an ordinary dialogue with both advisors out: evil, "You know what? Let's follow him." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Evil: "But let's make sure he doesn't see us." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Good: "Really. Following yogis isn't my idea of godly behaviour." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Following the guru

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guru walks a fixed path of 24 points down the mountainside, from the temple to a hidden spot by the water far below | pending | `MoveGameThing` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He walks slowly (speeds of 0.2 and 0.3 between points) | pending | the speed property works for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| At four places he stops to play a "looking for something" or "inspecting" animation, twice by mushrooms, before walking on | pending | `OverrideStateAnimation` and `Played` work for villagers; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| If the camera is more than 40 from him, he speeds up to 0.7 until his next point, so lagging behind makes him harder to keep up with | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Caught out by being thrown: if he is flying (thrown, for instance by the creature), once he lands he turns to the camera in despair and says "You followed me so I'm going back home now."; the good advisor points at him: "Oh. He's stopped."; the evil advisor: "I'm not surprised. You probably bust every bone in his body." | pending | the flying property works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Caught out by being held: if he is being held, once let go he turns to the camera in despair, says the same line, and the evil advisor points at him: "Oh real smart. He knows we're here now." | pending | the in-hand property is real (`GetProperty`, `src/CHLApi.cpp`); the hand can never pick him up, so this only happens if something else holds him (undetermined what can) |
| Losing him: if he is not on screen, or the camera is more than 70 from him, he is gone at once: he is put back at his temple and the good advisor says "Gosh, he's quick. He gave us the slip there." and "He's bound to be back at his Temple later." | pending | `GameThingFieldOfView` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Being seen: if he can see the camera (it is within the half circle in front of him), a second later he stops, turns to the camera in despair and says "You followed me so I'm going back home now."; the evil advisor points at him: "He saw us. Boss, we've got to try again. Rats." | pending | `GameThingCanViewCamera` is ported (2026-10-28, `field_of_view::ThingCanViewCamera`: the monk, a villager, by his Y angle); the quest is not run yet |
| The checks are made in that order (flying, held, lost, seen), over and over while he walks | pending | the held check is real; the seen check is a stub (above) |

## Trying again

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After any failure he vanishes in puffs of smoke (where he stood and at his standing place by the temple), and two seconds later is back at the temple, in plain view (the script does not wait for him to be off screen) | pending | `SpecialEffectPosition` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He waits there until the camera comes within 25 of him, playing a despairing animation | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cinematic as the camera closes in: "You followed me earlier.", "Please don't try it again.", "That is all." (said even when he was merely lost, not caught) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| He sets off again; the camera follows him from behind as in the introduction | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Checkpoints: once he has passed the twelfth point the attempt restarts at the eleventh, and once past the seventeenth at the sixteenth; the screen fades to black and back in with him standing at the checkpoint and the camera behind him | partial 40% | fades work (`SetFade`, `SetFadeIn`, `FadeFinished`) and the camera can be set at once (`SetCameraPosition`, `SetCameraFocus`); the script never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Puts the camera's eye at a position at once", "Points the camera at a position at once", "Fades the screen to a colour over a time", "Fades the screen back in over a time", "Whether the fade has finished", [land3_script.md](../../scripts/land3_script.md) "The story moves on to this land when Land 2 is finished"; still open: no quest run's PASS is cited for it |
| A lasting sparkle (the "command succeeded" spot effect) marks the latest checkpoint on the ground | pending | `SpecialEffectPosition` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| There is no limit on attempts and no time limit; the quest only ends by reaching the hidden spot (or if the guru stops existing) | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When he reaches his last point unseen, two clouds of butterflies appear at the hidden spot for six minutes and the checkpoint sparkle goes | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A cinematic: the guru, in high detail, sits down cross-legged and, after two seconds, rises into the air and floats there, bobbing gently up and down | pending | the height property (`SetProperty` y position) and the animations work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The scroll is marked done in the challenge log ("The Shaolin", success 1, alignment unchanged) and the silver scroll goes | pending | `Snapshot` is real; `ObjectDelete` works |
| Guru: "Your stealth surprised me." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "You know the secret of my special place of meditation." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "If you will remain quiet I will help you on this land." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "Rest assured I'll appear when needed." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "That is all." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The camera pulls back over six seconds to look down on him and the cinematic ends | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Once he is off screen and the camera is more than 50 away, he stops floating and, five seconds later, is put back at his temple, where he stays | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Doing the quest changes neither alignment nor belief by itself; there is no immediate prize | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |

## The reward: the guru's Wonder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reward is given from the gold scroll that frees the creature: when the second prison statue is stopped and the player owns the village guarded by it, the possessed wolves attack, and at that same moment, if the guru's quest is already done, the Wonder scene is started | pending | the creature's rescue never runs either (see [../land_3.md](../land_3.md)); the same finished quest brings the monk to turn most of the possessed wolves into cows ([../gold_scrolls/the_wolves_are_possessed.md](../gold_scrolls/the_wolves_are_possessed.md#the-monk-helps)) and to hand over two water miracles against the burning fishermen ([../gold_scrolls/fire_fire_im_on_fire.md](../gold_scrolls/fire_fire_im_on_fire.md#the-monk-helps)) |
| The scene waits 20 minutes first (skipped when the scripts' debug flag is set) | pending | a plain script sleep, handled by the script machine itself, but the scene is never started |
| The screen fades to black; a guru is made on a ledge above the land, facing the camera, and cannot be picked up | partial 40% | the fade and pick-up flag work and `Create` makes villagers; the scene never runs; finished by [challenge_natives_camera.md](../../scripts/challenge_natives_camera.md) "Fades the screen to a colour over a time", [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can pick an object up", "Create makes a villager", [land3_script.md](../../scripts/land3_script.md) "The story moves on to this land when Land 2 is finished"; still open: no quest run's PASS is cited for it |
| A cinematic fades in on him. Guru: "It seems you require help." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| A building planned on the land below is turned into a building site and begun (a tenth built), with a high desire to finish it | pending | `BuildBuilding` and the built property work; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| An info sign (bronze scroll) is put beside it: "Wonders are special buildings which power up certain aspects of each tribe. They also increase your influence. This Wonder will cause more people to be born in the Village. And their worship power will also be boosted." | pending | the shared info-sign script; `CreateHighlight` works; the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "That last Village is causing you trouble." | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "So I have given your people the ability to build a Wonder." while the camera rises over four seconds | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "It will give you greater influence across this land." while the camera sweeps down to the new building site over eight seconds | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Guru: "That is all." and the cinematic ends | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The guru made for this scene is never removed; with the first guru kept at the temple, two gurus can be on the land at once | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| Which tribe's Wonder is planned at that spot comes from the land's own data, not the script | pending | undetermined: the building planned at that spot was not read from the land file |

## Quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Wonder is checked for only once: if the guru's quest is finished after the second prison statue falls and its village is won, the reward is lost for good | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
| The quest's main script never ends: after success it waits for the guru at the temple to stop existing, which normally never happens | pending | the quest has not yet started in play: the story has not yet been run out of Land 2 since Khazar's and Lethys's hands (`COMPUTER_PLAYER_READY`) became real |
