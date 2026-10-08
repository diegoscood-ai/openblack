# Hidden content

Things that are really in the shipped game but that most players never find: the hidden phone box, the secret
creatures, scenery placed by the program rather than by the land, and the hidden tunes of the singing stones. The
challenges as such are owned by [../story/](../story/); the puzzles' rules by
[../story/minigames.md](../story/minigames.md). Date-based surprises (special days, footprints on special dates, the
night voices, real weather) are in [../pc_integration/](../pc_integration/), and the named villagers taken from the
Lionhead staff list or the address book are in [../villager/special_villagers.md](../villager/special_villagers.md) and
[../pc_integration/villager_names_from_contacts.md](../pc_integration/villager_names_from_contacts.md).

**Progress: 3/29 done, 1 partial — 12%**

## The hidden phone box (Land 1)

A "hidden holy script" compiled into the shipped challenges. In the script source it sits after more than a hundred
blank lines under the comment "The section below IS needed otherwise the game will not work."

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| How to find it: on Land 1, in a single-player game, take hold of your creature's leash and, while holding it, draw the square-wave gesture (a line of square steps up and down) | todo | no leash gesture picker in our tree, so the square wave is never looked for; see [../gesture/leash_gestures.md](../gesture/leash_gestures.md) |
| It works once: if the phone box is already there, the line "Already done" is shown instead | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| A phone box appears on the shore at about (3130, 1639); it cannot be picked up, moved or destroyed, and it rings | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Both advisors appear and say: "Hey Boss! Pick up the phone!", "You've found a hidden holy script!", "Are you expecting a call?", "Are you going to answer that?" | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Twenty seconds later, if it is still not clicked, a recorded call plays once: the phone is picked up, two recorded messages, it is put down, and fifteen seconds after that one more message | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Clicking the phone box: a small storm gathers over it (rain, full cloud, for 60 s), the camera flies to a set view, and the advisors say "You've found a hidden holy script!", "One of the Lionhead people." and "Let the prophet speak. But listen well - he talks quietly." | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| The box then opens like a chest, plays one of ten recorded messages picked at random, and closes; the camera goes back | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| It must be clicked at least three times in all: each further click has the good advisor say "It's for you." or "Are you expecting a call?" and plays the next message in turn, wrapping round after the tenth | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| On the click after that, with another storm, a recorded message plays (the script's comment: "Three and a half years...") and the box sinks into the ground with dust, a rumble and a shaking camera | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Five phone boxes then rise side by side at about (3125 to 3135, 1644), one for each land, the first ringing | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Clicking the first land's box plays another of the ten messages | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Clicking the box of land 2, 3, 4 or 5 plays one of three recorded messages, lowers the other boxes, opens the chosen one and fades to black; the shipped game then only says "Sorry, the boss got the cheat removed" | todo | the hidden script never starts in our tree (no leash gesture picker), and it calls `SET_OPEN_CLOSE` and `SET_AFFECTED_BY_WIND`, which log "not implemented"; see [../scripts/challenge_scripts.md](../scripts/challenge_scripts.md) |
| Four of the twenty-odd phone box recordings are never played (numbers 3, 12, 13 and 14 of the set) | n/a | they ship in the script sound bank; see [unused_content.md](unused_content.md) |

See also [../story/land_1.md](../story/land_1.md) and [../audio/voices_and_speech.md](../audio/voices_and_speech.md),
which list the phone box as one row each.

## Secret creatures

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature breeder (on lands 1, 2, 4 and 5) can offer five extra creatures, the leopard, horse, mandrill, gorilla and rhino, each only if it is unlocked | partial | `IS_CREATURE_AVAILABLE` is a stub that always answers no (`src/CHLApi.cpp`), which matches a game without the unlock, but the unlock itself is missing; the breeder is in [../story/land_1.md](../story/land_1.md) |
| The unlock is a code kept in two values of the game's setup registry key (`Software\Lionhead Studios Ltd\Black & White\BWSetup`), named "TimerValue" and "TimerCheckSum" as if they were timer settings | todo | our tree reads no unlock code |
| At start-up the code is checked against a number worked out from Windows' own product ID, so a code only works on the computer it was made for; if it does not match, no creature is unlocked | todo | our tree reads no unlock code |
| The code holds one switch per creature, so any mix of the five can be unlocked | todo | our tree reads no unlock code |

## Scenery placed by the program

These three are not objects of the land: the program places them by land number, as loose 3D models with no shadow, so
they cannot be picked up or harmed. Our wiki describes them in detail
([water.md](../../bw1-notes/water.md#fixed-scenery-per-land-land-3-waterfall-land-4-ark-and-dinosaur)).

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Land 4 has a dinosaur skeleton lying on high ground at (2690, 2590), pressed into the landscape | done | `ecs::designed_scenery` (`src/ECS/DesignedScenery.cpp`), updated each frame from `Game.cpp`: the dinosaur on Land 4, pressed into the landscape |
| Land 4 has an ark at (3538, 2129), with the sound of running water | done | `ecs::designed_scenery` (`src/ECS/DesignedScenery.cpp`), updated each frame from `Game.cpp`: the ark on Land 4, with the running-water sound on a sound tag |
| Land 3 has a waterfall at (3059, 3145) whose water scrolls down, with a ring of spray every 0.7 s and the sound of running water | done | `ecs::designed_scenery` (`src/ECS/DesignedScenery.cpp`), updated each frame from `Game.cpp`: the waterfall on Land 3, its scrolling water (`frame_anim::WaterfallScroll`), a ring every 0.7 s and the running-water sound |

## The Land 2 singing stones' hidden tunes

The stones' challenge says only that playing their melodies wakes the ancients. The rules are in
[../story/minigames.md](../story/minigames.md); the tunes themselves are these.

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The stones remember the last 14 taps and listen for three well-known tunes: "Twinkle Twinkle Little Star", Chopin's Funeral March and "White Christmas" | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| "Twinkle Twinkle Little Star" is the stones 1, 1, 8, 8, 9, 9, 8, 6, 6, 5, 5, 3, 3, 1 (counting the nine stones round the circle in the order they are placed) | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| The Funeral March and "White Christmas" are recognised by the steps between notes, not by fixed stones, so they can start on any stone | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Twinkle Twinkle: night falls (the clock moves to 23:00), ten bats circle, a mist rises over the stones, and its own music plays | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Funeral March: to funeral music the stones' keeper walks into the ring and says "Behold, the dead will rise again when placed within the ring."; for five minutes any dead villager or animal laid within 10 m of the centre comes back (villagers as skeletons of their old age, joining the nearby town) | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| White Christmas: snow falls over the circle for five minutes, to its own Christmas music | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Every night (game time after 22:00 or before 5:20) the singing stones fade into tombstones, and turn back into stones by day (not while the dead are rising) | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Killing the stones' keeper makes the evil advisor say "Ah... Now that's what I call music." | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Two pipers wander the land as hints, whistling Twinkle Twinkle and the Funeral March, stopping now and then to sit down for a few minutes | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
| Each tune counts a third towards the challenge, once | todo | the Land 2 quest never starts: the story never reaches Land 2 (`LOAD_MAP` is empty), and the taps would need `GET_OBJECT_CLICKED`, a stub; see [../story/silver_scrolls/the_singing_stones_land_2.md](../story/silver_scrolls/the_singing_stones_land_2.md) |
