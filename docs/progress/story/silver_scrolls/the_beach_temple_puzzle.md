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
the snapshot still only logs "not implemented" in `src/CHLApi.cpp`, and making a puzzle object works
(`CreateScriptObject`). But the land's control script never runs (see [../land_2.md](../land_2.md) row 1), so nothing
below happens; every row is todo unless the notes say otherwise.

**Progress: 0/49 done, 2 partial — 2%**

## How it appears

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest is started at once when the land begins, right after the land's set-up and alongside the singing stones and the tree puzzle, before the land's entry scene; no town has to be owned and there is no wait | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs ([../../scripts/land2_script.md](../../scripts/land2_script.md)) |
| The temple puzzle (four rings on three columns) is placed on the beach, turned 180 degrees, at normal size | todo | CREATE of a puzzle game makes only a bare `PuzzleGame` record (`src/ECS/PuzzleGames.cpp`): no rings or columns, only the fish puzzle has parts; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| A ring of the player's influence, radius 50, is put round the puzzle so the hand can reach the rings although the beach is outside any town; it is never removed, even after the puzzle is solved | todo | `INFLUENCE_POSITION` is real (`src/Magic/Script/CHLInfluence.cpp`) but the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| A silver scroll stands about 16 north of the puzzle, raised 2 above the ground; it belongs to this challenge in the challenge list | todo | `CREATE_HIGHLIGHT` and `SET_PROPERTY` altitude are real (`src/ECS/ScriptHighlight.cpp`); the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| While the scroll is unclicked, whenever the camera is within 100 of it and the scroll is on screen, the good advisor steps out, points at it and says "There's someone is this little temple. He definitely wants something." (the text table's own typo), at most once every 30 seconds and only when no other scene is using the widescreen | todo | the advisor natives, `RUN_TEXT` and `GAME_THING_FIELD_OF_VIEW` are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The notify loop ends when the scroll or the spot under it is clicked, or when the puzzle is solved first (the quest's end flag is set); either way the scroll is then made active | todo | `GAME_THING_CLICKED` and `SET_ACTIVE` on a highlight are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The first line can come straight away: the loop's 30-second gap is primed so the advisor may speak as soon as the camera is near | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## The introduction (scroll clicked)

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Clicking the scroll (before the puzzle is solved) starts a widescreen scene with the generic script music, and remembers that the introduction was played | todo | camera control, dialogue, game speed, widescreen and music natives are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The camera glides over 4 seconds to a raised view of the temple; a Greek farmer appears inside the temple (at the puzzle's centre), drawn in full detail, and walks out to a spot just west of it | todo | villager CREATE, the camera moves, `SET_HIGH_GRAPHICS_DETAIL` and `MOVE_GAME_THING` on a villager are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| After 2 seconds the camera drops over 3 seconds to ground level beside him; once he has arrived and the camera has stopped, he turns to face the camera | todo | `HAS_CAMERA_ARRIVED` and `SET_FOCUS` on a villager are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The challenge is recorded in the player's challenge log (the temple's list of challenges) with the title "The Beach Temple Puzzle", a picture of this view, progress 0 and an alignment of -0.5, and a reminder: the good advisor stepping out to say "There is risk of flooding." | todo | `SNAPSHOT` is a stub (no challenge log entries); the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "Thank goodness it's you. You're the only one who can help me." The camera creeps closer over 8 seconds while he plays a talking animation on loop | todo | `RUN_TEXT` and `TEXT_READ` are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "My temple here keeps getting flooded during heavy rain." | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "The building consists of separate rings which you place over these columns" | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "Please, I beg you, move the sections up the beach to rebuild it on the furthest column, where it'll be safe." He turns to the far column up the beach and points once; the camera shifts and tilts slightly up | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "But the temple has to be moved correctly. The architect left an explanation on this signpost." This line waits for the player to click on | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| A bronze "did you know" signpost appears beside the temple with the rules: "The Beach Temple Puzzle. You must move the Temple to the column furthest from the sea piece by piece. There are four Temple pieces and you may only place a piece on an empty spike or on a wider piece." (filed under miscellaneous tips) | todo | the signpost highlight and `HIGHLIGHT_PROPERTIES` are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The camera turns to the signpost and the man points at it; once the line is read he faces the camera, talks twice more, and walks back into the temple while the camera returns over 3 seconds to where the player left it | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man fades away, the music stops and the scene ends | todo | `OBJECT_DELETE` on a villager and `STOP_MUSIC` are real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## What the player must do

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Move the four rings, by hand, from their column by the sea onto the column furthest up the beach, never resting a ring on a smaller one | todo | the ring rules are not ported: our puzzle record has no rings (`src/ECS/PuzzleGames.cpp`) |
| There is no timer, move limit or penalty; the quest simply waits until the puzzle reports it is solved | todo | `PLAYED` on a puzzle answers only for the fish puzzle (`IsPuzzleGamePlayed`); the Hanoi test is not ported |
| Clicking the scroll is not needed: the puzzle can be solved before or without the introduction | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## The flood

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Nothing actually floods: the scripts start no water, rain, tide or timer, and the puzzle object in the PC game has no flooding of its own; the flood exists only in the man's lines, the reminder ("There is risk of flooding.") and the script's name | todo | nothing to do in our tree either, but the puzzle and the quest are not there; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The start snapshot records the quest with an alignment of -0.5, as if the challenge leaned evil; the success snapshot then records +1 | todo | `SNAPSHOT` is a stub; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Success

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| As soon as the puzzle is solved, the quest's end flag is set (ending the advisor's reminders if the scroll was never clicked) and a widescreen scene starts | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The camera glides over 4 seconds to look over the far column; the Greek farmer appears at the rebuilt temple, in full detail, and walks a little west, then faces the camera | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| If the introduction was played, he says "Thank you, Holy One. No more flooding for me!"; if the player solved it without clicking the scroll, he says instead "You must have noticed my temple was at risk from flooding! Thank you for your actions!" | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man, talking in a loop: "In return for this I will activate the temple's beneficial properties." | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The man: "Any living being who comes to the temple will be healed. " (with the text table's trailing space); the dialogue box is then closed | todo | `GAME_CLOSE_DIALOGUE` is real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The camera swings over 4 seconds to look along the temple; he faces the temple and plays the summoning animation (the one the Pied Piper uses) twice | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| After 2 seconds the challenge log entry is recorded again, with the same title and reminder, progress 1 (complete) and alignment +1 | todo | `SNAPSHOT` is a stub; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The camera pulls back over 3 seconds, the healing starts, and the man walks into the temple; 2 seconds after the camera stops, it returns over 4 seconds to where the player left it, the man fades away and the scene ends | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Failure and abandoning

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest cannot be failed: there is no time limit, no losing state is tested, and the puzzle cannot be broken (its pieces and columns are put back; see [../minigames.md](../minigames.md#beach-temple-rings-land-2)) | todo | no rings in our puzzle record; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| Ignoring the scroll leaves it standing with the advisor's reminders until the puzzle is solved or the land ends | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| After the introduction, the scroll's script waits until the puzzle is solved; the source's comment says this is to keep the scroll in the world (scripts' own objects are otherwise removed when the script ends, which is why the signpost helper hands its signpost back to the game) | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Reward

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| A level-2 heal miracle is cast on the rebuilt temple's spot, radius 10, by the scripts (no player's own), and cast again every 20 seconds forever, so any villager, animal or creature within 10 is healed; this lasts for the rest of the land | partial | `SPELL_AT_POS` works (`src/Magic/Script/CHLSpells.cpp`, the neutral player casts it); the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| Each new heal is cast without removing the last one; the script keeps only the newest | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| No miracle, dispenser, prayer power or belief is given; the reward is only the healing temple | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs; rewards: [../rewards.md](../rewards.md) |

## Aftermath

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The rules signpost beside the temple is found (within 2 of its spot) and faded away after the success scene | todo | `OBJECT_DELETE` is real; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| If the introduction was never played, no signpost was ever made, so this finds nothing | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The rebuilt temple stays on the far column, the influence ring stays round the puzzle, and the healing goes on | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Music and sounds

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The introduction plays the generic script music from its start to its end | partial | `START_MUSIC` and `STOP_MUSIC` work (`src/Audio/GameMusic.cpp`); the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The success scene starts no music or sound of its own (no reward sting) | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Creature involvement

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The scripts never look at the player's creature; it can move rings like any other object it picks up only if the puzzle allows it (undetermined: the scripts don't say) | todo | no rings to pick up in our tree; the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The creature, like any living thing, is healed when within 10 of the rebuilt temple | todo | the heal miracle exists (`src/Magic`), but the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Script quirks

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| The quest's end flag is a game-wide variable shared with other scripts (the land 5 lion puzzle uses it; the land 1 singing stones reset it); the shared helper's own comment warns to use it with caution. On land 2 only this quest uses it | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The comments beside the dialogue are older drafts that differ from the spoken text: "Aha. Just the god I was looking for.", "My beach house keeps getting flooded by the tides.", "If you could move it further up the beach I'd be most grateful.", "The signposts provide the rules and instructions you need.", "Cheers, Mighty One. I can live without fear of flooding now.", "In return for your kindess I will activate the powers of the Tower." and "Any living being who visits it will be healed." | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| Unused values: a timer primed 31 seconds back in the scroll script, and two marked spots in the main script (a start spot and a spot for the man) are set up but never used | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |
| The good advisor's scroll line reads "There's someone is this little temple" (sic) | todo | the quest never starts: LOAD_MAP is not ported, so Land 2's control script never runs |

## Unused or cut parts

| Behaviour | Status | Where / notes |
|-----------|--------|---------------|
| Eight of the man's lines, about belongings ruined by a flood, are in the text table but no script says them: "My collection of clocks!", "My soccer trophies!", "The Welsh dresser's toppled!", "Oh, my matchstick galleon!", "My ship in a bottle collection!", "Oh, my hip replacement!", "My CDs!", "My house of cards! That took four years!" | n/a | apparently a cut flooding stage, where the temple flooded and the man lamented as he lost things; flagged differently in the text table from the lines used. Whether the engine itself ever says them is not confirmed: no script source uses them, and a search of the PC executable for their text numbers finds no plausible use |
| Every other line of the quest's set (1 to 8, 17 to 19) is used | n/a | |
