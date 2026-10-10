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
quest and the scripts it starts call, 3 only log "not implemented" in `src/CHLApi.cpp`, but Land 3's control script
never runs (the land-loading command does nothing), so the guru never exists and every row below is todo unless the
notes say otherwise. The reward scene calls 28 (none missing; `BuildBuilding` works).

**Progress: 0/66 done, 5 partial — 4%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background as soon as the land begins, alongside the creature's rescue, the swap to an ape and the tree puzzle | todo | `LandControl3` never runs: `LOAD_MAP` is empty ([map-loading](../../../bw1-notes/map-loading.md#pending)); see [../../scripts/land3_script.md](../../scripts/land3_script.md) |
| The guru is made at once, at his temple on the mountain, as a special "Shaolin" villager | todo | `Create` makes villagers (`CreateScriptObject` in `src/CHLApi.cpp`); the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| He cannot be picked up by the hand, hurt by fire or set alight for the whole quest | partial | the three flag commands work (`SetIdPickupable`, `SetHurtByFire`, `SetSetOnFire` in `src/CHLApi.cpp`), but there is no guru to set them on |
| He walks slowly to his standing place just outside the temple and waits there | todo | `MoveGameThing` works for villagers; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The scroll does not appear until the player has some belief in the Japanese village (at least 0.02), checked every 5 seconds; the land's notes call this "waits until the player starts to take over the Japanese town" | todo | `BeliefForPlayer` is a stub |
| A silver scroll then appears beside the Japanese village, at the foot of the mountain, not at the temple | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| While the scroll is unclicked, whenever the camera is within 100 of it and it is on screen, the good advisor steps out every 30 seconds or more, points at it and says "Your godly attention is required here, Leader." | todo | the shared notify script's natives work (`SpiritEject`, `SpiritPointPos`, `RunText`); the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Clicking the scroll (or its spot on the ground) starts the quest | todo | `GameThingClicked` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The temple's good Tibetan music is attached to the temple | partial | `AttachMusic` works (`src/Audio`); the script never runs |
| A cinematic begins (letterbox): the camera rises from above the Japanese village and sweeps up the mountain to the temple over 4 to 6 seconds, then closes in on the guru, who is drawn in high detail and faces it | partial | the letterbox, the camera glides, `SetFocus` on villagers and `SetHighGraphicsDetail` work; the cinematic never runs |
| He plays a standing ambient animation three times | todo | `OverrideStateAnimation` works for villagers; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The scroll is entered in the player's challenge log as "The Shaolin", not yet done, with the good advisor's reminder "We should follow this guru carefully, Leader" | todo | `Snapshot` is a stub |
| Guru: "Ah, I see a god. Greetings." | todo | `RunText` and `TextRead` work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "I am a guru and a yogi." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "I am off to meditate and increase my powers." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "I have a secret place for this. Please don't follow me there." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "That is all." (his sign-off after every speech) | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The camera follows him from behind and above as he takes his first steps away from the temple, then the cinematic ends | todo | `SetFocusFollow` and `SetPositionFollow` work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Then, in an ordinary dialogue with both advisors out: evil, "You know what? Let's follow him." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Evil: "But let's make sure he doesn't see us." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Good: "Really. Following yogis isn't my idea of godly behaviour." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Following the guru

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The guru walks a fixed path of 24 points down the mountainside, from the temple to a hidden spot by the water far below | todo | `MoveGameThing` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| He walks slowly (speeds of 0.2 and 0.3 between points) | todo | the speed property works for villagers; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| At four places he stops to play a "looking for something" or "inspecting" animation, twice by mushrooms, before walking on | todo | `OverrideStateAnimation` and `Played` work for villagers; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| If the camera is more than 40 from him, he speeds up to 0.7 until his next point, so lagging behind makes him harder to keep up with | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Caught out by being thrown: if he is flying (thrown, for instance by the creature), once he lands he turns to the camera in despair and says "You followed me so I'm going back home now."; the good advisor points at him: "Oh. He's stopped."; the evil advisor: "I'm not surprised. You probably bust every bone in his body." | todo | the flying property works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Caught out by being held: if he is being held, once let go he turns to the camera in despair, says the same line, and the evil advisor points at him: "Oh real smart. He knows we're here now." | todo | the in-hand property (`GetProperty`) is not ported; the hand can never pick him up, so this only happens if something else holds him (undetermined what can) |
| Losing him: if he is not on screen, or the camera is more than 70 from him, he is gone at once: he is put back at his temple and the good advisor says "Gosh, he's quick. He gave us the slip there." and "He's bound to be back at his Temple later." | todo | `GameThingFieldOfView` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Being seen: if he can see the camera (it is within the half circle in front of him), a second later he stops, turns to the camera in despair and says "You followed me so I'm going back home now."; the evil advisor points at him: "He saw us. Boss, we've got to try again. Rats." | todo | `GameThingCanViewCamera` is a stub |
| The checks are made in that order (flying, held, lost, seen), over and over while he walks | todo | the held and seen checks need stubs (above) |

## Trying again

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After any failure he vanishes in puffs of smoke (where he stood and at his standing place by the temple), and two seconds later is back at the temple | todo | `SpecialEffectPosition` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| He waits there until the camera comes within 25 of him, playing a despairing animation | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A cinematic as the camera closes in: "You followed me earlier.", "Please don't try it again.", "That is all." (said even when he was merely lost, not caught) | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| He sets off again; the camera follows him from behind as in the introduction | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Checkpoints: once he has passed the twelfth point the attempt restarts at the eleventh, and once past the seventeenth at the sixteenth; the screen fades to black and back in with him standing at the checkpoint and the camera behind him | partial | fades work (`SetFade`, `SetFadeIn`, `FadeFinished`) and the camera can be set at once (`SetCameraPosition`, `SetCameraFocus`); the script never runs |
| A lasting sparkle (the "command succeeded" spot effect) marks the latest checkpoint on the ground | todo | `SpecialEffectPosition` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| There is no limit on attempts and no time limit; the quest only ends by reaching the hidden spot (or if the guru stops existing) | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When he reaches his last point unseen, two clouds of butterflies appear at the hidden spot for six minutes and the checkpoint sparkle goes | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A cinematic: the guru, in high detail, sits down cross-legged and, after two seconds, rises into the air and floats there, bobbing gently up and down | todo | the height property (`SetProperty` y position) and the animations work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The scroll is marked done in the challenge log ("The Shaolin", success 1, alignment unchanged) and the silver scroll goes | todo | `Snapshot` is a stub; `ObjectDelete` works |
| Guru: "Your stealth surprised me." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "You know the secret of my special place of meditation." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "If you will remain quiet I will help you on this land." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "Rest assured I'll appear when needed." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "That is all." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The camera pulls back over six seconds to look down on him and the cinematic ends | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Once he is off screen and the camera is more than 50 away, he stops floating and, five seconds later, is put back at his temple, where he stays | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Doing the quest changes neither alignment nor belief by itself; there is no immediate prize | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## The reward: the guru's Wonder

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The reward is given from the gold scroll that frees the creature: when the second prison statue is stopped and the player owns the village guarded by it, the possessed wolves attack, and at that same moment, if the guru's quest is already done, the Wonder scene is started | todo | the creature's rescue never runs either (see [../land_3.md](../land_3.md)); the same finished quest brings the monk to turn most of the possessed wolves into cows ([../gold_scrolls/the_wolves_are_possessed.md](../gold_scrolls/the_wolves_are_possessed.md#the-monk-helps)) and to hand over two water miracles against the burning fishermen ([../gold_scrolls/fire_fire_im_on_fire.md](../gold_scrolls/fire_fire_im_on_fire.md#the-monk-helps)) |
| The scene waits 20 minutes first (skipped when the scripts' debug flag is set) | todo | a plain script sleep, handled by the script machine itself, but the scene is never started |
| The screen fades to black; a guru is made on a ledge above the land, facing the camera, and cannot be picked up | partial | the fade and pick-up flag work and `Create` makes villagers; the scene never runs |
| A cinematic fades in on him. Guru: "It seems you require help." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| A building planned on the land below is turned into a building site and begun (a tenth built), with a high desire to finish it | todo | `BuildBuilding` and the built property work; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| An info sign (bronze scroll) is put beside it: "Wonders are special buildings which power up certain aspects of each tribe. They also increase your influence. This Wonder will cause more people to be born in the Village. And their worship power will also be boosted." | todo | the shared info-sign script; `CreateHighlight` works; the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "That last Village is causing you trouble." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "So I have given your people the ability to build a Wonder." while the camera rises over four seconds | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "It will give you greater influence across this land." while the camera sweeps down to the new building site over eight seconds | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Guru: "That is all." and the cinematic ends | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The guru made for this scene is never removed; with the first guru kept at the temple, two gurus can be on the land at once | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| Which tribe's Wonder is planned at that spot comes from the land's own data, not the script | todo | undetermined: the building planned at that spot was not read from the land file |

## Quirks and bugs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The Wonder is checked for only once: if the guru's quest is finished after the second prison statue falls and its village is won, the reward is lost for good | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The quest's main script never ends: after success it waits for the guru at the temple to stop existing, which normally never happens | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The "held" failure can never come from the hand, since he cannot be picked up | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| After being merely lost, the guru still says "You followed me earlier." | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
| The scroll sits by the Japanese village while the guru waits at the temple far up the mountain; the introduction carries the camera from one to the other | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |

## Unused and cut

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The text table has five guru-quest lines no script says: evil, "He saw us! I say we leave him and come back later."; guru, "You tried to follow me before." and "Please leave me with my secrets."; "What was that?"; and "Leader, we're not managing to impress anyone in this place." | n/a | cut dialogue; nothing to play |
| A wait for the guru to be off screen before he resets after a failure is commented out in the source, so he resets in plain view | todo | the quest never starts: no Land 3 (`LOAD_MAP` is empty) |
