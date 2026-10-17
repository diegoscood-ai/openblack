# The Totem Puzzle

The gold scroll on the fourth land that frees the first of Nemesis's Guardian Stones, the one sealed under a spiritual
shield beside the Japanese village, which calls down the fireballs. Five bell towers (totems) stand round the stone; the
village elder asks the player to work out their secret. It is a "follow the bells" memory game: the towers ring a
sequence and the player clicks them back in the same order, four rounds of growing length, with the time allowed to
click shrinking each round.

**Land:** 4 · **Giver:** the Japanese village's elder, at the bell towers by the village · **Script:** Land4Puzzle (in
Land4Meteorites) · **Reward:** the fire Guardian Stone is destroyed and the fireballs stop for good · **Repeatable:**
retried until solved (each failure needs the scroll clicked again); not after

The land as a whole is in [../land_4.md](../land_4.md); the other stones are freed by
[The Heartbroken Man](the_heartbroken_man.md) and [The Defending Ogres](the_defending_ogres.md). The two raised totems
that cure the skeletons belong to [Undead Village](undead_village.md), a different puzzle.

Sources: the land's challenge scripts (the original source text, which matches the shipped `challenge.chl`) and the
game's text table (`Scripts/InfoScript2.txt`) for every line and its voice. openblack's state is judged on this tree:
the story's top script always runs the first land's control script first, the map-loading command (`LoadMap` in
`src/CHLApi.cpp`) is empty and the first land stalls long before its end, so this quest never runs. Rows are partial
only where every command they need works in openblack.

**Progress: 0/65 done, 0 partial — 0%**

## Where it sits in the land

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The land's control script starts the quest in the background when the land begins | todo | `LandControl4` never runs (see [../../scripts/land4_script.md](../../scripts/land4_script.md)); `LOAD_MAP` is empty |
| The man who explains the curse describes this stone first: "So he set three Guardian Stones to control this land." "The first lies under a Spiritual Shield." "Even the wisest of our people don't know how to reach it." | todo | his scene is covered with [The Defending Ogres](the_defending_ogres.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After his story, every 3 minutes until the player's belief in the Japanese village reaches 0.3 (or the village is gone), both advisors step out, the good one pointing at the village: "Leader, let's start impressing the Japanese village." Evil: "If it means we get our hands on the Guardian Stone, I'm for it." | todo | `BeliefForPlayer` is a stub; the advisors are real |
| Its intro is what lets [The Heartbroken Man](the_heartbroken_man.md) start: that quest waits for the elder's introduction to finish | todo | plain script flag; see [The Heartbroken Man](the_heartbroken_man.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Breaking this stone is one of the three the land waits for before the [Undead Village](undead_village.md) and the way out | todo | plain script flags; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The stone and the towers

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the start of the land a small Guardian Stone (a meteor at a third of its size, 2 off the ground) sits by the Japanese village, indestructible, not moveable, not pick-up, smoking (a bonfire effect 25 times normal size) | todo | `CREATE` makes the meteor, its three flags and `SpecialEffectPosition` are real; the height (YPos) is handled; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| It is sealed by two permanent miracles cast on it: a physical shield of radius 15 and a spiritual shield of radius 7 | todo | `SpellAtPos` casts both shields ([../../miracles/physical_shield.md](../../miracles/physical_shield.md), [../../miracles/spiritual_shield.md](../../miracles/spiritual_shield.md)); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Five bell towers are built round it in an arc, each a totem at full height, turned 340, 10, 50, 80 and 100 degrees | todo | `CREATE` of a totem is not done (`CreateScriptObject` leaves it to its default case) |
| None of the towers can be picked up | todo | the towers are never made |
| The shields are what keeps the player from simply hitting the stone; nothing else in the script protects it | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scroll only comes once the player owns the Japanese village, it has been destroyed, or all its people are dead; this is checked every 10 seconds | todo | the town owner and population reads are stubs or unhandled |
| A gold scroll appears just north of the towers and its challenge is made the current one | todo | `CreateHighlight` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| While it is not clicked, whenever the camera is within 100 and looking at it, at most every 30 seconds the good advisor steps out, points at it and says "Your godly attention is required here, Leader." | todo | the notify script calls real natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Clicking it removes the scroll and starts an attempt | todo | `GameThingClicked` and `ObjectDelete` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The elder's introduction (first attempt only)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A cut scene begins with the generic script music (number 1); the camera glides over 4 seconds to look at the towers | todo | `StartMusic` and the camera are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The elder (a Japanese farmer, drawn in high detail) is made among the towers and walks at 0.4 speed past one spot to a stop before the camera | todo | `CREATE` makes villagers and `MoveGameThing` walks them; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| He turns to the camera, prays, stands out of his prayer and gossips on a loop | todo | villager animations and `SetFocus` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The elder: "Hail, spiritual leader!" | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "I am the Village elder. I have tried to learn the secrets of the bell towers." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| He walks on at 0.2 speed with a beckoning walk; 2 seconds later the camera rises over 4 seconds to look down over all five towers | todo | real villager and camera natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "But they are too much for my old brain. Perhaps you could solve the puzzle." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| "Good luck. I'll be praying for you." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The challenge log records the quest: title "The Totem Puzzle", progress 0, alignment 0, with the reminder: the evil advisor saying "Let's solve the puzzle and destroy the Guardian Stone." | todo | `Snapshot` is a stub |
| The elder is removed, the music stops and the cut scene ends | todo | `ObjectDelete` and `StopMusic` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The flag that starts The Heartbroken Man is raised | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Later attempts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| From the second attempt on, a short cut scene instead: the camera rises over the towers and the evil advisor says "Let's solve the puzzle and destroy the Guardian Stone." | todo | real camera and advisor natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## The bells

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| During an attempt the land's fireball waves are held where they are (no switching on or off, no advisor talk about them) | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The towers show a sequence: each tower in turn flashes (an appearing sparkle for 2 seconds) and rings its own bell note, one a second | todo | `SpecialEffectPosition` and `PlaySoundEffect` are real, but the towers are never made |
| Clicks made while the sequence plays are thrown away: each wait for a click starts by clearing the last clicked object | todo | the towers are never made; `ClearClickedObject` is real |
| Clicking a tower flashes it and rings its bell; the first tower clicked is taken as the answer for that step, right or wrong | todo | the towers are never made; `GameThingClicked` is real |
| Round 1: towers 1, 2, 3 (three notes). The player has 10 seconds for the first click and 5 for each after | todo | the towers are never made |
| Round 2: 1, 2, 3, 5, 2 (five notes). 10 seconds for the first click, 5 for each after | todo | the towers are never made |
| Round 3: 1, 2, 3, 5, 2, 3, 1 (seven notes). 5 seconds for the first click, 3 for each after | todo | the towers are never made |
| Round 4: 1, 2, 3, 5, 2, 3, 1, 4, 1 (nine notes). 3 seconds for the first click, 2 for each after | todo | the towers are never made |
| Every round starts with the same notes as the one before: each is the last one with notes added | todo | the towers are never made |
| A round answered fully plays a stone chime at the camera and, after 3 seconds, the next round's sequence | todo | the towers are never made |
| A wrong tower, or running out of time on any click, ends the attempt there | todo | the towers are never made |

## Failing

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A failed attempt plays the "bad stone" sound at the scroll's spot | todo | `PlaySoundEffect` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The first failure: a cut scene, the camera moves over 4 seconds; the elder is made again near the scroll, faces the camera and gossips: "That's the same problem I had!" "You'll get the hang of it though." | todo | real villager, camera and text natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The camera moves again over 4 seconds: "To try again, activate the Scroll once more." Then the elder is removed | todo | real camera and text natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Every later failure: the evil advisor steps out alone: "Huh. Looks like you messed it up, Boss." | todo | `SpiritEject` and `RunText` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The fireball waves may change again; a new scroll appears at the same spot and the whole puzzle restarts from round 1 when clicked | todo | `CreateHighlight` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| All four rounds answered: a beam explosion (2 seconds) bursts at each tower; a second later all five towers are destroyed with explosions | todo | the towers are never made |
| The challenge log entry is updated: progress 1, alignment 0, and its reminder becomes the good advisor's "Raise all the Totems to get the Guardian Stone." | todo | `UpdateSnapshot` is a stub |
| Both shields round the stone are removed | todo | `ObjectDelete` of a spell is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The fireballs are forced on for the cut scene, so the player sees them stop | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| After 2 seconds a cut scene: the camera moves to the stone over 6 seconds (4 to look at it) and holds 2 seconds | todo | real camera natives; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The good advisor points at the stone: "You solved the puzzle and got the Guardian Stone!" and goes home | todo | advisors and texts are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The Guardian Stones music starts; the camera shakes within 300 (amplitude 0.1, 5 seconds) and follows the stone as it rises a tenth at a time to 20 off the ground | todo | `StartMusic`, `ShakeCamera`, `FocusFollow` and the height (YPos) are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A bang and a flash (6 seconds) go off, the stone is destroyed with an explosion and the stone-explosion sound plays | todo | `SpecialEffectPosition` and `PlaySoundEffect` are real; which `ObjectDelete` mode the stone uses was not checked |
| A line in the good advisor's voice (the advisor has already been sent home): "That part of Nemesis just got vapourised." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The screen fades, the camera cuts to look over the home village's skies and fades back in; after 2 seconds the fire stone counts as broken | todo | fade and camera cut are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| 9 seconds later the evil advisor: "Where are the fireballs? They've stopped!" then "Hmm! I think I preferred it before." | todo | `RunText` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The music stops, the force on the fireballs is dropped and the stone's smoke is removed | todo | `StopMusic` and `ObjectDelete` are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## What it stops: the fireballs

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Until this stone breaks, fireballs (level 1 fireball miracles) are thrown from a spot in the sky at five targets round the land, each scattered by up to 50 (one up to 100) | todo | `SpellAtPos` casts fireballs; covered with the land in [../land_4.md](../land_4.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The fireballs come in waves: off for the first 4 minutes, then 2 minutes on and 2 off; the advisors say "Oh. The fireballs have stopped!" / "For now, at least." and "Oh dear. Here come the fireballs again." | todo | timers and advisors are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| Once the home village's worship site is built and its centre is built, each new wave comes with a warning, "Some of those fiery orbs are getting close to our Village!", and 30 seconds later the fireballs turn aggressive: aimed at the village at double strength | todo | the built percentage read and the advisors are real; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| When the stone breaks, every fireball thrower stops for good | todo | plain script flag; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| The fire effect's wait between fireballs is meant to be random, but its upper bound is never set (the source sets the lower one twice), so the wait is drawn between the lower bound and 0 | todo | the same script; `Random` is real; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## What it unlocks next

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| With the darkness and lightning stones also broken, the land moves on to the [Undead Village](undead_village.md) | todo | plain script flags; see [Undead Village](undead_village.md); never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Soft-locks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The puzzle can be retried for ever, so it cannot lock the story by failing | todo | the towers are never made, so it can never be solved |
| It never starts while the Japanese village stands with people in it and is not the player's; the land cannot finish until the player deals with that village | todo | the town owner and population reads are stubs |

## Creature

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The creature plays no part; only the player's clicks on the towers count | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |

## Quirks, unused and cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The title is "The Totem Puzzle" because the bell towers are totem objects, but the game is a memory game of bells; the reminder given on success ("Raise all the Totems…") belongs to a totem-raising puzzle | todo | the same script; never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A first version of this quest used the game's built-in totem puzzle (the third totem layout) at the stone and waited for it to be solved; it is left commented out | n/a | never in the shipped game |
| A separate, bronze "Did you know?" totem puzzle stands north-west of the Japanese village (the same built-in third totem layout, with a 30-radius ring of influence round it): solving it gives a shield miracle dispenser; it is not part of this gold scroll | todo | `JapaneseTotemPuzzle` makes a puzzle game (`CREATE` of a puzzle game, `ecs::CreatePuzzleGame`, `src/ECS/PuzzleGames`); never reached: Land 4 needs `LOAD_MAP`, an empty native |
| A line from the elder, "The rewards are surely worth the effort. But you must first gain influence over them.", is in the text table but cut from the scene | n/a | cut from the scene |
| A test launcher, "Simon Says" (not compiled into the game), runs this quest on its own | n/a | never in the shipped `challenge.chl` |
