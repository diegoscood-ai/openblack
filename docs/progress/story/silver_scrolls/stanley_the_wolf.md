# Stanley The Wolf

A silver scroll on the fifth land: a man sitting by a campfire asks the player to guide his blind wolf, Stanley, to a
sheep inside a fenced grid by ringing bells round it (the land's tilt-maze puzzle). Solving it while the man lives wins
a lion creature to swap for and the fireball's second and third levels for the player's home town. The land as a whole
is in [../land_5.md](../land_5.md), the puzzle object itself in [../minigames.md](../minigames.md) ("Lion maze"), the
creature swap in [../challenges_and_rewards.md](../challenges_and_rewards.md).

**Land:** 5 · **Giver:** the wolf's owner, a man sitting by a campfire beside the puzzle · **Script:** LionPuzzle ·
**Reward:** a lion creature offered for a swap, plus the fireball's level 2 and level 3 power-ups in the player's home
town · **Repeatable:** no (the swap offer itself stays open for good)

Sources: the quest's script source (checked against the decompiled shipped `challenge.chl`, where it is compiled), the
land's control script, the shared notify, reminder, "did you know" and creature-swap scripts, the game's text table
(`InfoScript2.txt`), the bw1-decomp source for the engine's death guard and the puzzle's animal types. openblack's state
is judged on this tree: openblack starts the story's top script, which always begins with Land 1's control script, and
the land-loading function does nothing, so Land 5's control script never runs and this quest never starts. Of the 62
commands the quest needs, only 6 log "not implemented" in `src/CHLApi.cpp` (among them the creature's actions, the
creature swap and the challenge log); a script can make the villager and the puzzle but not a creature
(`CreateScriptObject` makes no creatures). Every row is todo unless the notes say otherwise.

**Progress: 0/52 done, 4 partial — 4%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 5's control script starts the quest in the background as soon as the land's opening (Nemesis's curse on the creature) has finished, before any other silver scroll of the land | todo | `LandControl5` never runs: `LOAD_MAP` is empty, so the story never reaches Land 5 ([map-loading](../../../bw1-notes/map-loading.md#pending)); see [../../scripts/land5_script.md](../../scripts/land5_script.md) |
| Nothing else needs to be done first: the puzzle, the man and his fire are there from the moment the land begins | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The quest's title is "Stanley The Wolf" (title text 30); the unused "Swap To Lion" scroll reuses the same title, but only this one ships | todo | `Snapshot`, `UpdateSnapshot` are stubs (`UpdateSnapshotPicture` too) |
| The scroll belongs to no story chain: no other script waits for it or reads its outcome | todo | checked across all the land's scripts; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Set-up

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The tilt puzzle is placed at its site about 670 from the player's home town, at angle 0 and normal size | todo | `CreateWithAngleAndScale` makes a puzzle game only as a bare record (`src/ECS/PuzzleGames.cpp`): only the fish puzzle has parts, so no tilt grid, bells, wolf or sheep (see [../minigames.md](../minigames.md)) |
| A man is made a few metres from the puzzle, facing out from it | todo | `Create` makes villagers and `SetFocus` works for them; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He cannot be hurt by fire, is indestructible and cannot be picked up | partial | the three flag commands work (`SetHurtByFire`, `SetIndestructable`, `SetIdPickupable` in `src/CHLApi.cpp`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He sits down and stays sitting (a sitting loop kept going by a small helper that waits for each sit-down to finish) | todo | `Played`, `SetScriptState`, `SetScriptUlong` work for villagers; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A bonfire is lit beside him | partial | the bonfire is a mobile static, which `CreateScriptObject` makes; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A silver challenge scroll is put over the puzzle | todo | `CreateHighlight` works (`src/ECS/ScriptHighlight.cpp`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## How it appears (the advisor nag)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| While the scroll is waiting, whenever the camera is within 100 of it and it is on screen, the good advisor steps out, points at it and says "Look. Something for you to do here.", at most once every 30 seconds and only when no other scene is playing | todo | the shared notify script's natives work (`SpiritEject`, `SpiritPointGameThing`, `RunText`, `GameThingFieldOfView`); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The nag stops when the scroll is clicked, when the puzzle site itself is clicked, or when the quest says so | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The introduction starts when the scroll is clicked, or as soon as the player starts playing the puzzle (rings a bell) without clicking the scroll first | todo | `GetObjectState` (the puzzle's state) is a stub |
| The scroll is then switched to its opened look | todo | `SetActive` on a highlight works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Quirk: clicking the puzzle site (not the scroll) stops the nag but does not start the introduction; that still waits for a scroll click or a bell | todo | the quest's own wait checks only the scroll and the puzzle's state |

## The introduction

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A scene starts; the man is drawn in high detail, turns and stands up, and the camera closes in on him over 4 seconds (focus over 3) | todo | `StartCameraControl`, `MoveCameraPosition`, `MoveCameraFocus`, `SetHighGraphicsDetail` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The camera's focus then drifts slowly over 6 seconds | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| A "did you know" signpost is put at the corner of the fence next to the sheep: "Guide Stanley the wolf to his food, the sheep, by ringing the bell in the direction you want him to travel. He keeps going in that direction until something stops him. You can reset the puzzle by clicking on the corner bell." (filed under miscellaneous tips) | todo | the shared "did you know" script; the signpost is left in the world and the quest never removes it; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| Half a second later the scroll is recorded with progress 0 and the reminder "This man needs help in feeding Stanley, his blind wolf." (spoken by the good advisor when the reminder is asked for) | todo | `Snapshot` is a stub |
| The man gossips, then (man's voice): "Please help me. My wolf, Stanley, is blind and I need to get him to the sheep so he can eat it." | todo | `RunText` and the villagers' script animations work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The man: "Each time a bell rings, the wolf heads in that direction." | todo | `RunText` works; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He turns towards the bells and gestures as at a meeting: "You can help me get him to his food." while the camera moves round to look past the bells (position over 2 seconds, focus over 1) | todo | `RunText`, `SetFocus` and the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| He sits back down and the camera rises high over the puzzle, looking down on the whole grid (position over 4 seconds, focus over 3) | todo | the camera moves work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The scene ends straight into a dialogue: the evil advisor steps out: "Start ringing those bells, Boss." and goes home | todo | `SpiritEject`, `RunText`, `SpiritHome` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Playing the puzzle

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player rings the bells round the grid; each sends the wolf sliding that way until something stops it; a corner bell resets the puzzle | todo | the tilt puzzle is not ported (`src/ECS/PuzzleGames.cpp` has only the fish puzzle's parts); the puzzle's own rules are in [../minigames.md](../minigames.md) ("Lion maze") |
| The animal in the maze is a wolf (the puzzle's moving animal is built on the wolf) and its target a sheep, matching the dialogue; the puzzle kind is the one other files call the lion maze | todo | the puzzle is not ported; engine evidence: the puzzle's moving piece is a wolf type, its target a sheep type (the engine's puzzle code) |
| There is no time limit, no move count and no way to fail: the quest waits only for the puzzle to report it has been played to the end | todo | `Played` on a puzzle answers only for the fish puzzle (`IsPuzzleGamePlayed`, `src/ECS/PuzzleGames.cpp`) |
| The scroll's progress stays at 0 while the puzzle is played | todo | `Snapshot` stub |

## The man's death (unreachable)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The script watches the man's life while the puzzle is played; if it reaches 0 the good advisor says "Oh no! The wolf fellow is dead!" (once) | todo | the health property (`GetProperty`) is not ported; unreachable in the game anyway: the man is indestructible and the engine refuses to set an indestructible thing's life to 0.01 or below (the engine's life setter) |
| If he were dead when the puzzle is solved, the good advisor says "Hey. You've completed the puzzle." and "It's sad that you murdered the man who asked you to do it in the first place.", and there is no reward at all | todo | unreachable for the same reason; the lines exist in the text table |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A lion creature is made beside the puzzle | todo | `Create` does not make creatures |
| A scene starts; the camera position before it is remembered (but never returned to); the man stands up and the camera closes in on him (position over 4 seconds, focus over 3) | todo | needs the puzzle to report played (not ported) |
| The man and the lion turn to the camera; the man gossips: "Thank you, Mighty One." | todo | needs the puzzle and the lion |
| He turns to the lion and points: "Please take this special Creature in gratitude." | todo | needs the puzzle and the lion |
| The reward sting plays and the camera flies to the lion (position over 4 seconds, focus over 3) | todo | `PlaySoundEffect` works; never reached |
| The man sits down again, the lion looks at the camera, and a second later the scroll's progress becomes 1 (complete) | todo | `UpdateSnapshot` stub |
| The lion waves at the player; the good advisor points at it: "He must want to become our Creature. Let's have him." and stops pointing | todo | `CreatureDoAction` is a stub; `StopPointing` works |
| The scene ends; the scroll stays on the land (the quest never deletes it) | todo | the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## The reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The lion is offered as a creature swap through the shared swap script; it cannot be picked up | todo | `SwapCreature` is a stub; see [../challenges_and_rewards.md](../challenges_and_rewards.md) |
| A silver scroll hangs over the lion, which keeps looking at the camera; when the camera is within 100 and the lion is on screen, the evil advisor points at it: "Swap your Creature with this one if you want.", at most once a minute | todo | no lion (creatures are not made by `Create`) |
| Clicking the scroll or the lion with the player's creature further than 50 away: evil advisor "You'll need to bring our Creature, Boss." and the offer waits again | todo | no lion |
| With the creature within 50: the leash is taken off, the creature is walked to within 15 of the lion (given at most 5 seconds), and the two look each other over | todo | no lion; `CreatureDoAction` stub |
| The good advisor asks every 30 seconds: "Are you sure you want a new Creature? Click the Action Button on the Creature you want to swap to if you are. Click your own Creature to cancel."; clicking the player's own creature, or 110 seconds passing, cancels | todo | no lion |
| Clicking the lion swaps: a two-shot camera on both creatures, a success sparkle on each, both point at each other, the player's mind passes to the lion, the swap sound plays and the scene holds 3 seconds after both finish | todo | `SwapCreature` is a stub |
| After a swap: evil advisor "Great, Boss. If you want your old Creature back, just return here later."; the old creature stays as the one on offer, with a new scroll over it | todo | `SwapCreature` is a stub |
| The offer never ends: the swap script has a time limit worked out but never checks it, so the player can swap back and forth for the rest of the land | todo | quirk in the shared swap script |
| Right after the scene (in parallel with the swap offer), the fireball's level 2 and level 3 power-ups are enabled at the player's home town | partial | `SetMagicInObject` works (`src/Magic/Script/CHLWorship.cpp`) and the fireball works ([../../miracles/fireball.md](../../miracles/fireball.md)); the quest never starts: no Land 5 (`LOAD_MAP` is empty) |
| The evil advisor steps out, points at the home town: "Our Fireball's just got bigger." | todo | `SpiritEject`, `SpiritPointPos`, `RunText` work; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| After the fireball line the evil advisor is never told to stop pointing or go home; the dialogue's end is all that closes it | todo | undetermined whether ending the dialogue sends the advisor home by itself |
| After the progress is set to 1 the script waits for any line on screen to be read, though it has said nothing since the man's last line | todo | harmless; undetermined whether the progress update itself puts any text up |
| A commented-out second "complete" snapshot is left after the reward | n/a | not in the compiled script |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| No music is started by the quest; the only sounds it plays are the reward sting at the lion and the creature-swap sound | partial | `PlaySoundEffect` works; the bell sounds belong to the puzzle object, not ported; the quest never starts: no Land 5 (`LOAD_MAP` is empty) |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The player's creature plays no part in the puzzle; it matters only for the swap, where it must be within 50 of the lion | todo | `SwapCreature` stub |
