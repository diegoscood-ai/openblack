# The Singing Stones (land 2)

A silver scroll on Khazar's land: a priest shows the player a horseshoe of nine singing stones that "hold the spirits of
the ancients". Tapping the stones in the shape of three hidden melodies (Twinkle Twinkle Little Star, Chopin's funeral
march and White Christmas) brings on night with bats and mist, raises the dead inside the ring, or makes it snow. Two
whistling wanderers about the land give away the first two tunes. There is no reward object; each first-time tune adds a
third to the challenge's success.

A cut stone-circle quest, never compiled, is [The Miracle Stones](../../../not-in-the-original.md#storysilver_scrollsthe_miracle_stones).

**Land:** 2 · **Giver:** a priest from the hut above the stones (the scroll stands on the hut) · **Script:**
SingingStonesSongs · **Reward:** none given; the tunes' effects (night with bats, the dead raised, a snowfall) are the
prize · **Repeatable:** yes (every tune can be played again and again; the challenge never closes)

Land 1 has a different quest with the same title (the hippy's stone circle, `SingingStoneCircle`, documented by its own
file); it is not described here. The land as a whole is in [../land_2.md](../land_2.md), the land's control script in
[../../scripts/land2_script.md](../../scripts/land2_script.md), the stones as a puzzle in
[../minigames.md](../minigames.md#singing-stones-lands-1-and-2), the script program in
[../../scripts/challenge_scripts.md](../../scripts/challenge_scripts.md).

Sources: the quest's original script source (`SingingStonesSongs.txt`, 15 scripts, checked against the PC game's
compiled `challenge.chl` for argument orders and object types), the land's control script (`LandControl2.txt`), the
shared notify script (`ChallengeNotify.txt`) and the game's English text table. openblack is judged on this tree: the
quest and the scripts it runs use 78 script functions; the last stub of them, finding the dead (`GET_DEAD_LIVING`,
`GetDeadLiving` in `src/CHLApi.cpp`), is ported since ([../../../../scripts/script-commands.md](../../../../scripts/script-commands.md)); the clicked object, skeletons, the health read, the
container populating and the snapshots are real, as are creating objects, highlights, dialogue, advisors, the camera
moves, sounds, weather and timers. The land's control script never runs (see
[../land_2.md](../land_2.md)), so the quest never appears; every row is pending unless the notes say otherwise.

**Progress: 0/80 done, 0 chosen, 5 partial, 0 inferred — 3%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest at once, in the background, right after the land's set-up and the first did-you-know signposts and before the land's entry scene; no town has to be won and there is no wait | pending | `LandControl2` runs once Land 1 is left, and starts the quest before Khazar's first wait ([map-loading](../../../../scripts/map-script-reader.md#pending)) |
| The nine stones, their bases, both whistlers and the stone-tap listener are made before the scroll appears, so the stones already sing when tapped before the quest is taken (those early taps are wiped when the quest starts) | pending | `Create` makes the singing stones and their bases (mobile statics) and villagers; the tap listener's `GetObjectClicked` is real (`src/CHLApi.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A silver challenge scroll is put on the hut above the stones (a house at about 2135, 3011; no height is given, so the game's default for a challenge scroll) | pending | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Clicking the scroll or the hut itself starts the quest; the scroll is then switched to active | pending | `GameThingClicked` and `SetActive` on a highlight work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A priest is then made at the hut to give the introduction | pending | `Create` makes villagers; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## The stones

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nine singing stones, numbered 1 to 9, stand in a horseshoe of about 10 radius around the circle's centre (about 2105, 3000), open towards the west; each sits on its own singing-stone base | pending | `Create` / `CreateWithAngleAndScale` make the stones and bases (`CreateScriptObject` in `src/CHLApi.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The stones are made at three-quarter size, each turned to face the centre (turned by -10, -27, -44, -70, -100, -130, -140, -160 and -180 degrees from stone 1 to stone 9) | pending | as above; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The stones cannot be picked up or moved by the hand; the bases are not locked by the script | partial 50% | `SetIdPickupable`, `SetIdMoveable` work; the physics treats the stone as heavy and its base as unmovable; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Whether the player can move an object", "Whether the player can pick an object up", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |
| Tapping a stone (clicking it) plays that stone's own note at the stone, flashes a short success sparkle on it (a tenth of a second) and adds the stone's number to the tune being remembered | pending | `GetObjectClicked` is real; `PlaySoundEffect`, `SpecialEffectObject`, `ClearClickedObject` work |
| By day the stones play one set of nine notes (the "twinkle" samples); at night they play a second, darker set (the "funeral" samples) | pending | the taps are read by `GetObjectClicked` (real) and `PlaySoundEffect` plays the samples; not reached in play |
| Only the player's own clicks count; the script never looks at the creature or anything else touching the stones | pending | `GetObjectClicked` is real; not reached in play |
| The stones remember the last 14 taps in a ring: each tap goes in the next of 14 slots, the 15th tap overwriting the first | pending | `GetObjectClicked` is real; not reached in play; rules summary in [../minigames.md](../minigames.md#singing-stones-lands-1-and-2) |
| Each tune is listened for all the time by its own checker, which looks for the tune starting at every one of the 14 slots in turn | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| After any tune is recognised, and whenever the stones change between day and night, all 14 slots and the tap count are wiped | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

## Day and night

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| When the hour of the day is after 22:00 or before 5:20, the nine stones fade away and nine tombstones (three-quarter size, same places and turns) take their place; they too cannot be picked up or moved | pending | `GetGameTime`, `ObjectDelete` and creating the tombstone features work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Outside those hours the tombstones fade away and the singing stones come back, unless the dead are still being raised, in which case the tombstones stay until the five minutes are over | pending | as above; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The funeral march can only be recognised at night, while the tombstones stand; the other two tunes are recognised by day or night, on stones or tombstones (at night they sound in the darker notes) | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene (widescreen, camera under script control) starts with the fourth generic script theme | pending | `StartCameraControl`, `SetWidescreen`, `StartMusic` work (`src/Audio/GameMusic.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The priest is shown in high detail; the camera closes on the hut as he walks slowly out (speed 0.2) | pending | `SetHighGraphicsDetail`, the speed property and `MoveGameThing` work for villagers; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| After 2.5 seconds he goes on down the path to the stones on his own (speed 0.3, then 0.5 for the last stretch) while the camera follows him in three moves and then settles beside the stones | pending | `MoveGameThing` and the camera moves work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Facing the camera, gossiping with his hands: "These stones hold the spirits of the ancients. Each has its own voice." | pending | `RunText`, `SetFocus` and the villagers' script animations work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| "It is said that when a correct tune is played, special powers are unleashed." | pending | `RunText` works; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| "When the stones are tapped the spirit sings." as the camera drifts closer over seven seconds | pending | `RunText` and the camera moves work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| "Few people know the melodies. You may meet them, or discover the melodies for yourself." | pending | `RunText` works; the hint that the whistlers carry two of the tunes; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The dialogue closes, the camera pulls back over the stones, the priest goes back to normal detail | pending | `GameCloseDialogue`, the camera moves and `SetHighGraphicsDetail` work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The challenge is entered in the player's challenge log (the snapshot records a picture, the title "The Singing Stones" and the reminder script) with success 0 and alignment 0; the music stops | pending | `Snapshot` is real |

## The priest

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the introduction the priest walks slowly (speed 0.2) and wanders around the circle's centre within 6 of it, with two further wander settings of 4 and 30 (undetermined: their exact meaning in the wander state) | pending | `SetScriptStatePos`, `SetScriptFloat` and the wander round a place (state 199) are real (checked against v1.42 on 2026-10-20); the speed property works; never reached |
| He is not protected: he can be picked up, thrown or killed | partial 50% | the script sets no flags on him; the hand can pick up and throw villagers; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [picking_up.md](../../hand/picking_up.md) "Villagers can be picked up", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |
| The first time he is found dead, the evil advisor steps out: "Ah… Now that's what I call music." (once only) | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |

## The whistlers (hints)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A whistling wanderer (a pied-piper villager) is made north of the stones (about 2011, 3201) with the whistled Twinkle Twinkle Little Star fixed to him, so it is heard near him | pending | `Create` makes the villager and `AttachMusic` works (`src/Audio`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| He walks a loop of 31 points through the north of the land and back, waiting at each until within 5 of it or 60 seconds pass; at the fifth point he sits down for two minutes | pending | `MoveGameThing`, `GetDistance`, timers and the villagers' script states work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A second whistler is made far to the south (about 2387, 1840) with the whistled funeral march fixed to him; he walks a loop of 24 points, sitting for two minutes at a rest spot on a rise | pending | as above; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Both whistlers cannot be killed, cannot be hurt by fire and cannot be set alight | partial 50% | `SetIndestructable`, `SetHurtByFire`, `SetSetOnFire` work in `src/CHLApi.cpp`; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [challenge_natives_objects_and_world.md](../../scripts/challenge_natives_objects_and_world.md) "Makes an object impossible to destroy", "Whether fire hurts an object", "Whether an object can be set on fire", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |
| Both are made as soon as the land starts the quest, before its scroll is clicked, and walk forever | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| No one whistles White Christmas: that tune must be found by the player | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

## The melodies

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Twinkle Twinkle Little Star is 14 taps on fixed stones: 1, 1, 8, 8, 9, 9, 8, 6, 6, 5, 5, 3, 3, 1 | pending | `GetObjectClicked` is real; not reached in play |
| The funeral march is 11 taps judged by the steps between stones, not the stones themselves, so it can start on any stone: the same stone four times, three stones up, one down, the same, two down, the same, one down, one up (for example 4, 4, 4, 4, 7, 6, 6, 4, 4, 3, 4) | pending | the taps are read (`GetObjectClicked` is real); not reached in play; only listened for at night |
| White Christmas is 8 taps judged by steps, starting anywhere: one up, one down, one down, one up, one up, one up, one up (for example 3, 4, 3, 2, 3, 4, 5, 6) | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| The step shapes match the real tunes if neighbouring stones are a semitone apart (the funeral march's B-flat opening and White Christmas's chromatic climb); with that reading Twinkle's stones give C, D, E, F, G but stone 9 would be a semitone below the tune's A (undetermined: the stones' real pitches would need the sound samples) | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| The tunes can be played in any order; Twinkle's night is the intended way to reach the funeral march, which needs night | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| If more than one tune is waiting, the main loop deals with Twinkle first, then the funeral march, then White Christmas | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

## Twinkle Twinkle Little Star: night falls

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene starts with the Twinkle theme | pending | needs a tune to be recognised; the taps are read (`GetObjectClicked` is real); not reached in play |
| The first time only, a flock of 10 bats is made at the circle's centre (inner radius 15, outer 30); they stay for good | pending | `FlockCreate` and `ChangeInnerOuterProperties` work, `PopulateContainer` is real; never reached |
| Every time, a white mist is made at the centre (size 0.8, faint, low: height ratio 0.6) | pending | `CreateMist` is real (`test/test_script_mist_natives.cpp`); this is the only mist the shipped scripts make |
| The time of day moves to 23:00 over 10 seconds and the clock is started again | partial 50% | `MoveGameTime` and `GameTimeOnOff` work, but the scene needs a recognised tune |
| The camera starts low by the stones, rises straight up over 12 seconds, then swings out wide | pending | the camera moves work; never reached |
| At the end the mist grows from size 1 to 2 and fades from 50 to 0 over 10 seconds, and the music stops; the mist is never deleted (undetermined whether 0 there means fully gone) | pending | `SetMistFade` is real (`ecs::mist_fade`, `test/test_script_mist_natives.cpp`); openblack's draw skips a mist of alpha 0 |
| Because it is now night, the stones turn into tombstones straight after | pending | needs a recognised tune |

## The funeral march: the dead rise

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cinema scene starts with the funeral theme; the camera moves in low beside the stones | pending | needs a recognised tune |
| If the priest is alive he walks into the ring, stops, and faces the camera; the scene waits until he gets there with no time limit | pending | needs a recognised tune; quirk: if he cannot reach the spot the scene would never end (no time-out, unlike the whistlers' 60 seconds) |
| After 10 seconds the raising begins, and a second later the priest says: "Behold, the dead will rise again when placed within the ring." | pending | needs a recognised tune; the source's comment gives an older line, "The Dead shall live." |
| The camera pulls back over the stones and the music stops | pending | needs a recognised tune |
| A healing glow (the singing stones' heal effect, 13.5 times its size) covers the ring for five minutes | pending | `SpecialEffectPosition` and `CreateTimer` work; never reached |
| For those five minutes, any dead body within 10 of the centre is found (about every 0.3 seconds, one at a time), puffs steam, plays a spell sound and is brought back to full health | pending | `GetDeadLiving` is a stub; the sound is number 18 of the spell sound bank (undetermined which sound that is) |
| A dead villager or child is made again in its place as the same kind with the same age, but as a skeleton, and joins the town with id 11 (the Indian town by Lethys's snow edge) if it still exists, whichever town it came from | pending | `SetSkeleton` and `GetTownWithId` are real, but finding the body needs `GetDeadLiving`, a stub; not checked in play; the script calls it the "nearest town", but it is always the same town |
| A dead animal is made again in its place as the same kind | pending | `GetDeadLiving` is a stub |
| Each revived thing is held by the script for 3 seconds, given full health again and let go to live its own life | pending | `ReleaseFromScript` works; needs `GetDeadLiving`; at most about one revival every 3.3 seconds |
| Playing the march again while the raising is running replays the scene and the priest's line but starts no second raising | pending | needs a recognised tune |
| The tombstones stay, even past dawn, until the five minutes end; then the glow goes | pending | needs a recognised tune |

## White Christmas: snow

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A snowstorm is made at the centre: 300 seconds long fading over 5, snow only (no rain, no lightning, temperature 0, fall speed 1), 10 clouds, shade 1, height 70, inner radius 200, outer 500, not blown by the wind | pending | creating a weather thing and `ChangeWeatherProperties`, `ChangeCloudProperties`, `ChangeLightningProperties`, `ChangeTimeFadeProperties`, `ChangeInnerOuterProperties` and `SetAffectedByWind` work (`src/Magic/Script/CHLWeather.cpp`); needs a recognised tune |
| A cinema scene with the Christmas theme sweeps the camera round the stones in four moves (about 30 seconds) | pending | needs a recognised tune |
| The scene never stops its music (the other two scenes do) | pending | undetermined whether ending the cinema stops it |

## Success and the challenge log

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The first time each tune is played, the challenge log entry is updated: success 0.33 for the first tune found, 0.66 for the second, 0.99 for the third, in whatever order they were played | pending | `UpdateSnapshot` is real |
| Once all three have been played, success becomes 1.0 at once | pending | `UpdateSnapshot` is real; quirk: the update to 1.0 is then repeated on every pass of the main loop for the rest of the game |
| Alignment is 0 at every step (the snapshot and every update): nothing in the quest is good or evil | pending | `Snapshot` is real |
| Every entry keeps the title "The Singing Stones" and the reminder script | pending | land 1's stone circle uses the same title text, so both lands' entries read "The Singing Stones" |
| Playing a tune again replays its effects every time but adds no more success | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| There is no way to fail and no time limit; the quest's main loop never ends | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Killing the priest does not end it: only the evil advisor's remark changes, and the funeral scene skips his walk and his line | pending | the health read is real (`GetProperty`, `src/CHLApi.cpp`); not reached in play |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No reward object is given at any point; the quest's effects (night with bats and mist, the dead raised, the snowstorm) are its only payoff | pending | see [../rewards.md](../rewards.md) for the rewards other scrolls give; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The bats from the first Twinkle stay in the land | pending | `PopulateContainer` is real (`src/CHLApi.cpp`); not reached in play |
| Villagers raised by the march stay skeletons, living in town 11 | pending | `SetSkeleton` is real (`src/CHLApi.cpp`); not reached in play |
| The stones keep switching to tombstones every night and back every morning for the rest of the land | pending | the day and night swap's natives work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The whistlers keep walking their loops forever | pending | the walk loop's natives work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Advisors' comments

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Asking for a reminder from the challenge log: the good advisor steps out: "Create melodies on these stones to awaken the spirits of the ancients." then the evil advisor: "Few people know the melodies. But who knows? We may meet them, Boss." | pending | the reminder script's natives (`SpiritEject`, `RunText`) work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Five script themes: the fourth generic theme (introduction), Twinkle, the funeral march, Christmas, and the two whistled versions fixed to the whistlers | partial 50% | all six banks are listed in `src/Audio/GameMusic.cpp`; `StartMusic`, `StopMusic`, `AttachMusic` work; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", "Stops the script's music", "Fixes a piece of music to an object so it is heard near it", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part: no line, no action, and it cannot tap the stones for the player (only the player's clicks are read) | pending | undetermined whether the creature can learn anything from watching |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Because the checkers read the 14 slots as a ring, a tune may run across the point where the newest tap sits next to the oldest: e.g. White Christmas's last notes, then six other taps, then its first notes, is accepted; and any rotation of Twinkle (such as starting from its third note and ending with its first two) is accepted too | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| An empty slot counts as a stone numbered 0, so after a wipe a 13-tap run whose 11th to 13th taps and 1st to 4th taps happen to fit White Christmas around the still-empty 14th slot is accepted | pending | the taps are read (`GetObjectClicked` is real); not reached in play |
| The checkers loop without pausing, testing all 14 starting slots continuously | pending | the taps are read (`GetObjectClicked` is real); not reached in play |

