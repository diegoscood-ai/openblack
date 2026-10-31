# The Beach Temple Puzzle

The second land's beach silver scroll: a man in a small temple of four stacked rings on the beach asks the player to
rebuild it, ring by ring, on the column furthest up the beach so the floods can't reach it. Once the temple stands on
the far column, the man makes it heal every living thing that comes near it, for the rest of the land. The ring rules
themselves are in [../minigames.md](../minigames.md#beach-temple-rings-land-2); this file covers the quest around them.

**Land:** 2 · **Giver:** a Greek farmer living in the small temple on the beach · **Script:** HanoiFlood · **Reward:** the temple heals every living thing within 10 of it, for good · **Repeatable:** no

Sources: the quest's script source (`HanoiFlood.txt`) and the shared helpers it runs (the notify loop with an end
variable, the signpost helper and the standard reminder), each checked line by line against the PC game's compiled
`challenge.chl` (argument order, text numbers, the challenge number on the scroll and snapshots); the land's control
script for the start; the game's English text table for every line; the puzzle object's class list in the PC executable
for whether any flood exists. openblack is judged on this tree: of the 44 script functions the quest's scripts use, only
the snapshot keeps the challenge's record now (`src/CHLApi.cpp`), and making a puzzle object works
(`CreateScriptObject`). But no check runs the land's control script that far (see [../land_2.md](../land_2.md) row 1), so nothing
below has been seen; every row is pending unless the notes say otherwise.

**Progress: 0/46 done, 0 chosen, 2 partial, 0 inferred — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started at once when the land begins, right after the land's set-up and alongside the singing stones and the tree puzzle, before the land's entry scene; no town has to be owned and there is no wait | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path ([../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| The temple puzzle (four rings on three columns) is placed on the beach, turned 180 degrees, at normal size | pending | CREATE of a puzzle game makes only a bare `PuzzleGame` record (`src/ECS/PuzzleGames.cpp`): no rings or columns, only the fish puzzle has parts; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A ring of the player's influence, radius 50, is put round the puzzle so the hand can reach the rings although the beach is outside any town; it is never removed, even after the puzzle is solved | pending | `INFLUENCE_POSITION` is real (`src/Magic/Script/CHLInfluence.cpp`) but not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A silver scroll stands about 16 north of the puzzle, raised 2 above the ground; it belongs to this challenge in the challenge list | pending | `CREATE_HIGHLIGHT` and `SET_PROPERTY` altitude are real (`src/ECS/ScriptHighlight.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The notify loop ends when the scroll or the spot under it is clicked, or when the puzzle is solved first (the quest's end flag is set); either way the scroll is then made active | pending | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The first line can come straight away: the loop's 30-second gap is primed so the advisor may speak as soon as the camera is near | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## The introduction (scroll clicked)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll (before the puzzle is solved) starts a widescreen scene with the generic script music, and remembers that the introduction was played | pending | camera control, dialogue, game speed, widescreen and music natives are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The camera glides over 4 seconds to a raised view of the temple; a Greek farmer appears inside the temple (at the puzzle's centre), drawn in full detail, and walks out to a spot just west of it | pending | villager CREATE, the camera moves, `SET_HIGH_GRAPHICS_DETAIL` and `MOVE_GAME_THING` on a villager are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| After 2 seconds the camera drops over 3 seconds to ground level beside him; once he has arrived and the camera has stopped, he turns to face the camera | pending | `HAS_CAMERA_ARRIVED` and `SET_FOCUS` on a villager are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The challenge is recorded in the player's challenge log (the temple's list of challenges) with the title "The Beach Temple Puzzle", a picture of this view, progress 0 and an alignment of -0.5, and a reminder: the good advisor stepping out to say "There is risk of flooding." | pending | `SNAPSHOT` is real (the record is kept); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "Thank goodness it's you. You're the only one who can help me." The camera creeps closer over 8 seconds while he plays a talking animation on loop | pending | `RUN_TEXT` and `TEXT_READ` are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "My temple here keeps getting flooded during heavy rain." | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "The building consists of separate rings which you place over these columns" | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "Please, I beg you, move the sections up the beach to rebuild it on the furthest column, where it'll be safe." He turns to the far column up the beach and points once; the camera shifts and tilts slightly up | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "But the temple has to be moved correctly. The architect left an explanation on this signpost." This line waits for the player to click on | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| A bronze "did you know" signpost appears beside the temple with the rules: "The Beach Temple Puzzle. You must move the Temple to the column furthest from the sea piece by piece. There are four Temple pieces and you may only place a piece on an empty spike or on a wider piece." (filed under miscellaneous tips) | pending | the signpost highlight and `HIGHLIGHT_PROPERTIES` are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The camera turns to the signpost and the man points at it; once the line is read he faces the camera, talks twice more, and walks back into the temple while the camera returns over 3 seconds to where the player left it | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man fades away, the music stops and the scene ends | pending | `OBJECT_DELETE` on a villager and `STOP_MUSIC` are real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Move the four rings, by hand, from their column by the sea onto the column furthest up the beach, never resting a ring on a smaller one | pending | the ring rules are not ported: our puzzle record has no rings (`src/ECS/PuzzleGames.cpp`) |
| There is no timer, move limit or penalty; the quest simply waits until the puzzle reports it is solved | pending | `PLAYED` on a puzzle answers only for the fish puzzle (`IsPuzzleGamePlayed`); the Hanoi test is not ported |
| Clicking the scroll is not needed: the puzzle can be solved before or without the introduction | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## The flood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing actually floods: the scripts start no water, rain, tide or timer, and the puzzle object in the PC game has no flooding of its own; the flood exists only in the man's lines, the reminder ("There is risk of flooding.") and the script's name | pending | nothing to do in our tree either, but the puzzle and the quest are not there; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The start snapshot records the quest with an alignment of -0.5, as if the challenge leaned evil; the success snapshot then records +1 | pending | `SNAPSHOT` is real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| As soon as the puzzle is solved, the quest's end flag is set (ending the advisor's reminders if the scroll was never clicked) and a widescreen scene starts | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The camera glides over 4 seconds to look over the far column; the Greek farmer appears at the rebuilt temple, in full detail, and walks a little west, then faces the camera | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| If the introduction was played, he says "Thank you, Holy One. No more flooding for me!"; if the player solved it without clicking the scroll, he says instead "You must have noticed my temple was at risk from flooding! Thank you for your actions!" | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man, talking in a loop: "In return for this I will activate the temple's beneficial properties." | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The man: "Any living being who comes to the temple will be healed. " (with the text table's trailing space); the dialogue box is then closed | pending | `GAME_CLOSE_DIALOGUE` is real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The camera swings over 4 seconds to look along the temple; he faces the temple and plays the summoning animation (the one the Pied Piper uses) twice | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| After 2 seconds the challenge log entry is recorded again, with the same title and reminder, progress 1 (complete) and alignment +1 | pending | `SNAPSHOT` is real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The camera pulls back over 3 seconds, the healing starts, and the man walks into the temple; 2 seconds after the camera stops, it returns over 4 seconds to where the player left it, the man fades away and the scene ends | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: there is no time limit, no losing state is tested, and the puzzle cannot be broken (its pieces and columns are put back; see [../minigames.md](../minigames.md#beach-temple-rings-land-2)) | pending | no rings in our puzzle record; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| Ignoring the scroll leaves it standing with the advisor's reminders until the puzzle is solved or the land ends | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| After the introduction, the scroll's script waits until the puzzle is solved; the source's comment says this is to keep the scroll in the world (scripts' own objects are otherwise removed when the script ends, which is why the signpost helper hands its signpost back to the game) | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A level-2 heal miracle is cast on the rebuilt temple's spot, radius 10, by the scripts (no player's own), and cast again every 20 seconds forever, so any villager, animal or creature within 10 is healed; this lasts for the rest of the land | partial 50% | `SPELL_AT_POS` works (`src/Magic/Script/CHLSpells.cpp`, the neutral player casts it); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [challenge_natives_magic_and_weather.md](../../scripts/challenge_natives_magic_and_weather.md) "Casts a miracle at a position", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |
| Each new heal is cast without removing the last one; the script keeps only the newest | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| No miracle, dispenser, prayer power or belief is given; the reward is only the healing temple | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; rewards: [../rewards.md](../rewards.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rules signpost beside the temple is found (within 2 of its spot) and faded away after the success scene | pending | `OBJECT_DELETE` is real; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| If the introduction was never played, no signpost was ever made, so this finds nothing | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The rebuilt temple stays on the far column, the influence ring stays round the puzzle, and the healing goes on | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the generic script music from its start to its end | partial 50% | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio/GameMusic.cpp`); not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path; finished by [challenge_natives_sound_and_music.md](../../scripts/challenge_natives_sound_and_music.md) "Starts a piece of music", "Stops the script's music", [land2_script.md](../../scripts/land2_script.md) "The story moves on to this land when Land 1 is finished"; still open: no quest run's PASS is cited for it |
| The success scene starts no music or sound of its own (no reward sting) | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scripts never look at the player's creature; it can move rings like any other object it picks up only if the puzzle allows it (undetermined: the scripts don't say) | pending | no rings to pick up in our tree; not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The creature, like any living thing, is healed when within 10 of the rebuilt temple | pending | the heal miracle exists (`src/Magic`), but not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's end flag is a game-wide variable shared with other scripts (the land 5 lion puzzle uses it; the land 1 singing stones reset it); the shared helper's own comment warns to use it with caution. On land 2 only this quest uses it | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |
| The good advisor's scroll line reads "There's someone is this little temple" (sic) | pending | not reached by any check: the quest starts as Land 2's story begins, before Khazar's first wait, and play reaches Land 2 only by Land 1's skip path |

